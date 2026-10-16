/**
 * @file test_salience_fep_bridge.cpp
 * @brief Unit tests for Salience FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Salience bidirectional integration
 * WHY:  Ensure precision modulation, PE-based salience, and gating work correctly
 * HOW:  Test lifecycle, connections, precision modulation, salience computation, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/salience/nimcp_salience_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class SalienceFepBridgeTest : public ::testing::Test {
protected:
    salience_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        salience_fep_config_t config;
        salience_fep_bridge_default_config(&config);
        bridge = salience_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            salience_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SalienceFepBridgeTest, CreateWithNullConfig) {
    salience_fep_bridge_t* br = salience_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    salience_fep_bridge_destroy(br);
}

TEST_F(SalienceFepBridgeTest, DestroyNull) {
    salience_fep_bridge_destroy(nullptr);
}

TEST_F(SalienceFepBridgeTest, DefaultConfig) {
    salience_fep_config_t config;
    int ret = salience_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.salience_precision_gain, 0.0f);
    EXPECT_GT(config.surprise_salience_weight, 0.0f);
    EXPECT_TRUE(config.enable_precision_modulation);
    EXPECT_TRUE(config.enable_pe_salience);
}

TEST_F(SalienceFepBridgeTest, DefaultConfigNullPtr) {
    int ret = salience_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = salience_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(SalienceFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(salience_fep_bridge_connect_fep(nullptr, nullptr), 0);
}

TEST_F(SalienceFepBridgeTest, ConnectSalience) {
    salience_evaluator_t salience = 0;
    int ret = salience_fep_bridge_connect_salience(bridge, salience);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, ConnectSalienceNull) {
    salience_evaluator_t salience = 0;
    EXPECT_NE(salience_fep_bridge_connect_salience(nullptr, salience), 0);
}

/* ============================================================================
 * Salience → FEP Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, ModulatePrecisionBySalience) {
    int ret = salience_fep_modulate_precision_by_salience(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, ModulatePrecisionBySalienceNull) {
    int ret = salience_fep_modulate_precision_by_salience(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SalienceFepBridgeTest, GateBySalience) {
    int ret = salience_fep_gate_by_salience(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, GateBySalienceNull) {
    int ret = salience_fep_gate_by_salience(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * FEP → Salience Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, ComputeSalienceFromPe) {
    int ret = salience_fep_compute_salience_from_pe(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, ComputeSalienceFromPeNull) {
    int ret = salience_fep_compute_salience_from_pe(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, Update) {
    int ret = salience_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, UpdateNull) {
    int ret = salience_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(SalienceFepBridgeTest, GetState) {
    salience_fep_state_t state;
    int ret = salience_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, GetStateNull) {
    salience_fep_state_t state;
    EXPECT_NE(salience_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(salience_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(SalienceFepBridgeTest, GetStats) {
    salience_fep_stats_t stats;
    int ret = salience_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, GetStatsNull) {
    salience_fep_stats_t stats;
    EXPECT_NE(salience_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(salience_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(SalienceFepBridgeTest, ConnectBioAsync) {
    int ret = salience_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(SalienceFepBridgeTest, ConnectBioAsyncNull) {
    int ret = salience_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SalienceFepBridgeTest, DisconnectBioAsync) {
    salience_fep_bridge_connect_bio_async(bridge);
    int ret = salience_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(SalienceFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = salience_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(SalienceFepBridgeTest, IsBioAsyncConnected) {
    bool connected = salience_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(SalienceFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = salience_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
