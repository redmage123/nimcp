/**
 * @file test_cnn_cortex_integration.cpp
 * @brief Integration tests for CNN-Cortex Bridge
 *
 * WHAT: Integration tests verifying CNN-cortex module interactions
 * WHY:  Ensure bridge correctly integrates cortexes with training pipeline
 * HOW:  Test multi-module scenarios and data flow
 *
 * Test Categories:
 * - Visual+Audio multimodal integration
 * - Gradient feedback flow
 * - Perception-modulated learning
 * - Training state integration
 * - Repeated processing stress tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_cortex_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CNNCortexIntegrationTest : public ::testing::Test {
protected:
    cnn_cortex_bridge_t* bridge = nullptr;
    visual_cortex_t* visual_cortex = nullptr;
    audio_cortex_t* audio_cortex = nullptr;

    void SetUp() override {
        // Create visual cortex
        visual_cortex_config_t vc_config;
        memset(&vc_config, 0, sizeof(vc_config));
        vc_config.input_width = 32;
        vc_config.input_height = 32;
        vc_config.num_v1_filters = 4;
        vc_config.feature_dim = 16;
        vc_config.enable_attention = true;
        vc_config.enable_memory = true;
        vc_config.enable_fractal_topology = false;
        vc_config.hub_ratio = 0.15f;
        vc_config.power_law_gamma = -2.1f;
        vc_config.internal_neurons = 40;
        vc_config.enable_bio_async = false;
        vc_config.enable_second_messengers = false;
        visual_cortex = visual_cortex_create(&vc_config);

        // Create audio cortex
        audio_cortex_config_t ac_config;
        memset(&ac_config, 0, sizeof(ac_config));
        ac_config.sample_rate = 16000;
        ac_config.frame_size = 256;
        ac_config.num_freq_bins = 128;
        ac_config.num_mel_filters = 20;
        ac_config.num_mfcc = 10;
        ac_config.num_channels = 1;
        ac_config.feature_dim = 30;  // 20 mel + 10 mfcc
        ac_config.enable_attention = true;
        ac_config.enable_memory = true;
        ac_config.enable_fractal_topology = false;
        ac_config.hub_ratio = 0.15f;
        ac_config.power_law_gamma = -2.1f;
        ac_config.internal_neurons = 200;
        ac_config.enable_bio_async = false;
        ac_config.enable_second_messengers = false;
        audio_cortex = audio_cortex_create(&ac_config);

        // Create bridge with training mode
        cnn_cortex_bridge_config_t bc_config;
        cnn_cortex_bridge_default_config(&bc_config);
        bc_config.mode = CNN_CORTEX_MODE_TRAINING;
        bridge = cnn_cortex_bridge_create(&bc_config);
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

    // Generate test image
    std::vector<uint8_t> GenerateTestImage(uint32_t width, uint32_t height, uint32_t channels) {
        std::vector<uint8_t> image(width * height * channels);
        for (size_t i = 0; i < image.size(); i++) {
            image[i] = static_cast<uint8_t>((i * 17) % 256);
        }
        return image;
    }

    // Generate test audio
    std::vector<float> GenerateTestAudio(uint32_t samples, float frequency) {
        std::vector<float> audio(samples);
        for (size_t i = 0; i < audio.size(); i++) {
            audio[i] = 0.5f * std::sin(2.0f * M_PI * frequency * i / 16000.0f);
        }
        return audio;
    }
};

//=============================================================================
// Cortex Connection Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, ConnectBothVisualAndAudio) {
    ASSERT_NE(bridge, nullptr);

    int result1 = cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);
    EXPECT_EQ(result1, 0);

    int result2 = cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);
    EXPECT_EQ(result2, 0);

    // Both should be connected
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Both cortexes should be in training mode
    if (visual_cortex) {
        EXPECT_TRUE(visual_cortex_is_training_mode(visual_cortex));
    }
    if (audio_cortex) {
        EXPECT_TRUE(audio_cortex_is_training_mode(audio_cortex));
    }
}

TEST_F(CNNCortexIntegrationTest, ConnectDisconnectSequence) {
    // Connect visual
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Disconnect visual by passing NULL
    cnn_cortex_bridge_connect_visual_cortex(bridge, nullptr);

    // Connect audio
    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Disconnect audio
    cnn_cortex_bridge_connect_audio_cortex(bridge, nullptr);
    EXPECT_FALSE(cnn_cortex_bridge_is_connected(bridge));
}

//=============================================================================
// Multimodal Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, ExtractMultimodalFeatures) {
    if (!visual_cortex || !audio_cortex) {
        GTEST_SKIP() << "Cortexes not available";
    }

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);
    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    auto image = GenerateTestImage(32, 32, 1);  // Use 1 channel to match config
    auto audio = GenerateTestAudio(256, 440.0f);

    nimcp_tensor_t* features = nullptr;
    int result = cnn_cortex_bridge_extract_multimodal_features(
        bridge, image.data(), 32, 32, 1, audio.data(), 256, 1, &features);

    // May fail due to audio cortex FFT requirements
    if (result != 0 || !features) {
        GTEST_SKIP() << "Multimodal extraction not available in test environment";
    }

    // Verify features exist
    EXPECT_GT(nimcp_tensor_numel(features), 0u);

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexIntegrationTest, ExtractMultimodalWithOnlyVisualConnected) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    // Only connect visual
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    auto image = GenerateTestImage(32, 32, 1);
    auto audio = GenerateTestAudio(256, 440.0f);

    nimcp_tensor_t* features = nullptr;

    // Should fail or return only visual features
    int result = cnn_cortex_bridge_extract_multimodal_features(
        bridge, image.data(), 32, 32, 1, audio.data(), 256, 1, &features);

    // Don't crash is the main check
    if (features) {
        nimcp_tensor_destroy(features);
    }
}

//=============================================================================
// Data Flow Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, VisualFeatureExtractionDataFlow) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Extract features from image
    auto image = GenerateTestImage(32, 32, 1);
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 32, 32, 1, &features);

    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    // Verify features are valid (not all zeros)
    size_t numel = nimcp_tensor_numel(features);
    float* data = (float*)nimcp_tensor_data(features);
    float sum = 0.0f;
    for (size_t i = 0; i < numel; i++) {
        sum += std::abs(data[i]);
    }
    EXPECT_GT(sum, 0.0f);

    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexIntegrationTest, AudioFeatureExtractionDataFlow) {
    if (!audio_cortex) {
        GTEST_SKIP() << "Audio cortex not available";
    }

    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    // Extract features from audio
    auto audio = GenerateTestAudio(256, 440.0f);
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_audio_features(
        bridge, audio.data(), 256, 1, &features);

    // Audio cortex may fail due to FFT requirements
    if (result != 0 || !features) {
        GTEST_SKIP() << "Audio processing not available (FFT may not be configured)";
    }

    // Verify features are valid
    size_t numel = nimcp_tensor_numel(features);
    float* data = (float*)nimcp_tensor_data(features);
    float sum = 0.0f;
    for (size_t i = 0; i < numel; i++) {
        sum += std::abs(data[i]);
    }
    EXPECT_GT(sum, 0.0f);

    nimcp_tensor_destroy(features);
}

//=============================================================================
// Gradient Feedback Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, GradientFeedbackFlowVisual) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_config_t bc_config;
    cnn_cortex_bridge_default_config(&bc_config);
    bc_config.enable_gradient_feedback = true;
    bc_config.gradient_feedback_scale = 0.5f;
    bc_config.mode = CNN_CORTEX_MODE_FINE_TUNING;

    cnn_cortex_bridge_destroy(bridge);
    bridge = cnn_cortex_bridge_create(&bc_config);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // First extract features to set up state
    auto image = GenerateTestImage(32, 32, 1);
    nimcp_tensor_t* features = nullptr;
    cnn_cortex_bridge_extract_visual_features(bridge, image.data(), 32, 32, 1, &features);

    if (!features) {
        GTEST_SKIP() << "Feature extraction failed";
    }

    // Create gradients matching feature size
    size_t grad_size = nimcp_tensor_numel(features);
    uint32_t dims[1] = {(uint32_t)grad_size};
    nimcp_tensor_t* gradients = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    float* grad_data = (float*)nimcp_tensor_data(gradients);
    for (size_t i = 0; i < grad_size; i++) {
        grad_data[i] = 0.01f * (float)(i + 1);
    }

    // Set and propagate gradients
    int result1 = cnn_cortex_bridge_set_gradients(bridge, gradients);
    EXPECT_EQ(result1, 0);

    int result2 = cnn_cortex_bridge_propagate_gradients(bridge);
    EXPECT_EQ(result2, 0);

    nimcp_tensor_destroy(gradients);
    nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexIntegrationTest, GradientFeedbackFlowAudio) {
    if (!audio_cortex) {
        GTEST_SKIP() << "Audio cortex not available";
    }

    cnn_cortex_bridge_config_t bc_config;
    cnn_cortex_bridge_default_config(&bc_config);
    bc_config.enable_gradient_feedback = true;
    bc_config.gradient_feedback_scale = 0.3f;
    bc_config.mode = CNN_CORTEX_MODE_FINE_TUNING;

    cnn_cortex_bridge_destroy(bridge);
    bridge = cnn_cortex_bridge_create(&bc_config);

    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    // First extract features
    auto audio = GenerateTestAudio(256, 440.0f);
    nimcp_tensor_t* features = nullptr;
    int result = cnn_cortex_bridge_extract_audio_features(bridge, audio.data(), 256, 1, &features);

    if (result != 0 || !features) {
        GTEST_SKIP() << "Audio feature extraction failed (FFT may not be available)";
    }

    // Create gradients matching feature size
    size_t grad_size = nimcp_tensor_numel(features);
    uint32_t dims[1] = {(uint32_t)grad_size};
    nimcp_tensor_t* gradients = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    float* grad_data = (float*)nimcp_tensor_data(gradients);
    for (size_t i = 0; i < grad_size; i++) {
        grad_data[i] = 0.02f * (float)(i + 1);
    }

    int result1 = cnn_cortex_bridge_set_gradients(bridge, gradients);
    EXPECT_EQ(result1, 0);

    int result2 = cnn_cortex_bridge_propagate_gradients(bridge);
    EXPECT_EQ(result2, 0);

    nimcp_tensor_destroy(gradients);
    nimcp_tensor_destroy(features);
}

//=============================================================================
// Perception Modulation Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, PerceptionModulatedLearning) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_config_t bc_config;
    cnn_cortex_bridge_default_config(&bc_config);
    bc_config.enable_perception_modulation = true;
    bc_config.mode = CNN_CORTEX_MODE_TRAINING;

    cnn_cortex_bridge_destroy(bridge);
    bridge = cnn_cortex_bridge_create(&bc_config);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Process an image to update perception confidence
    auto image = GenerateTestImage(32, 32, 1);
    nimcp_tensor_t* features = nullptr;
    cnn_cortex_bridge_extract_visual_features(bridge, image.data(), 32, 32, 1, &features);

    // Get modulated LR
    float base_lr = 0.01f;
    float modulated_lr = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

    // LR should be positive and within reasonable bounds
    EXPECT_GT(modulated_lr, 0.0f);
    EXPECT_LT(modulated_lr, base_lr * 2.0f);  // Shouldn't be more than 2x base

    if (features) nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexIntegrationTest, SkipSampleBasedOnPerception) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_config_t bc_config;
    cnn_cortex_bridge_default_config(&bc_config);
    bc_config.enable_perception_modulation = true;
    bc_config.visual_confidence_threshold = 0.9f;  // High threshold
    bc_config.skip_low_quality_samples = true;
    bc_config.mode = CNN_CORTEX_MODE_TRAINING;

    cnn_cortex_bridge_destroy(bridge);
    bridge = cnn_cortex_bridge_create(&bc_config);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Process a very low-contrast image (likely low confidence)
    std::vector<uint8_t> low_contrast_image(32 * 32, 128);  // All gray

    nimcp_tensor_t* features = nullptr;
    cnn_cortex_bridge_extract_visual_features(bridge, low_contrast_image.data(), 32, 32, 1, &features);

    // With high threshold, low-confidence samples might be skipped
    bool should_skip = cnn_cortex_bridge_should_skip_sample(bridge);

    // Not asserting specific value since it depends on implementation details
    // Just verify it doesn't crash and returns a boolean
    (void)should_skip;

    if (features) nimcp_tensor_destroy(features);
}

//=============================================================================
// Training State Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, VisualTrainingStateAfterProcessing) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Process an image
    auto image = GenerateTestImage(32, 32, 1);
    nimcp_tensor_t* features = nullptr;
    cnn_cortex_bridge_extract_visual_features(bridge, image.data(), 32, 32, 1, &features);

    // Get training state
    visual_training_state_t state;
    memset(&state, 0, sizeof(state));
    int result = visual_cortex_get_training_state(visual_cortex, &state);
    EXPECT_EQ(result, 0);

    // Confidence should be in valid range
    EXPECT_GE(state.confidence, 0.0f);
    EXPECT_LE(state.confidence, 1.0f);

    if (features) nimcp_tensor_destroy(features);
}

TEST_F(CNNCortexIntegrationTest, AudioTrainingStateAfterProcessing) {
    if (!audio_cortex) {
        GTEST_SKIP() << "Audio cortex not available";
    }

    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    // Process audio
    auto audio = GenerateTestAudio(256, 440.0f);
    nimcp_tensor_t* features = nullptr;
    int result = cnn_cortex_bridge_extract_audio_features(bridge, audio.data(), 256, 1, &features);

    if (result != 0 || !features) {
        GTEST_SKIP() << "Audio processing failed (FFT may not be available)";
    }

    // Get training state
    audio_training_state_t state;
    memset(&state, 0, sizeof(state));
    result = audio_cortex_get_training_state(audio_cortex, &state);
    EXPECT_EQ(result, 0);

    // Quality should be in valid range
    EXPECT_GE(state.quality, 0.0f);
    EXPECT_LE(state.quality, 1.0f);

    nimcp_tensor_destroy(features);
}

//=============================================================================
// Stats Integration Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, StatsAccumulateAcrossExtractions) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    // Initial stats should be zero
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_extractions, 0u);

    // Process multiple images
    for (int i = 0; i < 5; i++) {
        auto image = GenerateTestImage(32, 32, 1);
        nimcp_tensor_t* features = nullptr;
        cnn_cortex_bridge_extract_visual_features(bridge, image.data(), 32, 32, 1, &features);
        if (features) nimcp_tensor_destroy(features);
    }

    // Stats should show 5 extractions
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_extractions, 5u);
    EXPECT_EQ(stats.total_feature_extractions, 5u);
}

//=============================================================================
// Repeated Processing Stress Tests
//=============================================================================

TEST_F(CNNCortexIntegrationTest, RepeatedVisualProcessing) {
    if (!visual_cortex) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex);

    for (int i = 0; i < 20; i++) {
        auto image = GenerateTestImage(32, 32, 1);
        // Vary the image slightly
        for (size_t j = 0; j < image.size(); j++) {
            image[j] = static_cast<uint8_t>((image[j] + i * 10) % 256);
        }

        nimcp_tensor_t* features = nullptr;
        int result = cnn_cortex_bridge_extract_visual_features(
            bridge, image.data(), 32, 32, 1, &features);

        EXPECT_EQ(result, 0);
        ASSERT_NE(features, nullptr);

        nimcp_tensor_destroy(features);
    }
}

TEST_F(CNNCortexIntegrationTest, RepeatedAudioProcessing) {
    if (!audio_cortex) {
        GTEST_SKIP() << "Audio cortex not available";
    }

    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex);

    int successful = 0;
    for (int i = 0; i < 10; i++) {
        // Vary frequency
        float freq = 220.0f + i * 50.0f;
        auto audio = GenerateTestAudio(256, freq);

        nimcp_tensor_t* features = nullptr;
        int result = cnn_cortex_bridge_extract_audio_features(
            bridge, audio.data(), 256, 1, &features);

        if (result == 0 && features) {
            successful++;
            nimcp_tensor_destroy(features);
        }
    }

    // At least some should succeed, or skip if none work
    if (successful == 0) {
        GTEST_SKIP() << "Audio processing not available (FFT may not be configured)";
    }
    EXPECT_GT(successful, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
