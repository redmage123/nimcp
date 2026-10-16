/**
 * @file test_reasoning_abduction_e2e.cpp
 * @brief End-to-end tests for abductive reasoning in the full NIMCP pipeline
 *
 * WHAT: Verify abductive reasoning works in complete brain+engine pipeline
 * WHY:  E2E tests catch integration issues missed by unit/integration tests
 * HOW:  Create brain, connect engine, run varied reasoning queries
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

class ReasoningAbductionE2ETest : public ::testing::Test {
protected:
    reasoning_engine_t* engine = nullptr;
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("abduction_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr);

        reasoning_engine_config_t config = reasoning_engine_default_config();
        config.enable_abductive_reasoning = true;
        /* Use sequential pipeline to test abductive phase directly */
        config.enable_convergent_reasoning = false;
        config.enable_concurrent_pipeline = false;
        engine = reasoning_engine_create(&config);
        ASSERT_NE(engine, nullptr);

        int rc = reasoning_engine_connect_brain(engine, brain);
        ASSERT_EQ(rc, 0);
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
 * E2E TESTS
 *===========================================================================*/

TEST_F(ReasoningAbductionE2ETest, AbductiveReasoningFullPipeline) {
    /* Run a full reasoning pipeline with abduction enabled */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine,
        "Why do neural networks sometimes fail to converge during training?",
        &chain);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 0u);
    EXPECT_GE(chain.overall_confidence, 0.0f);
    EXPECT_LE(chain.overall_confidence, 1.0f);

    /* Verify the chain has expected pipeline phases */
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

    /* Verify chain has a valid conclusion */
    EXPECT_GT(strlen(chain.conclusion), 0u);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningAbductionE2ETest, AbductionWithConvergentDisabled) {
    /* Verify abduction and sequential pipeline work together */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine,
        "What mechanism explains synaptic plasticity in the hippocampus?",
        &chain);

    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 2u);  /* Should have at least decomposition + inference + synthesis */

    /* Verify timing information */
    EXPECT_GT(chain.end_time_us, chain.start_time_us);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningAbductionE2ETest, MultipleAbductiveQueries) {
    /* Run varied queries to test consistent results */
    const char* queries[] = {
        "What explains the observed increase in prediction errors?",
        "Why does the brain prefer certain neural pathways over others?",
        "How can we explain the emergence of consciousness from neurons?",
        "What accounts for the difference between fast and slow thinking?",
        "Why do memories sometimes fade while others persist permanently?"
    };

    for (int q = 0; q < 5; q++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        int rc = reasoning_engine_reason(engine, queries[q], &chain);
        EXPECT_EQ(rc, 0) << "Query " << q << " failed: " << queries[q];
        EXPECT_TRUE(chain.is_complete) << "Query " << q << " not complete";
        EXPECT_GT(chain.num_steps, 0u) << "Query " << q << " has no steps";
        EXPECT_GE(chain.overall_confidence, 0.0f);
        EXPECT_LE(chain.overall_confidence, 1.0f);

        /* Verify each step has valid data */
        for (uint32_t i = 0; i < chain.num_steps; i++) {
            EXPECT_GE(chain.steps[i].confidence, 0.0f);
            EXPECT_LE(chain.steps[i].confidence, 1.0f);
            EXPECT_GT(strlen(chain.steps[i].description), 0u)
                << "Step " << i << " in query " << q << " has empty description";
        }

        reasoning_chain_cleanup(&chain);
    }

    /* Verify cumulative stats */
    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 5u);
    EXPECT_EQ(stats.successful_queries, 5u);
    EXPECT_GT(stats.avg_steps_per_query, 0.0f);
}
