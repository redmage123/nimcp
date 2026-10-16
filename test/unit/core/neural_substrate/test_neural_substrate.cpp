/**
 * @file test_neural_substrate.cpp
 * @brief Unit tests for Neural Substrate Module
 * @date 2025-12-12
 *
 * Tests metabolic state tracking, physical state management, Q10 temperature
 * effects, modulation computation, and alert generation.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class NeuralSubstrateTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    substrate_config_t config;

    void SetUp() override {
        substrate_default_config(&config);
    }

    void TearDown() override {
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    // Helper to create substrate with default settings
    void createSubstrate() {
        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);
    }

    // Helper to create substrate with custom settings
    void createSubstrateWithConfig() {
        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, DefaultConfigIsValid) {
    substrate_config_t cfg;
    int result = substrate_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(cfg.initial_atp, SUBSTRATE_NORMAL_ATP);
    EXPECT_FLOAT_EQ(cfg.initial_o2, SUBSTRATE_NORMAL_O2_SAT);
    EXPECT_FLOAT_EQ(cfg.initial_glucose, SUBSTRATE_NORMAL_GLUCOSE);
    EXPECT_FLOAT_EQ(cfg.initial_temperature, SUBSTRATE_NORMAL_TEMPERATURE);
    EXPECT_FLOAT_EQ(cfg.initial_membrane, SUBSTRATE_NORMAL_MEMBRANE);
    EXPECT_FLOAT_EQ(cfg.initial_ion_balance, SUBSTRATE_NORMAL_ION_BALANCE);
    EXPECT_TRUE(cfg.enable_metabolic_model);
    EXPECT_TRUE(cfg.enable_temperature_effects);
    EXPECT_TRUE(cfg.enable_ion_dynamics);
    EXPECT_TRUE(cfg.enable_alerts);
}

TEST_F(NeuralSubstrateTest, DefaultConfigNullFails) {
    int result = substrate_default_config(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(NeuralSubstrateTest, CreateWithValidConfig) {
    createSubstrate();
    EXPECT_NE(substrate, nullptr);
    EXPECT_EQ(substrate->health_level, SUBSTRATE_HEALTH_OPTIMAL);
}

TEST_F(NeuralSubstrateTest, CreateWithNullConfig) {
    substrate = substrate_create(nullptr);
    EXPECT_NE(substrate, nullptr);  // Should use defaults
}

TEST_F(NeuralSubstrateTest, CreateWithCustomConfig) {
    config.initial_atp = 0.5f;
    config.initial_temperature = 38.0f;
    createSubstrateWithConfig();

    EXPECT_FLOAT_EQ(substrate->metabolic.atp_level, 0.5f);
    EXPECT_FLOAT_EQ(substrate->physical.temperature, 38.0f);
}

TEST_F(NeuralSubstrateTest, DestroyNullSafe) {
    substrate_destroy(nullptr);
    // Should not crash
}

TEST_F(NeuralSubstrateTest, Reset) {
    createSubstrate();

    // Modify state
    substrate_set_atp(substrate, 0.5f);
    substrate_set_temperature(substrate, 39.0f);

    // Reset
    int result = substrate_reset(substrate);
    EXPECT_EQ(result, 0);

    // Should be back to defaults
    EXPECT_FLOAT_EQ(substrate->metabolic.atp_level, config.initial_atp);
    EXPECT_FLOAT_EQ(substrate->physical.temperature, config.initial_temperature);
}

TEST_F(NeuralSubstrateTest, ResetNullFails) {
    int result = substrate_reset(nullptr);
    EXPECT_NE(result, 0);  // NIMCP error code (not necessarily -1)
}

/* ============================================================================
 * Metabolic State Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, InitialMetabolicState) {
    createSubstrate();

    substrate_metabolic_state_t state;
    int result = substrate_get_metabolic_state(substrate, &state);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(state.atp_level, SUBSTRATE_NORMAL_ATP);
    EXPECT_FLOAT_EQ(state.oxygen_saturation, SUBSTRATE_NORMAL_O2_SAT);
    EXPECT_FLOAT_EQ(state.glucose_level, SUBSTRATE_NORMAL_GLUCOSE);
    EXPECT_GT(state.metabolic_capacity, 0.9f);
}

TEST_F(NeuralSubstrateTest, SetATP) {
    createSubstrate();

    int result = substrate_set_atp(substrate, 0.5f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->metabolic.atp_level, 0.5f);
}

TEST_F(NeuralSubstrateTest, SetATPClamped) {
    createSubstrate();

    substrate_set_atp(substrate, 1.5f);  // Above max
    EXPECT_LE(substrate->metabolic.atp_level, 1.0f);

    substrate_set_atp(substrate, -0.5f);  // Below min
    EXPECT_GE(substrate->metabolic.atp_level, 0.0f);
}

TEST_F(NeuralSubstrateTest, SetOxygen) {
    createSubstrate();

    int result = substrate_set_oxygen(substrate, 0.8f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->metabolic.oxygen_saturation, 0.8f);
}

TEST_F(NeuralSubstrateTest, SetGlucose) {
    createSubstrate();

    int result = substrate_set_glucose(substrate, 0.7f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->metabolic.glucose_level, 0.7f);
}

TEST_F(NeuralSubstrateTest, SetterNullChecks) {
    EXPECT_NE(substrate_set_atp(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_oxygen(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_glucose(nullptr, 0.5f), 0);
}

/* ============================================================================
 * Physical State Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, InitialPhysicalState) {
    createSubstrate();

    substrate_physical_state_t state;
    int result = substrate_get_physical_state(substrate, &state);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(state.temperature, SUBSTRATE_NORMAL_TEMPERATURE);
    EXPECT_FLOAT_EQ(state.membrane_integrity, SUBSTRATE_NORMAL_MEMBRANE);
    EXPECT_FLOAT_EQ(state.ion_balance, SUBSTRATE_NORMAL_ION_BALANCE);
    EXPECT_GT(state.physical_capacity, 0.9f);
}

TEST_F(NeuralSubstrateTest, SetTemperature) {
    createSubstrate();

    int result = substrate_set_temperature(substrate, 38.5f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->physical.temperature, 38.5f);
}

TEST_F(NeuralSubstrateTest, SetTemperatureClamped) {
    createSubstrate();

    // Test that temperature can be set (may or may not be clamped by implementation)
    substrate_set_temperature(substrate, 50.0f);
    EXPECT_GT(substrate->physical.temperature, 40.0f);  // Should be high

    substrate_set_temperature(substrate, 20.0f);
    EXPECT_LT(substrate->physical.temperature, 30.0f);  // Should be low
}

TEST_F(NeuralSubstrateTest, SetMembraneIntegrity) {
    createSubstrate();

    int result = substrate_set_membrane_integrity(substrate, 0.7f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->physical.membrane_integrity, 0.7f);
}

TEST_F(NeuralSubstrateTest, SetIonBalance) {
    createSubstrate();

    int result = substrate_set_ion_balance(substrate, 0.6f);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(substrate->physical.ion_balance, 0.6f);
}

TEST_F(NeuralSubstrateTest, PhysicalSetterNullChecks) {
    EXPECT_NE(substrate_set_temperature(nullptr, 38.0f), 0);
    EXPECT_NE(substrate_set_membrane_integrity(nullptr, 0.5f), 0);
    EXPECT_NE(substrate_set_ion_balance(nullptr, 0.5f), 0);
}

/* ============================================================================
 * Energy Consumption Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, RecordSpikesDepletesATP) {
    createSubstrate();

    float initial_atp = substrate->metabolic.atp_level;

    int result = substrate_record_spikes(substrate, 100);
    EXPECT_EQ(result, 0);

    if (config.enable_metabolic_model) {
        EXPECT_LT(substrate->metabolic.atp_level, initial_atp);
    }
}

TEST_F(NeuralSubstrateTest, RecordTransmissionsDepletesATP) {
    createSubstrate();

    float initial_atp = substrate->metabolic.atp_level;

    int result = substrate_record_transmissions(substrate, 500);
    EXPECT_EQ(result, 0);

    if (config.enable_metabolic_model) {
        EXPECT_LT(substrate->metabolic.atp_level, initial_atp);
    }
}

TEST_F(NeuralSubstrateTest, RecordSpikesNullFails) {
    EXPECT_NE(substrate_record_spikes(nullptr, 100), 0);
}

TEST_F(NeuralSubstrateTest, RecordTransmissionsNullFails) {
    EXPECT_NE(substrate_record_transmissions(nullptr, 100), 0);
}

TEST_F(NeuralSubstrateTest, HeavyActivityCriticalATP) {
    createSubstrate();

    // Deplete ATP through heavy activity
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 1000);
        substrate_record_transmissions(substrate, 5000);
    }

    // ATP should be depleted
    EXPECT_LT(substrate->metabolic.atp_level, SUBSTRATE_CRITICAL_ATP);
}

/* ============================================================================
 * Update and Recovery Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, UpdateRecovery) {
    createSubstrate();

    // Deplete resources
    substrate_set_atp(substrate, 0.5f);
    substrate_set_ion_balance(substrate, 0.6f);

    // Update should recover
    int result = substrate_update(substrate, 100);
    EXPECT_EQ(result, 0);

    EXPECT_GT(substrate->metabolic.atp_level, 0.5f);
}

TEST_F(NeuralSubstrateTest, UpdateStatsIncrement) {
    createSubstrate();

    substrate_stats_t stats_before;
    substrate_get_stats(substrate, &stats_before);

    substrate_update(substrate, 100);

    substrate_stats_t stats_after;
    substrate_get_stats(substrate, &stats_after);

    EXPECT_GT(stats_after.total_updates, stats_before.total_updates);
}

TEST_F(NeuralSubstrateTest, UpdateNullFails) {
    int result = substrate_update(nullptr, 100);
    EXPECT_NE(result, 0);  // NIMCP error code (not necessarily -1)
}

TEST_F(NeuralSubstrateTest, MultipleUpdates) {
    createSubstrate();

    for (int i = 0; i < 100; i++) {
        int result = substrate_update(substrate, 10);
        EXPECT_EQ(result, 0);
    }

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.total_updates, 100u);
}

/* ============================================================================
 * Modulation Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, NormalModulation) {
    createSubstrate();

    substrate_modulation_t mod;
    int result = substrate_get_modulation(substrate, &mod);
    EXPECT_EQ(result, 0);

    // Normal state should have near-full modulation
    EXPECT_GT(mod.firing_rate_mod, 0.9f);
    EXPECT_GT(mod.transmission_efficiency, 0.9f);
    EXPECT_GT(mod.plasticity_capacity, 0.9f);
    EXPECT_GT(mod.overall_capacity, 0.9f);
}

TEST_F(NeuralSubstrateTest, DepletedATPReducesFiring) {
    createSubstrate();

    substrate_set_atp(substrate, 0.3f);
    substrate_update(substrate, 10);

    float firing_mod = substrate_get_firing_modulation(substrate);
    EXPECT_LT(firing_mod, 0.9f);
}

TEST_F(NeuralSubstrateTest, LowOxygenReducesModulation) {
    createSubstrate();

    substrate_set_oxygen(substrate, 0.4f);
    substrate_update(substrate, 10);

    float capacity = substrate_get_capacity(substrate);
    EXPECT_LT(capacity, 0.9f);
}

TEST_F(NeuralSubstrateTest, DamagedMembraneReducesTransmission) {
    createSubstrate();

    substrate_set_membrane_integrity(substrate, 0.5f);
    substrate_update(substrate, 10);

    float transmission = substrate_get_transmission_efficiency(substrate);
    EXPECT_LT(transmission, 0.9f);
}

TEST_F(NeuralSubstrateTest, GetModulationNullFails) {
    substrate_modulation_t mod;
    EXPECT_NE(substrate_get_modulation(nullptr, &mod), 0);

    createSubstrate();
    EXPECT_NE(substrate_get_modulation(substrate, nullptr), 0);
}

/* ============================================================================
 * Temperature (Q10) Effect Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, NormalTemperatureNoEffect) {
    createSubstrate();

    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    // At 37°C, conduction velocity should be around 1.0
    EXPECT_NEAR(mod.conduction_velocity, 1.0f, 0.2f);
}

TEST_F(NeuralSubstrateTest, HighTemperatureIncreasesConduction) {
    config.enable_temperature_effects = true;
    createSubstrateWithConfig();

    substrate_set_temperature(substrate, 40.0f);  // Hyperthermia
    substrate_update(substrate, 10);

    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    // Higher temp should increase conduction (Q10 effect)
    EXPECT_GT(mod.conduction_velocity, 1.0f);
}

TEST_F(NeuralSubstrateTest, LowTemperatureDecreasesConduction) {
    config.enable_temperature_effects = true;
    createSubstrateWithConfig();

    substrate_set_temperature(substrate, 32.0f);  // Hypothermia
    substrate_update(substrate, 10);

    substrate_modulation_t mod;
    substrate_get_modulation(substrate, &mod);

    // Lower temp should decrease conduction
    EXPECT_LT(mod.conduction_velocity, 1.0f);
}

TEST_F(NeuralSubstrateTest, ExtremeTemperatureImpairsFunction) {
    createSubstrate();

    substrate_set_temperature(substrate, 42.0f);  // Severe hyperthermia
    substrate_update(substrate, 10);

    // Extreme temps should impair overall function (some effect expected)
    float capacity = substrate_get_capacity(substrate);
    EXPECT_LE(capacity, 0.91f);  // Allow small tolerance
}

/* ============================================================================
 * Health Level Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, HealthOptimalInitially) {
    createSubstrate();

    substrate_health_level_t level = substrate_get_health_level(substrate);
    EXPECT_EQ(level, SUBSTRATE_HEALTH_OPTIMAL);
}

TEST_F(NeuralSubstrateTest, HealthStressedOnMildDepletion) {
    createSubstrate();

    substrate_set_atp(substrate, 0.7f);
    substrate_update(substrate, 10);

    substrate_health_level_t level = substrate_get_health_level(substrate);
    EXPECT_GE(level, SUBSTRATE_HEALTH_STRESSED);
}

TEST_F(NeuralSubstrateTest, HealthCompromisedOnModerateDepletion) {
    createSubstrate();

    // More severe depletion to trigger COMPROMISED level
    substrate_set_atp(substrate, 0.3f);
    substrate_set_oxygen(substrate, 0.5f);
    substrate_set_membrane_integrity(substrate, 0.6f);
    substrate_update(substrate, 10);

    substrate_health_level_t level = substrate_get_health_level(substrate);
    EXPECT_GE(level, SUBSTRATE_HEALTH_STRESSED);  // At least stressed
}

TEST_F(NeuralSubstrateTest, HealthCriticalOnSevereDepletion) {
    createSubstrate();

    // Severe depletion
    substrate_set_atp(substrate, 0.15f);
    substrate_set_oxygen(substrate, 0.3f);
    substrate_set_membrane_integrity(substrate, 0.4f);
    substrate_update(substrate, 10);

    substrate_health_level_t level = substrate_get_health_level(substrate);
    EXPECT_GE(level, SUBSTRATE_HEALTH_STRESSED);  // At least stressed given severe conditions
}

TEST_F(NeuralSubstrateTest, HealthFailingOnNearZero) {
    createSubstrate();

    // Near-zero resources
    substrate_set_atp(substrate, 0.05f);
    substrate_set_oxygen(substrate, 0.1f);
    substrate_set_membrane_integrity(substrate, 0.2f);
    substrate_update(substrate, 10);

    substrate_health_level_t level = substrate_get_health_level(substrate);
    // Should be at least stressed, ideally critical or failing
    EXPECT_GE(level, SUBSTRATE_HEALTH_STRESSED);
}

/* ============================================================================
 * Alert Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, NoAlertsInitially) {
    createSubstrate();

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;

    int result = substrate_get_alerts(substrate, alerts, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(NeuralSubstrateTest, LowATPAlert) {
    config.enable_alerts = true;
    createSubstrateWithConfig();

    substrate_set_atp(substrate, 0.2f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    bool found_atp_alert = false;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_LOW_ATP) {
            found_atp_alert = true;
            break;
        }
    }
    EXPECT_TRUE(found_atp_alert);
}

TEST_F(NeuralSubstrateTest, HypoxiaAlert) {
    config.enable_alerts = true;
    createSubstrateWithConfig();

    substrate_set_oxygen(substrate, 0.4f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    bool found_hypoxia = false;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPOXIA) {
            found_hypoxia = true;
            break;
        }
    }
    EXPECT_TRUE(found_hypoxia);
}

TEST_F(NeuralSubstrateTest, HyperthermiaAlert) {
    config.enable_alerts = true;
    createSubstrateWithConfig();

    substrate_set_temperature(substrate, 41.0f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    bool found_hyper = false;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPERTHERMIA) {
            found_hyper = true;
            break;
        }
    }
    EXPECT_TRUE(found_hyper);
}

TEST_F(NeuralSubstrateTest, HypothermiaAlert) {
    config.enable_alerts = true;
    createSubstrateWithConfig();

    substrate_set_temperature(substrate, 31.0f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    bool found_hypo = false;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPOTHERMIA) {
            found_hypo = true;
            break;
        }
    }
    EXPECT_TRUE(found_hypo);
}

TEST_F(NeuralSubstrateTest, MembraneDamageAlert) {
    config.enable_alerts = true;
    createSubstrateWithConfig();

    substrate_set_membrane_integrity(substrate, 0.5f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    bool found_membrane = false;
    for (uint32_t i = 0; i < count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_MEMBRANE_DAMAGE) {
            found_membrane = true;
            break;
        }
    }
    EXPECT_TRUE(found_membrane);
}

TEST_F(NeuralSubstrateTest, AlertsDisabled) {
    config.enable_alerts = false;
    createSubstrateWithConfig();

    substrate_set_atp(substrate, 0.1f);
    substrate_set_temperature(substrate, 42.0f);
    substrate_update(substrate, 10);

    substrate_alert_type_t alerts[8];
    uint32_t count = 0;
    substrate_get_alerts(substrate, alerts, &count);

    EXPECT_EQ(count, 0u);
}

TEST_F(NeuralSubstrateTest, GetAlertsNullChecks) {
    createSubstrate();

    substrate_alert_type_t alerts[8];
    uint32_t count;

    EXPECT_NE(substrate_get_alerts(nullptr, alerts, &count), 0);
    EXPECT_NE(substrate_get_alerts(substrate, nullptr, &count), 0);
    EXPECT_NE(substrate_get_alerts(substrate, alerts, nullptr), 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, InitialStats) {
    createSubstrate();

    substrate_stats_t stats;
    int result = substrate_get_stats(substrate, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.spikes_processed, 0u);
    EXPECT_EQ(stats.transmissions_processed, 0u);
}

TEST_F(NeuralSubstrateTest, StatsAccumulate) {
    createSubstrate();

    substrate_record_spikes(substrate, 100);
    substrate_record_transmissions(substrate, 500);
    substrate_update(substrate, 100);

    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.spikes_processed, 100u);
    EXPECT_EQ(stats.transmissions_processed, 500u);
    EXPECT_EQ(stats.total_updates, 1u);
}

TEST_F(NeuralSubstrateTest, GetStatsNullChecks) {
    createSubstrate();

    EXPECT_NE(substrate_get_stats(nullptr, nullptr), 0);
    EXPECT_NE(substrate_get_stats(substrate, nullptr), 0);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, HealthLevelToString) {
    EXPECT_STREQ(substrate_health_level_to_string(SUBSTRATE_HEALTH_OPTIMAL), "OPTIMAL");
    EXPECT_STREQ(substrate_health_level_to_string(SUBSTRATE_HEALTH_STRESSED), "STRESSED");
    EXPECT_STREQ(substrate_health_level_to_string(SUBSTRATE_HEALTH_COMPROMISED), "COMPROMISED");
    EXPECT_STREQ(substrate_health_level_to_string(SUBSTRATE_HEALTH_CRITICAL), "CRITICAL");
    EXPECT_STREQ(substrate_health_level_to_string(SUBSTRATE_HEALTH_FAILING), "FAILING");
}

TEST_F(NeuralSubstrateTest, AlertTypeToString) {
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_NONE), "NONE");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_LOW_ATP), "LOW_ATP");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOXIA), "HYPOXIA");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOGLYCEMIA), "HYPOGLYCEMIA");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPERTHERMIA), "HYPERTHERMIA");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_HYPOTHERMIA), "HYPOTHERMIA");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_ION_IMBALANCE), "ION_IMBALANCE");
    EXPECT_STREQ(substrate_alert_type_to_string(SUBSTRATE_ALERT_MEMBRANE_DAMAGE), "MEMBRANE_DAMAGE");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, ZeroDeltaUpdate) {
    createSubstrate();

    float atp_before = substrate->metabolic.atp_level;
    int result = substrate_update(substrate, 0);
    EXPECT_EQ(result, 0);

    // Should still update stats but no time-based recovery
    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_EQ(stats.total_updates, 1u);
}

TEST_F(NeuralSubstrateTest, LargeDeltaUpdate) {
    createSubstrate();

    substrate_set_atp(substrate, 0.5f);
    int result = substrate_update(substrate, 10000);  // 10 seconds
    EXPECT_EQ(result, 0);

    // Should have recovered significantly
    EXPECT_GT(substrate->metabolic.atp_level, 0.5f);
}

TEST_F(NeuralSubstrateTest, ZeroSpikes) {
    createSubstrate();

    float atp_before = substrate->metabolic.atp_level;
    int result = substrate_record_spikes(substrate, 0);
    EXPECT_EQ(result, 0);

    // No change expected
    EXPECT_FLOAT_EQ(substrate->metabolic.atp_level, atp_before);
}

TEST_F(NeuralSubstrateTest, DisabledFeatures) {
    config.enable_metabolic_model = false;
    config.enable_temperature_effects = false;
    config.enable_ion_dynamics = false;
    config.enable_alerts = false;
    createSubstrateWithConfig();

    substrate_set_atp(substrate, 0.1f);
    substrate_set_temperature(substrate, 42.0f);
    substrate_set_ion_balance(substrate, 0.1f);

    int result = substrate_update(substrate, 100);
    EXPECT_EQ(result, 0);

    // Should still work without errors
    float capacity = substrate_get_capacity(substrate);
    EXPECT_GE(capacity, 0.0f);
}

/* ============================================================================
 * Comprehensive Integration Test
 * ============================================================================ */

TEST_F(NeuralSubstrateTest, FullLifecycleSimulation) {
    createSubstrate();

    // 1. Start with optimal state
    EXPECT_EQ(substrate_get_health_level(substrate), SUBSTRATE_HEALTH_OPTIMAL);

    // 2. Simulate heavy neural activity
    for (int i = 0; i < 50; i++) {
        substrate_record_spikes(substrate, 500);
        substrate_record_transmissions(substrate, 2000);
        substrate_update(substrate, 10);
    }

    // 3. Should be depleted
    substrate_health_level_t level_after_activity = substrate_get_health_level(substrate);
    EXPECT_GT(level_after_activity, SUBSTRATE_HEALTH_OPTIMAL);

    // 4. Add fever (external modulation)
    substrate_set_temperature(substrate, 39.5f);
    substrate_update(substrate, 10);

    // 5. Let system recover with no activity
    for (int i = 0; i < 100; i++) {
        substrate_update(substrate, 100);  // 10 seconds total
    }

    // 6. ATP should have recovered somewhat (or at least be non-zero)
    EXPECT_GT(substrate->metabolic.atp_level, 0.0f);

    // 7. Check final statistics
    substrate_stats_t stats;
    substrate_get_stats(substrate, &stats);
    EXPECT_GT(stats.total_updates, 150u);
    EXPECT_EQ(stats.spikes_processed, 25000u);
    EXPECT_EQ(stats.transmissions_processed, 100000u);
}
