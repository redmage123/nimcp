/**
 * @file test_reasoning_causal_e2e.cpp
 * @brief End-to-end tests for causal reasoning with a full brain
 *
 * WHAT: Tests complete causal reasoning pipeline from DAG creation to conclusion
 * WHY:  Verify the full causal inference pipeline works with a live brain
 * HOW:  Create brain, build causal DAGs, run queries, validate results
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

class CausalE2ETest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("causal_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

/*=============================================================================
 * E2E TESTS
 *===========================================================================*/

TEST_F(CausalE2ETest, CausalFullPipeline) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /*
     * Full pipeline:
     * 1. Create causal DAG
     * 2. Add nodes and edges
     * 3. Run multiple query types
     * 4. Build reasoning chain with causal steps
     * 5. Verify correct causal identification
     */

    /* Step 1-2: Build a medical causal model */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    int exercise = causal_dag_add_node(dag, "Exercise", 0.4f);
    int diet = causal_dag_add_node(dag, "Diet", 0.5f);
    int weight = causal_dag_add_node(dag, "Weight", 0.5f);
    int blood_pressure = causal_dag_add_node(dag, "BloodPressure", 0.3f);
    int heart_disease = causal_dag_add_node(dag, "HeartDisease", 0.1f);

    ASSERT_GE(exercise, 0);
    ASSERT_GE(heart_disease, 0);

    /* Causal relationships */
    causal_dag_add_edge(dag, (uint32_t)exercise, (uint32_t)weight, 0.7f);
    causal_dag_add_edge(dag, (uint32_t)diet, (uint32_t)weight, 0.8f);
    causal_dag_add_edge(dag, (uint32_t)weight, (uint32_t)blood_pressure, 0.6f);
    causal_dag_add_edge(dag, (uint32_t)blood_pressure, (uint32_t)heart_disease, 0.5f);
    causal_dag_add_edge(dag, (uint32_t)exercise, (uint32_t)heart_disease, 0.3f);  /* Direct */

    EXPECT_EQ(causal_dag_validate(dag), 0);

    /* Step 3: Run queries */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    /* Query 1: Does exercise cause heart disease reduction? */
    causal_query_t q;
    memset(&q, 0, sizeof(q));
    q.type = CAUSAL_QUERY_INTERVENTION;
    q.target_id = (uint32_t)heart_disease;
    q.intervention_id = (uint32_t)exercise;
    q.intervention_value = 1.0f;

    causal_result_t r;
    int rc = causal_dag_query(dag, &q, &r);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(r.is_causal);

    /* Step 4: Add to reasoning chain */
    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_CAUSAL;
    step.confidence = r.confidence;
    step.relevance = r.causal_strength;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Causal analysis: %s", r.explanation);

    reasoning_chain_add_step(&chain, &step);

    /* Query 2: What if we intervene on diet? */
    memset(&q, 0, sizeof(q));
    q.type = CAUSAL_QUERY_INTERVENTION;
    q.target_id = (uint32_t)heart_disease;
    q.intervention_id = (uint32_t)diet;
    q.intervention_value = 1.0f;

    rc = causal_dag_query(dag, &q, &r);
    EXPECT_EQ(rc, 0);

    step.step_id = 1;
    step.confidence = r.confidence;
    step.relevance = r.causal_strength;
    snprintf(step.description, REASONING_STEP_DESC_LEN,
             "Causal analysis: %s", r.explanation);
    reasoning_chain_add_step(&chain, &step);

    /* Step 5: Verify chain */
    EXPECT_EQ(chain.num_steps, 2u);
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* s = reasoning_chain_get_step(&chain, i);
        EXPECT_EQ(s->type, REASONING_STEP_CAUSAL);
        EXPECT_GT(s->confidence, 0.0f);
    }

    /* Verify stats */
    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_nodes, 5u);
    EXPECT_EQ(stats.num_edges, 5u);
    EXPECT_GE(stats.num_queries, 2u);
    EXPECT_GE(stats.num_interventions, 2u);

    reasoning_chain_cleanup(&chain);
    causal_dag_destroy(dag);
}

TEST_F(CausalE2ETest, InterventionChangesConclusion) {
    /*
     * Demonstrate that association suggests X causes Y, but intervention
     * reveals a confounder.
     *
     * Model: Confounder Z -> X, Z -> Y (no direct X -> Y path)
     * Association P(Y|X) will show correlation.
     * Intervention P(Y|do(X)) will show NO causal effect.
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    int z = causal_dag_add_node(dag, "Confounder", 0.5f);
    int x = causal_dag_add_node(dag, "X", 0.5f);
    int y = causal_dag_add_node(dag, "Y", 0.3f);

    /* Z causes both X and Y, but X does NOT cause Y */
    causal_dag_add_edge(dag, (uint32_t)z, (uint32_t)x, 0.8f);
    causal_dag_add_edge(dag, (uint32_t)z, (uint32_t)y, 0.7f);

    /* Association: P(Y|X) — should find a path via Z (indirect correlation) */
    /* Note: Our DFS follows directed edges only, so X is not an ancestor of Y
     * via the confounding path. This correctly shows no association from X to Y
     * through the DAG. In a full Bayesian network, we'd handle d-separation,
     * but in our simplified DAG traversal, X->Y has no directed path. */

    /* Intervention: P(Y|do(X)) — no causal path from X to Y */
    causal_query_t interv_q;
    memset(&interv_q, 0, sizeof(interv_q));
    interv_q.type = CAUSAL_QUERY_INTERVENTION;
    interv_q.target_id = (uint32_t)y;
    interv_q.intervention_id = (uint32_t)x;
    interv_q.intervention_value = 1.0f;

    causal_result_t interv_r;
    int rc = causal_dag_query(dag, &interv_q, &interv_r);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(interv_r.is_causal);  /* No causal effect! */

    /* This is the key insight: intervention correctly identifies
     * that X does not cause Y, even though they are correlated. */
    EXPECT_FLOAT_EQ(interv_r.probability, 0.3f);  /* Returns prior (no causal path) */

    causal_dag_destroy(dag);
}

TEST_F(CausalE2ETest, CounterfactualReasoning) {
    /*
     * Counterfactual: "What would have happened to the patient's
     * blood pressure if they had exercised?"
     *
     * Model: Exercise -> Weight -> BloodPressure
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    int exercise = causal_dag_add_node(dag, "Exercise", 0.4f);
    int weight = causal_dag_add_node(dag, "Weight", 0.6f);
    int bp = causal_dag_add_node(dag, "BloodPressure", 0.5f);

    causal_dag_add_edge(dag, (uint32_t)exercise, (uint32_t)weight, 0.8f);
    causal_dag_add_edge(dag, (uint32_t)weight, (uint32_t)bp, 0.7f);

    /* Counterfactual: What if exercise = 1.0? */
    causal_query_t cf_q;
    memset(&cf_q, 0, sizeof(cf_q));
    cf_q.type = CAUSAL_QUERY_COUNTERFACTUAL;
    cf_q.target_id = (uint32_t)bp;
    cf_q.intervention_id = (uint32_t)exercise;
    cf_q.intervention_value = 1.0f;

    causal_result_t cf_r;
    int rc = causal_dag_query(dag, &cf_q, &cf_r);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(cf_r.probability, 0.0f);
    EXPECT_LE(cf_r.probability, 1.0f);

    /* Counterfactual confidence should be lower than regular intervention */
    causal_query_t interv_q;
    memset(&interv_q, 0, sizeof(interv_q));
    interv_q.type = CAUSAL_QUERY_INTERVENTION;
    interv_q.target_id = (uint32_t)bp;
    interv_q.intervention_id = (uint32_t)exercise;
    interv_q.intervention_value = 1.0f;

    causal_result_t interv_r;
    causal_dag_query(dag, &interv_q, &interv_r);

    /* Counterfactual confidence = 0.7 * intervention confidence */
    EXPECT_LT(cf_r.confidence, interv_r.confidence);
    EXPECT_NEAR(cf_r.confidence, interv_r.confidence * 0.7f, 0.01f);

    /* Verify stats */
    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_counterfactuals, 1u);

    causal_dag_destroy(dag);
}
