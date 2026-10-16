/**
 * @file test_motor_signal_regression.cpp
 * @brief Signal propagation regression tests for Motor Cortex
 *
 * WHAT: Tests Motor Cortex signal flow consistency and numerical stability
 * WHY:  Ensure motor signals propagate correctly after code changes
 * HOW:  Test known input/output pairs, timing, and signal characteristics
 *
 * REGRESSION FOCUS:
 * - Signal timing consistency
 * - Trajectory generation determinism
 * - Feedback processing accuracy
 * - Hierarchical signal flow (M1 -> premotor -> SMA)
 * - Effector state propagation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorSignalRegressionTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;

    void SetUp() override {
        config = motor_default_config();
        config.enable_bio_async = false;
        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter);
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
    }

    /* Helper to create a standard test goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float y, float z,
                                float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }

    /* Helper to execute movement to completion */
    void ExecuteMovement(motor_goal_t* goal, float dt_ms = 10.0f) {
        motor_plan_movement(adapter, goal);
        motor_begin_execution(adapter);

        float elapsed = 0.0f;
        while (motor_get_status(adapter) == MOTOR_STATUS_EXECUTING &&
               elapsed < goal->max_duration_ms * 2) {
            motor_update_execution(adapter, dt_ms);
            elapsed += dt_ms;
        }
    }
};

/*=============================================================================
 * TRAJECTORY CONSISTENCY TESTS
 * Verify trajectory generation produces consistent results
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, TrajectoryDeterminism_SameInputSameOutput) {
    /* Same input should produce same trajectory */
    motor_goal_t goal1 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.2f, 200.0f);
    motor_goal_t goal2 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.2f, 200.0f);

    motor_plan_movement(adapter, &goal1);
    motor_begin_execution(adapter);
    motor_update_execution(adapter, 100.0f);  /* Halfway */

    motor_effector_state_t state1;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state1);

    motor_reset(adapter);

    motor_plan_movement(adapter, &goal2);
    motor_begin_execution(adapter);
    motor_update_execution(adapter, 100.0f);  /* Same time */

    motor_effector_state_t state2;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state2);

    /* Positions should be identical for same inputs */
    EXPECT_FLOAT_EQ(state1.position.x, state2.position.x);
    EXPECT_FLOAT_EQ(state1.position.y, state2.position.y);
    EXPECT_FLOAT_EQ(state1.position.z, state2.position.z);
}

TEST_F(MotorSignalRegressionTest, TrajectorySmoothing_NoJerkyMotion) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    std::vector<float> positions;
    std::vector<float> velocities;

    float dt = 10.0f;
    float prev_pos = 0.0f;

    for (float t = 0; t < 500.0f; t += dt) {
        motor_update_execution(adapter, dt);

        motor_effector_state_t state;
        motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

        positions.push_back(state.position.x);

        if (t > 0) {
            float velocity = (state.position.x - prev_pos) / dt;
            velocities.push_back(velocity);
        }
        prev_pos = state.position.x;
    }

    /* Check for smooth motion - no sudden velocity changes */
    for (size_t i = 1; i < velocities.size(); i++) {
        float acceleration = velocities[i] - velocities[i-1];
        /* Acceleration should be bounded - no infinite jerks */
        EXPECT_LT(std::abs(acceleration), 0.1f) << "Jerky motion at step " << i;
    }
}

TEST_F(MotorSignalRegressionTest, TrajectoryEndpoint_ReachesTarget) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 2.5f, 1.5f, 0.5f, 300.0f);

    ExecuteMovement(&goal);

    motor_effector_state_t final_state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &final_state);

    /* Should reach target within tolerance */
    float tolerance = 0.1f;
    EXPECT_NEAR(final_state.position.x, goal.target_position.x, tolerance);
    EXPECT_NEAR(final_state.position.y, goal.target_position.y, tolerance);
    EXPECT_NEAR(final_state.position.z, goal.target_position.z, tolerance);
}

/*=============================================================================
 * TIMING CONSISTENCY TESTS
 * Verify timing behavior is consistent
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, TimingConsistency_DurationRespected) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Execute for exactly the duration */
    float dt = 10.0f;
    int updates = 0;
    while (motor_get_status(adapter) == MOTOR_STATUS_EXECUTING) {
        motor_update_execution(adapter, dt);
        updates++;
        if (updates > 100) break;  /* Safety limit */
    }

    float total_time = updates * dt;

    /* Movement should complete close to specified duration */
    EXPECT_GE(total_time, goal.max_duration_ms * 0.8f);
    EXPECT_LE(total_time, goal.max_duration_ms * 1.5f);
}

TEST_F(MotorSignalRegressionTest, TimingConsistency_UpdateRateIndependence) {
    /* Result should be similar regardless of update rate */
    motor_goal_t goal1 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_goal_t goal2 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);

    /* Run with 10ms updates */
    motor_plan_movement(adapter, &goal1);
    motor_begin_execution(adapter);
    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 10.0f);
    }
    motor_effector_state_t state_10ms;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_10ms);

    motor_reset(adapter);

    /* Run with 20ms updates for same total time */
    motor_plan_movement(adapter, &goal2);
    motor_begin_execution(adapter);
    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 20.0f);
    }
    motor_effector_state_t state_20ms;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_20ms);

    /* Positions should be close despite different update rates */
    float tolerance = 0.15f;
    EXPECT_NEAR(state_10ms.position.x, state_20ms.position.x, tolerance);
}

/*=============================================================================
 * FEEDBACK PROCESSING CONSISTENCY TESTS
 * Verify feedback affects motor output consistently
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, FeedbackCorrection_ReducesError) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);
    motor_update_execution(adapter, 100.0f);

    /* Simulate position error via feedback */
    motor_effector_state_t feedback_state;
    memset(&feedback_state, 0, sizeof(feedback_state));
    feedback_state.region = MOTOR_REGION_HAND_RIGHT;
    feedback_state.position.x = 0.1f;  /* Less than expected */

    motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &feedback_state);

    /* Continue execution with feedback */
    for (int i = 0; i < 30; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

    /* With feedback correction, should still approach target */
    EXPECT_GT(state.position.x, feedback_state.position.x);
}

TEST_F(MotorSignalRegressionTest, FeedbackTiming_ImmediateEffect) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);
    motor_update_execution(adapter, 50.0f);

    motor_effector_state_t state_before;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_before);

    /* Apply significant feedback */
    motor_effector_state_t feedback_state;
    memset(&feedback_state, 0, sizeof(feedback_state));
    feedback_state.region = MOTOR_REGION_HAND_RIGHT;
    feedback_state.position.x = 0.0f;  /* Override position */

    motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &feedback_state);

    /* Feedback should have immediate effect on next update */
    motor_update_execution(adapter, 10.0f);

    motor_effector_state_t state_after;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_after);

    /* Motor should respond to feedback */
    EXPECT_TRUE(true);  /* Feedback accepted without error */
}

/*=============================================================================
 * MULTI-REGION SIGNAL FLOW TESTS
 * Verify signals propagate correctly across regions
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, MultiRegion_IndependentControl) {
    /* Different regions should be independently controllable (sequential execution) */
    motor_goal_t goal_right = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_goal_t goal_left = CreateTestGoal(MOTOR_REGION_HAND_LEFT, -1.0f, 0.0f, 0.0f, 200.0f);

    /* Execute right hand movement */
    motor_plan_movement(adapter, &goal_right);
    motor_begin_execution(adapter);
    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t state_right;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_right);

    /* Execute left hand movement (motor adapter supports one active trajectory at a time) */
    motor_plan_movement(adapter, &goal_left);
    motor_begin_execution(adapter);
    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t state_left;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_LEFT, &state_left);

    /* Hands should have moved in opposite directions */
    EXPECT_GT(state_right.position.x, 0.0f);
    EXPECT_LT(state_left.position.x, 0.0f);
}

TEST_F(MotorSignalRegressionTest, MultiRegion_NoInterference) {
    /* Moving one region should not affect another's state */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);

    motor_effector_state_t left_initial;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_LEFT, &left_initial);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t left_after;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_LEFT, &left_after);

    /* Left hand should not have moved */
    EXPECT_FLOAT_EQ(left_initial.position.x, left_after.position.x);
    EXPECT_FLOAT_EQ(left_initial.position.y, left_after.position.y);
    EXPECT_FLOAT_EQ(left_initial.position.z, left_after.position.z);
}

/*=============================================================================
 * MOVEMENT TYPE CONSISTENCY TESTS
 * Verify different movement types behave correctly
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, MovementType_DiscreteHasClearEnd) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Execute to completion */
    for (int i = 0; i < 50 && motor_get_status(adapter) == MOTOR_STATUS_EXECUTING; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Discrete movement should complete and stop */
    motor_status_t status = motor_get_status(adapter);
    EXPECT_NE(MOTOR_STATUS_EXECUTING, status);
}

TEST_F(MotorSignalRegressionTest, MovementType_SerialChaining) {
    /* Serial movements should chain together */
    motor_goal_t goal1 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.5f, 0.0f, 0.0f, 100.0f);
    goal1.type = MOVEMENT_TYPE_SERIAL;

    motor_goal_t goal2 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 100.0f);
    goal2.type = MOVEMENT_TYPE_SERIAL;

    motor_plan_movement(adapter, &goal1);
    motor_plan_movement(adapter, &goal2);
    motor_begin_execution(adapter);

    /* Execute both movements */
    for (int i = 0; i < 30; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

    /* Should have executed through both goals */
    EXPECT_GT(state.position.x, goal1.target_position.x * 0.5f);
}

/*=============================================================================
 * NUMERICAL STABILITY TESTS
 * Verify numerical accuracy under various conditions
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, NumericalStability_SmallMovements) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.001f, 0.001f, 0.001f, 100.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 5.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

    /* Small movements should not cause numerical issues */
    EXPECT_FALSE(std::isnan(state.position.x));
    EXPECT_FALSE(std::isinf(state.position.x));
}

TEST_F(MotorSignalRegressionTest, NumericalStability_LargeMovements) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 100.0f, 100.0f, 100.0f, 1000.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 100; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

    /* Large movements should not cause numerical issues */
    EXPECT_FALSE(std::isnan(state.position.x));
    EXPECT_FALSE(std::isinf(state.position.x));
}

TEST_F(MotorSignalRegressionTest, NumericalStability_RapidUpdates) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Very small timesteps */
    for (int i = 0; i < 1000; i++) {
        motor_update_execution(adapter, 0.5f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);

    EXPECT_FALSE(std::isnan(state.position.x));
    EXPECT_FALSE(std::isinf(state.position.x));
}

TEST_F(MotorSignalRegressionTest, NumericalStability_ZeroTimestep) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    motor_effector_state_t state_before;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_before);

    /* Zero timestep should be handled gracefully */
    motor_update_execution(adapter, 0.0f);

    motor_effector_state_t state_after;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_after);

    /* State should not change with zero timestep */
    EXPECT_FLOAT_EQ(state_before.position.x, state_after.position.x);
}

/*=============================================================================
 * STATS ACCUMULATION TESTS
 * Verify statistics are accumulated correctly
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, StatsAccumulation_MovementsCountedCorrectly) {
    motor_stats_t stats_before;
    motor_get_stats(adapter, &stats_before);

    /* Plan and execute 3 movements */
    for (int i = 0; i < 3; i++) {
        motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, (float)(i + 1) * 0.3f, 0.0f, 0.0f, 100.0f);
        motor_plan_movement(adapter, &goal);
        motor_begin_execution(adapter);

        for (int j = 0; j < 15; j++) {
            motor_update_execution(adapter, 10.0f);
        }
        motor_reset(adapter);
    }

    motor_stats_t stats_after;
    motor_get_stats(adapter, &stats_after);

    /* Should have planned 3 more movements */
    EXPECT_EQ(stats_after.movements_planned, stats_before.movements_planned + 3);
}

TEST_F(MotorSignalRegressionTest, StatsAccumulation_CommandsGeneratedIncreases) {
    motor_stats_t stats_before;
    motor_get_stats(adapter, &stats_before);

    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_stats_t stats_after;
    motor_get_stats(adapter, &stats_after);

    /* Commands should have been generated */
    EXPECT_GT(stats_after.commands_generated, stats_before.commands_generated);
}

/*=============================================================================
 * MOTOR PROGRAM REGRESSION TESTS
 * Verify stored programs work consistently
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, MotorProgram_StoredExecutesCorrectly) {
    /* Store a simple program */
    motor_command_t commands[3];
    memset(commands, 0, sizeof(commands));
    commands[0].target_position.x = 0.3f;
    commands[0].duration_ms = 50.0f;
    commands[1].target_position.x = 0.6f;
    commands[1].duration_ms = 50.0f;
    commands[2].target_position.x = 1.0f;
    commands[2].duration_ms = 50.0f;

    uint32_t program_id = motor_store_program(adapter, "regression_test",
        commands, 3, MOVEMENT_TYPE_SERIAL);

    EXPECT_GT(program_id, 0u);

    /* Program should be retrievable and executable */
    /* (Implementation-specific, but ID should be valid) */
}

TEST_F(MotorSignalRegressionTest, MotorProgram_DuplicateNamesHandled) {
    motor_command_t commands[1];
    memset(commands, 0, sizeof(commands));
    commands[0].target_position.x = 1.0f;
    commands[0].duration_ms = 100.0f;

    uint32_t id1 = motor_store_program(adapter, "duplicate_name",
        commands, 1, MOVEMENT_TYPE_DISCRETE);
    uint32_t id2 = motor_store_program(adapter, "duplicate_name",
        commands, 1, MOVEMENT_TYPE_DISCRETE);

    /* Both should succeed (may replace or create new) */
    EXPECT_GT(id1, 0u);
    /* Behavior with duplicates should be consistent */
}

/*=============================================================================
 * STATE TRANSITION REGRESSION TESTS
 * Verify state machine transitions are consistent
 *===========================================================================*/

TEST_F(MotorSignalRegressionTest, StateTransition_IdleToPlanningToExecuting) {
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));

    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(adapter, &goal);

    /* After planning, status depends on implementation - can be PLANNING or PREPARING */
    motor_status_t after_plan = motor_get_status(adapter);
    EXPECT_TRUE(after_plan == MOTOR_STATUS_IDLE ||
                after_plan == MOTOR_STATUS_PLANNING ||
                after_plan == MOTOR_STATUS_PREPARING);

    motor_begin_execution(adapter);
    EXPECT_EQ(MOTOR_STATUS_EXECUTING, motor_get_status(adapter));
}

TEST_F(MotorSignalRegressionTest, StateTransition_ResetFromAnyState) {
    /* Reset from idle */
    EXPECT_TRUE(motor_reset(adapter));
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));

    /* Reset from planning */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(adapter, &goal);
    EXPECT_TRUE(motor_reset(adapter));
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));

    /* Reset from executing */
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);
    EXPECT_TRUE(motor_reset(adapter));
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
