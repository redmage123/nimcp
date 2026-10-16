/**
 * @file test_joy_euphoria.cpp
 * @brief Unit tests for joy, euphoria, and value-aligned success system
 *
 * COVERAGE TARGET: 100%
 * Tests all functions, edge cases, and state transitions
 *
 * @version Phase E2: Joy and Euphoria
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "cognitive/nimcp_joy_euphoria.h"

class JoySystemTest : public ::testing::Test {
protected:
    joy_system_t* system;

    void SetUp() override {
        system = joy_system_create();
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        joy_system_destroy(system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(JoySystemTest, CreateInitializesValidSystem) {
    // WHAT: Verify creation initializes all fields correctly
    // WHY:  Proper initialization is critical

    EXPECT_EQ(system->active_value_count, 0);
    EXPECT_EQ(system->success_count, 0);
    EXPECT_EQ(system->total_successes, 0);
    EXPECT_TRUE(system->integrate_with_neuromodulators);
    EXPECT_TRUE(system->integrate_with_ethics);
    EXPECT_TRUE(system->integrate_with_learning);

    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_NEUTRAL);
    EXPECT_FLOAT_EQ(system->emotion.baseline_happiness, 0.5f);
    EXPECT_FALSE(system->emotion.experiencing_euphoria);
    EXPECT_FLOAT_EQ(system->emotion.joy_intensity, 0.0f);
}

TEST_F(JoySystemTest, ResetClearsState) {
    // WHAT: Reset clears emotional state but preserves values
    // WHY:  Testing capability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.9f, 0.8f);
    EXPECT_GT(val_id, 0);

    joy_system_reset(system);

    // Values preserved
    EXPECT_EQ(system->active_value_count, 1);

    // State cleared
    EXPECT_EQ(system->total_successes, 0);
    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_NEUTRAL);
}

TEST_F(JoySystemTest, DestroyHandlesNullPointer) {
    // WHAT: Null pointer doesn't crash
    // WHY:  Robustness

    joy_system_destroy(nullptr);
    // No crash = success
}

//=============================================================================
// Value System Tests
//=============================================================================

TEST_F(JoySystemTest, AddValueReturnsValidID) {
    // WHAT: Adding a value returns non-zero ID
    // WHY:  ID is used for referencing values

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);

    EXPECT_GT(val_id, 0);
    EXPECT_EQ(system->active_value_count, 1);
}

TEST_F(JoySystemTest, ValueHasCorrectProperties) {
    // WHAT: Value fields are initialized correctly
    // WHY:  Values determine joy intensity

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.85f, 0.75f);

    // Find the value
    bool found = false;
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_EQ(system->values[i].category, VALUE_CATEGORY_CREATIVITY);
            EXPECT_FLOAT_EQ(system->values[i].importance, 0.85f);
            EXPECT_FLOAT_EQ(system->values[i].weight, 0.75f);
            EXPECT_FLOAT_EQ(system->values[i].satisfaction, 0.5f);  // Default
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(JoySystemTest, UpdateValueSatisfactionIncreasesIt) {
    // WHAT: Satisfaction can be updated
    // WHY:  Values change satisfaction over time

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_ACCURACY, 0.7f, 0.6f);

    joy_update_value_satisfaction(system, val_id, 0.3f);

    // Check satisfaction increased
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_FLOAT_EQ(system->values[i].satisfaction, 0.8f);  // 0.5 + 0.3
            break;
        }
    }
}

TEST_F(JoySystemTest, MaxValuesRespected) {
    // WHAT: Cannot add more than JOY_MAX_VALUES values
    // WHY:  Prevent overflow

    // Fill all slots
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
        EXPECT_GT(val_id, 0);
    }

    // Try to add one more (should fail)
    uint32_t overflow_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    EXPECT_EQ(overflow_id, 0);
}

TEST_F(JoySystemTest, SatisfactionClampsToRange) {
    // WHAT: Satisfaction clamped to [0, 1]
    // WHY:  Prevent invalid values

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_SAFETY, 0.7f, 0.6f);

    joy_update_value_satisfaction(system, val_id, 1.5f);  // Try to exceed 1.0

    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_LE(system->values[i].satisfaction, 1.0f);
            break;
        }
    }
}

//=============================================================================
// Success Processing Tests
//=============================================================================

TEST_F(JoySystemTest, ProcessSuccessTriggersJoy) {
    // WHAT: Value-aligned success triggers joy
    // WHY:  Core mechanism

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.9f, 0.8f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.6f, 0.5f, 1000000);

    EXPECT_GT(system->emotion.joy_intensity, 0.0f);
    EXPECT_GT(system->emotion.positive_valence, 0.0f);
    EXPECT_EQ(system->total_successes, 1);
}

TEST_F(JoySystemTest, HighAlignmentTriggers_Euphoria) {
    // WHAT: Strong value alignment with difficulty triggers euphoria
    // WHY:  Euphoria requires high threshold

    // Add core value (high importance)
    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    // Breakthrough with high difficulty and novelty
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.9f, 0.9f, 1000000);

    EXPECT_TRUE(system->emotion.experiencing_euphoria);
    EXPECT_GE(system->emotion.euphoria_intensity, EUPHORIA_THRESHOLD);
    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_EUPHORIA);
    EXPECT_EQ(system->emotion.lifetime_euphorias, 1);
}

TEST_F(JoySystemTest, MultipleValuesIncreaseIntensity) {
    // WHAT: Satisfying multiple values increases joy
    // WHY:  Multiple value alignment is more rewarding

    uint32_t val1 = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t val2 = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.7f);
    uint32_t val3 = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);

    uint32_t aligned_single[] = {val1};
    joy_process_success(system, SUCCESS_TYPE_PROBLEM_SOLVED,
                       aligned_single, 1, 0.5f, 0.5f, 1000000);
    float single_value_joy = system->emotion.joy_intensity;

    joy_system_reset(system);

    // Re-add values
    val1 = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    val2 = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.7f);
    val3 = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);

    uint32_t aligned_multiple[] = {val1, val2, val3};
    joy_process_success(system, SUCCESS_TYPE_PROBLEM_SOLVED,
                       aligned_multiple, 3, 0.5f, 0.5f, 1000000);
    float multiple_value_joy = system->emotion.joy_intensity;

    EXPECT_GT(multiple_value_joy, single_value_joy);
}

TEST_F(JoySystemTest, DifficultyIncreasesJoyIntensity) {
    // WHAT: Harder tasks produce more joy
    // WHY:  Achievement proportional to difficulty

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_PERSEVERANCE, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    // Easy task
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.2f, 0.3f, 1000000);
    float easy_joy = system->emotion.joy_intensity;

    joy_system_reset(system);
    val_id = joy_add_value(system, VALUE_CATEGORY_PERSEVERANCE, 0.8f, 0.7f);
    aligned_values[0] = val_id;

    // Hard task
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.9f, 0.3f, 1000000);
    float hard_joy = system->emotion.joy_intensity;

    EXPECT_GT(hard_joy, easy_joy);
}

TEST_F(JoySystemTest, NoveltyIncreasesJoyIntensity) {
    // WHAT: Novel successes produce more joy
    // WHY:  Novelty bonus

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    // Familiar task
    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.5f, 0.1f, 1000000);
    float familiar_joy = system->emotion.joy_intensity;

    joy_system_reset(system);
    val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.8f, 0.7f);
    aligned_values[0] = val_id;

    // Novel task
    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.5f, 0.9f, 1000000);
    float novel_joy = system->emotion.joy_intensity;

    EXPECT_GT(novel_joy, familiar_joy);
}

TEST_F(JoySystemTest, SuccessTypeModifiesIntensity) {
    // WHAT: Different success types have different multipliers
    // WHY:  Breakthroughs more rewarding than routine

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_ACCURACY, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    // Routine task
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.5f, 0.5f, 1000000);
    float routine_joy = system->emotion.joy_intensity;

    joy_system_reset(system);
    val_id = joy_add_value(system, VALUE_CATEGORY_ACCURACY, 0.8f, 0.7f);
    aligned_values[0] = val_id;

    // Breakthrough
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.5f, 0.5f, 1000000);
    float breakthrough_joy = system->emotion.joy_intensity;

    EXPECT_GT(breakthrough_joy, routine_joy);
}

TEST_F(JoySystemTest, SuccessRecordedInHistory) {
    // WHAT: Success events are tracked
    // WHY:  History for analysis

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_PROBLEM_SOLVED,
                       aligned_values, 1, 0.6f, 0.5f, 1000000);

    EXPECT_EQ(system->success_count, 1);
    EXPECT_TRUE(system->recent_successes[0].active);
    EXPECT_EQ(system->recent_successes[0].type, SUCCESS_TYPE_PROBLEM_SOLVED);
}

//=============================================================================
// Emotional Dynamics Tests
//=============================================================================

TEST_F(JoySystemTest, JoyDecaysOverTime) {
    // WHAT: Joy intensity decreases over time
    // WHY:  Emotions are transient

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    float initial_joy = system->emotion.joy_intensity;
    EXPECT_GT(initial_joy, 0.4f);

    // Update over 30 minutes
    for (int i = 0; i < 30; i++) {
        joy_update(system, 60.0f, (uint64_t)(i * 60) * 1000000);
    }

    float later_joy = system->emotion.joy_intensity;
    EXPECT_LT(later_joy, initial_joy);
}

TEST_F(JoySystemTest, EuphoriaDecaysFasterThanJoy) {
    // WHAT: Euphoria has shorter duration than joy
    // WHY:  Intense emotions fade faster

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    EXPECT_TRUE(system->emotion.experiencing_euphoria);
    float initial_euphoria = system->emotion.euphoria_intensity;

    // Update over 15 minutes
    for (int i = 0; i < 15; i++) {
        joy_update(system, 60.0f, (uint64_t)(i * 60) * 1000000);
    }

    // Euphoria should have significantly decayed
    EXPECT_LT(system->emotion.euphoria_intensity, initial_euphoria * 0.5f);
}

TEST_F(JoySystemTest, EuphoriaTransitionsToJoy) {
    // WHAT: When euphoria fades below threshold, state becomes JOY
    // WHY:  State machine transition

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_EUPHORIA);

    // Run until euphoria ends
    for (int i = 0; i < 20; i++) {
        joy_update(system, 60.0f, (uint64_t)(i * 60) * 1000000);
        if (!system->emotion.experiencing_euphoria) {
            break;
        }
    }

    EXPECT_FALSE(system->emotion.experiencing_euphoria);
    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_JOY);
}

TEST_F(JoySystemTest, BaselineHappinessIncreasesWithSatisfaction) {
    // WHAT: Consistent value satisfaction increases baseline
    // WHY:  Long-term well-being

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.8f);

    float initial_baseline = system->emotion.baseline_happiness;

    // Many satisfying successes over time
    for (int i = 0; i < 10; i++) {
        uint32_t aligned_values[] = {val_id};
        joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                           aligned_values, 1, 0.6f, 0.5f, (uint64_t)i * 1000000);
        joy_update(system, 86400.0f, (uint64_t)(i * 86400) * 1000000);  // 1 day each
    }

    float final_baseline = system->emotion.baseline_happiness;
    EXPECT_GT(final_baseline, initial_baseline);
}

//=============================================================================
// State Machine Tests
//=============================================================================

TEST_F(JoySystemTest, StateTransitionsCorrectly) {
    // WHAT: Emotional state reflects intensity
    // WHY:  State machine correctness

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.7f, 0.6f);
    uint32_t aligned_values[] = {val_id};

    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_NEUTRAL);

    // Mild success → contentment
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.2f, 0.2f, 0);

    // Should be contentment or joy
    EXPECT_TRUE(system->emotion.state == JOY_EMOTION_STATE_CONTENTMENT ||
                system->emotion.state == JOY_EMOTION_STATE_JOY);
}

//=============================================================================
// Query Function Tests
//=============================================================================

TEST_F(JoySystemTest, IsJoyfulReturnsTrueWhenJoyful) {
    // WHAT: joy_is_joyful() returns true when joy >= threshold
    // WHY:  Query function correctness

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.85f, 0.8f);
    uint32_t aligned_values[] = {val_id};

    EXPECT_FALSE(joy_is_joyful(system));

    joy_process_success(system, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    EXPECT_TRUE(joy_is_joyful(system));
}

TEST_F(JoySystemTest, IsEuphoricReturnsTrueWhenEuphoric) {
    // WHAT: joy_is_euphoric() returns true during euphoria
    // WHY:  Query function correctness

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    EXPECT_FALSE(joy_is_euphoric(system));

    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    EXPECT_TRUE(joy_is_euphoric(system));
}

TEST_F(JoySystemTest, GetValenceReturnsCorrectValue) {
    // WHAT: joy_get_valence() returns positive_valence
    // WHY:  Query function correctness

    float valence = joy_get_valence(system);
    EXPECT_GE(valence, 0.0f);
    EXPECT_LE(valence, 1.0f);
}

TEST_F(JoySystemTest, GetArousalReturnsCorrectValue) {
    // WHAT: joy_get_arousal() returns arousal level
    // WHY:  Query function correctness

    float arousal = joy_get_arousal(system);
    EXPECT_GE(arousal, 0.0f);
    EXPECT_LE(arousal, 1.0f);
}

TEST_F(JoySystemTest, GetStateReturnsCorrectState) {
    // WHAT: joy_get_state() returns current state
    // WHY:  Query function correctness

    joy_emotion_state_t state = joy_get_state(system);
    EXPECT_EQ(state, JOY_EMOTION_STATE_NEUTRAL);
}

//=============================================================================
// Neuromodulator Integration Tests
//=============================================================================

TEST_F(JoySystemTest, GetNeuromodulatorEffectsReturnsCorrectValues) {
    // WHAT: Neuromodulator factors computed correctly
    // WHY:  Integration with neuromodulator system

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(system, &dopamine_factor, &serotonin_factor);

    // Joy should increase dopamine
    EXPECT_GE(dopamine_factor, 1.0f);
    // Joy should increase serotonin
    EXPECT_GE(serotonin_factor, 1.0f);
}

TEST_F(JoySystemTest, EuphoriaBoostsDopamineMore) {
    // WHAT: Euphoria increases dopamine more than joy
    // WHY:  Intense reward signal

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(system, &dopamine_factor, &serotonin_factor);

    EXPECT_GT(dopamine_factor, 1.5f);  // Significant boost
}

//=============================================================================
// Emotional Tag Integration Tests
//=============================================================================

TEST_F(JoySystemTest, GetEmotionReturnsNeutralWhenNoJoy) {
    // WHAT: Neutral emotion when no joy
    // WHY:  Emotional tagging integration

    emotional_tag_t emotion = joy_get_emotion(system);

    // Should be neutral or very low positive
    EXPECT_LT(emotion.valence, 0.3f);
}

TEST_F(JoySystemTest, GetEmotionReturnsPositiveWhenJoyful) {
    // WHAT: Positive valence when joyful
    // WHY:  Emotional tagging integration

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    emotional_tag_t emotion = joy_get_emotion(system);

    EXPECT_GT(emotion.valence, 0.4f);  // Joy threshold
    EXPECT_GT(emotion.arousal, 0.0f);
}

TEST_F(JoySystemTest, GetEmotionReturnsHighValenceWhenEuphoric) {
    // WHAT: High positive valence during euphoria
    // WHY:  Emotional tagging integration

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    emotional_tag_t emotion = joy_get_emotion(system);

    EXPECT_GT(emotion.valence, 0.7f);  // Euphoria threshold
    EXPECT_GT(emotion.arousal, 0.6f);  // High arousal
}

//=============================================================================
// Edge Case and Robustness Tests
//=============================================================================

TEST_F(JoySystemTest, HandlesNullPointers) {
    // WHAT: Null pointer safety
    // WHY:  Robustness

    joy_system_reset(nullptr);
    joy_add_value(nullptr, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    joy_update_value_satisfaction(nullptr, 1, 0.1f);
    joy_process_success(nullptr, SUCCESS_TYPE_TASK_COMPLETION, nullptr, 0, 0.5f, 0.5f, 0);
    joy_update(nullptr, 1.0f, 0);

    EXPECT_FALSE(joy_is_joyful(nullptr));
    EXPECT_FALSE(joy_is_euphoric(nullptr));
    EXPECT_FLOAT_EQ(joy_get_valence(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(joy_get_arousal(nullptr), 0.0f);
    EXPECT_EQ(joy_get_state(nullptr), JOY_EMOTION_STATE_NEUTRAL);

    // No crashes = success
}

TEST_F(JoySystemTest, HandlesZeroTimeStep) {
    // WHAT: Zero dt doesn't crash
    // WHY:  Edge case robustness

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.5f, 0.5f, 0);

    joy_update(system, 0.0f, 0);

    // No crash and joy doesn't change
    EXPECT_GT(system->emotion.joy_intensity, 0.0f);
}

TEST_F(JoySystemTest, HandlesInvalidValueID) {
    // WHAT: Invalid value ID doesn't crash
    // WHY:  Robustness

    joy_update_value_satisfaction(system, 9999, 0.5f);
    // No crash = success
}

TEST_F(JoySystemTest, HandlesExcessiveParameters) {
    // WHAT: Out-of-range parameters are clamped
    // WHY:  Input validation

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_EFFICIENCY, 2.0f, -1.0f);
    EXPECT_GT(val_id, 0);

    // Check clamping occurred
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_LE(system->values[i].importance, 1.0f);
            EXPECT_GE(system->values[i].weight, 0.0f);
            break;
        }
    }
}

TEST_F(JoySystemTest, UpdateCallsCounterIncrements) {
    // WHAT: Statistics track update calls
    // WHY:  Monitoring capability

    uint64_t initial = system->total_update_calls;

    for (int i = 0; i < 100; i++) {
        joy_update(system, 0.1f, (uint64_t)(i * 100000));
    }

    EXPECT_EQ(system->total_update_calls, initial + 100);
}

TEST_F(JoySystemTest, SuccessHistoryWrapsCorrectly) {
    // WHAT: Success history is ring buffer
    // WHY:  Bounded memory usage

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.7f, 0.6f);
    uint32_t aligned_values[] = {val_id};

    // Fill history beyond capacity
    for (int i = 0; i < JOY_MAX_RECENT_SUCCESSES + 5; i++) {
        joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                           aligned_values, 1, 0.5f, 0.5f, (uint64_t)i * 1000000);
    }

    EXPECT_EQ(system->total_successes, JOY_MAX_RECENT_SUCCESSES + 5);
    // Ring buffer wrapping happened (no overflow)
}
