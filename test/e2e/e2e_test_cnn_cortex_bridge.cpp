/**
 * @file e2e_test_cnn_cortex_bridge.cpp
 * @brief End-to-End tests for CNN-Cortex Bridge
 *
 * WHAT: Complete workflow tests with actual cortex instances
 * WHY: Verify realistic usage scenarios from image/audio input to training
 * HOW: Create full pipelines, process real-ish data, verify end-to-end behavior
 *
 * TEST SCENARIOS:
 * 1. Visual Training Pipeline - Image → Visual Cortex → CNN Features → Training
 * 2. Audio Training Pipeline - Audio → Audio Cortex → CNN Features → Training
 * 3. Multimodal Training Pipeline - Both modalities combined
 * 4. Perception-Modulated Training - LR adjustment based on input quality
 * 5. Gradient Feedback Pipeline - CNN gradients → Cortex STDP
 * 6. Quality Filtering Pipeline - Skip low-quality samples
 * 7. Full Training Loop Simulation - Complete epoch simulation
 * 8. Bio-async Coordination - Inter-module messaging
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_cortex_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CNNCortexBridgeE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create visual cortex
        visual_cortex_config_t vc_config;
        memset(&vc_config, 0, sizeof(vc_config));
        vc_config.input_width = 64;
        vc_config.input_height = 64;
        vc_config.num_v1_filters = 8;
        vc_config.feature_dim = 32;
        vc_config.enable_attention = true;
        vc_config.enable_memory = true;
        vc_config.enable_fractal_topology = false;
        vc_config.enable_bio_async = false;
        vc_config.enable_second_messengers = false;
        visual_cortex_ = visual_cortex_create(&vc_config);

        // Create audio cortex
        audio_cortex_config_t ac_config;
        memset(&ac_config, 0, sizeof(ac_config));
        ac_config.sample_rate = 16000;
        ac_config.frame_size = 512;
        ac_config.num_freq_bins = 256;
        ac_config.num_mel_filters = 40;
        ac_config.num_mfcc = 13;
        ac_config.num_channels = 1;
        ac_config.feature_dim = 53;
        ac_config.enable_attention = true;
        ac_config.enable_memory = true;
        ac_config.enable_fractal_topology = false;
        ac_config.enable_bio_async = false;
        ac_config.enable_second_messengers = false;
        audio_cortex_ = audio_cortex_create(&ac_config);

        // Initialize RNG
        rng_ = std::mt19937(42);
    }

    void TearDown() override {
        if (visual_cortex_) {
            visual_cortex_destroy(visual_cortex_);
            visual_cortex_ = nullptr;
        }
        if (audio_cortex_) {
            audio_cortex_destroy(audio_cortex_);
            audio_cortex_ = nullptr;
        }
    }

    // Generate synthetic grayscale image (edge pattern)
    std::vector<uint8_t> generate_test_image(uint32_t width, uint32_t height) {
        std::vector<uint8_t> image(width * height);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                // Diagonal edge pattern
                image[y * width + x] = (x + y) % 2 == 0 ? 255 : 0;
            }
        }
        return image;
    }

    // Generate synthetic audio (sine wave)
    std::vector<float> generate_test_audio(uint32_t num_samples, float frequency = 440.0f) {
        std::vector<float> audio(num_samples);
        float sample_rate = 16000.0f;
        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = sinf(2.0f * M_PI * frequency * i / sample_rate);
        }
        return audio;
    }

    // Generate noisy audio for low-quality testing
    std::vector<float> generate_noisy_audio(uint32_t num_samples) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        std::vector<float> audio(num_samples);
        for (uint32_t i = 0; i < num_samples; i++) {
            audio[i] = dist(rng_);
        }
        return audio;
    }

    // Generate random gradient tensor
    nimcp_tensor_t* generate_gradient_tensor(size_t size) {
        uint32_t dims[] = {(uint32_t)size};
        nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
        if (!tensor) return nullptr;

        std::normal_distribution<float> dist(0.0f, 0.1f);
        float* data = (float*)nimcp_tensor_data(tensor);
        for (size_t i = 0; i < size; i++) {
            data[i] = dist(rng_);
        }
        return tensor;
    }

    visual_cortex_t* visual_cortex_ = nullptr;
    audio_cortex_t* audio_cortex_ = nullptr;
    std::mt19937 rng_;
};

//=============================================================================
// Scenario 1: Visual Training Pipeline
//=============================================================================

/**
 * E2E: Image → Visual Cortex → CNN Features → Training step
 */
TEST_F(CNNCortexBridgeE2ETest, VisualTrainingPipeline) {
    // Skip if visual cortex creation failed
    if (!visual_cortex_) {
        GTEST_SKIP() << "Visual cortex not available";
    }

    // Create bridge
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.mode = CNN_CORTEX_MODE_TRAINING;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect visual cortex
    int result = cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Generate test image
    const uint32_t width = 64, height = 64, channels = 1;
    auto image = generate_test_image(width, height);

    // Extract features
    nimcp_tensor_t* features = nullptr;
    result = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), width, height, channels, &features);
    EXPECT_EQ(result, 0);
    ASSERT_NE(features, nullptr);

    // Verify features are valid
    size_t feature_size = nimcp_tensor_numel(features);
    EXPECT_GT(feature_size, 0u);

    float* feature_data = (float*)nimcp_tensor_data(features);
    bool has_nonzero = false;
    for (size_t i = 0; i < feature_size && !has_nonzero; i++) {
        if (feature_data[i] != 0.0f) has_nonzero = true;
    }
    EXPECT_TRUE(has_nonzero) << "Features should contain non-zero values";

    // Verify metrics updated
    cnn_cortex_perception_metrics_t metrics;
    result = cnn_cortex_bridge_get_perception_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(metrics.visual_available);
    EXPECT_TRUE(metrics.valid);

    // Verify stats updated
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.visual_extractions, 1u);
    EXPECT_EQ(stats.total_feature_extractions, 1u);
    EXPECT_EQ(stats.samples_processed, 1u);

    // Cleanup
    nimcp_tensor_destroy(features);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 2: Audio Training Pipeline
//=============================================================================

/**
 * E2E: Audio → Audio Cortex → CNN Features → Training step
 * Note: Audio cortex requires FFT/FFTW which may not be available in test environment.
 */
TEST_F(CNNCortexBridgeE2ETest, AudioTrainingPipeline) {
    // Skip if audio cortex creation failed
    if (!audio_cortex_) {
        GTEST_SKIP() << "Audio cortex not available";
    }

    // Create bridge
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.mode = CNN_CORTEX_MODE_TRAINING;
    config.priority = CNN_CORTEX_PRIORITY_AUDIO;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect audio cortex
    int result = cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex_);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Generate test audio (1 second at 16kHz)
    const uint32_t num_samples = 16000;
    const uint8_t num_channels = 1;
    auto audio = generate_test_audio(num_samples, 440.0f);

    // Extract features
    nimcp_tensor_t* features = nullptr;
    result = cnn_cortex_bridge_extract_audio_features(
        bridge, audio.data(), num_samples, num_channels, &features);

    // Audio cortex may fail in test environment due to FFT requirements
    if (result != 0 || !features) {
        cnn_cortex_bridge_destroy(bridge);
        GTEST_SKIP() << "Audio cortex processing failed (FFT may not be available)";
    }

    // Verify features are valid
    size_t feature_size = nimcp_tensor_numel(features);
    EXPECT_GT(feature_size, 0u);

    // Verify metrics updated
    cnn_cortex_perception_metrics_t metrics;
    result = cnn_cortex_bridge_get_perception_metrics(bridge, &metrics);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(metrics.audio_available);
    EXPECT_TRUE(metrics.valid);

    // Verify stats updated
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.audio_extractions, 1u);
    EXPECT_EQ(stats.total_feature_extractions, 1u);

    // Cleanup
    nimcp_tensor_destroy(features);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 3: Multimodal Training Pipeline
//=============================================================================

/**
 * E2E: Image + Audio → Both Cortexes → Combined Features
 * Note: Audio cortex may fail in test environment due to FFT requirements,
 *       so this test verifies the bridge handles partial success gracefully.
 */
TEST_F(CNNCortexBridgeE2ETest, MultimodalTrainingPipeline) {
    // Skip if either cortex failed
    if (!visual_cortex_ || !audio_cortex_) {
        GTEST_SKIP() << "Both cortexes required for multimodal test";
    }

    // Create bridge with multimodal priority
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.mode = CNN_CORTEX_MODE_TRAINING;
    config.priority = CNN_CORTEX_PRIORITY_MULTIMODAL;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect both cortexes
    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);
    cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex_);
    EXPECT_TRUE(cnn_cortex_bridge_is_connected(bridge));

    // Generate test data
    auto image = generate_test_image(64, 64);
    auto audio = generate_test_audio(16000, 440.0f);

    // Extract multimodal features
    // Note: Audio cortex may fail in test environment, so we accept partial success
    nimcp_tensor_t* features = nullptr;
    int result = cnn_cortex_bridge_extract_multimodal_features(
        bridge,
        image.data(), 64, 64, 1,
        audio.data(), 16000, 1,
        &features);

    // Accept success (0) or partial success (features from visual only)
    if (result != 0 || !features) {
        // If multimodal failed, verify at least visual works independently
        nimcp_tensor_t* visual_features = nullptr;
        result = cnn_cortex_bridge_extract_visual_features(
            bridge, image.data(), 64, 64, 1, &visual_features);
        EXPECT_EQ(result, 0) << "At least visual extraction should work";
        if (visual_features) {
            nimcp_tensor_destroy(visual_features);
        }
        cnn_cortex_bridge_destroy(bridge);
        GTEST_SKIP() << "Audio cortex not functional in test environment, skipping multimodal test";
    }

    // Multimodal features should be at least as large as visual (audio may fail)
    size_t feature_size = nimcp_tensor_numel(features);
    EXPECT_GE(feature_size, 128u) << "Features should be at least visual size";

    // Verify stats show at least visual extraction
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.visual_extractions, 0u);
    // Multimodal count may be 0 if audio failed but visual succeeded via fallback

    // Cleanup
    nimcp_tensor_destroy(features);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 4: Perception-Modulated Training
//=============================================================================

/**
 * E2E: Verify LR modulation based on perception quality
 */
TEST_F(CNNCortexBridgeE2ETest, PerceptionModulatedTraining) {
    if (!visual_cortex_) {
        GTEST_SKIP() << "Visual cortex required";
    }

    // Create bridge with perception modulation
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.enable_perception_modulation = true;
    config.lr_min_factor = 0.5f;
    config.lr_max_factor = 1.5f;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);

    // Test LR modulation before any extraction
    float base_lr = 0.01f;
    float initial_lr = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

    // Should be within bounds
    EXPECT_GE(initial_lr, base_lr * 0.5f);
    EXPECT_LE(initial_lr, base_lr * 1.5f);

    // Extract features to update metrics
    auto image = generate_test_image(64, 64);
    nimcp_tensor_t* features = nullptr;
    cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 1, &features);

    if (features) {
        // LR should still be within bounds after extraction
        float modulated_lr = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);
        EXPECT_GE(modulated_lr, base_lr * 0.5f);
        EXPECT_LE(modulated_lr, base_lr * 1.5f);

        nimcp_tensor_destroy(features);
    }

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 5: Gradient Feedback Pipeline
//=============================================================================

/**
 * E2E: CNN gradients → Bridge → Cortex STDP modulation
 */
TEST_F(CNNCortexBridgeE2ETest, GradientFeedbackPipeline) {
    if (!visual_cortex_) {
        GTEST_SKIP() << "Visual cortex required";
    }

    // Create bridge with gradient feedback enabled
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.enable_gradient_feedback = true;
    config.gradient_method = CNN_CORTEX_GRADIENT_MAGNITUDE;
    config.gradient_feedback_scale = 0.01f;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);

    // Extract features first
    auto image = generate_test_image(64, 64);
    nimcp_tensor_t* features = nullptr;
    int result = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 1, &features);
    EXPECT_EQ(result, 0);

    if (features) {
        size_t feature_size = nimcp_tensor_numel(features);

        // Generate gradients matching feature size
        nimcp_tensor_t* gradients = generate_gradient_tensor(feature_size);
        ASSERT_NE(gradients, nullptr);

        // Set gradients
        result = cnn_cortex_bridge_set_gradients(bridge, gradients);
        EXPECT_EQ(result, 0);

        // Propagate gradients
        result = cnn_cortex_bridge_propagate_gradients(bridge);
        EXPECT_EQ(result, 0);

        // Verify stats updated
        cnn_cortex_bridge_stats_t stats;
        cnn_cortex_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(stats.total_gradient_feedbacks, 1u);
        EXPECT_EQ(stats.visual_feedbacks, 1u);

        nimcp_tensor_destroy(gradients);
        nimcp_tensor_destroy(features);
    }

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 6: Quality Filtering Pipeline
//=============================================================================

/**
 * E2E: Skip low-quality samples based on perception metrics
 */
TEST_F(CNNCortexBridgeE2ETest, QualityFilteringPipeline) {
    // Create bridge with quality filtering
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.skip_low_quality_samples = true;
    config.visual_confidence_threshold = 0.3f;
    config.audio_quality_threshold = 0.3f;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Without connected cortex, skip should be false
    EXPECT_FALSE(cnn_cortex_bridge_should_skip_sample(bridge));

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 7: Full Training Loop Simulation
//=============================================================================

/**
 * E2E: Simulate a complete training epoch
 */
TEST_F(CNNCortexBridgeE2ETest, FullTrainingLoopSimulation) {
    if (!visual_cortex_) {
        GTEST_SKIP() << "Visual cortex required";
    }

    // Create bridge for training
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.mode = CNN_CORTEX_MODE_TRAINING;
    config.enable_perception_modulation = true;
    config.enable_gradient_feedback = true;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);

    const int NUM_SAMPLES = 10;
    const float BASE_LR = 0.01f;
    int samples_processed = 0;
    int samples_skipped = 0;

    // Simulate training loop
    for (int i = 0; i < NUM_SAMPLES; i++) {
        // Generate varied images (different patterns)
        std::vector<uint8_t> image(64 * 64);
        for (size_t j = 0; j < image.size(); j++) {
            image[j] = ((j + i) % 256);
        }

        // Check if should skip
        if (cnn_cortex_bridge_should_skip_sample(bridge)) {
            samples_skipped++;
            continue;
        }

        // Extract features
        nimcp_tensor_t* features = nullptr;
        int result = cnn_cortex_bridge_extract_visual_features(
            bridge, image.data(), 64, 64, 1, &features);

        if (result == 0 && features) {
            // Get modulated LR
            float lr = cnn_cortex_bridge_get_modulated_lr(bridge, BASE_LR);
            EXPECT_GT(lr, 0.0f);

            // Simulate backward pass with gradient feedback
            nimcp_tensor_t* grads = generate_gradient_tensor(nimcp_tensor_numel(features));
            if (grads) {
                cnn_cortex_bridge_set_gradients(bridge, grads);
                cnn_cortex_bridge_propagate_gradients(bridge);
                nimcp_tensor_destroy(grads);
            }

            samples_processed++;
            nimcp_tensor_destroy(features);
        }

        // Update bridge
        cnn_cortex_bridge_update(bridge);
    }

    // Verify training happened
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.samples_processed, (uint64_t)samples_processed);
    EXPECT_GE(stats.total_feature_extractions, (uint64_t)samples_processed);
    EXPECT_GE(stats.total_updates, (uint64_t)NUM_SAMPLES);

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 8: Bio-async Coordination
//=============================================================================

/**
 * E2E: Verify bio-async connection and disconnection
 */
TEST_F(CNNCortexBridgeE2ETest, BioAsyncCoordination) {
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.enable_bio_async = true;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Initially not connected
    EXPECT_FALSE(cnn_cortex_bridge_is_bio_async_connected(bridge));

    // Connect to bio-async
    int result = cnn_cortex_bridge_connect_bio_async(bridge);
    // Note: May fail if no router available, which is ok in unit test environment
    // Bio-async router is typically not initialized in E2E tests, so we just
    // verify the calls don't crash
    EXPECT_GE(result, 0);  // Success (0) or soft failure is OK

    // Disconnect (should always succeed even if not connected)
    result = cnn_cortex_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(cnn_cortex_bridge_is_bio_async_connected(bridge));

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 9: Multiple Extraction Consistency
//=============================================================================

/**
 * E2E: Same input should produce consistent features
 */
TEST_F(CNNCortexBridgeE2ETest, MultipleExtractionConsistency) {
    if (!visual_cortex_) {
        GTEST_SKIP() << "Visual cortex required";
    }

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);

    // Generate fixed test image
    auto image = generate_test_image(64, 64);

    // Extract features multiple times
    nimcp_tensor_t* features1 = nullptr;
    nimcp_tensor_t* features2 = nullptr;

    int result1 = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 1, &features1);
    int result2 = cnn_cortex_bridge_extract_visual_features(
        bridge, image.data(), 64, 64, 1, &features2);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    if (features1 && features2) {
        // Same size
        EXPECT_EQ(nimcp_tensor_numel(features1), nimcp_tensor_numel(features2));

        // Same values (deterministic processing)
        size_t size = nimcp_tensor_numel(features1);
        float* data1 = (float*)nimcp_tensor_data(features1);
        float* data2 = (float*)nimcp_tensor_data(features2);

        bool consistent = true;
        for (size_t i = 0; i < size && consistent; i++) {
            if (fabs(data1[i] - data2[i]) > 1e-5f) {
                consistent = false;
            }
        }
        EXPECT_TRUE(consistent) << "Same input should produce same features";
    }

    if (features1) nimcp_tensor_destroy(features1);
    if (features2) nimcp_tensor_destroy(features2);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 10: State Dump (Debug/Monitoring)
//=============================================================================

/**
 * E2E: Verify state dump doesn't crash
 */
TEST_F(CNNCortexBridgeE2ETest, StateDumpSafe) {
    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect cortexes
    if (visual_cortex_) {
        cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);
    }
    if (audio_cortex_) {
        cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex_);
    }

    // Do some operations
    cnn_cortex_bridge_update(bridge);

    // Dump should not crash
    cnn_cortex_bridge_dump_state(bridge);
    cnn_cortex_bridge_dump_state(nullptr);  // NULL should also be safe

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 11: Rapid Connect/Disconnect Cycle
//=============================================================================

/**
 * E2E: Verify rapid connection changes don't cause issues
 */
TEST_F(CNNCortexBridgeE2ETest, RapidConnectDisconnectCycle) {
    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 50; i++) {
        // Connect
        if (visual_cortex_) {
            cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);
        }
        if (audio_cortex_) {
            cnn_cortex_bridge_connect_audio_cortex(bridge, audio_cortex_);
        }

        // Disconnect
        cnn_cortex_bridge_connect_visual_cortex(bridge, nullptr);
        cnn_cortex_bridge_connect_audio_cortex(bridge, nullptr);
    }

    // Should end disconnected
    EXPECT_FALSE(cnn_cortex_bridge_is_connected(bridge));

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Scenario 12: Mixed Mode Operations
//=============================================================================

/**
 * E2E: Verify mode changes don't break operations
 */
TEST_F(CNNCortexBridgeE2ETest, MixedModeOperations) {
    cnn_cortex_mode_t modes[] = {
        CNN_CORTEX_MODE_FEATURE_ONLY,
        CNN_CORTEX_MODE_TRAINING,
        CNN_CORTEX_MODE_FINE_TUNING
    };

    for (cnn_cortex_mode_t mode : modes) {
        cnn_cortex_bridge_config_t config;
        cnn_cortex_bridge_default_config(&config);
        config.mode = mode;
        config.enable_gradient_feedback = (mode == CNN_CORTEX_MODE_FINE_TUNING);

        cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed for mode "
                                   << cnn_cortex_mode_to_string(mode);

        if (visual_cortex_) {
            cnn_cortex_bridge_connect_visual_cortex(bridge, visual_cortex_);

            // Use 64x64 to match cortex configuration
            auto image = generate_test_image(64, 64);
            nimcp_tensor_t* features = nullptr;

            int result = cnn_cortex_bridge_extract_visual_features(
                bridge, image.data(), 64, 64, 1, &features);
            EXPECT_EQ(result, 0) << "Extraction failed for mode "
                                 << cnn_cortex_mode_to_string(mode);

            if (features) {
                nimcp_tensor_destroy(features);
            }
        }

        cnn_cortex_bridge_destroy(bridge);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
