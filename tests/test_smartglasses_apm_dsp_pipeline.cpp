#include <gtest/gtest.h>
#include "apm/smartglasses_apm.hpp"
#include <cmath>
#include <vector>
#include <chrono>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class SmartglassesApmDspPipelineTest : public ::testing::Test {
protected:
    apm::SmartglassesApmConfig config;
    apm::SmartglassesAPM engine{config};
};

TEST_F(SmartglassesApmDspPipelineTest, Sub10msLatencyBenchmark) {
#ifndef APM_ENABLE_PERFORMANCE_TESTS
    GTEST_SKIP() << "Configure an uninstrumented Release build with BUILD_BENCHMARKS=ON to check the original latency budget";
#else
    std::vector<float> mic_left(160, 0.2f);
    std::vector<float> mic_right(160, 0.2f);
    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    // Warmup
    engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    const int iterations = 1000;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        engine.process_frame(mic_left, mic_right, out_enhanced, metadata);
    }
    auto end = std::chrono::steady_clock::now();

    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_ms_per_frame = (static_cast<double>(total_us) / iterations) / 1000.0;

    // Must strictly satisfy sub-10 ms per 10 ms frame requirement (target: < 0.5 ms on modern CPU)
    EXPECT_LT(avg_ms_per_frame, 1.0);
#endif
}

TEST_F(SmartglassesApmDspPipelineTest, PCM16InterleavedInterface) {
    std::vector<int16_t> pcm_interleaved(320); // 160 Left + 160 Right
    for (int i = 0; i < 160; ++i) {
        int16_t s = static_cast<int16_t>(12000.0f * std::sin(2.0f * static_cast<float>(M_PI) * 800.0f * i / 16000.0f));
        pcm_interleaved[2 * i] = s;     // Left
        pcm_interleaved[2 * i + 1] = s; // Right (In-phase mouth speech)
    }

    std::vector<int16_t> pcm_out_mono(160, 0);
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame_pcm16(
        pcm_interleaved.data(),
        pcm_interleaved.size(),
        pcm_out_mono.data(),
        metadata
    );

    EXPECT_TRUE(ok);
    EXPECT_TRUE(metadata.is_speech_active);
    EXPECT_GT(metadata.speech_confidence, 0.4f);
    EXPECT_TRUE(metadata.is_wake_word_window);
}

TEST_F(SmartglassesApmDspPipelineTest, ResetStateBuffers) {
    std::vector<float> mic_left(160, 0.5f);
    std::vector<float> mic_right(160, 0.5f);
    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    engine.process_frame(mic_left, mic_right, out_enhanced, metadata);
    engine.reset();

    // After reset, processing silent frame should produce near zero without residual state
    std::vector<float> silent_left(160, 0.0f);
    std::vector<float> silent_right(160, 0.0f);

    bool ok = engine.process_frame(silent_left, silent_right, out_enhanced, metadata);
    EXPECT_TRUE(ok);

    for (float sample : out_enhanced) {
        EXPECT_NEAR(sample, 0.0f, 1e-4f);
    }
}

TEST_F(SmartglassesApmDspPipelineTest, RobustnessZeroedSilentInput) {
    std::vector<float> mic_left(160, 0.0f);
    std::vector<float> mic_right(160, 0.0f);
    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);
    EXPECT_EQ(out_enhanced.size(), 160u);

    for (float sample : out_enhanced) {
        EXPECT_FALSE(std::isnan(sample));
        EXPECT_FALSE(std::isinf(sample));
        EXPECT_NEAR(sample, 0.0f, 1e-5f);
    }

    EXPECT_FALSE(metadata.is_speech_active);
    EXPECT_FALSE(metadata.is_wake_word_window);
}

TEST_F(SmartglassesApmDspPipelineTest, RobustnessNanAndInfInputSanitization) {
    std::vector<float> mic_left(160, 0.1f);
    std::vector<float> mic_right(160, 0.1f);

    // Inject NaNs and Infs into inputs
    mic_left[10] = std::numeric_limits<float>::quiet_NaN();
    mic_left[20] = std::numeric_limits<float>::infinity();
    mic_right[30] = -std::numeric_limits<float>::infinity();

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);
    EXPECT_EQ(out_enhanced.size(), 160u);

    for (float sample : out_enhanced) {
        EXPECT_FALSE(std::isnan(sample));
        EXPECT_FALSE(std::isinf(sample));
    }
}

TEST_F(SmartglassesApmDspPipelineTest, RobustnessExtremeAmplitudeClamping) {
    std::vector<float> mic_left(160, 1000.0f); // Extreme overdrive
    std::vector<float> mic_right(160, -1000.0f);

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);
    for (float sample : out_enhanced) {
        EXPECT_FALSE(std::isnan(sample));
        EXPECT_LE(sample, 1.0f);
        EXPECT_GE(sample, -1.0f);
    }
}

TEST_F(SmartglassesApmDspPipelineTest, RobustnessInvalidBufferLengthHandling) {
    std::vector<float> short_left(50, 0.1f);
    std::vector<float> valid_right(160, 0.1f);

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(short_left, valid_right, out_enhanced, metadata);

    // Mismatched or undersized input buffer must safely fail
    EXPECT_FALSE(ok);
    EXPECT_FALSE(metadata.is_speech_active);
}

TEST_F(SmartglassesApmDspPipelineTest, RobustnessSuddenAudioTransients) {
    std::vector<float> mic_left(160, 0.0f);
    std::vector<float> mic_right(160, 0.0f);

    // Sudden 1-sample impulse spike (transient)
    mic_left[80] = 0.9f;
    mic_right[80] = 0.9f;

    std::vector<float> out_enhanced;
    apm::SmartglassesFrameMetadata metadata;

    bool ok = engine.process_frame(mic_left, mic_right, out_enhanced, metadata);

    EXPECT_TRUE(ok);
    for (float sample : out_enhanced) {
        EXPECT_FALSE(std::isnan(sample));
        EXPECT_FALSE(std::isinf(sample));
    }
}
