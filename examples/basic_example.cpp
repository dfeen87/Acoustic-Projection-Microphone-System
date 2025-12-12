// ============================================================================
// Basic APM System Example
// Demonstrates simple usage of the APM system
// ============================================================================

#include "apm/apm_system.h"
#include <iostream>
#include <cmath>

int main() {
    using namespace apm;
    
    std::cout << "======================================\n";
    std::cout << "  APM System - Basic Example\n";
    std::cout << "======================================\n\n";
    
    // ========================================================================
    // STEP 1: Configure the system
    // ========================================================================
    
    std::cout << "[1] Configuring APM System...\n";
    
    APMSystem::Config config;
    config.num_microphones = 4;        // 4-mic array
    config.mic_spacing_m = 0.012f;     // 12mm spacing
    config.num_speakers = 3;           // 3 speakers for projection
    config.speaker_spacing_m = 0.015f; // 15mm spacing
    config.source_language = "en-US";  // English input
    config.target_language = "es-ES";  // Spanish output
    
    std::cout << "   - Microphones: " << config.num_microphones << "\n";
    std::cout << "   - Speakers: " << config.num_speakers << "\n";
    std::cout << "   - Translation: " << config.source_language 
              << " → " << config.target_language << "\n\n";
    
    // ========================================================================
    // STEP 2: Initialize the system
    // ========================================================================
    
    std::cout << "[2] Initializing APM System...\n";
    
    APMSystem apm(config);
    
    std::cout << "   ✓ System initialized\n\n";
    
    // ========================================================================
    // STEP 3: Create test audio input
    // ========================================================================
    
    std::cout << "[3] Creating test audio (20ms frame at 48kHz)...\n";
    
    const int sample_rate = 48000;
    const int frame_size = 960; // 20ms at 48kHz
    
    // Create 4-microphone array input
    std::vector<AudioFrame> mic_array;
    
    for (int mic = 0; mic < 4; ++mic) {
        AudioFrame frame(frame_size, sample_rate, 1);
        auto samples = frame.samples();
        
        // Generate test signal: 440 Hz sine wave (musical note A4)
        // Each mic has slight phase shift to simulate directional source
        float phase_shift = mic * 0.1f;
        
        for (size_t i = 0; i < samples.size(); ++i) {
            float t = static_cast<float>(i) / sample_rate;
            samples[i] = 0.3f * std::sin(2.0f * M_PI * 440.0f * t + phase_shift);
        }
        
        frame.compute_metadata();
        
        std::cout << "   Mic " << mic << ": "
                  << "Peak=" << frame.metadata().peak_db << " dB, "
                  << "RMS=" << frame.metadata().rms_db << " dB\n";
        
        mic_array.push_back(std::move(frame));
    }
    
    // Create speaker reference (for echo cancellation)
    AudioFrame speaker_ref(frame_size, sample_rate, 1);
    // Leave it silent for this example
    
    std::cout << "\n";
    
    // ========================================================================
    // STEP 4: Process audio through the pipeline
    // ========================================================================
    
    std::cout << "[4] Processing audio through APM pipeline...\n";
    std::cout << "   Pipeline stages:\n";
    std::cout << "   1. Beamforming (spatial filtering)\n";
    std::cout << "   2. Echo Cancellation\n";
    std::cout << "   3. Noise Suppression\n";
    std::cout << "   4. Voice Activity Detection\n";
    std::cout << "   5. Translation (mock)\n";
    std::cout << "   6. Directional Projection\n\n";
    
    // Target direction: 30 degrees to the right
    float target_angle_deg = 30.0f;
    float target_angle_rad = target_angle_deg * M_PI / 180.0f;
    
    std::cout << "   Target direction: " << target_angle_deg << "°\n";
    
    auto start_time = std::chrono::steady_clock::now();
    
    auto output_signals = apm.process(
        mic_array,
        speaker_ref,
        target_angle_rad
    );
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();
    
    std::cout << "   Processing time: " << duration_ms << " ms\n";
    std::cout << "   Real-time factor: " << (20.0f / duration_ms) << "x\n\n";
    
    // ========================================================================
    // STEP 5: Display results
    // ========================================================================
    
    std::cout << "[5] Results:\n";
    
    if (output_signals.empty()) {
        std::cout << "   ⚠ No speech detected (VAD threshold not met)\n";
        std::cout << "   This is normal for pure sine wave test signals.\n";
    } else {
        std::cout << "   ✓ Generated " << output_signals.size() 
                  << " speaker output signals\n\n";
        
        for (size_t i = 0; i < output_signals.size(); ++i) {
            output_signals[i].compute_metadata();
            
            std::cout << "   Speaker " << i << ":\n";
            std::cout << "      Samples: " << output_signals[i].frame_count() << "\n";
            std::cout << "      Peak: " << output_signals[i].metadata().peak_db << " dB\n";
            std::cout << "      RMS: " << output_signals[i].metadata().rms_db << " dB\n";
            
            if (output_signals[i].metadata().clipping) {
                std::cout << "      ⚠ CLIPPING DETECTED\n";
            }
        }
    }
    
    std::cout << "\n";
    
    // ========================================================================
    // STEP 6: Demonstrate async processing
    // ========================================================================
    
    std::cout << "[6] Demonstrating async processing...\n";
    
    auto future = apm.process_async(mic_array, speaker_ref, target_angle_rad);
    
    std::cout << "   Processing in background...\n";
    
    // You can do other work here while processing happens
    std::cout << "   Waiting for completion...\n";
    
    auto async_outputs = future.get();
    
    std::cout << "   ✓ Async processing complete\n";
    std::cout << "   Generated " << async_outputs.size() << " outputs\n\n";
    
    // ========================================================================
    // STEP 7: Reset and cleanup
    // ========================================================================
    
    std::cout << "[7] Cleaning up...\n";
    
    apm.reset_all();
    
    std::cout << "   ✓ System reset\n\n";
    
    // ========================================================================
    // Summary
    // ========================================================================
    
    std::cout << "======================================\n";
    std::cout << "  Example Complete!\n";
    std::cout << "======================================\n\n";
    
    std::cout << "Next steps:\n";
    std::cout << "  • Connect real audio hardware\n";
    std::cout << "  • Integrate translation models\n";
    std::cout << "  • Calibrate microphone array\n";
    std::cout << "  • Tune DSP parameters\n";
    std::cout << "\n";
    
    std::cout << "See more examples:\n";
    std::cout << "  • ptt_call_example      - PTT and call signaling\n";
    std::cout << "  • translation_example   - Full translation pipeline\n";
    std::cout << "  • encrypted_translation - Secure communications\n";
    std::cout << "\n";
    
    return 0;
}
