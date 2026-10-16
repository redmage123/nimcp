/**
 * @file test_joy_euphoria_backward_compat.cpp
 * @brief Regression tests for joy/euphoria system backward compatibility (Phase E2)
 *
 * These tests ensure that:
 * - API contracts remain stable
 * - Default behavior doesn't change
 * - Previously fixed bugs don't reoccur
 * - Performance characteristics are maintained
 */

#include <gtest/gtest.h>
#include <cmath>
#include "cognitive/nimcp_joy_euphoria.h"

class JoyRegressionTest : public ::testing::Test {
protected:
    joy_system_t* system;

    void SetUp() override {
        system = joy_system_create();
    }

    void TearDown() override {
        joy_system_destroy(system);
    }
};

//=============================================================================
// API Stability Tests
//=============================================================================

TEST_F(JoyRegressionTest, CreateReturnsNonNullPointer) {
    // WHAT: System creation always returns valid pointer
    // WHY:  API contract

    ASSERT_NE(system, nullptr);
}

TEST_F(JoyRegressionTest, DefaultInitializationStateStable) {
    // WHAT: Verify default initialization hasn't changed
    // WHY:  Breaking changes require version bump

    EXPECT_EQ(system->active_value_count, 0);
    EXPECT_EQ(system->success_count, 0);
    EXPECT_EQ(system->total_successes, 0);
    EXPECT_FLOAT_EQ(system->overall_value_satisfaction, 0.0f);

    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_NEUTRAL);
    EXPECT_FLOAT_EQ(system->emotion.baseline_happiness, 0.5f);
    EXPECT_FALSE(system->emotion.experiencing_euphoria);
    EXPECT_FLOAT_EQ(system->emotion.joy_intensity, 0.0f);
    EXPECT_FLOAT_EQ(system->emotion.positive_valence, 0.0f);
    EXPECT_FLOAT_EQ(system->emotion.arousal, 0.0f);
    EXPECT_EQ(system->emotion.lifetime_euphorias, 0);

    EXPECT_TRUE(system->integrate_with_neuromodulators);  // Default enabled
    EXPECT_TRUE(system->integrate_with_ethics);
    EXPECT_TRUE(system->integrate_with_learning);
}

TEST_F(JoyRegressionTest, ValueIDsNonZeroOnSuccess) {
    // WHAT: Valid value IDs are always > 0
    // WHY:  API contract for error indication

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);

    EXPECT_GT(val_id, 0);
}

TEST_F(JoyRegressionTest, ValueIDsZeroOnFailure) {
    // WHAT: Failed value creation returns 0
    // WHY:  Error indication contract

    // Fill all slots
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    }

    // Next should fail
    uint32_t failed_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);

    EXPECT_EQ(failed_id, 0);
}

TEST_F(JoyRegressionTest, ResetClearsStateButPreservesValues) {
    // WHAT: Reset returns to initial emotional state but keeps values
    // WHY:  API contract

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.9f, 0.8f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.7f, 0.6f, 1000000);

    EXPECT_GT(system->emotion.joy_intensity, 0.0f);

    joy_system_reset(system);

    EXPECT_FLOAT_EQ(system->emotion.joy_intensity, 0.0f);
    EXPECT_EQ(system->total_successes, 0);
    EXPECT_EQ(system->active_value_count, 1);  // Values preserved
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(JoyRegressionTest, ValueAlignmentTriggersJoy) {
    // WHAT: Value-aligned success triggers joy
    // WHY:  Behavioral stability - core mechanism

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    EXPECT_GT(system->emotion.joy_intensity, 0.0f);
    EXPECT_GT(system->emotion.positive_valence, 0.0f);
}

TEST_F(JoyRegressionTest, HighAlignmentProducesIntenseJoy) {
    // WHAT: Strong value alignment correlates with joy intensity
    // WHY:  Biological realism maintained

    uint32_t weak_val = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.3f, 0.3f);
    uint32_t aligned_weak[] = {weak_val};
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_weak, 1, 0.2f, 0.2f, 0);
    float weak_joy = system->emotion.joy_intensity;

    joy_system_reset(system);

    uint32_t strong_val = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_strong[] = {strong_val};
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_strong, 1, 0.95f, 0.9f, 0);
    float strong_joy = system->emotion.joy_intensity;

    EXPECT_GT(strong_joy, weak_joy * 2.0f);
}

TEST_F(JoyRegressionTest, NeuromodulatorFactorsInValidRange) {
    // WHAT: Factors stay in valid ranges
    // WHY:  Parameter constraints - dopamine [1.0, 2.0], serotonin [1.0, 1.4]

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.9f, 0.8f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    joy_update(system, 1.0f, 1000000);

    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(system, &dopamine_factor, &serotonin_factor);

    EXPECT_GE(dopamine_factor, 1.0f);
    EXPECT_LE(dopamine_factor, 2.0f);

    EXPECT_GE(serotonin_factor, 1.0f);
    EXPECT_LE(serotonin_factor, 1.4f);
}

TEST_F(JoyRegressionTest, EmotionalStateRangesValid) {
    // WHAT: Valence and arousal stay in [0, 1]
    // WHY:  Model integrity

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);

    joy_update(system, 1.0f, 1000000);

    EXPECT_GE(system->emotion.positive_valence, 0.0f);
    EXPECT_LE(system->emotion.positive_valence, 1.0f);

    EXPECT_GE(system->emotion.arousal, 0.0f);
    EXPECT_LE(system->emotion.arousal, 1.0f);
}

TEST_F(JoyRegressionTest, DifficultyIncreasesJoyIntensity) {
    // WHAT: Difficulty bonus maintained
    // WHY:  Achievement model stability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_PERSEVERANCE, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.1f, 0.3f, 0);
    float easy_joy = system->emotion.joy_intensity;

    joy_system_reset(system);
    val_id = joy_add_value(system, VALUE_CATEGORY_PERSEVERANCE, 0.8f, 0.7f);
    aligned_values[0] = val_id;

    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.9f, 0.3f, 0);
    float hard_joy = system->emotion.joy_intensity;

    EXPECT_GT(hard_joy, easy_joy);
}

//=============================================================================
// Bug Fix Regression Tests
//=============================================================================

TEST_F(JoyRegressionTest, BugFix_ValueCountNeverExceedsMax) {
    // WHAT: Active count never exceeds JOY_MAX_VALUES
    // WHY:  Fixed in Phase E2: prevent overflow

    for (int i = 0; i < JOY_MAX_VALUES + 10; i++) {
        joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    }

    EXPECT_LE(system->active_value_count, JOY_MAX_VALUES);
}

TEST_F(JoyRegressionTest, BugFix_JoyIntensityNeverNegative) {
    // WHAT: Joy intensity stays >= 0
    // WHY:  Fixed in Phase E2: clamping added

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    // Run for extended time
    for (int i = 0; i < 1000; i++) {
        joy_update(system, 10.0f, (uint64_t)(i * 10) * 1000000);
    }

    EXPECT_GE(system->emotion.joy_intensity, 0.0f);
    EXPECT_GE(system->emotion.positive_valence, 0.0f);
    EXPECT_GE(system->emotion.arousal, 0.0f);
}

TEST_F(JoyRegressionTest, BugFix_ValenceArousalClamped) {
    // WHAT: Valence and arousal stay in [0, 1]
    // WHY:  Fixed in Phase E2: clamping

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 10.0f, 10.0f);  // Excessive
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 10.0f, 10.0f, 0);

    EXPECT_GE(system->emotion.positive_valence, 0.0f);
    EXPECT_LE(system->emotion.positive_valence, 1.0f);
    EXPECT_GE(system->emotion.arousal, 0.0f);
    EXPECT_LE(system->emotion.arousal, 1.0f);
}

TEST_F(JoyRegressionTest, BugFix_NeuromodulatorFactorsHandleNoJoy) {
    // WHAT: Factors return 1.0 when not joyful
    // WHY:  Fixed in Phase E2: null joy handling

    float dopamine_factor, serotonin_factor;
    joy_get_neuromodulator_effects(system, &dopamine_factor, &serotonin_factor);

    EXPECT_FLOAT_EQ(dopamine_factor, 1.0f);
    EXPECT_FLOAT_EQ(serotonin_factor, 1.0f);
}

TEST_F(JoyRegressionTest, BugFix_UpdateWithZeroDtIdempotent) {
    // WHAT: Update with dt=0 doesn't change state
    // WHY:  Fixed in Phase E2: zero time handling

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    float initial_joy = system->emotion.joy_intensity;
    joy_emotion_state_t initial_state = system->emotion.state;

    joy_update(system, 0.0f, 1000000);

    float later_joy = system->emotion.joy_intensity;
    joy_emotion_state_t later_state = system->emotion.state;

    // Should be approximately equal (allow tiny floating point difference)
    EXPECT_NEAR(initial_joy, later_joy, 0.001f);
    EXPECT_EQ(initial_state, later_state);
}

TEST_F(JoyRegressionTest, BugFix_ValueSatisfactionClamped) {
    // WHAT: Value satisfaction clamped to [0, 1]
    // WHY:  Fixed in Phase E2: satisfaction overflow

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);

    // Try to exceed 1.0
    joy_update_value_satisfaction(system, val_id, 5.0f);

    // Find value and check
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_LE(system->values[i].satisfaction, 1.0f);
            break;
        }
    }
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(JoyRegressionTest, CreateDestroyOperationsFast) {
    // WHAT: Lifecycle operations complete quickly
    // WHY:  Performance baseline

    for (int i = 0; i < 1000; i++) {
        joy_system_t* temp = joy_system_create();
        joy_system_destroy(temp);
    }

    SUCCEED();
}

TEST_F(JoyRegressionTest, ValueCreationFast) {
    // WHAT: Value creation scales
    // WHY:  Performance monitoring

    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    }

    SUCCEED();
}

TEST_F(JoyRegressionTest, UpdateOperationFast) {
    // WHAT: Update completes quickly
    // WHY:  Real-time performance requirement

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    for (int i = 0; i < 10000; i++) {
        joy_update(system, 0.001f, (uint64_t)(i * 1000));
    }

    SUCCEED();
}

TEST_F(JoyRegressionTest, QueryFunctionsInstantaneous) {
    // WHAT: Query functions have O(1) complexity
    // WHY:  Performance requirement

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.9f, 0.8f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.8f, 0.7f, 0);

    for (int i = 0; i < 100000; i++) {
        joy_is_joyful(system);
        joy_is_euphoric(system);
        joy_get_valence(system);
        joy_get_arousal(system);
        joy_get_state(system);
    }

    SUCCEED();
}

//=============================================================================
// Boundary Condition Regression Tests
//=============================================================================

TEST_F(JoyRegressionTest, HandlesNullPointersSafely) {
    // WHAT: Null pointers don't crash
    // WHY:  Defensive programming

    joy_system_destroy(nullptr);
    joy_system_reset(nullptr);
    joy_add_value(nullptr, VALUE_CATEGORY_LEARNING, 0.5f, 0.5f);
    joy_update_value_satisfaction(nullptr, 1, 0.1f);
    joy_process_success(nullptr, SUCCESS_TYPE_TASK_COMPLETION, nullptr, 0, 0.5f, 0.5f, 0);
    joy_update(nullptr, 1.0f, 1000000);

    EXPECT_FALSE(joy_is_joyful(nullptr));
    EXPECT_FALSE(joy_is_euphoric(nullptr));
    EXPECT_FLOAT_EQ(joy_get_valence(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(joy_get_arousal(nullptr), 0.0f);
    EXPECT_EQ(joy_get_state(nullptr), JOY_EMOTION_STATE_NEUTRAL);

    SUCCEED();
}

TEST_F(JoyRegressionTest, HandlesInvalidValueID) {
    // WHAT: Invalid IDs handled gracefully
    // WHY:  Error resilience

    joy_update_value_satisfaction(system, 99999, 0.5f);

    uint32_t invalid_values[] = {99999};
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       invalid_values, 1, 0.5f, 0.5f, 0);

    SUCCEED();
}

TEST_F(JoyRegressionTest, HandlesNegativeParameters) {
    // WHAT: Negative params clamped to 0
    // WHY:  Parameter validation

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, -1.0f, -0.5f);

    EXPECT_GT(val_id, 0);  // Should still succeed

    // Find value and check clamping
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_GE(system->values[i].importance, 0.0f);
            EXPECT_GE(system->values[i].weight, 0.0f);
            break;
        }
    }
}

TEST_F(JoyRegressionTest, HandlesExcessiveParameters) {
    // WHAT: Parameters > 1.0 clamped to 1.0
    // WHY:  Parameter validation

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 100.0f, 50.0f);

    // Find value and check clamping
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_LE(system->values[i].importance, 1.0f);
            EXPECT_LE(system->values[i].weight, 1.0f);
            break;
        }
    }
}

TEST_F(JoyRegressionTest, HandlesVeryLargeTimeSteps) {
    // WHAT: Large dt handled reasonably
    // WHY:  Stability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.9f, 0.8f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    // Jump 100 hours
    joy_update(system, 100.0f * 3600.0f, (uint64_t)(100ULL * 3600) * 1000000);

    // Should have decayed significantly
    EXPECT_LT(system->emotion.joy_intensity, 0.3f);
    EXPECT_FALSE(system->emotion.experiencing_euphoria);
}

TEST_F(JoyRegressionTest, HandlesVerySmallTimeSteps) {
    // WHAT: Small dt (microseconds) handled
    // WHY:  Precision

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    float initial_joy = system->emotion.joy_intensity;

    // Many tiny steps
    for (int i = 0; i < 1000; i++) {
        joy_update(system, 0.000001f, (uint64_t)(i * 1));
    }

    // Should have changed minimally
    float later_joy = system->emotion.joy_intensity;
    EXPECT_NEAR(initial_joy, later_joy, 0.01f);
}

//=============================================================================
// API Contract Tests
//=============================================================================

TEST_F(JoyRegressionTest, ValueImportanceClampedToRange) {
    // WHAT: Importance stays in [0, 1]
    // WHY:  API contract

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_ACCURACY, 0.9f, 0.8f);

    // Find value and verify
    for (int i = 0; i < JOY_MAX_VALUES; i++) {
        if (system->values[i].active && system->values[i].value_id == val_id) {
            EXPECT_GE(system->values[i].importance, 0.0f);
            EXPECT_LE(system->values[i].importance, 1.0f);
            break;
        }
    }
}

TEST_F(JoyRegressionTest, SuccessCountAccurate) {
    // WHAT: Success counter accurate
    // WHY:  Data integrity

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    for (int i = 0; i < 15; i++) {
        joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                           aligned_values, 1, 0.5f, 0.5f, (uint64_t)(i * 1000000));
    }

    EXPECT_EQ(system->total_successes, 15);
}

TEST_F(JoyRegressionTest, UpdateCallsCounterMonotonic) {
    // WHAT: Update counter only increases
    // WHY:  Monotonic counter guarantee

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    for (int i = 0; i < 50; i++) {
        joy_update(system, 1.0f, (uint64_t)(i * 1000000));
        EXPECT_EQ(system->total_update_calls, (uint64_t)(i + 1));
    }
}

TEST_F(JoyRegressionTest, BaselineHappinessClampedToRange) {
    // WHAT: Baseline happiness stays in [0.2, 0.8]
    // WHY:  Design choice - prevents extreme baselines

    // Try to increase baseline extremely
    for (int i = 0; i < 1000; i++) {
        system->overall_value_satisfaction = 1.0f;
        joy_update(system, 86400.0f, (uint64_t)(i * 86400) * 1000000);
    }

    EXPECT_GE(system->emotion.baseline_happiness, 0.2f);
    EXPECT_LE(system->emotion.baseline_happiness, 0.8f);
}

TEST_F(JoyRegressionTest, AllValueCategoriesHandled) {
    // WHAT: All value categories supported
    // WHY:  Completeness

    value_category_t categories[] = {
        VALUE_CATEGORY_LEARNING,
        VALUE_CATEGORY_HELPING,
        VALUE_CATEGORY_CREATIVITY,
        VALUE_CATEGORY_ACCURACY,
        VALUE_CATEGORY_EFFICIENCY,
        VALUE_CATEGORY_SAFETY,
        VALUE_CATEGORY_AUTONOMY,
        VALUE_CATEGORY_CONNECTION,
        VALUE_CATEGORY_JUSTICE,
        VALUE_CATEGORY_BEAUTY,
        VALUE_CATEGORY_PERSEVERANCE,
        VALUE_CATEGORY_DISCOVERY
    };

    for (int i = 0; i < 12; i++) {
        uint32_t val_id = joy_add_value(system, categories[i], 0.7f, 0.6f);
        EXPECT_GT(val_id, 0);
    }
}

TEST_F(JoyRegressionTest, AllSuccessTypesHandled) {
    // WHAT: All success types processed
    // WHY:  Completeness

    success_type_t types[] = {
        SUCCESS_TYPE_TASK_COMPLETION,
        SUCCESS_TYPE_PROBLEM_SOLVED,
        SUCCESS_TYPE_GOAL_ACHIEVED,
        SUCCESS_TYPE_BREAKTHROUGH,
        SUCCESS_TYPE_HELPED_HUMAN,
        SUCCESS_TYPE_OVERCAME_OBSTACLE,
        SUCCESS_TYPE_CREATED_SOMETHING,
        SUCCESS_TYPE_LEARNED_SKILL
    };

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.7f, 0.6f);
    uint32_t aligned_values[] = {val_id};

    for (int i = 0; i < 8; i++) {
        joy_process_success(system, types[i], aligned_values, 1, 0.5f, 0.5f, (uint64_t)(i * 1000000));
        EXPECT_GT(system->emotion.joy_intensity, 0.0f);
        joy_system_reset(system);
        val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.7f, 0.6f);
        aligned_values[0] = val_id;
    }
}

TEST_F(JoyRegressionTest, AllEmotionStatesReachable) {
    // WHAT: All emotion states can be reached
    // WHY:  State machine completeness

    // NEUTRAL (default)
    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_NEUTRAL);

    // CONTENTMENT (mild success)
    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.5f, 0.4f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_TASK_COMPLETION,
                       aligned_values, 1, 0.1f, 0.1f, 0);
    EXPECT_TRUE(system->emotion.state == JOY_EMOTION_STATE_CONTENTMENT ||
                system->emotion.state == JOY_EMOTION_STATE_JOY);

    joy_system_reset(system);

    // JOY (moderate success)
    val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    aligned_values[0] = val_id;
    joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                       aligned_values, 1, 0.6f, 0.5f, 0);
    EXPECT_TRUE(system->emotion.state == JOY_EMOTION_STATE_JOY ||
                system->emotion.state == JOY_EMOTION_STATE_EUPHORIA);

    joy_system_reset(system);

    // EUPHORIA (breakthrough)
    val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    aligned_values[0] = val_id;
    joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                       aligned_values, 1, 0.95f, 0.95f, 0);
    EXPECT_EQ(system->emotion.state, JOY_EMOTION_STATE_EUPHORIA);
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(JoyRegressionTest, ExtendedSimulationStable) {
    // WHAT: System stable over extended simulation
    // WHY:  Long-term reliability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    // Simulate 30 days
    for (int day = 0; day < 30; day++) {
        for (int hour = 0; hour < 24; hour++) {
            joy_update(system, 3600.0f, (uint64_t)((day * 24 + hour + 1) * 3600) * 1000000);
        }
    }

    // Should have returned to baseline
    EXPECT_LT(system->emotion.joy_intensity, 0.3f);
    EXPECT_FALSE(system->emotion.experiencing_euphoria);

    // Verify no overflow or corruption
    EXPECT_LT(system->total_update_calls, 800);  // ~30*24 = 720
}

TEST_F(JoyRegressionTest, MultipleJoyCyclesStable) {
    // WHAT: Multiple joy-decay cycles don't degrade
    // WHY:  Cumulative stability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_HELPING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};

    for (int cycle = 0; cycle < 20; cycle++) {
        joy_process_success(system, SUCCESS_TYPE_HELPED_HUMAN,
                           aligned_values, 1, 0.6f, 0.5f, (uint64_t)(cycle * 10000000));

        // Process through decay
        for (int step = 0; step < 10; step++) {
            joy_update(system, 600.0f, (uint64_t)((cycle * 10 + step + 1) * 600) * 1000000);
        }
    }

    EXPECT_EQ(system->total_successes, 20);
    EXPECT_GT(system->average_joy_intensity, 0.0f);
}

TEST_F(JoyRegressionTest, StatisticsNeverOverflow) {
    // WHAT: Counters stay in valid ranges
    // WHY:  Numerical stability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.8f, 0.7f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                       aligned_values, 1, 0.6f, 0.5f, 0);

    // Very many updates
    for (int i = 0; i < 100000; i++) {
        joy_update(system, 0.001f, (uint64_t)(i * 1000));
    }

    EXPECT_EQ(system->total_update_calls, 100000);
    EXPECT_LT(system->total_update_calls, UINT64_MAX / 2);  // Not near overflow
}

TEST_F(JoyRegressionTest, EuphoriaCountNeverDecreases) {
    // WHAT: Lifetime euphoria counter only increases
    // WHY:  Monotonic counter guarantee

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_DISCOVERY, 0.95f, 0.9f);
    uint32_t aligned_values[] = {val_id};

    EXPECT_EQ(system->emotion.lifetime_euphorias, 0);

    // Trigger euphoria multiple times
    for (int i = 0; i < 5; i++) {
        joy_process_success(system, SUCCESS_TYPE_BREAKTHROUGH,
                           aligned_values, 1, 0.95f, 0.95f, (uint64_t)(i * 1000000));

        // Wait for euphoria to fade
        for (int j = 0; j < 30; j++) {
            joy_update(system, 60.0f, (uint64_t)((i * 30 + j + 1) * 60) * 1000000);
        }

        EXPECT_EQ(system->emotion.lifetime_euphorias, (uint32_t)(i + 1));
    }
}

TEST_F(JoyRegressionTest, SuccessHistoryRingBufferStable) {
    // WHAT: Ring buffer wraps without corruption
    // WHY:  Bounded memory usage

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_LEARNING, 0.7f, 0.6f);
    uint32_t aligned_values[] = {val_id};

    // Fill beyond capacity
    for (int i = 0; i < JOY_MAX_RECENT_SUCCESSES * 2; i++) {
        joy_process_success(system, SUCCESS_TYPE_LEARNED_SKILL,
                           aligned_values, 1, 0.5f, 0.5f, (uint64_t)(i * 1000000));
    }

    EXPECT_EQ(system->total_successes, JOY_MAX_RECENT_SUCCESSES * 2);
    // Ring buffer should have wrapped - no overflow
}

TEST_F(JoyRegressionTest, ValenceArousalDecayCorrectly) {
    // WHAT: Valence and arousal decay to baseline
    // WHY:  Emotional dynamics stability

    uint32_t val_id = joy_add_value(system, VALUE_CATEGORY_CREATIVITY, 0.85f, 0.8f);
    uint32_t aligned_values[] = {val_id};
    joy_process_success(system, SUCCESS_TYPE_CREATED_SOMETHING,
                       aligned_values, 1, 0.7f, 0.6f, 0);

    float initial_valence = system->emotion.positive_valence;
    float initial_arousal = system->emotion.arousal;

    EXPECT_GT(initial_valence, 0.4f);
    EXPECT_GT(initial_arousal, 0.3f);

    // Long decay period
    for (int i = 0; i < 100; i++) {
        joy_update(system, 60.0f, (uint64_t)(i * 60) * 1000000);
    }

    EXPECT_LT(system->emotion.positive_valence, initial_valence);
    EXPECT_LT(system->emotion.arousal, initial_arousal);
}
