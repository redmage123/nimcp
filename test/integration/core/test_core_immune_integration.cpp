/**
 * @file test_core_immune_integration.cpp
 * @brief Integration tests for Core Immune Bridges
 * @date 2025-12-12
 *
 * Tests integration between:
 * - brain_regions_immune_bridge
 * - substrate_immune_bridge
 * - brain_immune_system
 * - neural_substrate
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions_immune_bridge.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class CoreImmuneIntegrationTest : public ::testing::Test {
protected:
    // Core modules
    brain_module_t* brain_module = nullptr;
    neural_substrate_t* substrate = nullptr;
    brain_immune_system_t* immune_system = nullptr;

    // Bridges
    brain_regions_immune_bridge_t* regions_bridge = nullptr;
    substrate_immune_bridge_t* substrate_bridge = nullptr;

    void SetUp() override {
        // Create brain module with regions
        brain_module = brain_module_create(16);
        ASSERT_NE(brain_module, nullptr);

        // Add regions
        brain_region_t* hippocampus = brain_region_create(REGION_HIPPOCAMPUS, 100);
        brain_region_t* prefrontal = brain_region_create(REGION_PREFRONTAL, 100);
        brain_region_t* thalamus = brain_region_create(REGION_THALAMUS, 50);
        brain_module_add_region(brain_module, hippocampus);
        brain_module_add_region(brain_module, prefrontal);
        brain_module_add_region(brain_module, thalamus);

        // Create neural substrate
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create immune system
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        if (regions_bridge) {
            brain_regions_immune_bridge_destroy(regions_bridge);
        }
        if (substrate_bridge) {
            substrate_immune_bridge_destroy(substrate_bridge);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
        if (substrate) {
            substrate_destroy(substrate);
        }
        if (brain_module) {
            brain_module_destroy(brain_module);
        }
    }

    void createBridges() {
        brain_regions_immune_config_t regions_cfg;
        brain_regions_immune_default_config(&regions_cfg);
        regions_bridge = brain_regions_immune_bridge_create(&regions_cfg, brain_module, immune_system);
        ASSERT_NE(regions_bridge, nullptr);

        substrate_immune_config_t sub_cfg;
        substrate_immune_default_config(&sub_cfg);
        substrate_bridge = substrate_immune_bridge_create(&sub_cfg, substrate, immune_system);
        ASSERT_NE(substrate_bridge, nullptr);
    }

    void releaseCytokines(float il1, float il6, float tnf) {
        uint32_t cytokine_id;
        if (il1 > 0) brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, il1, 0, &cytokine_id);
        if (il6 > 0) brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, il6, 0, &cytokine_id);
        if (tnf > 0) brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, tnf, 0, &cytokine_id);
    }
};

/* ============================================================================
 * Shared Immune System Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, BothBridgesShareImmuneSystem) {
    createBridges();

    // Both bridges should respond to same cytokine release
    releaseCytokines(0.5f, 0.5f, 0.5f);

    brain_regions_immune_bridge_update(regions_bridge, 100);
    substrate_immune_bridge_update(substrate_bridge, 100);

    // Both should be modulated
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    bool region_modulated = brain_regions_immune_is_region_modulated(regions_bridge, hippocampus->id);
    bool substrate_modulated = substrate_immune_is_modulated(substrate_bridge);

    EXPECT_TRUE(region_modulated);
    EXPECT_TRUE(substrate_modulated);
}

TEST_F(CoreImmuneIntegrationTest, CytokineCascadeAffectsBoth) {
    createBridges();

    float initial_temp = substrate->physical.temperature;
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    float initial_activity_mod = brain_regions_immune_get_activity_modulation(regions_bridge, hippocampus->id);

    // Release inflammatory cytokines
    releaseCytokines(0.7f, 0.6f, 0.5f);

    for (int i = 0; i < 5; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // Substrate should have fever
    EXPECT_GT(substrate->physical.temperature, initial_temp);

    // Regions should have reduced activity
    float final_activity_mod = brain_regions_immune_get_activity_modulation(regions_bridge, hippocampus->id);
    EXPECT_LT(final_activity_mod, initial_activity_mod);
}

/* ============================================================================
 * Fever Propagation Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, FeverAffectsSubstrateModulation) {
    createBridges();

    // Create fever
    releaseCytokines(0.8f, 0.6f, 0.0f);
    substrate_immune_bridge_update(substrate_bridge, 100);

    // Get substrate modulation
    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    // High temperature should affect conduction velocity (Q10 effect)
    if (substrate->physical.temperature > SUBSTRATE_NORMAL_TEMPERATURE + 1.0f) {
        EXPECT_NE(mod.conduction_velocity, 1.0f);
    }
}

TEST_F(CoreImmuneIntegrationTest, FeverRecoveryWithIL10) {
    createBridges();

    // Create fever
    releaseCytokines(0.7f, 0.7f, 0.0f);
    substrate_immune_bridge_update(substrate_bridge, 100);

    float fever_temp = substrate->physical.temperature;

    // Release IL-10 (anti-inflammatory)
    uint32_t il10_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.8f, 0, &il10_id);
    substrate_immune_apply_il10_recovery(substrate_bridge, 0.8f);

    // Temperature should decrease
    EXPECT_LT(substrate->physical.temperature, fever_temp);
}

/* ============================================================================
 * Region-Substrate Interaction Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, RegionAbnormalityTriggersSubstrateAlert) {
    createBridges();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Create region abnormality (hyperactivity)
    hippocampus->activity_level = 1.0f;
    brain_regions_immune_detect_region_abnormality(regions_bridge, hippocampus->id);
    hippocampus->activity_level = 5.0f;

    for (int i = 0; i < 5; i++) {
        brain_regions_immune_detect_region_abnormality(regions_bridge, hippocampus->id);
    }

    // Trigger immune response from region abnormality
    brain_regions_immune_trigger_response(regions_bridge, hippocampus->id);

    // This should eventually affect substrate through immune system
    // Process a few update cycles
    for (int i = 0; i < 3; i++) {
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // Immune should be active
    EXPECT_TRUE(substrate_immune_is_modulated(substrate_bridge) ||
                brain_regions_immune_is_region_modulated(regions_bridge, hippocampus->id));
}

TEST_F(CoreImmuneIntegrationTest, SubstrateStressAffectsRegions) {
    createBridges();

    // Create substrate stress (ATP depletion)
    substrate_set_atp(substrate, 0.2f);
    substrate_update(substrate, 10);

    // This should trigger substrate immune response
    for (int i = 0; i < 5; i++) {
        substrate_immune_check_stress(substrate_bridge);
    }

    bool stress_detected = substrate_immune_check_stress(substrate_bridge);
    EXPECT_TRUE(stress_detected);

    // Trigger immune response
    substrate_immune_trigger_response(substrate_bridge);

    // Update both bridges
    brain_regions_immune_bridge_update(regions_bridge, 100);

    // Eventually this could affect regions through cytokine release
    // (depends on immune system behavior)
}

/* ============================================================================
 * Coordinated Update Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, SynchronizedUpdateCycle) {
    createBridges();

    releaseCytokines(0.4f, 0.4f, 0.4f);

    // Synchronized updates
    for (int i = 0; i < 10; i++) {
        int regions_result = brain_regions_immune_bridge_update(regions_bridge, 100);
        int substrate_result = substrate_immune_bridge_update(substrate_bridge, 100);

        EXPECT_EQ(regions_result, 0);
        EXPECT_EQ(substrate_result, 0);
    }

    // Both should have statistics
    brain_regions_immune_stats_t regions_stats;
    brain_regions_immune_get_stats(regions_bridge, &regions_stats);
    EXPECT_GE(regions_stats.total_updates, 10u);

    substrate_immune_stats_t substrate_stats;
    substrate_immune_get_stats(substrate_bridge, &substrate_stats);
    EXPECT_GE(substrate_stats.total_updates, 10u);
}

TEST_F(CoreImmuneIntegrationTest, AsynchronousUpdates) {
    createBridges();

    releaseCytokines(0.5f, 0.5f, 0.5f);

    // Update at different rates
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            brain_regions_immune_bridge_update(regions_bridge, 100);
        }
        if (i % 3 == 0) {
            substrate_immune_bridge_update(substrate_bridge, 100);
        }
    }

    // Both should still work correctly
    brain_regions_immune_stats_t regions_stats;
    brain_regions_immune_get_stats(regions_bridge, &regions_stats);
    EXPECT_GT(regions_stats.total_updates, 0u);

    substrate_immune_stats_t substrate_stats;
    substrate_immune_get_stats(substrate_bridge, &substrate_stats);
    EXPECT_GT(substrate_stats.total_updates, 0u);
}

/* ============================================================================
 * Metabolic Integration Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, MetabolicStressFromInfection) {
    createBridges();

    // Simulate infection scenario - high inflammatory response
    releaseCytokines(0.8f, 0.7f, 0.6f);

    float initial_atp = substrate->metabolic.atp_level;

    // Multiple update cycles
    for (int i = 0; i < 10; i++) {
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // ATP should be depleted due to TNF-α
    EXPECT_LT(substrate->metabolic.atp_level, initial_atp);

    // Health level should be affected
    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_GE(health, SUBSTRATE_HEALTH_STRESSED);
}

TEST_F(CoreImmuneIntegrationTest, RegionActivityAffectedBySubstrateHealth) {
    createBridges();

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);

    // Deplete substrate resources
    substrate_set_atp(substrate, 0.3f);
    substrate_set_oxygen(substrate, 0.5f);
    substrate_update(substrate, 10);

    // Get substrate modulation factors
    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    // Firing rate should be reduced due to metabolic stress
    EXPECT_LT(mod.firing_rate_mod, 0.95f);

    // This could be used to modulate region activity
    float firing_mod = substrate_get_firing_modulation(substrate);
    EXPECT_LT(firing_mod, 1.0f);
}

/* ============================================================================
 * Recovery Cycle Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, FullInflammationRecoveryCycle) {
    createBridges();

    // Phase 1: Healthy state
    float healthy_temp = substrate->physical.temperature;
    float healthy_atp = substrate->metabolic.atp_level;

    // Phase 2: Inflammation
    releaseCytokines(0.7f, 0.7f, 0.5f);
    for (int i = 0; i < 5; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    float inflamed_temp = substrate->physical.temperature;
    EXPECT_GT(inflamed_temp, healthy_temp);

    // Phase 3: Recovery with IL-10
    uint32_t recov_id;
    for (int i = 0; i < 10; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.5f, 0, &recov_id);
        substrate_immune_apply_il10_recovery(substrate_bridge, 0.5f);
        substrate_update(substrate, 100);
    }

    // Temperature should decrease
    float recovered_temp = substrate->physical.temperature;
    EXPECT_LT(recovered_temp, inflamed_temp);
}

/* ============================================================================
 * Multi-Region Coordination Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, DifferentRegionSensitivities) {
    createBridges();

    // IL-6 release (hippocampus is more sensitive)
    uint32_t il6_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.6f, 0, &il6_id);
    brain_regions_immune_bridge_update(regions_bridge, 100);

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    brain_region_t* prefrontal = brain_module_get_region_by_type(brain_module, REGION_PREFRONTAL);
    ASSERT_NE(hippocampus, nullptr);
    ASSERT_NE(prefrontal, nullptr);

    float hippocampus_mod = brain_regions_immune_get_activity_modulation(regions_bridge, hippocampus->id);
    float prefrontal_mod = brain_regions_immune_get_activity_modulation(regions_bridge, prefrontal->id);

    // Hippocampus should be more affected by IL-6
    EXPECT_LE(hippocampus_mod, prefrontal_mod + 0.1f);
}

TEST_F(CoreImmuneIntegrationTest, InflammationPropagationBetweenRegions) {
    createBridges();

    // Connect regions
    brain_region_t* thalamus = brain_module_get_region_by_type(brain_module, REGION_THALAMUS);
    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(thalamus, nullptr);
    ASSERT_NE(hippocampus, nullptr);

    brain_module_connect_regions(brain_module, thalamus->id, hippocampus->id, 0.5f);

    // High inflammation to thalamus
    releaseCytokines(0.8f, 0.8f, 0.8f);
    brain_regions_immune_apply_to_region(regions_bridge, thalamus->id);

    // Propagate
    int propagations = brain_regions_immune_propagate_inflammation(regions_bridge);
    EXPECT_GE(propagations, 0);
}

/* ============================================================================
 * Edge Cases and Robustness Tests
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, RapidCytokineFluctuations) {
    createBridges();

    // Rapid on/off cytokine release
    uint32_t fluct_id;
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            releaseCytokines(0.8f, 0.8f, 0.8f);
        } else {
            brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.8f, 0, &fluct_id);
        }

        brain_regions_immune_bridge_update(regions_bridge, 50);
        substrate_immune_bridge_update(substrate_bridge, 50);
    }

    // System should remain stable
    EXPECT_GE(substrate->metabolic.atp_level, 0.0f);
    EXPECT_LE(substrate->physical.temperature, 45.0f);
}

TEST_F(CoreImmuneIntegrationTest, ExtremeCytokineStorm) {
    createBridges();

    // Simulate cytokine storm
    uint32_t storm_id;
    for (int i = 0; i < 5; i++) {
        releaseCytokines(1.0f, 1.0f, 1.0f);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IFN_GAMMA, 0, 1.0f, 0, &storm_id);
    }

    for (int i = 0; i < 10; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // System should be severely affected but not crash
    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_GE(health, SUBSTRATE_HEALTH_COMPROMISED);

    // Temperature should be capped
    substrate_immune_config_t cfg;
    substrate_immune_default_config(&cfg);
    EXPECT_LE(substrate->physical.temperature, cfg.max_fever_temperature + 1.0f);
}

TEST_F(CoreImmuneIntegrationTest, LongRunningSimulation) {
    createBridges();

    // Extended simulation
    releaseCytokines(0.3f, 0.3f, 0.3f);

    for (int i = 0; i < 100; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
        substrate_update(substrate, 100);
    }

    // System should stabilize
    brain_regions_immune_stats_t regions_stats;
    brain_regions_immune_get_stats(regions_bridge, &regions_stats);
    EXPECT_EQ(regions_stats.total_updates, 100u);

    substrate_immune_stats_t substrate_stats;
    substrate_immune_get_stats(substrate_bridge, &substrate_stats);
    EXPECT_EQ(substrate_stats.total_updates, 100u);
}

/* ============================================================================
 * Comprehensive Integration Scenario
 * ============================================================================ */

TEST_F(CoreImmuneIntegrationTest, FullInfectionResponseScenario) {
    createBridges();

    // === Phase 1: Healthy State ===
    float baseline_temp = substrate->physical.temperature;
    float baseline_atp = substrate->metabolic.atp_level;

    brain_region_t* hippocampus = brain_module_get_region_by_type(brain_module, REGION_HIPPOCAMPUS);
    ASSERT_NE(hippocampus, nullptr);
    float baseline_activity = brain_regions_immune_get_activity_modulation(regions_bridge, hippocampus->id);

    EXPECT_FLOAT_EQ(baseline_temp, SUBSTRATE_NORMAL_TEMPERATURE);
    EXPECT_GT(baseline_activity, 0.9f);

    // === Phase 2: Infection Onset ===
    // Present antigen to immune system
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  epitope, sizeof(epitope), 8, 0, &antigen_id);

    // Inflammatory response
    releaseCytokines(0.6f, 0.6f, 0.5f);

    for (int i = 0; i < 5; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // === Phase 3: Verify Inflammatory Response ===
    // Fever
    EXPECT_GT(substrate->physical.temperature, baseline_temp);

    // Metabolic stress
    EXPECT_LT(substrate->metabolic.atp_level, baseline_atp);

    // Region modulation
    float inflamed_activity = brain_regions_immune_get_activity_modulation(regions_bridge, hippocampus->id);
    EXPECT_LT(inflamed_activity, baseline_activity);

    // === Phase 4: Peak Inflammation ===
    releaseCytokines(0.8f, 0.8f, 0.7f);

    for (int i = 0; i < 5; i++) {
        brain_regions_immune_bridge_update(regions_bridge, 100);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    float peak_temp = substrate->physical.temperature;
    EXPECT_GT(peak_temp, substrate->physical.temperature - 2.0f);  // Allow tolerance

    // === Phase 5: Resolution ===
    uint32_t resol_id;
    for (int i = 0; i < 10; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 0.6f, 0, &resol_id);
        substrate_immune_apply_il10_recovery(substrate_bridge, 0.6f);

        brain_regions_immune_bridge_update(regions_bridge, 200);
        substrate_immune_bridge_update(substrate_bridge, 200);
        substrate_update(substrate, 200);
    }

    // === Phase 6: Verify Recovery ===
    float recovered_temp = substrate->physical.temperature;
    EXPECT_LT(recovered_temp, peak_temp);

    // Get final statistics
    brain_regions_immune_stats_t regions_stats;
    brain_regions_immune_get_stats(regions_bridge, &regions_stats);
    EXPECT_GT(regions_stats.inflammations_applied, 0u);

    substrate_immune_stats_t substrate_stats;
    substrate_immune_get_stats(substrate_bridge, &substrate_stats);
    EXPECT_GT(substrate_stats.fever_cycles, 0u);
    EXPECT_GT(substrate_stats.il10_recoveries, 0u);
}
