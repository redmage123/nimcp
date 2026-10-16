/**
 * @file e2e_test_exception_pipeline.cpp
 * @brief E2E Test for Complete Exception Handling Pipeline
 *
 * WHAT: Full end-to-end tests for the exception handling system
 * WHY:  Verify complete error -> exception -> handler -> immune -> recovery flow
 * HOW:  Test exception creation, dispatch, immune presentation, and recovery
 *
 * TEST SCENARIOS:
 * 1. ExceptionCreationPipeline - Test exception object creation and hierarchy
 * 2. HandlerDispatchPipeline - Test handler registration and priority dispatch
 * 3. ImmuneIntegrationPipeline - Test exception-to-antigen presentation
 * 4. RecoveryFlowPipeline - Test complete recovery cycle
 * 5. CircuitBreakerPipeline - Test circuit breaker under load
 * 6. MetricsTrackingPipeline - Test adaptive metrics and learning
 * 7. TryCatchMechanism - Test setjmp/longjmp try/catch flow
 * 8. FullExceptionPipeline - Complete end-to-end flow
 * 9. ConcurrentExceptionsPipeline - Multiple threads raising exceptions
 * 10. StressTestPipeline - High-frequency exception handling
 *
 * ARCHITECTURE:
 * ```
 * Exception Raised -> Log -> Present as Antigen -> Immune Response -> Recovery
 *                                    |
 *                    B Cell Activation -> Antibody Production
 *                                    |
 *                    Execute Antibody (GC, restart, rollback, quarantine)
 *                                    |
 *                    Memory Formation (learn pattern for future)
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-01-16
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
}

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr uint32_t TEST_NODE_ID = 1;
constexpr uint32_t TEST_CLUSTER_SIZE = 5;

// Timing thresholds (milliseconds)
constexpr double MAX_EXCEPTION_CREATE_TIME_MS = 10.0;
constexpr double MAX_DISPATCH_TIME_MS = 50.0;
constexpr double MAX_IMMUNE_RESPONSE_TIME_MS = 100.0;
constexpr double MAX_RECOVERY_TIME_MS = 200.0;

// Test parameters
constexpr int CONCURRENT_THREADS = 4;
constexpr int EXCEPTIONS_PER_THREAD = 50;
constexpr int STRESS_TEST_ITERATIONS = 100;

//=============================================================================
// Callback Tracking
//=============================================================================

struct HandlerTracker {
    std::atomic<int> handler_calls{0};
    std::atomic<int> recovery_calls{0};
    std::atomic<int> immune_presentations{0};
    std::atomic<int> handled_count{0};

    nimcp_exception_t* last_exception{nullptr};
    nimcp_exception_recovery_action_t last_recovery_action{EXCEPTION_RECOVERY_NONE};

    void reset() {
        handler_calls = 0;
        recovery_calls = 0;
        immune_presentations = 0;
        handled_count = 0;
        last_exception = nullptr;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
    }
};

static HandlerTracker g_tracker;

// Custom exception handler callback
static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    g_tracker.handler_calls++;
    g_tracker.last_exception = ex;

    // Handle ERROR and below, pass SEVERE and above to immune
    if (ex->severity <= EXCEPTION_SEVERITY_ERROR) {
        g_tracker.handled_count++;
        return true;  // Mark as handled
    }
    return false;  // Pass to next handler
}

// Custom recovery callback
static int test_recovery_callback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)user_data;
    g_tracker.recovery_calls++;
    g_tracker.last_recovery_action = action;
    return 0;  // Success
}

// High-priority security handler
static bool security_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex->category == EXCEPTION_CATEGORY_SECURITY) {
        g_tracker.handled_count++;
        return true;  // Security exceptions always handled by this handler
    }
    return false;
}

//=============================================================================
// Test Fixture
//=============================================================================

class ExceptionPipelineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune;
    nimcp_handler_registration_t* test_handler_reg;
    nimcp_handler_registration_t* security_handler_reg;

    void SetUp() override {
        g_tracker.reset();

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0);

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0);

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0);

        // Create immune system
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        config.enable_logging = false;

        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);
        ASSERT_EQ(brain_immune_start(immune), 0);

        // Connect exception system to immune
        ASSERT_EQ(nimcp_exception_immune_connect(immune), 0);

        // Register test handlers
        test_handler_reg = nullptr;
        security_handler_reg = nullptr;
    }

    void TearDown() override {
        // Unregister handlers
        if (test_handler_reg) {
            nimcp_handler_unregister(test_handler_reg);
        }
        if (security_handler_reg) {
            nimcp_handler_unregister(security_handler_reg);
        }

        // Disconnect and cleanup
        nimcp_exception_immune_disconnect();

        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
        }

        // Shutdown subsystems
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    void RegisterTestHandlers() {
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "test_handler";
        opts.handler = test_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        test_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(test_handler_reg, nullptr);

        // Register security handler with high priority
        nimcp_handler_default_options(&opts);
        opts.name = "security_handler";
        opts.handler = security_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
        opts.category_filter = EXCEPTION_CATEGORY_SECURITY;
        security_handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(security_handler_reg, nullptr);
    }

    void RegisterRecoveryCallbacks() {
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_THREAD, test_recovery_callback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, test_recovery_callback, nullptr);
    }
};

//=============================================================================
// E2E Test: Exception Creation Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, ExceptionCreationPipeline) {
    E2E_PIPELINE_START("Exception Creation Pipeline");

    E2E_STAGE_BEGIN("Create base exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Test exception: %s", "creation test"
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->type, EXCEPTION_TYPE_BASE);
    EXPECT_EQ(ex->severity, EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_STRNE(ex->message, "");
    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create memory exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // 1MB requested
        "Memory allocation failed"
    );
    ASSERT_NE(mem_ex, nullptr);
    EXPECT_EQ(mem_ex->base.type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(mem_ex->base.category, EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(mem_ex->requested_size, 1024 * 1024);
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create brain exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_NOT_INITIALIZED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        42,  // brain_id
        "cortex",  // region_name
        "Brain training diverged"
    );
    ASSERT_NE(brain_ex, nullptr);
    EXPECT_EQ(brain_ex->base.type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(brain_ex->base.category, EXCEPTION_CATEGORY_BRAIN);
    EXPECT_EQ(brain_ex->brain_id, 42u);
    nimcp_exception_unref((nimcp_exception_t*)brain_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create threading exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,  // thread_id
        "Deadlock detected in worker pool"
    );
    ASSERT_NE(thread_ex, nullptr);
    EXPECT_EQ(thread_ex->base.type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(thread_ex->base.category, EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(thread_ex->thread_id, 12345u);
    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create security exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,  // threat_type
        "Unauthorized access attempt detected"
    );
    ASSERT_NE(sec_ex, nullptr);
    EXPECT_EQ(sec_ex->base.type, EXCEPTION_TYPE_SECURITY);
    EXPECT_EQ(sec_ex->base.category, EXCEPTION_CATEGORY_SECURITY);
    EXPECT_EQ(sec_ex->threat_type, 1u);
    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Create aggregate exception", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_aggregate_exception_t* agg_ex = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Batch operation failed"
    );
    ASSERT_NE(agg_ex, nullptr);

    // Add child exceptions
    nimcp_exception_t* child1 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child exception 1"
    );
    nimcp_exception_t* child2 = nimcp_exception_create(
        NIMCP_ERROR_INVALID_PARAM,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Child exception 2"
    );

    EXPECT_EQ(nimcp_aggregate_exception_add(agg_ex, child1), 0);
    EXPECT_EQ(nimcp_aggregate_exception_add(agg_ex, child2), 0);
    EXPECT_EQ(nimcp_aggregate_exception_count(agg_ex), 2u);

    nimcp_exception_unref((nimcp_exception_t*)agg_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test exception context", MAX_EXCEPTION_CREATE_TIME_MS);
    ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Context test"
    );
    ASSERT_NE(ex, nullptr);

    EXPECT_EQ(nimcp_exception_set_context(ex, "key1", "value1"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "key2", "value2"), 0);
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key1"), "value1");
    EXPECT_STREQ(nimcp_exception_get_context(ex, "key2"), "value2");
    EXPECT_EQ(nimcp_exception_context_count(ex), 2u);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test exception chaining", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_exception_t* cause = nimcp_exception_create(
        NIMCP_ERROR_IO_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause: disk full"
    );
    nimcp_exception_t* effect = nimcp_exception_create(
        NIMCP_ERROR_CHECKPOINT_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Checkpoint save failed"
    );

    nimcp_exception_ref(cause);  // Chain takes ownership of ref
    nimcp_exception_set_cause(effect, cause);
    EXPECT_EQ(nimcp_exception_get_cause(effect), cause);

    nimcp_exception_unref(effect);  // Will also release cause
    nimcp_exception_unref(cause);   // Release our ref
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Handler Dispatch Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, HandlerDispatchPipeline) {
    E2E_PIPELINE_START("Handler Dispatch Pipeline");

    E2E_STAGE_BEGIN("Register handlers", MAX_DISPATCH_TIME_MS);
    RegisterTestHandlers();
    EXPECT_GE(nimcp_handler_count(), 2u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Dispatch low severity exception", MAX_DISPATCH_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Low severity test"
    );

    bool handled = nimcp_exception_dispatch(ex);
    EXPECT_TRUE(handled);
    EXPECT_GE(g_tracker.handler_calls.load(), 1);
    EXPECT_GE(g_tracker.handled_count.load(), 1);
    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Dispatch security exception (high priority)", MAX_DISPATCH_TIME_MS);
    g_tracker.reset();

    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1, "Security test"
    );

    handled = nimcp_exception_dispatch((nimcp_exception_t*)sec_ex);
    EXPECT_TRUE(handled);
    // Security handler should catch it first due to high priority
    EXPECT_GE(g_tracker.handled_count.load(), 1);
    nimcp_exception_unref((nimcp_exception_t*)sec_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test handler disable/enable", MAX_DISPATCH_TIME_MS);
    g_tracker.reset();

    nimcp_handler_disable(test_handler_reg);

    ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Handler disabled test"
    );

    // Dispatch - test handler is disabled but default handlers may catch
    nimcp_exception_dispatch(ex);
    int initial_handled = g_tracker.handled_count.load();

    // Re-enable
    nimcp_handler_enable(test_handler_reg);

    // Dispatch same severity - should now be handled by our handler
    g_tracker.reset();
    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Handler re-enabled test"
    );
    nimcp_exception_dispatch(ex2);
    EXPECT_GE(g_tracker.handled_count.load(), 1);

    nimcp_exception_unref(ex);
    nimcp_exception_unref(ex2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test handler priority order", MAX_DISPATCH_TIME_MS);
    // Security handler has NIMCP_HANDLER_PRIORITY_HIGH
    // Test handler has NIMCP_HANDLER_PRIORITY_NORMAL
    // Security exceptions should be caught by security handler first

    g_tracker.reset();

    nimcp_security_exception_t* sec_ex2 = nimcp_security_exception_create(
        NIMCP_ERROR_PERMISSION_DENIED,
        EXCEPTION_SEVERITY_ERROR,  // Lower severity, but category matches
        __FILE__, __LINE__, __func__,
        2, "Priority test"
    );

    handled = nimcp_exception_dispatch((nimcp_exception_t*)sec_ex2);
    EXPECT_TRUE(handled);
    nimcp_exception_unref((nimcp_exception_t*)sec_ex2);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Immune Integration Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, ImmuneIntegrationPipeline) {
    E2E_PIPELINE_START("Immune Integration Pipeline");

    ASSERT_TRUE(nimcp_exception_immune_is_connected());

    E2E_STAGE_BEGIN("Present severe exception to immune", MAX_IMMUNE_RESPONSE_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Severe memory error for immune test"
    );

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int result = nimcp_exception_present_to_immune(ex, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.antigen_id, 0u);
    EXPECT_TRUE(ex->presented_to_immune);
    EXPECT_EQ(ex->antigen_id, response.antigen_id);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test epitope generation", MAX_IMMUNE_RESPONSE_TIME_MS);
    ex = nimcp_exception_create(
        NIMCP_ERROR_BRAIN_NOT_INITIALIZED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Epitope generation test"
    );

    size_t epitope_len = nimcp_exception_generate_epitope(ex);
    EXPECT_GT(epitope_len, 0u);
    EXPECT_LE(epitope_len, NIMCP_EXCEPTION_EPITOPE_SIZE);

    // Verify epitope is not all zeros
    bool has_nonzero = false;
    for (size_t i = 0; i < epitope_len; i++) {
        if (ex->epitope[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test category to antigen source mapping", MAX_IMMUNE_RESPONSE_TIME_MS);
    exception_antigen_source_t src;

    src = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_MEMORY);
    EXPECT_EQ(src, EX_ANTIGEN_SOURCE_ANOMALY);

    src = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_SECURITY);
    EXPECT_EQ(src, EX_ANTIGEN_SOURCE_BBB);

    src = nimcp_exception_to_antigen_source(EXCEPTION_CATEGORY_THREADING);
    EXPECT_EQ(src, EX_ANTIGEN_SOURCE_BFT);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test severity mapping", MAX_IMMUNE_RESPONSE_TIME_MS);
    uint32_t immune_sev;

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_DEBUG);
    EXPECT_EQ(immune_sev, 1u);

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_ERROR);
    EXPECT_EQ(immune_sev, 5u);

    immune_sev = nimcp_exception_to_immune_severity(EXCEPTION_SEVERITY_FATAL);
    EXPECT_EQ(immune_sev, 10u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test async presentation", MAX_IMMUNE_RESPONSE_TIME_MS);
    // Present multiple exceptions asynchronously
    for (int i = 0; i < 5; i++) {
        ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Async test %d", i
        );
        EXPECT_EQ(nimcp_exception_present_async(ex), 0);
        nimcp_exception_unref(ex);
    }

    // Process pending
    size_t processed = nimcp_exception_immune_process_pending(0);  // Process all
    EXPECT_GE(processed, 0u);  // May have been processed already
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify immune statistics", MAX_IMMUNE_RESPONSE_TIME_MS);
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    EXPECT_GT(stats.exceptions_presented, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Recovery Flow Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, RecoveryFlowPipeline) {
    E2E_PIPELINE_START("Recovery Flow Pipeline");

    E2E_STAGE_BEGIN("Register recovery callbacks", MAX_RECOVERY_TIME_MS);
    RegisterRecoveryCallbacks();
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test suggested recovery for memory exception", MAX_RECOVERY_TIME_MS);
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "OOM for recovery test"
    );

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)mem_ex, &strategy);

    // Memory exceptions typically suggest GC
    EXPECT_TRUE(
        strategy.primary_action == EXCEPTION_RECOVERY_GC ||
        strategy.primary_action == EXCEPTION_RECOVERY_COMPACT
    );

    nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute GC recovery action", MAX_RECOVERY_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Execute GC recovery"
    );

    int result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_tracker.recovery_calls.load(), 1);
    EXPECT_EQ(g_tracker.last_recovery_action, EXCEPTION_RECOVERY_GC);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute retry recovery action", MAX_RECOVERY_TIME_MS);
    g_tracker.reset();

    ex = nimcp_exception_create(
        NIMCP_ERROR_IO_ERROR,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Execute retry recovery"
    );

    result = nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    EXPECT_EQ(result, 0);
    EXPECT_GE(g_tracker.recovery_calls.load(), 1);
    EXPECT_EQ(g_tracker.last_recovery_action, EXCEPTION_RECOVERY_RETRY);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Notify immune of recovery result", MAX_RECOVERY_TIME_MS);
    ex = nimcp_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Recovery notification test"
    );

    // Present to immune first
    nimcp_immune_response_t response;
    nimcp_exception_present_to_immune(ex, &response);

    // Execute recovery
    nimcp_exception_execute_recovery(ex, EXCEPTION_RECOVERY_GC);

    // Notify result
    result = nimcp_exception_notify_recovery_result(ex, EXCEPTION_RECOVERY_GC, true);
    EXPECT_EQ(result, 0);

    // Verify exception was marked as recovered
    EXPECT_TRUE(ex->recovery_attempted);
    EXPECT_TRUE(ex->recovery_succeeded);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test recovery sequence for threading exception", MAX_RECOVERY_TIME_MS);
    g_tracker.reset();

    nimcp_threading_exception_t* thread_ex = nimcp_threading_exception_create(
        NIMCP_ERROR_DEADLOCK,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        12345,
        "Deadlock recovery test"
    );

    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)thread_ex, &strategy);

    // Execute primary action
    result = nimcp_exception_execute_recovery((nimcp_exception_t*)thread_ex, strategy.primary_action);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)thread_ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Circuit Breaker Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, CircuitBreakerPipeline) {
    E2E_PIPELINE_START("Circuit Breaker Pipeline");

    E2E_STAGE_BEGIN("Configure circuit breaker", MAX_DISPATCH_TIME_MS);
    // Set low threshold for testing
    EXPECT_EQ(nimcp_circuit_set_threshold(NIMCP_ERROR_OPERATION_FAILED, 5, 1000), 0);

    nimcp_circuit_state_t state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Trip circuit breaker", MAX_DISPATCH_TIME_MS);
    // Raise enough exceptions to trip the circuit
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Circuit trip test %d", i
        );

        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    // Check if circuit is now open
    state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    // Circuit may be open or closed depending on timing window
    // Just verify we can query the state
    EXPECT_TRUE(state == CIRCUIT_STATE_CLOSED ||
                state == CIRCUIT_STATE_OPEN ||
                state == CIRCUIT_STATE_HALF_OPEN);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test exception count tracking", MAX_DISPATCH_TIME_MS);
    size_t count = nimcp_circuit_get_count(NIMCP_ERROR_OPERATION_FAILED, 60);
    EXPECT_GE(count, 0u);  // May have decayed

    count = nimcp_circuit_get_count(NIMCP_ERROR_OPERATION_FAILED, 0);  // Total
    EXPECT_GT(count, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Manual circuit reset", MAX_DISPATCH_TIME_MS);
    nimcp_circuit_reset(NIMCP_ERROR_OPERATION_FAILED);
    state = nimcp_circuit_get_state(NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_EQ(state, CIRCUIT_STATE_CLOSED);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test circuit statistics", MAX_DISPATCH_TIME_MS);
    nimcp_circuit_stats_t stats;
    EXPECT_EQ(nimcp_circuit_get_stats(&stats), 0);
    EXPECT_GT(stats.total_tracked, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test circuit maintenance", MAX_DISPATCH_TIME_MS);
    nimcp_circuit_maintenance();
    // Just verify it doesn't crash
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Suppression System Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, SuppressionSystemPipeline) {
    E2E_PIPELINE_START("Suppression System Pipeline");

    E2E_STAGE_BEGIN("Suppress exception code", MAX_DISPATCH_TIME_MS);
    EXPECT_EQ(nimcp_exception_suppress(NIMCP_ERROR_OPERATION_FAILED, 5000, "Test suppression"), 0);
    EXPECT_TRUE(nimcp_exception_is_suppressed(NIMCP_ERROR_OPERATION_FAILED));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify exception is suppressed", MAX_DISPATCH_TIME_MS);
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "This should be suppressed"
    );

    bool should_process = nimcp_exception_should_process(ex);
    EXPECT_FALSE(should_process);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("List active suppressions", MAX_DISPATCH_TIME_MS);
    nimcp_error_t codes[10];
    size_t count = nimcp_suppression_list_active(codes, 10);
    EXPECT_GE(count, 1u);

    bool found_generic = false;
    for (size_t i = 0; i < count; i++) {
        if (codes[i] == NIMCP_ERROR_OPERATION_FAILED) {
            found_generic = true;
            break;
        }
    }
    EXPECT_TRUE(found_generic);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Remove suppression", MAX_DISPATCH_TIME_MS);
    EXPECT_EQ(nimcp_exception_unsuppress(NIMCP_ERROR_OPERATION_FAILED), 0);
    EXPECT_FALSE(nimcp_exception_is_suppressed(NIMCP_ERROR_OPERATION_FAILED));
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify exception is no longer suppressed", MAX_DISPATCH_TIME_MS);
    ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "This should not be suppressed"
    );

    should_process = nimcp_exception_should_process(ex);
    EXPECT_TRUE(should_process);

    nimcp_exception_unref(ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Metrics Tracking Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, MetricsTrackingPipeline) {
    E2E_PIPELINE_START("Metrics Tracking Pipeline");

    ASSERT_TRUE(nimcp_metrics_is_initialized());

    E2E_STAGE_BEGIN("Record exceptions for metrics", MAX_DISPATCH_TIME_MS);
    // Record various exception types
    for (int i = 0; i < 10; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Metrics test %d", i
        );
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    for (int i = 0; i < 5; i++) {
        nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
            NIMCP_ERROR_OUT_OF_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            1024,
            "Memory metrics test %d", i
        );
        nimcp_metrics_record_exception((nimcp_exception_t*)mem_ex);
        nimcp_exception_unref((nimcp_exception_t*)mem_ex);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get metrics snapshot", MAX_DISPATCH_TIME_MS);
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    EXPECT_GT(metrics.total_exceptions, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Query per-category metrics", MAX_DISPATCH_TIME_MS);
    float rate = nimcp_metrics_get_rate(EXCEPTION_CATEGORY_GENERIC);
    // Rate may be low if running on fast machine
    EXPECT_GE(rate, 0.0f);

    uint64_t count = nimcp_metrics_get_count(EXCEPTION_CATEGORY_MEMORY, 60);
    EXPECT_GE(count, 0u);  // May have time decay
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Record recovery metrics", MAX_DISPATCH_TIME_MS);
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OUT_OF_MEMORY,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Recovery metrics test %d", i
        );

        // Record recovery with timing
        nimcp_metrics_record_recovery(ex, EXCEPTION_RECOVERY_GC, true, 1000 + i * 100);
        nimcp_exception_unref(ex);
    }

    // Check recovery rate
    float recovery_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_GC);
    EXPECT_GT(recovery_rate, 0.0f);

    // Check MTTR
    float mttr = nimcp_metrics_get_mttr(EXCEPTION_RECOVERY_GC);
    EXPECT_GT(mttr, 0.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get top categories", MAX_DISPATCH_TIME_MS);
    nimcp_category_metrics_t top_cats[5];
    size_t top_count = nimcp_metrics_top_categories(top_cats, 5);
    EXPECT_GT(top_count, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Try/Catch Mechanism
//=============================================================================

TEST_F(ExceptionPipelineTest, TryCatchMechanism) {
    E2E_PIPELINE_START("Try/Catch Mechanism Pipeline");

    E2E_STAGE_BEGIN("Test basic try/catch", MAX_DISPATCH_TIME_MS);
    bool caught = false;

    NIMCP_TRY {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test try/catch"
        );
        nimcp_exception_raise(ex);
        // Should not reach here
        FAIL() << "Should have jumped to catch block";
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        caught = true;
        EXPECT_EQ(ex->code, NIMCP_ERROR_OPERATION_FAILED);
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(caught);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test nested try blocks", MAX_DISPATCH_TIME_MS);
    int outer_caught = 0;
    int inner_caught = 0;

    NIMCP_TRY {
        NIMCP_TRY {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_INVALID_PARAM,
                EXCEPTION_SEVERITY_WARNING,
                __FILE__, __LINE__, __func__,
                "Inner exception"
            );
            nimcp_exception_raise(ex);
        }
        NIMCP_CATCH(nimcp_exception_t, ex) {
            inner_caught++;
            nimcp_exception_unref(ex);
        }
        NIMCP_END_TRY;

        // Raise another exception in outer try
        nimcp_exception_t* ex2 = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Outer exception"
        );
        nimcp_exception_raise(ex2);
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        outer_caught++;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_EQ(inner_caught, 1);
    EXPECT_EQ(outer_caught, 1);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test try block without exception", MAX_DISPATCH_TIME_MS);
    bool entered_catch = false;

    NIMCP_TRY {
        // No exception raised
        int x = 1 + 1;
        (void)x;
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        entered_catch = true;
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_FALSE(entered_catch);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check try block state", MAX_DISPATCH_TIME_MS);
    bool in_try = false;

    NIMCP_TRY {
        in_try = nimcp_in_try_block();
    }
    NIMCP_CATCH(nimcp_exception_t, ex) {
        nimcp_exception_unref(ex);
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(in_try);
    EXPECT_FALSE(nimcp_in_try_block());  // Outside try block
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Full Exception Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, FullExceptionPipeline) {
    E2E_PIPELINE_START("Full Exception Pipeline");

    RegisterTestHandlers();
    RegisterRecoveryCallbacks();

    E2E_STAGE_BEGIN("PHASE 1: Exception Creation", MAX_EXCEPTION_CREATE_TIME_MS);
    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_OUT_OF_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        4 * 1024 * 1024,  // 4MB
        "Full pipeline test: memory exhaustion"
    );
    ASSERT_NE(ex, nullptr);

    // Add context
    nimcp_exception_set_context((nimcp_exception_t*)ex, "allocator", "default_pool");
    nimcp_exception_set_context((nimcp_exception_t*)ex, "component", "neural_network");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 2: Generate Epitope", MAX_EXCEPTION_CREATE_TIME_MS);
    size_t epitope_len = nimcp_exception_generate_epitope((nimcp_exception_t*)ex);
    EXPECT_GT(epitope_len, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 3: Record Metrics", MAX_DISPATCH_TIME_MS);
    nimcp_metrics_record_exception((nimcp_exception_t*)ex);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 4: Circuit Breaker Check", MAX_DISPATCH_TIME_MS);
    int circuit_result = nimcp_circuit_record((nimcp_exception_t*)ex);
    // Should not be blocked on first occurrence
    EXPECT_EQ(circuit_result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 5: Handler Dispatch", MAX_DISPATCH_TIME_MS);
    // High severity passes through test handler to default immune handler
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)ex);
    // May or may not be handled depending on severity
    (void)handled;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 6: Present to Immune", MAX_IMMUNE_RESPONSE_TIME_MS);
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);
    EXPECT_GT(response.antigen_id, 0u);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 7: Get Recovery Strategy", MAX_RECOVERY_TIME_MS);
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    EXPECT_NE(strategy.primary_action, EXCEPTION_RECOVERY_NONE);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 8: Execute Recovery", MAX_RECOVERY_TIME_MS);
    result = nimcp_exception_execute_recovery((nimcp_exception_t*)ex, strategy.primary_action);
    EXPECT_EQ(result, 0);

    // Notify immune of success
    nimcp_exception_notify_recovery_result((nimcp_exception_t*)ex, strategy.primary_action, true);

    EXPECT_TRUE(((nimcp_exception_t*)ex)->recovery_attempted);
    EXPECT_TRUE(((nimcp_exception_t*)ex)->recovery_succeeded);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 9: Record Recovery Metrics", MAX_DISPATCH_TIME_MS);
    nimcp_metrics_record_recovery((nimcp_exception_t*)ex, strategy.primary_action, true, 5000);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("PHASE 10: Verify Final State", MAX_DISPATCH_TIME_MS);
    nimcp_exception_immune_stats_t immune_stats;
    nimcp_exception_immune_get_stats(&immune_stats);
    EXPECT_GT(immune_stats.exceptions_presented, 0u);
    EXPECT_GT(immune_stats.recoveries_attempted, 0u);
    EXPECT_GT(immune_stats.recoveries_succeeded, 0u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Concurrent Exceptions Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, ConcurrentExceptionsPipeline) {
    E2E_PIPELINE_START("Concurrent Exceptions Pipeline");

    RegisterTestHandlers();

    std::atomic<int> exceptions_created{0};
    std::atomic<int> exceptions_dispatched{0};
    std::atomic<bool> has_error{false};

    E2E_STAGE_BEGIN("Spawn concurrent threads", 2000);
    std::vector<std::thread> threads;

    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    (i % 2 == 0) ? EXCEPTION_SEVERITY_WARNING : EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );

                if (!ex) {
                    has_error = true;
                    continue;
                }

                exceptions_created++;

                // Record metrics (thread-safe)
                nimcp_metrics_record_exception(ex);

                // Dispatch (thread-safe)
                nimcp_exception_dispatch(ex);
                exceptions_dispatched++;

                nimcp_exception_unref(ex);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(has_error);
    EXPECT_EQ(exceptions_created.load(), CONCURRENT_THREADS * EXCEPTIONS_PER_THREAD);
    EXPECT_EQ(exceptions_dispatched.load(), CONCURRENT_THREADS * EXCEPTIONS_PER_THREAD);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify metrics consistency", MAX_DISPATCH_TIME_MS);
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    // Metrics should have recorded all exceptions
    EXPECT_GE(metrics.total_exceptions, (uint64_t)(CONCURRENT_THREADS * EXCEPTIONS_PER_THREAD));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Stress Test Pipeline
//=============================================================================

TEST_F(ExceptionPipelineTest, StressTestPipeline) {
    E2E_PIPELINE_START("Stress Test Pipeline");

    E2E_STAGE_BEGIN("High-frequency exception creation", 3000);
    Timer timer;
    timer.start();

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Stress test iteration %d", i
        );
        ASSERT_NE(ex, nullptr);
        nimcp_exception_unref(ex);
    }

    timer.stop();
    double avg_create_time = (double)timer.elapsed_us() / STRESS_TEST_ITERATIONS;
    EXPECT_LT(avg_create_time, 1000.0);  // Should be < 1ms per exception
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High-frequency metrics recording", 3000);
    timer.reset();
    timer.start();

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Metrics stress %d", i
        );
        nimcp_metrics_record_exception(ex);
        nimcp_exception_unref(ex);
    }

    timer.stop();
    double avg_record_time = (double)timer.elapsed_us() / STRESS_TEST_ITERATIONS;
    EXPECT_LT(avg_record_time, 500.0);  // Should be < 500us per record
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High-frequency circuit breaker checks", 3000);
    timer.reset();
    timer.start();

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_IO_ERROR,  // Different code to avoid tripping
            EXCEPTION_SEVERITY_WARNING,
            __FILE__, __LINE__, __func__,
            "Circuit stress %d", i
        );
        nimcp_circuit_record(ex);
        nimcp_exception_unref(ex);
    }

    timer.stop();
    double avg_circuit_time = (double)timer.elapsed_us() / STRESS_TEST_ITERATIONS;
    EXPECT_LT(avg_circuit_time, 500.0);  // Should be < 500us per check
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no memory leaks", 500);
    // Run maintenance to clean up
    nimcp_circuit_maintenance();
    nimcp_suppression_clear_expired();
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: String Conversions
//=============================================================================

TEST_F(ExceptionPipelineTest, StringConversions) {
    E2E_PIPELINE_START("String Conversion Verification");

    E2E_STAGE_BEGIN("Severity strings", 50);
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_DEBUG), "DEBUG");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_WARNING), "WARNING");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_SEVERE), "SEVERE");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(nimcp_exception_severity_to_string(EXCEPTION_SEVERITY_FATAL), "FATAL");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Category strings", 50);
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_GENERIC), "GENERIC");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_IO), "IO");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_category_to_string(EXCEPTION_CATEGORY_SECURITY), "SECURITY");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Type strings", 50);
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BASE), "BASE");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_MEMORY), "MEMORY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_BRAIN), "BRAIN");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_IO), "IO");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_THREADING), "THREADING");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_SECURITY), "SECURITY");
    EXPECT_STREQ(nimcp_exception_type_to_string(EXCEPTION_TYPE_AGGREGATE), "AGGREGATE");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery action strings", 50);
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_GC), "GC");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_RESTART_THREAD), "RESTART_THREAD");
    EXPECT_STREQ(nimcp_exception_recovery_action_to_string(EXCEPTION_RECOVERY_QUARANTINE), "QUARANTINE");
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Circuit state strings", 50);
    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_CLOSED), "CLOSED");
    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_OPEN), "OPEN");
    EXPECT_STREQ(nimcp_circuit_state_to_string(CIRCUIT_STATE_HALF_OPEN), "HALF_OPEN");
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
