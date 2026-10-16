/**
 * @file test_basal_ganglia_thalamus_bridge.cpp
 * @brief Unit tests for BG-thalamus motor relay bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_thalamus_bridge.h"

class BGThalamBridgeTest : public ::testing::Test {
protected:
    bgt_bridge_t* bridge = nullptr;
    basal_ganglia_t* bg = nullptr;
    thalamus_t* thalamus = nullptr;

    void SetUp() override {
        bgt_bridge_config_t config;
        bgt_bridge_default_config(&config);
        config.num_channels = 8;
        bridge = bgt_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        // Create BG and thalamus for integration tests
        bg = basal_ganglia_create(nullptr);
        thalamus = thalamus_create(nullptr);
    }

    void TearDown() override {
        if (bridge) bgt_bridge_destroy(bridge);
        if (bg) basal_ganglia_destroy(bg);
        if (thalamus) thalamus_destroy(thalamus);
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BGThalamBridgeTest, CreateWithNullConfig) {
    bgt_bridge_t* b = bgt_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    bgt_bridge_destroy(b);
}

TEST_F(BGThalamBridgeTest, DefaultConfig) {
    bgt_bridge_config_t config;
    bgt_bridge_default_config(&config);

    EXPECT_EQ(config.num_channels, 16);
    EXPECT_FLOAT_EQ(config.relay_gain, BGT_DEFAULT_GAIN);
    EXPECT_FLOAT_EQ(config.disinhibition_threshold, BGT_DEFAULT_THRESHOLD);
    EXPECT_TRUE(config.enable_attention_gating);
    EXPECT_TRUE(config.enable_urgency_boost);
}

TEST_F(BGThalamBridgeTest, Reset) {
    bgt_bridge_set_attention(bridge, 0.9f);
    bgt_bridge_set_urgency(bridge, 0.5f);

    int ret = bgt_bridge_reset(bridge);
    EXPECT_EQ(ret, 0);

    EXPECT_FLOAT_EQ(bgt_bridge_get_attention(bridge), BGT_DEFAULT_ATTENTION);
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, ConnectBG) {
    EXPECT_FALSE(bgt_bridge_is_connected(bridge));

    int ret = bgt_bridge_connect_bg(bridge, bg);
    EXPECT_EQ(ret, 0);

    // Still not fully connected (no thalamus)
    EXPECT_FALSE(bgt_bridge_is_connected(bridge));
}

TEST_F(BGThalamBridgeTest, ConnectThalamus) {
    int ret = bgt_bridge_connect_thalamus(bridge, thalamus);
    EXPECT_EQ(ret, 0);
}

TEST_F(BGThalamBridgeTest, FullConnection) {
    bgt_bridge_connect_bg(bridge, bg);
    bgt_bridge_connect_thalamus(bridge, thalamus);

    EXPECT_TRUE(bgt_bridge_is_connected(bridge));
}

// ============================================================================
// Channel Mapping Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, SetChannelMap) {
    int ret = bgt_bridge_set_channel_map(bridge, 0, 0, 0.8f);
    EXPECT_EQ(ret, 0);

    float weight = bgt_bridge_get_channel_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.8f);
}

TEST_F(BGThalamBridgeTest, InvalidChannelMap) {
    int ret = bgt_bridge_set_channel_map(bridge, 100, 0, 0.5f);
    EXPECT_EQ(ret, -1);
}

TEST_F(BGThalamBridgeTest, DefaultMapping) {
    int ret = bgt_bridge_create_default_mapping(bridge);
    EXPECT_EQ(ret, 0);

    // Check all channels have weight 1.0
    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(bgt_bridge_get_channel_weight(bridge, i), 1.0f);
    }
}

// ============================================================================
// Relay Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, RelayExplicit) {
    float bg_output[8] = {0.1f, 0.2f, 0.8f, 0.3f, 0.1f, 0.2f, 0.1f, 0.1f};
    bgt_relay_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
    EXPECT_EQ(ret, 0);

    EXPECT_NE(result.motor_output, nullptr);
    EXPECT_EQ(result.output_size, 8u);
    EXPECT_EQ(result.selected_action, 2u);  // Index 2 has highest value
    EXPECT_GT(result.selection_confidence, 0.0f);
}

TEST_F(BGThalamBridgeTest, RelaySuppressed) {
    bgt_bridge_set_mode(bridge, BGT_MODE_SUPPRESSED);

    float bg_output[8] = {0.5f, 0.5f, 0.9f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;

    int ret = bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
    EXPECT_EQ(ret, 0);

    // All outputs should be zero in suppressed mode
    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_FLOAT_EQ(result.motor_output[i], 0.0f);
    }
    EXPECT_EQ(result.mode, BGT_MODE_SUPPRESSED);
}

TEST_F(BGThalamBridgeTest, AttentionModulation) {
    float bg_output[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result1, result2;

    // Low attention
    bgt_bridge_set_attention(bridge, 0.2f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result1);
    float low_att_output = result1.motor_output[0];

    // High attention
    bgt_bridge_reset(bridge);
    bgt_bridge_set_attention(bridge, 0.9f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result2);
    float high_att_output = result2.motor_output[0];

    // Higher attention should give higher output
    EXPECT_GT(high_att_output, low_att_output);
}

TEST_F(BGThalamBridgeTest, UrgencyBoost) {
    float bg_output[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result1, result2;

    // No urgency
    bgt_bridge_set_urgency(bridge, 0.0f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result1);
    float no_urgency = result1.motor_output[0];

    // High urgency
    bgt_bridge_reset(bridge);
    bgt_bridge_set_urgency(bridge, 1.0f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result2);
    float high_urgency = result2.motor_output[0];

    // Higher urgency should boost output
    EXPECT_GT(high_urgency, no_urgency);
}

TEST_F(BGThalamBridgeTest, TRNInhibition) {
    float bg_output[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result1, result2;

    // No TRN inhibition
    bgt_bridge_set_trn_inhibition(bridge, 0.0f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result1);
    float no_trn = result1.motor_output[0];

    // Strong TRN inhibition
    bgt_bridge_reset(bridge);
    bgt_bridge_set_trn_inhibition(bridge, 0.8f);
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result2);
    float high_trn = result2.motor_output[0];

    // TRN should suppress output
    EXPECT_LT(high_trn, no_trn);
}

// ============================================================================
// Modulation Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, SetGetAttention) {
    int ret = bgt_bridge_set_attention(bridge, 0.7f);
    EXPECT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(bgt_bridge_get_attention(bridge), 0.7f);
}

TEST_F(BGThalamBridgeTest, AttentionClamping) {
    bgt_bridge_set_attention(bridge, 1.5f);
    EXPECT_FLOAT_EQ(bgt_bridge_get_attention(bridge), 1.0f);

    bgt_bridge_set_attention(bridge, -0.5f);
    EXPECT_FLOAT_EQ(bgt_bridge_get_attention(bridge), 0.0f);
}

TEST_F(BGThalamBridgeTest, SetGetMode) {
    int ret = bgt_bridge_set_mode(bridge, BGT_MODE_URGENT);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bgt_bridge_get_mode(bridge), BGT_MODE_URGENT);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, Statistics) {
    float bg_output[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;

    // Perform some relays
    for (int i = 0; i < 10; i++) {
        bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
    }

    bgt_bridge_stats_t stats;
    int ret = bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_relays, 10u);
}

TEST_F(BGThalamBridgeTest, ResetStats) {
    float bg_output[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;

    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);

    bgt_bridge_reset_stats(bridge);

    bgt_bridge_stats_t stats;
    bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_relays, 0u);
}

// ============================================================================
// Utility Tests
// ============================================================================

TEST_F(BGThalamBridgeTest, ModeNames) {
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_NORMAL), "normal");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_URGENT), "urgent");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_SUPPRESSED), "suppressed");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_BURST), "burst");
}

TEST_F(BGThalamBridgeTest, OutputTypeNames) {
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_DISCRETE), "discrete");
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_CONTINUOUS), "continuous");
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_VELOCITY), "velocity");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(BGThalamBridgeTest, NullBridge) {
    EXPECT_EQ(bgt_bridge_reset(nullptr), -1);
    EXPECT_EQ(bgt_bridge_connect_bg(nullptr, bg), -1);
    EXPECT_FALSE(bgt_bridge_is_connected(nullptr));
}

TEST_F(BGThalamBridgeTest, GetActionOutputInvalid) {
    float output = bgt_bridge_get_action_output(bridge, 100);
    EXPECT_LT(output, 0.0f);
}
