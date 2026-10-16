/**
 * @file test_bio_router_fep_bridge.cpp
 * @brief Unit tests for Bio-Router-FEP Bridge module
 *
 * WHAT: Comprehensive tests for FEP-Bio-Router bidirectional integration
 * WHY:  Ensure prediction-based routing and latency optimization work correctly
 * HOW:  Test lifecycle, route prediction, latency prediction, and effects updates
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_router_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class BioRouterFepBridgeTest : public ::testing::Test {
protected:
    bio_router_fep_bridge_t* bridge = nullptr;
    fep_system_t* fep = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);

        /* Create bridge */
        bio_router_fep_config_t config;
        bio_router_fep_default_config(&config);
        bridge = bio_router_fep_create(&config, fep, (bio_router_t)0);
    }

    void TearDown() override {
        if (bridge) {
            bio_router_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BioRouterFepBridgeTest, CreateWithNullConfig) {
    bio_router_fep_bridge_t* br = bio_router_fep_create(nullptr, fep, (bio_router_t)0);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BioRouterFepBridgeTest, CreateWithNullFep) {
    bio_router_fep_config_t config;
    bio_router_fep_default_config(&config);
    bio_router_fep_bridge_t* br = bio_router_fep_create(&config, nullptr, (bio_router_t)0);
    EXPECT_EQ(br, nullptr);
}

TEST_F(BioRouterFepBridgeTest, DestroyNull) {
    bio_router_fep_destroy(nullptr);  /* Should not crash */
}

TEST_F(BioRouterFepBridgeTest, DefaultConfig) {
    bio_router_fep_config_t config;
    int ret = bio_router_fep_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(config.route_prediction_confidence, 0.0f);
    EXPECT_GT(config.latency_tolerance_ms, 0.0f);
    EXPECT_GT(config.surprise_threshold, 0.0f);
    EXPECT_TRUE(config.enable_route_learning);
    EXPECT_TRUE(config.enable_route_optimization);
    EXPECT_TRUE(config.enable_latency_prediction);
}

TEST_F(BioRouterFepBridgeTest, DefaultConfigNullPtr) {
    int ret = bio_router_fep_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Effects Update Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, UpdateEffects) {
    int ret = bio_router_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);
}

TEST_F(BioRouterFepBridgeTest, UpdateEffectsNull) {
    EXPECT_NE(bio_router_fep_update_effects(nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, UpdateEffectsComputesRouteConfidence) {
    int ret = bio_router_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);

    bio_router_fep_effects_t effects;
    ret = bio_router_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(effects.route_confidence, 0.0f);
    EXPECT_LE(effects.route_confidence, 1.0f);
}

TEST_F(BioRouterFepBridgeTest, UpdateEffectsComputesLatencyPrediction) {
    int ret = bio_router_fep_update_effects(bridge);
    EXPECT_EQ(ret, 0);

    bio_router_fep_effects_t effects;
    ret = bio_router_fep_get_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_GE(effects.predicted_latency_ms, 0.0f);
    EXPECT_GE(effects.latency_uncertainty, 0.0f);
}

/* ============================================================================
 * Route Prediction Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, PredictRoute) {
    bio_module_id_t predicted_hop;
    float confidence;

    int ret = bio_router_fep_predict_route(bridge, (bio_module_id_t)0x0100, (bio_module_id_t)0x0200, &predicted_hop, &confidence);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(BioRouterFepBridgeTest, PredictRouteNull) {
    bio_module_id_t predicted_hop;
    float confidence;

    EXPECT_NE(bio_router_fep_predict_route(nullptr, (bio_module_id_t)0x0100, (bio_module_id_t)0x0200, &predicted_hop, &confidence), 0);
    EXPECT_NE(bio_router_fep_predict_route(bridge, (bio_module_id_t)0x0100, (bio_module_id_t)0x0200, nullptr, &confidence), 0);
    EXPECT_NE(bio_router_fep_predict_route(bridge, (bio_module_id_t)0x0100, (bio_module_id_t)0x0200, &predicted_hop, nullptr), 0);
}

/* ============================================================================
 * Latency Prediction Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, PredictLatency) {
    float predicted_ms;
    float uncertainty;

    int ret = bio_router_fep_predict_latency(bridge, (bio_module_id_t)0x0200,
                                              &predicted_ms, &uncertainty);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(predicted_ms, 0.0f);
    EXPECT_GE(uncertainty, 0.0f);
}

TEST_F(BioRouterFepBridgeTest, PredictLatencyNull) {
    float predicted_ms;
    float uncertainty;

    EXPECT_NE(bio_router_fep_predict_latency(nullptr, (bio_module_id_t)0x0200,
                                              &predicted_ms, &uncertainty), 0);
    EXPECT_NE(bio_router_fep_predict_latency(bridge, (bio_module_id_t)0x0200,
                                              nullptr, &uncertainty), 0);
    EXPECT_NE(bio_router_fep_predict_latency(bridge, (bio_module_id_t)0x0200,
                                              &predicted_ms, nullptr), 0);
}

/* ============================================================================
 * Routing Observation Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, ObserveRouting) {
    int ret = bio_router_fep_observe_routing(bridge, (bio_module_id_t)0x0200, 5.0f, true);
    EXPECT_EQ(ret, 0);
}

TEST_F(BioRouterFepBridgeTest, ObserveRoutingNull) {
    EXPECT_NE(bio_router_fep_observe_routing(nullptr, (bio_module_id_t)0x0200, 5.0f, true), 0);
}

TEST_F(BioRouterFepBridgeTest, ObserveRoutingUpdatesEffects) {
    bio_router_fep_observe_routing(bridge, (bio_module_id_t)0x0200, 5.0f, true);

    fep_bio_router_effects_t effects;
    int ret = bio_router_fep_get_router_effects(bridge, &effects);
    EXPECT_EQ(ret, 0);

    EXPECT_FLOAT_EQ(effects.actual_latency_ms, 5.0f);
    EXPECT_EQ(effects.messages_routed, 1u);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, InitiallyNotConnected) {
    EXPECT_FALSE(bio_router_fep_is_bio_async_connected(bridge));
}

TEST_F(BioRouterFepBridgeTest, ConnectDisconnectBioAsync) {
    int ret = bio_router_fep_connect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(bio_router_fep_is_bio_async_connected(bridge));

    ret = bio_router_fep_disconnect_bio_async(bridge);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(bio_router_fep_is_bio_async_connected(bridge));
}

TEST_F(BioRouterFepBridgeTest, ConnectNull) {
    EXPECT_NE(bio_router_fep_connect_bio_async(nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, DisconnectNull) {
    EXPECT_NE(bio_router_fep_disconnect_bio_async(nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, IsConnectedNull) {
    EXPECT_FALSE(bio_router_fep_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, GetEffects) {
    bio_router_fep_effects_t effects;
    int ret = bio_router_fep_get_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.route_confidence, 0.0f);
}

TEST_F(BioRouterFepBridgeTest, GetEffectsNull) {
    bio_router_fep_effects_t effects;

    EXPECT_NE(bio_router_fep_get_effects(nullptr, &effects), 0);
    EXPECT_NE(bio_router_fep_get_effects(bridge, nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, GetRouterEffects) {
    fep_bio_router_effects_t effects;
    int ret = bio_router_fep_get_router_effects(bridge, &effects);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(effects.actual_latency_ms, 0.0f);
}

TEST_F(BioRouterFepBridgeTest, GetRouterEffectsNull) {
    fep_bio_router_effects_t effects;

    EXPECT_NE(bio_router_fep_get_router_effects(nullptr, &effects), 0);
    EXPECT_NE(bio_router_fep_get_router_effects(bridge, nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, GetStats) {
    bio_router_fep_stats_t stats;
    int ret = bio_router_fep_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.avg_precision, 0.0f);
}

TEST_F(BioRouterFepBridgeTest, GetStatsNull) {
    bio_router_fep_stats_t stats;

    EXPECT_NE(bio_router_fep_get_stats(nullptr, &stats), 0);
    EXPECT_NE(bio_router_fep_get_stats(bridge, nullptr), 0);
}

TEST_F(BioRouterFepBridgeTest, ResetStats) {
    /* Generate some stats */
    bio_router_fep_observe_routing(bridge, (bio_module_id_t)0x0200, 5.0f, true);

    /* Reset stats */
    int ret = bio_router_fep_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    /* Verify stats are reset */
    bio_router_fep_stats_t stats;
    bio_router_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_messages_routed, 0u);
}

TEST_F(BioRouterFepBridgeTest, ResetStatsNull) {
    EXPECT_NE(bio_router_fep_reset_stats(nullptr), 0);
}

/* ============================================================================
 * Integration Behavior Tests
 * ============================================================================ */

TEST_F(BioRouterFepBridgeTest, HighLatencyDetected) {
    /* First update effects to set predictions */
    bio_router_fep_update_effects(bridge);

    /* Observe high latency routing */
    bio_router_fep_observe_routing(bridge, (bio_module_id_t)0x0200, 100.0f, true);

    fep_bio_router_effects_t effects;
    bio_router_fep_get_router_effects(bridge, &effects);

    /* Should have detected high latency */
    EXPECT_TRUE(effects.high_latency_event || effects.routing_surprise > 0.0f);
}

TEST_F(BioRouterFepBridgeTest, RoutingErrorsTracked) {
    /* Observe failed routing */
    bio_router_fep_observe_routing(bridge, (bio_module_id_t)0x0200, 0.0f, false);

    fep_bio_router_effects_t effects;
    bio_router_fep_get_router_effects(bridge, &effects);

    EXPECT_EQ(effects.routing_errors, 1u);
}

TEST_F(BioRouterFepBridgeTest, CongestionAvoidanceComputed) {
    bio_router_fep_update_effects(bridge);

    bio_router_fep_effects_t effects;
    bio_router_fep_get_effects(bridge, &effects);

    /* Congestion estimate should be in valid range */
    EXPECT_GE(effects.congestion_estimate, 0.0f);
    EXPECT_LE(effects.congestion_estimate, 1.0f);
}
