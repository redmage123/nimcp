/**
 * @file test_hh_dynamics_adapter.cpp
 * @brief Unit tests for HH Dynamics Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/physics/nimcp_hh_dynamics_adapter.h"

class HHDynamicsAdapterTest : public ::testing::Test {
protected:
    nimcp_hh_adapter_t adapter;
    nimcp_hh_adapter_config_t config;

    void SetUp() override {
        config = nimcp_hh_adapter_default_config();
        adapter = nimcp_hh_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_hh_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_hh_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(HHDynamicsAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_hh_adapter_config_t default_config = nimcp_hh_adapter_default_config();
    EXPECT_GT(default_config.num_neurons, 0u);
}

TEST_F(HHDynamicsAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_hh_adapter_t adapter_null = nimcp_hh_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_hh_adapter_destroy(adapter_null);
}

TEST_F(HHDynamicsAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_hh_adapter_destroy(NULL);
}

TEST_F(HHDynamicsAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_hh_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(HHDynamicsAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_hh_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(HHDynamicsAdapterTest, InjectCurrentSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hh_adapter_inject_current(adapter, 0, 10.0f));
}

TEST_F(HHDynamicsAdapterTest, InjectCurrentNullFails) {
    EXPECT_NE(NIMCP_LAYER_OK, nimcp_hh_adapter_inject_current(NULL, 0, 10.0f));
}

TEST_F(HHDynamicsAdapterTest, GetStateSucceeds) {
    nimcp_hh_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hh_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(HHDynamicsAdapterTest, GetStatsSucceeds) {
    nimcp_hh_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hh_adapter_get_stats(adapter, &stats));
}

TEST_F(HHDynamicsAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_hh_adapter_reset_stats(adapter));
}
