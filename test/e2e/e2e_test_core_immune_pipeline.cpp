/**
 * @file e2e_test_core_immune_pipeline.cpp
 * @brief End-to-End tests for Core Immune Pipeline
 * @date 2025-12-12
 *
 * Comprehensive pipeline tests simulating realistic scenarios:
 * - Full infection/immunity cycles
 * - Neural activity under metabolic stress
 * - Region-substrate-immune coordination
 * - Long-running simulation stability
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions_immune_bridge.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class CoreImmunePipelineE2E : public ::testing::Test {
protected:
    // Core components
    brain_module_t* brain_module = nullptr;
    neural_substrate_t* substrate = nullptr;
    brain_immune_system_t* immune_system = nullptr;

    // Bridges
    brain_regions_immune_bridge_t* regions_bridge = nullptr;
    substrate_immune_bridge_t* substrate_bridge = nullptr;
    uint32_t cytokine_id = 0;

    // Test regions
    brain_region_t* hippocampus = nullptr;
    brain_region_t* prefrontal = nullptr;
    brain_region_t* motor = nullptr;
    brain_region_t* thalamus = nullptr;

    void SetUp() override {
        // Create comprehensive brain module
        brain_module = brain_module_create(32);
        ASSERT_NE(brain_module, nullptr);

        // Create regions
        hippocampus = brain_region_create(REGION_HIPPOCAMPUS, 200);
        prefrontal = brain_region_create(REGION_PREFRONTAL, 200);
        motor = brain_region_create(REGION_MOTOR_M1, 150);
        thalamus = brain_region_create(REGION_THALAMUS, 100);

        ASSERT_NE(hippocampus, nullptr);
        ASSERT_NE(prefrontal, nullptr);
        ASSERT_NE(motor, nullptr);
        ASSERT_NE(thalamus, nullptr);

        brain_module_add_region(brain_module, hippocampus);
        brain_module_add_region(brain_module, prefrontal);
        brain_module_add_region(brain_module, motor);
        brain_module_add_region(brain_module, thalamus);

        // Connect regions (thalamic relay pattern)
        brain_module_connect_regions(brain_module, thalamus->id, hippocampus->id, 0.6f);
        brain_module_connect_regions(brain_module, thalamus->id, prefrontal->id, 0.6f);
        brain_module_connect_regions(brain_module, thalamus->id, motor->id, 0.5f);
        brain_module_connect_regions(brain_module, prefrontal->id, motor->id, 0.4f);

        // Create substrate with realistic configuration
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        sub_cfg.enable_metabolic_model = true;
        sub_cfg.enable_temperature_effects = true;
        sub_cfg.enable_ion_dynamics = true;
        sub_cfg.enable_alerts = true;
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create immune system
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        // Create bridges
        brain_regions_immune_config_t regions_cfg;
        brain_regions_immune_default_config(&regions_cfg);
        regions_bridge = brain_regions_immune_bridge_create(&regions_cfg, brain_module, immune_system);
        ASSERT_NE(regions_bridge, nullptr);

        substrate_immune_config_t substrate_cfg;
        substrate_immune_default_config(&substrate_cfg);
        substrate_bridge = substrate_immune_bridge_create(&substrate_cfg, substrate, immune_system);
        ASSERT_NE(substrate_bridge, nullptr);
    }

    void TearDown() override {
        if (regions_bridge) brain_regions_immune_bridge_destroy(regions_bridge);
        if (substrate_bridge) substrate_immune_bridge_destroy(substrate_bridge);
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
        if (substrate) substrate_destroy(substrate);
        if (brain_module) brain_module_destroy(brain_module);
    }

    // Helper: Simulate time step
    void simulateTimeStep(uint64_t ms) {
        substrate_update(substrate, ms);
        brain_regions_immune_bridge_update(regions_bridge, ms);
        substrate_immune_bridge_update(substrate_bridge, ms);
    }

    // Helper: Simulate neural activity
    void simulateNeuralActivity(uint32_t spikes, uint32_t transmissions) {
        substrate_record_spikes(substrate, spikes);
        substrate_record_transmissions(substrate, transmissions);
    }

    // Helper: Release cytokine cocktail
    void releaseInflammatoryCytokines(float intensity) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, intensity, 0);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, intensity * 0.9f, 0);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, intensity * 0.7f, 0);
    }

    // Helper: Release anti-inflammatory
    void releaseAntiInflammatory(float intensity) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, intensity, 0);
    }

    // Helper: Get system health summary
    struct SystemHealth {
        float substrate_capacity;
        float temperature;
        float atp;
        float hippocampus_activity_mod;
        float prefrontal_activity_mod;
        substrate_health_level_t health_level;
    };

    SystemHealth getSystemHealth() {
        SystemHealth health;
        health.substrate_capacity = substrate_get_capacity(substrate);
        health.temperature = substrate->physical.temperature;
        health.atp = substrate->metabolic.atp_level;
        health.hippocampus_activity_mod = brain_regions_immune_get_activity_modulation(
            regions_bridge, hippocampus->id);
        health.prefrontal_activity_mod = brain_regions_immune_get_activity_modulation(
            regions_bridge, prefrontal->id);
        health.health_level = substrate_get_health_level(substrate);
        return health;
    }
};

/* ============================================================================
 * E2E Scenario 1: Full Infection Cycle
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, FullInfectionImmunityCycle) {
    // === Phase 1: Baseline - Healthy State ===
    SystemHealth baseline = getSystemHealth();
    EXPECT_EQ(baseline.health_level, SUBSTRATE_HEALTH_OPTIMAL);
    EXPECT_FLOAT_EQ(baseline.temperature, SUBSTRATE_NORMAL_TEMPERATURE);
    EXPECT_GT(baseline.hippocampus_activity_mod, 0.95f);

    // === Phase 2: Pathogen Detection ===
    // Present antigen (simulating pathogen detection)
    uint8_t pathogen_epitope[] = {0xBA, 0xD0, 0xF0, 0x0D, 0xDE, 0xAD};
    uint32_t antigen_id;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  pathogen_epitope, sizeof(pathogen_epitope),
                                  7, 0, &antigen_id);

    // === Phase 3: Early Immune Response ===
    // Mild initial inflammation
    releaseInflammatoryCytokines(0.3f);

    for (int i = 0; i < 10; i++) {
        simulateNeuralActivity(100, 500);
        simulateTimeStep(100);
    }

    SystemHealth early_response = getSystemHealth();
    // Early: slight temperature increase, mild activity reduction
    EXPECT_GT(early_response.temperature, baseline.temperature);
    EXPECT_LT(early_response.hippocampus_activity_mod, baseline.hippocampus_activity_mod);

    // === Phase 4: Peak Inflammation (Sickness Behavior) ===
    // Full inflammatory response
    for (int i = 0; i < 5; i++) {
        releaseInflammatoryCytokines(0.7f);
    }

    for (int i = 0; i < 20; i++) {
        simulateNeuralActivity(50, 200);  // Reduced activity due to sickness
        simulateTimeStep(100);
    }

    SystemHealth peak_inflammation = getSystemHealth();
    // Peak: significant fever, metabolic stress, cognitive impairment
    EXPECT_GT(peak_inflammation.temperature, 38.0f);
    EXPECT_LT(peak_inflammation.atp, baseline.atp);
    EXPECT_GE(peak_inflammation.health_level, SUBSTRATE_HEALTH_STRESSED);

    // === Phase 5: Immune Clearance ===
    // B-cell activation and antibody production
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);
    brain_immune_t_help_b(immune_system, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune_system, b_cell_id, ANTIBODY_IGG, &antibody_id);
    brain_immune_neutralize(immune_system, antigen_id, antibody_id);

    // === Phase 6: Resolution Phase ===
    // Anti-inflammatory response
    for (int i = 0; i < 30; i++) {
        releaseAntiInflammatory(0.6f);
        substrate_immune_apply_il10_recovery(substrate_bridge, 0.5f);
        simulateNeuralActivity(80, 400);
        simulateTimeStep(200);
    }

    SystemHealth resolution = getSystemHealth();
    // Resolution: temperature decreasing, function recovering
    EXPECT_LT(resolution.temperature, peak_inflammation.temperature);
    EXPECT_GT(resolution.substrate_capacity, peak_inflammation.substrate_capacity);

    // === Phase 7: Recovery ===
    // Continued recovery with normal activity
    for (int i = 0; i < 50; i++) {
        simulateNeuralActivity(100, 500);
        simulateTimeStep(200);
    }

    SystemHealth recovered = getSystemHealth();
    // Recovery: return to near-baseline
    EXPECT_NEAR(recovered.temperature, baseline.temperature, 1.5f);
    EXPECT_GT(recovered.hippocampus_activity_mod, 0.85f);
}

/* ============================================================================
 * E2E Scenario 2: High Neural Load Under Stress
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, HighNeuralLoadUnderMetabolicStress) {
    // === Phase 1: Normal High Activity ===
    for (int i = 0; i < 20; i++) {
        simulateNeuralActivity(500, 2000);
        simulateTimeStep(50);
    }

    SystemHealth normal_load = getSystemHealth();
    // Normal high activity should be sustainable
    EXPECT_GT(normal_load.atp, 0.7f);
    EXPECT_EQ(normal_load.health_level, SUBSTRATE_HEALTH_OPTIMAL);

    // === Phase 2: Add Immune Stress ===
    releaseInflammatoryCytokines(0.5f);

    for (int i = 0; i < 30; i++) {
        simulateNeuralActivity(500, 2000);  // Maintain high activity
        simulateTimeStep(50);
    }

    SystemHealth stressed_load = getSystemHealth();
    // Immune + neural stress should deplete resources faster
    EXPECT_LT(stressed_load.atp, normal_load.atp);
    EXPECT_GE(stressed_load.health_level, SUBSTRATE_HEALTH_STRESSED);

    // === Phase 3: Extreme Combined Stress ===
    for (int i = 0; i < 5; i++) {
        releaseInflammatoryCytokines(0.8f);
    }

    for (int i = 0; i < 50; i++) {
        simulateNeuralActivity(1000, 5000);  // Very high activity
        simulateTimeStep(50);
    }

    SystemHealth extreme_stress = getSystemHealth();
    // Should be significantly compromised
    EXPECT_GE(extreme_stress.health_level, SUBSTRATE_HEALTH_COMPROMISED);
    EXPECT_LT(extreme_stress.substrate_capacity, 0.7f);

    // === Phase 4: Recovery with Reduced Activity ===
    for (int i = 0; i < 100; i++) {
        releaseAntiInflammatory(0.3f);
        simulateNeuralActivity(20, 100);  // Low activity for recovery
        simulateTimeStep(100);
    }

    SystemHealth recovering = getSystemHealth();
    // Should show recovery
    EXPECT_GT(recovering.atp, extreme_stress.atp);
    EXPECT_LT(recovering.health_level, extreme_stress.health_level);
}

/* ============================================================================
 * E2E Scenario 3: Region-Specific Immune Effects
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, RegionSpecificImmuneResponse) {
    // === Phase 1: Baseline Region Activity ===
    for (int i = 0; i < 10; i++) {
        simulateTimeStep(100);
    }

    float baseline_hippocampus = brain_regions_immune_get_activity_modulation(
        regions_bridge, hippocampus->id);
    float baseline_prefrontal = brain_regions_immune_get_activity_modulation(
        regions_bridge, prefrontal->id);

    // === Phase 2: IL-6 Release (Hippocampus Sensitive) ===
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.7f, 0, &cytokine_id);

    for (int i = 0; i < 20; i++) {
        simulateTimeStep(100);
    }

    float il6_hippocampus = brain_regions_immune_get_activity_modulation(
        regions_bridge, hippocampus->id);
    float il6_prefrontal = brain_regions_immune_get_activity_modulation(
        regions_bridge, prefrontal->id);

    // Hippocampus should be more affected by IL-6
    float hippocampus_reduction = baseline_hippocampus - il6_hippocampus;
    float prefrontal_reduction = baseline_prefrontal - il6_prefrontal;
    EXPECT_GT(hippocampus_reduction, prefrontal_reduction * 0.8f);

    // === Phase 3: IL-1β Release (Prefrontal Sensitive) ===
    // Reset effects
    for (int i = 0; i < 30; i++) {
        releaseAntiInflammatory(0.5f);
        simulateTimeStep(100);
    }

    float reset_hippocampus = brain_regions_immune_get_activity_modulation(
        regions_bridge, hippocampus->id);
    float reset_prefrontal = brain_regions_immune_get_activity_modulation(
        regions_bridge, prefrontal->id);

    // Now release IL-1β
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.7f, 0, &cytokine_id);

    for (int i = 0; i < 20; i++) {
        simulateTimeStep(100);
    }

    float il1_hippocampus = brain_regions_immune_get_activity_modulation(
        regions_bridge, hippocampus->id);
    float il1_prefrontal = brain_regions_immune_get_activity_modulation(
        regions_bridge, prefrontal->id);

    // Prefrontal should be more affected by IL-1β
    hippocampus_reduction = reset_hippocampus - il1_hippocampus;
    prefrontal_reduction = reset_prefrontal - il1_prefrontal;
    EXPECT_GT(prefrontal_reduction, hippocampus_reduction * 0.8f);
}

/* ============================================================================
 * E2E Scenario 4: Cytokine Storm
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, CytokineStormAndRecovery) {
    // === Phase 1: Trigger Cytokine Storm ===
    for (int i = 0; i < 10; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 1.0f, 0, &cytokine_id);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 1.0f, 0, &cytokine_id);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cytokine_id);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IFN_GAMMA, 0, 1.0f, 0, &cytokine_id);
    }

    for (int i = 0; i < 20; i++) {
        simulateTimeStep(100);
    }

    SystemHealth storm = getSystemHealth();
    // Severe effects
    EXPECT_GE(storm.health_level, SUBSTRATE_HEALTH_COMPROMISED);
    EXPECT_LT(storm.substrate_capacity, 0.6f);

    // Temperature should be maxed
    substrate_immune_config_t cfg;
    substrate_immune_default_config(&cfg);
    EXPECT_LE(storm.temperature, cfg.max_fever_temperature + 0.5f);

    // === Phase 2: Emergency Anti-Inflammatory ===
    for (int i = 0; i < 50; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL10, 0, 1.0f, 0, &cytokine_id);
        substrate_immune_apply_il10_recovery(substrate_bridge, 0.8f);
        simulateTimeStep(100);
    }

    // === Phase 3: Extended Recovery ===
    for (int i = 0; i < 100; i++) {
        releaseAntiInflammatory(0.3f);
        simulateTimeStep(200);
    }

    SystemHealth post_storm = getSystemHealth();
    // Should be recovering
    EXPECT_LT(post_storm.temperature, storm.temperature);
    EXPECT_GT(post_storm.substrate_capacity, storm.substrate_capacity);
}

/* ============================================================================
 * E2E Scenario 5: Long-Running Stability
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, LongRunningStabilityTest) {
    std::vector<float> temperature_history;
    std::vector<float> atp_history;
    std::vector<float> capacity_history;

    // Run for simulated hours with varying conditions
    for (int hour = 0; hour < 10; hour++) {
        // Each "hour" = 360 steps of 10ms = 3.6 seconds real-time equivalent
        for (int step = 0; step < 360; step++) {
            // Periodic mild inflammation (circadian-like)
            if (step % 60 == 0) {
                float inflammation = 0.2f + 0.1f * sin(hour * 0.5f);
                releaseInflammatoryCytokines(inflammation);
            }

            // Periodic anti-inflammatory
            if (step % 90 == 0) {
                releaseAntiInflammatory(0.2f);
            }

            // Variable neural activity
            uint32_t activity = 100 + static_cast<uint32_t>(50 * sin(step * 0.1f));
            simulateNeuralActivity(activity, activity * 5);

            simulateTimeStep(10);
        }

        // Record state at end of each "hour"
        SystemHealth hourly = getSystemHealth();
        temperature_history.push_back(hourly.temperature);
        atp_history.push_back(hourly.atp);
        capacity_history.push_back(hourly.substrate_capacity);
    }

    // === Verify Stability ===
    // Temperature should oscillate but stay within bounds
    for (float temp : temperature_history) {
        EXPECT_GE(temp, 35.0f);
        EXPECT_LE(temp, 40.0f);
    }

    // ATP should remain viable
    for (float atp : atp_history) {
        EXPECT_GE(atp, 0.3f);
    }

    // Capacity should remain reasonable
    for (float cap : capacity_history) {
        EXPECT_GE(cap, 0.4f);
    }

    // Check statistics for consistency
    brain_regions_immune_stats_t regions_stats;
    brain_regions_immune_get_stats(regions_bridge, &regions_stats);
    EXPECT_GT(regions_stats.total_updates, 3000u);

    substrate_stats_t substrate_stats;
    substrate_get_stats(substrate, &substrate_stats);
    EXPECT_GT(substrate_stats.total_updates, 3000u);
}

/* ============================================================================
 * E2E Scenario 6: Abnormality Detection Pipeline
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, AbnormalityDetectionAndResponse) {
    // === Phase 1: Normal Activity ===
    for (int i = 0; i < 20; i++) {
        hippocampus->activity_level = 1.0f;
        prefrontal->activity_level = 1.0f;
        simulateTimeStep(100);
    }

    // === Phase 2: Induce Hyperactivity (Seizure-like) ===
    for (int i = 0; i < 10; i++) {
        hippocampus->activity_level = 5.0f;  // Hyperactive
        brain_regions_immune_detect_region_abnormality(regions_bridge, hippocampus->id);
        simulateTimeStep(100);
    }

    // Should detect abnormality
    region_abnormality_type_t abnorm_type =
        brain_regions_immune_detect_region_abnormality(regions_bridge, hippocampus->id);
    EXPECT_EQ(abnorm_type, REGION_ABNORMALITY_HYPERACTIVE);

    // === Phase 3: Trigger Immune Response ===
    int trigger_result = brain_regions_immune_trigger_response(regions_bridge, hippocampus->id);
    EXPECT_EQ(trigger_result, 0);

    // Process response
    for (int i = 0; i < 20; i++) {
        simulateTimeStep(100);
    }

    // Region should be modulated
    bool is_modulated = brain_regions_immune_is_region_modulated(regions_bridge, hippocampus->id);
    EXPECT_TRUE(is_modulated);

    // === Phase 4: Activity Normalization ===
    for (int i = 0; i < 30; i++) {
        hippocampus->activity_level = 1.0f;  // Return to normal
        simulateTimeStep(100);
    }

    // === Phase 5: Induce Hypoactivity ===
    for (int i = 0; i < 10; i++) {
        prefrontal->activity_level = 0.1f;  // Hypoactive
        brain_regions_immune_detect_region_abnormality(regions_bridge, prefrontal->id);
        simulateTimeStep(100);
    }

    abnorm_type = brain_regions_immune_detect_region_abnormality(regions_bridge, prefrontal->id);
    EXPECT_EQ(abnorm_type, REGION_ABNORMALITY_HYPOACTIVE);
}

/* ============================================================================
 * E2E Scenario 7: Substrate-Triggered Immune Response
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, SubstrateStressTriggersImmune) {
    // === Phase 1: Exhaust Substrate Resources ===
    for (int i = 0; i < 100; i++) {
        simulateNeuralActivity(1000, 5000);  // Very high activity
        simulateTimeStep(50);
    }

    // Should have depleted resources
    SystemHealth depleted = getSystemHealth();
    EXPECT_LT(depleted.atp, 0.5f);

    // === Phase 2: Force Critical State ===
    substrate_set_atp(substrate, 0.15f);
    substrate_set_membrane_integrity(substrate, 0.4f);
    substrate_update(substrate, 10);

    // === Phase 3: Check Stress Detection ===
    for (int i = 0; i < 5; i++) {
        bool stress = substrate_immune_check_stress(substrate_bridge);
        simulateTimeStep(100);
    }

    bool should_trigger = substrate_immune_check_stress(substrate_bridge);
    EXPECT_TRUE(should_trigger);

    // === Phase 4: Trigger and Verify Response ===
    substrate_immune_trigger_response(substrate_bridge);

    // Process response cycles
    for (int i = 0; i < 30; i++) {
        simulateTimeStep(100);
    }

    // Immune should be activated
    EXPECT_TRUE(substrate_bridge->trigger_state.immune_triggered);
}

/* ============================================================================
 * E2E Scenario 8: Complete System Coordination
 * ============================================================================ */

TEST_F(CoreImmunePipelineE2E, CompleteSystemCoordination) {
    // This test verifies that all components work together correctly

    // === Initial State ===
    SystemHealth initial = getSystemHealth();
    EXPECT_EQ(initial.health_level, SUBSTRATE_HEALTH_OPTIMAL);

    // === Simulate Complex Scenario ===
    // Day 1: Normal activity
    for (int i = 0; i < 100; i++) {
        simulateNeuralActivity(100 + i % 50, 500 + i % 200);
        simulateTimeStep(100);
    }

    // Day 2: Mild infection
    uint8_t mild_pathogen[] = {0x01, 0x02, 0x03};
    uint32_t mild_antigen;
    brain_immune_present_antigen(immune_system, ANTIGEN_SOURCE_MANUAL,
                                  mild_pathogen, 3, 4, 0, &mild_antigen);
    releaseInflammatoryCytokines(0.4f);

    for (int i = 0; i < 50; i++) {
        simulateNeuralActivity(80, 400);
        simulateTimeStep(100);
    }

    // Day 3: Resolution
    for (int i = 0; i < 50; i++) {
        releaseAntiInflammatory(0.3f);
        simulateNeuralActivity(90, 450);
        simulateTimeStep(100);
    }

    // Day 4: High workload (cognitive task)
    for (int i = 0; i < 50; i++) {
        simulateNeuralActivity(200, 1000);
        simulateTimeStep(100);
    }

    // Day 5: Rest
    for (int i = 0; i < 50; i++) {
        simulateNeuralActivity(30, 150);
        simulateTimeStep(200);
    }

    // === Verify Final State ===
    SystemHealth final_state = getSystemHealth();

    // Should be in reasonable state after mixed activity
    EXPECT_GE(final_state.atp, 0.5f);
    EXPECT_LE(final_state.health_level, SUBSTRATE_HEALTH_COMPROMISED);

    // === Verify Statistics ===
    substrate_stats_t sub_stats;
    substrate_get_stats(substrate, &sub_stats);
    EXPECT_GT(sub_stats.total_updates, 300u);
    EXPECT_GT(sub_stats.spikes_processed, 10000u);

    brain_regions_immune_stats_t reg_stats;
    brain_regions_immune_get_stats(regions_bridge, &reg_stats);
    EXPECT_GT(reg_stats.total_updates, 300u);

    substrate_immune_stats_t sub_imm_stats;
    substrate_immune_get_stats(substrate_bridge, &sub_imm_stats);
    EXPECT_GT(sub_imm_stats.total_updates, 300u);
}
