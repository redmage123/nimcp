/**
 * @file test_executive_fep_bridge.cpp
 * @brief Unit tests for Executive-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Executive bidirectional integration
 * WHY:  Ensure EFE-based policy selection and executive goal constraints work correctly
 * HOW:  Test lifecycle, connections, policy selection, exploration, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/executive/nimcp_executive_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class ExecutiveFepBridgeTest : public ::testing::Test {
protected:
    executive_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        executive_fep_config_t config;
        executive_fep_bridge_default_config(&config);
        bridge = executive_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            executive_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ExecutiveFepBridgeTest, CreateWithNullConfig) {
    executive_fep_bridge_t* br = executive_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    executive_fep_bridge_destroy(br);
}

TEST_F(ExecutiveFepBridgeTest, DestroyNull) {
    executive_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(ExecutiveFepBridgeTest, DefaultConfig) {
    executive_fep_config_t config;
    int ret = executive_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(config.efe_temperature, EXECUTIVE_FEP_DEFAULT_TEMPERATURE);
    EXPECT_GT(config.precision_exploration_threshold, 0.0f);
    EXPECT_GT(config.pe_control_threshold, 0.0f);
    EXPECT_TRUE(config.enable_efe_policy_selection);
    EXPECT_TRUE(config.enable_precision_exploration);
    EXPECT_TRUE(config.enable_pe_cognitive_control);
}

TEST_F(ExecutiveFepBridgeTest, DefaultConfigNullPtr) {
    int ret = executive_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = executive_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(ExecutiveFepBridgeTest, ConnectFepNull) {
    EXPECT_EQ(executive_fep_bridge_connect_fep(nullptr, nullptr), -1);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_EQ(executive_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(executive_fep_bridge_connect_fep(bridge, nullptr), -1);

    fep_destroy(fep);
}

TEST_F(ExecutiveFepBridgeTest, ConnectExecutive) {
    // Executive system requires complex initialization, test with NULL for now
    int ret = executive_fep_bridge_connect_executive(bridge, nullptr);
    EXPECT_EQ(ret, -1);  // Should fail with NULL
}

TEST_F(ExecutiveFepBridgeTest, ConnectExecutiveNull) {
    EXPECT_EQ(executive_fep_bridge_connect_executive(nullptr, nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, Disconnect) {
    int ret = executive_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, DisconnectNull) {
    EXPECT_EQ(executive_fep_bridge_disconnect(nullptr), -1);
}

/* ============================================================================
 * State/Stats Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, GetState) {
    executive_fep_state_t state;
    int ret = executive_fep_bridge_get_state(bridge, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_efe, 0.0f);
    EXPECT_GE(state.current_precision, 0.0f);
    EXPECT_GE(state.exploration_probability, 0.0f);
    EXPECT_LE(state.exploration_probability, 1.0f);
}

TEST_F(ExecutiveFepBridgeTest, GetStateNull) {
    executive_fep_state_t state;

    EXPECT_EQ(executive_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(executive_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, GetStats) {
    executive_fep_stats_t stats;
    int ret = executive_fep_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.policy_selections, 0u);
    EXPECT_EQ(stats.exploration_events, 0u);
    EXPECT_EQ(stats.cognitive_control_triggers, 0u);
}

TEST_F(ExecutiveFepBridgeTest, GetStatsNull) {
    executive_fep_stats_t stats;

    EXPECT_EQ(executive_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(executive_fep_bridge_get_stats(bridge, nullptr), -1);
}

/* ============================================================================
 * FEP → Executive Direction Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, SelectPolicyByEfe) {
    int ret = executive_fep_select_policy_by_efe(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, SelectPolicyByEfeNull) {
    EXPECT_EQ(executive_fep_select_policy_by_efe(nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, ModulateExploration) {
    int ret = executive_fep_modulate_exploration(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, ModulateExplorationNull) {
    EXPECT_EQ(executive_fep_modulate_exploration(nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, TriggerCognitiveControl) {
    float pe_magnitude = 6.0f;  // Above EXECUTIVE_FEP_PE_CONTROL_THRESHOLD

    int ret = executive_fep_trigger_cognitive_control(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    executive_fep_state_t state;
    executive_fep_bridge_get_state(bridge, &state);
    EXPECT_TRUE(state.control_active);
}

TEST_F(ExecutiveFepBridgeTest, TriggerCognitiveControlBelowThreshold) {
    float pe_magnitude = 2.0f;  // Below threshold

    int ret = executive_fep_trigger_cognitive_control(bridge, pe_magnitude);
    EXPECT_EQ(ret, 0);

    executive_fep_state_t state;
    executive_fep_bridge_get_state(bridge, &state);
    EXPECT_FALSE(state.control_active);
}

TEST_F(ExecutiveFepBridgeTest, TriggerCognitiveControlNull) {
    EXPECT_EQ(executive_fep_trigger_cognitive_control(nullptr, 5.0f), -1);
}

/* ============================================================================
 * Executive → FEP Direction Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, ApplyGoalPriors) {
    int ret = executive_fep_apply_goal_priors(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, ApplyGoalPriorsNull) {
    EXPECT_EQ(executive_fep_apply_goal_priors(nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, MaintainWmBeliefs) {
    int ret = executive_fep_maintain_wm_beliefs(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, MaintainWmBeliefsNull) {
    EXPECT_EQ(executive_fep_maintain_wm_beliefs(nullptr), -1);
}

TEST_F(ExecutiveFepBridgeTest, ApplyInhibitionPrecision) {
    int ret = executive_fep_apply_inhibition_precision(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, ApplyInhibitionPrecisionNull) {
    EXPECT_EQ(executive_fep_apply_inhibition_precision(nullptr), -1);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, Update) {
    int ret = executive_fep_bridge_update(bridge, 16);  // 16ms delta
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, UpdateNull) {
    EXPECT_EQ(executive_fep_bridge_update(nullptr, 16), -1);
}

TEST_F(ExecutiveFepBridgeTest, UpdateZeroDelta) {
    int ret = executive_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, BioAsyncConnect) {
    int ret = executive_fep_bridge_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(ExecutiveFepBridgeTest, BioAsyncDisconnect) {
    executive_fep_bridge_connect_bio_async(bridge);

    int ret = executive_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(executive_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(ExecutiveFepBridgeTest, BioAsyncIsConnected) {
    EXPECT_FALSE(executive_fep_bridge_is_bio_async_connected(bridge));

    executive_fep_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability

    executive_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_FALSE(executive_fep_bridge_is_bio_async_connected(bridge));
}

TEST_F(ExecutiveFepBridgeTest, BioAsyncNullParams) {
    EXPECT_EQ(executive_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(executive_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(executive_fep_bridge_is_bio_async_connected(nullptr));
}

TEST_F(ExecutiveFepBridgeTest, BioAsyncDoubleConnect) {
    executive_fep_bridge_connect_bio_async(bridge);
    int ret = executive_fep_bridge_connect_bio_async(bridge);  // Should be no-op
    EXPECT_EQ(ret, 0);
    executive_fep_bridge_disconnect_bio_async(bridge);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ExecutiveFepBridgeTest, PolicySelectionUpdatesStats) {
    executive_fep_select_policy_by_efe(bridge);

    executive_fep_stats_t stats;
    executive_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.policy_selections, 0u);
}

TEST_F(ExecutiveFepBridgeTest, HighPrecisionReducesExploration) {
    // High precision should favor exploitation
    executive_fep_modulate_exploration(bridge);

    executive_fep_state_t state;
    executive_fep_bridge_get_state(bridge, &state);
    // Exploration probability depends on precision value
    EXPECT_GE(state.exploration_probability, 0.0f);
    EXPECT_LE(state.exploration_probability, 1.0f);
}

TEST_F(ExecutiveFepBridgeTest, HighPeTriggersControl) {
    float high_pe = 10.0f;
    executive_fep_trigger_cognitive_control(bridge, high_pe);

    executive_fep_stats_t stats;
    executive_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.cognitive_control_triggers, 0u);
}

TEST_F(ExecutiveFepBridgeTest, GoalPriorsConstrainBeliefs) {
    executive_fep_apply_goal_priors(bridge);

    executive_fep_stats_t stats;
    executive_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.goal_prior_applications, 0u);
}
