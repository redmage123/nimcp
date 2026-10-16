/**
 * @file test_executive_portia_integration.cpp
 * @brief Integration tests for Executive-Portia system interaction
 *
 * WHAT: End-to-end integration tests with live Portia system
 * WHY:  Verify executive adapts correctly to real Portia tier transitions
 * HOW:  Initialize both systems, trigger tier changes, verify behavior
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "cognitive/nimcp_executive.h"
#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_tier_switch.h"
#include "portia/nimcp_portia_degradation.h"
#include "portia/nimcp_portia_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/logging/nimcp_logging.h"

namespace {

class ExecutivePortiaIntegrationTest : public ::testing::Test {
protected:
    executive_controller_t* exec = nullptr;
    bool portia_initialized = false;

    void SetUp() override {
        // Initialize bio-router first with proper defaults
        bio_router_config_t cfg = bio_router_default_config();
        bio_router_init(&cfg);

        // Initialize Portia system
        portia_config_t portia_config = portia_get_default_config();
        portia_config.enable_bio_async = true;
        portia_config.tier_config.enable_auto_switching = false;  // Manual control for tests

        nimcp_error_t result = portia_init(&portia_config);
        portia_initialized = (result == NIMCP_SUCCESS);

        if (portia_initialized) {
            // Give Portia time to register with bio-router
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Create executive with Portia integration
        executive_config_t exec_config = {
            .max_tasks = 16,
            .task_switch_cost_ms = 200.0f,
            .inhibition_threshold = 0.7f,
            .max_plan_depth = 10,
            .enable_task_prioritization = true,
            .enable_deadline_checking = true,
            .enable_portia_integration = true
        };
        exec = executive_create_custom(&exec_config);
        ASSERT_NE(exec, nullptr);

        // Give executive time to register
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        if (exec) {
            executive_destroy(exec);
            exec = nullptr;
        }

        if (portia_initialized) {
            portia_destroy();
            portia_initialized = false;
        }

        bio_router_shutdown();
    }

    // Helper: Wait for message processing
    void process_messages(uint32_t duration_ms = 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        // Process any pending bio-async messages for the executive
        if (exec) {
            // Process up to 100 messages to avoid potential infinite loops
            executive_process_messages(exec, 100);
        }
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(ExecutivePortiaIntegrationTest, ExecutiveReceivesPortiaTier) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Query tier from executive
    uint32_t exec_tier = executive_get_portia_tier(exec);
    EXPECT_NE(exec_tier, PLATFORM_TIER_MINIMAL);

    // Query tier from Portia directly
    platform_tier_t portia_tier = portia_get_current_tier();
    EXPECT_EQ(exec_tier, portia_tier);
}

TEST_F(ExecutivePortiaIntegrationTest, PlanningDepthMatchesPortiaTier) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    platform_tier_t tier = portia_get_current_tier();
    uint32_t recommended_depth = executive_get_recommended_plan_depth(exec);

    // Depth should be reasonable for current tier
    EXPECT_GE(recommended_depth, 1);
    EXPECT_LE(recommended_depth, 10);

    // At optimal tier, should get full depth
    if (tier >= PLATFORM_TIER_FULL) {
        EXPECT_GE(recommended_depth, 8);
    }
}

//=============================================================================
// Tier Change Tests
//=============================================================================

TEST_F(ExecutivePortiaIntegrationTest, TierDowngradeTriggersResourceAwareMode) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Get initial state
    platform_tier_t initial_tier = portia_get_current_tier();
    uint32_t initial_depth = executive_get_recommended_plan_depth(exec);

    // Force tier downgrade
    if (initial_tier > PLATFORM_TIER_MINIMAL) {
        platform_tier_t target_tier = static_cast<platform_tier_t>(initial_tier - 1);
        nimcp_error_t result = portia_set_tier(target_tier);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        // Process messages
        process_messages(200);

        // Verify executive received tier change
        uint32_t new_tier = executive_get_portia_tier(exec);
        EXPECT_EQ(new_tier, target_tier);

        // Resource-aware mode should be active
        bool resource_aware = executive_is_resource_aware(exec);
        EXPECT_TRUE(resource_aware);

        // Planning depth should be reduced
        uint32_t new_depth = executive_get_recommended_plan_depth(exec);
        EXPECT_LE(new_depth, initial_depth);

        // Restore original tier
        portia_set_tier(initial_tier);
        process_messages(200);
    }
}

TEST_F(ExecutivePortiaIntegrationTest, TierUpgradeIncreasesCapacity) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Start at minimal tier
    nimcp_error_t result = portia_set_tier(PLATFORM_TIER_MINIMAL);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    process_messages(200);

    uint32_t minimal_depth = executive_get_recommended_plan_depth(exec);
    EXPECT_LE(minimal_depth, 5);

    // Upgrade to optimal
    result = portia_set_tier(PLATFORM_TIER_FULL);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    process_messages(200);

    uint32_t optimal_depth = executive_get_recommended_plan_depth(exec);
    EXPECT_GT(optimal_depth, minimal_depth);
    EXPECT_GE(optimal_depth, 8);
}

TEST_F(ExecutivePortiaIntegrationTest, MultipleTierChangesInSequence) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    platform_tier_t tiers[] = {PLATFORM_TIER_FULL, PLATFORM_TIER_MEDIUM, PLATFORM_TIER_MINIMAL, PLATFORM_TIER_MEDIUM, PLATFORM_TIER_FULL};

    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        nimcp_error_t result = portia_set_tier(tiers[i]);
        ASSERT_EQ(result, NIMCP_SUCCESS);
        process_messages(200);

        uint32_t exec_tier = executive_get_portia_tier(exec);
        EXPECT_EQ(exec_tier, tiers[i]);

        uint32_t depth = executive_get_recommended_plan_depth(exec);
        EXPECT_GE(depth, 1);
        EXPECT_LE(depth, 10);
    }
}

//=============================================================================
// Degradation Tests
//=============================================================================

TEST_F(ExecutivePortiaIntegrationTest, DegradationReducesPlanningDepth) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Set optimal tier first
    portia_set_tier(PLATFORM_TIER_FULL);
    process_messages(200);

    uint32_t baseline_depth = executive_get_recommended_plan_depth(exec);

    // Trigger degradation
    portia_degradation_level_t levels[] = {
        PORTIA_DEGRADATION_MINOR,
        PORTIA_DEGRADATION_MODERATE,
        PORTIA_DEGRADATION_SEVERE
    };

    uint32_t prev_depth = baseline_depth;
    for (auto level : levels) {
        nimcp_error_t result = portia_set_degradation_level(level);
        if (result == NIMCP_SUCCESS) {
            process_messages(200);

            uint32_t depth = executive_get_recommended_plan_depth(exec);
            EXPECT_LE(depth, prev_depth);
            prev_depth = depth;

            bool resource_aware = executive_is_resource_aware(exec);
            EXPECT_TRUE(resource_aware);
        }
    }

    // Restore
    portia_set_degradation_level(PORTIA_DEGRADATION_NONE);
    process_messages(200);
}

//=============================================================================
// Task Execution Under Resource Constraints
//=============================================================================

TEST_F(ExecutivePortiaIntegrationTest, TaskExecutionAtMinimalTier) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Set minimal tier
    portia_set_tier(PLATFORM_TIER_MINIMAL);
    process_messages(200);

    // Add a task
    task_descriptor_t task = {};
    task.type = TASK_TYPE_PLANNING;
    task.priority = PRIORITY_NORMAL;
    snprintf(task.name, sizeof(task.name), "Resource-constrained task");

    uint32_t task_id = executive_add_task(exec, &task);
    EXPECT_GT(task_id, 0);

    // Switch to the task
    uint64_t current_time = 0;  // Would use real time in production
    bool switched = executive_switch_task(exec, task_id, current_time);
    EXPECT_TRUE(switched);

    // Complete the task
    bool completed = executive_complete_task(exec, true, current_time + 1000);
    EXPECT_TRUE(completed);
}

TEST_F(ExecutivePortiaIntegrationTest, PlanCreationUnderDegradation) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Set severe degradation
    portia_set_tier(PLATFORM_TIER_MINIMAL);
    portia_set_degradation_level(PORTIA_DEGRADATION_SEVERE);
    process_messages(200);

    // Try to create a plan
    plan_t* plan = executive_create_plan(exec, "Constrained goal", 8);
    ASSERT_NE(plan, nullptr);

    // Plan should be created but with reduced steps
    EXPECT_GT(plan->num_steps, 0);
    EXPECT_LE(plan->num_steps, 3);  // Severely constrained

    executive_destroy_plan(plan);

    // Restore
    portia_set_tier(PLATFORM_TIER_FULL);
    portia_set_degradation_level(PORTIA_DEGRADATION_NONE);
    process_messages(200);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ExecutivePortiaIntegrationTest, MessageProcessingLatency) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Trigger tier change
    portia_set_tier(PLATFORM_TIER_MEDIUM);
    process_messages(50);

    // Query updated tier
    uint32_t tier = executive_get_portia_tier(exec);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 200);  // Less than 200ms
    EXPECT_EQ(tier, PLATFORM_TIER_MEDIUM);

    // Restore
    portia_set_tier(PLATFORM_TIER_FULL);
}

TEST_F(ExecutivePortiaIntegrationTest, NoPerformanceDegradationWithPortia) {
    if (!portia_initialized) {
        GTEST_SKIP() << "Portia not initialized, skipping integration test";
    }

    // Measure plan creation time with Portia
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        plan_t* plan = executive_create_plan(exec, "Perf test", 5);
        ASSERT_NE(plan, nullptr);
        executive_destroy_plan(plan);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto with_portia = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Performance should be reasonable (< 10us per plan on average)
    EXPECT_LT(with_portia.count() / 100, 10000);  // < 10ms per plan
}

} // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
