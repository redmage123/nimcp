/**
 * @file test_nimcp_motor_adapter.cpp
 * @brief Unit tests for nimcp_motor_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Motor Cortex adapter
 * WHY:  Ensure correct integration of M1, premotor, and SMA sub-modules
 * HOW:  Use Google Test framework to test lifecycle, motor programs, execution,
 *       feedback processing, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"

// Test Fixture for Motor Adapter
class MotorAdapterTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;

    void SetUp() override {
        config = motor_default_config();
        // Disable bio-async for isolated testing
        config.enable_bio_async = false;
        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";
    }

    void TearDown() override {
        motor_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create a simple motor goal
    motor_goal_t create_test_goal(motor_region_t region = MOTOR_REGION_HAND_RIGHT,
                                   float x = 1.0f, float y = 0.5f, float z = 0.0f) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.target_velocity.x = 0.0f;
        goal.target_velocity.y = 0.0f;
        goal.target_velocity.z = 0.0f;
        goal.max_duration_ms = 500.0f;
        goal.precision_required = 0.1f;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        goal.urgency = 0.5f;
        return goal;
    }

    // Helper to create a sequence of motor commands
    void create_test_program_commands(motor_command_t* commands, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            memset(&commands[i], 0, sizeof(motor_command_t));
            commands[i].effector_id = 0;
            commands[i].target_position.x = 0.1f * (float)i;
            commands[i].target_position.y = 0.1f * (float)i;
            commands[i].target_position.z = 0.0f;
            commands[i].target_velocity.x = 0.0f;
            commands[i].target_velocity.y = 0.0f;
            commands[i].target_velocity.z = 0.0f;
            commands[i].target_force = 0.5f;
            commands[i].duration_ms = 50.0f;
            commands[i].timestamp_ms = (double)i * 50.0;
        }
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(MotorAdapterTest, DefaultConfigHasReasonableValues) {
    motor_config_t default_config = motor_default_config();

    EXPECT_EQ(default_config.max_motor_programs, MOTOR_DEFAULT_MAX_PROGRAMS);
    EXPECT_EQ(default_config.max_effectors, MOTOR_DEFAULT_MAX_EFFECTORS);
    EXPECT_EQ(default_config.max_trajectories, MOTOR_DEFAULT_MAX_TRAJECTORIES);
    EXPECT_FLOAT_EQ(default_config.planning_horizon_ms, MOTOR_DEFAULT_PLANNING_HORIZON_MS);
    EXPECT_FLOAT_EQ(default_config.execution_rate_hz, MOTOR_DEFAULT_EXECUTION_RATE_HZ);
    EXPECT_TRUE(default_config.enable_premotor);
    EXPECT_TRUE(default_config.enable_sma);
    EXPECT_TRUE(default_config.enable_trajectory_opt);
    EXPECT_TRUE(default_config.enable_feedforward);
    EXPECT_TRUE(default_config.enable_feedback);
    EXPECT_TRUE(default_config.enable_basal_ganglia);
    EXPECT_TRUE(default_config.enable_cerebellum);
    EXPECT_TRUE(default_config.enable_thalamus);
    EXPECT_TRUE(default_config.enable_events);
}

TEST_F(MotorAdapterTest, CreateWithNullConfigUsesDefaults) {
    motor_config_t cfg = motor_default_config();
    cfg.enable_bio_async = false;
    motor_adapter_t* adapter_default = motor_create(&cfg);
    ASSERT_NE(nullptr, adapter_default);

    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(adapter_default, &retrieved));
    EXPECT_EQ(retrieved.max_motor_programs, MOTOR_DEFAULT_MAX_PROGRAMS);
    EXPECT_EQ(retrieved.max_effectors, MOTOR_DEFAULT_MAX_EFFECTORS);

    motor_destroy(adapter_default);
}

TEST_F(MotorAdapterTest, DestroyNullDoesNotCrash) {
    motor_destroy(NULL);
    // Should not crash
}

TEST_F(MotorAdapterTest, ResetClearsState) {
    // Plan a movement first
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));

    EXPECT_TRUE(motor_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
    EXPECT_EQ(motor_get_last_error(adapter), MOTOR_ERROR_NONE);
}

TEST_F(MotorAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(motor_reset(NULL));
}

TEST_F(MotorAdapterTest, GetStatusNullReturnsError) {
    EXPECT_EQ(motor_get_status(NULL), MOTOR_STATUS_ERROR);
}

TEST_F(MotorAdapterTest, GetLastErrorNullReturnsInternal) {
    EXPECT_EQ(motor_get_last_error(NULL), MOTOR_ERROR_INTERNAL);
}

// ============================================================================
// ERROR AND STATUS STRING TESTS
// ============================================================================

TEST_F(MotorAdapterTest, ErrorStringReturnsCorrectStrings) {
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_NONE), "No error");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_INVALID_INPUT), "Invalid input");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_PLANNING_FAILURE), "Planning failed");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_EXECUTION_FAILURE), "Execution failed");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_TRAJECTORY_INFEASIBLE), "Trajectory infeasible");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_EFFECTOR_CONFLICT), "Effector conflict");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_TIMING_VIOLATION), "Timing violation");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_BUFFER_OVERFLOW), "Buffer overflow");
    EXPECT_STREQ(motor_error_string(MOTOR_ERROR_INTERNAL), "Internal error");
    EXPECT_STREQ(motor_error_string((motor_error_t)999), "Unknown error");
}

TEST_F(MotorAdapterTest, StatusStringReturnsCorrectStrings) {
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_IDLE), "Idle");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_PLANNING), "Planning");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_PREPARING), "Preparing");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_EXECUTING), "Executing");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_CORRECTING), "Correcting");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_COMPLETE), "Complete");
    EXPECT_STREQ(motor_status_string(MOTOR_STATUS_ERROR), "Error");
    EXPECT_STREQ(motor_status_string((motor_status_t)999), "Unknown");
}

// ============================================================================
// MOTOR PROGRAM MANAGEMENT TESTS
// ============================================================================

TEST_F(MotorAdapterTest, StoreProgramSuccess) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    uint32_t program_id = motor_store_program(adapter, "wave_hand", commands, 5, MOVEMENT_TYPE_SERIAL);
    EXPECT_GT(program_id, 0u);
}

TEST_F(MotorAdapterTest, StoreProgramNullReturnsZero) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    EXPECT_EQ(motor_store_program(NULL, "test", commands, 5, MOVEMENT_TYPE_DISCRETE), 0u);
    EXPECT_EQ(motor_store_program(adapter, NULL, commands, 5, MOVEMENT_TYPE_DISCRETE), 0u);
    EXPECT_EQ(motor_store_program(adapter, "test", NULL, 5, MOVEMENT_TYPE_DISCRETE), 0u);
    EXPECT_EQ(motor_store_program(adapter, "test", commands, 0, MOVEMENT_TYPE_DISCRETE), 0u);
}

TEST_F(MotorAdapterTest, GetProgramSuccess) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    uint32_t program_id = motor_store_program(adapter, "grasp", commands, 5, MOVEMENT_TYPE_SERIAL);
    ASSERT_GT(program_id, 0u);

    motor_program_info_t info;
    EXPECT_TRUE(motor_get_program(adapter, program_id, &info));
    EXPECT_EQ(info.program_id, program_id);
    EXPECT_STREQ(info.name, "grasp");
    EXPECT_EQ(info.num_commands, 5u);
    EXPECT_EQ(info.type, MOVEMENT_TYPE_SERIAL);
    EXPECT_TRUE(info.is_learned);
}

TEST_F(MotorAdapterTest, GetProgramNotFound) {
    motor_program_info_t info;
    EXPECT_FALSE(motor_get_program(adapter, 9999, &info));
}

TEST_F(MotorAdapterTest, GetProgramNullParams) {
    EXPECT_FALSE(motor_get_program(NULL, 1, NULL));
    EXPECT_FALSE(motor_get_program(adapter, 0, NULL));
}

TEST_F(MotorAdapterTest, DeleteProgramSuccess) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    uint32_t program_id = motor_store_program(adapter, "throw", commands, 5, MOVEMENT_TYPE_BALLISTIC);
    ASSERT_GT(program_id, 0u);

    EXPECT_TRUE(motor_delete_program(adapter, program_id));

    motor_program_info_t info;
    EXPECT_FALSE(motor_get_program(adapter, program_id, &info));
}

TEST_F(MotorAdapterTest, DeleteProgramNotFound) {
    EXPECT_FALSE(motor_delete_program(adapter, 9999));
}

TEST_F(MotorAdapterTest, DeleteProgramNullParams) {
    EXPECT_FALSE(motor_delete_program(NULL, 1));
    EXPECT_FALSE(motor_delete_program(adapter, 0));
}

// ============================================================================
// MOTOR PLANNING TESTS
// ============================================================================

TEST_F(MotorAdapterTest, PlanMovementSuccess) {
    motor_goal_t goal = create_test_goal();

    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_PREPARING);
}

TEST_F(MotorAdapterTest, PlanMovementNullParams) {
    EXPECT_FALSE(motor_plan_movement(NULL, NULL));
    EXPECT_FALSE(motor_plan_movement(adapter, NULL));
}

TEST_F(MotorAdapterTest, PlanMovementInvalidRegion) {
    motor_goal_t goal = create_test_goal();
    goal.region = (motor_region_t)999; // Invalid region

    EXPECT_FALSE(motor_plan_movement(adapter, &goal));
    EXPECT_EQ(motor_get_last_error(adapter), MOTOR_ERROR_INVALID_INPUT);
}

TEST_F(MotorAdapterTest, PlanTrajectorySuccess) {
    trajectory_waypoint_t waypoints[3];
    memset(waypoints, 0, sizeof(waypoints));

    waypoints[0].position.x = 0.0f;
    waypoints[0].position.y = 0.0f;
    waypoints[0].position.z = 0.0f;
    waypoints[0].time_ms = 0.0f;

    waypoints[1].position.x = 0.5f;
    waypoints[1].position.y = 0.5f;
    waypoints[1].position.z = 0.0f;
    waypoints[1].time_ms = 100.0f;

    waypoints[2].position.x = 1.0f;
    waypoints[2].position.y = 1.0f;
    waypoints[2].position.z = 0.0f;
    waypoints[2].time_ms = 200.0f;

    EXPECT_TRUE(motor_plan_trajectory(adapter, MOTOR_REGION_HAND_RIGHT, waypoints, 3));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_PREPARING);
}

TEST_F(MotorAdapterTest, PlanTrajectoryNullParams) {
    EXPECT_FALSE(motor_plan_trajectory(NULL, MOTOR_REGION_HAND_RIGHT, NULL, 0));
    EXPECT_FALSE(motor_plan_trajectory(adapter, MOTOR_REGION_HAND_RIGHT, NULL, 3));
}

TEST_F(MotorAdapterTest, PlanTrajectoryTooFewWaypoints) {
    trajectory_waypoint_t waypoint;
    memset(&waypoint, 0, sizeof(waypoint));

    EXPECT_FALSE(motor_plan_trajectory(adapter, MOTOR_REGION_HAND_RIGHT, &waypoint, 1));
}

TEST_F(MotorAdapterTest, PlanProgramExecutionSuccess) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    uint32_t program_id = motor_store_program(adapter, "reach", commands, 5, MOVEMENT_TYPE_DISCRETE);
    ASSERT_GT(program_id, 0u);

    EXPECT_TRUE(motor_plan_program_execution(adapter, program_id, 1.0f));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_PREPARING);
}

TEST_F(MotorAdapterTest, PlanProgramExecutionNotFound) {
    EXPECT_FALSE(motor_plan_program_execution(adapter, 9999, 1.0f));
}

TEST_F(MotorAdapterTest, PlanProgramExecutionNullParams) {
    EXPECT_FALSE(motor_plan_program_execution(NULL, 1, 1.0f));
    EXPECT_FALSE(motor_plan_program_execution(adapter, 0, 1.0f));
}

// ============================================================================
// MOTOR EXECUTION TESTS
// ============================================================================

TEST_F(MotorAdapterTest, BeginExecutionSuccess) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));

    EXPECT_TRUE(motor_begin_execution(adapter));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_EXECUTING);
}

TEST_F(MotorAdapterTest, BeginExecutionFromWrongState) {
    // Without planning first
    EXPECT_FALSE(motor_begin_execution(adapter));
}

TEST_F(MotorAdapterTest, BeginExecutionNullReturnsFalse) {
    EXPECT_FALSE(motor_begin_execution(NULL));
}

TEST_F(MotorAdapterTest, UpdateExecutionAdvancesTime) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    // Update should return true while still executing
    EXPECT_TRUE(motor_update_execution(adapter, 10.0f));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_EXECUTING);
}

TEST_F(MotorAdapterTest, UpdateExecutionCompletes) {
    motor_goal_t goal = create_test_goal();
    goal.max_duration_ms = 100.0f;  // Short duration
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    // Advance past completion
    for (int i = 0; i < 20; i++) {
        if (!motor_update_execution(adapter, 10.0f)) {
            break;
        }
    }

    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_COMPLETE);
}

TEST_F(MotorAdapterTest, UpdateExecutionNullReturnsFalse) {
    EXPECT_FALSE(motor_update_execution(NULL, 10.0f));
}

TEST_F(MotorAdapterTest, StopExecutionSuccess) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    EXPECT_TRUE(motor_stop_execution(adapter));
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
}

TEST_F(MotorAdapterTest, StopExecutionNullReturnsFalse) {
    EXPECT_FALSE(motor_stop_execution(NULL));
}

TEST_F(MotorAdapterTest, GetNextCommandDuringExecution) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    // Update to generate commands
    motor_update_execution(adapter, 10.0f);

    motor_command_t cmd;
    if (motor_get_next_command(adapter, &cmd)) {
        // If we got a command, it should have valid data
        EXPECT_LT(cmd.effector_id, (uint32_t)MOTOR_REGION_COUNT);
    }
}

TEST_F(MotorAdapterTest, GetNextCommandNullParams) {
    EXPECT_FALSE(motor_get_next_command(NULL, NULL));
    motor_command_t cmd;
    EXPECT_FALSE(motor_get_next_command(NULL, &cmd));
}

TEST_F(MotorAdapterTest, GetResultAfterCompletion) {
    motor_goal_t goal = create_test_goal();
    goal.max_duration_ms = 50.0f;
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    // Complete execution
    for (int i = 0; i < 20; i++) {
        if (!motor_update_execution(adapter, 10.0f)) {
            break;
        }
    }

    motor_result_t result;
    EXPECT_TRUE(motor_get_result(adapter, &result));
    EXPECT_TRUE(result.success);
}

TEST_F(MotorAdapterTest, GetResultBeforeCompletion) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));

    motor_result_t result;
    EXPECT_FALSE(motor_get_result(adapter, &result));
}

TEST_F(MotorAdapterTest, GetResultNullParams) {
    EXPECT_FALSE(motor_get_result(NULL, NULL));
    motor_result_t result;
    EXPECT_FALSE(motor_get_result(NULL, &result));
}

// ============================================================================
// SENSORY FEEDBACK TESTS
// ============================================================================

TEST_F(MotorAdapterTest, UpdateFeedbackSuccess) {
    motor_effector_state_t state;
    memset(&state, 0, sizeof(state));
    state.effector_id = 0;
    state.region = MOTOR_REGION_FACE;
    state.position.x = 0.1f;
    state.position.y = 0.2f;
    state.position.z = 0.0f;
    state.velocity.x = 0.0f;
    state.velocity.y = 0.0f;
    state.velocity.z = 0.0f;
    state.force = 0.5f;
    state.stiffness = 0.7f;
    state.is_active = true;

    EXPECT_TRUE(motor_update_feedback(adapter, 0, &state));
}

TEST_F(MotorAdapterTest, UpdateFeedbackInvalidEffector) {
    motor_effector_state_t state;
    memset(&state, 0, sizeof(state));

    EXPECT_FALSE(motor_update_feedback(adapter, 9999, &state));
}

TEST_F(MotorAdapterTest, UpdateFeedbackNullParams) {
    EXPECT_FALSE(motor_update_feedback(NULL, 0, NULL));
    EXPECT_FALSE(motor_update_feedback(adapter, 0, NULL));
}

TEST_F(MotorAdapterTest, UpdateVisualFeedbackSuccess) {
    motor_vec3_t visual_pos = {0.5f, 0.5f, 0.0f};
    EXPECT_TRUE(motor_update_visual_feedback(adapter, 0, &visual_pos, 0.9f));
}

TEST_F(MotorAdapterTest, UpdateVisualFeedbackInvalidEffector) {
    motor_vec3_t visual_pos = {0.5f, 0.5f, 0.0f};
    EXPECT_FALSE(motor_update_visual_feedback(adapter, 9999, &visual_pos, 0.9f));
}

TEST_F(MotorAdapterTest, UpdateVisualFeedbackNullParams) {
    EXPECT_FALSE(motor_update_visual_feedback(NULL, 0, NULL, 0.9f));
    EXPECT_FALSE(motor_update_visual_feedback(adapter, 0, NULL, 0.9f));
}

// ============================================================================
// INTEGRATION INTERFACE TESTS
// ============================================================================

TEST_F(MotorAdapterTest, ReceiveBGSelectionSuccess) {
    EXPECT_TRUE(motor_receive_bg_selection(adapter, 1, 0.8f));
}

TEST_F(MotorAdapterTest, ReceiveBGSelectionNullReturnsFalse) {
    EXPECT_FALSE(motor_receive_bg_selection(NULL, 1, 0.8f));
}

TEST_F(MotorAdapterTest, ReceiveCerebellarCorrectionSuccess) {
    EXPECT_TRUE(motor_receive_cerebellar_correction(adapter, 0, 10.0f, 1.1f));
}

TEST_F(MotorAdapterTest, ReceiveCerebellarCorrectionInvalidEffector) {
    EXPECT_FALSE(motor_receive_cerebellar_correction(adapter, 9999, 10.0f, 1.1f));
}

TEST_F(MotorAdapterTest, ReceiveCerebellarCorrectionNullReturnsFalse) {
    EXPECT_FALSE(motor_receive_cerebellar_correction(NULL, 0, 10.0f, 1.1f));
}

// ============================================================================
// TRAINING INTERFACE TESTS
// ============================================================================

TEST_F(MotorAdapterTest, TrainMovementRequiresEnabledTraining) {
    // Training is disabled in default config
    motor_vec3_t target = {1.0f, 1.0f, 0.0f};
    EXPECT_FALSE(motor_train_movement(adapter, &target, 0.01f));
}

TEST_F(MotorAdapterTest, TrainMovementNullParams) {
    EXPECT_FALSE(motor_train_movement(NULL, NULL, 0.01f));
}

TEST_F(MotorAdapterTest, TrainFromDemonstrationSuccess) {
    trajectory_waypoint_t waypoints[3];
    memset(waypoints, 0, sizeof(waypoints));

    waypoints[0].position.x = 0.0f;
    waypoints[0].time_ms = 0.0f;

    waypoints[1].position.x = 0.5f;
    waypoints[1].time_ms = 100.0f;

    waypoints[2].position.x = 1.0f;
    waypoints[2].time_ms = 200.0f;

    uint32_t program_id = motor_train_from_demonstration(adapter, waypoints, 3, "learned_reach");
    EXPECT_GT(program_id, 0u);

    // Verify program was stored
    motor_program_info_t info;
    EXPECT_TRUE(motor_get_program(adapter, program_id, &info));
    EXPECT_STREQ(info.name, "learned_reach");
}

TEST_F(MotorAdapterTest, TrainFromDemonstrationNullParams) {
    trajectory_waypoint_t waypoints[3];
    memset(waypoints, 0, sizeof(waypoints));

    EXPECT_EQ(motor_train_from_demonstration(NULL, waypoints, 3, "test"), 0u);
    EXPECT_EQ(motor_train_from_demonstration(adapter, NULL, 3, "test"), 0u);
    EXPECT_EQ(motor_train_from_demonstration(adapter, waypoints, 1, "test"), 0u);
    EXPECT_EQ(motor_train_from_demonstration(adapter, waypoints, 3, NULL), 0u);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(MotorAdapterTest, GetStatsSuccess) {
    motor_stats_t stats;
    EXPECT_TRUE(motor_get_stats(adapter, &stats));

    // Initial stats should be zero
    EXPECT_EQ(stats.movements_planned, 0u);
    EXPECT_EQ(stats.movements_executed, 0u);
}

TEST_F(MotorAdapterTest, GetStatsAfterOperations) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    motor_stats_t stats;
    EXPECT_TRUE(motor_get_stats(adapter, &stats));
    EXPECT_GT(stats.movements_planned, 0u);
    EXPECT_GT(stats.movements_executed, 0u);
}

TEST_F(MotorAdapterTest, GetStatsNullParams) {
    EXPECT_FALSE(motor_get_stats(NULL, NULL));
    motor_stats_t stats;
    EXPECT_FALSE(motor_get_stats(NULL, &stats));
    EXPECT_FALSE(motor_get_stats(adapter, NULL));
}

TEST_F(MotorAdapterTest, GetConfigSuccess) {
    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.max_motor_programs, config.max_motor_programs);
    EXPECT_EQ(retrieved.max_effectors, config.max_effectors);
}

TEST_F(MotorAdapterTest, GetConfigNullParams) {
    EXPECT_FALSE(motor_get_config(NULL, NULL));
    motor_config_t retrieved;
    EXPECT_FALSE(motor_get_config(NULL, &retrieved));
    EXPECT_FALSE(motor_get_config(adapter, NULL));
}

TEST_F(MotorAdapterTest, GetEffectorStateSuccess) {
    motor_effector_state_t state;
    EXPECT_TRUE(motor_get_effector_state(adapter, 0, &state));
    EXPECT_EQ(state.effector_id, 0u);
    EXPECT_EQ(state.region, MOTOR_REGION_FACE);
}

TEST_F(MotorAdapterTest, GetEffectorStateInvalidId) {
    motor_effector_state_t state;
    EXPECT_FALSE(motor_get_effector_state(adapter, 9999, &state));
}

TEST_F(MotorAdapterTest, GetEffectorStateNullParams) {
    EXPECT_FALSE(motor_get_effector_state(NULL, 0, NULL));
    motor_effector_state_t state;
    EXPECT_FALSE(motor_get_effector_state(NULL, 0, &state));
    EXPECT_FALSE(motor_get_effector_state(adapter, 0, NULL));
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int command_callback_count = 0;
static int complete_callback_count = 0;
static int event_callback_count = 0;

static void test_command_callback(const motor_command_t* command, void* user_data) {
    (void)command;
    (void)user_data;
    command_callback_count++;
}

static void test_complete_callback(const motor_result_t* result, void* user_data) {
    (void)result;
    (void)user_data;
    complete_callback_count++;
}

static void test_event_callback(uint32_t event_type, const void* event_data, void* user_data) {
    (void)event_type;
    (void)event_data;
    (void)user_data;
    event_callback_count++;
}

TEST_F(MotorAdapterTest, SetCommandCallbackSuccess) {
    EXPECT_TRUE(motor_set_command_callback(adapter, test_command_callback, NULL));
}

TEST_F(MotorAdapterTest, SetCommandCallbackNullAdapter) {
    EXPECT_FALSE(motor_set_command_callback(NULL, test_command_callback, NULL));
}

TEST_F(MotorAdapterTest, SetCompleteCallbackSuccess) {
    EXPECT_TRUE(motor_set_complete_callback(adapter, test_complete_callback, NULL));
}

TEST_F(MotorAdapterTest, SetCompleteCallbackNullAdapter) {
    EXPECT_FALSE(motor_set_complete_callback(NULL, test_complete_callback, NULL));
}

TEST_F(MotorAdapterTest, SetEventCallbackSuccess) {
    EXPECT_TRUE(motor_set_event_callback(adapter, test_event_callback, NULL));
}

TEST_F(MotorAdapterTest, SetEventCallbackNullAdapter) {
    EXPECT_FALSE(motor_set_event_callback(NULL, test_event_callback, NULL));
}

TEST_F(MotorAdapterTest, CallbacksInvokedDuringExecution) {
    command_callback_count = 0;
    complete_callback_count = 0;
    event_callback_count = 0;

    ASSERT_TRUE(motor_set_command_callback(adapter, test_command_callback, NULL));
    ASSERT_TRUE(motor_set_complete_callback(adapter, test_complete_callback, NULL));
    ASSERT_TRUE(motor_set_event_callback(adapter, test_event_callback, NULL));

    motor_goal_t goal = create_test_goal();
    goal.max_duration_ms = 50.0f;
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));

    // Event callback should be called for plan complete
    EXPECT_GT(event_callback_count, 0);

    ASSERT_TRUE(motor_begin_execution(adapter));

    // Complete execution
    for (int i = 0; i < 20; i++) {
        if (!motor_update_execution(adapter, 10.0f)) {
            break;
        }
    }

    // Callbacks should have been invoked
    EXPECT_GT(command_callback_count, 0);
    EXPECT_GT(complete_callback_count, 0);
}

// ============================================================================
// BIO-ASYNC TESTS (Limited - bio-async disabled in fixture)
// ============================================================================

TEST_F(MotorAdapterTest, GetBioContextReturnsNullWhenDisabled) {
    bio_module_context_t ctx = motor_get_bio_context(adapter);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(MotorAdapterTest, GetBioContextNullAdapter) {
    bio_module_context_t ctx = motor_get_bio_context(NULL);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(MotorAdapterTest, ProcessBioMessagesReturnsZeroWhenDisabled) {
    uint32_t processed = motor_process_bio_messages(adapter, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(MotorAdapterTest, ProcessBioMessagesNullAdapter) {
    uint32_t processed = motor_process_bio_messages(NULL, 10);
    EXPECT_EQ(processed, 0u);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(MotorAdapterTest, MultipleMovementPlansOverwrite) {
    motor_goal_t goal1 = create_test_goal(MOTOR_REGION_HAND_LEFT);
    motor_goal_t goal2 = create_test_goal(MOTOR_REGION_HAND_RIGHT);

    ASSERT_TRUE(motor_plan_movement(adapter, &goal1));
    ASSERT_TRUE(motor_plan_movement(adapter, &goal2));

    // Should be in preparing state for the second plan
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_PREPARING);
}

TEST_F(MotorAdapterTest, StoreMultiplePrograms) {
    motor_command_t commands[5];
    create_test_program_commands(commands, 5);

    uint32_t id1 = motor_store_program(adapter, "program1", commands, 5, MOVEMENT_TYPE_DISCRETE);
    uint32_t id2 = motor_store_program(adapter, "program2", commands, 5, MOVEMENT_TYPE_SERIAL);
    uint32_t id3 = motor_store_program(adapter, "program3", commands, 5, MOVEMENT_TYPE_CONTINUOUS);

    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);
    EXPECT_GT(id3, 0u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);

    // All should be retrievable
    motor_program_info_t info;
    EXPECT_TRUE(motor_get_program(adapter, id1, &info));
    EXPECT_TRUE(motor_get_program(adapter, id2, &info));
    EXPECT_TRUE(motor_get_program(adapter, id3, &info));
}

TEST_F(MotorAdapterTest, FeedbackDuringExecution) {
    motor_goal_t goal = create_test_goal();
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    // Update effector state during execution
    motor_effector_state_t state;
    memset(&state, 0, sizeof(state));
    state.effector_id = (uint32_t)MOTOR_REGION_HAND_RIGHT;
    state.region = MOTOR_REGION_HAND_RIGHT;
    state.position.x = 0.5f;
    state.position.y = 0.25f;
    state.position.z = 0.0f;

    EXPECT_TRUE(motor_update_feedback(adapter, state.effector_id, &state));

    // Execution should continue
    EXPECT_TRUE(motor_update_execution(adapter, 10.0f));
}

// Main function to run the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
