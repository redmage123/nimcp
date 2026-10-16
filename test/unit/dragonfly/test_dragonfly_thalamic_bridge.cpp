/**
 * @file test_dragonfly_thalamic_bridge.cpp
 * @brief Unit tests for Dragonfly-Thalamic Bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_thalamic_bridge.h"

class ThalamicBridgeTest : public ::testing::Test {
protected:
    dragonfly_thalamic_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, DefaultConfig) {
    dragonfly_thalamic_config_t config;
    EXPECT_EQ(dragonfly_thalamic_bridge_default_config(&config), 0);

    EXPECT_EQ(config.initial_mode, THAL_ROUTE_DISCOVERY);
    EXPECT_GT(config.lgn_channels, 0u);
    EXPECT_GT(config.lgn_attention_baseline, 0);
    EXPECT_LE(config.lgn_attention_baseline, 1.0f);
    EXPECT_GT(config.pulvinar_gain, 0);
    EXPECT_GT(config.motor_channels, 0u);
    EXPECT_GT(config.update_rate_hz, 0);
}

TEST_F(ThalamicBridgeTest, ValidateConfig) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    EXPECT_EQ(dragonfly_thalamic_bridge_validate_config(&config), 0);
}

TEST_F(ThalamicBridgeTest, ValidateNullConfig) {
    EXPECT_EQ(dragonfly_thalamic_bridge_validate_config(nullptr), -1);
}

TEST_F(ThalamicBridgeTest, ValidateInvalidChannels) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.lgn_channels = 0;
    EXPECT_EQ(dragonfly_thalamic_bridge_validate_config(&config), -1);
}

TEST_F(ThalamicBridgeTest, ValidateInvalidAttention) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.lgn_attention_baseline = 1.5f;  /* Out of range */
    EXPECT_EQ(dragonfly_thalamic_bridge_validate_config(&config), -1);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, CreateDestroyNoSystems) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    /* Destructor in TearDown */
}

TEST_F(ThalamicBridgeTest, CreateWithConfig) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.initial_mode = THAL_ROUTE_TRACKING;
    config.lgn_channels = 64;

    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(dragonfly_thalamic_get_mode(bridge), THAL_ROUTE_TRACKING);
}

TEST_F(ThalamicBridgeTest, CreateWithInvalidConfig) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.lgn_channels = 0;  /* Invalid */

    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, &config);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ThalamicBridgeTest, Reset) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_INTERCEPT);
    EXPECT_EQ(dragonfly_thalamic_bridge_reset(bridge), 0);
    EXPECT_EQ(dragonfly_thalamic_get_mode(bridge), THAL_ROUTE_DISCOVERY);
}

//=============================================================================
// Mode Control Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, SetMode) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_TRACKING), 0);
    EXPECT_EQ(dragonfly_thalamic_get_mode(bridge), THAL_ROUTE_TRACKING);

    EXPECT_EQ(dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_INTERCEPT), 0);
    EXPECT_EQ(dragonfly_thalamic_get_mode(bridge), THAL_ROUTE_INTERCEPT);

    EXPECT_EQ(dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_SUPPRESSED), 0);
    EXPECT_EQ(dragonfly_thalamic_get_mode(bridge), THAL_ROUTE_SUPPRESSED);
}

TEST_F(ThalamicBridgeTest, SetAttention) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_thalamic_set_attention(bridge, THAL_SIGNAL_POSITION, 0.8f), 0);
    EXPECT_EQ(dragonfly_thalamic_set_attention(bridge, THAL_SIGNAL_VELOCITY, 0.6f), 0);
    EXPECT_EQ(dragonfly_thalamic_set_attention(bridge, THAL_SIGNAL_SALIENCE, 0.9f), 0);
    EXPECT_EQ(dragonfly_thalamic_set_attention(bridge, THAL_SIGNAL_MOTOR_CMD, 0.7f), 0);
    EXPECT_EQ(dragonfly_thalamic_set_attention(bridge, THAL_SIGNAL_DECISION, 0.5f), 0);
}

TEST_F(ThalamicBridgeTest, ApplyInhibition) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_thalamic_apply_inhibition(bridge, THAL_SIGNAL_POSITION, 0.5f), 0);
    EXPECT_EQ(dragonfly_thalamic_apply_inhibition(bridge, THAL_SIGNAL_MOTOR_CMD, 0.3f), 0);
}

//=============================================================================
// Signal Routing Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, RelayVisual) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    thal_visual_target_t target;
    memset(&target, 0, sizeof(target));
    target.angular_position[0] = 0.5f;
    target.angular_position[1] = -0.2f;
    target.angular_velocity[0] = 0.1f;
    target.angular_velocity[1] = 0.05f;
    target.size = 0.02f;
    target.contrast = 0.8f;
    target.motion_energy = 0.6f;

    EXPECT_EQ(dragonfly_thalamic_relay_visual(bridge, &target), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_signals_relayed, 1u);
}

TEST_F(ThalamicBridgeTest, RelayMotor) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    thal_motor_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.heading_adjustment[0] = 0.1f;
    cmd.heading_adjustment[1] = 0.05f;
    cmd.thrust = 0.7f;
    cmd.urgency = 0.8f;
    cmd.is_pursuit = true;

    EXPECT_EQ(dragonfly_thalamic_relay_motor(bridge, &cmd), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.motor_signals_relayed, 1u);
}

TEST_F(ThalamicBridgeTest, RelayAttention) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    thal_attention_signal_t attn;
    memset(&attn, 0, sizeof(attn));
    attn.spatial_attention[0] = 0.5f;
    attn.spatial_attention[1] = 0.3f;
    attn.attention_width = 0.4f;
    attn.salience = 0.7f;  /* Above threshold */
    attn.priority = 0.8f;
    attn.is_covert = true;

    EXPECT_EQ(dragonfly_thalamic_relay_attention(bridge, &attn), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_signals_relayed, 1u);
}

TEST_F(ThalamicBridgeTest, RelayAttentionBelowThreshold) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.pulvinar_threshold = 0.5f;

    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    thal_attention_signal_t attn;
    memset(&attn, 0, sizeof(attn));
    attn.salience = 0.3f;  /* Below threshold */

    EXPECT_EQ(dragonfly_thalamic_relay_attention(bridge, &attn), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_signals_relayed, 0u);  /* Not relayed */
}

TEST_F(ThalamicBridgeTest, RelayDecision) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    thal_decision_signal_t decision;
    memset(&decision, 0, sizeof(decision));
    decision.action_code = 1;
    decision.confidence = 0.8f;  /* Above threshold */
    decision.expected_reward = 0.9f;
    decision.time_pressure = 0.5f;

    EXPECT_EQ(dragonfly_thalamic_relay_decision(bridge, &decision), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.decision_signals_relayed, 1u);
}

TEST_F(ThalamicBridgeTest, RelayDecisionGated) {
    dragonfly_thalamic_config_t config;
    dragonfly_thalamic_bridge_default_config(&config);
    config.enable_decision_gating = true;
    config.decision_threshold = 0.5f;

    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, &config);
    ASSERT_NE(bridge, nullptr);

    thal_decision_signal_t decision;
    memset(&decision, 0, sizeof(decision));
    decision.confidence = 0.3f;  /* Below threshold */

    EXPECT_EQ(dragonfly_thalamic_relay_decision(bridge, &decision), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.decision_signals_relayed, 0u);  /* Not relayed */
}

TEST_F(ThalamicBridgeTest, RelayTSDN) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    float tsdn_population[16] = {0};
    tsdn_population[4] = 0.8f;  /* Highest activation at index 4 */
    tsdn_population[3] = 0.5f;
    tsdn_population[5] = 0.5f;

    float heading = 0.785f;  /* ~45 degrees */
    float confidence = 0.75f;

    EXPECT_EQ(dragonfly_thalamic_relay_tsdn(bridge, tsdn_population, heading, confidence), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.visual_signals_relayed, 1u);
    EXPECT_GE(stats.attention_signals_relayed, 1u);
}

TEST_F(ThalamicBridgeTest, SignalGatedByInhibition) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Apply strong inhibition */
    dragonfly_thalamic_apply_inhibition(bridge, THAL_SIGNAL_POSITION, 0.95f);

    thal_visual_target_t target;
    memset(&target, 0, sizeof(target));
    target.contrast = 0.8f;

    EXPECT_EQ(dragonfly_thalamic_relay_visual(bridge, &target), 0);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.signals_gated, 1u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, HasDragonflyNoConnection) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(dragonfly_thalamic_has_dragonfly(bridge));
}

TEST_F(ThalamicBridgeTest, HasThalamusNoConnection) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(dragonfly_thalamic_has_thalamus(bridge));
}

TEST_F(ThalamicBridgeTest, UpdateNoSystems) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(dragonfly_thalamic_update(bridge), 0);
}

TEST_F(ThalamicBridgeTest, StepNoSystems) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(dragonfly_thalamic_step(bridge, 16.67f), 0);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, GetStats) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    thal_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_thalamic_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.visual_signals_relayed, 0u);
    EXPECT_EQ(stats.motor_signals_relayed, 0u);
    EXPECT_EQ(stats.attention_signals_relayed, 0u);
    EXPECT_EQ(stats.decision_signals_relayed, 0u);
}

TEST_F(ThalamicBridgeTest, ResetStats) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    /* Generate some stats */
    thal_visual_target_t target;
    memset(&target, 0, sizeof(target));
    target.contrast = 0.5f;
    dragonfly_thalamic_relay_visual(bridge, &target);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_signals_relayed, 1u);

    EXPECT_EQ(dragonfly_thalamic_bridge_reset_stats(bridge), 0);

    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_signals_relayed, 0u);
}

TEST_F(ThalamicBridgeTest, ModeSwitchCounted) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_TRACKING);
    dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_INTERCEPT);
    dragonfly_thalamic_set_mode(bridge, THAL_ROUTE_SUPPRESSED);

    thal_bridge_stats_t stats;
    dragonfly_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mode_switches, 3u);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(ThalamicBridgeTest, ModeNames) {
    EXPECT_STREQ(dragonfly_thalamic_mode_name(THAL_ROUTE_DISCOVERY), "discovery");
    EXPECT_STREQ(dragonfly_thalamic_mode_name(THAL_ROUTE_TRACKING), "tracking");
    EXPECT_STREQ(dragonfly_thalamic_mode_name(THAL_ROUTE_INTERCEPT), "intercept");
    EXPECT_STREQ(dragonfly_thalamic_mode_name(THAL_ROUTE_SUPPRESSED), "suppressed");
}

TEST_F(ThalamicBridgeTest, SignalNames) {
    EXPECT_STREQ(dragonfly_thalamic_signal_name(THAL_SIGNAL_POSITION), "position");
    EXPECT_STREQ(dragonfly_thalamic_signal_name(THAL_SIGNAL_VELOCITY), "velocity");
    EXPECT_STREQ(dragonfly_thalamic_signal_name(THAL_SIGNAL_SALIENCE), "salience");
    EXPECT_STREQ(dragonfly_thalamic_signal_name(THAL_SIGNAL_MOTOR_CMD), "motor");
    EXPECT_STREQ(dragonfly_thalamic_signal_name(THAL_SIGNAL_DECISION), "decision");
}

//=============================================================================
// Null Pointer Handling
//=============================================================================

TEST_F(ThalamicBridgeTest, NullBridgeHandling) {
    EXPECT_EQ(dragonfly_thalamic_bridge_default_config(nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_get_mode(nullptr), THAL_ROUTE_SUPPRESSED);
    EXPECT_EQ(dragonfly_thalamic_set_mode(nullptr, THAL_ROUTE_TRACKING), -1);
    EXPECT_EQ(dragonfly_thalamic_set_attention(nullptr, THAL_SIGNAL_POSITION, 0.5f), -1);
    EXPECT_EQ(dragonfly_thalamic_apply_inhibition(nullptr, THAL_SIGNAL_POSITION, 0.5f), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_visual(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_motor(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_attention(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_decision(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_tsdn(nullptr, nullptr, 0, 0), -1);
    EXPECT_EQ(dragonfly_thalamic_update(nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_step(nullptr, 16.67f), -1);
    EXPECT_EQ(dragonfly_thalamic_bridge_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_bridge_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_bridge_reset_stats(nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_connect_dragonfly(nullptr, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_connect_thalamus(nullptr, nullptr), -1);
    EXPECT_FALSE(dragonfly_thalamic_has_dragonfly(nullptr));
    EXPECT_FALSE(dragonfly_thalamic_has_thalamus(nullptr));
}

TEST_F(ThalamicBridgeTest, NullInputSignals) {
    bridge = dragonfly_thalamic_bridge_create(nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(dragonfly_thalamic_relay_visual(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_motor(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_attention(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_decision(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_thalamic_relay_tsdn(bridge, nullptr, 0, 0), -1);
}
