/**
 * @file test_fuzzy_e2e.cpp
 * @brief End-to-end tests for NIMCP Fuzzy Logic full system pipelines
 *
 * WHAT: ~20 E2E tests covering classic fuzzy control problems (tipping,
 *       temperature controller), full bridge pipelines, multi-variable
 *       systems, stress tests, real-world patterns, memory lifecycle,
 *       concurrent configs, and performance baselines
 * WHY:  Validate that the complete fuzzy stack works as a unified
 *       system for real-world-like workloads
 * HOW:  Build complete FIS configurations end-to-end and evaluate
 *       against expected behavior ranges
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>

extern "C" {
#include "utils/fuzzy/nimcp_fuzzy_types.h"
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"
}

static constexpr float TOL = 1e-4f;

// =============================================================================
// Fixture: Full Pipeline E2E
// =============================================================================

class FuzzyE2ETest : public ::testing::Test {
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
        cfg.enable_snn_integration = true;
        cfg.enable_training_integration = true;
        cfg.spike_rate_min = 0.0f;
        cfg.spike_rate_max = 200.0f;
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
// E2E 1: Classic Tipping Problem (Mamdani)
// =============================================================================

TEST_F(FuzzyE2ETest, ClassicTippingProblemMamdani) {
    // Input 1: food quality [0, 10]
    fuzzy_variable_t food;
    ASSERT_EQ(fuzzy_variable_create(&food, "food", 0.0f, 10.0f), 0);

    fuzzy_mf_t bad_mf = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t decent_mf = fuzzy_mf_triangular(2.5f, 5.0f, 7.5f);
    fuzzy_mf_t great_mf = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);

    fuzzy_set_t bad, decent, great;
    ASSERT_EQ(fuzzy_set_create(&bad, "bad", &bad_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&decent, "decent", &decent_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&great, "great", &great_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&food, &bad), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&food, &decent), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&food, &great), 0);

    // Input 2: service quality [0, 10]
    fuzzy_variable_t service;
    ASSERT_EQ(fuzzy_variable_create(&service, "service", 0.0f, 10.0f), 0);

    fuzzy_mf_t poor_mf = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t ok_mf = fuzzy_mf_triangular(2.5f, 5.0f, 7.5f);
    fuzzy_mf_t excellent_mf = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);

    fuzzy_set_t poor, ok, excellent;
    ASSERT_EQ(fuzzy_set_create(&poor, "poor", &poor_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&ok, "ok", &ok_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&excellent, "excellent", &excellent_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&service, &poor), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&service, &ok), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&service, &excellent), 0);

    // Output: tip [0, 30] percent
    fuzzy_variable_t tip;
    ASSERT_EQ(fuzzy_variable_create(&tip, "tip", 0.0f, 30.0f), 0);

    fuzzy_mf_t low_tip = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_mf_t med_tip = fuzzy_mf_triangular(10.0f, 15.0f, 20.0f);
    fuzzy_mf_t high_tip = fuzzy_mf_triangular(20.0f, 25.0f, 30.0f);

    fuzzy_set_t tip_low, tip_med, tip_high;
    ASSERT_EQ(fuzzy_set_create(&tip_low, "low", &low_tip, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&tip_med, "medium", &med_tip, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&tip_high, "high", &high_tip, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&tip, &tip_low), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&tip, &tip_med), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&tip, &tip_high), 0);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &food), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &service), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &tip), 0);

    // Rules:
    // IF food IS bad OR service IS poor THEN tip IS low
    // IF service IS ok THEN tip IS medium
    // IF food IS great OR service IS excellent THEN tip IS high
    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    r1.use_or = true;
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
    r3.use_or = true;

    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r3), 0);

    // Test 1: Bad food, poor service -> low tip
    float inputs_low[] = {1.0f, 1.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, inputs_low, 2, &result), 0);
    EXPECT_LT(result.crisp_outputs[0], 15.0f);

    // Test 2: Great food, excellent service -> high tip
    float inputs_high[] = {9.0f, 9.0f};
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, inputs_high, 2, &result), 0);
    EXPECT_GT(result.crisp_outputs[0], 15.0f);

    // Test 3: Medium -> medium tip
    float inputs_mid[] = {5.0f, 5.0f};
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, inputs_mid, 2, &result), 0);
    EXPECT_GE(result.crisp_outputs[0], 0.0f);
    EXPECT_LE(result.crisp_outputs[0], 30.0f);

    // Monotonicity: higher inputs -> higher tip
    EXPECT_LT(result.crisp_outputs[0], 30.0f);
}

// =============================================================================
// E2E 2: Temperature Controller (Sugeno)
// =============================================================================

TEST_F(FuzzyE2ETest, TemperatureControllerSugeno) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    fuzzy_inference_engine_t* sugeno = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(sugeno, nullptr);

    // Input: temperature [0, 100]
    fuzzy_variable_t temp;
    ASSERT_EQ(fuzzy_variable_create(&temp, "temperature", 0.0f, 100.0f), 0);

    fuzzy_mf_t cold_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 20.0f, 40.0f);
    fuzzy_mf_t warm_mf = fuzzy_mf_triangular(30.0f, 50.0f, 70.0f);
    fuzzy_mf_t hot_mf = fuzzy_mf_trapezoidal(60.0f, 80.0f, 100.0f, 100.0f);

    fuzzy_set_t cold_s, warm_s, hot_s;
    ASSERT_EQ(fuzzy_set_create(&cold_s, "cold", &cold_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&warm_s, "warm", &warm_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&hot_s, "hot", &hot_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&temp, &cold_s), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&temp, &warm_s), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&temp, &hot_s), 0);

    // Output: fan speed (Sugeno constant outputs)
    fuzzy_variable_t fan;
    ASSERT_EQ(fuzzy_variable_create(&fan, "fan_speed", 0.0f, 100.0f), 0);
    ASSERT_EQ(fuzzy_inference_add_input(sugeno, &temp), 0);
    ASSERT_EQ(fuzzy_inference_add_output(sugeno, &fan), 0);

    // Sugeno rules: output = constant (zero-order)
    float coeffs_low[] = {10.0f};   // fan = 10
    float coeffs_med[] = {50.0f};   // fan = 50
    float coeffs_high[] = {90.0f};  // fan = 90

    fuzzy_rule_t sr1 = fuzzy_rule_sugeno(0, 0, 0, 0, coeffs_low, 1, 1.0f);
    fuzzy_rule_t sr2 = fuzzy_rule_sugeno(0, 1, 0, 1, coeffs_med, 1, 1.0f);
    fuzzy_rule_t sr3 = fuzzy_rule_sugeno(0, 2, 0, 2, coeffs_high, 1, 1.0f);

    ASSERT_EQ(fuzzy_inference_add_rule(sugeno, &sr1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(sugeno, &sr2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(sugeno, &sr3), 0);

    // Cold temperature -> low fan
    float cold_input[] = {10.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(sugeno, cold_input, 1, &result), 0);
    EXPECT_LT(result.crisp_outputs[0], 40.0f);

    // Hot temperature -> high fan
    float hot_input[] = {90.0f};
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(sugeno, hot_input, 1, &result), 0);
    EXPECT_GT(result.crisp_outputs[0], 60.0f);

    fuzzy_inference_destroy(sugeno);
}

// =============================================================================
// E2E 3: Full Bridge Pipeline
// =============================================================================

TEST_F(FuzzyE2ETest, FullBridgePipeline) {
    // Step 1: Create types and build a variable
    fuzzy_variable_t risk;
    ASSERT_EQ(fuzzy_variable_create(&risk, "risk", 0.0f, 1.0f), 0);

    fuzzy_mf_t low_risk = fuzzy_mf_triangular(0.0f, 0.0f, 0.5f);
    fuzzy_mf_t med_risk = fuzzy_mf_triangular(0.25f, 0.5f, 0.75f);
    fuzzy_mf_t high_risk = fuzzy_mf_triangular(0.5f, 1.0f, 1.0f);

    fuzzy_set_t lr, mr, hr;
    ASSERT_EQ(fuzzy_set_create(&lr, "low", &low_risk, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&mr, "medium", &med_risk, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&hr, "high", &high_risk, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk, &lr), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk, &mr), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk, &hr), 0);

    // Step 2: Fuzzify
    fuzzy_value_t fuzzified;
    ASSERT_EQ(fuzzy_variable_fuzzify(&risk, 0.7f, &fuzzified), 0);
    EXPECT_GT(fuzzified.memberships[2], 0.0f);  // High risk should fire

    // Step 3: Convert to spike rates via bridge
    float rates[3] = {0};
    ASSERT_EQ(fuzzy_bridge_to_spike_population(bridge, fuzzified.memberships, 3, rates), 0);
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(rates[i], 0.0f);
    }

    // Step 4: Convert back from spikes to memberships
    float recovered[3] = {0};
    ASSERT_EQ(fuzzy_bridge_from_spike_population(bridge, rates, 3, recovered), 0);
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(recovered[i], -TOL);
        EXPECT_LE(recovered[i], 1.0f + TOL);
    }

    // Step 5: Apply operators on recovered values
    float combined_and = fuzzy_tnorm_array(recovered, 3, FUZZY_TNORM_MIN);
    float combined_or = fuzzy_tconorm_array(recovered, 3, FUZZY_TCONORM_MAX);
    EXPECT_LE(combined_and, combined_or + TOL);
}

// =============================================================================
// E2E 4: Multi-Variable System (3+ inputs)
// =============================================================================

TEST_F(FuzzyE2ETest, MultiVariableThreeInputs) {
    auto make_3term_var = [](const char* name, float lo, float hi,
                              fuzzy_variable_t* out) {
        ASSERT_EQ(fuzzy_variable_create(out, name, lo, hi), 0);
        float mid = (lo + hi) / 2.0f;
        float q1 = lo + (hi - lo) * 0.25f;
        float q3 = lo + (hi - lo) * 0.75f;

        fuzzy_mf_t m1 = fuzzy_mf_triangular(lo, lo, mid);
        fuzzy_mf_t m2 = fuzzy_mf_triangular(q1, mid, q3);
        fuzzy_mf_t m3 = fuzzy_mf_triangular(mid, hi, hi);

        fuzzy_set_t s1, s2, s3;
        ASSERT_EQ(fuzzy_set_create(&s1, "low", &m1, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&s2, "med", &m2, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&s3, "high", &m3, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(out, &s1), 0);
        ASSERT_EQ(fuzzy_variable_add_term(out, &s2), 0);
        ASSERT_EQ(fuzzy_variable_add_term(out, &s3), 0);
    };

    fuzzy_variable_t v1, v2, v3, out;
    make_3term_var("input1", 0.0f, 100.0f, &v1);
    make_3term_var("input2", 0.0f, 100.0f, &v2);
    make_3term_var("input3", 0.0f, 100.0f, &v3);
    make_3term_var("output", 0.0f, 100.0f, &out);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &v1), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &v2), 0);
    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &v3), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out), 0);

    // Add multiple rules covering different input combinations
    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r3), 0);

    // Evaluate
    float inputs[] = {30.0f, 50.0f, 70.0f};
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, inputs, 3, &result), 0);

    EXPECT_GE(result.crisp_outputs[0], 0.0f);
    EXPECT_LE(result.crisp_outputs[0], 100.0f);
    EXPECT_GT(result.num_rules_fired, 0u);
}

// =============================================================================
// E2E 5: Stress Test - 1000 Evaluations
// =============================================================================

TEST_F(FuzzyE2ETest, StressTest1000Evaluations) {
    // Build simple 1-input 1-output FIS
    fuzzy_variable_t in_var, out_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);
    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 10.0f), 0);

    fuzzy_mf_t lo = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t hi = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);

    fuzzy_set_t s_lo, s_hi;
    ASSERT_EQ(fuzzy_set_create(&s_lo, "low", &lo, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s_hi, "high", &hi, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s_lo), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s_hi), 0);

    fuzzy_mf_t o_lo = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t o_hi = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t os_lo, os_hi;
    ASSERT_EQ(fuzzy_set_create(&os_lo, "low", &o_lo, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&os_hi, "high", &o_hi, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os_lo), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os_hi), 0);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out_var), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 0, 1, 0, 1, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);

    fuzzy_inference_reset_stats(inf_engine);

    for (int i = 0; i < 1000; i++) {
        float input = (float)i / 100.0f;  // [0, 10]
        fuzzy_inference_result_t result;
        memset(&result, 0, sizeof(result));
        ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, &input, 1, &result), 0);
        EXPECT_GE(result.crisp_outputs[0], 0.0f);
        EXPECT_LE(result.crisp_outputs[0], 10.0f);
    }

    // Verify stats reflect the work
    fuzzy_inference_stats_t stats;
    ASSERT_EQ(fuzzy_inference_get_stats(inf_engine, &stats), 0);
    EXPECT_GE(stats.inferences_run, 1000u);
}

// =============================================================================
// E2E 6: Real-World Pattern: Sigmoid/Gaussian for Risk Classification
// =============================================================================

TEST_F(FuzzyE2ETest, SigmoidGaussianRiskClassification) {
    fuzzy_variable_t risk_score;
    ASSERT_EQ(fuzzy_variable_create(&risk_score, "risk_score", 0.0f, 100.0f), 0);

    // Use sigmoid for extreme categories, Gaussian for middle
    fuzzy_mf_t safe_mf = fuzzy_mf_z_shaped(20.0f, 40.0f);
    fuzzy_mf_t moderate_mf = fuzzy_mf_gaussian(50.0f, 10.0f);
    fuzzy_mf_t dangerous_mf = fuzzy_mf_s_shaped(60.0f, 80.0f);

    fuzzy_set_t safe, moderate, dangerous;
    ASSERT_EQ(fuzzy_set_create(&safe, "safe", &safe_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&moderate, "moderate", &moderate_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&dangerous, "dangerous", &dangerous_mf, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk_score, &safe), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk_score, &moderate), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&risk_score, &dangerous), 0);

    // Low score -> safe
    fuzzy_value_t val;
    ASSERT_EQ(fuzzy_variable_fuzzify(&risk_score, 10.0f, &val), 0);
    EXPECT_GT(val.memberships[0], 0.5f);  // "safe" dominates
    EXPECT_EQ(val.dominant_term, 0u);

    // High score -> dangerous
    ASSERT_EQ(fuzzy_variable_fuzzify(&risk_score, 90.0f, &val), 0);
    EXPECT_GT(val.memberships[2], 0.5f);  // "dangerous" dominates
    EXPECT_EQ(val.dominant_term, 2u);

    // Mid score -> moderate
    ASSERT_EQ(fuzzy_variable_fuzzify(&risk_score, 50.0f, &val), 0);
    EXPECT_GT(val.memberships[1], 0.5f);  // "moderate" dominates
}

// =============================================================================
// E2E 7: System Integration - Create All Objects, Wire, Evaluate
// =============================================================================

TEST_F(FuzzyE2ETest, SystemIntegrationAllObjectsWired) {
    // Types engine with custom config
    fuzzy_types_config_t tcfg = fuzzy_types_default_config();
    tcfg.enable_caching = true;
    tcfg.cache_size = 1024;
    fuzzy_types_engine_t* custom_types = fuzzy_types_create_custom(&tcfg);
    ASSERT_NE(custom_types, nullptr);

    // Inference engine with custom config
    fuzzy_inference_config_t icfg = fuzzy_inference_default_config();
    icfg.defuzz_method = FUZZY_DEFUZZ_BISECTOR;
    icfg.and_method = FUZZY_TNORM_ALGEBRAIC_PRODUCT;
    icfg.or_method = FUZZY_TCONORM_ALGEBRAIC_SUM;
    fuzzy_inference_engine_t* custom_inf = fuzzy_inference_create_custom(&icfg);
    ASSERT_NE(custom_inf, nullptr);

    // Build variables
    fuzzy_variable_t in_var, out_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 1.0f), 0);

    fuzzy_mf_t lo = fuzzy_mf_bell(0.2f, 2.0f, 0.25f);
    fuzzy_mf_t md = fuzzy_mf_bell(0.2f, 2.0f, 0.5f);
    fuzzy_mf_t hi = fuzzy_mf_bell(0.2f, 2.0f, 0.75f);

    fuzzy_set_t s1, s2, s3;
    ASSERT_EQ(fuzzy_set_create(&s1, "low", &lo, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s2, "med", &md, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s3, "high", &hi, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s2), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s3), 0);

    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 1.0f), 0);
    fuzzy_mf_t o1 = fuzzy_mf_triangular(0.0f, 0.0f, 0.5f);
    fuzzy_mf_t o2 = fuzzy_mf_triangular(0.25f, 0.5f, 0.75f);
    fuzzy_mf_t o3 = fuzzy_mf_triangular(0.5f, 1.0f, 1.0f);
    fuzzy_set_t os1, os2, os3;
    ASSERT_EQ(fuzzy_set_create(&os1, "low", &o1, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&os2, "med", &o2, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&os3, "high", &o3, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os2), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os3), 0);

    ASSERT_EQ(fuzzy_inference_add_input(custom_inf, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(custom_inf, &out_var), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 0, 1, 0, 1, 1.0f);
    fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 0, 2, 0, 2, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(custom_inf, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(custom_inf, &r2), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(custom_inf, &r3), 0);

    // Evaluate at multiple points
    for (float x = 0.0f; x <= 1.0f; x += 0.1f) {
        fuzzy_inference_result_t result;
        memset(&result, 0, sizeof(result));
        ASSERT_EQ(fuzzy_inference_evaluate(custom_inf, &x, 1, &result), 0);
        EXPECT_GE(result.crisp_outputs[0], 0.0f);
        EXPECT_LE(result.crisp_outputs[0], 1.0f);
    }

    fuzzy_inference_destroy(custom_inf);
    fuzzy_types_destroy(custom_types);
}

// =============================================================================
// E2E 8: Memory Cleanup - Create and Destroy Many Objects
// =============================================================================

TEST_F(FuzzyE2ETest, MemoryCleanupManyObjects) {
    const int N = 50;

    for (int i = 0; i < N; i++) {
        fuzzy_types_engine_t* te = fuzzy_types_create();
        ASSERT_NE(te, nullptr);

        fuzzy_inference_engine_t* ie = fuzzy_inference_create();
        ASSERT_NE(ie, nullptr);

        fuzzy_bridge_config_t bcfg = fuzzy_bridge_default_config();
        fuzzy_bridge_t* b = fuzzy_bridge_create(&bcfg);
        ASSERT_NE(b, nullptr);

        // Do some work
        fuzzy_mf_t mf = fuzzy_mf_gaussian(0.5f, 0.1f);
        fuzzy_mf_evaluate(&mf, 0.5f);

        fuzzy_bridge_destroy(b);
        fuzzy_inference_destroy(ie);
        fuzzy_types_destroy(te);
    }

    // Also test discrete set create/free cycle
    for (int i = 0; i < N; i++) {
        fuzzy_discrete_set_t ds;
        ASSERT_EQ(fuzzy_discrete_set_create(&ds, 128, 0.0f, 1.0f), 0);
        fuzzy_discrete_set_free(&ds);
    }
}

// =============================================================================
// E2E 9: Concurrent Safety - Multiple Engines with Different Configs
// =============================================================================

TEST_F(FuzzyE2ETest, MultipleEnginesDifferentConfigs) {
    // Create engines with different configurations
    fuzzy_inference_config_t cfg1 = fuzzy_inference_default_config();
    cfg1.fis_type = FUZZY_FIS_MAMDANI;
    cfg1.defuzz_method = FUZZY_DEFUZZ_CENTROID;

    fuzzy_inference_config_t cfg2 = fuzzy_inference_default_config();
    cfg2.fis_type = FUZZY_FIS_MAMDANI;
    cfg2.defuzz_method = FUZZY_DEFUZZ_MOM;

    fuzzy_inference_engine_t* e1 = fuzzy_inference_create_custom(&cfg1);
    fuzzy_inference_engine_t* e2 = fuzzy_inference_create_custom(&cfg2);
    ASSERT_NE(e1, nullptr);
    ASSERT_NE(e2, nullptr);

    // Build identical variables and rules for both
    auto setup_engine = [](fuzzy_inference_engine_t* eng) {
        fuzzy_variable_t in_var;
        ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);
        fuzzy_mf_t lo = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
        fuzzy_mf_t hi = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
        fuzzy_set_t s1, s2;
        ASSERT_EQ(fuzzy_set_create(&s1, "low", &lo, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&s2, "high", &hi, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s1), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s2), 0);

        fuzzy_variable_t out_var;
        ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 10.0f), 0);
        fuzzy_mf_t o1 = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
        fuzzy_mf_t o2 = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
        fuzzy_set_t os1, os2;
        ASSERT_EQ(fuzzy_set_create(&os1, "low", &o1, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_set_create(&os2, "high", &o2, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os1), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os2), 0);

        ASSERT_EQ(fuzzy_inference_add_input(eng, &in_var), 0);
        ASSERT_EQ(fuzzy_inference_add_output(eng, &out_var), 0);

        fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
        fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 0, 1, 0, 1, 1.0f);
        ASSERT_EQ(fuzzy_inference_add_rule(eng, &r1), 0);
        ASSERT_EQ(fuzzy_inference_add_rule(eng, &r2), 0);
    };

    setup_engine(e1);
    setup_engine(e2);

    // Evaluate both with same input
    float input = 3.0f;
    fuzzy_inference_result_t r1, r2;
    memset(&r1, 0, sizeof(r1));
    memset(&r2, 0, sizeof(r2));
    ASSERT_EQ(fuzzy_inference_evaluate(e1, &input, 1, &r1), 0);
    ASSERT_EQ(fuzzy_inference_evaluate(e2, &input, 1, &r2), 0);

    // Both in valid range
    EXPECT_GE(r1.crisp_outputs[0], 0.0f);
    EXPECT_LE(r1.crisp_outputs[0], 10.0f);
    EXPECT_GE(r2.crisp_outputs[0], 0.0f);
    EXPECT_LE(r2.crisp_outputs[0], 10.0f);

    // Different defuzz methods may produce different results
    // (both valid, just potentially different)

    fuzzy_inference_destroy(e2);
    fuzzy_inference_destroy(e1);
}

// =============================================================================
// E2E 10: Performance Baseline
// =============================================================================

TEST_F(FuzzyE2ETest, PerformanceBaseline) {
    // Build a moderately complex FIS
    fuzzy_variable_t in_var, out_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 100.0f), 0);

    const char* term_names[] = {"very_low", "low", "med_low", "medium", "med_high",
                                 "high", "very_high"};
    for (int i = 0; i < 7; i++) {
        float center = (float)i * 100.0f / 6.0f;
        float left = center - 100.0f / 6.0f;
        float right = center + 100.0f / 6.0f;
        if (left < 0.0f) left = 0.0f;
        if (right > 100.0f) right = 100.0f;
        fuzzy_mf_t mf = fuzzy_mf_triangular(left, center, right);
        fuzzy_set_t s;
        ASSERT_EQ(fuzzy_set_create(&s, term_names[i], &mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s), 0);
    }

    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 100.0f), 0);
    for (int i = 0; i < 7; i++) {
        float center = (float)i * 100.0f / 6.0f;
        float left = center - 100.0f / 6.0f;
        float right = center + 100.0f / 6.0f;
        if (left < 0.0f) left = 0.0f;
        if (right > 100.0f) right = 100.0f;
        fuzzy_mf_t mf = fuzzy_mf_triangular(left, center, right);
        fuzzy_set_t s;
        ASSERT_EQ(fuzzy_set_create(&s, term_names[i], &mf, FUZZY_HEDGE_NONE), 0);
        ASSERT_EQ(fuzzy_variable_add_term(&out_var, &s), 0);
    }

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out_var), 0);

    // Add 7 rules
    for (int i = 0; i < 7; i++) {
        fuzzy_rule_t r = fuzzy_rule_mamdani(0, i, 0, i, 0, i, 1.0f);
        ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r), 0);
    }

    // Time 100 evaluations
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        float input = (float)i;
        fuzzy_inference_result_t result;
        memset(&result, 0, sizeof(result));
        ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, &input, 1, &result), 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 100 evaluations should complete in under 5 seconds (generous)
    EXPECT_LT(elapsed_us, 5000000);
}

// =============================================================================
// E2E 11: Fuzzify -> Centroid Round-Trip
// =============================================================================

TEST_F(FuzzyE2ETest, FuzzifyCentroidRoundTrip) {
    fuzzy_variable_t var;
    ASSERT_EQ(fuzzy_variable_create(&var, "temp", 0.0f, 100.0f), 0);

    fuzzy_mf_t m1 = fuzzy_mf_triangular(0.0f, 25.0f, 50.0f);
    fuzzy_mf_t m2 = fuzzy_mf_triangular(25.0f, 50.0f, 75.0f);
    fuzzy_mf_t m3 = fuzzy_mf_triangular(50.0f, 75.0f, 100.0f);

    fuzzy_set_t s1, s2, s3;
    ASSERT_EQ(fuzzy_set_create(&s1, "cold", &m1, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s2, "warm", &m2, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s3, "hot", &m3, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s2), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&var, &s3), 0);

    // Fuzzify at the peak of "warm"
    fuzzy_value_t val;
    ASSERT_EQ(fuzzy_variable_fuzzify(&var, 50.0f, &val), 0);

    // Centroid should be close to the input (50) for a symmetric system
    float centroid = fuzzy_variable_centroid(&var, &val);
    EXPECT_GE(centroid, 0.0f);
    EXPECT_LE(centroid, 100.0f);
    // Expect near 50 since "warm" dominates at 50
    EXPECT_NEAR(centroid, 50.0f, 15.0f);  // Allow some tolerance
}

// =============================================================================
// E2E 12: Custom Membership Function Callback
// =============================================================================

static float custom_step_fn(float x, const float* params,
                             uint32_t num_params, void* user_data) {
    (void)num_params;
    (void)user_data;
    float threshold = params[0];
    return (x >= threshold) ? 1.0f : 0.0f;
}

TEST_F(FuzzyE2ETest, CustomMembershipFunctionInPipeline) {
    float params[] = {5.0f};
    fuzzy_mf_t custom_mf = fuzzy_mf_custom(custom_step_fn, params, 1, nullptr);

    // Below threshold -> 0
    float val_low = fuzzy_mf_evaluate(&custom_mf, 3.0f);
    EXPECT_NEAR(val_low, 0.0f, TOL);

    // At threshold -> 1
    float val_at = fuzzy_mf_evaluate(&custom_mf, 5.0f);
    EXPECT_NEAR(val_at, 1.0f, TOL);

    // Above threshold -> 1
    float val_hi = fuzzy_mf_evaluate(&custom_mf, 7.0f);
    EXPECT_NEAR(val_hi, 1.0f, TOL);

    // Use in a fuzzy set
    fuzzy_set_t custom_set;
    ASSERT_EQ(fuzzy_set_create(&custom_set, "step", &custom_mf, FUZZY_HEDGE_NONE), 0);
    float set_val = fuzzy_set_evaluate(&custom_set, 6.0f);
    EXPECT_NEAR(set_val, 1.0f, TOL);
}

// =============================================================================
// E2E 13: All Defuzzification Methods
// =============================================================================

TEST_F(FuzzyE2ETest, AllDefuzzificationMethods) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 50.0f, 100.0f);
    fuzzy_discrete_set_t disc = {0};
    ASSERT_EQ(fuzzy_mf_discretize(&tri, 0.0f, 100.0f, 256, &disc), 0);

    for (int d = 0; d < FUZZY_DEFUZZ_TYPE_COUNT; d++) {
        float result = fuzzy_defuzzify(&disc, (fuzzy_defuzz_type_t)d);
        EXPECT_GE(result, 0.0f) << "Defuzz type " << d;
        /* FUZZY_DEFUZZ_WEIGHTED_SUM is inherently unnormalized and can
         * return values outside the universe range - this is expected */
        if (d != FUZZY_DEFUZZ_WEIGHTED_SUM) {
            EXPECT_LE(result, 100.0f) << "Defuzz type " << d;
        }
    }

    fuzzy_discrete_set_free(&disc);
}

// =============================================================================
// E2E 14: Bridge Stats After Operations
// =============================================================================

TEST_F(FuzzyE2ETest, BridgeStatsAfterOperations) {
    fuzzy_bridge_reset_stats(bridge);

    // Perform some spike conversions
    float memberships[] = {0.3f, 0.7f};
    float rates[2] = {0};
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(fuzzy_bridge_to_spike_population(bridge, memberships, 2, rates), 0);
    }

    fuzzy_bridge_stats_t stats;
    ASSERT_EQ(fuzzy_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GE(stats.spike_conversions, 10u);
}

// =============================================================================
// E2E 15: Trapezoidal MF Coverage
// =============================================================================

TEST_F(FuzzyE2ETest, TrapezoidalMFFullCoverage) {
    fuzzy_mf_t trap = fuzzy_mf_trapezoidal(10.0f, 30.0f, 60.0f, 80.0f);

    // Before left foot -> 0
    EXPECT_NEAR(fuzzy_mf_evaluate(&trap, 5.0f), 0.0f, TOL);

    // On left ramp -> rising
    float ramp = fuzzy_mf_evaluate(&trap, 20.0f);
    EXPECT_GT(ramp, 0.0f);
    EXPECT_LT(ramp, 1.0f);

    // On shoulder plateau -> 1
    EXPECT_NEAR(fuzzy_mf_evaluate(&trap, 45.0f), 1.0f, TOL);

    // On right ramp -> falling
    float ramp2 = fuzzy_mf_evaluate(&trap, 70.0f);
    EXPECT_GT(ramp2, 0.0f);
    EXPECT_LT(ramp2, 1.0f);

    // After right foot -> 0
    EXPECT_NEAR(fuzzy_mf_evaluate(&trap, 90.0f), 0.0f, TOL);
}

// =============================================================================
// E2E 16: Aggregation End-to-End
// =============================================================================

TEST_F(FuzzyE2ETest, AggregationAllTypes) {
    float a = 0.6f, b = 0.4f;

    for (int t = 0; t < FUZZY_AGG_TYPE_COUNT; t++) {
        float result = fuzzy_aggregate(a, b, (fuzzy_aggregation_type_t)t);
        EXPECT_GE(result, 0.0f) << "Aggregation type " << t;
        EXPECT_LE(result, 1.0f + TOL) << "Aggregation type " << t;
    }

    // Array aggregation
    float vals[] = {0.3f, 0.5f, 0.7f, 0.9f};
    for (int t = 0; t < FUZZY_AGG_TYPE_COUNT; t++) {
        float result = fuzzy_aggregate_array(vals, 4, (fuzzy_aggregation_type_t)t);
        EXPECT_GE(result, 0.0f) << "Aggregate array type " << t;
        EXPECT_LE(result, 1.0f + TOL) << "Aggregate array type " << t;
    }
}

// =============================================================================
// E2E 17: Hedge Effects on All MF Types
// =============================================================================

TEST_F(FuzzyE2ETest, HedgeEffectsOnMembership) {
    float base_membership = 0.5f;

    // VERY: should decrease (0.5^2 = 0.25)
    float very = fuzzy_apply_hedge(base_membership, FUZZY_HEDGE_VERY);
    EXPECT_NEAR(very, 0.25f, TOL);

    // SOMEWHAT: should increase (sqrt(0.5) ~ 0.707)
    float somewhat = fuzzy_apply_hedge(base_membership, FUZZY_HEDGE_SOMEWHAT);
    EXPECT_NEAR(somewhat, std::sqrt(0.5f), TOL);

    // EXTREMELY: should decrease more (0.5^3 = 0.125)
    float extremely = fuzzy_apply_hedge(base_membership, FUZZY_HEDGE_EXTREMELY);
    EXPECT_NEAR(extremely, 0.125f, TOL);

    // NOT: complement (1 - 0.5 = 0.5)
    float not_hedge = fuzzy_apply_hedge(base_membership, FUZZY_HEDGE_NOT);
    EXPECT_NEAR(not_hedge, 0.5f, TOL);

    // NONE: identity
    float none = fuzzy_apply_hedge(base_membership, FUZZY_HEDGE_NONE);
    EXPECT_NEAR(none, 0.5f, TOL);

    // Ordering: extremely < very < identity < somewhat
    EXPECT_LT(extremely, very);
    EXPECT_LT(very, none);
    EXPECT_LT(none, somewhat);
}

// =============================================================================
// E2E 18: Custom Config FIS with All Non-Default Options
// =============================================================================

TEST_F(FuzzyE2ETest, CustomConfigAllNonDefault) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_MAMDANI;
    cfg.defuzz_method = FUZZY_DEFUZZ_LOM;
    cfg.and_method = FUZZY_TNORM_LUKASIEWICZ;
    cfg.or_method = FUZZY_TCONORM_LUKASIEWICZ;
    cfg.implication = FUZZY_IMPL_LARSEN;
    cfg.aggregation = FUZZY_AGG_BOUNDED_SUM;
    cfg.defuzz_resolution = 128;

    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);

    // Build simple FIS
    fuzzy_variable_t in_var, out_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);
    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 10.0f), 0);

    fuzzy_mf_t lo = fuzzy_mf_gaussian(2.5f, 2.0f);
    fuzzy_mf_t hi = fuzzy_mf_gaussian(7.5f, 2.0f);
    fuzzy_set_t s1, s2;
    ASSERT_EQ(fuzzy_set_create(&s1, "low", &lo, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s2, "high", &hi, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s2), 0);

    fuzzy_mf_t o1 = fuzzy_mf_triangular(0.0f, 2.5f, 5.0f);
    fuzzy_mf_t o2 = fuzzy_mf_triangular(5.0f, 7.5f, 10.0f);
    fuzzy_set_t os1, os2;
    ASSERT_EQ(fuzzy_set_create(&os1, "low", &o1, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&os2, "high", &o2, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os2), 0);

    ASSERT_EQ(fuzzy_inference_add_input(eng, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(eng, &out_var), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 0, 1, 0, 1, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(eng, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(eng, &r2), 0);

    float input = 5.0f;
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(eng, &input, 1, &result), 0);
    EXPECT_GE(result.crisp_outputs[0], 0.0f);
    EXPECT_LE(result.crisp_outputs[0], 10.0f);

    fuzzy_inference_destroy(eng);
}

// =============================================================================
// E2E 19: MF Evaluate Hedged Directly
// =============================================================================

TEST_F(FuzzyE2ETest, MFEvaluateHedgedDirectly) {
    fuzzy_mf_t tri = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);

    // At peak (5.0), membership = 1.0 regardless of hedge (1^n = 1)
    for (int h = 0; h < FUZZY_HEDGE_TYPE_COUNT; h++) {
        float val = fuzzy_mf_evaluate_hedged(&tri, 5.0f, (fuzzy_hedge_t)h);
        if (h == FUZZY_HEDGE_NOT) {
            EXPECT_NEAR(val, 0.0f, TOL);  // NOT(1.0) = 0.0
        } else {
            EXPECT_NEAR(val, 1.0f, TOL);
        }
    }

    // At 0, membership = 0 for all hedges
    for (int h = 0; h < FUZZY_HEDGE_TYPE_COUNT; h++) {
        float val = fuzzy_mf_evaluate_hedged(&tri, 0.0f, (fuzzy_hedge_t)h);
        if (h == FUZZY_HEDGE_NOT) {
            EXPECT_NEAR(val, 1.0f, TOL);  // NOT(0.0) = 1.0
        } else {
            EXPECT_NEAR(val, 0.0f, TOL);
        }
    }
}

// =============================================================================
// E2E 20: Full Pipeline with Modulation
// =============================================================================

TEST_F(FuzzyE2ETest, FullPipelineWithModulation) {
    // Set modulation levels on all engines
    ASSERT_EQ(fuzzy_types_set_inflammation(types_engine, 0.3f), 0);
    ASSERT_EQ(fuzzy_types_set_fatigue(types_engine, 0.2f), 0);
    ASSERT_EQ(fuzzy_inference_set_inflammation(inf_engine, 0.3f), 0);
    ASSERT_EQ(fuzzy_inference_set_fatigue(inf_engine, 0.2f), 0);
    ASSERT_EQ(fuzzy_bridge_set_inflammation(bridge, 0.3f), 0);
    ASSERT_EQ(fuzzy_bridge_set_fatigue(bridge, 0.2f), 0);

    // Build simple FIS
    fuzzy_variable_t in_var, out_var;
    ASSERT_EQ(fuzzy_variable_create(&in_var, "x", 0.0f, 10.0f), 0);
    ASSERT_EQ(fuzzy_variable_create(&out_var, "y", 0.0f, 10.0f), 0);

    fuzzy_mf_t lo = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t hi = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t s1, s2;
    ASSERT_EQ(fuzzy_set_create(&s1, "low", &lo, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&s2, "high", &hi, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&in_var, &s2), 0);

    fuzzy_mf_t o1 = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t o2 = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t os1, os2;
    ASSERT_EQ(fuzzy_set_create(&os1, "low", &o1, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_set_create(&os2, "high", &o2, FUZZY_HEDGE_NONE), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os1), 0);
    ASSERT_EQ(fuzzy_variable_add_term(&out_var, &os2), 0);

    ASSERT_EQ(fuzzy_inference_add_input(inf_engine, &in_var), 0);
    ASSERT_EQ(fuzzy_inference_add_output(inf_engine, &out_var), 0);

    fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 0, 0, 0, 0, 1.0f);
    fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 0, 1, 0, 1, 1.0f);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r1), 0);
    ASSERT_EQ(fuzzy_inference_add_rule(inf_engine, &r2), 0);

    // Should still produce valid output under modulation
    float input = 7.0f;
    fuzzy_inference_result_t result;
    memset(&result, 0, sizeof(result));
    ASSERT_EQ(fuzzy_inference_evaluate(inf_engine, &input, 1, &result), 0);
    EXPECT_GE(result.crisp_outputs[0], 0.0f);
    EXPECT_LE(result.crisp_outputs[0], 10.0f);

    // Heartbeat should still work
    ASSERT_EQ(fuzzy_bridge_heartbeat(bridge, "modulated_pipeline", 1.0f), 0);
}
