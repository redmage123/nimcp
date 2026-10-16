//=============================================================================
// test_basal_ganglia_thalamus_bridge_integration.cpp - BG-Thalamus Bridge Integration Tests
//=============================================================================
/**
 * @file test_basal_ganglia_thalamus_bridge_integration.cpp
 * @brief Integration tests for basal ganglia-thalamus motor relay bridge
 *
 * WHAT: Tests bridge creation, lifecycle, channel mapping, motor relay,
 *       attention/urgency modulation, TRN gating, and statistics tracking
 * WHY:  Verify correct BG disinhibition routing through thalamic VA/VL nuclei
 * HOW:  GTest framework testing cross-module bridge integration
 *
 * BIOLOGICAL BASIS:
 * - GPi/SNr tonically inhibit thalamic VA/VL nuclei
 * - Action selection disinhibits specific thalamic channels
 * - VA receives BG input, VL receives cerebellar input
 * - Attention and arousal modulate thalamic gating
 * - TRN provides additional inhibitory control
 *
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/brain/subcortical/nimcp_basal_ganglia_thalamus_bridge.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "core/brain/subcortical/nimcp_thalamus.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class BGTBridgeIntegrationTest : public NimcpTestBase {
protected:
    bgt_bridge_t* bridge = nullptr;
    basal_ganglia_t* bg = nullptr;
    thalamus_t* thal = nullptr;
    bgt_bridge_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create basal ganglia
        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 8;
        bg = basal_ganglia_create(&bg_config);

        // Create thalamus
        thalamus_config_t thal_config;
        thalamus_default_config(&thal_config);
        thal_config.neurons_per_nucleus = 32;
        thal_config.channels_per_nucleus = 16;
        thal = thalamus_create(&thal_config);

        // Create bridge with default config
        bgt_bridge_default_config(&config);
        config.num_channels = 8;
        bridge = bgt_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            bgt_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
        if (thal) {
            thalamus_destroy(thal);
            thal = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    bool FloatNear(float a, float b, float eps = 0.001f) {
        return std::fabs(a - b) < eps;
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, CreateWithDefaultConfig) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->num_channels, 8u);
}

TEST_F(BGTBridgeIntegrationTest, CreateWithNullConfigUsesDefaults) {
    bgt_bridge_t* test_bridge = bgt_bridge_create(nullptr);
    EXPECT_NE(test_bridge, nullptr);
    if (test_bridge) {
        bgt_bridge_destroy(test_bridge);
    }
}

TEST_F(BGTBridgeIntegrationTest, CreateWithCustomConfig) {
    bgt_bridge_config_t custom_config;
    bgt_bridge_default_config(&custom_config);
    custom_config.num_channels = 16;
    custom_config.relay_gain = 1.5f;
    custom_config.disinhibition_threshold = 0.4f;
    custom_config.enable_attention_gating = true;
    custom_config.enable_urgency_boost = true;

    bgt_bridge_t* custom_bridge = bgt_bridge_create(&custom_config);
    ASSERT_NE(custom_bridge, nullptr);
    EXPECT_EQ(custom_bridge->num_channels, 16u);
    bgt_bridge_destroy(custom_bridge);
}

TEST_F(BGTBridgeIntegrationTest, DestroyNullSafe) {
    bgt_bridge_destroy(nullptr);
    // Should not crash
}

TEST_F(BGTBridgeIntegrationTest, ResetRestoresInitialState) {
    ASSERT_NE(bridge, nullptr);

    // Connect and modify state
    bgt_bridge_connect_bg(bridge, bg);
    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_set_attention(bridge, 0.9f);
    bgt_bridge_set_urgency(bridge, 0.8f);

    // Reset
    int result = bgt_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify reset state
    EXPECT_FLOAT_EQ(bridge->current_attention, BGT_DEFAULT_ATTENTION);
    EXPECT_FLOAT_EQ(bridge->current_urgency, 0.0f);
}

TEST_F(BGTBridgeIntegrationTest, ResetNullBridgeFails) {
    int result = bgt_bridge_reset(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, ConnectBGSucceeds) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(bg, nullptr);

    int result = bgt_bridge_connect_bg(bridge, bg);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->bg, bg);
}

TEST_F(BGTBridgeIntegrationTest, ConnectThalamusSucceeds) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    int result = bgt_bridge_connect_thalamus(bridge, thal);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bridge->thalamus, thal);
}

TEST_F(BGTBridgeIntegrationTest, IsConnectedFalseInitially) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(bgt_bridge_is_connected(bridge));
}

TEST_F(BGTBridgeIntegrationTest, IsConnectedTrueWhenBothConnected) {
    ASSERT_NE(bridge, nullptr);

    bgt_bridge_connect_bg(bridge, bg);
    EXPECT_FALSE(bgt_bridge_is_connected(bridge));

    bgt_bridge_connect_thalamus(bridge, thal);
    EXPECT_TRUE(bgt_bridge_is_connected(bridge));
}

TEST_F(BGTBridgeIntegrationTest, ConnectNullBridgeFails) {
    EXPECT_EQ(bgt_bridge_connect_bg(nullptr, bg), -1);
    EXPECT_EQ(bgt_bridge_connect_thalamus(nullptr, thal), -1);
}

TEST_F(BGTBridgeIntegrationTest, ConnectNullComponentFails) {
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bgt_bridge_connect_bg(bridge, nullptr), -1);
    EXPECT_EQ(bgt_bridge_connect_thalamus(bridge, nullptr), -1);
}

//=============================================================================
// Channel Mapping Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, SetChannelMapSucceeds) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_set_channel_map(bridge, 0, 0, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(BGTBridgeIntegrationTest, SetChannelMapWithCustomWeight) {
    ASSERT_NE(bridge, nullptr);

    bgt_bridge_set_channel_map(bridge, 0, 0, 0.75f);

    float weight = bgt_bridge_get_channel_weight(bridge, 0);
    EXPECT_FLOAT_EQ(weight, 0.75f);
}

TEST_F(BGTBridgeIntegrationTest, CreateDefaultMappingOneToOne) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_create_default_mapping(bridge);
    EXPECT_EQ(result, 0);

    // Verify one-to-one mapping with weight 1.0
    for (uint32_t i = 0; i < bridge->num_channels; i++) {
        float weight = bgt_bridge_get_channel_weight(bridge, i);
        EXPECT_FLOAT_EQ(weight, 1.0f);
    }
}

TEST_F(BGTBridgeIntegrationTest, GetChannelWeightInvalidAction) {
    ASSERT_NE(bridge, nullptr);

    float weight = bgt_bridge_get_channel_weight(bridge, 999);
    EXPECT_LT(weight, 0.0f);  // Returns -1 on error
}

TEST_F(BGTBridgeIntegrationTest, SetChannelMapNullBridgeFails) {
    int result = bgt_bridge_set_channel_map(nullptr, 0, 0, 1.0f);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, SetAttentionSucceeds) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_set_attention(bridge, 0.8f);
    EXPECT_EQ(result, 0);

    float attention = bgt_bridge_get_attention(bridge);
    EXPECT_FLOAT_EQ(attention, 0.8f);
}

TEST_F(BGTBridgeIntegrationTest, SetUrgencySucceeds) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_set_urgency(bridge, 0.7f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->current_urgency, 0.7f);
}

TEST_F(BGTBridgeIntegrationTest, SetTRNInhibitionSucceeds) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_set_trn_inhibition(bridge, 0.6f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(bridge->trn_inhibition, 0.6f);
}

TEST_F(BGTBridgeIntegrationTest, SetModeSucceeds) {
    ASSERT_NE(bridge, nullptr);

    int result = bgt_bridge_set_mode(bridge, BGT_MODE_URGENT);
    EXPECT_EQ(result, 0);

    bgt_relay_mode_t mode = bgt_bridge_get_mode(bridge);
    EXPECT_EQ(mode, BGT_MODE_URGENT);
}

TEST_F(BGTBridgeIntegrationTest, AttentionClampedToRange) {
    ASSERT_NE(bridge, nullptr);

    bgt_bridge_set_attention(bridge, 1.5f);
    EXPECT_LE(bgt_bridge_get_attention(bridge), 1.0f);

    bgt_bridge_set_attention(bridge, -0.5f);
    EXPECT_GE(bgt_bridge_get_attention(bridge), 0.0f);
}

TEST_F(BGTBridgeIntegrationTest, ModulationNullBridgeFails) {
    EXPECT_EQ(bgt_bridge_set_attention(nullptr, 0.5f), -1);
    EXPECT_EQ(bgt_bridge_set_urgency(nullptr, 0.5f), -1);
    EXPECT_EQ(bgt_bridge_set_trn_inhibition(nullptr, 0.5f), -1);
    EXPECT_EQ(bgt_bridge_set_mode(nullptr, BGT_MODE_NORMAL), -1);
}

//=============================================================================
// Motor Relay Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, RelayExplicitSucceeds) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_create_default_mapping(bridge);

    // Create BG output (disinhibition signals)
    float bg_output[] = {0.8f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};

    bgt_relay_result_t result;
    result.motor_output = new float[8];
    result.output_size = 8;

    int status = bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
    EXPECT_EQ(status, 0);

    // Check that output was generated
    float sum = 0.0f;
    for (int i = 0; i < 8; i++) {
        sum += result.motor_output[i];
    }
    EXPECT_GT(sum, 0.0f);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, GetActionOutputSucceeds) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_create_default_mapping(bridge);

    // Relay some data
    float bg_output[] = {0.9f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};
    bgt_relay_result_t result;
    result.motor_output = new float[8];
    result.output_size = 8;

    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);

    float action_output = bgt_bridge_get_action_output(bridge, 0);
    EXPECT_GE(action_output, 0.0f);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, GetActionOutputInvalidActionFails) {
    ASSERT_NE(bridge, nullptr);

    float output = bgt_bridge_get_action_output(bridge, 999);
    EXPECT_LT(output, 0.0f);  // Returns -1 on error
}

TEST_F(BGTBridgeIntegrationTest, RelayExplicitNullBridgeFails) {
    float bg_output[] = {0.5f};
    bgt_relay_result_t result;
    result.motor_output = new float[1];
    result.output_size = 1;

    int status = bgt_bridge_relay_explicit(nullptr, bg_output, 1, &result);
    EXPECT_EQ(status, -1);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, RelayExplicitNullOutputFails) {
    ASSERT_NE(bridge, nullptr);

    float bg_output[] = {0.5f};
    int status = bgt_bridge_relay_explicit(bridge, bg_output, 1, nullptr);
    EXPECT_EQ(status, -1);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, GetStatsSucceeds) {
    ASSERT_NE(bridge, nullptr);

    bgt_bridge_stats_t stats;
    int result = bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_relays, 0u);
}

TEST_F(BGTBridgeIntegrationTest, StatsTrackRelays) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_create_default_mapping(bridge);

    // Perform relays
    float bg_output[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;
    result.motor_output = new float[8];
    result.output_size = 8;

    for (int i = 0; i < 5; i++) {
        bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
    }

    bgt_bridge_stats_t stats;
    bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_relays, 5u);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, ResetStatsClears) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_create_default_mapping(bridge);

    // Perform relay
    float bg_output[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;
    result.motor_output = new float[8];
    result.output_size = 8;
    bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);

    // Reset stats
    bgt_bridge_reset_stats(bridge);

    bgt_bridge_stats_t stats;
    bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_relays, 0u);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, GetStatsNullBridgeFails) {
    bgt_bridge_stats_t stats;
    int result = bgt_bridge_get_stats(nullptr, &stats);
    EXPECT_EQ(result, -1);
}

TEST_F(BGTBridgeIntegrationTest, GetStatsNullOutputFails) {
    ASSERT_NE(bridge, nullptr);
    int result = bgt_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Mode and Type Name Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, RelayModeNamesCorrect) {
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_NORMAL), "Normal");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_URGENT), "Urgent");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_SUPPRESSED), "Suppressed");
    EXPECT_STREQ(bgt_relay_mode_name(BGT_MODE_BURST), "Burst");
}

TEST_F(BGTBridgeIntegrationTest, OutputTypeNamesCorrect) {
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_DISCRETE), "Discrete");
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_CONTINUOUS), "Continuous");
    EXPECT_STREQ(bgt_output_type_name(BGT_OUTPUT_VELOCITY), "Velocity");
}

//=============================================================================
// Stability Tests
//=============================================================================

TEST_F(BGTBridgeIntegrationTest, ManyRelaysStable) {
    ASSERT_NE(bridge, nullptr);
    ASSERT_NE(thal, nullptr);

    bgt_bridge_connect_thalamus(bridge, thal);
    bgt_bridge_create_default_mapping(bridge);

    float bg_output[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    bgt_relay_result_t result;
    result.motor_output = new float[8];
    result.output_size = 8;

    // Run many relays
    for (int i = 0; i < 1000; i++) {
        int status = bgt_bridge_relay_explicit(bridge, bg_output, 8, &result);
        EXPECT_EQ(status, 0);
    }

    // Bridge should remain stable
    EXPECT_NE(bridge, nullptr);

    bgt_bridge_stats_t stats;
    bgt_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_relays, 1000u);

    delete[] result.motor_output;
}

TEST_F(BGTBridgeIntegrationTest, ModulationCyclesStable) {
    ASSERT_NE(bridge, nullptr);

    // Cycle through various modulation settings
    for (int i = 0; i < 100; i++) {
        float val = (float)(i % 10) / 10.0f;
        bgt_bridge_set_attention(bridge, val);
        bgt_bridge_set_urgency(bridge, val);
        bgt_bridge_set_trn_inhibition(bridge, val);
    }

    // Bridge should remain stable
    EXPECT_NE(bridge, nullptr);
}
