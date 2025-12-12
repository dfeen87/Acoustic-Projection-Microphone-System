#pragma once
#include <vector>
#include <span>
#include <chrono>
#include <optional>
#include <string>
#include <future>
#include <memory>

namespace apm {

// ============================================================================
// AudioFrame + Metadata
// ============================================================================

struct AudioMetadata {
    std::chrono::microseconds timestamp;
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
    std::vector<float> reference_buffer_;
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
    };

    struct TranslationResult {
        AudioFrame translated_audio;
        std::string source_text;
        std::string translated_text;
        float confidence;
        int latency_ms;
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
// APM System (Full Pipeline)
// ============================================================================

class APMSystem {
    BeamformingEngine beamformer_;
    NoiseSuppressionEngine noise_suppressor_;
    EchoCancellationEngine echo_canceller_;
    VoiceActivityDetector vad_;
    DirectionalProjector projector_;
    std::unique_ptr<TranslationEngine> translator_;

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

public:
    explicit APMSystem(const Config& cfg = Config{});

    std::future<std::vector<AudioFrame>> process_async(
        const std::vector<AudioFrame>& microphone_array,
        const AudioFrame& speaker_reference,
        float target_direction_rad);

    std::vector<AudioFrame> process(
        const std::vector<AudioFrame>& microphone_array,
        const AudioFrame& speaker_reference,
        float target_direction_rad);

    void reset_all();
};

} // namespace apm
