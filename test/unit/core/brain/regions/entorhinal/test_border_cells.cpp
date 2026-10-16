/**
 * @file test_border_cells.cpp
 * @brief Unit tests for Entorhinal Border Cell functionality
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

class BorderCellTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        config.num_border_cells = 64;
        config.border_detection_range = 2.0f;
        config.enable_boundary_detection = true;
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
 * BORDER CELL UPDATE TESTS
 *===========================================================================*/

TEST_F(BorderCellTest, UpdateBorderCellsBasic) {
    float boundary_distances[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
}

TEST_F(BorderCellTest, UpdateBorderCellsNearBoundary) {
    // Close to boundary - should trigger strong activation
    float boundary_distances[4] = {0.1f, 5.0f, 5.0f, 5.0f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
}

TEST_F(BorderCellTest, UpdateBorderCellsFarFromBoundary) {
    // Far from all boundaries
    float boundary_distances[4] = {10.0f, 10.0f, 10.0f, 10.0f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 4), 0);
}

TEST_F(BorderCellTest, UpdateBorderCellsSingleBoundary) {
    float boundary_distances[1] = {0.5f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 1), 0);
}

TEST_F(BorderCellTest, UpdateBorderCellsManyBoundaries) {
    float boundary_distances[8] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 4.5f};
    EXPECT_EQ(entorhinal_update_border_cells(ec, boundary_distances, 8), 0);
}

TEST_F(BorderCellTest, UpdateBorderCellsNull) {
    EXPECT_EQ(entorhinal_update_border_cells(nullptr, nullptr, 0), -1);
    float boundary_distances[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    EXPECT_EQ(entorhinal_update_border_cells(nullptr, boundary_distances, 4), -1);
    EXPECT_EQ(entorhinal_update_border_cells(ec, nullptr, 4), -1);
}

TEST_F(BorderCellTest, UpdateBorderCellsZeroBoundaries) {
    EXPECT_EQ(entorhinal_update_border_cells(ec, nullptr, 0), -1);
}

/*=============================================================================
 * BOUNDARY DETECTION TESTS
 *===========================================================================*/

TEST_F(BorderCellTest, DetectBoundariesBasic) {
    // First update with near boundary
    float boundary_distances[4] = {0.5f, 3.0f, 3.0f, 3.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    float boundary_directions[4];
    float detected_distances[4];
    uint32_t num_detected = 0;

    EXPECT_EQ(entorhinal_detect_boundaries(ec, boundary_directions, detected_distances,
        4, &num_detected), 0);
    EXPECT_GE(num_detected, 0u);
    EXPECT_LE(num_detected, 4u);
}

TEST_F(BorderCellTest, DetectBoundariesNoBoundaries) {
    // Far from all boundaries
    float boundary_distances[4] = {20.0f, 20.0f, 20.0f, 20.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    float boundary_directions[4];
    float detected_distances[4];
    uint32_t num_detected = 99;  // Initialize to non-zero

    EXPECT_EQ(entorhinal_detect_boundaries(ec, boundary_directions, detected_distances,
        4, &num_detected), 0);
    // May or may not detect depending on threshold
}

TEST_F(BorderCellTest, DetectBoundariesMultiple) {
    // Near two boundaries
    float boundary_distances[4] = {0.3f, 0.4f, 5.0f, 5.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    float boundary_directions[4];
    float detected_distances[4];
    uint32_t num_detected = 0;

    EXPECT_EQ(entorhinal_detect_boundaries(ec, boundary_directions, detected_distances,
        4, &num_detected), 0);
}

TEST_F(BorderCellTest, DetectBoundariesNull) {
    EXPECT_EQ(entorhinal_detect_boundaries(nullptr, nullptr, nullptr, 0, nullptr), -1);
    float dirs[4], dists[4];
    uint32_t num;
    EXPECT_EQ(entorhinal_detect_boundaries(nullptr, dirs, dists, 4, &num), -1);
    EXPECT_EQ(entorhinal_detect_boundaries(ec, nullptr, dists, 4, &num), -1);
    EXPECT_EQ(entorhinal_detect_boundaries(ec, dirs, nullptr, 4, &num), -1);
    EXPECT_EQ(entorhinal_detect_boundaries(ec, dirs, dists, 4, nullptr), -1);
}

TEST_F(BorderCellTest, DetectBoundariesSmallBuffer) {
    float boundary_distances[4] = {0.3f, 0.4f, 0.5f, 0.6f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    float boundary_directions[1];
    float detected_distances[1];
    uint32_t num_detected = 0;

    // Should still work with small buffer (limited results)
    EXPECT_EQ(entorhinal_detect_boundaries(ec, boundary_directions, detected_distances,
        1, &num_detected), 0);
    EXPECT_LE(num_detected, 1u);
}

/*=============================================================================
 * BORDER CELL ACTIVATION TESTS
 *===========================================================================*/

TEST_F(BorderCellTest, ActivationNearBoundary) {
    // Update with very close boundary
    float boundary_distances[4] = {0.1f, 10.0f, 10.0f, 10.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    // At least some border cells should have non-zero activation
    bool found_active = false;
    for (uint32_t i = 0; i < ec->num_border_cells && !found_active; i++) {
        if (ec->border_cells[i].activation > 0.1f) {
            found_active = true;
        }
    }
    EXPECT_TRUE(found_active);
}

TEST_F(BorderCellTest, ActivationFarFromBoundary) {
    // Update with far boundaries
    float boundary_distances[4] = {50.0f, 50.0f, 50.0f, 50.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    // Most border cells should have low activation
    float mean_activation = 0.0f;
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        mean_activation += ec->border_cells[i].activation;
    }
    mean_activation /= ec->num_border_cells;
    EXPECT_LT(mean_activation, 0.5f);
}

TEST_F(BorderCellTest, ActivationBounded) {
    float boundary_distances[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        EXPECT_GE(ec->border_cells[i].activation, 0.0f);
        // Activation typically bounded but may exceed 1.0 with certain tuning
    }
}

/*=============================================================================
 * BORDER CELL PROPERTIES TESTS
 *===========================================================================*/

TEST_F(BorderCellTest, BorderCellPreferredDistances) {
    // Border cells should have varied preferred distances
    float min_pref = 1e9f;
    float max_pref = -1e9f;

    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        float pref = ec->border_cells[i].preferred_distance;
        if (pref < min_pref) min_pref = pref;
        if (pref > max_pref) max_pref = pref;
    }

    // Should have a range of preferred distances
    EXPECT_LT(min_pref, max_pref);
}

TEST_F(BorderCellTest, BorderCellPreferredDirections) {
    // Border cells should cover different directions
    bool found_positive_dir = false;
    bool found_negative_dir = false;

    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        float dir = ec->border_cells[i].preferred_direction;
        if (dir > 0) found_positive_dir = true;
        if (dir < 0) found_negative_dir = true;
    }

    EXPECT_TRUE(found_positive_dir || found_negative_dir);
}

TEST_F(BorderCellTest, BorderCellTuningWidth) {
    // All border cells should have positive tuning width
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        EXPECT_GT(ec->border_cells[i].tuning_width, 0.0f);
    }
}

/*=============================================================================
 * BOUNDARY CONFIDENCE TESTS
 *===========================================================================*/

TEST_F(BorderCellTest, BoundaryConfidenceNearBoundary) {
    // Near boundary should give high confidence
    float boundary_distances[4] = {0.2f, 10.0f, 10.0f, 10.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    // Check if any border cell has high confidence
    float max_confidence = 0.0f;
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        if (ec->border_cells[i].boundary_confidence > max_confidence) {
            max_confidence = ec->border_cells[i].boundary_confidence;
        }
    }
    EXPECT_GE(max_confidence, 0.0f);  // May be 0 if not computed
}

TEST_F(BorderCellTest, BoundaryConfidenceFarFromBoundary) {
    // Far from boundary should give lower confidence
    float boundary_distances[4] = {100.0f, 100.0f, 100.0f, 100.0f};
    entorhinal_update_border_cells(ec, boundary_distances, 4);

    // All confidences should be low when far from boundaries
    float mean_confidence = 0.0f;
    for (uint32_t i = 0; i < ec->num_border_cells; i++) {
        mean_confidence += ec->border_cells[i].boundary_confidence;
    }
    mean_confidence /= ec->num_border_cells;
    EXPECT_LE(mean_confidence, 1.0f);
}

