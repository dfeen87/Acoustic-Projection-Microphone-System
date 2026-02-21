#include "apm/io/audio_device.h"
#include "apm/config.h"

#ifdef HAVE_PORTAUDIO
#include <portaudio.h>
#endif

#include <iostream>
#include <sstream>
#include <vector>
#include <mutex>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace apm {

#ifdef HAVE_PORTAUDIO

class AudioDevice::Impl {
    PaStream* stream_{nullptr};
    Config config_;
    AudioCallback callback_;
    bool active_{false};
    std::mutex callback_mutex_;

    // Static callback wrapper
    static int pa_callback(const void* inputBuffer, void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData) {

        auto* self = static_cast<Impl*>(userData);
        auto* in = static_cast<const float*>(inputBuffer);
        auto* out = static_cast<float*>(outputBuffer);

        int in_channels = self->config_.input_channels;
        int out_channels = self->config_.output_channels;
        int frames = static_cast<int>(framesPerBuffer);

        // PRODUCTION NOTE: Allocating vectors in audio callback is not real-time safe.
        // In a strict real-time system, we would use pre-allocated buffers or spans.
        // For this wiring phase, we use vectors to match the API.

        // Input vector (interleaved)
        std::vector<float> input_vec(in, in + frames * in_channels);

        // Output vector (interleaved) - resized to expected size
        std::vector<float> output_vec(frames * out_channels, 0.0f);

        {
            std::unique_lock<std::mutex> lock(self->callback_mutex_, std::try_to_lock);
            if (lock.owns_lock() && self->callback_) {
                self->callback_(input_vec, output_vec);
            } else {
                // If locked (contention) or no callback, silence output
                std::fill(output_vec.begin(), output_vec.end(), 0.0f);
            }
        }

        // Copy back to output buffer
        size_t copy_samples = std::min(output_vec.size(), static_cast<size_t>(frames * out_channels));
        std::memcpy(out, output_vec.data(), copy_samples * sizeof(float));

        // Zero fill remaining if any
        if (copy_samples < static_cast<size_t>(frames * out_channels)) {
             std::memset(out + copy_samples, 0, (frames * out_channels - copy_samples) * sizeof(float));
        }

        return paContinue;
    }

public:
    explicit Impl(const Config& config) : config_(config) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
            // Depending on strictness, we might want to throw or just log.
            // Throwing allows the caller to handle initialization failure.
            throw std::runtime_error(std::string("Failed to initialize PortAudio: ") + Pa_GetErrorText(err));
        }
    }

    ~Impl() {
        stop();
        Pa_Terminate();
    }

    bool start() {
        if (stream_) return true;

        PaStreamParameters inputParameters;
        inputParameters.device = (config_.input_device_index == -1)
                                ? Pa_GetDefaultInputDevice()
                                : config_.input_device_index;

        if (inputParameters.device == paNoDevice) {
             std::cerr << "Error: No default input device." << std::endl;
             return false;
        }

        inputParameters.channelCount = config_.input_channels;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        PaStreamParameters outputParameters;
        outputParameters.device = (config_.output_device_index == -1)
                                 ? Pa_GetDefaultOutputDevice()
                                 : config_.output_device_index;

        if (outputParameters.device == paNoDevice) {
             std::cerr << "Error: No default output device." << std::endl;
             return false;
        }

        outputParameters.channelCount = config_.output_channels;
        outputParameters.sampleFormat = paFloat32;
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        PaError err = Pa_OpenStream(
            &stream_,
            &inputParameters,
            &outputParameters,
            config_.sample_rate,
            config_.frames_per_buffer,
            paClipOff,
            &Impl::pa_callback,
            this
        );

        if (err != paNoError) {
            std::cerr << "PortAudio OpenStream error: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            std::cerr << "PortAudio StartStream error: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(stream_);
            stream_ = nullptr;
            return false;
        }

        active_ = true;
        return true;
    }

    bool stop() {
        if (!stream_) return true;

        PaError err = Pa_StopStream(stream_);
        if (err != paNoError) {
            std::cerr << "PortAudio StopStream error: " << Pa_GetErrorText(err) << std::endl;
        }

        err = Pa_CloseStream(stream_);
        if (err != paNoError) {
             std::cerr << "PortAudio CloseStream error: " << Pa_GetErrorText(err) << std::endl;
        }

        stream_ = nullptr;
        active_ = false;
        return true;
    }

    bool is_active() const {
        return active_;
    }

    void set_callback(AudioCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    static std::string list_devices() {
        std::stringstream ss;
        int numDevices = Pa_GetDeviceCount();
        if( numDevices < 0 ) {
            ss << "ERROR: Pa_GetDeviceCount returned " << numDevices;
            return ss.str();
        }

        for( int i=0; i<numDevices; i++ ) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo( i );
            ss << i << ": " << deviceInfo->name
               << " (In: " << deviceInfo->maxInputChannels
               << ", Out: " << deviceInfo->maxOutputChannels << ")\n";
        }
        return ss.str();
    }
};

#else

// Mock implementation for systems without PortAudio
class AudioDevice::Impl {
public:
    explicit Impl(const Config&) {}
    bool start() {
        std::cerr << "PortAudio not compiled in." << std::endl;
        return false;
    }
    bool stop() { return true; }
    bool is_active() const { return false; }
    void set_callback(AudioCallback) {}
    static std::string list_devices() { return "No PortAudio support."; }
};

#endif

// Forwarding methods
AudioDevice::AudioDevice(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

AudioDevice::~AudioDevice() = default;

bool AudioDevice::start() { return impl_->start(); }
bool AudioDevice::stop() { return impl_->stop(); }
bool AudioDevice::is_active() const { return impl_->is_active(); }
void AudioDevice::set_callback(AudioCallback callback) { impl_->set_callback(std::move(callback)); }

std::string AudioDevice::list_devices() {
#ifdef HAVE_PORTAUDIO
    // Ensure PA is initialized for this static call
    PaError err = Pa_Initialize();
    if (err != paNoError) return "Failed to initialize PortAudio";

    std::string list = Impl::list_devices();

    // We should only terminate if we initialized it just now and no other instances exist.
    // However, Pa_Terminate decrements a refcount, so it matches the Initialize above.
    Pa_Terminate();
    return list;
#else
    return "PortAudio not supported";
#endif
}

} // namespace apm
