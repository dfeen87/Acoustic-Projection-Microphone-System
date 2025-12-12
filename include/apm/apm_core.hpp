#pragma once

#include <string>
#include <vector>

namespace apm {

/**
 * @brief Core APM System functionality
 */
class APMCore {
public:
    APMCore() = default;
    ~APMCore() = default;

    /**
     * @brief Get the version string
     * @return Version string
     */
    std::string get_version() const;

    /**
     * @brief Initialize the APM system
     * @param sample_rate Sample rate in Hz
     * @param num_channels Number of audio channels
     * @return true if successful, false otherwise
     */
    bool initialize(int sample_rate, int num_channels);

    /**
     * @brief Check if system is initialized
     * @return true if initialized
     */
    bool is_initialized() const { return initialized_; }

    /**
     * @brief Get sample rate
     * @return Sample rate in Hz
     */
    int get_sample_rate() const { return sample_rate_; }

    /**
     * @brief Get number of channels
     * @return Number of channels
     */
    int get_num_channels() const { return num_channels_; }

    /**
     * @brief Process audio buffer (placeholder)
     * @param input Input audio samples
     * @return Processed audio samples
     */
    std::vector<float> process(const std::vector<float>& input);

private:
    bool initialized_ = false;
    int sample_rate_ = 0;
    int num_channels_ = 0;
};

} // namespace apm
