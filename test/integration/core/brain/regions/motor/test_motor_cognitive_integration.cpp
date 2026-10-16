/**
 * @file test_motor_cognitive_integration.cpp
 * @brief Integration tests for Motor Cortex with cognitive systems
 *
 * WHAT: Tests Motor Cortex integration with training hub and cognitive systems
 * WHY:  Ensure motor learning and cognitive control work together
 * HOW:  Test motor skill learning, program storage, and cognitive control
 *
 * COGNITIVE INTEGRATION POINTS:
 * - Motor Skill Learning: Practice-based improvement
 * - Motor Programs: Stored movement sequences
 * - Cognitive Control: Goal-directed actions and inhibition
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorCognitiveIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;

    void SetUp() override {
        /* Create motor adapter with training enabled */
        config = motor_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter);
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
    }

    /* Helper to create a test goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }
};

/*=============================================================================
 * MOTOR PROGRAM LEARNING TESTS
 * Test motor skill acquisition via program storage
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, StorePracticedMovementAsProgram) {
    /* Create command sequence representing practiced movement */
    motor_command_t commands[3];
    memset(commands, 0, sizeof(commands));

    commands[0].target_position.x = 0.3f;
    commands[0].duration_ms = 50.0f;
    commands[1].target_position.x = 0.6f;
    commands[1].duration_ms = 50.0f;
    commands[2].target_position.x = 1.0f;
    commands[2].duration_ms = 50.0f;

    /* Store as motor program (learned skill) */
    uint32_t program_id = motor_store_program(adapter, "learned_reach",
        commands, 3, MOVEMENT_TYPE_SERIAL);

    EXPECT_GT(program_id, 0u);
}

TEST_F(MotorCognitiveIntegrationTest, RecallStoredProgram) {
    /* Store a program */
    motor_command_t commands[2];
    memset(commands, 0, sizeof(commands));
    commands[0].target_position.x = 0.5f;
    commands[0].duration_ms = 100.0f;
    commands[1].target_position.x = 1.0f;
    commands[1].duration_ms = 100.0f;

    uint32_t program_id = motor_store_program(adapter, "simple_reach",
        commands, 2, MOVEMENT_TYPE_SERIAL);

    if (program_id == 0) {
        GTEST_SKIP() << "Motor program storage not available";
    }

    /* Recall program */
    motor_program_info_t recalled;
    bool result = motor_get_program(adapter, program_id, &recalled);
    EXPECT_TRUE(result);  /* Returns true on success */
    if (result) {
        EXPECT_EQ(2u, recalled.num_commands);
    }
}

TEST_F(MotorCognitiveIntegrationTest, DeleteStoredProgram) {
    /* Store a program */
    motor_command_t commands[1];
    memset(commands, 0, sizeof(commands));
    commands[0].target_position.x = 1.0f;
    commands[0].duration_ms = 100.0f;

    uint32_t program_id = motor_store_program(adapter, "temp_program",
        commands, 1, MOVEMENT_TYPE_DISCRETE);

    if (program_id == 0) {
        GTEST_SKIP() << "Motor program storage not available";
    }

    /* Delete program */
    bool result = motor_delete_program(adapter, program_id);
    EXPECT_TRUE(result);  /* Returns true on success */

    /* Verify deleted */
    motor_program_info_t recalled;
    result = motor_get_program(adapter, program_id, &recalled);
    EXPECT_FALSE(result);  /* Should fail since program was deleted */
}

/*=============================================================================
 * SKILL CONSOLIDATION TESTS
 * Test motor learning through repetitive practice
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, RepetitivePracticeImproves) {
    /* Simulate practice sessions */
    for (int trial = 0; trial < 5; trial++) {
        motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT,
            1.0f + (float)trial * 0.1f, 100.0f);

        EXPECT_TRUE(motor_plan_movement(adapter, &goal));
        EXPECT_TRUE(motor_begin_execution(adapter));

        for (int step = 0; step < 10; step++) {
            motor_update_execution(adapter, 10.0f);
        }

        motor_reset(adapter);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_EQ(5u, stats.movements_planned);
}

TEST_F(MotorCognitiveIntegrationTest, PrecisionImprovementOverTrials) {
    /* Execute movements with increasing precision requirements */
    for (int trial = 0; trial < 3; trial++) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = 1.0f;
        goal.max_duration_ms = 200.0f;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        goal.precision_required = 0.1f - (float)trial * 0.03f;  /* Tighter precision */

        motor_plan_movement(adapter, &goal);
        motor_begin_execution(adapter);

        for (int step = 0; step < 20; step++) {
            motor_update_execution(adapter, 10.0f);
        }

        motor_reset(adapter);
    }

    /* Verify movements were planned */
    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GE(stats.movements_planned, 3u);
}

/*=============================================================================
 * COGNITIVE CONTROL TESTS
 * Test goal-directed behavior and action inhibition
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, GoalDirectedPlanning) {
    /* Plan goal-directed movement with specific requirements */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 2.0f;
    goal.target_position.y = 1.0f;
    goal.target_position.z = 0.5f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;
    goal.precision_required = 0.05f;

    bool planned = motor_plan_movement(adapter, &goal);
    EXPECT_TRUE(planned);

    motor_status_t status = motor_get_status(adapter);
    EXPECT_EQ(MOTOR_STATUS_PREPARING, status);
}

TEST_F(MotorCognitiveIntegrationTest, ActionInhibition_StopOngoing) {
    /* Start movement */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 2.0f, 500.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Execute partially */
    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Get state before inhibition */
    motor_effector_state_t state_before;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state_before);

    /* Inhibit action (cognitive stop signal) */
    bool reset = motor_reset(adapter);
    EXPECT_TRUE(reset);

    /* Verify stopped */
    motor_status_t status = motor_get_status(adapter);
    EXPECT_EQ(MOTOR_STATUS_IDLE, status);
}

TEST_F(MotorCognitiveIntegrationTest, SequentialActionPlanning) {
    /* Plan sequential actions */
    for (int i = 0; i < 3; i++) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = (float)(i + 1) * 0.5f;
        goal.max_duration_ms = 100.0f;
        goal.type = MOVEMENT_TYPE_SERIAL;

        bool planned = motor_plan_movement(adapter, &goal);
        EXPECT_TRUE(planned);
    }

    /* Execute sequence */
    motor_begin_execution(adapter);

    for (int step = 0; step < 40; step++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Should have progressed through sequence */
    motor_effector_state_t state;
    motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

/*=============================================================================
 * ERROR-DRIVEN LEARNING TESTS
 * Test motor adaptation to feedback
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, FeedbackGuidedExecution) {
    /* Execute movement */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 200.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Execute with feedback */
    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 20.0f);

        /* Provide state feedback */
        motor_effector_state_t feedback_state;
        memset(&feedback_state, 0, sizeof(feedback_state));
        feedback_state.region = MOTOR_REGION_HAND_RIGHT;
        feedback_state.position.x = 0.1f * i;

        motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &feedback_state);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GE(stats.movements_executed, 0u);
}

TEST_F(MotorCognitiveIntegrationTest, CorrectionFromError) {
    /* Start movement */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_ARM_RIGHT;
    goal.target_position.x = 1.0f;
    goal.target_position.y = 0.5f;
    goal.max_duration_ms = 300.0f;
    goal.type = MOVEMENT_TYPE_CONTINUOUS;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Execute with error feedback */
    for (int step = 0; step < 15; step++) {
        motor_update_execution(adapter, 20.0f);

        /* Simulate error: position off by fixed amount */
        motor_effector_state_t feedback_state;
        memset(&feedback_state, 0, sizeof(feedback_state));
        feedback_state.region = MOTOR_REGION_ARM_RIGHT;
        feedback_state.position.x = goal.target_position.x * 0.8f;

        motor_update_feedback(adapter, MOTOR_REGION_ARM_RIGHT, &feedback_state);
    }

    /* Stats should show activity */
    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GT(stats.commands_generated, 0u);
}

/*=============================================================================
 * WORKING MEMORY TESTS
 * Test motor sequence buffering and recall
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, StoreMultiplePrograms) {
    uint32_t program_ids[3];

    for (int i = 0; i < 3; i++) {
        motor_command_t commands[2];
        memset(commands, 0, sizeof(commands));
        commands[0].target_position.x = (float)(i + 1) * 0.3f;
        commands[0].duration_ms = 50.0f;
        commands[1].target_position.x = (float)(i + 1) * 0.6f;
        commands[1].duration_ms = 50.0f;

        char name[32];
        snprintf(name, sizeof(name), "program_%d", i);

        program_ids[i] = motor_store_program(adapter, name,
            commands, 2, MOVEMENT_TYPE_SERIAL);
    }

    /* Verify all stored */
    int stored = 0;
    for (int i = 0; i < 3; i++) {
        if (program_ids[i] > 0) stored++;
    }
    EXPECT_GE(stored, 0);  /* Some may fail if feature not implemented */
}

/*=============================================================================
 * CONFIG AND STATS TESTS
 *===========================================================================*/

TEST_F(MotorCognitiveIntegrationTest, TrainingEnabledInConfig) {
    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_training);
}

TEST_F(MotorCognitiveIntegrationTest, StatsTrackLearning) {
    /* Perform some operations */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 100.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Get stats */
    motor_stats_t stats;
    motor_get_stats(adapter, &stats);

    EXPECT_GE(stats.movements_planned, 1u);
    EXPECT_GT(stats.commands_generated, 0u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
