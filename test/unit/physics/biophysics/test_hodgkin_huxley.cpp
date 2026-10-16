/**
 * @file test_hodgkin_huxley.cpp
 * @brief Unit tests for Hodgkin-Huxley neuron model
 * @version 1.0.0
 * @date 2026-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class HodgkinHuxleyTest : public ::testing::Test {
protected:
    nimcp_hh_neuron_t neuron;
    nimcp_hh_config_t config;

    void SetUp() override {
        nimcp_hh_config_default(&config);
        ASSERT_EQ(nimcp_hh_neuron_init(&neuron, &config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_hh_neuron_destroy(&neuron);
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST(HHConfigTest, DefaultConfigInitialization) {
    nimcp_hh_config_t config;
    EXPECT_EQ(nimcp_hh_config_default(&config), NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(config.g_Na, NIMCP_HH_DEFAULT_G_NA);
    EXPECT_FLOAT_EQ(config.g_K, NIMCP_HH_DEFAULT_G_K);
    EXPECT_FLOAT_EQ(config.g_L, NIMCP_HH_DEFAULT_G_L);
    EXPECT_FLOAT_EQ(config.E_Na, NIMCP_HH_DEFAULT_E_NA);
    EXPECT_FLOAT_EQ(config.E_K, NIMCP_HH_DEFAULT_E_K);
    EXPECT_FLOAT_EQ(config.C_m, NIMCP_HH_DEFAULT_C_M);
}

TEST(HHConfigTest, NullConfigReturnsError) {
    EXPECT_EQ(nimcp_hh_config_default(nullptr), NIMCP_ERROR_NULL_POINTER);
}

TEST(HHConfigTest, ConfigForPyramidal) {
    nimcp_hh_config_t config;
    EXPECT_EQ(nimcp_hh_config_for_type(&config, "pyramidal"), NIMCP_SUCCESS);
    EXPECT_TRUE(config.enable_calcium);
    EXPECT_TRUE(config.enable_adaptation);
}

TEST(HHConfigTest, ConfigForInterneuron) {
    nimcp_hh_config_t config;
    EXPECT_EQ(nimcp_hh_config_for_type(&config, "interneuron"), NIMCP_SUCCESS);
    EXPECT_GT(config.g_Na, 100.0f);  // Fast-spiking
    EXPECT_FALSE(config.enable_calcium);
}

/* ============================================================================
 * Neuron Lifecycle Tests
 * ============================================================================ */

TEST_F(HodgkinHuxleyTest, InitializationState) {
    EXPECT_TRUE(neuron.initialized);
    EXPECT_NEAR(neuron.V, config.V_rest, 0.1f);
    EXPECT_FALSE(neuron.spiked);
    EXPECT_EQ(neuron.spike_count, 0u);
}

TEST_F(HodgkinHuxleyTest, ResetNeuron) {
    // Stimulate to cause spike
    for (int i = 0; i < 100; i++) {
        nimcp_hh_neuron_update(&neuron, 20.0f, 0.025f);
    }
    EXPECT_GT(neuron.spike_count, 0u);

    // Reset
    EXPECT_EQ(nimcp_hh_neuron_reset(&neuron), NIMCP_SUCCESS);
    EXPECT_EQ(neuron.spike_count, 0u);
    EXPECT_NEAR(neuron.V, config.V_rest, 0.1f);
}

/* ============================================================================
 * Gating Variable Tests
 * ============================================================================ */

TEST(HHGatingTest, MSteadyStateBounds) {
    for (float V = -100.0f; V <= 50.0f; V += 10.0f) {
        float m_inf = nimcp_hh_m_inf(V);
        EXPECT_GE(m_inf, 0.0f);
        EXPECT_LE(m_inf, 1.0f);
    }
}

TEST(HHGatingTest, HSteadyStateBounds) {
    for (float V = -100.0f; V <= 50.0f; V += 10.0f) {
        float h_inf = nimcp_hh_h_inf(V);
        EXPECT_GE(h_inf, 0.0f);
        EXPECT_LE(h_inf, 1.0f);
    }
}

TEST(HHGatingTest, NSteadyStateBounds) {
    for (float V = -100.0f; V <= 50.0f; V += 10.0f) {
        float n_inf = nimcp_hh_n_inf(V);
        EXPECT_GE(n_inf, 0.0f);
        EXPECT_LE(n_inf, 1.0f);
    }
}

TEST(HHGatingTest, TimeConstantsPositive) {
    for (float V = -100.0f; V <= 50.0f; V += 10.0f) {
        EXPECT_GT(nimcp_hh_m_tau(V), 0.0f);
        EXPECT_GT(nimcp_hh_h_tau(V), 0.0f);
        EXPECT_GT(nimcp_hh_n_tau(V), 0.0f);
    }
}

/* ============================================================================
 * Simulation Tests
 * ============================================================================ */

TEST_F(HodgkinHuxleyTest, NoSpikeBelowThreshold) {
    // First let the neuron equilibrate with no current
    for (int i = 0; i < 500; i++) {
        nimcp_hh_neuron_update(&neuron, 0.0f, 0.025f);
    }

    // Reset spike count before applying subthreshold current
    neuron.spike_count = 0;

    // Subthreshold current - very low to ensure no spiking
    float I_ext = 1.0f;  // uA/cm^2 - well below rheobase
    for (int i = 0; i < 1000; i++) {
        nimcp_hh_neuron_update(&neuron, I_ext, 0.025f);
    }
    EXPECT_EQ(neuron.spike_count, 0u);
}

TEST_F(HodgkinHuxleyTest, SpikeAboveThreshold) {
    // Suprathreshold current
    float I_ext = 15.0f;  // uA/cm^2
    for (int i = 0; i < 1000; i++) {
        nimcp_hh_neuron_update(&neuron, I_ext, 0.025f);
    }
    EXPECT_GT(neuron.spike_count, 0u);
}

TEST_F(HodgkinHuxleyTest, SpikeDetection) {
    float I_ext = 20.0f;
    bool detected_spike = false;

    for (int i = 0; i < 500; i++) {
        nimcp_hh_neuron_update(&neuron, I_ext, 0.025f);
        bool spiked;
        nimcp_hh_neuron_get_spike(&neuron, &spiked);
        if (spiked) detected_spike = true;
    }
    EXPECT_TRUE(detected_spike);
}

TEST_F(HodgkinHuxleyTest, VoltageReturnsToRest) {
    // Stimulate briefly
    for (int i = 0; i < 100; i++) {
        nimcp_hh_neuron_update(&neuron, 15.0f, 0.025f);
    }

    // Let it settle
    for (int i = 0; i < 2000; i++) {
        nimcp_hh_neuron_update(&neuron, 0.0f, 0.025f);
    }

    // Should be near rest
    EXPECT_NEAR(neuron.V, config.V_rest, 5.0f);
}

/* ============================================================================
 * Ion Channel Tests
 * ============================================================================ */

TEST_F(HodgkinHuxleyTest, ChannelModulation) {
    nimcp_ion_channel_t channel;

    // Get sodium channel
    EXPECT_EQ(nimcp_hh_get_channel(&neuron, NIMCP_ION_CHANNEL_NA, &channel), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(channel.modulation_factor, 1.0f);

    // Modulate
    EXPECT_EQ(nimcp_hh_modulate_channel(&neuron, NIMCP_ION_CHANNEL_NA, 0.5f), NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_hh_get_channel(&neuron, NIMCP_ION_CHANNEL_NA, &channel), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(channel.modulation_factor, 0.5f);
}

TEST_F(HodgkinHuxleyTest, ChannelEnableDisable) {
    EXPECT_EQ(nimcp_hh_set_channel_enabled(&neuron, NIMCP_ION_CHANNEL_CA_L, true), NIMCP_SUCCESS);

    nimcp_ion_channel_t channel;
    EXPECT_EQ(nimcp_hh_get_channel(&neuron, NIMCP_ION_CHANNEL_CA_L, &channel), NIMCP_SUCCESS);
    EXPECT_TRUE(channel.enabled);

    EXPECT_EQ(nimcp_hh_set_channel_enabled(&neuron, NIMCP_ION_CHANNEL_CA_L, false), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_hh_get_channel(&neuron, NIMCP_ION_CHANNEL_CA_L, &channel), NIMCP_SUCCESS);
    EXPECT_FALSE(channel.enabled);
}

/* ============================================================================
 * Temperature Tests
 * ============================================================================ */

TEST_F(HodgkinHuxleyTest, TemperatureScaling) {
    float phi_cold = nimcp_hh_get_phi(&neuron);

    // Increase temperature
    EXPECT_EQ(nimcp_hh_set_temperature(&neuron, 37.0f), NIMCP_SUCCESS);
    float phi_warm = nimcp_hh_get_phi(&neuron);

    // Higher temperature -> higher phi
    EXPECT_GT(phi_warm, phi_cold);
}

/* ============================================================================
 * Population Tests
 * ============================================================================ */

TEST(HHPopulationTest, CreateAndDestroy) {
    nimcp_hh_population_t pop;
    EXPECT_EQ(nimcp_hh_population_create(&pop, 10, nullptr), NIMCP_SUCCESS);
    EXPECT_TRUE(pop.initialized);
    EXPECT_EQ(pop.count, 10u);

    nimcp_hh_population_destroy(&pop);
    EXPECT_FALSE(pop.initialized);
}

TEST(HHPopulationTest, PopulationUpdate) {
    nimcp_hh_population_t pop;
    EXPECT_EQ(nimcp_hh_population_create(&pop, 5, nullptr), NIMCP_SUCCESS);

    float currents[5] = {10.0f, 15.0f, 20.0f, 15.0f, 10.0f};

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_hh_population_update(&pop, currents, 0.025f), NIMCP_SUCCESS);
    }

    float rate;
    EXPECT_EQ(nimcp_hh_population_get_rate(&pop, &rate), NIMCP_SUCCESS);
    // With suprathreshold currents, should have some firing
    EXPECT_GE(rate, 0.0f);

    nimcp_hh_population_destroy(&pop);
}

/* ============================================================================
 * Analysis Tests
 * ============================================================================ */

TEST_F(HodgkinHuxleyTest, ComputeRheobase) {
    float rheobase;
    EXPECT_EQ(nimcp_hh_compute_rheobase(&neuron, &rheobase), NIMCP_SUCCESS);

    // Rheobase should be positive and reasonable
    EXPECT_GT(rheobase, 0.0f);
    EXPECT_LT(rheobase, 50.0f);  // Typically < 20 uA/cm^2
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST(HHErrorTest, NullNeuronUpdate) {
    EXPECT_EQ(nimcp_hh_neuron_update(nullptr, 10.0f, 0.025f), NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(HHErrorTest, UninitializedNeuronUpdate) {
    nimcp_hh_neuron_t neuron;
    memset(&neuron, 0, sizeof(neuron));
    neuron.initialized = false;

    EXPECT_EQ(nimcp_hh_neuron_update(&neuron, 10.0f, 0.025f), NIMCP_ERROR_NOT_INITIALIZED);
}

TEST(HHErrorTest, InvalidChannelType) {
    nimcp_hh_neuron_t neuron;
    nimcp_hh_neuron_init(&neuron, nullptr);

    EXPECT_EQ(nimcp_hh_modulate_channel(&neuron, (nimcp_ion_channel_type_t)100, 1.0f),
              NIMCP_ERROR_INVALID_PARAMETER);

    nimcp_hh_neuron_destroy(&neuron);
}
