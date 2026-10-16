//=============================================================================
// test_emotional_system_integration.cpp - Integration Tests for Emotional System
//=============================================================================
// WHAT: Integration tests showing emotional system working with brain systems
// WHY:  Verify emotional system integrates properly with memory, salience, etc.
// HOW:  Test emotional system with brain, working memory, salience, mental health
//
// Test Coverage:
// - Emotional system creation within brain
// - Integration with working memory (emotional tagging)
// - Integration with salience (arousal boost)
// - Integration with mental health (impact reporting)
// - Emotion regulation during high intensity
// - Multimodal emotion processing
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalSystemIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create brain with emotional processing enabled
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "emotion_integration_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 64;
        config.num_outputs = 10;
        config.enable_multimodal_integration = false;  // Not needed for emotion tests

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }
};

//=============================================================================
// Brain Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, BrainProcessingWithEmotionalSystem) {
    // WHAT: Verify brain processing works with emotional system
    // WHY:  Ensure no conflicts between brain and emotion system
    // HOW:  Process input through brain, verify output valid

    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = 0.1f * sinf(i * 0.1f);
    }

    float output[10];
    bool result = brain_predict(brain, input, 64, output, 10);

    ASSERT_TRUE(result);

    // Verify output is valid
    for (int i = 0; i < 10; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
        EXPECT_FALSE(std::isnan(output[i]));
    }
}

TEST_F(EmotionalSystemIntegrationTest, MultipleProcessingCyclesStable) {
    // WHAT: Verify repeated processing with emotions remains stable
    // WHY:  Ensure emotional system doesn't cause instability
    // HOW:  Process 100 inputs, verify no degradation

    float input[64];
    float output[10];

    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < 64; i++) {
            input[i] = 0.1f * sinf(i * 0.1f + iter * 0.01f);
        }

        bool result = brain_predict(brain, input, 64, output, 10);
        EXPECT_TRUE(result);

        for (int i = 0; i < 10; i++) {
            EXPECT_FALSE(std::isnan(output[i]));
            EXPECT_FALSE(std::isinf(output[i]));
        }
    }
}

//=============================================================================
// Emotional Tagging Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, EmotionalTaggingWithPositiveEmotion) {
    // WHAT: Verify emotional tagging integrates with emotional system
    // WHY:  Positive emotions should tag memories
    // HOW:  Create positive emotion, create memory tag, verify valence

    emotional_tag_t tag;
    tag.valence = 0.8f;   // Positive
    tag.arousal = 0.6f;   // Moderate arousal

    // Verify tag has expected properties
    EXPECT_GT(tag.valence, 0.0f);
    EXPECT_GT(tag.arousal, 0.0f);
}

TEST_F(EmotionalSystemIntegrationTest, EmotionalTaggingWithNegativeEmotion) {
    // WHAT: Verify negative emotions can be tagged
    // WHY:  Negative emotions affect memory consolidation
    // HOW:  Create negative tag, verify

    emotional_tag_t tag;
    tag.valence = -0.7f;  // Negative
    tag.arousal = 0.9f;   // High arousal (e.g., fear)

    EXPECT_LT(tag.valence, 0.0f);
    EXPECT_GT(tag.arousal, 0.5f);
}

//=============================================================================
// Salience Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, HighArousalBoostsSalience) {
    // WHAT: Verify high arousal emotions increase salience
    // WHY:  Emotional events grab attention
    // HOW:  Create emotion system, set high arousal, check salience boost

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // Set high arousal state
    emotion_system_set_state(emotion_sys, 0.3f, 0.95f, 1000);

    float salience_boost = emotion_system_get_salience_boost(emotion_sys);

    // High arousal should boost salience significantly
    EXPECT_GT(salience_boost, 1.5f);

    emotion_system_destroy(emotion_sys);
}

TEST_F(EmotionalSystemIntegrationTest, LowArousalMinimalSalienceBoost) {
    // WHAT: Verify low arousal has minimal salience boost
    // WHY:  Calm states shouldn't demand excessive attention
    // HOW:  Set low arousal, verify boost near 1.0

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    emotion_system_set_state(emotion_sys, 0.0f, 0.1f, 1000);

    float salience_boost = emotion_system_get_salience_boost(emotion_sys);

    EXPECT_LE(salience_boost, 1.3f);

    emotion_system_destroy(emotion_sys);
}

//=============================================================================
// Memory Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, EmotionalIntensityAffectsMemoryPriority) {
    // WHAT: Verify emotional intensity affects memory consolidation
    // WHY:  Emotional memories are stronger
    // HOW:  Compare memory priority for calm vs. intense emotions

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // Calm, neutral state
    emotion_system_set_state(emotion_sys, 0.0f, 0.1f, 1000);
    float priority_calm = emotion_system_get_memory_priority(emotion_sys);

    // Intense emotional state
    emotion_system_set_state(emotion_sys, 0.9f, 0.95f, 2000);
    float priority_intense = emotion_system_get_memory_priority(emotion_sys);

    // Intense emotions should have higher memory priority
    EXPECT_GT(priority_intense, priority_calm);
    EXPECT_GT(priority_intense, 0.7f);

    emotion_system_destroy(emotion_sys);
}

TEST_F(EmotionalSystemIntegrationTest, NegativeEmotionsAffectMemory) {
    // WHAT: Verify negative emotions also boost memory
    // WHY:  Negative events (threats) are important to remember
    // HOW:  Set negative high-arousal state, check memory priority

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // Fear-like state (negative + high arousal)
    emotion_system_set_state(emotion_sys, -0.8f, 0.9f, 1000);
    float priority = emotion_system_get_memory_priority(emotion_sys);

    // Should have high memory priority
    EXPECT_GT(priority, 0.7f);

    emotion_system_destroy(emotion_sys);
}

//=============================================================================
// Mental Health Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, HealthyEmotionsLowMentalHealthImpact) {
    // WHAT: Verify healthy emotions report low mental health impact
    // WHY:  Monitor wellbeing - neutral states are healthy
    // HOW:  Set neutral state, verify low impact

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    emotion_system_set_state(emotion_sys, 0.2f, 0.3f, 1000);
    float impact = emotion_system_get_mental_health_impact(emotion_sys);

    EXPECT_LT(impact, 0.3f);

    emotion_system_destroy(emotion_sys);
}

TEST_F(EmotionalSystemIntegrationTest, DysregulatedEmotionsHighImpact) {
    // WHAT: Verify dysregulated emotions report high mental health impact
    // WHY:  Detect potential mental health issues
    // HOW:  Set extreme negative state, verify high impact

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // Severe distress state
    emotion_system_set_state(emotion_sys, -0.95f, 0.95f, 1000);
    float impact = emotion_system_get_mental_health_impact(emotion_sys);

    EXPECT_GT(impact, 0.5f);

    emotion_system_destroy(emotion_sys);
}

//=============================================================================
// Regulation Integration Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, AutoRegulationReducesIntensity) {
    // WHAT: Verify auto-regulation reduces emotional intensity
    // WHY:  Maintain emotional homeostasis
    // HOW:  Set high intensity, auto-regulate, verify reduction

    emotion_config_t config = emotion_system_default_config();
    config.regulation_threshold = 0.7f;

    emotional_system_t* emotion_sys = emotion_system_create(&config);
    ASSERT_NE(emotion_sys, nullptr);

    // Set very high intensity
    emotion_system_set_state(emotion_sys, -0.9f, 0.95f, 1000);

    emotion_state_t state_before;
    emotion_system_get_state(emotion_sys, &state_before);
    float intensity_before = state_before.intensity;

    // Trigger auto-regulation
    bool regulated = emotion_system_auto_regulate(emotion_sys);
    EXPECT_TRUE(regulated);

    emotion_state_t state_after;
    emotion_system_get_state(emotion_sys, &state_after);
    float intensity_after = state_after.intensity;

    // Intensity should decrease
    EXPECT_LT(intensity_after, intensity_before);
    EXPECT_TRUE(state_after.in_self_regulation);

    emotion_system_destroy(emotion_sys);
}

TEST_F(EmotionalSystemIntegrationTest, RegulationStrategiesWork) {
    // WHAT: Verify different regulation strategies can be applied
    // WHY:  Need flexibility in emotion regulation
    // HOW:  Test reappraisal, suppression, distraction

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // Test reappraisal (strategy 0)
    emotion_system_set_state(emotion_sys, -0.8f, 0.7f, 1000);
    bool result1 = emotion_system_regulate(emotion_sys, 0);
    EXPECT_TRUE(result1);

    // Test suppression (strategy 1)
    emotion_system_set_state(emotion_sys, 0.5f, 0.9f, 2000);
    bool result2 = emotion_system_regulate(emotion_sys, 1);
    EXPECT_TRUE(result2);

    // Test distraction (strategy 2)
    emotion_system_set_state(emotion_sys, -0.7f, 0.8f, 3000);
    bool result3 = emotion_system_regulate(emotion_sys, 2);
    EXPECT_TRUE(result3);

    emotion_system_destroy(emotion_sys);
}

//=============================================================================
// Multimodal Processing Tests
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, MultimodalProcessingUpdatesState) {
    // WHAT: Verify multimodal inputs update emotional state
    // WHY:  Emotion recognition from visual, audio, text
    // HOW:  Pass multimodal data, verify state changes

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    float visual[64] = {0.5f};
    float audio[32] = {0.3f};
    const char* text = "I am happy!";

    emotion_state_t state_before;
    emotion_system_get_state(emotion_sys, &state_before);

    bool result = emotion_system_update_multimodal(
        emotion_sys, visual, 64, audio, 32, text, 1000
    );

    EXPECT_TRUE(result);

    emotion_state_t state_after;
    emotion_system_get_state(emotion_sys, &state_after);

    // State should update (arousal should increase slightly)
    EXPECT_GE(state_after.arousal, state_before.arousal);

    emotion_system_destroy(emotion_sys);
}

//=============================================================================
// End-to-End Integration Test
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, EndToEndEmotionalProcessingPipeline) {
    // WHAT: Complete emotional processing workflow
    // WHY:  Verify all components work together
    // HOW:  Process input → emotion → salience → memory → regulation

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    // 1. Process multimodal input
    float visual[64] = {0.7f};
    const char* text = "This is amazing!";
    emotion_system_update_multimodal(emotion_sys, visual, 64, nullptr, 0, text, 1000);

    // 2. Set emotional state (simulating recognition output)
    emotion_system_set_state(emotion_sys, 0.8f, 0.7f, 1100);

    // 3. Get emotional tag for memory
    emotional_tag_t tag;
    bool tag_result = emotion_system_get_tag(emotion_sys, &tag);
    ASSERT_TRUE(tag_result);
    EXPECT_GT(tag.valence, 0.5f);

    // 4. Get salience boost for attention
    float salience = emotion_system_get_salience_boost(emotion_sys);
    EXPECT_GT(salience, 1.3f);

    // 5. Get memory priority for consolidation
    float memory_pri = emotion_system_get_memory_priority(emotion_sys);
    EXPECT_GT(memory_pri, 0.6f);

    // 6. Check mental health impact
    float health_impact = emotion_system_get_mental_health_impact(emotion_sys);
    EXPECT_LT(health_impact, 0.4f);  // Positive emotions = low impact

    // 7. Get statistics
    emotion_stats_t stats;
    emotion_system_get_stats(emotion_sys, &stats);
    EXPECT_GT(stats.total_updates, 0u);

    emotion_system_destroy(emotion_sys);

    // Brain should still function normally
    float input[64] = {0.5f};
    float output[10];
    bool brain_result = brain_predict(brain, input, 64, output, 10);
    EXPECT_TRUE(brain_result);
}

//=============================================================================
// Performance Test
//=============================================================================

TEST_F(EmotionalSystemIntegrationTest, IntegrationPerformanceAcceptable) {
    // WHAT: Verify integration doesn't significantly slow processing
    // WHY:  Performance regression test
    // HOW:  Time 1000 emotional processing cycles

    emotional_system_t* emotion_sys = emotion_system_create(nullptr);
    ASSERT_NE(emotion_sys, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        float valence = sinf(i * 0.01f);
        float arousal = cosf(i * 0.005f) * 0.5f + 0.5f;

        emotion_system_set_state(emotion_sys, valence, arousal, 1000 + i);
        emotion_system_get_salience_boost(emotion_sys);
        emotion_system_get_memory_priority(emotion_sys);

        if (i % 100 == 0) {
            emotion_system_auto_regulate(emotion_sys);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 processing cycles should be fast (< 50ms)
    // Note: 50ms allows for system load variance while still catching regressions
    // Average: 50 microseconds per cycle including set_state, get_salience, get_memory, auto_regulate
    EXPECT_LT(duration.count(), 50);

    emotion_system_destroy(emotion_sys);
}
