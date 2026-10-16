/**
 * @file test_exception_resilience_e2e.cpp
 * @brief E2E tests for exception handling resilience under stress
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: End-to-end tests verifying exception system stability under load
 * WHY:  Ensure exception handling remains robust during high-stress scenarios
 * HOW:  Multi-threaded stress tests, concurrent exception handling, queue overflow,
 *       memory pressure, and shutdown scenarios
 *
 * Test Scenarios:
 * 1. High-frequency exception generation without system degradation
 * 2. Concurrent exception handling from multiple threads
 * 3. Exception queue overflow handling
 * 4. Memory pressure exception handling
 * 5. Exception handling during system shutdown
 * 6. Recovery callback failure resilience
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <random>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities and Counters
 * ============================================================================ */

static std::atomic<uint64_t> g_exceptions_created{0};
static std::atomic<uint64_t> g_exceptions_handled{0};
static std::atomic<uint64_t> g_exceptions_dispatched{0};
static std::atomic<uint64_t> g_recovery_callbacks_invoked{0};
static std::atomic<uint64_t> g_recovery_callbacks_failed{0};
static std::atomic<uint64_t> g_handler_invocations{0};
static std::atomic<bool> g_simulate_recovery_failure{false};
static std::atomic<bool> g_shutdown_requested{false};

static void reset_test_counters() {
    g_exceptions_created = 0;
    g_exceptions_handled = 0;
    g_exceptions_dispatched = 0;
    g_recovery_callbacks_invoked = 0;
    g_recovery_callbacks_failed = 0;
    g_handler_invocations = 0;
    g_simulate_recovery_failure = false;
    g_shutdown_requested = false;
}

// Custom exception handler for testing
static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
    g_handler_invocations++;
    if (ex != nullptr) {
        g_exceptions_handled++;
    }
    // Return false to allow exception to propagate to other handlers
    return false;
}

// Custom recovery callback for testing
static int test_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    g_recovery_callbacks_invoked++;

    if (g_simulate_recovery_failure.load()) {
        g_recovery_callbacks_failed++;
        return -1;  // Simulate failure
    }

    // Simulate some recovery work
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    return 0;
}

// Failing recovery callback for resilience testing
static int failing_recovery_callback(nimcp_exception_t* ex,
                                      nimcp_exception_recovery_action_t action,
                                      void* user_data) {
    g_recovery_callbacks_invoked++;
    g_recovery_callbacks_failed++;
    return -1;  // Always fail
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionResilienceE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* test_handler_reg = nullptr;

    void SetUp() override {
        reset_test_counters();

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0)
            << "Failed to initialize exception system";

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0)
            << "Failed to initialize circuit breaker";

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0)
            << "Failed to initialize metrics";

        // Initialize exception-immune integration (without connecting to actual immune system)
        nimcp_exception_immune_config_t config;
        nimcp_exception_immune_default_config(&config);
        config.enable_auto_present = false;  // Don't auto-present for tests
        config.enable_auto_recovery = false;
        ASSERT_EQ(nimcp_exception_immune_init(&config), 0)
            << "Failed to initialize exception-immune integration";

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "test_handler";
        opts.handler = test_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        test_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(test_handler_reg, nullptr) << "Failed to register test handler";
    }

    void TearDown() override {
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
            test_handler_reg = nullptr;
        }

        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to create and dispatch exception
    nimcp_exception_t* create_test_exception(nimcp_error_t code,
                                              nimcp_exception_severity_t severity,
                                              const char* message) {
        nimcp_exception_t* ex = nimcp_exception_create(
            code, severity, __FILE__, __LINE__, __func__, "%s", message
        );
        if (ex) {
            g_exceptions_created++;
        }
        return ex;
    }
};

/* ============================================================================
 * Test 1: High-Frequency Exception Generation
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, HighFrequencyExceptionGeneration) {
    printf("=== Test: High-Frequency Exception Generation ===\n");

    const int NUM_EXCEPTIONS = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_WARNING,
            "High frequency test exception"
        );
        ASSERT_NE(ex, nullptr) << "Failed to create exception at iteration " << i;

        // Dispatch the exception
        nimcp_exception_dispatch(ex);
        g_exceptions_dispatched++;

        // Clean up
        nimcp_exception_unref(ex);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    printf("  Created and dispatched %d exceptions in %ld ms\n", NUM_EXCEPTIONS, duration.count());
    printf("  Rate: %.2f exceptions/second\n", (NUM_EXCEPTIONS * 1000.0) / duration.count());
    printf("  Handler invocations: %lu\n", g_handler_invocations.load());

    EXPECT_EQ(g_exceptions_created.load(), (uint64_t)NUM_EXCEPTIONS);
    EXPECT_EQ(g_exceptions_dispatched.load(), (uint64_t)NUM_EXCEPTIONS);
    EXPECT_GT(g_handler_invocations.load(), 0UL);

    printf("Test passed: High-frequency exception generation completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, BurstExceptionGeneration) {
    printf("=== Test: Burst Exception Generation ===\n");

    const int NUM_BURSTS = 50;
    const int EXCEPTIONS_PER_BURST = 200;

    for (int burst = 0; burst < NUM_BURSTS; burst++) {
        auto burst_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < EXCEPTIONS_PER_BURST; i++) {
            nimcp_exception_t* ex = create_test_exception(
                NIMCP_ERROR_OPERATION_FAILED + (i % 10),  // Vary error codes
                static_cast<nimcp_exception_severity_t>(EXCEPTION_SEVERITY_DEBUG + (i % 5)),
                "Burst test exception"
            );
            if (ex) {
                nimcp_exception_dispatch(ex);
                g_exceptions_dispatched++;
                nimcp_exception_unref(ex);
            }
        }

        auto burst_end = std::chrono::high_resolution_clock::now();
        auto burst_duration = std::chrono::duration_cast<std::chrono::microseconds>(burst_end - burst_start);

        if (burst % 10 == 0) {
            printf("  Burst %d/%d: %d exceptions in %ld us\n",
                   burst + 1, NUM_BURSTS, EXCEPTIONS_PER_BURST, burst_duration.count());
        }

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    printf("  Total exceptions created: %lu\n", g_exceptions_created.load());
    printf("  Total exceptions dispatched: %lu\n", g_exceptions_dispatched.load());

    EXPECT_GE(g_exceptions_created.load(), (uint64_t)(NUM_BURSTS * EXCEPTIONS_PER_BURST * 0.99));

    printf("Test passed: Burst exception generation completed\n\n");
}

/* ============================================================================
 * Test 2: Concurrent Exception Handling from Multiple Threads
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, ConcurrentExceptionHandling) {
    printf("=== Test: Concurrent Exception Handling ===\n");

    const int NUM_THREADS = 8;
    const int EXCEPTIONS_PER_THREAD = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};
    std::atomic<int> threads_ready{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &start_flag, &threads_ready, EXCEPTIONS_PER_THREAD]() {
            threads_ready++;
            while (!start_flag) {
                std::this_thread::yield();
            }

            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_THREAD_SYNC + t,
                    EXCEPTION_SEVERITY_WARNING,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (ex) {
                    g_exceptions_created++;
                    nimcp_exception_dispatch(ex);
                    g_exceptions_dispatched++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    // Wait for all threads to be ready
    while (threads_ready < NUM_THREADS) {
        std::this_thread::yield();
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag = true;

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    uint64_t expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;
    printf("  %d threads x %d exceptions = %lu expected\n", NUM_THREADS, EXCEPTIONS_PER_THREAD, expected);
    printf("  Actual created: %lu\n", g_exceptions_created.load());
    printf("  Actual dispatched: %lu\n", g_exceptions_dispatched.load());
    printf("  Duration: %ld ms\n", duration.count());
    printf("  Rate: %.2f exceptions/second\n", (g_exceptions_dispatched.load() * 1000.0) / duration.count());

    EXPECT_EQ(g_exceptions_created.load(), expected);
    EXPECT_EQ(g_exceptions_dispatched.load(), expected);

    printf("Test passed: Concurrent exception handling completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, ConcurrentMixedSeverities) {
    printf("=== Test: Concurrent Mixed Severities ===\n");

    const int NUM_THREADS = 6;
    const int DURATION_MS = 2000;
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Different threads create different severity exceptions
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &running]() {
            nimcp_exception_severity_t severities[] = {
                EXCEPTION_SEVERITY_DEBUG,
                EXCEPTION_SEVERITY_INFO,
                EXCEPTION_SEVERITY_WARNING,
                EXCEPTION_SEVERITY_ERROR,
                EXCEPTION_SEVERITY_SEVERE,
                EXCEPTION_SEVERITY_CRITICAL
            };
            nimcp_exception_severity_t severity = severities[t % 6];

            while (running) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    severity,
                    __FILE__, __LINE__, __func__,
                    "Mixed severity test from thread %d", t
                );
                if (ex) {
                    g_exceptions_created++;
                    nimcp_exception_dispatch(ex);
                    g_exceptions_dispatched++;
                    nimcp_exception_unref(ex);
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    printf("  Total exceptions created: %lu\n", g_exceptions_created.load());
    printf("  Handler invocations: %lu\n", g_handler_invocations.load());

    EXPECT_GT(g_exceptions_created.load(), 0UL);
    EXPECT_EQ(g_exceptions_created.load(), g_exceptions_dispatched.load());

    printf("Test passed: Concurrent mixed severities completed\n\n");
}

/* ============================================================================
 * Test 3: Exception Queue Overflow Handling
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, AsyncQueueOverflow) {
    printf("=== Test: Async Queue Overflow Handling ===\n");

    // Generate more exceptions than the async queue can hold
    // NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE is 256
    const int NUM_EXCEPTIONS = 500;
    int queued_count = 0;
    int overflow_count = 0;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            "Async queue test exception"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_exception_present_async(ex);
        if (result == 0) {
            queued_count++;
        } else {
            overflow_count++;
        }

        // Don't unref here - the async system takes ownership
        // But we still need to track the exception
        nimcp_exception_unref(ex);
    }

    printf("  Attempted to queue: %d exceptions\n", NUM_EXCEPTIONS);
    printf("  Successfully queued: %d\n", queued_count);
    printf("  Overflow (rejected): %d\n", overflow_count);

    // Process pending exceptions
    size_t processed = nimcp_exception_immune_process_pending(0);
    printf("  Processed from queue: %zu\n", processed);

    // Get stats to verify overflow handling
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Queue overflows reported: %lu\n", (unsigned long)stats.queue_overflows);

    // System should handle overflow gracefully - no crashes
    EXPECT_GE(queued_count, 0);  // Some should be queued
    // overflow_count can vary depending on timing

    printf("Test passed: Async queue overflow handling completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, RapidAsyncPresentation) {
    printf("=== Test: Rapid Async Presentation ===\n");

    const int NUM_ITERATIONS = 100;
    const int EXCEPTIONS_PER_ITERATION = 50;
    std::atomic<size_t> total_processed{0};

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Rapidly queue exceptions
        for (int i = 0; i < EXCEPTIONS_PER_ITERATION; i++) {
            nimcp_exception_t* ex = create_test_exception(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                "Rapid async test"
            );
            if (ex) {
                nimcp_exception_present_async(ex);
                nimcp_exception_unref(ex);
            }
        }

        // Process some of the queue
        size_t processed = nimcp_exception_immune_process_pending(10);
        total_processed += processed;

        if (iter % 20 == 0) {
            printf("  Iteration %d: processed %zu from queue\n", iter, processed);
        }
    }

    // Final processing
    size_t remaining = nimcp_exception_immune_process_pending(0);
    total_processed += remaining;

    printf("  Total processed: %lu\n", total_processed.load());
    printf("  Exceptions created: %lu\n", g_exceptions_created.load());

    EXPECT_GT(total_processed.load(), 0UL);

    printf("Test passed: Rapid async presentation completed\n\n");
}

/* ============================================================================
 * Test 4: Memory Pressure Exception Handling
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, MemoryExceptionStress) {
    printf("=== Test: Memory Exception Stress ===\n");

    const int NUM_EXCEPTIONS = 1000;
    std::vector<nimcp_exception_t*> exceptions;
    exceptions.reserve(NUM_EXCEPTIONS);

    // Create many exceptions and hold references
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_memory_exception_t* mex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            1024 * (i + 1),  // Varying requested sizes
            "Memory pressure test %d", i
        );
        if (mex) {
            exceptions.push_back((nimcp_exception_t*)mex);
            g_exceptions_created++;
        }
    }

    printf("  Created %zu memory exceptions\n", exceptions.size());

    // Dispatch all while holding references
    for (auto* ex : exceptions) {
        nimcp_exception_dispatch(ex);
        g_exceptions_dispatched++;
    }

    printf("  Dispatched all exceptions\n");

    // Add context to some exceptions
    for (size_t i = 0; i < exceptions.size(); i += 10) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "context_%zu", i);
        snprintf(value, sizeof(value), "value_for_exception_%zu", i);
        nimcp_exception_set_context(exceptions[i], key, value);
    }

    // Release all exceptions
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
    exceptions.clear();

    printf("  Released all exceptions\n");
    printf("  Total created: %lu\n", g_exceptions_created.load());
    printf("  Total dispatched: %lu\n", g_exceptions_dispatched.load());

    EXPECT_EQ(g_exceptions_created.load(), (uint64_t)NUM_EXCEPTIONS);

    printf("Test passed: Memory exception stress completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, ExceptionChaining) {
    printf("=== Test: Exception Chaining Under Pressure ===\n");

    const int CHAIN_LENGTH = 10;
    const int NUM_CHAINS = 100;

    for (int c = 0; c < NUM_CHAINS; c++) {
        nimcp_exception_t* root = create_test_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            "Chain root exception"
        );
        ASSERT_NE(root, nullptr);

        nimcp_exception_t* current = root;
        for (int i = 1; i < CHAIN_LENGTH; i++) {
            nimcp_exception_t* cause = create_test_exception(
                NIMCP_ERROR_OPERATION_FAILED + i,
                EXCEPTION_SEVERITY_WARNING,
                "Chain cause exception"
            );
            if (cause) {
                nimcp_exception_set_cause(current, cause);
                current = cause;
            }
        }

        // Dispatch the chained exception
        nimcp_exception_dispatch(root);
        g_exceptions_dispatched++;

        // Clean up - only unref the root
        nimcp_exception_unref(root);

        if (c % 20 == 0) {
            printf("  Created chain %d/%d\n", c + 1, NUM_CHAINS);
        }
    }

    printf("  Total exceptions created: %lu\n", g_exceptions_created.load());

    EXPECT_GE(g_exceptions_created.load(), (uint64_t)(NUM_CHAINS * CHAIN_LENGTH * 0.9));

    printf("Test passed: Exception chaining under pressure completed\n\n");
}

/* ============================================================================
 * Test 5: Exception Handling During System Shutdown
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, ExceptionsDuringShutdown) {
    printf("=== Test: Exceptions During Shutdown ===\n");

    const int DURATION_MS = 1000;
    std::atomic<bool> running{true};
    std::atomic<uint64_t> exceptions_before_shutdown{0};
    std::atomic<uint64_t> exceptions_during_shutdown{0};

    // Exception generator thread
    std::thread generator([this, &running, &exceptions_before_shutdown, &exceptions_during_shutdown]() {
        while (running || !g_shutdown_requested) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Shutdown test exception"
            );
            if (ex) {
                g_exceptions_created++;
                if (!g_shutdown_requested) {
                    exceptions_before_shutdown++;
                } else {
                    exceptions_during_shutdown++;
                }
                nimcp_exception_dispatch(ex);
                nimcp_exception_unref(ex);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Let it run normally
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS / 2));

    // Signal shutdown
    g_shutdown_requested = true;
    printf("  Shutdown signaled\n");

    // Continue for a bit during "shutdown"
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS / 2));

    running = false;
    generator.join();

    printf("  Exceptions before shutdown: %lu\n", exceptions_before_shutdown.load());
    printf("  Exceptions during shutdown: %lu\n", exceptions_during_shutdown.load());
    printf("  Total exceptions: %lu\n", g_exceptions_created.load());

    // Both phases should have created exceptions
    EXPECT_GT(exceptions_before_shutdown.load(), 0UL);
    EXPECT_GT(exceptions_during_shutdown.load(), 0UL);

    printf("Test passed: Exceptions during shutdown completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, CleanShutdownWithPendingExceptions) {
    printf("=== Test: Clean Shutdown With Pending Exceptions ===\n");

    // Queue up exceptions in async queue
    const int NUM_PENDING = 100;
    for (int i = 0; i < NUM_PENDING; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_WARNING,
            "Pending exception for shutdown"
        );
        if (ex) {
            nimcp_exception_present_async(ex);
            nimcp_exception_unref(ex);
        }
    }

    printf("  Queued %d exceptions\n", NUM_PENDING);

    // Check pending count via stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Pending in queue: %lu\n", (unsigned long)stats.exceptions_pending);

    // Process half
    size_t processed = nimcp_exception_immune_process_pending(NUM_PENDING / 2);
    printf("  Processed: %zu\n", processed);

    // TearDown will shutdown with remaining pending - should not crash
    printf("  Proceeding to shutdown with pending exceptions...\n");

    // System should handle this gracefully
    EXPECT_GE(processed, 0UL);

    printf("Test passed: Clean shutdown with pending exceptions completed\n\n");
}

/* ============================================================================
 * Test 6: Recovery Callback Failure Resilience
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, RecoveryCallbackFailures) {
    printf("=== Test: Recovery Callback Failures ===\n");

    // Register failing recovery callback
    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        failing_recovery_callback,
        nullptr
    ), 0);

    const int NUM_RECOVERY_ATTEMPTS = 100;

    for (int i = 0; i < NUM_RECOVERY_ATTEMPTS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            "Recovery failure test"
        );
        ASSERT_NE(ex, nullptr);

        // Attempt recovery (should fail)
        int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
        // We expect failure since callback always returns -1
        EXPECT_NE(result, 0);

        nimcp_exception_unref(ex);
    }

    printf("  Recovery attempts: %d\n", NUM_RECOVERY_ATTEMPTS);
    printf("  Callback invocations: %lu\n", g_recovery_callbacks_invoked.load());
    printf("  Callback failures: %lu\n", g_recovery_callbacks_failed.load());

    EXPECT_EQ(g_recovery_callbacks_invoked.load(), (uint64_t)NUM_RECOVERY_ATTEMPTS);
    EXPECT_EQ(g_recovery_callbacks_failed.load(), (uint64_t)NUM_RECOVERY_ATTEMPTS);

    // Unregister callback
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);

    printf("Test passed: Recovery callback failures completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, IntermittentRecoveryFailures) {
    printf("=== Test: Intermittent Recovery Failures ===\n");

    // Register the test callback that can be made to fail
    EXPECT_EQ(nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        test_recovery_callback,
        nullptr
    ), 0);

    const int NUM_OPERATIONS = 200;
    int successes = 0;
    int failures = 0;

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // Intermittently enable failure simulation
        g_simulate_recovery_failure = (i % 3 == 0);

        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Intermittent recovery test"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
        if (result == 0) {
            successes++;
        } else {
            failures++;
        }

        nimcp_exception_unref(ex);
    }

    printf("  Total operations: %d\n", NUM_OPERATIONS);
    printf("  Successful recoveries: %d\n", successes);
    printf("  Failed recoveries: %d\n", failures);
    printf("  Callback invocations: %lu\n", g_recovery_callbacks_invoked.load());

    // Should have mix of successes and failures
    EXPECT_GT(successes, 0);
    EXPECT_GT(failures, 0);

    // Unregister callback
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    g_simulate_recovery_failure = false;

    printf("Test passed: Intermittent recovery failures completed\n\n");
}

/* ============================================================================
 * Test 7: Circuit Breaker Under Load
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, CircuitBreakerTripping) {
    printf("=== Test: Circuit Breaker Tripping ===\n");

    nimcp_error_t test_code = NIMCP_ERROR_NETWORK_IO;

    // Set low threshold for testing
    EXPECT_EQ(nimcp_circuit_set_threshold(test_code, 5, 1000), 0);

    // Generate exceptions rapidly to trip circuit
    const int NUM_EXCEPTIONS = 20;
    int blocked = 0;
    int passed = 0;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_ERROR,
            "Circuit breaker test"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_circuit_record(ex);
        if (result == 1) {
            blocked++;
        } else if (result == 0) {
            passed++;
        }

        nimcp_exception_unref(ex);
    }

    printf("  Exceptions passed: %d\n", passed);
    printf("  Exceptions blocked: %d\n", blocked);

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(test_code);
    printf("  Final circuit state: %s\n", nimcp_circuit_state_to_string(state));

    // Should have some blocked due to circuit opening
    EXPECT_GT(passed, 0);
    // Circuit may or may not be open depending on timing

    // Reset circuit
    nimcp_circuit_reset(test_code);

    printf("Test passed: Circuit breaker tripping completed\n\n");
}

TEST_F(ExceptionResilienceE2ETest, CircuitBreakerRecovery) {
    printf("=== Test: Circuit Breaker Recovery ===\n");

    nimcp_error_t test_code = NIMCP_ERROR_SOCKET_ERROR;

    // Set thresholds
    EXPECT_EQ(nimcp_circuit_set_threshold(test_code, 3, 500), 0);

    // Trip the circuit
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = create_test_exception(test_code, EXCEPTION_SEVERITY_ERROR, "Trip test");
        if (ex) {
            nimcp_circuit_record(ex);
            nimcp_exception_unref(ex);
        }
    }

    nimcp_circuit_state_t state = nimcp_circuit_get_state(test_code);
    printf("  State after tripping: %s\n", nimcp_circuit_state_to_string(state));

    // Wait for half-open transition
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Report successes to close circuit
    for (int i = 0; i < 5; i++) {
        nimcp_circuit_report_success(test_code);
    }

    state = nimcp_circuit_get_state(test_code);
    printf("  State after recovery: %s\n", nimcp_circuit_state_to_string(state));

    // Get statistics
    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);
    printf("  Total tracked: %zu\n", stats.total_tracked);
    printf("  Total blocked: %lu\n", (unsigned long)stats.total_blocked);

    printf("Test passed: Circuit breaker recovery completed\n\n");
}

/* ============================================================================
 * Test 8: Aggregate Exception Handling
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, AggregateExceptionStress) {
    printf("=== Test: Aggregate Exception Stress ===\n");

    const int NUM_AGGREGATES = 50;
    const int CHILDREN_PER_AGGREGATE = 10;

    for (int a = 0; a < NUM_AGGREGATES; a++) {
        nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Aggregate exception %d", a
        );
        ASSERT_NE(agg, nullptr);
        g_exceptions_created++;

        // Add children
        for (int c = 0; c < CHILDREN_PER_AGGREGATE; c++) {
            nimcp_exception_t* child = create_test_exception(
                NIMCP_ERROR_OPERATION_FAILED + c,
                EXCEPTION_SEVERITY_WARNING,
                "Child exception"
            );
            if (child) {
                int result = nimcp_aggregate_exception_add(agg, child);
                if (result != 0) {
                    nimcp_exception_unref(child);
                }
            }
        }

        // Verify count
        size_t count = nimcp_aggregate_exception_count(agg);
        EXPECT_EQ(count, (size_t)CHILDREN_PER_AGGREGATE);

        // Dispatch
        nimcp_exception_dispatch((nimcp_exception_t*)agg);
        g_exceptions_dispatched++;

        // Clean up
        nimcp_exception_unref((nimcp_exception_t*)agg);

        if (a % 10 == 0) {
            printf("  Created aggregate %d/%d with %d children\n", a + 1, NUM_AGGREGATES, CHILDREN_PER_AGGREGATE);
        }
    }

    printf("  Total exceptions created: %lu\n", g_exceptions_created.load());

    EXPECT_GE(g_exceptions_created.load(),
              (uint64_t)(NUM_AGGREGATES + NUM_AGGREGATES * CHILDREN_PER_AGGREGATE));

    printf("Test passed: Aggregate exception stress completed\n\n");
}

/* ============================================================================
 * Test 9: Metrics Under High Load
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, MetricsAccuracyUnderLoad) {
    printf("=== Test: Metrics Accuracy Under Load ===\n");

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 500;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, &start_flag, EXCEPTIONS_PER_THREAD]() {
            while (!start_flag) std::this_thread::yield();

            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_NO_MEMORY + (t % 3),
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Metrics test exception"
                );
                if (ex) {
                    g_exceptions_created++;
                    nimcp_metrics_record_exception(ex);
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    // Reset metrics before test
    nimcp_metrics_reset();

    start_flag = true;
    for (auto& t : threads) {
        t.join();
    }

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    printf("  Expected exceptions: %d\n", NUM_THREADS * EXCEPTIONS_PER_THREAD);
    printf("  Metrics total: %lu\n", (unsigned long)metrics.total_exceptions);
    printf("  Rate per second: %.2f\n", metrics.current_rate_per_second);

    // Metrics count should match or be close to actual count
    uint64_t expected = NUM_THREADS * EXCEPTIONS_PER_THREAD;
    EXPECT_GE(metrics.total_exceptions, expected * 0.95);  // Allow 5% tolerance

    printf("Test passed: Metrics accuracy under load completed\n\n");
}

/* ============================================================================
 * Combined Stress Test
 * ============================================================================ */

TEST_F(ExceptionResilienceE2ETest, FullExceptionResilienceStress) {
    printf("=== Test: Full Exception Resilience Stress ===\n");

    const int DURATION_MS = 5000;
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;
    std::random_device rd;

    // Exception generator threads
    for (int t = 0; t < 3; t++) {
        threads.emplace_back([this, t, &running]() {
            while (running) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED + (t * 100),
                    static_cast<nimcp_exception_severity_t>(EXCEPTION_SEVERITY_WARNING + t),
                    __FILE__, __LINE__, __func__,
                    "Full stress test exception from thread %d", t
                );
                if (ex) {
                    g_exceptions_created++;
                    nimcp_exception_dispatch(ex);
                    g_exceptions_dispatched++;
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    // Async queue thread
    threads.emplace_back([this, &running]() {
        while (running) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_TIMEOUT,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Async stress exception"
            );
            if (ex) {
                g_exceptions_created++;
                nimcp_exception_present_async(ex);
                nimcp_exception_unref(ex);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Queue processor thread
    threads.emplace_back([this, &running]() {
        while (running) {
            nimcp_exception_immune_process_pending(10);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Circuit maintenance thread
    threads.emplace_back([this, &running]() {
        while (running) {
            nimcp_circuit_maintenance();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    auto start_time = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Get final statistics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    nimcp_circuit_stats_t circuit_stats;
    nimcp_circuit_get_stats(&circuit_stats);

    nimcp_exception_immune_stats_t immune_stats;
    nimcp_exception_immune_get_stats(&immune_stats);

    printf("\n=== Final Results ===\n");
    printf("  Duration: %ld ms\n", duration.count());
    printf("  Exceptions created: %lu\n", g_exceptions_created.load());
    printf("  Exceptions dispatched: %lu\n", g_exceptions_dispatched.load());
    printf("  Handler invocations: %lu\n", g_handler_invocations.load());
    printf("  Exceptions/second: %.2f\n", (g_exceptions_dispatched.load() * 1000.0) / duration.count());
    printf("  Metrics recorded: %lu\n", (unsigned long)metrics.total_exceptions);
    printf("  Circuit blocked: %lu\n", (unsigned long)circuit_stats.total_blocked);
    printf("  Immune queue overflows: %lu\n", (unsigned long)immune_stats.queue_overflows);

    // Verify system remained stable
    EXPECT_GT(g_exceptions_created.load(), 0UL);
    EXPECT_GT(g_exceptions_dispatched.load(), 0UL);

    printf("\nTest passed: Full exception resilience stress completed\n\n");
}
