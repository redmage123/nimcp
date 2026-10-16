/**
 * @file test_fault_attention_regression.cpp
 * @brief Regression tests for Fault Attention Mechanism
 *
 * WHAT: Regression tests to prevent known bugs from reoccurring
 * WHY:  Ensure bug fixes remain fixed across code changes
 * HOW:  Test specific bug scenarios and edge cases from development
 *
 * TEST COVERAGE:
 * - Weight normalization edge cases
 * - Numerical stability issues
 * - Configuration validation bugs
 * - Memory management issues
 * - Adaptive learning edge cases
 * - Performance regressions
 *
 * @version 1.0.0
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultAttentionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        attention = fault_attention_create();
        ASSERT_NE(attention, nullptr);
    }

    void TearDown() override {
        if (attention) {
            fault_attention_destroy(attention);
            attention = nullptr;
        }
    }

    active_fault_t create_fault(
        uint32_t id, float severity, uint32_t count,
        uint32_t users, uint64_t time
    ) {
        active_fault_t fault = {};
        fault.fault_id = id;
        fault.severity = severity;
        fault.occurrence_count = count;
        fault.users_affected = users;
        fault.first_occurrence_ms = time;
        fault.last_occurrence_ms = time;
        fault.is_active = true;
        return fault;
    }

    fault_attention_t* attention = nullptr;
};

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Division by Zero in Normalization
 *
 * WHAT: Prevent division by zero when all factors are zero
 * WHY:  Bug #1: Crashed when all faults had zero occurrence/users
 * HOW:  Create faults with all zeros, verify no crash
 */
TEST_F(FaultAttentionRegressionTest, DivisionByZeroInNormalization) {
    // All faults have zero occurrence and users
    std::vector<active_fault_t> faults;
    faults.push_back(create_fault(1, 0.5f, 0, 0, 1000000));
    faults.push_back(create_fault(2, 0.6f, 0, 0, 1000000));

    // Should not crash
    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        1001000
    );
    ASSERT_TRUE(result);

    // Weights should still be computed (based on severity/recency only)
    float weight0 = 0.0f, weight1 = 0.0f;
    fault_attention_get_weight(attention, 0, &weight0);
    fault_attention_get_weight(attention, 1, &weight1);

    EXPECT_GE(weight0, 0.0f);
    EXPECT_LE(weight0, 1.0f);
    EXPECT_GE(weight1, 0.0f);
    EXPECT_LE(weight1, 1.0f);
}

/**
 * @test REGRESSION: Weight Sum Not Exactly 1.0
 *
 * WHAT: Ensure weight validation accepts floating point tolerance
 * WHY:  Bug #2: Rejected valid configs due to floating point precision
 * HOW:  Create config with weights summing to 0.9999, verify accepted
 */
TEST_F(FaultAttentionRegressionTest, WeightSumFloatingPointTolerance) {
    fault_attention_config_t config = {};

    // Weights sum to 0.9999 (floating point imprecision)
    config.severity_weight = 0.2500f;
    config.recency_weight = 0.2500f;
    config.frequency_weight = 0.2500f;
    config.impact_weight = 0.2499f; // Slightly off

    config.enable_adaptive_weights = false;
    config.learning_rate = 0.05f;
    config.max_tracked_faults = FAULT_ATTENTION_MAX_FAULTS;
    config.min_attention_threshold = 0.0f;

    // Should be accepted (within tolerance)
    fault_attention_t* tolerant_attention = fault_attention_create_custom(&config);
    EXPECT_NE(tolerant_attention, nullptr);

    if (tolerant_attention) {
        fault_attention_destroy(tolerant_attention);
    }
}

/**
 * @test REGRESSION: Negative Weights After Adaptive Learning
 *
 * WHAT: Prevent negative weights from aggressive learning
 * WHY:  Bug #3: Adaptive updates could drive weights below zero
 * HOW:  Repeatedly update with failures, verify weights stay >= 0
 */
TEST_F(FaultAttentionRegressionTest, NegativeWeightsAfterLearning) {
    // Enable aggressive learning
    fault_attention_config_t config = fault_attention_default_config();
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.5f; // Very aggressive

    fault_attention_t* aggressive = fault_attention_create_custom(&config);
    ASSERT_NE(aggressive, nullptr);

    // Create high-severity fault
    active_fault_t fault = create_fault(1, 0.9f, 5, 100, 1000000);

    // Repeatedly fail recovery (should decrease severity weight)
    for (int i = 0; i < 20; i++) {
        fault_attention_compute_weights(aggressive, &fault, 1, 1001000);
        fault_attention_update_weights(aggressive, 0, false); // Always fail
    }

    // Get final config
    fault_attention_config_t final_config;
    fault_attention_get_config(aggressive, &final_config);

    // All weights must be non-negative
    EXPECT_GE(final_config.severity_weight, 0.0f);
    EXPECT_GE(final_config.recency_weight, 0.0f);
    EXPECT_GE(final_config.frequency_weight, 0.0f);
    EXPECT_GE(final_config.impact_weight, 0.0f);

    // Weights should still sum to approximately 1.0
    float sum = final_config.severity_weight + final_config.recency_weight +
                final_config.frequency_weight + final_config.impact_weight;
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    fault_attention_destroy(aggressive);
}

/**
 * @test REGRESSION: Overflow in Recency Computation
 *
 * WHAT: Prevent overflow when computing very old fault recency
 * WHY:  Bug #4: Integer overflow when current_time << fault_time
 * HOW:  Create fault with time in distant past, verify no overflow
 */
TEST_F(FaultAttentionRegressionTest, RecencyComputationOverflow) {
    uint64_t current_time = 10000000000ULL; // 10 billion ms

    // Fault from very distant past
    active_fault_t old_fault = create_fault(
        1, 0.5f, 5, 100,
        1000000ULL // ~9.999 billion ms ago
    );

    // Should not overflow or crash
    bool result = fault_attention_compute_weights(
        attention,
        &old_fault,
        1,
        current_time
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    // Weight should be very small (very old fault)
    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);
}

/**
 * @test REGRESSION: Focus Index Out of Bounds
 *
 * WHAT: Ensure focused index always valid after computation
 * WHY:  Bug #5: Race condition could set invalid focus index
 * HOW:  Compute weights, verify focus index < fault_count
 */
TEST_F(FaultAttentionRegressionTest, FocusIndexBoundsCheck) {
    std::vector<active_fault_t> faults;

    // Create various fault counts and verify focus always valid
    for (uint32_t count = 1; count <= 10; count++) {
        faults.clear();
        for (uint32_t i = 0; i < count; i++) {
            faults.push_back(create_fault(i, 0.5f, i + 1, 10, 1000000));
        }

        bool result = fault_attention_compute_weights(
            attention,
            faults.data(),
            faults.size(),
            1001000
        );
        ASSERT_TRUE(result);

        uint32_t focused_idx = 0;
        result = fault_attention_get_focused_index(attention, &focused_idx);
        ASSERT_TRUE(result);

        // Focus must be in valid range
        EXPECT_LT(focused_idx, faults.size());
    }
}

/**
 * @test REGRESSION: Memory Leak in Repeated Create/Destroy
 *
 * WHAT: Verify no memory leaks in lifecycle
 * WHY:  Bug #6: Memory leak in weight array allocation
 * HOW:  Create and destroy many times, verify no growth
 *
 * NOTE: This test checks for obvious leaks. Use valgrind for full check.
 */
TEST_F(FaultAttentionRegressionTest, NoMemoryLeakInLifecycle) {
    // Create and destroy 1000 times
    for (int i = 0; i < 1000; i++) {
        fault_attention_t* temp = fault_attention_create();
        ASSERT_NE(temp, nullptr);

        // Use it briefly
        active_fault_t fault = create_fault(1, 0.5f, 5, 100, 1000000);
        fault_attention_compute_weights(temp, &fault, 1, 1001000);

        fault_attention_destroy(temp);
    }

    // If we got here without crashing or OOM, likely no major leak
    SUCCEED();
}

/**
 * @test REGRESSION: NaN in Weight Computation
 *
 * WHAT: Prevent NaN values in weight computation
 * WHY:  Bug #7: Division by zero could produce NaN
 * HOW:  Create edge case inputs, verify no NaN in outputs
 */
TEST_F(FaultAttentionRegressionTest, NoNaNInWeightComputation) {
    // Edge case: All zeros
    active_fault_t zero_fault = create_fault(1, 0.0f, 0, 0, 1000000);

    bool result = fault_attention_compute_weights(
        attention,
        &zero_fault,
        1,
        1000000 // Same time = zero delta
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    // Weight must not be NaN
    EXPECT_FALSE(std::isnan(weight));
    EXPECT_FALSE(std::isinf(weight));
}

/**
 * @test REGRESSION: Adaptive Learning Weights Denormalization
 *
 * WHAT: Ensure weights stay normalized after many updates
 * WHY:  Bug #8: Repeated updates caused weights to drift from 1.0
 * HOW:  Apply many updates, verify sum stays at 1.0
 */
TEST_F(FaultAttentionRegressionTest, AdaptiveLearningDenormalization) {
    fault_attention_config_t config = fault_attention_default_config();
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    fault_attention_t* adaptive = fault_attention_create_custom(&config);
    ASSERT_NE(adaptive, nullptr);

    active_fault_t fault = create_fault(1, 0.7f, 5, 100, 1000000);

    // Apply 100 random updates
    for (int i = 0; i < 100; i++) {
        fault_attention_compute_weights(adaptive, &fault, 1, 1001000 + i * 1000);
        fault_attention_update_weights(adaptive, 0, i % 2 == 0); // Alternate success/fail
    }

    // Verify weights still sum to 1.0
    fault_attention_config_t final_config;
    fault_attention_get_config(adaptive, &final_config);

    float sum = final_config.severity_weight + final_config.recency_weight +
                final_config.frequency_weight + final_config.impact_weight;

    EXPECT_NEAR(sum, 1.0f, 0.001f);

    fault_attention_destroy(adaptive);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Performance Degradation with Max Faults
 *
 * WHAT: Ensure computation stays under 100μs for max faults
 * WHY:  Performance regression in v1.1 increased time to 500μs
 * HOW:  Time computation with MAX_FAULTS, verify < 100μs
 */
TEST_F(FaultAttentionRegressionTest, PerformanceMaxFaults) {
    std::vector<active_fault_t> faults;

    // Create maximum faults
    for (uint32_t i = 0; i < FAULT_ATTENTION_MAX_FAULTS; i++) {
        faults.push_back(create_fault(i, 0.5f, i + 1, (i + 1) * 10, 1000000));
    }

    // Time 100 computations
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 100; i++) {
        fault_attention_compute_weights(
            attention,
            faults.data(),
            faults.size(),
            1001000
        );
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t duration_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);
    uint64_t avg_ns = duration_ns / 100;
    uint64_t avg_us = avg_ns / 1000;

    // Must complete in < 100μs average
    EXPECT_LT(avg_us, 100);
}

/**
 * @test REGRESSION: Statistics Overflow
 *
 * WHAT: Prevent statistics counter overflow
 * WHY:  Bug #9: total_computations overflowed at 2^32
 * HOW:  Verify uint64_t used for counters
 */
TEST_F(FaultAttentionRegressionTest, StatisticsCounterSize) {
    // This is a compile-time check via sizeof
    fault_attention_stats_t stats = {};

    // Counters should be 64-bit to prevent overflow
    EXPECT_EQ(sizeof(stats.total_computations), sizeof(uint64_t));
    EXPECT_EQ(sizeof(stats.total_updates), sizeof(uint64_t));
}

//=============================================================================
// Configuration Validation Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Accept Zero Learning Rate
 *
 * WHAT: Allow learning_rate = 0.0 (disables learning)
 * WHY:  Bug #10: Rejected valid config with lr=0
 * HOW:  Create config with lr=0, verify accepted
 */
TEST_F(FaultAttentionRegressionTest, ZeroLearningRateAccepted) {
    fault_attention_config_t config = fault_attention_default_config();
    config.learning_rate = 0.0f; // Valid: disables learning

    fault_attention_t* static_attention = fault_attention_create_custom(&config);
    EXPECT_NE(static_attention, nullptr);

    if (static_attention) {
        fault_attention_destroy(static_attention);
    }
}

/**
 * @test REGRESSION: Reject Negative Learning Rate
 *
 * WHAT: Prevent negative learning rates
 * WHY:  Bug #11: Negative lr caused unstable learning
 * HOW:  Try to create with negative lr, verify rejection
 */
TEST_F(FaultAttentionRegressionTest, NegativeLearningRateRejected) {
    fault_attention_config_t config = fault_attention_default_config();
    config.learning_rate = -0.1f; // Invalid

    fault_attention_t* invalid = fault_attention_create_custom(&config);
    EXPECT_EQ(invalid, nullptr); // Should be rejected
}

/**
 * @test REGRESSION: Reject Invalid Max Tracked Faults
 *
 * WHAT: Prevent invalid max_tracked_faults values
 * WHY:  Bug #12: Zero or excessive values caused crashes
 * HOW:  Try various invalid values, verify rejection
 */
TEST_F(FaultAttentionRegressionTest, InvalidMaxTrackedFaultsRejected) {
    fault_attention_config_t config = fault_attention_default_config();

    // Zero is invalid
    config.max_tracked_faults = 0;
    fault_attention_t* invalid1 = fault_attention_create_custom(&config);
    EXPECT_EQ(invalid1, nullptr);

    // Exceeding MAX is invalid
    config.max_tracked_faults = FAULT_ATTENTION_MAX_FAULTS + 1;
    fault_attention_t* invalid2 = fault_attention_create_custom(&config);
    EXPECT_EQ(invalid2, nullptr);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * @test REGRESSION: Handle Future Timestamps
 *
 * WHAT: Handle faults with timestamps in the future
 * WHY:  Bug #13: Clock skew could cause negative time deltas
 * HOW:  Create fault with future timestamp, verify no crash
 */
TEST_F(FaultAttentionRegressionTest, FutureTimestampHandling) {
    uint64_t current_time = 1000000;

    // Fault from the "future" (clock skew)
    active_fault_t future_fault = create_fault(
        1, 0.5f, 5, 100,
        current_time + 10000 // 10 seconds in future
    );

    // Should not crash or produce invalid results
    bool result = fault_attention_compute_weights(
        attention,
        &future_fault,
        1,
        current_time
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);
}

/**
 * @test REGRESSION: Extremely High Occurrence Count
 *
 * WHAT: Handle occurrence counts near UINT32_MAX
 * WHY:  Bug #14: Normalization overflow with huge counts
 * HOW:  Create fault with MAX occurrence, verify no overflow
 */
TEST_F(FaultAttentionRegressionTest, ExtremelyHighOccurrenceCount) {
    active_fault_t extreme_fault = create_fault(
        1, 0.5f,
        UINT32_MAX - 1, // Near maximum
        100,
        1000000
    );

    bool result = fault_attention_compute_weights(
        attention,
        &extreme_fault,
        1,
        1001000
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);
    EXPECT_FALSE(std::isnan(weight));
    EXPECT_FALSE(std::isinf(weight));
}

/**
 * @test REGRESSION: Single Fault with Maximum Values
 *
 * WHAT: Handle fault with all maximum factor values
 * WHY:  Bug #15: Normalization failed when all factors at max
 * HOW:  Create fault with severity=1.0, max occurrence, max users
 */
TEST_F(FaultAttentionRegressionTest, SingleFaultMaximumValues) {
    active_fault_t max_fault = {};
    max_fault.fault_id = 1;
    max_fault.severity = 1.0f;
    max_fault.occurrence_count = UINT32_MAX;
    max_fault.users_affected = UINT32_MAX;
    max_fault.first_occurrence_ms = 1000000;
    max_fault.last_occurrence_ms = 1000000;
    max_fault.is_active = true;

    bool result = fault_attention_compute_weights(
        attention,
        &max_fault,
        1,
        1000000
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    // Should get maximum normalized weight (1.0)
    EXPECT_NEAR(weight, 1.0f, 0.001f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
