/**
 * @file test_portia_planning_attention_integration.cpp
 * @brief Integration tests for Portia planning and attention resource coordination
 *
 * WHAT: Tests planning requests attention resources and coordination
 * WHY:  Validate attention allocation affects planning performance
 * HOW:  Simulate planning tasks with varying attention availability
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/platform/nimcp_platform_tier.h"
#include "portia/nimcp_portia_degradation.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/validation/nimcp_common.h"

// Mock attention system state
typedef struct {
    float available_attention;  // 0.0 - 1.0
    uint32_t active_allocations;
    float total_allocated;
    bool is_initialized;
} mock_attention_state_t;

// Mock planning system state
typedef struct {
    uint32_t active_plans;
    float planning_speed;  // Affected by attention
    uint32_t completed_plans;
    bool has_attention_resource;
    float allocated_attention;
    bool is_initialized;
} mock_planning_state_t;

class PortiaPlanningAttentionIntegrationTest : public ::testing::Test {
protected:
    degradation_state_t* degrade_state = nullptr;
    // bio_ctx not needed - pass NULL to degradation_evaluate
    mock_attention_state_t attention_state;
    mock_planning_state_t planning_state;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);
        // bio_ctx removed - not needed

        // Initialize degradation
        degradation_internal_config_t config = {
            .level_thresholds = {0.0f, 60.0f, 75.0f, 85.0f, 95.0f},
            .hysteresis_ms = 500,
            .enable_auto_degrade = true,
            .enable_auto_restore = true,
            .restore_threshold = 10.0f
        };

        degrade_state = portia_degradation_init(&config);
        ASSERT_NE(degrade_state, nullptr);

        // Register planning feature
        degradation_feature_t planning_feature = {
            FEATURE_PLANNING, "planning", DEGRADATION_LEVEL_SEVERE, 0.5f, false, true
        };
        int result = portia_degradation_register_feature(degrade_state, &planning_feature);
        // Accept either success or already-registered (feature may have been registered by init)
        ASSERT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_ALREADY_EXISTS)
            << "Feature registration failed with code: " << result;

        // Initialize mock systems
        attention_state = {
            .available_attention = 1.0f,
            .active_allocations = 0,
            .total_allocated = 0.0f,
            .is_initialized = true
        };

        planning_state = {
            .active_plans = 0,
            .planning_speed = 1.0f,
            .completed_plans = 0,
            .has_attention_resource = false,
            .allocated_attention = 0.0f,
            .is_initialized = true
        };
    }

    void TearDown() override {
        if (degrade_state) {
            portia_degradation_cleanup(degrade_state);
            degrade_state = nullptr;
        }
        nimcp_bio_async_shutdown();
    }

    // Helper: Request attention for planning
    bool request_attention_for_planning(float amount) {
        if (attention_state.available_attention >= amount) {
            attention_state.available_attention -= amount;
            attention_state.active_allocations++;
            attention_state.total_allocated += amount;

            planning_state.has_attention_resource = true;
            planning_state.allocated_attention = amount;
            planning_state.planning_speed = 0.5f + (amount * 0.5f);  // Speed scales with attention

            return true;
        }
        return false;
    }

    // Helper: Release attention from planning
    void release_attention_from_planning() {
        if (planning_state.has_attention_resource) {
            attention_state.available_attention += planning_state.allocated_attention;
            attention_state.active_allocations--;
            attention_state.total_allocated -= planning_state.allocated_attention;

            planning_state.has_attention_resource = false;
            planning_state.allocated_attention = 0.0f;
            planning_state.planning_speed = 0.5f;  // Baseline speed
        }
    }

    // Helper: Execute planning step
    uint32_t execute_planning_step(uint32_t num_steps) {
        if (!planning_state.has_attention_resource) {
            return 0;  // Can't plan without attention
        }

        uint32_t completed = static_cast<uint32_t>(num_steps * planning_state.planning_speed);
        planning_state.completed_plans += completed;
        return completed;
    }
};

//=============================================================================
// TEST SUITE 1: Planning Requests Attention Resources
//=============================================================================

TEST_F(PortiaPlanningAttentionIntegrationTest, PlanningRequest_SucceedsWithAvailableAttention) {
    // Initially full attention available
    EXPECT_FLOAT_EQ(attention_state.available_attention, 1.0f);
    EXPECT_EQ(attention_state.active_allocations, 0u);

    // Planning requests attention
    bool success = request_attention_for_planning(0.3f);
    EXPECT_TRUE(success);

    // Attention should be allocated
    EXPECT_FLOAT_EQ(attention_state.available_attention, 0.7f);
    EXPECT_EQ(attention_state.active_allocations, 1u);
    EXPECT_FLOAT_EQ(attention_state.total_allocated, 0.3f);

    // Planning should have attention resource
    EXPECT_TRUE(planning_state.has_attention_resource);
    EXPECT_FLOAT_EQ(planning_state.allocated_attention, 0.3f);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, PlanningRequest_FailsWithInsufficientAttention) {
    // Allocate most attention elsewhere
    attention_state.available_attention = 0.1f;
    attention_state.active_allocations = 3;
    attention_state.total_allocated = 0.9f;

    // Planning requests more attention than available
    bool success = request_attention_for_planning(0.3f);
    EXPECT_FALSE(success);

    // Planning should not have attention resource
    EXPECT_FALSE(planning_state.has_attention_resource);
    EXPECT_FLOAT_EQ(planning_state.allocated_attention, 0.0f);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, PlanningRequest_MultipleConcurrentAllocations) {
    // First allocation
    EXPECT_TRUE(request_attention_for_planning(0.2f));
    EXPECT_FLOAT_EQ(attention_state.available_attention, 0.8f);

    // Release and allocate more
    release_attention_from_planning();
    EXPECT_FLOAT_EQ(attention_state.available_attention, 1.0f);

    EXPECT_TRUE(request_attention_for_planning(0.5f));
    EXPECT_FLOAT_EQ(attention_state.available_attention, 0.5f);
}

//=============================================================================
// TEST SUITE 2: Attention Allocation Affects Planning Speed
//=============================================================================

TEST_F(PortiaPlanningAttentionIntegrationTest, AttentionAllocation_HighAttentionFasterPlanning) {
    // Allocate high attention
    ASSERT_TRUE(request_attention_for_planning(0.8f));

    // Planning speed should be high (0.5 + 0.8*0.5 = 0.9)
    EXPECT_GE(planning_state.planning_speed, 0.9f);

    // Execute planning steps
    uint32_t completed = execute_planning_step(100);
    EXPECT_GE(completed, 90u);  // Should complete >=90% of steps
}

TEST_F(PortiaPlanningAttentionIntegrationTest, AttentionAllocation_LowAttentionSlowerPlanning) {
    // Allocate low attention
    ASSERT_TRUE(request_attention_for_planning(0.2f));

    // Planning speed should be reduced
    EXPECT_LT(planning_state.planning_speed, 0.7f);

    // Execute planning steps
    uint32_t completed = execute_planning_step(100);
    EXPECT_LT(completed, 70u);  // Should complete <70% of steps
}

TEST_F(PortiaPlanningAttentionIntegrationTest, AttentionAllocation_NoAttentionNoplanning) {
    // Don't allocate attention
    EXPECT_FALSE(planning_state.has_attention_resource);

    // Execute planning steps
    uint32_t completed = execute_planning_step(100);
    EXPECT_EQ(completed, 0u);  // Can't plan without attention
}

TEST_F(PortiaPlanningAttentionIntegrationTest, AttentionAllocation_SpeedScalesLinearly) {
    // Test different attention levels
    struct {
        float attention;
        float expected_min_speed;
        float expected_max_speed;
    } test_cases[] = {
        {0.0f, 0.5f, 0.5f},   // Baseline
        {0.2f, 0.6f, 0.6f},
        {0.5f, 0.75f, 0.75f},
        {0.8f, 0.9f, 0.9f},
        {1.0f, 1.0f, 1.0f}    // Maximum
    };

    for (auto& test : test_cases) {
        // Reset planning state
        planning_state.has_attention_resource = false;
        planning_state.allocated_attention = 0.0f;
        planning_state.planning_speed = 1.0f;

        // Allocate attention
        if (test.attention > 0.0f) {
            attention_state.available_attention = 1.0f;
            ASSERT_TRUE(request_attention_for_planning(test.attention));

            EXPECT_GE(planning_state.planning_speed, test.expected_min_speed);
            EXPECT_LE(planning_state.planning_speed, test.expected_max_speed);
        }

        release_attention_from_planning();
    }
}

//=============================================================================
// TEST SUITE 3: Planning Completion Releases Resources
//=============================================================================

TEST_F(PortiaPlanningAttentionIntegrationTest, PlanningCompletion_ReleasesAttention) {
    // Allocate attention
    ASSERT_TRUE(request_attention_for_planning(0.4f));
    EXPECT_FLOAT_EQ(attention_state.available_attention, 0.6f);
    EXPECT_EQ(attention_state.active_allocations, 1u);

    // Execute and complete planning
    execute_planning_step(100);

    // Release attention
    release_attention_from_planning();

    // Attention should be released
    EXPECT_FLOAT_EQ(attention_state.available_attention, 1.0f);
    EXPECT_EQ(attention_state.active_allocations, 0u);
    EXPECT_FALSE(planning_state.has_attention_resource);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, PlanningCompletion_EnablesNewPlanning) {
    // First plan
    ASSERT_TRUE(request_attention_for_planning(0.7f));
    execute_planning_step(50);
    uint32_t first_completed = planning_state.completed_plans;

    // Release
    release_attention_from_planning();

    // Second plan (different attention level)
    ASSERT_TRUE(request_attention_for_planning(0.5f));
    execute_planning_step(50);
    uint32_t second_completed = planning_state.completed_plans - first_completed;

    // Both should complete successfully
    EXPECT_GT(first_completed, 0u);
    EXPECT_GT(second_completed, 0u);
}

//=============================================================================
// TEST SUITE 4: Degradation Affects Planning-Attention Interaction
//=============================================================================

TEST_F(PortiaPlanningAttentionIntegrationTest, Degradation_DisablesPlanning) {
    // Initially planning is enabled
    bool planning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLANNING, &planning_enabled), NIMCP_SUCCESS);
    EXPECT_TRUE(planning_enabled);

    // Trigger SEVERE degradation (planning disabled at SEVERE)
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    // Wait for hysteresis period and re-evaluate
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);

    // Planning should now be disabled
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLANNING, &planning_enabled), NIMCP_SUCCESS);
    EXPECT_FALSE(planning_enabled);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, Degradation_ReleasesAttentionWhenPlanningDisabled) {
    // Allocate attention for planning
    ASSERT_TRUE(request_attention_for_planning(0.5f));
    EXPECT_TRUE(planning_state.has_attention_resource);

    // Trigger degradation that disables planning
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    // Wait for hysteresis period and re-evaluate
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);

    // Simulate planning system responding to degradation
    bool planning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLANNING, &planning_enabled), NIMCP_SUCCESS);
    if (!planning_enabled && planning_state.has_attention_resource) {
        release_attention_from_planning();
    }

    // Attention should be released
    EXPECT_FLOAT_EQ(attention_state.available_attention, 1.0f);
    EXPECT_FALSE(planning_state.has_attention_resource);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, Degradation_RestoreEnablesPlanning) {
    // Disable planning via degradation
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);
    // Wait for hysteresis and re-evaluate to complete degradation
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 90.0f, NULL), NIMCP_SUCCESS);

    bool planning_enabled;
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLANNING, &planning_enabled), NIMCP_SUCCESS);
    EXPECT_FALSE(planning_enabled);

    // Wait for hysteresis before restore
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Restore resources
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 50.0f, NULL), NIMCP_SUCCESS);
    // Wait for hysteresis and re-evaluate to complete restore
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    ASSERT_EQ(portia_degradation_evaluate(degrade_state, 50.0f, NULL), NIMCP_SUCCESS);

    // Planning should be re-enabled
    ASSERT_EQ(portia_degradation_is_feature_enabled(degrade_state,
                                                      FEATURE_PLANNING, &planning_enabled), NIMCP_SUCCESS);
    EXPECT_TRUE(planning_enabled);

    // Should be able to allocate attention again
    EXPECT_TRUE(request_attention_for_planning(0.4f));
}

//=============================================================================
// TEST SUITE 5: Concurrent Planning and Attention Scenarios
//=============================================================================

TEST_F(PortiaPlanningAttentionIntegrationTest, Concurrent_MultipleAttentionConsumers) {
    // Simulate planning and other systems competing for attention
    float planning_request = 0.6f;  // Request more than available
    float other_systems = 0.5f;  // Other systems already using attention

    attention_state.available_attention = 1.0f - other_systems;  // 0.5 available

    // Planning request should fail - not enough attention (0.6 > 0.5)
    bool success = request_attention_for_planning(planning_request);
    EXPECT_FALSE(success);  // Not enough remaining

    // Try smaller request that fits
    success = request_attention_for_planning(0.4f);  // 0.4 <= 0.5
    EXPECT_TRUE(success);
}

TEST_F(PortiaPlanningAttentionIntegrationTest, Concurrent_AttentionContentionSlowsPlanning) {
    // Allocate minimal attention due to contention
    ASSERT_TRUE(request_attention_for_planning(0.1f));

    // Planning should be very slow
    uint32_t completed = execute_planning_step(100);
    EXPECT_LT(completed, 60u);  // Significantly reduced throughput

    // Release contention
    release_attention_from_planning();
    attention_state.available_attention = 1.0f;

    // Allocate full attention
    ASSERT_TRUE(request_attention_for_planning(1.0f));

    // Planning should be much faster now
    planning_state.completed_plans = 0;  // Reset
    completed = execute_planning_step(100);
    EXPECT_GT(completed, 95u);
}
