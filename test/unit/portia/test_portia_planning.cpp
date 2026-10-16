/**
 * @file test_portia_planning.cpp
 * @brief Unit tests for Portia Planning System
 *
 * WHAT: Comprehensive tests for memory-constrained route planning
 * WHY:  Verify Portia spider-inspired planning behaviors
 * HOW:  Test plan creation, waypoint management, detours, backtracking
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_planning.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaPlanningTest : public ::testing::Test {
protected:
    portia_planner_t planner;
    portia_planning_config_t config;

    void SetUp() override {
        // Initialize with reasonable defaults
        config.max_waypoints = 16;
        config.max_plans = 4;
        config.max_detour_depth = 3;
        config.scan_interval_s = 0.1f;
        config.confidence_threshold = 0.6f;
        config.enable_backtracking = true;

        planner = portia_planning_init(&config, nullptr);
        ASSERT_NE(planner, nullptr) << "Failed to initialize planner";
    }

    void TearDown() override {
        if (planner) {
            portia_planning_destroy(planner);
            planner = nullptr;
        }
    }

    // Helper to calculate distance
    float distance(float x1, float y1, float z1,
                    float x2, float y2, float z2) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dz = z2 - z1;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(PortiaPlanningTest, InitializeWithValidConfig) {
    EXPECT_NE(planner, nullptr);
}

TEST_F(PortiaPlanningTest, InitializeWithNullConfig) {
    portia_planner_t null_planner = portia_planning_init(nullptr, nullptr);
    EXPECT_EQ(null_planner, nullptr);
}

TEST_F(PortiaPlanningTest, InitializeWithInvalidMaxWaypoints) {
    portia_planning_config_t bad_config = config;
    bad_config.max_waypoints = 1;  // Too small

    portia_planner_t bad_planner = portia_planning_init(&bad_config, nullptr);
    EXPECT_EQ(bad_planner, nullptr);
}

TEST_F(PortiaPlanningTest, InitializeWithInvalidMaxPlans) {
    portia_planning_config_t bad_config = config;
    bad_config.max_plans = 0;  // Too small

    portia_planner_t bad_planner = portia_planning_init(&bad_config, nullptr);
    EXPECT_EQ(bad_planner, nullptr);
}

//=============================================================================
// Plan Creation Tests
//=============================================================================

TEST_F(PortiaPlanningTest, CreateSimplePlan) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 5.0f, 0.0f);

    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->waypoint_count, 1);
    EXPECT_EQ(plan->current_waypoint, 0);
    EXPECT_EQ(plan->state, PLAN_STATE_SCANNING);
    EXPECT_FLOAT_EQ(plan->waypoints[0].x, 10.0f);
    EXPECT_FLOAT_EQ(plan->waypoints[0].y, 5.0f);
    EXPECT_FLOAT_EQ(plan->waypoints[0].z, 0.0f);
    EXPECT_FLOAT_EQ(plan->waypoints[0].confidence, 1.0f);
    EXPECT_TRUE(plan->waypoints[0].visible);
}

TEST_F(PortiaPlanningTest, CreateMultiplePlans) {
    portia_plan_t* plan1 = portia_planning_create_plan(planner, 10.0f, 0.0f, 0.0f);
    portia_plan_t* plan2 = portia_planning_create_plan(planner, 0.0f, 10.0f, 0.0f);
    portia_plan_t* plan3 = portia_planning_create_plan(planner, 5.0f, 5.0f, 0.0f);

    EXPECT_NE(plan1, nullptr);
    EXPECT_NE(plan2, nullptr);
    EXPECT_NE(plan3, nullptr);
    EXPECT_NE(plan1->id, plan2->id);
    EXPECT_NE(plan2->id, plan3->id);
}

TEST_F(PortiaPlanningTest, CreatePlanExceedsMaxPlans) {
    // Create max plans
    for (uint32_t i = 0; i < config.max_plans; i++) {
        portia_plan_t* plan = portia_planning_create_plan(planner,
            (float)i, (float)i, 0.0f);
        EXPECT_NE(plan, nullptr);
    }

    // Try to exceed limit
    portia_plan_t* extra = portia_planning_create_plan(planner, 100.0f, 100.0f, 0.0f);
    EXPECT_EQ(extra, nullptr);
}

//=============================================================================
// Waypoint Management Tests
//=============================================================================

TEST_F(PortiaPlanningTest, AddWaypoint) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    bool added = portia_planning_add_waypoint(planner, plan->id,
        5.0f, 5.0f, 0.0f, 0.9f);

    EXPECT_TRUE(added);
    EXPECT_EQ(plan->waypoint_count, 2);
    EXPECT_FLOAT_EQ(plan->waypoints[1].x, 5.0f);
    EXPECT_FLOAT_EQ(plan->waypoints[1].y, 5.0f);
    EXPECT_FLOAT_EQ(plan->waypoints[1].confidence, 0.9f);
}

TEST_F(PortiaPlanningTest, AddMultipleWaypoints) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    for (uint32_t i = 1; i <= 5; i++) {
        bool added = portia_planning_add_waypoint(planner, plan->id,
            (float)i * 4.0f, (float)i * 4.0f, 0.0f, 0.95f);
        EXPECT_TRUE(added);
    }

    EXPECT_EQ(plan->waypoint_count, 6);  // 1 target + 5 waypoints
}

TEST_F(PortiaPlanningTest, AddWaypointExceedsLimit) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 100.0f, 100.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Add waypoints up to limit
    for (uint32_t i = 1; i < config.max_waypoints; i++) {
        bool added = portia_planning_add_waypoint(planner, plan->id,
            (float)i, (float)i, 0.0f, 0.9f);
        EXPECT_TRUE(added);
    }

    // Try to exceed limit
    bool exceeded = portia_planning_add_waypoint(planner, plan->id,
        1000.0f, 1000.0f, 0.0f, 0.9f);
    EXPECT_FALSE(exceeded);
}

TEST_F(PortiaPlanningTest, AddWaypointInvalidConfidence) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Negative confidence
    bool added1 = portia_planning_add_waypoint(planner, plan->id,
        5.0f, 5.0f, 0.0f, -0.5f);
    EXPECT_FALSE(added1);

    // Confidence > 1.0
    bool added2 = portia_planning_add_waypoint(planner, plan->id,
        5.0f, 5.0f, 0.0f, 1.5f);
    EXPECT_FALSE(added2);
}

//=============================================================================
// Plan Evaluation Tests
//=============================================================================

TEST_F(PortiaPlanningTest, EvaluatePlan) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.8f);

    bool evaluated = portia_planning_evaluate(planner, plan->id);
    EXPECT_TRUE(evaluated);
    EXPECT_EQ(plan->state, PLAN_STATE_EVALUATING);
}

TEST_F(PortiaPlanningTest, EvaluateInvalidPlan) {
    bool evaluated = portia_planning_evaluate(planner, 99999);
    EXPECT_FALSE(evaluated);
}

//=============================================================================
// Plan Execution Tests
//=============================================================================

TEST_F(PortiaPlanningTest, ExecuteSimplePlan) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Execute single step (should complete immediately)
    bool executed = portia_planning_execute_step(planner, plan->id);
    EXPECT_TRUE(executed);
    EXPECT_EQ(plan->state, PLAN_STATE_COMPLETE);
    EXPECT_FLOAT_EQ(plan->progress, 1.0f);
}

TEST_F(PortiaPlanningTest, ExecuteMultiStepPlan) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Add intermediate waypoints
    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);
    portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.9f);
    portia_planning_add_waypoint(planner, plan->id, 15.0f, 15.0f, 0.0f, 0.9f);

    EXPECT_EQ(plan->waypoint_count, 4);

    // Execute steps
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_NE(plan->state, PLAN_STATE_COMPLETE);
        bool executed = portia_planning_execute_step(planner, plan->id);
        EXPECT_TRUE(executed);
    }

    EXPECT_EQ(plan->state, PLAN_STATE_COMPLETE);
    EXPECT_FLOAT_EQ(plan->progress, 1.0f);
}

TEST_F(PortiaPlanningTest, ExecuteStepInvalidPlan) {
    bool executed = portia_planning_execute_step(planner, 99999);
    EXPECT_FALSE(executed);
}

//=============================================================================
// Detour Tests
//=============================================================================

TEST_F(PortiaPlanningTest, CanDetourWithinLimit) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Add waypoints with low confidence (invisible)
    for (uint32_t i = 0; i < config.max_detour_depth; i++) {
        portia_planning_add_waypoint(planner, plan->id,
            (float)i, (float)i, 0.0f, 0.3f);  // Below threshold
    }

    portia_planning_evaluate(planner, plan->id);
    bool can_detour = portia_planning_can_detour(planner, plan->id);
    EXPECT_TRUE(can_detour);
}

TEST_F(PortiaPlanningTest, CannotDetourExceedsLimit) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Add more invisible waypoints than detour limit
    for (uint32_t i = 0; i < config.max_detour_depth + 2; i++) {
        portia_planning_add_waypoint(planner, plan->id,
            (float)i, (float)i, 0.0f, 0.3f);  // Below threshold
    }

    portia_planning_evaluate(planner, plan->id);
    bool can_detour = portia_planning_can_detour(planner, plan->id);
    EXPECT_FALSE(can_detour);
}

//=============================================================================
// Obstacle Handling Tests
//=============================================================================

TEST_F(PortiaPlanningTest, HandleObstacleWithBacktracking) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 20.0f, 20.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    portia_planning_add_waypoint(planner, plan->id, 10.0f, 10.0f, 0.0f, 0.9f);

    // Execute one step
    portia_planning_execute_step(planner, plan->id);
    EXPECT_EQ(plan->current_waypoint, 1);

    // Hit obstacle
    bool handled = portia_planning_handle_obstacle(planner, plan->id,
        15.0f, 15.0f, 0.0f);
    EXPECT_TRUE(handled);
    EXPECT_EQ(plan->current_waypoint, 0);  // Backtracked
    EXPECT_EQ(plan->state, PLAN_STATE_EVALUATING);
}

TEST_F(PortiaPlanningTest, HandleObstacleWithoutBacktrackingOption) {
    // Create planner without backtracking
    portia_planning_config_t no_backtrack = config;
    no_backtrack.enable_backtracking = false;

    portia_planner_t no_bt_planner = portia_planning_init(&no_backtrack, nullptr);
    ASSERT_NE(no_bt_planner, nullptr);

    portia_plan_t* plan = portia_planning_create_plan(no_bt_planner,
        10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Hit obstacle
    bool handled = portia_planning_handle_obstacle(no_bt_planner, plan->id,
        5.0f, 5.0f, 0.0f);
    EXPECT_FALSE(handled);
    EXPECT_EQ(plan->state, PLAN_STATE_FAILED);

    portia_planning_destroy(no_bt_planner);
}

TEST_F(PortiaPlanningTest, HandleObstacleCannotBacktrackFurther) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Already at start, cannot backtrack
    bool handled = portia_planning_handle_obstacle(planner, plan->id,
        5.0f, 5.0f, 0.0f);
    EXPECT_FALSE(handled);
    EXPECT_EQ(plan->state, PLAN_STATE_FAILED);
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(PortiaPlanningTest, GetPlanState) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    plan_state_t state = portia_planning_get_state(planner, plan->id);
    EXPECT_EQ(state, PLAN_STATE_SCANNING);

    // Execute and check again
    portia_planning_execute_step(planner, plan->id);
    state = portia_planning_get_state(planner, plan->id);
    EXPECT_EQ(state, PLAN_STATE_COMPLETE);
}

TEST_F(PortiaPlanningTest, GetPlanById) {
    portia_plan_t* plan1 = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan1, nullptr);

    portia_plan_t* retrieved = portia_planning_get_plan(planner, plan1->id);
    EXPECT_EQ(retrieved, plan1);
    EXPECT_EQ(retrieved->id, plan1->id);
}

TEST_F(PortiaPlanningTest, GetInvalidPlan) {
    portia_plan_t* plan = portia_planning_get_plan(planner, 99999);
    EXPECT_EQ(plan, nullptr);
}

//=============================================================================
// Plan Deletion Tests
//=============================================================================

TEST_F(PortiaPlanningTest, DeletePlan) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);
    uint32_t plan_id = plan->id;

    bool deleted = portia_planning_delete_plan(planner, plan_id);
    EXPECT_TRUE(deleted);

    // Verify plan no longer exists
    portia_plan_t* retrieved = portia_planning_get_plan(planner, plan_id);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(PortiaPlanningTest, DeleteInvalidPlan) {
    bool deleted = portia_planning_delete_plan(planner, 99999);
    EXPECT_FALSE(deleted);
}

//=============================================================================
// Confidence Decay Tests (Integration)
//=============================================================================

TEST_F(PortiaPlanningTest, ConfidenceDecayOverTime) {
    portia_plan_t* plan = portia_planning_create_plan(planner, 10.0f, 10.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);

    float initial_confidence = plan->waypoints[1].confidence;
    EXPECT_FLOAT_EQ(initial_confidence, 0.9f);

    // Wait a bit (simulate time passing)
    nimcp_time_sleep_ms(100);

    // Re-evaluate to trigger confidence decay
    portia_planning_evaluate(planner, plan->id);

    float decayed_confidence = plan->waypoints[1].confidence;
    EXPECT_LT(decayed_confidence, initial_confidence);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(PortiaPlanningTest, ErrorMessageAvailable) {
    // Trigger an error
    portia_planning_create_plan(nullptr, 0.0f, 0.0f, 0.0f);

    const char* error = portia_planning_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_GT(strlen(error), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
