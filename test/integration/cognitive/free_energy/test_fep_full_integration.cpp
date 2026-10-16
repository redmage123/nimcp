/**
 * @file test_fep_full_integration.cpp
 * @brief Full integration tests for all FEP Enhancement modules working together
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_fep_consciousness.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/free_energy/nimcp_fep_sleep.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"

class FEPFullIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;
    fep_system_t* fep = nullptr;
    fep_transition_learner_t* transition_learner = nullptr;
    fep_likelihood_learner_t* likelihood_learner = nullptr;
    fep_curiosity_system_t* curiosity = nullptr;
    fep_consciousness_bridge_t* consciousness = nullptr;
    fep_neuromod_system_t* neuromod = nullptr;
    fep_planning_system_t* planning = nullptr;
    fep_evidence_system_t* evidence = nullptr;
    fep_context_system_t* context = nullptr;
    fep_sleep_system_t* sleep_sys = nullptr;
    fep_immune_bridge_t* immune_bridge = nullptr;

    void SetUp() override {
        /* Create FEP core system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create all enhancement modules */
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        transition_learner = fep_transition_learner_create(&learn_config, STATE_DIM);
        likelihood_learner = fep_likelihood_learner_create(&learn_config, OBS_DIM, STATE_DIM);

        fep_curiosity_config_t cur_config;
        fep_curiosity_default_config(&cur_config);
        curiosity = fep_curiosity_create(&cur_config);

        fep_consciousness_config_t con_config;
        fep_consciousness_default_config(&con_config);
        consciousness = fep_consciousness_create(&con_config);

        fep_neuromod_config_t neuro_config;
        fep_neuromod_default_config(&neuro_config);
        neuromod = fep_neuromod_create(&neuro_config);

        fep_planning_config_t plan_config;
        fep_planning_default_config(&plan_config);
        planning = fep_planning_create(&plan_config);

        fep_evidence_config_t ev_config;
        fep_evidence_default_config(&ev_config);
        evidence = fep_evidence_create(&ev_config);

        fep_context_config_t ctx_config;
        fep_context_default_config(&ctx_config);
        context = fep_context_create(&ctx_config);

        fep_sleep_config_t sleep_config;
        fep_sleep_default_config(&sleep_config);
        sleep_sys = fep_sleep_create(&sleep_config);

        fep_immune_config_t immune_config;
        fep_immune_bridge_default_config(&immune_config);
        immune_bridge = fep_immune_bridge_create(&immune_config);
    }

    void TearDown() override {
        if (immune_bridge) fep_immune_bridge_destroy(immune_bridge);
        if (sleep_sys) fep_sleep_destroy(sleep_sys);
        if (context) fep_context_destroy(context);
        if (evidence) fep_evidence_destroy(evidence);
        if (planning) fep_planning_destroy(planning);
        if (neuromod) fep_neuromod_destroy(neuromod);
        if (consciousness) fep_consciousness_destroy(consciousness);
        if (curiosity) fep_curiosity_destroy(curiosity);
        if (likelihood_learner) fep_likelihood_learner_destroy(likelihood_learner);
        if (transition_learner) fep_transition_learner_destroy(transition_learner);
        if (fep) fep_destroy(fep);
    }
};

/* ============================================================================
 * Full System Creation Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, AllSystemsCreated) {
    ASSERT_NE(fep, nullptr);
    ASSERT_NE(transition_learner, nullptr);
    ASSERT_NE(likelihood_learner, nullptr);
    ASSERT_NE(curiosity, nullptr);
    ASSERT_NE(consciousness, nullptr);
    ASSERT_NE(neuromod, nullptr);
    ASSERT_NE(planning, nullptr);
    ASSERT_NE(evidence, nullptr);
    ASSERT_NE(context, nullptr);
    ASSERT_NE(sleep_sys, nullptr);
    ASSERT_NE(immune_bridge, nullptr);
}

TEST_F(FEPFullIntegrationTest, AllSystemsConnectToFEP) {
    // Learning modules don't have separate connect functions - they work directly with FEP system
    EXPECT_EQ(fep_curiosity_connect(curiosity, fep), 0);
    EXPECT_EQ(fep_consciousness_connect_fep(consciousness, fep), 0);
    EXPECT_EQ(fep_planning_connect(planning, fep), 0);
    EXPECT_EQ(fep_evidence_connect(evidence, fep), 0);
    EXPECT_EQ(fep_context_connect(context, fep), 0);
    EXPECT_EQ(fep_sleep_connect(sleep_sys, fep), 0);
    EXPECT_EQ(fep_immune_bridge_connect_fep(immune_bridge, fep), 0);
}

/* ============================================================================
 * Active Inference Loop Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, ActiveInferenceLoop) {
    /* Connect all systems */
    fep_curiosity_connect(curiosity, fep);
    fep_consciousness_connect_fep(consciousness, fep);
    fep_planning_connect(planning, fep);
    fep_evidence_connect(evidence, fep);

    /* Current state */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> obs = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    /* 1. Plan actions */
    fep_plan_t plan;
    fep_plan_create(&plan, 5);
    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* 2. Evaluate curiosity for planned states */
    float curiosity_value = fep_compute_novelty(curiosity, state.data(), STATE_DIM);
    EXPECT_GE(curiosity_value, 0.0f);

    /* 3. Gate action through consciousness */
    if (plan.sequence_length > 0) {
        uint32_t gated;
        fep_consciousness_gate_action(consciousness, plan.action_sequence[0], &gated);
    }

    /* 4. Compute model evidence */
    float elbo;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);
    EXPECT_TRUE(std::isfinite(elbo));

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Neuromodulated Learning Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, NeuromodulatedLearning) {
    /* Set high dopamine (reward signal) */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.9f);

    /* Get precision from neuromod */
    float precision = fep_neuromod_compute_precision(neuromod, 1.0f);
    EXPECT_GT(precision, 0.0f);

    /* Learn with neuromodulated precision */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> next = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    int ret = fep_learn_transition(transition_learner, fep, state.data(), next.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Context-Dependent Inference Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, ContextDependentInference) {
    fep_context_connect(context, fep);
    fep_evidence_connect(evidence, fep);

    /* Create task-specific contexts */
    std::vector<float> driving_priors = {0.1f, 0.1f, 0.3f, 0.3f, 0.1f, 0.05f, 0.025f, 0.025f};
    std::vector<float> reading_priors = {0.3f, 0.3f, 0.1f, 0.1f, 0.05f, 0.05f, 0.05f, 0.05f};
    uint32_t driving_id, reading_id;

    fep_context_add(context, "driving", driving_priors.data(), STATE_DIM, &driving_id);
    fep_context_add(context, "reading", reading_priors.data(), STATE_DIM, &reading_id);

    /* Switch context and learn */
    fep_context_switch(context, fep, driving_id);

    std::vector<float> state = {0.0f, 0.0f, 0.4f, 0.4f, 0.1f, 0.05f, 0.025f, 0.025f};
    std::vector<float> next = {0.0f, 0.0f, 0.3f, 0.5f, 0.1f, 0.05f, 0.025f, 0.025f};
    std::vector<float> obs = {0.0f, 0.0f, 0.35f, 0.45f, 0.1f, 0.05f, 0.025f, 0.025f};
    fep_learn_transition(transition_learner, fep, state.data(), next.data(), STATE_DIM);

    /* Compute evidence in driving context */
    float elbo_driving;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_driving);

    /* Switch to reading and compute evidence */
    fep_context_switch(context, fep, reading_id);
    float elbo_reading;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo_reading);

    EXPECT_TRUE(std::isfinite(elbo_driving));
    EXPECT_TRUE(std::isfinite(elbo_reading));
}

/* ============================================================================
 * Sleep Consolidation Cycle Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, WakeSleepCycle) {
    fep_sleep_connect(sleep_sys, fep);
    fep_evidence_connect(evidence, fep);

    /* Wake phase: Learn and accumulate experiences */
    fep_sleep_set_stage(sleep_sys, SLEEP_STAGE_WAKE);
    std::vector<float> test_obs(OBS_DIM, 0.5f);

    for (int i = 0; i < 30; i++) {
        std::vector<float> state(STATE_DIM, 0.0f);
        std::vector<float> obs(OBS_DIM, 0.0f);
        std::vector<float> next(STATE_DIM, 0.0f);
        state[i % STATE_DIM] = 1.0f;
        next[(i + 1) % STATE_DIM] = 1.0f;

        fep_learn_transition(transition_learner, fep, state.data(), next.data(), STATE_DIM);
        fep_sleep_add_experience(sleep_sys, state.data(), obs.data(), next.data(), STATE_DIM, OBS_DIM);
    }

    /* Evidence before sleep */
    float elbo_before;
    fep_compute_elbo(evidence, fep, test_obs.data(), OBS_DIM, &elbo_before);

    /* Sleep phase: Consolidate */
    fep_sleep_set_stage(sleep_sys, SLEEP_STAGE_SWS);
    fep_sleep_replay_consolidation(sleep_sys, fep, 20);
    fep_sleep_apply_downscaling(sleep_sys, fep, 0.9f);

    fep_sleep_set_stage(sleep_sys, SLEEP_STAGE_REM);
    fep_sleep_rem_integration(sleep_sys, fep);

    /* Wake up */
    fep_sleep_set_stage(sleep_sys, SLEEP_STAGE_WAKE);

    /* Evidence after sleep */
    float elbo_after;
    fep_compute_elbo(evidence, fep, test_obs.data(), OBS_DIM, &elbo_after);

    EXPECT_TRUE(std::isfinite(elbo_before));
    EXPECT_TRUE(std::isfinite(elbo_after));
}

/* ============================================================================
 * Curiosity-Driven Exploration Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, CuriosityDrivenExploration) {
    fep_curiosity_connect(curiosity, fep);
    fep_planning_connect(planning, fep);

    /* Build up novelty memory */
    for (int i = 0; i < 5; i++) {
        std::vector<float> visited(STATE_DIM, 0.0f);
        visited[i] = 1.0f;
        fep_curiosity_record_observation(curiosity, visited.data(), STATE_DIM);
    }

    /* Novel state should have curiosity */
    std::vector<float> novel = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    float novelty = fep_compute_novelty(curiosity, novel.data(), STATE_DIM);

    EXPECT_GE(novelty, 0.0f);
}

/* ============================================================================
 * Conscious Planning Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, ConsciousPlanning) {
    fep_planning_connect(planning, fep);
    fep_consciousness_connect_fep(consciousness, fep);

    /* Plan from current state */
    std::vector<float> state = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    fep_plan_t plan;
    fep_plan_create(&plan, 5);

    fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

    /* Gate each planned action through consciousness */
    for (uint32_t i = 0; i < plan.sequence_length; i++) {
        uint32_t gated;
        int ret = fep_consciousness_gate_action(consciousness, plan.action_sequence[i], &gated);
        EXPECT_EQ(ret, 0);
    }

    fep_plan_destroy(&plan);
}

/* ============================================================================
 * Full System Update Cycle Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, FullSystemUpdateCycle) {
    /* Connect all systems */
    fep_curiosity_connect(curiosity, fep);
    fep_consciousness_connect_fep(consciousness, fep);
    fep_planning_connect(planning, fep);
    fep_evidence_connect(evidence, fep);
    fep_context_connect(context, fep);
    fep_sleep_connect(sleep_sys, fep);
    fep_immune_bridge_connect_fep(immune_bridge, fep);

    /* Create a context */
    std::vector<float> priors = {0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f};
    uint32_t ctx_id;
    fep_context_add(context, "default", priors.data(), STATE_DIM, &ctx_id);
    fep_context_switch(context, fep, ctx_id);

    /* Update systems that have update functions */
    fep_neuromod_update(neuromod, 100);
    fep_consciousness_update(consciousness, 100);
    fep_sleep_update(sleep_sys, 100);
    fep_immune_bridge_update(immune_bridge, 100);

    /* All updates should succeed */
    SUCCEED();
}

/* ============================================================================
 * All Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, AllBioAsyncConnections) {
    /* Connect bio-async for all systems */
    fep_transition_learner_connect_bio_async(transition_learner);
    fep_likelihood_learner_connect_bio_async(likelihood_learner);
    fep_curiosity_connect_bio_async(curiosity);
    fep_consciousness_connect_bio_async(consciousness);
    fep_neuromod_connect_bio_async(neuromod);
    fep_planning_connect_bio_async(planning);
    fep_evidence_connect_bio_async(evidence);
    fep_context_connect_bio_async(context);
    fep_sleep_connect_bio_async(sleep_sys);
    fep_immune_bridge_connect_bio_async(immune_bridge);

    /* Disconnect all */
    fep_transition_learner_disconnect_bio_async(transition_learner);
    fep_likelihood_learner_disconnect_bio_async(likelihood_learner);
    fep_curiosity_disconnect_bio_async(curiosity);
    fep_consciousness_disconnect_bio_async(consciousness);
    fep_neuromod_disconnect_bio_async(neuromod);
    fep_planning_disconnect_bio_async(planning);
    fep_evidence_disconnect_bio_async(evidence);
    fep_context_disconnect_bio_async(context);
    fep_sleep_disconnect_bio_async(sleep_sys);
    fep_immune_bridge_disconnect_bio_async(immune_bridge);
}

/* ============================================================================
 * Stats Collection Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, CollectAllStats) {
    /* Connect and run some operations */
    fep_evidence_connect(evidence, fep);
    fep_sleep_connect(sleep_sys, fep);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> next = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 10; i++) {
        fep_learn_transition(transition_learner, fep, state.data(), next.data(), STATE_DIM);
        float elbo;
        fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo);
        fep_sleep_add_experience(sleep_sys, state.data(), obs.data(), next.data(), STATE_DIM, OBS_DIM);
    }

    /* Collect stats from all systems */
    fep_learning_stats_t learn_stats;
    fep_transition_learning_get_stats(transition_learner, &learn_stats);
    EXPECT_GT(learn_stats.total_updates, 0u);

    fep_evidence_stats_t ev_stats;
    fep_evidence_get_stats(evidence, &ev_stats);
    EXPECT_GT(ev_stats.elbo_computations, 0u);

    fep_curiosity_stats_t cur_stats;
    fep_curiosity_get_stats(curiosity, &cur_stats);

    fep_planning_stats_t plan_stats;
    fep_planning_get_stats(planning, &plan_stats);

    fep_sleep_stats_t sleep_stats;
    fep_sleep_get_stats(sleep_sys, &sleep_stats);

    fep_immune_stats_t immune_stats;
    fep_immune_bridge_get_stats(immune_bridge, &immune_stats);
}

/* ============================================================================
 * End-to-End Cognitive Loop Tests
 * ============================================================================ */

TEST_F(FEPFullIntegrationTest, EndToEndCognitiveLoop) {
    /* This test simulates a full cognitive processing loop */

    /* Connect all systems */
    fep_curiosity_connect(curiosity, fep);
    fep_consciousness_connect_fep(consciousness, fep);
    fep_planning_connect(planning, fep);
    fep_evidence_connect(evidence, fep);
    fep_context_connect(context, fep);
    fep_sleep_connect(sleep_sys, fep);

    /* Create context */
    std::vector<float> ctx_priors = {0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f};
    uint32_t ctx_id;
    fep_context_add(context, "task", ctx_priors.data(), STATE_DIM, &ctx_id);
    fep_context_switch(context, fep, ctx_id);

    /* Set neuromodulator state */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.7f);

    /* Cognitive loop iterations */
    for (int iter = 0; iter < 5; iter++) {
        std::vector<float> state(STATE_DIM, 0.0f);
        state[iter % STATE_DIM] = 1.0f;

        /* 1. Compute curiosity (novelty and information gain) */
        float novelty = fep_compute_novelty(curiosity, state.data(), STATE_DIM);
        float info_gain = fep_compute_information_gain(curiosity, fep, state.data(), STATE_DIM);
        (void)novelty;  /* Used for exploration */
        (void)info_gain;

        /* 2. Plan actions */
        fep_plan_t plan;
        fep_plan_create(&plan, 3);
        fep_planning_generate_plan(planning, fep, state.data(), STATE_DIM, &plan);

        /* 3. Gate through consciousness */
        if (plan.sequence_length > 0) {
            uint32_t gated;
            fep_consciousness_gate_action(consciousness, plan.action_sequence[0], &gated);
        }

        /* 4. Learn from experience */
        std::vector<float> next(STATE_DIM, 0.0f);
        next[(iter + 1) % STATE_DIM] = 1.0f;
        fep_learn_transition(transition_learner, fep, state.data(), next.data(), STATE_DIM);

        /* 5. Accumulate for sleep */
        std::vector<float> obs(OBS_DIM, 0.0f);
        fep_sleep_add_experience(sleep_sys, state.data(), obs.data(), next.data(), STATE_DIM, OBS_DIM);

        /* 6. Update neuromodulators */
        fep_neuromod_update(neuromod, 100);

        /* 7. Add to curiosity memory */
        fep_curiosity_record_observation(curiosity, state.data(), STATE_DIM);

        fep_plan_destroy(&plan);
    }

    /* Compute final evidence */
    std::vector<float> final_obs(OBS_DIM, 0.5f);
    float final_elbo;
    fep_compute_elbo(evidence, fep, final_obs.data(), OBS_DIM, &final_elbo);
    EXPECT_TRUE(std::isfinite(final_elbo));

    /* Sleep consolidation */
    fep_sleep_set_stage(sleep_sys, SLEEP_STAGE_SWS);
    fep_sleep_replay_consolidation(sleep_sys, fep, 10);

    /* Final state check */
    fep_context_state_t ctx_state;
    fep_context_get_state(context, &ctx_state);
    EXPECT_EQ(ctx_state.active_context_id, ctx_id);
}
