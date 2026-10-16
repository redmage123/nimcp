/**
 * @file test_nimcp_occipital_thalamic_bridge.cpp
 * @brief Unit tests for nimcp_occipital_thalamic_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Thalamic bridge
 * WHY:  Ensure correct LGN/Pulvinar/SC routing and attention modulation
 * HOW:  Use Google Test framework to test signal routing, attention,
 *       pathway separation, and bio-async messaging.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_thalamic_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "middleware/routing/nimcp_thalamic_router.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalThalamicBridgeTest : public ::testing::Test {
protected:
    occipital_thalamic_bridge_t* bridge;
    occipital_adapter_t* occipital;
    thalamic_router_t* router;
    occipital_thalamic_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create thalamic router
        thalamic_router_config_t router_cfg = thalamic_router_default_config();
        router = thalamic_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create thalamic bridge with real router
        config = occipital_thalamic_default_config();
        bridge = occipital_thalamic_bridge_create(occipital, router, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_thalamic_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
        if (router) {
            thalamic_router_destroy(router);
            router = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_thalamic_config_t default_config = occipital_thalamic_default_config();

    EXPECT_TRUE(default_config.enable_attention_gating);
    EXPECT_TRUE(default_config.enable_contrast_boost);
    EXPECT_TRUE(default_config.enable_retinotopic_routing);
    EXPECT_TRUE(default_config.enable_magno_parvo_separation);

    // Pathway boosts should be positive
    EXPECT_GT(default_config.magnocellular_boost, 0.0f);
    EXPECT_GT(default_config.parvocellular_boost, 0.0f);
    EXPECT_GT(default_config.koniocellular_boost, 0.0f);

    // Latencies should be biologically plausible (2-10ms for LGN)
    EXPECT_GT(default_config.lgn_latency_ms, 0.0f);
    EXPECT_LT(default_config.lgn_latency_ms, 20.0f);
}

TEST_F(OccipitalThalamicBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_thalamic_config_t retrieved;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.enable_attention_gating, retrieved.enable_attention_gating);
    EXPECT_FLOAT_EQ(config.magnocellular_boost, retrieved.magnocellular_boost);
}

TEST_F(OccipitalThalamicBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_thalamic_config_t retrieved;
    EXPECT_EQ(-1, occipital_thalamic_bridge_get_config(nullptr, &retrieved));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, CreateWithNullOccipitalSucceeds) {
    occipital_thalamic_bridge_t* standalone = occipital_thalamic_bridge_create(
        NULL, router, &config);
    EXPECT_NE(nullptr, standalone);
    occipital_thalamic_bridge_destroy(standalone);
}

TEST_F(OccipitalThalamicBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_thalamic_bridge_t* default_bridge = occipital_thalamic_bridge_create(
        occipital, router, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_thalamic_config_t retrieved;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_config(default_bridge, &retrieved));
    EXPECT_TRUE(retrieved.enable_attention_gating);

    occipital_thalamic_bridge_destroy(default_bridge);
}

TEST_F(OccipitalThalamicBridgeTest, DestroyNullDoesNotCrash) {
    occipital_thalamic_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalThalamicBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_thalamic_bridge_reset(bridge));
}

TEST_F(OccipitalThalamicBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_thalamic_bridge_reset(nullptr));
}

// ============================================================================
// SIGNAL ROUTING TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, RouteV1SignalSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float test_data[64] = {0.5f};  // Simple test signal
    EXPECT_EQ(0, occipital_thalamic_route_v1(bridge, test_data, 0.8f));
}

TEST_F(OccipitalThalamicBridgeTest, RouteV1NullBridgeFails) {
    float test_data[64] = {0.5f};
    EXPECT_EQ(-1, occipital_thalamic_route_v1(nullptr, test_data, 0.8f));
}

TEST_F(OccipitalThalamicBridgeTest, RouteV1NullDataSucceeds) {
    ASSERT_NE(nullptr, bridge);
    // NULL data with intensity should still work (just intensity signal)
    EXPECT_EQ(0, occipital_thalamic_route_v1(bridge, NULL, 0.8f));
}

TEST_F(OccipitalThalamicBridgeTest, RouteDorsalStreamSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float motion_data[32] = {0.3f};
    EXPECT_EQ(0, occipital_thalamic_route_dorsal(bridge, motion_data, 0.7f));
}

TEST_F(OccipitalThalamicBridgeTest, RouteVentralStreamSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float form_data[32] = {0.4f};
    EXPECT_EQ(0, occipital_thalamic_route_ventral(bridge, form_data, 0.6f));
}

TEST_F(OccipitalThalamicBridgeTest, RouteSignalWithFullStruct) {
    ASSERT_NE(nullptr, bridge);

    occipital_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = OCCIPITAL_SIGNAL_V1;
    signal.pathway = LGN_PATHWAY_PARVOCELLULAR;
    signal.visual_intensity = 0.75f;
    signal.contrast = 0.5f;
    signal.spatial_frequency = 4.0f;
    signal.temporal_frequency = 10.0f;
    signal.retinotopic_x = 0.3f;
    signal.retinotopic_y = 0.4f;

    EXPECT_EQ(0, occipital_thalamic_route_signal(bridge, &signal));
}

TEST_F(OccipitalThalamicBridgeTest, RouteSignalNullBridgeFails) {
    occipital_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    EXPECT_EQ(-1, occipital_thalamic_route_signal(nullptr, &signal));
}

// ============================================================================
// ADVANCED ROUTING TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, AdvancedRoutingSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float signal_data[16] = {0.5f};
    occipital_thalamic_request_t request;
    memset(&request, 0, sizeof(request));
    request.source = OCCIPITAL_THALAMIC_LGN;
    request.signal = signal_data;
    request.signal_dim = 16;
    request.urgency = 0.8f;
    request.attention_boost = 0.5f;

    occipital_thalamic_response_t response;
    memset(&response, 0, sizeof(response));

    EXPECT_EQ(0, occipital_thalamic_route_advanced(bridge, &request, &response));

    // Response should have valid values
    EXPECT_GE(response.effective_gain, 0.0f);
    EXPECT_FALSE(response.was_suppressed);  // Normal signal shouldn't be suppressed
}

TEST_F(OccipitalThalamicBridgeTest, AdvancedRoutingNullRequestFails) {
    ASSERT_NE(nullptr, bridge);
    occipital_thalamic_response_t response;
    EXPECT_EQ(-1, occipital_thalamic_route_advanced(bridge, nullptr, &response));
}

// ============================================================================
// ATTENTION TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, SetAttentionSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_thalamic_set_attention(bridge, 0.8f));
}

TEST_F(OccipitalThalamicBridgeTest, SetAttentionClampsValue) {
    ASSERT_NE(nullptr, bridge);

    // Above 1.0 should clamp
    EXPECT_EQ(0, occipital_thalamic_set_attention(bridge, 2.0f));

    float attention;
    occipital_thalamic_get_attention(bridge, &attention);
    EXPECT_LE(attention, 1.0f);

    // Below 0.0 should clamp
    EXPECT_EQ(0, occipital_thalamic_set_attention(bridge, -0.5f));
    occipital_thalamic_get_attention(bridge, &attention);
    EXPECT_GE(attention, 0.0f);
}

TEST_F(OccipitalThalamicBridgeTest, GetAttentionSucceeds) {
    ASSERT_NE(nullptr, bridge);
    occipital_thalamic_set_attention(bridge, 0.75f);

    float attention;
    EXPECT_EQ(0, occipital_thalamic_get_attention(bridge, &attention));
    EXPECT_FLOAT_EQ(0.75f, attention);
}

TEST_F(OccipitalThalamicBridgeTest, SetSpatialAttentionSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_thalamic_set_spatial_attention(bridge, 0.5f, 0.5f, 0.2f));
}

TEST_F(OccipitalThalamicBridgeTest, SetNucleusGainSucceeds) {
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, occipital_thalamic_set_nucleus_gain(bridge, OCCIPITAL_THALAMIC_LGN, 1.5f));
    EXPECT_EQ(0, occipital_thalamic_set_nucleus_gain(bridge, OCCIPITAL_THALAMIC_PULVINAR, 0.8f));
    EXPECT_EQ(0, occipital_thalamic_set_nucleus_gain(bridge, OCCIPITAL_THALAMIC_SC, 1.0f));
    EXPECT_EQ(0, occipital_thalamic_set_nucleus_gain(bridge, OCCIPITAL_THALAMIC_TRN, 0.5f));
}

TEST_F(OccipitalThalamicBridgeTest, SetNucleusGainInvalidNucleusFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_thalamic_set_nucleus_gain(bridge,
        (occipital_thalamic_nucleus_t)100, 1.0f));
}

// ============================================================================
// STATE TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, GetStateSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Set some state first
    occipital_thalamic_set_attention(bridge, 0.8f);
    occipital_thalamic_set_spatial_attention(bridge, 0.3f, 0.4f, 0.1f);

    occipital_thalamic_state_t state;
    memset(&state, 0, sizeof(state));
    EXPECT_EQ(0, occipital_thalamic_get_state(bridge, &state));

    EXPECT_FLOAT_EQ(0.8f, state.pulvinar_attention);
    EXPECT_FLOAT_EQ(0.3f, state.attention_x);
    EXPECT_FLOAT_EQ(0.4f, state.attention_y);
    EXPECT_FLOAT_EQ(0.1f, state.attention_radius);
}

TEST_F(OccipitalThalamicBridgeTest, GetStateWithNullBridgeFails) {
    occipital_thalamic_state_t state;
    EXPECT_EQ(-1, occipital_thalamic_get_state(nullptr, &state));
}

TEST_F(OccipitalThalamicBridgeTest, GetStateWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_thalamic_get_state(bridge, nullptr));
}

// ============================================================================
// CORTICAL FEEDBACK TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, ApplyFeedbackSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float feedback[32] = {0.5f};
    EXPECT_EQ(0, occipital_thalamic_apply_feedback(bridge, feedback, 32));
}

TEST_F(OccipitalThalamicBridgeTest, ApplyFeedbackNullSignalSucceeds) {
    ASSERT_NE(nullptr, bridge);
    // NULL signal should disable feedback
    EXPECT_EQ(0, occipital_thalamic_apply_feedback(bridge, NULL, 0));

    occipital_thalamic_state_t state;
    occipital_thalamic_get_state(bridge, &state);
    EXPECT_FALSE(state.feedback_active);
}

// ============================================================================
// QUERY TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, IsLgnActiveReturnsTrue) {
    ASSERT_NE(nullptr, bridge);
    // LGN should be active by default
    EXPECT_TRUE(occipital_thalamic_is_lgn_active(bridge));
}

TEST_F(OccipitalThalamicBridgeTest, IsLgnActiveNullReturnsFalse) {
    EXPECT_FALSE(occipital_thalamic_is_lgn_active(nullptr));
}

TEST_F(OccipitalThalamicBridgeTest, NucleusNameReturnsValid) {
    const char* lgn = occipital_thalamic_nucleus_name(OCCIPITAL_THALAMIC_LGN);
    ASSERT_NE(nullptr, lgn);
    EXPECT_STRNE("", lgn);

    const char* pulvinar = occipital_thalamic_nucleus_name(OCCIPITAL_THALAMIC_PULVINAR);
    ASSERT_NE(nullptr, pulvinar);
    EXPECT_STRNE("", pulvinar);

    const char* sc = occipital_thalamic_nucleus_name(OCCIPITAL_THALAMIC_SC);
    ASSERT_NE(nullptr, sc);

    const char* trn = occipital_thalamic_nucleus_name(OCCIPITAL_THALAMIC_TRN);
    ASSERT_NE(nullptr, trn);
}

TEST_F(OccipitalThalamicBridgeTest, PathwayNameReturnsValid) {
    const char* magno = occipital_thalamic_pathway_name(LGN_PATHWAY_MAGNOCELLULAR);
    ASSERT_NE(nullptr, magno);
    EXPECT_STRNE("", magno);

    const char* parvo = occipital_thalamic_pathway_name(LGN_PATHWAY_PARVOCELLULAR);
    ASSERT_NE(nullptr, parvo);

    const char* konio = occipital_thalamic_pathway_name(LGN_PATHWAY_KONIOCELLULAR);
    ASSERT_NE(nullptr, konio);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_thalamic_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.v1_signals_routed);
    EXPECT_EQ(0ULL, stats.dorsal_signals);
    EXPECT_EQ(0ULL, stats.ventral_signals);
}

TEST_F(OccipitalThalamicBridgeTest, GetStatsUpdatesAfterRouting) {
    ASSERT_NE(nullptr, bridge);

    // Route some signals
    float test_data[16] = {0.5f};
    occipital_thalamic_route_v1(bridge, test_data, 0.8f);
    occipital_thalamic_route_v1(bridge, test_data, 0.7f);
    occipital_thalamic_route_dorsal(bridge, test_data, 0.6f);

    occipital_thalamic_stats_t stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(bridge, &stats));
    EXPECT_GE(stats.v1_signals_routed, 2ULL);
    EXPECT_GE(stats.dorsal_signals, 1ULL);
}

TEST_F(OccipitalThalamicBridgeTest, ResetStatsWorks) {
    ASSERT_NE(nullptr, bridge);

    // Generate some stats
    float test_data[16] = {0.5f};
    for (int i = 0; i < 10; i++) {
        occipital_thalamic_route_v1(bridge, test_data, 0.8f);
    }

    // Reset
    occipital_thalamic_bridge_reset_stats(bridge);

    // Verify
    occipital_thalamic_stats_t stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.v1_signals_routed);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, RegisterBioAsyncSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_thalamic_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalThalamicBridgeTest, BroadcastRoutingSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_thalamic_bridge_broadcast_routing(bridge));
}

TEST_F(OccipitalThalamicBridgeTest, ProcessMessagesSucceeds) {
    ASSERT_NE(nullptr, bridge);
    int result = occipital_thalamic_bridge_process_messages(bridge, 0);
    EXPECT_GE(result, 0);
}

// ============================================================================
// PATHWAY SEPARATION TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, MagnoCellularPathwayRouting) {
    ASSERT_NE(nullptr, bridge);

    occipital_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = OCCIPITAL_SIGNAL_MAGNO;
    signal.pathway = LGN_PATHWAY_MAGNOCELLULAR;
    signal.visual_intensity = 0.8f;
    signal.contrast = 0.2f;  // Low contrast (M-pathway specialization)
    signal.temporal_frequency = 30.0f;  // High temporal frequency

    EXPECT_EQ(0, occipital_thalamic_route_signal(bridge, &signal));

    occipital_thalamic_stats_t stats;
    occipital_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.magno_signals, 1ULL);
}

TEST_F(OccipitalThalamicBridgeTest, ParvoCellularPathwayRouting) {
    ASSERT_NE(nullptr, bridge);

    occipital_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = OCCIPITAL_SIGNAL_PARVO;
    signal.pathway = LGN_PATHWAY_PARVOCELLULAR;
    signal.visual_intensity = 0.9f;
    signal.contrast = 0.8f;  // High contrast (P-pathway specialization)
    signal.spatial_frequency = 20.0f;  // High spatial frequency

    EXPECT_EQ(0, occipital_thalamic_route_signal(bridge, &signal));

    occipital_thalamic_stats_t stats;
    occipital_thalamic_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.parvo_signals, 1ULL);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalThalamicBridgeTest, RepeatedRoutingDoesNotLeak) {
    ASSERT_NE(nullptr, bridge);

    float test_data[16] = {0.5f};
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_thalamic_route_v1(bridge, test_data, 0.8f));
    }

    occipital_thalamic_stats_t stats;
    EXPECT_EQ(0, occipital_thalamic_bridge_get_stats(bridge, &stats));
    EXPECT_GE(stats.v1_signals_routed, 1000ULL);
}

TEST_F(OccipitalThalamicBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_thalamic_bridge_t* temp = occipital_thalamic_bridge_create(
            occipital, router, &config);
        ASSERT_NE(nullptr, temp);
        occipital_thalamic_bridge_destroy(temp);
    }
    SUCCEED();
}
