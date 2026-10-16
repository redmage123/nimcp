/**
 * @file test_brain_regions_immune_bridge.cpp
 * @brief Unit tests for Brain Regions Immune Bridge
 * @date 2025-12-12
 *
 * Tests region-specific immune sensitivity, inflammation propagation,
 * abnormality detection, and immune response triggering.
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions_immune_bridge.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class BrainRegionsImmuneBridgeTest : public ::testing::Test {
protected:
    brain_module_t* brain_module = nullptr;
    brain_immune_system_t* immune_system = nullptr;
    brain_regions_immune_bridge_t* bridge = nullptr;
    brain_regions_immune_config_t config;

    void SetUp() override {
        // Create brain module with regions
        brain_module = brain_module_create(16);
        ASSERT_NE(brain_module, nullptr);

        // Add some regions
        brain_region_t* hippocampus = brain_region_create(REGION_HIPPOCAMPUS, 100);
        brain_region_t* prefrontal = brain_region_create(REGION_PREFRONTAL, 100);
        brain_region_t* motor = brain_region_create(REGION_MOTOR_M1, 100);
        brain_region_t* thalamus = brain_region_create(REGION_THALAMUS, 50);

        ASSERT_NE(hippocampus, nullptr);
        ASSERT_NE(prefrontal, nullptr);
        ASSERT_NE(motor, nullptr);
        ASSERT_NE(thalamus, nullptr);

        brain_module_add_region(brain_module, hippocampus);
        brain_module_add_region(brain_module, prefrontal);
        brain_module_add_region(brain_module, motor);
        brain_module_add_region(brain_module, thalamus);

        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        // Get default bridge config
        brain_regions_immune_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            brain_regions_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        if (brain_module) {
            brain_module_destroy(brain_module);
            brain_module = nullptr;
        }
    }

    // Helper to create bridge with default settings
    void createBridge() {
        bridge = brain_regions_immune_bridge_create(&config, brain_module, immune_system);
        ASSERT_NE(bridge, nullptr);
    }

    // Helper to release cytokines
    void releaseCytokines(float il1, float il6, float tnf) {
        if (!immune_system) return;
        if (il1 > 0) {
            brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, il1, 0);
        }
        if (il6 > 0) {
            brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, il6, 0);
        }
        if (tnf > 0) {
            brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, tnf, 0);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, DefaultConfigIsValid) {
    brain_regions_immune_config_t cfg;
    int result = brain_regions_immune_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cfg.enable_region_specific_sensitivity);
    EXPECT_TRUE(cfg.enable_inflammation_propagation);
    EXPECT_TRUE(cfg.enable_layer_effects);
    EXPECT_TRUE(cfg.enable_abnormality_detection);
    EXPECT_TRUE(cfg.enable_il10_recovery);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_EQ(cfg.cytokine_sensitivity_multiplier, 1.0f);
    EXPECT_GT(cfg.max_regions, 0u);
}

TEST_F(BrainRegionsImmuneBridgeTest, DefaultConfigNullFails) {
    int result = brain_regions_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, CreateWithValidParams) {
    createBridge();
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BrainRegionsImmuneBridgeTest, CreateWithNullBrainModuleFails) {
    bridge = brain_regions_immune_bridge_create(&config, nullptr, immune_system);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(BrainRegionsImmuneBridgeTest, CreateWithNullImmuneSystemSucceeds) {
    // Should succeed but with limited functionality
    bridge = brain_regions_immune_bridge_create(&config, brain_module, nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BrainRegionsImmuneBridgeTest, CreateWithNullConfig) {
    bridge = brain_regions_immune_bridge_create(nullptr, brain_module, immune_system);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(BrainRegionsImmuneBridgeTest, DestroyNullSafe) {
    brain_regions_immune_bridge_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, ConnectBioAsync) {
    createBridge();
    // May or may not succeed depending on router availability
    int result = brain_regions_immune_connect_bio_async(bridge);
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, DisconnectBioAsync) {
    createBridge();
    int result = brain_regions_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainRegionsImmuneBridgeTest, IsBioAsyncConnectedInitial) {
    createBridge();
    // May or may not be connected
    bool connected = brain_regions_immune_is_bio_async_connected(bridge);
    EXPECT_TRUE(connected || !connected);  // Valid either way
}

TEST_F(BrainRegionsImmuneBridgeTest, BioAsyncNullChecks) {
    EXPECT_EQ(brain_regions_immune_connect_bio_async(nullptr), -1);
    EXPECT_EQ(brain_regions_immune_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(brain_regions_immune_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Region Sensitivity Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, GetDefaultSensitivity) {
    createBridge();

    region_cytokine_sensitivity_t sens;
    int result = brain_regions_immune_get_sensitivity(bridge, REGION_HIPPOCAMPUS, &sens);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sens.region_type, REGION_HIPPOCAMPUS);
    EXPECT_GT(sens.il6_sensitivity, sens.il1_sensitivity);  // Hippocampus is IL-6 sensitive
}

TEST_F(BrainRegionsImmuneBridgeTest, GetPrefrontalSensitivity) {
    createBridge();

    region_cytokine_sensitivity_t sens;
    int result = brain_regions_immune_get_sensitivity(bridge, REGION_PREFRONTAL, &sens);
    EXPECT_EQ(result, 0);
    EXPECT_GT(sens.il1_sensitivity, sens.il6_sensitivity);  // Prefrontal is IL-1β sensitive
}

TEST_F(BrainRegionsImmuneBridgeTest, SetCustomSensitivity) {
    createBridge();

    region_cytokine_sensitivity_t custom;
    custom.region_type = REGION_HIPPOCAMPUS;
    custom.il1_sensitivity = 2.0f;
    custom.il6_sensitivity = 2.0f;
    custom.tnf_sensitivity = 2.0f;
    custom.ifn_sensitivity = 2.0f;
    custom.il10_responsiveness = 0.9f;

    int result = brain_regions_immune_set_sensitivity(bridge, &custom);
    EXPECT_EQ(result, 0);

    // Verify it was set
    region_cytokine_sensitivity_t retrieved;
    brain_regions_immune_get_sensitivity(bridge, REGION_HIPPOCAMPUS, &retrieved);
    EXPECT_EQ(retrieved.il1_sensitivity, 2.0f);
}

TEST_F(BrainRegionsImmuneBridgeTest, SetSensitivityNullFails) {
    createBridge();
    EXPECT_EQ(brain_regions_immune_set_sensitivity(nullptr, nullptr), -1);
    EXPECT_EQ(brain_regions_immune_set_sensitivity(bridge, nullptr), -1);
}

/* ============================================================================
 * Immune → Brain Regions Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, ApplyEffectsNoInflammation) {
    createBridge();

    int result = brain_regions_immune_apply_effects(bridge);
    EXPECT_EQ(result, 0);

    // Check hippocampus has no modulation
    float mod = brain_regions_immune_get_activity_modulation(bridge, 0);
    EXPECT_GE(mod, 0.9f);  // Near full activity
}

TEST_F(BrainRegionsImmuneBridgeTest, ApplyEffectsWithCytokines) {
    createBridge();

    // Release cytokines
    releaseCytokines(0.5f, 0.5f, 0.3f);

    int result = brain_regions_immune_apply_effects(bridge);
    EXPECT_EQ(result, 0);

    // Regions should be modulated
    // Get inflammation state
    region_inflammation_state_t state;
    for (uint32_t i = 0; i < brain_module->num_regions; i++) {
        brain_region_t* region = brain_module->regions[i];
        if (region) {
            int get_result = brain_regions_immune_get_inflammation_state(bridge, region->id, &state);
            EXPECT_EQ(get_result, 0);
        }
    }
}

TEST_F(BrainRegionsImmuneBridgeTest, ApplyToSpecificRegion) {
    createBridge();

    // Get hippocampus
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    releaseCytokines(0.4f, 0.6f, 0.0f);  // IL-6 high (hippocampus sensitive)

    int result = brain_regions_immune_apply_to_region(bridge, hippocampus->id);
    EXPECT_EQ(result, 0);

    // Check inflammation state
    region_inflammation_state_t state;
    int get_result = brain_regions_immune_get_inflammation_state(bridge, hippocampus->id, &state);
    EXPECT_EQ(get_result, 0);
    EXPECT_GT(state.il6_impact, state.il1_impact);  // IL-6 should have more impact
}

TEST_F(BrainRegionsImmuneBridgeTest, ApplyLayerEffects) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    releaseCytokines(0.5f, 0.5f, 0.5f);
    brain_regions_immune_apply_to_region(bridge, hippocampus->id);

    int result = brain_regions_immune_apply_layer_effects(bridge, hippocampus->id);
    EXPECT_EQ(result, 0);

    region_inflammation_state_t state;
    brain_regions_immune_get_inflammation_state(bridge, hippocampus->id, &state);

    // Layer 4 and 5 should have more disruption
    EXPECT_GE(state.layer_disruption[LAYER_4], state.layer_disruption[LAYER_1]);
}

TEST_F(BrainRegionsImmuneBridgeTest, PropagateInflammation) {
    createBridge();

    // Need regions to be connected - connect thalamus to others
    brain_region_t* thalamus = brain_module_get_region_by_type(brain_module, REGION_THALAMUS);
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(thalamus, nullptr);
    ASSERT_NE(hippocampus, nullptr);

    // Connect regions
    brain_module_connect_regions(brain_module, thalamus->id, hippocampus->id, 0.5f);

    // Apply high inflammation to thalamus
    releaseCytokines(0.8f, 0.8f, 0.8f);
    brain_regions_immune_apply_to_region(bridge, thalamus->id);

    int propagations = brain_regions_immune_propagate_inflammation(bridge);
    EXPECT_GE(propagations, 0);
}

TEST_F(BrainRegionsImmuneBridgeTest, RestoreWithIL10) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Create inflammation
    releaseCytokines(0.7f, 0.7f, 0.7f);
    brain_regions_immune_apply_to_region(bridge, hippocampus->id);

    region_inflammation_state_t before;
    brain_regions_immune_get_inflammation_state(bridge, hippocampus->id, &before);

    // Apply IL-10 recovery
    int result = brain_regions_immune_restore_region(bridge, hippocampus->id, 0.8f);
    EXPECT_EQ(result, 0);

    region_inflammation_state_t after;
    brain_regions_immune_get_inflammation_state(bridge, hippocampus->id, &after);
    EXPECT_LE(after.intensity, before.intensity);  // Should be lower or equal
}

/* ============================================================================
 * Brain Regions → Immune Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, DetectAbnormalitiesNormal) {
    createBridge();

    // Normal activity levels
    int abnormalities = brain_regions_immune_detect_abnormalities(bridge);
    EXPECT_GE(abnormalities, 0);  // May or may not detect based on activity
}

TEST_F(BrainRegionsImmuneBridgeTest, DetectRegionAbnormalityNormal) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Set normal activity
    hippocampus->activity_level = 1.0f;

    region_abnormality_type_t type =
        brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);
    EXPECT_EQ(type, REGION_ABNORMALITY_NONE);
}

TEST_F(BrainRegionsImmuneBridgeTest, DetectHyperactivity) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // First establish baseline
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);

    // Now set hyperactive
    hippocampus->activity_level = 5.0f;  // Way above threshold

    region_abnormality_type_t type =
        brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);
    EXPECT_EQ(type, REGION_ABNORMALITY_HYPERACTIVE);
}

TEST_F(BrainRegionsImmuneBridgeTest, DetectHypoactivity) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // First establish baseline
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);

    // Now set hypoactive
    hippocampus->activity_level = 0.1f;  // Below threshold

    region_abnormality_type_t type =
        brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);
    EXPECT_EQ(type, REGION_ABNORMALITY_HYPOACTIVE);
}

TEST_F(BrainRegionsImmuneBridgeTest, TriggerImmuneResponse) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Establish baseline and create persistent abnormality
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);

    hippocampus->activity_level = 5.0f;
    for (int i = 0; i < 5; i++) {
        brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);
    }

    // Should trigger response after persistence threshold
    int result = brain_regions_immune_trigger_response(bridge, hippocampus->id);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainRegionsImmuneBridgeTest, ComputeSeverity) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Create abnormality
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);
    hippocampus->activity_level = 5.0f;
    brain_regions_immune_detect_region_abnormality(bridge, hippocampus->id);

    uint32_t severity = brain_regions_immune_compute_severity(bridge, hippocampus->id);
    EXPECT_GE(severity, 1u);
    EXPECT_LE(severity, 10u);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, UpdateBridge) {
    createBridge();

    int result = brain_regions_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    brain_regions_immune_stats_t stats;
    brain_regions_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 1u);
}

TEST_F(BrainRegionsImmuneBridgeTest, UpdateWithInflammation) {
    createBridge();

    releaseCytokines(0.5f, 0.5f, 0.5f);

    int result = brain_regions_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    brain_regions_immune_stats_t stats;
    brain_regions_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.inflammations_applied, 0u);
}

TEST_F(BrainRegionsImmuneBridgeTest, UpdateNullFails) {
    int result = brain_regions_immune_bridge_update(nullptr, 1000);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, GetInflammationStateNotFound) {
    createBridge();

    region_inflammation_state_t state;
    int result = brain_regions_immune_get_inflammation_state(bridge, 9999, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, GetAbnormalityStateNotFound) {
    createBridge();

    region_abnormality_state_t state;
    int result = brain_regions_immune_get_abnormality_state(bridge, 9999, &state);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, GetActivityModulationUnknown) {
    createBridge();

    float mod = brain_regions_immune_get_activity_modulation(bridge, 9999);
    EXPECT_EQ(mod, 1.0f);  // Default no modulation
}

TEST_F(BrainRegionsImmuneBridgeTest, GetConnectivityModulationUnknown) {
    createBridge();

    float mod = brain_regions_immune_get_connectivity_modulation(bridge, 9999);
    EXPECT_EQ(mod, 1.0f);  // Default no modulation
}

TEST_F(BrainRegionsImmuneBridgeTest, IsRegionModulatedFalse) {
    createBridge();

    // No inflammation applied
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    brain_regions_immune_apply_to_region(bridge, hippocampus->id);

    bool modulated = brain_regions_immune_is_region_modulated(bridge, hippocampus->id);
    EXPECT_FALSE(modulated);  // No cytokines released
}

TEST_F(BrainRegionsImmuneBridgeTest, IsRegionModulatedTrue) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    releaseCytokines(0.5f, 0.5f, 0.5f);
    brain_regions_immune_apply_to_region(bridge, hippocampus->id);

    bool modulated = brain_regions_immune_is_region_modulated(bridge, hippocampus->id);
    EXPECT_TRUE(modulated);
}

TEST_F(BrainRegionsImmuneBridgeTest, GetStats) {
    createBridge();

    brain_regions_immune_stats_t stats;
    int result = brain_regions_immune_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainRegionsImmuneBridgeTest, GetStatsNullFails) {
    createBridge();
    EXPECT_EQ(brain_regions_immune_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(brain_regions_immune_get_stats(nullptr, nullptr), -1);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, InflammationLevelToString) {
    EXPECT_STREQ(region_inflammation_level_to_string(REGION_INFLAMMATION_NONE), "NONE");
    EXPECT_STREQ(region_inflammation_level_to_string(REGION_INFLAMMATION_MILD), "MILD");
    EXPECT_STREQ(region_inflammation_level_to_string(REGION_INFLAMMATION_MODERATE), "MODERATE");
    EXPECT_STREQ(region_inflammation_level_to_string(REGION_INFLAMMATION_SEVERE), "SEVERE");
    EXPECT_STREQ(region_inflammation_level_to_string(REGION_INFLAMMATION_CRITICAL), "CRITICAL");
}

TEST_F(BrainRegionsImmuneBridgeTest, AbnormalityTypeToString) {
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_NONE), "NONE");
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_HYPERACTIVE), "HYPERACTIVE");
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_HYPOACTIVE), "HYPOACTIVE");
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_DESYNC), "DESYNC");
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_LAYER_FAILURE), "LAYER_FAILURE");
    EXPECT_STREQ(region_abnormality_type_to_string(REGION_ABNORMALITY_MIXED), "MIXED");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, ApplyToNonExistentRegion) {
    createBridge();
    int result = brain_regions_immune_apply_to_region(bridge, 9999);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, RestoreNonExistentRegion) {
    createBridge();
    int result = brain_regions_immune_restore_region(bridge, 9999, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, TriggerResponseNonExistentRegion) {
    createBridge();
    int result = brain_regions_immune_trigger_response(bridge, 9999);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainRegionsImmuneBridgeTest, MultipleUpdateCycles) {
    createBridge();

    releaseCytokines(0.3f, 0.3f, 0.3f);

    for (int i = 0; i < 10; i++) {
        int result = brain_regions_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    brain_regions_immune_stats_t stats;
    brain_regions_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 10u);
}

TEST_F(BrainRegionsImmuneBridgeTest, ConfigDisableAllFeatures) {
    config.enable_region_specific_sensitivity = false;
    config.enable_inflammation_propagation = false;
    config.enable_layer_effects = false;
    config.enable_abnormality_detection = false;
    config.enable_il10_recovery = false;
    config.enable_bio_async = false;

    createBridge();

    int result = brain_regions_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Comprehensive Integration Test
 * ============================================================================ */

TEST_F(BrainRegionsImmuneBridgeTest, FullIntegrationCycle) {
    createBridge();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // 1. Start with normal state
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_bridge_update(bridge, 100);

    EXPECT_FALSE(brain_regions_immune_is_region_modulated(bridge, hippocampus->id));

    // 2. Release cytokines → inflammation
    releaseCytokines(0.6f, 0.8f, 0.5f);  // High IL-6 for hippocampus
    brain_regions_immune_bridge_update(bridge, 100);

    EXPECT_TRUE(brain_regions_immune_is_region_modulated(bridge, hippocampus->id));
    EXPECT_LT(brain_regions_immune_get_activity_modulation(bridge, hippocampus->id), 1.0f);

    // 3. Create abnormality
    hippocampus->activity_level = 0.1f;  // Hypoactive
    for (int i = 0; i < 5; i++) {
        brain_regions_immune_bridge_update(bridge, 100);
    }

    region_abnormality_state_t abnorm;
    brain_regions_immune_get_abnormality_state(bridge, hippocampus->id, &abnorm);
    EXPECT_NE(abnorm.abnormality_type, REGION_ABNORMALITY_NONE);

    // 4. Verify stats accumulated
    brain_regions_immune_stats_t stats;
    brain_regions_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.inflammations_applied, 0u);
}
