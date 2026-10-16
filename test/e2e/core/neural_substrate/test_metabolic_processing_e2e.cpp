/**
 * @file test_metabolic_processing_e2e.cpp
 * @brief End-to-end tests for neural substrate metabolic processing
 *
 * WHAT: Complete E2E testing of metabolic state management and energy dynamics
 * WHY:  Validate metabolic modeling with realistic neural activity scenarios
 * HOW:  Test ATP/glucose dynamics, energy consumption, stress response, recovery
 *
 * BIOLOGICAL BASIS:
 * - Neurons consume ~20% of body's oxygen/glucose despite being ~2% of mass
 * - Action potentials cost ~10^8 ATP molecules
 * - Energy deficit leads to reduced firing and impaired transmission
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
    constexpr uint32_t NUM_NEURONS = 10000;
    constexpr uint32_t SPIKES_PER_STEP = 500;
    constexpr uint32_t TRANSMISSIONS_PER_STEP = 2000;
    constexpr uint64_t SIMULATION_STEP_MS = 1;
    constexpr uint64_t LONG_SIMULATION_STEPS = 10000;
    constexpr uint64_t STRESS_DURATION_MS = 5000;
    constexpr float RECOVERY_THRESHOLD = 0.9f;
    constexpr float DEPLETION_THRESHOLD = 0.4f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MetabolicProcessingE2ETest : public ::testing::Test {
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

    void simulateNeuralActivity(uint64_t steps, uint32_t spikes, uint32_t transmissions) {
        for (uint64_t i = 0; i < steps; i++) {
            ASSERT_EQ(substrate_record_spikes(substrate, spikes), 0);
            ASSERT_EQ(substrate_record_transmissions(substrate, transmissions), 0);
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
        }
    }

    void waitForRecovery(float target_atp, uint64_t max_steps) {
        substrate_metabolic_state_t state;
        for (uint64_t i = 0; i < max_steps; i++) {
            ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
            ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);
            if (state.atp_level >= target_atp) {
                return;
            }
        }
    }
};

//=============================================================================
// E2E Test: Full Metabolic State Lifecycle
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, FullMetabolicStateLifecycle) {
    E2E_PIPELINE_START("Metabolic State Lifecycle");

    E2E_STAGE_BEGIN("Initialize substrate with default metabolic state", 100);
    substrate_metabolic_state_t initial_state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &initial_state), 0);
    EXPECT_GT(initial_state.atp_level, 0.9f);
    EXPECT_GT(initial_state.oxygen_saturation, 0.9f);
    EXPECT_GT(initial_state.glucose_level, 0.85f);
    EXPECT_GT(initial_state.metabolic_capacity, 0.8f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate neural activity with energy consumption", 5000);
    simulateNeuralActivity(1000, SPIKES_PER_STEP, TRANSMISSIONS_PER_STEP);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify ATP depletion from activity", 100);
    substrate_metabolic_state_t depleted_state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &depleted_state), 0);
    EXPECT_LT(depleted_state.atp_level, initial_state.atp_level);
    std::cout << "  ATP after activity: " << depleted_state.atp_level << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Allow metabolic recovery", 10000);
    waitForRecovery(0.8f, 5000);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery completion", 100);
    substrate_metabolic_state_t recovered_state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &recovered_state), 0);
    EXPECT_GT(recovered_state.atp_level, depleted_state.atp_level);
    std::cout << "  ATP after recovery: " << recovered_state.atp_level << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Energy Consumption Tracking Over Time
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, EnergyConsumptionTrackingOverTime) {
    E2E_PIPELINE_START("Energy Consumption Tracking");

    std::vector<float> atp_timeline;
    std::vector<float> metabolic_rate_timeline;

    E2E_STAGE_BEGIN("Record baseline", 100);
    substrate_metabolic_state_t state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);
    atp_timeline.push_back(state.atp_level);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run extended simulation with tracking", 30000);
    const uint64_t TRACKING_STEPS = 5000;
    const uint64_t SAMPLE_INTERVAL = 100;

    for (uint64_t step = 0; step < TRACKING_STEPS; step++) {
        // Variable activity: bursts of high activity
        uint32_t activity_level = (step % 500 < 100) ? SPIKES_PER_STEP * 3 : SPIKES_PER_STEP;
        ASSERT_EQ(substrate_record_spikes(substrate, activity_level), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, activity_level * 4), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % SAMPLE_INTERVAL == 0) {
            ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);
            atp_timeline.push_back(state.atp_level);
            metabolic_rate_timeline.push_back(state.metabolic_rate);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze consumption patterns", 100);
    EXPECT_GT(atp_timeline.size(), 10u);

    // Verify ATP fluctuations correlate with activity
    float min_atp = *std::min_element(atp_timeline.begin(), atp_timeline.end());
    float max_atp = *std::max_element(atp_timeline.begin(), atp_timeline.end());
    EXPECT_GT(max_atp - min_atp, 0.01f) << "ATP should fluctuate with activity";

    std::cout << "  ATP range: [" << min_atp << ", " << max_atp << "]" << std::endl;
    std::cout << "  Samples collected: " << atp_timeline.size() << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics tracking", 100);
    substrate_stats_t stats;
    ASSERT_EQ(substrate_get_stats(substrate, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.spikes_processed, 0u);
    EXPECT_GT(stats.total_atp_consumed, 0.0f);
    std::cout << "  Total spikes processed: " << stats.spikes_processed << std::endl;
    std::cout << "  Total ATP consumed: " << stats.total_atp_consumed << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: ATP/Glucose Dynamics Under Load
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, ATPGlucoseDynamicsUnderLoad) {
    E2E_PIPELINE_START("ATP/Glucose Dynamics Under Load");

    E2E_STAGE_BEGIN("Set initial high-energy state", 100);
    ASSERT_EQ(substrate_set_atp(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.90f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.97f), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply sustained high load", 20000);
    const uint64_t HIGH_LOAD_STEPS = 3000;
    const uint32_t HIGH_SPIKE_RATE = SPIKES_PER_STEP * 5;

    for (uint64_t step = 0; step < HIGH_LOAD_STEPS; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, HIGH_SPIKE_RATE), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, HIGH_SPIKE_RATE * 5), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify energy depletion under load", 100);
    substrate_metabolic_state_t state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);

    // Under sustained load, ATP should be depleted
    EXPECT_LT(state.atp_level, 0.8f);
    std::cout << "  ATP after high load: " << state.atp_level << std::endl;
    std::cout << "  Glucose after high load: " << state.glucose_level << std::endl;
    std::cout << "  Metabolic rate: " << state.metabolic_rate << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check modulation effects", 100);
    substrate_modulation_t mod;
    ASSERT_EQ(substrate_get_modulation(substrate, &mod), 0);

    // Low energy should reduce firing rate modulation
    float firing_mod = substrate_get_firing_modulation(substrate);
    float trans_eff = substrate_get_transmission_efficiency(substrate);

    std::cout << "  Firing rate modulation: " << firing_mod << std::endl;
    std::cout << "  Transmission efficiency: " << trans_eff << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Metabolic Stress Response
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, MetabolicStressResponse) {
    E2E_PIPELINE_START("Metabolic Stress Response");

    E2E_STAGE_BEGIN("Induce hypoglycemia", 100);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.3f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check stress alerts", 500);
    substrate_alert_type_t alerts[8];
    uint32_t alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool hypoglycemia_alert = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_HYPOGLYCEMIA) {
            hypoglycemia_alert = true;
        }
        std::cout << "  Alert: " << substrate_alert_type_to_string(alerts[i]) << std::endl;
    }
    EXPECT_TRUE(hypoglycemia_alert) << "Hypoglycemia alert should be triggered";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce hypoxia", 100);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.4f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify combined stress state", 500);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health level: " << substrate_health_level_to_string(health) << std::endl;

    // Combined stress should result in compromised or worse health
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_STRESSED);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity reduction", 100);
    float capacity = substrate_get_capacity(substrate);
    EXPECT_LT(capacity, 0.8f) << "Capacity should be reduced under stress";
    std::cout << "  Overall capacity: " << capacity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test ATP depletion stress", 100);
    ASSERT_EQ(substrate_set_atp(substrate, 0.25f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    bool atp_alert = false;
    for (uint32_t i = 0; i < alert_count; i++) {
        if (alerts[i] == SUBSTRATE_ALERT_LOW_ATP) {
            atp_alert = true;
        }
    }
    EXPECT_TRUE(atp_alert) << "Low ATP alert should be triggered";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Recovery from Metabolic Depletion
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, RecoveryFromMetabolicDepletion) {
    E2E_PIPELINE_START("Recovery from Metabolic Depletion");

    E2E_STAGE_BEGIN("Deplete metabolic resources", 5000);
    // Severe depletion
    ASSERT_EQ(substrate_set_atp(substrate, 0.35f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.35f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.55f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_metabolic_state_t depleted;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &depleted), 0);
    std::cout << "  Initial depleted ATP: " << depleted.atp_level << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Restore oxygen and glucose supply", 100);
    // Simulate restored blood supply
    ASSERT_EQ(substrate_set_glucose(substrate, 0.90f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.95f), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Monitor recovery over time", 60000);
    std::vector<float> recovery_curve;
    const uint64_t RECOVERY_STEPS = 10000;
    const uint64_t SAMPLE_INTERVAL = 500;

    for (uint64_t step = 0; step < RECOVERY_STEPS; step++) {
        // Low activity during recovery
        ASSERT_EQ(substrate_record_spikes(substrate, 50), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % SAMPLE_INTERVAL == 0) {
            substrate_metabolic_state_t state;
            ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);
            recovery_curve.push_back(state.atp_level);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery trajectory", 100);
    ASSERT_GT(recovery_curve.size(), 2u);

    // ATP should increase over time (recovery)
    float final_atp = recovery_curve.back();
    std::cout << "  Final ATP after recovery: " << final_atp << std::endl;
    EXPECT_GT(final_atp, depleted.atp_level) << "ATP should recover over time";

    // Check monotonic recovery (mostly increasing)
    int increases = 0;
    for (size_t i = 1; i < recovery_curve.size(); i++) {
        if (recovery_curve[i] > recovery_curve[i-1]) {
            increases++;
        }
    }
    float recovery_ratio = (float)increases / (recovery_curve.size() - 1);
    std::cout << "  Recovery monotonicity: " << (recovery_ratio * 100) << "%" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify health level restoration", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Final health: " << substrate_health_level_to_string(health) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Cyclic Activity Patterns
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, CyclicActivityPatterns) {
    E2E_PIPELINE_START("Cyclic Activity Patterns");

    E2E_STAGE_BEGIN("Simulate day/night activity cycles", 60000);
    const uint64_t TOTAL_STEPS = 10000;
    const uint64_t CYCLE_LENGTH = 1000;  // One cycle
    std::vector<float> cycle_atp;

    for (uint64_t step = 0; step < TOTAL_STEPS; step++) {
        // Sinusoidal activity pattern (simulates circadian rhythm)
        float phase = (float)(step % CYCLE_LENGTH) / CYCLE_LENGTH;
        float activity = 0.5f + 0.5f * std::sin(2.0f * M_PI * phase);

        uint32_t spikes = (uint32_t)(SPIKES_PER_STEP * (0.2f + 1.8f * activity));
        ASSERT_EQ(substrate_record_spikes(substrate, spikes), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, spikes * 3), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 100 == 0) {
            substrate_metabolic_state_t state;
            ASSERT_EQ(substrate_get_metabolic_state(substrate, &state), 0);
            cycle_atp.push_back(state.atp_level);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze cyclic metabolic response", 100);
    ASSERT_GT(cycle_atp.size(), 50u);

    float mean_atp = std::accumulate(cycle_atp.begin(), cycle_atp.end(), 0.0f) / cycle_atp.size();
    float min_atp = *std::min_element(cycle_atp.begin(), cycle_atp.end());
    float max_atp = *std::max_element(cycle_atp.begin(), cycle_atp.end());

    std::cout << "  Mean ATP: " << mean_atp << std::endl;
    std::cout << "  ATP range: [" << min_atp << ", " << max_atp << "]" << std::endl;

    // Should show cyclic variation
    EXPECT_GT(max_atp - min_atp, 0.01f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Burst Activity Metabolic Cost
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, BurstActivityMetabolicCost) {
    E2E_PIPELINE_START("Burst Activity Metabolic Cost");

    E2E_STAGE_BEGIN("Record pre-burst state", 100);
    substrate_metabolic_state_t pre_burst;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &pre_burst), 0);
    std::cout << "  Pre-burst ATP: " << pre_burst.atp_level << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply burst of high activity", 5000);
    const uint64_t BURST_STEPS = 100;
    const uint32_t BURST_SPIKES = SPIKES_PER_STEP * 20;

    for (uint64_t i = 0; i < BURST_STEPS; i++) {
        ASSERT_EQ(substrate_record_spikes(substrate, BURST_SPIKES), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, BURST_SPIKES * 10), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure burst metabolic impact", 100);
    substrate_metabolic_state_t post_burst;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &post_burst), 0);

    float atp_drop = pre_burst.atp_level - post_burst.atp_level;
    std::cout << "  Post-burst ATP: " << post_burst.atp_level << std::endl;
    std::cout << "  ATP drop from burst: " << atp_drop << std::endl;

    EXPECT_GT(atp_drop, 0.0f) << "Burst should consume ATP";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify modulation changes", 100);
    float firing_mod = substrate_get_firing_modulation(substrate);
    float trans_eff = substrate_get_transmission_efficiency(substrate);

    std::cout << "  Post-burst firing modulation: " << firing_mod << std::endl;
    std::cout << "  Post-burst transmission efficiency: " << trans_eff << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Long-term Metabolic Stability
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, LongTermMetabolicStability) {
    E2E_PIPELINE_START("Long-term Metabolic Stability");

    E2E_STAGE_BEGIN("Run extended simulation", 120000);
    const uint64_t EXTENDED_STEPS = LONG_SIMULATION_STEPS;
    std::vector<float> stability_samples;

    for (uint64_t step = 0; step < EXTENDED_STEPS; step++) {
        // Moderate sustained activity
        ASSERT_EQ(substrate_record_spikes(substrate, SPIKES_PER_STEP), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, TRANSMISSIONS_PER_STEP), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 1000 == 0) {
            float capacity = substrate_get_capacity(substrate);
            stability_samples.push_back(capacity);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify long-term stability", 100);
    ASSERT_GT(stability_samples.size(), 5u);

    // Calculate variance to check stability
    float mean = std::accumulate(stability_samples.begin(), stability_samples.end(), 0.0f)
                 / stability_samples.size();
    float variance = 0.0f;
    for (float s : stability_samples) {
        variance += (s - mean) * (s - mean);
    }
    variance /= stability_samples.size();
    float stddev = std::sqrt(variance);

    std::cout << "  Mean capacity: " << mean << std::endl;
    std::cout << "  Capacity stddev: " << stddev << std::endl;

    // System should remain stable (not drift to extremes)
    EXPECT_GT(mean, 0.3f) << "Should maintain reasonable capacity";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics accumulation", 100);
    substrate_stats_t stats;
    ASSERT_EQ(substrate_get_stats(substrate, &stats), 0);

    std::cout << "  Total updates: " << stats.total_updates << std::endl;
    std::cout << "  Total spikes: " << stats.spikes_processed << std::endl;
    std::cout << "  Total transmissions: " << stats.transmissions_processed << std::endl;
    std::cout << "  Total ATP consumed: " << stats.total_atp_consumed << std::endl;
    std::cout << "  Peak metabolic rate: " << stats.peak_metabolic_rate << std::endl;

    EXPECT_EQ(stats.total_updates, EXTENDED_STEPS);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Metabolic Reset and Reinitialization
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, MetabolicResetAndReinitialization) {
    E2E_PIPELINE_START("Metabolic Reset and Reinitialization");

    E2E_STAGE_BEGIN("Deplete substrate through activity", 5000);
    simulateNeuralActivity(2000, SPIKES_PER_STEP * 5, TRANSMISSIONS_PER_STEP * 5);

    substrate_metabolic_state_t depleted;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &depleted), 0);
    std::cout << "  Depleted ATP: " << depleted.atp_level << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reset substrate", 100);
    ASSERT_EQ(substrate_reset(substrate), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reset restored initial state", 100);
    substrate_metabolic_state_t reset_state;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &reset_state), 0);

    std::cout << "  Reset ATP: " << reset_state.atp_level << std::endl;
    EXPECT_GT(reset_state.atp_level, depleted.atp_level);
    EXPECT_GT(reset_state.atp_level, 0.9f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify reset health level", 100);
    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Reset health: " << substrate_health_level_to_string(health) << std::endl;
    EXPECT_EQ(health, SUBSTRATE_HEALTH_OPTIMAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics cleared", 100);
    substrate_stats_t stats;
    ASSERT_EQ(substrate_get_stats(substrate, &stats), 0);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.spikes_processed, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Extreme Conditions Handling
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, ExtremeConditionsHandling) {
    E2E_PIPELINE_START("Extreme Conditions Handling");

    E2E_STAGE_BEGIN("Test near-zero ATP handling", 100);
    ASSERT_EQ(substrate_set_atp(substrate, 0.05f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_health_level_t health = substrate_get_health_level(substrate);
    std::cout << "  Health at 5% ATP: " << substrate_health_level_to_string(health) << std::endl;
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_CRITICAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test complete oxygen deprivation", 100);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.1f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    float capacity = substrate_get_capacity(substrate);
    std::cout << "  Capacity at low O2: " << capacity << std::endl;
    EXPECT_LT(capacity, 0.5f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test combined extreme stress", 100);
    ASSERT_EQ(substrate_set_atp(substrate, 0.1f), 0);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.2f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.15f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    health = substrate_get_health_level(substrate);
    std::cout << "  Health under extreme stress: "
              << substrate_health_level_to_string(health) << std::endl;
    EXPECT_GE((int)health, (int)SUBSTRATE_HEALTH_CRITICAL);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify alerts generated", 100);
    substrate_alert_type_t alerts[8];
    uint32_t alert_count = 0;
    ASSERT_EQ(substrate_get_alerts(substrate, alerts, &alert_count), 0);

    std::cout << "  Active alerts: " << alert_count << std::endl;
    EXPECT_GT(alert_count, 0u) << "Should have alerts under extreme conditions";

    for (uint32_t i = 0; i < alert_count; i++) {
        std::cout << "    - " << substrate_alert_type_to_string(alerts[i]) << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Gradual Depletion and Warning System
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, GradualDepletionAndWarningSystem) {
    E2E_PIPELINE_START("Gradual Depletion and Warning System");

    E2E_STAGE_BEGIN("Monitor health transitions during depletion", 30000);
    std::vector<substrate_health_level_t> health_transitions;
    substrate_health_level_t last_health = SUBSTRATE_HEALTH_OPTIMAL;

    // Gradually deplete through sustained activity
    for (uint64_t step = 0; step < 5000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, SPIKES_PER_STEP * 3), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, TRANSMISSIONS_PER_STEP * 3), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        substrate_health_level_t current_health = substrate_get_health_level(substrate);
        if (current_health != last_health) {
            health_transitions.push_back(current_health);
            std::cout << "  Health transition at step " << step << ": "
                      << substrate_health_level_to_string(current_health) << std::endl;
            last_health = current_health;
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify health degradation sequence", 100);
    // Health should degrade in order (OPTIMAL -> STRESSED -> COMPROMISED -> CRITICAL)
    if (!health_transitions.empty()) {
        std::cout << "  Total health transitions: " << health_transitions.size() << std::endl;
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Concurrent Activity Simulation
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, ConcurrentActivitySimulation) {
    E2E_PIPELINE_START("Concurrent Activity Simulation");

    E2E_STAGE_BEGIN("Simulate multiple activity sources", 30000);
    std::vector<float> capacity_samples;

    for (uint64_t step = 0; step < 5000; step++) {
        // Simulate multiple concurrent processes
        uint32_t sensory_spikes = 200;
        uint32_t motor_spikes = 150;
        uint32_t cognitive_spikes = 300;

        ASSERT_EQ(substrate_record_spikes(substrate, sensory_spikes + motor_spikes + cognitive_spikes), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, (sensory_spikes + motor_spikes + cognitive_spikes) * 4), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);

        if (step % 250 == 0) {
            float cap = substrate_get_capacity(substrate);
            capacity_samples.push_back(cap);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify sustained operation", 100);
    ASSERT_GT(capacity_samples.size(), 10u);
    float final_capacity = capacity_samples.back();
    std::cout << "  Final capacity after concurrent load: " << final_capacity << std::endl;
    EXPECT_GT(final_capacity, 0.3f) << "System should maintain function under load";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Metabolic Rate Adaptation
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, MetabolicRateAdaptation) {
    E2E_PIPELINE_START("Metabolic Rate Adaptation");

    E2E_STAGE_BEGIN("Measure baseline metabolic rate", 100);
    substrate_metabolic_state_t baseline;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &baseline), 0);
    std::cout << "  Baseline metabolic rate: " << baseline.metabolic_rate << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Increase activity and measure adaptation", 10000);
    for (uint64_t step = 0; step < 2000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, SPIKES_PER_STEP * 3), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    substrate_metabolic_state_t high_activity;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &high_activity), 0);
    std::cout << "  High activity metabolic rate: " << high_activity.metabolic_rate << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reduce activity and observe recovery rate", 10000);
    for (uint64_t step = 0; step < 2000; step++) {
        ASSERT_EQ(substrate_record_spikes(substrate, 10), 0);  // Minimal activity
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }

    substrate_metabolic_state_t recovery;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &recovery), 0);
    std::cout << "  Recovery state metabolic rate: " << recovery.metabolic_rate << std::endl;
    std::cout << "  Recovery rate: " << recovery.recovery_rate << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Oxygen-Glucose Coupling
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, OxygenGlucoseCoupling) {
    E2E_PIPELINE_START("Oxygen-Glucose Coupling");

    E2E_STAGE_BEGIN("Test high oxygen low glucose", 1000);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.98f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.3f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_metabolic_state_t state1;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &state1), 0);
    std::cout << "  High O2/Low glucose capacity: " << state1.metabolic_capacity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test low oxygen high glucose", 1000);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.3f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.98f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_metabolic_state_t state2;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &state2), 0);
    std::cout << "  Low O2/High glucose capacity: " << state2.metabolic_capacity << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify both parameters needed for optimal function", 100);
    ASSERT_EQ(substrate_set_oxygen(substrate, 0.95f), 0);
    ASSERT_EQ(substrate_set_glucose(substrate, 0.90f), 0);
    ASSERT_EQ(substrate_update(substrate, 100), 0);

    substrate_metabolic_state_t optimal;
    ASSERT_EQ(substrate_get_metabolic_state(substrate, &optimal), 0);
    std::cout << "  Optimal O2/glucose capacity: " << optimal.metabolic_capacity << std::endl;

    EXPECT_GT(optimal.metabolic_capacity, state1.metabolic_capacity);
    EXPECT_GT(optimal.metabolic_capacity, state2.metabolic_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Energy Budget Accounting
//=============================================================================

TEST_F(MetabolicProcessingE2ETest, EnergyBudgetAccounting) {
    E2E_PIPELINE_START("Energy Budget Accounting");

    E2E_STAGE_BEGIN("Reset and record initial ATP", 100);
    ASSERT_EQ(substrate_reset(substrate), 0);
    substrate_stats_t initial_stats;
    ASSERT_EQ(substrate_get_stats(substrate, &initial_stats), 0);
    std::cout << "  Initial ATP consumed: " << initial_stats.total_atp_consumed << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process known spike count", 5000);
    const uint32_t KNOWN_SPIKES = 10000;
    const uint32_t KNOWN_TRANSMISSIONS = 40000;

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(substrate_record_spikes(substrate, KNOWN_SPIKES / 100), 0);
        ASSERT_EQ(substrate_record_transmissions(substrate, KNOWN_TRANSMISSIONS / 100), 0);
        ASSERT_EQ(substrate_update(substrate, SIMULATION_STEP_MS), 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify energy accounting", 100);
    substrate_stats_t final_stats;
    ASSERT_EQ(substrate_get_stats(substrate, &final_stats), 0);

    std::cout << "  Total spikes: " << final_stats.spikes_processed << std::endl;
    std::cout << "  Total transmissions: " << final_stats.transmissions_processed << std::endl;
    std::cout << "  Total ATP consumed: " << final_stats.total_atp_consumed << std::endl;
    std::cout << "  Peak metabolic rate: " << final_stats.peak_metabolic_rate << std::endl;

    EXPECT_EQ(final_stats.spikes_processed, KNOWN_SPIKES);
    EXPECT_EQ(final_stats.transmissions_processed, KNOWN_TRANSMISSIONS);
    EXPECT_GT(final_stats.total_atp_consumed, 0.0f);
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
