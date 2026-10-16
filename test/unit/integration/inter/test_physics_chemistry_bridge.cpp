/**
 * @file test_physics_chemistry_bridge.cpp
 * @brief Unit tests for physics-chemistry inter-layer bridge
 *
 * WHAT: Tests for nimcp_physics_chemistry_bridge.h
 * WHY:  Verify bridge lifecycle, default config, state/stats accessors
 * HOW:  GTest with fixture for create/destroy cycle
 *
 * FUNCTIONS TESTED:
 *   - nimcp_physics_chemistry_default_config() -> config
 *   - nimcp_physics_chemistry_create(config) -> bridge
 *   - nimcp_physics_chemistry_destroy(bridge)
 *   - nimcp_physics_chemistry_get_state(bridge, state) -> error
 *   - nimcp_physics_chemistry_get_stats(bridge, stats) -> error
 *   - nimcp_physics_chemistry_get_coherence(bridge) -> float
 *   - nimcp_physics_chemistry_reset_stats(bridge) -> error
 *   - nimcp_physics_chemistry_update(bridge, dt) -> error (without init)
 *   - nimcp_physics_chemistry_shutdown(bridge) -> error (without init)
 *
 * @date 2026-02-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "integration/inter/physics_chemistry/nimcp_physics_chemistry_bridge.h"
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

TEST(PhysicsChemistryConfigTest, DefaultConfigHasReasonableValues) {
    nimcp_physics_chemistry_config_t cfg = nimcp_physics_chemistry_default_config();
    EXPECT_GT(cfg.energy_coupling_strength, 0.0f);
    EXPECT_LE(cfg.energy_coupling_strength, 1.0f);
    EXPECT_GT(cfg.thermal_coupling_strength, 0.0f);
    EXPECT_LE(cfg.thermal_coupling_strength, 1.0f);
    EXPECT_GT(cfg.diffusion_coupling_strength, 0.0f);
    EXPECT_LE(cfg.diffusion_coupling_strength, 1.0f);
    EXPECT_GT(cfg.update_interval_ms, 0u);
    EXPECT_TRUE(cfg.enable_metrics);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

class PhysicsChemistryBridgeTest : public ::testing::Test {
protected:
    nimcp_physics_chemistry_bridge_t bridge = nullptr;

    void TearDown() override {
        if (bridge) {
            nimcp_physics_chemistry_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(PhysicsChemistryBridgeTest, CreateWithDefaultConfig) {
    nimcp_physics_chemistry_config_t cfg = nimcp_physics_chemistry_default_config();
    bridge = nimcp_physics_chemistry_create(&cfg);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PhysicsChemistryBridgeTest, CreateWithNullConfig) {
    bridge = nimcp_physics_chemistry_create(nullptr);
    ASSERT_NE(bridge, nullptr);
}

TEST_F(PhysicsChemistryBridgeTest, DestroyNull) {
    /* Should not crash */
    nimcp_physics_chemistry_destroy(nullptr);
}

TEST_F(PhysicsChemistryBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        nimcp_physics_chemistry_bridge_t b = nimcp_physics_chemistry_create(nullptr);
        ASSERT_NE(b, nullptr);
        nimcp_physics_chemistry_destroy(b);
    }
}

/* ============================================================================
 * State and Stats Tests
 * ============================================================================ */

class PhysicsChemistryStateTest : public ::testing::Test {
protected:
    nimcp_physics_chemistry_bridge_t bridge = nullptr;

    void SetUp() override {
        bridge = nimcp_physics_chemistry_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            nimcp_physics_chemistry_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST_F(PhysicsChemistryStateTest, GetStateReturnsOk) {
    nimcp_physics_chemistry_state_t state;
    memset(&state, 0, sizeof(state));
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_state(bridge, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    /* Default state should have coherence = 1.0 and temperature = 310 */
    EXPECT_FLOAT_EQ(state.bridge_coherence, 1.0f);
    EXPECT_FLOAT_EQ(state.current_temperature, 310.0f);
}

TEST_F(PhysicsChemistryStateTest, GetStateNullBridge) {
    nimcp_physics_chemistry_state_t state;
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_state(nullptr, &state);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, GetStateNullOutput) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_state(bridge, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, GetStatsReturnsOk) {
    nimcp_physics_chemistry_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_stats(bridge, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    /* Fresh bridge should have zero stats */
    EXPECT_EQ(stats.energy_state_transfers, 0u);
    EXPECT_EQ(stats.temperature_updates, 0u);
    EXPECT_EQ(stats.reaction_heat_events, 0u);
    EXPECT_EQ(stats.diffusion_requests, 0u);
}

TEST_F(PhysicsChemistryStateTest, GetStatsNullBridge) {
    nimcp_physics_chemistry_stats_t stats;
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_stats(nullptr, &stats);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, GetStatsNullOutput) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_get_stats(bridge, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, GetCoherence) {
    float coherence = nimcp_physics_chemistry_get_coherence(bridge);
    EXPECT_FLOAT_EQ(coherence, 1.0f);
}

TEST_F(PhysicsChemistryStateTest, GetCoherenceNullBridge) {
    float coherence = nimcp_physics_chemistry_get_coherence(nullptr);
    /* Implementation returns -1.0f for null bridge */
    EXPECT_FLOAT_EQ(coherence, -1.0f);
}

TEST_F(PhysicsChemistryStateTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Verify stats are zeroed */
    nimcp_physics_chemistry_stats_t stats;
    nimcp_physics_chemistry_get_stats(bridge, &stats);
    EXPECT_EQ(stats.energy_state_transfers, 0u);
    EXPECT_EQ(stats.temperature_updates, 0u);
}

TEST_F(PhysicsChemistryStateTest, ResetStatsNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_reset_stats(nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

/* ============================================================================
 * Uninitialized Operation Tests
 * ============================================================================ */

TEST_F(PhysicsChemistryStateTest, UpdateWithoutInitReturnsError) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_update(bridge, 0.01f);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, UpdateNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_update(nullptr, 0.01f);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, ShutdownWithoutInitReturnsError) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_shutdown(bridge);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, ShutdownNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_shutdown(nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, InitNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_init(nullptr, nullptr, nullptr, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, InitNullRegistry) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_init(bridge, nullptr, nullptr, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, TransferBottomUpNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_transfer_bottom_up(nullptr, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsChemistryStateTest, TransferTopDownNullBridge) {
    nimcp_layer_error_t err = nimcp_physics_chemistry_transfer_top_down(nullptr, nullptr);
    EXPECT_NE(err, NIMCP_LAYER_OK);
}
