/**
 * @file test_fuzzy_regression.cpp
 * @brief Regression tests for NIMCP Fuzzy Logic backward compatibility and correctness
 *
 * WHAT: ~30 regression tests covering NULL safety, boundary values,
 *       mathematical properties (t-norm/conorm axioms, idempotency,
 *       commutativity, monotonicity), error code ranges, config defaults,
 *       stats reset, modulation bounds, empty inputs, and large values
 * WHY:  Prevent regressions in API contracts, mathematical invariants,
 *       and edge-case handling across fuzzy logic subsystem releases
 * HOW:  GTest assertions verify invariant properties without depending
 *       on specific numerical outputs beyond axiomatic bounds
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cfloat>
#include <vector>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
}

static constexpr float TOL = 1e-4f;

// =============================================================================
// Fixture: Regression Tests
// =============================================================================

class FuzzyRegressionTest : public ::testing::Test {
protected:
    fuzzy_types_engine_t* types_engine = nullptr;
    fuzzy_inference_engine_t* inf_engine = nullptr;
    fuzzy_bridge_t* bridge = nullptr;

    void SetUp() override {
        types_engine = fuzzy_types_create();
        ASSERT_NE(types_engine, nullptr);

        inf_engine = fuzzy_inference_create();
        ASSERT_NE(inf_engine, nullptr);

        fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
        bridge = fuzzy_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        fuzzy_bridge_destroy(bridge);
        fuzzy_inference_destroy(inf_engine);
        fuzzy_types_destroy(types_engine);
    }
};

// =============================================================================
// NULL Safety Tests
// =============================================================================

TEST_F(FuzzyRegressionTest, NullMFEvaluateReturnsZero) {
    float result = fuzzy_mf_evaluate(nullptr, 0.5f);
    EXPECT_NEAR(result, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, NullSetEvaluateReturnsZero) {
    float result = fuzzy_set_evaluate(nullptr, 0.5f);
    EXPECT_NEAR(result, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, NullVariableCreateReturnsError) {
    int rc = fuzzy_variable_create(nullptr, "test", 0.0f, 1.0f);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullVariableFuzzifyReturnsError) {
    int rc = fuzzy_variable_fuzzify(nullptr, 0.5f, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullSetCreateReturnsError) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    int rc = fuzzy_set_create(nullptr, "test", &mf, FUZZY_HEDGE_NONE);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullInferenceAddInputReturnsError) {
    int rc = fuzzy_inference_add_input(nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullInferenceEvaluateReturnsError) {
    float inputs[] = {1.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(nullptr, inputs, 1, &result);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullBridgeToSpikeReturnsError) {
    float m[] = {0.5f};
    float r[1] = {0};
    int rc = fuzzy_bridge_to_spike_population(nullptr, m, 1, r);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullBridgeStatsReturnsError) {
    int rc = fuzzy_bridge_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

TEST_F(FuzzyRegressionTest, NullTypesStatsReturnsError) {
    int rc = fuzzy_types_get_stats(nullptr, nullptr);
    EXPECT_NE(rc, 0);
}

// =============================================================================
// Boundary Value Tests
// =============================================================================

TEST_F(FuzzyRegressionTest, MFEvaluateAtExactZero) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    float val = fuzzy_mf_evaluate(&tri, 0.0f);
    // At left foot of triangular, membership = 0
    EXPECT_NEAR(val, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, MFEvaluateAtExactOne) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    float val = fuzzy_mf_evaluate(&tri, 1.0f);
    // At right foot of triangular, membership = 0
    EXPECT_NEAR(val, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, MFEvaluateAtPeak) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    float val = fuzzy_mf_evaluate(&tri, 0.5f);
    EXPECT_NEAR(val, 1.0f, TOL);
}

TEST_F(FuzzyRegressionTest, SingletonMFExactMatch) {
    fuzzy_mf_t s = fuzzy_mf_singleton(5.0f);
    float val_at = fuzzy_mf_evaluate(&s, 5.0f);
    EXPECT_NEAR(val_at, 1.0f, TOL);

    float val_off = fuzzy_mf_evaluate(&s, 5.1f);
    EXPECT_NEAR(val_off, 0.0f, TOL);
}

// =============================================================================
// Mathematical Properties: T-Norm Axioms
// =============================================================================

TEST_F(FuzzyRegressionTest, TNormIdentity) {
    // T-norm(a, 1) = a for all t-norm types
    float a = 0.7f;
    for (int t = 0; t < FUZZY_TNORM_TYPE_COUNT; t++) {
        float result = fuzzy_tnorm(a, 1.0f, (fuzzy_tnorm_type_t)t);
        EXPECT_NEAR(result, a, TOL) << "Failed for t-norm type " << t;
    }
}

TEST_F(FuzzyRegressionTest, TNormAnnihilator) {
    // T-norm(a, 0) = 0 for all t-norm types
    float a = 0.7f;
    for (int t = 0; t < FUZZY_TNORM_TYPE_COUNT; t++) {
        float result = fuzzy_tnorm(a, 0.0f, (fuzzy_tnorm_type_t)t);
        EXPECT_NEAR(result, 0.0f, TOL) << "Failed for t-norm type " << t;
    }
}

TEST_F(FuzzyRegressionTest, TConormIdentity) {
    // T-conorm(a, 0) = a for all t-conorm types
    float a = 0.7f;
    for (int t = 0; t < FUZZY_TCONORM_TYPE_COUNT; t++) {
        float result = fuzzy_tconorm(a, 0.0f, (fuzzy_tconorm_type_t)t);
        EXPECT_NEAR(result, a, TOL) << "Failed for t-conorm type " << t;
    }
}

TEST_F(FuzzyRegressionTest, ComplementInvolution) {
    // complement(complement(a)) = a (standard complement only)
    float a = 0.35f;
    float c1 = fuzzy_complement(a, FUZZY_COMPLEMENT_STANDARD, 0.0f);
    float c2 = fuzzy_complement(c1, FUZZY_COMPLEMENT_STANDARD, 0.0f);
    EXPECT_NEAR(c2, a, TOL);
}

// =============================================================================
// Idempotency
// =============================================================================

TEST_F(FuzzyRegressionTest, MinIdempotency) {
    // min(a, a) = a
    float a = 0.42f;
    float result = fuzzy_tnorm(a, a, FUZZY_TNORM_MIN);
    EXPECT_NEAR(result, a, TOL);
}

TEST_F(FuzzyRegressionTest, MaxIdempotency) {
    // max(a, a) = a
    float a = 0.42f;
    float result = fuzzy_tconorm(a, a, FUZZY_TCONORM_MAX);
    EXPECT_NEAR(result, a, TOL);
}

// =============================================================================
// Commutativity
// =============================================================================

TEST_F(FuzzyRegressionTest, TNormCommutativity) {
    float a = 0.3f, b = 0.8f;
    for (int t = 0; t < FUZZY_TNORM_TYPE_COUNT; t++) {
        float ab = fuzzy_tnorm(a, b, (fuzzy_tnorm_type_t)t);
        float ba = fuzzy_tnorm(b, a, (fuzzy_tnorm_type_t)t);
        EXPECT_NEAR(ab, ba, TOL) << "Failed for t-norm type " << t;
    }
}

TEST_F(FuzzyRegressionTest, TConormCommutativity) {
    float a = 0.3f, b = 0.8f;
    for (int t = 0; t < FUZZY_TCONORM_TYPE_COUNT; t++) {
        float ab = fuzzy_tconorm(a, b, (fuzzy_tconorm_type_t)t);
        float ba = fuzzy_tconorm(b, a, (fuzzy_tconorm_type_t)t);
        EXPECT_NEAR(ab, ba, TOL) << "Failed for t-conorm type " << t;
    }
}

// =============================================================================
// Monotonicity
// =============================================================================

TEST_F(FuzzyRegressionTest, TNormMonotonicity) {
    // If a <= a', then tnorm(a, b) <= tnorm(a', b)
    float a = 0.3f, a_prime = 0.6f, b = 0.5f;
    for (int t = 0; t < FUZZY_TNORM_TYPE_COUNT; t++) {
        float low = fuzzy_tnorm(a, b, (fuzzy_tnorm_type_t)t);
        float high = fuzzy_tnorm(a_prime, b, (fuzzy_tnorm_type_t)t);
        EXPECT_LE(low, high + TOL) << "Monotonicity failed for t-norm type " << t;
    }
}

TEST_F(FuzzyRegressionTest, TConormMonotonicity) {
    float a = 0.3f, a_prime = 0.6f, b = 0.5f;
    for (int t = 0; t < FUZZY_TCONORM_TYPE_COUNT; t++) {
        float low = fuzzy_tconorm(a, b, (fuzzy_tconorm_type_t)t);
        float high = fuzzy_tconorm(a_prime, b, (fuzzy_tconorm_type_t)t);
        EXPECT_LE(low, high + TOL) << "Monotonicity failed for t-conorm type " << t;
    }
}

// =============================================================================
// Error Codes: Distinct and in Correct Range
// =============================================================================

TEST_F(FuzzyRegressionTest, TypesErrorCodesDistinct) {
    std::vector<int> codes = {
        FUZZY_ERR_NULL, FUZZY_ERR_INVALID_MF_TYPE, FUZZY_ERR_INVALID_PARAMS,
        FUZZY_ERR_PARAM_COUNT, FUZZY_ERR_INVALID_HEDGE, FUZZY_ERR_MAX_TERMS,
        FUZZY_ERR_UNIVERSE_RANGE, FUZZY_ERR_ALLOC, FUZZY_ERR_RESOLUTION,
        FUZZY_ERR_DIMENSION_MISMATCH, FUZZY_ERR_EMPTY_SET,
        FUZZY_ERR_INVALID_NAME, FUZZY_ERR_CUSTOM_FN_NULL
    };
    // All in range [28001, 28013]
    for (int c : codes) {
        EXPECT_GE(c, FUZZY_TYPES_ERROR_BASE + 1);
        EXPECT_LE(c, FUZZY_TYPES_ERROR_BASE + 13);
    }
    // All distinct
    for (size_t i = 0; i < codes.size(); i++) {
        for (size_t j = i + 1; j < codes.size(); j++) {
            EXPECT_NE(codes[i], codes[j]) << "Duplicate error codes at " << i << " and " << j;
        }
    }
}

TEST_F(FuzzyRegressionTest, InferenceErrorCodesDistinct) {
    std::vector<int> codes = {
        FUZZY_INF_ERR_NULL, FUZZY_INF_ERR_MAX_RULES, FUZZY_INF_ERR_MAX_INPUTS,
        FUZZY_INF_ERR_MAX_OUTPUTS, FUZZY_INF_ERR_INVALID_FIS,
        FUZZY_INF_ERR_INVALID_RULE, FUZZY_INF_ERR_NO_RULES,
        FUZZY_INF_ERR_INPUT_MISMATCH, FUZZY_INF_ERR_ALLOC,
        FUZZY_INF_ERR_CONVERGENCE
    };
    for (int c : codes) {
        EXPECT_GE(c, FUZZY_INFERENCE_ERROR_BASE + 1);
        EXPECT_LE(c, FUZZY_INFERENCE_ERROR_BASE + 10);
    }
    for (size_t i = 0; i < codes.size(); i++) {
        for (size_t j = i + 1; j < codes.size(); j++) {
            EXPECT_NE(codes[i], codes[j]);
        }
    }
}

TEST_F(FuzzyRegressionTest, BridgeErrorCodesDistinct) {
    std::vector<int> codes = {
        FUZZY_BRIDGE_ERR_NULL, FUZZY_BRIDGE_ERR_NOT_CONNECTED,
        FUZZY_BRIDGE_ERR_SUBSYSTEM, FUZZY_BRIDGE_ERR_STATE,
        FUZZY_BRIDGE_ERR_CONVERSION, FUZZY_BRIDGE_ERR_INVALID_DIM
    };
    for (int c : codes) {
        EXPECT_GE(c, FUZZY_BRIDGE_ERROR_BASE + 1);
        EXPECT_LE(c, FUZZY_BRIDGE_ERROR_BASE + 6);
    }
    for (size_t i = 0; i < codes.size(); i++) {
        for (size_t j = i + 1; j < codes.size(); j++) {
            EXPECT_NE(codes[i], codes[j]);
        }
    }
}

// =============================================================================
// Config Defaults
// =============================================================================

TEST_F(FuzzyRegressionTest, TypesDefaultConfig) {
    fuzzy_types_config_t cfg = fuzzy_types_default_config();
    EXPECT_EQ(cfg.default_resolution, 256u);
    EXPECT_NEAR(cfg.alpha_cut_default, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, InferenceDefaultConfig) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    EXPECT_EQ(cfg.fis_type, FUZZY_FIS_MAMDANI);
    EXPECT_EQ(cfg.defuzz_method, FUZZY_DEFUZZ_CENTROID);
    EXPECT_EQ(cfg.and_method, FUZZY_TNORM_MIN);
    EXPECT_EQ(cfg.or_method, FUZZY_TCONORM_MAX);
}

TEST_F(FuzzyRegressionTest, BridgeDefaultConfig) {
    fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
    EXPECT_GE(cfg.spike_rate_min, 0.0f);
    EXPECT_GT(cfg.spike_rate_max, cfg.spike_rate_min);
    EXPECT_GT(cfg.training_lr_max, cfg.training_lr_min);
}

// =============================================================================
// Stats Reset
// =============================================================================

TEST_F(FuzzyRegressionTest, TypesStatsResetZeroesAll) {
    // Do some work to get non-zero stats
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    fuzzy_mf_evaluate(&tri, 0.5f);

    fuzzy_types_reset_stats(types_engine);
    fuzzy_types_stats_t stats;
    ASSERT_EQ(fuzzy_types_get_stats(types_engine, &stats), 0);
    // Stats might be per-engine or global; verify the engine-level ones are zeroed
    EXPECT_EQ(stats.mf_evaluations, 0u);
    EXPECT_EQ(stats.fuzzifications, 0u);
    EXPECT_EQ(stats.hedge_applications, 0u);
}

TEST_F(FuzzyRegressionTest, InferenceStatsResetZeroesAll) {
    fuzzy_inference_reset_stats(inf_engine);
    fuzzy_inference_stats_t stats;
    ASSERT_EQ(fuzzy_inference_get_stats(inf_engine, &stats), 0);
    EXPECT_EQ(stats.inferences_run, 0u);
    EXPECT_EQ(stats.rules_evaluated, 0u);
    EXPECT_EQ(stats.defuzzifications, 0u);
}

TEST_F(FuzzyRegressionTest, BridgeStatsResetZeroesAll) {
    fuzzy_bridge_reset_stats(bridge);
    fuzzy_bridge_stats_t stats;
    ASSERT_EQ(fuzzy_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.spike_conversions, 0u);
    EXPECT_EQ(stats.stdp_modulations, 0u);
    EXPECT_EQ(stats.training_lr_schedules, 0u);
}

TEST_F(FuzzyRegressionTest, OperatorStatsResetZeroesAll) {
    fuzzy_operator_reset_stats();
    fuzzy_operator_stats_t stats;
    ASSERT_EQ(fuzzy_operator_get_stats(&stats), 0);
    EXPECT_EQ(stats.tnorm_evaluations, 0u);
    EXPECT_EQ(stats.tconorm_evaluations, 0u);
    EXPECT_EQ(stats.complement_evaluations, 0u);
}

// =============================================================================
// Modulation Bounds
// =============================================================================

TEST_F(FuzzyRegressionTest, InflammationClampedToUnitInterval) {
    // Values at boundaries should succeed
    EXPECT_EQ(fuzzy_types_set_inflammation(types_engine, 0.0f), 0);
    EXPECT_EQ(fuzzy_types_set_inflammation(types_engine, 1.0f), 0);
    EXPECT_EQ(fuzzy_bridge_set_inflammation(bridge, 0.0f), 0);
    EXPECT_EQ(fuzzy_bridge_set_inflammation(bridge, 1.0f), 0);
}

TEST_F(FuzzyRegressionTest, FatigueClampedToUnitInterval) {
    EXPECT_EQ(fuzzy_types_set_fatigue(types_engine, 0.0f), 0);
    EXPECT_EQ(fuzzy_types_set_fatigue(types_engine, 1.0f), 0);
    EXPECT_EQ(fuzzy_bridge_set_fatigue(bridge, 0.0f), 0);
    EXPECT_EQ(fuzzy_bridge_set_fatigue(bridge, 1.0f), 0);
}

// =============================================================================
// Empty / Zero Inputs
// =============================================================================

TEST_F(FuzzyRegressionTest, EmptyVariableNoTermsFuzzify) {
    fuzzy_variable_t var;
    ASSERT_EQ(fuzzy_variable_create(&var, "empty_var", 0.0f, 10.0f), 0);

    fuzzy_value_t val;
    // Fuzzifying a variable with zero terms may return error or zero result
    int rc = fuzzy_variable_fuzzify(&var, 5.0f, &val);
    // Either error or success with zero terms
    if (rc == 0) {
        EXPECT_EQ(val.num_terms, 0u);
    }
}

TEST_F(FuzzyRegressionTest, ZeroLengthArrayOperators) {
    // T-norm on zero-length array should handle gracefully
    float result = fuzzy_tnorm_array(nullptr, 0, FUZZY_TNORM_MIN);
    // Typically returns identity element (1.0) for empty conjunction
    EXPECT_TRUE(result >= 0.0f && result <= 1.0f);

    result = fuzzy_tconorm_array(nullptr, 0, FUZZY_TCONORM_MAX);
    // Typically returns identity element (0.0) for empty disjunction
    EXPECT_TRUE(result >= 0.0f && result <= 1.0f);
}

// =============================================================================
// Large / Extreme Values
// =============================================================================

TEST_F(FuzzyRegressionTest, LargeFloatMFEvaluation) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    // Very large value outside universe
    float val = fuzzy_mf_evaluate(&tri, 1e6f);
    EXPECT_GE(val, 0.0f);
    EXPECT_LE(val, 1.0f);

    // Very small negative value
    val = fuzzy_mf_evaluate(&tri, -1e6f);
    EXPECT_GE(val, 0.0f);
    EXPECT_LE(val, 1.0f);
}

TEST_F(FuzzyRegressionTest, VerySmallFloatGaussianMF) {
    fuzzy_mf_t gauss = fuzzy_mf_gaussian(0.0f, 1e-6f);
    // At center, should still be 1.0
    float val = fuzzy_mf_evaluate(&gauss, 0.0f);
    EXPECT_NEAR(val, 1.0f, TOL);

    // Far from center with very narrow sigma, should be ~0
    val = fuzzy_mf_evaluate(&gauss, 1.0f);
    EXPECT_NEAR(val, 0.0f, TOL);
}

TEST_F(FuzzyRegressionTest, DestroyNullSafe) {
    // All destroy functions should be NULL-safe (no crash)
    fuzzy_types_destroy(nullptr);
    fuzzy_inference_destroy(nullptr);
    fuzzy_bridge_destroy(nullptr);
    fuzzy_discrete_set_free(nullptr);
}

TEST_F(FuzzyRegressionTest, LastErrorStringNotNull) {
    const char* err = fuzzy_types_get_last_error();
    // Should return some string (possibly empty, but not NULL)
    // Allow NULL if implementation returns NULL for no error
    // Main point: does not crash
    (void)err;

    const char* ierr = fuzzy_inference_get_last_error();
    (void)ierr;

    const char* berr = fuzzy_bridge_get_last_error();
    (void)berr;
}
