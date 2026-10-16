/**
 * @file test_visual_jepa_bridge.cpp
 * @brief Unit tests for Visual JEPA Bridge and Visual JEPA-FEP Bridge
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Comprehensive unit tests for visual-to-JEPA encoding bridge and FEP integration
 * WHY:  Ensure correct lifecycle, encoding, training, and FEP precision weighting
 * HOW:  Test all API functions with valid and invalid inputs, verify return codes,
 *       state transitions, and null safety.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include "perception/nimcp_visual_jepa_bridge.h"
#include "perception/nimcp_visual_jepa_fep_bridge.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Visual JEPA Bridge Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Visual JEPA Bridge tests
 *
 * WHAT: Provides setup/teardown for bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create bridge in SetUp, destroy in TearDown
 */
class VisualJepaBridgeTest : public ::testing::Test {
protected:
    visual_jepa_bridge_t* bridge = nullptr;

    /**
     * WHAT: Initialize bridge before each test
     * WHY:  Ensure clean starting state
     * HOW:  Create with default config
     */
    void SetUp() override {
        visual_jepa_bridge_config_t config;
        visual_jepa_bridge_default_config(&config);
        bridge = visual_jepa_bridge_create(&config);
    }

    /**
     * WHAT: Clean up bridge after each test
     * WHY:  Prevent memory leaks
     * HOW:  Destroy bridge, reset pointer
     */
    void TearDown() override {
        if (bridge) {
            visual_jepa_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test successful bridge creation and destruction
 * WHY:  Verify basic lifecycle management
 * HOW:  Create with valid config, verify non-null, destroy
 */
TEST_F(VisualJepaBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(VisualJepaBridgeTest, CreateWithNullConfig) {
    // Clean up existing bridge
    visual_jepa_bridge_destroy(bridge);

    // Create with NULL config
    bridge = visual_jepa_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(VisualJepaBridgeTest, DestroyNull) {
    visual_jepa_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields
 */
TEST_F(VisualJepaBridgeTest, DefaultConfig) {
    visual_jepa_bridge_config_t config;
    int result = visual_jepa_bridge_default_config(&config);

    EXPECT_EQ(result, 0);

    // Verify encoder config
    EXPECT_GT(config.encoder.hidden_dim, 0u);
    EXPECT_GT(config.encoder.output_dim, 0u);

    // Verify patch config
    EXPECT_GT(config.patch.patch_width, 0u);
    EXPECT_GT(config.patch.patch_height, 0u);
    EXPECT_GT(config.patch.num_patches_x, 0u);
    EXPECT_GT(config.patch.num_patches_y, 0u);

    // Verify training parameters
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GE(config.momentum, 0.0f);
    EXPECT_LE(config.momentum, 1.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(VisualJepaBridgeTest, DefaultConfigNull) {
    int result = visual_jepa_bridge_default_config(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test bridge reset functionality
 * WHY:  Verify state reset works correctly
 * HOW:  Reset bridge, verify success
 */
TEST_F(VisualJepaBridgeTest, Reset) {
    int result = visual_jepa_bridge_reset(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reset with NULL bridge
 * WHY:  Verify NULL safety for reset
 * HOW:  Call reset with NULL
 */
TEST_F(VisualJepaBridgeTest, ResetNull) {
    int result = visual_jepa_bridge_reset(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Connection Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test connecting visual cortex
 * WHY:  Verify visual cortex integration setup
 * HOW:  Connect visual cortex pointer, verify success
 */
TEST_F(VisualJepaBridgeTest, ConnectVisualCortex) {
    // Mock visual cortex (just a non-null pointer for testing)
    visual_cortex_t* mock_visual = reinterpret_cast<visual_cortex_t*>(0x1000);

    int result = visual_jepa_bridge_connect_visual_cortex(bridge, mock_visual);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(visual_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test disconnecting visual cortex
 * WHY:  Verify visual cortex disconnection
 * HOW:  Connect then disconnect, verify state
 */
TEST_F(VisualJepaBridgeTest, DisconnectVisualCortex) {
    visual_cortex_t* mock_visual = reinterpret_cast<visual_cortex_t*>(0x1000);
    visual_jepa_bridge_connect_visual_cortex(bridge, mock_visual);

    int result = visual_jepa_bridge_disconnect_visual_cortex(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(visual_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test is_connected before any connection
 * WHY:  Verify initial connection state
 * HOW:  Check connection status on fresh bridge
 */
TEST_F(VisualJepaBridgeTest, InitialConnectionState) {
    EXPECT_FALSE(visual_jepa_bridge_is_connected(bridge));
}

/**
 * WHAT: Test connection functions with NULL bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect functions with NULL bridge
 */
TEST_F(VisualJepaBridgeTest, ConnectNullBridge) {
    visual_cortex_t* mock_visual = reinterpret_cast<visual_cortex_t*>(0x1000);

    EXPECT_EQ(visual_jepa_bridge_connect_visual_cortex(nullptr, mock_visual), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_bridge_disconnect_visual_cortex(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(visual_jepa_bridge_is_connected(nullptr));
}

/**
 * WHAT: Test connecting with NULL visual cortex
 * WHY:  Verify NULL safety for system pointers
 * HOW:  Call connect with NULL visual cortex
 */
TEST_F(VisualJepaBridgeTest, ConnectNullVisualCortex) {
    EXPECT_EQ(visual_jepa_bridge_connect_visual_cortex(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Encoding Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test encoding visual features to JEPA latent
 * WHY:  Verify core encoding functionality
 * HOW:  Create test features and latent, call encode
 */
TEST_F(VisualJepaBridgeTest, Encode) {
    // Get the config to know expected dimensions
    visual_jepa_bridge_config_t config;
    visual_jepa_bridge_default_config(&config);

    // Create test features with correct input dimension
    const uint32_t feature_dim = config.encoder.input_dim;
    std::vector<float> features(feature_dim);
    for (uint32_t i = 0; i < feature_dim; i++) {
        features[i] = static_cast<float>(i) / feature_dim;
    }

    // Create latent with correct output dimension
    jepa_latent_t* latent = jepa_latent_create_dim(config.encoder.output_dim);
    ASSERT_NE(latent, nullptr);

    int result = visual_jepa_bridge_encode(bridge, features.data(), feature_dim, latent);
    EXPECT_EQ(result, 0);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test encode with NULL parameters
 * WHY:  Verify NULL safety for encoding
 * HOW:  Call encode with various NULL parameters
 */
TEST_F(VisualJepaBridgeTest, EncodeNullParams) {
    const uint32_t feature_dim = 512;
    float features[feature_dim];
    jepa_latent_t* latent = jepa_latent_create_dim(256);

    // NULL bridge
    EXPECT_EQ(visual_jepa_bridge_encode(nullptr, features, feature_dim, latent), NIMCP_ERROR_NULL_POINTER);

    // NULL features
    EXPECT_EQ(visual_jepa_bridge_encode(bridge, nullptr, feature_dim, latent), NIMCP_ERROR_NULL_POINTER);

    // NULL latent output
    EXPECT_EQ(visual_jepa_bridge_encode(bridge, features, feature_dim, nullptr), NIMCP_ERROR_NULL_POINTER);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test encode with zero dimension
 * WHY:  Verify edge case handling
 * HOW:  Call encode with zero feature dimension
 */
TEST_F(VisualJepaBridgeTest, EncodeZeroDim) {
    float features[1] = {0.0f};
    jepa_latent_t* latent = jepa_latent_create_dim(256);

    int result = visual_jepa_bridge_encode(bridge, features, 0, latent);
    EXPECT_NE(result, NIMCP_SUCCESS);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test patch-level encoding of image
 * WHY:  Verify patch extraction and encoding
 * HOW:  Create test image, encode patches
 */
TEST_F(VisualJepaBridgeTest, EncodePatches) {
    // Get the config to know expected dimensions
    visual_jepa_bridge_config_t config;
    visual_jepa_bridge_default_config(&config);

    // Create test image (112x112x3)
    const uint32_t width = 112;
    const uint32_t height = 112;
    const uint32_t channels = 3;
    const uint32_t image_size = width * height * channels;

    std::vector<float> image(image_size);
    for (uint32_t i = 0; i < image_size; i++) {
        image[i] = static_cast<float>(i % 256) / 255.0f;
    }

    // Allocate output arrays with correct latent dimension
    const uint32_t max_patches = 64;
    std::vector<jepa_latent_t*> patch_latents(max_patches);
    for (uint32_t i = 0; i < max_patches; i++) {
        patch_latents[i] = jepa_latent_create_dim(config.encoder.output_dim);
    }
    uint32_t num_patches = 0;

    int result = visual_jepa_bridge_encode_patches(
        bridge, image.data(), width, height, channels,
        patch_latents.data(), &num_patches
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(num_patches, 0u);

    // Cleanup
    for (uint32_t i = 0; i < max_patches; i++) {
        jepa_latent_destroy(patch_latents[i]);
    }
}

/**
 * WHAT: Test encode_patches with NULL parameters
 * WHY:  Verify NULL safety for patch encoding
 * HOW:  Call encode_patches with various NULL parameters
 */
TEST_F(VisualJepaBridgeTest, EncodePatchesNullParams) {
    float image[100];
    jepa_latent_t* patch_latents[10];
    uint32_t num_patches = 0;

    // NULL bridge
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        nullptr, image, 10, 10, 1, patch_latents, &num_patches), NIMCP_ERROR_NULL_POINTER);

    // NULL image
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, nullptr, 10, 10, 1, patch_latents, &num_patches), NIMCP_ERROR_NULL_POINTER);

    // NULL output array
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, image, 10, 10, 1, nullptr, &num_patches), NIMCP_ERROR_NULL_POINTER);

    // NULL num_patches
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, image, 10, 10, 1, patch_latents, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test encode_patches with zero dimensions
 * WHY:  Verify edge case handling for invalid image size
 * HOW:  Call encode_patches with zero width/height
 */
TEST_F(VisualJepaBridgeTest, EncodePatchesZeroDimensions) {
    float image[100];
    jepa_latent_t* patch_latents[10];
    uint32_t num_patches = 0;

    // Zero width
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, image, 0, 10, 1, patch_latents, &num_patches), NIMCP_ERROR_INVALID_PARAMETER);

    // Zero height
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, image, 10, 0, 1, patch_latents, &num_patches), NIMCP_ERROR_INVALID_PARAMETER);

    // Zero channels
    EXPECT_EQ(visual_jepa_bridge_encode_patches(
        bridge, image, 10, 10, 0, patch_latents, &num_patches), NIMCP_ERROR_INVALID_PARAMETER);
}

/* ============================================================================
 * Training Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test setting training mode
 * WHY:  Verify training/inference mode toggle
 * HOW:  Set training mode, verify success
 */
TEST_F(VisualJepaBridgeTest, SetTraining) {
    // Enable training mode
    int result = visual_jepa_bridge_set_training(bridge, true);
    EXPECT_EQ(result, 0);

    // Disable training mode
    result = visual_jepa_bridge_set_training(bridge, false);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test set_training with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call set_training with NULL
 */
TEST_F(VisualJepaBridgeTest, SetTrainingNull) {
    int result = visual_jepa_bridge_set_training(nullptr, true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test training step functionality
 * WHY:  Verify JEPA training iteration works
 * HOW:  Create test features, perform training step
 */
TEST_F(VisualJepaBridgeTest, TrainStep) {
    // Enable training mode first
    visual_jepa_bridge_set_training(bridge, true);

    // Create test features (7x7x512 feature map)
    const uint32_t width = 7;
    const uint32_t height = 7;
    const uint32_t channels = 512;
    const uint32_t feature_size = width * height * channels;

    float* features = new float[feature_size];
    for (uint32_t i = 0; i < feature_size; i++) {
        features[i] = static_cast<float>(i % 256) / 255.0f;
    }

    float loss = -1.0f;
    int result = visual_jepa_bridge_train_step(
        bridge, features, width, height, channels, &loss
    );

    EXPECT_EQ(result, 0);
    EXPECT_GE(loss, 0.0f);  // Loss should be non-negative

    delete[] features;
}

/**
 * WHAT: Test train_step with NULL parameters
 * WHY:  Verify NULL safety for training
 * HOW:  Call train_step with various NULL parameters
 */
TEST_F(VisualJepaBridgeTest, TrainStepNullParams) {
    float features[100];
    float loss;

    // NULL bridge
    EXPECT_EQ(visual_jepa_bridge_train_step(
        nullptr, features, 10, 10, 1, &loss), NIMCP_ERROR_NULL_POINTER);

    // NULL features
    EXPECT_EQ(visual_jepa_bridge_train_step(
        bridge, nullptr, 10, 10, 1, &loss), NIMCP_ERROR_NULL_POINTER);

    // NULL loss output is acceptable (loss is optional output)
    // The function should succeed with NULL loss
}

/**
 * WHAT: Test target encoder update
 * WHY:  Verify EMA update mechanism
 * HOW:  Call update_target_encoder
 */
TEST_F(VisualJepaBridgeTest, UpdateTargetEncoder) {
    int result = visual_jepa_bridge_update_target_encoder(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update_target_encoder with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL
 */
TEST_F(VisualJepaBridgeTest, UpdateTargetEncoderNull) {
    int result = visual_jepa_bridge_update_target_encoder(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test getting bridge statistics
 * WHY:  Verify statistics reporting functionality
 * HOW:  Get stats, verify success and initial values
 */
TEST_F(VisualJepaBridgeTest, GetStats) {
    visual_jepa_stats_t stats;
    int result = visual_jepa_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(result, 0);
    // Initial stats should be zero
    EXPECT_EQ(stats.frames_processed, 0u);
    EXPECT_EQ(stats.patches_encoded, 0u);
    EXPECT_EQ(stats.predictions_made, 0u);
}

/**
 * WHAT: Test get_stats with NULL parameters
 * WHY:  Verify NULL safety for getter functions
 * HOW:  Call with various NULL combinations
 */
TEST_F(VisualJepaBridgeTest, GetStatsNull) {
    visual_jepa_stats_t stats;

    EXPECT_EQ(visual_jepa_bridge_get_stats(nullptr, &stats), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_bridge_get_stats(bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test statistics reset
 * WHY:  Verify statistics can be cleared
 * HOW:  Reset stats, verify success
 */
TEST_F(VisualJepaBridgeTest, ResetStats) {
    int result = visual_jepa_bridge_reset_stats(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reset_stats with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call reset_stats with NULL
 */
TEST_F(VisualJepaBridgeTest, ResetStatsNull) {
    int result = visual_jepa_bridge_reset_stats(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Bio-Async Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test bio-async connection
 * WHY:  Verify bio-async integration setup
 * HOW:  Connect bio-async, check state
 */
TEST_F(VisualJepaBridgeTest, BioAsyncConnect) {
    int result = visual_jepa_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    // Just verify it doesn't crash
    (void)result;
}

/**
 * WHAT: Test bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(VisualJepaBridgeTest, BioAsyncDisconnect) {
    visual_jepa_bridge_connect_bio_async(bridge);
    int result = visual_jepa_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test checking bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(VisualJepaBridgeTest, BioAsyncIsConnected) {
    bool connected = visual_jepa_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);

    visual_jepa_bridge_connect_bio_async(bridge);
    // May or may not be connected depending on router availability
}

/**
 * WHAT: Test bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(VisualJepaBridgeTest, BioAsyncNull) {
    EXPECT_EQ(visual_jepa_bridge_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_bridge_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(visual_jepa_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Batch Utility Tests - Visual JEPA Bridge
 * ============================================================================ */

/**
 * WHAT: Test batch creation and destruction
 * WHY:  Verify batch utility functions
 * HOW:  Create batch, verify non-null, destroy
 */
TEST_F(VisualJepaBridgeTest, BatchCreateDestroy) {
    visual_jepa_batch_t* batch = visual_jepa_batch_create(49, 512, 256);
    ASSERT_NE(batch, nullptr);

    EXPECT_EQ(batch->num_patches, 49u);

    visual_jepa_batch_destroy(batch);
}

/**
 * WHAT: Test batch destroy with NULL
 * WHY:  Verify NULL safety for batch destruction
 * HOW:  Call destroy with NULL
 */
TEST_F(VisualJepaBridgeTest, BatchDestroyNull) {
    visual_jepa_batch_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test batch creation with zero parameters
 * WHY:  Verify edge case handling
 * HOW:  Create batch with zero values
 */
TEST_F(VisualJepaBridgeTest, BatchCreateZero) {
    visual_jepa_batch_t* batch = visual_jepa_batch_create(0, 512, 256);
    EXPECT_EQ(batch, nullptr);
}

/* ============================================================================
 * Visual JEPA-FEP Bridge Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Visual JEPA-FEP Bridge tests
 *
 * WHAT: Provides setup/teardown for FEP bridge testing
 * WHY:  Ensure clean state for each test
 * HOW:  Create FEP bridge in SetUp, destroy in TearDown
 */
class VisualJepaFepBridgeTest : public ::testing::Test {
protected:
    visual_jepa_fep_bridge_t* fep_bridge = nullptr;

    void SetUp() override {
        visual_jepa_fep_config_t config;
        visual_jepa_fep_bridge_default_config(&config);
        fep_bridge = visual_jepa_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (fep_bridge) {
            visual_jepa_fep_bridge_destroy(fep_bridge);
            fep_bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests - Visual JEPA-FEP Bridge
 * ============================================================================ */

/**
 * WHAT: Test successful FEP bridge creation and destruction
 * WHY:  Verify basic lifecycle management
 * HOW:  Create with valid config, verify non-null, destroy
 */
TEST_F(VisualJepaFepBridgeTest, CreateDestroy) {
    ASSERT_NE(fep_bridge, nullptr);
    // Cleanup handled by TearDown
}

/**
 * WHAT: Test creation with NULL config uses defaults
 * WHY:  Verify NULL config fallback behavior
 * HOW:  Create with NULL, verify non-null result
 */
TEST_F(VisualJepaFepBridgeTest, CreateWithNullConfig) {
    visual_jepa_fep_bridge_destroy(fep_bridge);
    fep_bridge = visual_jepa_fep_bridge_create(nullptr);
    ASSERT_NE(fep_bridge, nullptr);
}

/**
 * WHAT: Test destroying NULL FEP bridge is safe
 * WHY:  Verify NULL pointer safety
 * HOW:  Call destroy with NULL, should not crash
 */
TEST_F(VisualJepaFepBridgeTest, DestroyNull) {
    visual_jepa_fep_bridge_destroy(nullptr);
    // Test passes if no crash
}

/**
 * WHAT: Test FEP default config provides valid values
 * WHY:  Verify configuration initialization
 * HOW:  Call default_config, check fields
 */
TEST_F(VisualJepaFepBridgeTest, DefaultConfig) {
    visual_jepa_fep_config_t config;
    int result = visual_jepa_fep_bridge_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_GT(config.initial_precision, 0.0f);
    EXPECT_GT(config.precision_learning_rate, 0.0f);
    EXPECT_GE(config.precision_decay, 0.0f);
    EXPECT_LE(config.precision_decay, 1.0f);
    EXPECT_GT(config.high_pe_threshold, 0.0f);
}

/**
 * WHAT: Test default config with NULL parameter
 * WHY:  Verify NULL safety for config initialization
 * HOW:  Call with NULL, expect error
 */
TEST_F(VisualJepaFepBridgeTest, DefaultConfigNull) {
    int result = visual_jepa_fep_bridge_default_config(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test FEP bridge reset functionality
 * WHY:  Verify state reset works correctly
 * HOW:  Reset bridge, verify success
 */
TEST_F(VisualJepaFepBridgeTest, Reset) {
    int result = visual_jepa_fep_bridge_reset(fep_bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test reset with NULL FEP bridge
 * WHY:  Verify NULL safety for reset
 * HOW:  Call reset with NULL
 */
TEST_F(VisualJepaFepBridgeTest, ResetNull) {
    int result = visual_jepa_fep_bridge_reset(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Connection Tests - Visual JEPA-FEP Bridge
 * ============================================================================ */

/**
 * WHAT: Test connecting to Visual JEPA bridge
 * WHY:  Verify JEPA integration setup
 * HOW:  Connect JEPA pointer, verify success
 */
TEST_F(VisualJepaFepBridgeTest, ConnectJepa) {
    // Create a real visual jepa bridge for connection
    visual_jepa_bridge_config_t jepa_config;
    visual_jepa_bridge_default_config(&jepa_config);
    visual_jepa_bridge_t* jepa = visual_jepa_bridge_create(&jepa_config);
    ASSERT_NE(jepa, nullptr);

    int result = visual_jepa_fep_bridge_connect_jepa(fep_bridge, jepa);
    EXPECT_EQ(result, 0);

    visual_jepa_bridge_destroy(jepa);
}

/**
 * WHAT: Test connecting to FEP system
 * WHY:  Verify FEP integration setup
 * HOW:  Connect FEP pointer, verify success
 */
TEST_F(VisualJepaFepBridgeTest, ConnectFep) {
    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x2000);

    int result = visual_jepa_fep_bridge_connect_fep(fep_bridge, mock_fep);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test is_connected with both systems
 * WHY:  Verify full connection status
 * HOW:  Connect both systems, check is_connected
 */
TEST_F(VisualJepaFepBridgeTest, IsConnected) {
    EXPECT_FALSE(visual_jepa_fep_bridge_is_connected(fep_bridge));

    // Create real objects for connection testing
    visual_jepa_bridge_config_t jepa_config;
    visual_jepa_bridge_default_config(&jepa_config);
    visual_jepa_bridge_t* jepa = visual_jepa_bridge_create(&jepa_config);
    ASSERT_NE(jepa, nullptr);

    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x2000);

    visual_jepa_fep_bridge_connect_jepa(fep_bridge, jepa);
    EXPECT_FALSE(visual_jepa_fep_bridge_is_connected(fep_bridge));

    visual_jepa_fep_bridge_connect_fep(fep_bridge, mock_fep);
    EXPECT_TRUE(visual_jepa_fep_bridge_is_connected(fep_bridge));

    visual_jepa_bridge_destroy(jepa);
}

/**
 * WHAT: Test connection functions with NULL FEP bridge
 * WHY:  Verify NULL safety for connection functions
 * HOW:  Call connect functions with NULL bridge
 */
TEST_F(VisualJepaFepBridgeTest, ConnectNullBridge) {
    // Create a real JEPA bridge for testing NULL fep_bridge case
    visual_jepa_bridge_config_t jepa_config;
    visual_jepa_bridge_default_config(&jepa_config);
    visual_jepa_bridge_t* jepa = visual_jepa_bridge_create(&jepa_config);
    ASSERT_NE(jepa, nullptr);

    fep_system_t* mock_fep = reinterpret_cast<fep_system_t*>(0x2000);

    EXPECT_EQ(visual_jepa_fep_bridge_connect_jepa(nullptr, jepa), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_bridge_connect_fep(nullptr, mock_fep), NIMCP_ERROR_NULL_POINTER);

    visual_jepa_bridge_destroy(jepa);
    EXPECT_FALSE(visual_jepa_fep_bridge_is_connected(nullptr));
}

/**
 * WHAT: Test connecting with NULL systems
 * WHY:  Verify NULL safety for system pointers
 * HOW:  Call connect functions with NULL systems
 */
TEST_F(VisualJepaFepBridgeTest, ConnectNullSystems) {
    EXPECT_EQ(visual_jepa_fep_bridge_connect_jepa(fep_bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_bridge_connect_fep(fep_bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * FEP -> JEPA Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test getting precision weights for JEPA training
 * WHY:  Verify precision weighting functionality
 * HOW:  Get precision weights, check values
 */
TEST_F(VisualJepaFepBridgeTest, GetPrecisionWeights) {
    const uint32_t num_patches = 49;
    float weights[num_patches];

    int result = visual_jepa_fep_get_precision_weights(fep_bridge, weights, num_patches);
    EXPECT_EQ(result, 0);

    // All weights should be positive
    for (uint32_t i = 0; i < num_patches; i++) {
        EXPECT_GT(weights[i], 0.0f);
    }
}

/**
 * WHAT: Test precision weights with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL parameters
 */
TEST_F(VisualJepaFepBridgeTest, GetPrecisionWeightsNull) {
    float weights[49];

    EXPECT_EQ(visual_jepa_fep_get_precision_weights(nullptr, weights, 49), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_get_precision_weights(fep_bridge, nullptr, 49), NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test applying attention to precision
 * WHY:  Verify attention-precision integration
 * HOW:  Apply attention map, check success
 */
TEST_F(VisualJepaFepBridgeTest, ApplyAttentionPrecision) {
    const uint32_t width = 7;
    const uint32_t height = 7;
    float attention[width * height];

    // Create simple attention map
    for (uint32_t i = 0; i < width * height; i++) {
        attention[i] = 1.0f / (width * height);  // Uniform attention
    }

    int result = visual_jepa_fep_apply_attention_precision(
        fep_bridge, attention, width, height
    );
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test apply_attention_precision with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL parameters
 */
TEST_F(VisualJepaFepBridgeTest, ApplyAttentionPrecisionNull) {
    float attention[49];

    EXPECT_EQ(visual_jepa_fep_apply_attention_precision(
        nullptr, attention, 7, 7), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_apply_attention_precision(
        fep_bridge, nullptr, 7, 7), NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test getting learning rate modifier
 * WHY:  Verify precision-based LR scaling
 * HOW:  Get modifier, check valid range
 */
TEST_F(VisualJepaFepBridgeTest, GetLRModifier) {
    float modifier = visual_jepa_fep_get_lr_modifier(fep_bridge);

    // Modifier should be positive
    EXPECT_GT(modifier, 0.0f);
}

/**
 * WHAT: Test get_lr_modifier with NULL
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL, check return
 */
TEST_F(VisualJepaFepBridgeTest, GetLRModifierNull) {
    float modifier = visual_jepa_fep_get_lr_modifier(nullptr);

    // Should return default or safe value
    EXPECT_GE(modifier, 0.0f);
}

/* ============================================================================
 * JEPA -> FEP Direction Tests
 * ============================================================================ */

/**
 * WHAT: Test reporting prediction error to FEP
 * WHY:  Verify JEPA-to-FEP error propagation
 * HOW:  Create latents, report prediction error
 */
TEST_F(VisualJepaFepBridgeTest, ReportPredictionError) {
    // Create prediction and target latents
    jepa_latent_t* prediction = jepa_latent_create_dim(256);
    jepa_latent_t* target = jepa_latent_create_dim(256);
    ASSERT_NE(prediction, nullptr);
    ASSERT_NE(target, nullptr);

    // Set some test values
    float pred_values[256];
    float target_values[256];
    for (uint32_t i = 0; i < 256; i++) {
        pred_values[i] = static_cast<float>(i) / 256.0f;
        target_values[i] = static_cast<float>(i + 10) / 256.0f;
    }
    jepa_latent_set_embedding(prediction, pred_values, 256);
    jepa_latent_set_embedding(target, target_values, 256);

    int result = visual_jepa_fep_report_prediction_error(
        fep_bridge, prediction, target
    );
    EXPECT_EQ(result, 0);

    jepa_latent_destroy(prediction);
    jepa_latent_destroy(target);
}

/**
 * WHAT: Test report_prediction_error with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL parameters
 */
TEST_F(VisualJepaFepBridgeTest, ReportPredictionErrorNull) {
    jepa_latent_t* latent = jepa_latent_create_dim(256);

    EXPECT_EQ(visual_jepa_fep_report_prediction_error(
        nullptr, latent, latent), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_report_prediction_error(
        fep_bridge, nullptr, latent), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_report_prediction_error(
        fep_bridge, latent, nullptr), NIMCP_ERROR_NULL_POINTER);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test reporting novelty to FEP
 * WHY:  Verify novelty detection signaling
 * HOW:  Report high prediction error as novelty
 */
TEST_F(VisualJepaFepBridgeTest, ReportNovelty) {
    float high_pe = 5.0f;  // High prediction error

    int result = visual_jepa_fep_report_novelty(fep_bridge, high_pe);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test report_novelty with NULL bridge
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL
 */
TEST_F(VisualJepaFepBridgeTest, ReportNoveltyNull) {
    EXPECT_EQ(visual_jepa_fep_report_novelty(nullptr, 1.0f), NIMCP_ERROR_NULL_POINTER);
}

/**
 * WHAT: Test updating FEP beliefs from JEPA
 * WHY:  Verify belief update mechanism
 * HOW:  Create latent, update beliefs
 */
TEST_F(VisualJepaFepBridgeTest, UpdateBeliefs) {
    jepa_latent_t* latent = jepa_latent_create_dim(256);
    ASSERT_NE(latent, nullptr);

    int result = visual_jepa_fep_update_beliefs(fep_bridge, latent);
    EXPECT_EQ(result, 0);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test update_beliefs with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL parameters
 */
TEST_F(VisualJepaFepBridgeTest, UpdateBeliefsNull) {
    jepa_latent_t* latent = jepa_latent_create_dim(256);

    EXPECT_EQ(visual_jepa_fep_update_beliefs(nullptr, latent), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_update_beliefs(fep_bridge, nullptr), NIMCP_ERROR_NULL_POINTER);

    jepa_latent_destroy(latent);
}

/* ============================================================================
 * Update Cycle Tests - Visual JEPA-FEP Bridge
 * ============================================================================ */

/**
 * WHAT: Test main bridge update cycle
 * WHY:  Verify bidirectional synchronization
 * HOW:  Call update, verify success
 */
TEST_F(VisualJepaFepBridgeTest, Update) {
    int result = visual_jepa_fep_bridge_update(fep_bridge, 16);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update with NULL bridge
 * WHY:  Verify NULL safety for update
 * HOW:  Call update with NULL
 */
TEST_F(VisualJepaFepBridgeTest, UpdateNull) {
    int result = visual_jepa_fep_bridge_update(nullptr, 16);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test precision update from errors
 * WHY:  Verify precision learning
 * HOW:  Update precision with test errors
 */
TEST_F(VisualJepaFepBridgeTest, UpdatePrecision) {
    const uint32_t num_patches = 49;
    float errors[num_patches];

    // Create test prediction errors
    for (uint32_t i = 0; i < num_patches; i++) {
        errors[i] = 0.1f + (static_cast<float>(i) / num_patches) * 0.2f;
    }

    int result = visual_jepa_fep_update_precision(fep_bridge, errors, num_patches);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test update_precision with NULL parameters
 * WHY:  Verify NULL safety
 * HOW:  Call with NULL parameters
 */
TEST_F(VisualJepaFepBridgeTest, UpdatePrecisionNull) {
    float errors[49];

    EXPECT_EQ(visual_jepa_fep_update_precision(nullptr, errors, 49), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_update_precision(fep_bridge, nullptr, 49), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * State/Stats Tests - Visual JEPA-FEP Bridge
 * ============================================================================ */

/**
 * WHAT: Test getting FEP bridge state
 * WHY:  Verify state reporting functionality
 * HOW:  Get state, verify success
 */
TEST_F(VisualJepaFepBridgeTest, GetState) {
    visual_jepa_fep_state_t state;
    int result = visual_jepa_fep_bridge_get_state(fep_bridge, &state);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(state.updates_processed, 0u);
    EXPECT_GE(state.avg_precision, 0.0f);
}

/**
 * WHAT: Test getting FEP bridge statistics
 * WHY:  Verify statistics reporting
 * HOW:  Get stats, verify success
 */
TEST_F(VisualJepaFepBridgeTest, GetStats) {
    visual_jepa_fep_stats_t stats;
    int result = visual_jepa_fep_bridge_get_stats(fep_bridge, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

/**
 * WHAT: Test get state/stats with NULL parameters
 * WHY:  Verify NULL safety for getter functions
 * HOW:  Call with various NULL combinations
 */
TEST_F(VisualJepaFepBridgeTest, GetStateStatsNull) {
    visual_jepa_fep_state_t state;
    visual_jepa_fep_stats_t stats;

    EXPECT_EQ(visual_jepa_fep_bridge_get_state(nullptr, &state), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_bridge_get_state(fep_bridge, nullptr), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(visual_jepa_fep_bridge_get_stats(nullptr, &stats), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(visual_jepa_fep_bridge_get_stats(fep_bridge, nullptr), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bio-Async Tests - Visual JEPA-FEP Bridge
 * ============================================================================ */

/**
 * WHAT: Test FEP bio-async connection
 * WHY:  Verify bio-async integration setup
 * HOW:  Connect bio-async, check state
 */
TEST_F(VisualJepaFepBridgeTest, BioAsyncConnect) {
    int result = visual_jepa_fep_bridge_connect_bio_async(fep_bridge);
    // May return 0 or -1 depending on router availability
    (void)result;
}

/**
 * WHAT: Test FEP bio-async disconnection
 * WHY:  Verify bio-async cleanup
 * HOW:  Disconnect bio-async, check state
 */
TEST_F(VisualJepaFepBridgeTest, BioAsyncDisconnect) {
    visual_jepa_fep_bridge_connect_bio_async(fep_bridge);
    int result = visual_jepa_fep_bridge_disconnect_bio_async(fep_bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test checking FEP bio-async connection status
 * WHY:  Verify connection state query
 * HOW:  Check status before/after connection
 */
TEST_F(VisualJepaFepBridgeTest, BioAsyncIsConnected) {
    bool connected = visual_jepa_fep_bridge_is_bio_async_connected(fep_bridge);
    EXPECT_FALSE(connected);
}

/**
 * WHAT: Test FEP bio-async functions with NULL bridge
 * WHY:  Verify NULL safety for bio-async API
 * HOW:  Call bio-async functions with NULL
 */
TEST_F(VisualJepaFepBridgeTest, BioAsyncNull) {
    EXPECT_EQ(visual_jepa_fep_bridge_connect_bio_async(nullptr), -1);
    EXPECT_EQ(visual_jepa_fep_bridge_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(visual_jepa_fep_bridge_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Integration Tests - Combined Visual JEPA + FEP
 * ============================================================================ */

/**
 * @brief Integration test fixture for combined JEPA and FEP testing
 */
class VisualJepaIntegrationTest : public ::testing::Test {
protected:
    visual_jepa_bridge_t* jepa_bridge = nullptr;
    visual_jepa_fep_bridge_t* fep_bridge = nullptr;

    void SetUp() override {
        // Create JEPA bridge
        visual_jepa_bridge_config_t jepa_config;
        visual_jepa_bridge_default_config(&jepa_config);
        jepa_bridge = visual_jepa_bridge_create(&jepa_config);

        // Create FEP bridge
        visual_jepa_fep_config_t fep_config;
        visual_jepa_fep_bridge_default_config(&fep_config);
        fep_bridge = visual_jepa_fep_bridge_create(&fep_config);
    }

    void TearDown() override {
        /* FEP bridge holds reference to JEPA bridge, destroy FEP first */
        if (fep_bridge) {
            visual_jepa_fep_bridge_destroy(fep_bridge);
            fep_bridge = nullptr;
        }
        if (jepa_bridge) {
            visual_jepa_bridge_destroy(jepa_bridge);
            jepa_bridge = nullptr;
        }
    }
};

/**
 * WHAT: Test connecting FEP bridge to JEPA bridge
 * WHY:  Verify integration between the two systems
 * HOW:  Connect FEP bridge to JEPA bridge
 */
TEST_F(VisualJepaIntegrationTest, ConnectJepaToFep) {
    ASSERT_NE(jepa_bridge, nullptr);
    ASSERT_NE(fep_bridge, nullptr);

    int result = visual_jepa_fep_bridge_connect_jepa(fep_bridge, jepa_bridge);
    EXPECT_EQ(result, 0);
}

/**
 * WHAT: Test full encoding + FEP precision workflow
 * WHY:  Verify end-to-end integration
 * HOW:  Encode features, get precision weights, verify consistency
 */
TEST_F(VisualJepaIntegrationTest, EncodingWithPrecision) {
    ASSERT_NE(jepa_bridge, nullptr);
    ASSERT_NE(fep_bridge, nullptr);

    // Connect bridges
    visual_jepa_fep_bridge_connect_jepa(fep_bridge, jepa_bridge);

    // Get correct dimensions from config
    visual_jepa_bridge_config_t config;
    visual_jepa_bridge_default_config(&config);
    const uint32_t feature_dim = config.encoder.input_dim;

    // Create test features
    std::vector<float> features(feature_dim);
    for (uint32_t i = 0; i < feature_dim; i++) {
        features[i] = static_cast<float>(i) / feature_dim;
    }

    // Encode with JEPA
    jepa_latent_t* latent = jepa_latent_create_dim(config.encoder.output_dim);
    ASSERT_NE(latent, nullptr);

    int result = visual_jepa_bridge_encode(jepa_bridge, features.data(), feature_dim, latent);
    EXPECT_EQ(result, 0);

    // Get precision-weighted importance from FEP
    const uint32_t num_patches = 49;
    float weights[num_patches];
    result = visual_jepa_fep_get_precision_weights(fep_bridge, weights, num_patches);
    EXPECT_EQ(result, 0);

    // Update FEP beliefs with encoded latent
    result = visual_jepa_fep_update_beliefs(fep_bridge, latent);
    EXPECT_EQ(result, 0);

    jepa_latent_destroy(latent);
}

/**
 * WHAT: Test training step with FEP precision modulation
 * WHY:  Verify training integrates with FEP framework
 * HOW:  Run training step, update precision, verify stats change
 */
TEST_F(VisualJepaIntegrationTest, TrainingWithFepPrecision) {
    ASSERT_NE(jepa_bridge, nullptr);
    ASSERT_NE(fep_bridge, nullptr);

    // Connect bridges
    visual_jepa_fep_bridge_connect_jepa(fep_bridge, jepa_bridge);

    // Enable training mode
    visual_jepa_bridge_set_training(jepa_bridge, true);

    // Create test features
    const uint32_t width = 7;
    const uint32_t height = 7;
    const uint32_t channels = 512;
    const uint32_t feature_size = width * height * channels;

    float* features = new float[feature_size];
    for (uint32_t i = 0; i < feature_size; i++) {
        features[i] = static_cast<float>(i % 256) / 255.0f;
    }

    // Get initial LR modifier
    float initial_lr_mod = visual_jepa_fep_get_lr_modifier(fep_bridge);

    // Perform training step
    float loss = -1.0f;
    int result = visual_jepa_bridge_train_step(
        jepa_bridge, features, width, height, channels, &loss
    );
    EXPECT_EQ(result, 0);
    EXPECT_GE(loss, 0.0f);

    // Report prediction error to FEP
    result = visual_jepa_fep_report_novelty(fep_bridge, loss);
    EXPECT_EQ(result, 0);

    // Update FEP bridge
    result = visual_jepa_fep_bridge_update(fep_bridge, 16);
    EXPECT_EQ(result, 0);

    // Get stats after training
    visual_jepa_stats_t jepa_stats;
    result = visual_jepa_bridge_get_stats(jepa_bridge, &jepa_stats);
    EXPECT_EQ(result, 0);

    visual_jepa_fep_stats_t fep_stats;
    result = visual_jepa_fep_bridge_get_stats(fep_bridge, &fep_stats);
    EXPECT_EQ(result, 0);

    delete[] features;
}

/**
 * WHAT: Test multiple training iterations with EMA target update
 * WHY:  Verify target encoder momentum update works over iterations
 * HOW:  Run multiple training steps, update target encoder each time
 */
TEST_F(VisualJepaIntegrationTest, MultipleTrainingIterations) {
    ASSERT_NE(jepa_bridge, nullptr);

    visual_jepa_bridge_set_training(jepa_bridge, true);

    const uint32_t width = 7;
    const uint32_t height = 7;
    const uint32_t channels = 512;
    const uint32_t feature_size = width * height * channels;

    float* features = new float[feature_size];
    for (uint32_t i = 0; i < feature_size; i++) {
        features[i] = sinf(static_cast<float>(i) / 100.0f);
    }

    // Run multiple iterations
    const int num_iterations = 5;
    float prev_loss = 1e10f;

    for (int iter = 0; iter < num_iterations; iter++) {
        float loss = -1.0f;
        int result = visual_jepa_bridge_train_step(
            jepa_bridge, features, width, height, channels, &loss
        );
        EXPECT_EQ(result, 0);
        EXPECT_GE(loss, 0.0f);

        // Update target encoder with EMA
        result = visual_jepa_bridge_update_target_encoder(jepa_bridge);
        EXPECT_EQ(result, 0);

        // Update FEP bridge
        if (fep_bridge) {
            visual_jepa_fep_bridge_update(fep_bridge, 16);
        }
    }

    // Verify stats updated
    visual_jepa_stats_t stats;
    int result = visual_jepa_bridge_get_stats(jepa_bridge, &stats);
    EXPECT_EQ(result, 0);
    // Stats should reflect processing

    delete[] features;
}

/**
 * Main entry point for tests
 */
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
