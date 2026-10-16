//=============================================================================
// test_physics_bio_async_integration.cpp - Physics Bio-Async Integration Tests
//=============================================================================
/**
 * @file test_physics_bio_async_integration.cpp
 * @brief Integration tests for physics layer bio-async message routing
 *
 * Tests bidirectional message flow between physics modules and other systems
 * via the bio-async messaging infrastructure.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/bridges/nimcp_hh_bio_async_bridge.h"
#include "physics/bridges/nimcp_thermo_bio_async_bridge.h"
#include "physics/bridges/nimcp_ephaptic_bio_async_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsBioAsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize physics modules
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
// HH Bio-Async Bridge Tests
//=============================================================================

TEST_F(PhysicsBioAsyncIntegrationTest, HHBioAsyncBridgeCreation) {
    hh_bio_async_bridge_t* bridge = hh_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    hh_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, HHBioAsyncDefaultConfig) {
    hh_bio_async_config_t config;
    EXPECT_EQ(hh_bio_async_default_config(&config), 0);
    EXPECT_GT(config.broadcast_interval_ms, 0.0f);
}

TEST_F(PhysicsBioAsyncIntegrationTest, HHBioAsyncVoltageBroadcast) {
    hh_bio_async_bridge_t* bridge = hh_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create a test neuron
    nimcp_hh_neuron_t neuron;
    memset(&neuron, 0, sizeof(neuron));
    neuron.V = -65.0f;
    neuron.initialized = true;

    // Broadcast voltage (no router connected, should still work)
    int result = hh_bio_async_broadcast_voltage(bridge, &neuron);
    // May return -1 if no router, but shouldn't crash
    (void)result;

    hh_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, HHBioAsyncSpikeBroadcast) {
    hh_bio_async_bridge_t* bridge = hh_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create a test neuron
    nimcp_hh_neuron_t neuron;
    memset(&neuron, 0, sizeof(neuron));
    neuron.V = 30.0f;  // Spike voltage

    // Broadcast spike event
    int result = hh_bio_async_broadcast_spike(bridge, &neuron, 10000ULL);
    (void)result;  // May fail without router

    hh_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, HHBioAsyncStats) {
    hh_bio_async_bridge_t* bridge = hh_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    hh_bio_async_stats_t stats;
    EXPECT_EQ(hh_bio_async_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.messages_sent, 0U);

    hh_bio_async_bridge_destroy(bridge);
}

//=============================================================================
// Thermo Bio-Async Bridge Tests
//=============================================================================

TEST_F(PhysicsBioAsyncIntegrationTest, ThermoBioAsyncBridgeCreation) {
    thermo_bio_async_bridge_t* bridge = thermo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    thermo_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, ThermoBioAsyncDefaultConfig) {
    thermo_bio_async_config_t config;
    EXPECT_EQ(thermo_bio_async_default_config(&config), 0);
    EXPECT_GT(config.broadcast_interval_ms, 0.0f);
}

TEST_F(PhysicsBioAsyncIntegrationTest, ThermoBioAsyncTemperatureBroadcast) {
    thermo_bio_async_bridge_t* bridge = thermo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Broadcast temperature (310.15 K = 37 C)
    int result = thermo_bio_async_broadcast_temperature(bridge, 310.15);
    (void)result;

    thermo_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, ThermoBioAsyncATPLevelBroadcast) {
    thermo_bio_async_bridge_t* bridge = thermo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int result = thermo_bio_async_broadcast_atp_level(bridge);
    (void)result;

    thermo_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, ThermoBioAsyncStats) {
    thermo_bio_async_bridge_t* bridge = thermo_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    thermo_bio_async_stats_t stats;
    EXPECT_EQ(thermo_bio_async_get_stats(bridge, &stats), 0);

    thermo_bio_async_bridge_destroy(bridge);
}

//=============================================================================
// Ephaptic Bio-Async Bridge Tests
//=============================================================================

TEST_F(PhysicsBioAsyncIntegrationTest, EphapticBioAsyncBridgeCreation) {
    ephaptic_bio_async_bridge_t* bridge = ephaptic_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    ephaptic_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, EphapticBioAsyncDefaultConfig) {
    ephaptic_bio_async_config_t config;
    EXPECT_EQ(ephaptic_bio_async_default_config(&config), 0);
    EXPECT_GT(config.broadcast_interval_ms, 0.0f);
}

TEST_F(PhysicsBioAsyncIntegrationTest, EphapticBioAsyncFieldStateBroadcast) {
    ephaptic_bio_async_bridge_t* bridge = ephaptic_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    if (ephaptic_initialized_) {
        int result = ephaptic_bio_async_broadcast_field_state(bridge, &ephaptic_);
        (void)result;
    }

    ephaptic_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, EphapticBioAsyncBandPowerBroadcast) {
    ephaptic_bio_async_bridge_t* bridge = ephaptic_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create a mock LFP result
    nimcp_lfp_result_t lfp;
    memset(&lfp, 0, sizeof(lfp));
    lfp.amplitude = 1.0f;
    lfp.dominant_frequency = 10.0f;
    lfp.band_power[0] = 0.1f;  // delta
    lfp.band_power[1] = 0.2f;  // theta
    lfp.band_power[2] = 0.3f;  // alpha
    lfp.band_power[3] = 0.2f;  // beta
    lfp.band_power[4] = 0.2f;  // gamma

    int result = ephaptic_bio_async_broadcast_band_power(bridge, &lfp);
    (void)result;

    ephaptic_bio_async_bridge_destroy(bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, EphapticBioAsyncStats) {
    ephaptic_bio_async_bridge_t* bridge = ephaptic_bio_async_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    ephaptic_bio_async_stats_t stats;
    EXPECT_EQ(ephaptic_bio_async_get_stats(bridge, &stats), 0);

    ephaptic_bio_async_bridge_destroy(bridge);
}

//=============================================================================
// Cross-Bridge Tests
//=============================================================================

TEST_F(PhysicsBioAsyncIntegrationTest, AllBridgesSimultaneous) {
    hh_bio_async_bridge_t* hh_bridge = hh_bio_async_bridge_create(nullptr);
    thermo_bio_async_bridge_t* thermo_bridge = thermo_bio_async_bridge_create(nullptr);
    ephaptic_bio_async_bridge_t* eph_bridge = ephaptic_bio_async_bridge_create(nullptr);

    ASSERT_NE(hh_bridge, nullptr);
    ASSERT_NE(thermo_bridge, nullptr);
    ASSERT_NE(eph_bridge, nullptr);

    // All bridges should coexist without issues
    hh_bio_async_bridge_destroy(hh_bridge);
    thermo_bio_async_bridge_destroy(thermo_bridge);
    ephaptic_bio_async_bridge_destroy(eph_bridge);
}

TEST_F(PhysicsBioAsyncIntegrationTest, BridgeUpdateLoop) {
    hh_bio_async_bridge_t* hh_bridge = hh_bio_async_bridge_create(nullptr);
    thermo_bio_async_bridge_t* thermo_bridge = thermo_bio_async_bridge_create(nullptr);
    ephaptic_bio_async_bridge_t* eph_bridge = ephaptic_bio_async_bridge_create(nullptr);

    ASSERT_NE(hh_bridge, nullptr);
    ASSERT_NE(thermo_bridge, nullptr);
    ASSERT_NE(eph_bridge, nullptr);

    // Simulate update loop
    for (int i = 0; i < 100; i++) {
        hh_bio_async_update(hh_bridge, 1);
        thermo_bio_async_update(thermo_bridge, 1);
        ephaptic_bio_async_process_inbox(eph_bridge);
    }

    hh_bio_async_bridge_destroy(hh_bridge);
    thermo_bio_async_bridge_destroy(thermo_bridge);
    ephaptic_bio_async_bridge_destroy(eph_bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
