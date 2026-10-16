/**
 * @file test_reasoning_metacognition_e2e.cpp
 * @brief End-to-end tests for metacognitive adaptive strategy selection
 *
 * WHAT: Full pipeline tests from brain creation through metacognitive dispatch
 * WHY:  Verify the complete metacognition flow with real brain instances
 * HOW:  Creates brains, enables metacognition, runs diverse queries,
 *       validates results and adaptation
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_metacognition.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MetacognitionE2ETest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("metacog_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

/*=============================================================================
 * E2E TESTS
 *===========================================================================*/

TEST_F(MetacognitionE2ETest, FullPipelineWithMetacognition) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with metacognition */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_metacognition);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run mix of trivial/simple/complex queries */
    const char* queries[] = {
        "define cat",                          /* Trivial */
        "What is photosynthesis?",             /* Simple */
        "How does gravity work?",              /* Simple-moderate */
        "If all metals conduct electricity and copper is a metal, "
        "therefore copper conducts electricity because of deductive logic",  /* Complex */
        "If birds had not evolved flight, would ecosystems be fundamentally "
        "different and would predator-prey dynamics be similar to "
        "what we observe in exclusively terrestrial environments?",          /* Hard */
    };

    for (int i = 0; i < 5; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        rc = reasoning_engine_reason(engine, queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Query " << i << " failed: " << queries[i];
        EXPECT_TRUE(chain.is_complete) << "Query " << i << " incomplete";
        EXPECT_GT(chain.num_steps, 0u) << "Query " << i << " no steps";

        /* Confidence should be valid */
        EXPECT_GE(chain.overall_confidence, 0.0f);
        EXPECT_LE(chain.overall_confidence, 1.0f);

        /* Conclusion should not be empty */
        EXPECT_GT(strlen(chain.conclusion), 0u);

        reasoning_chain_cleanup(&chain);
    }

    /* Verify aggregate stats */
    reasoning_engine_stats_t stats;
    rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_queries, 5u);
    EXPECT_EQ(stats.successful_queries, 5u);
    EXPECT_EQ(stats.metacognitive_assessments, 5u);

    reasoning_engine_destroy(engine);
}

TEST_F(MetacognitionE2ETest, AdaptiveStrategyOverMultipleQueries) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run 12 queries of varying complexity to exercise adaptation */
    const char* queries[] = {
        "hi",
        "define cat",
        "What is a dog?",
        "How does DNA replication work?",
        "What is the relationship between entropy and information?",
        "If all A implies B and all B implies C, therefore A implies C",
        "Compare and contrast supervised and unsupervised learning",
        "If quantum mechanics had never been discovered, would computing "
        "technology be similar to what we have today?",
        "hello world",
        "What is 2+2?",
        "Explain how neural networks learn through backpropagation "
        "and how gradient descent relates to loss minimization",
        "If consciousness is an emergent property, therefore reducing "
        "a brain would not eliminate consciousness because of emergence",
    };

    for (int i = 0; i < 12; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        rc = reasoning_engine_reason(engine, queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Query " << i << " failed";

        reasoning_chain_cleanup(&chain);
    }

    reasoning_engine_stats_t stats;
    rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_queries, 12u);
    EXPECT_EQ(stats.metacognitive_assessments, 12u);
    EXPECT_GT(stats.avg_confidence, 0.0f);

    reasoning_engine_destroy(engine);
}

TEST_F(MetacognitionE2ETest, MetacognitionWithAllBridges) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Enable everything: Portia, Hypo, Mesh, Convergent, Metacognition */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_metacognition = true;
    config.enable_convergent_reasoning = true;
    config.enable_concurrent_pipeline = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run queries that exercise different strategy paths */
    reasoning_chain_t chain;

    /* Trivial query */
    reasoning_chain_init(&chain);
    rc = reasoning_engine_reason(engine, "define cat", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    reasoning_chain_cleanup(&chain);

    /* Complex query */
    reasoning_chain_init(&chain);
    rc = reasoning_engine_reason(engine,
        "If socrates is mortal and all mortals die, therefore "
        "socrates will die because of syllogistic reasoning, "
        "but however some argue that consciousness persists",
        &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    reasoning_chain_cleanup(&chain);

    /* Verify stats */
    reasoning_engine_stats_t stats;
    rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_queries, 2u);
    EXPECT_EQ(stats.metacognitive_assessments, 2u);

    reasoning_engine_destroy(engine);
}
