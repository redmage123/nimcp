/**
 * @file test_orientation_columns.cpp
 * @brief Comprehensive unit tests for NIMCP orientation columns module
 *
 * Test Coverage:
 * - Orientation Column Lifecycle: Create/destroy columns
 * - Gabor Filters: Filter application, parameter configuration
 * - Energy Model: Complex cell response computation
 * - Tuning Curves: Von Mises, tuning width, orientation selectivity
 * - Hypercolumn Lifecycle: Create/destroy hypercolumns
 * - Orientation Processing: Image patch processing
 * - Normalization: Divisive normalization
 * - Inhibition: Cross-orientation suppression
 * - Selectivity Metrics: OSI, circular variance
 * - Pinwheel Organization: Spatial orientation maps
 * - Batch Processing: Full image orientation maps
 * - Edge Cases: NULL parameters, boundary conditions
 *
 * Mathematical Models Tested:
 * - Gabor filter: G(x,y,θ,λ,ψ,σ,γ) = exp(-(x'² + γ²y'²)/(2σ²)) × cos(2π×x'/λ + ψ)
 * - Energy model: E = √(G_even² + G_odd²)
 * - Tuning curve: R(θ) = R_base + R_max × exp(κ × cos(2(θ - θ_pref)))
 * - OSI: (R_pref - R_orth) / (R_pref + R_orth)
 * - Circular variance: 1 - |Σ(R_i × e^(2iθ_i))| / Σ(R_i)
 *
 * @version 1.0.0
 * @date 2025-01-25
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include <cmath>
#include <vector>
#include <algorithm>

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OrientationColumnsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test data
        test_orientation = 45.0f;
        test_tuning_width = 30.0f;
        test_spatial_freq = 2.0f;
        test_num_orientations = 8;

        // Create test image patches
        CreateTestPatches();
    }

    void TearDown() override {
        // Cleanup happens automatically
    }

    // Helper function to create test image patches
    void CreateTestPatches() {
        patch_size = 32;
        uniform_patch.resize(patch_size * patch_size, 0.5f);

        // Vertical edge (0 degrees)
        vertical_edge.resize(patch_size * patch_size);
        for (uint32_t y = 0; y < patch_size; y++) {
            for (uint32_t x = 0; x < patch_size; x++) {
                vertical_edge[y * patch_size + x] = (x < patch_size / 2) ? 0.0f : 1.0f;
            }
        }

        // Horizontal edge (90 degrees)
        horizontal_edge.resize(patch_size * patch_size);
        for (uint32_t y = 0; y < patch_size; y++) {
            for (uint32_t x = 0; x < patch_size; x++) {
                horizontal_edge[y * patch_size + x] = (y < patch_size / 2) ? 0.0f : 1.0f;
            }
        }

        // Diagonal edge (45 degrees)
        diagonal_edge.resize(patch_size * patch_size);
        for (uint32_t y = 0; y < patch_size; y++) {
            for (uint32_t x = 0; x < patch_size; x++) {
                diagonal_edge[y * patch_size + x] = (x + y < patch_size) ? 0.0f : 1.0f;
            }
        }
    }

    // Test data
    float test_orientation;
    float test_tuning_width;
    float test_spatial_freq;
    uint32_t test_num_orientations;
    uint32_t patch_size;

    std::vector<float> uniform_patch;
    std::vector<float> vertical_edge;
    std::vector<float> horizontal_edge;
    std::vector<float> diagonal_edge;
};

// ============================================================================
// ORIENTATION COLUMN LIFECYCLE TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, ColumnCreate_ValidParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation,
        test_tuning_width,
        test_spatial_freq
    );

    ASSERT_NE(col, nullptr);
    EXPECT_FLOAT_EQ(col->preferred_orientation, test_orientation);
    EXPECT_FLOAT_EQ(col->tuning_width, test_tuning_width);
    EXPECT_FLOAT_EQ(col->spatial_frequency, test_spatial_freq);
    EXPECT_FLOAT_EQ(col->activation, 0.0f);
    EXPECT_NE(col->mutex, nullptr);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ColumnCreate_VariousOrientations) {
    float orientations[] = {0.0f, 45.0f, 90.0f, 135.0f, 179.9f};

    for (float orient : orientations) {
        orientation_column_t* col = orientation_column_create(
            orient, test_tuning_width, test_spatial_freq
        );

        ASSERT_NE(col, nullptr);
        EXPECT_GE(col->preferred_orientation, 0.0f);
        EXPECT_LT(col->preferred_orientation, 180.0f);

        orientation_column_destroy(col);
    }
}

TEST_F(OrientationColumnsTest, ColumnCreate_OrientationNormalization) {
    // Test that orientations are normalized to [0, 180) range
    orientation_column_t* col1 = orientation_column_create(
        200.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col1, nullptr);
    EXPECT_GE(col1->preferred_orientation, 0.0f);
    EXPECT_LT(col1->preferred_orientation, 180.0f);
    orientation_column_destroy(col1);

    orientation_column_t* col2 = orientation_column_create(
        -45.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col2, nullptr);
    EXPECT_GE(col2->preferred_orientation, 0.0f);
    EXPECT_LT(col2->preferred_orientation, 180.0f);
    orientation_column_destroy(col2);
}

TEST_F(OrientationColumnsTest, ColumnCreate_InvalidTuningWidth) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, 0.0f, test_spatial_freq
    );
    EXPECT_EQ(col, nullptr);

    col = orientation_column_create(
        test_orientation, -10.0f, test_spatial_freq
    );
    EXPECT_EQ(col, nullptr);
}

TEST_F(OrientationColumnsTest, ColumnCreate_InvalidSpatialFrequency) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, 0.0f
    );
    EXPECT_EQ(col, nullptr);

    col = orientation_column_create(
        test_orientation, test_tuning_width, -1.0f
    );
    EXPECT_EQ(col, nullptr);
}

TEST_F(OrientationColumnsTest, ColumnDestroy_ValidColumn) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Should not crash
    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ColumnDestroy_NullColumn) {
    // Should not crash
    orientation_column_destroy(nullptr);
}

// ============================================================================
// GABOR FILTER TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, SetGabor_ValidParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    gabor_params_t params;
    params.sigma_x = 2.0f;
    params.sigma_y = 2.0f;
    params.lambda = 4.0f;
    params.gamma = 0.5f;
    params.psi = 0.0f;
    params.theta = M_PI / 4.0f;

    bool result = orientation_column_set_gabor(col, &params);
    EXPECT_TRUE(result);

    EXPECT_FLOAT_EQ(col->gabor_params.sigma_x, params.sigma_x);
    EXPECT_FLOAT_EQ(col->gabor_params.sigma_y, params.sigma_y);
    EXPECT_FLOAT_EQ(col->gabor_params.lambda, params.lambda);
    EXPECT_FLOAT_EQ(col->gabor_params.gamma, params.gamma);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, SetGabor_NullColumn) {
    gabor_params_t params;
    params.sigma_x = 2.0f;
    params.sigma_y = 2.0f;
    params.lambda = 4.0f;
    params.gamma = 0.5f;
    params.psi = 0.0f;
    params.theta = 0.0f;

    bool result = orientation_column_set_gabor(nullptr, &params);
    EXPECT_FALSE(result);
}

TEST_F(OrientationColumnsTest, SetGabor_NullParams) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    bool result = orientation_column_set_gabor(col, nullptr);
    EXPECT_FALSE(result);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ApplyGabor_UniformPatch) {
    orientation_column_t* col = orientation_column_create(
        0.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    float response = orientation_column_apply_gabor(
        col, uniform_patch.data(), patch_size, patch_size
    );

    // Uniform patch should give near-zero response (no edges)
    EXPECT_NEAR(response, 0.0f, 0.5f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ApplyGabor_VerticalEdge) {
    // Column tuned to vertical (0 degrees)
    orientation_column_t* col = orientation_column_create(
        0.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    float response = orientation_column_apply_gabor(
        col, vertical_edge.data(), patch_size, patch_size
    );

    // Should give non-zero response to vertical edge
    EXPECT_NE(response, 0.0f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ApplyGabor_NullParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // NULL column
    float response = orientation_column_apply_gabor(
        nullptr, uniform_patch.data(), patch_size, patch_size
    );
    EXPECT_FLOAT_EQ(response, 0.0f);

    // NULL image
    response = orientation_column_apply_gabor(
        col, nullptr, patch_size, patch_size
    );
    EXPECT_FLOAT_EQ(response, 0.0f);

    // Zero dimensions
    response = orientation_column_apply_gabor(
        col, uniform_patch.data(), 0, patch_size
    );
    EXPECT_FLOAT_EQ(response, 0.0f);

    orientation_column_destroy(col);
}

// ============================================================================
// ENERGY MODEL TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, ComputeEnergy_UniformPatch) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    float energy = orientation_column_compute_energy(
        col, uniform_patch.data(), patch_size, patch_size
    );

    // Uniform patch should give near-zero energy
    EXPECT_NEAR(energy, 0.0f, 0.5f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ComputeEnergy_VerticalEdge) {
    orientation_column_t* col = orientation_column_create(
        0.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    float energy = orientation_column_compute_energy(
        col, vertical_edge.data(), patch_size, patch_size
    );

    // Should give positive energy for edge
    EXPECT_GT(energy, 0.0f);

    // Energy should be stored in activation
    EXPECT_FLOAT_EQ(col->activation, energy);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ComputeEnergy_PhaseInvariance) {
    // Energy model should be phase-invariant
    orientation_column_t* col = orientation_column_create(
        45.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    float energy = orientation_column_compute_energy(
        col, diagonal_edge.data(), patch_size, patch_size
    );

    // Energy should always be non-negative
    EXPECT_GE(energy, 0.0f);

    // Energy = sqrt(even^2 + odd^2), so it should be >= max(|even|, |odd|)
    float even_response = orientation_column_apply_gabor(
        col, diagonal_edge.data(), patch_size, patch_size
    );

    EXPECT_GE(energy, fabs(even_response));

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, ComputeEnergy_NullParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // NULL column
    float energy = orientation_column_compute_energy(
        nullptr, uniform_patch.data(), patch_size, patch_size
    );
    EXPECT_FLOAT_EQ(energy, 0.0f);

    // NULL image
    energy = orientation_column_compute_energy(
        col, nullptr, patch_size, patch_size
    );
    EXPECT_FLOAT_EQ(energy, 0.0f);

    orientation_column_destroy(col);
}

// ============================================================================
// TUNING CURVE TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, GetResponse_PreferredOrientation) {
    orientation_column_t* col = orientation_column_create(
        45.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Response at preferred orientation should be maximum
    float response = orientation_column_get_response(col, 45.0f);
    EXPECT_GT(response, 0.0f);

    // Response at preferred should be greater than at other orientations
    float response_90 = orientation_column_get_response(col, 90.0f);
    EXPECT_GT(response, response_90);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, GetResponse_VonMisesTuning) {
    orientation_column_t* col = orientation_column_create(
        90.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Test von Mises tuning curve shape
    float resp_pref = orientation_column_get_response(col, 90.0f);
    float resp_near = orientation_column_get_response(col, 100.0f);
    float resp_far = orientation_column_get_response(col, 135.0f);
    float resp_orth = orientation_column_get_response(col, 0.0f);

    // Should decrease with angular distance
    EXPECT_GT(resp_pref, resp_near);
    EXPECT_GT(resp_near, resp_far);

    // Orthogonal should be minimal
    EXPECT_GT(resp_pref, resp_orth);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, GetResponse_180DegreeSymmetry) {
    orientation_column_t* col = orientation_column_create(
        30.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Orientations have 180-degree periodicity
    // Response at θ should equal response at θ+180
    float resp_30 = orientation_column_get_response(col, 30.0f);
    float resp_210 = orientation_column_get_response(col, 210.0f);

    EXPECT_NEAR(resp_30, resp_210, 0.01f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, GetResponse_NullColumn) {
    float response = orientation_column_get_response(nullptr, 45.0f);
    EXPECT_FLOAT_EQ(response, 0.0f);
}

TEST_F(OrientationColumnsTest, GetTuningCurve_ValidParameters) {
    orientation_column_t* col = orientation_column_create(
        60.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    uint32_t num_points = 36;
    std::vector<float> orientations(num_points);
    std::vector<float> responses(num_points);

    bool result = orientation_column_get_tuning_curve(
        col, orientations.data(), responses.data(), num_points
    );

    EXPECT_TRUE(result);

    // Verify orientations are evenly spaced
    float expected_step = 180.0f / num_points;
    for (uint32_t i = 0; i < num_points; i++) {
        EXPECT_NEAR(orientations[i], i * expected_step, 0.01f);
    }

    // Find peak response (should be near preferred orientation)
    auto max_it = std::max_element(responses.begin(), responses.end());
    size_t max_idx = std::distance(responses.begin(), max_it);
    float peak_orientation = orientations[max_idx];

    // Peak should be close to preferred (60 degrees)
    EXPECT_NEAR(peak_orientation, 60.0f, expected_step * 2);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, GetTuningCurve_NullParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    uint32_t num_points = 10;
    std::vector<float> orientations(num_points);
    std::vector<float> responses(num_points);

    // NULL column
    bool result = orientation_column_get_tuning_curve(
        nullptr, orientations.data(), responses.data(), num_points
    );
    EXPECT_FALSE(result);

    // NULL orientations array
    result = orientation_column_get_tuning_curve(
        col, nullptr, responses.data(), num_points
    );
    EXPECT_FALSE(result);

    // NULL responses array
    result = orientation_column_get_tuning_curve(
        col, orientations.data(), nullptr, num_points
    );
    EXPECT_FALSE(result);

    // Zero points
    result = orientation_column_get_tuning_curve(
        col, orientations.data(), responses.data(), 0
    );
    EXPECT_FALSE(result);

    orientation_column_destroy(col);
}

// ============================================================================
// COLUMN STATISTICS TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, GetStats_ValidColumn) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    orientation_stats_t stats;
    bool result = orientation_column_get_stats(col, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.tuning_sharpness, 0.0f);
    EXPECT_GT(stats.total_activations, 0);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, GetStats_NullParameters) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    orientation_stats_t stats;

    // NULL column
    bool result = orientation_column_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);

    // NULL stats
    result = orientation_column_get_stats(col, nullptr);
    EXPECT_FALSE(result);

    orientation_column_destroy(col);
}

// ============================================================================
// HYPERCOLUMN LIFECYCLE TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, HypercolumnCreate_ValidParameters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, test_num_orientations);
    EXPECT_NE(hcol->columns, nullptr);
    EXPECT_NE(hcol->mutex, nullptr);

    // Verify columns are evenly spaced
    float expected_step = 180.0f / test_num_orientations;
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        float expected_orient = i * expected_step;
        EXPECT_NEAR(hcol->columns[i].preferred_orientation, expected_orient, 0.01f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, HypercolumnCreate_VariousOrientationCounts) {
    uint32_t counts[] = {4, 8, 16, 32};

    for (uint32_t count : counts) {
        orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
            count, test_spatial_freq, test_tuning_width
        );

        ASSERT_NE(hcol, nullptr);
        EXPECT_EQ(hcol->num_orientations, count);

        orientation_hypercolumn_destroy(hcol);
    }
}

TEST_F(OrientationColumnsTest, HypercolumnCreate_InvalidOrientationCount) {
    // Zero orientations
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        0, test_spatial_freq, test_tuning_width
    );
    EXPECT_EQ(hcol, nullptr);

    // Exceeds maximum
    hcol = orientation_hypercolumn_create(
        ORIENTATION_MAX_ORIENTATIONS + 1, test_spatial_freq, test_tuning_width
    );
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(OrientationColumnsTest, HypercolumnCreate_InvalidSpatialFrequency) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, 0.0f, test_tuning_width
    );
    EXPECT_EQ(hcol, nullptr);

    hcol = orientation_hypercolumn_create(
        test_num_orientations, -1.0f, test_tuning_width
    );
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(OrientationColumnsTest, HypercolumnCreate_InvalidTuningWidth) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, 0.0f
    );
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(OrientationColumnsTest, HypercolumnDestroy_ValidHypercolumn) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Should not crash
    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, HypercolumnDestroy_NullHypercolumn) {
    // Should not crash
    orientation_hypercolumn_destroy(nullptr);
}

// ============================================================================
// ORIENTATION PROCESSING TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, HypercolumnProcess_UniformPatch) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    bool result = orientation_hypercolumn_process(
        hcol, uniform_patch.data(), patch_size, patch_size
    );

    EXPECT_TRUE(result);

    // All activations should be low for uniform patch
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        EXPECT_NEAR(hcol->columns[i].activation, 0.0f, 1.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, HypercolumnProcess_VerticalEdge) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    bool result = orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    EXPECT_TRUE(result);

    // Column tuned to 0 degrees (vertical) should have highest activation
    float max_activation = 0.0f;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < test_num_orientations; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            max_idx = i;
        }
    }

    // Dominant orientation should be close to 0 degrees
    EXPECT_NEAR(hcol->columns[max_idx].preferred_orientation, 0.0f, 25.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, HypercolumnProcess_HorizontalEdge) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    bool result = orientation_hypercolumn_process(
        hcol, horizontal_edge.data(), patch_size, patch_size
    );

    EXPECT_TRUE(result);

    // Column tuned to 90 degrees should have high activation
    float max_activation = 0.0f;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < test_num_orientations; i++) {
        if (hcol->columns[i].activation > max_activation) {
            max_activation = hcol->columns[i].activation;
            max_idx = i;
        }
    }

    // Dominant orientation should be close to 90 degrees
    EXPECT_NEAR(hcol->columns[max_idx].preferred_orientation, 90.0f, 25.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, HypercolumnProcess_NullParameters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // NULL hypercolumn
    bool result = orientation_hypercolumn_process(
        nullptr, uniform_patch.data(), patch_size, patch_size
    );
    EXPECT_FALSE(result);

    // NULL image
    result = orientation_hypercolumn_process(
        hcol, nullptr, patch_size, patch_size
    );
    EXPECT_FALSE(result);

    // Zero dimensions
    result = orientation_hypercolumn_process(
        hcol, uniform_patch.data(), 0, patch_size
    );
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// DOMINANT ORIENTATION TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, GetDominant_AfterProcessing) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    float dominant = orientation_hypercolumn_get_dominant(hcol);

    EXPECT_GE(dominant, 0.0f);
    EXPECT_LT(dominant, 180.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, GetDominant_NullHypercolumn) {
    float dominant = orientation_hypercolumn_get_dominant(nullptr);
    EXPECT_FLOAT_EQ(dominant, -1.0f);
}

// ============================================================================
// ORIENTATION DISTRIBUTION TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, GetDistribution_ValidParameters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, diagonal_edge.data(), patch_size, patch_size
    );

    std::vector<float> orientations(test_num_orientations);
    std::vector<float> responses(test_num_orientations);
    uint32_t num_orientations;

    bool result = orientation_hypercolumn_get_distribution(
        hcol, orientations.data(), responses.data(), &num_orientations
    );

    EXPECT_TRUE(result);
    EXPECT_EQ(num_orientations, test_num_orientations);

    // Verify orientations are in range
    for (uint32_t i = 0; i < num_orientations; i++) {
        EXPECT_GE(orientations[i], 0.0f);
        EXPECT_LT(orientations[i], 180.0f);
        EXPECT_GE(responses[i], 0.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, GetDistribution_NullParameters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    std::vector<float> orientations(test_num_orientations);
    std::vector<float> responses(test_num_orientations);
    uint32_t num_orientations;

    // NULL hypercolumn
    bool result = orientation_hypercolumn_get_distribution(
        nullptr, orientations.data(), responses.data(), &num_orientations
    );
    EXPECT_FALSE(result);

    // NULL orientations
    result = orientation_hypercolumn_get_distribution(
        hcol, nullptr, responses.data(), &num_orientations
    );
    EXPECT_FALSE(result);

    // NULL responses
    result = orientation_hypercolumn_get_distribution(
        hcol, orientations.data(), nullptr, &num_orientations
    );
    EXPECT_FALSE(result);

    // NULL output count
    result = orientation_hypercolumn_get_distribution(
        hcol, orientations.data(), responses.data(), nullptr
    );
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// NORMALIZATION TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, Normalize_DivisiveNormalization) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set up some activations
    orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    // Store original activations
    std::vector<float> original_activations(test_num_orientations);
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        original_activations[i] = hcol->columns[i].activation;
    }

    // Apply normalization
    bool result = orientation_hypercolumn_normalize(hcol);
    EXPECT_TRUE(result);

    // Compute total normalized activity
    float total_normalized = 0.0f;
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        total_normalized += hcol->columns[i].activation;
    }

    // After divisive normalization, total should be less than before
    float total_original = 0.0f;
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        total_original += original_activations[i];
    }

    EXPECT_LE(total_normalized, total_original + 0.01f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, Normalize_PreservesRelativeActivations) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, diagonal_edge.data(), patch_size, patch_size
    );

    // Find max activation before normalization
    uint32_t max_idx_before = 0;
    float max_val_before = hcol->columns[0].activation;
    for (uint32_t i = 1; i < test_num_orientations; i++) {
        if (hcol->columns[i].activation > max_val_before) {
            max_val_before = hcol->columns[i].activation;
            max_idx_before = i;
        }
    }

    orientation_hypercolumn_normalize(hcol);

    // Find max activation after normalization
    uint32_t max_idx_after = 0;
    float max_val_after = hcol->columns[0].activation;
    for (uint32_t i = 1; i < test_num_orientations; i++) {
        if (hcol->columns[i].activation > max_val_after) {
            max_val_after = hcol->columns[i].activation;
            max_idx_after = i;
        }
    }

    // Same column should have max activation
    EXPECT_EQ(max_idx_before, max_idx_after);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, Normalize_NullHypercolumn) {
    bool result = orientation_hypercolumn_normalize(nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// INHIBITION TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, ApplyInhibition_ReducesActivations) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    // Store original activations
    std::vector<float> original_activations(test_num_orientations);
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        original_activations[i] = hcol->columns[i].activation;
    }

    // Apply inhibition
    float inhibition_strength = 0.3f;
    bool result = orientation_hypercolumn_apply_inhibition(hcol, inhibition_strength);
    EXPECT_TRUE(result);

    // Most activations should be reduced (except possibly the winner)
    uint32_t num_reduced = 0;
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        if (hcol->columns[i].activation < original_activations[i]) {
            num_reduced++;
        }
    }

    EXPECT_GT(num_reduced, 0);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ApplyInhibition_NonNegativeActivations) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, horizontal_edge.data(), patch_size, patch_size
    );

    // Apply strong inhibition
    bool result = orientation_hypercolumn_apply_inhibition(hcol, 0.8f);
    EXPECT_TRUE(result);

    // All activations should remain non-negative
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        EXPECT_GE(hcol->columns[i].activation, 0.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ApplyInhibition_InvalidStrength) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Negative strength
    bool result = orientation_hypercolumn_apply_inhibition(hcol, -0.1f);
    EXPECT_FALSE(result);

    // Strength > 1
    result = orientation_hypercolumn_apply_inhibition(hcol, 1.5f);
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ApplyInhibition_NullHypercolumn) {
    bool result = orientation_hypercolumn_apply_inhibition(nullptr, 0.5f);
    EXPECT_FALSE(result);
}

// ============================================================================
// SELECTIVITY METRICS TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, ComputeOSI_UniformActivation) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set all activations equal
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 1.0f;
    }

    float osi = orientation_hypercolumn_compute_osi(hcol);

    // OSI should be near 0 for uniform activations (not selective)
    EXPECT_GE(osi, -0.1f);
    EXPECT_LE(osi, 0.1f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeOSI_SelectiveActivation) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set one orientation highly active, others low
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 0.1f;
    }
    hcol->columns[0].activation = 1.0f;

    float osi = orientation_hypercolumn_compute_osi(hcol);

    // OSI should be positive for selective activation
    EXPECT_GT(osi, 0.3f);
    EXPECT_LE(osi, 1.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeOSI_AfterProcessing) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    float osi = orientation_hypercolumn_compute_osi(hcol);

    // OSI should be in valid range
    EXPECT_GE(osi, -1.0f);
    EXPECT_LE(osi, 1.0f);

    // Edge should produce some selectivity
    EXPECT_GE(osi, 0.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeOSI_NullHypercolumn) {
    float osi = orientation_hypercolumn_compute_osi(nullptr);
    EXPECT_FLOAT_EQ(osi, -1.0f);
}

TEST_F(OrientationColumnsTest, ComputeCircularVariance_UniformActivation) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set all activations equal
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 1.0f;
    }

    float cv = orientation_hypercolumn_compute_circular_variance(hcol);

    // High circular variance for uniform (broad tuning)
    EXPECT_GE(cv, 0.5f);
    EXPECT_LE(cv, 1.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeCircularVariance_SelectiveActivation) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set one orientation highly active, others zero
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 0.0f;
    }
    hcol->columns[0].activation = 1.0f;

    float cv = orientation_hypercolumn_compute_circular_variance(hcol);

    // Low circular variance for selective (sharp tuning)
    EXPECT_GE(cv, 0.0f);
    EXPECT_LT(cv, 0.3f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeCircularVariance_ValidRange) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, diagonal_edge.data(), patch_size, patch_size
    );

    float cv = orientation_hypercolumn_compute_circular_variance(hcol);

    // Should be in [0, 1] range
    EXPECT_GE(cv, 0.0f);
    EXPECT_LE(cv, 1.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ComputeCircularVariance_NullHypercolumn) {
    float cv = orientation_hypercolumn_compute_circular_variance(nullptr);
    EXPECT_FLOAT_EQ(cv, -1.0f);
}

// ============================================================================
// PINWHEEL ORGANIZATION TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, SetPinwheel_ValidCoordinates) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    float center_x = 100.0f;
    float center_y = 150.0f;

    bool result = orientation_hypercolumn_set_pinwheel(hcol, center_x, center_y);
    EXPECT_TRUE(result);

    EXPECT_FLOAT_EQ(hcol->pinwheel_center_x, center_x);
    EXPECT_FLOAT_EQ(hcol->pinwheel_center_y, center_y);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, SetPinwheel_NullHypercolumn) {
    bool result = orientation_hypercolumn_set_pinwheel(nullptr, 0.0f, 0.0f);
    EXPECT_FALSE(result);
}

TEST_F(OrientationColumnsTest, GetLocalOrientation_RadialPattern) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set pinwheel center at origin
    orientation_hypercolumn_set_pinwheel(hcol, 0.0f, 0.0f);

    // Test points at different angles
    // Point to the right (should be 0 or 180 degrees)
    float orient_right = orientation_hypercolumn_get_local_orientation(hcol, 10.0f, 0.0f);
    EXPECT_GE(orient_right, 0.0f);
    EXPECT_LT(orient_right, 180.0f);

    // Point above (should be around 90 degrees)
    float orient_up = orientation_hypercolumn_get_local_orientation(hcol, 0.0f, 10.0f);
    EXPECT_NEAR(orient_up, 90.0f, 5.0f);

    // Point to the left (should be 0 or 180 degrees, normalized)
    float orient_left = orientation_hypercolumn_get_local_orientation(hcol, -10.0f, 0.0f);
    EXPECT_GE(orient_left, 0.0f);
    EXPECT_LT(orient_left, 180.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, GetLocalOrientation_DifferentCenters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Test with different pinwheel centers
    orientation_hypercolumn_set_pinwheel(hcol, 50.0f, 50.0f);

    float orient1 = orientation_hypercolumn_get_local_orientation(hcol, 60.0f, 50.0f);
    EXPECT_GE(orient1, 0.0f);
    EXPECT_LT(orient1, 180.0f);

    // Change center
    orientation_hypercolumn_set_pinwheel(hcol, 0.0f, 0.0f);

    float orient2 = orientation_hypercolumn_get_local_orientation(hcol, 60.0f, 50.0f);
    EXPECT_GE(orient2, 0.0f);
    EXPECT_LT(orient2, 180.0f);

    // Orientations should differ with different centers
    EXPECT_NE(orient1, orient2);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, GetLocalOrientation_NullHypercolumn) {
    float orient = orientation_hypercolumn_get_local_orientation(nullptr, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(orient, -1.0f);
}

// ============================================================================
// HYPERCOLUMN STATISTICS TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, GetHypercolumnStats_ValidHypercolumn) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );

    orientation_hypercolumn_stats_t stats;
    bool result = orientation_hypercolumn_get_stats(hcol, &stats);

    EXPECT_TRUE(result);

    // Verify stats are in valid ranges
    EXPECT_GE(stats.mean_osi, -1.0f);
    EXPECT_LE(stats.mean_osi, 1.0f);

    EXPECT_GE(stats.mean_circular_variance, 0.0f);
    EXPECT_LE(stats.mean_circular_variance, 1.0f);

    EXPECT_GE(stats.competition_strength, 0.0f);
    EXPECT_LE(stats.competition_strength, 1.0f);

    EXPECT_GE(stats.coverage_uniformity, 0.0f);
    EXPECT_LE(stats.coverage_uniformity, 1.0f);

    EXPECT_LE(stats.num_active_columns, test_num_orientations);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, GetHypercolumnStats_NullParameters) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    orientation_hypercolumn_stats_t stats;

    // NULL hypercolumn
    bool result = orientation_hypercolumn_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);

    // NULL stats
    result = orientation_hypercolumn_get_stats(hcol, nullptr);
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// BATCH PROCESSING TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, ProcessImage_SmallImage) {
    uint32_t img_width = 64;
    uint32_t img_height = 64;
    std::vector<float> image(img_width * img_height, 0.5f);
    std::vector<float> orientation_map(img_width * img_height);

    // Create hypercolumns
    uint32_t num_hcols = 4;
    std::vector<orientation_hypercolumn_t*> hypercolumns(num_hcols);
    for (uint32_t i = 0; i < num_hcols; i++) {
        hypercolumns[i] = orientation_hypercolumn_create(
            test_num_orientations, test_spatial_freq, test_tuning_width
        );
        ASSERT_NE(hypercolumns[i], nullptr);
    }

    bool result = orientation_process_image(
        image.data(), img_width, img_height,
        hypercolumns.data(), num_hcols,
        orientation_map.data()
    );

    EXPECT_TRUE(result);

    // Verify orientation map is populated
    for (uint32_t i = 0; i < img_width * img_height; i++) {
        EXPECT_GE(orientation_map[i], 0.0f);
        EXPECT_LT(orientation_map[i], 180.0f);
    }

    // Cleanup
    for (uint32_t i = 0; i < num_hcols; i++) {
        orientation_hypercolumn_destroy(hypercolumns[i]);
    }
}

TEST_F(OrientationColumnsTest, ProcessImage_VerticalEdgeImage) {
    uint32_t img_width = 64;
    uint32_t img_height = 64;
    std::vector<float> image(img_width * img_height);

    // Create vertical edge image
    for (uint32_t y = 0; y < img_height; y++) {
        for (uint32_t x = 0; x < img_width; x++) {
            image[y * img_width + x] = (x < img_width / 2) ? 0.0f : 1.0f;
        }
    }

    std::vector<float> orientation_map(img_width * img_height);

    // Create hypercolumns
    uint32_t num_hcols = 4;
    std::vector<orientation_hypercolumn_t*> hypercolumns(num_hcols);
    for (uint32_t i = 0; i < num_hcols; i++) {
        hypercolumns[i] = orientation_hypercolumn_create(
            test_num_orientations, test_spatial_freq, test_tuning_width
        );
        ASSERT_NE(hypercolumns[i], nullptr);
    }

    bool result = orientation_process_image(
        image.data(), img_width, img_height,
        hypercolumns.data(), num_hcols,
        orientation_map.data()
    );

    EXPECT_TRUE(result);

    // Most orientations should be close to 0 degrees (vertical)
    uint32_t vertical_count = 0;
    for (uint32_t i = 0; i < img_width * img_height; i++) {
        if (orientation_map[i] < 30.0f || orientation_map[i] > 150.0f) {
            vertical_count++;
        }
    }

    // At least some pixels should detect vertical orientation
    EXPECT_GT(vertical_count, 0);

    // Cleanup
    for (uint32_t i = 0; i < num_hcols; i++) {
        orientation_hypercolumn_destroy(hypercolumns[i]);
    }
}

TEST_F(OrientationColumnsTest, ProcessImage_NullParameters) {
    uint32_t img_width = 64;
    uint32_t img_height = 64;
    std::vector<float> image(img_width * img_height, 0.5f);
    std::vector<float> orientation_map(img_width * img_height);

    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // NULL image
    bool result = orientation_process_image(
        nullptr, img_width, img_height,
        &hcol, 1, orientation_map.data()
    );
    EXPECT_FALSE(result);

    // NULL hypercolumns
    result = orientation_process_image(
        image.data(), img_width, img_height,
        nullptr, 1, orientation_map.data()
    );
    EXPECT_FALSE(result);

    // NULL orientation map
    result = orientation_process_image(
        image.data(), img_width, img_height,
        &hcol, 1, nullptr
    );
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, ProcessImage_InvalidDimensions) {
    std::vector<float> image(64 * 64, 0.5f);
    std::vector<float> orientation_map(64 * 64);

    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Zero width
    bool result = orientation_process_image(
        image.data(), 0, 64,
        &hcol, 1, orientation_map.data()
    );
    EXPECT_FALSE(result);

    // Zero height
    result = orientation_process_image(
        image.data(), 64, 0,
        &hcol, 1, orientation_map.data()
    );
    EXPECT_FALSE(result);

    // Zero hypercolumns
    result = orientation_process_image(
        image.data(), 64, 64,
        &hcol, 0, orientation_map.data()
    );
    EXPECT_FALSE(result);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, EdgeCase_ZeroActivations) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Set all activations to zero
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 0.0f;
    }

    // OSI should handle zero activations
    float osi = orientation_hypercolumn_compute_osi(hcol);
    EXPECT_GE(osi, -1.0f);
    EXPECT_LE(osi, 1.0f);

    // Circular variance should handle zero activations
    float cv = orientation_hypercolumn_compute_circular_variance(hcol);
    EXPECT_GE(cv, -1.0f);
    EXPECT_LE(cv, 1.0f);

    // Normalization should handle zero activations
    bool result = orientation_hypercolumn_normalize(hcol);
    EXPECT_TRUE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, EdgeCase_VerySmallPatch) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Very small patch (1x1)
    float tiny_patch[] = {0.5f};
    float response = orientation_column_apply_gabor(col, tiny_patch, 1, 1);

    // Should handle gracefully
    EXPECT_GE(response, -1000.0f);
    EXPECT_LE(response, 1000.0f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, EdgeCase_LargeTuningWidth) {
    orientation_column_t* col = orientation_column_create(
        45.0f, 180.0f, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Very broad tuning should still produce valid responses
    float resp_pref = orientation_column_get_response(col, 45.0f);
    float resp_orth = orientation_column_get_response(col, 135.0f);

    EXPECT_GT(resp_pref, 0.0f);
    EXPECT_GT(resp_orth, 0.0f);

    // Difference should be small for broad tuning
    EXPECT_LT(fabs(resp_pref - resp_orth), 1.0f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, EdgeCase_MaxOrientations) {
    // Test with maximum allowed orientations
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        ORIENTATION_MAX_ORIENTATIONS, test_spatial_freq, test_tuning_width
    );

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, ORIENTATION_MAX_ORIENTATIONS);

    // Should be able to process
    bool result = orientation_hypercolumn_process(
        hcol, uniform_patch.data(), patch_size, patch_size
    );
    EXPECT_TRUE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(OrientationColumnsTest, EdgeCase_SingleOrientation) {
    // Hypercolumn with just one orientation
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        1, test_spatial_freq, test_tuning_width
    );

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, 1);

    // Should still work
    bool result = orientation_hypercolumn_process(
        hcol, vertical_edge.data(), patch_size, patch_size
    );
    EXPECT_TRUE(result);

    float dominant = orientation_hypercolumn_get_dominant(hcol);
    EXPECT_GE(dominant, 0.0f);
    EXPECT_LT(dominant, 180.0f);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// MATHEMATICAL CORRECTNESS TESTS
// ============================================================================

TEST_F(OrientationColumnsTest, Math_GaborSymmetry) {
    orientation_column_t* col = orientation_column_create(
        0.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Create symmetric patch
    std::vector<float> symmetric_patch(patch_size * patch_size);
    for (uint32_t y = 0; y < patch_size; y++) {
        for (uint32_t x = 0; x < patch_size; x++) {
            // Symmetric about vertical axis
            uint32_t mirror_x = patch_size - 1 - x;
            float val = (x < patch_size / 2) ? 0.3f : 0.7f;
            symmetric_patch[y * patch_size + x] = val;
            symmetric_patch[y * patch_size + mirror_x] = val;
        }
    }

    float response = orientation_column_apply_gabor(
        col, symmetric_patch.data(), patch_size, patch_size
    );

    // Should produce reasonable response
    EXPECT_NE(response, 0.0f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, Math_EnergyNonNegativity) {
    orientation_column_t* col = orientation_column_create(
        test_orientation, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Energy model should always be non-negative
    std::vector<std::vector<float>*> test_patches = {
        &uniform_patch, &vertical_edge, &horizontal_edge, &diagonal_edge
    };

    for (auto patch : test_patches) {
        float energy = orientation_column_compute_energy(
            col, patch->data(), patch_size, patch_size
        );
        EXPECT_GE(energy, 0.0f);
    }

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, Math_TuningCurveSymmetry) {
    orientation_column_t* col = orientation_column_create(
        90.0f, test_tuning_width, test_spatial_freq
    );
    ASSERT_NE(col, nullptr);

    // Tuning curve should be symmetric around preferred orientation
    float resp_minus_30 = orientation_column_get_response(col, 60.0f);
    float resp_plus_30 = orientation_column_get_response(col, 120.0f);

    // Should be approximately equal due to von Mises symmetry
    EXPECT_NEAR(resp_minus_30, resp_plus_30, 0.1f);

    orientation_column_destroy(col);
}

TEST_F(OrientationColumnsTest, Math_OSIRange) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        test_num_orientations, test_spatial_freq, test_tuning_width
    );
    ASSERT_NE(hcol, nullptr);

    // Test various activation patterns
    // Maximum selectivity
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 0.0f;
    }
    hcol->columns[0].activation = 1.0f;

    float osi_max = orientation_hypercolumn_compute_osi(hcol);
    EXPECT_GT(osi_max, 0.5f);
    EXPECT_LE(osi_max, 1.0f);

    // Minimum selectivity
    for (uint32_t i = 0; i < test_num_orientations; i++) {
        hcol->columns[i].activation = 1.0f;
    }

    float osi_min = orientation_hypercolumn_compute_osi(hcol);
    EXPECT_GE(osi_min, -0.2f);
    EXPECT_LT(osi_min, 0.2f);

    orientation_hypercolumn_destroy(hcol);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
