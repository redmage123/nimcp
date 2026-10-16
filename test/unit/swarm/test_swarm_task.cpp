//=============================================================================
// test_swarm_task.cpp - Unit Tests for Swarm Task Allocation System
//=============================================================================
/**
 * @file test_swarm_task.cpp
 * @brief Comprehensive unit tests for task manager, queue, and scheduler
 *
 * WHAT: Test swarm task allocation system
 * WHY:  Verify task lifecycle, queue operations, and scheduling algorithms
 * HOW:  GTest fixtures with mock agents and tasks
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <set>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_task.h"
#include "swarm/nimcp_swarm_task_queue.h"
#include "swarm/nimcp_swarm_task_scheduler.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Task Manager Test Fixture
 */
class TaskManagerTest : public ::testing::Test {
protected:
    swarm_task_manager_t* manager = nullptr;

    void SetUp() override {
        swarm_task_manager_config_t config;
        swarm_task_manager_default_config(&config);
        config.enable_bio_async = false;  // Disable for unit tests
        manager = swarm_task_manager_create(&config);
        ASSERT_NE(manager, nullptr);
    }

    void TearDown() override {
        if (manager) {
            swarm_task_manager_destroy(manager);
            manager = nullptr;
        }
    }
};

/**
 * @brief Task Queue Test Fixture
 */
class TaskQueueTest : public ::testing::Test {
protected:
    swarm_task_queue_t* queue = nullptr;
    static constexpr uint32_t TEST_AGENT_ID = 42;

    void SetUp() override {
        swarm_task_queue_config_t config;
        swarm_task_queue_default_config(&config);
        queue = swarm_task_queue_create(TEST_AGENT_ID, &config);
        ASSERT_NE(queue, nullptr);
    }

    void TearDown() override {
        if (queue) {
            swarm_task_queue_destroy(queue);
            queue = nullptr;
        }
    }
};

/**
 * @brief Task Scheduler Test Fixture
 */
class TaskSchedulerTest : public ::testing::Test {
protected:
    swarm_task_manager_t* manager = nullptr;
    swarm_task_scheduler_t* scheduler = nullptr;

    void SetUp() override {
        // Create task manager
        swarm_task_manager_config_t mgr_config;
        swarm_task_manager_default_config(&mgr_config);
        mgr_config.enable_bio_async = false;
        manager = swarm_task_manager_create(&mgr_config);
        ASSERT_NE(manager, nullptr);

        // Create scheduler
        swarm_scheduler_config_t sched_config;
        swarm_scheduler_default_config(&sched_config);
        sched_config.enable_bio_async = false;
        scheduler = swarm_scheduler_create(manager, &sched_config);
        ASSERT_NE(scheduler, nullptr);
    }

    void TearDown() override {
        if (scheduler) {
            swarm_scheduler_destroy(scheduler);
            scheduler = nullptr;
        }
        if (manager) {
            swarm_task_manager_destroy(manager);
            manager = nullptr;
        }
    }

    // Helper to register a test agent
    void RegisterTestAgent(uint32_t agent_id, uint32_t capabilities = 0xFFFF) {
        swarm_agent_profile_t profile = {};
        profile.agent_id = agent_id;
        profile.capabilities_mask = capabilities;
        profile.is_available = true;
        profile.energy_level = 1.0f;
        profile.max_concurrent_tasks = 10;
        for (int i = 0; i < NIMCP_SWARM_CAP_COUNT; i++) {
            profile.proficiency[i] = 0.8f;
        }
        ASSERT_EQ(swarm_scheduler_register_agent(scheduler, &profile), 0);
    }

    // Helper to create a test task
    uint64_t CreateTestTask(const char* desc = "Test task") {
        uint64_t task_id = 0;
        int result = swarm_task_create(manager, desc, SWARM_TASK_TYPE_OBSERVATION,
                                       SWARM_TASK_PRIORITY_NORMAL, &task_id);
        EXPECT_EQ(result, 0);
        EXPECT_NE(task_id, 0u);
        return task_id;
    }
};

//=============================================================================
// Task Manager Tests
//=============================================================================

TEST_F(TaskManagerTest, CreateManager_DefaultConfig) {
    // Manager created in SetUp
    swarm_task_manager_stats_t stats;
    ASSERT_EQ(swarm_task_manager_get_stats(manager, &stats), 0);
    EXPECT_EQ(stats.total_tasks_created, 0u);
}

TEST_F(TaskManagerTest, CreateTask_Success) {
    uint64_t task_id = 0;
    int result = swarm_task_create(manager, "Test task",
                                   SWARM_TASK_TYPE_MOVEMENT,
                                   SWARM_TASK_PRIORITY_NORMAL,
                                   &task_id);
    ASSERT_EQ(result, 0);
    EXPECT_NE(task_id, 0u);

    const swarm_task_t* task = swarm_task_get(manager, task_id);
    ASSERT_NE(task, nullptr);
    EXPECT_STREQ(task->description, "Test task");
    EXPECT_EQ(task->type, SWARM_TASK_TYPE_MOVEMENT);
    EXPECT_EQ(task->priority, SWARM_TASK_PRIORITY_NORMAL);
    EXPECT_EQ(task->status, SWARM_TASK_STATUS_PENDING);
}

TEST_F(TaskManagerTest, CreateMultipleTasks) {
    std::vector<uint64_t> task_ids;

    for (int i = 0; i < 10; i++) {
        uint64_t task_id = 0;
        char desc[64];
        snprintf(desc, sizeof(desc), "Task %d", i);
        ASSERT_EQ(swarm_task_create(manager, desc, SWARM_TASK_TYPE_OBSERVATION,
                                    SWARM_TASK_PRIORITY_NORMAL, &task_id), 0);
        task_ids.push_back(task_id);
    }

    // Verify all tasks created with unique IDs
    EXPECT_EQ(task_ids.size(), 10u);
    for (size_t i = 0; i < task_ids.size(); i++) {
        for (size_t j = i + 1; j < task_ids.size(); j++) {
            EXPECT_NE(task_ids[i], task_ids[j]);
        }
    }

    swarm_task_manager_stats_t stats;
    swarm_task_manager_get_stats(manager, &stats);
    EXPECT_EQ(stats.total_tasks_created, 10u);
}

TEST_F(TaskManagerTest, SetTaskRequirements) {
    uint64_t task_id = 0;
    swarm_task_create(manager, "Task with requirements",
                      SWARM_TASK_TYPE_MANIPULATION,
                      SWARM_TASK_PRIORITY_HIGH, &task_id);

    swarm_task_requirements_t req = {};
    req.required_capabilities = (1 << NIMCP_SWARM_CAP_TRANSPORT);
    req.min_energy = 0.5f;

    ASSERT_EQ(swarm_task_set_requirements(manager, task_id, &req), 0);

    const swarm_task_t* task = swarm_task_get(manager, task_id);
    EXPECT_EQ(task->requirements.required_capabilities, req.required_capabilities);
    EXPECT_FLOAT_EQ(task->requirements.min_energy, 0.5f);
}

TEST_F(TaskManagerTest, AddTaskDependency) {
    uint64_t task1 = 0, task2 = 0;
    swarm_task_create(manager, "Parent task", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task1);
    swarm_task_create(manager, "Child task", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task2);

    // Child depends on parent
    ASSERT_EQ(swarm_task_add_dependency(manager, task2, task1), 0);

    const swarm_task_t* child = swarm_task_get(manager, task2);
    EXPECT_EQ(child->dependency_count, 1u);
    EXPECT_EQ(child->depends_on[0], task1);
}

TEST_F(TaskManagerTest, CircularDependencyRejected) {
    uint64_t task_id = 0;
    swarm_task_create(manager, "Self-dependent task", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);

    // Task cannot depend on itself
    EXPECT_NE(swarm_task_add_dependency(manager, task_id, task_id), 0);
}

TEST_F(TaskManagerTest, SubmitTask_Dependencies) {
    uint64_t task1 = 0, task2 = 0;
    swarm_task_create(manager, "Parent", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task1);
    swarm_task_create(manager, "Child", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task2);
    swarm_task_add_dependency(manager, task2, task1);

    // Submit child - should be blocked
    swarm_task_submit(manager, task2);
    EXPECT_EQ(swarm_task_get_status(manager, task2), SWARM_TASK_STATUS_BLOCKED);

    // Submit and complete parent
    swarm_task_submit(manager, task1);
    EXPECT_EQ(swarm_task_get_status(manager, task1), SWARM_TASK_STATUS_QUEUED);
}

TEST_F(TaskManagerTest, TaskLifecycle_CompleteFlow) {
    uint64_t task_id = 0;
    swarm_task_create(manager, "Lifecycle test", SWARM_TASK_TYPE_MOVEMENT,
                      SWARM_TASK_PRIORITY_HIGH, &task_id);

    // PENDING -> QUEUED
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_PENDING);
    swarm_task_submit(manager, task_id);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_QUEUED);

    // QUEUED -> ASSIGNED
    swarm_task_assign(manager, task_id, 1);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_ASSIGNED);

    // ASSIGNED -> IN_PROGRESS
    swarm_task_start(manager, task_id);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_IN_PROGRESS);

    // Update progress
    swarm_task_update_progress(manager, task_id, 0.5f);
    const swarm_task_t* task = swarm_task_get(manager, task_id);
    EXPECT_FLOAT_EQ(task->progress, 0.5f);

    // IN_PROGRESS -> COMPLETED
    swarm_task_complete(manager, task_id, nullptr);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_COMPLETED);
}

TEST_F(TaskManagerTest, TaskFail_AndRetry) {
    uint64_t task_id = 0;
    swarm_task_create(manager, "Fail test", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);

    swarm_task_submit(manager, task_id);
    swarm_task_assign(manager, task_id, 1);
    swarm_task_start(manager, task_id);

    // Fail the task
    swarm_task_fail(manager, task_id, SWARM_TASK_FAIL_TIMEOUT);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_FAILED);

    // Retry should succeed
    EXPECT_EQ(swarm_task_retry(manager, task_id), 0);
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_QUEUED);

    const swarm_task_t* task = swarm_task_get(manager, task_id);
    EXPECT_EQ(task->retry_count, 1u);
}

TEST_F(TaskManagerTest, TaskCancel) {
    uint64_t task_id = 0;
    swarm_task_create(manager, "Cancel test", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);

    swarm_task_submit(manager, task_id);
    swarm_task_cancel(manager, task_id);

    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_CANCELLED);
}

TEST_F(TaskManagerTest, GetTasksByMission) {
    uint64_t tasks[5];
    for (int i = 0; i < 5; i++) {
        swarm_task_create(manager, "Mission task", SWARM_TASK_TYPE_OBSERVATION,
                          SWARM_TASK_PRIORITY_NORMAL, &tasks[i]);
        swarm_task_set_mission(manager, tasks[i], 100);  // Mission ID 100
    }

    // Create a task with different mission
    uint64_t other_task = 0;
    swarm_task_create(manager, "Other task", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &other_task);
    swarm_task_set_mission(manager, other_task, 200);

    uint64_t found[10];
    uint32_t count = 0;
    swarm_task_get_by_mission(manager, 100, found, 10, &count);

    EXPECT_EQ(count, 5u);
}

TEST_F(TaskManagerTest, TaskCallback) {
    struct CallbackData {
        bool called = false;
        uint64_t task_id = 0;
        swarm_task_status_t status = SWARM_TASK_STATUS_PENDING;
    } data;

    uint64_t task_id = 0;
    swarm_task_create(manager, "Callback test", SWARM_TASK_TYPE_OBSERVATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);

    swarm_task_set_callback(manager, task_id,
        [](uint64_t id, swarm_task_status_t status,
           const swarm_task_result_t* result, void* user_data) {
            auto* data = static_cast<CallbackData*>(user_data);
            data->called = true;
            data->task_id = id;
            data->status = status;
        }, &data);

    swarm_task_submit(manager, task_id);
    swarm_task_assign(manager, task_id, 1);
    swarm_task_start(manager, task_id);
    swarm_task_complete(manager, task_id, nullptr);

    EXPECT_TRUE(data.called);
    EXPECT_EQ(data.task_id, task_id);
    EXPECT_EQ(data.status, SWARM_TASK_STATUS_COMPLETED);
}

//=============================================================================
// Task Queue Tests
//=============================================================================

TEST_F(TaskQueueTest, CreateQueue) {
    // Queue created in SetUp
    EXPECT_EQ(swarm_task_queue_size(queue), 0u);
    EXPECT_TRUE(swarm_task_queue_is_empty(queue));
    EXPECT_FALSE(swarm_task_queue_is_full(queue));
}

TEST_F(TaskQueueTest, EnqueueDequeue) {
    // Enqueue tasks
    ASSERT_EQ(swarm_task_queue_enqueue(queue, 1, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f), 0);
    ASSERT_EQ(swarm_task_queue_enqueue(queue, 2, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f), 0);
    ASSERT_EQ(swarm_task_queue_enqueue(queue, 3, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f), 0);

    EXPECT_EQ(swarm_task_queue_size(queue), 3u);

    // Dequeue all tasks (heap doesn't guarantee FIFO within same priority when
    // enqueue times are identical - which happens in fast succession)
    uint64_t task_id = 0;
    std::set<uint64_t> dequeued;

    ASSERT_EQ(swarm_task_queue_dequeue(queue, &task_id), 0);
    dequeued.insert(task_id);
    EXPECT_TRUE(task_id >= 1 && task_id <= 3);

    ASSERT_EQ(swarm_task_queue_dequeue(queue, &task_id), 0);
    dequeued.insert(task_id);
    EXPECT_TRUE(task_id >= 1 && task_id <= 3);

    EXPECT_EQ(swarm_task_queue_size(queue), 1u);
    EXPECT_EQ(dequeued.size(), 2u);  // Got 2 unique tasks
}

TEST_F(TaskQueueTest, PriorityOrdering) {
    // Enqueue with different priorities (out of order)
    swarm_task_queue_enqueue(queue, 1, SWARM_TASK_PRIORITY_LOW, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 2, SWARM_TASK_PRIORITY_CRITICAL, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 3, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 4, SWARM_TASK_PRIORITY_HIGH, 0, 100.0f);

    // Should dequeue in priority order
    uint64_t task_id = 0;

    swarm_task_queue_dequeue(queue, &task_id);
    EXPECT_EQ(task_id, 2u);  // CRITICAL

    swarm_task_queue_dequeue(queue, &task_id);
    EXPECT_EQ(task_id, 4u);  // HIGH

    swarm_task_queue_dequeue(queue, &task_id);
    EXPECT_EQ(task_id, 3u);  // NORMAL

    swarm_task_queue_dequeue(queue, &task_id);
    EXPECT_EQ(task_id, 1u);  // LOW
}

TEST_F(TaskQueueTest, Peek) {
    swarm_task_queue_enqueue(queue, 42, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);

    uint64_t task_id = 0;
    ASSERT_EQ(swarm_task_queue_peek(queue, &task_id), 0);
    EXPECT_EQ(task_id, 42u);

    // Size shouldn't change after peek
    EXPECT_EQ(swarm_task_queue_size(queue), 1u);
}

TEST_F(TaskQueueTest, Remove) {
    swarm_task_queue_enqueue(queue, 1, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 2, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 3, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);

    // Remove middle task
    ASSERT_EQ(swarm_task_queue_remove(queue, 2), 0);
    EXPECT_EQ(swarm_task_queue_size(queue), 2u);
    EXPECT_FALSE(swarm_task_queue_contains(queue, 2));

    // Remove non-existent task should fail
    EXPECT_NE(swarm_task_queue_remove(queue, 999), 0);
}

TEST_F(TaskQueueTest, Contains) {
    swarm_task_queue_enqueue(queue, 42, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);

    EXPECT_TRUE(swarm_task_queue_contains(queue, 42));
    EXPECT_FALSE(swarm_task_queue_contains(queue, 99));
}

TEST_F(TaskQueueTest, UpdatePriority) {
    swarm_task_queue_enqueue(queue, 1, SWARM_TASK_PRIORITY_LOW, 0, 100.0f);
    swarm_task_queue_enqueue(queue, 2, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);

    // Task 2 should be first (NORMAL > LOW)
    uint64_t task_id = 0;
    swarm_task_queue_peek(queue, &task_id);
    EXPECT_EQ(task_id, 2u);

    // Upgrade task 1 to CRITICAL
    swarm_task_queue_update_priority(queue, 1, SWARM_TASK_PRIORITY_CRITICAL);

    // Now task 1 should be first
    swarm_task_queue_peek(queue, &task_id);
    EXPECT_EQ(task_id, 1u);
}

TEST_F(TaskQueueTest, Clear) {
    for (int i = 0; i < 10; i++) {
        swarm_task_queue_enqueue(queue, i + 1, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);
    }

    EXPECT_EQ(swarm_task_queue_size(queue), 10u);

    uint32_t removed = swarm_task_queue_clear(queue);
    EXPECT_EQ(removed, 10u);
    EXPECT_TRUE(swarm_task_queue_is_empty(queue));
}

TEST_F(TaskQueueTest, LoadFactor) {
    EXPECT_FLOAT_EQ(swarm_task_queue_get_load(queue), 0.0f);

    // Fill half the queue (default capacity is 32)
    for (int i = 0; i < 16; i++) {
        swarm_task_queue_enqueue(queue, i + 1, SWARM_TASK_PRIORITY_NORMAL, 0, 100.0f);
    }

    EXPECT_FLOAT_EQ(swarm_task_queue_get_load(queue), 0.5f);
}

TEST_F(TaskQueueTest, ActiveTask) {
    EXPECT_EQ(swarm_task_queue_get_active(queue), SWARM_TASK_ID_NONE);

    swarm_task_queue_set_active(queue, 42);
    EXPECT_EQ(swarm_task_queue_get_active(queue), 42u);

    swarm_task_queue_set_active(queue, SWARM_TASK_ID_NONE);
    EXPECT_EQ(swarm_task_queue_get_active(queue), SWARM_TASK_ID_NONE);
}

//=============================================================================
// Task Scheduler Tests
//=============================================================================

TEST_F(TaskSchedulerTest, RegisterAgent) {
    RegisterTestAgent(1);
    EXPECT_EQ(swarm_scheduler_agent_count(scheduler), 1u);
    EXPECT_EQ(swarm_scheduler_available_agent_count(scheduler), 1u);
}

TEST_F(TaskSchedulerTest, RegisterMultipleAgents) {
    for (uint32_t i = 0; i < 5; i++) {
        RegisterTestAgent(i + 1);
    }

    EXPECT_EQ(swarm_scheduler_agent_count(scheduler), 5u);
    EXPECT_EQ(swarm_scheduler_available_agent_count(scheduler), 5u);
}

TEST_F(TaskSchedulerTest, UnregisterAgent) {
    RegisterTestAgent(1);
    RegisterTestAgent(2);
    EXPECT_EQ(swarm_scheduler_agent_count(scheduler), 2u);

    ASSERT_EQ(swarm_scheduler_unregister_agent(scheduler, 1), 0);
    EXPECT_EQ(swarm_scheduler_agent_count(scheduler), 1u);
}

TEST_F(TaskSchedulerTest, SetAgentAvailability) {
    RegisterTestAgent(1);
    EXPECT_EQ(swarm_scheduler_available_agent_count(scheduler), 1u);

    swarm_scheduler_set_agent_available(scheduler, 1, false);
    EXPECT_EQ(swarm_scheduler_available_agent_count(scheduler), 0u);

    swarm_scheduler_set_agent_available(scheduler, 1, true);
    EXPECT_EQ(swarm_scheduler_available_agent_count(scheduler), 1u);
}

TEST_F(TaskSchedulerTest, ScheduleTask_Success) {
    RegisterTestAgent(1);

    uint64_t task_id = CreateTestTask("Test scheduling");
    swarm_task_submit(manager, task_id);

    uint32_t assigned_agent = 0;
    swarm_schedule_result_t result =
        swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);

    EXPECT_EQ(result, SWARM_SCHEDULE_SUCCESS);
    EXPECT_EQ(assigned_agent, 1u);

    // Task should now be assigned
    EXPECT_EQ(swarm_task_get_status(manager, task_id), SWARM_TASK_STATUS_ASSIGNED);
}

TEST_F(TaskSchedulerTest, ScheduleTask_NoAgents) {
    uint64_t task_id = CreateTestTask("No agents");
    swarm_task_submit(manager, task_id);

    uint32_t assigned_agent = 0;
    swarm_schedule_result_t result =
        swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);

    EXPECT_EQ(result, SWARM_SCHEDULE_NO_CAPABLE_AGENT);
}

TEST_F(TaskSchedulerTest, ScheduleTask_CapabilityMismatch) {
    // Register agent with limited capabilities
    swarm_agent_profile_t profile = {};
    profile.agent_id = 1;
    profile.capabilities_mask = (1 << NIMCP_SWARM_CAP_SURVEILLANCE);  // Only surveillance
    profile.is_available = true;
    profile.energy_level = 1.0f;
    swarm_scheduler_register_agent(scheduler, &profile);

    // Create task requiring transport capability
    uint64_t task_id = 0;
    swarm_task_create(manager, "Transport task", SWARM_TASK_TYPE_MANIPULATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);

    swarm_task_requirements_t req = {};
    req.required_capabilities = (1 << NIMCP_SWARM_CAP_TRANSPORT);
    swarm_task_set_requirements(manager, task_id, &req);
    swarm_task_submit(manager, task_id);

    uint32_t assigned_agent = 0;
    swarm_schedule_result_t result =
        swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);

    EXPECT_EQ(result, SWARM_SCHEDULE_NO_CAPABLE_AGENT);
}

TEST_F(TaskSchedulerTest, ScheduleAll) {
    RegisterTestAgent(1);
    RegisterTestAgent(2);

    // Create multiple tasks
    for (int i = 0; i < 5; i++) {
        uint64_t task_id = CreateTestTask("Batch task");
        swarm_task_submit(manager, task_id);
    }

    uint32_t scheduled_count = 0;
    swarm_scheduler_schedule_all(scheduler, &scheduled_count);

    EXPECT_EQ(scheduled_count, 5u);
}

TEST_F(TaskSchedulerTest, RoundRobinScheduling) {
    swarm_scheduler_set_algorithm(scheduler, SWARM_SCHEDULER_ROUND_ROBIN);

    RegisterTestAgent(1);
    RegisterTestAgent(2);
    RegisterTestAgent(3);

    std::vector<uint32_t> assignments;

    // Create and schedule multiple tasks
    for (int i = 0; i < 6; i++) {
        uint64_t task_id = CreateTestTask("RR task");
        swarm_task_submit(manager, task_id);

        uint32_t assigned_agent = 0;
        swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);
        assignments.push_back(assigned_agent);
    }

    // Should distribute evenly: 1, 2, 3, 1, 2, 3 or similar pattern
    // Check each agent got at least one task
    int agent1_count = 0, agent2_count = 0, agent3_count = 0;
    for (uint32_t agent : assignments) {
        if (agent == 1) agent1_count++;
        else if (agent == 2) agent2_count++;
        else if (agent == 3) agent3_count++;
    }

    EXPECT_EQ(agent1_count, 2);
    EXPECT_EQ(agent2_count, 2);
    EXPECT_EQ(agent3_count, 2);
}

TEST_F(TaskSchedulerTest, LoadBalanceScheduling) {
    swarm_scheduler_set_algorithm(scheduler, SWARM_SCHEDULER_LOAD_BALANCE);

    RegisterTestAgent(1);
    RegisterTestAgent(2);

    // Schedule tasks - should balance load
    for (int i = 0; i < 4; i++) {
        uint64_t task_id = CreateTestTask("LB task");
        swarm_task_submit(manager, task_id);
        swarm_scheduler_schedule_task(scheduler, task_id, nullptr);
    }

    // Check queue sizes are balanced
    swarm_task_queue_t* q1 = swarm_scheduler_get_agent_queue(scheduler, 1);
    swarm_task_queue_t* q2 = swarm_scheduler_get_agent_queue(scheduler, 2);

    uint32_t size1 = swarm_task_queue_size(q1);
    uint32_t size2 = swarm_task_queue_size(q2);

    // Load should be balanced (2 tasks each)
    EXPECT_EQ(size1, 2u);
    EXPECT_EQ(size2, 2u);
}

TEST_F(TaskSchedulerTest, EnergyAwareScheduling) {
    swarm_scheduler_set_algorithm(scheduler, SWARM_SCHEDULER_ENERGY_AWARE);

    // Agent 1: low energy
    swarm_agent_profile_t low_energy = {};
    low_energy.agent_id = 1;
    low_energy.capabilities_mask = 0xFFFF;
    low_energy.is_available = true;
    low_energy.energy_level = 0.2f;
    swarm_scheduler_register_agent(scheduler, &low_energy);

    // Agent 2: high energy
    swarm_agent_profile_t high_energy = {};
    high_energy.agent_id = 2;
    high_energy.capabilities_mask = 0xFFFF;
    high_energy.is_available = true;
    high_energy.energy_level = 0.9f;
    swarm_scheduler_register_agent(scheduler, &high_energy);

    uint64_t task_id = CreateTestTask("Energy task");
    swarm_task_submit(manager, task_id);

    uint32_t assigned_agent = 0;
    swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);

    // Should prefer high-energy agent
    EXPECT_EQ(assigned_agent, 2u);
}

TEST_F(TaskSchedulerTest, ScoreAgents) {
    RegisterTestAgent(1);
    RegisterTestAgent(2);

    uint64_t task_id = CreateTestTask("Score task");
    swarm_task_submit(manager, task_id);

    swarm_agent_score_t scores[SWARM_SCHEDULER_MAX_AGENTS];
    uint32_t count = 0;

    ASSERT_EQ(swarm_scheduler_score_agents(scheduler, task_id, scores, &count), 0);
    EXPECT_EQ(count, 2u);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_TRUE(scores[i].is_capable);
        EXPECT_GE(scores[i].total_score, 0.0f);
        EXPECT_LE(scores[i].total_score, 5.0f);  // Sum of 5 factors
    }
}

TEST_F(TaskSchedulerTest, GetCapableAgents) {
    // Agent 1: has transport
    swarm_agent_profile_t transport = {};
    transport.agent_id = 1;
    transport.capabilities_mask = (1 << NIMCP_SWARM_CAP_TRANSPORT);
    transport.is_available = true;
    transport.energy_level = 1.0f;
    swarm_scheduler_register_agent(scheduler, &transport);

    // Agent 2: has surveillance
    swarm_agent_profile_t surveillance = {};
    surveillance.agent_id = 2;
    surveillance.capabilities_mask = (1 << NIMCP_SWARM_CAP_SURVEILLANCE);
    surveillance.is_available = true;
    surveillance.energy_level = 1.0f;
    swarm_scheduler_register_agent(scheduler, &surveillance);

    // Create task requiring transport
    uint64_t task_id = 0;
    swarm_task_create(manager, "Transport task", SWARM_TASK_TYPE_MANIPULATION,
                      SWARM_TASK_PRIORITY_NORMAL, &task_id);
    swarm_task_requirements_t req = {};
    req.required_capabilities = (1 << NIMCP_SWARM_CAP_TRANSPORT);
    swarm_task_set_requirements(manager, task_id, &req);
    swarm_task_submit(manager, task_id);

    uint32_t capable_agents[10];
    uint32_t count = 0;
    swarm_scheduler_get_capable_agents(scheduler, task_id, capable_agents, 10, &count);

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(capable_agents[0], 1u);
}

TEST_F(TaskSchedulerTest, SchedulerStats) {
    RegisterTestAgent(1);

    for (int i = 0; i < 5; i++) {
        uint64_t task_id = CreateTestTask("Stats task");
        swarm_task_submit(manager, task_id);
        swarm_scheduler_schedule_task(scheduler, task_id, nullptr);
    }

    swarm_scheduler_stats_t stats;
    swarm_scheduler_get_stats(scheduler, &stats);

    EXPECT_EQ(stats.total_scheduled, 5u);
    EXPECT_EQ(stats.total_failed, 0u);
}

TEST_F(TaskSchedulerTest, HybridScheduling) {
    swarm_scheduler_set_algorithm(scheduler, SWARM_SCHEDULER_HYBRID);

    // Set custom weights
    swarm_scheduler_weights_t weights;
    weights.capability_weight = 0.5f;
    weights.load_weight = 0.2f;
    weights.energy_weight = 0.2f;
    weights.locality_weight = 0.05f;
    weights.deadline_weight = 0.05f;
    swarm_scheduler_set_weights(scheduler, &weights);

    RegisterTestAgent(1);
    RegisterTestAgent(2);

    uint64_t task_id = CreateTestTask("Hybrid task");
    swarm_task_submit(manager, task_id);

    uint32_t assigned_agent = 0;
    swarm_schedule_result_t result =
        swarm_scheduler_schedule_task(scheduler, task_id, &assigned_agent);

    EXPECT_EQ(result, SWARM_SCHEDULE_SUCCESS);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST(TaskUtilityTest, StatusNames) {
    EXPECT_STREQ(swarm_task_status_name(SWARM_TASK_STATUS_PENDING), "PENDING");
    EXPECT_STREQ(swarm_task_status_name(SWARM_TASK_STATUS_QUEUED), "QUEUED");
    EXPECT_STREQ(swarm_task_status_name(SWARM_TASK_STATUS_COMPLETED), "COMPLETED");
    EXPECT_STREQ(swarm_task_status_name(SWARM_TASK_STATUS_FAILED), "FAILED");
}

TEST(TaskUtilityTest, PriorityNames) {
    EXPECT_STREQ(swarm_task_priority_name(SWARM_TASK_PRIORITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(swarm_task_priority_name(SWARM_TASK_PRIORITY_HIGH), "HIGH");
    EXPECT_STREQ(swarm_task_priority_name(SWARM_TASK_PRIORITY_NORMAL), "NORMAL");
    EXPECT_STREQ(swarm_task_priority_name(SWARM_TASK_PRIORITY_LOW), "LOW");
}

TEST(TaskUtilityTest, TypeNames) {
    EXPECT_STREQ(swarm_task_type_name(SWARM_TASK_TYPE_MOVEMENT), "MOVEMENT");
    EXPECT_STREQ(swarm_task_type_name(SWARM_TASK_TYPE_OBSERVATION), "OBSERVATION");
    EXPECT_STREQ(swarm_task_type_name(SWARM_TASK_TYPE_COMMUNICATION), "COMMUNICATION");
}

TEST(TaskUtilityTest, FailureNames) {
    EXPECT_STREQ(swarm_task_failure_name(SWARM_TASK_FAIL_NONE), "NONE");
    EXPECT_STREQ(swarm_task_failure_name(SWARM_TASK_FAIL_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(swarm_task_failure_name(SWARM_TASK_FAIL_AGENT_LOST), "AGENT_LOST");
}

TEST(SchedulerUtilityTest, AlgorithmNames) {
    EXPECT_STREQ(swarm_scheduler_algorithm_name(SWARM_SCHEDULER_ROUND_ROBIN), "ROUND_ROBIN");
    EXPECT_STREQ(swarm_scheduler_algorithm_name(SWARM_SCHEDULER_CAPABILITY_MATCH), "CAPABILITY_MATCH");
    EXPECT_STREQ(swarm_scheduler_algorithm_name(SWARM_SCHEDULER_HYBRID), "HYBRID");
}

TEST(SchedulerUtilityTest, ResultNames) {
    EXPECT_STREQ(swarm_schedule_result_name(SWARM_SCHEDULE_SUCCESS), "SUCCESS");
    EXPECT_STREQ(swarm_schedule_result_name(SWARM_SCHEDULE_NO_CAPABLE_AGENT), "NO_CAPABLE_AGENT");
    EXPECT_STREQ(swarm_schedule_result_name(SWARM_SCHEDULE_ALL_AGENTS_BUSY), "ALL_AGENTS_BUSY");
}

TEST(SchedulerUtilityTest, AgentMeetsRequirements) {
    swarm_agent_profile_t profile = {};
    profile.agent_id = 1;
    profile.capabilities_mask = (1 << NIMCP_SWARM_CAP_TRANSPORT) |
                                 (1 << NIMCP_SWARM_CAP_SURVEILLANCE);
    profile.is_available = true;
    profile.energy_level = 0.8f;

    swarm_task_requirements_t req = {};
    req.required_capabilities = (1 << NIMCP_SWARM_CAP_TRANSPORT);
    req.min_energy = 0.5f;

    EXPECT_TRUE(swarm_scheduler_agent_meets_requirements(&profile, &req));

    // Test with higher energy requirement
    req.min_energy = 0.9f;
    EXPECT_FALSE(swarm_scheduler_agent_meets_requirements(&profile, &req));

    // Test with missing capability
    req.min_energy = 0.5f;
    req.required_capabilities = (1 << NIMCP_SWARM_CAP_COMBAT);
    EXPECT_FALSE(swarm_scheduler_agent_meets_requirements(&profile, &req));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
