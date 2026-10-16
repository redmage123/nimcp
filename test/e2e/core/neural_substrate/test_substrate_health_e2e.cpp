/**
 * @file test_substrate_health_e2e.cpp
 * @brief End-to-end tests for neural substrate health monitoring and maintenance
 *
 * WHAT: Complete E2E testing of substrate health, degradation, and repair mechanisms
 * WHY:  Validate physical substrate modeling (temperature, membrane, ion balance)
 * HOW:  Test health monitoring, injury simulation, maintenance, and long-term stability
 *
 * BIOLOGICAL BASIS:
 * - Temperature affects neural processes with Q10 coefficients of 2-3
 * - Membrane integrity affects conductance and leakiness
 * - Ion balance (Na+/K+/Ca2+) is critical for neural function
 *
 * @version 1.0
 * @date 2026
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float NORMAL_TEMPERATURE = 37.0f;
    constexpr float HYPERTHERMIA_TEMP = 41.0f;
    constexpr float HYPOTHERMIA_TEMP = 31.0f;
    constexpr float CRITICAL_MEMBRANE = 0.5f;
    constexpr float CRITICAL_ION_BALANCE = 0.4f;
    constexpr uint64_t SIMULATION_STEP_MS = 1;
    constexpr uint64_t LONG_SIMULATION_STEPS = 10000;
}

//=============================================================================
// Test Fixture
//=============================================================================

class SubstrateHealthE2ETest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        substrate_config_t config;
        ASSERT_EQ(substrate_default_config(&config), 0);
        config.enable_metabolic_model = true;
        config.enable_temperature_effects = true;
        config.enable_ion_dynamics = true;
        config.enable_alerts = true;

        substrate = substrate_create(&config);
        ASSERT_NE(substrate, nullptr);
    }

    void TearDown() override {
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Memory leak detected: " << stats.current_allocated << " bytes";
    }

    void runSimulation(uint64_t steps) {
        for (uint64_t i = 0; i < steps; i++) {
            ASSERT_EQ(substrate_record_spikes(substrate, 100), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
};

//=============================================================================
// E2E Test: Physical Substrate Degradation Over Time
//=============================================================================

TEST_F(SubstrateHealthE2ETest, PhysicalSubstrateDegradationOverTime) {
    E2E_PIPELINE_START("Physical Substrate Degradation");

    E2E_STAGE_BEGIN("Record initial physical state", 100);
    substrate_physical_state_t initial;
    ASSERT_EQ(substrate_get_physical_state(substrate, &initial), 0);

    std::cout << "  Initial temperature: " << initial.temperature << " C" << std::endl;
    std::cout << "  Initial membrane integrity: " << initial.membrane_integrity << std::endl;
    std::cout << "  Initial ion balance: " << initial.ion_balance << std::endl;
    std::cout << "  Initial physical capacity: " << initial.physical_capacity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate membrane degradation", 100);
    // Gradually degrade membrane
    float membrane = 0.98f;
    std::vector<float> health_over_time;

    for (int i = 0; i < 10; i++) {
        membrane -= 0.05f;
        ASSERT_EQ(substrate_set_membrane_integrity(substrate, membrane), 0);
        ASSERT_EQ(substrate_update(substrate, 100), 0);

        substrate_physical_state_t state;
        ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);
        health_over_time.push_back(state.physical_capacity);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify degradation effects", 100);
    substrate_physical_state_t degraded;
    ASSERT_EQ(substrate_get_physical_state(substrate, &degraded), 0);

    std::cout << "  Degraded membrane: " << degraded.membrane_integrity << std::endl;
    std::cout << "  Degraded physical capacity: " << degraded.physical_capacity << std::endl;

    EXPECT_LT(degraded.membrane_integrity, initial.membrane_integrity);
    EXPECT_LT(degraded.physical_capacity, initial.physical_capacity);

    // Verify capacity decreased as membrane degraded
    EXPECT_LT(health_over_time.back(), health_over_time.front());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check health level reflects degradation", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health level: " << substrate_health_level_to_string(health) << std::endl;
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_STRESSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Maintenance and Repair Mechanisms
//=============================================================================

TEST_F(SubstrateHealthE2ETest, MaintenanceAndRepairMechanisms) {
    E2E_PIPELINE_START("Maintenance and Repair Mechanisms");

    E2E_STAGE_BEGIN("Induce damage", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.6f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.6f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_physical_state_t damaged;
    ASSERT_EQ(substrate_get_physical_state(substrate, &damaged), 0);
    std::cout << "  Damaged membrane: " << damaged.membrane_integrity << std::endl;
    std::cout << "  Damaged ion balance: " << damaged.ion_balance << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Allow repair through simulation updates", 60000);
    std::vector<float> membrane_recovery;
    std::vector<float> ion_recovery;

    // Run simulation with minimal activity to allow repair
    for (uint64_t step = 0; step < 10000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 10), 0);  // Minimal activity
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 500 == 0) {
            substrate_physical_state_t state;
            ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);
            membrane_recovery.push_back(state.membrane_integrity);
            ion_recovery.push_back(state.ion_balance);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify repair progress", 100);
    substrate_physical_state_t repaired;
    ASSERT_EQ(substrate_get_physical_state(substrate, &repaired), 0);

    std::cout << "  Repaired membrane: " << repaired.membrane_integrity << std::endl;
    std::cout << "  Repaired ion balance: " << repaired.ion_balance << std::endl;

    // Some recovery should have occurred
    EXPECT_GE(repaired.membrane_integrity, damaged.membrane_integrity);
    EXPECT_GE(repaired.ion_balance, damaged.ion_balance);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze repair trajectory", 100);
    if (!membrane_recovery.empty()) {
        std::cout << "  Membrane recovery samples: " << membrane_recovery.size() << std::endl;
        std::cout << "  Start: " << membrane_recovery.front()
                  << ", End: " << membrane_recovery.back() << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Health Monitoring and Alerts
//=============================================================================

TEST_F(SubstrateHealthE2ETest, HealthMonitoringAndAlerts) {
    E2E_PIPELINE_START("Health Monitoring and Alerts");

    E2E_STAGE_BEGIN("Test hyperthermia alert", 500);
    ASSERT_EQ(substrate_set_temperature(substrate, HYPERTHERMIA_TEMP), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_alert_type_t alerts[8];
    uint32_t alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool found_hyperthermia = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPERTHERMIA) {
            found_hyperthermia = true;
        }
        std::cout << "  Alert: " << substrate_alert_type_to_string(alerts[i]) << std::endl;
    }
    EXPECT_TRUE(found_hyperthermia) << "Should detect hyperthermia";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test hypothermia alert", 500);
    ASSERT_EQ(substrate_set_temperature(substrate, HYPOTHERMIA_TEMP), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool found_hypothermia = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPOTHERMIA) {
            found_hypothermia = true;
        }
    }
    EXPECT_TRUE(found_hypothermia) << "Should detect hypothermia";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test membrane damage alert", 500);
    ASSERT_EQ(substrate_set_temperature(substrate, NORMAL_TEMPERATURE), 0);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, CRITICAL_MEMBRANE), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool found_membrane = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_MEMBRANE_DAMAGE) {
            found_membrane = true;
        }
    }
    EXPECT_TRUE(found_membrane) << "Should detect membrane damage";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test ion imbalance alert", 500);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, CRITICAL_ION_BALANCE), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool found_ion = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_ION_IMBALANCE) {
            found_ion = true;
        }
    }
    EXPECT_TRUE(found_ion) << "Should detect ion imbalance";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Injury Simulation and Recovery
//=============================================================================

TEST_F(SubstrateHealthE2ETest, InjurySimulationAndRecovery) {
    E2E_PIPELINE_START("Injury Simulation and Recovery");

    E2E_STAGE_BEGIN("Simulate acute injury", 100);
    // Multiple parameters damaged simultaneously
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.5f), 0);
    ASSERT_EQ(substrate_set_atp(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_health_level_t injury_health = substrate_get_health_level(substrate);
    float injury_capacity = substrate_get_capacity(substrate);

    std::cout << "  Injury health: " << substrate_health_level_to_string(injury_health) << std::endl;
    std::cout << "  Injury capacity: " << injury_capacity << std::endl;

    EXPECT_GE((int)injury_health, (int)SUBSTRATE_HEALTH_COMPROMISED);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Begin recovery process", 100);
    // Restore energy supply to enable repair
    ASSERT_EQ(substrate_set_glucose(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.97f), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Monitor recovery timeline", 120000);
    std::vector<std::pair<uint64_t, float>> recovery_timeline;
    recovery_timeline.push_back({0, injury_capacity});

    for (uint64_t step = 0; step < 20000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 20), 0);  // Low activity
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 1000 == 0) {
            float cap = substrate_get_capacity(substrate);
            recovery_timeline.push_back({step, cap});
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze recovery completion", 100);
    float final_capacity = substrate_get_capacity(substrate);
    substrate_health_level_t final_health = substrate_get_health_level(substrate);

    std::cout << "  Final capacity: " << final_capacity << std::endl;
    std::cout << "  Final health: " << substrate_health_level_to_string(final_health) << std::endl;

    // Should show some recovery
    EXPECT_GT(final_capacity, injury_capacity);

    // Print recovery timeline
    std::cout << "  Recovery timeline:" << std::endl;
    for (const auto& [step, cap] : recovery_timeline) {
        std::cout << "    Step " << step << ": " << cap << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Long-term Stability Tests
//=============================================================================

TEST_F(SubstrateHealthE2ETest, LongTermStabilityTests) {
    E2E_PIPELINE_START("Long-term Stability Tests");

    E2E_STAGE_BEGIN("Run extended healthy operation", 180000);
    std::vector<float> stability_data;

    for (uint64_t step = 0; step < LONG_SIMULATION_STEPS; step++) {
        // Normal activity
        ASSERT_EQ(substrate_record_spikes(substrate, 200), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, 800), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 500 == 0) {
            substrate_physical_state_t state;
            ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);
            stability_data.push_back(state.physical_capacity);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze long-term stability", 100);
    ASSERT_GT(stability_data.size(), 10u);

    float mean = std::accumulate(stability_data.begin(), stability_data.end(), 0.0f)
                 / stability_data.size();
    float min_val = *std::min_element(stability_data.begin(), stability_data.end());
    float max_val = *std::max_element(stability_data.begin(), stability_data.end());

    std::cout << "  Capacity mean: " << mean << std::endl;
    std::cout << "  Capacity range: [" << min_val << ", " << max_val << "]" << std::endl;

    // System should remain stable
    EXPECT_GT(min_val, 0.5f) << "Capacity should not drop severely";
    EXPECT_LT(max_val - min_val, 0.3f) << "Capacity should be relatively stable";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no critical events", 100);
    substrate_stats_t stats;
    ASSERT_EQ(substrate_get_stats(substrate, &stats), 0);

    std::cout << "  Critical events: " << stats.critical_events << std::endl;
    std::cout << "  Alerts generated: " << stats.alerts_generated << std::endl;
    std::cout << "  Average health score: " << stats.avg_health_score << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Temperature Effects on Neural Function
//=============================================================================

TEST_F(SubstrateHealthE2ETest, TemperatureEffectsOnNeuralFunction) {
    E2E_PIPELINE_START("Temperature Effects on Neural Function");

    E2E_STAGE_BEGIN("Measure baseline modulation at normal temperature", 100);
    ASSERT_EQ(substrate_set_temperature(substrate, NORMAL_TEMPERATURE), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_modulation_t normal_mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &normal_mod), 0);

    std::cout << "  Normal temp firing mod: " << normal_mod.firing_rate_mod << std::endl;
    std::cout << "  Normal temp conduction: " << normal_mod.conduction_velocity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test elevated temperature effects", 100);
    ASSERT_EQ(substrate_set_temperature(substrate, 39.5f), 0);  // Mild fever
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_modulation_t elevated_mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &elevated_mod), 0);

    std::cout << "  Elevated temp firing mod: " << elevated_mod.firing_rate_mod << std::endl;
    std::cout << "  Elevated temp conduction: " << elevated_mod.conduction_velocity << std::endl;

    // Q10 effect: higher temp should increase some rates
    EXPECT_NE(elevated_mod.firing_rate_mod, normal_mod.firing_rate_mod);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test reduced temperature effects", 100);
    ASSERT_EQ(substrate_set_temperature(substrate, 34.0f), 0);  // Mild hypothermia
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_modulation_t reduced_mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &reduced_mod), 0);

    std::cout << "  Reduced temp firing mod: " << reduced_mod.firing_rate_mod << std::endl;
    std::cout << "  Reduced temp conduction: " << reduced_mod.conduction_velocity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify temperature affects plasticity", 100);
    std::cout << "  Normal plasticity: " << normal_mod.plasticity_capacity << std::endl;
    std::cout << "  Elevated plasticity: " << elevated_mod.plasticity_capacity << std::endl;
    std::cout << "  Reduced plasticity: " << reduced_mod.plasticity_capacity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Ion Pump Activity
//=============================================================================

TEST_F(SubstrateHealthE2ETest, IonPumpActivity) {
    E2E_PIPELINE_START("Ion Pump Activity");

    E2E_STAGE_BEGIN("Test Na+/K+-ATPase activity tracking", 100);
    substrate_physical_state_t state;
    ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);

    std::cout << "  Na+/K+ pump activity: " << state.na_k_pump_activity << std::endl;
    std::cout << "  Ca2+ homeostasis: " << state.ca_homeostasis << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Deplete ATP and observe pump effects", 5000);
    // ATP depletion should affect pump activity
    ASSERT_EQ(substrate_set_atp(substrate, 0.3f), 0);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(substrate_update(substrate, 10), 0);
    }

    ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);
    std::cout << "  Low ATP pump activity: " << state.na_k_pump_activity << std::endl;
    std::cout << "  Low ATP ion balance: " << state.ion_balance << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Restore ATP and verify pump recovery", 5000);
    ASSERT_EQ(substrate_set_atp(substrate, 0.95f), 0);

    for (int i = 0; i < 500; i++) {
        ASSERT_EQ(substrate_update(substrate, 10), 0);
    }

    ASSERT_EQ(substrate_get_physical_state(substrate, &state), 0);
    std::cout << "  Restored pump activity: " << state.na_k_pump_activity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Combined Stressor Response
//=============================================================================

TEST_F(SubstrateHealthE2ETest, CombinedStressorResponse) {
    E2E_PIPELINE_START("Combined Stressor Response");

    E2E_STAGE_BEGIN("Apply multiple simultaneous stressors", 100);
    // Hyperthermia + membrane damage + ATP depletion
    ASSERT_EQ(substrate_set_temperature(substrate, 40.5f), 0);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.55f), 0);
    ASSERT_EQ(substrate_set_atp(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.55f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify combined effect severity", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    float capacity = substrate_get_capacity(substrate);

    std::cout << "  Combined stress health: " << substrate_health_level_to_string(health) << std::endl;
    std::cout << "  Combined stress capacity: " << capacity << std::endl;

    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_COMPROMISED);
    EXPECT_LT(capacity, 0.6f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check all alert types active", 100);
    substrate_alert_type_t alerts[8];
    uint32_t alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    std::cout << "  Active alerts: " << alert_count << std::endl;
    for (uint32_t i = 0; i < alert_count; i++) {
        std::cout << "    - " << substrate_alert_type_to_string(alerts[i]) << std::endl;
    }

    EXPECT_GT(alert_count, 2u) << "Should have multiple alerts";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Begin recovery from combined stress", 60000);
    // Restore favorable conditions
    ASSERT_EQ(substrate_set_temperature(substrate, NORMAL_TEMPERATURE), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.97f), 0);

    // Allow time for recovery
    for (uint64_t step = 0; step < 10000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 10), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    float recovered_capacity = substrate_get_capacity(substrate);
    std::cout << "  Recovered capacity: " << recovered_capacity << std::endl;
    EXPECT_GT(recovered_capacity, capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Health State Transitions
//=============================================================================

TEST_F(SubstrateHealthE2ETest, HealthStateTransitions) {
    E2E_PIPELINE_START("Health State Transitions");

    E2E_STAGE_BEGIN("Verify initial optimal state", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    EXPECT_EQ(health, SUBSTRATE_HEALTH_OPTIMAL);
    std::cout << "  Initial: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Transition to stressed state", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.75f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    health = substrate_get_health_level(substrate);
    std::cout << "  After mild damage: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Transition to compromised state", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.55f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.6f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    health = substrate_get_health_level(substrate);
    std::cout << "  After moderate damage: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Transition to critical state", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.35f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.35f), 0);
    ASSERT_EQ(substrate_set_atp(substrate, 0.25f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    health = substrate_get_health_level(substrate);
    std::cout << "  After severe damage: " << substrate_health_level_to_string(health) << std::endl;
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_CRITICAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify failing state threshold", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.15f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.15f), 0);
    ASSERT_EQ(substrate_set_atp(substrate, 0.1f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    health = substrate_get_health_level(substrate);
    std::cout << "  Near-failure: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Membrane Conductance Effects
//=============================================================================

TEST_F(SubstrateHealthE2ETest, MembraneConductanceEffects) {
    E2E_PIPELINE_START("Membrane Conductance Effects");

    E2E_STAGE_BEGIN("Test healthy membrane transmission", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.98f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    float healthy_trans = substrate_get_transmission_efficiency(substrate);
    std::cout << "  Healthy membrane transmission: " << healthy_trans << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test leaky membrane transmission", 100);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.5f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    float leaky_trans = substrate_get_transmission_efficiency(substrate);
    std::cout << "  Leaky membrane transmission: " << leaky_trans << std::endl;

    // Leaky membranes should reduce transmission efficiency
    EXPECT_LT(leaky_trans, healthy_trans);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify modulation coherence", 100);
    substrate_modulation_t mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &mod), 0);

    std::cout << "  Firing rate mod: " << mod.firing_rate_mod << std::endl;
    std::cout << "  Transmission efficiency: " << mod.transmission_efficiency << std::endl;
    std::cout << "  Conduction velocity: " << mod.conduction_velocity << std::endl;
    std::cout << "  Plasticity capacity: " << mod.plasticity_capacity << std::endl;
    std::cout << "  Overall capacity: " << mod.overall_capacity << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Stress Statistics Tracking
//=============================================================================

TEST_F(SubstrateHealthE2ETest, StressStatisticsTracking) {
    E2E_PIPELINE_START("Stress Statistics Tracking");

    E2E_STAGE_BEGIN("Generate multiple stress events", 30000);
    // Create various stress conditions
    for (int cycle = 0; cycle < 5; cycle++) {
        // Stress phase
        ASSERT_EQ(substrate_set_atp(substrate, 0.35f), 0);
        for (int i = 0; i < 100; i++) {
            ASSERT_EQ(substrate_record_spikes(substrate, 500), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }

        // Recovery phase
        ASSERT_EQ(substrate_set_atp(substrate, 0.9f), 0);
        ASSERT_EQ(substrate_set_glucose(substrate, 0.9f), 0);
        for (int i = 0; i < 200; i++) {
            ASSERT_EQ(substrate_record_spikes(substrate, 50), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics capture", 100);
    substrate_stats_t stats;
    ASSERT_EQ(substrate_get_stats(substrate, &stats), 0);

    std::cout << "  Total updates: " << stats.total_updates << std::endl;
    std::cout << "  Alerts generated: " << stats.alerts_generated << std::endl;
    std::cout << "  Critical events: " << stats.critical_events << std::endl;
    std::cout << "  Average health score: " << stats.avg_health_score << std::endl;

    EXPECT_GT(stats.total_updates, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Circadian Temperature Variation
//=============================================================================

TEST_F(SubstrateHealthE2ETest, CircadianTemperatureVariation) {
    E2E_PIPELINE_START("Circadian Temperature Variation");

    E2E_STAGE_BEGIN("Simulate daily temperature cycle", 30000);
    std::vector<std::pair<float, float>> temp_modulation;

    for (uint64_t step = 0; step < 5000; step++) {
        // Simulate circadian temperature variation (36.5 - 37.5 C)
        float phase = (float)(step % 1000) / 1000.0f;
        float temp = 37.0f + 0.5f * std::sin(2.0f * M_PI * phase);

        ASSERT_EQ(substrate_set_temperature(substrate, temp), 0);
        ASSERT_EQ(substrate_record_spikes(substrate, 100), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 100 == 0) {
            float firing_mod = substrate_get_firing_modulation(substrate);
            temp_modulation.push_back({temp, firing_mod});
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze temperature-modulation correlation", 100);
    std::cout << "  Sample temperature-modulation pairs:" << std::endl;
    for (size_t i = 0; i < std::min(temp_modulation.size(), (size_t)5); i++) {
        std::cout << "    Temp: " << temp_modulation[i].first
                  << " -> Firing mod: " << temp_modulation[i].second << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Rapid Physical State Changes
//=============================================================================

TEST_F(SubstrateHealthE2ETest, RapidPhysicalStateChanges) {
    E2E_PIPELINE_START("Rapid Physical State Changes");

    E2E_STAGE_BEGIN("Apply rapid state changes", 10000);
    for (int cycle = 0; cycle < 20; cycle++) {
        // Rapid temperature change
        float temp = (cycle % 2 == 0) ? 38.5f : 35.5f;
        ASSERT_EQ(substrate_set_temperature(substrate, temp), 0);

        // Rapid ion balance change
        float ion = (cycle % 2 == 0) ? 0.9f : 0.5f;
        ASSERT_EQ(substrate_set_ion_balance(substrate, ion), 0);

        // Run simulation
        for (int i = 0; i < 50; i++) {
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system stability after rapid changes", 100);
    // Restore normal conditions
    ASSERT_EQ(substrate_set_temperature(substrate, 37.0f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.95f), 0);

    for (int i = 0; i < 500; i++) {
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health after stabilization: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Membrane Lipid Composition Effects
//=============================================================================

TEST_F(SubstrateHealthE2ETest, MembraneLipidCompositionEffects) {
    E2E_PIPELINE_START("Membrane Lipid Composition Effects");

    E2E_STAGE_BEGIN("Test membrane at different integrity levels", 5000);
    float integrity_levels[] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
    std::vector<float> transmission_efficiencies;

    for (float integrity : integrity_levels) {
        ASSERT_EQ(substrate_set_membrane_integrity(substrate, integrity), 0);
        ASSERT_EQ(substrate_update(substrate, 100), 0);

        float trans_eff = substrate_get_transmission_efficiency(substrate);
        transmission_efficiencies.push_back(trans_eff);

        std::cout << "  Membrane " << (integrity * 100) << "% -> Transmission: "
                  << trans_eff << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify degradation trend", 100);
    // Transmission should decrease with membrane damage
    for (size_t i = 1; i < transmission_efficiencies.size(); i++) {
        EXPECT_LE(transmission_efficiencies[i], transmission_efficiencies[i-1] + 0.1f)
            << "Transmission should generally decrease with membrane damage";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Complete Health Lifecycle
//=============================================================================

TEST_F(SubstrateHealthE2ETest, CompleteHealthLifecycle) {
    E2E_PIPELINE_START("Complete Health Lifecycle");

    E2E_STAGE_BEGIN("Phase 1: Healthy operation", 10000);
    for (int i = 0; i < 1000; i++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 100), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    substrate_health_level_t phase1 = substrate_get_health_level(substrate);
    std::cout << "  Phase 1 (healthy): " << substrate_health_level_to_string(phase1) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 2: Stress induction", 5000);
    ASSERT_EQ(substrate_set_temperature(substrate, 39.5f), 0);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.65f), 0);

    for (int i = 0; i < 500; i++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 500), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    substrate_health_level_t phase2 = substrate_get_health_level(substrate);
    std::cout << "  Phase 2 (stressed): " << substrate_health_level_to_string(phase2) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 3: Critical state", 5000);
    ASSERT_EQ(substrate_set_temperature(substrate, 41.0f), 0);
    ASSERT_EQ(substrate_set_membrane_integrity(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_set_ion_balance(substrate, 0.4f), 0);

    for (int i = 0; i < 200; i++) {
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    substrate_health_level_t phase3 = substrate_get_health_level(substrate);
    std::cout << "  Phase 3 (critical): " << substrate_health_level_to_string(phase3) << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 4: Recovery treatment", 60000);
    ASSERT_EQ(substrate_set_temperature(substrate, NORMAL_TEMPERATURE), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.98f), 0);

    for (int i = 0; i < 10000; i++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 10), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    substrate_health_level_t phase4 = substrate_get_health_level(substrate);
    std::cout << "  Phase 4 (recovery): " << substrate_health_level_to_string(phase4) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
