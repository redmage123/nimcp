/**
 * @file test_fep_learning_integration.cpp
 * @brief Integration tests for FEP Learning module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"

class FEPLearningIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;

    fep_transition_learner_t* learner = nullptr;
    fep_system_t* fep = nullptr;
    fep_neuromod_system_t* neuromod = nullptr;

    void SetUp() override {
        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);

        // Create transition learner
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learner = fep_transition_learner_create(&learn_config, STATE_DIM);

        // Create neuromodulation system
        fep_neuromod_config_t neuro_config;
        fep_neuromod_default_config(&neuro_config);
        neuromod = fep_neuromod_create(&neuro_config);
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
        if (neuromod) {
            fep_neuromod_destroy(neuromod);
            neuromod = nullptr;
        }
    }
};

/* ============================================================================
 * Learning + FEP Core Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, LearningWithFEPSystem) {
    ASSERT_NE(learner, nullptr);
    ASSERT_NE(fep, nullptr);

    // Learn a transition
    std::vector<float> state_t(STATE_DIM, 0.5f);
    std::vector<float> state_t1(STATE_DIM, 0.6f);

    int ret = fep_learn_transition(learner, fep, state_t.data(), state_t1.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPLearningIntegrationTest, MultipleTransitions) {
    ASSERT_NE(learner, nullptr);

    // Learn multiple transitions
    for (int i = 0; i < 10; i++) {
        std::vector<float> state_t(STATE_DIM, 0.1f * i);
        std::vector<float> state_t1(STATE_DIM, 0.1f * (i + 1));

        int ret = fep_learn_transition(learner, fep, state_t.data(), state_t1.data(), STATE_DIM);
        EXPECT_EQ(ret, 0);
    }

    // Check stats
    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Learning + Neuromodulation Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, LearningWithNeuromodulation) {
    ASSERT_NE(learner, nullptr);
    ASSERT_NE(neuromod, nullptr);

    // Set high dopamine (should enhance learning)
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.8f);

    // Learn transitions
    std::vector<float> state_t(STATE_DIM, 0.3f);
    std::vector<float> state_t1(STATE_DIM, 0.7f);

    int ret = fep_learn_transition(learner, fep, state_t.data(), state_t1.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPLearningIntegrationTest, LearningRateModulationByDA) {
    ASSERT_NE(neuromod, nullptr);

    // Low dopamine
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.2f);
    float low_level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    // High dopamine
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.9f);
    float high_level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    // Verify DA levels are set correctly
    EXPECT_LT(low_level, 0.5f);
    EXPECT_GT(high_level, 0.5f);
}

/* ============================================================================
 * Batch Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, BatchLearning) {
    ASSERT_NE(learner, nullptr);

    // Create batch of states
    const int batch_size = 32;
    std::vector<float> states(batch_size * STATE_DIM);

    for (int i = 0; i < batch_size * STATE_DIM; i++) {
        states[i] = static_cast<float>(i) / (batch_size * STATE_DIM);
    }

    int ret = fep_learn_transition_batch(learner, fep, states.data(), batch_size, STATE_DIM);
    EXPECT_EQ(ret, 0);

    // Check stats
    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Stats and Reset Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, StatsTracking) {
    ASSERT_NE(learner, nullptr);

    // Learn some transitions
    for (int i = 0; i < 5; i++) {
        std::vector<float> state_t(STATE_DIM, 0.1f * i);
        std::vector<float> state_t1(STATE_DIM, 0.1f * (i + 1));
        fep_learn_transition(learner, fep, state_t.data(), state_t1.data(), STATE_DIM);
    }

    fep_learning_stats_t stats;
    int ret = fep_transition_learning_get_stats(learner, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.total_updates, 0u);
}

TEST_F(FEPLearningIntegrationTest, ResetStats) {
    ASSERT_NE(learner, nullptr);

    // Learn some transitions
    std::vector<float> state_t(STATE_DIM, 0.5f);
    std::vector<float> state_t1(STATE_DIM, 0.6f);
    fep_learn_transition(learner, fep, state_t.data(), state_t1.data(), STATE_DIM);

    // Reset stats
    int ret = fep_learning_reset_stats(learner);
    EXPECT_EQ(ret, 0);

    // Verify reset
    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, BioAsyncConnection) {
    ASSERT_NE(learner, nullptr);

    int ret = fep_transition_learner_connect_bio_async(learner);
    EXPECT_EQ(ret, 0);

    ret = fep_transition_learner_disconnect_bio_async(learner);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Likelihood Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, LikelihoodLearning) {
    // Create likelihood learner
    fep_learning_config_t config;
    fep_learning_default_config(&config);
    fep_likelihood_learner_t* likelihood_learner = fep_likelihood_learner_create(&config, OBS_DIM, STATE_DIM);
    ASSERT_NE(likelihood_learner, nullptr);

    // Learn likelihood mapping
    std::vector<float> obs(OBS_DIM, 0.5f);
    std::vector<float> state(STATE_DIM, 0.5f);

    int ret = fep_learn_likelihood(likelihood_learner, fep, obs.data(), state.data(), OBS_DIM, STATE_DIM);
    EXPECT_EQ(ret, 0);

    fep_likelihood_learner_destroy(likelihood_learner);
}

/* ============================================================================
 * Error Handling Integration Tests
 * ============================================================================ */

TEST_F(FEPLearningIntegrationTest, NullHandling) {
    std::vector<float> state(STATE_DIM, 0.5f);

    // These should handle null gracefully
    EXPECT_NE(fep_learn_transition(nullptr, fep, state.data(), state.data(), STATE_DIM), 0);
    EXPECT_NE(fep_learn_transition(learner, fep, nullptr, state.data(), STATE_DIM), 0);
}
