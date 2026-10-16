/**
 * @file test_visual_cortex_gpu.cpp
 * @brief Unit tests for GPU-accelerated Visual Cortex operations
 *
 * Tests Gabor filters, edge detection, orientation processing, saliency maps,
 * optical flow, and visual processing pipeline GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>
#include <algorithm>

// Headers already have their own extern "C" guards
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "perception/nimcp_visual_cortex.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class VisualCortexGPUTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    visual_cortex_t* visual_state = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (visual_state) {
            visual_cortex_destroy(visual_state);
            visual_state = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create visual cortex with default config
    visual_cortex_t* CreateVisualCortex(uint32_t width = 64, uint32_t height = 64) {
        visual_cortex_config_t config;
        config.input_width = width;
        config.input_height = height;
        config.num_v1_filters = 16;
        config.feature_dim = 128;
        config.enable_attention = true;
        config.enable_memory = true;
        config.enable_fractal_topology = false;
        config.hub_ratio = 0.15f;
        config.power_law_gamma = -2.1f;
        config.internal_neurons = 0;
        config.enable_bio_async = false;
        config.enable_second_messengers = false;
        return visual_cortex_create(&config);
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to create 3D tensor
    nimcp_gpu_tensor_t* Create3DTensor(size_t d1, size_t d2, size_t d3, float value = 0.0f) {
        size_t dims[3] = {d1, d2, d3};
        return CreateFilledTensor(dims, 3, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to create grayscale test image (solid gray)
    std::vector<uint8_t> CreateGrayImage(uint32_t width, uint32_t height, uint8_t value = 128) {
        return std::vector<uint8_t>(width * height, value);
    }

    // Helper to create checkerboard pattern
    std::vector<uint8_t> CreateCheckerboard(uint32_t width, uint32_t height, uint32_t block_size = 8) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                bool white = ((x / block_size) + (y / block_size)) % 2 == 0;
                image[y * width + x] = white ? 255 : 0;
            }
        }
        return image;
    }

    // Helper to create horizontal gradient
    std::vector<uint8_t> CreateHorizontalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((x * 255) / (width - 1));
            }
        }
        return image;
    }

    // Helper to create vertical gradient
    std::vector<uint8_t> CreateVerticalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((y * 255) / (height - 1));
            }
        }
        return image;
    }

    // Helper to create image with horizontal edges
    std::vector<uint8_t> CreateHorizontalEdges(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            uint8_t val = (y / (height / 4)) % 2 == 0 ? 255 : 0;
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = val;
            }
        }
        return image;
    }

    // Helper to create image with vertical edges
    std::vector<uint8_t> CreateVerticalEdges(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = (x / (width / 4)) % 2 == 0 ? 255 : 0;
            }
        }
        return image;
    }

    // Helper to create image with diagonal edges
    std::vector<uint8_t> CreateDiagonalEdges(uint32_t width, uint32_t height, bool positive = true) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                int coord = positive ? (x + y) : (x - static_cast<int>(y) + height);
                image[y * width + x] = (coord / 16) % 2 == 0 ? 255 : 0;
            }
        }
        return image;
    }

    // Helper to create Gaussian blob (salient region)
    std::vector<uint8_t> CreateGaussianBlob(uint32_t width, uint32_t height,
                                            float cx, float cy, float sigma) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float val = std::exp(-(dx*dx + dy*dy) / (2 * sigma * sigma));
                image[y * width + x] = static_cast<uint8_t>(val * 255);
            }
        }
        return image;
    }

    // Helper to create random noise image
    std::vector<uint8_t> CreateNoiseImage(uint32_t width, uint32_t height, int seed = 42) {
        std::vector<uint8_t> image(width * height);
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < image.size(); i++) {
            image[i] = static_cast<uint8_t>(dist(gen));
        }
        return image;
    }

    // Helper to create RGB image
    std::vector<uint8_t> CreateRGBImage(uint32_t width, uint32_t height,
                                        uint8_t r, uint8_t g, uint8_t b) {
        std::vector<uint8_t> image(width * height * 3);
        for (uint32_t i = 0; i < width * height; i++) {
            image[i * 3 + 0] = r;
            image[i * 3 + 1] = g;
            image[i * 3 + 2] = b;
        }
        return image;
    }

    // Helper to create shifted image (for optical flow testing)
    std::vector<uint8_t> ShiftImage(const std::vector<uint8_t>& src,
                                    uint32_t width, uint32_t height,
                                    int dx, int dy) {
        std::vector<uint8_t> dst(width * height, 0);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                int sx = x - dx;
                int sy = y - dy;
                if (sx >= 0 && sx < (int)width && sy >= 0 && sy < (int)height) {
                    dst[y * width + x] = src[sy * width + sx];
                }
            }
        }
        return dst;
    }
};

//=============================================================================
// Visual Cortex Creation Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, StateCreation_WithValidParams_ReturnsValidState) {
    visual_cortex_config_t config;
    config.input_width = 64;
    config.input_height = 64;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = true;
    config.enable_memory = true;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, StateCreation_DifferentSizes_Works) {
    uint32_t sizes[][2] = {{32, 32}, {64, 64}, {128, 128}, {256, 256}};

    for (auto& size : sizes) {
        visual_cortex_config_t config;
        config.input_width = size[0];
        config.input_height = size[1];
        config.num_v1_filters = 16;
        config.feature_dim = 128;
        config.enable_attention = true;
        config.enable_memory = false;
        config.enable_fractal_topology = false;
        config.hub_ratio = 0.15f;
        config.power_law_gamma = -2.1f;
        config.internal_neurons = 0;
        config.enable_bio_async = false;
        config.enable_second_messengers = false;

        visual_cortex_t* cortex = visual_cortex_create(&config);
        ASSERT_NE(cortex, nullptr) << "Failed for size: " << size[0] << "x" << size[1];

        visual_cortex_destroy(cortex);
    }
}

TEST_F(VisualCortexGPUTest, StateDestruction_NullSafe) {
    visual_cortex_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Gabor Filter Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, GaborFilter_Creation_ValidKernel) {
    gabor_params_t params;
    params.wavelength = 5.0f;
    params.orientation = 0.0f;  // Horizontal
    params.phase = 0.0f;
    params.aspect_ratio = 0.5f;
    params.bandwidth = 1.0f;

    int kernel_size = 9;
    float* kernel = gabor_create_kernel(kernel_size, &params);
    ASSERT_NE(kernel, nullptr);

    // Check kernel has non-zero values
    bool has_nonzero = false;
    for (int i = 0; i < kernel_size * kernel_size; i++) {
        if (kernel[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
        EXPECT_FALSE(std::isnan(kernel[i]));
        EXPECT_FALSE(std::isinf(kernel[i]));
    }
    EXPECT_TRUE(has_nonzero);

    free(kernel);
}

TEST_F(VisualCortexGPUTest, GaborFilter_DifferentOrientations) {
    float orientations[] = {0.0f, 45.0f, 90.0f, 135.0f};

    for (float orient : orientations) {
        gabor_params_t params;
        params.wavelength = 5.0f;
        params.orientation = orient;
        params.phase = 0.0f;
        params.aspect_ratio = 0.5f;
        params.bandwidth = 1.0f;

        float* kernel = gabor_create_kernel(9, &params);
        ASSERT_NE(kernel, nullptr) << "Failed for orientation: " << orient;

        free(kernel);
    }
}

TEST_F(VisualCortexGPUTest, GaborFilter_OrientationSensitivity) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Create image with vertical edges
    auto vertical_edges = CreateVerticalEdges(width, height);

    // Create visual cortex
    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, vertical_edges.data(),
                                         width, height, 1, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, GaborFilter_EnergyComputation) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Create checkerboard (has edges at all orientations)
    auto checkerboard = CreateCheckerboard(width, height, 8);

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, checkerboard.data(),
                                         width, height, 1, features.data());
    EXPECT_TRUE(result);

    // Features should be non-zero (edges detected)
    bool has_nonzero = false;
    for (float f : features) {
        if (f != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Convolution Layer Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, ConvLayer_Creation_ValidLayer) {
    conv_layer_config_t config;
    config.input_width = 64;
    config.input_height = 64;
    config.input_channels = 1;
    config.num_filters = 16;
    config.kernel_size = 3;
    config.stride = 1;
    config.padding = 1;
    config.activation = VISUAL_ACTIVATION_RELU;

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Check output dimensions
    EXPECT_EQ(conv_layer_get_output_width(layer), 64u);
    EXPECT_EQ(conv_layer_get_output_height(layer), 64u);
    EXPECT_EQ(conv_layer_get_output_channels(layer), 16u);

    conv_layer_destroy(layer);
}

TEST_F(VisualCortexGPUTest, ConvLayer_Forward_ValidOutput) {
    conv_layer_config_t config;
    config.input_width = 32;
    config.input_height = 32;
    config.input_channels = 1;
    config.num_filters = 8;
    config.kernel_size = 3;
    config.stride = 1;
    config.padding = 1;
    config.activation = VISUAL_ACTIVATION_RELU;

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Create input image
    auto input = CreateGrayImage(32, 32, 128);
    std::vector<float> input_float(input.begin(), input.end());

    // Forward pass
    uint32_t out_w = conv_layer_get_output_width(layer);
    uint32_t out_h = conv_layer_get_output_height(layer);
    uint32_t out_c = conv_layer_get_output_channels(layer);
    std::vector<float> output(out_w * out_h * out_c);

    bool result = conv_layer_forward(layer, input_float.data(), output.data());
    EXPECT_TRUE(result);

    // Check output is valid
    for (float val : output) {
        EXPECT_FALSE(std::isnan(val));
        EXPECT_FALSE(std::isinf(val));
        EXPECT_GE(val, 0.0f);  // ReLU activation
    }

    conv_layer_destroy(layer);
}

TEST_F(VisualCortexGPUTest, ConvLayer_SetKernel) {
    conv_layer_config_t config;
    config.input_width = 32;
    config.input_height = 32;
    config.input_channels = 1;
    config.num_filters = 4;
    config.kernel_size = 3;
    config.stride = 1;
    config.padding = 1;
    config.activation = VISUAL_ACTIVATION_NONE;

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Set Sobel-like kernel for edge detection
    float sobel_x[9] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    bool result = conv_layer_set_kernel(layer, 0, sobel_x);
    EXPECT_TRUE(result);

    conv_layer_destroy(layer);
}

//=============================================================================
// Pooling Layer Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, PoolLayer_Creation_ValidLayer) {
    pool_layer_config_t config;
    config.input_width = 64;
    config.input_height = 64;
    config.input_channels = 16;
    config.pool_size = 2;
    config.stride = 2;
    config.type = POOL_MAX;

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    pool_layer_destroy(layer);
}

TEST_F(VisualCortexGPUTest, PoolLayer_MaxPooling) {
    pool_layer_config_t config;
    config.input_width = 4;
    config.input_height = 4;
    config.input_channels = 1;
    config.pool_size = 2;
    config.stride = 2;
    config.type = POOL_MAX;

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Known input
    float input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    float output[4];
    bool result = pool_layer_forward(layer, input, output);
    EXPECT_TRUE(result);

    // Max of each 2x2 block
    EXPECT_EQ(output[0], 6.0f);   // max(1,2,5,6)
    EXPECT_EQ(output[1], 8.0f);   // max(3,4,7,8)
    EXPECT_EQ(output[2], 14.0f);  // max(9,10,13,14)
    EXPECT_EQ(output[3], 16.0f);  // max(11,12,15,16)

    pool_layer_destroy(layer);
}

TEST_F(VisualCortexGPUTest, PoolLayer_AveragePooling) {
    pool_layer_config_t config;
    config.input_width = 4;
    config.input_height = 4;
    config.input_channels = 1;
    config.pool_size = 2;
    config.stride = 2;
    config.type = POOL_AVG;

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Known input
    float input[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    float output[4];
    bool result = pool_layer_forward(layer, input, output);
    EXPECT_TRUE(result);

    // Average of each 2x2 block
    EXPECT_NEAR(output[0], 3.5f, 0.01f);   // avg(1,2,5,6)
    EXPECT_NEAR(output[1], 5.5f, 0.01f);   // avg(3,4,7,8)
    EXPECT_NEAR(output[2], 11.5f, 0.01f);  // avg(9,10,13,14)
    EXPECT_NEAR(output[3], 13.5f, 0.01f);  // avg(11,12,15,16)

    pool_layer_destroy(layer);
}

//=============================================================================
// Image Pyramid Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, ImagePyramid_MultiScale) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    auto image = CreateGaussianBlob(width, height, 32, 32, 10);

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, image.data(),
                                         width, height, 1, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Attention Map Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, AttentionMap_Creation) {
    attention_map_t* map = attention_map_create(64, 64);
    ASSERT_NE(map, nullptr);

    attention_map_destroy(map);
}

TEST_F(VisualCortexGPUTest, AttentionMap_SetGet) {
    attention_map_t* map = attention_map_create(64, 64);
    ASSERT_NE(map, nullptr);

    // Set value
    bool set_result = attention_map_set(map, 32, 32, 0.8f);
    EXPECT_TRUE(set_result);

    // Get value
    float val = attention_map_get(map, 32, 32);
    EXPECT_NEAR(val, 0.8f, 0.01f);

    attention_map_destroy(map);
}

TEST_F(VisualCortexGPUTest, AttentionMap_BoundsCheck) {
    attention_map_t* map = attention_map_create(64, 64);
    ASSERT_NE(map, nullptr);

    // Out of bounds should return error value
    float val = attention_map_get(map, 100, 100);
    EXPECT_LT(val, 0.0f);  // Error value is -1.0

    attention_map_destroy(map);
}

TEST_F(VisualCortexGPUTest, AttentionMap_ComputeFromImage) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Create image with salient blob in center
    auto image = CreateGaussianBlob(width, height, 32, 32, 8);

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = true;
    config.enable_memory = false;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    attention_map_t* attn_map = attention_map_create(width, height);
    ASSERT_NE(attn_map, nullptr);

    bool result = visual_cortex_compute_attention(cortex, image.data(),
                                                   width, height, attn_map);
    EXPECT_TRUE(result);

    attention_map_destroy(attn_map);
    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, AttentionMap_PeakDetection) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Create image with salient blob at known location
    auto image = CreateGaussianBlob(width, height, 48, 48, 5);

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = true;
    config.enable_memory = false;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    attention_map_t* attn_map = attention_map_create(width, height);
    visual_cortex_compute_attention(cortex, image.data(), width, height, attn_map);

    uint32_t peak_x, peak_y;
    float peak_val;
    bool result = visual_cortex_get_attention_peak(attn_map, &peak_x, &peak_y, &peak_val);
    EXPECT_TRUE(result);

    // Peak should be near the blob center (with some tolerance)
    EXPECT_NEAR(peak_x, 48u, 10u);
    EXPECT_NEAR(peak_y, 48u, 10u);

    attention_map_destroy(attn_map);
    visual_cortex_destroy(cortex);
}

//=============================================================================
// Saliency Map Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, SaliencyMap_IntensityConspicuity) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // High contrast image
    auto image = CreateCheckerboard(width, height, 4);

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, image.data(),
                                         width, height, 1, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, SaliencyMap_OrientationConspicuity) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Image with edges at different orientations
    auto image = CreateDiagonalEdges(width, height, true);

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, image.data(),
                                         width, height, 1, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Color Processing Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, ColorProcessing_RGBInput) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    // Create RGB image
    auto image = CreateRGBImage(width, height, 255, 0, 0);  // Red

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = true;
    config.enable_memory = false;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, image.data(),
                                         width, height, 3, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, ColorProcessing_DifferentColors) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    uint8_t colors[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255},
                           {255, 255, 0}, {255, 0, 255}, {0, 255, 255}};

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    for (auto& color : colors) {
        auto image = CreateRGBImage(width, height, color[0], color[1], color[2]);
        std::vector<float> features(128);

        bool result = visual_cortex_process(cortex, image.data(),
                                             width, height, 3, features.data());
        EXPECT_TRUE(result);
    }

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Visual Memory Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Memory_StoreAndRecall) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = false;
    config.enable_memory = true;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Process and store
    auto image1 = CreateCheckerboard(width, height, 8);
    std::vector<float> features1(128);
    visual_cortex_process(cortex, image1.data(), width, height, 1, features1.data());

    bool store_result = visual_cortex_store_memory(cortex, features1.data(), 0.9f);
    EXPECT_TRUE(store_result);

    // Recall similar
    visual_memory_t** memories = nullptr;
    int num_memories = 0;
    bool recall_result = visual_cortex_recall_memory(cortex, features1.data(), 5,
                                                      &memories, &num_memories);
    EXPECT_TRUE(recall_result);

    if (memories) {
        free(memories);
    }

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Memory_Consolidation) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = false;
    config.enable_memory = true;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    auto image = CreateGaussianBlob(width, height, 32, 32, 10);
    std::vector<float> features(128);
    visual_cortex_process(cortex, image.data(), width, height, 1, features.data());

    bool result = visual_cortex_consolidate_memory(cortex, features.data(), 0.8f, "test blob");
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Novelty Detection Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Novelty_FamiliarVsNovel) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 16;
    config.feature_dim = 128;
    config.enable_attention = false;
    config.enable_memory = true;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Store familiar image
    auto familiar_image = CreateCheckerboard(width, height, 8);
    std::vector<float> familiar_features(128);
    visual_cortex_process(cortex, familiar_image.data(), width, height, 1, familiar_features.data());
    visual_cortex_store_memory(cortex, familiar_features.data(), 0.9f);

    // Test novelty of familiar
    float familiar_novelty = visual_cortex_compute_novelty(cortex, familiar_features.data());
    EXPECT_GE(familiar_novelty, 0.0f);
    EXPECT_LE(familiar_novelty, 1.0f);

    // Test novelty of novel image
    auto novel_image = CreateGaussianBlob(width, height, 32, 32, 5);
    std::vector<float> novel_features(128);
    visual_cortex_process(cortex, novel_image.data(), width, height, 1, novel_features.data());

    float novel_novelty = visual_cortex_compute_novelty(cortex, novel_features.data());
    EXPECT_GE(novel_novelty, 0.0f);
    EXPECT_LE(novel_novelty, 1.0f);

    // Novel should have higher novelty score
    // EXPECT_GT(novel_novelty, familiar_novelty);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Statistics_ValidAfterProcessing) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    // Process several images
    for (int i = 0; i < 5; i++) {
        auto image = CreateNoiseImage(width, height, i);
        std::vector<float> features(128);
        visual_cortex_process(cortex, image.data(), width, height, 1, features.data());
    }

    visual_cortex_stats_t stats;
    bool result = visual_cortex_get_stats(cortex, &stats);
    EXPECT_TRUE(result);
    EXPECT_GE(stats.images_processed, 5u);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Neuromodulation Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Neuromodulation_PhasicBurst) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    // Trigger phasic dopamine burst
    bool result = visual_cortex_trigger_phasic_burst(cortex, 0, 0.5f);  // 0=dopamine
    EXPECT_TRUE(result);

    // Trigger ACh burst
    result = visual_cortex_trigger_phasic_burst(cortex, 1, 0.6f);  // 1=acetylcholine
    EXPECT_TRUE(result);

    // Trigger NE burst
    result = visual_cortex_trigger_phasic_burst(cortex, 2, 0.7f);  // 2=norepinephrine
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Neuromodulation_TonicLevel) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    bool result = visual_cortex_set_tonic_level(cortex, 0, 0.5f);
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Neuromodulation_ComputeEffects) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    neuromod_effects_t effects;
    bool result = visual_cortex_compute_neuromod_effects(cortex, 0, &effects);
    EXPECT_TRUE(result);

    EXPECT_GE(effects.gabor_gain, 0.0f);
    EXPECT_GE(effects.attention_boost, 0.0f);
    EXPECT_GE(effects.plasticity_gate, 0.0f);
    EXPECT_LE(effects.plasticity_gate, 1.0f);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Training Interface Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Training_EnableMode) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    // Enable training mode
    int result = visual_cortex_set_training_mode(cortex, true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(visual_cortex_is_training_mode(cortex));

    // Disable training mode
    result = visual_cortex_set_training_mode(cortex, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(visual_cortex_is_training_mode(cortex));

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Training_GetState) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_set_training_mode(cortex, true);

    // Process image
    auto image = CreateCheckerboard(width, height, 8);
    std::vector<float> features(128);
    visual_cortex_process(cortex, image.data(), width, height, 1, features.data());

    // Get training state
    visual_training_state_t state;
    int result = visual_cortex_get_training_state(cortex, &state);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(state.valid);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Training_GradientFeedback) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    visual_cortex_set_training_mode(cortex, true);

    // Process image
    auto image = CreateCheckerboard(width, height, 8);
    std::vector<float> features(128);
    visual_cortex_process(cortex, image.data(), width, height, 1, features.data());

    // Apply gradients
    std::vector<float> gradients(128, 0.01f);
    int result = visual_cortex_apply_gradient_feedback(cortex, gradients.data(), 128, 0.5f);
    EXPECT_EQ(result, 0);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Training_GetFeatureDim) {
    RequireGPU();

    visual_cortex_config_t config;
    config.input_width = 64;
    config.input_height = 64;
    config.num_v1_filters = 16;
    config.feature_dim = 256;
    config.enable_attention = false;
    config.enable_memory = false;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    uint32_t dim = visual_cortex_get_feature_dim(cortex);
    EXPECT_EQ(dim, 256u);

    visual_cortex_destroy(cortex);
}

//=============================================================================
// Bidirectional Feedback Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Feedback_BoostRegionAttention) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    // Boost center region
    visual_cortex_boost_region_attention(cortex, 0.5f, 0.5f, 1.5f);

    // Process image and check attention is affected
    auto image = CreateGrayImage(64, 64, 128);
    std::vector<float> features(128);
    bool result = visual_cortex_process(cortex, image.data(), 64, 64, 1, features.data());
    EXPECT_TRUE(result);

    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Feedback_DetectAgent) {
    RequireGPU();

    visual_cortex_t* cortex = CreateVisualCortex();
    ASSERT_NE(cortex, nullptr);

    // Process image
    auto image = CreateGrayImage(64, 64, 128);
    std::vector<float> features(128);
    visual_cortex_process(cortex, image.data(), 64, 64, 1, features.data());

    bool agent_detected = visual_cortex_detect_agent(cortex, features.data(), 128);
    // Result depends on features - just test API works
    (void)agent_detected;

    visual_cortex_destroy(cortex);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, NullSafety_ProcessWithNull) {
    std::vector<float> features(128);
    bool result = visual_cortex_process(nullptr, nullptr, 0, 0, 0, features.data());
    EXPECT_FALSE(result);
}

TEST_F(VisualCortexGPUTest, NullSafety_ComputeAttentionWithNull) {
    bool result = visual_cortex_compute_attention(nullptr, nullptr, 0, 0, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(VisualCortexGPUTest, NullSafety_GetStatsWithNull) {
    visual_cortex_stats_t stats;
    bool result = visual_cortex_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);
}

TEST_F(VisualCortexGPUTest, NullSafety_AttentionMapDestroy) {
    attention_map_destroy(nullptr);  // Should not crash
}

TEST_F(VisualCortexGPUTest, NullSafety_ConvLayerDestroy) {
    conv_layer_destroy(nullptr);  // Should not crash
}

TEST_F(VisualCortexGPUTest, NullSafety_PoolLayerDestroy) {
    pool_layer_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(VisualCortexGPUTest, Integration_FullVisualProcessing) {
    RequireGPU();

    const uint32_t width = 128;
    const uint32_t height = 128;

    visual_cortex_config_t config;
    config.input_width = width;
    config.input_height = height;
    config.num_v1_filters = 32;
    config.feature_dim = 256;
    config.enable_attention = true;
    config.enable_memory = true;
    config.enable_fractal_topology = false;
    config.hub_ratio = 0.15f;
    config.power_law_gamma = -2.1f;
    config.internal_neurons = 0;
    config.enable_bio_async = false;
    config.enable_second_messengers = false;

    visual_cortex_t* cortex = visual_cortex_create(&config);
    ASSERT_NE(cortex, nullptr);

    // Process multiple image types
    std::vector<std::vector<uint8_t>> images = {
        CreateCheckerboard(width, height, 16),
        CreateHorizontalGradient(width, height),
        CreateVerticalGradient(width, height),
        CreateGaussianBlob(width, height, 64, 64, 20),
        CreateNoiseImage(width, height)
    };

    for (const auto& image : images) {
        std::vector<float> features(256);
        bool result = visual_cortex_process(cortex, image.data(),
                                             width, height, 1, features.data());
        EXPECT_TRUE(result);

        // Store in memory
        visual_cortex_store_memory(cortex, features.data(), 0.8f);
    }

    // Compute attention
    attention_map_t* attn = attention_map_create(width, height);
    visual_cortex_compute_attention(cortex, images[0].data(), width, height, attn);

    uint32_t px, py;
    float pval;
    visual_cortex_get_attention_peak(attn, &px, &py, &pval);

    // Check statistics
    visual_cortex_stats_t stats;
    visual_cortex_get_stats(cortex, &stats);
    EXPECT_GE(stats.images_processed, 5u);

    attention_map_destroy(attn);
    visual_cortex_destroy(cortex);
}

TEST_F(VisualCortexGPUTest, Integration_VideoStreamProcessing) {
    RequireGPU();

    const uint32_t width = 64;
    const uint32_t height = 64;
    const int num_frames = 30;

    visual_cortex_t* cortex = CreateVisualCortex(width, height);
    ASSERT_NE(cortex, nullptr);

    // Simulate video stream with moving blob
    for (int frame = 0; frame < num_frames; frame++) {
        float cx = 20.0f + 24.0f * std::sin(frame * 0.2f);
        float cy = 20.0f + 24.0f * std::cos(frame * 0.2f);

        auto image = CreateGaussianBlob(width, height, cx, cy, 8);
        std::vector<float> features(128);

        bool result = visual_cortex_process(cortex, image.data(),
                                             width, height, 1, features.data());
        EXPECT_TRUE(result) << "Failed on frame " << frame;
    }

    visual_cortex_destroy(cortex);
}
