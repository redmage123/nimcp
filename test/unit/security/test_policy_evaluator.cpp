/**
 * @file test_policy_evaluator.cpp
 * @brief Unit tests for NIMCP Policy Evaluator
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "security/nimcp_policy_engine.h"
#include "security/nimcp_policy_parser.h"

class PolicyEvaluatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_policy_engine_config_t config = {0};
        config.max_policies = 10;
        config.enable_caching = false;
        config.enable_optimization = false;

        engine = nimcp_policy_engine_create(&config);
        ASSERT_NE(engine, nullptr);

        ctx = nimcp_policy_context_create();
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            nimcp_policy_context_destroy(ctx);
        }
        if (engine) {
            nimcp_policy_engine_destroy(engine);
        }
    }

    nimcp_policy_engine_t engine = nullptr;
    nimcp_policy_context_t ctx = nullptr;
};

/* ========================================================================
 * Basic Evaluation Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateSimpleTrue) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateSimpleFalse) {
    const char* policy = R"(
        rule "test" {
            condition: false
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Comparison Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateIntegerComparison) {
    const char* policy = R"(
        rule "test" {
            condition: 10 > 5
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateEquality) {
    const char* policy = R"(
        rule "test" {
            condition: 42 == 42
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateInequality) {
    const char* policy = R"(
        rule "test" {
            condition: 10 != 20
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Arithmetic Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateAddition) {
    const char* policy = R"(
        rule "test" {
            condition: 10 + 5 == 15
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateSubtraction) {
    const char* policy = R"(
        rule "test" {
            condition: 20 - 5 == 15
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateMultiplication) {
    const char* policy = R"(
        rule "test" {
            condition: 10 * 5 == 50
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateDivision) {
    const char* policy = R"(
        rule "test" {
            condition: 20 / 4 == 5
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Logical Operator Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateAnd) {
    const char* policy = R"(
        rule "test" {
            condition: true AND true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateOr) {
    const char* policy = R"(
        rule "test" {
            condition: false OR true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateNot) {
    const char* policy = R"(
        rule "test" {
            condition: NOT false
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Context Variable Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateContextInteger) {
    const char* policy = R"(
        rule "test" {
            condition: value > 50
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_set_int(ctx, "value", 100);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateContextString) {
    const char* policy = R"(
        rule "test" {
            condition: contains(input, "test")
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_set_string(ctx, "input", "this is a test string");

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateContextBoolean) {
    const char* policy = R"(
        rule "test" {
            condition: authenticated == true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_set_bool(ctx, "authenticated", true);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Built-in Function Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateContainsFunction) {
    const char* policy = R"(
        rule "test" {
            condition: contains("hello world", "world")
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateLengthFunction) {
    const char* policy = R"(
        rule "test" {
            condition: length("hello") == 5
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateEntropyFunction) {
    const char* policy = R"(
        rule "test" {
            condition: entropy("aaaaa") < 0.1
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Complex Evaluation Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateComplexCondition) {
    const char* policy = R"(
        rule "complex" {
            condition: (10 > 5 AND 20 < 30) OR NOT false
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateSQLInjectionDetection) {
    const char* policy = R"(
        rule "block_sql_injection" {
            condition: contains(input, "SELECT") AND contains(input, "FROM")
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_set_string(ctx, "input", "SELECT * FROM users");

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Performance Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, MeasureEvaluationTime) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_GT(result.eval_time_ns, 0);
    EXPECT_LT(result.eval_time_ns, 1000000);  // Less than 1ms

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Action Type Tests
 * ======================================================================== */

TEST_F(PolicyEvaluatorTest, EvaluateAllowAction) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateDenyAction) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);

    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEvaluatorTest, EvaluateThrottleAction) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: THROTTLE
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_THROTTLE);

    nimcp_policy_result_free(&result);
}
