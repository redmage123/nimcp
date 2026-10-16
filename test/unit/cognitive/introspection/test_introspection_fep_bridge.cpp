/**
 * @file test_introspection_fep_bridge.cpp
 * @brief Unit tests for Introspection FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Introspection bidirectional integration
 * WHY:  Ensure metacognitive monitoring of FEP precision and uncertainty works correctly
 * HOW:  Test lifecycle, connections, precision estimation, meta-learning, and bio-async integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include "cognitive/introspection/nimcp_introspection_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class IntrospectionFepBridgeTest : public ::testing::Test {
protected:
    introspection_fep_bridge_t* bridge = nullptr;

    void SetUp() override {
        introspection_fep_config_t config;
        introspection_fep_bridge_default_config(&config);
        bridge = introspection_fep_bridge_create(&config);
    }

    void TearDown() override {
        if (bridge) {
            introspection_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(IntrospectionFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(IntrospectionFepBridgeTest, CreateWithNullConfig) {
    introspection_fep_bridge_t* br = introspection_fep_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);
    introspection_fep_bridge_destroy(br);
}

TEST_F(IntrospectionFepBridgeTest, DestroyNull) {
    introspection_fep_bridge_destroy(nullptr);
}

TEST_F(IntrospectionFepBridgeTest, DefaultConfig) {
    introspection_fep_config_t config;
    int ret = introspection_fep_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.pe_threshold, 0.0f);
    EXPECT_GT(config.uncertainty_threshold, 0.0f);
    EXPECT_GT(config.meta_learning_rate, 0.0f);
    EXPECT_TRUE(config.enable_precision_monitoring);
}

TEST_F(IntrospectionFepBridgeTest, DefaultConfigNullPtr) {
    int ret = introspection_fep_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(IntrospectionFepBridgeTest, ConnectFep) {
    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);
    ASSERT_NE(fep, nullptr);

    int ret = introspection_fep_bridge_connect_fep(bridge, fep);
    EXPECT_EQ(ret, 0);

    fep_destroy(fep);
}

TEST_F(IntrospectionFepBridgeTest, ConnectFepNull) {
    EXPECT_NE(introspection_fep_bridge_connect_fep(nullptr, nullptr), 0);

    fep_config_t fep_config;
    fep_default_config(&fep_config);
    fep_system_t* fep = fep_create(&fep_config, 8, 4);

    EXPECT_NE(introspection_fep_bridge_connect_fep(nullptr, fep), 0);
    EXPECT_NE(introspection_fep_bridge_connect_fep(bridge, nullptr), 0);

    fep_destroy(fep);
}

TEST_F(IntrospectionFepBridgeTest, ConnectIntrospection) {
    introspection_context_t intro = 0;
    int ret = introspection_fep_bridge_connect_introspection(bridge, intro);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, ConnectIntrospectionNull) {
    introspection_context_t intro = 0;
    EXPECT_NE(introspection_fep_bridge_connect_introspection(nullptr, intro), 0);
}

/* ============================================================================
 * FEP → Introspection Tests
 * ============================================================================ */

TEST_F(IntrospectionFepBridgeTest, EstimatePrecision) {
    float precision = 0.0f;
    int ret = introspection_fep_estimate_precision(bridge, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(precision, 0.0f);
}

TEST_F(IntrospectionFepBridgeTest, EstimatePrecisionNull) {
    float precision = 0.0f;
    EXPECT_NE(introspection_fep_estimate_precision(nullptr, &precision), 0);
    EXPECT_NE(introspection_fep_estimate_precision(bridge, nullptr), 0);
}

TEST_F(IntrospectionFepBridgeTest, MonitorUncertainty) {
    int ret = introspection_fep_monitor_uncertainty(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, MonitorUncertaintyNull) {
    int ret = introspection_fep_monitor_uncertainty(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, MetaLearn) {
    int ret = introspection_fep_meta_learn(bridge, 4.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, MetaLearnNull) {
    int ret = introspection_fep_meta_learn(nullptr, 4.5f);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Update & State Tests
 * ============================================================================ */

TEST_F(IntrospectionFepBridgeTest, Update) {
    int ret = introspection_fep_bridge_update(bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, UpdateNull) {
    int ret = introspection_fep_bridge_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, GetState) {
    introspection_fep_state_t state;
    int ret = introspection_fep_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, GetStateNull) {
    introspection_fep_state_t state;
    EXPECT_NE(introspection_fep_bridge_get_state(nullptr, &state), 0);
    EXPECT_NE(introspection_fep_bridge_get_state(bridge, nullptr), 0);
}

TEST_F(IntrospectionFepBridgeTest, GetStats) {
    introspection_fep_stats_t stats;
    int ret = introspection_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, GetStatsNull) {
    introspection_fep_stats_t stats;
    EXPECT_NE(introspection_fep_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_NE(introspection_fep_bridge_get_stats(bridge, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(IntrospectionFepBridgeTest, ConnectBioAsync) {
    int ret = introspection_fep_bridge_connect_bio_async(bridge);
    (void)ret;
}

TEST_F(IntrospectionFepBridgeTest, ConnectBioAsyncNull) {
    int ret = introspection_fep_bridge_connect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, DisconnectBioAsync) {
    introspection_fep_bridge_connect_bio_async(bridge);
    int ret = introspection_fep_bridge_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, DisconnectBioAsyncNull) {
    int ret = introspection_fep_bridge_disconnect_bio_async(nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(IntrospectionFepBridgeTest, IsBioAsyncConnected) {
    bool connected = introspection_fep_bridge_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(IntrospectionFepBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = introspection_fep_bridge_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}
