#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace apm {
namespace crypto {

/**
 * @brief Encryption modes supported by the system
 */
enum class EncryptionMode {
    SYMMETRIC,   // Shared secret key (ChaCha20-Poly1305)
    ASYMMETRIC   // Public/private key pair (X25519 + ChaCha20-Poly1305)
};

/**
 * @brief Result type for encryption/decryption operations
 */
template<typename T>
struct Result {
    bool success;
    T value;
    std::string error;
    
    static Result<T> ok(T val) {
        return Result<T>{true, std::move(val), ""};
    }
    
    static Result<T> err(const std::string& msg) {
        return Result<T>{false, T{}, msg};
    }
};

/**
 * @brief Key pair for asymmetric encryption
 */
struct KeyPair {
    std::vector<uint8_t> public_key;
    std::vector<uint8_t> secret_key;
};

/**
 * @brief Initialize the crypto library
 * Must be called before any other crypto operations
 */
bool initialize();

/**
 * @brief Check if crypto library is initialized
 */
bool is_initialized();

// ============================================================================
// KEY GENERATION
// ============================================================================

/**
 * @brief Generate a random symmetric encryption key
 * @return 256-bit key suitable for symmetric encryption
 */
std::vector<uint8_t> generate_symmetric_key();

/**
 * @brief Generate a public/private key pair for asymmetric encryption
 * @return KeyPair containing public and secret keys
 */
KeyPair generate_keypair();

/**
 * @brief Derive a key from a password using Argon2id
 * @param password The password to derive from
 * @param salt Optional salt (generated if empty)
 * @return Derived key suitable for encryption
 */
Result<std::vector<uint8_t>> derive_key_from_password(
    const std::string& password,
    const std::vector<uint8_t>& salt = {}
);

// ============================================================================
// SYMMETRIC ENCRYPTION (Shared Key)
// ============================================================================

/**
 * @brief Encrypt data using symmetric encryption (ChaCha20-Poly1305)
 * @param plaintext Data to encrypt
 * @param key Symmetric key (32 bytes)
 * @return Encrypted data with authentication tag and nonce prepended
 */
Result<std::vector<uint8_t>> encrypt_symmetric(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key
);

/**
 * @brief Encrypt text using symmetric encryption
 * @param plaintext Text to encrypt
 * @param key Symmetric key (32 bytes)
 * @return Base64-encoded encrypted data
 */
Result<std::string> encrypt_symmetric(
    const std::string& plaintext,
    const std::vector<uint8_t>& key
);

/**
 * @brief Decrypt data using symmetric encryption
 * @param ciphertext Encrypted data with auth tag and nonce
 * @param key Symmetric key (32 bytes)
 * @return Decrypted data
 */
Result<std::vector<uint8_t>> decrypt_symmetric(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& key
);

/**
 * @brief Decrypt text using symmetric encryption
 * @param ciphertext Base64-encoded encrypted data
 * @param key Symmetric key (32 bytes)
 * @return Decrypted text
 */
Result<std::string> decrypt_symmetric(
    const std::string& ciphertext,
    const std::vector<uint8_t>& key
);

// ============================================================================
// ASYMMETRIC ENCRYPTION (Public/Private Key)
// ============================================================================

/**
 * @brief Encrypt data for a recipient using their public key
 * @param plaintext Data to encrypt
 * @param recipient_public_key Recipient's public key
 * @param sender_secret_key Sender's secret key
 * @return Encrypted data
 */
Result<std::vector<uint8_t>> encrypt_asymmetric(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& recipient_public_key,
    const std::vector<uint8_t>& sender_secret_key
);

/**
 * @brief Encrypt text for a recipient using their public key
 * @param plaintext Text to encrypt
 * @param recipient_public_key Recipient's public key
 * @param sender_secret_key Sender's secret key
 * @return Base64-encoded encrypted data
 */
Result<std::string> encrypt_asymmetric(
    const std::string& plaintext,
    const std::vector<uint8_t>& recipient_public_key,
    const std::vector<uint8_t>& sender_secret_key
);

/**
 * @brief Decrypt data using asymmetric encryption
 * @param ciphertext Encrypted data
 * @param sender_public_key Sender's public key
 * @param recipient_secret_key Recipient's secret key
 * @return Decrypted data
 */
Result<std::vector<uint8_t>> decrypt_asymmetric(
    const std::vector<uint8_t>& ciphertext,
    const std::vector<uint8_t>& sender_public_key,
    const std::vector<uint8_t>& recipient_secret_key
);

/**
 * @brief Decrypt text using asymmetric encryption
 * @param ciphertext Base64-encoded encrypted data
 * @param sender_public_key Sender's public key
 * @param recipient_secret_key Recipient's secret key
 * @return Decrypted text
 */
Result<std::string> decrypt_asymmetric(
    const std::string& ciphertext,
    const std::vector<uint8_t>& sender_public_key,
    const std::vector<uint8_t>& recipient_secret_key
);

// ============================================================================
// FILE OPERATIONS
// ============================================================================

/**
 * @brief Encrypt a file using symmetric encryption
 * @param input_path Path to file to encrypt
 * @param output_path Path to write encrypted file
 * @param key Symmetric key
 * @return Success status
 */
Result<bool> encrypt_file(
    const std::string& input_path,
    const std::string& output_path,
    const std::vector<uint8_t>& key
);

/**
 * @brief Decrypt a file using symmetric encryption
 * @param input_path Path to encrypted file
 * @param output_path Path to write decrypted file
 * @param key Symmetric key
 * @return Success status
 */
Result<bool> decrypt_file(
    const std::string& input_path,
    const std::string& output_path,
    const std::vector<uint8_t>& key
);

// ============================================================================
// KEY SERIALIZATION
// ============================================================================

/**
 * @brief Convert key to base64 string for storage/transmission
 * @param key Binary key data
 * @return Base64-encoded key
 */
std::string key_to_base64(const std::vector<uint8_t>& key);

/**
 * @brief Convert base64 string back to key
 * @param base64 Base64-encoded key
 * @return Binary key data
 */
Result<std::vector<uint8_t>> key_from_base64(const std::string& base64);

/**
 * @brief Save key to file (base64 encoded)
 * @param key Key to save
 * @param path File path
 * @return Success status
 */
Result<bool> save_key_to_file(
    const std::vector<uint8_t>& key,
    const std::string& path
);

/**
 * @brief Load key from file (base64 encoded)
 * @param path File path
 * @return Key data
 */
Result<std::vector<uint8_t>> load_key_from_file(const std::string& path);

// ============================================================================
// UTILITIES
// ============================================================================

/**
 * @brief Securely compare two keys in constant time
 * @param a First key
 * @param b Second key
 * @return true if keys match
 */
bool secure_compare(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b
);

/**
 * @brief Securely zero memory
 * @param data Buffer to zero
 */
void secure_zero(std::vector<uint8_t>& data);

} // namespace crypto
} // namespace apm
