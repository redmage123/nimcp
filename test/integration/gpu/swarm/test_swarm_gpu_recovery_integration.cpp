/**
 * @file test_swarm_gpu_recovery_integration.cpp
 * @brief Integration tests for GPU recovery in swarm modules
 *
 * WHAT: Integration tests for GPU recovery across swarm operations
 * WHY:  Verify recovery works correctly across component boundaries
 * HOW:  Test complete swarm workflows with recovery enabled
 *
 * TEST COVERAGE:
 * - Full flocking simulation cycle with recovery
 * - Swarm memory store/sample/consolidate cycle with recovery
 * - Multi-agent consensus convergence with recovery
 * - Recovery across component transitions
 * - Stress testing with repeated operations
 * - Memory pressure scenarios with recovery
 *
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>

#ifdef NIMCP_ENABLE_CUDA
#include <cuda_runtime.h>
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "gpu/swarm/nimcp_swarm_memory_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* Integration test constants */
static constexpr size_t N_AGENTS = 64;
static constexpr size_t MAX_NEIGHBORS = 16;
static constexpr size_t BELIEF_DIM = 8;
static constexpr size_t REPLAY_CAPACITY = 10000;
static constexpr size_t STATE_DIM = 16;
static constexpr size_t ACTION_DIM = 4;
static constexpr size_t MEMORY_DIM = 32;
static constexpr float DT = 0.016f;
static constexpr float TOLERANCE = 1e-3f;

/* ============================================================================
 * Test Fixture: SwarmGPURecoveryIntegrationTest
 * ============================================================================ */
class SwarmGPURecoveryIntegrationTest : public ::testing::Test {
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
        /* Reset stats at start of each test */
        nimcp_gpu_recovery_reset_stats();
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
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

    /* Helper: Create grid positions */
    std::vector<float> create_grid_positions(size_t n, float spacing) {
        std::vector<float> pos(n * 4);
        size_t side = static_cast<size_t>(std::ceil(std::cbrt(static_cast<double>(n))));
        size_t idx = 0;
        for (size_t z = 0; z < side && idx < n; z++) {
            for (size_t y = 0; y < side && idx < n; y++) {
                for (size_t x = 0; x < side && idx < n; x++) {
                    pos[idx * 4 + 0] = x * spacing;
                    pos[idx * 4 + 1] = y * spacing;
                    pos[idx * 4 + 2] = z * spacing;
                    pos[idx * 4 + 3] = 1.0f;
                    idx++;
                }
            }
        }
        return pos;
    }

    /* Helper: Create random velocities */
    std::vector<float> create_random_velocities(size_t n, float scale) {
        std::vector<float> vel(n * 4);
        for (size_t i = 0; i < n; i++) {
            vel[i * 4 + 0] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * scale;
            vel[i * 4 + 1] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * scale;
            vel[i * 4 + 2] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * scale;
            vel[i * 4 + 3] = 0.0f;
        }
        return vel;
    }
#endif
};

/* ============================================================================
 * Integration Test: Complete Flocking Simulation Cycle
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, CompleteFlockingSimulation) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);
    params.separation_weight = 1.5f;
    params.alignment_weight = 1.0f;
    params.cohesion_weight = 1.0f;

    nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
        ctx, N_AGENTS, MAX_NEIGHBORS, &params);
    ASSERT_NE(state, nullptr);

    /* Initialize positions and velocities */
    std::vector<float> positions = create_grid_positions(N_AGENTS, 2.0f);
    std::vector<float> velocities = create_random_velocities(N_AGENTS, 1.0f);

    size_t dims[2] = {N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, state->positions);
    nimcp_gpu_copy(ctx, vel_tensor, state->velocities);

    /* Run simulation for many steps */
    const int total_steps = 200;
    int successful_steps = 0;

    for (int step = 0; step < total_steps; step++) {
        /* Find neighbors */
        bool result = nimcp_gpu_flocking_find_neighbors(ctx, state, params.cohesion_radius);
        if (!result) continue;

        /* Compute forces */
        result = nimcp_gpu_flocking_compute_forces(ctx, state);
        if (!result) continue;

        /* Update kinematics */
        result = nimcp_gpu_flocking_update(ctx, state, DT);
        if (result) successful_steps++;
    }

    /* Most steps should succeed even under stress */
    EXPECT_GE(successful_steps, total_steps * 0.95)
        << "At least 95% of steps should succeed with recovery";

    /* Verify final state is valid */
    std::vector<float> final_positions(N_AGENTS * 4);
    nimcp_gpu_tensor_to_host(state->positions, final_positions.data());

    int valid_positions = 0;
    for (size_t i = 0; i < N_AGENTS; i++) {
        float x = final_positions[i * 4 + 0];
        float y = final_positions[i * 4 + 1];
        float z = final_positions[i * 4 + 2];
        if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
            valid_positions++;
        }
    }
    EXPECT_EQ(valid_positions, N_AGENTS)
        << "All agent positions should be valid after simulation";

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    /* If recoveries were needed, they should have succeeded */
    if (stats.recoveries_attempted > 0) {
        EXPECT_GT(stats.successful_recoveries, 0u)
            << "Some recoveries should succeed";
    }

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Swarm Memory Store/Sample/Consolidate Cycle
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, SwarmMemoryCompleteCycle) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_swarm_memory_gpu_t* memory = nimcp_swarm_memory_gpu_create(
        ctx, REPLAY_CAPACITY, 4, STATE_DIM, ACTION_DIM, MEMORY_DIM);
    ASSERT_NE(memory, nullptr);

    /* Store many transitions */
    const int n_transitions = 500;
    int successful_stores = 0;

    for (int i = 0; i < n_transitions; i++) {
        std::vector<float> state_data(STATE_DIM);
        std::vector<float> action_data(ACTION_DIM);
        std::vector<float> next_state_data(STATE_DIM);

        for (size_t j = 0; j < STATE_DIM; j++) {
            state_data[j] = static_cast<float>(rand()) / RAND_MAX;
            next_state_data[j] = state_data[j] + 0.1f *
                (static_cast<float>(rand()) / RAND_MAX - 0.5f);
        }
        for (size_t j = 0; j < ACTION_DIM; j++) {
            action_data[j] = static_cast<float>(rand()) / RAND_MAX;
        }

        size_t state_dims[1] = {STATE_DIM};
        size_t action_dims[1] = {ACTION_DIM};

        nimcp_gpu_tensor_t* state = nimcp_gpu_tensor_from_host(
            ctx, state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* action = nimcp_gpu_tensor_from_host(
            ctx, action_data.data(), action_dims, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* next_state = nimcp_gpu_tensor_from_host(
            ctx, next_state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);

        float reward = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f;
        bool done = (i % 50 == 49);

        bool result = nimcp_swarm_memory_gpu_store(
            memory, state, action, reward, next_state, done);
        if (result) successful_stores++;

        nimcp_gpu_tensor_destroy(state);
        nimcp_gpu_tensor_destroy(action);
        nimcp_gpu_tensor_destroy(next_state);
    }

    EXPECT_GE(successful_stores, n_transitions * 0.95)
        << "At least 95% of stores should succeed";

    /* Sample batches */
    const int n_samples = 50;
    int successful_samples = 0;

    nimcp_replay_batch_t* batch = nimcp_replay_batch_create(
        ctx, 32, STATE_DIM, ACTION_DIM);
    ASSERT_NE(batch, nullptr);

    for (int i = 0; i < n_samples; i++) {
        bool result = nimcp_swarm_memory_gpu_sample(memory, 32, batch);
        if (result) successful_samples++;
    }

    EXPECT_GE(successful_samples, n_samples * 0.90)
        << "At least 90% of samples should succeed";

    /* Run consolidation */
    int successful_consolidations = 0;
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_swarm_memory_gpu_consolidate(memory, DT);
        if (result) successful_consolidations++;
    }

    EXPECT_GE(successful_consolidations, 8)
        << "At least 80% of consolidations should succeed";

    /* Verify recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.recoveries_attempted > 0 && stats.total_errors > 0) {
        float recovery_rate = static_cast<float>(stats.successful_recoveries) /
                              stats.recoveries_attempted;
        EXPECT_GE(recovery_rate, 0.5f)
            << "Recovery success rate should be at least 50%";
    }

    nimcp_replay_batch_destroy(batch);
    nimcp_swarm_memory_gpu_destroy(memory);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Multi-Agent Consensus Convergence
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, ConsensusConvergence) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_consensus_gpu_params_t params;
    nimcp_consensus_gpu_default_params(&params);
    params.learning_rate = 0.3f;
    params.convergence_threshold = 0.01f;

    nimcp_consensus_gpu_state_t* state = nimcp_consensus_gpu_create(
        ctx, N_AGENTS, BELIEF_DIM, &params);
    ASSERT_NE(state, nullptr);

    /* Initialize beliefs with random values */
    std::vector<float> beliefs(N_AGENTS * BELIEF_DIM);
    for (size_t i = 0; i < beliefs.size(); i++) {
        beliefs[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    size_t dims[2] = {N_AGENTS, BELIEF_DIM};
    nimcp_gpu_tensor_t* belief_tensor = nimcp_gpu_tensor_from_host(
        ctx, beliefs.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, belief_tensor, state->beliefs);

    /* Initialize dense connectivity */
    std::vector<float> weights(N_AGENTS * N_AGENTS, 1.0f / N_AGENTS);
    size_t wdims[2] = {N_AGENTS, N_AGENTS};
    nimcp_gpu_tensor_t* weight_tensor = nimcp_gpu_tensor_from_host(
        ctx, weights.data(), wdims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, weight_tensor, state->weights);

    /* Run consensus iterations */
    const int max_iterations = 100;
    int successful_iterations = 0;
    bool converged = false;
    float final_variance = 1.0f;

    for (int iter = 0; iter < max_iterations && !converged; iter++) {
        bool result = nimcp_gpu_consensus_averaging(ctx, state);
        if (!result) continue;

        successful_iterations++;

        /* Swap buffers */
        nimcp_gpu_tensor_t* temp = state->beliefs;
        state->beliefs = state->new_beliefs;
        state->new_beliefs = temp;

        /* Check convergence every 10 iterations */
        if (iter % 10 == 9) {
            result = nimcp_gpu_consensus_check_convergence(
                ctx, state, &converged, &final_variance);
            if (result && converged) {
                break;
            }
        }
    }

    EXPECT_GE(successful_iterations, max_iterations * 0.90)
        << "At least 90% of iterations should succeed";

    /* Verify beliefs converged to similar values */
    std::vector<float> final_beliefs(N_AGENTS * BELIEF_DIM);
    nimcp_gpu_tensor_to_host(state->beliefs, final_beliefs.data());

    /* Calculate variance across agents for first belief dimension */
    float mean = 0.0f;
    for (size_t i = 0; i < N_AGENTS; i++) {
        mean += final_beliefs[i * BELIEF_DIM];
    }
    mean /= N_AGENTS;

    float variance = 0.0f;
    for (size_t i = 0; i < N_AGENTS; i++) {
        float diff = final_beliefs[i * BELIEF_DIM] - mean;
        variance += diff * diff;
    }
    variance /= N_AGENTS;

    EXPECT_LT(variance, 0.1f)
        << "Beliefs should converge to low variance with recovery";

    nimcp_gpu_tensor_destroy(belief_tensor);
    nimcp_gpu_tensor_destroy(weight_tensor);
    nimcp_consensus_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Multi-Agent Memory Synchronization
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, MultiAgentMemorySync) {
#ifdef NIMCP_ENABLE_CUDA
    const int num_agents = 8;

    nimcp_swarm_memory_gpu_t* memory = nimcp_swarm_memory_gpu_create(
        ctx, 5000, num_agents, STATE_DIM, ACTION_DIM, MEMORY_DIM);
    ASSERT_NE(memory, nullptr);

    /* Store transitions for each agent */
    for (int agent = 0; agent < num_agents; agent++) {
        for (int t = 0; t < 100; t++) {
            std::vector<float> state_data(STATE_DIM);
            std::vector<float> action_data(ACTION_DIM);
            std::vector<float> next_state_data(STATE_DIM);

            for (size_t j = 0; j < STATE_DIM; j++) {
                state_data[j] = static_cast<float>(agent) / num_agents +
                    0.1f * static_cast<float>(rand()) / RAND_MAX;
                next_state_data[j] = state_data[j] + 0.05f;
            }
            for (size_t j = 0; j < ACTION_DIM; j++) {
                action_data[j] = static_cast<float>(rand()) / RAND_MAX;
            }

            size_t state_dims[1] = {STATE_DIM};
            size_t action_dims[1] = {ACTION_DIM};

            nimcp_gpu_tensor_t* state = nimcp_gpu_tensor_from_host(
                ctx, state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);
            nimcp_gpu_tensor_t* action = nimcp_gpu_tensor_from_host(
                ctx, action_data.data(), action_dims, 1, NIMCP_GPU_PRECISION_FP32);
            nimcp_gpu_tensor_t* next_state = nimcp_gpu_tensor_from_host(
                ctx, next_state_data.data(), state_dims, 1, NIMCP_GPU_PRECISION_FP32);

            nimcp_swarm_memory_gpu_store(
                memory, state, action, 0.5f, next_state, t == 99);

            nimcp_gpu_tensor_destroy(state);
            nimcp_gpu_tensor_destroy(action);
            nimcp_gpu_tensor_destroy(next_state);
        }
    }

    /* Synchronize agents */
    int successful_syncs = 0;
    for (int i = 0; i < 20; i++) {
        bool result = nimcp_swarm_memory_gpu_sync_agents(memory);
        if (result) successful_syncs++;
    }

    EXPECT_GE(successful_syncs, 15)
        << "At least 75% of agent syncs should succeed";

    /* Verify recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.recoveries_attempted > 0) {
        EXPECT_GT(stats.success_rate, 0.0f)
            << "Recovery should have some success rate";
    }

    nimcp_swarm_memory_gpu_destroy(memory);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Stress Test with Rapid Operations
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, StressTestRapidOperations) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_flocking_gpu_params_t params;
    nimcp_flocking_gpu_default_params(&params);

    auto start = std::chrono::high_resolution_clock::now();

    const int n_simulations = 20;
    int successful_simulations = 0;

    for (int sim = 0; sim < n_simulations; sim++) {
        nimcp_flocking_gpu_state_t* state = nimcp_flocking_gpu_create(
            ctx, N_AGENTS, MAX_NEIGHBORS, &params);
        if (!state) continue;

        bool sim_success = true;
        for (int step = 0; step < 50 && sim_success; step++) {
            sim_success = nimcp_gpu_flocking_compute_forces(ctx, state);
            if (sim_success) {
                sim_success = nimcp_gpu_flocking_update(ctx, state, DT);
            }
        }

        if (sim_success) successful_simulations++;
        nimcp_flocking_gpu_destroy(state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(successful_simulations, n_simulations * 0.80)
        << "At least 80% of stress test simulations should succeed";

    /* Should complete within reasonable time */
    EXPECT_LT(duration.count(), 30000)
        << "Stress test should complete within 30 seconds";

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.total_errors > 0) {
        EXPECT_GT(stats.successful_recoveries, 0u)
            << "Under stress, recoveries should still succeed";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Pheromone System Complete Cycle
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, PheromoneSystemCycle) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_pheromone_gpu_params_t params;
    nimcp_pheromone_gpu_default_params(&params);
    params.diffusion_rate = 0.05f;
    params.decay_rates[0] = 0.01f;
    params.decay_rates[1] = 0.02f;

    nimcp_pheromone_gpu_state_t* state = nimcp_pheromone_gpu_create(
        ctx, 32, 32, 1, 2, 1.0f, &params);
    ASSERT_NE(state, nullptr);

    /* Initial zero concentration */
    nimcp_gpu_zeros(ctx, state->concentration);

    int successful_deposits = 0;
    int successful_diffusions = 0;
    int successful_decays = 0;

    /* Simulate pheromone cycle */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Deposit pheromones */
        std::vector<float> deposit_pos = {
            static_cast<float>(rand() % 32) + 0.5f,
            static_cast<float>(rand() % 32) + 0.5f,
            0.0f
        };
        std::vector<float> deposit_type = {static_cast<float>(cycle % 2)};
        std::vector<float> deposit_amount = {1.0f};

        size_t dims_pos[2] = {1, 3};
        size_t dims_scalar[1] = {1};
        nimcp_gpu_tensor_t* pos = nimcp_gpu_tensor_from_host(
            ctx, deposit_pos.data(), dims_pos, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* type = nimcp_gpu_tensor_from_host(
            ctx, deposit_type.data(), dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_tensor_t* amount = nimcp_gpu_tensor_from_host(
            ctx, deposit_amount.data(), dims_scalar, 1, NIMCP_GPU_PRECISION_FP32);

        if (nimcp_gpu_pheromone_deposit(ctx, state, pos, type, amount, 1)) {
            successful_deposits++;
        }

        nimcp_gpu_tensor_destroy(pos);
        nimcp_gpu_tensor_destroy(type);
        nimcp_gpu_tensor_destroy(amount);

        /* Apply diffusion */
        if (nimcp_gpu_pheromone_diffusion(ctx, state, DT)) {
            successful_diffusions++;
        }

        /* Apply decay */
        if (nimcp_gpu_pheromone_decay(ctx, state, DT)) {
            successful_decays++;
        }
    }

    EXPECT_GE(successful_deposits, 90)
        << "At least 90% of deposits should succeed";
    EXPECT_GE(successful_diffusions, 90)
        << "At least 90% of diffusions should succeed";
    EXPECT_GE(successful_decays, 90)
        << "At least 90% of decays should succeed";

    /* Verify concentration is valid */
    std::vector<float> concentration(32 * 32 * 2);
    nimcp_gpu_tensor_to_host(state->concentration, concentration.data());

    int valid_values = 0;
    for (float c : concentration) {
        if (std::isfinite(c) && c >= 0.0f) {
            valid_values++;
        }
    }
    EXPECT_EQ(valid_values, 32 * 32 * 2)
        << "All concentration values should be valid";

    nimcp_pheromone_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Task Allocation Auction Cycle
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, TaskAllocationCycle) {
#ifdef NIMCP_ENABLE_CUDA
    const size_t n_agents = 32;
    const size_t n_tasks = 16;
    const size_t n_caps = 4;

    nimcp_task_alloc_gpu_params_t params;
    nimcp_task_alloc_gpu_default_params(&params);
    params.bid_increment = 0.1f;

    nimcp_task_alloc_gpu_state_t* state = nimcp_task_alloc_gpu_create(
        ctx, n_agents, n_tasks, n_caps, &params);
    ASSERT_NE(state, nullptr);

    /* Initialize agent capabilities */
    std::vector<float> capabilities(n_agents * n_caps);
    for (size_t i = 0; i < capabilities.size(); i++) {
        capabilities[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    size_t cap_dims[2] = {n_agents, n_caps};
    nimcp_gpu_tensor_t* cap_tensor = nimcp_gpu_tensor_from_host(
        ctx, capabilities.data(), cap_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, cap_tensor, state->agent_capabilities);

    /* Initialize task requirements */
    std::vector<float> requirements(n_tasks * n_caps);
    for (size_t i = 0; i < requirements.size(); i++) {
        requirements[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    size_t req_dims[2] = {n_tasks, n_caps};
    nimcp_gpu_tensor_t* req_tensor = nimcp_gpu_tensor_from_host(
        ctx, requirements.data(), req_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, req_tensor, state->task_requirements);

    /* Run auction rounds */
    int successful_rounds = 0;
    int successful_price_updates = 0;

    for (int round = 0; round < 50; round++) {
        if (nimcp_gpu_task_auction_round(ctx, state)) {
            successful_rounds++;
        }
        if (nimcp_gpu_task_update_prices(ctx, state)) {
            successful_price_updates++;
        }
    }

    EXPECT_GE(successful_rounds, 45)
        << "At least 90% of auction rounds should succeed";
    EXPECT_GE(successful_price_updates, 45)
        << "At least 90% of price updates should succeed";

    /* Finalize assignments */
    bool result = nimcp_gpu_task_finalize_assignments(ctx, state);
    EXPECT_TRUE(result) << "Assignment finalization should succeed";

    nimcp_gpu_tensor_destroy(cap_tensor);
    nimcp_gpu_tensor_destroy(req_tensor);
    nimcp_task_alloc_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Collision Avoidance with Recovery
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, CollisionAvoidance) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_collision_gpu_params_t params;
    nimcp_collision_gpu_default_params(&params);
    params.collision_radius = 1.0f;

    nimcp_collision_gpu_state_t* state = nimcp_collision_gpu_create(
        ctx, N_AGENTS, N_AGENTS * 10, &params);
    ASSERT_NE(state, nullptr);

    int successful_detections = 0;

    /* Run collision detection multiple times with moving agents */
    for (int frame = 0; frame < 100; frame++) {
        /* Generate positions with some overlaps */
        std::vector<float> positions(N_AGENTS * 4);
        for (size_t i = 0; i < N_AGENTS; i++) {
            positions[i * 4 + 0] = static_cast<float>(i % 8) * 0.5f +
                0.1f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
            positions[i * 4 + 1] = static_cast<float>(i / 8) * 0.5f +
                0.1f * (static_cast<float>(rand()) / RAND_MAX - 0.5f);
            positions[i * 4 + 2] = 0.0f;
            positions[i * 4 + 3] = 1.0f;
        }

        size_t dims[2] = {N_AGENTS, 4};
        nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
            ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
        nimcp_gpu_copy(ctx, pos_tensor, state->positions);

        if (nimcp_gpu_collision_detect(ctx, state)) {
            successful_detections++;
        }

        nimcp_gpu_tensor_destroy(pos_tensor);
    }

    EXPECT_GE(successful_detections, 90)
        << "At least 90% of collision detections should succeed";

    nimcp_collision_gpu_destroy(state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Combined Swarm Systems
 * ============================================================================ */
TEST_F(SwarmGPURecoveryIntegrationTest, CombinedSwarmSystems) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create all subsystems */
    nimcp_flocking_gpu_params_t flock_params;
    nimcp_flocking_gpu_default_params(&flock_params);
    nimcp_flocking_gpu_state_t* flock = nimcp_flocking_gpu_create(
        ctx, N_AGENTS, MAX_NEIGHBORS, &flock_params);
    ASSERT_NE(flock, nullptr);

    nimcp_pheromone_gpu_params_t pher_params;
    nimcp_pheromone_gpu_default_params(&pher_params);
    nimcp_pheromone_gpu_state_t* pher = nimcp_pheromone_gpu_create(
        ctx, 16, 16, 1, 1, 2.0f, &pher_params);
    ASSERT_NE(pher, nullptr);

    nimcp_collision_gpu_params_t coll_params;
    nimcp_collision_gpu_default_params(&coll_params);
    coll_params.collision_radius = 0.5f;
    nimcp_collision_gpu_state_t* coll = nimcp_collision_gpu_create(
        ctx, N_AGENTS, N_AGENTS * 5, &coll_params);
    ASSERT_NE(coll, nullptr);

    /* Initialize flocking */
    std::vector<float> positions = create_grid_positions(N_AGENTS, 1.0f);
    std::vector<float> velocities = create_random_velocities(N_AGENTS, 0.5f);
    size_t dims[2] = {N_AGENTS, 4};
    nimcp_gpu_tensor_t* pos_tensor = nimcp_gpu_tensor_from_host(
        ctx, positions.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* vel_tensor = nimcp_gpu_tensor_from_host(
        ctx, velocities.data(), dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_copy(ctx, pos_tensor, flock->positions);
    nimcp_gpu_copy(ctx, vel_tensor, flock->velocities);

    /* Zero pheromone */
    nimcp_gpu_zeros(ctx, pher->concentration);

    int total_ops = 0;
    int successful_ops = 0;

    /* Run combined simulation */
    for (int step = 0; step < 100; step++) {
        /* Flocking */
        total_ops++;
        if (nimcp_gpu_flocking_compute_forces(ctx, flock)) successful_ops++;
        total_ops++;
        if (nimcp_gpu_flocking_update(ctx, flock, DT)) successful_ops++;

        /* Update collision positions from flocking */
        nimcp_gpu_copy(ctx, flock->positions, coll->positions);

        /* Collision detection */
        total_ops++;
        if (nimcp_gpu_collision_detect(ctx, coll)) successful_ops++;

        /* Pheromone diffusion */
        total_ops++;
        if (nimcp_gpu_pheromone_diffusion(ctx, pher, DT)) successful_ops++;
        total_ops++;
        if (nimcp_gpu_pheromone_decay(ctx, pher, DT)) successful_ops++;
    }

    float success_rate = static_cast<float>(successful_ops) / total_ops;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of combined operations should succeed";

    /* Verify recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.total_errors > 0) {
        EXPECT_GT(stats.successful_recoveries, 0u)
            << "Some recoveries should succeed in combined test";
    }

    nimcp_gpu_tensor_destroy(pos_tensor);
    nimcp_gpu_tensor_destroy(vel_tensor);
    nimcp_flocking_gpu_destroy(flock);
    nimcp_pheromone_gpu_destroy(pher);
    nimcp_collision_gpu_destroy(coll);
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
