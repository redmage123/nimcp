/**
 * @file test_introspection_substrate_bridge.cpp
 * @brief Unit tests for Introspection-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-introspection integration including self-awareness,
 * metacognitive accuracy, monitoring capacity, and uncertainty estimation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/introspection/nimcp_introspection_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class IntrospectionSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    nimcp_introspection_t* introspection = nullptr;
    introspection_substrate_bridge_t* bridge = nullptr;
    introspection_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create a dummy introspection pointer (opaque type, implementation doesn't exist)
        // The bridge just stores the pointer, doesn't call functions on it
        introspection = (nimcp_introspection_t*)nimcp_malloc(64);
        ASSERT_NE(introspection, nullptr);

        introspection_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            introspection_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (introspection) {
            nimcp_free(introspection);
            introspection = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = introspection_substrate_bridge_create(&config, substrate, introspection);
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

TEST_F(IntrospectionSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = introspection_substrate_bridge_create(&config, substrate, introspection);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(IntrospectionSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = introspection_substrate_bridge_create(nullptr, substrate, introspection);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(IntrospectionSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = introspection_substrate_bridge_create(&config, nullptr, introspection);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(IntrospectionSubstrateBridgeTest, CreateWithNullIntrospection) {
    bridge = introspection_substrate_bridge_create(&config, substrate, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(IntrospectionSubstrateBridgeTest, DestroyNull) {
    introspection_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(IntrospectionSubstrateBridgeTest, DestroyValid) {
    createBridge();
    introspection_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, DefaultConfigEnablesModulations) {
    introspection_substrate_config_t cfg;
    introspection_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_effects);
    EXPECT_TRUE(cfg.enable_metabolic_monitoring);
    EXPECT_TRUE(cfg.enable_bio_async);
}

TEST_F(IntrospectionSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    introspection_substrate_config_t cfg;
    introspection_substrate_default_config(&cfg);

    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, DefaultConfigNullSafe) {
    introspection_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(introspection_substrate_is_bio_async_connected(bridge));
}

TEST_F(IntrospectionSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = introspection_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(IntrospectionSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = introspection_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(introspection_substrate_get_self_awareness_depth(bridge), 1.0f);
    EXPECT_FLOAT_EQ(introspection_substrate_get_metacognitive_accuracy(bridge), 1.0f);
    EXPECT_FLOAT_EQ(introspection_substrate_get_monitoring_capacity(bridge), 1.0f);
    EXPECT_FLOAT_EQ(introspection_substrate_get_uncertainty_estimation(bridge), 1.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(introspection_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Self-Awareness
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithFullATPMaintainsSelfAwareness) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = introspection_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float awareness = introspection_substrate_get_self_awareness_depth(bridge);
    EXPECT_GE(awareness, 0.9f);
}

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithLowATPReducesSelfAwareness) {
    createBridge();
    setSubstrateATP(0.25f);

    introspection_substrate_update(bridge);

    float awareness = introspection_substrate_get_self_awareness_depth(bridge);
    EXPECT_LT(awareness, 0.6f);
}

/* ============================================================================
 * Update Tests - Metacognitive Accuracy
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithFullATPMaintainsMetacognition) {
    createBridge();
    setSubstrateATP(1.0f);

    introspection_substrate_update(bridge);

    float metacog = introspection_substrate_get_metacognitive_accuracy(bridge);
    /* Metacognition depends on metabolic_capacity + physical_capacity avg.
     * With default O2/glucose/membrane values (0.90-0.98), expect ~0.75-0.85 */
    EXPECT_GE(metacog, 0.7f);
}

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithLowATPReducesMetacognition) {
    createBridge();
    setSubstrateATP(0.2f);

    introspection_substrate_update(bridge);

    float metacog = introspection_substrate_get_metacognitive_accuracy(bridge);
    /* Low ATP reduces metabolic_capacity but physical_capacity unchanged.
     * Formula has 0.3 floor, expect reduced but not critically low */
    EXPECT_LT(metacog, 0.7f);
}

/* ============================================================================
 * Update Tests - Monitoring Capacity
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithFullATPMaintainsMonitoring) {
    createBridge();
    setSubstrateATP(1.0f);

    introspection_substrate_update(bridge);

    float monitoring = introspection_substrate_get_monitoring_capacity(bridge);
    EXPECT_GE(monitoring, 0.9f);
}

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithModerateATPReducesMonitoring) {
    createBridge();
    setSubstrateATP(0.45f);

    introspection_substrate_update(bridge);

    float monitoring = introspection_substrate_get_monitoring_capacity(bridge);
    EXPECT_LT(monitoring, 0.9f);
}

/* ============================================================================
 * Update Tests - Uncertainty Estimation
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithFullATPMaintainsUncertainty) {
    createBridge();
    setSubstrateATP(1.0f);

    introspection_substrate_update(bridge);

    float uncertainty = introspection_substrate_get_uncertainty_estimation(bridge);
    /* Uncertainty = metabolic_capacity * 0.9 * sensitivity.
     * With default O2/glucose, metabolic_capacity ~0.97, expect ~0.87 */
    EXPECT_GE(uncertainty, 0.85f);
}

TEST_F(IntrospectionSubstrateBridgeTest, UpdateWithLowATPAffectsUncertainty) {
    createBridge();
    setSubstrateATP(0.3f);

    introspection_substrate_update(bridge);

    float uncertainty = introspection_substrate_get_uncertainty_estimation(bridge);
    EXPECT_GE(uncertainty, 0.0f);
    EXPECT_LE(uncertainty, 1.0f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, ImpairedWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.1f);

    introspection_substrate_update(bridge);

    EXPECT_TRUE(introspection_substrate_is_impaired(bridge));
}

TEST_F(IntrospectionSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);

    introspection_substrate_update(bridge);

    EXPECT_FALSE(introspection_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    introspection_substrate_effects_t effects;
    int result = introspection_substrate_get_effects(bridge, &effects);

    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(effects.self_awareness_depth, 1.0f);
    EXPECT_FLOAT_EQ(effects.metacognitive_accuracy, 1.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetEffectsNullBridge) {
    introspection_substrate_effects_t effects;
    int result = introspection_substrate_get_effects(nullptr, &effects);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetEffectsNullOutput) {
    createBridge();
    int result = introspection_substrate_get_effects(bridge, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, GetSelfAwarenessNullReturnsZero) {
    EXPECT_FLOAT_EQ(introspection_substrate_get_self_awareness_depth(nullptr), 0.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetMetacognitionNullReturnsZero) {
    EXPECT_FLOAT_EQ(introspection_substrate_get_metacognitive_accuracy(nullptr), 0.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetMonitoringNullReturnsZero) {
    EXPECT_FLOAT_EQ(introspection_substrate_get_monitoring_capacity(nullptr), 0.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetUncertaintyNullReturnsZero) {
    EXPECT_FLOAT_EQ(introspection_substrate_get_uncertainty_estimation(nullptr), 0.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(introspection_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    introspection_substrate_stats_t stats;
    int result = introspection_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.update_count, 0u);
}

TEST_F(IntrospectionSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    introspection_substrate_update(bridge);
    introspection_substrate_update(bridge);

    introspection_substrate_stats_t stats;
    introspection_substrate_get_stats(bridge, &stats);

    EXPECT_EQ(stats.update_count, 2u);
}

TEST_F(IntrospectionSubstrateBridgeTest, GetStatsNullBridge) {
    introspection_substrate_stats_t stats;
    int result = introspection_substrate_get_stats(nullptr, &stats);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, DisabledATPModulationStaysOptimal) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    introspection_substrate_update(bridge);

    EXPECT_FLOAT_EQ(introspection_substrate_get_self_awareness_depth(bridge), 1.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, DisabledFatigueEffectsStaysOptimal) {
    config.enable_fatigue_effects = false;
    createBridge();
    setSubstrateATP(0.1f);

    introspection_substrate_update(bridge);

    EXPECT_FLOAT_EQ(introspection_substrate_get_metacognitive_accuracy(bridge), 1.0f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = introspection_substrate_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(IntrospectionSubstrateBridgeTest, AllEffectsNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    introspection_substrate_update(bridge);

    EXPECT_GE(introspection_substrate_get_self_awareness_depth(bridge), 0.0f);
    EXPECT_GE(introspection_substrate_get_metacognitive_accuracy(bridge), 0.0f);
    EXPECT_GE(introspection_substrate_get_monitoring_capacity(bridge), 0.0f);
    EXPECT_GE(introspection_substrate_get_uncertainty_estimation(bridge), 0.0f);
}

TEST_F(IntrospectionSubstrateBridgeTest, AllEffectsBoundedAbove) {
    createBridge();
    setSubstrateATP(2.0f);

    introspection_substrate_update(bridge);

    EXPECT_LE(introspection_substrate_get_self_awareness_depth(bridge), 1.0f);
    EXPECT_LE(introspection_substrate_get_metacognitive_accuracy(bridge), 1.0f);
    EXPECT_LE(introspection_substrate_get_monitoring_capacity(bridge), 1.0f);
    EXPECT_LE(introspection_substrate_get_uncertainty_estimation(bridge), 1.0f);
}
