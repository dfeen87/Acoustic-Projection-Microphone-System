#pragma once

#include <string>
#include <vector>

namespace apm {

/**
 * @brief Core APM System functionality
 */
class APMCore {
public:
    struct TextTranslationResult {
        std::string source_text;
        std::string translated_text;
        std::string source_language;
        std::string target_language;
        bool success{false};
        std::string error_message;
        int processing_time_ms{0};
    };

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
    bool initialize(int sample_rate = 48000, int num_channels = 1);

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
     * @brief Set source language code (e.g. "en")
     */
    void set_source_language(const std::string& lang);

    /**
     * @brief Set target language code (e.g. "es")
     */
    void set_target_language(const std::string& lang);

    /**
     * @brief Get current source language
     */
    const std::string& get_source_language() const;

    /**
     * @brief Get current target language
     */
    const std::string& get_target_language() const;

    /**
     * @brief Process audio buffer with DC removal + limiter
     * @param input Input audio samples
     * @return Processed audio samples
     */
    std::vector<float> process(const std::vector<float>& input);

    /**
     * @brief Translate text using the configured language pair
     * @param text Input text
     * @return Translation result
     */
    TextTranslationResult translate_text(const std::string& text) const;

private:
    bool initialized_ = false;
    int sample_rate_ = 0;
    int num_channels_ = 0;
    std::string source_language_ = "en";
    std::string target_language_ = "es";
    float dc_filter_coeff_ = 0.995f;
    float limiter_threshold_ = 0.98f;
    std::vector<float> dc_prev_input_;
    std::vector<float> dc_prev_output_;
};

} // namespace apm
