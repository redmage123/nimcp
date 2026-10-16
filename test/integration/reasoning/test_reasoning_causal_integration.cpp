/**
 * @file test_reasoning_causal_integration.cpp
 * @brief Integration tests for causal reasoning with the reasoning engine and brain
 *
 * WHAT: Tests causal DAG usage within the reasoning engine pipeline
 * WHY:  Verify causal reasoning integrates correctly with the broader reasoning system
 * HOW:  GTest suite creating engine + brain, running causal queries
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_causal.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class CausalIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = NULL;
    reasoning_engine_t* engine = NULL;

    void SetUp() override {
        brain = brain_create("causal_integration", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = NULL;
        }
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(CausalIntegrationTest, CausalWithEngine) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with causal reasoning enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_causal_reasoning = true;

    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);

    /* Verify engine was created successfully */
    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 0u);
}

TEST_F(CausalIntegrationTest, CausalStepProduced) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /*
     * Build a causal DAG separately and verify that REASONING_STEP_CAUSAL
     * can be added to a reasoning chain (simulating what the engine would do).
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    causal_dag_add_node(dag, "Cause", 0.5f);
    causal_dag_add_node(dag, "Effect", 0.3f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    /* Run an intervention query */
    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_INTERVENTION;
    query.target_id = 1;
    query.intervention_id = 0;
    query.intervention_value = 1.0f;

    causal_result_t result;
    int rc = causal_dag_query(dag, &query, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(result.is_causal);

    /* Add a CAUSAL step to a reasoning chain */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_CAUSAL;
    step.confidence = result.confidence;
    step.relevance = result.causal_strength;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Causal: %s", result.explanation);

    rc = reasoning_chain_add_step(&chain, &step);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(chain.num_steps, 1u);

    const reasoning_step_t* retrieved = reasoning_chain_get_step(&chain, 0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, REASONING_STEP_CAUSAL);
    EXPECT_STREQ(reasoning_step_type_name(retrieved->type), "CAUSAL");

    reasoning_chain_cleanup(&chain);
    causal_dag_destroy(dag);
}

TEST_F(CausalIntegrationTest, CausalDisabled) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Default config has causal disabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_FALSE(config.enable_causal_reasoning);

    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    reasoning_engine_connect_brain(engine, brain);

    /* Run a reasoning query — should not produce CAUSAL steps */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "test query", &chain);
    EXPECT_EQ(rc, 0);

    /* Verify no CAUSAL steps */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* s = reasoning_chain_get_step(&chain, i);
        EXPECT_NE(s->type, REASONING_STEP_CAUSAL);
    }

    reasoning_chain_cleanup(&chain);
}

TEST_F(CausalIntegrationTest, CausalWithBrain) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Full brain + causal DAG integration */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_causal_reasoning = true;

    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Build a causal model for the brain's domain */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    causal_dag_add_node(dag, "Input", 0.5f);
    causal_dag_add_node(dag, "Processing", 0.5f);
    causal_dag_add_node(dag, "Output", 0.3f);
    causal_dag_add_edge(dag, 0, 1, 0.9f);
    causal_dag_add_edge(dag, 1, 2, 0.8f);

    /* Validate the DAG */
    EXPECT_EQ(causal_dag_validate(dag), 0);

    /* Query causal effect */
    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_INTERVENTION;
    query.target_id = 2;
    query.intervention_id = 0;
    query.intervention_value = 1.0f;

    causal_result_t result;
    causal_dag_query(dag, &query, &result);
    EXPECT_TRUE(result.is_causal);
    EXPECT_GT(result.probability, 0.3f);

    causal_dag_destroy(dag);
}

TEST_F(CausalIntegrationTest, SmokersCausalExample) {
    /*
     * Classic example from Pearl's "The Book of Why":
     *   Genetics -> Smoking
     *   Genetics -> Cancer
     *   Smoking -> Cancer
     *
     * Association P(Cancer|Smoking) conflates genetic and smoking effects.
     * Intervention P(Cancer|do(Smoking)) isolates the causal effect of smoking.
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    int genetics = causal_dag_add_node(dag, "Genetics", 0.2f);
    int smoking = causal_dag_add_node(dag, "Smoking", 0.3f);
    int cancer = causal_dag_add_node(dag, "Cancer", 0.05f);

    causal_dag_add_edge(dag, (uint32_t)genetics, (uint32_t)smoking, 0.6f);
    causal_dag_add_edge(dag, (uint32_t)genetics, (uint32_t)cancer, 0.5f);
    causal_dag_add_edge(dag, (uint32_t)smoking, (uint32_t)cancer, 0.4f);

    /* Association: includes confounded path via genetics */
    causal_query_t assoc_q;
    memset(&assoc_q, 0, sizeof(assoc_q));
    assoc_q.type = CAUSAL_QUERY_ASSOCIATION;
    assoc_q.target_id = (uint32_t)cancer;
    assoc_q.condition_ids[0] = (uint32_t)smoking;
    assoc_q.num_conditions = 1;

    causal_result_t assoc_r;
    causal_dag_query(dag, &assoc_q, &assoc_r);
    EXPECT_FALSE(assoc_r.is_causal);

    /* Intervention: isolates smoking->cancer causal effect */
    causal_query_t interv_q;
    memset(&interv_q, 0, sizeof(interv_q));
    interv_q.type = CAUSAL_QUERY_INTERVENTION;
    interv_q.target_id = (uint32_t)cancer;
    interv_q.intervention_id = (uint32_t)smoking;
    interv_q.intervention_value = 1.0f;

    causal_result_t interv_r;
    causal_dag_query(dag, &interv_q, &interv_r);
    EXPECT_TRUE(interv_r.is_causal);

    /* Both should have meaningful probabilities */
    EXPECT_GT(assoc_r.probability, 0.05f);
    EXPECT_GT(interv_r.probability, 0.05f);

    causal_dag_destroy(dag);
}

TEST_F(CausalIntegrationTest, CausalStats) {
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.3f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    /* Run multiple queries of different types */
    causal_query_t query;
    causal_result_t result;

    /* Association */
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_ASSOCIATION;
    query.target_id = 1;
    query.condition_ids[0] = 0;
    query.num_conditions = 1;
    causal_dag_query(dag, &query, &result);

    /* Intervention */
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_INTERVENTION;
    query.target_id = 1;
    query.intervention_id = 0;
    query.intervention_value = 1.0f;
    causal_dag_query(dag, &query, &result);

    /* Counterfactual */
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_COUNTERFACTUAL;
    query.target_id = 1;
    query.intervention_id = 0;
    query.intervention_value = 0.5f;
    causal_dag_query(dag, &query, &result);

    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_queries, 3u);
    EXPECT_EQ(stats.num_interventions, 1u);
    EXPECT_EQ(stats.num_counterfactuals, 1u);

    causal_dag_destroy(dag);
}

TEST_F(CausalIntegrationTest, CausalWithConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Enable both convergent and causal reasoning */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_causal_reasoning = true;

    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Run a reasoning query to verify both systems coexist */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "What causes learning?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}
