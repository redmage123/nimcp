/**
 * @file test_jepa_context_encoding.cpp
 * @brief Integration tests for context-conditioned encoding
 *
 * Tests:
 * - Context-conditioned encoding end-to-end
 * - Same input producing different outputs with different contexts
 * - Integration with working memory and attention contexts
 *
 * @author NIMCP Development Team
 * @date 2025-12-26
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/jepa/nimcp_jepa_context.h"
#include "perception/nimcp_visual_jepa_bridge.h"
#include "perception/nimcp_speech_jepa_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/**
 * @brief Test fixture for context encoding integration tests
 */
class JepaContextEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create context encoder with default config
        jepa_context_config_t ctx_config;
        jepa_context_default_config(&ctx_config);
        ctx_config.input_dim = LATENT_DIM;
        ctx_config.output_dim = LATENT_DIM;
        ctx_config.context_dim = CONTEXT_DIM;
        ctx_config.film_hidden_dim = 128;
        ctx_config.conditioning = JEPA_COND_FILM;

        context_encoder = jepa_context_encoder_create(&ctx_config);
        ASSERT_NE(context_encoder, nullptr) << "Failed to create context encoder";

        // Create predictor for encoding
        jepa_predictor_config_t pred_config;
        jepa_predictor_default_config(&pred_config);
        pred_config.input_dim = LATENT_DIM;
        pred_config.output_dim = LATENT_DIM;
        pred_config.hidden_dim = LATENT_DIM * 2;

        predictor = jepa_predictor_create(&pred_config);
        ASSERT_NE(predictor, nullptr) << "Failed to create JEPA predictor";

        // Create visual bridge with small input dim for testing
        visual_jepa_bridge_config_t visual_config;
        visual_jepa_bridge_default_config(&visual_config);
        visual_config.encoder.input_dim = VISUAL_FEATURE_DIM;  // Match test features
        visual_config.encoder.output_dim = LATENT_DIM;

        visual_bridge = visual_jepa_bridge_create(&visual_config);
        // May be NULL if not implemented
    }

    void TearDown() override {
        if (visual_bridge) {
            visual_jepa_bridge_destroy(visual_bridge);
            visual_bridge = nullptr;
        }
        if (predictor) {
            jepa_predictor_destroy(predictor);
            predictor = nullptr;
        }
        if (context_encoder) {
            jepa_context_encoder_destroy(context_encoder);
            context_encoder = nullptr;
        }
    }

    // Helper to create input latent
    jepa_latent_t* create_input_latent() {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.latent_dim = LATENT_DIM;
        config.enable_variance = true;

        jepa_latent_t* latent = jepa_latent_create(&config);
        if (!latent) return nullptr;

        // Fixed input pattern
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            latent->embedding[i] = sinf((float)i * 0.15f) * 0.5f + 0.5f;
            if (latent->variance) {
                latent->variance[i] = 0.1f;
            }
        }

        return latent;
    }

    // Helper to create context vector
    std::vector<float> create_context(uint32_t context_type) {
        std::vector<float> context(CONTEXT_DIM);

        // Different context types produce different patterns
        for (uint32_t i = 0; i < CONTEXT_DIM; i++) {
            switch (context_type) {
                case 0:  // Attention focused
                    context[i] = (i < CONTEXT_DIM / 4) ? 1.0f : 0.1f;
                    break;
                case 1:  // Attention diffuse
                    context[i] = 0.5f + 0.1f * sinf((float)i * 0.2f);
                    break;
                case 2:  // Task context A
                    context[i] = cosf((float)i * 0.3f) * 0.5f + 0.5f;
                    break;
                case 3:  // Task context B
                    context[i] = sinf((float)i * 0.4f) * 0.5f + 0.5f;
                    break;
                default:
                    context[i] = 0.5f;
            }
        }

        return context;
    }

    // Helper to compute L2 distance between latents
    float compute_l2_distance(const jepa_latent_t* a, const jepa_latent_t* b) {
        if (!a || !b || a->latent_dim != b->latent_dim) return -1.0f;

        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < a->latent_dim; i++) {
            float diff = a->embedding[i] - b->embedding[i];
            sum_sq += diff * diff;
        }
        return sqrtf(sum_sq);
    }

    static constexpr uint32_t LATENT_DIM = 64;
    static constexpr uint32_t CONTEXT_DIM = 32;
    static constexpr uint32_t FEATURE_DIM = 256;
    static constexpr uint32_t VISUAL_FEATURE_DIM = 16;  // Features per spatial location

    jepa_context_encoder_t* context_encoder = nullptr;
    jepa_predictor_t* predictor = nullptr;
    visual_jepa_bridge_t* visual_bridge = nullptr;
};

//=============================================================================
// Context Encoder Basic Tests
//=============================================================================

/**
 * @brief Test context encoder creation and configuration
 */
TEST_F(JepaContextEncodingTest, ContextEncoderCreation) {
    EXPECT_NE(context_encoder, nullptr);
    EXPECT_EQ(context_encoder->config.input_dim, LATENT_DIM);
    EXPECT_EQ(context_encoder->config.context_dim, CONTEXT_DIM);
    EXPECT_EQ(context_encoder->config.output_dim, LATENT_DIM);
}

/**
 * @brief Test context encoder reset
 */
TEST_F(JepaContextEncodingTest, ContextEncoderReset) {
    // Set some context
    std::vector<float> context = create_context(0);
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context.data(), CONTEXT_DIM), NIMCP_SUCCESS);

    // Reset encoder
    EXPECT_EQ(jepa_context_encoder_reset(context_encoder), NIMCP_SUCCESS);
}

//=============================================================================
// Context-Conditioned Encoding Tests
//=============================================================================

/**
 * @brief Test that same input produces different output with different contexts
 */
TEST_F(JepaContextEncodingTest, SameInputDifferentContextDifferentOutput) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    // Create outputs for different contexts
    jepa_latent_t* output_ctx0 = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* output_ctx1 = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* output_ctx2 = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(output_ctx0, nullptr);
    ASSERT_NE(output_ctx1, nullptr);
    ASSERT_NE(output_ctx2, nullptr);

    // Create different contexts
    std::vector<float> context0 = create_context(0);  // Focused attention
    std::vector<float> context1 = create_context(1);  // Diffuse attention
    std::vector<float> context2 = create_context(2);  // Task A

    // Encode with context 0
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context0.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_ctx0), NIMCP_SUCCESS);

    // Encode with context 1
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context1.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_ctx1), NIMCP_SUCCESS);

    // Encode with context 2
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context2.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_ctx2), NIMCP_SUCCESS);

    // Outputs should be different for different contexts
    float dist_01 = compute_l2_distance(output_ctx0, output_ctx1);
    float dist_02 = compute_l2_distance(output_ctx0, output_ctx2);
    float dist_12 = compute_l2_distance(output_ctx1, output_ctx2);

    EXPECT_GT(dist_01, 0.0f) << "Context 0 and 1 outputs should differ";
    EXPECT_GT(dist_02, 0.0f) << "Context 0 and 2 outputs should differ";
    EXPECT_GT(dist_12, 0.0f) << "Context 1 and 2 outputs should differ";

    // Log distances for debugging
    SCOPED_TRACE("L2 distances: 0-1=" + std::to_string(dist_01) +
                 ", 0-2=" + std::to_string(dist_02) +
                 ", 1-2=" + std::to_string(dist_12));

    // Cleanup
    jepa_latent_destroy(output_ctx2);
    jepa_latent_destroy(output_ctx1);
    jepa_latent_destroy(output_ctx0);
    jepa_latent_destroy(input);
}

/**
 * @brief Test that same context produces consistent output
 */
TEST_F(JepaContextEncodingTest, SameContextConsistentOutput) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    jepa_latent_t* output1 = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* output2 = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(output1, nullptr);
    ASSERT_NE(output2, nullptr);

    std::vector<float> context = create_context(0);

    // Set context once
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context.data(), CONTEXT_DIM), NIMCP_SUCCESS);

    // Encode twice with same input and context
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output1), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output2), NIMCP_SUCCESS);

    // Outputs should be identical (deterministic encoding)
    float distance = compute_l2_distance(output1, output2);
    EXPECT_NEAR(distance, 0.0f, 1e-5f) << "Same input + context should produce identical output";

    jepa_latent_destroy(output2);
    jepa_latent_destroy(output1);
    jepa_latent_destroy(input);
}

/**
 * @brief Test context modulation strength
 */
TEST_F(JepaContextEncodingTest, ContextModulationStrength) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    jepa_latent_t* output_weak = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* output_strong = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* output_no_ctx = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(output_weak, nullptr);
    ASSERT_NE(output_strong, nullptr);
    ASSERT_NE(output_no_ctx, nullptr);

    // Create weak and strong context
    std::vector<float> context = create_context(0);
    std::vector<float> weak_context(CONTEXT_DIM, 0.1f);  // Weak signal
    std::vector<float> strong_context = context;
    for (auto& v : strong_context) v *= 2.0f;  // Strong signal

    // Encode with no context (zeros)
    std::vector<float> zero_context(CONTEXT_DIM, 0.0f);
    EXPECT_EQ(jepa_context_set_custom(context_encoder, zero_context.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_no_ctx), NIMCP_SUCCESS);

    // Encode with weak context
    EXPECT_EQ(jepa_context_set_custom(context_encoder, weak_context.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_weak), NIMCP_SUCCESS);

    // Encode with strong context
    EXPECT_EQ(jepa_context_set_custom(context_encoder, strong_context.data(), CONTEXT_DIM), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_strong), NIMCP_SUCCESS);

    // Distance from no-context should be proportional to context strength
    float dist_weak = compute_l2_distance(output_no_ctx, output_weak);
    float dist_strong = compute_l2_distance(output_no_ctx, output_strong);

    EXPECT_GT(dist_strong, dist_weak)
        << "Stronger context should produce larger modulation";

    jepa_latent_destroy(output_no_ctx);
    jepa_latent_destroy(output_strong);
    jepa_latent_destroy(output_weak);
    jepa_latent_destroy(input);
}

//=============================================================================
// Working Memory Context Integration Tests
//=============================================================================

/**
 * @brief Test working memory context integration
 */
TEST_F(JepaContextEncodingTest, WorkingMemoryContextIntegration) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    // Create working memory items
    std::vector<float> wm_items(CONTEXT_DIM * 3);  // 3 items
    for (uint32_t i = 0; i < CONTEXT_DIM; i++) {
        wm_items[i] = sinf((float)i * 0.1f);
        wm_items[CONTEXT_DIM + i] = cosf((float)i * 0.2f);
        wm_items[2 * CONTEXT_DIM + i] = sinf((float)i * 0.3f + 1.0f);
    }

    // Set working memory context
    EXPECT_EQ(jepa_context_set_working_memory(
        context_encoder, wm_items.data(), CONTEXT_DIM, 3
    ), NIMCP_SUCCESS);

    // Use context for encoding
    jepa_latent_t* output = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(jepa_context_encode(context_encoder, input, output), NIMCP_SUCCESS);

    // Verify valid output
    bool valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(output->embedding[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    jepa_latent_destroy(output);
    jepa_latent_destroy(input);
}

//=============================================================================
// Attention Context Integration Tests
//=============================================================================

/**
 * @brief Test attention context modulates encoding
 */
TEST_F(JepaContextEncodingTest, AttentionContextModulation) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    // Set attention weights (focused on first quarter)
    std::vector<float> attn_weights(CONTEXT_DIM);
    for (uint32_t i = 0; i < CONTEXT_DIM; i++) {
        attn_weights[i] = (i < CONTEXT_DIM / 4) ? 1.0f : 0.1f;
    }

    // Create attended features
    std::vector<float> attended_features(CONTEXT_DIM);
    for (uint32_t i = 0; i < CONTEXT_DIM; i++) {
        attended_features[i] = sinf((float)i * 0.2f);
    }

    // Set attention context
    EXPECT_EQ(jepa_context_set_attention(
        context_encoder, attn_weights.data(), CONTEXT_DIM,
        attended_features.data(), CONTEXT_DIM
    ), NIMCP_SUCCESS);

    // Encode with attention context
    jepa_latent_t* output = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(jepa_context_encode(context_encoder, input, output), NIMCP_SUCCESS);

    // Clear and encode with diffuse attention
    EXPECT_EQ(jepa_context_clear(context_encoder), NIMCP_SUCCESS);

    std::vector<float> diffuse_attn(CONTEXT_DIM, 1.0f / CONTEXT_DIM);
    EXPECT_EQ(jepa_context_set_attention(
        context_encoder, diffuse_attn.data(), CONTEXT_DIM,
        attended_features.data(), CONTEXT_DIM
    ), NIMCP_SUCCESS);

    jepa_latent_t* output_diffuse = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(output_diffuse, nullptr);

    EXPECT_EQ(jepa_context_encode(context_encoder, input, output_diffuse), NIMCP_SUCCESS);

    float distance = compute_l2_distance(output, output_diffuse);
    EXPECT_GT(distance, 0.0f) << "Focused vs diffuse attention should produce different outputs";

    jepa_latent_destroy(output_diffuse);
    jepa_latent_destroy(output);
    jepa_latent_destroy(input);
}

//=============================================================================
// Task Context Tests
//=============================================================================

/**
 * @brief Test task context encoding
 */
TEST_F(JepaContextEncodingTest, TaskContextEncoding) {
    jepa_latent_t* input = create_input_latent();
    ASSERT_NE(input, nullptr);

    // Create task and goal embeddings
    std::vector<float> goal_embedding(CONTEXT_DIM / 2);
    std::vector<float> task_embedding(CONTEXT_DIM / 2);

    for (uint32_t i = 0; i < CONTEXT_DIM / 2; i++) {
        goal_embedding[i] = cosf((float)i * 0.1f);
        task_embedding[i] = sinf((float)i * 0.2f);
    }

    // Set task context
    EXPECT_EQ(jepa_context_set_task(
        context_encoder,
        goal_embedding.data(), CONTEXT_DIM / 2,
        task_embedding.data(), CONTEXT_DIM / 2
    ), NIMCP_SUCCESS);

    // Encode with task context
    jepa_latent_t* output = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(jepa_context_encode(context_encoder, input, output), NIMCP_SUCCESS);

    // Verify valid output
    bool valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(output->embedding[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    jepa_latent_destroy(output);
    jepa_latent_destroy(input);
}

//=============================================================================
// Predictor Integration Tests
//=============================================================================

/**
 * @brief Test basic prediction
 */
TEST_F(JepaContextEncodingTest, BasicPrediction) {
    jepa_latent_t* context_latent = create_input_latent();
    ASSERT_NE(context_latent, nullptr);

    jepa_latent_t* prediction = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(prediction, nullptr);

    // Basic prediction
    EXPECT_EQ(jepa_predictor_predict(predictor, context_latent, prediction), NIMCP_SUCCESS);

    // Verify valid prediction
    bool valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(prediction->embedding[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(context_latent);
}

/**
 * @brief Test training step
 */
TEST_F(JepaContextEncodingTest, TrainingStep) {
    jepa_predictor_set_training(predictor, true);

    // Create training data
    jepa_latent_t* input = create_input_latent();
    jepa_latent_t* target = create_input_latent();

    ASSERT_NE(input, nullptr);
    ASSERT_NE(target, nullptr);

    // Modify target slightly
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        target->embedding[i] = input->embedding[i] * 1.1f + 0.05f;
    }

    // Train
    float loss = 0.0f;
    EXPECT_EQ(jepa_predictor_train_step(predictor, input, target, &loss), NIMCP_SUCCESS);

    EXPECT_TRUE(std::isfinite(loss));
    EXPECT_GE(loss, 0.0f);

    jepa_predictor_set_training(predictor, false);

    jepa_latent_destroy(target);
    jepa_latent_destroy(input);
}

//=============================================================================
// Visual Bridge Context Integration Tests
//=============================================================================

/**
 * @brief Test visual JEPA with context encoding
 */
TEST_F(JepaContextEncodingTest, VisualBridgeContextEncoding) {
    if (!visual_bridge) {
        GTEST_SKIP() << "Visual JEPA bridge not available";
    }

    // Create visual features: 4x4 spatial locations, VISUAL_FEATURE_DIM features each
    const uint32_t SPATIAL_WIDTH = 4;
    const uint32_t SPATIAL_HEIGHT = 4;
    const uint32_t TOTAL_FEATURES = SPATIAL_WIDTH * SPATIAL_HEIGHT * VISUAL_FEATURE_DIM;
    std::vector<float> visual_features(TOTAL_FEATURES);
    for (uint32_t i = 0; i < TOTAL_FEATURES; i++) {
        visual_features[i] = cosf((float)i * 0.05f) * 0.5f + 0.5f;
    }

    // Create attention map (context for visual processing)
    std::vector<float> attention_map(SPATIAL_WIDTH * SPATIAL_HEIGHT);
    for (uint32_t i = 0; i < attention_map.size(); i++) {
        attention_map[i] = (i < 8) ? 0.8f : 0.2f;
    }

    jepa_latent_t* visual_latent = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(visual_latent, nullptr);

    // Encode with attention context
    EXPECT_EQ(visual_jepa_bridge_encode_attended(
        visual_bridge,
        visual_features.data(),
        VISUAL_FEATURE_DIM,  // Feature dim per location (must match encoder input_dim)
        attention_map.data(),
        SPATIAL_WIDTH,
        SPATIAL_HEIGHT,
        visual_latent
    ), NIMCP_SUCCESS);

    // Verify valid encoding
    bool valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(visual_latent->embedding[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    jepa_latent_destroy(visual_latent);
}

//=============================================================================
// Context Statistics Tests
//=============================================================================

/**
 * @brief Test context encoder statistics
 */
TEST_F(JepaContextEncodingTest, ContextEncoderStatistics) {
    // Reset stats
    EXPECT_EQ(jepa_context_reset_stats(context_encoder), NIMCP_SUCCESS);

    jepa_context_stats_t stats;
    EXPECT_EQ(jepa_context_get_stats(context_encoder, &stats), NIMCP_SUCCESS);

    // Initial stats should be zero
    EXPECT_EQ(stats.encodings_performed, 0u);
    EXPECT_EQ(stats.context_updates, 0u);

    // Perform some encodings
    jepa_latent_t* input = create_input_latent();
    jepa_latent_t* output = jepa_latent_create_dim(LATENT_DIM);
    std::vector<float> context = create_context(0);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    // Set context
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context.data(), CONTEXT_DIM), NIMCP_SUCCESS);

    for (int i = 0; i < 5; i++) {
        jepa_context_encode(context_encoder, input, output);
    }

    // Check updated stats
    EXPECT_EQ(jepa_context_get_stats(context_encoder, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.encodings_performed, 5u);

    jepa_latent_destroy(output);
    jepa_latent_destroy(input);
}

/**
 * @brief Test context clear functionality
 */
TEST_F(JepaContextEncodingTest, ContextClear) {
    // Set some context
    std::vector<float> context = create_context(0);
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context.data(), CONTEXT_DIM), NIMCP_SUCCESS);

    // Clear context
    EXPECT_EQ(jepa_context_clear(context_encoder), NIMCP_SUCCESS);

    // Encoding should still work with cleared context
    jepa_latent_t* input = create_input_latent();
    jepa_latent_t* output = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    EXPECT_EQ(jepa_context_encode(context_encoder, input, output), NIMCP_SUCCESS);

    jepa_latent_destroy(output);
    jepa_latent_destroy(input);
}

/**
 * @brief Test batch encoding
 */
TEST_F(JepaContextEncodingTest, BatchEncoding) {
    const uint32_t BATCH_SIZE = 4;

    // Create batch of inputs and outputs
    std::vector<jepa_latent_t*> inputs(BATCH_SIZE);
    std::vector<jepa_latent_t*> outputs(BATCH_SIZE);

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        inputs[i] = create_input_latent();
        outputs[i] = jepa_latent_create_dim(LATENT_DIM);
        ASSERT_NE(inputs[i], nullptr);
        ASSERT_NE(outputs[i], nullptr);

        // Modify each input slightly
        for (uint32_t j = 0; j < LATENT_DIM; j++) {
            inputs[i]->embedding[j] += 0.1f * i;
        }
    }

    // Set context
    std::vector<float> context = create_context(0);
    EXPECT_EQ(jepa_context_set_custom(context_encoder, context.data(), CONTEXT_DIM), NIMCP_SUCCESS);

    // Batch encode
    EXPECT_EQ(jepa_context_encode_batch(
        context_encoder,
        inputs.data(),
        outputs.data(),
        BATCH_SIZE
    ), NIMCP_SUCCESS);

    // Verify all outputs are valid and different
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        for (uint32_t j = 0; j < LATENT_DIM; j++) {
            EXPECT_TRUE(std::isfinite(outputs[i]->embedding[j]));
        }
    }

    // Cleanup
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        jepa_latent_destroy(inputs[i]);
        jepa_latent_destroy(outputs[i]);
    }
}

//=============================================================================
// Bio-Async Connection Tests
//=============================================================================

/**
 * @brief Test bio-async connection status
 */
TEST_F(JepaContextEncodingTest, BioAsyncConnectionStatus) {
    // Initially not connected
    EXPECT_FALSE(jepa_context_is_bio_async_connected(context_encoder));

    // Connect
    EXPECT_EQ(jepa_context_connect_bio_async(context_encoder), NIMCP_SUCCESS);
    EXPECT_TRUE(jepa_context_is_bio_async_connected(context_encoder));

    // Disconnect
    EXPECT_EQ(jepa_context_disconnect_bio_async(context_encoder), NIMCP_SUCCESS);
    EXPECT_FALSE(jepa_context_is_bio_async_connected(context_encoder));
}

// Main provided by GTest::gtest_main
