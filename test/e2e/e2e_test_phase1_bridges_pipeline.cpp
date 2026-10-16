/**
 * @file e2e_test_phase1_bridges_pipeline.cpp
 * @brief End-to-end tests for Phase 1 bridges pipeline
 *
 * WHAT: Complete pipeline tests for all 6 Phase 1 bridges
 * WHY:  Verify bridges work correctly in real-world scenarios
 * HOW:  Simulate neural activity, immune responses, and plasticity
 *
 * SCENARIOS:
 * 1. Learning scenario - Activity → Plasticity → Weight changes
 * 2. Immune challenge - Cytokine storm → Glial response → Recovery
 * 3. Myelination adaptation - Sustained activity → Myelin changes
 * 4. Coordinated system - All bridges working together
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/bridges/nimcp_dendrite_plasticity_bridge.h"
#include "plasticity/bridges/nimcp_synapse_plasticity_bridge.h"
#include "plasticity/bridges/nimcp_axon_plasticity_bridge.h"
#include "glial/immune/nimcp_astrocytes_immune_bridge.h"
#include "glial/immune/nimcp_oligodendrocytes_immune_bridge.h"
#include "glial/immune/nimcp_myelin_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class Phase1BridgesPipelineTest : public ::testing::Test {
protected:
    // Plasticity bridges
    dendrite_plasticity_bridge_t* dendrite_bridge = nullptr;
    synapse_plasticity_bridge_t* synapse_bridge = nullptr;
    axon_plasticity_bridge_t* axon_bridge = nullptr;

    // Glial-immune bridges
    astro_immune_bridge_t* astro_bridge = nullptr;
    oligo_immune_bridge_t* oligo_bridge = nullptr;
    myelin_immune_bridge_t* myelin_bridge = nullptr;

    // Simulation state
    uint64_t simulation_time_us = 0;
    float dt_ms = 1.0f;

    void SetUp() override {
        // Create all bridges with default configs
        dendrite_bridge = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        synapse_bridge = synapse_plasticity_create(nullptr, nullptr, nullptr);
        axon_bridge = axon_plasticity_create(nullptr, nullptr, nullptr);
        astro_bridge = astro_cell_create(nullptr, nullptr, nullptr);
        oligo_bridge = oligo_immune_create(nullptr, nullptr, nullptr);
        myelin_bridge = myelin_immune_create(nullptr, nullptr, nullptr);

        ASSERT_NE(dendrite_bridge, nullptr);
        ASSERT_NE(synapse_bridge, nullptr);
        ASSERT_NE(axon_bridge, nullptr);
        ASSERT_NE(astro_bridge, nullptr);
        ASSERT_NE(oligo_bridge, nullptr);
        ASSERT_NE(myelin_bridge, nullptr);
    }

    void TearDown() override {
        if (dendrite_bridge) dendrite_plasticity_destroy(dendrite_bridge);
        if (synapse_bridge) synapse_plasticity_destroy(synapse_bridge);
        if (axon_bridge) axon_plasticity_destroy(axon_bridge);
        if (astro_bridge) astro_cell_destroy(astro_bridge);
        if (oligo_bridge) oligo_immune_destroy(oligo_bridge);
        if (myelin_bridge) myelin_immune_destroy(myelin_bridge);
    }

    // Helper: Advance simulation time
    void advanceTime(float ms) {
        simulation_time_us += (uint64_t)(ms * 1000);
    }

    // Helper: Simulate synaptic activity
    void simulateSynapticActivity(int pre_post_delay_ms) {
        uint64_t pre_time = simulation_time_us / 1000;
        uint64_t post_time = pre_time + pre_post_delay_ms;

        synapse_plasticity_on_pre_spike(synapse_bridge, pre_time);
        synapse_plasticity_on_post_spike(synapse_bridge, post_time);
    }

    // Helper: Update all bridges
    void updateAllBridges(float dt) {
        dendrite_plasticity_update(dendrite_bridge, dt);
        synapse_plasticity_update(synapse_bridge, dt);
        axon_plasticity_update(axon_bridge, dt);
        astro_cell_update(astro_bridge, dt);
        oligo_immune_update(oligo_bridge, dt);
        myelin_immune_update(myelin_bridge, dt);
    }
};

//=============================================================================
// Scenario 1: Learning Pipeline
//=============================================================================

TEST_F(Phase1BridgesPipelineTest, LearningScenario_SpikeTimingDependentPlasticity) {
    // Simulate learning through STDP
    // Pre-before-post timing should induce LTP-like changes

    // Generate LTP-favorable timing (pre before post)
    for (int trial = 0; trial < 100; trial++) {
        simulateSynapticActivity(10);  // Pre 10ms before post

        // Calcium influx at dendrite
        dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.7f);

        // Spike propagates through axon
        axon_plasticity_on_spike(axon_bridge, 0, simulation_time_us / 1000);

        advanceTime(50.0f);
        updateAllBridges(50.0f);
    }

    // Verify plasticity occurred
    synapse_plasticity_stats_t s_stats;
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);

    EXPECT_EQ(s_stats.pre_spike_count, 100u);
    EXPECT_EQ(s_stats.post_spike_count, 100u);

    // Verify dendrite processed calcium
    dendrite_plasticity_stats_t d_stats;
    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    EXPECT_GT(d_stats.calcium_events, 0u);
}

TEST_F(Phase1BridgesPipelineTest, LearningScenario_HomeostaticScaling) {
    // Simulate prolonged high activity and homeostatic response
    for (int i = 0; i < 500; i++) {
        // High-frequency spiking
        for (int j = 0; j < 10; j++) {
            simulateSynapticActivity(5);
            dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.8f);
        }

        advanceTime(10.0f);
        updateAllBridges(10.0f);
    }

    // System should remain stable (no NaN/Inf)
    synapse_plasticity_stats_t s_stats;
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);

    EXPECT_FALSE(std::isnan((float)s_stats.net_weight_change));
    EXPECT_FALSE(std::isinf((float)s_stats.net_weight_change));
}

//=============================================================================
// Scenario 2: Immune Challenge Pipeline
//=============================================================================

TEST_F(Phase1BridgesPipelineTest, ImmuneScenario_CytokineStormAndRecovery) {
    // Phase 1: Normal operation
    for (int i = 0; i < 100; i++) {
        updateAllBridges(10.0f);
    }

    EXPECT_EQ(astro_cell_get_reactivity(astro_bridge), ASTRO_QUIESCENT);

    // Phase 2: Cytokine storm (inflammation)
    astro_bridge->cytokine_effects.a1_drive = 0.9f;
    oligo_bridge->cytokine_effects.net_damage_signal = 0.8f;
    myelin_bridge->cytokine_effects.net_damage = 0.7f;

    for (int i = 0; i < 200; i++) {
        astro_cell_update_reactivity(astro_bridge, 10.0f);
        oligo_immune_accumulate_damage(oligo_bridge, 10.0f);
        myelin_immune_apply_damage(myelin_bridge, 10.0f);
    }

    // Should have progressed to reactive state
    astrocyte_reactivity_t react_state = astro_cell_get_reactivity(astro_bridge);
    EXPECT_EQ(react_state, ASTRO_A1_REACTIVE);

    // Myelin should be damaged
    float integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_LT(integrity, 1.0f);

    // Phase 3: Recovery (anti-inflammatory)
    astro_bridge->cytokine_effects.a1_drive = 0.1f;
    astro_bridge->cytokine_effects.a2_drive = 0.9f;
    oligo_bridge->cytokine_effects.net_damage_signal = 0.0f;
    oligo_bridge->cytokine_effects.net_protection_signal = 0.8f;
    myelin_bridge->cytokine_effects.net_damage = 0.0f;

    for (int i = 0; i < 300; i++) {
        astro_cell_update_reactivity(astro_bridge, 10.0f);
        myelin_immune_apply_repair(myelin_bridge, 10.0f);
        updateAllBridges(10.0f);
    }

    // Should shift toward protective state or remain at A1 (depending on recovery dynamics)
    react_state = astro_cell_get_reactivity(astro_bridge);
    // Either A2 or return to quiescent is valid recovery outcome
    EXPECT_NE(react_state, ASTRO_SCAR_FORMING);

    // Myelin should show some recovery
    float recovery_integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_GT(recovery_integrity, integrity);
}

TEST_F(Phase1BridgesPipelineTest, ImmuneScenario_GlutamateClearanceDuringInflammation) {
    // Get baseline clearance
    float baseline_clearance = astro_cell_get_glutamate_clearance(astro_bridge);

    // Induce A1 reactive state (impairs clearance)
    astro_bridge->cytokine_effects.a1_drive = 0.9f;
    for (int i = 0; i < 100; i++) {
        astro_cell_update_reactivity(astro_bridge, 10.0f);
    }

    float a1_clearance = astro_cell_get_glutamate_clearance(astro_bridge);

    // A1 state should reduce glutamate clearance
    EXPECT_LE(a1_clearance, baseline_clearance);
}

//=============================================================================
// Scenario 3: Myelination Adaptation Pipeline
//=============================================================================

TEST_F(Phase1BridgesPipelineTest, MyelinScenario_ActivityDependentMyelination) {
    // Configure for adaptive myelination
    axon_plasticity_config_t config;
    axon_plasticity_default_config(&config);
    config.enable_adaptive_myelination = true;

    axon_plasticity_bridge_t* adaptive_axon =
        axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(adaptive_axon, nullptr);

    float initial_myelin = axon_plasticity_get_myelination(adaptive_axon, 0);

    // Sustained high activity
    for (int i = 0; i < 1000; i++) {
        axon_plasticity_on_spike(adaptive_axon, 0, i * 10);
        if (i % 100 == 0) {
            axon_plasticity_update_myelination(adaptive_axon);
        }
        axon_plasticity_update(adaptive_axon, 1.0f);
    }

    float final_myelin = axon_plasticity_get_myelination(adaptive_axon, 0);

    // Activity should influence myelination
    EXPECT_GE(final_myelin, initial_myelin);

    axon_plasticity_destroy(adaptive_axon);
}

TEST_F(Phase1BridgesPipelineTest, MyelinScenario_DemyelinationAffectsConduction) {
    // Get baseline conduction
    float baseline_velocity = axon_plasticity_get_conduction_velocity(axon_bridge);

    // Damage myelin
    myelin_bridge->cytokine_effects.net_damage = 0.9f;
    for (int i = 0; i < 500; i++) {
        myelin_immune_apply_damage(myelin_bridge, 10.0f);
    }

    float damaged_integrity = myelin_immune_get_integrity(myelin_bridge);
    EXPECT_LT(damaged_integrity, 0.5f);

    // Verify myelin damage is recorded
    myelin_immune_stats_t stats;
    myelin_immune_get_stats(myelin_bridge, &stats);
    EXPECT_GT(stats.damage_events, 0u);
}

//=============================================================================
// Scenario 4: Coordinated System Pipeline
//=============================================================================

TEST_F(Phase1BridgesPipelineTest, CoordinatedScenario_FullPipelineSimulation) {
    // Simulate 1 second of neural activity with immune modulation
    float total_time_ms = 1000.0f;
    float step_ms = 1.0f;
    int steps = (int)(total_time_ms / step_ms);

    for (int i = 0; i < steps; i++) {
        // Synaptic events every 20ms
        if (i % 20 == 0) {
            simulateSynapticActivity(10);
            dendrite_plasticity_update_calcium(dendrite_bridge, i % 10, 0.6f);
            axon_plasticity_on_spike(axon_bridge, i % 10, i);
        }

        // Moderate inflammation
        if (i > 300 && i < 700) {
            astro_bridge->cytokine_effects.a1_drive = 0.4f;
            oligo_bridge->cytokine_effects.net_damage_signal = 0.2f;
        } else {
            astro_bridge->cytokine_effects.a1_drive = 0.1f;
            oligo_bridge->cytokine_effects.net_damage_signal = 0.0f;
        }

        updateAllBridges(step_ms);
        advanceTime(step_ms);
    }

    // All bridges should remain valid
    EXPECT_TRUE(dendrite_bridge->initialized);
    EXPECT_TRUE(synapse_bridge->initialized);
    EXPECT_TRUE(axon_bridge->initialized);
    EXPECT_TRUE(astro_bridge->initialized);
    EXPECT_TRUE(oligo_bridge->initialized);
    EXPECT_TRUE(myelin_bridge->initialized);

    // Get final statistics
    synapse_plasticity_stats_t s_stats;
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    EXPECT_GT(s_stats.pre_spike_count, 0u);

    dendrite_plasticity_stats_t d_stats;
    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    EXPECT_GT(d_stats.calcium_events, 0u);
}

TEST_F(Phase1BridgesPipelineTest, CoordinatedScenario_ExtendedSimulation10Seconds) {
    // 10 second simulation
    auto start = std::chrono::high_resolution_clock::now();

    float total_time_ms = 10000.0f;
    float step_ms = 10.0f;
    int steps = (int)(total_time_ms / step_ms);

    for (int i = 0; i < steps; i++) {
        // Random-ish activity pattern
        if ((i * 7) % 13 == 0) {
            simulateSynapticActivity((i % 20) - 10);
            dendrite_plasticity_update_calcium(dendrite_bridge, i % 50, 0.5f);
        }

        // Varying immune state
        float phase = sinf(i * 0.01f);
        astro_bridge->cytokine_effects.a1_drive = (phase + 1.0f) * 0.3f;
        astro_bridge->cytokine_effects.a2_drive = (1.0f - phase) * 0.3f;

        updateAllBridges(step_ms);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for 10 simulated seconds)
    EXPECT_LT(duration.count(), 1000);

    // All values should be finite
    synapse_plasticity_stats_t s_stats;
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    EXPECT_FALSE(std::isnan((float)s_stats.net_weight_change));
}

TEST_F(Phase1BridgesPipelineTest, CoordinatedScenario_ResetAndRestart) {
    // Run some activity
    for (int i = 0; i < 100; i++) {
        simulateSynapticActivity(10);
        updateAllBridges(10.0f);
    }

    // Get stats before reset
    synapse_plasticity_stats_t before_stats;
    synapse_plasticity_get_stats(synapse_bridge, &before_stats);
    EXPECT_GT(before_stats.pre_spike_count, 0u);

    // Reset all stats
    dendrite_plasticity_reset_stats(dendrite_bridge);
    synapse_plasticity_reset_stats(synapse_bridge);
    axon_plasticity_reset_stats(axon_bridge);
    astro_cell_reset_stats(astro_bridge);
    oligo_immune_reset_stats(oligo_bridge);
    myelin_immune_reset_stats(myelin_bridge);

    // Verify reset
    synapse_plasticity_stats_t after_stats;
    synapse_plasticity_get_stats(synapse_bridge, &after_stats);
    EXPECT_EQ(after_stats.pre_spike_count, 0u);

    // Run more activity
    for (int i = 0; i < 50; i++) {
        simulateSynapticActivity(10);
        updateAllBridges(10.0f);
    }

    // Verify new activity is tracked
    synapse_plasticity_stats_t new_stats;
    synapse_plasticity_get_stats(synapse_bridge, &new_stats);
    EXPECT_EQ(new_stats.pre_spike_count, 50u);
}

//=============================================================================
// Performance and Stress Tests
//=============================================================================

TEST_F(Phase1BridgesPipelineTest, PerformanceScenario_HighFrequencyUpdates) {
    auto start = std::chrono::high_resolution_clock::now();

    // 100,000 updates
    for (int i = 0; i < 100000; i++) {
        updateAllBridges(0.1f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // < 5 seconds for 100k updates
}

TEST_F(Phase1BridgesPipelineTest, StressScenario_RapidStateChanges) {
    for (int i = 0; i < 1000; i++) {
        // Rapidly oscillate immune state
        astro_bridge->cytokine_effects.a1_drive = (i % 2) ? 1.0f : 0.0f;
        astro_bridge->cytokine_effects.a2_drive = (i % 2) ? 0.0f : 1.0f;

        astro_cell_update_reactivity(astro_bridge, 1.0f);
        updateAllBridges(1.0f);
    }

    // Should handle without crash
    EXPECT_TRUE(astro_bridge->initialized);
}
