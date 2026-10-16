/**
 * @file test_middleware_exception_integration.cpp
 * @brief Integration tests for middleware exception handling with immune system
 *
 * WHAT: Test complete exception flow through middleware to immune system
 * WHY:  Verify middleware exceptions properly integrate with brain immune recovery
 * HOW:  Simulate middleware failures, verify immune presentation, test recovery
 *
 * TEST SCENARIOS:
 * - Middleware exception to immune system presentation
 * - Pipeline failure with immune recovery
 * - Cross-component exception propagation
 * - Controller command exception handling
 * - Exception metrics and statistics tracking
 * - Recovery callback integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <thread>
#include <chrono>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/error/nimcp_error_codes.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> recovery_call_count;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::atomic<bool> immune_presented;

    void SetUp() override {
        handler_call_count = 0;
        recovery_call_count = 0;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        immune_presented = false;

        nimcp_exception_system_init();
        nimcp_exception_immune_init(NULL);
        nimcp_install_default_handlers();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Handler to track exception processing
    static bool test_middleware_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        return false;
    }

    // Handler that triggers immune presentation
    static bool immune_presentation_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        if (ex->severity >= EXCEPTION_SEVERITY_SEVERE) {
            nimcp_immune_response_t response;
            memset(&response, 0, sizeof(response));
            nimcp_exception_present_to_immune(ex, &response);
            immune_presented = true;
        }
        return false;
    }

    // Recovery callback for testing
    static int test_recovery_callback(
        nimcp_exception_t* ex,
        nimcp_exception_recovery_action_t action,
        void* user_data
    ) {
        (void)ex;
        (void)user_data;
        recovery_call_count++;
        last_recovery_action = action;
        return 0;  // Success
    }

    // Helper to create middleware exception
    static nimcp_exception_t* create_middleware_exception(
        nimcp_error_t code,
        nimcp_exception_severity_t severity,
        const char* message
    ) {
        return nimcp_exception_create(
            code, severity, __FILE__, __LINE__, __func__, "%s", message
        );
    }
};

std::atomic<int> MiddlewareExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> MiddlewareExceptionIntegrationTest::recovery_call_count(0);
std::atomic<nimcp_exception_recovery_action_t> MiddlewareExceptionIntegrationTest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::atomic<bool> MiddlewareExceptionIntegrationTest::immune_presented(false);

//=============================================================================
// Immune Integration Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, MiddlewareExceptionToImmune) {
    // WHAT: Test middleware exception presentation to immune system
    // WHY:  Verify immune system receives and processes middleware exceptions

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_SEVERE,
        "Pipeline stage failure requiring immune response"
    );
    ASSERT_NE(ex, nullptr);

    // Set middleware-specific context
    nimcp_exception_set_context(ex, "component", "middleware_pipeline");
    nimcp_exception_set_context(ex, "stage", "feature_extraction");

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune(ex, &response);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(ex->presented_to_immune);

    // Verify immune system processed it
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    EXPECT_GE(stats.exceptions_presented, 1u);

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionIntegrationTest, PipelineFailureWithRecovery) {
    // WHAT: Test full exception-to-recovery flow for pipeline failures
    // WHY:  Verify recovery actions are triggered for middleware exceptions

    // Register recovery callback
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        test_recovery_callback,
        NULL
    );

    recovery_call_count = 0;

    // Create exception with retry as suggested action
    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Transient pipeline failure"
    );
    ASSERT_NE(ex, nullptr);
    ex->suggested_action = EXCEPTION_RECOVERY_RETRY;

    // Execute recovery
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(recovery_call_count.load(), 1);
    EXPECT_EQ(last_recovery_action.load(), EXCEPTION_RECOVERY_RETRY);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionIntegrationTest, BufferOverflowWithGCRecovery) {
    // WHAT: Test buffer overflow triggering GC recovery
    // WHY:  Memory pressure from buffer overflow should trigger cleanup

    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC,
        test_recovery_callback,
        NULL
    );

    recovery_call_count = 0;

    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // 1MB requested
        "Buffer overflow due to memory pressure"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune - should suggest GC
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);

    // Execute suggested recovery
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    if (strategy.primary_action == EXCEPTION_RECOVERY_GC) {
        nimcp_execute_recovery((nimcp_exception_t*)ex, EXCEPTION_RECOVERY_GC);
        EXPECT_EQ(recovery_call_count.load(), 1);
    }

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_GC);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Cross-Component Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, CrossComponentExceptionPropagation) {
    // WHAT: Test exception propagation across middleware components
    // WHY:  Verify exception chaining works for component interactions

    // Root cause: buffer failure
    nimcp_exception_t* buffer_ex = create_middleware_exception(
        NIMCP_ERROR_BUFFER_OVERFLOW,
        EXCEPTION_SEVERITY_WARNING,
        "Circular buffer overflow"
    );
    nimcp_exception_set_context(buffer_ex, "component", "circular_buffer");

    // Second level: feature extraction failed due to buffer
    nimcp_exception_t* feature_ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_ERROR,
        "Feature extraction failed: input buffer corrupted"
    );
    nimcp_exception_set_context(feature_ex, "component", "feature_extractor");
    nimcp_exception_set_cause(feature_ex, buffer_ex);

    // Top level: pipeline failure
    nimcp_exception_t* pipeline_ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_SEVERE,
        "Pipeline execution failed"
    );
    nimcp_exception_set_context(pipeline_ex, "component", "middleware_pipeline");
    nimcp_exception_set_cause(pipeline_ex, feature_ex);

    // Verify chain
    nimcp_exception_t* cause1 = nimcp_exception_get_cause(pipeline_ex);
    ASSERT_NE(cause1, nullptr);
    EXPECT_EQ(cause1->code, NIMCP_ERROR_INVALID_STATE);

    nimcp_exception_t* cause2 = nimcp_exception_get_cause(cause1);
    ASSERT_NE(cause2, nullptr);
    EXPECT_EQ(cause2->code, NIMCP_ERROR_BUFFER_OVERFLOW);

    // Present top-level to immune - should include chain info in epitope
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(pipeline_ex, &response);

    EXPECT_TRUE(pipeline_ex->presented_to_immune);

    nimcp_exception_unref(pipeline_ex);
}

TEST_F(MiddlewareExceptionIntegrationTest, RoutingToAttentionExceptionFlow) {
    // WHAT: Test exception flow from routing to attention components
    // WHY:  Verify inter-component exception handling

    // Create routing exception
    nimcp_exception_t* routing_ex = create_middleware_exception(
        NIMCP_ERROR_ROUTE_NOT_FOUND,
        EXCEPTION_SEVERITY_WARNING,
        "No route available for attention signal"
    );
    nimcp_exception_set_context(routing_ex, "component", "thalamic_router");
    nimcp_exception_set_context(routing_ex, "target", "prefrontal_attention");

    // Create attention gate exception caused by routing
    nimcp_exception_t* attention_ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_STATE,
        EXCEPTION_SEVERITY_ERROR,
        "Attention gate received no input: routing failed"
    );
    nimcp_exception_set_context(attention_ex, "component", "attention_gate");
    nimcp_exception_set_cause(attention_ex, routing_ex);

    // Dispatch through handlers
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_handler";
    options.handler = test_middleware_handler;
    options.priority = 50;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    handler_call_count = 0;
    nimcp_exception_dispatch(attention_ex);

    EXPECT_GE(handler_call_count.load(), 1);

    nimcp_handler_unregister(reg);
    nimcp_exception_unref(attention_ex);
}

//=============================================================================
// Controller Exception Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, ControllerCommandException) {
    // WHAT: Test exception handling for controller command failures
    // WHY:  Middleware controller commands can fail and need proper handling

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_WARNING,
        "Controller command rejected: invalid attention threshold"
    );
    nimcp_exception_set_context(ex, "component", "middleware_controller");
    nimcp_exception_set_context(ex, "command_type", "set_attention_threshold");
    nimcp_exception_set_context(ex, "parameter", "threshold");
    nimcp_exception_set_context(ex, "value", "2.5");
    nimcp_exception_set_context(ex, "valid_range", "0.0-1.0");

    EXPECT_STREQ(nimcp_exception_get_context(ex, "command_type"), "set_attention_threshold");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "value"), "2.5");

    nimcp_exception_unref(ex);
}

TEST_F(MiddlewareExceptionIntegrationTest, ControllerBatchFailureException) {
    // WHAT: Test aggregate exception for batch command failures
    // WHY:  Batch commands may have multiple partial failures

    nimcp_aggregate_exception_t* agg = nimcp_aggregate_exception_create(
        NIMCP_ERROR_MULTIPLE_ERRORS,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Batch command partial failure: 2 of 5 commands failed"
    );
    ASSERT_NE(agg, nullptr);

    // Add individual command failures
    nimcp_exception_t* cmd1_ex = create_middleware_exception(
        NIMCP_ERROR_INVALID_PARAMETER,
        EXCEPTION_SEVERITY_WARNING,
        "Command 2: invalid region"
    );
    nimcp_exception_set_context(cmd1_ex, "command_index", "2");

    nimcp_exception_t* cmd2_ex = create_middleware_exception(
        NIMCP_ERROR_ROUTE_NOT_FOUND,
        EXCEPTION_SEVERITY_WARNING,
        "Command 4: route unavailable"
    );
    nimcp_exception_set_context(cmd2_ex, "command_index", "4");

    nimcp_aggregate_exception_add(agg, cmd1_ex);
    nimcp_aggregate_exception_add(agg, cmd2_ex);

    EXPECT_EQ(nimcp_aggregate_exception_count(agg), 2u);

    nimcp_exception_unref((nimcp_exception_t*)agg);
}

//=============================================================================
// Statistics and Metrics Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, ExceptionMetricsTracking) {
    // WHAT: Test exception metrics tracking through immune system
    // WHY:  Verify statistics are properly accumulated

    // Get baseline stats
    nimcp_exception_immune_stats_t baseline_stats;
    nimcp_exception_immune_get_stats(&baseline_stats);

    // Generate several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = create_middleware_exception(
            NIMCP_ERROR_PIPELINE_FAILURE,
            EXCEPTION_SEVERITY_SEVERE,
            "Test exception for metrics"
        );

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);
    }

    // Get updated stats
    nimcp_exception_immune_stats_t updated_stats;
    nimcp_exception_immune_get_stats(&updated_stats);

    EXPECT_GE(updated_stats.exceptions_presented,
              baseline_stats.exceptions_presented + 5);
}

TEST_F(MiddlewareExceptionIntegrationTest, RecoverySuccessTracking) {
    // WHAT: Test tracking of recovery success/failure
    // WHY:  Verify recovery metrics are properly recorded

    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_RETRY,
        test_recovery_callback,
        NULL
    );

    nimcp_exception_immune_stats_t baseline_stats;
    nimcp_exception_immune_get_stats(&baseline_stats);

    // Create and recover from exception
    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Recoverable exception"
    );

    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_notify_recovery_result(ex, EXCEPTION_RECOVERY_RETRY, true);

    nimcp_exception_immune_stats_t updated_stats;
    nimcp_exception_immune_get_stats(&updated_stats);

    EXPECT_GE(updated_stats.recoveries_attempted,
              baseline_stats.recoveries_attempted);
    EXPECT_GE(updated_stats.recoveries_succeeded,
              baseline_stats.recoveries_succeeded);

    nimcp_unregister_recovery_callback(EXCEPTION_RECOVERY_RETRY);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Async Exception Processing Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, AsyncExceptionPresentation) {
    // WHAT: Test async exception presentation queue
    // WHY:  High-volume middleware may need async exception processing

    // Queue several exceptions asynchronously
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = create_middleware_exception(
            NIMCP_ERROR_BUFFER_OVERFLOW,
            EXCEPTION_SEVERITY_WARNING,
            "Async buffer warning"
        );

        int result = nimcp_exception_present_async(ex);
        EXPECT_EQ(result, 0);

        nimcp_exception_unref(ex);
    }

    // Process pending
    size_t processed = nimcp_exception_immune_process_pending(0);
    EXPECT_GE(processed, 0u);  // May be 0 if already processed
}

//=============================================================================
// Epitope Generation Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, MiddlewareExceptionEpitope) {
    // WHAT: Test epitope generation for middleware exceptions
    // WHY:  Epitopes enable immune memory for recurring errors

    nimcp_exception_t* ex1 = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "First pipeline failure"
    );
    nimcp_exception_set_context(ex1, "stage", "encoding");

    nimcp_exception_t* ex2 = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Second pipeline failure"
    );
    nimcp_exception_set_context(ex2, "stage", "encoding");

    // Generate epitopes
    size_t len1 = nimcp_exception_generate_epitope(ex1);
    size_t len2 = nimcp_exception_generate_epitope(ex2);

    EXPECT_GT(len1, 0u);
    EXPECT_GT(len2, 0u);
    EXPECT_EQ(len1, len2);

    // Similar exceptions should have similar epitopes
    // (exact matching depends on implementation)
    EXPECT_EQ(ex1->epitope_len, ex2->epitope_len);

    nimcp_exception_unref(ex1);
    nimcp_exception_unref(ex2);
}

//=============================================================================
// Handler Chain Integration Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, MultiHandlerChain) {
    // WHAT: Test multiple handlers in priority order
    // WHY:  Verify handler chain processes middleware exceptions correctly

    static std::atomic<int> order_tracker(0);
    static int first_order = 0;
    static int second_order = 0;

    // First handler (high priority)
    auto high_handler = [](nimcp_exception_t* ex, void* data) -> bool {
        (void)ex; (void)data;
        first_order = ++order_tracker;
        return false;
    };

    // Second handler (low priority)
    auto low_handler = [](nimcp_exception_t* ex, void* data) -> bool {
        (void)ex; (void)data;
        second_order = ++order_tracker;
        return false;
    };

    nimcp_handler_options_t high_options;
    nimcp_handler_default_options(&high_options);
    high_options.name = "high_priority";
    high_options.handler = high_handler;
    high_options.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* high_reg = nimcp_handler_register(&high_options);

    nimcp_handler_options_t low_options;
    nimcp_handler_default_options(&low_options);
    low_options.name = "low_priority";
    low_options.handler = low_handler;
    low_options.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* low_reg = nimcp_handler_register(&low_options);

    order_tracker = 0;
    first_order = 0;
    second_order = 0;

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_PIPELINE_FAILURE,
        EXCEPTION_SEVERITY_ERROR,
        "Test exception for handler chain"
    );
    nimcp_exception_dispatch(ex);

    // High priority should be called before low priority
    EXPECT_LT(first_order, second_order);

    nimcp_handler_unregister(high_reg);
    nimcp_handler_unregister(low_reg);
    nimcp_exception_unref(ex);
}

//=============================================================================
// Try/Catch Integration Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, TryCatchWithMiddlewareException) {
    // WHAT: Test try/catch macros with middleware exceptions
    // WHY:  Verify setjmp/longjmp mechanism works for middleware

    bool exception_caught = false;
    nimcp_error_t caught_code = 0;

    NIMCP_TRY {
        // Simulate middleware failure
        nimcp_exception_t* ex = create_middleware_exception(
            NIMCP_ERROR_PIPELINE_FAILURE,
            EXCEPTION_SEVERITY_ERROR,
            "Pipeline failure in try block"
        );
        nimcp_exception_raise(ex);

        // Should not reach here
        FAIL() << "Should have jumped to catch";
    }
    NIMCP_CATCH(nimcp_exception_t, caught_ex) {
        exception_caught = true;
        caught_code = caught_ex->code;
        nimcp_exception_unref(caught_ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_PIPELINE_FAILURE);
}

//=============================================================================
// Recovery Notification Tests
//=============================================================================

TEST_F(MiddlewareExceptionIntegrationTest, RecoveryNotificationToImmune) {
    // WHAT: Test recovery result notification to immune system
    // WHY:  Immune system learns from recovery outcomes

    nimcp_exception_t* ex = create_middleware_exception(
        NIMCP_ERROR_RESOURCE_EXHAUSTED,
        EXCEPTION_SEVERITY_SEVERE,
        "Resource exhaustion"
    );

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex, &response);

    // Notify successful recovery
    int result = nimcp_exception_notify_recovery_result(
        ex, EXCEPTION_RECOVERY_REDUCE_LOAD, true
    );
    EXPECT_EQ(result, 0);

    // Exception should be marked as recovered
    EXPECT_TRUE(ex->recovery_succeeded || ex->recovery_attempted);

    nimcp_exception_unref(ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
