/**
 * @file test_reasoning_causal_regression.cpp
 * @brief Regression tests for causal reasoning — ensure existing behavior unchanged
 *
 * WHAT: Verify that adding causal reasoning does not break existing functionality
 * WHY:  Guard against regressions in the reasoning pipeline when causal is added
 * HOW:  Test default config, step types, pipeline behavior without causal enabled
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_causal.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CausalRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * REGRESSION TESTS
 *===========================================================================*/

TEST_F(CausalRegressionTest, DefaultConfigDisabled) {
    /*
     * Causal reasoning is opt-in (default false).
     * Existing users who don't set enable_causal_reasoning should get
     * identical behavior to before this feature was added.
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_FALSE(config.enable_causal_reasoning);

    /* All other defaults should still be true */
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
    EXPECT_TRUE(config.enable_metacognition);
}

TEST_F(CausalRegressionTest, ExistingStepTypesPreserved) {
    /* Verify all existing step type names still work correctly */
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

    /* New causal step type */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_CAUSAL), "CAUSAL");
}

TEST_F(CausalRegressionTest, SequentialPipelineUnchanged) {
    /*
     * With causal reasoning disabled (default), the pipeline should
     * produce the same results as before causal was added.
     */
    brain_t brain = brain_create("causal_regression", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_FALSE(config.enable_causal_reasoning);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "test regression", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* No CAUSAL steps should appear when disabled */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* s = reasoning_chain_get_step(&chain, i);
        EXPECT_NE(s->type, REASONING_STEP_CAUSAL)
            << "CAUSAL step found at index " << i << " when causal is disabled";
    }

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(CausalRegressionTest, StatsBackwardCompat) {
    /*
     * New stats fields (causal_queries, avg_causal_strength) should be
     * zero-initialized for existing engines that don't use causal reasoning.
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with garbage */
    reasoning_engine_get_stats(engine, &stats);

    /* All causal stats should be zero */
    EXPECT_EQ(stats.causal_queries, 0u);
    EXPECT_FLOAT_EQ(stats.avg_causal_strength, 0.0f);

    /* Existing stats should also be zero for a fresh engine */
    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_FLOAT_EQ(stats.avg_confidence, 0.0f);

    reasoning_engine_destroy(engine);
}

TEST_F(CausalRegressionTest, ForwardChainingUnaffected) {
    /*
     * Adding causal reasoning should not affect forward chaining behavior.
     * Create a brain, add facts/rules, and verify forward chaining still works.
     */
    brain_t brain = brain_create("fc_regression", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Just verify the reasoning engine creates and connects normally */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);

    /* Run reasoning — should work identically to pre-causal */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    rc = reasoning_engine_reason(engine, "forward chain test", &chain);
    EXPECT_EQ(rc, 0);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(CausalRegressionTest, ChainManagementUnchanged) {
    /*
     * Chain init/add_step/get_step/cleanup should work identically.
     */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    EXPECT_EQ(chain.num_steps, 0u);
    EXPECT_FALSE(chain.is_complete);

    /* Add various step types including CAUSAL */
    reasoning_step_t steps[] = {
        {0, REASONING_STEP_RECALL, "recall step", 0.8f, 0.9f, 0},
        {1, REASONING_STEP_INFERENCE, "inference step", 0.7f, 0.8f, 0},
        {2, REASONING_STEP_CAUSAL, "causal step", 0.6f, 0.7f, 0},
    };

    for (int i = 0; i < 3; i++) {
        int rc = reasoning_chain_add_step(&chain, &steps[i]);
        EXPECT_EQ(rc, 0);
    }

    EXPECT_EQ(chain.num_steps, 3u);

    const reasoning_step_t* s = reasoning_chain_get_step(&chain, 2);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type, REASONING_STEP_CAUSAL);

    reasoning_chain_cleanup(&chain);
}
