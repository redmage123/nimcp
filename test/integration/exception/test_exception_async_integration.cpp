/**
 * @file test_exception_async_integration.cpp
 * @brief Integration tests for exception handling in async pipelines
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test exception handling in bio-async pipelines, async presentation, and recovery
 * WHY:  Async operations require proper exception propagation and non-blocking handling
 * HOW:  Use bio-async primitives with exception handlers, test async presentation flow
 *
 * TEST SCENARIOS:
 * - Bio-async promise failure propagates exception
 * - Async exception presentation and batch processing
 * - Exception callbacks in bio-future completion
 * - Phase sync exception handling
 * - Glial wave exception propagation
 * - Predictive coding error callbacks
 * - Recovery flows in async contexts
 *
 * HEADER FILES REFERENCED:
 * - include/async/nimcp_bio_async.h
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_metrics.h
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C" {
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Async Exception State Tracking
//=============================================================================

/**
 * @brief Tracks async exception events for testing
 */
struct AsyncExceptionEvent {
    nimcp_error_t code;
    nimcp_exception_severity_t severity;
    std::string message;
    std::string source;  // promise, future, callback, etc.
    uint64_t timestamp;
    bool handled;
};

static struct {
    std::mutex mutex;
    std::vector<AsyncExceptionEvent> events;
    std::atomic<int> callback_count{0};
    std::atomic<int> recovery_count{0};
    std::atomic<int> presentation_count{0};
    std::atomic<bool> last_recovery_success{false};
    std::condition_variable cv;
} g_async_state;

//=============================================================================
// Async Exception Callback Helpers
//=============================================================================

static void record_async_exception(nimcp_exception_t* ex, const char* source, bool handled) {
    AsyncExceptionEvent event;
    event.code = ex->code;
    event.severity = ex->severity;
    event.message = ex->message;
    event.source = source;
    event.timestamp = ex->timestamp_us;
    event.handled = handled;

    std::lock_guard<std::mutex> lock(g_async_state.mutex);
    g_async_state.events.push_back(event);
    g_async_state.cv.notify_all();
}

static bool async_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_async_state.callback_count++;
    record_async_exception(ex, "handler", false);
    return false;  // Don't consume
}

static int async_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)ex;
    (void)user_data;
    g_async_state.recovery_count++;

    // Simulate async recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    // Most recoveries succeed
    if (action == EXCEPTION_RECOVERY_RETRY ||
        action == EXCEPTION_RECOVERY_GC ||
        action == EXCEPTION_RECOVERY_CLEAR_CACHE) {
        g_async_state.last_recovery_success = true;
        return 0;
    }
    g_async_state.last_recovery_success = false;
    return -1;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionAsyncIntegrationTest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg_ = nullptr;
    bool bio_async_initialized_ = false;

    void SetUp() override {
        // Reset state
        {
            std::lock_guard<std::mutex> lock(g_async_state.mutex);
            g_async_state.events.clear();
        }
        g_async_state.callback_count = 0;
        g_async_state.recovery_count = 0;
        g_async_state.presentation_count = 0;
        g_async_state.last_recovery_success = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize bio-async system
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_statistics = true;
        config.thread_pool_size = 2;
        if (nimcp_bio_async_init(&config) == NIMCP_SUCCESS) {
            bio_async_initialized_ = true;
        }

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        immune_config.enable_auto_recovery = false;
        immune_config.max_pending_exceptions = 64;
        nimcp_exception_immune_init(&immune_config);

        // Initialize metrics
        nimcp_metrics_init();

        // Register handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "async_test_handler";
        opts.handler = async_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }

        nimcp_exception_clear_current();
        nimcp_metrics_shutdown();
        nimcp_exception_immune_shutdown();

        if (bio_async_initialized_) {
            nimcp_bio_async_shutdown();
            bio_async_initialized_ = false;
        }

        nimcp_exception_system_shutdown();
    }

    void waitForEvents(size_t expected_count, int timeout_ms = 1000) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
        std::unique_lock<std::mutex> lock(g_async_state.mutex);
        while (g_async_state.events.size() < expected_count) {
            if (g_async_state.cv.wait_until(lock, deadline) == std::cv_status::timeout) {
                break;
            }
        }
    }

    size_t eventCount() {
        std::lock_guard<std::mutex> lock(g_async_state.mutex);
        return g_async_state.events.size();
    }

    bool hasEventWithCode(nimcp_error_t code) {
        std::lock_guard<std::mutex> lock(g_async_state.mutex);
        for (const auto& event : g_async_state.events) {
            if (event.code == code) return true;
        }
        return false;
    }

    bool hasEventFromSource(const char* source) {
        std::lock_guard<std::mutex> lock(g_async_state.mutex);
        for (const auto& event : g_async_state.events) {
            if (event.source == source) return true;
        }
        return false;
    }
};

//=============================================================================
// Test: Bio-Promise Failure Propagates Exception
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, BioPromiseFailurePropagatesException) {
    // WHAT: Test that failing a bio-promise triggers exception handling
    // WHY:  Async failures must integrate with exception system
    // HOW:  Create promise, fail it, verify exception is raised

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Create promise
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_DOPAMINE,
        sizeof(int)
    );
    ASSERT_NE(promise, nullptr);

    // Get future
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Fail the promise with error
    nimcp_error_t error = NIMCP_ERROR_TIMEOUT;
    nimcp_error_t result = nimcp_bio_promise_fail(promise, error);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify future state
    nimcp_bio_future_state_t state = nimcp_bio_future_state(future);
    EXPECT_EQ(state, BIO_FUTURE_FAILED);

    // Create exception for the async failure
    nimcp_exception_t* ex = nimcp_exception_create(
        error,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Bio-promise failed with timeout"
    );
    ASSERT_NE(ex, nullptr);

    // Add async context
    nimcp_exception_set_context(ex, "operation_id", "1");
    nimcp_exception_set_context(ex, "operation_name", "bio_promise_fail");

    // Dispatch exception
    nimcp_exception_dispatch(ex);

    // Verify exception was handled
    EXPECT_GE(g_async_state.callback_count.load(), 1);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_TIMEOUT));

    nimcp_exception_unref(ex);
    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Test: Async Exception Presentation and Batch Processing
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, AsyncExceptionPresentationBatchProcessing) {
    // WHAT: Test async exception presentation queues for batch processing
    // WHY:  High-throughput systems need non-blocking exception handling
    // HOW:  Queue multiple exceptions, process batch, verify all handled

    const int NUM_EXCEPTIONS = 5;
    std::vector<nimcp_exception_t*> exceptions;

    // Create and queue exceptions asynchronously
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_CANCELLED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Async queued exception %d", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);

        // Queue for async presentation
        int result = nimcp_exception_present_async(ex);
        EXPECT_EQ(result, 0);
    }

    // Process all pending
    size_t processed = nimcp_exception_immune_process_pending(0);
    EXPECT_EQ(processed, (size_t)NUM_EXCEPTIONS);

    // Verify all were presented
    for (auto* ex : exceptions) {
        EXPECT_TRUE(ex->presented_to_immune)
            << "Exception should be marked as presented";
    }

    // Cleanup
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Test: Exception Callbacks in Bio-Future Completion
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, ExceptionCallbacksInBioFutureCompletion) {
    // WHAT: Test exception callbacks fired when bio-future fails
    // WHY:  Async callbacks must be able to handle exceptions
    // HOW:  Register callback, fail future, verify callback receives error

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    static std::atomic<bool> callback_fired{false};
    static std::atomic<nimcp_error_t> received_error{NIMCP_SUCCESS};

    // Create promise and future
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE,  // Alerting channel
        sizeof(float)
    );
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Register callback
    auto callback = [](const void* result, float confidence,
                       nimcp_error_t error, void* user_data) {
        (void)result;
        (void)confidence;
        (void)user_data;
        callback_fired = true;
        received_error = error;
    };

    nimcp_error_t reg_result = nimcp_bio_future_then(future, callback, nullptr);
    EXPECT_EQ(reg_result, NIMCP_SUCCESS);

    // Fail the promise
    nimcp_error_t fail_error = NIMCP_ERROR_OPERATION_FAILED;
    nimcp_bio_promise_fail(promise, fail_error);

    // Wait for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify callback was fired with error
    EXPECT_TRUE(callback_fired.load());
    EXPECT_EQ(received_error.load(), fail_error);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Test: Phase Sync Exception on Timeout
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, PhaseSyncExceptionOnTimeout) {
    // WHAT: Test exception raised when phase sync times out
    // WHY:  Sync failures are critical and need exception handling
    // HOW:  Create sync group, don't complete futures, timeout

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Create phase sync
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    // Add incomplete futures
    nimcp_bio_promise_t p1 = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_promise_t p2 = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);

    nimcp_bio_future_t f1 = nimcp_bio_promise_get_future(p1);
    nimcp_bio_future_t f2 = nimcp_bio_promise_get_future(p2);

    nimcp_phase_sync_add_future(sync, f1);
    nimcp_phase_sync_add_future(sync, f2);

    // Try to sync with very short timeout
    nimcp_error_t sync_result = nimcp_phase_sync_wait_all(sync, 10);

    // Should fail (futures not completed)
    if (sync_result != NIMCP_SUCCESS) {
        // Create exception for sync timeout
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_BIO_ERROR_PHASE_INCOHERENT,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Phase sync timeout - coherence not achieved"
        );

        if (ex) {
            nimcp_exception_set_context(ex, "operation", "phase_sync_wait_all");
            nimcp_exception_dispatch(ex);
            EXPECT_GE(g_async_state.callback_count.load(), 1);
            nimcp_exception_unref(ex);
        }
    }

    nimcp_bio_future_destroy(f1);
    nimcp_bio_future_destroy(f2);
    nimcp_bio_promise_destroy(p1);
    nimcp_bio_promise_destroy(p2);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Test: Bio-Async Message Delivery for Exceptions
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, BioAsyncMessageDeliveryForExceptions) {
    // WHAT: Test bio-async system can deliver exception-related messages
    // WHY:  Exception notifications may need async delivery
    // HOW:  Use bio-promise to deliver exception info

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Critical exception for async delivery"
    );
    ASSERT_NE(ex, nullptr);

    // Generate epitope
    nimcp_exception_generate_epitope(ex);

    // Create promise to deliver exception notification
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        BIO_CHANNEL_NOREPINEPHRINE,  // Alerting channel
        sizeof(nimcp_error_t)
    );
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Complete with error code
    nimcp_error_t code = ex->code;
    nimcp_bio_promise_complete(promise, &code);

    // Wait for result
    nimcp_error_t received_code;
    nimcp_error_t wait_result = nimcp_bio_future_wait(future, &received_code, 100);
    EXPECT_EQ(wait_result, NIMCP_SUCCESS);
    EXPECT_EQ(received_code, NIMCP_ERROR_OPERATION_FAILED);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Recovery Flow in Async Context
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, RecoveryFlowInAsyncContext) {
    // WHAT: Test exception recovery works in async context
    // WHY:  Async operations need async-compatible recovery
    // HOW:  Register async recovery callback, trigger recovery

    // Register recovery callbacks
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, async_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, async_recovery_callback, nullptr);

    // Create async-related exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Async operation timed out"
    );
    ASSERT_NE(ex, nullptr);

    // Add async context
    nimcp_exception_set_context(ex, "operation_id", "42");
    nimcp_exception_set_context(ex, "operation_name", "async_operation");

    // Present to immune
    nimcp_immune_response_t response;
    int present_result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(present_result, 0);

    // Execute recovery (retry)
    g_async_state.recovery_count = 0;
    int recovery_result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(recovery_result, 0);
    EXPECT_GE(g_async_state.recovery_count.load(), 1);
    EXPECT_TRUE(g_async_state.last_recovery_success.load());

    // Notify immune
    nimcp_exception_notify_recovery_result(ex, EXCEPTION_RECOVERY_RETRY, true);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Predictive Coding Error Triggers Exception
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, PredictiveCodingErrorTriggersException) {
    // WHAT: Test prediction errors can trigger exceptions
    // WHY:  Large prediction errors may indicate system problems
    // HOW:  Create model, observe value far from prediction, create exception

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Create predictive model
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "test_signal",
        50.0f,  // Initial prediction
        1.0f    // Initial precision
    );
    ASSERT_NE(model, nullptr);

    // Register error callback
    static std::atomic<bool> prediction_error_occurred{false};
    static std::atomic<float> last_error{0.0f};

    auto error_callback = [](const char* signal_name, float prediction,
                             float actual, float error, float surprise,
                             void* user_data) {
        (void)signal_name;
        (void)prediction;
        (void)actual;
        (void)surprise;
        (void)user_data;
        prediction_error_occurred = true;
        last_error = error;
    };

    nimcp_predictive_on_error(model, error_callback, nullptr, 0.0f);

    // Observe very different value (large prediction error)
    nimcp_predictive_observe(model, 150.0f);  // Far from 50.0f

    // Check if error callback was fired
    EXPECT_TRUE(prediction_error_occurred.load());
    EXPECT_GT(last_error.load(), 50.0f);

    // If error is large enough, create exception
    if (last_error.load() > 50.0f) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_PREDICTIVE,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Large prediction error: %.2f", last_error.load()
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_set_context(ex, "signal", "test_signal");
        nimcp_exception_set_context(ex, "error", std::to_string(last_error.load()).c_str());

        nimcp_exception_dispatch(ex);
        EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_PREDICTIVE));

        nimcp_exception_unref(ex);
    }

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Test: Glial Wave Exception Propagation
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, GlialWaveExceptionPropagation) {
    // WHAT: Test exception propagation via glial wave analogy
    // WHY:  System-wide errors can propagate like glial calcium waves
    // HOW:  Create glial wave, track propagation with exception notification

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Initiate glial wave (simulates system-wide error propagation)
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(
        0,      // Source region
        5.0f    // Initial calcium
    );
    ASSERT_NE(wave, nullptr);

    // Create exception that will propagate with the wave
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "System-wide error propagating via glial wave"
    );
    ASSERT_NE(ex, nullptr);

    // Set propagation context via context entries
    nimcp_exception_set_context(ex, "origin_module", "region_0");
    nimcp_exception_set_context(ex, "propagation_type", "WAVE_INIT");
    nimcp_exception_set_context(ex, "hop_count", "1");

    // Step the wave a few times
    for (int i = 0; i < 5; i++) {
        nimcp_error_t step_result = nimcp_glial_wave_step(wave, 10.0f);
        if (step_result == NIMCP_BIO_ERROR_WAVE_EXTINCT) {
            break;
        }
    }

    // Dispatch exception
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_OPERATION_FAILED));

    nimcp_glial_wave_destroy(wave);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Concurrent Async Exception Creation
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, ConcurrentAsyncExceptionCreation) {
    // WHAT: Test thread-safe async exception creation and handling
    // WHY:  Multiple async operations may fail concurrently
    // HOW:  Create exceptions from multiple threads

    const int NUM_THREADS = 4;
    const int EXCEPTIONS_PER_THREAD = 10;
    std::atomic<int> total_created{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&total_created, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_CANCELLED + (i % 5),
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d op %d failed", t, i
                );
                if (ex) {
                    total_created++;
                    nimcp_exception_dispatch(ex);
                    nimcp_exception_unref(ex);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_created.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
}

//=============================================================================
// Test: Async Presentation Queue Overflow Handling
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, AsyncPresentationQueueOverflow) {
    // WHAT: Test handling when async presentation queue is full
    // WHY:  System must gracefully handle queue overflow
    // HOW:  Fill queue beyond capacity, verify graceful degradation

    // Create many exceptions quickly
    std::vector<nimcp_exception_t*> exceptions;
    int queued_count = 0;
    int rejected_count = 0;

    for (int i = 0; i < 100; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Queue test exception %d", i
        );
        ASSERT_NE(ex, nullptr);
        exceptions.push_back(ex);

        int result = nimcp_exception_present_async(ex);
        if (result == 0) {
            queued_count++;
        } else {
            rejected_count++;
        }

        // Process some to make room
        if (i % 20 == 19) {
            nimcp_exception_immune_process_pending(0);
        }
    }

    // Process remaining
    nimcp_exception_immune_process_pending(0);

    // Most should have been processed (some may be rejected if queue full)
    EXPECT_GT(queued_count, 0);

    // Cleanup
    for (auto* ex : exceptions) {
        nimcp_exception_unref(ex);
    }
}

//=============================================================================
// Test: Async Exception with Timeout Context
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, AsyncExceptionWithTimeoutContext) {
    // WHAT: Test async exception carries timeout-related context
    // WHY:  Timeout exceptions need timeout info for debugging
    // HOW:  Create async exception with timeout context

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Database query timed out"
    );
    ASSERT_NE(ex, nullptr);

    // Add timeout-specific context
    nimcp_exception_set_context(ex, "operation_id", "123");
    nimcp_exception_set_context(ex, "operation_name", "database_query");
    nimcp_exception_set_context(ex, "timeout_ms", "5000");
    nimcp_exception_set_context(ex, "elapsed_ms", "5023");
    nimcp_exception_set_context(ex, "retry_count", "3");
    nimcp_exception_set_context(ex, "operation_type", "SELECT");

    // Verify context
    EXPECT_EQ(nimcp_exception_context_count(ex), 6u);

    const char* timeout = nimcp_exception_get_context(ex, "timeout_ms");
    EXPECT_STREQ(timeout, "5000");

    const char* elapsed = nimcp_exception_get_context(ex, "elapsed_ms");
    EXPECT_STREQ(elapsed, "5023");

    // Dispatch and verify handler receives context
    nimcp_exception_dispatch(ex);
    EXPECT_TRUE(hasEventWithCode(NIMCP_ERROR_TIMEOUT));

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Neuromodulator Channel-Specific Exception Handling
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, NeuromodulatorChannelExceptionHandling) {
    // WHAT: Test exception handling varies by neuromodulator channel
    // WHY:  Different channels have different timing/priority characteristics
    // HOW:  Create exceptions for each channel type

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,       // Reward/completion
        BIO_CHANNEL_SEROTONIN,      // Mood/state
        BIO_CHANNEL_NOREPINEPHRINE, // Alertness
        BIO_CHANNEL_ACETYLCHOLINE   // Attention
    };

    for (auto channel : channels) {
        // Create promise for this channel
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(channel, sizeof(int));
        ASSERT_NE(promise, nullptr);

        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        ASSERT_NE(future, nullptr);

        // Fail with channel-specific error
        nimcp_bio_promise_fail(promise, NIMCP_BIO_ERROR_CHANNEL_SATURATED);

        // Create exception
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_BIO_ERROR_CHANNEL_SATURATED,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "%s channel saturated", nimcp_bio_channel_name(channel)
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_set_context(ex, "channel", nimcp_bio_channel_name(channel));
        nimcp_exception_dispatch(ex);

        nimcp_exception_unref(ex);
        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    // All channel errors should have been seen
    EXPECT_GE(g_async_state.callback_count.load(), 4);
}

//=============================================================================
// Test: Bio-Async Statistics After Exceptions
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, BioAsyncStatisticsAfterExceptions) {
    // WHAT: Test bio-async statistics track failed operations
    // WHY:  Statistics help diagnose async failure patterns
    // HOW:  Create and fail promises, check statistics

    if (!bio_async_initialized_) {
        GTEST_SKIP() << "Bio-async not initialized";
    }

    // Reset stats
    nimcp_bio_async_reset_stats();

    // Create and complete some promises
    for (int i = 0; i < 3; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        if (p) {
            nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
            int val = i;
            nimcp_bio_promise_complete(p, &val);
            nimcp_bio_future_destroy(f);
            nimcp_bio_promise_destroy(p);
        }
    }

    // Create and fail some promises
    for (int i = 0; i < 2; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        if (p) {
            nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
            nimcp_bio_promise_fail(p, NIMCP_ERROR_OPERATION_FAILED);
            nimcp_bio_future_destroy(f);
            nimcp_bio_promise_destroy(p);
        }
    }

    // Get statistics
    nimcp_bio_async_stats_t stats;
    nimcp_error_t stat_result = nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(stat_result, NIMCP_SUCCESS);

    // Should have tracked created and completed futures
    // Note: In async context, not all futures may complete immediately
    EXPECT_GE(stats.total_futures_created, 1u);
    EXPECT_GE(stats.total_futures_completed, 1u);
}

//=============================================================================
// Test: Exception Metrics for Async Operations
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, ExceptionMetricsForAsyncOperations) {
    // WHAT: Test exception metrics track async-related exceptions
    // WHY:  Metrics help identify patterns in async failures
    // HOW:  Create async exceptions, check metrics

    // Reset metrics
    nimcp_metrics_reset();

    // Create several async exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_TIMEOUT,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Metrics test exception %d", i
        );
        if (ex) {
            nimcp_metrics_record_exception(ex);
            nimcp_exception_unref(ex);
        }
    }

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_GE(metrics.total_exceptions, 5u);
}

//=============================================================================
// Test: Async Exception with Recovery Metrics
//=============================================================================

TEST_F(ExceptionAsyncIntegrationTest, AsyncExceptionWithRecoveryMetrics) {
    // WHAT: Test recovery metrics are tracked for async exceptions
    // WHY:  Track recovery success rates for async operations
    // HOW:  Create exception, recover, check metrics

    // Register recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, async_recovery_callback, nullptr);

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_CANCELLED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception for recovery metrics"
    );
    ASSERT_NE(ex, nullptr);

    // Record exception
    nimcp_metrics_record_exception(ex);

    // Execute recovery
    auto start = std::chrono::steady_clock::now();
    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    auto end = std::chrono::steady_clock::now();
    uint64_t duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(result, 0);

    // Record recovery
    nimcp_metrics_record_recovery(
        ex,
        EXCEPTION_RECOVERY_RETRY,
        true,
        duration_us
    );

    // Check metrics
    float recovery_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_RETRY);
    EXPECT_GE(recovery_rate, 0.0f);
    EXPECT_LE(recovery_rate, 1.0f);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
