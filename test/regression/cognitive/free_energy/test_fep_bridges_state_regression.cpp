/**
 * @file test_fep_bridges_state_regression.cpp
 * @brief State consistency regression tests for FEP bridge modules
 *
 * WHAT: Tests state consistency across update cycles, range validation, trend verification
 * WHY:  Prevent state corruption, ensure values stay in valid ranges, verify expected behaviors
 * HOW:  Monitor state over many updates, validate ranges, check monotonicity/convergence
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_immune_bridge.h"
#include "cognitive/free_energy/nimcp_fep_consciousness.h"
#include "cognitive/free_energy/nimcp_fep_context.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/free_energy/nimcp_fep_planning.h"
#include "cognitive/free_energy/nimcp_fep_sleep.h"
#include "cognitive/free_energy/nimcp_fep_evidence.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/introspection/nimcp_introspection.h"

class FEPStateRegressionTest : public ::testing::Test {
protected:
    static const int ITERATIONS = 1000;
    static const uint64_t TIMESTEP_MS = 16;

    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;

    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;

    void SetUp() override {
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.num_levels = 2;
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune = brain_immune_create(&immune_config);
        ASSERT_NE(immune, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }

    // Helper to check if value is in valid range
    bool in_range(float value, float min, float max) {
        return value >= min && value <= max && !std::isnan(value) && !std::isinf(value);
    }
};

/* ============================================================================
 * Immune Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, ImmuneBridgePrecisionRangeRegression) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    // Track precision reduction over many updates
    for (int i = 0; i < ITERATIONS; i++) {
        fep_immune_bridge_update(bridge, TIMESTEP_MS);

        fep_immune_state_t state;
        int ret = fep_immune_bridge_get_state(bridge, &state);
        ASSERT_EQ(ret, 0);

        // Precision reduction must be in [0, 1]
        EXPECT_TRUE(in_range(state.precision_reduction, 0.0f, 1.0f))
            << "Iteration " << i << ": precision_reduction=" << state.precision_reduction;

        // Learning impairment must be in [0, 1]
        EXPECT_TRUE(in_range(state.learning_impairment, 0.0f, 1.0f))
            << "Iteration " << i << ": learning_impairment=" << state.learning_impairment;

        // Current inflammation must be in [0, 1]
        EXPECT_TRUE(in_range(state.current_inflammation, 0.0f, 1.0f))
            << "Iteration " << i << ": current_inflammation=" << state.current_inflammation;

        // Sickness intensity must be in [0, 1]
        EXPECT_TRUE(in_range(state.sickness_intensity, 0.0f, 1.0f))
            << "Iteration " << i << ": sickness_intensity=" << state.sickness_intensity;

        // Recovery progress must be in [0, 1]
        EXPECT_TRUE(in_range(state.recovery_progress, 0.0f, 1.0f))
            << "Iteration " << i << ": recovery_progress=" << state.recovery_progress;
    }

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPStateRegressionTest, ImmuneBridgeStatisticsMonotonicity) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    fep_immune_stats_t prev_stats;
    int ret = fep_immune_bridge_get_stats(bridge, &prev_stats);
    ASSERT_EQ(ret, 0);

    // Statistics should only increase (counters)
    for (int i = 0; i < 100; i++) {
        fep_immune_bridge_update(bridge, TIMESTEP_MS);

        if (i % 10 == 0) {
            float magnitude = 5.0f + (i % 5) * 2.0f;
            fep_immune_report_prediction_failure(bridge, magnitude);
        }

        fep_immune_stats_t curr_stats;
        ret = fep_immune_bridge_get_stats(bridge, &curr_stats);
        ASSERT_EQ(ret, 0);

        // Counters should be monotonic
        EXPECT_GE(curr_stats.prediction_failures, prev_stats.prediction_failures);
        EXPECT_GE(curr_stats.immune_activations, prev_stats.immune_activations);

        prev_stats = curr_stats;
    }

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPStateRegressionTest, ImmuneBridgeInflammationLevelConsistency) {
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    // Verify inflammation level matches current_inflammation value
    for (int i = 0; i < 100; i++) {
        fep_immune_bridge_update(bridge, TIMESTEP_MS);

        fep_immune_state_t state;
        int ret = fep_immune_bridge_get_state(bridge, &state);
        ASSERT_EQ(ret, 0);

        brain_inflammation_level_t level = fep_immune_get_inflammation_level(bridge);

        // Check consistency between level enum and float value
        switch (level) {
            case INFLAMMATION_NONE:
                EXPECT_LT(state.current_inflammation, 0.1f);
                break;
            case INFLAMMATION_LOCAL:
                EXPECT_GE(state.current_inflammation, 0.1f);
                EXPECT_LT(state.current_inflammation, 0.3f);
                break;
            case INFLAMMATION_REGIONAL:
                EXPECT_GE(state.current_inflammation, 0.3f);
                EXPECT_LT(state.current_inflammation, 0.6f);
                break;
            case INFLAMMATION_SYSTEMIC:
                EXPECT_GE(state.current_inflammation, 0.6f);
                EXPECT_LT(state.current_inflammation, 0.9f);
                break;
            case INFLAMMATION_STORM:
                EXPECT_GE(state.current_inflammation, 0.9f);
                break;
        }
    }

    fep_immune_bridge_destroy(bridge);
}

/* ============================================================================
 * Learning Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, LearningBridgeLossMonotonicity) {
    const uint32_t STATE_DIM = 8;

    fep_learning_config_t config;
    fep_learning_default_config(&config);
    config.learning_rate = 0.01f;

    // Create transition learner (verified API: fep_transition_learner_create(config, state_dim))
    fep_transition_learner_t* trans_learner = fep_transition_learner_create(&config, STATE_DIM);
    ASSERT_NE(trans_learner, nullptr);

    // Create likelihood learner (verified API: fep_likelihood_learner_create(config, obs_dim, state_dim))
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(&config, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float prev_states[STATE_DIM] = {0.5f, 0.5f, 0.3f, 0.3f, 0.2f, 0.2f, 0.1f, 0.1f};
    float curr_states[STATE_DIM] = {0.6f, 0.6f, 0.4f, 0.4f, 0.3f, 0.3f, 0.2f, 0.2f};

    for (int i = 0; i < OBS_DIM; i++) {
        observations[i] = 0.5f + 0.1f * (i % 3);
    }

    std::vector<float> loss_history;

    // Train and track loss
    for (int i = 0; i < 500; i++) {
        // Verified API: fep_learn_transition(learner, sys, state_t, state_t1, dim)
        fep_learn_transition(trans_learner, fep, prev_states, curr_states, STATE_DIM);
        // Verified API: fep_learn_likelihood(learner, sys, observation, state, obs_dim)
        fep_learn_likelihood(like_learner, fep, observations, curr_states, OBS_DIM, STATE_DIM);

        // Get stats to check loss
        fep_learning_stats_t stats;
        int ret = fep_likelihood_learning_get_stats(like_learner, &stats);
        ASSERT_EQ(ret, 0);

        float loss = stats.current_loss;
        EXPECT_TRUE(in_range(loss, 0.0f, 1000.0f)) << "Iteration " << i << ": loss=" << loss;
        loss_history.push_back(loss);
    }

    // Loss should show general decreasing trend
    // Compare first 50 vs last 50
    float avg_early = 0.0f, avg_late = 0.0f;
    for (int i = 0; i < 50; i++) {
        avg_early += loss_history[i];
        avg_late += loss_history[loss_history.size() - 50 + i];
    }
    avg_early /= 50.0f;
    avg_late /= 50.0f;

    EXPECT_LT(avg_late, avg_early) << "Loss should decrease with training";

    fep_likelihood_learner_destroy(like_learner);
    fep_transition_learner_destroy(trans_learner);
}

TEST_F(FEPStateRegressionTest, LearningBridgeGradientNormRegression) {
    const uint32_t STATE_DIM = 8;

    // Create likelihood learner (verified API)
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float states[STATE_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;
    for (uint32_t i = 0; i < STATE_DIM; i++) states[i] = 0.5f;

    // Gradient norm should remain finite
    for (int i = 0; i < ITERATIONS; i++) {
        // Verified API: fep_learn_likelihood(learner, sys, observation, state, obs_dim)
        fep_learn_likelihood(like_learner, fep, observations, states, OBS_DIM, STATE_DIM);

        // Get stats to check gradient norm
        fep_learning_stats_t stats;
        int ret = fep_likelihood_learning_get_stats(like_learner, &stats);
        ASSERT_EQ(ret, 0);

        float grad_norm = stats.current_grad_norm;
        EXPECT_FALSE(std::isnan(grad_norm)) << "Iteration " << i;
        EXPECT_FALSE(std::isinf(grad_norm)) << "Iteration " << i;
        EXPECT_GE(grad_norm, 0.0f) << "Iteration " << i;
        EXPECT_LT(grad_norm, 1000.0f) << "Iteration " << i; // Reasonable upper bound
    }

    fep_likelihood_learner_destroy(like_learner);
}

TEST_F(FEPStateRegressionTest, LearningBridgeConvergenceDetection) {
    const uint32_t STATE_DIM = 8;

    fep_learning_config_t config;
    fep_learning_default_config(&config);
    config.convergence_threshold = 0.01f;

    // Create likelihood learner (verified API)
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(&config, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float states[STATE_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;
    for (uint32_t i = 0; i < STATE_DIM; i++) states[i] = 0.5f;

    bool converged = false;
    int convergence_iter = -1;

    for (int i = 0; i < 1000; i++) {
        // Verified API: fep_learn_likelihood(learner, sys, observation, state, obs_dim)
        fep_learn_likelihood(like_learner, fep, observations, states, OBS_DIM, STATE_DIM);

        // Check convergence via stats.state
        fep_learning_stats_t stats;
        fep_likelihood_learning_get_stats(like_learner, &stats);
        bool is_converged = (stats.state == FEP_LEARNING_CONVERGED);

        if (is_converged && !converged) {
            converged = true;
            convergence_iter = i;
        }

        // Once converged, should stay converged (or very close)
        if (converged && i > convergence_iter + 50) {
            EXPECT_TRUE(is_converged || i < convergence_iter + 100)
                << "Should maintain convergence after iteration " << convergence_iter;
        }
    }

    fep_likelihood_learner_destroy(like_learner);
}

/* ============================================================================
 * Context Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, ContextBridgeActivationRangeRegression) {
    fep_context_system_t* context = fep_context_create(nullptr);
    ASSERT_NE(context, nullptr);

    fep_context_connect(context, fep);

    uint32_t ctx_ids[5];
    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "context_%d", i);
        // Verified API: fep_context_add(sys, name, prior_beliefs, belief_dim, &context_id)
        int ret = fep_context_add(context, name, prior_beliefs, OBS_DIM, &ctx_ids[i]);
        ASSERT_EQ(ret, 0);
    }

    float observations[OBS_DIM];

    // Verify activation levels stay in [0, 1]
    for (int i = 0; i < ITERATIONS; i++) {
        for (int j = 0; j < OBS_DIM; j++) {
            observations[j] = 0.5f + 0.3f * sinf(i * 0.01f + j);
        }

        uint32_t inferred_ctx;
        float confidence;
        int ret = fep_context_infer(context, fep, observations, OBS_DIM, &inferred_ctx, &confidence);
        ASSERT_EQ(ret, 0);

        EXPECT_TRUE(in_range(confidence, 0.0f, 1.0f))
            << "Iteration " << i << ": confidence=" << confidence;

        // Verify inferred context is one of the valid IDs
        bool valid_ctx = false;
        for (int j = 0; j < 5; j++) {
            if (inferred_ctx == ctx_ids[j]) {
                valid_ctx = true;
                break;
            }
        }
        EXPECT_TRUE(valid_ctx) << "Iteration " << i << ": inferred invalid context";
    }

    fep_context_destroy(context);
}

TEST_F(FEPStateRegressionTest, ContextBridgeSwitchConsistency) {
    fep_context_system_t* context = fep_context_create(nullptr);
    ASSERT_NE(context, nullptr);

    fep_context_connect(context, fep);

    uint32_t ctx1, ctx2;
    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    // Verified API: fep_context_add(sys, name, prior_beliefs, belief_dim, &context_id)
    fep_context_add(context, "context_1", prior_beliefs, OBS_DIM, &ctx1);
    fep_context_add(context, "context_2", prior_beliefs, OBS_DIM, &ctx2);

    // After explicit switch, active context should match
    for (int i = 0; i < 100; i++) {
        uint32_t target = (i % 2 == 0) ? ctx1 : ctx2;
        // Verified API: fep_context_switch(sys, fep, target_context_id)
        int ret = fep_context_switch(context, fep, target);
        ASSERT_EQ(ret, 0);

        uint32_t active;
        ret = fep_context_get_active(context, &active);
        ASSERT_EQ(ret, 0);

        EXPECT_EQ(active, target) << "Iteration " << i;
    }

    fep_context_destroy(context);
}

/* ============================================================================
 * Neuromodulation Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, NeuromodBridgeLevelRangeRegression) {
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);

    fep_neuromod_connect(neuromod, fep);

    // Verify all neuromodulator levels stay in [0, 1]
    for (int i = 0; i < ITERATIONS; i++) {
        fep_neuromod_update(neuromod, TIMESTEP_MS);

        fep_neuromod_state_t state;
        int ret = fep_neuromod_get_state(neuromod, &state);
        ASSERT_EQ(ret, 0);

        // State uses levels[FEP_NEUROMOD_*] array
        EXPECT_TRUE(in_range(state.levels[FEP_NEUROMOD_DA], 0.0f, 1.0f))
            << "Iteration " << i << ": dopamine=" << state.levels[FEP_NEUROMOD_DA];

        EXPECT_TRUE(in_range(state.levels[FEP_NEUROMOD_5HT], 0.0f, 1.0f))
            << "Iteration " << i << ": serotonin=" << state.levels[FEP_NEUROMOD_5HT];

        EXPECT_TRUE(in_range(state.levels[FEP_NEUROMOD_NE], 0.0f, 1.0f))
            << "Iteration " << i << ": norepinephrine=" << state.levels[FEP_NEUROMOD_NE];

        EXPECT_TRUE(in_range(state.levels[FEP_NEUROMOD_ACH], 0.0f, 1.0f))
            << "Iteration " << i << ": acetylcholine=" << state.levels[FEP_NEUROMOD_ACH];
    }

    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPStateRegressionTest, NeuromodBridgeSetLevelConsistency) {
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);

    fep_neuromod_connect(neuromod, fep);

    const float test_levels[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float level : test_levels) {
        // Set dopamine using verified API
        int ret1 = fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, level);
        ASSERT_EQ(ret1, 0);

        // Verify it was set correctly
        fep_neuromod_state_t state;
        int ret2 = fep_neuromod_get_state(neuromod, &state);
        ASSERT_EQ(ret2, 0);

        EXPECT_NEAR(state.levels[FEP_NEUROMOD_DA], level, 0.01f)
            << "Set level=" << level << " but got " << state.levels[FEP_NEUROMOD_DA];
    }

    fep_neuromod_destroy(neuromod);
}

/* ============================================================================
 * Sleep Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, SleepBridgeStateTransitionConsistency) {
    fep_sleep_system_t* sleep = fep_sleep_create(nullptr);
    ASSERT_NE(sleep, nullptr);

    fep_sleep_connect(sleep, fep);

    // Test state transitions are consistent
    for (int cycle = 0; cycle < 10; cycle++) {
        // Enter sleep using verified API
        int ret1 = fep_sleep_set_stage(sleep, SLEEP_STAGE_N1);
        ASSERT_EQ(ret1, 0);

        fep_sleep_state_t state1;
        fep_sleep_get_state(sleep, &state1);
        EXPECT_NE(state1.current_stage, SLEEP_STAGE_WAKE);

        // Update in sleep
        for (int i = 0; i < 50; i++) {
            fep_sleep_update(sleep, TIMESTEP_MS);

            fep_sleep_state_t state;
            fep_sleep_get_state(sleep, &state);
            EXPECT_NE(state.current_stage, SLEEP_STAGE_WAKE) << "Cycle " << cycle << ", update " << i;
        }

        // Wake up using verified API
        int ret2 = fep_sleep_set_stage(sleep, SLEEP_STAGE_WAKE);
        ASSERT_EQ(ret2, 0);

        fep_sleep_state_t state2;
        fep_sleep_get_state(sleep, &state2);
        EXPECT_EQ(state2.current_stage, SLEEP_STAGE_WAKE);

        // Update while awake
        for (int i = 0; i < 50; i++) {
            fep_sleep_update(sleep, TIMESTEP_MS);

            fep_sleep_state_t state;
            fep_sleep_get_state(sleep, &state);
            EXPECT_EQ(state.current_stage, SLEEP_STAGE_WAKE) << "Cycle " << cycle << ", update " << i;
        }
    }

    fep_sleep_destroy(sleep);
}

TEST_F(FEPStateRegressionTest, SleepBridgeTimeAccumulation) {
    fep_sleep_system_t* sleep = fep_sleep_create(nullptr);
    ASSERT_NE(sleep, nullptr);

    fep_sleep_connect(sleep, fep);

    fep_sleep_set_stage(sleep, SLEEP_STAGE_SWS);

    uint64_t expected_time = 0;

    // Verify time accumulates correctly
    for (int i = 0; i < 100; i++) {
        fep_sleep_update(sleep, TIMESTEP_MS);
        expected_time += TIMESTEP_MS;

        fep_sleep_state_t state;
        fep_sleep_get_state(sleep, &state);

        // Allow small tolerance for floating point
        EXPECT_NEAR(state.total_sleep_ms, expected_time, TIMESTEP_MS)
            << "Iteration " << i;
    }

    fep_sleep_destroy(sleep);
}

/* ============================================================================
 * Curiosity Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, CuriosityBridgeNoveltyRangeRegression) {
    fep_curiosity_config_t config;
    fep_curiosity_default_config(&config);

    fep_curiosity_system_t* curiosity = fep_curiosity_create(&config);
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    float observations[OBS_DIM];

    // Novelty should stay in [0, 1] range
    for (int i = 0; i < ITERATIONS; i++) {
        for (int j = 0; j < OBS_DIM; j++) {
            observations[j] = 0.5f + 0.3f * sinf(i * 0.01f + j);
        }

        // fep_compute_novelty returns float directly
        float novelty = fep_compute_novelty(curiosity, observations, OBS_DIM);

        EXPECT_TRUE(in_range(novelty, 0.0f, 1.0f))
            << "Iteration " << i << ": novelty=" << novelty;
    }

    fep_curiosity_destroy(curiosity);
}

TEST_F(FEPStateRegressionTest, CuriosityBridgeIntrinsicMotivationRegression) {
    fep_curiosity_config_t config;
    fep_curiosity_default_config(&config);

    fep_curiosity_system_t* curiosity = fep_curiosity_create(&config);
    ASSERT_NE(curiosity, nullptr);

    fep_curiosity_connect(curiosity, fep);

    float observations[OBS_DIM];

    // Intrinsic motivation (exploration drive) should stay in valid range
    for (int i = 0; i < ITERATIONS; i++) {
        // Generate observations and record them
        for (int j = 0; j < OBS_DIM; j++) {
            observations[j] = 0.5f + 0.3f * sinf(i * 0.01f + j);
        }
        fep_curiosity_record_observation(curiosity, observations, OBS_DIM);

        fep_curiosity_state_t state;
        int ret = fep_curiosity_get_state(curiosity, &state);
        ASSERT_EQ(ret, 0);

        EXPECT_TRUE(in_range(state.exploration_drive, 0.0f, 1.0f))
            << "Iteration " << i << ": exploration_drive=" << state.exploration_drive;
    }

    fep_curiosity_destroy(curiosity);
}

/* ============================================================================
 * Evidence Bridge State Consistency Tests
 * ============================================================================ */

TEST_F(FEPStateRegressionTest, EvidenceBridgeValueRangeRegression) {
    fep_evidence_config_t config;
    fep_evidence_default_config(&config);

    fep_evidence_system_t* evidence = fep_evidence_create(&config);
    ASSERT_NE(evidence, nullptr);

    fep_evidence_connect(evidence, fep);

    float observations[OBS_DIM];

    // Evidence values should be finite
    for (int i = 0; i < ITERATIONS; i++) {
        // Generate observations
        for (int j = 0; j < OBS_DIM; j++) {
            observations[j] = 0.5f + 0.3f * sinf(i * 0.01f + j);
        }

        fep_evidence_result_t result;
        int ret = fep_compute_log_evidence(evidence, fep, observations, 1, OBS_DIM, &result);
        ASSERT_EQ(ret, 0);

        EXPECT_FALSE(std::isnan(result.log_evidence)) << "Iteration " << i;
        EXPECT_FALSE(std::isinf(result.log_evidence)) << "Iteration " << i;
        EXPECT_GT(result.log_evidence, -1000.0f) << "Iteration " << i; // Reasonable lower bound
    }

    fep_evidence_destroy(evidence);
}
