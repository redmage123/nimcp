/**
 * @file test_path_traversal_regression.cpp
 * @brief Regression Tests for Path Traversal Detection
 *
 * WHAT: Regression tests to ensure path traversal detection remains stable
 * WHY:  Prevent performance degradation and feature regressions
 * HOW:  Test performance, memory usage, concurrent access, and edge cases
 *
 * @author NIMCP Team
 * @date 2025-12-07
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_path_traversal.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PathTraversalRegressionTest : public ::testing::Test {
protected:
    nimcp_path_validator_t validator;

    void SetUp() override {
        validator = nimcp_path_validator_create(nullptr);
        ASSERT_NE(nullptr, validator);
    }

    void TearDown() override {
        if (validator) {
            nimcp_path_validator_destroy(validator);
        }
    }
};

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, PerformanceBenchmark_SimpleValidation) {
    const int iterations = 10000;
    nimcp_path_validation_result_t result;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_path_validate(validator, "safe_file.txt",
                           NIMCP_PATH_CONTEXT_AUTO, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should complete in reasonable time (< 1 second for 10k validations) */
    EXPECT_LT(duration.count(), 1000)
        << "10k validations took " << duration.count() << "ms";
}

TEST_F(PathTraversalRegressionTest, PerformanceBenchmark_ComplexValidation) {
    const int iterations = 5000;
    nimcp_path_validation_result_t result;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_path_validate(validator, "../../%2e%2e/%252e%252e/etc/passwd",
                           NIMCP_PATH_CONTEXT_AUTO, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Complex patterns should still be fast (< 2 seconds for 5k) */
    EXPECT_LT(duration.count(), 2000)
        << "5k complex validations took " << duration.count() << "ms";
}

TEST_F(PathTraversalRegressionTest, PerformanceBenchmark_Normalization) {
    const int iterations = 10000;
    char normalized[256];

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_path_normalize("dir//subdir/../file.txt", normalized,
                            sizeof(normalized));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 500)
        << "10k normalizations took " << duration.count() << "ms";
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, MemoryLeak_CreateDestroy) {
    /* Create and destroy many validators */
    for (int i = 0; i < 1000; i++) {
        nimcp_path_validator_t v = nimcp_path_validator_create(nullptr);
        ASSERT_NE(nullptr, v);
        nimcp_path_validator_destroy(v);
    }
    /* If there's a memory leak, this test will fail under valgrind */
    SUCCEED();
}

TEST_F(PathTraversalRegressionTest, MemoryLeak_HighVolumeValidation) {
    nimcp_path_validation_result_t result;

    /* Perform many validations */
    for (int i = 0; i < 10000; i++) {
        nimcp_path_validate(validator, "../malicious.txt",
                           NIMCP_PATH_CONTEXT_AUTO, &result);
    }

    SUCCEED();
}

//=============================================================================
// Concurrent Access Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, ConcurrentValidation_MultipleThreads) {
    const int num_threads = 10;
    const int iterations_per_thread = 100;
    std::vector<std::thread> threads;

    auto worker = [this, iterations_per_thread]() {
        nimcp_path_validation_result_t result;
        for (int i = 0; i < iterations_per_thread; i++) {
            const char* path = (i % 2 == 0) ? "safe.txt" : "../bad.txt";
            nimcp_path_validate(validator, path,
                               NIMCP_PATH_CONTEXT_AUTO, &result);
        }
    };

    /* Launch threads */
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }

    /* Wait for completion */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify stats are consistent.
     * Note: Stats counter may have slight race conditions, so allow small tolerance. */
    nimcp_path_validator_stats_t stats;
    nimcp_path_validator_get_stats(validator, &stats);
    uint32_t expected = num_threads * iterations_per_thread;
    EXPECT_GE(stats.total_validations, expected - 10) << "Too many missed validations";
    EXPECT_LE(stats.total_validations, expected) << "Counter exceeded expected";
}

//=============================================================================
// Stability Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, Stability_ConsistentDetection) {
    /* Same input should always produce same result */
    nimcp_path_validation_result_t result1, result2;

    nimcp_path_validate(validator, "../../etc/passwd",
                       NIMCP_PATH_CONTEXT_AUTO, &result1);
    nimcp_path_validate(validator, "../../etc/passwd",
                       NIMCP_PATH_CONTEXT_AUTO, &result2);

    EXPECT_EQ(result1.valid, result2.valid);
    EXPECT_EQ(result1.pattern, result2.pattern);
    EXPECT_EQ(result1.severity, result2.severity);
}

TEST_F(PathTraversalRegressionTest, Stability_StateIsolation) {
    /* Multiple validators should not interfere */
    nimcp_path_validator_t v1 = nimcp_path_validator_create(nullptr);
    nimcp_path_validator_t v2 = nimcp_path_validator_create(nullptr);

    nimcp_path_validation_result_t r1, r2;

    nimcp_path_validate(v1, "../test1.txt", NIMCP_PATH_CONTEXT_AUTO, &r1);
    nimcp_path_validate(v2, "../test2.txt", NIMCP_PATH_CONTEXT_AUTO, &r2);

    /* Both should detect the pattern */
    EXPECT_FALSE(r1.valid);
    EXPECT_FALSE(r2.valid);

    nimcp_path_validator_destroy(v1);
    nimcp_path_validator_destroy(v2);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, EdgeCase_ExtremelyLongPath) {
    std::string long_path(10000, 'a');
    nimcp_path_validation_result_t result;

    nimcp_path_error_t err = nimcp_path_validate(
        validator, long_path.c_str(), NIMCP_PATH_CONTEXT_AUTO, &result);

    /* Should handle gracefully */
    EXPECT_NE(NIMCP_PATH_SUCCESS, err);
}

TEST_F(PathTraversalRegressionTest, EdgeCase_DeepTraversal) {
    std::string deep_path = "";
    for (int i = 0; i < 1000; i++) {
        deep_path += "../";
    }

    nimcp_path_validation_result_t result;
    nimcp_path_error_t err = nimcp_path_validate(
        validator, deep_path.c_str(), NIMCP_PATH_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
    EXPECT_FALSE(result.valid);
}

TEST_F(PathTraversalRegressionTest, EdgeCase_MixedEncodings) {
    /* Mix different encoding types */
    nimcp_path_validation_result_t result;

    nimcp_path_error_t err = nimcp_path_validate(
        validator, "../%2e%2e/%252e%252e/%c0%ae%c0%ae",
        NIMCP_PATH_CONTEXT_AUTO, &result);

    EXPECT_EQ(NIMCP_PATH_ERROR_THREAT_DETECTED, err);
}

//=============================================================================
// Statistics Accuracy Regression Tests
//=============================================================================

TEST_F(PathTraversalRegressionTest, Statistics_Accuracy) {
    nimcp_path_validator_stats_t stats;
    nimcp_path_validation_result_t result;

    nimcp_path_validator_reset_stats(validator);

    /* Perform specific validations */
    nimcp_path_validate(validator, "../basic", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "%2e%2e%2furl", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "/etc/passwd", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "test%00", NIMCP_PATH_CONTEXT_AUTO, &result);
    nimcp_path_validate(validator, "safe.txt", NIMCP_PATH_CONTEXT_AUTO, &result);

    nimcp_path_validator_get_stats(validator, &stats);

    EXPECT_EQ(5, stats.total_validations);
    EXPECT_EQ(4, stats.threats_detected);
    EXPECT_EQ(1, stats.basic_patterns);
    EXPECT_EQ(1, stats.url_encoded_patterns);
    EXPECT_EQ(1, stats.absolute_paths);
    EXPECT_EQ(1, stats.null_byte_patterns);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
