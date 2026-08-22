#pragma once

#include <math.h>
#include <stdint.h>

#include "generated/aln_saturation_models.h"

namespace aln_saturation {

static const float kMaximumDrive = 8.0f;
static const uint8_t kStageCount = 3u;

inline float clamp(float value, float lower, float upper) {
    return value < lower ? lower : (value > upper ? upper : value);
}

inline uint8_t clampStageIndex(uint8_t stageIndex) {
    return stageIndex < kStageCount ? stageIndex : 0u;
}

inline uint8_t modelIndexForStage(uint8_t stageIndex) {
    return clampStageIndex(stageIndex) == 0u ? 0u : 1u;
}

inline uint16_t leafIndex(const SaturationModelData& model, float input) {
    uint16_t ref = model.rootRef;
    while ((ref & kLeafMask) == 0u) {
        const DTreeNode& node = model.nodes[ref];
        ref = input > node.threshold ? node.upperRef : node.lowerRef;
    }
    return ref & 0x7fffu;
}

inline float evaluateRawLeaf(
    const SaturationModelData& model,
    float input,
    uint16_t index
) {
    const AffineLeaf& leaf = model.leaves[index];
    return leaf.slope * input + leaf.intercept;
}

inline float evaluateRawModel(const SaturationModelData& model, float input) {
    return evaluateRawLeaf(model, input, leafIndex(model, input));
}

inline float evaluateRawAntiderivative(
    const SaturationModelData& model,
    float input,
    uint16_t index
) {
    const AffineLeaf& leaf = model.leaves[index];
    return 0.5f * leaf.slope * input * input
        + leaf.intercept * input
        + leaf.integralConstant;
}

inline float evaluateNormalizedModel(float input, uint8_t modelIndex) {
    const uint8_t selected = modelIndex < kSaturationModelCount ? modelIndex : 0u;
    const SaturationModelData& model = kSaturationModels[selected];
    const float driven = clamp(input, model.inputMinimum, model.inputMaximum);
    return model.outputGain * evaluateRawModel(model, driven) + model.outputOffset;
}

struct Antialiaser {
    float previousInput;
    bool initialised;

    Antialiaser() : previousInput(0.0f), initialised(false) {}

    float process(float input, uint8_t modelIndex) {
        const uint8_t selected = modelIndex < kSaturationModelCount ? modelIndex : 0u;
        const SaturationModelData& model = kSaturationModels[selected];
        const float current = clamp(input, model.inputMinimum, model.inputMaximum);
        const float previous = clamp(
            previousInput,
            model.inputMinimum,
            model.inputMaximum
        );
        const float delta = current - previous;
        float raw;
        if (!initialised) {
            raw = evaluateRawModel(model, current);
            initialised = true;
        } else {
            const uint16_t currentLeaf = leafIndex(model, current);
            const uint16_t previousLeaf = leafIndex(model, previous);
            if (currentLeaf == previousLeaf || fabsf(delta) <= 1.0e-5f) {
                raw = 0.5f * (
                    evaluateRawLeaf(model, current, currentLeaf)
                    + evaluateRawLeaf(model, previous, previousLeaf)
                );
            } else {
                raw = (
                    evaluateRawAntiderivative(model, current, currentLeaf)
                    - evaluateRawAntiderivative(model, previous, previousLeaf)
                ) / delta;
            }
        }
        previousInput = current;
        return model.outputGain * raw + model.outputOffset;
    }
};

struct DcBlocker {
    float previousInput;
    float previousOutput;
    float coefficient;
    bool initialised;

    DcBlocker()
        : previousInput(0.0f), previousOutput(0.0f),
          coefficient(0.999f), initialised(false) {}

    void initialise(float sampleRate, float cutoffHz = 5.0f) {
        coefficient = expf(-2.0f * 3.14159265358979323846f * cutoffHz / sampleRate);
    }

    float process(float input) {
        if (!initialised) {
            previousInput = input;
            previousOutput = 0.0f;
            initialised = true;
            return 0.0f;
        }
        const float output = input - previousInput + coefficient * previousOutput;
        previousInput = input;
        previousOutput = output;
        return output;
    }
};

struct BoundedDcBlocker {
    float dcEstimate;
    float coefficient;
    bool initialised;

    BoundedDcBlocker()
        : dcEstimate(0.0f), coefficient(0.0f), initialised(false) {}

    void initialise(float sampleRate, float cutoffHz = 5.0f) {
        coefficient = 1.0f - expf(
            -2.0f * 3.14159265358979323846f * cutoffHz / sampleRate
        );
    }

    float process(float input, float limit) {
        if (!initialised) {
            dcEstimate = input;
            initialised = true;
            return 0.0f;
        }
        dcEstimate += coefficient * (input - dcEstimate);
        const float centred = input - dcEstimate;
        const float headroom = limit / (limit + fabsf(dcEstimate));
        return centred * headroom;
    }
};

struct BiasMemory {
    float shift;
    float attackCoefficient;
    float releaseCoefficient;

    BiasMemory()
        : shift(0.0f), attackCoefficient(0.0f), releaseCoefficient(0.0f) {}

    void initialise(float sampleRate) {
        attackCoefficient = 1.0f - expf(
            -2.0f * 3.14159265358979323846f * 80.0f / sampleRate
        );
        releaseCoefficient = 1.0f - expf(
            -2.0f * 3.14159265358979323846f * 1.0f / sampleRate
        );
    }

    float process(float excitation, float amount) {
        const float target = clamp(0.4f * (excitation - 0.75f), 0.0f, 1.75f);
        const float coefficient = target > shift
            ? attackCoefficient
            : releaseCoefficient;
        shift += coefficient * (target - shift);
        return clamp(amount, 0.0f, 1.0f) * shift;
    }
};

struct StageState {
    Antialiaser antialiaser;
    DcBlocker dcBlocker;
};

struct SaturationBank {
    StageState states[kStageCount];
    BiasMemory biasMemory;
    uint8_t currentStage;
    uint8_t previousStage;
    float blend;
    float maximumStep;
    bool initialised;

    SaturationBank()
        : currentStage(0u), previousStage(0u), blend(1.0f),
          maximumStep(1.0f), initialised(false) {}

    void initialise(float sampleRate) {
        maximumStep = 1.0f / (0.01f * sampleRate);
        biasMemory.initialise(sampleRate);
        for (uint8_t stage = 0; stage < kStageCount; ++stage)
            states[stage].dcBlocker.initialise(sampleRate);
    }

    float process(
        float input,
        float drive,
        float biasV,
        float memoryAmount,
        uint8_t selectedStage
    ) {
        const uint8_t selected = clampStageIndex(selectedStage);
        if (!initialised) {
            currentStage = selected;
            previousStage = selected;
            blend = 1.0f;
            initialised = true;
        } else if (selected != currentStage) {
            previousStage = currentStage;
            currentStage = selected;
            blend = 0.0f;
        }
        blend = clamp(blend + maximumStep, 0.0f, 1.0f);

        const float inputGain = clamp(drive, 0.0f, kMaximumDrive);
        const float wetAmount = clamp(drive, 0.0f, 1.0f);
        const float driven = inputGain * (input + biasV);
        const float memoryShift = biasMemory.process(driven, memoryAmount);
        const float stageInputs[kStageCount] = {
            driven,
            driven,
            driven - memoryShift,
        };
        float shaped[kStageCount];
        for (uint8_t stage = 0; stage < kStageCount; ++stage) {
            const float limited = clamp(
                states[stage].antialiaser.process(
                    stageInputs[stage],
                    modelIndexForStage(stage)
                ),
                -5.0f,
                5.0f
            );
            const float circuit = states[stage].dcBlocker.process(limited);
            shaped[stage] = input + wetAmount * (circuit - input);
        }
        return shaped[previousStage]
            + blend * (shaped[currentStage] - shaped[previousStage]);
    }
};

struct ControlSmoother {
    float value;
    float coefficient;
    bool initialised;

    ControlSmoother() : value(0.0f), coefficient(0.0f), initialised(false) {}

    void initialise(float sampleRate, float cutoffHz = 10.0f) {
        coefficient = 1.0f - expf(
            -2.0f * 3.14159265358979323846f * cutoffHz / sampleRate
        );
    }

    float process(float target) {
        if (!initialised) {
            value = target;
            initialised = true;
        } else {
            value += coefficient * (target - value);
        }
        return value;
    }
};

struct OnePole {
    float state;
    float coefficient;

    OnePole() : state(0.0f), coefficient(1.0f) {}

    void setCutoff(float cutoffHz, float sampleRate) {
        coefficient = 1.0f - expf(
            -2.0f * 3.14159265358979323846f * cutoffHz / sampleRate
        );
    }

    float process(float input) {
        state += coefficient * (input - state);
        return state;
    }
};

}  // namespace aln_saturation
