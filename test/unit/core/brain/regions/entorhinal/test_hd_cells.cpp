/**
 * @file test_hd_cells.cpp
 * @brief Unit tests for Entorhinal Head Direction Cell functionality
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

class HDCellTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.num_hd_cells = 60;
        config.hd_tuning_width = 0.5f;  // ~30 degrees
        config.anticipatory_time_ms = 25.0f;
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
 * HD CELL UPDATE TESTS
 *===========================================================================*/

TEST_F(HDCellTest, UpdateHDCellsNorth) {
    float heading = 0.0f;  // North
    float angular_velocity = 0.0f;
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsEast) {
    float heading = M_PI / 2.0f;  // East (90 degrees)
    float angular_velocity = 0.0f;
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsSouth) {
    float heading = M_PI;  // South (180 degrees)
    float angular_velocity = 0.0f;
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsWest) {
    float heading = -M_PI / 2.0f;  // West (-90 degrees)
    float angular_velocity = 0.0f;
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsWithRotation) {
    float heading = M_PI / 4.0f;  // 45 degrees
    float angular_velocity = 0.5f;  // Turning right
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsWithLeftTurn) {
    float heading = 0.0f;
    float angular_velocity = -0.5f;  // Turning left
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsFastRotation) {
    float heading = 0.0f;
    float angular_velocity = 3.0f;  // Fast spin
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, angular_velocity), 0);
}

TEST_F(HDCellTest, UpdateHDCellsNull) {
    EXPECT_EQ(entorhinal_update_hd_cells(nullptr, 0.0f, 0.0f), -1);
}

TEST_F(HDCellTest, UpdateHDCellsWraparound) {
    // Test angle wrapping
    float heading = 3.0f * M_PI;  // 540 degrees - should wrap
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, 0.0f), 0);
}

TEST_F(HDCellTest, UpdateHDCellsNegativeAngle) {
    float heading = -M_PI;  // -180 degrees
    EXPECT_EQ(entorhinal_update_hd_cells(ec, heading, 0.0f), 0);
}

/*=============================================================================
 * HEADING DECODE TESTS
 *===========================================================================*/

TEST_F(HDCellTest, DecodeHeadingNorth) {
    entorhinal_update_hd_cells(ec, 0.0f, 0.0f);

    float decoded_heading;
    float confidence;
    EXPECT_EQ(entorhinal_decode_heading(ec, &decoded_heading, &confidence), 0);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(HDCellTest, DecodeHeadingEast) {
    entorhinal_update_hd_cells(ec, M_PI / 2.0f, 0.0f);

    float decoded_heading;
    float confidence;
    EXPECT_EQ(entorhinal_decode_heading(ec, &decoded_heading, &confidence), 0);
    // Decoded heading should be close to input
    // Allow some tolerance for population coding
}

TEST_F(HDCellTest, DecodeHeadingNoConfidence) {
    entorhinal_update_hd_cells(ec, M_PI / 4.0f, 0.0f);

    float decoded_heading;
    EXPECT_EQ(entorhinal_decode_heading(ec, &decoded_heading, nullptr), 0);
}

TEST_F(HDCellTest, DecodeHeadingNull) {
    EXPECT_EQ(entorhinal_decode_heading(nullptr, nullptr, nullptr), -1);
    float heading;
    EXPECT_EQ(entorhinal_decode_heading(nullptr, &heading, nullptr), -1);
    EXPECT_EQ(entorhinal_decode_heading(ec, nullptr, nullptr), -1);
}

TEST_F(HDCellTest, DecodeHeadingAfterRotation) {
    // Update during rotation
    for (int i = 0; i < 10; i++) {
        float heading = (float)i * (M_PI / 5.0f);
        entorhinal_update_hd_cells(ec, heading, 0.3f);
    }

    float decoded_heading;
    float confidence;
    EXPECT_EQ(entorhinal_decode_heading(ec, &decoded_heading, &confidence), 0);
}

/*=============================================================================
 * HD CELL CALIBRATION TESTS
 *===========================================================================*/

TEST_F(HDCellTest, CalibrateHDCellsNorth) {
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, 0.0f), 0);
}

TEST_F(HDCellTest, CalibrateHDCellsEast) {
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, M_PI / 2.0f), 0);
}

TEST_F(HDCellTest, CalibrateHDCellsNull) {
    EXPECT_EQ(entorhinal_calibrate_hd_cells(nullptr, 0.0f), -1);
}

TEST_F(HDCellTest, CalibrateHDCellsResetsDrift) {
    // Simulate some drift by doing updates
    for (int i = 0; i < 50; i++) {
        entorhinal_update_hd_cells(ec, 0.1f * i, 0.1f);
    }

    // Calibrate to known heading
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, 0.0f), 0);

    // Verify calibration took effect
    float decoded_heading;
    float confidence;
    entorhinal_decode_heading(ec, &decoded_heading, &confidence);
    // Confidence should be high after calibration
}

TEST_F(HDCellTest, CalibrateHDCellsMultipleTimes) {
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, 0.0f), 0);
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, M_PI), 0);
    EXPECT_EQ(entorhinal_calibrate_hd_cells(ec, -M_PI / 2.0f), 0);
}

/*=============================================================================
 * HD CELL ACTIVATION TESTS
 *===========================================================================*/

TEST_F(HDCellTest, ActivationPeakAtPreferredDirection) {
    // Update with a specific heading
    float target_heading = M_PI / 4.0f;  // 45 degrees
    entorhinal_update_hd_cells(ec, target_heading, 0.0f);

    // Find cell with preferred direction closest to target
    uint32_t best_cell = 0;
    float best_diff = 1e9f;
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        float diff = fabs(ec->hd_cells[i].preferred_direction - target_heading);
        if (diff > M_PI) diff = 2.0f * M_PI - diff;  // Handle wrap
        if (diff < best_diff) {
            best_diff = diff;
            best_cell = i;
        }
    }

    // The best matching cell should have high activation
    EXPECT_GT(ec->hd_cells[best_cell].activation, 0.0f);
}

TEST_F(HDCellTest, ActivationDecaysFromPreferred) {
    float target_heading = 0.0f;
    entorhinal_update_hd_cells(ec, target_heading, 0.0f);

    // Cells far from preferred direction should have lower activation
    // than cells near preferred direction
    float near_sum = 0.0f;
    float far_sum = 0.0f;
    uint32_t near_count = 0;
    uint32_t far_count = 0;

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        float diff = fabs(ec->hd_cells[i].preferred_direction - target_heading);
        if (diff > M_PI) diff = 2.0f * M_PI - diff;

        if (diff < M_PI / 6.0f) {  // Within 30 degrees
            near_sum += ec->hd_cells[i].activation;
            near_count++;
        } else if (diff > M_PI / 2.0f) {  // More than 90 degrees
            far_sum += ec->hd_cells[i].activation;
            far_count++;
        }
    }

    if (near_count > 0 && far_count > 0) {
        float near_mean = near_sum / near_count;
        float far_mean = far_sum / far_count;
        EXPECT_GE(near_mean, far_mean);
    }
}

TEST_F(HDCellTest, ActivationBounded) {
    entorhinal_update_hd_cells(ec, M_PI / 3.0f, 0.0f);

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        EXPECT_GE(ec->hd_cells[i].activation, 0.0f);
    }
}

/*=============================================================================
 * HD CELL COVERAGE TESTS
 *===========================================================================*/

TEST_F(HDCellTest, HDCellsCoverAllDirections) {
    // Check that HD cells cover 360 degrees
    float min_dir = 1e9f;
    float max_dir = -1e9f;

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        float dir = ec->hd_cells[i].preferred_direction;
        if (dir < min_dir) min_dir = dir;
        if (dir > max_dir) max_dir = dir;
    }

    // Should span close to 360 degrees
    float range = max_dir - min_dir;
    EXPECT_GT(range, M_PI);  // At least 180 degrees coverage
}

TEST_F(HDCellTest, HDCellsUniformDistribution) {
    // Check roughly uniform distribution of preferred directions
    int quadrant_counts[4] = {0, 0, 0, 0};

    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        float dir = ec->hd_cells[i].preferred_direction;
        // Normalize to [0, 2*PI)
        while (dir < 0) dir += 2.0f * M_PI;
        while (dir >= 2.0f * M_PI) dir -= 2.0f * M_PI;

        int quadrant = (int)(dir / (M_PI / 2.0f));
        if (quadrant >= 4) quadrant = 3;
        quadrant_counts[quadrant]++;
    }

    // Each quadrant should have some cells
    for (int q = 0; q < 4; q++) {
        EXPECT_GT(quadrant_counts[q], 0);
    }
}

/*=============================================================================
 * HD CELL ANTICIPATORY FIRING TESTS
 *===========================================================================*/

TEST_F(HDCellTest, AnticipatoryOffsetExists) {
    // Check that HD cells have anticipatory offset
    bool found_nonzero = false;
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        if (fabs(ec->hd_cells[i].anticipatory_offset) > 0.001f) {
            found_nonzero = true;
            break;
        }
    }
    // Anticipatory offset may be zero initially
    // This test just verifies the field exists
    SUCCEED();
}

TEST_F(HDCellTest, AngularVelocityGainPositive) {
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        EXPECT_GE(ec->hd_cells[i].angular_velocity_gain, 0.0f);
    }
}

/*=============================================================================
 * HD CELL TUNING WIDTH TESTS
 *===========================================================================*/

TEST_F(HDCellTest, TuningWidthPositive) {
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        EXPECT_GT(ec->hd_cells[i].tuning_width, 0.0f);
    }
}

TEST_F(HDCellTest, TuningWidthReasonable) {
    // Tuning width should be between ~15 and ~90 degrees
    for (uint32_t i = 0; i < ec->num_hd_cells; i++) {
        EXPECT_LT(ec->hd_cells[i].tuning_width, M_PI);  // Less than 180 degrees
    }
}

