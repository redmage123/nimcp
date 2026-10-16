/**
 * @file test_shadow_immune_integration.cpp
 * @brief Unit tests for Shadow Emotions - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between shadow emotions and immune system
 * WHY:  Validate jealousy/envy/narcissism inflammatory effects and inflammation-aggression coupling
 * HOW:  Test maladaptive emotion-triggered inflammation, inflammation-amplified hostility
 *
 * BIOLOGICAL BASIS:
 * - Jealousy, envy, and hostility increase cortisol and inflammation
 * - Chronic inflammation increases irritability, hostility, and aggression
 * - Narcissistic injury triggers stress-induced immune responses
 * - CBT interventions reduce both shadow emotions and inflammation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_shadow_emotions.h"
#include "cognitive/nimcp_emotional_system.h"

class ShadowImmuneTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    shadow_emotion_system_t* shadow;
    emotional_system_t* emotion_system;

    void SetUp() override {
        // Create systems
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        shadow = shadow_system_create(10);  // Track 10 others max
        ASSERT_NE(shadow, nullptr);

        emotion_config_t emo_config = emotion_system_default_config();
        emotion_system = emotion_system_create(&emo_config);
        ASSERT_NE(emotion_system, nullptr);

        // Create bridge
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge_config.enable_shadow_integration = true;
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect shadow system
        int result = emotion_immune_bridge_connect_shadow(bridge, shadow);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        emotion_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        shadow_system_destroy(shadow);
        emotion_system_destroy(emotion_system);
    }
};

/**
 * TEST: Jealousy Triggers Inflammatory Response
 * BIOLOGICAL: Jealousy activates HPA axis, increases cortisol and inflammation
 */
TEST_F(ShadowImmuneTest, JealousyTriggersInflammation) {
    // Trigger jealousy
    shadow_experience_jealousy(shadow, 1,  // bond_id
                                0.8f,       // threat_level
                                0.9f,       // attachment_strength
                                0);

    // Check if jealousy active
    bool is_jealous = shadow_is_active(shadow, SHADOW_JEALOUSY);

    // Trigger immune from shadow emotions
    int result = emotion_immune_trigger_from_shadow(bridge);
    EXPECT_EQ(result, 0);

    // Jealousy should trigger inflammatory response
}

/**
 * TEST: Envy Triggers Inflammatory Response
 * BIOLOGICAL: Envy and resentment increase cortisol and cytokines
 */
TEST_F(ShadowImmuneTest, EnvyTriggersInflammation) {
    // Trigger malicious envy
    shadow_experience_envy(shadow, 2,      // target_id
                           0.3f,            // self_level
                           0.9f,            // other_level (much higher)
                           0.8f,            // maliciousness
                           0);

    // Check if envy active
    bool is_envious = shadow_is_active(shadow, SHADOW_ENVY);

    // Trigger immune
    emotion_immune_trigger_from_shadow(bridge);

    // Malicious envy should trigger inflammation
}

/**
 * TEST: Narcissism Triggers Inflammation
 * BIOLOGICAL: Narcissistic injury activates stress response
 */
TEST_F(ShadowImmuneTest, NarcissismTriggersInflammation) {
    // Assess high narcissism
    shadow_assess_narcissism(shadow, 0.9f,  // grandiosity
                             0.2f,           // empathy (low)
                             0.9f,           // need_for_admiration
                             0.9f);          // entitlement

    // Check if narcissism active
    bool is_narcissistic = shadow_is_active(shadow, SHADOW_NARCISSISM);

    // Trigger immune from narcissistic patterns
    emotion_immune_trigger_from_shadow(bridge);

    // Narcissism (especially when challenged) triggers inflammation
}

/**
 * TEST: Hubris Triggers Inflammation
 * BIOLOGICAL: Overconfidence and grandiosity create stress when reality hits
 */
TEST_F(ShadowImmuneTest, HubrisTriggersInflammation) {
    // Assess hubris
    shadow_assess_hubris(shadow, 10.0f,  // many recent successes
                         0.9f,            // high power
                         0.1f);           // low accountability

    // Check if hubris active
    bool has_hubris = shadow_is_active(shadow, SHADOW_HUBRIS);

    // Trigger immune
    emotion_immune_trigger_from_shadow(bridge);

    // Hubris-driven stress triggers inflammation
}

/**
 * TEST: Greed Triggers Inflammatory Response
 * BIOLOGICAL: Excessive acquisition drive increases stress and inflammation
 */
TEST_F(ShadowImmuneTest, GreedTriggersInflammation) {
    // Assess greed
    shadow_assess_greed(shadow, 0.9f,  // high acquisition value
                        0.1f,           // low necessity
                        0.3f,           // not scarce
                        0);

    // Check if greed active
    bool is_greedy = shadow_is_active(shadow, SHADOW_GREED);

    // Trigger immune
    emotion_immune_trigger_from_shadow(bridge);

    // Greed-driven stress triggers inflammation
}

/**
 * TEST: Inflammation Amplifies Shadow Emotions
 * BIOLOGICAL: Inflammation increases irritability, hostility, and jealousy
 */
TEST_F(ShadowImmuneTest, InflammationAmplifiesShadowEmotions) {
    // Set baseline shadow emotions
    shadow_experience_jealousy(shadow, 1, 0.4f, 0.6f, 0);

    // Trigger high inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_CRITICAL, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Amplify shadow emotions from inflammation
    int result = emotion_immune_amplify_shadow_from_inflammation(bridge);
    EXPECT_EQ(result, 0);

    // Inflammation should amplify jealousy and hostility
    float jealousy_intensity = shadow_get_intensity(shadow, SHADOW_JEALOUSY);
    // Should be higher than baseline 0.4f
}

/**
 * TEST: CBT Intervention Reduces Inflammation
 * BIOLOGICAL: Cognitive reframing reduces stress and inflammation
 */
TEST_F(ShadowImmuneTest, CBTInterventionReducesInflammation) {
    // Create shadow emotion
    shadow_experience_envy(shadow, 2, 0.3f, 0.8f, 0.7f, 0);

    // Trigger inflammation
    emotion_immune_trigger_from_shadow(bridge);

    // Apply CBT intervention
    bool intervention_applied = shadow_apply_intervention(shadow, SHADOW_ENVY,
                                                           SHADOW_INTERVENTION_COGNITIVE_REFRAME,
                                                           1000);

    // Soothe immune from successful intervention
    int result = emotion_immune_soothe_from_shadow_correction(bridge);
    EXPECT_EQ(result, 0);

    // Successful intervention should reduce inflammation
}

/**
 * TEST: Chronic Shadow Emotions Cause Sustained Inflammation
 * BIOLOGICAL: Chronic hostility/jealousy maintains elevated inflammation
 */
TEST_F(ShadowImmuneTest, ChronicShadowEmotionsCauseSustainedInflammation) {
    // Create chronic jealousy by repeated triggering
    for (int i = 0; i < 10; i++) {
        shadow_experience_jealousy(shadow, 1, 0.7f, 0.8f, i * 1000);
        emotion_immune_trigger_from_shadow(bridge);
        shadow_update(shadow, 3600.0f, i * 3600000000);  // 1 hour updates
    }

    // Get mental health impact
    float mental_health_impact = shadow_get_mental_health_impact(shadow);

    // Chronic shadow emotions should maintain elevated inflammation
}

/**
 * TEST: Obsessive Thoughts Trigger Stress Response
 * BIOLOGICAL: Intrusive obsessive thoughts activate stress response
 */
TEST_F(ShadowImmuneTest, ObsessiveThoughtsTriggerInflammation) {
    // Register obsessive thought
    shadow_register_obsession(shadow, 1,  // thought_id
                               OBSESSION_THOUGHT,
                               0.8f,       // intensity
                               0.9f,       // distress
                               0);

    // Check if obsession active
    bool is_obsessive = shadow_is_active(shadow, SHADOW_OBSESSION);

    // Trigger immune from obsessive distress
    emotion_immune_trigger_from_shadow(bridge);

    // Obsessive thoughts should trigger stress-induced inflammation
}

/**
 * TEST: Malignant Narcissism Has Strongest Immune Effect
 * BIOLOGICAL: Malignant narcissism (+ antisocial + paranoid) = severe stress
 */
TEST_F(ShadowImmuneTest, MalignantNarcissismStrongestEffect) {
    // Assess malignant narcissism (high on all features)
    shadow_assess_narcissism(shadow, 0.95f,  // extreme grandiosity
                             0.05f,           // near-zero empathy
                             0.95f,           // extreme need for admiration
                             0.95f);          // extreme entitlement

    // Trigger immune
    emotion_immune_trigger_from_shadow(bridge);

    // Malignant narcissism should have strongest inflammatory effect
}

/**
 * TEST: Mindfulness Intervention Reduces Immune Activation
 * BIOLOGICAL: Mindfulness reduces cortisol and inflammation
 */
TEST_F(ShadowImmuneTest, MindfulnessReducesInflammation) {
    // Create obsessive pattern
    shadow_register_obsession(shadow, 1, OBSESSION_THOUGHT, 0.7f, 0.8f, 0);

    // Trigger inflammation
    emotion_immune_trigger_from_shadow(bridge);

    // Apply mindfulness intervention
    shadow_apply_intervention(shadow, SHADOW_OBSESSION,
                               SHADOW_INTERVENTION_MINDFULNESS,
                               1000);

    // Soothe immune
    emotion_immune_soothe_from_shadow_correction(bridge);

    // Mindfulness should reduce inflammation
}

/**
 * TEST: Gratitude Intervention Counters Envy and Inflammation
 * BIOLOGICAL: Gratitude reduces stress and enhances immunity
 */
TEST_F(ShadowImmuneTest, GratitudeCountersEnvyInflammation) {
    // Create envy
    shadow_experience_envy(shadow, 2, 0.4f, 0.9f, 0.6f, 0);

    // Trigger inflammation
    emotion_immune_trigger_from_shadow(bridge);

    // Apply gratitude intervention
    shadow_apply_intervention(shadow, SHADOW_ENVY,
                               SHADOW_INTERVENTION_GRATITUDE,
                               1000);

    // Soothe immune
    emotion_immune_soothe_from_shadow_correction(bridge);

    // Gratitude should counter envy and reduce inflammation
}

/**
 * TEST: Integration Update Cycle
 */
TEST_F(ShadowImmuneTest, UpdateCycleIntegration) {
    // Create shadow emotion
    shadow_experience_jealousy(shadow, 1, 0.6f, 0.7f, 0);

    // Update shadow system
    shadow_update(shadow, 1.0f, 1000);

    // Update bridge
    int result = emotion_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Trigger, amplify, and soothe
    emotion_immune_trigger_from_shadow(bridge);
    emotion_immune_amplify_shadow_from_inflammation(bridge);
    emotion_immune_soothe_from_shadow_correction(bridge);
}

/**
 * TEST: Null Safety
 */
TEST_F(ShadowImmuneTest, NullSafety) {
    int result = emotion_immune_bridge_connect_shadow(nullptr, shadow);
    EXPECT_NE(result, 0);

    result = emotion_immune_bridge_connect_shadow(bridge, nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_trigger_from_shadow(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_amplify_shadow_from_inflammation(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_soothe_from_shadow_correction(nullptr);
    EXPECT_NE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
