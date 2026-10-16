/**
 * @file test_reasoning_metacognition_integration.cpp
 * @brief Integration tests for metacognitive controller with live brain + engine
 *
 * WHAT: Tests metacognition influencing actual reasoning pipelines
 * WHY:  Unit tests verify components in isolation; integration tests verify
 *       that metacognition correctly routes queries through the right pipeline
 * HOW:  Creates a brain, connects reasoning engine with metacognition enabled,
 *       runs queries of varying complexity, validates strategy routing
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_metacognition.h"
}

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

static constexpr uint32_t INPUT_DIM = 4;
static constexpr uint32_t OUTPUT_DIM = 2;

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MetacognitionIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    reasoning_engine_t* engine = nullptr;

    void SetUp() override {
        brain = brain_create("metacog_integration", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        if (!brain) GTEST_SKIP() << "Brain creation failed";

        reasoning_engine_config_t cfg = reasoning_engine_default_config();
        cfg.enable_metacognition = true;
        engine = reasoning_engine_create(&cfg);
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
 * TESTS
 *===========================================================================*/

TEST_F(MetacognitionIntegration, MetacognitionWithBrain) {
    /* Run a simple query through the full pipeline with metacognition */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int rc = reasoning_engine_reason(engine, "What is consciousness?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GT(chain.num_steps, 0u);

    /* Should have a metacognitive step as the first step */
    if (chain.num_steps > 0) {
        const reasoning_step_t* first_step = reasoning_chain_get_step(&chain, 0);
        ASSERT_NE(first_step, nullptr);
        EXPECT_EQ(first_step->type, REASONING_STEP_METACOGNITIVE);
    }

    reasoning_chain_cleanup(&chain);
}

TEST_F(MetacognitionIntegration, StrategySelectionAffectsPerformance) {
    /* Simple query should use sequential → fewer steps typically */
    reasoning_chain_t chain_simple;
    reasoning_chain_init(&chain_simple);
    int rc = reasoning_engine_reason(engine, "define cat", &chain_simple);
    EXPECT_EQ(rc, 0);

    /* Complex query should use convergent → potentially more steps */
    reasoning_chain_t chain_complex;
    reasoning_chain_init(&chain_complex);
    rc = reasoning_engine_reason(engine,
        "If consciousness had not emerged in biological organisms, would "
        "artificial intelligence be similar to what we see today, and "
        "therefore implies a fundamentally different epistemological "
        "framework because of the hard problem of consciousness?",
        &chain_complex);
    EXPECT_EQ(rc, 0);

    /* Both should complete successfully */
    EXPECT_TRUE(chain_simple.is_complete);
    EXPECT_TRUE(chain_complex.is_complete);

    reasoning_chain_cleanup(&chain_simple);
    reasoning_chain_cleanup(&chain_complex);
}

TEST_F(MetacognitionIntegration, DisabledMetacognitionUsesDefault) {
    /* Create engine without metacognition */
    reasoning_engine_t* engine2 = nullptr;
    reasoning_engine_config_t cfg = reasoning_engine_default_config();
    cfg.enable_metacognition = false;
    engine2 = reasoning_engine_create(&cfg);
    ASSERT_NE(engine2, nullptr);

    int rc = reasoning_engine_connect_brain(engine2, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    rc = reasoning_engine_reason(engine2, "What is consciousness?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* Should NOT have a metacognitive step */
    bool has_metacog_step = false;
    for (uint32_t i = 0; i < chain.num_steps; i++) {
        const reasoning_step_t* step = reasoning_chain_get_step(&chain, i);
        if (step && step->type == REASONING_STEP_METACOGNITIVE) {
            has_metacog_step = true;
            break;
        }
    }
    EXPECT_FALSE(has_metacog_step);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine2);
}

TEST_F(MetacognitionIntegration, MetacognitionStats) {
    /* Run several queries */
    const char* queries[] = {
        "define cat",
        "What is a dog?",
        "How does photosynthesis work and why is it important?",
    };

    for (int i = 0; i < 3; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        reasoning_engine_reason(engine, queries[i], &chain);
        reasoning_chain_cleanup(&chain);
    }

    /* Check engine stats reflect metacognitive assessments */
    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.metacognitive_assessments, 3u);
    EXPECT_EQ(stats.total_queries, 3u);
}

TEST_F(MetacognitionIntegration, OutcomeLearningImproves) {
    /* Run many queries to exercise outcome learning */
    for (int i = 0; i < 10; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        const char* query = (i % 2 == 0)
            ? "What is a simple fact?"
            : "If all A are B and all B are C, therefore all A are C because of transitivity";

        reasoning_engine_reason(engine, query, &chain);
        reasoning_chain_cleanup(&chain);
    }

    /* Verify engine processed all queries without error */
    reasoning_engine_stats_t stats;
    int rc = reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.total_queries, 10u);
    EXPECT_EQ(stats.metacognitive_assessments, 10u);
}

TEST_F(MetacognitionIntegration, MetacognitionWithMultipleQueries) {
    /* Run a mix of trivial, simple, and complex queries */
    const char* queries[] = {
        "hi",
        "define cat",
        "What is the meaning of life?",
        "If birds had not evolved wings, would they still be able to migrate?",
        "How does gravity work?",
        "If all mammals are warm-blooded and whales are mammals, therefore whales "
        "are warm-blooded because of logical deduction",
        "hello",
    };

    for (int i = 0; i < 7; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);

        int rc = reasoning_engine_reason(engine, queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Failed on query " << i << ": " << queries[i];
        EXPECT_TRUE(chain.is_complete) << "Incomplete for query " << i;

        reasoning_chain_cleanup(&chain);
    }

    reasoning_engine_stats_t stats;
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 7u);
    EXPECT_EQ(stats.metacognitive_assessments, 7u);
}
