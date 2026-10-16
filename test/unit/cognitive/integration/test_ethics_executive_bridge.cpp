/**
 * @file test_ethics_executive_bridge.cpp
 * @brief Unit tests for Ethics-Executive Integration Bridge
 *
 * Tests bidirectional integration between ethics and executive systems.
 * Ethics constrains executive action selection; executive functions
 * evaluate and implement ethical constraints.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/integration/nimcp_ethics_executive_bridge.h"

class EthicsExecutiveBridgeTest : public ::testing::Test {
protected:
    ethics_executive_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = ethics_executive_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ethics_executive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, BridgeCreation) {
    // Bridge was created in SetUp - verify it's not null
    EXPECT_NE(bridge, nullptr);

    // Create and destroy a separate bridge
    ethics_executive_bridge_t* test_bridge = ethics_executive_bridge_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    ethics_executive_bridge_destroy(test_bridge);
}

TEST_F(EthicsExecutiveBridgeTest, CreateWithConfig) {
    ethics_executive_config_t config;
    int ret = ethics_executive_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);

    config.ethical_threshold = 0.7f;
    config.veto_enabled = false;

    ethics_executive_bridge_t* custom = ethics_executive_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    ethics_executive_bridge_destroy(custom);
}

TEST_F(EthicsExecutiveBridgeTest, DestroyNullSafe) {
    // Should not crash
    ethics_executive_bridge_destroy(nullptr);
}

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, DefaultConfig) {
    ethics_executive_config_t config;
    memset(&config, 0, sizeof(config));

    int ret = ethics_executive_bridge_default_config(&config);
    EXPECT_EQ(ret, 0);

    // Default values: threshold=0.6, veto_enabled=true, strictness=0.8
    EXPECT_FLOAT_EQ(config.ethical_threshold, 0.6f);
    EXPECT_TRUE(config.veto_enabled);
    EXPECT_FLOAT_EQ(config.constraint_strictness, 0.8f);
}

TEST_F(EthicsExecutiveBridgeTest, DefaultConfigNullPointer) {
    int ret = ethics_executive_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Action Evaluation Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, EvaluateAction) {
    uint64_t action_id = 1001;
    float ethical_score = 0.0f;

    int ret = ethics_executive_evaluate_action(bridge, action_id, &ethical_score);
    EXPECT_EQ(ret, 0);

    // Score should be within valid range [0, 1]
    EXPECT_GE(ethical_score, ETHICS_EXECUTIVE_SCORE_MIN);
    EXPECT_LE(ethical_score, ETHICS_EXECUTIVE_SCORE_MAX);
}

TEST_F(EthicsExecutiveBridgeTest, EvaluateMultipleActions) {
    float scores[3] = {0.0f, 0.0f, 0.0f};

    for (uint64_t i = 0; i < 3; i++) {
        int ret = ethics_executive_evaluate_action(bridge, i + 100, &scores[i]);
        EXPECT_EQ(ret, 0);
        EXPECT_GE(scores[i], 0.0f);
        EXPECT_LE(scores[i], 1.0f);
    }
}

TEST_F(EthicsExecutiveBridgeTest, EvaluateActionNullBridge) {
    float score = 0.0f;
    int ret = ethics_executive_evaluate_action(nullptr, 1, &score);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsExecutiveBridgeTest, EvaluateActionNullScore) {
    int ret = ethics_executive_evaluate_action(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Constraint Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, ConstrainAction) {
    uint64_t action_id = 2001;
    ethics_constraints_out_t constraints;
    memset(&constraints, 0, sizeof(constraints));

    int ret = ethics_executive_constrain_action(bridge, action_id, &constraints);
    EXPECT_EQ(ret, 0);

    // Verify constraints output is populated
    EXPECT_GE(constraints.overall_ethical_score, 0.0f);
    EXPECT_LE(constraints.overall_ethical_score, 1.0f);
    EXPECT_LE(constraints.constraint_count, ETHICS_EXECUTIVE_MAX_CONSTRAINTS);
}

TEST_F(EthicsExecutiveBridgeTest, ConstrainActionNullBridge) {
    ethics_constraints_out_t constraints;
    int ret = ethics_executive_constrain_action(nullptr, 1, &constraints);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsExecutiveBridgeTest, ConstrainActionNullConstraints) {
    int ret = ethics_executive_constrain_action(bridge, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Veto Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, VetoAction) {
    uint64_t action_id = 3001;

    // First evaluate to get a score
    float score = 0.0f;
    ethics_executive_evaluate_action(bridge, action_id, &score);

    // Veto action - returns 0 if vetoed, 1 if not vetoed, -1 on error
    int ret = ethics_executive_veto_action(bridge, action_id);
    EXPECT_GE(ret, 0);  // Should succeed (not -1)
}

TEST_F(EthicsExecutiveBridgeTest, VetoActionPasses) {
    // Create a bridge with a very low threshold so actions pass
    ethics_executive_config_t config;
    ethics_executive_bridge_default_config(&config);
    config.ethical_threshold = 0.1f;  // Very low threshold

    ethics_executive_bridge_t* permissive_bridge = ethics_executive_bridge_create(&config);
    ASSERT_NE(permissive_bridge, nullptr);

    uint64_t action_id = 3002;
    float score = 0.0f;
    ethics_executive_evaluate_action(permissive_bridge, action_id, &score);

    // If score >= threshold, action should not be vetoed
    // Veto returns 0 if vetoed, 1 if not vetoed, -1 on error
    int ret = ethics_executive_veto_action(permissive_bridge, action_id);
    EXPECT_GE(ret, 0);  // Should succeed (not -1)

    ethics_executive_bridge_destroy(permissive_bridge);
}

TEST_F(EthicsExecutiveBridgeTest, VetoDisabled) {
    // Create a bridge with veto disabled
    ethics_executive_config_t config;
    ethics_executive_bridge_default_config(&config);
    config.veto_enabled = false;

    ethics_executive_bridge_t* no_veto_bridge = ethics_executive_bridge_create(&config);
    ASSERT_NE(no_veto_bridge, nullptr);

    uint64_t action_id = 3003;

    // Veto should return -1 when disabled
    int ret = ethics_executive_veto_action(no_veto_bridge, action_id);
    EXPECT_EQ(ret, -1);

    ethics_executive_bridge_destroy(no_veto_bridge);
}

TEST_F(EthicsExecutiveBridgeTest, VetoActionNullBridge) {
    int ret = ethics_executive_veto_action(nullptr, 1);
    EXPECT_EQ(ret, -1);
}

//=============================================================================
// Permitted Actions Query Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, GetPermittedActions) {
    uint64_t actions[ETHICS_EXECUTIVE_MAX_ACTIONS];
    memset(actions, 0, sizeof(actions));

    // Evaluate some actions first
    for (uint64_t i = 0; i < 5; i++) {
        float score = 0.0f;
        ethics_executive_evaluate_action(bridge, i + 1, &score);
    }

    int count = ethics_executive_get_permitted_actions(bridge, actions, ETHICS_EXECUTIVE_MAX_ACTIONS);
    EXPECT_GE(count, 0);
    EXPECT_LE(count, ETHICS_EXECUTIVE_MAX_ACTIONS);
}

TEST_F(EthicsExecutiveBridgeTest, GetPermittedActionsLimitedBuffer) {
    uint64_t actions[3];
    memset(actions, 0, sizeof(actions));

    // Evaluate more actions than buffer can hold
    for (uint64_t i = 0; i < 10; i++) {
        float score = 0.0f;
        ethics_executive_evaluate_action(bridge, i + 1, &score);
    }

    int count = ethics_executive_get_permitted_actions(bridge, actions, 3);
    EXPECT_GE(count, 0);
    EXPECT_LE(count, 3);  // Should not exceed buffer size
}

TEST_F(EthicsExecutiveBridgeTest, GetPermittedActionsNullBridge) {
    uint64_t actions[10];
    int count = ethics_executive_get_permitted_actions(nullptr, actions, 10);
    EXPECT_EQ(count, -1);
}

TEST_F(EthicsExecutiveBridgeTest, GetPermittedActionsNullArray) {
    int count = ethics_executive_get_permitted_actions(bridge, nullptr, 10);
    EXPECT_EQ(count, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, StatsTracking) {
    ethics_executive_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Perform some operations
    for (uint64_t i = 0; i < 5; i++) {
        float score = 0.0f;
        ethics_executive_evaluate_action(bridge, i + 1, &score);
    }

    // Constrain an action
    ethics_constraints_out_t constraints;
    ethics_executive_constrain_action(bridge, 1, &constraints);

    // Veto an action
    ethics_executive_veto_action(bridge, 2);

    // Get stats
    int ret = ethics_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Verify stats are tracked
    EXPECT_GE(stats.evaluations_performed, 5u);
    EXPECT_GE(stats.actions_constrained, 0u);
    EXPECT_GE(stats.actions_vetoed, 0u);
    EXPECT_GE(stats.avg_ethical_score, 0.0f);
    EXPECT_LE(stats.avg_ethical_score, 1.0f);
    EXPECT_GE(stats.veto_rate, 0.0f);
    EXPECT_LE(stats.veto_rate, 1.0f);
}

TEST_F(EthicsExecutiveBridgeTest, StatsNullBridge) {
    ethics_executive_stats_t stats;
    int ret = ethics_executive_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsExecutiveBridgeTest, StatsNullOutput) {
    int ret = ethics_executive_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(EthicsExecutiveBridgeTest, StatsInitialValues) {
    // Get stats from fresh bridge
    ethics_executive_stats_t stats;
    int ret = ethics_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    // Initial stats should be zero or reasonable defaults
    EXPECT_EQ(stats.evaluations_performed, 0u);
    EXPECT_EQ(stats.actions_constrained, 0u);
    EXPECT_EQ(stats.actions_vetoed, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EthicsExecutiveBridgeTest, FullEvaluationPipeline) {
    uint64_t action_id = 5001;

    // 1. Evaluate action ethically
    float score = 0.0f;
    int ret = ethics_executive_evaluate_action(bridge, action_id, &score);
    EXPECT_EQ(ret, 0);

    // 2. Get constraints for action
    ethics_constraints_out_t constraints;
    ret = ethics_executive_constrain_action(bridge, action_id, &constraints);
    EXPECT_EQ(ret, 0);

    // 3. Check if action is permitted based on constraints
    EXPECT_GE(constraints.overall_ethical_score, 0.0f);

    // 4. If not permitted, veto
    if (!constraints.action_permitted) {
        ret = ethics_executive_veto_action(bridge, action_id);
        EXPECT_EQ(ret, 0);
    }

    // 5. Verify stats were updated
    ethics_executive_stats_t stats;
    ret = ethics_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.evaluations_performed, 1u);
}

TEST_F(EthicsExecutiveBridgeTest, MultipleActionWorkflow) {
    const size_t num_actions = 10;

    // Evaluate multiple actions
    for (uint64_t i = 0; i < num_actions; i++) {
        float score = 0.0f;
        ethics_executive_evaluate_action(bridge, i + 1, &score);
    }

    // Get all permitted actions
    uint64_t permitted[ETHICS_EXECUTIVE_MAX_ACTIONS];
    int permitted_count = ethics_executive_get_permitted_actions(
        bridge, permitted, ETHICS_EXECUTIVE_MAX_ACTIONS);
    EXPECT_GE(permitted_count, 0);

    // Verify stats
    ethics_executive_stats_t stats;
    ethics_executive_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.evaluations_performed, num_actions);
}
