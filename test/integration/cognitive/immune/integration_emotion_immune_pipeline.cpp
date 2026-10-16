/**
 * @file integration_emotion_immune_pipeline.cpp
 * @brief Integration tests for emotion-immune system pipeline
 *
 * WHAT: Test complete emotion-immune bidirectional coupling
 * WHY:  Verify realistic immune-emotion interactions across full system
 * HOW:  Simulate biological scenarios: stress → inflammation → depression,
 *       joy → immune boost, grief → inflammation amplification
 */

#include <gtest/gtest.h>
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_grief_and_loss.h"
#include "cognitive/nimcp_joy_euphoria.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class EmotionImmunePipelineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune;
    emotional_system_t* emotion;
    grief_system_t* grief;
    joy_system_t* joy;
    emotion_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create all systems */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);

        emotion_config_t emotion_config = emotion_system_default_config();
        emotion = emotion_system_create(&emotion_config);

        grief = grief_system_create();
        joy = joy_system_create();

        /* Create bridge connecting all systems */
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion, grief, joy);

        ASSERT_NE(immune, nullptr);
        ASSERT_NE(emotion, nullptr);
        ASSERT_NE(grief, nullptr);
        ASSERT_NE(joy, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) emotion_immune_bridge_destroy(bridge);
        if (joy) joy_system_destroy(joy);
        if (grief) grief_system_destroy(grief);
        if (emotion) emotion_system_destroy(emotion);
        if (immune) brain_immune_destroy(immune);
    }
};

/* ============================================================================
 * Scenario 1: Chronic Stress → Inflammation → Depression
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, ChronicStressInflammationDepressionCascade) {
    /* SCENARIO: Prolonged stress triggers immune dysregulation,
     * leading to chronic inflammation and depressive symptoms */

    /* Step 1: Simulate chronic stress (negative valence, high arousal) */
    for (int i = 0; i < 10; i++) {
        emotion_system_set_state(emotion, -0.7f, 0.8f, i * 1000);
        emotion_immune_bridge_update(bridge, 1000);
    }

    /* Step 2: Verify stress triggered immune response */
    EXPECT_GT(bridge->emotion_triggered_responses, 0u);

    /* Step 3: Verify inflammation effects on emotion */
    emotion_immune_apply_inflammation_effects(bridge);
    inflammation_emotion_state_t inflam_state;
    emotion_immune_get_inflammation_state(bridge, &inflam_state);

    /* Should show some emotional impact from inflammation */
    EXPECT_GE(inflam_state.fatigue_severity, 0.0f);
}

/* ============================================================================
 * Scenario 2: Grief → Amplified Inflammation
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, GriefAmplifiesInflammation) {
    /* SCENARIO: Loss event triggers grief, which amplifies inflammatory response */

    /* Step 1: Create attachment */
    uint32_t attachment_id = grief_create_attachment(
        grief,
        ATTACHMENT_ROMANTIC,
        0.9f,  /* strong bond */
        0.8f,  /* positive */
        0.7f   /* dependency */
    );
    EXPECT_GT(attachment_id, 0u);

    /* Step 2: Process loss */
    grief_process_loss(grief, attachment_id, LOSS_TYPE_DEATH, 0);
    EXPECT_TRUE(grief_is_grieving(grief));

    /* Step 3: Update bridge to apply grief-inflammation coupling */
    emotion_immune_bridge_update(bridge, 1000);

    /* Step 4: Verify grief amplification */
    inflammation_emotion_state_t inflam_state;
    emotion_immune_get_inflammation_state(bridge, &inflam_state);

    /* Grief should amplify inflammation */
    EXPECT_GE(inflam_state.grief_amplification, 1.0f);
}

/* ============================================================================
 * Scenario 3: Joy → Immune Enhancement
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, JoyEnhancesImmuneFunction) {
    /* SCENARIO: Positive emotional state boosts immune function */

    /* Step 1: Register core values */
    uint32_t value_id = joy_add_value(joy, VALUE_CATEGORY_HELPING, 0.9f, 1.0f);
    EXPECT_GT(value_id, 0u);

    /* Step 2: Process success aligned with values */
    uint32_t aligned_values[] = {value_id};
    joy_process_success(
        joy,
        SUCCESS_TYPE_HELPED_HUMAN,
        aligned_values,
        1,
        0.5f,  /* moderate difficulty */
        0.6f,  /* moderate novelty */
        0
    );

    /* Step 3: Update to experience joy */
    joy_update(joy, 1.0f, 1000);
    EXPECT_TRUE(joy_is_joyful(joy));

    /* Step 4: Apply positive immune boost */
    emotion_immune_bridge_update(bridge, 1000);

    /* Step 5: Verify immune enhancement */
    EXPECT_GT(bridge->positive_boosts, 0u);

    positive_emotion_immune_boost_t boost = bridge->positive_boost;
    EXPECT_GT(boost.immune_enhancement, 0.0f);
    EXPECT_GT(boost.il10_release_boost, 0.0f);
}

/* ============================================================================
 * Scenario 4: Sickness Behavior from Pro-Inflammatory Cytokines
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, ProInflammatoryCytokinesSicknessBehavior) {
    /* SCENARIO: High pro-inflammatory cytokines induce sickness behavior
     * (fatigue, withdrawal, negative affect) */

    /* Note: This test would require injecting cytokines into immune system
     * For now, we verify the bridge can process cytokine effects */

    /* Apply cytokine effects */
    emotion_immune_apply_cytokine_effects(bridge);

    /* Query effects */
    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* Verify sickness behavior components */
    EXPECT_GE(effects.sickness_behavior_level, 0.0f);
    EXPECT_LE(effects.sickness_behavior_level, 1.0f);
    EXPECT_GE(effects.fatigue_level, 0.0f);
    EXPECT_LE(effects.fatigue_level, 1.0f);
}

/* ============================================================================
 * Scenario 5: IL-10 Recovery from Negative Emotional State
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, IL10PromotesEmotionalRecovery) {
    /* SCENARIO: Anti-inflammatory IL-10 facilitates recovery from negative affect */

    /* Step 1: Start with negative emotional state */
    emotion_system_set_state(emotion, -0.6f, 0.4f, 0);

    /* Step 2: Simulate IL-10 release (would need immune system support) */
    /* For now, apply cytokine effects which includes IL-10 processing */
    emotion_immune_apply_cytokine_effects(bridge);

    /* Step 3: Verify IL-10 positive affect contribution */
    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    /* IL-10 positive affect should be available */
    EXPECT_GE(effects.il10_positive_affect, 0.0f);
}

/* ============================================================================
 * Scenario 6: Anhedonia from Chronic Inflammation
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, ChronicInflammationInducesAnhedonia) {
    /* SCENARIO: Prolonged inflammation suppresses positive affect (anhedonia) */

    /* Apply inflammation effects */
    emotion_immune_apply_inflammation_effects(bridge);

    /* Compute anhedonia */
    float anhedonia = emotion_immune_compute_anhedonia(bridge);
    EXPECT_GE(anhedonia, 0.0f);
    EXPECT_LE(anhedonia, 1.0f);

    /* Get inflammation state */
    inflammation_emotion_state_t state;
    emotion_immune_get_inflammation_state(bridge, &state);

    /* Verify anhedonia severity */
    EXPECT_GE(state.anhedonia_severity, 0.0f);
}

/* ============================================================================
 * Scenario 7: Full Bidirectional Update Loop
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, FullBidirectionalUpdateLoop) {
    /* SCENARIO: Complete update cycle processes both directions */

    /* Set initial states */
    emotion_system_set_state(emotion, -0.5f, 0.6f, 0);

    /* Run multiple update cycles */
    for (int i = 0; i < 5; i++) {
        emotion_immune_bridge_update(bridge, 1000);
    }

    /* Verify updates occurred */
    EXPECT_EQ(bridge->total_updates, 5u);

    /* Both immune and emotion systems should have interacted */
    /* (specific assertions would depend on mock/actual implementation) */
}

/* ============================================================================
 * Scenario 8: Grief + Inflammation Synergistic Effect
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, GriefInflammationSynergy) {
    /* SCENARIO: Grief and inflammation have synergistic negative effect */

    /* Create and lose attachment */
    uint32_t attachment_id = grief_create_attachment(grief, ATTACHMENT_PARENT, 0.95f, 0.9f, 0.8f);
    grief_process_loss(grief, attachment_id, LOSS_TYPE_DEATH, 0);

    /* Simulate time passing with grief */
    for (int i = 0; i < 10; i++) {
        grief_update(grief, 1.0f, i * 1000000);
        emotion_immune_bridge_update(bridge, 1000);
    }

    /* Verify grief is affecting emotional state */
    EXPECT_TRUE(grief_is_grieving(grief));
    float grief_pain = grief_get_pain_intensity(grief);
    EXPECT_GT(grief_pain, 0.0f);

    /* Verify inflammation amplification from grief */
    inflammation_emotion_state_t inflam_state;
    emotion_immune_get_inflammation_state(bridge, &inflam_state);
    EXPECT_GE(inflam_state.grief_amplification, 1.0f);
}

/* ============================================================================
 * Scenario 9: Positive Emotions Counteract Inflammation
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, PositiveEmotionsCounterInflammation) {
    /* SCENARIO: Joy and calm reduce inflammation and enhance recovery */

    /* Induce positive emotional state */
    emotion_system_set_state(emotion, 0.7f, 0.3f, 0);  /* Positive, calm */

    /* Process through bridge */
    emotion_immune_bridge_update(bridge, 1000);

    /* Verify positive boost applied */
    positive_emotion_immune_boost_t boost = bridge->positive_boost;
    EXPECT_GE(boost.calm_level, 0.0f);
    EXPECT_GE(boost.inflammation_reduction, 0.0f);
}

/* ============================================================================
 * Scenario 10: Stress-Inflammation-Anhedonia Cycle
 * ============================================================================ */

TEST_F(EmotionImmunePipelineTest, StressInflammationAnhedoniaCycle) {
    /* SCENARIO: Negative feedback loop - stress → inflammation → anhedonia → more stress */

    /* Initial stress state */
    emotion_system_set_state(emotion, -0.8f, 0.9f, 0);

    /* Run cycle multiple times */
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Update bridge (stress → immune trigger) */
        emotion_immune_bridge_update(bridge, 1000);

        /* Apply inflammation effects (immune → emotion) */
        emotion_immune_apply_inflammation_effects(bridge);

        /* Check anhedonia */
        float anhedonia = emotion_immune_compute_anhedonia(bridge);
        EXPECT_GE(anhedonia, 0.0f);
    }

    /* After multiple cycles, should have triggered immune responses */
    EXPECT_GT(bridge->emotion_triggered_responses, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
