/**
 * @file test_ethics_fep_bridge.cpp
 * @brief Unit tests for Ethics FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Ethics bidirectional integration
 * WHY:  Ensure value priors, policy constraints, and harm prediction work correctly
 * HOW:  Test lifecycle, connections, value priors, constraints, harm prediction, and bio-async
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/ethics/nimcp_ethics_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class EthicsFepBridgeTest : public ::testing::Test {
protected:
    ethics_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        ethics_fep_config_t config;
        ethics_fep_bridge_default_config(&config);
        bridge = ethics_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            ethics_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EthicsFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EthicsFepBridgeTest, CreateWithNullConfig) {
    ethics_fep_bridge_t* br = ethics_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    ethics_fep_bridge_destroy(br);
}

TEST_F(EthicsFepBridgeTest, DestroyNull) {
    ethics_fep_bridge_destroy(nullptr);
}

TEST_F(EthicsFepBridgeTest, DefaultConfig) {
    ethics_fep_config_t config;
    int ret = ethics_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.harm_threshold, 0.0f);
    EXPECT_GT(config.value_prior_weight, 0.0f);
    EXPECT_GT(config.deontological_penalty, 0.0f);
    EXPECT_TRUE(config.enable_value_priors);
    EXPECT_TRUE(config.enable_deontological_constraints);
    EXPECT_TRUE(config.enable_harm_prediction);
}

TEST_F(EthicsFepBridgeTest, DefaultConfigNullPtr) {
    int ret = ethics_fep_bridge_default_config(nullptr);
    EXPECT_NE(ret, 0);  /* Returns error code for NULL */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(EthicsFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = ethics_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EthicsFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(ethics_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(ethics_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(ethics_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(EthicsFepBridgeTest, ConnectEthics) {
    void* ethics = (void*)1;  // Mock pointer
    int ret = ethics_fep_bridge_connect_ethics(bridge, ethics);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, ConnectEthicsNull) {
    EXPECT_NE(ethics_fep_bridge_connect_ethics(nullptr, nullptr), 0);
    EXPECT_NE(ethics_fep_bridge_connect_ethics(bridge, nullptr), 0);
}

/* ============================================================================
 * Ethics → FEP Tests
 * ============================================================================ */

TEST_F(EthicsFepBridgeTest, ApplyValuePriors) {
    /* Must connect FEP system before value priors can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    ethics_fep_bridge_connect_fep(bridge, fep);
    int ret = ethics_fep_apply_value_priors(bridge);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(EthicsFepBridgeTest, ApplyValuePriorsNull) {
    int ret = ethics_fep_apply_value_priors(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(EthicsFepBridgeTest, ConstrainPolicy) {
    int ret1 = ethics_fep_constrain_policy(bridge, true);
    EXPECT_EQ(ret1, 0);

    int ret2 = ethics_fep_constrain_policy(bridge, false);
    EXPECT_EQ(ret2, 0);
}

TEST_F(EthicsFepBridgeTest, ConstrainPolicyNull) {
    int ret = ethics_fep_constrain_policy(nullptr, true);
    EXPECT_NE(ret, 0);
}

TEST_F(EthicsFepBridgeTest, PredictHarm) {
    float harm_score = 0.0f;
    int ret = ethics_fep_predict_harm(bridge, &harm_score);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(harm_score, 0.0f);
}

TEST_F(EthicsFepBridgeTest, PredictHarmNull) {
    float harm_score = 0.0f;
    EXPECT_NE(ethics_fep_predict_harm(nullptr, &harm_score), 0);
    EXPECT_NE(ethics_fep_predict_harm(bridge, nullptr), 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(EthicsFepBridgeTest, Update) {
    int ret = ethics_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, UpdateNull) {
    int ret = ethics_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(EthicsFepBridgeTest, GetState) {
    ethics_fep_state_t state;
    int ret = ethics_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, GetStateNull) {
    ethics_fep_state_t state;
    EXPECT_NE(ethics_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(ethics_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(EthicsFepBridgeTest, GetStats) {
    ethics_fep_stats_t stats;
    int ret = ethics_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, GetStatsNull) {
    ethics_fep_stats_t stats;
    EXPECT_NE(ethics_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(ethics_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(EthicsFepBridgeTest, ConnectBioAsync) {
    int ret = ethics_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(EthicsFepBridgeTest, ConnectBioAsyncNull) {
    int ret = ethics_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(EthicsFepBridgeTest, DisconnectBioAsync) {
    ethics_fep_bridge_connect_bio_async(bridge);
    int ret = ethics_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, DisconnectBioAsyncNull) {
    /* Disconnect with NULL is a graceful no-op, returns SUCCESS */
    int ret = ethics_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(EthicsFepBridgeTest, IsBioAsyncConnected) {
    bool connected = ethics_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(EthicsFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = ethics_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
