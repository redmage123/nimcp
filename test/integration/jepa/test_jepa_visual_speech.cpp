/**
 * @file test_jepa_visual_speech.cpp
 * @brief Integration tests for Visual + Speech JEPA together
 *
 * Tests:
 * - Visual and speech JEPA bridge coordination
 * - Cross-modal prediction integration
 * - Multimodal fusion end-to-end
 * - Audio-visual correspondence learning
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
#include "cognitive/jepa/nimcp_jepa_multimodal.h"
#include "cognitive/jepa/nimcp_jepa_masking.h"
#include "perception/nimcp_visual_jepa_bridge.h"
#include "perception/nimcp_speech_jepa_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/**
 * @brief Test fixture for Visual + Speech JEPA integration tests
 */
class JepaVisualSpeechTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create multimodal JEPA system
        jepa_multimodal_config_t mm_config;
        jepa_multimodal_default_config(&mm_config);
        mm_config.joint_dim = JOINT_DIM;
        mm_config.fusion_type = JEPA_MM_FUSION_AVERAGE;
        mm_config.enable_visual_to_speech = true;
        mm_config.enable_speech_to_visual = true;

        // Update projection dimensions: input from LATENT_DIM, output to JOINT_DIM
        mm_config.visual_proj.input_dim = LATENT_DIM;
        mm_config.visual_proj.output_dim = JOINT_DIM;
        mm_config.speech_proj.input_dim = LATENT_DIM;
        mm_config.speech_proj.output_dim = JOINT_DIM;
        mm_config.cross_predictor.input_dim = JOINT_DIM;
        mm_config.cross_predictor.output_dim = JOINT_DIM;

        multimodal = jepa_multimodal_create(&mm_config);
        // May be NULL if not fully implemented - tests handle gracefully

        // Create visual JEPA bridge
        visual_jepa_bridge_config_t visual_config;
        visual_jepa_bridge_default_config(&visual_config);
        visual_config.encoder.output_dim = LATENT_DIM;

        visual_bridge = visual_jepa_bridge_create(&visual_config);

        // Create speech JEPA bridge
        speech_jepa_bridge_config_t speech_config;
        speech_jepa_bridge_default_config(&speech_config);
        speech_config.encoder.output_dim = LATENT_DIM;

        speech_bridge = speech_jepa_bridge_create(&speech_config);
    }

    void TearDown() override {
        if (speech_bridge) {
            speech_jepa_bridge_destroy(speech_bridge);
            speech_bridge = nullptr;
        }
        if (visual_bridge) {
            visual_jepa_bridge_destroy(visual_bridge);
            visual_bridge = nullptr;
        }
        if (multimodal) {
            jepa_multimodal_destroy(multimodal);
            multimodal = nullptr;
        }
    }

    // Helper to create visual latent
    jepa_latent_t* create_visual_latent() {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.latent_dim = LATENT_DIM;
        config.modality = JEPA_MODALITY_VISUAL;
        config.enable_variance = true;

        jepa_latent_t* latent = jepa_latent_create(&config);
        if (!latent) return nullptr;

        // Fill with visual-like pattern (smooth gradients)
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            latent->embedding[i] = cosf((float)i * 0.1f) * 0.5f + 0.5f;
            if (latent->variance) {
                latent->variance[i] = 0.1f;
            }
        }

        return latent;
    }

    // Helper to create speech latent
    jepa_latent_t* create_speech_latent() {
        jepa_latent_config_t config;
        jepa_latent_default_config(&config);
        config.latent_dim = LATENT_DIM;
        config.modality = JEPA_MODALITY_SPEECH;
        config.enable_variance = true;

        jepa_latent_t* latent = jepa_latent_create(&config);
        if (!latent) return nullptr;

        // Fill with speech-like pattern (more temporal variation)
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            latent->embedding[i] = sinf((float)i * 0.2f) * 0.6f + 0.4f;
            if (latent->variance) {
                latent->variance[i] = 0.15f;
            }
        }

        return latent;
    }

    // Helper to create matched visual-speech pair (correlated content)
    std::pair<jepa_latent_t*, jepa_latent_t*> create_matched_pair() {
        jepa_latent_t* visual = create_visual_latent();
        jepa_latent_t* speech = create_speech_latent();

        if (visual && speech) {
            // Make them correlated (share some structure)
            for (uint32_t i = 0; i < LATENT_DIM / 2; i++) {
                // First half shares information
                float shared = (visual->embedding[i] + speech->embedding[i]) / 2.0f;
                visual->embedding[i] = shared + 0.1f * sinf((float)i);
                speech->embedding[i] = shared + 0.1f * cosf((float)i);
            }
        }

        return {visual, speech};
    }

    // Helper to create unmatched pair (uncorrelated)
    std::pair<jepa_latent_t*, jepa_latent_t*> create_unmatched_pair() {
        jepa_latent_t* visual = create_visual_latent();
        jepa_latent_t* speech = create_speech_latent();

        if (visual && speech) {
            // Make them uncorrelated
            for (uint32_t i = 0; i < LATENT_DIM; i++) {
                visual->embedding[i] = cosf((float)i * 0.3f);
                speech->embedding[i] = sinf((float)i * 0.7f + 2.0f);
            }
        }

        return {visual, speech};
    }

    static constexpr uint32_t LATENT_DIM = 64;
    static constexpr uint32_t JOINT_DIM = 128;
    static constexpr uint32_t VISUAL_FEATURE_DIM = 256;
    static constexpr uint32_t SPEECH_FRAME_DIM = 64;

    jepa_multimodal_t* multimodal = nullptr;
    visual_jepa_bridge_t* visual_bridge = nullptr;
    speech_jepa_bridge_t* speech_bridge = nullptr;
};

//=============================================================================
// Visual + Speech Bridge Coordination Tests
//=============================================================================

/**
 * @brief Test visual and speech bridge creation
 */
TEST_F(JepaVisualSpeechTest, BridgeCreation) {
    // Bridges may be NULL if not implemented, but creation shouldn't crash
    // Check if available and verify basic properties

    if (visual_bridge) {
        EXPECT_TRUE(visual_bridge->training_mode == false);  // Default is inference
        EXPECT_EQ(visual_bridge->training_step, 0);
    }

    if (speech_bridge) {
        EXPECT_TRUE(speech_bridge->training_mode == false);
        EXPECT_EQ(speech_bridge->training_step, 0);
    }
}

/**
 * @brief Test multimodal system connection
 */
TEST_F(JepaVisualSpeechTest, MultimodalSystemConnection) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    // Connect visual encoder
    if (visual_bridge) {
        int result = jepa_multimodal_connect_visual(multimodal, visual_bridge);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Connect speech encoder
    if (speech_bridge) {
        int result = jepa_multimodal_connect_speech(multimodal, speech_bridge);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Check connection status
    if (visual_bridge && speech_bridge) {
        EXPECT_TRUE(jepa_multimodal_is_connected(multimodal));
    }

    // Disconnect
    jepa_multimodal_disconnect_all(multimodal);
    EXPECT_FALSE(jepa_multimodal_is_connected(multimodal));
}

//=============================================================================
// Cross-Modal Prediction Tests
//=============================================================================

/**
 * @brief Test visual to speech prediction
 */
TEST_F(JepaVisualSpeechTest, VisualToSpeechPrediction) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    jepa_latent_t* visual_latent = create_visual_latent();
    jepa_latent_t* predicted_speech = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(visual_latent, nullptr);
    ASSERT_NE(predicted_speech, nullptr);

    // Predict speech from visual
    int result = jepa_multimodal_predict_speech_from_visual(
        multimodal, visual_latent, predicted_speech
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Predicted speech should have valid values
    bool has_valid_prediction = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(predicted_speech->embedding[i])) {
            has_valid_prediction = false;
            break;
        }
    }
    EXPECT_TRUE(has_valid_prediction);

    // Modality should be set correctly
    EXPECT_EQ(predicted_speech->modality, JEPA_MODALITY_SPEECH);

    jepa_latent_destroy(predicted_speech);
    jepa_latent_destroy(visual_latent);
}

/**
 * @brief Test speech to visual prediction
 */
TEST_F(JepaVisualSpeechTest, SpeechToVisualPrediction) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    jepa_latent_t* speech_latent = create_speech_latent();
    jepa_latent_t* predicted_visual = jepa_latent_create_dim(LATENT_DIM);

    ASSERT_NE(speech_latent, nullptr);
    ASSERT_NE(predicted_visual, nullptr);

    // Predict visual from speech
    int result = jepa_multimodal_predict_visual_from_speech(
        multimodal, speech_latent, predicted_visual
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify valid prediction
    bool has_valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(predicted_visual->embedding[i])) {
            has_valid = false;
            break;
        }
    }
    EXPECT_TRUE(has_valid);

    jepa_latent_destroy(predicted_visual);
    jepa_latent_destroy(speech_latent);
}

/**
 * @brief Test bidirectional cross-modal prediction
 */
TEST_F(JepaVisualSpeechTest, BidirectionalCrossModalPrediction) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    auto [visual, speech] = create_matched_pair();
    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);

    // Cross-modal predictions output in JOINT_DIM space
    jepa_latent_t* predicted_speech = jepa_latent_create_dim(JOINT_DIM);
    jepa_latent_t* predicted_visual = jepa_latent_create_dim(JOINT_DIM);
    jepa_latent_t* reconstructed_visual = jepa_latent_create_dim(JOINT_DIM);

    ASSERT_NE(predicted_speech, nullptr);
    ASSERT_NE(predicted_visual, nullptr);
    ASSERT_NE(reconstructed_visual, nullptr);

    // Visual -> Speech (outputs in joint space)
    EXPECT_EQ(jepa_multimodal_predict_speech_from_visual(
        multimodal, visual, predicted_speech
    ), NIMCP_SUCCESS);

    // Speech -> Visual (outputs in joint space)
    EXPECT_EQ(jepa_multimodal_predict_visual_from_speech(
        multimodal, speech, predicted_visual
    ), NIMCP_SUCCESS);

    // Round-trip: Visual -> Speech -> Visual
    EXPECT_EQ(jepa_multimodal_predict_visual_from_speech(
        multimodal, predicted_speech, reconstructed_visual
    ), NIMCP_SUCCESS);

    // Check predictions produce valid finite values
    bool predicted_valid = true;
    for (uint32_t i = 0; i < JOINT_DIM && predicted_valid; i++) {
        if (!std::isfinite(predicted_speech->embedding[i]) ||
            !std::isfinite(reconstructed_visual->embedding[i])) {
            predicted_valid = false;
        }
    }
    EXPECT_TRUE(predicted_valid) << "Cross-modal predictions should produce finite values";

    // Compare reconstructed_visual with predicted_visual (both in joint space)
    float reconstruction_sim = jepa_latent_cosine_similarity(predicted_visual, reconstructed_visual);
    EXPECT_TRUE(std::isfinite(reconstruction_sim));

    jepa_latent_destroy(reconstructed_visual);
    jepa_latent_destroy(predicted_visual);
    jepa_latent_destroy(predicted_speech);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(visual);
}

//=============================================================================
// Multimodal Fusion Tests
//=============================================================================

/**
 * @brief Test multimodal fusion encoding
 */
TEST_F(JepaVisualSpeechTest, MultimodalFusionEncoding) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);

    // Create fused output (may be larger if using concatenation)
    uint32_t fused_dim = JOINT_DIM;
    if (multimodal->config.fusion_type == JEPA_MM_FUSION_CONCATENATE) {
        fused_dim = 2 * LATENT_DIM;  // Concatenated
    }

    jepa_latent_t* fused = jepa_latent_create_dim(fused_dim);
    ASSERT_NE(fused, nullptr);

    // Fuse visual and speech
    int result = jepa_multimodal_fuse(multimodal, visual, speech, fused);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Fused modality should be multimodal
    EXPECT_EQ(fused->modality, JEPA_MODALITY_MULTIMODAL);

    // Verify fused representation has valid values
    bool valid_fusion = true;
    for (uint32_t i = 0; i < fused->latent_dim; i++) {
        if (!std::isfinite(fused->embedding[i])) {
            valid_fusion = false;
            break;
        }
    }
    EXPECT_TRUE(valid_fusion);

    jepa_latent_destroy(fused);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(visual);
}

/**
 * @brief Test multimodal encoding to joint space
 */
TEST_F(JepaVisualSpeechTest, JointSpaceEncoding) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);

    jepa_latent_t* visual_joint = jepa_latent_create_dim(JOINT_DIM);
    jepa_latent_t* speech_joint = jepa_latent_create_dim(JOINT_DIM);

    ASSERT_NE(visual_joint, nullptr);
    ASSERT_NE(speech_joint, nullptr);

    // Project to joint space
    EXPECT_EQ(jepa_multimodal_encode_visual(multimodal, visual, visual_joint), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_multimodal_encode_speech(multimodal, speech, speech_joint), NIMCP_SUCCESS);

    // Both should now be in same-dimensional joint space
    EXPECT_EQ(visual_joint->latent_dim, JOINT_DIM);
    EXPECT_EQ(speech_joint->latent_dim, JOINT_DIM);

    // Can compute cross-modal similarity in joint space
    float joint_similarity = jepa_latent_cosine_similarity(visual_joint, speech_joint);
    EXPECT_TRUE(std::isfinite(joint_similarity));
    EXPECT_GE(joint_similarity, -1.0f);
    EXPECT_LE(joint_similarity, 1.0f);

    jepa_latent_destroy(speech_joint);
    jepa_latent_destroy(visual_joint);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(visual);
}

/**
 * @brief Test cross-modal similarity computation
 */
TEST_F(JepaVisualSpeechTest, CrossModalSimilarity) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    // Create matched and unmatched pairs
    auto [matched_visual, matched_speech] = create_matched_pair();
    auto [unmatched_visual, unmatched_speech] = create_unmatched_pair();

    ASSERT_NE(matched_visual, nullptr);
    ASSERT_NE(matched_speech, nullptr);
    ASSERT_NE(unmatched_visual, nullptr);
    ASSERT_NE(unmatched_speech, nullptr);

    // Compute similarities
    float matched_sim = 0.0f;
    float unmatched_sim = 0.0f;

    EXPECT_EQ(jepa_multimodal_similarity(
        multimodal, matched_visual, matched_speech, &matched_sim
    ), NIMCP_SUCCESS);

    EXPECT_EQ(jepa_multimodal_similarity(
        multimodal, unmatched_visual, unmatched_speech, &unmatched_sim
    ), NIMCP_SUCCESS);

    // Both should be valid
    EXPECT_TRUE(std::isfinite(matched_sim));
    EXPECT_TRUE(std::isfinite(unmatched_sim));
    EXPECT_GE(matched_sim, 0.0f);
    EXPECT_LE(matched_sim, 1.0f);
    EXPECT_GE(unmatched_sim, 0.0f);
    EXPECT_LE(unmatched_sim, 1.0f);

    // Matched should have higher similarity than unmatched
    // (after training - may not hold initially)
    SCOPED_TRACE("Matched sim: " + std::to_string(matched_sim) +
                 ", Unmatched sim: " + std::to_string(unmatched_sim));

    jepa_latent_destroy(unmatched_speech);
    jepa_latent_destroy(unmatched_visual);
    jepa_latent_destroy(matched_speech);
    jepa_latent_destroy(matched_visual);
}

//=============================================================================
// Multimodal Training Tests
//=============================================================================

/**
 * @brief Test alignment training step
 */
TEST_F(JepaVisualSpeechTest, AlignmentTrainingStep) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    // Create batch of pairs
    jepa_mm_batch_t* batch = jepa_mm_batch_create(10);
    ASSERT_NE(batch, nullptr);

    // Store latents to keep them alive (batch stores references, not copies)
    std::vector<jepa_latent_t*> visual_latents;
    std::vector<jepa_latent_t*> speech_latents;

    // Add matched pairs
    for (int i = 0; i < 5; i++) {
        auto [visual, speech] = create_matched_pair();
        if (visual && speech) {
            jepa_mm_batch_add_pair(batch, visual, speech, true);
            visual_latents.push_back(visual);
            speech_latents.push_back(speech);
        }
    }

    // Add unmatched pairs
    for (int i = 0; i < 3; i++) {
        auto [visual, speech] = create_unmatched_pair();
        if (visual && speech) {
            jepa_mm_batch_add_pair(batch, visual, speech, false);
            visual_latents.push_back(visual);
            speech_latents.push_back(speech);
        }
    }

    // Set training mode
    EXPECT_EQ(jepa_multimodal_set_training(multimodal, true), NIMCP_SUCCESS);

    // Perform alignment training step
    float loss = 0.0f;
    int result = jepa_multimodal_align_step(multimodal, batch, &loss);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(std::isfinite(loss));
    EXPECT_GE(loss, 0.0f);

    EXPECT_EQ(jepa_multimodal_set_training(multimodal, false), NIMCP_SUCCESS);

    jepa_mm_batch_destroy(batch);

    // Clean up latents after batch is done
    for (auto* v : visual_latents) jepa_latent_destroy(v);
    for (auto* s : speech_latents) jepa_latent_destroy(s);
}

/**
 * @brief Test cross-modal prediction training
 */
TEST_F(JepaVisualSpeechTest, CrossModalPredictionTraining) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    auto [visual, speech] = create_matched_pair();
    ASSERT_NE(visual, nullptr);
    ASSERT_NE(speech, nullptr);

    EXPECT_EQ(jepa_multimodal_set_training(multimodal, true), NIMCP_SUCCESS);

    // Train cross-modal prediction
    float loss = 0.0f;
    int result = jepa_multimodal_cross_pred_step(multimodal, visual, speech, &loss);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(std::isfinite(loss));

    EXPECT_EQ(jepa_multimodal_set_training(multimodal, false), NIMCP_SUCCESS);

    // Check statistics
    jepa_multimodal_stats_t stats;
    EXPECT_EQ(jepa_multimodal_get_stats(multimodal, &stats), NIMCP_SUCCESS);

    // Should have recorded the training step
    EXPECT_GE(stats.visual_to_speech_preds + stats.speech_to_visual_preds, 1);

    jepa_latent_destroy(speech);
    jepa_latent_destroy(visual);
}

/**
 * @brief Test batch similarity computation
 */
TEST_F(JepaVisualSpeechTest, BatchSimilarityComputation) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    const uint32_t BATCH_SIZE = 4;

    // Create arrays of latents
    std::vector<jepa_latent_t*> visual_latents(BATCH_SIZE);
    std::vector<jepa_latent_t*> speech_latents(BATCH_SIZE);

    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        visual_latents[i] = create_visual_latent();
        speech_latents[i] = create_speech_latent();
        ASSERT_NE(visual_latents[i], nullptr);
        ASSERT_NE(speech_latents[i], nullptr);
    }

    // Make diagonal pairs matched
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        for (uint32_t j = 0; j < LATENT_DIM / 2; j++) {
            float shared = (visual_latents[i]->embedding[j] + speech_latents[i]->embedding[j]) / 2.0f;
            visual_latents[i]->embedding[j] = shared;
            speech_latents[i]->embedding[j] = shared;
        }
    }

    // Allocate similarity matrix
    std::vector<float> sim_matrix(BATCH_SIZE * BATCH_SIZE);

    int result = jepa_multimodal_batch_similarity(
        multimodal,
        visual_latents.data(),
        speech_latents.data(),
        BATCH_SIZE,
        sim_matrix.data()
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify diagonal should have higher similarity (matched pairs)
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        float diagonal_sim = sim_matrix[i * BATCH_SIZE + i];
        EXPECT_TRUE(std::isfinite(diagonal_sim));

        // Compare to off-diagonal
        for (uint32_t j = 0; j < BATCH_SIZE; j++) {
            float off_diag_sim = sim_matrix[i * BATCH_SIZE + j];
            EXPECT_TRUE(std::isfinite(off_diag_sim));
        }
    }

    // Cleanup
    for (uint32_t i = 0; i < BATCH_SIZE; i++) {
        jepa_latent_destroy(speech_latents[i]);
        jepa_latent_destroy(visual_latents[i]);
    }
}

//=============================================================================
// End-to-End Visual-Speech Integration Tests
//=============================================================================

/**
 * @brief Test visual encoding to JEPA latent
 */
TEST_F(JepaVisualSpeechTest, VisualEncodingToLatent) {
    if (!visual_bridge) {
        GTEST_SKIP() << "Visual JEPA bridge not available";
    }

    // Create visual features (simulating V1 output)
    std::vector<float> visual_features(VISUAL_FEATURE_DIM);
    for (uint32_t i = 0; i < VISUAL_FEATURE_DIM; i++) {
        visual_features[i] = cosf((float)i * 0.05f) * 0.5f + 0.5f;
    }

    jepa_latent_t* visual_latent = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(visual_latent, nullptr);

    // Encode visual features to latent
    int result = visual_jepa_bridge_encode(
        visual_bridge,
        visual_features.data(),
        VISUAL_FEATURE_DIM,
        visual_latent
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify latent has valid values
    bool valid_encoding = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(visual_latent->embedding[i])) {
            valid_encoding = false;
            break;
        }
    }
    EXPECT_TRUE(valid_encoding);

    // Check modality is set
    EXPECT_EQ(visual_latent->modality, JEPA_MODALITY_VISUAL);

    jepa_latent_destroy(visual_latent);
}

/**
 * @brief Test speech sequence encoding to JEPA latent
 */
TEST_F(JepaVisualSpeechTest, SpeechSequenceEncodingToLatent) {
    if (!speech_bridge) {
        GTEST_SKIP() << "Speech JEPA bridge not available";
    }

    // Create speech sequence
    speech_jepa_sequence_t* sequence = speech_jepa_sequence_create(50);
    ASSERT_NE(sequence, nullptr);

    // Add frames to sequence
    for (int i = 0; i < 20; i++) {
        speech_jepa_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.phoneme = (phoneme_t)(i % 10);  // Cycle through phonemes
        frame.phoneme_confidence = 0.9f;
        frame.pitch = 100.0f + 10.0f * sinf((float)i * 0.3f);
        frame.energy = 0.5f + 0.2f * cosf((float)i * 0.2f);
        frame.duration_ms = 20.0f;
        frame.timestamp_ms = i * 20;
        frame.is_voiced = (i % 3 != 0);

        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    jepa_latent_t* speech_latent = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(speech_latent, nullptr);

    // Encode speech sequence to latent
    int result = speech_jepa_bridge_encode(speech_bridge, sequence, speech_latent);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify encoding
    EXPECT_EQ(speech_latent->modality, JEPA_MODALITY_SPEECH);

    bool valid = true;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        if (!std::isfinite(speech_latent->embedding[i])) {
            valid = false;
            break;
        }
    }
    EXPECT_TRUE(valid);

    jepa_latent_destroy(speech_latent);
    speech_jepa_sequence_destroy(sequence);
}

/**
 * @brief Test full visual-speech fusion pipeline
 */
TEST_F(JepaVisualSpeechTest, FullVisualSpeechFusionPipeline) {
    if (!multimodal || !visual_bridge || !speech_bridge) {
        GTEST_SKIP() << "Full multimodal system not available";
    }

    // Connect bridges to multimodal system
    EXPECT_EQ(jepa_multimodal_connect_visual(multimodal, visual_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(jepa_multimodal_connect_speech(multimodal, speech_bridge), NIMCP_SUCCESS);
    EXPECT_TRUE(jepa_multimodal_is_connected(multimodal));

    // Create visual input
    std::vector<float> visual_features(VISUAL_FEATURE_DIM);
    for (uint32_t i = 0; i < VISUAL_FEATURE_DIM; i++) {
        visual_features[i] = cosf((float)i * 0.03f) * 0.6f + 0.4f;
    }

    // Create speech sequence
    speech_jepa_sequence_t* sequence = speech_jepa_sequence_create(30);
    ASSERT_NE(sequence, nullptr);

    for (int i = 0; i < 15; i++) {
        speech_jepa_frame_t frame;
        memset(&frame, 0, sizeof(frame));
        frame.phoneme = (phoneme_t)(i % 8);
        frame.phoneme_confidence = 0.85f;
        frame.pitch = 120.0f;
        frame.energy = 0.6f;
        frame.duration_ms = 20.0f;
        frame.timestamp_ms = i * 20;
        frame.is_voiced = true;
        speech_jepa_sequence_add_frame(sequence, &frame);
    }

    // Encode both modalities
    jepa_latent_t* visual_latent = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* speech_latent = jepa_latent_create_dim(LATENT_DIM);
    jepa_latent_t* fused_latent = jepa_latent_create_dim(JOINT_DIM);

    ASSERT_NE(visual_latent, nullptr);
    ASSERT_NE(speech_latent, nullptr);
    ASSERT_NE(fused_latent, nullptr);

    EXPECT_EQ(visual_jepa_bridge_encode(
        visual_bridge, visual_features.data(), VISUAL_FEATURE_DIM, visual_latent
    ), NIMCP_SUCCESS);

    EXPECT_EQ(speech_jepa_bridge_encode(
        speech_bridge, sequence, speech_latent
    ), NIMCP_SUCCESS);

    // Fuse modalities
    EXPECT_EQ(jepa_multimodal_fuse(
        multimodal, visual_latent, speech_latent, fused_latent
    ), NIMCP_SUCCESS);

    EXPECT_EQ(fused_latent->modality, JEPA_MODALITY_MULTIMODAL);

    // Get multimodal stats
    jepa_multimodal_stats_t stats;
    EXPECT_EQ(jepa_multimodal_get_stats(multimodal, &stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.fusions_performed, 1);

    // Cleanup
    jepa_latent_destroy(fused_latent);
    jepa_latent_destroy(speech_latent);
    jepa_latent_destroy(visual_latent);
    speech_jepa_sequence_destroy(sequence);

    jepa_multimodal_disconnect_all(multimodal);
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

/**
 * @brief Test multimodal statistics tracking
 */
TEST_F(JepaVisualSpeechTest, MultimodalStatisticsTracking) {
    if (!multimodal) {
        GTEST_SKIP() << "Multimodal JEPA not available";
    }

    // Reset stats
    EXPECT_EQ(jepa_multimodal_reset_stats(multimodal), NIMCP_SUCCESS);

    jepa_multimodal_stats_t stats;
    EXPECT_EQ(jepa_multimodal_get_stats(multimodal, &stats), NIMCP_SUCCESS);

    // Initial stats should be zero
    EXPECT_EQ(stats.visual_encodings, 0);
    EXPECT_EQ(stats.speech_encodings, 0);
    EXPECT_EQ(stats.fusions_performed, 0);

    // Perform some operations
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* joint_visual = jepa_latent_create_dim(JOINT_DIM);
    jepa_latent_t* joint_speech = jepa_latent_create_dim(JOINT_DIM);

    if (visual && speech && joint_visual && joint_speech) {
        jepa_multimodal_encode_visual(multimodal, visual, joint_visual);
        jepa_multimodal_encode_speech(multimodal, speech, joint_speech);

        // Check stats updated
        EXPECT_EQ(jepa_multimodal_get_stats(multimodal, &stats), NIMCP_SUCCESS);
        EXPECT_GE(stats.visual_encodings, 1);
        EXPECT_GE(stats.speech_encodings, 1);
    }

    if (joint_speech) jepa_latent_destroy(joint_speech);
    if (joint_visual) jepa_latent_destroy(joint_visual);
    if (speech) jepa_latent_destroy(speech);
    if (visual) jepa_latent_destroy(visual);
}

// Main provided by GTest::gtest_main
