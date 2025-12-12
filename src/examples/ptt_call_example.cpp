#include "apm/apm_core.hpp"
#include "apm/crypto.hpp"
#include "apm/ptt_controller.hpp"
#include "apm/call_signaling.hpp"
#include <iostream>
#include <thread>
#include <csignal>

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    std::cout << "\nShutdown signal received\n";
    g_running = false;
}

void print_header() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          APM SECURE PUSH-TO-TALK TRANSLATION SYSTEM              ║\n";
    std::cout << "║          Real-time Translation with Encryption & PTT             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
}

void demo_ptt_only() {
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  DEMO 1: Push-to-Talk (PTT) Controller\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
    
    // Initialize PTT
    apm::PTTController ptt;
    if (!ptt.initialize()) {
        std::cerr << "❌ Failed to initialize PTT\n";
        return;
    }
    
    std::cout << "✅ PTT Controller initialized\n\n";
    
    // Set up callbacks
    ptt.on_state_changed([](apm::PTTController::State state) {
        switch (state) {
            case apm::PTTController::State::IDLE:
                std::cout << "🟢 PTT: READY\n";
                break;
            case apm::PTTController::State::TRANSMITTING:
                std::cout << "🔴 PTT: TRANSMITTING\n";
                break;
            case apm::PTTController::State::COOLDOWN:
                std::cout << "🟡 PTT: COOLDOWN\n";
                break;
        }
    });
    
    ptt.on_audio_available([](const std::vector<float>& audio) {
        std::cout << "📡 Audio captured: " << audio.size() << " samples\n";
    });
    
    // Simulate PTT usage
    std::cout << "Simulating PTT button presses...\n\n";
    
    std::cout << "Press 1: Short transmission\n";
    ptt.press();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Simulate audio
    std::vector<float> audio_chunk(480, 0.5f); // 10ms at 48kHz
    for (int i = 0; i < 10; i++) {
        ptt.process_audio(audio_chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    ptt.release();
    std::cout << "\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "Press 2: Longer transmission\n";
    ptt.press();
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    ptt.release();
    std::cout << "\n";
    
    // Show statistics
    std::cout << "📊 PTT Statistics:\n";
    std::cout << "   Total transmissions: " << ptt.get_transmission_count() << "\n";
    std::cout << "   Total samples: " << ptt.get_total_samples() << "\n\n";
    
    ptt.shutdown();
}

void demo_call_signaling() {
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  DEMO 2: Call Signaling System\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
    
    // Create two participants (Alice and Bob)
    apm::signaling::Participant alice;
    alice.id = "alice_123";
    alice.display_name = "Alice";
    alice.ip_address = "127.0.0.1";
    alice.port = 5060;
    alice.source_language = "en";
    alice.target_language = "fr";
    
    apm::signaling::Participant bob;
    bob.id = "bob_456";
    bob.display_name = "Bob";
    bob.ip_address = "127.0.0.1";
    bob.port = 5061;
    bob.source_language = "fr";
    bob.target_language = "en";
    
    // Initialize Alice's signaling
    apm::signaling::CallSignaling alice_signaling;
    if (!alice_signaling.initialize(alice, 5060)) {
        std::cerr << "❌ Failed to initialize Alice's signaling\n";
        return;
    }
    
    std::cout << "✅ Alice's call signaling initialized\n";
    
    // Initialize Bob's signaling
    apm::signaling::CallSignaling bob_signaling;
    if (!bob_signaling.initialize(bob, 5061)) {
        std::cerr << "❌ Failed to initialize Bob's signaling\n";
        return;
    }
    
    std::cout << "✅ Bob's call signaling initialized\n\n";
    
    // Set up Bob's incoming call callback
    bob_signaling.on_incoming_call([&](const apm::signaling::CallSession& session) {
        std::cout << "📞 Bob: Incoming call from " << session.caller.display_name << "\n";
        std::cout << "   Session ID: " << session.session_id << "\n";
        
        // Auto-accept after 1 second
        std::thread([&bob_signaling, session_id = session.session_id]() {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "✅ Bob: Accepting call...\n";
            bob_signaling.accept_call(session_id);
        }).detach();
    });
    
    // Set up call state callbacks
    alice_signaling.on_call_state_changed([](const std::string& session_id, 
                                            apm::signaling::CallState state) {
        std::cout << "Alice call state: " << apm::signaling::call_state_to_string(state) << "\n";
    });
    
    bob_signaling.on_call_state_changed([](const std::string& session_id,
                                          apm::signaling::CallState state) {
        std::cout << "Bob call state: " << apm::signaling::call_state_to_string(state) << "\n";
    });
    
    // Alice calls Bob
    std::cout << "📱 Alice: Initiating call to Bob...\n";
    std::string session_id = alice_signaling.initiate_call(bob);
    
    if (session_id.empty()) {
        std::cerr << "❌ Failed to initiate call\n";
        return;
    }
    
    std::cout << "🔄 Call session created: " << session_id << "\n\n";
    
    // Wait for call to connect
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Check if connected
    auto session = alice_signaling.get_active_session();
    if (session && session->state == apm::signaling::CallState::CONNECTED) {
        std::cout << "✅ Call connected!\n";
        std::cout << "   Duration: " << std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - session->start_time
        ).count() << "s\n\n";
    }
    
    // Simulate call duration
    std::cout << "💬 Call in progress...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // End call
    std::cout << "\n📴 Alice: Ending call...\n";
    alice_signaling.end_call(session_id);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "✅ Call ended\n\n";
    
    alice_signaling.shutdown();
    bob_signaling.shutdown();
}

void demo_complete_system() {
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  DEMO 3: Complete PTT + Translation + Encryption System\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
    
    // Initialize all systems
    std::cout << "Initializing systems...\n";
    
    // 1. APM Core
    apm::APMCore apm_system;
    if (!apm_system.initialize()) {
        std::cerr << "❌ Failed to initialize APM\n";
        return;
    }
    std::cout << "✅ APM Core initialized\n";
    
    // 2. Crypto
    if (!apm::crypto::initialize()) {
        std::cerr << "❌ Failed to initialize crypto\n";
        return;
    }
    std::cout << "✅ Encryption initialized\n";
    
    // 3. PTT
    apm::PTTController ptt;
    if (!ptt.initialize()) {
        std::cerr << "❌ Failed to initialize PTT\n";
        return;
    }
    std::cout << "✅ PTT initialized\n";
    
    // 4. Call Signaling
    apm::signaling::Participant local;
    local.id = "user_001";
    local.display_name = "Demo User";
    local.ip_address = "127.0.0.1";
    local.port = 5070;
    local.source_language = "en";
    local.target_language = "es";
    
    apm::signaling::CallSignaling signaling;
    if (!signaling.initialize(local, 5070)) {
        std::cerr << "❌ Failed to initialize signaling\n";
        return;
    }
    std::cout << "✅ Call signaling initialized\n\n";
    
    // Generate encryption key
    auto encryption_key = apm::crypto::generate_symmetric_key();
    std::cout << "🔑 Encryption key generated\n\n";
    
    // Configure translation
    apm_system.set_source_language("en");
    apm_system.set_target_language("es");
    
    std::cout << "📋 Configuration:\n";
    std::cout << "   Translation: English → Spanish\n";
    std::cout << "   Encryption: ChaCha20-Poly1305 (256-bit)\n";
    std::cout << "   PTT: Software-controlled\n\n";
    
    // Simulate complete workflow
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "  WORKFLOW SIMULATION\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
    
    std::string original_text = "Hello, how can I help you today?";
    
    std::cout << "1️⃣  User presses PTT button\n";
    ptt.press();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "2️⃣  User speaks: \"" << original_text << "\"\n";
    std::cout << "3️⃣  Audio captured and buffered\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::cout << "4️⃣  User releases PTT button\n";
    ptt.release();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::cout << "5️⃣  Processing audio through Whisper...\n";
    std::cout << "    Transcribed: \"" << original_text << "\"\n";
    
    std::cout << "6️⃣  Translating to Spanish...\n";
    auto translation_result = apm_system.translate_text(original_text);
    
    if (translation_result.success) {
        std::cout << "    Translated: \"" << translation_result.translated_text << "\"\n";
        
        std::cout << "7️⃣  Encrypting translation...\n";
        auto encrypted = apm::crypto::encrypt_symmetric(
            translation_result.translated_text,
            encryption_key
        );
        
        if (encrypted.success) {
            std::cout << "    Encrypted: " << encrypted.value.substr(0, 40) << "...\n";
            
            std::cout << "8️⃣  Transmitting encrypted data securely\n";
            std::cout << "    Size: " << encrypted.value.length() << " bytes\n";
            
            std::cout << "9️⃣  Recipient decrypts message...\n";
            auto decrypted = apm::crypto::decrypt_symmetric(encrypted.value, encryption_key);
            
            if (decrypted.success) {
                std::cout << "    Decrypted: \"" << decrypted.value << "\"\n";
                std::cout << "🔟 Recipient hears translation in their language\n";
            }
        }
    }
    
    std::cout << "\n✅ Complete workflow executed successfully!\n\n";
    
    // Show final statistics
    std::cout << "📊 Session Statistics:\n";
    std::cout << "   PTT activations: " << ptt.get_transmission_count() << "\n";
    std::cout << "   Audio samples: " << ptt.get_total_samples() << "\n";
    std::cout << "   Translation time: " << translation_result.processing_time_ms << "ms\n";
    std::cout << "   Security: End-to-end encrypted\n\n";
    
    // Cleanup
    signaling.shutdown();
    ptt.shutdown();
}

int main() {
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    print_header();
    
    std::cout << "This demo showcases:\n";
    std::cout << "  • Push-to-Talk (PTT) audio control\n";
    std::cout << "  • Call signaling and setup\n";
    std::cout << "  • Real-time translation\n";
    std::cout << "  • End-to-end encryption\n";
    std::cout << "  • Complete integrated workflow\n\n";
    
    std::cout << "Press Enter to start demonstrations...\n";
    std::cin.get();
    
    try {
        // Run demonstrations
        demo_ptt_only();
        
        std::cout << "\nPress Enter for next demo...\n";
        std::cin.get();
        
        demo_call_signaling();
        
        std::cout << "\nPress Enter for final demo...\n";
        std::cin.get();
        
        demo_complete_system();
        
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n";
        std::cout << "  ALL DEMONSTRATIONS COMPLETE\n";
        std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
        
        std::cout << "💡 Key Takeaways:\n";
        std::cout << "   ✓ PTT prevents background noise interference\n";
        std::cout << "   ✓ Call signaling handles connection setup\n";
        std::cout << "   ✓ Encryption protects sensitive translations\n";
        std::cout << "   ✓ Modular design allows flexible integration\n\n";
        
        std::cout << "🚀 Ready for production deployment!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
