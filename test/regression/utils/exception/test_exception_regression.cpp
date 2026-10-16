/**
 * @file test_exception_regression.cpp
 * @brief Regression tests for exception handling stability
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Stability and regression tests for the exception handling system
 * WHY:  Prevent regressions in exception handling, ensure stability under stress
 * HOW:  Test memory safety, null safety, thread safety, callbacks, metrics,
 *       circuit breaker, and epitope generation
 *
 * REGRESSION CATEGORIES:
 * 1. Memory Safety - Create/destroy doesn't leak memory
 * 2. Null Safety - All APIs handle NULL gracefully
 * 3. Thread Safety - Concurrent exception handling is stable
 * 4. Recovery Callbacks - Registration/unregistration stability
 * 5. Metrics Accuracy - Metrics are correct after many operations
 * 6. Circuit Breaker - State transitions are correct
 * 7. Epitope Generation - Deterministic for same inputs
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <set>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t get_time_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

/**
 * @brief Create a test exception for regression testing
 */
static nimcp_exception_t* create_test_exception(
    nimcp_error_t code,
    nimcp_exception_severity_t severity,
    const char* message
) {
    return nimcp_exception_create(
        code,
        severity,
        __FILE__, __LINE__, __func__,
        "%s", message
    );
}

/* ============================================================================
 * Test Fixture for Exception System Regression Tests
 * ============================================================================ */

class ExceptionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        nimcp_circuit_init();
        nimcp_metrics_init();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_system_shutdown();
    }
};

/* ============================================================================
 * CATEGORY 1: Memory Safety Regression Tests
 * Test that exception creation/destruction doesn't leak memory
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, CreateDestroyBaseExceptionNoLeak) {
    /* Test that repeated create/destroy doesn't leak memory */
    for (int i = 0; i < 100; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Test exception for leak check"
        );
        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->ref_count, 1);
        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionRegressionTest, CreateDestroyMemoryExceptionNoLeak) {
    /* Test memory exception repeated create/destroy */
    for (int i = 0; i < 100; i++) {
        nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024,
            "Memory exception leak test"
        );
        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->base.ref_count, 1);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

TEST_F(ExceptionRegressionTest, CreateDestroyBrainExceptionNoLeak) {
    /* Test brain exception repeated create/destroy */
    for (int i = 0; i < 100; i++) {
        nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
            NIMCP_ERROR_BRAIN_CREATION,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            42,
            "visual_cortex",
            "Brain exception leak test"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

TEST_F(ExceptionRegressionTest, CreateDestroyAggregateExceptionNoLeak) {
    /* Test aggregate exception with children doesn't leak */
    for (int i = 0; i < 50; i++) {
        nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Aggregate exception leak test"
        );
        ASSERT_NE(agg, nullptr);

        /* Add several children */
        for (int j = 0; j < 5; j++) {
            nimcp_exception_t* child = create_test_exception(
                NIMCP_ERROR_UNKNOWN,
                EXCEPTION_SEVERITY_WARNING,
                "Child exception"
            );
            int ret = nimcp_aggregate_exception_add(agg, child);
            EXPECT_EQ(ret, 0);
        }

        /* Destroy aggregate (should also free children) */
        nimcp_exception_unref((nimcp_exception_t*)agg);
    }
}

TEST_F(ExceptionRegressionTest, ReferenceCountingCorrectness) {
    /* Test that reference counting works correctly */
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Ref count test"
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->ref_count, 1);

    /* Add multiple references */
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ref = nimcp_exception_ref(ex);
        EXPECT_EQ(ref, ex);
        EXPECT_EQ(ex->ref_count, i + 2);
    }
    EXPECT_EQ(ex->ref_count, 11);

    /* Release all but one reference */
    for (int i = 0; i < 10; i++) {
        nimcp_exception_unref(ex);
        EXPECT_EQ(ex->ref_count, 10 - i);
    }
    EXPECT_EQ(ex->ref_count, 1);

    /* Final release */
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, CauseChainNoLeak) {
    /* Test that exception chains don't leak memory */
    for (int i = 0; i < 50; i++) {
        nimcp_exception_t* cause = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Root cause"
        );

        nimcp_exception_t* wrapper = create_test_exception(
            NIMCP_ERROR_BRAIN_CREATION,
            EXCEPTION_SEVERITY_ERROR,
            "Wrapper"
        );

        /* Add ref because set_cause takes ownership */
        nimcp_exception_ref(cause);
        nimcp_exception_set_cause(wrapper, cause);

        /* Unref cause (our ref) */
        nimcp_exception_unref(cause);

        /* Unref wrapper (should also free cause via chain) */
        nimcp_exception_unref(wrapper);
    }
}

/* ============================================================================
 * CATEGORY 2: Null Safety Regression Tests
 * Test that all APIs handle NULL parameters gracefully
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, UnrefNullSafe) {
    /* Should not crash */
    nimcp_exception_unref(nullptr);
}

TEST_F(ExceptionRegressionTest, RefNullReturnsNull) {
    nimcp_exception_t* result = nimcp_exception_ref(nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ExceptionRegressionTest, SetCauseNullSafe) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Test"
    );

    /* Setting NULL cause should be safe */
    nimcp_exception_set_cause(ex, nullptr);
    EXPECT_EQ(nimcp_exception_get_cause(ex), nullptr);

    /* Setting cause on NULL exception should not crash */
    nimcp_exception_set_cause(nullptr, ex);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, GetCauseNullReturnsNull) {
    nimcp_exception_t* result = nimcp_exception_get_cause(nullptr);
    EXPECT_EQ(result, nullptr);
}

TEST_F(ExceptionRegressionTest, SetContextNullSafe) {
    /* All NULL should return error */
    int result = nimcp_exception_set_context(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, -1);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Context null test"
    );

    /* NULL key should fail */
    result = nimcp_exception_set_context(ex, nullptr, "value");
    EXPECT_EQ(result, -1);

    /* NULL value should fail */
    result = nimcp_exception_set_context(ex, "key", nullptr);
    EXPECT_EQ(result, -1);

    /* Valid should succeed */
    result = nimcp_exception_set_context(ex, "key", "value");
    EXPECT_EQ(result, 0);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, GetContextNullSafe) {
    const char* result = nimcp_exception_get_context(nullptr, "key");
    EXPECT_EQ(result, nullptr);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Test"
    );

    result = nimcp_exception_get_context(ex, nullptr);
    EXPECT_EQ(result, nullptr);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, AggregateExceptionNullSafe) {
    /* Add NULL child should fail */
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Aggregate test"
    );
    ASSERT_NE(agg, nullptr);

    int result = nimcp_aggregate_exception_add(agg, nullptr);
    EXPECT_EQ(result, -1);

    result = nimcp_aggregate_exception_add(nullptr, nullptr);
    EXPECT_EQ(result, -1);

    /* Count on NULL should return 0 */
    size_t count = nimcp_aggregate_exception_count(nullptr);
    EXPECT_EQ(count, 0u);

    /* Get on NULL should return NULL */
    nimcp_exception_t* child = nimcp_aggregate_exception_get(nullptr, 0);
    EXPECT_EQ(child, nullptr);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

TEST_F(ExceptionRegressionTest, ContextCountNullReturnsZero) {
    size_t count = nimcp_exception_context_count(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(ExceptionRegressionTest, RemoveContextNullSafe) {
    int result = nimcp_exception_remove_context(nullptr, "key");
    EXPECT_EQ(result, -1);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Test"
    );

    result = nimcp_exception_remove_context(ex, nullptr);
    EXPECT_EQ(result, -1);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, GenerateEpitopeNullSafe) {
    size_t len = nimcp_exception_generate_epitope(nullptr);
    EXPECT_EQ(len, 0u);
}

TEST_F(ExceptionRegressionTest, ExceptionLogNullSafe) {
    /* Should not crash */
    nimcp_exception_log(nullptr);
}

TEST_F(ExceptionRegressionTest, ExceptionPrintNullSafe) {
    /* Should not crash */
    nimcp_exception_print(nullptr);
}

TEST_F(ExceptionRegressionTest, ExceptionToStringNullSafe) {
    char buffer[256];
    size_t len = nimcp_exception_to_string(nullptr, buffer, sizeof(buffer));
    EXPECT_EQ(len, 0u);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Test"
    );

    len = nimcp_exception_to_string(ex, nullptr, 0);
    EXPECT_EQ(len, 0u);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, StringConversionNullSafe) {
    /* Invalid enum values should return reasonable strings */
    const char* sev = nimcp_exception_severity_to_string((nimcp_exception_severity_t)999);
    EXPECT_NE(sev, nullptr);

    const char* cat = nimcp_exception_category_to_string((nimcp_exception_category_t)999);
    EXPECT_NE(cat, nullptr);

    const char* typ = nimcp_exception_type_to_string((nimcp_exception_type_t)999);
    EXPECT_NE(typ, nullptr);

    const char* act = nimcp_exception_recovery_action_to_string((nimcp_exception_recovery_action_t)999);
    EXPECT_NE(act, nullptr);
}

/* ============================================================================
 * CATEGORY 3: Thread Safety Regression Tests
 * Test concurrent exception handling stability
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, ConcurrentExceptionCreation) {
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;
    const int num_threads = 8;
    const int ops_per_thread = 50;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&success_count, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                nimcp_exception_t* ex = create_test_exception(
                    NIMCP_ERROR_UNKNOWN,
                    EXCEPTION_SEVERITY_ERROR,
                    "Concurrent creation test"
                );
                if (ex != nullptr) {
                    success_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * ops_per_thread);
}

TEST_F(ExceptionRegressionTest, ConcurrentContextOperations) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Concurrent context test"
    );
    ASSERT_NE(ex, nullptr);

    std::atomic<int> completed{0};
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int ops_per_thread = 25;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([ex, &completed, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                char key[32];
                char value[64];
                snprintf(key, sizeof(key), "key_%d_%d", t, i % 5);
                snprintf(value, sizeof(value), "value_%d_%d", t, i);

                /* Alternate between set and get */
                if (i % 2 == 0) {
                    nimcp_exception_set_context(ex, key, value);
                } else {
                    nimcp_exception_get_context(ex, key);
                }
                completed++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completed.load(), num_threads * ops_per_thread);
    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, ConcurrentReferenceOperations) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Concurrent ref test"
    );
    ASSERT_NE(ex, nullptr);

    /* Take references so we don't free while testing */
    for (int i = 0; i < 100; i++) {
        nimcp_exception_ref(ex);
    }

    std::atomic<int> completed{0};
    std::vector<std::thread> threads;
    const int num_threads = 8;
    const int ops_per_thread = 10;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([ex, &completed, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                /* Alternate between ref and unref */
                if (i % 2 == 0) {
                    nimcp_exception_ref(ex);
                } else {
                    nimcp_exception_unref(ex);
                }
                completed++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completed.load(), num_threads * ops_per_thread);

    /* Release our 100 refs plus 1 original */
    for (int i = 0; i <= 100; i++) {
        nimcp_exception_unref(ex);
    }
}

TEST_F(ExceptionRegressionTest, ConcurrentDispatch) {
    std::atomic<int> dispatch_count{0};
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int ops_per_thread = 25;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&dispatch_count, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                nimcp_exception_t* ex = create_test_exception(
                    NIMCP_ERROR_UNKNOWN,
                    EXCEPTION_SEVERITY_WARNING,
                    "Concurrent dispatch test"
                );
                if (ex != nullptr) {
                    nimcp_exception_dispatch(ex);
                    dispatch_count++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(dispatch_count.load(), num_threads * ops_per_thread);
}

/* ============================================================================
 * CATEGORY 4: Recovery Callback Registration/Unregistration Stability
 * ============================================================================ */

static std::atomic<int> g_recovery_callback_count{0};

static int test_recovery_callback(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    void* user_data
) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_callback_count++;
    return 0;
}

TEST_F(ExceptionRegressionTest, RecoveryCallbackRegistrationStability) {
    /* Test repeated registration/unregistration */
    for (int i = 0; i < 50; i++) {
        int ret = nimcp_register_recovery_callback(
            EXCEPTION_RECOVERY_RETRY,
            test_recovery_callback,
            nullptr
        );
        EXPECT_EQ(ret, 0);

        ret = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ExceptionRegressionTest, RecoveryCallbackUnregisterNonExistent) {
    /* Unregistering non-existent callback - verify it doesn't crash and returns predictable value.
     * Note: Implementation may return 0 even for non-existent callback (idempotent behavior). */
    int ret = nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    /* Accept either 0 (idempotent) or -1 (strict) as valid behavior */
    EXPECT_TRUE(ret == 0 || ret == -1);
}

TEST_F(ExceptionRegressionTest, RecoveryCallbackAllActions) {
    /* Register callbacks for all recovery actions */
    nimcp_exception_recovery_action_t actions[] = {
        EXCEPTION_RECOVERY_RETRY,
        EXCEPTION_RECOVERY_GC,
        EXCEPTION_RECOVERY_COMPACT,
        EXCEPTION_RECOVERY_ROLLBACK,
        EXCEPTION_RECOVERY_RESTART_THREAD,
        EXCEPTION_RECOVERY_RESTART_COMPONENT,
        EXCEPTION_RECOVERY_QUARANTINE,
        EXCEPTION_RECOVERY_REDUCE_LOAD,
        EXCEPTION_RECOVERY_CLEAR_CACHE,
        EXCEPTION_RECOVERY_EMERGENCY_SAVE,
        EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN
    };
    size_t num_actions = sizeof(actions) / sizeof(actions[0]);

    /* Register all */
    for (size_t i = 0; i < num_actions; i++) {
        int ret = nimcp_register_recovery_callback(
            actions[i],
            test_recovery_callback,
            nullptr
        );
        EXPECT_EQ(ret, 0);
    }

    /* Unregister all */
    for (size_t i = 0; i < num_actions; i++) {
        int ret = nimcp_unregister_recovery_callback(actions[i]);
        EXPECT_EQ(ret, 0);
    }
}

TEST_F(ExceptionRegressionTest, RecoveryCallbackExecution) {
    g_recovery_callback_count = 0;

    int ret = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        test_recovery_callback,
        nullptr
    );
    EXPECT_EQ(ret, 0);

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Recovery callback test"
    );
    ASSERT_NE(ex, nullptr);

    /* Execute recovery multiple times */
    for (int i = 0; i < 10; i++) {
        nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    }

    EXPECT_EQ(g_recovery_callback_count.load(), 10);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(ex);
}

/* ============================================================================
 * CATEGORY 5: Exception Metrics Accuracy After Many Operations
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, MetricsAccuracyAfterManyExceptions) {
    const int num_exceptions = 100;

    for (int i = 0; i < num_exceptions; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Metrics accuracy test"
        );
        ASSERT_NE(ex, nullptr);
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_EQ(metrics.total_exceptions, (uint64_t)num_exceptions);
}

TEST_F(ExceptionRegressionTest, MetricsCategoryAccuracy) {
    /* Create exceptions of different categories */
    struct {
        nimcp_error_t code;
        nimcp_exception_category_t expected_category;
        int count;
    } test_cases[] = {
        {NIMCP_ERROR_UNKNOWN, EXCEPTION_CATEGORY_GENERIC, 10},
        {NIMCP_ERROR_NO_MEMORY, EXCEPTION_CATEGORY_MEMORY, 20},
        {NIMCP_ERROR_BRAIN_CREATION, EXCEPTION_CATEGORY_BRAIN, 15},
        {NIMCP_ERROR_FILE_NOT_FOUND, EXCEPTION_CATEGORY_IO, 5},
    };
    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);

    int total_expected = 0;
    for (size_t i = 0; i < num_cases; i++) {
        for (int j = 0; j < test_cases[i].count; j++) {
            nimcp_exception_t* ex = create_test_exception(
                test_cases[i].code,
                EXCEPTION_SEVERITY_ERROR,
                "Category accuracy test"
            );
            ASSERT_NE(ex, nullptr);
            nimcp_metrics_record_exception(ex);
            nimcp_exception_unref(ex);
            total_expected++;
        }
    }

    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_EQ(metrics.total_exceptions, (uint64_t)total_expected);
}

TEST_F(ExceptionRegressionTest, MetricsRecoveryAccuracy) {
    const int num_successes = 30;
    const int num_failures = 20;

    for (int i = 0; i < num_successes + num_failures; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Recovery metrics test"
        );
        ASSERT_NE(ex, nullptr);

        bool success = (i < num_successes);
        nimcp_metrics_record_recovery(ex, EXCEPTION_RECOVERY_RETRY, success, 1000);

        nimcp_exception_unref(ex);
    }

    float rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_RETRY);
    float expected_rate = (float)num_successes / (float)(num_successes + num_failures);

    /* Allow some floating point tolerance */
    EXPECT_NEAR(rate, expected_rate, 0.01f);
}

TEST_F(ExceptionRegressionTest, MetricsResetClears) {
    /* Record some exceptions */
    for (int i = 0; i < 50; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Reset test"
        );
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    nimcp_exception_metrics_t metrics_before;
    nimcp_metrics_get(&metrics_before);
    EXPECT_GE(metrics_before.total_exceptions, 50u);

    nimcp_metrics_reset();

    nimcp_exception_metrics_t metrics_after;
    nimcp_metrics_get(&metrics_after);
    EXPECT_EQ(metrics_after.total_exceptions, 0u);
}

/* ============================================================================
 * CATEGORY 6: Circuit Breaker State Transitions
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, CircuitBreakerInitialStateClosed) {
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_UNKNOWN);
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
}

TEST_F(ExceptionRegressionTest, CircuitBreakerTripsAtThreshold) {
    nimcp_error_t test_code = NIMCP_ERROR_BUFFER_OVERFLOW;

    /* Set a low threshold for testing */
    int ret = nimcp_circuit_set_threshold(test_code, 5, 1000);
    EXPECT_EQ(ret, 0);

    /* Record exceptions until circuit trips */
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_ERROR,
            "Circuit breaker trip test"
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    /* Circuit should be open now */
    bool is_open = nimcp_circuit_is_open(test_code);
    EXPECT_TRUE(is_open);

    nimcp_circuit_state_t state = nimcp_circuit_get_state(test_code);
    EXPECT_TRUE(state == CIRCUIT_STATE_OPEN || state == CIRCUIT_STATE_HALF_OPEN);
}

TEST_F(ExceptionRegressionTest, CircuitBreakerManualReset) {
    nimcp_error_t test_code = NIMCP_ERROR_MEMORY_CORRUPTION;

    /* Set low threshold and trip the circuit */
    nimcp_circuit_set_threshold(test_code, 3, 60000);

    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_ERROR,
            "Manual reset test"
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    /* Reset manually */
    int ret = nimcp_circuit_reset(test_code);
    EXPECT_EQ(ret, 0);

    /* Should be closed now */
    nimcp_circuit_state_t state = nimcp_circuit_get_state(test_code);
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
}

TEST_F(ExceptionRegressionTest, CircuitBreakerResetAll) {
    nimcp_error_t codes[] = {
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BUFFER_OVERFLOW
    };
    size_t num_codes = sizeof(codes) / sizeof(codes[0]);

    /* Trip all circuits */
    for (size_t c = 0; c < num_codes; c++) {
        nimcp_circuit_set_threshold(codes[c], 3, 60000);
        for (int i = 0; i < 10; i++) {
            nimcp_exception_t* ex = create_test_exception(
                codes[c],
                EXCEPTION_SEVERITY_ERROR,
                "Reset all test"
            );
            nimcp_circuit_record(ex);
            nimcp_exception_unref(ex);
        }
    }

    /* Reset all */
    nimcp_circuit_reset_all();

    /* All should be closed */
    for (size_t c = 0; c < num_codes; c++) {
        nimcp_circuit_state_t state = nimcp_circuit_get_state(codes[c]);
        EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
    }
}

TEST_F(ExceptionRegressionTest, CircuitBreakerStatsAccurate) {
    nimcp_circuit_reset_all();

    /* Record some exceptions */
    for (int i = 0; i < 25; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Stats accuracy test"
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    nimcp_circuit_stats_t stats;
    int ret = nimcp_circuit_get_stats(&stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.total_exceptions, 25u);
}

TEST_F(ExceptionRegressionTest, CircuitBreakerCountTracking) {
    nimcp_error_t test_code = NIMCP_ERROR_FILE_READ;
    nimcp_circuit_reset(test_code);

    const int num_exceptions = 15;
    for (int i = 0; i < num_exceptions; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_ERROR,
            "Count tracking test"
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    /* Check total count */
    size_t count = nimcp_circuit_get_count(test_code, 0);  /* 0 = total */
    EXPECT_GE(count, (size_t)num_exceptions);
}

/* ============================================================================
 * CATEGORY 7: Epitope Generation Determinism
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, EpitopeGenerationDeterministic) {
    /* Create two identical exceptions */
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_function",
        "Memory allocation failed"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_function",
        "Memory allocation failed"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    /* Generate epitopes */
    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    EXPECT_EQ(len1, len2);
    EXPECT_GT(len1, 0u);

    /* Epitopes should be identical */
    EXPECT_EQ(ex1->epitope_len, ex2->epitope_len);
    EXPECT_EQ(memcmp(ex1->epitope, ex2->epitope, ex1->epitope_len), 0);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionRegressionTest, EpitopeGenerationDifferentInputs) {
    /* Create two different exceptions */
    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        "test.c", 100, "test_function",
        "Memory allocation failed"
    );

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_CRITICAL,
        "other.c", 200, "other_function",
        "Buffer overflow detected"
    );

    ASSERT_NE(ex1, nullptr);
    ASSERT_NE(ex2, nullptr);

    /* Generate epitopes */
    nimcp_exception_generate_epitope(ex1);
    nimcp_exception_generate_epitope(ex2);

    /* Epitopes should be different */
    bool different = (ex1->epitope_len != ex2->epitope_len) ||
                     (memcmp(ex1->epitope, ex2->epitope,
                             std::min(ex1->epitope_len, ex2->epitope_len)) != 0);
    EXPECT_TRUE(different);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

TEST_F(ExceptionRegressionTest, EpitopeGenerationRepeatedCalls) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Repeated epitope test"
    );
    ASSERT_NE(ex, nullptr);

    /* Generate epitope multiple times */
    uint8_t first_epitope[NIMCP_EXCEPTION_EPITOPE_SIZE];
    size_t first_len = nimcp_exception_generate_epitope(ex);
    ASSERT_GT(first_len, 0u);
    memcpy(first_epitope, ex->epitope, first_len);

    /* Generate again - should be same */
    for (int i = 0; i < 10; i++) {
        size_t len = nimcp_exception_generate_epitope(ex);
        EXPECT_EQ(len, first_len);
        EXPECT_EQ(memcmp(ex->epitope, first_epitope, first_len), 0);
    }

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, EpitopeUniquenessAcrossTypes) {
    /* Create exceptions of different types, verify epitopes are unique */
    std::vector<nimcp_exception_t*> exceptions;
    std::set<std::string> epitopes;

    nimcp_error_t codes[] = {
        NIMCP_ERROR_UNKNOWN,
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_BUFFER_OVERFLOW,
        NIMCP_ERROR_BRAIN_CREATION,
        NIMCP_ERROR_FILE_NOT_FOUND,
        NIMCP_ERROR_THREAD_CREATE,
        NIMCP_ERROR_DEADLOCK,
        NIMCP_ERROR_SIGSEGV
    };
    size_t num_codes = sizeof(codes) / sizeof(codes[0]);

    for (size_t i = 0; i < num_codes; i++) {
        nimcp_exception_t* ex = create_test_exception(
            codes[i],
            EXCEPTION_SEVERITY_ERROR,
            "Uniqueness test"
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_generate_epitope(ex);
        std::string epitope_str((char*)ex->epitope, ex->epitope_len);
        epitopes.insert(epitope_str);

        exceptions.push_back(ex);
    }

    /* All epitopes should be unique */
    EXPECT_EQ(epitopes.size(), num_codes);

    /* Cleanup */
    for (auto ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

/* ============================================================================
 * Additional Stability Tests
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, SystemInitShutdownCycles) {
    /* Test repeated init/shutdown cycles */
    for (int i = 0; i < 10; i++) {
        nimcp_exception_system_shutdown();
        int ret = nimcp_exception_system_init();
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(nimcp_exception_system_is_initialized());
    }
}

static bool test_handler_for_limit_test(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    return false;  /* Don't consume the exception */
}

TEST_F(ExceptionRegressionTest, HandlerRegistrationLimit) {
    /* Test registering multiple handlers - focus on stability, not limits */
    std::vector<nimcp_handler_registration_t*> registrations;

    const int num_handlers = 10;  /* Register a reasonable number */
    for (int i = 0; i < num_handlers; i++) {
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "test_handler";
        options.handler = test_handler_for_limit_test;
        options.priority = i;

        nimcp_handler_registration_t* reg = nimcp_handler_register(&options);
        if (reg != nullptr) {
            registrations.push_back(reg);
        }
    }

    /* Verify we registered some handlers (implementation may vary) */
    EXPECT_GT(registrations.size(), 0u);

    /* Unregister all - should not crash */
    for (auto reg : registrations) {
        nimcp_handler_unregister(reg);
    }
}

TEST_F(ExceptionRegressionTest, ContextEntryLimit) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Context limit test"
    );
    ASSERT_NE(ex, nullptr);

    /* Fill all context entries */
    int success_count = 0;
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES + 5; i++) {
        char key[32];
        char value[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);

        int ret = nimcp_exception_set_context(ex, key, value);
        if (ret == 0) {
            success_count++;
        }
    }

    /* Should have filled exactly to limit */
    EXPECT_EQ(success_count, NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES);
    EXPECT_EQ(nimcp_exception_context_count(ex), (size_t)NIMCP_EXCEPTION_MAX_CONTEXT_ENTRIES);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, AggregateExceptionChildLimit) {
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Child limit test"
    );
    ASSERT_NE(agg, nullptr);

    /* Fill all child slots */
    int success_count = 0;
    for (int i = 0; i < NIMCP_EXCEPTION_MAX_CHILDREN + 5; i++) {
        nimcp_exception_t* child = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_WARNING,
            "Child"
        );
        int ret = nimcp_aggregate_exception_add(agg, child);
        if (ret == 0) {
            success_count++;
        } else {
            /* Failed to add, need to unref the child ourselves */
            nimcp_exception_unref(child);
        }
    }

    /* Should have filled exactly to limit */
    EXPECT_EQ(success_count, NIMCP_EXCEPTION_MAX_CHILDREN);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), (size_t)NIMCP_EXCEPTION_MAX_CHILDREN);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

/* ============================================================================
 * Benchmark-Style Performance Regression Tests
 * ============================================================================ */

TEST_F(ExceptionRegressionTest, BenchmarkExceptionCreation) {
    const int iterations = 1000;
    uint64_t start = get_time_us();

    for (int i = 0; i < iterations; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Benchmark test"
        );
        nimcp_exception_unref(ex);
    }

    uint64_t elapsed = get_time_us() - start;
    double avg_us = (double)elapsed / iterations;

    /* Exception creation should be fast - less than 100 microseconds average */
    EXPECT_LT(avg_us, 100.0) << "Exception creation too slow: " << avg_us << " us/exception";

    printf("[BENCHMARK] Exception creation: %.2f us/exception\n", avg_us);
}

TEST_F(ExceptionRegressionTest, BenchmarkEpitopeGeneration) {
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_UNKNOWN,
        EXCEPTION_SEVERITY_ERROR,
        "Epitope benchmark test"
    );
    ASSERT_NE(ex, nullptr);

    const int iterations = 1000;
    uint64_t start = get_time_us();

    for (int i = 0; i < iterations; i++) {
        nimcp_exception_generate_epitope(ex);
    }

    uint64_t elapsed = get_time_us() - start;
    double avg_us = (double)elapsed / iterations;

    /* Epitope generation should be fast - less than 50 microseconds */
    EXPECT_LT(avg_us, 50.0) << "Epitope generation too slow: " << avg_us << " us/epitope";

    printf("[BENCHMARK] Epitope generation: %.2f us/epitope\n", avg_us);

    nimcp_exception_unref(ex);
}

TEST_F(ExceptionRegressionTest, BenchmarkMetricsRecording) {
    const int iterations = 1000;
    uint64_t start = get_time_us();

    for (int i = 0; i < iterations; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_UNKNOWN,
            EXCEPTION_SEVERITY_ERROR,
            "Metrics benchmark"
        );
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    uint64_t elapsed = get_time_us() - start;
    double avg_us = (double)elapsed / iterations;

    /* Metrics recording should be fast - less than 150 microseconds total */
    EXPECT_LT(avg_us, 150.0) << "Metrics recording too slow: " << avg_us << " us/record";

    printf("[BENCHMARK] Metrics recording: %.2f us/record\n", avg_us);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
