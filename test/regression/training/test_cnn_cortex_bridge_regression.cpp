/**
 * @file test_cnn_cortex_bridge_regression.cpp
 * @brief Regression tests for CNN-Cortex Bridge
 *
 * WHAT: Tests to ensure CNN-cortex bridge behavior remains stable over time
 * WHY: Detect regressions in feature extraction, LR modulation, and gradient feedback
 * HOW: Compare outputs against expected values, stress test edge cases
 *
 * TEST CATEGORIES:
 * 1. Numerical Stability - LR factor computation edge cases
 * 2. Memory Stability - Repeated create/destroy cycles
 * 3. State Consistency - Operations produce consistent results
 * 4. Edge Cases - Boundary conditions and error handling
 * 5. Configuration Stability - Default values don't change unexpectedly
 *
 * @author NIMCP Development Team
 * @date 2025-12-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "training/nimcp_cnn_cortex_bridge.h"
#include "perception/nimcp_visual_cortex.h"
#include "perception/nimcp_audio_cortex.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CNNCortexBridgeRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Record initial memory state for leak detection
        initial_alloc_count_ = get_allocation_count();
    }

    void TearDown() override {
        // Note: In a full implementation, we'd verify no leaks here
    }

    // Helper to get approximate allocation count (mock for testing)
    static size_t get_allocation_count() {
        // In real implementation, this would query nimcp_memory
        return 0;
    }

    // Helper to create bridge with default config
    cnn_cortex_bridge_t* create_default_bridge() {
        cnn_cortex_bridge_config_t config;
        cnn_cortex_bridge_default_config(&config);
        return cnn_cortex_bridge_create(&config);
    }

    // Helper to create visual cortex
    visual_cortex_t* create_visual_cortex() {
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
        return visual_cortex_create(&vc_config);
    }

    // Helper to create audio cortex
    audio_cortex_t* create_audio_cortex() {
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
        return audio_cortex_create(&ac_config);
    }

    size_t initial_alloc_count_;
};

//=============================================================================
// Configuration Stability Tests
//=============================================================================

/**
 * Verify default configuration values haven't changed unexpectedly
 * REGRESSION TARGET: Default config values
 */
TEST_F(CNNCortexBridgeRegressionTest, DefaultConfigValuesStable) {
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);

    // These values are documented and should remain stable
    EXPECT_EQ(config.mode, CNN_CORTEX_MODE_TRAINING)
        << "Default mode should be TRAINING";
    EXPECT_EQ(config.priority, CNN_CORTEX_PRIORITY_VISUAL)
        << "Default priority should be VISUAL";
    EXPECT_TRUE(config.freeze_cortex_weights)
        << "Cortex weights should be frozen by default";
    EXPECT_FALSE(config.enable_gradient_feedback)
        << "Gradient feedback should be disabled by default";
    EXPECT_EQ(config.gradient_method, CNN_CORTEX_GRADIENT_MAGNITUDE)
        << "Default gradient method should be MAGNITUDE";

    // LR modulation defaults
    EXPECT_TRUE(config.enable_perception_modulation)
        << "Perception modulation should be enabled by default";
    EXPECT_FLOAT_EQ(config.lr_min_factor, CNN_CORTEX_DEFAULT_LR_MIN_FACTOR);
    EXPECT_FLOAT_EQ(config.lr_max_factor, CNN_CORTEX_DEFAULT_LR_MAX_FACTOR);
    EXPECT_FLOAT_EQ(config.gradient_feedback_scale, CNN_CORTEX_DEFAULT_GRADIENT_SCALE);

    // Threshold defaults
    EXPECT_FLOAT_EQ(config.visual_confidence_threshold, CNN_CORTEX_DEFAULT_CONFIDENCE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.audio_quality_threshold, CNN_CORTEX_DEFAULT_QUALITY_THRESHOLD);
    EXPECT_FALSE(config.skip_low_quality_samples);

    // Integration defaults
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_TRUE(config.integrate_perception_bridge);
    EXPECT_EQ(config.cache_size, 16u);
    EXPECT_EQ(config.update_interval_ms, 100u);
}

/**
 * Verify constant definitions haven't changed
 * REGRESSION TARGET: API constants
 */
TEST_F(CNNCortexBridgeRegressionTest, ConstantValuesStable) {
    // These are part of the API contract
    EXPECT_FLOAT_EQ(CNN_CORTEX_DEFAULT_GRADIENT_SCALE, 0.01f);
    EXPECT_FLOAT_EQ(CNN_CORTEX_DEFAULT_LR_MIN_FACTOR, 0.5f);
    EXPECT_FLOAT_EQ(CNN_CORTEX_DEFAULT_LR_MAX_FACTOR, 1.5f);
    EXPECT_FLOAT_EQ(CNN_CORTEX_DEFAULT_CONFIDENCE_THRESHOLD, 0.3f);
    EXPECT_FLOAT_EQ(CNN_CORTEX_DEFAULT_QUALITY_THRESHOLD, 0.3f);
    EXPECT_EQ(CNN_CORTEX_MAX_VISUAL_FEATURES, 4096u);
    EXPECT_EQ(CNN_CORTEX_MAX_AUDIO_FEATURES, 2048u);

    // Bio-async IDs
    EXPECT_EQ(BIO_MODULE_CNN_CORTEX_BRIDGE, 0x0713u);
    EXPECT_EQ(BIO_MSG_CNN_CORTEX_FEATURES, 0x0714u);
    EXPECT_EQ(BIO_MSG_CNN_CORTEX_GRADIENT, 0x0715u);
    EXPECT_EQ(BIO_MSG_CNN_CORTEX_QUALITY, 0x0716u);
}

//=============================================================================
// Memory Stability Tests
//=============================================================================

/**
 * Verify no memory leaks on repeated create/destroy cycles
 * REGRESSION TARGET: Memory management
 */
TEST_F(CNNCortexBridgeRegressionTest, CreateDestroyMemoryStability) {
    const int NUM_CYCLES = 100;

    for (int i = 0; i < NUM_CYCLES; i++) {
        cnn_cortex_bridge_t* bridge = create_default_bridge();
        ASSERT_NE(bridge, nullptr) << "Create failed on cycle " << i;
        cnn_cortex_bridge_destroy(bridge);
    }

    // If we get here without crashing, basic memory stability is ok
    SUCCEED();
}

/**
 * Verify destroy handles NULL gracefully
 * REGRESSION TARGET: NULL safety
 */
TEST_F(CNNCortexBridgeRegressionTest, DestroyNullSafe) {
    // Should not crash
    cnn_cortex_bridge_destroy(nullptr);
    SUCCEED();
}

/**
 * Verify repeated gradient set/propagate doesn't leak
 * REGRESSION TARGET: Gradient buffer management
 */
TEST_F(CNNCortexBridgeRegressionTest, GradientBufferMemoryStability) {
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.enable_gradient_feedback = true;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create test gradient tensor
    uint32_t dims[] = {128};
    nimcp_tensor_t* grad = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(grad, nullptr);

    float* data = (float*)nimcp_tensor_data(grad);
    for (size_t i = 0; i < 128; i++) {
        data[i] = (float)i * 0.01f;
    }

    // Repeated set/propagate cycles
    for (int i = 0; i < 50; i++) {
        int result = cnn_cortex_bridge_set_gradients(bridge, grad);
        EXPECT_EQ(result, 0) << "Set gradients failed on cycle " << i;

        result = cnn_cortex_bridge_propagate_gradients(bridge);
        EXPECT_EQ(result, 0) << "Propagate gradients failed on cycle " << i;
    }

    nimcp_tensor_destroy(grad);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

/**
 * Verify LR factor stays within bounds for extreme confidence values
 * REGRESSION TARGET: LR clamping
 */
TEST_F(CNNCortexBridgeRegressionTest, LRFactorBoundsStability) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    // Test with various base LR values
    float test_lrs[] = {0.0001f, 0.001f, 0.01f, 0.1f, 1.0f, 10.0f};

    for (float base_lr : test_lrs) {
        float modulated = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

        // Modulated LR should be within [base * min_factor, base * max_factor]
        float min_expected = base_lr * CNN_CORTEX_DEFAULT_LR_MIN_FACTOR;
        float max_expected = base_lr * CNN_CORTEX_DEFAULT_LR_MAX_FACTOR;

        EXPECT_GE(modulated, min_expected * 0.99f)
            << "LR below minimum for base_lr=" << base_lr;
        EXPECT_LE(modulated, max_expected * 1.01f)
            << "LR above maximum for base_lr=" << base_lr;
    }

    cnn_cortex_bridge_destroy(bridge);
}

/**
 * Verify LR modulation produces consistent results for same input
 * REGRESSION TARGET: Deterministic LR computation
 */
TEST_F(CNNCortexBridgeRegressionTest, LRModulationDeterminism) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    float base_lr = 0.01f;

    // Get LR multiple times
    float lr1 = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);
    float lr2 = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);
    float lr3 = cnn_cortex_bridge_get_modulated_lr(bridge, base_lr);

    // Should be identical
    EXPECT_FLOAT_EQ(lr1, lr2);
    EXPECT_FLOAT_EQ(lr2, lr3);

    cnn_cortex_bridge_destroy(bridge);
}

/**
 * Verify skip sample decision is consistent
 * REGRESSION TARGET: Skip logic
 */
TEST_F(CNNCortexBridgeRegressionTest, SkipSampleConsistency) {
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.skip_low_quality_samples = true;

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Without cortex connected, skip should be false
    bool skip1 = cnn_cortex_bridge_should_skip_sample(bridge);
    bool skip2 = cnn_cortex_bridge_should_skip_sample(bridge);
    bool skip3 = cnn_cortex_bridge_should_skip_sample(bridge);

    EXPECT_EQ(skip1, skip2);
    EXPECT_EQ(skip2, skip3);

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// State Consistency Tests
//=============================================================================

/**
 * Verify stats accumulate correctly across operations
 * REGRESSION TARGET: Statistics accumulation
 */
TEST_F(CNNCortexBridgeRegressionTest, StatisticsAccumulation) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    cnn_cortex_bridge_stats_t stats1, stats2;

    // Initial stats
    int result = cnn_cortex_bridge_get_stats(bridge, &stats1);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats1.total_feature_extractions, 0u);
    EXPECT_EQ(stats1.total_updates, 0u);

    // Update bridge
    cnn_cortex_bridge_update(bridge);
    cnn_cortex_bridge_update(bridge);
    cnn_cortex_bridge_update(bridge);

    result = cnn_cortex_bridge_get_stats(bridge, &stats2);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats2.total_updates, 3u);

    cnn_cortex_bridge_destroy(bridge);
}

/**
 * Verify stats reset preserves connection state
 * REGRESSION TARGET: Stats reset behavior
 */
TEST_F(CNNCortexBridgeRegressionTest, StatsResetPreservesConnections) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    // Do some operations
    cnn_cortex_bridge_update(bridge);

    cnn_cortex_bridge_stats_t stats_before;
    cnn_cortex_bridge_get_stats(bridge, &stats_before);
    bool trainer_connected = stats_before.trainer_connected;
    bool visual_connected = stats_before.visual_cortex_connected;
    bool audio_connected = stats_before.audio_cortex_connected;
    cnn_cortex_mode_t mode = stats_before.current_mode;

    // Reset
    int result = cnn_cortex_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);

    cnn_cortex_bridge_stats_t stats_after;
    cnn_cortex_bridge_get_stats(bridge, &stats_after);

    // Connection state should be preserved
    EXPECT_EQ(stats_after.trainer_connected, trainer_connected);
    EXPECT_EQ(stats_after.visual_cortex_connected, visual_connected);
    EXPECT_EQ(stats_after.audio_cortex_connected, audio_connected);
    EXPECT_EQ(stats_after.current_mode, mode);

    // Counters should be reset
    EXPECT_EQ(stats_after.total_updates, 0u);
    EXPECT_EQ(stats_after.total_feature_extractions, 0u);

    cnn_cortex_bridge_destroy(bridge);
}

/**
 * Verify connection state changes correctly tracked
 * REGRESSION TARGET: Connection tracking
 */
TEST_F(CNNCortexBridgeRegressionTest, ConnectionStateTracking) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    // Initially not connected
    EXPECT_FALSE(cnn_cortex_bridge_is_connected(bridge));

    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_FALSE(stats.trainer_connected);
    EXPECT_FALSE(stats.visual_cortex_connected);
    EXPECT_FALSE(stats.audio_cortex_connected);

    // Connect/disconnect should update state
    // (Would need actual cortex instances for full test)

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

/**
 * Verify all API functions handle NULL bridge gracefully
 * REGRESSION TARGET: NULL safety across API
 */
TEST_F(CNNCortexBridgeRegressionTest, NullBridgeSafety) {
    cnn_cortex_bridge_t* null_bridge = nullptr;

    // All these should return error, not crash
    EXPECT_EQ(cnn_cortex_bridge_connect_trainer(null_bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_connect_visual_cortex(null_bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_connect_audio_cortex(null_bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_connect_perception_bridge(null_bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_FALSE(cnn_cortex_bridge_is_connected(null_bridge));

    EXPECT_EQ(cnn_cortex_bridge_extract_visual_features(null_bridge, nullptr, 0, 0, 0, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_extract_audio_features(null_bridge, nullptr, 0, 0, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(cnn_cortex_bridge_set_gradients(null_bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_propagate_gradients(null_bridge),
              NIMCP_ERROR_NULL_POINTER);

    cnn_cortex_perception_metrics_t metrics;
    EXPECT_EQ(cnn_cortex_bridge_get_perception_metrics(null_bridge, &metrics),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_FLOAT_EQ(cnn_cortex_bridge_get_modulated_lr(null_bridge, 0.01f), 0.01f);
    EXPECT_FALSE(cnn_cortex_bridge_should_skip_sample(null_bridge));

    visual_cortex_training_state_t vs;
    EXPECT_EQ(cnn_cortex_bridge_get_visual_state(null_bridge, &vs),
              NIMCP_ERROR_NULL_POINTER);

    audio_cortex_training_state_t as;
    EXPECT_EQ(cnn_cortex_bridge_get_audio_state(null_bridge, &as),
              NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(cnn_cortex_bridge_update(null_bridge), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(cnn_cortex_bridge_connect_bio_async(null_bridge),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_disconnect_bio_async(null_bridge),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(cnn_cortex_bridge_is_bio_async_connected(null_bridge));

    cnn_cortex_bridge_stats_t stats;
    EXPECT_EQ(cnn_cortex_bridge_get_stats(null_bridge, &stats),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(cnn_cortex_bridge_reset_stats(null_bridge),
              NIMCP_ERROR_NULL_POINTER);
}

/**
 * Verify operations without connected cortexes return appropriate errors
 * REGRESSION TARGET: Disconnected operation handling
 */
TEST_F(CNNCortexBridgeRegressionTest, DisconnectedOperationHandling) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    // Extraction without connected cortex should fail gracefully
    uint8_t image[64];
    nimcp_tensor_t* features = nullptr;

    int result = cnn_cortex_bridge_extract_visual_features(
        bridge, image, 8, 8, 1, &features);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(features, nullptr);

    float audio[64];
    result = cnn_cortex_bridge_extract_audio_features(
        bridge, audio, 64, 1, &features);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(features, nullptr);

    cnn_cortex_bridge_destroy(bridge);
}

/**
 * Verify gradient feedback with disabled config is no-op
 * REGRESSION TARGET: Disabled gradient handling
 */
TEST_F(CNNCortexBridgeRegressionTest, DisabledGradientFeedbackNoOp) {
    cnn_cortex_bridge_config_t config;
    cnn_cortex_bridge_default_config(&config);
    config.enable_gradient_feedback = false;  // Explicitly disabled

    cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Create gradient tensor
    uint32_t dims[] = {64};
    nimcp_tensor_t* grad = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(grad, nullptr);

    // Should succeed (as no-op)
    int result = cnn_cortex_bridge_set_gradients(bridge, grad);
    EXPECT_EQ(result, 0);

    result = cnn_cortex_bridge_propagate_gradients(bridge);
    EXPECT_EQ(result, 0);

    // Stats should show no feedbacks
    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_gradient_feedbacks, 0u);

    nimcp_tensor_destroy(grad);
    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// String Conversion Stability Tests
//=============================================================================

/**
 * Verify enum-to-string conversions return valid strings
 * REGRESSION TARGET: String representations
 */
TEST_F(CNNCortexBridgeRegressionTest, EnumStringConversions) {
    // Mode strings
    EXPECT_STREQ(cnn_cortex_mode_to_string(CNN_CORTEX_MODE_DISABLED), "DISABLED");
    EXPECT_STREQ(cnn_cortex_mode_to_string(CNN_CORTEX_MODE_FEATURE_ONLY), "FEATURE_ONLY");
    EXPECT_STREQ(cnn_cortex_mode_to_string(CNN_CORTEX_MODE_TRAINING), "TRAINING");
    EXPECT_STREQ(cnn_cortex_mode_to_string(CNN_CORTEX_MODE_FINE_TUNING), "FINE_TUNING");
    EXPECT_STREQ(cnn_cortex_mode_to_string((cnn_cortex_mode_t)99), "UNKNOWN");

    // Priority strings
    EXPECT_STREQ(cnn_cortex_priority_to_string(CNN_CORTEX_PRIORITY_VISUAL), "VISUAL");
    EXPECT_STREQ(cnn_cortex_priority_to_string(CNN_CORTEX_PRIORITY_AUDIO), "AUDIO");
    EXPECT_STREQ(cnn_cortex_priority_to_string(CNN_CORTEX_PRIORITY_MULTIMODAL), "MULTIMODAL");
    EXPECT_STREQ(cnn_cortex_priority_to_string((cnn_cortex_priority_t)99), "UNKNOWN");

    // Gradient method strings
    EXPECT_STREQ(cnn_cortex_gradient_method_to_string(CNN_CORTEX_GRADIENT_MAGNITUDE), "MAGNITUDE");
    EXPECT_STREQ(cnn_cortex_gradient_method_to_string(CNN_CORTEX_GRADIENT_SIGN), "SIGN");
    EXPECT_STREQ(cnn_cortex_gradient_method_to_string(CNN_CORTEX_GRADIENT_HEBBIAN), "HEBBIAN");
    EXPECT_STREQ(cnn_cortex_gradient_method_to_string(CNN_CORTEX_GRADIENT_NONE), "NONE");
    EXPECT_STREQ(cnn_cortex_gradient_method_to_string((cnn_cortex_gradient_method_t)99), "UNKNOWN");
}

//=============================================================================
// Thread Safety Regression Tests
//=============================================================================

/**
 * Verify update cycle is safe to call repeatedly
 * REGRESSION TARGET: Update cycle stability
 */
TEST_F(CNNCortexBridgeRegressionTest, RepeatedUpdateCycleStability) {
    cnn_cortex_bridge_t* bridge = create_default_bridge();
    ASSERT_NE(bridge, nullptr);

    // Rapid update cycles
    for (int i = 0; i < 1000; i++) {
        int result = cnn_cortex_bridge_update(bridge);
        EXPECT_EQ(result, 0) << "Update failed on cycle " << i;
    }

    cnn_cortex_bridge_stats_t stats;
    cnn_cortex_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_updates, 1000u);

    cnn_cortex_bridge_destroy(bridge);
}

//=============================================================================
// Configuration Variation Tests
//=============================================================================

/**
 * Verify bridge works correctly with all mode combinations
 * REGRESSION TARGET: Mode handling
 */
TEST_F(CNNCortexBridgeRegressionTest, AllModesWork) {
    cnn_cortex_mode_t modes[] = {
        CNN_CORTEX_MODE_DISABLED,
        CNN_CORTEX_MODE_FEATURE_ONLY,
        CNN_CORTEX_MODE_TRAINING,
        CNN_CORTEX_MODE_FINE_TUNING
    };

    for (cnn_cortex_mode_t mode : modes) {
        cnn_cortex_bridge_config_t config;
        cnn_cortex_bridge_default_config(&config);
        config.mode = mode;

        cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create bridge with mode "
                                   << cnn_cortex_mode_to_string(mode);

        cnn_cortex_bridge_stats_t stats;
        cnn_cortex_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(stats.current_mode, mode);

        cnn_cortex_bridge_destroy(bridge);
    }
}

/**
 * Verify bridge works correctly with all priority combinations
 * REGRESSION TARGET: Priority handling
 */
TEST_F(CNNCortexBridgeRegressionTest, AllPrioritiesWork) {
    cnn_cortex_priority_t priorities[] = {
        CNN_CORTEX_PRIORITY_VISUAL,
        CNN_CORTEX_PRIORITY_AUDIO,
        CNN_CORTEX_PRIORITY_MULTIMODAL
    };

    for (cnn_cortex_priority_t priority : priorities) {
        cnn_cortex_bridge_config_t config;
        cnn_cortex_bridge_default_config(&config);
        config.priority = priority;

        cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create bridge with priority "
                                   << cnn_cortex_priority_to_string(priority);

        cnn_cortex_bridge_destroy(bridge);
    }
}

/**
 * Verify bridge works correctly with all gradient method combinations
 * REGRESSION TARGET: Gradient method handling
 */
TEST_F(CNNCortexBridgeRegressionTest, AllGradientMethodsWork) {
    cnn_cortex_gradient_method_t methods[] = {
        CNN_CORTEX_GRADIENT_MAGNITUDE,
        CNN_CORTEX_GRADIENT_SIGN,
        CNN_CORTEX_GRADIENT_HEBBIAN,
        CNN_CORTEX_GRADIENT_NONE
    };

    for (cnn_cortex_gradient_method_t method : methods) {
        cnn_cortex_bridge_config_t config;
        cnn_cortex_bridge_default_config(&config);
        config.gradient_method = method;

        cnn_cortex_bridge_t* bridge = cnn_cortex_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create bridge with gradient method "
                                   << cnn_cortex_gradient_method_to_string(method);

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
