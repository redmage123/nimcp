/**
 * @file test_basal_ganglia_amygdala_bridge.cpp
 * @brief Unit tests for BG-amygdala emotional modulation bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_amygdala_bridge.h"

class BGAmygdalaBridgeTest : public ::testing::Test {
protected:
    bga_bridge_t* bridge = nullptr;
    basal_ganglia_t* bg = nullptr;
    amygdala_t* amygdala = nullptr;

    void SetUp() override {
        bga_bridge_config_t config;
        bga_bridge_default_config(&config);
        bridge = bga_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        bg = basal_ganglia_create(nullptr);
        amygdala = amygdala_create(nullptr);
    }

    void TearDown() override {
        if (bridge) bga_bridge_destroy(bridge);
        if (bg) basal_ganglia_destroy(bg);
        if (amygdala) amygdala_destroy(amygdala);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BGAmygdalaBridgeTest, CreateWithNullConfig) {
    bga_bridge_t* b = bga_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    bga_bridge_destroy(b);
}

TEST_F(BGAmygdalaBridgeTest, DefaultConfig) {
    bga_bridge_config_t config;
    bga_bridge_default_config(&config);

    EXPECT_FLOAT_EQ(config.fear_weight, BGA_DEFAULT_FEAR_WEIGHT);
    EXPECT_FLOAT_EQ(config.anxiety_weight, BGA_DEFAULT_ANXIETY_WEIGHT);
    EXPECT_FLOAT_EQ(config.freeze_threshold, BGA_FREEZE_THRESHOLD);
    EXPECT_TRUE(config.enable_freeze_response);
    EXPECT_TRUE(config.enable_flight_bias);
    EXPECT_TRUE(config.enable_feedback);
}

TEST_F(BGAmygdalaBridgeTest, Reset) {
    bga_bridge_tag_threat_action(bridge, 0);
    bga_bridge_tag_safe_action(bridge, 1);

    int ret = bga_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(bga_bridge_get_action_tag(bridge, 0), BGA_TAG_NEUTRAL);
    EXPECT_EQ(bga_bridge_get_action_tag(bridge, 1), BGA_TAG_NEUTRAL);
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, ConnectBG) {
    EXPECT_FALSE(bga_bridge_is_connected(bridge));

    int ret = bga_bridge_connect_bg(bridge, bg);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGAmygdalaBridgeTest, ConnectAmygdala) {
    int ret = bga_bridge_connect_amygdala(bridge, amygdala);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGAmygdalaBridgeTest, FullConnection) {
    bga_bridge_connect_bg(bridge, bg);
    bga_bridge_connect_amygdala(bridge, amygdala);

    EXPECT_TRUE(bga_bridge_is_connected(bridge));
}

// ============================================================================
// Action Tagging Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, TagThreatAction) {
    int ret = bga_bridge_tag_threat_action(bridge, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bga_bridge_get_action_tag(bridge, 0), BGA_TAG_THREATENING);
}

TEST_F(BGAmygdalaBridgeTest, TagSafeAction) {
    int ret = bga_bridge_tag_safe_action(bridge, 1);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bga_bridge_get_action_tag(bridge, 1), BGA_TAG_SAFE);
}

TEST_F(BGAmygdalaBridgeTest, TagEscapeAction) {
    int ret = bga_bridge_tag_escape_action(bridge, 2);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bga_bridge_get_action_tag(bridge, 2), BGA_TAG_ESCAPE);
}

TEST_F(BGAmygdalaBridgeTest, InvalidActionTag) {
    int ret = bga_bridge_tag_threat_action(bridge, 1000);
    EXPECT_EQ(ret, -1);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, UpdateModulation) {
    bga_bridge_connect_amygdala(bridge, amygdala);

    int ret = bga_bridge_update_modulation(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGAmygdalaBridgeTest, ApplyModulation) {
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    int ret = bga_bridge_apply_modulation(bridge, action_values, 8);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGAmygdalaBridgeTest, ThreatActionSuppression) {
    // Tag action 0 as threatening
    bga_bridge_tag_threat_action(bridge, 0);

    // Simulate high fear state
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.8f);
    bga_bridge_update_modulation(bridge);

    // Apply modulation
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float original_threat = action_values[0];
    float original_neutral = action_values[3];

    bga_bridge_apply_modulation(bridge, action_values, 8);

    // Threatening action should be suppressed more
    EXPECT_LT(action_values[0], original_threat);
}

TEST_F(BGAmygdalaBridgeTest, SafeActionBoost) {
    // Tag action 1 as safe
    bga_bridge_tag_safe_action(bridge, 1);

    // Simulate anxiety
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_anxiety(amygdala, 0.7f);
    bga_bridge_update_modulation(bridge);

    float modulation = bga_bridge_get_action_modulation(bridge, 1);
    EXPECT_GT(modulation, 1.0f);  // Safe actions get boosted
}

TEST_F(BGAmygdalaBridgeTest, FreezeResponse) {
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.9f);  // Above freeze threshold
    bga_bridge_update_modulation(bridge);

    EXPECT_TRUE(bga_bridge_is_frozen(bridge));
}

TEST_F(BGAmygdalaBridgeTest, NoFreezeWhenLowFear) {
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.3f);  // Below freeze threshold
    bga_bridge_update_modulation(bridge);

    EXPECT_FALSE(bga_bridge_is_frozen(bridge));
}

TEST_F(BGAmygdalaBridgeTest, STNBoostFromAnxiety) {
    bga_bridge_connect_amygdala(bridge, amygdala);

    amygdala_set_anxiety(amygdala, 0.0f);
    bga_bridge_update_modulation(bridge);
    float low_anxiety_boost = bga_bridge_get_stn_boost(bridge);

    amygdala_set_anxiety(amygdala, 0.8f);
    bga_bridge_update_modulation(bridge);
    float high_anxiety_boost = bga_bridge_get_stn_boost(bridge);

    EXPECT_GT(high_anxiety_boost, low_anxiety_boost);
}

// ============================================================================
// State Query Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, GetEmotionalState) {
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.5f);
    amygdala_set_anxiety(amygdala, 0.3f);
    bga_bridge_update_modulation(bridge);

    bga_emotional_state_t state;
    int ret = bga_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(state.fear_level, 0.5f);
    EXPECT_FLOAT_EQ(state.anxiety_level, 0.3f);
}

TEST_F(BGAmygdalaBridgeTest, GetInfluenceType) {
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.0f);
    bga_bridge_update_modulation(bridge);

    EXPECT_EQ(bga_bridge_get_influence(bridge), BGA_INFLUENCE_NONE);
}

// ============================================================================
// Feedback Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, SendOutcome) {
    bga_bridge_connect_amygdala(bridge, amygdala);
    amygdala_set_fear_level(amygdala, 0.5f);

    int ret = bga_bridge_send_outcome(bridge, 0, 1.0f, true);
    EXPECT_EQ(ret, 0);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, Statistics) {
    float action_values[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    for (int i = 0; i < 5; i++) {
        bga_bridge_update_modulation(bridge);
        bga_bridge_apply_modulation(bridge, action_values, 8);
    }

    bga_bridge_stats_t stats;
    int ret = bga_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_modulations, 5u);
}

TEST_F(BGAmygdalaBridgeTest, ResetStats) {
    bga_bridge_update_modulation(bridge);

    bga_bridge_reset_stats(bridge);

    bga_bridge_stats_t stats;
    bga_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_modulations, 0u);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, InfluenceTypeNames) {
    EXPECT_STREQ(bga_influence_type_name(BGA_INFLUENCE_NONE), "none");
    EXPECT_STREQ(bga_influence_type_name(BGA_INFLUENCE_AVOIDANCE), "avoidance");
    EXPECT_STREQ(bga_influence_type_name(BGA_INFLUENCE_FREEZE), "freeze");
    EXPECT_STREQ(bga_influence_type_name(BGA_INFLUENCE_FLIGHT), "flight");
    EXPECT_STREQ(bga_influence_type_name(BGA_INFLUENCE_FIGHT), "fight");
}

TEST_F(BGAmygdalaBridgeTest, ActionTagNames) {
    EXPECT_STREQ(bga_action_tag_name(BGA_TAG_NEUTRAL), "neutral");
    EXPECT_STREQ(bga_action_tag_name(BGA_TAG_THREATENING), "threatening");
    EXPECT_STREQ(bga_action_tag_name(BGA_TAG_SAFE), "safe");
    EXPECT_STREQ(bga_action_tag_name(BGA_TAG_ESCAPE), "escape");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BGAmygdalaBridgeTest, NullBridge) {
    EXPECT_EQ(bga_bridge_reset(nullptr), -1);
    EXPECT_EQ(bga_bridge_update_modulation(nullptr), -1);
    EXPECT_FALSE(bga_bridge_is_connected(nullptr));
}

TEST_F(BGAmygdalaBridgeTest, NullActionValues) {
    int ret = bga_bridge_apply_modulation(bridge, nullptr, 8);
    EXPECT_EQ(ret, -1);
}
