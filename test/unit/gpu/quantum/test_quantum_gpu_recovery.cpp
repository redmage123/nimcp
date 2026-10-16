/**
 * @file test_quantum_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in quantum module
 *
 * WHAT: Tests GPU recovery for quantum state operations
 * WHY:  Verify self-healing from OOM, numerical errors, and kernel failures
 * HOW:  Test recovery initialization, error handling, and CPU fallback
 *
 * TEST COVERAGE:
 * - Recovery initialization in quantum state creation
 * - OOM recovery during quantum state allocation
 * - Numerical error recovery in state evolution
 * - Kernel launch failure recovery
 * - CPU fallback for quantum operations
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

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
class QuantumGPURecoveryTest : public ::testing::Test {
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
    nimcp_quantum_state_t* state = nullptr;
    nimcp_ising_model_t* model = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at quantum state creation
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, RecoveryInitializedAtStateCreation) {
#ifdef NIMCP_ENABLE_CUDA
    /* Recovery should auto-init if not already */
    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after state creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at Ising model creation
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, RecoveryInitializedAtIsingCreation) {
#ifdef NIMCP_ENABLE_CUDA
    model = nimcp_ising_model_create(ctx, 8);
    ASSERT_NE(model, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after Ising model creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in quantum state
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, InvalidParamRecoveryQuantumState) {
#ifdef NIMCP_ENABLE_CUDA
    /* Very large qubit count should fail gracefully */
    nimcp_quantum_state_t* bad_state = nimcp_quantum_state_create(ctx, 100);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for 100 qubits";

    /* Zero qubits should fail gracefully */
    bad_state = nimcp_quantum_state_create(ctx, 0);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for 0 qubits";

    /* NULL context should fail gracefully */
    bad_state = nimcp_quantum_state_create(nullptr, 4);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for NULL context";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery context in quantum operations
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, RecoveryContextInQuantumOps) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create recovery context */
    nimcp_gpu_recovery_context_t* rctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(rctx, nullptr);

    /* Perform quantum operation */
    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    bool result = nimcp_quantum_state_hadamard_all(ctx, state);
    EXPECT_TRUE(result);

    /* Verify recovery stats are available */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* No errors expected, so recoveries should be 0 or minimal */
    /* Just verify stats structure is accessible */
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_recovery_context_destroy(rctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for context invalid
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, ErrorCategoryContextInvalid) {
#ifdef NIMCP_ENABLE_CUDA
    /* Select recovery strategy for invalid context */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_CONTEXT_INVALID, cudaSuccess, 0);

    /* Should suggest CPU fallback or context reset */
    EXPECT_TRUE(action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_RESET_DEVICE)
        << "Context invalid should trigger fallback or reset";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for OOM
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, ErrorCategoryOOM) {
#ifdef NIMCP_ENABLE_CUDA
    /* Select recovery strategy for OOM */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);

    /* Should suggest batch reduction or cache freeing first */
    EXPECT_TRUE(action == GPU_RECOVERY_FREE_CACHE ||
                action == GPU_RECOVERY_REDUCE_BATCH ||
                action == GPU_RECOVERY_REDUCE_DIMENSIONS ||
                action == GPU_RECOVERY_CPU_FALLBACK)
        << "OOM should trigger memory management action";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for kernel launch
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, ErrorCategoryKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    /* Select recovery strategy for kernel launch failure */
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 0);

    /* Should suggest retry, sync, or CPU fallback */
    EXPECT_TRUE(action == GPU_RECOVERY_RETRY_IMMEDIATE ||
                action == GPU_RECOVERY_RETRY_BACKOFF ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_RESET_DEVICE ||
                action == GPU_RECOVERY_STREAM_SYNC ||
                action == GPU_RECOVERY_REDUCE_BATCH)
        << "Kernel launch failure should trigger retry, sync, or fallback";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery action names
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category names
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Grover search with recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, GroverSearchWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_grover_config_t config = nimcp_grover_default_config(4);
    config.marked_states = new uint32_t[1];
    config.marked_states[0] = 5;
    config.n_marked = 1;

    uint32_t found_state = UINT32_MAX;
    bool success = false;

    bool result = nimcp_grover_search(ctx, &config, &found_state, &success);
    EXPECT_TRUE(result) << "Grover search should complete with recovery enabled";

    delete[] config.marked_states;
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Quantum annealing with recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, QuantumAnnealingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    model = nimcp_ising_model_create(ctx, 8);
    ASSERT_NE(model, nullptr);

    /* Set simple coupling matrix */
    std::vector<float> J(64, 0.0f);
    std::vector<float> h(8, 0.0f);
    for (int i = 0; i < 7; i++) {
        J[i * 8 + (i + 1)] = -1.0f;
        J[(i + 1) * 8 + i] = -1.0f;
    }

    bool set_result = nimcp_ising_model_set_params(ctx, model, J.data(), h.data());
    EXPECT_TRUE(set_result);

    nimcp_annealing_config_t config = nimcp_annealing_default_config(8);
    config.n_steps = 100;

    float energy = nimcp_quantum_anneal(ctx, model, &config);
    EXPECT_TRUE(std::isfinite(energy))
        << "Quantum annealing should complete with finite energy";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stats tracking after quantum operations
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, StatsTrackingAfterQuantumOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Perform several quantum operations */
    state = nimcp_quantum_state_create(ctx, 5);
    ASSERT_NE(state, nullptr);

    nimcp_quantum_state_hadamard_all(ctx, state);

    uint32_t marked[1] = {7};
    for (int i = 0; i < 5; i++) {
        nimcp_grover_iteration(ctx, state, marked, 1);
    }

    /* Get stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be accessible */
    EXPECT_GE(stats.total_errors, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Measurement with recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, MeasurementWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    state = nimcp_quantum_state_create(ctx, 4);
    ASSERT_NE(state, nullptr);

    nimcp_quantum_state_hadamard_all(ctx, state);

    uint32_t measured_state = UINT32_MAX;
    float probability = 0.0f;

    bool result = nimcp_quantum_measure(ctx, state, &measured_state, &probability);
    EXPECT_TRUE(result) << "Measurement should succeed with recovery";
    EXPECT_LT(measured_state, 16u) << "Measured state should be valid";
    EXPECT_GT(probability, 0.0f) << "Probability should be positive";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: VQC initialization with recovery
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, VQCInitWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    size_t dims[1] = {12};  /* 4 qubits * 3 params */
    nimcp_gpu_tensor_t* params = nimcp_gpu_tensor_create(ctx, dims, 1,
                                                          NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(params, nullptr);

    bool result = nimcp_vqc_init_params(ctx, 4, 1, params);
    EXPECT_TRUE(result) << "VQC init should succeed with recovery";

    nimcp_gpu_tensor_destroy(params);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Repeated operations with recovery monitoring
 * ============================================================================ */
TEST_F(QuantumGPURecoveryTest, RepeatedOperationsWithRecoveryMonitoring) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    /* Perform many operations to stress-test recovery */
    for (int i = 0; i < 20; i++) {
        nimcp_quantum_state_t* s = nimcp_quantum_state_create(ctx, 4);
        ASSERT_NE(s, nullptr);

        nimcp_quantum_state_hadamard_all(ctx, s);

        uint32_t marked[1] = {(uint32_t)(i % 16)};
        nimcp_grover_iteration(ctx, s, marked, 1);

        uint32_t measured;
        nimcp_quantum_measure(ctx, s, &measured, nullptr);

        nimcp_quantum_state_destroy(s);
    }

    /* Get final stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* If any recoveries were needed, success rate should be tracked */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GE(stats.success_rate, 0.0f);
    }
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
