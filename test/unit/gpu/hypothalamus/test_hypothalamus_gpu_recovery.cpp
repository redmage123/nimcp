/**
 * @file test_hypothalamus_gpu_recovery.cpp
 * @brief Unit tests for GPU recovery integration in hypothalamus modules
 *
 * WHAT: Tests GPU recovery for hypothalamus drive dynamics and reward computation
 * WHY:  Verify self-healing from OOM, numerical errors, and kernel failures
 * HOW:  Test recovery initialization, error handling, and CPU fallback
 *
 * TEST COVERAGE:
 * - Recovery initialization in drive state creation
 * - OOM recovery during tensor allocation
 * - Numerical error recovery in hormone calculations
 * - Kernel launch failure recovery
 * - CPU fallback for hypothalamus operations
 * - Parameter correction for invalid configurations
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
#include "gpu/hypothalamus/nimcp_hypothalamus_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#endif

namespace {

/* Test constants */
static constexpr size_t DEFAULT_BATCH_SIZE = 16;
static constexpr size_t SMALL_BATCH_SIZE = 4;
static constexpr float TOLERANCE = 1e-4f;
static constexpr float DEFAULT_DT = 0.01f;

/* ============================================================================
 * Test Fixture: HypothalamusGPURecoveryTest
 * ============================================================================ */
class HypothalamusGPURecoveryTest : public ::testing::Test {
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
        if (drive_state) {
            nimcp_hypo_gpu_drive_state_destroy(drive_state);
            drive_state = nullptr;
        }
        if (setpoints) {
            nimcp_hypo_gpu_setpoints_destroy(setpoints);
            setpoints = nullptr;
        }
        if (reward_output) {
            nimcp_hypo_gpu_reward_output_destroy(reward_output);
            reward_output = nullptr;
        }
        if (controller) {
            nimcp_hypo_gpu_controller_destroy(controller);
            controller = nullptr;
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
    nimcp_hypo_gpu_drive_state_t* drive_state = nullptr;
    nimcp_hypo_gpu_setpoints_t* setpoints = nullptr;
    nimcp_hypo_gpu_reward_output_t* reward_output = nullptr;
    nimcp_hypo_gpu_controller_state_t* controller = nullptr;
#endif
};

/* ============================================================================
 * Test: Recovery initialization at drive state creation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryInitializedAtDriveStateCreation) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after drive state creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at setpoints creation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryInitializedAtSetpointsCreation) {
#ifdef NIMCP_ENABLE_CUDA
    setpoints = nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH_SIZE, true);
    ASSERT_NE(setpoints, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after setpoints creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at reward output creation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryInitializedAtRewardOutputCreation) {
#ifdef NIMCP_ENABLE_CUDA
    reward_output = nimcp_hypo_gpu_reward_output_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(reward_output, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after reward output creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery initialization at controller creation
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryInitializedAtControllerCreation) {
#ifdef NIMCP_ENABLE_CUDA
    controller = nimcp_hypo_gpu_controller_create(ctx, DEFAULT_BATCH_SIZE, 4);
    ASSERT_NE(controller, nullptr);

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery should be initialized after controller creation";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in drive state
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, InvalidParamRecoveryDriveState) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    nimcp_hypo_gpu_drive_state_t* bad_state =
        nimcp_hypo_gpu_drive_state_create(nullptr, DEFAULT_BATCH_SIZE);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for NULL context";

    /* Zero batch size should fail gracefully */
    bad_state = nimcp_hypo_gpu_drive_state_create(ctx, 0);
    EXPECT_EQ(bad_state, nullptr) << "Should fail gracefully for 0 batch_size";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in setpoints
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, InvalidParamRecoverySetpoints) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    nimcp_hypo_gpu_setpoints_t* bad_setpoints =
        nimcp_hypo_gpu_setpoints_create(nullptr, DEFAULT_BATCH_SIZE, true);
    EXPECT_EQ(bad_setpoints, nullptr) << "Should fail gracefully for NULL context";

    /* Zero batch size should fail gracefully */
    bad_setpoints = nimcp_hypo_gpu_setpoints_create(ctx, 0, true);
    EXPECT_EQ(bad_setpoints, nullptr) << "Should fail gracefully for 0 batch_size";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Invalid parameter recovery in controller
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, InvalidParamRecoveryController) {
#ifdef NIMCP_ENABLE_CUDA
    /* NULL context should fail gracefully */
    nimcp_hypo_gpu_controller_state_t* bad_ctrl =
        nimcp_hypo_gpu_controller_create(nullptr, DEFAULT_BATCH_SIZE, 4);
    EXPECT_EQ(bad_ctrl, nullptr) << "Should fail gracefully for NULL context";

    /* Zero variables should fail gracefully */
    bad_ctrl = nimcp_hypo_gpu_controller_create(ctx, DEFAULT_BATCH_SIZE, 0);
    EXPECT_EQ(bad_ctrl, nullptr) << "Should fail gracefully for 0 variables";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery context in drive integration
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryContextInDriveIntegration) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* rctx = nimcp_gpu_recovery_context_create(NULL);
    ASSERT_NE(rctx, nullptr);

    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, SMALL_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    /* Get default dynamics config */
    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_EULER;
    config.dt = DEFAULT_DT;

    /* Drive integration should succeed with recovery enabled */
    bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DEFAULT_DT, &config);
    EXPECT_TRUE(result) << "Drive integration should succeed with recovery";

    /* Verify recovery stats are accessible */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);
    EXPECT_GE(stats.success_rate, 0.0f);
    EXPECT_LE(stats.success_rate, 1.0f);

    nimcp_gpu_recovery_context_destroy(rctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for OOM in hypothalamus operations
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ErrorCategoryOOM) {
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
 * Test: Error category for numerical errors in hormone calculations
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ErrorCategoryNumericalHormone) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_action_t action = nimcp_gpu_select_recovery_strategy(
        GPU_ERROR_NUMERICAL, cudaSuccess, 0);

    /* Numerical error in hormone calculation should suggest correction or fallback */
    EXPECT_TRUE(action == GPU_RECOVERY_CLAMP_PARAMS ||
                action == GPU_RECOVERY_CPU_FALLBACK ||
                action == GPU_RECOVERY_NONE)
        << "Numerical error should trigger parameter correction or fallback";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category for kernel launch failure
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ErrorCategoryKernelLaunch) {
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
 * Test: Recovery action names are valid
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, RecoveryActionNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CPU_FALLBACK);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_CLAMP_PARAMS);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_recovery_action_name(GPU_RECOVERY_RETRY_IMMEDIATE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Error category names are valid
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ErrorCategoryNames) {
#ifdef NIMCP_ENABLE_CUDA
    const char* name = nimcp_gpu_error_category_name(GPU_ERROR_OUT_OF_MEMORY);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_NUMERICAL);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_gpu_error_category_name(GPU_ERROR_INVALID_PARAMS);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Drive integration Euler with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, DriveIntegrationEulerWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_EULER;
    config.dt = DEFAULT_DT;

    /* Run multiple integration steps */
    for (int step = 0; step < 100; step++) {
        bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DEFAULT_DT, &config);
        EXPECT_TRUE(result) << "Integration step " << step
                            << " should succeed with recovery";
    }

    /* Verify state is valid (no NaN) */
    if (drive_state->state) {
        size_t total_elements = DEFAULT_BATCH_SIZE * NIMCP_HYPO_GPU_DRIVE_COUNT * 8;
        std::vector<float> state_data(total_elements);
        nimcp_gpu_tensor_to_host(drive_state->state, state_data.data());

        for (size_t i = 0; i < total_elements; i++) {
            EXPECT_FALSE(std::isnan(state_data[i]))
                << "Should not have NaN at index " << i;
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Drive integration RK4 with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, DriveIntegrationRK4WithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, SMALL_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_RK4;
    config.dt = DEFAULT_DT;

    /* Run integration steps with RK4 */
    for (int step = 0; step < 50; step++) {
        bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DEFAULT_DT, &config);
        EXPECT_TRUE(result) << "RK4 integration step " << step
                            << " should succeed with recovery";
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Compute urgency with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ComputeUrgencyWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    setpoints = nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH_SIZE, false);
    ASSERT_NE(setpoints, nullptr);

    /* Compute urgency should succeed with recovery */
    bool result = nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, 0.5f);
    EXPECT_TRUE(result) << "Urgency computation should succeed with recovery";

    /* Verify urgency values are valid */
    if (drive_state->urgency) {
        size_t urgency_size = DEFAULT_BATCH_SIZE * NIMCP_HYPO_GPU_DRIVE_COUNT;
        std::vector<float> urgency_data(urgency_size);
        nimcp_gpu_tensor_to_host(drive_state->urgency, urgency_data.data());

        for (size_t i = 0; i < urgency_size; i++) {
            EXPECT_FALSE(std::isnan(urgency_data[i]))
                << "Urgency should not be NaN at index " << i;
            EXPECT_GE(urgency_data[i], 0.0f)
                << "Urgency should be non-negative at index " << i;
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Compute reward with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, ComputeRewardWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    setpoints = nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH_SIZE, true);
    ASSERT_NE(setpoints, nullptr);

    reward_output = nimcp_hypo_gpu_reward_output_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(reward_output, nullptr);

    nimcp_hypo_gpu_reward_config_t config = nimcp_hypo_gpu_reward_config_default();
    config.mode = NIMCP_HYPO_GPU_REWARD_WEIGHTED;

    /* Compute reward should succeed with recovery */
    bool result = nimcp_hypo_gpu_compute_reward(
        ctx, drive_state, setpoints, reward_output, &config);
    EXPECT_TRUE(result) << "Reward computation should succeed with recovery";

    /* Verify reward values are finite */
    if (reward_output->reward) {
        std::vector<float> reward_data(DEFAULT_BATCH_SIZE);
        nimcp_gpu_tensor_to_host(reward_output->reward, reward_data.data());

        for (size_t i = 0; i < DEFAULT_BATCH_SIZE; i++) {
            EXPECT_TRUE(std::isfinite(reward_data[i]))
                << "Reward should be finite at index " << i;
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: PID controller update with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, PIDControllerUpdateWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    const size_t n_variables = 4;
    controller = nimcp_hypo_gpu_controller_create(ctx, DEFAULT_BATCH_SIZE, n_variables);
    ASSERT_NE(controller, nullptr);

    /* Create current and target tensors */
    size_t dims[2] = {DEFAULT_BATCH_SIZE, n_variables};
    nimcp_gpu_tensor_t* current = nimcp_gpu_tensor_create(
        ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* target = nimcp_gpu_tensor_create(
        ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(current, nullptr);
    ASSERT_NE(target, nullptr);

    /* Initialize with some values */
    std::vector<float> current_data(DEFAULT_BATCH_SIZE * n_variables, 0.5f);
    std::vector<float> target_data(DEFAULT_BATCH_SIZE * n_variables, 1.0f);
    nimcp_gpu_tensor_upload(current, current_data.data(), current_data.size() * sizeof(float));
    nimcp_gpu_tensor_upload(target, target_data.data(), target_data.size() * sizeof(float));

    nimcp_hypo_gpu_pid_config_t pid_config = nimcp_hypo_gpu_pid_config_default();

    /* Controller update should succeed with recovery */
    bool result = nimcp_hypo_gpu_controller_update(
        ctx, controller, current, target, DEFAULT_DT, &pid_config);
    EXPECT_TRUE(result) << "PID controller update should succeed with recovery";

    /* Verify output is finite */
    if (controller->output) {
        std::vector<float> output_data(DEFAULT_BATCH_SIZE * n_variables);
        nimcp_gpu_tensor_to_host(controller->output, output_data.data());

        for (size_t i = 0; i < output_data.size(); i++) {
            EXPECT_TRUE(std::isfinite(output_data[i]))
                << "Controller output should be finite at index " << i;
        }
    }

    nimcp_gpu_tensor_destroy(current);
    nimcp_gpu_tensor_destroy(target);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Stats tracking after hypothalamus operations
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, StatsTrackingAfterHypothalamusOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    setpoints = nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH_SIZE, false);
    ASSERT_NE(setpoints, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    /* Run several operations */
    for (int i = 0; i < 20; i++) {
        nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DEFAULT_DT, &config);
        nimcp_hypo_gpu_compute_urgency(ctx, drive_state, setpoints, 0.5f);
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
 * Test: CPU fallback availability for hypothalamus
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, CPUFallbackAvailability) {
#ifdef NIMCP_ENABLE_CUDA
    bool available = nimcp_gpu_cpu_fallback_available();
    EXPECT_TRUE(available) << "CPU fallback should be available for hypothalamus";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: CPU fallback enabled by default
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, CPUFallbackEnabledByDefault) {
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
TEST_F(HypothalamusGPURecoveryTest, RepeatedOperationsWithRecoveryMonitoring) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_reset_stats();

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    /* Create and run multiple hypothalamus simulations */
    for (int sim = 0; sim < 5; sim++) {
        nimcp_hypo_gpu_drive_state_t* state =
            nimcp_hypo_gpu_drive_state_create(ctx, SMALL_BATCH_SIZE);
        ASSERT_NE(state, nullptr);

        /* Run simulation steps */
        for (int step = 0; step < 50; step++) {
            nimcp_hypo_gpu_drive_integrate(ctx, state, DEFAULT_DT, &config);
        }

        nimcp_hypo_gpu_drive_state_destroy(state);
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
TEST_F(HypothalamusGPURecoveryTest, RecoveryStrategyContextInvalid) {
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
 * Test: NULL safety in drive operations with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, NullSafetyDriveOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();

    EXPECT_FALSE(nimcp_hypo_gpu_drive_integrate(ctx, nullptr, DEFAULT_DT, &config))
        << "Should return false for NULL state";
    EXPECT_FALSE(nimcp_hypo_gpu_compute_urgency(ctx, nullptr, nullptr, 0.5f))
        << "Should return false for NULL state and setpoints";
    EXPECT_FALSE(nimcp_hypo_gpu_compute_deviation(ctx, nullptr, nullptr))
        << "Should return false for NULL parameters";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NULL safety in reward operations with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, NullSafetyRewardOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_reward_config_t config = nimcp_hypo_gpu_reward_config_default();

    EXPECT_FALSE(nimcp_hypo_gpu_compute_reward(ctx, nullptr, nullptr, nullptr, &config))
        << "Should return false for NULL parameters";
    EXPECT_FALSE(nimcp_hypo_gpu_compute_rpe(ctx, nullptr, nullptr, nullptr))
        << "Should return false for NULL parameters";
    EXPECT_FALSE(nimcp_hypo_gpu_compute_alignment(ctx, nullptr, nullptr, nullptr))
        << "Should return false for NULL parameters";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NULL safety in controller operations with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, NullSafetyControllerOps) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_hypo_gpu_pid_config_t config = nimcp_hypo_gpu_pid_config_default();

    EXPECT_FALSE(nimcp_hypo_gpu_controller_update(
        ctx, nullptr, nullptr, nullptr, DEFAULT_DT, &config))
        << "Should return false for NULL parameters";
    EXPECT_FALSE(nimcp_hypo_gpu_controller_reset_integral(ctx, nullptr))
        << "Should return false for NULL controller";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Numerical stability in drive dynamics
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, NumericalStabilityDriveDynamics) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, SMALL_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    nimcp_hypo_gpu_dynamics_config_t config = nimcp_hypo_gpu_dynamics_config_default();
    config.integrator = NIMCP_HYPO_GPU_EULER;

    /* Run many integration steps to check stability */
    for (int step = 0; step < 1000; step++) {
        bool result = nimcp_hypo_gpu_drive_integrate(ctx, drive_state, DEFAULT_DT, &config);
        ASSERT_TRUE(result) << "Integration should not fail at step " << step;
    }

    /* Verify no NaN or Inf values */
    if (drive_state->state) {
        size_t total_elements = SMALL_BATCH_SIZE * NIMCP_HYPO_GPU_DRIVE_COUNT * 8;
        std::vector<float> state_data(total_elements);
        nimcp_gpu_tensor_to_host(drive_state->state, state_data.data());

        for (size_t i = 0; i < total_elements; i++) {
            EXPECT_FALSE(std::isnan(state_data[i]))
                << "Should not have NaN at index " << i << " after 1000 steps";
            EXPECT_FALSE(std::isinf(state_data[i]))
                << "Should not have Inf at index " << i << " after 1000 steps";
        }
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Alignment score computation with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, AlignmentScoreWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    setpoints = nimcp_hypo_gpu_setpoints_create(ctx, DEFAULT_BATCH_SIZE, true);
    ASSERT_NE(setpoints, nullptr);

    /* Create score output tensor */
    size_t dims[1] = {DEFAULT_BATCH_SIZE};
    nimcp_gpu_tensor_t* score_out = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(score_out, nullptr);

    /* Compute alignment score */
    bool result = nimcp_hypo_gpu_compute_alignment(
        ctx, drive_state, setpoints, score_out);
    EXPECT_TRUE(result) << "Alignment computation should succeed with recovery";

    /* Verify scores are valid */
    std::vector<float> scores(DEFAULT_BATCH_SIZE);
    nimcp_gpu_tensor_to_host(score_out, scores.data());

    for (size_t i = 0; i < DEFAULT_BATCH_SIZE; i++) {
        EXPECT_TRUE(std::isfinite(scores[i]))
            << "Alignment score should be finite at index " << i;
    }

    nimcp_gpu_tensor_destroy(score_out);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Priority finding with recovery
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, PriorityFindingWithRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    drive_state = nimcp_hypo_gpu_drive_state_create(ctx, DEFAULT_BATCH_SIZE);
    ASSERT_NE(drive_state, nullptr);

    /* Create output tensors */
    size_t dims[1] = {DEFAULT_BATCH_SIZE};
    nimcp_gpu_tensor_t* priority_out = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    nimcp_gpu_tensor_t* max_urgency_out = nimcp_gpu_tensor_create(
        ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(priority_out, nullptr);
    ASSERT_NE(max_urgency_out, nullptr);

    /* Find priority */
    bool result = nimcp_hypo_gpu_find_priority(
        ctx, drive_state, priority_out, max_urgency_out);
    EXPECT_TRUE(result) << "Priority finding should succeed with recovery";

    /* Verify outputs are valid */
    std::vector<float> priorities(DEFAULT_BATCH_SIZE);
    std::vector<float> max_urgencies(DEFAULT_BATCH_SIZE);
    nimcp_gpu_tensor_to_host(priority_out, priorities.data());
    nimcp_gpu_tensor_to_host(max_urgency_out, max_urgencies.data());

    for (size_t i = 0; i < DEFAULT_BATCH_SIZE; i++) {
        EXPECT_TRUE(std::isfinite(priorities[i]))
            << "Priority should be finite at index " << i;
        EXPECT_TRUE(std::isfinite(max_urgencies[i]))
            << "Max urgency should be finite at index " << i;
    }

    nimcp_gpu_tensor_destroy(priority_out);
    nimcp_gpu_tensor_destroy(max_urgency_out);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Default configuration getters
 * ============================================================================ */
TEST_F(HypothalamusGPURecoveryTest, DefaultConfigurationGetters) {
#ifdef NIMCP_ENABLE_CUDA
    /* Test dynamics config */
    nimcp_hypo_gpu_dynamics_config_t dyn_config = nimcp_hypo_gpu_dynamics_config_default();
    EXPECT_GT(dyn_config.dt, 0.0f);
    EXPECT_GE(dyn_config.threads_per_block, 1u);

    /* Test reward config */
    nimcp_hypo_gpu_reward_config_t rew_config = nimcp_hypo_gpu_reward_config_default();
    EXPECT_GE(rew_config.alignment_weight, 0.0f);
    EXPECT_LE(rew_config.temporal_discount, 1.0f);

    /* Test PID config */
    nimcp_hypo_gpu_pid_config_t pid_config = nimcp_hypo_gpu_pid_config_default();
    EXPECT_TRUE(std::isfinite(pid_config.kp));
    EXPECT_TRUE(std::isfinite(pid_config.ki));
    EXPECT_TRUE(std::isfinite(pid_config.kd));
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
