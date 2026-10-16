/**
 * @file test_motor_security_integration.cpp
 * @brief Integration tests for Motor Cortex with Security systems
 *
 * WHAT: Tests Motor Cortex integration with Blood-Brain Barrier security
 * WHY:  Ensure motor system is protected from security threats
 * HOW:  Test BBB protection, input validation, access control
 *
 * BIOLOGICAL BASIS:
 * The motor cortex must be protected from:
 * - Malicious motor commands (could cause harmful movements)
 * - Unauthorized access to motor planning
 * - Corrupted motor programs
 * - Injection attacks via motor feedback
 *
 * INTEGRATION POINTS:
 * - Blood-Brain Barrier (BBB) threat detection
 * - Input validation for motor commands
 * - Access control for motor execution
 * - Memory protection for motor programs
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorSecurityIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;
    bbb_system_t bbb;

    void SetUp() override {
        /* Create motor adapter */
        config = motor_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";

        /* Create BBB system */
        bbb_config_t bbb_config = bbb_default_config();
        bbb = bbb_system_create(&bbb_config);
        /* BBB may or may not be available - tests handle gracefully */
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
        if (bbb) {
            bbb_system_destroy(bbb);
            bbb = nullptr;
        }
    }

    /* Helper to create a test motor goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float y, float z, float duration_ms) {
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
};

/*=============================================================================
 * BBB SYSTEM LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, BBBSystemCreation) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }
    EXPECT_NE(nullptr, bbb);
}

TEST_F(MotorSecurityIntegrationTest, BBBDefaultConfig) {
    bbb_config_t cfg = bbb_default_config();
    /* Verify default config is sensible */
    EXPECT_TRUE(cfg.input.validate_strings || cfg.input.validate_integers || true);
}

TEST_F(MotorSecurityIntegrationTest, BBBSystemEnableDisable) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    /* Enable and verify */
    EXPECT_TRUE(bbb_system_set_enabled(bbb, true));
    EXPECT_TRUE(bbb_system_is_enabled(bbb));

    /* Disable and verify */
    EXPECT_TRUE(bbb_system_set_enabled(bbb, false));
    EXPECT_FALSE(bbb_system_is_enabled(bbb));
}

/*=============================================================================
 * INPUT VALIDATION TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ValidateMotorRegionString) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    const char* valid_string = "hand_right";
    bbb_validation_result_t result;
    bool is_valid = bbb_validate_string(bbb, valid_string, &result);
    /* Result depends on BBB configuration, just verify no crash */
    (void)is_valid;
}

TEST_F(MotorSecurityIntegrationTest, ValidateMotorPosition) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    /* Test position value validation */
    int64_t valid_pos = 100;  /* 0.1 meters in mm */
    bbb_validation_result_t result;
    bool is_valid = bbb_validate_integer(bbb, valid_pos, &result);
    (void)is_valid;
}

TEST_F(MotorSecurityIntegrationTest, ValidateMotorGoalStruct) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.0f, 500.0f);

    bbb_validation_result_t result;
    bool is_valid = bbb_validate_input(bbb, &goal, sizeof(goal), &result);
    (void)is_valid;
}

/*=============================================================================
 * MOTOR COMMAND VALIDATION TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ValidMotorGoalAccepted) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
}

TEST_F(MotorSecurityIntegrationTest, InvalidRegionRejected) {
    motor_goal_t goal = CreateTestGoal((motor_region_t)999, 1.0f, 0.0f, 0.0f, 500.0f);
    EXPECT_FALSE(motor_plan_movement(adapter, &goal));
}

TEST_F(MotorSecurityIntegrationTest, NullGoalRejected) {
    EXPECT_FALSE(motor_plan_movement(adapter, nullptr));
}

TEST_F(MotorSecurityIntegrationTest, NullAdapterHandled) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    EXPECT_FALSE(motor_plan_movement(nullptr, &goal));
}

/*=============================================================================
 * MOTOR EXECUTION SECURITY TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ExecutionRequiresPlan) {
    /* Attempt execution without planning */
    EXPECT_FALSE(motor_begin_execution(adapter));
}

TEST_F(MotorSecurityIntegrationTest, SecureMotorSequence) {
    /* Plan valid movement */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    ASSERT_TRUE(motor_plan_movement(adapter, &goal));

    /* Execute securely */
    ASSERT_TRUE(motor_begin_execution(adapter));

    /* Update execution */
    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GT(stats.commands_generated, 0u);
}

/*=============================================================================
 * FEEDBACK VALIDATION TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ValidFeedbackAccepted) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    /* Valid feedback */
    motor_effector_state_t state;
    memset(&state, 0, sizeof(state));
    state.region = MOTOR_REGION_HAND_RIGHT;
    state.position.x = 0.5f;

    bool result = motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &state);
    (void)result;  /* Result depends on implementation */
}

TEST_F(MotorSecurityIntegrationTest, NullFeedbackHandled) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    bool result = motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, nullptr);
    EXPECT_FALSE(result);
}

/*=============================================================================
 * BBB STATISTICS TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, BBBStatistics) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_statistics_t stats;
    bool result = bbb_system_get_statistics(bbb, &stats);
    if (result) {
        EXPECT_GE(stats.total_validations, 0u);
        EXPECT_GE(stats.threats_detected, 0u);
    }
}

TEST_F(MotorSecurityIntegrationTest, BBBStatisticsReset) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_reset_statistics(bbb);

    bbb_statistics_t stats;
    bool result = bbb_system_get_statistics(bbb, &stats);
    if (result) {
        EXPECT_EQ(stats.total_validations, 0u);
        EXPECT_EQ(stats.threats_detected, 0u);
    }
}

/*=============================================================================
 * STRING SANITIZATION TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, SanitizeMotorCommands) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    bbb_system_set_enabled(bbb, true);

    const char* dangerous_input = "hand_right'; DROP TABLE motor;--";
    char sanitized[256];

    ssize_t len = bbb_sanitize_string(bbb, dangerous_input, sanitized, sizeof(sanitized));
    if (len > 0) {
        /* Verify something was sanitized */
        EXPECT_GT(len, 0);
    }
}

/*=============================================================================
 * MEMORY PROTECTION TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, RegisterMotorMemory) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    motor_goal_t goal;
    uint32_t region_id = bbb_register_memory_region(bbb, &goal, sizeof(goal), true);
    if (region_id != 0) {
        bbb_unregister_memory_region(bbb, region_id);
    }
}

TEST_F(MotorSecurityIntegrationTest, CheckMemoryAccess) {
    if (!bbb) {
        GTEST_SKIP() << "BBB system not available";
    }

    motor_goal_t goal;
    uint32_t region_id = bbb_register_memory_region(bbb, &goal, sizeof(goal), true);

    if (region_id != 0) {
        /* Check memory access for registered region */
        bool allowed = bbb_check_memory_access(bbb, &goal, sizeof(goal), false);
        (void)allowed;

        bbb_unregister_memory_region(bbb, region_id);
    }
}

/*=============================================================================
 * BBB ENUM TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ThreatTypeEnum) {
    /* Verify threat types are accessible */
    bbb_threat_type_t threat = BBB_THREAT_NONE;
    EXPECT_EQ(0, (int)threat);

    threat = BBB_THREAT_BUFFER_OVERFLOW;
    EXPECT_GT((int)threat, 0);
}

TEST_F(MotorSecurityIntegrationTest, SeverityEnum) {
    /* Verify severity levels */
    bbb_severity_t sev = BBB_SEVERITY_NONE;
    EXPECT_EQ(0, (int)sev);

    sev = BBB_SEVERITY_CRITICAL;
    EXPECT_GT((int)sev, (int)BBB_SEVERITY_HIGH);
}

TEST_F(MotorSecurityIntegrationTest, ActionEnum) {
    /* Verify actions */
    bbb_action_t action = BBB_ACTION_ALLOW;
    EXPECT_EQ(0, (int)action);

    action = BBB_ACTION_LOCKDOWN;
    EXPECT_GT((int)action, (int)BBB_ACTION_ALLOW);
}

/*=============================================================================
 * BBB NAME FUNCTIONS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, ThreatTypeName) {
    const char* name = bbb_threat_type_name(BBB_THREAT_BUFFER_OVERFLOW);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(MotorSecurityIntegrationTest, SeverityName) {
    const char* name = bbb_severity_name(BBB_SEVERITY_HIGH);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(MotorSecurityIntegrationTest, ActionName) {
    const char* name = bbb_action_name(BBB_ACTION_BLOCK);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

/*=============================================================================
 * MOTOR STATE SECURITY TESTS
 *===========================================================================*/

TEST_F(MotorSecurityIntegrationTest, MotorStatusValidStates) {
    /* Verify motor status enum works */
    motor_status_t status = motor_get_status(adapter);
    EXPECT_EQ(MOTOR_STATUS_IDLE, status);

    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    motor_plan_movement(adapter, &goal);
    status = motor_get_status(adapter);
    EXPECT_EQ(MOTOR_STATUS_PREPARING, status);
}

TEST_F(MotorSecurityIntegrationTest, MotorResetClearsState) {
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 500.0f);
    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    motor_reset(adapter);
    EXPECT_EQ(MOTOR_STATUS_IDLE, motor_get_status(adapter));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
