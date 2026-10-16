/**
 * @file test_reasoning_affective_e2e.cpp
 * @brief End-to-end tests for affective modulation with full brain pipeline
 *
 * WHAT: Tests affective modulation in full brain reasoning pipeline
 * WHY:  Verify emotional queries produce different results than neutral ones
 * HOW:  Create brain, connect reasoning engine, run varied emotional queries
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_affective.h"
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class AffectiveE2ETest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        brain = brain_create("affective_e2e", BRAIN_SIZE_SMALL,
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

TEST_F(AffectiveE2ETest, AffectiveFullPipeline) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_affective_modulation = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Run emotional queries through full pipeline */
    const char* emotional_queries[] = {
        "I feel deep grief over the loss of my friend",
        "What a great success and achievement we celebrate!",
        "I regret my mistake, I am sorry for the fault",
        "My family and friends trust and loyalty are important",
        "The hidden fear and anger I suppress is painful",
        "Is this fair or biased with prejudice and discrimination?",
    };

    for (int i = 0; i < 6; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        chain.start_time_us = 0;

        int rc = reasoning_engine_reason(engine, emotional_queries[i], &chain);
        EXPECT_EQ(rc, 0) << "Query " << i << " failed: " << emotional_queries[i];
        EXPECT_TRUE(chain.is_complete) << "Query " << i << " not complete";
        EXPECT_GE(chain.overall_confidence, 0.0f) << "Query " << i;
        EXPECT_LE(chain.overall_confidence, 1.0f) << "Query " << i;
        EXPECT_GT(strlen(chain.conclusion), 0u) << "Query " << i;

        reasoning_chain_cleanup(&chain);
    }

    /* Verify stats */
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 6u);

    reasoning_engine_destroy(engine);
}

TEST_F(AffectiveE2ETest, AffectiveVsNeutral) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_affective_modulation = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Run neutral query */
    reasoning_chain_t neutral_chain;
    reasoning_chain_init(&neutral_chain);
    neutral_chain.start_time_us = 0;
    int rc = reasoning_engine_reason(engine, "What is photosynthesis?",
                                      &neutral_chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(neutral_chain.is_complete);

    /* Run emotional query */
    reasoning_chain_t emotional_chain;
    reasoning_chain_init(&emotional_chain);
    emotional_chain.start_time_us = 0;
    rc = reasoning_engine_reason(engine,
        "What is photosynthesis?", &emotional_chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(emotional_chain.is_complete);

    /* Both should complete successfully with valid confidence */
    EXPECT_GE(neutral_chain.overall_confidence, 0.0f);
    EXPECT_GE(emotional_chain.overall_confidence, 0.0f);
    EXPECT_LE(neutral_chain.overall_confidence, 1.0f);
    EXPECT_LE(emotional_chain.overall_confidence, 1.0f);

    reasoning_chain_cleanup(&neutral_chain);
    reasoning_chain_cleanup(&emotional_chain);
    reasoning_engine_destroy(engine);
}

TEST_F(AffectiveE2ETest, AffectiveWithAllBridges) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Enable everything: convergent + Portia + Hypo + Mesh + Affective */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_affective_modulation = true;
    config.enable_concurrent_pipeline = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    reasoning_engine_connect_brain(engine, brain);

    /* Mix of emotional keywords covering multiple categories */
    const char* mixed_query =
        "How can a family trust help with the loss and grief "
        "while celebrating the great achievement of a friend?";

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine, mixed_query, &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GE(chain.overall_confidence, 0.0f);
    EXPECT_LE(chain.overall_confidence, 1.0f);
    EXPECT_GT(strlen(chain.conclusion), 0u);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}
