#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>

namespace apm {

/**
 * @brief Result of a translation operation
 */
struct TranslationResult {
    bool success{false};
    std::string transcribed_text;
    std::string translated_text;
    std::string source_language;
    std::string target_language;
    float confidence{0.0f};
    std::string error_message;
};

/**
 * @brief Local translation engine using Whisper + NLLB
 * 
 * This engine provides 100% local speech-to-speech translation
 * using OpenAI Whisper for transcription and Meta NLLB for translation.
 * No cloud APIs required - all processing happens on device.
 */
class LocalTranslationEngine {
public:
    /**
     * @brief Configuration for the translation engine
     */
    struct Config {
        std::string source_language{"en"};
        std::string target_language{"es"};
        std::string script_path{"scripts/translation_bridge.py"};
        std::string whisper_model_path{"small"};
        std::string nllb_model_path{"facebook/nllb-200-distilled-600M"};
        bool use_gpu{true};
        bool offline_mode{true};
    };

    /**
     * @brief Construct a new Local Translation Engine
     * 
     * @param config Configuration settings
     */
    explicit LocalTranslationEngine(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~LocalTranslationEngine();

    /**
     * @brief Translate audio samples (blocking)
     * 
     * @param audio_samples Audio data as float samples (-1.0 to 1.0)
     * @param sample_rate Sample rate in Hz (e.g., 16000, 48000)
     * @return TranslationResult containing transcribed and translated text
     */
    TranslationResult translate(const std::vector<float>& audio_samples,
                               int sample_rate);

    /**
     * @brief Translate audio samples (non-blocking)
     * 
     * @param audio_samples Audio data as float samples (-1.0 to 1.0)
     * @param sample_rate Sample rate in Hz (e.g., 16000, 48000)
     * @return std::future<TranslationResult> Future containing the result
     */
    std::future<TranslationResult> translate_async(
        const std::vector<float>& audio_samples,
        int sample_rate);

    /**
     * @brief Get list of supported language codes
     * 
     * @return std::vector<std::string> ISO 639-1 language codes
     */
    std::vector<std::string> get_supported_languages() const;

    /**
     * @brief Check if engine is ready
     * 
     * @return true if ready to translate
     */
    bool is_ready() const { return ready_; }

    /**
     * @brief Get current configuration
     * 
     * @return const Config& Current configuration
     */
    const Config& get_config() const { return config_; }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    Config config_;
    bool ready_{false};
};

} // namespace apm
