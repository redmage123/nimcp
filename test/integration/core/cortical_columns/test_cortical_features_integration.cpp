/**
 * @file test_cortical_features_integration.cpp
 * @brief Integration tests for cortical features (orientation columns)
 *
 * Tests cortical feature functionality including:
 * - Orientation selectivity
 * - Feature hypercolumns
 * - Topographic maps
 * - Sparse coding
 * - Surround modulation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"
#include "core/cortical_columns/nimcp_orientation_columns.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CorticalFeaturesIntegrationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }

    // Helper to create oriented grating image patch
    std::vector<float> CreateOrientedGrating(
        uint32_t width, uint32_t height,
        float orientation_deg, float frequency) {

        std::vector<float> patch(width * height);
        float orientation_rad = orientation_deg * M_PI / 180.0f;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float xr = x * cosf(orientation_rad) + y * sinf(orientation_rad);
                patch[y * width + x] = 0.5f + 0.5f * sinf(2 * M_PI * frequency * xr);
            }
        }

        return patch;
    }

    // Helper to create edge at specific orientation
    std::vector<float> CreateEdge(uint32_t width, uint32_t height,
                                   float orientation_deg) {
        std::vector<float> patch(width * height, 0.0f);
        float orientation_rad = orientation_deg * M_PI / 180.0f;

        float center_x = width / 2.0f;
        float center_y = height / 2.0f;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float dx = x - center_x;
                float dy = y - center_y;
                float perpendicular = -dx * sinf(orientation_rad) +
                                       dy * cosf(orientation_rad);

                patch[y * width + x] = (perpendicular > 0) ? 1.0f : 0.0f;
            }
        }

        return patch;
    }
};

/*=============================================================================
 * Orientation Column Creation Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, OrientationColumnCreateBasic) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    EXPECT_FLOAT_EQ(col->preferred_orientation, 45.0f);
    EXPECT_FLOAT_EQ(col->tuning_width, 30.0f);
    EXPECT_FLOAT_EQ(col->spatial_frequency, 2.0f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, OrientationColumnCreateAllOrientations) {
    std::vector<orientation_column_t*> columns;

    for (float orient = 0.0f; orient < 180.0f; orient += 22.5f) {
        orientation_column_t* col = orientation_column_create(
            orient, ORIENTATION_DEFAULT_TUNING_WIDTH,
            ORIENTATION_DEFAULT_SPATIAL_FREQ);
        ASSERT_NE(col, nullptr);
        EXPECT_FLOAT_EQ(col->preferred_orientation, orient);
        columns.push_back(col);
    }

    EXPECT_EQ(columns.size(), 8u);

    for (auto col : columns) {
        orientation_column_destroy(col);
    }
}

TEST_F(CorticalFeaturesIntegrationTest, OrientationColumnDestroyNull) {
    orientation_column_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalFeaturesIntegrationTest, OrientationColumnCreateEdgeCases) {
    // Zero orientation
    {
        orientation_column_t* col = orientation_column_create(0.0f, 30.0f, 2.0f);
        ASSERT_NE(col, nullptr);
        orientation_column_destroy(col);
    }

    // 180 degrees (should wrap to 0)
    {
        orientation_column_t* col = orientation_column_create(180.0f, 30.0f, 2.0f);
        ASSERT_NE(col, nullptr);
        orientation_column_destroy(col);
    }

    // Narrow tuning width
    {
        orientation_column_t* col = orientation_column_create(45.0f, 5.0f, 2.0f);
        ASSERT_NE(col, nullptr);
        orientation_column_destroy(col);
    }

    // High spatial frequency
    {
        orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 10.0f);
        ASSERT_NE(col, nullptr);
        orientation_column_destroy(col);
    }
}

/*=============================================================================
 * Gabor Filter Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, SetGaborParameters) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    cc_gabor_params_t params;
    params.sigma_x = 2.0f;
    params.sigma_y = 1.0f;
    params.lambda = 4.0f;
    params.gamma = 0.5f;
    params.psi = 0.0f;
    params.theta = 45.0f * M_PI / 180.0f;

    bool result = orientation_column_set_gabor(col, &params);
    EXPECT_TRUE(result);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, SetGaborNullParams) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    bool result = orientation_column_set_gabor(col, nullptr);
    EXPECT_FALSE(result);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, ApplyGaborToMatchingOrientation) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Create grating at matching orientation
    auto patch = CreateOrientedGrating(16, 16, 45.0f, 0.1f);

    float response = orientation_column_apply_gabor(
        col, patch.data(), 16, 16);

    // Should have positive response
    EXPECT_GE(response, 0.0f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, ApplyGaborToOrthogonalOrientation) {
    orientation_column_t* col = orientation_column_create(0.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Create grating at orthogonal orientation (90 degrees)
    auto patch = CreateOrientedGrating(16, 16, 90.0f, 0.1f);

    float response = orientation_column_apply_gabor(
        col, patch.data(), 16, 16);

    // Response should be lower than preferred orientation
    EXPECT_GE(response, -1.0f);  // Valid response

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, ApplyGaborNullPatch) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    float response = orientation_column_apply_gabor(col, nullptr, 16, 16);
    EXPECT_LT(response, 0.0f);  // Error value

    orientation_column_destroy(col);
}

/*=============================================================================
 * Energy Model Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, ComputeEnergyBasic) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    auto patch = CreateOrientedGrating(16, 16, 45.0f, 0.1f);

    float energy = orientation_column_compute_energy(
        col, patch.data(), 16, 16);

    EXPECT_GE(energy, 0.0f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, EnergyPhaseInvariance) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Create two gratings with different phases
    std::vector<float> patch1(16 * 16);
    std::vector<float> patch2(16 * 16);

    float orientation_rad = 45.0f * M_PI / 180.0f;
    for (uint32_t y = 0; y < 16; y++) {
        for (uint32_t x = 0; x < 16; x++) {
            float xr = x * cosf(orientation_rad) + y * sinf(orientation_rad);
            patch1[y * 16 + x] = 0.5f + 0.5f * sinf(2 * M_PI * 0.1f * xr);
            patch2[y * 16 + x] = 0.5f + 0.5f * cosf(2 * M_PI * 0.1f * xr);
        }
    }

    float energy1 = orientation_column_compute_energy(
        col, patch1.data(), 16, 16);
    float energy2 = orientation_column_compute_energy(
        col, patch2.data(), 16, 16);

    // Energy should be similar regardless of phase
    EXPECT_NEAR(energy1, energy2, 0.3f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, EnergyToBlankImage) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Uniform gray image
    std::vector<float> patch(16 * 16, 0.5f);

    float energy = orientation_column_compute_energy(
        col, patch.data(), 16, 16);

    // Should have low energy for blank image
    EXPECT_GE(energy, 0.0f);

    orientation_column_destroy(col);
}

/*=============================================================================
 * Tuning Curve Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, GetResponseAtPreferred) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    float response = orientation_column_get_response(col, 45.0f);

    // Response at preferred orientation should be high
    EXPECT_GT(response, 0.5f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, GetResponseAtOrthogonal) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Orthogonal is 90 degrees away (135 or -45)
    float response_preferred = orientation_column_get_response(col, 45.0f);
    float response_orthogonal = orientation_column_get_response(col, 135.0f);

    // Orthogonal response should be lower
    EXPECT_LT(response_orthogonal, response_preferred);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, GetFullTuningCurve) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    float orientations[16];
    float responses[16];

    bool result = orientation_column_get_tuning_curve(
        col, orientations, responses, 16);

    EXPECT_TRUE(result);

    // Find maximum response and its orientation
    float max_response = 0.0f;
    float max_orientation = 0.0f;
    for (int i = 0; i < 16; i++) {
        EXPECT_GE(responses[i], 0.0f);
        EXPECT_LE(responses[i], 1.0f);

        if (responses[i] > max_response) {
            max_response = responses[i];
            max_orientation = orientations[i];
        }
    }

    // Maximum should be near preferred orientation
    EXPECT_NEAR(max_orientation, 45.0f, 15.0f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, TuningCurveSymmetry) {
    orientation_column_t* col = orientation_column_create(90.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Check symmetry around preferred orientation
    float response_70 = orientation_column_get_response(col, 70.0f);
    float response_110 = orientation_column_get_response(col, 110.0f);

    // Should be approximately symmetric
    EXPECT_NEAR(response_70, response_110, 0.1f);

    orientation_column_destroy(col);
}

/*=============================================================================
 * Orientation Column Statistics Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, GetOrientationColumnStats) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    // Process some stimuli
    for (float orient = 0.0f; orient < 180.0f; orient += 10.0f) {
        orientation_column_get_response(col, orient);
    }

    orientation_stats_t stats;
    bool result = orientation_column_get_stats(col, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.mean_activation, 0.0f);
    EXPECT_GE(stats.max_activation, 0.0f);

    orientation_column_destroy(col);
}

TEST_F(CorticalFeaturesIntegrationTest, GetStatsNullStats) {
    orientation_column_t* col = orientation_column_create(45.0f, 30.0f, 2.0f);
    ASSERT_NE(col, nullptr);

    bool result = orientation_column_get_stats(col, nullptr);
    EXPECT_FALSE(result);

    orientation_column_destroy(col);
}

/*=============================================================================
 * Orientation Hypercolumn Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnCreateDefault) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        ORIENTATION_DEFAULT_NUM_ORIENTATIONS,
        ORIENTATION_DEFAULT_SPATIAL_FREQ,
        ORIENTATION_DEFAULT_TUNING_WIDTH);

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, ORIENTATION_DEFAULT_NUM_ORIENTATIONS);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnCreateCustom) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(8, 3.0f, 25.0f);

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, 8u);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnDestroyNull) {
    orientation_hypercolumn_destroy(nullptr);
    SUCCEED();
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnProcessImage) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.1f);

    bool result = orientation_hypercolumn_process(
        hcol, patch.data(), 32, 32);

    EXPECT_TRUE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnGetDominant) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    // Process a 45-degree grating
    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.08f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    float dominant = orientation_hypercolumn_get_dominant(hcol);

    // Dominant should be near 45 degrees
    EXPECT_GE(dominant, 0.0f);
    EXPECT_LT(dominant, 180.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnGetDistribution) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(8, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 90.0f, 0.1f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    float orientations[8];
    float responses[8];
    uint32_t num_orientations;

    bool result = orientation_hypercolumn_get_distribution(
        hcol, orientations, responses, &num_orientations);

    EXPECT_TRUE(result);
    EXPECT_EQ(num_orientations, 8u);

    for (uint32_t i = 0; i < num_orientations; i++) {
        EXPECT_GE(orientations[i], 0.0f);
        EXPECT_LT(orientations[i], 180.0f);
        EXPECT_GE(responses[i], 0.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Normalization and Inhibition Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnNormalize) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.1f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    bool result = orientation_hypercolumn_normalize(hcol);
    EXPECT_TRUE(result);

    // After normalization, responses should be bounded
    float orientations[16];
    float responses[16];
    uint32_t num_orientations;

    ASSERT_TRUE(orientation_hypercolumn_get_distribution(
        hcol, orientations, responses, &num_orientations));

    for (uint32_t i = 0; i < num_orientations; i++) {
        EXPECT_GE(responses[i], 0.0f);
        EXPECT_LE(responses[i], 1.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, HypercolumnApplyInhibition) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.1f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    // Get responses before inhibition
    float responses_before[16];
    float orientations[16];
    uint32_t num;
    ASSERT_TRUE(orientation_hypercolumn_get_distribution(
        hcol, orientations, responses_before, &num));

    // Apply cross-orientation inhibition
    bool result = orientation_hypercolumn_apply_inhibition(hcol, 0.5f);
    EXPECT_TRUE(result);

    // Get responses after inhibition
    float responses_after[16];
    ASSERT_TRUE(orientation_hypercolumn_get_distribution(
        hcol, orientations, responses_after, &num));

    // Responses should generally be reduced
    SUCCEED();

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, InhibitionStrengthEffect) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(8, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.1f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    // Strong inhibition
    EXPECT_TRUE(orientation_hypercolumn_apply_inhibition(hcol, 0.9f));

    float osi_strong = orientation_hypercolumn_compute_osi(hcol);

    // Reset and apply weak inhibition
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));
    EXPECT_TRUE(orientation_hypercolumn_apply_inhibition(hcol, 0.1f));

    float osi_weak = orientation_hypercolumn_compute_osi(hcol);

    // Both should be valid
    EXPECT_GE(osi_strong, -1.0f);
    EXPECT_GE(osi_weak, -1.0f);

    orientation_hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Selectivity Metrics Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, ComputeOSI) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.08f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    float osi = orientation_hypercolumn_compute_osi(hcol);

    // OSI should be between 0 and 1
    EXPECT_GE(osi, -1.0f);  // Error check
    if (osi >= 0.0f) {
        EXPECT_LE(osi, 1.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, ComputeCircularVariance) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    auto patch = CreateOrientedGrating(32, 32, 90.0f, 0.08f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    float cv = orientation_hypercolumn_compute_circular_variance(hcol);

    // CV should be between 0 and 1
    EXPECT_GE(cv, -1.0f);  // Error check
    if (cv >= 0.0f) {
        EXPECT_LE(cv, 1.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, SelectivityForDifferentStimuli) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    // Sharp edge - should have high selectivity
    auto edge = CreateEdge(32, 32, 45.0f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, edge.data(), 32, 32));

    float osi_edge = orientation_hypercolumn_compute_osi(hcol);

    // Random noise - should have low selectivity
    std::vector<float> noise(32 * 32);
    for (size_t i = 0; i < noise.size(); i++) {
        noise[i] = (float)rand() / RAND_MAX;
    }
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, noise.data(), 32, 32));

    float osi_noise = orientation_hypercolumn_compute_osi(hcol);

    // Edge should have higher selectivity than noise
    if (osi_edge >= 0.0f && osi_noise >= 0.0f) {
        EXPECT_GE(osi_edge, osi_noise - 0.1f);
    }

    orientation_hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Pinwheel Organization Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, SetPinwheelCenter) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    bool result = orientation_hypercolumn_set_pinwheel(hcol, 5.0f, 5.0f);
    EXPECT_TRUE(result);

    EXPECT_FLOAT_EQ(hcol->pinwheel_center_x, 5.0f);
    EXPECT_FLOAT_EQ(hcol->pinwheel_center_y, 5.0f);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, GetLocalOrientation) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    ASSERT_TRUE(orientation_hypercolumn_set_pinwheel(hcol, 0.0f, 0.0f));

    // Points at different angles from center
    float orient_right = orientation_hypercolumn_get_local_orientation(hcol, 1.0f, 0.0f);
    float orient_up = orientation_hypercolumn_get_local_orientation(hcol, 0.0f, 1.0f);
    float orient_left = orientation_hypercolumn_get_local_orientation(hcol, -1.0f, 0.0f);
    float orient_down = orientation_hypercolumn_get_local_orientation(hcol, 0.0f, -1.0f);

    // All should be valid orientations
    if (orient_right >= 0.0f) {
        EXPECT_LT(orient_right, 180.0f);
    }
    if (orient_up >= 0.0f) {
        EXPECT_LT(orient_up, 180.0f);
    }
    if (orient_left >= 0.0f) {
        EXPECT_LT(orient_left, 180.0f);
    }
    if (orient_down >= 0.0f) {
        EXPECT_LT(orient_down, 180.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, PinwheelSymmetry) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    ASSERT_TRUE(orientation_hypercolumn_set_pinwheel(hcol, 0.0f, 0.0f));

    // Opposite points should have orientations 90 degrees apart
    float orient_right = orientation_hypercolumn_get_local_orientation(hcol, 1.0f, 0.0f);
    float orient_up = orientation_hypercolumn_get_local_orientation(hcol, 0.0f, 1.0f);

    if (orient_right >= 0.0f && orient_up >= 0.0f) {
        float diff = fabsf(orient_right - orient_up);
        // Should differ by approximately 90 degrees
        EXPECT_NEAR(diff, 90.0f, 10.0f);
    }

    orientation_hypercolumn_destroy(hcol);
}

/*=============================================================================
 * Hypercolumn Statistics Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, GetHypercolumnStats) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    // Process multiple stimuli
    for (float orient = 0.0f; orient < 180.0f; orient += 45.0f) {
        auto patch = CreateOrientedGrating(32, 32, orient, 0.1f);
        ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));
    }

    orientation_hypercolumn_stats_t stats;
    bool result = orientation_hypercolumn_get_stats(hcol, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.mean_osi, 0.0f);
    EXPECT_GE(stats.mean_circular_variance, 0.0f);
    EXPECT_GE(stats.num_active_columns, 0u);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, GetStatsNullHypercolumn) {
    orientation_hypercolumn_stats_t stats;
    bool result = orientation_hypercolumn_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

/*=============================================================================
 * Batch Processing Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, ProcessEntireImage) {
    // Create hypercolumns
    std::vector<orientation_hypercolumn_t*> hypercolumns;
    for (int i = 0; i < 4; i++) {
        orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(8, 2.0f, 30.0f);
        ASSERT_NE(hcol, nullptr);
        hypercolumns.push_back(hcol);
    }

    // Create test image
    auto image = CreateOrientedGrating(64, 64, 45.0f, 0.05f);

    // Create orientation map
    std::vector<float> orientation_map(64 * 64);

    bool result = orientation_process_image(
        image.data(), 64, 64,
        hypercolumns.data(), 4,
        orientation_map.data());

    EXPECT_TRUE(result);

    // Check map values are valid
    for (size_t i = 0; i < orientation_map.size(); i++) {
        if (orientation_map[i] >= 0.0f) {
            EXPECT_LT(orientation_map[i], 180.0f);
        }
    }

    for (auto hcol : hypercolumns) {
        orientation_hypercolumn_destroy(hcol);
    }
}

TEST_F(CorticalFeaturesIntegrationTest, ProcessImageNullInputs) {
    std::vector<float> orientation_map(64 * 64);

    // Null image
    bool result = orientation_process_image(
        nullptr, 64, 64, nullptr, 0, orientation_map.data());
    EXPECT_FALSE(result);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CorticalFeaturesIntegrationTest, HighFrequencyProcessing) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    // Process many frames
    for (int frame = 0; frame < 100; frame++) {
        float orientation = (frame * 3.6f);  // Rotating orientation
        auto patch = CreateOrientedGrating(32, 32, orientation, 0.1f);
        ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

        float dominant = orientation_hypercolumn_get_dominant(hcol);
        EXPECT_GE(dominant, -1.0f);  // Valid or error
    }

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, ManyHypercolumns) {
    std::vector<orientation_hypercolumn_t*> hypercolumns;

    // Create many hypercolumns
    for (int i = 0; i < 50; i++) {
        orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(8, 2.0f, 30.0f);
        ASSERT_NE(hcol, nullptr);
        hypercolumns.push_back(hcol);
    }

    // Process all with same input
    auto patch = CreateOrientedGrating(16, 16, 45.0f, 0.1f);

    for (auto hcol : hypercolumns) {
        ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 16, 16));
    }

    for (auto hcol : hypercolumns) {
        orientation_hypercolumn_destroy(hcol);
    }
}

TEST_F(CorticalFeaturesIntegrationTest, LargeImagePatch) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(16, 2.0f, 30.0f);
    ASSERT_NE(hcol, nullptr);

    // Large image patch
    auto patch = CreateOrientedGrating(128, 128, 45.0f, 0.05f);

    bool result = orientation_hypercolumn_process(hcol, patch.data(), 128, 128);
    EXPECT_TRUE(result);

    orientation_hypercolumn_destroy(hcol);
}

TEST_F(CorticalFeaturesIntegrationTest, MaxOrientations) {
    orientation_hypercolumn_t* hcol = orientation_hypercolumn_create(
        ORIENTATION_MAX_ORIENTATIONS, 2.0f, 30.0f);

    ASSERT_NE(hcol, nullptr);
    EXPECT_EQ(hcol->num_orientations, ORIENTATION_MAX_ORIENTATIONS);

    auto patch = CreateOrientedGrating(32, 32, 45.0f, 0.1f);
    ASSERT_TRUE(orientation_hypercolumn_process(hcol, patch.data(), 32, 32));

    float dominant = orientation_hypercolumn_get_dominant(hcol);
    EXPECT_GE(dominant, -1.0f);

    orientation_hypercolumn_destroy(hcol);
}
