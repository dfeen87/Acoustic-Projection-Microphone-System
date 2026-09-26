#include <gtest/gtest.h>
#include "apm/smartglasses_apm.hpp"
#include <cmath>
#include <vector>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class SmartglassesApmProjectionTest : public ::testing::Test {
protected:
    apm::SmartglassesApmConfig config;
};

TEST_F(SmartglassesApmProjectionTest, InPhaseMouthSpeechPreservation) {
    // In-phase speech from mouth vector (zero phase difference)
    apm::SmartglassesAPM engine{config};

    std::vector<float> mic_left(160);
    std::vector<float> mic_right(160);

    for (int i = 0; i < 160; ++i) {
        float sample = 0.6f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * i / 16000.0f);
        mic_left[i] = sample;
        mic_right[i] = sample;
    }

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);
    EXPECT_EQ(out_enhanced.size(), 160u);

    // Energy preservation check: output energy should be close to input energy
    float input_energy = 0.0f;
    float output_energy = 0.0f;
    for (int i = 0; i < 160; ++i) {
        input_energy += mic_left[i] * mic_left[i];
        output_energy += out_enhanced[i] * out_enhanced[i];
    }

    EXPECT_GT(output_energy, 0.3f * input_energy);
    EXPECT_GT(metadata.estimated_snr_db, 10.0f);
    EXPECT_TRUE(metadata.is_speech_active);
    EXPECT_GT(metadata.speech_confidence, 0.5f);
}

TEST_F(SmartglassesApmProjectionTest, OffAxisLateralNoiseSuppression) {
    // Off-axis lateral noise (anti-phase L = -R)
    apm::SmartglassesAPM engine{config};

    std::vector<float> mic_left(160);
    std::vector<float> mic_right(160);

    for (int i = 0; i < 160; ++i) {
        float sample = 0.6f * std::sin(2.0f * static_cast<float>(M_PI) * 2000.0f * i / 16000.0f);
        mic_left[i] = sample;
        mic_right[i] = -sample; // 180 deg anti-phase off-axis
    }

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);

    float input_energy = 0.0f;
    float output_energy = 0.0f;
    for (int i = 0; i < 160; ++i) {
        input_energy += mic_left[i] * mic_left[i];
        output_energy += out_enhanced[i] * out_enhanced[i];
    }

    // Strong spatial suppression expected for anti-phase noise
    EXPECT_LT(output_energy, 0.25f * input_energy);
    EXPECT_FALSE(metadata.is_speech_active);
    EXPECT_LT(metadata.speech_confidence, 0.35f);
}

TEST_F(SmartglassesApmProjectionTest, SpatialExponentScaling) {
    apm::SmartglassesApmConfig config_mild;
    config_mild.spatial_exp = 0.5f;

    apm::SmartglassesApmConfig config_aggressive;
    config_aggressive.spatial_exp = 3.0f;

    apm::SmartglassesAPM engine_mild{config_mild};
    apm::SmartglassesAPM engine_aggr{config_aggressive};

    std::vector<float> mic_left(160);
    std::vector<float> mic_right(160);

    // Partial off-axis noise (90 deg phase shift)
    for (int i = 0; i < 160; ++i) {
        mic_left[i] = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 1500.0f * i / 16000.0f);
        mic_right[i] = 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * 1500.0f * i / 16000.0f);
    }

    std::vector<float> out_mild, out_aggr;
    apm::SmartglassesFrameMetadata meta_mild, meta_aggr;

    engine_mild.process_frame(mic_left, mic_right, out_mild, meta_mild);
    engine_aggr.process_frame(mic_left, mic_right, out_aggr, meta_aggr);

    float energy_mild = 0.0f;
    float energy_aggr = 0.0f;
    for (int i = 0; i < 160; ++i) {
        energy_mild += out_mild[i] * out_mild[i];
        energy_aggr += out_aggr[i] * out_aggr[i];
    }

    // Higher spatial exponent yields stronger suppression of off-axis components
    EXPECT_LE(energy_aggr, energy_mild + 1e-4f);
}

TEST_F(SmartglassesApmProjectionTest, BaselineConfigurationFlexibility) {
    apm::SmartglassesApmConfig cfg_14cm;
    cfg_14cm.mic_baseline_m = 0.14f;

    apm::SmartglassesApmConfig cfg_16cm;
    cfg_16cm.mic_baseline_m = 0.16f;

    apm::SmartglassesAPM apm_14{cfg_14cm};
    apm::SmartglassesAPM apm_16{cfg_16cm};

    EXPECT_FLOAT_EQ(apm_14.get_config().mic_baseline_m, 0.14f);
    EXPECT_FLOAT_EQ(apm_16.get_config().mic_baseline_m, 0.16f);
}
