/**
 * @file test_nimcp_occipital_cognitive_bridge.cpp
 * @brief Unit tests for nimcp_occipital_cognitive_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Cognitive bridge
 * WHY:  Ensure correct integration with all cognitive modules
 * HOW:  Use Google Test framework to test events, modulation, and stats
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_cognitive_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalCognitiveBridgeTest : public ::testing::Test {
protected:
    occipital_cognitive_bridge_t* bridge;
    occipital_adapter_t* occipital;
    occipital_cognitive_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create cognitive bridge
        config = occipital_cognitive_default_config();
        bridge = occipital_cognitive_bridge_create(occipital, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_cognitive_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_cognitive_config_t default_config = occipital_cognitive_default_config();

    EXPECT_TRUE(default_config.enable_bidirectional);
    EXPECT_GT(default_config.global_gain, 0.0f);
}

TEST_F(OccipitalCognitiveBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_cognitive_config_t retrieved;
    EXPECT_EQ(0, occipital_cognitive_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.enable_bidirectional, retrieved.enable_bidirectional);
    EXPECT_EQ(config.enable_bio_async, retrieved.enable_bio_async);
}

TEST_F(OccipitalCognitiveBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_cognitive_config_t retrieved;
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_config(nullptr, &retrieved));
}

TEST_F(OccipitalCognitiveBridgeTest, GetConfigWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_config(bridge, nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, CreateWithNullOccipitalReturnsNull) {
    // Bridge requires occipital lobe connection
    occipital_cognitive_bridge_t* standalone = occipital_cognitive_bridge_create(
        NULL, &config);
    EXPECT_EQ(nullptr, standalone);
}

TEST_F(OccipitalCognitiveBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_cognitive_bridge_t* default_bridge = occipital_cognitive_bridge_create(
        occipital, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_cognitive_config_t retrieved;
    EXPECT_EQ(0, occipital_cognitive_bridge_get_config(default_bridge, &retrieved));
    EXPECT_TRUE(retrieved.enable_bidirectional);

    occipital_cognitive_bridge_destroy(default_bridge);
}

TEST_F(OccipitalCognitiveBridgeTest, DestroyNullDoesNotCrash) {
    occipital_cognitive_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalCognitiveBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_bridge_reset(bridge));
}

TEST_F(OccipitalCognitiveBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_bridge_reset(nullptr));
}

// ============================================================================
// MODULE CONNECTION TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, ConnectModuleSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Connect with NULL module pointer (for testing without real module)
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_EMOTION, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ConnectModuleNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_connect_module(nullptr, COG_MODULE_EMOTION, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, DisconnectModuleSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Connect then disconnect
    occipital_cognitive_connect_module(bridge, COG_MODULE_ATTENTION, nullptr);
    EXPECT_EQ(0, occipital_cognitive_disconnect_module(bridge, COG_MODULE_ATTENTION));
}

TEST_F(OccipitalCognitiveBridgeTest, DisconnectModuleNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_disconnect_module(nullptr, COG_MODULE_ATTENTION));
}

TEST_F(OccipitalCognitiveBridgeTest, ConnectAllModuleTypes) {
    ASSERT_NE(nullptr, bridge);

    // Test connecting all module types
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_EMOTION, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_ATTENTION, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_MEMORY, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_SALIENCE, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_CURIOSITY, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_INTROSPECTION, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_GLOBAL_WORKSPACE, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_KNOWLEDGE, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_THEORY_OF_MIND, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_SOCIAL, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_REASONING, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_PREDICTIVE, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_FREE_ENERGY, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_PERSONALITY, nullptr));
    EXPECT_EQ(0, occipital_cognitive_connect_module(bridge, COG_MODULE_MIRROR_NEURONS, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ConnectSubstrateNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_connect_substrate(bridge, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ConnectSubstrateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_connect_substrate(nullptr, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, IsModuleConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_cognitive_is_module_connected(bridge, COG_MODULE_EMOTION));
}

TEST_F(OccipitalCognitiveBridgeTest, GetActiveModuleCountInitiallyZero) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0u, occipital_cognitive_get_active_module_count(bridge));
}

TEST_F(OccipitalCognitiveBridgeTest, SetModuleEnabledSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_set_module_enabled(bridge, COG_MODULE_EMOTION, true));
}

TEST_F(OccipitalCognitiveBridgeTest, SetModuleEnabledNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_set_module_enabled(nullptr, COG_MODULE_EMOTION, true));
}

// ============================================================================
// EVENT TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, SendEventSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_cognitive_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = 1;  // Object detected event
    event.target = COG_MODULE_ATTENTION;
    event.salience = 0.8f;
    event.urgency = 0.5f;

    EXPECT_EQ(0, occipital_cognitive_send_event(bridge, &event));
}

TEST_F(OccipitalCognitiveBridgeTest, SendEventNullBridgeFails) {
    visual_cognitive_event_t event;
    memset(&event, 0, sizeof(event));
    EXPECT_EQ(-1, occipital_cognitive_send_event(nullptr, &event));
}

TEST_F(OccipitalCognitiveBridgeTest, SendEventNullEventFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_send_event(bridge, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, BroadcastSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float features[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    EXPECT_GE(occipital_cognitive_broadcast(bridge, features, 8, 0.9f), 0);
}

TEST_F(OccipitalCognitiveBridgeTest, BroadcastNullBridgeFails) {
    float features[8] = {0.0f};
    EXPECT_EQ(-1, occipital_cognitive_broadcast(nullptr, features, 8, 0.5f));
}

// ============================================================================
// MODULATION TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, GetModulationReturnsValidValues) {
    ASSERT_NE(nullptr, bridge);

    occipital_cognitive_bridge_update(bridge);

    cognitive_modulation_t modulation;
    memset(&modulation, 0, sizeof(modulation));
    EXPECT_EQ(0, occipital_cognitive_get_modulation(bridge, &modulation));

    // Modulation values should be in valid range
    EXPECT_GE(modulation.attention_gain, 0.0f);
    EXPECT_LE(modulation.attention_gain, 2.0f);  // Allow some boost
}

TEST_F(OccipitalCognitiveBridgeTest, GetModulationNullBridgeFails) {
    cognitive_modulation_t modulation;
    EXPECT_EQ(-1, occipital_cognitive_get_modulation(nullptr, &modulation));
}

TEST_F(OccipitalCognitiveBridgeTest, GetModulationNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_get_modulation(bridge, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ApplyModulationSucceeds) {
    ASSERT_NE(nullptr, bridge);

    cognitive_modulation_t modulation;
    memset(&modulation, 0, sizeof(modulation));
    modulation.attention_gain = 1.0f;
    modulation.emotional_valence = 0.5f;
    modulation.emotional_arousal = 0.5f;

    EXPECT_EQ(0, occipital_cognitive_apply_modulation(bridge, &modulation));
}

TEST_F(OccipitalCognitiveBridgeTest, ApplyModulationNullBridgeFails) {
    cognitive_modulation_t modulation;
    memset(&modulation, 0, sizeof(modulation));
    EXPECT_EQ(-1, occipital_cognitive_apply_modulation(nullptr, &modulation));
}

TEST_F(OccipitalCognitiveBridgeTest, ApplyModulationNullModFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_apply_modulation(bridge, nullptr));
}

// ============================================================================
// PROCESSING TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, UpdateSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_bridge_update(bridge));
}

TEST_F(OccipitalCognitiveBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_bridge_update(nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ProcessSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_bridge_process(bridge));
}

TEST_F(OccipitalCognitiveBridgeTest, ProcessNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_bridge_process(nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, GetEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_cognitive_bridge_update(bridge);

    occipital_cognitive_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_cognitive_bridge_get_effects(bridge, &effects));
}

TEST_F(OccipitalCognitiveBridgeTest, GetEffectsNullBridgeFails) {
    occipital_cognitive_effects_t effects;
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_effects(nullptr, &effects));
}

TEST_F(OccipitalCognitiveBridgeTest, GetEffectsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_effects(bridge, nullptr));
}

// ============================================================================
// MODULE-SPECIFIC API TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, SendEmotionCueSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_send_emotion_cue(bridge, 0.5f, 0.7f, 1, 0.9f));
}

TEST_F(OccipitalCognitiveBridgeTest, SendEmotionCueNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_send_emotion_cue(nullptr, 0.5f, 0.7f, 1, 0.9f));
}

TEST_F(OccipitalCognitiveBridgeTest, ReportSalienceSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_report_salience(bridge, 0.5f, 0.5f, 0.8f, 1));
}

TEST_F(OccipitalCognitiveBridgeTest, ReportSalienceNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_report_salience(nullptr, 0.5f, 0.5f, 0.8f, 1));
}

TEST_F(OccipitalCognitiveBridgeTest, QueryMemorySucceeds) {
    ASSERT_NE(nullptr, bridge);

    float pattern[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float match_score = 0.0f;
    uint32_t match_id = 0;

    EXPECT_EQ(0, occipital_cognitive_query_memory(bridge, pattern, 8,
        &match_score, &match_id));
}

TEST_F(OccipitalCognitiveBridgeTest, QueryMemoryNullBridgeFails) {
    float pattern[8] = {0.0f};
    float match_score;
    uint32_t match_id;
    EXPECT_EQ(-1, occipital_cognitive_query_memory(nullptr, pattern, 8,
        &match_score, &match_id));
}

TEST_F(OccipitalCognitiveBridgeTest, ReportNoveltySucceeds) {
    ASSERT_NE(nullptr, bridge);

    float features[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    EXPECT_EQ(0, occipital_cognitive_report_novelty(bridge, 0.9f, features, 8));
}

TEST_F(OccipitalCognitiveBridgeTest, ReportNoveltyNullBridgeFails) {
    float features[8] = {0.0f};
    EXPECT_EQ(-1, occipital_cognitive_report_novelty(nullptr, 0.9f, features, 8));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_cognitive_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_cognitive_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.total_events_sent);
    EXPECT_EQ(0ULL, stats.total_modulations_received);
}

TEST_F(OccipitalCognitiveBridgeTest, GetStatsUpdatesAfterEvents) {
    ASSERT_NE(nullptr, bridge);

    // Send some events
    visual_cognitive_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = 1;
    event.target = COG_MODULE_ATTENTION;

    for (int i = 0; i < 5; i++) {
        occipital_cognitive_send_event(bridge, &event);
    }

    occipital_cognitive_stats_t stats;
    EXPECT_EQ(0, occipital_cognitive_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(5ULL, stats.total_events_sent);
}

TEST_F(OccipitalCognitiveBridgeTest, GetStatsNullBridgeFails) {
    occipital_cognitive_stats_t stats;
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_stats(nullptr, &stats));
}

TEST_F(OccipitalCognitiveBridgeTest, GetStatsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cognitive_bridge_get_stats(bridge, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, ResetStatsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Send some events
    visual_cognitive_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = 1;
    event.target = COG_MODULE_ATTENTION;
    occipital_cognitive_send_event(bridge, &event);

    // Reset stats
    occipital_cognitive_bridge_reset_stats(bridge);

    occipital_cognitive_stats_t stats;
    EXPECT_EQ(0, occipital_cognitive_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.total_events_sent);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, RegisterBioAsyncNullRouterSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cognitive_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalCognitiveBridgeTest, RegisterBioAsyncNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cognitive_bridge_register_bio_async(nullptr, nullptr));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalCognitiveBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_cognitive_bridge_update(bridge));
    }

    SUCCEED();
}

TEST_F(OccipitalCognitiveBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_cognitive_bridge_t* temp = occipital_cognitive_bridge_create(
            occipital, &config);
        ASSERT_NE(nullptr, temp);
        occipital_cognitive_bridge_destroy(temp);
    }
    SUCCEED();
}

TEST_F(OccipitalCognitiveBridgeTest, SendManyEventsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_cognitive_event_t event;
    memset(&event, 0, sizeof(event));
    event.event_type = 1;
    event.target = COG_MODULE_SALIENCE;

    for (int i = 0; i < 500; i++) {
        EXPECT_EQ(0, occipital_cognitive_send_event(bridge, &event));
    }

    occipital_cognitive_stats_t stats;
    EXPECT_EQ(0, occipital_cognitive_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(500ULL, stats.total_events_sent);
}
