/**
 * @file test_ephaptic_coupling_adapter.cpp
 * @brief Unit tests for Ephaptic Coupling Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/physics/nimcp_ephaptic_coupling_adapter.h"

class EphapticCouplingAdapterTest : public ::testing::Test {
protected:
    nimcp_ephaptic_adapter_t adapter;
    nimcp_ephaptic_adapter_config_t config;

    void SetUp() override {
        config = nimcp_ephaptic_adapter_default_config();
        adapter = nimcp_ephaptic_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_ephaptic_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_ephaptic_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(EphapticCouplingAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_ephaptic_adapter_config_t default_config = nimcp_ephaptic_adapter_default_config();
    EXPECT_GT(default_config.num_neurons, 0u);
}

TEST_F(EphapticCouplingAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_ephaptic_adapter_t adapter_null = nimcp_ephaptic_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_ephaptic_adapter_destroy(adapter_null);
}

TEST_F(EphapticCouplingAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_ephaptic_adapter_destroy(NULL);
}

TEST_F(EphapticCouplingAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_ephaptic_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(EphapticCouplingAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_ephaptic_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(EphapticCouplingAdapterTest, GetLFPSucceeds) {
    float lfp = 0.0f;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_ephaptic_adapter_get_lfp(adapter, &lfp));
}

TEST_F(EphapticCouplingAdapterTest, GetStateSucceeds) {
    nimcp_ephaptic_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_ephaptic_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(EphapticCouplingAdapterTest, GetStatsSucceeds) {
    nimcp_ephaptic_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_ephaptic_adapter_get_stats(adapter, &stats));
}

TEST_F(EphapticCouplingAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_ephaptic_adapter_reset_stats(adapter));
}
