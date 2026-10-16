/**
 * @file test_fep_bridges_stability_regression.cpp
 * @brief Stability regression tests for FEP bridge modules
 *
 * WHAT: Tests bridge stability under repeated operations, stress conditions
 * WHY:  Prevent regressions in long-running stability, memory safety
 * HOW:  Repeated update cycles, stress testing, memory leak detection
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
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

class FEPStabilityRegressionTest : public ::testing::Test {
protected:
    static const int ITERATIONS = 1000;
    static const int STRESS_ITERATIONS = 5000;
    static const uint64_t TIMESTEP_MS = 16; // 60Hz update rate

    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;
    introspection_context_t introspection = nullptr;

    static const uint32_t OBS_DIM = 16;
    static const uint32_t ACTION_DIM = 8;

    void SetUp() override {
        // Create FEP system
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_config.num_levels = 2;
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);

        // Create brain immune system
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
};

/* ============================================================================
 * Immune Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, ImmuneBridgeRepeatedUpdateStability) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    // Repeated updates should not crash or cause memory issues
    for (int i = 0; i < ITERATIONS; i++) {
        int ret = fep_immune_bridge_update(bridge, TIMESTEP_MS);
        EXPECT_EQ(ret, 0);
    }

    // Verify state remains valid
    fep_immune_state_t state;
    int ret = fep_immune_bridge_get_state(bridge, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.precision_reduction, 0.0f);
    EXPECT_LE(state.precision_reduction, 1.0f);

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPStabilityRegressionTest, ImmuneBridgeStressTest) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    fep_immune_bridge_connect_fep(bridge, fep);
    fep_immune_bridge_connect_immune(bridge, immune);

    // High-frequency stress test
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        fep_immune_bridge_update(bridge, TIMESTEP_MS);

        // Periodically report prediction failures
        if (i % 100 == 0) {
            float magnitude = 5.0f + (i % 10) * 2.0f;
            fep_immune_report_prediction_failure(bridge, magnitude);
        }
    }

    // Check statistics are reasonable
    fep_immune_stats_t stats;
    int ret = fep_immune_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GT(stats.immune_activations, 0);

    fep_immune_bridge_destroy(bridge);
}

TEST_F(FEPStabilityRegressionTest, ImmuneBridgeRapidConnectionCycles) {
    fep_immune_config_t config;
    fep_immune_bridge_default_config(&config);
    fep_immune_bridge_t* bridge = fep_immune_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Rapid connect/disconnect cycles
    for (int i = 0; i < 100; i++) {
        int ret1 = fep_immune_bridge_connect_fep(bridge, fep);
        EXPECT_EQ(ret1, 0);

        int ret2 = fep_immune_bridge_connect_immune(bridge, immune);
        EXPECT_EQ(ret2, 0);

        int ret3 = fep_immune_bridge_disconnect(bridge);
        EXPECT_EQ(ret3, 0);
    }

    fep_immune_bridge_destroy(bridge);
}

/* ============================================================================
 * Learning Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, LearningBridgeConvergenceStability) {
    const uint32_t STATE_DIM = 8;

    fep_learning_config_t config;
    fep_learning_default_config(&config);
    config.learning_rate = 0.01f;

    // Verified API
    fep_transition_learner_t* trans_learner = fep_transition_learner_create(&config, STATE_DIM);
    ASSERT_NE(trans_learner, nullptr);

    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(&config, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    // Train over many iterations
    float observations[OBS_DIM];
    float prev_states[STATE_DIM] = {0.5f, 0.5f, 0.3f, 0.3f, 0.2f, 0.2f, 0.1f, 0.1f};
    float curr_states[STATE_DIM] = {0.6f, 0.6f, 0.4f, 0.4f, 0.3f, 0.3f, 0.2f, 0.2f};

    for (int i = 0; i < OBS_DIM; i++) {
        observations[i] = 0.5f + 0.1f * (i % 3);
    }

    std::vector<float> loss_history;
    for (int i = 0; i < ITERATIONS; i++) {
        // Update transition matrix (verified API)
        fep_learn_transition(trans_learner, fep, prev_states, curr_states, STATE_DIM);

        // Update likelihood matrix (verified API)
        fep_learn_likelihood(like_learner, fep, observations, curr_states, OBS_DIM, STATE_DIM);

        // Track loss
        fep_learning_stats_t stats;
        int ret = fep_likelihood_learning_get_stats(like_learner, &stats);
        EXPECT_EQ(ret, 0);
        loss_history.push_back(stats.current_loss);
    }

    // Loss should generally decrease (may have some noise)
    EXPECT_LT(loss_history.back(), loss_history[10]);

    fep_likelihood_learner_destroy(like_learner);
    fep_transition_learner_destroy(trans_learner);
}

TEST_F(FEPStabilityRegressionTest, LearningBridgeBatchUpdateStability) {
    const uint32_t STATE_DIM = 8;

    fep_learning_config_t config;
    fep_learning_default_config(&config);

    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(&config, OBS_DIM, STATE_DIM);
    ASSERT_NE(like_learner, nullptr);

    float observations[OBS_DIM];
    float states[STATE_DIM];
    for (int i = 0; i < OBS_DIM; i++) observations[i] = 0.5f;
    for (uint32_t i = 0; i < STATE_DIM; i++) states[i] = 0.5f;

    // Batch updates with various batch sizes
    const int batch_sizes[] = {1, 5, 10, 50, 100};
    for (int batch_size : batch_sizes) {
        for (int iter = 0; iter < 10; iter++) {
            // Simulate batch update by performing batch_size learning steps
            for (int j = 0; j < batch_size; j++) {
                fep_learn_likelihood(like_learner, fep, observations, states, OBS_DIM, STATE_DIM);
            }
        }
    }

    fep_likelihood_learner_destroy(like_learner);
}

/* ============================================================================
 * Context Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, ContextBridgeRapidSwitchingStability) {
    fep_context_config_t config;
    fep_context_default_config(&config);

    fep_context_system_t* context = fep_context_create(&config);
    ASSERT_NE(context, nullptr);

    // Verified API
    fep_context_connect(context, fep);

    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    // Add multiple contexts (verified API: fep_context_add(sys, name, prior_beliefs, belief_dim, &context_id))
    uint32_t ctx_ids[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "context_%d", i);
        int ret = fep_context_add(context, name, prior_beliefs, OBS_DIM, &ctx_ids[i]);
        EXPECT_EQ(ret, 0);
    }

    // Rapid context switching (verified API: fep_context_switch(sys, fep, target_context_id))
    for (int i = 0; i < ITERATIONS; i++) {
        uint32_t target_ctx = ctx_ids[i % 5];
        int ret = fep_context_switch(context, fep, target_ctx);
        EXPECT_EQ(ret, 0);
    }

    // Verify system is still responsive
    uint32_t active_ctx;
    int ret = fep_context_get_active(context, &active_ctx);
    EXPECT_EQ(ret, 0);

    fep_context_destroy(context);
}

TEST_F(FEPStabilityRegressionTest, ContextBridgeInferenceStability) {
    fep_context_config_t config;
    fep_context_default_config(&config);

    fep_context_system_t* context = fep_context_create(&config);
    ASSERT_NE(context, nullptr);

    // Verified API
    fep_context_connect(context, fep);

    float prior_beliefs[OBS_DIM];
    for (int j = 0; j < OBS_DIM; j++) prior_beliefs[j] = 1.0f / OBS_DIM;

    // Add contexts (verified API)
    uint32_t ctx1, ctx2;
    fep_context_add(context, "context_1", prior_beliefs, OBS_DIM, &ctx1);
    fep_context_add(context, "context_2", prior_beliefs, OBS_DIM, &ctx2);

    // Repeated context inference
    float observations[OBS_DIM];
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Vary observations
        for (int i = 0; i < OBS_DIM; i++) {
            observations[i] = 0.5f + 0.3f * sinf(iter * 0.01f + i);
        }

        uint32_t inferred_ctx;
        float confidence;
        int ret = fep_context_infer(context, fep, observations, OBS_DIM, &inferred_ctx, &confidence);
        EXPECT_EQ(ret, 0);
        EXPECT_GE(confidence, 0.0f);
        EXPECT_LE(confidence, 1.0f);
    }

    fep_context_destroy(context);
}

/* ============================================================================
 * Neuromodulation Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, NeuromodBridgeLongRunningStability) {
    fep_neuromod_config_t config;
    fep_neuromod_default_config(&config);

    fep_neuromod_system_t* neuromod = fep_neuromod_create(&config);
    ASSERT_NE(neuromod, nullptr);

    // Verified API
    fep_neuromod_connect(neuromod, fep);

    // Long-running modulation simulation
    for (int i = 0; i < ITERATIONS; i++) {
        // Update neuromodulation state
        int ret = fep_neuromod_update(neuromod, TIMESTEP_MS);
        EXPECT_EQ(ret, 0);

        // Periodically apply modulation (verified API)
        if (i % 10 == 0) {
            fep_neuromod_apply_to_fep(neuromod, fep);
        }
    }

    // Check modulation levels are in valid ranges (verified: state uses levels[] array)
    fep_neuromod_state_t state;
    int ret = fep_neuromod_get_state(neuromod, &state);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.levels[FEP_NEUROMOD_DA], 0.0f);
    EXPECT_LE(state.levels[FEP_NEUROMOD_DA], 1.0f);

    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPStabilityRegressionTest, NeuromodBridgeExtremeModulationStability) {
    fep_neuromod_config_t config;
    fep_neuromod_default_config(&config);

    fep_neuromod_system_t* neuromod = fep_neuromod_create(&config);
    ASSERT_NE(neuromod, nullptr);

    // Verified API
    fep_neuromod_connect(neuromod, fep);

    // Test extreme modulation levels
    const float extreme_levels[] = {0.0f, 0.1f, 0.5f, 0.9f, 1.0f};

    for (float level : extreme_levels) {
        // Set dopamine (verified API: fep_neuromod_set_level(sys, type, level))
        int ret1 = fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, level);
        EXPECT_EQ(ret1, 0);

        // Set serotonin (verified API)
        int ret2 = fep_neuromod_set_level(neuromod, FEP_NEUROMOD_5HT, 1.0f - level);
        EXPECT_EQ(ret2, 0);

        // Apply modulation (verified API)
        int ret3 = fep_neuromod_apply_to_fep(neuromod, fep);
        EXPECT_EQ(ret3, 0);

        // Update
        fep_neuromod_update(neuromod, TIMESTEP_MS);
    }

    fep_neuromod_destroy(neuromod);
}

/* ============================================================================
 * Sleep Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, SleepBridgeCycleStability) {
    fep_sleep_config_t config;
    fep_sleep_default_config(&config);

    fep_sleep_system_t* sleep = fep_sleep_create(&config);
    ASSERT_NE(sleep, nullptr);

    // Verified API
    fep_sleep_connect(sleep, fep);

    // Simulate multiple sleep/wake cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Enter sleep (verified API: fep_sleep_set_stage(sys, stage))
        int ret1 = fep_sleep_set_stage(sleep, SLEEP_STAGE_N1);
        EXPECT_EQ(ret1, 0);

        // Sleep for "some time"
        for (int i = 0; i < 100; i++) {
            fep_sleep_update(sleep, TIMESTEP_MS);
        }

        // Wake up (verified API)
        int ret2 = fep_sleep_set_stage(sleep, SLEEP_STAGE_WAKE);
        EXPECT_EQ(ret2, 0);

        // Awake for "some time"
        for (int i = 0; i < 100; i++) {
            fep_sleep_update(sleep, TIMESTEP_MS);
        }
    }

    fep_sleep_destroy(sleep);
}

TEST_F(FEPStabilityRegressionTest, SleepBridgeConsolidationStability) {
    fep_sleep_config_t config;
    fep_sleep_default_config(&config);
    config.enable_replay_consolidation = true;

    fep_sleep_system_t* sleep = fep_sleep_create(&config);
    ASSERT_NE(sleep, nullptr);

    // Verified API
    fep_sleep_connect(sleep, fep);

    // Repeated consolidation cycles (verified: use set_stage for SWS which does consolidation)
    fep_sleep_set_stage(sleep, SLEEP_STAGE_SWS);

    for (int i = 0; i < ITERATIONS; i++) {
        // SWS stage handles consolidation during update
        fep_sleep_update(sleep, TIMESTEP_MS);
    }

    fep_sleep_set_stage(sleep, SLEEP_STAGE_WAKE);
    fep_sleep_destroy(sleep);
}

/* ============================================================================
 * Multi-Bridge Stability Tests
 * ============================================================================ */

TEST_F(FEPStabilityRegressionTest, MultiBridgeConcurrentOperations) {
    const uint32_t STATE_DIM = 8;

    // Create multiple bridges
    fep_immune_bridge_t* immune_bridge = fep_immune_bridge_create(nullptr);
    fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
    fep_neuromod_system_t* neuromod = fep_neuromod_create(nullptr);

    ASSERT_NE(immune_bridge, nullptr);
    ASSERT_NE(like_learner, nullptr);
    ASSERT_NE(neuromod, nullptr);

    // Connect all (verified API)
    fep_immune_bridge_connect_fep(immune_bridge, fep);
    fep_immune_bridge_connect_immune(immune_bridge, immune);
    fep_neuromod_connect(neuromod, fep);

    // Concurrent updates
    for (int i = 0; i < 500; i++) {
        fep_immune_bridge_update(immune_bridge, TIMESTEP_MS);
        fep_neuromod_update(neuromod, TIMESTEP_MS);

        if (i % 10 == 0) {
            fep_neuromod_apply_to_fep(neuromod, fep);
        }
    }

    // All should still be operational
    fep_immune_state_t immune_state;
    EXPECT_EQ(fep_immune_bridge_get_state(immune_bridge, &immune_state), 0);

    fep_neuromod_state_t neuromod_state;
    EXPECT_EQ(fep_neuromod_get_state(neuromod, &neuromod_state), 0);

    // Cleanup
    fep_immune_bridge_destroy(immune_bridge);
    fep_likelihood_learner_destroy(like_learner);
    fep_neuromod_destroy(neuromod);
}

TEST_F(FEPStabilityRegressionTest, MultiBridgeMemorySafety) {
    const uint32_t STATE_DIM = 8;

    // Create and destroy bridges repeatedly
    for (int i = 0; i < 100; i++) {
        fep_immune_bridge_t* immune_bridge = fep_immune_bridge_create(nullptr);
        fep_likelihood_learner_t* like_learner = fep_likelihood_learner_create(nullptr, OBS_DIM, STATE_DIM);
        fep_context_system_t* context = fep_context_create(nullptr);

        ASSERT_NE(immune_bridge, nullptr);
        ASSERT_NE(like_learner, nullptr);
        ASSERT_NE(context, nullptr);

        // Do some operations (verified API)
        fep_immune_bridge_connect_fep(immune_bridge, fep);
        fep_context_connect(context, fep);

        fep_immune_bridge_update(immune_bridge, TIMESTEP_MS);

        // Cleanup
        fep_immune_bridge_destroy(immune_bridge);
        fep_likelihood_learner_destroy(like_learner);
        fep_context_destroy(context);
    }
    // No memory leaks expected
}
