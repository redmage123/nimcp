/**
 * @file test_shadow_emotions.cpp
 * @brief Unit tests for Shadow Emotions module
 *
 * WHAT: Comprehensive unit tests for shadow emotion monitoring and self-correction
 * WHY:  Ensure correct lifecycle, emotion tracking, and intervention functions
 * HOW:  Test create/destroy, emotion experience, queries, interventions
 *
 * Function signatures tested (from include/cognitive/nimcp_shadow_emotions.h):
 *   shadow_emotion_system_t* shadow_system_create(uint32_t max_others_tracked);
 *   void shadow_system_destroy(shadow_emotion_system_t* system);
 *   void shadow_system_reset(shadow_emotion_system_t* system);
 *   void shadow_update(shadow_emotion_system_t* system, float dt, uint64_t current_time);
 *   void shadow_experience_jealousy(shadow_emotion_system_t*, uint32_t bond_id, float threat, float attachment, uint64_t time);
 *   void shadow_experience_envy(shadow_emotion_system_t*, uint32_t target_id, float self_level, float other_level, float maliciousness, uint64_t time);
 *   void shadow_register_obsession(shadow_emotion_system_t*, uint32_t thought_id, obsession_target_type_t type, float intensity, float distress, uint64_t time);
 *   void shadow_assess_hubris(shadow_emotion_system_t*, float success_count, float power_level, float accountability);
 *   void shadow_assess_greed(shadow_emotion_system_t*, float value, float necessity, float scarcity, uint64_t time);
 *   void shadow_assess_narcissism(shadow_emotion_system_t*, float grandiosity, float empathy, float admiration, float entitlement);
 *   bool shadow_is_active(const shadow_emotion_system_t*, shadow_emotion_type_t emotion);
 *   float shadow_get_intensity(const shadow_emotion_system_t*, shadow_emotion_type_t emotion);
 *   float shadow_get_mental_health_impact(const shadow_emotion_system_t* system);
 *   float shadow_get_insight(const shadow_emotion_system_t* system);
 *   bool shadow_is_correcting(const shadow_emotion_system_t* system);
 *   bool shadow_apply_intervention(shadow_emotion_system_t*, shadow_emotion_type_t emotion, shadow_intervention_type_t strategy, uint64_t time);
 *   bool shadow_auto_intervene(shadow_emotion_system_t* system, uint64_t current_time);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/nimcp_shadow_emotions.h"
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ShadowEmotionsTest : public NimcpTestBase {
protected:
    shadow_emotion_system_t* system = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (system) {
            shadow_system_destroy(system);
            system = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, SystemCreate_ValidParams) {
    // WHAT: Create shadow emotion system
    // WHY:  Basic lifecycle validation
    // HOW:  Call create with valid params

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    EXPECT_EQ(system->max_others_tracked, 8u);
}

TEST_F(ShadowEmotionsTest, SystemCreate_ZeroOthers) {
    // WHAT: Create with zero others tracking
    // WHY:  Edge case validation
    // HOW:  Create with max_others=0

    system = shadow_system_create(0);
    // Should either succeed with minimum or fail
    // Either way, shouldn't crash
    SUCCEED() << "Zero others tracking handled";
}

TEST_F(ShadowEmotionsTest, SystemDestroy_NullIsNoop) {
    // WHAT: Verify destroying NULL system doesn't crash
    // WHY:  Defensive programming
    // HOW:  Call destroy with NULL

    shadow_system_destroy(nullptr);
    SUCCEED() << "shadow_system_destroy(NULL) did not crash";
}

TEST_F(ShadowEmotionsTest, SystemReset_ValidSystem) {
    // WHAT: Reset system to initial state
    // WHY:  Test reset functionality
    // HOW:  Create, trigger some emotions, reset, verify

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Trigger some emotions
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    shadow_assess_hubris(system, 10.0f, 0.9f, 0.1f);

    // Reset
    shadow_system_reset(system);

    // Should be back to baseline
    EXPECT_FALSE(shadow_is_active(system, SHADOW_JEALOUSY));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_HUBRIS));
}

TEST_F(ShadowEmotionsTest, SystemReset_NullSystem) {
    // WHAT: Test NULL safety for reset
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_system_reset(nullptr);
    SUCCEED() << "shadow_system_reset(NULL) did not crash";
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, Update_ValidParams) {
    // WHAT: Test update cycle
    // WHY:  Verify regular update works
    // HOW:  Create system, call update

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_update(system, 0.016f, 1000000); // 16ms at 1s
    EXPECT_GE(system->total_update_calls, 1u);
}

TEST_F(ShadowEmotionsTest, Update_NullSystem) {
    // WHAT: Test NULL safety for update
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_update(nullptr, 0.016f, 1000000);
    SUCCEED() << "shadow_update(NULL, ...) did not crash";
}

/* ============================================================================
 * Jealousy Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, ExperienceJealousy_HighThreat) {
    // WHAT: Trigger jealousy with high threat
    // WHY:  Test jealousy activation
    // HOW:  High threat, high attachment should trigger jealousy

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_experience_jealousy(system, 1, 0.9f, 0.9f, 1000000);

    EXPECT_TRUE(shadow_is_active(system, SHADOW_JEALOUSY));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_JEALOUSY), SHADOW_JEALOUSY_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, ExperienceJealousy_LowThreat) {
    // WHAT: Low threat should not trigger jealousy
    // WHY:  Verify threshold behavior
    // HOW:  Low threat level

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_experience_jealousy(system, 1, 0.1f, 0.9f, 1000000);

    // Low threat should not activate jealousy
    EXPECT_LT(shadow_get_intensity(system, SHADOW_JEALOUSY), SHADOW_JEALOUSY_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, ExperienceJealousy_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_experience_jealousy(nullptr, 1, 0.9f, 0.9f, 1000000);
    SUCCEED() << "shadow_experience_jealousy(NULL, ...) did not crash";
}

/* ============================================================================
 * Envy Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, ExperienceEnvy_LargeDiscrepancy) {
    // WHAT: Trigger envy with large competence discrepancy
    // WHY:  Test envy activation
    // HOW:  Other much better than self

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_experience_envy(system, 2, 0.3f, 0.9f, 0.5f, 1000000);

    EXPECT_TRUE(shadow_is_active(system, SHADOW_ENVY));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_ENVY), SHADOW_ENVY_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, ExperienceEnvy_SmallDiscrepancy) {
    // WHAT: Small discrepancy should not trigger envy
    // WHY:  Verify threshold behavior
    // HOW:  Similar competence levels

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_experience_envy(system, 2, 0.8f, 0.85f, 0.0f, 1000000);

    // Small gap should not strongly activate
    EXPECT_LT(shadow_get_intensity(system, SHADOW_ENVY), SHADOW_ENVY_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, ExperienceEnvy_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_experience_envy(nullptr, 2, 0.3f, 0.9f, 0.5f, 1000000);
    SUCCEED() << "shadow_experience_envy(NULL, ...) did not crash";
}

/* ============================================================================
 * Obsession Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, RegisterObsession_HighIntensity) {
    // WHAT: Register high-intensity obsessive thought
    // WHY:  Test obsession tracking
    // HOW:  High intensity and distress

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_register_obsession(system, 1, OBSESSION_THOUGHT, 0.9f, 0.8f, 1000000);

    EXPECT_TRUE(shadow_is_active(system, SHADOW_OBSESSION));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_OBSESSION), SHADOW_OBSESSION_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, RegisterObsession_LowIntensity) {
    // WHAT: Low intensity thought should not trigger obsession
    // WHY:  Verify threshold behavior
    // HOW:  Low intensity

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_register_obsession(system, 1, OBSESSION_THOUGHT, 0.2f, 0.1f, 1000000);

    EXPECT_LT(shadow_get_intensity(system, SHADOW_OBSESSION), SHADOW_OBSESSION_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, RegisterObsession_AllTypes) {
    // WHAT: Test all obsession target types
    // WHY:  Verify all types work
    // HOW:  Register each type

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_register_obsession(system, 1, OBSESSION_PERSON, 0.5f, 0.5f, 1000000);
    shadow_register_obsession(system, 2, OBSESSION_GOAL, 0.5f, 0.5f, 1000001);
    shadow_register_obsession(system, 3, OBSESSION_THOUGHT, 0.5f, 0.5f, 1000002);
    shadow_register_obsession(system, 4, OBSESSION_BEHAVIOR, 0.5f, 0.5f, 1000003);

    SUCCEED() << "All obsession types registered";
}

TEST_F(ShadowEmotionsTest, RegisterObsession_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_register_obsession(nullptr, 1, OBSESSION_THOUGHT, 0.9f, 0.8f, 1000000);
    SUCCEED() << "shadow_register_obsession(NULL, ...) did not crash";
}

/* ============================================================================
 * Hubris Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, AssessHubris_HighPowerLowAccountability) {
    // WHAT: High power with low accountability triggers hubris
    // WHY:  Test hubris detection
    // HOW:  High success, power, low accountability

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_assess_hubris(system, 10.0f, 0.95f, 0.1f);

    EXPECT_TRUE(shadow_is_active(system, SHADOW_HUBRIS));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_HUBRIS), SHADOW_HUBRIS_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessHubris_HighAccountability) {
    // WHAT: High accountability should prevent hubris
    // WHY:  Test accountability buffer
    // HOW:  High accountability despite power

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_assess_hubris(system, 5.0f, 0.5f, 0.9f);

    // High accountability should buffer hubris
    EXPECT_LT(shadow_get_intensity(system, SHADOW_HUBRIS), SHADOW_HUBRIS_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessHubris_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_assess_hubris(nullptr, 10.0f, 0.95f, 0.1f);
    SUCCEED() << "shadow_assess_hubris(NULL, ...) did not crash";
}

/* ============================================================================
 * Greed Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, AssessGreed_HighValueLowNecessity) {
    // WHAT: Repeated high value acquisition with low necessity triggers greed
    // WHY:  Test greed detection -- intensity accumulates over multiple acquisitions
    // HOW:  High value, low necessity, repeated calls to exceed threshold

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Each call adds ~0.072 intensity; need multiple acquisitions to exceed 0.5 threshold
    for (int i = 0; i < 10; i++) {
        shadow_assess_greed(system, 0.9f, 0.1f, 0.2f, (uint64_t)(1000000 + i * 100000));
    }

    EXPECT_TRUE(shadow_is_active(system, SHADOW_GREED));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_GREED), SHADOW_GREED_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessGreed_HighNecessity) {
    // WHAT: High necessity should not trigger greed
    // WHY:  Necessity justifies acquisition
    // HOW:  High necessity

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_assess_greed(system, 0.9f, 0.9f, 0.2f, 1000000);

    // High necessity should buffer greed
    EXPECT_LT(shadow_get_intensity(system, SHADOW_GREED), SHADOW_GREED_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessGreed_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_assess_greed(nullptr, 0.9f, 0.1f, 0.2f, 1000000);
    SUCCEED() << "shadow_assess_greed(NULL, ...) did not crash";
}

/* ============================================================================
 * Narcissism Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, AssessNarcissism_HighGrandiosity) {
    // WHAT: High grandiosity with low empathy triggers narcissism
    // WHY:  Test narcissism detection
    // HOW:  High grandiosity, low empathy

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_assess_narcissism(system, 0.9f, 0.1f, 0.9f, 0.8f);

    EXPECT_TRUE(shadow_is_active(system, SHADOW_NARCISSISM));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_NARCISSISM), SHADOW_NARCISSISM_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessNarcissism_HealthyEmpathy) {
    // WHAT: Healthy empathy should prevent narcissism
    // WHY:  Empathy buffers narcissistic traits
    // HOW:  High empathy

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    shadow_assess_narcissism(system, 0.5f, 0.9f, 0.3f, 0.2f);

    // High empathy should buffer narcissism
    EXPECT_LT(shadow_get_intensity(system, SHADOW_NARCISSISM), SHADOW_NARCISSISM_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AssessNarcissism_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    shadow_assess_narcissism(nullptr, 0.9f, 0.1f, 0.9f, 0.8f);
    SUCCEED() << "shadow_assess_narcissism(NULL, ...) did not crash";
}

/* ============================================================================
 * Query Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, IsActive_AllTypes) {
    // WHAT: Test is_active for all emotion types
    // WHY:  Verify query works for all types
    // HOW:  Check each type

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Fresh system should have no active emotions
    EXPECT_FALSE(shadow_is_active(system, SHADOW_JEALOUSY));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_ENVY));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_OBSESSION));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_HUBRIS));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_GREED));
    EXPECT_FALSE(shadow_is_active(system, SHADOW_NARCISSISM));
}

TEST_F(ShadowEmotionsTest, IsActive_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = shadow_is_active(nullptr, SHADOW_JEALOUSY);
    EXPECT_FALSE(result);
}

TEST_F(ShadowEmotionsTest, GetIntensity_AllTypes) {
    // WHAT: Get intensity for all emotion types
    // WHY:  Verify query works for all types
    // HOW:  Check each type

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Fresh system should have zero intensity
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_JEALOUSY), 0.0f);
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_ENVY), 0.0f);
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_OBSESSION), 0.0f);
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_HUBRIS), 0.0f);
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_GREED), 0.0f);
    EXPECT_EQ(shadow_get_intensity(system, SHADOW_NARCISSISM), 0.0f);
}

TEST_F(ShadowEmotionsTest, GetIntensity_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float result = shadow_get_intensity(nullptr, SHADOW_JEALOUSY);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(ShadowEmotionsTest, GetMentalHealthImpact_ValidSystem) {
    // WHAT: Get mental health impact
    // WHY:  Test aggregate impact measure
    // HOW:  Create system with no active emotions

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    float impact = shadow_get_mental_health_impact(system);

    // Fresh system should have minimal impact
    EXPECT_GE(impact, 0.0f);
    EXPECT_LE(impact, 1.0f);
}

TEST_F(ShadowEmotionsTest, GetMentalHealthImpact_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float result = shadow_get_mental_health_impact(nullptr);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(ShadowEmotionsTest, GetInsight_ValidSystem) {
    // WHAT: Get self-awareness/insight level
    // WHY:  Test insight measure
    // HOW:  Query insight level

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    float insight = shadow_get_insight(system);

    EXPECT_GE(insight, 0.0f);
    EXPECT_LE(insight, 1.0f);
}

TEST_F(ShadowEmotionsTest, GetInsight_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    float result = shadow_get_insight(nullptr);
    EXPECT_EQ(result, 0.0f);
}

TEST_F(ShadowEmotionsTest, IsCorrecting_ValidSystem) {
    // WHAT: Check if system is in self-correction mode
    // WHY:  Test correction status query
    // HOW:  Query correction status

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    bool correcting = shadow_is_correcting(system);

    // Fresh system should not be correcting
    EXPECT_FALSE(correcting);
}

TEST_F(ShadowEmotionsTest, IsCorrecting_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = shadow_is_correcting(nullptr);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Intervention Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, ApplyIntervention_CognitiveReframe) {
    // WHAT: Apply cognitive reframing intervention
    // WHY:  Test intervention application
    // HOW:  Trigger jealousy (cognitive reframe handles jealousy/hubris), apply intervention

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Trigger jealousy (cognitive reframe specifically handles jealousy and hubris)
    shadow_experience_jealousy(system, 1, 0.9f, 0.9f, 1000000);
    ASSERT_TRUE(shadow_is_active(system, SHADOW_JEALOUSY));

    // Apply intervention
    bool applied = shadow_apply_intervention(
        system,
        SHADOW_JEALOUSY,
        SHADOW_INTERVENTION_COGNITIVE_REFRAME,
        2000000
    );

    EXPECT_TRUE(applied);
    EXPECT_TRUE(shadow_is_correcting(system));
}

TEST_F(ShadowEmotionsTest, ApplyIntervention_AllStrategies) {
    // WHAT: Test all intervention strategies
    // WHY:  Verify all strategies work
    // HOW:  Apply each strategy

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Trigger emotion
    shadow_experience_jealousy(system, 1, 0.9f, 0.9f, 1000000);

    // Try all strategies
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_COGNITIVE_REFRAME, 2000000);
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_MINDFULNESS, 3000000);
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_PERSPECTIVE_TAKING, 4000000);
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_GRATITUDE, 5000000);
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_REALITY_TESTING, 6000000);
    shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_EXPOSURE, 7000000);

    SUCCEED() << "All intervention strategies applied";
}

TEST_F(ShadowEmotionsTest, ApplyIntervention_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = shadow_apply_intervention(
        nullptr,
        SHADOW_ENVY,
        SHADOW_INTERVENTION_COGNITIVE_REFRAME,
        1000000
    );
    EXPECT_FALSE(result);
}

TEST_F(ShadowEmotionsTest, AutoIntervene_ValidSystem) {
    // WHAT: Test automatic intervention selection
    // WHY:  Test auto-intervention
    // HOW:  Trigger emotion, call auto_intervene

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // Trigger multiple emotions
    shadow_experience_jealousy(system, 1, 0.9f, 0.9f, 1000000);
    shadow_assess_hubris(system, 10.0f, 0.95f, 0.1f);

    bool intervened = shadow_auto_intervene(system, 2000000);

    // Should have intervened on the most severe
    // (Result depends on implementation priorities)
    SUCCEED() << "Auto intervention completed (result: " << intervened << ")";
}

TEST_F(ShadowEmotionsTest, AutoIntervene_NoActiveEmotions) {
    // WHAT: Auto-intervene when no emotions active
    // WHY:  Test edge case
    // HOW:  Fresh system

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    bool intervened = shadow_auto_intervene(system, 1000000);

    // No active emotions, should return false
    EXPECT_FALSE(intervened);
}

TEST_F(ShadowEmotionsTest, AutoIntervene_NullSystem) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = shadow_auto_intervene(nullptr, 1000000);
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(ShadowEmotionsTest, GetNeuromodulatorEffects_ValidSystem) {
    // WHAT: Get neuromodulator effects from shadow emotions
    // WHY:  Test integration with brain systems
    // HOW:  Trigger emotions, get effects

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    float dopamine, serotonin, cortisol;

    // Fresh system should have neutral effects
    shadow_get_neuromodulator_effects(system, &dopamine, &serotonin, &cortisol);

    EXPECT_GE(dopamine, 0.0f);
    EXPECT_GE(serotonin, 0.0f);
    EXPECT_GE(cortisol, 0.0f);

    // Trigger jealousy - should affect cortisol
    shadow_experience_jealousy(system, 1, 0.9f, 0.9f, 1000000);
    shadow_get_neuromodulator_effects(system, &dopamine, &serotonin, &cortisol);

    SUCCEED() << "Neuromodulator effects retrieved";
}

TEST_F(ShadowEmotionsTest, GetNeuromodulatorEffects_NullParams) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // NULL system
    float d, s, c;
    shadow_get_neuromodulator_effects(nullptr, &d, &s, &c);

    // NULL output params - should not crash
    shadow_get_neuromodulator_effects(system, nullptr, nullptr, nullptr);
    SUCCEED() << "NULL params handled";
}

TEST_F(ShadowEmotionsTest, GetInteractionModulation_ValidParams) {
    // WHAT: Get interaction modulation for a person
    // WHY:  Test protective measures
    // HOW:  Call with valid params

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    float empathy_mod, trust_mod, engagement_mod;

    shadow_get_interaction_modulation(system, 1, &empathy_mod, &trust_mod, &engagement_mod);

    // All should be in valid range
    EXPECT_GE(empathy_mod, 0.0f);
    EXPECT_GE(trust_mod, 0.0f);
    EXPECT_GE(engagement_mod, 0.0f);
}

TEST_F(ShadowEmotionsTest, GetInteractionModulation_NullParams) {
    // WHAT: Test NULL safety
    // WHY:  Defensive programming
    // HOW:  Call with NULL params

    system = shadow_system_create(8);
    ASSERT_NE(system, nullptr);

    // NULL output params
    shadow_get_interaction_modulation(system, 1, nullptr, nullptr, nullptr);

    // NULL system
    float e, t, g;
    shadow_get_interaction_modulation(nullptr, 1, &e, &t, &g);

    SUCCEED() << "NULL params handled";
}
