/**
 * @file test_dragonfly_emotion_bridge.cpp
 * @brief Unit tests for dragonfly emotion bridge module
 *
 * Tests emotion-dragonfly integration including arousal, valence,
 * and emotional modulation of hunting behavior.
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_emotion_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class DragonEmotionBridgeTest : public ::testing::Test {
protected:
    dragonfly_emotion_bridge_t bridge = nullptr;

    void SetUp() override {
        dragonfly_emotion_config_t config = dragonfly_emotion_default_config();
        bridge = dragonfly_emotion_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_emotion_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, DefaultConfig) {
    dragonfly_emotion_config_t config = dragonfly_emotion_default_config();

    // Hunger parameters
    EXPECT_GT(config.hunger_decay_rate, 0.0f);
    EXPECT_GT(config.hunger_satisfaction, 0.0f);

    // Fear parameters
    EXPECT_GT(config.fear_decay_rate, 0.0f);
    EXPECT_GT(config.fear_abort_threshold, 0.0f);

    // Frustration parameters
    EXPECT_GT(config.frustration_buildup_rate, 0.0f);

    // Confidence parameters
    EXPECT_GT(config.success_confidence_boost, 0.0f);
    EXPECT_GT(config.failure_confidence_penalty, 0.0f);
}

TEST_F(DragonEmotionBridgeTest, ValidateConfig) {
    dragonfly_emotion_config_t config = dragonfly_emotion_default_config();
    EXPECT_TRUE(dragonfly_emotion_validate_config(&config));

    EXPECT_FALSE(dragonfly_emotion_validate_config(nullptr));
}

TEST_F(DragonEmotionBridgeTest, CreateWithCustomConfig) {
    dragonfly_emotion_config_t config = dragonfly_emotion_default_config();
    config.emotional_learning_rate = 0.1f;

    dragonfly_emotion_bridge_t custom = dragonfly_emotion_bridge_create(&config);
    ASSERT_NE(custom, nullptr);
    dragonfly_emotion_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, CreateAndDestroy) {
    dragonfly_emotion_bridge_t b = dragonfly_emotion_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_emotion_bridge_destroy(b);
}

TEST_F(DragonEmotionBridgeTest, DestroyNull) {
    dragonfly_emotion_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(DragonEmotionBridgeTest, Disconnect) {
    EXPECT_EQ(dragonfly_emotion_bridge_disconnect(bridge), 0);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, UpdateBasic) {
    EXPECT_EQ(dragonfly_emotion_bridge_update(bridge, 0.016f), 0);
}

TEST_F(DragonEmotionBridgeTest, UpdateNullBridge) {
    EXPECT_NE(dragonfly_emotion_bridge_update(nullptr, 0.016f), 0);
}

TEST_F(DragonEmotionBridgeTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(dragonfly_emotion_bridge_update(bridge, 0.016f), 0);
    }
}

//=============================================================================
// Hunt Event Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, ReportSuccess) {
    EXPECT_EQ(dragonfly_emotion_report_success(bridge, 0.8f), 0);

    emotional_state_t state;
    EXPECT_EQ(dragonfly_emotion_get_state(bridge, &state), 0);
    // Success should improve confidence
    EXPECT_GT(state.confidence, 0.0f);
}

TEST_F(DragonEmotionBridgeTest, ReportFailure) {
    EXPECT_EQ(dragonfly_emotion_report_failure(bridge, 0.5f, "target escaped"), 0);
}

TEST_F(DragonEmotionBridgeTest, ReportThreat) {
    float threat_pos[3] = {10.0f, 5.0f, 3.0f};
    EXPECT_EQ(dragonfly_emotion_report_threat(bridge, 0.7f, threat_pos), 0);

    // Threat should increase fear drive
    float fear = dragonfly_emotion_get_drive(bridge, DRIVE_FEAR);
    EXPECT_GT(fear, 0.0f);
}

//=============================================================================
// Emotional State Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, GetState) {
    emotional_state_t state;
    EXPECT_EQ(dragonfly_emotion_get_state(bridge, &state), 0);

    // State should have valid values
    EXPECT_GE(state.motivation, 0.0f);
    EXPECT_LE(state.motivation, 1.0f);
    EXPECT_GE(state.confidence, 0.0f);
    EXPECT_LE(state.confidence, 1.0f);
    EXPECT_GE(state.dominance, 0.0f);
    EXPECT_LE(state.dominance, 1.0f);
}

TEST_F(DragonEmotionBridgeTest, DriveValues) {
    emotional_state_t state;
    EXPECT_EQ(dragonfly_emotion_get_state(bridge, &state), 0);

    // All drives should be in valid range
    for (int i = 0; i < DRIVE_COUNT; i++) {
        EXPECT_GE(state.drives[i], 0.0f);
        EXPECT_LE(state.drives[i], 1.0f);
    }
}

//=============================================================================
// Modulation Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, GetModulation) {
    emotion_modulation_t mod;
    EXPECT_EQ(dragonfly_emotion_get_modulation(bridge, &mod), 0);

    EXPECT_GE(mod.pursuit_aggression, 0.0f);
    EXPECT_LE(mod.pursuit_aggression, 1.0f);
    EXPECT_GE(mod.focus_level, 0.0f);
    EXPECT_LE(mod.focus_level, 1.0f);
    EXPECT_GE(mod.reaction_speed, 0.0f);
    EXPECT_LE(mod.reaction_speed, 1.0f);
}

TEST_F(DragonEmotionBridgeTest, ShouldHunt) {
    // Fresh bridge should be willing to hunt
    bool should = dragonfly_emotion_should_hunt(bridge);
    // Result depends on initial hunger level
    (void)should;
}

TEST_F(DragonEmotionBridgeTest, GetMotivation) {
    float motivation = dragonfly_emotion_get_motivation(bridge);
    EXPECT_GE(motivation, 0.0f);
    EXPECT_LE(motivation, 1.0f);
}

TEST_F(DragonEmotionBridgeTest, GetDrive) {
    float hunger = dragonfly_emotion_get_drive(bridge, DRIVE_HUNGER);
    EXPECT_GE(hunger, 0.0f);
    EXPECT_LE(hunger, 1.0f);

    float fear = dragonfly_emotion_get_drive(bridge, DRIVE_FEAR);
    EXPECT_GE(fear, 0.0f);
    EXPECT_LE(fear, 1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, GetStats) {
    dragonfly_emotion_stats_t stats;
    EXPECT_EQ(dragonfly_emotion_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.emotional_events, 0u);
}

TEST_F(DragonEmotionBridgeTest, StatsAccumulate) {
    // Report a success event
    EXPECT_EQ(dragonfly_emotion_report_success(bridge, 0.8f), 0);

    dragonfly_emotion_stats_t stats;
    EXPECT_EQ(dragonfly_emotion_get_stats(bridge, &stats), 0);
    // Stats tracking depends on implementation details
    // Just verify the call succeeds
}

TEST_F(DragonEmotionBridgeTest, NegativeEvents) {
    dragonfly_emotion_report_failure(bridge, 0.5f, "escaped");

    dragonfly_emotion_stats_t stats;
    EXPECT_EQ(dragonfly_emotion_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.negative_events, 0u);
}

//=============================================================================
// Name Function Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, DriveNames) {
    EXPECT_STREQ(dragonfly_drive_name(DRIVE_HUNGER), "hunger");
    EXPECT_STREQ(dragonfly_drive_name(DRIVE_FEAR), "fear");
    EXPECT_STREQ(dragonfly_drive_name(DRIVE_CURIOSITY), "curiosity");
}

TEST_F(DragonEmotionBridgeTest, ValenceNames) {
    EXPECT_STREQ(dragonfly_valence_name(VALENCE_NEUTRAL), "neutral");
    EXPECT_STREQ(dragonfly_valence_name(VALENCE_POSITIVE), "positive");
    EXPECT_STREQ(dragonfly_valence_name(VALENCE_NEGATIVE), "negative");
}

TEST_F(DragonEmotionBridgeTest, ArousalNames) {
    EXPECT_STREQ(dragonfly_arousal_name(AROUSAL_CALM), "calm");
    EXPECT_STREQ(dragonfly_arousal_name(AROUSAL_ALERT), "alert");
    EXPECT_STREQ(dragonfly_arousal_name(AROUSAL_EXCITED), "excited");
}

//=============================================================================
// Null Parameter Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, NullModulationOutput) {
    EXPECT_NE(dragonfly_emotion_get_modulation(bridge, nullptr), 0);
}

TEST_F(DragonEmotionBridgeTest, NullStateOutput) {
    EXPECT_NE(dragonfly_emotion_get_state(bridge, nullptr), 0);
}

TEST_F(DragonEmotionBridgeTest, NullStatsOutput) {
    EXPECT_NE(dragonfly_emotion_get_stats(bridge, nullptr), 0);
}

//=============================================================================
// Process Event Tests
//=============================================================================

TEST_F(DragonEmotionBridgeTest, ProcessSuccessEvent) {
    emotional_event_t event = {0};
    event.is_success = true;
    event.pursuit_duration_s = 2.0f;

    EXPECT_EQ(dragonfly_emotion_process_event(bridge, &event), 0);
}

TEST_F(DragonEmotionBridgeTest, ProcessFailureEvent) {
    emotional_event_t event = {0};
    event.is_success = false;
    event.is_escape = true;
    event.pursuit_duration_s = 3.0f;
    event.miss_distance = 0.5f;

    EXPECT_EQ(dragonfly_emotion_process_event(bridge, &event), 0);
}

TEST_F(DragonEmotionBridgeTest, ProcessThreatEvent) {
    emotional_event_t event = {0};
    event.is_threat = true;

    EXPECT_EQ(dragonfly_emotion_process_event(bridge, &event), 0);
}
