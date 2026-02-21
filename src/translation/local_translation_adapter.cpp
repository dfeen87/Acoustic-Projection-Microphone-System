#include "apm/translation/local_translation_adapter.h"
#include <iostream>
#include <vector>
#include <future>
#include <algorithm>

namespace apm {

LocalTranslationAdapter::LocalTranslationAdapter(const LocalTranslationEngine::Config& config)
    : engine_(std::make_unique<LocalTranslationEngine>(config)) {}

std::future<TranslationEngine::TranslationResult>
LocalTranslationAdapter::translate_async(const TranslationRequest& request) {

    // Extract samples from AudioFrame
    std::vector<float> samples;
    const int sample_rate = request.audio.sample_rate();
    const int channels = request.audio.channels();
    const size_t frames = request.audio.frame_count();

    if (frames == 0 || channels == 0) {
        // Return empty result
        return std::async(std::launch::async, []() {
            return TranslationResult{};
        });
    }

    if (channels > 1) {
        // Mixdown to mono for translation engine input
        auto data = request.audio.samples();
        samples.reserve(frames);
        for (size_t i = 0; i < frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < channels; ++ch) {
                sum += data[i * channels + ch];
            }
            samples.push_back(sum / static_cast<float>(channels));
        }
    } else {
        auto data = request.audio.samples();
        samples.assign(data.begin(), data.end());
    }

    // Launch async translation
    // We capture samples by value (copy)
    return std::async(std::launch::async, [this, samples, sample_rate]() -> TranslationResult {

        // This blocks the async thread, waiting for the engine to finish
        // Since engine->translate_async returns a future, we just get() it here
        // as we are already in an async context.
        auto local_future = engine_->translate_async(samples, sample_rate);
        auto local_result = local_future.get();

        TranslationResult result;
        result.source_text = local_result.transcribed_text;
        result.translated_text = local_result.translated_text;
        result.confidence = local_result.confidence;
        result.latency_ms = 0; // Unknown latency

        // Audio output is not yet supported by LocalTranslationEngine (TTS missing)
        // Return empty or silent frame
        result.translated_audio = AudioFrame(0, sample_rate, 1);

        return result;
    });
}

} // namespace apm
