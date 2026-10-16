/**
 * @file test_metabolic_adapter.cpp
 * @brief Unit tests for Metabolic Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/chemistry/nimcp_metabolic_adapter.h"

class MetabolicAdapterTest : public ::testing::Test {
protected:
    nimcp_metabolic_adapter_t adapter;
    nimcp_metabolic_adapter_config_t config;

    void SetUp() override {
        config = nimcp_metabolic_adapter_default_config();
        adapter = nimcp_metabolic_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_metabolic_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_metabolic_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(MetabolicAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_metabolic_adapter_config_t default_config = nimcp_metabolic_adapter_default_config();
    EXPECT_GT(default_config.num_compartments, 0u);
}

TEST_F(MetabolicAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_metabolic_adapter_t adapter_null = nimcp_metabolic_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_metabolic_adapter_destroy(adapter_null);
}

TEST_F(MetabolicAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_metabolic_adapter_destroy(NULL);
}

TEST_F(MetabolicAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_metabolic_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(MetabolicAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_metabolic_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(MetabolicAdapterTest, ConsumeATPSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_metabolic_adapter_consume_atp(adapter, 0, 0.1f));
}

TEST_F(MetabolicAdapterTest, GetEnergyChargeSucceeds) {
    float charge = 0.0f;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_metabolic_adapter_get_energy_charge(adapter, &charge));
    EXPECT_GE(charge, 0.0f);
    EXPECT_LE(charge, 1.0f);
}

TEST_F(MetabolicAdapterTest, GetStateSucceeds) {
    nimcp_metabolic_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_metabolic_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(MetabolicAdapterTest, GetStatsSucceeds) {
    nimcp_metabolic_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_metabolic_adapter_get_stats(adapter, &stats));
}

TEST_F(MetabolicAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_metabolic_adapter_reset_stats(adapter));
}
