/**
 * @file test_nimcp_occipital_audiovisual_bridge.cpp
 * @brief Unit tests for nimcp_occipital_audiovisual_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Audiovisual bridge
 * WHY:  Ensure correct lip reading, gesture-speech binding, and McGurk integration
 * HOW:  Use Google Test framework to test lifecycle, processing, and statistics
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalAudiovisualBridgeTest : public ::testing::Test {
protected:
    occipital_audiovisual_bridge_t* bridge;
    occipital_adapter_t* occipital;
    occipital_av_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create audiovisual bridge
        config = occipital_av_default_config();
        bridge = occipital_av_bridge_create(occipital, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_av_bridge_destroy(bridge);
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

TEST_F(OccipitalAudiovisualBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_av_config_t default_config = occipital_av_default_config();

    EXPECT_TRUE(default_config.enable_lip_reading);
    EXPECT_TRUE(default_config.enable_gesture_binding);
    EXPECT_GT(default_config.lip_audio_offset_ms, 0.0f);
    EXPECT_GT(default_config.gesture_window_ms, 0.0f);
    EXPECT_GT(default_config.lip_confidence_threshold, 0.0f);
    EXPECT_LT(default_config.lip_confidence_threshold, 1.0f);
}

TEST_F(OccipitalAudiovisualBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_av_config_t retrieved;
    EXPECT_EQ(0, occipital_av_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.enable_lip_reading, retrieved.enable_lip_reading);
    EXPECT_EQ(config.enable_gesture_binding, retrieved.enable_gesture_binding);
    EXPECT_FLOAT_EQ(config.lip_audio_offset_ms, retrieved.lip_audio_offset_ms);
}

TEST_F(OccipitalAudiovisualBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_av_config_t retrieved;
    EXPECT_EQ(-1, occipital_av_bridge_get_config(nullptr, &retrieved));
}

TEST_F(OccipitalAudiovisualBridgeTest, GetConfigWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_av_bridge_get_config(bridge, nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, CreateWithNullOccipitalReturnsNull) {
    // Bridge requires occipital lobe connection
    occipital_audiovisual_bridge_t* standalone = occipital_av_bridge_create(
        NULL, &config);
    EXPECT_EQ(nullptr, standalone);
}

TEST_F(OccipitalAudiovisualBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_audiovisual_bridge_t* default_bridge = occipital_av_bridge_create(
        occipital, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_av_config_t retrieved;
    EXPECT_EQ(0, occipital_av_bridge_get_config(default_bridge, &retrieved));
    EXPECT_TRUE(retrieved.enable_lip_reading);

    occipital_av_bridge_destroy(default_bridge);
}

TEST_F(OccipitalAudiovisualBridgeTest, DestroyNullDoesNotCrash) {
    occipital_av_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalAudiovisualBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_av_bridge_reset(bridge));
}

TEST_F(OccipitalAudiovisualBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_bridge_reset(nullptr));
}

// ============================================================================
// CONNECTION TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, ConnectAudioCortexNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    // Should succeed with NULL (no audio cortex connected)
    EXPECT_EQ(0, occipital_av_connect_audio_cortex(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ConnectAudioCortexNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_connect_audio_cortex(nullptr, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ConnectBrocaNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_av_connect_broca(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ConnectBrocaNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_connect_broca(nullptr, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ConnectSpeechCortexNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_av_connect_speech_cortex(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ConnectSpeechCortexNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_connect_speech_cortex(nullptr, nullptr));
}

// ============================================================================
// PROCESSING TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, UpdateSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_av_bridge_update(bridge));
}

TEST_F(OccipitalAudiovisualBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_bridge_update(nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ProcessObservationSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_speech_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.type = VISUAL_SPEECH_LIP_SHAPE;
    obs.confidence = 0.8f;
    obs.timestamp_us = 1000000;

    EXPECT_EQ(0, occipital_av_process_observation(bridge, &obs));
}

TEST_F(OccipitalAudiovisualBridgeTest, ProcessObservationNullBridgeFails) {
    visual_speech_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    EXPECT_EQ(-1, occipital_av_process_observation(nullptr, &obs));
}

TEST_F(OccipitalAudiovisualBridgeTest, ProcessObservationNullObsFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_av_process_observation(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, GetEffectsReturnsValidValues) {
    ASSERT_NE(nullptr, bridge);

    occipital_av_bridge_update(bridge);

    occipital_av_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_av_bridge_get_effects(bridge, &effects));

    // Effects should be in valid range
    EXPECT_GE(effects.lip_phoneme_confidence, 0.0f);
    EXPECT_LE(effects.lip_phoneme_confidence, 1.0f);
}

TEST_F(OccipitalAudiovisualBridgeTest, GetEffectsNullBridgeFails) {
    occipital_av_effects_t effects;
    EXPECT_EQ(-1, occipital_av_bridge_get_effects(nullptr, &effects));
}

TEST_F(OccipitalAudiovisualBridgeTest, GetEffectsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_av_bridge_get_effects(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ApplyEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);
    occipital_av_bridge_update(bridge);
    EXPECT_EQ(0, occipital_av_bridge_apply_effects(bridge));
}

TEST_F(OccipitalAudiovisualBridgeTest, ApplyEffectsNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_bridge_apply_effects(nullptr));
}

// ============================================================================
// PREDICTION TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, PredictAudioTimingSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_av_bridge_update(bridge);

    float predicted_onset_ms;
    uint32_t predicted_phoneme_id;
    float confidence;
    EXPECT_EQ(0, occipital_av_predict_audio_timing(bridge,
        &predicted_onset_ms, &predicted_phoneme_id, &confidence));

    // Confidence should be valid
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(OccipitalAudiovisualBridgeTest, PredictAudioTimingNullBridgeFails) {
    float predicted_onset_ms;
    uint32_t predicted_phoneme_id;
    float confidence;
    EXPECT_EQ(-1, occipital_av_predict_audio_timing(nullptr,
        &predicted_onset_ms, &predicted_phoneme_id, &confidence));
}

TEST_F(OccipitalAudiovisualBridgeTest, ReportAudioEventSucceeds) {
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, occipital_av_report_audio_event(bridge, 150.0f, 5));
}

TEST_F(OccipitalAudiovisualBridgeTest, ReportAudioEventNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_report_audio_event(nullptr, 150.0f, 5));
}

// ============================================================================
// CONNECTION QUERY TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, IsAudioConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_av_is_audio_connected(bridge));
}

TEST_F(OccipitalAudiovisualBridgeTest, IsBrocaConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_av_is_broca_connected(bridge));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_av_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_av_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.lip_observations);
    EXPECT_EQ(0ULL, stats.gesture_observations);
}

TEST_F(OccipitalAudiovisualBridgeTest, GetStatsUpdatesAfterProcessing) {
    ASSERT_NE(nullptr, bridge);

    // Process some observations
    visual_speech_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.type = VISUAL_SPEECH_LIP_SHAPE;
    obs.confidence = 0.8f;

    for (int i = 0; i < 5; i++) {
        occipital_av_process_observation(bridge, &obs);
    }

    occipital_av_stats_t stats;
    EXPECT_EQ(0, occipital_av_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(5ULL, stats.lip_observations);
}

TEST_F(OccipitalAudiovisualBridgeTest, GetStatsNullBridgeFails) {
    occipital_av_stats_t stats;
    EXPECT_EQ(-1, occipital_av_bridge_get_stats(nullptr, &stats));
}

TEST_F(OccipitalAudiovisualBridgeTest, GetStatsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_av_bridge_get_stats(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, ResetStatsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Process some observations
    visual_speech_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.type = VISUAL_SPEECH_LIP_SHAPE;
    obs.confidence = 0.8f;
    occipital_av_process_observation(bridge, &obs);

    // Reset stats
    occipital_av_bridge_reset_stats(bridge);

    occipital_av_stats_t stats;
    EXPECT_EQ(0, occipital_av_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.lip_observations);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, RegisterBioAsyncNullRouterSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_av_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalAudiovisualBridgeTest, RegisterBioAsyncNullBridgeFails) {
    EXPECT_EQ(-1, occipital_av_bridge_register_bio_async(nullptr, nullptr));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalAudiovisualBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_av_bridge_update(bridge));
    }

    SUCCEED();
}

TEST_F(OccipitalAudiovisualBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_audiovisual_bridge_t* temp = occipital_av_bridge_create(
            occipital, &config);
        ASSERT_NE(nullptr, temp);
        occipital_av_bridge_destroy(temp);
    }
    SUCCEED();
}

TEST_F(OccipitalAudiovisualBridgeTest, ProcessManyObservationsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    visual_speech_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.type = VISUAL_SPEECH_LIP_SHAPE;
    obs.confidence = 0.8f;

    for (int i = 0; i < 500; i++) {
        EXPECT_EQ(0, occipital_av_process_observation(bridge, &obs));
    }

    occipital_av_stats_t stats;
    EXPECT_EQ(0, occipital_av_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(500ULL, stats.lip_observations);
}
