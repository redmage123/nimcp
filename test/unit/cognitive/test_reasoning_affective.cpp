/**
 * @file test_reasoning_affective.cpp
 * @brief Unit tests for the affective modulation system
 *
 * WHAT: Tests affective evaluation functions, configuration, and net modulation
 * WHY:  Verify emotional keyword analysis and confidence delta computation
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_affective.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningAffectiveTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    /* Dummy system pointer (non-NULL to pass gate check) */
    int dummy_system = 1;
};

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, DefaultConfig) {
    affective_config_t config = reasoning_affective_default_config();

    EXPECT_TRUE(config.enable_affective_modulation);
    EXPECT_FLOAT_EQ(config.grief_weight, AFFECTIVE_DEFAULT_GRIEF_WEIGHT);
    EXPECT_FLOAT_EQ(config.joy_weight, AFFECTIVE_DEFAULT_JOY_WEIGHT);
    EXPECT_FLOAT_EQ(config.remorse_weight, AFFECTIVE_DEFAULT_REMORSE_WEIGHT);
    EXPECT_FLOAT_EQ(config.social_weight, AFFECTIVE_DEFAULT_SOCIAL_WEIGHT);
    EXPECT_FLOAT_EQ(config.shadow_weight, AFFECTIVE_DEFAULT_SHADOW_WEIGHT);
    EXPECT_FLOAT_EQ(config.bias_weight, AFFECTIVE_DEFAULT_BIAS_WEIGHT);
    EXPECT_FLOAT_EQ(config.intensity_threshold, 0.1f);
}

/*=============================================================================
 * GRIEF EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateGriefWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_grief(
        &dummy_system, "I am feeling deep grief and loss");

    EXPECT_EQ(result.influence_type, AFFECT_GRIEF);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_LT(result.confidence_delta, 0.0f);  /* Grief reduces confidence */
}

TEST_F(ReasoningAffectiveTest, EvaluateGriefNoKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_grief(
        &dummy_system, "The weather is nice today");

    EXPECT_EQ(result.influence_type, AFFECT_GRIEF);
    EXPECT_FLOAT_EQ(result.intensity, 0.0f);
    EXPECT_FLOAT_EQ(result.confidence_delta, 0.0f);
}

TEST_F(ReasoningAffectiveTest, EvaluateGriefNullSystem) {
    affective_contribution_t result = reasoning_affective_evaluate_grief(
        NULL, "I am feeling grief");

    EXPECT_EQ(result.influence_type, AFFECT_NONE);
    EXPECT_FLOAT_EQ(result.intensity, 0.0f);
    EXPECT_FLOAT_EQ(result.confidence_delta, 0.0f);
}

/*=============================================================================
 * JOY EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateJoyWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_joy(
        &dummy_system, "What a great success and achievement!");

    EXPECT_EQ(result.influence_type, AFFECT_JOY);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_GT(result.confidence_delta, 0.0f);  /* Joy boosts confidence */
}

TEST_F(ReasoningAffectiveTest, EvaluateJoyNoKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_joy(
        &dummy_system, "The cat sat on the mat");

    EXPECT_EQ(result.influence_type, AFFECT_JOY);
    EXPECT_FLOAT_EQ(result.intensity, 0.0f);
}

/*=============================================================================
 * REMORSE EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateRemorseWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_remorse(
        &dummy_system, "I regret my mistake, I am sorry");

    EXPECT_EQ(result.influence_type, AFFECT_REMORSE);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_LT(result.confidence_delta, 0.0f);  /* Remorse reduces confidence */
}

/*=============================================================================
 * SOCIAL BOND EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateSocialWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_social(
        &dummy_system, "My friend and family trust each other");

    EXPECT_EQ(result.influence_type, AFFECT_SOCIAL_BOND);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_GT(result.confidence_delta, 0.0f);  /* Social boosts confidence */
}

/*=============================================================================
 * SHADOW EMOTIONS EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateShadowWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_shadow(
        &dummy_system, "I fear the hidden anger I suppress");

    EXPECT_EQ(result.influence_type, AFFECT_SHADOW);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_LT(result.confidence_delta, 0.0f);  /* Shadow reduces confidence */
}

/*=============================================================================
 * BIAS DETECTION EVALUATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, EvaluateBiasWithKeywords) {
    affective_contribution_t result = reasoning_affective_evaluate_bias(
        &dummy_system, "Is this fair or biased with prejudice?");

    EXPECT_EQ(result.influence_type, AFFECT_BIAS);
    EXPECT_GT(result.intensity, 0.0f);
    EXPECT_LT(result.confidence_delta, 0.0f);  /* Bias reduces confidence */
}

/*=============================================================================
 * INTENSITY RANGE TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, IntensityRange) {
    /* All evaluate functions should return intensity in [0, 1] */
    const char* queries[] = {
        "",
        "loss death grief missing gone farewell",  /* max grief keywords */
        "happy success achievement celebrate win great",  /* max joy keywords */
        "neutral query with no keywords",
    };

    for (int i = 0; i < 4; i++) {
        affective_contribution_t r;

        r = reasoning_affective_evaluate_grief(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);

        r = reasoning_affective_evaluate_joy(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);

        r = reasoning_affective_evaluate_remorse(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);

        r = reasoning_affective_evaluate_social(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);

        r = reasoning_affective_evaluate_shadow(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);

        r = reasoning_affective_evaluate_bias(&dummy_system, queries[i]);
        EXPECT_GE(r.intensity, 0.0f);
        EXPECT_LE(r.intensity, 1.0f);
    }
}

/*=============================================================================
 * CONFIDENCE DELTA SIGN TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, ConfidenceDeltaSign) {
    /* Grief, remorse, shadow, bias → negative delta */
    /* Joy, social → positive delta */

    affective_contribution_t grief = reasoning_affective_evaluate_grief(
        &dummy_system, "grief and loss");
    EXPECT_LE(grief.confidence_delta, 0.0f);

    affective_contribution_t remorse = reasoning_affective_evaluate_remorse(
        &dummy_system, "mistake and regret");
    EXPECT_LE(remorse.confidence_delta, 0.0f);

    affective_contribution_t shadow = reasoning_affective_evaluate_shadow(
        &dummy_system, "hidden fear");
    EXPECT_LE(shadow.confidence_delta, 0.0f);

    affective_contribution_t bias = reasoning_affective_evaluate_bias(
        &dummy_system, "biased prejudice");
    EXPECT_LE(bias.confidence_delta, 0.0f);

    affective_contribution_t joy = reasoning_affective_evaluate_joy(
        &dummy_system, "happy success");
    EXPECT_GE(joy.confidence_delta, 0.0f);

    affective_contribution_t social = reasoning_affective_evaluate_social(
        &dummy_system, "friend trust");
    EXPECT_GE(social.confidence_delta, 0.0f);
}

/*=============================================================================
 * NET MODULATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, ComputeNetModulation) {
    affective_contribution_t contribs[3];
    memset(contribs, 0, sizeof(contribs));

    contribs[0].influence_type = AFFECT_GRIEF;
    contribs[0].intensity = 0.5f;
    contribs[0].confidence_delta = -0.075f;

    contribs[1].influence_type = AFFECT_JOY;
    contribs[1].intensity = 0.3f;
    contribs[1].confidence_delta = 0.03f;

    contribs[2].influence_type = AFFECT_BIAS;
    contribs[2].intensity = 0.7f;
    contribs[2].confidence_delta = -0.175f;

    affective_config_t config = reasoning_affective_default_config();
    float net = reasoning_affective_compute_net_modulation(contribs, 3, &config);

    /* Net should be sum of deltas: -0.075 + 0.03 + (-0.175) = -0.22 */
    EXPECT_NEAR(net, -0.22f, 0.001f);
}

TEST_F(ReasoningAffectiveTest, ComputeNetModulationClamping) {
    /* Create extreme contributions that exceed ±0.5 */
    affective_contribution_t contribs[4];
    memset(contribs, 0, sizeof(contribs));

    for (int i = 0; i < 4; i++) {
        contribs[i].influence_type = AFFECT_GRIEF;
        contribs[i].intensity = 0.9f;
        contribs[i].confidence_delta = -0.2f;  /* Total = -0.8 */
    }

    affective_config_t config = reasoning_affective_default_config();
    float net = reasoning_affective_compute_net_modulation(contribs, 4, &config);

    /* Should be clamped to -0.5 */
    EXPECT_FLOAT_EQ(net, -0.5f);

    /* Test positive clamping */
    for (int i = 0; i < 4; i++) {
        contribs[i].influence_type = AFFECT_JOY;
        contribs[i].intensity = 0.9f;
        contribs[i].confidence_delta = 0.2f;  /* Total = +0.8 */
    }

    net = reasoning_affective_compute_net_modulation(contribs, 4, &config);
    EXPECT_FLOAT_EQ(net, 0.5f);
}

TEST_F(ReasoningAffectiveTest, IntensityThreshold) {
    affective_contribution_t contribs[2];
    memset(contribs, 0, sizeof(contribs));

    /* One above threshold, one below */
    contribs[0].influence_type = AFFECT_GRIEF;
    contribs[0].intensity = 0.05f;  /* Below 0.1 threshold */
    contribs[0].confidence_delta = -0.1f;

    contribs[1].influence_type = AFFECT_JOY;
    contribs[1].intensity = 0.5f;  /* Above threshold */
    contribs[1].confidence_delta = 0.05f;

    affective_config_t config = reasoning_affective_default_config();
    float net = reasoning_affective_compute_net_modulation(contribs, 2, &config);

    /* Only the joy contribution should count (grief is below threshold) */
    EXPECT_NEAR(net, 0.05f, 0.001f);
}

TEST_F(ReasoningAffectiveTest, ComputeNetModulationEmpty) {
    affective_config_t config = reasoning_affective_default_config();
    float net = reasoning_affective_compute_net_modulation(NULL, 0, &config);
    EXPECT_FLOAT_EQ(net, 0.0f);
}

TEST_F(ReasoningAffectiveTest, ComputeNetModulationNull) {
    float net = reasoning_affective_compute_net_modulation(NULL, 5, NULL);
    EXPECT_FLOAT_EQ(net, 0.0f);
}

/*=============================================================================
 * MULTIPLE KEYWORDS HIGHER INTENSITY
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, MultipleKeywordsHigherIntensity) {
    /* Single keyword match */
    affective_contribution_t single = reasoning_affective_evaluate_grief(
        &dummy_system, "loss");
    float single_intensity = single.intensity;

    /* Multiple keyword matches */
    affective_contribution_t multiple = reasoning_affective_evaluate_grief(
        &dummy_system, "loss death grief missing");
    float multiple_intensity = multiple.intensity;

    /* More keywords → higher intensity */
    EXPECT_GT(multiple_intensity, single_intensity);
}

/*=============================================================================
 * WEIGHT CONFIGURATION
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, WeightConfiguration) {
    /* Default grief weight is -0.15, intensity of 0.3 gives delta = -0.045 */
    affective_contribution_t result = reasoning_affective_evaluate_grief(
        &dummy_system, "loss");

    EXPECT_EQ(result.influence_type, AFFECT_GRIEF);
    EXPECT_FLOAT_EQ(result.intensity, 0.3f);  /* 1 keyword → 0.3 */
    EXPECT_NEAR(result.confidence_delta,
                AFFECTIVE_DEFAULT_GRIEF_WEIGHT * 0.3f, 0.001f);
}

/*=============================================================================
 * CASE INSENSITIVE MATCHING
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, CaseInsensitiveMatching) {
    affective_contribution_t lower = reasoning_affective_evaluate_grief(
        &dummy_system, "loss");
    affective_contribution_t upper = reasoning_affective_evaluate_grief(
        &dummy_system, "LOSS");
    affective_contribution_t mixed = reasoning_affective_evaluate_grief(
        &dummy_system, "LoSs");

    EXPECT_FLOAT_EQ(lower.intensity, upper.intensity);
    EXPECT_FLOAT_EQ(lower.intensity, mixed.intensity);
    EXPECT_GT(lower.intensity, 0.0f);
}

/*=============================================================================
 * NULL QUERY HANDLING
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, NullQueryHandling) {
    affective_contribution_t result = reasoning_affective_evaluate_grief(
        &dummy_system, NULL);
    EXPECT_EQ(result.influence_type, AFFECT_NONE);
}

/*=============================================================================
 * DEFAULT CONFIG WITH NULL
 *===========================================================================*/

TEST_F(ReasoningAffectiveTest, ComputeNetModulationDefaultConfig) {
    /* Should use default config when NULL is passed */
    affective_contribution_t contribs[1];
    memset(contribs, 0, sizeof(contribs));
    contribs[0].influence_type = AFFECT_GRIEF;
    contribs[0].intensity = 0.5f;
    contribs[0].confidence_delta = -0.075f;

    float net = reasoning_affective_compute_net_modulation(contribs, 1, NULL);
    EXPECT_NEAR(net, -0.075f, 0.001f);
}
