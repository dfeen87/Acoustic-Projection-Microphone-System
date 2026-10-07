#include "apm/translation/local_translation_engine.hpp"
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
#include <climits>
#include <filesystem>
#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

namespace apm {

std::string shell_argument(const std::string& value) {
    if (value.find('\0') != std::string::npos) throw std::invalid_argument("NUL in translation argument");
#ifdef _WIN32
    // cmd.exe expands these characters even in quoted strings. Reject
    // unsupported arguments rather than interpreting them as commands.
    if (value.find_first_of("\"%!?^&|<>\r\n") != std::string::npos) {
        throw std::invalid_argument("Unsupported Windows translation argument");
    }
    return "\"" + value + "\"";
#else
    std::string quoted = "'";
    for (char ch : value) quoted += ch == '\'' ? "'\"'\"'" : std::string(1, ch);
    return quoted + "'";
#endif
}

std::string generate_temp_filename() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    // Use a wider range for better uniqueness
    std::uniform_int_distribution<uint32_t> dis(100000, 999999);

    // Use std::filesystem::temp_directory_path() for cross-platform support.
    // Caller (translate()) is responsible for removing the file after use.
    std::filesystem::path tmp_path =
        std::filesystem::temp_directory_path() /
        ("apm_audio_" + std::to_string(ms) + "_" + std::to_string(dis(gen)) + ".wav");
    return tmp_path.string();
}

bool write_wav_file(const std::string& filename,
                   const std::vector<float>& samples,
                   int sample_rate) {
    // Guard against WAV header overflow: the data chunk size and file size are
    // stored as uint32_t in the WAV header, so they must not exceed UINT32_MAX.
    if (sample_rate <= 0 || sample_rate > INT_MAX / 2) return false;
    if (samples.size() > (static_cast<size_t>(UINT32_MAX) - 36) / sizeof(int16_t)) {
        std::cerr << "ERROR: Audio data too large for WAV format ("
                  << samples.size() << " samples)" << std::endl;
        return false;
    }
    const size_t raw_data_bytes = samples.size() * sizeof(int16_t);

    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    const int num_channels = 1;
    const int bits_per_sample = 16;
    const int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    const int block_align = num_channels * bits_per_sample / 8;
    const uint32_t data_size = static_cast<uint32_t>(raw_data_bytes);
    const uint32_t file_size = 36 + data_size;

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);

    file.write("fmt ", 4);
    int fmt_size = 16;
    file.write(reinterpret_cast<const char*>(&fmt_size), 4);
    int16_t audio_format = 1;
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    int16_t num_ch = num_channels;
    file.write(reinterpret_cast<const char*>(&num_ch), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    int16_t blk_align = block_align;
    file.write(reinterpret_cast<const char*>(&blk_align), 2);
    int16_t bps = bits_per_sample;
    file.write(reinterpret_cast<const char*>(&bps), 2);

    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);

    for (float sample : samples) {
        if (!std::isfinite(sample)) sample = 0.0f;
        int16_t value = static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
        file.write(reinterpret_cast<const char*>(&value), 2);
    }

    file.close();
    return file.good();
}

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
    const bool read_failed = ferror(pipe.get()) != 0;
    const int status = pclose(pipe.release());
    if (read_failed || status != 0) throw std::runtime_error("Translation subprocess failed");

    return result;
}

std::string find_python() {
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

    return {};
}

class LocalTranslationEngine::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {
        python_cmd_ = find_python();
        initialize_models();
    }

    ~Impl() = default;

    bool available() const {
        return !python_cmd_.empty() && std::filesystem::is_regular_file(config_.script_path);
    }

    TranslationResult translate(const std::vector<float>& audio_samples,
                               int sample_rate) {
        TranslationResult result;
        result.source_language = config_.source_language;
        result.target_language = config_.target_language;

        try {
            struct TemporaryWav {
                std::string path;
                ~TemporaryWav() { std::remove(path.c_str()); }
            } temporary{generate_temp_filename()};
            const auto& temp_wav = temporary.path;
            if (!write_wav_file(temp_wav, audio_samples, sample_rate)) {
                result.success = false;
                result.error_message = "Failed to write temporary WAV file";
                return result;
            }

            std::ostringstream cmd;
            if (config_.offline_mode) {
                cmd << "APM_OFFLINE=1 TRANSFORMERS_OFFLINE=1 HF_HUB_OFFLINE=1 HF_DATASETS_OFFLINE=1 ";
            }
            cmd << shell_argument(python_cmd_) << " " << shell_argument(config_.script_path)
                << " " << shell_argument(temp_wav)
                << " --source " << shell_argument(config_.source_language)
                << " --target " << shell_argument(config_.target_language)
                << " --whisper-model " << shell_argument(config_.whisper_model_path)
                << " --nllb-model " << shell_argument(config_.nllb_model_path);
            if (!config_.use_gpu) {
                cmd << " --device cpu";
            }
            cmd << " --json"
                << " 2>/dev/null";

            std::string json_output;
            try {
                json_output = exec_command(cmd.str());
            } catch (const std::exception& e) {
                result.success = false;
                result.error_message = std::string("Failed to execute translation: ") + e.what();
                return result;
            }

            const auto payload = nlohmann::json::parse(json_output);
            if (!payload.is_object() || !payload.contains("success") || !payload["success"].is_boolean()
                    || !payload["success"].get<bool>() || !payload.contains("translated_text")
                    || !payload["translated_text"].is_string()) {
                result.error_message = "Invalid or unsuccessful translation bridge response";
                return result;
            }
            result.transcribed_text = payload.value("transcribed_text", std::string{});
            result.translated_text = payload["translated_text"].get<std::string>();
            result.success = result.translated_text.find_first_not_of(" \t\r\n") != std::string::npos;

            if (!result.success) {
                result.error_message = "Translation failed - check if models are installed";
            } else {
                result.confidence = 0.95f;
            }


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
        std::cout << "  Whisper model: " << config_.whisper_model_path << std::endl;
        std::cout << "  NLLB model: " << config_.nllb_model_path << std::endl;
        std::cout << "  Source language: " << config_.source_language << std::endl;
        std::cout << "  Target language: " << config_.target_language << std::endl;
        std::cout << "  Offline mode: " << (config_.offline_mode ? "enabled" : "disabled") << std::endl;

        std::ifstream script_file(config_.script_path);
        if (!script_file.good()) {
            std::cerr << "WARNING: Translation script not found at "
                     << config_.script_path << std::endl;
            std::cerr << "Translation will fail until script is available." << std::endl;
        }

        std::cout << "Translation engine ready." << std::endl;
    }
};

LocalTranslationEngine::LocalTranslationEngine(const Config& config)
    : impl_(std::make_unique<Impl>(config)), config_(config) {
    ready_ = impl_->available();
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
    return {
        "en", "es", "fr", "de", "it", "pt", "nl", "pl", "ru",
        "zh", "ja", "ko", "ar", "hi", "tr", "sv", "no", "da",
        "fi", "cs", "el", "he", "th", "vi", "id", "ms", "tl"
    };
}

} // namespace apm
