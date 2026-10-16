/**
 * @file test_reasoning_affective_regression.cpp
 * @brief Regression tests for affective modulation
 *
 * WHAT: Tests backward compatibility — existing contributors preserved,
 *       default config unchanged, neutral queries unaffected
 * WHY:  Ensure affective additions don't break existing reasoning behavior
 * HOW:  GTest suite verifying preserved behaviors
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_affective.h"
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class AffectiveRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * REGRESSION TESTS
 *===========================================================================*/

TEST_F(AffectiveRegressionTest, ExistingContributorsPreserved) {
    /* Original 16 contributors must still be present */
    uint32_t count = 0;
    const reasoning_contributor_entry_t* registry =
        reasoning_convergent_get_registry(&count);

    ASSERT_NE(registry, nullptr);

    /* Original names that must still exist */
    const char* expected_names[] = {
        "hippocampus", "semantic_memory", "parietal", "intuition",
        "creative", "kg_self_knowledge", "mesh_evidence",
        "emotional", "ethics", "directives", "bias",
        "theory_of_mind", "introspection", "cingulate", "salience",
        "shadow_emotions",
    };
    uint32_t num_expected = sizeof(expected_names) / sizeof(expected_names[0]);

    for (uint32_t e = 0; e < num_expected; e++) {
        bool found = false;
        for (uint32_t i = 0; i < count; i++) {
            if (strcmp(registry[i].name, expected_names[e]) == 0) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Missing expected contributor: " << expected_names[e];
    }
}

TEST_F(AffectiveRegressionTest, ConvergentWithoutAffective) {
    /* Disable affective, verify convergent still works */
    brain_t brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_affective_modulation = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine, "What is 2+2?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(AffectiveRegressionTest, NeutralQueryNoAffect) {
    /* Neutral query should produce zero modulation from all affective systems */
    int dummy = 1;
    const char* neutral = "What is the capital of France?";

    affective_contribution_t grief = reasoning_affective_evaluate_grief(&dummy, neutral);
    affective_contribution_t joy = reasoning_affective_evaluate_joy(&dummy, neutral);
    affective_contribution_t remorse = reasoning_affective_evaluate_remorse(&dummy, neutral);
    affective_contribution_t social = reasoning_affective_evaluate_social(&dummy, neutral);
    affective_contribution_t shadow = reasoning_affective_evaluate_shadow(&dummy, neutral);
    affective_contribution_t bias = reasoning_affective_evaluate_bias(&dummy, neutral);

    EXPECT_FLOAT_EQ(grief.intensity, 0.0f);
    EXPECT_FLOAT_EQ(joy.intensity, 0.0f);
    EXPECT_FLOAT_EQ(remorse.intensity, 0.0f);
    EXPECT_FLOAT_EQ(social.intensity, 0.0f);
    EXPECT_FLOAT_EQ(shadow.intensity, 0.0f);
    EXPECT_FLOAT_EQ(bias.intensity, 0.0f);

    affective_contribution_t contribs[] = {grief, joy, remorse, social, shadow, bias};
    affective_config_t config = reasoning_affective_default_config();
    float net = reasoning_affective_compute_net_modulation(contribs, 6, &config);
    EXPECT_FLOAT_EQ(net, 0.0f);
}

TEST_F(AffectiveRegressionTest, DefaultConfigBackwardCompat) {
    /* Default reasoning config should have affective enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_TRUE(config.enable_affective_modulation);

    /* All existing defaults must be preserved */
    EXPECT_TRUE(config.enable_convergent_reasoning);
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_predictive_verify);
    EXPECT_TRUE(config.enable_epistemic_check);
    EXPECT_TRUE(config.enable_symbolic_logic);
}

TEST_F(AffectiveRegressionTest, StepTypesPreserved) {
    /* Existing step type names must be preserved */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_RECALL), "RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_KNOWLEDGE), "KNOWLEDGE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_INFERENCE), "INFERENCE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_VERIFICATION), "VERIFICATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_HIPPOCAMPAL_RECALL),
                 "HIPPOCAMPAL_RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MESH_CONSENSUS),
                 "MESH_CONSENSUS");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_METACOGNITIVE),
                 "METACOGNITIVE");

    /* New affective step type */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_AFFECTIVE), "AFFECTIVE");
}

TEST_F(AffectiveRegressionTest, RegistryCountIncreased) {
    /* Registry should have 16 original + 4 new = 20 entries */
    uint32_t count = 0;
    reasoning_convergent_get_registry(&count);
    EXPECT_EQ(count, 20u);
}
