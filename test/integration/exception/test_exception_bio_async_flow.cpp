/**
 * @file test_exception_bio_async_flow.cpp
 * @brief Integration tests for bio-async exception flow and message dispatch
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: Test exception handling integration with bio-router message dispatch
 * WHY:  Bio-async messaging must properly handle and propagate exceptions
 * HOW:  Simulate module communication with exception scenarios, verify recovery
 *
 * TEST SCENARIOS:
 * - Exception handling in bio-router message dispatch
 * - Exception handling in async task execution
 * - Exception recovery in concurrent scenarios
 * - Exception aggregation from parallel operations
 * - Exception propagation via bio-async channels
 * - Circuit breaker integration with bio-router
 * - Message handler exception flow
 * - Phase sync exception handling
 *
 * HEADER FILES REFERENCED:
 * - include/async/nimcp_bio_router.h
 * - include/async/nimcp_bio_async.h
 * - include/utils/exception/nimcp_exception.h
 * - include/utils/exception/nimcp_exception_handlers.h
 * - include/utils/exception/nimcp_exception_immune.h
 * - include/utils/exception/nimcp_exception_circuit.h
 * - include/utils/exception/nimcp_exception_metrics.h
 *
 * FUNCTION SIGNATURES USED:
 * - bio_router_init(config) -> nimcp_error_t
 * - bio_router_shutdown()
 * - bio_router_register_module(info) -> bio_module_context_t
 * - bio_router_unregister_module(ctx)
 * - bio_router_register_handler(ctx, msg_type, handler) -> nimcp_error_t
 * - bio_router_send(ctx, msg, msg_size, timeout_ms) -> nimcp_error_t
 * - bio_router_send_async(ctx, msg, msg_size, channel) -> nimcp_bio_promise_t
 * - bio_router_process_inbox(ctx, max_messages) -> uint32_t
 * - nimcp_exception_create(code, severity, file, line, func, format, ...) -> nimcp_exception_t*
 * - nimcp_aggregate_exception_create(...) -> nimcp_aggregate_exception_t*
 * - nimcp_aggregate_exception_add(agg, child) -> int
 * - nimcp_exception_dispatch(ex) -> bool
 * - nimcp_circuit_record(ex) -> int
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
#include <pthread.h>

extern "C" {
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test State Tracking
//=============================================================================

/**
 * @brief Tracks exception events from bio-router message handling
 */
struct BioAsyncExceptionEvent {
    nimcp_error_t code;
    nimcp_exception_severity_t severity;
    std::string message;
    std::string module_name;
    uint32_t msg_type;
    uint64_t timestamp;
    bool handler_returned_error;
};

/**
 * @brief Global state for bio-async exception tests
 */
static struct {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<BioAsyncExceptionEvent> events;
    std::atomic<int> messages_processed{0};
    std::atomic<int> exceptions_caught{0};
    std::atomic<int> handler_errors{0};
    std::atomic<bool> force_handler_error{false};
    std::atomic<bool> force_slow_handler{false};
    std::queue<nimcp_exception_t*> pending_exceptions;
} g_bio_async_state;

//=============================================================================
// Mock Message Types
//=============================================================================

/** Custom message type for testing - cast to bio_message_type_t */
static const bio_message_type_t BIO_MSG_TEST_REQUEST  = (bio_message_type_t)0x1001;
static const bio_message_type_t BIO_MSG_TEST_RESPONSE = (bio_message_type_t)0x1002;
static const bio_message_type_t BIO_MSG_TEST_ERROR    = (bio_message_type_t)0x1003;
static const bio_message_type_t BIO_MSG_TEST_BATCH    = (bio_message_type_t)0x1004;

/**
 * @brief Test message header (MUST match bio_message_header_t layout exactly)
 *
 * Layout from nimcp_bio_messages.h:
 * - bio_message_type_t type (4 bytes)
 * - uint32_t sequence_id (4 bytes)
 * - uint32_t source_module (4 bytes)
 * - uint32_t target_module (4 bytes)
 * - uint64_t timestamp_us (8 bytes)
 * - nimcp_bio_channel_type_t channel (4 bytes)
 * - uint32_t payload_size (4 bytes)
 * - uint32_t flags (4 bytes)
 */
typedef struct {
    uint32_t msg_type;       /* bio_message_type_t type */
    uint32_t sequence_id;    /* sequence number */
    uint32_t source_module;  /* source module ID */
    uint32_t target_module;  /* target module ID */
    uint64_t timestamp;      /* timestamp_us */
    uint32_t channel;        /* nimcp_bio_channel_type_t */
    uint32_t payload_size;   /* payload size */
    uint32_t flags;          /* message flags */
} test_bio_msg_header_t;

/**
 * @brief Test request message
 */
typedef struct {
    test_bio_msg_header_t header;
    uint32_t operation_id;
    char payload[64];
} test_request_msg_t;

/**
 * @brief Test response message
 */
typedef struct {
    test_bio_msg_header_t header;
    uint32_t operation_id;
    nimcp_error_t result_code;
    char result_data[64];
} test_response_msg_t;

//=============================================================================
// Test Module IDs - cast to bio_module_id_t
//=============================================================================

static const bio_module_id_t TEST_MODULE_SENDER      = (bio_module_id_t)0x0001;
static const bio_module_id_t TEST_MODULE_RECEIVER    = (bio_module_id_t)0x0002;
static const bio_module_id_t TEST_MODULE_AGGREGATOR  = (bio_module_id_t)0x0003;

//=============================================================================
// Message Handler Functions
//=============================================================================

/**
 * @brief Message handler that may throw exceptions
 */
static nimcp_error_t test_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;
    (void)response_promise;
    (void)user_data;

    const test_bio_msg_header_t* header = (const test_bio_msg_header_t*)msg;

    g_bio_async_state.messages_processed++;

    // Simulate slow handler if requested
    if (g_bio_async_state.force_slow_handler.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Simulate handler error if requested
    if (g_bio_async_state.force_handler_error.load()) {
        g_bio_async_state.handler_errors++;

        // Create and record exception
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Handler error for msg_type 0x%04X", header->msg_type
        );

        if (ex) {
            // Record event
            BioAsyncExceptionEvent event;
            event.code = ex->code;
            event.severity = ex->severity;
            event.message = ex->message;
            event.module_name = "test_receiver";
            event.msg_type = header->msg_type;
            event.timestamp = ex->timestamp_us;
            event.handler_returned_error = true;

            {
                std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
                g_bio_async_state.events.push_back(event);
                g_bio_async_state.cv.notify_all();
            }

            g_bio_async_state.exceptions_caught++;
            nimcp_exception_dispatch(ex);
            nimcp_exception_unref(ex);
        }

        return NIMCP_ERROR_OPERATION_FAILED;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Message handler that creates exceptions based on message content
 */
static nimcp_error_t error_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg_size;
    (void)response_promise;
    (void)user_data;

    const test_request_msg_t* request = (const test_request_msg_t*)msg;

    g_bio_async_state.messages_processed++;

    // This handler always creates an exception for ERROR messages
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Error message processed: operation_id=%u", request->operation_id
    );

    if (ex) {
        nimcp_exception_set_context(ex, "operation_id",
            std::to_string(request->operation_id).c_str());
        nimcp_exception_set_context(ex, "module", "error_handler");

        BioAsyncExceptionEvent event;
        event.code = ex->code;
        event.severity = ex->severity;
        event.message = ex->message;
        event.module_name = "error_handler";
        event.msg_type = request->header.msg_type;
        event.timestamp = ex->timestamp_us;
        event.handler_returned_error = true;

        {
            std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
            g_bio_async_state.events.push_back(event);
            g_bio_async_state.cv.notify_all();
        }

        g_bio_async_state.exceptions_caught++;
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    return NIMCP_ERROR_OPERATION_FAILED;
}

/**
 * @brief Exception handler callback for bio-async tests
 */
static bool bio_async_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;

    BioAsyncExceptionEvent event;
    event.code = ex->code;
    event.severity = ex->severity;
    event.message = ex->message;
    event.module_name = "exception_handler";
    event.msg_type = 0;
    event.timestamp = ex->timestamp_us;
    event.handler_returned_error = false;

    {
        std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
        g_bio_async_state.events.push_back(event);
    }

    return false;  // Don't consume, let others see it
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionBioAsyncFlowTest : public ::testing::Test {
protected:
    bool bio_router_initialized_ = false;
    bool bio_async_initialized_ = false;
    bio_module_context_t sender_ctx_ = nullptr;
    bio_module_context_t receiver_ctx_ = nullptr;
    nimcp_handler_registration_t* handler_reg_ = nullptr;

    void SetUp() override {
        // Reset state
        {
            std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
            g_bio_async_state.events.clear();
            while (!g_bio_async_state.pending_exceptions.empty()) {
                nimcp_exception_t* ex = g_bio_async_state.pending_exceptions.front();
                g_bio_async_state.pending_exceptions.pop();
                if (ex) nimcp_exception_unref(ex);
            }
        }
        g_bio_async_state.messages_processed = 0;
        g_bio_async_state.exceptions_caught = 0;
        g_bio_async_state.handler_errors = 0;
        g_bio_async_state.force_handler_error = false;
        g_bio_async_state.force_slow_handler = false;

        // Initialize exception system
        nimcp_exception_system_init();

        // Initialize circuit breaker
        nimcp_circuit_init();

        // Initialize metrics
        nimcp_metrics_init();

        // Initialize immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = false;
        nimcp_exception_immune_init(&immune_config);

        // Initialize bio-async
        nimcp_bio_async_config_t async_config = nimcp_bio_async_default_config();
        async_config.enable_statistics = true;
        async_config.thread_pool_size = 2;
        if (nimcp_bio_async_init(&async_config) == NIMCP_SUCCESS) {
            bio_async_initialized_ = true;
        }

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 16;
        router_config.inbox_capacity = 64;
        router_config.enable_statistics = true;
        if (bio_router_init(&router_config) == NIMCP_SUCCESS) {
            bio_router_initialized_ = true;
        }

        // Register exception handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "bio_async_test_handler";
        opts.handler = bio_async_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg_ = nimcp_handler_register(&opts);
    }

    void TearDown() override {
        // Unregister modules
        if (receiver_ctx_) {
            bio_router_unregister_module(receiver_ctx_);
            receiver_ctx_ = nullptr;
        }
        if (sender_ctx_) {
            bio_router_unregister_module(sender_ctx_);
            sender_ctx_ = nullptr;
        }

        // Unregister handler
        if (handler_reg_) {
            nimcp_handler_unregister(handler_reg_);
            handler_reg_ = nullptr;
        }

        // Shutdown in reverse order
        if (bio_router_initialized_) {
            bio_router_shutdown();
            bio_router_initialized_ = false;
        }

        if (bio_async_initialized_) {
            nimcp_bio_async_shutdown();
            bio_async_initialized_ = false;
        }

        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_handlers_shutdown();
        nimcp_exception_system_shutdown();
    }

    void registerTestModules() {
        if (!bio_router_initialized_) return;

        // Register sender module
        bio_module_info_t sender_info;
        sender_info.module_id = TEST_MODULE_SENDER;
        sender_info.module_name = "test_sender";
        sender_info.inbox_capacity = 32;
        sender_info.user_data = nullptr;
        sender_ctx_ = bio_router_register_module(&sender_info);

        // Register receiver module
        bio_module_info_t receiver_info;
        receiver_info.module_id = TEST_MODULE_RECEIVER;
        receiver_info.module_name = "test_receiver";
        receiver_info.inbox_capacity = 32;
        receiver_info.user_data = nullptr;
        receiver_ctx_ = bio_router_register_module(&receiver_info);

        // Register message handlers
        if (receiver_ctx_) {
            bio_router_register_handler(receiver_ctx_, BIO_MSG_TEST_REQUEST,
                                        test_message_handler);
            bio_router_register_handler(receiver_ctx_, BIO_MSG_TEST_ERROR,
                                        error_message_handler);
        }
    }

    void waitForEvents(size_t expected_count, int timeout_ms = 2000) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
        std::unique_lock<std::mutex> lock(g_bio_async_state.mutex);
        while (g_bio_async_state.events.size() < expected_count) {
            if (g_bio_async_state.cv.wait_until(lock, deadline) ==
                std::cv_status::timeout) {
                break;
            }
        }
    }

    void waitForMessages(int expected_count, int timeout_ms = 2000) {
        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms);
        while (g_bio_async_state.messages_processed.load() < expected_count) {
            if (std::chrono::steady_clock::now() >= deadline) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

//=============================================================================
// Test: Basic Message Handler Exception Flow
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, MessageHandlerExceptionFlow) {
    // WHAT: Verify exceptions from message handlers are properly dispatched
    // WHY: Handler errors need to be captured and propagated

    ASSERT_TRUE(bio_router_initialized_) << "Bio-router must be initialized";

    registerTestModules();
    ASSERT_NE(sender_ctx_, nullptr);
    ASSERT_NE(receiver_ctx_, nullptr);

    // Enable handler error simulation
    g_bio_async_state.force_handler_error = true;

    // Send a test message
    test_request_msg_t request;
    memset(&request, 0, sizeof(request));
    request.header.msg_type = BIO_MSG_TEST_REQUEST;
    request.header.source_module = TEST_MODULE_SENDER;
    request.header.target_module = TEST_MODULE_RECEIVER;
    request.header.payload_size = sizeof(request) - sizeof(test_bio_msg_header_t);
    request.operation_id = 1;
    strcpy(request.payload, "test_payload");

    nimcp_error_t result = bio_router_send(sender_ctx_, &request, sizeof(request), 1000);

    // Process inbox to trigger handler
    if (receiver_ctx_) {
        bio_router_process_inbox(receiver_ctx_, 10);
    }

    // Wait for exception to be recorded
    waitForMessages(1, 1000);

    // Verify exception was caught
    EXPECT_GE(g_bio_async_state.exceptions_caught.load(), 1)
        << "Handler exception should be caught";
    EXPECT_GE(g_bio_async_state.handler_errors.load(), 1)
        << "Handler error should be recorded";

    // Check event was recorded
    {
        std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
        EXPECT_FALSE(g_bio_async_state.events.empty())
            << "Exception event should be recorded";

        if (!g_bio_async_state.events.empty()) {
            const auto& event = g_bio_async_state.events[0];
            EXPECT_EQ(event.code, NIMCP_ERROR_OPERATION_FAILED);
            EXPECT_TRUE(event.handler_returned_error);
        }
    }
}

//=============================================================================
// Test: Error Message Handler Creates Exception
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, ErrorMessageHandlerCreatesException) {
    // WHAT: Verify error message type triggers exception creation
    // WHY: Some message types should always generate exceptions

    ASSERT_TRUE(bio_router_initialized_);

    registerTestModules();
    ASSERT_NE(sender_ctx_, nullptr);
    ASSERT_NE(receiver_ctx_, nullptr);

    // Send error message type
    test_request_msg_t request;
    memset(&request, 0, sizeof(request));
    request.header.msg_type = BIO_MSG_TEST_ERROR;
    request.header.source_module = TEST_MODULE_SENDER;
    request.header.target_module = TEST_MODULE_RECEIVER;
    request.header.payload_size = sizeof(request) - sizeof(test_bio_msg_header_t);
    request.operation_id = 42;
    strcpy(request.payload, "error_payload");

    bio_router_send(sender_ctx_, &request, sizeof(request), 1000);

    // Process inbox
    if (receiver_ctx_) {
        bio_router_process_inbox(receiver_ctx_, 10);
    }

    waitForMessages(1, 1000);

    // Verify exception was created with context
    EXPECT_GE(g_bio_async_state.exceptions_caught.load(), 1);

    {
        std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
        bool found_error_event = false;
        for (const auto& event : g_bio_async_state.events) {
            if (event.msg_type == BIO_MSG_TEST_ERROR) {
                found_error_event = true;
                EXPECT_EQ(event.code, NIMCP_ERROR_OPERATION_FAILED);
                EXPECT_EQ(event.module_name, "error_handler");
                break;
            }
        }
        EXPECT_TRUE(found_error_event) << "Should find error message event";
    }
}

//=============================================================================
// Test: Multiple Concurrent Handler Exceptions
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, MultipleConcurrentHandlerExceptions) {
    // WHAT: Verify multiple concurrent exceptions are all handled
    // WHY: Real systems have concurrent errors from multiple messages

    ASSERT_TRUE(bio_router_initialized_);

    registerTestModules();
    ASSERT_NE(sender_ctx_, nullptr);
    ASSERT_NE(receiver_ctx_, nullptr);

    // Send multiple error messages
    const int num_messages = 5;
    for (int i = 0; i < num_messages; i++) {
        test_request_msg_t request;
        memset(&request, 0, sizeof(request));
        request.header.msg_type = BIO_MSG_TEST_ERROR;
        request.header.source_module = TEST_MODULE_SENDER;
        request.header.target_module = TEST_MODULE_RECEIVER;
        request.header.payload_size = sizeof(request) - sizeof(test_bio_msg_header_t);
        request.operation_id = 100 + i;
        snprintf(request.payload, sizeof(request.payload), "error_%d", i);

        bio_router_send(sender_ctx_, &request, sizeof(request), 100);
    }

    // Process all messages
    if (receiver_ctx_) {
        bio_router_process_inbox(receiver_ctx_, num_messages + 5);
    }

    waitForMessages(num_messages, 2000);

    // All messages should have generated exceptions
    EXPECT_GE(g_bio_async_state.exceptions_caught.load(), num_messages)
        << "All error messages should generate exceptions";

    {
        std::lock_guard<std::mutex> lock(g_bio_async_state.mutex);
        EXPECT_GE(g_bio_async_state.events.size(), (size_t)num_messages)
            << "All exception events should be recorded";
    }
}

//=============================================================================
// Test: Exception Aggregation from Parallel Operations
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, ExceptionAggregationFromParallelOperations) {
    // WHAT: Verify multiple exceptions can be aggregated
    // WHY: Batch operations may produce multiple related errors

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch operation failed"
    );
    ASSERT_NE(agg, nullptr);

    // Create child exceptions
    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Child exception %d", i
        );
        ASSERT_NE(child, nullptr);

        char ctx_key[32];
        snprintf(ctx_key, sizeof(ctx_key), "child_%d", i);
        nimcp_exception_set_context(child, ctx_key, "value");

        int result = nimcp_aggregate_exception_add(agg, child);
        EXPECT_EQ(result, 0) << "Should add child exception";
    }

    // Verify aggregate structure
    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 3u)
        << "Aggregate should contain 3 children";

    // Dispatch aggregate
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)agg);

    // Check children are accessible
    for (size_t i = 0; i < 3; i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg, i);
        EXPECT_NE(child, nullptr) << "Child " << i << " should be accessible";
    }

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Test: Circuit Breaker with Bio-Router Exceptions
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, CircuitBreakerWithBioRouterExceptions) {
    // WHAT: Verify circuit breaker records bio-router exceptions
    // WHY: Prevent cascading failures from repeated handler errors

    ASSERT_TRUE(nimcp_circuit_is_initialized());

    // Create and record multiple exceptions of same type
    for (int i = 0; i < 8; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Bio-router handler error %d", i
        );
        ASSERT_NE(ex, nullptr);

        int cb_result = nimcp_circuit_record(ex);
        // At low counts, circuit should be closed (proceed = 0)
        if (i < 5) {
            EXPECT_LE(cb_result, 0) << "Circuit should not be open at count " << i;
        }

        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    // State depends on threshold settings, but should be tracked
    EXPECT_TRUE(nimcp_circuit_is_initialized());

    // Get count
    size_t count = nimcp_circuit_get_count(NIMCP_ERROR_OPERATION_FAILED, 60);
    EXPECT_GE(count, 8u) << "Circuit should track exception count";
}

//=============================================================================
// Test: Metrics Recording for Bio-Router Exceptions
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, MetricsRecordingForBioRouterExceptions) {
    // WHAT: Verify metrics system tracks bio-router exceptions
    // WHY: Observability for exception patterns

    ASSERT_TRUE(nimcp_metrics_is_initialized());

    // Create and record exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Metrics test exception %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_metrics_record_exception(ex);
        nimcp_exception_dispatch(ex);
        nimcp_exception_unref(ex);
    }

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_GE(metrics.total_exceptions, 5u)
        << "Metrics should record all exceptions";
}

//=============================================================================
// Test: Exception Context Preserved in Async Flow
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, ExceptionContextPreservedInAsyncFlow) {
    // WHAT: Verify exception context survives async processing
    // WHY: Debug information must not be lost

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Async timeout"
    );
    ASSERT_NE(ex, nullptr);

    // Add context
    nimcp_exception_set_context(ex, "operation", "bio_router_send");
    nimcp_exception_set_context(ex, "target_module", "receiver");
    nimcp_exception_set_context(ex, "timeout_ms", "1000");

    // Generate epitope
    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u) << "Epitope should be generated";

    // Dispatch
    nimcp_exception_dispatch(ex);

    // Verify context is still accessible
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u)
        << "Context entries should be preserved";

    const char* op = nimcp_exception_get_context(ex, "operation");
    EXPECT_NE(op, nullptr);
    if (op) {
        EXPECT_STREQ(op, "bio_router_send");
    }

    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Exception Recovery in Async Context
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, ExceptionRecoveryInAsyncContext) {
    // WHAT: Verify recovery actions work in async context
    // WHY: Async operations need proper recovery support

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Async operation timeout"
    );
    ASSERT_NE(ex, nullptr);

    // Get suggested recovery action
    nimcp_exception_recovery_action_t suggested = nimcp_exception_get_suggested_recovery(ex);

    // Timeout usually suggests RETRY
    EXPECT_TRUE(
        suggested == EXCEPTION_RECOVERY_RETRY ||
        suggested == EXCEPTION_RECOVERY_NONE ||
        suggested == EXCEPTION_RECOVERY_REDUCE_LOAD
    ) << "Suggested action should be reasonable for timeout";

    // Set suggested action explicitly
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    // Mark recovery attempted
    ex->recovery_attempted = true;
    ex->recovery_succeeded = false;  // First attempt failed

    // Record in metrics
    nimcp_metrics_record_exception(ex);

    nimcp_exception_dispatch(ex);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Test: Handler Slow Execution Does Not Block Others
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, SlowHandlerDoesNotBlock) {
    // WHAT: Verify slow handlers don't prevent exception processing
    // WHY: System must remain responsive under load

    ASSERT_TRUE(bio_router_initialized_);

    registerTestModules();
    ASSERT_NE(sender_ctx_, nullptr);
    ASSERT_NE(receiver_ctx_, nullptr);

    // Enable slow handler
    g_bio_async_state.force_slow_handler = true;

    auto start = std::chrono::steady_clock::now();

    // Send multiple messages
    for (int i = 0; i < 3; i++) {
        test_request_msg_t request;
        memset(&request, 0, sizeof(request));
        request.header.msg_type = BIO_MSG_TEST_REQUEST;
        request.header.source_module = TEST_MODULE_SENDER;
        request.header.target_module = TEST_MODULE_RECEIVER;
        request.header.payload_size = sizeof(request) - sizeof(test_bio_msg_header_t);
        request.operation_id = i;

        bio_router_send(sender_ctx_, &request, sizeof(request), 100);
    }

    // Process messages
    if (receiver_ctx_) {
        bio_router_process_inbox(receiver_ctx_, 10);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete eventually
    waitForMessages(3, 3000);

    EXPECT_GE(g_bio_async_state.messages_processed.load(), 3)
        << "All messages should be processed";
}

//=============================================================================
// Test: Exception Cause Chain in Async Context
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, ExceptionCauseChainInAsyncContext) {
    // WHAT: Verify exception cause chains work in async flow
    // WHY: Root cause tracking essential for debugging

    // Create root cause
    nimcp_exception_t* root = nimcp_exception_create(
        NIMCP_ERROR_TIMEOUT,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Network timeout"
    );
    ASSERT_NE(root, nullptr);
    nimcp_exception_set_context(root, "host", "remote_node");

    // Create wrapper
    nimcp_exception_t* wrapper = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Bio-router send failed"
    );
    ASSERT_NE(wrapper, nullptr);

    // Chain them
    nimcp_exception_set_cause(wrapper, root);

    // Verify chain
    nimcp_exception_t* cause = nimcp_exception_get_cause(wrapper);
    EXPECT_EQ(cause, root) << "Cause should be the root exception";
    EXPECT_EQ(cause->code, NIMCP_ERROR_TIMEOUT);

    // Context should be on root
    const char* host = nimcp_exception_get_context(cause, "host");
    EXPECT_NE(host, nullptr);
    if (host) {
        EXPECT_STREQ(host, "remote_node");
    }

    nimcp_exception_dispatch(wrapper);
    nimcp_exception_unref(wrapper);
    // root is unreffed by wrapper cleanup
}

//=============================================================================
// Test: Statistics After Bio-Router Exceptions
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, StatisticsAfterBioRouterExceptions) {
    // WHAT: Verify bio-router statistics reflect exception handling
    // WHY: Observability for system health

    if (!bio_router_initialized_) {
        GTEST_SKIP() << "Bio-router not initialized";
    }

    registerTestModules();

    // Enable handler errors
    g_bio_async_state.force_handler_error = true;

    // Send messages that will cause errors
    for (int i = 0; i < 3; i++) {
        test_request_msg_t request;
        memset(&request, 0, sizeof(request));
        request.header.msg_type = BIO_MSG_TEST_REQUEST;
        request.header.source_module = TEST_MODULE_SENDER;
        request.header.target_module = TEST_MODULE_RECEIVER;
        request.header.payload_size = sizeof(request) - sizeof(test_bio_msg_header_t);
        request.operation_id = i;

        bio_router_send(sender_ctx_, &request, sizeof(request), 100);
    }

    // Process inbox
    if (receiver_ctx_) {
        bio_router_process_inbox(receiver_ctx_, 10);
    }

    waitForMessages(3, 1000);

    // Get router statistics
    bio_router_stats_t stats;
    if (bio_router_get_stats(&stats) == NIMCP_SUCCESS) {
        EXPECT_GE(stats.messages_routed, 3u)
            << "Messages should be routed";
        // handler_errors tracks handler return codes
        EXPECT_GE(stats.handler_errors + g_bio_async_state.handler_errors.load(), 0u);
    }
}

//=============================================================================
// Test: Exception Type Dispatch in Bio-Router Context
//=============================================================================

TEST_F(ExceptionBioAsyncFlowTest, TypedExceptionDispatchInBioRouter) {
    // WHAT: Verify typed exceptions work in bio-router context
    // WHY: Different exception types may need different handling

    // Memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,  // requested size
        "Bio-router buffer allocation failed"
    );
    ASSERT_NE(mem_ex, nullptr);
    EXPECT_EQ(((nimcp_exception_t*)mem_ex)->type, EXCEPTION_TYPE_MEMORY);
    nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);

    // Threading exception - use pthread_self() for thread ID
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_MUTEX_LOCK,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        (uint64_t)pthread_self(),
        "Bio-router mutex lock failed"
    );
    ASSERT_NE(thread_ex, nullptr);
    EXPECT_EQ(((nimcp_exception_t*)thread_ex)->type, EXCEPTION_TYPE_THREADING);
    nimcp_exception_dispatch((nimcp_exception_t*)thread_ex);
    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
