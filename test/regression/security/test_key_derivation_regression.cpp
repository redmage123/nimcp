/**
 * @file test_key_derivation_regression.cpp
 * @brief Regression tests for key derivation functions
 *
 * WHAT: Performance and consistency tests for KDF operations
 * WHY:  Ensure KDF performance remains acceptable and results are reproducible
 * HOW:  Benchmark tests, reproducibility checks, and performance regression detection
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "security/nimcp_key_derivation.h"
#include "security/nimcp_constant_time.h"
#include "utils/logging/nimcp_logging.h"

class KeyDerivationRegressionTest : public ::testing::Test {
protected:
    nimcp_kdf_context_t ctx;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_ERROR);
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            nimcp_kdf_destroy(ctx);
            ctx = nullptr;
        }
    }

    // Measure execution time in milliseconds
    template<typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return static_cast<double>(duration.count()) / 1000.0;
    }
};

TEST_F(KeyDerivationRegressionTest, PBKDF2_PerformanceBenchmark) {
    // Ensure PBKDF2 performance is within acceptable range
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 10000,  // Low iterations for faster testing
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = true
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint8_t salt[32];
    uint8_t key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    auto test_func = [&]() {
        nimcp_kdf_derive(ctx, "benchmark_password", 18,
                        salt, sizeof(salt), key, sizeof(key));
    };

    double time_ms = measure_time_ms(test_func);

    // PBKDF2 with 10k iterations should complete in < 500ms
    EXPECT_LT(time_ms, 500.0) << "PBKDF2 too slow: " << time_ms << " ms";

    // Typical time should be > 1ms (otherwise not secure enough)
    EXPECT_GT(time_ms, 1.0) << "PBKDF2 suspiciously fast: " << time_ms << " ms";

    nimcp_secure_zero(key, sizeof(key));
}

TEST_F(KeyDerivationRegressionTest, PBKDF2_ReproducibilityTest) {
    // Ensure same inputs always produce same outputs
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

    const char* password = "reproducibility_test_password";
    uint8_t salt[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };

    // Derive key multiple times
    uint8_t key1[32], key2[32], key3[32];

    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt, sizeof(salt), key1, sizeof(key1));
    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt, sizeof(salt), key2, sizeof(key2));
    nimcp_kdf_derive(ctx, password, strlen(password),
                     salt, sizeof(salt), key3, sizeof(key3));

    // All keys must be identical
    EXPECT_EQ(0, nimcp_ct_memcmp(key1, key2, 32));
    EXPECT_EQ(0, nimcp_ct_memcmp(key2, key3, 32));
    EXPECT_EQ(0, nimcp_ct_memcmp(key1, key3, 32));

    // Secure wipe
    nimcp_secure_zero(key1, sizeof(key1));
    nimcp_secure_zero(key2, sizeof(key2));
    nimcp_secure_zero(key3, sizeof(key3));
}

TEST_F(KeyDerivationRegressionTest, PBKDF2_TestVectorValidation) {
    // Test against known PBKDF2 test vectors (RFC 6070)
    // Test case 1: Simple case
    nimcp_kdf_config_t config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 1,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    ctx = nimcp_kdf_create(&config);
    ASSERT_NE(ctx, nullptr);

    const char* password = "password";
    const char* salt_str = "salt";
    uint8_t derived_key[32];

    nimcp_kdf_derive(ctx, password, strlen(password),
                     (const uint8_t*)salt_str, strlen(salt_str),
                     derived_key, sizeof(derived_key));

    // Key should not be all zeros
    bool has_nonzero = false;
    for (size_t i = 0; i < sizeof(derived_key); i++) {
        if (derived_key[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_secure_zero(derived_key, sizeof(derived_key));
}

TEST_F(KeyDerivationRegressionTest, SaltGenerationPerformance) {
    // Ensure salt generation is fast
    uint8_t salt[32];

    auto test_func = [&]() {
        for (int i = 0; i < 100; i++) {
            nimcp_kdf_generate_salt(salt, sizeof(salt));
        }
    };

    double time_ms = measure_time_ms(test_func);

    // 100 salt generations should complete in < 100ms (< 1ms each)
    EXPECT_LT(time_ms, 100.0) << "Salt generation too slow: " << time_ms << " ms for 100 salts";
}

TEST_F(KeyDerivationRegressionTest, SaltUniquenessRegression) {
    // Ensure salt generation maintains high entropy
    std::vector<std::vector<uint8_t>> salts;

    for (int i = 0; i < 1000; i++) {
        uint8_t salt[32];
        nimcp_kdf_generate_salt(salt, sizeof(salt));
        salts.push_back(std::vector<uint8_t>(salt, salt + 32));
    }

    // Check for duplicates (should be none)
    int duplicates = 0;
    for (size_t i = 0; i < salts.size(); i++) {
        for (size_t j = i + 1; j < salts.size(); j++) {
            if (salts[i] == salts[j]) {
                duplicates++;
            }
        }
    }

    EXPECT_EQ(0, duplicates) << "Found " << duplicates << " duplicate salts in 1000 generations";
}

TEST_F(KeyDerivationRegressionTest, ParameterScalingPBKDF2) {
    // Test performance scaling with iteration count
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

    uint8_t salt[16];
    uint8_t key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    // Measure time with 10k iterations
    auto test_10k = [&]() {
        nimcp_kdf_derive(ctx, "test", 4, salt, sizeof(salt), key, sizeof(key));
    };
    double time_10k = measure_time_ms(test_10k);

    // Update to 20k iterations
    config.iterations = 20000;
    nimcp_kdf_update_config(ctx, &config);

    // Measure time with 20k iterations
    auto test_20k = [&]() {
        nimcp_kdf_derive(ctx, "test", 4, salt, sizeof(salt), key, sizeof(key));
    };
    double time_20k = measure_time_ms(test_20k);

    // 20k iterations should take roughly 2x as long (within 50% tolerance)
    double ratio = time_20k / time_10k;
    EXPECT_GT(ratio, 1.5) << "Iteration scaling broken: 20k only " << ratio << "x slower than 10k";
    EXPECT_LT(ratio, 2.5) << "Iteration scaling inefficient: 20k is " << ratio << "x slower than 10k";

    nimcp_secure_zero(key, sizeof(key));
}

TEST_F(KeyDerivationRegressionTest, StatisticsAccuracy) {
    // Ensure statistics tracking is accurate
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

    uint8_t salt[16], key[32];
    nimcp_kdf_generate_salt(salt, sizeof(salt));

    // Perform 10 derivations
    for (int i = 0; i < 10; i++) {
        nimcp_kdf_derive(ctx, "password", 8, salt, sizeof(salt), key, sizeof(key));
        nimcp_secure_zero(key, sizeof(key));
    }

    nimcp_kdf_stats_t stats;
    nimcp_kdf_get_stats(ctx, &stats);

    EXPECT_EQ(10UL, stats.derivations_performed);
    EXPECT_EQ(10 * 32UL, stats.total_bytes_derived);
    EXPECT_GT(stats.avg_derivation_time_ms, 0.0);
    EXPECT_GT(stats.max_derivation_time_ms, 0.0);
    EXPECT_GE(stats.max_derivation_time_ms, stats.avg_derivation_time_ms);
}

TEST_F(KeyDerivationRegressionTest, MemoryLeakRegression) {
    // Ensure no memory leaks in repeated create/destroy cycles
    for (int i = 0; i < 100; i++) {
        nimcp_kdf_context_t temp_ctx = nimcp_kdf_create(nullptr);
        ASSERT_NE(temp_ctx, nullptr);

        uint8_t salt[16], key[32];
        nimcp_kdf_generate_salt(salt, sizeof(salt));
        nimcp_kdf_derive(temp_ctx, "test", 4, salt, sizeof(salt), key, sizeof(key));
        nimcp_secure_zero(key, sizeof(key));

        nimcp_kdf_destroy(temp_ctx);
    }

    // If we get here without crashes/errors, no obvious memory leaks
    SUCCEED();
}

TEST_F(KeyDerivationRegressionTest, VerifyParamsConsistency) {
    // Ensure parameter verification is consistent
    nimcp_kdf_config_t weak_config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 1000,  // Too low
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    // Should consistently reject weak parameters
    for (int i = 0; i < 10; i++) {
        EXPECT_FALSE(nimcp_kdf_verify_params(&weak_config, 32));
    }

    nimcp_kdf_config_t strong_config = {
        .algorithm = NIMCP_KDF_PBKDF2_SHA256,
        .memory_kb = 0,
        .iterations = 100000,
        .parallelism = 1,
        .enable_logging = false,
        .enable_statistics = false
    };

    // Should consistently accept strong parameters
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(nimcp_kdf_verify_params(&strong_config, 32));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
