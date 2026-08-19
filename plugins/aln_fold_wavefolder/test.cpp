#include <distingnt/api.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "aln_fold_core.h"

extern "C" const _NT_globals NT_globals = {
    48000u,
    128u,
    NULL,
    0u,
    0u,
    0u,
};

namespace {

bool near(float actual, float expected, float tolerance = 1.0e-4f) {
    return fabsf(actual - expected) < tolerance;
}

int fail(const char* message) {
    std::cerr << "FAIL: " << message << "\n";
    return 1;
}

}  // namespace

int main() {
    for (int index = 0; index <= 100; ++index) {
        const float input = -5.0f + 10.0f * index / 100.0f;
        if (aln_fold::evaluateFoldDTree(input, 0.0f) != input)
            return fail("zero-Fold identity evaluator");
    }

    if (pluginEntry(kNT_selector_version, 0) != kNT_apiVersion13)
        return fail("API version");
    const _NT_factory* factory = reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0)
    );
    if (factory == NULL || factory->guid != NT_MULTICHAR('T', 'h', 'W', 'f'))
        return fail("factory metadata");
    if (std::string(factory->name) != "ALN Fold")
        return fail("factory name");

    _NT_algorithmRequirements requirements = {};
    factory->calculateRequirements(requirements, NULL);
    if (requirements.numParameters != 7u || requirements.sram == 0u)
        return fail("requirements");
    std::vector<uint64_t> alignedSram((requirements.sram + 7u) / 8u);
    _NT_algorithmMemoryPtrs pointers = {};
    pointers.sram = reinterpret_cast<uint8_t*>(alignedSram.data());
    _NT_algorithm* algorithm = factory->construct(pointers, requirements, NULL);
    if (algorithm->parameters[5].def != 0)
        return fail("Low-pass must default Off");
    if (
        algorithm->parameters[6].def != 1
        || algorithm->parameters[6].unit != kNT_unitEnum
        || std::string(algorithm->parameters[6].enumStrings[0]) != "Buchla 259"
        || std::string(algorithm->parameters[6].enumStrings[1]) != "16-fold"
    )
        return fail("Model enum contract");

    float previousOutput = aln_fold::evaluateFoldDTree(-5.0f, 1.0f);
    float previousDelta = 0.0f;
    float foldedPeak = fabsf(previousOutput);
    int foldTurns = 0;
    for (int index = 1; index <= 4096; ++index) {
        const float input = -5.0f + 10.0f * index / 4096.0f;
        const float currentOutput = aln_fold::evaluateFoldDTree(input, 1.0f);
        const float delta = currentOutput - previousOutput;
        foldedPeak = std::max(foldedPeak, fabsf(currentOutput));
        if (index > 1 && delta * previousDelta < 0.0f) ++foldTurns;
        previousOutput = currentOutput;
        previousDelta = delta;
    }
    if (foldTurns < 16) return fail("10 Vpp input does not reach all Fold turns");
    if (foldedPeak < 4.9f || foldedPeak > 5.1f)
        return fail("full-Fold output level");

    float previousBuchla = aln_fold::evaluateFoldDTree(
        -5.0f, 1.0f, aln_fold::kBuchla259Model
    );
    float previousBuchlaDelta = 0.0f;
    float buchlaPeak = fabsf(previousBuchla);
    int buchlaTurns = 0;
    for (int index = 1; index <= 4096; ++index) {
        const float input = -5.0f + 10.0f * index / 4096.0f;
        const float current = aln_fold::evaluateFoldDTree(
            input, 1.0f, aln_fold::kBuchla259Model
        );
        const float delta = current - previousBuchla;
        buchlaPeak = std::max(buchlaPeak, fabsf(current));
        if (index > 1 && delta * previousBuchlaDelta < 0.0f) ++buchlaTurns;
        previousBuchla = current;
        previousBuchlaDelta = delta;
    }
    if (buchlaTurns != 8) return fail("Buchla model Fold turns");
    if (buchlaPeak < 4.9f || buchlaPeak > 5.1f)
        return fail("Buchla model output level");

    aln_fold::FoldAntialiaser audioAntialiaser;
    float audioMinimum = 100.0f;
    float audioMaximum = -100.0f;
    for (int index = 0; index < 4800; ++index) {
        const float input = 5.0f * sinf(
            2.0f * 3.14159265358979323846f * 100.0f * index / 48000.0f
        );
        const float output = audioAntialiaser.process(input, 1.0f);
        if (index >= 480) {
            audioMinimum = std::min(audioMinimum, output);
            audioMaximum = std::max(audioMaximum, output);
        }
    }
    const float audioPeakToPeak = audioMaximum - audioMinimum;
    if (audioPeakToPeak < 9.7f || audioPeakToPeak > 10.1f)
        return fail("antialiased 100 Hz output level");

    int16_t values[] = {1, 13, 1, 100, 100, 0, 1};
    algorithm->v = values;
    algorithm->vIncludingCommon = values;

    const int frames = 128;
    std::vector<float> buses(kNT_lastBus * frames, 0.0f);
    for (int index = 0; index < frames; ++index)
        buses[index] = -5.0f + 10.0f * index / (frames - 1.0f);
    aln_fold::FoldAntialiaser expectedAntialiaser;
    factory->step(algorithm, buses.data(), frames / 4);
    const int outputOffset = 12 * frames;
    for (int index = 0; index < frames; ++index) {
        if (!near(
                buses[outputOffset + index],
                expectedAntialiaser.process(buses[index], 1.0f)
            ))
            return fail("maximum-Fold replace result");
    }

    values[3] = 0;
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 2.5f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    if (!near(
            buses[outputOffset + frames - 1],
            2.5f,
            2.0e-3f
        ))
        return fail("zero-Fold settled identity result");

    values[3] = 100;
    float previous = buses[outputOffset + frames - 1];
    float maximumStep = 0.0f;
    for (int block = 0; block < 20; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 2.5f;
        factory->step(algorithm, buses.data(), frames / 4);
        for (int index = 0; index < frames; ++index) {
            maximumStep = std::max(maximumStep, fabsf(buses[outputOffset + index] - previous));
            previous = buses[outputOffset + index];
        }
    }
    if (maximumStep > 0.05f) {
        std::cerr << "measured maximum Fold step: " << maximumStep << " V\n";
        return fail("Fold smoothing discontinuity");
    }

    values[2] = 0;
    values[4] = 0;
    for (int index = 0; index < frames; ++index) {
        buses[index] = 0.25f;
        buses[outputOffset + index] = 1.0f;
    }
    factory->step(algorithm, buses.data(), frames / 4);
    for (int index = 0; index < frames; ++index) {
        if (!near(buses[outputOffset + index], 1.25f))
            return fail("add-mode dry result");
    }

    values[2] = 1;
    values[3] = 100;
    values[4] = 100;
    values[6] = 1;
    for (int block = 0; block < 200; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 1.25f;
        factory->step(algorithm, buses.data(), frames / 4);
    }
    previous = buses[outputOffset + frames - 1];
    float modelSwitchMaximumStep = 0.0f;
    values[6] = 0;
    for (int block = 0; block < 8; ++block) {
        for (int index = 0; index < frames; ++index) buses[index] = 1.25f;
        factory->step(algorithm, buses.data(), frames / 4);
        for (int index = 0; index < frames; ++index) {
            modelSwitchMaximumStep = std::max(
                modelSwitchMaximumStep,
                fabsf(buses[outputOffset + index] - previous)
            );
            previous = buses[outputOffset + index];
        }
    }
    if (modelSwitchMaximumStep > 0.05f)
        return fail("Model switch discontinuity");
    if (!near(
            previous,
            aln_fold::evaluateFoldDTree(
                1.25f, 1.0f, aln_fold::kBuchla259Model
            ),
            2.0e-3f
        ))
        return fail("Buchla model selection result");

    std::cout << "PASS: API, zero-Fold identity, 10 Vpp input / 10 Vpp output full Fold ("
              << "Buchla " << buchlaTurns << " turns, 16-fold " << foldTurns
              << " turns, " << foldedPeak
              << " V direct peak, " << audioPeakToPeak
              << " Vpp antialiased at 100 Hz), Low-pass default Off, smoothing, replace, add, and click-safe Model enum; Fold step "
              << maximumStep << " V, Model step " << modelSwitchMaximumStep << " V\n";
    return 0;
}
