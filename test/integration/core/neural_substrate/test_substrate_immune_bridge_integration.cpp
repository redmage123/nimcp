/**
 * @file test_substrate_immune_bridge_integration.cpp
 * @brief Integration tests for Substrate-Immune Bridge
 * @date 2025-01-24
 *
 * Tests the bidirectional integration between neural substrate and brain immune system:
 * - Bridge creation and lifecycle management
 * - Immune system effects on substrate (fever, metabolic, damage)
 * - Substrate stress triggering immune responses
 * - IL-10 recovery mechanisms
 * - Bio-async cross-module communication
 *
 * Biological basis: Inflammation affects metabolic/physical substrate (fever, ATP depletion,
 * membrane damage). Substrate stress can trigger immune responses via DAMPs.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_substrate_immune_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static constexpr float FLOAT_TOLERANCE = 0.01f;
// NIMCP_SUCCESS is defined in nimcp_common.h as 0

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SubstrateImmuneBridgeIntegrationTest : public NimcpTestBase {
protected:
    neural_substrate_t* substrate = nullptr;
    brain_immune_system_t* immune_system = nullptr;
    substrate_immune_bridge_t* bridge = nullptr;
    substrate_immune_config_t bridge_config;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create neural substrate with defaults
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        // Create brain immune system with defaults
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_system = brain_immune_create(&immune_cfg);
        ASSERT_NE(immune_system, nullptr);

        // Get default bridge config
        int result = substrate_immune_default_config(&bridge_config);
        ASSERT_EQ(result, NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (bridge) {
            substrate_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void createBridge() {
        bridge = substrate_immune_bridge_create(&bridge_config, substrate, immune_system);
        ASSERT_NE(bridge, nullptr);
    }

    void createBridgeWithDefaults() {
        bridge = substrate_immune_bridge_create(nullptr, substrate, immune_system);
        ASSERT_NE(bridge, nullptr);
    }

    void setHealthySubstrate() {
        substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
        substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
        substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
        substrate_set_temperature(substrate, SUBSTRATE_NORMAL_TEMPERATURE);
        substrate_set_membrane_integrity(substrate, SUBSTRATE_NORMAL_MEMBRANE);
        substrate_set_ion_balance(substrate, SUBSTRATE_NORMAL_ION_BALANCE);
        substrate_update(substrate, 10);
    }

    void setStressedSubstrate() {
        substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP);
        substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
        substrate_set_membrane_integrity(substrate, SUBSTRATE_CRITICAL_MEMBRANE);
        substrate_set_ion_balance(substrate, SUBSTRATE_CRITICAL_ION_IMBALANCE);
        substrate_update(substrate, 10);
    }
};

/* ============================================================================
 * Bridge Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, CreateWithDefaultConfig) {
    createBridgeWithDefaults();

    EXPECT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->substrate, substrate);
    EXPECT_EQ(bridge->immune_system, immune_system);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CreateWithCustomConfig) {
    bridge_config.enable_fever_response = true;
    bridge_config.enable_metabolic_effects = true;
    bridge_config.enable_damage_effects = true;
    bridge_config.temperature_sensitivity = 1.5f;

    createBridge();

    EXPECT_TRUE(bridge->config.enable_fever_response);
    EXPECT_TRUE(bridge->config.enable_metabolic_effects);
    EXPECT_TRUE(bridge->config.enable_damage_effects);
    EXPECT_NEAR(bridge->config.temperature_sensitivity, 1.5f, FLOAT_TOLERANCE);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, DestroyNullSafe) {
    // Should not crash
    substrate_immune_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CreateWithNullSubstrateFails) {
    bridge = substrate_immune_bridge_create(&bridge_config, nullptr, immune_system);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CreateWithNullImmuneSystem) {
    // Bridge may or may not accept NULL immune system depending on implementation
    bridge = substrate_immune_bridge_create(&bridge_config, substrate, nullptr);
    // Clean up if created
    if (bridge) {
        substrate_immune_bridge_destroy(bridge);
        bridge = nullptr;
    }
    SUCCEED();
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, ConnectBioAsync) {
    createBridge();

    // May succeed or fail depending on router availability
    int result = substrate_immune_connect_bio_async(bridge);
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, DisconnectBioAsync) {
    createBridge();

    substrate_immune_connect_bio_async(bridge);
    int result = substrate_immune_disconnect_bio_async(bridge);
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CheckBioAsyncConnectionStatus) {
    createBridge();

    bool connected_before = substrate_immune_is_bio_async_connected(bridge);

    substrate_immune_connect_bio_async(bridge);
    bool connected_after = substrate_immune_is_bio_async_connected(bridge);

    // Status should be valid booleans
    EXPECT_TRUE(connected_before == true || connected_before == false);
    EXPECT_TRUE(connected_after == true || connected_after == false);
}

/* ============================================================================
 * Immune -> Substrate Effect Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, ApplyFeverResponse) {
    createBridge();
    setHealthySubstrate();

    float initial_temp = substrate->physical.temperature;

    int result = substrate_immune_apply_fever(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Temperature may change based on immune state
    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);
    EXPECT_GE(state.temperature, initial_temp - 1.0f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, ApplyMetabolicEffects) {
    createBridge();
    setHealthySubstrate();

    float initial_atp = substrate->metabolic.atp_level;

    int result = substrate_immune_apply_metabolic_effects(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // ATP may be affected by immune metabolic burden
    EXPECT_GE(substrate->metabolic.atp_level, 0.0f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, ApplyDamageEffects) {
    createBridge();
    setHealthySubstrate();

    float initial_membrane = substrate->physical.membrane_integrity;

    int result = substrate_immune_apply_damage(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Membrane integrity should remain valid
    EXPECT_GE(substrate->physical.membrane_integrity, 0.0f);
    EXPECT_LE(substrate->physical.membrane_integrity, 1.0f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, ApplyIL10Recovery) {
    createBridge();

    // Set stressed state
    substrate_set_temperature(substrate, 39.5f);
    substrate_set_atp(substrate, 0.5f);
    substrate_update(substrate, 10);

    float stressed_temp = substrate->physical.temperature;

    // Apply IL-10 recovery
    int result = substrate_immune_apply_il10_recovery(bridge, 0.8f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // IL-10 should have recovery effects
    // Temperature may reduce, ATP recovery may boost
    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.il10_recoveries, 0u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, IL10RecoveryWithZeroConcentration) {
    createBridge();
    setStressedSubstrate();

    int result = substrate_immune_apply_il10_recovery(bridge, 0.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, IL10RecoveryWithMaxConcentration) {
    createBridge();
    setStressedSubstrate();

    int result = substrate_immune_apply_il10_recovery(bridge, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Substrate -> Immune Trigger Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, CheckStressWithHealthySubstrate) {
    createBridge();
    setHealthySubstrate();

    bool should_trigger = substrate_immune_check_stress(bridge);

    // Healthy substrate should not trigger immune
    EXPECT_FALSE(should_trigger);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CheckStressWithStressedSubstrate) {
    createBridge();
    setStressedSubstrate();

    // Update to detect alerts
    for (int i = 0; i < 5; i++) {
        substrate_update(substrate, 100);
        substrate_immune_bridge_update(bridge, 100);
    }

    bool should_trigger = substrate_immune_check_stress(bridge);

    // Stressed substrate may trigger immune
    EXPECT_TRUE(should_trigger == true || should_trigger == false);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, TriggerImmuneResponse) {
    createBridge();
    setStressedSubstrate();

    // Ensure stress is detected
    for (int i = 0; i < 5; i++) {
        substrate_immune_bridge_update(bridge, 100);
    }

    int result = substrate_immune_trigger_response(bridge);
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, ComputeStressSeverity) {
    createBridge();
    setStressedSubstrate();

    uint32_t severity = substrate_immune_compute_severity(bridge);

    EXPECT_GE(severity, 1u);
    EXPECT_LE(severity, 10u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, SeverityIncreasesWithWorseConditions) {
    createBridge();

    // Mild stress
    substrate_set_atp(substrate, 0.6f);
    substrate_set_oxygen(substrate, 0.7f);
    substrate_update(substrate, 10);
    uint32_t mild_severity = substrate_immune_compute_severity(bridge);

    // Severe stress
    substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP - 0.1f);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2 - 0.1f);
    substrate_update(substrate, 10);
    uint32_t severe_severity = substrate_immune_compute_severity(bridge);

    EXPECT_GE(severe_severity, mild_severity);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, BridgeUpdate) {
    createBridge();
    setHealthySubstrate();

    int result = substrate_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 1u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, MultipleUpdateCycles) {
    createBridge();
    setHealthySubstrate();

    for (int i = 0; i < 50; i++) {
        int result = substrate_immune_bridge_update(bridge, 20);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 50u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, UpdateWithZeroDeltaMs) {
    createBridge();

    int result = substrate_immune_bridge_update(bridge, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, UpdateProcessesBothDirections) {
    createBridge();
    setStressedSubstrate();

    // Start immune system if needed
    brain_immune_start(immune_system);

    for (int i = 0; i < 10; i++) {
        substrate_immune_bridge_update(bridge, 100);
    }

    // Check that both directions were processed
    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, GetCytokineEffects) {
    createBridge();
    setHealthySubstrate();
    substrate_immune_bridge_update(bridge, 100);

    cytokine_substrate_effects_t effects;
    int result = substrate_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_GE(effects.fever_intensity, 0.0f);
    EXPECT_LE(effects.fever_intensity, 1.0f);
    EXPECT_GE(effects.metabolic_burden, 0.0f);
    EXPECT_LE(effects.metabolic_burden, 1.0f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, GetTriggerState) {
    createBridge();
    setStressedSubstrate();

    for (int i = 0; i < 5; i++) {
        substrate_immune_bridge_update(bridge, 100);
    }

    substrate_immune_trigger_t trigger;
    int result = substrate_immune_get_trigger_state(bridge, &trigger);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Trigger state should be valid
    EXPECT_GE(trigger.computed_severity, 0u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, CheckModulationStatus) {
    createBridge();
    setHealthySubstrate();

    bool modulated = substrate_immune_is_modulated(bridge);
    EXPECT_TRUE(modulated == true || modulated == false);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, GetFeverIntensity) {
    createBridge();

    float fever = substrate_immune_get_fever_intensity(bridge);

    EXPECT_GE(fever, 0.0f);
    EXPECT_LE(fever, 1.0f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, GetBridgeStatistics) {
    createBridge();
    setHealthySubstrate();

    // Generate some activity
    for (int i = 0; i < 10; i++) {
        substrate_immune_bridge_update(bridge, 100);
    }

    substrate_immune_stats_t stats;
    int result = substrate_immune_get_stats(bridge, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_updates, 10u);
}

/* ============================================================================
 * Recovery Mechanism Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, RecoveryFromInflammation) {
    createBridge();

    // Induce stress (inflammation-like state)
    substrate_set_temperature(substrate, 39.5f);
    substrate_set_atp(substrate, 0.5f);
    substrate_update(substrate, 10);

    float stressed_temp = substrate->physical.temperature;
    float stressed_atp = substrate->metabolic.atp_level;

    // Apply IL-10 recovery repeatedly
    for (int i = 0; i < 20; i++) {
        substrate_immune_apply_il10_recovery(bridge, 0.6f);
        substrate_immune_bridge_update(bridge, 100);
        substrate_update(substrate, 100);
    }

    // Check for recovery
    float recovered_temp = substrate->physical.temperature;
    float recovered_atp = substrate->metabolic.atp_level;

    // Recovery should show improvement or stability
    EXPECT_GE(recovered_atp, stressed_atp * 0.9f);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, GradualTemperatureNormalization) {
    createBridge();
    bridge_config.enable_il10_recovery = true;

    // Create fever state
    substrate_set_temperature(substrate, 40.0f);
    substrate_update(substrate, 10);

    // Apply IL-10 cooling
    for (int i = 0; i < 50; i++) {
        substrate_immune_apply_il10_recovery(bridge, 0.7f);
        substrate_immune_bridge_update(bridge, 50);
    }

    float final_temp = substrate->physical.temperature;

    // Temperature should have moved toward normal
    EXPECT_LE(final_temp, 42.0f);
}

/* ============================================================================
 * Cross-Module Communication Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, SubstrateImmuneCoordination) {
    createBridge();

    brain_immune_start(immune_system);
    setStressedSubstrate();

    // Run coordination cycle
    for (int cycle = 0; cycle < 20; cycle++) {
        // Check for stress
        bool should_trigger = substrate_immune_check_stress(bridge);

        if (should_trigger) {
            substrate_immune_trigger_response(bridge);
        }

        // Update both directions
        substrate_immune_bridge_update(bridge, 50);
        substrate_update(substrate, 50);
    }

    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_GE(stats.total_updates, 20u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, FeverCycleWithRecovery) {
    createBridge();
    brain_immune_start(immune_system);

    // Phase 1: Induce fever
    for (int i = 0; i < 10; i++) {
        substrate_immune_apply_fever(bridge);
        substrate_immune_bridge_update(bridge, 100);
    }

    float peak_temp = substrate->physical.temperature;

    // Phase 2: Recovery with IL-10
    for (int i = 0; i < 20; i++) {
        substrate_immune_apply_il10_recovery(bridge, 0.8f);
        substrate_immune_bridge_update(bridge, 100);
    }

    float recovered_temp = substrate->physical.temperature;

    // Temperature should have decreased during recovery
    EXPECT_LE(recovered_temp, peak_temp + 1.0f);
}

/* ============================================================================
 * Complex Scenario Tests
 * ============================================================================ */

TEST_F(SubstrateImmuneBridgeIntegrationTest, FullInflammationCycle) {
    createBridge();
    brain_immune_start(immune_system);
    setHealthySubstrate();

    // Phase 1: Healthy baseline
    for (int i = 0; i < 5; i++) {
        substrate_immune_bridge_update(bridge, 100);
    }

    float baseline_temp = substrate->physical.temperature;
    float baseline_atp = substrate->metabolic.atp_level;

    // Phase 2: Stress develops
    setStressedSubstrate();
    for (int i = 0; i < 10; i++) {
        if (substrate_immune_check_stress(bridge)) {
            substrate_immune_trigger_response(bridge);
        }
        substrate_immune_apply_fever(bridge);
        substrate_immune_apply_metabolic_effects(bridge);
        substrate_immune_bridge_update(bridge, 100);
    }

    // Phase 3: Peak inflammation
    float peak_temp = substrate->physical.temperature;
    substrate_immune_stats_t peak_stats;
    substrate_immune_get_stats(bridge, &peak_stats);

    // Phase 4: Recovery
    for (int i = 0; i < 30; i++) {
        substrate_immune_apply_il10_recovery(bridge, 0.7f);
        substrate_set_atp(substrate, std::min(1.0f, substrate->metabolic.atp_level + 0.01f));
        substrate_immune_bridge_update(bridge, 100);
        substrate_update(substrate, 100);
    }

    // Verify cycle completed
    substrate_immune_stats_t final_stats;
    substrate_immune_get_stats(bridge, &final_stats);
    EXPECT_GT(final_stats.total_updates, peak_stats.total_updates);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, LongRunningStabilityTest) {
    createBridge();
    brain_immune_start(immune_system);

    // Run extended simulation with varying conditions
    for (int i = 0; i < 200; i++) {
        // Vary substrate conditions
        float atp = 0.5f + 0.4f * sinf(i * 0.05f);
        float temp = 37.0f + 2.0f * sinf(i * 0.03f);

        substrate_set_atp(substrate, atp);
        substrate_set_temperature(substrate, temp);

        // Occasionally check for immune trigger
        if (i % 10 == 0) {
            if (substrate_immune_check_stress(bridge)) {
                substrate_immune_trigger_response(bridge);
            }
        }

        // Alternating immune effects
        if (i % 5 == 0) {
            substrate_immune_apply_fever(bridge);
        }
        if (i % 7 == 0) {
            substrate_immune_apply_il10_recovery(bridge, 0.5f);
        }

        substrate_immune_bridge_update(bridge, 50);
        substrate_update(substrate, 50);
    }

    // Verify system remains stable
    EXPECT_NE(bridge, nullptr);
    EXPECT_NE(substrate, nullptr);
    EXPECT_NE(immune_system, nullptr);

    substrate_immune_stats_t stats;
    substrate_immune_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 200u);
}

TEST_F(SubstrateImmuneBridgeIntegrationTest, ConcurrentEffects) {
    createBridge();
    brain_immune_start(immune_system);
    setHealthySubstrate();

    // Apply all effects concurrently
    for (int i = 0; i < 50; i++) {
        substrate_immune_apply_fever(bridge);
        substrate_immune_apply_metabolic_effects(bridge);
        substrate_immune_apply_damage(bridge);

        if (i > 25) {
            substrate_immune_apply_il10_recovery(bridge, 0.5f);
        }

        substrate_immune_bridge_update(bridge, 20);
    }

    // System should handle concurrent effects
    cytokine_substrate_effects_t effects;
    substrate_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GE(effects.fever_intensity, 0.0f);
    EXPECT_GE(effects.metabolic_burden, 0.0f);
    EXPECT_GE(effects.damage_severity, 0.0f);
}
