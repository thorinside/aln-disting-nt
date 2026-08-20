#pragma once

#include <math.h>
#include <stdint.h>

#include "generated/aln_distortion_models.h"

namespace aln_distortion {

static const float kMaximumDrive = 8.0f;

inline float clamp(float value, float lower, float upper) {
    return value < lower ? lower : (value > upper ? upper : value);
}

inline uint8_t clampModelIndex(uint8_t modelIndex) {
    return modelIndex < kDistortionModelCount ? modelIndex : 0u;
}

inline uint16_t leafIndex(const DistortionModelData& model, float input) {
    uint16_t ref = model.rootRef;
    while ((ref & kLeafMask) == 0u) {
        const DTreeNode& node = model.nodes[ref];
        ref = input > node.threshold ? node.upperRef : node.lowerRef;
    }
    return ref & 0x7fffu;
}

inline float evaluateRawLeaf(
    const DistortionModelData& model,
    float input,
    uint16_t index
) {
    const AffineLeaf& leaf = model.leaves[index];
    return leaf.slope * input + leaf.intercept;
}

inline float evaluateRawModel(const DistortionModelData& model, float input) {
    return evaluateRawLeaf(model, input, leafIndex(model, input));
}

inline float evaluateRawAntiderivative(
    const DistortionModelData& model,
    float input,
    uint16_t index
) {
    const AffineLeaf& leaf = model.leaves[index];
    return 0.5f * leaf.slope * input * input
        + leaf.intercept * input
        + leaf.integralConstant;
}

inline float evaluateNormalizedModel(float input, uint8_t modelIndex) {
    const DistortionModelData& model = kDistortionModels[
        clampModelIndex(modelIndex)
    ];
    const float driven = clamp(input, model.inputMinimum, model.inputMaximum);
    return model.outputGain * evaluateRawModel(model, driven) + model.outputOffset;
}

inline float evaluateDistortion(
    float input,
    float drive,
    float biasV,
    uint8_t modelIndex
) {
    const float inputGain = clamp(drive, 0.0f, kMaximumDrive);
    const float wetAmount = clamp(drive, 0.0f, 1.0f);
    const float driven = inputGain * (input + biasV);
    const float distorted = evaluateNormalizedModel(driven, modelIndex);
    return input + wetAmount * (distorted - input);
}

struct Antialiaser {
    float previousInput;
    bool initialised;

    Antialiaser() : previousInput(0.0f), initialised(false) {}

    float process(float input, uint8_t modelIndex) {
        const DistortionModelData& model = kDistortionModels[
            clampModelIndex(modelIndex)
        ];
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

struct ModelState {
    Antialiaser antialiaser;
    DcBlocker dcBlocker;
};

struct DistortionBank {
    ModelState states[kDistortionModelCount];
    uint8_t currentModel;
    uint8_t previousModel;
    float blend;
    float maximumStep;
    bool initialised;

    DistortionBank()
        : currentModel(0u), previousModel(0u), blend(1.0f),
          maximumStep(1.0f), initialised(false) {}

    void initialise(float sampleRate) {
        maximumStep = 1.0f / (0.01f * sampleRate);
        for (uint8_t model = 0; model < kDistortionModelCount; ++model)
            states[model].dcBlocker.initialise(sampleRate);
    }

    float process(
        float input,
        float drive,
        float biasV,
        uint8_t selectedModel
    ) {
        const uint8_t selected = clampModelIndex(selectedModel);
        if (!initialised) {
            currentModel = selected;
            previousModel = selected;
            blend = 1.0f;
            initialised = true;
        } else if (selected != currentModel) {
            previousModel = currentModel;
            currentModel = selected;
            blend = 0.0f;
        }
        blend = clamp(blend + maximumStep, 0.0f, 1.0f);

        const float inputGain = clamp(drive, 0.0f, kMaximumDrive);
        const float wetAmount = clamp(drive, 0.0f, 1.0f);
        const float driven = inputGain * (input + biasV);
        float shaped[kDistortionModelCount];
        for (uint8_t model = 0; model < kDistortionModelCount; ++model) {
            const float limited = clamp(
                states[model].antialiaser.process(driven, model),
                -5.0f,
                5.0f
            );
            const float circuit = states[model].dcBlocker.process(limited);
            shaped[model] = input + wetAmount * (circuit - input);
        }
        return shaped[previousModel]
            + blend * (shaped[currentModel] - shaped[previousModel]);
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

}  // namespace aln_distortion
