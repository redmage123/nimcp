/**
 * @file test_hypothalamus_gpu_recovery_integration.cpp
 * @brief Integration tests for GPU recovery in hypothalamus modules
 *
 * WHAT: Integration tests for GPU recovery across hypothalamus operations
 * WHY:  Verify recovery works correctly across component boundaries
 * HOW:  Test complete hypothalamus workflows with recovery enabled
 *
 * TEST COVERAGE:
 * - Full drive dynamics simulation cycle with recovery
 * - Reward computation pipeline with recovery
 * - PID controller integration with recovery
 * - Multi-batch processing with recovery
 * - Numerical stability under stress
 * - Component interaction testing
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
#include "gpu/hypothalamus/nimcp_hypothalamus_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* Integration test constants */
static constexpr size_t DEFAULT_BATCH = 32;
static constexpr size_t LARGE_BATCH = 128;
static constexpr size_t N_VARIABLES = 8;
static constexpr float DT = 0.01f;
static constexpr float TOLERANCE = 1e-3f;

/* ============================================================================
 * Test Fixture: HypothalamusGPURecoveryIntegrationTest
 * ============================================================================ */
class HypothalamusGPURecoveryIntegrationTest : public ::testing::Test {
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

    /* Helper: Verify tensor has no NaN or Inf values */
    bool verify_tensor_finite(nimcp_gpu_tensor_t* tensor, size_t size) {
        std::vector<float> data(size);
        nimcp_gpu_tensor_to_host(tensor, data.data());
        for (size_t i = 0; i < size; i++) {
            if (!std::isfinite(data[i])) return false;
        }
        return true;
    }
#endif
};

/* ============================================================================
 * Integration Test: Complete Drive Dynamics Simulation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, CompleteDriveDynamicsSimulation) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, false);
    ASSERT_NE(setpoints, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_EULER;

    /* Simulate 10 seconds of drive dynamics */
    const float total_time = 10.0f;
    const int n_steps = static_cast<int>(total_time / DT);
    int successful_steps = 0;

    for (int step = 0; step < n_steps; step++) {
        /* Drive integration */
        bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
        if (!result) continue;

        /* Compute urgency every 10 steps */
        if (step % 10 == 0) {
            float arousal = 0.5f + 0.3f * std::sin(step * 0.01f);
            result = nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, arousal);
            if (!result) continue;
        }

        /* Compute deviation every 20 steps */
        if (step % 20 == 0) {
            result = nimcp_hypo_gpu_compute_deviation(ctx, drive_state, setpoints);
            if (!result) continue;
        }

        successful_steps++;
    }

    float success_rate = static_cast<float>(successful_steps) / n_steps;
    EXPECT_GE(success_rate, 0.95f)
        << "At least 95% of simulation steps should succeed";

    /* Verify state is valid */
    if (drive_state->state) {
        size_t state_size = DEFAULT_BATCH * NIMCP_HYPO_GPU_DRIVE_COUNT * 8;
        EXPECT_TRUE(verify_tensor_finite(drive_state->state, state_size))
            << "Drive state should have no NaN/Inf values";
    }

    /* Check recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.recoveries_attempted > 0) {
        EXPECT_GT(stats.successful_recoveries, 0u)
            << "Some recoveries should succeed";
    }

    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Reward Computation Pipeline
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, RewardComputationPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, true);
    ASSERT_NE(setpoints, nullptr);

    nimcp_hypo_gpu_reward_output_t* reward_output =
        nimcp_hypo_gpu_reward_output_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(reward_output, nullptr);

    nimcp_hypo_gpu_dynamics_config_t dyn_config = nimcp_hypo_gpu_dynamics_config_default();
    nimcp_hypo_gpu_reward_config_t rew_config = nimcp_hypo_gpu_reward_config_default();

    int successful_reward_computations = 0;
    int total_computations = 0;

    /* Run reward pipeline multiple times */
    for (int epoch = 0; epoch < 100; epoch++) {
        /* Integrate drives */
        for (int step = 0; step < 10; step++) {
            nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &dyn_config);
        }

        /* Compute urgency */
        float arousal = 0.5f + 0.2f * (static_cast<float>(rand()) / RAND_MAX);
        nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, arousal);

        /* Test different reward modes */
        rew_config.mode = static_cast<nimcp_hypo_gpu_reward_mode_t>(epoch % 4);

        total_computations++;
        bool result = nimcp_hypo_gpu_compute_reward(
            ctx, drive_state, setpoints, reward_output, &rew_config);
        if (result) successful_reward_computations++;
    }

    float success_rate = static_cast<float>(successful_reward_computations) / total_computations;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of reward computations should succeed";

    /* Verify reward values are valid */
    if (reward_output->reward) {
        EXPECT_TRUE(verify_tensor_finite(reward_output->reward, DEFAULT_BATCH))
            << "Reward values should be finite";
    }

    nimcp_hypo_gpu_reward_output_destroy(reward_output);
    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: PID Controller Long-Term Operation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, PIDControllerLongTerm) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_controller_state_t* controller =
        nimcp_hypo_gpu_controller_create(ctx, DEFAULT_BATCH, N_VARIABLES);
    ASSERT_NE(controller, nullptr);

    nimcp_hypo_gpu_pid_config_t pid_config = nimcp_hypo_gpu_pid_config_default();

    /* Create current and target tensors */
    size_t dims[2] = {DEFAULT_BATCH, N_VARIABLES};
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_create(
        ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target = nimcp_gpu_tensor_create(
        ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(current, nullptr);
    ASSERT_NE(target, nullptr);

    /* Initialize target to oscillating setpoint */
    std::vector<float> target_data(DEFAULT_BATCH * N_VARIABLES);
    for (size_t i = 0; i < target_data.size(); i++) {
        target_data[i] = 1.0f;
    }
    nimcp_gpu_tensor_upload(target, target_data.data(), target_data.size() * sizeof(float));

    int successful_updates = 0;
    const int n_iterations = 500;

    for (int iter = 0; iter < n_iterations; iter++) {
        /* Update target (oscillating) */
        for (size_t i = 0; i < target_data.size(); i++) {
            target_data[i] = 1.0f + 0.5f * std::sin(iter * 0.05f);
        }
        nimcp_gpu_tensor_upload(target, target_data.data(), target_data.size() * sizeof(float));

        /* Controller update */
        bool result = nimcp_hypo_gpu_controller_update(
            ctx, controller, current, target, DT, &pid_config);
        if (!result) continue;

        /* Apply output to current (simple first-order response) */
        if (controller->output) {
            std::vector<float> output_data(DEFAULT_BATCH * N_VARIABLES);
            nimcp_gpu_tensor_to_host(controller->output, output_data.data());

            std::vector<float> current_data(DEFAULT_BATCH * N_VARIABLES);
            nimcp_gpu_tensor_to_host(current, current_data.data());

            for (size_t i = 0; i < current_data.size(); i++) {
                current_data[i] += output_data[i] * DT;
            }
            nimcp_gpu_tensor_upload(current, current_data.data(), current_data.size() * sizeof(float));
        }

        successful_updates++;
    }

    float success_rate = static_cast<float>(successful_updates) / n_iterations;
    EXPECT_GE(success_rate, 0.95f)
        << "At least 95% of controller updates should succeed";

    /* Verify integral term is bounded (anti-windup working) */
    if (controller->integral) {
        std::vector<float> integral_data(DEFAULT_BATCH * N_VARIABLES);
        nimcp_gpu_tensor_to_host(controller->integral, integral_data.data());

        for (size_t i = 0; i < integral_data.size(); i++) {
            EXPECT_LE(std::abs(integral_data[i]), pid_config.integral_limit * 1.1f)
                << "Integral should be bounded by anti-windup";
        }
    }

    nimcp_gpu_tensor_destroy(current);
    nimcp_gpu_tensor_destroy(target);
    nimcp_hypo_gpu_controller_destroy(controller);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Large Batch Processing
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, LargeBatchProcessing) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, LARGE_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, LARGE_BATCH, false);
    ASSERT_NE(setpoints, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    auto start = std::chrono::high_resolution_clock::now();

    int successful_operations = 0;
    const int n_operations = 200;

    for (int op = 0; op < n_operations; op++) {
        /* Alternate between different operations */
        bool result = false;
        switch (op % 4) {
            case 0:
                result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
                break;
            case 1:
                result = nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, 0.5f);
                break;
            case 2:
                result = nimcp_hypo_gpu_compute_deviation(ctx, drive_state, setpoints);
                break;
            case 3:
                result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
                break;
        }
        if (result) successful_operations++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    float success_rate = static_cast<float>(successful_operations) / n_operations;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of large batch operations should succeed";

    /* Should complete in reasonable time */
    EXPECT_LT(duration.count(), 10000)
        << "Large batch processing should complete within 10 seconds";

    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Reward Prediction Error Computation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, RewardPredictionError) {
#ifdef NIMCP_ENABLE_CUDA
    size_t dims[1] = {DEFAULT_BATCH};

    nimcp_gpu_tensor_t* actual = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* expected = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* rpe = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(rpe, nullptr);

    int successful_rpe = 0;
    const int n_iterations = 100;

    for (int iter = 0; iter < n_iterations; iter++) {
        /* Generate random actual and expected rewards */
        std::vector<float> actual_data(DEFAULT_BATCH);
        std::vector<float> expected_data(DEFAULT_BATCH);

        for (size_t i = 0; i < DEFAULT_BATCH; i++) {
            actual_data[i] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f;
            expected_data[i] = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f;
        }

        nimcp_gpu_tensor_upload(actual, actual_data.data(), actual_data.size() * sizeof(float));
        nimcp_gpu_tensor_upload(expected, expected_data.data(), expected_data.size() * sizeof(float));

        bool result = nimcp_hypo_gpu_compute_rpe(ctx, actual, expected, rpe);
        if (!result) continue;

        /* Verify RPE = actual - expected */
        std::vector<float> rpe_data(DEFAULT_BATCH);
        nimcp_gpu_tensor_to_host(rpe, rpe_data.data());

        bool correct = true;
        for (size_t i = 0; i < DEFAULT_BATCH && correct; i++) {
            float expected_rpe = actual_data[i] - expected_data[i];
            if (std::abs(rpe_data[i] - expected_rpe) > TOLERANCE) {
                correct = false;
            }
        }

        if (correct) successful_rpe++;
    }

    float success_rate = static_cast<float>(successful_rpe) / n_iterations;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of RPE computations should be correct";

    nimcp_gpu_tensor_destroy(actual);
    nimcp_gpu_tensor_destroy(expected);
    nimcp_gpu_tensor_destroy(rpe);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Alignment Score Tracking
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, AlignmentScoreTracking) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, true);
    ASSERT_NE(setpoints, nullptr);

    size_t dims[1] = {DEFAULT_BATCH};
    nimcp_gpu_tensor_t* score = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(score, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    std::vector<float> score_history;
    int successful_scores = 0;

    /* Track alignment score over simulation */
    for (int step = 0; step < 200; step++) {
        /* Integrate drives */
        nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);

        /* Compute alignment score every 10 steps */
        if (step % 10 == 0) {
            bool result = nimcp_hypo_gpu_compute_alignment(
                ctx, drive_state, setpoints, score);
            if (!result) continue;

            std::vector<float> score_data(DEFAULT_BATCH);
            nimcp_gpu_tensor_to_host(score, score_data.data());

            /* Record average score */
            float avg_score = std::accumulate(score_data.begin(), score_data.end(), 0.0f) /
                              DEFAULT_BATCH;
            if (std::isfinite(avg_score)) {
                score_history.push_back(avg_score);
                successful_scores++;
            }
        }
    }

    EXPECT_GE(successful_scores, 15)
        << "At least 75% of alignment scores should be computed";

    /* Verify score history is valid */
    for (float s : score_history) {
        EXPECT_TRUE(std::isfinite(s)) << "All alignment scores should be finite";
    }

    nimcp_gpu_tensor_destroy(score);
    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Priority Finding Under Varying Urgency
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, PriorityFindingVaryingUrgency) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, false);
    ASSERT_NE(setpoints, nullptr);

    size_t dims[1] = {DEFAULT_BATCH};
    nimcp_gpu_tensor_t* priority = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* max_urgency = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(priority, nullptr);
    ASSERT_NE(max_urgency, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    int successful_priorities = 0;

    /* Simulate varying arousal levels */
    for (int cycle = 0; cycle < 50; cycle++) {
        /* Integrate drives */
        for (int step = 0; step < 20; step++) {
            nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
        }

        /* Compute urgency with varying arousal */
        float arousal = 0.3f + 0.6f * (cycle % 10) / 10.0f;
        nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, arousal);

        /* Find priority */
        bool result = nimcp_hypo_gpu_find_priority(
            ctx, drive_state, priority, max_urgency);
        if (!result) continue;

        /* Verify priority indices are valid */
        std::vector<float> priority_data(DEFAULT_BATCH);
        nimcp_gpu_tensor_to_host(priority, priority_data.data());

        bool valid = true;
        for (size_t i = 0; i < DEFAULT_BATCH && valid; i++) {
            if (priority_data[i] < 0 ||
                priority_data[i] >= NIMCP_HYPO_GPU_DRIVE_COUNT) {
                valid = false;
            }
        }

        if (valid) successful_priorities++;
    }

    float success_rate = static_cast<float>(successful_priorities) / 50;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of priority computations should succeed";

    nimcp_gpu_tensor_destroy(priority);
    nimcp_gpu_tensor_destroy(max_urgency);
    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Stress Test with Rapid Operations
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, StressTestRapidOperations) {
#ifdef NIMCP_ENABLE_CUDA
    auto start = std::chrono::high_resolution_clock::now();

    const int n_simulations = 10;
    int successful_simulations = 0;

    for (int sim = 0; sim < n_simulations; sim++) {
        nimcp_hypo_gpu_drive_state_t* drive_state =
            nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
        if (!drive_state) continue;

        nimcp_hypo_gpu_setpoints_t* setpoints =
            nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, false);
        if (!setpoints) {
            nimcp_hypo_gpu_drive_state_destroy(drive_state);
            continue;
        }

        nimcp_hypo_gpu_reward_output_t* reward =
            nimcp_hypo_gpu_reward_output_create(ctx, DEFAULT_BATCH);
        if (!reward) {
            nimcp_hypo_gpu_setpoints_destroy(setpoints);
            nimcp_hypo_gpu_drive_state_destroy(drive_state);
            continue;
        }

        nimcp_hypo_gpu_dynamics_config_t dyn_config = nimcp_hypo_gpu_dynamics_config_default();
        nimcp_hypo_gpu_reward_config_t rew_config = nimcp_hypo_gpu_reward_config_default();

        bool sim_success = true;
        for (int step = 0; step < 100 && sim_success; step++) {
            sim_success = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &dyn_config);
            if (sim_success && step % 5 == 0) {
                sim_success = nimcp_hypo_gpu_compute_reward(
                    ctx, drive_state, setpoints, reward, &rew_config);
            }
        }

        if (sim_success) successful_simulations++;

        nimcp_hypo_gpu_reward_output_destroy(reward);
        nimcp_hypo_gpu_setpoints_destroy(setpoints);
        nimcp_hypo_gpu_drive_state_destroy(drive_state);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GE(successful_simulations, n_simulations * 0.70)
        << "At least 70% of stress test simulations should succeed";

    EXPECT_LT(duration.count(), 20000)
        << "Stress test should complete within 20 seconds";

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
 * Integration Test: Multi-Integrator Comparison
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, MultiIntegratorComparison) {
#ifdef NIMCP_ENABLE_CUDA
    const int n_steps = 100;

    /* Test each integrator type */
    nimcp_hypo_gpu_integrator_t integrators[] = {
        NIMCP_HYPO_GPU_EULER,
        NIMCP_HYPO_GPU_RK2,
        NIMCP_HYPO_GPU_RK4
    };

    for (int integ_idx = 0; integ_idx < 3; integ_idx++) {
        nimcp_hypo_gpu_drive_state_t* drive_state =
            nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
        ASSERT_NE(drive_state, nullptr);

        nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
        config.integrator = integrators[integ_idx];

        int successful_steps = 0;
        for (int step = 0; step < n_steps; step++) {
            bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
            if (result) successful_steps++;
        }

        float success_rate = static_cast<float>(successful_steps) / n_steps;
        EXPECT_GE(success_rate, 0.95f)
            << "Integrator " << integ_idx << " should have at least 95% success";

        /* Verify final state is valid */
        if (drive_state->state) {
            size_t state_size = DEFAULT_BATCH * NIMCP_HYPO_GPU_DRIVE_COUNT * 8;
            EXPECT_TRUE(verify_tensor_finite(drive_state->state, state_size))
                << "Integrator " << integ_idx << " should produce finite results";
        }

        nimcp_hypo_gpu_drive_state_destroy(drive_state);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Complete Hypothalamus Pipeline
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, CompleteHypothalamusPipeline) {
#ifdef NIMCP_ENABLE_CUDA
    /* Create all components */
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_setpoints_t* setpoints =
        nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH, true);
    ASSERT_NE(setpoints, nullptr);

    nimcp_hypo_gpu_reward_output_t* reward_output =
        nimcp_hypo_gpu_reward_output_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(reward_output, nullptr);

    nimcp_hypo_gpu_controller_state_t* controller =
        nimcp_hypo_gpu_controller_create(ctx, DEFAULT_BATCH, NIMCP_HYPO_GPU_DRIVE_COUNT);
    ASSERT_NE(controller, nullptr);

    nimcp_hypo_gpu_dynamics_config_t dyn_config = nimcp_hypo_gpu_dynamics_config_default();
    nimcp_hypo_gpu_reward_config_t rew_config = nimcp_hypo_gpu_reward_config_default();
    rew_config.mode = NIMCP_HYPO_GPU_REWARD_ALIGNED;
    nimcp_hypo_gpu_pid_config_t pid_config = nimcp_hypo_gpu_pid_config_default();

    /* Create tensors for controller */
    size_t ctrl_dims[2] = {DEFAULT_BATCH, NIMCP_HYPO_GPU_DRIVE_COUNT};
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_create(
        ctx, ctrl_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target = nimcp_gpu_tensor_create(
        ctx, ctrl_dims, 2, NIMCP_GPU_PRECISION_FP32);
    size_t score_dims[1] = {DEFAULT_BATCH};
    nimcp_gpu_tensor_t* alignment_score = nimcp_gpu_tensor_create(
        ctx, score_dims, 1, NIMCP_GPU_PRECISION_FP32);

    int total_ops = 0;
    int successful_ops = 0;

    /* Run complete pipeline */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* 1. Drive dynamics integration */
        total_ops++;
        if (nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &dyn_config)) {
            successful_ops++;
        }

        /* 2. Compute urgency */
        float arousal = 0.5f + 0.3f * std::sin(cycle * 0.1f);
        total_ops++;
        if (nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, arousal)) {
            successful_ops++;
        }

        /* 3. Compute deviation */
        total_ops++;
        if (nimcp_hypo_gpu_compute_deviation(ctx, drive_state, setpoints)) {
            successful_ops++;
        }

        /* 4. Compute reward */
        total_ops++;
        if (nimcp_hypo_gpu_compute_reward(ctx, drive_state, setpoints,
                                           reward_output, &rew_config)) {
            successful_ops++;
        }

        /* 5. Compute alignment */
        total_ops++;
        if (nimcp_hypo_gpu_compute_alignment(ctx, drive_state, setpoints,
                                              alignment_score)) {
            successful_ops++;
        }

        /* 6. Update controller (every 5 cycles) */
        if (cycle % 5 == 0) {
            total_ops++;
            if (nimcp_hypo_gpu_controller_update(ctx, controller, current, target,
                                                  DT, &pid_config)) {
                successful_ops++;
            }
        }
    }

    float success_rate = static_cast<float>(successful_ops) / total_ops;
    EXPECT_GE(success_rate, 0.90f)
        << "At least 90% of pipeline operations should succeed";

    /* Verify recovery stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    if (stats.total_errors > 0 && stats.recoveries_attempted > 0) {
        float recovery_rate = static_cast<float>(stats.successful_recoveries) /
                              stats.recoveries_attempted;
        EXPECT_GE(recovery_rate, 0.5f)
            << "Recovery success rate should be at least 50%";
    }

    nimcp_gpu_tensor_destroy(current);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(alignment_score);
    nimcp_hypo_gpu_controller_destroy(controller);
    nimcp_hypo_gpu_reward_output_destroy(reward_output);
    nimcp_hypo_gpu_setpoints_destroy(setpoints);
    nimcp_hypo_gpu_drive_state_destroy(drive_state);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Integration Test: Numerical Stability Long Run
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryIntegrationTest, NumericalStabilityLongRun) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_drive_state_t* drive_state =
        nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_EULER;

    /* Run for many iterations */
    const int n_iterations = 10000;
    int nan_detected = 0;
    int inf_detected = 0;

    for (int iter = 0; iter < n_iterations; iter++) {
        bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DT, &config);
        if (!result) continue;

        /* Check for numerical issues every 1000 iterations */
        if (iter % 1000 == 999 && drive_state->state) {
            size_t state_size = DEFAULT_BATCH * NIMCP_HYPO_GPU_DRIVE_COUNT * 8;
            std::vector<float> state_data(state_size);
            nimcp_gpu_tensor_to_host(drive_state->state, state_data.data());

            for (float v : state_data) {
                if (std::isnan(v)) nan_detected++;
                if (std::isinf(v)) inf_detected++;
            }
        }
    }

    EXPECT_EQ(nan_detected, 0) << "Should not produce NaN values over " << n_iterations << " steps";
    EXPECT_EQ(inf_detected, 0) << "Should not produce Inf values over " << n_iterations << " steps";

    nimcp_hypo_gpu_drive_state_destroy(drive_state);
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
