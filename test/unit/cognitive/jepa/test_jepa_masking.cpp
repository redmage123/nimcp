/**
 * @file test_jepa_masking.cpp
 * @brief Comprehensive unit tests for NIMCP JEPA Masking Module
 *
 * Tests cover:
 * - Generator lifecycle (create/destroy)
 * - Mask lifecycle (create/destroy/clone)
 * - All masking strategies (RANDOM, BLOCK, ATTENTION_GUIDED, CURRICULUM, TUBE, CAUSAL)
 * - Mask operations (apply, get indices, invert, compute stats)
 * - Curriculum learning API
 * - Edge cases and NULL safety
 */

#include "test_helpers.h"

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "utils/error/nimcp_error_codes.h"

#include <cstring>
#include <cmath>
#include <vector>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class JepaMaskingTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Create generator with default RANDOM strategy
        generator_ = jepa_mask_generator_create(NULL);
    }

    void TearDown() override
    {
        if (generator_) {
            jepa_mask_generator_destroy(generator_);
            generator_ = nullptr;
        }
    }

    // Helper to create generator with specific strategy
    jepa_mask_generator_t* create_generator_with_strategy(jepa_mask_strategy_t strategy)
    {
        jepa_mask_config_t config;
        jepa_mask_default_config(&config, strategy);
        return jepa_mask_generator_create(&config);
    }

    // Helper to verify mask dimensions
    void verify_mask_dimensions(const jepa_mask_t* mask,
                                uint32_t expected_width,
                                uint32_t expected_height,
                                uint32_t expected_temporal)
    {
        ASSERT_NE(mask, nullptr);
        EXPECT_EQ(mask->width, expected_width);
        EXPECT_EQ(mask->height, expected_height);
        EXPECT_EQ(mask->temporal, expected_temporal);
        EXPECT_EQ(mask->total_size, expected_width * expected_height * expected_temporal);
    }

    // Helper to verify mask ratio is approximately correct
    void verify_mask_ratio(const jepa_mask_t* mask, float expected_ratio, float tolerance = 0.15f)
    {
        ASSERT_NE(mask, nullptr);
        ASSERT_NE(mask->data, nullptr);
        ASSERT_GT(mask->total_size, 0u);

        // Count masked elements
        uint32_t masked_count = 0;
        for (uint32_t i = 0; i < mask->total_size; i++) {
            if (mask->data[i] > 0.5f) {
                masked_count++;
            }
        }

        float actual_ratio = static_cast<float>(masked_count) / mask->total_size;
        EXPECT_NEAR(actual_ratio, expected_ratio, tolerance)
            << "Expected ratio " << expected_ratio << " but got " << actual_ratio;
    }

    // Helper to verify all mask values are in [0, 1]
    void verify_mask_values_in_range(const jepa_mask_t* mask)
    {
        ASSERT_NE(mask, nullptr);
        ASSERT_NE(mask->data, nullptr);

        for (uint32_t i = 0; i < mask->total_size; i++) {
            EXPECT_GE(mask->data[i], 0.0f) << "Mask value at " << i << " is negative";
            EXPECT_LE(mask->data[i], 1.0f) << "Mask value at " << i << " exceeds 1.0";
        }
    }

    jepa_mask_generator_t* generator_;
};

//=============================================================================
// Generator Creation/Destruction Tests
//=============================================================================

TEST_F(JepaMaskingTest, GeneratorCreateWithNullConfig)
{
    // NULL config should create with defaults (RANDOM strategy)
    EXPECT_NE(generator_, nullptr);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithRandomStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_RANDOM);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_RANDOM);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithBlockStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_BLOCK);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_BLOCK);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithAttentionStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_ATTENTION_GUIDED);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_ATTENTION_GUIDED);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithCurriculumStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_CURRICULUM);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithTubeStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_TUBE);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_TUBE);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorCreateWithCausalStrategy)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CAUSAL);
    ASSERT_NE(gen, nullptr);
    EXPECT_EQ(gen->config.strategy, JEPA_MASK_CAUSAL);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GeneratorDestroyNullSafe)
{
    // Should not crash
    jepa_mask_generator_destroy(nullptr);
}

TEST_F(JepaMaskingTest, GeneratorReset)
{
    ASSERT_NE(generator_, nullptr);

    int result = jepa_mask_generator_reset(generator_, 12345);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, GeneratorResetNullSafe)
{
    int result = jepa_mask_generator_reset(nullptr, 12345);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, GeneratorResetKeepSeed)
{
    ASSERT_NE(generator_, nullptr);

    // First reset with a seed
    int result1 = jepa_mask_generator_reset(generator_, 42);
    EXPECT_EQ(result1, NIMCP_SUCCESS);

    // Reset with 0 should keep current seed
    int result2 = jepa_mask_generator_reset(generator_, 0);
    EXPECT_EQ(result2, NIMCP_SUCCESS);
}

//=============================================================================
// Mask Creation/Destruction Tests
//=============================================================================

TEST_F(JepaMaskingTest, MaskCreate2D)
{
    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);
    verify_mask_dimensions(mask, 16, 16, 1);
    EXPECT_NE(mask->data, nullptr);
    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskCreate3D)
{
    jepa_mask_t* mask = jepa_mask_create(8, 8, 4);
    ASSERT_NE(mask, nullptr);
    verify_mask_dimensions(mask, 8, 8, 4);
    EXPECT_NE(mask->data, nullptr);
    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskCreate1D)
{
    // 1D mask: width > 1, height = 1, temporal = 1
    jepa_mask_t* mask = jepa_mask_create(64, 1, 1);
    ASSERT_NE(mask, nullptr);
    verify_mask_dimensions(mask, 64, 1, 1);
    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskCreateZeroWidth)
{
    jepa_mask_t* mask = jepa_mask_create(0, 16, 1);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskCreateZeroHeight)
{
    jepa_mask_t* mask = jepa_mask_create(16, 0, 1);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskCreateZeroTemporal)
{
    jepa_mask_t* mask = jepa_mask_create(16, 16, 0);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskCreateExceedsMaxWidth)
{
    jepa_mask_t* mask = jepa_mask_create(JEPA_MASK_MAX_WIDTH + 1, 16, 1);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskCreateExceedsMaxHeight)
{
    jepa_mask_t* mask = jepa_mask_create(16, JEPA_MASK_MAX_HEIGHT + 1, 1);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskCreateExceedsMaxTemporal)
{
    jepa_mask_t* mask = jepa_mask_create(16, 16, JEPA_MASK_MAX_TEMPORAL + 1);
    EXPECT_EQ(mask, nullptr);
}

TEST_F(JepaMaskingTest, MaskDestroyNullSafe)
{
    // Should not crash
    jepa_mask_destroy(nullptr);
}

TEST_F(JepaMaskingTest, MaskClone)
{
    jepa_mask_t* original = jepa_mask_create(8, 8, 1);
    ASSERT_NE(original, nullptr);

    // Set some values
    for (uint32_t i = 0; i < original->total_size; i++) {
        original->data[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }
    original->mask_ratio = 0.5f;
    original->num_masked = original->total_size / 2;
    original->num_visible = original->total_size / 2;

    jepa_mask_t* clone = jepa_mask_clone(original);
    ASSERT_NE(clone, nullptr);

    // Verify dimensions match
    verify_mask_dimensions(clone, original->width, original->height, original->temporal);

    // Verify data matches
    for (uint32_t i = 0; i < original->total_size; i++) {
        EXPECT_FLOAT_EQ(clone->data[i], original->data[i]);
    }

    // Verify stats match
    EXPECT_FLOAT_EQ(clone->mask_ratio, original->mask_ratio);
    EXPECT_EQ(clone->num_masked, original->num_masked);
    EXPECT_EQ(clone->num_visible, original->num_visible);

    // Verify they're independent
    EXPECT_NE(clone->data, original->data);

    jepa_mask_destroy(original);
    jepa_mask_destroy(clone);
}

TEST_F(JepaMaskingTest, MaskCloneNullSafe)
{
    jepa_mask_t* clone = jepa_mask_clone(nullptr);
    EXPECT_EQ(clone, nullptr);
}

//=============================================================================
// Mask Generation Tests - RANDOM Strategy
//=============================================================================

TEST_F(JepaMaskingTest, Generate2DRandomMask)
{
    ASSERT_NE(generator_, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(generator_, 16, 16, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);
    verify_mask_ratio(mask, JEPA_MASK_DEFAULT_RATIO);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, Generate2DRandomMaskMultipleCalls)
{
    ASSERT_NE(generator_, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    // Generate multiple masks and verify they're different (statistical test)
    std::vector<float> first_mask(mask->total_size);

    int result1 = jepa_mask_generate_2d(generator_, 16, 16, mask);
    EXPECT_EQ(result1, NIMCP_SUCCESS);
    std::copy(mask->data, mask->data + mask->total_size, first_mask.begin());

    int result2 = jepa_mask_generate_2d(generator_, 16, 16, mask);
    EXPECT_EQ(result2, NIMCP_SUCCESS);

    // Check that masks are different (with very high probability)
    bool different = false;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        if (std::abs(mask->data[i] - first_mask[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different) << "Two consecutive random masks should be different";

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, Generate2DNullGenerator)
{
    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(nullptr, 16, 16, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, Generate2DNullMask)
{
    ASSERT_NE(generator_, nullptr);

    int result = jepa_mask_generate_2d(generator_, 16, 16, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, Generate2DZeroDimensions)
{
    ASSERT_NE(generator_, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result1 = jepa_mask_generate_2d(generator_, 0, 16, mask);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = jepa_mask_generate_2d(generator_, 16, 0, mask);
    EXPECT_NE(result2, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

//=============================================================================
// Mask Generation Tests - BLOCK Strategy
//=============================================================================

TEST_F(JepaMaskingTest, Generate2DBlockMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_BLOCK);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 16, 16, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    // Block masks should have contiguous regions
    // We don't verify the exact ratio as blocks may not precisely match target

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, Generate2DBlockMaskWithConfig)
{
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_BLOCK);
    config.target_ratio = 0.5f;
    config.params.block.num_blocks = 4;
    config.params.block.shape = JEPA_MASK_SHAPE_SQUARE;

    jepa_mask_generator_t* gen = jepa_mask_generator_create(&config);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(32, 32, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 32, 32, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

//=============================================================================
// Mask Generation Tests - ATTENTION_GUIDED Strategy
//=============================================================================

TEST_F(JepaMaskingTest, GenerateAttentionGuidedMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_ATTENTION_GUIDED);
    ASSERT_NE(gen, nullptr);

    const uint32_t width = 8;
    const uint32_t height = 8;

    // Create attention scores (higher in center)
    std::vector<float> attention(width * height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            float cx = static_cast<float>(x) - width / 2.0f;
            float cy = static_cast<float>(y) - height / 2.0f;
            float dist = std::sqrt(cx * cx + cy * cy);
            attention[y * width + x] = std::exp(-dist * 0.1f);
        }
    }

    jepa_mask_t* mask = jepa_mask_create(width, height, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_attention(gen, attention.data(), width, height, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GenerateAttentionGuidedMaskNullAttention)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_ATTENTION_GUIDED);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(8, 8, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_attention(gen, nullptr, 8, 8, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, GenerateAttentionGuidedMaskNullGenerator)
{
    std::vector<float> attention(64, 0.5f);
    jepa_mask_t* mask = jepa_mask_create(8, 8, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_attention(nullptr, attention.data(), 8, 8, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

//=============================================================================
// Mask Generation Tests - CURRICULUM Strategy
//=============================================================================

TEST_F(JepaMaskingTest, GenerateCurriculumMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 16, 16, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    // Initial curriculum should use start ratio
    verify_mask_ratio(mask, JEPA_MASK_CURRICULUM_START_RATIO);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, CurriculumStep)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);

    float initial_ratio = jepa_mask_curriculum_get_ratio(gen);
    EXPECT_NEAR(initial_ratio, JEPA_MASK_CURRICULUM_START_RATIO, 0.01f);

    // Advance curriculum
    for (int i = 0; i < 50; i++) {
        int result = jepa_mask_curriculum_step(gen);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    float advanced_ratio = jepa_mask_curriculum_get_ratio(gen);
    EXPECT_GT(advanced_ratio, initial_ratio)
        << "Curriculum ratio should increase over steps";

    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, CurriculumStepNull)
{
    int result = jepa_mask_curriculum_step(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, CurriculumSetStep)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);

    int result = jepa_mask_curriculum_set_step(gen, 50);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float ratio = jepa_mask_curriculum_get_ratio(gen);
    // At step 50 of 100, ratio should be approximately halfway
    float expected = JEPA_MASK_CURRICULUM_START_RATIO +
                     0.5f * (JEPA_MASK_CURRICULUM_END_RATIO - JEPA_MASK_CURRICULUM_START_RATIO);
    EXPECT_NEAR(ratio, expected, 0.1f);

    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, CurriculumSetStepNull)
{
    int result = jepa_mask_curriculum_set_step(nullptr, 50);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, CurriculumGetRatioNull)
{
    float ratio = jepa_mask_curriculum_get_ratio(nullptr);
    EXPECT_FLOAT_EQ(ratio, 0.0f);
}

TEST_F(JepaMaskingTest, CurriculumProgressToEnd)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);

    // Set to end of curriculum
    int result = jepa_mask_curriculum_set_step(gen, JEPA_MASK_CURRICULUM_EPOCHS);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float final_ratio = jepa_mask_curriculum_get_ratio(gen);
    EXPECT_NEAR(final_ratio, JEPA_MASK_CURRICULUM_END_RATIO, 0.01f);

    jepa_mask_generator_destroy(gen);
}

//=============================================================================
// Mask Generation Tests - TUBE Strategy (3D)
//=============================================================================

TEST_F(JepaMaskingTest, Generate3DTubeMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_TUBE);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(8, 8, 8);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_3d(gen, 8, 8, 8, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, Generate3DNullGenerator)
{
    jepa_mask_t* mask = jepa_mask_create(8, 8, 4);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_3d(nullptr, 8, 8, 4, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, Generate3DNullMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_TUBE);
    ASSERT_NE(gen, nullptr);

    int result = jepa_mask_generate_3d(gen, 8, 8, 4, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_generator_destroy(gen);
}

//=============================================================================
// Mask Generation Tests - CAUSAL Strategy (1D)
//=============================================================================

TEST_F(JepaMaskingTest, Generate1DCausalMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CAUSAL);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(32, 1, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_1d(gen, 32, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    // Causal masking should mask future positions
    // For causal masking, later positions should generally be more masked

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, Generate1DNullGenerator)
{
    jepa_mask_t* mask = jepa_mask_create(32, 1, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_1d(nullptr, 32, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, Generate1DNullMask)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CAUSAL);
    ASSERT_NE(gen, nullptr);

    int result = jepa_mask_generate_1d(gen, 32, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, Generate1DZeroLength)
{
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CAUSAL);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(32, 1, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_1d(gen, 0, mask);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

//=============================================================================
// Mask Operations Tests
//=============================================================================

TEST_F(JepaMaskingTest, MaskApply)
{
    ASSERT_NE(generator_, nullptr);

    const uint32_t size = 16;
    const uint32_t dim = 4;

    jepa_mask_t* mask = jepa_mask_create(size, 1, 1);
    ASSERT_NE(mask, nullptr);

    // Generate a mask
    int gen_result = jepa_mask_generate_2d(generator_, size, 1, mask);
    EXPECT_EQ(gen_result, NIMCP_SUCCESS);

    // Create input and output arrays
    std::vector<float> input(size * dim, 1.0f);
    std::vector<float> output(size * dim, 999.0f);

    int result = jepa_mask_apply(mask, input.data(), output.data(), dim);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify: masked positions should be zeroed
    for (uint32_t i = 0; i < size; i++) {
        float expected_multiplier = 1.0f - mask->data[i];
        for (uint32_t d = 0; d < dim; d++) {
            float expected = input[i * dim + d] * expected_multiplier;
            EXPECT_NEAR(output[i * dim + d], expected, 0.001f);
        }
    }

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskApplyNullMask)
{
    std::vector<float> input(64, 1.0f);
    std::vector<float> output(64);

    int result = jepa_mask_apply(nullptr, input.data(), output.data(), 4);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, MaskApplyNullInput)
{
    jepa_mask_t* mask = jepa_mask_create(16, 1, 1);
    ASSERT_NE(mask, nullptr);

    std::vector<float> output(64);

    int result = jepa_mask_apply(mask, nullptr, output.data(), 4);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskApplyNullOutput)
{
    jepa_mask_t* mask = jepa_mask_create(16, 1, 1);
    ASSERT_NE(mask, nullptr);

    std::vector<float> input(64, 1.0f);

    int result = jepa_mask_apply(mask, input.data(), nullptr, 4);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskGetVisibleIndices)
{
    jepa_mask_t* mask = jepa_mask_create(8, 1, 1);
    ASSERT_NE(mask, nullptr);

    // Set specific mask pattern: mask even indices
    for (uint32_t i = 0; i < mask->total_size; i++) {
        mask->data[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    std::vector<uint32_t> indices(mask->total_size);
    uint32_t num_indices = 0;

    int result = jepa_mask_get_visible_indices(mask, indices.data(), &num_indices);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Half should be visible (odd indices)
    EXPECT_EQ(num_indices, mask->total_size / 2);

    // Verify visible indices are odd
    for (uint32_t i = 0; i < num_indices; i++) {
        EXPECT_EQ(indices[i] % 2, 1u);
    }

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskGetVisibleIndicesNullMask)
{
    std::vector<uint32_t> indices(64);
    uint32_t num_indices = 0;

    int result = jepa_mask_get_visible_indices(nullptr, indices.data(), &num_indices);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, MaskGetMaskedIndices)
{
    jepa_mask_t* mask = jepa_mask_create(8, 1, 1);
    ASSERT_NE(mask, nullptr);

    // Set specific mask pattern: mask even indices
    for (uint32_t i = 0; i < mask->total_size; i++) {
        mask->data[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    std::vector<uint32_t> indices(mask->total_size);
    uint32_t num_indices = 0;

    int result = jepa_mask_get_masked_indices(mask, indices.data(), &num_indices);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Half should be masked (even indices)
    EXPECT_EQ(num_indices, mask->total_size / 2);

    // Verify masked indices are even
    for (uint32_t i = 0; i < num_indices; i++) {
        EXPECT_EQ(indices[i] % 2, 0u);
    }

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskGetMaskedIndicesNullMask)
{
    std::vector<uint32_t> indices(64);
    uint32_t num_indices = 0;

    int result = jepa_mask_get_masked_indices(nullptr, indices.data(), &num_indices);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, MaskInvert)
{
    jepa_mask_t* mask = jepa_mask_create(4, 4, 1);
    ASSERT_NE(mask, nullptr);

    // Set pattern
    for (uint32_t i = 0; i < mask->total_size; i++) {
        mask->data[i] = (i < mask->total_size / 2) ? 1.0f : 0.0f;
    }

    // Store original values
    std::vector<float> original(mask->data, mask->data + mask->total_size);

    int result = jepa_mask_invert(mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify inversion
    for (uint32_t i = 0; i < mask->total_size; i++) {
        EXPECT_FLOAT_EQ(mask->data[i], 1.0f - original[i]);
    }

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskInvertNullSafe)
{
    int result = jepa_mask_invert(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaMaskingTest, MaskInvertTwiceRestoresOriginal)
{
    jepa_mask_t* mask = jepa_mask_create(4, 4, 1);
    ASSERT_NE(mask, nullptr);

    // Set pattern
    for (uint32_t i = 0; i < mask->total_size; i++) {
        mask->data[i] = static_cast<float>(i) / mask->total_size;
    }

    // Store original
    std::vector<float> original(mask->data, mask->data + mask->total_size);

    // Invert twice
    jepa_mask_invert(mask);
    jepa_mask_invert(mask);

    // Should be back to original
    for (uint32_t i = 0; i < mask->total_size; i++) {
        EXPECT_NEAR(mask->data[i], original[i], 0.0001f);
    }

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskComputeStats)
{
    jepa_mask_t* mask = jepa_mask_create(10, 10, 1);
    ASSERT_NE(mask, nullptr);

    // Set exactly 30% masked
    uint32_t num_to_mask = 30;
    for (uint32_t i = 0; i < mask->total_size; i++) {
        mask->data[i] = (i < num_to_mask) ? 1.0f : 0.0f;
    }

    int result = jepa_mask_compute_stats(mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(mask->num_masked, 30u);
    EXPECT_EQ(mask->num_visible, 70u);
    EXPECT_NEAR(mask->mask_ratio, 0.3f, 0.01f);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, MaskComputeStatsNullSafe)
{
    int result = jepa_mask_compute_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Configuration API Tests
//=============================================================================

TEST_F(JepaMaskingTest, DefaultConfigRandom)
{
    jepa_mask_config_t config;
    int result = jepa_mask_default_config(&config, JEPA_MASK_RANDOM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(config.strategy, JEPA_MASK_RANDOM);
    EXPECT_NEAR(config.target_ratio, JEPA_MASK_DEFAULT_RATIO, 0.01f);
}

TEST_F(JepaMaskingTest, DefaultConfigBlock)
{
    jepa_mask_config_t config;
    int result = jepa_mask_default_config(&config, JEPA_MASK_BLOCK);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(config.strategy, JEPA_MASK_BLOCK);
    EXPECT_GT(config.params.block.num_blocks, 0u);
}

TEST_F(JepaMaskingTest, DefaultConfigCurriculum)
{
    jepa_mask_config_t config;
    int result = jepa_mask_default_config(&config, JEPA_MASK_CURRICULUM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(config.strategy, JEPA_MASK_CURRICULUM);
    EXPECT_NEAR(config.params.curriculum.start_ratio,
                JEPA_MASK_CURRICULUM_START_RATIO, 0.01f);
    EXPECT_NEAR(config.params.curriculum.end_ratio,
                JEPA_MASK_CURRICULUM_END_RATIO, 0.01f);
}

TEST_F(JepaMaskingTest, DefaultConfigNull)
{
    int result = jepa_mask_default_config(nullptr, JEPA_MASK_RANDOM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// String Conversion Tests
//=============================================================================

TEST_F(JepaMaskingTest, StrategyToString)
{
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_RANDOM), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_BLOCK), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_BLOCK_MULTI), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_ATTENTION_GUIDED), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_CURRICULUM), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_TUBE), nullptr);
    EXPECT_NE(jepa_mask_strategy_to_string(JEPA_MASK_CAUSAL), nullptr);

    // Verify unique strings
    EXPECT_STRNE(jepa_mask_strategy_to_string(JEPA_MASK_RANDOM),
                 jepa_mask_strategy_to_string(JEPA_MASK_BLOCK));
}

TEST_F(JepaMaskingTest, ShapeToString)
{
    EXPECT_NE(jepa_mask_shape_to_string(JEPA_MASK_SHAPE_RECT), nullptr);
    EXPECT_NE(jepa_mask_shape_to_string(JEPA_MASK_SHAPE_SQUARE), nullptr);
    EXPECT_NE(jepa_mask_shape_to_string(JEPA_MASK_SHAPE_ELLIPSE), nullptr);
    EXPECT_NE(jepa_mask_shape_to_string(JEPA_MASK_SHAPE_IRREGULAR), nullptr);
}

TEST_F(JepaMaskingTest, ModeToString)
{
    EXPECT_NE(jepa_mask_mode_to_string(JEPA_MASK_MODE_PATCH), nullptr);
    EXPECT_NE(jepa_mask_mode_to_string(JEPA_MASK_MODE_PIXEL), nullptr);
    EXPECT_NE(jepa_mask_mode_to_string(JEPA_MASK_MODE_FEATURE), nullptr);
}

//=============================================================================
// Reproducibility Tests (Fixed Seed)
//=============================================================================

TEST_F(JepaMaskingTest, FixedSeedReproducibility)
{
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_RANDOM);
    config.seed = 42;
    config.use_fixed_seed = true;

    jepa_mask_generator_t* gen1 = jepa_mask_generator_create(&config);
    jepa_mask_generator_t* gen2 = jepa_mask_generator_create(&config);
    ASSERT_NE(gen1, nullptr);
    ASSERT_NE(gen2, nullptr);

    jepa_mask_t* mask1 = jepa_mask_create(8, 8, 1);
    jepa_mask_t* mask2 = jepa_mask_create(8, 8, 1);
    ASSERT_NE(mask1, nullptr);
    ASSERT_NE(mask2, nullptr);

    jepa_mask_generate_2d(gen1, 8, 8, mask1);
    jepa_mask_generate_2d(gen2, 8, 8, mask2);

    // With same fixed seed, masks should be identical
    for (uint32_t i = 0; i < mask1->total_size; i++) {
        EXPECT_FLOAT_EQ(mask1->data[i], mask2->data[i])
            << "Mismatch at index " << i;
    }

    jepa_mask_destroy(mask1);
    jepa_mask_destroy(mask2);
    jepa_mask_generator_destroy(gen1);
    jepa_mask_generator_destroy(gen2);
}

TEST_F(JepaMaskingTest, DifferentSeedsDifferentMasks)
{
    jepa_mask_config_t config1, config2;
    jepa_mask_default_config(&config1, JEPA_MASK_RANDOM);
    jepa_mask_default_config(&config2, JEPA_MASK_RANDOM);
    config1.seed = 42;
    config1.use_fixed_seed = true;
    config2.seed = 123;
    config2.use_fixed_seed = true;

    jepa_mask_generator_t* gen1 = jepa_mask_generator_create(&config1);
    jepa_mask_generator_t* gen2 = jepa_mask_generator_create(&config2);
    ASSERT_NE(gen1, nullptr);
    ASSERT_NE(gen2, nullptr);

    jepa_mask_t* mask1 = jepa_mask_create(8, 8, 1);
    jepa_mask_t* mask2 = jepa_mask_create(8, 8, 1);
    ASSERT_NE(mask1, nullptr);
    ASSERT_NE(mask2, nullptr);

    jepa_mask_generate_2d(gen1, 8, 8, mask1);
    jepa_mask_generate_2d(gen2, 8, 8, mask2);

    // Different seeds should produce different masks
    bool different = false;
    for (uint32_t i = 0; i < mask1->total_size; i++) {
        if (std::abs(mask1->data[i] - mask2->data[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different) << "Different seeds should produce different masks";

    jepa_mask_destroy(mask1);
    jepa_mask_destroy(mask2);
    jepa_mask_generator_destroy(gen1);
    jepa_mask_generator_destroy(gen2);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(JepaMaskingTest, SingleElementMask)
{
    jepa_mask_t* mask = jepa_mask_create(1, 1, 1);
    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->total_size, 1u);

    ASSERT_NE(generator_, nullptr);
    int result = jepa_mask_generate_2d(generator_, 1, 1, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, LargeMask)
{
    // Use max allowed dimensions
    jepa_mask_t* mask = jepa_mask_create(
        JEPA_MASK_MAX_WIDTH, JEPA_MASK_MAX_HEIGHT, 1);
    ASSERT_NE(mask, nullptr);

    ASSERT_NE(generator_, nullptr);
    int result = jepa_mask_generate_2d(generator_,
                                       JEPA_MASK_MAX_WIDTH,
                                       JEPA_MASK_MAX_HEIGHT,
                                       mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_values_in_range(mask);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, NonSquareMask)
{
    jepa_mask_t* mask = jepa_mask_create(32, 8, 1);
    ASSERT_NE(mask, nullptr);
    verify_mask_dimensions(mask, 32, 8, 1);

    ASSERT_NE(generator_, nullptr);
    int result = jepa_mask_generate_2d(generator_, 32, 8, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_mask_destroy(mask);
}

TEST_F(JepaMaskingTest, CustomTargetRatio)
{
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_RANDOM);
    config.target_ratio = 0.5f;

    jepa_mask_generator_t* gen = jepa_mask_generator_create(&config);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(32, 32, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 32, 32, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    verify_mask_ratio(mask, 0.5f);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, ZeroTargetRatio)
{
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_RANDOM);
    config.target_ratio = 0.0f;

    jepa_mask_generator_t* gen = jepa_mask_generator_create(&config);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 16, 16, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // With 0 target ratio, nothing should be masked
    verify_mask_ratio(mask, 0.0f, 0.05f);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, FullTargetRatio)
{
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_RANDOM);
    config.target_ratio = 1.0f;

    jepa_mask_generator_t* gen = jepa_mask_generator_create(&config);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    int result = jepa_mask_generate_2d(gen, 16, 16, mask);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // With 1.0 target ratio, everything should be masked
    verify_mask_ratio(mask, 1.0f, 0.05f);

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(JepaMaskingTest, IntegrationFullWorkflow)
{
    // 1. Create generator with custom config
    jepa_mask_config_t config;
    jepa_mask_default_config(&config, JEPA_MASK_BLOCK);
    config.target_ratio = 0.6f;
    config.seed = 12345;
    config.use_fixed_seed = true;

    jepa_mask_generator_t* gen = jepa_mask_generator_create(&config);
    ASSERT_NE(gen, nullptr);

    // 2. Create mask
    const uint32_t width = 14;
    const uint32_t height = 14;
    jepa_mask_t* mask = jepa_mask_create(width, height, 1);
    ASSERT_NE(mask, nullptr);

    // 3. Generate mask
    int gen_result = jepa_mask_generate_2d(gen, width, height, mask);
    EXPECT_EQ(gen_result, NIMCP_SUCCESS);

    // 4. Compute and verify stats
    int stats_result = jepa_mask_compute_stats(mask);
    EXPECT_EQ(stats_result, NIMCP_SUCCESS);
    EXPECT_EQ(mask->num_masked + mask->num_visible, mask->total_size);

    // 5. Get masked/visible indices
    std::vector<uint32_t> masked_indices(mask->total_size);
    std::vector<uint32_t> visible_indices(mask->total_size);
    uint32_t num_masked, num_visible;

    jepa_mask_get_masked_indices(mask, masked_indices.data(), &num_masked);
    jepa_mask_get_visible_indices(mask, visible_indices.data(), &num_visible);

    EXPECT_EQ(num_masked, mask->num_masked);
    EXPECT_EQ(num_visible, mask->num_visible);

    // 6. Apply mask to embeddings
    const uint32_t dim = 64;
    std::vector<float> embeddings(width * height * dim, 1.0f);
    std::vector<float> masked_embeddings(width * height * dim);

    int apply_result = jepa_mask_apply(mask, embeddings.data(),
                                       masked_embeddings.data(), dim);
    EXPECT_EQ(apply_result, NIMCP_SUCCESS);

    // 7. Cleanup
    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, IntegrationCurriculumTraining)
{
    // Simulate curriculum learning progression
    jepa_mask_generator_t* gen = create_generator_with_strategy(JEPA_MASK_CURRICULUM);
    ASSERT_NE(gen, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    std::vector<float> ratios;

    // Simulate 100 training steps
    for (int step = 0; step < 100; step++) {
        jepa_mask_generate_2d(gen, 16, 16, mask);
        jepa_mask_compute_stats(mask);
        ratios.push_back(mask->mask_ratio);

        jepa_mask_curriculum_step(gen);
    }

    // Verify curriculum progression: ratios should generally increase
    float first_quarter_avg = 0;
    float last_quarter_avg = 0;
    for (int i = 0; i < 25; i++) {
        first_quarter_avg += ratios[i];
        last_quarter_avg += ratios[75 + i];
    }
    first_quarter_avg /= 25;
    last_quarter_avg /= 25;

    EXPECT_LT(first_quarter_avg, last_quarter_avg)
        << "Curriculum should increase mask ratio over time";

    jepa_mask_destroy(mask);
    jepa_mask_generator_destroy(gen);
}

TEST_F(JepaMaskingTest, IntegrationMaskInversionSymmetry)
{
    ASSERT_NE(generator_, nullptr);

    jepa_mask_t* mask = jepa_mask_create(16, 16, 1);
    ASSERT_NE(mask, nullptr);

    jepa_mask_generate_2d(generator_, 16, 16, mask);
    jepa_mask_compute_stats(mask);

    uint32_t original_masked = mask->num_masked;
    uint32_t original_visible = mask->num_visible;

    // Invert and verify stats swap
    jepa_mask_invert(mask);
    jepa_mask_compute_stats(mask);

    EXPECT_EQ(mask->num_masked, original_visible);
    EXPECT_EQ(mask->num_visible, original_masked);

    jepa_mask_destroy(mask);
}

}  // anonymous namespace
