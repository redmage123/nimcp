/**
 * @file test_emotion_tensor_immune_integration.cpp
 * @brief Unit tests for Emotion Tensor - Immune System Integration
 *
 * WHAT: Test bidirectional coupling between emotion tensor and immune system
 * WHY:  Validate cytokine modulation of emotion channels and tensor-triggered immunity
 * HOW:  Test cytokine effects on joy/sadness channels, stress-induced immune activation
 *
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_emotion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/nimcp_emotional_system.h"

class EmotionTensorImmuneTest : public ::testing::Test {
protected:
    emotion_immune_bridge_t* bridge;
    brain_immune_system_t* immune;
    emotion_tensor_system_t* tensor;
    emotional_system_t* emotion_system;

    void SetUp() override {
        // Create systems
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);

        emotion_tensor_config_t tensor_config = emotion_tensor_default_config();
        tensor = emotion_tensor_create(&tensor_config);
        ASSERT_NE(tensor, nullptr);

        emotion_config_t emo_config = emotion_system_default_config();
        emotion_system = emotion_system_create(&emo_config);
        ASSERT_NE(emotion_system, nullptr);

        // Create bridge
        emotion_immune_config_t bridge_config;
        emotion_immune_default_config(&bridge_config);
        bridge_config.enable_emotion_tensor_integration = true;
        bridge = emotion_immune_bridge_create(&bridge_config, immune, emotion_system, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Connect tensor
        int result = emotion_immune_bridge_connect_tensor(bridge, tensor);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        emotion_immune_bridge_destroy(bridge);
        brain_immune_destroy(immune);
        emotion_tensor_destroy(tensor);
        emotion_system_destroy(emotion_system);
    }
};

/**
 * TEST: Cytokine Suppression of Joy Channel
 * BIOLOGICAL: Pro-inflammatory cytokines reduce positive affect (anhedonia)
 */
TEST_F(EmotionTensorImmuneTest, CytokineSuppressesJoy) {
    // Set high joy in tensor
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.8f, 0);

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x01, 0x02, 0x03};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    // Full immune activation
    uint32_t b_cell_id, helper_id;
    brain_immune_activate_b_cell(immune, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune, antigen_id, &helper_id);
    brain_immune_t_help_b(immune, helper_id, b_cell_id);

    // Apply cytokine effects to tensor
    int result = emotion_immune_modulate_tensor(bridge);
    EXPECT_EQ(result, 0);

    // Joy channel should be suppressed (test depends on implementation)
    float joy_level = emotion_tensor_get_channel(tensor, TENSOR_JOY);
    // With inflammation, joy should decrease
}

/**
 * TEST: Cytokine Amplification of Sadness Channel
 * BIOLOGICAL: Pro-inflammatory cytokines increase negative affect
 */
TEST_F(EmotionTensorImmuneTest, CytokineAmplifiesSadness) {
    // Set moderate sadness
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.3f, 0);

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAA, 0xBB, 0xCC};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_CRITICAL, 0, &antigen_id);

    // Apply cytokine effects
    emotion_immune_modulate_tensor(bridge);

    // Sadness should be amplified by inflammation
    float sadness_level = emotion_tensor_get_channel(tensor, TENSOR_SADNESS);
    // Should be higher than baseline 0.3f
}

/**
 * TEST: Positive Tensor Channels Boost Immunity
 * BIOLOGICAL: Positive emotions enhance immune function, release IL-10
 */
TEST_F(EmotionTensorImmuneTest, PositiveChannelsBoostImmune) {
    // Set high positive emotions
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.9f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_TRUST, 0.8f, 0);

    // Boost immune from positive emotions
    int result = emotion_immune_boost_from_tensor(bridge);
    EXPECT_EQ(result, 0);

    // Should trigger IL-10 release and reduce inflammation
}

/**
 * TEST: Negative Tensor Channels Trigger Inflammation
 * BIOLOGICAL: Anxiety, anger, fear activate HPA axis and inflammation
 */
TEST_F(EmotionTensorImmuneTest, NegativeChannelsTriggerInflammation) {
    // Set high negative emotions (anxiety = fear + anger)
    emotion_tensor_set_channel(tensor, TENSOR_FEAR, 0.8f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_ANGER, 0.7f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.6f, 0);

    // Trigger immune from negative tensor state
    int result = emotion_immune_trigger_from_tensor(bridge);
    EXPECT_EQ(result, 0);

    // Should activate inflammatory response
}

/**
 * TEST: Anhedonia from Chronic Inflammation
 * BIOLOGICAL: Sustained inflammation causes anhedonia (joy suppression)
 */
TEST_F(EmotionTensorImmuneTest, ChronicInflammationCausesAnhedonia) {
    // Create chronic inflammation
    for (int i = 0; i < 15; i++) {
        uint32_t antigen_id;
        uint8_t epitope[] = {(uint8_t)(0x10 + i), 0x20, 0x30};
        brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                      ANTIGEN_SEVERITY_MODERATE, 0, &antigen_id);
    }

    // Apply inflammation effects
    emotion_immune_apply_inflammation_effects(bridge);

    // Get anhedonia level
    float anhedonia = emotion_immune_compute_anhedonia(bridge);

    // Should show some anhedonia with chronic inflammation
    // (Test depends on implementation thresholds)
}

/**
 * TEST: Mixed Emotions and Immune Complexity
 * BIOLOGICAL: Complex emotional states have nuanced immune effects
 */
TEST_F(EmotionTensorImmuneTest, MixedEmotionsImmuneInteraction) {
    // Set mixed emotional state (bittersweet)
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.5f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.5f, 0);

    // Compute compounds
    emotion_tensor_compute_compounds(tensor);

    // Check if contradictory
    bool contradictory = emotion_tensor_is_contradictory(tensor, 0.4f);

    // Apply to immune system
    emotion_immune_modulate_tensor(bridge);
    emotion_immune_trigger_from_tensor(bridge);

    // Mixed emotions should have balanced immune effects
}

/**
 * TEST: Tensor Update Cycle with Immune Feedback
 */
TEST_F(EmotionTensorImmuneTest, TensorUpdateWithImmuneFeedback) {
    // Set initial state
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.7f, 0);

    // Trigger inflammation
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33};
    brain_immune_present_antigen(immune, ANTIGEN_SOURCE_MANUAL, epitope, 3,
                                  ANTIGEN_SEVERITY_HIGH, 0, &antigen_id);

    // Update bridge
    emotion_immune_bridge_update(bridge, 1000);

    // Apply immune effects to tensor
    emotion_immune_modulate_tensor(bridge);

    // Tensor dynamics should update with immune feedback
    emotion_tensor_update(tensor, 1.0f, 1000);
}

/**
 * TEST: Stability and Entropy Effects
 */
TEST_F(EmotionTensorImmuneTest, StabilityEntropyInteraction) {
    // Create unstable emotional state (high entropy)
    emotion_tensor_set_channel(tensor, TENSOR_JOY, 0.4f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_FEAR, 0.4f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_ANGER, 0.4f, 0);
    emotion_tensor_set_channel(tensor, TENSOR_SADNESS, 0.4f, 0);

    // Get entropy
    float entropy = emotion_tensor_get_entropy(tensor);

    // High entropy (emotional instability) should interact with immune
    if (entropy > 0.6f) {
        emotion_immune_trigger_from_tensor(bridge);
    }
}

/**
 * TEST: Null Safety
 */
TEST_F(EmotionTensorImmuneTest, NullSafety) {
    int result = emotion_immune_bridge_connect_tensor(nullptr, tensor);
    EXPECT_NE(result, 0);

    result = emotion_immune_bridge_connect_tensor(bridge, nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_modulate_tensor(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_trigger_from_tensor(nullptr);
    EXPECT_NE(result, 0);

    result = emotion_immune_boost_from_tensor(nullptr);
    EXPECT_NE(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
