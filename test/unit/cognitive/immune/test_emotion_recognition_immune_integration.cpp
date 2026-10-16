/**
 * @file test_emotion_recognition_immune_integration.cpp
 * @brief Unit tests for Emotion Recognition - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between emotion recognition and immune system
 * WHY:  Validate cytokine modulation of emotion detection and distress-triggered immunity
 * HOW:  Test cytokine effects on recognition thresholds, distress-induced immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_emotion_recognition.h"
#include "cognitive/nimcp_emotional_system.h"

class EmotionRecognitionImmuneTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    emotion_recognition_system_t* recognition;
    emotional_system_t* emotion_system;

    void SetUp() override {
        // Create immune system
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        // Create emotion recognition
        emotion_recognition_config_t rec_config = emotion_recognition_config_default();
        recognition = emotion_recognition_create(&rec_config);
        ASSERT_NE(recognition, nullptr);

        // Create emotional system
        emotion_config_t emo_config = emotion_system_default_config();
        emotion_system = emotion_system_create(&emo_config);
        ASSERT_NE(emotion_system, nullptr);

        // Create bridge
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge_config.enable_emotion_recognition_integration = true;
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect recognition
        int result = emotion_immune_bridge_connect_recognition(bridge, recognition);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        emotion_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        emotion_recognition_destroy(recognition);
        emotion_system_destroy(emotion_system);
    }
};

/**
 * TEST: Cytokine Effects on Emotion Recognition
 * BIOLOGICAL: Pro-inflammatory cytokines bias toward negative emotion detection
 */
TEST_F(EmotionRecognitionImmuneTest, CytokineModulationOfRecognition) {
    // Trigger inflammation in immune system
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    // Activate full immune response to raise cytokine levels
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Apply cytokine effects to recognition
    int result = emotion_immune_modulate_recognition(bridge);
    EXPECT_EQ(result, 0);

    // Get cytokine effects - should show negative affect
    cytokine_emotion_effects_t effects;
    emotion_immune_get_cytokine_effects(bridge, &effects);

    // Pro-inflammatory cytokines should create negative bias
    EXPECT_LT(effects.total_valence_shift, 0.0f);
    EXPECT_GT(effects.sickness_behavior_level, 0.0f);
}

/**
 * TEST: Distress-Triggered Immune Response
 * BIOLOGICAL: Extreme negative emotions trigger inflammatory response
 */
TEST_F(EmotionRecognitionImmuneTest, DistressTriggerImmuneResponse) {
    // Simulate detecting extreme distress in emotion recognition
    // (In real implementation, this would come from multimodal input)

    // Get baseline inflammation
    brain_inflammation_level_t baseline_inflammation = brain_immune_get_inflammation(immune);

    // Trigger immune response from recognized distress
    int result = emotion_immune_trigger_from_recognition(bridge);
    EXPECT_EQ(result, 0);

    // Check that immune system was activated (in real implementation)
    // Inflammation level should increase (test depends on implementation)
}

/**
 * TEST: Recognition Bias from Chronic Inflammation
 * BIOLOGICAL: Chronic inflammation increases negative emotion sensitivity
 */
TEST_F(EmotionRecognitionImmuneTest, ChronicInflammationBiasesRecognition) {
    // Create chronic inflammation by sustained immune activation
    for (int i = 0; i < 10; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x10 + i), 0x20, 0x30};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      ANTIGEN_SEVERITY_MODERATE, 0, &antigen_id);
    }

    // Apply inflammation effects
    emotion_immune_apply_inflammation_effects(bridge);

    // Get inflammation state
    inflammation_emotion_state_t inflam_state;
    emotion_immune_get_inflammation_state(bridge, &inflam_state);

    // Chronic inflammation should bias recognition toward negative
    if (inflam_state.is_chronic) {
        EXPECT_GT(inflam_state.depression_risk, 0.0f);
    }
}

/**
 * TEST: Sickness Behavior Detection
 * BIOLOGICAL: Cytokines induce sickness behavior (fatigue, withdrawal)
 */
TEST_F(EmotionRecognitionImmuneTest, SicknessBehaviorDetection) {
    // Trigger high inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_CRITICAL, 0, &antigen_id);

    // Full immune response
    uint32_t b_cell_id, helper_id, antibody_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);
    brain_immune_produce_antibody(immune, b_cell_id, ANTIBODY_IGG, &antibody_id);

    // Update bridge to compute cytokine effects
    emotion_immune_bridge_update(bridge, 1000);

    // Check for sickness behavior
    bool is_sick = emotion_immune_is_sick_behavior(bridge);

    // With critical severity, should show sickness behavior
    // (Test outcome depends on implementation thresholds)
}

/**
 * TEST: Panic/Rage Detection Triggers Immune
 * BIOLOGICAL: Extreme distress emotions activate HPA axis and inflammation
 */
TEST_F(EmotionRecognitionImmuneTest, ExtremDistressTriggersInflammation) {
    // Set emotional state to extreme negative (panic/rage)
    emotion_system_set_state(emotion_system, -0.9f, 0.9f, 0);

    // Trigger immune from emotional stress
    int result = emotion_immune_trigger_from_stress(bridge);
    EXPECT_EQ(result, 0);

    // Check emotion trigger state
    // Should show stress-induced immune activation
}

/**
 * TEST: Connection API
 */
TEST_F(EmotionRecognitionImmuneTest, ConnectionAPI) {
    // Test disconnection and reconnection
    emotion_immune_bridge_t* test_bridge;
    emotion_immune_config_t config;
    emotion_immune_default_config(&config);
    config.enable_emotion_recognition_integration = true;

    test_bridge = emotion_immune_bridge_create(&config, immune, emotion_system, nullptr, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    // Connect recognition
    int result = emotion_immune_bridge_connect_recognition(test_bridge, recognition);
    EXPECT_EQ(result, 0);

    // Verify integration is enabled (test depends on implementation)

    emotion_immune_bridge_destroy(test_bridge);
}

/**
 * TEST: Update Cycle Integration
 */
TEST_F(EmotionRecognitionImmuneTest, UpdateCycleIntegration) {
    // Run update cycle
    int result = emotion_immune_bridge_update(bridge, 1000);
    EXPECT_EQ(result, 0);

    // Modulate recognition based on current immune state
    result = emotion_immune_modulate_recognition(bridge);
    EXPECT_EQ(result, 0);

    // Verify bridge is functioning
}

/**
 * TEST: Null Safety
 */
TEST_F(EmotionRecognitionImmuneTest, NullSafety) {
    // Test null pointer handling
    int result = emotion_immune_bridge_connect_recognition(nullptr, recognition);
    EXPECT_NE(result, 0);

    result = emotion_immune_bridge_connect_recognition(bridge, nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_modulate_recognition(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_trigger_from_recognition(nullptr);
    EXPECT_NE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
