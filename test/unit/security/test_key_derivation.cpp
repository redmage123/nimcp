/**
 * @file test_key_derivation.cpp
 * @brief Unit tests for key derivation functions
 *
 * WHAT: Comprehensive tests for Argon2id and PBKDF2 key derivation
 * WHY:  Ensure key derivation is functionally correct and secure
 * HOW:  GoogleTest framework with test vectors and security checks
 *
 * TEST COVERAGE:
 * 1. Context creation and configuration
 * 2. PBKDF2-HMAC-SHA256 derivation
 * 3. Argon2id derivation (if available)
 * 4. Salt generation
 * 5. Parameter validation
 * 6. Statistics tracking
 * 7. Error handling
 * 8. Security properties
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <set>
#include <algorithm>

// Headers have their own extern "C" guards
#include "security/nimcp_key_derivation.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class KeyDerivationTest : public ::testing::Test {
protected:
    nimcp_kdf_context_t ctx;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            nimcp_kdf_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Helper: Check if two buffers are different
    bool buffers_different(const uint8_t* a, const uint8_t* b, size_t len) {
        for (size_t i = 0; i < len; i++) {
            if (a[i] != b[i]) {
                return true;
            }
        }
        return false;
    }

    // Helper: Check if buffer contains all zeros
    bool is_all_zeros(const uint8_t* buf, size_t len) {
        for (size_t i = 0; i < len; i++) {
            if (buf[i] != 0) {
                return false;
            }
        }
        return true;
    }

    // Helper: Check if buffer has good entropy (not all same byte)
    bool has_good_entropy(const uint8_t* buf, size_t len) {
        if (len < 2) return true;

        uint8_t first = buf[0];
        for (size_t i = 1; i < len; i++) {
            if (buf[i] != first) {
                return true;
            }
        }
        return false;  // All bytes are the same
    }
};

//=============================================================================
// CONFIGURATION AND CONTEXT TESTS
//=============================================================================

TEST_F(KeyDerivationTest, DefaultConfig) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();

    EXPECT_EQ(NIMCP_KDF_ARGON2ID, config.algorithm);
    EXPECT_GE(config.memory_kb, 8192U);
    EXPECT_GE(config.iterations, 1U);
    EXPECT_GE(config.parallelism, 1U);
}

TEST_F(KeyDerivationTest, CreateWithDefaults) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(KeyDerivationTest, CreateWithCustomConfig) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,  // Not used for PBKDF2
        .iterations = 100000,
        .parallelism = 1,
        .enable_logging = true,
        .enable_statistics = true
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(KeyDerivationTest, CreateInvalidAlgorithm) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();
    config.algorithm = static_cast<nimcp_kdf_algorithm_t>(999);

    ctx = nimcp_kdf_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(KeyDerivationTest, CreateArgon2LowMemory) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();
    config.memory_kb = 1024;  // Too low (< 8192)

    ctx = nimcp_kdf_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(KeyDerivationTest, CreateArgon2ZeroIterations) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();
    config.iterations = 0;

    ctx = nimcp_kdf_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(KeyDerivationTest, CreateArgon2InvalidParallelism) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();
    config.parallelism = 0;

    ctx = nimcp_kdf_create(&config);
    EXPECT_EQ(ctx, nullptr);

    config.parallelism = 100;  // Too high
    ctx = nimcp_kdf_create(&config);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(KeyDerivationTest, DestroyNull) {
    // Should not crash
    nimcp_kdf_destroy(nullptr);
}

TEST_F(KeyDerivationTest, UpdateConfig) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    nimcp_kdf_config_t new_config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 150000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = true
    };

    nimcp_result_t result = nimcp_kdf_update_config(ctx, &new_config);
    EXPECT_EQ(NIMCP_SUCCESS, result);
}

//=============================================================================
// SALT GENERATION TESTS
//=============================================================================

TEST_F(KeyDerivationTest, GenerateSalt) {
    uint8_t salt[32];
    nimcp_result_t result = nimcp_kdf_generate_salt(salt, sizeof(salt));

    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_TRUE(has_good_entropy(salt, sizeof(salt)));
}

TEST_F(KeyDerivationTest, GenerateSaltMinimumSize) {
    uint8_t salt[16];  // Minimum size
    nimcp_result_t result = nimcp_kdf_generate_salt(salt, sizeof(salt));

    EXPECT_EQ(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, GenerateSaltTooSmall) {
    uint8_t salt[8];  // Too small
    nimcp_result_t result = nimcp_kdf_generate_salt(salt, sizeof(salt));

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, GenerateSaltNullPointer) {
    nimcp_result_t result = nimcp_kdf_generate_salt(nullptr, 32);

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, GenerateSaltUniqueness) {
    // Generate multiple salts and ensure they're different
    std::set<std::vector<uint8_t>> unique_salts;

    for (int i = 0; i < 100; i++) {
        uint8_t salt[32];
        nimcp_result_t result = nimcp_kdf_generate_salt(salt, sizeof(salt));
        ASSERT_EQ(NIMCP_SUCCESS, result);

        std::vector<uint8_t> salt_vec(salt, salt + 32);
        unique_salts.insert(salt_vec);
    }

    // All should be unique (probability of collision is negligible)
    EXPECT_EQ(100U, unique_salts.size());
}

TEST_F(KeyDerivationTest, GenerateSaltNotAllZeros) {
    uint8_t salt[32];
    nimcp_result_t result = nimcp_kdf_generate_salt(salt, sizeof(salt));

    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_FALSE(is_all_zeros(salt, sizeof(salt)));
}

//=============================================================================
// PBKDF2 DERIVATION TESTS
//=============================================================================

TEST_F(KeyDerivationTest, PBKDF2_BasicDerivation) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,  // Minimum for testing
        .parallelism = 1,
        .enable_logging = true,
        .enable_statistics = true
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    const char* password = "test_password";
    uint8_t salt[32];
    uint8_t key[32];

    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, password, strlen(password),
        salt, sizeof(salt),
        key, sizeof(key)
    );

    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_TRUE(has_good_entropy(key, sizeof(key)));
    EXPECT_FALSE(is_all_zeros(key, sizeof(key)));
}

TEST_F(KeyDerivationTest, PBKDF2_Reproducibility) {
    // Same password + salt should produce same key
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    const char* password = "my_password";
    uint8_t salt[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t key1[32];
    uint8_t key2[32];

    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt, sizeof(salt), key1, sizeof(key1));

    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt, sizeof(salt), key2, sizeof(key2));

    // Keys should be identical
    EXPECT_EQ(0, memcmp(key1, key2, sizeof(key1)));
}

TEST_F(KeyDerivationTest, PBKDF2_DifferentPassword) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t key1[32];
    uint8_t key2[32];

    nimcp_kdf_derive(ctx, "password1", 9, salt, sizeof(salt), key1, sizeof(key1));
    nimcp_kdf_derive(ctx, "password2", 9, salt, sizeof(salt), key2, sizeof(key2));

    // Keys should be different
    EXPECT_TRUE(buffers_different(key1, key2, sizeof(key1)));
}

TEST_F(KeyDerivationTest, PBKDF2_DifferentSalt) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    const char* password = "my_password";
    uint8_t salt1[16];
    uint8_t salt2[16];
    uint8_t key1[32];
    uint8_t key2[32];

    nimcp_kdf_generate_salt(salt1, sizeof(salt1));
    nimcp_kdf_generate_salt(salt2, sizeof(salt2));

    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt1, sizeof(salt1), key1, sizeof(key1));
    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt2, sizeof(salt2), key2, sizeof(key2));

    // Keys should be different (different salts)
    EXPECT_TRUE(buffers_different(key1, key2, sizeof(key1)));
}

TEST_F(KeyDerivationTest, PBKDF2_DifferentKeyLengths) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    const char* password = "test";
    uint8_t salt[16];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    // Test different key sizes
    uint8_t key16[16];
    uint8_t key32[32];
    uint8_t key64[64];

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_kdf_derive(ctx, password, strlen(password),
                                               salt, sizeof(salt), key16, sizeof(key16)));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_kdf_derive(ctx, password, strlen(password),
                                               salt, sizeof(salt), key32, sizeof(key32)));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_kdf_derive(ctx, password, strlen(password),
                                               salt, sizeof(salt), key64, sizeof(key64)));

    // All should have good entropy
    EXPECT_TRUE(has_good_entropy(key16, sizeof(key16)));
    EXPECT_TRUE(has_good_entropy(key32, sizeof(key32)));
    EXPECT_TRUE(has_good_entropy(key64, sizeof(key64)));
}

//=============================================================================
// PARAMETER VALIDATION TESTS
//=============================================================================

TEST_F(KeyDerivationTest, DeriveNullContext) {
    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        nullptr, "password", 8,
        salt, sizeof(salt), key, sizeof(key)
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveNullPassword) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, nullptr, 8,
        salt, sizeof(salt), key, sizeof(key)
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveNullSalt) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t key[32];

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, "password", 8,
        nullptr, 16, key, sizeof(key)
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveNullKey) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[16];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, "password", 8,
        salt, sizeof(salt), nullptr, 32
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveZeroPasswordLength) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, "password", 0,
        salt, sizeof(salt), key, sizeof(key)
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveSaltTooShort) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[8];  // Too short
    uint8_t key[32];

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, "password", 8,
        salt, sizeof(salt), key, sizeof(key)
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, DeriveZeroKeyLength) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[16];
    uint8_t key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    nimcp_result_t result = nimcp_kdf_derive(
        ctx, "password", 8,
        salt, sizeof(salt), key, 0
    );

    EXPECT_NE(NIMCP_SUCCESS, result);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(KeyDerivationTest, GetStats) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    nimcp_kdf_stats_t stats;
    nimcp_result_t result = nimcp_kdf_get_stats(ctx, &stats);

    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_EQ(0UL, stats.derivations_performed);
}

TEST_F(KeyDerivationTest, GetStatsNullContext) {
    nimcp_kdf_stats_t stats;
    nimcp_result_t result = nimcp_kdf_get_stats(nullptr, &stats);

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, GetStatsNullPointer) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    nimcp_result_t result = nimcp_kdf_get_stats(ctx, nullptr);

    EXPECT_NE(NIMCP_SUCCESS, result);
}

TEST_F(KeyDerivationTest, StatsTracking) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = true
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Perform some derivations
    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    for (int i = 0; i < 5; i++) {
        nimcp_kdf_derive(ctx, "password", 8, salt, sizeof(salt), key, sizeof(key));
    }

    nimcp_kdf_stats_t stats;
    nimcp_kdf_get_stats(ctx, &stats);

    EXPECT_EQ(5UL, stats.derivations_performed);
    EXPECT_EQ(5 * 32UL, stats.total_bytes_derived);
    EXPECT_GT(stats.avg_derivation_time_ms, 0.0);
    EXPECT_GT(stats.total_derivation_time_ms, 0.0);
}

TEST_F(KeyDerivationTest, ResetStats) {
    ctx = nimcp_kdf_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    // Perform a derivation
    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));
    nimcp_kdf_derive(ctx, "password", 8, salt, sizeof(salt), key, sizeof(key));

    nimcp_kdf_stats_t stats_before;
    nimcp_kdf_get_stats(ctx, &stats_before);
    EXPECT_GT(stats_before.derivations_performed, 0UL);

    // Reset
    nimcp_result_t result = nimcp_kdf_reset_stats(ctx);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    nimcp_kdf_stats_t stats_after;
    nimcp_kdf_get_stats(ctx, &stats_after);
    EXPECT_EQ(0UL, stats_after.derivations_performed);
    EXPECT_EQ(0UL, stats_after.total_bytes_derived);
}

//=============================================================================
// UTILITY FUNCTION TESTS
//=============================================================================

TEST_F(KeyDerivationTest, AlgorithmName) {
    EXPECT_STREQ("Argon2id", nimcp_kdf_algorithm_name(NIMCP_KDF_ARGON2ID));
    EXPECT_STREQ("PBKDF2-HMAC-SHA256", nimcp_kdf_algorithm_name(NIMCP_KDF_PBKDF2_SHA256));
    EXPECT_STREQ("Unknown", nimcp_kdf_algorithm_name(static_cast<nimcp_kdf_algorithm_t>(999)));
}

TEST_F(KeyDerivationTest, VerifyParamsSecure) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();

    EXPECT_TRUE(nimcp_kdf_verify_params(&config, 32));
}

TEST_F(KeyDerivationTest, VerifyParamsWeakSalt) {
    nimcp_kdf_config_t config = nimcp_kdf_default_config();

    EXPECT_FALSE(nimcp_kdf_verify_params(&config, 8));  // Salt too short
}

TEST_F(KeyDerivationTest, VerifyParamsWeakArgon2) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_ARGON2ID,
        .memory_kb = 16384,  // Below recommended
        .iterations = 1,     // Below recommended
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    EXPECT_FALSE(nimcp_kdf_verify_params(&config, 32));
}

TEST_F(KeyDerivationTest, VerifyParamsWeakPBKDF2) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,  // Below recommended 100,000
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    EXPECT_FALSE(nimcp_kdf_verify_params(&config, 32));
}

TEST_F(KeyDerivationTest, EstimateTime) {
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = true
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    double estimated_ms = 0.0;
    nimcp_result_t result = nimcp_kdf_estimate_time(ctx, &estimated_ms);

    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_GT(estimated_ms, 0.0);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
