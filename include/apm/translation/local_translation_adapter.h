#pragma once

#include "apm/apm_system.h"
#include "apm/translation/local_translation_engine.hpp"
#include <memory>
#include <future>

namespace apm {

/**
 * @brief Adapter to adapt LocalTranslationEngine to the TranslationEngine interface
 */
class LocalTranslationAdapter : public TranslationEngine {
public:
    explicit LocalTranslationAdapter(const LocalTranslationEngine::Config& config);

    std::future<TranslationResult> translate_async(
        const TranslationRequest& request) override;

private:
    std::unique_ptr<LocalTranslationEngine> engine_;
};

} // namespace apm
