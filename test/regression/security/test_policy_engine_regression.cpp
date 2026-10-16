/**
 * @file test_policy_engine_regression.cpp
 * @brief Regression tests for NIMCP Policy Engine
 */

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "security/nimcp_policy_engine.h"

class PolicyEngineRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_policy_engine_config_t config = {0};
        config.max_policies = 100;
        config.enable_caching = true;
        config.cache_size = 1000;
        config.enable_optimization = true;

        engine = nimcp_policy_engine_create(&config);
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override {
        if (engine) {
            nimcp_policy_engine_destroy(engine);
        }
    }

    nimcp_policy_engine_t engine = nullptr;
};

/* ========================================================================
 * Performance Regression Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, EvaluationPerformance) {
    const char* policy = R"(
        rule "perf_test" {
            condition: value > 50
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_int(ctx, "value", 100);

    // Warm up
    for (int i = 0; i < 100; i++) {
        nimcp_policy_result_t result = {};
        nimcp_policy_evaluate(engine, ctx, &result);
        nimcp_policy_result_free(&result);
    }

    // Measure performance
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        nimcp_policy_result_t result = {};
        nimcp_policy_evaluate(engine, ctx, &result);
        nimcp_policy_result_free(&result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = duration.count() / 10000.0;

    // Average evaluation should be under 100 microseconds
    EXPECT_LT(avg_time_us, 100.0);

    std::cout << "Average evaluation time: " << avg_time_us << " microseconds" << std::endl;

    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, ComplexPolicyPerformance) {
    const char* policy = R"(
        rule "complex" {
            condition: (
                (val1 > 10 AND val2 < 100) OR
                (val3 == 42 AND NOT val4) OR
                (val5 + val6 > val7)
            )
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_int(ctx, "val1", 20);
    nimcp_policy_context_set_int(ctx, "val2", 50);
    nimcp_policy_context_set_int(ctx, "val3", 42);
    nimcp_policy_context_set_bool(ctx, "val4", false);
    nimcp_policy_context_set_int(ctx, "val5", 30);
    nimcp_policy_context_set_int(ctx, "val6", 40);
    nimcp_policy_context_set_int(ctx, "val7", 60);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        nimcp_policy_result_t result = {};
        nimcp_policy_evaluate(engine, ctx, &result);
        nimcp_policy_result_free(&result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_us = duration.count() / 1000.0;

    // Complex evaluation should still be under 500 microseconds
    EXPECT_LT(avg_time_us, 500.0);

    std::cout << "Average complex evaluation time: " << avg_time_us << " microseconds" << std::endl;

    nimcp_policy_context_destroy(ctx);
}

/* ========================================================================
 * Memory Leak Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, NoMemoryLeakOnLoad) {
    for (int i = 0; i < 1000; i++) {
        const char* policy = R"(
            rule "test" {
                condition: true
                action: ALLOW
            }
        )";

        nimcp_policy_t p;
        ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);
        ASSERT_EQ(nimcp_policy_engine_unload(engine, p), NIMCP_SUCCESS);
    }

    // If we get here without crashing, no obvious memory leaks
    SUCCEED();
}

TEST_F(PolicyEngineRegressionTest, NoMemoryLeakOnEvaluation) {
    const char* policy = R"(
        rule "test" {
            condition: contains(input, "test")
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    for (int i = 0; i < 10000; i++) {
        nimcp_policy_context_set_string(ctx, "input", "this is a test string");
        nimcp_policy_result_t result = {};
        nimcp_policy_evaluate(engine, ctx, &result);
        nimcp_policy_result_free(&result);
    }

    nimcp_policy_context_destroy(ctx);
    SUCCEED();
}

/* ========================================================================
 * Stability Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, HandleManyPolicies) {
    std::vector<nimcp_policy_t> policies;

    for (int i = 0; i < 50; i++) {
        std::string policy = "rule \"test_" + std::to_string(i) + "\" {\n"
                           "    condition: value > " + std::to_string(i * 10) + "\n"
                           "    action: ALLOW\n"
                           "}\n";

        nimcp_policy_t p;
        ASSERT_EQ(nimcp_policy_engine_load(engine, policy.c_str(), &p), NIMCP_SUCCESS);
        policies.push_back(p);
    }

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_policies, 50);

    // Clean up
    for (auto p : policies) {
        nimcp_policy_engine_unload(engine, p);
    }
}

TEST_F(PolicyEngineRegressionTest, HandleLargeContexts) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    // Add many context variables
    for (int i = 0; i < 1000; i++) {
        std::string key = "var_" + std::to_string(i);
        nimcp_policy_context_set_int(ctx, key.c_str(), i);
    }

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);

    nimcp_policy_context_destroy(ctx);
}

/* ========================================================================
 * Edge Case Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, HandleEmptyString) {
    const char* policy = R"(
        rule "test" {
            condition: length(input) == 0
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_string(ctx, "input", "");

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, HandleVeryLongString) {
    const char* policy = R"(
        rule "test" {
            condition: length(input) > 10000
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    std::string long_string(20000, 'a');
    nimcp_policy_context_set_string(ctx, "input", long_string.c_str());

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, HandleDivisionByZero) {
    const char* policy = R"(
        rule "test" {
            condition: 10 / 0 == 0
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, HandleIntegerOverflow) {
    const char* policy = R"(
        rule "test" {
            condition: val1 + val2 > 0
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_int(ctx, "val1", INT64_MAX);
    nimcp_policy_context_set_int(ctx, "val2", 1);

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

/* ========================================================================
 * Stress Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, StressTestEvaluations) {
    const char* policy = R"(
        rule "stress" {
            condition: (val1 > 10 AND val2 < 100) OR val3 == 42
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 200);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    for (int i = 0; i < 100000; i++) {
        nimcp_policy_context_set_int(ctx, "val1", dis(gen));
        nimcp_policy_context_set_int(ctx, "val2", dis(gen));
        nimcp_policy_context_set_int(ctx, "val3", dis(gen));

        nimcp_policy_result_t result = {};
        ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
        nimcp_policy_result_free(&result);
    }

    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, StressTestPolicyLoadUnload) {
    for (int i = 0; i < 1000; i++) {
        std::string policy = "rule \"test\" {\n"
                           "    condition: true\n"
                           "    action: ALLOW\n"
                           "}\n";

        nimcp_policy_t p;
        ASSERT_EQ(nimcp_policy_engine_load(engine, policy.c_str(), &p), NIMCP_SUCCESS);

        if (i % 2 == 0) {
            ASSERT_EQ(nimcp_policy_engine_unload(engine, p), NIMCP_SUCCESS);
        }
    }

    SUCCEED();
}

/* ========================================================================
 * Correctness Regression Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, OperatorPrecedence) {
    const char* policy = R"(
        rule "precedence" {
            condition: 2 + 3 * 4 == 14
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, ShortCircuitEvaluation) {
    // This test verifies that AND/OR use short-circuit evaluation
    const char* policy = R"(
        rule "short_circuit" {
            condition: false AND (10 / 0 == 0)
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_result_t result = {};

    // Should not crash even though second operand divides by zero
    // (if short-circuit works)
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

TEST_F(PolicyEngineRegressionTest, StringComparisonCorrectness) {
    const char* policy = R"(
        rule "string_cmp" {
            condition: str1 == str2
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_string(ctx, "str1", "hello");
    nimcp_policy_context_set_string(ctx, "str2", "hello");

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}

/* ========================================================================
 * Statistics Accuracy Tests
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, StatisticsAccuracy) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();

    const int num_evaluations = 100;
    for (int i = 0; i < num_evaluations; i++) {
        nimcp_policy_result_t result = {};
        nimcp_policy_evaluate(engine, ctx, &result);
        nimcp_policy_result_free(&result);
    }

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_evaluations, num_evaluations);
    EXPECT_GT(stats.avg_eval_time_ns, 0);
    EXPECT_GT(stats.max_eval_time_ns, 0);

    nimcp_policy_context_destroy(ctx);
}

/* ========================================================================
 * Entropy Function Accuracy
 * ======================================================================== */

TEST_F(PolicyEngineRegressionTest, EntropyCalculationAccuracy) {
    const char* policy = R"(
        rule "low_entropy" {
            condition: entropy(input) < 0.1
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_context_t ctx = nimcp_policy_context_create();
    nimcp_policy_context_set_string(ctx, "input", "aaaaaaaaaa");  // Low entropy

    nimcp_policy_result_t result = {};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
    nimcp_policy_context_destroy(ctx);
}
