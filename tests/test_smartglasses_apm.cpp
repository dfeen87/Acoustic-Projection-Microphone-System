#include <gtest/gtest.h>
#include "apm/smartglasses_apm.hpp"
#include <cmath>
#include <vector>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class SmartglassesApmTest : public ::testing::Test {
protected:
    apm::SmartglassesApmConfig config;
    apm::SmartglassesAPM engine{config};
};

TEST_F(SmartglassesApmTest, ConfigurationDefaults) {
    EXPECT_EQ(engine.get_config().sample_rate, 16000);
    EXPECT_EQ(engine.get_config().frame_size, 160);
    EXPECT_EQ(engine.get_config().fft_size, 256);
    EXPECT_FLOAT_EQ(engine.get_config().mic_baseline_m, 0.15f);
}

TEST_F(SmartglassesApmTest, ProcessInPhaseMouthSpeech) {
    // Simulate target speech from mouth (in-phase at left & right temple mics)
    std::vector<float> mic_left(160);
    std::vector<float> mic_right(160);

    for (int i = 0; i < 160; ++i) {
        float val = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * i / 16000.0f); // 1 kHz tone
        mic_left[i] = val;
        mic_right[i] = val;
    }

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool result = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(result);
    EXPECT_EQ(out_enhanced.size(), 160u);
    EXPECT_GT(metadata.estimated_snr_db, 10.0f);
    EXPECT_TRUE(metadata.is_speech_active);
    EXPECT_GT(metadata.speech_confidence, 0.5f);
}

TEST_F(SmartglassesApmTest, SuppressOutOfPhaseLateralNoise) {
    // Simulate off-axis lateral noise (180 degrees phase shift / anti-phase between left & right)
    std::vector<float> mic_left(160);
    std::vector<float> mic_right(160);

    for (int i = 0; i < 160; ++i) {
        float t = static_cast<float>(i) / 16000.0f;
        float val = 0.5f * std::sin(2.0f * static_cast<float>(M_PI) * 2000.0f * t);
        mic_left[i] = val;
        mic_right[i] = -val; // 180 deg anti-phase off-axis noise
    }

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool result = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(result);
    // Lower speech confidence estimated for anti-phase noise
    EXPECT_LT(metadata.speech_confidence, 0.35f);
    EXPECT_FALSE(metadata.is_speech_active);
}

TEST_F(SmartglassesApmTest, PCM16InterfaceCompliance) {
    std::vector<int16_t> pcm_interleaved(320); // 160 L + 160 R
    for (int i = 0; i < 160; ++i) {
        int16_t val = static_cast<int16_t>(10000.0f * std::sin(2.0f * static_cast<float>(M_PI) * 500.0f * i / 16000.0f));
        pcm_interleaved[2 * i] = val;     // Left
        pcm_interleaved[2 * i + 1] = val; // Right (In-phase)
    }

    std::vector<int16_t> pcm_out_mono(160);
    apm::SmartglassesFrameMetadata metadata;

    bool success = engine.process_frame_pcm16(
        pcm_interleaved.data(),
        pcm_interleaved.size(),
        pcm_out_mono.data(),
        metadata
    );

    EXPECT_TRUE(success);
    EXPECT_TRUE(metadata.is_speech_active);
}

TEST_F(SmartglassesApmTest, RealTimeFrameProcessingLatency) {
    std::vector<float> mic_left(160, 0.1f);
    std::vector<float> mic_right(160, 0.1f);
    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        engine.process_frame(mic_left, mic_right, out_enhanced, metadata);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_ms_per_frame = (static_cast<double>(total_us) / 100.0) / 1000.0;

    // Must easily beat the 10ms real-time constraint (typically < 1 ms on standard CPUs)
    EXPECT_LT(avg_ms_per_frame, 2.0);
}
