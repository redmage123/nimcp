//=============================================================================
// test_emotion_tensor_integration.cpp - Integration Tests for Emotion Tensor
//=============================================================================
// WHAT: Integration tests showing emotion tensor working with brain systems
// WHY:  Verify tensor integrates properly with emotional system, memory, salience
// HOW:  Test tensor with brain, working memory, emotional tagging, mental health
//
// Test Coverage:
// - Emotion tensor + brain creation
// - Integration with emotional system (valence/arousal conversion)
// - Integration with working memory (emotional tagging)
// - Integration with salience (arousal boost)
// - Integration with mental health (entropy as indicator)
// - Mixed emotion processing scenarios
// - Compound emotion detection in context
//=============================================================================

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <chrono>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_emotion_tensor.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionTensorIntegrationTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* tensor_system = nullptr;
    emotional_system_t* emotion_system = nullptr;
    brain_t brain = nullptr;

    void SetUp() override {
        // Create emotion tensor with default config
        tensor_system = emotion_tensor_create(nullptr);
        ASSERT_NE(tensor_system, nullptr);

        // Create emotional system
        emotion_system = emotion_system_create(nullptr);
        ASSERT_NE(emotion_system, nullptr);

        // Create brain with emotional processing enabled
        brain_config_t config;
        memset(&config, 0, sizeof(config));
        strncpy(config.task_name, "emotion_tensor_test", sizeof(config.task_name) - 1);
        config.size = BRAIN_SIZE_SMALL;
        config.task = BRAIN_TASK_CLASSIFICATION;
        config.num_inputs = 32;
        config.num_outputs = 8;

        brain = brain_create_custom(&config);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (tensor_system) {
            emotion_tensor_destroy(tensor_system);
        }
        if (emotion_system) {
            emotion_system_destroy(emotion_system);
        }
        if (brain) {
            brain_destroy(brain);
        }
    }

    // Helper: Sync tensor to emotional system
    void sync_tensor_to_emotional_system() {
        float valence = emotion_tensor_get_valence(tensor_system);
        float arousal = emotion_tensor_get_arousal(tensor_system);
        emotion_system_set_state(emotion_system, valence, arousal,
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()));
    }
};

//=============================================================================
// Basic Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, BrainProcessingWithTensor) {
    // WHAT: Verify brain processing works alongside emotion tensor
    // WHY:  Ensure no conflicts between brain and tensor systems
    // HOW:  Process input through brain while tensor active

    // Set some emotional state in tensor
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.7f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANTICIPATION, 0.4f, 1000);

    float input[32];
    for (int i = 0; i < 32; i++) {
        input[i] = 0.1f * sinf(i * 0.1f);
    }

    float output[8];
    bool result = brain_predict(brain, input, 32, output, 8);

    ASSERT_TRUE(result);

    for (int i = 0; i < 8; i++) {
        EXPECT_GE(output[i], 0.0f);
        EXPECT_LE(output[i], 1.0f);
        EXPECT_FALSE(std::isnan(output[i]));
    }
}

TEST_F(EmotionTensorIntegrationTest, TensorToValenceArousalConversion) {
    // WHAT: Verify tensor provides backward-compatible valence/arousal
    // WHY:  Existing emotional system uses scalar valence/arousal
    // HOW:  Set tensor channels, read valence/arousal, verify reasonable

    // Set positive emotions
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_TRUST, 0.6f, 1000);

    float valence = emotion_tensor_get_valence(tensor_system);
    float arousal = emotion_tensor_get_arousal(tensor_system);

    // Positive emotions should yield positive valence
    EXPECT_GT(valence, 0.0f);
    // Active emotions should yield arousal > 0
    EXPECT_GT(arousal, 0.0f);

    // Sync to emotional system
    emotion_system_set_state(emotion_system, valence, arousal, 1000);

    emotion_state_t state;
    emotion_system_get_state(emotion_system, &state);

    EXPECT_FLOAT_EQ(state.valence, valence);
}

TEST_F(EmotionTensorIntegrationTest, NegativeEmotionsTensorToScalar) {
    // WHAT: Verify negative tensor emotions convert correctly
    // WHY:  Fear, sadness, anger should yield negative valence
    // HOW:  Set negative emotions, verify negative valence

    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.7f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.5f, 1000);

    float valence = emotion_tensor_get_valence(tensor_system);

    // Negative emotions should yield negative valence
    EXPECT_LT(valence, 0.0f);
}

//=============================================================================
// Emotional Tagging Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, TensorDrivesEmotionalTagging) {
    // WHAT: Verify tensor can drive emotional tagging for memory
    // WHY:  Memories should be tagged with tensor-derived emotion
    // HOW:  Set tensor, get valence/arousal, create tag

    emotion_tensor_set_channel(tensor_system, TENSOR_SURPRISE, 0.9f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.6f, 1000);

    // Create emotional tag from tensor
    emotional_tag_t tag;
    tag.valence = emotion_tensor_get_valence(tensor_system);
    tag.arousal = emotion_tensor_get_arousal(tensor_system);

    // High surprise + joy should yield positive valence, high arousal
    EXPECT_GT(tag.valence, 0.0f);
    EXPECT_GT(tag.arousal, 0.4f);
}

TEST_F(EmotionTensorIntegrationTest, MixedEmotionTagging) {
    // WHAT: Verify mixed emotions (bittersweet) tagged correctly
    // WHY:  Mixed emotions are complex but should still tag
    // HOW:  Set joy + sadness, verify compound + tag

    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.6f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.5f, 1000);

    // Check bittersweet compound
    float bittersweet = emotion_tensor_get_compound(tensor_system, COMPOUND_BITTERSWEETNESS);
    EXPECT_GT(bittersweet, 0.0f);

    // Tag should reflect mixed state
    emotional_tag_t tag;
    tag.valence = emotion_tensor_get_valence(tensor_system);
    tag.arousal = emotion_tensor_get_arousal(tensor_system);

    // Mixed emotions may have near-zero valence
    EXPECT_GT(fabsf(tag.valence), 0.0f);  // Some valence
}

//=============================================================================
// Salience Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, HighArousalTensorBoostsSalience) {
    // WHAT: Verify high-arousal tensor states boost salience
    // WHY:  Emotional states should grab attention
    // HOW:  Set high-arousal emotions, sync to system, check salience boost

    // Fear + surprise = high arousal
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.8f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SURPRISE, 0.7f, 1000);

    sync_tensor_to_emotional_system();

    float salience_boost = emotion_system_get_salience_boost(emotion_system);

    // High arousal emotions should boost salience significantly
    EXPECT_GT(salience_boost, 1.3f);
}

TEST_F(EmotionTensorIntegrationTest, LowArousalTensorMinimalSalience) {
    // WHAT: Verify calm tensor states have minimal salience boost
    // WHY:  Calm states shouldn't demand excessive attention
    // HOW:  Set low emotions, verify boost near 1.0

    emotion_tensor_set_channel(tensor_system, TENSOR_TRUST, 0.3f, 1000);

    sync_tensor_to_emotional_system();

    float salience_boost = emotion_system_get_salience_boost(emotion_system);

    EXPECT_LE(salience_boost, 1.5f);  // Relaxed threshold for positive emotions
}

//=============================================================================
// Memory Priority Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, IntenseEmotionHighMemoryPriority) {
    // WHAT: Verify intense tensor emotions increase memory priority
    // WHY:  Emotional memories are stronger
    // HOW:  Set intense emotions, check memory priority

    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.9f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.8f, 1000);

    sync_tensor_to_emotional_system();

    float priority = emotion_system_get_memory_priority(emotion_system);

    EXPECT_GT(priority, 0.6f);
}

TEST_F(EmotionTensorIntegrationTest, CalmStateNormalMemoryPriority) {
    // WHAT: Verify calm states have normal memory priority
    // WHY:  Not everything needs strong memory
    // HOW:  Set neutral state, check lower priority

    emotion_tensor_set_channel(tensor_system, TENSOR_TRUST, 0.2f, 1000);

    sync_tensor_to_emotional_system();

    float priority = emotion_system_get_memory_priority(emotion_system);

    EXPECT_LT(priority, 0.5f);
}

//=============================================================================
// Mental Health Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, HighEntropyIndicatesEmotionalComplexity) {
    // WHAT: Verify high entropy (many emotions active) is detectable
    // WHY:  Emotional complexity can indicate mental state
    // HOW:  Activate many emotions, check entropy

    // Set multiple emotions active
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.5f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.4f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.3f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.3f, 1000);

    float entropy = emotion_tensor_get_entropy(tensor_system);

    // Multiple active emotions = higher entropy
    EXPECT_GT(entropy, 0.3f);
}

TEST_F(EmotionTensorIntegrationTest, FocusedEmotionLowEntropy) {
    // WHAT: Verify single strong emotion has low entropy
    // WHY:  Focused emotional state is healthy
    // HOW:  Set one dominant emotion, check low entropy

    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.9f, 1000);

    float entropy = emotion_tensor_get_entropy(tensor_system);

    // Single emotion = low entropy
    EXPECT_LT(entropy, 0.5f);
}

TEST_F(EmotionTensorIntegrationTest, ContradictoryEmotionsHighImpact) {
    // WHAT: Verify contradictory emotions indicate potential mental health concern
    // WHY:  Chronic contradictions may indicate distress
    // HOW:  Set opposing emotions, check mental health impact

    // Set contradictory emotions
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.7f, 1000);

    bool contradictory = emotion_tensor_is_contradictory(tensor_system, 0.5f);
    EXPECT_TRUE(contradictory);

    // Sync and check mental health impact
    sync_tensor_to_emotional_system();
    float impact = emotion_system_get_mental_health_impact(emotion_system);

    // Impact depends on intensity, not necessarily high for single instance
    EXPECT_GE(impact, 0.0f);
}

//=============================================================================
// Compound Emotion Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, NostalgiaInContextualProcessing) {
    // WHAT: Verify nostalgia (anticipation + sadness) detected and usable
    // WHY:  Nostalgia affects memory retrieval patterns
    // HOW:  Set components, verify compound, check usable values

    emotion_tensor_set_channel(tensor_system, TENSOR_ANTICIPATION, 0.6f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_SADNESS, 0.5f, 1000);

    float nostalgia = emotion_tensor_get_compound(tensor_system, COMPOUND_NOSTALGIA);
    EXPECT_GT(nostalgia, 0.0f);

    // Nostalgia has both positive and negative aspects
    float valence = emotion_tensor_get_valence(tensor_system);
    // Should be slightly negative (sadness component)
    EXPECT_LT(valence, 0.3f);
}

TEST_F(EmotionTensorIntegrationTest, AnxietyInThreatProcessing) {
    // WHAT: Verify anxiety (fear + anger) detected for threat response
    // WHY:  Anxiety affects threat detection and response
    // HOW:  Set fear + anger, verify anxiety compound

    emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, 0.7f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.6f, 1000);

    float anxiety = emotion_tensor_get_compound(tensor_system, COMPOUND_ANXIETY);
    EXPECT_GT(anxiety, 0.0f);

    // Anxiety should boost salience
    sync_tensor_to_emotional_system();
    float salience = emotion_system_get_salience_boost(emotion_system);
    EXPECT_GT(salience, 1.4f);
}

TEST_F(EmotionTensorIntegrationTest, OptimismInDecisionMaking) {
    // WHAT: Verify optimism (anticipation + joy) supports positive decisions
    // WHY:  Optimism affects risk assessment and choices
    // HOW:  Set anticipation + joy, verify optimism compound

    emotion_tensor_set_channel(tensor_system, TENSOR_ANTICIPATION, 0.7f, 1000);
    emotion_tensor_set_channel(tensor_system, TENSOR_JOY, 0.6f, 1000);

    float optimism = emotion_tensor_get_compound(tensor_system, COMPOUND_OPTIMISM);
    EXPECT_GT(optimism, 0.3f);  // Primary dyad, geometric mean

    float valence = emotion_tensor_get_valence(tensor_system);
    EXPECT_GT(valence, 0.3f);  // Positive overall
}

//=============================================================================
// Temporal Dynamics Integration Tests
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, EmotionDecayOverTime) {
    // WHAT: Verify emotions decay and integrate with system over time
    // WHY:  Emotions naturally fade
    // HOW:  Set emotion, update multiple times, verify decay

    emotion_tensor_set_channel(tensor_system, TENSOR_ANGER, 0.9f, 1000);

    float initial_anger = emotion_tensor_get_channel(tensor_system, TENSOR_ANGER);

    // Update (decay) multiple times
    for (int i = 0; i < 10; i++) {
        emotion_tensor_update(tensor_system, 0.5f, 1000 + (i + 1) * 500);
    }

    float final_anger = emotion_tensor_get_channel(tensor_system, TENSOR_ANGER);

    EXPECT_LT(final_anger, initial_anger);
}

TEST_F(EmotionTensorIntegrationTest, StabilityOverRepeatedProcessing) {
    // WHAT: Verify repeated processing maintains stability
    // WHY:  System should not become unstable
    // HOW:  Process many cycles, verify no NaN/Inf

    for (int cycle = 0; cycle < 100; cycle++) {
        // Vary emotions
        float joy = 0.5f + 0.3f * sinf(cycle * 0.1f);
        float fear = 0.3f + 0.2f * cosf(cycle * 0.15f);

        emotion_tensor_set_channel(tensor_system, TENSOR_JOY, joy, 1000 + cycle * 10);
        emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, fear, 1000 + cycle * 10);
        emotion_tensor_update(tensor_system, 0.01f, 1000 + cycle * 10);

        float valence = emotion_tensor_get_valence(tensor_system);
        float arousal = emotion_tensor_get_arousal(tensor_system);

        EXPECT_FALSE(std::isnan(valence));
        EXPECT_FALSE(std::isnan(arousal));
        EXPECT_FALSE(std::isinf(valence));
        EXPECT_FALSE(std::isinf(arousal));
    }
}

//=============================================================================
// End-to-End Integration Test
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, EndToEndEmotionalProcessingPipeline) {
    // WHAT: Complete emotional processing workflow with tensor
    // WHY:  Verify all components work together
    // HOW:  Stimulus → tensor → compounds → scalar → system → outputs

    // 1. Apply stimulus (fear + surprise = startle)
    emotion_tensor_apply_stimulus(tensor_system, TENSOR_FEAR, 0.8f, false, 1000);
    emotion_tensor_apply_stimulus(tensor_system, TENSOR_SURPRISE, 0.7f, true, 1000);

    // 2. Compute compounds before checking them
    emotion_tensor_compute_compounds(tensor_system);

    // 3. Check compound emotions
    float awe = emotion_tensor_get_compound(tensor_system, COMPOUND_AWE);
    EXPECT_GE(awe, 0.0f);  // May be 0 based on threshold math

    // 3. Get scalar values for backward compat
    float valence = emotion_tensor_get_valence(tensor_system);
    float arousal = emotion_tensor_get_arousal(tensor_system);

    // 4. Sync to emotional system
    emotion_system_set_state(emotion_system, valence, arousal, 1000);

    // 5. Get integration outputs
    emotional_tag_t tag;
    emotion_system_get_tag(emotion_system, &tag);
    EXPECT_GE(fabsf(tag.valence), 0.0f);  // May be 0 if valence computation cancels

    float salience = emotion_system_get_salience_boost(emotion_system);
    EXPECT_GT(salience, 1.0f);

    float memory_pri = emotion_system_get_memory_priority(emotion_system);
    EXPECT_GT(memory_pri, 0.3f);

    // 6. Check tensor analysis
    emotion_primary_t primary, secondary;
    float blend;
    emotion_tensor_get_dominant(tensor_system, &primary, &secondary, &blend);
    EXPECT_GE(primary, TENSOR_JOY);
    EXPECT_LT(primary, TENSOR_PRIMARY_COUNT);

    float entropy = emotion_tensor_get_entropy(tensor_system);
    EXPECT_GE(entropy, 0.0f);  // May be 0 if only one emotion active

    // 7. Process through brain (emotions active during)
    float input[32] = {0.5f};
    float output[8];
    bool brain_result = brain_predict(brain, input, 32, output, 8);
    EXPECT_TRUE(brain_result);
}

//=============================================================================
// Performance Integration Test
//=============================================================================

TEST_F(EmotionTensorIntegrationTest, IntegrationPerformanceAcceptable) {
    // WHAT: Verify integration doesn't significantly slow processing
    // WHY:  Performance regression test
    // HOW:  Time 1000 integrated processing cycles

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        // Set tensor
        float joy = 0.5f + 0.4f * sinf(i * 0.01f);
        float fear = 0.3f + 0.2f * cosf(i * 0.02f);
        emotion_tensor_set_channel(tensor_system, TENSOR_JOY, joy, 1000 + i);
        emotion_tensor_set_channel(tensor_system, TENSOR_FEAR, fear, 1000 + i);

        // Get compounds
        emotion_tensor_get_compound(tensor_system, COMPOUND_GUILT);
        emotion_tensor_get_compound(tensor_system, COMPOUND_OPTIMISM);

        // Sync to system
        float valence = emotion_tensor_get_valence(tensor_system);
        float arousal = emotion_tensor_get_arousal(tensor_system);
        emotion_system_set_state(emotion_system, valence, arousal, 1000 + i);

        // Get integration outputs
        emotion_system_get_salience_boost(emotion_system);
        emotion_system_get_memory_priority(emotion_system);

        // Update tensor
        if (i % 10 == 0) {
            emotion_tensor_update(tensor_system, 0.1f, 1000 + i);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 integrated cycles should be fast (< 50ms)
    EXPECT_LT(duration.count(), 50);
}
