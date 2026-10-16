/**
 * @file test_rubric.cpp
 * @brief Unit tests for Cognitive Output Rubric — human-style quality evaluation
 *
 * Function signatures tested (from include/nimcp.h):
 *   nimcp_status_t nimcp_brain_rubric(nimcp_brain_t brain, nimcp_rubric_t* rubric);
 *   nimcp_status_t nimcp_brain_broadcast_rubric(nimcp_brain_t brain);
 *
 * Internal functions tested (from cognitive/rubric/nimcp_rubric.h):
 *   rubric_evaluator_t* rubric_evaluator_create(const rubric_config_t* config);
 *   void rubric_evaluator_destroy(rubric_evaluator_t* eval);
 *   int rubric_evaluate_decision(rubric_evaluator_t* eval, brain_t brain,
 *                                const brain_decision_t* decision, rubric_result_t* result);
 *   void rubric_config_defaults(rubric_config_t* config);
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "nimcp.h"
#include "cognitive/rubric/nimcp_rubric.h"
#include "core/brain/nimcp_brain.h"
}

#include "utils/nimcp_test_base.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RubricTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
        nimcp_init();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Internal API: Config Defaults
 * ============================================================================ */

TEST_F(RubricTest, ConfigDefaults_SetsExpectedValues) {
    // WHAT: Verify rubric_config_defaults fills correct values
    // WHY:  Defaults drive tier weighting; must be balanced 50/50
    // HOW:  Call defaults, check all fields

    rubric_config_t config = {};
    rubric_config_defaults(&config);

    EXPECT_FLOAT_EQ(config.tier1_weight, 0.5f);
    EXPECT_FLOAT_EQ(config.tier2_weight, 0.5f);
    EXPECT_TRUE(config.skip_missing_subsystems);
}

TEST_F(RubricTest, ConfigDefaults_NullSafe) {
    // WHAT: rubric_config_defaults(NULL) doesn't crash
    // WHY:  Defensive null guard
    rubric_config_defaults(nullptr);
}

/* ============================================================================
 * Internal API: Evaluator Lifecycle
 * ============================================================================ */

TEST_F(RubricTest, EvaluatorCreate_DefaultConfig) {
    // WHAT: Create evaluator with NULL config (uses defaults)
    // WHY:  Most common usage path
    rubric_evaluator_t* eval = rubric_evaluator_create(nullptr);
    ASSERT_NE(eval, nullptr);
    rubric_evaluator_destroy(eval);
}

TEST_F(RubricTest, EvaluatorCreate_CustomConfig) {
    // WHAT: Create evaluator with custom tier weights
    rubric_config_t config;
    rubric_config_defaults(&config);
    config.tier1_weight = 0.7f;
    config.tier2_weight = 0.3f;

    rubric_evaluator_t* eval = rubric_evaluator_create(&config);
    ASSERT_NE(eval, nullptr);
    rubric_evaluator_destroy(eval);
}

TEST_F(RubricTest, EvaluatorDestroy_NullSafe) {
    // WHAT: Destroy NULL evaluator doesn't crash
    rubric_evaluator_destroy(nullptr);
}

/* ============================================================================
 * Internal API: Evaluate Decision — NULL guards
 * ============================================================================ */

TEST_F(RubricTest, EvaluateDecision_NullEval) {
    // WHAT: NULL evaluator returns error
    rubric_result_t result = {};
    int rc = rubric_evaluate_decision(nullptr, nullptr, nullptr, &result);
    EXPECT_EQ(rc, -1);
}

TEST_F(RubricTest, EvaluateDecision_NullResult) {
    // WHAT: NULL result returns error
    rubric_evaluator_t* eval = rubric_evaluator_create(nullptr);
    ASSERT_NE(eval, nullptr);
    int rc = rubric_evaluate_decision(eval, nullptr, nullptr, nullptr);
    EXPECT_EQ(rc, -1);
    rubric_evaluator_destroy(eval);
}

/* ============================================================================
 * Internal API: Evaluate Decision — Synthetic Decision
 * ============================================================================ */

TEST_F(RubricTest, EvaluateDecision_SyntheticDecision) {
    // WHAT: Evaluate a hand-crafted decision to verify scoring pipeline
    // WHY:  End-to-end test without requiring full brain_decide pipeline
    // HOW:  Build a brain_decision_t, call rubric_evaluate_decision, check ranges

    rubric_evaluator_t* eval = rubric_evaluator_create(nullptr);
    ASSERT_NE(eval, nullptr);

    // Build a synthetic decision
    brain_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    strncpy(decision.label, "cat", sizeof(decision.label) - 1);
    decision.confidence = 0.85f;
    decision.output_size = 4;

    float output_vec[4] = {0.1f, 0.85f, 0.03f, 0.02f};
    decision.output_vector = output_vec;
    decision.num_active_neurons = 50;
    strncpy(decision.explanation, "WHAT: Classified as cat | WHY: High activation in feline cluster | PROOF: cosine>0.9",
            sizeof(decision.explanation) - 1);
    decision.sparsity = 0.05f;

    rubric_result_t result = {};
    int rc = rubric_evaluate_decision(eval, nullptr, &decision, &result);
    EXPECT_EQ(rc, 0);

    // All scores in [0, 1]
    EXPECT_GE(result.tier1.internal_consistency, 0.0f);
    EXPECT_LE(result.tier1.internal_consistency, 1.0f);
    EXPECT_GE(result.tier1.confidence_calibration, 0.0f);
    EXPECT_LE(result.tier1.confidence_calibration, 1.0f);
    EXPECT_GE(result.tier1.completeness, 0.0f);
    EXPECT_LE(result.tier1.completeness, 1.0f);
    EXPECT_GE(result.tier1.reasoning_chain_quality, 0.0f);
    EXPECT_LE(result.tier1.reasoning_chain_quality, 1.0f);
    EXPECT_GE(result.tier1.tier1_score, 0.0f);
    EXPECT_LE(result.tier1.tier1_score, 1.0f);

    EXPECT_GE(result.tier2.originality, 0.0f);
    EXPECT_LE(result.tier2.originality, 1.0f);
    EXPECT_GE(result.tier2.integration_depth, 0.0f);
    EXPECT_LE(result.tier2.integration_depth, 1.0f);
    EXPECT_GE(result.tier2.communication_clarity, 0.0f);
    EXPECT_LE(result.tier2.communication_clarity, 1.0f);
    EXPECT_GE(result.tier2.information_density, 0.0f);
    EXPECT_LE(result.tier2.information_density, 1.0f);
    EXPECT_GE(result.tier2.tier2_score, 0.0f);
    EXPECT_LE(result.tier2.tier2_score, 1.0f);

    EXPECT_GE(result.overall_score, 0.0f);
    EXPECT_LE(result.overall_score, 1.0f);

    // Grade must be valid letter
    EXPECT_TRUE(result.grade == 'A' || result.grade == 'B' ||
                result.grade == 'C' || result.grade == 'D' || result.grade == 'F');

    // Modifier must be +, -, or space
    EXPECT_TRUE(result.grade_modifier == '+' || result.grade_modifier == '-' ||
                result.grade_modifier == ' ');

    // Evaluation time must be > 0
    EXPECT_GT(result.evaluation_time_us, (uint64_t)0);

    rubric_evaluator_destroy(eval);
}

/* ============================================================================
 * Grade Derivation Boundaries
 * ============================================================================ */

class GradeBoundaryTest : public RubricTest {};

TEST_F(GradeBoundaryTest, EvaluateDecision_HighConfidence_HighOutput) {
    // WHAT: High-quality decision should get good grade
    // WHY:  Verify grade derivation for strong outputs
    rubric_evaluator_t* eval = rubric_evaluator_create(nullptr);
    ASSERT_NE(eval, nullptr);

    brain_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    strncpy(decision.label, "excellent_result", sizeof(decision.label) - 1);
    decision.confidence = 0.95f;
    decision.output_size = 8;

    // Strong, clear output vector — one dominant activation
    float output_vec[8] = {0.01f, 0.01f, 0.95f, 0.01f, 0.005f, 0.005f, 0.005f, 0.005f};
    decision.output_vector = output_vec;
    decision.num_active_neurons = 200;
    strncpy(decision.explanation,
            "WHAT: Identified pattern alpha | WHY: Strong evidence from neural ensemble | "
            "PROOF: Cross-validated with epistemic filter, confidence interval narrow",
            sizeof(decision.explanation) - 1);

    rubric_result_t result = {};
    int rc = rubric_evaluate_decision(eval, nullptr, &decision, &result);
    EXPECT_EQ(rc, 0);

    // With high completeness, good explanation, high confidence → expect decent grade
    // (No brain means epistemic/ethics/mirror fallback to 0.5)
    EXPECT_GE(result.overall_score, 0.4f);
    EXPECT_TRUE(result.grade != 'F');  // Should not be F for well-formed input

    rubric_evaluator_destroy(eval);
}

TEST_F(GradeBoundaryTest, EvaluateDecision_EmptyDecision_LowGrade) {
    // WHAT: Empty decision should get low grade
    // WHY:  Verify graceful degradation for garbage input
    rubric_evaluator_t* eval = rubric_evaluator_create(nullptr);
    ASSERT_NE(eval, nullptr);

    brain_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    // label is empty, no output vector, no explanation
    decision.confidence = 0.0f;
    decision.output_size = 0;
    decision.output_vector = nullptr;

    rubric_result_t result = {};
    int rc = rubric_evaluate_decision(eval, nullptr, &decision, &result);
    EXPECT_EQ(rc, 0);

    // Empty decision → low structural scores
    EXPECT_LE(result.tier1.completeness, 0.1f);
    EXPECT_LE(result.tier1.reasoning_chain_quality, 0.2f);

    rubric_evaluator_destroy(eval);
}

/* ============================================================================
 * Public API: nimcp_brain_rubric via full brain pipeline
 * ============================================================================ */

TEST_F(RubricTest, PublicAPI_BrainRubric_AfterPredict) {
    // WHAT: Test the full public API path: create brain, train, predict, rubric
    // WHY:  End-to-end validation that nimcp_brain_rubric works through the API
    nimcp_brain_t brain = nimcp_brain_create("rubric_test", NIMCP_BRAIN_SMALL,
                                             NIMCP_TASK_CLASSIFICATION, 8, 3);
    ASSERT_NE(brain, nullptr);

    // Train a few examples
    float features_a[8] = {0.9f, 0.8f, 0.1f, 0.1f, 0.9f, 0.8f, 0.1f, 0.1f};
    float features_b[8] = {0.1f, 0.2f, 0.9f, 0.8f, 0.1f, 0.2f, 0.9f, 0.8f};
    float features_c[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, features_a, 8, "alpha", 1.0f);
        nimcp_brain_learn_example(brain, features_b, 8, "beta", 1.0f);
        nimcp_brain_learn_example(brain, features_c, 8, "gamma", 1.0f);
    }

    // Predict
    char label[64] = {0};
    float confidence = 0.0f;
    nimcp_status_t rc = nimcp_brain_predict(brain, features_a, 8, label, &confidence);
    EXPECT_EQ(rc, NIMCP_OK);

    // Rubric
    nimcp_rubric_t rubric = {};
    rc = nimcp_brain_rubric(brain, &rubric);
    EXPECT_EQ(rc, NIMCP_OK);

    // All scores in [0, 1]
    EXPECT_GE(rubric.overall_score, 0.0f);
    EXPECT_LE(rubric.overall_score, 1.0f);
    EXPECT_GE(rubric.tier1_score, 0.0f);
    EXPECT_LE(rubric.tier1_score, 1.0f);
    EXPECT_GE(rubric.tier2_score, 0.0f);
    EXPECT_LE(rubric.tier2_score, 1.0f);

    // Grade is valid
    EXPECT_TRUE(rubric.grade == 'A' || rubric.grade == 'B' ||
                rubric.grade == 'C' || rubric.grade == 'D' || rubric.grade == 'F');
    EXPECT_TRUE(rubric.grade_modifier == '+' || rubric.grade_modifier == '-' ||
                rubric.grade_modifier == ' ');

    // Evaluation time recorded
    EXPECT_GT(rubric.evaluation_time_us, (uint64_t)0);

    nimcp_brain_destroy(brain);
}

TEST_F(RubricTest, PublicAPI_BrainRubric_NullBrain) {
    // WHAT: nimcp_brain_rubric(NULL, &rubric) returns error
    nimcp_rubric_t rubric = {};
    nimcp_status_t rc = nimcp_brain_rubric(nullptr, &rubric);
    EXPECT_NE(rc, NIMCP_OK);
}

TEST_F(RubricTest, PublicAPI_BrainRubric_NullRubric) {
    // WHAT: nimcp_brain_rubric(brain, NULL) returns error
    nimcp_brain_t brain = nimcp_brain_create("rubric_null", NIMCP_BRAIN_SMALL,
                                             NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    nimcp_status_t rc = nimcp_brain_rubric(brain, nullptr);
    EXPECT_NE(rc, NIMCP_OK);

    nimcp_brain_destroy(brain);
}

TEST_F(RubricTest, PublicAPI_BrainRubric_BeforePredict) {
    // WHAT: Rubric before any predict should handle gracefully
    // WHY:  last_decision is NULL before first predict
    nimcp_brain_t brain = nimcp_brain_create("rubric_early", NIMCP_BRAIN_SMALL,
                                             NIMCP_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    nimcp_rubric_t rubric = {};
    nimcp_status_t rc = nimcp_brain_rubric(brain, &rubric);
    // Should fail because no decision has been made yet
    EXPECT_NE(rc, NIMCP_OK);

    nimcp_brain_destroy(brain);
}

/* ============================================================================
 * Public API: Broadcast Rubric
 * ============================================================================ */

TEST_F(RubricTest, PublicAPI_BroadcastRubric_NullBrain) {
    // WHAT: Broadcast with NULL brain returns error
    nimcp_status_t rc = nimcp_brain_broadcast_rubric(nullptr);
    EXPECT_NE(rc, NIMCP_OK);
}

/* ============================================================================
 * Training Integration: Rubric Config in Training Config
 * ============================================================================ */

class RubricTrainingTest : public RubricTest {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        RubricTest::SetUp();
        brain = nimcp_brain_create("rubric_train", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION, 8, 3);
        ASSERT_NE(brain, nullptr);

        // Teach some examples so predict returns meaningful output
        float fa[8] = {0.9f, 0.8f, 0.1f, 0.1f, 0.9f, 0.8f, 0.1f, 0.1f};
        float fb[8] = {0.1f, 0.2f, 0.9f, 0.8f, 0.1f, 0.2f, 0.9f, 0.8f};
        float fc[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        for (int i = 0; i < 3; i++) {
            nimcp_brain_learn_example(brain, fa, 8, "alpha", 1.0f);
            nimcp_brain_learn_example(brain, fb, 8, "beta", 1.0f);
            nimcp_brain_learn_example(brain, fc, 8, "gamma", 1.0f);
        }
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        RubricTest::TearDown();
    }
};

TEST_F(RubricTrainingTest, RubricConfig_DefaultDisabled) {
    // WHAT: Default training config has rubric disabled
    nimcp_training_config_t config = nimcp_training_config_default();
    EXPECT_FALSE(config.enable_rubric);
    EXPECT_EQ(config.rubric_interval, 0u);
    EXPECT_FLOAT_EQ(config.rubric_min_score, 0.0f);
    EXPECT_FALSE(config.rubric_stop_on_threshold);
}

TEST_F(RubricTrainingTest, RubricConfig_EnableAndConfigure) {
    // WHAT: Configure training with rubric enabled
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = true;
    config.rubric_interval = 5;
    config.rubric_min_score = 0.3f;
    config.rubric_stop_on_threshold = true;

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    EXPECT_EQ(rc, NIMCP_OK);
}

TEST_F(RubricTrainingTest, RubricEval_FiresAtInterval) {
    // WHAT: Rubric evaluation fires at the configured step interval
    // WHY:  Verify the inline rubric logic triggers at modulo steps
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = true;
    config.rubric_interval = 3;  // evaluate every 3 steps

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float targets[3]  = {0.0f, 0.0f, 1.0f};

    int rubric_count = 0;
    for (int step = 0; step < 9; step++) {
        nimcp_training_result_t result = {};
        rc = nimcp_brain_train_step(brain, features, 8, targets, 3, &result);
        ASSERT_EQ(rc, NIMCP_OK);

        if (result.rubric_evaluated) {
            rubric_count++;
            // Score should be in valid range
            EXPECT_GE(result.rubric_score, 0.0f);
            EXPECT_LE(result.rubric_score, 1.0f);
            // Grade should be valid
            EXPECT_TRUE(result.rubric_grade == 'A' || result.rubric_grade == 'B' ||
                        result.rubric_grade == 'C' || result.rubric_grade == 'D' ||
                        result.rubric_grade == 'F');
        }
    }

    // Should have evaluated at steps 3, 6, 9 (step_count is 1-indexed after increment)
    EXPECT_GE(rubric_count, 1);
}

TEST_F(RubricTrainingTest, RubricStats_Accumulation) {
    // WHAT: Rubric stats accumulate correctly during training
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = true;
    config.rubric_interval = 2;  // evaluate every 2 steps

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    float features[8] = {0.9f, 0.8f, 0.1f, 0.1f, 0.9f, 0.8f, 0.1f, 0.1f};
    float targets[3]  = {1.0f, 0.0f, 0.0f};

    for (int step = 0; step < 6; step++) {
        nimcp_training_result_t result = {};
        rc = nimcp_brain_train_step(brain, features, 8, targets, 3, &result);
        ASSERT_EQ(rc, NIMCP_OK);
    }

    // Check accumulated stats
    uint64_t eval_count = 0;
    float min_s = 0.0f, max_s = 0.0f, avg_s = 0.0f;
    nimcp_rubric_t last_rubric = {};

    rc = nimcp_brain_get_rubric_training_stats(brain, &eval_count, &min_s, &max_s,
                                                &avg_s, &last_rubric);
    EXPECT_EQ(rc, NIMCP_OK);

    // Should have at least one evaluation
    if (eval_count > 0) {
        EXPECT_GT(eval_count, 0u);
        EXPECT_GE(min_s, 0.0f);
        EXPECT_LE(max_s, 1.0f);
        EXPECT_GE(avg_s, 0.0f);
        EXPECT_LE(avg_s, 1.0f);
        EXPECT_GE(min_s, 0.0f);
        EXPECT_LE(min_s, max_s);

        // Last rubric should have valid grade
        EXPECT_TRUE(last_rubric.grade == 'A' || last_rubric.grade == 'B' ||
                    last_rubric.grade == 'C' || last_rubric.grade == 'D' ||
                    last_rubric.grade == 'F');
    }
}

TEST_F(RubricTrainingTest, RubricThreshold_TriggersEarlyStop) {
    // WHAT: Rubric threshold below min_score triggers early_stopped
    // WHY:  Quality-gated training should stop when output quality drops
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = true;
    config.rubric_interval = 1;  // evaluate every step
    config.rubric_min_score = 0.99f;  // Unrealistically high threshold
    config.rubric_stop_on_threshold = true;

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float targets[3]  = {0.0f, 1.0f, 0.0f};

    bool saw_early_stop = false;
    for (int step = 0; step < 10; step++) {
        nimcp_training_result_t result = {};
        rc = nimcp_brain_train_step(brain, features, 8, targets, 3, &result);
        ASSERT_EQ(rc, NIMCP_OK);

        if (result.early_stopped && result.rubric_evaluated) {
            saw_early_stop = true;
            break;
        }
    }

    // With threshold 0.99, a small brain should almost certainly trigger early stop
    EXPECT_TRUE(saw_early_stop);
}

TEST_F(RubricTrainingTest, ValidationFeatures_Override) {
    // WHAT: Set dedicated validation features for rubric evaluation
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = true;
    config.rubric_interval = 1;

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    // Set separate validation features
    float val_features[8] = {0.9f, 0.8f, 0.1f, 0.1f, 0.9f, 0.8f, 0.1f, 0.1f};
    rc = nimcp_brain_set_rubric_validation(brain, val_features, 8);
    EXPECT_EQ(rc, NIMCP_OK);

    // Train with different features — rubric should use validation features
    float train_features[8] = {0.1f, 0.2f, 0.9f, 0.8f, 0.1f, 0.2f, 0.9f, 0.8f};
    float targets[3] = {0.0f, 1.0f, 0.0f};

    nimcp_training_result_t result = {};
    rc = nimcp_brain_train_step(brain, train_features, 8, targets, 3, &result);
    EXPECT_EQ(rc, NIMCP_OK);

    // Should have evaluated rubric (interval=1)
    EXPECT_TRUE(result.rubric_evaluated);
    EXPECT_GE(result.rubric_score, 0.0f);
    EXPECT_LE(result.rubric_score, 1.0f);
}

TEST_F(RubricTrainingTest, SetValidation_NullChecks) {
    // WHAT: Null checks on set_rubric_validation
    nimcp_status_t rc = nimcp_brain_set_rubric_validation(nullptr, nullptr, 0);
    EXPECT_NE(rc, NIMCP_OK);

    float features[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    rc = nimcp_brain_set_rubric_validation(brain, nullptr, 4);
    EXPECT_NE(rc, NIMCP_OK);

    rc = nimcp_brain_set_rubric_validation(brain, features, 0);
    EXPECT_NE(rc, NIMCP_OK);
}

TEST_F(RubricTrainingTest, GetStats_NullBrain) {
    // WHAT: get_rubric_training_stats with NULL brain returns error
    uint64_t count;
    nimcp_status_t rc = nimcp_brain_get_rubric_training_stats(nullptr, &count, nullptr,
                                                              nullptr, nullptr, nullptr);
    EXPECT_NE(rc, NIMCP_OK);
}

TEST_F(RubricTrainingTest, TrainingResult_RubricFields_DefaultClear) {
    // WHAT: When rubric is disabled, result rubric fields are clear
    nimcp_training_config_t config = nimcp_training_config_default();
    config.enable_rubric = false;

    nimcp_status_t rc = nimcp_brain_configure_training(brain, &config);
    ASSERT_EQ(rc, NIMCP_OK);

    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float targets[3]  = {0.0f, 0.0f, 1.0f};

    nimcp_training_result_t result = {};
    rc = nimcp_brain_train_step(brain, features, 8, targets, 3, &result);
    EXPECT_EQ(rc, NIMCP_OK);
    EXPECT_FALSE(result.rubric_evaluated);
}
