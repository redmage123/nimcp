/**
 * @file test_curiosity_snn_plasticity_regression.cpp
 * @brief Regression tests for Curiosity SNN-Plasticity bridges
 * @version 1.0.0
 * @date 2026-01-06
 *
 * WHAT: Regression tests ensuring curiosity SNN and plasticity behavior stability
 * WHY:  Prevent regressions in curiosity-driven exploration learning
 * HOW:  Test fixed scenarios with expected outputs, boundary conditions,
 *       and edge cases that have caused issues in the past
 *
 * REGRESSION COVERAGE:
 * - Initialization with various configurations
 * - Encoding edge cases (0, 1, boundary values)
 * - Learning with extreme parameters
 * - Protected synapse guarantees
 * - Statistics accuracy over many iterations
 * - Memory and state leak prevention
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "cognitive/curiosity/nimcp_curiosity_snn_bridge.h"
#include "cognitive/curiosity/nimcp_curiosity_plasticity_bridge.h"

//=============================================================================
// SNN Bridge Regression Tests
//=============================================================================

class CuriositySNNRegressionTest : public ::testing::Test {
protected:
    curiosity_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        curiosity_snn_config_t config = curiosity_snn_config_default();
        config.enable_bio_async = false;
        bridge = curiosity_snn_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_snn_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(CuriositySNNRegressionTest, ZeroInputsDoNotCrash) {
    float dims[CURIOSITY_DIM_COUNT] = {0};
    int spikes = curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    EXPECT_EQ(curiosity_snn_simulate(bridge, 10.0f), 0);

    curiosity_drive_t drive;
    EXPECT_EQ(curiosity_snn_get_drive(bridge, &drive), 0);
    EXPECT_GE(drive.novelty_level, 0.0f);
    EXPECT_LE(drive.novelty_level, 1.0f);
}

TEST_F(CuriositySNNRegressionTest, MaxInputsDoNotCrash) {
    float dims[CURIOSITY_DIM_COUNT];
    for (int i = 0; i < CURIOSITY_DIM_COUNT; i++) {
        dims[i] = 1.0f;
    }
    int spikes = curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    EXPECT_EQ(curiosity_snn_simulate(bridge, 10.0f), 0);

    curiosity_drive_t drive;
    EXPECT_EQ(curiosity_snn_get_drive(bridge, &drive), 0);
    EXPECT_GE(drive.exploration_drive, 0.0f);
    EXPECT_LE(drive.exploration_drive, 1.0f);
}

TEST_F(CuriositySNNRegressionTest, OutOfRangeInputsClamped) {
    float dims[CURIOSITY_DIM_COUNT] = {0};
    dims[0] = 5.0f;   // Above max
    dims[1] = -2.0f;  // Below min

    int spikes = curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
    EXPECT_GE(spikes, 0);

    curiosity_drive_t drive;
    curiosity_snn_simulate(bridge, 10.0f);
    curiosity_snn_get_drive(bridge, &drive);

    // All outputs should still be in valid range
    EXPECT_GE(drive.novelty_level, 0.0f);
    EXPECT_LE(drive.novelty_level, 1.0f);
}

TEST_F(CuriositySNNRegressionTest, RepeatedEncodingStable) {
    float dims[CURIOSITY_DIM_COUNT] = {0.5f};

    std::vector<float> exploration_values;
    for (int i = 0; i < 100; i++) {
        curiosity_snn_encode_state(bridge, dims, 1);
        curiosity_snn_simulate(bridge, 5.0f);

        curiosity_drive_t drive;
        curiosity_snn_get_drive(bridge, &drive);
        exploration_values.push_back(drive.exploration_drive);
    }

    // Check values don't explode or collapse
    for (float val : exploration_values) {
        EXPECT_GE(val, 0.0f);
        EXPECT_LE(val, 1.0f);
    }
}

TEST_F(CuriositySNNRegressionTest, StatsAccurateAfterManyEvaluations) {
    const int NUM_EVALS = 50;

    for (int i = 0; i < NUM_EVALS; i++) {
        float dims[CURIOSITY_DIM_COUNT] = {0};
        dims[i % CURIOSITY_DIM_COUNT] = 0.8f;
        curiosity_snn_encode_state(bridge, dims, CURIOSITY_DIM_COUNT);
        curiosity_snn_simulate(bridge, 10.0f);
    }

    curiosity_snn_stats_t stats;
    curiosity_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, (uint64_t)NUM_EVALS);
}

TEST_F(CuriositySNNRegressionTest, ResetClearsAllState) {
    // Accumulate state
    float dims[CURIOSITY_DIM_COUNT] = {0.9f};
    for (int i = 0; i < 10; i++) {
        curiosity_snn_encode_state(bridge, dims, 1);
        curiosity_snn_simulate(bridge, 10.0f);
    }

    // Reset
    curiosity_snn_reset(bridge);

    // Verify clean state
    curiosity_snn_bridge_state_t state;
    curiosity_snn_get_state(bridge, &state);
    EXPECT_EQ(state.state, CURIOSITY_SNN_STATE_IDLE);
    EXPECT_FLOAT_EQ(state.total_activity, 0.0f);
}

TEST_F(CuriositySNNRegressionTest, ConfigWithEdgeValues) {
    // Test minimum dimensions
    curiosity_snn_config_t config = curiosity_snn_config_default();
    config.num_dimensions = 1;
    config.neurons_per_dim = 4;

    curiosity_snn_bridge_t* min_bridge = curiosity_snn_create(&config);
    ASSERT_NE(min_bridge, nullptr);

    float dims[1] = {0.5f};
    EXPECT_GE(curiosity_snn_encode_state(min_bridge, dims, 1), 0);
    EXPECT_EQ(curiosity_snn_simulate(min_bridge, 10.0f), 0);

    curiosity_snn_destroy(min_bridge);
}

//=============================================================================
// Plasticity Bridge Regression Tests
//=============================================================================

class CuriosityPlasticityRegressionTest : public ::testing::Test {
protected:
    curiosity_plasticity_bridge_t* bridge = nullptr;

    void SetUp() override {
        curiosity_plasticity_config_t config = curiosity_plasticity_config_default();
        config.enable_bio_async = false;
        bridge = curiosity_plasticity_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            curiosity_plasticity_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(CuriosityPlasticityRegressionTest, WeightsStayInBounds) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    // Try extreme learning
    for (int i = 0; i < 1000; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 1.0f, 1, 1.0f);
    }

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GE(syn.weight, 0.0f);
    EXPECT_LE(syn.weight, 1.0f);
}

TEST_F(CuriosityPlasticityRegressionTest, WeightsStayInBoundsNegative) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    // Try extreme negative learning
    for (int i = 0; i < 1000; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_FALSE_NOVELTY, 1.0f, 1, 1.0f);
    }

    curiosity_plasticity_synapse_t syn;
    curiosity_plasticity_get_synapse(bridge, 1, &syn);
    EXPECT_GE(syn.weight, 0.0f);
    EXPECT_LE(syn.weight, 1.0f);
}

TEST_F(CuriosityPlasticityRegressionTest, ProtectedSynapseNeverChanges) {
    // Test EXPLORATION type
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_EXPLORATION, 0.7f);

    curiosity_plasticity_synapse_t initial;
    curiosity_plasticity_get_synapse(bridge, 1, &initial);
    EXPECT_TRUE(initial.is_protected);

    // Attempt all forms of modification
    for (int i = 0; i < 100; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_FALSE_NOVELTY, 1.0f, 1, 1.0f);
        curiosity_plasticity_apply_stdp(bridge, 1, (float)i, (float)i + 10.0f);
        curiosity_plasticity_apply_reward(bridge, 1.0f);
    }

    curiosity_plasticity_synapse_t final_syn;
    curiosity_plasticity_get_synapse(bridge, 1, &final_syn);
    EXPECT_FLOAT_EQ(initial.weight, final_syn.weight);
}

TEST_F(CuriosityPlasticityRegressionTest, ProtectedLearningSynapseNeverChanges) {
    // Test LEARNING type
    curiosity_plasticity_register_synapse(bridge, 2, CURIOSITY_SYNAPSE_LEARNING, 0.8f);

    curiosity_plasticity_synapse_t initial;
    curiosity_plasticity_get_synapse(bridge, 2, &initial);
    EXPECT_TRUE(initial.is_protected);

    // Attempt modification
    for (int i = 0; i < 100; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_EXPLORATION_FAILURE, 1.0f, 2, 0.1f);
        curiosity_plasticity_apply_stdp(bridge, 2, (float)i, (float)i + 10.0f);
    }

    curiosity_plasticity_synapse_t final_syn;
    curiosity_plasticity_get_synapse(bridge, 2, &final_syn);
    EXPECT_FLOAT_EQ(initial.weight, final_syn.weight);
}

TEST_F(CuriosityPlasticityRegressionTest, STDPReturnsCorrectSign) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);

    // Post after pre = potentiation (positive)
    float pot = curiosity_plasticity_apply_stdp(bridge, 1, 10.0f, 20.0f);
    EXPECT_GT(pot, 0.0f);

    // Pre after post = depression (negative)
    float dep = curiosity_plasticity_apply_stdp(bridge, 1, 20.0f, 10.0f);
    EXPECT_LT(dep, 0.0f);
}

TEST_F(CuriosityPlasticityRegressionTest, StatsCountCorrectly) {
    for (int i = 0; i < 20; i++) {
        curiosity_plasticity_register_synapse(bridge, i, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    const int NOVELTY_EVENTS = 30;
    const int FALSE_NOVELTY_EVENTS = 20;
    const int INFO_GAIN_EVENTS = 25;
    const int EXPLORATION_EVENTS = 15;

    for (int i = 0; i < NOVELTY_EVENTS; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.1f, i % 20, 0.5f);
    }
    for (int i = 0; i < FALSE_NOVELTY_EVENTS; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_FALSE_NOVELTY, 0.1f, i % 20, 0.5f);
    }
    for (int i = 0; i < INFO_GAIN_EVENTS; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_INFO_GAIN_HIGH, 0.1f, i % 20, 0.5f);
    }
    for (int i = 0; i < EXPLORATION_EVENTS; i++) {
        curiosity_plasticity_learn(bridge, CURIOSITY_LEARN_EXPLORATION_SUCCESS, 0.1f, i % 20, 0.5f);
    }

    curiosity_plasticity_stats_t stats;
    curiosity_plasticity_get_stats(bridge, &stats);

    EXPECT_EQ(stats.novelty_confirmed_events, (uint64_t)NOVELTY_EVENTS);
    EXPECT_EQ(stats.false_novelty_events, (uint64_t)FALSE_NOVELTY_EVENTS);
    EXPECT_EQ(stats.high_info_gain_events, (uint64_t)INFO_GAIN_EVENTS);
    EXPECT_EQ(stats.exploration_success_events, (uint64_t)EXPLORATION_EVENTS);

    int total_expected = NOVELTY_EVENTS + FALSE_NOVELTY_EVENTS + INFO_GAIN_EVENTS + EXPLORATION_EVENTS;
    EXPECT_EQ(stats.total_learning_events, (uint64_t)total_expected);
}

TEST_F(CuriosityPlasticityRegressionTest, ResetPreservesProtection) {
    curiosity_plasticity_register_synapse(bridge, 1, CURIOSITY_SYNAPSE_EXPLORATION, 0.5f);
    curiosity_plasticity_register_synapse(bridge, 2, CURIOSITY_SYNAPSE_LEARNING, 0.5f);

    curiosity_plasticity_reset(bridge);

    // Protection should still be set
    curiosity_plasticity_synapse_t syn1, syn2;
    curiosity_plasticity_get_synapse(bridge, 1, &syn1);
    curiosity_plasticity_get_synapse(bridge, 2, &syn2);

    EXPECT_TRUE(syn1.is_protected);
    EXPECT_TRUE(syn2.is_protected);
}

TEST_F(CuriosityPlasticityRegressionTest, ManySynapsesDoNotExceedCapacity) {
    // Try to register more than max
    for (uint32_t i = 0; i < CURIOSITY_PLASTICITY_MAX_SYNAPSES + 10; i++) {
        int result = curiosity_plasticity_register_synapse(bridge, i, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
        if (i < CURIOSITY_PLASTICITY_MAX_SYNAPSES) {
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_EQ(result, -1);
        }
    }

    curiosity_plasticity_bridge_state_t state;
    curiosity_plasticity_get_state(bridge, &state);
    EXPECT_LE(state.active_synapses, CURIOSITY_PLASTICITY_MAX_SYNAPSES);
}

TEST_F(CuriosityPlasticityRegressionTest, ConsolidateUpdatesExplorationState) {
    for (int i = 0; i < 10; i++) {
        curiosity_plasticity_register_synapse(bridge, i, CURIOSITY_SYNAPSE_NOVELTY, 0.8f);
    }

    curiosity_exploration_state_t before;
    curiosity_plasticity_get_exploration_state(bridge, &before);

    curiosity_plasticity_consolidate(bridge);

    curiosity_exploration_state_t after;
    curiosity_plasticity_get_exploration_state(bridge, &after);

    // Novelty sensitivity should be updated based on weights
    // With 0.8 weights, sensitivity should be > 1.0
    EXPECT_GT(after.novelty_sensitivity, before.novelty_sensitivity);
}

//=============================================================================
// Combined Regression Tests
//=============================================================================

class CuriosityCombinedRegressionTest : public ::testing::Test {
protected:
    curiosity_snn_bridge_t* snn = nullptr;
    curiosity_plasticity_bridge_t* plasticity = nullptr;

    void SetUp() override {
        curiosity_snn_config_t snn_config = curiosity_snn_config_default();
        snn_config.enable_bio_async = false;
        snn = curiosity_snn_create(&snn_config);
        ASSERT_NE(snn, nullptr);

        curiosity_plasticity_config_t plasticity_config = curiosity_plasticity_config_default();
        plasticity_config.enable_bio_async = false;
        plasticity = curiosity_plasticity_create(&plasticity_config);
        ASSERT_NE(plasticity, nullptr);
    }

    void TearDown() override {
        if (snn) {
            curiosity_snn_destroy(snn);
            snn = nullptr;
        }
        if (plasticity) {
            curiosity_plasticity_destroy(plasticity);
            plasticity = nullptr;
        }
    }
};

TEST_F(CuriosityCombinedRegressionTest, LongRunningStability) {
    for (int i = 0; i < 20; i++) {
        curiosity_plasticity_register_synapse(plasticity, i, CURIOSITY_SYNAPSE_NOVELTY, 0.5f);
    }

    // Run for many iterations
    const int ITERATIONS = 200;
    for (int iter = 0; iter < ITERATIONS; iter++) {
        float dims[CURIOSITY_DIM_COUNT] = {0};
        dims[iter % CURIOSITY_DIM_COUNT] = (float)(iter % 10) / 10.0f;

        curiosity_snn_encode_state(snn, dims, CURIOSITY_DIM_COUNT);
        curiosity_snn_simulate(snn, 5.0f);

        curiosity_drive_t drive;
        curiosity_snn_get_drive(snn, &drive);

        curiosity_plasticity_learn(plasticity,
            CURIOSITY_LEARN_NOVELTY_CONFIRMED, 0.1f, iter % 20, drive.novelty_level);

        if (iter % 10 == 0) {
            curiosity_plasticity_update_bcm(plasticity, 1.0f);
            curiosity_plasticity_homeostatic_update(plasticity, 1.0f);
        }
    }

    // Verify final state is valid
    curiosity_snn_stats_t snn_stats;
    curiosity_snn_get_stats(snn, &snn_stats);
    EXPECT_EQ(snn_stats.total_evaluations, (uint64_t)ITERATIONS);

    curiosity_plasticity_stats_t plasticity_stats;
    curiosity_plasticity_get_stats(plasticity, &plasticity_stats);
    EXPECT_EQ(plasticity_stats.total_learning_events, (uint64_t)ITERATIONS);

    // All weights should still be valid
    for (int i = 0; i < 20; i++) {
        curiosity_plasticity_synapse_t syn;
        curiosity_plasticity_get_synapse(plasticity, i, &syn);
        EXPECT_GE(syn.weight, 0.0f);
        EXPECT_LE(syn.weight, 1.0f);
    }
}

TEST_F(CuriosityCombinedRegressionTest, ResetBothDoesNotLeak) {
    // Allocate and use
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++) {
            float dims[1] = {0.5f};
            curiosity_snn_encode_state(snn, dims, 1);
            curiosity_snn_simulate(snn, 5.0f);
        }

        curiosity_snn_reset(snn);
        curiosity_plasticity_reset(plasticity);
    }

    // Should complete without memory issues
    curiosity_snn_bridge_state_t snn_state;
    curiosity_snn_get_state(snn, &snn_state);
    EXPECT_EQ(snn_state.state, CURIOSITY_SNN_STATE_IDLE);

    curiosity_plasticity_bridge_state_t plasticity_state;
    curiosity_plasticity_get_state(plasticity, &plasticity_state);
    EXPECT_EQ(plasticity_state.state, CURIOSITY_PLASTICITY_STATE_IDLE);
}
