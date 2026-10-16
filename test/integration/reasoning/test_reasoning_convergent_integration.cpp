/**
 * @file test_reasoning_convergent_integration.cpp
 * @brief Integration tests for convergent reasoning with a live brain
 *
 * WHAT: Tests convergent reasoning connected to a real brain instance
 * WHY:  Verify multi-module contributor activation, fallback paths,
 *       bridge interactions, and training API integration
 * HOW:  GTest suite with brain create/destroy lifecycle
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
#include "cognitive/training/nimcp_training_integration.h"
#include "core/brain/nimcp_brain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ConvergentIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = NULL;

    void SetUp() override {
        /* Create a brain with default configuration */
        brain = brain_create("convergent_test", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 4, 2);
        /* brain_create may return NULL in constrained test environments */
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = NULL;
        }
    }
};

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ConvergentIntegrationTest, ConvergentWithBrain) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Create reasoning engine with convergent mode enabled */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    /* Connect to brain */
    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    /* Run convergent reasoning */
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "What is the meaning of life?", &chain);
    EXPECT_EQ(rc, 0);

    /* Chain should have some steps from convergent contributors */
    /* May be 0 if no brain modules are available, but should not crash */
    EXPECT_TRUE(chain.is_complete);

    /* Check stats */
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_GT(stats.total_queries, 0u);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(ConvergentIntegrationTest, FallbackToWavePipeline) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Disable convergent mode, enable concurrent */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.enable_convergent_reasoning = false;
    config.enable_concurrent_pipeline = true;

    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    rc = reasoning_engine_reason(engine, "What is 2+2?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    /* With convergent disabled, convergent_queries should be 0 */
    reasoning_engine_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    reasoning_engine_get_stats(engine, &stats);
    EXPECT_EQ(stats.convergent_queries, 0u);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(ConvergentIntegrationTest, PortiaDowngrade) {
    /* Test that when Portia budget disables convergent, it falls through */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);

    /* Simulate SEVERE budget */
    reasoning_budget_t budget = reasoning_portia_full_budget();
    budget.allow_convergent_mode = false;
    budget.source_degradation = PORTIA_DEGRADATION_SEVERE;

    int disabled = reasoning_portia_apply_budget(&config, &budget);
    EXPECT_GE(disabled, 0);
    EXPECT_FALSE(config.enable_convergent_reasoning);
}

TEST_F(ConvergentIntegrationTest, HypoFightOrFlight) {
    /* Test that FIGHT_OR_FLIGHT forces wave pipeline */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);
    ASSERT_TRUE(config.enable_concurrent_pipeline);

    reasoning_hypo_modulation_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.hypothalamus_available = true;
    mod.urgency_mode = REASONING_URGENCY_FIGHT_OR_FLIGHT;
    mod.force_sequential = true;
    mod.force_wave_pipeline = true;
    mod.cognitive_capacity = 0.3f;
    mod.recommended_max_steps = 10;

    reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_FALSE(config.enable_convergent_reasoning);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
    EXPECT_LE(config.max_steps, 10u);
}

TEST_F(ConvergentIntegrationTest, DomainRestrictedConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int rc = reasoning_engine_connect_brain(engine, brain);
    ASSERT_EQ(rc, 0);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    chain.start_time_us = 0;

    /* Domain-restricted reasoning should also use convergent path */
    rc = reasoning_engine_reason_in_domain(engine, "Explain photosynthesis", 1, &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(chain.is_complete);

    reasoning_chain_cleanup(&chain);
    reasoning_engine_destroy(engine);
}

TEST_F(ConvergentIntegrationTest, TrainingAPIConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    /* Initialize reasoning engine via training API */
    int rc = brain_ti_init_reasoning(brain);
    EXPECT_EQ(rc, 0);

    /* Run reasoning */
    float confidence = brain_ti_reason(brain, "What is gravity?");
    /* Confidence can be low if modules are minimal, but should not be -1.0 */
    EXPECT_GE(confidence, 0.0f);

    /* Check step count */
    uint32_t steps = brain_ti_get_reasoning_steps(brain);
    /* May be 0 with minimal brain, but function should work */
    (void)steps;

    brain_ti_destroy_reasoning(brain);
}

TEST_F(ConvergentIntegrationTest, TrainingAPIIsConvergent) {
    if (!brain) GTEST_SKIP() << "Brain creation failed";

    int rc = brain_ti_init_reasoning(brain);
    EXPECT_EQ(rc, 0);

    /* Use a complex query so metacognition routes to convergent mode
     * (needs counterfactual "would"+"had", causal, and logical keywords
     * to score above the COMPLEX threshold) */
    brain_ti_reason(brain, "What if entropy had been reversible, would "
                    "equilibrium still emerge, and therefore would the "
                    "second law be invalid because of symmetry?");

    /* Check if convergent was used */
    bool is_convergent = brain_ti_is_convergent_reasoning(brain);
    /* Should be true since default config enables convergent */
    EXPECT_TRUE(is_convergent);

    brain_ti_destroy_reasoning(brain);
}
