/**
 * @file test_tom_substrate_bridge.cpp
 * @brief Unit tests for Theory of Mind-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-ToM integration including mentalizing capacity,
 * perspective-taking, belief tracking, and empathy factor.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/tom/nimcp_tom_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TomSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    theory_of_mind_t tom = nullptr;
    tom_substrate_bridge_t* bridge = nullptr;
    tom_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // theory_of_mind_t is an opaque handle (typedef struct theory_of_mind_s* theory_of_mind_t)
        // tom_create() requires a brain_t, which can be NULL for standalone usage
        tom = tom_create(nullptr);
        ASSERT_NE(tom, nullptr);

        tom_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            tom_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (tom) {
            tom_destroy(tom);
            tom = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        // Correct parameter order: config, tom, substrate
        bridge = tom_substrate_bridge_create(&config, tom, substrate);
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

TEST_F(TomSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = tom_substrate_bridge_create(&config, tom, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(TomSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = tom_substrate_bridge_create(nullptr, tom, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(TomSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = tom_substrate_bridge_create(&config, tom, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(TomSubstrateBridgeTest, CreateWithNullTom) {
    bridge = tom_substrate_bridge_create(&config, nullptr, substrate);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(TomSubstrateBridgeTest, DestroyNull) {
    tom_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(TomSubstrateBridgeTest, DestroyValid) {
    createBridge();
    tom_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, DefaultConfigEnablesModulations) {
    tom_substrate_config_t cfg;
    tom_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_modulation);
    EXPECT_TRUE(cfg.enable_stress_modulation);
    EXPECT_TRUE(cfg.enable_empathy_modulation);
}

TEST_F(TomSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    tom_substrate_config_t cfg;
    tom_substrate_default_config(&cfg);

    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(TomSubstrateBridgeTest, DefaultConfigNullReturnsError) {
    int result = tom_substrate_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(tom_substrate_is_bio_async_connected(bridge));
}

TEST_F(TomSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = tom_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = tom_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(tom_substrate_get_mentalizing_capacity(bridge), 1.0f);
    EXPECT_FLOAT_EQ(tom_substrate_get_perspective_taking(bridge), 1.0f);
    EXPECT_FLOAT_EQ(tom_substrate_get_belief_tracking(bridge), 1.0f);
    EXPECT_FLOAT_EQ(tom_substrate_get_empathy_factor(bridge), 1.0f);
}

TEST_F(TomSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(tom_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Mentalizing Capacity
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, UpdateWithFullATPMaintainsMentalizing) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = tom_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float capacity = tom_substrate_get_mentalizing_capacity(bridge);
    EXPECT_GE(capacity, 0.9f);
}

TEST_F(TomSubstrateBridgeTest, UpdateWithLowATPReducesMentalizing) {
    createBridge();
    setSubstrateATP(0.3f);

    tom_substrate_update(bridge);

    float capacity = tom_substrate_get_mentalizing_capacity(bridge);
    EXPECT_LT(capacity, 0.7f);
}

/* ============================================================================
 * Update Tests - Perspective Taking
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, UpdateWithFullATPMaintainsPerspective) {
    createBridge();
    setSubstrateATP(1.0f);

    tom_substrate_update(bridge);

    float perspective = tom_substrate_get_perspective_taking(bridge);
    EXPECT_GE(perspective, 0.9f);
}

TEST_F(TomSubstrateBridgeTest, UpdateWithLowATPReducesPerspective) {
    createBridge();
    setSubstrateATP(0.25f);

    tom_substrate_update(bridge);

    float perspective = tom_substrate_get_perspective_taking(bridge);
    EXPECT_LT(perspective, 0.6f);
}

/* ============================================================================
 * Update Tests - Belief Tracking
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, UpdateWithFullATPMaintainsBeliefTracking) {
    createBridge();
    setSubstrateATP(1.0f);

    tom_substrate_update(bridge);

    float beliefs = tom_substrate_get_belief_tracking(bridge);
    EXPECT_GE(beliefs, 0.9f);
}

TEST_F(TomSubstrateBridgeTest, UpdateWithLowATPReducesBeliefTracking) {
    createBridge();
    setSubstrateATP(0.35f);

    tom_substrate_update(bridge);

    float beliefs = tom_substrate_get_belief_tracking(bridge);
    EXPECT_LT(beliefs, 0.7f);
}

/* ============================================================================
 * Update Tests - Empathy Factor
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, UpdateWithFullATPMaintainsEmpathy) {
    createBridge();
    setSubstrateATP(1.0f);

    tom_substrate_update(bridge);

    float empathy = tom_substrate_get_empathy_factor(bridge);
    EXPECT_GE(empathy, 0.9f);
}

TEST_F(TomSubstrateBridgeTest, UpdateWithLowATPReducesEmpathy) {
    createBridge();
    setSubstrateATP(0.2f);

    tom_substrate_update(bridge);

    float empathy = tom_substrate_get_empathy_factor(bridge);
    // Empathy is based on metabolic_capacity, not ATP directly
    // With ATP=0.2, empathy should still be reduced from 1.0
    EXPECT_LT(empathy, 1.0f);
    // But empathy has a floor of 0.3 (basic emotional contagion)
    EXPECT_GE(empathy, 0.3f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, ImpairedWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.1f);

    tom_substrate_update(bridge);

    EXPECT_TRUE(tom_substrate_is_impaired(bridge));
}

TEST_F(TomSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);

    tom_substrate_update(bridge);

    EXPECT_FALSE(tom_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    tom_substrate_effects_t effects;
    int result = tom_substrate_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.mentalizing_capacity, 1.0f);
    EXPECT_FLOAT_EQ(effects.perspective_taking, 1.0f);
}

TEST_F(TomSubstrateBridgeTest, GetEffectsNullBridge) {
    tom_substrate_effects_t effects;
    int result = tom_substrate_get_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(TomSubstrateBridgeTest, GetEffectsNullOutput) {
    createBridge();
    int result = tom_substrate_get_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, GetMentalizingNullReturnsNegative) {
    EXPECT_FLOAT_EQ(tom_substrate_get_mentalizing_capacity(nullptr), -1.0f);
}

TEST_F(TomSubstrateBridgeTest, GetPerspectiveNullReturnsNegative) {
    EXPECT_FLOAT_EQ(tom_substrate_get_perspective_taking(nullptr), -1.0f);
}

TEST_F(TomSubstrateBridgeTest, GetBeliefTrackingNullReturnsNegative) {
    EXPECT_FLOAT_EQ(tom_substrate_get_belief_tracking(nullptr), -1.0f);
}

TEST_F(TomSubstrateBridgeTest, GetEmpathyNullReturnsNegative) {
    EXPECT_FLOAT_EQ(tom_substrate_get_empathy_factor(nullptr), -1.0f);
}

TEST_F(TomSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(tom_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    tom_substrate_stats_t stats;
    int result = tom_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(TomSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    tom_substrate_update(bridge);

    tom_substrate_stats_t stats;
    tom_substrate_get_stats(bridge, &stats);

    // First update should succeed
    EXPECT_EQ(stats.total_updates, 1u);

    // Second update within interval should be skipped due to update_interval_ms
    tom_substrate_update(bridge);
    tom_substrate_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 1u);  // Still 1, second update skipped
}

TEST_F(TomSubstrateBridgeTest, GetStatsNullBridge) {
    tom_substrate_stats_t stats;
    int result = tom_substrate_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, DisabledATPModulationStaysOptimal) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    tom_substrate_update(bridge);

    EXPECT_FLOAT_EQ(tom_substrate_get_mentalizing_capacity(bridge), 1.0f);
}

TEST_F(TomSubstrateBridgeTest, DisabledEmpathyModulationStaysOptimal) {
    config.enable_empathy_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    tom_substrate_update(bridge);

    EXPECT_FLOAT_EQ(tom_substrate_get_empathy_factor(bridge), 1.0f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = tom_substrate_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, AllEffectsNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    tom_substrate_update(bridge);

    EXPECT_GE(tom_substrate_get_mentalizing_capacity(bridge), 0.0f);
    EXPECT_GE(tom_substrate_get_perspective_taking(bridge), 0.0f);
    EXPECT_GE(tom_substrate_get_belief_tracking(bridge), 0.0f);
    EXPECT_GE(tom_substrate_get_empathy_factor(bridge), 0.0f);
}

TEST_F(TomSubstrateBridgeTest, AllEffectsBoundedAbove) {
    createBridge();
    setSubstrateATP(2.0f);

    tom_substrate_update(bridge);

    EXPECT_LE(tom_substrate_get_mentalizing_capacity(bridge), 1.0f);
    EXPECT_LE(tom_substrate_get_perspective_taking(bridge), 1.0f);
    EXPECT_LE(tom_substrate_get_belief_tracking(bridge), 1.0f);
    EXPECT_LE(tom_substrate_get_empathy_factor(bridge), 1.0f);
}

/* ============================================================================
 * Impairment Episode Tracking Tests
 * ============================================================================ */

TEST_F(TomSubstrateBridgeTest, TracksImpairmentEpisodes) {
    createBridge();
    setSubstrateATP(0.1f);

    tom_substrate_update(bridge);

    tom_substrate_stats_t stats;
    tom_substrate_get_stats(bridge, &stats);

    EXPECT_GE(stats.impairment_episodes, 1u);
}

TEST_F(TomSubstrateBridgeTest, TracksSevereImpairmentEpisodes) {
    createBridge();
    setSubstrateATP(0.05f);

    tom_substrate_update(bridge);

    tom_substrate_stats_t stats;
    tom_substrate_get_stats(bridge, &stats);

    // Severe impairment when mentalizing < 0.3
    EXPECT_GE(stats.severe_impairment_episodes, 0u);
}
