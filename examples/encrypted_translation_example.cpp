#include "apm/apm_core.hpp"
#include "apm/crypto/crypto.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <fstream>

void print_separator() {
    std::cout << "\n" << std::string(70, '=') << "\n\n";
}

void demo_symmetric_encryption() {
    std::cout << "📝 SYMMETRIC ENCRYPTION DEMO\n";
    std::cout << "   (Shared key encryption - both parties use same key)\n";
    print_separator();
    
    // Simulate translation output
    std::string original_text = "Hello, how are you?";
    std::string translated_text = "Bonjour, comment allez-vous?";
    
    std::cout << "Original:    " << original_text << "\n";
    std::cout << "Translated:  " << translated_text << "\n\n";
    
    // Generate encryption key
    auto key = apm::crypto::generate_symmetric_key();
    std::cout << "🔑 Generated symmetric key\n";
    std::cout << "   Key (base64): " << apm::crypto::key_to_base64(key) << "\n\n";
    
    // Encrypt the translation
    auto encrypt_result = apm::crypto::encrypt_symmetric(translated_text, key);
    if (!encrypt_result.success) {
        std::cerr << "❌ Encryption failed: " << encrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔒 Encrypted translation:\n";
    std::cout << "   " << encrypt_result.value.substr(0, 60) << "...\n\n";
    
    // Decrypt the translation
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key);
    if (!decrypt_result.success) {
        std::cerr << "❌ Decryption failed: " << decrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔓 Decrypted translation: " << decrypt_result.value << "\n";
    std::cout << "✅ Match: " << (decrypt_result.value == translated_text ? "YES" : "NO") << "\n";
}

void demo_asymmetric_encryption() {
    print_separator();
    std::cout << "📝 ASYMMETRIC ENCRYPTION DEMO\n";
    std::cout << "   (Public/private key - sender uses receiver's public key)\n";
    print_separator();
    
    std::string translated_text = "Hola, ¿cómo estás?";
    std::cout << "Translated text: " << translated_text << "\n\n";
    
    // Generate key pairs for sender and receiver
    auto sender_keys = apm::crypto::generate_keypair();
    auto receiver_keys = apm::crypto::generate_keypair();
    
    std::cout << "🔑 Generated key pairs\n";
    std::cout << "   Sender public key:   " 
              << apm::crypto::key_to_base64(sender_keys.public_key).substr(0, 32) << "...\n";
    std::cout << "   Receiver public key: " 
              << apm::crypto::key_to_base64(receiver_keys.public_key).substr(0, 32) << "...\n\n";
    
    // Sender encrypts for receiver
    auto encrypt_result = apm::crypto::encrypt_asymmetric(
        translated_text,
        receiver_keys.public_key,  // Encrypt TO receiver
        sender_keys.secret_key     // Signed BY sender
    );
    
    if (!encrypt_result.success) {
        std::cerr << "❌ Encryption failed: " << encrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔒 Encrypted message:\n";
    std::cout << "   " << encrypt_result.value.substr(0, 60) << "...\n\n";
    
    // Receiver decrypts from sender
    auto decrypt_result = apm::crypto::decrypt_asymmetric(
        encrypt_result.value,
        sender_keys.public_key,     // Verify FROM sender
        receiver_keys.secret_key    // Decrypt WITH receiver's private key
    );
    
    if (!decrypt_result.success) {
        std::cerr << "❌ Decryption failed: " << decrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔓 Decrypted message: " << decrypt_result.value << "\n";
    std::cout << "✅ Match: " << (decrypt_result.value == translated_text ? "YES" : "NO") << "\n";
}

void demo_password_derived_key() {
    print_separator();
    std::cout << "📝 PASSWORD-DERIVED KEY DEMO\n";
    std::cout << "   (Derive encryption key from user password)\n";
    print_separator();
    
    std::string password = "MySecurePassword123!";
    std::string translated_text = "Guten Tag! Wie geht es Ihnen?";
    
    std::cout << "Password: " << password << "\n";
    std::cout << "Message:  " << translated_text << "\n\n";
    
    // Derive key from password
    auto key_result = apm::crypto::derive_key_from_password(password);
    if (!key_result.success) {
        std::cerr << "❌ Key derivation failed: " << key_result.error << "\n";
        return;
    }
    
    std::cout << "🔑 Derived key from password\n";
    std::cout << "   Key (base64): " 
              << apm::crypto::key_to_base64(key_result.value).substr(0, 32) << "...\n\n";
    
    // Encrypt
    auto encrypt_result = apm::crypto::encrypt_symmetric(translated_text, key_result.value);
    if (!encrypt_result.success) {
        std::cerr << "❌ Encryption failed: " << encrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔒 Encrypted with password-derived key\n\n";
    
    // Decrypt
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key_result.value);
    if (!decrypt_result.success) {
        std::cerr << "❌ Decryption failed: " << decrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔓 Decrypted message: " << decrypt_result.value << "\n";
    std::cout << "✅ Match: " << (decrypt_result.value == translated_text ? "YES" : "NO") << "\n";
}

void demo_file_encryption() {
    print_separator();
    std::cout << "📝 FILE ENCRYPTION DEMO\n";
    std::cout << "   (Encrypt translation logs to file)\n";
    print_separator();
    
    // Create a sample translation log
    std::string log_content = 
        "Translation Log\n"
        "===============\n"
        "[2024-12-12 10:30:15] EN->FR: Hello -> Bonjour\n"
        "[2024-12-12 10:30:16] EN->ES: Thank you -> Gracias\n"
        "[2024-12-12 10:30:17] EN->DE: Goodbye -> Auf Wiedersehen\n";
    
    std::cout << "Original log content:\n" << log_content << "\n";
    
    // Save to temp file
    std::ofstream temp("translation_log.txt");
    temp << log_content;
    temp.close();
    
    // Generate key
    auto key = apm::crypto::generate_symmetric_key();
    std::cout << "🔑 Generated encryption key\n\n";
    
    // Encrypt file
    auto encrypt_result = apm::crypto::encrypt_file(
        "translation_log.txt",
        "translation_log.encrypted",
        key
    );
    
    if (!encrypt_result.success) {
        std::cerr << "❌ File encryption failed: " << encrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔒 File encrypted: translation_log.txt -> translation_log.encrypted\n\n";
    
    // Decrypt file
    auto decrypt_result = apm::crypto::decrypt_file(
        "translation_log.encrypted",
        "translation_log.decrypted",
        key
    );
    
    if (!decrypt_result.success) {
        std::cerr << "❌ File decryption failed: " << decrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔓 File decrypted: translation_log.encrypted -> translation_log.decrypted\n\n";
    
    // Read and verify
    std::ifstream decrypted("translation_log.decrypted");
    std::string decrypted_content(
        (std::istreambuf_iterator<char>(decrypted)),
        std::istreambuf_iterator<char>()
    );
    decrypted.close();
    
    std::cout << "Decrypted content:\n" << decrypted_content << "\n";
    std::cout << "✅ Match: " << (decrypted_content == log_content ? "YES" : "NO") << "\n";
    
    // Cleanup
    std::remove("translation_log.txt");
    std::remove("translation_log.encrypted");
    std::remove("translation_log.decrypted");
}

void demo_with_real_translation() {
    print_separator();
    std::cout << "📝 COMPLETE TRANSLATION + ENCRYPTION PIPELINE\n";
    print_separator();
    
    // Initialize crypto
    if (!apm::crypto::initialize()) {
        std::cerr << "❌ Failed to initialize crypto library\n";
        return;
    }
    
    // Initialize APM
    apm::APMCore apm_system;
    if (!apm_system.initialize()) {
        std::cerr << "❌ Failed to initialize APM system\n";
        return;
    }
    
    std::cout << "✅ APM System initialized\n";
    std::cout << "✅ Crypto system initialized\n\n";
    
    // Set up translation
    apm_system.set_source_language("en");
    apm_system.set_target_language("fr");
    
    std::string original_text = "Welcome to the encrypted translation system!";
    std::cout << "Original (EN): " << original_text << "\n";
    
    // Translate
    std::cout << "\n🔄 Translating...\n";
    auto translation_result = apm_system.translate_text(original_text);
    
    if (!translation_result.success) {
        std::cerr << "❌ Translation failed: " << translation_result.error_message << "\n";
        return;
    }
    
    std::cout << "Translated (FR): " << translation_result.translated_text << "\n\n";
    
    // Generate encryption key
    auto key = apm::crypto::generate_symmetric_key();
    std::cout << "🔑 Generated encryption key\n";
    
    // Encrypt translation
    auto encrypt_result = apm::crypto::encrypt_symmetric(
        translation_result.translated_text, 
        key
    );
    
    if (!encrypt_result.success) {
        std::cerr << "❌ Encryption failed: " << encrypt_result.error << "\n";
        return;
    }
    
    std::cout << "🔒 Translation encrypted\n";
    std::cout << "   Encrypted data: " << encrypt_result.value.substr(0, 60) << "...\n\n";
    
    std::cout << "📤 Ready to transmit encrypted translation securely!\n";
    std::cout << "   • Original translation is protected\n";
    std::cout << "   • Only recipient with key can decrypt\n";
    std::cout << "   • Integrity verified with authentication tag\n";
}

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          APM ENCRYPTED TRANSLATION EXAMPLES                      ║\n";
    std::cout << "║          Secure End-to-End Translation Encryption                ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    
    // Initialize crypto library
    if (!apm::crypto::initialize()) {
        std::cerr << "\n❌ Failed to initialize libsodium\n";
        std::cerr << "   Make sure libsodium is installed:\n";
        std::cerr << "   - Ubuntu/Debian: sudo apt-get install libsodium-dev\n";
        std::cerr << "   - macOS: brew install libsodium\n";
        return 1;
    }
    
    std::cout << "\n✅ Encryption library initialized\n";
    
    try {
        // Run demonstrations
        demo_symmetric_encryption();
        demo_asymmetric_encryption();
        demo_password_derived_key();
        demo_file_encryption();
        demo_with_real_translation();
        
        print_separator();
        std::cout << "✅ All encryption demos completed successfully!\n\n";
        std::cout << "💡 Security Notes:\n";
        std::cout << "   • Keys are 256-bit (ChaCha20-Poly1305)\n";
        std::cout << "   • Authentication tags prevent tampering\n";
        std::cout << "   • Constant-time operations prevent timing attacks\n";
        std::cout << "   • Memory is securely zeroed after use\n";
        print_separator();
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
