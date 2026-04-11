#pragma once
#include <vector>
#include <span>
#include <chrono>
#include <optional>
#include <string>
#include <future>
#include <memory>
#include <numeric>
#include <algorithm>
#include <deque>

namespace apm {

// ============================================================================
// AudioFrame + Metadata
// ============================================================================

struct AudioMetadata {
    std::chrono::microseconds timestamp{};
    float peak_db{-96.0f};
    float rms_db{-96.0f};
    float snr_db{0.0f};
    bool clipping{false};
    std::optional<std::string> speaker_id;
    std::optional<std::string> emotion;
    std::optional<float> pitch_hz;
};

class AudioFrame {
    std::vector<float> data_;
    int sample_rate_;
    int channels_;
    AudioMetadata metadata_;

public:
    // ✅ Default constructor required for TranslationRequest/Result
    AudioFrame() : data_(), sample_rate_(0), channels_(0), metadata_{} {}

    AudioFrame(size_t samples, int sr, int ch);

    std::span<float> samples();
    std::span<const float> samples() const;
    int sample_rate() const { return sample_rate_; }
    int channels() const { return channels_; }
    size_t frame_count() const;
    AudioMetadata& metadata() { return metadata_; }
    const AudioMetadata& metadata() const { return metadata_; }

    std::vector<float> channel(int ch) const;
    void compute_metadata();
};

// ============================================================================
// Beamforming Engine
// ============================================================================

class BeamformingEngine {
    int array_size_;
    float spacing_m_;
    float speed_of_sound_{343.0f};

    float lagrange_interpolate(std::span<const float> signal, float idx) const;

public:
    BeamformingEngine(int mics, float spacing);

    AudioFrame delay_and_sum(const std::vector<AudioFrame>& mic_array,
                             float azimuth_rad, float elevation_rad);

    AudioFrame superdirective(const std::vector<AudioFrame>& mic_array,
                              float azimuth_rad);

    AudioFrame adaptive_null_steering(const std::vector<AudioFrame>& mic_array,
                                      float target_azimuth,
                                      const std::vector<float>& interference_azimuths);
};

// ============================================================================
// Noise Suppression Engine
// ============================================================================

class NoiseSuppressionEngine {
    struct LSTMState;

    static constexpr int FFT_SIZE = 512;
    static constexpr int HOP_SIZE = 256;

    std::unique_ptr<LSTMState> lstm_state_;
    std::vector<float> prev_frame_;
    std::vector<float> hann_window_;

    void init_window();
    std::vector<float> compute_features(const std::vector<float>& frame);
    std::vector<float> lstm_forward(const std::vector<float>& input);

public:
    NoiseSuppressionEngine();
    ~NoiseSuppressionEngine();

    AudioFrame suppress(const AudioFrame& noisy);
    void reset_state();
};

// ============================================================================
// Echo Cancellation Engine
// ============================================================================

class EchoCancellationEngine {
    int filter_length_;
    std::vector<float> adaptive_weights_;
    std::deque<float> reference_buffer_;  // Changed to deque for O(1) operations
    float mu_{0.3f};

public:
    explicit EchoCancellationEngine(int filter_len = 2048);

    AudioFrame cancel_echo(const AudioFrame& microphone,
                           const AudioFrame& speaker_reference);

    bool detect_double_talk(const AudioFrame& mic,
                            const AudioFrame& ref) const;

    void reset();
};

// ============================================================================
// Voice Activity Detector
// ============================================================================

class VoiceActivityDetector {
    float energy_threshold_db_{-30.0f};
    int hangover_frames_{10};
    int current_hangover_{0};
    std::vector<float> noise_estimate_;

    float compute_energy_db(const AudioFrame& frame) const;
    int compute_zero_crossing_rate(const AudioFrame& frame) const;

public:
    struct VadResult {
        bool speech_detected;
        float confidence;
        float snr_db;
        float energy_db;
    };

    VadResult detect(const AudioFrame& frame);
    void adapt_threshold(float ambient_noise_db);
    void reset();
};

// ============================================================================
// Translation Engine (Interface + Mock)
// ============================================================================

class TranslationEngine {
public:
    struct TranslationRequest {
        AudioFrame audio;
        std::string source_lang;
        std::string target_lang;
        std::vector<std::string> context_history;

        // ✅ Default constructor required
        TranslationRequest() = default;
    };

    struct TranslationResult {
        AudioFrame translated_audio;
        std::string source_text;
        std::string translated_text;
        float confidence{0.0f};
        int latency_ms{0};

        // ✅ Default constructor required
        TranslationResult() = default;
    };

    virtual std::future<TranslationResult> translate_async(
        const TranslationRequest& request) = 0;

    virtual ~TranslationEngine() = default;
};

class MockTranslationEngine : public TranslationEngine {
public:
    std::future<TranslationResult> translate_async(
        const TranslationRequest& request) override;
};

// ============================================================================
// Directional Projector
// ============================================================================

class DirectionalProjector {
    int speaker_array_size_;
    float spacing_m_;
    float speed_of_sound_{343.0f};

public:
    DirectionalProjector(int speakers, float spacing);

    std::vector<AudioFrame> create_projection_signals(
        const AudioFrame& source,
        float target_azimuth_rad,
        float target_distance_m);
};

// ============================================================================
// Profiles & Calibration
// ============================================================================

struct CalibrationProfile {
    float rms_noise_floor_db{-96.0f};
    float peak_noise_floor_db{-96.0f};
    float recommended_input_gain{1.0f};
    float estimated_latency_ms{0.0f};
    bool valid{false};
};

struct Profile {
    std::string name{"Default"};
    CalibrationProfile calibration;
    float max_feedback_attenuation_db{12.0f};
    int max_feedback_notches{3};
    bool feedback_suppression_enabled{true};
    float user_eq_low_gain{1.0f};
    float user_eq_mid_gain{1.0f};
    float user_eq_high_gain{1.0f};
};

class ProfileManager {
    std::vector<Profile> profiles_;
    std::string current_profile_name_;

public:
    ProfileManager();

    bool save_profile(const Profile& profile, const std::string& filepath);
    std::optional<Profile> load_profile(const std::string& filepath);

    void add_profile(const Profile& profile);
    std::vector<std::string> get_profile_names() const;
    std::optional<Profile> get_profile(const std::string& name) const;
    void set_active_profile(const std::string& name);
    std::string get_active_profile_name() const;
};

class AutoCalibrationEngine {
public:
    enum class Step {
        Idle,
        MeasureNoiseFloor,
        MeasureGain,
        MeasureLatency,
        Complete
    };

private:
    Step current_step_{Step::Idle};
    CalibrationProfile current_profile_;
    int frames_processed_{0};
    int target_frames_{0};

    // Accumulators for measurements
    float acc_energy_{0.0f};
    float max_peak_{-96.0f};

public:
    AutoCalibrationEngine() = default;

    void start_calibration();
    void cancel_calibration();
    void advance_step();
    Step get_current_step() const { return current_step_; }

    // Call this repeatedly with audio frames during the calibration process
    void process_frame(const AudioFrame& frame);

    CalibrationProfile get_result() const { return current_profile_; }
    float get_progress() const;
};

// ============================================================================
// Feedback Suppression
// ============================================================================

class FeedbackSuppressionEngine {
    struct BiquadFilter {
        float b0, b1, b2, a1, a2;
        float x1, x2, y1, y2;
        float freq_hz;

        void set_notch(float freq, float sample_rate, float q = 10.0f);
        float process(float in);
    };

    std::vector<BiquadFilter> notches_;
    int sample_rate_{48000};
    int max_notches_{3};
    float max_attenuation_db_{12.0f};

    // Simple ZCR based feedback detection
    int frames_since_update_{0};
    std::deque<float> zcr_history_;

    float detect_feedback_freq(const AudioFrame& frame) const;

public:
    FeedbackSuppressionEngine(int max_notches = 3, float max_attenuation = 12.0f);

    void configure(int sample_rate, int max_notches, float max_attenuation);

    AudioFrame process(const AudioFrame& frame);
    void reset();
};

// ============================================================================
// Diagnostics & Monitoring
// ============================================================================

struct MonitoringMetrics {
    float peak_db{-96.0f};
    float rms_db{-96.0f};
    float snr_db{0.0f};
    bool clipping{false};
    float latency_ms{0.0f};
};

struct HealthStatus {
    bool ok{true};
    std::string message{"OK"};
    bool input_device_ok{true};
    bool sample_rate_ok{true};
    bool channel_mapping_ok{true};
};

class DiagnosticsEngine {
public:
    static HealthStatus run_startup_checks(int expected_sample_rate, int expected_channels);
};

// ============================================================================
// APM System (Full Pipeline)
// ============================================================================

class APMSystem {
public:
    struct Config {
        int num_microphones = 4;
        float mic_spacing_m = 0.012f;
        int num_speakers = 3;
        float speaker_spacing_m = 0.015f;
        int sample_rate = 48000;
        std::string source_language = "en-US";
        std::string target_language = "es-ES";
    };

private:
    Config config_;
    BeamformingEngine beamformer_;
    NoiseSuppressionEngine noise_suppressor_;
    EchoCancellationEngine echo_canceller_;
    VoiceActivityDetector vad_;
    DirectionalProjector projector_;
    std::unique_ptr<TranslationEngine> translator_;

    // New Features
    FeedbackSuppressionEngine feedback_suppressor_;
    ProfileManager profile_manager_;
    AutoCalibrationEngine calibration_engine_;

    MonitoringMetrics current_metrics_;
    mutable std::mutex metrics_mutex_;

public:
    // ✅ Remove invalid default argument
    explicit APMSystem(const Config& cfg);

    // ✅ Add default constructor
    APMSystem();

    std::future<std::vector<AudioFrame>> process_async(
        const std::vector<AudioFrame>& microphone_array,
        const AudioFrame& speaker_reference,
        float target_direction_rad);

    std::vector<AudioFrame> process(
        const std::vector<AudioFrame>& microphone_array,
        const AudioFrame& speaker_reference,
        float target_direction_rad);

    void reset_all();

    // New API
    MonitoringMetrics get_monitoring_metrics() const;
    ProfileManager& get_profile_manager() { return profile_manager_; }
    AutoCalibrationEngine& get_calibration_engine() { return calibration_engine_; }
};

} // namespace apm
