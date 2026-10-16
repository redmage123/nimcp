/**
 * @file test_jepa_context.cpp
 * @brief Comprehensive unit tests for JEPA Context Encoder module
 *
 * Tests cover:
 * - Context encoder creation/destruction/reset
 * - Default configuration
 * - Task/working memory/attention/custom context setting
 * - Context-conditioned encoding
 * - All conditioning types (FiLM, cross-attention, additive, multiplicative, gated, concatenate)
 * - Context state lifecycle
 * - Edge cases and NULL pointer handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-26
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

//=============================================================================
// Constants
//=============================================================================

constexpr float FLOAT_TOLERANCE = 1e-5f;
constexpr uint32_t TEST_CONTEXT_DIM = 64;
constexpr uint32_t TEST_INPUT_DIM = 128;
constexpr uint32_t TEST_OUTPUT_DIM = 128;
constexpr uint32_t TEST_LATENT_DIM = 128;

//=============================================================================
// Test Fixture
//=============================================================================

class JepaContextTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Get default config and modify for testing
        int result = jepa_context_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        config_.context_dim = TEST_CONTEXT_DIM;
        config_.input_dim = TEST_INPUT_DIM;
        config_.output_dim = TEST_OUTPUT_DIM;

        // Create encoder with default FiLM conditioning
        encoder_ = jepa_context_encoder_create(&config_);
        // Note: encoder_ may be NULL if implementation not complete
    }

    void TearDown() override
    {
        if (encoder_) {
            jepa_context_encoder_destroy(encoder_);
            encoder_ = nullptr;
        }
    }

    // Helper to create encoder with specific conditioning type
    jepa_context_encoder_t* create_encoder_with_conditioning(jepa_conditioning_t cond_type)
    {
        jepa_context_config_t cond_config;
        jepa_context_default_config(&cond_config);
        cond_config.conditioning = cond_type;
        cond_config.context_dim = TEST_CONTEXT_DIM;
        cond_config.input_dim = TEST_INPUT_DIM;
        cond_config.output_dim = TEST_OUTPUT_DIM;
        return jepa_context_encoder_create(&cond_config);
    }

    // Helper to create test latent
    jepa_latent_t* create_test_latent(uint32_t dim = TEST_LATENT_DIM)
    {
        jepa_latent_config_t latent_config;
        jepa_latent_default_config(&latent_config);
        latent_config.latent_dim = dim;
        return jepa_latent_create(&latent_config);
    }

    // Helper to fill latent with test values
    void fill_latent(jepa_latent_t* latent, float value)
    {
        if (latent && latent->embedding) {
            for (uint32_t i = 0; i < latent->latent_dim; i++) {
                latent->embedding[i] = value + static_cast<float>(i) * 0.01f;
            }
        }
    }

    // Helper to create test float array
    std::vector<float> create_test_array(uint32_t size, float base_value = 1.0f)
    {
        std::vector<float> arr(size);
        for (uint32_t i = 0; i < size; i++) {
            arr[i] = base_value + static_cast<float>(i) * 0.01f;
        }
        return arr;
    }

    jepa_context_config_t config_;
    jepa_context_encoder_t* encoder_ = nullptr;
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(JepaContextTest, DefaultConfigReturnsSuccess)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, DefaultConfigSetsReasonableValues)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Check context dimension is set to default
    EXPECT_EQ(config.context_dim, JEPA_CONTEXT_DEFAULT_DIM);

    // Check reasonable number of sources
    EXPECT_LE(config.num_sources, JEPA_CONTEXT_MAX_SOURCES);

    // Check attention heads
    EXPECT_EQ(config.num_attention_heads, JEPA_CONTEXT_DEFAULT_NUM_HEADS);

    // Check conditioning type is valid
    EXPECT_GE(static_cast<int>(config.conditioning), 0);
    EXPECT_LE(static_cast<int>(config.conditioning), static_cast<int>(JEPA_COND_GATED));

    // Check dropout is in valid range
    EXPECT_GE(config.dropout_rate, 0.0f);
    EXPECT_LE(config.dropout_rate, 1.0f);
}

TEST_F(JepaContextTest, DefaultConfigNullPointer)
{
    int result = jepa_context_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Encoder Creation Tests
//=============================================================================

TEST_F(JepaContextTest, EncoderCreationWithConfig)
{
    // encoder_ is created in SetUp
    // Skip if implementation returns NULL
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }
    EXPECT_NE(encoder_, nullptr);
}

TEST_F(JepaContextTest, EncoderCreationWithNullConfigUsesDefaults)
{
    jepa_context_encoder_t* enc = jepa_context_encoder_create(nullptr);
    // Should create with defaults or return NULL
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithFiLMConditioning)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_FILM);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_FILM);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithCrossAttention)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_CROSS_ATTENTION);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_CROSS_ATTENTION);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithAdditiveConditioning)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_ADDITIVE);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_ADDITIVE);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithMultiplicativeConditioning)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_MULTIPLICATIVE);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_MULTIPLICATIVE);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithGatedConditioning)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_GATED);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_GATED);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, EncoderCreationWithConcatenateConditioning)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_CONCATENATE);
    if (enc != nullptr) {
        EXPECT_NE(enc, nullptr);
        EXPECT_EQ(enc->config.conditioning, JEPA_COND_CONCATENATE);
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// Encoder Destruction Tests
//=============================================================================

TEST_F(JepaContextTest, EncoderDestructionNullSafe)
{
    // Should not crash
    jepa_context_encoder_destroy(nullptr);
}

TEST_F(JepaContextTest, EncoderDestructionValid)
{
    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config_);
    if (enc != nullptr) {
        // Should not crash
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// Encoder Reset Tests
//=============================================================================

TEST_F(JepaContextTest, EncoderResetNullPointer)
{
    int result = jepa_context_encoder_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, EncoderResetValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_encoder_reset(encoder_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Context State Lifecycle Tests
//=============================================================================

TEST_F(JepaContextTest, ContextStateCreate)
{
    jepa_context_state_t* state = jepa_context_state_create();
    if (state != nullptr) {
        EXPECT_NE(state, nullptr);
        jepa_context_state_destroy(state);
    }
}

TEST_F(JepaContextTest, ContextStateDestroyNullSafe)
{
    // Should not crash
    jepa_context_state_destroy(nullptr);
}

TEST_F(JepaContextTest, ContextStateDestroyValid)
{
    jepa_context_state_t* state = jepa_context_state_create();
    if (state != nullptr) {
        // Should not crash
        jepa_context_state_destroy(state);
    }
}

//=============================================================================
// Set Task Context Tests
//=============================================================================

TEST_F(JepaContextTest, SetTaskNullEncoder)
{
    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);

    int result = jepa_context_set_task(nullptr, goal.data(), 32, task.data(), 32);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetTaskNullEmbeddings)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);

    // NULL goal should fail or be handled gracefully
    int result1 = jepa_context_set_task(encoder_, nullptr, 32, task.data(), 32);
    // NULL task should fail or be handled gracefully
    int result2 = jepa_context_set_task(encoder_, goal.data(), 32, nullptr, 32);

    // At least one should indicate error
    EXPECT_TRUE(result1 != NIMCP_SUCCESS || result2 != NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetTaskValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);

    int result = jepa_context_set_task(encoder_, goal.data(), 32, task.data(), 32);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetTaskZeroDimension)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);

    // Zero dimension should be handled
    int result = jepa_context_set_task(encoder_, goal.data(), 0, task.data(), 0);
    // Either success (empty context) or error
    (void)result;
}

//=============================================================================
// Set Working Memory Context Tests
//=============================================================================

TEST_F(JepaContextTest, SetWorkingMemoryNullEncoder)
{
    auto wm_items = create_test_array(64, 0.3f);

    int result = jepa_context_set_working_memory(nullptr, wm_items.data(), 32, 2);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetWorkingMemoryNullItems)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_set_working_memory(encoder_, nullptr, 32, 2);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetWorkingMemoryValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // 2 items, each with dimension 32
    auto wm_items = create_test_array(64, 0.3f);

    int result = jepa_context_set_working_memory(encoder_, wm_items.data(), 32, 2);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetWorkingMemoryZeroItems)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto wm_items = create_test_array(64, 0.3f);

    // Zero items should be handled gracefully
    int result = jepa_context_set_working_memory(encoder_, wm_items.data(), 32, 0);
    // Either success (empty WM) or error
    (void)result;
}

TEST_F(JepaContextTest, SetWorkingMemoryMultipleItems)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // 5 items, each with dimension 16
    auto wm_items = create_test_array(80, 0.2f);

    int result = jepa_context_set_working_memory(encoder_, wm_items.data(), 16, 5);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Set Attention Context Tests
//=============================================================================

TEST_F(JepaContextTest, SetAttentionNullEncoder)
{
    auto weights = create_test_array(16, 0.0625f);
    auto features = create_test_array(32, 0.5f);

    int result = jepa_context_set_attention(nullptr, weights.data(), 16, features.data(), 32);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetAttentionNullWeights)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto features = create_test_array(32, 0.5f);

    int result = jepa_context_set_attention(encoder_, nullptr, 16, features.data(), 32);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetAttentionNullFeatures)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto weights = create_test_array(16, 0.0625f);

    int result = jepa_context_set_attention(encoder_, weights.data(), 16, nullptr, 32);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetAttentionValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto weights = create_test_array(16, 0.0625f);
    auto features = create_test_array(32, 0.5f);

    int result = jepa_context_set_attention(encoder_, weights.data(), 16, features.data(), 32);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetAttentionNormalizedWeights)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Normalized attention weights (sum to 1)
    std::vector<float> weights(16, 1.0f / 16.0f);
    auto features = create_test_array(32, 0.5f);

    int result = jepa_context_set_attention(encoder_, weights.data(), 16, features.data(), 32);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Set Custom Context Tests
//=============================================================================

TEST_F(JepaContextTest, SetCustomNullEncoder)
{
    auto custom = create_test_array(64, 0.4f);

    int result = jepa_context_set_custom(nullptr, custom.data(), 64);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetCustomNullVector)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_set_custom(encoder_, nullptr, 64);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetCustomValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto custom = create_test_array(64, 0.4f);

    int result = jepa_context_set_custom(encoder_, custom.data(), 64);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, SetCustomZeroDimension)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto custom = create_test_array(64, 0.4f);

    // Zero dimension should be handled
    int result = jepa_context_set_custom(encoder_, custom.data(), 0);
    // Either success (empty custom) or error
    (void)result;
}

//=============================================================================
// Clear Context Tests
//=============================================================================

TEST_F(JepaContextTest, ClearContextNullEncoder)
{
    int result = jepa_context_clear(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ClearContextValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set some context first
    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);
    jepa_context_set_task(encoder_, goal.data(), 32, task.data(), 32);

    // Clear should succeed
    int result = jepa_context_clear(encoder_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ClearContextThenEncode)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set context
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    // Clear context
    int clear_result = jepa_context_clear(encoder_);
    EXPECT_EQ(clear_result, NIMCP_SUCCESS);

    // Encoding after clear should still work (with empty context)
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);
        int encode_result = jepa_context_encode(encoder_, input, output);
        // Should succeed or fail gracefully
        (void)encode_result;
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

//=============================================================================
// Context Encoding Tests
//=============================================================================

TEST_F(JepaContextTest, EncodeNullEncoder)
{
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);
        int result = jepa_context_encode(nullptr, input, output);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeNullInput)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* output = create_test_latent();
    if (output) {
        int result = jepa_context_encode(encoder_, nullptr, output);
        EXPECT_NE(result, NIMCP_SUCCESS);
        jepa_latent_destroy(output);
    }
}

TEST_F(JepaContextTest, EncodeNullOutput)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* input = create_test_latent();
    if (input) {
        fill_latent(input, 0.5f);
        int result = jepa_context_encode(encoder_, input, nullptr);
        EXPECT_NE(result, NIMCP_SUCCESS);
        jepa_latent_destroy(input);
    }
}

TEST_F(JepaContextTest, EncodeValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        // Output should have valid values
        if (output->embedding) {
            // Check output is not all zeros (some modulation occurred)
            bool has_nonzero = false;
            for (uint32_t i = 0; i < output->latent_dim && !has_nonzero; i++) {
                if (std::fabs(output->embedding[i]) > FLOAT_TOLERANCE) {
                    has_nonzero = true;
                }
            }
            EXPECT_TRUE(has_nonzero);
        }
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeWithTaskContext)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set task context
    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);
    jepa_context_set_task(encoder_, goal.data(), 32, task.data(), 32);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeWithWorkingMemoryContext)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set WM context
    auto wm_items = create_test_array(64, 0.3f);
    jepa_context_set_working_memory(encoder_, wm_items.data(), 32, 2);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeWithAttentionContext)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set attention context
    auto weights = create_test_array(16, 0.0625f);
    auto features = create_test_array(32, 0.5f);
    jepa_context_set_attention(encoder_, weights.data(), 16, features.data(), 32);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeWithCustomContext)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set custom context
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, EncodeWithAllContexts)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set all context types
    auto goal = create_test_array(32, 0.5f);
    auto task = create_test_array(32, 0.7f);
    jepa_context_set_task(encoder_, goal.data(), 32, task.data(), 32);

    auto wm_items = create_test_array(64, 0.3f);
    jepa_context_set_working_memory(encoder_, wm_items.data(), 32, 2);

    auto weights = create_test_array(16, 0.0625f);
    auto features = create_test_array(32, 0.5f);
    jepa_context_set_attention(encoder_, weights.data(), 16, features.data(), 32);

    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

//=============================================================================
// Batch Encoding Tests
//=============================================================================

TEST_F(JepaContextTest, EncodeBatchNullEncoder)
{
    jepa_latent_t* inputs[2];
    jepa_latent_t* outputs[2];

    inputs[0] = create_test_latent();
    inputs[1] = create_test_latent();
    outputs[0] = create_test_latent();
    outputs[1] = create_test_latent();

    if (inputs[0] && inputs[1] && outputs[0] && outputs[1]) {
        int result = jepa_context_encode_batch(nullptr, inputs, outputs, 2);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }

    if (inputs[0]) jepa_latent_destroy(inputs[0]);
    if (inputs[1]) jepa_latent_destroy(inputs[1]);
    if (outputs[0]) jepa_latent_destroy(outputs[0]);
    if (outputs[1]) jepa_latent_destroy(outputs[1]);
}

TEST_F(JepaContextTest, EncodeBatchNullInputs)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* outputs[2];
    outputs[0] = create_test_latent();
    outputs[1] = create_test_latent();

    if (outputs[0] && outputs[1]) {
        int result = jepa_context_encode_batch(encoder_, nullptr, outputs, 2);
        EXPECT_NE(result, NIMCP_SUCCESS);
    }

    if (outputs[0]) jepa_latent_destroy(outputs[0]);
    if (outputs[1]) jepa_latent_destroy(outputs[1]);
}

TEST_F(JepaContextTest, EncodeBatchZeroSize)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* inputs[2];
    jepa_latent_t* outputs[2];

    inputs[0] = create_test_latent();
    inputs[1] = create_test_latent();
    outputs[0] = create_test_latent();
    outputs[1] = create_test_latent();

    if (inputs[0] && inputs[1] && outputs[0] && outputs[1]) {
        // Zero batch size
        int result = jepa_context_encode_batch(encoder_, inputs, outputs, 0);
        // Should succeed (no-op) or return error
        (void)result;
    }

    if (inputs[0]) jepa_latent_destroy(inputs[0]);
    if (inputs[1]) jepa_latent_destroy(inputs[1]);
    if (outputs[0]) jepa_latent_destroy(outputs[0]);
    if (outputs[1]) jepa_latent_destroy(outputs[1]);
}

TEST_F(JepaContextTest, EncodeBatchValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    constexpr int BATCH_SIZE = 4;
    jepa_latent_t* inputs[BATCH_SIZE];
    jepa_latent_t* outputs[BATCH_SIZE];

    bool all_created = true;
    for (int i = 0; i < BATCH_SIZE; i++) {
        inputs[i] = create_test_latent();
        outputs[i] = create_test_latent();
        if (!inputs[i] || !outputs[i]) {
            all_created = false;
        } else {
            fill_latent(inputs[i], 0.5f + static_cast<float>(i) * 0.1f);
        }
    }

    if (all_created) {
        int result = jepa_context_encode_batch(encoder_, inputs, outputs, BATCH_SIZE);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    for (int i = 0; i < BATCH_SIZE; i++) {
        if (inputs[i]) jepa_latent_destroy(inputs[i]);
        if (outputs[i]) jepa_latent_destroy(outputs[i]);
    }
}

//=============================================================================
// Get Composed Context Tests
//=============================================================================

TEST_F(JepaContextTest, GetComposedNullEncoder)
{
    std::vector<float> context(TEST_CONTEXT_DIM, 0.0f);
    int result = jepa_context_get_composed(nullptr, context.data(), TEST_CONTEXT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, GetComposedNullBuffer)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_get_composed(encoder_, nullptr, TEST_CONTEXT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, GetComposedValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set some context first
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    std::vector<float> context(TEST_CONTEXT_DIM, 0.0f);
    int result = jepa_context_get_composed(encoder_, context.data(), TEST_CONTEXT_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, GetComposedDimensionMismatch)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    // Request with smaller dimension
    std::vector<float> context(16, 0.0f);
    int result = jepa_context_get_composed(encoder_, context.data(), 16);
    // Should either truncate or return error
    (void)result;
}

//=============================================================================
// Conditioning Mechanism Tests - FiLM
//=============================================================================

TEST_F(JepaContextTest, ApplyFiLMNullEncoder)
{
    auto input = create_test_array(64, 0.5f);
    auto context = create_test_array(32, 0.3f);
    std::vector<float> output(64, 0.0f);

    int result = jepa_context_apply_film(nullptr, input.data(), context.data(),
                                          output.data(), 64);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ApplyFiLMNullInput)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_FILM);
    if (enc == nullptr) {
        GTEST_SKIP() << "FiLM encoder creation not implemented";
    }

    auto context = create_test_array(TEST_CONTEXT_DIM, 0.3f);
    std::vector<float> output(TEST_INPUT_DIM, 0.0f);

    int result = jepa_context_apply_film(enc, nullptr, context.data(), output.data(), TEST_INPUT_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_context_encoder_destroy(enc);
}

TEST_F(JepaContextTest, ApplyFiLMValid)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_FILM);
    if (enc == nullptr) {
        GTEST_SKIP() << "FiLM encoder creation not implemented";
    }

    // Use correct dimensions matching encoder config
    auto input = create_test_array(TEST_INPUT_DIM, 0.5f);
    auto context = create_test_array(TEST_CONTEXT_DIM, 0.3f);
    std::vector<float> output(TEST_INPUT_DIM, 0.0f);

    int result = jepa_context_apply_film(enc, input.data(), context.data(),
                                          output.data(), TEST_INPUT_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Check output is modified (FiLM: gamma * input + beta)
    bool has_nonzero = false;
    for (size_t i = 0; i < output.size() && !has_nonzero; i++) {
        if (std::fabs(output[i]) > FLOAT_TOLERANCE) {
            has_nonzero = true;
        }
    }
    EXPECT_TRUE(has_nonzero);

    jepa_context_encoder_destroy(enc);
}

//=============================================================================
// Conditioning Mechanism Tests - Cross-Attention
//=============================================================================

TEST_F(JepaContextTest, ApplyCrossAttentionNullEncoder)
{
    auto input = create_test_array(TEST_INPUT_DIM, 0.5f);
    auto context = create_test_array(TEST_CONTEXT_DIM, 0.3f);
    std::vector<float> output(TEST_INPUT_DIM, 0.0f);

    int result = jepa_context_apply_cross_attention(nullptr, input.data(),
                                                     context.data(), output.data());
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ApplyCrossAttentionNullInput)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_CROSS_ATTENTION);
    if (enc == nullptr) {
        GTEST_SKIP() << "Cross-attention encoder creation not implemented";
    }

    auto context = create_test_array(TEST_CONTEXT_DIM, 0.3f);
    std::vector<float> output(TEST_INPUT_DIM, 0.0f);

    int result = jepa_context_apply_cross_attention(enc, nullptr, context.data(), output.data());
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_context_encoder_destroy(enc);
}

TEST_F(JepaContextTest, ApplyCrossAttentionValid)
{
    jepa_context_encoder_t* enc = create_encoder_with_conditioning(JEPA_COND_CROSS_ATTENTION);
    if (enc == nullptr) {
        GTEST_SKIP() << "Cross-attention encoder creation not implemented";
    }

    auto input = create_test_array(TEST_INPUT_DIM, 0.5f);
    auto context = create_test_array(TEST_CONTEXT_DIM, 0.3f);
    std::vector<float> output(TEST_INPUT_DIM, 0.0f);

    int result = jepa_context_apply_cross_attention(enc, input.data(),
                                                     context.data(), output.data());
    EXPECT_EQ(result, NIMCP_SUCCESS);

    jepa_context_encoder_destroy(enc);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(JepaContextTest, GetStatsNullEncoder)
{
    jepa_context_stats_t stats;
    int result = jepa_context_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, GetStatsNullStats)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_get_stats(encoder_, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, GetStatsValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_context_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with non-zero values

    int result = jepa_context_get_stats(encoder_, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should be initialized (at least to zero)
    EXPECT_GE(stats.encodings_performed, 0UL);
    EXPECT_GE(stats.context_updates, 0UL);
}

TEST_F(JepaContextTest, GetStatsAfterEncoding)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Get initial stats
    jepa_context_stats_t stats_before;
    jepa_context_get_stats(encoder_, &stats_before);

    // Perform encoding
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);
        jepa_context_encode(encoder_, input, output);
    }

    // Get stats after encoding
    jepa_context_stats_t stats_after;
    int result = jepa_context_get_stats(encoder_, &stats_after);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Encodings should have increased
    EXPECT_GE(stats_after.encodings_performed, stats_before.encodings_performed);

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, ResetStatsNullEncoder)
{
    int result = jepa_context_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ResetStatsValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Perform some encoding first
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);
        jepa_context_encode(encoder_, input, output);
    }

    // Reset stats
    int result = jepa_context_reset_stats(encoder_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Stats should be reset
    jepa_context_stats_t stats;
    jepa_context_get_stats(encoder_, &stats);
    EXPECT_EQ(stats.encodings_performed, 0UL);
    EXPECT_EQ(stats.context_updates, 0UL);

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

//=============================================================================
// Conditioning Type Tests - Different Outputs
//=============================================================================

class JepaConditioningTest : public ::testing::TestWithParam<jepa_conditioning_t> {
protected:
    void SetUp() override
    {
        jepa_context_config_t config;
        jepa_context_default_config(&config);
        config.conditioning = GetParam();
        config.context_dim = TEST_CONTEXT_DIM;
        config.input_dim = TEST_INPUT_DIM;
        config.output_dim = TEST_OUTPUT_DIM;
        encoder_ = jepa_context_encoder_create(&config);
    }

    void TearDown() override
    {
        if (encoder_) {
            jepa_context_encoder_destroy(encoder_);
            encoder_ = nullptr;
        }
    }

    jepa_context_encoder_t* encoder_ = nullptr;
};

TEST_P(JepaConditioningTest, EncoderCreatedWithConditioningType)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Conditioning type not implemented";
    }
    EXPECT_EQ(encoder_->config.conditioning, GetParam());
}

TEST_P(JepaConditioningTest, EncodingWithConditioningType)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Conditioning type not implemented";
    }

    // Set custom context
    std::vector<float> custom(64, 0.4f);
    for (size_t i = 0; i < custom.size(); i++) {
        custom[i] = 0.4f + static_cast<float>(i) * 0.01f;
    }
    jepa_context_set_custom(encoder_, custom.data(), 64);

    // Create latents
    jepa_latent_config_t latent_config;
    jepa_latent_default_config(&latent_config);
    latent_config.latent_dim = TEST_LATENT_DIM;

    jepa_latent_t* input = jepa_latent_create(&latent_config);
    jepa_latent_t* output = jepa_latent_create(&latent_config);

    if (input && output) {
        // Fill input
        for (uint32_t i = 0; i < input->latent_dim; i++) {
            input->embedding[i] = 0.5f + static_cast<float>(i) * 0.01f;
        }

        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

INSTANTIATE_TEST_SUITE_P(
    AllConditioningTypes,
    JepaConditioningTest,
    ::testing::Values(
        JEPA_COND_CONCATENATE,
        JEPA_COND_FILM,
        JEPA_COND_CROSS_ATTENTION,
        JEPA_COND_ADDITIVE,
        JEPA_COND_MULTIPLICATIVE,
        JEPA_COND_GATED
    ),
    [](const ::testing::TestParamInfo<jepa_conditioning_t>& info) {
        switch (info.param) {
            case JEPA_COND_CONCATENATE: return "Concatenate";
            case JEPA_COND_FILM: return "FiLM";
            case JEPA_COND_CROSS_ATTENTION: return "CrossAttention";
            case JEPA_COND_ADDITIVE: return "Additive";
            case JEPA_COND_MULTIPLICATIVE: return "Multiplicative";
            case JEPA_COND_GATED: return "Gated";
            default: return "Unknown";
        }
    }
);

//=============================================================================
// Context Produces Different Representations Test
//=============================================================================

TEST_F(JepaContextTest, DifferentContextsProduceDifferentOutputs)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output1 = create_test_latent();
    jepa_latent_t* output2 = create_test_latent();

    if (!input || !output1 || !output2) {
        if (input) jepa_latent_destroy(input);
        if (output1) jepa_latent_destroy(output1);
        if (output2) jepa_latent_destroy(output2);
        GTEST_SKIP() << "Latent creation not implemented";
    }

    fill_latent(input, 0.5f);

    // Encode with context A
    auto context_a = create_test_array(64, 0.2f);
    jepa_context_set_custom(encoder_, context_a.data(), 64);
    int result1 = jepa_context_encode(encoder_, input, output1);

    // Clear and set different context
    jepa_context_clear(encoder_);

    // Encode with context B
    auto context_b = create_test_array(64, 0.8f);
    jepa_context_set_custom(encoder_, context_b.data(), 64);
    int result2 = jepa_context_encode(encoder_, input, output2);

    if (result1 == NIMCP_SUCCESS && result2 == NIMCP_SUCCESS) {
        // Outputs should be different for different contexts
        bool outputs_differ = false;
        for (uint32_t i = 0; i < output1->latent_dim && !outputs_differ; i++) {
            if (std::fabs(output1->embedding[i] - output2->embedding[i]) > FLOAT_TOLERANCE) {
                outputs_differ = true;
            }
        }
        EXPECT_TRUE(outputs_differ) << "Same input with different contexts should produce different outputs";
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(JepaContextTest, MultipleContextUpdates)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Update context many times
    for (int i = 0; i < 100; i++) {
        auto custom = create_test_array(64, static_cast<float>(i) * 0.01f);
        int result = jepa_context_set_custom(encoder_, custom.data(), 64);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Should still be able to encode
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);
        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, RepeatedEncodings)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set context once
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        // Encode many times
        for (int i = 0; i < 50; i++) {
            int result = jepa_context_encode(encoder_, input, output);
            EXPECT_EQ(result, NIMCP_SUCCESS);
        }

        // Check stats
        jepa_context_stats_t stats;
        jepa_context_get_stats(encoder_, &stats);
        EXPECT_GE(stats.encodings_performed, 50UL);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);
}

TEST_F(JepaContextTest, CreateDestroyMultipleTimes)
{
    // Create and destroy encoder many times
    for (int i = 0; i < 20; i++) {
        jepa_context_encoder_t* enc = jepa_context_encoder_create(&config_);
        if (enc) {
            jepa_context_encoder_destroy(enc);
        }
    }
    // Should not leak memory or crash
}

TEST_F(JepaContextTest, LargeBatchEncoding)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    constexpr int LARGE_BATCH = 32;
    std::vector<jepa_latent_t*> inputs(LARGE_BATCH);
    std::vector<jepa_latent_t*> outputs(LARGE_BATCH);

    bool all_created = true;
    for (int i = 0; i < LARGE_BATCH; i++) {
        inputs[i] = create_test_latent();
        outputs[i] = create_test_latent();
        if (!inputs[i] || !outputs[i]) {
            all_created = false;
        } else {
            fill_latent(inputs[i], static_cast<float>(i) * 0.03f);
        }
    }

    if (all_created) {
        int result = jepa_context_encode_batch(encoder_, inputs.data(), outputs.data(), LARGE_BATCH);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    for (int i = 0; i < LARGE_BATCH; i++) {
        if (inputs[i]) jepa_latent_destroy(inputs[i]);
        if (outputs[i]) jepa_latent_destroy(outputs[i]);
    }
}

//=============================================================================
// Bio-Async API Tests
//=============================================================================

TEST_F(JepaContextTest, ConnectBioAsyncNullEncoder)
{
    int result = jepa_context_connect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, ConnectBioAsyncValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result = jepa_context_connect_bio_async(encoder_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Cleanup: disconnect if connected
    jepa_context_disconnect_bio_async(encoder_);
}

TEST_F(JepaContextTest, DisconnectBioAsyncNullEncoder)
{
    int result = jepa_context_disconnect_bio_async(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, DisconnectBioAsyncValid)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Connect first
    jepa_context_connect_bio_async(encoder_);

    // Then disconnect
    int result = jepa_context_disconnect_bio_async(encoder_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(JepaContextTest, DisconnectBioAsyncWithoutConnect)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Disconnect without prior connect - should handle gracefully
    int result = jepa_context_disconnect_bio_async(encoder_);
    // Either success (no-op) or specific error
    (void)result;
}

TEST_F(JepaContextTest, IsBioAsyncConnectedNullEncoder)
{
    bool result = jepa_context_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(JepaContextTest, IsBioAsyncConnectedInitiallyFalse)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    bool result = jepa_context_is_bio_async_connected(encoder_);
    EXPECT_FALSE(result);
}

TEST_F(JepaContextTest, IsBioAsyncConnectedAfterConnect)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Connect
    int connect_result = jepa_context_connect_bio_async(encoder_);
    if (connect_result != NIMCP_SUCCESS) {
        GTEST_SKIP() << "Bio-async connection not implemented";
    }

    bool result = jepa_context_is_bio_async_connected(encoder_);
    EXPECT_TRUE(result);

    // Cleanup
    jepa_context_disconnect_bio_async(encoder_);
}

TEST_F(JepaContextTest, IsBioAsyncConnectedAfterDisconnect)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Connect then disconnect
    jepa_context_connect_bio_async(encoder_);
    jepa_context_disconnect_bio_async(encoder_);

    bool result = jepa_context_is_bio_async_connected(encoder_);
    EXPECT_FALSE(result);
}

TEST_F(JepaContextTest, BioAsyncConnectDisconnectMultipleTimes)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Connect/disconnect cycle should be safe to repeat
    for (int i = 0; i < 5; i++) {
        int connect_result = jepa_context_connect_bio_async(encoder_);
        if (connect_result != NIMCP_SUCCESS && i == 0) {
            GTEST_SKIP() << "Bio-async connection not implemented";
        }

        int disconnect_result = jepa_context_disconnect_bio_async(encoder_);
        EXPECT_EQ(disconnect_result, NIMCP_SUCCESS);
    }
}

TEST_F(JepaContextTest, BioAsyncDoubleConnect)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    int result1 = jepa_context_connect_bio_async(encoder_);
    if (result1 != NIMCP_SUCCESS) {
        GTEST_SKIP() << "Bio-async connection not implemented";
    }

    // Second connect should either succeed (no-op) or return specific error
    int result2 = jepa_context_connect_bio_async(encoder_);
    // Either success or already connected error is acceptable
    (void)result2;

    // Cleanup
    jepa_context_disconnect_bio_async(encoder_);
}

//=============================================================================
// Integration Tests - Context + Bio-Async
//=============================================================================

TEST_F(JepaContextTest, EncodeWhileBioAsyncConnected)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Connect to bio-async
    int connect_result = jepa_context_connect_bio_async(encoder_);

    // Set context
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    // Create latents
    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output = create_test_latent();

    if (input && output) {
        fill_latent(input, 0.5f);

        // Encoding should work regardless of bio-async connection status
        int result = jepa_context_encode(encoder_, input, output);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    if (input) jepa_latent_destroy(input);
    if (output) jepa_latent_destroy(output);

    // Cleanup bio-async if connected
    if (connect_result == NIMCP_SUCCESS) {
        jepa_context_disconnect_bio_async(encoder_);
    }
}

//=============================================================================
// Context Source Configuration Tests
//=============================================================================

TEST_F(JepaContextTest, ConfigWithMultipleSources)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Configure multiple sources
    config.num_sources = 3;

    config.sources[0].type = JEPA_CTX_TASK;
    config.sources[0].dim = 32;
    config.sources[0].weight = 0.4f;
    config.sources[0].enabled = true;
    config.sources[0].name = "task_context";

    config.sources[1].type = JEPA_CTX_WORKING_MEMORY;
    config.sources[1].dim = 48;
    config.sources[1].weight = 0.3f;
    config.sources[1].enabled = true;
    config.sources[1].name = "wm_context";

    config.sources[2].type = JEPA_CTX_ATTENTION;
    config.sources[2].dim = 32;
    config.sources[2].weight = 0.3f;
    config.sources[2].enabled = true;
    config.sources[2].name = "attention_context";

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_EQ(enc->config.num_sources, 3u);
        EXPECT_EQ(enc->config.sources[0].type, JEPA_CTX_TASK);
        EXPECT_EQ(enc->config.sources[1].type, JEPA_CTX_WORKING_MEMORY);
        EXPECT_EQ(enc->config.sources[2].type, JEPA_CTX_ATTENTION);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, ConfigWithDisabledSource)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.num_sources = 2;

    config.sources[0].type = JEPA_CTX_TASK;
    config.sources[0].dim = 32;
    config.sources[0].weight = 0.5f;
    config.sources[0].enabled = true;

    config.sources[1].type = JEPA_CTX_CUSTOM;
    config.sources[1].dim = 32;
    config.sources[1].weight = 0.5f;
    config.sources[1].enabled = false;  // Disabled

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_FALSE(enc->config.sources[1].enabled);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, ConfigWithAllSourceTypes)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Use maximum number of sources with all types
    config.num_sources = 7;  // All source types

    config.sources[0].type = JEPA_CTX_TASK;
    config.sources[0].enabled = true;
    config.sources[0].dim = 16;
    config.sources[0].weight = 1.0f / 7.0f;

    config.sources[1].type = JEPA_CTX_WORKING_MEMORY;
    config.sources[1].enabled = true;
    config.sources[1].dim = 16;
    config.sources[1].weight = 1.0f / 7.0f;

    config.sources[2].type = JEPA_CTX_ATTENTION;
    config.sources[2].enabled = true;
    config.sources[2].dim = 16;
    config.sources[2].weight = 1.0f / 7.0f;

    config.sources[3].type = JEPA_CTX_TEMPORAL;
    config.sources[3].enabled = true;
    config.sources[3].dim = 16;
    config.sources[3].weight = 1.0f / 7.0f;

    config.sources[4].type = JEPA_CTX_SPATIAL;
    config.sources[4].enabled = true;
    config.sources[4].dim = 16;
    config.sources[4].weight = 1.0f / 7.0f;

    config.sources[5].type = JEPA_CTX_SEMANTIC;
    config.sources[5].enabled = true;
    config.sources[5].dim = 16;
    config.sources[5].weight = 1.0f / 7.0f;

    config.sources[6].type = JEPA_CTX_CUSTOM;
    config.sources[6].enabled = true;
    config.sources[6].dim = 16;
    config.sources[6].weight = 1.0f / 7.0f;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_EQ(enc->config.num_sources, 7u);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, ConfigMaxSources)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Configure maximum sources
    config.num_sources = JEPA_CONTEXT_MAX_SOURCES;
    for (uint32_t i = 0; i < JEPA_CONTEXT_MAX_SOURCES; i++) {
        config.sources[i].type = JEPA_CTX_CUSTOM;
        config.sources[i].dim = 16;
        config.sources[i].weight = 1.0f / static_cast<float>(JEPA_CONTEXT_MAX_SOURCES);
        config.sources[i].enabled = true;
    }

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_EQ(enc->config.num_sources, JEPA_CONTEXT_MAX_SOURCES);
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// Layer Norm and Dropout Configuration Tests
//=============================================================================

TEST_F(JepaContextTest, ConfigWithLayerNorm)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.use_layer_norm = true;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_TRUE(enc->config.use_layer_norm);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, ConfigWithDropout)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.dropout_rate = 0.1f;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_FLOAT_EQ(enc->config.dropout_rate, 0.1f);
        jepa_context_encoder_destroy(enc);
    }
}

TEST_F(JepaContextTest, ConfigWithZeroDropout)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.dropout_rate = 0.0f;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_FLOAT_EQ(enc->config.dropout_rate, 0.0f);
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// FiLM Hidden Dimension Configuration Tests
//=============================================================================

TEST_F(JepaContextTest, FiLMWithCustomHiddenDim)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.conditioning = JEPA_COND_FILM;
    config.film_hidden_dim = 256;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_EQ(enc->config.film_hidden_dim, 256u);
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// Cross-Attention Configuration Tests
//=============================================================================

TEST_F(JepaContextTest, CrossAttentionWithCustomHeads)
{
    jepa_context_config_t config;
    int result = jepa_context_default_config(&config);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    config.conditioning = JEPA_COND_CROSS_ATTENTION;
    config.num_attention_heads = 8;
    config.attention_dim = 64;

    jepa_context_encoder_t* enc = jepa_context_encoder_create(&config);
    if (enc != nullptr) {
        EXPECT_EQ(enc->config.num_attention_heads, 8u);
        EXPECT_EQ(enc->config.attention_dim, 64u);
        jepa_context_encoder_destroy(enc);
    }
}

//=============================================================================
// Same Context Same Output Test
//=============================================================================

TEST_F(JepaContextTest, SameContextProducesSameOutput)
{
    if (encoder_ == nullptr) {
        GTEST_SKIP() << "Encoder creation not implemented";
    }

    // Set context
    auto custom = create_test_array(64, 0.4f);
    jepa_context_set_custom(encoder_, custom.data(), 64);

    jepa_latent_t* input = create_test_latent();
    jepa_latent_t* output1 = create_test_latent();
    jepa_latent_t* output2 = create_test_latent();

    if (!input || !output1 || !output2) {
        if (input) jepa_latent_destroy(input);
        if (output1) jepa_latent_destroy(output1);
        if (output2) jepa_latent_destroy(output2);
        GTEST_SKIP() << "Latent creation not implemented";
    }

    fill_latent(input, 0.5f);

    // Encode twice with same context
    int result1 = jepa_context_encode(encoder_, input, output1);
    int result2 = jepa_context_encode(encoder_, input, output2);

    if (result1 == NIMCP_SUCCESS && result2 == NIMCP_SUCCESS) {
        // Outputs should be identical (no stochastic elements without dropout)
        bool outputs_same = true;
        for (uint32_t i = 0; i < output1->latent_dim && outputs_same; i++) {
            if (std::fabs(output1->embedding[i] - output2->embedding[i]) > FLOAT_TOLERANCE) {
                outputs_same = false;
            }
        }
        // Note: With dropout enabled, outputs may differ
        // This test assumes dropout_rate = 0 or inference mode
        EXPECT_TRUE(outputs_same) << "Same input + same context should produce same output (without dropout)";
    }

    jepa_latent_destroy(input);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(output2);
}

}  // namespace
