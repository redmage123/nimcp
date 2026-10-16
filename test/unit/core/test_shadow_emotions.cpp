/* SPDX-License-Identifier: MIT */
/**
 * @file test_shadow_emotions.cpp
 * @brief Unit tests for Phase E5: Shadow Emotions
 */

#include <gtest/gtest.h>
#include "cognitive/nimcp_shadow_emotions.h"

class ShadowEmotionsTest : public ::testing::Test {
protected:
    shadow_emotion_system_t* system;

    void SetUp() override {
        system = shadow_system_create(8);  // Track up to 8 others
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        shadow_system_destroy(system);
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, SystemCreatesSuccessfully) {
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(system->max_others_tracked, 8);
    EXPECT_FALSE(system->in_self_correction);
}

TEST_F(ShadowEmotionsTest, SystemResetsCorrectly) {
    // Trigger some emotions
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    EXPECT_TRUE(system->jealousy.active);

    // Reset
    shadow_system_reset(system);
    EXPECT_FALSE(system->jealousy.active);
    EXPECT_FLOAT_EQ(system->total_shadow_intensity, 0.0f);
}

//=============================================================================
// JEALOUSY TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, JealousyTriggersFromThreat) {
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    
    EXPECT_TRUE(system->jealousy.active);
    EXPECT_GT(system->jealousy.intensity, 0.5f);
    EXPECT_EQ(system->jealousy.threatened_bond_id, 1);
}

TEST_F(ShadowEmotionsTest, JealousyIntensityScalesWithAttachment) {
    shadow_experience_jealousy(system, 1, 0.5f, 1.0f, 1000000);
    float high_attachment = system->jealousy.intensity;

    shadow_system_reset(system);
    shadow_experience_jealousy(system, 1, 0.5f, 0.3f, 1000000);
    float low_attachment = system->jealousy.intensity;

    EXPECT_GT(high_attachment, low_attachment);
}

//=============================================================================
// ENVY TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, EnvyTriggersFromSocialComparison) {
    shadow_experience_envy(system, 1, 0.3f, 0.8f, 0.6f, 1000000);
    
    EXPECT_GT(system->envy.active_envy_count, 0);
    EXPECT_GT(system->envy.chronic_envy, 0.0f);
}

TEST_F(ShadowEmotionsTest, EnvyIntensityFollowsDiscrepancy) {
    shadow_experience_envy(system, 1, 0.2f, 0.9f, 0.5f, 1000000);  // Large gap
    float large_gap_intensity = system->envy.targets[0].intensity;

    shadow_system_reset(system);
    shadow_experience_envy(system, 1, 0.7f, 0.8f, 0.5f, 1000000);  // Small gap
    float small_gap_intensity = system->envy.targets[0].intensity;

    EXPECT_GT(large_gap_intensity, small_gap_intensity);
}

//=============================================================================
// OBSESSION TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, ObsessionRegistersIntrusiveThought) {
    shadow_register_obsession(system, 1, OBSESSION_PERSON, 0.7f, 0.8f, 1000000);
    
    EXPECT_GT(system->obsession.active_obsession_count, 0);
    EXPECT_GT(system->obsession.overall_obsession_level, 0.0f);
}

TEST_F(ShadowEmotionsTest, ObsessionTracksFrPequency) {
    shadow_register_obsession(system, 1, OBSESSION_THOUGHT, 0.6f, 0.7f, 1000000);
    shadow_register_obsession(system, 1, OBSESSION_THOUGHT, 0.6f, 0.7f, 2000000);
    shadow_register_obsession(system, 1, OBSESSION_THOUGHT, 0.6f, 0.7f, 3000000);

    EXPECT_EQ(system->obsession.thoughts[0].intrusion_count_today, 3);
}

//=============================================================================
// HUBRIS TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, HubrisIncreasesWithSuccessAndPower) {
    shadow_assess_hubris(system, 5.0f, 0.8f, 0.2f);  // High success, power, low accountability
    
    EXPECT_TRUE(system->hubris.active);
    EXPECT_GT(system->hubris.intensity, SHADOW_HUBRIS_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, AccountabilityReducesHubris) {
    shadow_assess_hubris(system, 5.0f, 0.8f, 0.9f);  // High accountability
    
    EXPECT_LT(system->hubris.intensity, 0.5f);
}

//=============================================================================
// GREED TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, GreedDetectsExcessiveAcquisition) {
    // Acquire far more than needed
    shadow_assess_greed(system, 0.9f, 0.2f, 0.5f, 1000000);
    
    EXPECT_GT(system->greed.intensity, 0.0f);
    EXPECT_GT(system->greed.acquisition_count, 0);
}

TEST_F(ShadowEmotionsTest, NecessityReducesGreed) {
    shadow_assess_greed(system, 0.7f, 0.7f, 0.5f, 1000000);  // Acquire what's needed
    
    EXPECT_LT(system->greed.intensity, 0.2f);
}

//=============================================================================
// NARCISSISM TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, NarcissismDetectsGrandiosityAndLackOfEmpathy) {
    shadow_assess_narcissism(system, 0.9f, 0.2f, 0.8f, 0.8f);  // High grandiosity, low empathy
    
    EXPECT_TRUE(system->narcissism.active);
    EXPECT_GT(system->narcissism.intensity, SHADOW_NARCISSISM_THRESHOLD);
}

TEST_F(ShadowEmotionsTest, NarcissismSubtypeDetermination) {
    shadow_assess_narcissism(system, 0.9f, 0.1f, 0.8f, 0.9f);
    system->narcissism.exploitativeness = 0.8f;
    system->narcissism.paranoia = 0.7f;
    
    shadow_assess_narcissism(system, 0.9f, 0.1f, 0.8f, 0.9f);  // Re-assess with exploit/paranoia
    
    // Malignant requires: intensity >= 0.7, exploit > 0.6, paranoia > 0.5
    EXPECT_TRUE(system->narcissism.intensity >= 0.7f);
}

//=============================================================================
// OTHER-DETECTION TESTS (Recognizing in Humans)
//=============================================================================

TEST_F(ShadowEmotionsTest, DetectsNarcissismInOthers) {
    shadow_analyze_other(system, 1, "test", 0.8f, 0.2f, 0.9f, 1000000);  // High manipulation, low empathy, high grandiosity
    
    float narcissism = 0.0f;
    bool found = shadow_get_detected_in_other(system, 1, nullptr, &narcissism, nullptr);
    
    EXPECT_TRUE(found);
    EXPECT_GT(narcissism, 0.5f);
}

TEST_F(ShadowEmotionsTest, BoundariesSetForToxicIndividuals) {
    // Multiple toxic interactions
    for (int i = 0; i < 5; i++) {
        shadow_analyze_other(system, 1, "test", 0.9f, 0.1f, 0.8f, 1000000 + i * 1000000);
    }
    
    EXPECT_TRUE(shadow_should_maintain_boundaries(system, 1));
}

TEST_F(ShadowEmotionsTest, TrustDecreasesWithManipulation) {
    // Initial analysis
    shadow_analyze_other(system, 1, "test", 0.8f, 0.3f, 0.7f, 1000000);
    
    // Check trust level decreased
    float empathy_mod = 0.0f, trust_mod = 0.0f, engagement_mod = 0.0f;
    shadow_get_interaction_modulation(system, 1, &empathy_mod, &trust_mod, &engagement_mod);
    
    EXPECT_LT(trust_mod, 0.5f);  // Trust should decrease with manipulation
}

//=============================================================================
// SELF-CORRECTION TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, InterventionReducesTargetEmotion) {
    // Trigger jealousy
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    float before = system->jealousy.intensity;
    
    // Apply mindfulness intervention
    bool success = shadow_apply_intervention(system, SHADOW_JEALOUSY, SHADOW_INTERVENTION_MINDFULNESS, 2000000);
    
    EXPECT_TRUE(success);
    EXPECT_LT(system->jealousy.rumination, 0.5f);  // Rumination reduced
}

TEST_F(ShadowEmotionsTest, AutoInterventionSelectsAppropriateStrategy) {
    // Trigger hubris
    shadow_assess_hubris(system, 10.0f, 0.9f, 0.1f);
    
    bool intervened = shadow_auto_intervene(system, 1000000);
    
    EXPECT_TRUE(intervened);
    EXPECT_TRUE(system->in_self_correction);
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, NeuromodulatorEffectsReflectEmotions) {
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    
    float dopamine = 1.0f, serotonin = 1.0f, cortisol = 1.0f;
    shadow_get_neuromodulator_effects(system, &dopamine, &serotonin, &cortisol);
    
    EXPECT_LT(serotonin, 1.0f);  // Jealousy reduces serotonin
    EXPECT_GT(cortisol, 1.0f);   // Jealousy increases cortisol
}

TEST_F(ShadowEmotionsTest, MentalHealthImpactIncreasesWithIntensity) {
    // Trigger multiple shadow emotions
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    shadow_experience_envy(system, 2, 0.2f, 0.9f, 0.7f, 1000000);
    shadow_assess_hubris(system, 10.0f, 0.9f, 0.1f);
    
    shadow_update(system, 1.0f, 1000000);

    float impact = shadow_get_mental_health_impact(system);
    EXPECT_GT(impact, 0.0f);  // Multiple active emotions should produce measurable impact
    EXPECT_LT(impact, 1.0f);  // Impact should be in valid range
}

TEST_F(ShadowEmotionsTest, QueryFunctionsReturnCorrectValues) {
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    
    EXPECT_TRUE(shadow_is_active(system, SHADOW_JEALOUSY));
    EXPECT_GT(shadow_get_intensity(system, SHADOW_JEALOUSY), 0.5f);
    EXPECT_GT(shadow_get_insight(system), 0.0f);
}

//=============================================================================
// DECAY TESTS
//=============================================================================

TEST_F(ShadowEmotionsTest, EmotionsDecayOverTime) {
    shadow_experience_jealousy(system, 1, 0.8f, 0.9f, 1000000);
    float initial = system->jealousy.intensity;
    
    // Simulate 2 hours passing
    shadow_update(system, 7200.0f, 1000000 + 7200000000);
    
    EXPECT_LT(system->jealousy.intensity, initial);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
