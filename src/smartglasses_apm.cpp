#include "apm/smartglasses_apm.hpp"
#include <algorithm>
#include <complex>
#include <cmath>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace apm {

SmartglassesAPM::SmartglassesAPM(const SmartglassesApmConfig& config)
    : config_(config),
      num_bins_(config.fft_size / 2 + 1),
      window_(config.fft_size, 0.0f),
      prev_gain_(num_bins_, 1.0f) {

    // Construct Hann window for N_FFT = 256
    for (int i = 0; i < config_.fft_size; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (config_.fft_size - 1)));
    }

    // Pre-compute trig tables for real-valued DFT on 256 points (ultra-fast, fixed RAM)
    cos_table_.resize(config_.fft_size * num_bins_);
    sin_table_.resize(config_.fft_size * num_bins_);

    for (size_t k = 0; k < num_bins_; ++k) {
        for (int n = 0; n < config_.fft_size; ++n) {
            float angle = 2.0f * static_cast<float>(M_PI) * k * n / config_.fft_size;
            cos_table_[k * config_.fft_size + n] = std::cos(angle);
            sin_table_[k * config_.fft_size + n] = std::sin(angle);
        }
    }
}

bool SmartglassesAPM::process_frame(
    const std::vector<float>& mic_left,
    const std::vector<float>& mic_right,
    std::vector<float>& out_enhanced,
    SmartglassesFrameMetadata& metadata) {

    if (mic_left.size() < static_cast<size_t>(config_.frame_size) ||
        mic_right.size() < static_cast<size_t>(config_.frame_size)) {
        return false;
    }

    out_enhanced.resize(config_.frame_size, 0.0f);

    // 1. Prepare Windowed STFT Buffers (zero-padded to 256)
    std::vector<float> win_L(config_.fft_size, 0.0f);
    std::vector<float> win_R(config_.fft_size, 0.0f);

    for (int i = 0; i < config_.frame_size; ++i) {
        win_L[i] = mic_left[i] * window_[i];
        win_R[i] = mic_right[i] * window_[i];
    }

    // 2. Real-valued DFT computation
    std::vector<std::complex<float>> L_fft(num_bins_);
    std::vector<std::complex<float>> R_fft(num_bins_);

    for (size_t k = 0; k < num_bins_; ++k) {
        float re_L = 0.0f, im_L = 0.0f;
        float re_R = 0.0f, im_R = 0.0f;
        size_t tbl_base = k * config_.fft_size;

        for (int n = 0; n < config_.fft_size; ++n) {
            float c = cos_table_[tbl_base + n];
            float s = sin_table_[tbl_base + n];

            re_L += win_L[n] * c;
            im_L -= win_L[n] * s;

            re_R += win_R[n] * c;
            im_R -= win_R[n] * s;
        }

        L_fft[k] = {re_L, im_L};
        R_fft[k] = {re_R, im_R};
    }

    // 3. STFT Bin Processing (Geometric Projection + Phase Masking)
    std::vector<std::complex<float>> S_enhanced(num_bins_);
    float total_signal_energy = 0.0f;
    float total_noise_energy = 0.0f;

    for (size_t k = 0; k < num_bins_; ++k) {
        // Cross-spectral density P_LR = L * conj(R)
        std::complex<float> P_lr = L_fft[k] * std::conj(R_fft[k]);
        float phase_diff = std::arg(P_lr);

        float mag_L = std::abs(L_fft[k]) + 1e-6f;
        float mag_R = std::abs(R_fft[k]) + 1e-6f;

        // Phase Coherence Index
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

        // Average Left + Right projected onto mouth axis
        std::complex<float> S_avg = 0.5f * (L_fft[k] + R_fft[k]);
        S_enhanced[k] = S_avg * smooth_gain;

        float raw_input_power = 0.5f * (std::norm(L_fft[k]) + std::norm(R_fft[k]));
        float enhanced_power = std::norm(S_enhanced[k]);

        total_signal_energy += enhanced_power;
        total_noise_energy += std::max(0.0f, raw_input_power - enhanced_power);
    }

    // 4. Inverse DFT & Output Framing
    for (int n = 0; n < config_.frame_size; ++n) {
        float sample_acc = S_enhanced[0].real() + S_enhanced[num_bins_ - 1].real() * std::cos(M_PI * n);

        for (size_t k = 1; k < num_bins_ - 1; ++k) {
            size_t tbl_base = k * config_.fft_size;
            float c = cos_table_[tbl_base + n];
            float s = sin_table_[tbl_base + n];

            // 2 * (real * cos - imag * sin)
            sample_acc += 2.0f * (S_enhanced[k].real() * c - S_enhanced[k].imag() * s);
        }

        out_enhanced[n] = sample_acc / static_cast<float>(config_.fft_size);
    }

    // 5. Metadata Generation for Downstream AI Assistant
    float snr_ratio = (total_noise_energy > 1e-6f) ? (total_signal_energy / total_noise_energy) : (total_signal_energy > 1e-6f ? 100.0f : 0.01f);
    metadata.estimated_snr_db = 10.0f * std::log10(std::max(0.001f, snr_ratio));

    metadata.speech_confidence = std::clamp((metadata.estimated_snr_db - 3.0f) / 15.0f, 0.0f, 1.0f);
    metadata.is_speech_active = (metadata.speech_confidence > 0.35f);

    return true;
}

bool SmartglassesAPM::process_frame_pcm16(
    const int16_t* pcm_interleaved_lr,
    size_t total_samples,
    int16_t* pcm_out_mono,
    SmartglassesFrameMetadata& metadata) {

    if (!pcm_interleaved_lr || !pcm_out_mono || total_samples < static_cast<size_t>(config_.frame_size * 2)) {
        return false;
    }

    std::vector<float> mic_L(config_.frame_size);
    std::vector<float> mic_R(config_.frame_size);

    for (int i = 0; i < config_.frame_size; ++i) {
        mic_L[i] = static_cast<float>(pcm_interleaved_lr[2 * i]) / 32768.0f;
        mic_R[i] = static_cast<float>(pcm_interleaved_lr[2 * i + 1]) / 32768.0f;
    }

    std::vector<float> out_enhanced;
    bool success = process_frame(mic_L, mic_R, out_enhanced, metadata);

    if (success) {
        for (int i = 0; i < config_.frame_size; ++i) {
            float clamped = std::clamp(out_enhanced[i] * 32768.0f, -32768.0f, 32767.0f);
            pcm_out_mono[i] = static_cast<int16_t>(clamped);
        }
    }

    return success;
}

} // namespace apm
