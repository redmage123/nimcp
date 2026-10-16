//=============================================================================
// test_physics_bridges_regression.cpp - Physics Bridges Regression Tests
//=============================================================================
/**
 * @file test_physics_bridges_regression.cpp
 * @brief Regression tests for all Phase 1 Physics bridges
 *
 * Ensures all physics bridges maintain consistent behavior across changes.
 * Tests cover: Bio-Async, QMC, FFT, SNN, Immune, KG, Hypothalamus, Cognitive,
 * Training, Swarm, and Perception bridges.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Physics core modules
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/ephaptic/nimcp_ephaptic.h"

// Bio-Async bridges
#include "physics/bridges/nimcp_hh_bio_async_bridge.h"
#include "physics/bridges/nimcp_thermo_bio_async_bridge.h"
#include "physics/bridges/nimcp_ephaptic_bio_async_bridge.h"

// QMC bridges
#include "physics/bridges/nimcp_hh_quantum_bridge.h"
#include "physics/bridges/nimcp_thermo_quantum_bridge.h"
#include "physics/bridges/nimcp_ephaptic_quantum_bridge.h"

// FFT bridge
#include "physics/bridges/nimcp_ephaptic_fft_bridge.h"

// System bridges
#include "physics/bridges/nimcp_physics_snn_bridge.h"
#include "physics/bridges/nimcp_physics_immune_bridge.h"
#include "physics/bridges/nimcp_physics_kg_wiring.h"
#include "physics/bridges/nimcp_physics_brain_init.h"

// External system bridges
#include "physics/bridges/nimcp_physics_hypothalamus_bridge.h"
#include "physics/bridges/nimcp_physics_cognitive_bridge.h"
#include "physics/bridges/nimcp_physics_training_bridge.h"
#include "physics/bridges/nimcp_physics_swarm_bridge.h"
#include "physics/bridges/nimcp_physics_perception_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsBridgesRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize core physics modules
        nimcp_hh_config_t hh_config = {
            .g_Na = 120.0f, .g_K = 36.0f, .g_L = 0.3f,
            .g_Ca_L = 0.0f, .g_Ca_T = 0.0f, .g_K_Ca = 0.0f,
            .g_K_A = 0.0f, .g_H = 0.0f,
            .E_Na = 50.0f, .E_K = -77.0f, .E_L = -54.4f,
            .E_Ca = 120.0f, .E_H = -30.0f,
            .C_m = 1.0f, .V_rest = -65.0f,
            .temperature = 37.0f, .surface_area = 0.01f,
            .length = 100.0f, .diameter = 10.0f,
            .enable_calcium = false, .enable_adaptation = false,
            .enable_h_current = false,
            .dt_max = 0.1f, .adaptive_dt = false
        };
        hh_initialized_ = (nimcp_hh_population_create(&hh_pop_, 10, &hh_config) == NIMCP_SUCCESS);

        nimcp_thermo_config_t thermo_config = nimcp_thermo_default_config();
        thermo_initialized_ = (nimcp_thermo_init(&thermo_state_, &thermo_config) == NIMCP_SUCCESS);

        nimcp_ephaptic_config_t eph_config = nimcp_ephaptic_default_config();
        ephaptic_initialized_ = (nimcp_ephaptic_init(&ephaptic_, &eph_config) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (hh_initialized_) nimcp_hh_population_destroy(&hh_pop_);
        if (thermo_initialized_) nimcp_thermo_destroy(&thermo_state_);
        if (ephaptic_initialized_) nimcp_ephaptic_destroy(&ephaptic_);
    }

    nimcp_hh_population_t hh_pop_;
    nimcp_thermodynamic_state_t thermo_state_;
    nimcp_ephaptic_system_t ephaptic_;
    bool hh_initialized_ = false;
    bool thermo_initialized_ = false;
    bool ephaptic_initialized_ = false;
};

//=============================================================================
// Bio-Async Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, HHBioAsyncCreateDestroy) {
    hh_bio_async_bridge_t* bridge = hh_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    hh_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, ThermoBioAsyncCreateDestroy) {
    thermo_bio_async_bridge_t* bridge = thermo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    thermo_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, EphapticBioAsyncCreateDestroy) {
    ephaptic_bio_async_bridge_t* bridge = ephaptic_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    ephaptic_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, BioAsyncDefaultConfigs) {
    hh_bio_async_config_t hh_cfg;
    EXPECT_EQ(hh_bio_async_default_config(&hh_cfg), 0);
    EXPECT_GT(hh_cfg.broadcast_interval_ms, 0.0f);

    thermo_bio_async_config_t thermo_cfg;
    EXPECT_EQ(thermo_bio_async_default_config(&thermo_cfg), 0);
    EXPECT_GT(thermo_cfg.broadcast_interval_ms, 0.0f);

    ephaptic_bio_async_config_t eph_cfg;
    EXPECT_EQ(ephaptic_bio_async_default_config(&eph_cfg), 0);
    EXPECT_GT(eph_cfg.broadcast_interval_ms, 0.0f);
}

//=============================================================================
// QMC Bridge Regression Tests (function-based, no create/destroy)
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, HHQMCDefaultConfigs) {
    hh_qmc_config_t hh_cfg;
    EXPECT_EQ(hh_qmc_default_config(&hh_cfg), 0);
    EXPECT_GT(hh_cfg.num_iterations, 0U);

    hh_qmc_target_t target;
    EXPECT_EQ(hh_qmc_default_target(&target), 0);
    EXPECT_GT(target.target_firing_rate, 0.0f);

    hh_entropy_config_t entropy_cfg;
    EXPECT_EQ(hh_entropy_default_config(&entropy_cfg), 0);
    EXPECT_GT(entropy_cfg.num_samples, 0U);

    hh_stochastic_config_t stochastic_cfg;
    EXPECT_EQ(hh_stochastic_default_config(&stochastic_cfg), 0);
    EXPECT_GT(stochastic_cfg.num_channels, 0U);
}

TEST_F(PhysicsBridgesRegressionTest, ThermoQMCDefaultConfigs) {
    thermo_partition_config_t partition_cfg;
    EXPECT_EQ(thermo_qmc_partition_default_config(&partition_cfg), 0);
    EXPECT_GT(partition_cfg.num_samples, 0U);

    thermo_landauer_config_t landauer_cfg;
    EXPECT_EQ(thermo_qmc_landauer_default_config(&landauer_cfg), 0);
    EXPECT_GT(landauer_cfg.num_iterations, 0U);

    thermo_atp_config_t atp_cfg;
    EXPECT_EQ(thermo_qmc_atp_default_config(&atp_cfg), 0);

    thermo_entropy_config_t entropy_cfg;
    EXPECT_EQ(thermo_qmc_entropy_default_config(&entropy_cfg), 0);
}

TEST_F(PhysicsBridgesRegressionTest, EphapticQMCDefaultConfigs) {
    ephaptic_coherence_config_t coherence_cfg;
    EXPECT_EQ(ephaptic_qmc_coherence_default_config(&coherence_cfg), 0);
    EXPECT_GT(coherence_cfg.num_iterations, 0U);

    ephaptic_coherence_target_t target;
    EXPECT_EQ(ephaptic_qmc_coherence_default_target(&target), 0);
    EXPECT_GT(target.target_coherence, 0.0f);

    ephaptic_walk_config_t walk_cfg;
    EXPECT_EQ(ephaptic_qmc_walk_default_config(&walk_cfg), 0);
    EXPECT_GT(walk_cfg.max_steps, 0U);

    ephaptic_entropy_config_t entropy_cfg;
    EXPECT_EQ(ephaptic_qmc_entropy_default_config(&entropy_cfg), 0);

    ephaptic_pattern_config_t pattern_cfg;
    EXPECT_EQ(ephaptic_qmc_pattern_default_config(&pattern_cfg), 0);
}

//=============================================================================
// FFT Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, EphapticFFTCreateDestroy) {
    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    ephaptic_fft_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, EphapticFFTDefaultConfig) {
    ephaptic_fft_config_t config;
    EXPECT_EQ(ephaptic_fft_default_config(&config), 0);
    EXPECT_EQ(config.fft_size, EPHAPTIC_FFT_DEFAULT_SIZE);
}

TEST_F(PhysicsBridgesRegressionTest, EphapticFFTBufferOperations) {
    ephaptic_fft_config_t config;
    ephaptic_fft_default_config(&config);
    config.fft_size = 64;  // Small for fast test

    ephaptic_fft_bridge_t* bridge = ephaptic_fft_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(ephaptic_fft_buffer_ready(bridge));
    EXPECT_EQ(ephaptic_fft_buffer_level(bridge), 0.0f);

    for (int i = 0; i < 64; i++) {
        ephaptic_fft_add_sample(bridge, 0.5f, (float)i);
    }

    EXPECT_TRUE(ephaptic_fft_buffer_ready(bridge));
    EXPECT_EQ(ephaptic_fft_bridge_reset(bridge), 0);
    EXPECT_FALSE(ephaptic_fft_buffer_ready(bridge));

    ephaptic_fft_bridge_destroy(bridge);
}

//=============================================================================
// SNN Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, SNNBridgeCreateDestroy) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, SNNBridgeDefaultConfig) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    EXPECT_GT(config.ltp_window_ms, 0.0f);
    EXPECT_GT(config.ltd_window_ms, 0.0f);
}

TEST_F(PhysicsBridgesRegressionTest, SNNSpikeRegistration) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_snn_spike_t spike = {
        .source_id = 0,
        .spike_time_ms = 10.0f,
        .membrane_voltage = 30.0f,
        .temperature = 310.15f,
        .atp_level = 1.0f
    };

    EXPECT_EQ(physics_snn_register_spike(bridge, &spike), 0);

    physics_snn_stats_t stats;
    physics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_encoded, 1U);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Immune Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, ImmuneBridgeCreateDestroy) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, ImmuneBridgeDefaultConfig) {
    physics_immune_config_t config;
    EXPECT_EQ(physics_immune_default_config(&config), 0);
    EXPECT_TRUE(config.monitor_temperature);
}

TEST_F(PhysicsBridgesRegressionTest, ImmuneBridgeStateQuery) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_state_t state;
    EXPECT_EQ(physics_immune_get_state(bridge, &state), 0);
    EXPECT_NEAR(state.temperature, PHYSICS_IMMUNE_NORMAL_TEMP, 1.0f);

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// KG Wiring Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, KGWiringDefaultConfig) {
    physics_kg_config_t config;
    EXPECT_EQ(physics_kg_default_config(&config), 0);
    EXPECT_TRUE(config.register_hh);
    EXPECT_TRUE(config.register_thermo);
    EXPECT_TRUE(config.register_ephaptic);
}

//=============================================================================
// Hypothalamus Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, HypothalamusBridgeCreateDestroy) {
    physics_hypo_bridge_t* bridge = physics_hypo_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_hypo_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, HypothalamusBridgeDefaultConfig) {
    physics_hypo_config_t config;
    EXPECT_EQ(physics_hypo_default_config(&config), 0);
    EXPECT_GT(config.temp_setpoint, 0.0f);
}

TEST_F(PhysicsBridgesRegressionTest, HypothalamusBridgeUpdate) {
    physics_hypo_bridge_t* bridge = physics_hypo_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(physics_hypo_update(bridge, 10.0f), 0);

    physics_hypo_bridge_destroy(bridge);
}

//=============================================================================
// Cognitive Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, CognitiveBridgeCreateDestroy) {
    physics_cog_bridge_t* bridge = physics_cog_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_cog_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, CognitiveBridgeDefaultConfig) {
    physics_cog_config_t config;
    EXPECT_EQ(physics_cog_default_config(&config), 0);
}

//=============================================================================
// Training Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, TrainingBridgeCreateDestroy) {
    physics_train_bridge_t* bridge = physics_train_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_train_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, TrainingBridgeDefaultConfig) {
    physics_train_config_t config;
    EXPECT_EQ(physics_train_default_config(&config), 0);
}

//=============================================================================
// Swarm Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, SwarmBridgeCreateDestroy) {
    physics_swarm_bridge_t* bridge = physics_swarm_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_swarm_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, SwarmBridgeDefaultConfig) {
    physics_swarm_config_t config;
    EXPECT_EQ(physics_swarm_default_config(&config), 0);
}

//=============================================================================
// Perception Bridge Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, PerceptionBridgeCreateDestroy) {
    physics_percept_bridge_t* bridge = physics_percept_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_percept_bridge_destroy(bridge);
}

TEST_F(PhysicsBridgesRegressionTest, PerceptionBridgeDefaultConfig) {
    physics_percept_config_t config;
    EXPECT_EQ(physics_percept_default_config(&config), 0);
    EXPECT_GT(config.gamma_binding_threshold, 0.0f);
}

//=============================================================================
// Brain Init Regression Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, BrainInitDefaultConfig) {
    physics_init_config_t config;
    EXPECT_EQ(physics_init_default_config(&config), 0);
    EXPECT_TRUE(config.enable_ephaptic);
    EXPECT_TRUE(config.enable_qmc);
    EXPECT_TRUE(config.enable_fft);
}

//=============================================================================
// Cross-Bridge Stability Tests
//=============================================================================

TEST_F(PhysicsBridgesRegressionTest, AllBridgesSimultaneous) {
    // Create all bridges
    hh_bio_async_bridge_t* hh_bio = hh_bio_async_bridge_create(nullptr);
    thermo_bio_async_bridge_t* thermo_bio = thermo_bio_async_bridge_create(nullptr);
    ephaptic_bio_async_bridge_t* eph_bio = ephaptic_bio_async_bridge_create(nullptr);

    ephaptic_fft_bridge_t* fft = ephaptic_fft_bridge_create(nullptr);
    physics_snn_bridge_t* snn = physics_snn_bridge_create(nullptr);
    physics_immune_bridge_t* immune = physics_immune_bridge_create(nullptr);

    physics_hypo_bridge_t* hypo = physics_hypo_bridge_create(nullptr);
    physics_cog_bridge_t* cog = physics_cog_bridge_create(nullptr);
    physics_train_bridge_t* train = physics_train_bridge_create(nullptr);
    physics_swarm_bridge_t* swarm = physics_swarm_bridge_create(nullptr);
    physics_percept_bridge_t* percept = physics_percept_bridge_create(nullptr);

    // Verify all created
    ASSERT_NE(hh_bio, nullptr);
    ASSERT_NE(thermo_bio, nullptr);
    ASSERT_NE(eph_bio, nullptr);
    ASSERT_NE(fft, nullptr);
    ASSERT_NE(snn, nullptr);
    ASSERT_NE(immune, nullptr);
    ASSERT_NE(hypo, nullptr);
    ASSERT_NE(cog, nullptr);
    ASSERT_NE(train, nullptr);
    ASSERT_NE(swarm, nullptr);
    ASSERT_NE(percept, nullptr);

    // Destroy all (reverse order)
    physics_percept_bridge_destroy(percept);
    physics_swarm_bridge_destroy(swarm);
    physics_train_bridge_destroy(train);
    physics_cog_bridge_destroy(cog);
    physics_hypo_bridge_destroy(hypo);

    physics_immune_bridge_destroy(immune);
    physics_snn_bridge_destroy(snn);
    ephaptic_fft_bridge_destroy(fft);

    ephaptic_bio_async_bridge_destroy(eph_bio);
    thermo_bio_async_bridge_destroy(thermo_bio);
    hh_bio_async_bridge_destroy(hh_bio);
}

TEST_F(PhysicsBridgesRegressionTest, BridgeUpdateLoop) {
    physics_hypo_bridge_t* hypo = physics_hypo_bridge_create(nullptr);
    physics_snn_bridge_t* snn = physics_snn_bridge_create(nullptr);
    physics_immune_bridge_t* immune = physics_immune_bridge_create(nullptr);

    ASSERT_NE(hypo, nullptr);
    ASSERT_NE(snn, nullptr);
    ASSERT_NE(immune, nullptr);

    // Simulate update loop
    for (int i = 0; i < 100; i++) {
        physics_hypo_update(hypo, 1.0f);
        physics_snn_update(snn, 1.0f);
        physics_immune_update(immune, 1.0f);
    }

    physics_immune_bridge_destroy(immune);
    physics_snn_bridge_destroy(snn);
    physics_hypo_bridge_destroy(hypo);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
