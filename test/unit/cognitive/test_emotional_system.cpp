//=============================================================================
// test_emotional_system.cpp - Unit Tests for Integrated Emotional System
//=============================================================================
// WHAT: Comprehensive unit tests for emotional system (TDD approach)
// WHY:  Ensure 100% code coverage before implementation
// HOW:  Test all API functions, edge cases, and error conditions
//
// Test Coverage:
// - Lifecycle: create, destroy, default_config
// - State management: get_state, set_state, decay
// - Emotion tagging: get_tag
// - Emotion detection: is_active, update_multimodal
// - Regulation: regulate, auto_regulate
// - Integration: salience_boost, memory_priority, mental_health_impact
// - Statistics: get_stats
// - Error handling: NULL inputs, invalid parameters
// - Edge cases: boundary values, zero values, extreme values
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalSystemTest : public NimcpTestBase {
protected:
    emotional_system_t* system = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent FIRST
        // Tests will create systems as needed
    }

    void TearDown() override {
        if (system) {
            emotion_system_destroy(system);
            system = nullptr;
        }
        NimcpTestBase::TearDown();  // Call parent LAST
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(EmotionalSystemTest, CreateWithDefaultConfig) {
    // WHAT: Create emotional system with default configuration
    // WHY:  Verify basic initialization works
    // HOW:  Create with NULL config, verify non-NULL return

    system = emotion_system_create(nullptr);

    ASSERT_NE(system, nullptr);
}

TEST_F(EmotionalSystemTest, CreateWithCustomConfig) {
    // WHAT: Create emotional system with custom configuration
    // WHY:  Verify configuration is respected
    // HOW:  Create config, set parameters, verify system created

    emotion_config_t config = emotion_system_default_config();
    config.enable_emotion_recognition = true;
    config.enable_emotional_tagging = true;
    config.enable_shadow_detection = true;
    config.enable_emotion_regulation = true;
    config.emotion_decay_rate = 0.1f;
    config.arousal_sensitivity = 1.5f;
    config.valence_sensitivity = 1.2f;

    system = emotion_system_create(&config);

    ASSERT_NE(system, nullptr);
}

TEST_F(EmotionalSystemTest, DefaultConfigHasSensibleValues) {
    // WHAT: Verify default configuration has reasonable values
    // WHY:  Ensure users get working system without manual config
    // HOW:  Call default_config, check all values

    emotion_config_t config = emotion_system_default_config();

    // Core features should be enabled by default
    EXPECT_TRUE(config.enable_emotion_recognition);
    EXPECT_TRUE(config.enable_emotional_tagging);
    EXPECT_TRUE(config.enable_shadow_detection);
    EXPECT_TRUE(config.enable_emotion_regulation);

    // Integration features should be enabled
    EXPECT_TRUE(config.integrate_with_memory);
    EXPECT_TRUE(config.integrate_with_salience);
    EXPECT_TRUE(config.integrate_with_mental_health);
    EXPECT_TRUE(config.integrate_with_ethics);

    // Regulation parameters should be in valid ranges
    EXPECT_GE(config.emotion_decay_rate, 0.0f);
    EXPECT_LE(config.emotion_decay_rate, 1.0f);
    EXPECT_GE(config.arousal_sensitivity, 0.5f);
    EXPECT_LE(config.arousal_sensitivity, 2.0f);
    EXPECT_GE(config.valence_sensitivity, 0.5f);
    EXPECT_LE(config.valence_sensitivity, 2.0f);
    EXPECT_GE(config.regulation_threshold, 0.0f);
    EXPECT_LE(config.regulation_threshold, 1.0f);

    // Shadow limits should be reasonable
    EXPECT_GT(config.max_shadow_tracked, 0u);
    EXPECT_GE(config.shadow_intervention_threshold, 0.0f);
    EXPECT_LE(config.shadow_intervention_threshold, 1.0f);
}

TEST_F(EmotionalSystemTest, DestroyNullSystemIsNoop) {
    // WHAT: Verify destroying NULL system doesn't crash
    // WHY:  Defensive programming - handle NULL gracefully
    // HOW:  Call destroy with NULL

    emotion_system_destroy(nullptr);

    // If we get here, test passed
    SUCCEED();
}

TEST_F(EmotionalSystemTest, DestroyValidSystem) {
    // WHAT: Verify destroying valid system frees resources
    // WHY:  Prevent memory leaks
    // HOW:  Create and destroy system

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_destroy(system);
    system = nullptr;  // Prevent double-free in TearDown

    SUCCEED();
}

//=============================================================================
// State Query Tests
//=============================================================================

TEST_F(EmotionalSystemTest, GetStateInitialValues) {
    // WHAT: Verify initial emotional state is neutral
    // WHY:  System should start in neutral emotional state
    // HOW:  Create system, get state, check values

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_state_t state;
    bool result = emotion_system_get_state(system, &state);

    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(state.valence, 0.0f);  // Neutral valence
    EXPECT_FLOAT_EQ(state.arousal, 0.0f);  // Calm arousal
    EXPECT_FLOAT_EQ(state.intensity, 0.0f);  // No intensity
    EXPECT_EQ(state.dominant_emotion, 0u);
    EXPECT_FLOAT_EQ(state.emotion_confidence, 0.0f);
    EXPECT_FLOAT_EQ(state.shadow_intensity, 0.0f);
    EXPECT_EQ(state.active_shadow_count, 0u);
    EXPECT_FALSE(state.in_self_regulation);
}

TEST_F(EmotionalSystemTest, GetStateWithNullSystem) {
    // WHAT: Verify get_state handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL system

    emotion_state_t state;
    bool result = emotion_system_get_state(nullptr, &state);

    EXPECT_FALSE(result);
}

TEST_F(EmotionalSystemTest, GetStateWithNullOutput) {
    // WHAT: Verify get_state handles NULL output
    // WHY:  Defensive programming
    // HOW:  Call with NULL state pointer

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_system_get_state(system, nullptr);

    EXPECT_FALSE(result);
}

TEST_F(EmotionalSystemTest, GetTagInitialValues) {
    // WHAT: Verify initial emotional tag is neutral
    // WHY:  Tag should reflect neutral initial state
    // HOW:  Create system, get tag, check values

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotional_tag_t tag;
    bool result = emotion_system_get_tag(system, &tag);

    ASSERT_TRUE(result);
    EXPECT_FLOAT_EQ(tag.valence, 0.0f);
    EXPECT_FLOAT_EQ(tag.arousal, 0.0f);
}

TEST_F(EmotionalSystemTest, GetTagWithNullInputs) {
    // WHAT: Verify get_tag handles NULL inputs
    // WHY:  Defensive programming
    // HOW:  Test NULL system and NULL tag

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotional_tag_t tag;

    // NULL system
    EXPECT_FALSE(emotion_system_get_tag(nullptr, &tag));

    // NULL tag
    EXPECT_FALSE(emotion_system_get_tag(system, nullptr));

    // Both NULL
    EXPECT_FALSE(emotion_system_get_tag(nullptr, nullptr));
}

TEST_F(EmotionalSystemTest, IsActiveInitiallyFalse) {
    // WHAT: Verify is_active returns false initially
    // WHY:  No emotions should be active at start
    // HOW:  Create system, check various emotion IDs

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Check various emotion IDs
    for (uint32_t emotion_id = 0; emotion_id < 10; emotion_id++) {
        bool active = emotion_system_is_active(system, emotion_id, 0.5f);
        EXPECT_FALSE(active);
    }
}

TEST_F(EmotionalSystemTest, IsActiveWithNullSystem) {
    // WHAT: Verify is_active handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL system

    bool result = emotion_system_is_active(nullptr, 0, 0.5f);

    EXPECT_FALSE(result);
}

//=============================================================================
// State Update Tests
//=============================================================================

TEST_F(EmotionalSystemTest, SetStateUpdatesValenceAndArousal) {
    // WHAT: Verify set_state updates emotional state
    // WHY:  Need to control emotional state for testing/simulation
    // HOW:  Set state, verify get_state returns new values

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_system_set_state(system, 0.7f, 0.8f, 1000);
    ASSERT_TRUE(result);

    emotion_state_t state;
    result = emotion_system_get_state(system, &state);
    ASSERT_TRUE(result);

    // Note: Values are scaled by sensitivity multipliers
    // Default arousal_sensitivity = 1.2, so 0.8 * 1.2 = 0.96 (clamped to 1.0)
    // Default valence_sensitivity = 1.0, so 0.7 * 1.0 = 0.7
    EXPECT_FLOAT_EQ(state.valence, 0.7f);
    EXPECT_FLOAT_EQ(state.arousal, 0.96f);  // 0.8 * 1.2 sensitivity
    EXPECT_GT(state.intensity, 0.0f);
    EXPECT_EQ(state.last_update_ms, 1000u);
}

TEST_F(EmotionalSystemTest, SetStateHandlesNegativeValence) {
    // WHAT: Verify set_state handles negative valence
    // WHY:  Valence can be negative (unpleasant emotions)
    // HOW:  Set negative valence, verify

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    bool result = emotion_system_set_state(system, -0.6f, 0.5f, 1000);
    ASSERT_TRUE(result);

    emotion_state_t state;
    emotion_system_get_state(system, &state);

    EXPECT_FLOAT_EQ(state.valence, -0.6f);
}

TEST_F(EmotionalSystemTest, SetStateHandlesBoundaryValues) {
    // WHAT: Verify set_state handles boundary values correctly
    // WHY:  Valence [-1, +1], arousal [0, 1]
    // HOW:  Test extremes

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Max positive valence, max arousal
    emotion_system_set_state(system, 1.0f, 1.0f, 1000);
    emotion_state_t state;
    emotion_system_get_state(system, &state);
    EXPECT_FLOAT_EQ(state.valence, 1.0f);
    EXPECT_FLOAT_EQ(state.arousal, 1.0f);

    // Max negative valence, min arousal
    emotion_system_set_state(system, -1.0f, 0.0f, 2000);
    emotion_system_get_state(system, &state);
    EXPECT_FLOAT_EQ(state.valence, -1.0f);
    EXPECT_FLOAT_EQ(state.arousal, 0.0f);
}

TEST_F(EmotionalSystemTest, SetStateWithNullSystem) {
    // WHAT: Verify set_state handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = emotion_system_set_state(nullptr, 0.5f, 0.5f, 1000);

    EXPECT_FALSE(result);
}

TEST_F(EmotionalSystemTest, DecayReducesIntensity) {
    // WHAT: Verify decay reduces emotional intensity over time
    // WHY:  Emotions naturally fade without stimulation
    // HOW:  Set high arousal, decay, verify reduction

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set high arousal
    emotion_system_set_state(system, 0.5f, 0.9f, 1000);

    emotion_state_t state_before;
    emotion_system_get_state(system, &state_before);
    float arousal_before = state_before.arousal;

    // Decay for 1 second
    bool result = emotion_system_decay(system, 1.0f, 2000);
    ASSERT_TRUE(result);

    emotion_state_t state_after;
    emotion_system_get_state(system, &state_after);
    float arousal_after = state_after.arousal;

    // Arousal should decrease
    EXPECT_LT(arousal_after, arousal_before);
}

TEST_F(EmotionalSystemTest, DecayMultipleTimes) {
    // WHAT: Verify repeated decay continues reducing intensity
    // WHY:  Emotions should continue fading
    // HOW:  Decay multiple times, verify monotonic decrease

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.3f, 1.0f, 1000);

    float previous_arousal = 1.0f;

    for (int i = 0; i < 5; i++) {
        emotion_system_decay(system, 0.5f, 1000 + (i + 1) * 500);

        emotion_state_t state;
        emotion_system_get_state(system, &state);

        EXPECT_LE(state.arousal, previous_arousal);
        previous_arousal = state.arousal;
    }
}

TEST_F(EmotionalSystemTest, DecayDoesNotGoNegative) {
    // WHAT: Verify decay doesn't make arousal negative
    // WHY:  Arousal has floor of 0.0
    // HOW:  Decay many times, verify >= 0

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.2f, 0.5f, 1000);

    // Decay many times
    for (int i = 0; i < 100; i++) {
        emotion_system_decay(system, 1.0f, 1000 + (i + 1) * 1000);
    }

    emotion_state_t state;
    emotion_system_get_state(system, &state);

    EXPECT_GE(state.arousal, 0.0f);
    EXPECT_GE(state.intensity, 0.0f);
}

TEST_F(EmotionalSystemTest, DecayWithNullSystem) {
    // WHAT: Verify decay handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = emotion_system_decay(nullptr, 1.0f, 1000);

    EXPECT_FALSE(result);
}

TEST_F(EmotionalSystemTest, UpdateMultimodalWithTextOnly) {
    // WHAT: Verify multimodal update works with text only
    // WHY:  Should handle partial modalities
    // HOW:  Pass text, NULL visual/audio

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    const char* text = "I am very happy today!";
    bool result = emotion_system_update_multimodal(
        system, nullptr, 0, nullptr, 0, text, 1000
    );

    EXPECT_TRUE(result);
}

TEST_F(EmotionalSystemTest, UpdateMultimodalWithVisualAndAudio) {
    // WHAT: Verify multimodal update works with visual and audio
    // WHY:  Should process multiple modalities
    // HOW:  Pass visual and audio features

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    float visual_features[128];
    for (int i = 0; i < 128; i++) {
        visual_features[i] = 0.1f * sinf(i * 0.1f);
    }

    float audio_features[64];
    for (int i = 0; i < 64; i++) {
        audio_features[i] = 0.2f * cosf(i * 0.05f);
    }

    bool result = emotion_system_update_multimodal(
        system, visual_features, 128, audio_features, 64, nullptr, 1000
    );

    EXPECT_TRUE(result);
}

TEST_F(EmotionalSystemTest, UpdateMultimodalAllModalities) {
    // WHAT: Verify multimodal update with all modalities
    // WHY:  Full multimodal processing test
    // HOW:  Pass visual, audio, and text

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    float visual[64] = {0.5f};
    float audio[32] = {0.3f};
    const char* text = "test";

    bool result = emotion_system_update_multimodal(
        system, visual, 64, audio, 32, text, 1000
    );

    EXPECT_TRUE(result);
}

TEST_F(EmotionalSystemTest, UpdateMultimodalWithNullSystem) {
    // WHAT: Verify multimodal update handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL system

    const char* text = "test";
    bool result = emotion_system_update_multimodal(
        nullptr, nullptr, 0, nullptr, 0, text, 1000
    );

    EXPECT_FALSE(result);
}

//=============================================================================
// Regulation Tests
//=============================================================================

TEST_F(EmotionalSystemTest, RegulateWithStrategy) {
    // WHAT: Verify regulate applies regulation strategy
    // WHY:  Enable explicit emotion regulation
    // HOW:  Set high arousal, regulate, verify change

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set high arousal (needs regulation)
    emotion_system_set_state(system, -0.8f, 0.95f, 1000);

    emotion_state_t state_before;
    emotion_system_get_state(system, &state_before);

    // Apply regulation strategy (0 = reappraisal, 1 = suppression, etc.)
    bool result = emotion_system_regulate(system, 0);

    EXPECT_TRUE(result);

    emotion_state_t state_after;
    emotion_system_get_state(system, &state_after);

    // Should enter regulation state
    EXPECT_TRUE(state_after.in_self_regulation);
}

TEST_F(EmotionalSystemTest, RegulateWithNullSystem) {
    // WHAT: Verify regulate handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = emotion_system_regulate(nullptr, 0);

    EXPECT_FALSE(result);
}

TEST_F(EmotionalSystemTest, AutoRegulateTriggersOnHighIntensity) {
    // WHAT: Verify auto_regulate triggers when intensity exceeds threshold
    // WHY:  Automatic emotional homeostasis
    // HOW:  Set very high arousal, call auto_regulate, verify triggered

    emotion_config_t config = emotion_system_default_config();
    config.regulation_threshold = 0.7f;

    system = emotion_system_create(&config);
    ASSERT_NE(system, nullptr);

    // Set intensity above threshold
    emotion_system_set_state(system, 0.9f, 0.95f, 1000);

    bool regulated = emotion_system_auto_regulate(system);

    // Should trigger regulation
    EXPECT_TRUE(regulated);

    emotion_state_t state;
    emotion_system_get_state(system, &state);
    EXPECT_TRUE(state.in_self_regulation);
}

TEST_F(EmotionalSystemTest, AutoRegulateDoesNotTriggerOnLowIntensity) {
    // WHAT: Verify auto_regulate doesn't trigger on low intensity
    // WHY:  Only regulate when needed
    // HOW:  Set low arousal, verify no regulation

    emotion_config_t config = emotion_system_default_config();
    config.regulation_threshold = 0.7f;

    system = emotion_system_create(&config);
    ASSERT_NE(system, nullptr);

    // Set low intensity
    emotion_system_set_state(system, 0.2f, 0.3f, 1000);

    bool regulated = emotion_system_auto_regulate(system);

    // Should NOT trigger
    EXPECT_FALSE(regulated);
}

TEST_F(EmotionalSystemTest, AutoRegulateWithNullSystem) {
    // WHAT: Verify auto_regulate handles NULL system
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = emotion_system_auto_regulate(nullptr);

    EXPECT_FALSE(result);
}

//=============================================================================
// Integration API Tests
//=============================================================================

TEST_F(EmotionalSystemTest, SalienceBoostIncreasesWithArousal) {
    // WHAT: Verify salience boost increases with arousal
    // WHY:  High arousal events grab attention
    // HOW:  Set low then high arousal, compare boost

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Low arousal
    emotion_system_set_state(system, 0.0f, 0.1f, 1000);
    float boost_low = emotion_system_get_salience_boost(system);

    // High arousal
    emotion_system_set_state(system, 0.0f, 0.9f, 2000);
    float boost_high = emotion_system_get_salience_boost(system);

    EXPECT_GT(boost_high, boost_low);
}

TEST_F(EmotionalSystemTest, SalienceBoostMinimumIsOne) {
    // WHAT: Verify salience boost minimum is 1.0 (no penalty)
    // WHY:  Emotion should boost or maintain salience, never reduce
    // HOW:  Set zero arousal, verify boost >= 1.0

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.0f, 0.0f, 1000);
    float boost = emotion_system_get_salience_boost(system);

    EXPECT_GE(boost, 1.0f);
}

TEST_F(EmotionalSystemTest, MemoryPriorityIncreasesWithIntensity) {
    // WHAT: Verify memory priority increases with emotional intensity
    // WHY:  Emotional events remembered better
    // HOW:  Compare low vs high intensity

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Low intensity
    emotion_system_set_state(system, 0.1f, 0.2f, 1000);
    float priority_low = emotion_system_get_memory_priority(system);

    // High intensity
    emotion_system_set_state(system, 0.9f, 0.95f, 2000);
    float priority_high = emotion_system_get_memory_priority(system);

    EXPECT_GT(priority_high, priority_low);
}

TEST_F(EmotionalSystemTest, MemoryPriorityBoundedZeroToOne) {
    // WHAT: Verify memory priority stays in [0, 1]
    // WHY:  Valid probability range
    // HOW:  Test various states

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Test various states
    float test_cases[][2] = {
        {-1.0f, 0.0f},
        {0.0f, 0.0f},
        {0.5f, 0.5f},
        {1.0f, 1.0f},
        {-0.8f, 0.9f}
    };

    for (auto& test_case : test_cases) {
        emotion_system_set_state(system, test_case[0], test_case[1], 1000);
        float priority = emotion_system_get_memory_priority(system);

        EXPECT_GE(priority, 0.0f);
        EXPECT_LE(priority, 1.0f);
    }
}

TEST_F(EmotionalSystemTest, MentalHealthImpactLowWhenHealthy) {
    // WHAT: Verify mental health impact is low when system is healthy
    // WHY:  Neutral emotions should indicate good mental health
    // HOW:  Set neutral state, verify low impact

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.0f, 0.0f, 1000);
    float impact = emotion_system_get_mental_health_impact(system);

    EXPECT_LE(impact, 0.2f);  // Low impact when healthy
}

TEST_F(EmotionalSystemTest, MentalHealthImpactIncreasesWithDysregulation) {
    // WHAT: Verify mental health impact increases with dysregulation
    // WHY:  Extreme emotions indicate potential mental health issues
    // HOW:  Set extreme negative valence + high arousal

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Extreme negative emotion (e.g., severe anxiety/depression)
    emotion_system_set_state(system, -0.95f, 0.9f, 1000);
    float impact = emotion_system_get_mental_health_impact(system);

    EXPECT_GE(impact, 0.5f);  // Significant impact
}

TEST_F(EmotionalSystemTest, MentalHealthImpactBoundedZeroToOne) {
    // WHAT: Verify mental health impact stays in [0, 1]
    // WHY:  Valid severity range
    // HOW:  Test extreme cases

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Extreme negative
    emotion_system_set_state(system, -1.0f, 1.0f, 1000);
    float impact1 = emotion_system_get_mental_health_impact(system);
    EXPECT_GE(impact1, 0.0f);
    EXPECT_LE(impact1, 1.0f);

    // Neutral
    emotion_system_set_state(system, 0.0f, 0.0f, 2000);
    float impact2 = emotion_system_get_mental_health_impact(system);
    EXPECT_GE(impact2, 0.0f);
    EXPECT_LE(impact2, 1.0f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(EmotionalSystemTest, GetStatsInitiallyZero) {
    // WHAT: Verify statistics are zero initially
    // WHY:  No processing has occurred yet
    // HOW:  Create system, get stats, verify zeros

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_stats_t stats;
    bool result = emotion_system_get_stats(system, &stats);

    ASSERT_TRUE(result);
    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.total_regulations, 0u);
    EXPECT_EQ(stats.successful_regulations, 0u);
    EXPECT_FLOAT_EQ(stats.avg_valence, 0.0f);
    EXPECT_FLOAT_EQ(stats.avg_arousal, 0.0f);
    EXPECT_EQ(stats.shadow_activations, 0u);
}

TEST_F(EmotionalSystemTest, GetStatsIncrementsUpdateCount) {
    // WHAT: Verify stats track state updates
    // WHY:  Monitor system usage
    // HOW:  Update state, check stats

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Make several updates
    for (int i = 0; i < 5; i++) {
        emotion_system_set_state(system, 0.1f * i, 0.2f * i, 1000 + i * 100);
    }

    emotion_stats_t stats;
    emotion_system_get_stats(system, &stats);

    EXPECT_EQ(stats.total_updates, 5u);
}

TEST_F(EmotionalSystemTest, GetStatsTracksRegulations) {
    // WHAT: Verify stats track regulation attempts
    // WHY:  Monitor regulation usage
    // HOW:  Regulate, check stats

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set high arousal and regulate multiple times
    emotion_system_set_state(system, 0.8f, 0.9f, 1000);

    for (int i = 0; i < 3; i++) {
        emotion_system_regulate(system, i % 2);  // Alternate strategies
    }

    emotion_stats_t stats;
    emotion_system_get_stats(system, &stats);

    EXPECT_GE(stats.total_regulations, 3u);
}

TEST_F(EmotionalSystemTest, GetStatsWithNullInputs) {
    // WHAT: Verify get_stats handles NULL inputs
    // WHY:  Defensive programming
    // HOW:  Test NULL combinations

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_stats_t stats;

    // NULL system
    EXPECT_FALSE(emotion_system_get_stats(nullptr, &stats));

    // NULL stats
    EXPECT_FALSE(emotion_system_get_stats(system, nullptr));

    // Both NULL
    EXPECT_FALSE(emotion_system_get_stats(nullptr, nullptr));
}

//=============================================================================
// Edge Cases and Stress Tests
//=============================================================================

TEST_F(EmotionalSystemTest, RapidStateChanges) {
    // WHAT: Verify system handles rapid state changes
    // WHY:  Stress test for stability
    // HOW:  Update state 1000 times rapidly

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < 1000; i++) {
        float valence = sinf(i * 0.1f);
        float arousal = cosf(i * 0.05f) * 0.5f + 0.5f;

        bool result = emotion_system_set_state(system, valence, arousal, 1000 + i);
        EXPECT_TRUE(result);
    }

    // Verify system still functional
    emotion_state_t state;
    bool result = emotion_system_get_state(system, &state);
    EXPECT_TRUE(result);
    EXPECT_FALSE(std::isnan(state.valence));
    EXPECT_FALSE(std::isnan(state.arousal));
}

TEST_F(EmotionalSystemTest, AlternatingExtremeStates) {
    // WHAT: Verify system handles extreme state swings
    // WHY:  Edge case - rapid mood swings
    // HOW:  Alternate between extreme positive/negative

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    for (int i = 0; i < 10; i++) {
        // Extreme positive
        emotion_system_set_state(system, 1.0f, 1.0f, 1000 + i * 200);

        emotion_state_t state;
        emotion_system_get_state(system, &state);
        EXPECT_FALSE(std::isnan(state.intensity));

        // Extreme negative
        emotion_system_set_state(system, -1.0f, 1.0f, 1100 + i * 200);

        emotion_system_get_state(system, &state);
        EXPECT_FALSE(std::isnan(state.intensity));
    }
}

TEST_F(EmotionalSystemTest, MultipleCreateDestroyCycles) {
    // WHAT: Verify no memory leaks across create/destroy cycles
    // WHY:  Resource management validation
    // HOW:  Create and destroy 100 times

    for (int i = 0; i < 100; i++) {
        emotional_system_t* temp_system = emotion_system_create(nullptr);
        ASSERT_NE(temp_system, nullptr);

        // Do some operations
        emotion_system_set_state(temp_system, 0.5f, 0.5f, 1000);
        emotion_system_decay(temp_system, 0.1f, 1100);

        emotion_system_destroy(temp_system);
    }

    SUCCEED();
}
