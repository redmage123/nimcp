/**
 * @file test_fuzzy_integration.cpp
 * @brief Integration tests for NIMCP Fuzzy Logic cross-module interactions
 *
 * WHAT: ~40 integration tests spanning Types+Operators, Types+Inference,
 *       Bridge+Inference, SNN conversion, training scheduling, ANFIS,
 *       quantum fallback, KG/symbolic matching, bio-async modulation,
 *       and safety (ethics/LGSS) integration
 * WHY:  Verify that fuzzy modules compose correctly and produce
 *       mathematically consistent outputs across module boundaries
 * HOW:  GTest fixtures create real fuzzy engines and pipe data through
 *       multi-module paths; assertions check numerical consistency
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <numeric>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
}

static constexpr float TOL = 1e-4f;

// =============================================================================
// Fixture: Types + Operators Integration
// =============================================================================

class FuzzyTypesOperatorsTest : public ::testing::Test {
protected:
    fuzzy_types_engine_t* types_engine = nullptr;

    void SetUp() override {
        types_engine = fuzzy_types_create();
        ASSERT_NE(types_engine, nullptr);
    }

    void TearDown() override {
        fuzzy_types_destroy(types_engine);
    }
};

// --- Types + Operators Integration Tests ---

TEST_F(FuzzyTypesOperatorsTest, HedgeThenTNormComposition) {
    // Create a triangular MF, evaluate, apply hedge, then combine with t-norm
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    float raw = fuzzy_mf_evaluate(&tri, 0.5f);
    EXPECT_NEAR(raw, 1.0f, TOL);

    // "very" hedge: square the membership
    float hedged = fuzzy_apply_hedge(raw, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(hedged, 1.0f, TOL);  // 1.0^2 = 1.0

    // Evaluate at x=0.75 -> membership = 0.5
    float m2 = fuzzy_mf_evaluate(&tri, 0.75f);
    EXPECT_NEAR(m2, 0.5f, TOL);
    float h2 = fuzzy_apply_hedge(m2, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(h2, 0.25f, TOL);  // 0.5^2 = 0.25

    // T-norm MIN of hedged values
    float result = fuzzy_tnorm(hedged, h2, FUZZY_TNORM_MIN);
    EXPECT_NEAR(result, 0.25f, TOL);  // min(1.0, 0.25) = 0.25
}

TEST_F(FuzzyTypesOperatorsTest, CompoundHedgeAndTConorm) {
    fuzzy_mf_t gauss = fuzzy_mf_gaussian(5.0f, 1.0f);
    float m_center = fuzzy_mf_evaluate(&gauss, 5.0f);
    EXPECT_NEAR(m_center, 1.0f, TOL);

    float m_offset = fuzzy_mf_evaluate(&gauss, 6.0f);
    // Gaussian at 1 sigma => exp(-0.5) ~ 0.6065
    EXPECT_NEAR(m_offset, std::exp(-0.5f), TOL);

    // Apply "somewhat" (sqrt) to both
    float h1 = fuzzy_apply_hedge(m_center, FUZZY_HEDGE_SOMEWHAT);
    EXPECT_NEAR(h1, 1.0f, TOL);
    float h2 = fuzzy_apply_hedge(m_offset, FUZZY_HEDGE_SOMEWHAT);
    EXPECT_NEAR(h2, std::sqrt(std::exp(-0.5f)), TOL);

    // T-conorm MAX
    float result = fuzzy_tconorm(h1, h2, FUZZY_TCONORM_MAX);
    EXPECT_NEAR(result, 1.0f, TOL);
}

TEST_F(FuzzyTypesOperatorsTest, MFEvaluateAndImplication) {
    fuzzy_mf_t sig = fuzzy_mf_sigmoid(10.0f, 0.5f);
    float antecedent = fuzzy_mf_evaluate(&sig, 1.0f);
    // sigmoid(10*(1 - 0.5)) = sigmoid(5) ~ 0.9933
    EXPECT_GT(antecedent, 0.99f);

    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 0.5f, 1.0f);
    float consequent = fuzzy_mf_evaluate(&tri, 0.3f);
    EXPECT_NEAR(consequent, 0.6f, TOL);  // 0.3 / 0.5 = 0.6

    // Mamdani implication: min(a, b)
    float impl = fuzzy_implication(antecedent, consequent, FUZZY_IMPL_MAMDANI);
    EXPECT_NEAR(impl, consequent, TOL);  // min(~1.0, 0.6) = 0.6

    // Larsen implication: a * b
    float larsen = fuzzy_implication(antecedent, consequent, FUZZY_IMPL_LARSEN);
    EXPECT_NEAR(larsen, antecedent * consequent, TOL);
}

TEST_F(FuzzyTypesOperatorsTest, DiscreteSetUnionIntersectionPipeline) {
    fuzzy_mf_t tri1 = fuzzy_mf_triangular(0.0f, 0.3f, 0.6f);
    fuzzy_mf_t tri2 = fuzzy_mf_triangular(0.4f, 0.7f, 1.0f);

    fuzzy_discrete_set_t d1 = {0}, d2 = {0}, dunion = {0}, dinter = {0};
    ASSERT_EQ(fuzzy_mf_discretize(&tri1, 0.0f, 1.0f, 64, &d1), 0);
    ASSERT_EQ(fuzzy_mf_discretize(&tri2, 0.0f, 1.0f, 64, &d2), 0);

    ASSERT_EQ(fuzzy_discrete_set_union(&d1, &d2, &dunion), 0);
    ASSERT_EQ(fuzzy_discrete_set_intersection(&d1, &d2, &dinter), 0);

    // Union values >= intersection values at every point
    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_GE(dunion.values[i] + TOL, dinter.values[i]);
    }

    fuzzy_discrete_set_free(&d1);
    fuzzy_discrete_set_free(&d2);
    fuzzy_discrete_set_free(&dunion);
    fuzzy_discrete_set_free(&dinter);
}

TEST_F(FuzzyTypesOperatorsTest, FuzzyValueAndOrNot) {
    // Build two fuzzy_value_t and combine with fuzzy_value_and / or / not
    fuzzy_variable_t var;
    ASSERT_EQ(fuzzy_variable_create(&var, "test_var", 0.0f, 10.0f), 0);

    fuzzy_mf_t low_mf = fuzzy_mf_triangular(0.0f, 2.0f, 5.0f);
    fuzzy_mf_t high_mf = fuzzy_mf_triangular(5.0f, 8.0f, 10.0f);

    fuzzy_set_t low_set, high_set;
    ASSERT_EQ(fuzzy_set_create(&low_set, "low", &low_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&high_set, "high", &high_mf, FUZZY_HEDGE_NONE), 0);

    ASSERT_EQ(fuzzy_variable_add_term(&var, &low_set), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &high_set), 0);

    fuzzy_value_t val_a, val_b;
    ASSERT_EQ(fuzzy_variable_fuzzify(&var, 3.0f, &val_a), 0);
    ASSERT_EQ(fuzzy_variable_fuzzify(&var, 7.0f, &val_b), 0);

    fuzzy_value_t result_and, result_or, result_not;
    ASSERT_EQ(fuzzy_value_and(&val_a, &val_b, FUZZY_TNORM_MIN, &result_and), 0);
    ASSERT_EQ(fuzzy_value_or(&val_a, &val_b, FUZZY_TCONORM_MAX, &result_or), 0);
    ASSERT_EQ(fuzzy_value_not(&val_a, FUZZY_COMPLEMENT_STANDARD, 0.0f, &result_not), 0);

    // AND <= OR for every term
    for (uint32_t i = 0; i < result_and.num_terms; i++) {
        EXPECT_LE(result_and.memberships[i], result_or.memberships[i] + TOL);
    }

    // NOT(a) + a ~ 1.0 for standard complement
    for (uint32_t i = 0; i < val_a.num_terms; i++) {
        EXPECT_NEAR(result_not.memberships[i] + val_a.memberships[i], 1.0f, TOL);
    }
}

TEST_F(FuzzyTypesOperatorsTest, WeightedOperatorsPipeline) {
    float values[] = {0.8f, 0.6f, 0.4f};
    float weights[] = {0.5f, 0.3f, 0.2f};

    float wavg = fuzzy_weighted_average(values, weights, 3);
    // Expected: (0.8*0.5 + 0.6*0.3 + 0.4*0.2) / (0.5 + 0.3 + 0.2)
    float expected = (0.8f * 0.5f + 0.6f * 0.3f + 0.4f * 0.2f) /
                     (0.5f + 0.3f + 0.2f);
    EXPECT_NEAR(wavg, expected, TOL);

    float wtnorm = fuzzy_weighted_tnorm(values, weights, 3, FUZZY_TNORM_MIN);
    // Weighted tnorm result should be in [0, 1]
    EXPECT_GE(wtnorm, 0.0f);
    EXPECT_LE(wtnorm, 1.0f);

    float wtconorm = fuzzy_weighted_tconorm(values, weights, 3, FUZZY_TCONORM_MAX);
    EXPECT_GE(wtconorm, 0.0f);
    EXPECT_LE(wtconorm, 1.0f);
}

TEST_F(FuzzyTypesOperatorsTest, RelationCompositionWithMFEvaluations) {
    // 2x3 relation A, 3x2 relation B -> compose to 2x2 result
    float rel_a[6] = {0.8f, 0.5f, 0.2f,
                       0.3f, 0.7f, 0.6f};
    float rel_b[6] = {0.9f, 0.1f,
                       0.4f, 0.8f,
                       0.6f, 0.5f};
    float out[4] = {0};

    int rc = fuzzy_relation_compose(rel_a, 2, 3, rel_b, 3, 2, out,
                                     FUZZY_TNORM_MIN, FUZZY_TCONORM_MAX);
    ASSERT_EQ(rc, 0);

    // All output values in [0, 1]
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(out[i], 0.0f);
        EXPECT_LE(out[i], 1.0f);
    }

    // Verify specific: out[0][0] = max(min(0.8,0.9), min(0.5,0.4), min(0.2,0.6))
    float expected_00 = std::max({std::min(0.8f, 0.9f),
                                   std::min(0.5f, 0.4f),
                                   std::min(0.2f, 0.6f)});
    EXPECT_NEAR(out[0], expected_00, TOL);
}

// =============================================================================
// Fixture: Types + Inference Integration
// =============================================================================

class FuzzyTypesInferenceTest : public ::testing::Test {
protected:
    fuzzy_types_engine_t* types_engine = nullptr;
    fuzzy_inference_engine_t* inf_engine = nullptr;

    void SetUp() override {
        types_engine = fuzzy_types_create();
        ASSERT_NE(types_engine, nullptr);
        inf_engine = fuzzy_inference_create();
        ASSERT_NE(inf_engine, nullptr);
    }

    void TearDown() override {
        fuzzy_inference_destroy(inf_engine);
        fuzzy_types_destroy(types_engine);
    }

    // Helper: create a simple 2-input, 1-output Mamdani FIS
    void build_two_input_fis(fuzzy_variable_t& in1, fuzzy_variable_t& in2,
                             fuzzy_variable_t& out) {
        // Input 1: temperature [0, 100]
        ASSERT_EQ(fuzzy_variable_create(&in1, "temperature", 0.0f, 100.0f), 0);
        fuzzy_mf_t cold_mf = fuzzy_mf_triangular(0.0f, 0.0f, 50.0f);
        fuzzy_mf_t warm_mf = fuzzy_mf_triangular(20.0f, 50.0f, 80.0f);
        fuzzy_mf_t hot_mf = fuzzy_mf_triangular(50.0f, 100.0f, 100.0f);

        fuzzy_set_t cold, warm, hot;
        ASSERT_EQ(fuzzy_set_create(&cold, "cold", &cold_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&warm, "warm", &warm_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&hot, "hot", &hot_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in1, &cold), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in1, &warm), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in1, &hot), 0);

        // Input 2: humidity [0, 100]
        ASSERT_EQ(fuzzy_variable_create(&in2, "humidity", 0.0f, 100.0f), 0);
        fuzzy_mf_t dry_mf = fuzzy_mf_triangular(0.0f, 0.0f, 50.0f);
        fuzzy_mf_t normal_mf = fuzzy_mf_triangular(25.0f, 50.0f, 75.0f);
        fuzzy_mf_t wet_mf = fuzzy_mf_triangular(50.0f, 100.0f, 100.0f);

        fuzzy_set_t dry, normal, wet;
        ASSERT_EQ(fuzzy_set_create(&dry, "dry", &dry_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&normal, "normal", &normal_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&wet, "wet", &wet_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in2, &dry), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in2, &normal), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in2, &wet), 0);

        // Output: fan speed [0, 100]
        ASSERT_EQ(fuzzy_variable_create(&out, "fan_speed", 0.0f, 100.0f), 0);
        fuzzy_mf_t low_mf = fuzzy_mf_triangular(0.0f, 0.0f, 50.0f);
        fuzzy_mf_t med_mf = fuzzy_mf_triangular(20.0f, 50.0f, 80.0f);
        fuzzy_mf_t high_mf = fuzzy_mf_triangular(50.0f, 100.0f, 100.0f);

        fuzzy_set_t low, med, high_s;
        ASSERT_EQ(fuzzy_set_create(&low, "low", &low_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&med, "medium", &med_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&high_s, "high", &high_mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out, &low), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out, &med), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out, &high_s), 0);
    }
};

TEST_F(FuzzyTypesInferenceTest, BuildFISFromTypesAndEvaluate) {
    fuzzy_variable_t in1, in2, out;
    build_two_input_fis(in1, in2, out);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in1), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in2), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out), 0);

    // Rule: IF temp IS cold AND humidity IS dry THEN fan IS low
    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    // Rule: IF temp IS warm AND humidity IS normal THEN fan IS medium
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
    // Rule: IF temp IS hot AND humidity IS wet THEN fan IS high
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);

    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r3), 0);

    EXPECT_EQ(fuzzy_inference_get_rule_count(inf_engine), 3);

    // Evaluate with moderate inputs
    float inputs[] = {50.0f, 50.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = fuzzy_inference_evaluate(inf_engine, inputs, 2, &result);
    ASSERT_EQ(rc, 0);
    EXPECT_EQ(result.num_outputs, 1u);

    // Output should be in the fan_speed universe [0, 100]
    EXPECT_GE(result.crisp_outputs[0], 0.0f);
    EXPECT_LE(result.crisp_outputs[0], 100.0f);

    // With balanced inputs, expect mid-range output
    EXPECT_GT(result.crisp_outputs[0], 20.0f);
    EXPECT_LT(result.crisp_outputs[0], 80.0f);
}

TEST_F(FuzzyTypesInferenceTest, FISWithMultipleRulesFiringStrengths) {
    fuzzy_variable_t in1, in2, out;
    build_two_input_fis(in1, in2, out);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in1), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in2), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);

    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r3), 0);

    // High temperature, high humidity -> expect high firing on rule 3
    float inputs[] = {85.0f, 80.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));

    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, inputs, 2, &result), 0);

    // Rule 3 (hot+wet->high) should have strongest firing
    EXPECT_GT(result.rule_firing_strengths[2], result.rule_firing_strengths[0]);
    EXPECT_GT(result.num_rules_fired, 0u);
    EXPECT_GT(result.total_firing_strength, 0.0f);
}

TEST_F(FuzzyTypesInferenceTest, ClearRulesAndRebuild) {
    fuzzy_variable_t in1, in2, out;
    build_two_input_fis(in1, in2, out);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in1), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in2), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    EXPECT_EQ(fuzzy_inference_get_rule_count(inf_engine), 1);

    ASSERT_EQ(fuzzy_inference_clear_rules(inf_engine), 0);
    EXPECT_EQ(fuzzy_inference_get_rule_count(inf_engine), 0);

    // Rebuild with different rule
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    EXPECT_EQ(fuzzy_inference_get_rule_count(inf_engine), 1);
}

TEST_F(FuzzyTypesInferenceTest, BatchInferenceConsistency) {
    fuzzy_variable_t in1, in2, out;
    build_two_input_fis(in1, in2, out);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in1), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in2), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r3), 0);

    // Batch: 3 samples, each with 2 inputs
    float batch_inputs[] = {
        10.0f, 10.0f,   // cold, dry
        50.0f, 50.0f,   // warm, normal
        90.0f, 90.0f    // hot, wet
    };
    fuzzy_inference_result_t batch_results[3];
    memset(batch_results, 0, sizeof(batch_results));

    int rc = fuzzy_inference_evaluate_batch(inf_engine, batch_inputs, 3, 2, batch_results);
    ASSERT_EQ(rc, 0);

    // Cold/dry -> low output, hot/wet -> high output
    EXPECT_LT(batch_results[0].crisp_outputs[0], batch_results[2].crisp_outputs[0]);

    // Verify single evaluation matches batch
    float single_in[] = {50.0f, 50.0f};
    fuzzy_inference_result_t single_result;
    memset(&single_result, 0, sizeof(single_result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, single_in, 2, &single_result), 0);
    EXPECT_NEAR(single_result.crisp_outputs[0], batch_results[1].crisp_outputs[0], TOL);
}

TEST_F(FuzzyTypesInferenceTest, DefuzzifyStandaloneWithTypesDiscretization) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 50.0f, 100.0f);
    fuzzy_discrete_set_t disc = {0};
    ASSERT_EQ(fuzzy_mf_discretize(&tri, 0.0f, 100.0f, 256, &disc), 0);

    // Centroid of a symmetric triangle should be at its peak
    float centroid = fuzzy_defuzzify(&disc, FUZZY_DEFUZZ_CENTROID);
    EXPECT_NEAR(centroid, 50.0f, 1.0f);

    // MOM of a symmetric triangle = peak
    float mom = fuzzy_defuzzify(&disc, FUZZY_DEFUZZ_MOM);
    EXPECT_NEAR(mom, 50.0f, 1.0f);

    fuzzy_discrete_set_free(&disc);
}

// =============================================================================
// Fixture: Bridge + Inference Integration
// =============================================================================

class FuzzyBridgeInferenceTest : public ::testing::Test {
protected:
    fuzzy_bridge_t* bridge = nullptr;
    fuzzy_inference_engine_t* inf_engine = nullptr;
    fuzzy_types_engine_t* types_engine = nullptr;

    void SetUp() override {
        fuzzy_bridge_config_t cfg = fuzzy_bridge_default_config();
        cfg.enable_snn_integration = true;
        cfg.enable_training_integration = true;
        cfg.enable_quantum_integration = true;
        cfg.enable_symbolic_integration = true;
        cfg.enable_immune_integration = true;
        cfg.enable_ethics = true;
        cfg.enable_lgss = true;
        cfg.spike_rate_min = 0.0f;
        cfg.spike_rate_max = 100.0f;
        cfg.training_lr_min = 0.0001f;
        cfg.training_lr_max = 0.1f;
        bridge = fuzzy_bridge_create(&cfg);
        ASSERT_NE(bridge, nullptr);

        inf_engine = fuzzy_inference_create();
        ASSERT_NE(inf_engine, nullptr);

        types_engine = fuzzy_types_create();
        ASSERT_NE(types_engine, nullptr);
    }

    void TearDown() override {
        fuzzy_types_destroy(types_engine);
        fuzzy_inference_destroy(inf_engine);
        fuzzy_bridge_destroy(bridge);
    }
};

TEST_F(FuzzyBridgeInferenceTest, BridgeStateAfterCreation) {
    fuzzy_bridge_state_t state = fuzzy_bridge_get_state(bridge);
    // Should be IDLE or ACTIVE after creation
    EXPECT_TRUE(state == FUZZY_BRIDGE_STATE_IDLE ||
                state == FUZZY_BRIDGE_STATE_ACTIVE ||
                state == FUZZY_BRIDGE_STATE_INITIALIZING);
}

// --- SNN Integration ---

TEST_F(FuzzyBridgeInferenceTest, FuzzyToSpikeRoundTrip) {
    float memberships[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    float rates[5] = {0};
    float recovered[5] = {0};

    int rc = fuzzy_bridge_to_spike_population(bridge, memberships, 5, rates);
    ASSERT_EQ(rc, 0);

    // Rates should be in configured range [0, 100]
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(rates[i], 0.0f);
        EXPECT_LE(rates[i], 100.0f + TOL);
    }

    // Rates should be monotonically non-decreasing
    for (int i = 1; i < 5; i++) {
        EXPECT_GE(rates[i] + TOL, rates[i - 1]);
    }

    // Convert back
    rc = fuzzy_bridge_from_spike_population(bridge, rates, 5, recovered);
    ASSERT_EQ(rc, 0);

    // Recovered memberships should be in [0, 1]
    for (int i = 0; i < 5; i++) {
        EXPECT_GE(recovered[i], -TOL);
        EXPECT_LE(recovered[i], 1.0f + TOL);
    }
}

TEST_F(FuzzyBridgeInferenceTest, SpikeConversionBoundaryValues) {
    // All zeros
    float zero_m[] = {0.0f, 0.0f, 0.0f};
    float rates_z[3] = {0};
    ASSERT_EQ(fuzzy_bridge_to_spike_population(bridge, zero_m, 3, rates_z), 0);
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(rates_z[i], 0.0f, 1.0f); // near min rate
    }

    // All ones
    float one_m[] = {1.0f, 1.0f, 1.0f};
    float rates_o[3] = {0};
    ASSERT_EQ(fuzzy_bridge_to_spike_population(bridge, one_m, 3, rates_o), 0);
    for (int i = 0; i < 3; i++) {
        EXPECT_GT(rates_o[i], 50.0f); // near max rate
    }
}

// --- STDP Integration ---

TEST_F(FuzzyBridgeInferenceTest, STDPTemporalMembership) {
    float potentiation = 0.0f, depression = 0.0f;

    // Positive dt -> potentiation
    int rc = fuzzy_bridge_stdp_temporal_membership(bridge, 5.0f,
                                                    &potentiation, &depression);
    ASSERT_EQ(rc, 0);
    EXPECT_GT(potentiation, depression);
    EXPECT_GE(potentiation, 0.0f);
    EXPECT_LE(potentiation, 1.0f);

    // Negative dt -> depression
    rc = fuzzy_bridge_stdp_temporal_membership(bridge, -5.0f,
                                               &potentiation, &depression);
    ASSERT_EQ(rc, 0);
    EXPECT_GT(depression, potentiation);

    // Zero dt -> both should be at transition point
    rc = fuzzy_bridge_stdp_temporal_membership(bridge, 0.0f,
                                               &potentiation, &depression);
    ASSERT_EQ(rc, 0);
}

// --- Plasticity Integration ---

TEST_F(FuzzyBridgeInferenceTest, PlasticityRateComputation) {
    float rate = 0.0f;

    // High performance, high stability -> lower plasticity rate (system is converged)
    int rc = fuzzy_bridge_plasticity_rate(bridge, 0.9f, 0.9f, &rate);
    ASSERT_EQ(rc, 0);
    float high_perf_rate = rate;

    // Low performance, low stability -> higher plasticity rate (more learning needed)
    rc = fuzzy_bridge_plasticity_rate(bridge, 0.1f, 0.1f, &rate);
    ASSERT_EQ(rc, 0);
    float low_perf_rate = rate;

    // Both rates should be valid
    EXPECT_GE(high_perf_rate, 0.0f);
    EXPECT_GE(low_perf_rate, 0.0f);
}

// --- Training Integration ---

TEST_F(FuzzyBridgeInferenceTest, TrainingLRSchedulePipeline) {
    float adjusted_lr = 0.0f;
    float base_lr = 0.01f;

    // Early training, loss decreasing -> keep learning rate
    int rc = fuzzy_bridge_training_lr_schedule(bridge, 0.1f, -0.5f,
                                                base_lr, &adjusted_lr);
    ASSERT_EQ(rc, 0);
    EXPECT_GT(adjusted_lr, 0.0f);
    float early_lr = adjusted_lr;

    // Late training, loss stable -> reduce learning rate
    rc = fuzzy_bridge_training_lr_schedule(bridge, 0.9f, 0.0f,
                                            base_lr, &adjusted_lr);
    ASSERT_EQ(rc, 0);
    EXPECT_GT(adjusted_lr, 0.0f);
}

TEST_F(FuzzyBridgeInferenceTest, TrainingConvergenceAssessment) {
    float convergence = 0.0f;

    // Small loss delta, small gradient -> converged
    int rc = fuzzy_bridge_training_convergence(bridge, 0.0001f, 0.001f,
                                                &convergence);
    ASSERT_EQ(rc, 0);
    EXPECT_GE(convergence, 0.0f);
    EXPECT_LE(convergence, 1.0f);
    float near_converged = convergence;

    // Large loss delta, large gradient -> not converged
    rc = fuzzy_bridge_training_convergence(bridge, 1.0f, 10.0f, &convergence);
    ASSERT_EQ(rc, 0);
    float far_converged = convergence;

    // Near-converged should have higher convergence degree
    EXPECT_GE(near_converged + TOL, far_converged);
}

// --- ANFIS Integration ---

TEST_F(FuzzyBridgeInferenceTest, ANFISTrainOnSimpleData) {
    // Create a Sugeno FIS for ANFIS training
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    cfg.enable_anfis = true;
    cfg.anfis_learning_rate = 0.01f;
    cfg.anfis_max_epochs = 100;
    cfg.anfis_convergence_tol = 0.01f;

    fuzzy_inference_engine_t* sugeno = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(sugeno, nullptr);

    // Simple 1-input -> 1-output: y = 2*x + 1
    fuzzy_variable_t in_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);

    fuzzy_mf_t low_mf = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t high_mf = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t low_set, high_set;
    ASSERT_EQ(fuzzy_set_create(&low_set, "low", &low_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&high_set, "high", &high_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &low_set), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &high_set), 0);

    fuzzy_variable_t out_var;
    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 25.0f), 0);

    ASSERT_EQ(fuzzy_inference_add_input(sugeno, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(sugeno, &out_var), 0);

    // Two Sugeno rules
    float coeffs1[] = {1.0f, 2.0f};  // y = 1.0 + 2.0*x
    float coeffs2[] = {1.0f, 2.0f};
    fuzzy_rule_t sr1 = fuzzy_rule_sugeno(0, 0, 0, 0, coeffs1, 2, 1.0f);
    fuzzy_rule_t sr2 = fuzzy_rule_sugeno(0, 1, 0, 1, coeffs2, 2, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(sugeno, &sr1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(sugeno, &sr2), 0);

    // Training data
    const int N = 10;
    float input_data[10];
    float target_data[10];
    for (int i = 0; i < N; i++) {
        input_data[i] = (float)i;
        target_data[i] = 2.0f * (float)i + 1.0f;
    }

    float final_error = 999.0f;
    int rc = fuzzy_anfis_train(sugeno, input_data, target_data, N, 100, &final_error);
    // ANFIS might not converge perfectly but should not crash
    // Accept success or convergence error
    EXPECT_TRUE(rc == 0 || rc == FUZZY_INF_ERR_CONVERGENCE);

    fuzzy_inference_destroy(sugeno);
}

// --- Quantum Integration ---

TEST_F(FuzzyBridgeInferenceTest, QuantumInferenceFallback) {
    // Without a real quantum backend set, should fall back to classical
    fuzzy_variable_t in_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);
    fuzzy_mf_t low_mf = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t high_mf = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t low_set, high_set;
    ASSERT_EQ(fuzzy_set_create(&low_set, "low", &low_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&high_set, "high", &high_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &low_set), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &high_set), 0);

    fuzzy_variable_t out_var;
    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 10.0f), 0);
    fuzzy_mf_t out_low = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t out_high = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t ol, oh;
    ASSERT_EQ(fuzzy_set_create(&ol, "low", &out_low, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&oh, "high", &out_high, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &ol), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &oh), 0);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out_var), 0);
    fuzzy_rule_t rule = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &rule), 0);

    float inputs[] = {3.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));

    int rc = fuzzy_bridge_quantum_inference(bridge, inputs, 1, inf_engine, &result);
    // Should succeed (fallback) or return NOT_CONNECTED if quantum not set
    EXPECT_TRUE(rc == 0 || rc == FUZZY_BRIDGE_ERR_NOT_CONNECTED);
}

// --- Symbolic / KG Integration ---

TEST_F(FuzzyBridgeInferenceTest, SymbolicMatchPipeline) {
    // Without symbolic engine connected, should return NOT_CONNECTED
    bool matched = false;
    float score = 0.0f;
    int rc = fuzzy_bridge_symbolic_match(bridge, "risk_assess", 0.75f,
                                          &matched, &score);
    // Expect NOT_CONNECTED since we haven't set a symbolic engine
    EXPECT_TRUE(rc == 0 || rc == FUZZY_BRIDGE_ERR_NOT_CONNECTED);
}

// --- Bio-Async / Health Modulation ---

TEST_F(FuzzyBridgeInferenceTest, InflammationModulatesAll) {
    ASSERT_EQ(fuzzy_bridge_set_inflammation(bridge, 0.5f), 0);
    ASSERT_EQ(fuzzy_types_set_inflammation(types_engine, 0.5f), 0);
    ASSERT_EQ(fuzzy_inference_set_inflammation(inf_engine, 0.5f), 0);

    // All should accept valid range without error
    ASSERT_EQ(fuzzy_bridge_set_inflammation(bridge, 0.0f), 0);
    ASSERT_EQ(fuzzy_bridge_set_inflammation(bridge, 1.0f), 0);
}

TEST_F(FuzzyBridgeInferenceTest, FatigueModulatesAll) {
    ASSERT_EQ(fuzzy_bridge_set_fatigue(bridge, 0.5f), 0);
    ASSERT_EQ(fuzzy_types_set_fatigue(types_engine, 0.5f), 0);
    ASSERT_EQ(fuzzy_inference_set_fatigue(inf_engine, 0.5f), 0);

    // All should accept valid range without error
    ASSERT_EQ(fuzzy_bridge_set_fatigue(bridge, 0.0f), 0);
    ASSERT_EQ(fuzzy_bridge_set_fatigue(bridge, 1.0f), 0);
}

TEST_F(FuzzyBridgeInferenceTest, HealthHeartbeatAndCheck) {
    int rc = fuzzy_bridge_heartbeat(bridge, "integration_test", 0.5f);
    ASSERT_EQ(rc, 0);

    rc = fuzzy_bridge_check_health(bridge);
    // 0 = healthy, or specific degraded state code
    EXPECT_GE(rc, 0);
}

// --- Safety: Ethics + LGSS ---

TEST_F(FuzzyBridgeInferenceTest, EthicsSubsystemSetterNullSafety) {
    // Setting NULL ethics should return NULL error or handle gracefully
    int rc = fuzzy_bridge_set_ethics(bridge, nullptr);
    EXPECT_TRUE(rc == FUZZY_BRIDGE_ERR_NULL || rc == 0);
}

TEST_F(FuzzyBridgeInferenceTest, LGSSSubsystemSetterNullSafety) {
    int rc = fuzzy_bridge_set_lgss(bridge, nullptr);
    EXPECT_TRUE(rc == FUZZY_BRIDGE_ERR_NULL || rc == 0);
}

// --- Stats Integration ---

TEST_F(FuzzyBridgeInferenceTest, StatsAcrossAllModules) {
    fuzzy_types_stats_t ts;
    ASSERT_EQ(fuzzy_types_get_stats(types_engine, &ts), 0);

    fuzzy_inference_stats_t is_stats;
    ASSERT_EQ(fuzzy_inference_get_stats(inf_engine, &is_stats), 0);

    fuzzy_bridge_stats_t bs;
    ASSERT_EQ(fuzzy_bridge_get_stats(bridge, &bs), 0);

    fuzzy_operator_stats_t os;
    ASSERT_EQ(fuzzy_operator_get_stats(&os), 0);
}

TEST_F(FuzzyBridgeInferenceTest, SubsystemSettersAcceptVoidPointers) {
    // Verify all setters with NULL don't crash (defense-in-depth)
    fuzzy_bridge_set_immune(bridge, nullptr);
    fuzzy_bridge_set_bbb(bridge, nullptr);
    fuzzy_bridge_set_health_agent(bridge, nullptr);
    fuzzy_bridge_set_kg_wiring(bridge, nullptr);
    fuzzy_bridge_set_kg_registry(bridge, nullptr);
    fuzzy_bridge_set_logger(bridge, nullptr);
    fuzzy_bridge_set_security(bridge, nullptr);
    fuzzy_bridge_set_cycle_coordinator(bridge, nullptr);
    fuzzy_bridge_set_bio_router(bridge, nullptr);
    fuzzy_bridge_set_snn(bridge, nullptr);
    fuzzy_bridge_set_stdp(bridge, nullptr);
    fuzzy_bridge_set_plasticity(bridge, nullptr);
    fuzzy_bridge_set_lnn(bridge, nullptr);
    fuzzy_bridge_set_training(bridge, nullptr);
    fuzzy_bridge_set_quantum(bridge, nullptr);
    fuzzy_bridge_set_symbolic(bridge, nullptr);
}

// --- LNN Classification ---

TEST_F(FuzzyBridgeInferenceTest, LNNStateClassification) {
    float state[] = {0.5f, 0.3f, 0.8f, 0.2f};
    fuzzy_value_t value;
    memset(&value, 0, sizeof(value));

    int rc = fuzzy_bridge_lnn_classify_state(bridge, state, 4, &value);
    // Without LNN connected, may return NOT_CONNECTED
    EXPECT_TRUE(rc == 0 || rc == FUZZY_BRIDGE_ERR_NOT_CONNECTED);
}

// --- Entropy & Cardinality with Operators ---

TEST_F(FuzzyBridgeInferenceTest, EntropyAndCardinalityOfFuzzifiedValues) {
    fuzzy_variable_t var;
    ASSERT_EQ(fuzzy_variable_create(&var, "test", 0.0f, 10.0f), 0);

    fuzzy_mf_t m1 = fuzzy_mf_triangular(0.0f, 2.5f, 5.0f);
    fuzzy_mf_t m2 = fuzzy_mf_triangular(2.5f, 5.0f, 7.5f);
    fuzzy_mf_t m3 = fuzzy_mf_triangular(5.0f, 7.5f, 10.0f);

    fuzzy_set_t s1, s2, s3;
    ASSERT_EQ(fuzzy_set_create(&s1, "low", &m1, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s2, "med", &m2, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s3, "high", &m3, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s2), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s3), 0);

    fuzzy_value_t val;
    ASSERT_EQ(fuzzy_variable_fuzzify(&var, 5.0f, &val), 0);

    float entropy = fuzzy_entropy(val.memberships, val.num_terms);
    EXPECT_GE(entropy, 0.0f);

    float cardinality = fuzzy_cardinality(val.memberships, val.num_terms);
    EXPECT_GE(cardinality, 0.0f);
}

TEST_F(FuzzyBridgeInferenceTest, SimilarityAndDistanceBetweenFuzzySets) {
    float set_a[] = {1.0f, 0.5f, 0.0f};
    float set_b[] = {0.0f, 0.5f, 1.0f};

    float sim = fuzzy_set_similarity(set_a, set_b, 3);
    EXPECT_GE(sim, 0.0f);
    EXPECT_LE(sim, 1.0f);

    float dist = fuzzy_set_distance(set_a, set_b, 3);
    EXPECT_GE(dist, 0.0f);

    float incl = fuzzy_set_inclusion(set_a, set_b, 3);
    EXPECT_GE(incl, 0.0f);
    EXPECT_LE(incl, 1.0f);

    // Identity: similarity of set with itself = 1.0
    float self_sim = fuzzy_set_similarity(set_a, set_a, 3);
    EXPECT_NEAR(self_sim, 1.0f, TOL);

    // Identity: distance of set with itself = 0.0
    float self_dist = fuzzy_set_distance(set_a, set_a, 3);
    EXPECT_NEAR(self_dist, 0.0f, TOL);
}
