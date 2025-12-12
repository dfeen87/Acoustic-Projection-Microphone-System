#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>

namespace apm {

/**
 * @brief Result from translation operation
 */
struct TranslationResult {
    std::string transcribed_text;     // Original speech-to-text
    std::string translated_text;      // Translated text
    std::string source_language;      // Detected/specified source language
    std::string target_language;      // Target language
    float confidence;                 // Confidence score (0.0-1.0)
    bool success;                     // Operation success flag
    std::string error_message;        // Error details if failed
};

/**
 * @brief Local translation engine using Whisper + NLLB
 * 
 * Completely local processing - no cloud APIs, fully private
 */
class LocalTranslationEngine {
public:
    struct Config {
        std::string whisper_model_path = "./models/whisper-small";
        std::string nllb_model_path = "./models/nllb-200-distilled-600M";
        std::string source_language = "en";
        std::string target_language = "es";
        bool use_gpu = true;
        int num_threads = 4;
    };

    explicit LocalTranslationEngine(const Config& config);
    ~LocalTranslationEngine();

    // Synchronous translation
    TranslationResult translate(const std::vector<float>& audio_samples,
                               int sample_rate);

    // Asynchronous translation
    std::future<TranslationResult> translate_async(
        const std::vector<float>& audio_samples,
        int sample_rate);

    // Check if models are loaded
    bool is_ready() const { return ready_; }

    // Get supported languages
    std::vector<std::string> get_supported_languages() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
    Config config_;
};

} // namespace apm
