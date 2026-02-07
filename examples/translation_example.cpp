/**
 * @file translation_example.cpp
 * @brief Complete example showing APM System with real translation
 * 
 * This demonstrates the full pipeline:
 * 1. Capture multi-channel audio
 * 2. Apply beamforming
 * 3. Echo cancellation
 * 4. Noise suppression
 * 5. Voice activity detection
 * 6. Real translation (Whisper + NLLB)
 * 7. Directional audio projection
 */

#include "apm/apm_system.h"
#include "apm/local_translation_engine.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>

using namespace apm;

int main() {
    std::cout << "=== APM System with Local Translation Demo ===" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 1: Configure APM System
    // ========================================================================
    
    APMSystem::Config apm_config;
    apm_config.num_microphones = 4;
    apm_config.mic_spacing_m = 0.012f;  // 12mm spacing
    apm_config.num_speakers = 3;
    apm_config.speaker_spacing_m = 0.015f;
    apm_config.sample_rate = 48000;
    
    APMSystem apm(apm_config);
    std::cout << "✓ APM System initialized" << std::endl;

    // ========================================================================
    // STEP 2: Configure Local Translation Engine
    // ========================================================================
    
    LocalTranslationEngine::Config trans_config;
    trans_config.source_language = "en";
    trans_config.target_language = "es";
    trans_config.use_gpu = true;
    
    LocalTranslationEngine translator(trans_config);
    
    if (!translator.is_ready()) {
        std::cerr << "✗ Translation engine not ready" << std::endl;
        return 1;
    }
    std::cout << "✓ Translation engine initialized" << std::endl;
    std::cout << "  Source: " << trans_config.source_language << std::endl;
    std::cout << "  Target: " << trans_config.target_language << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 3: Simulate Audio Input (20ms frames at 48kHz)
    // ========================================================================
    
    const int frame_size = 960;  // 20ms at 48kHz
    const int num_frames = 100;  // Process 2 seconds of audio
    
    std::cout << "Processing audio stream..." << std::endl;
    std::cout << "  Frame size: " << frame_size << " samples" << std::endl;
    std::cout << "  Sample rate: " << apm_config.sample_rate << " Hz" << std::endl;
    std::cout << "  Duration: " << (num_frames * frame_size) / apm_config.sample_rate << " seconds" << std::endl;
    std::cout << std::endl;

    // Target angle: 30 degrees to the right
    constexpr float kPi = 3.14159265358979323846f;
    float target_angle = 30.0f * kPi / 180.0f;
    
    // Accumulate audio for translation
    std::vector<float> accumulated_audio;
    int speech_frame_count = 0;
    
    auto start_time = std::chrono::steady_clock::now();

    for (int frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
        
        // Create microphone array input
        std::vector<AudioFrame> mic_array;
        for (int mic = 0; mic < apm_config.num_microphones; ++mic) {
            AudioFrame frame(frame_size, apm_config.sample_rate, 1);
            auto samples = frame.samples();
            
            // Simulate audio: sine wave with noise
            for (int i = 0; i < frame_size; ++i) {
                float t = (frame_idx * frame_size + i) / (float)apm_config.sample_rate;
                float signal = 0.3f * std::sin(2.0f * kPi * 440.0f * t);  // A4 note
                float noise = 0.05f * (2.0f * (std::rand() / (float)RAND_MAX) - 1.0f);
                samples[i] = signal + noise;
            }
            
            frame.compute_metadata();
            mic_array.push_back(std::move(frame));
        }
        
        // Speaker reference for echo cancellation
        AudioFrame speaker_ref(frame_size, apm_config.sample_rate, 1);
        
        // ====================================================================
        // STEP 4: Process through APM pipeline (DSP only, no translation yet)
        // ====================================================================
        
        // This would normally call apm.process(), but we'll do it step by step
        // to show each stage and accumulate audio for translation
        
        BeamformingEngine beamformer(apm_config.num_microphones, apm_config.mic_spacing_m);
        auto beamformed = beamformer.delay_and_sum(mic_array, target_angle, 0.0f);
        
        EchoCancellationEngine echo_cancel(2048);
        auto echo_cancelled = echo_cancel.cancel_echo(beamformed, speaker_ref);
        
        NoiseSuppressionEngine noise_suppress;
        auto denoised = noise_suppress.suppress(echo_cancelled);
        
        VoiceActivityDetector vad;
        auto vad_result = vad.detect(denoised);
        
        // If speech detected, accumulate audio
        if (vad_result.speech_detected) {
            speech_frame_count++;
            auto samples = denoised.samples();
            accumulated_audio.insert(accumulated_audio.end(), samples.begin(), samples.end());
            
            if (frame_idx % 10 == 0) {
                std::cout << "  Frame " << frame_idx << ": Speech detected "
                         << "(confidence: " << vad_result.confidence << ")" << std::endl;
            }
        }
        
        // Small delay to simulate real-time processing
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto processing_time = std::chrono::steady_clock::now() - start_time;
    auto processing_ms = std::chrono::duration_cast<std::chrono::milliseconds>(processing_time).count();
    
    std::cout << std::endl;
    std::cout << "DSP Processing complete:" << std::endl;
    std::cout << "  Speech frames: " << speech_frame_count << "/" << num_frames << std::endl;
    std::cout << "  Processing time: " << processing_ms << " ms" << std::endl;
    std::cout << "  Real-time factor: " << (processing_ms / 2000.0f) << "x" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // STEP 5: Translate Accumulated Speech
    // ========================================================================
    
    if (!accumulated_audio.empty()) {
        std::cout << "Starting translation..." << std::endl;
        std::cout << "  Audio samples: " << accumulated_audio.size() << std::endl;
        std::cout << "  Duration: " << accumulated_audio.size() / (float)apm_config.sample_rate << " seconds" << std::endl;
        
        auto trans_start = std::chrono::steady_clock::now();
        
        // Translate (this calls Python Whisper + NLLB)
        auto result = translator.translate(accumulated_audio, apm_config.sample_rate);
        
        auto trans_time = std::chrono::steady_clock::now() - trans_start;
        auto trans_ms = std::chrono::duration_cast<std::chrono::milliseconds>(trans_time).count();
        
        std::cout << std::endl;
        std::cout << "=== Translation Results ===" << std::endl;
        std::cout << "Status: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
        
        if (result.success) {
            std::cout << std::endl;
            std::cout << "Transcribed (" << result.source_language << "):" << std::endl;
            std::cout << "  " << result.transcribed_text << std::endl;
            std::cout << std::endl;
            std::cout << "Translated (" << result.target_language << "):" << std::endl;
            std::cout << "  " << result.translated_text << std::endl;
            std::cout << std::endl;
            std::cout << "Confidence: " << (result.confidence * 100.0f) << "%" << std::endl;
            std::cout << "Translation time: " << trans_ms << " ms" << std::endl;
        } else {
            std::cout << "Error: " << result.error_message << std::endl;
            std::cout << std::endl;
            std::cout << "Make sure you've run: ./scripts/setup_translation.sh" << std::endl;
        }
        
        // ====================================================================
        // STEP 6: Project Translated Audio (placeholder - would use TTS)
        // ====================================================================
        
        if (result.success) {
            std::cout << std::endl;
            std::cout << "Projecting translated audio to speaker array..." << std::endl;
            
            DirectionalProjector projector(apm_config.num_speakers, apm_config.speaker_spacing_m);
            
            // In production, you'd convert result.translated_text to speech using TTS
            // For now, we'll project the original audio as a demo
            AudioFrame audio_to_project(accumulated_audio.size(), apm_config.sample_rate, 1);
            auto proj_samples = audio_to_project.samples();
            std::copy(accumulated_audio.begin(), accumulated_audio.end(), proj_samples.begin());
            
            auto speaker_signals = projector.create_projection_signals(
                audio_to_project,
                target_angle,
                1.5f  // 1.5 meters distance
            );
            
            std::cout << "✓ Generated " << speaker_signals.size() << " speaker signals" << std::endl;
            std::cout << "✓ Projection angle: " << (target_angle * 180.0f / M_PI) << " degrees" << std::endl;
        }
        
    } else {
        std::cout << "No speech detected in audio stream" << std::endl;
    }

    std::cout << std::endl;
    std::cout << "=== Demo Complete ===" << std::endl;
    
    return 0;
}

// Compile with:
// g++ -std=c++20 -o translation_example translation_example.cpp \
//     -I./include -L./build -lapm -pthread

// Run with:
// ./translation_example
