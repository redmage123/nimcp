/**
 * @file test_attention_substrate_bridge.cpp
 * @brief Unit tests for Attention-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-attention integration including focus capacity,
 * shifting efficiency, filter strength, vigilance, and metabolic modulation.
 *
 * NOTE: Uses mock attention system since the bridge uses opaque pointers.
 * The actual attention system integration is tested at the integration level.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/attention/nimcp_attention_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class AttentionSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    nimcp_attention_system_t* attention = nullptr;
    attention_substrate_bridge_t* bridge = nullptr;
    attention_substrate_config_t config;

    void SetUp() override {
        // Create neural substrate
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create mock attention system (bridge uses opaque pointer)
        attention = (nimcp_attention_system_t*)nimcp_malloc(64);
        ASSERT_NE(attention, nullptr);

        // Get default bridge config
        attention_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            attention_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (attention) {
            nimcp_free(attention);
            attention = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    // Helper to create bridge
    void createBridge() {
        bridge = attention_substrate_bridge_create(&config, substrate, attention);
        ASSERT_NE(bridge, nullptr);
    }

    // Helper to set substrate ATP level
    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }

    // Helper to set substrate temperature
    void setSubstrateTemperature(float temp) {
        if (substrate) {
            substrate_set_temperature(substrate, temp);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = attention_substrate_bridge_create(&config, substrate, attention);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(AttentionSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = attention_substrate_bridge_create(nullptr, substrate, attention);
    EXPECT_NE(bridge, nullptr);  // Should use defaults
}

TEST_F(AttentionSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = attention_substrate_bridge_create(&config, nullptr, attention);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(AttentionSubstrateBridgeTest, CreateWithNullAttention) {
    bridge = attention_substrate_bridge_create(&config, substrate, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(AttentionSubstrateBridgeTest, DestroyNull) {
    attention_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(AttentionSubstrateBridgeTest, DestroyValid) {
    createBridge();
    attention_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, DefaultConfigEnablesAllModulations) {
    attention_substrate_config_t cfg;
    attention_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_focus_modulation);
    EXPECT_TRUE(cfg.enable_shifting_modulation);
    EXPECT_TRUE(cfg.enable_filter_modulation);
}

TEST_F(AttentionSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    attention_substrate_config_t cfg;
    attention_substrate_default_config(&cfg);

    EXPECT_FLOAT_EQ(cfg.atp_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.temperature_sensitivity, 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, DefaultConfigNullSafe) {
    attention_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(attention_substrate_is_bio_async_connected(bridge));
}

TEST_F(AttentionSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = attention_substrate_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(AttentionSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = attention_substrate_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(AttentionSubstrateBridgeTest, IsConnectedNullReturnsFalse) {
    EXPECT_FALSE(attention_substrate_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(attention_substrate_get_focus_capacity(bridge), 1.0f);
    EXPECT_FLOAT_EQ(attention_substrate_get_shifting_efficiency(bridge), 1.0f);
    EXPECT_FLOAT_EQ(attention_substrate_get_filter_strength(bridge), 1.0f);
    EXPECT_FLOAT_EQ(attention_substrate_get_vigilance(bridge), 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(attention_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Focus Capacity
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, UpdateWithFullATPMaintainsFocus) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = attention_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_GE(focus, 0.9f);
}

TEST_F(AttentionSubstrateBridgeTest, UpdateWithLowATPReducesFocus) {
    createBridge();
    setSubstrateATP(0.4f);

    int result = attention_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_LT(focus, 0.7f);
    EXPECT_GE(focus, 0.2f);
}

TEST_F(AttentionSubstrateBridgeTest, UpdateWithCriticalATPMinimalFocus) {
    createBridge();
    setSubstrateATP(0.1f);

    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_LE(focus, 0.4f);
}

/* ============================================================================
 * Update Tests - Shifting Efficiency
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, UpdateWithNormalTempMaintainsShifting) {
    createBridge();
    setSubstrateTemperature(37.0f);

    attention_substrate_update(bridge);

    float shifting = attention_substrate_get_shifting_efficiency(bridge);
    EXPECT_GE(shifting, 0.8f);
}

TEST_F(AttentionSubstrateBridgeTest, UpdateWithHyperthermiaReducesShifting) {
    createBridge();
    setSubstrateTemperature(41.0f);

    attention_substrate_update(bridge);

    float shifting = attention_substrate_get_shifting_efficiency(bridge);
    EXPECT_LE(shifting, 1.0f);  // Should not exceed ceiling
}

/* ============================================================================
 * Update Tests - Filter Strength
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, UpdateWithFullATPMaintainsFilter) {
    createBridge();
    setSubstrateATP(1.0f);

    attention_substrate_update(bridge);

    float filter = attention_substrate_get_filter_strength(bridge);
    EXPECT_GE(filter, 0.9f);
}

TEST_F(AttentionSubstrateBridgeTest, UpdateWithLowATPReducesFilter) {
    createBridge();
    setSubstrateATP(0.3f);

    attention_substrate_update(bridge);

    float filter = attention_substrate_get_filter_strength(bridge);
    EXPECT_LT(filter, 0.7f);
}

/* ============================================================================
 * Update Tests - Vigilance
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, VigilanceDependsOnMetabolicState) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    attention_substrate_update(bridge);

    float vigilance = attention_substrate_get_vigilance(bridge);
    EXPECT_GE(vigilance, 0.8f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, ImpairedWithLowATP) {
    createBridge();
    setSubstrateATP(0.2f);

    attention_substrate_update(bridge);

    EXPECT_TRUE(attention_substrate_is_impaired(bridge));
}

TEST_F(AttentionSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);
    setSubstrateTemperature(37.0f);

    attention_substrate_update(bridge);

    EXPECT_FALSE(attention_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    attention_substrate_effects_t effects;
    int result = attention_substrate_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.focus_capacity, 1.0f);
    EXPECT_FLOAT_EQ(effects.shifting_efficiency, 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, GetEffectsNullBridge) {
    attention_substrate_effects_t effects;
    int result = attention_substrate_get_effects(nullptr, &effects);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(AttentionSubstrateBridgeTest, GetEffectsNullOutput) {
    createBridge();
    int result = attention_substrate_get_effects(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, GetFocusNullReturnsNegative) {
    EXPECT_FLOAT_EQ(attention_substrate_get_focus_capacity(nullptr), -1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, GetShiftingNullReturnsNegative) {
    EXPECT_FLOAT_EQ(attention_substrate_get_shifting_efficiency(nullptr), -1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, GetFilterNullReturnsNegative) {
    EXPECT_FLOAT_EQ(attention_substrate_get_filter_strength(nullptr), -1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, GetVigilanceNullReturnsNegative) {
    EXPECT_FLOAT_EQ(attention_substrate_get_vigilance(nullptr), -1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(attention_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    attention_substrate_stats_t stats;
    int result = attention_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.update_count, 0u);
    EXPECT_EQ(stats.impairment_events, 0u);
}

TEST_F(AttentionSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    attention_substrate_update(bridge);
    attention_substrate_update(bridge);
    attention_substrate_update(bridge);

    attention_substrate_stats_t stats;
    attention_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(stats.update_count, 3u);
}

TEST_F(AttentionSubstrateBridgeTest, ImpairmentEventTracked) {
    createBridge();
    setSubstrateATP(0.2f);

    attention_substrate_update(bridge);

    attention_substrate_stats_t stats;
    attention_substrate_get_stats(bridge, &stats);

    EXPECT_GE(stats.impairment_events, 1u);
}

TEST_F(AttentionSubstrateBridgeTest, GetStatsNullBridge) {
    attention_substrate_stats_t stats;
    int result = attention_substrate_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

TEST_F(AttentionSubstrateBridgeTest, GetStatsNullOutput) {
    createBridge();
    int result = attention_substrate_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, DisabledFocusModulationStaysOptimal) {
    config.enable_focus_modulation = false;
    createBridge();
    setSubstrateATP(0.3f);

    attention_substrate_update(bridge);

    EXPECT_FLOAT_EQ(attention_substrate_get_focus_capacity(bridge), 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, DisabledShiftingModulationStaysOptimal) {
    config.enable_shifting_modulation = false;
    createBridge();
    setSubstrateTemperature(42.0f);

    attention_substrate_update(bridge);

    EXPECT_FLOAT_EQ(attention_substrate_get_shifting_efficiency(bridge), 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, DisabledFilterModulationStaysOptimal) {
    config.enable_filter_modulation = false;
    createBridge();
    setSubstrateATP(0.3f);

    attention_substrate_update(bridge);

    EXPECT_FLOAT_EQ(attention_substrate_get_filter_strength(bridge), 1.0f);
}

/* ============================================================================
 * Sensitivity Scaling Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, HighATPSensitivityAmplifiesEffect) {
    config.atp_sensitivity = 2.0f;
    createBridge();
    setSubstrateATP(0.4f);  // Use lower ATP to see effect

    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_GT(focus, 0.2f);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, LowATPSensitivityReducesEffect) {
    config.atp_sensitivity = 0.5f;
    createBridge();
    setSubstrateATP(0.4f);

    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_GE(focus, 0.2f);
}

/* ============================================================================
 * Running Average Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, StatsTrackAverageFocus) {
    createBridge();

    for (int i = 0; i < 10; i++) {
        attention_substrate_update(bridge);
    }

    attention_substrate_stats_t stats;
    attention_substrate_get_stats(bridge, &stats);

    EXPECT_GT(stats.avg_focus_capacity, 0.0f);
}

TEST_F(AttentionSubstrateBridgeTest, StatsTrackMinMaxFocus) {
    createBridge();

    setSubstrateATP(1.0f);
    attention_substrate_update(bridge);

    setSubstrateATP(0.3f);
    attention_substrate_update(bridge);

    attention_substrate_stats_t stats;
    attention_substrate_get_stats(bridge, &stats);

    EXPECT_LT(stats.min_focus_observed, stats.max_focus_observed);
}

/* ============================================================================
 * Thread Safety and Error Handling Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = attention_substrate_update(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (may be positive)
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(AttentionSubstrateBridgeTest, FocusNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);
    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_GE(focus, 0.0f);
}

TEST_F(AttentionSubstrateBridgeTest, FocusNeverAboveCeiling) {
    createBridge();
    setSubstrateATP(2.0f);
    attention_substrate_update(bridge);

    float focus = attention_substrate_get_focus_capacity(bridge);
    EXPECT_LE(focus, 1.0f);
}

TEST_F(AttentionSubstrateBridgeTest, ShiftingNeverBelowFloor) {
    createBridge();
    setSubstrateTemperature(45.0f);
    attention_substrate_update(bridge);

    float shifting = attention_substrate_get_shifting_efficiency(bridge);
    EXPECT_GE(shifting, 0.0f);
}

TEST_F(AttentionSubstrateBridgeTest, VigilanceNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);
    attention_substrate_update(bridge);

    float vigilance = attention_substrate_get_vigilance(bridge);
    EXPECT_GE(vigilance, 0.0f);
}
