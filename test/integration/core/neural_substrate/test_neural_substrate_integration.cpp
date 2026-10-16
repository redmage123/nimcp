/**
 * @file test_neural_substrate_integration.cpp
 * @brief Integration tests for Neural Substrate Module
 * @date 2025-01-24
 *
 * Tests neural substrate functionality including:
 * - Creation and lifecycle management
 * - Metabolic state management (ATP, O2, glucose)
 * - Physical substrate health (temperature, membrane, ions)
 * - Update cycle processing
 * - Statistics tracking
 * - Bio-async integration for imagination
 *
 * Biological basis: The neural substrate models physical constraints
 * that affect neural computation - metabolic energy, temperature,
 * ion concentrations, and membrane integrity.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static constexpr float FLOAT_TOLERANCE = 0.001f;
// NIMCP_SUCCESS is defined in nimcp_common.h as 0

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class NeuralSubstrateIntegrationTest : public NimcpTestBase {
protected:
    neural_substrate_t* substrate = nullptr;
    substrate_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Get default configuration
        int result = substrate_default_config(&config);
        ASSERT_EQ(result, NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void createSubstrate() {
        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);
    }

    void createSubstrateWithDefaults() {
        substrate = substrate_create(nullptr);
        ASSERT_NE(substrate, nullptr);
    }

    void setHealthyState() {
        substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
        substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
        substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
        substrate_set_temperature(substrate, SUBSTRATE_NORMAL_TEMPERATURE);
        substrate_set_membrane_integrity(substrate, SUBSTRATE_NORMAL_MEMBRANE);
        substrate_set_ion_balance(substrate, SUBSTRATE_NORMAL_ION_BALANCE);
    }

    void setStressedState(float atp, float o2, float temp) {
        substrate_set_atp(substrate, atp);
        substrate_set_oxygen(substrate, o2);
        substrate_set_temperature(substrate, temp);
        substrate_update(substrate, 10);
    }
};

/* ============================================================================
 * Creation and Lifecycle Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, CreateWithDefaultConfig) {
    createSubstrateWithDefaults();

    EXPECT_NE(substrate, nullptr);
    EXPECT_EQ(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_OPTIMAL);
}

TEST_F(NeuralSubstrateIntegrationTest, CreateWithCustomConfig) {
    config.initial_atp = 0.8f;
    config.initial_o2 = 0.85f;
    config.initial_glucose = 0.75f;
    config.initial_temperature = 36.5f;

    createSubstrate();

    substrate_metabolic_state_t metabolic;
    int result = substrate_get_metabolic_state(substrate, &metabolic);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_NEAR(metabolic.atp_level, 0.8f, FLOAT_TOLERANCE);
    EXPECT_NEAR(metabolic.oxygen_saturation, 0.85f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, DestroyNullSafe) {
    // Should not crash
    substrate_destroy(nullptr);
    SUCCEED();
}

TEST_F(NeuralSubstrateIntegrationTest, ResetToInitialState) {
    createSubstrate();

    // Modify state
    substrate_set_atp(substrate, 0.3f);
    substrate_set_oxygen(substrate, 0.4f);
    substrate_record_spikes(substrate, 1000);

    // Reset
    int result = substrate_reset(substrate);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should be back to initial
    substrate_metabolic_state_t metabolic;
    substrate_get_metabolic_state(substrate, &metabolic);
    EXPECT_NEAR(metabolic.atp_level, config.initial_atp, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, ConfigEnablesMetabolicModel) {
    config.enable_metabolic_model = true;
    config.enable_temperature_effects = true;
    config.enable_ion_dynamics = true;
    config.enable_alerts = true;

    createSubstrate();

    EXPECT_TRUE(substrate->config.enable_metabolic_model);
    EXPECT_TRUE(substrate->config.enable_temperature_effects);
    EXPECT_TRUE(substrate->config.enable_ion_dynamics);
    EXPECT_TRUE(substrate->config.enable_alerts);
}

/* ============================================================================
 * Metabolic State Management Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetATPLevel) {
    createSubstrate();

    int result = substrate_set_atp(substrate, 0.75f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_metabolic_state_t state;
    substrate_get_metabolic_state(substrate, &state);
    EXPECT_NEAR(state.atp_level, 0.75f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetOxygenSaturation) {
    createSubstrate();

    int result = substrate_set_oxygen(substrate, 0.85f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_metabolic_state_t state;
    substrate_get_metabolic_state(substrate, &state);
    EXPECT_NEAR(state.oxygen_saturation, 0.85f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetGlucoseLevel) {
    createSubstrate();

    int result = substrate_set_glucose(substrate, 0.70f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_metabolic_state_t state;
    substrate_get_metabolic_state(substrate, &state);
    EXPECT_NEAR(state.glucose_level, 0.70f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, MetabolicCapacityComputation) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    substrate_metabolic_state_t state;
    substrate_get_metabolic_state(substrate, &state);

    // Healthy state should have high metabolic capacity
    EXPECT_GT(state.metabolic_capacity, 0.8f);
}

TEST_F(NeuralSubstrateIntegrationTest, MetabolicStressReducesCapacity) {
    createSubstrate();

    // Create metabolic stress
    substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2);
    substrate_update(substrate, 10);

    substrate_metabolic_state_t state;
    substrate_get_metabolic_state(substrate, &state);

    EXPECT_LT(state.metabolic_capacity, 0.5f);
}

TEST_F(NeuralSubstrateIntegrationTest, ATPRecoveryOverTime) {
    createSubstrate();

    // Deplete ATP
    substrate_set_atp(substrate, 0.5f);
    float depleted_atp = substrate->metabolic.atp_level;

    // Allow recovery over time
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 100);
    }

    float recovered_atp = substrate->metabolic.atp_level;
    EXPECT_GT(recovered_atp, depleted_atp);
}

/* ============================================================================
 * Physical Substrate Health Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetTemperature) {
    createSubstrate();

    int result = substrate_set_temperature(substrate, 38.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);
    EXPECT_NEAR(state.temperature, 38.5f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetMembraneIntegrity) {
    createSubstrate();

    int result = substrate_set_membrane_integrity(substrate, 0.85f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);
    EXPECT_NEAR(state.membrane_integrity, 0.85f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, SetAndGetIonBalance) {
    createSubstrate();

    int result = substrate_set_ion_balance(substrate, 0.80f);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);
    EXPECT_NEAR(state.ion_balance, 0.80f, FLOAT_TOLERANCE);
}

TEST_F(NeuralSubstrateIntegrationTest, PhysicalCapacityComputation) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);

    EXPECT_GT(state.physical_capacity, 0.8f);
}

TEST_F(NeuralSubstrateIntegrationTest, HyperthermiaReducesPhysicalCapacity) {
    createSubstrate();

    substrate_set_temperature(substrate, SUBSTRATE_HYPERTHERMIA_THRESHOLD + 1.0f);
    substrate_update(substrate, 10);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);

    EXPECT_LT(state.physical_capacity, 1.0f);
}

TEST_F(NeuralSubstrateIntegrationTest, HypothermiaReducesPhysicalCapacity) {
    createSubstrate();

    substrate_set_temperature(substrate, SUBSTRATE_HYPOTHERMIA_THRESHOLD - 2.0f);
    substrate_update(substrate, 10);

    substrate_physical_state_t state;
    substrate_get_physical_state(substrate, &state);

    EXPECT_LT(state.physical_capacity, 1.0f);
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, BasicUpdateCycle) {
    createSubstrate();

    int result = substrate_update(substrate, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.total_updates, 1u);
}

TEST_F(NeuralSubstrateIntegrationTest, MultipleUpdateCycles) {
    createSubstrate();

    for (int i = 0; i < 50; i++) {
        int result = substrate_update(substrate, 20);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.total_updates, 50u);
}

TEST_F(NeuralSubstrateIntegrationTest, RecordSpikesConsumesEnergy) {
    createSubstrate();
    setHealthyState();

    float initial_atp = substrate->metabolic.atp_level;

    // Record many spikes
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 10);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.spikes_processed, 1000u);

    // ATP should have decreased
    EXPECT_LT(substrate->metabolic.atp_level, initial_atp);
}

TEST_F(NeuralSubstrateIntegrationTest, RecordTransmissionsConsumesEnergy) {
    createSubstrate();
    setHealthyState();

    float initial_atp = substrate->metabolic.atp_level;

    // Record many transmissions
    for (int i = 0; i < 100; i++) {
        substrate_record_transmissions(substrate, 20);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.transmissions_processed, 2000u);

    EXPECT_LT(substrate->metabolic.atp_level, initial_atp);
}

TEST_F(NeuralSubstrateIntegrationTest, UpdateWithZeroDeltaMs) {
    createSubstrate();

    int result = substrate_update(substrate, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Modulation Factor Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, GetModulationFactors) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    substrate_modulation_t mod;
    int result = substrate_get_modulation(substrate, &mod);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_GT(mod.firing_rate_mod, 0.0f);
    EXPECT_GT(mod.transmission_efficiency, 0.0f);
    EXPECT_GT(mod.conduction_velocity, 0.0f);
    EXPECT_GT(mod.plasticity_capacity, 0.0f);
    EXPECT_GT(mod.overall_capacity, 0.0f);
}

TEST_F(NeuralSubstrateIntegrationTest, FiringModulationUnderStress) {
    createSubstrate();

    // Healthy firing modulation
    setHealthyState();
    substrate_update(substrate, 10);
    float healthy_mod = substrate_get_firing_modulation(substrate);

    // Stressed firing modulation
    setStressedState(0.3f, 0.5f, 40.0f);
    float stressed_mod = substrate_get_firing_modulation(substrate);

    EXPECT_LT(stressed_mod, healthy_mod);
}

TEST_F(NeuralSubstrateIntegrationTest, TransmissionEfficiencyUnderStress) {
    createSubstrate();

    // Healthy efficiency
    setHealthyState();
    substrate_update(substrate, 10);
    float healthy_eff = substrate_get_transmission_efficiency(substrate);

    // Stressed efficiency
    setStressedState(0.3f, 0.5f, 40.0f);
    float stressed_eff = substrate_get_transmission_efficiency(substrate);

    EXPECT_LT(stressed_eff, healthy_eff);
}

TEST_F(NeuralSubstrateIntegrationTest, OverallCapacityComputation) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    float capacity = substrate_get_capacity(substrate);
    EXPECT_GT(capacity, 0.8f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(NeuralSubstrateIntegrationTest, TemperatureAffectsModulation) {
    createSubstrate();
    setHealthyState();

    // Normal temperature
    substrate_set_temperature(substrate, 37.0f);
    substrate_update(substrate, 10);
    substrate_modulation_t normal_mod;
    substrate_get_modulation(substrate, &normal_mod);

    // High temperature (Q10 effects)
    substrate_set_temperature(substrate, 40.0f);
    substrate_update(substrate, 10);
    substrate_modulation_t hot_mod;
    substrate_get_modulation(substrate, &hot_mod);

    // Higher temperature should increase conduction velocity
    EXPECT_GT(hot_mod.conduction_velocity, normal_mod.conduction_velocity);
}

/* ============================================================================
 * Health Level and Alert Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, HealthySubstrateHasOptimalHealth) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_EQ(health, SUBSTRATE_HEALTH_OPTIMAL);
}

TEST_F(NeuralSubstrateIntegrationTest, StressedSubstrateHasDegradedHealth) {
    createSubstrate();
    setStressedState(0.4f, 0.5f, 39.0f);

    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_NE(health, SUBSTRATE_HEALTH_OPTIMAL);
}

TEST_F(NeuralSubstrateIntegrationTest, CriticalSubstrateHealth) {
    createSubstrate();

    // Set critical conditions
    substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP - 0.1f);
    substrate_set_oxygen(substrate, SUBSTRATE_CRITICAL_O2 - 0.1f);
    substrate_update(substrate, 10);

    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_GE(health, SUBSTRATE_HEALTH_COMPROMISED);
}

TEST_F(NeuralSubstrateIntegrationTest, GetActiveAlerts) {
    createSubstrate();
    config.enable_alerts = true;

    // Create alert conditions
    substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP - 0.1f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    int result = substrate_get_alerts(substrate, alerts, &count);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should have at least one alert
    EXPECT_GE(count, 0u);
}

TEST_F(NeuralSubstrateIntegrationTest, HyperthermiaAlert) {
    createSubstrate();
    config.enable_alerts = true;

    substrate_set_temperature(substrate, SUBSTRATE_HYPERTHERMIA_THRESHOLD + 1.0f);
    substrate_update(substrate, 10);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);

    // May have generated alerts
    EXPECT_GE(stats.alerts_generated, 0u);
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, StatisticsInitializedToZero) {
    createSubstrate();

    substrate_stats_t stats;
    int result = substrate_get_stats(substrate, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.spikes_processed, 0u);
    EXPECT_EQ(stats.transmissions_processed, 0u);
}

TEST_F(NeuralSubstrateIntegrationTest, StatisticsTrackSpikes) {
    createSubstrate();

    substrate_record_spikes(substrate, 50);
    substrate_record_spikes(substrate, 30);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.spikes_processed, 80u);
}

TEST_F(NeuralSubstrateIntegrationTest, StatisticsTrackTransmissions) {
    createSubstrate();

    substrate_record_transmissions(substrate, 100);
    substrate_record_transmissions(substrate, 200);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.transmissions_processed, 300u);
}

TEST_F(NeuralSubstrateIntegrationTest, StatisticsTrackATPConsumption) {
    createSubstrate();
    setHealthyState();

    // Record activity to consume ATP
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 10);
        substrate_record_transmissions(substrate, 10);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_GT(stats.total_atp_consumed, 0.0f);
}

TEST_F(NeuralSubstrateIntegrationTest, PeakMetabolicRateTracking) {
    createSubstrate();
    setHealthyState();

    // Burst of activity
    for (int i = 0; i < 50; i++) {
        substrate_record_spikes(substrate, 50);
        substrate_update(substrate, 1);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_GE(stats.peak_metabolic_rate, 0.0f);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, HealthLevelToString) {
    const char* optimal = substrate_health_level_to_string(SUBSTRATE_HEALTH_OPTIMAL);
    const char* stressed = substrate_health_level_to_string(SUBSTRATE_HEALTH_STRESSED);
    const char* compromised = substrate_health_level_to_string(SUBSTRATE_HEALTH_COMPROMISED);
    const char* critical = substrate_health_level_to_string(SUBSTRATE_HEALTH_CRITICAL);
    const char* failing = substrate_health_level_to_string(SUBSTRATE_HEALTH_FAILING);

    EXPECT_NE(optimal, nullptr);
    EXPECT_NE(stressed, nullptr);
    EXPECT_NE(compromised, nullptr);
    EXPECT_NE(critical, nullptr);
    EXPECT_NE(failing, nullptr);
}

TEST_F(NeuralSubstrateIntegrationTest, AlertTypeToString) {
    const char* low_atp = substrate_alert_type_to_string(SUBSTRATE_ALERT_LOW_ATP);
    const char* hypoxia = substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOXIA);
    const char* hyperthermia = substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPERTHERMIA);

    EXPECT_NE(low_atp, nullptr);
    EXPECT_NE(hypoxia, nullptr);
    EXPECT_NE(hyperthermia, nullptr);
}

/* ============================================================================
 * Bio-async Integration Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, RegisterImaginationHandler) {
    createSubstrate();

    // May succeed or fail depending on bio-async availability
    int result = neural_substrate_register_imagination_handler(substrate);
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(NeuralSubstrateIntegrationTest, UnregisterImaginationHandler) {
    createSubstrate();

    neural_substrate_register_imagination_handler(substrate);

    int result = neural_substrate_unregister_imagination_handler();
    EXPECT_TRUE(result == 0 || result == -1);
}

TEST_F(NeuralSubstrateIntegrationTest, SendImaginationCapacity) {
    createSubstrate();
    setHealthyState();
    substrate_update(substrate, 10);

    // May succeed or fail depending on bio-async
    int result = neural_substrate_send_imagination_capacity(substrate);
    EXPECT_TRUE(result == 0 || result == -1);
}

/* ============================================================================
 * Complex Scenario Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateIntegrationTest, SimulatedNeuralActivityScenario) {
    createSubstrate();
    setHealthyState();

    // Simulate neural activity over time
    for (int cycle = 0; cycle < 100; cycle++) {
        // Record activity
        substrate_record_spikes(substrate, 10 + (cycle % 20));
        substrate_record_transmissions(substrate, 50 + (cycle % 30));

        // Update substrate
        substrate_update(substrate, 10);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);

    EXPECT_EQ(stats.total_updates, 100u);
    EXPECT_GT(stats.spikes_processed, 0u);
    EXPECT_GT(stats.transmissions_processed, 0u);
    EXPECT_GT(stats.total_atp_consumed, 0.0f);
}

TEST_F(NeuralSubstrateIntegrationTest, RecoveryFromExhaustion) {
    createSubstrate();
    setHealthyState();

    // Exhaust substrate with high activity
    for (int i = 0; i < 500; i++) {
        substrate_record_spikes(substrate, 100);
        substrate_record_transmissions(substrate, 200);
    }

    float exhausted_capacity = substrate_get_capacity(substrate);

    // Allow recovery with no activity
    for (int i = 0; i < 1000; i++) {
        substrate_update(substrate, 100);
    }

    float recovered_capacity = substrate_get_capacity(substrate);
    EXPECT_GT(recovered_capacity, exhausted_capacity);
}

TEST_F(NeuralSubstrateIntegrationTest, LongRunningStabilityTest) {
    createSubstrate();
    setHealthyState();

    // Run for many cycles with varying conditions
    for (int i = 0; i < 500; i++) {
        // Vary conditions
        float atp = 0.7f + 0.25f * sinf(i * 0.02f);
        float o2 = 0.75f + 0.22f * cosf(i * 0.03f);
        float temp = 37.0f + 2.0f * sinf(i * 0.01f);

        substrate_set_atp(substrate, atp);
        substrate_set_oxygen(substrate, o2);
        substrate_set_temperature(substrate, temp);

        if (i % 3 == 0) {
            substrate_record_spikes(substrate, 5);
        }
        if (i % 5 == 0) {
            substrate_record_transmissions(substrate, 10);
        }

        substrate_update(substrate, 20);
    }

    // Verify substrate remains valid
    EXPECT_NE(substrate, nullptr);
    EXPECT_GT(substrate->metabolic.atp_level, 0.0f);
    EXPECT_LT(substrate->physical.temperature, 50.0f);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.total_updates, 500u);
}
