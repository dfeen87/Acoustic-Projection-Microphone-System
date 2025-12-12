#include "apm/crypto.hpp"
#include <gtest/gtest.h>
#include <fstream>

class CryptoTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(apm::crypto::initialize()) << "Failed to initialize crypto library";
    }
};

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

TEST_F(CryptoTest, InitializeSucceeds) {
    EXPECT_TRUE(apm::crypto::is_initialized());
}

TEST_F(CryptoTest, MultipleInitializationsSucceed) {
    EXPECT_TRUE(apm::crypto::initialize());
    EXPECT_TRUE(apm::crypto::initialize());
    EXPECT_TRUE(apm::crypto::is_initialized());
}

// ============================================================================
// KEY GENERATION TESTS
// ============================================================================

TEST_F(CryptoTest, GenerateSymmetricKeyProducesCorrectSize) {
    auto key = apm::crypto::generate_symmetric_key();
    EXPECT_EQ(key.size(), 32); // 256 bits
}

TEST_F(CryptoTest, GenerateSymmetricKeyProducesUniqueKeys) {
    auto key1 = apm::crypto::generate_symmetric_key();
    auto key2 = apm::crypto::generate_symmetric_key();
    EXPECT_NE(key1, key2);
}

TEST_F(CryptoTest, GenerateKeypairProducesCorrectSizes) {
    auto kp = apm::crypto::generate_keypair();
    EXPECT_EQ(kp.public_key.size(), 32);
    EXPECT_EQ(kp.secret_key.size(), 32);
}

TEST_F(CryptoTest, GenerateKeypairProducesUniqueKeys) {
    auto kp1 = apm::crypto::generate_keypair();
    auto kp2 = apm::crypto::generate_keypair();
    EXPECT_NE(kp1.public_key, kp2.public_key);
    EXPECT_NE(kp1.secret_key, kp2.secret_key);
}

TEST_F(CryptoTest, DeriveKeyFromPasswordSucceeds) {
    auto result = apm::crypto::derive_key_from_password("test_password");
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(result.value.size(), 32);
}

TEST_F(CryptoTest, DeriveKeyFromPasswordWithSameSaltProducesSameKey) {
    std::vector<uint8_t> salt(16, 0x42);
    auto result1 = apm::crypto::derive_key_from_password("password", salt);
    auto result2 = apm::crypto::derive_key_from_password("password", salt);
    
    ASSERT_TRUE(result1.success);
    ASSERT_TRUE(result2.success);
    EXPECT_EQ(result1.value, result2.value);
}

// ============================================================================
// SYMMETRIC ENCRYPTION TESTS
// ============================================================================

TEST_F(CryptoTest, SymmetricEncryptDecryptBinaryRoundtrip) {
    auto key = apm::crypto::generate_symmetric_key();
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key);
    ASSERT_TRUE(encrypt_result.success) << encrypt_result.error;
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key);
    ASSERT_TRUE(decrypt_result.success) << decrypt_result.error;
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

TEST_F(CryptoTest, SymmetricEncryptDecryptTextRoundtrip) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string plaintext = "Hello, World! This is a test message.";
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key);
    ASSERT_TRUE(encrypt_result.success) << encrypt_result.error;
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key);
    ASSERT_TRUE(decrypt_result.success) << decrypt_result.error;
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

TEST_F(CryptoTest, SymmetricEncryptProducesDifferentCiphertexts) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string plaintext = "Same message";
    
    auto result1 = apm::crypto::encrypt_symmetric(plaintext, key);
    auto result2 = apm::crypto::encrypt_symmetric(plaintext, key);
    
    ASSERT_TRUE(result1.success);
    ASSERT_TRUE(result2.success);
    
    // Different nonces mean different ciphertexts
    EXPECT_NE(result1.value, result2.value);
}

TEST_F(CryptoTest, SymmetricDecryptFailsWithWrongKey) {
    auto key1 = apm::crypto::generate_symmetric_key();
    auto key2 = apm::crypto::generate_symmetric_key();
    std::string plaintext = "Secret message";
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key1);
    ASSERT_TRUE(encrypt_result.success);
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key2);
    EXPECT_FALSE(decrypt_result.success);
}

TEST_F(CryptoTest, SymmetricDecryptFailsWithCorruptedData) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string plaintext = "Test message";
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key);
    ASSERT_TRUE(encrypt_result.success);
    
    // Corrupt the ciphertext
    auto corrupted = encrypt_result.value;
    corrupted[10] ^= 0xFF;
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(corrupted, key);
    EXPECT_FALSE(decrypt_result.success);
}

TEST_F(CryptoTest, SymmetricEncryptFailsWithInvalidKeySize) {
    std::vector<uint8_t> invalid_key = {1, 2, 3}; // Too short
    std::string plaintext = "Test";
    
    auto result = apm::crypto::encrypt_symmetric(plaintext, invalid_key);
    EXPECT_FALSE(result.success);
}

TEST_F(CryptoTest, SymmetricEncryptHandlesEmptyString) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string plaintext = "";
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key);
    ASSERT_TRUE(encrypt_result.success);
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key);
    ASSERT_TRUE(decrypt_result.success);
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

TEST_F(CryptoTest, SymmetricEncryptHandlesUnicodeText) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string plaintext = "Hello 世界 🌍 Привет مرحبا";
    
    auto encrypt_result = apm::crypto::encrypt_symmetric(plaintext, key);
    ASSERT_TRUE(encrypt_result.success);
    
    auto decrypt_result = apm::crypto::decrypt_symmetric(encrypt_result.value, key);
    ASSERT_TRUE(decrypt_result.success);
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

// ============================================================================
// ASYMMETRIC ENCRYPTION TESTS
// ============================================================================

TEST_F(CryptoTest, AsymmetricEncryptDecryptRoundtrip) {
    auto sender_keys = apm::crypto::generate_keypair();
    auto receiver_keys = apm::crypto::generate_keypair();
    std::string plaintext = "Secure message";
    
    auto encrypt_result = apm::crypto::encrypt_asymmetric(
        plaintext,
        receiver_keys.public_key,
        sender_keys.secret_key
    );
    ASSERT_TRUE(encrypt_result.success) << encrypt_result.error;
    
    auto decrypt_result = apm::crypto::decrypt_asymmetric(
        encrypt_result.value,
        sender_keys.public_key,
        receiver_keys.secret_key
    );
    ASSERT_TRUE(decrypt_result.success) << decrypt_result.error;
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

TEST_F(CryptoTest, AsymmetricDecryptFailsWithWrongKeys) {
    auto sender_keys = apm::crypto::generate_keypair();
    auto receiver_keys = apm::crypto::generate_keypair();
    auto wrong_keys = apm::crypto::generate_keypair();
    std::string plaintext = "Secret";
    
    auto encrypt_result = apm::crypto::encrypt_asymmetric(
        plaintext,
        receiver_keys.public_key,
        sender_keys.secret_key
    );
    ASSERT_TRUE(encrypt_result.success);
    
    // Try to decrypt with wrong receiver key
    auto decrypt_result = apm::crypto::decrypt_asymmetric(
        encrypt_result.value,
        sender_keys.public_key,
        wrong_keys.secret_key
    );
    EXPECT_FALSE(decrypt_result.success);
}

TEST_F(CryptoTest, AsymmetricEncryptHandlesLargeMessages) {
    auto sender_keys = apm::crypto::generate_keypair();
    auto receiver_keys = apm::crypto::generate_keypair();
    std::string plaintext(10000, 'A'); // 10KB
    
    auto encrypt_result = apm::crypto::encrypt_asymmetric(
        plaintext,
        receiver_keys.public_key,
        sender_keys.secret_key
    );
    ASSERT_TRUE(encrypt_result.success);
    
    auto decrypt_result = apm::crypto::decrypt_asymmetric(
        encrypt_result.value,
        sender_keys.public_key,
        receiver_keys.secret_key
    );
    ASSERT_TRUE(decrypt_result.success);
    
    EXPECT_EQ(plaintext, decrypt_result.value);
}

// ============================================================================
// FILE ENCRYPTION TESTS
// ============================================================================

TEST_F(CryptoTest, FileEncryptDecryptRoundtrip) {
    auto key = apm::crypto::generate_symmetric_key();
    std::string content = "File content to encrypt\nMultiple lines\n123456";
    
    // Write test file
    std::ofstream out("test_plain.txt");
    out << content;
    out.close();
    
    // Encrypt
    auto encrypt_result = apm::crypto::encrypt_file(
        "test_plain.txt",
        "test_encrypted.bin",
        key
    );
    ASSERT_TRUE(encrypt_result.success) << encrypt_result.error;
    
    // Decrypt
    auto decrypt_result = apm::crypto::decrypt_file(
        "test_encrypted.bin",
        "test_decrypted.txt",
        key
    );
    ASSERT_TRUE(decrypt_result.success) << decrypt_result.error;
    
    // Verify
    std::ifstream in("test_decrypted.txt");
    std::string decrypted_content(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    in.close();
    
    EXPECT_EQ(content, decrypted_content);
    
    // Cleanup
    std::remove("test_plain.txt");
    std::remove("test_encrypted.bin");
    std::remove("test_decrypted.txt");
}

// ============================================================================
// KEY SERIALIZATION TESTS
// ============================================================================

TEST_F(CryptoTest, KeyToBase64AndBack) {
    auto key = apm::crypto::generate_symmetric_key();
    
    auto base64 = apm::crypto::key_to_base64(key);
    EXPECT_FALSE(base64.empty());
    
    auto result = apm::crypto::key_from_base64(base64);
    ASSERT_TRUE(result.success) << result.error;
    EXPECT_EQ(key, result.value);
}

TEST_F(CryptoTest, SaveAndLoadKeyFromFile) {
    auto key = apm::crypto::generate_symmetric_key();
    
    auto save_result = apm::crypto::save_key_to_file(key, "test_key.txt");
    ASSERT_TRUE(save_result.success) << save_result.error;
    
    auto load_result = apm::crypto::load_key_from_file("test_key.txt");
    ASSERT_TRUE(load_result.success) << load_result.error;
    
    EXPECT_EQ(key, load_result.value);
    
    std::remove("test_key.txt");
}

// ============================================================================
// UTILITY TESTS
// ============================================================================

TEST_F(CryptoTest, SecureCompareIdenticalKeys) {
    auto key = apm::crypto::generate_symmetric_key();
    EXPECT_TRUE(apm::crypto::secure_compare(key, key));
}

TEST_F(CryptoTest, SecureCompareDifferentKeys) {
    auto key1 = apm::crypto::generate_symmetric_key();
    auto key2 = apm::crypto::generate_symmetric_key();
    EXPECT_FALSE(apm::crypto::secure_compare(key1, key2));
}

TEST_F(CryptoTest, SecureCompareDifferentSizes) {
    std::vector<uint8_t> key1 = {1, 2, 3, 4};
    std::vector<uint8_t> key2 = {1, 2, 3};
    EXPECT_FALSE(apm::crypto::secure_compare(key1, key2));
}

TEST_F(CryptoTest, SecureZeroErasesData) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    apm::crypto::secure_zero(data);
    
    for (auto byte : data) {
        EXPECT_EQ(byte, 0);
    }
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(CryptoTest, EndToEndTranslationEncryption) {
    // Simulate a translation workflow
    std::string original = "Hello, world!";
    std::string translated = "Bonjour, le monde!";
    
    // Generate key
    auto key = apm::crypto::generate_symmetric_key();
    
    // Encrypt translation
    auto encrypt_result = apm::crypto::encrypt_symmetric(translated, key);
    ASSERT_TRUE(encrypt_result.success);
    
    // Simulate transmission (encrypted data)
    std::string transmitted = encrypt_result.value;
    
    // Decrypt on receiving end
    auto decrypt_result = apm::crypto::decrypt_symmetric(transmitted, key);
    ASSERT_TRUE(decrypt_result.success);
    
    EXPECT_EQ(translated, decrypt_result.value);
}

TEST_F(CryptoTest, MultipleRecipients) {
    std::string message = "Broadcast translation";
    auto sender_keys = apm::crypto::generate_keypair();
    
    // Generate keys for 3 recipients
    std::vector<apm::crypto::KeyPair> recipients;
    for (int i = 0; i < 3; i++) {
        recipients.push_back(apm::crypto::generate_keypair());
    }
    
    // Encrypt for each recipient
    for (const auto& recipient : recipients) {
        auto encrypt_result = apm::crypto::encrypt_asymmetric(
            message,
            recipient.public_key,
            sender_keys.secret_key
        );
        ASSERT_TRUE(encrypt_result.success);
        
        // Each recipient can decrypt
        auto decrypt_result = apm::crypto::decrypt_asymmetric(
            encrypt_result.value,
            sender_keys.public_key,
            recipient.secret_key
        );
        ASSERT_TRUE(decrypt_result.success);
        EXPECT_EQ(message, decrypt_result.value);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
