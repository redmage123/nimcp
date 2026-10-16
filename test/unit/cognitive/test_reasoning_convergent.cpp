/**
 * @file test_reasoning_convergent.cpp
 * @brief Unit tests for the convergent evidence accumulation architecture
 *
 * WHAT: Tests accumulator, registry, contributor logic, and config integration
 * WHY:  Verify the convergent reasoning components work correctly in isolation
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_convergent.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_portia_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningConvergentTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * ACCUMULATOR TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, AccumulatorInit) {
    evidence_accumulator_t acc;
    int rc = reasoning_accumulator_init(&acc, 10, 0.0f, 0.0f);
    ASSERT_EQ(rc, 0);

    /* Check defaults */
    EXPECT_FLOAT_EQ(acc.ema_alpha, REASONING_DEFAULT_EMA_ALPHA);
    EXPECT_FLOAT_EQ(acc.convergence_threshold, REASONING_DEFAULT_CONVERGENCE_THRESHOLD);
    EXPECT_EQ(acc.total_contributors, 10u);
    EXPECT_EQ(acc.confidence_count, 0u);
    EXPECT_FALSE(acc.converged);
    EXPECT_FLOAT_EQ(acc.current_confidence, 0.0f);
    EXPECT_EQ(acc.modulator_count, 0u);

    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorInitCustomAlpha) {
    evidence_accumulator_t acc;
    int rc = reasoning_accumulator_init(&acc, 5, 0.5f, 0.01f);
    ASSERT_EQ(rc, 0);

    EXPECT_FLOAT_EQ(acc.ema_alpha, 0.5f);
    EXPECT_FLOAT_EQ(acc.convergence_threshold, 0.01f);

    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorInitNull) {
    int rc = reasoning_accumulator_init(NULL, 10, 0.0f, 0.0f);
    EXPECT_EQ(rc, -1);
}

TEST_F(ReasoningConvergentTest, AccumulatorDestroyNull) {
    /* Should not crash */
    reasoning_accumulator_destroy(NULL);
}

TEST_F(ReasoningConvergentTest, AccumulatorSubmitEvidence) {
    evidence_accumulator_t acc;
    reasoning_accumulator_init(&acc, 5, 0.3f, 0.005f);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    /* Create a mock contribution */
    convergent_contribution_t contrib;
    memset(&contrib, 0, sizeof(contrib));
    contrib.module_name = "test_module";
    contrib.role = REASONING_ROLE_EVIDENCE_PRODUCER;
    contrib.result_confidence = 0.7f;
    reasoning_chain_init(&contrib.local_chain);

    /* Add a step to the local chain */
    reasoning_step_t step;
    memset(&step, 0, sizeof(step));
    step.step_id = 0;
    step.type = REASONING_STEP_HIPPOCAMPAL_RECALL;
    step.confidence = 0.7f;
    step.relevance = 0.8f;
    snprintf(step.description, REASONING_STEP_DESC_LEN, "Test step");
    reasoning_chain_add_step(&contrib.local_chain, &step);

    /* Submit evidence */
    int rc = reasoning_accumulator_submit_evidence(&acc, &chain, &contrib);
    ASSERT_EQ(rc, 0);

    /* Check accumulator state */
    EXPECT_EQ(acc.confidence_count, 1u);
    EXPECT_GT(acc.current_confidence, 0.0f);

    /* Check main chain has the step */
    EXPECT_EQ(chain.num_steps, 1u);
    EXPECT_EQ(chain.steps[0].type, REASONING_STEP_HIPPOCAMPAL_RECALL);

    reasoning_chain_cleanup(&contrib.local_chain);
    reasoning_chain_cleanup(&chain);
    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorConvergence) {
    evidence_accumulator_t acc;
    /*
     * EMA starts at 1.0 to avoid premature convergence. With delta=0 on each
     * submission, ema decays: 1.0 → 0.7 → 0.49 → 0.343 → 0.24 → 0.168...
     * Need threshold > 0.168 for convergence in 5 identical submissions,
     * or use more submissions. We use 12 submissions with threshold 0.1
     * which gives EMA ≈ 0.7^11 ≈ 0.020 after 12 identical inputs.
     */
    reasoning_accumulator_init(&acc, 15, 0.3f, 0.1f);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    /* Submit many evidence contributions with identical confidences */
    for (int i = 0; i < 12; i++) {
        convergent_contribution_t contrib;
        memset(&contrib, 0, sizeof(contrib));
        contrib.module_name = "test";
        contrib.role = REASONING_ROLE_EVIDENCE_PRODUCER;
        contrib.result_confidence = 0.6f;  /* Same confidence each time */
        reasoning_chain_init(&contrib.local_chain);

        reasoning_accumulator_submit_evidence(&acc, &chain, &contrib);
        reasoning_chain_cleanup(&contrib.local_chain);
    }

    /* After 12 identical submissions, EMA delta should be well below 0.1 */
    EXPECT_TRUE(acc.converged);
    EXPECT_GE(acc.confidence_count, REASONING_MIN_CONVERGENCE_SUBMISSIONS);
    EXPECT_LT(acc.ema_delta, 0.1f);

    reasoning_chain_cleanup(&chain);
    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorModulation) {
    evidence_accumulator_t acc;
    reasoning_accumulator_init(&acc, 5, 0.3f, 0.005f);

    /* Start with some base confidence */
    acc.current_confidence = 0.5f;

    /* Submit positive modulation */
    reasoning_accumulator_submit_modulation(&acc, 0.1f);
    reasoning_accumulator_submit_modulation(&acc, 0.05f);

    /* Submit negative modulation */
    reasoning_accumulator_submit_modulation(&acc, -0.2f);

    EXPECT_EQ(acc.modulator_count, 3u);
    EXPECT_FLOAT_EQ(acc.total_positive_modulation, 0.15f);
    EXPECT_FLOAT_EQ(acc.total_negative_modulation, -0.2f);

    /* Apply modulation — net is -0.05 */
    float net = reasoning_accumulator_apply_modulation(&acc);
    EXPECT_NEAR(net, -0.05f, 0.001f);
    EXPECT_NEAR(acc.current_confidence, 0.45f, 0.001f);

    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorModulationClamp) {
    evidence_accumulator_t acc;
    reasoning_accumulator_init(&acc, 5, 0.3f, 0.005f);

    acc.current_confidence = 0.5f;

    /* Submit large negative modulation */
    reasoning_accumulator_submit_modulation(&acc, -0.5f);

    /* Should clamp to -0.3 */
    float net = reasoning_accumulator_apply_modulation(&acc);
    EXPECT_FLOAT_EQ(net, -0.3f);
    EXPECT_NEAR(acc.current_confidence, 0.2f, 0.001f);

    reasoning_accumulator_destroy(&acc);
}

TEST_F(ReasoningConvergentTest, AccumulatorIsConverged) {
    evidence_accumulator_t acc;
    reasoning_accumulator_init(&acc, 5, 0.3f, 0.005f);

    EXPECT_FALSE(reasoning_accumulator_is_converged(&acc));

    acc.converged = true;
    EXPECT_TRUE(reasoning_accumulator_is_converged(&acc));

    EXPECT_FALSE(reasoning_accumulator_is_converged(NULL));

    reasoning_accumulator_destroy(&acc);
}

/*=============================================================================
 * REGISTRY TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, ContributorTableIntegrity) {
    uint32_t count = 0;
    const reasoning_contributor_entry_t* registry =
        reasoning_convergent_get_registry(&count);

    ASSERT_NE(registry, nullptr);
    ASSERT_GT(count, 0u);

    for (uint32_t i = 0; i < count; i++) {
        /* Every entry must have a name */
        EXPECT_NE(registry[i].name, nullptr) << "Entry " << i << " has NULL name";
        EXPECT_GT(strlen(registry[i].name), 0u) << "Entry " << i << " has empty name";

        /* Every entry must have a function */
        EXPECT_NE(registry[i].fn, nullptr) << "Entry " << i << " (" << registry[i].name << ") has NULL fn";

        /* Every entry must have an availability check */
        EXPECT_NE(registry[i].is_available, nullptr) << "Entry " << i << " (" << registry[i].name << ") has NULL is_available";

        /* Wave must be 0, 1, or 2 */
        EXPECT_LE(registry[i].wave, 2u) << "Entry " << i << " (" << registry[i].name << ") has invalid wave " << registry[i].wave;

        /* Role must be valid */
        EXPECT_GE((int)registry[i].role, 0);
        EXPECT_LE((int)registry[i].role, 2);
    }
}

TEST_F(ReasoningConvergentTest, RegistryHasExpectedModules) {
    uint32_t count = 0;
    const reasoning_contributor_entry_t* registry =
        reasoning_convergent_get_registry(&count);

    /* Check for some key modules */
    bool found_hippocampus = false;
    bool found_emotional = false;
    bool found_parietal = false;
    bool found_mesh = false;

    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(registry[i].name, "hippocampus") == 0) found_hippocampus = true;
        if (strcmp(registry[i].name, "emotional") == 0) found_emotional = true;
        if (strcmp(registry[i].name, "parietal") == 0) found_parietal = true;
        if (strcmp(registry[i].name, "mesh_evidence") == 0) found_mesh = true;
    }

    EXPECT_TRUE(found_hippocampus);
    EXPECT_TRUE(found_emotional);
    EXPECT_TRUE(found_parietal);
    EXPECT_TRUE(found_mesh);
}

TEST_F(ReasoningConvergentTest, NullCountOutput) {
    /* Should not crash when count_out is NULL */
    const reasoning_contributor_entry_t* registry =
        reasoning_convergent_get_registry(NULL);
    EXPECT_NE(registry, nullptr);
}

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, DefaultConfigHasConvergent) {
    reasoning_engine_config_t config = reasoning_engine_default_config();

    EXPECT_TRUE(config.enable_convergent_reasoning);
    EXPECT_EQ(config.convergent_pool_size, 8u);
    EXPECT_EQ(config.max_convergent_contributors, 64u);
    EXPECT_FLOAT_EQ(config.convergence_ema_alpha, REASONING_DEFAULT_EMA_ALPHA);
    EXPECT_FLOAT_EQ(config.convergence_threshold, REASONING_DEFAULT_CONVERGENCE_THRESHOLD);
    EXPECT_EQ(config.convergence_timeout_ms, REASONING_DEFAULT_CONVERGENCE_TIMEOUT_MS);
}

/*=============================================================================
 * STEP TYPE TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, NewStepTypes) {
    /* Verify all new step types resolve to non-"UNKNOWN" strings */
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SEMANTIC_ACTIVATION), "SEMANTIC_ACTIVATION");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_HIPPOCAMPAL_RECALL), "HIPPOCAMPAL_RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MATHEMATICAL), "MATHEMATICAL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_INTUITIVE), "INTUITIVE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_CREATIVE_ANALOGY), "CREATIVE_ANALOGY");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SELF_KNOWLEDGE), "SELF_KNOWLEDGE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_NEURAL_LOGIC), "NEURAL_LOGIC");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MESH_CONSENSUS), "MESH_CONSENSUS");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_MODULATION), "MODULATION");
}

TEST_F(ReasoningConvergentTest, OldStepTypesStillWork) {
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_RECALL), "RECALL");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_KNOWLEDGE), "KNOWLEDGE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_INFERENCE), "INFERENCE");
    EXPECT_STREQ(reasoning_step_type_name(REASONING_STEP_SYMBOLIC_LOGIC), "SYMBOLIC_LOGIC");
}

/*=============================================================================
 * CONVERGENT ORCHESTRATOR TESTS (NULL safety)
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, ConvergentNullEngine) {
    reasoning_chain_t chain;
    int rc = reasoning_engine_reason_convergent(NULL, "test", 0, &chain);
    EXPECT_EQ(rc, -1);
}

TEST_F(ReasoningConvergentTest, ConvergentNullQuery) {
    /* Create a minimal engine to test NULL query */
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    if (!engine) {
        GTEST_SKIP() << "Failed to create engine";
        return;
    }

    reasoning_chain_t chain;
    int rc = reasoning_engine_reason_convergent(engine, NULL, 0, &chain);
    EXPECT_EQ(rc, -1);

    reasoning_engine_destroy(engine);
}

TEST_F(ReasoningConvergentTest, ConvergentNullChain) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    if (!engine) {
        GTEST_SKIP() << "Failed to create engine";
        return;
    }

    int rc = reasoning_engine_reason_convergent(engine, "test", 0, NULL);
    EXPECT_EQ(rc, -1);

    reasoning_engine_destroy(engine);
}

/*=============================================================================
 * PORTIA BRIDGE CONVERGENT TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, PortiaFullBudgetAllowsConvergent) {
    reasoning_budget_t budget = reasoning_portia_full_budget();
    EXPECT_TRUE(budget.allow_convergent_mode);
    EXPECT_EQ(budget.max_convergent_contributors, 0u); /* 0 = no override */
}

TEST_F(ReasoningConvergentTest, PortiaBudgetApplyDisablesConvergent) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);

    reasoning_budget_t budget = reasoning_portia_full_budget();
    budget.allow_convergent_mode = false;

    reasoning_portia_apply_budget(&config, &budget);
    EXPECT_FALSE(config.enable_convergent_reasoning);
}

TEST_F(ReasoningConvergentTest, PortiaBudgetApplyContributorCap) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_EQ(config.max_convergent_contributors, 64u);

    reasoning_budget_t budget = reasoning_portia_full_budget();
    budget.max_convergent_contributors = 16;

    reasoning_portia_apply_budget(&config, &budget);
    EXPECT_EQ(config.max_convergent_contributors, 16u);
}

/*=============================================================================
 * HYPO BRIDGE CONVERGENT TESTS
 *===========================================================================*/

TEST_F(ReasoningConvergentTest, HypoNeutralNoForceWave) {
    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    EXPECT_FALSE(mod.force_wave_pipeline);
}

TEST_F(ReasoningConvergentTest, HypoApplyForceWavePipeline) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    ASSERT_TRUE(config.enable_convergent_reasoning);
    ASSERT_TRUE(config.enable_concurrent_pipeline);

    reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
    mod.hypothalamus_available = true;
    mod.force_wave_pipeline = true;

    reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_FALSE(config.enable_convergent_reasoning);
    EXPECT_FALSE(config.enable_concurrent_pipeline);
}
