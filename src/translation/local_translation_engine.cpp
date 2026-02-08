#include "apm/local_translation_engine.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <memory>
#include <random>
#include <chrono>

// Simple JSON parsing (or use nlohmann/json if available)
namespace simple_json {
    std::string extract_field(const std::string& json, const std::string& field) {
        std::string search = "\"" + field + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        
        pos += search.length();
        // Skip whitespace and opening quote
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        
        return json.substr(pos, end - pos);
    }
    
    bool extract_bool(const std::string& json, const std::string& field) {
        std::string search = "\"" + field + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return false;
        
        pos += search.length();
        while (pos < json.length() && json[pos] == ' ') pos++;
        
        return (pos < json.length() && json[pos] == 't'); // true
    }
}

namespace apm {

// Helper: Generate unique temp filename
std::string generate_temp_filename() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    std::ostringstream oss;
    oss << "/tmp/apm_audio_" << ms << "_" << dis(gen) << ".wav";
    return oss.str();
}

// Helper: Write WAV file
bool write_wav_file(const std::string& filename, 
                   const std::vector<float>& samples, 
                   int sample_rate) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;
    
    // WAV header
    const int num_channels = 1;
    const int bits_per_sample = 16;
    const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const int block_align = num_channels * bits_per_sample / 8;
    const int data_size = samples.size() * sizeof(int16_t);
    const int file_size = 36 + data_size;
    
    // RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);
    
    // fmt chunk
    file.write("fmt ", 4);
    int fmt_size = 16;
    file.write(reinterpret_cast<const char*>(&fmt_size), 4);
    int16_t audio_format = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    int16_t num_ch = num_channels;
    file.write(reinterpret_cast<const char*>(&num_ch), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    int16_t blk_align = block_align;
    file.write(reinterpret_cast<const char*>(&blk_align), 2);
    int16_t bps = bits_per_sample;
    file.write(reinterpret_cast<const char*>(&bps), 2);
    
    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);
    
    // Convert float to int16 and write
    for (float sample : samples) {
        int16_t value = static_cast<int16_t>(sample * 32767.0f);
        file.write(reinterpret_cast<const char*>(&value), 2);
    }
    
    file.close();
    return true;
}

// Helper: Execute command and capture output
std::string exec_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    
    return result;
}

// Helper: Find Python executable
std::string find_python() {
    // Try common Python executables
    std::vector<std::string> python_cmds = {"python3", "python", "python3.9", "python3.10", "python3.11"};
    
    for (const auto& cmd : python_cmds) {
        std::string test_cmd = cmd + " --version 2>&1";
        try {
            std::string output = exec_command(test_cmd);
            if (output.find("Python 3") != std::string::npos) {
                return cmd;
            }
        } catch (...) {
            continue;
        }
    }
    
    return "python3"; // Default fallback
}

class LocalTranslationEngine::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {
        python_cmd_ = find_python();
        initialize_models();
    }

    ~Impl() {
        // Cleanup any temp files if needed
    }

    TranslationResult translate(const std::vector<float>& audio_samples,
                               int sample_rate) {
        TranslationResult result;
        result.source_language = config_.source_language;
        result.target_language = config_.target_language;

        try {
            // Step 1: Write audio to temp WAV file
            std::string temp_wav = generate_temp_filename();
            if (!write_wav_file(temp_wav, audio_samples, sample_rate)) {
                result.success = false;
                result.error_message = "Failed to write temporary WAV file";
                return result;
            }

            // Step 2: Call Python translation bridge
            std::ostringstream cmd;
            if (config_.offline_mode) {
                cmd << "APM_OFFLINE=1 TRANSFORMERS_OFFLINE=1 HF_HUB_OFFLINE=1 HF_DATASETS_OFFLINE=1 ";
            }
            cmd << python_cmd_ << " " << config_.script_path
                << " " << temp_wav
                << " --source " << config_.source_language
                << " --target " << config_.target_language
                << " --whisper-model " << config_.whisper_model_path
                << " --nllb-model " << config_.nllb_model_path
                << " --json"
                << " 2>/dev/null"; // Suppress stderr
            
            std::string json_output;
            try {
                json_output = exec_command(cmd.str());
            } catch (const std::exception& e) {
                std::remove(temp_wav.c_str());
                result.success = false;
                result.error_message = std::string("Failed to execute translation: ") + e.what();
                return result;
            }

            // Step 3: Parse JSON response
            result.transcribed_text = simple_json::extract_field(json_output, "transcribed_text");
            result.translated_text = simple_json::extract_field(json_output, "translated_text");
            result.success = simple_json::extract_bool(json_output, "success");
            
            if (!result.success) {
                result.error_message = "Translation failed - check if models are installed";
            } else {
                result.confidence = 0.95f; // Could parse from JSON if available
            }

            // Step 4: Cleanup
            std::remove(temp_wav.c_str());

        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = e.what();
        }

        return result;
    }

private:
    Config config_;
    std::string python_cmd_;

    void initialize_models() {
        std::cout << "Initializing local translation models..." << std::endl;
        std::cout << "  Python command: " << python_cmd_ << std::endl;
        std::cout << "  Script path: " << config_.script_path << std::endl;
        std::cout << "  Source language: " << config_.source_language << std::endl;
        std::cout << "  Target language: " << config_.target_language << std::endl;
        
        // Verify Python script exists
        std::ifstream script_file(config_.script_path);
        if (!script_file.good()) {
            std::cerr << "WARNING: Translation script not found at " 
                     << config_.script_path << std::endl;
            std::cerr << "Translation will fail until script is available." << std::endl;
        }
        
        // Optionally: Pre-warm the models by doing a dummy translation
        // This loads Whisper/NLLB into memory on first use
        std::cout << "Translation engine ready." << std::endl;
    }
};

LocalTranslationEngine::LocalTranslationEngine(const Config& config)
    : impl_(std::make_unique<Impl>(config)), config_(config) {
    ready_ = true;
}

LocalTranslationEngine::~LocalTranslationEngine() = default;

TranslationResult LocalTranslationEngine::translate(
    const std::vector<float>& audio_samples,
    int sample_rate) {
    if (!ready_) {
        TranslationResult result;
        result.success = false;
        result.error_message = "Translation engine not ready";
        return result;
    }
    return impl_->translate(audio_samples, sample_rate);
}

std::future<TranslationResult> LocalTranslationEngine::translate_async(
    const std::vector<float>& audio_samples,
    int sample_rate) {
    return std::async(std::launch::async, [this, audio_samples, sample_rate]() {
        return this->translate(audio_samples, sample_rate);
    });
}

std::vector<std::string> LocalTranslationEngine::get_supported_languages() const {
    // NLLB supports 200+ languages
    return {
        "en", "es", "fr", "de", "it", "pt", "nl", "pl", "ru",
        "zh", "ja", "ko", "ar", "hi", "tr", "sv", "no", "da",
        "fi", "cs", "el", "he", "th", "vi", "id", "ms", "tl"
        // ... and 170+ more
    };
}

} // namespace apm
