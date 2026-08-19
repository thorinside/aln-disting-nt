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

inline float evaluateFoldDTree(float input, float fold) {
    return evaluateFoldDTree(input, fold, kSixteenFoldModel);
}

struct FoldAntialiaser {
    float previousInput;
    bool initialised;

    FoldAntialiaser() : previousInput(0.0f), initialised(false) {}

    float process(float input, float fold, uint8_t modelIndex) {
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
            raw = evaluateRawFoldDTree(model, driven);
            initialised = true;
        } else {
            const uint16_t currentLeaf = foldLeafIndex(model, driven);
            const uint16_t previousLeaf = foldLeafIndex(model, previousDriven);
            if (currentLeaf == previousLeaf || fabsf(delta) <= 1.0e-5f) {
                raw = 0.5f * (
                    evaluateRawFoldLeaf(model, driven, currentLeaf)
                    + evaluateRawFoldLeaf(model, previousDriven, previousLeaf)
                );
            } else {
                raw = (
                    evaluateRawFoldAntiderivative(model, driven, currentLeaf)
                    - evaluateRawFoldAntiderivative(model, previousDriven, previousLeaf)
                ) / delta;
            }
        }
        previousInput = input;
        const float folded = model.outputGain * raw;
        return input + amount * (folded - input);
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

    float process(float input, float fold, uint8_t selectedModel) {
        const uint8_t modelIndex = clampModelIndex(selectedModel);
        const float target = modelIndex == kBuchla259Model ? 0.0f : 1.0f;
        if (!initialised) {
            blend = target;
            initialised = true;
        } else {
            blend += clamp(target - blend, -maximumStep, maximumStep);
        }
        const float buchla = antialiasers[kBuchla259Model].process(
            input, fold, kBuchla259Model
        );
        const float sixteenFold = antialiasers[kSixteenFoldModel].process(
            input, fold, kSixteenFoldModel
        );
        return buchla + blend * (sixteenFold - buchla);
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

}  // namespace aln_fold
