#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <complex>

namespace apm {

/**
 * @file smartglasses_apm.hpp
 * @brief Acoustic-Projection-Microphone-System (A-P-M-S) for Smartglasses
 *
 * Interface Contract:
 * "A-P-M-S is a voice-enhancement front-end that outputs AI-ready PCM frames for wake-word + ASR ingestion."
 *
 * Designed for 2-microphone frame topologies (left and right temples, ~14-16 cm baseline).
 * Replaces physical delay-and-sum beamforming with geometric acoustic projection
 * onto the virtual mouth acoustic vector, phase-coherence masking, and ultra-low-power
 * recursive spectral IIR gain smoothing.
 *
 * Native Frame Configuration: 16 kHz sample rate, 10 ms frame (160 samples per channel).
 */

struct SmartglassesApmConfig {
    int sample_rate{16000};         ///< Primary voice sample rate (default: 16000 Hz)
    int frame_size{160};            ///< Primary frame size in samples (10 ms @ 16 kHz)
    int fft_size{256};              ///< Zero-padded STFT window size (power of 2)
    float mic_baseline_m{0.15f};    ///< Distance between left and right temple mics (~15 cm)
    float min_gain_floor{0.10f};    ///< Minimum suppression floor (-20 dB)
    float alpha_smooth{0.85f};      ///< Temporal recursive smoothing factor
    float spatial_exp{1.5f};        ///< Spatial projection exponent alpha
};

struct SmartglassesFrameMetadata {
    float speech_confidence{0.0f};  ///< VAD indicator [0.0, 1.0]
    float estimated_snr_db{0.0f};   ///< Real-time estimated SNR in dB
    bool is_speech_active{false};   ///< Voice activity decision
    bool is_wake_word_window{false};///< High-priority speech flag for wake-word engines
};

class SmartglassesAPM {
public:
    explicit SmartglassesAPM(const SmartglassesApmConfig& config = SmartglassesApmConfig{});
    ~SmartglassesAPM() = default;

    /**
     * @brief Reset internal DSP state buffers and filter states.
     */
    void reset();

    /**
     * @brief Process a single frame of dual-temple microphone audio.
     *
     * No working-buffer allocations. Pre-size out_enhanced to frame_size
     * to avoid growing the caller-owned output vector. Single caller per
     * instance; processing and reset must not run concurrently.
     *
     * @param mic_left Left temple microphone samples (160 samples @ 16 kHz)
     * @param mic_right Right temple microphone samples (160 samples @ 16 kHz)
     * @param out_enhanced Output vector for mouth-projected mono PCM samples (160 samples)
     * @param metadata Output metadata for downstream AI assistants (VAD / SNR / Wake-Word)
     * @return true if processing succeeded, false otherwise.
     */
    bool process_frame(
        const std::vector<float>& mic_left,
        const std::vector<float>& mic_right,
        std::vector<float>& out_enhanced,
        SmartglassesFrameMetadata& metadata
    );

    /**
     * @brief Process interleaved signed 16-bit PCM dual-channel input.
     *
     * @param pcm_interleaved_lr Interleaved L/R 16-bit PCM buffer (320 samples total for 160 frame)
     * @param total_samples Total int16 elements in pcm_interleaved_lr (must be >= frame_size * 2)
     * @param pcm_out_mono Output buffer for 16 kHz mono PCM 16-bit samples (160 samples)
     * @param metadata Output metadata for downstream AI assistants
     * @return true if processing succeeded, false otherwise.
     */
    bool process_frame_pcm16(
        const int16_t* pcm_interleaved_lr,
        size_t total_samples,
        int16_t* pcm_out_mono,
        SmartglassesFrameMetadata& metadata
    );

    const SmartglassesApmConfig& get_config() const { return config_; }

private:
    SmartglassesApmConfig config_;
    size_t num_bins_{0};

    // Pre-allocated static buffers (No dynamic heap allocations in process_frame)
    std::vector<float> window_;
    std::vector<float> prev_gain_;
    std::vector<float> cos_table_;
    std::vector<float> sin_table_;

    // Working buffers pre-allocated in constructor
    std::vector<float> win_L_;
    std::vector<float> win_R_;
    std::vector<std::complex<float>> L_fft_;
    std::vector<std::complex<float>> R_fft_;
    std::vector<std::complex<float>> S_enhanced_;
    std::vector<float> pcm_left_;
    std::vector<float> pcm_right_;
    std::vector<float> pcm_output_;
};

} // namespace apm
