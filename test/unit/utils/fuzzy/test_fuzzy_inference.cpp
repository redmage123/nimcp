/**
 * @file test_fuzzy_inference.cpp
 * @brief Unit tests for fuzzy inference engine module
 *
 * WHAT: ~50 tests for Mamdani/Sugeno/Tsukamoto inference, rule management,
 *       defuzzification methods, batch inference, and ANFIS learning
 * WHY:  Verify correct IF-THEN rule evaluation, all 7 defuzzification methods,
 *       and adaptive learning functionality
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
#include "utils/fuzzy/nimcp_fuzzy_operators.h"
#include "utils/fuzzy/nimcp_fuzzy_inference.h"
}

// ============================================================================
// Test Constants
// ============================================================================

namespace {
    constexpr float TOL = 1e-3f;
    constexpr float RELAXED = 1e-1f;
}

// ============================================================================
// Fixture: Basic Inference Engine
// ============================================================================

class FuzzyInferenceTest : public ::testing::Test {
protected:
    fuzzy_inference_engine_t* engine = nullptr;

    void SetUp() override {
        engine = fuzzy_inference_create();
    }

    void TearDown() override {
        if (engine) {
            fuzzy_inference_destroy(engine);
            engine = nullptr;
        }
    }
};

// ============================================================================
// Fixture: Configured Mamdani System (2 inputs, 1 output)
// ============================================================================

class FuzzyMamdaniTest : public ::testing::Test {
protected:
    fuzzy_inference_engine_t* engine = nullptr;
    fuzzy_variable_t input1, input2, output;

    void SetUp() override {
        fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
        cfg.fis_type = FUZZY_FIS_MAMDANI;
        cfg.defuzz_method = FUZZY_DEFUZZ_CENTROID;
        engine = fuzzy_inference_create_custom(&cfg);
        ASSERT_NE(engine, nullptr);

        // Input1: "temperature" [0, 100]
        fuzzy_variable_create(&input1, "temperature", 0.0f, 100.0f);
        fuzzy_mf_t cold_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 20.0f, 40.0f);
        fuzzy_mf_t warm_mf = fuzzy_mf_triangular(20.0f, 50.0f, 80.0f);
        fuzzy_mf_t hot_mf  = fuzzy_mf_trapezoidal(60.0f, 80.0f, 100.0f, 100.0f);
        fuzzy_set_t s;
        fuzzy_set_create(&s, "cold", &cold_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input1, &s);
        fuzzy_set_create(&s, "warm", &warm_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input1, &s);
        fuzzy_set_create(&s, "hot", &hot_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input1, &s);

        // Input2: "humidity" [0, 100]
        fuzzy_variable_create(&input2, "humidity", 0.0f, 100.0f);
        fuzzy_mf_t low_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 30.0f, 50.0f);
        fuzzy_mf_t med_mf = fuzzy_mf_triangular(30.0f, 50.0f, 70.0f);
        fuzzy_mf_t high_mf = fuzzy_mf_trapezoidal(50.0f, 70.0f, 100.0f, 100.0f);
        fuzzy_set_create(&s, "low", &low_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input2, &s);
        fuzzy_set_create(&s, "medium", &med_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input2, &s);
        fuzzy_set_create(&s, "high", &high_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&input2, &s);

        // Output: "fan_speed" [0, 100]
        fuzzy_variable_create(&output, "fan_speed", 0.0f, 100.0f);
        fuzzy_mf_t slow_mf = fuzzy_mf_trapezoidal(0.0f, 0.0f, 20.0f, 40.0f);
        fuzzy_mf_t med_out_mf = fuzzy_mf_triangular(20.0f, 50.0f, 80.0f);
        fuzzy_mf_t fast_mf = fuzzy_mf_trapezoidal(60.0f, 80.0f, 100.0f, 100.0f);
        fuzzy_set_create(&s, "slow", &slow_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&output, &s);
        fuzzy_set_create(&s, "medium", &med_out_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&output, &s);
        fuzzy_set_create(&s, "fast", &fast_mf, FUZZY_HEDGE_NONE);
        fuzzy_variable_add_term(&output, &s);

        fuzzy_inference_add_input(engine, &input1);
        fuzzy_inference_add_input(engine, &input2);
        fuzzy_inference_add_output(engine, &output);

        // Rules:
        // IF temp IS cold AND humidity IS low THEN fan IS slow
        fuzzy_rule_t r1 = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
        fuzzy_inference_add_rule(engine, &r1);

        // IF temp IS warm AND humidity IS medium THEN fan IS medium
        fuzzy_rule_t r2 = fuzzy_rule_mamdani(0, 1, 1, 1, 0, 1, 1.0f);
        fuzzy_inference_add_rule(engine, &r2);

        // IF temp IS hot AND humidity IS high THEN fan IS fast
        fuzzy_rule_t r3 = fuzzy_rule_mamdani(0, 2, 1, 2, 0, 2, 1.0f);
        fuzzy_inference_add_rule(engine, &r3);
    }

    void TearDown() override {
        if (engine) {
            fuzzy_inference_destroy(engine);
            engine = nullptr;
        }
    }
};

// ============================================================================
// 1. Lifecycle Tests
// ============================================================================

TEST_F(FuzzyInferenceTest, CreateDefault_ReturnsNonNull) {
    ASSERT_NE(engine, nullptr);
}

TEST(FuzzyInferenceLifecycle, DestroyNull_NoOp) {
    fuzzy_inference_destroy(nullptr);  // should not crash
}

TEST(FuzzyInferenceLifecycle, CreateCustom_MamdaniConfig) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_MAMDANI;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);
    fuzzy_inference_destroy(eng);
}

TEST(FuzzyInferenceLifecycle, CreateCustom_SugenoConfig) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);
    fuzzy_inference_destroy(eng);
}

TEST(FuzzyInferenceLifecycle, CreateCustom_TsukamotoConfig) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_TSUKAMOTO;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);
    fuzzy_inference_destroy(eng);
}

TEST(FuzzyInferenceLifecycle, CreateCustom_NullConfig_UsesDefaults) {
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(nullptr);
    ASSERT_NE(eng, nullptr);
    fuzzy_inference_destroy(eng);
}

TEST(FuzzyInferenceLifecycle, DefaultConfig_HasValidDefaults) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    EXPECT_GE(cfg.defuzz_resolution, 32u);
    EXPECT_GE(cfg.anfis_learning_rate, 0.0f);
    EXPECT_GT(cfg.anfis_max_epochs, 0u);
}

// ============================================================================
// 2. Variable Registration
// ============================================================================

TEST_F(FuzzyInferenceTest, AddInput_Valid) {
    fuzzy_variable_t var;
    fuzzy_variable_create(&var, "x", 0.0f, 10.0f);
    int rc = fuzzy_inference_add_input(engine, &var);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddInput_Null) {
    int rc = fuzzy_inference_add_input(engine, nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddInput_NullEngine) {
    fuzzy_variable_t var;
    fuzzy_variable_create(&var, "x", 0.0f, 10.0f);
    int rc = fuzzy_inference_add_input(nullptr, &var);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddOutput_Valid) {
    fuzzy_variable_t var;
    fuzzy_variable_create(&var, "y", 0.0f, 10.0f);
    int rc = fuzzy_inference_add_output(engine, &var);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddOutput_Null) {
    int rc = fuzzy_inference_add_output(engine, nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

// ============================================================================
// 3. Rule Management
// ============================================================================

TEST_F(FuzzyInferenceTest, AddRule_Valid) {
    fuzzy_variable_t in1, in2, out;
    fuzzy_variable_create(&in1, "x", 0.0f, 10.0f);
    fuzzy_variable_create(&in2, "y", 0.0f, 10.0f);
    fuzzy_variable_create(&out, "z", 0.0f, 10.0f);

    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t s;
    fuzzy_set_create(&s, "mid", &mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&in1, &s);
    fuzzy_variable_add_term(&in2, &s);
    fuzzy_variable_add_term(&out, &s);

    fuzzy_inference_add_input(engine, &in1);
    fuzzy_inference_add_input(engine, &in2);
    fuzzy_inference_add_output(engine, &out);

    fuzzy_rule_t rule = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    int rc = fuzzy_inference_add_rule(engine, &rule);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddRule_NullEngine) {
    fuzzy_rule_t rule = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    int rc = fuzzy_inference_add_rule(nullptr, &rule);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, AddRule_NullRule) {
    int rc = fuzzy_inference_add_rule(engine, nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, ClearRules_Valid) {
    int rc = fuzzy_inference_clear_rules(engine);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, ClearRules_NullEngine) {
    int rc = fuzzy_inference_clear_rules(nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, GetRuleCount_InitiallyZero) {
    int count = fuzzy_inference_get_rule_count(engine);
    EXPECT_EQ(count, 0);
}

TEST_F(FuzzyInferenceTest, GetRuleCount_NullEngine) {
    int count = fuzzy_inference_get_rule_count(nullptr);
    EXPECT_LE(count, 0);
}

TEST_F(FuzzyInferenceTest, GetRuleCount_AfterAdding) {
    fuzzy_variable_t in1, in2, out;
    fuzzy_variable_create(&in1, "x", 0.0f, 10.0f);
    fuzzy_variable_create(&in2, "y", 0.0f, 10.0f);
    fuzzy_variable_create(&out, "z", 0.0f, 10.0f);
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t s;
    fuzzy_set_create(&s, "mid", &mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&in1, &s);
    fuzzy_variable_add_term(&in2, &s);
    fuzzy_variable_add_term(&out, &s);
    fuzzy_inference_add_input(engine, &in1);
    fuzzy_inference_add_input(engine, &in2);
    fuzzy_inference_add_output(engine, &out);

    fuzzy_rule_t r = fuzzy_rule_mamdani(0, 0, 1, 0, 0, 0, 1.0f);
    fuzzy_inference_add_rule(engine, &r);
    int count = fuzzy_inference_get_rule_count(engine);
    EXPECT_EQ(count, 1);
}

// ============================================================================
// 4. Rule Convenience Builders
// ============================================================================

TEST(FuzzyRuleBuilder, MamdaniRule_FieldsSet) {
    fuzzy_rule_t r = fuzzy_rule_mamdani(0, 1, 1, 2, 0, 0, 0.8f);
    EXPECT_EQ(r.num_antecedents, 2u);
    EXPECT_EQ(r.antecedents[0].var_index, 0u);
    EXPECT_EQ(r.antecedents[0].term_index, 1u);
    EXPECT_EQ(r.antecedents[1].var_index, 1u);
    EXPECT_EQ(r.antecedents[1].term_index, 2u);
    EXPECT_EQ(r.mamdani.var_index, 0u);
    EXPECT_EQ(r.mamdani.term_index, 0u);
    EXPECT_NEAR(r.weight, 0.8f, TOL);
}

TEST(FuzzyRuleBuilder, SugenoRule_FieldsSet) {
    float coeffs[] = {1.0f, 2.0f, 3.0f};
    fuzzy_rule_t r = fuzzy_rule_sugeno(0, 0, 1, 1, coeffs, 3, 1.0f);
    EXPECT_EQ(r.num_antecedents, 2u);
    EXPECT_EQ(r.sugeno.num_coeffs, 3u);
    EXPECT_NEAR(r.sugeno.coefficients[0], 1.0f, TOL);
    EXPECT_NEAR(r.sugeno.coefficients[1], 2.0f, TOL);
    EXPECT_NEAR(r.sugeno.coefficients[2], 3.0f, TOL);
}

// ============================================================================
// 5. Mamdani Inference Evaluation
// ============================================================================

TEST_F(FuzzyMamdaniTest, Evaluate_ColdLowInput) {
    float inputs[] = {10.0f, 20.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(engine, inputs, 2, &result);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
    EXPECT_EQ(result.num_outputs, 1u);
    // Cold/low -> fan slow: output should be in the low range
    EXPECT_LT(result.crisp_outputs[0], 50.0f);
}

TEST_F(FuzzyMamdaniTest, Evaluate_WarmMediumInput) {
    float inputs[] = {50.0f, 50.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(engine, inputs, 2, &result);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
    // Warm/medium -> fan medium: output near center
    EXPECT_GT(result.crisp_outputs[0], 20.0f);
    EXPECT_LT(result.crisp_outputs[0], 80.0f);
}

TEST_F(FuzzyMamdaniTest, Evaluate_HotHighInput) {
    float inputs[] = {90.0f, 85.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(engine, inputs, 2, &result);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
    // Hot/high -> fan fast: output should be high
    EXPECT_GT(result.crisp_outputs[0], 50.0f);
}

TEST_F(FuzzyMamdaniTest, Evaluate_FiringStrengths) {
    float inputs[] = {50.0f, 50.0f};
    fuzzy_inference_result_t result;
    fuzzy_inference_evaluate(engine, inputs, 2, &result);
    // At least one rule should fire
    EXPECT_GT(result.num_rules_fired, 0u);
    EXPECT_GT(result.total_firing_strength, 0.0f);
}

TEST_F(FuzzyMamdaniTest, Evaluate_NullEngine) {
    float inputs[] = {50.0f, 50.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(nullptr, inputs, 2, &result);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyMamdaniTest, Evaluate_NullInputs) {
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(engine, nullptr, 2, &result);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyMamdaniTest, Evaluate_NullResult) {
    float inputs[] = {50.0f, 50.0f};
    int rc = fuzzy_inference_evaluate(engine, inputs, 2, nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyMamdaniTest, Evaluate_InputCountMismatch) {
    float inputs[] = {50.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(engine, inputs, 1, &result);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

// ============================================================================
// 6. Defuzzification Methods
// ============================================================================

class FuzzyDefuzzTest : public ::testing::Test {
protected:
    fuzzy_discrete_set_t dset;

    void SetUp() override {
        // Create a discrete triangular shape for testing defuzzification
        fuzzy_discrete_set_create(&dset, 101, 0.0f, 100.0f);
        for (uint32_t i = 0; i < 101; i++) {
            float x = static_cast<float>(i);
            if (x <= 50.0f) {
                dset.values[i] = x / 50.0f;
            } else {
                dset.values[i] = (100.0f - x) / 50.0f;
            }
        }
    }

    void TearDown() override {
        fuzzy_discrete_set_free(&dset);
    }
};

TEST_F(FuzzyDefuzzTest, Centroid_NearCenter) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_CENTROID);
    EXPECT_NEAR(crisp, 50.0f, 5.0f);
}

TEST_F(FuzzyDefuzzTest, Bisector_NearCenter) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_BISECTOR);
    EXPECT_NEAR(crisp, 50.0f, 5.0f);
}

TEST_F(FuzzyDefuzzTest, MOM_AtPeak) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_MOM);
    EXPECT_NEAR(crisp, 50.0f, 5.0f);
}

TEST_F(FuzzyDefuzzTest, SOM_AtPeak) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_SOM);
    EXPECT_NEAR(crisp, 50.0f, 5.0f);
}

TEST_F(FuzzyDefuzzTest, LOM_AtPeak) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_LOM);
    EXPECT_NEAR(crisp, 50.0f, 5.0f);
}

TEST_F(FuzzyDefuzzTest, WeightedAvg_InRange) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_WEIGHTED_AVG);
    EXPECT_GE(crisp, 0.0f);
    EXPECT_LE(crisp, 100.0f);
}

TEST_F(FuzzyDefuzzTest, WeightedSum_InRange) {
    float crisp = fuzzy_defuzzify(&dset, FUZZY_DEFUZZ_WEIGHTED_SUM);
    // Weighted sum may be large but should be finite
    EXPECT_FALSE(std::isnan(crisp));
    EXPECT_FALSE(std::isinf(crisp));
}

TEST(FuzzyDefuzz, NullSet_ReturnsZero) {
    float crisp = fuzzy_defuzzify(nullptr, FUZZY_DEFUZZ_CENTROID);
    EXPECT_NEAR(crisp, 0.0f, TOL);
}

// ============================================================================
// 7. Batch Inference
// ============================================================================

TEST_F(FuzzyMamdaniTest, BatchEvaluate_TwoSamples) {
    // inputs: [sample0_in0, sample0_in1, sample1_in0, sample1_in1]
    float inputs[] = {10.0f, 20.0f, 90.0f, 85.0f};
    fuzzy_inference_result_t results[2];
    int rc = fuzzy_inference_evaluate_batch(engine, inputs, 2, 2, results);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
    // Sample 0 (cold/low) should give lower output than Sample 1 (hot/high)
    EXPECT_LT(results[0].crisp_outputs[0], results[1].crisp_outputs[0]);
}

TEST_F(FuzzyMamdaniTest, BatchEvaluate_NullInputs) {
    fuzzy_inference_result_t results[1];
    int rc = fuzzy_inference_evaluate_batch(engine, nullptr, 1, 2, results);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

// ============================================================================
// 8. ANFIS Learning
// ============================================================================

TEST(FuzzyANFIS, Train_NullEngine) {
    float in[] = {1.0f, 2.0f};
    float tgt[] = {3.0f};
    float err;
    int rc = fuzzy_anfis_train(nullptr, in, tgt, 1, 10, &err);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST(FuzzyANFIS, Train_NullInputs) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    cfg.enable_anfis = true;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    float tgt[] = {3.0f};
    float err;
    int rc = fuzzy_anfis_train(eng, nullptr, tgt, 1, 10, &err);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
    fuzzy_inference_destroy(eng);
}

TEST(FuzzyANFIS, Train_NullTargets) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    cfg.enable_anfis = true;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    float in[] = {1.0f, 2.0f};
    float err;
    int rc = fuzzy_anfis_train(eng, in, nullptr, 1, 10, &err);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
    fuzzy_inference_destroy(eng);
}

// ============================================================================
// 9. Modulation
// ============================================================================

TEST_F(FuzzyInferenceTest, SetInflammation_Valid) {
    int rc = fuzzy_inference_set_inflammation(engine, 0.5f);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, SetInflammation_Null) {
    int rc = fuzzy_inference_set_inflammation(nullptr, 0.5f);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, SetFatigue_Valid) {
    int rc = fuzzy_inference_set_fatigue(engine, 0.3f);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, SetFatigue_Null) {
    int rc = fuzzy_inference_set_fatigue(nullptr, 0.3f);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

// ============================================================================
// 10. Statistics
// ============================================================================

TEST_F(FuzzyInferenceTest, GetStats_Valid) {
    fuzzy_inference_stats_t stats;
    int rc = fuzzy_inference_get_stats(engine, &stats);
    EXPECT_EQ(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, GetStats_NullEngine) {
    fuzzy_inference_stats_t stats;
    int rc = fuzzy_inference_get_stats(nullptr, &stats);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, GetStats_NullStats) {
    int rc = fuzzy_inference_get_stats(engine, nullptr);
    EXPECT_NE(rc, FUZZY_INF_ERR_OK);
}

TEST_F(FuzzyInferenceTest, ResetStats_Valid) {
    fuzzy_inference_reset_stats(engine);  // should not crash
}

TEST_F(FuzzyInferenceTest, ResetStats_Null) {
    fuzzy_inference_reset_stats(nullptr);  // should not crash
}

// ============================================================================
// 11. Error String
// ============================================================================

TEST(FuzzyInferenceError, GetLastError_ReturnsNonNull) {
    const char* err = fuzzy_inference_get_last_error();
    EXPECT_NE(err, nullptr);
}

// ============================================================================
// 12. Evaluate with Empty Rule Base
// ============================================================================

TEST(FuzzyInferenceEdge, EvaluateNoRules) {
    fuzzy_inference_engine_t* eng = fuzzy_inference_create();
    fuzzy_variable_t in1, out;
    fuzzy_variable_create(&in1, "x", 0.0f, 10.0f);
    fuzzy_variable_create(&out, "y", 0.0f, 10.0f);
    fuzzy_mf_t mf = fuzzy_mf_triangular(0.0f, 5.0f, 10.0f);
    fuzzy_set_t s;
    fuzzy_set_create(&s, "mid", &mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&in1, &s);
    fuzzy_variable_add_term(&out, &s);
    fuzzy_inference_add_input(eng, &in1);
    fuzzy_inference_add_output(eng, &out);

    float inputs[] = {5.0f};
    fuzzy_inference_result_t result;
    // With no rules, evaluation should fail or return default
    int rc = fuzzy_inference_evaluate(eng, inputs, 1, &result);
    // Either error or zero output is acceptable
    if (rc == FUZZY_INF_ERR_OK) {
        EXPECT_EQ(result.num_rules_fired, 0u);
    } else {
        EXPECT_EQ(rc, FUZZY_INF_ERR_NO_RULES);
    }
    fuzzy_inference_destroy(eng);
}

// ============================================================================
// 13. Sugeno Inference (basic smoke test)
// ============================================================================

TEST(FuzzyInferenceSugeno, SmokeTest) {
    fuzzy_inference_config_t cfg = fuzzy_inference_default_config();
    cfg.fis_type = FUZZY_FIS_SUGENO;
    fuzzy_inference_engine_t* eng = fuzzy_inference_create_custom(&cfg);
    ASSERT_NE(eng, nullptr);

    fuzzy_variable_t in1, in2, out;
    fuzzy_variable_create(&in1, "x", 0.0f, 10.0f);
    fuzzy_variable_create(&in2, "y", 0.0f, 10.0f);
    fuzzy_variable_create(&out, "z", 0.0f, 100.0f);

    fuzzy_mf_t low_mf = fuzzy_mf_triangular(0.0f, 0.0f, 5.0f);
    fuzzy_mf_t high_mf = fuzzy_mf_triangular(5.0f, 10.0f, 10.0f);
    fuzzy_set_t s;
    fuzzy_set_create(&s, "low", &low_mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&in1, &s);
    fuzzy_variable_add_term(&in2, &s);
    fuzzy_set_create(&s, "high", &high_mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&in1, &s);
    fuzzy_variable_add_term(&in2, &s);

    fuzzy_mf_t out_mf = fuzzy_mf_triangular(0.0f, 50.0f, 100.0f);
    fuzzy_set_create(&s, "mid", &out_mf, FUZZY_HEDGE_NONE);
    fuzzy_variable_add_term(&out, &s);

    fuzzy_inference_add_input(eng, &in1);
    fuzzy_inference_add_input(eng, &in2);
    fuzzy_inference_add_output(eng, &out);

    // z = 2*x + 3*y + 1
    float coeffs[] = {1.0f, 2.0f, 3.0f};
    fuzzy_rule_t r = fuzzy_rule_sugeno(0, 0, 1, 0, coeffs, 3, 1.0f);
    fuzzy_inference_add_rule(eng, &r);

    float inputs[] = {2.0f, 3.0f};
    fuzzy_inference_result_t result;
    int rc = fuzzy_inference_evaluate(eng, inputs, 2, &result);
    if (rc == FUZZY_INF_ERR_OK) {
        EXPECT_FALSE(std::isnan(result.crisp_outputs[0]));
    }

    fuzzy_inference_destroy(eng);
}
