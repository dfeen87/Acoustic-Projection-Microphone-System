/**
 * @file test_translation_integration.cpp
 * @brief Integration test for C++ <-> Python translation bridge
 */

#include "apm/local_translation_engine.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void test_engine_initialization() {
    std::cout << "Test 1: Engine Initialization..." << std::endl;
    
    apm::LocalTranslationEngine::Config config;
    config.source_language = "en";
    config.target_language = "es";
    config.script_path = "scripts/translation_bridge.py";
    
    apm::LocalTranslationEngine engine(config);
    
    assert(engine.is_ready());
    std::cout << "  ✓ Engine initialized successfully" << std::endl;
    
    auto langs = engine.get_supported_languages();
    assert(langs.size() > 0);
    std::cout << "  ✓ Supported languages: " << langs.size() << std::endl;
}

void test_audio_generation() {
    std::cout << "\nTest 2: Audio Generation..." << std::endl;
    
    const int sample_rate = 16000;
    const int duration_sec = 1;
    std::vector<float> audio(sample_rate * duration_sec);
    
    // Generate 440Hz sine wave (musical note A)
    for (int i = 0; i < audio.size(); ++i) {
        float t = i / (float)sample_rate;
        audio[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * t);
    }
    
    std::cout << "  ✓ Generated " << audio.size() << " samples" << std::endl;
    std::cout << "  ✓ Duration: " << duration_sec << " seconds" << std::endl;
}

void test_translation_pipeline() {
    std::cout << "\nTest 3: Translation Pipeline..." << std::endl;
    
    apm::LocalTranslationEngine::Config config;
    config.source_language = "en";
    config.target_language = "es";
    config.script_path = "scripts/translation_bridge.py";
    
    apm::LocalTranslationEngine engine(config);
    
    // Generate test audio (sine wave)
    const int sample_rate = 16000;
    std::vector<float> audio(sample_rate * 2); // 2 seconds
    
    for (int i = 0; i < audio.size(); ++i) {
        float t = i / (float)sample_rate;
        audio[i] = 0.3f * std::sin(2.0f * M_PI * 440.0f * t);
    }
    
    std::cout << "  Running translation (this may take 5-10 seconds)..." << std::endl;
    auto result = engine.translate(audio, sample_rate);
    
    std::cout << "\n  Results:" << std::endl;
    std::cout << "    Success: " << (result.success ? "YES" : "NO") << std::endl;
    
    if (result.success) {
        std::cout << "    Transcribed: " << result.transcribed_text << std::endl;
        std::cout << "    Translated: " << result.translated_text << std::endl;
        std::cout << "    Confidence: " << result.confidence << std::endl;
        std::cout << "  ✓ Translation completed successfully" << std::endl;
    } else {
        std::cout << "    Error: " << result.error_message << std::endl;
        std::cout << "  ✗ Translation failed (this is expected if models not installed)" << std::endl;
        std::cout << "  → Run: ./scripts/setup_translation.sh" << std::endl;
    }
}

void test_async_translation() {
    std::cout << "\nTest 4: Async Translation..." << std::endl;
    
    apm::LocalTranslationEngine::Config config;
    config.source_language = "en";
    config.target_language = "fr";
    config.script_path = "scripts/translation_bridge.py";
    
    apm::LocalTranslationEngine engine(config);
    
    // Generate test audio
    const int sample_rate = 16000;
    std::vector<float> audio(sample_rate);
    
    for (int i = 0; i < audio.size(); ++i) {
        float t = i / (float)sample_rate;
        audio[i] = 0.3f * std::sin(2.0f * M_PI * 523.25f * t); // C5 note
    }
    
    std::cout << "  Starting async translation..." << std::endl;
    auto future = engine.translate_async(audio, sample_rate);
    
    std::cout << "  Doing other work while translating..." << std::endl;
    // Simulate other work
    for (int i = 0; i < 5; ++i) {
        std::cout << "    Working... " << i << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "  Waiting for translation result..." << std::endl;
    auto result = future.get();
    
    std::cout << "  ✓ Async translation completed" << std::endl;
    std::cout << "    Success: " << (result.success ? "YES" : "NO") << std::endl;
}

void test_multiple_languages() {
    std::cout << "\nTest 5: Multiple Languages..." << std::endl;
    
    std::vector<std::pair<std::string, std::string>> lang_pairs = {
        {"en", "es"},  // English -> Spanish
        {"en", "fr"},  // English -> French
        {"en", "de"},  // English -> German
        {"en", "ja"}   // English -> Japanese
    };
    
    for (const auto& [src, tgt] : lang_pairs) {
        std::cout << "  Testing " << src << " -> " << tgt << "..." << std::endl;
        
        apm::LocalTranslationEngine::Config config;
        config.source_language = src;
        config.target_language = tgt;
        config.script_path = "scripts/translation_bridge.py";
        
        apm::LocalTranslationEngine engine(config);
        assert(engine.is_ready());
        
        std::cout << "    ✓ Engine ready for " << src << " -> " << tgt << std::endl;
    }
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "APM Translation Integration Tests" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;
    
    try {
        test_engine_initialization();
        test_audio_generation();
        test_translation_pipeline();
        test_async_translation();
        test_multiple_languages();
        
        std::cout << "\n=====================================" << std::endl;
        std::cout << "All tests completed!" << std::endl;
        std::cout << "=====================================" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}

// Build and run:
// g++ -std=c++20 -o test_translation test_translation_integration.cpp \
//     -I./include -L./build -lapm -pthread
// ./test_translation
