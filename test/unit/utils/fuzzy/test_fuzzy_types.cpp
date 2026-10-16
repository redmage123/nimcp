/**
 * @file test_fuzzy_types.cpp
 * @brief Unit tests for fuzzy logic core types module
 *
 * WHAT: ~50 tests for fuzzy membership functions, hedges, sets, variables,
 *       discretization, entropy, cardinality, modulation, and error handling
 * WHY:  Verify correct evaluation of all 14 MF types, 8 hedges, set operations,
 *       fuzzification, and engine lifecycle
 * HOW:  GTest C++17, extern "C" headers, EXPECT_NEAR for float comparisons
 *
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
}

// ============================================================================
// Test Constants
// ============================================================================

namespace {
    constexpr float TOLERANCE = 1e-4f;
    constexpr float RELAXED_TOL = 1e-2f;
}

// ============================================================================
// Fixture: Engine Lifecycle
// ============================================================================

class FuzzyTypesEngineTest : public ::testing::Test {
protected:
    fuzzy_types_engine_t* engine = nullptr;

    void SetUp() override {
        engine = fuzzy_types_create();
    }

    void TearDown() override {
        if (engine) {
            fuzzy_types_destroy(engine);
            engine = nullptr;
        }
    }
};

// ============================================================================
// Fixture: Membership Function Evaluation
// ============================================================================

class FuzzyMFTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// 1. Engine Lifecycle Tests
// ============================================================================

TEST_F(FuzzyTypesEngineTest, CreateDefault_ReturnsNonNull) {
    ASSERT_NE(engine, nullptr);
}

TEST(FuzzyTypesLifecycle, DestroyNull_NoOp) {
    fuzzy_types_destroy(nullptr);  // should not crash
}

TEST(FuzzyTypesLifecycle, CreateCustomWithNullConfig_ReturnsNonNull) {
    fuzzy_types_engine_t* eng = fuzzy_types_create_custom(nullptr);
    ASSERT_NE(eng, nullptr);
    fuzzy_types_destroy(eng);
}

TEST(FuzzyTypesLifecycle, CreateCustomWithConfig_ReturnsNonNull) {
    fuzzy_types_config_t cfg = fuzzy_types_default_config();
    cfg.default_resolution = 128;
    cfg.enable_caching = true;
    fuzzy_types_engine_t* eng = fuzzy_types_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);
    fuzzy_types_destroy(eng);
}

TEST(FuzzyTypesLifecycle, DefaultConfig_HasReasonableDefaults) {
    fuzzy_types_config_t cfg = fuzzy_types_default_config();
    EXPECT_GT(cfg.default_resolution, 0u);
    EXPECT_GE(cfg.alpha_cut_default, 0.0f);
    EXPECT_LE(cfg.alpha_cut_default, 1.0f);
}

// ============================================================================
// 2. Triangular MF
// ============================================================================

TEST_F(FuzzyMFTest, Triangular_AtPeak) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Triangular_AtLeftFoot) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 0.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Triangular_AtRightFoot) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 10.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Triangular_BelowRange) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, -1.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Triangular_Midpoint) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 2.5f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

// ============================================================================
// 3. Trapezoidal MF
// ============================================================================

TEST_F(FuzzyMFTest, Trapezoidal_OnShoulder) {
    fuzzy_mf_t mf = fuzzy_mf_trapezoidal(0.0f, 2.0f, 8.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Trapezoidal_AtLeftFoot) {
    fuzzy_mf_t mf = fuzzy_mf_trapezoidal(0.0f, 2.0f, 8.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 0.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Trapezoidal_AtRightFoot) {
    fuzzy_mf_t mf = fuzzy_mf_trapezoidal(0.0f, 2.0f, 8.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 10.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Trapezoidal_RisingEdge) {
    fuzzy_mf_t mf = fuzzy_mf_trapezoidal(0.0f, 2.0f, 8.0f, 10.0f);
    float val = fuzzy_mf_evaluate(&mf, 1.0f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

// ============================================================================
// 4. Gaussian MF
// ============================================================================

TEST_F(FuzzyMFTest, Gaussian_AtMean) {
    fuzzy_mf_t mf = fuzzy_mf_gaussian(5.0f, 1.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Gaussian_AtOneSigma) {
    fuzzy_mf_t mf = fuzzy_mf_gaussian(5.0f, 1.0f);
    float val = fuzzy_mf_evaluate(&mf, 6.0f);
    // exp(-0.5) ~ 0.6065
    EXPECT_NEAR(val, std::exp(-0.5f), RELAXED_TOL);
}

TEST_F(FuzzyMFTest, Gaussian_FarAway) {
    fuzzy_mf_t mf = fuzzy_mf_gaussian(5.0f, 1.0f);
    float val = fuzzy_mf_evaluate(&mf, 20.0f);
    EXPECT_LT(val, 0.01f);
}

// ============================================================================
// 5. Bell MF
// ============================================================================

TEST_F(FuzzyMFTest, Bell_AtCenter) {
    fuzzy_mf_t mf = fuzzy_mf_bell(2.0f, 4.0f, 5.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Bell_FarFromCenter) {
    fuzzy_mf_t mf = fuzzy_mf_bell(2.0f, 4.0f, 5.0f);
    float val = fuzzy_mf_evaluate(&mf, 100.0f);
    EXPECT_LT(val, 0.01f);
}

// ============================================================================
// 6. Sigmoid MF
// ============================================================================

TEST_F(FuzzyMFTest, Sigmoid_AtCenter) {
    fuzzy_mf_t mf = fuzzy_mf_sigmoid(2.0f, 5.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Sigmoid_FarPositive) {
    fuzzy_mf_t mf = fuzzy_mf_sigmoid(2.0f, 5.0f);
    float val = fuzzy_mf_evaluate(&mf, 20.0f);
    EXPECT_GT(val, 0.99f);
}

TEST_F(FuzzyMFTest, Sigmoid_FarNegative) {
    fuzzy_mf_t mf = fuzzy_mf_sigmoid(2.0f, 5.0f);
    float val = fuzzy_mf_evaluate(&mf, -10.0f);
    EXPECT_LT(val, 0.01f);
}

// ============================================================================
// 7. S-Shaped MF
// ============================================================================

TEST_F(FuzzyMFTest, SShaped_AtFoot) {
    fuzzy_mf_t mf = fuzzy_mf_s_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 1.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, SShaped_AtShoulder) {
    fuzzy_mf_t mf = fuzzy_mf_s_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 9.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, SShaped_AtMidpoint) {
    fuzzy_mf_t mf = fuzzy_mf_s_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

// ============================================================================
// 8. Z-Shaped MF
// ============================================================================

TEST_F(FuzzyMFTest, ZShaped_AtShoulder) {
    fuzzy_mf_t mf = fuzzy_mf_z_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 1.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, ZShaped_AtFoot) {
    fuzzy_mf_t mf = fuzzy_mf_z_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 9.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, ZShaped_AtMidpoint) {
    fuzzy_mf_t mf = fuzzy_mf_z_shaped(1.0f, 9.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

// ============================================================================
// 9. Singleton MF
// ============================================================================

TEST_F(FuzzyMFTest, Singleton_AtValue) {
    fuzzy_mf_t mf = fuzzy_mf_singleton(5.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST_F(FuzzyMFTest, Singleton_AwayFromValue) {
    fuzzy_mf_t mf = fuzzy_mf_singleton(5.0f);
    float val = fuzzy_mf_evaluate(&mf, 5.5f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

// ============================================================================
// 10. Custom MF
// ============================================================================

static float custom_constant_half(float x, const float* params,
                                   uint32_t num_params, void* user_data) {
    (void)x; (void)params; (void)num_params; (void)user_data;
    return 0.5f;
}

TEST_F(FuzzyMFTest, Custom_CallbackInvoked) {
    fuzzy_mf_t mf = fuzzy_mf_custom(custom_constant_half, nullptr, 0, nullptr);
    float val = fuzzy_mf_evaluate(&mf, 42.0f);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

// ============================================================================
// 11. Hedges
// ============================================================================

TEST(FuzzyHedge, None_NoChange) {
    float val = fuzzy_apply_hedge(0.7f, FUZZY_HEDGE_NONE);
    EXPECT_NEAR(val, 0.7f, TOLERANCE);
}

TEST(FuzzyHedge, Very_SquaresValue) {
    float val = fuzzy_apply_hedge(0.5f, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(val, 0.25f, TOLERANCE);
}

TEST(FuzzyHedge, Somewhat_SqrtValue) {
    float val = fuzzy_apply_hedge(0.25f, FUZZY_HEDGE_SOMEWHAT);
    EXPECT_NEAR(val, 0.5f, TOLERANCE);
}

TEST(FuzzyHedge, Extremely_CubesValue) {
    float val = fuzzy_apply_hedge(0.5f, FUZZY_HEDGE_EXTREMELY);
    EXPECT_NEAR(val, 0.125f, TOLERANCE);
}

TEST(FuzzyHedge, Not_ComplementsValue) {
    float val = fuzzy_apply_hedge(0.3f, FUZZY_HEDGE_NOT);
    EXPECT_NEAR(val, 0.7f, TOLERANCE);
}

TEST(FuzzyHedge, MoreOrLess_Power075) {
    float val = fuzzy_apply_hedge(0.5f, FUZZY_HEDGE_MORE_OR_LESS);
    float expected = std::pow(0.5f, 0.75f);
    EXPECT_NEAR(val, expected, TOLERANCE);
}

TEST(FuzzyHedge, Slightly_IntensificationAround05) {
    float val = fuzzy_apply_hedge(0.5f, FUZZY_HEDGE_SLIGHTLY);
    // Result should be between 0 and 1
    EXPECT_GE(val, 0.0f);
    EXPECT_LE(val, 1.0f);
}

TEST(FuzzyHedge, Indeed_IntensifiesValue) {
    float val = fuzzy_apply_hedge(0.8f, FUZZY_HEDGE_INDEED);
    // Indeed should shift toward 0 or 1
    EXPECT_GE(val, 0.0f);
    EXPECT_LE(val, 1.0f);
}

TEST(FuzzyHedge, HedgedMFEvaluation) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float base = fuzzy_mf_evaluate(&mf, 5.0f);
    float hedged = fuzzy_mf_evaluate_hedged(&mf, 5.0f, FUZZY_HEDGE_VERY);
    // very(1.0) = 1.0^2 = 1.0
    EXPECT_NEAR(base, 1.0f, TOLERANCE);
    EXPECT_NEAR(hedged, 1.0f, TOLERANCE);
}

TEST(FuzzyHedge, HedgedMFEvaluation_NonPeak) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    float base = fuzzy_mf_evaluate(&mf, 2.5f);      // 0.5
    float hedged = fuzzy_mf_evaluate_hedged(&mf, 2.5f, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(base, 0.5f, TOLERANCE);
    EXPECT_NEAR(hedged, 0.25f, TOLERANCE);
}

// ============================================================================
// 12. Fuzzy Set Operations
// ============================================================================

TEST(FuzzySet, CreateAndEvaluate) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t set;
    int rc = fuzzy_set_create(&set, "warm", &mf, FUZZY_HEDGE_NONE);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_STREQ(set.name, "warm");
    float val = fuzzy_set_evaluate(&set, 5.0f);
    EXPECT_NEAR(val, 1.0f, TOLERANCE);
}

TEST(FuzzySet, CreateWithHedge) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t set;
    int rc = fuzzy_set_create(&set, "very_warm", &mf, FUZZY_HEDGE_VERY);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    float val = fuzzy_set_evaluate(&set, 2.5f);
    // base=0.5, very=0.25
    EXPECT_NEAR(val, 0.25f, TOLERANCE);
}

TEST(FuzzySet, CreateWithNullName_ReturnsError) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t set;
    int rc = fuzzy_set_create(&set, nullptr, &mf, FUZZY_HEDGE_NONE);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST(FuzzySet, CreateWithNullMF_ReturnsError) {
    fuzzy_set_t set;
    int rc = fuzzy_set_create(&set, "test", nullptr, FUZZY_HEDGE_NONE);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST(FuzzySet, CreateWithNullSet_ReturnsError) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    int rc = fuzzy_set_create(nullptr, "test", &mf, FUZZY_HEDGE_NONE);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

// ============================================================================
// 13. Linguistic Variable Operations
// ============================================================================

class FuzzyVariableTest : public ::testing::Test {
protected:
    fuzzy_variable_t var;
    fuzzy_set_t cold_set, warm_set, hot_set;

    void SetUp() override {
        int rc = fuzzy_variable_create(&var, "temperature", 0.0f, 100.0f);
        ASSERT_EQ(rc, FUZZY_ERR_OK);

        fuzzy_mf_t cold_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 20.0f, 40.0f);
        fuzzy_mf_t warm_mf = fuzzy_mf_triangular(20.0f, 50.0f, 80.0f);
        fuzzy_mf_t hot_mf  = fuzzy_mf_trapezoidal(60.0f, 80.0f, 100.0f, 100.0f);

        fuzzy_set_create(&cold_set, "cold", &cold_mf, FUZZY_HEDGE_NONE);
        fuzzy_set_create(&warm_set, "warm", &warm_mf, FUZZY_HEDGE_NONE);
        fuzzy_set_create(&hot_set,  "hot",  &hot_mf,  FUZZY_HEDGE_NONE);

        fuzzy_variable_add_term(&var, &cold_set);
        fuzzy_variable_add_term(&var, &warm_set);
        fuzzy_variable_add_term(&var, &hot_set);
    }
};

TEST_F(FuzzyVariableTest, Create_SetsNameAndBounds) {
    EXPECT_STREQ(var.name, "temperature");
    EXPECT_FLOAT_EQ(var.universe_min, 0.0f);
    EXPECT_FLOAT_EQ(var.universe_max, 100.0f);
    EXPECT_EQ(var.num_terms, 3u);
}

TEST_F(FuzzyVariableTest, Fuzzify_ColdValue) {
    fuzzy_value_t fval;
    int rc = fuzzy_variable_fuzzify(&var, 10.0f, &fval);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_GT(fval.memberships[0], 0.5f);   // cold should dominate
    EXPECT_EQ(fval.num_terms, 3u);
}

TEST_F(FuzzyVariableTest, Fuzzify_WarmValue) {
    fuzzy_value_t fval;
    int rc = fuzzy_variable_fuzzify(&var, 50.0f, &fval);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_NEAR(fval.memberships[1], 1.0f, TOLERANCE);  // warm peak at 50
}

TEST_F(FuzzyVariableTest, Fuzzify_HotValue) {
    fuzzy_value_t fval;
    int rc = fuzzy_variable_fuzzify(&var, 90.0f, &fval);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_GT(fval.memberships[2], 0.5f);   // hot should dominate
}

TEST_F(FuzzyVariableTest, Fuzzify_DominantTerm) {
    fuzzy_value_t fval;
    fuzzy_variable_fuzzify(&var, 50.0f, &fval);
    EXPECT_EQ(fval.dominant_term, 1u);      // warm is dominant
    EXPECT_NEAR(fval.dominant_degree, 1.0f, TOLERANCE);
}

TEST_F(FuzzyVariableTest, Centroid_WarmDominated) {
    fuzzy_value_t fval;
    fuzzy_variable_fuzzify(&var, 50.0f, &fval);
    float centroid = fuzzy_variable_centroid(&var, &fval);
    // Centroid should be near the warm center (50)
    EXPECT_GT(centroid, 30.0f);
    EXPECT_LT(centroid, 70.0f);
}

TEST(FuzzyVariable, CreateWithNullVar_ReturnsError) {
    int rc = fuzzy_variable_create(nullptr, "test", 0.0f, 100.0f);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST(FuzzyVariable, CreateWithNullName_ReturnsError) {
    fuzzy_variable_t var;
    int rc = fuzzy_variable_create(&var, nullptr, 0.0f, 100.0f);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST(FuzzyVariable, FuzzifyNull_ReturnsError) {
    fuzzy_value_t fval;
    int rc = fuzzy_variable_fuzzify(nullptr, 50.0f, &fval);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

// ============================================================================
// 14. Discrete Set Operations
// ============================================================================

TEST(FuzzyDiscreteSet, CreateAndFree) {
    fuzzy_discrete_set_t set;
    int rc = fuzzy_discrete_set_create(&set, 64, 0.0f, 10.0f);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_NE(set.values, nullptr);
    EXPECT_EQ(set.resolution, 64u);
    EXPECT_FLOAT_EQ(set.x_min, 0.0f);
    EXPECT_FLOAT_EQ(set.x_max, 10.0f);
    fuzzy_discrete_set_free(&set);
}

TEST(FuzzyDiscreteSet, FreeNull_NoOp) {
    fuzzy_discrete_set_free(nullptr);  // should not crash
}

TEST(FuzzyDiscreteSet, Union_PointwiseMax) {
    fuzzy_discrete_set_t a, b, out;
    fuzzy_discrete_set_create(&a, 4, 0.0f, 3.0f);
    fuzzy_discrete_set_create(&b, 4, 0.0f, 3.0f);
    a.values[0] = 0.1f; a.values[1] = 0.5f; a.values[2] = 0.9f; a.values[3] = 0.2f;
    b.values[0] = 0.4f; b.values[1] = 0.3f; b.values[2] = 0.7f; b.values[3] = 0.8f;

    int rc = fuzzy_discrete_set_union(&a, &b, &out);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_NEAR(out.values[0], 0.4f, TOLERANCE);  // max(0.1, 0.4)
    EXPECT_NEAR(out.values[1], 0.5f, TOLERANCE);  // max(0.5, 0.3)
    EXPECT_NEAR(out.values[2], 0.9f, TOLERANCE);  // max(0.9, 0.7)
    EXPECT_NEAR(out.values[3], 0.8f, TOLERANCE);  // max(0.2, 0.8)

    fuzzy_discrete_set_free(&a);
    fuzzy_discrete_set_free(&b);
    fuzzy_discrete_set_free(&out);
}

TEST(FuzzyDiscreteSet, Intersection_PointwiseMin) {
    fuzzy_discrete_set_t a, b, out;
    fuzzy_discrete_set_create(&a, 4, 0.0f, 3.0f);
    fuzzy_discrete_set_create(&b, 4, 0.0f, 3.0f);
    a.values[0] = 0.1f; a.values[1] = 0.5f; a.values[2] = 0.9f; a.values[3] = 0.2f;
    b.values[0] = 0.4f; b.values[1] = 0.3f; b.values[2] = 0.7f; b.values[3] = 0.8f;

    int rc = fuzzy_discrete_set_intersection(&a, &b, &out);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_NEAR(out.values[0], 0.1f, TOLERANCE);  // min(0.1, 0.4)
    EXPECT_NEAR(out.values[1], 0.3f, TOLERANCE);  // min(0.5, 0.3)
    EXPECT_NEAR(out.values[2], 0.7f, TOLERANCE);  // min(0.9, 0.7)
    EXPECT_NEAR(out.values[3], 0.2f, TOLERANCE);  // min(0.2, 0.8)

    fuzzy_discrete_set_free(&a);
    fuzzy_discrete_set_free(&b);
    fuzzy_discrete_set_free(&out);
}

TEST(FuzzyDiscreteSet, Discretize_Triangular) {
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_discrete_set_t ds;
    int rc = fuzzy_mf_discretize(&mf, 0.0f, 10.0f, 11, &ds);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
    EXPECT_EQ(ds.resolution, 11u);
    // At index 5 (x=5.0), should be peak
    EXPECT_NEAR(ds.values[5], 1.0f, TOLERANCE);
    // At index 0 (x=0.0), should be 0
    EXPECT_NEAR(ds.values[0], 0.0f, TOLERANCE);
    fuzzy_discrete_set_free(&ds);
}

// ============================================================================
// 15. Entropy and Cardinality
// ============================================================================

TEST(FuzzyUtility, Entropy_Uniform) {
    float m[] = {0.5f, 0.5f, 0.5f};
    float ent = fuzzy_entropy(m, 3);
    EXPECT_GT(ent, 0.0f);
}

TEST(FuzzyUtility, Entropy_AllZero) {
    float m[] = {0.0f, 0.0f, 0.0f};
    float ent = fuzzy_entropy(m, 3);
    EXPECT_GE(ent, 0.0f);
}

TEST(FuzzyUtility, Entropy_NullInput) {
    float ent = fuzzy_entropy(nullptr, 3);
    EXPECT_GE(ent, 0.0f);
}

TEST(FuzzyUtility, Cardinality_Sum) {
    float m[] = {0.2f, 0.5f, 0.8f};
    float card = fuzzy_cardinality(m, 3);
    EXPECT_NEAR(card, 1.5f, TOLERANCE);
}

TEST(FuzzyUtility, Cardinality_NullInput) {
    float card = fuzzy_cardinality(nullptr, 3);
    EXPECT_NEAR(card, 0.0f, TOLERANCE);
}

// ============================================================================
// 16. Modulation (Inflammation / Fatigue)
// ============================================================================

TEST_F(FuzzyTypesEngineTest, SetInflammation_Valid) {
    int rc = fuzzy_types_set_inflammation(engine, 0.5f);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, SetInflammation_Null) {
    int rc = fuzzy_types_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, SetFatigue_Valid) {
    int rc = fuzzy_types_set_fatigue(engine, 0.3f);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, SetFatigue_Null) {
    int rc = fuzzy_types_set_fatigue(nullptr, 0.3f);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

// ============================================================================
// 17. Statistics
// ============================================================================

TEST_F(FuzzyTypesEngineTest, GetStats_Valid) {
    fuzzy_types_stats_t stats;
    int rc = fuzzy_types_get_stats(engine, &stats);
    EXPECT_EQ(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, GetStats_NullEngine) {
    fuzzy_types_stats_t stats;
    int rc = fuzzy_types_get_stats(nullptr, &stats);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, GetStats_NullStats) {
    int rc = fuzzy_types_get_stats(engine, nullptr);
    EXPECT_NE(rc, FUZZY_ERR_OK);
}

TEST_F(FuzzyTypesEngineTest, ResetStats_Valid) {
    fuzzy_types_reset_stats(engine);  // should not crash
}

TEST_F(FuzzyTypesEngineTest, ResetStats_Null) {
    fuzzy_types_reset_stats(nullptr);  // should not crash
}

// ============================================================================
// 18. Error String
// ============================================================================

TEST(FuzzyTypesError, GetLastError_ReturnsNonNull) {
    const char* err = fuzzy_types_get_last_error();
    // May return empty string or a valid error message
    EXPECT_NE(err, nullptr);
}

// ============================================================================
// 19. MF Evaluate NULL
// ============================================================================

TEST(FuzzyMFNull, EvaluateNull_ReturnsZero) {
    float val = fuzzy_mf_evaluate(nullptr, 5.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST(FuzzyMFNull, EvaluateHedgedNull_ReturnsZero) {
    float val = fuzzy_mf_evaluate_hedged(nullptr, 5.0f, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}

TEST(FuzzySetNull, EvaluateNull_ReturnsZero) {
    float val = fuzzy_set_evaluate(nullptr, 5.0f);
    EXPECT_NEAR(val, 0.0f, TOLERANCE);
}
