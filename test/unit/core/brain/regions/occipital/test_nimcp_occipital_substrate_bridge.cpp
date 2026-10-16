/**
 * @file test_nimcp_occipital_substrate_bridge.cpp
 * @brief Unit tests for nimcp_occipital_substrate_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Substrate bridge
 * WHY:  Ensure correct metabolic modulation of V1-V5 visual processing
 * HOW:  Use Google Test framework to test lifecycle, effects computation,
 *       bio-async messaging, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_substrate_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalSubstrateBridgeTest : public ::testing::Test {
protected:
    occipital_substrate_bridge_t* bridge;
    occipital_adapter_t* occipital;
    neural_substrate_t* substrate;
    occipital_substrate_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create neural substrate
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create substrate bridge with real substrate
        config = occipital_substrate_default_config();
        bridge = occipital_substrate_bridge_create(occipital, substrate, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_substrate_config_t default_config = occipital_substrate_default_config();

    EXPECT_TRUE(default_config.enable_atp_modulation);
    EXPECT_TRUE(default_config.enable_fatigue_modulation);
    EXPECT_GT(default_config.atp_sensitivity, 0.0f);
    EXPECT_GT(default_config.fatigue_sensitivity, 0.0f);
    EXPECT_GT(default_config.min_capacity, 0.0f);
    EXPECT_LT(default_config.min_capacity, 1.0f);

    // V1 should have high ATP weight (early processing)
    EXPECT_GT(default_config.v1_atp_weight, 1.0f);

    // V5 motion should be fatigue-sensitive
    EXPECT_GT(default_config.v5_fatigue_weight, 0.5f);
}

TEST_F(OccipitalSubstrateBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_substrate_config_t retrieved;
    EXPECT_EQ(0, occipital_substrate_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.enable_atp_modulation, retrieved.enable_atp_modulation);
    EXPECT_EQ(config.enable_fatigue_modulation, retrieved.enable_fatigue_modulation);
    EXPECT_FLOAT_EQ(config.atp_sensitivity, retrieved.atp_sensitivity);
}

TEST_F(OccipitalSubstrateBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_substrate_config_t retrieved;
    EXPECT_EQ(-1, occipital_substrate_bridge_get_config(nullptr, &retrieved));
}

TEST_F(OccipitalSubstrateBridgeTest, GetConfigWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_substrate_bridge_get_config(bridge, nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, CreateWithNullOccipitalSucceeds) {
    // Bridge can be created without occipital (for standalone testing)
    occipital_substrate_bridge_t* standalone = occipital_substrate_bridge_create(
        NULL, substrate, &config);
    EXPECT_NE(nullptr, standalone);
    occipital_substrate_bridge_destroy(standalone);
}

TEST_F(OccipitalSubstrateBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_substrate_bridge_t* default_bridge = occipital_substrate_bridge_create(
        occipital, substrate, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_substrate_config_t retrieved;
    EXPECT_EQ(0, occipital_substrate_bridge_get_config(default_bridge, &retrieved));
    EXPECT_TRUE(retrieved.enable_atp_modulation);

    occipital_substrate_bridge_destroy(default_bridge);
}

TEST_F(OccipitalSubstrateBridgeTest, DestroyNullDoesNotCrash) {
    occipital_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalSubstrateBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_substrate_bridge_reset(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_reset(nullptr));
}

// ============================================================================
// EFFECTS COMPUTATION TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, UpdateEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_substrate_bridge_update(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_update(nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, GetEffectsReturnsValidValues) {
    ASSERT_NE(nullptr, bridge);

    // Update first
    occipital_substrate_bridge_update(bridge);

    occipital_substrate_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_substrate_bridge_get_effects(bridge, &effects));

    // All effects should be in valid range [0, 1]
    EXPECT_GE(effects.v1_contrast_sensitivity, 0.0f);
    EXPECT_LE(effects.v1_contrast_sensitivity, 1.0f);

    EXPECT_GE(effects.v4_color_constancy, 0.0f);
    EXPECT_LE(effects.v4_color_constancy, 1.0f);

    EXPECT_GE(effects.v5_motion_direction, 0.0f);
    EXPECT_LE(effects.v5_motion_direction, 1.0f);

    EXPECT_GE(effects.overall_capacity, 0.0f);
    EXPECT_LE(effects.overall_capacity, 1.0f);
}

TEST_F(OccipitalSubstrateBridgeTest, GetEffectsWithNullBridgeFails) {
    occipital_substrate_effects_t effects;
    EXPECT_EQ(-1, occipital_substrate_bridge_get_effects(nullptr, &effects));
}

TEST_F(OccipitalSubstrateBridgeTest, GetEffectsWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_substrate_bridge_get_effects(bridge, nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, GetAreaCapacityReturnsValidRange) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);

    // Test all visual areas (0=V1, 1=V2, 2=V3, 3=V4, 4=V5)
    for (uint32_t area = 0; area < 5; area++) {
        float capacity = occipital_substrate_bridge_get_area_capacity(bridge, area);
        EXPECT_GE(capacity, 0.0f);
        EXPECT_LE(capacity, 1.0f);
    }
}

TEST_F(OccipitalSubstrateBridgeTest, GetAreaCapacityInvalidAreaReturnsError) {
    ASSERT_NE(nullptr, bridge);
    float capacity = occipital_substrate_bridge_get_area_capacity(bridge, 100);
    EXPECT_LT(capacity, 0.0f);  // Should return -1.0f
}

TEST_F(OccipitalSubstrateBridgeTest, ApplyEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);
    EXPECT_EQ(0, occipital_substrate_bridge_apply_effects(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, ApplyEffectsNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_apply_effects(nullptr));
}

// ============================================================================
// QUERY TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, IsImpairedReturnsFalseWhenHealthy) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);
    // Without substrate, default capacity should be high
    EXPECT_FALSE(occipital_substrate_bridge_is_impaired(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, IsAtpCriticalReturnsFalseWhenHealthy) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);
    // Without substrate, default ATP should be high
    EXPECT_FALSE(occipital_substrate_bridge_is_atp_critical(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, IsImpairedNullBridgeReturnsFalse) {
    // Null check - should return false safely
    EXPECT_FALSE(occipital_substrate_bridge_is_impaired(nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, IsAtpCriticalNullBridgeReturnsFalse) {
    EXPECT_FALSE(occipital_substrate_bridge_is_atp_critical(nullptr));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_substrate_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Pre-fill with non-zero
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.updates_processed);
    EXPECT_EQ(0ULL, stats.low_atp_events);
    EXPECT_EQ(0ULL, stats.high_fatigue_events);
}

TEST_F(OccipitalSubstrateBridgeTest, GetStatsUpdatesAfterProcessing) {
    ASSERT_NE(nullptr, bridge);

    // Perform some updates
    for (int i = 0; i < 5; i++) {
        occipital_substrate_bridge_update(bridge);
    }

    occipital_substrate_stats_t stats;
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(5ULL, stats.updates_processed);
}

TEST_F(OccipitalSubstrateBridgeTest, GetStatsWithNullBridgeFails) {
    occipital_substrate_stats_t stats;
    EXPECT_EQ(-1, occipital_substrate_bridge_get_stats(nullptr, &stats));
}

TEST_F(OccipitalSubstrateBridgeTest, GetStatsWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_substrate_bridge_get_stats(bridge, nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, ResetStatsWorks) {
    ASSERT_NE(nullptr, bridge);

    // Perform some updates
    for (int i = 0; i < 5; i++) {
        occipital_substrate_bridge_update(bridge);
    }

    // Reset stats
    occipital_substrate_bridge_reset_stats(bridge);

    // Verify reset
    occipital_substrate_stats_t stats;
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.updates_processed);
}

TEST_F(OccipitalSubstrateBridgeTest, ResetStatsNullBridgeDoesNotCrash) {
    occipital_substrate_bridge_reset_stats(nullptr);
    SUCCEED();
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, RegisterBioAsyncNullRouterSucceeds) {
    ASSERT_NE(nullptr, bridge);
    // Should succeed but do nothing without router
    EXPECT_EQ(0, occipital_substrate_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, RegisterBioAsyncNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_register_bio_async(nullptr, nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, BroadcastCapacitySucceeds) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);
    // Should succeed even without router (no-op)
    EXPECT_EQ(0, occipital_substrate_bridge_broadcast_capacity(bridge));
}

TEST_F(OccipitalSubstrateBridgeTest, BroadcastCapacityNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_broadcast_capacity(nullptr));
}

TEST_F(OccipitalSubstrateBridgeTest, ProcessMessagesWithZeroCount) {
    ASSERT_NE(nullptr, bridge);
    // Process all pending (0 means all)
    int result = occipital_substrate_bridge_process_messages(bridge, 0);
    EXPECT_GE(result, 0);  // Should return count (0 if none)
}

TEST_F(OccipitalSubstrateBridgeTest, ProcessMessagesNullBridgeFails) {
    EXPECT_EQ(-1, occipital_substrate_bridge_process_messages(nullptr, 10));
}

// ============================================================================
// LAYER-SPECIFIC EFFECTS TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, LayerGainsAreWithinRange) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);

    occipital_substrate_effects_t effects;
    EXPECT_EQ(0, occipital_substrate_bridge_get_effects(bridge, &effects));

    // Layer gains should be in valid range
    EXPECT_GE(effects.layer_4_gain, 0.0f);
    EXPECT_LE(effects.layer_4_gain, 1.0f);

    EXPECT_GE(effects.layer_23_gain, 0.0f);
    EXPECT_LE(effects.layer_23_gain, 1.0f);

    EXPECT_GE(effects.layer_56_gain, 0.0f);
    EXPECT_LE(effects.layer_56_gain, 1.0f);
}

// ============================================================================
// VISUAL AREA HIERARCHY TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, AllVisualAreasHaveValidEffects) {
    ASSERT_NE(nullptr, bridge);
    occipital_substrate_bridge_update(bridge);

    occipital_substrate_effects_t effects;
    EXPECT_EQ(0, occipital_substrate_bridge_get_effects(bridge, &effects));

    // V1 effects
    EXPECT_GE(effects.v1_contrast_sensitivity, 0.0f);
    EXPECT_GE(effects.v1_orientation_selectivity, 0.0f);
    EXPECT_GE(effects.v1_spatial_frequency, 0.0f);

    // V2 effects
    EXPECT_GE(effects.v2_contour_integration, 0.0f);
    EXPECT_GE(effects.v2_texture_processing, 0.0f);
    EXPECT_GE(effects.v2_figure_ground, 0.0f);

    // V3 effects
    EXPECT_GE(effects.v3_dynamic_form, 0.0f);

    // V4 effects
    EXPECT_GE(effects.v4_color_constancy, 0.0f);
    EXPECT_GE(effects.v4_complex_form, 0.0f);
    EXPECT_GE(effects.v4_object_recognition, 0.0f);

    // V5 effects
    EXPECT_GE(effects.v5_motion_direction, 0.0f);
    EXPECT_GE(effects.v5_optic_flow, 0.0f);
    EXPECT_GE(effects.v5_speed_tuning, 0.0f);
    EXPECT_GE(effects.v5_motion_coherence, 0.0f);

    // Global effects
    EXPECT_GE(effects.attention_capacity, 0.0f);
    EXPECT_GE(effects.processing_speed, 0.0f);
    EXPECT_GE(effects.overall_capacity, 0.0f);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalSubstrateBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    // Perform many updates
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_substrate_bridge_update(bridge));
    }

    occipital_substrate_stats_t stats;
    EXPECT_EQ(0, occipital_substrate_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(1000ULL, stats.updates_processed);
}

TEST_F(OccipitalSubstrateBridgeTest, CreateDestroyMultipleTimes) {
    // Create and destroy multiple bridges to check for leaks
    for (int i = 0; i < 100; i++) {
        occipital_substrate_bridge_t* temp = occipital_substrate_bridge_create(
            occipital, substrate, &config);
        ASSERT_NE(nullptr, temp);
        occipital_substrate_bridge_destroy(temp);
    }
    SUCCEED();
}
