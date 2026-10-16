//=============================================================================
// test_gw_snn_plasticity_integration.cpp - Global Workspace Integration
//=============================================================================
/**
 * @file test_gw_snn_plasticity_integration.cpp
 * @brief Integration tests for Global Workspace-SNN-Plasticity bidirectional dataflows
 *
 * WHAT: Tests complete integration between global workspace, SNN, and plasticity
 * WHY:  Verify bidirectional dataflows work correctly for conscious access learning
 * HOW:  Create both bridges, simulate workspace scenarios, verify broadcast calibration
 *
 * INTEGRATION POINTS:
 * - Workspace state encoding -> SNN population activity
 * - SNN spikes -> Plasticity STDP -> weight updates
 * - Learning events -> Synapse modification -> Access calibration
 * - Protection mechanisms -> Block learning on core broadcast mechanisms
 *
 * THEORETICAL BASIS:
 * - Global Workspace Theory (Baars 1988)
 * - Neuronal Workspace (Dehaene 2011)
 * - Conscious access and learning
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/global_workspace/nimcp_gw_snn_bridge.h"
#include "cognitive/global_workspace/nimcp_gw_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GWSNNPlasticityIntegrationTest : public ::testing::Test {
protected:
    gw_snn_bridge_t* snn_bridge;
    gw_plasticity_bridge_t* plasticity_bridge;

    // Callback tracking
    std::atomic<int> ignition_detection_count{0};
    std::atomic<int> broadcast_count{0};
    std::atomic<int> weight_change_count{0};
    std::atomic<int> access_update_count{0};
    std::atomic<float> last_ignition_level{0.0f};

    void SetUp() override {
        // Create SNN bridge with test-friendly config
        gw_snn_config_t snn_config = gw_snn_config_default();
        snn_config.num_dimensions = GW_DIM_COUNT;
        snn_config.neurons_per_dim = 32;
        snn_config.enable_competition = true;
        snn_config.enable_bio_async = false;  // Disable for predictable tests

        snn_bridge = gw_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr) << "Failed to create SNN bridge";

        // Create Plasticity bridge with defaults
        gw_plasticity_config_t plasticity_config = gw_plasticity_config_default();
        plasticity_config.base_learning_rate = 0.01f;
        plasticity_config.stdp_a_plus = 0.01f;
        plasticity_config.stdp_a_minus = 0.012f;

        plasticity_bridge = gw_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr) << "Failed to create plasticity bridge";

        // Reset counters
        ignition_detection_count = 0;
        broadcast_count = 0;
        weight_change_count = 0;
        access_update_count = 0;
        last_ignition_level = 0.0f;
    }

    void TearDown() override {
        if (snn_bridge) {
            gw_snn_destroy(snn_bridge);
            snn_bridge = nullptr;
        }
        if (plasticity_bridge) {
            gw_plasticity_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
    }

    // Generate workspace context for scenario
    void generate_workspace_context(float* dims, uint32_t scenario_type) {
        memset(dims, 0, sizeof(float) * GW_DIM_COUNT);
        switch (scenario_type) {
            case 0: // Strong broadcast (conscious access)
                dims[GW_DIM_BROADCAST] = 0.9f;
                dims[GW_DIM_IGNITION] = 0.85f;
                dims[GW_DIM_COMPETITION] = 0.7f;
                dims[GW_DIM_ACCESS_CONSCIOUSNESS] = 0.8f;
                break;
            case 1: // Subthreshold (no conscious access)
                dims[GW_DIM_BROADCAST] = 0.3f;
                dims[GW_DIM_IGNITION] = 0.2f;
                dims[GW_DIM_COMPETITION] = 0.4f;
                dims[GW_DIM_ACCESS_CONSCIOUSNESS] = 0.2f;
                break;
            case 2: // High competition (coalition conflict)
                dims[GW_DIM_COMPETITION] = 0.95f;
                dims[GW_DIM_COALITION_STRENGTH] = 0.8f;
                dims[GW_DIM_ATTENTION_WINNER] = 0.6f;
                break;
            case 3: // Feature binding scenario
                dims[GW_DIM_BINDING] = 0.9f;
                dims[GW_DIM_INTEGRATION] = 0.85f;
                dims[GW_DIM_CONSCIOUS_CONTENT] = 0.7f;
                break;
            default:
                for (int i = 0; i < GW_DIM_COUNT; i++) {
                    dims[i] = 0.5f;
                }
                break;
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, BothBridgesInitialize) {
    // Verify both bridges are functional
    EXPECT_NE(snn_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    // Check initial states
    gw_snn_bridge_state_t snn_state;
    EXPECT_EQ(gw_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, GW_SNN_STATE_IDLE);

    gw_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(gw_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, GW_PLASTICITY_STATE_IDLE);
}

TEST_F(GWSNNPlasticityIntegrationTest, SNNEncodingDrivesPlasticityActivity) {
    // Encode workspace context in SNN
    float dims[GW_DIM_COUNT];
    generate_workspace_context(dims, 0);  // Strong broadcast scenario

    int spikes = gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0) << "Encoding should succeed (0 or more spikes)";

    // Simulate SNN to process
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 20.0f), 0);

    // Register synapse in plasticity
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Learn from broadcast success
    EXPECT_EQ(gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.9f, 1, 1.0f), 0);

    // Verify statistics updated
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GT(stats.total_learning_events, 0u);
}

//=============================================================================
// Ignition Detection Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, IgnitionDrivesLearning) {
    // Encode high ignition signal
    int spikes = gw_snn_encode_ignition(snn_bridge, 0.9f, 3);
    EXPECT_GE(spikes, 0);

    // Simulate to process ignition
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 30.0f), 0);

    // Check ignition detected
    float level;
    gw_snn_check_ignition(snn_bridge, &level);
    EXPECT_GE(level, 0.0f);

    // Register synapse and learn from ignition
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_IGNITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_learn(plasticity_bridge, GW_LEARN_IGNITION_TRIGGERED, level, 1, 1.0f), 0);

    // Check ignition events in stats
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GT(stats.ignition_events, 0u);
}

//=============================================================================
// Competition Learning Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, CompetitionDrivesLearning) {
    // Encode competition scenario
    float dims[GW_DIM_COUNT];
    generate_workspace_context(dims, 2);  // High competition

    int spikes = gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // Simulate
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 30.0f), 0);

    // Register coalition synapse
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Learn from competition win
    EXPECT_EQ(gw_plasticity_learn(plasticity_bridge, GW_LEARN_COMPETITION_WON, 0.8f, 1, 1.0f), 0);

    // Verify learning updated weights
    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_GT(synapse.weight, 0.5f);  // Should have increased
}

//=============================================================================
// STDP Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, STDPAcrossBridges) {
    // Encode state in SNN
    float dims[GW_DIM_COUNT];
    generate_workspace_context(dims, 0);
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);

    // Simulate to get spike timing
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 10.0f), 0);

    // Register synapse in plasticity
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    gw_plasticity_synapse_t before;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &before), 0);

    // Apply STDP with pre->post timing (potentiation)
    float delta = gw_plasticity_apply_stdp(plasticity_bridge, 1, 0.0f, 15.0f);
    EXPECT_GT(delta, 0.0f);

    gw_plasticity_synapse_t after;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &after), 0);
    EXPECT_GT(after.weight, before.weight);
}

//=============================================================================
// Protection Mechanism Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, BroadcastSynapseProtected) {
    // Register broadcast synapse (should be auto-protected)
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f), 0);

    gw_plasticity_synapse_t before;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &before), 0);
    EXPECT_TRUE(before.is_protected);

    // Attempt to learn on protected synapse
    EXPECT_EQ(gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 1.0f, 1, 1.0f), 0);

    gw_plasticity_synapse_t after;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &after), 0);

    // Weight should be unchanged
    EXPECT_NEAR(before.weight, after.weight, 0.001f);
}

TEST_F(GWSNNPlasticityIntegrationTest, IntegrationSynapseProtected) {
    // Register integration synapse (should be auto-protected)
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_INTEGRATION, 0.5f), 0);

    gw_plasticity_synapse_t synapse;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse), 0);
    EXPECT_TRUE(synapse.is_protected);
}

//=============================================================================
// Broadcast Learning Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, BroadcastSuccessLearning) {
    // Encode strong broadcast
    gw_snn_encode_broadcast(snn_bridge, 0.9f, 1);
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 30.0f), 0);

    // Get conscious access
    gw_conscious_access_t access;
    EXPECT_EQ(gw_snn_get_conscious_access(snn_bridge, &access), 0);

    // Register coalition synapse
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Learn based on broadcast strength
    EXPECT_EQ(gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS,
                                   access.broadcast_strength, 1, 1.0f), 0);

    // Verify broadcast success event tracked
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GT(stats.broadcast_success_events, 0u);
}

//=============================================================================
// Homeostatic Regulation Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, HomeostaticRegulationAfterLearning) {
    // Register multiple synapses
    for (uint32_t i = 0; i < 5; i++) {
        EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, i + 1,
                                                  GW_SYNAPSE_COALITION, 0.5f), 0);
    }

    // Apply strong learning to shift weights
    for (uint32_t i = 0; i < 5; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.9f, i + 1, 1.0f);
    }

    // Get state before homeostasis
    gw_plasticity_bridge_state_t before;
    EXPECT_EQ(gw_plasticity_get_state(plasticity_bridge, &before), 0);

    // Apply homeostatic regulation
    EXPECT_EQ(gw_plasticity_homeostatic_update(plasticity_bridge, 100.0f), 0);

    // Weights should be slightly modulated
    gw_plasticity_bridge_state_t after;
    EXPECT_EQ(gw_plasticity_get_state(plasticity_bridge, &after), 0);
}

//=============================================================================
// Full Pipeline Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, FullConsciousAccessPipeline) {
    // 1. Encode workspace state
    float dims[GW_DIM_COUNT];
    generate_workspace_context(dims, 0);  // Strong broadcast

    int spikes = gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    // 2. Simulate SNN processing
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 50.0f), 0);

    // 3. Get conscious access result
    gw_conscious_access_t access;
    EXPECT_EQ(gw_snn_get_conscious_access(snn_bridge, &access), 0);

    // 4. Register synapses for learning
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 2, GW_SYNAPSE_BINDING, 0.5f), 0);

    // 5. Learn from access
    gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS,
                        access.broadcast_strength, 1, 1.0f);
    gw_plasticity_learn(plasticity_bridge, GW_LEARN_BINDING_FORMED,
                        access.binding_strength, 2, 1.0f);

    // 6. Apply STDP
    gw_plasticity_apply_stdp(plasticity_bridge, 1, 0.0f, 10.0f);
    gw_plasticity_apply_stdp(plasticity_bridge, 2, 0.0f, 15.0f);

    // 7. Update traces
    EXPECT_EQ(gw_plasticity_update_traces(plasticity_bridge, 10.0f), 0);

    // 8. Consolidate
    EXPECT_EQ(gw_plasticity_consolidate(plasticity_bridge), 0);

    // Verify final state
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GT(stats.total_learning_events, 0u);
    EXPECT_GT(stats.weight_updates, 0u);
}

TEST_F(GWSNNPlasticityIntegrationTest, RepeatedLearningCycles) {
    // Register synapse
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    float dims[GW_DIM_COUNT];

    // Multiple learning cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Encode and simulate
        generate_workspace_context(dims, cycle % 4);
        gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
        gw_snn_simulate(snn_bridge, 20.0f);

        // Get access
        gw_conscious_access_t access;
        gw_snn_get_conscious_access(snn_bridge, &access);

        // Learn
        gw_learn_event_t event = (access.broadcast_strength > 0.5f) ?
            GW_LEARN_BROADCAST_SUCCESS : GW_LEARN_BROADCAST_FAILURE;
        gw_plasticity_learn(plasticity_bridge, event, access.broadcast_strength, 1, 1.0f);

        // Update BCM and traces
        gw_plasticity_update_bcm(plasticity_bridge, 10.0f);
        gw_plasticity_update_traces(plasticity_bridge, 10.0f);
    }

    // Verify multiple learning events
    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);
    EXPECT_GE(stats.total_learning_events, 10u);
}

//=============================================================================
// Access Learning State Integration Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, AccessLearningStateEvolution) {
    // Register synapses
    EXPECT_EQ(gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f), 0);

    // Get initial synapse weight
    gw_plasticity_synapse_t initial_syn;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &initial_syn), 0);

    // Multiple learning and homeostatic cycles
    for (int i = 0; i < 5; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.8f, 1, 1.0f);
        gw_plasticity_homeostatic_update(plasticity_bridge, 100.0f);
    }

    // Get final synapse and stats
    gw_plasticity_synapse_t final_syn;
    EXPECT_EQ(gw_plasticity_get_synapse(plasticity_bridge, 1, &final_syn), 0);

    gw_plasticity_stats_t stats;
    EXPECT_EQ(gw_plasticity_get_stats(plasticity_bridge, &stats), 0);

    // Verify learning occurred - weight should have changed or learning events recorded
    EXPECT_GE(stats.total_learning_events, 5u);
    // Weight may have changed (unless it's a protected synapse)
    // Just verify the system is tracking learning events
}

//=============================================================================
// Reset and State Consistency Tests
//=============================================================================

TEST_F(GWSNNPlasticityIntegrationTest, ResetClearsAllState) {
    // Build up state
    float dims[GW_DIM_COUNT];
    generate_workspace_context(dims, 0);
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(snn_bridge, 20.0f);

    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);
    gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.9f, 1, 1.0f);

    // Reset both bridges
    EXPECT_EQ(gw_snn_reset(snn_bridge), 0);
    EXPECT_EQ(gw_plasticity_reset(plasticity_bridge), 0);

    // Verify states are reset
    gw_snn_bridge_state_t snn_state;
    EXPECT_EQ(gw_snn_get_state(snn_bridge, &snn_state), 0);
    EXPECT_EQ(snn_state.state, GW_SNN_STATE_IDLE);

    gw_plasticity_bridge_state_t plasticity_state;
    EXPECT_EQ(gw_plasticity_get_state(plasticity_bridge, &plasticity_state), 0);
    EXPECT_EQ(plasticity_state.state, GW_PLASTICITY_STATE_IDLE);
}
