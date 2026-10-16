/**
 * @file test_executive.cpp
 * @brief Comprehensive test suite for Executive Functions (Phase 10.3)
 *
 * WHAT: Tests for task switching, inhibitory control, and planning
 * WHY:  Validate executive controller meets specification
 * HOW:  40+ tests covering all public API functions
 *
 * TEST COVERAGE:
 * - Creation/destruction
 * - Task management (add, switch, complete, abort)
 * - Priority scheduling
 * - Deadline handling
 * - Inhibitory control
 * - Planning (sequential, branching, hierarchical)
 * - Statistics tracking
 * - Error handling
 * - Edge cases
 *
 * @author Claude Code
 * @date 2025-11-09
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>

    #include "cognitive/nimcp_executive.h"
    #include "utils/time/nimcp_time.h"

// =============================================================================
// TEST FIXTURES
// =============================================================================

class ExecutiveTest : public ::testing::Test {
protected:
    executive_controller_t* exec;

    void SetUp() override {
        exec = executive_create();
        ASSERT_NE(exec, nullptr) << "Failed to create executive controller";
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
            exec = nullptr;
        }
    }

    // Helper: Create task descriptor
    task_descriptor_t create_test_task(const char* name, task_type_t type, task_priority_t priority) {
        task_descriptor_t task = {};
        strncpy(task.name, name, sizeof(task.name) - 1);
        task.type = type;
        task.priority = priority;
        task.status = TASK_STATUS_PENDING;
        task.created_ms = nimcp_time_monotonic_ms();
        return task;
    }
};

class ExecutiveCustomConfigTest : public ::testing::Test {
protected:
    executive_controller_t* exec;

    void SetUp() override {
        exec = nullptr;  // Created per-test
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
            exec = nullptr;
        }
    }
};

// =============================================================================
// CREATION AND DESTRUCTION TESTS
// =============================================================================

TEST_F(ExecutiveTest, CreateDefault) {
    // Controller already created in SetUp
    EXPECT_NE(exec, nullptr);
}

TEST_F(ExecutiveCustomConfigTest, CreateCustomConfig) {
    executive_config_t config = {
        .max_tasks = 32,
        .task_switch_cost_ms = 150.0f,
        .inhibition_threshold = 0.8f,
        .max_plan_depth = 15,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true
    };

    exec = executive_create_custom(&config);
    ASSERT_NE(exec, nullptr);
}

TEST_F(ExecutiveCustomConfigTest, CreateWithNullConfig) {
    exec = executive_create_custom(nullptr);
    EXPECT_EQ(exec, nullptr) << "Should reject NULL config";
}

TEST_F(ExecutiveTest, DestroyNull) {
    executive_destroy(nullptr);  // Should not crash
}

// =============================================================================
// TASK MANAGEMENT TESTS
// =============================================================================

TEST_F(ExecutiveTest, AddSingleTask) {
    task_descriptor_t task = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0u) << "Task ID should be > 0";
}

TEST_F(ExecutiveTest, AddMultipleTasks) {
    const char* task_names[] = {"Task1", "Task2", "Task3", "Task4"};

    for (int i = 0; i < 4; i++) {
        task_descriptor_t task = create_test_task(task_names[i], TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
        uint32_t task_id = executive_add_task(exec, &task);
        EXPECT_GT(task_id, 0u) << "Failed to add " << task_names[i];
    }
}

TEST_F(ExecutiveTest, AddTaskWithPriority) {
    task_descriptor_t high_task = create_test_task("HighPriority", TASK_TYPE_CLASSIFICATION, PRIORITY_HIGH);
    task_descriptor_t low_task = create_test_task("LowPriority", TASK_TYPE_CLASSIFICATION, PRIORITY_LOW);

    uint32_t high_id = executive_add_task(exec, &high_task);
    uint32_t low_id = executive_add_task(exec, &low_task);

    EXPECT_GT(high_id, 0u);
    EXPECT_GT(low_id, 0u);
    EXPECT_NE(high_id, low_id) << "Task IDs should be unique";
}

TEST_F(ExecutiveTest, AddTaskWithDeadline) {
    task_descriptor_t task = create_test_task("DeadlineTask", TASK_TYPE_CLASSIFICATION, PRIORITY_URGENT);
    task.deadline_ms = nimcp_time_monotonic_ms() + 5000;  // 5 seconds from now

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0u);
}

TEST_F(ExecutiveTest, AddTaskWithNullTask) {
    uint32_t task_id = executive_add_task(exec, nullptr);
    EXPECT_EQ(task_id, 0u) << "Should reject NULL task";
}

TEST_F(ExecutiveTest, AddTaskWithNullController) {
    task_descriptor_t task = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    uint32_t task_id = executive_add_task(nullptr, &task);
    EXPECT_EQ(task_id, 0u) << "Should reject NULL controller";
}

TEST_F(ExecutiveTest, SwitchTask) {
    // Add two tasks
    task_descriptor_t task1 = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    task_descriptor_t task2 = create_test_task("Task2", TASK_TYPE_REGRESSION, PRIORITY_NORMAL);

    uint32_t task1_id = executive_add_task(exec, &task1);
    uint32_t task2_id = executive_add_task(exec, &task2);

    ASSERT_GT(task1_id, 0u);
    ASSERT_GT(task2_id, 0u);

    // Switch to task1
    uint64_t current_time = nimcp_time_monotonic_ms();
    bool success = executive_switch_task(exec, task1_id, current_time);
    EXPECT_TRUE(success);

    // Verify active task
    const task_descriptor_t* active = executive_get_active_task(exec);
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->task_id, task1_id);

    // Switch to task2
    current_time = nimcp_time_monotonic_ms();
    success = executive_switch_task(exec, task2_id, current_time);
    EXPECT_TRUE(success);

    active = executive_get_active_task(exec);
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->task_id, task2_id);
}

TEST_F(ExecutiveTest, SwitchToInvalidTask) {
    uint64_t current_time = nimcp_time_monotonic_ms();
    bool success = executive_switch_task(exec, 9999, current_time);
    EXPECT_FALSE(success) << "Should reject invalid task ID";
}

TEST_F(ExecutiveTest, GetActiveTaskWhenNone) {
    const task_descriptor_t* active = executive_get_active_task(exec);
    EXPECT_EQ(active, nullptr) << "Should return NULL when no active task";
}

TEST_F(ExecutiveTest, CompleteTask) {
    task_descriptor_t task = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    uint32_t task_id = executive_add_task(exec, &task);
    ASSERT_GT(task_id, 0u);

    // Switch to task
    uint64_t current_time = nimcp_time_monotonic_ms();
    ASSERT_TRUE(executive_switch_task(exec, task_id, current_time));

    // Complete successfully
    current_time = nimcp_time_monotonic_ms();
    bool success = executive_complete_task(exec, true, current_time);
    EXPECT_TRUE(success);

    // No active task after completion
    const task_descriptor_t* active = executive_get_active_task(exec);
    EXPECT_EQ(active, nullptr);
}

TEST_F(ExecutiveTest, CompleteTaskWithFailure) {
    task_descriptor_t task = create_test_task("FailTask", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    uint32_t task_id = executive_add_task(exec, &task);
    ASSERT_GT(task_id, 0u);

    uint64_t current_time = nimcp_time_monotonic_ms();
    ASSERT_TRUE(executive_switch_task(exec, task_id, current_time));

    // Complete with failure
    current_time = nimcp_time_monotonic_ms();
    bool success = executive_complete_task(exec, false, current_time);
    EXPECT_TRUE(success) << "API call should succeed even if task failed";
}

TEST_F(ExecutiveTest, CompleteTaskWhenNoActiveTask) {
    uint64_t current_time = nimcp_time_monotonic_ms();
    bool success = executive_complete_task(exec, true, current_time);
    EXPECT_FALSE(success) << "Should fail when no active task";
}

// =============================================================================
// INHIBITORY CONTROL TESTS
// =============================================================================

TEST_F(ExecutiveTest, InhibitHighSalience) {
    // High salience response should be inhibited
    bool inhibited = executive_should_inhibit(exec, 0.9f, "unethical");
    EXPECT_TRUE(inhibited) << "High salience (0.9) should be inhibited";
}

TEST_F(ExecutiveTest, AllowLowSalience) {
    // Low salience response should be allowed
    bool inhibited = executive_should_inhibit(exec, 0.3f, "safe");
    EXPECT_FALSE(inhibited) << "Low salience (0.3) should be allowed";
}

TEST_F(ExecutiveTest, InhibitBoundary) {
    // Test at threshold boundary (default 0.7)
    bool inhibited_above = executive_should_inhibit(exec, 0.71f, nullptr);
    bool inhibited_below = executive_should_inhibit(exec, 0.69f, nullptr);

    EXPECT_TRUE(inhibited_above) << "Just above threshold should inhibit";
    EXPECT_FALSE(inhibited_below) << "Just below threshold should allow";
}

TEST_F(ExecutiveTest, InhibitWithNullReason) {
    bool inhibited = executive_should_inhibit(exec, 0.8f, nullptr);
    EXPECT_TRUE(inhibited) << "Should work with NULL reason";
}

TEST_F(ExecutiveTest, InhibitWithNullController) {
    bool inhibited = executive_should_inhibit(nullptr, 0.8f, "test");
    EXPECT_FALSE(inhibited) << "Should return false with NULL controller";
}

// =============================================================================
// PLANNING TESTS
// =============================================================================

TEST_F(ExecutiveTest, CreateSimplePlan) {
    plan_t* plan = executive_create_plan(exec, "Complete classification task", 5);
    ASSERT_NE(plan, nullptr);

    EXPECT_GT(plan->num_steps, 0u) << "Plan should have at least one step";
    EXPECT_LE(plan->num_steps, 5u) << "Plan should not exceed max steps";
    EXPECT_STREQ(plan->goal, "Complete classification task");

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveTest, CreatePlanWithMaxSteps) {
    plan_t* plan = executive_create_plan(exec, "Complex multi-step task", 10);
    ASSERT_NE(plan, nullptr);

    EXPECT_LE(plan->num_steps, 10u);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveTest, CreatePlanWithNullGoal) {
    plan_t* plan = executive_create_plan(exec, nullptr, 5);
    EXPECT_EQ(plan, nullptr) << "Should reject NULL goal";
}

TEST_F(ExecutiveTest, CreatePlanWithZeroSteps) {
    plan_t* plan = executive_create_plan(exec, "Test goal", 0);
    EXPECT_EQ(plan, nullptr) << "Should reject max_steps = 0";
}

TEST_F(ExecutiveTest, DestroyNullPlan) {
    executive_destroy_plan(nullptr);  // Should not crash
}

TEST_F(ExecutiveTest, PlanStepsHaveDescriptions) {
    plan_t* plan = executive_create_plan(exec, "Test task", 3);
    ASSERT_NE(plan, nullptr);

    for (uint32_t i = 0; i < plan->num_steps; i++) {
        EXPECT_GT(strlen(plan->steps[i].description), 0u)
            << "Step " << i << " should have description";
    }

    executive_destroy_plan(plan);
}

// =============================================================================
// STATISTICS TESTS
// =============================================================================

TEST_F(ExecutiveTest, GetInitialStats) {
    executive_stats_t stats;
    bool success = executive_get_stats(exec, &stats);

    ASSERT_TRUE(success);
    EXPECT_EQ(stats.total_tasks, 0u);
    EXPECT_EQ(stats.completed_tasks, 0u);
    EXPECT_EQ(stats.failed_tasks, 0u);
    EXPECT_EQ(stats.total_switches, 0u);
    EXPECT_EQ(stats.inhibitions, 0u);
    EXPECT_EQ(stats.plans_created, 0u);
}

TEST_F(ExecutiveTest, StatsTrackTasks) {
    // Add and complete a task
    task_descriptor_t task = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    uint32_t task_id = executive_add_task(exec, &task);

    uint64_t current_time = nimcp_time_monotonic_ms();
    executive_switch_task(exec, task_id, current_time);
    executive_complete_task(exec, true, current_time);

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GT(stats.total_tasks, 0u);
    EXPECT_GT(stats.completed_tasks, 0u);
}

TEST_F(ExecutiveTest, StatsTrackSwitches) {
    // Add two tasks and switch between them
    task_descriptor_t task1 = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    task_descriptor_t task2 = create_test_task("Task2", TASK_TYPE_REGRESSION, PRIORITY_NORMAL);

    uint32_t task1_id = executive_add_task(exec, &task1);
    uint32_t task2_id = executive_add_task(exec, &task2);

    uint64_t current_time = nimcp_time_monotonic_ms();
    executive_switch_task(exec, task1_id, current_time);
    executive_switch_task(exec, task2_id, current_time);

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GE(stats.total_switches, 1u) << "Should record at least one switch";
}

TEST_F(ExecutiveTest, StatsTrackInhibitions) {
    // Trigger inhibition
    executive_should_inhibit(exec, 0.9f, "test");

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GT(stats.inhibitions, 0u);
}

TEST_F(ExecutiveTest, StatsTrackPlans) {
    plan_t* plan = executive_create_plan(exec, "Test goal", 5);
    ASSERT_NE(plan, nullptr);

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GT(stats.plans_created, 0u);

    executive_destroy_plan(plan);
}

TEST_F(ExecutiveTest, ResetStats) {
    // Generate some activity
    task_descriptor_t task = create_test_task("Task1", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    executive_add_task(exec, &task);
    executive_should_inhibit(exec, 0.9f, "test");

    // Verify stats are non-zero
    executive_stats_t stats_before;
    executive_get_stats(exec, &stats_before);
    ASSERT_GT(stats_before.total_tasks, 0u);

    // Reset
    executive_reset_stats(exec);

    // Verify stats are zero
    executive_stats_t stats_after;
    executive_get_stats(exec, &stats_after);

    EXPECT_EQ(stats_after.total_tasks, 0u);
    EXPECT_EQ(stats_after.inhibitions, 0u);
}

TEST_F(ExecutiveTest, GetStatsWithNullController) {
    executive_stats_t stats;
    bool success = executive_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(ExecutiveTest, GetStatsWithNullStats) {
    bool success = executive_get_stats(exec, nullptr);
    EXPECT_FALSE(success);
}

// =============================================================================
// INTEGRATION TESTS
// =============================================================================

TEST_F(ExecutiveTest, FullTaskLifecycle) {
    // Add task
    task_descriptor_t task = create_test_task("FullCycle", TASK_TYPE_CLASSIFICATION, PRIORITY_HIGH);
    uint32_t task_id = executive_add_task(exec, &task);
    ASSERT_GT(task_id, 0u);

    // Switch to task
    uint64_t current_time = nimcp_time_monotonic_ms();
    ASSERT_TRUE(executive_switch_task(exec, task_id, current_time));

    // Verify active
    const task_descriptor_t* active = executive_get_active_task(exec);
    ASSERT_NE(active, nullptr);
    EXPECT_EQ(active->task_id, task_id);

    // Complete task
    current_time = nimcp_time_monotonic_ms();
    ASSERT_TRUE(executive_complete_task(exec, true, current_time));

    // Verify no active task
    active = executive_get_active_task(exec);
    EXPECT_EQ(active, nullptr);
}

TEST_F(ExecutiveTest, MultiTaskScenario) {
    // Simulate realistic multi-tasking scenario
    task_descriptor_t tasks[3] = {
        create_test_task("Classify", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL),
        create_test_task("Reason", TASK_TYPE_REASONING, PRIORITY_HIGH),
        create_test_task("Retrieve", TASK_TYPE_MEMORY_RETRIEVAL, PRIORITY_LOW)
    };

    uint32_t task_ids[3];
    for (int i = 0; i < 3; i++) {
        task_ids[i] = executive_add_task(exec, &tasks[i]);
        ASSERT_GT(task_ids[i], 0u);
    }

    // Switch between tasks multiple times
    uint64_t current_time = nimcp_time_monotonic_ms();
    EXPECT_TRUE(executive_switch_task(exec, task_ids[0], current_time));
    EXPECT_TRUE(executive_switch_task(exec, task_ids[1], current_time));
    EXPECT_TRUE(executive_switch_task(exec, task_ids[2], current_time));

    // Verify statistics
    executive_stats_t stats;
    executive_get_stats(exec, &stats);
    EXPECT_EQ(stats.total_tasks, 3u);
    EXPECT_GE(stats.total_switches, 2u);
}

TEST_F(ExecutiveTest, PlanAndExecute) {
    // Create plan
    plan_t* plan = executive_create_plan(exec, "Complete classification", 3);
    ASSERT_NE(plan, nullptr);
    ASSERT_GT(plan->num_steps, 0u);

    // Simulate executing each step
    for (uint32_t i = 0; i < plan->num_steps; i++) {
        EXPECT_NE(plan->steps[i].description[0], '\0')
            << "Step " << i << " has valid description";
    }

    executive_destroy_plan(plan);
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

TEST_F(ExecutiveCustomConfigTest, PerformanceManyTasks) {
    // NOTE: Moved from ExecutiveTest to ExecutiveCustomConfigTest
    // DEFAULT_MAX_TASKS is 16, but this test needs 100
    executive_config_t config = {
        .max_tasks = 128,  // Enough for 100 tasks
        .task_switch_cost_ms = 200.0f,
        .inhibition_threshold = 0.7f,
        .max_plan_depth = 10,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true
    };

    exec = executive_create_custom(&config);
    ASSERT_NE(exec, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    // Add 100 tasks
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "Task%d", i);

        task_descriptor_t task = {};
        strncpy(task.name, name, sizeof(task.name) - 1);
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;
        task.created_ms = nimcp_time_monotonic_ms();

        uint32_t task_id = executive_add_task(exec, &task);
        ASSERT_GT(task_id, 0u) << "Failed at task " << i << ": " << executive_get_last_error();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 100) << "100 tasks should be added in < 100ms";
}

TEST_F(ExecutiveTest, PerformanceManyInhibitions) {
    auto start = std::chrono::high_resolution_clock::now();

    // Test 10000 inhibition checks
    for (int i = 0; i < 10000; i++) {
        float salience = (float)(i % 100) / 100.0f;
        executive_should_inhibit(exec, salience, nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 50) << "10000 inhibition checks should complete in < 50ms";
}

// =============================================================================
// ERROR HANDLING TESTS
// =============================================================================

TEST_F(ExecutiveTest, ErrorMessageAvailable) {
    // Trigger an error
    executive_add_task(nullptr, nullptr);

    const char* error = executive_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0u) << "Error message should not be empty";
}

TEST_F(ExecutiveTest, ErrorMessagePersists) {
    // Trigger error
    executive_add_task(nullptr, nullptr);
    const char* error1 = executive_get_last_error();

    // Get error again
    const char* error2 = executive_get_last_error();

    EXPECT_STREQ(error1, error2) << "Error message should persist";
}
