/**
 * @file test_training_enhancements_cl_mtl.cpp
 * @brief Unit tests for Continual Learning (CL) and Multi-Task Learning (MTL) modules
 *
 * WHAT: Comprehensive unit tests for CL and MTL training enhancements
 * WHY:  Ensure continual learning prevents catastrophic forgetting and MTL handles
 *       multi-task gradient conflicts correctly
 * HOW:  Test lifecycle, task management, regularization, replay, gradient handling
 *
 * Test Categories:
 * - Continual Learning (CL):
 *   - Lifecycle (default_config, create, destroy)
 *   - Task management (start_task, end_task, get_current_task, get_num_tasks)
 *   - EWC penalty computation
 *   - Replay buffer operations
 *   - Statistics tracking
 *   - Strategy and config validation
 *
 * - Multi-Task Learning (MTL):
 *   - Lifecycle (default_config, create, destroy)
 *   - Task registration and management
 *   - Task weighting
 *   - Task sampling
 *   - Gradient similarity and conflict detection
 *   - Statistics tracking
 *   - Architecture and strategy name utilities
 *   - Config validation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <vector>

// Headers have their own extern "C" guards
#include "training/nimcp_continual_learning.h"
#include "training/nimcp_multi_task.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Continual Learning Test Fixture
//=============================================================================

class ContinualLearningTest : public ::testing::Test {
protected:
    cl_ctx_t* ctx = nullptr;
    cl_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (ctx) {
            cl_destroy(ctx);
            ctx = nullptr;
        }
    }

    void CreateDefaultContext() {
        int ret = cl_default_config(&config);
        ASSERT_EQ(ret, 0) << "Failed to get default CL config";
        ctx = cl_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create CL context";
    }
};

//=============================================================================
// Continual Learning - Lifecycle Tests
//=============================================================================

TEST_F(ContinualLearningTest, DefaultConfig_InitializesCorrectly) {
    // WHAT: Test cl_default_config initializes configuration correctly
    // WHY:  Verify default values are sensible for EWC strategy
    // HOW:  Call default_config and check expected defaults

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0) << "cl_default_config should return 0 on success";

    // Verify strategy is set to EWC (default)
    EXPECT_EQ(config.strategy, CL_STRATEGY_EWC)
        << "Default strategy should be EWC";

    // Verify EWC lambda is set correctly
    EXPECT_FLOAT_EQ(config.ewc.lambda, CL_DEFAULT_EWC_LAMBDA)
        << "Default EWC lambda should be " << CL_DEFAULT_EWC_LAMBDA;

    // Verify replay buffer size
    EXPECT_EQ(config.replay.buffer_size, CL_DEFAULT_BUFFER_SIZE)
        << "Default replay buffer size should be " << CL_DEFAULT_BUFFER_SIZE;
}

TEST_F(ContinualLearningTest, DefaultConfig_NullPointer_ReturnsError) {
    // WHAT: Test cl_default_config rejects null pointer
    // WHY:  Ensure proper error handling for invalid input
    // HOW:  Call default_config with NULL

    int ret = cl_default_config(nullptr);
    EXPECT_NE(ret, 0) << "cl_default_config should reject NULL config";
}

TEST_F(ContinualLearningTest, Create_WithValidConfig_Succeeds) {
    // WHAT: Test cl_create creates context with valid configuration
    // WHY:  Verify context creation works
    // HOW:  Create context with default config

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0);

    ctx = cl_create(&config);
    EXPECT_NE(ctx, nullptr) << "cl_create should return valid context";
}

TEST_F(ContinualLearningTest, Create_WithNullConfig_ReturnsNull) {
    // WHAT: Test cl_create rejects null configuration
    // WHY:  Ensure proper error handling
    // HOW:  Call create with NULL config

    ctx = cl_create(nullptr);
    EXPECT_EQ(ctx, nullptr) << "cl_create should return NULL for NULL config";
}

TEST_F(ContinualLearningTest, Destroy_NullContext_DoesNotCrash) {
    // WHAT: Test cl_destroy handles null context gracefully
    // WHY:  Ensure NULL-safe destruction
    // HOW:  Call destroy with NULL

    cl_destroy(nullptr);
    SUCCEED() << "cl_destroy should handle NULL without crashing";
}

TEST_F(ContinualLearningTest, Destroy_ValidContext_Succeeds) {
    // WHAT: Test cl_destroy properly cleans up context
    // WHY:  Verify no memory leaks or crashes on destruction
    // HOW:  Create and destroy context

    CreateDefaultContext();
    cl_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
    SUCCEED() << "cl_destroy should clean up context properly";
}

//=============================================================================
// Continual Learning - Task Management Tests
//=============================================================================

TEST_F(ContinualLearningTest, StartTask_FirstTask_Succeeds) {
    // WHAT: Test cl_start_task starts a new task
    // WHY:  Verify task initialization works
    // HOW:  Start first task and check it's current

    CreateDefaultContext();

    int ret = cl_start_task(ctx, 0, "Task_0");
    EXPECT_EQ(ret, 0) << "cl_start_task should succeed for first task";

    int current = cl_get_current_task(ctx);
    EXPECT_EQ(current, 0) << "Current task should be 0";
}

TEST_F(ContinualLearningTest, StartTask_NullContext_ReturnsError) {
    // WHAT: Test cl_start_task rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call start_task with NULL context

    int ret = cl_start_task(nullptr, 0, "Task_0");
    EXPECT_NE(ret, 0) << "cl_start_task should reject NULL context";
}

TEST_F(ContinualLearningTest, StartTask_MultipleTasks_TracksCorrectly) {
    // WHAT: Test cl_start_task handles multiple sequential tasks
    // WHY:  Verify task tracking works across task transitions
    // HOW:  Start task 0, end it, start task 1

    CreateDefaultContext();

    // Start task 0
    int ret = cl_start_task(ctx, 0, "Task_0");
    ASSERT_EQ(ret, 0);

    // End task 0
    ret = cl_end_task(ctx);
    EXPECT_EQ(ret, 0) << "cl_end_task should succeed";

    // Start task 1
    ret = cl_start_task(ctx, 1, "Task_1");
    EXPECT_EQ(ret, 0) << "cl_start_task should succeed for task 1";

    int current = cl_get_current_task(ctx);
    EXPECT_EQ(current, 1) << "Current task should be 1";
}

TEST_F(ContinualLearningTest, EndTask_NullContext_ReturnsError) {
    // WHAT: Test cl_end_task rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call end_task with NULL context

    int ret = cl_end_task(nullptr);
    EXPECT_NE(ret, 0) << "cl_end_task should reject NULL context";
}

TEST_F(ContinualLearningTest, GetCurrentTask_NoTaskStarted_ReturnsNegative) {
    // WHAT: Test cl_get_current_task returns -1 when no task active
    // WHY:  Verify proper state tracking
    // HOW:  Create context without starting task

    CreateDefaultContext();

    int current = cl_get_current_task(ctx);
    EXPECT_EQ(current, -1) << "Current task should be -1 when none started";
}

TEST_F(ContinualLearningTest, GetCurrentTask_NullContext_ReturnsNegative) {
    // WHAT: Test cl_get_current_task handles null context
    // WHY:  Ensure proper error handling
    // HOW:  Call get_current_task with NULL

    int current = cl_get_current_task(nullptr);
    EXPECT_EQ(current, -1) << "Current task should be -1 for NULL context";
}

TEST_F(ContinualLearningTest, GetNumTasks_InitiallyZero) {
    // WHAT: Test cl_get_num_tasks returns 0 initially
    // WHY:  Verify task count starts at zero
    // HOW:  Create context and check task count

    CreateDefaultContext();

    uint32_t num_tasks = cl_get_num_tasks(ctx);
    EXPECT_EQ(num_tasks, 0u) << "Initial task count should be 0";
}

TEST_F(ContinualLearningTest, GetNumTasks_AfterEndTask_Increments) {
    // WHAT: Test cl_get_num_tasks increments after ending task
    // WHY:  Verify task count tracks completed tasks
    // HOW:  Start and end a task, check count

    CreateDefaultContext();

    cl_start_task(ctx, 0, "Task_0");
    cl_end_task(ctx);

    uint32_t num_tasks = cl_get_num_tasks(ctx);
    EXPECT_EQ(num_tasks, 1u) << "Task count should be 1 after ending first task";
}

TEST_F(ContinualLearningTest, GetNumTasks_NullContext_ReturnsZero) {
    // WHAT: Test cl_get_num_tasks handles null context
    // WHY:  Ensure proper error handling
    // HOW:  Call get_num_tasks with NULL

    uint32_t num_tasks = cl_get_num_tasks(nullptr);
    EXPECT_EQ(num_tasks, 0u) << "Task count should be 0 for NULL context";
}

//=============================================================================
// Continual Learning - EWC Penalty Tests
//=============================================================================

TEST_F(ContinualLearningTest, EWCPenalty_NoTasks_ReturnsZero) {
    // WHAT: Test cl_ewc_penalty returns 0 when no tasks learned
    // WHY:  Verify penalty is zero without reference parameters
    // HOW:  Create context, compute penalty without training

    CreateDefaultContext();

    std::vector<float> params(100, 0.5f);
    float penalty = cl_ewc_penalty(ctx, params.data(), params.size());

    EXPECT_FLOAT_EQ(penalty, 0.0f)
        << "EWC penalty should be 0 with no learned tasks";
}

TEST_F(ContinualLearningTest, EWCPenalty_NullContext_ReturnsZero) {
    // WHAT: Test cl_ewc_penalty handles null context
    // WHY:  Ensure proper error handling
    // HOW:  Call ewc_penalty with NULL context

    std::vector<float> params(100, 0.5f);
    float penalty = cl_ewc_penalty(nullptr, params.data(), params.size());

    EXPECT_FLOAT_EQ(penalty, 0.0f)
        << "EWC penalty should be 0 for NULL context";
}

TEST_F(ContinualLearningTest, EWCPenalty_NullParams_ReturnsZero) {
    // WHAT: Test cl_ewc_penalty handles null parameters
    // WHY:  Ensure proper error handling
    // HOW:  Call ewc_penalty with NULL params

    CreateDefaultContext();

    float penalty = cl_ewc_penalty(ctx, nullptr, 100);

    EXPECT_FLOAT_EQ(penalty, 0.0f)
        << "EWC penalty should be 0 for NULL params";
}

TEST_F(ContinualLearningTest, EWCPenalty_ZeroParams_ReturnsZero) {
    // WHAT: Test cl_ewc_penalty handles zero parameter count
    // WHY:  Ensure proper handling of edge case
    // HOW:  Call ewc_penalty with 0 num_params

    CreateDefaultContext();

    std::vector<float> params(1, 0.5f);
    float penalty = cl_ewc_penalty(ctx, params.data(), 0);

    EXPECT_FLOAT_EQ(penalty, 0.0f)
        << "EWC penalty should be 0 for zero params";
}

//=============================================================================
// Continual Learning - Replay Buffer Tests
//=============================================================================

TEST_F(ContinualLearningTest, ReplayBufferSize_InitiallyZero) {
    // WHAT: Test cl_replay_buffer_size returns 0 initially
    // WHY:  Verify buffer starts empty
    // HOW:  Create context, check buffer size

    CreateDefaultContext();

    uint32_t size = cl_replay_buffer_size(ctx);
    EXPECT_EQ(size, 0u) << "Initial replay buffer size should be 0";
}

TEST_F(ContinualLearningTest, ReplayBufferSize_NullContext_ReturnsZero) {
    // WHAT: Test cl_replay_buffer_size handles null context
    // WHY:  Ensure proper error handling
    // HOW:  Call replay_buffer_size with NULL

    uint32_t size = cl_replay_buffer_size(nullptr);
    EXPECT_EQ(size, 0u) << "Replay buffer size should be 0 for NULL context";
}

//=============================================================================
// Continual Learning - Statistics Tests
//=============================================================================

TEST_F(ContinualLearningTest, GetStats_InitialValues) {
    // WHAT: Test cl_get_stats returns initialized statistics
    // WHY:  Verify statistics start at sensible defaults
    // HOW:  Create context, get stats, check values

    CreateDefaultContext();

    cl_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero to detect unchanged fields

    int ret = cl_get_stats(ctx, &stats);
    EXPECT_EQ(ret, 0) << "cl_get_stats should succeed";

    EXPECT_EQ(stats.total_steps, 0u) << "Initial total_steps should be 0";
    EXPECT_EQ(stats.tasks_learned, 0u) << "Initial tasks_learned should be 0";
}

TEST_F(ContinualLearningTest, GetStats_NullContext_ReturnsError) {
    // WHAT: Test cl_get_stats rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call get_stats with NULL context

    cl_stats_t stats;
    int ret = cl_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0) << "cl_get_stats should reject NULL context";
}

TEST_F(ContinualLearningTest, GetStats_NullStats_ReturnsError) {
    // WHAT: Test cl_get_stats rejects null stats pointer
    // WHY:  Ensure proper error handling
    // HOW:  Call get_stats with NULL stats

    CreateDefaultContext();

    int ret = cl_get_stats(ctx, nullptr);
    EXPECT_NE(ret, 0) << "cl_get_stats should reject NULL stats";
}

//=============================================================================
// Continual Learning - Utility Tests
//=============================================================================

TEST_F(ContinualLearningTest, StrategyName_AllStrategies_ReturnValidNames) {
    // WHAT: Test cl_strategy_name returns valid names for all strategies
    // WHY:  Verify string conversion works for all enum values
    // HOW:  Iterate through all strategies and check non-null return

    const char* expected_names[] = {
        "NAIVE", "EWC", "EWC_ONLINE", "MAS", "SI", "LWF",
        "PACKNET", "PROGRESSIVE", "HAT", "GEM", "AGEM",
        "REPLAY", "GENERATIVE_REPLAY", "HYBRID"
    };

    for (int i = 0; i < CL_STRATEGY_COUNT; i++) {
        const char* name = cl_strategy_name(static_cast<cl_strategy_t>(i));
        EXPECT_NE(name, nullptr) << "Strategy " << i << " should have a name";
        EXPECT_STRNE(name, "") << "Strategy " << i << " name should not be empty";
    }
}

TEST_F(ContinualLearningTest, StrategyName_InvalidStrategy_ReturnsUnknown) {
    // WHAT: Test cl_strategy_name handles invalid strategy value
    // WHY:  Ensure graceful handling of out-of-range values
    // HOW:  Call with value beyond CL_STRATEGY_COUNT

    const char* name = cl_strategy_name(static_cast<cl_strategy_t>(CL_STRATEGY_COUNT + 100));
    EXPECT_NE(name, nullptr) << "Invalid strategy should return non-null";
    // Should return "UNKNOWN" or similar
}

TEST_F(ContinualLearningTest, ValidateConfig_ValidConfig_ReturnsSuccess) {
    // WHAT: Test cl_validate_config accepts valid configuration
    // WHY:  Verify validation passes for correct configs
    // HOW:  Get default config and validate

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0);

    ret = cl_validate_config(&config);
    EXPECT_EQ(ret, 0) << "Valid config should pass validation";
}

TEST_F(ContinualLearningTest, ValidateConfig_NullConfig_ReturnsError) {
    // WHAT: Test cl_validate_config rejects null config
    // WHY:  Ensure proper error handling
    // HOW:  Call validate_config with NULL

    int ret = cl_validate_config(nullptr);
    EXPECT_NE(ret, 0) << "cl_validate_config should reject NULL config";
}

//=============================================================================
// Multi-Task Learning Test Fixture
//=============================================================================

class MultiTaskLearningTest : public ::testing::Test {
protected:
    mtl_ctx_t* ctx = nullptr;
    mtl_config_t config;

    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    void TearDown() override {
        if (ctx) {
            mtl_destroy(ctx);
            ctx = nullptr;
        }
    }

    void CreateDefaultContext() {
        int ret = mtl_default_config(&config);
        ASSERT_EQ(ret, 0) << "Failed to get default MTL config";
        ctx = mtl_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed to create MTL context";
    }

    mtl_task_def_t CreateTaskDef(uint32_t task_id, const char* name,
                                  uint32_t output_dim, float weight) {
        mtl_task_def_t task;
        memset(&task, 0, sizeof(task));
        task.task_id = task_id;
        task.name = name;
        task.loss_type = NIMCP_LOSS_CROSS_ENTROPY;
        task.output_dim = output_dim;
        task.weight = weight;
        task.active = true;
        task.num_classes = output_dim;
        task.is_regression = false;
        task.loss_scale = 1.0f;
        return task;
    }
};

//=============================================================================
// Multi-Task Learning - Lifecycle Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, DefaultConfig_InitializesCorrectly) {
    // WHAT: Test mtl_default_config initializes configuration correctly
    // WHY:  Verify default values are sensible
    // HOW:  Call default_config and check expected defaults

    int ret = mtl_default_config(&config);
    ASSERT_EQ(ret, 0) << "mtl_default_config should return 0 on success";

    // Verify architecture is hard sharing (default)
    EXPECT_EQ(config.architecture, MTL_ARCH_HARD_SHARING)
        << "Default architecture should be hard sharing";

    // Verify weighting strategy
    EXPECT_EQ(config.weighting, MTL_WEIGHT_UNCERTAINTY)
        << "Default weighting should be uncertainty";

    // Verify gradient method
    EXPECT_EQ(config.gradient_method, MTL_GRAD_PCGRAD)
        << "Default gradient method should be PCGrad";
}

TEST_F(MultiTaskLearningTest, DefaultConfig_NullPointer_ReturnsError) {
    // WHAT: Test mtl_default_config rejects null pointer
    // WHY:  Ensure proper error handling
    // HOW:  Call default_config with NULL

    int ret = mtl_default_config(nullptr);
    EXPECT_NE(ret, 0) << "mtl_default_config should reject NULL config";
}

TEST_F(MultiTaskLearningTest, Create_WithValidConfig_Succeeds) {
    // WHAT: Test mtl_create creates context with valid configuration
    // WHY:  Verify context creation works
    // HOW:  Create context with default config

    int ret = mtl_default_config(&config);
    ASSERT_EQ(ret, 0);

    ctx = mtl_create(&config);
    EXPECT_NE(ctx, nullptr) << "mtl_create should return valid context";
}

TEST_F(MultiTaskLearningTest, Create_WithNullConfig_ReturnsNull) {
    // WHAT: Test mtl_create rejects null configuration
    // WHY:  Ensure proper error handling
    // HOW:  Call create with NULL config

    ctx = mtl_create(nullptr);
    EXPECT_EQ(ctx, nullptr) << "mtl_create should return NULL for NULL config";
}

TEST_F(MultiTaskLearningTest, Destroy_NullContext_DoesNotCrash) {
    // WHAT: Test mtl_destroy handles null context gracefully
    // WHY:  Ensure NULL-safe destruction
    // HOW:  Call destroy with NULL

    mtl_destroy(nullptr);
    SUCCEED() << "mtl_destroy should handle NULL without crashing";
}

TEST_F(MultiTaskLearningTest, Destroy_ValidContext_Succeeds) {
    // WHAT: Test mtl_destroy properly cleans up context
    // WHY:  Verify no memory leaks or crashes on destruction
    // HOW:  Create and destroy context

    CreateDefaultContext();
    mtl_destroy(ctx);
    ctx = nullptr;  // Prevent double-free in TearDown
    SUCCEED() << "mtl_destroy should clean up context properly";
}

//=============================================================================
// Multi-Task Learning - Task Registration Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, RegisterTask_FirstTask_Succeeds) {
    // WHAT: Test mtl_register_task registers first task
    // WHY:  Verify task registration works
    // HOW:  Register task and check return value

    CreateDefaultContext();

    mtl_task_def_t task = CreateTaskDef(0, "Classification", 10, 1.0f);
    int ret = mtl_register_task(ctx, &task);

    EXPECT_GE(ret, 0) << "mtl_register_task should return non-negative task index";
}

TEST_F(MultiTaskLearningTest, RegisterTask_NullContext_ReturnsError) {
    // WHAT: Test mtl_register_task rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call register_task with NULL context

    mtl_task_def_t task = CreateTaskDef(0, "Test", 10, 1.0f);
    int ret = mtl_register_task(nullptr, &task);

    EXPECT_LT(ret, 0) << "mtl_register_task should reject NULL context";
}

TEST_F(MultiTaskLearningTest, RegisterTask_NullTask_ReturnsError) {
    // WHAT: Test mtl_register_task rejects null task definition
    // WHY:  Ensure proper error handling
    // HOW:  Call register_task with NULL task

    CreateDefaultContext();

    int ret = mtl_register_task(ctx, nullptr);
    EXPECT_LT(ret, 0) << "mtl_register_task should reject NULL task";
}

TEST_F(MultiTaskLearningTest, RegisterTask_MultipleTasks_TracksAll) {
    // WHAT: Test mtl_register_task handles multiple tasks
    // WHY:  Verify multi-task tracking works
    // HOW:  Register several tasks

    CreateDefaultContext();

    mtl_task_def_t task0 = CreateTaskDef(0, "Classification", 10, 1.0f);
    mtl_task_def_t task1 = CreateTaskDef(1, "Segmentation", 20, 0.5f);
    mtl_task_def_t task2 = CreateTaskDef(2, "Detection", 15, 0.75f);

    int idx0 = mtl_register_task(ctx, &task0);
    int idx1 = mtl_register_task(ctx, &task1);
    int idx2 = mtl_register_task(ctx, &task2);

    EXPECT_GE(idx0, 0) << "First task registration should succeed";
    EXPECT_GE(idx1, 0) << "Second task registration should succeed";
    EXPECT_GE(idx2, 0) << "Third task registration should succeed";
}

//=============================================================================
// Multi-Task Learning - Task Active State Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, SetTaskActive_DeactivateTask_Succeeds) {
    // WHAT: Test mtl_set_task_active can deactivate a task
    // WHY:  Verify task activation control works
    // HOW:  Register task, deactivate it

    CreateDefaultContext();

    mtl_task_def_t task = CreateTaskDef(0, "Test", 10, 1.0f);
    mtl_register_task(ctx, &task);

    int ret = mtl_set_task_active(ctx, 0, false);
    EXPECT_EQ(ret, 0) << "mtl_set_task_active should succeed";
}

TEST_F(MultiTaskLearningTest, SetTaskActive_NullContext_ReturnsError) {
    // WHAT: Test mtl_set_task_active rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call set_task_active with NULL context

    int ret = mtl_set_task_active(nullptr, 0, true);
    EXPECT_NE(ret, 0) << "mtl_set_task_active should reject NULL context";
}

TEST_F(MultiTaskLearningTest, SetTaskActive_InvalidTaskId_ReturnsError) {
    // WHAT: Test mtl_set_task_active rejects invalid task ID
    // WHY:  Ensure proper bounds checking
    // HOW:  Call with task ID that doesn't exist

    CreateDefaultContext();

    int ret = mtl_set_task_active(ctx, 999, true);
    EXPECT_NE(ret, 0) << "mtl_set_task_active should reject invalid task ID";
}

//=============================================================================
// Multi-Task Learning - Task Weight Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, GetTaskWeight_ReturnsRegisteredWeight) {
    // WHAT: Test mtl_get_task_weight returns correct initial weight
    // WHY:  Verify weight retrieval works
    // HOW:  Register task with specific weight, retrieve it

    CreateDefaultContext();

    mtl_task_def_t task = CreateTaskDef(0, "Test", 10, 0.75f);
    mtl_register_task(ctx, &task);

    float weight = mtl_get_task_weight(ctx, 0);
    EXPECT_FLOAT_EQ(weight, 0.75f) << "Task weight should match registered value";
}

TEST_F(MultiTaskLearningTest, GetTaskWeight_NullContext_ReturnsSafeDefault) {
    // WHAT: Test mtl_get_task_weight handles null context
    // WHY:  Ensure proper error handling
    // HOW:  Call get_task_weight with NULL context

    float weight = mtl_get_task_weight(nullptr, 0);
    // Implementation returns 1.0 as a safe default (neutral weight)
    EXPECT_FLOAT_EQ(weight, 1.0f) << "Task weight should be 1.0 (neutral) for NULL context";
}

TEST_F(MultiTaskLearningTest, SetTaskWeight_UpdatesWeight) {
    // WHAT: Test mtl_set_task_weight updates task weight
    // WHY:  Verify dynamic weight adjustment works
    // HOW:  Register task, change weight, verify

    CreateDefaultContext();

    mtl_task_def_t task = CreateTaskDef(0, "Test", 10, 1.0f);
    mtl_register_task(ctx, &task);

    int ret = mtl_set_task_weight(ctx, 0, 0.5f);
    EXPECT_EQ(ret, 0) << "mtl_set_task_weight should succeed";

    float weight = mtl_get_task_weight(ctx, 0);
    EXPECT_FLOAT_EQ(weight, 0.5f) << "Task weight should be updated";
}

TEST_F(MultiTaskLearningTest, SetTaskWeight_NullContext_ReturnsError) {
    // WHAT: Test mtl_set_task_weight rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call set_task_weight with NULL context

    int ret = mtl_set_task_weight(nullptr, 0, 0.5f);
    EXPECT_NE(ret, 0) << "mtl_set_task_weight should reject NULL context";
}

//=============================================================================
// Multi-Task Learning - Task Sampling Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, SampleTask_WithRegisteredTasks_ReturnsValidId) {
    // WHAT: Test mtl_sample_task returns valid task ID
    // WHY:  Verify task sampling works
    // HOW:  Register tasks and sample

    CreateDefaultContext();

    mtl_task_def_t task0 = CreateTaskDef(0, "Task0", 10, 1.0f);
    mtl_task_def_t task1 = CreateTaskDef(1, "Task1", 20, 1.0f);

    mtl_register_task(ctx, &task0);
    mtl_register_task(ctx, &task1);

    // Sample multiple times and verify returned IDs are valid
    for (int i = 0; i < 10; i++) {
        uint32_t task_id = mtl_sample_task(ctx);
        EXPECT_LE(task_id, 1u) << "Sampled task ID should be 0 or 1";
    }
}

//=============================================================================
// Multi-Task Learning - Gradient Similarity Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, GradientSimilarity_IdenticalGradients_ReturnsOne) {
    // WHAT: Test mtl_gradient_similarity returns 1.0 for identical gradients
    // WHY:  Verify cosine similarity computation
    // HOW:  Create identical gradient vectors, compute similarity

    std::vector<float> grad1 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> grad2 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float similarity = mtl_gradient_similarity(grad1.data(), grad2.data(), grad1.size());

    EXPECT_NEAR(similarity, 1.0f, 1e-5f)
        << "Identical gradients should have similarity 1.0";
}

TEST_F(MultiTaskLearningTest, GradientSimilarity_OppositeGradients_ReturnsNegativeOne) {
    // WHAT: Test mtl_gradient_similarity returns -1.0 for opposite gradients
    // WHY:  Verify detection of conflicting gradients
    // HOW:  Create opposite gradient vectors, compute similarity

    std::vector<float> grad1 = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> grad2 = {-1.0f, -2.0f, -3.0f, -4.0f, -5.0f};

    float similarity = mtl_gradient_similarity(grad1.data(), grad2.data(), grad1.size());

    EXPECT_NEAR(similarity, -1.0f, 1e-5f)
        << "Opposite gradients should have similarity -1.0";
}

TEST_F(MultiTaskLearningTest, GradientSimilarity_OrthogonalGradients_ReturnsZero) {
    // WHAT: Test mtl_gradient_similarity returns ~0 for orthogonal gradients
    // WHY:  Verify handling of independent gradient directions
    // HOW:  Create orthogonal vectors, compute similarity

    std::vector<float> grad1 = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> grad2 = {0.0f, 1.0f, 0.0f, 0.0f};

    float similarity = mtl_gradient_similarity(grad1.data(), grad2.data(), grad1.size());

    EXPECT_NEAR(similarity, 0.0f, 1e-5f)
        << "Orthogonal gradients should have similarity 0.0";
}

TEST_F(MultiTaskLearningTest, GradientSimilarity_NullGradients_ReturnsZero) {
    // WHAT: Test mtl_gradient_similarity handles null gradients
    // WHY:  Ensure proper error handling
    // HOW:  Call with NULL gradient pointers

    std::vector<float> grad = {1.0f, 2.0f, 3.0f};

    float sim1 = mtl_gradient_similarity(nullptr, grad.data(), grad.size());
    float sim2 = mtl_gradient_similarity(grad.data(), nullptr, grad.size());
    float sim3 = mtl_gradient_similarity(nullptr, nullptr, grad.size());

    EXPECT_FLOAT_EQ(sim1, 0.0f) << "Should return 0 for NULL grad1";
    EXPECT_FLOAT_EQ(sim2, 0.0f) << "Should return 0 for NULL grad2";
    EXPECT_FLOAT_EQ(sim3, 0.0f) << "Should return 0 for both NULL";
}

//=============================================================================
// Multi-Task Learning - Gradient Conflict Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, GradientsConflict_AlignedGradients_ReturnsFalse) {
    // WHAT: Test mtl_gradients_conflict returns false for aligned gradients
    // WHY:  Verify conflict detection for non-conflicting case
    // HOW:  Create aligned gradients, check conflict

    std::vector<float> grad1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> grad2 = {2.0f, 4.0f, 6.0f};  // Same direction, different magnitude

    bool conflict = mtl_gradients_conflict(grad1.data(), grad2.data(), grad1.size());

    EXPECT_FALSE(conflict) << "Aligned gradients should not conflict";
}

TEST_F(MultiTaskLearningTest, GradientsConflict_OppositeGradients_ReturnsTrue) {
    // WHAT: Test mtl_gradients_conflict returns true for opposite gradients
    // WHY:  Verify conflict detection for conflicting case
    // HOW:  Create opposite gradients, check conflict

    std::vector<float> grad1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> grad2 = {-1.0f, -2.0f, -3.0f};

    bool conflict = mtl_gradients_conflict(grad1.data(), grad2.data(), grad1.size());

    EXPECT_TRUE(conflict) << "Opposite gradients should conflict";
}

TEST_F(MultiTaskLearningTest, GradientsConflict_NullGradients_ReturnsFalse) {
    // WHAT: Test mtl_gradients_conflict handles null gradients
    // WHY:  Ensure proper error handling
    // HOW:  Call with NULL gradient pointers

    std::vector<float> grad = {1.0f, 2.0f, 3.0f};

    bool conflict = mtl_gradients_conflict(nullptr, grad.data(), grad.size());
    EXPECT_FALSE(conflict) << "Should return false for NULL gradient";
}

//=============================================================================
// Multi-Task Learning - Statistics Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, GetStats_InitialValues) {
    // WHAT: Test mtl_get_stats returns initialized statistics
    // WHY:  Verify statistics start at sensible defaults
    // HOW:  Create context, get stats, check values

    CreateDefaultContext();

    mtl_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero

    int ret = mtl_get_stats(ctx, &stats);
    EXPECT_EQ(ret, 0) << "mtl_get_stats should succeed";

    EXPECT_EQ(stats.total_steps, 0u) << "Initial total_steps should be 0";
    EXPECT_FLOAT_EQ(stats.avg_loss, 0.0f) << "Initial avg_loss should be 0";
}

TEST_F(MultiTaskLearningTest, GetStats_NullContext_ReturnsError) {
    // WHAT: Test mtl_get_stats rejects null context
    // WHY:  Ensure proper error handling
    // HOW:  Call get_stats with NULL context

    mtl_stats_t stats;
    int ret = mtl_get_stats(nullptr, &stats);
    EXPECT_NE(ret, 0) << "mtl_get_stats should reject NULL context";
}

TEST_F(MultiTaskLearningTest, GetStats_NullStats_ReturnsError) {
    // WHAT: Test mtl_get_stats rejects null stats pointer
    // WHY:  Ensure proper error handling
    // HOW:  Call get_stats with NULL stats

    CreateDefaultContext();

    int ret = mtl_get_stats(ctx, nullptr);
    EXPECT_NE(ret, 0) << "mtl_get_stats should reject NULL stats";
}

//=============================================================================
// Multi-Task Learning - Utility Tests
//=============================================================================

TEST_F(MultiTaskLearningTest, ArchitectureName_AllArchitectures_ReturnValidNames) {
    // WHAT: Test mtl_architecture_name returns valid names for all architectures
    // WHY:  Verify string conversion works for all enum values
    // HOW:  Iterate through all architectures and check non-null return

    for (int i = 0; i < MTL_ARCH_COUNT; i++) {
        const char* name = mtl_architecture_name(static_cast<mtl_architecture_t>(i));
        EXPECT_NE(name, nullptr) << "Architecture " << i << " should have a name";
        EXPECT_STRNE(name, "") << "Architecture " << i << " name should not be empty";
    }
}

TEST_F(MultiTaskLearningTest, ArchitectureName_InvalidArchitecture_ReturnsUnknown) {
    // WHAT: Test mtl_architecture_name handles invalid architecture value
    // WHY:  Ensure graceful handling of out-of-range values
    // HOW:  Call with value beyond MTL_ARCH_COUNT

    const char* name = mtl_architecture_name(static_cast<mtl_architecture_t>(MTL_ARCH_COUNT + 100));
    EXPECT_NE(name, nullptr) << "Invalid architecture should return non-null";
}

TEST_F(MultiTaskLearningTest, WeightStrategyName_AllStrategies_ReturnValidNames) {
    // WHAT: Test mtl_weight_strategy_name returns valid names for all strategies
    // WHY:  Verify string conversion works for all enum values
    // HOW:  Iterate through all weight strategies

    for (int i = 0; i < MTL_WEIGHT_COUNT; i++) {
        const char* name = mtl_weight_strategy_name(static_cast<mtl_weight_strategy_t>(i));
        EXPECT_NE(name, nullptr) << "Weight strategy " << i << " should have a name";
        EXPECT_STRNE(name, "") << "Weight strategy " << i << " name should not be empty";
    }
}

TEST_F(MultiTaskLearningTest, WeightStrategyName_InvalidStrategy_ReturnsUnknown) {
    // WHAT: Test mtl_weight_strategy_name handles invalid strategy value
    // WHY:  Ensure graceful handling of out-of-range values
    // HOW:  Call with value beyond MTL_WEIGHT_COUNT

    const char* name = mtl_weight_strategy_name(static_cast<mtl_weight_strategy_t>(MTL_WEIGHT_COUNT + 100));
    EXPECT_NE(name, nullptr) << "Invalid weight strategy should return non-null";
}

TEST_F(MultiTaskLearningTest, ValidateConfig_ValidConfig_ReturnsSuccess) {
    // WHAT: Test mtl_validate_config accepts valid configuration
    // WHY:  Verify validation passes for correct configs
    // HOW:  Get default config and validate

    int ret = mtl_default_config(&config);
    ASSERT_EQ(ret, 0);

    ret = mtl_validate_config(&config);
    EXPECT_EQ(ret, 0) << "Valid config should pass validation";
}

TEST_F(MultiTaskLearningTest, ValidateConfig_NullConfig_ReturnsError) {
    // WHAT: Test mtl_validate_config rejects null config
    // WHY:  Ensure proper error handling
    // HOW:  Call validate_config with NULL

    int ret = mtl_validate_config(nullptr);
    EXPECT_NE(ret, 0) << "mtl_validate_config should reject NULL config";
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

TEST_F(ContinualLearningTest, Integration_EWCStrategy_FullWorkflow) {
    // WHAT: Test complete EWC workflow
    // WHY:  Verify end-to-end EWC continual learning
    // HOW:  Create context, start task, end task, compute penalty

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.strategy = CL_STRATEGY_EWC;
    config.ewc.lambda = 100.0f;

    ctx = cl_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Start first task
    ret = cl_start_task(ctx, 0, "MNIST");
    ASSERT_EQ(ret, 0);

    // Simulate training (would normally include Fisher computation)
    // End task
    ret = cl_end_task(ctx);
    ASSERT_EQ(ret, 0);

    // Verify task count
    EXPECT_EQ(cl_get_num_tasks(ctx), 1u);

    // Start second task
    ret = cl_start_task(ctx, 1, "CIFAR10");
    ASSERT_EQ(ret, 0);

    // Compute EWC penalty (would be non-zero if Fisher was computed)
    std::vector<float> params(100, 0.5f);
    float penalty = cl_ewc_penalty(ctx, params.data(), params.size());

    // Penalty should be computable (value depends on Fisher computation)
    SUCCEED() << "EWC workflow completed successfully";
}

TEST_F(MultiTaskLearningTest, Integration_MultipleTasksWorkflow) {
    // WHAT: Test complete multi-task workflow
    // WHY:  Verify end-to-end MTL training setup
    // HOW:  Create context, register tasks, sample, check weights

    int ret = mtl_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.architecture = MTL_ARCH_HARD_SHARING;
    config.weighting = MTL_WEIGHT_UNCERTAINTY;
    config.gradient_method = MTL_GRAD_PCGRAD;

    ctx = mtl_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Register multiple tasks
    mtl_task_def_t task0 = CreateTaskDef(0, "ImageClassification", 1000, 1.0f);
    mtl_task_def_t task1 = CreateTaskDef(1, "ObjectDetection", 80, 0.5f);
    mtl_task_def_t task2 = CreateTaskDef(2, "SemanticSegmentation", 21, 0.75f);

    int idx0 = mtl_register_task(ctx, &task0);
    int idx1 = mtl_register_task(ctx, &task1);
    int idx2 = mtl_register_task(ctx, &task2);

    ASSERT_GE(idx0, 0);
    ASSERT_GE(idx1, 0);
    ASSERT_GE(idx2, 0);

    // Verify weights
    EXPECT_FLOAT_EQ(mtl_get_task_weight(ctx, 0), 1.0f);
    EXPECT_FLOAT_EQ(mtl_get_task_weight(ctx, 1), 0.5f);
    EXPECT_FLOAT_EQ(mtl_get_task_weight(ctx, 2), 0.75f);

    // Adjust weight
    ret = mtl_set_task_weight(ctx, 1, 1.5f);
    ASSERT_EQ(ret, 0);
    EXPECT_FLOAT_EQ(mtl_get_task_weight(ctx, 1), 1.5f);

    // Deactivate a task
    ret = mtl_set_task_active(ctx, 2, false);
    ASSERT_EQ(ret, 0);

    // Get stats
    mtl_stats_t stats;
    ret = mtl_get_stats(ctx, &stats);
    ASSERT_EQ(ret, 0);

    SUCCEED() << "MTL workflow completed successfully";
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(ContinualLearningTest, EdgeCase_MaxTasks) {
    // WHAT: Test handling near maximum task limit
    // WHY:  Verify boundary condition handling
    // HOW:  Configure with small max_tasks and test limit

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.max_tasks = 3;

    ctx = cl_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Should be able to create up to max_tasks
    for (uint32_t i = 0; i < config.max_tasks; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Task_%u", i);
        ret = cl_start_task(ctx, i, name);
        EXPECT_EQ(ret, 0) << "Should be able to start task " << i;
        ret = cl_end_task(ctx);
        EXPECT_EQ(ret, 0) << "Should be able to end task " << i;
    }

    EXPECT_EQ(cl_get_num_tasks(ctx), config.max_tasks);
}

TEST_F(MultiTaskLearningTest, EdgeCase_ZeroLengthGradient) {
    // WHAT: Test gradient similarity with zero-length vectors
    // WHY:  Verify handling of degenerate case
    // HOW:  Call with num_params = 0

    std::vector<float> grad1 = {1.0f};
    std::vector<float> grad2 = {1.0f};

    float similarity = mtl_gradient_similarity(grad1.data(), grad2.data(), 0);

    // Should handle gracefully (likely return 0 or NaN protection)
    EXPECT_TRUE(std::isfinite(similarity) || similarity == 0.0f)
        << "Zero-length gradient similarity should be handled gracefully";
}

TEST_F(MultiTaskLearningTest, EdgeCase_ZeroGradient) {
    // WHAT: Test gradient similarity with zero-magnitude gradient
    // WHY:  Verify handling of zero vector
    // HOW:  Create zero gradient vector

    std::vector<float> grad1 = {0.0f, 0.0f, 0.0f};
    std::vector<float> grad2 = {1.0f, 2.0f, 3.0f};

    float similarity = mtl_gradient_similarity(grad1.data(), grad2.data(), grad1.size());

    // Should handle gracefully (division by zero protection)
    EXPECT_TRUE(std::isfinite(similarity) || std::isnan(similarity))
        << "Zero gradient similarity should not cause crash";
}

//=============================================================================
// Strategy-Specific Tests
//=============================================================================

TEST_F(ContinualLearningTest, Strategy_Replay_BufferConfig) {
    // WHAT: Test replay strategy configuration
    // WHY:  Verify replay buffer settings are applied
    // HOW:  Configure with replay strategy and check buffer

    int ret = cl_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.strategy = CL_STRATEGY_REPLAY;
    config.replay.strategy = CL_REPLAY_RANDOM;
    config.replay.buffer_size = 1000;
    config.replay.replay_ratio = 0.2f;

    ctx = cl_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Buffer should start empty
    EXPECT_EQ(cl_replay_buffer_size(ctx), 0u);

    SUCCEED() << "Replay strategy configuration successful";
}

TEST_F(MultiTaskLearningTest, Strategy_Uncertainty_Config) {
    // WHAT: Test uncertainty weighting configuration
    // WHY:  Verify uncertainty weighting settings
    // HOW:  Configure with uncertainty weighting

    int ret = mtl_default_config(&config);
    ASSERT_EQ(ret, 0);

    config.weighting = MTL_WEIGHT_UNCERTAINTY;
    config.uncertainty.prior_strength = 0.01f;
    config.uncertainty.learn_log_vars = true;

    ctx = mtl_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Register task and verify it works with uncertainty weighting
    mtl_task_def_t task = CreateTaskDef(0, "Test", 10, 1.0f);
    int idx = mtl_register_task(ctx, &task);
    EXPECT_GE(idx, 0);

    SUCCEED() << "Uncertainty weighting configuration successful";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
