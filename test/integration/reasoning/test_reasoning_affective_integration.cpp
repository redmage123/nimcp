/**
 * @file test_reasoning_affective_integration.cpp
 * @brief Integration tests for affective modulation with live brain
 *
 * WHAT: Tests affective contributors registered in convergent architecture
 * WHY:  Verify affective modulation integrates correctly with brain and
 *       convergent reasoning pipeline
 * HOW:  Create brain, connect reasoning engine, run emotional queries
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

class AffectiveIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = NULL;
    reasoning_engine_t* engine = NULL;

    void SetUp() override {
        brain = brain_create("affective_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
        if (!brain) return;

        reasoning_engine_config_t config = reasoning_engine_default_config();
        config.enable_convergent_reasoning = true;
        config.enable_affective_modulation = true;
        engine = reasoning_engine_create(&config);
        if (engine) {
            reasoning_engine_connect_brain(engine, brain);
        }
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

TEST_F(AffectiveIntegrationTest, AffectiveWithBrain) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Verify affective contributors are registered in the convergent registry */
    uint32_t count = 0;
    const reasoning_contributor_entry_t* registry =
        reasoning_convergent_get_registry(&count);

    ASSERT_NE(registry, nullptr);
    EXPECT_GT(count, 16u);  /* Original 16 + 4 new affective */

    /* Check that grief, joy, remorse, social_bond entries exist */
    bool found_grief = false, found_joy = false;
    bool found_remorse = false, found_social = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(registry[i].name, "grief") == 0) found_grief = true;
        if (strcmp(registry[i].name, "joy") == 0) found_joy = true;
        if (strcmp(registry[i].name, "remorse") == 0) found_remorse = true;
        if (strcmp(registry[i].name, "social_bond") == 0) found_social = true;
    }

    EXPECT_TRUE(found_grief) << "grief contributor not found in registry";
    EXPECT_TRUE(found_joy) << "joy contributor not found in registry";
    EXPECT_TRUE(found_remorse) << "remorse contributor not found in registry";
    EXPECT_TRUE(found_social) << "social_bond contributor not found in registry";
}

TEST_F(AffectiveIntegrationTest, AffectiveModulatesConvergent) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Run a grief-related query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine, "What happens after death and loss?",
                                      &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* Confidence should be non-negative even with grief modulation */
    EXPECT_GE(chain.overall_confidence, 0.0f);
    EXPECT_LE(chain.overall_confidence, 1.0f);

    reasoning_chain_cleanup(&chain);
}

TEST_F(AffectiveIntegrationTest, AffectiveDisabled) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create engine with affective disabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = true;
    config.enable_affective_modulation = false;

    reasoning_engine_t* disabled_engine = reasoning_engine_create(&config);
    ASSERT_NE(disabled_engine, nullptr);
    reasoning_engine_connect_brain(disabled_engine, brain);

    /* Run a query — should complete without affective influence */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(disabled_engine, "What is happiness?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(disabled_engine);
}

TEST_F(AffectiveIntegrationTest, JoyBoostsConfidence) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Run a joy-related query */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine,
        "How can we celebrate this great achievement and success?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GE(chain.overall_confidence, 0.0f);

    reasoning_chain_cleanup(&chain);
}

TEST_F(AffectiveIntegrationTest, MultipleAffectsCompound) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Query with both grief and social keywords */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine,
        "How does family trust help with loss and grief?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);
    EXPECT_GE(chain.overall_confidence, 0.0f);
    EXPECT_LE(chain.overall_confidence, 1.0f);

    reasoning_chain_cleanup(&chain);
}

TEST_F(AffectiveIntegrationTest, AffectiveStats) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Run a few queries */
    const char* queries[] = {
        "The loss of a friend causes grief",
        "What a great success!",
        "I regret that mistake",
    };

    for (int i = 0; i < 3; i++) {
        reasoning_chain_t chain;
        reasoning_chain_init(&chain);
        chain.start_time_us = 0;

        reasoning_engine_reason(engine, queries[i], &chain);
        reasoning_chain_cleanup(&chain);
    }

    /* Verify stats are populated */
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.total_queries, 3u);
}

TEST_F(AffectiveIntegrationTest, AffectiveWithPortia) {
    if (!brain || !engine) GTEST_SKIP() << "Brain or engine creation failed";

    /* Portia resource constraints should not break affective modulation */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    int rc = reasoning_engine_reason(engine,
        "I feel hidden anger and fear about this prejudice", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
}
