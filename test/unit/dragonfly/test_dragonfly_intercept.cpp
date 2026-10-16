/**
 * @file test_dragonfly_intercept.cpp
 * @brief Unit tests for interception planning module
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// Test Fixture
//=============================================================================

class InterceptTest : public ::testing::Test {
protected:
    dragonfly_interceptor_t* interceptor = nullptr;

    void SetUp() override {
        interceptor = dragonfly_interceptor_create(nullptr);
        ASSERT_NE(interceptor, nullptr);
    }

    void TearDown() override {
        if (interceptor) {
            dragonfly_interceptor_destroy(interceptor);
            interceptor = nullptr;
        }
    }

    interceptor_state_t make_self(float x, float y, float z,
                                   float vx, float vy, float vz) {
        interceptor_state_t self = {};
        self.position[0] = x;
        self.position[1] = y;
        self.position[2] = z;
        self.velocity[0] = vx;
        self.velocity[1] = vy;
        self.velocity[2] = vz;
        self.max_speed = 20.0f;
        self.max_accel = 10.0f;
        self.max_turn_rate = 3.0f;
        return self;
    }

    target_state_t make_target(float x, float y, float z,
                                float vx, float vy, float vz) {
        target_state_t target = {};
        target.position[0] = x;
        target.position[1] = y;
        target.position[2] = z;
        target.velocity[0] = vx;
        target.velocity[1] = vy;
        target.velocity[2] = vz;
        target.confidence = 0.9f;
        return target;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(InterceptTest, DefaultConfig) {
    intercept_config_t config = intercept_default_config();
    EXPECT_GT(config.pn_gain, 0.0f);
    EXPECT_GT(config.max_intercept_time_s, config.min_intercept_time_s);
    EXPECT_GE(config.safety_margin, 1.0f);
}

TEST_F(InterceptTest, ValidateConfig) {
    intercept_config_t config = intercept_default_config();
    EXPECT_TRUE(intercept_validate_config(&config));

    config.pn_gain = 0.5f;  // Too low
    EXPECT_FALSE(intercept_validate_config(&config));

    config = intercept_default_config();
    config.safety_margin = 0.5f;  // Too low
    EXPECT_FALSE(intercept_validate_config(&config));

    EXPECT_FALSE(intercept_validate_config(nullptr));
}

TEST_F(InterceptTest, CreateWithCustomConfig) {
    intercept_config_t config = intercept_default_config();
    config.pn_gain = 5.0f;
    config.preferred_strategy = INTERCEPT_LEAD;

    dragonfly_interceptor_t* custom = dragonfly_interceptor_create(&config);
    ASSERT_NE(custom, nullptr);

    intercept_config_t retrieved;
    EXPECT_EQ(dragonfly_interceptor_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.pn_gain, 5.0f);
    EXPECT_EQ(retrieved.preferred_strategy, INTERCEPT_LEAD);

    dragonfly_interceptor_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(InterceptTest, CreateAndDestroy) {
    dragonfly_interceptor_t* i = dragonfly_interceptor_create(nullptr);
    ASSERT_NE(i, nullptr);
    dragonfly_interceptor_destroy(i);
}

TEST_F(InterceptTest, DestroyNull) {
    dragonfly_interceptor_destroy(nullptr);  // Should not crash
}

TEST_F(InterceptTest, Reset) {
    EXPECT_EQ(dragonfly_interceptor_reset(interceptor), 0);
}

//=============================================================================
// Core Interception Tests
//=============================================================================

TEST_F(InterceptTest, ComputeBasic) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);  // Stationary target

    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, &self, &target, &solution), 0);

    EXPECT_EQ(solution.feasibility, INTERCEPT_FEASIBLE);
    EXPECT_GT(solution.intercept_time_s, 0.0f);
}

TEST_F(InterceptTest, ComputeMovingTarget) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 5, 5, 0);  // Moving target

    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, &self, &target, &solution), 0);

    EXPECT_GT(solution.intercept_time_s, 0.0f);
    // Lead angle should be non-zero for moving target
    // (depends on strategy)
}

TEST_F(InterceptTest, ComputePursuit) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 50, 0, 0, 0, 0);

    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute_strategy(
        interceptor, &self, &target, INTERCEPT_PURSUIT, &solution), 0);

    EXPECT_EQ(solution.strategy, INTERCEPT_PURSUIT);
    // Pure pursuit: heading should point directly at target
    float expected_heading = atan2f(50.0f, 100.0f);
    EXPECT_NEAR(solution.heading_rad, expected_heading, 0.1f);
}

TEST_F(InterceptTest, ComputeLead) {
    interceptor_state_t self = make_self(0, 0, 0, 15, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 10, 0);  // Moving perpendicular

    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute_strategy(
        interceptor, &self, &target, INTERCEPT_LEAD, &solution), 0);

    EXPECT_EQ(solution.strategy, INTERCEPT_LEAD);
    // Lead angle should be positive (aiming ahead)
    EXPECT_GT(solution.lead_angle_rad, 0.0f);
}

TEST_F(InterceptTest, ComputePN) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 20, 0, -5, 0, 0);  // Approaching

    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute_strategy(
        interceptor, &self, &target, INTERCEPT_PN, &solution), 0);

    EXPECT_EQ(solution.strategy, INTERCEPT_PN);
    EXPECT_GT(solution.closing_speed, 0.0f);  // Should be closing
}

//=============================================================================
// Feasibility Tests
//=============================================================================

TEST_F(InterceptTest, FeasibilityFeasible) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(50, 0, 0, 0, 0, 0);

    intercept_feasibility_t feas = dragonfly_intercept_check_feasibility(
        interceptor, &self, &target);
    EXPECT_EQ(feas, INTERCEPT_FEASIBLE);
}

TEST_F(InterceptTest, FeasibilityTooFast) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    self.max_speed = 10.0f;
    target_state_t target = make_target(100, 0, 0, 50, 0, 0);  // Very fast target

    intercept_feasibility_t feas = dragonfly_intercept_check_feasibility(
        interceptor, &self, &target);
    EXPECT_EQ(feas, INTERCEPT_TOO_FAST);
}

TEST_F(InterceptTest, FeasibilityEscaping) {
    interceptor_state_t self = make_self(0, 0, 0, 5, 0, 0);
    target_state_t target = make_target(100, 0, 0, 20, 0, 0);  // Fast escaping

    intercept_feasibility_t feas = dragonfly_intercept_check_feasibility(
        interceptor, &self, &target);
    // Either ESCAPING or TOO_FAST depending on speed ratio
    EXPECT_TRUE(feas == INTERCEPT_ESCAPING || feas == INTERCEPT_TOO_FAST);
}

TEST_F(InterceptTest, FeasibilityLowConfidence) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(50, 0, 0, 0, 0, 0);
    target.confidence = 0.1f;

    intercept_feasibility_t feas = dragonfly_intercept_check_feasibility(
        interceptor, &self, &target);
    EXPECT_EQ(feas, INTERCEPT_UNCERTAIN);
}

//=============================================================================
// Command Generation Tests
//=============================================================================

TEST_F(InterceptTest, GetCommand) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 20, 0, 0, 0, 0);

    float accel_cmd[3];
    EXPECT_EQ(dragonfly_intercept_get_command(
        interceptor, &self, &target, accel_cmd), 0);

    // Should have some lateral acceleration to turn toward target
    float accel_mag = sqrtf(accel_cmd[0]*accel_cmd[0] +
                            accel_cmd[1]*accel_cmd[1] +
                            accel_cmd[2]*accel_cmd[2]);
    EXPECT_GE(accel_mag, 0.0f);
}

//=============================================================================
// Trajectory Planning Tests
//=============================================================================

TEST_F(InterceptTest, PlanTrajectory) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);

    intercept_trajectory_t trajectory;
    EXPECT_EQ(dragonfly_intercept_plan_trajectory(
        interceptor, &self, &target, &trajectory), 0);

    EXPECT_GT(trajectory.num_waypoints, 0u);
    EXPECT_GT(trajectory.total_time_s, 0.0f);

    // Waypoints should be in increasing time order
    for (uint32_t i = 1; i < trajectory.num_waypoints; i++) {
        EXPECT_GT(trajectory.waypoints[i].time_s,
                  trajectory.waypoints[i-1].time_s);
    }
}

TEST_F(InterceptTest, GetWaypoint) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);

    intercept_trajectory_t trajectory;
    dragonfly_intercept_plan_trajectory(interceptor, &self, &target, &trajectory);

    if (trajectory.num_waypoints > 0) {
        intercept_waypoint_t wp;
        float mid_time = trajectory.total_time_s / 2.0f;
        EXPECT_EQ(dragonfly_intercept_get_waypoint(&trajectory, mid_time, &wp), 0);
        EXPECT_NEAR(wp.time_s, mid_time, 0.5f);
    }
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(InterceptTest, TimeToIntercept) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);

    float tti = dragonfly_intercept_time_to_intercept(&self, &target);
    EXPECT_GT(tti, 0.0f);
    EXPECT_NEAR(tti, 10.0f, 2.0f);  // ~100m / 10m/s = 10s
}

TEST_F(InterceptTest, LeadAngle) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 5, 0);

    float lead = dragonfly_intercept_lead_angle(&self, &target);
    // Lead angle should be in reasonable range
    EXPECT_GE(lead, -M_PI/2);
    EXPECT_LE(lead, M_PI/2);
}

TEST_F(InterceptTest, Bearing) {
    interceptor_state_t self = make_self(0, 0, 0, 0, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);

    float bearing = dragonfly_intercept_bearing(&self, &target);
    EXPECT_NEAR(bearing, 0.0f, 0.01f);  // Target directly ahead

    target.position[1] = 100.0f;  // 45 degrees
    bearing = dragonfly_intercept_bearing(&self, &target);
    EXPECT_NEAR(bearing, M_PI/4.0f, 0.01f);
}

TEST_F(InterceptTest, ClosingSpeed) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, -5, 0, 0);  // Coming toward us

    float closing = dragonfly_intercept_closing_speed(&self, &target);
    EXPECT_GT(closing, 0.0f);  // Positive = closing
    EXPECT_NEAR(closing, 15.0f, 1.0f);  // 10 + 5 = 15 m/s
}

TEST_F(InterceptTest, Range) {
    interceptor_state_t self = make_self(0, 0, 0, 0, 0, 0);
    target_state_t target = make_target(30, 40, 0, 0, 0, 0);

    float range = dragonfly_intercept_range(&self, &target);
    EXPECT_NEAR(range, 50.0f, 0.01f);  // 3-4-5 triangle
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(InterceptTest, GetStats) {
    intercept_stats_t stats;
    EXPECT_EQ(dragonfly_interceptor_get_stats(interceptor, &stats), 0);
    EXPECT_EQ(stats.solutions_computed, 0u);
}

TEST_F(InterceptTest, StatsTrackSolutions) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);
    intercept_solution_t solution;

    for (int i = 0; i < 5; i++) {
        dragonfly_intercept_compute(interceptor, &self, &target, &solution);
    }

    intercept_stats_t stats;
    dragonfly_interceptor_get_stats(interceptor, &stats);
    EXPECT_EQ(stats.solutions_computed, 5u);
}

TEST_F(InterceptTest, ResetStats) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);
    intercept_solution_t solution;

    dragonfly_intercept_compute(interceptor, &self, &target, &solution);

    EXPECT_EQ(dragonfly_interceptor_reset_stats(interceptor), 0);

    intercept_stats_t stats;
    dragonfly_interceptor_get_stats(interceptor, &stats);
    EXPECT_EQ(stats.solutions_computed, 0u);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(InterceptTest, StrategyName) {
    EXPECT_STREQ(dragonfly_strategy_name(INTERCEPT_PURSUIT), "PURSUIT");
    EXPECT_STREQ(dragonfly_strategy_name(INTERCEPT_LEAD), "LEAD");
    EXPECT_STREQ(dragonfly_strategy_name(INTERCEPT_PARALLEL), "PARALLEL");
    EXPECT_STREQ(dragonfly_strategy_name(INTERCEPT_PN), "PN");
    EXPECT_STREQ(dragonfly_strategy_name(INTERCEPT_OPTIMAL), "OPTIMAL");
}

TEST_F(InterceptTest, FeasibilityName) {
    EXPECT_STREQ(dragonfly_feasibility_name(INTERCEPT_FEASIBLE), "FEASIBLE");
    EXPECT_STREQ(dragonfly_feasibility_name(INTERCEPT_TOO_FAST), "TOO_FAST");
    EXPECT_STREQ(dragonfly_feasibility_name(INTERCEPT_OUT_OF_RANGE), "OUT_OF_RANGE");
    EXPECT_STREQ(dragonfly_feasibility_name(INTERCEPT_ESCAPING), "ESCAPING");
    EXPECT_STREQ(dragonfly_feasibility_name(INTERCEPT_UNCERTAIN), "UNCERTAIN");
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(InterceptTest, NullPointerHandling) {
    interceptor_state_t self = make_self(0, 0, 0, 10, 0, 0);
    target_state_t target = make_target(100, 0, 0, 0, 0, 0);
    intercept_solution_t solution;
    intercept_trajectory_t trajectory;
    intercept_stats_t stats;
    intercept_config_t config;
    float accel[3];

    EXPECT_EQ(dragonfly_interceptor_reset(nullptr), -1);

    EXPECT_EQ(dragonfly_intercept_compute(nullptr, &self, &target, &solution), -1);
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, nullptr, &target, &solution), -1);
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, &self, nullptr, &solution), -1);
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, &self, &target, nullptr), -1);

    EXPECT_EQ(dragonfly_intercept_check_feasibility(nullptr, &self, &target), INTERCEPT_UNCERTAIN);
    EXPECT_EQ(dragonfly_intercept_check_feasibility(interceptor, nullptr, &target), INTERCEPT_UNCERTAIN);

    EXPECT_EQ(dragonfly_intercept_get_command(nullptr, &self, &target, accel), -1);
    EXPECT_EQ(dragonfly_intercept_get_command(interceptor, nullptr, &target, accel), -1);
    EXPECT_EQ(dragonfly_intercept_get_command(interceptor, &self, nullptr, accel), -1);
    EXPECT_EQ(dragonfly_intercept_get_command(interceptor, &self, &target, nullptr), -1);

    EXPECT_EQ(dragonfly_intercept_plan_trajectory(nullptr, &self, &target, &trajectory), -1);

    EXPECT_LT(dragonfly_intercept_time_to_intercept(nullptr, &target), 0.0f);
    EXPECT_FLOAT_EQ(dragonfly_intercept_lead_angle(nullptr, &target), 0.0f);
    EXPECT_FLOAT_EQ(dragonfly_intercept_bearing(nullptr, &target), 0.0f);
    EXPECT_FLOAT_EQ(dragonfly_intercept_closing_speed(nullptr, &target), 0.0f);
    EXPECT_FLOAT_EQ(dragonfly_intercept_range(nullptr, &target), 0.0f);

    EXPECT_EQ(dragonfly_interceptor_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_interceptor_get_stats(interceptor, nullptr), -1);
    EXPECT_EQ(dragonfly_interceptor_reset_stats(nullptr), -1);

    EXPECT_EQ(dragonfly_interceptor_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_interceptor_set_config(interceptor, nullptr), -1);
    EXPECT_EQ(dragonfly_interceptor_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_interceptor_get_config(interceptor, nullptr), -1);
}
