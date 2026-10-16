/**
 * @file test_sensory_logic_bridges.cpp
 * @brief Unit Tests for Sensory-Logic Bridges
 *
 * TEST COVERAGE: 45+ tests covering:
 * - Visual-Logic Bridge: grounding, relations, attention, verification
 * - Somatosensory-Logic Bridge: touch, position, body state predicates
 * - Audio-Logic Bridge: speech, sounds, speaker tracking
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/logic/nimcp_visual_logic_bridge.h"
#include "cognitive/logic/nimcp_somatosensory_logic_bridge.h"
#include "cognitive/logic/nimcp_audio_logic_bridge.h"

//=============================================================================
// Visual-Logic Bridge Tests
//=============================================================================

class VisualLogicBridgeTest : public ::testing::Test {
protected:
    visual_logic_bridge_t* bridge;

    void SetUp() override {
        bridge = visual_logic_bridge_create(nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            visual_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST(VisualLogicDefaultConfig, ReasonableDefaults) {
    visual_logic_config_t config = visual_logic_default_config();

    EXPECT_TRUE(config.enable_object_grounding);
    EXPECT_TRUE(config.enable_relation_extraction);
    EXPECT_TRUE(config.enable_top_down_attention);
    EXPECT_TRUE(config.enable_verification);
    EXPECT_FLOAT_EQ(config.min_confidence_threshold, 0.5f);
    EXPECT_FLOAT_EQ(config.min_salience_threshold, 0.3f);
    EXPECT_EQ(config.max_objects_per_frame, 32u);
}

TEST_F(VisualLogicBridgeTest, CreateDestroy) {
    // Already tested in SetUp, just verify bridge exists
    EXPECT_NE(bridge, nullptr);
}

TEST_F(VisualLogicBridgeTest, GroundObservation) {
    visual_logic_observation_t obs = {};
    obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs.concept_name, "cat", sizeof(obs.concept_name));
    obs.confidence = 0.9f;
    obs.salience = 0.8f;
    obs.object_id = 1;
    obs.location_x = 100;
    obs.location_y = 200;

    int result = visual_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    // Check grounded count increased
    int count = visual_logic_get_grounded_count(bridge);
    EXPECT_EQ(count, 1);
}

TEST_F(VisualLogicBridgeTest, GroundObservationLowConfidence) {
    visual_logic_observation_t obs = {};
    obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs.concept_name, "cat", sizeof(obs.concept_name));
    obs.confidence = 0.2f;  // Below threshold
    obs.salience = 0.8f;
    obs.object_id = 1;

    int result = visual_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);  // Returns 0 but doesn't ground

    int count = visual_logic_get_grounded_count(bridge);
    EXPECT_EQ(count, 0);  // Not grounded due to low confidence
}

TEST_F(VisualLogicBridgeTest, GroundMultipleObjects) {
    for (uint32_t i = 0; i < 5; i++) {
        visual_logic_observation_t obs = {};
        obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
        snprintf(obs.concept_name, sizeof(obs.concept_name), "obj_%u", i);
        obs.confidence = 0.9f;
        obs.salience = 0.8f;
        obs.object_id = i + 1;

        visual_logic_ground_observation(bridge, &obs);
    }

    int count = visual_logic_get_grounded_count(bridge);
    EXPECT_EQ(count, 5);
}

TEST_F(VisualLogicBridgeTest, ReportRelation) {
    // First ground two objects
    visual_logic_observation_t obs1 = {};
    obs1.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs1.concept_name, "cat", sizeof(obs1.concept_name));
    obs1.confidence = 0.9f;
    obs1.salience = 0.8f;
    obs1.object_id = 1;
    visual_logic_ground_observation(bridge, &obs1);

    visual_logic_observation_t obs2 = {};
    obs2.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs2.concept_name, "mat", sizeof(obs2.concept_name));
    obs2.confidence = 0.9f;
    obs2.salience = 0.8f;
    obs2.object_id = 2;
    visual_logic_ground_observation(bridge, &obs2);

    // Report relation
    visual_logic_relation_t rel = {};
    rel.subject_id = 1;
    rel.object_id = 2;
    strncpy(rel.relation_name, "on", sizeof(rel.relation_name));
    rel.confidence = 0.85f;

    int result = visual_logic_report_relation(bridge, &rel);
    EXPECT_EQ(result, 0);

    // Check stats
    visual_logic_stats_t stats;
    visual_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.relations_extracted, 1u);
}

TEST_F(VisualLogicBridgeTest, RequestAttention) {
    int result = visual_logic_request_attention(bridge, "target_object", 0.9f);
    EXPECT_EQ(result, 0);

    visual_logic_stats_t stats;
    visual_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_commands, 1u);
}

TEST_F(VisualLogicBridgeTest, VerifyPredicate) {
    // Ground an object
    visual_logic_observation_t obs = {};
    obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs.concept_name, "cat", sizeof(obs.concept_name));
    obs.confidence = 0.9f;
    obs.salience = 0.8f;
    obs.object_id = 1;
    visual_logic_ground_observation(bridge, &obs);

    // Verify the predicate
    bool verified = false;
    float confidence = 0.0f;
    int result = visual_logic_verify_predicate(bridge, "cat", true, &verified, &confidence);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(verified);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(VisualLogicBridgeTest, IsGrounded) {
    visual_logic_observation_t obs = {};
    obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs.concept_name, "cat", sizeof(obs.concept_name));
    obs.confidence = 0.9f;
    obs.salience = 0.8f;
    obs.object_id = 42;
    visual_logic_ground_observation(bridge, &obs);

    bool grounded = false;
    visual_logic_is_grounded(bridge, 42, &grounded);
    EXPECT_TRUE(grounded);

    visual_logic_is_grounded(bridge, 999, &grounded);
    EXPECT_FALSE(grounded);
}

TEST_F(VisualLogicBridgeTest, Reset) {
    // Ground some objects
    visual_logic_observation_t obs = {};
    obs.signal_type = VISUAL_LOGIC_OBJECT_DETECTED;
    strncpy(obs.concept_name, "cat", sizeof(obs.concept_name));
    obs.confidence = 0.9f;
    obs.salience = 0.8f;
    obs.object_id = 1;
    visual_logic_ground_observation(bridge, &obs);

    EXPECT_EQ(visual_logic_get_grounded_count(bridge), 1);

    // Reset
    visual_logic_bridge_reset(bridge);

    EXPECT_EQ(visual_logic_get_grounded_count(bridge), 0);
}

TEST_F(VisualLogicBridgeTest, NullArgs) {
    EXPECT_EQ(visual_logic_ground_observation(nullptr, nullptr), -1);
    EXPECT_EQ(visual_logic_report_relation(nullptr, nullptr), -1);
    EXPECT_EQ(visual_logic_request_attention(nullptr, "test", 0.5f), -1);
    EXPECT_EQ(visual_logic_get_grounded_count(nullptr), -1);
}

//=============================================================================
// Somatosensory-Logic Bridge Tests
//=============================================================================

class SomatoLogicBridgeTest : public ::testing::Test {
protected:
    somato_logic_bridge_t* bridge;

    void SetUp() override {
        bridge = somato_logic_bridge_create(nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            somato_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST(SomatoLogicDefaultConfig, ReasonableDefaults) {
    somato_logic_config_t config = somato_logic_default_config();

    EXPECT_TRUE(config.enable_touch_grounding);
    EXPECT_TRUE(config.enable_position_grounding);
    EXPECT_TRUE(config.enable_pain_priority);
    EXPECT_TRUE(config.enable_top_down_attention);
    EXPECT_FLOAT_EQ(config.min_intensity_threshold, 0.1f);
    EXPECT_FLOAT_EQ(config.pain_priority_boost, 2.0f);
}

TEST(SomatoLogicRegionNames, AllRegionsNamed) {
    EXPECT_STREQ(somato_logic_region_name(BODY_REGION_HEAD), "head");
    EXPECT_STREQ(somato_logic_region_name(BODY_REGION_LEFT_HAND), "left_hand");
    EXPECT_STREQ(somato_logic_region_name(BODY_REGION_RIGHT_FOOT), "right_foot");
    EXPECT_STREQ(somato_logic_region_name((body_region_t)999), "unknown");
}

TEST_F(SomatoLogicBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(SomatoLogicBridgeTest, GroundTouchObservation) {
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_TOUCH_DETECTED;
    obs.body_region = BODY_REGION_LEFT_HAND;
    obs.intensity = 0.8f;
    obs.confidence = 0.9f;
    strncpy(obs.contacted_object_name, "cup", sizeof(obs.contacted_object_name));

    int result = somato_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    somato_logic_stats_t stats;
    somato_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.touch_events_grounded, 1u);
}

TEST_F(SomatoLogicBridgeTest, GroundPositionUpdate) {
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_POSITION_UPDATE;
    obs.body_region = BODY_REGION_RIGHT_ARM;
    obs.intensity = 1.0f;
    obs.confidence = 0.85f;
    obs.position[0] = 1.0f;
    obs.position[1] = 2.0f;
    obs.position[2] = 3.0f;

    int result = somato_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    somato_logic_stats_t stats;
    somato_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.position_updates, 1u);
}

TEST_F(SomatoLogicBridgeTest, PainSignal) {
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_PAIN_SIGNAL;
    obs.body_region = BODY_REGION_LEFT_LEG;
    obs.intensity = 0.7f;
    obs.confidence = 0.95f;
    obs.pain_level = 0.6f;

    int result = somato_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    somato_logic_stats_t stats;
    somato_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.pain_signals, 1u);
}

TEST_F(SomatoLogicBridgeTest, HasContact) {
    // First ground a touch
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_TOUCH_DETECTED;
    obs.body_region = BODY_REGION_RIGHT_HAND;
    obs.intensity = 0.8f;
    obs.confidence = 0.9f;
    strncpy(obs.contacted_object_name, "ball", sizeof(obs.contacted_object_name));
    somato_logic_ground_observation(bridge, &obs);

    bool has_contact = false;
    somato_logic_has_contact(bridge, BODY_REGION_RIGHT_HAND, &has_contact);
    EXPECT_TRUE(has_contact);

    somato_logic_has_contact(bridge, BODY_REGION_LEFT_HAND, &has_contact);
    EXPECT_FALSE(has_contact);
}

TEST_F(SomatoLogicBridgeTest, GetPosition) {
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_POSITION_UPDATE;
    obs.body_region = BODY_REGION_HEAD;
    obs.intensity = 1.0f;
    obs.confidence = 0.95f;
    obs.position[0] = 0.0f;
    obs.position[1] = 1.8f;  // Head height
    obs.position[2] = 0.0f;
    somato_logic_ground_observation(bridge, &obs);

    float position[3] = {0};
    float confidence = 0.0f;
    somato_logic_get_position(bridge, BODY_REGION_HEAD, position, &confidence);

    EXPECT_FLOAT_EQ(position[1], 1.8f);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(SomatoLogicBridgeTest, RequestAttention) {
    int result = somato_logic_request_attention(bridge, BODY_REGION_LEFT_FOOT, 0.8f);
    EXPECT_EQ(result, 0);

    somato_logic_stats_t stats;
    somato_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_commands, 1u);
}

TEST_F(SomatoLogicBridgeTest, ExpectContact) {
    int result = somato_logic_expect_contact(bridge, BODY_REGION_RIGHT_HAND, "door_handle");
    EXPECT_EQ(result, 0);
}

TEST_F(SomatoLogicBridgeTest, VerifyPosition) {
    // Set position first
    somato_logic_observation_t obs = {};
    obs.signal_type = SOMATO_LOGIC_POSITION_UPDATE;
    obs.body_region = BODY_REGION_LEFT_ARM;
    obs.intensity = 1.0f;
    obs.confidence = 0.9f;
    obs.position[0] = 1.0f;
    obs.position[1] = 1.0f;
    obs.position[2] = 0.0f;
    somato_logic_ground_observation(bridge, &obs);

    // Verify close position
    float expected[3] = {1.0f, 1.0f, 0.05f};
    bool verified = false;
    float confidence = 0.0f;
    int result = somato_logic_verify_position(bridge, BODY_REGION_LEFT_ARM, expected, &verified, &confidence);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(verified);  // Within tolerance
}

TEST_F(SomatoLogicBridgeTest, NullArgs) {
    EXPECT_EQ(somato_logic_ground_observation(nullptr, nullptr), -1);
    EXPECT_EQ(somato_logic_request_attention(nullptr, BODY_REGION_HEAD, 0.5f), -1);

    bool has_contact;
    EXPECT_EQ(somato_logic_has_contact(nullptr, BODY_REGION_HEAD, &has_contact), -1);
}

//=============================================================================
// Audio-Logic Bridge Tests
//=============================================================================

class AudioLogicBridgeTest : public ::testing::Test {
protected:
    audio_logic_bridge_t* bridge;

    void SetUp() override {
        bridge = audio_logic_bridge_create(nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            audio_logic_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

TEST(AudioLogicDefaultConfig, ReasonableDefaults) {
    audio_logic_config_t config = audio_logic_default_config();

    EXPECT_TRUE(config.enable_speech_grounding);
    EXPECT_TRUE(config.enable_sound_grounding);
    EXPECT_TRUE(config.enable_speaker_tracking);
    EXPECT_TRUE(config.enable_top_down_attention);
    EXPECT_FLOAT_EQ(config.min_confidence_threshold, 0.4f);
    EXPECT_FLOAT_EQ(config.speech_priority_boost, 1.5f);
}

TEST(AudioLogicCategoryNames, AllCategoriesNamed) {
    EXPECT_STREQ(audio_logic_category_name(SOUND_CAT_SPEECH), "speech");
    EXPECT_STREQ(audio_logic_category_name(SOUND_CAT_MUSIC), "music");
    EXPECT_STREQ(audio_logic_category_name(SOUND_CAT_ALERT), "alert");
    EXPECT_STREQ(audio_logic_category_name((sound_category_t)999), "invalid");
}

TEST_F(AudioLogicBridgeTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(AudioLogicBridgeTest, GroundSpeechObservation) {
    audio_logic_observation_t obs = {};
    obs.signal_type = AUDIO_LOGIC_WORD_RECOGNIZED;
    obs.category = SOUND_CAT_SPEECH;
    strncpy(obs.concept_name, "hello", sizeof(obs.concept_name));
    strncpy(obs.speaker_id, "speaker_1", sizeof(obs.speaker_id));
    obs.confidence = 0.9f;
    obs.salience = 0.8f;

    int result = audio_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.words_recognized, 1u);
}

TEST_F(AudioLogicBridgeTest, GroundSoundObservation) {
    audio_logic_observation_t obs = {};
    obs.signal_type = AUDIO_LOGIC_SOUND_DETECTED;
    obs.category = SOUND_CAT_ALERT;
    strncpy(obs.concept_name, "doorbell", sizeof(obs.concept_name));
    obs.confidence = 0.85f;
    obs.salience = 0.9f;

    int result = audio_logic_ground_observation(bridge, &obs);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sounds_grounded, 1u);
}

TEST_F(AudioLogicBridgeTest, ReportSpeech) {
    int result = audio_logic_report_speech(bridge, "john", "good morning", 0.92f);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.words_recognized, 1u);
}

TEST_F(AudioLogicBridgeTest, ReportSound) {
    int result = audio_logic_report_sound(bridge, "thunder", SOUND_CAT_ENVIRONMENTAL, 0.88f);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sounds_grounded, 1u);
}

TEST_F(AudioLogicBridgeTest, SpeakerTracking) {
    // Add speech from two speakers
    audio_logic_report_speech(bridge, "alice", "hello", 0.9f);
    audio_logic_report_speech(bridge, "bob", "hi there", 0.85f);

    int count = audio_logic_get_active_speaker_count(bridge);
    EXPECT_EQ(count, 2);

    bool active = false;
    audio_logic_is_speaker_active(bridge, "alice", &active);
    EXPECT_TRUE(active);

    audio_logic_is_speaker_active(bridge, "charlie", &active);
    EXPECT_FALSE(active);
}

TEST_F(AudioLogicBridgeTest, RequestAttention) {
    int result = audio_logic_request_attention(bridge, SOUND_CAT_SPEECH, 0.9f);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_commands, 1u);
}

TEST_F(AudioLogicBridgeTest, FocusSpeaker) {
    int result = audio_logic_focus_speaker(bridge, "target_speaker", 0.95f);
    EXPECT_EQ(result, 0);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.attention_commands, 1u);
}

TEST_F(AudioLogicBridgeTest, ExpectWord) {
    int result = audio_logic_expect_word(bridge, "password");
    EXPECT_EQ(result, 0);
}

TEST_F(AudioLogicBridgeTest, CategoryHeardRecently) {
    // Report a sound
    audio_logic_report_sound(bridge, "music_playing", SOUND_CAT_MUSIC, 0.9f);

    bool heard = false;
    audio_logic_category_heard_recently(bridge, SOUND_CAT_MUSIC, 1000, &heard);
    EXPECT_TRUE(heard);

    audio_logic_category_heard_recently(bridge, SOUND_CAT_ANIMAL, 1000, &heard);
    EXPECT_FALSE(heard);
}

TEST_F(AudioLogicBridgeTest, ProcessBatch) {
    audio_logic_observation_t batch[3] = {};

    batch[0].signal_type = AUDIO_LOGIC_SOUND_DETECTED;
    batch[0].category = SOUND_CAT_ENVIRONMENTAL;
    strncpy(batch[0].concept_name, "rain", sizeof(batch[0].concept_name));
    batch[0].confidence = 0.8f;
    batch[0].salience = 0.6f;

    batch[1].signal_type = AUDIO_LOGIC_WORD_RECOGNIZED;
    batch[1].category = SOUND_CAT_SPEECH;
    strncpy(batch[1].concept_name, "test", sizeof(batch[1].concept_name));
    batch[1].confidence = 0.9f;
    batch[1].salience = 0.8f;

    batch[2].signal_type = AUDIO_LOGIC_SOUND_DETECTED;
    batch[2].category = SOUND_CAT_ALERT;
    strncpy(batch[2].concept_name, "alarm", sizeof(batch[2].concept_name));
    batch[2].confidence = 0.95f;
    batch[2].salience = 1.0f;

    int processed = audio_logic_process_batch(bridge, batch, 3);
    EXPECT_EQ(processed, 3);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sounds_grounded, 2u);
    EXPECT_EQ(stats.words_recognized, 1u);
}

TEST_F(AudioLogicBridgeTest, Reset) {
    audio_logic_report_speech(bridge, "test_speaker", "hello", 0.9f);
    EXPECT_EQ(audio_logic_get_active_speaker_count(bridge), 1);

    audio_logic_bridge_reset(bridge);
    EXPECT_EQ(audio_logic_get_active_speaker_count(bridge), 0);
}

TEST_F(AudioLogicBridgeTest, NullArgs) {
    EXPECT_EQ(audio_logic_ground_observation(nullptr, nullptr), -1);
    EXPECT_EQ(audio_logic_report_speech(nullptr, "test", "hello", 0.9f), -1);
    EXPECT_EQ(audio_logic_request_attention(nullptr, SOUND_CAT_SPEECH, 0.5f), -1);
    EXPECT_EQ(audio_logic_get_active_speaker_count(nullptr), -1);
}

TEST_F(AudioLogicBridgeTest, StatsReset) {
    audio_logic_report_sound(bridge, "test", SOUND_CAT_ENVIRONMENTAL, 0.9f);

    audio_logic_stats_t stats;
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sounds_grounded, 1u);

    audio_logic_bridge_reset_stats(bridge);
    audio_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sounds_grounded, 0u);
}
