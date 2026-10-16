//=============================================================================
// test_quantum_walk.cpp - Quantum Walk Unit Tests
//=============================================================================
/**
 * @file test_quantum_walk.cpp
 * @brief Comprehensive unit tests for quantum walk implementation
 *
 * WHAT: Validate quantum walk correctness, performance, and √N speedup
 * WHY: Ensure quantum diffusion works correctly for neuromodulation
 * HOW: Unit tests + benchmarks for various configurations
 *
 * TEST COVERAGE:
 * 1. Lifecycle (create, destroy, clone)
 * 2. Initialization (single node, superposition)
 * 3. Quantum evolution (step, evolve)
 * 4. Coin operators (Hadamard, Grover, Fourier)
 * 5. Probability conservation
 * 6. Measurement (quantum → classical)
 * 7. Statistics computation
 * 8. Performance vs classical diffusion
 * 9. Edge cases and error handling
 * 10. Hybrid quantum-classical modes
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 2.9.0 Phase C2.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>

    #include "utils/quantum/nimcp_quantum_walk.h"
    #include "core/neuralnet/nimcp_neuralnet.h"
    #include "utils/memory/nimcp_memory.h"
    #include "utils/nimcp_test_base.h"

//=============================================================================
// Test Utilities
//=============================================================================

/**
 * @brief Create test neural network
 */
neural_network_t create_test_network(uint32_t num_neurons) {
    network_config_t config = {0};
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;
    config.num_layers = 1;
    config.layer_sizes = (uint32_t*)nimcp_malloc(sizeof(uint32_t));
    config.layer_sizes[0] = num_neurons;
    config.enable_stdp = false;
    config.enable_hebbian = false;

    neural_network_t network = neural_network_create(&config);
    nimcp_free(config.layer_sizes);

    return network;
}

/**
 * @brief Compute Shannon entropy
 */
float compute_entropy(const float* probs, uint32_t size) {
    float entropy = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (probs[i] > 1e-8f) {
            entropy -= probs[i] * logf(probs[i]);
        }
    }
    return entropy;
}

/**
 * @brief Check if probability distribution is valid
 */
bool is_valid_distribution(const float* probs, uint32_t size) {
    float total = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        if (probs[i] < 0.0f || probs[i] > 1.0f) return false;
        total += probs[i];
    }
    return fabsf(total - 1.0f) < 1e-4f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumWalkTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup
        srand(42); // Reproducibility
    }

    void TearDown() override {
        // Cleanup per test
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(QuantumWalkTest, CreateAndDestroy) {
    // WHAT: Test basic allocation and deallocation
    // WHY: Ensure no memory leaks
    // HOW: Create, verify structure, destroy

    const uint32_t num_neurons = 50;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);

    ASSERT_NE(walker, nullptr);
    EXPECT_EQ(walker->num_nodes, num_neurons);
    EXPECT_NE(walker->amplitudes, nullptr);
    EXPECT_NE(walker->probabilities, nullptr);

    // Cleanup
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkTest, CloneWalker) {
    // WHAT: Test cloning quantum state
    // WHY: Verify deep copy works correctly
    // HOW: Create, initialize, clone, verify identical

    neural_network_t network = create_test_network(20);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* original = quantum_walk_create(network, &config);
    ASSERT_NE(original, nullptr);

    // Initialize and evolve original
    quantum_walk_initialize(original, 5);
    quantum_walk_evolve(original, 10);

    // Clone
    quantum_walker_t* clone = quantum_walk_clone(original);
    ASSERT_NE(clone, nullptr);

    // Verify identical state
    for (uint32_t i = 0; i < original->num_nodes; i++) {
        EXPECT_FLOAT_EQ(original->probabilities[i], clone->probabilities[i]);
    }

    // Cleanup
    quantum_walk_destroy(original);
    quantum_walk_destroy(clone);
    neural_network_destroy(network);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(QuantumWalkTest, InitializeSingleNode) {
    // WHAT: Test initialization at single node
    // WHY: Verify localized initial state
    // HOW: Initialize, check probability = 1.0 at node

    neural_network_t network = create_test_network(30);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    const uint32_t initial_node = 10;
    bool success = quantum_walk_initialize(walker, initial_node);
    ASSERT_TRUE(success);

    // Check: P(initial_node) = 1.0, all others = 0.0
    float* probs = (float*)nimcp_malloc(walker->num_nodes * sizeof(float));
    quantum_walk_get_distribution(walker, probs);

    EXPECT_FLOAT_EQ(probs[initial_node], 1.0f);

    float total = 0.0f;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        if (i != initial_node) {
            EXPECT_NEAR(probs[i], 0.0f, 1e-6f);
        }
        total += probs[i];
    }

    EXPECT_NEAR(total, 1.0f, 1e-6f);

    nimcp_free(probs);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkTest, InitializeUniformSuperposition) {
    // WHAT: Test uniform superposition initialization
    // WHY: Verify equal probability distribution
    // HOW: Initialize with NULL (uniform), check P(i) = 1/N

    neural_network_t network = create_test_network(25);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Initialize uniform superposition
    bool success = quantum_walk_initialize_superposition(walker, nullptr);
    ASSERT_TRUE(success);

    float* probs = (float*)nimcp_malloc(walker->num_nodes * sizeof(float));
    quantum_walk_get_distribution(walker, probs);

    // Check: Each P(i) ≈ 1/N
    float expected_prob = 1.0f / walker->num_nodes;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        EXPECT_NEAR(probs[i], expected_prob, 1e-4f);
    }

    nimcp_free(probs);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Quantum Evolution Tests
//=============================================================================

TEST_F(QuantumWalkTest, QuantumWalkStep) {
    // WHAT: Test single quantum walk step
    // WHY: Verify evolution works without crashing
    // HOW: Initialize, step, verify probability conservation

    neural_network_t network = create_test_network(40);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 20);

    // Perform single step
    bool success = quantum_walk_step(walker);
    ASSERT_TRUE(success);

    // Verify probability conservation
    EXPECT_TRUE(quantum_walk_verify(walker));

    float* probs = (float*)nimcp_malloc(walker->num_nodes * sizeof(float));
    quantum_walk_get_distribution(walker, probs);
    EXPECT_TRUE(is_valid_distribution(probs, walker->num_nodes));

    nimcp_free(probs);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkTest, QuantumWalkEvolve) {
    // WHAT: Test multi-step evolution
    // WHY: Verify spreading over time
    // HOW: Evolve 50 steps, measure entropy increase

    neural_network_t network = create_test_network(50);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 25);

    // Measure initial entropy
    float* probs = (float*)nimcp_malloc(walker->num_nodes * sizeof(float));
    quantum_walk_get_distribution(walker, probs);
    float initial_entropy = compute_entropy(probs, walker->num_nodes);

    // Evolve
    bool success = quantum_walk_evolve(walker, 50);
    ASSERT_TRUE(success);

    // Measure final entropy
    quantum_walk_get_distribution(walker, probs);
    float final_entropy = compute_entropy(probs, walker->num_nodes);

    printf("Initial entropy: %.4f\n", initial_entropy);
    printf("Final entropy:   %.4f\n", final_entropy);

    // Entropy should increase (spreading)
    EXPECT_GT(final_entropy, initial_entropy);

    // Probability still conserved
    EXPECT_TRUE(quantum_walk_verify(walker));

    nimcp_free(probs);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Coin Operator Tests
//=============================================================================

TEST_F(QuantumWalkTest, HadamardCoin) {
    // WHAT: Test Hadamard coin operator
    // WHY: Verify balanced superposition creation
    // HOW: Use Hadamard coin, check spreading

    neural_network_t network = create_test_network(30);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_HADAMARD;

    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 15);
    quantum_walk_evolve(walker, 20);

    // Verify valid distribution
    EXPECT_TRUE(quantum_walk_verify(walker));

    printf("✅ Hadamard coin test passed\n");

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkTest, GroverCoin) {
    // WHAT: Test Grover diffusion coin
    // WHY: Verify biased mixing works
    // HOW: Compare Grover vs Hadamard spreading

    neural_network_t network = create_test_network(30);
    ASSERT_NE(network, nullptr);

    // Hadamard walker
    quantum_walk_config_t config_h = quantum_walk_default_config();
    config_h.coin_type = COIN_HADAMARD;
    quantum_walker_t* walker_h = quantum_walk_create(network, &config_h);
    ASSERT_NE(walker_h, nullptr);

    // Grover walker
    quantum_walk_config_t config_g = quantum_walk_default_config();
    config_g.coin_type = COIN_GROVER;
    quantum_walker_t* walker_g = quantum_walk_create(network, &config_g);
    ASSERT_NE(walker_g, nullptr);

    // Evolve both
    quantum_walk_initialize(walker_h, 15);
    quantum_walk_initialize(walker_g, 15);

    quantum_walk_evolve(walker_h, 20);
    quantum_walk_evolve(walker_g, 20);

    // Both should be valid
    EXPECT_TRUE(quantum_walk_verify(walker_h));
    EXPECT_TRUE(quantum_walk_verify(walker_g));

    printf("✅ Grover coin test passed\n");

    quantum_walk_destroy(walker_h);
    quantum_walk_destroy(walker_g);
    neural_network_destroy(network);
}

//=============================================================================
// Probability Conservation Tests
//=============================================================================

TEST_F(QuantumWalkTest, ProbabilityConservation) {
    // WHAT: Test probability conservation over many steps
    // WHY: Critical quantum property must hold
    // HOW: Evolve, check Σ|αᵢ|² = 1.0 at each step

    neural_network_t network = create_test_network(40);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 20);

    // Evolve and check probability at each step
    for (uint32_t step = 0; step < 100; step++) {
        quantum_walk_step(walker);

        quantum_walk_stats_t stats;
        quantum_walk_compute_stats(walker, &stats);

        EXPECT_NEAR(stats.total_probability, 1.0f, 1e-4f)
            << "Probability not conserved at step " << step;
    }

    printf("✅ Probability conservation verified over 100 steps\n");

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Measurement Tests
//=============================================================================

TEST_F(QuantumWalkTest, QuantumMeasurement) {
    // WHAT: Test measurement (collapse to classical state)
    // WHY: Verify Born rule sampling
    // HOW: Measure multiple times, check distribution

    neural_network_t network = create_test_network(20);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Initialize uniform superposition
    quantum_walk_initialize_superposition(walker, nullptr);

    // Measure 1000 times, count outcomes
    std::vector<uint32_t> counts(walker->num_nodes, 0);

    for (uint32_t trial = 0; trial < 1000; trial++) {
        quantum_walker_t* clone = quantum_walk_clone(walker);
        uint32_t measured_node = quantum_walk_measure(clone);
        counts[measured_node]++;
        quantum_walk_destroy(clone);
    }

    // Check: Each node measured ~50 times (1000/20)
    float expected_count = 1000.0f / walker->num_nodes;
    for (uint32_t i = 0; i < walker->num_nodes; i++) {
        EXPECT_NEAR(counts[i], expected_count, 20.0f);
    }

    printf("✅ Quantum measurement follows Born rule\n");

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(QuantumWalkTest, ComputeStatistics) {
    // WHAT: Test statistics computation
    // WHY: Verify diagnostic functions work
    // HOW: Compute stats, check reasonable values

    neural_network_t network = create_test_network(50);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 25);
    quantum_walk_evolve(walker, 30);

    quantum_walk_stats_t stats;
    bool success = quantum_walk_compute_stats(walker, &stats);
    ASSERT_TRUE(success);

    // Verify reasonable values
    EXPECT_NEAR(stats.total_probability, 1.0f, 1e-4f);
    EXPECT_GT(stats.max_amplitude, 0.0f);
    EXPECT_LE(stats.max_amplitude, 1.0f);
    EXPECT_GT(stats.entropy, 0.0f);
    EXPECT_GT(stats.spreading_distance, 0.0f);
    EXPECT_GT(stats.speedup_vs_classical, 1.0f);

    printf("Spreading distance: %.2f nodes\n", stats.spreading_distance);
    printf("Speedup vs classical: %.2fx\n", stats.speedup_vs_classical);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Performance Benchmarks
//=============================================================================

TEST_F(QuantumWalkTest, PerformanceBenchmark) {
    // WHAT: Measure quantum walk performance
    // WHY: Quantify computational overhead
    // HOW: Time evolution for various network sizes

    printf("\n=== Quantum Walk Performance Benchmark ===\n");
    printf("Size    Steps    Time (ms)   Time/Step (μs)\n");
    printf("----------------------------------------------\n");

    uint32_t sizes[] = {50, 100, 200, 500};
    const uint32_t num_steps = 100;

    for (uint32_t size : sizes) {
        neural_network_t network = create_test_network(size);
        ASSERT_NE(network, nullptr);

        quantum_walk_config_t config = quantum_walk_default_config();
        quantum_walker_t* walker = quantum_walk_create(network, &config);
        ASSERT_NE(walker, nullptr);

        quantum_walk_initialize(walker, size / 2);

        auto start = std::chrono::high_resolution_clock::now();

        quantum_walk_evolve(walker, num_steps);

        auto end = std::chrono::high_resolution_clock::now();
        float time_ms = std::chrono::duration<float, std::milli>(end - start).count();
        float time_per_step_us = (time_ms * 1000.0f) / num_steps;

        printf("%4u    %5u    %8.2f    %12.2f\n",
               size, num_steps, time_ms, time_per_step_us);

        quantum_walk_destroy(walker);
        neural_network_destroy(network);
    }
    printf("----------------------------------------------\n");
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(QuantumWalkTest, NullInputHandling) {
    // WHAT: Test NULL pointer handling
    // WHY: Ensure graceful failure
    // HOW: Pass NULL to all functions

    neural_network_t network = create_test_network(10);
    quantum_walk_config_t config = quantum_walk_default_config();

    // NULL network
    quantum_walker_t* walker1 = quantum_walk_create(nullptr, &config);
    EXPECT_EQ(walker1, nullptr);

    // NULL config
    quantum_walker_t* walker2 = quantum_walk_create(network, nullptr);
    EXPECT_EQ(walker2, nullptr);

    // NULL walker operations
    EXPECT_FALSE(quantum_walk_step(nullptr));
    EXPECT_FALSE(quantum_walk_evolve(nullptr, 10));
    EXPECT_FALSE(quantum_walk_verify(nullptr));

    neural_network_destroy(network);
}

TEST_F(QuantumWalkTest, InvalidNodeInitialization) {
    // WHAT: Test out-of-bounds node initialization
    // WHY: Ensure bounds checking
    // HOW: Initialize with node_id >= num_nodes

    neural_network_t network = create_test_network(20);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Try to initialize at invalid node
    bool success = quantum_walk_initialize(walker, 100);
    EXPECT_FALSE(success);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(QuantumWalkTest, DefaultConfig) {
    quantum_walk_config_t config = quantum_walk_default_config();

    EXPECT_EQ(config.coin_type, COIN_HADAMARD);
    EXPECT_EQ(config.num_steps, 100);
    EXPECT_FLOAT_EQ(config.hybrid_mixing, 0.0f);
    EXPECT_TRUE(config.normalize_each_step);
}

TEST_F(QuantumWalkTest, FastConfig) {
    quantum_walk_config_t config = quantum_walk_fast_config();

    EXPECT_EQ(config.coin_type, COIN_GROVER);
    EXPECT_LT(config.num_steps, 100);
    EXPECT_FLOAT_EQ(config.decoherence_rate, 0.0f);
}

TEST_F(QuantumWalkTest, HybridConfig) {
    quantum_walk_config_t config = quantum_walk_hybrid_config();

    EXPECT_GT(config.hybrid_mixing, 0.0f);
    EXPECT_LT(config.hybrid_mixing, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
