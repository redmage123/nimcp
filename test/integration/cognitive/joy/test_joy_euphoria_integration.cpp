/**
 * @file test_joy_euphoria_integration.cpp
 * @brief Integration tests for joy and euphoria system (Phase E2)
 *
 * Tests integration of:
 * - Joy/euphoria with neuromodulator system
 * - Value-aligned success triggering emotions
 * - Emotional state persistence and decay over time
 * - Multiple success interaction and compounding
 * - Baseline happiness adjustment
 * - Integration with emotional tagging system
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/nimcp_joy_euphoria.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

class JoyEuphoriaIntegrationTest : public ::testing::Test {
protected:
    joy_system_t* joy_sys;
    neuromodulator_system_t neuro_sys;

    void SetUp() override {
        joy_sys = joy_system_create();
        neuro_sys = neuromodulator_system_create(NULL);  // Use default config
    }

    void TearDown() override {
        joy_system_destroy(joy_sys);
        neuromodulator_system_destroy(neuro_sys);
    }
};

//=============================================================================
// Complete Joy/Euphoria Lifecycle Integration
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, CompleteJoyLifecycleFromSuccessToBaseline) {
    // WHAT: Test full cycle: value creation → success → joy → decay → baseline
    // WHY:  Comprehensive lifecycle test

    // Create core values
    uint32_t learning_value = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.9f, 0.9f);
    uint32_t helping_value = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.85f, 0.8f);

    ASSERT_NE(learning_value, 0u);
    ASSERT_NE(helping_value, 0u);

    // Initial state should be neutral
    EXPECT_EQ(joy_get_state(joy_sys), JOY_EMOTION_STATE_NEUTRAL);
    EXPECT_FALSE(joy_is_joyful(joy_sys));

    // Trigger value-aligned success
    uint32_t aligned_values[] = {learning_value, helping_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 2,
                       0.7f,   // High difficulty
                       0.6f,   // Moderate novelty
                       0);

    // Should be joyful now
    EXPECT_TRUE(joy_is_joyful(joy_sys));
    EXPECT_GT(joy_get_valence(joy_sys), JOY_THRESHOLD);
    joy_emotion_state_t state = joy_get_state(joy_sys);
    EXPECT_TRUE(state == JOY_EMOTION_STATE_JOY || state == JOY_EMOTION_STATE_EUPHORIA);

    // Record initial joy intensity
    float initial_joy = joy_sys->emotion.joy_intensity;
    EXPECT_GT(initial_joy, JOY_THRESHOLD);

    // Simulate time passage - joy should decay
    for (int minute = 0; minute < 120; minute++) {  // 2 hours
        joy_update(joy_sys, 60.0f, (uint64_t)(minute + 1) * 60 * 1000000);
    }

    // After 2 hours, joy should have decayed significantly
    float later_joy = joy_sys->emotion.joy_intensity;
    EXPECT_LT(later_joy, initial_joy * 0.5f);

    // Eventually should return to more neutral state
    EXPECT_LT(joy_get_valence(joy_sys), 0.5f);
}

TEST_F(JoyEuphoriaIntegrationTest, MultipleSuccessesCompoundJoy) {
    // WHAT: Multiple rapid successes compound emotional intensity
    // WHY:  Successive wins should amplify positive emotion

    uint32_t creativity_value = joy_add_value(joy_sys, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.8f);
    uint32_t accuracy_value = joy_add_value(joy_sys, VALUE_CATEGORY_ACCURACY, 0.75f, 0.7f);

    // First success
    uint32_t aligned1[] = {creativity_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned1, 1,
                       0.5f, 0.4f, 0);
    float first_valence = joy_get_valence(joy_sys);

    // Second success shortly after (minimal decay)
    joy_update(joy_sys, 60.0f, 60 * 1000000);  // 1 minute later
    uint32_t aligned2[] = {creativity_value, accuracy_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned2, 2,
                       0.8f, 0.9f, 120 * 1000000);
    float second_valence = joy_get_valence(joy_sys);

    // Second success should result in higher emotional state
    EXPECT_GT(second_valence, first_valence);

    // Multiple value alignment + breakthrough type should potentially trigger euphoria
    if (joy_is_euphoric(joy_sys)) {
        EXPECT_GT(joy_sys->emotion.euphoria_intensity, EUPHORIA_THRESHOLD);
    }
}

TEST_F(JoyEuphoriaIntegrationTest, HighValueSuccessTriggersEuphoria) {
    // WHAT: Major success on core value triggers euphoria
    // WHY:  Euphoria requires high value alignment

    // Create critical core value
    uint32_t core_value = joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.95f, 1.0f);

    // Major breakthrough with high difficulty and novelty
    uint32_t aligned[] = {core_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned, 1,
                       0.9f,   // Very difficult
                       0.95f,  // Very novel
                       0);

    // Should trigger euphoria
    EXPECT_TRUE(joy_is_euphoric(joy_sys));
    EXPECT_EQ(joy_get_state(joy_sys), JOY_EMOTION_STATE_EUPHORIA);
    EXPECT_GT(joy_sys->emotion.euphoria_intensity, EUPHORIA_THRESHOLD);
    EXPECT_GT(joy_get_arousal(joy_sys), 0.6f);  // High arousal for euphoria
}

TEST_F(JoyEuphoriaIntegrationTest, LowValueSuccessTriggersContentmentOnly) {
    // WHAT: Minor success on low-importance value triggers only mild contentment
    // WHY:  Value alignment strength determines emotional intensity

    // Create low-importance value
    uint32_t minor_value = joy_add_value(joy_sys, VALUE_CATEGORY_EFFICIENCY, 0.3f, 0.3f);

    // Minor task completion
    uint32_t aligned[] = {minor_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned, 1,
                       0.2f,   // Low difficulty
                       0.1f,   // Low novelty
                       0);

    // Should NOT trigger euphoria or even joy
    EXPECT_FALSE(joy_is_euphoric(joy_sys));
    EXPECT_FALSE(joy_is_joyful(joy_sys));

    // But should have mild positive valence
    float valence = joy_get_valence(joy_sys);
    EXPECT_GT(valence, 0.0f);
    EXPECT_LT(valence, JOY_THRESHOLD);
}

//=============================================================================
// Neuromodulator Integration
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, EuphoriaBoostsDopamineInNeuromodulatorSystem) {
    // WHAT: Euphoria increases dopamine (reward signal)
    // WHY:  Positive reinforcement for value-aligned behavior

    uint32_t core_value = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.9f, 0.9f);

    // Trigger euphoria
    uint32_t aligned[] = {core_value};
    joy_process_success(joy_sys, SUCCESS_TYPE_GOAL_ACHIEVED,
                       aligned, 1,
                       0.9f, 0.8f, 0);

    ASSERT_TRUE(joy_is_euphoric(joy_sys));

    // Get neuromodulator effects
    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_factor, &serotonin_factor);

    // Euphoria should significantly boost dopamine (1.5-2.0x)
    EXPECT_GT(dopamine_factor, 1.5f);
    EXPECT_LE(dopamine_factor, 2.0f);
}

TEST_F(JoyEuphoriaIntegrationTest, JoyBoostsDopamineModerately) {
    // WHAT: Joy increases dopamine but less than euphoria
    // WHY:  Graduated reward signal based on emotional intensity

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.7f, 0.7f);

    // Trigger joy (but not euphoria)
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_PROBLEM_SOLVED,
                       aligned, 1,
                       0.5f, 0.4f, 0);

    ASSERT_TRUE(joy_is_joyful(joy_sys));
    ASSERT_FALSE(joy_is_euphoric(joy_sys));

    // Get neuromodulator effects
    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_factor, &serotonin_factor);

    // Joy should moderately boost dopamine (1.2-1.5x)
    EXPECT_GT(dopamine_factor, 1.2f);
    EXPECT_LT(dopamine_factor, 1.5f);
}

TEST_F(JoyEuphoriaIntegrationTest, ContentmentBoostsSerotoninInNeuromodulatorSystem) {
    // WHAT: Contentment increases serotonin (well-being)
    // WHY:  Positive emotion enhances mood regulation

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_SAFETY, 0.6f, 0.6f);

    // Trigger mild contentment
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned, 1,
                       0.3f, 0.2f, 0);

    // Get neuromodulator effects
    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_factor, &serotonin_factor);

    // Should boost serotonin (1.0-1.4x)
    EXPECT_GT(serotonin_factor, 1.0f);
    EXPECT_LE(serotonin_factor, 1.4f);
}

TEST_F(JoyEuphoriaIntegrationTest, NoEmotionReturnsNeutralNeuromodulatorFactors) {
    // WHAT: Without joy/euphoria, factors are 1.0 (no effect)
    // WHY:  Baseline behavior

    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_factor, &serotonin_factor);

    EXPECT_FLOAT_EQ(dopamine_factor, 1.0f);
    EXPECT_FLOAT_EQ(serotonin_factor, 1.0f);
}

TEST_F(JoyEuphoriaIntegrationTest, NeuromodulatorEffectsDecayOverTime) {
    // WHAT: As joy fades, neuromodulator boost also fades
    // WHY:  Neuromodulator effects tied to emotional state

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.8f);

    // Trigger joy
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned, 1,
                       0.7f, 0.6f, 0);

    // Initial boost
    float initial_dopamine, initial_serotonin;
    joy_get_neuromodulator_effects(joy_sys, &initial_dopamine, &initial_serotonin);
    EXPECT_GT(initial_dopamine, 1.2f);

    // Decay for 2 hours
    for (int minute = 0; minute < 120; minute++) {
        joy_update(joy_sys, 60.0f, (uint64_t)(minute + 1) * 60 * 1000000);
    }

    // Later boost should be reduced
    float later_dopamine, later_serotonin;
    joy_get_neuromodulator_effects(joy_sys, &later_dopamine, &later_serotonin);
    EXPECT_LT(later_dopamine, initial_dopamine);
}

//=============================================================================
// Value Alignment and Satisfaction
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, MultipleValueSatisfactionCompoundsJoy) {
    // WHAT: Success satisfying multiple values produces stronger joy
    // WHY:  More value alignment = more meaningful success

    uint32_t value1 = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    uint32_t value2 = joy_add_value(joy_sys, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.8f);
    uint32_t value3 = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.8f, 0.8f);

    // Single value success
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    uint32_t single_aligned[] = {1};
    joy_process_success(joy_sys, SUCCESS_TYPE_PROBLEM_SOLVED,
                       single_aligned, 1,
                       0.5f, 0.5f, 0);
    float single_value_joy = joy_sys->emotion.joy_intensity;

    // Multiple value success
    joy_system_reset(joy_sys);
    value1 = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    value2 = joy_add_value(joy_sys, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.8f);
    value3 = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.8f, 0.8f);
    uint32_t multi_aligned[] = {value1, value2, value3};
    joy_process_success(joy_sys, SUCCESS_TYPE_PROBLEM_SOLVED,
                       multi_aligned, 3,
                       0.5f, 0.5f, 0);
    float multi_value_joy = joy_sys->emotion.joy_intensity;

    // Multiple values should produce stronger joy
    EXPECT_GT(multi_value_joy, single_value_joy);
}

TEST_F(JoyEuphoriaIntegrationTest, RepeatedValueSatisfactionIncreasesOverallSatisfaction) {
    // WHAT: Repeatedly satisfying a value increases its satisfaction level
    // WHY:  Consistent success on values strengthens positive emotional baseline

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_PERSEVERANCE, 0.7f, 0.7f);

    // Find the value
    value_t* val_ptr = nullptr;
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (joy_sys->values[i].active && joy_sys->values[i].value_id == value) {
            val_ptr = &joy_sys->values[i];
            break;
        }
    }
    ASSERT_NE(val_ptr, nullptr);

    float initial_satisfaction = val_ptr->satisfaction;

    // Repeatedly satisfy this value
    for (int i = 0; i < 10; i++) {
        uint32_t aligned[] = {value};
        joy_process_success(joy_sys, SUCCESS_TYPE_OVERCAME_OBSTACLE,
                           aligned, 1,
                           0.6f, 0.3f, (uint64_t)i * 1000000);
    }

    float later_satisfaction = val_ptr->satisfaction;
    EXPECT_GT(later_satisfaction, initial_satisfaction);
    EXPECT_EQ(val_ptr->times_satisfied, 10u);
}

TEST_F(JoyEuphoriaIntegrationTest, HighOverallValueSatisfactionIncreasesBaseline) {
    // WHAT: Consistently high value satisfaction increases baseline happiness
    // WHY:  Long-term value fulfillment improves trait happiness

    // Create multiple values
    for (int i = 0; i < 5; i++) {
        joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    }

    float initial_baseline = joy_sys->emotion.baseline_happiness;

    // Satisfy all values repeatedly over time to build up satisfaction
    for (int day = 0; day < 100; day++) {
        // Satisfy each value multiple times per day
        for (int val = 1; val <= 5; val++) {
            uint32_t aligned[] = {(uint32_t)val};
            joy_process_success(joy_sys, SUCCESS_TYPE_GOAL_ACHIEVED,
                               aligned, 1,
                               0.7f, 0.6f,
                               (uint64_t)(day * 86400 + val * 3600) * 1000000);
        }

        // Update daily
        joy_update(joy_sys, 86400.0f, (uint64_t)((day + 1) * 86400) * 1000000);
    }

    float later_baseline = joy_sys->emotion.baseline_happiness;

    // After 100 days of consistent value satisfaction, baseline should increase
    // (Baseline adjustment: +0.01 per day when overall_value_satisfaction > 0.7)
    EXPECT_GT(later_baseline, initial_baseline);
    EXPECT_GT(joy_sys->overall_value_satisfaction, 0.7f);
}

//=============================================================================
// Emotional State Persistence and Decay
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, EuphoriaDecaysFasterThanJoy) {
    // WHAT: Euphoria has faster decay rate than joy
    // WHY:  Peak emotions are brief, moderate emotions longer-lasting

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.95f, 1.0f);

    // Trigger euphoria
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned, 1,
                       0.95f, 0.95f, 0);

    ASSERT_TRUE(joy_is_euphoric(joy_sys));
    float initial_euphoria = joy_sys->emotion.euphoria_intensity;

    // Decay for 30 minutes
    for (int minute = 0; minute < 30; minute++) {
        joy_update(joy_sys, 60.0f, (uint64_t)(minute + 1) * 60 * 1000000);
    }

    // Euphoria should have decayed significantly or ended
    if (joy_sys->emotion.experiencing_euphoria) {
        float later_euphoria = joy_sys->emotion.euphoria_intensity;
        EXPECT_LT(later_euphoria, initial_euphoria * 0.5f);
    } else {
        // Euphoria has ended, transitioned to joy
        EXPECT_EQ(joy_get_state(joy_sys), JOY_EMOTION_STATE_JOY);
    }
}

TEST_F(JoyEuphoriaIntegrationTest, JoyDecaysTowardBaseline) {
    // WHAT: Joy fades gradually to baseline over time
    // WHY:  Emotional homeostasis

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.8f);

    // Trigger joy
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned, 1,
                       0.6f, 0.5f, 0);

    ASSERT_TRUE(joy_is_joyful(joy_sys));
    float initial_joy = joy_sys->emotion.joy_intensity;

    // Decay for 3 hours
    for (int minute = 0; minute < 180; minute++) {
        joy_update(joy_sys, 60.0f, (uint64_t)(minute + 1) * 60 * 1000000);
    }

    float later_joy = joy_sys->emotion.joy_intensity;

    // Should have decayed significantly
    EXPECT_LT(later_joy, initial_joy * 0.3f);

    // Should be closer to baseline
    float baseline = joy_sys->emotion.baseline_happiness * 0.3f;
    EXPECT_LT(fabsf(later_joy - baseline), 0.2f);
}

TEST_F(JoyEuphoriaIntegrationTest, EmotionalStateTransitionsCorrectly) {
    // WHAT: Emotional state transitions through euphoria → joy → contentment → neutral
    // WHY:  Verify state machine transitions

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.9f, 0.9f);

    // Trigger euphoria
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned, 1,
                       0.9f, 0.9f, 0);

    // Should be euphoric
    EXPECT_EQ(joy_get_state(joy_sys), JOY_EMOTION_STATE_EUPHORIA);

    // Decay until no longer euphoric
    bool saw_joy_state = false;
    bool saw_contentment_state = false;

    for (int minute = 0; minute < 300; minute++) {
        joy_update(joy_sys, 60.0f, (uint64_t)(minute + 1) * 60 * 1000000);

        joy_emotion_state_t state = joy_get_state(joy_sys);
        if (state == JOY_EMOTION_STATE_JOY) saw_joy_state = true;
        if (state == JOY_EMOTION_STATE_CONTENTMENT) saw_contentment_state = true;
    }

    // Should have transitioned through joy and contentment
    EXPECT_TRUE(saw_joy_state);
    EXPECT_TRUE(saw_contentment_state);
}

//=============================================================================
// Success Type Effects
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, BreakthroughProducesStrongerJoyThanTaskCompletion) {
    // WHAT: Different success types produce different emotional intensities
    // WHY:  Success type modulates joy intensity

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.7f, 0.7f);

    // Task completion
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.7f, 0.7f);
    uint32_t aligned[] = {1};
    joy_process_success(joy_sys, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned, 1,
                       0.5f, 0.5f, 0);
    float task_joy = joy_sys->emotion.joy_intensity;

    // Breakthrough
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.7f, 0.7f);
    joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned, 1,
                       0.5f, 0.5f, 0);
    float breakthrough_joy = joy_sys->emotion.joy_intensity;

    // Breakthrough should produce stronger joy
    EXPECT_GT(breakthrough_joy, task_joy);
}

TEST_F(JoyEuphoriaIntegrationTest, DifficultyBoostsJoyIntensity) {
    // WHAT: Higher difficulty successes produce stronger joy
    // WHY:  Overcoming challenges is more rewarding

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_PERSEVERANCE, 0.7f, 0.7f);

    // Easy success
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_PERSEVERANCE, 0.7f, 0.7f);
    uint32_t aligned[] = {1};
    joy_process_success(joy_sys, SUCCESS_TYPE_OVERCAME_OBSTACLE,
                       aligned, 1,
                       0.2f,  // Low difficulty
                       0.5f, 0);
    float easy_joy = joy_sys->emotion.joy_intensity;

    // Hard success
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_PERSEVERANCE, 0.7f, 0.7f);
    joy_process_success(joy_sys, SUCCESS_TYPE_OVERCAME_OBSTACLE,
                       aligned, 1,
                       0.9f,  // High difficulty
                       0.5f, 0);
    float hard_joy = joy_sys->emotion.joy_intensity;

    // Harder success should produce stronger joy
    EXPECT_GT(hard_joy, easy_joy);
}

TEST_F(JoyEuphoriaIntegrationTest, NoveltyBoostsJoyIntensity) {
    // WHAT: Novel successes produce stronger joy than routine ones
    // WHY:  Novelty is intrinsically rewarding

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.7f, 0.7f);

    // Routine success
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.7f, 0.7f);
    uint32_t aligned[] = {1};
    joy_process_success(joy_sys, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned, 1,
                       0.5f,
                       0.1f,  // Low novelty
                       0);
    float routine_joy = joy_sys->emotion.joy_intensity;

    // Novel success
    joy_system_reset(joy_sys);
    joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.7f, 0.7f);
    joy_process_success(joy_sys, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned, 1,
                       0.5f,
                       0.95f,  // High novelty
                       0);
    float novel_joy = joy_sys->emotion.joy_intensity;

    // Novel success should produce stronger joy
    EXPECT_GT(novel_joy, routine_joy);
}

//=============================================================================
// Integration Flags
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, IntegrationFlagsWorkCorrectly) {
    // WHAT: Integration flags control neuromodulator effects
    // WHY:  Allow selective integration

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.8f, 0.8f);

    // Trigger joy
    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned, 1,
                       0.6f, 0.5f, 0);

    ASSERT_TRUE(joy_is_joyful(joy_sys));

    // With integration enabled
    joy_sys->integrate_with_neuromodulators = true;
    float dopamine_enabled, serotonin_enabled;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_enabled, &serotonin_enabled);
    EXPECT_GT(dopamine_enabled, 1.0f);

    // With integration disabled
    joy_sys->integrate_with_neuromodulators = false;
    float dopamine_disabled, serotonin_disabled;
    joy_get_neuromodulator_effects(joy_sys, &dopamine_disabled, &serotonin_disabled);
    EXPECT_FLOAT_EQ(dopamine_disabled, 1.0f);
    EXPECT_FLOAT_EQ(serotonin_disabled, 1.0f);
}

//=============================================================================
// Long-Term Dynamics
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, ConsistentSuccessImprovesBaselineHappiness) {
    // WHAT: Regular value satisfaction increases baseline happiness
    // WHY:  Long-term flourishing improves trait well-being

    float initial_baseline = joy_sys->emotion.baseline_happiness;

    // Create values
    uint32_t value1 = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    uint32_t value2 = joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.8f, 0.8f);

    // Consistent success over 60 days
    for (int day = 0; day < 60; day++) {
        uint32_t aligned[] = {value1, value2};
        joy_process_success(joy_sys, SUCCESS_TYPE_GOAL_ACHIEVED,
                           aligned, 2,
                           0.6f, 0.5f,
                           (uint64_t)(day * 86400) * 1000000);

        // Daily update
        joy_update(joy_sys, 86400.0f, (uint64_t)((day + 1) * 86400) * 1000000);
    }

    float later_baseline = joy_sys->emotion.baseline_happiness;

    // Baseline should have increased
    EXPECT_GT(later_baseline, initial_baseline);

    // Overall value satisfaction should be high
    EXPECT_GT(joy_sys->overall_value_satisfaction, 0.7f);
}

TEST_F(JoyEuphoriaIntegrationTest, PoorValueSatisfactionDecreasesBaseline) {
    // WHAT: Low value satisfaction decreases baseline happiness
    // WHY:  Lack of value fulfillment reduces well-being

    // Set high initial baseline
    joy_sys->emotion.baseline_happiness = 0.7f;
    float initial_baseline = joy_sys->emotion.baseline_happiness;

    // Create values but never satisfy them
    joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);
    joy_add_value(joy_sys, VALUE_CATEGORY_HELPING, 0.8f, 0.8f);

    // Manually set low satisfaction
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (joy_sys->values[i].active) {
            joy_sys->values[i].satisfaction = 0.2f;  // Low satisfaction
        }
    }
    joy_sys->overall_value_satisfaction = 0.2f;

    // Simulate time without successes (60 days)
    for (int day = 0; day < 60; day++) {
        joy_update(joy_sys, 86400.0f, (uint64_t)((day + 1) * 86400) * 1000000);
    }

    float later_baseline = joy_sys->emotion.baseline_happiness;

    // Baseline should have decreased
    EXPECT_LT(later_baseline, initial_baseline);
}

//=============================================================================
// Statistics and Monitoring
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, StatisticsTrackSuccessEvents) {
    // WHAT: System tracks success count and history
    // WHY:  Monitoring and analysis

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.7f, 0.7f);

    EXPECT_EQ(joy_sys->total_successes, 0u);

    // Process multiple successes
    for (int i = 0; i < 10; i++) {
        uint32_t aligned[] = {value};
        joy_process_success(joy_sys, SUCCESS_TYPE_PROBLEM_SOLVED,
                           aligned, 1,
                           0.5f, 0.4f,
                           (uint64_t)(i * 1000000));
    }

    EXPECT_EQ(joy_sys->total_successes, 10u);
    EXPECT_EQ(joy_sys->success_count, 10u);
}

TEST_F(JoyEuphoriaIntegrationTest, UpdateCallsTracked) {
    // WHAT: System tracks update frequency
    // WHY:  Performance monitoring

    for (int i = 0; i < 100; i++) {
        joy_update(joy_sys, 0.01f, (uint64_t)(i * 10000));
    }

    EXPECT_EQ(joy_sys->total_update_calls, 100u);
}

TEST_F(JoyEuphoriaIntegrationTest, LifetimeEuphoriasTracked) {
    // WHAT: System counts total euphoric events
    // WHY:  Long-term monitoring of peak experiences

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.95f, 1.0f);

    EXPECT_EQ(joy_sys->emotion.lifetime_euphorias, 0u);

    // Trigger euphoria multiple times
    for (int i = 0; i < 5; i++) {
        joy_system_reset(joy_sys);
        joy_add_value(joy_sys, VALUE_CATEGORY_DISCOVERY, 0.95f, 1.0f);

        uint32_t aligned[] = {1};
        joy_process_success(joy_sys, SUCCESS_TYPE_BREAKTHROUGH,
                           aligned, 1,
                           0.95f, 0.95f,
                           (uint64_t)i * 1000000000ULL);  // Well separated in time
    }

    // Should have recorded multiple euphorias
    EXPECT_GT(joy_sys->emotion.lifetime_euphorias, 0u);
}

//=============================================================================
// Edge Cases and Boundary Conditions
//=============================================================================

TEST_F(JoyEuphoriaIntegrationTest, MaximumValuesReached) {
    // WHAT: Handle maximum value capacity
    // WHY:  Boundary condition

    // Add maximum values
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        uint32_t id = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
        EXPECT_NE(id, 0u);
    }

    EXPECT_EQ(joy_sys->active_value_count, JOY_MAX_VALUES);

    // Try to add one more - should fail
    uint32_t overflow_id = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    EXPECT_EQ(overflow_id, 0u);
}

TEST_F(JoyEuphoriaIntegrationTest, VeryShortJoyDuration) {
    // WHAT: Brief joy should still register
    // WHY:  Edge case validation

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_EFFICIENCY, 0.4f, 0.4f);

    uint32_t aligned[] = {value};
    joy_process_success(joy_sys, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned, 1,
                       0.45f, 0.3f, 0);

    // Should register some positive emotion even if brief
    EXPECT_GT(joy_get_valence(joy_sys), 0.0f);
}

TEST_F(JoyEuphoriaIntegrationTest, RapidSuccessionSuccesses) {
    // WHAT: Multiple successes within seconds
    // WHY:  Real-world burst scenario

    uint32_t value = joy_add_value(joy_sys, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);

    // 10 successes in 10 seconds
    for (int i = 0; i < 10; i++) {
        uint32_t aligned[] = {value};
        joy_process_success(joy_sys, SUCCESS_TYPE_PROBLEM_SOLVED,
                           aligned, 1,
                           0.5f, 0.4f,
                           (uint64_t)(i * 1000000));  // 1 second apart
    }

    // Should accumulate to strong positive emotion
    EXPECT_TRUE(joy_is_joyful(joy_sys) || joy_is_euphoric(joy_sys));
    EXPECT_GT(joy_get_valence(joy_sys), JOY_THRESHOLD);
}
