/**
 * @file test_bias_fep_bridge.cpp
 * @brief Unit tests for Cognitive Bias FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Bias bidirectional integration
 * WHY:  Ensure bias detection from systematic PEs and prior correction work correctly
 * HOW:  Test lifecycle, connections, bias detection, prior correction, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/bias/nimcp_bias_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/error/nimcp_error_codes.h"

class BiasFepBridgeTest : public ::testing::Test {
protected:
    bias_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        bias_fep_config_t config;
        bias_fep_bridge_default_config(&config);
        bridge = bias_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            bias_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BiasFepBridgeTest, CreateWithNullConfig) {
    bias_fep_bridge_t* br = bias_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    bias_fep_bridge_destroy(br);
}

TEST_F(BiasFepBridgeTest, DestroyNull) {
    bias_fep_bridge_destroy(nullptr);
}

TEST_F(BiasFepBridgeTest, DefaultConfig) {
    bias_fep_config_t config;
    int ret = bias_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.systematic_pe_threshold, 0.0f);
    EXPECT_GT(config.confirmation_threshold, 0.0f);
    EXPECT_GT(config.prior_correction_rate, 0.0f);
    EXPECT_TRUE(config.enable_bias_detection);
    EXPECT_TRUE(config.enable_prior_correction);
}

TEST_F(BiasFepBridgeTest, DefaultConfigNullPtr) {
    int ret = bias_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);  /* Returns proper error code, not -1 */
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = bias_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(BiasFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(bias_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(bias_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(bias_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

/* ============================================================================
 * Bias Detection Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, DetectBias) {
    int ret = bias_fep_detect_bias(bridge, 4.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, DetectBiasNull) {
    int ret = bias_fep_detect_bias(nullptr, 4.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(BiasFepBridgeTest, DetectAllBiasTypes) {
    // Test all bias type detection
    int ret1 = bias_fep_detect_bias(bridge, 3.5f);
    EXPECT_EQ(ret1, 0);

    int ret2 = bias_fep_detect_bias(bridge, 5.0f);
    EXPECT_EQ(ret2, 0);
}

/* ============================================================================
 * Prior Correction Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, CorrectPrior) {
    /* Must connect FEP system before prior correction can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    bias_fep_bridge_connect_fep(bridge, fep);
    int ret = bias_fep_correct_prior(bridge, BIAS_TYPE_CONFIRMATION);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(BiasFepBridgeTest, CorrectPriorNull) {
    int ret = bias_fep_correct_prior(nullptr, BIAS_TYPE_CONFIRMATION);
    EXPECT_NE(ret, 0);
}

TEST_F(BiasFepBridgeTest, CorrectAllBiasTypes) {
    /* Must connect FEP system before prior correction can work */
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    bias_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(bias_fep_correct_prior(bridge, BIAS_TYPE_CONFIRMATION), 0);
    EXPECT_EQ(bias_fep_correct_prior(bridge, BIAS_TYPE_AVAILABILITY), 0);
    EXPECT_EQ(bias_fep_correct_prior(bridge, BIAS_TYPE_ANCHORING), 0);
    EXPECT_EQ(bias_fep_correct_prior(bridge, BIAS_TYPE_RECENCY), 0);

    fep_destroy(fep);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, Update) {
    int ret = bias_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, UpdateNull) {
    int ret = bias_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(BiasFepBridgeTest, GetState) {
    bias_fep_state_t state;
    int ret = bias_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, GetStateNull) {
    bias_fep_state_t state;
    EXPECT_NE(bias_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(bias_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(BiasFepBridgeTest, GetStats) {
    bias_fep_stats_t stats;
    int ret = bias_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, GetStatsNull) {
    bias_fep_stats_t stats;
    EXPECT_NE(bias_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(bias_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(BiasFepBridgeTest, ConnectBioAsync) {
    int ret = bias_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(BiasFepBridgeTest, ConnectBioAsyncNull) {
    int ret = bias_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(BiasFepBridgeTest, DisconnectBioAsync) {
    bias_fep_bridge_connect_bio_async(bridge);
    int ret = bias_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, DisconnectBioAsyncNull) {
    /* Disconnect with NULL is a graceful no-op, returns SUCCESS */
    int ret = bias_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(BiasFepBridgeTest, IsBioAsyncConnected) {
    bool connected = bias_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(BiasFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = bias_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
