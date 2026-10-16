/**
 * @file test_fep_cognitive_bridges_integration.cpp
 * @brief Integration tests for FEP system core functionality
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests the core FEP system and its existing integrations:
 * - FEP system lifecycle and configuration
 * - FEP-immune bridge integration
 * - Belief updates and precision weighting
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * FEP Core Integration Test Fixture
 * ============================================================================ */

class FEPCoreIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;

    fep_system_t* fep = nullptr;
    fep_immune_bridge_t* immune_bridge = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        /* Create FEP-immune bridge */
        fep_immune_config_t bridge_config;
        fep_immune_bridge_default_config(&bridge_config);
        immune_bridge = fep_immune_bridge_create(&bridge_config);
        if (immune_bridge) {
            fep_immune_bridge_connect_fep(immune_bridge, fep);
        }
    }

    void TearDown() override {
        if (immune_bridge) {
            fep_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * FEP System Core Tests
 * ============================================================================ */

TEST_F(FEPCoreIntegrationTest, FEPSystemCreation) {
    EXPECT_NE(fep, nullptr);
}

TEST_F(FEPCoreIntegrationTest, FEPDefaultConfig) {
    fep_config_t config;
    int ret = fep_default_config(&config);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, FEPProcessObservation) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    int ret = fep_process_observation(fep, obs, OBS_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, FEPUpdateBeliefs) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 0.5f;
    }
    fep_process_observation(fep, obs, OBS_DIM);

    int ret = fep_update_beliefs(fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, FEPUpdatePrecision) {
    int ret = fep_update_precision(fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, FEPGetFreeEnergy) {
    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPCoreIntegrationTest, FEPGetPredictionError) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 1.0f;
    }
    fep_process_observation(fep, obs, OBS_DIM);

    float pe = fep_get_prediction_error(fep, 0);
    EXPECT_GE(pe, 0.0f);
}

TEST_F(FEPCoreIntegrationTest, FEPEvaluatePolicies) {
    int ret = fep_evaluate_policies(fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, FEPSelectAction) {
    float action[ACTION_DIM];
    int policy_idx = fep_select_action(fep, action, ACTION_DIM);
    EXPECT_GE(policy_idx, -1);
}

TEST_F(FEPCoreIntegrationTest, FEPGetStats) {
    fep_stats_t stats;
    int ret = fep_get_stats(fep, &stats);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * FEP-Immune Bridge Tests
 * ============================================================================ */

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeCreation) {
    EXPECT_NE(immune_bridge, nullptr);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeFEPConnection) {
    ASSERT_NE(immune_bridge, nullptr);
    EXPECT_EQ(immune_bridge->fep_system, fep);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeUpdate) {
    ASSERT_NE(immune_bridge, nullptr);
    int ret = fep_immune_bridge_update(immune_bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeGetState) {
    ASSERT_NE(immune_bridge, nullptr);
    fep_immune_state_t state;
    int ret = fep_immune_bridge_get_state(immune_bridge, &state);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeGetStats) {
    ASSERT_NE(immune_bridge, nullptr);
    fep_immune_stats_t stats;
    int ret = fep_immune_bridge_get_stats(immune_bridge, &stats);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeGetPrecisionModifier) {
    ASSERT_NE(immune_bridge, nullptr);
    float precision;
    int ret = fep_immune_get_precision_modifier(immune_bridge, &precision);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(precision, 0.0f);
    EXPECT_LE(precision, 1.0f);
}

TEST_F(FEPCoreIntegrationTest, ImmuneBridgeGetLearningModifier) {
    ASSERT_NE(immune_bridge, nullptr);
    float learning;
    int ret = fep_immune_get_learning_modifier(immune_bridge, &learning);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(learning, 0.0f);
    EXPECT_LE(learning, 1.0f);
}

/* ============================================================================
 * FEP Processing Pipeline Tests
 * ============================================================================ */

TEST_F(FEPCoreIntegrationTest, FullProcessingCycle) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i % 3 == 0) ? 1.0f : 0.0f;
    }

    /* Full processing cycle */
    EXPECT_EQ(fep_process_observation(fep, obs, OBS_DIM), 0);
    EXPECT_EQ(fep_update_beliefs(fep), 0);
    EXPECT_EQ(fep_update_precision(fep), 0);
    EXPECT_EQ(fep_evaluate_policies(fep), 0);

    float action[ACTION_DIM];
    int policy_idx = fep_select_action(fep, action, ACTION_DIM);
    EXPECT_GE(policy_idx, -1);
}

TEST_F(FEPCoreIntegrationTest, MultipleObservationCycles) {
    for (int cycle = 0; cycle < 10; cycle++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf((cycle * OBS_DIM + i) * 0.1f);
        }

        EXPECT_EQ(fep_process_observation(fep, obs, OBS_DIM), 0);
        EXPECT_EQ(fep_update_beliefs(fep), 0);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
}

TEST_F(FEPCoreIntegrationTest, FEPWithImmuneBridgeProcessing) {
    ASSERT_NE(immune_bridge, nullptr);

    for (int cycle = 0; cycle < 5; cycle++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = (i == (uint32_t)(cycle % OBS_DIM)) ? 1.0f : 0.0f;
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_immune_bridge_update(immune_bridge, 100);
        fep_update_beliefs(fep);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GT(stats.belief_updates, 0u);
}

/* ============================================================================
 * FEP Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPCoreIntegrationTest, TransitionLearnerCreation) {
    fep_learning_config_t config;
    fep_learning_default_config(&config);

    fep_transition_learner_t* learner = fep_transition_learner_create(&config, OBS_DIM);
    EXPECT_NE(learner, nullptr);

    if (learner) {
        fep_transition_learner_destroy(learner);
    }
}

TEST_F(FEPCoreIntegrationTest, LearnTransition) {
    fep_learning_config_t config;
    fep_learning_default_config(&config);

    fep_transition_learner_t* learner = fep_transition_learner_create(&config, OBS_DIM);
    ASSERT_NE(learner, nullptr);

    float state[OBS_DIM], next[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        state[i] = (i % 2 == 0) ? 1.0f : 0.0f;
        next[i] = (i % 2 == 1) ? 1.0f : 0.0f;
    }

    int ret = fep_learn_transition(learner, fep, state, next, OBS_DIM);
    EXPECT_EQ(ret, 0);

    fep_transition_learner_destroy(learner);
}

TEST_F(FEPCoreIntegrationTest, MultiStepTransitionLearning) {
    fep_learning_config_t config;
    fep_learning_default_config(&config);

    fep_transition_learner_t* learner = fep_transition_learner_create(&config, OBS_DIM);
    ASSERT_NE(learner, nullptr);

    for (int i = 0; i < 5; i++) {
        float state[OBS_DIM], next[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            state[j] = (j == (uint32_t)i % OBS_DIM) ? 1.0f : 0.0f;
            next[j] = (j == (uint32_t)(i + 1) % OBS_DIM) ? 1.0f : 0.0f;
        }
        fep_learn_transition(learner, fep, state, next, OBS_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 5u);

    fep_transition_learner_destroy(learner);
}

