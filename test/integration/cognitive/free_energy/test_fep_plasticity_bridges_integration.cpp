/**
 * @file test_fep_plasticity_bridges_integration.cpp
 * @brief Integration tests for FEP with plasticity-related processing
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests FEP system with plasticity-related learning scenarios:
 * - STDP-like temporal learning patterns
 * - Homeostatic-like stability maintenance
 * - Learning rate modulation
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * FEP Plasticity Integration Test Fixture
 * ============================================================================ */

class FEPPlasticityIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 16;

    fep_system_t* fep = nullptr;
    fep_transition_learner_t* learner = nullptr;

    void SetUp() override {
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learner = fep_transition_learner_create(&learn_config, STATE_DIM);
    }

    void TearDown() override {
        if (learner) {
            fep_transition_learner_destroy(learner);
            learner = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * STDP-Like Temporal Learning Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, TemporalCausalLearning) {
    ASSERT_NE(learner, nullptr);

    /* Pre-synaptic followed by post-synaptic (causal) */
    float pre[STATE_DIM], post[STATE_DIM];
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        pre[i] = (i < STATE_DIM/2) ? 1.0f : 0.0f;
        post[i] = (i >= STATE_DIM/2) ? 1.0f : 0.0f;
    }

    int ret = fep_learn_transition(learner, fep, pre, post, STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPlasticityIntegrationTest, TemporalAntiCausalLearning) {
    ASSERT_NE(learner, nullptr);

    /* Post before pre (anti-causal) */
    float post[STATE_DIM], pre[STATE_DIM];
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        post[i] = (i >= STATE_DIM/2) ? 1.0f : 0.0f;
        pre[i] = (i < STATE_DIM/2) ? 1.0f : 0.0f;
    }

    int ret = fep_learn_transition(learner, fep, post, pre, STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPlasticityIntegrationTest, TemporalSequenceLearning) {
    ASSERT_NE(learner, nullptr);

    /* Learn sequence of states */
    for (int step = 0; step < 5; step++) {
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            state[i] = (i == (uint32_t)step % STATE_DIM) ? 1.0f : 0.0f;
            next[i] = (i == (uint32_t)(step + 1) % STATE_DIM) ? 1.0f : 0.0f;
        }

        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * Homeostatic-Like Stability Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, StabilityUnderConstantInput) {
    /* Process constant input, should stabilize */
    float const_obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        const_obs[i] = 0.5f;
    }

    for (int t = 0; t < 10; t++) {
        fep_process_observation(fep, const_obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPPlasticityIntegrationTest, StabilityUnderOscillatingInput) {
    /* Oscillating input, should maintain stability */
    for (int t = 0; t < 20; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf(t * 0.5f + i * 0.1f) * 0.5f + 0.5f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPPlasticityIntegrationTest, RecoveryFromPerturbation) {
    /* Establish baseline */
    float baseline[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        baseline[i] = 0.5f;
    }

    for (int t = 0; t < 5; t++) {
        fep_process_observation(fep, baseline, OBS_DIM);
        fep_update_beliefs(fep);
    }

    /* Perturbation */
    float perturb[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        perturb[i] = 1.0f;
    }
    fep_process_observation(fep, perturb, OBS_DIM);

    /* Recovery */
    for (int t = 0; t < 5; t++) {
        fep_process_observation(fep, baseline, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

/* ============================================================================
 * Learning Rate Modulation Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, BasicTransitionLearning) {
    ASSERT_NE(learner, nullptr);

    float state[STATE_DIM], next[STATE_DIM];
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        state[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        next[i] = (i % 2 == 1) ? 1.0f : 0.0f;
    }

    int ret = fep_learn_transition(learner, fep, state, next, STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPPlasticityIntegrationTest, RepeatedPatternLearning) {
    ASSERT_NE(learner, nullptr);

    /* Learn same transition multiple times */
    float state[STATE_DIM], next[STATE_DIM];
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        state[i] = (i < STATE_DIM/2) ? 1.0f : 0.0f;
        next[i] = (i >= STATE_DIM/2) ? 1.0f : 0.0f;
    }

    for (int rep = 0; rep < 5; rep++) {
        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 5u);
}

TEST_F(FEPPlasticityIntegrationTest, NovelPatternLearning) {
    ASSERT_NE(learner, nullptr);

    /* Learn different patterns */
    for (int p = 0; p < 4; p++) {
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            state[i] = (i % 4 == (uint32_t)p) ? 1.0f : 0.0f;
            next[i] = (i % 4 == (uint32_t)((p + 1) % 4)) ? 1.0f : 0.0f;
        }

        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 4u);
}

/* ============================================================================
 * Eligibility Trace-Like Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, DelayedRewardLearning) {
    ASSERT_NE(learner, nullptr);

    /* Sequence of states leading to reward */
    float states[3][STATE_DIM];
    for (int s = 0; s < 3; s++) {
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            states[s][i] = (i == (uint32_t)s) ? 1.0f : 0.0f;
        }
    }

    /* Learn transitions */
    fep_learn_transition(learner, fep, states[0], states[1], STATE_DIM);
    fep_learn_transition(learner, fep, states[1], states[2], STATE_DIM);

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 2u);
}

TEST_F(FEPPlasticityIntegrationTest, TemporalCreditAssignment) {
    ASSERT_NE(learner, nullptr);

    /* Chain of transitions */
    for (int step = 0; step < 4; step++) {
        float current[STATE_DIM], next[STATE_DIM];
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            current[i] = (i == (uint32_t)step) ? 1.0f : 0.0f;
            next[i] = (i == (uint32_t)(step + 1)) ? 1.0f : 0.0f;
        }

        fep_learn_transition(learner, fep, current, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 4u);
}

/* ============================================================================
 * BCM-Like Sliding Threshold Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, ActivityDependentPlasticity) {
    /* High activity input */
    float high_activity[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        high_activity[i] = 0.9f;
    }

    for (int t = 0; t < 5; t++) {
        fep_process_observation(fep, high_activity, OBS_DIM);
        fep_update_beliefs(fep);
    }

    /* Low activity input */
    float low_activity[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        low_activity[i] = 0.1f;
    }

    for (int t = 0; t < 5; t++) {
        fep_process_observation(fep, low_activity, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPPlasticityIntegrationTest, BimodalActivityProcessing) {
    /* Alternating high/low activity */
    for (int t = 0; t < 10; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = (t % 2 == 0) ? 0.9f : 0.1f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GE(stats.belief_updates, 10u);
}

/* ============================================================================
 * Long-Term Plasticity Tests
 * ============================================================================ */

TEST_F(FEPPlasticityIntegrationTest, ExtendedLearningSession) {
    ASSERT_NE(learner, nullptr);

    for (int epoch = 0; epoch < 10; epoch++) {
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            state[i] = sinf(epoch * 0.3f + i * 0.1f) * 0.5f + 0.5f;
            next[i] = sinf((epoch + 1) * 0.3f + i * 0.1f) * 0.5f + 0.5f;
        }

        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 10u);
}

TEST_F(FEPPlasticityIntegrationTest, StableAfterLearning) {
    ASSERT_NE(learner, nullptr);

    /* Learn patterns */
    for (int i = 0; i < 5; i++) {
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t j = 0; j < STATE_DIM; j++) {
            state[j] = (j == (uint32_t)i) ? 1.0f : 0.0f;
            next[j] = (j == (uint32_t)(i + 1) % STATE_DIM) ? 1.0f : 0.0f;
        }
        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    /* Process observations after learning */
    for (int t = 0; t < 5; t++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = (i == (uint32_t)t) ? 1.0f : 0.0f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

