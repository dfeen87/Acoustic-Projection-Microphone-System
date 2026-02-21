#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <string>

namespace apm {

/**
 * @brief Real-time audio I/O using PortAudio
 */
class AudioDevice {
public:
    using AudioCallback = std::function<void(const std::vector<float>& input, std::vector<float>& output)>;

    struct Config {
        int sample_rate = 48000;
        int input_channels = 4;
        int output_channels = 2;
        int frames_per_buffer = 512;
        int input_device_index = -1;  // -1 for default
        int output_device_index = -1; // -1 for default
    };

    explicit AudioDevice(const Config& config);
    ~AudioDevice();

    // Prevent copying
    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    /**
     * @brief Start audio streaming
     * @return true on success
     */
    bool start();

    /**
     * @brief Stop audio streaming
     * @return true on success
     */
    bool stop();

    /**
     * @brief Check if stream is active
     */
    bool is_active() const;

    /**
     * @brief Set the callback for audio processing
     * The callback receives interleaved input samples and expects interleaved output samples.
     */
    void set_callback(AudioCallback callback);

    /**
     * @brief List available audio devices
     */
    static std::string list_devices();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace apm
