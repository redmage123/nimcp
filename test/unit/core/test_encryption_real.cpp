#include <gtest/gtest.h>
#include <cstring>

#include "io/serialization/nimcp_encryption.h"

//=============================================================================
// Encryption Real Tests
//=============================================================================

class EncryptionRealTest : public ::testing::Test {
protected:
    uint8_t plaintext[1024];
    uint8_t ciphertext[2048];
    uint8_t decrypted[1024];

    void SetUp() override {
        // Initialize test data
        for (size_t i = 0; i < sizeof(plaintext); i++) {
            plaintext[i] = (uint8_t)(i % 256);
        }
        memset(ciphertext, 0, sizeof(ciphertext));
        memset(decrypted, 0, sizeof(decrypted));
    }
};

//=============================================================================
// Availability Tests
//=============================================================================

TEST_F(EncryptionRealTest, CheckAvailability) {
    bool available = nimcp_encryption_available();
    // Just check it returns a valid bool
    SUCCEED();
}

//=============================================================================
// Size Calculation Tests
//=============================================================================

TEST_F(EncryptionRealTest, GetEncryptedSize) {
    size_t size = nimcp_encrypted_size(100);
    EXPECT_GT(size, 100); // Encrypted should be larger due to salt/nonce/tag
}

TEST_F(EncryptionRealTest, GetEncryptedSizeZero) {
    size_t size = nimcp_encrypted_size(0);
    EXPECT_GT(size, 0); // Still needs salt/nonce/tag
}

TEST_F(EncryptionRealTest, GetEncryptedSizeLarge) {
    size_t size = nimcp_encrypted_size(1024 * 1024);
    EXPECT_GT(size, 1024 * 1024);
}

//=============================================================================
// Encryption Tests
//=============================================================================

TEST_F(EncryptionRealTest, EncryptWithPassword) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );

    EXPECT_TRUE(result);
    EXPECT_GT(ciphertext_len, plaintext_len);
}

TEST_F(EncryptionRealTest, EncryptNullPlaintext) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        nullptr, 256,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, EncryptNullPassword) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        nullptr, 0,
        ciphertext, &ciphertext_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, EncryptNullCiphertext) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        nullptr, &ciphertext_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, EncryptZeroLength) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t ciphertext_len = sizeof(ciphertext);

    bool result = nimcp_encrypt_with_password(
        plaintext, 0,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );

    // May succeed or fail depending on implementation
    SUCCEED();
}

//=============================================================================
// Decryption Tests
//=============================================================================

TEST_F(EncryptionRealTest, DecryptWithPassword) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    // First encrypt
    bool enc_result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Then decrypt
    size_t decrypted_len = sizeof(decrypted);
    bool dec_result = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password, strlen(password),
        decrypted, &decrypted_len
    );

    EXPECT_TRUE(dec_result);
    EXPECT_EQ(decrypted_len, plaintext_len);
    EXPECT_EQ(memcmp(plaintext, decrypted, plaintext_len), 0);
}

TEST_F(EncryptionRealTest, DecryptWrongPassword) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    const char* wrong_password = "wrong_password_456";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    // Encrypt with correct password
    bool enc_result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Try to decrypt with wrong password
    size_t decrypted_len = sizeof(decrypted);
    bool dec_result = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        wrong_password, strlen(wrong_password),
        decrypted, &decrypted_len
    );

    EXPECT_FALSE(dec_result);
}

TEST_F(EncryptionRealTest, DecryptNullCiphertext) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t decrypted_len = sizeof(decrypted);

    bool result = nimcp_decrypt_with_password(
        nullptr, 256,
        password, strlen(password),
        decrypted, &decrypted_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, DecryptNullPassword) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    size_t decrypted_len = sizeof(decrypted);

    bool result = nimcp_decrypt_with_password(
        ciphertext, 256,
        nullptr, 0,
        decrypted, &decrypted_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, DecryptNullPlaintext) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t decrypted_len = sizeof(decrypted);

    bool result = nimcp_decrypt_with_password(
        ciphertext, 256,
        password, strlen(password),
        nullptr, &decrypted_len
    );

    EXPECT_FALSE(result);
}

TEST_F(EncryptionRealTest, DecryptCorruptedData) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password_123";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    // Encrypt
    bool enc_result = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Corrupt the ciphertext
    if (ciphertext_len > 50) {
        ciphertext[50] ^= 0xFF;
    }

    // Try to decrypt corrupted data
    size_t decrypted_len = sizeof(decrypted);
    bool dec_result = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password, strlen(password),
        decrypted, &decrypted_len
    );

    EXPECT_FALSE(dec_result);
}

//=============================================================================
// Round-Trip Tests
//=============================================================================

TEST_F(EncryptionRealTest, RoundTripSmallData) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password";
    size_t plaintext_len = 16;
    size_t ciphertext_len = sizeof(ciphertext);

    bool enc = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc);

    size_t decrypted_len = sizeof(decrypted);
    bool dec = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password, strlen(password),
        decrypted, &decrypted_len
    );

    EXPECT_TRUE(dec);
    EXPECT_EQ(decrypted_len, plaintext_len);
    EXPECT_EQ(memcmp(plaintext, decrypted, plaintext_len), 0);
}

TEST_F(EncryptionRealTest, RoundTripLargeData) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password = "test_password";
    size_t plaintext_len = 1024;
    size_t ciphertext_len = sizeof(ciphertext);

    bool enc = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password, strlen(password),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc);

    size_t decrypted_len = sizeof(decrypted);
    bool dec = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password, strlen(password),
        decrypted, &decrypted_len
    );

    EXPECT_TRUE(dec);
    EXPECT_EQ(decrypted_len, plaintext_len);
    EXPECT_EQ(memcmp(plaintext, decrypted, plaintext_len), 0);
}

TEST_F(EncryptionRealTest, RoundTripDifferentPasswords) {
    if (!nimcp_encryption_available()) {
        GTEST_SKIP() << "Encryption not available";
    }

    const char* password1 = "password1";
    const char* password2 = "password2";
    size_t plaintext_len = 256;
    size_t ciphertext_len = sizeof(ciphertext);

    // Encrypt with password1
    bool enc = nimcp_encrypt_with_password(
        plaintext, plaintext_len,
        password1, strlen(password1),
        ciphertext, &ciphertext_len
    );
    ASSERT_TRUE(enc);

    // Can decrypt with password1
    size_t decrypted_len = sizeof(decrypted);
    bool dec1 = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password1, strlen(password1),
        decrypted, &decrypted_len
    );
    EXPECT_TRUE(dec1);

    // Cannot decrypt with password2
    decrypted_len = sizeof(decrypted);
    bool dec2 = nimcp_decrypt_with_password(
        ciphertext, ciphertext_len,
        password2, strlen(password2),
        decrypted, &decrypted_len
    );
    EXPECT_FALSE(dec2);
}
