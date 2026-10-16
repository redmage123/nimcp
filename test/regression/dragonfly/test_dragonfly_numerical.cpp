//=============================================================================
// test_dragonfly_numerical.cpp - Numerical Stability Regression Tests
//=============================================================================
/**
 * @file test_dragonfly_numerical.cpp
 * @brief Numerical stability regression tests for dragonfly calculations
 *
 * WHAT: Tests that dragonfly calculations remain numerically stable
 * WHY:  Floating-point errors can accumulate and cause failures
 * HOW:  Test edge cases, extreme values, and long-running simulations
 *
 * CRITICAL AREAS:
 * - TSDN population vector normalization
 * - Kalman filter updates (prediction)
 * - Interception geometry calculations
 * - IMM model probability updates
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// Helper Functions
//=============================================================================

static bool is_finite_float(float x) {
    return std::isfinite(x);
}

static bool is_valid_angle(float angle) {
    return is_finite_float(angle) && angle >= -M_PI * 2 && angle <= M_PI * 2;
}

static bool is_valid_probability(float p) {
    return is_finite_float(p) && p >= 0.0f && p <= 1.0f;
}

//=============================================================================
// Test Fixture
//=============================================================================

class DragonflyNumericalTest : public ::testing::Test {
protected:
    dragonfly_system_t* system = nullptr;

    void SetUp() override {
        dragonfly_config_t config = dragonfly_default_config();
        system = dragonfly_system_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) {
            dragonfly_system_destroy(system);
            system = nullptr;
        }
    }
};

//=============================================================================
// TSDN Numerical Stability Tests
//=============================================================================

TEST_F(DragonflyNumericalTest, TSDNPopulationVectorNormalized) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    tsdn_population_t* tsdn = tsdn_create(&config);
    ASSERT_NE(tsdn, nullptr);

    // Process many directions
    for (int i = 0; i < 360; i++) {
        float direction = (float)i * M_PI / 180.0f;

        tsdn_vector_t output = tsdn_encode_direction(tsdn, direction);

        // Population vector should be valid
        EXPECT_TRUE(is_valid_angle(output.direction))
            << "Direction should be valid at input angle " << i;
        EXPECT_TRUE(is_finite_float(output.magnitude))
            << "Magnitude should be finite";
        EXPECT_GE(output.magnitude, 0.0f);
    }

    tsdn_destroy(tsdn);
}

TEST_F(DragonflyNumericalTest, TSDNExtremeSpeeds) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    tsdn_population_t* tsdn = tsdn_create(&config);
    ASSERT_NE(tsdn, nullptr);

    // Test extreme positions (which affects angle computation)
    float positions[][2] = {
        {0.001f, 0.0f},    // Very close
        {1000.0f, 0.0f},   // Very far
        {100.0f, 0.0f},    // Moderate
        {0.0f, 0.001f}     // Edge case
    };

    for (int i = 0; i < 4; i++) {
        tsdn_vector_t output = tsdn_encode(tsdn, positions[i][0], positions[i][1]);
        EXPECT_TRUE(is_finite_float(output.direction));
        EXPECT_TRUE(is_finite_float(output.magnitude));
    }

    tsdn_destroy(tsdn);
}

TEST_F(DragonflyNumericalTest, TSDNOriginEdgeCase) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    tsdn_population_t* tsdn = tsdn_create(&config);
    ASSERT_NE(tsdn, nullptr);

    // Target at origin - edge case for atan2
    tsdn_vector_t output = tsdn_encode(tsdn, 0.0f, 0.0f);
    EXPECT_TRUE(is_finite_float(output.direction));
    EXPECT_TRUE(is_finite_float(output.magnitude));

    tsdn_destroy(tsdn);
}

//=============================================================================
// Prediction Numerical Stability Tests
//=============================================================================

TEST_F(DragonflyNumericalTest, PredictionLongDuration) {
    prediction_config_t config = prediction_default_config();
    dragonfly_predictor_t* predictor = dragonfly_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    // Long duration prediction without divergence
    for (int i = 0; i < 10000; i++) {
        float t = i * 0.016f;
        float position[3] = {10.0f + t, 5.0f + sinf(t), 0.0f};
        float velocity[3] = {1.0f, cosf(t), 0.0f};

        EXPECT_EQ(dragonfly_predictor_update(predictor, position, velocity, 0.016f), 0);

        predicted_state_t state;
        EXPECT_EQ(dragonfly_predictor_get_state_at(predictor, 100.0f, &state), 0);

        // All outputs should remain finite
        EXPECT_TRUE(is_finite_float(state.position[0]))
            << "X prediction diverged at iteration " << i;
        EXPECT_TRUE(is_finite_float(state.position[1]))
            << "Y prediction diverged at iteration " << i;
        EXPECT_TRUE(is_finite_float(state.position[2]))
            << "Z prediction diverged at iteration " << i;
        EXPECT_TRUE(is_finite_float(state.velocity[0]));
        EXPECT_TRUE(is_finite_float(state.velocity[1]));
        EXPECT_TRUE(is_finite_float(state.velocity[2]));
    }

    dragonfly_predictor_destroy(predictor);
}

TEST_F(DragonflyNumericalTest, PredictionWithJumps) {
    prediction_config_t config = prediction_default_config();
    dragonfly_predictor_t* predictor = dragonfly_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    // Introduce sudden position jumps (simulating lost track)
    for (int i = 0; i < 100; i++) {
        float x = (i % 10 == 0) ? 50.0f : 10.0f + i * 0.1f;  // Jump every 10 frames
        float position[3] = {x, 5.0f, 0.0f};
        float velocity[3] = {1.0f, 0.0f, 0.0f};

        EXPECT_EQ(dragonfly_predictor_update(predictor, position, velocity, 0.016f), 0);

        predicted_state_t state;
        EXPECT_EQ(dragonfly_predictor_get_state_at(predictor, 50.0f, &state), 0);

        // Should not diverge even with jumps
        EXPECT_TRUE(is_finite_float(state.position[0]));
        EXPECT_TRUE(is_finite_float(state.confidence));
    }

    dragonfly_predictor_destroy(predictor);
}

TEST_F(DragonflyNumericalTest, PredictionHighFrequencyOscillation) {
    prediction_config_t config = prediction_default_config();
    dragonfly_predictor_t* predictor = dragonfly_predictor_create(&config);
    ASSERT_NE(predictor, nullptr);

    // High-frequency oscillation (challenging for Kalman filter)
    for (int i = 0; i < 1000; i++) {
        float t = i * 0.001f;  // 1000 Hz
        float x = 10.0f + 0.5f * sinf(t * 100.0f);  // 100 Hz oscillation
        float position[3] = {x, 5.0f, 0.0f};
        float velocity[3] = {50.0f * cosf(t * 100.0f), 0.0f, 0.0f};

        EXPECT_EQ(dragonfly_predictor_update(predictor, position, velocity, 0.001f), 0);

        predicted_state_t state;
        EXPECT_EQ(dragonfly_predictor_get_state_at(predictor, 10.0f, &state), 0);

        EXPECT_TRUE(is_finite_float(state.position[0]));
    }

    dragonfly_predictor_destroy(predictor);
}

//=============================================================================
// Interception Numerical Stability Tests
//=============================================================================

TEST_F(DragonflyNumericalTest, InterceptionNearOrigin) {
    intercept_config_t config = intercept_default_config();
    dragonfly_interceptor_t* interceptor = dragonfly_interceptor_create(&config);
    ASSERT_NE(interceptor, nullptr);

    // Target near origin (potential division by zero)
    interceptor_state_t self = {
        .position = {0.0f, 0.0f, 0.0f},
        .velocity = {0.0f, 0.0f, 0.0f},
        .max_speed = 10.0f,
        .max_accel = 5.0f,
        .max_turn_rate = 3.0f
    };
    target_state_t target = {
        .position = {0.001f, 0.001f, 0.0f},
        .velocity = {-0.1f, 0.0f, 0.0f},
        .acceleration = {0.0f, 0.0f, 0.0f},
        .confidence = 0.9f
    };
    intercept_solution_t solution;
    EXPECT_EQ(dragonfly_intercept_compute(interceptor, &self, &target, &solution), 0);

    EXPECT_TRUE(is_finite_float(solution.intercept_point[0]));
    EXPECT_TRUE(is_finite_float(solution.intercept_point[1]));
    EXPECT_TRUE(is_finite_float(solution.intercept_point[2]));
    EXPECT_TRUE(is_finite_float(solution.intercept_time_s));

    dragonfly_interceptor_destroy(interceptor);
}

TEST_F(DragonflyNumericalTest, InterceptionParallelTrajectories) {
    intercept_config_t config = intercept_default_config();
    dragonfly_interceptor_t* interceptor = dragonfly_interceptor_create(&config);
    ASSERT_NE(interceptor, nullptr);

    // Parallel trajectories (no intersection)
    interceptor_state_t self = {
        .position = {0.0f, 0.0f, 0.0f},
        .velocity = {1.0f, 0.0f, 0.0f},  // Same direction, parallel
        .max_speed = 10.0f,
        .max_accel = 5.0f,
        .max_turn_rate = 3.0f
    };
    target_state_t target = {
        .position = {10.0f, 10.0f, 0.0f},
        .velocity = {1.0f, 0.0f, 0.0f},
        .acceleration = {0.0f, 0.0f, 0.0f},
        .confidence = 0.9f
    };
    intercept_solution_t solution;
    int result = dragonfly_intercept_compute(interceptor, &self, &target, &solution);
    // May return error for infeasible, but should not crash
    EXPECT_TRUE(result == 0 || result == -1);

    // Even if infeasible, outputs should be finite
    EXPECT_TRUE(is_finite_float(solution.intercept_point[0]));
    EXPECT_TRUE(is_finite_float(solution.intercept_point[1]));

    dragonfly_interceptor_destroy(interceptor);
}

TEST_F(DragonflyNumericalTest, InterceptionVeryFarTarget) {
    intercept_config_t config = intercept_default_config();
    dragonfly_interceptor_t* interceptor = dragonfly_interceptor_create(&config);
    ASSERT_NE(interceptor, nullptr);

    // Very far target
    interceptor_state_t self = {
        .position = {0.0f, 0.0f, 0.0f},
        .velocity = {0.0f, 0.0f, 0.0f},
        .max_speed = 50.0f,
        .max_accel = 10.0f,
        .max_turn_rate = 3.0f
    };
    target_state_t target = {
        .position = {10000.0f, 5000.0f, 1000.0f},
        .velocity = {-100.0f, 0.0f, 0.0f},
        .acceleration = {0.0f, 0.0f, 0.0f},
        .confidence = 0.9f
    };
    intercept_solution_t solution;
    dragonfly_intercept_compute(interceptor, &self, &target, &solution);

    EXPECT_TRUE(is_finite_float(solution.intercept_point[0]));
    EXPECT_TRUE(is_finite_float(solution.intercept_point[1]));
    EXPECT_TRUE(is_finite_float(solution.intercept_point[2]));
    EXPECT_TRUE(is_finite_float(solution.intercept_time_s));

    dragonfly_interceptor_destroy(interceptor);
}

//=============================================================================
// System-Level Numerical Stability Tests
//=============================================================================

TEST_F(DragonflyNumericalTest, SystemLongRunningSimulation) {
    // Run for equivalent of 10 minutes at 60 FPS
    const int NUM_FRAMES = 36000;
    const float dt = 0.016f;

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        float t = frame * dt;

        // Circular target motion
        float x = 15.0f + 5.0f * cosf(t * 0.5f);
        float y = 5.0f + 5.0f * sinf(t * 0.5f);

        dragonfly_detection_t detection = {
            .position = {x, y, 0.0f},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = t * 0.5f + (float)(M_PI / 2),
            .motion_speed = 2.5f,
            .timestamp_us = (uint64_t)(t * 1000000),
            .id = 1
        };
        EXPECT_EQ(dragonfly_process_detection(system, &detection), 0);
        EXPECT_EQ(dragonfly_update(system, dt), 0);

        // Verify motor command is valid
        dragonfly_motor_cmd_t cmd;
        EXPECT_EQ(dragonfly_get_motor_command(system, &cmd), 0);

        EXPECT_TRUE(is_valid_angle(cmd.heading_rad))
            << "Heading invalid at frame " << frame;
        EXPECT_TRUE(is_valid_angle(cmd.pitch_rad))
            << "Pitch invalid at frame " << frame;
        EXPECT_TRUE(is_finite_float(cmd.velocity[0]));
        EXPECT_TRUE(is_finite_float(cmd.velocity[1]));
        EXPECT_TRUE(is_finite_float(cmd.velocity[2]));
        EXPECT_TRUE(is_valid_probability(cmd.urgency))
            << "Urgency invalid at frame " << frame;
    }
}

TEST_F(DragonflyNumericalTest, SystemExtremePositions) {
    float positions[][3] = {
        {1e6f, 1e6f, 1e6f},      // Very far
        {1e-6f, 1e-6f, 1e-6f},   // Very close
        {-1e4f, 1e4f, -1e4f},    // Mixed signs
        {0.0f, 0.0f, 0.0f},      // Origin
    };

    for (int i = 0; i < 4; i++) {
        dragonfly_system_reset(system);

        dragonfly_detection_t detection = {
            .position = {positions[i][0], positions[i][1], positions[i][2]},
            .size = 0.05f,
            .contrast = 0.8f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 2.0f,
            .timestamp_us = 0,
            .id = 1
        };
        dragonfly_process_detection(system, &detection);
        dragonfly_update(system, 0.016f);

        dragonfly_motor_cmd_t cmd;
        dragonfly_get_motor_command(system, &cmd);

        EXPECT_TRUE(is_finite_float(cmd.heading_rad))
            << "Heading not finite for position " << i;
        EXPECT_TRUE(is_finite_float(cmd.pitch_rad))
            << "Pitch not finite for position " << i;
    }
}

TEST_F(DragonflyNumericalTest, SystemRapidModeTransitions) {
    // Rapid start/abort cycles
    for (int i = 0; i < 100; i++) {
        // Start tracking
        dragonfly_detection_t detection = {
            .position = {10.0f, 5.0f, 0.0f},
            .size = 0.05f,
            .contrast = 0.9f,
            .motion_direction_rad = 0.5f,
            .motion_speed = 3.0f,
            .timestamp_us = (uint64_t)(i * 100000),
            .id = 1
        };
        dragonfly_process_detection(system, &detection);
        dragonfly_update(system, 0.016f);

        // Abort
        dragonfly_abort_pursuit(system);

        // Verify system is stable
        dragonfly_motor_cmd_t cmd;
        EXPECT_EQ(dragonfly_get_motor_command(system, &cmd), 0);
        EXPECT_TRUE(is_finite_float(cmd.heading_rad));
    }
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(DragonflyNumericalTest, ZeroDeltaTime) {
    dragonfly_detection_t detection = {
        .position = {10.0f, 5.0f, 0.0f},
        .size = 0.05f,
        .contrast = 0.8f,
        .motion_direction_rad = 0.5f,
        .motion_speed = 2.0f,
        .timestamp_us = 0,
        .id = 1
    };
    dragonfly_process_detection(system, &detection);

    // Zero delta time - should not crash or produce NaN
    int result = dragonfly_update(system, 0.0f);
    EXPECT_TRUE(result == 0 || result == -1);  // May reject zero dt

    dragonfly_motor_cmd_t cmd;
    dragonfly_get_motor_command(system, &cmd);
    EXPECT_TRUE(is_finite_float(cmd.heading_rad));
}

TEST_F(DragonflyNumericalTest, VerySmallDeltaTime) {
    dragonfly_detection_t detection = {
        .position = {10.0f, 5.0f, 0.0f},
        .size = 0.05f,
        .contrast = 0.8f,
        .motion_direction_rad = 0.5f,
        .motion_speed = 2.0f,
        .timestamp_us = 0,
        .id = 1
    };
    dragonfly_process_detection(system, &detection);

    // Very small delta time
    EXPECT_EQ(dragonfly_update(system, 1e-9f), 0);

    dragonfly_motor_cmd_t cmd;
    dragonfly_get_motor_command(system, &cmd);
    EXPECT_TRUE(is_finite_float(cmd.heading_rad));
}

TEST_F(DragonflyNumericalTest, VeryLargeDeltaTime) {
    dragonfly_detection_t detection = {
        .position = {10.0f, 5.0f, 0.0f},
        .size = 0.05f,
        .contrast = 0.8f,
        .motion_direction_rad = 0.5f,
        .motion_speed = 2.0f,
        .timestamp_us = 0,
        .id = 1
    };
    dragonfly_process_detection(system, &detection);

    // Very large delta time (1 second)
    int result = dragonfly_update(system, 1.0f);
    EXPECT_TRUE(result == 0 || result == -1);

    dragonfly_motor_cmd_t cmd;
    dragonfly_get_motor_command(system, &cmd);
    EXPECT_TRUE(is_finite_float(cmd.heading_rad));
}
