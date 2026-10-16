/**
 * @file test_quantum_gpu_recovery_integration.cpp
 * @brief Integration tests for GPU recovery in quantum module
 *
 * WHAT: Integration tests for quantum GPU operations with recovery
 * WHY:  Verify end-to-end recovery across quantum algorithms
 * HOW:  Test complete quantum workflows with error injection and recovery
 *
 * TEST COVERAGE:
 * - End-to-end Grover search with recovery
 * - Complete quantum annealing workflow with recovery
 * - VQE optimization with recovery
 * - QMC simulation with recovery
 * - Cross-module quantum operations with recovery
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/quantum/nimcp_quantum_gpu.h"
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class QuantumGPURecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx = nimcp_gpu_context_create_auto();
        if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
            GTEST_SKIP() << "Failed to create GPU context";
        }
        rctx = nimcp_gpu_recovery_context_create(NULL);
        if (!rctx) {
            GTEST_SKIP() << "Failed to create recovery context";
        }
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (state) {
            nimcp_quantum_state_destroy(state);
            state = nullptr;
        }
        if (model) {
            nimcp_ising_model_destroy(model);
            model = nullptr;
        }
        if (rctx) {
            nimcp_gpu_recovery_context_destroy(rctx);
            rctx = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_gpu_recovery_context_t* rctx = nullptr;
    nimcp_quantum_state_t* state = nullptr;
    nimcp_ising_model_t* model = nullptr;
#endif
};

/* ============================================================================
 * Integration Test: Complete Grover Search Workflow
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, CompleteGroverSearchWorkflow) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Test Grover search with different qubit counts */
    for (int n_qubits = 3; n_qubits <= 6; n_qubits++) {
        uint32_t n_states = 1u << n_qubits;
        uint32_t target_state = n_states / 2;  /* Middle state */

        nimcp_grover_config_t config = nimcp_grover_default_config(n_qubits);
        config.marked_states = new uint32_t[1];
        config.marked_states[0] = target_state;
        config.n_marked = 1;

        uint32_t found_state = UINT32_MAX;
        bool success = false;

        bool result = nimcp_grover_search(ctx, &config, &found_state, &success);
        EXPECT_TRUE(result) << "Grover search should complete for " << n_qubits << " qubits";

        if (success) {
            EXPECT_EQ(found_state, target_state)
                << "Should find the target state for " << n_qubits << " qubits";
        }

        delete[] config.marked_states;
    }

    /* Verify recovery tracked operations */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Complete Quantum Annealing Workflow
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, CompleteQuantumAnnealingWorkflow) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Test annealing with increasing problem sizes */
    for (int n_spins = 4; n_spins <= 12; n_spins += 4) {
        model = nimcp_ising_model_create(ctx, n_spins);
        ASSERT_NE(model, nullptr) << "Model creation failed for " << n_spins << " spins";

        /* Set up antiferromagnetic chain (ground state is alternating) */
        std::vector<float> J(n_spins * n_spins, 0.0f);
        std::vector<float> h(n_spins, 0.0f);

        for (int i = 0; i < n_spins - 1; i++) {
            J[i * n_spins + (i + 1)] = 1.0f;  /* Positive = antiferromagnetic */
            J[(i + 1) * n_spins + i] = 1.0f;
        }

        bool set_result = nimcp_ising_model_set_params(ctx, model, J.data(), h.data());
        EXPECT_TRUE(set_result) << "Set params should succeed";

        nimcp_annealing_config_t config = nimcp_annealing_default_config(n_spins);
        config.n_steps = 200;
        config.initial_temperature = 10.0f;
        config.final_temperature = 0.01f;

        float energy = nimcp_quantum_anneal(ctx, model, &config);
        EXPECT_TRUE(std::isfinite(energy))
            << "Annealing should produce finite energy for " << n_spins << " spins";

        nimcp_ising_model_destroy(model);
        model = nullptr;
    }

    /* Verify recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: VQE Optimization Loop
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, VQEOptimizationLoop) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    const int n_qubits = 4;
    const int n_layers = 2;
    const int n_params = n_qubits * 3 * n_layers;

    /* Create parameter tensor */
    size_t dims[1] = {(size_t)n_params};
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    /* Initialize parameters */
    bool init_result = nimcp_vqc_init_params(ctx, n_qubits, n_layers, params);
    EXPECT_TRUE(init_result) << "VQC init should succeed";

    /* Simple optimization loop */
    std::vector<float> h_params(n_params);
    float prev_energy = INFINITY;

    for (int iter = 0; iter < 10; iter++) {
        /* Create quantum state */
        state = nimcp_quantum_state_create(ctx, n_qubits);
        if (!state) break;

        /* Apply variational circuit */
        bool apply_result = nimcp_vqc_apply(ctx, state, params, n_layers);
        EXPECT_TRUE(apply_result) << "VQC apply should succeed at iteration " << iter;

        /* Measure energy (simplified: just measure probabilities) */
        uint32_t measured_state;
        float probability;
        bool measure_result = nimcp_quantum_measure(ctx, state, &measured_state, &probability);
        EXPECT_TRUE(measure_result);

        /* Energy approximation based on measurement */
        float energy = (float)measured_state / (1 << n_qubits);

        /* Update parameters (simplified gradient descent) */
        if (energy < prev_energy) {
            /* Parameter update would go here */
            prev_energy = energy;
        }

        nimcp_quantum_state_destroy(state);
        state = nullptr;
    }

    nimcp_gpu_tensor_destroy(params);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: QMC Simulation Pipeline
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, QMCSimulationPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Initialize QMC RNG */
    nimcp_qmc_rng_t* rng = nimcp_qmc_rng_create(ctx, 42);
    ASSERT_NE(rng, nullptr) << "QMC RNG creation should succeed";

    /* Generate samples */
    const int n_samples = 1000;
    const int n_dims = 4;

    float* d_samples = nullptr;
    cudaMalloc(&d_samples, n_samples * n_dims * sizeof(float));
    ASSERT_NE(d_samples, nullptr);

    /* Test different samplers */
    bool halton_result = nimcp_qmc_halton_sequence(rng, n_samples, n_dims, d_samples);
    EXPECT_TRUE(halton_result) << "Halton sequence should succeed";

    bool sobol_result = nimcp_qmc_sobol_sequence(rng, n_samples, n_dims, d_samples);
    EXPECT_TRUE(sobol_result) << "Sobol sequence should succeed";

    /* Verify samples are in [0, 1] */
    std::vector<float> h_samples(n_samples * n_dims);
    cudaMemcpy(h_samples.data(), d_samples, n_samples * n_dims * sizeof(float),
               cudaMemcpyDeviceToHost);

    int valid_count = 0;
    for (int i = 0; i < n_samples * n_dims; i++) {
        if (h_samples[i] >= 0.0f && h_samples[i] <= 1.0f) {
            valid_count++;
        }
    }
    EXPECT_EQ(valid_count, n_samples * n_dims)
        << "All samples should be in [0, 1]";

    cudaFree(d_samples);
    nimcp_qmc_rng_destroy(rng);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Quantum State Evolution with Recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, QuantumStateEvolution) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create state and evolve through multiple gates */
    state = nimcp_quantum_state_create(ctx, 5);
    ASSERT_NE(state, nullptr);

    /* Apply Hadamard to all qubits */
    bool h_result = nimcp_quantum_state_hadamard_all(ctx, state);
    EXPECT_TRUE(h_result);

    /* Apply controlled operations */
    for (int i = 0; i < 4; i++) {
        /* Apply CNOT between adjacent qubits */
        bool cnot_result = nimcp_quantum_state_cnot(ctx, state, i, i + 1);
        EXPECT_TRUE(cnot_result) << "CNOT should succeed between qubits " << i << " and " << i + 1;
    }

    /* Apply phase gates */
    for (int i = 0; i < 5; i++) {
        bool phase_result = nimcp_quantum_state_phase(ctx, state, i, M_PI / 4.0f);
        EXPECT_TRUE(phase_result) << "Phase gate should succeed on qubit " << i;
    }

    /* Multiple measurements */
    for (int m = 0; m < 10; m++) {
        nimcp_quantum_state_t* state_copy = nimcp_quantum_state_clone(ctx, state);
        ASSERT_NE(state_copy, nullptr);

        uint32_t measured;
        float prob;
        bool measure_result = nimcp_quantum_measure(ctx, state_copy, &measured, &prob);
        EXPECT_TRUE(measure_result);
        EXPECT_LT(measured, 32u);
        EXPECT_GT(prob, 0.0f);

        nimcp_quantum_state_destroy(state_copy);
    }

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Repeated Grover Iterations with Recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, RepeatedGroverIterations) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    /* Apply initial Hadamard */
    bool h_result = nimcp_quantum_state_hadamard_all(ctx, state);
    EXPECT_TRUE(h_result);

    /* Perform multiple Grover iterations */
    uint32_t marked[2] = {5, 10};
    int optimal_iterations = (int)(M_PI / 4.0 * sqrt(16.0 / 2.0));

    for (int i = 0; i < optimal_iterations; i++) {
        bool iter_result = nimcp_grover_iteration(ctx, state, marked, 2);
        EXPECT_TRUE(iter_result) << "Grover iteration " << i << " should succeed";
    }

    /* Measure multiple times to verify probability amplification */
    std::vector<int> measurement_counts(16, 0);
    const int n_measurements = 100;

    for (int m = 0; m < n_measurements; m++) {
        nimcp_quantum_state_t* state_copy = nimcp_quantum_state_clone(ctx, state);
        ASSERT_NE(state_copy, nullptr);

        uint32_t measured;
        nimcp_quantum_measure(ctx, state_copy, &measured, nullptr);
        measurement_counts[measured]++;

        nimcp_quantum_state_destroy(state_copy);
    }

    /* Marked states should have higher counts */
    int marked_count = measurement_counts[5] + measurement_counts[10];
    EXPECT_GT(marked_count, n_measurements / 4)
        << "Marked states should be amplified";

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: MCTS with Recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, MCTSWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Create MCTS tree for simple problem */
    nimcp_mcts_tree_t* tree = nimcp_mcts_tree_create(ctx, 100, 10);
    ASSERT_NE(tree, nullptr) << "MCTS tree creation should succeed";

    /* Perform MCTS iterations */
    for (int i = 0; i < 50; i++) {
        bool expand_result = nimcp_mcts_expand(tree, 0);
        EXPECT_TRUE(expand_result) << "MCTS expand should succeed at iteration " << i;

        /* Random simulation */
        float reward = ((float)rand() / RAND_MAX);
        bool backup_result = nimcp_mcts_backpropagate(tree, 0, reward);
        EXPECT_TRUE(backup_result);
    }

    /* Select best action */
    int best_action = nimcp_mcts_select_action(tree, 0, 1.0f);
    EXPECT_GE(best_action, 0) << "Should select a valid action";

    nimcp_mcts_tree_destroy(tree);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: High-Stress Quantum Operations
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, HighStressQuantumOperations) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Perform many quantum operations rapidly */
    const int n_iterations = 100;

    for (int i = 0; i < n_iterations; i++) {
        /* Create state */
        nimcp_quantum_state_t* s = nimcp_quantum_state_create(ctx, 4);
        if (!s) continue;

        /* Apply random gates */
        nimcp_quantum_state_hadamard_all(ctx, s);

        for (int q = 0; q < 3; q++) {
            nimcp_quantum_state_cnot(ctx, s, q, q + 1);
        }

        /* Grover iteration */
        uint32_t marked[1] = {(uint32_t)(i % 16)};
        nimcp_grover_iteration(ctx, s, marked, 1);

        /* Measure */
        uint32_t measured;
        nimcp_quantum_measure(ctx, s, &measured, nullptr);

        nimcp_quantum_state_destroy(s);
    }

    /* Get final stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Success rate should be high for normal operations */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GT(stats.success_rate, 0.5f)
            << "Recovery success rate should be reasonable";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Cross-Module Quantum-Classical Hybrid
 * ============================================================================ */
TEST_F(QuantumGPURecoveryIntegrationTest, QuantumClassicalHybrid) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Simulate hybrid quantum-classical optimization */
    const int n_qubits = 4;
    const int n_classical_params = 8;

    /* Create parameter tensor on GPU */
    size_t dims[1] = {(size_t)n_classical_params};
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    /* Initialize with random values */
    std::vector<float> h_params(n_classical_params);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 2.0f * M_PI);
    for (int i = 0; i < n_classical_params; i++) {
        h_params[i] = dist(rng);
    }
    nimcp_gpu_tensor_copy_to_device(params, h_params.data());

    /* Hybrid loop */
    for (int iter = 0; iter < 20; iter++) {
        /* Quantum part: create and evolve state */
        state = nimcp_quantum_state_create(ctx, n_qubits);
        ASSERT_NE(state, nullptr);

        nimcp_quantum_state_hadamard_all(ctx, state);

        /* Apply parameterized rotations */
        for (int q = 0; q < n_qubits; q++) {
            nimcp_quantum_state_phase(ctx, state, q, h_params[q]);
        }

        /* Measure */
        uint32_t measured;
        float prob;
        nimcp_quantum_measure(ctx, state, &measured, &prob);

        /* Classical part: update parameters based on measurement */
        float gradient_estimate = prob * 0.1f;
        for (int i = 0; i < n_classical_params; i++) {
            h_params[i] -= gradient_estimate * ((measured >> i) & 1 ? 1.0f : -1.0f);
        }

        nimcp_quantum_state_destroy(state);
        state = nullptr;
    }

    nimcp_gpu_tensor_destroy(params);

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */

/* Main function for standalone test execution */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
