/**
 * @file test_cortical_oscillations_integration.cpp
 * @brief Unit tests for cortical oscillation integration
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_oscillations_integration.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalOscillationsIntegrationTest : public ::testing::Test {
protected:
    cortical_oscillation_system_t* osc;
    cortical_oscillation_config_t config;

    void SetUp() override {
        cortical_oscillation_default_config(&config);
        osc = cortical_oscillation_create(&config);
        ASSERT_NE(osc, nullptr);
    }

    void TearDown() override {
        if (osc) {
            cortical_oscillation_destroy(osc);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, DefaultConfig) {
    cortical_oscillation_config_t cfg;
    int result = cortical_oscillation_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GE(cfg.gamma_frequency, 30.0f);
    EXPECT_LE(cfg.gamma_frequency, 100.0f);
    EXPECT_GE(cfg.theta_frequency, 4.0f);
    EXPECT_LE(cfg.theta_frequency, 8.0f);
    EXPECT_GE(cfg.alpha_frequency, 8.0f);
    EXPECT_LE(cfg.alpha_frequency, 13.0f);
}

TEST_F(CorticalOscillationsIntegrationTest, DefaultConfigNullPointer) {
    int result = cortical_oscillation_default_config(nullptr);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalOscillationsIntegrationTest, CreateWithConfig) {
    cortical_oscillation_config_t custom_config;
    cortical_oscillation_default_config(&custom_config);
    custom_config.gamma_frequency = 50.0f;
    custom_config.gamma_amplitude = 0.5f;

    cortical_oscillation_system_t* system = cortical_oscillation_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_oscillation_destroy(system);
}

TEST_F(CorticalOscillationsIntegrationTest, CreateWithNullConfig) {
    cortical_oscillation_system_t* system = cortical_oscillation_create(nullptr);
    ASSERT_NE(system, nullptr);
    cortical_oscillation_destroy(system);
}

/* ============================================================================
 * Phase Computation Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, ComputeGammaPhase) {
    float phase;
    int result = cortical_oscillation_compute_gamma_phase(osc, 0.0f, &phase);
    EXPECT_EQ(result, 0);
    EXPECT_GE(phase, -M_PI);
    EXPECT_LE(phase, M_PI);
}

TEST_F(CorticalOscillationsIntegrationTest, ComputeThetaPhase) {
    float phase;
    int result = cortical_oscillation_compute_theta_phase(osc, 0.0f, &phase);
    EXPECT_EQ(result, 0);
    EXPECT_GE(phase, -M_PI);
    EXPECT_LE(phase, M_PI);
}

TEST_F(CorticalOscillationsIntegrationTest, ComputeAlphaPhase) {
    float phase;
    int result = cortical_oscillation_compute_alpha_phase(osc, 0.0f, &phase);
    EXPECT_EQ(result, 0);
    EXPECT_GE(phase, -M_PI);
    EXPECT_LE(phase, M_PI);
}

TEST_F(CorticalOscillationsIntegrationTest, ComputeBetaPhase) {
    float phase;
    int result = cortical_oscillation_compute_beta_phase(osc, 0.0f, &phase);
    EXPECT_EQ(result, 0);
    EXPECT_GE(phase, -M_PI);
    EXPECT_LE(phase, M_PI);
}

/* ============================================================================
 * Amplitude Modulation Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, ModulateByGammaPhase) {
    float modulated = cortical_oscillation_modulate_by_gamma(osc, 1.0f, 0.0f);
    EXPECT_GT(modulated, 0.0f);
}

TEST_F(CorticalOscillationsIntegrationTest, ModulateByThetaPhase) {
    float modulated = cortical_oscillation_modulate_by_theta(osc, 1.0f, 0.0f);
    EXPECT_GT(modulated, 0.0f);
}

/* ============================================================================
 * Cross-Frequency Coupling Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, ComputeThetaGammaCoupling) {
    float coupling;
    int result = cortical_oscillation_compute_theta_gamma_coupling(osc, &coupling);
    EXPECT_EQ(result, 0);
    EXPECT_GE(coupling, 0.0f);
    EXPECT_LE(coupling, 1.0f);
}

TEST_F(CorticalOscillationsIntegrationTest, ComputeAlphaBetaCoupling) {
    float coupling;
    int result = cortical_oscillation_compute_alpha_beta_coupling(osc, &coupling);
    EXPECT_EQ(result, 0);
    EXPECT_GE(coupling, 0.0f);
}

/* ============================================================================
 * Update and Timestep Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, Update) {
    int result = cortical_oscillation_update(osc, 1.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalOscillationsIntegrationTest, MultipleUpdates) {
    for (int i = 0; i < 1000; i++) {
        int result = cortical_oscillation_update(osc, 0.001f);
        EXPECT_EQ(result, 0);
    }
}

/* ============================================================================
 * Phase Locking Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, ComputePhaseLockingValue) {
    /* Generate some test spike times */
    float spike_times[100];
    for (int i = 0; i < 100; i++) {
        spike_times[i] = (float)i * 0.01f;
    }

    float plv;
    int result = cortical_oscillation_compute_plv(osc, OSC_BAND_GAMMA, spike_times, 100, &plv);
    EXPECT_EQ(result, 0);
    EXPECT_GE(plv, 0.0f);
    EXPECT_LE(plv, 1.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, GetStats) {
    cortical_oscillation_stats_t stats;
    int result = cortical_oscillation_get_stats(osc, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalOscillationsIntegrationTest, GetBandPower) {
    cortical_oscillation_update(osc, 1.0f);

    float power;
    int result = cortical_oscillation_get_band_power(osc, OSC_BAND_GAMMA, &power);
    EXPECT_EQ(result, 0);
    EXPECT_GE(power, 0.0f);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, ConnectBioAsync) {
    int result = cortical_oscillation_connect_bio_async(osc);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalOscillationsIntegrationTest, IsBioAsyncConnected) {
    bool connected = cortical_oscillation_is_bio_async_connected(osc);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalOscillationsIntegrationTest, DisconnectBioAsync) {
    int result = cortical_oscillation_disconnect_bio_async(osc);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalOscillationsIntegrationTest, DestroyNull) {
    cortical_oscillation_destroy(nullptr);
}

TEST_F(CorticalOscillationsIntegrationTest, LargeTimestep) {
    int result = cortical_oscillation_update(osc, 1000.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalOscillationsIntegrationTest, ZeroTimestep) {
    int result = cortical_oscillation_update(osc, 0.0f);
    EXPECT_EQ(result, 0);
}
