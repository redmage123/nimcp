/**
 * @file test_thermodynamics_adapter.cpp
 * @brief Unit tests for Thermodynamics Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/physics/nimcp_thermodynamics_adapter.h"

class ThermodynamicsAdapterTest : public ::testing::Test {
protected:
    nimcp_thermo_adapter_t adapter;
    nimcp_thermo_adapter_config_t config;

    void SetUp() override {
        config = nimcp_thermo_adapter_default_config();
        adapter = nimcp_thermo_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_thermo_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_thermo_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(ThermodynamicsAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_thermo_adapter_config_t default_config = nimcp_thermo_adapter_default_config();
    EXPECT_GT(default_config.num_regions, 0u);
}

TEST_F(ThermodynamicsAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_thermo_adapter_t adapter_null = nimcp_thermo_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_thermo_adapter_destroy(adapter_null);
}

TEST_F(ThermodynamicsAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_thermo_adapter_destroy(NULL);
}

TEST_F(ThermodynamicsAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_thermo_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(ThermodynamicsAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_thermo_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(ThermodynamicsAdapterTest, AddHeatSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_thermo_adapter_add_heat(adapter, 0, 0.1f));
}

TEST_F(ThermodynamicsAdapterTest, GetKineticScalingSucceeds) {
    float scaling = 0.0f;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_thermo_adapter_get_kinetic_scaling(adapter, &scaling));
    EXPECT_GT(scaling, 0.0f);
}

TEST_F(ThermodynamicsAdapterTest, GetStateSucceeds) {
    nimcp_thermo_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_thermo_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(ThermodynamicsAdapterTest, GetStatsSucceeds) {
    nimcp_thermo_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_thermo_adapter_get_stats(adapter, &stats));
}

TEST_F(ThermodynamicsAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_thermo_adapter_reset_stats(adapter));
}
