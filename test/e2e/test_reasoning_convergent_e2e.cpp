/**
 * @file test_reasoning_convergent_e2e.cpp
 * @brief End-to-end test for convergent reasoning with a full brain
 *
 * WHAT: Tests convergent reasoning with maximum module participation
 * WHY:  Verify the full pipeline from brain creation through convergent
 *       evidence accumulation to conclusion synthesis
 * HOW:  Creates a brain, connects reasoning engine, runs queries,
 *       validates results
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

class ConvergentE2ETest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("convergent_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

TEST_F(ConvergentE2ETest, FullBrainConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with convergent reasoning */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Complex queries with counterfactual ("would"+"had"), causal, and logical
     * keywords to score above COMPLEX threshold for convergent routing */
    const char* queries[] = {
        "What if consciousness had been purely computational, would subjective "
        "experience still emerge, and therefore would free will be invalid "
        "because of determinism?",
        "What if memory consolidation had failed during sleep, would learning "
        "still occur, and therefore would knowledge decay because of the "
        "absence of replay?",
        "What if dreaming had served no purpose, would the brain still "
        "generate hallucinations, and therefore would cognition degrade "
        "because of unfiltered noise?",
        "What if artificial intelligence had achieved embodiment, would it "
        "then reason generally, and therefore would understanding emerge "
        "because of grounding?",
    };

    for (int i = 0; i < 4; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        chain.start_time_us = 0;

        rc = reasoning_engine_reason(engine, queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Query " << i << " failed: " << queries[i];
        EXPECT_TRUE(chain.is_complete) << "Query " << i << " incomplete";

        /* Confidence should be non-negative */
        EXPECT_GE(chain.overall_confidence, 0.0f)
            << "Query " << i << " has negative confidence";

        /* Conclusion should not be empty */
        EXPECT_GT(strlen(chain.conclusion), 0u)
            << "Query " << i << " has empty conclusion";

        reasoning_chain_cleanup(&chain);
    }

    /* Verify stats */
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 4u);
    EXPECT_GE(stats.convergent_queries, 1u);

    reasoning_engine_destroy(engine);
}

TEST_F(ConvergentE2ETest, ConvergentThenDisable) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* First run with convergent */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain1;
    reasoning_chain_init(&chain1);
    chain1.start_time_us = 0;
    int rc1 = reasoning_engine_reason(engine, "What is gravity?", &chain1);
    EXPECT_EQ(rc1, 0);
    reasoning_chain_cleanup(&chain1);
    reasoning_engine_destroy(engine);

    /* Now run without convergent (wave pipeline) */
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = true;
    engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    reasoning_chain_t chain2;
    reasoning_chain_init(&chain2);
    chain2.start_time_us = 0;
    int rc2 = reasoning_engine_reason(engine, "What is gravity?", &chain2);
    EXPECT_EQ(rc2, 0);
    EXPECT_TRUE(chain2.is_complete);

    /* Both should succeed — different paths, same result contract */
    reasoning_chain_cleanup(&chain2);
    reasoning_engine_destroy(engine);
}
