/**
 * @file test_fep_context_integration.cpp
 * @brief Integration tests for FEP Context module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_evidence.h"

class FEPContextIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;
    fep_context_system_t* ctx = nullptr;
    fep_system_t* fep = nullptr;
    fep_transition_learner_t* learning = nullptr;
    fep_evidence_system_t* evidence = nullptr;

    void SetUp() override {
        /* Create context system */
        fep_context_config_t ctx_config;
        fep_context_default_config(&ctx_config);
        ctx = fep_context_create(&ctx_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        /* Create learning system */
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learning = fep_transition_learner_create(&learn_config, STATE_DIM);

        /* Create evidence system */
        fep_evidence_config_t ev_config;
        fep_evidence_default_config(&ev_config);
        evidence = fep_evidence_create(&ev_config);
    }

    void TearDown() override {
        if (ctx) {
            fep_context_destroy(ctx);
            ctx = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (learning) {
            fep_transition_learner_destroy(learning);
            learning = nullptr;
        }
        if (evidence) {
            fep_evidence_destroy(evidence);
            evidence = nullptr;
        }
    }
};

/* ============================================================================
 * Context + FEP Core Integration Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, ContextWithFEPSystem) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t context_id;
    int ret = fep_context_add(ctx, "test_context", priors.data(), STATE_DIM, &context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextIntegrationTest, ContextSwitchWithFEP) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> priors2 = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    fep_context_add(ctx, "context1", priors1.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "context2", priors2.data(), STATE_DIM, &id2);

    int ret1 = fep_context_switch(ctx, fep, id1);
    int ret2 = fep_context_switch(ctx, fep, id2);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
}

/* ============================================================================
 * Context Inference Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, InferContextFromObservation) {
    fep_context_connect(ctx, fep);

    /* Create distinct contexts */
    std::vector<float> priors1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> priors2 = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    fep_context_add(ctx, "context_a", priors1.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "context_b", priors2.data(), STATE_DIM, &id2);

    /* Infer context from observation */
    std::vector<float> observation = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t inferred_id;
    float confidence;

    int ret = fep_context_infer(ctx, fep, observation.data(), STATE_DIM, &inferred_id, &confidence);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(FEPContextIntegrationTest, AutoSwitchFromObservation) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t context_id;
    fep_context_add(ctx, "auto_ctx", priors.data(), STATE_DIM, &context_id);

    std::vector<float> observation = {0.6f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int ret = fep_context_auto_switch(ctx, fep, observation.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Context + Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, LearnContextFromExperience) {
    fep_context_connect(ctx, fep);

    /* Create context */
    std::vector<float> priors(STATE_DIM, 0.0f);
    uint32_t context_id;
    fep_context_add(ctx, "learn_ctx", priors.data(), STATE_DIM, &context_id);

    /* Train FEP on data */
    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> next = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 10; i++) {
        fep_learn_transition(learning, fep, state.data(), next.data(), STATE_DIM);
    }

    /* Learn context from experience */
    int ret = fep_context_learn_from_experience(ctx, fep, context_id);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextIntegrationTest, CreateContextFromCurrent) {
    fep_context_connect(ctx, fep);

    /* Train FEP to establish current beliefs */
    std::vector<float> state = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> next = {0.6f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 10; i++) {
        fep_learn_transition(learning, fep, state.data(), next.data(), STATE_DIM);
    }

    /* Create new context from current state */
    uint32_t new_id;
    int ret = fep_context_create_from_current(ctx, fep, "new_context", &new_id);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Context + Evidence Integration Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, EvidencePerContext) {
    fep_context_connect(ctx, fep);
    fep_evidence_connect(evidence, fep);

    /* Create contexts */
    std::vector<float> priors1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> priors2 = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> obs = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    fep_context_add(ctx, "ctx1", priors1.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "ctx2", priors2.data(), STATE_DIM, &id2);

    /* Apply context 1 */
    fep_context_apply(ctx, fep, id1);
    float elbo1;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo1);

    /* Apply context 2 */
    fep_context_apply(ctx, fep, id2);
    float elbo2;
    fep_compute_elbo(evidence, fep, obs.data(), OBS_DIM, &elbo2);

    EXPECT_TRUE(std::isfinite(elbo1));
    EXPECT_TRUE(std::isfinite(elbo2));
}

/* ============================================================================
 * Context Blending Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, BlendContextsWithFEP) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> priors2 = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    fep_context_add(ctx, "blend_a", priors1.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "blend_b", priors2.data(), STATE_DIM, &id2);

    /* Blend 50/50 */
    int ret = fep_context_blend(ctx, fep, id1, id2, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPContextIntegrationTest, GradualContextTransition) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors1 = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> priors2 = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id1, id2;

    fep_context_add(ctx, "from", priors1.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "to", priors2.data(), STATE_DIM, &id2);

    /* Gradual transition */
    for (float alpha = 0.0f; alpha <= 1.0f; alpha += 0.1f) {
        int ret = fep_context_blend(ctx, fep, id1, id2, alpha);
        EXPECT_EQ(ret, 0);
    }
}

/* ============================================================================
 * Switching Mode Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, HardSwitchWithFEP) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_HARD;

    fep_context_system_t* hard = fep_context_create(&config);
    fep_context_connect(hard, fep);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id;
    fep_context_add(hard, "hard_ctx", priors.data(), STATE_DIM, &id);

    int ret = fep_context_switch(hard, fep, id);
    EXPECT_EQ(ret, 0);

    fep_context_destroy(hard);
}

TEST_F(FEPContextIntegrationTest, SoftSwitchWithFEP) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_SOFT;
    config.interpolation_rate = 0.9f;

    fep_context_system_t* soft = fep_context_create(&config);
    fep_context_connect(soft, fep);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id;
    fep_context_add(soft, "soft_ctx", priors.data(), STATE_DIM, &id);

    int ret = fep_context_switch(soft, fep, id);
    EXPECT_EQ(ret, 0);

    fep_context_destroy(soft);
}

TEST_F(FEPContextIntegrationTest, GatedSwitchWithFEP) {
    fep_context_config_t config;
    fep_context_default_config(&config);
    config.switch_mode = CONTEXT_SWITCH_GATED;
    config.switch_threshold = 0.7f;

    fep_context_system_t* gated = fep_context_create(&config);
    fep_context_connect(gated, fep);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id;
    fep_context_add(gated, "gated_ctx", priors.data(), STATE_DIM, &id);

    std::vector<float> observation = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    int ret = fep_context_auto_switch(gated, fep, observation.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);

    fep_context_destroy(gated);
}

/* ============================================================================
 * State Tracking Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, StateAfterOperations) {
    fep_context_connect(ctx, fep);

    std::vector<float> priors(STATE_DIM, 0.0f);
    uint32_t id1, id2, id3;
    fep_context_add(ctx, "state_a", priors.data(), STATE_DIM, &id1);
    fep_context_add(ctx, "state_b", priors.data(), STATE_DIM, &id2);
    fep_context_add(ctx, "state_c", priors.data(), STATE_DIM, &id3);

    fep_context_switch(ctx, fep, id2);

    fep_context_state_t state;
    fep_context_get_state(ctx, &state);

    EXPECT_EQ(state.num_contexts, 3u);
    EXPECT_EQ(state.active_context_id, id2);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, BioAsyncWithContext) {
    fep_context_connect(ctx, fep);
    fep_context_connect_bio_async(ctx);

    std::vector<float> priors = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t id;
    fep_context_add(ctx, "bio_ctx", priors.data(), STATE_DIM, &id);

    int ret = fep_context_switch(ctx, fep, id);
    EXPECT_EQ(ret, 0);

    fep_context_disconnect_bio_async(ctx);
}

/* ============================================================================
 * Full Context Integration Tests
 * ============================================================================ */

TEST_F(FEPContextIntegrationTest, FullContextWorkflow) {
    fep_context_connect(ctx, fep);

    /* 1. Create multiple contexts */
    std::vector<float> driving_priors = {0.1f, 0.1f, 0.2f, 0.3f, 0.1f, 0.1f, 0.05f, 0.05f};
    std::vector<float> reading_priors = {0.3f, 0.3f, 0.1f, 0.1f, 0.05f, 0.05f, 0.05f, 0.05f};
    uint32_t driving_id, reading_id;

    fep_context_add(ctx, "driving", driving_priors.data(), STATE_DIM, &driving_id);
    fep_context_add(ctx, "reading", reading_priors.data(), STATE_DIM, &reading_id);

    /* 2. Switch to driving context */
    fep_context_switch(ctx, fep, driving_id);

    /* 3. Learn in driving context */
    std::vector<float> state = {0.0f, 0.0f, 0.3f, 0.4f, 0.1f, 0.1f, 0.05f, 0.05f};
    std::vector<float> next = {0.0f, 0.0f, 0.2f, 0.5f, 0.1f, 0.1f, 0.05f, 0.05f};
    fep_learn_transition(learning, fep, state.data(), next.data(), STATE_DIM);

    /* 4. Update context from experience */
    fep_context_learn_from_experience(ctx, fep, driving_id);

    /* 5. Infer context from new observation */
    std::vector<float> observation = {0.3f, 0.3f, 0.1f, 0.1f, 0.1f, 0.05f, 0.025f, 0.025f};
    uint32_t inferred_id;
    float confidence;
    fep_context_infer(ctx, fep, observation.data(), STATE_DIM, &inferred_id, &confidence);

    /* 6. Get final state */
    fep_context_state_t final_state;
    fep_context_get_state(ctx, &final_state);

    EXPECT_EQ(final_state.num_contexts, 2u);
    EXPECT_GE(confidence, 0.0f);
}
