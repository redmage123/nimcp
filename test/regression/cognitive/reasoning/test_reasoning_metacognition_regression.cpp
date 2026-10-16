/**
 * @file test_reasoning_metacognition_regression.cpp
 * @brief Regression tests for metacognitive controller backward compatibility
 *
 * WHAT: Verify that adding metacognition doesn't break existing behavior
 * WHY:  Guard against regressions in the reasoning pipeline
 * HOW:  Check default configs, step types, and sequential fallback behavior
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_metacognition.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MetacognitionRegression : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * BACKWARD COMPATIBILITY TESTS
 *===========================================================================*/

TEST_F(MetacognitionRegression, DefaultConfigBackwardCompat) {
    /* Default config should have metacognition enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_TRUE(config.enable_metacognition);

    /* All previous config fields should still have their defaults */
    EXPECT_EQ(config.max_depth, REASONING_CHAIN_DEFAULT_MAX_DEPTH);
    EXPECT_EQ(config.max_steps, REASONING_CHAIN_DEFAULT_MAX_STEPS);
    EXPECT_FLOAT_EQ(config.confidence_threshold,
                    REASONING_CHAIN_DEFAULT_CONFIDENCE_THRESHOLD);
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
    EXPECT_TRUE(config.enable_convergent_reasoning);
}

TEST_F(MetacognitionRegression, SequentialPipelineUnchanged) {
    /* Disable metacognition, verify sequential pipeline still works */
    brain_t brain = brain_create("regression_seq", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_metacognition = false;
    cfg.enable_convergent_reasoning = false;
    cfg.enable_concurrent_pipeline = false;

    reasoning_engine_t* engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine, "What patterns exist?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 0u);

    /* No metacognitive step should be present */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* step = reasoning_chain_get_step(&chain, i);
        EXPECT_NE(step->type, REASONING_STEP_METACOGNITIVE)
            << "Metacognitive step found with metacognition disabled";
    }

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(MetacognitionRegression, ExistingStepTypesPreserved) {
    /* All pre-existing step type names should still return correct strings */
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

    /* Convergent step types */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SEMANTIC_ACTIVATION),
                 "SEMANTIC_ACTIVATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_HIPPOCAMPAL_RECALL),
                 "HIPPOCAMPAL_RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MODULATION), "MODULATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MESH_CONSENSUS), "MESH_CONSENSUS");

    /* New metacognitive step type */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_METACOGNITIVE), "METACOGNITIVE");
}

TEST_F(MetacognitionRegression, StatsBackwardCompat) {
    /* Create engine, check old stats fields are still populated */
    brain_t brain = brain_create("regression_stats", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&cfg);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    rc = reasoning_engine_reason(engine, "What is data?", &chain);
    EXPECT_EQ(rc, 0);

    reasoning_engine_stats_t stats;
    rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);

    /* Old fields should be populated */
    EXPECT_EQ(stats.total_queries, 1u);
    EXPECT_EQ(stats.successful_queries, 1u);
    EXPECT_GT(stats.total_steps, 0u);

    /* New metacognitive fields should also be populated */
    EXPECT_EQ(stats.metacognitive_assessments, 1u);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(MetacognitionRegression, ConfigFieldsBackwardCompat) {
    /* Verify old config fields still have same defaults */
    reasoning_engine_config_t cfg = reasoning_engine_default_config();

    EXPECT_EQ(cfg.concurrent_pool_size, 4u);
    EXPECT_EQ(cfg.working_memory_slots, REASONING_CHAIN_DEFAULT_WM_SLOTS);
    EXPECT_TRUE(cfg.enable_convergent_reasoning);
    EXPECT_EQ(cfg.convergent_pool_size, 8u);
    EXPECT_EQ(cfg.max_convergent_contributors, 64u);
}
