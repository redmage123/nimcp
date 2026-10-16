/**
 * @file test_key_derivation_integration.cpp
 * @brief Integration tests for key derivation with encryption and bio-async
 *
 * WHAT: Test KDF integrated with encryption context and bio-async messaging
 * WHY:  Ensure key derivation works in real encryption scenarios
 * HOW:  Integration with nimcp_security encryption and bio-async
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>

// Headers have their own extern "C" guards
#include "security/nimcp_key_derivation.h"
#include "security/nimcp_constant_time.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

class KeyDerivationIntegrationTest : public ::testing::Test {
protected:
    nimcp_kdf_context_t kdf_ctx;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);
        kdf_ctx = nullptr;

        // Initialize bio-async router
        bio_router_init(nullptr);
    }

    void TearDown() override {
        if (kdf_ctx) {
            nimcp_kdf_destroy(kdf_ctx);
            kdf_ctx = nullptr;
        }
        bio_router_shutdown();
    }
};

TEST_F(KeyDerivationIntegrationTest, DeriveKeyForEncryption) {
    // Test full flow: password -> KDF -> encryption key -> encrypt/decrypt
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = true,
        .enable_statistics = true
    };

    kdf_ctx = nimcp_kdf_create(&config);
    ASSERT_NE(kdf_ctx, nullptr);

    // Derive encryption key from password
    const char* password = "my_secure_password_2025";
    uint8_t salt[32];
    uint8_t derived_key[NIMCP_SECURITY_KEY_SIZE];

    nimcp_kdf_generate_salt(salt, sizeof(salt));
    nimcp_result_t result = nimcp_kdf_derive(
        kdf_ctx, password, strlen(password),
        salt, sizeof(salt),
        derived_key, sizeof(derived_key)
    );
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Use derived key for encryption
    nimcp_encryption_context_t* enc_ctx = nimcp_encryption_create(derived_key);
    ASSERT_NE(enc_ctx, nullptr);

    // Encrypt data
    const char* plaintext = "Secret message";
    size_t plaintext_len = strlen(plaintext);
    uint8_t ciphertext[NIMCP_SECURITY_MAX_ENCRYPTED_SIZE];
    size_t ciphertext_len;

    result = nimcp_encryption_encrypt(
        enc_ctx,
        reinterpret_cast<const uint8_t*>(plaintext), plaintext_len,
        ciphertext, sizeof(ciphertext),
        &ciphertext_len
    );
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Decrypt data
    uint8_t decrypted[256];
    size_t decrypted_len;

    result = nimcp_encryption_decrypt(
        enc_ctx,
        ciphertext, ciphertext_len,
        decrypted, sizeof(decrypted),
        &decrypted_len
    );
    ASSERT_EQ(NIMCP_SUCCESS, result);
    EXPECT_EQ(plaintext_len, decrypted_len);
    EXPECT_EQ(0, memcmp(plaintext, decrypted, plaintext_len));

    // Secure wipe sensitive data
    nimcp_secure_zero(derived_key, sizeof(derived_key));
    nimcp_encryption_destroy(enc_ctx);
}

TEST_F(KeyDerivationIntegrationTest, MultipleKeysFromSamePassword) {
    // Derive multiple keys for different purposes from same password
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    kdf_ctx = nimcp_kdf_create(&config);
    ASSERT_NE(kdf_ctx, nullptr);

    const char* password = "master_password";
    uint8_t salt_encryption[32];
    uint8_t salt_authentication[32];
    uint8_t key_encryption[32];
    uint8_t key_authentication[32];

    // Generate different salts for different purposes
    nimcp_kdf_generate_salt(salt_encryption, sizeof(salt_encryption));
    nimcp_kdf_generate_salt(salt_authentication, sizeof(salt_authentication));

    // Derive encryption key
    nimcp_result_t result = nimcp_kdf_derive(
        kdf_ctx, password, strlen(password),
        salt_encryption, sizeof(salt_encryption),
        key_encryption, sizeof(key_encryption)
    );
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Derive authentication key
    result = nimcp_kdf_derive(
        kdf_ctx, password, strlen(password),
        salt_authentication, sizeof(salt_authentication),
        key_authentication, sizeof(key_authentication)
    );
    ASSERT_EQ(NIMCP_SUCCESS, result);

    // Keys should be different (different salts)
    bool keys_different = false;
    for (size_t i = 0; i < 32; i++) {
        if (key_encryption[i] != key_authentication[i]) {
            keys_different = true;
            break;
        }
    }
    EXPECT_TRUE(keys_different);

    // Secure wipe
    nimcp_secure_zero(key_encryption, sizeof(key_encryption));
    nimcp_secure_zero(key_authentication, sizeof(key_authentication));
}

TEST_F(KeyDerivationIntegrationTest, PasswordChangedDetection) {
    // Test that wrong password produces different key
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    kdf_ctx = nimcp_kdf_create(&config);
    ASSERT_NE(kdf_ctx, nullptr);

    uint8_t salt[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    // Derive key with correct password
    uint8_t key_correct[32];
    nimcp_kdf_derive(kdf_ctx, "correct_password", 16,
                     salt, sizeof(salt), key_correct, sizeof(key_correct));

    // Try with wrong password
    uint8_t key_wrong[32];
    nimcp_kdf_derive(kdf_ctx, "wrong_password", 14,
                     salt, sizeof(salt), key_wrong, sizeof(key_wrong));

    // Keys must be different
    EXPECT_NE(0, nimcp_ct_memcmp(key_correct, key_wrong, 32));

    nimcp_secure_zero(key_correct, sizeof(key_correct));
    nimcp_secure_zero(key_wrong, sizeof(key_wrong));
}

TEST_F(KeyDerivationIntegrationTest, StatisticsTracking) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = true
    };

    kdf_ctx = nimcp_kdf_create(&config);
    ASSERT_NE(kdf_ctx, nullptr);

    // Perform multiple derivations
    for (int i = 0; i < 10; i++) {
        uint8_t salt[16];
        uint8_t key[32];
        nimcp_kdf_generate_salt(salt, sizeof(salt));
        nimcp_kdf_derive(kdf_ctx, "password", 8,
                        salt, sizeof(salt), key, sizeof(key));
        nimcp_secure_zero(key, sizeof(key));
    }

    // Check statistics
    nimcp_kdf_stats_t stats;
    nimcp_kdf_get_stats(kdf_ctx, &stats);

    EXPECT_EQ(10UL, stats.derivations_performed);
    EXPECT_EQ(10 * 32UL, stats.total_bytes_derived);
    EXPECT_GT(stats.avg_derivation_time_ms, 0.0);
    EXPECT_GT(stats.total_derivation_time_ms, 0.0);
    EXPECT_EQ(NIMCP_KDF_PBKDF2_SHA256, stats.algorithm);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
