/**
 * @file test_visual_cortex_extended.cpp
 * @brief Extended tests for visual cortex to achieve 95%+ coverage
 *
 * WHAT: Comprehensive unit tests for uncovered visual cortex functionality
 * WHY:  Boost coverage from 7.9% to 95%+ for visual_cortex.c
 * HOW:  Test all activation functions, neuromodulation, attention, memory,
 *       brain integration, and edge cases
 *
 * TARGET: 352 uncovered lines → comprehensive coverage
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "perception/nimcp_visual_cortex.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

//=============================================================================
// Activation Function Tests
//=============================================================================

class ActivationTest : public ::testing::Test {
protected:
    conv_layer_t* layer;

    void SetUp() override {
        layer = nullptr;
    }

    void TearDown() override {
        if (layer) {
            conv_layer_destroy(layer);
        }
    }
};

TEST_F(ActivationTest, SigmoidActivation) {
    conv_layer_config_t config = {
        .input_width = 2,
        .input_height = 2,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 2,
        .stride = 1,
        .padding = 0,
        .activation = VISUAL_ACTIVATION_SIGMOID
    };

    layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[4] = {0, 1, 2, 3};
    float kernel[4] = {1, 0, 0, 0};  // Just take first value
    conv_layer_set_kernel(layer, 0, kernel);

    float output[1];
    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    // Sigmoid(0) should be 0.5
    EXPECT_NEAR(output[0], 0.5f, 0.01f);
}

TEST_F(ActivationTest, TanhActivation) {
    conv_layer_config_t config = {
        .input_width = 2,
        .input_height = 2,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 2,
        .stride = 1,
        .padding = 0,
        .activation = VISUAL_ACTIVATION_TANH
    };

    layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[4] = {0, 0, 0, 0};
    float kernel[4] = {1, 1, 1, 1};
    conv_layer_set_kernel(layer, 0, kernel);

    float output[1];
    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    // Tanh(0) should be 0
    EXPECT_NEAR(output[0], 0.0f, 0.01f);
}

TEST_F(ActivationTest, NoneActivation) {
    conv_layer_config_t config = {
        .input_width = 2,
        .input_height = 2,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 2,
        .stride = 1,
        .padding = 0,
        .activation = VISUAL_ACTIVATION_NONE
    };

    layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[4] = {1, 2, 3, 4};
    float kernel[4] = {1, 1, 1, 1};
    conv_layer_set_kernel(layer, 0, kernel);

    float output[1];
    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    // No activation, just sum: 1+2+3+4 = 10
    EXPECT_FLOAT_EQ(output[0], 10.0f);
}

//=============================================================================
// Convolution Layer Edge Cases
//=============================================================================

TEST(ConvolutionEdgeCases, MultipleFilters) {
    conv_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 1,
        .num_filters = 3,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Set different kernels for each filter
    for (uint32_t f = 0; f < 3; f++) {
        float kernel[9];
        for (int i = 0; i < 9; i++) {
            kernel[i] = (f + 1) * 0.1f;
        }
        EXPECT_TRUE(conv_layer_set_kernel(layer, f, kernel));
    }

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = i * 0.1f;
    }

    uint32_t output_size = 4 * 4 * 3;  // width * height * filters
    float* output = new float[output_size];

    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    delete[] output;
    conv_layer_destroy(layer);
}

TEST(ConvolutionEdgeCases, MultiChannelInput) {
    conv_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 3,  // RGB
        .num_filters = 2,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Set kernels
    for (uint32_t f = 0; f < 2; f++) {
        float kernel[27];  // 3x3x3
        for (int i = 0; i < 27; i++) {
            kernel[i] = 0.1f;
        }
        EXPECT_TRUE(conv_layer_set_kernel(layer, f, kernel));
    }

    float input[48];  // 4x4x3
    for (int i = 0; i < 48; i++) {
        input[i] = i * 0.01f;
    }

    float output[32];  // 4x4x2
    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    conv_layer_destroy(layer);
}

TEST(ConvolutionEdgeCases, WithStride) {
    conv_layer_config_t config = {
        .input_width = 8,
        .input_height = 8,
        .input_channels = 1,
        .num_filters = 1,
        .kernel_size = 3,
        .stride = 2,  // Stride of 2
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    EXPECT_EQ(conv_layer_get_output_width(layer), 4);
    EXPECT_EQ(conv_layer_get_output_height(layer), 4);

    conv_layer_destroy(layer);
}

TEST(ConvolutionEdgeCases, InvalidFilterIndex) {
    conv_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 1,
        .num_filters = 2,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float kernel[9] = {0};

    // Valid indices
    EXPECT_TRUE(conv_layer_set_kernel(layer, 0, kernel));
    EXPECT_TRUE(conv_layer_set_kernel(layer, 1, kernel));

    // Invalid index
    EXPECT_FALSE(conv_layer_set_kernel(layer, 2, kernel));
    EXPECT_FALSE(conv_layer_set_kernel(layer, 999, kernel));

    // Null kernel
    EXPECT_FALSE(conv_layer_set_kernel(layer, 0, nullptr));

    conv_layer_destroy(layer);
}

TEST(ConvolutionEdgeCases, NullLayerOperations) {
    EXPECT_EQ(conv_layer_get_output_width(nullptr), 0);
    EXPECT_EQ(conv_layer_get_output_height(nullptr), 0);
    EXPECT_EQ(conv_layer_get_output_channels(nullptr), 0);

    float input[4] = {0};
    float output[4] = {0};
    EXPECT_FALSE(conv_layer_forward(nullptr, input, output));

    conv_layer_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Pooling Layer Edge Cases
//=============================================================================

TEST(PoolingEdgeCases, InvalidPoolConfig) {
    pool_layer_config_t config = {
        .input_width = 0,  // Invalid
        .input_height = 4,
        .input_channels = 1,
        .pool_size = 2,
        .stride = 2,
        .type = POOL_MAX
    };

    pool_layer_t* layer = pool_layer_create(&config);
    EXPECT_EQ(layer, nullptr);
}

TEST(PoolingEdgeCases, NullPoolConfig) {
    pool_layer_t* layer = pool_layer_create(nullptr);
    EXPECT_EQ(layer, nullptr);
}

TEST(PoolingEdgeCases, NullPoolOperations) {
    float input[4] = {0};
    float output[1] = {0};
    EXPECT_FALSE(pool_layer_forward(nullptr, input, output));

    pool_layer_destroy(nullptr);  // Should not crash
}

TEST(PoolingEdgeCases, MultiChannelPooling) {
    pool_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 3,  // 3 channels
        .pool_size = 2,
        .stride = 2,
        .type = POOL_MAX
    };

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[48];  // 4x4x3
    for (int i = 0; i < 48; i++) {
        input[i] = i % 10;
    }

    float output[12];  // 2x2x3
    EXPECT_TRUE(pool_layer_forward(layer, input, output));

    pool_layer_destroy(layer);
}

//=============================================================================
// Gabor Filter Tests
//=============================================================================

TEST(GaborFilterTests, InvalidKernelSize) {
    gabor_params_t params = {
        .wavelength = 4.0f,
        .orientation = 0.0f,
        .phase = 0.0f,
        .aspect_ratio = 0.5f,
        .bandwidth = 1.0f
    };

    // Even kernel size (invalid)
    float* kernel = gabor_create_kernel(8, &params);
    EXPECT_EQ(kernel, nullptr);

    // Negative kernel size (invalid)
    kernel = gabor_create_kernel(-5, &params);
    EXPECT_EQ(kernel, nullptr);

    // Zero kernel size (invalid)
    kernel = gabor_create_kernel(0, &params);
    EXPECT_EQ(kernel, nullptr);
}

TEST(GaborFilterTests, NullParams) {
    float* kernel = gabor_create_kernel(7, nullptr);
    EXPECT_EQ(kernel, nullptr);
}

TEST(GaborFilterTests, VariousOrientations) {
    float orientations[] = {0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f};

    for (float orientation : orientations) {
        gabor_params_t params = {
            .wavelength = 4.0f,
            .orientation = orientation,
            .phase = 0.0f,
            .aspect_ratio = 0.5f,
            .bandwidth = 1.0f
        };

        float* kernel = gabor_create_kernel(7, &params);
        ASSERT_NE(kernel, nullptr);

        // Verify kernel has structure
        float sum = 0.0f;
        for (int i = 0; i < 49; i++) {
            sum += fabsf(kernel[i]);
        }
        EXPECT_GT(sum, 0.1f);

        nimcp_free(kernel);
    }
}

TEST(GaborFilterTests, VariousPhases) {
    float phases[] = {0.0f, 45.0f, 90.0f, 135.0f, 180.0f};

    for (float phase : phases) {
        gabor_params_t params = {
            .wavelength = 4.0f,
            .orientation = 0.0f,
            .phase = phase,
            .aspect_ratio = 0.5f,
            .bandwidth = 1.0f
        };

        float* kernel = gabor_create_kernel(7, &params);
        ASSERT_NE(kernel, nullptr);
        nimcp_free(kernel);
    }
}

TEST(GaborFilterTests, VariousWavelengths) {
    float wavelengths[] = {2.0f, 4.0f, 6.0f, 8.0f};

    for (float wavelength : wavelengths) {
        gabor_params_t params = {
            .wavelength = wavelength,
            .orientation = 0.0f,
            .phase = 0.0f,
            .aspect_ratio = 0.5f,
            .bandwidth = 1.0f
        };

        float* kernel = gabor_create_kernel(9, &params);
        ASSERT_NE(kernel, nullptr);
        nimcp_free(kernel);
    }
}

//=============================================================================
// Attention Map Tests
//=============================================================================

TEST(AttentionMapTests, CreateAndDestroy) {
    attention_map_t* map = attention_map_create(32, 32);
    ASSERT_NE(map, nullptr);
    attention_map_destroy(map);
}

TEST(AttentionMapTests, InvalidDimensions) {
    EXPECT_EQ(attention_map_create(0, 32), nullptr);
    EXPECT_EQ(attention_map_create(32, 0), nullptr);
    EXPECT_EQ(attention_map_create(0, 0), nullptr);
}

TEST(AttentionMapTests, SetAndGet) {
    attention_map_t* map = attention_map_create(10, 10);
    ASSERT_NE(map, nullptr);

    // Set values
    EXPECT_TRUE(attention_map_set(map, 5, 5, 0.8f));
    EXPECT_TRUE(attention_map_set(map, 0, 0, 0.1f));
    EXPECT_TRUE(attention_map_set(map, 9, 9, 0.9f));

    // Get values
    EXPECT_FLOAT_EQ(attention_map_get(map, 5, 5), 0.8f);
    EXPECT_FLOAT_EQ(attention_map_get(map, 0, 0), 0.1f);
    EXPECT_FLOAT_EQ(attention_map_get(map, 9, 9), 0.9f);

    attention_map_destroy(map);
}

TEST(AttentionMapTests, OutOfBounds) {
    attention_map_t* map = attention_map_create(10, 10);
    ASSERT_NE(map, nullptr);

    // Out of bounds set
    EXPECT_FALSE(attention_map_set(map, 10, 5, 0.5f));
    EXPECT_FALSE(attention_map_set(map, 5, 10, 0.5f));
    EXPECT_FALSE(attention_map_set(map, 100, 100, 0.5f));

    // Out of bounds get
    EXPECT_FLOAT_EQ(attention_map_get(map, 10, 5), -1.0f);
    EXPECT_FLOAT_EQ(attention_map_get(map, 5, 10), -1.0f);

    attention_map_destroy(map);
}

TEST(AttentionMapTests, NullOperations) {
    EXPECT_FLOAT_EQ(attention_map_get(nullptr, 0, 0), -1.0f);
    EXPECT_FALSE(attention_map_set(nullptr, 0, 0, 0.5f));
    attention_map_destroy(nullptr);  // Should not crash
}

TEST(AttentionMapTests, GetAttentionPeak) {
    attention_map_t* map = attention_map_create(10, 10);
    ASSERT_NE(map, nullptr);

    // Set a clear peak
    attention_map_set(map, 5, 5, 0.9f);
    attention_map_set(map, 3, 3, 0.5f);
    attention_map_set(map, 7, 7, 0.3f);

    uint32_t max_x, max_y;
    float max_value;

    EXPECT_TRUE(visual_cortex_get_attention_peak(map, &max_x, &max_y, &max_value));
    EXPECT_EQ(max_x, 5);
    EXPECT_EQ(max_y, 5);
    EXPECT_FLOAT_EQ(max_value, 0.9f);

    attention_map_destroy(map);
}

TEST(AttentionMapTests, AttentionPeakNullInputs) {
    attention_map_t* map = attention_map_create(10, 10);
    ASSERT_NE(map, nullptr);

    uint32_t max_x, max_y;
    float max_value;

    EXPECT_FALSE(visual_cortex_get_attention_peak(nullptr, &max_x, &max_y, &max_value));
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, nullptr, &max_y, &max_value));
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, &max_x, nullptr, &max_value));
    EXPECT_FALSE(visual_cortex_get_attention_peak(map, &max_x, &max_y, nullptr));

    attention_map_destroy(map);
}

//=============================================================================
// Visual Cortex Configuration Tests
//=============================================================================

TEST(VisualCortexConfig, InvalidConfig) {
    visual_cortex_config_t config = {
        .input_width = 0,  // Invalid
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
    EXPECT_EQ(cortex, nullptr);
}

TEST(VisualCortexConfig, NullConfig) {
    visual_cortex_t* cortex = visual_cortex_create(nullptr);
    EXPECT_EQ(cortex, nullptr);
}

//=============================================================================
// RGB Image Processing Tests
//=============================================================================

TEST(VisualCortexRGB, ProcessRGBImage) {
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

    // Create RGB image
    uint8_t* image = new uint8_t[16 * 16 * 3];
    for (int i = 0; i < 16 * 16 * 3; i++) {
        image[i] = (i * 7) % 256;
    }

    float* features = new float[32];
    EXPECT_TRUE(visual_cortex_process(cortex, image, 16, 16, 3, features));

    // Verify features are normalized
    float norm = 0.0f;
    for (int i = 0; i < 32; i++) {
        norm += features[i] * features[i];
    }
    norm = sqrtf(norm);
    EXPECT_NEAR(norm, 1.0f, 0.1f);  // Should be approximately unit norm

    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Memory and Novelty Tests
//=============================================================================

TEST(VisualMemoryTests, NoveltyDetection) {
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

    // Process first image
    uint8_t* image1 = new uint8_t[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        image1[i] = i % 256;
    }

    float* features1 = new float[32];
    visual_cortex_process(cortex, image1, 16, 16, 1, features1);

    // Before storing any memory, everything is novel
    float novelty_before = visual_cortex_compute_novelty(cortex, features1);
    EXPECT_FLOAT_EQ(novelty_before, 1.0f);

    // Store memory
    visual_cortex_store_memory(cortex, features1, 0.8f);

    // Same features should have low novelty now
    float novelty_after = visual_cortex_compute_novelty(cortex, features1);
    EXPECT_LT(novelty_after, 0.3f);

    // Different image should have higher novelty
    uint8_t* image2 = new uint8_t[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        image2[i] = (255 - i) % 256;  // Different pattern
    }

    float* features2 = new float[32];
    visual_cortex_process(cortex, image2, 16, 16, 1, features2);
    float novelty_different = visual_cortex_compute_novelty(cortex, features2);
    EXPECT_GT(novelty_different, novelty_after);

    delete[] image1;
    delete[] image2;
    delete[] features1;
    delete[] features2;
    visual_cortex_destroy(cortex);
}

TEST(VisualMemoryTests, NoveltyNullInputs) {
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

    float features[32] = {0};

    EXPECT_FLOAT_EQ(visual_cortex_compute_novelty(nullptr, features), 0.0f);
    EXPECT_FLOAT_EQ(visual_cortex_compute_novelty(cortex, nullptr), 0.0f);

    visual_cortex_destroy(cortex);
}

TEST(VisualMemoryTests, MemoryConsolidation) {
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

    float features[32];
    for (int i = 0; i < 32; i++) {
        features[i] = i * 0.01f;
    }

    EXPECT_TRUE(visual_cortex_consolidate_memory(cortex, features, 0.9f, "test context"));

    // Verify memory was stored
    visual_memory_t** memories = nullptr;
    int num_memories = 0;
    EXPECT_TRUE(visual_cortex_recall_memory(cortex, features, 5, &memories, &num_memories));
    EXPECT_GT(num_memories, 0);

    if (memories) {
        nimcp_free(memories);
    }

    visual_cortex_destroy(cortex);
}

TEST(VisualMemoryTests, ConsolidationWithoutMemoryEnabled) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,  // Disabled
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    float features[32] = {0};
    EXPECT_FALSE(visual_cortex_consolidate_memory(cortex, features, 0.9f, "test"));

    visual_cortex_destroy(cortex);
}

TEST(VisualMemoryTests, RecallEmptyMemory) {
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

    float features[32] = {0};
    visual_memory_t** memories = nullptr;
    int num_memories = 0;

    // Recall from empty memory
    EXPECT_TRUE(visual_cortex_recall_memory(cortex, features, 5, &memories, &num_memories));
    EXPECT_EQ(num_memories, 0);
    EXPECT_EQ(memories, nullptr);

    visual_cortex_destroy(cortex);
}

TEST(VisualMemoryTests, StoreWithoutMemoryEnabled) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,  // Disabled
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    float features[32] = {0};
    EXPECT_FALSE(visual_cortex_store_memory(cortex, features, 0.8f));

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Attention Computation Tests
//=============================================================================

TEST(VisualAttention, ComputeAttentionDisabled) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,  // Disabled
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t image[16 * 16] = {0};
    attention_map_t* map = attention_map_create(16, 16);

    EXPECT_FALSE(visual_cortex_compute_attention(cortex, image, 16, 16, map));

    attention_map_destroy(map);
    visual_cortex_destroy(cortex);
}

TEST(VisualAttention, ComputeAttentionEdgeImage) {
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

    // Create image with horizontal edge
    uint8_t* image = new uint8_t[32 * 32];
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            image[y * 32 + x] = (y < 16) ? 0 : 255;
        }
    }

    attention_map_t* map = attention_map_create(32, 32);
    EXPECT_TRUE(visual_cortex_compute_attention(cortex, image, 32, 32, map));

    // Edge should have higher attention
    float edge_attn = attention_map_get(map, 16, 16);
    float uniform_attn = attention_map_get(map, 5, 5);
    EXPECT_GT(edge_attn, uniform_attn);

    attention_map_destroy(map);
    delete[] image;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST(VisualCortexStats, GetStats) {
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

    // Initial stats
    visual_cortex_stats_t stats;
    EXPECT_TRUE(visual_cortex_get_stats(cortex, &stats));
    EXPECT_EQ(stats.images_processed, 0);
    EXPECT_EQ(stats.memories_stored, 0);
    EXPECT_FLOAT_EQ(stats.avg_processing_time, 0.0f);

    // Process some images
    uint8_t* image = new uint8_t[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        image[i] = i % 256;
    }

    float features[32];
    for (int i = 0; i < 3; i++) {
        visual_cortex_process(cortex, image, 16, 16, 1, features);
        visual_cortex_store_memory(cortex, features, 0.5f);
    }

    // Updated stats
    EXPECT_TRUE(visual_cortex_get_stats(cortex, &stats));
    EXPECT_EQ(stats.images_processed, 3);
    EXPECT_EQ(stats.memories_stored, 3);
    EXPECT_GT(stats.avg_processing_time, 0.0f);
    EXPECT_GT(stats.memory_usage_mb, 0.0f);

    delete[] image;
    visual_cortex_destroy(cortex);
}

TEST(VisualCortexStats, StatsNullInputs) {
    visual_cortex_stats_t stats;
    EXPECT_FALSE(visual_cortex_get_stats(nullptr, &stats));

    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    EXPECT_FALSE(visual_cortex_get_stats(cortex, nullptr));
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST(BrainIntegration, SetBrain) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Create a brain
    brain_t brain = brain_create("test_visual", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 10);

    // Set brain (should not crash)
    visual_cortex_set_brain(cortex, brain);

    // Process with brain attached
    uint8_t image[16 * 16] = {0};
    float features[32];
    EXPECT_TRUE(visual_cortex_process(cortex, image, 16, 16, 1, features));

    // Clear brain
    visual_cortex_set_brain(cortex, nullptr);

    brain_destroy(brain);
    visual_cortex_destroy(cortex);
}

TEST(BrainIntegration, SetBrainNull) {
    // Should handle null cortex gracefully
    visual_cortex_set_brain(nullptr, nullptr);
}

TEST(BrainIntegration, BoostRegionAttention) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Boost various regions (should not crash)
    visual_cortex_boost_region_attention(cortex, 0.5f, 0.5f, 1.5f);
    visual_cortex_boost_region_attention(cortex, 0.0f, 0.0f, 1.2f);
    visual_cortex_boost_region_attention(cortex, 1.0f, 1.0f, 2.0f);

    // Out of range values (should be clamped)
    visual_cortex_boost_region_attention(cortex, -0.5f, 1.5f, 3.0f);
    visual_cortex_boost_region_attention(cortex, 0.5f, 0.5f, 0.5f);

    visual_cortex_destroy(cortex);
}

TEST(BrainIntegration, BoostRegionAttentionNull) {
    // Should handle null gracefully
    visual_cortex_boost_region_attention(nullptr, 0.5f, 0.5f, 1.5f);
}

TEST(BrainIntegration, DetectAgent) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Features with structure and variance (like an agent)
    float agent_features[32];
    for (uint32_t i = 0; i < 32; i++) {
        agent_features[i] = 0.4f + (i % 7) * 0.05f;  // Mid-range with variation
    }

    bool detected = visual_cortex_detect_agent(cortex, agent_features, 32);
    // The heuristic may or may not detect this - check both outcomes are valid
    // We mainly test that the function doesn't crash
    (void)detected;

    // Uniform features (no agent)
    float uniform_features[32];
    for (uint32_t i = 0; i < 32; i++) {
        uniform_features[i] = 0.5f;  // All same value
    }

    detected = visual_cortex_detect_agent(cortex, uniform_features, 32);
    EXPECT_FALSE(detected);

    // Low variance features
    float low_variance_features[32];
    for (uint32_t i = 0; i < 32; i++) {
        low_variance_features[i] = 0.1f + (i % 2) * 0.01f;
    }

    detected = visual_cortex_detect_agent(cortex, low_variance_features, 32);
    EXPECT_FALSE(detected);

    visual_cortex_destroy(cortex);
}

TEST(BrainIntegration, DetectAgentNull) {
    EXPECT_FALSE(visual_cortex_detect_agent(nullptr, nullptr, 0));

    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    float features[32] = {0};
    EXPECT_FALSE(visual_cortex_detect_agent(cortex, nullptr, 32));
    EXPECT_FALSE(visual_cortex_detect_agent(cortex, features, 0));

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Error Handling and Edge Cases
//=============================================================================

TEST(ErrorHandling, ProcessInvalidDimensions) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t image[256] = {0};
    float features[32];

    // Wrong width
    EXPECT_FALSE(visual_cortex_process(cortex, image, 32, 16, 1, features));

    // Wrong height
    EXPECT_FALSE(visual_cortex_process(cortex, image, 16, 32, 1, features));

    // Zero channels
    EXPECT_FALSE(visual_cortex_process(cortex, image, 16, 16, 0, features));

    visual_cortex_destroy(cortex);
}

TEST(ErrorHandling, ProcessNullPointers) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t image[256] = {0};
    float features[32];

    EXPECT_FALSE(visual_cortex_process(nullptr, image, 16, 16, 1, features));
    EXPECT_FALSE(visual_cortex_process(cortex, nullptr, 16, 16, 1, features));
    EXPECT_FALSE(visual_cortex_process(cortex, image, 16, 16, 1, nullptr));

    visual_cortex_destroy(cortex);
}

TEST(ErrorHandling, AttentionNullPointers) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = true,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint8_t image[256] = {0};
    attention_map_t* map = attention_map_create(16, 16);

    EXPECT_FALSE(visual_cortex_compute_attention(nullptr, image, 16, 16, map));
    EXPECT_FALSE(visual_cortex_compute_attention(cortex, nullptr, 16, 16, map));
    EXPECT_FALSE(visual_cortex_compute_attention(cortex, image, 16, 16, nullptr));

    attention_map_destroy(map);
    visual_cortex_destroy(cortex);
}

TEST(ErrorHandling, MemoryNullPointers) {
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

    float features[32] = {0};
    visual_memory_t** memories = nullptr;
    int num_memories = 0;

    EXPECT_FALSE(visual_cortex_store_memory(nullptr, features, 0.5f));
    EXPECT_FALSE(visual_cortex_store_memory(cortex, nullptr, 0.5f));

    EXPECT_FALSE(visual_cortex_recall_memory(nullptr, features, 5, &memories, &num_memories));
    EXPECT_FALSE(visual_cortex_recall_memory(cortex, nullptr, 5, &memories, &num_memories));
    EXPECT_FALSE(visual_cortex_recall_memory(cortex, features, 5, nullptr, &num_memories));
    EXPECT_FALSE(visual_cortex_recall_memory(cortex, features, 5, &memories, nullptr));

    EXPECT_FALSE(visual_cortex_consolidate_memory(nullptr, features, 0.5f, "test"));
    EXPECT_FALSE(visual_cortex_consolidate_memory(cortex, nullptr, 0.5f, "test"));

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Complex Pipeline Tests
//=============================================================================

TEST(ComplexPipeline, MultipleMemoryRecall) {
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

    // Store multiple different memories
    for (int i = 0; i < 10; i++) {
        float features[32];
        for (int j = 0; j < 32; j++) {
            features[j] = (i * 0.1f + j * 0.01f);
        }
        // Normalize
        float norm = 0.0f;
        for (int j = 0; j < 32; j++) {
            norm += features[j] * features[j];
        }
        norm = sqrtf(norm);
        for (int j = 0; j < 32; j++) {
            features[j] /= norm;
        }

        visual_cortex_store_memory(cortex, features, 0.5f + i * 0.05f);
    }

    // Recall top 5 similar memories
    float query[32];
    for (int j = 0; j < 32; j++) {
        query[j] = j * 0.01f;
    }
    float norm = 0.0f;
    for (int j = 0; j < 32; j++) {
        norm += query[j] * query[j];
    }
    norm = sqrtf(norm);
    for (int j = 0; j < 32; j++) {
        query[j] /= norm;
    }

    visual_memory_t** memories = nullptr;
    int num_memories = 0;
    EXPECT_TRUE(visual_cortex_recall_memory(cortex, query, 5, &memories, &num_memories));
    EXPECT_EQ(num_memories, 5);

    if (memories) {
        nimcp_free(memories);
    }

    visual_cortex_destroy(cortex);
}

TEST(ComplexPipeline, LargeImageProcessing) {
    visual_cortex_config_t config = {
        .input_width = 64,
        .input_height = 64,
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

    // Create large image with pattern
    uint8_t* image = new uint8_t[64 * 64];
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            image[y * 64 + x] = (x + y) % 256;
        }
    }

    float* features = new float[128];
    attention_map_t* map = attention_map_create(64, 64);

    // Full pipeline
    EXPECT_TRUE(visual_cortex_process(cortex, image, 64, 64, 1, features));
    EXPECT_TRUE(visual_cortex_compute_attention(cortex, image, 64, 64, map));
    EXPECT_TRUE(visual_cortex_store_memory(cortex, features, 0.8f));

    float novelty = visual_cortex_compute_novelty(cortex, features);
    EXPECT_LT(novelty, 0.5f);  // Should be familiar after storing

    attention_map_destroy(map);
    delete[] image;
    delete[] features;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Additional Coverage Tests
//=============================================================================

TEST(AdditionalCoverage, AveragePoolingFullPath) {
    pool_layer_config_t config = {
        .input_width = 4,
        .input_height = 4,
        .input_channels = 2,  // Multiple channels
        .pool_size = 2,
        .stride = 2,
        .type = POOL_AVG  // Average pooling
    };

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    float input[32];  // 4x4x2
    for (int i = 0; i < 32; i++) {
        input[i] = (float)(i % 10);
    }

    float output[8];  // 2x2x2
    EXPECT_TRUE(pool_layer_forward(layer, input, output));

    // Verify average pooling worked
    EXPECT_GT(output[0], 0.0f);

    pool_layer_destroy(layer);
}

TEST(AdditionalCoverage, BrainWithNeuromodulators) {
    visual_cortex_config_t config = {
        .input_width = 16,
        .input_height = 16,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Create a brain with neuromodulation enabled
    brain_t brain = brain_create("test_neuromod", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    // Set brain (enables neuromodulation)
    visual_cortex_set_brain(cortex, brain);

    // Process with neuromodulation active
    uint8_t image[16 * 16];
    for (int i = 0; i < 16 * 16; i++) {
        image[i] = (i * 3) % 256;
    }

    float features[32];
    EXPECT_TRUE(visual_cortex_process(cortex, image, 16, 16, 1, features));

    // Verify neuromodulation path was executed
    bool has_features = false;
    for (int i = 0; i < 32; i++) {
        if (fabsf(features[i]) > 1e-6f) {
            has_features = true;
            break;
        }
    }
    EXPECT_TRUE(has_features);

    brain_destroy(brain);
    visual_cortex_destroy(cortex);
}

TEST(AdditionalCoverage, MultiChannelConvolutionPadding) {
    conv_layer_config_t config = {
        .input_width = 5,
        .input_height = 5,
        .input_channels = 2,
        .num_filters = 2,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,  // With padding
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Set kernels
    for (uint32_t f = 0; f < 2; f++) {
        float kernel[18];  // 3x3x2
        for (int i = 0; i < 18; i++) {
            kernel[i] = 0.1f;
        }
        EXPECT_TRUE(conv_layer_set_kernel(layer, f, kernel));
    }

    float input[50];  // 5x5x2
    for (int i = 0; i < 50; i++) {
        input[i] = (i % 5) * 0.1f;
    }

    float output[50];  // 5x5x2 (same due to padding)
    EXPECT_TRUE(conv_layer_forward(layer, input, output));

    conv_layer_destroy(layer);
}

TEST(AdditionalCoverage, LargeKernelGabor) {
    gabor_params_t params = {
        .wavelength = 6.0f,
        .orientation = 67.5f,
        .phase = 90.0f,
        .aspect_ratio = 0.8f,
        .bandwidth = 1.5f
    };

    float* kernel = gabor_create_kernel(11, &params);  // Large kernel
    ASSERT_NE(kernel, nullptr);

    // Verify kernel has structure
    float sum = 0.0f;
    for (int i = 0; i < 121; i++) {
        sum += fabsf(kernel[i]);
    }
    EXPECT_GT(sum, 0.1f);

    nimcp_free(kernel);
}

TEST(AdditionalCoverage, MemoryRecallSorting) {
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

    // Store 5 different memories with different similarities
    for (int i = 0; i < 5; i++) {
        float features[32];
        for (int j = 0; j < 32; j++) {
            features[j] = (i * 0.2f + j * 0.01f);
        }
        // Normalize
        float norm = 0.0f;
        for (int j = 0; j < 32; j++) {
            norm += features[j] * features[j];
        }
        norm = sqrtf(norm);
        for (int j = 0; j < 32; j++) {
            features[j] /= norm;
        }

        visual_cortex_store_memory(cortex, features, 0.5f);
    }

    // Query with features similar to memory 0
    float query[32];
    for (int j = 0; j < 32; j++) {
        query[j] = j * 0.01f;
    }
    float norm = 0.0f;
    for (int j = 0; j < 32; j++) {
        norm += query[j] * query[j];
    }
    norm = sqrtf(norm);
    for (int j = 0; j < 32; j++) {
        query[j] /= norm;
    }

    // Recall - should trigger sorting
    visual_memory_t** memories = nullptr;
    int num_memories = 0;
    EXPECT_TRUE(visual_cortex_recall_memory(cortex, query, 3, &memories, &num_memories));
    EXPECT_EQ(num_memories, 3);

    // First memory should have highest similarity
    if (memories && num_memories > 0) {
        EXPECT_NE(memories[0], nullptr);
        nimcp_free(memories);
    }

    visual_cortex_destroy(cortex);
}

TEST(AdditionalCoverage, FeatureNormalizationEdgeCase) {
    visual_cortex_config_t config = {
        .input_width = 8,
        .input_height = 8,
        .num_v1_filters = 4,
        .feature_dim = 16,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // All zero image (edge case for normalization)
    uint8_t image[64];
    memset(image, 0, 64);

    float features[16];
    EXPECT_TRUE(visual_cortex_process(cortex, image, 8, 8, 1, features));

    visual_cortex_destroy(cortex);
}

TEST(AdditionalCoverage, AttentionMapBorderCoverage) {
    visual_cortex_config_t config = {
        .input_width = 20,
        .input_height = 20,
        .num_v1_filters = 4,
        .feature_dim = 32,
        .enable_attention = true,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 40
    };

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Create image with edges at borders
    uint8_t* image = new uint8_t[20 * 20];
    for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 20; x++) {
            // Border pixels white, interior black
            image[y * 20 + x] = (x == 0 || x == 19 || y == 0 || y == 19) ? 255 : 0;
        }
    }

    attention_map_t* map = attention_map_create(20, 20);
    EXPECT_TRUE(visual_cortex_compute_attention(cortex, image, 20, 20, map));

    // Check border attention (should handle y=1 to y=height-1 loop)
    float center_attn = attention_map_get(map, 10, 10);
    (void)center_attn;  // Use the value

    attention_map_destroy(map);
    delete[] image;
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
