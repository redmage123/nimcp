/**
 * @file test_visual_cortex.cpp
 * @brief Tests for visual cortex and CNN operations
 *
 * WHAT: Unit and integration tests for visual processing
 * WHY:  Ensure visual cortex works correctly and integrates with NIMCP
 * HOW:  TDD approach with comprehensive test coverage
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <cstring>

#include "perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Convolution Tests
//=============================================================================

TEST(ConvolutionTest, CreateConvLayer) {
    conv_layer_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .input_channels = 1,
        .num_filters = 8,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    EXPECT_EQ(conv_layer_get_output_width(layer), 32);
    EXPECT_EQ(conv_layer_get_output_height(layer), 32);
    EXPECT_EQ(conv_layer_get_output_channels(layer), 8);

    conv_layer_destroy(layer);
}

TEST(ConvolutionTest, InvalidConfig) {
    conv_layer_config_t config = {
        .input_width = 0,  // Invalid
        .input_height = 32,
        .input_channels = 1,
        .num_filters = 8,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    EXPECT_EQ(layer, nullptr);
}

TEST(ConvolutionTest, SimpleConvolution) {
    // 3x3 input, 2x2 kernel, stride 1, no padding
    conv_layer_config_t config = {
        .input_width = 3,
        .input_height = 3,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 2,
        .stride = 1,
        .padding = 0,
        .activation = VISUAL_ACTIVATION_NONE
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Input: 3x3 image
    float input[9] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    // Set kernel to identity-like (sum)
    float kernel[4] = {1, 0, 0, 1};  // Top-left and bottom-right
    conv_layer_set_kernel(layer, 0, kernel);

    // Output should be 2x2
    float output[4];
    bool success = conv_layer_forward(layer, input, output);
    EXPECT_TRUE(success);

    // Expected: [1+5, 2+6, 4+8, 5+9] = [6, 8, 12, 14]
    EXPECT_FLOAT_EQ(output[0], 6.0f);
    EXPECT_FLOAT_EQ(output[1], 8.0f);
    EXPECT_FLOAT_EQ(output[2], 12.0f);
    EXPECT_FLOAT_EQ(output[3], 14.0f);

    conv_layer_destroy(layer);
}

TEST(ConvolutionTest, ReLUActivation) {
    conv_layer_config_t config = {
        .input_width = 2,
        .input_height = 2,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 2,
        .stride = 1,
        .padding = 0,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[4] = {1, -2, -3, 4};
    float kernel[4] = {1, 1, 1, 1};
    conv_layer_set_kernel(layer, 0, kernel);

    float output[1];
    conv_layer_forward(layer, input, output);

    // Sum = 1 + (-2) + (-3) + 4 = 0, ReLU(0) = 0
    EXPECT_FLOAT_EQ(output[0], 0.0f);

    conv_layer_destroy(layer);
}

//=============================================================================
// Pooling Tests
//=============================================================================

TEST(PoolingTest, MaxPooling2x2) {
    pool_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 1,
        .pool_size = 2,
        .stride = 2,
        .type = POOL_MAX
    };

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // 4x4 input
    float input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    // Output should be 2x2
    float output[4];
    bool success = pool_layer_forward(layer, input, output);
    EXPECT_TRUE(success);

    // Max of each 2x2 block: [6, 8, 14, 16]
    EXPECT_FLOAT_EQ(output[0], 6.0f);
    EXPECT_FLOAT_EQ(output[1], 8.0f);
    EXPECT_FLOAT_EQ(output[2], 14.0f);
    EXPECT_FLOAT_EQ(output[3], 16.0f);

    pool_layer_destroy(layer);
}

TEST(PoolingTest, AveragePooling) {
    pool_layer_config_t config = {
        .input_width = 2,
        .input_height = 2,
        .input_channels = 1,
        .pool_size = 2,
        .stride = 2,
        .type = POOL_AVG
    };

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[4] = {1, 2, 3, 4};
    float output[1];

    pool_layer_forward(layer, input, output);

    // Average of [1,2,3,4] = 2.5
    EXPECT_FLOAT_EQ(output[0], 2.5f);

    pool_layer_destroy(layer);
}

//=============================================================================
// Visual Cortex Tests
//=============================================================================

TEST(VisualCortexTest, CreateDestroy) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 16,
        .feature_dim = 128,
        .enable_attention = true,
        .enable_memory = true,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 160
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
}

TEST(VisualCortexTest, ProcessGrayscaleImage) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 8,
        .feature_dim = 64,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 80
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Create test image (32x32 grayscale)
    uint8_t* image = new uint8_t[32 * 32];
    for (int i = 0; i < 32 * 32; i++) {
        image[i] = (i % 256);
    }

    // Process image
    float* features = new float[64];
    bool success = visual_cortex_process(cortex, image, 32, 32, 1, features);
    EXPECT_TRUE(success);

    // Check features are not all zero
    bool has_nonzero = false;
    for (int i = 0; i < 64; i++) {
        if (fabsf(features[i]) > 1e-6f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

TEST(VisualCortexTest, EdgeDetection) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 8,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 80
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Create vertical edge image
    uint8_t* image = new uint8_t[16 * 16];
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            image[y * 16 + x] = (x < 8) ? 0 : 255;
        }
    }

    float* features = new float[32];
    bool success = visual_cortex_process(cortex, image, 16, 16, 1, features);
    EXPECT_TRUE(success);

    // Should detect the edge
    float max_response = 0.0f;
    for (int i = 0; i < 32; i++) {
        if (features[i] > max_response) {
            max_response = features[i];
        }
    }
    EXPECT_GT(max_response, 0.1f);

    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Gabor Filter Tests (V1 Edge Detection)
//=============================================================================

TEST(GaborFilterTest, CreateGaborKernel) {
    gabor_params_t params = {
        .wavelength = 4.0f,
        .orientation = 0.0f,  // Vertical
        .phase = 0.0f,
        .aspect_ratio = 0.5f,
        .bandwidth = 1.0f
    };

    int kernel_size = 7;
    float* kernel = gabor_create_kernel(kernel_size, &params);
    ASSERT_NE(kernel, nullptr);

    // Check kernel is not all zeros
    bool has_nonzero = false;
    for (int i = 0; i < kernel_size * kernel_size; i++) {
        if (fabsf(kernel[i]) > 1e-6f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_free(kernel);
}

TEST(GaborFilterTest, MultipleOrientations) {
    int kernel_size = 7;
    float orientations[] = {0.0f, 45.0f, 90.0f, 135.0f};

    for (float orientation : orientations) {
        gabor_params_t params = {
            .wavelength = 4.0f,
            .orientation = orientation,
            .phase = 0.0f,
            .aspect_ratio = 0.5f,
            .bandwidth = 1.0f
        };

        float* kernel = gabor_create_kernel(kernel_size, &params);
        ASSERT_NE(kernel, nullptr);
        nimcp_free(kernel);
    }
}

//=============================================================================
// Visual Attention Tests
//=============================================================================

TEST(VisualAttentionTest, SalienceMap) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 8,
        .feature_dim = 64,
        .enable_attention = true,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 80
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t* image = new uint8_t[32 * 32];
    // Create image with bright spot in center
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            int dx = x - 16;
            int dy = y - 16;
            image[y * 32 + x] = (dx*dx + dy*dy < 25) ? 255 : 0;
        }
    }

    attention_map_t* attn_map = attention_map_create(32, 32);
    ASSERT_NE(attn_map, nullptr);

    bool success = visual_cortex_compute_attention(cortex, image, 32, 32, attn_map);
    EXPECT_TRUE(success);

    // Check that edge of bright spot has higher attention than corner
    // The edge is at radius ~5 from center (16,16), so check (21,16)
    float edge_attention = attention_map_get(attn_map, 21, 16);
    float corner_attention = attention_map_get(attn_map, 2, 2);
    EXPECT_GT(edge_attention, corner_attention);

    attention_map_destroy(attn_map);
    delete[] image;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Visual Memory Tests
//=============================================================================

TEST(VisualMemoryTest, StoreAndRecall) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = true,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Process and store image
    uint8_t* image = new uint8_t[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        image[i] = (i * 7) % 256;
    }

    float* features = new float[32];
    visual_cortex_process(cortex, image, 16, 16, 1, features);

    // Store with high salience
    bool stored = visual_cortex_store_memory(cortex, features, 0.9f);
    EXPECT_TRUE(stored);

    // Recall similar memories
    visual_memory_t** memories = nullptr;
    int num_memories = 0;
    bool recalled = visual_cortex_recall_memory(cortex, features, 5, &memories, &num_memories);
    EXPECT_TRUE(recalled);
    EXPECT_GT(num_memories, 0);

    if (memories) {
        nimcp_free(memories);
    }

    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST(VisualCortexIntegration, FullPipeline) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 16,
        .feature_dim = 128,
        .enable_attention = true,
        .enable_memory = true,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 160
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Process multiple images
    for (int img_idx = 0; img_idx < 3; img_idx++) {
        uint8_t* image = new uint8_t[32 * 32];
        for (int i = 0; i < 32 * 32; i++) {
            image[i] = ((i + img_idx * 100) * 13) % 256;
        }

        float* features = new float[128];
        attention_map_t* attn_map = attention_map_create(32, 32);

        // Full processing pipeline
        bool processed = visual_cortex_process(cortex, image, 32, 32, 1, features);
        EXPECT_TRUE(processed);

        bool attended = visual_cortex_compute_attention(cortex, image, 32, 32, attn_map);
        EXPECT_TRUE(attended);

        bool stored = visual_cortex_store_memory(cortex, features, 0.5f + img_idx * 0.1f);
        EXPECT_TRUE(stored);

        attention_map_destroy(attn_map);
        delete[] image;
        delete[] features;
    }

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST(VisualCortexPerformance, ProcessingSpeed) {
    visual_cortex_config_t config = {
        .input_width = 64,
        .input_height = 64,
        .num_v1_filters = 32,
        .feature_dim = 256,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 320
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t* image = new uint8_t[64 * 64];
    for (int i = 0; i < 64 * 64; i++) {
        image[i] = i % 256;
    }

    float* features = new float[256];

    auto start = std::chrono::high_resolution_clock::now();

    int num_iterations = 100;
    for (int i = 0; i < num_iterations; i++) {
        visual_cortex_process(cortex, image, 64, 64, 1, features);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time = duration.count() / (double)num_iterations;
    std::cout << "Visual cortex processing: " << avg_time << " μs per 64x64 image" << std::endl;

    // Should process in reasonable time (< 50ms for unoptimized first implementation)
    EXPECT_LT(avg_time, 50000.0);

    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Validation Tests
//=============================================================================

TEST(VisualCortexValidation, NullPointers) {
    EXPECT_EQ(visual_cortex_create(nullptr), nullptr);
    EXPECT_FALSE(visual_cortex_process(nullptr, nullptr, 0, 0, 0, nullptr));

    visual_cortex_destroy(nullptr);  // Should not crash
}

TEST(VisualCortexValidation, InvalidDimensions) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 8,
        .feature_dim = 64,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 80
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t image[100];
    float features[64];

    // Wrong dimensions should fail gracefully
    EXPECT_FALSE(visual_cortex_process(cortex, image, 0, 32, 1, features));
    EXPECT_FALSE(visual_cortex_process(cortex, image, 32, 0, 1, features));
    EXPECT_FALSE(visual_cortex_process(cortex, image, 32, 32, 0, features));

    visual_cortex_destroy(cortex);
}
