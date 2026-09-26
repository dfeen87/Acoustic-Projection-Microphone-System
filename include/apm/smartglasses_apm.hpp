#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>

namespace apm {

/**
 * @file smartglasses_apm.hpp
 * @brief Acoustic-Projection-Microphone-System (A-P-M-S) for Smartglasses
 *
 * Designed for 2-microphone frame topologies (left and right temples).
 * Replaces physical delay-and-sum beamforming with geometric acoustic projection
 * onto the mouth vector, phase-coherence masking, and ultra-low-power IIR smoothing.
 *
 * Native Frame Config: 16 kHz sample rate, 10 ms frame (160 samples per channel).
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
};

class SmartglassesAPM {
public:
    explicit SmartglassesAPM(const SmartglassesApmConfig& config = SmartglassesApmConfig{});
    ~SmartglassesAPM() = default;

    /**
     * @brief Process a single frame of dual-temple microphone audio.
     *
     * @param mic_left Left temple microphone samples (160 samples @ 16 kHz)
     * @param mic_right Right temple microphone samples (160 samples @ 16 kHz)
     * @param out_enhanced Output vector for mouth-projected mono PCM samples (160 samples)
     * @param metadata Output metadata for downstream AI assistants (VAD / SNR)
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
     */
    bool process_frame_pcm16(
        const int16_t* pcm_interleaved_lr,
        size_t total_samples, // 320 total for L/R interleaved 160 samples
        int16_t* pcm_out_mono,
        SmartglassesFrameMetadata& metadata
    );

    const SmartglassesApmConfig& get_config() const { return config_; }

private:
    SmartglassesApmConfig config_;
    size_t num_bins_{0};

    std::vector<float> window_;
    std::vector<float> prev_gain_;

    // Low-power DFT implementation buffers
    std::vector<float> cos_table_;
    std::vector<float> sin_table_;
};

} // namespace apm
