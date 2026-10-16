/**
 * @file test_oscillations_fep_bridge.cpp
 * @brief Unit tests for Oscillations-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-oscillations bidirectional integration
 * WHY:  Ensure proper mapping between prediction errors and oscillatory dynamics
 * HOW:  Test lifecycle, connections, FEP→oscillations, oscillations→FEP, analysis
 */

#include <gtest/gtest.h>
#include <cmath>
#include "core/brain/oscillations/nimcp_oscillations_fep_bridge.h"
#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class OscillationsFepBridgeTest : public ::testing::Test {
protected:
    oscillations_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;
    brain_complex_oscillation_state_t* osc_state = nullptr;

    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;

    void SetUp() override {
        // Create oscillations-FEP bridge
        oscillations_fep_config_t config;
        oscillations_fep_bridge_default_config(&config);
        bridge = oscillations_fep_bridge_create(&config);

        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        // Create oscillation state (placeholder allocation)
        osc_state = (brain_complex_oscillation_state_t*)calloc(1, sizeof(brain_complex_oscillation_state_t));
    }

    void TearDown() override {
        if (bridge) {
            oscillations_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (osc_state) {
            free(osc_state);
            osc_state = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(OscillationsFepBridgeTest, CreateWithNullConfig) {
    oscillations_fep_bridge_t* b = oscillations_fep_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    oscillations_fep_bridge_destroy(b);
}

TEST_F(OscillationsFepBridgeTest, DestroyNull) {
    oscillations_fep_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(OscillationsFepBridgeTest, DefaultConfig) {
    oscillations_fep_config_t config;
    int ret = oscillations_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_pe_gamma_coupling);
    EXPECT_TRUE(config.enable_prediction_beta_coupling);
    EXPECT_TRUE(config.enable_precision_alpha_coupling);
    EXPECT_TRUE(config.enable_theta_gamma_pac);
    EXPECT_GT(config.pe_gamma_gain, 0.0f);
    EXPECT_GT(config.prediction_beta_gain, 0.0f);
    EXPECT_GT(config.precision_alpha_gain, 0.0f);
    EXPECT_GT(config.hierarchy_theta_gain, 0.0f);
    EXPECT_GT(config.pac_threshold, 0.0f);
    EXPECT_GT(config.coherence_threshold, 0.0f);
    EXPECT_GT(config.power_adaptation_rate, 0.0f);
    EXPECT_GT(config.phase_adaptation_rate, 0.0f);
}

TEST_F(OscillationsFepBridgeTest, DefaultConfigNullPtr) {
    int ret = oscillations_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, ConnectFEP) {
    int ret = oscillations_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, ConnectFEPNullParams) {
    EXPECT_EQ(oscillations_fep_bridge_connect_fep(nullptr, fep), -1);
    EXPECT_EQ(oscillations_fep_bridge_connect_fep(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, ConnectOscillations) {
    int ret = oscillations_fep_bridge_connect_oscillations(bridge, osc_state);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, ConnectOscillationsNullParams) {
    EXPECT_EQ(oscillations_fep_bridge_connect_oscillations(nullptr, osc_state), -1);
    EXPECT_EQ(oscillations_fep_bridge_connect_oscillations(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, Disconnect) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_bridge_disconnect(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, DisconnectNullPtr) {
    int ret = oscillations_fep_bridge_disconnect(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, ConnectBothSystems) {
    EXPECT_EQ(oscillations_fep_bridge_connect_fep(bridge, fep), 0);
    EXPECT_EQ(oscillations_fep_bridge_connect_oscillations(bridge, osc_state), 0);
}

/* ============================================================================
 * FEP → Oscillations Direction Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, ModulateGammaFromPE) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_modulate_gamma_from_pe(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, ModulateGammaFromPENullPtr) {
    int ret = oscillations_fep_modulate_gamma_from_pe(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, ModulateGammaFromPEWithoutConnection) {
    // Test without connecting systems
    int ret = oscillations_fep_modulate_gamma_from_pe(bridge);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, ModulateBetaFromPredictions) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_modulate_beta_from_predictions(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, ModulateBetaFromPredictionsNullPtr) {
    int ret = oscillations_fep_modulate_beta_from_predictions(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, ModulateAlphaFromPrecision) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_modulate_alpha_from_precision(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, ModulateAlphaFromPrecisionNullPtr) {
    int ret = oscillations_fep_modulate_alpha_from_precision(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, GenerateThetaGammaPAC) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_generate_theta_gamma_pac(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, GenerateThetaGammaPACNullPtr) {
    int ret = oscillations_fep_generate_theta_gamma_pac(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Oscillations → FEP Direction Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, DerivePrecisionFromRatio) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    float precision = 0.0f;
    int ret = oscillations_fep_derive_precision_from_ratio(bridge, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(OscillationsFepBridgeTest, DerivePrecisionFromRatioNullPtr) {
    float precision = 0.0f;
    EXPECT_EQ(oscillations_fep_derive_precision_from_ratio(nullptr, &precision), -1);
    EXPECT_EQ(oscillations_fep_derive_precision_from_ratio(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, DerivePrecisionFromRatioOutputValid) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    float precision = -1.0f;
    int ret = oscillations_fep_derive_precision_from_ratio(bridge, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(std::isnan(precision));
    EXPECT_FALSE(std::isinf(precision));
}

TEST_F(OscillationsFepBridgeTest, WeightErrorsByGamma) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_weight_errors_by_gamma(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, WeightErrorsByGammaNullPtr) {
    int ret = oscillations_fep_weight_errors_by_gamma(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, BindBeliefsViaCoherence) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_bind_beliefs_via_coherence(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, BindBeliefsViaCoherenceNullPtr) {
    int ret = oscillations_fep_bind_beliefs_via_coherence(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Analysis Function Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, ComputeBandPower) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    oscillation_band_power_t band_power;
    int ret = oscillations_fep_compute_band_power(bridge, &band_power);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(band_power.delta, 0.0f);
    EXPECT_GE(band_power.theta, 0.0f);
    EXPECT_GE(band_power.alpha, 0.0f);
    EXPECT_GE(band_power.beta, 0.0f);
    EXPECT_GE(band_power.gamma, 0.0f);
}

TEST_F(OscillationsFepBridgeTest, ComputeBandPowerNullPtr) {
    oscillation_band_power_t band_power;
    EXPECT_EQ(oscillations_fep_compute_band_power(nullptr, &band_power), -1);
    EXPECT_EQ(oscillations_fep_compute_band_power(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, ComputeBandPowerAllBands) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    oscillation_band_power_t band_power;
    int ret = oscillations_fep_compute_band_power(bridge, &band_power);
    EXPECT_EQ(ret, 0);

    // All bands should have valid values
    EXPECT_FALSE(std::isnan(band_power.delta));
    EXPECT_FALSE(std::isnan(band_power.theta));
    EXPECT_FALSE(std::isnan(band_power.alpha));
    EXPECT_FALSE(std::isnan(band_power.beta));
    EXPECT_FALSE(std::isnan(band_power.gamma));
}

TEST_F(OscillationsFepBridgeTest, DetectPAC) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    float pac_strength = 0.0f;
    float preferred_phase = 0.0f;
    int ret = oscillations_fep_detect_pac(bridge, &pac_strength, &preferred_phase);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(pac_strength, 0.0f);
    EXPECT_LE(pac_strength, 1.0f);
    EXPECT_GE(preferred_phase, 0.0f);
}

TEST_F(OscillationsFepBridgeTest, DetectPACNullPtr) {
    float pac_strength = 0.0f;
    float preferred_phase = 0.0f;

    EXPECT_EQ(oscillations_fep_detect_pac(nullptr, &pac_strength, &preferred_phase), -1);
    EXPECT_EQ(oscillations_fep_detect_pac(bridge, nullptr, &preferred_phase), -1);
    EXPECT_EQ(oscillations_fep_detect_pac(bridge, &pac_strength, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, DetectPACOutputValid) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    float pac_strength = -1.0f;
    float preferred_phase = -1.0f;
    int ret = oscillations_fep_detect_pac(bridge, &pac_strength, &preferred_phase);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(std::isnan(pac_strength));
    EXPECT_FALSE(std::isinf(pac_strength));
    EXPECT_FALSE(std::isnan(preferred_phase));
    EXPECT_FALSE(std::isinf(preferred_phase));
}

/* ============================================================================
 * Update Cycle Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, Update) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, UpdateNullPtr) {
    int ret = oscillations_fep_bridge_update(nullptr, 100);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, UpdateMultipleTimes) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    // Run multiple update cycles
    for (int i = 0; i < 10; i++) {
        int ret = oscillations_fep_bridge_update(bridge, 50);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(OscillationsFepBridgeTest, UpdateZeroDelta) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    int ret = oscillations_fep_bridge_update(bridge, 0);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * State/Stats API Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, GetState) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    oscillations_fep_state_t state;
    int ret = oscillations_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.current_gamma, 0.0f);
    EXPECT_GE(state.current_beta, 0.0f);
    EXPECT_GE(state.current_alpha, 0.0f);
    EXPECT_GE(state.current_theta, 0.0f);
    EXPECT_GE(state.gamma_alpha_ratio, 0.0f);
}

TEST_F(OscillationsFepBridgeTest, GetStateNullPtr) {
    oscillations_fep_state_t state;
    EXPECT_EQ(oscillations_fep_bridge_get_state(nullptr, &state), -1);
    EXPECT_EQ(oscillations_fep_bridge_get_state(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, GetStats) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    oscillations_fep_stats_t stats;
    int ret = oscillations_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, GetStatsNullPtr) {
    oscillations_fep_stats_t stats;
    EXPECT_EQ(oscillations_fep_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(oscillations_fep_bridge_get_stats(bridge, nullptr), -1);
}

TEST_F(OscillationsFepBridgeTest, StatsAfterUpdate) {
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    // Run updates
    oscillations_fep_bridge_update(bridge, 100);
    oscillations_fep_modulate_gamma_from_pe(bridge);
    oscillations_fep_modulate_beta_from_predictions(bridge);

    oscillations_fep_stats_t stats;
    int ret = oscillations_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.gamma_modulations, 0);
    EXPECT_GE(stats.beta_modulations, 0);
    EXPECT_GE(stats.alpha_modulations, 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, ConnectBioAsync) {
    int ret = oscillations_fep_bridge_connect_bio_async(bridge);
    // May return 0 or -1 depending on router availability
    EXPECT_TRUE(ret == 0 || ret == -1);
}

TEST_F(OscillationsFepBridgeTest, ConnectBioAsyncNullPtr) {
    int ret = oscillations_fep_bridge_connect_bio_async(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, DisconnectBioAsync) {
    oscillations_fep_bridge_connect_bio_async(bridge);
    int ret = oscillations_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(OscillationsFepBridgeTest, DisconnectBioAsyncNullPtr) {
    int ret = oscillations_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(OscillationsFepBridgeTest, IsBioAsyncConnected) {
    bool connected = oscillations_fep_bridge_is_bio_async_connected(bridge);
    // Initially should be false
    EXPECT_FALSE(connected);
}

TEST_F(OscillationsFepBridgeTest, IsBioAsyncConnectedNullPtr) {
    bool connected = oscillations_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(OscillationsFepBridgeTest, BioAsyncConnectDisconnectCycle) {
    oscillations_fep_bridge_connect_bio_async(bridge);
    bool connected = oscillations_fep_bridge_is_bio_async_connected(bridge);

    oscillations_fep_bridge_disconnect_bio_async(bridge);
    bool disconnected = oscillations_fep_bridge_is_bio_async_connected(bridge);
    EXPECT_FALSE(disconnected);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(OscillationsFepBridgeTest, FullPipelineFEPToOscillations) {
    // Connect systems
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    // Execute FEP → oscillations pathway
    EXPECT_EQ(oscillations_fep_modulate_gamma_from_pe(bridge), 0);
    EXPECT_EQ(oscillations_fep_modulate_beta_from_predictions(bridge), 0);
    EXPECT_EQ(oscillations_fep_modulate_alpha_from_precision(bridge), 0);
    EXPECT_EQ(oscillations_fep_generate_theta_gamma_pac(bridge), 0);

    // Verify state
    oscillations_fep_state_t state;
    EXPECT_EQ(oscillations_fep_bridge_get_state(bridge, &state), 0);
}

TEST_F(OscillationsFepBridgeTest, FullPipelineOscillationsToFEP) {
    // Connect systems
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    // Execute oscillations → FEP pathway
    float precision = 0.0f;
    EXPECT_EQ(oscillations_fep_derive_precision_from_ratio(bridge, &precision), 0);
    EXPECT_EQ(oscillations_fep_weight_errors_by_gamma(bridge), 0);
    EXPECT_EQ(oscillations_fep_bind_beliefs_via_coherence(bridge), 0);

    // Verify statistics
    oscillations_fep_stats_t stats;
    EXPECT_EQ(oscillations_fep_bridge_get_stats(bridge, &stats), 0);
}

TEST_F(OscillationsFepBridgeTest, BidirectionalIntegration) {
    // Connect systems
    oscillations_fep_bridge_connect_fep(bridge, fep);
    oscillations_fep_bridge_connect_oscillations(bridge, osc_state);

    // Run bidirectional update
    EXPECT_EQ(oscillations_fep_bridge_update(bridge, 100), 0);

    // Test both directions work
    float precision = 0.0f;
    EXPECT_EQ(oscillations_fep_derive_precision_from_ratio(bridge, &precision), 0);
    EXPECT_EQ(oscillations_fep_modulate_gamma_from_pe(bridge), 0);

    // Verify analysis functions
    oscillation_band_power_t band_power;
    float pac_strength, preferred_phase;
    EXPECT_EQ(oscillations_fep_compute_band_power(bridge, &band_power), 0);
    EXPECT_EQ(oscillations_fep_detect_pac(bridge, &pac_strength, &preferred_phase), 0);
}
