#include <distingnt/api.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

extern "C" const _NT_globals NT_globals = {
    48000u,
    128u,
    NULL,
    0u,
    0u,
    0u,
};

namespace {

const int kFramesPerBlock = 128;
const int kSampleRate = 48000;
const int kOutputOffset = 12 * kFramesPerBlock;
const float kPi = 3.14159265358979323846f;

static const char* const kStageNames[] = {
    "AX7 Data",
    "AX7 Hot",
    "Bias Memory",
};

struct Settings {
    int stage;
    int drivePercent;
    int biasPercent;
    int memoryPercent;
    int mixPercent;
    int levelPercent;
    int lowPass;
    int outputMode;
    float frequencyHz;
    float amplitudeV;
    float existingOutputV;
};

struct Metrics {
    double mean;
    double rms;
    double peak;
    double fundamentalRms;
    double residualRms;
    double thdnPercent;
    double maximumInputError;
    bool finite;
};

class PluginHarness {
public:
    PluginHarness()
        : factory_(reinterpret_cast<const _NT_factory*>(
              pluginEntry(kNT_selector_factoryInfo, 0)
          )), algorithm_(NULL),
          values_{1, 13, 1, 0, 100, 0, 50, 100, 100, 0},
          buses_(kNT_lastBus * kFramesPerBlock, 0.0f) {
        _NT_algorithmRequirements requirements = {};
        factory_->calculateRequirements(requirements, NULL);
        sram_.resize((requirements.sram + 7u) / 8u);
        _NT_algorithmMemoryPtrs pointers = {};
        pointers.sram = reinterpret_cast<uint8_t*>(sram_.data());
        algorithm_ = factory_->construct(pointers, requirements, NULL);
        algorithm_->v = values_;
        algorithm_->vIncludingCommon = values_;
    }

    Metrics measure(const Settings& settings) {
        values_[2] = static_cast<int16_t>(settings.outputMode);
        values_[3] = static_cast<int16_t>(settings.stage);
        values_[4] = static_cast<int16_t>(settings.drivePercent);
        values_[5] = static_cast<int16_t>(settings.biasPercent);
        values_[6] = static_cast<int16_t>(settings.memoryPercent);
        values_[7] = static_cast<int16_t>(settings.mixPercent);
        values_[8] = static_cast<int16_t>(settings.levelPercent);
        values_[9] = static_cast<int16_t>(settings.lowPass);

        const int settleBlocks = kSampleRate / kFramesPerBlock;
        const int captureBlocks = kSampleRate / kFramesPerBlock;
        int64_t sampleIndex = 0;
        for (int block = 0; block < settleBlocks; ++block)
            processBlock(settings, sampleIndex);

        double sum = 0.0;
        double sumSquares = 0.0;
        double sineProjection = 0.0;
        double cosineProjection = 0.0;
        double peak = 0.0;
        double maximumInputError = 0.0;
        bool finite = true;
        const int captureSamples = captureBlocks * kFramesPerBlock;
        for (int block = 0; block < captureBlocks; ++block) {
            const int64_t blockStart = sampleIndex;
            processBlock(settings, sampleIndex);
            for (int frame = 0; frame < kFramesPerBlock; ++frame) {
                const double phase = 2.0 * kPi * settings.frequencyHz
                    * (blockStart + frame) / kSampleRate;
                const double input = buses_[frame];
                const double output = buses_[kOutputOffset + frame];
                finite = finite && std::isfinite(output);
                sum += output;
                sumSquares += output * output;
                sineProjection += output * sin(phase);
                cosineProjection += output * cos(phase);
                peak = std::max(peak, fabs(output));
                maximumInputError = std::max(
                    maximumInputError,
                    fabs(output - input)
                );
            }
        }

        const double mean = sum / captureSamples;
        const double rms = sqrt(sumSquares / captureSamples);
        const double sineAmplitude = 2.0 * sineProjection / captureSamples;
        const double cosineAmplitude = 2.0 * cosineProjection / captureSamples;
        const double fundamentalRms = sqrt(
            sineAmplitude * sineAmplitude + cosineAmplitude * cosineAmplitude
        ) / sqrt(2.0);
        const double acPower = std::max(0.0, rms * rms - mean * mean);
        const double residualPower = std::max(
            0.0,
            acPower - fundamentalRms * fundamentalRms
        );
        const double residualRms = sqrt(residualPower);
        const double thdnPercent = fundamentalRms > 1.0e-9
            ? 100.0 * residualRms / fundamentalRms
            : 0.0;
        Metrics metrics = {
            mean,
            rms,
            peak,
            fundamentalRms,
            residualRms,
            thdnPercent,
            maximumInputError,
            finite,
        };
        return metrics;
    }

private:
    void processBlock(const Settings& settings, int64_t& sampleIndex) {
        for (int frame = 0; frame < kFramesPerBlock; ++frame, ++sampleIndex) {
            const float phase = 2.0f * kPi * settings.frequencyHz
                * static_cast<float>(sampleIndex) / kSampleRate;
            buses_[frame] = settings.amplitudeV * sinf(phase);
            buses_[kOutputOffset + frame] = settings.existingOutputV;
        }
        factory_->step(algorithm_, buses_.data(), kFramesPerBlock / 4);
    }

    const _NT_factory* factory_;
    _NT_algorithm* algorithm_;
    int16_t values_[10];
    std::vector<uint64_t> sram_;
    std::vector<float> buses_;
};

Metrics measure(const Settings& settings) {
    PluginHarness harness;
    return harness.measure(settings);
}

void writeCsvRow(
    std::ofstream& csv,
    const char* path,
    const Settings& settings,
    const Metrics& metrics
) {
    csv << path << ',' << kStageNames[settings.stage] << ','
        << settings.drivePercent << ',' << settings.biasPercent << ','
        << settings.memoryPercent << ',' << settings.mixPercent << ','
        << settings.levelPercent << ',' << settings.lowPass << ','
        << settings.outputMode << ',' << settings.frequencyHz << ','
        << 2.0f * settings.amplitudeV << ',' << metrics.mean << ','
        << metrics.rms << ',' << metrics.peak << ','
        << metrics.fundamentalRms << ',' << metrics.residualRms << ','
        << metrics.thdnPercent << ',' << metrics.maximumInputError << '\n';
}

int failCase(
    const char* reason,
    const Settings& settings,
    const Metrics& metrics
) {
    std::cerr << "FAIL: " << reason << "; stage="
              << kStageNames[settings.stage]
              << ", drive=" << settings.drivePercent
              << "%, bias=" << settings.biasPercent
              << "%, memory=" << settings.memoryPercent
              << "%, mix=" << settings.mixPercent
              << "%, level=" << settings.levelPercent
              << "%, frequency=" << settings.frequencyHz
              << " Hz, mean=" << metrics.mean
              << " V, RMS=" << metrics.rms
              << " V, peak=" << metrics.peak << " V\n";
    return 1;
}

int checkWetPath(const Settings& settings, const Metrics& metrics) {
    if (!metrics.finite)
        return failCase("non-finite output", settings, metrics);
    if (fabs(metrics.mean) > 0.02)
        return failCase("residual DC exceeds 20 mV", settings, metrics);
    if (metrics.peak > 5.001)
        return failCase("Replace contribution exceeds 5 V", settings, metrics);
    if (metrics.rms < 0.02)
        return failCase("wet path is unexpectedly quiet", settings, metrics);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* csvPath = argc > 1
        ? argv[1]
        : "build/aln_saturation_audio_metrics.csv";
    std::ofstream csv(csvPath);
    if (!csv) {
        std::cerr << "FAIL: could not open metrics file " << csvPath << "\n";
        return 1;
    }
    csv << std::setprecision(10);
    csv << "path,stage,drive_percent,bias_percent,memory_percent,mix_percent,"
           "level_percent,low_pass,output_mode,frequency_hz,input_vpp,mean_v,"
           "rms_v,peak_v,fundamental_rms_v,residual_rms_v,thdn_percent,"
           "maximum_input_error_v\n";

    const int frequencies[] = {30, 100, 1000};
    const int drives[] = {25, 50, 100, 400, 800};
    const int biases[] = {-100, 0, 100};
    const int memories[] = {0, 50, 100};
    int measuredPaths = 0;
    double worstDc = 0.0;
    double maximumPeak = 0.0;
    for (int stage = 0; stage < 3; ++stage) {
        const int memoryCount = stage == 2 ? 3 : 1;
        for (int memoryIndex = 0; memoryIndex < memoryCount; ++memoryIndex) {
            for (int frequencyIndex = 0; frequencyIndex < 3; ++frequencyIndex) {
                for (int driveIndex = 0; driveIndex < 5; ++driveIndex) {
                    for (int biasIndex = 0; biasIndex < 3; ++biasIndex) {
                        Settings settings = {
                            stage,
                            drives[driveIndex],
                            biases[biasIndex],
                            memories[memoryIndex],
                            100,
                            100,
                            0,
                            1,
                            static_cast<float>(frequencies[frequencyIndex]),
                            5.0f,
                            0.0f,
                        };
                        const Metrics metrics = measure(settings);
                        writeCsvRow(csv, "wet-matrix", settings, metrics);
                        ++measuredPaths;
                        worstDc = std::max(worstDc, fabs(metrics.mean));
                        maximumPeak = std::max(maximumPeak, metrics.peak);
                        if (checkWetPath(settings, metrics) != 0)
                            return 1;
                    }
                }
            }
        }
    }

    Settings driveBypass = {
        2, 0, 100, 100, 100, 100, 0, 1, 1000.0f, 4.0f, 0.0f,
    };
    const Metrics driveBypassMetrics = measure(driveBypass);
    writeCsvRow(csv, "drive-zero-bypass", driveBypass, driveBypassMetrics);
    ++measuredPaths;
    if (driveBypassMetrics.maximumInputError > 1.0e-5)
        return failCase("Drive 0% is not transparent", driveBypass, driveBypassMetrics);

    Settings mixBypass = {
        2, 800, 100, 100, 0, 100, 0, 1, 1000.0f, 4.0f, 0.0f,
    };
    const Metrics mixBypassMetrics = measure(mixBypass);
    writeCsvRow(csv, "mix-zero-bypass", mixBypass, mixBypassMetrics);
    ++measuredPaths;
    if (mixBypassMetrics.maximumInputError > 1.0e-5)
        return failCase("Mix 0% is not transparent", mixBypass, mixBypassMetrics);

    Settings lowPassOff = {
        0, 100, 0, 0, 100, 100, 0, 1, 12000.0f, 2.0f, 0.0f,
    };
    Settings lowPassOn = lowPassOff;
    lowPassOn.lowPass = 1;
    const Metrics lowPassOffMetrics = measure(lowPassOff);
    const Metrics lowPassOnMetrics = measure(lowPassOn);
    writeCsvRow(csv, "low-pass-off", lowPassOff, lowPassOffMetrics);
    writeCsvRow(csv, "low-pass-on", lowPassOn, lowPassOnMetrics);
    measuredPaths += 2;
    if (lowPassOnMetrics.rms >= 0.9 * lowPassOffMetrics.rms)
        return failCase("Low-pass does not attenuate 12 kHz", lowPassOn, lowPassOnMetrics);

    Settings staticHot = {
        2, 100, 0, 0, 100, 100, 0, 1, 100.0f, 5.0f, 0.0f,
    };
    Settings dynamicHot = staticHot;
    dynamicHot.memoryPercent = 100;
    const Metrics staticHotMetrics = measure(staticHot);
    const Metrics dynamicHotMetrics = measure(dynamicHot);
    writeCsvRow(csv, "memory-zero", staticHot, staticHotMetrics);
    writeCsvRow(csv, "memory-full", dynamicHot, dynamicHotMetrics);
    measuredPaths += 2;
    if (fabs(dynamicHotMetrics.rms - staticHotMetrics.rms) < 0.05)
        return failCase("Bias Memory has no material audio effect", dynamicHot, dynamicHotMetrics);

    Settings maximumLevel = {
        2, 800, -100, 100, 100, 200, 0, 1, 100.0f, 5.0f, 0.0f,
    };
    const Metrics maximumLevelMetrics = measure(maximumLevel);
    writeCsvRow(csv, "level-200", maximumLevel, maximumLevelMetrics);
    ++measuredPaths;
    if (checkWetPath(maximumLevel, maximumLevelMetrics) != 0)
        return 1;

    Settings addPath = {
        2, 800, -100, 100, 100, 100, 0, 0, 100.0f, 5.0f, 1.25f,
    };
    const Metrics addMetrics = measure(addPath);
    writeCsvRow(csv, "add-mode", addPath, addMetrics);
    ++measuredPaths;
    if (fabs(addMetrics.mean - addPath.existingOutputV) > 0.02)
        return failCase("Add mode changes shared-bus DC", addPath, addMetrics);

    std::cout << "PASS: " << measuredPaths
              << " settled/captured ALN Saturation paths; worst full-wet DC "
              << worstDc << " V; maximum Replace peak " << maximumPeak
              << " V; memory RMS change "
              << fabs(dynamicHotMetrics.rms - staticHotMetrics.rms)
              << " V; 12 kHz low-pass RMS ratio "
              << lowPassOnMetrics.rms / lowPassOffMetrics.rms
              << "; metrics " << csvPath << "\n";
    return 0;
}
