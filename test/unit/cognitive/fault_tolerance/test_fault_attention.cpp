/**
 * @file test_fault_attention.cpp
 * @brief Unit tests for Fault Attention Mechanism
 *
 * WHAT: Comprehensive unit tests for attention-based error prioritization
 * WHY:  Ensure correct attention weight computation, focus selection, and adaptive learning
 * HOW:  Test all functions in isolation with mocked dependencies
 *
 * TEST COVERAGE:
 * - Creation/destruction
 * - Attention weight computation
 * - Priority factor weighting
 * - Focus selection
 * - Adaptive weight updates
 * - Edge cases and error conditions
 * - Performance characteristics
 *
 * @version 1.0.0
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FaultAttentionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create default attention mechanism
        attention = fault_attention_create();
        ASSERT_NE(attention, nullptr);
    }

    void TearDown() override {
        if (attention) {
            fault_attention_destroy(attention);
            attention = nullptr;
        }
    }

    // Helper: Create test fault
    active_fault_t create_test_fault(
        uint32_t id,
        float severity,
        uint32_t occurrence_count,
        uint32_t users_affected,
        uint64_t timestamp_ms
    ) {
        active_fault_t fault = {};
        fault.fault_id = id;
        fault.severity = severity;
        fault.occurrence_count = occurrence_count;
        fault.users_affected = users_affected;
        fault.first_occurrence_ms = timestamp_ms;
        fault.last_occurrence_ms = timestamp_ms;
        fault.is_active = true;
        return fault;
    }

    fault_attention_t* attention = nullptr;
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * @test Attention Creation
 *
 * WHAT: Verify attention mechanism creation with default parameters
 * WHY:  Ensure proper initialization
 * HOW:  Create, verify non-null, check default weights
 */
TEST_F(FaultAttentionTest, Creation) {
    // Verify creation succeeded
    ASSERT_NE(attention, nullptr);

    // Verify default weight configuration
    fault_attention_config_t config = {};
    bool result = fault_attention_get_config(attention, &config);
    ASSERT_TRUE(result);

    // Default weights should sum to 1.0
    float total = config.severity_weight + config.recency_weight +
                  config.frequency_weight + config.impact_weight;
    EXPECT_NEAR(total, 1.0f, 0.001f);

    // Individual weights should be reasonable
    EXPECT_GT(config.severity_weight, 0.0f);
    EXPECT_GT(config.recency_weight, 0.0f);
    EXPECT_GT(config.frequency_weight, 0.0f);
    EXPECT_GT(config.impact_weight, 0.0f);
}

/**
 * @test Attention Creation with Custom Config
 */
TEST_F(FaultAttentionTest, CreationWithCustomConfig) {
    fault_attention_config_t config = {};
    config.severity_weight = 0.5f;
    config.recency_weight = 0.2f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.05f;

    fault_attention_t* custom_attention = fault_attention_create_custom(&config);
    ASSERT_NE(custom_attention, nullptr);

    fault_attention_config_t retrieved = {};
    bool result = fault_attention_get_config(custom_attention, &retrieved);
    ASSERT_TRUE(result);

    EXPECT_FLOAT_EQ(retrieved.severity_weight, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.recency_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved.frequency_weight, 0.2f);
    EXPECT_FLOAT_EQ(retrieved.impact_weight, 0.1f);
    EXPECT_TRUE(retrieved.enable_adaptive_weights);
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.05f);

    fault_attention_destroy(custom_attention);
}

/**
 * @test Null Parameter Handling
 */
TEST_F(FaultAttentionTest, NullParameterHandling) {
    // Create with NULL config should use defaults
    fault_attention_t* default_attn = fault_attention_create_custom(nullptr);
    EXPECT_NE(default_attn, nullptr);
    fault_attention_destroy(default_attn);

    // Destroy with NULL should not crash
    fault_attention_destroy(nullptr);

    // Get config with NULL should fail
    fault_attention_config_t config = {};
    EXPECT_FALSE(fault_attention_get_config(nullptr, &config));
    EXPECT_FALSE(fault_attention_get_config(attention, nullptr));
}

//=============================================================================
// Attention Weight Computation Tests
//=============================================================================

/**
 * @test Basic Attention Weight Computation
 *
 * WHAT: Verify attention weights computed correctly for single fault
 * WHY:  Core functionality validation
 * HOW:  Create fault, compute weights, verify formula
 */
TEST_F(FaultAttentionTest, BasicWeightComputation) {
    // Create single fault
    active_fault_t fault = create_test_fault(
        1,          // id
        0.8f,       // severity (high)
        5,          // occurrence_count
        100,        // users_affected
        1000000     // timestamp
    );

    uint64_t current_time = 1001000; // 1 second later

    // Compute weights
    bool result = fault_attention_compute_weights(
        attention,
        &fault,
        1,
        current_time
    );
    ASSERT_TRUE(result);

    // Get computed weight
    float weight = 0.0f;
    result = fault_attention_get_weight(attention, 0, &weight);
    ASSERT_TRUE(result);

    // Weight should be non-zero and in valid range
    EXPECT_GT(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);
}

/**
 * @test Multiple Fault Prioritization
 */
TEST_F(FaultAttentionTest, MultipleFaultPrioritization) {
    // Create multiple faults with different characteristics
    std::vector<active_fault_t> faults;

    // Fault 1: High severity, recent
    faults.push_back(create_test_fault(1, 0.9f, 1, 50, 1000000));

    // Fault 2: Medium severity, older, frequent
    faults.push_back(create_test_fault(2, 0.5f, 10, 30, 900000));

    // Fault 3: Low severity, high impact
    faults.push_back(create_test_fault(3, 0.3f, 2, 500, 950000));

    uint64_t current_time = 1001000;

    // Compute weights
    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        current_time
    );
    ASSERT_TRUE(result);

    // Get all weights
    std::vector<float> weights(3);
    for (size_t i = 0; i < 3; i++) {
        result = fault_attention_get_weight(attention, i, &weights[i]);
        ASSERT_TRUE(result);
        EXPECT_GT(weights[i], 0.0f);
        EXPECT_LE(weights[i], 1.0f);
    }

    // Fault 1 (high severity, recent) should have highest weight
    EXPECT_GT(weights[0], weights[1]);
    EXPECT_GT(weights[0], weights[2]);
}

/**
 * @test Severity Factor Dominance
 */
TEST_F(FaultAttentionTest, SeverityFactorDominance) {
    // Configure with severity-dominant weights
    fault_attention_config_t config = {};
    config.severity_weight = 0.8f;
    config.recency_weight = 0.1f;
    config.frequency_weight = 0.05f;
    config.impact_weight = 0.05f;

    fault_attention_t* severity_attention = fault_attention_create_custom(&config);
    ASSERT_NE(severity_attention, nullptr);

    // Create faults: one high severity, one low severity
    active_fault_t high_severity = create_test_fault(1, 0.9f, 1, 10, 1000000);
    active_fault_t low_severity = create_test_fault(2, 0.1f, 100, 1000, 1000000);

    active_fault_t faults[] = {high_severity, low_severity};

    bool result = fault_attention_compute_weights(
        severity_attention,
        faults,
        2,
        1001000
    );
    ASSERT_TRUE(result);

    float weight_high = 0.0f, weight_low = 0.0f;
    fault_attention_get_weight(severity_attention, 0, &weight_high);
    fault_attention_get_weight(severity_attention, 1, &weight_low);

    // High severity should dominate despite low frequency/impact
    EXPECT_GT(weight_high, weight_low);

    fault_attention_destroy(severity_attention);
}

/**
 * @test Recency Factor Impact
 */
TEST_F(FaultAttentionTest, RecencyFactorImpact) {
    uint64_t current_time = 1000000;

    // Recent fault vs old fault (same severity)
    active_fault_t recent = create_test_fault(1, 0.5f, 1, 100, 999000);  // 1s ago
    active_fault_t old = create_test_fault(2, 0.5f, 1, 100, 900000);     // 100s ago

    active_fault_t faults[] = {recent, old};

    bool result = fault_attention_compute_weights(
        attention,
        faults,
        2,
        current_time
    );
    ASSERT_TRUE(result);

    float weight_recent = 0.0f, weight_old = 0.0f;
    fault_attention_get_weight(attention, 0, &weight_recent);
    fault_attention_get_weight(attention, 1, &weight_old);

    // Recent fault should have higher weight
    EXPECT_GT(weight_recent, weight_old);
}

/**
 * @test Frequency Factor Impact
 */
TEST_F(FaultAttentionTest, FrequencyFactorImpact) {
    // Frequent fault vs rare fault
    active_fault_t frequent = create_test_fault(1, 0.5f, 100, 50, 1000000);
    active_fault_t rare = create_test_fault(2, 0.5f, 1, 50, 1000000);

    active_fault_t faults[] = {frequent, rare};

    bool result = fault_attention_compute_weights(
        attention,
        faults,
        2,
        1001000
    );
    ASSERT_TRUE(result);

    float weight_frequent = 0.0f, weight_rare = 0.0f;
    fault_attention_get_weight(attention, 0, &weight_frequent);
    fault_attention_get_weight(attention, 1, &weight_rare);

    // Frequent fault should have higher weight
    EXPECT_GT(weight_frequent, weight_rare);
}

/**
 * @test Impact Factor (Users Affected)
 */
TEST_F(FaultAttentionTest, ImpactFactorUsersAffected) {
    // High impact vs low impact
    active_fault_t high_impact = create_test_fault(1, 0.5f, 5, 1000, 1000000);
    active_fault_t low_impact = create_test_fault(2, 0.5f, 5, 10, 1000000);

    active_fault_t faults[] = {high_impact, low_impact};

    bool result = fault_attention_compute_weights(
        attention,
        faults,
        2,
        1001000
    );
    ASSERT_TRUE(result);

    float weight_high = 0.0f, weight_low = 0.0f;
    fault_attention_get_weight(attention, 0, &weight_high);
    fault_attention_get_weight(attention, 1, &weight_low);

    // High impact should have higher weight
    EXPECT_GT(weight_high, weight_low);
}

//=============================================================================
// Focus Selection Tests
//=============================================================================

/**
 * @test Get Focused Fault
 *
 * WHAT: Verify focus selection returns highest priority fault
 * WHY:  Critical for resource allocation
 * HOW:  Compute weights, get focus, verify it's the max
 */
TEST_F(FaultAttentionTest, GetFocusedFault) {
    // Create faults with different priorities
    active_fault_t faults[3];
    faults[0] = create_test_fault(1, 0.3f, 2, 50, 1000000);   // Low priority
    faults[1] = create_test_fault(2, 0.9f, 5, 200, 1000000);  // High priority
    faults[2] = create_test_fault(3, 0.5f, 3, 100, 1000000);  // Medium priority

    bool result = fault_attention_compute_weights(
        attention,
        faults,
        3,
        1001000
    );
    ASSERT_TRUE(result);

    // Get focused fault
    uint32_t focused_index = 0;
    result = fault_attention_get_focused_index(attention, &focused_index);
    ASSERT_TRUE(result);

    // Should be fault 1 (index 1, highest priority)
    EXPECT_EQ(focused_index, 1);

    // Verify weight is indeed highest
    float weights[3];
    for (int i = 0; i < 3; i++) {
        fault_attention_get_weight(attention, i, &weights[i]);
    }
    EXPECT_GE(weights[focused_index], weights[0]);
    EXPECT_GE(weights[focused_index], weights[1]);
    EXPECT_GE(weights[focused_index], weights[2]);
}

/**
 * @test Focus on Single Fault
 */
TEST_F(FaultAttentionTest, FocusOnSingleFault) {
    active_fault_t fault = create_test_fault(1, 0.7f, 3, 100, 1000000);

    bool result = fault_attention_compute_weights(
        attention,
        &fault,
        1,
        1001000
    );
    ASSERT_TRUE(result);

    uint32_t focused_index = 0;
    result = fault_attention_get_focused_index(attention, &focused_index);
    ASSERT_TRUE(result);

    // Should focus on the only fault
    EXPECT_EQ(focused_index, 0);
}

/**
 * @test No Faults Focus
 */
TEST_F(FaultAttentionTest, NoFaultsFocus) {
    // Compute with zero faults
    bool result = fault_attention_compute_weights(
        attention,
        nullptr,
        0,
        1000000
    );
    EXPECT_TRUE(result); // Should succeed but have no focus

    uint32_t focused_index = 0;
    result = fault_attention_get_focused_index(attention, &focused_index);
    EXPECT_FALSE(result); // Should fail - no faults to focus on
}

//=============================================================================
// Adaptive Weight Update Tests
//=============================================================================

/**
 * @test Successful Recovery Weight Increase
 *
 * WHAT: Verify weights increase for factors that led to successful recovery
 * WHY:  Adaptive learning improves prioritization over time
 * HOW:  Record recovery, verify weight adjustment
 */
TEST_F(FaultAttentionTest, SuccessfulRecoveryWeightIncrease) {
    // Enable adaptive weights
    fault_attention_config_t config = {};
    config.severity_weight = 0.25f;
    config.recency_weight = 0.25f;
    config.frequency_weight = 0.25f;
    config.impact_weight = 0.25f;
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    fault_attention_t* adaptive_attention = fault_attention_create_custom(&config);
    ASSERT_NE(adaptive_attention, nullptr);

    // Get initial weights
    fault_attention_config_t initial_config = {};
    fault_attention_get_config(adaptive_attention, &initial_config);

    // Create high-severity fault and compute weights
    active_fault_t fault = create_test_fault(1, 0.9f, 5, 100, 1000000);
    fault_attention_compute_weights(adaptive_attention, &fault, 1, 1001000);

    // Update with successful recovery
    bool result = fault_attention_update_weights(
        adaptive_attention,
        0,  // fault index
        true  // recovery_success
    );
    ASSERT_TRUE(result);

    // Get updated weights
    fault_attention_config_t updated_config = {};
    fault_attention_get_config(adaptive_attention, &updated_config);

    // Severity weight should increase (high severity led to success)
    EXPECT_GT(updated_config.severity_weight, initial_config.severity_weight);

    // Weights should still sum to ~1.0
    float total = updated_config.severity_weight + updated_config.recency_weight +
                  updated_config.frequency_weight + updated_config.impact_weight;
    EXPECT_NEAR(total, 1.0f, 0.01f);

    fault_attention_destroy(adaptive_attention);
}

/**
 * @test Failed Recovery Weight Decrease
 */
TEST_F(FaultAttentionTest, FailedRecoveryWeightDecrease) {
    // Enable adaptive weights
    fault_attention_config_t config = {};
    config.severity_weight = 0.5f;
    config.recency_weight = 0.2f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    fault_attention_t* adaptive_attention = fault_attention_create_custom(&config);
    ASSERT_NE(adaptive_attention, nullptr);

    fault_attention_config_t initial_config = {};
    fault_attention_get_config(adaptive_attention, &initial_config);

    // Create high-severity fault
    active_fault_t fault = create_test_fault(1, 0.9f, 5, 100, 1000000);
    fault_attention_compute_weights(adaptive_attention, &fault, 1, 1001000);

    // Update with failed recovery
    bool result = fault_attention_update_weights(
        adaptive_attention,
        0,
        false  // recovery_success = false
    );
    ASSERT_TRUE(result);

    fault_attention_config_t updated_config = {};
    fault_attention_get_config(adaptive_attention, &updated_config);

    // Severity weight should decrease (high severity didn't guarantee success)
    EXPECT_LT(updated_config.severity_weight, initial_config.severity_weight);

    fault_attention_destroy(adaptive_attention);
}

/**
 * @test Adaptive Learning Disabled
 */
TEST_F(FaultAttentionTest, AdaptiveLearningDisabled) {
    // Disable adaptive weights
    fault_attention_config_t config = {};
    config.severity_weight = 0.4f;
    config.recency_weight = 0.3f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;
    config.enable_adaptive_weights = false;
    config.learning_rate = 0.1f;

    fault_attention_t* static_attention = fault_attention_create_custom(&config);
    ASSERT_NE(static_attention, nullptr);

    fault_attention_config_t initial_config = {};
    fault_attention_get_config(static_attention, &initial_config);

    // Create fault and attempt update
    active_fault_t fault = create_test_fault(1, 0.8f, 5, 100, 1000000);
    fault_attention_compute_weights(static_attention, &fault, 1, 1001000);

    fault_attention_update_weights(static_attention, 0, true);

    // Weights should not change
    fault_attention_config_t final_config = {};
    fault_attention_get_config(static_attention, &final_config);

    EXPECT_FLOAT_EQ(final_config.severity_weight, initial_config.severity_weight);
    EXPECT_FLOAT_EQ(final_config.recency_weight, initial_config.recency_weight);
    EXPECT_FLOAT_EQ(final_config.frequency_weight, initial_config.frequency_weight);
    EXPECT_FLOAT_EQ(final_config.impact_weight, initial_config.impact_weight);

    fault_attention_destroy(static_attention);
}

//=============================================================================
// Edge Cases and Error Handling Tests
//=============================================================================

/**
 * @test Zero Severity Handling
 */
TEST_F(FaultAttentionTest, ZeroSeverityHandling) {
    active_fault_t fault = create_test_fault(1, 0.0f, 5, 100, 1000000);

    bool result = fault_attention_compute_weights(
        attention,
        &fault,
        1,
        1001000
    );
    ASSERT_TRUE(result);

    float weight = 0.0f;
    fault_attention_get_weight(attention, 0, &weight);

    // Should still compute valid weight (based on other factors)
    EXPECT_GE(weight, 0.0f);
    EXPECT_LE(weight, 1.0f);
}

/**
 * @test Maximum Capacity Handling
 */
TEST_F(FaultAttentionTest, MaximumCapacityHandling) {
    // Create maximum number of faults
    std::vector<active_fault_t> faults;
    for (uint32_t i = 0; i < FAULT_ATTENTION_MAX_FAULTS; i++) {
        faults.push_back(create_test_fault(i, 0.5f, 1, 10, 1000000));
    }

    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        1001000
    );
    ASSERT_TRUE(result);

    // Verify we can get focus even with max faults
    uint32_t focused_index = 0;
    result = fault_attention_get_focused_index(attention, &focused_index);
    ASSERT_TRUE(result);
    EXPECT_LT(focused_index, FAULT_ATTENTION_MAX_FAULTS);
}

/**
 * @test Exceeding Maximum Capacity
 */
TEST_F(FaultAttentionTest, ExceedingMaximumCapacity) {
    // Try to compute weights for too many faults
    std::vector<active_fault_t> faults(FAULT_ATTENTION_MAX_FAULTS + 10);
    for (size_t i = 0; i < faults.size(); i++) {
        faults[i] = create_test_fault(i, 0.5f, 1, 10, 1000000);
    }

    bool result = fault_attention_compute_weights(
        attention,
        faults.data(),
        faults.size(),
        1001000
    );
    EXPECT_FALSE(result); // Should fail gracefully
}

/**
 * @test Invalid Weight Configuration
 */
TEST_F(FaultAttentionTest, InvalidWeightConfiguration) {
    fault_attention_config_t config = {};

    // Negative weights
    config.severity_weight = -0.1f;
    config.recency_weight = 0.5f;
    config.frequency_weight = 0.3f;
    config.impact_weight = 0.3f;

    fault_attention_t* invalid_attention = fault_attention_create_custom(&config);
    EXPECT_EQ(invalid_attention, nullptr); // Should fail

    // Weights don't sum to 1.0
    config.severity_weight = 0.5f;
    config.recency_weight = 0.5f;
    config.frequency_weight = 0.5f;
    config.impact_weight = 0.5f;

    invalid_attention = fault_attention_create_custom(&config);
    EXPECT_EQ(invalid_attention, nullptr); // Should fail
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * @test Computation Performance
 *
 * WHAT: Verify weight computation is fast enough
 * WHY:  Must not impact real-time fault handling
 * HOW:  Time computation for typical load
 */
TEST_F(FaultAttentionTest, ComputationPerformance) {
    // Create realistic fault set (10 active faults)
    std::vector<active_fault_t> faults;
    for (int i = 0; i < 10; i++) {
        faults.push_back(create_test_fault(
            i,
            0.5f + (i * 0.05f),
            i + 1,
            (i + 1) * 10,
            1000000
        ));
    }

    // Time 1000 computations
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 1000; i++) {
        fault_attention_compute_weights(
            attention,
            faults.data(),
            faults.size(),
            1001000 + i
        );
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t duration_ns = (end.tv_sec - start.tv_sec) * 1000000000ULL +
                          (end.tv_nsec - start.tv_nsec);
    uint64_t avg_ns = duration_ns / 1000;

    // Should average < 10 microseconds per computation
    EXPECT_LT(avg_ns, 10000);
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * @test Attention Statistics
 */
TEST_F(FaultAttentionTest, AttentionStatistics) {
    // Create and compute weights for faults
    active_fault_t faults[3];
    faults[0] = create_test_fault(1, 0.3f, 2, 50, 1000000);
    faults[1] = create_test_fault(2, 0.9f, 5, 200, 1000000);
    faults[2] = create_test_fault(3, 0.5f, 3, 100, 1000000);

    fault_attention_compute_weights(attention, faults, 3, 1001000);

    // Get statistics
    fault_attention_stats_t stats = {};
    bool result = fault_attention_get_stats(attention, &stats);
    ASSERT_TRUE(result);

    EXPECT_EQ(stats.total_computations, 1);
    EXPECT_EQ(stats.total_updates, 0);
    EXPECT_EQ(stats.current_fault_count, 3);

    // Update weights and check stats again
    fault_attention_update_weights(attention, 1, true);

    result = fault_attention_get_stats(attention, &stats);
    ASSERT_TRUE(result);
    EXPECT_EQ(stats.total_updates, 1);
}

/**
 * @test Reset Statistics
 */
TEST_F(FaultAttentionTest, ResetStatistics) {
    // Generate some activity
    active_fault_t fault = create_test_fault(1, 0.7f, 5, 100, 1000000);
    fault_attention_compute_weights(attention, &fault, 1, 1001000);
    fault_attention_update_weights(attention, 0, true);

    // Reset
    bool result = fault_attention_reset_stats(attention);
    ASSERT_TRUE(result);

    // Verify stats cleared
    fault_attention_stats_t stats = {};
    fault_attention_get_stats(attention, &stats);

    EXPECT_EQ(stats.total_computations, 0);
    EXPECT_EQ(stats.total_updates, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
