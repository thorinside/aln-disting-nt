// Selectable learned D-tree wavefolders for disting NT API v13.

#include <distingnt/api.h>
#include <new>

#include "aln_fold_core.h"

namespace {

enum Parameter {
    kParamInput,
    kParamOutput,
    kParamOutputMode,
    kParamFold,
    kParamMix,
    kParamLowPass,
    kParamModel,
    kParameterCount,
};

static char const* const kLowPassNames[] = {"Off", "On"};
static char const* const kModelNames[] = {"Buchla 259", "16-fold"};

static const _NT_parameter kParameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Input", 1, 1)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    {
        .name = "Fold",
        .min = 0,
        .max = 100,
        .def = 100,
        .unit = kNT_unitPercent,
        .scaling = 0,
        .enumStrings = NULL,
    },
    {
        .name = "Mix",
        .min = 0,
        .max = 100,
        .def = 100,
        .unit = kNT_unitPercent,
        .scaling = 0,
        .enumStrings = NULL,
    },
    {
        .name = "Low-pass",
        .min = 0,
        .max = 1,
        .def = 0,
        .unit = kNT_unitEnum,
        .scaling = 0,
        .enumStrings = kLowPassNames,
    },
    {
        .name = "Model",
        .min = 0,
        .max = 1,
        .def = 1,
        .unit = kNT_unitEnum,
        .scaling = 0,
        .enumStrings = kModelNames,
    },
};

static_assert(
    sizeof(kParameters) / sizeof(kParameters[0]) == kParameterCount,
    "parameter mismatch"
);

static const uint8_t kMainPageParameters[] = {
    kParamInput,
    kParamOutput,
    kParamOutputMode,
    kParamFold,
    kParamMix,
    kParamLowPass,
    kParamModel,
};

static const _NT_parameterPage kParameterPage[] = {
    {
        .name = "Main",
        .numParams = sizeof(kMainPageParameters) / sizeof(kMainPageParameters[0]),
        .group = 0,
        .unused = {0, 0},
        .params = kMainPageParameters,
    },
};

static const _NT_parameterPages kParameterPages = {
    .numPages = sizeof(kParameterPage) / sizeof(kParameterPage[0]),
    .pages = kParameterPage,
};

struct Algorithm : public _NT_algorithm {
    aln_fold::FoldSmoother fold;
    aln_fold::ModelCrossfader model;
    aln_fold::OnePole filter;
};

void calculateRequirements(
    _NT_algorithmRequirements& requirements,
    const int32_t*
) {
    requirements.numParameters = kParameterCount;
    requirements.sram = sizeof(Algorithm);
    requirements.dram = 0;
    requirements.dtc = 0;
    requirements.itc = 0;
}

_NT_algorithm* construct(
    const _NT_algorithmMemoryPtrs& pointers,
    const _NT_algorithmRequirements&,
    const int32_t*
) {
    Algorithm* algorithm = new (pointers.sram) Algorithm();
    algorithm->parameters = kParameters;
    algorithm->parameterPages = &kParameterPages;
    algorithm->fold.initialise(static_cast<float>(NT_globals.sampleRate));
    algorithm->model.initialise(static_cast<float>(NT_globals.sampleRate));
    algorithm->filter.setCutoff(1326.0f, static_cast<float>(NT_globals.sampleRate));
    return algorithm;
}

void step(_NT_algorithm* self, float* busFrames, int numFramesBy4) {
    Algorithm* algorithm = static_cast<Algorithm*>(self);
    const int numFrames = numFramesBy4 * 4;
    const int inputBus = algorithm->v[kParamInput] - 1;
    const int outputBus = algorithm->v[kParamOutput] - 1;
    const float* input = busFrames + inputBus * numFrames;
    float* output = busFrames + outputBus * numFrames;
    const bool replace = algorithm->v[kParamOutputMode] != 0;
    const float targetFold = algorithm->v[kParamFold] * 0.01f;
    const float mix = algorithm->v[kParamMix] * 0.01f;
    const bool lowPassEnabled = algorithm->v[kParamLowPass] != 0;
    const uint8_t selectedModel = algorithm->v[kParamModel] == 0
        ? aln_fold::kBuchla259Model
        : aln_fold::kSixteenFoldModel;

    for (int frame = 0; frame < numFrames; ++frame) {
        const float dry = input[frame];
        const float modelInput = aln_fold::clamp(dry, -5.0f, 5.0f);
        const float fold = algorithm->fold.process(targetFold);
        float wet = algorithm->model.process(modelInput, fold, selectedModel);
        const float filtered = algorithm->filter.process(wet);
        if (lowPassEnabled) wet = filtered;
        const float contribution = dry + mix * (wet - dry);
        output[frame] = replace ? contribution : output[frame] + contribution;
    }
}

static const _NT_factory kFactory = {
    .guid = NT_MULTICHAR('A', 'f', 'T', '1'),
    .name = "ALN Fold Wavefolder",
    .description = "Selectable learned Buchla and 16-fold wavefolders",
    .numSpecifications = 0,
    .specifications = NULL,
    .calculateStaticRequirements = NULL,
    .initialise = NULL,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = NULL,
    .step = step,
    .draw = NULL,
    .midiRealtime = NULL,
    .midiMessage = NULL,
    .tags = kNT_tagEffect,
    .hasCustomUi = NULL,
    .customUi = NULL,
    .setupUi = NULL,
    .serialise = NULL,
    .deserialise = NULL,
    .midiSysEx = NULL,
    .parameterUiPrefix = NULL,
    .parameterString = NULL,
};

}  // namespace

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersionCurrent;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return data == 0 ? reinterpret_cast<uintptr_t>(&kFactory) : 0;
        default:
            return 0;
    }
}
