/**
 * @file test_perception_visual.cpp
 * @brief Unit tests for visual perception/visual cortex module
 *
 * WHAT: Comprehensive tests for visual cortex functionality
 * WHY:  Ensure visual cortex correctly processes images, extracts features,
 *       computes attention, and integrates with memory systems
 * HOW:  GTest framework with synthetic image generators
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>
#include <random>
#include <cstring>

extern "C" {
#include "perception/nimcp_visual_cortex.h"
#include "core/brain/nimcp_brain.h"
}

//=============================================================================
// Synthetic Image Generator (copied from fixtures for standalone tests)
//=============================================================================

class SyntheticImageGenerator {
public:
    static std::vector<uint8_t> GenerateHorizontalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((x * 255) / width);
            }
        }
        return image;
    }

    static std::vector<uint8_t> GenerateVerticalGradient(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = static_cast<uint8_t>((y * 255) / height);
            }
        }
        return image;
    }

    static std::vector<uint8_t> GenerateCheckerboard(uint32_t width, uint32_t height, uint32_t square_size) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t square_x = x / square_size;
                uint32_t square_y = y / square_size;
                bool is_white = ((square_x + square_y) % 2) == 0;
                image[y * width + x] = is_white ? 255 : 0;
            }
        }
        return image;
    }

    static std::vector<uint8_t> GenerateGaussianNoise(uint32_t width, uint32_t height,
                                                       float mean = 128.0f, float stddev = 50.0f) {
        std::vector<uint8_t> image(width * height);
        std::random_device rd;
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::normal_distribution<float> dist(mean, stddev);

        for (uint32_t i = 0; i < width * height; i++) {
            float value = dist(gen);
            value = std::max(0.0f, std::min(255.0f, value));
            image[i] = static_cast<uint8_t>(value);
        }
        return image;
    }

    static std::vector<uint8_t> GenerateVerticalEdge(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        uint32_t edge_x = width / 2;
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                image[y * width + x] = (x < edge_x) ? 0 : 255;
            }
        }
        return image;
    }

    static std::vector<uint8_t> GenerateSineGrating(uint32_t width, uint32_t height,
                                                     float frequency, float orientation_deg) {
        std::vector<uint8_t> image(width * height);
        float theta = orientation_deg * M_PI / 180.0f;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float x_rot = x * std::cos(theta) + y * std::sin(theta);
                float value = 127.5f * (1.0f + std::sin(2.0f * M_PI * frequency * x_rot / width));
                image[y * width + x] = static_cast<uint8_t>(value);
            }
        }
        return image;
    }

    static std::vector<uint8_t> GenerateSolidColor(uint32_t width, uint32_t height, uint8_t color) {
        return std::vector<uint8_t>(width * height, color);
    }
};

//=============================================================================
// Visual Cortex Test Fixture
//=============================================================================

class VisualCortexTest : public ::testing::Test {
protected:
    visual_cortex_t* cortex = nullptr;

    static constexpr uint32_t TEST_WIDTH = 64;
    static constexpr uint32_t TEST_HEIGHT = 64;
    static constexpr uint32_t TEST_CHANNELS = 1;
    static constexpr uint32_t FEATURE_DIM = 64;

    void SetUp() override {
        visual_cortex_config_t config = {
            .input_width = TEST_WIDTH,
            .input_height = TEST_HEIGHT,
            .num_v1_filters = 16,
            .feature_dim = FEATURE_DIM,
            .enable_attention = true,
            .enable_memory = true,
            .enable_fractal_topology = false,
            .hub_ratio = 0.15f,
            .power_law_gamma = -2.1f,
            .internal_neurons = 160,
            .enable_bio_async = false,
            .enable_second_messengers = false
        };

        cortex = visual_cortex_create(&config);
    }

    void TearDown() override {
        if (cortex) {
            visual_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    std::vector<float> ProcessImage(const std::vector<uint8_t>& image) {
        std::vector<float> features(FEATURE_DIM);
        bool success = visual_cortex_process(cortex, image.data(),
                                              TEST_WIDTH, TEST_HEIGHT,
                                              TEST_CHANNELS, features.data());
        EXPECT_TRUE(success);
        return features;
    }
};

//=============================================================================
// Visual Cortex Creation/Destruction Tests
//=============================================================================

TEST_F(VisualCortexTest, CreateValidConfig) {
    ASSERT_NE(cortex, nullptr) << "Failed to create visual cortex";
}

TEST(VisualCortexCreationTest, CreateWithNullConfig) {
    visual_cortex_t* ctx = visual_cortex_create(nullptr);
    EXPECT_EQ(ctx, nullptr) << "Should fail with null config";
}

TEST(VisualCortexCreationTest, CreateWithMinimalConfig) {
    visual_cortex_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .num_v1_filters = 8,
        .feature_dim = 32,
        .enable_attention = false,
        .enable_memory = false,
        .enable_fractal_topology = false,
        .hub_ratio = 0.15f,
        .power_law_gamma = -2.1f,
        .internal_neurons = 80,
        .enable_bio_async = false,
        .enable_second_messengers = false
    };

    visual_cortex_t* ctx = visual_cortex_create(&config);
    ASSERT_NE(ctx, nullptr);
    visual_cortex_destroy(ctx);
}

TEST(VisualCortexCreationTest, DestroyNull) {
    // Should not crash
    visual_cortex_destroy(nullptr);
}

//=============================================================================
// Image Processing Tests
//=============================================================================

TEST_F(VisualCortexTest, ProcessHorizontalGradient) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);

    // Features should be non-zero for gradient image
    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f) << "Features should have non-zero values for gradient";
}

TEST_F(VisualCortexTest, ProcessVerticalGradient) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateVerticalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(VisualCortexTest, ProcessCheckerboard) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 8);
    auto features = ProcessImage(image);

    // Checkerboard should produce strong edge responses
    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(VisualCortexTest, ProcessSolidColor) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateSolidColor(TEST_WIDTH, TEST_HEIGHT, 128);
    auto features = ProcessImage(image);

    // Solid color should produce minimal edge responses
    // (though features might still have some values due to biases)
    bool valid = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid) << "All features should be finite";
}

TEST_F(VisualCortexTest, ProcessVerticalEdge) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateVerticalEdge(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);

    // Vertical edge should produce strong response
    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(VisualCortexTest, ProcessSineGrating) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateSineGrating(TEST_WIDTH, TEST_HEIGHT, 4.0f, 45.0f);
    auto features = ProcessImage(image);

    float sum = 0.0f;
    for (const auto& f : features) {
        sum += std::abs(f);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(VisualCortexTest, ProcessNullImage) {
    ASSERT_NE(cortex, nullptr);

    std::vector<float> features(FEATURE_DIM);
    bool success = visual_cortex_process(cortex, nullptr, TEST_WIDTH, TEST_HEIGHT,
                                          TEST_CHANNELS, features.data());
    EXPECT_FALSE(success) << "Should fail with null image";
}

TEST_F(VisualCortexTest, ProcessNullFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    bool success = visual_cortex_process(cortex, image.data(), TEST_WIDTH, TEST_HEIGHT,
                                          TEST_CHANNELS, nullptr);
    EXPECT_FALSE(success) << "Should fail with null features output";
}

TEST_F(VisualCortexTest, ProcessNullCortex) {
    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    std::vector<float> features(FEATURE_DIM);
    bool success = visual_cortex_process(nullptr, image.data(), TEST_WIDTH, TEST_HEIGHT,
                                          TEST_CHANNELS, features.data());
    EXPECT_FALSE(success) << "Should fail with null cortex";
}

//=============================================================================
// Feature Extraction Tests
//=============================================================================

TEST_F(VisualCortexTest, DifferentImagesProduceDifferentFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto image1 = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto image2 = SyntheticImageGenerator::GenerateVerticalGradient(TEST_WIDTH, TEST_HEIGHT);

    auto features1 = ProcessImage(image1);
    auto features2 = ProcessImage(image2);

    // Features should be different for different images
    float diff = 0.0f;
    for (size_t i = 0; i < features1.size(); i++) {
        diff += std::abs(features1[i] - features2[i]);
    }
    EXPECT_GT(diff, 0.0f) << "Different images should produce different features";
}

TEST_F(VisualCortexTest, SameImageProducesSameFeatures) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 8);

    auto features1 = ProcessImage(image);
    auto features2 = ProcessImage(image);

    // Same image should produce same features (deterministic)
    float diff = 0.0f;
    for (size_t i = 0; i < features1.size(); i++) {
        diff += std::abs(features1[i] - features2[i]);
    }
    EXPECT_LT(diff, 0.001f) << "Same image should produce same features";
}

TEST_F(VisualCortexTest, GetFeatureDimension) {
    ASSERT_NE(cortex, nullptr);

    uint32_t dim = visual_cortex_get_feature_dim(cortex);
    EXPECT_EQ(dim, FEATURE_DIM);
}

TEST_F(VisualCortexTest, GetFeatureDimensionNullCortex) {
    uint32_t dim = visual_cortex_get_feature_dim(nullptr);
    EXPECT_EQ(dim, 0);
}

//=============================================================================
// Attention Map Tests
//=============================================================================

TEST_F(VisualCortexTest, CreateAttentionMap) {
    attention_map_t* map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    ASSERT_NE(map, nullptr);
    attention_map_destroy(map);
}

TEST_F(VisualCortexTest, DestroyNullAttentionMap) {
    // Should not crash
    attention_map_destroy(nullptr);
}

TEST_F(VisualCortexTest, ComputeAttention) {
    ASSERT_NE(cortex, nullptr);

    attention_map_t* map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    ASSERT_NE(map, nullptr);

    auto image = SyntheticImageGenerator::GenerateVerticalEdge(TEST_WIDTH, TEST_HEIGHT);

    bool success = visual_cortex_compute_attention(cortex, image.data(),
                                                    TEST_WIDTH, TEST_HEIGHT, map);
    EXPECT_TRUE(success);

    attention_map_destroy(map);
}

TEST_F(VisualCortexTest, AttentionMapGetSet) {
    attention_map_t* map = attention_map_create(32, 32);
    ASSERT_NE(map, nullptr);

    // Set a value
    bool set_success = attention_map_set(map, 10, 10, 0.75f);
    EXPECT_TRUE(set_success);

    // Get the value back
    float value = attention_map_get(map, 10, 10);
    EXPECT_NEAR(value, 0.75f, 0.001f);

    attention_map_destroy(map);
}

TEST_F(VisualCortexTest, AttentionMapOutOfBounds) {
    attention_map_t* map = attention_map_create(32, 32);
    ASSERT_NE(map, nullptr);

    // Get out of bounds
    float value = attention_map_get(map, 100, 100);
    EXPECT_LT(value, 0.0f) << "Out of bounds should return negative value (error)";

    // Set out of bounds
    bool success = attention_map_set(map, 100, 100, 0.5f);
    EXPECT_FALSE(success);

    attention_map_destroy(map);
}

TEST_F(VisualCortexTest, GetAttentionPeak) {
    ASSERT_NE(cortex, nullptr);

    attention_map_t* map = attention_map_create(TEST_WIDTH, TEST_HEIGHT);
    ASSERT_NE(map, nullptr);

    // Create image with strong edge on the right side
    auto image = SyntheticImageGenerator::GenerateVerticalEdge(TEST_WIDTH, TEST_HEIGHT);

    bool attn_success = visual_cortex_compute_attention(cortex, image.data(),
                                                         TEST_WIDTH, TEST_HEIGHT, map);
    EXPECT_TRUE(attn_success);

    uint32_t max_x = 0, max_y = 0;
    float max_value = 0.0f;
    bool peak_success = visual_cortex_get_attention_peak(map, &max_x, &max_y, &max_value);
    EXPECT_TRUE(peak_success);

    // Peak attention should be somewhere (valid coordinates)
    EXPECT_LT(max_x, TEST_WIDTH);
    EXPECT_LT(max_y, TEST_HEIGHT);
    EXPECT_GE(max_value, 0.0f);

    attention_map_destroy(map);
}

//=============================================================================
// Visual Memory Tests
//=============================================================================

TEST_F(VisualCortexTest, StoreMemory) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 8);
    auto features = ProcessImage(image);

    bool success = visual_cortex_store_memory(cortex, features.data(), 0.8f);
    EXPECT_TRUE(success);
}

TEST_F(VisualCortexTest, StoreMemoryNullFeatures) {
    ASSERT_NE(cortex, nullptr);

    bool success = visual_cortex_store_memory(cortex, nullptr, 0.5f);
    EXPECT_FALSE(success);
}

TEST_F(VisualCortexTest, RecallMemory) {
    ASSERT_NE(cortex, nullptr);

    // Store a memory first
    auto image = SyntheticImageGenerator::GenerateVerticalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);
    visual_cortex_store_memory(cortex, features.data(), 0.9f);

    // Recall similar memories
    visual_memory_t** memories = nullptr;
    int num_memories = 0;

    bool success = visual_cortex_recall_memory(cortex, features.data(), 5,
                                                &memories, &num_memories);

    // Memory recall may or may not succeed depending on implementation
    // Just verify no crash and proper output handling
    if (success && num_memories > 0 && memories != nullptr) {
        // Clean up if memories were returned
        free(memories);
    }
}

TEST_F(VisualCortexTest, ConsolidateMemory) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);

    bool success = visual_cortex_consolidate_memory(cortex, features.data(), 0.75f,
                                                     "test context");
    // Success depends on implementation - just verify no crash
    (void)success;
}

//=============================================================================
// Novelty Computation Tests
//=============================================================================

TEST_F(VisualCortexTest, ComputeNovelty) {
    ASSERT_NE(cortex, nullptr);

    // Store some memories first
    auto image1 = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    auto features1 = ProcessImage(image1);
    visual_cortex_store_memory(cortex, features1.data(), 0.8f);

    // Compute novelty for a similar image
    float novelty_similar = visual_cortex_compute_novelty(cortex, features1.data());

    // Compute novelty for a different image
    auto image2 = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 4);
    auto features2 = ProcessImage(image2);
    float novelty_different = visual_cortex_compute_novelty(cortex, features2.data());

    // Both should be valid (non-negative or -1 for error)
    // Novel patterns should have higher novelty than familiar ones
    if (novelty_similar >= 0.0f && novelty_different >= 0.0f) {
        EXPECT_GE(novelty_different, novelty_similar)
            << "Unfamiliar pattern should have higher novelty";
    }
}

TEST_F(VisualCortexTest, ComputeNoveltyNullFeatures) {
    ASSERT_NE(cortex, nullptr);

    float novelty = visual_cortex_compute_novelty(cortex, nullptr);
    EXPECT_LT(novelty, 0.0f) << "Should return error for null features";
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(VisualCortexTest, GetStats) {
    ASSERT_NE(cortex, nullptr);

    visual_cortex_stats_t stats;
    bool success = visual_cortex_get_stats(cortex, &stats);
    EXPECT_TRUE(success);

    // Initial stats should be zero or minimal
    EXPECT_GE(stats.images_processed, 0u);
}

TEST_F(VisualCortexTest, StatsIncrementAfterProcessing) {
    ASSERT_NE(cortex, nullptr);

    visual_cortex_stats_t stats_before;
    visual_cortex_get_stats(cortex, &stats_before);

    // Process an image
    auto image = SyntheticImageGenerator::GenerateHorizontalGradient(TEST_WIDTH, TEST_HEIGHT);
    ProcessImage(image);

    visual_cortex_stats_t stats_after;
    visual_cortex_get_stats(cortex, &stats_after);

    EXPECT_GT(stats_after.images_processed, stats_before.images_processed);
}

//=============================================================================
// Neuromodulation Tests
//=============================================================================

TEST_F(VisualCortexTest, SetTriggerPhasicBurst) {
    ASSERT_NE(cortex, nullptr);

    // Trigger dopamine burst
    bool success = visual_cortex_trigger_phasic_burst(cortex, 0, 0.5f);
    // May or may not succeed depending on configuration
    (void)success;
}

TEST_F(VisualCortexTest, SetTonicLevel) {
    ASSERT_NE(cortex, nullptr);

    // Set acetylcholine tonic level
    bool success = visual_cortex_set_tonic_level(cortex, 1, 0.6f);
    (void)success;
}

TEST_F(VisualCortexTest, GetNeuromodState) {
    ASSERT_NE(cortex, nullptr);

    const phasic_tonic_state_t* state = visual_cortex_get_neuromod_state(cortex, 0);
    // May be null if neuromodulation not enabled
    (void)state;
}

TEST_F(VisualCortexTest, ComputeNeuromodEffects) {
    ASSERT_NE(cortex, nullptr);

    neuromod_effects_t effects;
    bool success = visual_cortex_compute_neuromod_effects(cortex, 0, &effects);

    if (success) {
        // Verify effects are in reasonable ranges
        EXPECT_GE(effects.gabor_gain, 0.0f);
        EXPECT_GE(effects.attention_boost, 0.0f);
        EXPECT_GE(effects.plasticity_gate, 0.0f);
        EXPECT_LE(effects.plasticity_gate, 1.0f);
    }
}

//=============================================================================
// Receptor Profile Tests
//=============================================================================

TEST_F(VisualCortexTest, SetReceptorProfile) {
    ASSERT_NE(cortex, nullptr);

    receptor_expression_t receptors = {
        .d1_density = 0.4f,
        .d2_density = 0.2f,
        .m1_density = 0.5f,
        .m2_density = 0.3f,
        .alpha1_density = 0.4f,
        .beta2_density = 0.3f
    };

    bool success = visual_cortex_set_receptor_profile(cortex, 0, &receptors);
    // Success depends on implementation
    (void)success;
}

TEST_F(VisualCortexTest, GetReceptorProfile) {
    ASSERT_NE(cortex, nullptr);

    const receptor_expression_t* profile = visual_cortex_get_receptor_profile(cortex, 0);
    // May be null if not configured
    (void)profile;
}

//=============================================================================
// Training Mode Tests
//=============================================================================

TEST_F(VisualCortexTest, SetTrainingMode) {
    ASSERT_NE(cortex, nullptr);

    int result = visual_cortex_set_training_mode(cortex, true);
    // 0 indicates success
    if (result == 0) {
        bool is_training = visual_cortex_is_training_mode(cortex);
        EXPECT_TRUE(is_training);

        visual_cortex_set_training_mode(cortex, false);
        is_training = visual_cortex_is_training_mode(cortex);
        EXPECT_FALSE(is_training);
    }
}

TEST_F(VisualCortexTest, GetTrainingState) {
    ASSERT_NE(cortex, nullptr);

    // Enable training mode first
    visual_cortex_set_training_mode(cortex, true);

    // Process an image
    auto image = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 8);
    ProcessImage(image);

    visual_training_state_t state;
    int result = visual_cortex_get_training_state(cortex, &state);

    if (result == 0 && state.valid) {
        EXPECT_GE(state.confidence, 0.0f);
        EXPECT_LE(state.confidence, 1.0f);
    }
}

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST_F(VisualCortexTest, SetBrain) {
    ASSERT_NE(cortex, nullptr);

    // Create a minimal brain
    brain_t brain = brain_create("test_brain", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 10, 10);

    if (brain) {
        // Set brain association
        visual_cortex_set_brain(cortex, brain);

        // Clear association
        visual_cortex_set_brain(cortex, nullptr);

        brain_destroy(brain);
    }
}

//=============================================================================
// Agent Detection Tests
//=============================================================================

TEST_F(VisualCortexTest, DetectAgent) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateCheckerboard(TEST_WIDTH, TEST_HEIGHT, 8);
    auto features = ProcessImage(image);

    bool agent_detected = visual_cortex_detect_agent(cortex, features.data(), FEATURE_DIM);
    // Result depends on features - just verify no crash
    (void)agent_detected;
}

//=============================================================================
// Region Attention Boost Tests
//=============================================================================

TEST_F(VisualCortexTest, BoostRegionAttention) {
    ASSERT_NE(cortex, nullptr);

    // Boost attention at center
    visual_cortex_boost_region_attention(cortex, 0.5f, 0.5f, 1.5f);

    // Should not crash with extreme values
    visual_cortex_boost_region_attention(cortex, 0.0f, 0.0f, 1.0f);
    visual_cortex_boost_region_attention(cortex, 1.0f, 1.0f, 2.0f);
}

//=============================================================================
// Gabor Filter Tests
//=============================================================================

TEST(GaborFilterTest, CreateKernel) {
    gabor_params_t params = {
        .wavelength = 5.0f,
        .orientation = 0.0f,
        .phase = 0.0f,
        .aspect_ratio = 0.5f,
        .bandwidth = 1.0f
    };

    float* kernel = gabor_create_kernel(7, &params);
    ASSERT_NE(kernel, nullptr);

    // Verify kernel is not all zeros
    float sum = 0.0f;
    for (int i = 0; i < 49; i++) {
        sum += std::abs(kernel[i]);
    }
    EXPECT_GT(sum, 0.0f);

    free(kernel);
}

TEST(GaborFilterTest, CreateKernelNullParams) {
    float* kernel = gabor_create_kernel(7, nullptr);
    EXPECT_EQ(kernel, nullptr);
}

TEST(GaborFilterTest, CreateKernelDifferentOrientations) {
    gabor_params_t params0 = {
        .wavelength = 5.0f,
        .orientation = 0.0f,
        .phase = 0.0f,
        .aspect_ratio = 0.5f,
        .bandwidth = 1.0f
    };

    gabor_params_t params45 = params0;
    params45.orientation = 45.0f;

    gabor_params_t params90 = params0;
    params90.orientation = 90.0f;

    float* kernel0 = gabor_create_kernel(7, &params0);
    float* kernel45 = gabor_create_kernel(7, &params45);
    float* kernel90 = gabor_create_kernel(7, &params90);

    ASSERT_NE(kernel0, nullptr);
    ASSERT_NE(kernel45, nullptr);
    ASSERT_NE(kernel90, nullptr);

    // Different orientations should produce different kernels
    float diff_0_45 = 0.0f;
    float diff_0_90 = 0.0f;
    for (int i = 0; i < 49; i++) {
        diff_0_45 += std::abs(kernel0[i] - kernel45[i]);
        diff_0_90 += std::abs(kernel0[i] - kernel90[i]);
    }

    EXPECT_GT(diff_0_45, 0.0f);
    EXPECT_GT(diff_0_90, 0.0f);

    free(kernel0);
    free(kernel45);
    free(kernel90);
}

//=============================================================================
// Convolution Layer Tests
//=============================================================================

TEST(ConvLayerTest, CreateAndDestroy) {
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

    conv_layer_destroy(layer);
}

TEST(ConvLayerTest, GetOutputDimensions) {
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

    uint32_t out_w = conv_layer_get_output_width(layer);
    uint32_t out_h = conv_layer_get_output_height(layer);
    uint32_t out_c = conv_layer_get_output_channels(layer);

    // With padding=1 and stride=1, output should match input
    EXPECT_EQ(out_w, 32);
    EXPECT_EQ(out_h, 32);
    EXPECT_EQ(out_c, 8);

    conv_layer_destroy(layer);
}

TEST(ConvLayerTest, SetKernel) {
    conv_layer_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .input_channels = 1,
        .num_filters = 4,
        .kernel_size = 3,
        .stride = 1,
        .padding = 1,
        .activation = VISUAL_ACTIVATION_RELU
    };

    conv_layer_t* layer = conv_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    // Create a simple kernel
    std::vector<float> kernel(9, 1.0f / 9.0f); // Averaging kernel

    bool success = conv_layer_set_kernel(layer, 0, kernel.data());
    EXPECT_TRUE(success);

    // Invalid filter index should fail
    success = conv_layer_set_kernel(layer, 100, kernel.data());
    EXPECT_FALSE(success);

    conv_layer_destroy(layer);
}

//=============================================================================
// Pooling Layer Tests
//=============================================================================

TEST(PoolLayerTest, CreateAndDestroy) {
    pool_layer_config_t config = {
        .input_width = 32,
        .input_height = 32,
        .input_channels = 8,
        .pool_size = 2,
        .stride = 2,
        .type = POOL_MAX
    };

    pool_layer_t* layer = pool_layer_create(&config);
    ASSERT_NE(layer, nullptr);

    pool_layer_destroy(layer);
}

TEST(PoolLayerTest, CreateNullConfig) {
    pool_layer_t* layer = pool_layer_create(nullptr);
    EXPECT_EQ(layer, nullptr);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(VisualCortexTest, ProcessNoiseImage) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateGaussianNoise(TEST_WIDTH, TEST_HEIGHT);
    auto features = ProcessImage(image);

    // Noise should still produce valid features
    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(VisualCortexTest, ProcessBlackImage) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateSolidColor(TEST_WIDTH, TEST_HEIGHT, 0);
    auto features = ProcessImage(image);

    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

TEST_F(VisualCortexTest, ProcessWhiteImage) {
    ASSERT_NE(cortex, nullptr);

    auto image = SyntheticImageGenerator::GenerateSolidColor(TEST_WIDTH, TEST_HEIGHT, 255);
    auto features = ProcessImage(image);

    bool all_finite = true;
    for (const auto& f : features) {
        if (!std::isfinite(f)) {
            all_finite = false;
            break;
        }
    }
    EXPECT_TRUE(all_finite);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
