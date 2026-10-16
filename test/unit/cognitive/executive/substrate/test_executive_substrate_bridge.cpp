/**
 * @file test_executive_substrate_bridge.cpp
 * @brief Unit tests for Executive-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-executive integration including decision quality,
 * inhibition strength, planning depth, cognitive flexibility, and fatigue tracking.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/executive/nimcp_executive_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_executive.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExecutiveSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    executive_controller_t* executive = nullptr;
    executive_substrate_bridge_t* bridge = nullptr;
    executive_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        executive = executive_create();
        ASSERT_NE(executive, nullptr);

        executive_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            executive_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (executive) {
            executive_destroy(executive);
            executive = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = executive_substrate_bridge_create(&config, (nimcp_executive_t*)executive, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = executive_substrate_bridge_create(&config, (nimcp_executive_t*)executive, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(ExecutiveSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = executive_substrate_bridge_create(nullptr, (nimcp_executive_t*)executive, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(ExecutiveSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = executive_substrate_bridge_create(&config, (nimcp_executive_t*)executive, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ExecutiveSubstrateBridgeTest, CreateWithNullExecutive) {
    bridge = executive_substrate_bridge_create(&config, nullptr, substrate);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ExecutiveSubstrateBridgeTest, DestroyNull) {
    executive_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(ExecutiveSubstrateBridgeTest, DestroyValid) {
    createBridge();
    executive_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, DefaultConfigEnablesAllModulations) {
    executive_substrate_config_t cfg;
    executive_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_decision_modulation);
    EXPECT_TRUE(cfg.enable_inhibition_modulation);
    EXPECT_TRUE(cfg.enable_planning_modulation);
    EXPECT_TRUE(cfg.enable_flexibility_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_tracking);
}

TEST_F(ExecutiveSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    executive_substrate_config_t cfg;
    executive_substrate_default_config(&cfg);

    EXPECT_FLOAT_EQ(cfg.decision_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.inhibition_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.planning_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(cfg.flexibility_sensitivity, 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, DefaultConfigNullSafe) {
    executive_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(executive_substrate_is_bio_async_connected(bridge));
}

TEST_F(ExecutiveSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = executive_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(ExecutiveSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = executive_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(executive_substrate_get_decision_quality(bridge), 1.0f);
    EXPECT_FLOAT_EQ(executive_substrate_get_inhibition_strength(bridge), 1.0f);
    EXPECT_FLOAT_EQ(executive_substrate_get_planning_depth(bridge), 1.0f);
    EXPECT_FLOAT_EQ(executive_substrate_get_cognitive_flexibility(bridge), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, InitialFatigueIsZero) {
    createBridge();
    EXPECT_FLOAT_EQ(executive_substrate_get_fatigue(bridge), 0.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(executive_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Decision Quality
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithFullATPMaintainsDecisionQuality) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = executive_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float quality = executive_substrate_get_decision_quality(bridge);
    EXPECT_GE(quality, 0.9f);
}

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithLowATPReducesDecisionQuality) {
    createBridge();
    setSubstrateATP(0.3f);

    executive_substrate_update(bridge);

    float quality = executive_substrate_get_decision_quality(bridge);
    EXPECT_LT(quality, 0.7f);
}

/* ============================================================================
 * Update Tests - Inhibition Strength
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithFullATPMaintainsInhibition) {
    createBridge();
    setSubstrateATP(1.0f);

    executive_substrate_update(bridge);

    float inhibition = executive_substrate_get_inhibition_strength(bridge);
    EXPECT_GE(inhibition, 0.9f);
}

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithLowATPReducesInhibition) {
    createBridge();
    setSubstrateATP(0.25f);

    executive_substrate_update(bridge);

    float inhibition = executive_substrate_get_inhibition_strength(bridge);
    EXPECT_LT(inhibition, 0.5f);
}

/* ============================================================================
 * Update Tests - Planning Depth
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithFullATPMaintainsPlanning) {
    createBridge();
    setSubstrateATP(1.0f);

    executive_substrate_update(bridge);

    float planning = executive_substrate_get_planning_depth(bridge);
    EXPECT_GE(planning, 0.9f);
}

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithLowATPReducesPlanning) {
    createBridge();
    setSubstrateATP(0.4f);

    executive_substrate_update(bridge);

    float planning = executive_substrate_get_planning_depth(bridge);
    EXPECT_LT(planning, 0.8f);
}

/* ============================================================================
 * Update Tests - Cognitive Flexibility
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithFullATPMaintainsFlexibility) {
    createBridge();
    setSubstrateATP(1.0f);

    executive_substrate_update(bridge);

    float flexibility = executive_substrate_get_cognitive_flexibility(bridge);
    EXPECT_GE(flexibility, 0.9f);
}

TEST_F(ExecutiveSubstrateBridgeTest, UpdateWithLowATPReducesFlexibility) {
    createBridge();
    setSubstrateATP(0.35f);

    executive_substrate_update(bridge);

    float flexibility = executive_substrate_get_cognitive_flexibility(bridge);
    EXPECT_LT(flexibility, 0.7f);
}

/* ============================================================================
 * Fatigue Tracking Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, FatigueAccumulatesWithLowATP) {
    createBridge();
    setSubstrateATP(0.4f);

    // Multiple updates accumulate fatigue
    for (int i = 0; i < 5; i++) {
        executive_substrate_update(bridge);
    }

    float fatigue = executive_substrate_get_fatigue(bridge);
    EXPECT_GT(fatigue, 0.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, FatigueCappedAtMaximum) {
    createBridge();
    setSubstrateATP(0.1f);

    // Many updates to push fatigue high
    for (int i = 0; i < 100; i++) {
        executive_substrate_update(bridge);
    }

    float fatigue = executive_substrate_get_fatigue(bridge);
    EXPECT_LE(fatigue, 1.0f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, ImpairedWithLowATP) {
    createBridge();
    setSubstrateATP(0.15f);

    executive_substrate_update(bridge);

    EXPECT_TRUE(executive_substrate_is_impaired(bridge));
}

TEST_F(ExecutiveSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);

    executive_substrate_update(bridge);

    EXPECT_FALSE(executive_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    executive_substrate_effects_t effects = executive_substrate_get_effects(bridge);

    EXPECT_FLOAT_EQ(effects.decision_quality, 1.0f);
    EXPECT_FLOAT_EQ(effects.inhibition_strength, 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetEffectsNullBridge) {
    executive_substrate_effects_t effects = executive_substrate_get_effects(nullptr);
    // Should return neutral effects
    EXPECT_FLOAT_EQ(effects.decision_quality, 1.0f);
    EXPECT_FLOAT_EQ(effects.inhibition_strength, 1.0f);
    EXPECT_FLOAT_EQ(effects.planning_depth, 1.0f);
    EXPECT_FLOAT_EQ(effects.cognitive_flexibility, 1.0f);
    EXPECT_FLOAT_EQ(effects.fatigue_level, 0.0f);
    EXPECT_FALSE(effects.is_impaired);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, GetDecisionQualityNullReturnsNeutral) {
    EXPECT_FLOAT_EQ(executive_substrate_get_decision_quality(nullptr), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetInhibitionNullReturnsNeutral) {
    EXPECT_FLOAT_EQ(executive_substrate_get_inhibition_strength(nullptr), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetPlanningNullReturnsNeutral) {
    EXPECT_FLOAT_EQ(executive_substrate_get_planning_depth(nullptr), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetFlexibilityNullReturnsNeutral) {
    EXPECT_FLOAT_EQ(executive_substrate_get_cognitive_flexibility(nullptr), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetFatigueNullReturnsZero) {
    EXPECT_FLOAT_EQ(executive_substrate_get_fatigue(nullptr), 0.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(executive_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    executive_substrate_stats_t stats = executive_substrate_get_stats(bridge);

    EXPECT_EQ(stats.update_count, 0u);
}

TEST_F(ExecutiveSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    executive_substrate_update(bridge);
    executive_substrate_update(bridge);
    executive_substrate_update(bridge);

    executive_substrate_stats_t stats = executive_substrate_get_stats(bridge);

    EXPECT_EQ(stats.update_count, 3u);
}

TEST_F(ExecutiveSubstrateBridgeTest, GetStatsNullBridge) {
    executive_substrate_stats_t stats = executive_substrate_get_stats(nullptr);
    // Should return zeroed stats
    EXPECT_EQ(stats.update_count, 0u);
    EXPECT_EQ(stats.impairment_events, 0u);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, DisabledDecisionModulationStaysOptimal) {
    config.enable_decision_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    executive_substrate_update(bridge);

    EXPECT_FLOAT_EQ(executive_substrate_get_decision_quality(bridge), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, DisabledInhibitionModulationStaysOptimal) {
    config.enable_inhibition_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    executive_substrate_update(bridge);

    EXPECT_FLOAT_EQ(executive_substrate_get_inhibition_strength(bridge), 1.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, DisabledPlanningModulationStaysOptimal) {
    config.enable_planning_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    executive_substrate_update(bridge);

    EXPECT_FLOAT_EQ(executive_substrate_get_planning_depth(bridge), 1.0f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = executive_substrate_update(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(ExecutiveSubstrateBridgeTest, DecisionQualityNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    executive_substrate_update(bridge);

    float quality = executive_substrate_get_decision_quality(bridge);
    EXPECT_GE(quality, 0.0f);
}

TEST_F(ExecutiveSubstrateBridgeTest, AllEffectsBoundedAbove) {
    createBridge();
    setSubstrateATP(2.0f);

    executive_substrate_update(bridge);

    EXPECT_LE(executive_substrate_get_decision_quality(bridge), 1.0f);
    EXPECT_LE(executive_substrate_get_inhibition_strength(bridge), 1.0f);
    EXPECT_LE(executive_substrate_get_planning_depth(bridge), 1.0f);
    EXPECT_LE(executive_substrate_get_cognitive_flexibility(bridge), 1.0f);
}
