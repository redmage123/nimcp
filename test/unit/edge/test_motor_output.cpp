/**
 * @file test_motor_output.cpp
 * @brief Unit tests for NIMCP motor output translator — channel mapping,
 *        deadzone, scaling, clamping, smoothing, and presets.
 *
 * WHAT: Test motor output lifecycle, translation pipeline, presets, and NULL safety.
 * WHY:  Motor output is the final actuator interface; incorrect translation
 *       can cause dangerous physical behavior on real hardware.
 * HOW:  Google Test, stub mode (no real actuators).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>

extern "C" {
#include "edge/nimcp_motor_output.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(MotorOutput, CreateDestroyDefault) {
    nimcp_motor_output_t* motor = nimcp_motor_create(NULL);
    ASSERT_NE(motor, nullptr);
    EXPECT_EQ(nimcp_motor_get_num_channels(motor), 6u)
        << "Default config should create 6-channel twist";
    nimcp_motor_destroy(motor);
}

TEST(MotorOutput, DestroyNull) {
    nimcp_motor_destroy(NULL);
    SUCCEED() << "nimcp_motor_destroy(NULL) did not crash";
}

// ============================================================================
// Preset: Twist (6-DOF)
// ============================================================================

TEST(MotorOutput, PresetTwist) {
    nimcp_motor_config_t cfg = nimcp_motor_preset_twist(1.0f, 2.0f);
    EXPECT_EQ(cfg.type, NIMCP_MOTOR_TWIST);
    EXPECT_EQ(cfg.num_channels, 6u);

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    // Feed identity brain outputs [1,1,1,1,1,1]
    float brain[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float cmd[6] = {0};
    int rc = nimcp_motor_translate(motor, brain, 6, cmd, 6);
    EXPECT_EQ(rc, 0);

    // All 6 channels should produce non-zero output
    for (int i = 0; i < 6; i++) {
        EXPECT_NE(cmd[i], 0.0f) << "Channel " << i << " should be non-zero";
    }

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Preset: Quadrotor (4 motors)
// ============================================================================

TEST(MotorOutput, PresetQuadrotor) {
    nimcp_motor_config_t cfg = nimcp_motor_preset_quadrotor(0.0f, 1.0f);
    EXPECT_EQ(cfg.type, NIMCP_MOTOR_QUADROTOR);
    EXPECT_EQ(cfg.num_channels, 4u);

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float cmd[4] = {0};
    int rc = nimcp_motor_translate(motor, brain, 4, cmd, 4);
    EXPECT_EQ(rc, 0);

    for (int i = 0; i < 4; i++) {
        EXPECT_GE(cmd[i], 0.0f) << "Quadrotor channel " << i << " below min";
        EXPECT_LE(cmd[i], 1.0f) << "Quadrotor channel " << i << " above max";
    }

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Preset: Differential Drive (2 wheels)
// ============================================================================

TEST(MotorOutput, PresetDifferential) {
    nimcp_motor_config_t cfg = nimcp_motor_preset_differential(5.0f);
    EXPECT_EQ(cfg.type, NIMCP_MOTOR_DIFFERENTIAL);
    EXPECT_EQ(cfg.num_channels, 2u);

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[2] = {0.8f, -0.3f};
    float cmd[2] = {0};
    int rc = nimcp_motor_translate(motor, brain, 2, cmd, 2);
    EXPECT_EQ(rc, 0);

    // Commands should be clamped within [-5, 5]
    for (int i = 0; i < 2; i++) {
        EXPECT_GE(cmd[i], -5.0f);
        EXPECT_LE(cmd[i], 5.0f);
    }

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Deadzone
// ============================================================================

TEST(MotorOutput, DeadzoneZerosSmallValues) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_TWIST;
    cfg.num_channels = 2;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = true;
    cfg.enable_smoothing = false;
    for (uint32_t i = 0; i < 2; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = 1.0f;
        cfg.channels[i].offset = 0.0f;
        cfg.channels[i].min_value = -10.0f;
        cfg.channels[i].max_value = 10.0f;
        cfg.channels[i].deadzone = 0.1f;
    }

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    // Brain output below deadzone threshold
    float brain[2] = {0.05f, -0.03f};
    float cmd[2] = {99.0f, 99.0f};
    int rc = nimcp_motor_translate(motor, brain, 2, cmd, 2);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(cmd[0], 0.0f) << "Below deadzone should be zero";
    EXPECT_FLOAT_EQ(cmd[1], 0.0f) << "Below deadzone should be zero";

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Scale + Offset
// ============================================================================

TEST(MotorOutput, ScaleAndOffset) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 2.0f;
    cfg.channels[0].offset = 0.5f;
    cfg.channels[0].min_value = -100.0f;
    cfg.channels[0].max_value = 100.0f;
    cfg.channels[0].deadzone = 0.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[1] = {3.0f};
    float cmd[1] = {0};
    int rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    EXPECT_EQ(rc, 0);
    // Expected: 3.0 * 2.0 * 1.0 + 0.5 = 6.5
    EXPECT_NEAR(cmd[0], 6.5f, 0.01f);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Clamp
// ============================================================================

TEST(MotorOutput, ClampToMinMax) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 10.0f;
    cfg.channels[0].offset = 0.0f;
    cfg.channels[0].min_value = -5.0f;
    cfg.channels[0].max_value = 5.0f;
    cfg.channels[0].deadzone = 0.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    // Brain output 10.0 * scale 10.0 = 100.0 -> clamped to 5.0
    float brain[1] = {10.0f};
    float cmd[1] = {0};
    int rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(cmd[0], 5.0f);

    // Negative: -10.0 * 10.0 = -100.0 -> clamped to -5.0
    brain[0] = -10.0f;
    rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(cmd[0], -5.0f);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Smoothing
// ============================================================================

TEST(MotorOutput, SmoothingApplied) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = true;
    cfg.smoothing_alpha = 0.5f;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 1.0f;
    cfg.channels[0].offset = 0.0f;
    cfg.channels[0].min_value = -100.0f;
    cfg.channels[0].max_value = 100.0f;
    cfg.channels[0].deadzone = 0.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    // First call: no previous value, output should be the raw value
    float brain[1] = {10.0f};
    float cmd[1] = {0};
    nimcp_motor_translate(motor, brain, 1, cmd, 1);
    float first = cmd[0];

    // Second call with different value: should be smoothed
    brain[0] = 0.0f;
    nimcp_motor_translate(motor, brain, 1, cmd, 1);
    float second = cmd[0];

    // The second value should be between 0 and the first value (smoothed)
    EXPECT_GT(second, 0.0f) << "Smoothed value should retain some of previous";
    EXPECT_LT(second, first) << "Smoothed value should move toward new input";

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Global Scale
// ============================================================================

TEST(MotorOutput, GlobalScaleMultiplier) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 0.5f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 1.0f;
    cfg.channels[0].offset = 0.0f;
    cfg.channels[0].min_value = -100.0f;
    cfg.channels[0].max_value = 100.0f;
    cfg.channels[0].deadzone = 0.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[1] = {4.0f};
    float cmd[1] = {0};
    int rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    EXPECT_EQ(rc, 0);
    // Expected: 4.0 * 1.0 * 0.5 + 0.0 = 2.0
    EXPECT_NEAR(cmd[0], 2.0f, 0.01f);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Set Channel at Runtime
// ============================================================================

TEST(MotorOutput, SetChannelRuntime) {
    nimcp_motor_output_t* motor = nimcp_motor_create(NULL);
    ASSERT_NE(motor, nullptr);

    nimcp_motor_channel_t ch;
    memset(&ch, 0, sizeof(ch));
    ch.brain_output_idx = 0;
    ch.scale = 3.0f;
    ch.offset = 1.0f;
    ch.min_value = -50.0f;
    ch.max_value = 50.0f;
    ch.deadzone = 0.0f;

    int rc = nimcp_motor_set_channel(motor, 0, &ch);
    EXPECT_EQ(rc, 0);

    // Out of range index should fail
    rc = nimcp_motor_set_channel(motor, 999, &ch);
    EXPECT_LT(rc, 0);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Get Num Channels
// ============================================================================

TEST(MotorOutput, GetNumChannels) {
    nimcp_motor_config_t cfg = nimcp_motor_preset_quadrotor(0.0f, 1.0f);
    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);
    EXPECT_EQ(nimcp_motor_get_num_channels(motor), 4u);
    nimcp_motor_destroy(motor);
}

TEST(MotorOutput, GetNumChannelsNull) {
    EXPECT_EQ(nimcp_motor_get_num_channels(NULL), 0u);
}

// ============================================================================
// NULL Safety
// ============================================================================

TEST(MotorOutput, TranslateNullMotor) {
    float brain[6] = {0};
    float cmd[6] = {0};
    int rc = nimcp_motor_translate(NULL, brain, 6, cmd, 6);
    EXPECT_LT(rc, 0);
}

TEST(MotorOutput, TranslateNullBrainOutput) {
    nimcp_motor_output_t* motor = nimcp_motor_create(NULL);
    ASSERT_NE(motor, nullptr);
    float cmd[6] = {0};
    int rc = nimcp_motor_translate(motor, NULL, 6, cmd, 6);
    EXPECT_LT(rc, 0);
    nimcp_motor_destroy(motor);
}

TEST(MotorOutput, TranslateNullMotorCommands) {
    nimcp_motor_output_t* motor = nimcp_motor_create(NULL);
    ASSERT_NE(motor, nullptr);
    float brain[6] = {0};
    int rc = nimcp_motor_translate(motor, brain, 6, NULL, 6);
    EXPECT_LT(rc, 0);
    nimcp_motor_destroy(motor);
}

TEST(MotorOutput, SetChannelNull) {
    nimcp_motor_channel_t ch;
    memset(&ch, 0, sizeof(ch));
    int rc = nimcp_motor_set_channel(NULL, 0, &ch);
    EXPECT_LT(rc, 0);
}

// ============================================================================
// NaN and Inf Handling
// ============================================================================

TEST(MotorOutput, NaNInBrainOutput) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 1.0f;
    cfg.channels[0].min_value = -1.0f;
    cfg.channels[0].max_value = 1.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[1] = {std::numeric_limits<float>::quiet_NaN()};
    float cmd[1] = {99.0f};
    int rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    // Should either return error or produce a safe value (0 or clamped)
    if (rc == 0) {
        EXPECT_FALSE(std::isnan(cmd[0])) << "NaN must not propagate to motor commands";
    }

    nimcp_motor_destroy(motor);
}

TEST(MotorOutput, InfInBrainOutput) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.channels[0].brain_output_idx = 0;
    cfg.channels[0].scale = 1.0f;
    cfg.channels[0].min_value = -1.0f;
    cfg.channels[0].max_value = 1.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[1] = {std::numeric_limits<float>::infinity()};
    float cmd[1] = {0};
    int rc = nimcp_motor_translate(motor, brain, 1, cmd, 1);
    if (rc == 0) {
        EXPECT_FALSE(std::isinf(cmd[0])) << "Inf must be clamped";
        EXPECT_LE(cmd[0], 1.0f) << "Inf should clamp to max";
    }

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Zero Brain Output
// ============================================================================

TEST(MotorOutput, ZeroBrainOutputNoOffset) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 2;
    cfg.global_scale = 1.0f;
    cfg.enable_deadzone = false;
    cfg.enable_smoothing = false;
    for (uint32_t i = 0; i < 2; i++) {
        cfg.channels[i].brain_output_idx = i;
        cfg.channels[i].scale = 5.0f;
        cfg.channels[i].offset = 0.0f;
        cfg.channels[i].min_value = -10.0f;
        cfg.channels[i].max_value = 10.0f;
        cfg.channels[i].deadzone = 0.0f;
    }

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[2] = {0.0f, 0.0f};
    float cmd[2] = {99.0f, 99.0f};
    int rc = nimcp_motor_translate(motor, brain, 2, cmd, 2);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(cmd[0], 0.0f);
    EXPECT_FLOAT_EQ(cmd[1], 0.0f);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Brain Output Index Out of Range
// ============================================================================

TEST(MotorOutput, BrainOutputIndexOutOfRange) {
    nimcp_motor_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.type = NIMCP_MOTOR_PWM;
    cfg.num_channels = 1;
    cfg.global_scale = 1.0f;
    cfg.channels[0].brain_output_idx = 10;  // Only 2 brain outputs provided
    cfg.channels[0].scale = 1.0f;
    cfg.channels[0].min_value = -1.0f;
    cfg.channels[0].max_value = 1.0f;

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    float brain[2] = {1.0f, 2.0f};
    float cmd[1] = {99.0f};
    int rc = nimcp_motor_translate(motor, brain, 2, cmd, 1);
    // Should either fail or use safe default (0)
    if (rc == 0) {
        EXPECT_FALSE(std::isnan(cmd[0])) << "Out-of-range index should not produce NaN";
    }

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Arm Preset with Per-Joint Limits
// ============================================================================

TEST(MotorOutput, PresetArm) {
    float limits[6] = {
        -1.57f, 1.57f,   // Joint 0
        -0.5f,  0.5f,    // Joint 1
        0.0f,   3.14f,   // Joint 2
    };
    nimcp_motor_config_t cfg = nimcp_motor_preset_arm(3, limits);
    EXPECT_EQ(cfg.type, NIMCP_MOTOR_JOINT_POSITION);
    EXPECT_EQ(cfg.num_channels, 3u);

    nimcp_motor_output_t* motor = nimcp_motor_create(&cfg);
    ASSERT_NE(motor, nullptr);

    // Check that per-joint limits are respected
    float brain[3] = {100.0f, 100.0f, 100.0f};
    float cmd[3] = {0};
    int rc = nimcp_motor_translate(motor, brain, 3, cmd, 3);
    EXPECT_EQ(rc, 0);

    // Each channel should be clamped to its upper limit
    EXPECT_LE(cmd[0], 1.57f + 0.01f);
    EXPECT_LE(cmd[1], 0.5f + 0.01f);
    EXPECT_LE(cmd[2], 3.14f + 0.01f);

    nimcp_motor_destroy(motor);
}

// ============================================================================
// Motor Type Name
// ============================================================================

TEST(MotorOutput, TypeNames) {
    const char* name = nimcp_motor_type_name(NIMCP_MOTOR_TWIST);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_motor_type_name(NIMCP_MOTOR_QUADROTOR);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_motor_type_name(NIMCP_MOTOR_DIFFERENTIAL);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = nimcp_motor_type_name(NIMCP_MOTOR_JOINT_POSITION);
    EXPECT_NE(name, nullptr);
}
