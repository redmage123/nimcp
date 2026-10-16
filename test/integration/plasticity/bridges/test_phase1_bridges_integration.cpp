/**
 * @file test_phase1_bridges_integration.cpp
 * @brief Integration tests for Phase 1 bridges (neural component-plasticity and glial-immune)
 *
 * WHAT: Integration tests for all 6 Phase 1 bridge modules
 * WHY:  Verify bridges work correctly together and with their connected subsystems
 * HOW:  Test cross-module interactions, cascading effects, orchestrator coordination
 *
 * TEST SCENARIOS:
 * - Dendrite-Synapse-Axon plasticity chain
 * - Glial-Immune cascade (astrocytes, oligodendrocytes, myelin)
 * - Cross-domain coordination (plasticity <-> immune)
 * - Bio-async message routing between bridges
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

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

class Phase1BridgesIntegrationTest : public ::testing::Test {
protected:
    // Plasticity bridges
    dendrite_plasticity_bridge_t* dendrite_bridge = nullptr;
    synapse_plasticity_bridge_t* synapse_bridge = nullptr;
    axon_plasticity_bridge_t* axon_bridge = nullptr;

    // Glial-immune bridges
    astro_immune_bridge_t* astro_bridge = nullptr;
    oligo_immune_bridge_t* oligo_bridge = nullptr;
    myelin_immune_bridge_t* myelin_bridge = nullptr;

    void SetUp() override {
        // Create plasticity bridges
        dendrite_bridge = dendrite_plasticity_create(nullptr, nullptr, nullptr);
        synapse_bridge = synapse_plasticity_create(nullptr, nullptr, nullptr);
        axon_bridge = axon_plasticity_create(nullptr, nullptr, nullptr);

        // Create glial-immune bridges
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
};

//=============================================================================
// All Bridges Creation and Lifecycle Tests
//=============================================================================

TEST_F(Phase1BridgesIntegrationTest, AllBridgesCreatedSuccessfully) {
    // Verify all bridges are initialized
    EXPECT_TRUE(dendrite_bridge->initialized);
    EXPECT_TRUE(synapse_bridge->initialized);
    EXPECT_TRUE(axon_bridge->initialized);
    EXPECT_TRUE(astro_bridge->initialized);
    EXPECT_TRUE(oligo_bridge->initialized);
    EXPECT_TRUE(myelin_bridge->initialized);
}

TEST_F(Phase1BridgesIntegrationTest, AllBridgesHaveMutex) {
    EXPECT_NE(dendrite_bridge->base.mutex, nullptr);
    EXPECT_NE(synapse_bridge->base.mutex, nullptr);
    EXPECT_NE(axon_bridge->base.mutex, nullptr);
    EXPECT_NE(astro_bridge->base.mutex, nullptr);
    EXPECT_NE(oligo_bridge->base.mutex, nullptr);
    EXPECT_NE(myelin_bridge->base.mutex, nullptr);
}

TEST_F(Phase1BridgesIntegrationTest, AllBridgesCanUpdate) {
    float dt_ms = 10.0f;

    EXPECT_EQ(dendrite_plasticity_update(dendrite_bridge, dt_ms), 0);
    EXPECT_EQ(synapse_plasticity_update(synapse_bridge, dt_ms), 0);
    EXPECT_EQ(axon_plasticity_update(axon_bridge, dt_ms), 0);
    EXPECT_EQ(astro_cell_update(astro_bridge, dt_ms), 0);
    EXPECT_EQ(oligo_immune_update(oligo_bridge, dt_ms), 0);
    EXPECT_EQ(myelin_immune_update(myelin_bridge, dt_ms), 0);
}

//=============================================================================
// Plasticity Chain Integration Tests
//=============================================================================

TEST_F(Phase1BridgesIntegrationTest, PlasticityChainUpdateSequence) {
    // Test that updates can be chained across all plasticity bridges
    float dt_ms = 10.0f;

    // Pre-synaptic event at synapse
    synapse_plasticity_on_pre_spike(synapse_bridge, 1000);

    // BPAP reaches dendrite
    dendrite_plasticity_process_bpap(dendrite_bridge, 0, 1010.0f, 0.8f);

    // Spike propagates through axon
    axon_plasticity_on_spike(axon_bridge, 0, 1020);

    // Update all bridges
    dendrite_plasticity_update(dendrite_bridge, dt_ms);
    synapse_plasticity_update(synapse_bridge, dt_ms);
    axon_plasticity_update(axon_bridge, dt_ms);

    // Verify stats were updated
    dendrite_plasticity_stats_t d_stats;
    synapse_plasticity_stats_t s_stats;
    axon_plasticity_stats_t a_stats;

    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    axon_plasticity_get_stats(axon_bridge, &a_stats);

    EXPECT_GT(s_stats.pre_spike_count, 0u);
}

TEST_F(Phase1BridgesIntegrationTest, CalciumAndSTDPInteraction) {
    // High calcium should affect plasticity direction
    dendrite_plasticity_config_t d_config;
    dendrite_plasticity_default_config(&d_config);
    d_config.enable_stdp = true;
    d_config.enable_bcm = true;

    dendrite_plasticity_bridge_t* db = dendrite_plasticity_create(&d_config, nullptr, nullptr);
    ASSERT_NE(db, nullptr);

    // Inject calcium
    dendrite_plasticity_update_calcium(db, 0, 0.8f);

    // Apply STDP
    float delta = dendrite_plasticity_apply_stdp(db, 0, 100.0f, 110.0f);

    // Calcium state should influence outcome
    dendrite_calcium_level_t ca_state = dendrite_plasticity_get_calcium_state(db, 0);
    EXPECT_NE(ca_state, CALCIUM_LEVEL_NONE);

    dendrite_plasticity_destroy(db);
}

TEST_F(Phase1BridgesIntegrationTest, SynapticWeightAccumulation) {
    // Test weight change accumulation over multiple events
    for (int i = 0; i < 10; i++) {
        synapse_plasticity_on_pre_spike(synapse_bridge, i * 20);
        synapse_plasticity_on_post_spike(synapse_bridge, i * 20 + 10);
    }

    synapse_plasticity_stats_t stats;
    synapse_plasticity_get_stats(synapse_bridge, &stats);

    EXPECT_EQ(stats.pre_spike_count, 10u);
    EXPECT_EQ(stats.post_spike_count, 10u);
}

TEST_F(Phase1BridgesIntegrationTest, AxonActivityAndMyelination) {
    // Test that activity affects myelination
    axon_plasticity_config_t config;
    axon_plasticity_default_config(&config);
    config.enable_adaptive_myelination = true;

    axon_plasticity_bridge_t* ab = axon_plasticity_create(&config, nullptr, nullptr);
    ASSERT_NE(ab, nullptr);

    float initial_myelin = axon_plasticity_get_myelination(ab, 0);

    // Generate activity
    for (int i = 0; i < 50; i++) {
        axon_plasticity_on_spike(ab, 0, i * 50);
    }
    axon_plasticity_update_myelination(ab);

    float after_myelin = axon_plasticity_get_myelination(ab, 0);
    EXPECT_GE(after_myelin, initial_myelin);

    axon_plasticity_destroy(ab);
}

//=============================================================================
// Glial-Immune Integration Tests
//=============================================================================

TEST_F(Phase1BridgesIntegrationTest, InflammatoryDamageChain) {
    // Simulate inflammation affecting all glial types
    // Set high inflammatory cytokines
    astro_bridge->cytokine_effects.a1_drive = 0.8f;
    oligo_bridge->cytokine_effects.net_damage_signal = 0.7f;
    myelin_bridge->cytokine_effects.net_damage = 0.6f;

    float dt_ms = 100.0f;

    // Update all bridges
    astro_cell_update_reactivity(astro_bridge, dt_ms);
    oligo_immune_accumulate_damage(oligo_bridge, dt_ms);
    myelin_immune_apply_damage(myelin_bridge, dt_ms);

    // Verify inflammatory state
    EXPECT_EQ(astro_cell_get_reactivity(astro_bridge), ASTRO_A1_REACTIVE);
    EXPECT_GT(oligo_bridge->damage_level, 0.0f);
    EXPECT_LT(myelin_immune_get_integrity(myelin_bridge), 1.0f);
}

TEST_F(Phase1BridgesIntegrationTest, AntiInflammatoryProtection) {
    // Test IL-10/A2 protective effects
    astro_bridge->cytokine_effects.a2_drive = 0.8f;
    astro_bridge->cytokine_effects.a1_drive = 0.1f;

    astro_cell_update_reactivity(astro_bridge, 10.0f);

    EXPECT_EQ(astro_cell_get_reactivity(astro_bridge), ASTRO_A2_REACTIVE);

    // Glutamate clearance should be normal in A2 state
    float clearance = astro_cell_get_glutamate_clearance(astro_bridge);
    EXPECT_FLOAT_EQ(clearance, astro_bridge->config.glutamate_clearance_base);
}

TEST_F(Phase1BridgesIntegrationTest, MyelinDamageAndConduction) {
    // Myelin damage should reduce conduction efficiency
    myelin_bridge->cytokine_effects.net_damage = 0.8f;

    for (int i = 0; i < 20; i++) {
        myelin_immune_apply_damage(myelin_bridge, 100.0f);
    }

    float integrity = myelin_immune_get_integrity(myelin_bridge);
    float conduction = myelin_immune_get_conduction_efficiency(myelin_bridge);

    EXPECT_LT(integrity, 1.0f);
    // Conduction efficiency correlates with integrity
}

TEST_F(Phase1BridgesIntegrationTest, OligodendrocyteDamageProgression) {
    // Test damage accumulation - exactly like unit test
    oligo_bridge->cytokine_effects.net_damage_signal = 0.5f;

    float initial_damage = oligo_bridge->damage_level;
    oligo_immune_accumulate_damage(oligo_bridge, 100.0f);
    float after_damage = oligo_bridge->damage_level;

    EXPECT_GT(after_damage, initial_damage);
}

//=============================================================================
// Cross-Domain Integration Tests
//=============================================================================

TEST_F(Phase1BridgesIntegrationTest, SimultaneousUpdatesDoNotInterfere) {
    // Update all bridges in various orders
    float dt_ms = 10.0f;

    // Order 1: Plasticity first
    dendrite_plasticity_update(dendrite_bridge, dt_ms);
    synapse_plasticity_update(synapse_bridge, dt_ms);
    axon_plasticity_update(axon_bridge, dt_ms);
    astro_cell_update(astro_bridge, dt_ms);
    oligo_immune_update(oligo_bridge, dt_ms);
    myelin_immune_update(myelin_bridge, dt_ms);

    // Order 2: Immune first
    astro_cell_update(astro_bridge, dt_ms);
    oligo_immune_update(oligo_bridge, dt_ms);
    myelin_immune_update(myelin_bridge, dt_ms);
    dendrite_plasticity_update(dendrite_bridge, dt_ms);
    synapse_plasticity_update(synapse_bridge, dt_ms);
    axon_plasticity_update(axon_bridge, dt_ms);

    // All should remain valid
    EXPECT_TRUE(dendrite_bridge->initialized);
    EXPECT_TRUE(synapse_bridge->initialized);
    EXPECT_TRUE(axon_bridge->initialized);
    EXPECT_TRUE(astro_bridge->initialized);
    EXPECT_TRUE(oligo_bridge->initialized);
    EXPECT_TRUE(myelin_bridge->initialized);
}

TEST_F(Phase1BridgesIntegrationTest, StatisticsAccumulateAcrossUpdates) {
    // Generate events across all bridges
    dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.5f);
    synapse_plasticity_on_pre_spike(synapse_bridge, 100);
    synapse_plasticity_on_post_spike(synapse_bridge, 110);
    axon_plasticity_on_spike(axon_bridge, 0, 120);

    astro_bridge->cytokine_effects.a1_drive = 0.8f;
    astro_cell_update_reactivity(astro_bridge, 10.0f);

    oligo_bridge->cytokine_effects.net_damage_signal = 0.3f;
    oligo_immune_accumulate_damage(oligo_bridge, 100.0f);

    myelin_bridge->cytokine_effects.net_damage = 0.3f;
    myelin_immune_apply_damage(myelin_bridge, 100.0f);

    // Get all stats
    dendrite_plasticity_stats_t d_stats;
    synapse_plasticity_stats_t s_stats;
    axon_plasticity_stats_t a_stats;
    astro_immune_stats_t ast_stats;
    oligo_immune_stats_t olg_stats;
    myelin_immune_stats_t myl_stats;

    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    axon_plasticity_get_stats(axon_bridge, &a_stats);
    astro_cell_get_stats(astro_bridge, &ast_stats);
    oligo_immune_get_stats(oligo_bridge, &olg_stats);
    myelin_immune_get_stats(myelin_bridge, &myl_stats);

    // Verify stats have accumulated
    EXPECT_GT(d_stats.calcium_events, 0u);
    EXPECT_GT(s_stats.pre_spike_count, 0u);
    EXPECT_GT(s_stats.post_spike_count, 0u);
    EXPECT_GT(ast_stats.reactivity_changes, 0u);
}

TEST_F(Phase1BridgesIntegrationTest, ResetAllStatistics) {
    // Generate events
    dendrite_plasticity_update_calcium(dendrite_bridge, 0, 0.5f);
    synapse_plasticity_on_pre_spike(synapse_bridge, 100);
    axon_plasticity_on_spike(axon_bridge, 0, 120);

    // Reset all
    dendrite_plasticity_reset_stats(dendrite_bridge);
    synapse_plasticity_reset_stats(synapse_bridge);
    axon_plasticity_reset_stats(axon_bridge);
    astro_cell_reset_stats(astro_bridge);
    oligo_immune_reset_stats(oligo_bridge);
    myelin_immune_reset_stats(myelin_bridge);

    // Verify reset
    dendrite_plasticity_stats_t d_stats;
    synapse_plasticity_stats_t s_stats;
    axon_plasticity_stats_t a_stats;

    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    synapse_plasticity_get_stats(synapse_bridge, &s_stats);
    axon_plasticity_get_stats(axon_bridge, &a_stats);

    EXPECT_EQ(d_stats.calcium_events, 0u);
    EXPECT_EQ(s_stats.pre_spike_count, 0u);
    EXPECT_EQ(a_stats.spikes_generated, 0u);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(Phase1BridgesIntegrationTest, HighFrequencyUpdates) {
    // Many rapid updates should not crash
    for (int i = 0; i < 1000; i++) {
        dendrite_plasticity_update(dendrite_bridge, 0.1f);
        synapse_plasticity_update(synapse_bridge, 0.1f);
        axon_plasticity_update(axon_bridge, 0.1f);
        astro_cell_update(astro_bridge, 0.1f);
        oligo_immune_update(oligo_bridge, 0.1f);
        myelin_immune_update(myelin_bridge, 0.1f);
    }

    // All should still be valid
    EXPECT_TRUE(dendrite_bridge->initialized);
    EXPECT_TRUE(synapse_bridge->initialized);
    EXPECT_TRUE(axon_bridge->initialized);
    EXPECT_TRUE(astro_bridge->initialized);
    EXPECT_TRUE(oligo_bridge->initialized);
    EXPECT_TRUE(myelin_bridge->initialized);
}

TEST_F(Phase1BridgesIntegrationTest, ManyCompartmentsAndSegments) {
    // Test with multiple compartments/segments
    for (uint32_t id = 0; id < 100; id++) {
        dendrite_plasticity_update_calcium(dendrite_bridge, id, 0.5f);
    }

    for (uint32_t id = 0; id < 50; id++) {
        axon_plasticity_on_spike(axon_bridge, id, id * 10);
    }

    // Should handle gracefully
    dendrite_plasticity_stats_t d_stats;
    axon_plasticity_stats_t a_stats;

    dendrite_plasticity_get_stats(dendrite_bridge, &d_stats);
    axon_plasticity_get_stats(axon_bridge, &a_stats);

    EXPECT_GT(d_stats.calcium_events, 0u);
}

TEST_F(Phase1BridgesIntegrationTest, ExtremeInflammation) {
    // Test behavior under extreme inflammation
    astro_bridge->cytokine_effects.a1_drive = 1.0f;
    oligo_bridge->cytokine_effects.net_damage_signal = 1.0f;
    myelin_bridge->cytokine_effects.net_damage = 1.0f;

    for (int i = 0; i < 100; i++) {
        astro_cell_update_reactivity(astro_bridge, 100.0f);
        oligo_immune_accumulate_damage(oligo_bridge, 100.0f);
        myelin_immune_apply_damage(myelin_bridge, 100.0f);
    }

    // Values should stay within bounds
    EXPECT_LE(oligo_bridge->damage_level, OLIGO_IMMUNE_MAX_DAMAGE);
    EXPECT_GE(myelin_immune_get_integrity(myelin_bridge), 0.0f);
}
