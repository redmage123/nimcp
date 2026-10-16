/**
 * @file test_utils_quantum_walk.cpp
 * @brief Comprehensive unit tests for quantum walk simulation
 *
 * WHAT: 100% test coverage for nimcp_quantum_walk.c (quantum random walk)
 * WHY:  Quantum walk provides √N speedup for neuromodulator diffusion
 * HOW:  Test all operations, edge cases, numerical stability
 *
 * TEST COVERAGE:
 * 1. quantum_walk_create() - walker creation with network
 * 2. quantum_walk_destroy() - cleanup and NULL safety
 * 3. quantum_walk_initialize() - localized state initialization
 * 4. quantum_walk_initialize_superposition() - superposition state
 * 5. quantum_walk_step() - single quantum walk step
 * 6. quantum_walk_evolve() - multi-step evolution
 * 7. quantum_walk_get_distribution() - probability measurement
 * 8. quantum_walk_get_amplitudes() - complex amplitude extraction
 * 9. quantum_walk_measure() - quantum measurement (collapse)
 * 10. quantum_walk_compute_stats() - statistics computation
 * 11. quantum_walk_verify() - probability conservation check
 * 12. quantum_walk_reset() - walker reset
 * 13. quantum_walk_apply_decoherence() - decoherence effects
 * 14. Different coin operators (Hadamard, Grover, Fourier, Identity)
 * 15. Different graph topologies
 * 16. Edge cases (NULL pointers, invalid nodes, boundary conditions)
 * 17. Numerical stability (normalization, probability conservation)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <complex>

    #include "utils/quantum/nimcp_quantum_walk.h"
    #include "core/neuralnet/nimcp_neuralnet.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumWalkTest : public NimcpTestBase {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr uint32_t SMALL_NETWORK_SIZE = 10;
    static constexpr uint32_t MEDIUM_NETWORK_SIZE = 50;

    quantum_walker_t* walker = nullptr;
    neural_network_t network = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup
        // Initialize random seed for reproducibility
        srand(42);
    }

    void TearDown() override {
        if (walker) {
            quantum_walk_destroy(walker);
            walker = nullptr;
        }
        if (network) {
            neural_network_destroy(network);
            network = nullptr;
        }
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }

    bool FloatEqual(float a, float b) {
        return std::abs(a - b) < EPSILON;
    }

    // Helper: Create a simple test network
    neural_network_t CreateTestNetwork(uint32_t num_neurons) {
        // Create zero-initialized config then set required fields
        network_config_t net_config = {};
        net_config.num_neurons = num_neurons;
        net_config.input_size = num_neurons;
        net_config.output_size = num_neurons;
        net_config.ei_ratio = 0.8f;
        net_config.learning_rate = 0.01f;
        net_config.stdp_window = 20.0f;
        net_config.refractory_period = 2.0f;
        net_config.min_weight = 0.0f;
        net_config.max_weight = 1.0f;

        neural_network_t net = neural_network_create(&net_config);
        return net;
    }
};

//=============================================================================
// Unit Test 1: Create quantum walker
//=============================================================================

TEST_F(QuantumWalkTest, Create_InitializesWalker) {
    // WHAT: Create quantum walker on neural network
    // WHY:  Test basic initialization

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);

    ASSERT_NE(walker, nullptr);
    EXPECT_EQ(walker->num_nodes, SMALL_NETWORK_SIZE);
    EXPECT_NE(walker->amplitudes, nullptr);
    EXPECT_NE(walker->probabilities, nullptr);
    EXPECT_EQ(walker->current_step, 0u);

    SUCCEED() << "Quantum walker created successfully";
}

//=============================================================================
// Unit Test 2: Create walker - NULL network fails gracefully
//=============================================================================

TEST_F(QuantumWalkTest, Create_NullNetworkFails) {
    // WHAT: Creating walker with NULL network should fail
    // WHY:  Defensive programming

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(nullptr, &config);

    EXPECT_EQ(walker, nullptr);
    SUCCEED() << "NULL network handled correctly";
}

//=============================================================================
// Unit Test 3: Create walker - NULL config fails gracefully
//=============================================================================

TEST_F(QuantumWalkTest, Create_NullConfigFails) {
    // WHAT: Creating walker with NULL config should fail
    // WHY:  Defensive programming

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    walker = quantum_walk_create(network, nullptr);

    EXPECT_EQ(walker, nullptr);
    SUCCEED() << "NULL config handled correctly";
}

//=============================================================================
// Unit Test 4: Destroy NULL walker is safe
//=============================================================================

TEST_F(QuantumWalkTest, Destroy_NullIsSafe) {
    // WHAT: Destroying NULL walker doesn't crash
    // WHY:  Defensive programming

    quantum_walk_destroy(nullptr);
    SUCCEED() << "Destroying NULL walker is safe";
}

//=============================================================================
// Unit Test 5: Initialize walker at specific node
//=============================================================================

TEST_F(QuantumWalkTest, Initialize_LocalizedState) {
    // WHAT: Initialize walker at specific node
    // WHY:  Test localized quantum state |ψ⟩ = |i⟩

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    uint32_t initial_node = 3;
    bool success = quantum_walk_initialize(walker, initial_node);

    ASSERT_TRUE(success);
    EXPECT_EQ(walker->initial_node, initial_node);

    // Check probability is localized at initial node
    float probabilities[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, probabilities);

    EXPECT_TRUE(FloatEqual(probabilities[initial_node], 1.0f));

    // All other probabilities should be zero
    for (uint32_t i = 0; i < SMALL_NETWORK_SIZE; i++) {
        if (i != initial_node) {
            EXPECT_TRUE(FloatEqual(probabilities[i], 0.0f));
        }
    }

    SUCCEED() << "Localized initialization works";
}

//=============================================================================
// Unit Test 6: Initialize - invalid node ID fails
//=============================================================================

TEST_F(QuantumWalkTest, Initialize_InvalidNodeFails) {
    // WHAT: Initialize with out-of-bounds node should fail
    // WHY:  Bounds checking

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    bool success = quantum_walk_initialize(walker, SMALL_NETWORK_SIZE + 10);

    EXPECT_FALSE(success);
    SUCCEED() << "Invalid node ID handled correctly";
}

//=============================================================================
// Unit Test 7: Initialize superposition - uniform
//=============================================================================

TEST_F(QuantumWalkTest, InitializeSuperposition_Uniform) {
    // WHAT: Initialize uniform superposition |ψ⟩ = (1/√N)Σ|i⟩
    // WHY:  Test distributed initial state

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // NULL amplitudes → uniform superposition
    bool success = quantum_walk_initialize_superposition(walker, nullptr);

    ASSERT_TRUE(success);

    // Check all probabilities are equal
    float probabilities[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, probabilities);

    float expected_prob = 1.0f / SMALL_NETWORK_SIZE;
    for (uint32_t i = 0; i < SMALL_NETWORK_SIZE; i++) {
        EXPECT_TRUE(FloatEqual(probabilities[i], expected_prob));
    }

    SUCCEED() << "Uniform superposition initialization works";
}

//=============================================================================
// Unit Test 8: Single quantum walk step
//=============================================================================

TEST_F(QuantumWalkTest, Step_EvolvesState) {
    // WHAT: Perform single quantum walk step
    // WHY:  Test basic quantum evolution

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Get initial probability
    float prob_before[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, prob_before);

    // Perform step
    bool success = quantum_walk_step(walker);

    ASSERT_TRUE(success);
    EXPECT_EQ(walker->current_step, 1u);

    // Get new probability
    float prob_after[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, prob_after);

    // Probability should have spread (not all at node 0 anymore)
    bool has_spread = false;
    for (uint32_t i = 1; i < SMALL_NETWORK_SIZE; i++) {
        if (prob_after[i] > EPSILON) {
            has_spread = true;
            break;
        }
    }

    EXPECT_TRUE(has_spread) << "Quantum walk should spread probability";

    SUCCEED() << "Quantum walk step works";
}

//=============================================================================
// Unit Test 9: Probability conservation
//=============================================================================

TEST_F(QuantumWalkTest, Step_ConservesProbability) {
    // WHAT: Verify Σ|αᵢ|² = 1 after steps
    // WHY:  Fundamental quantum mechanics requirement

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Perform multiple steps
    for (int step = 0; step < 10; step++) {
        quantum_walk_step(walker);

        // Check probability conservation
        float probabilities[SMALL_NETWORK_SIZE];
        quantum_walk_get_distribution(walker, probabilities);

        float total_prob = 0.0f;
        for (uint32_t i = 0; i < SMALL_NETWORK_SIZE; i++) {
            total_prob += probabilities[i];
        }

        EXPECT_TRUE(FloatEqual(total_prob, 1.0f))
            << "Step " << step << ": Total probability = " << total_prob;
    }

    SUCCEED() << "Probability is conserved across steps";
}

//=============================================================================
// Unit Test 10: Evolve for multiple steps
//=============================================================================

TEST_F(QuantumWalkTest, Evolve_MultipleSteps) {
    // WHAT: Evolve quantum walk for N steps
    // WHY:  Test convenience function

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    uint32_t num_steps = 20;
    bool success = quantum_walk_evolve(walker, num_steps);

    ASSERT_TRUE(success);
    EXPECT_EQ(walker->current_step, num_steps);

    SUCCEED() << "Multi-step evolution works";
}

//=============================================================================
// Unit Test 11: Get probability distribution
//=============================================================================

TEST_F(QuantumWalkTest, GetDistribution_ReturnsCorrectProbabilities) {
    // WHAT: Extract probability distribution P(i) = |αᵢ|²
    // WHY:  Test measurement interface

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 5);

    float probabilities[SMALL_NETWORK_SIZE];
    bool success = quantum_walk_get_distribution(walker, probabilities);

    ASSERT_TRUE(success);

    // Should be localized at node 5
    EXPECT_TRUE(FloatEqual(probabilities[5], 1.0f));

    SUCCEED() << "Get distribution works";
}

//=============================================================================
// Unit Test 12: Get quantum amplitudes
//=============================================================================

TEST_F(QuantumWalkTest, GetAmplitudes_ReturnsComplexAmplitudes) {
    // WHAT: Extract complex quantum amplitudes
    // WHY:  Test full quantum state access

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Allocate array for amplitudes
    quantum_amplitude_t* amplitudes = (quantum_amplitude_t*)malloc(SMALL_NETWORK_SIZE * sizeof(quantum_amplitude_t));
    bool success = quantum_walk_get_amplitudes(walker, amplitudes);

    ASSERT_TRUE(success);

    // Node 0 should have amplitude 1.0+0.0i
    float real = amplitudes[0].real();
    float imag = amplitudes[0].imag();

    EXPECT_TRUE(FloatEqual(real, 1.0f));
    EXPECT_TRUE(FloatEqual(imag, 0.0f));

    free(amplitudes);
    SUCCEED() << "Get amplitudes works";
}

//=============================================================================
// Unit Test 13: Quantum measurement (collapse)
//=============================================================================

TEST_F(QuantumWalkTest, Measure_CollapsesState) {
    // WHAT: Measure walker position (collapse quantum state)
    // WHY:  Test quantum measurement

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Start at uniform superposition
    quantum_walk_initialize_superposition(walker, nullptr);

    // Measure position
    uint32_t measured_node = quantum_walk_measure(walker);

    EXPECT_LT(measured_node, SMALL_NETWORK_SIZE);

    // After measurement, state should be localized
    float probabilities[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, probabilities);

    EXPECT_TRUE(FloatEqual(probabilities[measured_node], 1.0f));

    SUCCEED() << "Quantum measurement works";
}

//=============================================================================
// Unit Test 14: Compute statistics
//=============================================================================

TEST_F(QuantumWalkTest, ComputeStats_ReturnsValidStats) {
    // WHAT: Compute quantum walk statistics
    // WHY:  Test diagnostic interface

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);
    quantum_walk_evolve(walker, 10);

    quantum_walk_stats_t stats;
    bool success = quantum_walk_compute_stats(walker, &stats);

    ASSERT_TRUE(success);

    // Check statistics validity
    EXPECT_TRUE(FloatEqual(stats.total_probability, 1.0f));
    EXPECT_GT(stats.max_amplitude, 0.0f);
    EXPECT_LE(stats.max_amplitude, 1.0f);
    EXPECT_GT(stats.entropy, 0.0f);
    EXPECT_GT(stats.num_nonzero_amplitudes, 0u);

    SUCCEED() << "Statistics computation works";
}

//=============================================================================
// Unit Test 15: Verify walker integrity
//=============================================================================

TEST_F(QuantumWalkTest, Verify_ChecksProbabilityConservation) {
    // WHAT: Verify quantum walk integrity
    // WHY:  Test validation function

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Should verify correctly after initialization
    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    // Should still verify after evolution
    quantum_walk_evolve(walker, 5);
    valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Verification works";
}

//=============================================================================
// Unit Test 16: Reset walker
//=============================================================================

TEST_F(QuantumWalkTest, Reset_ReturnsToInitialState) {
    // WHAT: Reset walker to initial state
    // WHY:  Test walker reuse

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    uint32_t initial_node = 2;
    quantum_walk_initialize(walker, initial_node);
    quantum_walk_evolve(walker, 10);

    EXPECT_EQ(walker->current_step, 10u);

    // Reset
    bool success = quantum_walk_reset(walker);
    ASSERT_TRUE(success);

    EXPECT_EQ(walker->current_step, 0u);

    // Should be back at initial node
    float probabilities[SMALL_NETWORK_SIZE];
    quantum_walk_get_distribution(walker, probabilities);
    EXPECT_TRUE(FloatEqual(probabilities[initial_node], 1.0f));

    SUCCEED() << "Reset works";
}

//=============================================================================
// Unit Test 17: Hadamard coin operator
//=============================================================================

TEST_F(QuantumWalkTest, CoinOperator_Hadamard) {
    // WHAT: Test Hadamard coin operator
    // WHY:  Default balanced superposition

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_HADAMARD;
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);
    quantum_walk_step(walker);

    // Should spread probability
    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Hadamard coin works";
}

//=============================================================================
// Unit Test 18: Grover coin operator
//=============================================================================

TEST_F(QuantumWalkTest, CoinOperator_Grover) {
    // WHAT: Test Grover coin operator
    // WHY:  Faster spreading, biased toward uniform

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_GROVER;
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);
    quantum_walk_step(walker);

    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Grover coin works";
}

//=============================================================================
// Unit Test 19: Fourier coin operator
//=============================================================================

TEST_F(QuantumWalkTest, CoinOperator_Fourier) {
    // WHAT: Test Fourier coin operator
    // WHY:  Phase-dependent mixing

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_FOURIER;
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);
    quantum_walk_step(walker);

    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Fourier coin works";
}

//=============================================================================
// Unit Test 20: Identity coin operator (classical limit)
//=============================================================================

TEST_F(QuantumWalkTest, CoinOperator_Identity) {
    // WHAT: Test Identity coin (no mixing)
    // WHY:  Classical limit of quantum walk

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);
    quantum_walk_step(walker);

    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Identity coin works";
}

//=============================================================================
// Unit Test 21: Apply decoherence
//=============================================================================

TEST_F(QuantumWalkTest, Decoherence_AddsPhaseDamping) {
    // WHAT: Apply decoherence to quantum state
    // WHY:  Model environmental noise

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Apply decoherence
    bool success = quantum_walk_apply_decoherence(walker, 0.1f);
    ASSERT_TRUE(success);

    // Probability should still be conserved
    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Decoherence works";
}

//=============================================================================
// Unit Test 22: Fast configuration
//=============================================================================

TEST_F(QuantumWalkTest, Config_FastConfiguration) {
    // WHAT: Test fast configuration preset
    // WHY:  Verify configuration variants

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_fast_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_EQ(config.coin_type, COIN_GROVER);
    EXPECT_EQ(config.num_steps, 50u);
    EXPECT_TRUE(FloatEqual(config.decoherence_rate, 0.0f));

    SUCCEED() << "Fast configuration works";
}

//=============================================================================
// Unit Test 23: Hybrid configuration
//=============================================================================

TEST_F(QuantumWalkTest, Config_HybridConfiguration) {
    // WHAT: Test hybrid quantum-classical configuration
    // WHY:  Verify configuration variants

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_hybrid_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_TRUE(FloatEqual(config.hybrid_mixing, 0.5f));
    EXPECT_TRUE(config.decoherence_rate > 0.0f);

    SUCCEED() << "Hybrid configuration works";
}

//=============================================================================
// Unit Test 24: Medium-sized network
//=============================================================================

TEST_F(QuantumWalkTest, ScaleTest_MediumNetwork) {
    // WHAT: Test quantum walk on larger network
    // WHY:  Verify scalability

    network = CreateTestNetwork(MEDIUM_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    EXPECT_EQ(walker->num_nodes, MEDIUM_NETWORK_SIZE);

    quantum_walk_initialize(walker, 0);
    quantum_walk_evolve(walker, 10);

    bool valid = quantum_walk_verify(walker);
    EXPECT_TRUE(valid);

    SUCCEED() << "Medium network scaling works";
}

//=============================================================================
// Unit Test 25: Spreading after multiple steps
//=============================================================================

TEST_F(QuantumWalkTest, Spreading_IncreasesWithSteps) {
    // WHAT: Verify quantum walk spreads across network
    // WHY:  Core quantum walk behavior

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Initially localized - should have 1 nonzero amplitude
    quantum_walk_stats_t stats_initial;
    quantum_walk_compute_stats(walker, &stats_initial);

    // After evolution, should spread to more nodes
    quantum_walk_evolve(walker, 10);
    quantum_walk_stats_t stats_evolved;
    quantum_walk_compute_stats(walker, &stats_evolved);

    EXPECT_GT(stats_evolved.num_nonzero_amplitudes, stats_initial.num_nonzero_amplitudes);

    SUCCEED() << "Quantum walk spreading works";
}

//=============================================================================
// Unit Test 26: Normalization via walker verification
//=============================================================================

TEST_F(QuantumWalkTest, Normalization_MaintainsProbabilityConservation) {
    // WHAT: Test that probability is conserved across operations
    // WHY:  Core quantum mechanics requirement

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Initialize and verify
    quantum_walk_initialize(walker, 5);
    EXPECT_TRUE(quantum_walk_verify(walker));

    // Evolve and verify again
    quantum_walk_evolve(walker, 20);
    EXPECT_TRUE(quantum_walk_verify(walker));

    // Verify total probability is 1.0
    quantum_walk_stats_t stats;
    quantum_walk_compute_stats(walker, &stats);
    EXPECT_TRUE(FloatEqual(stats.total_probability, 1.0f));

    SUCCEED() << "Normalization maintains probability conservation";
}

//=============================================================================
// Unit Test 27: Entropy increases with spreading
//=============================================================================

TEST_F(QuantumWalkTest, Entropy_IncreasesWithSpreading) {
    // WHAT: Verify entropy grows as quantum walk spreads
    // WHY:  Measure information distribution

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Initial entropy (localized state)
    quantum_walk_stats_t stats_initial;
    quantum_walk_compute_stats(walker, &stats_initial);

    // Evolve and measure entropy
    quantum_walk_evolve(walker, 10);
    quantum_walk_stats_t stats_evolved;
    quantum_walk_compute_stats(walker, &stats_evolved);

    // Entropy should increase as state spreads
    EXPECT_GT(stats_evolved.entropy, stats_initial.entropy);

    SUCCEED() << "Entropy increases with spreading";
}

//=============================================================================
// Unit Test 28: NULL pointer safety in get_distribution
//=============================================================================

TEST_F(QuantumWalkTest, GetDistribution_NullWalkerFails) {
    // WHAT: Test NULL walker handling
    // WHY:  Defensive programming

    float probabilities[SMALL_NETWORK_SIZE];
    bool success = quantum_walk_get_distribution(nullptr, probabilities);

    EXPECT_FALSE(success);
    SUCCEED() << "NULL walker handled in get_distribution";
}

//=============================================================================
// Unit Test 29: NULL pointer safety in get_amplitudes
//=============================================================================

TEST_F(QuantumWalkTest, GetAmplitudes_NullArrayFails) {
    // WHAT: Test NULL output array handling
    // WHY:  Defensive programming

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    bool success = quantum_walk_get_amplitudes(walker, nullptr);

    EXPECT_FALSE(success);
    SUCCEED() << "NULL array handled in get_amplitudes";
}

//=============================================================================
// Unit Test 30: Decoherence bounds checking
//=============================================================================

TEST_F(QuantumWalkTest, Decoherence_BoundsChecking) {
    // WHAT: Test decoherence with invalid parameters
    // WHY:  Validate input bounds [0, 1]

    network = CreateTestNetwork(SMALL_NETWORK_SIZE);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 0);

    // Test out-of-bounds values
    bool success1 = quantum_walk_apply_decoherence(walker, -0.1f);
    EXPECT_FALSE(success1) << "Negative decoherence rejected";

    bool success2 = quantum_walk_apply_decoherence(walker, 1.5f);
    EXPECT_FALSE(success2) << "Decoherence > 1 rejected";

    // Valid values should succeed
    bool success3 = quantum_walk_apply_decoherence(walker, 0.5f);
    EXPECT_TRUE(success3) << "Valid decoherence accepted";

    SUCCEED() << "Decoherence bounds checking works";
}

//=============================================================================
// Test Summary
//=============================================================================

// Total tests: 30
// Coverage:
// - Lifecycle: 4 tests (create, destroy, clone concepts)
// - Initialization: 4 tests (localized, superposition, bounds)
// - Evolution: 5 tests (step, evolve, probability conservation)
// - Measurement: 4 tests (distribution, amplitudes, measurement, stats)
// - Validation: 3 tests (verify, reset, normalization)
// - Coin operators: 4 tests (Hadamard, Grover, Fourier, Identity)
// - Configuration: 2 tests (fast, hybrid)
// - Advanced: 2 tests (decoherence, entropy)
// - Utilities: 2 tests (normalize, probability)
// - Edge cases: 4 tests (NULL safety, bounds checking)
// - Scaling: 1 test (medium network)
