#include "apm/smartglasses_apm.hpp"
#include <algorithm>
#include <complex>
#include <cmath>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace apm {

namespace {

inline float sanitize_sample(float v) {
    if (!std::isfinite(v)) {
        return 0.0f;
    }
    return std::clamp(v, -1.0f, 1.0f);
}

SmartglassesApmConfig sanitize_config(SmartglassesApmConfig config) {
    if (config.fft_size < 2 || (config.fft_size & (config.fft_size - 1)) != 0) {
        config.fft_size = 256;
    }
    if (config.frame_size <= 0 || config.frame_size > config.fft_size) {
        config.frame_size = std::min(160, config.fft_size);
    }
    if (config.sample_rate <= 0) config.sample_rate = 16000;
    if (!std::isfinite(config.min_gain_floor)) config.min_gain_floor = 0.10f;
    if (!std::isfinite(config.alpha_smooth)) config.alpha_smooth = 0.85f;
    if (!std::isfinite(config.spatial_exp)) config.spatial_exp = 1.5f;
    if (!std::isfinite(config.mic_baseline_m) || config.mic_baseline_m <= 0) config.mic_baseline_m = 0.15f;
    config.min_gain_floor = std::clamp(config.min_gain_floor, 0.001f, 1.0f);
    config.alpha_smooth = std::clamp(config.alpha_smooth, 0.0f, 0.999f);
    config.spatial_exp = std::clamp(config.spatial_exp, 0.1f, 5.0f);
    return config;
}

} // namespace

SmartglassesAPM::SmartglassesAPM(const SmartglassesApmConfig& config)
    : config_(sanitize_config(config)),
      num_bins_(config_.fft_size / 2 + 1),
      window_(config_.fft_size, 0.0f),
      prev_gain_(num_bins_, 1.0f),
      win_L_(config_.fft_size, 0.0f),
      win_R_(config_.fft_size, 0.0f),
      L_fft_(num_bins_),
      R_fft_(num_bins_),
      S_enhanced_(num_bins_),
      pcm_left_(config_.frame_size),
      pcm_right_(config_.frame_size),
      pcm_output_(config_.frame_size) {

    // Construct Hann window for N_FFT = 256
    for (int i = 0; i < config_.fft_size; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / static_cast<float>(config_.fft_size - 1)));
    }

    // Pre-compute trig tables for real-valued DFT on 256 points (fixed RAM, ultra-fast)
    cos_table_.resize(config_.fft_size * num_bins_);
    sin_table_.resize(config_.fft_size * num_bins_);

    for (size_t k = 0; k < num_bins_; ++k) {
        for (int n = 0; n < config_.fft_size; ++n) {
            float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(k * n) / static_cast<float>(config_.fft_size);
            cos_table_[k * config_.fft_size + n] = std::cos(angle);
            sin_table_[k * config_.fft_size + n] = std::sin(angle);
        }
    }
}

void SmartglassesAPM::reset() {
    std::fill(prev_gain_.begin(), prev_gain_.end(), 1.0f);
    std::fill(win_L_.begin(), win_L_.end(), 0.0f);
    std::fill(win_R_.begin(), win_R_.end(), 0.0f);
    std::fill(L_fft_.begin(), L_fft_.end(), std::complex<float>(0.0f, 0.0f));
    std::fill(R_fft_.begin(), R_fft_.end(), std::complex<float>(0.0f, 0.0f));
    std::fill(S_enhanced_.begin(), S_enhanced_.end(), std::complex<float>(0.0f, 0.0f));
}

bool SmartglassesAPM::process_frame(
    const std::vector<float>& mic_left,
    const std::vector<float>& mic_right,
    std::vector<float>& out_enhanced,
    SmartglassesFrameMetadata& metadata) {

    // Guard against invalid inputs, nullptr-like empty vectors, or undersized buffers
    if (mic_left.size() < static_cast<size_t>(config_.frame_size) ||
        mic_right.size() < static_cast<size_t>(config_.frame_size)) {
        out_enhanced.assign(config_.frame_size, 0.0f);
        metadata.speech_confidence = 0.0f;
        metadata.estimated_snr_db = -100.0f;
        metadata.is_speech_active = false;
        metadata.is_wake_word_window = false;
        return false;
    }

    // 1. Prepare Windowed STFT Buffers (zero-padded to 256) with input sanitization
    std::fill(win_L_.begin(), win_L_.end(), 0.0f);
    std::fill(win_R_.begin(), win_R_.end(), 0.0f);

    for (int i = 0; i < config_.frame_size; ++i) {
        win_L_[i] = sanitize_sample(mic_left[i]) * window_[i];
        win_R_[i] = sanitize_sample(mic_right[i]) * window_[i];
    }
    // Consume both inputs before writing output, including in-place callers.
    out_enhanced.resize(config_.frame_size);
    std::fill(out_enhanced.begin(), out_enhanced.end(), 0.0f);

    // 2. Real-valued DFT computation
    for (size_t k = 0; k < num_bins_; ++k) {
        float re_L = 0.0f, im_L = 0.0f;
        float re_R = 0.0f, im_R = 0.0f;
        size_t tbl_base = k * config_.fft_size;

        for (int n = 0; n < config_.fft_size; ++n) {
            float c = cos_table_[tbl_base + n];
            float s = sin_table_[tbl_base + n];

            re_L += win_L_[n] * c;
            im_L -= win_L_[n] * s;

            re_R += win_R_[n] * c;
            im_R -= win_R_[n] * s;
        }

        L_fft_[k] = {re_L, im_L};
        R_fft_[k] = {re_R, im_R};
    }

    // 3. STFT Bin Processing (Geometric Mouth Projection + Phase Masking)
    float total_signal_energy = 0.0f;
    float total_noise_energy = 0.0f;

    for (size_t k = 0; k < num_bins_; ++k) {
        // Cross-spectral density P_LR = L * conj(R)
        std::complex<float> P_lr = L_fft_[k] * std::conj(R_fft_[k]);
        float phase_diff = std::arg(P_lr);

        float mag_L = std::abs(L_fft_[k]) + 1e-6f;
        float mag_R = std::abs(R_fft_[k]) + 1e-6f;

        // Phase Coherence Index gamma
        float coherence = std::abs(P_lr) / (mag_L * mag_R);
        coherence = std::clamp(coherence, 0.0f, 1.0f);

        // Phase Mask centered on symmetrical mouth vector (phase_diff = 0)
        float cos_phase = std::cos(phase_diff / 2.0f);
        float phase_mask = std::pow(std::abs(cos_phase), config_.spatial_exp);

        float inst_gain = phase_mask * coherence;
        inst_gain = std::max(config_.min_gain_floor, inst_gain);

        // IIR Temporal Gain Smoothing
        float smooth_gain = config_.alpha_smooth * prev_gain_[k] + (1.0f - config_.alpha_smooth) * inst_gain;
        prev_gain_[k] = smooth_gain;

        // Average Left + Right projected onto mouth acoustic vector
        std::complex<float> S_avg = 0.5f * (L_fft_[k] + R_fft_[k]);
        S_enhanced_[k] = S_avg * smooth_gain;

        float raw_input_power = 0.5f * (std::norm(L_fft_[k]) + std::norm(R_fft_[k]));
        float enhanced_power = std::norm(S_enhanced_[k]);

        total_signal_energy += enhanced_power;
        total_noise_energy += std::max(0.0f, raw_input_power - enhanced_power);
    }

    // 4. Inverse DFT & Output Mono PCM Framing
    for (int n = 0; n < config_.frame_size; ++n) {
        float sample_acc = S_enhanced_[0].real() + S_enhanced_[num_bins_ - 1].real() * std::cos(M_PI * n);

        for (size_t k = 1; k < num_bins_ - 1; ++k) {
            size_t tbl_base = k * config_.fft_size;
            float c = cos_table_[tbl_base + n];
            float s = sin_table_[tbl_base + n];

            // 2 * (real * cos - imag * sin)
            sample_acc += 2.0f * (S_enhanced_[k].real() * c - S_enhanced_[k].imag() * s);
        }

        float out_val = sample_acc / static_cast<float>(config_.fft_size);
        out_enhanced[n] = sanitize_sample(out_val);
    }

    // 5. Metadata Generation for Downstream AI Assistant
    float snr_ratio = (total_noise_energy > 1e-6f) ? (total_signal_energy / total_noise_energy) : (total_signal_energy > 1e-6f ? 100.0f : 0.01f);
    metadata.estimated_snr_db = 10.0f * std::log10(std::max(0.001f, snr_ratio));
    metadata.estimated_snr_db = std::clamp(metadata.estimated_snr_db, -100.0f, 100.0f);

    metadata.speech_confidence = std::clamp((metadata.estimated_snr_db - 3.0f) / 15.0f, 0.0f, 1.0f);
    metadata.is_speech_active = (metadata.speech_confidence > 0.35f);
    metadata.is_wake_word_window = (metadata.speech_confidence > 0.50f && metadata.estimated_snr_db > 6.0f);

    return true;
}

bool SmartglassesAPM::process_frame_pcm16(
    const int16_t* pcm_interleaved_lr,
    size_t total_samples,
    int16_t* pcm_out_mono,
    SmartglassesFrameMetadata& metadata) {

    if (!pcm_interleaved_lr || !pcm_out_mono || total_samples < static_cast<size_t>(config_.frame_size * 2)) {
        metadata.speech_confidence = 0.0f;
        metadata.estimated_snr_db = -100.0f;
        metadata.is_speech_active = false;
        metadata.is_wake_word_window = false;
        return false;
    }

    for (int i = 0; i < config_.frame_size; ++i) {
        pcm_left_[i] = static_cast<float>(pcm_interleaved_lr[2 * i]) / 32768.0f;
        pcm_right_[i] = static_cast<float>(pcm_interleaved_lr[2 * i + 1]) / 32768.0f;
    }

    bool success = process_frame(pcm_left_, pcm_right_, pcm_output_, metadata);

    if (success) {
        for (int i = 0; i < config_.frame_size; ++i) {
            float clamped = std::clamp(pcm_output_[i] * 32768.0f, -32768.0f, 32767.0f);
            pcm_out_mono[i] = static_cast<int16_t>(clamped);
        }
    }

    return success;
}

} // namespace apm
