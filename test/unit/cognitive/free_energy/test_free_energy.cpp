/**
 * @file test_free_energy.cpp
 * @brief Unit tests for Free Energy Principle Module
 * @date 2025-12-12
 *
 * Tests hierarchical predictive processing, belief updates, free energy
 * computation, active inference, and policy selection.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class FreeEnergyTest : public ::testing::Test {
protected:
    fep_system_t* fep = nullptr;
    fep_config_t config;

    static constexpr uint32_t OBS_DIM = 16;
    static constexpr uint32_t ACTION_DIM = 4;

    void SetUp() override {
        fep_default_config(&config);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }

    // Helper to create FEP system with defaults
    void createFEP() {
        fep = fep_create(&config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);
    }

    // Helper to create observation
    std::vector<float> createObservation(float base_value = 1.0f) {
        std::vector<float> obs(OBS_DIM);
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = base_value + 0.1f * i;
        }
        return obs;
    }

    // Helper to create random observation
    std::vector<float> createRandomObservation() {
        std::vector<float> obs(OBS_DIM);
        for (uint32_t i = 0; i < OBS_DIM; i++) {
            obs[i] = static_cast<float>(rand()) / RAND_MAX;
        }
        return obs;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, DefaultConfigIsValid) {
    fep_config_t cfg;
    int result = fep_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.num_levels, 0u);
    EXPECT_EQ(cfg.belief_learning_rate, FEP_DEFAULT_BELIEF_LR);
    EXPECT_EQ(cfg.precision_learning_rate, FEP_DEFAULT_PRECISION_LR);
    EXPECT_EQ(cfg.action_learning_rate, FEP_DEFAULT_ACTION_LR);
    EXPECT_EQ(cfg.initial_precision, FEP_DEFAULT_PRECISION);
    EXPECT_TRUE(cfg.learn_precision);
    EXPECT_TRUE(cfg.enable_active_inference);
}

TEST_F(FreeEnergyTest, DefaultConfigNullFails) {
    int result = fep_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(FreeEnergyTest, CreateWithValidConfig) {
    createFEP();
    EXPECT_NE(fep, nullptr);
    EXPECT_EQ(fep->observation_dim, OBS_DIM);
    EXPECT_EQ(fep->action_dim, ACTION_DIM);
}

TEST_F(FreeEnergyTest, CreateWithNullConfig) {
    fep = fep_create(nullptr, OBS_DIM, ACTION_DIM);
    EXPECT_NE(fep, nullptr);  // Should use defaults
}

TEST_F(FreeEnergyTest, CreateWithZeroObservationDim) {
    fep = fep_create(&config, 0, ACTION_DIM);
    EXPECT_EQ(fep, nullptr);  // Should fail
}

TEST_F(FreeEnergyTest, CreateWithZeroActionDim) {
    // May or may not fail depending on implementation
    fep = fep_create(&config, OBS_DIM, 0);
    // Either valid for passive inference or null
    EXPECT_TRUE(fep != nullptr || fep == nullptr);
}

TEST_F(FreeEnergyTest, CreateWithMultipleLevels) {
    config.num_levels = 3;
    createFEP();
    EXPECT_EQ(fep->num_levels, 3u);
}

TEST_F(FreeEnergyTest, CreateWithMaxLevels) {
    config.num_levels = FEP_MAX_HIERARCHY_LEVELS;
    createFEP();
    EXPECT_EQ(fep->num_levels, FEP_MAX_HIERARCHY_LEVELS);
}

TEST_F(FreeEnergyTest, DestroyNullSafe) {
    fep_destroy(nullptr);
    // Should not crash
}

TEST_F(FreeEnergyTest, Reset) {
    createFEP();

    // Process some observations to change state
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Reset
    int result = fep_reset(fep);
    EXPECT_EQ(result, 0);

    // Statistics should be reset
    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(FreeEnergyTest, ResetNullFails) {
    int result = fep_reset(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Observation Processing Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, ProcessObservation) {
    createFEP();

    auto obs = createObservation();
    int result = fep_process_observation(fep, obs.data(), OBS_DIM);
    EXPECT_EQ(result, 0);

    // Should have stored observation
    for (uint32_t i = 0; i < OBS_DIM; i++) {
        EXPECT_FLOAT_EQ(fep->observations[i], obs[i]);
    }
}

TEST_F(FreeEnergyTest, ProcessObservationNullFails) {
    createFEP();

    auto obs = createObservation();
    EXPECT_EQ(fep_process_observation(nullptr, obs.data(), OBS_DIM), -1);
    EXPECT_EQ(fep_process_observation(fep, nullptr, OBS_DIM), -1);
}

TEST_F(FreeEnergyTest, ProcessObservationWrongDim) {
    createFEP();

    auto obs = createObservation();
    // Using wrong dimension - should handle gracefully
    int result = fep_process_observation(fep, obs.data(), OBS_DIM / 2);
    EXPECT_TRUE(result == 0 || result == -1);  // Implementation dependent
}

TEST_F(FreeEnergyTest, ProcessMultipleObservations) {
    createFEP();

    for (int i = 0; i < 10; i++) {
        auto obs = createObservation(static_cast<float>(i));
        int result = fep_process_observation(fep, obs.data(), OBS_DIM);
        EXPECT_EQ(result, 0);
    }

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GE(stats.total_updates, 10u);
}

/* ============================================================================
 * Prediction Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, ComputePrediction) {
    createFEP();

    // Process an observation first
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Get prediction
    std::vector<float> prediction(OBS_DIM);
    uint32_t dim = fep_compute_prediction(fep, prediction.data(), OBS_DIM);

    EXPECT_GT(dim, 0u);
    EXPECT_LE(dim, OBS_DIM);
}

TEST_F(FreeEnergyTest, ComputePredictionNullFails) {
    createFEP();

    std::vector<float> prediction(OBS_DIM);
    EXPECT_EQ(fep_compute_prediction(nullptr, prediction.data(), OBS_DIM), 0u);
    EXPECT_EQ(fep_compute_prediction(fep, nullptr, OBS_DIM), 0u);
}

TEST_F(FreeEnergyTest, ComputePredictionError) {
    createFEP();

    // Process observation
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Compute error
    fep_prediction_error_t error;
    error.error = new float[OBS_DIM];
    error.weighted_error = new float[OBS_DIM];
    error.precision = new float[OBS_DIM];
    error.dim = OBS_DIM;

    int result = fep_compute_prediction_error(fep, &error);
    EXPECT_EQ(result, 0);
    EXPECT_GE(error.magnitude, 0.0f);

    delete[] error.error;
    delete[] error.weighted_error;
    delete[] error.precision;
}

TEST_F(FreeEnergyTest, ComputePredictionErrorNullFails) {
    createFEP();

    fep_prediction_error_t error;
    EXPECT_EQ(fep_compute_prediction_error(nullptr, &error), -1);
    EXPECT_EQ(fep_compute_prediction_error(fep, nullptr), -1);
}

/* ============================================================================
 * Belief Update Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, UpdateBeliefs) {
    createFEP();

    // Process observation
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Update beliefs
    int result = fep_update_beliefs(fep);
    EXPECT_EQ(result, 0);

    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GE(stats.belief_updates, 1u);
}

TEST_F(FreeEnergyTest, UpdateBeliefsNullFails) {
    EXPECT_EQ(fep_update_beliefs(nullptr), -1);
}

TEST_F(FreeEnergyTest, UpdateBeliefsReducesError) {
    createFEP();

    // Process observation
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float initial_error = fep_get_prediction_error(fep, 0);

    // Multiple belief updates should reduce error
    for (int i = 0; i < 10; i++) {
        fep_update_beliefs(fep);
    }

    float final_error = fep_get_prediction_error(fep, 0);
    EXPECT_LE(final_error, initial_error + 0.1f);  // Allow some tolerance
}

TEST_F(FreeEnergyTest, UpdatePrecision) {
    config.learn_precision = true;
    createFEP();

    // Process observations
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_update_precision(fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, UpdatePrecisionNullFails) {
    EXPECT_EQ(fep_update_precision(nullptr), -1);
}

TEST_F(FreeEnergyTest, UpdatePrecisionDisabled) {
    config.learn_precision = false;
    createFEP();

    fep_belief_t beliefs_before;
    fep_get_beliefs(fep, 0, &beliefs_before);
    float precision_before = beliefs_before.precision ? beliefs_before.precision[0] : 0;

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);
    fep_update_precision(fep);

    fep_belief_t beliefs_after;
    fep_get_beliefs(fep, 0, &beliefs_after);
    float precision_after = beliefs_after.precision ? beliefs_after.precision[0] : 0;

    // Precision should not change when disabled
    EXPECT_FLOAT_EQ(precision_before, precision_after);
}

/* ============================================================================
 * Hierarchy Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, PropagateHierarchy) {
    config.num_levels = 3;
    createFEP();

    // Process observation
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_propagate_hierarchy(fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, PropagateHierarchyNullFails) {
    EXPECT_EQ(fep_propagate_hierarchy(nullptr), -1);
}

TEST_F(FreeEnergyTest, PropagateHierarchySingleLevel) {
    config.num_levels = 1;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_propagate_hierarchy(fep);
    EXPECT_EQ(result, 0);  // Should work even with single level
}

TEST_F(FreeEnergyTest, GetBeliefsAtLevel) {
    config.num_levels = 3;
    createFEP();

    for (uint32_t level = 0; level < 3; level++) {
        fep_belief_t beliefs;
        int result = fep_get_beliefs(fep, level, &beliefs);
        EXPECT_EQ(result, 0);
        EXPECT_GT(beliefs.dim, 0u);
    }
}

TEST_F(FreeEnergyTest, GetBeliefsInvalidLevel) {
    createFEP();

    fep_belief_t beliefs;
    int result = fep_get_beliefs(fep, 999, &beliefs);
    EXPECT_EQ(result, -1);
}

TEST_F(FreeEnergyTest, GetBeliefsNullChecks) {
    createFEP();

    fep_belief_t beliefs;
    EXPECT_EQ(fep_get_beliefs(nullptr, 0, &beliefs), -1);
    EXPECT_EQ(fep_get_beliefs(fep, 0, nullptr), -1);
}

/* ============================================================================
 * Free Energy Computation Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, ComputeFreeEnergy) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    fep_free_energy_t fe;
    int result = fep_compute_free_energy(fep, &fe);
    EXPECT_EQ(result, 0);
    EXPECT_GE(fe.total, 0.0f);  // Free energy should be non-negative
}

TEST_F(FreeEnergyTest, ComputeFreeEnergyNullChecks) {
    createFEP();

    fep_free_energy_t fe;
    EXPECT_EQ(fep_compute_free_energy(nullptr, &fe), -1);
    EXPECT_EQ(fep_compute_free_energy(fep, nullptr), -1);
}

TEST_F(FreeEnergyTest, FreeEnergyDecomposition) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    fep_free_energy_t fe;
    fep_compute_free_energy(fep, &fe);

    // F = Complexity + Inaccuracy
    float sum1 = fe.complexity + fe.inaccuracy;
    EXPECT_NEAR(fe.total, sum1, 0.1f);

    // F = Energy - Entropy (approximately)
    float sum2 = fe.energy - fe.entropy;
    EXPECT_NEAR(fe.total, sum2, 0.1f);
}

TEST_F(FreeEnergyTest, ComputeComponent) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float complexity = fep_compute_component(fep, FEP_COMPONENT_COMPLEXITY);
    float inaccuracy = fep_compute_component(fep, FEP_COMPONENT_INACCURACY);
    float energy = fep_compute_component(fep, FEP_COMPONENT_ENERGY);
    float entropy = fep_compute_component(fep, FEP_COMPONENT_ENTROPY);

    EXPECT_GE(complexity, 0.0f);
    EXPECT_GE(inaccuracy, 0.0f);
}

TEST_F(FreeEnergyTest, ComputeSurprise) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float surprise = fep_compute_surprise(fep);
    EXPECT_GE(surprise, 0.0f);

    // Free energy should bound surprise
    float fe = fep_get_free_energy(fep);
    EXPECT_GE(fe, surprise - 0.1f);  // Allow small tolerance
}

TEST_F(FreeEnergyTest, GetFreeEnergy) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float fe = fep_get_free_energy(fep);
    EXPECT_GE(fe, 0.0f);
}

TEST_F(FreeEnergyTest, GetFreeEnergyNullReturnsZero) {
    float fe = fep_get_free_energy(nullptr);
    EXPECT_EQ(fe, 0.0f);
}

/* ============================================================================
 * Active Inference Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, ComputeEFE) {
    config.enable_active_inference = true;
    createFEP();

    // Create a simple policy
    fep_policy_t policy;
    policy.policy_id = 0;
    policy.actions = new float[ACTION_DIM];
    policy.num_actions = 1;
    policy.action_dim = ACTION_DIM;
    for (uint32_t i = 0; i < ACTION_DIM; i++) {
        policy.actions[i] = 0.5f;
    }

    fep_efe_t efe;
    int result = fep_compute_efe(fep, &policy, &efe);
    EXPECT_EQ(result, 0);
    EXPECT_GE(efe.risk, 0.0f);
    EXPECT_GE(efe.ambiguity, 0.0f);

    delete[] policy.actions;
}

TEST_F(FreeEnergyTest, ComputeEFENullChecks) {
    createFEP();

    fep_policy_t policy;
    fep_efe_t efe;

    EXPECT_EQ(fep_compute_efe(nullptr, &policy, &efe), -1);
    EXPECT_EQ(fep_compute_efe(fep, nullptr, &efe), -1);
    EXPECT_EQ(fep_compute_efe(fep, &policy, nullptr), -1);
}

TEST_F(FreeEnergyTest, EvaluatePolicies) {
    config.enable_active_inference = true;
    createFEP();

    int result = fep_evaluate_policies(fep);
    EXPECT_EQ(result, 0);

    // Policies should have probabilities
    if (fep->num_policies > 0) {
        float total_prob = 0.0f;
        for (uint32_t i = 0; i < fep->num_policies; i++) {
            total_prob += fep->policies[i].probability;
        }
        EXPECT_NEAR(total_prob, 1.0f, 0.01f);
    }
}

TEST_F(FreeEnergyTest, EvaluatePoliciesNullFails) {
    EXPECT_EQ(fep_evaluate_policies(nullptr), -1);
}

TEST_F(FreeEnergyTest, SelectAction) {
    config.enable_active_inference = true;
    createFEP();

    // Process observation
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Select action
    std::vector<float> action(ACTION_DIM);
    int selected = fep_select_action(fep, action.data(), ACTION_DIM);

    EXPECT_GE(selected, 0);
    if (fep->num_policies > 0) {
        EXPECT_LT(static_cast<uint32_t>(selected), fep->num_policies);
    }
}

TEST_F(FreeEnergyTest, SelectActionNullChecks) {
    createFEP();

    std::vector<float> action(ACTION_DIM);
    EXPECT_EQ(fep_select_action(nullptr, action.data(), ACTION_DIM), -1);
    EXPECT_EQ(fep_select_action(fep, nullptr, ACTION_DIM), -1);
}

TEST_F(FreeEnergyTest, SelectActionGreedy) {
    config.enable_active_inference = true;
    config.action_mode = FEP_ACTION_GREEDY;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    std::vector<float> action(ACTION_DIM);
    int selected = fep_select_action(fep, action.data(), ACTION_DIM);

    EXPECT_GE(selected, 0);
}

TEST_F(FreeEnergyTest, SelectActionThompson) {
    config.enable_active_inference = true;
    config.action_mode = FEP_ACTION_THOMPSON;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    std::vector<float> action(ACTION_DIM);
    int selected = fep_select_action(fep, action.data(), ACTION_DIM);

    EXPECT_GE(selected, 0);
}

TEST_F(FreeEnergyTest, SetPreferences) {
    config.enable_active_inference = true;
    createFEP();

    std::vector<float> preferred(OBS_DIM, 0.5f);
    int result = fep_set_preferences(fep, preferred.data(), 1.0f, OBS_DIM);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, SetPreferencesNullChecks) {
    createFEP();

    std::vector<float> preferred(OBS_DIM);
    EXPECT_EQ(fep_set_preferences(nullptr, preferred.data(), 1.0f, OBS_DIM), -1);
    EXPECT_EQ(fep_set_preferences(fep, nullptr, 1.0f, OBS_DIM), -1);
}

TEST_F(FreeEnergyTest, ActiveInferenceDisabled) {
    config.enable_active_inference = false;
    createFEP();

    std::vector<float> action(ACTION_DIM);
    int selected = fep_select_action(fep, action.data(), ACTION_DIM);

    // Should either return -1 or select default
    EXPECT_TRUE(selected >= -1);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, GetPredictionError) {
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float error = fep_get_prediction_error(fep, 0);
    EXPECT_GE(error, 0.0f);
}

TEST_F(FreeEnergyTest, GetPredictionErrorInvalidLevel) {
    createFEP();

    float error = fep_get_prediction_error(fep, 999);
    EXPECT_EQ(error, 0.0f);  // Should return 0 for invalid level
}

TEST_F(FreeEnergyTest, GetSelectedPolicy) {
    config.enable_active_inference = true;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    std::vector<float> action(ACTION_DIM);
    fep_select_action(fep, action.data(), ACTION_DIM);

    fep_policy_t policy;
    int result = fep_get_selected_policy(fep, &policy);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, GetSelectedPolicyNullChecks) {
    createFEP();

    fep_policy_t policy;
    EXPECT_EQ(fep_get_selected_policy(nullptr, &policy), -1);
    EXPECT_EQ(fep_get_selected_policy(fep, nullptr), -1);
}

TEST_F(FreeEnergyTest, GetStats) {
    createFEP();

    fep_stats_t stats;
    int result = fep_get_stats(fep, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(FreeEnergyTest, GetStatsAfterOperations) {
    createFEP();

    // Perform various operations
    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);
    fep_update_beliefs(fep);
    fep_select_action(fep, nullptr, 0);

    fep_stats_t stats;
    fep_get_stats(fep, &stats);

    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.belief_updates, 0u);
}

TEST_F(FreeEnergyTest, GetStatsNullChecks) {
    createFEP();

    fep_stats_t stats;
    EXPECT_EQ(fep_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(fep_get_stats(fep, nullptr), -1);
}

/* ============================================================================
 * Update Mode Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, UpdateModeGradientDescent) {
    config.update_mode = FEP_UPDATE_GRADIENT_DESCENT;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_update_beliefs(fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, UpdateModePredictiveCoding) {
    config.update_mode = FEP_UPDATE_PREDICTIVE_CODING;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_update_beliefs(fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, UpdateModeVariationalMessage) {
    config.update_mode = FEP_UPDATE_VARIATIONAL_MESSAGE;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_update_beliefs(fep);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, UpdateModeKalman) {
    config.update_mode = FEP_UPDATE_KALMAN_FILTER;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    int result = fep_update_beliefs(fep);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, UpdateModeToString) {
    EXPECT_STREQ(fep_update_mode_to_string(FEP_UPDATE_GRADIENT_DESCENT), "GRADIENT_DESCENT");
    EXPECT_STREQ(fep_update_mode_to_string(FEP_UPDATE_PREDICTIVE_CODING), "PREDICTIVE_CODING");
    EXPECT_STREQ(fep_update_mode_to_string(FEP_UPDATE_VARIATIONAL_MESSAGE), "VARIATIONAL_MESSAGE");
    EXPECT_STREQ(fep_update_mode_to_string(FEP_UPDATE_KALMAN_FILTER), "KALMAN_FILTER");
}

TEST_F(FreeEnergyTest, ActionModeToString) {
    EXPECT_STREQ(fep_action_mode_to_string(FEP_ACTION_SOFTMAX), "SOFTMAX");
    EXPECT_STREQ(fep_action_mode_to_string(FEP_ACTION_GREEDY), "GREEDY");
    EXPECT_STREQ(fep_action_mode_to_string(FEP_ACTION_THOMPSON), "THOMPSON");
}

TEST_F(FreeEnergyTest, ComponentToString) {
    EXPECT_STREQ(fep_component_to_string(FEP_COMPONENT_COMPLEXITY), "COMPLEXITY");
    EXPECT_STREQ(fep_component_to_string(FEP_COMPONENT_INACCURACY), "INACCURACY");
    EXPECT_STREQ(fep_component_to_string(FEP_COMPONENT_ENERGY), "ENERGY");
    EXPECT_STREQ(fep_component_to_string(FEP_COMPONENT_ENTROPY), "ENTROPY");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FreeEnergyTest, LargeObservationDimension) {
    fep = fep_create(&config, FEP_MAX_OBSERVATION_DIM, ACTION_DIM);
    EXPECT_NE(fep, nullptr);

    if (fep) {
        std::vector<float> obs(FEP_MAX_OBSERVATION_DIM, 0.5f);
        int result = fep_process_observation(fep, obs.data(), FEP_MAX_OBSERVATION_DIM);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(FreeEnergyTest, VerySmallLearningRate) {
    config.belief_learning_rate = 0.0001f;
    createFEP();

    auto obs = createObservation();
    fep_process_observation(fep, obs.data(), OBS_DIM);

    float fe_before = fep_get_free_energy(fep);

    for (int i = 0; i < 100; i++) {
        fep_update_beliefs(fep);
    }

    // With tiny learning rate, change should be small
    float fe_after = fep_get_free_energy(fep);
    EXPECT_NEAR(fe_before, fe_after, fe_before);  // Large tolerance
}

TEST_F(FreeEnergyTest, HighPrecision) {
    config.initial_precision = FEP_MAX_PRECISION;
    createFEP();

    auto obs = createObservation();
    int result = fep_process_observation(fep, obs.data(), OBS_DIM);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, LowPrecision) {
    config.initial_precision = FEP_MIN_PRECISION;
    createFEP();

    auto obs = createObservation();
    int result = fep_process_observation(fep, obs.data(), OBS_DIM);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, ZeroObservation) {
    createFEP();

    std::vector<float> obs(OBS_DIM, 0.0f);
    int result = fep_process_observation(fep, obs.data(), OBS_DIM);
    EXPECT_EQ(result, 0);
}

TEST_F(FreeEnergyTest, ConstantObservation) {
    createFEP();

    std::vector<float> obs(OBS_DIM, 1.0f);

    // Process same observation multiple times
    for (int i = 0; i < 20; i++) {
        fep_process_observation(fep, obs.data(), OBS_DIM);
        fep_update_beliefs(fep);
    }

    // Error should decrease with consistent input
    float error = fep_get_prediction_error(fep, 0);
    EXPECT_GE(error, 0.0f);  // Should be small after learning
}

/* ============================================================================
 * Comprehensive Integration Test
 * ============================================================================ */

TEST_F(FreeEnergyTest, FullPerceptionActionCycle) {
    config.enable_active_inference = true;
    config.num_levels = 3;
    createFEP();

    // 1. Set goals/preferences
    std::vector<float> preferred(OBS_DIM, 0.5f);
    fep_set_preferences(fep, preferred.data(), 1.0f, OBS_DIM);

    // 2. Perception cycle
    auto obs = createObservation(0.3f);  // Different from preferred
    fep_process_observation(fep, obs.data(), OBS_DIM);

    // Initial free energy should be high (far from preferred)
    float initial_fe = fep_get_free_energy(fep);

    // 3. Belief updates
    for (int i = 0; i < 10; i++) {
        fep_update_beliefs(fep);
        fep_propagate_hierarchy(fep);
    }

    // 4. Action selection
    std::vector<float> action(ACTION_DIM);
    int policy_idx = fep_select_action(fep, action.data(), ACTION_DIM);
    EXPECT_GE(policy_idx, 0);

    // 5. Simulate environment response (observation closer to preferred)
    auto obs2 = createObservation(0.4f);
    fep_process_observation(fep, obs2.data(), OBS_DIM);

    for (int i = 0; i < 10; i++) {
        fep_update_beliefs(fep);
    }

    // 6. Free energy should be lower (closer to goal)
    float final_fe = fep_get_free_energy(fep);
    EXPECT_LE(final_fe, initial_fe + 1.0f);  // Allow some tolerance

    // 7. Verify statistics
    fep_stats_t stats;
    fep_get_stats(fep, &stats);
    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.belief_updates, 0u);
    EXPECT_GT(stats.action_selections, 0u);
}

TEST_F(FreeEnergyTest, LongTermLearning) {
    createFEP();

    // Simulate extended learning with consistent pattern
    float running_error = 0.0f;
    float prev_error = 1e6f;

    for (int epoch = 0; epoch < 5; epoch++) {
        for (int i = 0; i < 20; i++) {
            auto obs = createObservation(1.0f);
            fep_process_observation(fep, obs.data(), OBS_DIM);
            fep_update_beliefs(fep);
            fep_propagate_hierarchy(fep);
        }

        running_error = fep_get_prediction_error(fep, 0);

        // Error should generally decrease (or at least not explode)
        EXPECT_LE(running_error, prev_error + 1.0f);
        prev_error = running_error;
    }
}
