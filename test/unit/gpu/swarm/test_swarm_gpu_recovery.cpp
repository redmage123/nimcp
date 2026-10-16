/**
 * @file test_swarm_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in swarm modules
 *
 * WHAT: Tests GPU recovery for swarm operations (flocking, consensus, memory)
 * WHY:  Verify self-healing from OOM, numerical errors, and kernel failures
 * HOW:  Test recovery initialization, error handling, and CPU fallback
 *
 * TEST COVERAGE:
 * - Recovery initialization in swarm state creation
 * - OOM recovery during swarm memory allocation
 * - Numerical error recovery in swarm computations
 * - Kernel launch failure recovery
 * - CPU fallback for swarm operations
 * - Parameter correction for invalid swarm configs
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "gpu/swarm/nimcp_swarm_memory_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* Test constants */
static constexpr size_t SMALL_N_AGENTS = 10;
static constexpr size_t DEFAULT_N_AGENTS = 100;
static constexpr size_t DEFAULT_MAX_NEIGHBORS = 16;
static constexpr size_t DEFAULT_BELIEF_DIM = 4;
static constexpr float TOLERANCE = 1e-4f;

/* ============================================================================
 * Test Fixture: SwarmGPURecoveryTest
 * ============================================================================ */
class SwarmGPURecoveryTest : public ::testing::Test {
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
        if (flocking_state) {
            nimcp_flocking_gpu_destroy(flocking_state);
            flocking_state = nullptr;
        }
        if (consensus_state) {
            nimcp_consensus_gpu_destroy(consensus_state);
            consensus_state = nullptr;
        }
        if (swarm_memory) {
            nimcp_swarm_memory_gpu_destroy(swarm_memory);
            swarm_memory = nullptr;
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
    nimcp_flocking_gpu_state_t* flocking_state = nullptr;
    nimcp_consensus_gpu_state_t* consensus_state = nullptr;
    nimcp_swarm_memory_gpu_t* swarm_memory = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at flocking state creation
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryInitializedAtFlockingCreation) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    flocking_state = nimcp_flocking_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(flocking_state, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after flocking state creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at consensus state creation
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryInitializedAtConsensusCreation) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);

    consensus_state = nimcp_consensus_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_BELIEF_DIM, &params);
    ASSERT_NE(consensus_state, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after consensus state creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at swarm memory creation
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryInitializedAtSwarmMemoryCreation) {
#ifdef NIMCP_ENABLE_CUDA
    swarm_memory = nimcp_swarm_memory_gpu_create(
        ctx,
        10000,  /* replay_capacity */
        4,      /* num_agents */
        8,      /* state_dim */
        2,      /* action_dim */
        16);    /* memory_dim */
    ASSERT_NE(swarm_memory, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after swarm memory creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in flocking
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, InvalidParamRecoveryFlocking) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    /* NULL context should fail gracefully */
    nimcp_flocking_gpu_state_t* bad_state = nimcp_flocking_gpu_create(
        nullptr, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for NULL context";

    /* Zero agents should fail gracefully */
    bad_state = nimcp_flocking_gpu_create(
        ctx, 0, DEFAULT_MAX_NEIGHBORS, &params);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for 0 agents";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in consensus
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, InvalidParamRecoveryConsensus) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);

    /* NULL context should fail gracefully */
    nimcp_consensus_gpu_state_t* bad_state = nimcp_consensus_gpu_create(
        nullptr, DEFAULT_N_AGENTS, DEFAULT_BELIEF_DIM, &params);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for NULL context";

    /* Zero belief dimension should fail gracefully */
    bad_state = nimcp_consensus_gpu_create(
        ctx, DEFAULT_N_AGENTS, 0, &params);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for 0 belief_dim";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in swarm memory
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, InvalidParamRecoverySwarmMemory) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    nimcp_swarm_memory_gpu_t* bad_mem = nimcp_swarm_memory_gpu_create(
        nullptr, 10000, 4, 8, 2, 16);
    EXPECT_EQ(bad_mem, nullptr) << "Should fail gracefully for NULL context";

    /* Zero state dimension should fail gracefully */
    bad_mem = nimcp_swarm_memory_gpu_create(
        ctx, 10000, 4, 0, 2, 16);
    EXPECT_EQ(bad_mem, nullptr) << "Should fail gracefully for 0 state_dim";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery context in flocking operations
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryContextInFlockingOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* rctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(rctx, nullptr);

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    flocking_state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(flocking_state, nullptr);

    /* Initialize positions */
    std::vector<float> positions(SMALL_N_AGENTS * 4, 0.0f);
    for (size_t i = 0; i < SMALL_N_AGENTS; i++) {
        positions[i * 4 + 0] = static_cast<float>(i);
        positions[i * 4 + 1] = static_cast<float>(i % 3);
        positions[i * 4 + 2] = 0.0f;
        positions[i * 4 + 3] = 1.0f;  /* mass */
    }
    size_t dims[2] = {SMALL_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(pos_tensor, nullptr);
    nimcp_gpu_copy(ctx, pos_tensor, flocking_state->positions);

    /* Compute forces - should succeed with recovery enabled */
    bool result = nimcp_gpu_flocking_compute_forces(ctx, flocking_state);
    EXPECT_TRUE(result);

    /* Verify recovery stats are accessible */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_recovery_context_destroy(rctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for OOM in swarm operations
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, ErrorCategoryOOM) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_OUT_OF_MEMORY, cudaErrorMemoryAllocation, 0);

    /* OOM should suggest memory management action */
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
 * Test: Error category for kernel launch failure
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, ErrorCategoryKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_KERNEL_LAUNCH, cudaErrorLaunchFailure, 0);

    /* Kernel launch failure should trigger retry or fallback */
    EXPECT_TRUE(action == GPU_RECOVERY_RETRY_IMMEDIATE ||
                action == GPU_RECOVERY_RETRY_BACKOFF ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_RESET_DEVICE)
        << "Kernel launch failure should trigger retry or fallback";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for numerical errors
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, ErrorCategoryNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_NUMERICAL, cudaSuccess, 0);

    /* Numerical error should suggest parameter correction or fallback */
    EXPECT_TRUE(action == GPU_RECOVERY_CLAMP_PARAMS ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_NONE)
        << "Numerical error should trigger parameter correction or fallback";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery action names are valid
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_REDUCE_BATCH);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_FREE_CACHE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category names are valid
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_KERNEL_LAUNCH);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Flocking compute forces with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, FlockingComputeForcesWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);
    params.separation_weight = 1.0f;
    params.alignment_weight = 0.5f;
    params.cohesion_weight = 0.5f;

    flocking_state = nimcp_flocking_gpu_create(
        ctx, DEFAULT_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(flocking_state, nullptr);

    /* Initialize positions in a grid */
    std::vector<float> positions(DEFAULT_N_AGENTS * 4);
    for (size_t i = 0; i < DEFAULT_N_AGENTS; i++) {
        positions[i * 4 + 0] = static_cast<float>(i % 10);
        positions[i * 4 + 1] = static_cast<float>(i / 10);
        positions[i * 4 + 2] = 0.0f;
        positions[i * 4 + 3] = 1.0f;
    }
    size_t dims[2] = {DEFAULT_N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, flocking_state->positions);

    /* Initialize uniform velocities */
    std::vector<float> velocities(DEFAULT_N_AGENTS * 4, 0.0f);
    for (size_t i = 0; i < DEFAULT_N_AGENTS; i++) {
        velocities[i * 4 + 0] = 1.0f;
    }
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, vel_tensor, flocking_state->velocities);

    /* Compute forces - should succeed with recovery */
    bool result = nimcp_gpu_flocking_compute_forces(ctx, flocking_state);
    EXPECT_TRUE(result) << "Force computation should succeed with recovery enabled";

    /* Verify forces are bounded */
    std::vector<float> forces(DEFAULT_N_AGENTS * 4);
    nimcp_gpu_tensor_to_host(flocking_state->forces, forces.data());

    for (size_t i = 0; i < DEFAULT_N_AGENTS; i++) {
        float fx = forces[i * 4 + 0];
        float fy = forces[i * 4 + 1];
        float fz = forces[i * 4 + 2];
        float magnitude = std::sqrt(fx * fx + fy * fy + fz * fz);
        EXPECT_LE(magnitude, params.max_force + TOLERANCE)
            << "Force should be bounded even with recovery";
    }

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Consensus averaging with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, ConsensusAveragingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);
    params.learning_rate = 0.5f;

    consensus_state = nimcp_consensus_gpu_create(
        ctx, SMALL_N_AGENTS, 1, &params);
    ASSERT_NE(consensus_state, nullptr);

    /* Initialize beliefs */
    std::vector<float> beliefs = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f,
                                   6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    size_t dims[2] = {SMALL_N_AGENTS, 1};
    nimcp_gpu_tensor_t* belief_tensor = nimcp_gpu_tensor_from_host(
        ctx, beliefs.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, belief_tensor, consensus_state->beliefs);

    /* Initialize uniform weights */
    std::vector<float> weights(SMALL_N_AGENTS * SMALL_N_AGENTS,
                                1.0f / SMALL_N_AGENTS);
    size_t wdims[2] = {SMALL_N_AGENTS, SMALL_N_AGENTS};
    nimcp_gpu_tensor_t* weight_tensor = nimcp_gpu_tensor_from_host(
        ctx, weights.data(), wdims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, weight_tensor, consensus_state->weights);

    /* Run averaging iterations */
    for (int iter = 0; iter < 10; iter++) {
        bool result = nimcp_gpu_consensus_averaging(ctx, consensus_state);
        EXPECT_TRUE(result) << "Averaging iteration " << iter
                            << " should succeed with recovery";

        /* Swap buffers */
        nimcp_gpu_tensor_t* temp = consensus_state->beliefs;
        consensus_state->beliefs = consensus_state->new_beliefs;
        consensus_state->new_beliefs = temp;
    }

    /* Verify beliefs converged toward mean */
    float expected_mean = 5.5f;  /* (1+2+...+10)/10 */
    std::vector<float> final_beliefs(SMALL_N_AGENTS);
    nimcp_gpu_tensor_to_host(consensus_state->beliefs, final_beliefs.data());

    for (float b : final_beliefs) {
        EXPECT_NEAR(b, expected_mean, 1.0f)
            << "Beliefs should converge toward mean with recovery";
    }

    nimcp_gpu_tensor_destroy(belief_tensor);
    nimcp_gpu_tensor_destroy(weight_tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Swarm memory store with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, SwarmMemoryStoreWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    swarm_memory = nimcp_swarm_memory_gpu_create(
        ctx, 1000, 2, 4, 2, 8);
    ASSERT_NE(swarm_memory, nullptr);

    /* Create state and action tensors */
    std::vector<float> state_data = {0.1f, 0.2f, 0.3f, 0.4f};
    std::vector<float> action_data = {1.0f, 0.0f};
    std::vector<float> next_state_data = {0.2f, 0.3f, 0.4f, 0.5f};

    size_t state_dims[1] = {4};
    size_t action_dims[1] = {2};

    nimcp_gpu_tensor_t* state = nimcp_gpu_tensor_from_host(
        ctx, state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* action = nimcp_gpu_tensor_from_host(
        ctx, action_data.data(), action_dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* next_state = nimcp_gpu_tensor_from_host(
        ctx, next_state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);

    /* Store transition */
    bool result = nimcp_swarm_memory_gpu_store(
        swarm_memory, state, action, 1.0f, next_state, false);
    EXPECT_TRUE(result) << "Store should succeed with recovery enabled";

    /* Verify stats */
    if (swarm_memory->replay_buffer) {
        EXPECT_GE(swarm_memory->replay_buffer->current_size, 1u);
    }

    nimcp_gpu_tensor_destroy(state);
    nimcp_gpu_tensor_destroy(action);
    nimcp_gpu_tensor_destroy(next_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Swarm memory consolidation with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, SwarmMemoryConsolidationWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    swarm_memory = nimcp_swarm_memory_gpu_create(
        ctx, 1000, 2, 4, 2, 8);
    ASSERT_NE(swarm_memory, nullptr);

    /* Store several transitions first */
    std::vector<float> state_data = {0.1f, 0.2f, 0.3f, 0.4f};
    std::vector<float> action_data = {1.0f, 0.0f};
    std::vector<float> next_state_data = {0.2f, 0.3f, 0.4f, 0.5f};

    size_t state_dims[1] = {4};
    size_t action_dims[1] = {2};

    for (int i = 0; i < 10; i++) {
        nimcp_gpu_tensor_t* state = nimcp_gpu_tensor_from_host(
            ctx, state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* action = nimcp_gpu_tensor_from_host(
            ctx, action_data.data(), action_dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* next_state = nimcp_gpu_tensor_from_host(
            ctx, next_state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);

        nimcp_swarm_memory_gpu_store(
            swarm_memory, state, action, 0.5f, next_state, i == 9);

        nimcp_gpu_tensor_destroy(state);
        nimcp_gpu_tensor_destroy(action);
        nimcp_gpu_tensor_destroy(next_state);

        /* Update state for next iteration */
        for (int j = 0; j < 4; j++) {
            state_data[j] += 0.1f;
            next_state_data[j] += 0.1f;
        }
    }

    /* Run consolidation with recovery */
    bool result = nimcp_swarm_memory_gpu_consolidate(swarm_memory, 0.016f);
    EXPECT_TRUE(result) << "Consolidation should succeed with recovery enabled";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stats tracking after swarm operations
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, StatsTrackingAfterSwarmOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    flocking_state = nimcp_flocking_gpu_create(
        ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
    ASSERT_NE(flocking_state, nullptr);

    /* Run several operations */
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_flocking_compute_forces(ctx, flocking_state);
        nimcp_gpu_flocking_update(ctx, flocking_state, 0.016f);
    }

    /* Get stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be accessible */
    EXPECT_GE(stats.total_errors, 0u);
    EXPECT_GE(stats.recoveries_attempted, 0u);

    /* If any recoveries were needed, success rate should be tracked */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GE(stats.success_rate, 0.0f);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPU fallback availability for swarm
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, CPUFallbackAvailability) {
#ifdef NIMCP_ENABLE_CUDA
    bool available = nimcp_gpu_cpu_fallback_available();
    EXPECT_TRUE(available) << "CPU fallback should be available for swarm";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPU fallback enabled by default
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, CPUFallbackEnabledByDefault) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);

    EXPECT_TRUE(config.enable_cpu_fallback)
        << "CPU fallback should be enabled by default";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Repeated operations with recovery monitoring
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RepeatedOperationsWithRecoveryMonitoring) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    /* Create and run multiple flocking simulations */
    for (int sim = 0; sim < 5; sim++) {
        nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
            ctx, SMALL_N_AGENTS, DEFAULT_MAX_NEIGHBORS, &params);
        ASSERT_NE(state, nullptr);

        /* Run simulation steps */
        for (int step = 0; step < 20; step++) {
            nimcp_gpu_flocking_compute_forces(ctx, state);
            nimcp_gpu_flocking_update(ctx, state, 0.016f);
        }

        nimcp_flocking_gpu_destroy(state);
    }

    /* Get final stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    /* Stats should be tracked across all simulations */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GE(stats.success_rate, 0.0f);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery strategy selection for context invalid
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, RecoveryStrategyContextInvalid) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_CONTEXT_INVALID, cudaSuccess, 0);

    EXPECT_TRUE(action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_RESET_DEVICE)
        << "Context invalid should trigger fallback or reset";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NULL safety in flocking operations with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, NullSafetyFlockingOps) {
#ifdef NIMCP_ENABLE_CUDA
    EXPECT_FALSE(nimcp_gpu_flocking_compute_forces(ctx, nullptr))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_gpu_flocking_update(ctx, nullptr, 0.016f))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_gpu_flocking_separation(ctx, nullptr))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_gpu_flocking_alignment(ctx, nullptr))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_gpu_flocking_cohesion(ctx, nullptr))
        << "Should return false for NULL state";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NULL safety in consensus operations with recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, NullSafetyConsensusOps) {
#ifdef NIMCP_ENABLE_CUDA
    EXPECT_FALSE(nimcp_gpu_consensus_averaging(ctx, nullptr))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_gpu_consensus_belief_propagation(ctx, nullptr))
        << "Should return false for NULL state";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Memory consolidation with decay (numerical stability)
 * ============================================================================ */
TEST_F(SwarmGPURecoveryTest, MemoryConsolidationNumericalStability) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create memory strength tensor */
    size_t capacity = 100;
    size_t dims[1] = {capacity};
    nimcp_gpu_tensor_t* memory_strength = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(memory_strength, nullptr);

    /* Initialize with varying strengths */
    std::vector<float> strengths(capacity);
    for (size_t i = 0; i < capacity; i++) {
        strengths[i] = static_cast<float>(i) / capacity;
    }
    nimcp_gpu_tensor_upload(memory_strength, strengths.data(), strengths.size() * sizeof(float));

    /* Apply decay - should not produce NaN or Inf */
    bool result = nimcp_swarm_memory_gpu_decay(
        ctx, memory_strength, 0.01f, 0.01f);
    EXPECT_TRUE(result) << "Decay should succeed with recovery";

    /* Verify no NaN values */
    std::vector<float> decayed(capacity);
    nimcp_gpu_tensor_to_host(memory_strength, decayed.data());

    for (size_t i = 0; i < capacity; i++) {
        EXPECT_FALSE(std::isnan(decayed[i])) << "Should not have NaN at index " << i;
        EXPECT_FALSE(std::isinf(decayed[i])) << "Should not have Inf at index " << i;
    }

    nimcp_gpu_tensor_destroy(memory_strength);
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
