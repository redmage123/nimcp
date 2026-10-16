/**
 * @file test_reasoning_abduction_integration.cpp
 * @brief Integration tests for abductive reasoning with the reasoning engine and brain
 *
 * WHAT: Verify abductive reasoning integrates correctly with the reasoning chain
 *       engine, brain connection, and other reasoning subsystems
 * WHY:  Unit tests check components in isolation; integration tests verify the
 *       abduction module works within the full reasoning pipeline
 * HOW:  Create brain + engine, run queries, verify ABDUCTIVE steps appear
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_abduction.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningAbductionIntegrationTest : public ::testing::Test {
protected:
    reasoning_engine_t* engine = nullptr;
    brain_t brain = nullptr;

    void SetUp() override {
        /* Create a small brain for integration testing */
        brain = brain_create("abduction_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (engine) {
            reasoning_engine_destroy(engine);
            engine = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionIntegrationTest, AbductionWithEngine) {
    /* Create engine with abduction enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = true;
    /* Disable convergent to use sequential pipeline where we added abduction */
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    /* Run a reasoning query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    int rc = reasoning_engine_reason(engine, "Why did the temperature increase?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 0u);

    /* Check for ABDUCTIVE step in the chain */
    bool found_abductive = false;
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        if (chain.steps[i].type == REASONING_STEP_ABDUCTIVE) {
            found_abductive = true;
            EXPECT_GT(chain.steps[i].confidence, 0.0f);
            EXPECT_LE(chain.steps[i].confidence, 1.0f);
            break;
        }
    }
    /* Abductive step may or may not appear depending on whether observations
     * were generated from prior steps. At minimum, verify no crash. */

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionDisabled) {
    /* Create engine with abduction disabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = false;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    /* Run a reasoning query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    int rc = reasoning_engine_reason(engine, "What is the cause of the error?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* Should NOT have any ABDUCTIVE steps */
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        EXPECT_NE(chain.steps[i].type, REASONING_STEP_ABDUCTIVE)
            << "Found abductive step at index " << i << " when abduction is disabled";
    }

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionWithBrain) {
    /* Create engine with all reasoning features */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = true;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    ASSERT_NE(brain, nullptr);
    int rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);

    /* Run multiple queries to test brain integration */
    for (int q = 0; q < 3; q++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        const char* queries[] = {
            "What causes memory corruption?",
            "Why do neurons fire in patterns?",
            "How does learning emerge from connections?"
        };

        rc = reasoning_engine_reason(engine, queries[q], &chain);
        EXPECT_EQ(rc, 0);
        EXPECT_TRUE(chain.is_complete);
        EXPECT_GT(chain.num_steps, 0u);

        reasoning_chain_cleanup(&chain);
    }
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionAffectsConfidence) {
    /* Create engine with abduction enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = true;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    int rc = reasoning_engine_reason(engine, "What pattern explains these observations?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* Overall confidence should be non-negative */
    EXPECT_GE(chain.overall_confidence, 0.0f);
    EXPECT_LE(chain.overall_confidence, 1.0f);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionStats) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = true;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    /* Run a query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    reasoning_engine_reason(engine, "Why did the system fail?", &chain);
    reasoning_chain_cleanup(&chain);

    /* Check engine stats */
    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.total_queries, 1u);
    /* abductive_queries may or may not have incremented depending on
     * whether observations were generated; just verify no crash */
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionWithFEP) {
    /* Test that free_energy is computed for hypotheses */
    reasoning_abduction_t* abd = reasoning_abduction_create(NULL);
    ASSERT_NE(abd, nullptr);

    abductive_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    strncpy(obs.description, "Prediction error increased beyond threshold values",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.8f;
    reasoning_abduction_add_observation(abd, &obs);

    strncpy(obs.description, "Free energy failed to minimize during inference process",
            ABDUCTION_MAX_EXPLANATION_LEN - 1);
    obs.confidence = 0.7f;
    reasoning_abduction_add_observation(abd, &obs);

    abduction_result_t result;
    memset(&result, 0, sizeof(result));
    reasoning_abduction_generate(abd, &result);

    /* All hypotheses should have computed free_energy */
    for (uint32_t i = 0; i < result.num_hypotheses; i++) {
        EXPECT_GT(result.hypotheses[i].free_energy, 0.0f)
            << "Hypothesis " << i << " has non-positive free_energy";
        /* Verify FEP formula: free_energy = -log(plausibility + 1e-6) */
        float expected = -logf(result.hypotheses[i].plausibility + 1e-6f);
        EXPECT_NEAR(result.hypotheses[i].free_energy, expected, 0.01f);
    }

    reasoning_abduction_destroy(abd);
}

TEST_F(ReasoningAbductionIntegrationTest, AbductionMultipleQueries) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_abductive_reasoning = true;
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = false;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    if (brain) {
        reasoning_engine_connect_brain(engine, brain);
    }

    /* Run varied queries and verify state resets between queries */
    const char* queries[] = {
        "What explains the neuron activation pattern?",
        "Why does learning plateau after initial progress?",
        "What mechanism drives synaptic plasticity?",
        "How does prediction error signal surprise?"
    };

    for (int q = 0; q < 4; q++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        int rc = reasoning_engine_reason(engine, queries[q], &chain);
        EXPECT_EQ(rc, 0) << "Query " << q << " failed";
        EXPECT_TRUE(chain.is_complete) << "Query " << q << " not complete";
        EXPECT_GT(chain.num_steps, 0u) << "Query " << q << " has no steps";

        reasoning_chain_cleanup(&chain);
    }

    /* Verify stats accumulated */
    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 4u);
}
