/**
 * @file test_auditory_adapter.cpp
 * @brief Unit tests for Auditory Adapter
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "integration/adapters/sensory/nimcp_auditory_adapter.h"

class AuditoryAdapterTest : public ::testing::Test {
protected:
    nimcp_auditory_adapter_t adapter;
    nimcp_auditory_adapter_config_t config;

    void SetUp() override {
        config = nimcp_auditory_adapter_default_config();
        adapter = nimcp_auditory_adapter_create(&config);
        ASSERT_NE(nullptr, adapter);

        nimcp_module_interface_t* iface = nimcp_auditory_adapter_get_interface(adapter);
        ASSERT_NE(nullptr, iface);
        ASSERT_EQ(NIMCP_LAYER_OK, iface->init(adapter, &config));
    }

    void TearDown() override {
        nimcp_auditory_adapter_destroy(adapter);
        adapter = nullptr;
    }
};

TEST_F(AuditoryAdapterTest, DefaultConfigHasReasonableValues) {
    nimcp_auditory_adapter_config_t default_config = nimcp_auditory_adapter_default_config();
    EXPECT_GT(default_config.sample_rate, 0u);
    EXPECT_GT(default_config.num_frequency_bands, 0u);
}

TEST_F(AuditoryAdapterTest, CreateWithNullConfigUsesDefaults) {
    nimcp_auditory_adapter_t adapter_null = nimcp_auditory_adapter_create(NULL);
    ASSERT_NE(nullptr, adapter_null);
    nimcp_auditory_adapter_destroy(adapter_null);
}

TEST_F(AuditoryAdapterTest, DestroyNullDoesNotCrash) {
    nimcp_auditory_adapter_destroy(NULL);
}

TEST_F(AuditoryAdapterTest, GetInterfaceReturnsValid) {
    nimcp_module_interface_t* iface = nimcp_auditory_adapter_get_interface(adapter);
    ASSERT_NE(nullptr, iface);
}

TEST_F(AuditoryAdapterTest, UpdateProcessesSuccessfully) {
    nimcp_module_interface_t* iface = nimcp_auditory_adapter_get_interface(adapter);
    EXPECT_EQ(NIMCP_LAYER_OK, iface->update(adapter, 0.001f));
}

TEST_F(AuditoryAdapterTest, ProcessSamplesSucceeds) {
    float samples[1024];
    for (int i = 0; i < 1024; i++) samples[i] = sinf(i * 0.1f);
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_auditory_adapter_process_samples(adapter, samples, 1024));
}

TEST_F(AuditoryAdapterTest, GetSpectrumSucceeds) {
    float spectrum[64];
    uint32_t count = 0;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_auditory_adapter_get_spectrum(adapter, spectrum, 64, &count));
}

TEST_F(AuditoryAdapterTest, GetStateSucceeds) {
    nimcp_auditory_adapter_state_t state;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_auditory_adapter_get_state(adapter, &state));
    EXPECT_TRUE(state.is_active);
}

TEST_F(AuditoryAdapterTest, GetStatsSucceeds) {
    nimcp_auditory_adapter_stats_t stats;
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_auditory_adapter_get_stats(adapter, &stats));
}

TEST_F(AuditoryAdapterTest, ResetStatsSucceeds) {
    EXPECT_EQ(NIMCP_LAYER_OK, nimcp_auditory_adapter_reset_stats(adapter));
}
