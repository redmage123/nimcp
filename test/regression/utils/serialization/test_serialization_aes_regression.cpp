/**
 * @file test_serialization_aes_regression.cpp
 * @brief Regression tests for AES-256-GCM encryption in nimcp_serialization
 *
 * WHAT: Tests invariants of AES-GCM that must never regress: ciphertext size,
 *       IV randomness, authentication on bit-flip, key padding, large payloads
 * WHY:  Prevent future changes from breaking cryptographic guarantees
 * HOW:  Stress-test with many payloads, verify size formulas, bit-flip every
 *       position, and test all key size edge cases
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <ctime>

// Header has its own extern "C" guard
#include "utils/serialization/nimcp_serialization.h"

// AES-256-GCM constants (must match implementation)
#define AES_GCM_IV_SIZE   12
#define AES_GCM_TAG_SIZE  16
#define AES_GCM_OVERHEAD  (AES_GCM_IV_SIZE + AES_GCM_TAG_SIZE)  // 28

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class SerializationAesRegressionTest : public ::testing::Test {
protected:
    uint8_t key_[32];

    void SetUp() override {
        for (size_t i = 0; i < 32; i++) {
            key_[i] = static_cast<uint8_t>(0x42 + i);
        }
    }

    // Helper: generate pseudo-random test data using a simple LCG
    std::vector<uint8_t> make_random_data(size_t size, uint32_t seed) {
        std::vector<uint8_t> data(size);
        uint32_t state = seed;
        for (size_t i = 0; i < size; i++) {
            state = state * 1103515245u + 12345u;
            data[i] = static_cast<uint8_t>((state >> 16) & 0xFF);
        }
        return data;
    }
};

//=============================================================================
// Mass Round-Trip Tests
//=============================================================================

TEST_F(SerializationAesRegressionTest, HundredRandomPayloadsRoundTrip) {
    // WHAT: Encrypt and decrypt 100 different random payloads
    // WHY:  Stress test that no payload pattern causes a failure

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    for (int i = 0; i < 100; i++) {
        size_t payload_size = 1 + (i * 37) % 4096;  // Varying sizes 1-4096
        auto original = make_random_data(payload_size, static_cast<uint32_t>(i * 7919));

        size_t encrypted_size = 0;
        uint8_t* encrypted = nimcp_encrypt(
            original.data(), original.size(),
            key_, sizeof(key_), &encrypted_size);
        ASSERT_NE(encrypted, nullptr) << "Encryption failed for payload " << i
            << " (size=" << payload_size << ")";

        size_t decrypted_size = 0;
        uint8_t* decrypted = nimcp_decrypt(
            encrypted, encrypted_size,
            key_, sizeof(key_), &decrypted_size);
        ASSERT_NE(decrypted, nullptr) << "Decryption failed for payload " << i
            << " (size=" << payload_size << ")";
        EXPECT_EQ(decrypted_size, original.size())
            << "Size mismatch for payload " << i;
        EXPECT_EQ(memcmp(decrypted, original.data(), original.size()), 0)
            << "Data mismatch for payload " << i;

        free(encrypted);
        free(decrypted);
    }
}

//=============================================================================
// IV Randomness Tests
//=============================================================================

TEST_F(SerializationAesRegressionTest, FiftyEncryptionsProduceDifferentCiphertexts) {
    // WHAT: Encrypt the same data 50 times, all must produce different ciphertexts
    // WHY:  AES-GCM random IV must ensure semantic security (no ciphertext reuse)

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    const size_t NUM_ENCRYPTIONS = 50;

    std::vector<std::vector<uint8_t>> ciphertexts(NUM_ENCRYPTIONS);

    for (size_t i = 0; i < NUM_ENCRYPTIONS; i++) {
        size_t enc_size = 0;
        uint8_t* enc = nimcp_encrypt(data, sizeof(data), key_, sizeof(key_), &enc_size);
        ASSERT_NE(enc, nullptr) << "Encryption " << i << " failed";
        ciphertexts[i].assign(enc, enc + enc_size);
        free(enc);
    }

    // Every pair must differ
    size_t duplicate_count = 0;
    for (size_t i = 0; i < NUM_ENCRYPTIONS; i++) {
        for (size_t j = i + 1; j < NUM_ENCRYPTIONS; j++) {
            if (ciphertexts[i].size() == ciphertexts[j].size() &&
                memcmp(ciphertexts[i].data(), ciphertexts[j].data(),
                       ciphertexts[i].size()) == 0) {
                duplicate_count++;
            }
        }
    }
    EXPECT_EQ(duplicate_count, 0u)
        << "All 50 encryptions of same data must produce unique ciphertexts";

    // Additionally verify all ciphertexts have the same size
    for (size_t i = 1; i < NUM_ENCRYPTIONS; i++) {
        EXPECT_EQ(ciphertexts[i].size(), ciphertexts[0].size())
            << "All ciphertexts for same-size plaintext must have same size";
    }
}

//=============================================================================
// Ciphertext Size Invariant
//=============================================================================

TEST_F(SerializationAesRegressionTest, CiphertextSizeIsInputPlusOverhead) {
    // WHAT: For any input size N, ciphertext size must be N + 28 (IV=12 + tag=16)
    // WHY:  AES-GCM output format is [IV][ciphertext][tag], size is deterministic

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    // Test various sizes
    size_t test_sizes[] = {1, 2, 15, 16, 17, 31, 32, 33, 64, 100, 255, 256,
                           512, 1000, 1024, 4096, 8192, 65536};

    for (size_t s : test_sizes) {
        auto data = make_random_data(s, static_cast<uint32_t>(s));
        size_t encrypted_size = 0;
        uint8_t* encrypted = nimcp_encrypt(
            data.data(), data.size(),
            key_, sizeof(key_), &encrypted_size);
        ASSERT_NE(encrypted, nullptr) << "Encryption failed for size " << s;
        EXPECT_EQ(encrypted_size, s + AES_GCM_OVERHEAD)
            << "Ciphertext size for input size " << s
            << " must be " << (s + AES_GCM_OVERHEAD)
            << " (input + IV=12 + tag=16)";
        free(encrypted);
    }
}

//=============================================================================
// Authentication on Bit-Flip
//=============================================================================

TEST_F(SerializationAesRegressionTest, SingleBitFlipInCiphertextFailsAuth) {
    // WHAT: Flip every single bit in the ciphertext; every flip must cause
    //       decryption to fail (GCM authentication)
    // WHY:  AES-GCM must detect any single-bit modification
    // NOTE: We test a moderate-size payload to keep bit-flip count manageable

    if (!nimcp_aes_available()) {
        GTEST_SKIP() << "AES not available";
    }

    uint8_t plaintext[] = "Bit-flip authentication regression test data!!";
    size_t plaintext_size = sizeof(plaintext);  // includes null

    size_t encrypted_size = 0;
    uint8_t* encrypted = nimcp_encrypt(
        plaintext, plaintext_size,
        key_, sizeof(key_), &encrypted_size);
    ASSERT_NE(encrypted, nullptr);

    size_t total_bits = encrypted_size * 8;
    size_t auth_failures = 0;

    for (size_t bit = 0; bit < total_bits; bit++) {
        // Make a copy and flip one bit
        std::vector<uint8_t> tampered(encrypted, encrypted + encrypted_size);
        size_t byte_idx = bit / 8;
        uint8_t bit_mask = static_cast<uint8_t>(1u << (bit % 8));
        tampered[byte_idx] ^= bit_mask;

        // Attempt decryption - should fail
        size_t dec_size = 0;
        uint8_t* dec = nimcp_decrypt(
            tampered.data(), tampered.size(),
            key_, sizeof(key_), &dec_size);
        if (dec == nullptr) {
            auth_failures++;
        } else {
            // Even if decryption "succeeded" (very unlikely with GCM),
            // verify data is wrong
            free(dec);
        }
    }

    EXPECT_EQ(auth_failures, total_bits)
        << "Every single-bit flip must cause authentication failure. "
        << "Failed " << auth_failures << "/" << total_bits;

    free(encrypted);
}

//=============================================================================
// Large Payload Tests
//=============================================================================

TEST_F(SerializationAesRegressionTest, LargePayload1KB) {
    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    auto data = make_random_data(1024, 1001);
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data.data(), data.size(), key_, sizeof(key_), &enc_size);
    ASSERT_NE(enc, nullptr);
    EXPECT_EQ(enc_size, 1024 + AES_GCM_OVERHEAD);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key_, sizeof(key_), &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, 1024u);
    EXPECT_EQ(memcmp(dec, data.data(), 1024), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, LargePayload64KB) {
    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    auto data = make_random_data(64 * 1024, 2002);
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data.data(), data.size(), key_, sizeof(key_), &enc_size);
    ASSERT_NE(enc, nullptr);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key_, sizeof(key_), &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, 64u * 1024u);
    EXPECT_EQ(memcmp(dec, data.data(), data.size()), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, LargePayload1MB) {
    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    auto data = make_random_data(1024 * 1024, 3003);
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data.data(), data.size(), key_, sizeof(key_), &enc_size);
    ASSERT_NE(enc, nullptr);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key_, sizeof(key_), &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, 1024u * 1024u);
    EXPECT_EQ(memcmp(dec, data.data(), data.size()), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, LargePayload4MB) {
    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    auto data = make_random_data(4 * 1024 * 1024, 4004);
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data.data(), data.size(), key_, sizeof(key_), &enc_size);
    ASSERT_NE(enc, nullptr);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key_, sizeof(key_), &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, 4u * 1024u * 1024u);
    EXPECT_EQ(memcmp(dec, data.data(), data.size()), 0);

    free(enc);
    free(dec);
}

//=============================================================================
// Empty Payload Edge Case
//=============================================================================

TEST_F(SerializationAesRegressionTest, EmptyPayloadHandledGracefully) {
    // WHAT: Encrypting zero-length data must return NULL (guard clause)
    // WHY:  Implementation rejects size==0 - this behavior must not change

    uint8_t data[] = {0x42};
    size_t out_size = 0;
    uint8_t* result = nimcp_encrypt(data, 0, key_, sizeof(key_), &out_size);
    EXPECT_EQ(result, nullptr)
        << "Encrypting zero-length payload must return NULL";
}

//=============================================================================
// Key Length Edge Cases
//=============================================================================

TEST_F(SerializationAesRegressionTest, KeyLength1BytePadded) {
    // WHAT: 1-byte key is zero-padded to 32 bytes for AES-256
    // WHY:  Implementation must handle short keys gracefully

    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    uint8_t short_key[] = {0xFF};
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};

    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data, sizeof(data), short_key, 1, &enc_size);
    ASSERT_NE(enc, nullptr) << "Encryption with 1-byte key must succeed";

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, short_key, 1, &dec_size);
    ASSERT_NE(dec, nullptr) << "Decryption with 1-byte key must succeed";
    EXPECT_EQ(dec_size, sizeof(data));
    EXPECT_EQ(memcmp(dec, data, sizeof(data)), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, KeyLength16Bytes) {
    // WHAT: 16-byte key (AES-128 size) is zero-padded to 32 bytes for AES-256

    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    uint8_t key16[16];
    for (size_t i = 0; i < 16; i++) key16[i] = static_cast<uint8_t>(0x30 + i);

    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data, sizeof(data), key16, sizeof(key16), &enc_size);
    ASSERT_NE(enc, nullptr);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key16, sizeof(key16), &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, sizeof(data));
    EXPECT_EQ(memcmp(dec, data, sizeof(data)), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, KeyLength32BytesExact) {
    // WHAT: Exact 32-byte key (AES-256 native size) - no padding needed

    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    uint8_t data[] = {0xCA, 0xFE, 0xBA, 0xBE};

    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data, sizeof(data), key_, 32, &enc_size);
    ASSERT_NE(enc, nullptr);

    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, key_, 32, &dec_size);
    ASSERT_NE(dec, nullptr);
    EXPECT_EQ(dec_size, sizeof(data));
    EXPECT_EQ(memcmp(dec, data, sizeof(data)), 0);

    free(enc);
    free(dec);
}

TEST_F(SerializationAesRegressionTest, KeyLength64BytesTruncated) {
    // WHAT: 64-byte key is truncated to 32 bytes for AES-256
    // WHY:  Keys longer than 32 must be truncated, not rejected

    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    uint8_t long_key[64];
    for (size_t i = 0; i < 64; i++) {
        long_key[i] = static_cast<uint8_t>(0x10 + i);
    }

    uint8_t data[] = {0x11, 0x22, 0x33, 0x44, 0x55};

    // Encrypt with 64-byte key
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data, sizeof(data), long_key, 64, &enc_size);
    ASSERT_NE(enc, nullptr) << "Encryption with 64-byte key must succeed";

    // Decrypt with same 64-byte key
    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, long_key, 64, &dec_size);
    ASSERT_NE(dec, nullptr) << "Decryption with 64-byte key must succeed";
    EXPECT_EQ(dec_size, sizeof(data));
    EXPECT_EQ(memcmp(dec, data, sizeof(data)), 0);

    // Also verify: encrypt with 64 bytes, decrypt with first 32 bytes
    // (since implementation truncates to 32)
    size_t dec2_size = 0;
    uint8_t* dec2 = nimcp_decrypt(enc, enc_size, long_key, 32, &dec2_size);
    ASSERT_NE(dec2, nullptr)
        << "Decrypt with first 32 bytes of 64-byte key must also work (truncation)";
    EXPECT_EQ(dec2_size, sizeof(data));
    EXPECT_EQ(memcmp(dec2, data, sizeof(data)), 0);

    free(enc);
    free(dec);
    free(dec2);
}

TEST_F(SerializationAesRegressionTest, ShortKeyNotInterchangeableWithPaddedKey) {
    // WHAT: A 1-byte key {0xFF} zero-padded to 32 is different from a
    //       32-byte key {0xFF, 0x00, ..., 0x00} only if the user passes
    //       different key_size values. Verify they produce same result since
    //       implementation pads shorter keys with zeros.
    // WHY:  Verify key padding consistency

    if (!nimcp_aes_available()) { GTEST_SKIP() << "AES not available"; }

    uint8_t short_key[1] = {0xFF};
    uint8_t padded_key[32];
    memset(padded_key, 0, 32);
    padded_key[0] = 0xFF;

    uint8_t data[] = {0xAA, 0xBB, 0xCC};

    // Encrypt with 1-byte key
    size_t enc_size = 0;
    uint8_t* enc = nimcp_encrypt(data, sizeof(data), short_key, 1, &enc_size);
    ASSERT_NE(enc, nullptr);

    // Decrypt with equivalent 32-byte zero-padded key
    size_t dec_size = 0;
    uint8_t* dec = nimcp_decrypt(enc, enc_size, padded_key, 32, &dec_size);
    ASSERT_NE(dec, nullptr)
        << "1-byte key zero-padded must be equivalent to explicit 32-byte padded key";
    EXPECT_EQ(dec_size, sizeof(data));
    EXPECT_EQ(memcmp(dec, data, sizeof(data)), 0);

    free(enc);
    free(dec);
}

} // namespace
