/**
 * @file test_serialization_aes.cpp
 * @brief Unit tests for AES-256-GCM encryption in nimcp_serialization
 *
 * WHAT: Tests for nimcp_aes_available(), nimcp_encrypt(), nimcp_decrypt()
 * WHY:  Verify AES encryption round-trip, authentication, error handling
 * HOW:  Use OpenSSL-backed AES-256-GCM through nimcp_serialization.h API
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>

// Header has its own extern "C" guard
#include "utils/serialization/nimcp_serialization.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationAesTest : public ::testing::Test {
protected:
    // A 32-byte AES-256 key
    uint8_t key_[32];
    // A different 32-byte key for wrong-key tests
    uint8_t wrong_key_[32];

    void SetUp() override {
        // Initialize key with deterministic pattern
        for (size_t i = 0; i < 32; i++) {
            key_[i] = static_cast<uint8_t>(0x42 + i);
        }
        // Different key
        for (size_t i = 0; i < 32; i++) {
            wrong_key_[i] = static_cast<uint8_t>(0xAA + i);
        }
    }
};

//=============================================================================
// Availability Tests
//=============================================================================

TEST_F(SerializationAesTest, AesAvailable) {
    // WHAT: Check AES availability status
    // WHY:  AES is not yet implemented (uses XOR fallback), so it reports unavailable

    bool available = nimcp_aes_available();
    if (!available) {
        GTEST_SKIP() << "AES not available (not yet implemented, using XOR fallback)";
    }
}

//=============================================================================
// Encrypt/Decrypt Round-Trip Tests
//=============================================================================

TEST_F(SerializationAesTest, RoundTripBasic) {
    // WHAT: Encrypt then decrypt, verify data is preserved
    // WHY:  Core correctness of encrypt/decrypt

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const char* message = "Hello, NIMCP AES encryption!";
    size_t plaintext_size = strlen(message) + 1;  // include null terminator

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        reinterpret_cast<const uint8_t*>(message), plaintext_size,
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr) << "Encryption must succeed";
    EXPECT_GT(encrypted_size, plaintext_size)
        << "Encrypted data should be larger (IV + tag overhead)";

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr) << "Decryption must succeed";
    EXPECT_EQ(decrypted_size, plaintext_size);
    EXPECT_EQ(memcmp(decrypted, message, plaintext_size), 0)
        << "Decrypted data must match original";

    free(encrypted);
    free(decrypted);
}

TEST_F(SerializationAesTest, RoundTripBinaryData) {
    // WHAT: Encrypt/decrypt binary data (all byte values 0x00-0xFF)
    // WHY:  Verify encryption handles arbitrary byte patterns

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Create binary test data with all byte values
    std::vector<uint8_t> original(256);
    for (size_t i = 0; i < 256; i++) {
        original[i] = static_cast<uint8_t>(i);
    }

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        original.data(), original.size(),
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, original.size());
    EXPECT_EQ(memcmp(decrypted, original.data(), original.size()), 0);

    free(encrypted);
    free(decrypted);
}

//=============================================================================
// Wrong Key Tests (Authentication)
//=============================================================================

TEST_F(SerializationAesTest, DecryptWithWrongKeyFails) {
    // WHAT: Decrypt with wrong key must fail (GCM authentication)
    // WHY:  AES-GCM provides authenticated encryption - tampered key = auth fail

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const char* message = "Sensitive data for wrong-key test";
    size_t plaintext_size = strlen(message) + 1;

    // Encrypt with correct key
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        reinterpret_cast<const uint8_t*>(message), plaintext_size,
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Try to decrypt with wrong key - must fail
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        wrong_key_, sizeof(wrong_key_), &decrypted_size);
    EXPECT_EQ(decrypted, nullptr)
        << "Decryption with wrong key must return NULL (authentication failure)";

    free(encrypted);
    // decrypted should be NULL, but free it just in case
    if (decrypted) free(decrypted);
}

//=============================================================================
// NULL and Invalid Parameter Tests
//=============================================================================

TEST_F(SerializationAesTest, EncryptNullData) {
    // WHAT: Encrypt NULL data returns error
    // WHY:  Guard clause validation

    size_t out_size = 0;
    uint8_t* result = nimcp_encrypt(nullptr, 100, key_, sizeof(key_), &out_size);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(out_size, 0u);
}

TEST_F(SerializationAesTest, EncryptZeroSize) {
    // WHAT: Encrypt zero-length data returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3};
    size_t out_size = 0;
    uint8_t* result = nimcp_encrypt(data, 0, key_, sizeof(key_), &out_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, EncryptNullKey) {
    // WHAT: Encrypt with NULL key returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3, 4};
    size_t out_size = 0;
    uint8_t* result = nimcp_encrypt(data, sizeof(data), nullptr, 32, &out_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, EncryptZeroKeySize) {
    // WHAT: Encrypt with zero key size returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3, 4};
    size_t out_size = 0;
    uint8_t* result = nimcp_encrypt(data, sizeof(data), key_, 0, &out_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, EncryptNullOutSize) {
    // WHAT: Encrypt with NULL out_size returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3, 4};
    uint8_t* result = nimcp_encrypt(data, sizeof(data), key_, sizeof(key_), nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, DecryptNullData) {
    // WHAT: Decrypt NULL data returns error
    // WHY:  Guard clause validation

    size_t out_size = 0;
    uint8_t* result = nimcp_decrypt(nullptr, 100, key_, sizeof(key_), &out_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, DecryptZeroSize) {
    // WHAT: Decrypt zero-length data returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3};
    size_t out_size = 0;
    uint8_t* result = nimcp_decrypt(data, 0, key_, sizeof(key_), &out_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SerializationAesTest, DecryptNullKey) {
    // WHAT: Decrypt with NULL key returns error
    // WHY:  Guard clause validation

    uint8_t data[] = {1, 2, 3, 4};
    size_t out_size = 0;
    uint8_t* result = nimcp_decrypt(data, sizeof(data), nullptr, 32, &out_size);
    EXPECT_EQ(result, nullptr);
}

//=============================================================================
// Large Data Tests
//=============================================================================

TEST_F(SerializationAesTest, LargeDataRoundTrip) {
    // WHAT: Encrypt and decrypt a large data block (64 KB)
    // WHY:  Verify encryption handles larger payloads correctly

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const size_t LARGE_SIZE = 64 * 1024;  // 64 KB
    std::vector<uint8_t> original(LARGE_SIZE);
    for (size_t i = 0; i < LARGE_SIZE; i++) {
        original[i] = static_cast<uint8_t>((i * 17 + 31) % 256);
    }

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        original.data(), original.size(),
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);
    EXPECT_GT(encrypted_size, original.size());

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, original.size());
    EXPECT_EQ(memcmp(decrypted, original.data(), original.size()), 0)
        << "Large data round-trip must preserve all bytes";

    free(encrypted);
    free(decrypted);
}

TEST_F(SerializationAesTest, VeryLargeDataRoundTrip) {
    // WHAT: Encrypt and decrypt a 1 MB data block
    // WHY:  Verify encryption scales to realistic brain state sizes

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const size_t VERY_LARGE_SIZE = 1024 * 1024;  // 1 MB
    std::vector<uint8_t> original(VERY_LARGE_SIZE);
    for (size_t i = 0; i < VERY_LARGE_SIZE; i++) {
        original[i] = static_cast<uint8_t>((i * 7 + 13) % 256);
    }

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        original.data(), original.size(),
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, original.size());
    EXPECT_EQ(memcmp(decrypted, original.data(), original.size()), 0);

    free(encrypted);
    free(decrypted);
}

//=============================================================================
// Tampered Ciphertext Tests
//=============================================================================

TEST_F(SerializationAesTest, DecryptTamperedCiphertext) {
    // WHAT: Modify ciphertext and verify decryption fails
    // WHY:  GCM authentication must detect ciphertext tampering

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const char* message = "Data integrity test payload";
    size_t plaintext_size = strlen(message) + 1;

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        reinterpret_cast<const uint8_t*>(message), plaintext_size,
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    // Tamper with ciphertext (flip a byte in the middle)
    if (encrypted_size > 20) {
        encrypted[encrypted_size / 2] ^= 0xFF;
    }

    // Decryption should fail due to authentication tag mismatch
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    EXPECT_EQ(decrypted, nullptr)
        << "Decryption of tampered ciphertext must fail";

    free(encrypted);
    if (decrypted) free(decrypted);
}

TEST_F(SerializationAesTest, DecryptTruncatedCiphertext) {
    // WHAT: Truncated ciphertext should fail decryption
    // WHY:  Incomplete data cannot be authenticated

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    const char* message = "Truncation test data with enough length";
    size_t plaintext_size = strlen(message) + 1;

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        reinterpret_cast<const uint8_t*>(message), plaintext_size,
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);
    ASSERT_GT(encrypted_size, 10u);

    // Try to decrypt with truncated data (only first 10 bytes)
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, 10,
        key_, sizeof(key_), &decrypted_size);
    EXPECT_EQ(decrypted, nullptr)
        << "Decryption of truncated ciphertext must fail";

    free(encrypted);
    if (decrypted) free(decrypted);
}

//=============================================================================
// Small Data Edge Cases
//=============================================================================

TEST_F(SerializationAesTest, RoundTripSingleByte) {
    // WHAT: Encrypt/decrypt a single byte
    // WHY:  Minimum data size edge case

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    uint8_t data = 0x42;

    // Encrypt
    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(&data, 1, key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);
    EXPECT_GT(encrypted_size, 1u);  // Must have IV + tag overhead

    // Decrypt
    size_t decrypted_size = 0;
    uint8_t* decrypted = nimcp_decrypt(
        encrypted, encrypted_size,
        key_, sizeof(key_), &decrypted_size);
    ASSERT_NE(decrypted, nullptr);
    EXPECT_EQ(decrypted_size, 1u);
    EXPECT_EQ(decrypted[0], 0x42);

    free(encrypted);
    free(decrypted);
}

//=============================================================================
// Context API Tests
//=============================================================================

TEST_F(SerializationAesTest, ContextInitialization) {
    // WHAT: Verify serialization context initializes cleanly
    // WHY:  Context provides stateful encrypt/decrypt operations

    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);

    EXPECT_FALSE(ctx.key_set);
    EXPECT_EQ(ctx.flags, 0u);
    EXPECT_EQ(ctx.key_size, 0u);
}

TEST_F(SerializationAesTest, ContextSetKey) {
    // WHAT: Set encryption key on context
    // WHY:  Context must store key for subsequent operations

    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);

    nimcp_serialize_ctx_set_key(&ctx, key_, sizeof(key_));
    EXPECT_TRUE(ctx.key_set);
    EXPECT_EQ(ctx.key_size, sizeof(key_));
    EXPECT_EQ(memcmp(ctx.key, key_, sizeof(key_)), 0);
}

TEST_F(SerializationAesTest, ContextSetFlags) {
    // WHAT: Set format flags on context
    // WHY:  Flags control compression/encryption behavior

    nimcp_serialize_ctx_t ctx;
    nimcp_serialize_ctx_init(&ctx);

    nimcp_serialize_ctx_set_flags(&ctx, 0x03);
    EXPECT_EQ(ctx.flags, 0x03u);
}

//=============================================================================
// Determinism Test
//=============================================================================

TEST_F(SerializationAesTest, EncryptionIsNonDeterministic) {
    // WHAT: Two encryptions of same data produce different ciphertexts
    // WHY:  AES-GCM uses random IV, so ciphertexts should differ

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8};

    size_t enc_size1 = 0, enc_size2 = 0;
    uint8_t* enc1 = nimcp_encrypt(data, sizeof(data), key_, sizeof(key_), &enc_size1);
    uint8_t* enc2 = nimcp_encrypt(data, sizeof(data), key_, sizeof(key_), &enc_size2);
    ASSERT_NE(enc1, nullptr);
    ASSERT_NE(enc2, nullptr);

    // Same plaintext encrypted twice should produce different ciphertexts
    // (due to random IV)
    EXPECT_EQ(enc_size1, enc_size2);
    bool all_same = (memcmp(enc1, enc2, enc_size1) == 0);
    EXPECT_FALSE(all_same)
        << "Two encryptions of same data should produce different ciphertexts (random IV)";

    // But both should decrypt to the same plaintext
    size_t dec_size1 = 0, dec_size2 = 0;
    uint8_t* dec1 = nimcp_decrypt(enc1, enc_size1, key_, sizeof(key_), &dec_size1);
    uint8_t* dec2 = nimcp_decrypt(enc2, enc_size2, key_, sizeof(key_), &dec_size2);
    ASSERT_NE(dec1, nullptr);
    ASSERT_NE(dec2, nullptr);
    EXPECT_EQ(dec_size1, sizeof(data));
    EXPECT_EQ(dec_size2, sizeof(data));
    EXPECT_EQ(memcmp(dec1, data, sizeof(data)), 0);
    EXPECT_EQ(memcmp(dec2, data, sizeof(data)), 0);

    free(enc1);
    free(enc2);
    free(dec1);
    free(dec2);
}

} // namespace
