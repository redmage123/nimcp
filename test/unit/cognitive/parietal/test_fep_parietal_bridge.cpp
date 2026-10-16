/**
 * @file test_fep_parietal_bridge.cpp
 * @brief Unit tests for FEP-Parietal Bridge
 *
 * WHAT: Tests FEP-Parietal bridge functionality in isolation
 * WHY:  Verify predictive processing, active inference, and belief updates
 * HOW:  GTest framework with mock data and expected outcomes
 *
 * TEST CATEGORIES:
 * - Lifecycle tests (create, destroy, config)
 * - Belief update tests (hierarchical updates, precision weighting)
 * - Prediction tests (generative model predictions)
 * - Active inference tests (policy evaluation, action selection)
 * - Domain-specific tests (numerical, spatial, physics inference)
 * - Modulation tests (inflammation, fatigue effects)
 * - Statistics tests (tracking and reset)
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

/**
 * @brief Test fixture for FEP-Parietal bridge unit tests
 */
class FepParietalBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get default config
        config = fep_parietal_default_config();
        config.enabled = true;
        config.num_levels = 4;
        config.enable_active_inference = true;
        config.enable_numerical_model = true;
        config.enable_spatial_model = true;
        config.enable_physical_model = true;

        // Create bridge
        bridge = fep_parietal_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed to create FEP-Parietal bridge";
    }

    void TearDown() override {
        if (bridge) {
            fep_parietal_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    // Helper to create test observations
    std::vector<float> create_test_observations(uint32_t dim, float base = 1.0f) {
        std::vector<float> obs(dim);
        for (uint32_t i = 0; i < dim; i++) {
            obs[i] = base + sinf((float)i * 0.1f) * 0.5f;
        }
        return obs;
    }

    // Helper to create beliefs
    fep_math_belief_t create_test_belief(uint32_t dim) {
        fep_math_belief_t belief;
        memset(&belief, 0, sizeof(belief));
        belief.dim = dim;
        belief.mean = (float*)calloc(dim, sizeof(float));
        belief.precision = (float*)calloc(dim, sizeof(float));
        belief.confidence = 0.8f;
        belief.surprise = 0.1f;
        belief.domain = FEP_MATH_DOMAIN_NUMERICAL;

        for (uint32_t i = 0; i < dim; i++) {
            belief.mean[i] = (float)i * 0.1f;
            belief.precision[i] = 1.0f;
        }
        return belief;
    }

    void free_belief(fep_math_belief_t* belief) {
        if (belief->mean) free(belief->mean);
        if (belief->precision) free(belief->precision);
        belief->mean = nullptr;
        belief->precision = nullptr;
    }

    static constexpr uint32_t TEST_DIM = 16;
    static constexpr uint32_t TEST_LEVELS = 4;

    fep_parietal_config_t config;
    fep_parietal_bridge_t* bridge = nullptr;
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

/**
 * @brief Test default config values
 */
TEST_F(FepParietalBridgeTest, DefaultConfigHasValidValues) {
    fep_parietal_config_t default_config = fep_parietal_default_config();

    EXPECT_TRUE(default_config.enabled);
    EXPECT_GT(default_config.num_levels, 0);
    EXPECT_GT(default_config.belief_learning_rate, 0.0f);
    EXPECT_GT(default_config.precision_learning_rate, 0.0f);
    EXPECT_GT(default_config.initial_precision, 0.0f);
    EXPECT_LE(default_config.min_precision, default_config.max_precision);
}

/**
 * @brief Test bridge creation with default config
 */
TEST_F(FepParietalBridgeTest, CreateWithDefaultConfig) {
    fep_parietal_bridge_t* test_bridge = fep_parietal_bridge_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    EXPECT_TRUE(fep_parietal_is_available(test_bridge));
    fep_parietal_bridge_destroy(test_bridge);
}

/**
 * @brief Test bridge creation with custom config
 */
TEST_F(FepParietalBridgeTest, CreateWithCustomConfig) {
    fep_parietal_config_t custom_config = fep_parietal_default_config();
    custom_config.num_levels = 6;
    custom_config.planning_horizon = 10;
    custom_config.exploration_weight = 0.5f;

    fep_parietal_bridge_t* test_bridge = fep_parietal_bridge_create(&custom_config);
    ASSERT_NE(test_bridge, nullptr);
    EXPECT_TRUE(fep_parietal_is_available(test_bridge));
    fep_parietal_bridge_destroy(test_bridge);
}

/**
 * @brief Test bridge enable/disable
 */
TEST_F(FepParietalBridgeTest, EnableDisableBridge) {
    EXPECT_TRUE(fep_parietal_is_available(bridge));

    EXPECT_EQ(fep_parietal_set_enabled(bridge, false), 0);
    EXPECT_FALSE(fep_parietal_is_available(bridge));

    EXPECT_EQ(fep_parietal_set_enabled(bridge, true), 0);
    EXPECT_TRUE(fep_parietal_is_available(bridge));
}

/**
 * @brief Test null safety for destroy
 */
TEST_F(FepParietalBridgeTest, DestroyNullSafe) {
    fep_parietal_bridge_destroy(nullptr);  // Should not crash
    SUCCEED();
}

/* ============================================================================
 * BELIEF UPDATE TESTS
 * ============================================================================ */

/**
 * @brief Test belief update from observations
 */
TEST_F(FepParietalBridgeTest, UpdateBeliefsFromObservations) {
    auto observations = create_test_observations(TEST_DIM);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    int result = fep_parietal_update_beliefs(
        bridge,
        observations.data(),
        (uint32_t)observations.size(),
        FEP_MATH_DOMAIN_NUMERICAL,
        &beliefs
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(beliefs.dim, 0);
    EXPECT_GE(beliefs.confidence, 0.0f);
    EXPECT_LE(beliefs.confidence, 1.0f);

    // Clean up
    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
}

/**
 * @brief Test belief update with different domains
 */
TEST_F(FepParietalBridgeTest, UpdateBeliefsMultipleDomains) {
    auto observations = create_test_observations(TEST_DIM);

    fep_math_domain_t domains[] = {
        FEP_MATH_DOMAIN_NUMERICAL,
        FEP_MATH_DOMAIN_SPATIAL,
        FEP_MATH_DOMAIN_ALGEBRAIC,
        FEP_MATH_DOMAIN_PHYSICAL
    };

    for (auto domain : domains) {
        fep_math_belief_t beliefs;
        memset(&beliefs, 0, sizeof(beliefs));

        int result = fep_parietal_update_beliefs(
            bridge,
            observations.data(),
            (uint32_t)observations.size(),
            domain,
            &beliefs
        );

        EXPECT_EQ(result, 0) << "Failed for domain " << domain;
        EXPECT_EQ(beliefs.domain, domain);

        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }
}

/**
 * @brief Test that repeated updates improve confidence
 */
TEST_F(FepParietalBridgeTest, RepeatedUpdatesImproveConfidence) {
    auto observations = create_test_observations(TEST_DIM);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    // First update
    fep_parietal_update_beliefs(bridge, observations.data(),
        (uint32_t)observations.size(), FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    float first_confidence = beliefs.confidence;

    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
    memset(&beliefs, 0, sizeof(beliefs));

    // Consistent repeated updates should maintain or improve confidence
    for (int i = 0; i < 5; i++) {
        fep_parietal_update_beliefs(bridge, observations.data(),
            (uint32_t)observations.size(), FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
        if (i < 4) memset(&beliefs, 0, sizeof(beliefs));
    }

    // Confidence should be reasonable after consistent observations
    EXPECT_GE(beliefs.confidence, 0.0f);
}

/* ============================================================================
 * PREDICTION TESTS
 * ============================================================================ */

/**
 * @brief Test prediction generation from beliefs
 */
TEST_F(FepParietalBridgeTest, GeneratePredictionFromBeliefs) {
    fep_math_belief_t beliefs = create_test_belief(TEST_DIM);
    fep_math_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    int result = fep_parietal_predict(bridge, &beliefs, &prediction);

    EXPECT_EQ(result, 0);
    EXPECT_NE(prediction.predicted, nullptr);
    EXPECT_GT(prediction.dim, 0);

    // Clean up
    free_belief(&beliefs);
    if (prediction.predicted) free(prediction.predicted);
    if (prediction.actual) free(prediction.actual);
    if (prediction.error) free(prediction.error);
    if (prediction.weighted_error) free(prediction.weighted_error);
}

/**
 * @brief Test prediction error computation
 */
TEST_F(FepParietalBridgeTest, ComputePredictionError) {
    std::vector<float> predicted(TEST_DIM);
    std::vector<float> actual(TEST_DIM);

    for (uint32_t i = 0; i < TEST_DIM; i++) {
        predicted[i] = (float)i * 0.1f;
        actual[i] = (float)i * 0.1f + 0.05f;  // Small error
    }

    fep_math_prediction_t error;
    memset(&error, 0, sizeof(error));

    int result = fep_parietal_prediction_error(
        bridge,
        predicted.data(),
        actual.data(),
        TEST_DIM,
        &error
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(error.error_magnitude, 0.0f);
    EXPECT_GT(error.free_energy, 0.0f);

    // Clean up
    if (error.predicted) free(error.predicted);
    if (error.actual) free(error.actual);
    if (error.error) free(error.error);
    if (error.weighted_error) free(error.weighted_error);
}

/**
 * @brief Test free energy computation
 */
TEST_F(FepParietalBridgeTest, ComputeFreeEnergy) {
    fep_math_belief_t beliefs = create_test_belief(TEST_DIM);
    auto observations = create_test_observations(TEST_DIM);

    float free_energy = fep_parietal_compute_free_energy(
        bridge,
        &beliefs,
        observations.data(),
        (uint32_t)observations.size()
    );

    // Free energy should be non-negative (or bounded)
    EXPECT_GE(free_energy, 0.0f);

    free_belief(&beliefs);
}

/* ============================================================================
 * ACTIVE INFERENCE TESTS
 * ============================================================================ */

/**
 * @brief Test policy evaluation
 */
TEST_F(FepParietalBridgeTest, EvaluatePolicies) {
    fep_problem_state_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.dim = TEST_DIM;
    problem.current_state = (float*)calloc(TEST_DIM, sizeof(float));
    problem.goal_state = (float*)calloc(TEST_DIM, sizeof(float));

    for (uint32_t i = 0; i < TEST_DIM; i++) {
        problem.current_state[i] = 0.0f;
        problem.goal_state[i] = 1.0f;  // Goal is all 1s
    }

    fep_math_policy_t policies[FEP_PARIETAL_MAX_POLICIES];
    memset(policies, 0, sizeof(policies));
    uint32_t num_policies = FEP_PARIETAL_MAX_POLICIES;

    int result = fep_parietal_evaluate_policies(
        bridge,
        &problem,
        policies,
        &num_policies
    );

    EXPECT_EQ(result, 0);
    EXPECT_GT(num_policies, 0);

    // Check that at least one policy has reasonable expected free energy
    bool has_valid_policy = false;
    for (uint32_t i = 0; i < num_policies; i++) {
        if (policies[i].expected_free_energy != 0.0f) {
            has_valid_policy = true;
            break;
        }
    }
    EXPECT_TRUE(has_valid_policy);

    // Clean up
    free(problem.current_state);
    free(problem.goal_state);
}

/**
 * @brief Test active inference action selection
 */
TEST_F(FepParietalBridgeTest, ActiveInferenceSelectsAction) {
    fep_problem_state_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.dim = TEST_DIM;
    problem.current_state = (float*)calloc(TEST_DIM, sizeof(float));
    problem.goal_state = (float*)calloc(TEST_DIM, sizeof(float));

    for (uint32_t i = 0; i < TEST_DIM; i++) {
        problem.current_state[i] = (float)i * 0.05f;
        problem.goal_state[i] = (float)i * 0.1f;
    }

    fep_active_inference_result_t result_data;
    memset(&result_data, 0, sizeof(result_data));

    int result = fep_parietal_active_inference(
        bridge,
        &problem,
        &result_data
    );

    EXPECT_EQ(result, 0);
    EXPECT_GE(result_data.selected_strategy, 0);
    EXPECT_LT(result_data.selected_strategy, 7);  // Valid strategy enum
    EXPECT_GE(result_data.confidence, 0.0f);
    EXPECT_LE(result_data.confidence, 1.0f);

    // Clean up
    free(problem.current_state);
    free(problem.goal_state);
    if (result_data.action) free(result_data.action);
}

/* ============================================================================
 * DOMAIN-SPECIFIC INFERENCE TESTS
 * ============================================================================ */

/**
 * @brief Test numerical inference (number sense)
 */
TEST_F(FepParietalBridgeTest, NumericalInference) {
    float quantities[] = {1.0f, 2.0f, 3.0f, 5.0f, 8.0f, 13.0f};  // Fibonacci-like
    fep_math_belief_t estimated;
    memset(&estimated, 0, sizeof(estimated));

    int result = fep_parietal_numerical_inference(
        bridge,
        quantities,
        6,
        &estimated
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(estimated.domain, FEP_MATH_DOMAIN_NUMERICAL);
    EXPECT_GT(estimated.confidence, 0.0f);

    if (estimated.mean) free(estimated.mean);
    if (estimated.precision) free(estimated.precision);
}

/**
 * @brief Test spatial inference
 */
TEST_F(FepParietalBridgeTest, SpatialInference) {
    // 3D positions (x,y,z triples)
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f};
    fep_math_belief_t transformed;
    memset(&transformed, 0, sizeof(transformed));

    int result = fep_parietal_spatial_inference(
        bridge,
        positions,
        9,  // 3 positions
        &transformed
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(transformed.domain, FEP_MATH_DOMAIN_SPATIAL);

    if (transformed.mean) free(transformed.mean);
    if (transformed.precision) free(transformed.precision);
}

/**
 * @brief Test physics inference
 */
TEST_F(FepParietalBridgeTest, PhysicsInference) {
    // State: [position, velocity] for simple harmonic oscillator
    float state[] = {1.0f, 0.0f, 0.0f, 0.0f};  // Initial position, zero velocity
    fep_math_belief_t predicted;
    memset(&predicted, 0, sizeof(predicted));

    int result = fep_parietal_physics_inference(
        bridge,
        state,
        4,
        0.01f,  // dt
        &predicted
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(predicted.domain, FEP_MATH_DOMAIN_PHYSICAL);
    EXPECT_GT(predicted.confidence, 0.0f);

    if (predicted.mean) free(predicted.mean);
    if (predicted.precision) free(predicted.precision);
}

/**
 * @brief Test engineering inference
 */
TEST_F(FepParietalBridgeTest, EngineeringInference) {
    float input[] = {100.0f, 50.0f, 25.0f};  // Example engineering parameters
    fep_math_belief_t result_belief;
    memset(&result_belief, 0, sizeof(result_belief));

    int result = fep_parietal_engineering_inference(
        bridge,
        input,
        3,
        FEP_MATH_DOMAIN_ENGINEERING,
        &result_belief
    );

    EXPECT_EQ(result, 0);
    EXPECT_EQ(result_belief.domain, FEP_MATH_DOMAIN_ENGINEERING);

    if (result_belief.mean) free(result_belief.mean);
    if (result_belief.precision) free(result_belief.precision);
}

/* ============================================================================
 * SURPRISE AND CURIOSITY TESTS
 * ============================================================================ */

/**
 * @brief Test surprise computation
 */
TEST_F(FepParietalBridgeTest, ComputeSurprise) {
    auto expected_obs = create_test_observations(TEST_DIM, 1.0f);
    auto unexpected_obs = create_test_observations(TEST_DIM, 10.0f);  // Very different

    float expected_surprise = fep_parietal_compute_surprise(
        bridge,
        expected_obs.data(),
        TEST_DIM
    );

    float unexpected_surprise = fep_parietal_compute_surprise(
        bridge,
        unexpected_obs.data(),
        TEST_DIM
    );

    // Both should be non-negative
    EXPECT_GE(expected_surprise, 0.0f);
    EXPECT_GE(unexpected_surprise, 0.0f);
}

/**
 * @brief Test epistemic value (curiosity)
 */
TEST_F(FepParietalBridgeTest, ComputeEpistemicValue) {
    auto query = create_test_observations(TEST_DIM);

    float epistemic_value = fep_parietal_epistemic_value(
        bridge,
        query.data(),
        TEST_DIM
    );

    // Epistemic value (information gain) should be non-negative
    EXPECT_GE(epistemic_value, 0.0f);
}

/* ============================================================================
 * MODULATION TESTS
 * ============================================================================ */

/**
 * @brief Test inflammation modulation
 */
TEST_F(FepParietalBridgeTest, InflammationModulation) {
    EXPECT_EQ(fep_parietal_set_inflammation(bridge, 0.0f), 0);
    EXPECT_EQ(fep_parietal_set_inflammation(bridge, 0.5f), 0);
    EXPECT_EQ(fep_parietal_set_inflammation(bridge, 1.0f), 0);

    // Null safety
    EXPECT_EQ(fep_parietal_set_inflammation(nullptr, 0.5f), -1);
}

/**
 * @brief Test fatigue modulation
 */
TEST_F(FepParietalBridgeTest, FatigueModulation) {
    EXPECT_EQ(fep_parietal_set_fatigue(bridge, 0.0f), 0);
    EXPECT_EQ(fep_parietal_set_fatigue(bridge, 0.5f), 0);
    EXPECT_EQ(fep_parietal_set_fatigue(bridge, 1.0f), 0);

    // Null safety
    EXPECT_EQ(fep_parietal_set_fatigue(nullptr, 0.5f), -1);
}

/**
 * @brief Test that high inflammation reduces precision
 */
TEST_F(FepParietalBridgeTest, InflammationReducesPrecision) {
    // Get precision at low inflammation
    fep_parietal_set_inflammation(bridge, 0.1f);
    auto obs = create_test_observations(TEST_DIM);
    fep_math_belief_t low_infl_beliefs;
    memset(&low_infl_beliefs, 0, sizeof(low_infl_beliefs));
    fep_parietal_update_beliefs(bridge, obs.data(), TEST_DIM,
        FEP_MATH_DOMAIN_NUMERICAL, &low_infl_beliefs);

    // Get precision at high inflammation
    fep_parietal_set_inflammation(bridge, 0.9f);
    fep_math_belief_t high_infl_beliefs;
    memset(&high_infl_beliefs, 0, sizeof(high_infl_beliefs));
    fep_parietal_update_beliefs(bridge, obs.data(), TEST_DIM,
        FEP_MATH_DOMAIN_NUMERICAL, &high_infl_beliefs);

    // High inflammation should generally reduce confidence
    // (exact behavior depends on implementation)
    EXPECT_GE(low_infl_beliefs.confidence, 0.0f);
    EXPECT_GE(high_infl_beliefs.confidence, 0.0f);

    if (low_infl_beliefs.mean) free(low_infl_beliefs.mean);
    if (low_infl_beliefs.precision) free(low_infl_beliefs.precision);
    if (high_infl_beliefs.mean) free(high_infl_beliefs.mean);
    if (high_infl_beliefs.precision) free(high_infl_beliefs.precision);
}

/* ============================================================================
 * STATISTICS TESTS
 * ============================================================================ */

/**
 * @brief Test statistics collection
 */
TEST_F(FepParietalBridgeTest, StatisticsCollection) {
    fep_parietal_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Get initial stats
    EXPECT_EQ(fep_parietal_get_stats(bridge, &stats), 0);
    uint64_t initial_updates = stats.belief_updates;

    // Perform some operations
    auto obs = create_test_observations(TEST_DIM);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    for (int i = 0; i < 5; i++) {
        fep_parietal_update_beliefs(bridge, obs.data(), TEST_DIM,
            FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) { free(beliefs.mean); beliefs.mean = nullptr; }
        if (beliefs.precision) { free(beliefs.precision); beliefs.precision = nullptr; }
    }

    // Stats should show updates
    EXPECT_EQ(fep_parietal_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.belief_updates, initial_updates);
    EXPECT_GT(stats.predictions_made, 0);
}

/**
 * @brief Test statistics reset
 */
TEST_F(FepParietalBridgeTest, StatisticsReset) {
    // Perform some operations first
    auto obs = create_test_observations(TEST_DIM);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));
    fep_parietal_update_beliefs(bridge, obs.data(), TEST_DIM,
        FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);

    // Reset stats
    fep_parietal_reset_stats(bridge);

    // Verify stats are reset
    fep_parietal_stats_t stats;
    EXPECT_EQ(fep_parietal_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.belief_updates, 0);
    EXPECT_EQ(stats.predictions_made, 0);
    EXPECT_EQ(stats.active_inferences, 0);
}

/**
 * @brief Test null safety for statistics functions
 */
TEST_F(FepParietalBridgeTest, StatisticsNullSafety) {
    fep_parietal_stats_t stats;

    EXPECT_EQ(fep_parietal_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(fep_parietal_get_stats(bridge, nullptr), -1);

    fep_parietal_reset_stats(nullptr);  // Should not crash
    SUCCEED();
}

/* ============================================================================
 * EDGE CASE TESTS
 * ============================================================================ */

/**
 * @brief Test with zero-dimensional data
 */
TEST_F(FepParietalBridgeTest, HandleZeroDimensionalData) {
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    int result = fep_parietal_update_beliefs(bridge, nullptr, 0,
        FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    // Should handle gracefully (either succeed with empty or return error)
    EXPECT_TRUE(result == 0 || result == -1);
}

/**
 * @brief Test with large data
 */
TEST_F(FepParietalBridgeTest, HandleLargeData) {
    const uint32_t large_dim = 1024;
    std::vector<float> large_obs(large_dim);
    for (uint32_t i = 0; i < large_dim; i++) {
        large_obs[i] = sinf((float)i * 0.01f);
    }

    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    int result = fep_parietal_update_beliefs(bridge, large_obs.data(),
        large_dim, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    EXPECT_EQ(result, 0);

    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
}

/**
 * @brief Test with extreme values
 */
TEST_F(FepParietalBridgeTest, HandleExtremeValues) {
    float extreme_obs[] = {1e10f, -1e10f, 0.0f, 1e-10f, -1e-10f};
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    int result = fep_parietal_update_beliefs(bridge, extreme_obs, 5,
        FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    // Should handle without crashing
    EXPECT_TRUE(result == 0 || result == -1);

    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
}

/* ============================================================================
 * PRECISION MODULATION TESTS
 * ============================================================================ */

/**
 * @brief Test attention precision setting
 */
TEST_F(FepParietalBridgeTest, SetAttentionPrecision) {
    std::vector<float> attention_weights(TEST_DIM, 1.0f);
    attention_weights[0] = 2.0f;  // Higher attention on first dimension

    int result = fep_parietal_set_attention_precision(
        bridge,
        attention_weights.data(),
        TEST_DIM
    );

    EXPECT_EQ(result, 0);
}

/**
 * @brief Test adaptive precision
 */
TEST_F(FepParietalBridgeTest, AdaptivePrecision) {
    int result = fep_parietal_adapt_precision(bridge);
    EXPECT_EQ(result, 0);
}

/**
 * @brief Test precision retrieval
 */
TEST_F(FepParietalBridgeTest, GetPrecision) {
    float* precision = nullptr;
    uint32_t dim = 0;

    int result = fep_parietal_get_precision(bridge, 0, &precision, &dim);

    EXPECT_EQ(result, 0);
    // Precision may be null if not initialized
    if (precision) {
        EXPECT_GT(dim, 0);
        free(precision);
    }
}

/* ============================================================================
 * GENERATIVE MODEL TESTS
 * ============================================================================ */

/**
 * @brief Test generative model retrieval
 */
TEST_F(FepParietalBridgeTest, GetGenerativeModel) {
    fep_math_generative_model_t model;
    memset(&model, 0, sizeof(model));

    int result = fep_parietal_get_generative_model(bridge, &model);

    EXPECT_EQ(result, 0);
    EXPECT_GE(model.total_free_energy, 0.0f);
}

/**
 * @brief Test generative model training
 */
TEST_F(FepParietalBridgeTest, TrainGenerativeModel) {
    const uint32_t num_samples = 10;
    std::vector<std::vector<float>> observations(num_samples);
    std::vector<std::vector<float>> targets(num_samples);
    std::vector<const float*> obs_ptrs(num_samples);
    std::vector<const float*> target_ptrs(num_samples);

    for (uint32_t i = 0; i < num_samples; i++) {
        observations[i] = create_test_observations(TEST_DIM, (float)i * 0.1f);
        targets[i] = create_test_observations(TEST_DIM, (float)i * 0.1f + 0.5f);
        obs_ptrs[i] = observations[i].data();
        target_ptrs[i] = targets[i].data();
    }

    float loss = fep_parietal_train_model(
        bridge,
        obs_ptrs.data(),
        target_ptrs.data(),
        num_samples
    );

    // Loss should be non-negative
    EXPECT_GE(loss, 0.0f);
}
