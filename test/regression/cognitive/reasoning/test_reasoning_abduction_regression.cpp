/**
 * @file test_reasoning_abduction_regression.cpp
 * @brief Regression tests for abductive reasoning module
 *
 * WHAT: Verify backward compatibility and structural preservation after
 *       adding abductive reasoning to the reasoning chain
 * WHY:  Ensure existing functionality is not broken by the new module
 * HOW:  GTest suite checking existing types, configs, and pipeline behavior
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_abduction.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningAbductionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * BACKWARD COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionRegressionTest, DefaultConfigBackwardCompat) {
    /* Verify all pre-existing config fields have expected defaults */
    reasoning_engine_config_t config = reasoning_engine_default_config();

    /* Pre-existing fields should not change */
    EXPECT_EQ(config.max_depth, (uint32_t)REASONING_CHAIN_DEFAULT_MAX_DEPTH);
    EXPECT_EQ(config.max_steps, (uint32_t)REASONING_CHAIN_DEFAULT_MAX_STEPS);
    EXPECT_FLOAT_EQ(config.confidence_threshold, REASONING_CHAIN_DEFAULT_CONFIDENCE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.uncertainty_threshold, REASONING_CHAIN_DEFAULT_UNCERTAINTY_THRESHOLD);
    EXPECT_TRUE(config.enable_engram_recall);
    EXPECT_TRUE(config.enable_knowledge_query);
    EXPECT_TRUE(config.enable_predictive_verify);
    EXPECT_TRUE(config.enable_epistemic_check);
    EXPECT_TRUE(config.enable_analogical);
    EXPECT_TRUE(config.enable_working_memory);
    EXPECT_TRUE(config.enable_world_model);
    EXPECT_TRUE(config.enable_jepa_prediction);
    EXPECT_TRUE(config.enable_symbolic_logic);
    EXPECT_TRUE(config.enable_concurrent_pipeline);

    /* New abductive field should default to true */
    EXPECT_TRUE(config.enable_abductive_reasoning);
}

TEST_F(ReasoningAbductionRegressionTest, ExistingStepTypesPreserved) {
    /* Verify all pre-existing step type names are unchanged */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_RECALL), "RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_KNOWLEDGE), "KNOWLEDGE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_INFERENCE), "INFERENCE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_VERIFICATION), "VERIFICATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_UNCERTAINTY), "UNCERTAINTY");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_ANALOGY), "ANALOGY");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_DECOMPOSITION), "DECOMPOSITION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SYNTHESIS), "SYNTHESIS");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_WORLD_MODEL), "WORLD_MODEL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_JEPA_PREDICTION), "JEPA_PREDICTION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SYMBOLIC_LOGIC), "SYMBOLIC_LOGIC");

    /* Convergent types preserved */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SEMANTIC_ACTIVATION), "SEMANTIC_ACTIVATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_HIPPOCAMPAL_RECALL), "HIPPOCAMPAL_RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MATHEMATICAL), "MATHEMATICAL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_INTUITIVE), "INTUITIVE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_CREATIVE_ANALOGY), "CREATIVE_ANALOGY");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SELF_KNOWLEDGE), "SELF_KNOWLEDGE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_NEURAL_LOGIC), "NEURAL_LOGIC");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MESH_CONSENSUS), "MESH_CONSENSUS");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MODULATION), "MODULATION");

    /* New abductive type */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_ABDUCTIVE), "ABDUCTIVE");
}

TEST_F(ReasoningAbductionRegressionTest, SequentialPipelineWithoutAbduction) {
    /* Disable abduction, verify old pipeline unchanged */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = false;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    brain_t brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    int rc = reasoning_engine_reason(engine, "What is the meaning of life?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* Verify no ABDUCTIVE steps */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        EXPECT_NE(chain.steps[i].type, REASONING_STEP_ABDUCTIVE);
    }

    /* Verify standard pipeline steps exist */
    bool has_decomposition = false;
    bool has_inference = false;
    bool has_synthesis = false;
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        if (chain.steps[i].type == REASONING_STEP_DECOMPOSITION) has_decomposition = true;
        if (chain.steps[i].type == REASONING_STEP_INFERENCE) has_inference = true;
        if (chain.steps[i].type == REASONING_STEP_SYNTHESIS) has_synthesis = true;
    }
    EXPECT_TRUE(has_decomposition);
    EXPECT_TRUE(has_inference);
    EXPECT_TRUE(has_synthesis);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    if (brain) brain_destroy(brain);
}

TEST_F(ReasoningAbductionRegressionTest, StatsBackwardCompat) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = false;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    brain_t brain = brain_create("stats_test", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    /* Run a query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    reasoning_engine_reason(engine, "How does photosynthesis work?", &chain);
    reasoning_chain_cleanup(&chain);

    /* Verify existing stats fields work */
    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.total_queries, 1u);
    EXPECT_GE(stats.successful_queries, 1u);
    EXPECT_GE(stats.total_steps, 1u);
    EXPECT_GE(stats.avg_confidence, 0.0f);
    EXPECT_GE(stats.avg_steps_per_query, 0.0f);

    /* Abductive stats should be zero when disabled */
    EXPECT_EQ(stats.abductive_queries, 0u);
    EXPECT_FLOAT_EQ(stats.avg_hypotheses_per_query, 0.0f);

    reasoning_engine_destroy(engine);
    if (brain) brain_destroy(brain);
}

TEST_F(ReasoningAbductionRegressionTest, ChainStructurePreserved) {
    /* Verify chain management functions still work correctly */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    EXPECT_EQ(chain.num_steps, 0u);
    EXPECT_NE(chain.steps, nullptr);  /* Pre-allocated */
    EXPECT_FALSE(chain.is_complete);

    /* Add steps of various types */
    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.type = REASONING_STEP_RECALL;
    step.confidence = 0.5f;
    step.relevance = 0.6f;
    snprintf(step.description, REASONING_STEP_DESC_LEN, "Test recall");
    EXPECT_EQ(reasoning_chain_add_step(&chain, &step), 0);

    step.type = REASONING_STEP_ABDUCTIVE;
    step.confidence = 0.7f;
    step.relevance = 0.8f;
    snprintf(step.description, REASONING_STEP_DESC_LEN, "Test abductive");
    EXPECT_EQ(reasoning_chain_add_step(&chain, &step), 0);

    EXPECT_EQ(chain.num_steps, 2u);

    const reasoning_step_t* s0 = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(s0, nullptr);
    EXPECT_EQ(s0->type, REASONING_STEP_RECALL);

    const reasoning_step_t* s1 = reasoning_chain_get_step(&chain, 1);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(s1->type, REASONING_STEP_ABDUCTIVE);

    /* Out of bounds */
    EXPECT_EQ(reasoning_chain_get_step(&chain, 2), nullptr);

    reasoning_chain_cleanup(&chain);
}
