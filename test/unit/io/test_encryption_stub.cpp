/**
 * @file test_encryption_stub.cpp
 * @brief Tests for encryption stub behavior (Bug H8)
 *
 * WHAT: Verify encryption stub returns proper errors and logs warnings
 * WHY:  When libsodium is unavailable, stubs must clearly fail, not silently pass
 * HOW:  Call encryption functions and verify they return false (not success)
 *
 * NOTE: These tests exercise the stub path specifically. When libsodium IS
 * available, nimcp_encryption_available() returns true and the stubs are not
 * compiled — those tests live in test_encryption.cpp.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

// Headers have their own extern "C" guards
#include "io/serialization/nimcp_encryption.h"

class EncryptionStubTest : public ::testing::Test {
protected:
    void SetUp() override {
        // These tests are ONLY meaningful if encryption is NOT available
        // If encryption IS available, the main test_encryption.cpp covers it
        if (nimcp_encryption_available()) {
            GTEST_SKIP() << "Encryption is available (libsodium compiled) — stub tests not applicable";
        }
    }
};

/**
 * WHAT: Test that encryption_available returns false when no libsodium
 */
TEST_F(EncryptionStubTest, EncryptionUnavailable) {
    EXPECT_FALSE(nimcp_encryption_available());
}

/**
 * WHAT: Test that encrypted_size returns 0 when encryption unavailable
 */
TEST_F(EncryptionStubTest, EncryptedSizeReturnsZero) {
    EXPECT_EQ(nimcp_encrypted_size(100), 0u);
    EXPECT_EQ(nimcp_encrypted_size(0), 0u);
}

/**
 * WHAT: Test that encrypt returns false (failure) — not success
 * WHY:  Bug H8: stubs must not silently succeed
 */
TEST_F(EncryptionStubTest, EncryptReturnsFalse) {
    uint8_t plaintext[] = "test data";
    uint8_t ciphertext[256];
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        plaintext, sizeof(plaintext) - 1,
        "password", 8,
        ciphertext, &ciphertext_len
    );

    EXPECT_FALSE(result) << "Encrypt stub must return false when encryption unavailable";
}

/**
 * WHAT: Test that decrypt returns false (failure) — not success
 * WHY:  Bug H8: stubs must not silently succeed
 */
TEST_F(EncryptionStubTest, DecryptReturnsFalse) {
    uint8_t ciphertext[256] = {0};
    uint8_t plaintext[256];
    size_t plaintext_len = sizeof(plaintext);

    bool result = nimcp_decrypt_with_password(
        ciphertext, sizeof(ciphertext),
        "password", 8,
        plaintext, &plaintext_len
    );

    EXPECT_FALSE(result) << "Decrypt stub must return false when encryption unavailable";
}

/**
 * A test that always runs regardless of encryption availability.
 * Verifies the availability function is consistent.
 */
TEST(EncryptionAvailabilityTest, ConsistentAvailability) {
    bool first = nimcp_encryption_available();
    bool second = nimcp_encryption_available();
    EXPECT_EQ(first, second) << "Availability should be consistent across calls";
}
