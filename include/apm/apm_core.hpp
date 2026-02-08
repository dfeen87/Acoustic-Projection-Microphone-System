#pragma once

#include <string>
#include <vector>

namespace apm {

/**
 * @file apm_core.hpp
 * @brief Stable public API for the Acoustic Projection Microphone (APM) system.
 *
 * This header defines the APMCore facade, which provides a simplified,
 * production-safe interface for initializing the system, processing audio,
 * and performing text translation.
 *
 * This file is part of the guaranteed public API.
 * Backward compatibility is maintained across minor releases.
 */
class APMCore {
public:
    /**
     * @brief Result structure for text translation.
     */
    struct TextTranslationResult {
        std::string source_text;
        std::string translated_text;
        std::string source_language;
        std::string target_language;
        bool success{false};
        std::string error_message;
        int processing_time_ms{0};
    };

    /**
     * @brief Construct an uninitialized APMCore instance.
     */
    APMCore() = default;

    /**
     * @brief Destructor.
     */
    ~APMCore() = default;

    /**
     * @brief Get the APM library version string.
     * @return Version string (e.g. "4.0.0")
     */
    std::string get_version() const;

    /**
     * @brief Initialize the APM system.
     *
     * Must be called before processing audio or performing translations.
     *
     * @param sample_rate Sample rate in Hz (default: 48000)
     * @param num_channels Number of input audio channels (default: 1)
     * @return true if initialization succeeds, false otherwise
     */
    bool initialize(int sample_rate = 48000, int num_channels = 1);

    /**
     * @brief Check whether the system is initialized.
     * @return true if initialized
     */
    bool is_initialized() const { return initialized_; }

    /**
     * @brief Get the configured sample rate.
     * @return Sample rate in Hz
     */
    int get_sample_rate() const { return sample_rate_; }

    /**
     * @brief Get the configured number of audio channels.
     * @return Number of channels
     */
    int get_num_channels() const { return num_channels_; }

    /**
     * @brief Set the source language code.
     *
     * Example: "en", "en-US"
     *
     * @param lang Source language code
     */
    void set_source_language(const std::string& lang);

    /**
     * @brief Set the target language code.
     *
     * Example: "es", "fr"
     *
     * @param lang Target language code
     */
    void set_target_language(const std::string& lang);

    /**
     * @brief Get the current source language code.
     */
    const std::string& get_source_language() const;

    /**
     * @brief Get the current target language code.
     */
    const std::string& get_target_language() const;

    /**
     * @brief Process an audio buffer.
     *
     * Applies lightweight DC offset removal and peak limiting.
     * This function is real-time safe and does not allocate internally
     * once initialized.
     *
     * @param input Interleaved mono audio samples
     * @return Processed audio samples
     */
    std::vector<float> process(const std::vector<float>& input);

    /**
     * @brief Translate text using the configured language pair.
     *
     * If no translation engine is available, a mock or pass-through
     * result may be returned depending on build configuration.
     *
     * @param text Input text
     * @return Translation result structure
     */
    TextTranslationResult translate_text(const std::string& text) const;

private:
    // Initialization state
    bool initialized_{false};

    // Audio configuration
    int sample_rate_{0};
    int num_channels_{0};

    // Language configuration
    std::string source_language_{"en"};
    std::string target_language_{"es"};

    // DSP parameters
    float dc_filter_coeff_{0.995f};
    float limiter_threshold_{0.98f};

    // DC filter state (per channel)
    std::vector<float> dc_prev_input_;
    std::vector<float> dc_prev_output_;
};

} // namespace apm
