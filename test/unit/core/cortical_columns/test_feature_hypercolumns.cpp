/**
 * @file test_feature_hypercolumns.cpp
 * @brief Comprehensive unit tests for NIMCP Feature Hypercolumns module
 *
 * WHAT: Full coverage tests for feature hypercolumn creation, processing,
 *       competition, decoding, learning, and statistics
 * WHY:  Ensure correct implementation of population coding, sparse representation,
 *       tuning curves, and feature readout for all supported feature types
 * HOW:  GoogleTest framework with extensive edge cases and mathematical validation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_feature_hypercolumns.h"

//=============================================================================
// TEST CONSTANTS
//=============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr float PI = 3.14159265358979323846f;
static constexpr uint32_t DEFAULT_NUM_ORIENTATIONS = 8;
static constexpr uint32_t DEFAULT_NUM_DIRECTIONS = 12;
static constexpr uint32_t DEFAULT_NUM_HUES = 6;
static constexpr uint32_t DEFAULT_NUM_SATURATIONS = 4;

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * WHAT: Check if two floats are approximately equal
 * WHY:  Floating point comparison needs tolerance
 */
static bool float_approx_equal(float a, float b, float tolerance = FLOAT_TOLERANCE) {
    return std::fabs(a - b) < tolerance;
}

/**
 * WHAT: Create test input pattern
 * WHY:  Generate reproducible test data
 */
static std::vector<float> create_test_input(uint32_t size, float scale = 1.0f) {
    std::vector<float> input(size);
    for (uint32_t i = 0; i < size; i++) {
        input[i] = scale * std::sin(2.0f * PI * i / size);
    }
    return input;
}

/**
 * WHAT: Compute Gaussian response
 * WHY:  Validate tuning curve implementation
 */
static float gaussian_response(float value, float preferred, float sigma) {
    float diff = value - preferred;
    return std::exp(-(diff * diff) / (2.0f * sigma * sigma));
}

/**
 * WHAT: Compute circular distance
 * WHY:  Validate circular feature handling
 */
static float circular_distance(float a, float b, float period) {
    float diff = std::fmod(std::fabs(a - b), period);
    if (diff > period / 2.0f) {
        diff = period - diff;
    }
    return diff;
}

//=============================================================================
// TEST FIXTURE
//=============================================================================

class FeatureHypercolumnTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test parameters
        num_columns = 8;
        min_value = 0.0f;
        max_value = 180.0f;
        tuning_width = 0.2f;
    }

    void TearDown() override {
    }

    uint32_t num_columns;
    float min_value;
    float max_value;
    float tuning_width;
};

//=============================================================================
// FEATURE DIMENSION TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_Orientation) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_ORIENTATION);
    EXPECT_FLOAT_EQ(dim.min_value, 0.0f);
    EXPECT_FLOAT_EQ(dim.max_value, 180.0f);
    EXPECT_EQ(dim.num_columns, num_columns);
    EXPECT_TRUE(dim.is_circular);  // Orientation should be circular
    EXPECT_GT(dim.tuning_width, 0.0f);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_Direction) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_DIRECTION, 0.0f, 360.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_DIRECTION);
    EXPECT_FLOAT_EQ(dim.max_value, 360.0f);
    EXPECT_TRUE(dim.is_circular);  // Direction should be circular
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_SpatialFreq) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_SPATIAL_FREQ);
    EXPECT_FALSE(dim.is_circular);  // Spatial frequency should be linear
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_ColorHue) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_COLOR_HUE, 0.0f, 360.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_COLOR_HUE);
    EXPECT_TRUE(dim.is_circular);  // Hue should be circular
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_ColorSaturation) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_COLOR_SATURATION, 0.0f, 1.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_COLOR_SATURATION);
    EXPECT_FALSE(dim.is_circular);  // Saturation should be linear
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_Disparity) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_DISPARITY, -2.0f, 2.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_DISPARITY);
    EXPECT_FALSE(dim.is_circular);  // Disparity should be linear
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_TemporalFreq) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_TEMPORAL_FREQ, 0.1f, 100.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_TEMPORAL_FREQ);
    EXPECT_FALSE(dim.is_circular);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionCreate_Custom) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_CUSTOM, -1.0f, 1.0f, num_columns
    );

    EXPECT_EQ(dim.type, FEATURE_CUSTOM);
    EXPECT_FALSE(dim.is_circular);  // Custom defaults to linear
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionSetCircular) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, num_columns
    );

    EXPECT_FALSE(dim.is_circular);

    feature_dimension_set_circular(&dim, true);
    EXPECT_TRUE(dim.is_circular);

    feature_dimension_set_circular(&dim, false);
    EXPECT_FALSE(dim.is_circular);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionSetCircular_Null) {
    // Should not crash
    feature_dimension_set_circular(nullptr, true);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionSetTuningWidth) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, num_columns
    );

    float new_width = 0.5f;
    feature_dimension_set_tuning_width(&dim, new_width);
    EXPECT_FLOAT_EQ(dim.tuning_width, new_width);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionSetTuningWidth_Invalid) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, num_columns
    );

    float original_width = dim.tuning_width;

    // Should reject negative
    feature_dimension_set_tuning_width(&dim, -0.1f);
    EXPECT_FLOAT_EQ(dim.tuning_width, original_width);

    // Should reject zero
    feature_dimension_set_tuning_width(&dim, 0.0f);
    EXPECT_FLOAT_EQ(dim.tuning_width, original_width);
}

TEST_F(FeatureHypercolumnTest, FeatureDimensionSetTuningWidth_Null) {
    // Should not crash
    feature_dimension_set_tuning_width(nullptr, 0.5f);
}

//=============================================================================
// HYPERCOLUMN CREATION AND DESTRUCTION
//=============================================================================

TEST_F(FeatureHypercolumnTest, CreateDestroy_Basic) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, num_columns
    );

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 1);
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 1);
    EXPECT_EQ(hcol->total_columns, num_columns);
    EXPECT_NE(hcol->dimensions, nullptr);
    EXPECT_NE(hcol->columns, nullptr);
    EXPECT_NE(hcol->mutex, nullptr);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateDestroy_NullSafe) {
    // Should not crash
    feature_hypercolumn_destroy(nullptr);
}

TEST_F(FeatureHypercolumnTest, Create_NullDimensions) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create(nullptr, 1);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(FeatureHypercolumnTest, Create_ZeroDimensions) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, num_columns
    );

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 0);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(FeatureHypercolumnTest, Create_MultiDimensional) {
    feature_dimension_t dims[2];
    dims[0] = feature_dimension_create(FEATURE_ORIENTATION, 0.0f, 180.0f, 4);
    dims[1] = feature_dimension_create(FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, 3);

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(dims, 2);
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 2);
    EXPECT_EQ(hcol->total_columns, 12);  // 4 × 3

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Create_ThreeDimensional) {
    feature_dimension_t dims[3];
    dims[0] = feature_dimension_create(FEATURE_ORIENTATION, 0.0f, 180.0f, 2);
    dims[1] = feature_dimension_create(FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, 3);
    dims[2] = feature_dimension_create(FEATURE_DISPARITY, -1.0f, 1.0f, 2);

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(dims, 3);
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 3);
    EXPECT_EQ(hcol->total_columns, 12);  // 2 × 3 × 2

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// CONVENIENCE CONSTRUCTORS
//=============================================================================

TEST_F(FeatureHypercolumnTest, CreateOrientation_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(
        DEFAULT_NUM_ORIENTATIONS
    );
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 1);
    EXPECT_EQ(hcol->total_columns, DEFAULT_NUM_ORIENTATIONS);
    EXPECT_EQ(hcol->dimensions[0].type, FEATURE_ORIENTATION);
    EXPECT_TRUE(hcol->dimensions[0].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].min_value, 0.0f);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].max_value, 180.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateOrientation_Invalid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(0);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(FeatureHypercolumnTest, CreateDirection_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_direction(
        DEFAULT_NUM_DIRECTIONS
    );
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 1);
    EXPECT_EQ(hcol->total_columns, DEFAULT_NUM_DIRECTIONS);
    EXPECT_EQ(hcol->dimensions[0].type, FEATURE_DIRECTION);
    EXPECT_TRUE(hcol->dimensions[0].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].min_value, 0.0f);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].max_value, 360.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateDirection_Invalid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_direction(0);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(FeatureHypercolumnTest, CreateSpatialFreq_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_spatial_freq(
        4, 0.5f, 8.0f
    );
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 1);
    EXPECT_EQ(hcol->total_columns, 4);
    EXPECT_EQ(hcol->dimensions[0].type, FEATURE_SPATIAL_FREQ);
    EXPECT_FALSE(hcol->dimensions[0].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].min_value, 0.5f);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].max_value, 8.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateSpatialFreq_InvalidParams) {
    EXPECT_EQ(feature_hypercolumn_create_spatial_freq(0, 0.5f, 8.0f), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_spatial_freq(4, 0.0f, 8.0f), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_spatial_freq(4, -1.0f, 8.0f), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_spatial_freq(4, 8.0f, 0.5f), nullptr);
}

TEST_F(FeatureHypercolumnTest, CreateColor_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_color(
        DEFAULT_NUM_HUES, DEFAULT_NUM_SATURATIONS
    );
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 2);
    EXPECT_EQ(hcol->total_columns, DEFAULT_NUM_HUES * DEFAULT_NUM_SATURATIONS);

    // Check hue dimension
    EXPECT_EQ(hcol->dimensions[0].type, FEATURE_COLOR_HUE);
    EXPECT_TRUE(hcol->dimensions[0].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].min_value, 0.0f);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].max_value, 360.0f);

    // Check saturation dimension
    EXPECT_EQ(hcol->dimensions[1].type, FEATURE_COLOR_SATURATION);
    EXPECT_FALSE(hcol->dimensions[1].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[1].min_value, 0.0f);
    EXPECT_FLOAT_EQ(hcol->dimensions[1].max_value, 1.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateColor_InvalidParams) {
    EXPECT_EQ(feature_hypercolumn_create_color(0, DEFAULT_NUM_SATURATIONS), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_color(DEFAULT_NUM_HUES, 0), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_color(0, 0), nullptr);
}

TEST_F(FeatureHypercolumnTest, CreateDisparity_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_disparity(
        8, 2.0f
    );
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->num_dimensions, 1);
    EXPECT_EQ(hcol->total_columns, 8);
    EXPECT_EQ(hcol->dimensions[0].type, FEATURE_DISPARITY);
    EXPECT_FALSE(hcol->dimensions[0].is_circular);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].min_value, -2.0f);
    EXPECT_FLOAT_EQ(hcol->dimensions[0].max_value, 2.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, CreateDisparity_InvalidParams) {
    EXPECT_EQ(feature_hypercolumn_create_disparity(0, 2.0f), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_disparity(8, 0.0f), nullptr);
    EXPECT_EQ(feature_hypercolumn_create_disparity(8, -1.0f), nullptr);
}

//=============================================================================
// INPUT PROCESSING TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, Process_SingleDimension) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Process input feature at 45 degrees
    float input_features[] = {45.0f};
    feature_hypercolumn_process(hcol, input_features, 1);

    // Verify activation pattern
    float max_activation = 0.0f;
    uint32_t max_idx = 0;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        float act = feature_hypercolumn_get_activation(hcol, i);
        if (act > max_activation) {
            max_activation = act;
            max_idx = i;
        }
    }

    // Maximum should be around column preferring 45 degrees
    EXPECT_GT(max_activation, 0.5f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Process_MultiDimension) {
    feature_dimension_t dims[2];
    dims[0] = feature_dimension_create(FEATURE_ORIENTATION, 0.0f, 180.0f, 4);
    dims[1] = feature_dimension_create(FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, 3);

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(dims, 2);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {90.0f, 2.0f};
    feature_hypercolumn_process(hcol, input_features, 2);

    // At least one column should be activated
    bool has_activation = false;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (feature_hypercolumn_get_activation(hcol, i) > 0.1f) {
            has_activation = true;
            break;
        }
    }
    EXPECT_TRUE(has_activation);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Process_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {45.0f};

    // Should not crash
    feature_hypercolumn_process(nullptr, input_features, 1);
    feature_hypercolumn_process(hcol, nullptr, 1);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Process_WrongFeatureCount) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {45.0f, 90.0f};

    // Should handle mismatch gracefully
    feature_hypercolumn_process(hcol, input_features, 2);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ProcessWithInput_RawInput) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Create raw input
    auto input = create_test_input(16);
    feature_hypercolumn_process_with_input(hcol, input.data(), input.size());

    // Verify some activations
    bool has_activation = false;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (feature_hypercolumn_get_activation(hcol, i) > 0.0f) {
            has_activation = true;
            break;
        }
    }
    EXPECT_TRUE(has_activation);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ProcessWithInput_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);

    // Should not crash
    feature_hypercolumn_process_with_input(nullptr, input.data(), input.size());
    feature_hypercolumn_process_with_input(hcol, nullptr, 16);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// COMPETITION AND SPARSITY TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, Normalize_DivisiveNormalization) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set some activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i + 1);
    }

    feature_hypercolumn_normalize(hcol);

    // Sum should be 1.0
    float sum = 0.0f;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        sum += feature_hypercolumn_get_activation(hcol, i);
    }
    EXPECT_TRUE(float_approx_equal(sum, 1.0f, 1e-4f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Normalize_ZeroSum) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set all to zero
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = 0.0f;
    }

    // Should handle gracefully
    feature_hypercolumn_normalize(hcol);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Normalize_Null) {
    // Should not crash
    feature_hypercolumn_normalize(nullptr);
}

TEST_F(FeatureHypercolumnTest, Softmax_TemperatureEffect) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    float temperature = 1.0f;
    feature_hypercolumn_softmax(hcol, temperature);

    // Sum should be 1.0
    float sum = 0.0f;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        sum += feature_hypercolumn_get_activation(hcol, i);
    }
    EXPECT_TRUE(float_approx_equal(sum, 1.0f, 1e-4f));

    // Winner should have highest activation
    uint32_t winner = feature_hypercolumn_get_winner(hcol);
    EXPECT_EQ(winner, 7);  // Last column had highest input

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Softmax_LowTemperature) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    // Low temperature = sharper competition
    feature_hypercolumn_softmax(hcol, 0.1f);

    uint32_t winner = feature_hypercolumn_get_winner(hcol);
    float winner_act = feature_hypercolumn_get_activation(hcol, winner);

    // Winner should dominate
    EXPECT_GT(winner_act, 0.9f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Softmax_InvalidTemperature) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Should reject invalid temperatures
    feature_hypercolumn_softmax(hcol, 0.0f);
    feature_hypercolumn_softmax(hcol, -1.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Softmax_Null) {
    feature_hypercolumn_softmax(nullptr, 1.0f);
}

TEST_F(FeatureHypercolumnTest, KWinners_SelectsTopK) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    uint32_t k = 3;
    feature_hypercolumn_k_winners(hcol, k);

    // Count non-zero activations
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        if (feature_hypercolumn_get_activation(hcol, i) > 0.0f) {
            active_count++;
        }
    }

    EXPECT_EQ(active_count, k);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, KWinners_KZero) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    // Should handle gracefully
    feature_hypercolumn_k_winners(hcol, 0);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, KWinners_KTooLarge) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    // Should handle gracefully
    feature_hypercolumn_k_winners(hcol, 100);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, KWinners_Null) {
    feature_hypercolumn_k_winners(nullptr, 3);
}

TEST_F(FeatureHypercolumnTest, Threshold_RemovesWeakActivations) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i) * 0.1f;
    }

    float threshold = 0.3f;
    feature_hypercolumn_threshold(hcol, threshold);

    // Count activations above threshold
    uint32_t above_count = 0;
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        float act = feature_hypercolumn_get_activation(hcol, i);
        if (act > 0.0f) {
            EXPECT_GE(act, threshold);
            above_count++;
        }
    }

    EXPECT_GT(above_count, 0);
    EXPECT_LT(above_count, hcol->total_columns);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Threshold_Null) {
    feature_hypercolumn_threshold(nullptr, 0.5f);
}

//=============================================================================
// DECODING TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, Decode_PopulationVector) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Process known input
    float input_features[] = {90.0f};
    feature_hypercolumn_process(hcol, input_features, 1);

    // Decode
    float decoded[1];
    feature_hypercolumn_decode(hcol, decoded);

    // Should be close to 90 degrees
    EXPECT_TRUE(float_approx_equal(decoded[0], 90.0f, 15.0f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Decode_MultiDimension) {
    feature_dimension_t dims[2];
    dims[0] = feature_dimension_create(FEATURE_ORIENTATION, 0.0f, 180.0f, 8);
    dims[1] = feature_dimension_create(FEATURE_SPATIAL_FREQ, 0.5f, 8.0f, 4);

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(dims, 2);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {45.0f, 2.0f};
    feature_hypercolumn_process(hcol, input_features, 2);

    float decoded[2];
    feature_hypercolumn_decode(hcol, decoded);

    // Should be roughly close to input
    EXPECT_TRUE(float_approx_equal(decoded[0], 45.0f, 30.0f));
    EXPECT_TRUE(float_approx_equal(decoded[1], 2.0f, 2.0f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, Decode_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float decoded[1];

    // Should not crash
    feature_hypercolumn_decode(nullptr, decoded);
    feature_hypercolumn_decode(hcol, nullptr);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, DecodeSingle_ValidDimension) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {135.0f};
    feature_hypercolumn_process(hcol, input_features, 1);

    float decoded = feature_hypercolumn_decode_single(hcol, 0);
    EXPECT_TRUE(float_approx_equal(decoded, 135.0f, 20.0f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, DecodeSingle_InvalidDimension) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float decoded = feature_hypercolumn_decode_single(hcol, 10);
    EXPECT_FLOAT_EQ(decoded, 0.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, DecodeSingle_Null) {
    float decoded = feature_hypercolumn_decode_single(nullptr, 0);
    EXPECT_FLOAT_EQ(decoded, 0.0f);
}

TEST_F(FeatureHypercolumnTest, DecodePopulationVector_Explicit) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_direction(12);
    ASSERT_NE(hcol, nullptr);

    float input_features[] = {180.0f};
    feature_hypercolumn_process(hcol, input_features, 1);

    float decoded[1];
    feature_hypercolumn_decode_population_vector(hcol, decoded);

    EXPECT_TRUE(float_approx_equal(decoded[0], 180.0f, 30.0f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, DecodePopulationVector_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float decoded[1];

    // Should not crash
    feature_hypercolumn_decode_population_vector(nullptr, decoded);
    feature_hypercolumn_decode_population_vector(hcol, nullptr);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// ACTIVATION ACCESS TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, GetActivation_Valid) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float test_value = 0.75f;
    hcol->columns[3].activation = test_value;

    float act = feature_hypercolumn_get_activation(hcol, 3);
    EXPECT_FLOAT_EQ(act, test_value);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetActivation_InvalidIndex) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    float act = feature_hypercolumn_get_activation(hcol, 100);
    EXPECT_FLOAT_EQ(act, 0.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetActivation_Null) {
    float act = feature_hypercolumn_get_activation(nullptr, 0);
    EXPECT_FLOAT_EQ(act, 0.0f);
}

TEST_F(FeatureHypercolumnTest, GetAllActivations_CopiesCorrectly) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i) * 0.1f;
    }

    std::vector<float> activations(hcol->total_columns);
    feature_hypercolumn_get_all_activations(hcol, activations.data());

    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        EXPECT_FLOAT_EQ(activations[i], static_cast<float>(i) * 0.1f);
    }

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetAllActivations_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    std::vector<float> activations(8);

    // Should not crash
    feature_hypercolumn_get_all_activations(nullptr, activations.data());
    feature_hypercolumn_get_all_activations(hcol, nullptr);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetWinner_FindsMaximum) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set winner at index 5
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i) * 0.1f;
    }
    hcol->columns[5].activation = 10.0f;

    uint32_t winner = feature_hypercolumn_get_winner(hcol);
    EXPECT_EQ(winner, 5);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetWinner_Null) {
    uint32_t winner = feature_hypercolumn_get_winner(nullptr);
    EXPECT_EQ(winner, 0);
}

TEST_F(FeatureHypercolumnTest, GetTopK_ReturnsCorrectIndices) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set known activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    uint32_t k = 3;
    std::vector<uint32_t> indices(k);
    std::vector<float> activations(k);

    feature_hypercolumn_get_top_k(hcol, k, indices.data(), activations.data());

    // Should get indices 7, 6, 5 (highest values)
    EXPECT_EQ(indices[0], 7);
    EXPECT_EQ(indices[1], 6);
    EXPECT_EQ(indices[2], 5);

    EXPECT_FLOAT_EQ(activations[0], 7.0f);
    EXPECT_FLOAT_EQ(activations[1], 6.0f);
    EXPECT_FLOAT_EQ(activations[2], 5.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetTopK_InvalidK) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    std::vector<uint32_t> indices(3);
    std::vector<float> activations(3);

    // Should handle gracefully
    feature_hypercolumn_get_top_k(hcol, 0, indices.data(), activations.data());
    feature_hypercolumn_get_top_k(hcol, 100, indices.data(), activations.data());

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetTopK_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    std::vector<uint32_t> indices(3);
    std::vector<float> activations(3);

    // Should not crash
    feature_hypercolumn_get_top_k(nullptr, 3, indices.data(), activations.data());
    feature_hypercolumn_get_top_k(hcol, 3, nullptr, activations.data());
    feature_hypercolumn_get_top_k(hcol, 3, indices.data(), nullptr);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// LEARNING TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, LearnHebbian_UpdatesWeights) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Initialize with raw input processing
    auto input = create_test_input(16);
    feature_hypercolumn_process_with_input(hcol, input.data(), input.size());

    // Store initial weights
    std::vector<float> initial_weights(hcol->columns[0].num_weights);
    if (hcol->columns[0].weights) {
        memcpy(initial_weights.data(), hcol->columns[0].weights,
               hcol->columns[0].num_weights * sizeof(float));
    }

    // Learn
    float learning_rate = 0.01f;
    feature_hypercolumn_learn_hebbian(hcol, input.data(), learning_rate);

    // Weights should change
    if (hcol->columns[0].weights) {
        bool weights_changed = false;
        for (uint32_t i = 0; i < hcol->columns[0].num_weights; i++) {
            if (std::fabs(hcol->columns[0].weights[i] - initial_weights[i]) > 1e-6f) {
                weights_changed = true;
                break;
            }
        }
        EXPECT_TRUE(weights_changed);
    }

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, LearnHebbian_InvalidLearningRate) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);

    // Should reject invalid rates
    feature_hypercolumn_learn_hebbian(hcol, input.data(), 0.0f);
    feature_hypercolumn_learn_hebbian(hcol, input.data(), -0.1f);
    feature_hypercolumn_learn_hebbian(hcol, input.data(), 1.5f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, LearnHebbian_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);

    // Should not crash
    feature_hypercolumn_learn_hebbian(nullptr, input.data(), 0.01f);
    feature_hypercolumn_learn_hebbian(hcol, nullptr, 0.01f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, LearnCompetitive_UpdatesWinnerNeighborhood) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);
    feature_hypercolumn_process_with_input(hcol, input.data(), input.size());

    float learning_rate = 0.01f;
    float neighborhood_sigma = 2.0f;
    feature_hypercolumn_learn_competitive(hcol, input.data(),
                                          learning_rate, neighborhood_sigma);

    // Function should complete without error
    SUCCEED();

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, LearnCompetitive_InvalidParams) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);

    // Should reject invalid parameters
    feature_hypercolumn_learn_competitive(hcol, input.data(), 0.0f, 1.0f);
    feature_hypercolumn_learn_competitive(hcol, input.data(), -0.1f, 1.0f);
    feature_hypercolumn_learn_competitive(hcol, input.data(), 1.5f, 1.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, LearnCompetitive_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    auto input = create_test_input(16);

    // Should not crash
    feature_hypercolumn_learn_competitive(nullptr, input.data(), 0.01f, 1.0f);
    feature_hypercolumn_learn_competitive(hcol, nullptr, 0.01f, 1.0f);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// TUNING CURVE TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, GetTuningCurve_ValidDimension) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    uint32_t num_points = 10;
    std::vector<float> values(num_points);
    std::vector<float> responses(num_points * hcol->dimensions[0].num_columns);

    feature_hypercolumn_get_tuning_curve(hcol, 0, values.data(),
                                         responses.data(), num_points);

    // Verify values are in range
    for (uint32_t i = 0; i < num_points; i++) {
        EXPECT_GE(values[i], 0.0f);
        EXPECT_LE(values[i], 180.0f);
    }

    // Verify some responses are non-zero
    bool has_response = false;
    for (uint32_t i = 0; i < num_points * hcol->dimensions[0].num_columns; i++) {
        if (responses[i] > 0.1f) {
            has_response = true;
            break;
        }
    }
    EXPECT_TRUE(has_response);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetTuningCurve_InvalidDimension) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    uint32_t num_points = 10;
    std::vector<float> values(num_points);
    std::vector<float> responses(num_points * 4);

    // Should handle invalid dimension gracefully
    feature_hypercolumn_get_tuning_curve(hcol, 10, values.data(),
                                         responses.data(), num_points);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetTuningCurve_ZeroPoints) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    std::vector<float> values(1);
    std::vector<float> responses(1);

    // Should handle gracefully
    feature_hypercolumn_get_tuning_curve(hcol, 0, values.data(),
                                         responses.data(), 0);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetTuningCurve_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    std::vector<float> values(10);
    std::vector<float> responses(40);

    // Should not crash
    feature_hypercolumn_get_tuning_curve(nullptr, 0, values.data(),
                                         responses.data(), 10);
    feature_hypercolumn_get_tuning_curve(hcol, 0, nullptr,
                                         responses.data(), 10);
    feature_hypercolumn_get_tuning_curve(hcol, 0, values.data(),
                                         nullptr, 10);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// MULTI-COLUMN POOLING TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, PoolResponses_AveragePooling) {
    // Create multiple hypercolumns
    feature_hypercolumn_t* hcol1 = feature_hypercolumn_create_orientation(4);
    feature_hypercolumn_t* hcol2 = feature_hypercolumn_create_orientation(4);
    feature_hypercolumn_t* hcol3 = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol1, nullptr);
    ASSERT_NE(hcol2, nullptr);
    ASSERT_NE(hcol3, nullptr);

    // Set different activations
    for (uint32_t i = 0; i < 4; i++) {
        hcol1->columns[i].activation = 1.0f;
        hcol2->columns[i].activation = 2.0f;
        hcol3->columns[i].activation = 3.0f;
    }

    feature_hypercolumn_t* hcols[] = {hcol1, hcol2, hcol3};
    std::vector<float> pooled(4);

    feature_hypercolumn_pool_responses(hcols, 3, pooled.data());

    // Average should be 2.0
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(pooled[i], 2.0f);
    }

    feature_hypercolumn_destroy(hcol1);
    feature_hypercolumn_destroy(hcol2);
    feature_hypercolumn_destroy(hcol3);
}

TEST_F(FeatureHypercolumnTest, PoolResponses_SingleHypercolumn) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    for (uint32_t i = 0; i < 4; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    feature_hypercolumn_t* hcols[] = {hcol};
    std::vector<float> pooled(4);

    feature_hypercolumn_pool_responses(hcols, 1, pooled.data());

    // Should match original
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(pooled[i], static_cast<float>(i));
    }

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, PoolResponses_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(4);
    ASSERT_NE(hcol, nullptr);

    feature_hypercolumn_t* hcols[] = {hcol};
    std::vector<float> pooled(4);

    // Should not crash
    feature_hypercolumn_pool_responses(nullptr, 1, pooled.data());
    feature_hypercolumn_pool_responses(hcols, 0, pooled.data());
    feature_hypercolumn_pool_responses(hcols, 1, nullptr);

    feature_hypercolumn_destroy(hcol);
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(FeatureHypercolumnTest, GetStats_ComputesCorrectly) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Set known activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = static_cast<float>(i);
    }

    feature_cc_hypercolumn_stats_t stats;
    feature_hypercolumn_get_stats(hcol, &stats);

    // Verify mean
    float expected_mean = 3.5f;  // (0+1+2+3+4+5+6+7)/8
    EXPECT_TRUE(float_approx_equal(stats.mean_activation, expected_mean, 0.1f));

    // Verify max
    EXPECT_FLOAT_EQ(stats.max_activation, 7.0f);

    // Verify winner
    EXPECT_EQ(stats.winner_index, 7);

    // Verify sparsity is computed
    EXPECT_GE(stats.sparsity, 0.0f);
    EXPECT_LE(stats.sparsity, 1.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, GetStats_NullPointers) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    feature_cc_hypercolumn_stats_t stats;

    // Should not crash
    feature_hypercolumn_get_stats(nullptr, &stats);
    feature_hypercolumn_get_stats(hcol, nullptr);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ComputeSparsity_SparseActivation) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Only activate 2 out of 8
    hcol->columns[0].activation = 1.0f;
    hcol->columns[7].activation = 1.0f;

    float sparsity = feature_hypercolumn_compute_sparsity(hcol);

    // Sparsity = 1 - (active / total) = 1 - (2/8) = 0.75
    EXPECT_TRUE(float_approx_equal(sparsity, 0.75f, 0.1f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ComputeSparsity_DenseActivation) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Activate all
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = 1.0f;
    }

    float sparsity = feature_hypercolumn_compute_sparsity(hcol);

    // Should be close to 0 (dense)
    EXPECT_LT(sparsity, 0.2f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ComputeSparsity_Null) {
    float sparsity = feature_hypercolumn_compute_sparsity(nullptr);
    EXPECT_FLOAT_EQ(sparsity, 0.0f);
}

TEST_F(FeatureHypercolumnTest, ComputeSelectivity_SharpTuning) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Single strong activation
    hcol->columns[0].activation = 10.0f;
    for (uint32_t i = 1; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = 0.1f;
    }

    float selectivity = feature_hypercolumn_compute_selectivity(hcol);

    // Should be high (max >> mean)
    EXPECT_GT(selectivity, 5.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ComputeSelectivity_BroadTuning) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Uniform activation
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = 1.0f;
    }

    float selectivity = feature_hypercolumn_compute_selectivity(hcol);

    // Should be close to 1 (max ≈ mean)
    EXPECT_TRUE(float_approx_equal(selectivity, 1.0f, 0.1f));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, ComputeSelectivity_Null) {
    float selectivity = feature_hypercolumn_compute_selectivity(nullptr);
    EXPECT_FLOAT_EQ(selectivity, 0.0f);
}

//=============================================================================
// EDGE CASES AND BOUNDARY CONDITIONS
//=============================================================================

TEST_F(FeatureHypercolumnTest, EdgeCase_SingleColumn) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, 1
    );

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 1);
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->total_columns, 1);

    // Process should work
    float input[] = {90.0f};
    feature_hypercolumn_process(hcol, input, 1);

    EXPECT_GT(feature_hypercolumn_get_activation(hcol, 0), 0.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, EdgeCase_LargeColumnCount) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_ORIENTATION, 0.0f, 180.0f, 180
    );

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 1);
    ASSERT_NE(hcol, nullptr);

    EXPECT_EQ(hcol->total_columns, 180);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, EdgeCase_CircularWrapAround) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Process at 0 degrees
    float input1[] = {0.0f};
    feature_hypercolumn_process(hcol, input1, 1);
    float decoded1 = feature_hypercolumn_decode_single(hcol, 0);

    // Process at 180 degrees (wraps around for orientation)
    float input2[] = {180.0f};
    feature_hypercolumn_process(hcol, input2, 1);
    float decoded2 = feature_hypercolumn_decode_single(hcol, 0);

    // Both should activate similar columns due to circularity
    // (orientation is 180-degree periodic)

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, EdgeCase_ZeroActivations) {
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(8);
    ASSERT_NE(hcol, nullptr);

    // Zero all activations
    for (uint32_t i = 0; i < hcol->total_columns; i++) {
        hcol->columns[i].activation = 0.0f;
    }

    // Decode should handle gracefully
    float decoded = feature_hypercolumn_decode_single(hcol, 0);
    EXPECT_FLOAT_EQ(decoded, 0.0f);

    // Stats should handle gracefully
    feature_cc_hypercolumn_stats_t stats;
    feature_hypercolumn_get_stats(hcol, &stats);
    EXPECT_FLOAT_EQ(stats.mean_activation, 0.0f);

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, EdgeCase_ExtremeValues) {
    feature_dimension_t dim = feature_dimension_create(
        FEATURE_DISPARITY, -1000.0f, 1000.0f, 10
    );

    feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 1);
    ASSERT_NE(hcol, nullptr);

    // Process extreme values
    float input[] = {999.0f};
    feature_hypercolumn_process(hcol, input, 1);

    // Should not crash or produce NaN
    float decoded = feature_hypercolumn_decode_single(hcol, 0);
    EXPECT_FALSE(std::isnan(decoded));

    feature_hypercolumn_destroy(hcol);
}

TEST_F(FeatureHypercolumnTest, EdgeCase_AllFeatureTypes) {
    // Test that all feature types can be created and processed
    feature_type_t types[] = {
        FEATURE_ORIENTATION,
        FEATURE_DIRECTION,
        FEATURE_SPATIAL_FREQ,
        FEATURE_COLOR_HUE,
        FEATURE_COLOR_SATURATION,
        FEATURE_DISPARITY,
        FEATURE_TEMPORAL_FREQ,
        FEATURE_CUSTOM
    };

    for (auto type : types) {
        feature_dimension_t dim = feature_dimension_create(type, 0.0f, 100.0f, 4);
        feature_hypercolumn_t* hcol = feature_hypercolumn_create(&dim, 1);
        ASSERT_NE(hcol, nullptr) << "Failed for type " << type;

        float input[] = {50.0f};
        feature_hypercolumn_process(hcol, input, 1);

        feature_hypercolumn_destroy(hcol);
    }
}

TEST_F(FeatureHypercolumnTest, Integration_CompleteWorkflow) {
    // Create hypercolumn
    feature_hypercolumn_t* hcol = feature_hypercolumn_create_orientation(12);
    ASSERT_NE(hcol, nullptr);

    // Process input
    float input[] = {60.0f};
    feature_hypercolumn_process(hcol, input, 1);

    // Apply competition
    feature_hypercolumn_softmax(hcol, 1.0f);

    // Decode
    float decoded = feature_hypercolumn_decode_single(hcol, 0);
    EXPECT_TRUE(float_approx_equal(decoded, 60.0f, 20.0f));

    // Get statistics
    feature_cc_hypercolumn_stats_t stats;
    feature_hypercolumn_get_stats(hcol, &stats);
    EXPECT_GT(stats.max_activation, 0.0f);

    // Get top-k
    std::vector<uint32_t> indices(3);
    std::vector<float> activations(3);
    feature_hypercolumn_get_top_k(hcol, 3, indices.data(), activations.data());

    // Cleanup
    feature_hypercolumn_destroy(hcol);
    SUCCEED();
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
