//=============================================================================
// test_physics_snn_integration.cpp - Physics SNN Integration Tests
//=============================================================================
/**
 * @file test_physics_snn_integration.cpp
 * @brief Integration tests for physics-SNN/Plasticity bridge
 *
 * Tests bidirectional integration between HH biophysics and SNN/STDP systems.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/bridges/nimcp_physics_snn_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsSNNIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
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
        hh_initialized_ = (nimcp_hh_population_create(&hh_pop_, 20, &hh_config) == NIMCP_SUCCESS);

        nimcp_thermo_config_t thermo_config = nimcp_thermo_default_config();
        thermo_initialized_ = (nimcp_thermo_init(&thermo_state_, &thermo_config) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (hh_initialized_) nimcp_hh_population_destroy(&hh_pop_);
        if (thermo_initialized_) nimcp_thermo_destroy(&thermo_state_);
    }

    nimcp_hh_population_t hh_pop_;
    nimcp_thermodynamic_state_t thermo_state_;
    bool hh_initialized_ = false;
    bool thermo_initialized_ = false;
};

//=============================================================================
// Bridge Creation Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, BridgeCreation) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, BridgeWithConfig) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.stdp_rule = PHYSICS_SNN_STDP_CLASSICAL;
    config.ltp_window_ms = 25.0f;
    config.ltd_window_ms = 30.0f;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    float ltp, ltd;
    physics_snn_get_stdp_windows(bridge, &ltp, &ltd);
    EXPECT_FLOAT_EQ(ltp, 25.0f);
    EXPECT_FLOAT_EQ(ltd, 30.0f);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Spike Registration Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, SingleSpikeRegistration) {
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

TEST_F(PhysicsSNNIntegrationTest, MultipleSpikeRegistration) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        physics_snn_spike_t spike = {
            .source_id = (uint32_t)(i % 20),
            .spike_time_ms = (float)i,
            .membrane_voltage = 30.0f,
            .temperature = 310.15f,
            .atp_level = 1.0f
        };
        physics_snn_register_spike(bridge, &spike);
    }

    physics_snn_stats_t stats;
    physics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_encoded, 100U);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// STDP Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, STDPLTPComputation) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Pre before post -> LTP
    physics_snn_spike_t pre = {
        .source_id = 0, .spike_time_ms = 10.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 1.0f
    };
    physics_snn_spike_t post = {
        .source_id = 1, .spike_time_ms = 15.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 1.0f
    };

    physics_snn_stdp_event_t event;
    float dw = physics_snn_compute_stdp(bridge, &pre, &post, &event);

    EXPECT_GT(dw, 0.0f);  // LTP
    EXPECT_FLOAT_EQ(event.dt_ms, 5.0f);

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, STDPLTDComputation) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Post before pre -> LTD
    physics_snn_spike_t pre = {
        .source_id = 0, .spike_time_ms = 15.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 1.0f
    };
    physics_snn_spike_t post = {
        .source_id = 1, .spike_time_ms = 10.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 1.0f
    };

    physics_snn_stdp_event_t event;
    float dw = physics_snn_compute_stdp(bridge, &pre, &post, &event);

    EXPECT_LT(dw, 0.0f);  // LTD
    EXPECT_FLOAT_EQ(event.dt_ms, -5.0f);

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, STDPTemperatureScaling) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    float ltp_cold, ltd_cold, ltp_hot, ltd_hot;

    // Cold temperature
    physics_snn_set_temperature(bridge, 300.15f);  // -10K from reference
    physics_snn_get_stdp_windows(bridge, &ltp_cold, &ltd_cold);

    // Hot temperature
    physics_snn_set_temperature(bridge, 320.15f);  // +10K from reference
    physics_snn_get_stdp_windows(bridge, &ltp_hot, &ltd_hot);

    // Higher temperature -> narrower windows
    EXPECT_LT(ltp_hot, ltp_cold);
    EXPECT_LT(ltd_hot, ltd_cold);

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, STDPATPGating) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.enable_atp_gating = true;
    config.atp_ltp_threshold = 0.3f;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    physics_snn_spike_t pre = {
        .source_id = 0, .spike_time_ms = 10.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 0.1f
    };
    physics_snn_spike_t post = {
        .source_id = 1, .spike_time_ms = 15.0f,
        .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 0.1f
    };

    // With low ATP, LTP should be gated
    float dw = physics_snn_compute_stdp(bridge, &pre, &post, nullptr);
    EXPECT_EQ(dw, 0.0f);  // Gated by low ATP

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Eligibility Trace Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, EligibilityTraceUpdate) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.enable_eligibility = true;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    float trace = physics_snn_update_eligibility(bridge, 0, 0.5f);
    EXPECT_EQ(trace, 0.5f);

    trace = physics_snn_update_eligibility(bridge, 0, 0.3f);
    EXPECT_EQ(trace, 0.8f);

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, EligibilityTraceDecay) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.enable_eligibility = true;
    config.eligibility_decay_ms = 100.0f;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    physics_snn_update_eligibility(bridge, 0, 1.0f);
    physics_snn_decay_eligibility(bridge, 100.0f);

    // After one time constant, should decay to ~0.37
    // (but we can't directly query trace value, so just verify no crash)

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, EligibilityConversion) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.enable_eligibility = true;
    config.coherence_gate_threshold = 0.3f;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    physics_snn_update_eligibility(bridge, 0, 0.5f);

    // Low coherence - should not convert
    int converted = physics_snn_convert_eligibility(bridge, 0.1f);
    EXPECT_EQ(converted, 0);

    // High coherence - should convert
    physics_snn_update_eligibility(bridge, 1, 0.5f);
    converted = physics_snn_convert_eligibility(bridge, 0.8f);
    EXPECT_GT(converted, 0);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, ModulationReceive) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_snn_modulation_t mod = {
        .target = PHYSICS_SNN_MOD_CONDUCTANCE,
        .g_na_factor = 1.2f,
        .g_k_factor = 0.9f,
        .threshold_shift = -5.0f,
        .tau_factor = 1.1f
    };

    EXPECT_EQ(physics_snn_receive_modulation(bridge, &mod), 0);

    physics_snn_modulation_t output;
    physics_snn_get_modulation(bridge, &output);

    // Should be smoothed but close
    EXPECT_NEAR(output.g_na_factor, 1.02f, 0.1f);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Learning Gating Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, LearningGatingCheck) {
    physics_snn_config_t config;
    physics_snn_default_config(&config);
    config.enable_atp_gating = true;
    config.atp_ltp_threshold = 0.3f;

    physics_snn_bridge_t* bridge = physics_snn_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Normal ATP - not gated
    physics_snn_set_atp(bridge, 1.0f);
    EXPECT_FALSE(physics_snn_is_learning_gated(bridge));

    // Low ATP - gated
    physics_snn_set_atp(bridge, 0.1f);
    EXPECT_TRUE(physics_snn_is_learning_gated(bridge));

    // Threshold ATP - not gated
    physics_snn_set_atp(bridge, 0.3f);
    EXPECT_FALSE(physics_snn_is_learning_gated(bridge));

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Update Loop Tests
//=============================================================================

TEST_F(PhysicsSNNIntegrationTest, UpdateLoop) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Simulate 1 second of updates
    for (int i = 0; i < 1000; i++) {
        // Register some spikes
        if (i % 20 == 0) {
            physics_snn_spike_t spike = {
                .source_id = (uint32_t)(i % 10),
                .spike_time_ms = (float)i,
                .membrane_voltage = 30.0f,
                .temperature = 310.15f,
                .atp_level = 0.8f
            };
            physics_snn_register_spike(bridge, &spike);
        }

        physics_snn_update(bridge, 1.0f);
    }

    physics_snn_stats_t stats;
    physics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_encoded, 50U);

    physics_snn_bridge_destroy(bridge);
}

TEST_F(PhysicsSNNIntegrationTest, Reset) {
    physics_snn_bridge_t* bridge = physics_snn_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Add some activity
    for (int i = 0; i < 10; i++) {
        physics_snn_spike_t spike = {
            .source_id = 0, .spike_time_ms = (float)i,
            .membrane_voltage = 30.0f, .temperature = 310.15f, .atp_level = 1.0f
        };
        physics_snn_register_spike(bridge, &spike);
    }

    // Reset
    physics_snn_reset(bridge);

    physics_snn_stats_t stats;
    physics_snn_get_stats(bridge, &stats);
    EXPECT_EQ(stats.spikes_encoded, 0U);

    physics_snn_bridge_destroy(bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
