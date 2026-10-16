/**
 * @file test_neuromod_pool_adapter.cpp
 * @brief Unit tests for Neuromodulator Pool Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/neuromod/nimcp_neuromod_pool_adapter.h"

class NeuromodPoolAdapterTest : public ::testing::Test {
protected:
    nimcp_neuromod_pool_adapter_t adapter;
    nimcp_neuromod_pool_config_t config;

    void SetUp() override {
        config = nimcp_neuromod_pool_adapter_default_config();
        adapter = nimcp_neuromod_pool_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_neuromod_pool_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_neuromod_pool_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(NeuromodPoolAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_neuromod_pool_config_t default_config = nimcp_neuromod_pool_adapter_default_config();
    // Config has basal_levels array - check dopamine baseline is valid
    EXPECT_GE(default_config.basal_levels[NMOD_DOPAMINE], 0.0f);
    EXPECT_LE(default_config.basal_levels[NMOD_DOPAMINE], 1.0f);
}

TEST_F(NeuromodPoolAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_neuromod_pool_adapter_t adapter_null = nimcp_neuromod_pool_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_neuromod_pool_adapter_destroy(adapter_null);
}

TEST_F(NeuromodPoolAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_neuromod_pool_adapter_destroy(NULL);
}

TEST_F(NeuromodPoolAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_neuromod_pool_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(NeuromodPoolAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_neuromod_pool_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(NeuromodPoolAdapterTest, ReleaseSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_neuromod_pool_adapter_release(adapter, NMOD_DOPAMINE, 0.5f));
}

TEST_F(NeuromodPoolAdapterTest, GetLevelSucceeds) {
    float level = 0.0f;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_neuromod_pool_adapter_get_level(adapter, NMOD_DOPAMINE, &level));
}

TEST_F(NeuromodPoolAdapterTest, GetStateSucceeds) {
    nimcp_neuromod_pool_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_neuromod_pool_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(NeuromodPoolAdapterTest, GetStatsSucceeds) {
    nimcp_neuromod_pool_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_neuromod_pool_adapter_get_stats(adapter, &stats));
}

TEST_F(NeuromodPoolAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_neuromod_pool_adapter_reset_stats(adapter));
}
