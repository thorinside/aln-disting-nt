#pragma once

#include <math.h>
#include <stdint.h>

#include "generated/aln_fold_models.h"

namespace aln_fold {

inline float clamp(float value, float lower, float upper) {
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}

inline uint8_t clampModelIndex(uint8_t modelIndex) {
    return modelIndex < kFoldModelCount ? modelIndex : kSixteenFoldModel;
}

inline float rotatedCoefficient(
    const FoldModelData& model,
    uint16_t coefficientIndex,
    float rotation
) {
    const uint16_t coefficientCount = model.hingeCount + 1u;
    const float phase = clamp(rotation, 0.0f, 1.0f) * coefficientCount;
    uint16_t lowerShift = static_cast<uint16_t>(phase);
    float fraction = phase - lowerShift;
    if (lowerShift >= coefficientCount) {
        lowerShift = 0u;
        fraction = 0.0f;
    }
    uint16_t lowerIndex = coefficientIndex + lowerShift;
    if (lowerIndex >= coefficientCount) lowerIndex -= coefficientCount;
    uint16_t upperIndex = lowerIndex + 1u;
    if (upperIndex >= coefficientCount) upperIndex = 0u;
    return model.coefficients[lowerIndex]
        + fraction * (
            model.coefficients[upperIndex] - model.coefficients[lowerIndex]
        );
}

inline float evaluateRawRotatedFold(
    const FoldModelData& model,
    float driven,
    float rotation
) {
    const float magnitude = fabsf(driven);
    const float sign = driven < 0.0f ? -1.0f : 1.0f;
    float output = rotatedCoefficient(model, 0u, rotation) * driven;
    for (uint16_t index = 0u; index < model.hingeCount; ++index) {
        const float distance = magnitude - model.thresholds[index];
        if (distance > 0.0f) {
            output += rotatedCoefficient(model, index + 1u, rotation)
                * sign * distance;
        }
    }
    return output;
}

inline float evaluateRawRotatedAntiderivative(
    const FoldModelData& model,
    float driven,
    float rotation
) {
    const float magnitude = fabsf(driven);
    float output = 0.5f * rotatedCoefficient(model, 0u, rotation)
        * driven * driven;
    for (uint16_t index = 0u; index < model.hingeCount; ++index) {
        const float distance = magnitude - model.thresholds[index];
        if (distance > 0.0f) {
            output += 0.5f * rotatedCoefficient(
                model,
                index + 1u,
                rotation
            ) * distance * distance;
        }
    }
    return output;
}

inline float rotatedRawPeak(const FoldModelData& model, float rotation) {
    float value = 0.0f;
    float previous = 0.0f;
    float peak = 0.0f;
    float slope = rotatedCoefficient(model, 0u, rotation);
    for (uint16_t index = 0u; index < model.hingeCount; ++index) {
        const float threshold = model.thresholds[index];
        value += slope * (threshold - previous);
        peak = fmaxf(peak, fabsf(value));
        slope += rotatedCoefficient(model, index + 1u, rotation);
        previous = threshold;
    }
    value += slope * (model.inputMaximum - previous);
    return fmaxf(peak, fabsf(value));
}

inline float rotatedOutputGain(
    const FoldModelData& model,
    float rotation,
    float levelCompensation
) {
    const float peak = fmaxf(rotatedRawPeak(model, rotation), 1.0e-6f);
    const float compensated = 5.0f / peak;
    const float blend = clamp(levelCompensation, 0.0f, 1.0f);
    return model.outputGain + blend * (compensated - model.outputGain);
}

inline uint16_t foldLeafIndex(
    const FoldModelData& model,
    float driven
) {
    uint16_t ref = model.rootRef;
    while ((ref & kLeafMask) == 0u) {
        const DTreeNode& node = model.nodes[ref];
        ref = driven > node.threshold ? node.upperRef : node.lowerRef;
    }
    return ref & 0x7fffu;
}

inline float evaluateRawFoldLeaf(
    const FoldModelData& model,
    float driven,
    uint16_t leafIndex
) {
    const AffineLeaf& leaf = model.leaves[leafIndex];
    return leaf.slope * driven + leaf.intercept;
}

inline float evaluateRawFoldDTree(
    const FoldModelData& model,
    float driven
) {
    return evaluateRawFoldLeaf(model, driven, foldLeafIndex(model, driven));
}

inline float evaluateRawFoldAntiderivative(
    const FoldModelData& model,
    float driven,
    uint16_t leafIndex
) {
    const AffineLeaf& leaf = model.leaves[leafIndex];
    return 0.5f * leaf.slope * driven * driven
        + leaf.intercept * driven
        + leaf.integralConstant;
}

inline float evaluateFoldDTree(float input, float fold, uint8_t modelIndex) {
    const FoldModelData& model = kFoldModels[clampModelIndex(modelIndex)];
    const float amount = clamp(fold, 0.0f, 1.0f);
    const float drive = model.driveMinimum
        + amount * (model.driveMaximum - model.driveMinimum);
    const float driven = clamp(
        input * drive,
        model.inputMinimum,
        model.inputMaximum
    );
    const float folded = model.outputGain * (
        evaluateRawFoldDTree(model, driven)
    );
    return input + amount * (folded - input);
}

inline float evaluateRotatedFold(
    float input,
    float fold,
    uint8_t modelIndex,
    float rotation,
    float levelCompensation
) {
    const FoldModelData& model = kFoldModels[clampModelIndex(modelIndex)];
    const float amount = clamp(fold, 0.0f, 1.0f);
    const float drive = model.driveMinimum
        + amount * (model.driveMaximum - model.driveMinimum);
    const float driven = clamp(
        input * drive,
        model.inputMinimum,
        model.inputMaximum
    );
    const float folded = clamp(
        rotatedOutputGain(model, rotation, levelCompensation)
            * evaluateRawRotatedFold(model, driven, rotation),
        -5.0f,
        5.0f
    );
    return input + amount * (folded - input);
}

inline float evaluateFoldDTree(float input, float fold) {
    return evaluateFoldDTree(input, fold, kSixteenFoldModel);
}

struct FoldAntialiaser {
    float previousInput;
    bool initialised;

    FoldAntialiaser() : previousInput(0.0f), initialised(false) {}

    float process(
        float input,
        float fold,
        uint8_t modelIndex,
        float rotation,
        float levelCompensation
    ) {
        const FoldModelData& model = kFoldModels[clampModelIndex(modelIndex)];
        const float amount = clamp(fold, 0.0f, 1.0f);
        if (amount == 0.0f) {
            previousInput = input;
            initialised = true;
            return input;
        }
        const float drive = model.driveMinimum
            + amount * (model.driveMaximum - model.driveMinimum);
        const float driven = clamp(
            input * drive,
            model.inputMinimum,
            model.inputMaximum
        );
        const float previousDriven = clamp(
            previousInput * drive,
            model.inputMinimum,
            model.inputMaximum
        );
        const float delta = driven - previousDriven;
        float raw;
        if (!initialised) {
            raw = evaluateRawRotatedFold(model, driven, rotation);
            initialised = true;
        } else {
            if (fabsf(delta) <= 1.0e-5f) {
                raw = 0.5f * (
                    evaluateRawRotatedFold(model, driven, rotation)
                    + evaluateRawRotatedFold(model, previousDriven, rotation)
                );
            } else {
                raw = (
                    evaluateRawRotatedAntiderivative(model, driven, rotation)
                    - evaluateRawRotatedAntiderivative(
                        model,
                        previousDriven,
                        rotation
                    )
                ) / delta;
            }
        }
        previousInput = input;
        const float folded = clamp(
            rotatedOutputGain(model, rotation, levelCompensation) * raw,
            -5.0f,
            5.0f
        );
        return input + amount * (folded - input);
    }

    float process(float input, float fold, uint8_t modelIndex) {
        return process(input, fold, modelIndex, 0.0f, 1.0f);
    }

    float process(float input, float fold) {
        return process(input, fold, kSixteenFoldModel);
    }
};

struct ModelCrossfader {
    FoldAntialiaser antialiasers[kFoldModelCount];
    float blend;
    float maximumStep;
    bool initialised;

    ModelCrossfader()
        : blend(1.0f), maximumStep(1.0f), initialised(false) {}

    void initialise(float sampleRate) {
        maximumStep = 1.0f / (0.01f * sampleRate);
    }

    float process(
        float input,
        float fold,
        uint8_t selectedModel,
        float rotation,
        float levelCompensation
    ) {
        const uint8_t modelIndex = clampModelIndex(selectedModel);
        const float target = modelIndex == kBuchla259Model ? 0.0f : 1.0f;
        if (!initialised) {
            blend = target;
            initialised = true;
        } else {
            blend += clamp(target - blend, -maximumStep, maximumStep);
        }
        const float buchla = antialiasers[kBuchla259Model].process(
            input,
            fold,
            kBuchla259Model,
            rotation,
            levelCompensation
        );
        const float sixteenFold = antialiasers[kSixteenFoldModel].process(
            input,
            fold,
            kSixteenFoldModel,
            rotation,
            levelCompensation
        );
        return buchla + blend * (sixteenFold - buchla);
    }

    float process(float input, float fold, uint8_t selectedModel) {
        return process(input, fold, selectedModel, 0.0f, 1.0f);
    }
};

struct OnePole {
    float state;
    float coefficient;

    OnePole() : state(0.0f), coefficient(1.0f) {}

    void setCutoff(float cutoffHz, float sampleRate) {
        coefficient = 1.0f - expf(-6.2831853071795864769f * cutoffHz / sampleRate);
    }

    float process(float input) {
        state += coefficient * (input - state);
        return state;
    }
};

struct FoldSmoother {
    float state;
    float coefficient;

    FoldSmoother() : state(1.0f), coefficient(1.0f) {}

    void initialise(float sampleRate) {
        coefficient = 1.0f - expf(-6.2831853071795864769f * 10.0f / sampleRate);
    }

    float process(float target) {
        state += coefficient * (target - state);
        return state;
    }
};

struct ControlSmoother {
    float state;
    float coefficient;

    ControlSmoother() : state(0.0f), coefficient(1.0f) {}

    void reset(float value) {
        state = value;
    }

    void initialise(float sampleRate, float cutoffHz = 10.0f) {
        coefficient = 1.0f - expf(
            -6.2831853071795864769f * cutoffHz / sampleRate
        );
    }

    float process(float target) {
        state += coefficient * (target - state);
        return state;
    }
};

}  // namespace aln_fold
