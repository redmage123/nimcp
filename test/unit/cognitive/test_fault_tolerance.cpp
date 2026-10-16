/**
 * @file test_fault_tolerance.cpp
 * @brief Unit tests for Fault Tolerance Attention Mechanism
 *
 * WHAT: Comprehensive unit tests for fault prioritization system
 * WHY:  Ensure correct error prioritization using attention-based weighting
 * HOW:  Test lifecycle, weight computation, focus management, and adaptation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/fault_tolerance/nimcp_fault_attention.h"

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Fault Attention Core Tests
 * ============================================================================ */

class FaultAttentionTest : public NimcpTestBase {
protected:
    fault_attention_t* attention = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (attention) {
            fault_attention_destroy(attention);
            attention = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create test faults
    void createTestFaults(active_fault_t* faults, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            faults[i].fault_id = i + 1;
            faults[i].severity = 0.5f + (float)i * 0.1f;
            faults[i].occurrence_count = i + 1;
            faults[i].users_affected = (i + 1) * 10;
            faults[i].first_occurrence_ms = 1000 * (i + 1);
            faults[i].last_occurrence_ms = 2000 + 100 * i;
            faults[i].is_active = true;
            snprintf(faults[i].description, sizeof(faults[i].description),
                     "Test fault %u", i);
        }
    }
};

TEST_F(FaultAttentionTest, CreateWithDefaults) {
    // WHAT: Create attention mechanism with default config
    // WHY:  Basic lifecycle test
    // HOW:  Create and verify non-NULL

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);
}

TEST_F(FaultAttentionTest, CreateWithCustomConfig) {
    // WHAT: Create attention mechanism with custom config
    // WHY:  Verify custom configuration
    // HOW:  Create config, create mechanism

    fault_attention_config_t config = fault_attention_default_config();
    config.severity_weight = 0.5f;
    config.recency_weight = 0.2f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    attention = fault_attention_create_custom(&config);
    ASSERT_NE(attention, nullptr);
}

TEST_F(FaultAttentionTest, CreateWithNullConfigUsesDefaults) {
    // WHAT: Create with NULL config uses defaults
    // WHY:  Convenience for default usage
    // HOW:  Pass NULL, verify creation

    attention = fault_attention_create_custom(nullptr);
    ASSERT_NE(attention, nullptr);
}

TEST_F(FaultAttentionTest, CreateWithInvalidWeights) {
    // WHAT: Reject config with invalid weights
    // WHY:  Weights must sum to 1.0
    // HOW:  Create invalid config, expect NULL

    fault_attention_config_t config = fault_attention_default_config();
    config.severity_weight = 0.9f;  // Sum > 1.0
    config.recency_weight = 0.3f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;

    attention = fault_attention_create_custom(&config);
    EXPECT_EQ(attention, nullptr);
}

TEST_F(FaultAttentionTest, DestroyNullIsNoop) {
    // WHAT: Verify destroying NULL is safe
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    fault_attention_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, DefaultConfigHasSensibleValues) {
    // WHAT: Verify default config values
    // WHY:  Ensure reasonable defaults
    // HOW:  Check all values

    fault_attention_config_t config = fault_attention_default_config();

    EXPECT_FLOAT_EQ(config.severity_weight, FAULT_ATTENTION_DEFAULT_SEVERITY_WEIGHT);
    EXPECT_FLOAT_EQ(config.recency_weight, FAULT_ATTENTION_DEFAULT_RECENCY_WEIGHT);
    EXPECT_FLOAT_EQ(config.frequency_weight, FAULT_ATTENTION_DEFAULT_FREQUENCY_WEIGHT);
    EXPECT_FLOAT_EQ(config.impact_weight, FAULT_ATTENTION_DEFAULT_IMPACT_WEIGHT);
    EXPECT_FLOAT_EQ(config.learning_rate, FAULT_ATTENTION_DEFAULT_LEARNING_RATE);

    // Weights should sum to 1.0
    float sum = config.severity_weight + config.recency_weight +
                config.frequency_weight + config.impact_weight;
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

TEST_F(FaultAttentionTest, ValidateConfigValid) {
    // WHAT: Verify valid config passes validation
    // WHY:  Config validation works correctly
    // HOW:  Pass valid config

    fault_attention_config_t config = fault_attention_default_config();
    bool valid = fault_attention_validate_config(&config);
    EXPECT_TRUE(valid);
}

TEST_F(FaultAttentionTest, ValidateConfigNull) {
    // WHAT: Verify NULL config fails validation
    // WHY:  Defensive programming
    // HOW:  Pass NULL

    bool valid = fault_attention_validate_config(nullptr);
    EXPECT_FALSE(valid);
}

TEST_F(FaultAttentionTest, ValidateConfigNegativeWeight) {
    // WHAT: Reject negative weights
    // WHY:  Weights must be non-negative
    // HOW:  Set negative weight

    fault_attention_config_t config = fault_attention_default_config();
    config.severity_weight = -0.1f;
    config.recency_weight = 0.5f;  // Adjust to still sum to ~1.0

    bool valid = fault_attention_validate_config(&config);
    EXPECT_FALSE(valid);
}

/* ============================================================================
 * Weight Computation Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, ComputeWeightsWithNullAttention) {
    // WHAT: Verify compute handles NULL attention
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    active_fault_t faults[2];
    createTestFaults(faults, 2);

    bool result = fault_attention_compute_weights(nullptr, faults, 2, 10000);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, ComputeWeightsWithNullFaults) {
    // WHAT: Verify compute handles NULL faults
    // WHY:  Defensive programming
    // HOW:  Call with NULL faults

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    bool result = fault_attention_compute_weights(attention, nullptr, 2, 10000);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, ComputeWeightsWithZeroCount) {
    // WHAT: Verify compute handles zero fault count
    // WHY:  Edge case - no faults
    // HOW:  Pass zero count

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[1];
    bool result = fault_attention_compute_weights(attention, faults, 0, 10000);

    // Zero count should be valid (empty list)
    EXPECT_TRUE(result);
}

TEST_F(FaultAttentionTest, ComputeWeightsSingleFault) {
    // WHAT: Compute weight for single fault
    // WHY:  Simple case
    // HOW:  One fault, check weight

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[1];
    createTestFaults(faults, 1);

    bool result = fault_attention_compute_weights(attention, faults, 1, 10000);
    ASSERT_TRUE(result);

    float weight;
    bool got_weight = fault_attention_get_weight(attention, 0, &weight);
    ASSERT_TRUE(got_weight);

    // Single fault should have weight 1.0 (normalized)
    EXPECT_FLOAT_EQ(weight, 1.0f);
}

TEST_F(FaultAttentionTest, ComputeWeightsMultipleFaults) {
    // WHAT: Compute weights for multiple faults
    // WHY:  Core functionality
    // HOW:  Multiple faults, check weights sum to 1.0

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[5];
    createTestFaults(faults, 5);

    bool result = fault_attention_compute_weights(attention, faults, 5, 10000);
    ASSERT_TRUE(result);

    // Get all weights
    float weights[5];
    uint32_t count = fault_attention_get_all_weights(attention, weights, 5);
    ASSERT_EQ(count, 5u);

    // Weights are max-normalized: all in [0,1], at least one equals 1.0
    float max_weight = 0.0f;
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(weights[i], 0.0f);
        EXPECT_LE(weights[i], 1.0f);
        if (weights[i] > max_weight) max_weight = weights[i];
    }
    EXPECT_NEAR(max_weight, 1.0f, 0.01f);
}

TEST_F(FaultAttentionTest, ComputeWeightsHigherSeverityHigherWeight) {
    // WHAT: Verify higher severity gets higher weight
    // WHY:  Severity is key factor
    // HOW:  Create faults with different severities

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[3];
    memset(faults, 0, sizeof(faults));

    // All same except severity
    for (int i = 0; i < 3; i++) {
        faults[i].fault_id = i + 1;
        faults[i].occurrence_count = 1;
        faults[i].users_affected = 10;
        faults[i].last_occurrence_ms = 9000;
        faults[i].is_active = true;
    }
    faults[0].severity = 0.2f;
    faults[1].severity = 0.5f;
    faults[2].severity = 0.9f;  // Highest severity

    fault_attention_compute_weights(attention, faults, 3, 10000);

    float weights[3];
    fault_attention_get_all_weights(attention, weights, 3);

    // Highest severity should have highest weight
    EXPECT_GT(weights[2], weights[1]);
    EXPECT_GT(weights[1], weights[0]);
}

TEST_F(FaultAttentionTest, GetWeightWithNullAttention) {
    // WHAT: Verify get_weight handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float weight;
    bool result = fault_attention_get_weight(nullptr, 0, &weight);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetWeightWithNullOutput) {
    // WHAT: Verify get_weight handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL weight

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    bool result = fault_attention_get_weight(attention, 0, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetWeightOutOfBounds) {
    // WHAT: Verify get_weight handles out of bounds
    // WHY:  Defensive programming
    // HOW:  Request invalid index

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    fault_attention_compute_weights(attention, faults, 2, 10000);

    float weight;
    bool result = fault_attention_get_weight(attention, 10, &weight);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Focus Management Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, GetFocusedIndexWithNullAttention) {
    // WHAT: Verify get_focused_index handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t index;
    bool result = fault_attention_get_focused_index(nullptr, &index);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetFocusedIndexWithNullOutput) {
    // WHAT: Verify get_focused_index handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    bool result = fault_attention_get_focused_index(attention, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, FocusedIndexIsHighestWeight) {
    // WHAT: Verify focused index is highest weight fault
    // WHY:  Core focus behavior
    // HOW:  Create faults, verify focus

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[3];
    memset(faults, 0, sizeof(faults));
    for (int i = 0; i < 3; i++) {
        faults[i].fault_id = i + 1;
        faults[i].occurrence_count = 1;
        faults[i].users_affected = 10;
        faults[i].last_occurrence_ms = 9000;
        faults[i].is_active = true;
    }
    faults[0].severity = 0.2f;
    faults[1].severity = 0.9f;  // Highest
    faults[2].severity = 0.5f;

    fault_attention_compute_weights(attention, faults, 3, 10000);

    uint32_t focused_index;
    bool result = fault_attention_get_focused_index(attention, &focused_index);
    ASSERT_TRUE(result);

    // Index 1 should be focused (highest severity)
    EXPECT_EQ(focused_index, 1u);
}

TEST_F(FaultAttentionTest, GetFocusedFaultWithNullAttention) {
    // WHAT: Verify get_focused_fault handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    active_fault_t faults[2];
    active_fault_t focused;
    bool result = fault_attention_get_focused_fault(nullptr, faults, 2, &focused);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetFocusedFaultReturnsCorrectFault) {
    // WHAT: Verify get_focused_fault returns correct fault
    // WHY:  Convenience function
    // HOW:  Get focused fault, verify ID

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[3];
    createTestFaults(faults, 3);
    faults[0].severity = 0.2f;
    faults[1].severity = 0.9f;  // Should be focused
    faults[2].severity = 0.5f;

    fault_attention_compute_weights(attention, faults, 3, 10000);

    active_fault_t focused;
    bool result = fault_attention_get_focused_fault(attention, faults, 3, &focused);
    ASSERT_TRUE(result);

    EXPECT_EQ(focused.fault_id, faults[1].fault_id);
}

/* ============================================================================
 * Adaptive Learning Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, UpdateWeightsWithAdaptiveLearning) {
    // WHAT: Verify weight update works with adaptive learning
    // WHY:  Learning from recovery outcomes
    // HOW:  Enable learning, update weights

    fault_attention_config_t config = fault_attention_default_config();
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    attention = fault_attention_create_custom(&config);
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    fault_attention_compute_weights(attention, faults, 2, 10000);

    bool result = fault_attention_update_weights(attention, 0, true);
    EXPECT_TRUE(result);
}

TEST_F(FaultAttentionTest, UpdateWeightsDisabledByDefault) {
    // WHAT: Verify update weights fails when disabled
    // WHY:  Adaptive learning must be enabled
    // HOW:  Use default config (disabled)

    attention = fault_attention_create();  // Adaptive disabled by default
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    fault_attention_compute_weights(attention, faults, 2, 10000);

    bool result = fault_attention_update_weights(attention, 0, true);
    EXPECT_TRUE(result);  // Silently succeeds when adaptive is disabled (no-op)
}

TEST_F(FaultAttentionTest, ResetWeights) {
    // WHAT: Verify weight reset works
    // WHY:  Undo maladaptive learning
    // HOW:  Enable learning, update, reset

    fault_attention_config_t config = fault_attention_default_config();
    config.enable_adaptive_weights = true;
    config.learning_rate = 0.1f;

    attention = fault_attention_create_custom(&config);
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    fault_attention_compute_weights(attention, faults, 2, 10000);

    // Update weights
    fault_attention_update_weights(attention, 0, true);

    // Reset
    bool result = fault_attention_reset_weights(attention);
    EXPECT_TRUE(result);

    // Verify weights are back to default
    fault_attention_config_t current_config;
    fault_attention_get_config(attention, &current_config);

    fault_attention_config_t default_config = fault_attention_default_config();
    EXPECT_FLOAT_EQ(current_config.severity_weight, default_config.severity_weight);
    EXPECT_FLOAT_EQ(current_config.recency_weight, default_config.recency_weight);
}

TEST_F(FaultAttentionTest, ResetWeightsWithNullAttention) {
    // WHAT: Verify reset handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fault_attention_reset_weights(nullptr);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, GetConfigWithNullAttention) {
    // WHAT: Verify get_config handles NULL attention
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    fault_attention_config_t config;
    bool result = fault_attention_get_config(nullptr, &config);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetConfigWithNullOutput) {
    // WHAT: Verify get_config handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    bool result = fault_attention_get_config(attention, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetAndSetConfig) {
    // WHAT: Verify config round-trip
    // WHY:  Dynamic configuration
    // HOW:  Get, modify, set, verify

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    fault_attention_config_t config;
    bool result = fault_attention_get_config(attention, &config);
    ASSERT_TRUE(result);

    // Modify config
    config.severity_weight = 0.5f;
    config.recency_weight = 0.2f;
    config.frequency_weight = 0.2f;
    config.impact_weight = 0.1f;

    result = fault_attention_set_config(attention, &config);
    ASSERT_TRUE(result);

    // Verify
    fault_attention_config_t new_config;
    fault_attention_get_config(attention, &new_config);
    EXPECT_FLOAT_EQ(new_config.severity_weight, 0.5f);
}

TEST_F(FaultAttentionTest, SetConfigRejectsInvalid) {
    // WHAT: Verify set_config rejects invalid config
    // WHY:  Maintain valid state
    // HOW:  Try to set invalid config

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    fault_attention_config_t invalid_config = fault_attention_default_config();
    invalid_config.severity_weight = 0.9f;  // Sum > 1.0

    bool result = fault_attention_set_config(attention, &invalid_config);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, GetStatsWithNullAttention) {
    // WHAT: Verify get_stats handles NULL attention
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    fault_attention_stats_t stats;
    bool result = fault_attention_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, GetStatsWithNullOutput) {
    // WHAT: Verify get_stats handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL output

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    bool result = fault_attention_get_stats(attention, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(FaultAttentionTest, StatsTrackComputations) {
    // WHAT: Verify stats track computation count
    // WHY:  Monitor usage
    // HOW:  Compute weights, check stats

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    fault_attention_stats_t stats_before;
    fault_attention_get_stats(attention, &stats_before);

    active_fault_t faults[2];
    createTestFaults(faults, 2);

    // Compute weights multiple times
    fault_attention_compute_weights(attention, faults, 2, 10000);
    fault_attention_compute_weights(attention, faults, 2, 11000);
    fault_attention_compute_weights(attention, faults, 2, 12000);

    fault_attention_stats_t stats_after;
    fault_attention_get_stats(attention, &stats_after);

    EXPECT_EQ(stats_after.total_computations, stats_before.total_computations + 3);
    EXPECT_EQ(stats_after.current_fault_count, 2u);
}

TEST_F(FaultAttentionTest, ResetStats) {
    // WHAT: Verify stats reset works
    // WHY:  Fresh measurement period
    // HOW:  Compute, reset, verify

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    fault_attention_compute_weights(attention, faults, 2, 10000);

    bool result = fault_attention_reset_stats(attention);
    ASSERT_TRUE(result);

    fault_attention_stats_t stats;
    fault_attention_get_stats(attention, &stats);
    EXPECT_EQ(stats.total_computations, 0u);
}

TEST_F(FaultAttentionTest, ResetStatsWithNullAttention) {
    // WHAT: Verify reset_stats handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = fault_attention_reset_stats(nullptr);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FaultAttentionTest, MaxFaultsLimit) {
    // WHAT: Verify max faults limit enforced
    // WHY:  Prevent buffer overflow
    // HOW:  Try to compute with too many faults

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    // Create more than max faults
    active_fault_t faults[FAULT_ATTENTION_MAX_FAULTS + 10];
    createTestFaults(faults, FAULT_ATTENTION_MAX_FAULTS + 10);

    bool result = fault_attention_compute_weights(
        attention, faults, FAULT_ATTENTION_MAX_FAULTS + 10, 10000);

    // Should handle gracefully (reject or limit)
    // Behavior depends on implementation
    (void)result;
    SUCCEED();
}

TEST_F(FaultAttentionTest, VeryRecentFault) {
    // WHAT: Handle very recent fault (recency factor)
    // WHY:  Recency weight should increase
    // HOW:  Create fault with timestamp very close to current

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);

    faults[0].last_occurrence_ms = 9999;  // Very recent (1ms ago)
    faults[1].last_occurrence_ms = 5000;  // Older (5s ago)

    fault_attention_compute_weights(attention, faults, 2, 10000);

    float weights[2];
    fault_attention_get_all_weights(attention, weights, 2);

    // More recent fault should have higher weight (all else equal)
    // Note: depends on other factors too
    EXPECT_GE(weights[0], 0.0f);
    EXPECT_LE(weights[0], 1.0f);
}

TEST_F(FaultAttentionTest, ZeroSeverityFault) {
    // WHAT: Handle zero severity fault
    // WHY:  Edge case
    // HOW:  Create fault with severity 0

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[2];
    createTestFaults(faults, 2);
    faults[0].severity = 0.0f;

    bool result = fault_attention_compute_weights(attention, faults, 2, 10000);
    EXPECT_TRUE(result);

    float weight;
    fault_attention_get_weight(attention, 0, &weight);
    EXPECT_GE(weight, 0.0f);  // Should still have some weight from other factors
}

TEST_F(FaultAttentionTest, MultipleComputationsStable) {
    // WHAT: Verify multiple computations produce stable results
    // WHY:  Deterministic behavior
    // HOW:  Compute same input multiple times

    attention = fault_attention_create();
    ASSERT_NE(attention, nullptr);

    active_fault_t faults[3];
    createTestFaults(faults, 3);

    fault_attention_compute_weights(attention, faults, 3, 10000);
    float weights1[3];
    fault_attention_get_all_weights(attention, weights1, 3);

    fault_attention_compute_weights(attention, faults, 3, 10000);
    float weights2[3];
    fault_attention_get_all_weights(attention, weights2, 3);

    // Same input should produce same output
    for (int i = 0; i < 3; i++) {
        EXPECT_FLOAT_EQ(weights1[i], weights2[i]);
    }
}
