/**
 * @file test_snn_visual_bridge.cpp
 * @brief Unit tests for SNN Visual Cortex Bridge
 *
 * Tests the bidirectional integration between SNN and visual cortex,
 * covering encoding, decoding, attention modulation, and bio-async.
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include <gtest/gtest.h>
#include "snn/bridges/nimcp_snn_visual_bridge.h"
#include "snn/nimcp_snn_network.h"
#include "perception/nimcp_visual_cortex.h"

class SNNVisualBridgeTest : public ::testing::Test {
protected:
    snn_visual_bridge_t* bridge;
    snn_network_t* snn;
    visual_cortex_t* visual_cortex;
    snn_visual_config_t config;

    void SetUp() override {
        bridge = nullptr;
        snn = nullptr;
        visual_cortex = nullptr;

        // Initialize config with defaults
        snn_visual_config_default(&config);

        // Override with smaller dimensions for testing (avoid huge allocations)
        config.frame_width = 32;
        config.frame_height = 32;
    }

    void TearDown() override {
        if (bridge) {
            snn_visual_bridge_destroy(bridge);
        }
        if (visual_cortex) {
            visual_cortex_destroy(visual_cortex);
        }
        if (snn) {
            snn_network_destroy(snn);
        }
    }

    void CreateMinimalSNN() {
        /* Create visual cortex first if not created */
        if (!visual_cortex) {
            CreateMinimalVisualCortex();
        }

        snn_config_t snn_cfg;
        snn_config_default(&snn_cfg);  /* Initialize with valid defaults (dt, tau, etc.) */
        snn_cfg.n_inputs = config.frame_width * config.frame_height;
        snn_cfg.n_outputs = 128;  /* Match visual cortex feature_dim */
        snn_cfg.n_populations = 2;
        snn = snn_network_create(&snn_cfg);
    }

    void CreateMinimalVisualCortex() {
        visual_cortex_config_t vc_cfg;
        memset(&vc_cfg, 0, sizeof(vc_cfg));
        vc_cfg.input_width = config.frame_width;
        vc_cfg.input_height = config.frame_height;
        vc_cfg.num_v1_filters = 32;
        vc_cfg.feature_dim = 128;
        vc_cfg.enable_attention = true;
        vc_cfg.enable_memory = false;
        visual_cortex = visual_cortex_create(&vc_cfg);
    }
};

// Test 1: Config defaults (note: SetUp overrides frame_width/height for testing)
TEST_F(SNNVisualBridgeTest, ConfigDefaults) {
    // Test original defaults from snn_visual_config_default() directly
    snn_visual_config_t orig_cfg;
    snn_visual_config_default(&orig_cfg);

    EXPECT_EQ(orig_cfg.encoding_method, SNN_ENCODE_RATE);
    EXPECT_GT(orig_cfg.max_spike_rate, 0.0f);
    EXPECT_GT(orig_cfg.min_spike_rate, 0.0f);
    EXPECT_LT(orig_cfg.min_spike_rate, orig_cfg.max_spike_rate);
    EXPECT_GT(orig_cfg.temporal_window_ms, 0.0f);
    EXPECT_EQ(orig_cfg.neurons_per_pixel, 1);
    EXPECT_EQ(orig_cfg.frame_width, 640);
    EXPECT_EQ(orig_cfg.frame_height, 480);
    EXPECT_EQ(orig_cfg.frame_channels, 1);
    EXPECT_TRUE(orig_cfg.use_attention_modulation);

    // config from SetUp has overridden dimensions for testing
    EXPECT_EQ(config.frame_width, 32);
    EXPECT_EQ(config.frame_height, 32);
}

// Test 2: Bridge creation/destruction
TEST_F(SNNVisualBridgeTest, CreateDestroy) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();

    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->snn, snn);
    EXPECT_EQ(bridge->visual_cortex, visual_cortex);
    EXPECT_TRUE(bridge->connected);

    snn_visual_bridge_destroy(bridge);
    bridge = nullptr;  // Prevent double-free in TearDown
}

// Test 3: Frame encoding (grayscale)
TEST_F(SNNVisualBridgeTest, FrameEncodingGrayscale) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    // Create test frame (checkerboard pattern) - use config dimensions
    uint32_t width = config.frame_width, height = config.frame_height;
    uint8_t* frame = (uint8_t*)malloc(width * height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            frame[y * width + x] = ((x / 4) + (y / 4)) % 2 ? 255 : 0;
        }
    }

    snn_spike_train_t* spike_trains = nullptr;
    int ret = snn_visual_bridge_encode(bridge, frame, width, height, 1, &spike_trains);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(spike_trains, nullptr);

    free(frame);
}

// Test 4: Frame encoding (RGB)
TEST_F(SNNVisualBridgeTest, FrameEncodingRGB) {
    config.frame_channels = 3;
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    // Use config dimensions
    uint32_t width = config.frame_width, height = config.frame_height;
    uint8_t* frame = (uint8_t*)malloc(width * height * 3);
    for (uint32_t i = 0; i < width * height * 3; i++) {
        frame[i] = (uint8_t)(i % 256);
    }

    snn_spike_train_t* spike_trains = nullptr;
    int ret = snn_visual_bridge_encode(bridge, frame, width, height, 3, &spike_trains);
    EXPECT_EQ(ret, 0);

    free(frame);
}

// Test 5: Feature encoding
TEST_F(SNNVisualBridgeTest, FeatureEncoding) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    uint32_t num_features = 128;
    float* features = (float*)malloc(num_features * sizeof(float));
    for (uint32_t i = 0; i < num_features; i++) {
        features[i] = (float)i / num_features;  // Normalized [0, 1]
    }

    snn_spike_train_t* spike_trains = nullptr;
    int ret = snn_visual_bridge_encode_features(bridge, features, num_features, &spike_trains);
    EXPECT_EQ(ret, 0);

    free(features);
}

// Test 6: Spike decoding to frame
TEST_F(SNNVisualBridgeTest, SpikeDecodingToFrame) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    // Create mock spike trains - use config dimensions
    uint32_t width = config.frame_width, height = config.frame_height;
    uint32_t num_trains = width * height;
    snn_spike_train_t* spike_trains = (snn_spike_train_t*)calloc(num_trains, sizeof(snn_spike_train_t));

    uint8_t* frame_out = (uint8_t*)malloc(width * height);
    int ret = snn_visual_bridge_decode(bridge, spike_trains, num_trains, frame_out);
    EXPECT_EQ(ret, 0);

    free(frame_out);
    free(spike_trains);
}

// Test 7: Spike decoding to features
TEST_F(SNNVisualBridgeTest, SpikeDecodingToFeatures) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    uint32_t num_trains = 128;
    snn_spike_train_t* spike_trains = (snn_spike_train_t*)calloc(num_trains, sizeof(snn_spike_train_t));

    float* features_out = (float*)malloc(num_trains * sizeof(float));
    int ret = snn_visual_bridge_decode_features(bridge, spike_trains, num_trains, features_out, num_trains);
    EXPECT_EQ(ret, 0);

    free(features_out);
    free(spike_trains);
}

// Test 8: Attention modulation
TEST_F(SNNVisualBridgeTest, AttentionModulation) {
    config.use_attention_modulation = true;
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_visual_bridge_update_attention(bridge);
    EXPECT_EQ(ret, 0);

    // Check that attention gains were computed
    EXPECT_NE(bridge->attention_gains, nullptr);
}

// Test 9: Downsampling
TEST_F(SNNVisualBridgeTest, Downsampling) {
    config.downsample_frames = true;
    config.downsample_factor = 2;
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    // Use config dimensions (bridge validates frame matches config)
    uint32_t width = config.frame_width, height = config.frame_height;
    uint8_t* frame = (uint8_t*)malloc(width * height);
    memset(frame, 128, width * height);

    snn_spike_train_t* spike_trains = nullptr;
    int ret = snn_visual_bridge_encode(bridge, frame, width, height, 1, &spike_trains);
    EXPECT_EQ(ret, 0);

    // Verify downsampling config is enabled
    EXPECT_TRUE(config.downsample_frames);
    EXPECT_EQ(config.downsample_factor, 2);

    free(frame);
}

// Test 10: Bio-async connection
TEST_F(SNNVisualBridgeTest, BioAsyncConnection) {
    config.enable_bio_async = true;
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    int ret = snn_visual_bridge_connect_bio_async(bridge);
    // Bio-async may not be available in test environment
    // Just check that function doesn't crash
    EXPECT_GE(ret, -1);  // Allow failure but not crash

    bool connected = snn_visual_bridge_is_bio_async_connected(bridge);
    // If bio-async connected, should return true
    if (ret == 0) {
        EXPECT_TRUE(connected);
    }
}

// Test 11: Statistics tracking
TEST_F(SNNVisualBridgeTest, StatisticsTracking) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    snn_visual_encode_stats_t stats;
    int ret = snn_visual_bridge_get_encode_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.frames_encoded, 0);

    snn_visual_bridge_reset_stats(bridge);
    ret = snn_visual_bridge_get_encode_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.frames_encoded, 0);
}

// Test 12: Update cycle
TEST_F(SNNVisualBridgeTest, UpdateCycle) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    float dt = 1.0f;  // 1ms timestep
    int ret = snn_visual_bridge_update(bridge, dt);
    EXPECT_EQ(ret, 0);
}

// Test 13: Null pointer handling
TEST_F(SNNVisualBridgeTest, NullPointerHandling) {
    // Test null config
    EXPECT_EQ(snn_visual_bridge_create(nullptr, snn, visual_cortex), nullptr);

    // Test null snn
    EXPECT_EQ(snn_visual_bridge_create(&config, nullptr, visual_cortex), nullptr);

    // Test null visual cortex
    EXPECT_EQ(snn_visual_bridge_create(&config, snn, nullptr), nullptr);

    // Test null bridge to destroy (should not crash)
    snn_visual_bridge_destroy(nullptr);
}

// Test 14: Invalid dimensions
TEST_F(SNNVisualBridgeTest, InvalidDimensions) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    uint8_t* frame = (uint8_t*)malloc(100);
    snn_spike_train_t* spike_trains = nullptr;

    // Test with mismatched dimensions
    int ret = snn_visual_bridge_encode(bridge, frame, 0, 100, 1, &spike_trains);
    EXPECT_NE(ret, 0);  // Should fail with width=0

    ret = snn_visual_bridge_encode(bridge, frame, 100, 0, 1, &spike_trains);
    EXPECT_NE(ret, 0);  // Should fail with height=0

    free(frame);
}

// Test 15: Spike rate queries
TEST_F(SNNVisualBridgeTest, SpikeRateQueries) {
    CreateMinimalSNN();
    CreateMinimalVisualCortex();
    bridge = snn_visual_bridge_create(&config, snn, visual_cortex);
    ASSERT_NE(bridge, nullptr);

    // Query spike rate for a pixel
    float rate = snn_visual_bridge_get_spike_rate(bridge, 0);
    EXPECT_GE(rate, 0.0f);  // Rate should be non-negative

    // Check bridge is active
    bool active = snn_visual_bridge_is_active(bridge);
    EXPECT_TRUE(active);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
