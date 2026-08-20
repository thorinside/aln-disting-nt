// Selectable learned circuit distortions for disting NT API v13.

#include <distingnt/api.h>
#include <new>

#include "aln_distortion_core.h"

namespace {

enum Parameter {
    kParamInput,
    kParamOutput,
    kParamOutputMode,
    kParamCircuit,
    kParamDrive,
    kParamBias,
    kParamMix,
    kParamLevel,
    kParamLowPass,
    kParameterCount,
};

static char const* const kCircuitNames[] = {
    "Silicon Soft",
    "Germanium",
    "LED Clip",
    "Asymmetric",
    "Op-amp Hard",
    "BJT Saturation",
    "CMOS Inverter",
    "Full-wave",
};
static char const* const kLowPassNames[] = {"Off", "On"};

static const _NT_parameter kParameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Input", 1, 1)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Output", 1, 13)
    {
        .name = "Circuit",
        .min = 0,
        .max = 7,
        .def = 0,
        .unit = kNT_unitEnum,
        .scaling = 0,
        .enumStrings = kCircuitNames,
    },
    {
        .name = "Drive",
        .min = 0,
        .max = 800,
        .def = 100,
        .unit = kNT_unitPercent,
        .scaling = 0,
        .enumStrings = NULL,
    },
    {
        .name = "Bias",
        .min = -100,
        .max = 100,
        .def = 0,
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
        .name = "Level",
        .min = 0,
        .max = 200,
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
};

static_assert(
    sizeof(kParameters) / sizeof(kParameters[0]) == kParameterCount,
    "parameter mismatch"
);
static_assert(
    sizeof(kCircuitNames) / sizeof(kCircuitNames[0])
        == aln_distortion::kDistortionModelCount,
    "circuit enum mismatch"
);

static const uint8_t kMainPageParameters[] = {
    kParamInput,
    kParamOutput,
    kParamOutputMode,
    kParamCircuit,
    kParamDrive,
    kParamBias,
    kParamMix,
    kParamLevel,
    kParamLowPass,
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
    aln_distortion::ControlSmoother drive;
    aln_distortion::ControlSmoother bias;
    aln_distortion::ControlSmoother mix;
    aln_distortion::ControlSmoother level;
    aln_distortion::DistortionBank bank;
    aln_distortion::OnePole filter;
    aln_distortion::BoundedDcBlocker outputDcBlocker;
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
    const float sampleRate = static_cast<float>(NT_globals.sampleRate);
    algorithm->drive.initialise(sampleRate);
    algorithm->bias.initialise(sampleRate);
    algorithm->mix.initialise(sampleRate);
    algorithm->level.initialise(sampleRate);
    algorithm->bank.initialise(sampleRate);
    algorithm->filter.setCutoff(6000.0f, sampleRate);
    algorithm->outputDcBlocker.initialise(sampleRate);
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
    const uint8_t circuit = static_cast<uint8_t>(aln_distortion::clamp(
        static_cast<float>(algorithm->v[kParamCircuit]),
        0.0f,
        static_cast<float>(aln_distortion::kDistortionModelCount - 1u)
    ));
    const float targetDrive = algorithm->v[kParamDrive] * 0.01f;
    const float targetBias = algorithm->v[kParamBias] * 0.01f;
    const float targetMix = algorithm->v[kParamMix] * 0.01f;
    const float targetLevel = algorithm->v[kParamLevel] * 0.01f;
    const bool lowPassEnabled = algorithm->v[kParamLowPass] != 0;

    for (int frame = 0; frame < numFrames; ++frame) {
        const float dry = input[frame];
        const float drive = algorithm->drive.process(targetDrive);
        const float bias = algorithm->bias.process(targetBias);
        const float mix = algorithm->mix.process(targetMix);
        const float level = algorithm->level.process(targetLevel);
        float wet = algorithm->bank.process(
            aln_distortion::clamp(dry, -5.0f, 5.0f),
            drive,
            bias,
            circuit
        );
        const float filtered = algorithm->filter.process(wet);
        if (lowPassEnabled) wet = filtered;
        const float mixed = dry + mix * (wet - dry);
        const float limitedContribution = aln_distortion::clamp(
            level * mixed,
            -5.0f,
            5.0f
        );
        const float dcBlocked = algorithm->outputDcBlocker.process(
            limitedContribution,
            5.0f
        );
        // Keep the true bypass endpoints exact, then bring the sub-audio
        // rejector fully in over the first quarter of the wet-excitation range.
        const float dcRemovalAmount = aln_distortion::clamp(
            4.0f * mix * drive,
            0.0f,
            1.0f
        );
        const float contribution = limitedContribution + dcRemovalAmount * (
            dcBlocked - limitedContribution
        );
        output[frame] = replace ? contribution : output[frame] + contribution;
    }
}

static const _NT_factory kFactory = {
    .guid = NT_MULTICHAR('T', 'h', 'D', 'b'),
    .name = "ALN Distort",
    .description = "Eight learned analog-circuit distortions",
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
