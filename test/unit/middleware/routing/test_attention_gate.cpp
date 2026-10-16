//=============================================================================
// test_attention_gate.cpp - Comprehensive Attention Gate Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/routing/nimcp_attention_gate.h"

/**
 * WHAT: Comprehensive test suite for attention gate
 * WHY:  Ensure attention mechanisms (top-down, bottom-up, WTA, spotlight) work correctly
 * HOW:  Unit tests for all 11 functions, integration tests, regression tests
 */

class AttentionGateTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    bool FloatEquals(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================
// WHAT: Test gate creation and destruction
// WHY:  Verify resource management and configuration
// HOW:  Test various configurations and edge cases

TEST_F(AttentionGateTest, DefaultConfig_Valid) {
    attention_gate_config_t config = attention_gate_default_config();

    EXPECT_EQ(config.mode, ATTENTION_MODE_MIXED);
    EXPECT_EQ(config.max_targets, ATTENTION_MAX_TARGETS);
    EXPECT_EQ(config.spotlight_size, ATTENTION_SPOTLIGHT_SIZE);
    EXPECT_FALSE(config.enable_winner_take_all);
    EXPECT_TRUE(config.enable_shift_detection);
    EXPECT_FLOAT_EQ(config.topdown_weight, 0.7f);
    EXPECT_FLOAT_EQ(config.bottomup_weight, 0.3f);
}

TEST_F(AttentionGateTest, Create_Success_DefaultConfig) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);

    ASSERT_NE(gate, nullptr);
    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Create_Success_CustomConfig) {
    attention_gate_config_t config = attention_gate_default_config();
    config.max_targets = 100;
    config.spotlight_size = 5;

    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Create_Failure_NullConfig) {
    attention_gate_t* gate = attention_gate_create(nullptr);
    EXPECT_EQ(gate, nullptr);
}

TEST_F(AttentionGateTest, Create_Failure_ZeroTargets) {
    attention_gate_config_t config = attention_gate_default_config();
    config.max_targets = 0;

    attention_gate_t* gate = attention_gate_create(&config);
    EXPECT_EQ(gate, nullptr);
}

TEST_F(AttentionGateTest, Destroy_NullSafe) {
    attention_gate_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// WEIGHT OPERATIONS TESTS
//=============================================================================
// WHAT: Test top-down attention weight setting and retrieval
// WHY:  Verify explicit attention control mechanism
// HOW:  Set weights, verify retrieval, test modes

TEST_F(AttentionGateTest, SetWeight_Success) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    bool result = attention_gate_set_weight(gate, 1, 100, 0.8f);
    EXPECT_TRUE(result);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, SetGet_Weight_RoundTrip) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.6f);

    float weight = 0.0f;
    bool result = attention_gate_get_weight(gate, 1, 100, &weight);
    EXPECT_TRUE(result);
    EXPECT_GT(weight, 0.0f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, SetWeight_Failure_NullGate) {
    bool result = attention_gate_set_weight(nullptr, 1, 100, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(AttentionGateTest, SetWeight_Failure_InvalidWeight) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    EXPECT_FALSE(attention_gate_set_weight(gate, 1, 100, -0.1f));
    EXPECT_FALSE(attention_gate_set_weight(gate, 1, 100, 1.1f));

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, GetWeight_NonExistentTarget_ReturnsPassThrough) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    // Non-existent targets return 1.0 (pass-through / unmodulated)
    // This ensures signals to unregistered routes are not blocked
    float weight = 99.9f;
    bool result = attention_gate_get_weight(gate, 1, 999, &weight);
    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(weight, 1.0f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, GetWeight_Failure_NullParams) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);

    float weight = 0.0f;
    EXPECT_FALSE(attention_gate_get_weight(nullptr, 1, 100, &weight));
    EXPECT_FALSE(attention_gate_get_weight(gate, 1, 100, nullptr));

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Mode_TopDown_UsesOnlyTopDown) {
    attention_gate_config_t config = attention_gate_default_config();
    config.mode = ATTENTION_MODE_TOPDOWN;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.8f);
    attention_gate_update_salience(gate, 100, 0.5f);

    float weight = 0.0f;
    attention_gate_get_weight(gate, 1, 100, &weight);
    EXPECT_FLOAT_EQ(weight, 0.8f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Mode_BottomUp_UsesOnlySalience) {
    attention_gate_config_t config = attention_gate_default_config();
    config.mode = ATTENTION_MODE_BOTTOMUP;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.8f);
    attention_gate_update_salience(gate, 100, 0.5f);

    float weight = 0.0f;
    attention_gate_get_weight(gate, 1, 100, &weight);
    EXPECT_FLOAT_EQ(weight, 0.5f);

    attention_gate_destroy(gate);
}

//=============================================================================
// SALIENCE UPDATE TESTS
//=============================================================================
// WHAT: Test bottom-up salience updates
// WHY:  Verify automatic attention capture
// HOW:  Update salience, check combined weights

TEST_F(AttentionGateTest, UpdateSalience_Success) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    bool result = attention_gate_update_salience(gate, 100, 0.7f);
    EXPECT_TRUE(result);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, UpdateSalience_Failure_NullGate) {
    bool result = attention_gate_update_salience(nullptr, 100, 0.5f);
    EXPECT_FALSE(result);
}

TEST_F(AttentionGateTest, UpdateSalience_Failure_InvalidSalience) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    EXPECT_FALSE(attention_gate_update_salience(gate, 100, -0.1f));
    EXPECT_FALSE(attention_gate_update_salience(gate, 100, 1.1f));

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, UpdateSalience_CreatesEntryIfNeeded) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    bool result = attention_gate_update_salience(gate, 200, 0.6f);
    EXPECT_TRUE(result);

    float weight = 0.0f;
    attention_gate_get_weight(gate, 0, 200, &weight);
    EXPECT_GT(weight, 0.0f);

    attention_gate_destroy(gate);
}

//=============================================================================
// WINNER-TAKE-ALL TESTS
//=============================================================================
// WHAT: Test WTA competition mechanism
// WHY:  Verify selective attention via competition
// HOW:  Apply WTA, check winner selection and suppression

TEST_F(AttentionGateTest, ApplyWTA_Success_SelectsMaxWeight) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.5f);
    attention_gate_set_weight(gate, 1, 101, 0.8f);
    attention_gate_set_weight(gate, 1, 102, 0.3f);

    uint32_t winner_id = 0;
    bool result = attention_gate_apply_wta(gate, &winner_id);
    EXPECT_TRUE(result);
    EXPECT_EQ(winner_id, 101);

    float winner_weight = 0.0f;
    attention_gate_get_weight(gate, 1, 101, &winner_weight);
    EXPECT_FLOAT_EQ(winner_weight, 1.0f);

    float loser_weight = 0.0f;
    attention_gate_get_weight(gate, 1, 100, &loser_weight);
    EXPECT_LT(loser_weight, 0.5f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, ApplyWTA_Failure_NoEntries) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    bool result = attention_gate_apply_wta(gate, nullptr);
    EXPECT_FALSE(result);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, ApplyWTA_Failure_NullGate) {
    bool result = attention_gate_apply_wta(nullptr, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// SPOTLIGHT TESTS
//=============================================================================
// WHAT: Test attention spotlight selection
// WHY:  Verify limited capacity attention mechanism
// HOW:  Update spotlight, verify top-N selection

TEST_F(AttentionGateTest, UpdateSpotlight_Success) {
    attention_gate_config_t config = attention_gate_default_config();
    config.spotlight_size = 3;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.2f);
    attention_gate_set_weight(gate, 1, 101, 0.8f);
    attention_gate_set_weight(gate, 1, 102, 0.5f);
    attention_gate_set_weight(gate, 1, 103, 0.9f);
    attention_gate_set_weight(gate, 1, 104, 0.3f);

    uint32_t spotlight_ids[3];
    uint32_t num_in_spotlight = 0;

    bool result = attention_gate_update_spotlight(gate, spotlight_ids, &num_in_spotlight);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_in_spotlight, 3);

    EXPECT_EQ(spotlight_ids[0], 103);
    EXPECT_EQ(spotlight_ids[1], 101);
    EXPECT_EQ(spotlight_ids[2], 102);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, UpdateSpotlight_LessThanCapacity) {
    attention_gate_config_t config = attention_gate_default_config();
    config.spotlight_size = 10;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.5f);
    attention_gate_set_weight(gate, 1, 101, 0.7f);

    uint32_t num_in_spotlight = 0;
    bool result = attention_gate_update_spotlight(gate, nullptr, &num_in_spotlight);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_in_spotlight, 2);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, UpdateSpotlight_Failure_NullGate) {
    bool result = attention_gate_update_spotlight(nullptr, nullptr, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// SHIFT DETECTION TESTS
//=============================================================================
// WHAT: Test attention shift tracking
// WHY:  Monitor attention transitions
// HOW:  Cause shifts, retrieve history

TEST_F(AttentionGateTest, GetShifts_Success) {
    attention_gate_config_t config = attention_gate_default_config();
    config.enable_shift_detection = true;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.2f);
    attention_gate_set_weight(gate, 1, 100, 0.8f);

    attention_shift_t shifts[10];
    uint32_t num_shifts = 0;

    bool result = attention_gate_get_shifts(gate, shifts, 10, &num_shifts);
    EXPECT_TRUE(result);
    EXPECT_GT(num_shifts, 0);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, GetShifts_Failure_NullParams) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);

    attention_shift_t shifts[10];
    uint32_t num_shifts = 0;

    EXPECT_FALSE(attention_gate_get_shifts(nullptr, shifts, 10, &num_shifts));
    EXPECT_FALSE(attention_gate_get_shifts(gate, nullptr, 10, &num_shifts));
    EXPECT_FALSE(attention_gate_get_shifts(gate, shifts, 10, nullptr));

    attention_gate_destroy(gate);
}

//=============================================================================
// RESET AND STATS TESTS
//=============================================================================
// WHAT: Test reset and statistics operations
// WHY:  Verify state management
// HOW:  Reset gate, verify clean state, check stats

TEST_F(AttentionGateTest, Reset_ClearsAllWeights) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.8f);
    attention_gate_set_weight(gate, 1, 101, 0.6f);

    attention_gate_reset(gate);

    float weight = 99.9f;
    attention_gate_get_weight(gate, 1, 100, &weight);
    EXPECT_FLOAT_EQ(weight, 0.0f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Reset_NullSafe) {
    attention_gate_reset(nullptr);
}

TEST_F(AttentionGateTest, GetStats_Success) {
    attention_gate_config_t config = attention_gate_default_config();
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 0.5f);
    attention_gate_set_weight(gate, 1, 101, 0.7f);

    uint32_t num_targets = 0;
    uint32_t num_in_spotlight = 0;
    uint64_t total_shifts = 0;

    bool result = attention_gate_get_stats(gate, &num_targets, &num_in_spotlight, &total_shifts);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_targets, 2);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, GetStats_Failure_NullGate) {
    uint32_t num_targets = 0;
    bool result = attention_gate_get_stats(nullptr, &num_targets, nullptr, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// REGRESSION TESTS
//=============================================================================
// WHAT: Test edge cases and known issues
// WHY:  Prevent regressions
// HOW:  Test boundary conditions

TEST_F(AttentionGateTest, Regression_MixedMode_ClampsCombinedWeight) {
    attention_gate_config_t config = attention_gate_default_config();
    config.mode = ATTENTION_MODE_MIXED;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    attention_gate_set_weight(gate, 1, 100, 1.0f);
    attention_gate_update_salience(gate, 100, 1.0f);

    float weight = 0.0f;
    attention_gate_get_weight(gate, 1, 100, &weight);

    EXPECT_LE(weight, 1.0f);

    attention_gate_destroy(gate);
}

TEST_F(AttentionGateTest, Regression_CapacityLimit) {
    attention_gate_config_t config = attention_gate_default_config();
    config.max_targets = 5;
    attention_gate_t* gate = attention_gate_create(&config);
    ASSERT_NE(gate, nullptr);

    for (uint32_t i = 0; i < 10; i++) {
        attention_gate_set_weight(gate, 1, 100 + i, 0.5f);
    }

    uint32_t num_targets = 0;
    attention_gate_get_stats(gate, &num_targets, nullptr, nullptr);
    EXPECT_LE(num_targets, config.max_targets);

    attention_gate_destroy(gate);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
