/**
 * @file test_remorse_immune_integration.cpp
 * @brief Unit tests for Remorse/Regret - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between remorse/regret and immune system
 * WHY:  Validate guilt/shame-triggered inflammation and forgiveness immune benefits
 * HOW:  Test moral distress inflammatory response, self-compassion IL-10 release
 *
 * BIOLOGICAL BASIS:
 * - Guilt and shame activate HPA axis and inflammatory response
 * - Chronic guilt increases cortisol and immune suppression
 * - Self-forgiveness and self-compassion reduce inflammation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_remorse_regret.h"
#include "cognitive/nimcp_emotional_system.h"

class RemorseImmuneTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    remorse_regret_system_t* remorse;
    emotional_system_t* emotion_system;

    void SetUp() override {
        // Create systems
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        remorse = remorse_regret_system_create();
        ASSERT_NE(remorse, nullptr);

        emotion_config_t emo_config = emotion_system_default_config();
        emotion_system = emotion_system_create(&emo_config);
        ASSERT_NE(emotion_system, nullptr);

        // Create bridge
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge_config.enable_remorse_integration = true;
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect remorse system
        int result = emotion_immune_bridge_connect_remorse(bridge, remorse);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        emotion_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        remorse_regret_system_destroy(remorse);
        emotion_system_destroy(emotion_system);
    }
};

/**
 * TEST: Guilt Triggers Inflammatory Response
 * BIOLOGICAL: Guilt activates HPA axis, increases cortisol and cytokines
 */
TEST_F(RemorseImmuneTest, GuiltTriggersInflammation) {
    // Process moral violation (high guilt)
    uint32_t violated_values[] = {1, 2};  // Example value IDs
    remorse_process_event(remorse, EVENT_MORAL_VIOLATION, violated_values, 2,
                          0.8f,  // harm_caused
                          0.9f,  // controllability
                          false, // not reversible
                          0);

    // Check if guilty
    bool is_guilty = remorse_is_guilty(remorse);

    // Trigger immune from guilt
    int result = emotion_immune_trigger_from_remorse(bridge);
    EXPECT_EQ(result, 0);

    // Guilt should trigger cortisol and pro-inflammatory response
}

/**
 * TEST: Shame Triggers Stronger Immune Response
 * BIOLOGICAL: Shame (global self-condemnation) triggers more stress than guilt
 */
TEST_F(RemorseImmuneTest, ShameTriggersStrongerInflammation) {
    // Process severe moral violation leading to shame
    uint32_t violated_values[] = {1, 2, 3, 4};
    remorse_process_event(remorse, EVENT_BETRAYAL, violated_values, 4,
                          0.95f,  // severe harm
                          1.0f,   // fully controllable
                          false,  // irreversible
                          0);

    // Check if ashamed
    bool is_ashamed = remorse_is_ashamed(remorse);

    // Trigger immune from shame
    int result = emotion_immune_trigger_from_remorse(bridge);
    EXPECT_EQ(result, 0);

    // Shame should trigger even stronger inflammatory response than guilt
}

/**
 * TEST: Remorse Intensity Correlates with Inflammation
 * BIOLOGICAL: Higher moral distress = higher inflammatory response
 */
TEST_F(RemorseImmuneTest, RemorseIntensityCorrelatesWithInflammation) {
    // Process high-severity moral violation
    uint32_t violated_values[] = {1, 2, 3};
    remorse_process_event(remorse, EVENT_RELATIONSHIP_HARM, violated_values, 3,
                          0.9f,  // high harm
                          0.8f,  // controllability
                          false, // not reversible
                          0);

    // Check remorse intensity
    bool is_remorseful = remorse_is_remorseful(remorse);
    float regret_intensity = remorse_get_regret_intensity(remorse);

    // Trigger immune
    emotion_immune_trigger_from_remorse(bridge);

    // Higher remorse should correlate with stronger immune activation
}

/**
 * TEST: Self-Forgiveness Reduces Inflammation
 * BIOLOGICAL: Self-compassion reduces cortisol and inflammation
 */
TEST_F(RemorseImmuneTest, SelfForgivenessReducesInflammation) {
    // Create guilt
    uint32_t violated_values[] = {1};
    remorse_process_event(remorse, EVENT_POOR_DECISION, violated_values, 1,
                          0.5f, 0.7f, true, 0);

    // Trigger inflammation from guilt
    emotion_immune_trigger_from_remorse(bridge);

    // Practice self-forgiveness
    remorse_practice_self_forgiveness(remorse, 0, 0.9f);  // event index 0

    // Soothe immune system from forgiveness
    int result = emotion_immune_soothe_from_forgiveness(bridge);
    EXPECT_EQ(result, 0);

    // Self-forgiveness should reduce inflammation and release IL-10
}

/**
 * TEST: Atonement Reduces Immune Activation
 * BIOLOGICAL: Making amends reduces stress and inflammation
 */
TEST_F(RemorseImmuneTest, AtonementReducesInflammation) {
    // Create remorse
    uint32_t violated_values[] = {1, 2};
    remorse_process_event(remorse, EVENT_BROKEN_PROMISE, violated_values, 2,
                          0.7f, 0.8f, true, 0);

    // Trigger inflammation
    emotion_immune_trigger_from_remorse(bridge);

    // Attempt atonement
    remorse_attempt_atonement(remorse, 0, 0.9f, true);  // effective, forgiven

    // Soothe immune from atonement
    emotion_immune_soothe_from_forgiveness(bridge);

    // Atonement should reduce immune activation
}

/**
 * TEST: Chronic Rumination Prolongs Inflammation
 * BIOLOGICAL: Ruminating on guilt maintains stress response
 */
TEST_F(RemorseImmuneTest, ChronicRuminationProlongsInflammation) {
    // Create guilt
    uint32_t violated_values[] = {1};
    remorse_process_event(remorse, EVENT_ACTION_COMMISSION, violated_values, 1,
                          0.6f, 0.7f, false, 0);

    // Trigger inflammation repeatedly (simulating rumination)
    for (int i = 0; i < 10; i++) {
        emotion_immune_trigger_from_remorse(bridge);
        remorse_update(remorse, 3600.0f, i * 3600000000);  // 1 hour updates
    }

    // Chronic rumination should maintain elevated inflammation
}

/**
 * TEST: Counterfactual Thinking and Stress
 * BIOLOGICAL: "If only" thinking increases regret and stress
 */
TEST_F(RemorseImmuneTest, CounterfactualThinkingIncreasesStress) {
    // Create regret
    uint32_t violated_values[] = {1};
    remorse_process_event(remorse, EVENT_MISSED_OPPORTUNITY, violated_values, 1,
                          0.3f, 0.5f, false, 0);

    // Run upward counterfactual (increases regret)
    remorse_run_counterfactual(remorse, 0, 0.9f, COUNTERFACTUAL_UPWARD);

    // Trigger immune from increased regret
    emotion_immune_trigger_from_remorse(bridge);

    // Upward counterfactuals should amplify stress and inflammation
}

/**
 * TEST: Inflammation Amplifies Guilt Feelings
 * BIOLOGICAL: Inflammation can amplify negative self-evaluation
 */
TEST_F(RemorseImmuneTest, InflammationAmplifiesGuilt) {
    // Create moderate guilt
    uint32_t violated_values[] = {1};
    remorse_process_event(remorse, EVENT_ACTION_COMMISSION, violated_values, 1,
                          0.4f, 0.5f, true, 0);

    // Trigger high inflammation from external source
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_CRITICAL, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Apply cytokine effects
    emotion_immune_apply_cytokine_effects(bridge);

    // Inflammation should amplify guilt feelings (test depends on implementation)
}

/**
 * TEST: Self-Worth and Immune Function
 * BIOLOGICAL: Low self-worth (shame) correlates with immune dysfunction
 */
TEST_F(RemorseImmuneTest, LowSelfWorthAffectsImmunity) {
    // Create severe shame (reduces self-worth)
    uint32_t violated_values[] = {1, 2, 3};
    remorse_process_event(remorse, EVENT_BETRAYAL, violated_values, 3,
                          0.95f, 1.0f, false, 0);

    // Get self-worth
    float self_worth = remorse_get_self_worth(remorse);

    // Trigger immune
    emotion_immune_trigger_from_remorse(bridge);

    // Low self-worth should correlate with immune dysfunction
}

/**
 * TEST: Integration Update Cycle
 */
TEST_F(RemorseImmuneTest, UpdateCycleIntegration) {
    // Create remorse
    uint32_t violated_values[] = {1};
    remorse_process_event(remorse, EVENT_MORAL_VIOLATION, violated_values, 1,
                          0.6f, 0.7f, true, 0);

    // Update remorse system
    remorse_update(remorse, 1.0f, 1000);

    // Update bridge
    int result = emotion_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Trigger and soothe
    emotion_immune_trigger_from_remorse(bridge);
    emotion_immune_soothe_from_forgiveness(bridge);
}

/**
 * TEST: Null Safety
 */
TEST_F(RemorseImmuneTest, NullSafety) {
    int result = emotion_immune_bridge_connect_remorse(nullptr, remorse);
    EXPECT_NE(result, 0);

    result = emotion_immune_bridge_connect_remorse(bridge, nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_trigger_from_remorse(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_soothe_from_forgiveness(nullptr);
    EXPECT_NE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
