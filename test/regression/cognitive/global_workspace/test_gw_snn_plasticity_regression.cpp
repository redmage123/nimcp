//=============================================================================
// test_gw_snn_plasticity_regression.cpp - Global Workspace Regression Tests
//=============================================================================
/**
 * @file test_gw_snn_plasticity_regression.cpp
 * @brief Regression tests for Global Workspace SNN and Plasticity bridges
 *
 * WHAT: Performance and behavior regression tests
 * WHY:  Ensure bridge behavior remains consistent across code changes
 * HOW:  Test known baselines, boundary conditions, and edge cases
 *
 * REGRESSION CATEGORIES:
 * - Output consistency: Same inputs produce same outputs
 * - Performance baselines: Operations complete within time bounds
 * - Edge case handling: Boundary values handled correctly
 * - API contract: Return values and errors match specification
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>

#include "cognitive/global_workspace/nimcp_gw_snn_bridge.h"
#include "cognitive/global_workspace/nimcp_gw_plasticity_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GWSNNPlasticityRegressionTest : public ::testing::Test {
protected:
    gw_snn_bridge_t* snn_bridge;
    gw_plasticity_bridge_t* plasticity_bridge;

    void SetUp() override {
        gw_snn_config_t snn_config = gw_snn_config_default();
        snn_config.enable_bio_async = false;
        snn_bridge = gw_snn_create(&snn_config);
        ASSERT_NE(snn_bridge, nullptr);

        gw_plasticity_config_t plasticity_config = gw_plasticity_config_default();
        plasticity_bridge = gw_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity_bridge, nullptr);
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
};

//=============================================================================
// Output Consistency Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNOutputDeterministic) {
    // Same input should produce consistent output
    float dims[GW_DIM_COUNT];
    memset(dims, 0, sizeof(dims));
    dims[GW_DIM_BROADCAST] = 0.7f;
    dims[GW_DIM_IGNITION] = 0.6f;

    // First run
    gw_snn_reset(snn_bridge);
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(snn_bridge, 20.0f);

    gw_conscious_access_t first_access;
    gw_snn_get_conscious_access(snn_bridge, &first_access);

    // Second run with same inputs
    gw_snn_reset(snn_bridge);
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(snn_bridge, 20.0f);

    gw_conscious_access_t second_access;
    gw_snn_get_conscious_access(snn_bridge, &second_access);

    // Should be deterministic
    EXPECT_NEAR(first_access.broadcast_strength, second_access.broadcast_strength, 0.01f);
    EXPECT_NEAR(first_access.ignition_level, second_access.ignition_level, 0.01f);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityWeightChangeConsistent) {
    // Same STDP pattern should produce consistent weight change
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);

    gw_plasticity_synapse_t before;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &before);

    // Apply STDP
    float delta = gw_plasticity_apply_stdp(plasticity_bridge, 1, 0.0f, 10.0f);

    gw_plasticity_synapse_t after;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &after);

    // Weight change should be consistent
    EXPECT_NEAR(after.weight - before.weight, delta, 0.001f);
    EXPECT_GT(delta, 0.0f);  // Should be potentiation
}

//=============================================================================
// Performance Baseline Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNEncodingPerformance) {
    float dims[GW_DIM_COUNT];
    for (int i = 0; i < GW_DIM_COUNT; i++) {
        dims[i] = 0.5f;
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 encodings in under 100ms
    EXPECT_LT(duration.count(), 100) << "Encoding too slow: " << duration.count() << "ms";
}

TEST_F(GWSNNPlasticityRegressionTest, SNNSimulationPerformance) {
    float dims[GW_DIM_COUNT] = {0.5f};
    gw_snn_encode_state(snn_bridge, dims, 1);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 50; i++) {
        gw_snn_simulate(snn_bridge, 10.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 50 simulations in under 500ms
    EXPECT_LT(duration.count(), 500) << "Simulation too slow: " << duration.count() << "ms";
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityLearningPerformance) {
    // Register synapses
    for (int i = 0; i < 100; i++) {
        gw_plasticity_register_synapse(plasticity_bridge, i + 1, GW_SYNAPSE_COALITION, 0.5f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.8f, (i % 100) + 1, 1.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 learning events in under 50ms
    EXPECT_LT(duration.count(), 50) << "Learning too slow: " << duration.count() << "ms";
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNZeroDimensionHandling) {
    float dims[GW_DIM_COUNT] = {0};
    int spikes = gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0);  // Should succeed with zero values
}

TEST_F(GWSNNPlasticityRegressionTest, SNNMaxDimensionHandling) {
    float dims[GW_DIM_COUNT];
    for (int i = 0; i < GW_DIM_COUNT; i++) {
        dims[i] = 1.0f;  // Max values
    }
    int spikes = gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    EXPECT_GE(spikes, 0);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityWeightBoundsRespected) {
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);

    // Apply many potentiating events
    for (int i = 0; i < 100; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 1.0f, 1, 1.0f);
    }

    gw_plasticity_synapse_t synapse;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse);

    // Weight should not exceed max
    EXPECT_LE(synapse.weight, 2.0f);  // Default max
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityWeightMinBoundsRespected) {
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);

    // Apply many depressing events
    for (int i = 0; i < 100; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_FAILURE, 1.0f, 1, 1.0f);
    }

    gw_plasticity_synapse_t synapse;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse);

    // Weight should not go below min
    EXPECT_GE(synapse.weight, 0.0f);  // Default min
}

//=============================================================================
// API Contract Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNNullHandling) {
    EXPECT_EQ(gw_snn_reset(nullptr), -1);
    EXPECT_EQ(gw_snn_encode_state(nullptr, nullptr, 0), -1);
    EXPECT_EQ(gw_snn_simulate(nullptr, 10.0f), -1);
    EXPECT_EQ(gw_snn_get_conscious_access(nullptr, nullptr), -1);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityNullHandling) {
    EXPECT_EQ(gw_plasticity_reset(nullptr), -1);
    EXPECT_EQ(gw_plasticity_register_synapse(nullptr, 1, GW_SYNAPSE_COALITION, 0.5f), -1);
    EXPECT_EQ(gw_plasticity_learn(nullptr, GW_LEARN_BROADCAST_SUCCESS, 0.5f, 1, 1.0f), -1);
    EXPECT_TRUE(std::isnan(gw_plasticity_apply_stdp(nullptr, 1, 0.0f, 10.0f)));
}

TEST_F(GWSNNPlasticityRegressionTest, SNNInvalidParameterHandling) {
    EXPECT_EQ(gw_snn_simulate(snn_bridge, 0.0f), -1);
    EXPECT_EQ(gw_snn_simulate(snn_bridge, -10.0f), -1);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityInvalidParameterHandling) {
    EXPECT_EQ(gw_plasticity_update_bcm(plasticity_bridge, 0.0f), -1);
    EXPECT_EQ(gw_plasticity_update_bcm(plasticity_bridge, -10.0f), -1);
    EXPECT_EQ(gw_plasticity_update_traces(plasticity_bridge, 0.0f), -1);
    EXPECT_EQ(gw_plasticity_homeostatic_update(plasticity_bridge, -1.0f), -1);
}

//=============================================================================
// Protection Mechanism Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, BroadcastSynapseProtection) {
    // Broadcast synapses must be auto-protected
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_BROADCAST, 0.5f);

    gw_plasticity_synapse_t synapse;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse);
    EXPECT_TRUE(synapse.is_protected);

    float original_weight = synapse.weight;

    // Attempt to modify
    gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 1.0f, 1, 1.0f);

    gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse);
    EXPECT_NEAR(synapse.weight, original_weight, 0.001f);  // Should be unchanged
}

TEST_F(GWSNNPlasticityRegressionTest, IntegrationSynapseProtection) {
    // Integration synapses must be auto-protected
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_INTEGRATION, 0.5f);

    gw_plasticity_synapse_t synapse;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &synapse);
    EXPECT_TRUE(synapse.is_protected);
}

//=============================================================================
// Reset State Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNResetClearsState) {
    // Build up state
    float dims[GW_DIM_COUNT] = {0.9f};
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(snn_bridge, 50.0f);

    gw_snn_stats_t stats_before;
    gw_snn_get_stats(snn_bridge, &stats_before);
    EXPECT_GT(stats_before.total_evaluations, 0u);

    // Reset
    gw_snn_reset(snn_bridge);

    // Stats should be preserved (reset doesn't clear stats)
    gw_snn_stats_t stats_after;
    gw_snn_get_stats(snn_bridge, &stats_after);
    EXPECT_EQ(stats_after.total_evaluations, stats_before.total_evaluations);

    // But state should be reset
    gw_snn_bridge_state_t state;
    gw_snn_get_state(snn_bridge, &state);
    EXPECT_EQ(state.state, GW_SNN_STATE_IDLE);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityResetRestoresWeights) {
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);

    // Modify weight
    gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 1.0f, 1, 1.0f);

    gw_plasticity_synapse_t modified;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &modified);
    EXPECT_NE(modified.weight, 0.5f);

    // Reset
    gw_plasticity_reset(plasticity_bridge);

    gw_plasticity_synapse_t reset;
    gw_plasticity_get_synapse(plasticity_bridge, 1, &reset);
    EXPECT_NEAR(reset.weight, 0.5f, 0.001f);  // Restored to initial
}

//=============================================================================
// Statistics Regression Tests
//=============================================================================

TEST_F(GWSNNPlasticityRegressionTest, SNNStatsAccumulate) {
    float dims[GW_DIM_COUNT] = {0.5f};

    for (int i = 0; i < 10; i++) {
        gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
        gw_snn_simulate(snn_bridge, 10.0f);
    }

    gw_snn_stats_t stats;
    gw_snn_get_stats(snn_bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 10u);
}

TEST_F(GWSNNPlasticityRegressionTest, PlasticityStatsAccumulate) {
    gw_plasticity_register_synapse(plasticity_bridge, 1, GW_SYNAPSE_COALITION, 0.5f);

    for (int i = 0; i < 10; i++) {
        gw_plasticity_learn(plasticity_bridge, GW_LEARN_BROADCAST_SUCCESS, 0.5f, 1, 1.0f);
    }

    gw_plasticity_stats_t stats;
    gw_plasticity_get_stats(plasticity_bridge, &stats);
    EXPECT_EQ(stats.total_learning_events, 10u);
}

TEST_F(GWSNNPlasticityRegressionTest, StatsResetWorks) {
    float dims[GW_DIM_COUNT] = {0.5f};
    gw_snn_encode_state(snn_bridge, dims, GW_DIM_COUNT);
    gw_snn_simulate(snn_bridge, 10.0f);

    gw_snn_reset_stats(snn_bridge);

    gw_snn_stats_t stats;
    gw_snn_get_stats(snn_bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}
