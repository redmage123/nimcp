/**
 * @file test_encryption.cpp
 * @brief Comprehensive test suite for NIMCP encryption utilities
 *
 * WHAT: Tests for password-based encryption using libsodium
 * WHY:  Ensure encryption/decryption works correctly, securely, and handles errors
 * HOW:  Use GoogleTest framework with edge cases and security validation
 */

#include <gtest/gtest.h>
#include <string.h>
#include <string>
#include <vector>
#include <cstdint>

// Headers have their own extern "C" guards
#include "io/serialization/nimcp_encryption.h"

//=============================================================================
// Test Constants
//=============================================================================

static const char* TEST_PASSWORD = "SecureTestPassword123!";
static const char* WRONG_PASSWORD = "WrongPassword456!";
static const uint8_t TEST_PLAINTEXT[] = "Hello, NIMCP encryption test!";
static const size_t TEST_PLAINTEXT_LEN = sizeof(TEST_PLAINTEXT) - 1;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Create test data buffer
 * WHY:  Generate test plaintext of specific size
 */
static std::vector<uint8_t> create_test_data(size_t size)
{
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    return data;
}

/**
 * WHAT: Check if two buffers are different
 * WHY:  Verify encryption changes the data
 */
static bool buffers_different(const uint8_t* buf1, const uint8_t* buf2, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf1[i] != buf2[i]) {
            return true;
        }
    }
    return false;
}

//=============================================================================
// Test Fixture
//=============================================================================

class EncryptionTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Check if encryption is available
        encryption_available = nimcp_encryption_available();
        if (!encryption_available) {
            GTEST_SKIP() << "Encryption not available (libsodium not compiled)";
        }
    }

    void TearDown() override
    {
        // Nothing to clean up
    }

    bool encryption_available;
};

//=============================================================================
// Availability Tests
//=============================================================================

/**
 * WHAT: Test encryption availability check
 * WHY:  Verify we can detect if encryption is supported
 */
TEST_F(EncryptionTest, EncryptionAvailability)
{
    bool available = nimcp_encryption_available();
    EXPECT_TRUE(available) << "Encryption should be available with libsodium";
}

/**
 * WHAT: Test encrypted size calculation
 * WHY:  Verify correct buffer size is returned
 */
TEST_F(EncryptionTest, EncryptedSizeCalculation)
{
    // Encrypted size = salt (16) + nonce (24) + plaintext + tag (16)
    // Total overhead = 56 bytes
    size_t plaintext_len = 100;
    size_t encrypted_size = nimcp_encrypted_size(plaintext_len);

    EXPECT_GT(encrypted_size, plaintext_len);
    EXPECT_EQ(encrypted_size, plaintext_len + 56);
}

/**
 * WHAT: Test encrypted size for zero-length input
 * WHY:  Verify edge case handling
 */
TEST_F(EncryptionTest, EncryptedSizeZeroLength)
{
    size_t encrypted_size = nimcp_encrypted_size(0);
    EXPECT_EQ(encrypted_size, 56); // Just overhead: salt + nonce + tag
}

/**
 * WHAT: Test encrypted size for large input
 * WHY:  Verify no overflow for large sizes
 */
TEST_F(EncryptionTest, EncryptedSizeLargeInput)
{
    size_t large_len = 1024 * 1024; // 1 MB
    size_t encrypted_size = nimcp_encrypted_size(large_len);
    EXPECT_EQ(encrypted_size, large_len + 56);
}

//=============================================================================
// Basic Encryption/Decryption Tests
//=============================================================================

/**
 * WHAT: Test basic encryption
 * WHY:  Verify encryption succeeds with valid inputs
 */
TEST_F(EncryptionTest, BasicEncryption)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT,
        TEST_PLAINTEXT_LEN,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        ciphertext.data(),
        &ciphertext_len
    );

    EXPECT_TRUE(result);
    EXPECT_GT(ciphertext_len, TEST_PLAINTEXT_LEN);
    EXPECT_EQ(ciphertext_len, nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
}

/**
 * WHAT: Test basic decryption
 * WHY:  Verify decryption succeeds with correct password
 */
TEST_F(EncryptionTest, BasicDecryption)
{
    // First encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT,
        TEST_PLAINTEXT_LEN,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        ciphertext.data(),
        &ciphertext_len
    );

    // Then decrypt
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(),
        ciphertext_len,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        plaintext.data(),
        &plaintext_len
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(plaintext_len, TEST_PLAINTEXT_LEN);
    EXPECT_EQ(memcmp(plaintext.data(), TEST_PLAINTEXT, TEST_PLAINTEXT_LEN), 0);
}

/**
 * WHAT: Test encrypt/decrypt round-trip
 * WHY:  Verify data integrity through encryption/decryption cycle
 */
TEST_F(EncryptionTest, EncryptDecryptRoundTrip)
{
    const char* original = "Round-trip test data with various chars: !@#$%^&*()";
    size_t original_len = strlen(original);

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(original_len));
    size_t ciphertext_len = ciphertext.size();

    bool enc_result = nimcp_encrypt_with_password(
        reinterpret_cast<const uint8_t*>(original),
        original_len,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        ciphertext.data(),
        &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Decrypt
    std::vector<uint8_t> decrypted(original_len);
    size_t decrypted_len = decrypted.size();

    bool dec_result = nimcp_decrypt_with_password(
        ciphertext.data(),
        ciphertext_len,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        decrypted.data(),
        &decrypted_len
    );
    ASSERT_TRUE(dec_result);

    // Verify
    EXPECT_EQ(decrypted_len, original_len);
    decrypted.push_back(0); // Null terminate for string comparison
    EXPECT_STREQ(reinterpret_cast<const char*>(decrypted.data()), original);
}

/**
 * WHAT: Test ciphertext is different from plaintext
 * WHY:  Verify encryption actually changes the data
 */
TEST_F(EncryptionTest, CiphertextDifferentFromPlaintext)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT,
        TEST_PLAINTEXT_LEN,
        TEST_PASSWORD,
        strlen(TEST_PASSWORD),
        ciphertext.data(),
        &ciphertext_len
    );

    // Skip salt and nonce (first 40 bytes), check encrypted data is different
    size_t offset = 40; // salt (16) + nonce (24)
    EXPECT_TRUE(buffers_different(
        ciphertext.data() + offset,
        TEST_PLAINTEXT,
        std::min(TEST_PLAINTEXT_LEN, ciphertext_len - offset)
    ));
}

/**
 * WHAT: Test same plaintext produces different ciphertext
 * WHY:  Verify random nonces prevent ciphertext reuse
 */
TEST_F(EncryptionTest, DifferentCiphertextForSamePlaintext)
{
    std::vector<uint8_t> ciphertext1(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    std::vector<uint8_t> ciphertext2(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t len1 = ciphertext1.size();
    size_t len2 = ciphertext2.size();

    // Encrypt same data twice
    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext1.data(), &len1
    );

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext2.data(), &len2
    );

    // Ciphertexts should be different (different salts/nonces)
    EXPECT_NE(memcmp(ciphertext1.data(), ciphertext2.data(), len1), 0);
}

//=============================================================================
// Password Validation Tests
//=============================================================================

/**
 * WHAT: Test decryption with wrong password fails
 * WHY:  Verify authentication prevents unauthorized decryption
 */
TEST_F(EncryptionTest, WrongPasswordFailsDecryption)
{
    // Encrypt with correct password
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Try to decrypt with wrong password
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        WRONG_PASSWORD, strlen(WRONG_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result) << "Decryption should fail with wrong password";
}

/**
 * WHAT: Test different passwords produce different ciphertexts
 * WHY:  Verify password uniqueness affects encryption
 */
TEST_F(EncryptionTest, DifferentPasswordsDifferentCiphertext)
{
    const char* password1 = "password1";
    const char* password2 = "password2";

    std::vector<uint8_t> ciphertext1(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    std::vector<uint8_t> ciphertext2(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t len1 = ciphertext1.size();
    size_t len2 = ciphertext2.size();

    // Encrypt with different passwords
    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        password1, strlen(password1),
        ciphertext1.data(), &len1
    );

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        password2, strlen(password2),
        ciphertext2.data(), &len2
    );

    // Ciphertexts should be different
    EXPECT_NE(memcmp(ciphertext1.data(), ciphertext2.data(), len1), 0);
}

/**
 * WHAT: Test empty password handling
 * WHY:  Verify empty passwords are rejected
 */
TEST_F(EncryptionTest, EmptyPasswordRejected)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        "", 0, // Empty password
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_FALSE(result) << "Empty password should be rejected";
}

/**
 * WHAT: Test long password
 * WHY:  Verify system handles long passwords correctly
 */
TEST_F(EncryptionTest, LongPassword)
{
    std::string long_password(1000, 'x');

    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool enc_result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        long_password.c_str(), long_password.length(),
        ciphertext.data(), &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Decrypt with same long password
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool dec_result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        long_password.c_str(), long_password.length(),
        plaintext.data(), &plaintext_len
    );
    ASSERT_TRUE(dec_result);

    EXPECT_EQ(memcmp(plaintext.data(), TEST_PLAINTEXT, TEST_PLAINTEXT_LEN), 0);
}

/**
 * WHAT: Test password with special characters
 * WHY:  Verify UTF-8 passwords work correctly
 */
TEST_F(EncryptionTest, PasswordWithSpecialCharacters)
{
    const char* special_password = "P@ssw0rd!#$%^&*()_+-=[]{}|;':\",./<>?";

    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool enc_result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        special_password, strlen(special_password),
        ciphertext.data(), &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Decrypt
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool dec_result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        special_password, strlen(special_password),
        plaintext.data(), &plaintext_len
    );
    ASSERT_TRUE(dec_result);

    EXPECT_EQ(memcmp(plaintext.data(), TEST_PLAINTEXT, TEST_PLAINTEXT_LEN), 0);
}

//=============================================================================
// NULL Pointer Tests
//=============================================================================

/**
 * WHAT: Test encryption with NULL plaintext
 * WHY:  Verify NULL pointer is rejected
 */
TEST_F(EncryptionTest, EncryptNullPlaintext)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        nullptr, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test encryption with NULL password
 * WHY:  Verify NULL password is rejected
 */
TEST_F(EncryptionTest, EncryptNullPassword)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        nullptr, 10,
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test encryption with NULL ciphertext buffer
 * WHY:  Verify NULL output buffer is rejected
 */
TEST_F(EncryptionTest, EncryptNullCiphertext)
{
    size_t ciphertext_len = nimcp_encrypted_size(TEST_PLAINTEXT_LEN);

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        nullptr, &ciphertext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test encryption with NULL ciphertext_len pointer
 * WHY:  Verify NULL size pointer is rejected
 */
TEST_F(EncryptionTest, EncryptNullCiphertextLen)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), nullptr
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test decryption with NULL ciphertext
 * WHY:  Verify NULL pointer is rejected
 */
TEST_F(EncryptionTest, DecryptNullCiphertext)
{
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        nullptr, 100,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test decryption with NULL password
 * WHY:  Verify NULL password is rejected
 */
TEST_F(EncryptionTest, DecryptNullPassword)
{
    std::vector<uint8_t> ciphertext(100);
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext.size(),
        nullptr, 10,
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test decryption with NULL plaintext buffer
 * WHY:  Verify NULL output buffer is rejected
 */
TEST_F(EncryptionTest, DecryptNullPlaintext)
{
    std::vector<uint8_t> ciphertext(100);
    size_t plaintext_len = TEST_PLAINTEXT_LEN;

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        nullptr, &plaintext_len
    );

    EXPECT_FALSE(result);
}

/**
 * WHAT: Test decryption with NULL plaintext_len pointer
 * WHY:  Verify NULL size pointer is rejected
 */
TEST_F(EncryptionTest, DecryptNullPlaintextLen)
{
    std::vector<uint8_t> ciphertext(100);
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), nullptr
    );

    EXPECT_FALSE(result);
}

//=============================================================================
// Buffer Size Tests
//=============================================================================

/**
 * WHAT: Test encryption with insufficient output buffer
 * WHY:  Verify buffer overflow protection
 */
TEST_F(EncryptionTest, EncryptInsufficientBuffer)
{
    std::vector<uint8_t> ciphertext(10); // Too small
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_FALSE(result);
    // Size should be updated to required size
    EXPECT_EQ(ciphertext_len, nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
}

/**
 * WHAT: Test decryption with insufficient output buffer
 * WHY:  Verify buffer overflow protection
 */
TEST_F(EncryptionTest, DecryptInsufficientBuffer)
{
    // First encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Try to decrypt with too small buffer
    std::vector<uint8_t> plaintext(5); // Too small
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result);
    // Size should be updated to required size
    EXPECT_EQ(plaintext_len, TEST_PLAINTEXT_LEN);
}

/**
 * WHAT: Test encryption with zero-length plaintext
 * WHY:  Verify edge case handling
 */
TEST_F(EncryptionTest, EncryptZeroLengthPlaintext)
{
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(0));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, 0, // Zero length
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_FALSE(result) << "Zero-length plaintext should be rejected";
}

/**
 * WHAT: Test decryption with too-small ciphertext
 * WHY:  Verify minimum size validation
 */
TEST_F(EncryptionTest, DecryptTooSmallCiphertext)
{
    std::vector<uint8_t> ciphertext(40); // Less than minimum (56 bytes)
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result) << "Too-small ciphertext should be rejected";
}

//=============================================================================
// Data Integrity Tests
//=============================================================================

/**
 * WHAT: Test tampered ciphertext is rejected
 * WHY:  Verify authentication tag prevents tampering
 */
TEST_F(EncryptionTest, TamperedCiphertextRejected)
{
    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Tamper with ciphertext (modify a byte in encrypted data)
    if (ciphertext_len > 50) {
        ciphertext[50] ^= 0x01; // Flip one bit
    }

    // Try to decrypt tampered data
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result) << "Tampered ciphertext should fail authentication";
}

/**
 * WHAT: Test truncated ciphertext is rejected
 * WHY:  Verify length validation
 */
TEST_F(EncryptionTest, TruncatedCiphertextRejected)
{
    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Try to decrypt with truncated ciphertext
    std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
    size_t plaintext_len = plaintext.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len - 10, // Truncated
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext.data(), &plaintext_len
    );

    EXPECT_FALSE(result) << "Truncated ciphertext should be rejected";
}

//=============================================================================
// Large Data Tests
//=============================================================================

/**
 * WHAT: Test encryption of large data
 * WHY:  Verify system handles large plaintexts correctly
 */
TEST_F(EncryptionTest, EncryptLargeData)
{
    size_t large_size = 10 * 1024; // 10 KB
    std::vector<uint8_t> plaintext = create_test_data(large_size);

    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(large_size));
    size_t ciphertext_len = ciphertext.size();

    bool result = nimcp_encrypt_with_password(
        plaintext.data(), plaintext.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_TRUE(result);
    EXPECT_GT(ciphertext_len, large_size);
}

/**
 * WHAT: Test decryption of large data
 * WHY:  Verify system handles large ciphertexts correctly
 */
TEST_F(EncryptionTest, DecryptLargeData)
{
    size_t large_size = 10 * 1024; // 10 KB
    std::vector<uint8_t> plaintext_orig = create_test_data(large_size);

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(large_size));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        plaintext_orig.data(), plaintext_orig.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Decrypt
    std::vector<uint8_t> plaintext_dec(large_size);
    size_t plaintext_len = plaintext_dec.size();

    bool result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext_dec.data(), &plaintext_len
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(plaintext_len, large_size);
    EXPECT_EQ(memcmp(plaintext_dec.data(), plaintext_orig.data(), large_size), 0);
}

/**
 * WHAT: Test very large data (1 MB)
 * WHY:  Verify no size limitations or performance issues
 */
TEST_F(EncryptionTest, VeryLargeData)
{
    size_t very_large_size = 1024 * 1024; // 1 MB
    std::vector<uint8_t> plaintext_orig = create_test_data(very_large_size);

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(very_large_size));
    size_t ciphertext_len = ciphertext.size();

    bool enc_result = nimcp_encrypt_with_password(
        plaintext_orig.data(), plaintext_orig.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );
    ASSERT_TRUE(enc_result);

    // Decrypt
    std::vector<uint8_t> plaintext_dec(very_large_size);
    size_t plaintext_len = plaintext_dec.size();

    bool dec_result = nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        plaintext_dec.data(), &plaintext_len
    );
    ASSERT_TRUE(dec_result);

    EXPECT_EQ(plaintext_len, very_large_size);
    EXPECT_EQ(memcmp(plaintext_dec.data(), plaintext_orig.data(), very_large_size), 0);
}

//=============================================================================
// Special Data Tests
//=============================================================================

/**
 * WHAT: Test encryption of binary data
 * WHY:  Verify system handles all byte values correctly
 */
TEST_F(EncryptionTest, BinaryData)
{
    // Create binary data with all byte values
    std::vector<uint8_t> binary_data(256);
    for (int i = 0; i < 256; i++) {
        binary_data[i] = static_cast<uint8_t>(i);
    }

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(binary_data.size()));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        binary_data.data(), binary_data.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Decrypt
    std::vector<uint8_t> decrypted(binary_data.size());
    size_t decrypted_len = decrypted.size();

    nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        decrypted.data(), &decrypted_len
    );

    // Verify all bytes
    EXPECT_EQ(decrypted_len, binary_data.size());
    for (size_t i = 0; i < binary_data.size(); i++) {
        EXPECT_EQ(decrypted[i], static_cast<uint8_t>(i));
    }
}

/**
 * WHAT: Test encryption of data with all zeros
 * WHY:  Verify encryption doesn't leak patterns
 */
TEST_F(EncryptionTest, AllZerosData)
{
    std::vector<uint8_t> zeros(100, 0);

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(zeros.size()));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        zeros.data(), zeros.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Ciphertext should not be all zeros
    bool has_nonzero = false;
    for (size_t i = 40; i < ciphertext_len; i++) { // Skip salt/nonce
        if (ciphertext[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    // Decrypt and verify
    std::vector<uint8_t> decrypted(zeros.size());
    size_t decrypted_len = decrypted.size();

    nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        decrypted.data(), &decrypted_len
    );

    EXPECT_EQ(memcmp(decrypted.data(), zeros.data(), zeros.size()), 0);
}

/**
 * WHAT: Test encryption of data with all ones
 * WHY:  Verify encryption handles uniform data correctly
 */
TEST_F(EncryptionTest, AllOnesData)
{
    std::vector<uint8_t> ones(100, 0xFF);

    // Encrypt
    std::vector<uint8_t> ciphertext(nimcp_encrypted_size(ones.size()));
    size_t ciphertext_len = ciphertext.size();

    nimcp_encrypt_with_password(
        ones.data(), ones.size(),
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    // Decrypt and verify
    std::vector<uint8_t> decrypted(ones.size());
    size_t decrypted_len = decrypted.size();

    nimcp_decrypt_with_password(
        ciphertext.data(), ciphertext_len,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        decrypted.data(), &decrypted_len
    );

    EXPECT_EQ(memcmp(decrypted.data(), ones.data(), ones.size()), 0);
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

/**
 * WHAT: Test no buffer overflow with exact-size buffer
 * WHY:  Verify buffer bounds are respected
 */
TEST_F(EncryptionTest, ExactSizeBuffer)
{
    size_t required_size = nimcp_encrypted_size(TEST_PLAINTEXT_LEN);
    std::vector<uint8_t> ciphertext(required_size);
    size_t ciphertext_len = required_size;

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(ciphertext_len, required_size);
}

/**
 * WHAT: Test oversized buffer is handled correctly
 * WHY:  Verify extra buffer space doesn't cause issues
 */
TEST_F(EncryptionTest, OversizedBuffer)
{
    size_t oversized = nimcp_encrypted_size(TEST_PLAINTEXT_LEN) + 1000;
    std::vector<uint8_t> ciphertext(oversized);
    size_t ciphertext_len = oversized;

    bool result = nimcp_encrypt_with_password(
        TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
        TEST_PASSWORD, strlen(TEST_PASSWORD),
        ciphertext.data(), &ciphertext_len
    );

    EXPECT_TRUE(result);
    EXPECT_LE(ciphertext_len, oversized);
    EXPECT_EQ(ciphertext_len, nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * WHAT: Test multiple encrypt/decrypt cycles
 * WHY:  Verify repeated use doesn't cause issues
 */
TEST_F(EncryptionTest, MultipleEncryptDecryptCycles)
{
    for (int i = 0; i < 10; i++) {
        std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
        size_t ciphertext_len = ciphertext.size();

        bool enc_result = nimcp_encrypt_with_password(
            TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
            TEST_PASSWORD, strlen(TEST_PASSWORD),
            ciphertext.data(), &ciphertext_len
        );
        ASSERT_TRUE(enc_result) << "Encryption failed on iteration " << i;

        std::vector<uint8_t> plaintext(TEST_PLAINTEXT_LEN);
        size_t plaintext_len = plaintext.size();

        bool dec_result = nimcp_decrypt_with_password(
            ciphertext.data(), ciphertext_len,
            TEST_PASSWORD, strlen(TEST_PASSWORD),
            plaintext.data(), &plaintext_len
        );
        ASSERT_TRUE(dec_result) << "Decryption failed on iteration " << i;

        EXPECT_EQ(memcmp(plaintext.data(), TEST_PLAINTEXT, TEST_PLAINTEXT_LEN), 0)
            << "Data mismatch on iteration " << i;
    }
}

/**
 * WHAT: Test concurrent encryption operations
 * WHY:  Verify thread safety (if applicable)
 */
TEST_F(EncryptionTest, ConcurrentEncryption)
{
    const int NUM_THREADS = 4;
    std::vector<bool> results(NUM_THREADS, false);

    auto encrypt_worker = [&](int thread_id) {
        std::vector<uint8_t> ciphertext(nimcp_encrypted_size(TEST_PLAINTEXT_LEN));
        size_t ciphertext_len = ciphertext.size();

        results[thread_id] = nimcp_encrypt_with_password(
            TEST_PLAINTEXT, TEST_PLAINTEXT_LEN,
            TEST_PASSWORD, strlen(TEST_PASSWORD),
            ciphertext.data(), &ciphertext_len
        );
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(encrypt_worker, i);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // All encryptions should succeed
    for (int i = 0; i < NUM_THREADS; i++) {
        EXPECT_TRUE(results[i]) << "Thread " << i << " failed";
    }
}
