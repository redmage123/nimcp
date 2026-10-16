//=============================================================================
// test_physics_immune_integration.cpp - Physics Immune Integration Tests
//=============================================================================
/**
 * @file test_physics_immune_integration.cpp
 * @brief Integration tests for physics-immune system bridge
 *
 * Tests bidirectional integration between physics modules and brain immune system.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"
#include "physics/ephaptic/nimcp_ephaptic.h"
#include "physics/bridges/nimcp_physics_immune_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsImmuneIntegrationTest : public ::testing::Test {
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
// Bridge Creation Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, BridgeCreation) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);
    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, BridgeWithConfig) {
    physics_immune_config_t config;
    physics_immune_default_config(&config);
    config.monitor_temperature = true;
    config.monitor_atp = true;
    config.enable_cytokine_mod = true;
    config.fever_response_scale = 1.5f;

    physics_immune_bridge_t* bridge = physics_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);
    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, DefaultConfig) {
    physics_immune_config_t config;
    EXPECT_EQ(physics_immune_default_config(&config), 0);
    EXPECT_TRUE(config.monitor_temperature);
    EXPECT_TRUE(config.monitor_atp);
    EXPECT_GT(config.temp_sample_interval_ms, 0.0f);
}

//=============================================================================
// Physics Connection Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, ConnectPhysics) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    if (hh_initialized_ && thermo_initialized_ && ephaptic_initialized_) {
        int result = physics_immune_connect_physics(
            bridge, &thermo_state_, &hh_pop_, &ephaptic_);
        EXPECT_EQ(result, 0);
    }

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, ConnectPhysicsPartial) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Connect with only thermodynamics
    if (thermo_initialized_) {
        int result = physics_immune_connect_physics(
            bridge, &thermo_state_, nullptr, nullptr);
        EXPECT_EQ(result, 0);
    }

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// Temperature Monitoring Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, TemperatureCheckNormal) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Update to propagate normal temperature
    physics_immune_update(bridge, 10.0f);

    physics_immune_interaction_t interaction = physics_immune_check_temperature(bridge);
    EXPECT_EQ(interaction, PHYSICS_IMMUNE_INTERACTION_NONE);

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, StateReporting) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_state_t state;
    int result = physics_immune_report_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Temperature should be normal (37C)
    EXPECT_NEAR(state.temperature, PHYSICS_IMMUNE_NORMAL_TEMP, 1.0f);

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// ATP Monitoring Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, ATPCheckNormal) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_update(bridge, 10.0f);

    physics_immune_interaction_t interaction = physics_immune_check_atp(bridge);
    EXPECT_EQ(interaction, PHYSICS_IMMUNE_INTERACTION_NONE);

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// Immune Modulation Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, ApplyModulation) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_modulation_t mod = {
        .g_na_modifier = 1.1f,
        .g_k_modifier = 0.95f,
        .g_leak_modifier = 1.0f,
        .cm_modifier = 1.0f,
        .temp_offset = 0.5f,
        .q10_modifier = 1.05f,
        .inflammation_level = 0.2f,
        .cytokine_source = 0
    };

    int result = physics_immune_apply_modulation(bridge, &mod);
    EXPECT_EQ(result, 0);

    // Verify modulation was stored
    physics_immune_modulation_t output;
    EXPECT_EQ(physics_immune_get_modulation(bridge, &output), 0);

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, CytokineReceive) {
    physics_immune_config_t config;
    physics_immune_default_config(&config);
    config.enable_cytokine_mod = true;

    physics_immune_bridge_t* bridge = physics_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = physics_immune_receive_cytokine(bridge, 1, 0.5f);
    EXPECT_EQ(result, 0);

    physics_immune_stats_t stats;
    physics_immune_get_stats(bridge, &stats);

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, InflammationReceive) {
    physics_immune_config_t config;
    physics_immune_default_config(&config);
    config.enable_inflammation = true;

    physics_immune_bridge_t* bridge = physics_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int result = physics_immune_receive_inflammation(bridge, 0.5f, 0.0f, 0.0f, 0.0f);
    EXPECT_EQ(result, 0);

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// Update Loop Tests
//=============================================================================

TEST_F(PhysicsImmuneIntegrationTest, UpdateLoop) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    // Simulate 1 second of updates
    for (int i = 0; i < 1000; i++) {
        int result = physics_immune_update(bridge, 1.0f);
        EXPECT_EQ(result, 0);
    }

    physics_immune_stats_t stats;
    EXPECT_EQ(physics_immune_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.last_update_ms, 0.0f);

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, GetState) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_update(bridge, 100.0f);

    physics_immune_state_t state;
    EXPECT_EQ(physics_immune_get_state(bridge, &state), 0);
    EXPECT_GE(state.temperature, 0.0f);
    EXPECT_GE(state.atp_level, 0.0f);
    EXPECT_LE(state.atp_level, 1.0f);

    physics_immune_bridge_destroy(bridge);
}

TEST_F(PhysicsImmuneIntegrationTest, GetStats) {
    physics_immune_bridge_t* bridge = physics_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    physics_immune_stats_t stats;
    EXPECT_EQ(physics_immune_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.physics_to_immune_count, 0U);
    EXPECT_EQ(stats.immune_to_physics_count, 0U);

    physics_immune_bridge_destroy(bridge);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
