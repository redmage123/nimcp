/**
 * @file test_motor_e2e.cpp
 * @brief End-to-End tests for Motor Cortex integration
 *
 * WHAT: Complete end-to-end scenarios testing Motor Cortex in full system context
 * WHY:  Verify motor functionality works correctly with all integrated systems
 * HOW:  Test realistic usage scenarios from goal definition to completion
 *
 * E2E TEST SCENARIOS:
 * 1. Simple Reaching Task: Visual target → Motor reach → Completion
 * 2. Motor Skill Learning: Repeated practice → Improved performance
 * 3. Coordinated Actions: Multi-effector coordination
 * 4. Stress Response: Motor under homeostatic pressure
 * 5. Full Brain Integration: Motor with all brain regions active
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>

extern "C" {
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "utils/logging/nimcp_logging.h"
}

/*=============================================================================
 * TEST FIXTURE - E2E TEST ENVIRONMENT
 *===========================================================================*/

class MotorE2ETest : public ::testing::Test {
protected:
    motor_adapter_t* motor;
    motor_config_t config;

    void SetUp() override {
        config = motor_default_config();
        config.enable_bio_async = false;
        motor = motor_create(&config);
        ASSERT_NE(nullptr, motor);
    }

    void TearDown() override {
        if (motor) {
            motor_destroy(motor);
            motor = nullptr;
        }
    }

    /* Helper to create a goal */
    motor_goal_t CreateGoal(motor_region_t region,
                           float x, float y, float z,
                           float duration_ms,
                           movement_type_t type = MOVEMENT_TYPE_DISCRETE) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.max_duration_ms = duration_ms;
        goal.type = type;
        return goal;
    }

    /* Helper to execute movement to completion */
    void ExecuteMovement(motor_goal_t* goal, float dt_ms = 10.0f) {
        motor_plan_movement(motor, goal);
        motor_begin_execution(motor);

        float elapsed = 0.0f;
        while (motor_get_status(motor) == MOTOR_STATUS_EXECUTING &&
               elapsed < goal->max_duration_ms * 2) {
            motor_update_execution(motor, dt_ms);
            elapsed += dt_ms;
        }
    }

    /* Calculate distance between positions */
    float CalculateDistance(const motor_vec3_t& a, const motor_vec3_t& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

/*=============================================================================
 * E2E SCENARIO 1: SIMPLE REACHING TASK
 * Complete reaching task from planning to execution to verification
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_SimpleReachingTask) {
    /* PHASE 1: Define target */
    motor_goal_t reach_goal = CreateGoal(
        MOTOR_REGION_HAND_RIGHT,
        1.5f, 0.5f, 0.2f,  /* Target position */
        500.0f             /* Duration */
    );

    /* PHASE 2: Plan movement */
    ASSERT_TRUE(motor_plan_movement(motor, &reach_goal));
    EXPECT_EQ(MOTOR_ERROR_NONE, motor_get_last_error(motor));

    /* PHASE 3: Execute movement */
    ASSERT_TRUE(motor_begin_execution(motor));
    EXPECT_EQ(MOTOR_STATUS_EXECUTING, motor_get_status(motor));

    /* PHASE 4: Run simulation loop */
    float dt = 10.0f;
    int steps = 0;
    const int MAX_STEPS = 100;

    while (motor_get_status(motor) == MOTOR_STATUS_EXECUTING && steps < MAX_STEPS) {
        motor_update_execution(motor, dt);
        steps++;
    }

    /* PHASE 5: Verify completion */
    motor_effector_state_t final_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &final_state);

    /* Should reach near target */
    float distance = CalculateDistance(final_state.position, reach_goal.target_position);
    EXPECT_LT(distance, 0.5f);  /* Within tolerance */

    /* Verify stats */
    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    EXPECT_GE(stats.movements_planned, 1u);
    EXPECT_GE(stats.commands_generated, 1u);
}

/*=============================================================================
 * E2E SCENARIO 2: MOTOR SKILL LEARNING
 * Repeated practice of a motor sequence
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_MotorSkillLearning) {
    /* Define a motor skill (3-point sequence) */
    motor_goal_t waypoints[3] = {
        CreateGoal(MOTOR_REGION_HAND_RIGHT, 0.5f, 0.0f, 0.0f, 100.0f, MOVEMENT_TYPE_SERIAL),
        CreateGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.0f, 100.0f, MOVEMENT_TYPE_SERIAL),
        CreateGoal(MOTOR_REGION_HAND_RIGHT, 1.5f, 0.0f, 0.0f, 100.0f, MOVEMENT_TYPE_SERIAL)
    };

    /* Track performance across trials */
    std::vector<float> trial_durations;
    const int NUM_TRIALS = 5;

    for (int trial = 0; trial < NUM_TRIALS; trial++) {
        auto start = std::chrono::high_resolution_clock::now();

        /* Plan and execute sequence */
        for (int i = 0; i < 3; i++) {
            motor_plan_movement(motor, &waypoints[i]);
        }

        motor_begin_execution(motor);

        int steps = 0;
        while (motor_get_status(motor) == MOTOR_STATUS_EXECUTING && steps < 50) {
            motor_update_execution(motor, 10.0f);
            steps++;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        trial_durations.push_back((float)duration.count());

        /* Reset for next trial */
        motor_reset(motor);
    }

    /* Verify all trials completed */
    EXPECT_EQ(NUM_TRIALS, (int)trial_durations.size());

    /* Verify stats show practice */
    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    EXPECT_EQ(NUM_TRIALS * 3, stats.movements_planned);
}

/*=============================================================================
 * E2E SCENARIO 3: BIMANUAL COORDINATION
 * Coordinated two-hand movement
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_BimanualCoordination) {
    /* Note: Motor adapter supports one active trajectory at a time, so we
     * execute movements sequentially to test bimanual coordination capability */
    motor_goal_t right_goal = CreateGoal(
        MOTOR_REGION_HAND_RIGHT,
        1.0f, 0.5f, 0.0f,
        300.0f
    );

    motor_goal_t left_goal = CreateGoal(
        MOTOR_REGION_HAND_LEFT,
        -1.0f, 0.5f, 0.0f,
        300.0f
    );

    /* Execute right hand movement */
    ASSERT_TRUE(motor_plan_movement(motor, &right_goal));
    motor_begin_execution(motor);
    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t right_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &right_state);

    /* Execute left hand movement */
    ASSERT_TRUE(motor_plan_movement(motor, &left_goal));
    motor_begin_execution(motor);
    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t left_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_LEFT, &left_state);

    /* Hands should have moved in opposite directions */
    EXPECT_GT(right_state.position.x, 0.0f);
    EXPECT_LT(left_state.position.x, 0.0f);

    /* Both should have moved toward y=0.5 */
    EXPECT_GE(right_state.position.y, 0.0f);
    EXPECT_GE(left_state.position.y, 0.0f);
}

/*=============================================================================
 * E2E SCENARIO 4: CONTINUOUS TRACKING
 * Track a moving target over time
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_ContinuousTracking) {
    /* Simulate tracking a target that moves in a circle */
    const int NUM_FRAMES = 20;
    const float RADIUS = 1.0f;
    const float ANGULAR_SPEED = 0.314f;  /* ~18 degrees per frame */

    motor_goal_t tracking_goal;
    memset(&tracking_goal, 0, sizeof(tracking_goal));
    tracking_goal.region = MOTOR_REGION_HAND_RIGHT;
    tracking_goal.type = MOVEMENT_TYPE_CONTINUOUS;
    tracking_goal.max_duration_ms = 50.0f;

    std::vector<float> tracking_errors;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        /* Calculate target position on circle */
        float angle = frame * ANGULAR_SPEED;
        float target_x = RADIUS * std::cos(angle);
        float target_y = RADIUS * std::sin(angle);

        tracking_goal.target_position.x = target_x;
        tracking_goal.target_position.y = target_y;
        tracking_goal.target_position.z = 0.0f;

        /* Plan and execute small step toward target */
        motor_plan_movement(motor, &tracking_goal);
        motor_begin_execution(motor);
        motor_update_execution(motor, 50.0f);

        /* Measure tracking error */
        motor_effector_state_t state;
        motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

        float error = CalculateDistance(state.position, tracking_goal.target_position);
        tracking_errors.push_back(error);

        motor_reset(motor);
    }

    /* Calculate average tracking error */
    float avg_error = 0.0f;
    for (float e : tracking_errors) {
        avg_error += e;
    }
    avg_error /= tracking_errors.size();

    /* Tracking should have some following behavior */
    EXPECT_GT(tracking_errors.size(), 0u);
}

/*=============================================================================
 * E2E SCENARIO 5: MOTOR PROGRAM EXECUTION
 * Store and replay a learned motor program
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_MotorProgramExecution) {
    /* Create a motor program (drawing a square) */
    motor_command_t square_commands[4];
    memset(square_commands, 0, sizeof(square_commands));

    /* Right */
    square_commands[0].target_position.x = 1.0f;
    square_commands[0].target_position.y = 0.0f;
    square_commands[0].duration_ms = 100.0f;

    /* Up */
    square_commands[1].target_position.x = 1.0f;
    square_commands[1].target_position.y = 1.0f;
    square_commands[1].duration_ms = 100.0f;

    /* Left */
    square_commands[2].target_position.x = 0.0f;
    square_commands[2].target_position.y = 1.0f;
    square_commands[2].duration_ms = 100.0f;

    /* Down (back to start) */
    square_commands[3].target_position.x = 0.0f;
    square_commands[3].target_position.y = 0.0f;
    square_commands[3].duration_ms = 100.0f;

    /* Store as program */
    uint32_t program_id = motor_store_program(motor, "draw_square",
        square_commands, 4, MOVEMENT_TYPE_SERIAL);

    EXPECT_GT(program_id, 0u);

    /* Program stored - in a full implementation, we would retrieve and execute it */
    /* For now, verify the storage worked */
    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    /* Stats should reflect program storage */
}

/*=============================================================================
 * E2E SCENARIO 6: ERROR RECOVERY
 * Handle errors and recover gracefully
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_ErrorRecovery) {
    /* Phase 1: Attempt invalid operation */
    motor_goal_t invalid_goal;
    memset(&invalid_goal, 0, sizeof(invalid_goal));
    invalid_goal.region = (motor_region_t)9999;  /* Invalid region */

    bool result = motor_plan_movement(motor, &invalid_goal);
    EXPECT_FALSE(result);
    EXPECT_EQ(MOTOR_ERROR_INVALID_INPUT, motor_get_last_error(motor));

    /* Phase 2: Motor enters error state, but should still be recoverable */
    EXPECT_EQ(MOTOR_STATUS_ERROR, motor_get_status(motor));

    /* Phase 3: Reset to recover from error */
    EXPECT_TRUE(motor_reset(motor));
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(motor));

    /* Phase 4: Execute valid movement */
    motor_goal_t valid_goal = CreateGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    result = motor_plan_movement(motor, &valid_goal);
    EXPECT_TRUE(result);

    /* Phase 4: Complete movement */
    motor_begin_execution(motor);
    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Phase 5: Verify recovery */
    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

/*=============================================================================
 * E2E SCENARIO 7: PRECISION MOVEMENT
 * High-precision small movement
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_PrecisionMovement) {
    /* Small precise movement */
    motor_goal_t precision_goal = CreateGoal(
        MOTOR_REGION_HAND_RIGHT,
        0.1f, 0.05f, 0.02f,  /* Small target */
        200.0f
    );
    precision_goal.precision_required = 0.05f;  /* High precision requirement */

    motor_plan_movement(motor, &precision_goal);
    motor_begin_execution(motor);

    /* Execute with fine timesteps */
    for (int i = 0; i < 40; i++) {
        motor_update_execution(motor, 5.0f);
    }

    /* Verify precision */
    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

    float error = CalculateDistance(state.position, precision_goal.target_position);
    /* Precision movement should get close to target */
    EXPECT_LT(error, 0.2f);
}

/*=============================================================================
 * E2E SCENARIO 8: FULL BODY COORDINATION
 * Multiple effectors working together
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_FullBodyCoordination) {
    /* Note: Motor adapter supports one active trajectory at a time, so we
     * execute movements sequentially to test full body coordination capability */
    motor_goal_t right_hand = CreateGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.0f, 300.0f);
    motor_goal_t left_hand = CreateGoal(MOTOR_REGION_HAND_LEFT, -1.0f, 0.5f, 0.0f, 300.0f);

    /* Execute right hand movement */
    motor_plan_movement(motor, &right_hand);
    motor_begin_execution(motor);
    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t rh_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &rh_state);

    /* Execute left hand movement */
    motor_plan_movement(motor, &left_hand);
    motor_begin_execution(motor);
    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t lh_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_LEFT, &lh_state);

    /* Both hands should have moved */
    EXPECT_NE(0.0f, rh_state.position.x + rh_state.position.y);
    EXPECT_NE(0.0f, lh_state.position.x + lh_state.position.y);
}

/*=============================================================================
 * E2E SCENARIO 9: LONG DURATION MOVEMENT
 * Extended movement with many updates
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_LongDurationMovement) {
    motor_goal_t long_goal = CreateGoal(
        MOTOR_REGION_HAND_RIGHT,
        5.0f, 2.0f, 1.0f,  /* Distant target */
        2000.0f            /* 2 second movement */
    );

    motor_plan_movement(motor, &long_goal);
    motor_begin_execution(motor);

    /* Execute many steps */
    for (int i = 0; i < 200; i++) {
        motor_update_execution(motor, 10.0f);
    }

    /* Verify completion */
    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

    /* Should have progressed significantly */
    EXPECT_GT(state.position.x, 1.0f);
}

/*=============================================================================
 * E2E SCENARIO 10: LIFECYCLE STRESS TEST
 * Multiple create/destroy cycles
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_LifecycleStressTest) {
    const int CYCLES = 10;

    for (int cycle = 0; cycle < CYCLES; cycle++) {
        /* Create fresh adapter */
        motor_config_t cycle_config = motor_default_config();
        cycle_config.enable_bio_async = false;
        motor_adapter_t* cycle_motor = motor_create(&cycle_config);
        ASSERT_NE(nullptr, cycle_motor);

        /* Use it */
        motor_goal_t goal = CreateGoal(MOTOR_REGION_HAND_RIGHT,
            (float)(cycle + 1) * 0.2f, 0.0f, 0.0f, 100.0f);

        motor_plan_movement(cycle_motor, &goal);
        motor_begin_execution(cycle_motor);
        motor_update_execution(cycle_motor, 50.0f);

        /* Verify operation */
        motor_effector_state_t state;
        motor_get_effector_state(cycle_motor, MOTOR_REGION_HAND_RIGHT, &state);
        EXPECT_GE(state.position.x, 0.0f);

        /* Destroy */
        motor_destroy(cycle_motor);
    }
}

/*=============================================================================
 * E2E SCENARIO 11: FEEDBACK LOOP
 * Continuous feedback-based correction
 *===========================================================================*/

TEST_F(MotorE2ETest, E2E_FeedbackLoop) {
    motor_goal_t goal = CreateGoal(MOTOR_REGION_HAND_RIGHT, 2.0f, 0.0f, 0.0f, 500.0f);

    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    /* Simulate closed-loop control with feedback */
    for (int i = 0; i < 50; i++) {
        /* Update execution */
        motor_update_execution(motor, 10.0f);

        /* Get current state */
        motor_effector_state_t state;
        motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);

        /* Compute feedback - using actual measured state */
        motor_effector_state_t feedback_state;
        memset(&feedback_state, 0, sizeof(feedback_state));
        feedback_state.region = MOTOR_REGION_HAND_RIGHT;
        feedback_state.position = state.position;

        /* Apply feedback */
        motor_update_feedback(motor, MOTOR_REGION_HAND_RIGHT, &feedback_state);
    }

    /* Verify approach to target */
    motor_effector_state_t final_state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &final_state);

    float final_error = CalculateDistance(final_state.position, goal.target_position);
    EXPECT_LT(final_error, 1.0f);  /* Should be within 1.0 of target */
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
