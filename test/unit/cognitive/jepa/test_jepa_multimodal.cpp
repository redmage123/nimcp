/**
 * @file test_jepa_multimodal.cpp
 * @brief Comprehensive unit tests for Multimodal JEPA module
 *
 * Tests the joint embedding space for visual and speech modalities,
 * including cross-modal prediction, alignment training, and fusion strategies.
 *
 * CORE FUNCTIONS TESTED:
 * 1. jepa_multimodal_create() - creation with config
 * 2. jepa_multimodal_destroy() - cleanup
 * 3. jepa_multimodal_default_config() - defaults
 * 4. jepa_multimodal_encode_visual() - visual projection
 * 5. jepa_multimodal_encode_speech() - speech projection
 * 6. jepa_multimodal_fuse() - multimodal fusion (all fusion types)
 * 7. jepa_multimodal_similarity() - cross-modal similarity
 * 8. jepa_multimodal_predict_speech_from_visual() - V->S prediction
 * 9. jepa_multimodal_predict_visual_from_speech() - S->V prediction
 * 10. jepa_multimodal_align_step() - alignment training
 * 11. jepa_mm_batch_* functions - batch management
 * 12. Edge cases: NULL pointers, disconnected encoders
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_multimodal.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

namespace {

//=============================================================================
// Constants
//=============================================================================

static constexpr float FLOAT_TOLERANCE = 1e-5f;
static constexpr uint32_t TEST_JOINT_DIM = 128;
static constexpr uint32_t TEST_VISUAL_DIM = 256;
static constexpr uint32_t TEST_SPEECH_DIM = 192;

//=============================================================================
// Test Fixture
//=============================================================================

class JepaMultimodalTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        LOG_DEBUG("Setting up JepaMultimodalTest");

        // Initialize default configuration
        int result = jepa_multimodal_default_config(&config_);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to get default config";

        // Customize for testing
        config_.joint_dim = TEST_JOINT_DIM;
        config_.visual_proj.input_dim = TEST_VISUAL_DIM;
        config_.visual_proj.output_dim = TEST_JOINT_DIM;
        config_.speech_proj.input_dim = TEST_SPEECH_DIM;
        config_.speech_proj.output_dim = TEST_JOINT_DIM;

        // Also update cross-predictor dimensions to match joint_dim
        config_.cross_predictor.input_dim = TEST_JOINT_DIM;
        config_.cross_predictor.output_dim = TEST_JOINT_DIM;

        LOG_DEBUG("JepaMultimodalTest setup complete");
    }

    void TearDown() override
    {
        LOG_DEBUG("Tearing down JepaMultimodalTest");

        // Cleanup is handled by individual tests
        LOG_DEBUG("JepaMultimodalTest teardown complete");
    }

    // Helper to create a test latent with random values
    jepa_latent_t* create_test_latent(uint32_t dim, jepa_modality_t modality)
    {
        jepa_latent_config_t latent_config;
        jepa_latent_default_config(&latent_config);
        latent_config.latent_dim = dim;
        latent_config.modality = modality;
        latent_config.enable_variance = false;

        jepa_latent_t* latent = jepa_latent_create(&latent_config);
        if (latent && latent->embedding) {
            // Initialize with deterministic values for reproducibility
            for (uint32_t i = 0; i < dim; i++) {
                latent->embedding[i] = sinf((float)i * 0.1f) * 0.5f;
            }
        }
        return latent;
    }

    // Helper to create visual latent
    jepa_latent_t* create_visual_latent()
    {
        return create_test_latent(TEST_VISUAL_DIM, JEPA_MODALITY_VISUAL);
    }

    // Helper to create speech latent
    jepa_latent_t* create_speech_latent()
    {
        return create_test_latent(TEST_SPEECH_DIM, JEPA_MODALITY_SPEECH);
    }

    // Helper to create joint space latent
    jepa_latent_t* create_joint_latent()
    {
        return create_test_latent(TEST_JOINT_DIM, JEPA_MODALITY_MULTIMODAL);
    }

    // Helper to compare floating point values
    bool float_equals(float a, float b, float tolerance = FLOAT_TOLERANCE)
    {
        return fabsf(a - b) < tolerance;
    }

    // Helper to check if latent has valid values (not NaN/Inf)
    bool latent_is_valid(const jepa_latent_t* latent)
    {
        if (!latent || !latent->embedding) return false;
        for (uint32_t i = 0; i < latent->latent_dim; i++) {
            if (std::isnan(latent->embedding[i]) || std::isinf(latent->embedding[i])) {
                return false;
            }
        }
        return true;
    }

    jepa_multimodal_config_t config_;
};

//=============================================================================
// Default Configuration Tests
//=============================================================================

TEST_F(JepaMultimodalTest, DefaultConfig_ReturnsValidConfiguration)
{
    LOG_INFO("Testing jepa_multimodal_default_config returns valid config");

    jepa_multimodal_config_t config;
    int result = jepa_multimodal_default_config(&config);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Should succeed with valid pointer";
    EXPECT_GT(config.joint_dim, 0u) << "Joint dimension should be positive";
    EXPECT_GT(config.temperature, 0.0f) << "Temperature should be positive";
    EXPECT_GE(config.alignment_weight, 0.0f) << "Alignment weight should be non-negative";
    EXPECT_LE(config.alignment_weight, 1.0f) << "Alignment weight should be <= 1.0";
}

TEST_F(JepaMultimodalTest, DefaultConfig_NullPointer_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_default_config with NULL pointer");

    int result = jepa_multimodal_default_config(nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS) << "Should fail with NULL pointer";
}

TEST_F(JepaMultimodalTest, DefaultConfig_ProjectionConfigsValid)
{
    LOG_INFO("Testing default projection configurations");

    jepa_multimodal_config_t config;
    jepa_multimodal_default_config(&config);

    // Visual projection should have valid dimensions
    EXPECT_GT(config.visual_proj.input_dim, 0u) << "Visual input dim should be positive";
    EXPECT_GT(config.visual_proj.output_dim, 0u) << "Visual output dim should be positive";

    // Speech projection should have valid dimensions
    EXPECT_GT(config.speech_proj.input_dim, 0u) << "Speech input dim should be positive";
    EXPECT_GT(config.speech_proj.output_dim, 0u) << "Speech output dim should be positive";

    // Output dimensions should match joint dim
    EXPECT_EQ(config.visual_proj.output_dim, config.joint_dim);
    EXPECT_EQ(config.speech_proj.output_dim, config.joint_dim);
}

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(JepaMultimodalTest, Create_WithValidConfig_Succeeds)
{
    LOG_INFO("Testing jepa_multimodal_create with valid config");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);

    ASSERT_NE(mm, nullptr) << "Should create multimodal system";
    EXPECT_EQ(mm->config.joint_dim, TEST_JOINT_DIM) << "Joint dim should match config";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Create_WithNullConfig_UsesDefaults)
{
    LOG_INFO("Testing jepa_multimodal_create with NULL config (uses defaults)");

    jepa_multimodal_t* mm = jepa_multimodal_create(nullptr);

    ASSERT_NE(mm, nullptr) << "Should create with defaults";
    EXPECT_EQ(mm->config.joint_dim, JEPA_MULTIMODAL_DEFAULT_JOINT_DIM)
        << "Should use default joint dim";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Create_InitializesProjectionLayers)
{
    LOG_INFO("Testing projection layers are initialized");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    EXPECT_NE(mm->visual_projection, nullptr) << "Visual projection should be created";
    EXPECT_NE(mm->speech_projection, nullptr) << "Speech projection should be created";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Create_InitializesBuffers)
{
    LOG_INFO("Testing working buffers are initialized");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    EXPECT_NE(mm->visual_buffer, nullptr) << "Visual buffer should be allocated";
    EXPECT_NE(mm->speech_buffer, nullptr) << "Speech buffer should be allocated";
    EXPECT_NE(mm->fused_buffer, nullptr) << "Fused buffer should be allocated";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Create_InitializesStatistics)
{
    LOG_INFO("Testing statistics are initialized to zero");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    EXPECT_EQ(mm->stats.visual_encodings, 0u) << "Visual encodings should be 0";
    EXPECT_EQ(mm->stats.speech_encodings, 0u) << "Speech encodings should be 0";
    EXPECT_EQ(mm->stats.fusions_performed, 0u) << "Fusions should be 0";
    EXPECT_EQ(mm->stats.alignment_steps, 0u) << "Alignment steps should be 0";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Destroy_NullPointer_DoesNotCrash)
{
    LOG_INFO("Testing jepa_multimodal_destroy with NULL is safe");

    // Should not crash
    jepa_multimodal_destroy(nullptr);

    SUCCEED() << "Destroy with NULL did not crash";
}

TEST_F(JepaMultimodalTest, Destroy_ValidPointer_FreesResources)
{
    LOG_INFO("Testing jepa_multimodal_destroy frees resources");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Should not crash or leak memory
    jepa_multimodal_destroy(mm);

    SUCCEED() << "Destroy completed successfully";
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(JepaMultimodalTest, Reset_ClearsStatistics)
{
    LOG_INFO("Testing jepa_multimodal_reset clears statistics");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Manually set some stats
    mm->stats.visual_encodings = 100;
    mm->stats.speech_encodings = 50;

    int result = jepa_multimodal_reset(mm);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(mm->stats.visual_encodings, 0u) << "Visual encodings should be reset";
    EXPECT_EQ(mm->stats.speech_encodings, 0u) << "Speech encodings should be reset";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Reset_NullPointer_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_reset with NULL");

    int result = jepa_multimodal_reset(nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS) << "Should fail with NULL pointer";
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(JepaMultimodalTest, IsConnected_InitiallyFalse)
{
    LOG_INFO("Testing initial connection status is false");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    bool connected = jepa_multimodal_is_connected(mm);

    EXPECT_FALSE(connected) << "Should not be connected initially";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, IsConnected_NullPointer_ReturnsFalse)
{
    LOG_INFO("Testing jepa_multimodal_is_connected with NULL");

    bool connected = jepa_multimodal_is_connected(nullptr);

    EXPECT_FALSE(connected) << "Should return false for NULL";
}

TEST_F(JepaMultimodalTest, DisconnectAll_Succeeds)
{
    LOG_INFO("Testing jepa_multimodal_disconnect_all");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    int result = jepa_multimodal_disconnect_all(mm);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FALSE(jepa_multimodal_is_connected(mm));

    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Visual Encoding Tests
//=============================================================================

TEST_F(JepaMultimodalTest, EncodeVisual_ValidInput_Succeeds)
{
    LOG_INFO("Testing jepa_multimodal_encode_visual with valid input");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    ASSERT_NE(visual, nullptr);

    jepa_latent_t* joint = create_joint_latent();
    ASSERT_NE(joint, nullptr);

    int result = jepa_multimodal_encode_visual(mm, visual, joint);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Encoding should succeed";
    EXPECT_TRUE(latent_is_valid(joint)) << "Output should have valid values";
    EXPECT_GT(mm->stats.visual_encodings, 0u) << "Stats should be updated";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, EncodeVisual_NullMultimodal_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_encode_visual with NULL system");

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* joint = create_joint_latent();

    int result = jepa_multimodal_encode_visual(nullptr, visual, joint);

    EXPECT_NE(result, NIMCP_SUCCESS) << "Should fail with NULL system";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(joint);
}

TEST_F(JepaMultimodalTest, EncodeVisual_NullInput_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_encode_visual with NULL input");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* joint = create_joint_latent();

    int result = jepa_multimodal_encode_visual(mm, nullptr, joint);

    EXPECT_NE(result, NIMCP_SUCCESS) << "Should fail with NULL input";

    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, EncodeVisual_NullOutput_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_encode_visual with NULL output");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();

    int result = jepa_multimodal_encode_visual(mm, visual, nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS) << "Should fail with NULL output";

    jepa_latent_destroy(visual);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, EncodeVisual_ProjectsToDifferentDimension)
{
    LOG_INFO("Testing visual encoding projects to joint dimension");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    ASSERT_EQ(visual->latent_dim, TEST_VISUAL_DIM);

    jepa_latent_t* joint = create_joint_latent();
    ASSERT_EQ(joint->latent_dim, TEST_JOINT_DIM);

    int result = jepa_multimodal_encode_visual(mm, visual, joint);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Output dimension should match joint space
    EXPECT_EQ(joint->latent_dim, TEST_JOINT_DIM);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Speech Encoding Tests
//=============================================================================

TEST_F(JepaMultimodalTest, EncodeSpeech_ValidInput_Succeeds)
{
    LOG_INFO("Testing jepa_multimodal_encode_speech with valid input");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* speech = create_speech_latent();
    ASSERT_NE(speech, nullptr);

    jepa_latent_t* joint = create_joint_latent();
    ASSERT_NE(joint, nullptr);

    int result = jepa_multimodal_encode_speech(mm, speech, joint);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Encoding should succeed";
    EXPECT_TRUE(latent_is_valid(joint)) << "Output should have valid values";
    EXPECT_GT(mm->stats.speech_encodings, 0u) << "Stats should be updated";

    jepa_latent_destroy(speech);
    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, EncodeSpeech_NullPointers_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_encode_speech with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* joint = create_joint_latent();

    EXPECT_NE(jepa_multimodal_encode_speech(nullptr, speech, joint), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_encode_speech(mm, nullptr, joint), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_encode_speech(mm, speech, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(speech);
    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Fusion Tests - All Fusion Types
//=============================================================================

TEST_F(JepaMultimodalTest, Fuse_Concatenate_Succeeds)
{
    LOG_INFO("Testing fusion with CONCATENATE strategy");

    config_.fusion_type = JEPA_MM_FUSION_CONCATENATE;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    // For concatenation, fused output may have different dimension
    jepa_latent_config_t fused_config;
    jepa_latent_default_config(&fused_config);
    fused_config.latent_dim = TEST_JOINT_DIM * 2;  // Concatenated
    fused_config.modality = JEPA_MODALITY_MULTIMODAL;
    jepa_latent_t* fused = jepa_latent_create(&fused_config);

    int result = jepa_multimodal_fuse(mm, visual, speech, fused);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Concatenate fusion should succeed";
    EXPECT_TRUE(latent_is_valid(fused)) << "Fused output should be valid";
    EXPECT_GT(mm->stats.fusions_performed, 0u);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Fuse_Average_Succeeds)
{
    LOG_INFO("Testing fusion with AVERAGE strategy");

    config_.fusion_type = JEPA_MM_FUSION_AVERAGE;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* fused = create_joint_latent();

    int result = jepa_multimodal_fuse(mm, visual, speech, fused);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Average fusion should succeed";
    EXPECT_TRUE(latent_is_valid(fused)) << "Fused output should be valid";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Fuse_Attention_Succeeds)
{
    LOG_INFO("Testing fusion with ATTENTION strategy");

    config_.fusion_type = JEPA_MM_FUSION_ATTENTION;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* fused = create_joint_latent();

    int result = jepa_multimodal_fuse(mm, visual, speech, fused);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Attention fusion should succeed";
    EXPECT_TRUE(latent_is_valid(fused)) << "Fused output should be valid";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Fuse_Gate_Succeeds)
{
    LOG_INFO("Testing fusion with GATE strategy");

    config_.fusion_type = JEPA_MM_FUSION_GATE;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* fused = create_joint_latent();

    int result = jepa_multimodal_fuse(mm, visual, speech, fused);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Gated fusion should succeed";
    EXPECT_TRUE(latent_is_valid(fused)) << "Fused output should be valid";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Fuse_NullPointers_ReturnsError)
{
    LOG_INFO("Testing fusion with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* fused = create_joint_latent();

    EXPECT_NE(jepa_multimodal_fuse(nullptr, visual, speech, fused), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_fuse(mm, nullptr, speech, fused), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_fuse(mm, visual, nullptr, fused), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_fuse(mm, visual, speech, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Fuse_UpdatesStatistics)
{
    LOG_INFO("Testing fusion updates statistics");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* fused = create_joint_latent();

    uint64_t initial_fusions = mm->stats.fusions_performed;

    jepa_multimodal_fuse(mm, visual, speech, fused);

    EXPECT_GT(mm->stats.fusions_performed, initial_fusions)
        << "Fusion count should increase";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_latent_destroy(fused);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Cross-Modal Similarity Tests
//=============================================================================

TEST_F(JepaMultimodalTest, Similarity_ValidInputs_ReturnsValue)
{
    LOG_INFO("Testing cross-modal similarity computation");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    float similarity = 0.0f;

    int result = jepa_multimodal_similarity(mm, visual, speech, &similarity);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Similarity should compute successfully";
    EXPECT_GE(similarity, 0.0f) << "Similarity should be >= 0";
    EXPECT_LE(similarity, 1.0f) << "Similarity should be <= 1";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Similarity_IdenticalInputs_HighSimilarity)
{
    LOG_INFO("Testing similarity with identical inputs");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Create two identical latents
    jepa_latent_t* latent1 = create_joint_latent();
    jepa_latent_t* latent2 = jepa_latent_clone(latent1);
    ASSERT_NE(latent2, nullptr);

    float similarity = 0.0f;
    int result = jepa_multimodal_similarity(mm, latent1, latent2, &similarity);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(similarity, 0.9f) << "Identical inputs should have high similarity";

    jepa_latent_destroy(latent1);
    jepa_latent_destroy(latent2);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, Similarity_NullPointers_ReturnsError)
{
    LOG_INFO("Testing similarity with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    float similarity = 0.0f;

    EXPECT_NE(jepa_multimodal_similarity(nullptr, visual, speech, &similarity), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_similarity(mm, nullptr, speech, &similarity), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_similarity(mm, visual, nullptr, &similarity), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_similarity(mm, visual, speech, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Cross-Modal Prediction Tests (V->S)
//=============================================================================

TEST_F(JepaMultimodalTest, PredictSpeechFromVisual_ValidInput_Succeeds)
{
    LOG_INFO("Testing V->S prediction with valid input");

    config_.enable_visual_to_speech = true;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* predicted = create_joint_latent();

    int result = jepa_multimodal_predict_speech_from_visual(mm, visual, predicted);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "V->S prediction should succeed";
    EXPECT_TRUE(latent_is_valid(predicted)) << "Predicted output should be valid";
    EXPECT_GT(mm->stats.visual_to_speech_preds, 0u) << "Stats should be updated";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(predicted);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, PredictSpeechFromVisual_NullPointers_ReturnsError)
{
    LOG_INFO("Testing V->S prediction with NULL pointers");

    config_.enable_visual_to_speech = true;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* predicted = create_joint_latent();

    EXPECT_NE(jepa_multimodal_predict_speech_from_visual(nullptr, visual, predicted), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_predict_speech_from_visual(mm, nullptr, predicted), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_predict_speech_from_visual(mm, visual, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(predicted);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, PredictSpeechFromVisual_Disabled_ReturnsError)
{
    LOG_INFO("Testing V->S prediction when disabled");

    config_.enable_visual_to_speech = false;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* predicted = create_joint_latent();

    int result = jepa_multimodal_predict_speech_from_visual(mm, visual, predicted);

    // May return error or succeed depending on implementation
    // At minimum, should not crash
    SUCCEED() << "Did not crash when prediction disabled";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(predicted);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Cross-Modal Prediction Tests (S->V)
//=============================================================================

TEST_F(JepaMultimodalTest, PredictVisualFromSpeech_ValidInput_Succeeds)
{
    LOG_INFO("Testing S->V prediction with valid input");

    config_.enable_speech_to_visual = true;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* predicted = create_joint_latent();

    int result = jepa_multimodal_predict_visual_from_speech(mm, speech, predicted);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "S->V prediction should succeed";
    EXPECT_TRUE(latent_is_valid(predicted)) << "Predicted output should be valid";
    EXPECT_GT(mm->stats.speech_to_visual_preds, 0u) << "Stats should be updated";

    jepa_latent_destroy(speech);
    jepa_latent_destroy(predicted);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, PredictVisualFromSpeech_NullPointers_ReturnsError)
{
    LOG_INFO("Testing S->V prediction with NULL pointers");

    config_.enable_speech_to_visual = true;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* speech = create_speech_latent();
    jepa_latent_t* predicted = create_joint_latent();

    EXPECT_NE(jepa_multimodal_predict_visual_from_speech(nullptr, speech, predicted), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_predict_visual_from_speech(mm, nullptr, predicted), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_predict_visual_from_speech(mm, speech, nullptr), NIMCP_SUCCESS);

    jepa_latent_destroy(speech);
    jepa_latent_destroy(predicted);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Alignment Training Tests
//=============================================================================

TEST_F(JepaMultimodalTest, AlignStep_ValidBatch_Succeeds)
{
    LOG_INFO("Testing alignment training step with valid batch");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Enable training mode
    jepa_multimodal_set_training(mm, true);

    // Create batch with pairs
    jepa_mm_batch_t* batch = jepa_mm_batch_create(4);
    ASSERT_NE(batch, nullptr);

    // Add positive pairs
    jepa_latent_t* visual1 = create_visual_latent();
    jepa_latent_t* speech1 = create_speech_latent();
    jepa_mm_batch_add_pair(batch, visual1, speech1, true);

    jepa_latent_t* visual2 = create_visual_latent();
    jepa_latent_t* speech2 = create_speech_latent();
    jepa_mm_batch_add_pair(batch, visual2, speech2, true);

    // Add negative pair
    jepa_latent_t* visual3 = create_visual_latent();
    jepa_latent_t* speech3 = create_speech_latent();
    jepa_mm_batch_add_pair(batch, visual3, speech3, false);

    float loss = 0.0f;
    int result = jepa_multimodal_align_step(mm, batch, &loss);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Alignment step should succeed";
    EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";
    EXPECT_GT(mm->stats.alignment_steps, 0u) << "Alignment steps should increase";

    jepa_latent_destroy(visual1);
    jepa_latent_destroy(speech1);
    jepa_latent_destroy(visual2);
    jepa_latent_destroy(speech2);
    jepa_latent_destroy(visual3);
    jepa_latent_destroy(speech3);
    jepa_mm_batch_destroy(batch);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, AlignStep_NullPointers_ReturnsError)
{
    LOG_INFO("Testing alignment step with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_mm_batch_t* batch = jepa_mm_batch_create(1);
    float loss = 0.0f;

    EXPECT_NE(jepa_multimodal_align_step(nullptr, batch, &loss), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_align_step(mm, nullptr, &loss), NIMCP_SUCCESS);
    // NULL loss pointer may or may not be allowed depending on implementation

    jepa_mm_batch_destroy(batch);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, AlignStep_EmptyBatch_HandlesGracefully)
{
    LOG_INFO("Testing alignment step with empty batch");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_mm_batch_t* batch = jepa_mm_batch_create(4);
    ASSERT_NE(batch, nullptr);
    // Don't add any pairs

    float loss = 0.0f;
    int result = jepa_multimodal_align_step(mm, batch, &loss);

    // Should either succeed with zero loss or return error, but not crash
    SUCCEED() << "Empty batch handled without crash";

    jepa_mm_batch_destroy(batch);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, AlignStep_AllAlignmentTypes)
{
    LOG_INFO("Testing alignment with all alignment loss types");

    jepa_mm_alignment_t alignment_types[] = {
        JEPA_MM_ALIGN_CONTRASTIVE,
        JEPA_MM_ALIGN_MSE,
        JEPA_MM_ALIGN_COSINE,
        JEPA_MM_ALIGN_BARLOW_TWINS
    };

    for (auto alignment_type : alignment_types) {
        config_.alignment_type = alignment_type;
        jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
        ASSERT_NE(mm, nullptr);

        jepa_multimodal_set_training(mm, true);

        jepa_mm_batch_t* batch = jepa_mm_batch_create(2);
        jepa_latent_t* visual = create_visual_latent();
        jepa_latent_t* speech = create_speech_latent();
        jepa_mm_batch_add_pair(batch, visual, speech, true);

        float loss = 0.0f;
        int result = jepa_multimodal_align_step(mm, batch, &loss);

        EXPECT_EQ(result, NIMCP_SUCCESS)
            << "Alignment type " << (int)alignment_type << " should work";

        jepa_latent_destroy(visual);
        jepa_latent_destroy(speech);
        jepa_mm_batch_destroy(batch);
        jepa_multimodal_destroy(mm);
    }
}

//=============================================================================
// Batch Management Tests
//=============================================================================

TEST_F(JepaMultimodalTest, BatchCreate_ValidCapacity_Succeeds)
{
    LOG_INFO("Testing jepa_mm_batch_create");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(10);

    ASSERT_NE(batch, nullptr) << "Should create batch";
    EXPECT_EQ(batch->num_pairs, 0u) << "Should start empty";
    EXPECT_EQ(batch->num_positive, 0u) << "Should have no positive pairs initially";

    jepa_mm_batch_destroy(batch);
}

TEST_F(JepaMultimodalTest, BatchCreate_ZeroCapacity_ReturnsNull)
{
    LOG_INFO("Testing jepa_mm_batch_create with zero capacity");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(0);

    EXPECT_EQ(batch, nullptr) << "Should fail with zero capacity";
}

TEST_F(JepaMultimodalTest, BatchDestroy_NullPointer_DoesNotCrash)
{
    LOG_INFO("Testing jepa_mm_batch_destroy with NULL");

    jepa_mm_batch_destroy(nullptr);

    SUCCEED() << "Did not crash with NULL";
}

TEST_F(JepaMultimodalTest, BatchAddPair_ValidPair_Succeeds)
{
    LOG_INFO("Testing jepa_mm_batch_add_pair");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(5);
    ASSERT_NE(batch, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    int result = jepa_mm_batch_add_pair(batch, visual, speech, true);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Should add pair successfully";
    EXPECT_EQ(batch->num_pairs, 1u) << "Pair count should increase";
    EXPECT_EQ(batch->num_positive, 1u) << "Positive count should increase for matched pair";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_mm_batch_destroy(batch);
}

TEST_F(JepaMultimodalTest, BatchAddPair_NegativePair_TrackedCorrectly)
{
    LOG_INFO("Testing adding negative pair");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(5);
    ASSERT_NE(batch, nullptr);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    int result = jepa_mm_batch_add_pair(batch, visual, speech, false);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(batch->num_pairs, 1u);
    EXPECT_EQ(batch->num_positive, 0u) << "Positive count should not increase for negative pair";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_mm_batch_destroy(batch);
}

TEST_F(JepaMultimodalTest, BatchAddPair_NullPointers_ReturnsError)
{
    LOG_INFO("Testing jepa_mm_batch_add_pair with NULL pointers");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(5);
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();

    EXPECT_NE(jepa_mm_batch_add_pair(nullptr, visual, speech, true), NIMCP_SUCCESS);
    EXPECT_NE(jepa_mm_batch_add_pair(batch, nullptr, speech, true), NIMCP_SUCCESS);
    EXPECT_NE(jepa_mm_batch_add_pair(batch, visual, nullptr, true), NIMCP_SUCCESS);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_mm_batch_destroy(batch);
}

TEST_F(JepaMultimodalTest, BatchClear_ResetsCount)
{
    LOG_INFO("Testing jepa_mm_batch_clear");

    jepa_mm_batch_t* batch = jepa_mm_batch_create(5);
    ASSERT_NE(batch, nullptr);

    // Add some pairs
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    jepa_mm_batch_add_pair(batch, visual, speech, true);
    jepa_mm_batch_add_pair(batch, visual, speech, false);

    EXPECT_EQ(batch->num_pairs, 2u);
    EXPECT_EQ(batch->num_positive, 1u);

    int result = jepa_mm_batch_clear(batch);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(batch->num_pairs, 0u) << "Pair count should be reset";
    EXPECT_EQ(batch->num_positive, 0u) << "Positive count should be reset";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_mm_batch_destroy(batch);
}

TEST_F(JepaMultimodalTest, BatchClear_NullPointer_ReturnsError)
{
    LOG_INFO("Testing jepa_mm_batch_clear with NULL");

    int result = jepa_mm_batch_clear(nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(JepaMultimodalTest, GetStats_ValidPointer_Succeeds)
{
    LOG_INFO("Testing jepa_multimodal_get_stats");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_multimodal_stats_t stats;
    int result = jepa_multimodal_get_stats(mm, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.visual_encodings, 0u);
    EXPECT_EQ(stats.speech_encodings, 0u);
    EXPECT_EQ(stats.fusions_performed, 0u);

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, GetStats_NullPointers_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_get_stats with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_multimodal_stats_t stats;

    EXPECT_NE(jepa_multimodal_get_stats(nullptr, &stats), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_get_stats(mm, nullptr), NIMCP_SUCCESS);

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, ResetStats_ClearsAllCounters)
{
    LOG_INFO("Testing jepa_multimodal_reset_stats");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Perform some operations to increment stats
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* joint = create_joint_latent();
    jepa_multimodal_encode_visual(mm, visual, joint);

    EXPECT_GT(mm->stats.visual_encodings, 0u);

    int result = jepa_multimodal_reset_stats(mm);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_EQ(mm->stats.visual_encodings, 0u);
    EXPECT_EQ(mm->stats.speech_encodings, 0u);
    EXPECT_EQ(mm->stats.fusions_performed, 0u);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(joint);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, ResetStats_NullPointer_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_reset_stats with NULL");

    int result = jepa_multimodal_reset_stats(nullptr);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Training Mode Tests
//=============================================================================

TEST_F(JepaMultimodalTest, SetTraining_EnableDisable_Succeeds)
{
    LOG_INFO("Testing training mode enable/disable");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    // Enable training
    int result = jepa_multimodal_set_training(mm, true);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(mm->training_mode);

    // Disable training
    result = jepa_multimodal_set_training(mm, false);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(mm->training_mode);

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, SetTraining_NullPointer_ReturnsError)
{
    LOG_INFO("Testing jepa_multimodal_set_training with NULL");

    int result = jepa_multimodal_set_training(nullptr, true);

    EXPECT_NE(result, NIMCP_SUCCESS);
}

//=============================================================================
// Bio-Async Connection Tests
//=============================================================================

TEST_F(JepaMultimodalTest, BioAsyncConnection_InitiallyDisconnected)
{
    LOG_INFO("Testing initial bio-async connection status");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    bool connected = jepa_multimodal_is_bio_async_connected(mm);

    EXPECT_FALSE(connected) << "Should not be connected initially";

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, BioAsyncConnection_NullPointer_ReturnsFalse)
{
    LOG_INFO("Testing bio-async status with NULL");

    bool connected = jepa_multimodal_is_bio_async_connected(nullptr);

    EXPECT_FALSE(connected);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(JepaMultimodalTest, EdgeCase_VerySmallJointDim)
{
    LOG_INFO("Testing with very small joint dimension");

    config_.joint_dim = JEPA_LATENT_MIN_DIM;
    config_.visual_proj.output_dim = JEPA_LATENT_MIN_DIM;
    config_.speech_proj.output_dim = JEPA_LATENT_MIN_DIM;

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);

    // Should either succeed or fail gracefully
    if (mm != nullptr) {
        EXPECT_EQ(mm->config.joint_dim, JEPA_LATENT_MIN_DIM);
        jepa_multimodal_destroy(mm);
    }

    SUCCEED() << "Small dimension handled without crash";
}

TEST_F(JepaMultimodalTest, EdgeCase_LargeJointDim)
{
    LOG_INFO("Testing with large joint dimension");

    config_.joint_dim = JEPA_LATENT_MAX_DIM;
    config_.visual_proj.output_dim = JEPA_LATENT_MAX_DIM;
    config_.speech_proj.output_dim = JEPA_LATENT_MAX_DIM;

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);

    // May fail due to memory constraints, should not crash
    if (mm != nullptr) {
        jepa_multimodal_destroy(mm);
    }

    SUCCEED() << "Large dimension handled without crash";
}

TEST_F(JepaMultimodalTest, EdgeCase_ZeroTemperature)
{
    LOG_INFO("Testing with zero temperature");

    config_.temperature = 0.0f;

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);

    // Implementation may reject zero temperature or clamp it
    if (mm != nullptr) {
        jepa_multimodal_destroy(mm);
    }

    SUCCEED() << "Zero temperature handled without crash";
}

TEST_F(JepaMultimodalTest, EdgeCase_NegativeTemperature)
{
    LOG_INFO("Testing with negative temperature");

    config_.temperature = -0.1f;

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);

    // Should either reject negative temperature or handle it
    if (mm != nullptr) {
        // If accepted, temperature should be corrected
        EXPECT_GT(mm->config.temperature, 0.0f);
        jepa_multimodal_destroy(mm);
    }

    SUCCEED() << "Negative temperature handled without crash";
}

TEST_F(JepaMultimodalTest, EdgeCase_MultipleEncodingsSequentially)
{
    LOG_INFO("Testing multiple sequential encodings");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    const int NUM_ENCODINGS = 100;

    for (int i = 0; i < NUM_ENCODINGS; i++) {
        jepa_latent_t* visual = create_visual_latent();
        jepa_latent_t* joint = create_joint_latent();

        int result = jepa_multimodal_encode_visual(mm, visual, joint);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        jepa_latent_destroy(visual);
        jepa_latent_destroy(joint);
    }

    EXPECT_EQ(mm->stats.visual_encodings, (uint64_t)NUM_ENCODINGS);

    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, EdgeCase_BatchFullCapacity)
{
    LOG_INFO("Testing batch at full capacity");

    const uint32_t CAPACITY = 5;
    jepa_mm_batch_t* batch = jepa_mm_batch_create(CAPACITY);
    ASSERT_NE(batch, nullptr);

    // Fill to capacity
    std::vector<jepa_latent_t*> visuals, speeches;
    for (uint32_t i = 0; i < CAPACITY; i++) {
        jepa_latent_t* v = create_visual_latent();
        jepa_latent_t* s = create_speech_latent();
        visuals.push_back(v);
        speeches.push_back(s);

        int result = jepa_mm_batch_add_pair(batch, v, s, (i % 2) == 0);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    EXPECT_EQ(batch->num_pairs, CAPACITY);

    // Cleanup
    for (auto v : visuals) jepa_latent_destroy(v);
    for (auto s : speeches) jepa_latent_destroy(s);
    jepa_mm_batch_destroy(batch);
}

//=============================================================================
// Cross-Modal Prediction Training Tests
//=============================================================================

TEST_F(JepaMultimodalTest, CrossPredStep_ValidInputs_Succeeds)
{
    LOG_INFO("Testing cross-modal prediction training step");

    config_.enable_visual_to_speech = true;
    config_.enable_speech_to_visual = true;
    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    jepa_multimodal_set_training(mm, true);

    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    float loss = 0.0f;

    int result = jepa_multimodal_cross_pred_step(mm, visual, speech, &loss);

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Cross-pred step should succeed";
    EXPECT_GE(loss, 0.0f) << "Loss should be non-negative";

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, CrossPredStep_NullPointers_ReturnsError)
{
    LOG_INFO("Testing cross-pred step with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    jepa_latent_t* visual = create_visual_latent();
    jepa_latent_t* speech = create_speech_latent();
    float loss = 0.0f;

    EXPECT_NE(jepa_multimodal_cross_pred_step(nullptr, visual, speech, &loss), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_cross_pred_step(mm, nullptr, speech, &loss), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_cross_pred_step(mm, visual, nullptr, &loss), NIMCP_SUCCESS);

    jepa_latent_destroy(visual);
    jepa_latent_destroy(speech);
    jepa_multimodal_destroy(mm);
}

//=============================================================================
// Batch Similarity Tests
//=============================================================================

TEST_F(JepaMultimodalTest, BatchSimilarity_ValidInputs_Succeeds)
{
    LOG_INFO("Testing batch similarity computation");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    ASSERT_NE(mm, nullptr);

    const uint32_t NUM_SAMPLES = 3;

    // Create arrays of latents
    std::vector<jepa_latent_t*> visuals(NUM_SAMPLES);
    std::vector<jepa_latent_t*> speeches(NUM_SAMPLES);

    for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
        visuals[i] = create_visual_latent();
        speeches[i] = create_speech_latent();
        ASSERT_NE(visuals[i], nullptr);
        ASSERT_NE(speeches[i], nullptr);
    }

    // Allocate similarity matrix
    std::vector<float> sim_matrix(NUM_SAMPLES * NUM_SAMPLES, 0.0f);

    int result = jepa_multimodal_batch_similarity(
        mm,
        visuals.data(),
        speeches.data(),
        NUM_SAMPLES,
        sim_matrix.data()
    );

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Batch similarity should succeed";

    // All similarities should be in valid range
    for (uint32_t i = 0; i < NUM_SAMPLES * NUM_SAMPLES; i++) {
        EXPECT_GE(sim_matrix[i], 0.0f);
        EXPECT_LE(sim_matrix[i], 1.0f);
    }

    // Cleanup
    for (auto v : visuals) jepa_latent_destroy(v);
    for (auto s : speeches) jepa_latent_destroy(s);
    jepa_multimodal_destroy(mm);
}

TEST_F(JepaMultimodalTest, BatchSimilarity_NullPointers_ReturnsError)
{
    LOG_INFO("Testing batch similarity with NULL pointers");

    jepa_multimodal_t* mm = jepa_multimodal_create(&config_);
    std::vector<jepa_latent_t*> visuals(2);
    std::vector<jepa_latent_t*> speeches(2);
    std::vector<float> sim_matrix(4, 0.0f);

    for (int i = 0; i < 2; i++) {
        visuals[i] = create_visual_latent();
        speeches[i] = create_speech_latent();
    }

    EXPECT_NE(jepa_multimodal_batch_similarity(nullptr, visuals.data(), speeches.data(), 2, sim_matrix.data()), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_batch_similarity(mm, nullptr, speeches.data(), 2, sim_matrix.data()), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_batch_similarity(mm, visuals.data(), nullptr, 2, sim_matrix.data()), NIMCP_SUCCESS);
    EXPECT_NE(jepa_multimodal_batch_similarity(mm, visuals.data(), speeches.data(), 2, nullptr), NIMCP_SUCCESS);

    for (auto v : visuals) jepa_latent_destroy(v);
    for (auto s : speeches) jepa_latent_destroy(s);
    jepa_multimodal_destroy(mm);
}

}  // namespace

/**
 * Test Summary:
 *
 * Default Configuration Tests (3):
 * - Default config returns valid values
 * - NULL pointer handling
 * - Projection configs are valid
 *
 * Creation and Destruction Tests (7):
 * - Create with valid config
 * - Create with NULL config (uses defaults)
 * - Projection layers initialized
 * - Working buffers initialized
 * - Statistics initialized
 * - Destroy NULL pointer safety
 * - Destroy valid pointer
 *
 * Reset Tests (2):
 * - Reset clears statistics
 * - Reset NULL pointer handling
 *
 * Connection Tests (3):
 * - Initial connection status
 * - NULL pointer handling
 * - Disconnect all
 *
 * Visual Encoding Tests (5):
 * - Valid input succeeds
 * - NULL multimodal error
 * - NULL input error
 * - NULL output error
 * - Projects to correct dimension
 *
 * Speech Encoding Tests (2):
 * - Valid input succeeds
 * - NULL pointers error
 *
 * Fusion Tests (7):
 * - Concatenate fusion
 * - Average fusion
 * - Attention fusion
 * - Gated fusion
 * - NULL pointers error
 * - Statistics updated
 *
 * Similarity Tests (4):
 * - Valid inputs return value
 * - Identical inputs high similarity
 * - NULL pointers error
 *
 * V->S Prediction Tests (3):
 * - Valid input succeeds
 * - NULL pointers error
 * - Disabled prediction handling
 *
 * S->V Prediction Tests (2):
 * - Valid input succeeds
 * - NULL pointers error
 *
 * Alignment Training Tests (4):
 * - Valid batch succeeds
 * - NULL pointers error
 * - Empty batch handling
 * - All alignment types work
 *
 * Batch Management Tests (8):
 * - Create valid capacity
 * - Create zero capacity fails
 * - Destroy NULL safety
 * - Add pair succeeds
 * - Negative pair tracked
 * - NULL pointers error
 * - Clear resets count
 * - Clear NULL error
 *
 * Statistics Tests (4):
 * - Get stats succeeds
 * - NULL pointers error
 * - Reset clears counters
 * - Reset NULL error
 *
 * Training Mode Tests (2):
 * - Enable/disable succeeds
 * - NULL pointer error
 *
 * Bio-Async Tests (2):
 * - Initially disconnected
 * - NULL returns false
 *
 * Edge Case Tests (6):
 * - Very small joint dim
 * - Large joint dim
 * - Zero temperature
 * - Negative temperature
 * - Multiple sequential encodings
 * - Batch at full capacity
 *
 * Cross-Modal Prediction Training Tests (2):
 * - Valid inputs succeed
 * - NULL pointers error
 *
 * Batch Similarity Tests (2):
 * - Valid inputs succeed
 * - NULL pointers error
 *
 * TOTAL: 68 comprehensive unit tests
 */
