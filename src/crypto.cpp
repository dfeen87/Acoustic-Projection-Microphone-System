#include "apm/crypto.hpp"
#include <sodium.h>
#include <fstream>
#include <cstring>
#include <algorithm>

namespace apm {
namespace crypto {

namespace {
    bool g_initialized = false;
    
    // Convert binary to base64
    std::string to_base64(const std::vector<uint8_t>& data) {
        size_t b64_len = sodium_base64_ENCODED_LEN(data.size(), sodium_base64_VARIANT_ORIGINAL);
        std::string result(b64_len, '\0');
        sodium_bin2base64(result.data(), b64_len, 
                         data.data(), data.size(),
                         sodium_base64_VARIANT_ORIGINAL);
        // Remove trailing null
        result.resize(strlen(result.c_str()));
        return result;
    }
    
    // Convert base64 to binary
    std::vector<uint8_t> from_base64(const std::string& b64) {
        std::vector<uint8_t> result(b64.size());
        size_t bin_len;
        if (sodium_base642bin(result.data(), result.size(),
                             b64.data(), b64.size(),
                             nullptr, &bin_len, nullptr,
                             sodium_base64_VARIANT_ORIGINAL) != 0) {
            return {};
        }
        result.resize(bin_len);
        return result;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool initialize() {
    if (g_initialized) {
        return true;
    }
    
    if (sodium_init() < 0) {
        return false;
    }
    
    g_initialized = true;
    return true;
}

bool is_initialized() {
    return g_initialized;
}

// ============================================================================
// KEY GENERATION
// ============================================================================

std::vector<uint8_t> generate_symmetric_key() {
    std::vector<uint8_t> key(crypto_secretbox_KEYBYTES);
    crypto_secretbox_keygen(key.data());
    return key;
}

KeyPair generate_keypair() {
    KeyPair kp;
    kp.public_key.resize(crypto_box_PUBLICKEYBYTES);
    kp.secret_key.resize(crypto_box_SECRETKEYBYTES);
    crypto_box_keypair(kp.public_key.data(), kp.secret_key.data());
    return kp;
}

Result<std::vector<uint8_t>> derive_key_from_password(
    const std::string& password,
    const std::vector<uint8_t>& salt_in
) {
    if (!g_initialized) {
        return Result<std::vector<uint8_t>>::err("Crypto not initialized");
    }
    
    std::vector<uint8_t> salt = salt_in;
    if (salt.empty()) {
        salt.resize(crypto_pwhash_SALTBYTES);
        randombytes_buf(salt.data(), salt.size());
    }
    
    if (salt.size() != crypto_pwhash_SALTBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid salt size");
    }
    
    std::vector<uint8_t> key(crypto_secretbox_KEYBYTES);
    
    if (crypto_pwhash(key.data(), key.size(),
                     password.c_str(), password.size(),
                     salt.data(),
                     crypto_pwhash_OPSLIMIT_INTERACTIVE,
                     crypto_pwhash_MEMLIMIT_INTERACTIVE,
                     crypto_pwhash_ALG_DEFAULT) != 0) {
        return Result<std::vector<uint8_t>>::err("Password derivation failed");
    }
    
    return Result<std::vector<uint8_t>>::ok(key);
}

// ============================================================================
// SYMMETRIC ENCRYPTION
// ============================================================================

Result<std::vector<uint8_t>> encrypt_symmetric(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key
) {
    if (!g_initialized) {
        return Result<std::vector<uint8_t>>::err("Crypto not initialized");
    }
    
    if (key.size() != crypto_secretbox_KEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid key size");
    }
    
    // Allocate space for nonce + ciphertext
    std::vector<uint8_t> ciphertext(crypto_secretbox_NONCEBYTES + 
                                    crypto_secretbox_MACBYTES + 
                                    plaintext.size());
    
    // Generate random nonce
    uint8_t* nonce = ciphertext.data();
    randombytes_buf(nonce, crypto_secretbox_NONCEBYTES);
    
    // Encrypt (MAC is prepended automatically by secretbox)
    if (crypto_secretbox_easy(
            ciphertext.data() + crypto_secretbox_NONCEBYTES,
            plaintext.data(), plaintext.size(),
            nonce, key.data()) != 0) {
        return Result<std::vector<uint8_t>>::err("Encryption failed");
    }
    
    return Result<std::vector<uint8_t>>::ok(ciphertext);
}

Result<std::string> encrypt_symmetric(
    const std::string& plaintext,
    const std::vector<uint8_t>& key
) {
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());
    auto result = encrypt_symmetric(data, key);
    
    if (!result.success) {
        return Result<std::string>::err(result.error);
    }
    
    return Result<std::string>::ok(to_base64(result.value));
}

Result<std::vector<uint8_t>> decrypt_symmetric(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key
) {
    if (!g_initialized) {
        return Result<std::vector<uint8_t>>::err("Crypto not initialized");
    }
    
    if (key.size() != crypto_secretbox_KEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid key size");
    }
    
    if (ciphertext.size() < crypto_secretbox_NONCEBYTES + crypto_secretbox_MACBYTES) {
        return Result<std::vector<uint8_t>>::err("Ciphertext too short");
    }
    
    // Extract nonce
    const uint8_t* nonce = ciphertext.data();
    
    // Allocate plaintext buffer
    size_t plaintext_len = ciphertext.size() - crypto_secretbox_NONCEBYTES - crypto_secretbox_MACBYTES;
    std::vector<uint8_t> plaintext(plaintext_len);
    
    // Decrypt and verify MAC
    if (crypto_secretbox_open_easy(
            plaintext.data(),
            ciphertext.data() + crypto_secretbox_NONCEBYTES,
            ciphertext.size() - crypto_secretbox_NONCEBYTES,
            nonce, key.data()) != 0) {
        return Result<std::vector<uint8_t>>::err("Decryption failed (wrong key or corrupted data)");
    }
    
    return Result<std::vector<uint8_t>>::ok(plaintext);
}

Result<std::string> decrypt_symmetric(
    const std::string& ciphertext,
    const std::vector<uint8_t>& key
) {
    auto data = from_base64(ciphertext);
    if (data.empty()) {
        return Result<std::string>::err("Invalid base64 encoding");
    }
    
    auto result = decrypt_symmetric(data, key);
    
    if (!result.success) {
        return Result<std::string>::err(result.error);
    }
    
    return Result<std::string>::ok(
        std::string(result.value.begin(), result.value.end())
    );
}

// ============================================================================
// ASYMMETRIC ENCRYPTION
// ============================================================================

Result<std::vector<uint8_t>> encrypt_asymmetric(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& recipient_public_key,
    const std::vector<uint8_t>& sender_secret_key
) {
    if (!g_initialized) {
        return Result<std::vector<uint8_t>>::err("Crypto not initialized");
    }
    
    if (recipient_public_key.size() != crypto_box_PUBLICKEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid recipient public key size");
    }
    
    if (sender_secret_key.size() != crypto_box_SECRETKEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid sender secret key size");
    }
    
    // Allocate space for nonce + ciphertext
    std::vector<uint8_t> ciphertext(crypto_box_NONCEBYTES + 
                                    crypto_box_MACBYTES + 
                                    plaintext.size());
    
    // Generate random nonce
    uint8_t* nonce = ciphertext.data();
    randombytes_buf(nonce, crypto_box_NONCEBYTES);
    
    // Encrypt
    if (crypto_box_easy(
            ciphertext.data() + crypto_box_NONCEBYTES,
            plaintext.data(), plaintext.size(),
            nonce,
            recipient_public_key.data(),
            sender_secret_key.data()) != 0) {
        return Result<std::vector<uint8_t>>::err("Asymmetric encryption failed");
    }
    
    return Result<std::vector<uint8_t>>::ok(ciphertext);
}

Result<std::string> encrypt_asymmetric(
    const std::string& plaintext,
    const std::vector<uint8_t>& recipient_public_key,
    const std::vector<uint8_t>& sender_secret_key
) {
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());
    auto result = encrypt_asymmetric(data, recipient_public_key, sender_secret_key);
    
    if (!result.success) {
        return Result<std::string>::err(result.error);
    }
    
    return Result<std::string>::ok(to_base64(result.value));
}

Result<std::vector<uint8_t>> decrypt_asymmetric(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& sender_public_key,
    const std::vector<uint8_t>& recipient_secret_key
) {
    if (!g_initialized) {
        return Result<std::vector<uint8_t>>::err("Crypto not initialized");
    }
    
    if (sender_public_key.size() != crypto_box_PUBLICKEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid sender public key size");
    }
    
    if (recipient_secret_key.size() != crypto_box_SECRETKEYBYTES) {
        return Result<std::vector<uint8_t>>::err("Invalid recipient secret key size");
    }
    
    if (ciphertext.size() < crypto_box_NONCEBYTES + crypto_box_MACBYTES) {
        return Result<std::vector<uint8_t>>::err("Ciphertext too short");
    }
    
    // Extract nonce
    const uint8_t* nonce = ciphertext.data();
    
    // Allocate plaintext buffer
    size_t plaintext_len = ciphertext.size() - crypto_box_NONCEBYTES - crypto_box_MACBYTES;
    std::vector<uint8_t> plaintext(plaintext_len);
    
    // Decrypt and verify MAC
    if (crypto_box_open_easy(
            plaintext.data(),
            ciphertext.data() + crypto_box_NONCEBYTES,
            ciphertext.size() - crypto_box_NONCEBYTES,
            nonce,
            sender_public_key.data(),
            recipient_secret_key.data()) != 0) {
        return Result<std::vector<uint8_t>>::err("Asymmetric decryption failed (wrong key or corrupted data)");
    }
    
    return Result<std::vector<uint8_t>>::ok(plaintext);
}

Result<std::string> decrypt_asymmetric(
    const std::string& ciphertext,
    const std::vector<uint8_t>& sender_public_key,
    const std::vector<uint8_t>& recipient_secret_key
) {
    auto data = from_base64(ciphertext);
    if (data.empty()) {
        return Result<std::string>::err("Invalid base64 encoding");
    }
    
    auto result = decrypt_asymmetric(data, sender_public_key, recipient_secret_key);
    
    if (!result.success) {
        return Result<std::string>::err(result.error);
    }
    
    return Result<std::string>::ok(
        std::string(result.value.begin(), result.value.end())
    );
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

Result<bool> encrypt_file(
    const std::string& input_path,
    const std::string& output_path,
    const std::vector<uint8_t>& key
) {
    // Read input file
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        return Result<bool>::err("Cannot open input file");
    }
    
    std::vector<uint8_t> plaintext(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    input.close();
    
    // Encrypt
    auto result = encrypt_symmetric(plaintext, key);
    if (!result.success) {
        return Result<bool>::err(result.error);
    }
    
    // Write output file
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return Result<bool>::err("Cannot open output file");
    }
    
    output.write(reinterpret_cast<const char*>(result.value.data()), 
                result.value.size());
    output.close();
    
    return Result<bool>::ok(true);
}

Result<bool> decrypt_file(
    const std::string& input_path,
    const std::string& output_path,
    const std::vector<uint8_t>& key
) {
    // Read input file
    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        return Result<bool>::err("Cannot open input file");
    }
    
    std::vector<uint8_t> ciphertext(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    input.close();
    
    // Decrypt
    auto result = decrypt_symmetric(ciphertext, key);
    if (!result.success) {
        return Result<bool>::err(result.error);
    }
    
    // Write output file
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return Result<bool>::err("Cannot open output file");
    }
    
    output.write(reinterpret_cast<const char*>(result.value.data()), 
                result.value.size());
    output.close();
    
    return Result<bool>::ok(true);
}

// ============================================================================
// KEY SERIALIZATION
// ============================================================================

std::string key_to_base64(const std::vector<uint8_t>& key) {
    return to_base64(key);
}

Result<std::vector<uint8_t>> key_from_base64(const std::string& base64) {
    auto key = from_base64(base64);
    if (key.empty()) {
        return Result<std::vector<uint8_t>>::err("Invalid base64 encoding");
    }
    return Result<std::vector<uint8_t>>::ok(key);
}

Result<bool> save_key_to_file(
    const std::vector<uint8_t>& key,
    const std::string& path
) {
    std::ofstream file(path);
    if (!file) {
        return Result<bool>::err("Cannot open file for writing");
    }
    
    file << key_to_base64(key);
    file.close();
    
    return Result<bool>::ok(true);
}

Result<std::vector<uint8_t>> load_key_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return Result<std::vector<uint8_t>>::err("Cannot open file for reading");
    }
    
    std::string base64;
    std::getline(file, base64);
    file.close();
    
    return key_from_base64(base64);
}

// ============================================================================
// UTILITIES
// ============================================================================

bool secure_compare(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b
) {
    if (a.size() != b.size()) {
        return false;
    }
    
    return sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}

void secure_zero(std::vector<uint8_t>& data) {
    sodium_memzero(data.data(), data.size());
}

} // namespace crypto
} // namespace apm
