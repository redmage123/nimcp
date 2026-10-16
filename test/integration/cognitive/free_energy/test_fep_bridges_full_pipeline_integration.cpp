/**
 * @file test_fep_bridges_full_pipeline_integration.cpp
 * @brief End-to-end integration tests for complete FEP pipeline
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests complete FEP pipeline functionality:
 * - Observation -> Belief Update -> Policy Evaluation -> Action Selection
 * - FEP-Immune bridge integration
 * - Learning and adaptation scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Full Pipeline Test Fixture
 * ============================================================================ */

class FEPFullPipelineTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;
    static const uint32_t STATE_DIM = 16;

    fep_system_t* fep = nullptr;
    fep_immune_bridge_t* immune_bridge = nullptr;
    fep_transition_learner_t* learner = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.enable_active_inference = true;
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        /* Create FEP-immune bridge */
        fep_immune_config_t bridge_config;
        fep_immune_bridge_default_config(&bridge_config);
        immune_bridge = fep_immune_bridge_create(&bridge_config);
        if (immune_bridge) {
            fep_immune_bridge_connect_fep(immune_bridge, fep);
        }

        /* Create learner */
        fep_learning_config_t learn_config;
        fep_learning_default_config(&learn_config);
        learner = fep_transition_learner_create(&learn_config, STATE_DIM);
    }

    void TearDown() override {
        if (learner) {
            fep_transition_learner_destroy(learner);
            learner = nullptr;
        }
        if (immune_bridge) {
            fep_immune_bridge_destroy(immune_bridge);
            immune_bridge = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }

    /* Helper: Full processing cycle */
    void fullProcessingCycle(const float* obs) {
        fep_process_observation(fep, obs, OBS_DIM);
        if (immune_bridge) {
            fep_immune_bridge_update(immune_bridge, 100);
        }
        fep_update_beliefs(fep);
        fep_update_precision(fep);
    }
};

/* ============================================================================
 * Complete Pipeline Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, SystemCreation) {
    EXPECT_NE(fep, nullptr);
    EXPECT_NE(immune_bridge, nullptr);
    EXPECT_NE(learner, nullptr);
}

TEST_F(FEPFullPipelineTest, ImmuneBridgeConnection) {
    ASSERT_NE(immune_bridge, nullptr);
    EXPECT_EQ(immune_bridge->fep_system, fep);
}

TEST_F(FEPFullPipelineTest, ObservationThroughFullPipeline) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i % 4 == 0) ? 1.0f : 0.0f;
    }

    fullProcessingCycle(obs);

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GT(stats.belief_updates, 0u);
}

TEST_F(FEPFullPipelineTest, MultipleProcessingCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf((cycle * OBS_DIM + i) * 0.1f);
        }
        fullProcessingCycle(obs);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GE(stats.belief_updates, 5u);
}

/* ============================================================================
 * Active Inference Pipeline Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, ActionSelectionPipeline) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i % 2 == 0) ? 0.7f : 0.3f;
    }

    fullProcessingCycle(obs);

    fep_evaluate_policies(fep);
    float action[ACTION_DIM];
    int policy_idx = fep_select_action(fep, action, ACTION_DIM);

    EXPECT_GE(policy_idx, -1);
}

TEST_F(FEPFullPipelineTest, PolicyEvaluationLoop) {
    for (int i = 0; i < 3; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = (j == (uint32_t)i) ? 1.0f : 0.0f;
        }

        fullProcessingCycle(obs);
        int ret = fep_evaluate_policies(fep);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(FEPFullPipelineTest, ActiveInferenceComplete) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = (i < OBS_DIM/2) ? 1.0f : 0.0f;
    }

    /* Full active inference cycle */
    fep_process_observation(fep, obs, OBS_DIM);
    fep_update_beliefs(fep);
    fep_update_precision(fep);
    fep_evaluate_policies(fep);

    float action[ACTION_DIM];
    int policy = fep_select_action(fep, action, ACTION_DIM);
    EXPECT_GE(policy, -1);
}

/* ============================================================================
 * Learning Pipeline Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, LearningWithProcessing) {
    ASSERT_NE(learner, nullptr);

    /* Process observation */
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 0.5f;
    }
    fullProcessingCycle(obs);

    /* Learn transition */
    float state[STATE_DIM], next[STATE_DIM];
    for (uint32_t i = 0; i < STATE_DIM; i++) {
        state[i] = (i % 3 == 0) ? 1.0f : 0.0f;
        next[i] = (i % 3 == 1) ? 1.0f : 0.0f;
    }

    int ret = fep_learn_transition(learner, fep, state, next, STATE_DIM);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPFullPipelineTest, MultiStepLearningPipeline) {
    ASSERT_NE(learner, nullptr);

    for (int i = 0; i < 5; i++) {
        /* Process observation */
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = (j == (uint32_t)i % OBS_DIM) ? 1.0f : 0.0f;
        }
        fullProcessingCycle(obs);

        /* Learn transition */
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t j = 0; j < STATE_DIM; j++) {
            state[j] = (j == (uint32_t)i) ? 1.0f : 0.0f;
            next[j] = (j == (uint32_t)((i+1) % STATE_DIM)) ? 1.0f : 0.0f;
        }

        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 5u);
}

/* ============================================================================
 * FEP-Immune Integration Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, ImmuneBridgeUpdate) {
    ASSERT_NE(immune_bridge, nullptr);

    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 0.5f;
    }

    fep_process_observation(fep, obs, OBS_DIM);
    int ret = fep_immune_bridge_update(immune_bridge, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPFullPipelineTest, ImmuneBridgeModulation) {
    ASSERT_NE(immune_bridge, nullptr);

    float precision, learning;
    int ret1 = fep_immune_get_precision_modifier(immune_bridge, &precision);
    int ret2 = fep_immune_get_learning_modifier(immune_bridge, &learning);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);
    EXPECT_GT(precision, 0.0f);
    EXPECT_GT(learning, 0.0f);
}

TEST_F(FEPFullPipelineTest, ImmuneBridgeStats) {
    ASSERT_NE(immune_bridge, nullptr);

    /* Process a few cycles */
    for (int i = 0; i < 3; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = sinf(i + j * 0.1f);
        }
        fullProcessingCycle(obs);
    }

    fep_immune_stats_t stats;
    int ret = fep_immune_bridge_get_stats(immune_bridge, &stats);
    EXPECT_EQ(ret, 0);
}

/* ============================================================================
 * Long-Term Stability Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, ExtendedProcessingSession) {
    for (int i = 0; i < 20; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = sinf((i * OBS_DIM + j) * 0.1f);
        }

        fullProcessingCycle(obs);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPFullPipelineTest, StableProcessingOverTime) {
    for (int i = 0; i < 15; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = (j % 3 == i % 3) ? 1.0f : 0.0f;
        }

        fullProcessingCycle(obs);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GT(stats.belief_updates, 10u);
}

TEST_F(FEPFullPipelineTest, GracefulProcessingCycles) {
    for (int i = 0; i < 10; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = (j == (uint32_t)i % OBS_DIM) ? 1.0f : 0.0f;
        }

        int ret = fep_process_observation(fep, obs, OBS_DIM);
        EXPECT_EQ(ret, 0);

        fep_update_beliefs(fep);
    }
}

/* ============================================================================
 * Prediction Error Analysis Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, PredictionErrorTracking) {
    /* Process predictable pattern */
    float pattern[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        pattern[i] = (i % 2 == 0) ? 1.0f : 0.0f;
    }

    for (int rep = 0; rep < 5; rep++) {
        fep_process_observation(fep, pattern, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float pe = fep_get_prediction_error(fep, 0);
    EXPECT_GE(pe, 0.0f);
}

TEST_F(FEPFullPipelineTest, SurpriseFromNovelInput) {
    /* Establish baseline with pattern */
    float baseline[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        baseline[i] = 0.5f;
    }

    for (int i = 0; i < 5; i++) {
        fep_process_observation(fep, baseline, OBS_DIM);
        fep_update_beliefs(fep);
    }

    /* Novel input */
    float novel[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        novel[i] = 1.0f;
    }

    fep_process_observation(fep, novel, OBS_DIM);
    float pe = fep_get_prediction_error(fep, 0);
    EXPECT_GE(pe, 0.0f);
}

/* ============================================================================
 * Free Energy Minimization Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, FreeEnergyComputation) {
    float obs[OBS_DIM];
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        obs[i] = 0.5f;
    }

    fullProcessingCycle(obs);

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPFullPipelineTest, FreeEnergyUnderVariedInput) {
    for (int pattern = 0; pattern < 4; pattern++) {
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = (i % 4 == (uint32_t)pattern) ? 1.0f : 0.0f;
        }

        fullProcessingCycle(obs);

        float fe = fep_get_free_energy(fep);
        EXPECT_FALSE(std::isnan(fe));
    }
}

/* ============================================================================
 * Integration Stress Tests
 * ============================================================================ */

TEST_F(FEPFullPipelineTest, RapidInputChanges) {
    for (int i = 0; i < 20; i++) {
        float obs[OBS_DIM];
        for (uint32_t j = 0; j < OBS_DIM; j++) {
            obs[j] = (i % 2 == 0) ? ((j % 2 == 0) ? 1.0f : 0.0f) : ((j % 2 == 1) ? 1.0f : 0.0f);
        }

        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);
    }

    float fe = fep_get_free_energy(fep);
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(FEPFullPipelineTest, ContinuousLearningLoop) {
    ASSERT_NE(learner, nullptr);

    for (int epoch = 0; epoch < 10; epoch++) {
        /* Process observation */
        float obs[OBS_DIM];
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = sinf(epoch * 0.5f + i * 0.1f);
        }
        fep_process_observation(fep, obs, OBS_DIM);
        fep_update_beliefs(fep);

        /* Learn */
        float state[STATE_DIM], next[STATE_DIM];
        for (uint32_t i = 0; i < STATE_DIM; i++) {
            state[i] = obs[i % OBS_DIM];
            next[i] = sinf((epoch + 1) * 0.5f + i * 0.1f);
        }
        fep_learn_transition(learner, fep, state, next, STATE_DIM);
    }

    fep_learning_stats_t stats;
    fep_transition_learning_get_stats(learner, &stats);
    EXPECT_GE(stats.total_updates, 10u);
}

