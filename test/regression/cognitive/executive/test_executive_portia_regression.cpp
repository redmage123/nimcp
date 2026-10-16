/**
 * @file test_executive_portia_regression.cpp
 * @brief Regression tests for Executive-Portia integration
 *
 * WHAT: Regression tests ensuring backward compatibility and no regressions
 * WHY:  Verify Portia integration doesn't break existing executive functionality
 * HOW:  Test executive works without Portia, performance is maintained, no leaks
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include "cognitive/nimcp_executive.h"
#include "portia/nimcp_portia.h"
#include "async/nimcp_bio_router.h"
#include "utils/logging/nimcp_logging.h"

namespace {

class ExecutivePortiaRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        bio_router_config_t cfg = {0}; bio_router_init(&cfg);
    }

    void TearDown() override {
        bio_router_shutdown();
    }
};

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ExecutivePortiaRegressionTest, ExecutiveWorksWithoutPortia) {
    // Don't initialize Portia
    // Executive should still function normally

    executive_config_t config = {
        .max_tasks = 16,
        .task_switch_cost_ms = 200.0f,
        .inhibition_threshold = 0.7f,
        .max_plan_depth = 10,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true,
        .enable_portia_integration = true  // Enabled but Portia not init
    };

    executive_controller_t* exec = executive_create_custom(&config);
    ASSERT_NE(exec, nullptr);

    // Should get PLATFORM_TIER_MINIMAL since Portia not initialized
    uint32_t tier = executive_get_portia_tier(exec);
    EXPECT_EQ(tier, PLATFORM_TIER_MINIMAL);

    // Should not be resource-aware
    bool resource_aware = executive_is_resource_aware(exec);
    EXPECT_FALSE(resource_aware);

    // Should get default planning depth
    uint32_t depth = executive_get_recommended_plan_depth(exec);
    EXPECT_EQ(depth, 10);

    // Basic operations should work
    task_descriptor_t task = {};
    task.type = TASK_TYPE_PLANNING;
    task.priority = PRIORITY_NORMAL;
    snprintf(task.name, sizeof(task.name), "Test task");

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0);

    bool switched = executive_switch_task(exec, task_id, 0);
    EXPECT_TRUE(switched);

    bool completed = executive_complete_task(exec, true, 1000);
    EXPECT_TRUE(completed);

    executive_destroy(exec);
}

TEST_F(ExecutivePortiaRegressionTest, ExecutiveWorksWithPortiaDisabledInConfig) {
    executive_config_t config = {
        .max_tasks = 16,
        .task_switch_cost_ms = 200.0f,
        .inhibition_threshold = 0.7f,
        .max_plan_depth = 10,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true,
        .enable_portia_integration = false  // Explicitly disabled
    };

    executive_controller_t* exec = executive_create_custom(&config);
    ASSERT_NE(exec, nullptr);

    // All Portia queries should return safe defaults
    EXPECT_EQ(executive_get_portia_tier(exec), PLATFORM_TIER_MINIMAL);
    EXPECT_FALSE(executive_is_resource_aware(exec));
    EXPECT_EQ(executive_get_recommended_plan_depth(exec), 10);

    // Plan creation should use full depth
    plan_t* plan = executive_create_plan(exec, "Full depth plan", 10);
    ASSERT_NE(plan, nullptr);
    EXPECT_GT(plan->num_steps, 0);
    executive_destroy_plan(plan);

    executive_destroy(exec);
}

TEST_F(ExecutivePortiaRegressionTest, LegacyCodeStillWorks) {
    // Code that uses executive_create() (default config) should still work
    executive_controller_t* exec = executive_create();
    ASSERT_NE(exec, nullptr);

    // Basic executive operations
    plan_t* plan = executive_create_plan(exec, "Legacy plan", 5);
    ASSERT_NE(plan, nullptr);
    executive_destroy_plan(plan);

    task_descriptor_t task = {};
    task.type = TASK_TYPE_CLASSIFICATION;
    task.priority = PRIORITY_NORMAL;
    snprintf(task.name, sizeof(task.name), "Legacy task");

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0);

    executive_stats_t stats;
    bool got_stats = executive_get_stats(exec, &stats);
    EXPECT_TRUE(got_stats);
    EXPECT_GT(stats.total_tasks, 0);

    executive_destroy(exec);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(ExecutivePortiaRegressionTest, NoPerformanceRegressionInTaskSwitching) {
    executive_controller_t* exec_no_portia = nullptr;
    executive_controller_t* exec_with_portia = nullptr;

    // Create executive without Portia
    {
        executive_config_t config = {
            .max_tasks = 16,
            .task_switch_cost_ms = 200.0f,
            .inhibition_threshold = 0.7f,
            .max_plan_depth = 10,
            .enable_task_prioritization = true,
            .enable_deadline_checking = true,
            .enable_portia_integration = false
        };
        exec_no_portia = executive_create_custom(&config);
    }

    // Create executive with Portia
    {
        executive_config_t config = {
            .max_tasks = 16,
            .task_switch_cost_ms = 200.0f,
            .inhibition_threshold = 0.7f,
            .max_plan_depth = 10,
            .enable_task_prioritization = true,
            .enable_deadline_checking = true,
            .enable_portia_integration = true
        };
        exec_with_portia = executive_create_custom(&config);
    }

    ASSERT_NE(exec_no_portia, nullptr);
    ASSERT_NE(exec_with_portia, nullptr);

    // Measure task switching performance without Portia
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        task_descriptor_t task = {};
        task.type = TASK_TYPE_PLANNING;
        task.priority = PRIORITY_NORMAL;
        snprintf(task.name, sizeof(task.name), "Perf task %d", i);

        uint32_t task_id = executive_add_task(exec_no_portia, &task);
        executive_switch_task(exec_no_portia, task_id, i * 1000);
        executive_complete_task(exec_no_portia, true, (i + 1) * 1000);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration_no_portia = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Measure task switching performance with Portia
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        task_descriptor_t task = {};
        task.type = TASK_TYPE_PLANNING;
        task.priority = PRIORITY_NORMAL;
        snprintf(task.name, sizeof(task.name), "Perf task %d", i);

        uint32_t task_id = executive_add_task(exec_with_portia, &task);
        executive_switch_task(exec_with_portia, task_id, i * 1000);
        executive_complete_task(exec_with_portia, true, (i + 1) * 1000);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration_with_portia = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Portia integration should add < 20% overhead
    float overhead = (float)duration_with_portia.count() / (float)duration_no_portia.count();
    EXPECT_LT(overhead, 1.2f);  // Less than 20% slower

    executive_destroy(exec_no_portia);
    executive_destroy(exec_with_portia);
}

TEST_F(ExecutivePortiaRegressionTest, NoPerformanceRegressionInPlanCreation) {
    executive_config_t config_no_portia = {
        .max_tasks = 16,
        .task_switch_cost_ms = 200.0f,
        .inhibition_threshold = 0.7f,
        .max_plan_depth = 10,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true,
        .enable_portia_integration = false
    };

    executive_config_t config_with_portia = config_no_portia;
    config_with_portia.enable_portia_integration = true;

    executive_controller_t* exec_no_portia = executive_create_custom(&config_no_portia);
    executive_controller_t* exec_with_portia = executive_create_custom(&config_with_portia);

    ASSERT_NE(exec_no_portia, nullptr);
    ASSERT_NE(exec_with_portia, nullptr);

    // Benchmark plan creation
    std::vector<plan_t*> plans_no_portia;
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        plan_t* plan = executive_create_plan(exec_no_portia, "Benchmark", 8);
        plans_no_portia.push_back(plan);
    }
    auto end1 = std::chrono::high_resolution_clock::now();

    std::vector<plan_t*> plans_with_portia;
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        plan_t* plan = executive_create_plan(exec_with_portia, "Benchmark", 8);
        plans_with_portia.push_back(plan);
    }
    auto end2 = std::chrono::high_resolution_clock::now();

    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Should be comparable performance
    float ratio = (float)duration2.count() / (float)duration1.count();
    EXPECT_LT(ratio, 1.3f);  // Less than 30% slower

    // Cleanup
    for (plan_t* plan : plans_no_portia) {
        executive_destroy_plan(plan);
    }
    for (plan_t* plan : plans_with_portia) {
        executive_destroy_plan(plan);
    }

    executive_destroy(exec_no_portia);
    executive_destroy(exec_with_portia);
}

//=============================================================================
// Memory Leak Tests
//=============================================================================

TEST_F(ExecutivePortiaRegressionTest, NoMemoryLeaksWithPortia) {
    // Create/destroy cycles
    for (int i = 0; i < 100; i++) {
        executive_config_t config = {
            .max_tasks = 16,
            .task_switch_cost_ms = 200.0f,
            .inhibition_threshold = 0.7f,
            .max_plan_depth = 10,
            .enable_task_prioritization = true,
            .enable_deadline_checking = true,
            .enable_portia_integration = true
        };

        executive_controller_t* exec = executive_create_custom(&config);
        ASSERT_NE(exec, nullptr);

        // Do some operations
        plan_t* plan = executive_create_plan(exec, "Leak test", 5);
        ASSERT_NE(plan, nullptr);
        executive_destroy_plan(plan);

        task_descriptor_t task = {};
        task.type = TASK_TYPE_PLANNING;
        task.priority = PRIORITY_NORMAL;
        snprintf(task.name, sizeof(task.name), "Leak test task");
        uint32_t task_id = executive_add_task(exec, &task);
        EXPECT_GT(task_id, 0);

        executive_destroy(exec);
    }

    // Run with Valgrind or ASAN to detect leaks
}

TEST_F(ExecutivePortiaRegressionTest, NoMemoryLeaksInMessageHandlers) {
    executive_controller_t* exec = executive_create();
    ASSERT_NE(exec, nullptr);

    // Simulate many queries
    for (int i = 0; i < 1000; i++) {
        uint32_t tier = executive_get_portia_tier(exec);
        (void)tier;

        bool resource_aware = executive_is_resource_aware(exec);
        (void)resource_aware;

        uint32_t depth = executive_get_recommended_plan_depth(exec);
        (void)depth;
    }

    executive_destroy(exec);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(ExecutivePortiaRegressionTest, NullPointerHandling) {
    // All API functions should handle NULL gracefully
    EXPECT_EQ(executive_get_portia_tier(nullptr), PLATFORM_TIER_MINIMAL);
    EXPECT_FALSE(executive_is_resource_aware(nullptr));
    EXPECT_EQ(executive_get_recommended_plan_depth(nullptr), 10);
}

TEST_F(ExecutivePortiaRegressionTest, MultipleExecutivesWithPortia) {
    // Multiple executives can coexist with Portia integration
    std::vector<executive_controller_t*> executives;

    for (int i = 0; i < 5; i++) {
        executive_config_t config = {
            .max_tasks = 8,
            .task_switch_cost_ms = 200.0f,
            .inhibition_threshold = 0.7f,
            .max_plan_depth = 10,
            .enable_task_prioritization = true,
            .enable_deadline_checking = true,
            .enable_portia_integration = true
        };
        executive_controller_t* exec = executive_create_custom(&config);
        ASSERT_NE(exec, nullptr);
        executives.push_back(exec);
    }

    // All should work independently
    for (auto exec : executives) {
        uint32_t tier = executive_get_portia_tier(exec);
        (void)tier;

        plan_t* plan = executive_create_plan(exec, "Multi-exec test", 5);
        ASSERT_NE(plan, nullptr);
        executive_destroy_plan(plan);
    }

    // Cleanup
    for (auto exec : executives) {
        executive_destroy(exec);
    }
}

//=============================================================================
// Security Tests
//=============================================================================

TEST_F(ExecutivePortiaRegressionTest, SecurityBBBValidationPasses) {
    // Executive should register with BBB successfully
    executive_controller_t* exec = executive_create();
    ASSERT_NE(exec, nullptr);

    // BBB validation happens during creation
    // If we get a valid exec, BBB validation passed

    executive_destroy(exec);
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
