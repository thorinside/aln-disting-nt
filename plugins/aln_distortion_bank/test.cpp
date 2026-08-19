#include <distingnt/api.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "aln_distortion_core.h"

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

void setParameter(int16_t* values, int index, int16_t value) {
    values[index] = value;
}

}  // namespace

int main() {
    static const char* const expectedNames[] = {
        "Silicon Soft",
        "Germanium",
        "LED Clip",
        "Asymmetric",
        "Op-amp Hard",
        "BJT Saturation",
        "CMOS Inverter",
        "Full-wave",
    };
    static_assert(
        aln_distortion::kDistortionModelCount == 8u,
        "model count"
    );

    for (uint8_t model = 0; model < aln_distortion::kDistortionModelCount; ++model) {
        if (aln_distortion::evaluateDistortion(2.5f, 0.0f, 0.0f, model) != 2.5f)
            return fail("zero-Drive identity");
        const float compatibilityInput = 0.5f;
        if (!near(
                aln_distortion::evaluateDistortion(
                    compatibilityInput, 1.0f, 0.0f, model
                ),
                aln_distortion::evaluateNormalizedModel(compatibilityInput, model)
            ))
            return fail("100-percent Drive compatibility");
        float minimum = 100.0f;
        float maximum = -100.0f;
        for (int index = 0; index <= 4096; ++index) {
            const float sweepInput = -5.0f + 10.0f * index / 4096.0f;
            const float output = aln_distortion::evaluateNormalizedModel(
                sweepInput, model
            );
            minimum = std::min(minimum, output);
            maximum = std::max(maximum, output);
        }
        if (minimum > -4.9f || maximum < 4.9f)
            return fail("normalized circuit output range");
    }

    if (pluginEntry(kNT_selector_version, 0) != kNT_apiVersion13)
        return fail("API version");
    const _NT_factory* factory = reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0)
    );
    if (factory == NULL || factory->guid != NT_MULTICHAR('A', 'd', 'B', '1'))
        return fail("factory metadata");
    if (std::string(factory->name) != "ALN Distortion Bank")
        return fail("factory name");
    _NT_algorithmRequirements requirements = {};
    factory->calculateRequirements(requirements, NULL);
    if (requirements.numParameters != 9u || requirements.sram == 0u)
        return fail("requirements");
    std::vector<uint64_t> alignedSram((requirements.sram + 7u) / 8u);
    _NT_algorithmMemoryPtrs pointers = {};
    pointers.sram = reinterpret_cast<uint8_t*>(alignedSram.data());
    _NT_algorithm* algorithm = factory->construct(pointers, requirements, NULL);
    if (
        algorithm->parameters[3].unit != kNT_unitEnum
        || algorithm->parameters[3].min != 0
        || algorithm->parameters[3].max != 7
        || algorithm->parameters[3].def != 0
    )
        return fail("Circuit parameter definition");
    for (int index = 0; index < 8; ++index) {
        if (std::string(algorithm->parameters[3].enumStrings[index]) != expectedNames[index])
            return fail("Circuit enum strings");
    }
    if (
        algorithm->parameters[4].min != 0
        || algorithm->parameters[4].max != 800
        || algorithm->parameters[4].def != 100
    )
        return fail("Drive range/default");
    if (algorithm->parameters[5].min != -100 || algorithm->parameters[5].max != 100)
        return fail("Bias range");
    if (algorithm->parameters[7].max != 200 || algorithm->parameters[7].def != 100)
        return fail("Level range");
    if (algorithm->parameters[8].def != 0)
        return fail("Low-pass default");

    const float normalDrive = aln_distortion::evaluateDistortion(
        0.5f, 1.0f, 0.0f, 0u
    );
    const float slammedDrive = aln_distortion::evaluateDistortion(
        0.5f, aln_distortion::kMaximumDrive, 0.0f, 0u
    );
    if (fabsf(slammedDrive - normalDrive) < 0.5f)
        return fail("extended Drive changes circuit excitation");
    if (!near(
            slammedDrive,
            aln_distortion::evaluateDistortion(0.5f, 80.0f, 0.0f, 0u)
        ))
        return fail("extended Drive clamp");

    int16_t values[] = {1, 13, 1, 0, 100, 0, 100, 100, 0};
    algorithm->v = values;
    algorithm->vIncludingCommon = values;
    const int frames = 128;
    const int outputOffset = 12 * frames;
    std::vector<float> buses(kNT_lastBus * frames, 0.0f);
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index)
            buses[index] = 4.0f * sinf(
                2.0f * 3.14159265358979323846f * (block * frames + index) / 480.0f
            );
        factory->step(algorithm, buses.data(), frames / 4);
    }
    float peak = 0.0f;
    for (int index = 0; index < frames; ++index)
        peak = std::max(peak, fabsf(buses[outputOffset + index]));
    if (peak < 1.0f || peak > 5.01f)
        return fail("replace-mode audio output level");

    for (int block = 0; block < 8; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 1.5f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    float previous = buses[outputOffset + frames - 1];
    float maximumSwitchStep = 0.0f;
    values[3] = 7;
    for (int block = 0; block < 8; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 1.5f;
        factory->step(algorithm, buses.data(), frames / 4);
        for (int index = 0; index < frames; ++index) {
            maximumSwitchStep = std::max(
                maximumSwitchStep,
                fabsf(buses[outputOffset + index] - previous)
            );
            previous = buses[outputOffset + index];
        }
    }
    if (maximumSwitchStep > 0.08f) {
        std::cerr << "measured switch step: " << maximumSwitchStep << " V\n";
        return fail("Circuit switch discontinuity");
    }

    values[4] = 0;
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 2.5f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    if (!near(buses[outputOffset + frames - 1], 2.5f, 0.003f))
        return fail("zero-Drive settled identity");

    setParameter(values, 3, 0);
    setParameter(values, 4, 100);
    std::vector<float> normalDriveOutput(frames);
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index)
            buses[index] = 0.5f * sinf(
                2.0f * 3.14159265358979323846f * (block * frames + index) / 480.0f
            );
        factory->step(algorithm, buses.data(), frames / 4);
    }
    std::copy(
        buses.begin() + outputOffset,
        buses.begin() + outputOffset + frames,
        normalDriveOutput.begin()
    );
    setParameter(values, 4, 800);
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index)
            buses[index] = 0.5f * sinf(
                2.0f * 3.14159265358979323846f * (block * frames + index) / 480.0f
            );
        factory->step(algorithm, buses.data(), frames / 4);
    }
    float slamDifferenceSquared = 0.0f;
    for (int index = 0; index < frames; ++index) {
        const float difference = buses[outputOffset + index]
            - normalDriveOutput[index];
        slamDifferenceSquared += difference * difference;
        if (fabsf(buses[outputOffset + index]) > 5.01f)
            return fail("extended Drive output ceiling");
    }
    const float slamDifferenceRms = sqrtf(slamDifferenceSquared / frames);
    if (slamDifferenceRms < 0.5f)
        return fail("extended Drive full-plugin response");

    setParameter(values, 4, 0);
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 0.25f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    values[2] = 0;
    for (int index = 0; index < frames; ++index) {
        buses[index] = 0.25f;
        buses[outputOffset + index] = 1.0f;
    }
    factory->step(algorithm, buses.data(), frames / 4);
    for (int index = 0; index < frames; ++index) {
        if (!near(buses[outputOffset + index], 1.25f))
            return fail("add-mode dry result");
    }

    std::cout << "PASS: API v13, eight circuit enum, 10 Vpp learned ranges, 0-800% Drive with 100% compatibility and full-plugin slam response, Drive identity, Bias/Level contracts, replace/add routing, and click-safe switching; maximum switch step "
              << maximumSwitchStep << " V; slam difference "
              << slamDifferenceRms << " V RMS\n";
    return 0;
}
