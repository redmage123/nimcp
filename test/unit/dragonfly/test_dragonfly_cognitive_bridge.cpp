/**
 * @file test_dragonfly_cognitive_bridge.cpp
 * @brief Unit tests for Dragonfly Cognitive Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_cognitive_bridge.h"

class CognitiveBridgeTest : public ::testing::Test {
protected:
    dragonfly_cognitive_bridge_t* bridge = nullptr;
    dragonfly_cognitive_config_t config;

    void SetUp() override {
        ASSERT_EQ(dragonfly_cognitive_bridge_default_config(&config), 0);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_cognitive_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, DefaultConfig) {
    EXPECT_EQ(config.mode, COGNITIVE_BRIDGE_ACTIVE);
    EXPECT_FLOAT_EQ(config.motion_salience_weight, 0.3f);
    EXPECT_FLOAT_EQ(config.velocity_salience_weight, 0.2f);
    EXPECT_FLOAT_EQ(config.direction_salience_weight, 0.3f);
    EXPECT_FLOAT_EQ(config.evasion_salience_weight, 0.2f);
    EXPECT_FLOAT_EQ(config.salience_threshold, 0.2f);
    EXPECT_GT(config.attention_base_width, 0);
    EXPECT_EQ(config.wm_max_targets, COGNITIVE_MAX_WM_TARGETS);
    EXPECT_FLOAT_EQ(config.pursuit_threshold, 0.4f);
    EXPECT_FLOAT_EQ(config.intercept_threshold, 0.7f);
    EXPECT_TRUE(config.allow_target_switching);
}

TEST_F(CognitiveBridgeTest, ValidateConfig) {
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(&config), 0);
}

TEST_F(CognitiveBridgeTest, ValidateNullConfig) {
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(nullptr), -1);
}

TEST_F(CognitiveBridgeTest, ValidateInvalidThreshold) {
    config.salience_threshold = 1.5f;
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(&config), -1);

    config.salience_threshold = -0.1f;
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(&config), -1);
}

TEST_F(CognitiveBridgeTest, ValidateInvalidMaxTargets) {
    config.wm_max_targets = 0;
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(&config), -1);

    config.wm_max_targets = COGNITIVE_MAX_WM_TARGETS + 1;
    EXPECT_EQ(dragonfly_cognitive_bridge_validate_config(&config), -1);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, CreateDestroyNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    dragonfly_cognitive_bridge_destroy(bridge);
    bridge = nullptr;
}

TEST_F(CognitiveBridgeTest, CreateWithConfig) {
    config.mode = COGNITIVE_BRIDGE_PASSIVE;
    config.salience_threshold = 0.5f;

    bridge = dragonfly_cognitive_bridge_create(nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    dragonfly_cognitive_config_t out_config;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_config(bridge, &out_config), 0);
    EXPECT_EQ(out_config.mode, COGNITIVE_BRIDGE_PASSIVE);
    EXPECT_FLOAT_EQ(out_config.salience_threshold, 0.5f);
}

TEST_F(CognitiveBridgeTest, CreateWithInvalidConfig) {
    config.salience_threshold = 2.0f;
    bridge = dragonfly_cognitive_bridge_create(nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(CognitiveBridgeTest, Reset) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_bridge_reset(bridge), 0);
}

//=============================================================================
// Salience Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, ComputeSalienceNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    target_salience_t salience;
    /* Without dragonfly system, should still return success with zeros */
    EXPECT_EQ(dragonfly_cognitive_compute_salience(bridge, 1, &salience), 0);
    EXPECT_FLOAT_EQ(salience.combined_salience, 0);
}

TEST_F(CognitiveBridgeTest, UpdateSalienceNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_update_salience(bridge), 0);

    uint32_t target_id;
    float salience;
    EXPECT_EQ(dragonfly_cognitive_get_most_salient(bridge, &target_id, &salience), 0);
    EXPECT_FLOAT_EQ(salience, 0);
}

TEST_F(CognitiveBridgeTest, SalienceNullInputs) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_compute_salience(nullptr, 1, nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_compute_salience(bridge, 1, nullptr), -1);
}

//=============================================================================
// Attention Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, UpdateAttentionNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_update_attention(bridge), 0);

    dragonfly_attention_focus_t focus;
    EXPECT_EQ(dragonfly_cognitive_get_attention_focus(bridge, &focus), 0);
    EXPECT_EQ(focus.priority, ATTENTION_PRIORITY_NONE);
}

TEST_F(CognitiveBridgeTest, GetMultipleFoci) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    dragonfly_attention_focus_t foci[COGNITIVE_MAX_ATTENTION_FOCI];
    uint32_t num_foci = 0;

    EXPECT_EQ(dragonfly_cognitive_get_attention_foci(bridge, foci, COGNITIVE_MAX_ATTENTION_FOCI, &num_foci), 0);
    EXPECT_EQ(num_foci, 0u);  /* No targets without dragonfly system */
}

TEST_F(CognitiveBridgeTest, SetAttentionPriority) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_set_attention_priority(bridge, 1, ATTENTION_PRIORITY_HIGH), 0);
}

//=============================================================================
// Working Memory Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, UpdateWorkingMemoryNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_update_working_memory(bridge), 0);
    EXPECT_FLOAT_EQ(dragonfly_cognitive_wm_get_occupancy(bridge), 0);
}

TEST_F(CognitiveBridgeTest, WMGetNonexistentTarget) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    wm_target_entry_t entry;
    EXPECT_EQ(dragonfly_cognitive_wm_get_target(bridge, 999, &entry), -1);
}

TEST_F(CognitiveBridgeTest, WMGetAllTargetsEmpty) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    wm_target_entry_t entries[COGNITIVE_MAX_WM_TARGETS];
    uint32_t num_entries = 0;

    EXPECT_EQ(dragonfly_cognitive_wm_get_all_targets(bridge, entries, COGNITIVE_MAX_WM_TARGETS, &num_entries), 0);
    EXPECT_EQ(num_entries, 0u);
}

TEST_F(CognitiveBridgeTest, WMClear) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_wm_clear(bridge), 0);
    EXPECT_FLOAT_EQ(dragonfly_cognitive_wm_get_occupancy(bridge), 0);
}

//=============================================================================
// Executive Control Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, UpdateExecutiveNoSystem) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_update_executive(bridge), 0);

    executive_state_t state;
    EXPECT_EQ(dragonfly_cognitive_get_executive_state(bridge, &state), 0);
    EXPECT_EQ(state.recommended_action, EXEC_ACTION_NONE);
}

TEST_F(CognitiveBridgeTest, GetRecommendedAction) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_get_recommended_action(bridge), EXEC_ACTION_NONE);
}

TEST_F(CognitiveBridgeTest, ExecuteAction) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_execute_action(bridge), 0);
}

TEST_F(CognitiveBridgeTest, RequestAbort) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_request_abort(bridge, "Test abort"), 0);

    executive_state_t state;
    EXPECT_EQ(dragonfly_cognitive_get_executive_state(bridge, &state), 0);
    EXPECT_TRUE(state.abort_recommended);
    EXPECT_EQ(state.current_action, EXEC_ACTION_ABORT);
}

//=============================================================================
// Unified Update Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, UpdateAll) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_update_all(bridge), 0);
}

TEST_F(CognitiveBridgeTest, Step) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_step(bridge, 16.67f), 0);
}

//=============================================================================
// Registration Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, RegisterSalience) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Register NULL (allowed, just clears the reference) */
    EXPECT_EQ(dragonfly_cognitive_register_salience(bridge, nullptr), 0);
}

TEST_F(CognitiveBridgeTest, RegisterAttention) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_register_attention(bridge, nullptr), 0);
}

TEST_F(CognitiveBridgeTest, RegisterWorkingMemory) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_register_working_memory(bridge, nullptr), 0);
}

TEST_F(CognitiveBridgeTest, RegisterExecutive) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_register_executive(bridge, nullptr), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, GetStats) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    cognitive_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.salience_updates, 0u);
    EXPECT_EQ(stats.attention_updates, 0u);
}

TEST_F(CognitiveBridgeTest, StatsUpdatedAfterStep) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Run several update cycles */
    for (int i = 0; i < 5; i++) {
        dragonfly_cognitive_step(bridge, 16.67f);
    }

    cognitive_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.salience_updates, 5u);
    EXPECT_EQ(stats.attention_updates, 5u);
    EXPECT_EQ(stats.executive_updates, 5u);
    /* Note: processing_time may be 0 if execution is faster than clock resolution */
    EXPECT_GE(stats.total_processing_time_us, 0u);
}

TEST_F(CognitiveBridgeTest, ResetStats) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    dragonfly_cognitive_step(bridge, 16.67f);

    EXPECT_EQ(dragonfly_cognitive_bridge_reset_stats(bridge), 0);

    cognitive_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.salience_updates, 0u);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(CognitiveBridgeTest, ActionName) {
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_NONE), "none");
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_TRACK), "track");
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_PURSUE), "pursue");
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_INTERCEPT), "intercept");
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_ABORT), "abort");
    EXPECT_STREQ(dragonfly_cognitive_action_name(EXEC_ACTION_SWITCH_TARGET), "switch_target");
}

TEST_F(CognitiveBridgeTest, PriorityName) {
    EXPECT_STREQ(dragonfly_cognitive_priority_name(ATTENTION_PRIORITY_NONE), "none");
    EXPECT_STREQ(dragonfly_cognitive_priority_name(ATTENTION_PRIORITY_LOW), "low");
    EXPECT_STREQ(dragonfly_cognitive_priority_name(ATTENTION_PRIORITY_MEDIUM), "medium");
    EXPECT_STREQ(dragonfly_cognitive_priority_name(ATTENTION_PRIORITY_HIGH), "high");
    EXPECT_STREQ(dragonfly_cognitive_priority_name(ATTENTION_PRIORITY_CRITICAL), "critical");
}

TEST_F(CognitiveBridgeTest, ModeName) {
    EXPECT_STREQ(dragonfly_cognitive_mode_name(COGNITIVE_BRIDGE_PASSIVE), "passive");
    EXPECT_STREQ(dragonfly_cognitive_mode_name(COGNITIVE_BRIDGE_ACTIVE), "active");
    EXPECT_STREQ(dragonfly_cognitive_mode_name(COGNITIVE_BRIDGE_OVERRIDE), "override");
}

//=============================================================================
// Null Pointer Handling
//=============================================================================

TEST_F(CognitiveBridgeTest, NullPointerHandling) {
    EXPECT_EQ(dragonfly_cognitive_bridge_default_config(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_bridge_reset(nullptr), -1);

    EXPECT_EQ(dragonfly_cognitive_update_salience(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_update_attention(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_update_working_memory(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_update_executive(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_update_all(nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_step(nullptr, 10.0f), -1);

    EXPECT_EQ(dragonfly_cognitive_get_recommended_action(nullptr), EXEC_ACTION_NONE);
    EXPECT_FLOAT_EQ(dragonfly_cognitive_wm_get_occupancy(nullptr), 0);

    cognitive_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_cognitive_bridge_reset_stats(nullptr), -1);

    dragonfly_cognitive_config_t cfg;
    EXPECT_EQ(dragonfly_cognitive_bridge_get_config(nullptr, &cfg), -1);
    EXPECT_EQ(dragonfly_cognitive_bridge_set_config(nullptr, &cfg), -1);
}

TEST_F(CognitiveBridgeTest, NullOutputPointers) {
    bridge = dragonfly_cognitive_bridge_create(nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_cognitive_get_attention_focus(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_get_executive_state(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_cognitive_bridge_get_config(bridge, nullptr), -1);
}
