//=============================================================================
// test_topographic_maps.cpp - Unit Tests for Topographic Maps Module
//=============================================================================
/**
 * @file test_topographic_maps.cpp
 * @brief Comprehensive unit tests for topographic mapping functionality
 *
 * WHAT: Unit tests for topographic maps module (retinotopic, tonotopic, somatotopic)
 * WHY:  Ensure correctness of all coordinate transforms and receptive field calculations
 * HOW:  GoogleTest framework with fixtures for each mapping type
 *
 * TEST CATEGORIES:
 * 1. Lifecycle Tests - Create, destroy, validate config
 * 2. Retinotopic Mapping Tests - Log-polar transform, foveal magnification
 * 3. Tonotopic Mapping Tests - Logarithmic and linear frequency mapping
 * 4. Somatotopic Mapping Tests - Piecewise body regions, homunculus
 * 5. Coordinate Transform Tests - Forward and inverse mappings
 * 6. Receptive Field Tests - Center, size, magnification calculations
 * 7. Activity Projection Tests - Bilinear resampling
 * 8. Column Assignment Tests - RF center pre-computation
 * 9. Neighborhood Tests - Spatial queries
 * 10. Statistics Tests - Map metrics and validation
 * 11. Edge Cases Tests - NULL handling, boundaries, out-of-range
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

// Test constants
#define EPSILON 1e-5f
#define PI 3.14159265358979323846f

// Helper macros
#define EXPECT_NEAR_FLOAT(a, b, eps) EXPECT_NEAR((double)(a), (double)(b), (double)(eps))
#define ASSERT_NEAR_FLOAT(a, b, eps) ASSERT_NEAR((double)(a), (double)(b), (double)(eps))

//=============================================================================
// Test Fixture - Base
//=============================================================================

class TopographicMapsTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();
    }

    void TearDown() override {
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }

    // Helper: Create basic config
    topographic_map_config_t createBasicConfig() {
        topographic_map_config_t config;
        memset(&config, 0, sizeof(config));

        config.type = TOPOGRAPHIC_CUSTOM;
        config.input_dims = 2;
        config.cortical_dims = 2;

        config.input_range[0] = 0.0f;
        config.input_range[1] = 100.0f;
        config.input_range[2] = 0.0f;
        config.input_range[3] = 100.0f;

        config.cortical_range[0] = 0.0f;
        config.cortical_range[1] = 50.0f;
        config.cortical_range[2] = 0.0f;
        config.cortical_range[3] = 50.0f;

        config.magnification_factor = 1.0f;

        return config;
    }
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(TopographicMapsTest, Lifecycle_CreateDestroy) {
    topographic_map_config_t config = createBasicConfig();

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Lifecycle_CreateNullConfig) {
    topographic_map_t* map = topographic_map_create(nullptr);
    EXPECT_EQ(map, nullptr);
}

TEST_F(TopographicMapsTest, Lifecycle_DestroyNull) {
    // Should not crash
    topographic_map_destroy(nullptr);
}

TEST_F(TopographicMapsTest, Lifecycle_ValidateConfig_Valid) {
    topographic_map_config_t config = createBasicConfig();
    EXPECT_TRUE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, Lifecycle_ValidateConfig_Null) {
    EXPECT_FALSE(topographic_map_validate_config(nullptr));
}

TEST_F(TopographicMapsTest, Lifecycle_ValidateConfig_InvalidDimensions) {
    topographic_map_config_t config = createBasicConfig();
    config.input_dims = 0;
    EXPECT_FALSE(topographic_map_validate_config(&config));

    config = createBasicConfig();
    config.input_dims = 4; // Too large
    EXPECT_FALSE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, Lifecycle_ValidateConfig_InvalidRanges) {
    topographic_map_config_t config = createBasicConfig();
    config.input_range[1] = config.input_range[0]; // Equal
    EXPECT_FALSE(topographic_map_validate_config(&config));

    config = createBasicConfig();
    config.input_range[1] = config.input_range[0] - 1.0f; // Reversed
    EXPECT_FALSE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, Lifecycle_GetDimensions) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width = 0, height = 0;
    topographic_map_get_dimensions(map, &width, &height);

    EXPECT_GT(width, 0u);
    EXPECT_GT(height, 0u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Lifecycle_GetDimensions_NullPointers) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Should not crash with NULL outputs
    topographic_map_get_dimensions(map, nullptr, nullptr);

    uint32_t width = 0;
    topographic_map_get_dimensions(map, &width, nullptr);
    EXPECT_GT(width, 0u);

    topographic_map_destroy(map);
}

//=============================================================================
// 2. Retinotopic Mapping Tests
//=============================================================================

TEST_F(TopographicMapsTest, Retinotopic_Create) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Retinotopic_CreateNull) {
    topographic_map_t* map = topographic_map_create_retinotopic(nullptr, 100, 100);
    EXPECT_EQ(map, nullptr);
}

TEST_F(TopographicMapsTest, Retinotopic_CreateZeroDimensions) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 0, 100);
    EXPECT_EQ(map, nullptr);
}

TEST_F(TopographicMapsTest, Retinotopic_LogPolarTransform_Forward) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    // Test foveal point (small eccentricity)
    float input[2] = {1.0f, PI};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    // Should map to cortical coordinates
    EXPECT_GT(cortical[0], 0.0f);
    EXPECT_GT(cortical[1], 0.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Retinotopic_LogPolarTransform_Inverse) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    // Forward then inverse should recover input (approximately)
    float input_orig[2] = {5.0f, PI / 2.0f};
    float cortical[2];
    float input_recovered[2];

    topographic_map_input_to_cortex(map, input_orig, cortical, 1);
    topographic_map_cortex_to_input(map, cortical, input_recovered, 1);

    EXPECT_NEAR_FLOAT(input_orig[0], input_recovered[0], 0.5f);
    EXPECT_NEAR_FLOAT(input_orig[1], input_recovered[1], 0.1f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Retinotopic_FovealMagnification) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    // Foveal region should have higher magnification than periphery
    float foveal[2] = {0.5f, 0.0f};
    float peripheral[2] = {15.0f, 0.0f};

    float mag_foveal = topographic_map_get_magnification(map, foveal);
    float mag_peripheral = topographic_map_get_magnification(map, peripheral);

    EXPECT_GT(mag_foveal, mag_peripheral);

    // Test magnification formula: M = M₀ / (1 + E/E₂)
    float expected_foveal = params.cortical_magnification /
                           (1.0f + foveal[0] / params.eccentricity_half);
    EXPECT_NEAR_FLOAT(mag_foveal, expected_foveal, 0.1f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Retinotopic_AspectRatio) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 2.0f; // Non-unity aspect ratio
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    float input[2] = {5.0f, PI};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    // Aspect ratio should affect x-coordinate
    EXPECT_GT(cortical[0], 0.0f);

    topographic_map_destroy(map);
}

//=============================================================================
// 3. Tonotopic Mapping Tests
//=============================================================================

TEST_F(TopographicMapsTest, Tonotopic_CreateLogarithmic) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_CreateLinear) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = false;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_CreateNull) {
    topographic_map_t* map = topographic_map_create_tonotopic(nullptr, 64);
    EXPECT_EQ(map, nullptr);
}

TEST_F(TopographicMapsTest, Tonotopic_LogarithmicMapping_Forward) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    // Test middle frequency (should map to middle cortical position)
    float input[1] = {1000.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    EXPECT_GT(cortical[0], 0.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_LogarithmicMapping_Inverse) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    // Forward then inverse
    float input_orig[1] = {440.0f}; // A4 note
    float cortical[2];
    float input_recovered[1];

    topographic_map_input_to_cortex(map, input_orig, cortical, 1);
    topographic_map_cortex_to_input(map, cortical, input_recovered, 1);

    EXPECT_NEAR_FLOAT(input_orig[0], input_recovered[0], 10.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_LinearMapping) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = false;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    // Linear mapping: evenly spaced frequencies should map to evenly spaced cortical positions
    float input1[1] = {5000.0f};
    float input2[1] = {10000.0f};
    float input3[1] = {15000.0f};

    float cortical1[2], cortical2[2], cortical3[2];

    topographic_map_input_to_cortex(map, input1, cortical1, 1);
    topographic_map_input_to_cortex(map, input2, cortical2, 1);
    topographic_map_input_to_cortex(map, input3, cortical3, 1);

    float spacing1 = cortical2[0] - cortical1[0];
    float spacing2 = cortical3[0] - cortical2[0];

    EXPECT_NEAR_FLOAT(spacing1, spacing2, 1.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_LogarithmicMagnification) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    // Lower frequencies should have higher magnification
    float low_freq[1] = {100.0f};
    float high_freq[1] = {10000.0f};

    float mag_low = topographic_map_get_magnification(map, low_freq);
    float mag_high = topographic_map_get_magnification(map, high_freq);

    EXPECT_GT(mag_low, mag_high);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Tonotopic_LinearMagnification) {
    tonotopic_params_t params;
    params.min_frequency = 20.0f;
    params.max_frequency = 20000.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = false;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 64);
    ASSERT_NE(map, nullptr);

    // Linear mapping should have constant magnification
    float freq1[1] = {1000.0f};
    float freq2[1] = {10000.0f};

    float mag1 = topographic_map_get_magnification(map, freq1);
    float mag2 = topographic_map_get_magnification(map, freq2);

    EXPECT_NEAR_FLOAT(mag1, mag2, EPSILON);

    topographic_map_destroy(map);
}

//=============================================================================
// 4. Somatotopic Mapping Tests
//=============================================================================

TEST_F(TopographicMapsTest, Somatotopic_Create) {
    topographic_map_t* map = topographic_map_create_somatotopic(5);
    ASSERT_NE(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_CreateZeroRegions) {
    topographic_map_t* map = topographic_map_create_somatotopic(0);
    EXPECT_EQ(map, nullptr);
}

TEST_F(TopographicMapsTest, Somatotopic_AddBodyRegion) {
    topographic_map_t* map = topographic_map_create_somatotopic(3);
    ASSERT_NE(map, nullptr);

    somatotopic_region_t region;
    strncpy(region.name, "thumb", sizeof(region.name));
    region.input_start = 0.0f;
    region.input_end = 10.0f;
    region.cortical_start = 0.0f;
    region.cortical_end = 30.0f;  // 3x magnification
    region.magnification = 3.0f;
    region.receptor_density = 100.0f;

    bool result = topographic_map_add_body_region(map, &region);
    EXPECT_TRUE(result);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_AddBodyRegion_Null) {
    topographic_map_t* map = topographic_map_create_somatotopic(3);
    ASSERT_NE(map, nullptr);

    bool result = topographic_map_add_body_region(map, nullptr);
    EXPECT_FALSE(result);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_Homunculus) {
    topographic_map_t* map = topographic_map_create_somatotopic(3);
    ASSERT_NE(map, nullptr);

    // Create homunculus with hand, arm, leg
    somatotopic_region_t hand;
    strncpy(hand.name, "hand", sizeof(hand.name));
    hand.input_start = 0.0f;
    hand.input_end = 10.0f;
    hand.cortical_start = 0.0f;
    hand.cortical_end = 40.0f;  // Large representation
    hand.magnification = 4.0f;
    hand.receptor_density = 200.0f;

    somatotopic_region_t arm;
    strncpy(arm.name, "arm", sizeof(arm.name));
    arm.input_start = 10.0f;
    arm.input_end = 30.0f;
    arm.cortical_start = 40.0f;
    arm.cortical_end = 60.0f;  // Medium representation
    arm.magnification = 1.0f;
    arm.receptor_density = 50.0f;

    somatotopic_region_t leg;
    strncpy(leg.name, "leg", sizeof(leg.name));
    leg.input_start = 30.0f;
    leg.input_end = 60.0f;
    leg.cortical_start = 60.0f;
    leg.cortical_end = 70.0f;  // Small representation
    leg.magnification = 0.33f;
    leg.receptor_density = 20.0f;

    EXPECT_TRUE(topographic_map_add_body_region(map, &hand));
    EXPECT_TRUE(topographic_map_add_body_region(map, &arm));
    EXPECT_TRUE(topographic_map_add_body_region(map, &leg));

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_PiecewiseMapping_Forward) {
    topographic_map_t* map = topographic_map_create_somatotopic(2);
    ASSERT_NE(map, nullptr);

    somatotopic_region_t region1;
    strncpy(region1.name, "region1", sizeof(region1.name));
    region1.input_start = 0.0f;
    region1.input_end = 10.0f;
    region1.cortical_start = 0.0f;
    region1.cortical_end = 50.0f;
    region1.magnification = 5.0f;
    region1.receptor_density = 100.0f;

    somatotopic_region_t region2;
    strncpy(region2.name, "region2", sizeof(region2.name));
    region2.input_start = 10.0f;
    region2.input_end = 20.0f;
    region2.cortical_start = 50.0f;
    region2.cortical_end = 60.0f;
    region2.magnification = 1.0f;
    region2.receptor_density = 20.0f;

    EXPECT_TRUE(topographic_map_add_body_region(map, &region1));
    EXPECT_TRUE(topographic_map_add_body_region(map, &region2));

    // Test mapping within region1
    float input1[1] = {5.0f};
    float cortical1[2];
    topographic_map_input_to_cortex(map, input1, cortical1, 1);
    EXPECT_NEAR_FLOAT(cortical1[0], 25.0f, 1.0f); // Middle of region1 cortical range

    // Test mapping within region2
    float input2[1] = {15.0f};
    float cortical2[2];
    topographic_map_input_to_cortex(map, input2, cortical2, 1);
    EXPECT_NEAR_FLOAT(cortical2[0], 55.0f, 1.0f); // Middle of region2 cortical range

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_PiecewiseMapping_Inverse) {
    topographic_map_t* map = topographic_map_create_somatotopic(2);
    ASSERT_NE(map, nullptr);

    somatotopic_region_t region;
    strncpy(region.name, "test", sizeof(region.name));
    region.input_start = 0.0f;
    region.input_end = 10.0f;
    region.cortical_start = 0.0f;
    region.cortical_end = 50.0f;
    region.magnification = 5.0f;
    region.receptor_density = 100.0f;

    EXPECT_TRUE(topographic_map_add_body_region(map, &region));

    // Forward then inverse
    float input_orig[1] = {7.0f};
    float cortical[2];
    float input_recovered[1];

    topographic_map_input_to_cortex(map, input_orig, cortical, 1);
    topographic_map_cortex_to_input(map, cortical, input_recovered, 1);

    EXPECT_NEAR_FLOAT(input_orig[0], input_recovered[0], 0.1f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Somatotopic_Magnification) {
    topographic_map_t* map = topographic_map_create_somatotopic(2);
    ASSERT_NE(map, nullptr);

    somatotopic_region_t high_mag;
    strncpy(high_mag.name, "high", sizeof(high_mag.name));
    high_mag.input_start = 0.0f;
    high_mag.input_end = 10.0f;
    high_mag.cortical_start = 0.0f;
    high_mag.cortical_end = 50.0f;
    high_mag.magnification = 10.0f;
    high_mag.receptor_density = 200.0f;

    somatotopic_region_t low_mag;
    strncpy(low_mag.name, "low", sizeof(low_mag.name));
    low_mag.input_start = 10.0f;
    low_mag.input_end = 20.0f;
    low_mag.cortical_start = 50.0f;
    low_mag.cortical_end = 55.0f;
    low_mag.magnification = 0.5f;
    low_mag.receptor_density = 10.0f;

    EXPECT_TRUE(topographic_map_add_body_region(map, &high_mag));
    EXPECT_TRUE(topographic_map_add_body_region(map, &low_mag));

    float input_high[1] = {5.0f};
    float input_low[1] = {15.0f};

    float mag_high = topographic_map_get_magnification(map, input_high);
    float mag_low = topographic_map_get_magnification(map, input_low);

    EXPECT_NEAR_FLOAT(mag_high, 10.0f, 0.1f);
    EXPECT_NEAR_FLOAT(mag_low, 0.5f, 0.1f);

    topographic_map_destroy(map);
}

//=============================================================================
// 5. Coordinate Transform Tests
//=============================================================================

TEST_F(TopographicMapsTest, CoordinateTransform_InputToCortex_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {50.0f, 50.0f};
    float cortical[2];

    // NULL map
    topographic_map_input_to_cortex(nullptr, input, cortical, 1);

    // NULL input
    topographic_map_input_to_cortex(map, nullptr, cortical, 1);

    // NULL output
    topographic_map_input_to_cortex(map, input, nullptr, 1);

    // Zero points
    topographic_map_input_to_cortex(map, input, cortical, 0);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, CoordinateTransform_CortexToInput_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float cortical[2] = {25.0f, 25.0f};
    float input[2];

    // NULL map
    topographic_map_cortex_to_input(nullptr, cortical, input, 1);

    // NULL input
    topographic_map_cortex_to_input(map, nullptr, input, 1);

    // NULL output
    topographic_map_cortex_to_input(map, cortical, nullptr, 1);

    // Zero points
    topographic_map_cortex_to_input(map, cortical, input, 0);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, CoordinateTransform_LinearMapping) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Input space: [0, 100] x [0, 100]
    // Cortical space: [0, 50] x [0, 50]
    // Scale factor: 0.5

    float input[2] = {50.0f, 50.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    EXPECT_NEAR_FLOAT(cortical[0], 25.0f, 1.0f);
    EXPECT_NEAR_FLOAT(cortical[1], 25.0f, 1.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, CoordinateTransform_MultiplePoints) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[6] = {0.0f, 0.0f, 50.0f, 50.0f, 100.0f, 100.0f};
    float cortical[6];

    topographic_map_input_to_cortex(map, input, cortical, 3);

    // First point
    EXPECT_NEAR_FLOAT(cortical[0], 0.0f, 1.0f);
    EXPECT_NEAR_FLOAT(cortical[1], 0.0f, 1.0f);

    // Second point
    EXPECT_NEAR_FLOAT(cortical[2], 25.0f, 1.0f);
    EXPECT_NEAR_FLOAT(cortical[3], 25.0f, 1.0f);

    // Third point
    EXPECT_NEAR_FLOAT(cortical[4], 50.0f, 1.0f);
    EXPECT_NEAR_FLOAT(cortical[5], 50.0f, 1.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, CoordinateTransform_RoundTrip) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input_orig[2] = {75.0f, 25.0f};
    float cortical[2];
    float input_recovered[2];

    topographic_map_input_to_cortex(map, input_orig, cortical, 1);
    topographic_map_cortex_to_input(map, cortical, input_recovered, 1);

    EXPECT_NEAR_FLOAT(input_orig[0], input_recovered[0], 1.0f);
    EXPECT_NEAR_FLOAT(input_orig[1], input_recovered[1], 1.0f);

    topographic_map_destroy(map);
}

//=============================================================================
// 6. Receptive Field Tests
//=============================================================================

TEST_F(TopographicMapsTest, ReceptiveField_GetColumnForInput) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {50.0f, 50.0f};
    uint32_t column_id = topographic_map_get_column_for_input(map, input);

    EXPECT_NE(column_id, UINT32_MAX);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_GetColumnForInput_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {50.0f, 50.0f};

    // NULL map
    uint32_t result = topographic_map_get_column_for_input(nullptr, input);
    EXPECT_EQ(result, UINT32_MAX);

    // NULL input
    result = topographic_map_get_column_for_input(map, nullptr);
    EXPECT_EQ(result, UINT32_MAX);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_GetReceptiveField) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t column_id = 0;
    float rf_center[2];
    float rf_size;

    topographic_map_get_receptive_field(map, column_id, rf_center, &rf_size);

    EXPECT_GE(rf_center[0], 0.0f);
    EXPECT_GE(rf_center[1], 0.0f);
    EXPECT_GT(rf_size, 0.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_GetReceptiveField_InvalidColumn) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t total_columns = width * height;

    float rf_center[2];
    float rf_size;

    // Invalid column ID - should not crash
    topographic_map_get_receptive_field(map, total_columns + 100, rf_center, &rf_size);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_GetReceptiveField_NullOutputs) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t column_id = 0;

    // NULL outputs should not crash
    topographic_map_get_receptive_field(map, column_id, nullptr, nullptr);

    float rf_center[2];
    topographic_map_get_receptive_field(map, column_id, rf_center, nullptr);

    float rf_size;
    topographic_map_get_receptive_field(map, column_id, nullptr, &rf_size);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_Magnification) {
    topographic_map_config_t config = createBasicConfig();
    config.magnification_factor = 2.0f;

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {50.0f, 50.0f};
    float mag = topographic_map_get_magnification(map, input);

    EXPECT_NEAR_FLOAT(mag, 2.0f, 0.1f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ReceptiveField_Magnification_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {50.0f, 50.0f};

    // NULL map
    float mag = topographic_map_get_magnification(nullptr, input);
    EXPECT_EQ(mag, 0.0f);

    // NULL input
    mag = topographic_map_get_magnification(map, nullptr);
    EXPECT_EQ(mag, 0.0f);

    topographic_map_destroy(map);
}

//=============================================================================
// 7. Activity Projection Tests
//=============================================================================

TEST_F(TopographicMapsTest, ActivityProjection_Basic) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Create input activity pattern
    const uint32_t input_width = 10;
    const uint32_t input_height = 10;
    float input_activity[input_width * input_height];

    for (uint32_t i = 0; i < input_width * input_height; i++) {
        input_activity[i] = 1.0f;
    }

    // Project to cortical surface
    const uint32_t cortical_width = 20;
    const uint32_t cortical_height = 20;
    float cortical_activity[cortical_width * cortical_height];

    topographic_map_project_activity(
        map,
        input_activity, input_width, input_height,
        cortical_activity, cortical_width, cortical_height
    );

    // Check that some activity was projected
    bool has_activity = false;
    for (uint32_t i = 0; i < cortical_width * cortical_height; i++) {
        if (cortical_activity[i] > 0.0f) {
            has_activity = true;
            break;
        }
    }

    EXPECT_TRUE(has_activity);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ActivityProjection_BilinearInterpolation) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Create a simple gradient
    const uint32_t input_width = 4;
    const uint32_t input_height = 4;
    float input_activity[input_width * input_height];

    for (uint32_t y = 0; y < input_height; y++) {
        for (uint32_t x = 0; x < input_width; x++) {
            input_activity[y * input_width + x] = (float)x / (input_width - 1);
        }
    }

    // Project to higher resolution
    const uint32_t cortical_width = 8;
    const uint32_t cortical_height = 8;
    float cortical_activity[cortical_width * cortical_height];

    topographic_map_project_activity(
        map,
        input_activity, input_width, input_height,
        cortical_activity, cortical_width, cortical_height
    );

    // Check for smooth interpolation
    EXPECT_GE(cortical_activity[0], 0.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ActivityProjection_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input_activity[100];
    float cortical_activity[100];

    // NULL map
    topographic_map_project_activity(
        nullptr,
        input_activity, 10, 10,
        cortical_activity, 10, 10
    );

    // NULL input
    topographic_map_project_activity(
        map,
        nullptr, 10, 10,
        cortical_activity, 10, 10
    );

    // NULL output
    topographic_map_project_activity(
        map,
        input_activity, 10, 10,
        nullptr, 10, 10
    );

    topographic_map_destroy(map);
}

//=============================================================================
// 8. Column Assignment Tests
//=============================================================================

TEST_F(TopographicMapsTest, ColumnAssignment_AssignColumns) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t num_columns = width * height;

    uint32_t* column_ids = new uint32_t[num_columns];
    float* column_positions = new float[num_columns * 2];

    topographic_map_assign_columns(map, column_ids, column_positions, num_columns);

    // Check that columns were assigned
    for (uint32_t i = 0; i < num_columns; i++) {
        EXPECT_EQ(column_ids[i], i);
    }

    // Check that positions are valid
    for (uint32_t i = 0; i < num_columns; i++) {
        EXPECT_GE(column_positions[i * 2 + 0], 0.0f);
        EXPECT_GE(column_positions[i * 2 + 1], 0.0f);
    }

    delete[] column_ids;
    delete[] column_positions;
    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ColumnAssignment_NullColumnIds) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t num_columns = width * height;

    float* column_positions = new float[num_columns * 2];

    // NULL column_ids should be allowed
    topographic_map_assign_columns(map, nullptr, column_positions, num_columns);

    delete[] column_positions;
    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, ColumnAssignment_InvalidParams) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t num_columns = width * height;

    uint32_t* column_ids = new uint32_t[num_columns];
    float* column_positions = new float[num_columns * 2];

    // NULL map
    topographic_map_assign_columns(nullptr, column_ids, column_positions, num_columns);

    // NULL positions
    topographic_map_assign_columns(map, column_ids, nullptr, num_columns);

    // Wrong number of columns
    topographic_map_assign_columns(map, column_ids, column_positions, num_columns / 2);

    delete[] column_ids;
    delete[] column_positions;
    topographic_map_destroy(map);
}

//=============================================================================
// 9. Neighborhood Tests
//=============================================================================

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t column_id = 0;
    float radius = 10.0f;
    uint32_t neighbor_ids[100];
    uint32_t max_neighbors = 100;

    uint32_t count = topographic_map_get_neighbors(
        map, column_id, radius, neighbor_ids, max_neighbors
    );

    EXPECT_GT(count, 0u);
    EXPECT_LE(count, max_neighbors);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors_SmallRadius) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t center_column = (height / 2) * width + (width / 2);

    float small_radius = 0.5f;
    uint32_t neighbor_ids[100];

    uint32_t count = topographic_map_get_neighbors(
        map, center_column, small_radius, neighbor_ids, 100
    );

    // Very small radius might have few or no neighbors
    EXPECT_LE(count, 100u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors_LargeRadius) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t column_id = 0;
    float large_radius = 100.0f;
    uint32_t neighbor_ids[1000];

    uint32_t count = topographic_map_get_neighbors(
        map, column_id, large_radius, neighbor_ids, 1000
    );

    // Large radius should find many neighbors
    EXPECT_GT(count, 0u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors_ExcludesSelf) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t column_id = 5;
    float radius = 100.0f;
    uint32_t neighbor_ids[1000];

    uint32_t count = topographic_map_get_neighbors(
        map, column_id, radius, neighbor_ids, 1000
    );

    // Check that self is not in neighbors
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_NE(neighbor_ids[i], column_id);
    }

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t neighbor_ids[100];

    // NULL map
    uint32_t count = topographic_map_get_neighbors(nullptr, 0, 10.0f, neighbor_ids, 100);
    EXPECT_EQ(count, 0u);

    // NULL neighbor_ids
    count = topographic_map_get_neighbors(map, 0, 10.0f, nullptr, 100);
    EXPECT_EQ(count, 0u);

    // Zero max_neighbors
    count = topographic_map_get_neighbors(map, 0, 10.0f, neighbor_ids, 0);
    EXPECT_EQ(count, 0u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Neighborhood_GetNeighbors_InvalidColumn) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);
    uint32_t total_columns = width * height;

    uint32_t neighbor_ids[100];

    // Invalid column ID
    uint32_t count = topographic_map_get_neighbors(
        map, total_columns + 100, 10.0f, neighbor_ids, 100
    );
    EXPECT_EQ(count, 0u);

    topographic_map_destroy(map);
}

//=============================================================================
// 10. Statistics Tests
//=============================================================================

TEST_F(TopographicMapsTest, Statistics_GetStats) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    topographic_stats_t stats;
    topographic_map_get_stats(map, &stats);

    EXPECT_GT(stats.num_columns, 0u);
    EXPECT_GT(stats.mean_magnification, 0.0f);
    EXPECT_GT(stats.max_magnification, 0.0f);
    EXPECT_GT(stats.min_magnification, 0.0f);
    EXPECT_GE(stats.max_magnification, stats.mean_magnification);
    EXPECT_LE(stats.min_magnification, stats.mean_magnification);
    EXPECT_GT(stats.total_cortical_area, 0.0f);
    EXPECT_GT(stats.coverage_ratio, 0.0f);
    EXPECT_LE(stats.coverage_ratio, 1.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Statistics_GetStats_Null) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    topographic_stats_t stats;

    // NULL map
    topographic_map_get_stats(nullptr, &stats);

    // NULL stats
    topographic_map_get_stats(map, nullptr);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, Statistics_Retinotopic) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 50, 50);
    ASSERT_NE(map, nullptr);

    topographic_stats_t stats;
    topographic_map_get_stats(map, &stats);

    // Retinotopic should have variable magnification
    EXPECT_GT(stats.max_magnification, stats.min_magnification);

    topographic_map_destroy(map);
}

//=============================================================================
// 11. Edge Cases Tests
//=============================================================================

TEST_F(TopographicMapsTest, EdgeCases_BoundaryInputs) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Test corners
    float corners[8] = {
        0.0f, 0.0f,           // Bottom-left
        100.0f, 0.0f,         // Bottom-right
        0.0f, 100.0f,         // Top-left
        100.0f, 100.0f        // Top-right
    };
    float cortical[8];

    topographic_map_input_to_cortex(map, corners, cortical, 4);

    // All should map to valid cortical positions
    for (int i = 0; i < 8; i++) {
        EXPECT_GE(cortical[i], 0.0f);
    }

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_OutOfRange) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    // Input outside range
    float input[2] = {200.0f, 200.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    // Should still produce output (clamped or extrapolated)
    EXPECT_TRUE(std::isfinite(cortical[0]));
    EXPECT_TRUE(std::isfinite(cortical[1]));

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_NegativeInputs) {
    topographic_map_config_t config = createBasicConfig();
    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[2] = {-50.0f, -50.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    // Should handle gracefully
    EXPECT_TRUE(std::isfinite(cortical[0]));
    EXPECT_TRUE(std::isfinite(cortical[1]));

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_ZeroInputRange) {
    topographic_map_config_t config = createBasicConfig();
    config.input_range[0] = 50.0f;
    config.input_range[1] = 50.0f; // Zero range

    // Should fail validation
    EXPECT_FALSE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, EdgeCases_VerySmallMap) {
    topographic_map_config_t config = createBasicConfig();
    config.cortical_range[0] = 0.0f;
    config.cortical_range[1] = 1.0f;
    config.cortical_range[2] = 0.0f;
    config.cortical_range[3] = 1.0f;

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);

    EXPECT_GT(width, 0u);
    EXPECT_GT(height, 0u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_VeryLargeMap) {
    topographic_map_config_t config = createBasicConfig();
    config.cortical_range[0] = 0.0f;
    config.cortical_range[1] = 1000.0f;
    config.cortical_range[2] = 0.0f;
    config.cortical_range[3] = 1000.0f;

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    uint32_t width, height;
    topographic_map_get_dimensions(map, &width, &height);

    EXPECT_GT(width, 0u);
    EXPECT_GT(height, 0u);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_SingleDimensionInput) {
    topographic_map_config_t config = createBasicConfig();
    config.input_dims = 1;

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[1] = {50.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    EXPECT_TRUE(std::isfinite(cortical[0]));
    EXPECT_TRUE(std::isfinite(cortical[1]));

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_ThreeDimensionalInput) {
    topographic_map_config_t config = createBasicConfig();
    config.input_dims = 3;
    config.input_range[4] = 0.0f;
    config.input_range[5] = 100.0f;

    topographic_map_t* map = topographic_map_create(&config);
    ASSERT_NE(map, nullptr);

    float input[3] = {50.0f, 50.0f, 50.0f};
    float cortical[2];

    topographic_map_input_to_cortex(map, input, cortical, 1);

    EXPECT_TRUE(std::isfinite(cortical[0]));
    EXPECT_TRUE(std::isfinite(cortical[1]));

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, EdgeCases_RetinotopicInvalidParams) {
    retinotopic_params_t params;
    params.foveal_radius = 0.0f; // Invalid
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = TOPOGRAPHIC_RETINOTOPIC;
    config.input_dims = 2;
    config.cortical_dims = 2;
    config.input_range[0] = 0.0f;
    config.input_range[1] = 100.0f;
    config.input_range[2] = 0.0f;
    config.input_range[3] = 100.0f;
    config.cortical_range[0] = 0.0f;
    config.cortical_range[1] = 100.0f;
    config.cortical_range[2] = 0.0f;
    config.cortical_range[3] = 100.0f;
    memcpy(&config.retinotopic, &params, sizeof(params));

    EXPECT_FALSE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, EdgeCases_TonotopicInvalidFrequencies) {
    tonotopic_params_t params;
    params.min_frequency = 1000.0f;
    params.max_frequency = 100.0f; // Invalid: max < min
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = TOPOGRAPHIC_TONOTOPIC;
    config.input_dims = 1;
    config.cortical_dims = 2;
    config.input_range[0] = params.min_frequency;
    config.input_range[1] = params.max_frequency;
    config.cortical_range[0] = 0.0f;
    config.cortical_range[1] = 100.0f;
    config.cortical_range[2] = 0.0f;
    config.cortical_range[3] = 100.0f;
    memcpy(&config.tonotopic, &params, sizeof(params));

    EXPECT_FALSE(topographic_map_validate_config(&config));
}

TEST_F(TopographicMapsTest, EdgeCases_SomatotopicNoRegions) {
    topographic_map_config_t config;
    memset(&config, 0, sizeof(config));
    config.type = TOPOGRAPHIC_SOMATOTOPIC;
    config.input_dims = 1;
    config.cortical_dims = 2;
    config.input_range[0] = 0.0f;
    config.input_range[1] = 100.0f;
    config.cortical_range[0] = 0.0f;
    config.cortical_range[1] = 100.0f;
    config.cortical_range[2] = 0.0f;
    config.cortical_range[3] = 100.0f;
    config.somatotopic.num_regions = 0; // Invalid

    EXPECT_FALSE(topographic_map_validate_config(&config));
}

//=============================================================================
// 12. Mathematical Model Tests
//=============================================================================

TEST_F(TopographicMapsTest, MathModel_Retinotopic_LogPolarProperties) {
    retinotopic_params_t params;
    params.foveal_radius = 2.0f;
    params.cortical_magnification = 10.0f;
    params.log_polar_a = 0.5f;
    params.aspect_ratio = 1.0f;
    params.eccentricity_half = 5.0f;
    params.angle_coverage = 2.0f * PI;

    topographic_map_t* map = topographic_map_create_retinotopic(&params, 100, 100);
    ASSERT_NE(map, nullptr);

    // Test log-polar: Equal eccentricity ratios map to equal cortical distances
    float ecc1[2] = {1.0f, 0.0f};
    float ecc2[2] = {2.0f, 0.0f};
    float ecc3[2] = {4.0f, 0.0f};

    float cort1[2], cort2[2], cort3[2];

    topographic_map_input_to_cortex(map, ecc1, cort1, 1);
    topographic_map_input_to_cortex(map, ecc2, cort2, 1);
    topographic_map_input_to_cortex(map, ecc3, cort3, 1);

    float dist1 = cort2[0] - cort1[0];
    float dist2 = cort3[0] - cort2[0];

    // Ratio 2:1 should produce equal cortical distances in log space
    EXPECT_NEAR_FLOAT(dist1, dist2, 2.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, MathModel_Tonotopic_OctaveSpacing) {
    tonotopic_params_t params;
    params.min_frequency = 100.0f;
    params.max_frequency = 12800.0f;
    params.octave_span = 1.0f;
    params.is_logarithmic = true;
    params.q_factor = 3.0f;

    topographic_map_t* map = topographic_map_create_tonotopic(&params, 128);
    ASSERT_NE(map, nullptr);

    // Test octave spacing: Each doubling of frequency adds constant cortical distance
    float f1[1] = {200.0f};
    float f2[1] = {400.0f};  // 1 octave up
    float f3[1] = {800.0f};  // 2 octaves up

    float c1[2], c2[2], c3[2];

    topographic_map_input_to_cortex(map, f1, c1, 1);
    topographic_map_input_to_cortex(map, f2, c2, 1);
    topographic_map_input_to_cortex(map, f3, c3, 1);

    float octave_spacing1 = c2[0] - c1[0];
    float octave_spacing2 = c3[0] - c2[0];

    EXPECT_NEAR_FLOAT(octave_spacing1, octave_spacing2, 2.0f);

    topographic_map_destroy(map);
}

TEST_F(TopographicMapsTest, MathModel_Somatotopic_PiecewiseLinear) {
    topographic_map_t* map = topographic_map_create_somatotopic(1);
    ASSERT_NE(map, nullptr);

    somatotopic_region_t region;
    strncpy(region.name, "test", sizeof(region.name));
    region.input_start = 0.0f;
    region.input_end = 10.0f;
    region.cortical_start = 0.0f;
    region.cortical_end = 20.0f;
    region.magnification = 2.0f;
    region.receptor_density = 100.0f;

    EXPECT_TRUE(topographic_map_add_body_region(map, &region));

    // Test linearity within region
    float in1[1] = {2.5f};
    float in2[1] = {5.0f};
    float in3[1] = {7.5f};

    float c1[2], c2[2], c3[2];

    topographic_map_input_to_cortex(map, in1, c1, 1);
    topographic_map_input_to_cortex(map, in2, c2, 1);
    topographic_map_input_to_cortex(map, in3, c3, 1);

    // Check linear spacing
    float spacing1 = c2[0] - c1[0];
    float spacing2 = c3[0] - c2[0];

    EXPECT_NEAR_FLOAT(spacing1, spacing2, 1.0f);

    topographic_map_destroy(map);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
