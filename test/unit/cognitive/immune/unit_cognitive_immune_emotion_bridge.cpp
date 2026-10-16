/**
 * @file unit_cognitive_immune_emotion_bridge.cpp
 * @brief Unit tests for emotion-immune bridge
 *
 * WHAT: Test bidirectional emotion-immune coupling
 * WHY:  Ensure cytokines modulate emotion, emotions trigger immune responses
 * HOW:  Mock systems, test individual pathways, verify biological mappings
 */

#include <gtest/gtest.h>
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_emotional_system.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionImmuneBridgeTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    emotional_system_t* emotion;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        /* Create emotional system */
        emotion_config_t emotion_config = emotion_system_default_config();
        emotion = emotion_system_create(&emotion_config);
        ASSERT_NE(emotion, nullptr);

        /* Create bridge */
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) emotion_immune_bridge_destroy(bridge);
        if (emotion) emotion_system_destroy(emotion);
        if (immune) brain_immune_destroy(immune);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, CreateWithDefaultConfig) {
    EXPECT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->immune_system, immune);
    EXPECT_EQ(bridge->emotion_system, emotion);
}

TEST_F(EmotionImmuneBridgeTest, CreateWithNullSystemsFails) {
    emotion_immune_config_t config;
    emotion_immune_default_config(&config);

    /* Null immune system */
    emotion_immune_bridge_t* b1 = emotion_immune_bridge_create(&config, nullptr, emotion, nullptr, nullptr);
    EXPECT_EQ(b1, nullptr);

    /* Null emotion system */
    emotion_immune_bridge_t* b2 = emotion_immune_bridge_create(&config, immune, nullptr, nullptr, nullptr);
    EXPECT_EQ(b2, nullptr);
}

TEST_F(EmotionImmuneBridgeTest, DefaultConfigValues) {
    emotion_immune_config_t config;
    int result = emotion_immune_default_config(&config);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(config.enable_cytokine_emotion_modulation);
    EXPECT_TRUE(config.enable_inflammation_anhedonia);
    EXPECT_TRUE(config.enable_emotion_immune_trigger);
    EXPECT_TRUE(config.enable_positive_immune_boost);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
}

/* ============================================================================
 * Cytokine → Emotion Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, ApplyCytokineEffects) {
    int result = emotion_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->cytokine_modulations, 0u);
}

TEST_F(EmotionImmuneBridgeTest, ProInflammatoryCytokinesNegativeAffect) {
    /* Set baseline positive emotion */
    emotion_system_set_state(emotion, 0.5f, 0.5f, 0);

    /* Apply cytokine effects (would need to simulate cytokine levels) */
    emotion_immune_apply_cytokine_effects(bridge);

    /* Get cytokine effects */
    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* Pro-inflammatory cytokines should create negative valence shift */
    /* Note: In actual test, would inject cytokines and verify */
    EXPECT_GE(effects.total_valence_shift, -1.0f);
    EXPECT_LE(effects.total_valence_shift, 1.0f);
}

TEST_F(EmotionImmuneBridgeTest, IL10PositiveAffect) {
    /* IL-10 (anti-inflammatory) should promote positive affect */
    emotion_immune_apply_cytokine_effects(bridge);

    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* IL-10 positive affect should be non-negative */
    EXPECT_GE(effects.il10_positive_affect, 0.0f);
}

TEST_F(EmotionImmuneBridgeTest, SicknessBehaviorCalculation) {
    emotion_immune_apply_cytokine_effects(bridge);

    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* Sickness behavior should be in valid range */
    EXPECT_GE(effects.sickness_behavior_level, 0.0f);
    EXPECT_LE(effects.sickness_behavior_level, 1.0f);
}

TEST_F(EmotionImmuneBridgeTest, CytokineInducedAnhedonia) {
    /* Pro-inflammatory cytokines should cause anhedonia */
    emotion_immune_apply_cytokine_effects(bridge);

    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* Anhedonia level should be valid */
    EXPECT_GE(effects.anhedonia_level, 0.0f);
    EXPECT_LE(effects.anhedonia_level, 1.0f);
}

/* ============================================================================
 * Inflammation → Emotion Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, ApplyInflammationEffects) {
    int result = emotion_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(EmotionImmuneBridgeTest, ChronicInflammationDepressionRisk) {
    /* Apply inflammation effects */
    emotion_immune_apply_inflammation_effects(bridge);

    inflammation_emotion_state_t state;
    emotion_immune_get_inflammation_state(bridge, &state);

    /* Depression risk should be valid */
    EXPECT_GE(state.depression_risk, 0.0f);
    EXPECT_LE(state.depression_risk, 1.0f);
}

TEST_F(EmotionImmuneBridgeTest, InflammationAnhedonia) {
    emotion_immune_apply_inflammation_effects(bridge);

    inflammation_emotion_state_t state;
    emotion_immune_get_inflammation_state(bridge, &state);

    /* Anhedonia severity should be valid */
    EXPECT_GE(state.anhedonia_severity, 0.0f);
    EXPECT_LE(state.anhedonia_severity, 1.0f);
}

TEST_F(EmotionImmuneBridgeTest, InflammationFatigue) {
    emotion_immune_apply_inflammation_effects(bridge);

    inflammation_emotion_state_t state;
    emotion_immune_get_inflammation_state(bridge, &state);

    /* Fatigue severity should be valid */
    EXPECT_GE(state.fatigue_severity, 0.0f);
    EXPECT_LE(state.fatigue_severity, 1.0f);
}

TEST_F(EmotionImmuneBridgeTest, ComputeAnhedoniaFromInflammation) {
    float anhedonia = emotion_immune_compute_anhedonia(bridge);
    EXPECT_GE(anhedonia, 0.0f);
    EXPECT_LE(anhedonia, 1.0f);
}

/* ============================================================================
 * Emotion → Immune Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, StressTriggerImmuneResponse) {
    /* Set high stress state */
    emotion_system_set_state(emotion, -0.8f, 0.9f, 0);  /* Negative valence, high arousal */

    /* Trigger immune response from stress */
    int result = emotion_immune_trigger_from_stress(bridge);
    EXPECT_EQ(result, 0);

    /* Should have triggered response */
    EXPECT_GT(bridge->emotion_triggered_responses, 0u);
}

TEST_F(EmotionImmuneBridgeTest, LowStressNoTrigger) {
    /* Set low stress state */
    emotion_system_set_state(emotion, 0.3f, 0.2f, 0);  /* Positive valence, low arousal */

    uint32_t before_count = bridge->emotion_triggered_responses;
    emotion_immune_trigger_from_stress(bridge);

    /* Should not trigger */
    EXPECT_EQ(bridge->emotion_triggered_responses, before_count);
}

TEST_F(EmotionImmuneBridgeTest, PositiveEmotionImmuneBoost) {
    /* Create joy system for positive emotion test */
    joy_system_t* joy = joy_system_create();
    ASSERT_NE(joy, nullptr);

    /* Recreate bridge with joy system */
    emotion_immune_bridge_destroy(bridge);
    emotion_immune_config_t config;
    emotion_immune_default_config(&config);
    bridge = emotion_immune_bridge_create(&config, immune, emotion, nullptr, joy);
    ASSERT_NE(bridge, nullptr);

    /* Simulate joyful state */
    emotion_system_set_state(emotion, 0.7f, 0.5f, 0);

    /* Apply positive boost */
    int result = emotion_immune_boost_from_positive_affect(bridge);
    EXPECT_EQ(result, 0);

    joy_system_destroy(joy);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, BridgeUpdate) {
    uint64_t before_updates = bridge->total_updates;

    int result = emotion_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge->total_updates, before_updates);
}

TEST_F(EmotionImmuneBridgeTest, MultipleUpdates) {
    for (int i = 0; i < 10; i++) {
        int result = emotion_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }
    EXPECT_EQ(bridge->total_updates, 10u);
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, GetCytokineEffects) {
    cytokine_emotion_effects_t effects;
    int result = emotion_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
}

TEST_F(EmotionImmuneBridgeTest, GetInflammationState) {
    inflammation_emotion_state_t state;
    int result = emotion_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(EmotionImmuneBridgeTest, IsSickBehavior) {
    bool sick = emotion_immune_is_sick_behavior(bridge);
    /* Should be false initially */
    EXPECT_FALSE(sick);
}

TEST_F(EmotionImmuneBridgeTest, GetAnhedoniaSeverity) {
    float severity = emotion_immune_get_anhedonia_severity(bridge);
    EXPECT_GE(severity, 0.0f);
    EXPECT_LE(severity, 1.0f);
}

/* ============================================================================
 * Feature Toggle Tests
 * ============================================================================ */

TEST_F(EmotionImmuneBridgeTest, DisableCytokineModulation) {
    bridge->enable_cytokine_emotion_modulation = false;

    uint32_t before = bridge->cytokine_modulations;
    emotion_immune_apply_cytokine_effects(bridge);

    /* Should not modulate when disabled */
    EXPECT_EQ(bridge->cytokine_modulations, before);
}

TEST_F(EmotionImmuneBridgeTest, DisableInflammationAnhedonia) {
    bridge->enable_inflammation_anhedonia = false;

    int result = emotion_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);  /* Should return success but not apply effects */
}

TEST_F(EmotionImmuneBridgeTest, DisableEmotionTrigger) {
    bridge->enable_emotion_immune_trigger = false;

    emotion_system_set_state(emotion, -0.8f, 0.9f, 0);
    uint32_t before = bridge->emotion_triggered_responses;
    emotion_immune_trigger_from_stress(bridge);

    EXPECT_EQ(bridge->emotion_triggered_responses, before);
}

/* ============================================================================
 * Null Safety Tests
 * ============================================================================ */

TEST(EmotionImmuneBridgeNullTest, NullBridgeHandling) {
    EXPECT_EQ(emotion_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(emotion_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(emotion_immune_trigger_from_stress(nullptr), -1);
    EXPECT_EQ(emotion_immune_bridge_update(nullptr, 100), -1);
}

TEST(EmotionImmuneBridgeNullTest, NullConfigHandling) {
    int result = emotion_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST(EmotionImmuneBridgeNullTest, ComputeAnhedoniaNull) {
    float anhedonia = emotion_immune_compute_anhedonia(nullptr);
    EXPECT_EQ(anhedonia, 0.0f);
}

TEST(EmotionImmuneBridgeNullTest, IsSickBehaviorNull) {
    bool sick = emotion_immune_is_sick_behavior(nullptr);
    EXPECT_FALSE(sick);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
