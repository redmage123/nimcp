/**
 * @file test_neuromod_nucleus_adapter.cpp
 * @brief Unit tests for Neuromodulator Nucleus Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/neuromod/nimcp_neuromod_nucleus_adapter.h"

class NeuromodNucleusAdapterTest : public ::testing::Test {
protected:
    nimcp_nucleus_adapter_t adapter;
    nimcp_nucleus_adapter_config_t config;

    void SetUp() override {
        config = nimcp_nucleus_adapter_default_config(NUCLEUS_VTA);
        adapter = nimcp_nucleus_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_nucleus_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_nucleus_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(NeuromodNucleusAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_nucleus_adapter_config_t default_config = nimcp_nucleus_adapter_default_config(NUCLEUS_VTA);
    EXPECT_GT(default_config.num_neurons, 0u);
}

TEST_F(NeuromodNucleusAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_nucleus_adapter_t adapter_null = nimcp_nucleus_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_nucleus_adapter_destroy(adapter_null);
}

TEST_F(NeuromodNucleusAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_nucleus_adapter_destroy(NULL);
}

TEST_F(NeuromodNucleusAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_nucleus_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(NeuromodNucleusAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_nucleus_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(NeuromodNucleusAdapterTest, TriggerBurstSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nucleus_adapter_trigger_burst(adapter, 0.8f));
}

TEST_F(NeuromodNucleusAdapterTest, GetOutputSucceeds) {
    float output = 0.0f;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nucleus_adapter_get_output(adapter, &output));
}

TEST_F(NeuromodNucleusAdapterTest, GetStateSucceeds) {
    nimcp_nucleus_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nucleus_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(NeuromodNucleusAdapterTest, GetStatsSucceeds) {
    nimcp_nucleus_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nucleus_adapter_get_stats(adapter, &stats));
}

TEST_F(NeuromodNucleusAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nucleus_adapter_reset_stats(adapter));
}
