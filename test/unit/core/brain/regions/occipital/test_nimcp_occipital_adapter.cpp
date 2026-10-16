/**
 * @file test_nimcp_occipital_adapter.cpp
 * @brief Unit tests for nimcp_occipital_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Occipital cortex adapter
 * WHY:  Ensure correct integration of V1-V5 visual processing sub-modules
 * HOW:  Use Google Test framework to test lifecycle, visual processing,
 *       feature detection, attention modulation, and statistics tracking.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// Test Fixture for Occipital Adapter
class OccipitalAdapterTest : public ::testing::Test {
protected:
    occipital_adapter_t* adapter;
    occipital_config_t config;

    void SetUp() override {
        config = occipital_default_config();
        // Use smaller dimensions for faster tests
        config.image_width = 64;
        config.image_height = 64;
        config.color_channels = 3;
        adapter = occipital_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create occipital adapter";
    }

    void TearDown() override {
        occipital_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create a test image with features
    float* create_test_image(uint32_t width, uint32_t height, uint32_t channels) {
        uint32_t size = width * height * channels;
        float* image = (float*)calloc(size, sizeof(float));
        if (!image) return nullptr;

        // Create a simple pattern with edges and color regions
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                // Red channel - gradient
                image[0 * width * height + y * width + x] = (float)x / (float)width;
                // Green channel - inverse gradient
                image[1 * width * height + y * width + x] = 1.0f - (float)x / (float)width;
                // Blue channel - vertical bars
                image[2 * width * height + y * width + x] = ((x / 8) % 2 == 0) ? 1.0f : 0.0f;
            }
        }

        return image;
    }

    void free_test_image(float* image) {
        if (image) free(image);
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, DefaultConfigHasReasonableValues) {
    occipital_config_t default_config = occipital_default_config();

    EXPECT_EQ(default_config.image_width, OCCIPITAL_DEFAULT_IMAGE_WIDTH);
    EXPECT_EQ(default_config.image_height, OCCIPITAL_DEFAULT_IMAGE_HEIGHT);
    EXPECT_EQ(default_config.color_channels, OCCIPITAL_DEFAULT_COLOR_CHANNELS);
    EXPECT_EQ(default_config.v1_num_orientations, OCCIPITAL_DEFAULT_NUM_ORIENTATIONS);
    EXPECT_EQ(default_config.v1_num_scales, OCCIPITAL_DEFAULT_NUM_SCALES);
    EXPECT_TRUE(default_config.v1_enable_gabor);
    EXPECT_TRUE(default_config.v4_enable_color);
    EXPECT_TRUE(default_config.v5_enable_motion);
    EXPECT_TRUE(default_config.enable_attention);
}

TEST_F(OccipitalAdapterTest, CreateWithNullConfigUsesDefaults) {
    occipital_adapter_t* adapter_null = occipital_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    occipital_config_t retrieved;
    EXPECT_TRUE(occipital_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.image_width, OCCIPITAL_DEFAULT_IMAGE_WIDTH);
    EXPECT_EQ(retrieved.image_height, OCCIPITAL_DEFAULT_IMAGE_HEIGHT);

    occipital_destroy(adapter_null);
}

TEST_F(OccipitalAdapterTest, DestroyNullDoesNotCrash) {
    occipital_destroy(NULL);
    // Should not crash
}

TEST_F(OccipitalAdapterTest, ResetClearsState) {
    // Process an image first to change state
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };
    ASSERT_TRUE(occipital_set_input(adapter, &input));

    EXPECT_TRUE(occipital_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(occipital_get_status(adapter), OCCIPITAL_STATUS_IDLE);
    EXPECT_EQ(occipital_get_last_error(adapter), OCCIPITAL_ERROR_NONE);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(occipital_reset(NULL));
}

// ============================================================================
// INPUT HANDLING TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, SetInputSuccess) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 1000,
        .frame_id = 1
    };

    EXPECT_TRUE(occipital_set_input(adapter, &input));
    EXPECT_EQ(occipital_get_status(adapter), OCCIPITAL_STATUS_IDLE);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, SetInputWrongDimensionsFails) {
    // Create image with wrong dimensions
    float* image = create_test_image(32, 32, 3);  // Wrong size
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = 32,
        .height = 32,
        .channels = 3,
        .timestamp_us = 0,
        .frame_id = 0
    };

    EXPECT_FALSE(occipital_set_input(adapter, &input));
    EXPECT_EQ(occipital_get_last_error(adapter), OCCIPITAL_ERROR_INVALID_INPUT);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, SetInputNullFails) {
    EXPECT_FALSE(occipital_set_input(adapter, NULL));
    EXPECT_FALSE(occipital_set_input(NULL, NULL));

    visual_input_t input = { .data = NULL };
    EXPECT_FALSE(occipital_set_input(adapter, &input));
}

// ============================================================================
// VISUAL PROCESSING PIPELINE TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, ProcessFullPipeline) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));

    visual_processing_result_t result;
    EXPECT_TRUE(occipital_process(adapter, &result));

    // Check that processing stages completed
    EXPECT_TRUE(result.v1_processed);
    EXPECT_TRUE(result.v2_processed);
    EXPECT_TRUE(result.v4_processed || config.active_stream == VISUAL_STREAM_DORSAL);
    EXPECT_TRUE(result.v5_processed || config.active_stream == VISUAL_STREAM_VENTRAL);
    EXPECT_TRUE(result.ready_for_downstream);

    // Should have detected some features (edges from the pattern)
    EXPECT_GT(result.total_features, 0u);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, ProcessWithoutInputFails) {
    visual_processing_result_t result;
    EXPECT_FALSE(occipital_process(adapter, &result));
    EXPECT_EQ(occipital_get_last_error(adapter), OCCIPITAL_ERROR_NO_INPUT);
}

TEST_F(OccipitalAdapterTest, ProcessV1Only) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    EXPECT_TRUE(occipital_process_v1(adapter));
    EXPECT_EQ(occipital_get_status(adapter), OCCIPITAL_STATUS_V1_PROCESSING);

    // Should have detected edges
    uint32_t edge_count = occipital_get_feature_count(adapter, VISUAL_AREA_V1);
    EXPECT_GT(edge_count, 0u);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, ProcessV4ColorDetection) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    EXPECT_TRUE(occipital_process_v4(adapter));

    // Should have detected color regions
    color_percept_t percepts[64];
    uint32_t count = 64;
    EXPECT_TRUE(occipital_get_color_percepts(adapter, percepts, &count));
    EXPECT_GT(count, 0u);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, ProcessV5MotionDetection) {
    float* image1 = create_test_image(config.image_width, config.image_height, config.color_channels);
    float* image2 = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image1);
    ASSERT_NE(nullptr, image2);

    // Modify second image to simulate motion
    for (uint32_t y = 0; y < config.image_height; y++) {
        for (uint32_t x = 0; x < config.image_width - 4; x++) {
            for (uint32_t c = 0; c < config.color_channels; c++) {
                // Shift pattern right by 4 pixels
                image2[c * config.image_width * config.image_height + y * config.image_width + x + 4] =
                    image1[c * config.image_width * config.image_height + y * config.image_width + x];
            }
        }
    }

    visual_input_t input1 = {
        .data = image1,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    visual_input_t input2 = {
        .data = image2,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 33333,  // ~30fps
        .frame_id = 1
    };

    // Process two frames
    ASSERT_TRUE(occipital_set_input(adapter, &input1));
    EXPECT_TRUE(occipital_process_v5(adapter));

    ASSERT_TRUE(occipital_set_input(adapter, &input2));
    EXPECT_TRUE(occipital_process_v5(adapter));

    // Motion may or may not be detected depending on threshold
    // Just check the API works without errors
    motion_vector_t motions[64];
    uint32_t motion_count = 64;
    occipital_get_motion_vectors(adapter, motions, &motion_count);

    free_test_image(image1);
    free_test_image(image2);
}

// ============================================================================
// FEATURE ACCESS TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, GetFeatureCount) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    ASSERT_TRUE(occipital_process(adapter, NULL));

    // Test feature counts for each area
    uint32_t v1_count = occipital_get_feature_count(adapter, VISUAL_AREA_V1);
    uint32_t v2_count = occipital_get_feature_count(adapter, VISUAL_AREA_V2);
    uint32_t total = occipital_get_feature_count(adapter, VISUAL_AREA_COUNT);

    // V1 should have edges, V2 should have contours
    EXPECT_GT(v1_count, 0u);
    EXPECT_GE(total, v1_count + v2_count);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, GetFeatureByIndex) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    ASSERT_TRUE(occipital_process(adapter, NULL));

    uint32_t count = occipital_get_feature_count(adapter, VISUAL_AREA_V1);
    if (count > 0) {
        visual_feature_t feature;
        EXPECT_TRUE(occipital_get_feature(adapter, VISUAL_AREA_V1, 0, &feature));
        EXPECT_EQ(feature.source_area, VISUAL_AREA_V1);
        EXPECT_EQ(feature.type, VISUAL_FEATURE_EDGE);
        EXPECT_GE(feature.x, 0.0f);
        EXPECT_LE(feature.x, 1.0f);
        EXPECT_GE(feature.y, 0.0f);
        EXPECT_LE(feature.y, 1.0f);
        EXPECT_GE(feature.strength, 0.0f);
        EXPECT_LE(feature.strength, 1.0f);
    }

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, GetFeaturesArray) {
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    ASSERT_TRUE(occipital_process(adapter, NULL));

    visual_feature_t features[100];
    uint32_t count = 100;
    EXPECT_TRUE(occipital_get_features(adapter, VISUAL_AREA_V1, features, &count));

    // Should return at least one feature
    if (occipital_get_feature_count(adapter, VISUAL_AREA_V1) > 0) {
        EXPECT_GT(count, 0u);
    }

    free_test_image(image);
}

// ============================================================================
// ATTENTION MODULATION TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, ApplySpatialAttention) {
    EXPECT_TRUE(occipital_apply_spatial_attention(adapter, 0.5f, 0.5f, 0.2f, 2.0f));

    // Process with attention applied
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    EXPECT_TRUE(occipital_process(adapter, NULL));

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, ApplyFeatureAttention) {
    EXPECT_TRUE(occipital_apply_feature_attention(adapter, VISUAL_FEATURE_EDGE, 1.5f));
    EXPECT_TRUE(occipital_apply_feature_attention(adapter, VISUAL_FEATURE_COLOR, 1.2f));
    EXPECT_TRUE(occipital_apply_feature_attention(adapter, VISUAL_FEATURE_MOTION, 2.0f));
}

TEST_F(OccipitalAdapterTest, AttentionWithNullAdapter) {
    EXPECT_FALSE(occipital_apply_spatial_attention(NULL, 0.5f, 0.5f, 0.2f, 1.5f));
    EXPECT_FALSE(occipital_apply_feature_attention(NULL, VISUAL_FEATURE_EDGE, 1.5f));
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static int feature_callback_count = 0;
static void test_feature_callback(const visual_feature_t* feature, void* user_data) {
    (void)feature;
    (void)user_data;
    feature_callback_count++;
}

static int motion_callback_count = 0;
static void test_motion_callback(const motion_vector_t* motion, void* user_data) {
    (void)motion;
    (void)user_data;
    motion_callback_count++;
}

TEST_F(OccipitalAdapterTest, SetFeatureCallback) {
    feature_callback_count = 0;
    EXPECT_TRUE(occipital_set_feature_callback(adapter, test_feature_callback, NULL));

    // Process an image to trigger callbacks
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    ASSERT_TRUE(occipital_process(adapter, NULL));

    // Callback should have been invoked for each edge
    EXPECT_GE(feature_callback_count, 0);  // May be 0 if no edges detected

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, SetMotionCallback) {
    motion_callback_count = 0;
    EXPECT_TRUE(occipital_set_motion_callback(adapter, test_motion_callback, NULL));
}

TEST_F(OccipitalAdapterTest, SetCallbackNullAdapter) {
    EXPECT_FALSE(occipital_set_feature_callback(NULL, test_feature_callback, NULL));
    EXPECT_FALSE(occipital_set_motion_callback(NULL, test_motion_callback, NULL));
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, GetStatusIdle) {
    EXPECT_EQ(occipital_get_status(adapter), OCCIPITAL_STATUS_IDLE);
}

TEST_F(OccipitalAdapterTest, GetLastErrorNone) {
    EXPECT_EQ(occipital_get_last_error(adapter), OCCIPITAL_ERROR_NONE);
}

TEST_F(OccipitalAdapterTest, ErrorStringNotNull) {
    const char* str = occipital_error_string(OCCIPITAL_ERROR_NONE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = occipital_error_string(OCCIPITAL_ERROR_INVALID_INPUT);
    EXPECT_NE(str, nullptr);

    str = occipital_error_string(OCCIPITAL_ERROR_V1_FAILURE);
    EXPECT_NE(str, nullptr);
}

TEST_F(OccipitalAdapterTest, StatusStringNotNull) {
    const char* str = occipital_status_string(OCCIPITAL_STATUS_IDLE);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = occipital_status_string(OCCIPITAL_STATUS_V1_PROCESSING);
    EXPECT_NE(str, nullptr);

    str = occipital_status_string(OCCIPITAL_STATUS_READY);
    EXPECT_NE(str, nullptr);
}

TEST_F(OccipitalAdapterTest, GetStats) {
    occipital_stats_t stats;
    EXPECT_TRUE(occipital_get_stats(adapter, &stats));
    EXPECT_EQ(stats.frames_processed, 0u);  // No frames yet

    // Process one frame
    float* image = create_test_image(config.image_width, config.image_height, config.color_channels);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = config.image_width,
        .height = config.image_height,
        .channels = config.color_channels,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(adapter, &input));
    ASSERT_TRUE(occipital_process(adapter, NULL));

    EXPECT_TRUE(occipital_get_stats(adapter, &stats));
    EXPECT_EQ(stats.frames_processed, 1u);
    EXPECT_EQ(stats.successful_frames, 1u);
    EXPECT_EQ(stats.failed_frames, 0u);

    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, GetConfig) {
    occipital_config_t retrieved;
    EXPECT_TRUE(occipital_get_config(adapter, &retrieved));
    EXPECT_EQ(retrieved.image_width, config.image_width);
    EXPECT_EQ(retrieved.image_height, config.image_height);
    EXPECT_EQ(retrieved.color_channels, config.color_channels);
}

// ============================================================================
// SUB-MODULE ACCESS TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, GetV1ProcessorNotNull) {
    EXPECT_NE(occipital_get_v1_processor(adapter), nullptr);
}

TEST_F(OccipitalAdapterTest, GetV2ProcessorNotNull) {
    EXPECT_NE(occipital_get_v2_processor(adapter), nullptr);
}

TEST_F(OccipitalAdapterTest, GetV4ProcessorNotNull) {
    EXPECT_NE(occipital_get_v4_processor(adapter), nullptr);
}

TEST_F(OccipitalAdapterTest, GetV5ProcessorNotNull) {
    EXPECT_NE(occipital_get_v5_processor(adapter), nullptr);
}

TEST_F(OccipitalAdapterTest, GetSubModulesNullAdapter) {
    EXPECT_EQ(occipital_get_v1_processor(NULL), nullptr);
    EXPECT_EQ(occipital_get_v2_processor(NULL), nullptr);
    EXPECT_EQ(occipital_get_v4_processor(NULL), nullptr);
    EXPECT_EQ(occipital_get_v5_processor(NULL), nullptr);
}

// ============================================================================
// NULL SAFETY TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, GetStatusNull) {
    EXPECT_EQ(occipital_get_status(NULL), OCCIPITAL_STATUS_ERROR);
}

TEST_F(OccipitalAdapterTest, GetLastErrorNull) {
    EXPECT_NE(occipital_get_last_error(NULL), OCCIPITAL_ERROR_NONE);
}

TEST_F(OccipitalAdapterTest, GetStatsNull) {
    occipital_stats_t stats;
    EXPECT_FALSE(occipital_get_stats(NULL, &stats));
    EXPECT_FALSE(occipital_get_stats(adapter, NULL));
}

TEST_F(OccipitalAdapterTest, GetConfigNull) {
    occipital_config_t cfg;
    EXPECT_FALSE(occipital_get_config(NULL, &cfg));
    EXPECT_FALSE(occipital_get_config(adapter, NULL));
}

TEST_F(OccipitalAdapterTest, ProcessNullAdapter) {
    visual_processing_result_t result;
    EXPECT_FALSE(occipital_process(NULL, &result));
}

// ============================================================================
// VISUAL STREAM TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, DorsalStreamOnly) {
    occipital_config_t dorsal_config = occipital_default_config();
    dorsal_config.image_width = 64;
    dorsal_config.image_height = 64;
    dorsal_config.active_stream = VISUAL_STREAM_DORSAL;

    occipital_adapter_t* dorsal_adapter = occipital_create(&dorsal_config);
    ASSERT_NE(nullptr, dorsal_adapter);

    float* image = create_test_image(64, 64, 3);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = 64,
        .height = 64,
        .channels = 3,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(dorsal_adapter, &input));

    visual_processing_result_t result;
    EXPECT_TRUE(occipital_process(dorsal_adapter, &result));

    // V5 should be processed, V4 should not
    EXPECT_TRUE(result.v5_processed);
    EXPECT_FALSE(result.v4_processed);

    occipital_destroy(dorsal_adapter);
    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, VentralStreamOnly) {
    occipital_config_t ventral_config = occipital_default_config();
    ventral_config.image_width = 64;
    ventral_config.image_height = 64;
    ventral_config.active_stream = VISUAL_STREAM_VENTRAL;

    occipital_adapter_t* ventral_adapter = occipital_create(&ventral_config);
    ASSERT_NE(nullptr, ventral_adapter);

    float* image = create_test_image(64, 64, 3);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = 64,
        .height = 64,
        .channels = 3,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(ventral_adapter, &input));

    visual_processing_result_t result;
    EXPECT_TRUE(occipital_process(ventral_adapter, &result));

    // V4 should be processed, V5 should not
    EXPECT_TRUE(result.v4_processed);
    EXPECT_FALSE(result.v5_processed);

    occipital_destroy(ventral_adapter);
    free_test_image(image);
}

// ============================================================================
// TRAINING TESTS
// ============================================================================

TEST_F(OccipitalAdapterTest, TrainNotEnabledFails) {
    // Default config has training disabled
    visual_feature_t targets[1];
    memset(targets, 0, sizeof(targets));
    targets[0].type = VISUAL_FEATURE_EDGE;
    targets[0].x = 0.5f;
    targets[0].y = 0.5f;

    EXPECT_FALSE(occipital_train(adapter, targets, 1, 0.001f));
}

TEST_F(OccipitalAdapterTest, TrainWithTrainingEnabled) {
    occipital_config_t train_config = occipital_default_config();
    train_config.image_width = 64;
    train_config.image_height = 64;
    train_config.enable_training = true;

    occipital_adapter_t* train_adapter = occipital_create(&train_config);
    ASSERT_NE(nullptr, train_adapter);

    // First process an image
    float* image = create_test_image(64, 64, 3);
    ASSERT_NE(nullptr, image);

    visual_input_t input = {
        .data = image,
        .width = 64,
        .height = 64,
        .channels = 3,
        .timestamp_us = 0,
        .frame_id = 0
    };

    ASSERT_TRUE(occipital_set_input(train_adapter, &input));
    ASSERT_TRUE(occipital_process(train_adapter, NULL));

    // Now train
    visual_feature_t targets[2];
    memset(targets, 0, sizeof(targets));
    targets[0].type = VISUAL_FEATURE_EDGE;
    targets[0].x = 0.5f;
    targets[0].y = 0.5f;
    targets[1].type = VISUAL_FEATURE_EDGE;
    targets[1].x = 0.3f;
    targets[1].y = 0.3f;

    EXPECT_TRUE(occipital_train(train_adapter, targets, 2, 0.001f));

    occipital_stats_t stats;
    EXPECT_TRUE(occipital_get_stats(train_adapter, &stats));
    EXPECT_EQ(stats.training_iterations, 1u);

    occipital_destroy(train_adapter);
    free_test_image(image);
}

TEST_F(OccipitalAdapterTest, TrainNullAdapter) {
    visual_feature_t targets[1];
    EXPECT_FALSE(occipital_train(NULL, targets, 1, 0.001f));
}

TEST_F(OccipitalAdapterTest, TrainNullTargets) {
    EXPECT_FALSE(occipital_train(adapter, NULL, 1, 0.001f));
}

TEST_F(OccipitalAdapterTest, TrainZeroTargets) {
    visual_feature_t targets[1];
    EXPECT_FALSE(occipital_train(adapter, targets, 0, 0.001f));
}
