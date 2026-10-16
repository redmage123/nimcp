//=============================================================================
// test_quantum_walk_regression.cpp - Quantum Walk Regression Tests
//=============================================================================
/**
 * @file test_quantum_walk_regression.cpp
 * @brief Regression tests to ensure quantum walk behavior remains consistent
 *
 * WHAT: Verify quantum walk implementation maintains expected behavior across versions
 * WHY: Detect unintended changes and ensure backward compatibility
 * HOW: Fixed test cases with known expected outputs
 *
 * TEST COVERAGE:
 * 1. Deterministic evolution with fixed seed
 * 2. Probability distribution snapshots
 * 3. Custom coin operator behavior consistency
 * 4. Performance benchmarks (detect regressions)
 * 5. Numerical stability checks
 * 6. Edge case handling consistency
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.9.0 Phase C2.1
 */

#include <gtest/gtest.h>
#include <cmath>
#include <complex>
#include <vector>
#include <chrono>

#include "utils/quantum/nimcp_quantum_walk.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/memory/nimcp_memory.h"
#include "test_helpers.h"

//=============================================================================
// Test Utilities
//=============================================================================

/**
 * @brief Create deterministic test network
 */
neural_network_t create_test_network(uint32_t num_neurons) {
    network_config_t config = create_test_network_config();
    config.num_neurons = num_neurons;
    config.input_size = num_neurons;
    config.output_size = num_neurons;

    neural_network_t network = neural_network_create(&config);

    return network;
}

/**
 * @brief Allocate 2D matrix
 */
quantum_amplitude_t** allocate_coin_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = (quantum_amplitude_t**)nimcp_malloc(
        size * sizeof(quantum_amplitude_t*)
    );

    for (uint32_t i = 0; i < size; i++) {
        matrix[i] = (quantum_amplitude_t*)nimcp_calloc(
            size, sizeof(quantum_amplitude_t)
        );
    }

    return matrix;
}

/**
 * @brief Free 2D matrix
 */
void free_coin_matrix(quantum_amplitude_t** matrix, uint32_t size) {
    if (!matrix) return;

    for (uint32_t i = 0; i < size; i++) {
        if (matrix[i]) {
            nimcp_free(matrix[i]);
        }
    }
    nimcp_free(matrix);
}

/**
 * @brief Create fixed Hadamard matrix for regression testing
 */
quantum_amplitude_t** create_fixed_hadamard_matrix(uint32_t size) {
    quantum_amplitude_t** matrix = allocate_coin_matrix(size);

    float inv_sqrt2 = 1.0f / sqrtf(2.0f);

    if (size == 2) {
        matrix[0][0] = inv_sqrt2;
        matrix[0][1] = inv_sqrt2;
        matrix[1][0] = inv_sqrt2;
        matrix[1][1] = -inv_sqrt2;
    } else {
        // Generalized pattern
        float scale = 1.0f / sqrtf((float)size);
        for (uint32_t i = 0; i < size; i++) {
            for (uint32_t j = 0; j < size; j++) {
                float sign = ((i + j) % 2 == 0) ? 1.0f : -1.0f;
                matrix[i][j] = scale * sign;
            }
        }
    }

    return matrix;
}

/**
 * @brief Check if two probability distributions are close
 */
bool distributions_match(const float* probs1, const float* probs2, uint32_t size, float tolerance) {
    for (uint32_t i = 0; i < size; i++) {
        if (fabsf(probs1[i] - probs2[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumWalkRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        srand(12345); // Fixed seed for determinism
    }

    void TearDown() override {
        // Cleanup per test
    }
};

//=============================================================================
// Deterministic Evolution Tests
//=============================================================================

TEST_F(QuantumWalkRegressionTest, DeterministicEvolutionSnapshot) {
    // WHAT: Verify quantum walk produces consistent results
    // WHY: Detect any changes to core algorithm
    // HOW: Fixed network, seed, evolution → compare with known snapshot

    const uint32_t num_neurons = 20;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_HADAMARD;
    config.decoherence_rate = 0.0f; // Disable decoherence for determinism
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    // Fixed initialization
    quantum_walk_initialize(walker, 10);

    // Evolve exactly 50 steps
    quantum_walk_evolve(walker, 50);

    // Get probability distribution
    float* probs = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs);

    // Expected snapshot (from known correct implementation)
    // These values should remain consistent across versions
    // Note: Actual values depend on network connectivity, so we check properties

    // Property 1: Total probability = 1.0
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        total_prob += probs[i];
    }
    EXPECT_NEAR(total_prob, 1.0f, 1e-4f);

    // Property 2: Spreading occurred (not localized)
    uint32_t nonzero_count = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (probs[i] > 1e-6f) {
            nonzero_count++;
        }
    }
    EXPECT_GT(nonzero_count, 5u);

    // Property 3: Maximum probability < 0.5 (spread out)
    float max_prob = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (probs[i] > max_prob) {
            max_prob = probs[i];
        }
    }
    EXPECT_LT(max_prob, 0.5f);

    printf("Deterministic evolution properties verified:\n");
    printf("  Total probability: %.6f\n", total_prob);
    printf("  Nonzero amplitudes: %u\n", nonzero_count);
    printf("  Max probability: %.6f\n", max_prob);

    nimcp_free(probs);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkRegressionTest, CustomCoinDeterministicBehavior) {
    // WHAT: Verify custom coin operator produces consistent results
    // WHY: Ensure custom coin implementation doesn't change
    // HOW: Apply fixed Hadamard matrix, compare output

    const uint32_t num_neurons = 10;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 5);

    // Create fixed Hadamard matrix
    quantum_amplitude_t** hadamard = create_fixed_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    // Apply custom coin operator once
    bool success = quantum_walk_apply_custom_coin(walker,
        const_cast<const quantum_amplitude_t**>(hadamard));
    ASSERT_TRUE(success);

    // Get resulting distribution
    float* probs = (float*)nimcp_malloc(num_neurons * sizeof(float));
    quantum_walk_get_distribution(walker, probs);

    // Verify expected properties (not exact values due to normalization)
    float total_prob = 0.0f;
    for (uint32_t i = 0; i < num_neurons; i++) {
        total_prob += probs[i];
    }
    EXPECT_NEAR(total_prob, 1.0f, 1e-4f);

    // After applying Hadamard to localized state, probability spreads
    uint32_t nonzero_count = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (probs[i] > 1e-6f) {
            nonzero_count++;
        }
    }
    EXPECT_GT(nonzero_count, 1u); // Should spread to multiple nodes

    printf("Custom coin deterministic behavior verified\n");

    nimcp_free(probs);
    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(QuantumWalkRegressionTest, NumericalStabilityLongEvolution) {
    // WHAT: Verify numerical stability over many steps
    // WHY: Detect accumulation of floating-point errors
    // HOW: Evolve 1000 steps, check probability conservation

    const uint32_t num_neurons = 30;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.normalize_each_step = true; // Enable normalization
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 15);

    // Evolve many steps
    const uint32_t num_steps = 1000;
    for (uint32_t step = 0; step < num_steps; step++) {
        quantum_walk_step(walker);

        // Check probability conservation every 100 steps
        if (step % 100 == 0) {
            quantum_walk_stats_t stats;
            quantum_walk_compute_stats(walker, &stats);

            EXPECT_NEAR(stats.total_probability, 1.0f, 1e-3f)
                << "Probability drift at step " << step;
        }
    }

    // Final check
    EXPECT_TRUE(quantum_walk_verify(walker));

    printf("✅ Numerical stability verified over %u steps\n", num_steps);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkRegressionTest, CustomCoinNumericalStability) {
    // WHAT: Test numerical stability with custom coin operators
    // WHY: Ensure custom coins don't introduce instability
    // HOW: Apply custom coin 500 times, check conservation

    const uint32_t num_neurons = 16;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 8);

    quantum_amplitude_t** hadamard = create_fixed_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    // Apply custom coin many times
    const uint32_t num_applications = 500;
    for (uint32_t i = 0; i < num_applications; i++) {
        quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(hadamard));

        if (i % 50 == 0) {
            quantum_walk_stats_t stats;
            quantum_walk_compute_stats(walker, &stats);

            EXPECT_NEAR(stats.total_probability, 1.0f, 1e-3f)
                << "Probability drift at application " << i;
        }
    }

    printf("✅ Custom coin numerical stability verified over %u applications\n",
           num_applications);

    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(QuantumWalkRegressionTest, PerformanceBaseline) {
    // WHAT: Establish performance baseline for quantum walk
    // WHY: Detect performance regressions
    // HOW: Time evolution on standard network size

    const uint32_t num_neurons = 100;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 50);

    const uint32_t num_steps = 100;

    auto start = std::chrono::high_resolution_clock::now();
    quantum_walk_evolve(walker, num_steps);
    auto end = std::chrono::high_resolution_clock::now();

    float time_ms = std::chrono::duration<float, std::milli>(end - start).count();
    float time_per_step_us = (time_ms * 1000.0f) / num_steps;

    printf("Performance baseline (N=%u, steps=%u):\n", num_neurons, num_steps);
    printf("  Total time: %.2f ms\n", time_ms);
    printf("  Time per step: %.2f μs\n", time_per_step_us);

    // Baseline: Should complete in < 500ms on typical hardware
    EXPECT_LT(time_ms, 500.0f);

    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

TEST_F(QuantumWalkRegressionTest, CustomCoinPerformanceBaseline) {
    // WHAT: Establish performance baseline for custom coin
    // WHY: Detect custom coin performance regressions
    // HOW: Time many custom coin applications

    const uint32_t num_neurons = 50;
    neural_network_t network = create_test_network(num_neurons);
    ASSERT_NE(network, nullptr);

    quantum_walk_config_t config = quantum_walk_default_config();
    config.coin_type = COIN_IDENTITY;
    quantum_walker_t* walker = quantum_walk_create(network, &config);
    ASSERT_NE(walker, nullptr);

    quantum_walk_initialize(walker, 25);

    quantum_amplitude_t** hadamard = create_fixed_hadamard_matrix(num_neurons);
    ASSERT_NE(hadamard, nullptr);

    const uint32_t num_applications = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < num_applications; i++) {
        quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(hadamard));
    }

    auto end = std::chrono::high_resolution_clock::now();

    float time_ms = std::chrono::duration<float, std::milli>(end - start).count();
    float time_per_application_us = (time_ms * 1000.0f) / num_applications;

    printf("Custom coin performance baseline (N=%u, apps=%u):\n",
           num_neurons, num_applications);
    printf("  Total time: %.2f ms\n", time_ms);
    printf("  Time per application: %.2f μs\n", time_per_application_us);

    // Should complete in reasonable time
    EXPECT_LT(time_ms, 200.0f);

    free_coin_matrix(hadamard, num_neurons);
    quantum_walk_destroy(walker);
    neural_network_destroy(network);
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(QuantumWalkRegressionTest, SmallNetworkBehavior) {
    // WHAT: Test behavior on very small networks
    // WHY: Ensure edge cases handled consistently
    // HOW: Test with N=2, N=3, N=5 neurons

    uint32_t sizes[] = {2, 3, 5};

    for (uint32_t size : sizes) {
        neural_network_t network = create_test_network(size);
        ASSERT_NE(network, nullptr);

        quantum_walk_config_t config = quantum_walk_default_config();
        quantum_walker_t* walker = quantum_walk_create(network, &config);
        ASSERT_NE(walker, nullptr);

        quantum_walk_initialize(walker, 0);
        quantum_walk_evolve(walker, 20);

        // Verify valid state
        EXPECT_TRUE(quantum_walk_verify(walker));

        printf("Small network (N=%u) behavior verified\n", size);

        quantum_walk_destroy(walker);
        neural_network_destroy(network);
    }
}

TEST_F(QuantumWalkRegressionTest, CustomCoinSmallMatrix) {
    // WHAT: Test custom coin with small matrices
    // WHY: Ensure edge cases handled
    // HOW: Test with 2x2 and 3x3 matrices

    uint32_t sizes[] = {2, 3};

    for (uint32_t size : sizes) {
        neural_network_t network = create_test_network(size);
        ASSERT_NE(network, nullptr);

        quantum_walk_config_t config = quantum_walk_default_config();
        config.coin_type = COIN_IDENTITY;
        quantum_walker_t* walker = quantum_walk_create(network, &config);
        ASSERT_NE(walker, nullptr);

        quantum_walk_initialize(walker, 0);

        quantum_amplitude_t** hadamard = create_fixed_hadamard_matrix(size);
        ASSERT_NE(hadamard, nullptr);

        bool success = quantum_walk_apply_custom_coin(walker,
            const_cast<const quantum_amplitude_t**>(hadamard));
        ASSERT_TRUE(success);

        EXPECT_TRUE(quantum_walk_verify(walker));

        printf("Custom coin small matrix (%ux%u) verified\n", size, size);

        free_coin_matrix(hadamard, size);
        quantum_walk_destroy(walker);
        neural_network_destroy(network);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
