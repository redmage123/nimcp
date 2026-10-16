//=============================================================================
// test_emotion_tensor_regression.cpp - Regression Tests for Emotion Tensor
//=============================================================================
// WHAT: Regression tests ensuring emotion tensor maintains backward compatibility
// WHY:  Prevent breaking changes, ensure stable API
// HOW:  Test API consistency, data structure compatibility, performance baselines
//
// Test Coverage:
// - API backward compatibility
// - Data structure size/layout stability
// - Performance regression detection
// - Memory safety regression
// - Configuration default stability
// - Compound emotion formula stability
// - Valence/arousal conversion stability
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_emotion_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionTensorRegressionTest : public ::testing::Test {
protected:
    emotion_tensor_system_t* system = nullptr;

    void TearDown() override {
        if (system) {
            emotion_tensor_destroy(system);
        }
    }
};

//=============================================================================
// API Backward Compatibility Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, DefaultConfigValuesStable) {
    // WHAT: Verify default configuration hasn't changed
    // WHY:  Users rely on specific default behavior
    // HOW:  Check all default config values

    emotion_tensor_config_t config = emotion_tensor_default_config();

    // Decay rate should be reasonable
    EXPECT_GE(config.decay_rate, 0.01f);
    EXPECT_LE(config.decay_rate, 0.5f);

    // Interaction strength should be reasonable
    EXPECT_GE(config.interaction_strength, 0.0f);
    EXPECT_LE(config.interaction_strength, 1.0f);

    // Blend threshold should allow compound detection
    EXPECT_GE(config.blend_threshold, 0.05f);
    EXPECT_LE(config.blend_threshold, 0.5f);

    // Dominance threshold should be reasonable
    EXPECT_GE(config.dominance_threshold, 0.2f);
    EXPECT_LE(config.dominance_threshold, 0.8f);

    // Features should be enabled by default
    EXPECT_TRUE(config.enable_temporal_dynamics);
    EXPECT_TRUE(config.enable_appraisals);
    EXPECT_TRUE(config.enable_interactions);
}

TEST_F(EmotionTensorRegressionTest, CreateDestroyApiStable) {
    // WHAT: Verify create/destroy API hasn't changed
    // WHY:  Core lifecycle API must remain stable
    // HOW:  Test create with NULL and custom config

    // Create with NULL config (default)
    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);
    emotion_tensor_destroy(system);
    system = nullptr;

    // Create with custom config
    emotion_tensor_config_t config = emotion_tensor_default_config();
    system = emotion_tensor_create(&config);
    ASSERT_NE(system, nullptr);
    emotion_tensor_destroy(system);
    system = nullptr;

    // Destroy NULL is safe
    emotion_tensor_destroy(nullptr);
}

TEST_F(EmotionTensorRegressionTest, StateQueryApiStable) {
    // WHAT: Verify state query API signatures haven't changed
    // WHY:  Existing code relies on these functions
    // HOW:  Call all query functions, verify behavior

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // get tensor API
    emotion_tensor_t tensor;
    bool result1 = emotion_tensor_get(system, &tensor);
    EXPECT_TRUE(result1);

    // get_channel API
    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_GE(joy, 0.0f);

    // get_compound API
    float love = emotion_tensor_get_compound(system, COMPOUND_LOVE);
    EXPECT_GE(love, 0.0f);

    // is_contradictory API
    bool contradictory = emotion_tensor_is_contradictory(system, 0.5f);
    EXPECT_FALSE(contradictory);  // Initially neutral

    // get_valence API
    float valence = emotion_tensor_get_valence(system);
    EXPECT_GE(valence, -1.0f);
    EXPECT_LE(valence, 1.0f);

    // get_arousal API
    float arousal = emotion_tensor_get_arousal(system);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

TEST_F(EmotionTensorRegressionTest, UpdateApiStable) {
    // WHAT: Verify update API signatures stable
    // WHY:  Critical for emotion processing
    // HOW:  Test set_channel, set_channels, update, apply_stimulus

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // set_channel API
    bool result1 = emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 1000);
    EXPECT_TRUE(result1);

    // set_channels API
    float activations[EMOTION_TENSOR_PRIMARY_COUNT] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    bool result2 = emotion_tensor_set_channels(system, activations, 2000);
    EXPECT_TRUE(result2);

    // update API
    bool result3 = emotion_tensor_update(system, 0.1f, 3000);
    EXPECT_TRUE(result3);

    // apply_stimulus API
    bool result4 = emotion_tensor_apply_stimulus(system, TENSOR_FEAR, 0.5f, false, 4000);
    EXPECT_TRUE(result4);

    // set_appraisal API
    bool result5 = emotion_tensor_set_appraisal(system, TENSOR_JOY, APPRAISAL_CERTAINTY, 0.8f);
    EXPECT_TRUE(result5);
}

TEST_F(EmotionTensorRegressionTest, DynamicsApiStable) {
    // WHAT: Verify dynamics API stable
    // WHY:  Temporal dynamics are core feature
    // HOW:  Test compute_compounds, apply_interactions, reset

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.4f, 1000);

    // compute_compounds API
    bool result1 = emotion_tensor_compute_compounds(system);
    EXPECT_TRUE(result1);

    // apply_interactions API
    bool result2 = emotion_tensor_apply_interactions(system, 0.1f);
    EXPECT_TRUE(result2);

    // reset API
    bool result3 = emotion_tensor_reset(system);
    EXPECT_TRUE(result3);

    float joy = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_FLOAT_EQ(joy, 0.0f);
}

TEST_F(EmotionTensorRegressionTest, AnalysisApiStable) {
    // WHAT: Verify analysis API stable
    // WHY:  Analysis functions used for monitoring
    // HOW:  Test get_entropy, get_stability, get_dominant

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_tensor_set_channel(system, TENSOR_JOY, 0.7f, 1000);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.3f, 1000);

    // get_entropy API
    float entropy = emotion_tensor_get_entropy(system);
    EXPECT_GE(entropy, 0.0f);
    EXPECT_LE(entropy, 1.0f);

    // get_stability API
    float stability = emotion_tensor_get_stability(system);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);

    // get_dominant API
    emotion_primary_t primary, secondary;
    float blend;
    bool result = emotion_tensor_get_dominant(system, &primary, &secondary, &blend);
    EXPECT_TRUE(result);
    EXPECT_GE(primary, TENSOR_JOY);
    EXPECT_LT(primary, TENSOR_PRIMARY_COUNT);
}

TEST_F(EmotionTensorRegressionTest, UtilityApiStable) {
    // WHAT: Verify utility API stable
    // WHY:  Name functions used for logging/display
    // HOW:  Test emotion_name, compound_name, init_interaction_matrix

    // emotion_name API
    const char* joy_name = emotion_tensor_emotion_name(TENSOR_JOY);
    EXPECT_NE(joy_name, nullptr);
    EXPECT_STREQ(joy_name, "joy");

    const char* fear_name = emotion_tensor_emotion_name(TENSOR_FEAR);
    EXPECT_NE(fear_name, nullptr);
    EXPECT_STREQ(fear_name, "fear");

    // compound_name API
    const char* love_name = emotion_tensor_compound_name(COMPOUND_LOVE);
    EXPECT_NE(love_name, nullptr);
    EXPECT_STREQ(love_name, "love");

    const char* bittersweet_name = emotion_tensor_compound_name(COMPOUND_BITTERSWEETNESS);
    EXPECT_NE(bittersweet_name, nullptr);
    EXPECT_STREQ(bittersweet_name, "bittersweetness");

    // init_interaction_matrix API
    emotion_interaction_matrix_t matrix;
    emotion_tensor_init_interaction_matrix(&matrix);
    // Should not crash, values should be reasonable
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        for (int j = 0; j < EMOTION_TENSOR_PRIMARY_COUNT; j++) {
            EXPECT_GE(matrix.matrix[i][j], -1.0f);
            EXPECT_LE(matrix.matrix[i][j], 1.0f);
        }
    }
}

//=============================================================================
// Data Structure Compatibility Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, EmotionTensorStructureStable) {
    // WHAT: Verify emotion_tensor_t structure layout stable
    // WHY:  Binary compatibility for serialization
    // HOW:  Check expected fields exist and are accessible

    emotion_tensor_t tensor = {0};

    // All fields must be accessible (compile-time check)
    tensor.channels[TENSOR_JOY] = 0.5f;
    tensor.appraisals[TENSOR_JOY][APPRAISAL_CERTAINTY] = 0.8f;
    tensor.dynamics[TENSOR_JOY][0] = 0.3f;
    tensor.dynamics_index = 5;
    tensor.compounds[COMPOUND_LOVE] = 0.4f;
    tensor.primary_emotion = TENSOR_JOY;
    tensor.secondary_emotion = TENSOR_TRUST;
    tensor.primary_strength = 0.7f;
    tensor.secondary_strength = 0.4f;
    tensor.blend_ratio = 0.5f;
    tensor.overall_valence = 0.6f;
    tensor.overall_arousal = 0.5f;
    tensor.emotional_entropy = 0.3f;
    tensor.stability = 0.9f;
    tensor.last_update_ms = 1000;

    // Verify values stored correctly
    EXPECT_FLOAT_EQ(tensor.channels[TENSOR_JOY], 0.5f);
    EXPECT_FLOAT_EQ(tensor.appraisals[TENSOR_JOY][APPRAISAL_CERTAINTY], 0.8f);
    EXPECT_EQ(tensor.primary_emotion, TENSOR_JOY);
    EXPECT_EQ(tensor.dynamics_index, 5u);
}

TEST_F(EmotionTensorRegressionTest, ConfigStructureStable) {
    // WHAT: Verify emotion_tensor_config_t structure stable
    // WHY:  Configuration compatibility
    // HOW:  Check all fields accessible

    emotion_tensor_config_t config = {0};

    config.decay_rate = 0.1f;
    config.interaction_strength = 0.5f;
    config.blend_threshold = 0.2f;
    config.dominance_threshold = 0.4f;
    config.enable_temporal_dynamics = true;
    config.enable_appraisals = true;
    config.enable_interactions = true;

    EXPECT_FLOAT_EQ(config.decay_rate, 0.1f);
    EXPECT_FLOAT_EQ(config.interaction_strength, 0.5f);
    EXPECT_TRUE(config.enable_temporal_dynamics);
}

TEST_F(EmotionTensorRegressionTest, ConstantsStable) {
    // WHAT: Verify constants haven't changed
    // WHY:  Code depends on these values
    // HOW:  Check constant values

    EXPECT_EQ(EMOTION_TENSOR_PRIMARY_COUNT, 8);
    EXPECT_EQ(EMOTION_TENSOR_COMPOUND_COUNT, 24);
    EXPECT_EQ(EMOTION_TENSOR_TEMPORAL_WINDOW, 16);
    EXPECT_EQ(EMOTION_TENSOR_MAX_BLEND_DEPTH, 3);
    EXPECT_EQ(TENSOR_PRIMARY_COUNT, 8);
    EXPECT_EQ(COMPOUND_COUNT, 24);
    EXPECT_EQ(APPRAISAL_COUNT, 6);
}

TEST_F(EmotionTensorRegressionTest, EnumValuesStable) {
    // WHAT: Verify enum values haven't changed
    // WHY:  Code may depend on specific values
    // HOW:  Check enum indices

    // Primary emotions (Plutchik's wheel order)
    EXPECT_EQ(TENSOR_JOY, 0);
    EXPECT_EQ(TENSOR_TRUST, 1);
    EXPECT_EQ(TENSOR_FEAR, 2);
    EXPECT_EQ(TENSOR_SURPRISE, 3);
    EXPECT_EQ(TENSOR_SADNESS, 4);
    EXPECT_EQ(TENSOR_DISGUST, 5);
    EXPECT_EQ(TENSOR_ANGER, 6);
    EXPECT_EQ(TENSOR_ANTICIPATION, 7);

    // Compound emotions (critical ones)
    EXPECT_EQ(COMPOUND_LOVE, 0);
    EXPECT_EQ(COMPOUND_AWE, 2);
    EXPECT_EQ(COMPOUND_OPTIMISM, 7);
    EXPECT_EQ(COMPOUND_BITTERSWEETNESS, 16);
    EXPECT_EQ(COMPOUND_NOSTALGIA, 23);

    // Appraisals
    EXPECT_EQ(APPRAISAL_CERTAINTY, 0);
    EXPECT_EQ(APPRAISAL_CONTROL, 1);
    EXPECT_EQ(APPRAISAL_RELEVANCE, 2);
    EXPECT_EQ(APPRAISAL_PLEASANTNESS, 3);
    EXPECT_EQ(APPRAISAL_NOVELTY, 4);
    EXPECT_EQ(APPRAISAL_GOAL_CONDUCIVE, 5);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, ChannelRangeEnforced) {
    // WHAT: Verify channels stay in [0, 1]
    // WHY:  Prevent out-of-range values
    // HOW:  Try to set extreme values, verify clamping

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Try setting beyond upper bound
    emotion_tensor_set_channel(system, TENSOR_JOY, 5.0f, 1000);
    float joy1 = emotion_tensor_get_channel(system, TENSOR_JOY);
    EXPECT_LE(joy1, 1.0f);

    // Try setting below lower bound
    emotion_tensor_set_channel(system, TENSOR_FEAR, -5.0f, 2000);
    float fear1 = emotion_tensor_get_channel(system, TENSOR_FEAR);
    EXPECT_GE(fear1, 0.0f);
}

TEST_F(EmotionTensorRegressionTest, ValenceRangeEnforced) {
    // WHAT: Verify valence stays in [-1, +1]
    // WHY:  Backward compatibility with scalar emotion system
    // HOW:  Set extreme channels, verify valence bounded

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Max positive emotions
    emotion_tensor_set_channel(system, TENSOR_JOY, 1.0f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 1.0f, 1000);
    emotion_tensor_set_channel(system, TENSOR_ANTICIPATION, 1.0f, 1000);

    float valence = emotion_tensor_get_valence(system);
    EXPECT_LE(valence, 1.0f);

    // Reset and max negative
    emotion_tensor_reset(system);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 1.0f, 2000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 1.0f, 2000);
    emotion_tensor_set_channel(system, TENSOR_ANGER, 1.0f, 2000);

    valence = emotion_tensor_get_valence(system);
    EXPECT_GE(valence, -1.0f);
}

TEST_F(EmotionTensorRegressionTest, ArousalRangeEnforced) {
    // WHAT: Verify arousal stays in [0, 1]
    // WHY:  Backward compatibility with scalar emotion system
    // HOW:  Set max channels, verify arousal bounded

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set all channels to max
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        emotion_tensor_set_channel(system, static_cast<emotion_primary_t>(i), 1.0f, 1000);
    }

    float arousal = emotion_tensor_get_arousal(system);
    EXPECT_LE(arousal, 1.0f);
    EXPECT_GE(arousal, 0.0f);
}

TEST_F(EmotionTensorRegressionTest, CompoundEmotionFormulaStable) {
    // WHAT: Verify compound emotion computation formulas stable
    // WHY:  Behavioral consistency for emotion detection
    // HOW:  Set known inputs, verify expected outputs

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Primary dyad: Love = Joy + Trust (geometric mean)
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.8f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.5f, 1000);

    float love = emotion_tensor_get_compound(system, COMPOUND_LOVE);
    float expected_love = sqrtf(0.8f * 0.5f);  // geometric mean
    EXPECT_NEAR(love, expected_love, 0.01f);

    // Tertiary dyad: Bittersweet = Joy * Sadness (product)
    emotion_tensor_reset(system);
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.6f, 2000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.5f, 2000);

    float bittersweet = emotion_tensor_get_compound(system, COMPOUND_BITTERSWEETNESS);
    float expected_bittersweet = 0.6f * 0.5f;
    EXPECT_NEAR(bittersweet, expected_bittersweet, 0.01f);
}

TEST_F(EmotionTensorRegressionTest, DecayBehaviorStable) {
    // WHAT: Verify decay behavior consistent
    // WHY:  Decay rate affects system dynamics
    // HOW:  Apply decay, verify expected reduction

    emotion_tensor_config_t config = emotion_tensor_default_config();
    config.decay_rate = 0.1f;  // Known decay rate
    system = emotion_tensor_create(&config);
    ASSERT_NE(system, nullptr);

    emotion_tensor_set_channel(system, TENSOR_ANGER, 1.0f, 1000);
    float initial = emotion_tensor_get_channel(system, TENSOR_ANGER);

    // Apply 1 second of decay
    emotion_tensor_update(system, 1.0f, 2000);

    float after_decay = emotion_tensor_get_channel(system, TENSOR_ANGER);

    // Should decay by approximately decay_rate * delta_time
    EXPECT_LT(after_decay, initial);
    EXPECT_GT(after_decay, 0.5f);  // Shouldn't decay completely
}

TEST_F(EmotionTensorRegressionTest, ResetBehaviorStable) {
    // WHAT: Verify reset clears all state
    // WHY:  Reset should return to baseline
    // HOW:  Set state, reset, verify zeroed

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set some state
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        emotion_tensor_set_channel(system, static_cast<emotion_primary_t>(i), 0.5f, 1000);
    }

    // Reset
    emotion_tensor_reset(system);

    // Verify all zeroed
    for (int i = 0; i < EMOTION_TENSOR_PRIMARY_COUNT; i++) {
        float val = emotion_tensor_get_channel(system, static_cast<emotion_primary_t>(i));
        EXPECT_FLOAT_EQ(val, 0.0f);
    }

    // Compounds should be zero
    for (int i = 0; i < EMOTION_TENSOR_COMPOUND_COUNT; i++) {
        float val = emotion_tensor_get_compound(system, static_cast<emotion_compound_t>(i));
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, CreationPerformanceBaseline) {
    // WHAT: Verify system creation time acceptable
    // WHY:  Detect performance regressions
    // HOW:  Time 1000 create/destroy cycles

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        emotion_tensor_system_t* temp = emotion_tensor_create(nullptr);
        emotion_tensor_destroy(temp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 create/destroy should be < 200ms (relaxed for CI variance)
    EXPECT_LT(duration.count(), 200);
}

TEST_F(EmotionTensorRegressionTest, SetChannelPerformanceBaseline) {
    // WHAT: Verify set_channel performance
    // WHY:  Detect performance regressions
    // HOW:  Time 100000 set_channel calls

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; i++) {
        emotion_primary_t emotion = static_cast<emotion_primary_t>(i % EMOTION_TENSOR_PRIMARY_COUNT);
        float value = (i % 100) / 100.0f;
        emotion_tensor_set_channel(system, emotion, value, 1000 + i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 100000 set_channel calls should be < 200ms (relaxed for CI variance)
    EXPECT_LT(duration.count(), 200);
}

TEST_F(EmotionTensorRegressionTest, CompoundComputationPerformanceBaseline) {
    // WHAT: Verify compound computation performance
    // WHY:  Detect performance regressions
    // HOW:  Time 10000 compound computations

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Set up some emotions
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.5f, 1000);
    emotion_tensor_set_channel(system, TENSOR_TRUST, 0.4f, 1000);
    emotion_tensor_set_channel(system, TENSOR_SADNESS, 0.3f, 1000);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        emotion_tensor_compute_compounds(system);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10000 compound computations should be < 50ms
    EXPECT_LT(duration.count(), 50);
}

TEST_F(EmotionTensorRegressionTest, FullUpdatePerformanceBaseline) {
    // WHAT: Verify full update cycle performance
    // WHY:  Detect performance regressions
    // HOW:  Time 10000 update cycles

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        emotion_tensor_set_channel(system, static_cast<emotion_primary_t>(i % 8), 0.5f, 1000 + i);
        emotion_tensor_update(system, 0.01f, 1000 + i);
        emotion_tensor_get_valence(system);
        emotion_tensor_get_arousal(system);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10000 full update cycles should be < 200ms (relaxed for CI variance)
    EXPECT_LT(duration.count(), 200);
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, NoMemoryLeaksOnRepeatedUse) {
    // WHAT: Verify no memory leaks with repeated use
    // WHY:  Prevent memory leaks
    // HOW:  Create/destroy 1000 times with extensive use

    for (int iter = 0; iter < 1000; iter++) {
        emotion_tensor_system_t* temp = emotion_tensor_create(nullptr);
        ASSERT_NE(temp, nullptr);

        // Use extensively
        for (int i = 0; i < 10; i++) {
            emotion_tensor_set_channel(temp, static_cast<emotion_primary_t>(i % 8), 0.5f, 1000 + i);
            emotion_tensor_update(temp, 0.1f, 1000 + i);
            emotion_tensor_compute_compounds(temp);
        }

        emotion_tensor_t tensor;
        emotion_tensor_get(temp, &tensor);
        emotion_tensor_get_entropy(temp);
        emotion_tensor_get_stability(temp);

        emotion_tensor_destroy(temp);
    }

    // If we get here without crashes/ASAN errors, no obvious leaks
    SUCCEED();
}

TEST_F(EmotionTensorRegressionTest, NullInputsSafetyMaintained) {
    // WHAT: Verify NULL inputs handled safely (no crashes)
    // WHY:  Defensive programming regression
    // HOW:  Pass NULL to all functions

    // All these should return false/0/-1 safely, not crash
    EXPECT_FALSE(emotion_tensor_get(nullptr, nullptr));
    EXPECT_FLOAT_EQ(emotion_tensor_get_channel(nullptr, TENSOR_JOY), -1.0f);
    EXPECT_FLOAT_EQ(emotion_tensor_get_compound(nullptr, COMPOUND_LOVE), -1.0f);
    EXPECT_FALSE(emotion_tensor_is_contradictory(nullptr, 0.5f));
    EXPECT_FLOAT_EQ(emotion_tensor_get_valence(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(emotion_tensor_get_arousal(nullptr), 0.0f);
    EXPECT_FALSE(emotion_tensor_set_channel(nullptr, TENSOR_JOY, 0.5f, 1000));
    EXPECT_FALSE(emotion_tensor_set_channels(nullptr, nullptr, 1000));
    EXPECT_FALSE(emotion_tensor_set_appraisal(nullptr, TENSOR_JOY, APPRAISAL_CERTAINTY, 0.5f));
    EXPECT_FALSE(emotion_tensor_apply_stimulus(nullptr, TENSOR_JOY, 0.5f, true, 1000));
    EXPECT_FALSE(emotion_tensor_update(nullptr, 0.1f, 1000));
    EXPECT_FALSE(emotion_tensor_compute_compounds(nullptr));
    EXPECT_FALSE(emotion_tensor_apply_interactions(nullptr, 0.1f));
    EXPECT_FALSE(emotion_tensor_reset(nullptr));
    EXPECT_FLOAT_EQ(emotion_tensor_get_entropy(nullptr), -1.0f);
    EXPECT_FLOAT_EQ(emotion_tensor_get_stability(nullptr), -1.0f);
    EXPECT_FALSE(emotion_tensor_get_dominant(nullptr, nullptr, nullptr, nullptr));

    emotion_tensor_destroy(nullptr);  // Should not crash

    // Verify name functions handle invalid input
    const char* invalid_name = emotion_tensor_emotion_name(static_cast<emotion_primary_t>(100));
    EXPECT_STREQ(invalid_name, "unknown");

    const char* invalid_compound = emotion_tensor_compound_name(static_cast<emotion_compound_t>(100));
    EXPECT_STREQ(invalid_compound, "unknown");

    SUCCEED();
}

TEST_F(EmotionTensorRegressionTest, InvalidEnumHandling) {
    // WHAT: Verify invalid enum values handled safely
    // WHY:  Prevent crashes from bad input
    // HOW:  Pass invalid enum values

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Invalid emotion index
    bool result1 = emotion_tensor_set_channel(system, static_cast<emotion_primary_t>(100), 0.5f, 1000);
    EXPECT_FALSE(result1);

    float val1 = emotion_tensor_get_channel(system, static_cast<emotion_primary_t>(100));
    EXPECT_FLOAT_EQ(val1, -1.0f);

    // Invalid compound index
    float val2 = emotion_tensor_get_compound(system, static_cast<emotion_compound_t>(100));
    EXPECT_FLOAT_EQ(val2, -1.0f);

    // Invalid appraisal
    bool result2 = emotion_tensor_set_appraisal(system, TENSOR_JOY, static_cast<appraisal_dimension_t>(100), 0.5f);
    EXPECT_FALSE(result2);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(EmotionTensorRegressionTest, ValenceArousalBackwardCompatibility) {
    // WHAT: Verify valence/arousal compatible with scalar emotion system
    // WHY:  Existing code uses scalar valence/arousal
    // HOW:  Set tensor state, verify valence/arousal match expected patterns

    system = emotion_tensor_create(nullptr);
    ASSERT_NE(system, nullptr);

    // High joy -> positive valence, moderate arousal
    emotion_tensor_set_channel(system, TENSOR_JOY, 0.9f, 1000);
    float valence1 = emotion_tensor_get_valence(system);
    float arousal1 = emotion_tensor_get_arousal(system);
    EXPECT_GT(valence1, 0.5f);
    EXPECT_GT(arousal1, 0.0f);

    // High fear -> negative valence, high arousal
    emotion_tensor_reset(system);
    emotion_tensor_set_channel(system, TENSOR_FEAR, 0.9f, 2000);
    float valence2 = emotion_tensor_get_valence(system);
    float arousal2 = emotion_tensor_get_arousal(system);
    EXPECT_LT(valence2, 0.0f);
    EXPECT_GT(arousal2, 0.0f);

    // Neutral -> near-zero valence, low arousal
    emotion_tensor_reset(system);
    float valence3 = emotion_tensor_get_valence(system);
    float arousal3 = emotion_tensor_get_arousal(system);
    EXPECT_NEAR(valence3, 0.0f, 0.01f);
    EXPECT_NEAR(arousal3, 0.0f, 0.01f);
}
