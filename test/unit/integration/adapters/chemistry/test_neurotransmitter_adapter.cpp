/**
 * @file test_neurotransmitter_adapter.cpp
 * @brief Unit tests for Neurotransmitter Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/chemistry/nimcp_neurotransmitter_adapter.h"

class NeurotransmitterAdapterTest : public ::testing::Test {
protected:
    nimcp_nt_adapter_t adapter;
    nimcp_nt_adapter_config_t config;

    void SetUp() override {
        config = nimcp_nt_adapter_default_config();
        adapter = nimcp_nt_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_nt_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_nt_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(NeurotransmitterAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_nt_adapter_config_t default_config = nimcp_nt_adapter_default_config();
    EXPECT_GT(default_config.num_synapses, 0u);
}

TEST_F(NeurotransmitterAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_nt_adapter_t adapter_null = nimcp_nt_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_nt_adapter_destroy(adapter_null);
}

TEST_F(NeurotransmitterAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_nt_adapter_destroy(NULL);
}

TEST_F(NeurotransmitterAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_nt_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(NeurotransmitterAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_nt_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(NeurotransmitterAdapterTest, ReleaseSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nt_adapter_release(adapter, 0, 1.0f));
}

TEST_F(NeurotransmitterAdapterTest, GetOccupancySucceeds) {
    float occupancy[64];
    uint32_t count = 0;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nt_adapter_get_occupancy(adapter, occupancy, 64, &count));
}

TEST_F(NeurotransmitterAdapterTest, GetStateSucceeds) {
    nimcp_nt_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nt_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(NeurotransmitterAdapterTest, GetStatsSucceeds) {
    nimcp_nt_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nt_adapter_get_stats(adapter, &stats));
}

TEST_F(NeurotransmitterAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_nt_adapter_reset_stats(adapter));
}
