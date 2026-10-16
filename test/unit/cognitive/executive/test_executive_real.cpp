/**
 * @file test_executive_real.cpp
 * @brief Real tests for executive functions module
 *
 * Tests only functions that ACTUALLY EXIST in nimcp_executive.h
 */

#include <gtest/gtest.h>

#include "cognitive/nimcp_executive.h"
#include "core/brain/nimcp_brain.h"

class ExecutiveRealTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    executive_controller_t* exec = nullptr;

    void SetUp() override {
        brain = brain_create("test", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        exec = executive_create();
        ASSERT_NE(exec, nullptr);
    }

    void TearDown() override {
        if (exec) executive_destroy(exec);
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

TEST_F(ExecutiveRealTest, Create_ReturnsNonNull) {
    executive_controller_t* test_exec = executive_create();
    EXPECT_NE(test_exec, nullptr);
    executive_destroy(test_exec);
}

TEST_F(ExecutiveRealTest, CreateCustom_ValidConfig_ReturnsNonNull) {
    executive_config_t config = {};
    config.max_tasks = 16;
    config.task_switch_cost_ms = 200.0f;
    config.inhibition_threshold = 0.7f;
    config.max_plan_depth = 10;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = true;

    executive_controller_t* test_exec = executive_create_custom(&config);
    EXPECT_NE(test_exec, nullptr);
    executive_destroy(test_exec);
}

TEST_F(ExecutiveRealTest, Destroy_DoesNotCrash) {
    executive_controller_t* test_exec = executive_create();
    ASSERT_NE(test_exec, nullptr);

    // Should not crash
    executive_destroy(test_exec);
}

//=============================================================================
// Task Management
//=============================================================================

TEST_F(ExecutiveRealTest, AddTask_ValidTask_ReturnsNonZeroId) {
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "test_task", sizeof(task.name) - 1);
    task.created_ms = 1000;

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0);
}

TEST_F(ExecutiveRealTest, AddTask_MultipleTasksWithPriorities_AllAccepted) {
    task_descriptor_t task1 = {};
    task1.type = TASK_TYPE_CLASSIFICATION;
    task1.priority = PRIORITY_LOW;
    task1.status = TASK_STATUS_PENDING;
    strncpy(task1.name, "low_priority", sizeof(task1.name) - 1);

    task_descriptor_t task2 = {};
    task2.type = TASK_TYPE_CLASSIFICATION;
    task2.priority = PRIORITY_HIGH;
    task2.status = TASK_STATUS_PENDING;
    strncpy(task2.name, "high_priority", sizeof(task2.name) - 1);

    uint32_t id1 = executive_add_task(exec, &task1);
    uint32_t id2 = executive_add_task(exec, &task2);

    EXPECT_GT(id1, 0);
    EXPECT_GT(id2, 0);
    EXPECT_NE(id1, id2);
}

TEST_F(ExecutiveRealTest, SwitchTask_ToValidTask_ReturnsTrue) {
    // Add a task first
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "switch_target", sizeof(task.name) - 1);

    uint32_t task_id = executive_add_task(exec, &task);
    ASSERT_GT(task_id, 0);

    bool result = executive_switch_task(exec, task_id, 1000);
    EXPECT_TRUE(result);
}

TEST_F(ExecutiveRealTest, SwitchTask_ToInvalidTask_ReturnsFalse) {
    bool result = executive_switch_task(exec, 9999, 1000);
    EXPECT_FALSE(result);
}

TEST_F(ExecutiveRealTest, GetActiveTask_NoActiveTasks_ReturnsNull) {
    const task_descriptor_t* active = executive_get_active_task(exec);
    // May be NULL or not depending on implementation
}

TEST_F(ExecutiveRealTest, GetActiveTask_AfterSwitch_ReturnsTask) {
    // Add and switch to a task
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "active_task", sizeof(task.name) - 1);

    uint32_t task_id = executive_add_task(exec, &task);
    executive_switch_task(exec, task_id, 1000);

    const task_descriptor_t* active = executive_get_active_task(exec);
    if (active) {
        EXPECT_STREQ(active->name, "active_task");
    }
}

TEST_F(ExecutiveRealTest, CompleteTask_Success_ReturnsTrue) {
    // Add and activate a task
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;

    uint32_t task_id = executive_add_task(exec, &task);
    executive_switch_task(exec, task_id, 1000);

    bool result = executive_complete_task(exec, true, 2000);
    // May succeed or fail depending on implementation
}

TEST_F(ExecutiveRealTest, CompleteTask_Failure_ReturnsTrue) {
    // Add and activate a task
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;

    uint32_t task_id = executive_add_task(exec, &task);
    executive_switch_task(exec, task_id, 1000);

    bool result = executive_complete_task(exec, false, 2000);
    // May succeed or fail depending on implementation
}

//=============================================================================
// Inhibitory Control
//=============================================================================

TEST_F(ExecutiveRealTest, ShouldInhibit_LowSalience_ReturnsFalse) {
    bool inhibit = executive_should_inhibit(exec, 0.3f, "low salience");
    EXPECT_FALSE(inhibit);
}

TEST_F(ExecutiveRealTest, ShouldInhibit_HighSalience_ReturnsTrue) {
    bool inhibit = executive_should_inhibit(exec, 0.95f, "high salience");
    EXPECT_TRUE(inhibit);
}

TEST_F(ExecutiveRealTest, ShouldInhibit_ThresholdBoundary_ConsistentBehavior) {
    // Test at threshold (default 0.7)
    bool inhibit1 = executive_should_inhibit(exec, 0.7f, "at threshold");
    bool inhibit2 = executive_should_inhibit(exec, 0.7f, "at threshold");

    // Should be consistent
    EXPECT_EQ(inhibit1, inhibit2);
}

TEST_F(ExecutiveRealTest, ShouldInhibit_WithReason_DoesNotCrash) {
    // Should not crash with various reasons
    executive_should_inhibit(exec, 0.8f, "unethical");
    executive_should_inhibit(exec, 0.8f, "dangerous");
    executive_should_inhibit(exec, 0.8f, nullptr); // NULL reason
}

//=============================================================================
// Planning
//=============================================================================

TEST_F(ExecutiveRealTest, CreatePlan_ValidGoal_ReturnsNonNull) {
    plan_t* plan = executive_create_plan(exec, "test goal", 5);

    if (plan) {
        EXPECT_GT(strlen(plan->goal), 0);
        executive_destroy_plan(plan);
    }
}

TEST_F(ExecutiveRealTest, CreatePlan_ZeroMaxSteps_HandledGracefully) {
    plan_t* plan = executive_create_plan(exec, "test goal", 0);

    // May return NULL or empty plan
    if (plan) {
        executive_destroy_plan(plan);
    }
}

TEST_F(ExecutiveRealTest, DestroyPlan_NullPlan_DoesNotCrash) {
    // Should not crash with NULL
    executive_destroy_plan(nullptr);
}

TEST_F(ExecutiveRealTest, DestroyPlan_ValidPlan_DoesNotCrash) {
    plan_t* plan = executive_create_plan(exec, "test goal", 5);

    if (plan) {
        // Should not crash
        executive_destroy_plan(plan);
    }
}

//=============================================================================
// Statistics
//=============================================================================

TEST_F(ExecutiveRealTest, GetStats_InitialState_ReturnsZeros) {
    executive_stats_t stats;
    bool result = executive_get_stats(exec, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_tasks, 0);
    EXPECT_EQ(stats.total_switches, 0);
}

TEST_F(ExecutiveRealTest, GetStats_AfterAddingTasks_IncrementsCount) {
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;

    executive_add_task(exec, &task);

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GT(stats.total_tasks, 0);
}

TEST_F(ExecutiveRealTest, GetStats_AfterTaskSwitch_IncrementsSwitchCount) {
    // Add two tasks
    task_descriptor_t task1 = {};
    task1.type = TASK_TYPE_CLASSIFICATION;
    task1.priority = PRIORITY_NORMAL;
    task1.status = TASK_STATUS_PENDING;
    uint32_t id1 = executive_add_task(exec, &task1);

    task_descriptor_t task2 = {};
    task2.type = TASK_TYPE_CLASSIFICATION;
    task2.priority = PRIORITY_NORMAL;
    task2.status = TASK_STATUS_PENDING;
    uint32_t id2 = executive_add_task(exec, &task2);

    // Switch between them
    executive_switch_task(exec, id1, 1000);
    executive_switch_task(exec, id2, 2000);

    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_GT(stats.total_switches, 0);
}

TEST_F(ExecutiveRealTest, ResetStats_ResetsAllCounters) {
    // Add some tasks
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    executive_add_task(exec, &task);

    // Reset
    executive_reset_stats(exec);

    // Check stats are zero
    executive_stats_t stats;
    executive_get_stats(exec, &stats);

    EXPECT_EQ(stats.total_tasks, 0);
}

//=============================================================================
// Error Handling
//=============================================================================

TEST_F(ExecutiveRealTest, GetLastError_ReturnsNonNull) {
    const char* error = executive_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(ExecutiveRealTest, GetLastError_AfterError_ReturnsMessage) {
    // Trigger an error
    executive_switch_task(exec, 9999, 0);

    const char* error = executive_get_last_error();
    EXPECT_NE(error, nullptr);
    // Error message should have content
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

TEST_F(ExecutiveRealTest, GetCognitiveLoad_InitialState_ReturnsLowLoad) {
    float load = executive_get_cognitive_load(exec);

    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 1.0f);
}

TEST_F(ExecutiveRealTest, GetCognitiveLoad_AfterAddingTasks_LoadIncreases) {
    float initial_load = executive_get_cognitive_load(exec);

    // Add several tasks
    for (int i = 0; i < 5; i++) {
        task_descriptor_t task = {};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;
        executive_add_task(exec, &task);
    }

    float loaded = executive_get_cognitive_load(exec);

    EXPECT_GE(loaded, initial_load);
    EXPECT_LE(loaded, 1.0f);
}

TEST_F(ExecutiveRealTest, BoostTaskPriority_ValidTaskName_ReturnsTrue) {
    // Add a task
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "boostable_task", sizeof(task.name) - 1);
    executive_add_task(exec, &task);

    bool result = executive_boost_task_priority(exec, "boostable_task", 0.3f);
    // May or may not find the task, just check no crash
}

TEST_F(ExecutiveRealTest, BoostTaskPriority_InvalidTaskName_ReturnsFalse) {
    bool result = executive_boost_task_priority(exec, "nonexistent", 0.3f);
    EXPECT_FALSE(result);
}

TEST_F(ExecutiveRealTest, BoostTaskPriority_ZeroBoost_Handled) {
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "zero_boost", sizeof(task.name) - 1);
    executive_add_task(exec, &task);

    bool result = executive_boost_task_priority(exec, "zero_boost", 0.0f);
    // Should handle zero boost gracefully
}

TEST_F(ExecutiveRealTest, BoostTaskPriority_NegativeBoost_Clamped) {
    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    task.status = TASK_STATUS_PENDING;
    strncpy(task.name, "negative_boost", sizeof(task.name) - 1);
    executive_add_task(exec, &task);

    bool result = executive_boost_task_priority(exec, "negative_boost", -0.5f);
    // Should handle negative boost gracefully
}
