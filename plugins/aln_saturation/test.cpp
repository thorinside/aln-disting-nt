#include <distingnt/api.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "aln_saturation_core.h"

extern "C" const _NT_globals NT_globals = {
    48000u,
    128u,
    NULL,
    0u,
    0u,
    0u,
};

namespace {

int fail(const char* message) {
    std::cerr << "FAIL: " << message << "\n";
    return 1;
}

bool near(float actual, float expected, float tolerance = 1.0e-3f) {
    return fabsf(actual - expected) <= tolerance;
}

}  // namespace

int main() {
    static_assert(aln_saturation::kSaturationModelCount == 2u, "model count");
    static_assert(aln_saturation::kStageCount == 3u, "stage count");
    for (uint8_t model = 0; model < aln_saturation::kSaturationModelCount; ++model) {
        const aln_saturation::SaturationModelData& data =
            aln_saturation::kSaturationModels[model];
        if (data.nodeCount != 255u || data.leafCount != 256u)
            return fail("generated tree size");
        float minimum = 100.0f;
        float maximum = -100.0f;
        for (int index = 0; index <= 4096; ++index) {
            const float input = -5.0f + 10.0f * index / 4096.0f;
            const float output = aln_saturation::evaluateNormalizedModel(input, model);
            minimum = std::min(minimum, output);
            maximum = std::max(maximum, output);
        }
        if (minimum > -4.99f || maximum < 4.99f)
            return fail("normalized model range");
    }

    double modelDifferenceSquared = 0.0;
    for (int index = 0; index <= 4096; ++index) {
        const float input = -5.0f + 10.0f * index / 4096.0f;
        const float difference =
            aln_saturation::evaluateNormalizedModel(input, 0u)
            - aln_saturation::evaluateNormalizedModel(input, 1u);
        modelDifferenceSquared += difference * difference;
    }
    const float modelDifferenceRms = static_cast<float>(sqrt(
        modelDifferenceSquared / 4097.0
    ));
    if (modelDifferenceRms < 0.25f)
        return fail("simulated stages are not distinct");

    aln_saturation::BiasMemory memory;
    memory.initialise(48000.0f);
    for (int index = 0; index < 4800; ++index) memory.process(5.0f, 1.0f);
    const float chargedShift = memory.shift;
    if (chargedShift < 1.5f || chargedShift > 1.76f)
        return fail("bias-memory charge");
    for (int index = 0; index < 48000; ++index) memory.process(0.0f, 1.0f);
    if (memory.shift > 0.01f)
        return fail("bias-memory recovery");

    if (pluginEntry(kNT_selector_version, 0) != kNT_apiVersion13)
        return fail("API version");
    const _NT_factory* factory = reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0)
    );
    if (factory == NULL || factory->guid != NT_MULTICHAR('T', 'h', 'S', 'a'))
        return fail("factory metadata");
    if (std::string(factory->name) != "ALN Saturation")
        return fail("factory name");

    _NT_algorithmRequirements requirements = {};
    factory->calculateRequirements(requirements, NULL);
    if (requirements.numParameters != 10u || requirements.sram == 0u)
        return fail("requirements");
    std::vector<uint64_t> alignedSram((requirements.sram + 7u) / 8u);
    _NT_algorithmMemoryPtrs pointers = {};
    pointers.sram = reinterpret_cast<uint8_t*>(alignedSram.data());
    _NT_algorithm* algorithm = factory->construct(pointers, requirements, NULL);

    static const char* const expectedStages[] = {
        "AX7 Data",
        "AX7 Hot",
        "Bias Memory",
    };
    if (
        algorithm->parameters[3].unit != kNT_unitEnum
        || algorithm->parameters[3].min != 0
        || algorithm->parameters[3].max != 2
        || algorithm->parameters[3].def != 0
    )
        return fail("Stage parameter definition");
    for (int index = 0; index < 3; ++index) {
        if (std::string(algorithm->parameters[3].enumStrings[index]) != expectedStages[index])
            return fail("Stage enum strings");
    }
    if (
        algorithm->parameters[4].min != 0
        || algorithm->parameters[4].max != 800
        || algorithm->parameters[4].def != 100
    )
        return fail("Drive range/default");
    if (algorithm->parameters[5].min != -100 || algorithm->parameters[5].max != 100)
        return fail("Bias range");
    if (
        algorithm->parameters[6].min != 0
        || algorithm->parameters[6].max != 100
        || algorithm->parameters[6].def != 50
    )
        return fail("Memory range/default");
    if (algorithm->parameters[8].max != 200 || algorithm->parameters[8].def != 100)
        return fail("Level range/default");
    if (algorithm->parameters[9].def != 0)
        return fail("Low-pass default");

    int16_t values[] = {1, 13, 1, 0, 100, 0, 50, 100, 100, 0};
    algorithm->v = values;
    algorithm->vIncludingCommon = values;
    const int frames = 128;
    const int outputOffset = 12 * frames;
    std::vector<float> buses(kNT_lastBus * frames, 0.0f);
    for (int block = 0; block < 200; ++block) {
        for (int frame = 0; frame < frames; ++frame) {
            buses[frame] = 4.0f * sinf(
                2.0f * 3.14159265358979323846f
                * (block * frames + frame) / 480.0f
            );
        }
        factory->step(algorithm, buses.data(), frames / 4);
    }
    float peak = 0.0f;
    for (int frame = 0; frame < frames; ++frame)
        peak = std::max(peak, fabsf(buses[outputOffset + frame]));
    if (peak < 1.0f || peak > 5.01f)
        return fail("replace-mode audio output level");

    float previous = buses[outputOffset + frames - 1];
    float maximumSwitchStep = 0.0f;
    values[3] = 2;
    values[6] = 100;
    for (int block = 0; block < 16; ++block) {
        for (int frame = 0; frame < frames; ++frame) {
            buses[frame] = 4.0f * sinf(
                2.0f * 3.14159265358979323846f
                * ((200 + block) * frames + frame) / 480.0f
            );
        }
        factory->step(algorithm, buses.data(), frames / 4);
        for (int frame = 0; frame < frames; ++frame) {
            maximumSwitchStep = std::max(
                maximumSwitchStep,
                fabsf(buses[outputOffset + frame] - previous)
            );
            previous = buses[outputOffset + frame];
        }
    }
    if (maximumSwitchStep > 0.2f)
        return fail("Stage switch discontinuity");

    values[4] = 0;
    for (int block = 0; block < 200; ++block) {
        for (int frame = 0; frame < frames; ++frame) buses[frame] = 2.5f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    if (!near(buses[outputOffset + frames - 1], 2.5f, 0.003f))
        return fail("Drive 0% settled identity");

    values[2] = 0;
    values[7] = 0;
    for (int frame = 0; frame < frames; ++frame) {
        buses[frame] = 0.25f;
        buses[outputOffset + frame] = 1.0f;
    }
    factory->step(algorithm, buses.data(), frames / 4);
    for (int frame = 0; frame < frames; ++frame) {
        if (!near(buses[outputOffset + frame], 1.25f))
            return fail("Add-mode Mix 0% result");
    }

    std::cout << "PASS: API v13, ThSa GUID, two 256-leaf learned ngspice stages, Bias Memory charge/recovery, Eurorack levels, Drive/Mix identity, Add/Replace routing, and click-safe Stage switching; model difference "
              << modelDifferenceRms << " V RMS; memory charge "
              << chargedShift << " V; maximum switch step "
              << maximumSwitchStep << " V\n";
    return 0;
}
