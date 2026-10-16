/**
 * @file test_calcium_adapter.cpp
 * @brief Unit tests for Calcium Signaling Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/chemistry/nimcp_calcium_adapter.h"

class CalciumAdapterTest : public ::testing::Test {
protected:
    nimcp_calcium_adapter_t adapter;
    nimcp_calcium_adapter_config_t config;

    void SetUp() override {
        config = nimcp_calcium_adapter_default_config();
        adapter = nimcp_calcium_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_calcium_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_calcium_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(CalciumAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_calcium_adapter_config_t default_config = nimcp_calcium_adapter_default_config();
    EXPECT_GT(default_config.num_compartments, 0u);
}

TEST_F(CalciumAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_calcium_adapter_t adapter_null = nimcp_calcium_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_calcium_adapter_destroy(adapter_null);
}

TEST_F(CalciumAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_calcium_adapter_destroy(NULL);
}

TEST_F(CalciumAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_calcium_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(CalciumAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_calcium_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(CalciumAdapterTest, InfluxSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_calcium_adapter_influx(adapter, 0, 0.5f));
}

TEST_F(CalciumAdapterTest, GetKinaseActivitySucceeds) {
    float camkii, calcineurin;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_calcium_adapter_get_kinase_activity(adapter, &camkii, &calcineurin));
}

TEST_F(CalciumAdapterTest, GetStateSucceeds) {
    nimcp_calcium_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_calcium_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(CalciumAdapterTest, GetStatsSucceeds) {
    nimcp_calcium_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_calcium_adapter_get_stats(adapter, &stats));
}

TEST_F(CalciumAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_calcium_adapter_reset_stats(adapter));
}
