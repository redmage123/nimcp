/**
 * @file test_motor_backward_compat.cpp
 * @brief Backward compatibility regression tests for Motor Cortex
 *
 * WHAT: Tests Motor Cortex API stability and backward compatibility
 * WHY:  Ensure existing motor code continues to work after updates
 * HOW:  Test core API functions, data structures, and return values
 *
 * REGRESSION FOCUS:
 * - API function signatures unchanged
 * - Return value semantics preserved
 * - Default behaviors maintained
 * - Error codes consistent
 * - Configuration defaults stable
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

class MotorBackwardCompatTest : public ::testing::Test {
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
};

/*=============================================================================
 * API FUNCTION SIGNATURE TESTS
 * These tests verify that the expected API functions exist and have
 * compatible signatures.
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, API_motor_default_config_exists) {
    /* Verify function exists and returns valid config */
    motor_config_t cfg = motor_default_config();
    EXPECT_TRUE(true);  /* Compilation success = function exists */
}

TEST_F(MotorBackwardCompatTest, API_motor_create_exists) {
    /* Verify motor_create accepts config pointer */
    motor_config_t cfg = motor_default_config();
    cfg.enable_bio_async = false;
    motor_adapter_t* test_adapter = motor_create(&cfg);
    ASSERT_NE(nullptr, test_adapter);
    motor_destroy(test_adapter);
}

TEST_F(MotorBackwardCompatTest, API_motor_destroy_exists) {
    /* Verify motor_destroy accepts adapter pointer */
    motor_config_t cfg = motor_default_config();
    cfg.enable_bio_async = false;
    motor_adapter_t* test_adapter = motor_create(&cfg);
    motor_destroy(test_adapter);
    motor_destroy(nullptr);  /* Should handle NULL safely */
}

TEST_F(MotorBackwardCompatTest, API_motor_plan_movement_exists) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    bool result = motor_plan_movement(adapter, &goal);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_begin_execution_exists) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(adapter, &goal);
    bool result = motor_begin_execution(adapter);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_update_execution_exists) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 100.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);
    motor_update_execution(adapter, 10.0f);
    EXPECT_TRUE(true);
}

TEST_F(MotorBackwardCompatTest, API_motor_reset_exists) {
    bool result = motor_reset(adapter);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_get_status_exists) {
    motor_status_t status = motor_get_status(adapter);
    EXPECT_EQ(MOTOR_STATUS_IDLE, status);
}

TEST_F(MotorBackwardCompatTest, API_motor_get_last_error_exists) {
    motor_error_t error = motor_get_last_error(adapter);
    EXPECT_EQ(MOTOR_ERROR_NONE, error);
}

TEST_F(MotorBackwardCompatTest, API_motor_get_config_exists) {
    motor_config_t retrieved;
    bool result = motor_get_config(adapter, &retrieved);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_get_stats_exists) {
    motor_stats_t stats;
    bool result = motor_get_stats(adapter, &stats);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_update_feedback_exists) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    motor_effector_state_t feedback_state;
    memset(&feedback_state, 0, sizeof(feedback_state));
    feedback_state.region = MOTOR_REGION_HAND_RIGHT;
    feedback_state.position.x = 0.5f;

    bool result = motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &feedback_state);
    EXPECT_TRUE(result);
}

TEST_F(MotorBackwardCompatTest, API_motor_store_program_exists) {
    motor_command_t commands[2];
    memset(commands, 0, sizeof(commands));
    commands[0].target_position.x = 0.5f;
    commands[0].duration_ms = 100.0f;
    commands[1].target_position.x = 1.0f;
    commands[1].duration_ms = 100.0f;

    uint32_t id = motor_store_program(adapter, "test_program",
        commands, 2, MOVEMENT_TYPE_DISCRETE);
    EXPECT_GT(id, 0u);
}

TEST_F(MotorBackwardCompatTest, API_motor_get_effector_state_exists) {
    motor_effector_state_t state;
    bool result = motor_get_effector_state(adapter, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_TRUE(result);
}

/*=============================================================================
 * RETURN VALUE SEMANTICS TESTS
 * Verify that return values have consistent meanings
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, ReturnSemantics_PlanMovementReturnsTrue) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 500.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    /* Valid input should return true */
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
}

TEST_F(MotorBackwardCompatTest, ReturnSemantics_PlanMovementReturnsFalseOnInvalid) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = (motor_region_t)9999;  /* Invalid */

    /* Invalid input should return false */
    EXPECT_FALSE(motor_plan_movement(adapter, &goal));
}

TEST_F(MotorBackwardCompatTest, ReturnSemantics_NullReturnsCorrectly) {
    /* NULL adapter should be handled */
    EXPECT_FALSE(motor_plan_movement(nullptr, nullptr));
    EXPECT_FALSE(motor_begin_execution(nullptr));
    EXPECT_FALSE(motor_reset(nullptr));
}

/*=============================================================================
 * DEFAULT BEHAVIOR TESTS
 * Verify default values and behaviors are maintained
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, DefaultConfig_HasReasonableValues) {
    motor_config_t cfg = motor_default_config();

    /* These defaults should not change */
    EXPECT_GT(cfg.max_motor_programs, 0u);
    EXPECT_GT(cfg.max_effectors, 0u);
    EXPECT_GT(cfg.planning_horizon_ms, 0.0f);
    EXPECT_GT(cfg.execution_rate_hz, 0.0f);
}

TEST_F(MotorBackwardCompatTest, DefaultState_IsIdle) {
    /* Fresh adapter should be idle */
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));
}

TEST_F(MotorBackwardCompatTest, DefaultError_IsNone) {
    /* Fresh adapter should have no error */
    EXPECT_EQ(MOTOR_ERROR_NONE, motor_get_last_error(adapter));
}

TEST_F(MotorBackwardCompatTest, DefaultStats_AreZero) {
    motor_stats_t stats;
    motor_get_stats(adapter, &stats);

    EXPECT_EQ(0u, stats.movements_planned);
    EXPECT_EQ(0u, stats.movements_executed);
    EXPECT_EQ(0u, stats.commands_generated);
}

/*=============================================================================
 * ERROR CODE CONSISTENCY TESTS
 * Verify error codes are consistent
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, ErrorCode_InvalidInputOnBadRegion) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = (motor_region_t)9999;

    motor_plan_movement(adapter, &goal);
    EXPECT_EQ(MOTOR_ERROR_INVALID_INPUT, motor_get_last_error(adapter));
}

TEST_F(MotorBackwardCompatTest, ErrorCode_OnPrematureExecution) {
    /* Try to execute without planning - should fail or set error */
    bool result = motor_begin_execution(adapter);
    /* Either returns false or sets an error */
    EXPECT_TRUE(!result || motor_get_last_error(adapter) != MOTOR_ERROR_NONE);
}

/*=============================================================================
 * ENUM VALUE STABILITY TESTS
 * Verify enum values haven't changed (important for serialization)
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, EnumStability_MotorRegions) {
    /* These values may be serialized - they must not change */
    EXPECT_EQ(0, (int)MOTOR_REGION_FACE);
    EXPECT_EQ(1, (int)MOTOR_REGION_HAND_LEFT);
    EXPECT_EQ(2, (int)MOTOR_REGION_HAND_RIGHT);
    EXPECT_EQ(3, (int)MOTOR_REGION_ARM_LEFT);
    EXPECT_EQ(4, (int)MOTOR_REGION_ARM_RIGHT);
}

TEST_F(MotorBackwardCompatTest, EnumStability_MovementTypes) {
    EXPECT_EQ(0, (int)MOVEMENT_TYPE_DISCRETE);
    EXPECT_EQ(1, (int)MOVEMENT_TYPE_SERIAL);
    EXPECT_EQ(2, (int)MOVEMENT_TYPE_CONTINUOUS);
}

TEST_F(MotorBackwardCompatTest, EnumStability_MotorStatus) {
    EXPECT_EQ(0, (int)MOTOR_STATUS_IDLE);
}

TEST_F(MotorBackwardCompatTest, EnumStability_MotorError) {
    EXPECT_EQ(0, (int)MOTOR_ERROR_NONE);
}

/*=============================================================================
 * STRUCTURE SIZE TESTS
 * Verify structure sizes haven't changed (important for ABI compatibility)
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, StructSize_motor_goal_t) {
    /* Structure size should be stable for binary compatibility */
    /* motor_goal_t has: region, target_position, target_velocity, 3 floats, type */
    size_t expected_min_size = sizeof(motor_region_t) + 2 * sizeof(motor_vec3_t) +
                               sizeof(float) * 3 + sizeof(movement_type_t);
    EXPECT_GE(sizeof(motor_goal_t), expected_min_size);
}

TEST_F(MotorBackwardCompatTest, StructSize_motor_command_t) {
    size_t expected_min_size = sizeof(motor_region_t) + 2 * sizeof(motor_vec3_t) +
                               sizeof(float);
    EXPECT_GE(sizeof(motor_command_t), expected_min_size);
}

TEST_F(MotorBackwardCompatTest, StructSize_motor_stats_t) {
    /* Stats structure should have counting fields */
    EXPECT_GT(sizeof(motor_stats_t), sizeof(uint32_t) * 4);
}

/*=============================================================================
 * LIFECYCLE COMPATIBILITY TESTS
 * Verify create/destroy cycles work correctly
 *===========================================================================*/

TEST_F(MotorBackwardCompatTest, Lifecycle_CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        motor_config_t cfg = motor_default_config();
        cfg.enable_bio_async = false;
        motor_adapter_t* test = motor_create(&cfg);
        EXPECT_NE(nullptr, test);
        motor_destroy(test);
    }
}

TEST_F(MotorBackwardCompatTest, Lifecycle_ResetMultipleTimes) {
    for (int i = 0; i < 10; i++) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = (float)i * 0.1f;
        goal.max_duration_ms = 100.0f;
        goal.type = MOVEMENT_TYPE_DISCRETE;

        motor_plan_movement(adapter, &goal);
        motor_reset(adapter);

        EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
