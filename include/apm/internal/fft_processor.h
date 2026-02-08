// include/apm/fft_processor.h
#pragma once

#include <vector>
#include <complex>
#include <memory>
#include <mutex>
#include <stdexcept>

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

namespace apm {

#ifdef HAVE_FFTW3
class FFTProcessor {
public:
    explicit FFTProcessor(int size);
    ~FFTProcessor();
    
    // Disable copy, enable move
    FFTProcessor(const FFTProcessor&) = delete;
    FFTProcessor& operator=(const FFTProcessor&) = delete;
    FFTProcessor(FFTProcessor&&) noexcept;
    FFTProcessor& operator=(FFTProcessor&&) noexcept;
    
    // Real-to-complex FFT
    void forward(const std::vector<float>& input, 
                 std::vector<std::complex<float>>& output);
    
    // Complex-to-real IFFT
    void inverse(const std::vector<std::complex<float>>& input,
                 std::vector<float>& output);
    
    // In-place windowing
    static void apply_window(std::vector<float>& data, WindowType type);
    
    enum class WindowType {
        Hann,
        Hamming,
        Blackman,
        Kaiser
    };
    
    int size() const { return size_; }
    int complex_size() const { return size_ / 2 + 1; }

private:
    int size_;
    fftwf_plan forward_plan_;
    fftwf_plan inverse_plan_;
    std::vector<float> real_buffer_;
    std::vector<fftwf_complex> complex_buffer_;
    
    static std::mutex fftw_mutex_; // FFTW planning is not thread-safe
};

// STFT (Short-Time Fourier Transform) helper
class STFTProcessor {
public:
    STFTProcessor(int fft_size, int hop_size, FFTProcessor::WindowType window);
    
    // Process audio in overlapping frames
    std::vector<std::vector<std::complex<float>>> analyze(
        const std::vector<float>& signal);
    
    // Reconstruct from STFT
    std::vector<float> synthesize(
        const std::vector<std::vector<std::complex<float>>>& stft);

private:
    int fft_size_;
    int hop_size_;
    FFTProcessor::WindowType window_type_;
    FFTProcessor fft_;
    std::vector<float> window_;
    std::vector<float> synthesis_window_;
};

} // namespace apm

// src/fft_processor.cpp
#include "apm/fft_processor.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace apm {

std::mutex FFTProcessor::fftw_mutex_;

FFTProcessor::FFTProcessor(int size) 
    : size_(size),
      real_buffer_(size),
      complex_buffer_(size / 2 + 1) {
    
    std::lock_guard<std::mutex> lock(fftw_mutex_);
    
    // Create FFTW plans
    forward_plan_ = fftwf_plan_dft_r2c_1d(
        size_,
        real_buffer_.data(),
        reinterpret_cast<fftwf_complex*>(complex_buffer_.data()),
        FFTW_MEASURE
    );
    
    inverse_plan_ = fftwf_plan_dft_c2r_1d(
        size_,
        reinterpret_cast<fftwf_complex*>(complex_buffer_.data()),
        real_buffer_.data(),
        FFTW_MEASURE
    );
    
    if (!forward_plan_ || !inverse_plan_) {
        throw std::runtime_error("Failed to create FFTW plans");
    }
}

FFTProcessor::~FFTProcessor() {
    std::lock_guard<std::mutex> lock(fftw_mutex_);
    if (forward_plan_) fftwf_destroy_plan(forward_plan_);
    if (inverse_plan_) fftwf_destroy_plan(inverse_plan_);
}

FFTProcessor::FFTProcessor(FFTProcessor&& other) noexcept
    : size_(other.size_),
      forward_plan_(other.forward_plan_),
      inverse_plan_(other.inverse_plan_),
      real_buffer_(std::move(other.real_buffer_)),
      complex_buffer_(std::move(other.complex_buffer_)) {
    other.forward_plan_ = nullptr;
    other.inverse_plan_ = nullptr;
}

FFTProcessor& FFTProcessor::operator=(FFTProcessor&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(fftw_mutex_);
        if (forward_plan_) fftwf_destroy_plan(forward_plan_);
        if (inverse_plan_) fftwf_destroy_plan(inverse_plan_);
        
        size_ = other.size_;
        forward_plan_ = other.forward_plan_;
        inverse_plan_ = other.inverse_plan_;
        real_buffer_ = std::move(other.real_buffer_);
        complex_buffer_ = std::move(other.complex_buffer_);
        
        other.forward_plan_ = nullptr;
        other.inverse_plan_ = nullptr;
    }
    return *this;
}

void FFTProcessor::forward(const std::vector<float>& input,
                           std::vector<std::complex<float>>& output) {
    if (input.size() != size_) {
        throw std::invalid_argument("Input size mismatch");
    }
    
    // Copy input to buffer
    std::copy(input.begin(), input.end(), real_buffer_.begin());
    
    // Execute FFT
    fftwf_execute(forward_plan_);
    
    // Copy result
    output.resize(complex_size());
    for (int i = 0; i < complex_size(); ++i) {
        output[i] = std::complex<float>(
            complex_buffer_[i][0],
            complex_buffer_[i][1]
        );
    }
}

void FFTProcessor::inverse(const std::vector<std::complex<float>>& input,
                           std::vector<float>& output) {
    if (input.size() != complex_size()) {
        throw std::invalid_argument("Input size mismatch");
    }
    
    // Copy input to buffer
    for (size_t i = 0; i < input.size(); ++i) {
        complex_buffer_[i][0] = input[i].real();
        complex_buffer_[i][1] = input[i].imag();
    }
    
    // Execute IFFT
    fftwf_execute(inverse_plan_);
    
    // Normalize and copy result
    output.resize(size_);
    float norm = 1.0f / size_;
    for (int i = 0; i < size_; ++i) {
        output[i] = real_buffer_[i] * norm;
    }
}

void FFTProcessor::apply_window(std::vector<float>& data, WindowType type) {
    const int N = data.size();
    
    for (int n = 0; n < N; ++n) {
        float w = 1.0f;
        
        switch (type) {
            case WindowType::Hann:
                w = 0.5f * (1.0f - std::cos(2.0f * M_PI * n / (N - 1)));
                break;
                
            case WindowType::Hamming:
                w = 0.54f - 0.46f * std::cos(2.0f * M_PI * n / (N - 1));
                break;
                
            case WindowType::Blackman:
                w = 0.42f - 0.5f * std::cos(2.0f * M_PI * n / (N - 1))
                    + 0.08f * std::cos(4.0f * M_PI * n / (N - 1));
                break;
                
            case WindowType::Kaiser:
                // Simplified Kaiser window (beta=8.6)
                {
                    float alpha = (N - 1) / 2.0f;
                    float x = (n - alpha) / alpha;
                    w = std::cyl_bessel_i(0.0f, 8.6f * std::sqrt(1.0f - x * x)) /
                        std::cyl_bessel_i(0.0f, 8.6f);
                }
                break;
        }
        
        data[n] *= w;
    }
}

// STFT Implementation
STFTProcessor::STFTProcessor(int fft_size, int hop_size, 
                             FFTProcessor::WindowType window)
    : fft_size_(fft_size),
      hop_size_(hop_size),
      window_type_(window),
      fft_(fft_size),
      window_(fft_size, 1.0f),
      synthesis_window_(fft_size, 1.0f) {
    
    // Create analysis window
    FFTProcessor::apply_window(window_, window_type_);
    
    // Create synthesis window (for overlap-add)
    // Use sqrt of analysis window for perfect reconstruction
    for (int i = 0; i < fft_size_; ++i) {
        synthesis_window_[i] = std::sqrt(window_[i]);
        window_[i] = synthesis_window_[i];
    }
}

std::vector<std::vector<std::complex<float>>> 
STFTProcessor::analyze(const std::vector<float>& signal) {
    
    std::vector<std::vector<std::complex<float>>> result;
    
    for (size_t pos = 0; pos + fft_size_ <= signal.size(); pos += hop_size_) {
        // Extract frame
        std::vector<float> frame(signal.begin() + pos,
                                signal.begin() + pos + fft_size_);
        
        // Apply window
        for (int i = 0; i < fft_size_; ++i) {
            frame[i] *= window_[i];
        }
        
        // FFT
        std::vector<std::complex<float>> spectrum;
        fft_.forward(frame, spectrum);
        
        result.push_back(std::move(spectrum));
    }
    
    return result;
}

std::vector<float> STFTProcessor::synthesize(
    const std::vector<std::vector<std::complex<float>>>& stft) {
    
    if (stft.empty()) return {};
    
    int output_length = (stft.size() - 1) * hop_size_ + fft_size_;
    std::vector<float> output(output_length, 0.0f);
    std::vector<float> window_sum(output_length, 0.0f);
    
    for (size_t frame_idx = 0; frame_idx < stft.size(); ++frame_idx) {
        // IFFT
        std::vector<float> time_frame;
        fft_.inverse(stft[frame_idx], time_frame);
        
        // Overlap-add with synthesis window
        size_t pos = frame_idx * hop_size_;
        for (int i = 0; i < fft_size_ && pos + i < output.size(); ++i) {
            output[pos + i] += time_frame[i] * synthesis_window_[i];
            window_sum[pos + i] += synthesis_window_[i] * synthesis_window_[i];
        }
    }
    
    // Normalize by window sum
    for (size_t i = 0; i < output.size(); ++i) {
        if (window_sum[i] > 1e-8f) {
            output[i] /= window_sum[i];
        }
    }
    
    return output;
}

} // namespace apm

#else

namespace apm {

class FFTProcessor {
public:
    explicit FFTProcessor(int size)
        : size_(size) {
        throw std::runtime_error("FFTW3 support is not available");
    }
    ~FFTProcessor() = default;

    FFTProcessor(const FFTProcessor&) = delete;
    FFTProcessor& operator=(const FFTProcessor&) = delete;
    FFTProcessor(FFTProcessor&&) noexcept = default;
    FFTProcessor& operator=(FFTProcessor&&) noexcept = default;

    void forward(const std::vector<float>&,
                 std::vector<std::complex<float>>&) {
        throw std::runtime_error("FFTW3 support is not available");
    }

    void inverse(const std::vector<std::complex<float>>&,
                 std::vector<float>&) {
        throw std::runtime_error("FFTW3 support is not available");
    }

    enum class WindowType {
        Hann,
        Hamming,
        Blackman,
        Kaiser
    };

    static void apply_window(std::vector<float>&, WindowType) {
        throw std::runtime_error("FFTW3 support is not available");
    }

    int size() const { return size_; }
    int complex_size() const { return size_ / 2 + 1; }

private:
    int size_;
};

class STFTProcessor {
public:
    STFTProcessor(int, int, FFTProcessor::WindowType) {
        throw std::runtime_error("FFTW3 support is not available");
    }

    std::vector<std::vector<std::complex<float>>> analyze(
        const std::vector<float>&) {
        throw std::runtime_error("FFTW3 support is not available");
    }

    std::vector<float> synthesize(
        const std::vector<std::vector<std::complex<float>>>&) {
        throw std::runtime_error("FFTW3 support is not available");
    }
};

} // namespace apm

#endif
