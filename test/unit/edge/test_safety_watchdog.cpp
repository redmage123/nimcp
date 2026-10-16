/**
 * @file test_safety_watchdog.cpp
 * @brief Unit tests for NIMCP safety watchdog — arm/disarm, heartbeat, validation, e-stop.
 *
 * WHAT: Test safety-critical watchdog lifecycle, state transitions, output validation
 *       (NaN/Inf/magnitude), safe output generation, and emergency stop.
 * WHY:  The watchdog is the last line of defense before motor commands reach actuators.
 *       Any bug here can cause physical harm.
 * HOW:  Google Test, all tests use stub mode (no real actuators).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "edge/nimcp_safety_watchdog.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(SafetyWatchdog, CreateDestroyDefault) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, CreateWithConfig) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.timeout_ms = 200;
    cfg.action = NIMCP_SAFE_ACTION_HOLD;
    cfg.max_outputs = 8;

    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, DestroyNull) {
    nimcp_watchdog_destroy(NULL);
    SUCCEED() << "nimcp_watchdog_destroy(NULL) did not crash";
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(SafetyWatchdog, ConfigDefaultsReasonable) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    EXPECT_GT(cfg.timeout_ms, 0u);
    EXPECT_EQ(cfg.action, NIMCP_SAFE_ACTION_STOP);
    EXPECT_GT(cfg.max_outputs, 0u);
    EXPECT_GT(cfg.validation.max_output_magnitude, 0.0f);
    EXPECT_GT(cfg.validation.max_output_rate, 0.0f);
    EXPECT_GT(cfg.validation.consecutive_nan_limit, 0u);
    EXPECT_TRUE(cfg.validation.check_nan);
    EXPECT_TRUE(cfg.validation.check_magnitude);
}

// ============================================================================
// Arm / Disarm
// ============================================================================

TEST(SafetyWatchdog, ArmDisarm) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);

    int rc = nimcp_watchdog_arm(wd);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ARMED);

    rc = nimcp_watchdog_disarm(wd);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);

    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, DoubleArmNoCrash) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    nimcp_watchdog_arm(wd);
    int rc = nimcp_watchdog_arm(wd); // Already armed — should be no-op
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ARMED);

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, DoubleDisarmNoCrash) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    nimcp_watchdog_disarm(wd); // Already idle
    nimcp_watchdog_disarm(wd); // Still idle
    SUCCEED();

    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ArmNull) {
    EXPECT_EQ(nimcp_watchdog_arm(NULL), -1);
}

TEST(SafetyWatchdog, DisarmNull) {
    EXPECT_EQ(nimcp_watchdog_disarm(NULL), -1);
}

// ============================================================================
// Heartbeat
// ============================================================================

TEST(SafetyWatchdog, HeartbeatWhileArmed) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    nimcp_watchdog_arm(wd);
    // Multiple heartbeats should not crash
    for (int i = 0; i < 100; i++) {
        nimcp_watchdog_heartbeat(wd);
    }
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ARMED);

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, HeartbeatNull) {
    nimcp_watchdog_heartbeat(NULL);
    SUCCEED() << "nimcp_watchdog_heartbeat(NULL) did not crash";
}

TEST(SafetyWatchdog, HeartbeatWhileIdle) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    // Heartbeat while idle — silently ignored
    nimcp_watchdog_heartbeat(wd);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);

    nimcp_watchdog_destroy(wd);
}

// ============================================================================
// Output Validation
// ============================================================================

TEST(SafetyWatchdog, ValidateValidOutput) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    float output[4] = {0.1f, -0.2f, 0.5f, -0.8f};
    int rc = nimcp_watchdog_validate_output(wd, output, 4);
    EXPECT_EQ(rc, 0) << "Valid output should pass validation";

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ValidateNaNTriggers) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.validation.consecutive_nan_limit = 1; // Trigger on first NaN
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    float output[4] = {0.1f, NAN, 0.5f, 0.0f};
    int rc = nimcp_watchdog_validate_output(wd, output, 4);
    EXPECT_LT(rc, 0) << "NaN in output should fail validation";

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ValidateInfTriggers) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.validation.consecutive_nan_limit = 1;
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    float output[4] = {INFINITY, 0.0f, 0.0f, 0.0f};
    int rc = nimcp_watchdog_validate_output(wd, output, 4);
    EXPECT_LT(rc, 0) << "Inf in output should fail validation";

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ValidateMagnitudeClamp) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.validation.max_output_magnitude = 1.0f;
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    float output[4] = {5.0f, -3.0f, 0.5f, 0.0f};
    int rc = nimcp_watchdog_validate_output(wd, output, 4);
    EXPECT_LT(rc, 0) << "Excessive magnitude should fail validation";
    // Output should be clamped
    EXPECT_LE(output[0], 1.0f);
    EXPECT_GE(output[1], -1.0f);

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ValidateNullArgs) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    EXPECT_LT(nimcp_watchdog_validate_output(NULL, NULL, 0), 0);
    EXPECT_LT(nimcp_watchdog_validate_output(wd, NULL, 4), 0);

    float output[4] = {0.0f};
    EXPECT_LT(nimcp_watchdog_validate_output(wd, output, 0), 0);

    nimcp_watchdog_destroy(wd);
}

// ============================================================================
// Safe Output
// ============================================================================

TEST(SafetyWatchdog, GetSafeOutputStopMode) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.action = NIMCP_SAFE_ACTION_STOP;
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);

    float output[4];
    memset(output, 0xFF, sizeof(output));
    int rc = nimcp_watchdog_get_safe_output(wd, output, 4);
    EXPECT_EQ(rc, 0);

    // STOP mode: all zeros
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(output[i], 0.0f) << "STOP mode output[" << i << "] should be zero";
    }

    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, GetSafeOutputHoldMode) {
    nimcp_watchdog_config_t cfg = nimcp_watchdog_config_default();
    cfg.action = NIMCP_SAFE_ACTION_HOLD;
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(&cfg);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    // Submit a valid output first
    float valid_output[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    nimcp_watchdog_validate_output(wd, valid_output, 4);

    // Get safe output — should be last valid
    float safe[4];
    memset(safe, 0, sizeof(safe));
    int rc = nimcp_watchdog_get_safe_output(wd, safe, 4);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(safe[0], 0.1f, 0.01f);
    EXPECT_NEAR(safe[1], 0.2f, 0.01f);

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, GetSafeOutputNull) {
    EXPECT_LT(nimcp_watchdog_get_safe_output(NULL, NULL, 0), 0);

    // NULL watchdog but valid buffer — should zero the buffer
    float output[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_watchdog_get_safe_output(NULL, output, 4);
    EXPECT_FLOAT_EQ(output[0], 0.0f);
}

// ============================================================================
// Emergency Stop
// ============================================================================

TEST(SafetyWatchdog, EmergencyStop) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);
    nimcp_watchdog_arm(wd);

    nimcp_watchdog_estop(wd);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ESTOP);

    // Arm from ESTOP should fail
    int rc = nimcp_watchdog_arm(wd);
    EXPECT_LT(rc, 0) << "Cannot arm from ESTOP — must reset first";

    nimcp_watchdog_disarm(wd);
    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, EstopNull) {
    nimcp_watchdog_estop(NULL);
    SUCCEED() << "nimcp_watchdog_estop(NULL) did not crash";
}

// ============================================================================
// Reset
// ============================================================================

TEST(SafetyWatchdog, ResetFromTriggered) {
    nimcp_safety_watchdog_t* wd = nimcp_watchdog_create(NULL);
    ASSERT_NE(wd, nullptr);

    nimcp_watchdog_estop(wd);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_ESTOP);

    int rc = nimcp_watchdog_reset(wd);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(nimcp_watchdog_get_state(wd), NIMCP_WATCHDOG_IDLE);

    nimcp_watchdog_destroy(wd);
}

TEST(SafetyWatchdog, ResetNull) {
    EXPECT_EQ(nimcp_watchdog_reset(NULL), -1);
}

// ============================================================================
// State Names
// ============================================================================

TEST(SafetyWatchdog, StateNames) {
    EXPECT_STREQ(nimcp_watchdog_state_name(NIMCP_WATCHDOG_IDLE), "IDLE");
    EXPECT_STREQ(nimcp_watchdog_state_name(NIMCP_WATCHDOG_ARMED), "ARMED");
    EXPECT_STREQ(nimcp_watchdog_state_name(NIMCP_WATCHDOG_TRIGGERED), "TRIGGERED");
    EXPECT_STREQ(nimcp_watchdog_state_name(NIMCP_WATCHDOG_ESTOP), "ESTOP");
}

// ============================================================================
// NULL Safety Summary
// ============================================================================

TEST(SafetyWatchdog, GetStateNull) {
    EXPECT_EQ(nimcp_watchdog_get_state(NULL), NIMCP_WATCHDOG_IDLE);
}
