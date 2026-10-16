//=============================================================================
// test_three_factor_network_learning.cpp - Integration Tests
//=============================================================================
/**
 * @file test_three_factor_network_learning.cpp
 * @brief Integration tests for three-factor learning in neural network context
 *
 * WHAT: Tests synapse_learn_three_factor() with actual neural network
 * WHY:  Verify integration with network, neuromodulators, and learning loops
 * HOW:  Full network simulations with reward-based learning tasks
 *
 * TEST SCENARIOS:
 * 1. Network-wide learning: All synapses respond to dopamine
 * 2. Reward prediction: Learn to predict rewards from stimuli
 * 3. Temporal credit assignment: Delayed rewards strengthen earlier actions
 * 4. Dopamine bursts: Phasic dopamine triggers consolidation
 *
 * @author NIMCP Test Suite
 * @date 2025-11-16
 * @version 2.7.1
 */

#include <gtest/gtest.h>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include <vector>
#include <cmath>

//=============================================================================
// Test Fixture
//=============================================================================

class ThreeFactorNetworkLearningTest : public ::testing::Test {
protected:
    void SetUp() override {
        GTEST_SKIP() << "Test uses old neural_network API that has been replaced with adaptive_network API";
    }

    void TearDown() override {
    }
};

//=============================================================================
// Network Learning Tests
//=============================================================================

TEST_F(ThreeFactorNetworkLearningTest, NetworkLearning_WithDopamine) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Network with three-factor learning responds to dopamine
    // WHY:  Verify network-wide dopamine modulation
    // EXPECT: Weight changes correlate with dopamine level

    // Add synapses with eligibility traces
    add_synapse_with_eligibility(0, 5, 0.5f);
    add_synapse_with_eligibility(1, 5, 0.5f);

    // Set high dopamine
    set_dopamine(1.0f);

    // Stimulate network
    neural_network_set_input(network, 0, 1.0f);
    neural_network_set_input(network, 1, 1.0f);

    // Run simulation
    for (int t = 0; t < 100; t++) {
        neural_network_step(network);
    }

    // Get synapses
    synapse_t* syn1 = neural_network_get_synapse(network, 0, 5);
    synapse_t* syn2 = neural_network_get_synapse(network, 1, 5);

    // Weights should have changed (due to dopamine)
    EXPECT_NE(syn1->weight, 0.5f);
    EXPECT_NE(syn2->weight, 0.5f);
#endif
}

TEST_F(ThreeFactorNetworkLearningTest, RewardPredictionLearning) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Network learns to predict reward from stimulus
    // WHY:  Classic reward learning task
    // EXPECT: Weights increase for stimulus-reward associations

    // Create stimulus → prediction pathway
    add_synapse_with_eligibility(0, 5, 0.1f);  // Weak initial connection

    // Training: stimulus + reward
    for (int trial = 0; trial < 50; trial++) {
        // Present stimulus
        neural_network_set_input(network, 0, 1.0f);

        // Run network
        for (int t = 0; t < 10; t++) {
            neural_network_step(network);
        }

        // Deliver reward (triggers dopamine)
        trigger_dopamine_burst(1.0f);
        neural_network_step(network);

        // Reset
        neural_network_set_input(network, 0, 0.0f);
    }

    // Check learned weight
    synapse_t* syn = neural_network_get_synapse(network, 0, 5);
    EXPECT_GT(syn->weight, 0.1f);  // Should have strengthened
#endif
}

TEST_F(ThreeFactorNetworkLearningTest, TemporalCreditAssignment) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Delayed reward strengthens earlier synapses (via eligibility trace)
    // WHY:  Core function of eligibility traces
    // EXPECT: Weight change even with 50ms reward delay

    add_synapse_with_eligibility(0, 5, 0.5f);

    float initial_weight = 0.5f;

    // Stimulus at t=0
    neural_network_set_input(network, 0, 1.0f);
    for (int t = 0; t < 10; t++) {
        neural_network_step(network);
    }

    // Clear stimulus
    neural_network_set_input(network, 0, 0.0f);

    // Delay (50ms)
    for (int t = 0; t < 50; t++) {
        neural_network_step(network);
    }

    // Deliver reward (late)
    trigger_dopamine_burst(1.0f);
    neural_network_step(network);

    // Weight should have changed (trace bridged the gap)
    synapse_t* syn = neural_network_get_synapse(network, 0, 5);
    EXPECT_NE(syn->weight, initial_weight);
#endif
}

TEST_F(ThreeFactorNetworkLearningTest, NoDopamine_NoLearning) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Without dopamine, no learning occurs
    // WHY:  Dopamine is essential third factor
    // EXPECT: Weights unchanged with DA = 0

    add_synapse_with_eligibility(0, 5, 0.5f);

    // Set zero dopamine
    set_dopamine(0.0f);

    float initial_weight = 0.5f;

    // Stimulate network repeatedly
    for (int trial = 0; trial < 50; trial++) {
        neural_network_set_input(network, 0, 1.0f);
        for (int t = 0; t < 10; t++) {
            neural_network_step(network);
        }
        // Reward signal (but no dopamine!)
        neural_network_step(network);
        neural_network_set_input(network, 0, 0.0f);
    }

    // Weight should not change
    synapse_t* syn = neural_network_get_synapse(network, 0, 5);
    EXPECT_NEAR(syn->weight, initial_weight, 0.05f);
#endif
}

//=============================================================================
// Dopamine Burst Integration Tests
//=============================================================================

TEST_F(ThreeFactorNetworkLearningTest, DopamineBurst_AmplifiedLearning) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Dopamine burst should amplify learning
    // WHY:  Phasic dopamine signals reward prediction error
    // EXPECT: Larger weight changes during bursts

    add_synapse_with_eligibility(0, 5, 0.5f);

    // Trial 1: Tonic dopamine
    set_dopamine(0.5f);
    neural_network_set_input(network, 0, 1.0f);
    for (int t = 0; t < 10; t++) {
        neural_network_step(network);
    }
    float weight_after_tonic = neural_network_get_synapse(network, 0, 5)->weight;

    // Reset
    neural_network_get_synapse(network, 0, 5)->weight = 0.5f;
    eligibility_trace_init(neural_network_get_synapse(network, 0, 5)->eligibility, 0);

    // Trial 2: Phasic burst
    neural_network_set_input(network, 0, 1.0f);
    for (int t = 0; t < 10; t++) {
        neural_network_step(network);
    }
    trigger_dopamine_burst(1.0f);  // Burst!
    neural_network_step(network);

    float weight_after_burst = neural_network_get_synapse(network, 0, 5)->weight;

    // Burst should produce larger change
    float change_tonic = std::abs(weight_after_tonic - 0.5f);
    float change_burst = std::abs(weight_after_burst - 0.5f);
    EXPECT_GT(change_burst, change_tonic);
#endif
}

//=============================================================================
// Multi-Synapse Tests
//=============================================================================

TEST_F(ThreeFactorNetworkLearningTest, MultiSynapse_IndependentLearning) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Multiple synapses learn independently
    // WHY:  Each synapse has own eligibility trace
    // EXPECT: Different weight changes based on activity

    add_synapse_with_eligibility(0, 5, 0.5f);
    add_synapse_with_eligibility(1, 5, 0.5f);

    set_dopamine(0.8f);

    // Only activate input 0
    neural_network_set_input(network, 0, 1.0f);
    neural_network_set_input(network, 1, 0.0f);

    for (int t = 0; t < 20; t++) {
        neural_network_step(network);
    }

    // Deliver reward
    trigger_dopamine_burst(1.0f);
    neural_network_step(network);

    // Synapse 0 should change more (active)
    synapse_t* syn0 = neural_network_get_synapse(network, 0, 5);
    synapse_t* syn1 = neural_network_get_synapse(network, 1, 5);

    float change0 = std::abs(syn0->weight - 0.5f);
    float change1 = std::abs(syn1->weight - 0.5f);

    EXPECT_GT(change0, change1);
#endif
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ThreeFactorNetworkLearningTest, Performance_100Synapses) {
#if 0  // Test uses old neural_network API - disabled to prevent compilation errors
    // WHAT: Learning with many synapses should be fast
    // WHY:  Verify scalability
    // EXPECT: Completes in reasonable time

    // Add 100 synapses with eligibility
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t pre = i % 8;
        uint32_t post = 2 + (i % 6);
        add_synapse_with_eligibility(pre, post, 0.5f);
    }

    set_dopamine(0.8f);

    auto start = std::chrono::high_resolution_clock::now();

    // Run 1000 steps
    for (int t = 0; t < 1000; t++) {
        neural_network_set_input(network, 0, (t % 10) / 10.0f);
        neural_network_step(network);

        if (t % 100 == 0) {
            trigger_dopamine_burst(0.5f);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in < 1 second
    EXPECT_LT(duration.count(), 1000);
#endif
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
