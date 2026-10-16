//=============================================================================
// test_emotional_system_regression.cpp - Regression Tests for Emotional System
//=============================================================================
// WHAT: Regression tests ensuring emotional system maintains backward compatibility
// WHY:  Prevent breaking changes, ensure stable API
// HOW:  Test API consistency, data structure compatibility, performance baselines
//
// Test Coverage:
// - API backward compatibility
// - Data structure size/layout stability
// - Performance regression detection
// - Memory safety regression
// - Configuration default stability
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>

#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalSystemRegressionTest : public ::testing::Test {
protected:
    emotional_system_t* system = nullptr;

    void TearDown() override {
        if (system) {
            emotion_system_destroy(system);
        }
    }
};

//=============================================================================
// API Backward Compatibility Tests
//=============================================================================

TEST_F(EmotionalSystemRegressionTest, DefaultConfigValuesStable) {
    // WHAT: Verify default configuration hasn't changed
    // WHY:  Users rely on specific default behavior
    // HOW:  Check all default config values

    emotion_config_t config = emotion_system_default_config();

    // Core features must remain enabled by default
    EXPECT_TRUE(config.enable_emotion_recognition);
    EXPECT_TRUE(config.enable_emotional_tagging);
    EXPECT_TRUE(config.enable_shadow_detection);
    EXPECT_TRUE(config.enable_emotion_regulation);

    // Integration features must remain enabled
    EXPECT_TRUE(config.integrate_with_memory);
    EXPECT_TRUE(config.integrate_with_salience);
    EXPECT_TRUE(config.integrate_with_mental_health);
    EXPECT_TRUE(config.integrate_with_ethics);

    // Parameter ranges must be stable
    EXPECT_GE(config.emotion_decay_rate, 0.0f);
    EXPECT_LE(config.emotion_decay_rate, 1.0f);
    EXPECT_GE(config.arousal_sensitivity, 0.5f);
    EXPECT_LE(config.arousal_sensitivity, 2.0f);
    EXPECT_GE(config.regulation_threshold, 0.0f);
    EXPECT_LE(config.regulation_threshold, 1.0f);
}

TEST_F(EmotionalSystemRegressionTest, CreateDestroyApiStable) {
    // WHAT: Verify create/destroy API hasn't changed
    // WHY:  Core lifecycle API must remain stable
    // HOW:  Test create with NULL and custom config

    // Create with NULL config (default)
    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);
    emotion_system_destroy(system);
    system = nullptr;

    // Create with custom config
    emotion_config_t config = emotion_system_default_config();
    system = emotion_system_create(&config);
    ASSERT_NE(system, nullptr);
    emotion_system_destroy(system);
    system = nullptr;

    // Destroy NULL is safe
    emotion_system_destroy(nullptr);
}

TEST_F(EmotionalSystemRegressionTest, StateQueryApiStable) {
    // WHAT: Verify state query API signatures haven't changed
    // WHY:  Existing code relies on these functions
    // HOW:  Call all query functions, verify behavior

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // get_state API
    emotion_state_t state;
    bool result1 = emotion_system_get_state(system, &state);
    EXPECT_TRUE(result1);

    // get_tag API
    emotional_tag_t tag;
    bool result2 = emotion_system_get_tag(system, &tag);
    EXPECT_TRUE(result2);

    // is_active API
    bool result3 = emotion_system_is_active(system, 0, 0.5f);
    EXPECT_FALSE(result3);  // Initially no emotions active
}

TEST_F(EmotionalSystemRegressionTest, UpdateApiStable) {
    // WHAT: Verify update API signatures stable
    // WHY:  Critical for emotion processing
    // HOW:  Test set_state, decay, update_multimodal

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // set_state API
    bool result1 = emotion_system_set_state(system, 0.5f, 0.6f, 1000);
    EXPECT_TRUE(result1);

    // decay API
    bool result2 = emotion_system_decay(system, 1.0f, 2000);
    EXPECT_TRUE(result2);

    // update_multimodal API
    float visual[64] = {0.5f};
    const char* text = "test";
    bool result3 = emotion_system_update_multimodal(
        system, visual, 64, nullptr, 0, text, 3000
    );
    EXPECT_TRUE(result3);
}

TEST_F(EmotionalSystemRegressionTest, RegulationApiStable) {
    // WHAT: Verify regulation API stable
    // WHY:  Emotion regulation is core feature
    // HOW:  Test regulate and auto_regulate

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, -0.8f, 0.9f, 1000);

    // regulate API
    bool result1 = emotion_system_regulate(system, 0);
    EXPECT_TRUE(result1);

    // auto_regulate API
    emotion_system_set_state(system, -0.9f, 0.95f, 2000);
    bool result2 = emotion_system_auto_regulate(system);
    EXPECT_TRUE(result2);
}

TEST_F(EmotionalSystemRegressionTest, IntegrationApiStable) {
    // WHAT: Verify integration API stable
    // WHY:  Integration functions used by other systems
    // HOW:  Test salience_boost, memory_priority, mental_health_impact

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.7f, 0.8f, 1000);

    // salience_boost API
    float boost = emotion_system_get_salience_boost(system);
    EXPECT_GE(boost, 1.0f);

    // memory_priority API
    float priority = emotion_system_get_memory_priority(system);
    EXPECT_GE(priority, 0.0f);
    EXPECT_LE(priority, 1.0f);

    // mental_health_impact API
    float impact = emotion_system_get_mental_health_impact(system);
    EXPECT_GE(impact, 0.0f);
    EXPECT_LE(impact, 1.0f);
}

TEST_F(EmotionalSystemRegressionTest, StatsApiStable) {
    // WHAT: Verify statistics API stable
    // WHY:  Monitoring and debugging rely on stats
    // HOW:  Test get_stats function and structure

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_stats_t stats;
    bool result = emotion_system_get_stats(system, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_updates, 0u);  // Initially zero
}

//=============================================================================
// Data Structure Compatibility Tests
//=============================================================================

TEST_F(EmotionalSystemRegressionTest, EmotionStateStructureStable) {
    // WHAT: Verify emotion_state_t structure layout stable
    // WHY:  Binary compatibility for serialization
    // HOW:  Check expected fields exist and are accessible

    emotion_state_t state = {0};

    // All fields must be accessible (compile-time check)
    state.valence = 0.5f;
    state.arousal = 0.6f;
    state.intensity = 0.7f;
    state.dominant_emotion = 1;
    state.emotion_confidence = 0.8f;
    state.shadow_intensity = 0.1f;
    state.active_shadow_count = 0;
    state.in_self_regulation = false;
    state.last_update_ms = 1000;
    state.emotional_stability = 0.9f;

    // Verify values stored correctly
    EXPECT_FLOAT_EQ(state.valence, 0.5f);
    EXPECT_FLOAT_EQ(state.arousal, 0.6f);
    EXPECT_EQ(state.dominant_emotion, 1u);
}

TEST_F(EmotionalSystemRegressionTest, EmotionConfigStructureStable) {
    // WHAT: Verify emotion_config_t structure stable
    // WHY:  Configuration compatibility
    // HOW:  Check all fields accessible

    emotion_config_t config = {0};

    // Boolean flags
    config.enable_emotion_recognition = true;
    config.enable_emotional_tagging = true;
    config.enable_shadow_detection = true;
    config.enable_emotion_regulation = true;
    config.integrate_with_memory = true;
    config.integrate_with_salience = true;
    config.integrate_with_mental_health = true;
    config.integrate_with_ethics = true;

    // Float parameters
    config.emotion_decay_rate = 0.05f;
    config.arousal_sensitivity = 1.2f;
    config.valence_sensitivity = 1.0f;
    config.regulation_threshold = 0.75f;

    // Integer parameters
    config.max_shadow_tracked = 10;
    config.shadow_intervention_threshold = 0.6f;

    // Verify stored
    EXPECT_TRUE(config.enable_emotion_recognition);
    EXPECT_FLOAT_EQ(config.arousal_sensitivity, 1.2f);
}

TEST_F(EmotionalSystemRegressionTest, EmotionStatsStructureStable) {
    // WHAT: Verify emotion_stats_t structure stable
    // WHY:  Stats reporting compatibility
    // HOW:  Check all stat fields accessible

    emotion_stats_t stats = {0};

    stats.total_updates = 100;
    stats.total_regulations = 10;
    stats.successful_regulations = 8;
    stats.avg_valence = 0.3f;
    stats.avg_arousal = 0.5f;
    stats.avg_stability = 0.8f;
    stats.shadow_activations = 2;

    EXPECT_EQ(stats.total_updates, 100u);
    EXPECT_EQ(stats.total_regulations, 10u);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(EmotionalSystemRegressionTest, ValenceRangeEnforced) {
    // WHAT: Verify valence stays in [-1, +1]
    // WHY:  Prevent out-of-range valence values
    // HOW:  Try to set extreme values, verify clamping

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Try setting beyond upper bound
    emotion_system_set_state(system, 5.0f, 0.5f, 1000);
    emotion_state_t state1;
    emotion_system_get_state(system, &state1);
    EXPECT_LE(state1.valence, 1.0f);

    // Try setting below lower bound
    emotion_system_set_state(system, -5.0f, 0.5f, 2000);
    emotion_state_t state2;
    emotion_system_get_state(system, &state2);
    EXPECT_GE(state2.valence, -1.0f);
}

TEST_F(EmotionalSystemRegressionTest, ArousalRangeEnforced) {
    // WHAT: Verify arousal stays in [0, 1]
    // WHY:  Prevent invalid arousal values
    // HOW:  Try extreme values, verify clamping

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    // Try setting above upper bound
    emotion_system_set_state(system, 0.5f, 10.0f, 1000);
    emotion_state_t state1;
    emotion_system_get_state(system, &state1);
    EXPECT_LE(state1.arousal, 1.0f);

    // Try setting below lower bound
    emotion_system_set_state(system, 0.5f, -5.0f, 2000);
    emotion_state_t state2;
    emotion_system_get_state(system, &state2);
    EXPECT_GE(state2.arousal, 0.0f);
}

TEST_F(EmotionalSystemRegressionTest, DecayAlwaysReducesArousal) {
    // WHAT: Verify decay never increases arousal
    // WHY:  Decay should only reduce, never amplify
    // HOW:  Decay multiple times, verify monotonic decrease or stable

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, 0.5f, 0.9f, 1000);

    // Get actual arousal after set_state (accounts for sensitivity multiplier)
    emotion_state_t initial_state;
    emotion_system_get_state(system, &initial_state);
    float previous_arousal = initial_state.arousal;

    for (int i = 0; i < 10; i++) {
        emotion_system_decay(system, 0.5f, 1000 + (i + 1) * 500);

        emotion_state_t state;
        emotion_system_get_state(system, &state);

        EXPECT_LE(state.arousal, previous_arousal);
        previous_arousal = state.arousal;
    }
}

TEST_F(EmotionalSystemRegressionTest, RegulationAlwaysReducesIntensity) {
    // WHAT: Verify regulation reduces intensity, never increases
    // WHY:  Regulation purpose is to calm, not amplify
    // HOW:  Apply regulation, verify intensity decreases

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    emotion_system_set_state(system, -0.8f, 0.9f, 1000);

    emotion_state_t state_before;
    emotion_system_get_state(system, &state_before);

    emotion_system_regulate(system, 0);

    emotion_state_t state_after;
    emotion_system_get_state(system, &state_after);

    EXPECT_LE(state_after.intensity, state_before.intensity);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(EmotionalSystemRegressionTest, CreationPerformanceBaseline) {
    // WHAT: Verify system creation time acceptable
    // WHY:  Detect performance regressions
    // HOW:  Time 1000 create/destroy cycles

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        emotional_system_t* temp = emotion_system_create(nullptr);
        emotion_system_destroy(temp);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 1000 create/destroy cycles - threshold relaxed for different hardware
    // This is a regression guard, not a strict performance requirement
    EXPECT_LT(duration.count(), 20000);  // 20ms per cycle average (relaxed for CI/parallel ctest)
}

TEST_F(EmotionalSystemRegressionTest, SetStatePerformanceBaseline) {
    // WHAT: Verify set_state performance
    // WHY:  Detect performance regressions
    // HOW:  Time 10000 set_state calls

    system = emotion_system_create(nullptr);
    ASSERT_NE(system, nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        float valence = (i % 100) / 100.0f - 0.5f;
        float arousal = (i % 50) / 50.0f;
        emotion_system_set_state(system, valence, arousal, 1000 + i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10000 set_state calls should be < 10ms
    EXPECT_LT(duration.count(), 10);
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

TEST_F(EmotionalSystemRegressionTest, NoMemoryLeaksOnRepeatedUse) {
    // WHAT: Verify no memory leaks with repeated use
    // WHY:  Prevent memory leaks
    // HOW:  Create/destroy 1000 times, use extensively

    for (int iter = 0; iter < 1000; iter++) {
        emotional_system_t* temp = emotion_system_create(nullptr);
        ASSERT_NE(temp, nullptr);

        // Use the system
        emotion_system_set_state(temp, 0.5f, 0.6f, 1000);
        emotion_system_decay(temp, 0.1f, 1100);
        emotion_system_auto_regulate(temp);

        emotion_state_t state;
        emotion_system_get_state(temp, &state);

        emotion_system_destroy(temp);
    }

    // If we get here without crashes/ASAN errors, no obvious leaks
    SUCCEED();
}

TEST_F(EmotionalSystemRegressionTest, NullInputsSafetyMaintained) {
    // WHAT: Verify NULL inputs handled safely (no crashes)
    // WHY:  Defensive programming regression
    // HOW:  Pass NULL to all functions

    // All these should return false/0 safely, not crash
    EXPECT_FALSE(emotion_system_get_state(nullptr, nullptr));
    EXPECT_FALSE(emotion_system_get_tag(nullptr, nullptr));
    EXPECT_FALSE(emotion_system_is_active(nullptr, 0, 0.5f));
    EXPECT_FALSE(emotion_system_set_state(nullptr, 0.0f, 0.0f, 0));
    EXPECT_FALSE(emotion_system_decay(nullptr, 0.0f, 0));
    EXPECT_FALSE(emotion_system_update_multimodal(nullptr, nullptr, 0, nullptr, 0, nullptr, 0));
    EXPECT_FALSE(emotion_system_regulate(nullptr, 0));
    EXPECT_FALSE(emotion_system_auto_regulate(nullptr));
    EXPECT_FLOAT_EQ(emotion_system_get_salience_boost(nullptr), 1.0f);
    EXPECT_FLOAT_EQ(emotion_system_get_memory_priority(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(emotion_system_get_mental_health_impact(nullptr), 0.0f);
    EXPECT_FALSE(emotion_system_get_stats(nullptr, nullptr));

    emotion_system_destroy(nullptr);  // Should not crash

    SUCCEED();
}
