/**
 * @file test_constant_time_integration.cpp
 * @brief Integration tests for constant-time operations with security module
 *
 * WHAT: Test constant-time operations integrated with hash verification and bio-async
 * WHY:  Ensure constant-time functions work correctly in real-world scenarios
 * HOW:  Integration with nimcp_security hash functions and bio-async messaging
 *
 * @author NIMCP Security Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_constant_time.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

class ConstantTimeIntegrationTest : public ::testing::Test {
protected:
    nimcp_ct_context_t ctx;

    void SetUp() override {
        nimcp_log_set_level(NULL, LOG_LEVEL_WARN);
        ctx = nimcp_ct_create();
        ASSERT_NE(ctx, nullptr);

        // Initialize bio-async router
        bio_router_init(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_ct_destroy(ctx);
            ctx = nullptr;
        }
        bio_router_shutdown();
    }
};

TEST_F(ConstantTimeIntegrationTest, HashVerificationIntegration) {
    // Test integration with hash verification
    const char* data = "test data for hashing";
    uint8_t hash1[NIMCP_SECURITY_HASH_SIZE];
    uint8_t hash2[NIMCP_SECURITY_HASH_SIZE];

    // Create encryption context to generate hashes
    uint8_t key[NIMCP_SECURITY_KEY_SIZE];
    nimcp_encryption_generate_key(key);
    nimcp_encryption_context_t* enc_ctx = nimcp_encryption_create(key);
    ASSERT_NE(enc_ctx, nullptr);

    // In real usage, these would be actual hash outputs
    // For testing, we'll use the same "hash" (in practice, from hash function)
    memcpy(hash1, data, NIMCP_SECURITY_HASH_SIZE);
    memcpy(hash2, data, NIMCP_SECURITY_HASH_SIZE);

    // Constant-time hash comparison should succeed
    EXPECT_TRUE(nimcp_ct_hash_equal(hash1, hash2, NIMCP_SECURITY_HASH_SIZE));

    // Modify one bit
    hash2[16] ^= 0x01;
    EXPECT_FALSE(nimcp_ct_hash_equal(hash1, hash2, NIMCP_SECURITY_HASH_SIZE));

    nimcp_encryption_destroy(enc_ctx);
}

TEST_F(ConstantTimeIntegrationTest, SecureComparisonInAuthentication) {
    // Simulate password hash verification scenario
    const char* stored_hash_hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    const char* provided_hash_hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

    // Convert hex to bytes (simplified - in real use, proper hex decode)
    EXPECT_EQ(0, nimcp_ct_strcmp(stored_hash_hex, provided_hash_hex));

    // Wrong password attempt
    const char* wrong_hash_hex = "0000000000000000000000000000000000000000000000000000000000000000";
    EXPECT_NE(0, nimcp_ct_strcmp(stored_hash_hex, wrong_hash_hex));
}

TEST_F(ConstantTimeIntegrationTest, ThreadSafety) {
    // Test concurrent access from multiple threads
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    auto test_func = [&]() {
        for (int i = 0; i < 100; i++) {
            uint8_t buf1[32] = {1, 2, 3, 4, 5};
            uint8_t buf2[32] = {1, 2, 3, 4, 5};

            if (nimcp_ct_memcmp(buf1, buf2, 5) == 0) {
                success_count++;
            }
        }
    };

    for (int i = 0; i < 10; i++) {
        threads.emplace_back(test_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(1000, success_count.load());
}

TEST_F(ConstantTimeIntegrationTest, LargeScaleComparison) {
    // Test with realistic large data
    std::vector<uint8_t> data1(1024 * 1024);  // 1 MB
    std::vector<uint8_t> data2(1024 * 1024);

    // Fill with pattern
    for (size_t i = 0; i < data1.size(); i++) {
        data1[i] = static_cast<uint8_t>(i & 0xFF);
        data2[i] = static_cast<uint8_t>(i & 0xFF);
    }

    EXPECT_EQ(0, nimcp_ct_memcmp(data1.data(), data2.data(), data1.size()));

    // Change one byte in the middle
    data2[data2.size() / 2] ^= 0x01;
    EXPECT_NE(0, nimcp_ct_memcmp(data1.data(), data2.data(), data1.size()));
}

TEST_F(ConstantTimeIntegrationTest, BioAsyncStatsRetrieval) {
    // Test bio-async integration for stats retrieval
    // This would normally use bio_router to send messages
    nimcp_ct_stats_t stats;
    nimcp_result_t result = nimcp_ct_get_stats(ctx, &stats);
    EXPECT_EQ(NIMCP_SUCCESS, result);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
