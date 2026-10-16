/**
 * @file test_cnn_cortex_bridge.cpp
 * @brief Unit tests for CNN-Cortex Bridge
 *
 * WHAT: Comprehensive unit tests for CNN-cortex integration
 * WHY:  Ensure bridge correctly connects CNN trainer with visual/audio cortexes
 * HOW:  Test lifecycle, connections, feature extraction, gradient feedback
 *
 * Test Categories:
 * - Lifecycle (create, destroy, default config)
 * - Visual cortex connection
 * - Audio cortex connection
 * - Feature extraction (visual and audio)
 * - Training state retrieval
 * - Gradient feedback propagation
 * - LR modulation
 * - Error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_cortex_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/tensor/nimcp_tensor.h"

// Forward declaration for tests that would use cnn_trainer_t
// (not compiled until nimcp_cnn_training.c enum conflicts resolved)
struct cnn_trainer_s;
typedef struct cnn_trainer_s cnn_trainer_t;

//=============================================================================
// Test Fixture
//=============================================================================

class CNNCortexBridgeTest : public ::testing::Test {
protected:
    cnn_cortex_bridge_t* bridge = nullptr;
    cnn_cortex_bridge_config_t config;
    visual_cortex_t* visual_cortex = nullptr;
    audio_cortex_t* audio_cortex = nullptr;

    void SetUp() override {
        // Get default config
        cnn_cortex_bridge_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            cnn_cortex_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (visual_cortex) {
            visual_cortex_destroy(visual_cortex);
            visual_cortex = nullptr;
        }
        if (audio_cortex) {
            audio_cortex_destroy(audio_cortex);
            audio_cortex = nullptr;
        }
    }

    void CreateVisualCortex() {
        visual_cortex_config_t vc_config;
        memset(&vc_config, 0, sizeof(vc_config));
        vc_config.input_width = 64;
        vc_config.input_height = 64;
        vc_config.num_v1_filters = 8;
        vc_config.feature_dim = 32;
        vc_config.enable_attention = true;
        vc_config.enable_memory = true;
        vc_config.enable_fractal_topology = false;
        vc_config.hub_ratio = 0.15f;
        vc_config.power_law_gamma = -2.1f;
        vc_config.internal_neurons = 80;
        vc_config.enable_bio_async = false;
        vc_config.enable_second_messengers = false;
        visual_cortex = visual_cortex_create(&vc_config);
    }

    void CreateAudioCortex() {
        audio_cortex_config_t ac_config;
        memset(&ac_config, 0, sizeof(ac_config));
        ac_config.sample_rate = 16000;
        ac_config.frame_size = 512;
        ac_config.num_freq_bins = 256;
        ac_config.num_mel_filters = 40;
        ac_config.num_mfcc = 13;
        ac_config.num_channels = 1;
        ac_config.feature_dim = 53;  // 40 mel + 13 mfcc
        ac_config.enable_attention = true;
        ac_config.enable_memory = true;
        ac_config.enable_fractal_topology = false;
        ac_config.hub_ratio = 0.15f;
        ac_config.power_law_gamma = -2.1f;
        ac_config.internal_neurons = 400;
        ac_config.enable_bio_async = false;
        ac_config.enable_second_messengers = false;
        audio_cortex = audio_cortex_create(&ac_config);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, DefaultConfigHasReasonableValues) {
    EXPECT_TRUE(config.freeze_cortex_weights);
    EXPECT_FALSE(config.enable_gradient_feedback);
    EXPECT_GT(config.gradient_feedback_scale, 0.0f);
    EXPECT_LE(config.gradient_feedback_scale, 1.0f);
    EXPECT_TRUE(config.enable_perception_modulation);
}

TEST_F(CNNCortexBridgeTest, CreateWithDefaultConfig) {
    bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(CNNCortexBridgeTest, CreateWithNullConfigUsesDefaults) {
    // NULL config should use defaults, not fail
    bridge = cnn_cortex_bridge_create(nullptr);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(CNNCortexBridgeTest, DestroyNullIsNoOp) {
    // Should not crash
    cnn_cortex_bridge_destroy(nullptr);
}

TEST_F(CNNCortexBridgeTest, CreateWithGradientFeedbackEnabled) {
    config.enable_gradient_feedback = true;
    config.gradient_feedback_scale = 0.5f;
    bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(CNNCortexBridgeTest, CreateWithPerceptionModulationDisabled) {
    config.enable_perception_modulation = false;
    bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
}

//=============================================================================
// Trainer Connection Tests - DISABLED
// NOTE: These tests are disabled until nimcp_cnn_training.c enum conflicts
// are resolved and cnn_trainer_t is available
//=============================================================================

//=============================================================================
// Visual Cortex Connection Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, ConnectVisualCortexSuccess) {
    bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    CreateVisualCortex();
    ASSERT_NE(visual_cortex, nullptr);

    int result = cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);
    EXPECT_EQ(result, 0);
}

TEST_F(CNNCortexBridgeTest, ConnectVisualNullBridgeFails) {
    CreateVisualCortex();
    int result = cnn_cortex_bridge_connect_visual_cortex(nullptr, visual_cortex);
    // NIMCP uses positive error codes
    EXPECT_NE(result, 0);
}

TEST_F(CNNCortexBridgeTest, ConnectVisualNullCortexDisconnects) {
    // NULL cortex should disconnect, not fail
    bridge = cnn_cortex_bridge_create(&config);
    int result = cnn_cortex_bridge_connect_visual_cortex(bridge, nullptr);
    EXPECT_EQ(result, 0);  // Success - disconnection is valid
}

TEST_F(CNNCortexBridgeTest, VisualCortexEnablesTrainingMode) {
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();

    EXPECT_FALSE(visual_cortex_is_training_mode(visual_cortex));

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    EXPECT_TRUE(visual_cortex_is_training_mode(visual_cortex));
}

//=============================================================================
// Audio Cortex Connection Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, ConnectAudioCortexSuccess) {
    bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    CreateAudioCortex();
    ASSERT_NE(audio_cortex, nullptr);

    int result = cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);
    EXPECT_EQ(result, 0);
}

TEST_F(CNNCortexBridgeTest, ConnectAudioNullBridgeFails) {
    CreateAudioCortex();
    int result = cnn_cortex_bridge_connect_audio_cortex(nullptr, audio_cortex);
    // NIMCP uses positive error codes
    EXPECT_NE(result, 0);
}

TEST_F(CNNCortexBridgeTest, ConnectAudioNullCortexDisconnects) {
    // NULL cortex should disconnect, not fail
    bridge = cnn_cortex_bridge_create(&config);
    int result = cnn_cortex_bridge_connect_audio_cortex(bridge, nullptr);
    EXPECT_EQ(result, 0);  // Success - disconnection is valid
}

TEST_F(CNNCortexBridgeTest, AudioCortexEnablesTrainingMode) {
    bridge = cnn_cortex_bridge_create(&config);
    CreateAudioCortex();

    EXPECT_FALSE(audio_cortex_is_training_mode(audio_cortex));

    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    EXPECT_TRUE(audio_cortex_is_training_mode(audio_cortex));
}

//=============================================================================
// Visual Cortex Training Interface Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, VisualCortexGetFeatureDim) {
    CreateVisualCortex();
    uint32_t dim = visual_cortex_get_feature_dim(visual_cortex);
    EXPECT_EQ(dim, 32u);  // As set in CreateVisualCortex
}

TEST_F(CNNCortexBridgeTest, VisualCortexGetFeatureDimNullReturnsZero) {
    uint32_t dim = visual_cortex_get_feature_dim(nullptr);
    EXPECT_EQ(dim, 0u);
}

TEST_F(CNNCortexBridgeTest, VisualCortexSetTrainingMode) {
    CreateVisualCortex();

    EXPECT_FALSE(visual_cortex_is_training_mode(visual_cortex));

    int result = visual_cortex_set_training_mode(visual_cortex, true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(visual_cortex_is_training_mode(visual_cortex));

    result = visual_cortex_set_training_mode(visual_cortex, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(visual_cortex_is_training_mode(visual_cortex));
}

TEST_F(CNNCortexBridgeTest, VisualCortexSetTrainingModeNullFails) {
    int result = visual_cortex_set_training_mode(nullptr, true);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGetTrainingState) {
    CreateVisualCortex();
    visual_cortex_set_training_mode(visual_cortex, true);

    visual_training_state_t state;
    int result = visual_cortex_get_training_state(visual_cortex, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGetTrainingStateNullCortexFails) {
    visual_training_state_t state;
    int result = visual_cortex_get_training_state(nullptr, &state);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGetTrainingStateNullStateFails) {
    CreateVisualCortex();
    int result = visual_cortex_get_training_state(visual_cortex, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Audio Cortex Training Interface Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, AudioCortexGetFeatureDim) {
    CreateAudioCortex();
    uint32_t dim = audio_cortex_get_feature_dim(audio_cortex);
    EXPECT_EQ(dim, 53u);  // 40 mel + 13 mfcc
}

TEST_F(CNNCortexBridgeTest, AudioCortexGetFeatureDimNullReturnsZero) {
    uint32_t dim = audio_cortex_get_feature_dim(nullptr);
    EXPECT_EQ(dim, 0u);
}

TEST_F(CNNCortexBridgeTest, AudioCortexSetTrainingMode) {
    CreateAudioCortex();

    EXPECT_FALSE(audio_cortex_is_training_mode(audio_cortex));

    int result = audio_cortex_set_training_mode(audio_cortex, true);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(audio_cortex_is_training_mode(audio_cortex));

    result = audio_cortex_set_training_mode(audio_cortex, false);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(audio_cortex_is_training_mode(audio_cortex));
}

TEST_F(CNNCortexBridgeTest, AudioCortexSetTrainingModeNullFails) {
    int result = audio_cortex_set_training_mode(nullptr, true);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, AudioCortexGetTrainingState) {
    CreateAudioCortex();
    audio_cortex_set_training_mode(audio_cortex, true);

    audio_training_state_t state;
    int result = audio_cortex_get_training_state(audio_cortex, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(CNNCortexBridgeTest, AudioCortexGetTrainingStateNullCortexFails) {
    audio_training_state_t state;
    int result = audio_cortex_get_training_state(nullptr, &state);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, AudioCortexGetTrainingStateNullStateFails) {
    CreateAudioCortex();
    int result = audio_cortex_get_training_state(audio_cortex, nullptr);
    EXPECT_LT(result, 0);
}

//=============================================================================
// Gradient Feedback Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, VisualCortexGradientFeedbackRequiresTrainingMode) {
    CreateVisualCortex();

    float gradients[32];
    memset(gradients, 0, sizeof(gradients));
    gradients[0] = 0.1f;

    // Not in training mode - should fail
    int result = visual_cortex_apply_gradient_feedback(visual_cortex, gradients, 32, 1.0f);
    EXPECT_LT(result, 0);

    // Enable training mode
    visual_cortex_set_training_mode(visual_cortex, true);

    // Now should succeed
    result = visual_cortex_apply_gradient_feedback(visual_cortex, gradients, 32, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGradientFeedbackNullCortexFails) {
    float gradients[32] = {0.1f};
    int result = visual_cortex_apply_gradient_feedback(nullptr, gradients, 32, 1.0f);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGradientFeedbackNullGradientsFails) {
    CreateVisualCortex();
    visual_cortex_set_training_mode(visual_cortex, true);
    int result = visual_cortex_apply_gradient_feedback(visual_cortex, nullptr, 32, 1.0f);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGradientFeedbackZeroSizeFails) {
    CreateVisualCortex();
    visual_cortex_set_training_mode(visual_cortex, true);
    float gradients[32] = {0.1f};
    int result = visual_cortex_apply_gradient_feedback(visual_cortex, gradients, 0, 1.0f);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexGradientFeedbackInvalidScaleFails) {
    CreateVisualCortex();
    visual_cortex_set_training_mode(visual_cortex, true);
    float gradients[32] = {0.1f};

    // Negative scale
    int result = visual_cortex_apply_gradient_feedback(visual_cortex, gradients, 32, -1.0f);
    EXPECT_LT(result, 0);

    // Scale too large
    result = visual_cortex_apply_gradient_feedback(visual_cortex, gradients, 32, 100.0f);
    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, AudioCortexGradientFeedbackRequiresTrainingMode) {
    CreateAudioCortex();

    float gradients[53];
    memset(gradients, 0, sizeof(gradients));
    gradients[0] = 0.1f;

    // Not in training mode - should fail
    int result = audio_cortex_apply_gradient_feedback(audio_cortex, gradients, 53, 1.0f);
    EXPECT_LT(result, 0);

    // Enable training mode
    audio_cortex_set_training_mode(audio_cortex, true);

    // Now should succeed
    result = audio_cortex_apply_gradient_feedback(audio_cortex, gradients, 53, 1.0f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Feature Extraction Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, VisualCortexExtractFeaturesTensor) {
    CreateVisualCortex();
    visual_cortex_set_training_mode(visual_cortex, true);

    // Create a simple test image (64x64, 3 channels)
    std::vector<uint8_t> image(64 * 64 * 3, 128);

    nimcp_tensor_t* features = nullptr;
    int result = visual_cortex_extract_features_tensor(
        visual_cortex, image.data(), 64, 64, 3,
        (struct nimcp_tensor**)&features);

    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    // Check tensor dimensions
    EXPECT_EQ(nimcp_tensor_rank(features), 1u);
    EXPECT_EQ(nimcp_tensor_shape(features)->dims[0], 32u);  // feature_dim

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexBridgeTest, VisualCortexExtractFeaturesNullCortexFails) {
    std::vector<uint8_t> image(64 * 64 * 3, 128);
    nimcp_tensor_t* features = nullptr;

    int result = visual_cortex_extract_features_tensor(
        nullptr, image.data(), 64, 64, 3,
        (struct nimcp_tensor**)&features);

    EXPECT_LT(result, 0);
    EXPECT_EQ(features, nullptr);
}

TEST_F(CNNCortexBridgeTest, VisualCortexExtractFeaturesNullImageFails) {
    CreateVisualCortex();
    nimcp_tensor_t* features = nullptr;

    int result = visual_cortex_extract_features_tensor(
        visual_cortex, nullptr, 64, 64, 3,
        (struct nimcp_tensor**)&features);

    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, VisualCortexExtractFeaturesNullOutputFails) {
    CreateVisualCortex();
    std::vector<uint8_t> image(64 * 64 * 3, 128);

    int result = visual_cortex_extract_features_tensor(
        visual_cortex, image.data(), 64, 64, 3, nullptr);

    EXPECT_LT(result, 0);
}

TEST_F(CNNCortexBridgeTest, AudioCortexExtractFeaturesTensor) {
    CreateAudioCortex();
    audio_cortex_set_training_mode(audio_cortex, true);

    // Create a simple test audio (512 samples at 16kHz = 32ms)
    std::vector<float> audio(512, 0.0f);
    // Add a simple tone
    for (size_t i = 0; i < audio.size(); i++) {
        audio[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / 16000.0f);
    }

    nimcp_tensor_t* features = nullptr;
    int result = audio_cortex_extract_features_tensor(
        audio_cortex, audio.data(), 512, 1,
        (struct nimcp_tensor**)&features);

    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    // Check tensor dimensions
    EXPECT_EQ(nimcp_tensor_rank(features), 1u);
    EXPECT_EQ(nimcp_tensor_shape(features)->dims[0], 53u);  // 40 mel + 13 mfcc

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexBridgeTest, AudioCortexExtractFeaturesNullCortexFails) {
    std::vector<float> audio(512, 0.0f);
    nimcp_tensor_t* features = nullptr;

    int result = audio_cortex_extract_features_tensor(
        nullptr, audio.data(), 512, 1,
        (struct nimcp_tensor**)&features);

    EXPECT_LT(result, 0);
}

//=============================================================================
// Bridge Feature Extraction Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, BridgeExtractVisualFeatures) {
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    std::vector<uint8_t> image(64 * 64 * 3, 128);
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 3, &features);

    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexBridgeTest, BridgeExtractAudioFeatures) {
    bridge = cnn_cortex_bridge_create(&config);
    CreateAudioCortex();
    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    std::vector<float> audio(512, 0.0f);
    for (size_t i = 0; i < audio.size(); i++) {
        audio[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / 16000.0f);
    }

    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_audio_features(
        bridge, audio.data(), 512, 1, &features);

    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexBridgeTest, BridgeExtractVisualWithoutConnectionFails) {
    bridge = cnn_cortex_bridge_create(&config);

    std::vector<uint8_t> image(64 * 64 * 3, 128);
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 3, &features);

    // NIMCP uses positive error codes
    EXPECT_NE(result, 0);
}

TEST_F(CNNCortexBridgeTest, BridgeExtractAudioWithoutConnectionFails) {
    bridge = cnn_cortex_bridge_create(&config);

    std::vector<float> audio(512, 0.0f);
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_audio_features(
        bridge, audio.data(), 512, 1, &features);

    // NIMCP uses positive error codes
    EXPECT_NE(result, 0);
}

//=============================================================================
// LR Modulation Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, BridgeGetModulatedLR) {
    bridge = cnn_cortex_bridge_create(&config);

    float base_lr = 0.01f;
    float modulated_lr = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

    // Without perception connection, should return base LR
    EXPECT_FLOAT_EQ(modulated_lr, base_lr);
}

TEST_F(CNNCortexBridgeTest, BridgeGetModulatedLRWithVisualCortex) {
    config.enable_perception_modulation = true;
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    float base_lr = 0.01f;
    float modulated_lr = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

    // With perception connection, LR should be modulated
    // Exact value depends on perception confidence
    EXPECT_GT(modulated_lr, 0.0f);
}

TEST_F(CNNCortexBridgeTest, BridgeGetModulatedLRNullBridge) {
    float result = cnn_cortex_bridge_get_modulated_lr(nullptr, 0.01f);
    EXPECT_EQ(result, 0.01f);  // Should return base LR
}

//=============================================================================
// Skip Sample Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, BridgeShouldSkipSampleWithoutConnection) {
    bridge = cnn_cortex_bridge_create(&config);

    // Without perception connection, should never skip
    bool should_skip = cnn_cortex_bridge_should_skip_sample(bridge);
    EXPECT_FALSE(should_skip);
}

TEST_F(CNNCortexBridgeTest, BridgeShouldSkipSampleNullBridge) {
    bool should_skip = cnn_cortex_bridge_should_skip_sample(nullptr);
    EXPECT_FALSE(should_skip);
}

//=============================================================================
// Gradient Propagation Tests
//=============================================================================

TEST_F(CNNCortexBridgeTest, BridgeSetGradients) {
    config.enable_gradient_feedback = true;
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Create gradients tensor
    uint32_t dims[1] = {32};
    nimcp_tensor_t* gradients = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(gradients, nullptr);

    float* grad_data = (float*)nimcp_tensor_data(gradients);
    for (uint32_t i = 0; i < 32; i++) {
        grad_data[i] = 0.01f * (float)i;
    }

    int result = cnn_cortex_bridge_set_gradients(bridge, gradients);
    EXPECT_EQ(result, 0);

    nimcp_tensor_destroy(gradients);
}

TEST_F(CNNCortexBridgeTest, BridgePropagateGradients) {
    config.enable_gradient_feedback = true;
    config.gradient_feedback_scale = 0.5f;
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Create gradients tensor
    uint32_t dims[1] = {32};
    nimcp_tensor_t* gradients = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    float* grad_data = (float*)nimcp_tensor_data(gradients);
    for (uint32_t i = 0; i < 32; i++) {
        grad_data[i] = 0.01f;
    }

    cnn_cortex_bridge_set_gradients(bridge, gradients);
    int result = cnn_cortex_bridge_propagate_gradients(bridge);
    EXPECT_EQ(result, 0);

    nimcp_tensor_destroy(gradients);
}

TEST_F(CNNCortexBridgeTest, BridgePropagateGradientsWithFeedbackDisabled) {
    config.enable_gradient_feedback = false;  // Disabled
    bridge = cnn_cortex_bridge_create(&config);
    CreateVisualCortex();
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Should succeed but do nothing
    int result = cnn_cortex_bridge_propagate_gradients(bridge);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// CNN Trainer Connection Tests
// NOTE: Disabled until nimcp_cnn_training.c is compiled without conflicts
//=============================================================================

// These tests require cnn_trainer_* functions which are not yet available
// Will be enabled once nimcp_cnn_training.c enum conflicts are resolved

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
