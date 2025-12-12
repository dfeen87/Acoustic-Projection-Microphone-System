// ============================================================================
// APM Core Tests
// Unit tests for the core APM system components
// ============================================================================

#include "apm/apm_system.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace apm;

// ============================================================================
// AudioFrame Tests
// ============================================================================

TEST(AudioFrameTest, Construction) {
    AudioFrame frame(960, 48000, 1);
    
    EXPECT_EQ(frame.frame_count(), 960);
    EXPECT_EQ(frame.sample_rate(), 48000);
    EXPECT_EQ(frame.channels(), 1);
}

TEST(AudioFrameTest, SampleAccess) {
    AudioFrame frame(100, 48000, 1);
    auto samples = frame.samples();
    
    EXPECT_EQ(samples.size(), 100);
    
    // Write some data
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0f * M_PI * i / 100.0f);
    }
    
    // Read it back
    auto const_samples = const_cast<const AudioFrame&>(frame).samples();
    EXPECT_FLOAT_EQ(const_samples[0], 0.0f);
    EXPECT_NEAR(const_samples[25], 1.0f, 0.01f);
}

TEST(AudioFrameTest, ComputeMetadata) {
    AudioFrame frame(100, 48000, 1);
    auto samples = frame.samples();
    
    // Silent frame
    frame.compute_metadata();
    EXPECT_LT(frame.metadata().peak_db, -80.0f);
    EXPECT_FALSE(frame.metadata().clipping);
    
    // Full-scale sine
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0f * M_PI * i / 100.0f);
    }
    frame.compute_metadata();
    EXPECT_NEAR(frame.metadata().peak_db, 0.0f, 1.0f);
    EXPECT_FALSE(frame.metadata().clipping);
    
    // Clipping signal
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 1.5f;
    }
    frame.compute_metadata();
    EXPECT_TRUE(frame.metadata().clipping);
}

TEST(AudioFrameTest, ChannelExtraction) {
    AudioFrame frame(100, 48000, 2); // Stereo
    auto samples = frame.samples();
    
    // Fill left channel with 1.0, right with 0.5
    for (size_t i = 0; i < 100; ++i) {
        samples[i * 2] = 1.0f;     // Left
        samples[i * 2 + 1] = 0.5f; // Right
    }
    
    auto left = frame.channel(0);
    auto right = frame.channel(1);
    
    EXPECT_EQ(left.size(), 100);
    EXPECT_EQ(right.size(), 100);
    EXPECT_FLOAT_EQ(left[0], 1.0f);
    EXPECT_FLOAT_EQ(right[0], 0.5f);
}

// ============================================================================
// BeamformingEngine Tests
// ============================================================================

TEST(BeamformingTest, Construction) {
    BeamformingEngine bf(4, 0.012f);
    // Just ensure it constructs without error
    SUCCEED();
}

TEST(BeamformingTest, DelayAndSum) {
    BeamformingEngine bf(4, 0.012f);
    
    std::vector<AudioFrame> mic_array;
    for (int i = 0; i < 4; ++i) {
        AudioFrame frame(960, 48000, 1);
        auto samples = frame.samples();
        
        // Fill with sine wave
        for (size_t j = 0; j < samples.size(); ++j) {
            samples[j] = std::sin(2.0f * M_PI * j / 48.0f);
        }
        
        mic_array.push_back(std::move(frame));
    }
    
    auto output = bf.delay_and_sum(mic_array, 0.0f, 0.0f);
    
    EXPECT_EQ(output.frame_count(), 960);
    EXPECT_EQ(output.sample_rate(), 48000);
}

TEST(BeamformingTest, EmptyArray) {
    BeamformingEngine bf(4, 0.012f);
    
    std::vector<AudioFrame> empty_array;
    auto output = bf.delay_and_sum(empty_array, 0.0f, 0.0f);
    
    EXPECT_EQ(output.frame_count(), 0);
}

// ============================================================================
// NoiseSuppressionEngine Tests
// ============================================================================

TEST(NoiseSuppressionTest, Construction) {
    NoiseSuppressionEngine ns;
    SUCCEED();
}

TEST(NoiseSuppressionTest, Suppress) {
    NoiseSuppressionEngine ns;
    
    AudioFrame noisy(960, 48000, 1);
    auto samples = noisy.samples();
    
    // Add noise
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.1f * (rand() / static_cast<float>(RAND_MAX) - 0.5f);
    }
    
    auto clean = ns.suppress(noisy);
    
    EXPECT_EQ(clean.frame_count(), 960);
    // After suppression, energy should be reduced (or similar)
}

TEST(NoiseSuppressionTest, ResetState) {
    NoiseSuppressionEngine ns;
    
    AudioFrame frame(960, 48000, 1);
    ns.suppress(frame); // Process once
    ns.reset_state();   // Reset
    ns.suppress(frame); // Process again
    
    SUCCEED();
}

// ============================================================================
// EchoCancellationEngine Tests
// ============================================================================

TEST(EchoCancellationTest, Construction) {
    EchoCancellationEngine aec(2048);
    SUCCEED();
}

TEST(EchoCancellationTest, CancelEcho) {
    EchoCancellationEngine aec(2048);
    
    AudioFrame mic(960, 48000, 1);
    AudioFrame ref(960, 48000, 1);
    
    auto mic_samples = mic.samples();
    auto ref_samples = ref.samples();
    
    // Simulate echo: mic contains delayed version of ref
    for (size_t i = 0; i < 900; ++i) {
        ref_samples[i] = std::sin(2.0f * M_PI * i / 48.0f);
        mic_samples[i + 60] = 0.5f * ref_samples[i]; // Echo with 60 sample delay
    }
    
    auto output = aec.cancel_echo(mic, ref);
    
    EXPECT_EQ(output.frame_count(), 960);
}

TEST(EchoCancellationTest, DoubleTalkDetection) {
    EchoCancellationEngine aec;
    
    AudioFrame mic(960, 48000, 1);
    AudioFrame ref(960, 48000, 1);
    
    auto mic_samples = mic.samples();
    auto ref_samples = ref.samples();
    
    // Mic has strong signal, ref is weak
    for (size_t i = 0; i < mic_samples.size(); ++i) {
        mic_samples[i] = 0.8f * std::sin(2.0f * M_PI * i / 48.0f);
        ref_samples[i] = 0.1f * std::sin(2.0f * M_PI * i / 48.0f);
    }
    
    EXPECT_TRUE(aec.detect_double_talk(mic, ref));
    
    // Both signals similar
    for (size_t i = 0; i < mic_samples.size(); ++i) {
        mic_samples[i] = 0.5f * std::sin(2.0f * M_PI * i / 48.0f);
        ref_samples[i] = 0.5f * std::sin(2.0f * M_PI * i / 48.0f);
    }
    
    EXPECT_FALSE(aec.detect_double_talk(mic, ref));
}

// ============================================================================
// VoiceActivityDetector Tests
// ============================================================================

TEST(VADTest, SilenceDetection) {
    VoiceActivityDetector vad;
    
    AudioFrame silent(960, 48000, 1);
    // Leave it silent (zeros)
    
    auto result = vad.detect(silent);
    
    EXPECT_FALSE(result.speech_detected);
    EXPECT_LT(result.confidence, 0.1f);
}

TEST(VADTest, SpeechDetection) {
    VoiceActivityDetector vad;
    
    AudioFrame speech(960, 48000, 1);
    auto samples = speech.samples();
    
    // Simulate speech-like signal
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.3f * std::sin(2.0f * M_PI * i / 10.0f) // Low freq
                   + 0.1f * std::sin(2.0f * M_PI * i / 3.0f);  // Higher freq
    }
    
    auto result = vad.detect(speech);
    
    EXPECT_TRUE(result.speech_detected);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST(VADTest, AdaptiveThreshold) {
    VoiceActivityDetector vad;
    
    vad.adapt_threshold(-40.0f);
    
    AudioFrame frame(960, 48000, 1);
    auto samples = frame.samples();
    
    // Low energy signal
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = 0.01f * std::sin(2.0f * M_PI * i / 48.0f);
    }
    
    auto result = vad.detect(frame);
    // After adapting to low noise, should detect low signals
}

// ============================================================================
// APMSystem Integration Tests
// ============================================================================

TEST(APMSystemTest, Construction) {
    APMSystem::Config config;
    config.num_microphones = 4;
    config.num_speakers = 3;
    
    APMSystem apm(config);
    SUCCEED();
}

TEST(APMSystemTest, FullPipeline) {
    APMSystem::Config config;
    config.num_microphones = 4;
    config.num_speakers = 3;
    config.source_language = "en-US";
    config.target_language = "es-ES";
    
    APMSystem apm(config);
    
    // Create mock input
    std::vector<AudioFrame> mic_array;
    for (int i = 0; i < 4; ++i) {
        AudioFrame frame(960, 48000, 1);
        auto samples = frame.samples();
        
        // Fill with test signal
        for (size_t j = 0; j < samples.size(); ++j) {
            samples[j] = 0.1f * std::sin(2.0f * M_PI * j / 48.0f);
        }
        
        mic_array.push_back(std::move(frame));
    }
    
    AudioFrame speaker_ref(960, 48000, 1);
    
    // Process
    auto outputs = apm.process(mic_array, speaker_ref, 0.0f);
    
    // Should produce output (or empty if no speech detected)
    EXPECT_TRUE(outputs.empty() || outputs.size() == 3);
}

TEST(APMSystemTest, AsyncProcessing) {
    APMSystem::Config config;
    APMSystem apm(config);
    
    std::vector<AudioFrame> mic_array;
    for (int i = 0; i < 4; ++i) {
        mic_array.emplace_back(960, 48000, 1);
    }
    
    AudioFrame speaker_ref(960, 48000, 1);
    
    auto future = apm.process_async(mic_array, speaker_ref, 0.0f);
    auto outputs = future.get();
    
    SUCCEED();
}

TEST(APMSystemTest, ResetAll) {
    APMSystem::Config config;
    APMSystem apm(config);
    
    apm.reset_all();
    SUCCEED();
}

// ============================================================================
// DirectionalProjector Tests
// ============================================================================

TEST(ProjectorTest, CreateProjectionSignals) {
    DirectionalProjector projector(3, 0.015f);
    
    AudioFrame source(960, 48000, 1);
    auto samples = source.samples();
    
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0f * M_PI * i / 48.0f);
    }
    
    auto outputs = projector.create_projection_signals(source, 0.0f, 1.5f);
    
    EXPECT_EQ(outputs.size(), 3);
    for (const auto& output : outputs) {
        EXPECT_EQ(output.frame_count(), 960);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
