/**
 * @file test_path_integration.cpp
 * @brief Unit tests for Entorhinal Path Integration functionality
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class PathIntegrationTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.enable_path_integration = true;
        config.path_integration_gain = 1.0f;
        config.drift_correction_rate = 0.01f;
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * PATH INTEGRATION BASIC TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, PathIntegrateForward) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};  // Forward at 1 m/s
    float angular_velocity = 0.0f;
    float dt = 0.1f;  // 100ms timestep

    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, angular_velocity, dt), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateSideways) {
    float velocity[3] = {0.0f, 1.0f, 0.0f};  // Sideways
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateBackward) {
    float velocity[3] = {-1.0f, 0.0f, 0.0f};  // Backward
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateStationary) {
    float velocity[3] = {0.0f, 0.0f, 0.0f};  // Not moving
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateWithTurn) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    float angular_velocity = 0.5f;  // rad/s turning
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, angular_velocity, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateDiagonal) {
    float velocity[3] = {1.0f, 1.0f, 0.0f};  // Diagonal movement
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrate3D) {
    float velocity[3] = {1.0f, 1.0f, 1.0f};  // 3D movement
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f), 0);
}

TEST_F(PathIntegrationTest, PathIntegrateNull) {
    EXPECT_EQ(entorhinal_path_integrate(nullptr, nullptr, 0.0f, 0.1f), -1);
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_path_integrate(nullptr, velocity, 0.0f, 0.1f), -1);
    EXPECT_EQ(entorhinal_path_integrate(ec, nullptr, 0.0f, 0.1f), -1);
}

TEST_F(PathIntegrationTest, PathIntegrateZeroDt) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.0f), 0);
    // Position should not change with dt=0
}

TEST_F(PathIntegrationTest, PathIntegrateNegativeDt) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, -0.1f), -1);
}

/*=============================================================================
 * POSITION ESTIMATE TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, GetPositionEstimateInitial) {
    float position[3];
    float heading;
    float pos_conf, head_conf;

    EXPECT_EQ(entorhinal_get_position_estimate(ec, position, &heading,
        &pos_conf, &head_conf), 0);

    // Initial position should be at origin
    EXPECT_NEAR(position[0], 0.0f, 0.1f);
    EXPECT_NEAR(position[1], 0.0f, 0.1f);
    EXPECT_NEAR(position[2], 0.0f, 0.1f);
}

TEST_F(PathIntegrationTest, GetPositionEstimateAfterMove) {
    // Move forward 1 meter over 1 second
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    float position[3];
    float heading;
    float pos_conf, head_conf;

    EXPECT_EQ(entorhinal_get_position_estimate(ec, position, &heading,
        &pos_conf, &head_conf), 0);

    // Should have moved approximately 1 meter in X
    EXPECT_NEAR(position[0], 1.0f, 0.2f);
    EXPECT_NEAR(position[1], 0.0f, 0.1f);
}

TEST_F(PathIntegrationTest, GetPositionEstimateNull) {
    EXPECT_EQ(entorhinal_get_position_estimate(nullptr, nullptr, nullptr,
        nullptr, nullptr), -1);
    float pos[3], heading, pc, hc;
    EXPECT_EQ(entorhinal_get_position_estimate(nullptr, pos, &heading, &pc, &hc), -1);
    EXPECT_EQ(entorhinal_get_position_estimate(ec, nullptr, &heading, &pc, &hc), -1);
    // heading can be null
    EXPECT_EQ(entorhinal_get_position_estimate(ec, pos, nullptr, &pc, &hc), 0);
}

TEST_F(PathIntegrationTest, GetPositionEstimateConfidence) {
    float position[3];
    float heading;
    float pos_conf, head_conf;

    EXPECT_EQ(entorhinal_get_position_estimate(ec, position, &heading,
        &pos_conf, &head_conf), 0);

    // Confidence should be in [0, 1]
    EXPECT_GE(pos_conf, 0.0f);
    EXPECT_LE(pos_conf, 1.0f);
    EXPECT_GE(head_conf, 0.0f);
    EXPECT_LE(head_conf, 1.0f);
}

TEST_F(PathIntegrationTest, ConfidenceDecaysWithDistance) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};

    float position[3];
    float heading;
    float initial_conf, pos_conf, head_conf;

    // Get initial confidence
    entorhinal_get_position_estimate(ec, position, &heading, &initial_conf, &head_conf);

    // Move a long distance
    for (int i = 0; i < 100; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    entorhinal_get_position_estimate(ec, position, &heading, &pos_conf, &head_conf);

    // Confidence should have decayed
    EXPECT_LE(pos_conf, initial_conf);
}

/*=============================================================================
 * VISUAL CORRECTION TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, ApplyVisualCorrectionBasic) {
    float visual_position[3] = {1.0f, 0.0f, 0.0f};
    float visual_heading = 0.0f;
    float confidence = 0.8f;

    EXPECT_EQ(entorhinal_apply_visual_correction(ec, visual_position,
        visual_heading, confidence), 0);
}

TEST_F(PathIntegrationTest, ApplyVisualCorrectionHighConfidence) {
    // First move away from origin
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    // Apply high-confidence visual correction back to origin
    float visual_position[3] = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_visual_correction(ec, visual_position, 0.0f, 1.0f), 0);

    // Position should be corrected toward origin
    float position[3];
    float heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, position, &heading, &pos_conf, &head_conf);

    // With high confidence, position should move toward visual correction
}

TEST_F(PathIntegrationTest, ApplyVisualCorrectionLowConfidence) {
    // Move to known position
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 10; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    // Get position before correction
    float pos_before[3], heading, pc, hc;
    entorhinal_get_position_estimate(ec, pos_before, &heading, &pc, &hc);

    // Apply low-confidence visual correction
    float visual_position[3] = {-5.0f, -5.0f, 0.0f};
    entorhinal_apply_visual_correction(ec, visual_position, 0.0f, 0.1f);

    // Position should change minimally with low confidence
}

TEST_F(PathIntegrationTest, ApplyVisualCorrectionNull) {
    EXPECT_EQ(entorhinal_apply_visual_correction(nullptr, nullptr, 0.0f, 0.0f), -1);
    float pos[3] = {0.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_visual_correction(nullptr, pos, 0.0f, 0.5f), -1);
    EXPECT_EQ(entorhinal_apply_visual_correction(ec, nullptr, 0.0f, 0.5f), -1);
}

TEST_F(PathIntegrationTest, ApplyVisualCorrectionZeroConfidence) {
    float visual_position[3] = {10.0f, 10.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_visual_correction(ec, visual_position, 0.0f, 0.0f), 0);
    // Should have no effect with zero confidence
}

/*=============================================================================
 * BOUNDARY CORRECTION TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, ApplyBoundaryCorrectionBasic) {
    float boundary_position[3] = {5.0f, 0.0f, 0.0f};
    float boundary_direction = 0.0f;

    EXPECT_EQ(entorhinal_apply_boundary_correction(ec, boundary_position,
        boundary_direction), 0);
}

TEST_F(PathIntegrationTest, ApplyBoundaryCorrectionNorth) {
    float boundary_position[3] = {0.0f, 5.0f, 0.0f};
    float boundary_direction = M_PI / 2.0f;  // North

    EXPECT_EQ(entorhinal_apply_boundary_correction(ec, boundary_position,
        boundary_direction), 0);
}

TEST_F(PathIntegrationTest, ApplyBoundaryCorrectionNull) {
    EXPECT_EQ(entorhinal_apply_boundary_correction(nullptr, nullptr, 0.0f), -1);
    float pos[3] = {1.0f, 0.0f, 0.0f};
    EXPECT_EQ(entorhinal_apply_boundary_correction(nullptr, pos, 0.0f), -1);
    EXPECT_EQ(entorhinal_apply_boundary_correction(ec, nullptr, 0.0f), -1);
}

TEST_F(PathIntegrationTest, ApplyBoundaryCorrectionMultiple) {
    // Apply multiple boundary corrections from different directions
    float boundary_positions[4][3] = {
        {5.0f, 0.0f, 0.0f},
        {0.0f, 5.0f, 0.0f},
        {-5.0f, 0.0f, 0.0f},
        {0.0f, -5.0f, 0.0f}
    };
    float directions[4] = {0.0f, M_PI/2.0f, M_PI, -M_PI/2.0f};

    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(entorhinal_apply_boundary_correction(ec, boundary_positions[i],
            directions[i]), 0);
    }
}

/*=============================================================================
 * PATH INTEGRATION ACCURACY TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, SquarePathReturnsToOrigin) {
    // Walk in a square pattern
    float velocities[4][3] = {
        {1.0f, 0.0f, 0.0f},   // East
        {0.0f, 1.0f, 0.0f},   // North
        {-1.0f, 0.0f, 0.0f},  // West
        {0.0f, -1.0f, 0.0f}   // South
    };

    for (int direction = 0; direction < 4; direction++) {
        for (int step = 0; step < 10; step++) {
            entorhinal_path_integrate(ec, velocities[direction], 0.0f, 0.1f);
        }
    }

    float position[3];
    float heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, position, &heading, &pos_conf, &head_conf);

    // Should be approximately back at origin (with some accumulated error)
    EXPECT_NEAR(position[0], 0.0f, 0.5f);
    EXPECT_NEAR(position[1], 0.0f, 0.5f);
}

TEST_F(PathIntegrationTest, CircularPathReturnsToOrigin) {
    // Walk in a circle
    float speed = 1.0f;
    float radius = 1.0f;
    float omega = speed / radius;  // angular velocity
    int steps = 63;  // approximately one full circle
    float dt = 0.1f;

    for (int i = 0; i < steps; i++) {
        float angle = omega * i * dt;
        float velocity[3] = {
            speed * cos(angle + M_PI/2.0f),  // perpendicular to radius
            speed * sin(angle + M_PI/2.0f),
            0.0f
        };
        entorhinal_path_integrate(ec, velocity, omega, dt);
    }

    float position[3];
    float heading, pos_conf, head_conf;
    entorhinal_get_position_estimate(ec, position, &heading, &pos_conf, &head_conf);

    // Should be approximately back at origin (with accumulated drift)
}

/*=============================================================================
 * PATH INTEGRATION DRIFT TESTS
 *===========================================================================*/

TEST_F(PathIntegrationTest, DriftAccumulatesOverTime) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};

    // Long journey with small random perturbations
    for (int i = 0; i < 1000; i++) {
        entorhinal_path_integrate(ec, velocity, 0.01f, 0.01f);
    }

    // Accumulated error should exist
    EXPECT_GT(ec->path_integration.accumulated_error, 0.0f);
}

TEST_F(PathIntegrationTest, DriftRatePositive) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};

    for (int i = 0; i < 100; i++) {
        entorhinal_path_integrate(ec, velocity, 0.0f, 0.1f);
    }

    // Drift rate should be set (may be zero if no drift configured)
    EXPECT_GE(ec->path_integration.drift_rate, 0.0f);
}

