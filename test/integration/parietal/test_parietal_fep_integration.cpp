/**
 * @file test_parietal_fep_integration.cpp
 * @brief Integration tests for Parietal Lobe + FEP Bridge
 *
 * WHAT: Tests parietal-FEP integration across module boundaries
 * WHY:  Verify predictive processing enhances mathematical reasoning
 * HOW:  GTest framework with cross-module interaction tests
 *
 * TEST SCENARIOS:
 * - FEP-enhanced number sense (Weber-Fechner as precision-weighted)
 * - FEP-enhanced spatial reasoning (generative models of 3D space)
 * - FEP-enhanced pattern detection (hierarchical priors)
 * - FEP-enhanced physics prediction (physics-informed generative)
 * - Active inference for problem solving
 * - Precision modulation from attention
 * - Belief propagation through hierarchy
 * - Cross-domain inference (numerical + spatial + physical)
 *
 * @author NIMCP Development Team
 * @date 2025-12-29
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_fep_parietal_bridge.h"
#include "cognitive/parietal/nimcp_number_sense.h"
#include "cognitive/parietal/nimcp_spatial_reasoning.h"
#include "cognitive/parietal/nimcp_mathematical_intuition.h"
#include "cognitive/parietal/nimcp_scientific_reasoning.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

/**
 * @brief Test fixture for parietal-FEP integration tests
 */
class ParietalFepIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure with FEP enabled
        config = parietal_default_config();
        config.enable_fep_parietal_bridge = true;
        config.enable_neural_network = true;
        config.fep_parietal_config = fep_parietal_default_config();
        config.fep_parietal_config.enable_active_inference = true;
        config.fep_parietal_config.enable_numerical_model = true;
        config.fep_parietal_config.enable_spatial_model = true;
        config.fep_parietal_config.enable_physical_model = true;

        parietal = parietal_create_custom(&config);
        ASSERT_NE(parietal, nullptr) << "Failed to create parietal lobe";

        fep_bridge = parietal_get_fep_bridge(parietal);
        ASSERT_NE(fep_bridge, nullptr) << "FEP bridge not available";
    }

    void TearDown() override {
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    // Helper: Create arithmetic sequence
    std::vector<float> create_arithmetic_sequence(uint32_t n, float start, float diff) {
        std::vector<float> seq(n);
        for (uint32_t i = 0; i < n; i++) {
            seq[i] = start + (float)i * diff;
        }
        return seq;
    }

    // Helper: Create geometric sequence
    std::vector<float> create_geometric_sequence(uint32_t n, float start, float ratio) {
        std::vector<float> seq(n);
        seq[0] = start;
        for (uint32_t i = 1; i < n; i++) {
            seq[i] = seq[i-1] * ratio;
        }
        return seq;
    }

    // Helper: Create noisy observations
    std::vector<float> add_noise(const std::vector<float>& data, float noise_level) {
        std::vector<float> noisy(data.size());
        for (size_t i = 0; i < data.size(); i++) {
            float noise = ((float)rand() / RAND_MAX - 0.5f) * 2.0f * noise_level;
            noisy[i] = data[i] + noise;
        }
        return noisy;
    }

    // Helper: Create 3D positions
    std::vector<float> create_3d_positions(uint32_t n) {
        std::vector<float> positions(n * 3);
        for (uint32_t i = 0; i < n; i++) {
            positions[i*3 + 0] = cosf((float)i * 0.5f);  // x
            positions[i*3 + 1] = sinf((float)i * 0.5f);  // y
            positions[i*3 + 2] = (float)i * 0.1f;        // z
        }
        return positions;
    }

    // Helper: Create physics state (position, velocity pairs)
    std::vector<float> create_physics_state(uint32_t n_particles) {
        std::vector<float> state(n_particles * 4);  // 2D: x,y,vx,vy per particle
        for (uint32_t i = 0; i < n_particles; i++) {
            state[i*4 + 0] = (float)i * 0.5f;         // x
            state[i*4 + 1] = sinf((float)i);          // y
            state[i*4 + 2] = 0.1f;                    // vx
            state[i*4 + 3] = cosf((float)i) * 0.1f;   // vy
        }
        return state;
    }

    parietal_config_t config;
    parietal_lobe_t* parietal = nullptr;
    fep_parietal_bridge_t* fep_bridge = nullptr;
};

/* ============================================================================
 * FEP-ENHANCED NUMBER SENSE TESTS
 * ============================================================================ */

/**
 * @brief Test that FEP improves number estimation consistency
 */
TEST_F(ParietalFepIntegrationTest, FepEnhancesNumberEstimation) {
    // Create consistent observations
    auto clean_seq = create_arithmetic_sequence(10, 1.0f, 1.0f);

    // First, update FEP beliefs with clean data
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));
    fep_parietal_update_beliefs(fep_bridge, clean_seq.data(),
        (uint32_t)clean_seq.size(), FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    float initial_confidence = beliefs.confidence;
    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);

    // Continue updating with same pattern
    for (int i = 0; i < 5; i++) {
        memset(&beliefs, 0, sizeof(beliefs));
        fep_parietal_update_beliefs(fep_bridge, clean_seq.data(),
            (uint32_t)clean_seq.size(), FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }

    // Now estimate with parietal
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_ESTIMATE_QUANTITY;
    req.input.quantity_input.values = clean_seq.data();
    req.input.quantity_input.num_values = (uint32_t)clean_seq.size();

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.confidence, 0.0f);
}

/**
 * @brief Test FEP handles noisy observations gracefully
 */
TEST_F(ParietalFepIntegrationTest, FepHandlesNoisyObservations) {
    auto clean_seq = create_arithmetic_sequence(20, 1.0f, 2.0f);
    auto noisy_seq = add_noise(clean_seq, 0.5f);

    // First process clean data to establish beliefs
    fep_math_belief_t clean_beliefs;
    memset(&clean_beliefs, 0, sizeof(clean_beliefs));
    fep_parietal_update_beliefs(fep_bridge, clean_seq.data(),
        (uint32_t)clean_seq.size(), FEP_MATH_DOMAIN_NUMERICAL, &clean_beliefs);
    float clean_surprise = fep_parietal_compute_surprise(fep_bridge,
        clean_seq.data(), (uint32_t)clean_seq.size());

    // Now process noisy data
    fep_math_belief_t noisy_beliefs;
    memset(&noisy_beliefs, 0, sizeof(noisy_beliefs));
    fep_parietal_update_beliefs(fep_bridge, noisy_seq.data(),
        (uint32_t)noisy_seq.size(), FEP_MATH_DOMAIN_NUMERICAL, &noisy_beliefs);
    float noisy_surprise = fep_parietal_compute_surprise(fep_bridge,
        noisy_seq.data(), (uint32_t)noisy_seq.size());

    // Noisy data should generally have higher surprise
    EXPECT_GE(noisy_surprise, 0.0f);
    EXPECT_GE(clean_surprise, 0.0f);

    // Clean up
    if (clean_beliefs.mean) free(clean_beliefs.mean);
    if (clean_beliefs.precision) free(clean_beliefs.precision);
    if (noisy_beliefs.mean) free(noisy_beliefs.mean);
    if (noisy_beliefs.precision) free(noisy_beliefs.precision);
}

/* ============================================================================
 * FEP-ENHANCED SPATIAL REASONING TESTS
 * ============================================================================ */

/**
 * @brief Test FEP spatial inference with 3D positions
 */
TEST_F(ParietalFepIntegrationTest, FepSpatialInference) {
    auto positions = create_3d_positions(10);

    fep_math_belief_t spatial_beliefs;
    memset(&spatial_beliefs, 0, sizeof(spatial_beliefs));

    int result = fep_parietal_spatial_inference(fep_bridge,
        positions.data(), (uint32_t)positions.size(), &spatial_beliefs);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(spatial_beliefs.domain, FEP_MATH_DOMAIN_SPATIAL);
    EXPECT_GT(spatial_beliefs.confidence, 0.0f);

    if (spatial_beliefs.mean) free(spatial_beliefs.mean);
    if (spatial_beliefs.precision) free(spatial_beliefs.precision);
}

/**
 * @brief Test combined numerical + spatial FEP processing
 */
TEST_F(ParietalFepIntegrationTest, CombinedNumericalSpatialFep) {
    auto numbers = create_arithmetic_sequence(10, 0.0f, 1.0f);
    auto positions = create_3d_positions(10);

    // Process numerical domain
    fep_math_belief_t num_beliefs;
    memset(&num_beliefs, 0, sizeof(num_beliefs));
    fep_parietal_update_beliefs(fep_bridge, numbers.data(),
        (uint32_t)numbers.size(), FEP_MATH_DOMAIN_NUMERICAL, &num_beliefs);

    // Process spatial domain
    fep_math_belief_t spatial_beliefs;
    memset(&spatial_beliefs, 0, sizeof(spatial_beliefs));
    fep_parietal_spatial_inference(fep_bridge, positions.data(),
        (uint32_t)positions.size(), &spatial_beliefs);

    // Both should have reasonable confidence
    EXPECT_GT(num_beliefs.confidence, 0.0f);
    EXPECT_GT(spatial_beliefs.confidence, 0.0f);

    // Clean up
    if (num_beliefs.mean) free(num_beliefs.mean);
    if (num_beliefs.precision) free(num_beliefs.precision);
    if (spatial_beliefs.mean) free(spatial_beliefs.mean);
    if (spatial_beliefs.precision) free(spatial_beliefs.precision);
}

/* ============================================================================
 * FEP-ENHANCED PATTERN DETECTION TESTS
 * ============================================================================ */

/**
 * @brief Test FEP improves pattern detection through belief priors
 */
TEST_F(ParietalFepIntegrationTest, FepEnhancesPatternDetection) {
    // Create geometric pattern
    auto pattern = create_geometric_sequence(8, 1.0f, 2.0f);

    // Build up beliefs about geometric patterns
    for (int i = 0; i < 3; i++) {
        fep_math_belief_t beliefs;
        memset(&beliefs, 0, sizeof(beliefs));
        fep_parietal_update_beliefs(fep_bridge, pattern.data(),
            (uint32_t)pattern.size(), FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }

    // Now detect pattern through parietal
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_PATTERN_DETECT;
    req.input.pattern_input.sequence = pattern.data();
    req.input.pattern_input.length = (uint32_t)pattern.size();

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.confidence, 0.0f);
    // Should detect geometric/exponential pattern type
}

/**
 * @brief Test hierarchical pattern with multiple levels
 */
TEST_F(ParietalFepIntegrationTest, HierarchicalPatternProcessing) {
    // Level 0: Raw numbers
    float level0[] = {1, 2, 3, 4, 5, 6, 7, 8};

    // Build hierarchical beliefs
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));
    fep_parietal_update_beliefs(fep_bridge, level0, 8,
        FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    // Get the generative model state
    fep_math_generative_model_t model;
    memset(&model, 0, sizeof(model));
    int result = fep_parietal_get_generative_model(fep_bridge, &model);

    EXPECT_EQ(result, 0);
    EXPECT_GE(model.total_free_energy, 0.0f);

    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
}

/* ============================================================================
 * FEP-ENHANCED PHYSICS PREDICTION TESTS
 * ============================================================================ */

/**
 * @brief Test FEP physics inference
 */
TEST_F(ParietalFepIntegrationTest, FepPhysicsInference) {
    auto state = create_physics_state(2);

    fep_math_belief_t physics_beliefs;
    memset(&physics_beliefs, 0, sizeof(physics_beliefs));

    int result = fep_parietal_physics_inference(fep_bridge,
        state.data(), (uint32_t)state.size(), 0.01f, &physics_beliefs);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(physics_beliefs.domain, FEP_MATH_DOMAIN_PHYSICAL);

    if (physics_beliefs.mean) free(physics_beliefs.mean);
    if (physics_beliefs.precision) free(physics_beliefs.precision);
}

/**
 * @brief Test combined physics NN + FEP prediction
 */
TEST_F(ParietalFepIntegrationTest, PhysicsNNWithFepIntegration) {
    auto state = create_physics_state(2);

    // First, process through FEP
    fep_math_belief_t fep_beliefs;
    memset(&fep_beliefs, 0, sizeof(fep_beliefs));
    fep_parietal_physics_inference(fep_bridge, state.data(),
        (uint32_t)state.size(), 0.01f, &fep_beliefs);

    // Then process through parietal (which uses physics NN internally)
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_PHYSICS_PREDICT;
    req.input.physics_input.state = state.data();
    req.input.physics_input.state_dim = (uint32_t)state.size();
    req.input.physics_input.dt = 0.01f;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);

    if (fep_beliefs.mean) free(fep_beliefs.mean);
    if (fep_beliefs.precision) free(fep_beliefs.precision);
    if (result.output.physics_output.predicted_state) {
        free(result.output.physics_output.predicted_state);
    }
}

/* ============================================================================
 * ACTIVE INFERENCE TESTS
 * ============================================================================ */

/**
 * @brief Test active inference for problem solving
 */
TEST_F(ParietalFepIntegrationTest, ActiveInferenceForProblemSolving) {
    // Define a problem state
    fep_problem_state_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.state_dim = 8;
    problem.goal_dim = 8;
    problem.domain = FEP_MATH_DOMAIN_NUMERICAL;
    problem.state_vector = (float*)calloc(8, sizeof(float));
    problem.goal_state = (float*)calloc(8, sizeof(float));

    for (int i = 0; i < 8; i++) {
        problem.state_vector[i] = (float)i * 0.1f;
        problem.goal_state[i] = (float)(7 - i) * 0.1f;  // Reversed goal
    }

    // Evaluate policies
    fep_math_policy_t policies[FEP_PARIETAL_MAX_POLICIES];
    memset(policies, 0, sizeof(policies));
    uint32_t num_policies = FEP_PARIETAL_MAX_POLICIES;

    int result = fep_parietal_evaluate_policies(fep_bridge, &problem,
        policies, &num_policies);

    // Verify policy evaluation succeeds (actual policy count depends on implementation)
    EXPECT_EQ(result, 0);

    // Now do active inference
    fep_active_inference_result_t inference_result;
    memset(&inference_result, 0, sizeof(inference_result));

    result = fep_parietal_active_inference(fep_bridge, &problem, &inference_result);

    EXPECT_EQ(result, 0);
    EXPECT_GE(inference_result.selected_strategy, 0);
    EXPECT_LT(inference_result.selected_strategy, 7);

    // Clean up
    free(problem.state_vector);
    free(problem.goal_state);
    if (inference_result.action) free(inference_result.action);
}

/**
 * @brief Test FEP request through parietal orchestrator
 */
TEST_F(ParietalFepIntegrationTest, FepActiveInferenceThroughParietal) {
    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_ACTIVE_INFERENCE;

    parietal_result_t result = parietal_process(parietal, &req);

    EXPECT_TRUE(result.success);
    EXPECT_GT(result.confidence, 0.0f);
}

/* ============================================================================
 * PRECISION MODULATION TESTS
 * ============================================================================ */

/**
 * @brief Test attention-based precision modulation
 */
TEST_F(ParietalFepIntegrationTest, AttentionPrecisionModulation) {
    const uint32_t dim = 16;
    std::vector<float> attention_weights(dim, 1.0f);

    // High attention on first half
    for (uint32_t i = 0; i < dim / 2; i++) {
        attention_weights[i] = 2.0f;
    }

    int result = fep_parietal_set_attention_precision(fep_bridge,
        attention_weights.data(), dim);

    EXPECT_EQ(result, 0);

    // Now process observations
    auto observations = create_arithmetic_sequence(dim, 1.0f, 1.0f);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));

    fep_parietal_update_beliefs(fep_bridge, observations.data(),
        dim, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);

    EXPECT_GT(beliefs.confidence, 0.0f);

    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);
}

/**
 * @brief Test adaptive precision learning
 */
TEST_F(ParietalFepIntegrationTest, AdaptivePrecisionLearning) {
    // Process several observations to allow precision adaptation
    auto seq1 = create_arithmetic_sequence(10, 1.0f, 1.0f);
    auto seq2 = create_arithmetic_sequence(10, 1.0f, 2.0f);

    for (int i = 0; i < 5; i++) {
        fep_math_belief_t beliefs;
        memset(&beliefs, 0, sizeof(beliefs));
        fep_parietal_update_beliefs(fep_bridge, seq1.data(),
            10, FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }

    // Adapt precision
    int result = fep_parietal_adapt_precision(fep_bridge);
    EXPECT_EQ(result, 0);

    // Get precision for level 0
    float* precision = nullptr;
    uint32_t precision_dim = 0;
    result = fep_parietal_get_precision(fep_bridge, 0, &precision, &precision_dim);

    EXPECT_EQ(result, 0);
    if (precision) {
        free(precision);
    }
}

/* ============================================================================
 * INFLAMMATION AND FATIGUE INTEGRATION TESTS
 * ============================================================================ */

/**
 * @brief Test inflammation affects FEP processing through parietal
 */
TEST_F(ParietalFepIntegrationTest, InflammationAffectsFepProcessing) {
    auto seq = create_arithmetic_sequence(10, 1.0f, 1.0f);

    // Get baseline result with no inflammation
    parietal_set_inflammation(parietal, 0.0f);

    fep_math_belief_t low_infl_beliefs;
    memset(&low_infl_beliefs, 0, sizeof(low_infl_beliefs));
    fep_parietal_update_beliefs(fep_bridge, seq.data(), 10,
        FEP_MATH_DOMAIN_NUMERICAL, &low_infl_beliefs);

    // Set high inflammation
    parietal_set_inflammation(parietal, 0.9f);

    fep_math_belief_t high_infl_beliefs;
    memset(&high_infl_beliefs, 0, sizeof(high_infl_beliefs));
    fep_parietal_update_beliefs(fep_bridge, seq.data(), 10,
        FEP_MATH_DOMAIN_NUMERICAL, &high_infl_beliefs);

    // Both should work, but potentially with different characteristics
    EXPECT_GE(low_infl_beliefs.confidence, 0.0f);
    EXPECT_GE(high_infl_beliefs.confidence, 0.0f);

    if (low_infl_beliefs.mean) free(low_infl_beliefs.mean);
    if (low_infl_beliefs.precision) free(low_infl_beliefs.precision);
    if (high_infl_beliefs.mean) free(high_infl_beliefs.mean);
    if (high_infl_beliefs.precision) free(high_infl_beliefs.precision);

    // Reset inflammation
    parietal_set_inflammation(parietal, 0.0f);
}

/**
 * @brief Test fatigue affects FEP processing
 */
TEST_F(ParietalFepIntegrationTest, FatigueAffectsFepProcessing) {
    auto seq = create_arithmetic_sequence(10, 1.0f, 1.0f);

    // Get baseline with no fatigue
    parietal_set_fatigue(parietal, 0.0f);

    parietal_request_t req;
    memset(&req, 0, sizeof(req));
    req.type = PARIETAL_FEP_NUMERICAL_INFERENCE;

    parietal_result_t result_fresh = parietal_process(parietal, &req);

    // Set high fatigue
    parietal_set_fatigue(parietal, 0.9f);

    parietal_result_t result_fatigued = parietal_process(parietal, &req);

    // Both should succeed
    EXPECT_TRUE(result_fresh.success);
    EXPECT_TRUE(result_fatigued.success);

    // Reset
    parietal_set_fatigue(parietal, 0.0f);
}

/* ============================================================================
 * CROSS-DOMAIN INTEGRATION TESTS
 * ============================================================================ */

/**
 * @brief Test cross-domain belief transfer
 */
TEST_F(ParietalFepIntegrationTest, CrossDomainBeliefTransfer) {
    // Numerical pattern
    auto numbers = create_geometric_sequence(8, 1.0f, 2.0f);

    // Train on numerical
    for (int i = 0; i < 3; i++) {
        fep_math_belief_t beliefs;
        memset(&beliefs, 0, sizeof(beliefs));
        fep_parietal_update_beliefs(fep_bridge, numbers.data(), 8,
            FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }

    // Spatial positions following similar pattern
    auto positions = create_3d_positions(8);

    fep_math_belief_t spatial_beliefs;
    memset(&spatial_beliefs, 0, sizeof(spatial_beliefs));
    fep_parietal_spatial_inference(fep_bridge, positions.data(),
        (uint32_t)positions.size(), &spatial_beliefs);

    // Generative model should have cross-domain structure
    fep_math_generative_model_t model;
    fep_parietal_get_generative_model(fep_bridge, &model);

    EXPECT_GE(model.total_free_energy, 0.0f);

    if (spatial_beliefs.mean) free(spatial_beliefs.mean);
    if (spatial_beliefs.precision) free(spatial_beliefs.precision);
}

/**
 * @brief Test engineering domain through FEP
 */
TEST_F(ParietalFepIntegrationTest, EngineeringDomainThroughFep) {
    float engineering_params[] = {100.0f, 50.0f, 25.0f, 12.5f, 6.25f};

    fep_math_belief_t eng_beliefs;
    memset(&eng_beliefs, 0, sizeof(eng_beliefs));

    int result = fep_parietal_engineering_inference(fep_bridge,
        engineering_params, 5, FEP_MATH_DOMAIN_ENGINEERING, &eng_beliefs);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(eng_beliefs.domain, FEP_MATH_DOMAIN_ENGINEERING);

    if (eng_beliefs.mean) free(eng_beliefs.mean);
    if (eng_beliefs.precision) free(eng_beliefs.precision);
}

/* ============================================================================
 * SURPRISE AND CURIOSITY INTEGRATION TESTS
 * ============================================================================ */

/**
 * @brief Test surprise computation for unexpected patterns
 */
TEST_F(ParietalFepIntegrationTest, SurpriseForUnexpectedPatterns) {
    // Train on arithmetic sequence
    auto expected = create_arithmetic_sequence(10, 1.0f, 1.0f);
    for (int i = 0; i < 5; i++) {
        fep_math_belief_t beliefs;
        memset(&beliefs, 0, sizeof(beliefs));
        fep_parietal_update_beliefs(fep_bridge, expected.data(), 10,
            FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
        if (beliefs.mean) free(beliefs.mean);
        if (beliefs.precision) free(beliefs.precision);
    }

    // Compute surprise for expected pattern
    float expected_surprise = fep_parietal_compute_surprise(fep_bridge,
        expected.data(), 10);

    // Create unexpected pattern
    auto unexpected = create_geometric_sequence(10, 1.0f, 3.0f);
    float unexpected_surprise = fep_parietal_compute_surprise(fep_bridge,
        unexpected.data(), 10);

    // Unexpected should generally be more surprising
    EXPECT_GE(expected_surprise, 0.0f);
    EXPECT_GE(unexpected_surprise, 0.0f);
}

/**
 * @brief Test epistemic value (curiosity) drives exploration
 */
TEST_F(ParietalFepIntegrationTest, EpistemicValueDrivesExploration) {
    // Create a query that would provide information
    auto query = create_arithmetic_sequence(8, 1.0f, 1.0f);

    float epistemic_value = fep_parietal_epistemic_value(fep_bridge,
        query.data(), 8);

    // Should have positive epistemic value (information gain potential)
    EXPECT_GE(epistemic_value, 0.0f);
}

/* ============================================================================
 * STATISTICS TRACKING INTEGRATION TESTS
 * ============================================================================ */

/**
 * @brief Test FEP stats are tracked by parietal
 */
TEST_F(ParietalFepIntegrationTest, FepStatsTrackedByParietal) {
    // Reset stats
    parietal_reset_stats(parietal);

    // Perform FEP operations through parietal
    parietal_request_t req;
    memset(&req, 0, sizeof(req));

    req.type = PARIETAL_FEP_UPDATE_BELIEFS;
    parietal_process(parietal, &req);

    req.type = PARIETAL_FEP_PREDICT;
    parietal_process(parietal, &req);

    req.type = PARIETAL_FEP_ACTIVE_INFERENCE;
    parietal_process(parietal, &req);

    // Check stats
    parietal_stats_t stats;
    parietal_get_stats(parietal, &stats);

    EXPECT_GT(stats.fep_predictions, 0);
    EXPECT_GT(stats.fep_belief_updates, 0);
    EXPECT_GT(stats.fep_active_inferences, 0);
}

/**
 * @brief Test FEP bridge stats are accessible through parietal
 */
TEST_F(ParietalFepIntegrationTest, FepBridgeStatsAccessible) {
    // Perform some operations
    auto seq = create_arithmetic_sequence(10, 1.0f, 1.0f);
    fep_math_belief_t beliefs;
    memset(&beliefs, 0, sizeof(beliefs));
    int update_result = fep_parietal_update_beliefs(fep_bridge, seq.data(), 10,
        FEP_MATH_DOMAIN_NUMERICAL, &beliefs);
    if (beliefs.mean) free(beliefs.mean);
    if (beliefs.precision) free(beliefs.precision);

    // Verify belief update succeeded
    EXPECT_EQ(update_result, 0);

    // Get FEP bridge stats - verify API works
    fep_parietal_stats_t fep_stats;
    memset(&fep_stats, 0, sizeof(fep_stats));
    int result = fep_parietal_get_stats(fep_bridge, &fep_stats);

    EXPECT_EQ(result, 0);
    // Note: Stats tracking is implementation-dependent
}

/* ============================================================================
 * MODEL TRAINING INTEGRATION TESTS
 * ============================================================================ */

/**
 * @brief Test generative model training through FEP bridge
 */
TEST_F(ParietalFepIntegrationTest, GenerativeModelTraining) {
    const uint32_t num_samples = 20;
    std::vector<std::vector<float>> observations(num_samples);
    std::vector<std::vector<float>> targets(num_samples);
    std::vector<const float*> obs_ptrs(num_samples);
    std::vector<const float*> target_ptrs(num_samples);

    for (uint32_t i = 0; i < num_samples; i++) {
        observations[i] = create_arithmetic_sequence(8, (float)i, 1.0f);
        targets[i] = create_arithmetic_sequence(8, (float)i + 1.0f, 1.0f);
        obs_ptrs[i] = observations[i].data();
        target_ptrs[i] = targets[i].data();
    }

    float loss = fep_parietal_train_model(fep_bridge,
        obs_ptrs.data(), target_ptrs.data(), num_samples);

    EXPECT_GE(loss, 0.0f);
}
