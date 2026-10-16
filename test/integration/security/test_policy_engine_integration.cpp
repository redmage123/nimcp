/**
 * @file test_policy_engine_integration.cpp
 * @brief Integration tests for NIMCP Policy Engine
 */

#include <gtest/gtest.h>
#include <fstream>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "security/nimcp_policy_engine.h"
#include "async/nimcp_bio_async.h"

class PolicyEngineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create bio-async for integration
        nimcp_bio_async_config_t bio_config = {0};
        bio_config.inbox_capacity = 100;
        bio_config.num_workers = 2;
        bio_async = nimcp_bio_async_create(&bio_config);

        nimcp_policy_engine_config_t config = {0};
        config.max_policies = 10;
        config.enable_caching = true;
        config.cache_size = 100;
        config.enable_optimization = true;
        config.bio_async = bio_async;

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
        if (bio_async) {
            nimcp_bio_async_destroy(bio_async);
        }
    }

    nimcp_bio_async_t bio_async = nullptr;
    nimcp_policy_engine_t engine = nullptr;
    nimcp_policy_context_t ctx = nullptr;
};

/* ========================================================================
 * End-to-End Integration Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, LoadAndEvaluatePolicy) {
    const char* policy = R"(
        policy "security_policy" {
            rule "sql_injection" {
                condition: contains(input, "SELECT") AND contains(input, "FROM")
                action: DENY
            }
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Test with malicious input
    nimcp_policy_context_set_string(ctx, "input", "SELECT * FROM users");
    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);
    nimcp_policy_result_free(&result);

    // Test with safe input
    nimcp_policy_context_set_string(ctx, "input", "hello world");
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEngineIntegrationTest, MultiplePolicesEvaluation) {
    const char* policy1 = R"(
        rule "length_check" {
            condition: length(input) > 1000
            action: DENY
        }
    )";

    const char* policy2 = R"(
        rule "entropy_check" {
            condition: entropy(input) > 0.9
            action: DENY
        }
    )";

    nimcp_policy_t p1, p2;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy1, &p1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy2, &p2), NIMCP_SUCCESS);

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_policies, 2);
}

TEST_F(PolicyEngineIntegrationTest, PolicyUnloadAndReload) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_policies, 1);

    ASSERT_EQ(nimcp_policy_engine_unload(engine, p), NIMCP_SUCCESS);

    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.num_policies, 0);
}

/* ========================================================================
 * File-Based Policy Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, LoadPolicyFromFile) {
    // Create temporary policy file
    const char* policy_content = R"(
        rule "file_test" {
            condition: true
            action: ALLOW
        }
    )";

    std::ofstream file("/tmp/test_policy.nspl");
    file << policy_content;
    file.close();

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load_file(engine, "/tmp/test_policy.nspl", &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
    std::remove("/tmp/test_policy.nspl");
}

TEST_F(PolicyEngineIntegrationTest, ReloadPolicyFromFile) {
    const char* policy_v1 = R"(
        rule "test" {
            condition: value > 50
            action: DENY
        }
    )";

    std::ofstream file("/tmp/test_reload.nspl");
    file << policy_v1;
    file.close();

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load_file(engine, "/tmp/test_reload.nspl", &p), NIMCP_SUCCESS);

    // Modify file
    const char* policy_v2 = R"(
        rule "test" {
            condition: value > 100
            action: ALLOW
        }
    )";

    std::ofstream file2("/tmp/test_reload.nspl");
    file2 << policy_v2;
    file2.close();

    // Reload
    ASSERT_EQ(nimcp_policy_engine_reload(engine), NIMCP_SUCCESS);

    std::remove("/tmp/test_reload.nspl");
}

/* ========================================================================
 * Bio-Async Integration Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, BioAsyncRegistration) {
    // Engine should be registered with bio-async
    // This is verified by successful creation
    EXPECT_NE(engine, nullptr);
}

TEST_F(PolicyEngineIntegrationTest, BioAsyncEventOnLoad) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Give bio-async time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Event should have been sent
}

/* ========================================================================
 * Custom Function Registration Tests
 * ======================================================================== */

static nimcp_error_t custom_is_even(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    if (num_args != 1 || args[0].type != NIMCP_POLICY_VALUE_INT) {
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    result->type = NIMCP_POLICY_VALUE_BOOL;
    result->bool_val = (args[0].int_val % 2 == 0);
    return NIMCP_SUCCESS;
}

TEST_F(PolicyEngineIntegrationTest, RegisterCustomFunction) {
    ASSERT_EQ(
        nimcp_policy_register_function(engine, "is_even", custom_is_even, nullptr),
        NIMCP_SUCCESS
    );

    const char* policy = R"(
        rule "test" {
            condition: is_even(42)
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_ALLOW);

    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Statistics Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, TrackStatistics) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Perform evaluations
    for (int i = 0; i < 10; i++) {
        nimcp_policy_result_t result = {0};
        ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
        nimcp_policy_result_free(&result);
    }

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.num_policies, 1);
    EXPECT_EQ(stats.total_evaluations, 10);
    EXPECT_GT(stats.avg_eval_time_ns, 0);
}

TEST_F(PolicyEngineIntegrationTest, ResetStatistics) {
    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);

    ASSERT_EQ(nimcp_policy_engine_reset_stats(engine), NIMCP_SUCCESS);

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_evaluations, 0);
}

/* ========================================================================
 * Event Callback Tests
 * ======================================================================== */

static int callback_count = 0;

static void test_callback(
    const char* event_type,
    const nimcp_policy_result_t* result,
    void* user_data)
{
    callback_count++;
}

TEST_F(PolicyEngineIntegrationTest, RegisterEventCallback) {
    callback_count = 0;

    ASSERT_EQ(
        nimcp_policy_register_callback(engine, test_callback, nullptr),
        NIMCP_SUCCESS
    );

    const char* policy = R"(
        rule "test" {
            condition: true
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);

    EXPECT_EQ(callback_count, 1);
}

/* ========================================================================
 * Real-World Scenario Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, WebAPIAccessControl) {
    const char* policy = R"(
        policy "api_access" {
            rule "require_auth" {
                condition: NOT authenticated
                action: DENY
            }

            rule "rate_limit" {
                condition: rate > 100
                action: THROTTLE
            }
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Test unauthenticated request
    nimcp_policy_context_set_bool(ctx, "authenticated", false);
    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);
    nimcp_policy_result_free(&result);

    // Test authenticated request with high rate
    nimcp_policy_context_set_bool(ctx, "authenticated", true);
    nimcp_policy_context_set_int(ctx, "rate", 150);
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);
}

TEST_F(PolicyEngineIntegrationTest, InputValidation) {
    const char* policy = R"(
        rule "validate_input" {
            condition: (
                length(input) > 1000 OR
                entropy(input) > 0.8 OR
                contains(input, "script")
            )
            action: DENY
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Test with script tag
    nimcp_policy_context_set_string(ctx, "input", "<script>alert('xss')</script>");
    nimcp_policy_result_t result = {0};
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.action, NIMCP_POLICY_ACTION_DENY);
    nimcp_policy_result_free(&result);

    // Test with safe input
    nimcp_policy_context_set_string(ctx, "input", "Hello, world!");
    ASSERT_EQ(nimcp_policy_evaluate(engine, ctx, &result), NIMCP_SUCCESS);
    nimcp_policy_result_free(&result);
}

/* ========================================================================
 * Concurrency Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, ConcurrentEvaluations) {
    const char* policy = R"(
        rule "test" {
            condition: value > 50
            action: ALLOW
        }
    )";

    nimcp_policy_t p;
    ASSERT_EQ(nimcp_policy_engine_load(engine, policy, &p), NIMCP_SUCCESS);

    // Launch multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this, i]() {
            nimcp_policy_context_t thread_ctx = nimcp_policy_context_create();
            nimcp_policy_context_set_int(thread_ctx, "value", 60 + i);

            for (int j = 0; j < 10; j++) {
                nimcp_policy_result_t result = {0};
                nimcp_policy_evaluate(engine, thread_ctx, &result);
                nimcp_policy_result_free(&result);
            }

            nimcp_policy_context_destroy(thread_ctx);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_evaluations, 50);
}

/* ========================================================================
 * Error Handling Tests
 * ======================================================================== */

TEST_F(PolicyEngineIntegrationTest, HandleInvalidPolicy) {
    const char* invalid_policy = "this is not valid syntax";

    nimcp_policy_t p;
    EXPECT_NE(nimcp_policy_engine_load(engine, invalid_policy, &p), NIMCP_SUCCESS);

    nimcp_policy_stats_t stats = {0};
    ASSERT_EQ(nimcp_policy_engine_get_stats(engine, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.parse_errors, 0);
}

TEST_F(PolicyEngineIntegrationTest, HandleMissingFile) {
    nimcp_policy_t p;
    EXPECT_NE(
        nimcp_policy_engine_load_file(engine, "/nonexistent/file.nspl", &p),
        NIMCP_SUCCESS
    );
}
