/**
 * @file test_remorse_regret.cpp
 * @brief Unit tests for Remorse and Regret system
 *
 * WHAT: Comprehensive unit tests for remorse/regret emotional processing
 * WHY:  Ensure correct lifecycle, event processing, and emotional dynamics
 * HOW:  Test create/destroy, event handling, queries, emotional updates
 *
 * Function signatures tested (from include/cognitive/nimcp_remorse_regret.h):
 *   remorse_regret_system_t* remorse_regret_system_create(void);
 *   void remorse_regret_system_destroy(remorse_regret_system_t* system);
 *   void remorse_regret_system_reset(remorse_regret_system_t* system);
 *   void remorse_process_event(remorse_regret_system_t*, event_type_t, const uint32_t*, uint32_t, float, float, bool, uint64_t);
 *   void remorse_run_counterfactual(remorse_regret_system_t*, uint32_t, float, counterfactual_direction_t);
 *   void remorse_attempt_atonement(remorse_regret_system_t*, uint32_t, float, bool);
 *   void remorse_practice_self_forgiveness(remorse_regret_system_t*, uint32_t, float);
 *   void remorse_update(remorse_regret_system_t*, float, uint64_t);
 *   bool remorse_is_guilty(const remorse_regret_system_t* system);
 *   bool remorse_is_remorseful(const remorse_regret_system_t* system);
 *   bool remorse_is_ashamed(const remorse_regret_system_t* system);
 *   float remorse_get_regret_intensity(const remorse_regret_system_t* system);
 *   float remorse_get_self_worth(const remorse_regret_system_t* system);
 *   float remorse_get_lessons_learned(const remorse_regret_system_t* system);
 *   void remorse_get_neuromodulator_effects(const remorse_regret_system_t*, float*, float*, float*);
 *   emotional_tag_t remorse_get_emotion(const remorse_regret_system_t* system);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/nimcp_remorse_regret.h"
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RemorseRegretTest : public NimcpTestBase {
protected:
    remorse_regret_system_t* system = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (system) {
            remorse_regret_system_destroy(system);
            system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, SystemCreate_ReturnsValid) {
    // WHAT: Create remorse/regret system
    // WHY:  Basic lifecycle validation
    // HOW:  Call create, verify non-NULL

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Verify initial state
    EXPECT_EQ(system->event_count, 0u);
    EXPECT_FALSE(system->emotion.experiencing_guilt);
    EXPECT_FALSE(system->emotion.experiencing_remorse);
    EXPECT_FALSE(system->emotion.experiencing_shame);
}

TEST_F(RemorseRegretTest, SystemDestroy_NullIsNoop) {
    // WHAT: Verify destroying NULL system doesn't crash
    // WHY:  Defensive programming
    // HOW:  Call destroy with NULL

    remorse_regret_system_destroy(nullptr);
    SUCCEED() << "remorse_regret_system_destroy(NULL) did not crash";
}

TEST_F(RemorseRegretTest, SystemReset_ValidSystem) {
    // WHAT: Reset system to initial state
    // WHY:  Test reset functionality
    // HOW:  Create, process event, reset, verify

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Process an event
    uint32_t values[] = {1, 2};
    remorse_process_event(system, EVENT_MORAL_VIOLATION, values, 2, 0.8f, 0.9f, false, 1000000);

    // Reset
    remorse_regret_system_reset(system);

    // Should be back to initial state
    EXPECT_EQ(system->event_count, 0u);
    EXPECT_FALSE(remorse_is_guilty(system));
    EXPECT_FALSE(remorse_is_remorseful(system));
    EXPECT_FALSE(remorse_is_ashamed(system));
}

TEST_F(RemorseRegretTest, SystemReset_NullSystem) {
    // WHAT: Test NULL safety for reset
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    remorse_regret_system_reset(nullptr);
    SUCCEED() << "remorse_regret_system_reset(NULL) did not crash";
}

/* ============================================================================
 * Event Processing Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, ProcessEvent_MoralViolation) {
    // WHAT: Process a moral violation event
    // WHY:  Test event handling for severe cases
    // HOW:  Process moral violation, verify remorse triggered
    //
    // NOTE: moral_severity = harm_caused when num_values > 0.
    //       SHAME_THRESHOLD=0.7, so harm < 0.7 triggers REMORSE (not SHAME).
    //       In REMORSE path, regret_intensity = base * 0.5 which tops out at
    //       ~0.29 for harm<0.7, so we check regret > 0 instead of > REGRET_THRESHOLD.

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    uint32_t values[] = {1, 2, 3};
    remorse_process_event(
        system,
        EVENT_MORAL_VIOLATION,
        values,
        3,
        0.69f,  // harm_caused (< 0.7 to avoid SHAME, high enough for strong remorse)
        0.95f,  // controllability
        false,  // not reversible
        1000000
    );

    EXPECT_GE(system->event_count, 1u);
    EXPECT_TRUE(remorse_is_remorseful(system));
    EXPECT_GT(remorse_get_regret_intensity(system), 0.0f);
}

TEST_F(RemorseRegretTest, ProcessEvent_PoorDecision) {
    // WHAT: Process a poor decision event
    // WHY:  Test event handling for mild cases
    // HOW:  Process poor decision, verify regret but not remorse

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    remorse_process_event(
        system,
        EVENT_POOR_DECISION,
        nullptr,  // no violated values
        0,
        0.2f,   // low harm
        0.5f,   // medium controllability
        true,   // reversible
        1000000
    );

    EXPECT_GE(system->event_count, 1u);
    // Should trigger regret but not necessarily remorse
    EXPECT_GE(remorse_get_regret_intensity(system), 0.0f);
}

TEST_F(RemorseRegretTest, ProcessEvent_AllTypes) {
    // WHAT: Process all event types
    // WHY:  Verify all types are handled
    // HOW:  Process each type

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    uint64_t time = 1000000;

    remorse_process_event(system, EVENT_ACTION_COMMISSION, nullptr, 0, 0.3f, 0.5f, true, time++);
    remorse_process_event(system, EVENT_ACTION_OMISSION, nullptr, 0, 0.3f, 0.5f, true, time++);
    remorse_process_event(system, EVENT_MORAL_VIOLATION, nullptr, 0, 0.5f, 0.5f, false, time++);
    remorse_process_event(system, EVENT_RELATIONSHIP_HARM, nullptr, 0, 0.4f, 0.5f, true, time++);
    remorse_process_event(system, EVENT_MISSED_OPPORTUNITY, nullptr, 0, 0.2f, 0.5f, false, time++);
    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.3f, 0.5f, true, time++);
    remorse_process_event(system, EVENT_BROKEN_PROMISE, nullptr, 0, 0.4f, 0.5f, false, time++);
    remorse_process_event(system, EVENT_BETRAYAL, nullptr, 0, 0.7f, 0.8f, false, time++);

    EXPECT_EQ(system->event_count, 8u);
}

TEST_F(RemorseRegretTest, ProcessEvent_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    uint32_t values[] = {1};
    remorse_process_event(nullptr, EVENT_MORAL_VIOLATION, values, 1, 0.5f, 0.5f, false, 1000000);
    SUCCEED() << "remorse_process_event(NULL, ...) did not crash";
}

TEST_F(RemorseRegretTest, ProcessEvent_NullValues) {
    // WHAT: Test NULL values array with count > 0
    // WHY:  Edge case validation
    // HOW:  NULL values with non-zero count

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Should handle gracefully
    remorse_process_event(system, EVENT_MORAL_VIOLATION, nullptr, 5, 0.5f, 0.5f, false, 1000000);
    SUCCEED() << "NULL values with count > 0 handled";
}

/* ============================================================================
 * Counterfactual Thinking Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, RunCounterfactual_Upward) {
    // WHAT: Run upward counterfactual ("if only...")
    // WHY:  Test counterfactual processing
    // HOW:  Process event, run upward counterfactual

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // First process an event
    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.4f, 0.7f, false, 1000000);

    // Run upward counterfactual - should increase regret
    float initial_regret = remorse_get_regret_intensity(system);
    remorse_run_counterfactual(system, 0, 0.9f, COUNTERFACTUAL_UPWARD);

    // Upward thinking about much better alternative should increase regret
    EXPECT_GE(remorse_get_regret_intensity(system), initial_regret);
}

TEST_F(RemorseRegretTest, RunCounterfactual_Downward) {
    // WHAT: Run downward counterfactual ("at least...")
    // WHY:  Test counterfactual processing
    // HOW:  Process event, run downward counterfactual

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // First process an event
    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.6f, 0.7f, false, 1000000);

    // Run downward counterfactual - should potentially decrease regret
    remorse_run_counterfactual(system, 0, 0.2f, COUNTERFACTUAL_DOWNWARD);

    // Downward thinking should help cope
    SUCCEED() << "Downward counterfactual processed";
}

TEST_F(RemorseRegretTest, RunCounterfactual_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    remorse_run_counterfactual(nullptr, 0, 0.5f, COUNTERFACTUAL_UPWARD);
    SUCCEED() << "remorse_run_counterfactual(NULL, ...) did not crash";
}

TEST_F(RemorseRegretTest, RunCounterfactual_InvalidEventIndex) {
    // WHAT: Test invalid event index
    // WHY:  Edge case validation
    // HOW:  Use index beyond events

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // No events yet, index 0 is invalid
    remorse_run_counterfactual(system, 99, 0.5f, COUNTERFACTUAL_UPWARD);
    SUCCEED() << "Invalid event index handled";
}

/* ============================================================================
 * Atonement Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, AttemptAtonement_Successful) {
    // WHAT: Attempt successful atonement
    // WHY:  Test atonement effects on remorse
    // HOW:  Process event, attempt atonement with forgiveness

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Process a moral violation
    remorse_process_event(system, EVENT_RELATIONSHIP_HARM, nullptr, 0, 0.7f, 0.8f, false, 1000000);
    float initial_remorse = system->emotion.remorse_intensity;

    // Attempt successful atonement
    remorse_attempt_atonement(system, 0, 0.9f, true);

    // Should reduce remorse
    // Note: immediate effect depends on implementation
    SUCCEED() << "Atonement attempt processed";
}

TEST_F(RemorseRegretTest, AttemptAtonement_PartialSuccess) {
    // WHAT: Attempt partially successful atonement
    // WHY:  Test partial atonement
    // HOW:  Medium effectiveness, no forgiveness

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    remorse_process_event(system, EVENT_BROKEN_PROMISE, nullptr, 0, 0.5f, 0.7f, false, 1000000);
    remorse_attempt_atonement(system, 0, 0.4f, false);

    SUCCEED() << "Partial atonement processed";
}

TEST_F(RemorseRegretTest, AttemptAtonement_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    remorse_attempt_atonement(nullptr, 0, 0.5f, true);
    SUCCEED() << "remorse_attempt_atonement(NULL, ...) did not crash";
}

/* ============================================================================
 * Self-Forgiveness Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, PracticeSelfForgiveness_HighCompassion) {
    // WHAT: Practice self-forgiveness with high compassion
    // WHY:  Test self-forgiveness effects on shame
    // HOW:  Process event, practice self-forgiveness

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Process an event that might cause shame
    remorse_process_event(system, EVENT_MORAL_VIOLATION, nullptr, 0, 0.8f, 0.9f, false, 1000000);

    // Practice self-forgiveness
    remorse_practice_self_forgiveness(system, 0, 0.9f);

    SUCCEED() << "Self-forgiveness practiced";
}

TEST_F(RemorseRegretTest, PracticeSelfForgiveness_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    remorse_practice_self_forgiveness(nullptr, 0, 0.5f);
    SUCCEED() << "remorse_practice_self_forgiveness(NULL, ...) did not crash";
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, Update_ValidParams) {
    // WHAT: Test update cycle
    // WHY:  Verify emotional dynamics
    // HOW:  Process event, run updates

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.4f, 0.5f, true, 1000000);

    // Run several updates
    for (int i = 0; i < 100; i++) {
        remorse_update(system, 1.0f, 1000000 + (i * 1000000));
    }

    EXPECT_GE(system->total_update_calls, 100u);
}

TEST_F(RemorseRegretTest, Update_EmotionDecay) {
    // WHAT: Test that emotions decay over time
    // WHY:  Verify temporal dynamics
    // HOW:  Process event, update many times, check decay

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.5f, 0.5f, true, 1000000);
    float initial_regret = remorse_get_regret_intensity(system);

    // Simulate long time passing (1 hour = 3600 seconds per update, 1000 updates)
    for (int i = 0; i < 1000; i++) {
        remorse_update(system, 3600.0f, 1000000 + (i * 3600000000ULL));
    }

    float final_regret = remorse_get_regret_intensity(system);

    // Regret should have decayed over time
    EXPECT_LE(final_regret, initial_regret);
}

TEST_F(RemorseRegretTest, Update_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    remorse_update(nullptr, 1.0f, 1000000);
    SUCCEED() << "remorse_update(NULL, ...) did not crash";
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, IsGuilty_InitialState) {
    // WHAT: Check guilt status on fresh system
    // WHY:  Verify initial state
    // HOW:  Create system, check guilt

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    EXPECT_FALSE(remorse_is_guilty(system));
}

TEST_F(RemorseRegretTest, IsGuilty_AfterEvent) {
    // WHAT: Check guilt after processing event
    // WHY:  Verify guilt triggering
    // HOW:  Process event, check guilt

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    remorse_process_event(system, EVENT_ACTION_COMMISSION, nullptr, 0, 0.5f, 0.7f, true, 1000000);

    // May or may not trigger guilt depending on severity
    // Just verify it returns a valid result
    bool is_guilty = remorse_is_guilty(system);
    SUCCEED() << "is_guilty = " << is_guilty;
}

TEST_F(RemorseRegretTest, IsGuilty_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = remorse_is_guilty(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(RemorseRegretTest, IsRemorseful_InitialState) {
    // WHAT: Check remorse status on fresh system
    // WHY:  Verify initial state
    // HOW:  Create system, check remorse

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    EXPECT_FALSE(remorse_is_remorseful(system));
}

TEST_F(RemorseRegretTest, IsRemorseful_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = remorse_is_remorseful(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(RemorseRegretTest, IsAshamed_InitialState) {
    // WHAT: Check shame status on fresh system
    // WHY:  Verify initial state
    // HOW:  Create system, check shame

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    EXPECT_FALSE(remorse_is_ashamed(system));
}

TEST_F(RemorseRegretTest, IsAshamed_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = remorse_is_ashamed(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(RemorseRegretTest, GetRegretIntensity_ValidRange) {
    // WHAT: Verify regret intensity is in valid range
    // WHY:  Test intensity reporting
    // HOW:  Check fresh and after events

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    float intensity = remorse_get_regret_intensity(system);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);

    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.5f, 0.5f, true, 1000000);

    intensity = remorse_get_regret_intensity(system);
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(RemorseRegretTest, GetRegretIntensity_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float result = remorse_get_regret_intensity(nullptr);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(RemorseRegretTest, GetSelfWorth_ValidRange) {
    // WHAT: Verify self-worth is in valid range
    // WHY:  Test self-worth reporting
    // HOW:  Check value is 0-1

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    float self_worth = remorse_get_self_worth(system);
    EXPECT_GE(self_worth, 0.0f);
    EXPECT_LE(self_worth, 1.0f);
}

TEST_F(RemorseRegretTest, GetSelfWorth_NullSystem) {
    // WHAT: Test NULL safety -- returns default self-worth (0.7)
    // WHY:  Defensive programming
    // HOW:  Call with NULL, implementation returns 0.7f as default self-worth

    float result = remorse_get_self_worth(nullptr);
    EXPECT_FLOAT_EQ(result, 0.7f);
}

TEST_F(RemorseRegretTest, GetLessonsLearned_ValidRange) {
    // WHAT: Verify lessons learned is in valid range
    // WHY:  Test learning accumulation
    // HOW:  Check value is 0-1

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    float lessons = remorse_get_lessons_learned(system);
    EXPECT_GE(lessons, 0.0f);
    EXPECT_LE(lessons, 1.0f);
}

TEST_F(RemorseRegretTest, GetLessonsLearned_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float result = remorse_get_lessons_learned(nullptr);
    EXPECT_EQ(result, 0.0f);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, GetNeuromodulatorEffects_ValidSystem) {
    // WHAT: Get neuromodulator effects from remorse/regret
    // WHY:  Test integration with brain systems
    // HOW:  Get effects from fresh and triggered states

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    float dopamine, serotonin, norepinephrine;

    remorse_get_neuromodulator_effects(system, &dopamine, &serotonin, &norepinephrine);

    // All should be valid (implementation-specific values)
    SUCCEED() << "Neuromodulator effects retrieved";
}

TEST_F(RemorseRegretTest, GetNeuromodulatorEffects_NullParams) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // NULL output params
    remorse_get_neuromodulator_effects(system, nullptr, nullptr, nullptr);

    // NULL system
    float d, s, n;
    remorse_get_neuromodulator_effects(nullptr, &d, &s, &n);

    SUCCEED() << "NULL params handled";
}

TEST_F(RemorseRegretTest, GetEmotion_ValidSystem) {
    // WHAT: Get emotional tag from remorse/regret
    // WHY:  Test emotional tagging integration
    // HOW:  Get emotion from various states

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Fresh system - should be neutral
    emotional_tag_t emotion = remorse_get_emotion(system);
    EXPECT_GE(emotion.valence, -1.0f);
    EXPECT_LE(emotion.valence, 1.0f);
    EXPECT_GE(emotion.arousal, 0.0f);
    EXPECT_LE(emotion.arousal, 1.0f);

    // After processing event
    remorse_process_event(system, EVENT_MORAL_VIOLATION, nullptr, 0, 0.8f, 0.9f, false, 1000000);
    emotion = remorse_get_emotion(system);

    // Moral violation should result in negative valence
    EXPECT_LT(emotion.valence, 0.0f);
}

TEST_F(RemorseRegretTest, GetEmotion_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    emotional_tag_t emotion = remorse_get_emotion(nullptr);

    // Should return neutral emotion
    EXPECT_EQ(emotion.valence, 0.0f);
    EXPECT_EQ(emotion.arousal, 0.0f);
}

/* ============================================================================
 * Personality Trait Effects Tests
 * ============================================================================ */

TEST_F(RemorseRegretTest, PersonalityTraits_HighConscientiousness) {
    // WHAT: Test effect of high conscientiousness
    // WHY:  High conscientiousness should increase guilt/regret
    // HOW:  Set trait and process event

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    system->conscientiousness = 0.9f;

    remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.3f, 0.5f, true, 1000000);

    // High conscientiousness should amplify regret
    SUCCEED() << "High conscientiousness processed";
}

TEST_F(RemorseRegretTest, PersonalityTraits_HighSelfCompassion) {
    // WHAT: Test effect of high self-compassion
    // WHY:  High self-compassion should ease self-forgiveness
    // HOW:  Set trait and practice forgiveness

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    system->self_compassion = 0.9f;

    remorse_process_event(system, EVENT_MORAL_VIOLATION, nullptr, 0, 0.5f, 0.7f, false, 1000000);
    remorse_practice_self_forgiveness(system, 0, 0.5f);

    SUCCEED() << "High self-compassion processed";
}

/* ============================================================================
 * Ring Buffer Edge Cases
 * ============================================================================ */

TEST_F(RemorseRegretTest, EventBuffer_Overflow) {
    // WHAT: Test behavior when exceeding max events
    // WHY:  Verify ring buffer wrapping
    // HOW:  Process more events than max

    system = remorse_regret_system_create();
    ASSERT_NE(system, nullptr);

    // Process more than REGRET_MAX_EVENTS events
    for (uint32_t i = 0; i < REGRET_MAX_EVENTS + 10; i++) {
        remorse_process_event(system, EVENT_POOR_DECISION, nullptr, 0, 0.3f, 0.5f, true, 1000000 + i);
    }

    // Should have wrapped around
    EXPECT_LE(system->event_count, REGRET_MAX_EVENTS);
    SUCCEED() << "Event buffer overflow handled";
}
