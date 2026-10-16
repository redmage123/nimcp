/**
 * @file test_visual_adapter.cpp
 * @brief Unit tests for Visual Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/sensory/nimcp_visual_adapter.h"

class VisualAdapterTest : public ::testing::Test {
protected:
    nimcp_visual_adapter_t adapter;
    nimcp_visual_adapter_config_t config;

    void SetUp() override {
        config = nimcp_visual_adapter_default_config();
        adapter = nimcp_visual_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_visual_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_visual_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(VisualAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_visual_adapter_config_t default_config = nimcp_visual_adapter_default_config();
    EXPECT_GT(default_config.width, 0u);
    EXPECT_GT(default_config.height, 0u);
}

TEST_F(VisualAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_visual_adapter_t adapter_null = nimcp_visual_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_visual_adapter_destroy(adapter_null);
}

TEST_F(VisualAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_visual_adapter_destroy(NULL);
}

TEST_F(VisualAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_visual_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(VisualAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_visual_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(VisualAdapterTest, ProcessFrameSucceeds) {
    float pixels[64 * 64];
    memset(pixels, 0, sizeof(pixels));
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_visual_adapter_process_frame(adapter, pixels, 64 * 64));
}

TEST_F(VisualAdapterTest, GetFeaturesSucceeds) {
    float features[256];
    uint32_t count = 0;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_visual_adapter_get_features(adapter, features, 256, &count));
}

TEST_F(VisualAdapterTest, GetStateSucceeds) {
    nimcp_visual_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_visual_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(VisualAdapterTest, GetStatsSucceeds) {
    nimcp_visual_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_visual_adapter_get_stats(adapter, &stats));
}

TEST_F(VisualAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_visual_adapter_reset_stats(adapter));
}
