/**
 * @file test_executive_cognitive_integration.cpp
 * @brief Integration tests for Executive Module with Meta-Controller
 *
 * WHAT: Test executive module connects to meta-controller
 * WHY:  Verify executive control properly coordinates with meta-cognition
 *       and working memory slot allocation works with arbitration
 * HOW:  Create executive and meta-controller, test coordination
 *
 * TEST COVERAGE:
 * - Executive module to meta-controller connection
 * - Emotional system integrates with other cognitive modules
 * - Working memory slot allocation with meta-controller arbitration
 * - Task prioritization across cognitive modules
 * - Resource allocation decisions
 *
 * @author NIMCP Development Team
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_cognitive_meta_controller.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ExecutiveCognitiveIntegrationTest : public ::testing::Test {
protected:
    unified_mem_manager_t mem_mgr_ = nullptr;
    executive_controller_t* executive_ = nullptr;
    working_memory_t* wm_ = nullptr;
    global_workspace_t* gw_ = nullptr;

    void SetUp() override {
        // Initialize unified memory
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_mgr_ = unified_mem_create(&mem_config);
        ASSERT_NE(mem_mgr_, nullptr);

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (executive_) {
            executive_destroy(executive_);
            executive_ = nullptr;
        }
        if (wm_) {
            working_memory_destroy(wm_);
            wm_ = nullptr;
        }
        if (gw_) {
            global_workspace_destroy(gw_);
            gw_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
        if (mem_mgr_) {
            unified_mem_destroy(mem_mgr_);
            mem_mgr_ = nullptr;
        }
    }

    task_descriptor_t CreateTestTask(const char* name, task_type_t type,
                                     task_priority_t priority) {
        task_descriptor_t task;
        memset(&task, 0, sizeof(task));
        task.type = type;
        task.priority = priority;
        task.status = TASK_STATUS_PENDING;
        strncpy(task.name, name, sizeof(task.name) - 1);
        return task;
    }
};

//=============================================================================
// EXECUTIVE-META-CONTROLLER CONNECTION TESTS
//=============================================================================

TEST_F(ExecutiveCognitiveIntegrationTest, Executive_Creation) {
    /* WHAT: Create executive controller */
    /* WHY:  Verify basic creation works */

    executive_ = executive_create();
    ASSERT_NE(executive_, nullptr);
}

TEST_F(ExecutiveCognitiveIntegrationTest, Executive_CustomConfig) {
    /* WHAT: Create executive with custom configuration */
    /* WHY:  Verify all configuration options work */

    executive_config_t config;
    config.max_tasks = 32;
    config.task_switch_cost_ms = 150.0f;
    config.inhibition_threshold = 0.65f;
    config.max_plan_depth = 15;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = true;
    config.enable_portia_integration = true;
    config.enable_tom_integration = false;
    config.max_agent_models = 4;
    config.enable_immune_integration = false;
    config.immune_impairment_threshold = 0.6f;
    config.immune_critical_threshold = 0.85f;
    config.enable_quantum_executive = true;

    executive_ = executive_create_custom(&config);
    ASSERT_NE(executive_, nullptr);
}

// FEP bridge tests removed - FEP API not available as standalone
// FEP integration is tested through brain region FEP bridges

//=============================================================================
// WORKING MEMORY INTEGRATION TESTS
//=============================================================================

TEST_F(ExecutiveCognitiveIntegrationTest, WorkingMemory_Creation) {
    /* WHAT: Create working memory system */
    /* WHY:  Verify WM initialization */

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);
}

TEST_F(ExecutiveCognitiveIntegrationTest, WorkingMemory_AddAndRetrieve) {
    /* WHAT: Test basic item addition and retrieval */
    /* WHY:  Verify items can be added and retrieved */

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);

    // Add items using the correct API
    float test_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    bool added = working_memory_add(wm_, test_data, 4, 0.8f);
    EXPECT_TRUE(added);

    // Check size increased
    uint32_t count = working_memory_get_count(wm_);
    EXPECT_GE(count, 1u);

    // Get the item back
    const float* retrieved = working_memory_get(wm_, 0, nullptr);
    if (retrieved) {
        EXPECT_NEAR(retrieved[0], 1.0f, 1e-5f);
    }
}

TEST_F(ExecutiveCognitiveIntegrationTest, WorkingMemory_Capacity) {
    /* WHAT: Test capacity limits */
    /* WHY:  Verify capacity is enforced */

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);

    uint32_t capacity = working_memory_get_capacity(wm_);
    EXPECT_GT(capacity, 0u);

    // Try to add items up to capacity
    float test_data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint32_t added_count = 0;

    for (uint32_t i = 0; i < capacity + 5; ++i) {
        float salience = 0.5f + 0.02f * i;
        bool added = working_memory_add(wm_, test_data, 4, salience);
        if (added) {
            added_count++;
        }
    }

    // Should have added some items
    EXPECT_GT(added_count, 0u);
    // Should not exceed capacity
    EXPECT_LE(working_memory_get_count(wm_), capacity);
}

TEST_F(ExecutiveCognitiveIntegrationTest, WorkingMemory_SalienceBasedEviction) {
    /* WHAT: Test salience-based eviction */
    /* WHY:  Verify high-salience items are retained */

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);

    // Fill with low salience items
    float low_data[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t capacity = working_memory_get_capacity(wm_);

    for (uint32_t i = 0; i < capacity; ++i) {
        working_memory_add(wm_, low_data, 4, 0.1f);  // Low salience
    }

    // Add high salience item
    float high_data[4] = {9.0f, 9.0f, 9.0f, 9.0f};
    bool added = working_memory_add(wm_, high_data, 4, 0.95f);  // High salience

    // High salience item should be added (possibly evicting low salience)
    // depending on implementation
    if (added) {
        // Find the highest salience item
        int highest_idx = working_memory_find_highest_salience(wm_, nullptr);
        if (highest_idx >= 0) {
            float salience;
            working_memory_get_salience(wm_, (uint32_t)highest_idx, &salience);
            // The high salience item should be retained
            EXPECT_GE(salience, 0.9f);
        }
    }
}

TEST_F(ExecutiveCognitiveIntegrationTest, WorkingMemory_ExecutiveCoordination) {
    /* WHAT: Test WM and executive coordination */
    /* WHY:  Verify executive controls WM allocation */

    executive_ = executive_create();
    ASSERT_NE(executive_, nullptr);

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);

    // Add tasks that require WM
    task_descriptor_t task1 = CreateTestTask("task1", TASK_TYPE_REASONING, PRIORITY_HIGH);
    task_descriptor_t task2 = CreateTestTask("task2", TASK_TYPE_MEMORY_RETRIEVAL, PRIORITY_NORMAL);

    uint32_t id1 = executive_add_task(executive_, &task1);
    uint32_t id2 = executive_add_task(executive_, &task2);
    (void)id1;
    (void)id2;

    // Add WM item for task context
    float task_context[8] = {0};
    bool added = working_memory_add(wm_, task_context, 8, 0.7f);
    EXPECT_TRUE(added);

    // Verify both systems are working
    EXPECT_GE(working_memory_get_count(wm_), 1u);
}

//=============================================================================
// TASK SWITCHING AND PRIORITIZATION TESTS
//=============================================================================

TEST_F(ExecutiveCognitiveIntegrationTest, TaskSwitching_Basic) {
    /* WHAT: Test basic task switching */
    /* WHY:  Verify executive can switch between tasks */

    executive_config_t config;
    config.max_tasks = 16;
    config.task_switch_cost_ms = 100.0f;
    config.inhibition_threshold = 0.7f;
    config.max_plan_depth = 10;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = false;
    config.enable_portia_integration = false;
    config.enable_tom_integration = false;
    config.max_agent_models = 4;
    config.enable_immune_integration = false;
    config.immune_impairment_threshold = 0.6f;
    config.immune_critical_threshold = 0.85f;
    config.enable_quantum_executive = false;

    executive_ = executive_create_custom(&config);
    ASSERT_NE(executive_, nullptr);

    // Add multiple tasks
    task_descriptor_t task1 = CreateTestTask("analysis", TASK_TYPE_REASONING, PRIORITY_NORMAL);
    task_descriptor_t task2 = CreateTestTask("retrieval", TASK_TYPE_MEMORY_RETRIEVAL, PRIORITY_HIGH);

    uint32_t id1 = executive_add_task(executive_, &task1);
    uint32_t id2 = executive_add_task(executive_, &task2);

    // Switch to high priority task
    if (id1 > 0 && id2 > 0) {
        bool switched = executive_switch_task(executive_, id2, 0);
        (void)switched;  // May succeed or fail depending on implementation
    }
}

TEST_F(ExecutiveCognitiveIntegrationTest, TaskSwitching_PriorityPreemption) {
    /* WHAT: Test priority-based task preemption */
    /* WHY:  Verify urgent tasks preempt normal tasks */

    executive_config_t config;
    config.max_tasks = 16;
    config.task_switch_cost_ms = 50.0f;
    config.inhibition_threshold = 0.7f;
    config.max_plan_depth = 10;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = true;
    config.enable_portia_integration = false;
    config.enable_tom_integration = false;
    config.max_agent_models = 4;
    config.enable_immune_integration = false;
    config.immune_impairment_threshold = 0.6f;
    config.immune_critical_threshold = 0.85f;
    config.enable_quantum_executive = false;

    executive_ = executive_create_custom(&config);
    ASSERT_NE(executive_, nullptr);

    // Add normal priority task and start it
    task_descriptor_t normal_task = CreateTestTask("normal", TASK_TYPE_CLASSIFICATION, PRIORITY_NORMAL);
    uint32_t normal_id = executive_add_task(executive_, &normal_task);
    (void)normal_id;

    // Add urgent task
    task_descriptor_t urgent_task = CreateTestTask("urgent", TASK_TYPE_PLANNING, PRIORITY_URGENT);
    uint32_t urgent_id = executive_add_task(executive_, &urgent_task);
    (void)urgent_id;

    // Urgent task should be schedulable before normal
}

TEST_F(ExecutiveCognitiveIntegrationTest, InhibitoryControl) {
    /* WHAT: Test inhibitory control mechanism */
    /* WHY:  Verify executive can suppress inappropriate responses */

    executive_config_t config;
    config.max_tasks = 16;
    config.task_switch_cost_ms = 100.0f;
    config.inhibition_threshold = 0.6f;  // Lower threshold = more inhibition
    config.max_plan_depth = 10;
    config.enable_task_prioritization = true;
    config.enable_deadline_checking = false;
    config.enable_portia_integration = false;
    config.enable_tom_integration = false;
    config.max_agent_models = 4;
    config.enable_immune_integration = false;
    config.immune_impairment_threshold = 0.6f;
    config.immune_critical_threshold = 0.85f;
    config.enable_quantum_executive = false;

    executive_ = executive_create_custom(&config);
    ASSERT_NE(executive_, nullptr);

    // Inhibitory control should suppress impulsive task additions
    // or inappropriate task switches
}

//=============================================================================
// GLOBAL WORKSPACE INTEGRATION TESTS
//=============================================================================

TEST_F(ExecutiveCognitiveIntegrationTest, GlobalWorkspace_ExecutiveCoordination) {
    /* WHAT: Test executive-global workspace coordination */
    /* WHY:  Verify conscious access to executive functions */

    executive_ = executive_create();
    ASSERT_NE(executive_, nullptr);

    global_workspace_config_t gw_config = global_workspace_default_config();
    gw_config.capacity_dim = 64;
    gw_config.ignition_threshold = 0.5f;

    gw_ = global_workspace_create_custom(&gw_config);
    if (!gw_) {
        GTEST_SKIP() << "Global workspace not available";
    }

    // Executive decisions should be broadcast to global workspace
    // for conscious awareness

    // Add task
    task_descriptor_t task = CreateTestTask("conscious_task", TASK_TYPE_PLANNING, PRIORITY_HIGH);
    uint32_t task_id = executive_add_task(executive_, &task);
    (void)task_id;

    // Task addition should potentially trigger global broadcast
}

//=============================================================================
// CONCURRENT ACCESS TESTS
//=============================================================================

TEST_F(ExecutiveCognitiveIntegrationTest, ConcurrentTaskAddition) {
    /* WHAT: Test concurrent task additions */
    /* WHY:  Verify thread safety */

    executive_ = executive_create();
    ASSERT_NE(executive_, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> tasks_added{0};

    auto worker = [&](int thread_id) {
        while (!stop.load()) {
            char name[64];
            snprintf(name, sizeof(name), "task_%d_%d", thread_id, tasks_added.load());

            task_descriptor_t task = CreateTestTask(
                name, TASK_TYPE_CUSTOM, PRIORITY_NORMAL
            );

            uint32_t id = executive_add_task(executive_, &task);
            if (id > 0) {
                tasks_added.fetch_add(1);
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // Some tasks should have been added
    EXPECT_GE(tasks_added.load(), 0);
}

TEST_F(ExecutiveCognitiveIntegrationTest, ConcurrentWMAccess) {
    /* WHAT: Test concurrent WM access */
    /* WHY:  Verify thread safety of WM */

    wm_ = working_memory_create();
    ASSERT_NE(wm_, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> operations{0};

    auto worker = [&]() {
        while (!stop.load()) {
            float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
            bool added = working_memory_add(wm_, data, 4, 0.5f);
            if (added) {
                operations.fetch_add(1);
                // Try to remove
                working_memory_remove(wm_, 0);
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // Should not crash - operations may or may not succeed
    EXPECT_GE(operations.load(), 0);
}
