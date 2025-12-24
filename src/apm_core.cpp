#include "apm/apm_core.hpp"
#include "apm/config.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <unordered_map>

namespace apm {
namespace {

std::string trim_copy(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = input.find_last_not_of(" \t\n\r\f\v");
    return input.substr(first, last - first + 1);
}

std::string to_lower_copy(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (unsigned char ch : input) {
        output.push_back(static_cast<char>(std::tolower(ch)));
    }
    return output;
}

bool is_all_upper(const std::string& input) {
    bool has_alpha = false;
    for (unsigned char ch : input) {
        if (std::isalpha(ch)) {
            has_alpha = true;
            if (!std::isupper(ch)) {
                return false;
            }
        }
    }
    return has_alpha;
}

bool is_title_case(const std::string& input) {
    bool first_alpha = true;
    bool has_alpha = false;
    for (unsigned char ch : input) {
        if (!std::isalpha(ch)) {
            continue;
        }
        has_alpha = true;
        if (first_alpha) {
            if (!std::isupper(ch)) {
                return false;
            }
            first_alpha = false;
        } else if (std::isupper(ch)) {
            return false;
        }
    }
    return has_alpha;
}

std::string apply_capitalization(const std::string& original,
                                 const std::string& translated) {
    if (translated.empty()) {
        return translated;
    }

    if (is_all_upper(original)) {
        std::string out = translated;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return out;
    }

    if (is_title_case(original)) {
        std::string out = translated;
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
        return out;
    }

    return translated;
}

std::string translate_word(const std::string& word,
                           const std::unordered_map<std::string, std::string>& dict) {
    const auto lower = to_lower_copy(word);
    const auto it = dict.find(lower);
    if (it == dict.end()) {
        return word;
    }
    return apply_capitalization(word, it->second);
}

std::string translate_text_with_dictionary(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& dict) {

    std::string result;
    result.reserve(text.size());

    std::string token;
    token.reserve(16);

    auto flush_token = [&]() {
        if (!token.empty()) {
            result += translate_word(token, dict);
            token.clear();
        }
    };

    for (unsigned char ch : text) {
        if (std::isalpha(ch) || ch == '\'' || ch == '-') {
            token.push_back(static_cast<char>(ch));
        } else {
            flush_token();
            result.push_back(static_cast<char>(ch));
        }
    }

    flush_token();
    return result;
}

const std::unordered_map<std::string, std::string>& phrasebook_en_es() {
    static const std::unordered_map<std::string, std::string> phrases{
        {"hello, how can i help you today?",
         "Hola, ¿cómo puedo ayudarte hoy?"},
        {"welcome to the encrypted translation system!",
         "¡Bienvenido al sistema de traducción cifrada!"}
    };
    return phrases;
}

const std::unordered_map<std::string, std::string>& phrasebook_en_fr() {
    static const std::unordered_map<std::string, std::string> phrases{
        {"hello, how can i help you today?",
         "Bonjour, comment puis-je vous aider aujourd'hui ?"},
        {"welcome to the encrypted translation system!",
         "Bienvenue dans le système de traduction chiffré !"}
    };
    return phrases;
}

const std::unordered_map<std::string, std::string>& dictionary_en_es() {
    static const std::unordered_map<std::string, std::string> dict{
        {"hello", "hola"},
        {"how", "cómo"},
        {"can", "puedo"},
        {"i", "yo"},
        {"help", "ayudar"},
        {"you", "tú"},
        {"today", "hoy"},
        {"welcome", "bienvenido"},
        {"to", "a"},
        {"the", "el"},
        {"encrypted", "cifrado"},
        {"translation", "traducción"},
        {"system", "sistema"},
        {"this", "este"},
        {"is", "es"},
        {"please", "por favor"}
    };
    return dict;
}

const std::unordered_map<std::string, std::string>& dictionary_en_fr() {
    static const std::unordered_map<std::string, std::string> dict{
        {"hello", "bonjour"},
        {"how", "comment"},
        {"can", "peux"},
        {"i", "je"},
        {"help", "aider"},
        {"you", "vous"},
        {"today", "aujourd'hui"},
        {"welcome", "bienvenue"},
        {"to", "à"},
        {"the", "le"},
        {"encrypted", "chiffré"},
        {"translation", "traduction"},
        {"system", "système"},
        {"this", "ce"},
        {"is", "est"},
        {"please", "s'il vous plaît"}
    };
    return dict;
}

} // namespace

std::string APMCore::get_version() const {
    return config::version;
}

bool APMCore::initialize(int sample_rate, int num_channels) {
    if (sample_rate <= 0 || num_channels <= 0) {
        initialized_ = false;
        return false;
    }

    sample_rate_ = sample_rate;
    num_channels_ = num_channels;
    dc_prev_input_.assign(num_channels_, 0.0f);
    dc_prev_output_.assign(num_channels_, 0.0f);
    initialized_ = true;
    return true;
}

void APMCore::set_source_language(const std::string& lang) {
    if (!lang.empty()) {
        source_language_ = lang;
    }
}

void APMCore::set_target_language(const std::string& lang) {
    if (!lang.empty()) {
        target_language_ = lang;
    }
}

const std::string& APMCore::get_source_language() const {
    return source_language_;
}

const std::string& APMCore::get_target_language() const {
    return target_language_;
}

std::vector<float> APMCore::process(const std::vector<float>& input) {
    if (!initialized_ || input.empty()) {
        return {};
    }

    if (num_channels_ <= 0 || input.size() % static_cast<size_t>(num_channels_) != 0) {
        return {};
    }

    std::vector<float> output = input;
    const size_t frame_count = output.size() / static_cast<size_t>(num_channels_);

    for (int ch = 0; ch < num_channels_; ++ch) {
        float prev_x = dc_prev_input_[ch];
        float prev_y = dc_prev_output_[ch];

        for (size_t frame = 0; frame < frame_count; ++frame) {
            const size_t idx = frame * static_cast<size_t>(num_channels_) + ch;
            float x = output[idx];
            if (!std::isfinite(x)) {
                x = 0.0f;
            }

            float y = x - prev_x + dc_filter_coeff_ * prev_y;
            prev_x = x;
            prev_y = y;

            float limited = std::tanh(y / limiter_threshold_) * limiter_threshold_;
            output[idx] = limited;
        }

        dc_prev_input_[ch] = prev_x;
        dc_prev_output_[ch] = prev_y;
    }

    return output;
}

APMCore::TextTranslationResult APMCore::translate_text(
    const std::string& text) const {

    TextTranslationResult result;
    result.source_text = text;
    result.source_language = source_language_;
    result.target_language = target_language_;

    const auto start = std::chrono::steady_clock::now();

    const std::string trimmed = trim_copy(text);
    if (trimmed.empty()) {
        result.success = false;
        result.error_message = "Input text is empty";
    } else if (source_language_ == target_language_) {
        result.translated_text = text;
        result.success = true;
    } else if (source_language_ == "en" && target_language_ == "es") {
        const auto normalized = to_lower_copy(trimmed);
        const auto& phrases = phrasebook_en_es();
        const auto phrase_it = phrases.find(normalized);
        if (phrase_it != phrases.end()) {
            result.translated_text = phrase_it->second;
        } else {
            result.translated_text =
                translate_text_with_dictionary(text, dictionary_en_es());
        }
        result.success = true;
    } else if (source_language_ == "en" && target_language_ == "fr") {
        const auto normalized = to_lower_copy(trimmed);
        const auto& phrases = phrasebook_en_fr();
        const auto phrase_it = phrases.find(normalized);
        if (phrase_it != phrases.end()) {
            result.translated_text = phrase_it->second;
        } else {
            result.translated_text =
                translate_text_with_dictionary(text, dictionary_en_fr());
        }
        result.success = true;
    } else {
        result.success = false;
        result.error_message =
            "Translation pair not supported in text-only fallback";
    }

    const auto end = std::chrono::steady_clock::now();
    result.processing_time_ms =
        static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count());

    return result;
}

} // namespace apm
