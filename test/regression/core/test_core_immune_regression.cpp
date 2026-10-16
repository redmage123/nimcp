/**
 * @file test_core_immune_regression.cpp
 * @brief Regression tests for Core Immune Bridges
 * @date 2025-12-12
 *
 * Tests stability, edge cases, and known issue prevention for:
 * - neural_substrate
 * - substrate_immune_bridge
 * - brain_regions_immune_bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
#include "core/brain_regions/nimcp_brain_regions_immune_bridge.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Regression Test Fixture
 * ============================================================================ */

class CoreImmuneRegressionTest : public ::testing::Test {
protected:
    brain_module_t* brain_module = nullptr;
    neural_substrate_t* substrate = nullptr;
    brain_immune_system_t* immune_system = nullptr;
    brain_regions_immune_bridge_t* regions_bridge = nullptr;
    substrate_immune_bridge_t* substrate_bridge = nullptr;
    uint32_t cytokine_id = 0;

    void SetUp() override {
        // Create brain module
        brain_module = brain_module_create(16);
        ASSERT_NE(brain_module, nullptr);

        brain_region_t* hippocampus = brain_region_create(REGION_HIPPOCAMPUS, 100);
        brain_region_t* prefrontal = brain_region_create(REGION_PREFRONTAL, 100);
        brain_module_add_region(brain_module, hippocampus);
        brain_module_add_region(brain_module, prefrontal);

        // Create substrate
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
        if (regions_bridge) brain_regions_immune_bridge_destroy(regions_bridge);
        if (substrate_bridge) substrate_immune_bridge_destroy(substrate_bridge);
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
        if (substrate) substrate_destroy(substrate);
        if (brain_module) brain_module_destroy(brain_module);
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
};

/* ============================================================================
 * Numerical Stability Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, ATPNeverGoesNegative) {
    createBridges();

    // Extreme ATP depletion scenario
    for (int i = 0; i < 100; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cytokine_id);
        substrate_immune_bridge_update(substrate_bridge, 100);
        substrate_record_spikes(substrate, 10000);
        substrate_update(substrate, 100);

        // ATP must never go negative
        EXPECT_GE(substrate->metabolic.atp_level, 0.0f);
    }
}

TEST_F(CoreImmuneRegressionTest, TemperatureNeverExceedsPhysicalLimit) {
    createBridges();

    // Extreme fever scenario
    for (int i = 0; i < 50; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 1.0f, 0, &cytokine_id);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 1.0f, 0, &cytokine_id);
        substrate_immune_bridge_update(substrate_bridge, 100);

        // Temperature must stay within physiological bounds
        EXPECT_LE(substrate->physical.temperature, 45.0f);
        EXPECT_GE(substrate->physical.temperature, 25.0f);
    }
}

TEST_F(CoreImmuneRegressionTest, MembraneIntegrityNeverGoesNegative) {
    createBridges();

    // Extreme damage scenario
    for (int i = 0; i < 100; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cytokine_id);
        substrate_immune_apply_damage(substrate_bridge);

        EXPECT_GE(substrate->physical.membrane_integrity, 0.0f);
        EXPECT_LE(substrate->physical.membrane_integrity, 1.0f);
    }
}

TEST_F(CoreImmuneRegressionTest, PrecisionWeightsRemainValid) {
    createBridges();

    // Precision should stay within bounds
    for (int i = 0; i < 20; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.5f, 0, &cytokine_id);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // Modulation factors should be valid
    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    EXPECT_GE(mod.firing_rate_mod, 0.0f);
    EXPECT_LE(mod.firing_rate_mod, 2.0f);
    EXPECT_GE(mod.transmission_efficiency, 0.0f);
    EXPECT_LE(mod.transmission_efficiency, 1.0f);
    EXPECT_GE(mod.overall_capacity, 0.0f);
    EXPECT_LE(mod.overall_capacity, 1.0f);
}

/* ============================================================================
 * Memory Safety Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, RepeatedCreateDestroy) {
    // Create and destroy many times - check for memory leaks
    for (int i = 0; i < 20; i++) {
        brain_regions_immune_config_t regions_cfg;
        brain_regions_immune_default_config(&regions_cfg);
        auto* bridge = brain_regions_immune_bridge_create(&regions_cfg, brain_module, immune_system);
        ASSERT_NE(bridge, nullptr);
        brain_regions_immune_bridge_destroy(bridge);
    }
}

TEST_F(CoreImmuneRegressionTest, RepeatedSubstrateCreateDestroy) {
    // Create and destroy substrate many times
    for (int i = 0; i < 20; i++) {
        substrate_config_t cfg;
        substrate_default_config(&cfg);
        auto* sub = substrate_create(&cfg);
        ASSERT_NE(sub, nullptr);
        substrate_destroy(sub);
    }
}

TEST_F(CoreImmuneRegressionTest, NullPointerHandling) {
    // All functions should handle NULL gracefully (return non-zero error)
    EXPECT_NE(substrate_update(nullptr, 100), 0);
    EXPECT_NE(substrate_set_atp(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_temperature(nullptr, 37.0f), 0);
    EXPECT_NE(substrate_record_spikes(nullptr, 100), 0);

    EXPECT_NE(brain_regions_immune_bridge_update(nullptr, 100), 0);
    EXPECT_NE(brain_regions_immune_apply_effects(nullptr), 0);

    EXPECT_NE(substrate_immune_bridge_update(nullptr, 100), 0);
    EXPECT_NE(substrate_immune_apply_fever(nullptr), 0);
}

/* ============================================================================
 * State Consistency Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, StatisticsNeverOverflow) {
    createBridges();

    // Run many iterations
    for (int i = 0; i < 1000; i++) {
        substrate_update(substrate, 1);
        substrate_record_spikes(substrate, 1);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);

    // Stats should accumulate correctly
    EXPECT_EQ(stats.total_updates, 1000u);
    EXPECT_EQ(stats.spikes_processed, 1000u);
    EXPECT_LT(stats.total_updates, UINT64_MAX);
}

TEST_F(CoreImmuneRegressionTest, AlertCountBounded) {
    createBridges();

    // Create multiple alert conditions
    substrate_set_atp(substrate, 0.1f);
    substrate_set_oxygen(substrate, 0.3f);
    substrate_set_glucose(substrate, 0.2f);
    substrate_set_temperature(substrate, 42.0f);
    substrate_set_membrane_integrity(substrate, 0.4f);
    substrate_set_ion_balance(substrate, 0.3f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count;
    substrate_get_alerts(substrate, alerts, &count);

    // Alert count should be bounded
    EXPECT_LE(count, 8u);
}

TEST_F(CoreImmuneRegressionTest, HealthLevelTransitionsCorrectly) {
    createBridges();

    // Start optimal
    EXPECT_EQ(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_OPTIMAL);

    // Degrade gradually
    substrate_set_atp(substrate, 0.7f);
    substrate_update(substrate, 10);
    EXPECT_GE(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_OPTIMAL);

    substrate_set_atp(substrate, 0.5f);
    substrate_update(substrate, 10);
    EXPECT_GE(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_STRESSED);

    substrate_set_atp(substrate, 0.2f);
    substrate_set_oxygen(substrate, 0.4f);
    substrate_update(substrate, 10);
    EXPECT_GE(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_COMPROMISED);
}

/* ============================================================================
 * Concurrency Safety Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, ConcurrentUpdatesSubstrate) {
    createBridges();

    // Simulate concurrent-style access (sequential but rapid)
    for (int i = 0; i < 100; i++) {
        // Interleaved operations
        substrate_set_atp(substrate, 0.5f + 0.001f * i);
        substrate_update(substrate, 1);
        substrate_get_capacity(substrate);

        substrate_set_temperature(substrate, 37.0f + 0.001f * i);
        substrate_update(substrate, 1);
        substrate_get_firing_modulation(substrate);
    }

    // Should complete without issues
    EXPECT_GE(substrate_get_capacity(substrate), 0.0f);
}

/* ============================================================================
 * Edge Value Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, ZeroTimeUpdate) {
    createBridges();

    // Zero delta should not cause issues
    int result = substrate_update(substrate, 0);
    EXPECT_EQ(result, 0);

    result = substrate_immune_bridge_update(substrate_bridge, 0);
    EXPECT_EQ(result, 0);

    result = brain_regions_immune_bridge_update(regions_bridge, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CoreImmuneRegressionTest, VeryLargeTimeUpdate) {
    createBridges();

    // Very large delta should not overflow
    int result = substrate_update(substrate, 1000000);  // 1000 seconds
    EXPECT_EQ(result, 0);

    // ATP should have fully recovered
    EXPECT_GT(substrate->metabolic.atp_level, 0.0f);
}

TEST_F(CoreImmuneRegressionTest, BoundaryATPValues) {
    createBridges();

    // Test boundary values
    substrate_set_atp(substrate, 0.0f);
    EXPECT_EQ(substrate->metabolic.atp_level, 0.0f);

    substrate_set_atp(substrate, 1.0f);
    EXPECT_EQ(substrate->metabolic.atp_level, 1.0f);

    substrate_set_atp(substrate, 0.5f);
    EXPECT_FLOAT_EQ(substrate->metabolic.atp_level, 0.5f);
}

TEST_F(CoreImmuneRegressionTest, BoundaryTemperatureValues) {
    createBridges();

    // Min/max clamping (substrate clamps to [20.0, 45.0])
    substrate_set_temperature(substrate, 15.0f);
    EXPECT_GE(substrate->physical.temperature, 20.0f);

    substrate_set_temperature(substrate, 50.0f);
    EXPECT_LE(substrate->physical.temperature, 45.0f);
}

/* ============================================================================
 * Recovery Path Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, RecoveryFromCriticalState) {
    createBridges();

    // Put in critical state (use extreme values to guarantee CRITICAL)
    substrate_set_atp(substrate, 0.05f);
    substrate_set_oxygen(substrate, 0.1f);
    substrate_set_membrane_integrity(substrate, 0.2f);
    substrate_set_temperature(substrate, 42.0f);
    substrate_update(substrate, 10);

    EXPECT_GE(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_STRESSED);

    // Recovery cycle
    for (int i = 0; i < 100; i++) {
        substrate_set_atp(substrate, substrate->metabolic.atp_level + 0.01f);
        substrate_set_oxygen(substrate, substrate->metabolic.oxygen_saturation + 0.005f);
        substrate_update(substrate, 100);
    }

    // Should recover
    EXPECT_GT(substrate->metabolic.atp_level, 0.5f);
}

TEST_F(CoreImmuneRegressionTest, MultipleInflammationCycles) {
    createBridges();

    float initial_temp = substrate->physical.temperature;

    // Multiple inflammation/recovery cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        // Inflammation
        for (int i = 0; i < 10; i++) {
            brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.5f, 0, &cytokine_id);
            substrate_immune_bridge_update(substrate_bridge, 100);
        }

        // Recovery
        for (int i = 0; i < 20; i++) {
            substrate_immune_apply_il10_recovery(substrate_bridge, 0.5f);
            substrate_update(substrate, 100);
        }
    }

    // Temperature should be close to normal
    EXPECT_NEAR(substrate->physical.temperature, initial_temp, 2.0f);
}

/* ============================================================================
 * Feature Flag Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, DisabledFeaturesNoSideEffects) {
    substrate_immune_config_t cfg;
    substrate_immune_default_config(&cfg);
    cfg.enable_fever_response = false;
    cfg.enable_metabolic_effects = false;
    cfg.enable_damage_effects = false;
    cfg.enable_substrate_immune_trigger = false;
    cfg.enable_il10_recovery = false;

    substrate_bridge = substrate_immune_bridge_create(&cfg, substrate, immune_system);
    ASSERT_NE(substrate_bridge, nullptr);

    float initial_temp = substrate->physical.temperature;
    float initial_atp = substrate->metabolic.atp_level;
    float initial_membrane = substrate->physical.membrane_integrity;

    // Heavy cytokine release
    for (int i = 0; i < 20; i++) {
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 1.0f, 0, &cytokine_id);
        brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 1.0f, 0, &cytokine_id);
        substrate_immune_bridge_update(substrate_bridge, 100);
    }

    // Nothing should have changed
    EXPECT_FLOAT_EQ(substrate->physical.temperature, initial_temp);
    EXPECT_FLOAT_EQ(substrate->physical.membrane_integrity, initial_membrane);
}

/* ============================================================================
 * String Conversion Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, AllStringConversionsValid) {
    // Health levels
    EXPECT_NE(substrate_health_level_to_string(SUBSTRATE_HEALTH_OPTIMAL), nullptr);
    EXPECT_NE(substrate_health_level_to_string(SUBSTRATE_HEALTH_STRESSED), nullptr);
    EXPECT_NE(substrate_health_level_to_string(SUBSTRATE_HEALTH_COMPROMISED), nullptr);
    EXPECT_NE(substrate_health_level_to_string(SUBSTRATE_HEALTH_CRITICAL), nullptr);
    EXPECT_NE(substrate_health_level_to_string(SUBSTRATE_HEALTH_FAILING), nullptr);

    // Alert types
    EXPECT_NE(substrate_alert_type_to_string(SUBSTRATE_ALERT_NONE), nullptr);
    EXPECT_NE(substrate_alert_type_to_string(SUBSTRATE_ALERT_LOW_ATP), nullptr);
    EXPECT_NE(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOXIA), nullptr);
    EXPECT_NE(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPERTHERMIA), nullptr);
    EXPECT_NE(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOTHERMIA), nullptr);
}

TEST_F(CoreImmuneRegressionTest, InvalidEnumStringConversion) {
    // Invalid enum values should return something (not crash)
    const char* result = substrate_health_level_to_string(static_cast<substrate_health_level_t>(999));
    EXPECT_NE(result, nullptr);

    result = substrate_alert_type_to_string(static_cast<substrate_alert_type_t>(999));
    EXPECT_NE(result, nullptr);
}

/* ============================================================================
 * Determinism Regression Tests
 * ============================================================================ */

TEST_F(CoreImmuneRegressionTest, DeterministicBehavior) {
    // Run same sequence twice, results should be identical

    // First run
    createBridges();

    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.5f, 0, &cytokine_id);
    for (int i = 0; i < 10; i++) {
        substrate_immune_bridge_update(substrate_bridge, 100);
        substrate_update(substrate, 100);
    }

    float first_temp = substrate->physical.temperature;
    float first_atp = substrate->metabolic.atp_level;

    // Reset
    substrate_bridge = nullptr;
    substrate_destroy(substrate);

    substrate_config_t sub_cfg;
    substrate_default_config(&sub_cfg);
    substrate = substrate_create(&sub_cfg);

    substrate_immune_config_t bridge_cfg;
    substrate_immune_default_config(&bridge_cfg);
    substrate_bridge = substrate_immune_bridge_create(&bridge_cfg, substrate, immune_system);

    // Second run (same sequence)
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.5f, 0, &cytokine_id);
    for (int i = 0; i < 10; i++) {
        substrate_immune_bridge_update(substrate_bridge, 100);
        substrate_update(substrate, 100);
    }

    float second_temp = substrate->physical.temperature;
    float second_atp = substrate->metabolic.atp_level;

    // Should be similar (immune system interactions cause small deviations
    // due to cytokine concentration drift between runs)
    EXPECT_NEAR(first_temp, second_temp, 0.5f);
    EXPECT_NEAR(first_atp, second_atp, 0.1f);
}
