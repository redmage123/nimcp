/**
 * @file test_reasoning_visuospatial_regression.cpp
 * @brief Regression tests for visuospatial reasoning -- ensure existing behavior unchanged
 *
 * WHAT: Verify that adding visuospatial reasoning does not break existing functionality
 * WHY:  Guard against regressions in the reasoning pipeline when visuospatial is added
 * HOW:  Test default config, step types, pipeline behavior without visuospatial enabled
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_visuospatial.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class VisuospatialRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * REGRESSION TESTS
 *===========================================================================*/

TEST_F(VisuospatialRegressionTest, DefaultConfigDisabled) {
    /*
     * Visuospatial reasoning is opt-in (default false).
     * Existing users who don't set enable_visuospatial_reasoning should get
     * identical behavior to before this feature was added.
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_FALSE(config.enable_visuospatial_reasoning);

    /* All core defaults should still be true */
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

TEST_F(VisuospatialRegressionTest, ExistingStepTypesPreserved) {
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
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_CAUSAL), "CAUSAL");

    /* New visuospatial step type */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_VISUOSPATIAL), "VISUOSPATIAL");
}

TEST_F(VisuospatialRegressionTest, SequentialPipelineUnchanged) {
    /*
     * With visuospatial reasoning disabled (default), the pipeline should
     * produce the same results as before visuospatial was added.
     */
    brain_t brain = brain_create("vs_regression", BRAIN_SIZE_SMALL,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_FALSE(config.enable_visuospatial_reasoning);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "test regression", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* No VISUOSPATIAL steps should appear when disabled */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* s = reasoning_chain_get_step(&chain, i);
        EXPECT_NE(s->type, REASONING_STEP_VISUOSPATIAL)
            << "VISUOSPATIAL step found at index " << i << " when visuospatial is disabled";
    }

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
    brain_destroy(brain);
}

TEST_F(VisuospatialRegressionTest, StatsBackwardCompat) {
    /*
     * New stats field (visuospatial_queries) should be zero-initialized
     * for existing engines that don't use visuospatial reasoning.
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with garbage */
    reasoning_engine_get_stats(engine, &stats);

    /* Visuospatial stat should be zero */
    EXPECT_EQ(stats.visuospatial_queries, 0u);

    /* Existing stats should also be zero for a fresh engine */
    EXPECT_EQ(stats.total_queries, 0u);
    EXPECT_FLOAT_EQ(stats.avg_confidence, 0.0f);

    reasoning_engine_destroy(engine);
}

TEST_F(VisuospatialRegressionTest, ConvergentUnchanged) {
    /*
     * Adding visuospatial reasoning should not affect convergent reasoning.
     * Create an engine with convergent enabled (default), visuospatial disabled,
     * and verify convergent stats are still zero-initialized.
     */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);
    ASSERT_FALSE(config.enable_visuospatial_reasoning);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);

    EXPECT_EQ(stats.convergent_queries, 0u);
    EXPECT_FLOAT_EQ(stats.avg_convergent_contributors, 0.0f);

    reasoning_engine_destroy(engine);
}

TEST_F(VisuospatialRegressionTest, ChainManagementUnchanged) {
    /*
     * Chain init/add_step/get_step/cleanup should work identically.
     */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    EXPECT_EQ(chain.num_steps, 0u);
    EXPECT_FALSE(chain.is_complete);

    /* Add various step types including VISUOSPATIAL */
    reasoning_step_t steps[] = {
        {0, REASONING_STEP_RECALL, "recall step", 0.8f, 0.9f, 0},
        {1, REASONING_STEP_INFERENCE, "inference step", 0.7f, 0.8f, 0},
        {2, REASONING_STEP_VISUOSPATIAL, "visuospatial step", 0.6f, 0.7f, 0},
    };

    for (int i = 0; i < 3; i++) {
        int rc = reasoning_chain_add_step(&chain, &steps[i]);
        EXPECT_EQ(rc, 0);
    }

    EXPECT_EQ(chain.num_steps, 3u);

    const reasoning_step_t* s = reasoning_chain_get_step(&chain, 2);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type, REASONING_STEP_VISUOSPATIAL);

    reasoning_chain_cleanup(&chain);
}

TEST_F(VisuospatialRegressionTest, VisuospatialDefaultConfig) {
    /*
     * The visuospatial subsystem's own default config should be sensible.
     */
    visuospatial_config_t cfg = reasoning_visuospatial_default_config();
    EXPECT_EQ(cfg.max_objects, (uint32_t)VS_MAX_OBJECTS);
    EXPECT_FLOAT_EQ(cfg.proximity_threshold, VS_DEFAULT_PROXIMITY_THRESHOLD);
    EXPECT_FALSE(cfg.enable_3d);
}
