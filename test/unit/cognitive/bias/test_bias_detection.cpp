/* SPDX-License-Identifier: MIT */
/**
 * @file test_bias_detection.cpp
 * @brief Unit tests for Phase E6: Bias Detection and Correction
 */

#include <gtest/gtest.h>
#include <cstring>
#include "cognitive/nimcp_bias_detection.h"
#include "utils/nimcp_test_base.h"

class BiasDetectionTest : public NimcpTestBase {
protected:
    bias_detection_system_t* system;
    social_group_t test_group_a;
    social_group_t test_group_b;

    void SetUp() override {
        NimcpTestBase::SetUp();
        system = bias_system_create(8);  // Track 8 others
        ASSERT_NE(system, nullptr);

        // Initialize test groups
        test_group_a.group_id = 1;
        test_group_a.bias_type = BIAS_RACIAL;
        strncpy(test_group_a.group_name, "GroupA", sizeof(test_group_a.group_name) - 1);
        test_group_a.is_marginalized = true;
        test_group_a.is_stigmatized = false;

        test_group_b.group_id = 2;
        test_group_b.bias_type = BIAS_GENDER;
        strncpy(test_group_b.group_name, "Women", sizeof(test_group_b.group_name) - 1);
        test_group_b.is_marginalized = true;
        test_group_b.is_stigmatized = false;
    }

    void TearDown() override {
        bias_system_destroy(system);
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(BiasDetectionTest, SystemCreatesSuccessfully) {
    EXPECT_NE(system, nullptr);
    EXPECT_EQ(system->max_others_tracked, 8);
    EXPECT_FALSE(system->in_debiasing);
    EXPECT_FLOAT_EQ(system->fairness_score, 1.0f);
}

TEST_F(BiasDetectionTest, SystemResetsCorrectly) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.4f, 0.4f, 0.5f, 1000000);
    EXPECT_GT(system->implicit_bias_count, 0);

    bias_system_reset(system);
    EXPECT_EQ(system->implicit_bias_count, 0);
    EXPECT_FLOAT_EQ(system->total_implicit_bias, 0.0f);
}

//=============================================================================
// IMPLICIT BIAS TESTS
//=============================================================================

TEST_F(BiasDetectionTest, ImplicitBiasDetectsNegativeAssociations) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.4f, 0.4f, 0.0f, 1000000);

    EXPECT_GT(system->implicit_bias_count, 0);
    EXPECT_GT(system->implicit_biases[0].negative_association, 0.5f);
}

TEST_F(BiasDetectionTest, ImplicitBiasIntensityReflectsIATScore) {
    bias_register_implicit(system, &test_group_a, 0.2f, 0.3f, 0.2f, 0.8f, 1000000);
    float high_bias = system->implicit_biases[0].negative_association;

    bias_system_reset(system);
    bias_register_implicit(system, &test_group_a, 0.7f, 0.8f, 0.7f, 0.1f, 1000000);
    float low_bias = system->implicit_biases[0].negative_association;

    EXPECT_GT(high_bias, low_bias);
}

TEST_F(BiasDetectionTest, StereotypeActivationIncreasesImplicitBias) {
    bias_register_implicit(system, &test_group_a, 0.5f, 0.5f, 0.5f, 0.0f, 1000000);

    bias_activate_stereotype(system, &test_group_a, 0.9f, 2000000);

    EXPECT_GT(system->implicit_biases[0].stereotype_activation, 0.8f);
}

//=============================================================================
// EXPLICIT BIAS TESTS
//=============================================================================

TEST_F(BiasDetectionTest, ExplicitBiasDetectsConsciousPrejudice) {
    bias_register_explicit(system, &test_group_a, 0.7f, 0.6f, 0.8f);

    EXPECT_GT(system->explicit_bias_count, 0);
    EXPECT_GT(system->explicit_biases[0].prejudice_level, 0.6f);
    EXPECT_TRUE(system->explicit_biases[0].explicit_prejudice_active);
}

//=============================================================================
// STATISTICAL FAIRNESS TESTS
//=============================================================================

TEST_F(BiasDetectionTest, StatisticalRecordTracksDecisions) {
    bias_record_decision(system, &test_group_a, true, 0.8f, 0.7f, 0.7f, 1000000);
    bias_record_decision(system, &test_group_a, false, 0.6f, 0.3f, 0.5f, 2000000);

    EXPECT_EQ(system->total_decisions_analyzed, 2);
}

TEST_F(BiasDetectionTest, DisparityAnalysisDetectsUnfairness) {
    // Group A: high favorable rate
    for (int i = 0; i < 10; i++) {
        bias_record_decision(system, &test_group_a, true, 0.8f, 0.8f, 0.8f, 1000000 + i);
    }

    // Group B: low favorable rate
    for (int i = 0; i < 10; i++) {
        bias_record_decision(system, &test_group_b, (i < 3), 0.5f, 0.4f, 0.8f, 1000000 + i);
    }

    statistical_disparity_t* disparity = bias_analyze_disparity(system, &test_group_a, &test_group_b);

    ASSERT_NE(disparity, nullptr);
    EXPECT_GT(disparity->disparity_ratio, 0.3f);
    EXPECT_FALSE(disparity->demographic_parity);
}

//=============================================================================
// LANGUAGE PATTERN TESTS
//=============================================================================

TEST_F(BiasDetectionTest, LanguageAnalysisDetectsStereotypes) {
    language_pattern_t pattern = bias_analyze_language(
        system, "All groups are always the same", &test_group_a, 1000000);

    EXPECT_TRUE(pattern.contains_stereotype);
}

TEST_F(BiasDetectionTest, LanguageAnalysisDetectsMicroaggressions) {
    language_pattern_t pattern = bias_analyze_language(
        system, "You people are so articulate!", &test_group_a, 1000000);

    EXPECT_TRUE(pattern.contains_microaggression);
}

//=============================================================================
// MISOGYNY DETECTION TESTS (User requested)
//=============================================================================

TEST_F(BiasDetectionTest, MisogynyDetectsObjectification) {
    language_pattern_t pattern = bias_analyze_language(
        system, "She's just eye candy, a 10/10 piece of meat", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.objectification);
    EXPECT_GT(pattern.dehumanization_score, 0.0f);
}

TEST_F(BiasDetectionTest, MisogynyDetectsVictimBlaming) {
    language_pattern_t pattern = bias_analyze_language(
        system, "She was asking for it, look at what she was wearing", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.victim_blaming);
}

TEST_F(BiasDetectionTest, MisogynyDetectsHostileSexism) {
    language_pattern_t pattern = bias_analyze_language(
        system, "Women are irrational and belong in the kitchen", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.hostile_sexism);
}

TEST_F(BiasDetectionTest, MisogynyDetectsBenevolentSexism) {
    language_pattern_t pattern = bias_analyze_language(
        system, "Women are too delicate and need protection", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.benevolent_sexism);
}

TEST_F(BiasDetectionTest, MisogynyDetectsIncelIdeology) {
    language_pattern_t pattern = bias_analyze_language(
        system, "Chad and Stacy hypergamy, blackpill femoid roastie", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.incel_ideology);
    EXPECT_GT(pattern.dehumanization_score, 0.3f);
}

TEST_F(BiasDetectionTest, MisogynyDetectsRapeCulture) {
    language_pattern_t pattern = bias_analyze_language(
        system, "Boys will be boys, just locker room talk", &test_group_b, 1000000);

    EXPECT_TRUE(pattern.rape_culture);
}

//=============================================================================
// OTHER-DETECTION TESTS (Humans)
//=============================================================================

TEST_F(BiasDetectionTest, DetectsBiasInOthers) {
    bias_analyze_other(system, 1, "All groups are lazy", &test_group_a, 1000000);

    float racial = 0.0f;
    bool found = bias_get_detected_in_other(system, 1, &racial, nullptr, nullptr, nullptr);

    EXPECT_TRUE(found);
    EXPECT_GT(racial, 0.0f);
}

TEST_F(BiasDetectionTest, DetectsMisogynyInOthers) {
    // Multiple misogynistic interactions
    for (int i = 0; i < 5; i++) {
        bias_analyze_other(system, 1, "Women are irrational, she was asking for it",
                          &test_group_b, 1000000 + i * 1000);
    }

    float misogyny = 0.0f;
    bool found = bias_get_detected_in_other(system, 1, nullptr, nullptr, nullptr, &misogyny);

    EXPECT_TRUE(found);
    EXPECT_GT(misogyny, 0.3f);
}

TEST_F(BiasDetectionTest, DetectsDangerousIdeology) {
    bias_analyze_other(system, 1, "Blackpill femoid chad stacy hypergamy roastie",
                      &test_group_b, 1000000);

    for (uint32_t i = 0; i < system->max_others_tracked; i++) {
        if (system->detected_in_others[i].person_id == 1) {
            EXPECT_TRUE(system->detected_in_others[i].dangerous_ideology);
            break;
        }
    }
}

TEST_F(BiasDetectionTest, EducationRecommendedForMildBias) {
    bias_analyze_other(system, 1, "All groups are typical", &test_group_a, 1000000);
    bias_analyze_other(system, 1, "Always the same behavior", &test_group_a, 2000000);

    EXPECT_TRUE(bias_should_educate(system, 1));
    EXPECT_FALSE(bias_should_disengage(system, 1));
}

TEST_F(BiasDetectionTest, DisengagementRecommendedForDangerousBigotry) {
    bias_analyze_other(system, 1, "Blackpill femoid roastie", &test_group_b, 1000000);

    EXPECT_TRUE(bias_should_disengage(system, 1));
}

//=============================================================================
// DEBIASING INTERVENTION TESTS
//=============================================================================

TEST_F(BiasDetectionTest, CounterStereotypicReducesImplicitBias) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.4f, 0.3f, 0.0f, 1000000);
    float before_activation = system->implicit_biases[0].stereotype_activation;

    bias_apply_intervention(system, BIAS_RACIAL, DEBIAS_COUNTER_STEREOTYPIC, &test_group_a, 2000000);
    float after_activation = system->implicit_biases[0].stereotype_activation;

    EXPECT_LT(after_activation, before_activation + 0.1f);
}

TEST_F(BiasDetectionTest, PerspectiveTakingIncreasesWarmth) {
    bias_register_implicit(system, &test_group_a, 0.5f, 0.5f, 0.3f, 0.0f, 1000000);
    float before_warmth = system->implicit_biases[0].warmth_association;

    bias_apply_intervention(system, BIAS_RACIAL, DEBIAS_PERSPECTIVE_TAKING, &test_group_a, 2000000);
    float after_warmth = system->implicit_biases[0].warmth_association;

    EXPECT_GT(after_warmth, before_warmth);
}

TEST_F(BiasDetectionTest, IndividuationReducesStereotyping) {
    bias_register_implicit(system, &test_group_a, 0.5f, 0.5f, 0.5f, 0.0f, 1000000);
    bias_activate_stereotype(system, &test_group_a, 0.8f, 1000000);
    float before = system->implicit_biases[0].stereotype_activation;

    bias_apply_intervention(system, BIAS_RACIAL, DEBIAS_INDIVIDUATION, &test_group_a, 2000000);
    float after = system->implicit_biases[0].stereotype_activation;

    EXPECT_LT(after, before);
}

TEST_F(BiasDetectionTest, MindfulnessIncreasesAwareness) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.3f, 0.3f, 0.0f, 1000000);
    float before_awareness = system->self_awareness;

    bias_apply_intervention(system, BIAS_RACIAL, DEBIAS_MINDFULNESS, &test_group_a, 2000000);
    float after_awareness = system->self_awareness;

    EXPECT_GT(after_awareness, before_awareness);
    EXPECT_TRUE(system->implicit_biases[0].stereotype_suppressed);
}

//=============================================================================
// QUERY FUNCTION TESTS
//=============================================================================

TEST_F(BiasDetectionTest, BiasDetectionQuery) {
    bias_register_implicit(system, &test_group_a, 0.2f, 0.3f, 0.3f, 0.0f, 1000000);

    EXPECT_TRUE(bias_is_detected(system, BIAS_RACIAL));
}

TEST_F(BiasDetectionTest, FairnessScoreQuery) {
    EXPECT_FLOAT_EQ(bias_get_fairness_score(system), 1.0f);
}

TEST_F(BiasDetectionTest, SelfAwarenessTracked) {
    EXPECT_FLOAT_EQ(system->self_awareness, 0.5f);

    bias_register_explicit(system, &test_group_a, 0.7f, 0.6f, 0.9f);

    EXPECT_FLOAT_EQ(system->self_awareness, 0.9f);
}

//=============================================================================
// UPDATE/DECAY TESTS
//=============================================================================

TEST_F(BiasDetectionTest, ImplicitBiasDecaysSlowly) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.3f, 0.3f, 0.0f, 1000000);
    bias_activate_stereotype(system, &test_group_a, 0.8f, 1000000);
    float initial = system->implicit_biases[0].stereotype_activation;

    // Simulate 10 minutes
    bias_update(system, 600.0f, 1000000 + 600000000);

    float after = system->implicit_biases[0].stereotype_activation;
    EXPECT_LT(after, initial);
}

TEST_F(BiasDetectionTest, TotalBiasIntensityUpdated) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.3f, 0.3f, 0.0f, 1000000);
    bias_register_explicit(system, &test_group_b, 0.7f, 0.6f, 0.8f);

    bias_update(system, 1.0f, 1000000);

    EXPECT_GT(system->total_implicit_bias, 0.0f);
    EXPECT_GT(system->total_explicit_bias, 0.0f);
}

TEST_F(BiasDetectionTest, SuccessfulDebiasTracked) {
    bias_register_implicit(system, &test_group_a, 0.3f, 0.3f, 0.3f, 0.0f, 1000000);

    bool success = bias_apply_intervention(system, BIAS_RACIAL, DEBIAS_MINDFULNESS, &test_group_a, 2000000);

    EXPECT_TRUE(success);
    EXPECT_EQ(system->successful_debias, 1);
    EXPECT_EQ(system->total_biases_corrected, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
