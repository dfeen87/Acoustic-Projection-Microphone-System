#include <gtest/gtest.h>
#include "apm/apm_system.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace apm;

// ============================================================================
// FeedbackSuppressionEngine Tests
// ============================================================================

TEST(FeedbackSuppressionTest, Initialization) {
    FeedbackSuppressionEngine fse(3, 12.0f);
    fse.configure(48000, 3, 12.0f);
    SUCCEED();
}

TEST(FeedbackSuppressionTest, NotchFilterProcess) {
    FeedbackSuppressionEngine fse(1, 12.0f);
    fse.configure(48000, 1, 12.0f);

    // Create a frame with a strong 3000Hz sine wave (simulating feedback)
    AudioFrame frame(48000, 48000, 1);
    auto samples = frame.samples();
    for (size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0f * M_PI * 3000.0f * i / 48000.0f);
    }

    // Process enough frames to trigger detection and suppression
    AudioFrame output;
    for (int i = 0; i < 20; ++i) {
        output = fse.process(frame);
    }

    // Measure energy of output to ensure it was reduced
    float out_energy = 0.0f;
    for (float s : output.samples()) {
        out_energy += s * s;
    }

    float in_energy = 0.0f;
    for (float s : frame.samples()) {
        in_energy += s * s;
    }

    // Energy should be significantly attenuated
    EXPECT_LT(out_energy, in_energy * 0.5f);
}

// ============================================================================
// AutoCalibrationEngine Tests
// ============================================================================

TEST(AutoCalibrationTest, StateMachineProgression) {
    AutoCalibrationEngine cal;

    EXPECT_EQ(cal.get_current_step(), AutoCalibrationEngine::Step::Idle);

    cal.start_calibration();
    EXPECT_EQ(cal.get_current_step(), AutoCalibrationEngine::Step::MeasureNoiseFloor);

    // Simulate processing enough frames to advance step
    AudioFrame frame(1024, 48000, 1);
    for (int i = 0; i < 100; ++i) {
        cal.process_frame(frame);
    }

    EXPECT_EQ(cal.get_current_step(), AutoCalibrationEngine::Step::MeasureGain);

    for (int i = 0; i < 100; ++i) {
        cal.process_frame(frame);
    }

    EXPECT_EQ(cal.get_current_step(), AutoCalibrationEngine::Step::MeasureLatency);

    for (int i = 0; i < 100; ++i) {
        cal.process_frame(frame);
    }

    EXPECT_EQ(cal.get_current_step(), AutoCalibrationEngine::Step::Complete);

    auto result = cal.get_result();
    EXPECT_TRUE(result.valid);
}

// ============================================================================
// ProfileManager Tests
// ============================================================================

TEST(ProfileManagerTest, SaveLoadRoundtrip) {
    ProfileManager pm;

    Profile p;
    p.name = "TestProfile";
    p.calibration.rms_noise_floor_db = -80.0f;
    p.calibration.peak_noise_floor_db = -70.0f;
    p.calibration.recommended_input_gain = 0.8f;
    p.calibration.estimated_latency_ms = 42.0f;
    p.calibration.valid = true;
    p.max_feedback_attenuation_db = 15.0f;
    p.max_feedback_notches = 5;
    p.feedback_suppression_enabled = false;
    p.user_eq_low_gain = 1.2f;
    p.user_eq_mid_gain = 0.9f;
    p.user_eq_high_gain = 1.1f;

    const std::string filename = "test_profile_temp.cfg";
    EXPECT_TRUE(pm.save_profile(p, filename));

    auto loaded_opt = pm.load_profile(filename);
    ASSERT_TRUE(loaded_opt.has_value());

    auto loaded = loaded_opt.value();
    EXPECT_EQ(loaded.name, p.name);
    EXPECT_FLOAT_EQ(loaded.calibration.rms_noise_floor_db, p.calibration.rms_noise_floor_db);
    EXPECT_FLOAT_EQ(loaded.calibration.peak_noise_floor_db, p.calibration.peak_noise_floor_db);
    EXPECT_FLOAT_EQ(loaded.calibration.recommended_input_gain, p.calibration.recommended_input_gain);
    EXPECT_FLOAT_EQ(loaded.calibration.estimated_latency_ms, p.calibration.estimated_latency_ms);
    EXPECT_EQ(loaded.calibration.valid, p.calibration.valid);
    EXPECT_FLOAT_EQ(loaded.max_feedback_attenuation_db, p.max_feedback_attenuation_db);
    EXPECT_EQ(loaded.max_feedback_notches, p.max_feedback_notches);
    EXPECT_EQ(loaded.feedback_suppression_enabled, p.feedback_suppression_enabled);
    EXPECT_FLOAT_EQ(loaded.user_eq_low_gain, p.user_eq_low_gain);
    EXPECT_FLOAT_EQ(loaded.user_eq_mid_gain, p.user_eq_mid_gain);
    EXPECT_FLOAT_EQ(loaded.user_eq_high_gain, p.user_eq_high_gain);

    std::remove(filename.c_str());
}

// ============================================================================
// DiagnosticsEngine Tests
// ============================================================================

TEST(DiagnosticsEngineTest, ValidChecks) {
    auto health = DiagnosticsEngine::run_startup_checks(48000, 4);
    EXPECT_TRUE(health.ok);
    EXPECT_TRUE(health.sample_rate_ok);
    EXPECT_TRUE(health.channel_mapping_ok);
}

TEST(DiagnosticsEngineTest, InvalidSampleRate) {
    auto health = DiagnosticsEngine::run_startup_checks(-1, 4);
    EXPECT_FALSE(health.ok);
    EXPECT_FALSE(health.sample_rate_ok);
}

TEST(DiagnosticsEngineTest, InvalidChannels) {
    auto health = DiagnosticsEngine::run_startup_checks(48000, 0);
    EXPECT_FALSE(health.ok);
    EXPECT_FALSE(health.channel_mapping_ok);
}
