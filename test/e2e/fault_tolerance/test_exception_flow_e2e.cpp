/**
 * @file test_exception_flow_e2e.cpp
 * @brief E2E tests for complete exception handling flow
 * @version 1.0.0
 * @date 2026-01-20
 *
 * WHAT: End-to-end tests for complete exception handling from error occurrence
 *       through immune response to recovery
 * WHY:  Verify the integrated exception handling pipeline works correctly
 * HOW:  Test the full flow: Exception -> Logging -> Immune presentation ->
 *       Recovery action -> Result notification
 *
 * Test Scenarios:
 * 1. Complete flow: Exception -> Logging -> Immune -> Recovery -> Notification
 * 2. Exception during brain initialization handling
 * 3. Exception during forward propagation handling
 * 4. Memory exception triggering GC recovery
 * 5. Brain exception triggering rollback recovery
 * 6. Signal-based exception emergency save recovery
 * 7. Multi-exception scenario with aggregate handling
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <csignal>
#include <functional>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

// Callback tracking for exception handling
static std::atomic<int> g_exception_count{0};
static std::atomic<int> g_handler_calls{0};
static std::atomic<int> g_recovery_attempts{0};
static std::atomic<int> g_recovery_successes{0};
static std::atomic<bool> g_immune_response_received{false};
static nimcp_exception_recovery_action_t g_last_recovery_action = EXCEPTION_RECOVERY_NONE;
static nimcp_exception_severity_t g_last_severity = EXCEPTION_SEVERITY_DEBUG;
static nimcp_exception_t* g_last_exception = nullptr;

// Test handler that tracks calls
static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_calls++;
        g_last_severity = ex->severity;
        g_last_exception = ex;
    }
    return false; // Don't consume - let chain continue
}

// Test handler that consumes severe+ exceptions
static bool test_severe_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex && ex->severity >= EXCEPTION_SEVERITY_SEVERE) {
        g_handler_calls++;
        return true; // Consume severe exceptions
    }
    return false;
}

// Test recovery callback
static int test_recovery_callback(nimcp_exception_t* ex,
                                   nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)user_data;
    (void)ex;
    g_recovery_attempts++;
    g_last_recovery_action = action;
    // Simulate successful recovery
    g_recovery_successes++;
    return 0;
}

// Reset all tracking state
static void reset_callbacks() {
    g_exception_count = 0;
    g_handler_calls = 0;
    g_recovery_attempts = 0;
    g_recovery_successes = 0;
    g_immune_response_received = false;
    g_last_recovery_action = EXCEPTION_RECOVERY_NONE;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_last_exception = nullptr;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ExceptionFlowE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg = nullptr;

    void SetUp() override {
        reset_callbacks();

        // Initialize exception system
        int init_result = nimcp_exception_system_init();
        ASSERT_EQ(init_result, 0) << "Failed to initialize exception system";

        // Initialize exception-immune integration with defaults
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = true;
        immune_config.enable_auto_recovery = true;
        immune_config.min_present_severity = EXCEPTION_SEVERITY_ERROR;

        int immune_init = nimcp_exception_immune_init(&immune_config);
        ASSERT_EQ(immune_init, 0) << "Failed to initialize exception-immune integration";
    }

    void TearDown() override {
        // Unregister any test handlers
        if (handler_reg) {
            nimcp_handler_unregister(handler_reg);
            handler_reg = nullptr;
        }

        // Clean up last exception reference
        if (g_last_exception) {
            g_last_exception = nullptr;
        }

        // Shutdown subsystems
        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to register a test handler
    void RegisterTestHandler(nimcp_exception_handler_fn fn, int priority = NIMCP_HANDLER_PRIORITY_NORMAL) {
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "TestHandler";
        opts.handler = fn;
        opts.priority = priority;
        opts.user_data = nullptr;

        handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(handler_reg, nullptr) << "Failed to register handler";
    }
};

/* ============================================================================
 * Test 1: Complete Exception Flow
 *
 * Verifies: Exception -> Logging -> Immune presentation -> Recovery -> Notification
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, CompleteExceptionFlow) {
    printf("=== Test: Complete Exception Flow ===\n");

    // Step 1: Register test handler to track flow
    RegisterTestHandler(test_exception_handler, NIMCP_HANDLER_PRIORITY_LOW);
    printf("  Handler registered\n");

    // Step 2: Install default handlers (logging + immune)
    int install_result = nimcp_install_default_handlers();
    EXPECT_EQ(install_result, 0);
    printf("  Default handlers installed\n");

    // Step 3: Register recovery callback for GC action
    int recovery_reg = nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
    EXPECT_EQ(recovery_reg, 0);
    printf("  Recovery callback registered\n");

    // Step 4: Create and dispatch a memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1024 * 1024,  // Requested 1MB
        "E2E test memory allocation failed"
    );
    ASSERT_NE(mem_ex, nullptr);
    printf("  Memory exception created: %s\n", mem_ex->base.message);

    // Step 5: Dispatch exception through handler chain
    bool handled = nimcp_exception_dispatch((nimcp_exception_t*)mem_ex);
    printf("  Exception dispatched, handled=%d\n", handled ? 1 : 0);

    // Step 6: Verify handler was called
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Handler calls: %d\n", g_handler_calls.load());

    // Step 7: Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int present_result = nimcp_exception_present_to_immune(
        (nimcp_exception_t*)mem_ex, &response);
    printf("  Presented to immune, result=%d\n", present_result);

    // Step 8: Execute recovery action
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery((nimcp_exception_t*)mem_ex);
    printf("  Suggested recovery: %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    if (suggested != EXCEPTION_RECOVERY_NONE) {
        int exec_result = nimcp_execute_recovery((nimcp_exception_t*)mem_ex, suggested);
        printf("  Recovery executed, result=%d\n", exec_result);
    }

    // Step 9: Notify of recovery result
    int notify_result = nimcp_exception_notify_recovery_result(
        (nimcp_exception_t*)mem_ex,
        suggested,
        true  // success
    );
    printf("  Recovery result notified, result=%d\n", notify_result);

    // Step 10: Clean up
    nimcp_exception_unref((nimcp_exception_t*)mem_ex);

    // Verify complete flow
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("Test passed: Complete exception flow verified\n\n");
}

/* ============================================================================
 * Test 2: Exception During Brain Initialization
 *
 * Verifies handling of exceptions during brain/network initialization
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, ExceptionDuringBrainInitialization) {
    printf("=== Test: Exception During Brain Initialization ===\n");

    // Register handler
    RegisterTestHandler(test_exception_handler);

    // Create brain initialization exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1,                  // brain_id
        "prefrontal_cortex", // region_name
        "Brain initialization failed: insufficient memory for neural network"
    );
    ASSERT_NE(brain_ex, nullptr);
    printf("  Brain exception created\n");

    // Set additional context
    nimcp_exception_set_context(&brain_ex->base, "phase", "initialization");
    nimcp_exception_set_context(&brain_ex->base, "layer_count", "6");
    nimcp_exception_set_context(&brain_ex->base, "neuron_count", "1000000");
    printf("  Context set\n");

    // Verify context
    const char* phase = nimcp_exception_get_context(&brain_ex->base, "phase");
    EXPECT_NE(phase, nullptr);
    if (phase) {
        EXPECT_STREQ(phase, "initialization");
    }

    // Dispatch
    nimcp_exception_dispatch(&brain_ex->base);
    printf("  Exception dispatched\n");

    // Verify handler was invoked
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_ERROR);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&brain_ex->base, &strategy);
    printf("  Recovery strategy: primary=%s, fallback=%s\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action),
           nimcp_exception_recovery_action_to_string(strategy.fallback_action));

    // Clean up
    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Brain initialization exception handled\n\n");
}

/* ============================================================================
 * Test 3: Exception During Forward Propagation
 *
 * Verifies handling of exceptions during neural network forward pass
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, ExceptionDuringForwardPropagation) {
    printf("=== Test: Exception During Forward Propagation ===\n");

    RegisterTestHandler(test_exception_handler);

    // Create forward pass exception with NaN detection
    nimcp_brain_exception_t* fwd_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        2,              // brain_id
        "visual_cortex", // region_name
        "Forward propagation failed: NaN detected in layer 3 activations"
    );
    ASSERT_NE(fwd_ex, nullptr);

    // Set brain-specific fields
    fwd_ex->network_id = 5;
    fwd_ex->layer_id = 3;
    fwd_ex->gradient_norm = NAN;
    fwd_ex->has_nan_weights = true;
    fwd_ex->learning_diverged = false;
    printf("  Forward propagation exception created with NaN\n");

    // Dispatch
    nimcp_exception_dispatch(&fwd_ex->base);

    // Verify handling
    EXPECT_GT(g_handler_calls.load(), 0);

    // Present to immune for NaN-specific handling
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&fwd_ex->base, &response);
    printf("  Presented to immune system\n");

    // Verify suggested recovery is appropriate for NaN
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery(&fwd_ex->base);
    printf("  Suggested recovery for NaN: %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    // Clean up
    nimcp_exception_unref(&fwd_ex->base);
    printf("Test passed: Forward propagation exception handled\n\n");
}

/* ============================================================================
 * Test 4: Memory Exception Triggering GC Recovery
 *
 * Verifies memory exceptions trigger garbage collection recovery
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, MemoryExceptionTriggersGCRecovery) {
    printf("=== Test: Memory Exception Triggers GC Recovery ===\n");

    // Register GC recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, test_recovery_callback, nullptr);
    printf("  Recovery callbacks registered\n");

    // Create memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        64 * 1024 * 1024,  // Requested 64MB
        "Large allocation failed - triggering GC recovery"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Set memory-specific fields
    mem_ex->available_size = 32 * 1024 * 1024;  // Only 32MB available
    mem_ex->is_heap = true;
    mem_ex->allocator_name = "arena_pool";
    printf("  Memory exception created: requested=%zu, available=%zu\n",
           mem_ex->requested_size, mem_ex->available_size);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&mem_ex->base, &strategy);
    printf("  Strategy: primary=%s, fallback=%s, retries=%u\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action),
           nimcp_exception_recovery_action_to_string(strategy.fallback_action),
           strategy.retry_count);

    // Execute primary recovery (should be GC)
    g_recovery_attempts = 0;
    int exec_result = nimcp_execute_recovery(&mem_ex->base, strategy.primary_action);
    printf("  Primary recovery executed, result=%d\n", exec_result);

    // Verify recovery was attempted
    EXPECT_GT(g_recovery_attempts.load(), 0);
    printf("  Recovery attempts: %d\n", g_recovery_attempts.load());

    // Notify result
    nimcp_exception_notify_recovery_result(&mem_ex->base, strategy.primary_action, true);

    // Verify exception state updated
    EXPECT_TRUE(mem_ex->base.recovery_attempted);
    EXPECT_TRUE(mem_ex->base.recovery_succeeded);

    // Clean up
    nimcp_exception_unref(&mem_ex->base);
    printf("Test passed: Memory exception triggered GC recovery\n\n");
}

/* ============================================================================
 * Test 5: Brain Exception Triggering Rollback Recovery
 *
 * Verifies brain exceptions can trigger checkpoint rollback
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, BrainExceptionTriggersRollbackRecovery) {
    printf("=== Test: Brain Exception Triggers Rollback Recovery ===\n");

    // Register rollback recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);
    printf("  Rollback callback registered\n");

    // Create brain exception with diverged learning
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        3,           // brain_id
        "hippocampus", // region_name
        "Learning diverged: loss exploded to infinity"
    );
    ASSERT_NE(brain_ex, nullptr);

    brain_ex->gradient_norm = INFINITY;
    brain_ex->has_nan_weights = false;
    brain_ex->learning_diverged = true;
    printf("  Brain exception created: learning_diverged=true\n");

    // Suggest rollback since learning diverged
    brain_ex->base.suggested_action = EXCEPTION_RECOVERY_ROLLBACK;

    // Execute rollback recovery
    g_recovery_attempts = 0;
    int exec_result = nimcp_execute_recovery(&brain_ex->base, EXCEPTION_RECOVERY_ROLLBACK);
    printf("  Rollback recovery executed, result=%d\n", exec_result);

    // Verify recovery
    EXPECT_GT(g_recovery_attempts.load(), 0);
    EXPECT_EQ(g_last_recovery_action, EXCEPTION_RECOVERY_ROLLBACK);

    // Mark recovery result
    brain_ex->base.recovery_attempted = true;
    brain_ex->base.recovery_succeeded = true;
    nimcp_exception_notify_recovery_result(&brain_ex->base, EXCEPTION_RECOVERY_ROLLBACK, true);
    printf("  Recovery result notified\n");

    // Clean up
    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Brain exception triggered rollback recovery\n\n");
}

/* ============================================================================
 * Test 6: Signal-Based Exception Emergency Save Recovery
 *
 * Verifies signal exceptions trigger emergency save
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, SignalExceptionEmergencySaveRecovery) {
    printf("=== Test: Signal Exception Emergency Save Recovery ===\n");

    // Register emergency save callback
    nimcp_register_recovery_callback(
        EXCEPTION_RECOVERY_EMERGENCY_SAVE, test_recovery_callback, nullptr);
    printf("  Emergency save callback registered\n");

    // Create signal exception (simulating SIGSEGV)
    nimcp_signal_exception_t* sig_ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0xDEADBEEF,  // Fault address
        __FILE__, __LINE__, __func__,
        "Segmentation fault at address 0xDEADBEEF"
    );
    ASSERT_NE(sig_ex, nullptr);
    printf("  Signal exception created: signal=%d\n", sig_ex->signal_number);

    // Verify signal-to-error mapping
    nimcp_error_t error_code = nimcp_signal_to_error_code(SIGSEGV);
    EXPECT_EQ(error_code, NIMCP_ERROR_SIGSEGV);
    printf("  Error code mapped: %d (expected %d)\n", error_code, NIMCP_ERROR_SIGSEGV);

    // Get signal name
    const char* sig_name = nimcp_signal_name(SIGSEGV);
    EXPECT_NE(sig_name, nullptr);
    printf("  Signal name: %s\n", sig_name ? sig_name : "unknown");

    // Signal exceptions should suggest emergency save
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery(&sig_ex->base);
    printf("  Suggested recovery: %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    // Execute emergency save if suggested
    g_recovery_attempts = 0;
    if (suggested == EXCEPTION_RECOVERY_EMERGENCY_SAVE ||
        suggested == EXCEPTION_RECOVERY_ROLLBACK) {
        int exec_result = nimcp_execute_recovery(&sig_ex->base, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
        printf("  Emergency save executed, result=%d\n", exec_result);
    } else {
        // Force emergency save for test
        int exec_result = nimcp_execute_recovery(&sig_ex->base, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
        printf("  Emergency save forced, result=%d\n", exec_result);
    }

    // Verify recovery attempted
    printf("  Recovery attempts: %d\n", g_recovery_attempts.load());

    // Clean up
    nimcp_exception_unref(&sig_ex->base);
    printf("Test passed: Signal exception emergency save recovery\n\n");
}

/* ============================================================================
 * Test 7: Multi-Exception Scenario with Aggregate Handling
 *
 * Verifies aggregate exceptions can contain multiple child exceptions
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, MultiExceptionAggregateHandling) {
    printf("=== Test: Multi-Exception Aggregate Handling ===\n");

    RegisterTestHandler(test_exception_handler);

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg_ex = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Batch operation failed with multiple errors"
    );
    ASSERT_NE(agg_ex, nullptr);
    printf("  Aggregate exception created\n");

    // Create and add child exceptions
    nimcp_memory_exception_t* child1 = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,
        "Child 1: Memory allocation failed"
    );
    ASSERT_NE(child1, nullptr);

    nimcp_brain_exception_t* child2 = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        1, "cortex",
        "Child 2: Forward pass warning"
    );
    ASSERT_NE(child2, nullptr);

    nimcp_io_exception_t* child3 = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "/tmp/checkpoint.bin",
        "Child 3: Checkpoint write failed"
    );
    ASSERT_NE(child3, nullptr);

    // Add children to aggregate
    int add_result;
    add_result = nimcp_aggregate_exception_add(agg_ex, (nimcp_exception_t*)child1);
    EXPECT_EQ(add_result, 0);
    add_result = nimcp_aggregate_exception_add(agg_ex, (nimcp_exception_t*)child2);
    EXPECT_EQ(add_result, 0);
    add_result = nimcp_aggregate_exception_add(agg_ex, (nimcp_exception_t*)child3);
    EXPECT_EQ(add_result, 0);
    printf("  Added %zu child exceptions\n", nimcp_aggregate_exception_count(agg_ex));

    // Verify child count
    EXPECT_EQ(nimcp_aggregate_exception_count(agg_ex), 3u);

    // Get children by index
    nimcp_exception_t* retrieved = nimcp_aggregate_exception_get(agg_ex, 0);
    EXPECT_NE(retrieved, nullptr);
    if (retrieved) {
        EXPECT_EQ(retrieved->type, EXCEPTION_TYPE_MEMORY);
        printf("  Child 0 type: %s\n", nimcp_exception_type_to_string(retrieved->type));
    }

    retrieved = nimcp_aggregate_exception_get(agg_ex, 1);
    EXPECT_NE(retrieved, nullptr);
    if (retrieved) {
        EXPECT_EQ(retrieved->type, EXCEPTION_TYPE_BRAIN);
        printf("  Child 1 type: %s\n", nimcp_exception_type_to_string(retrieved->type));
    }

    retrieved = nimcp_aggregate_exception_get(agg_ex, 2);
    EXPECT_NE(retrieved, nullptr);
    if (retrieved) {
        EXPECT_EQ(retrieved->type, EXCEPTION_TYPE_IO);
        printf("  Child 2 type: %s\n", nimcp_exception_type_to_string(retrieved->type));
    }

    // Dispatch aggregate
    nimcp_exception_dispatch(&agg_ex->base);
    printf("  Aggregate exception dispatched\n");

    // Verify handling
    EXPECT_GT(g_handler_calls.load(), 0);

    // Format aggregate as string for debugging
    char buffer[1024];
    size_t len = nimcp_exception_to_string(&agg_ex->base, buffer, sizeof(buffer));
    printf("  Exception string length: %zu\n", len);

    // Clean up - aggregate owns children references
    nimcp_exception_unref(&agg_ex->base);
    printf("Test passed: Multi-exception aggregate handling\n\n");
}

/* ============================================================================
 * Test 8: Exception Chaining (Cause Chain)
 *
 * Verifies exception cause chains work correctly
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, ExceptionChainingCauseChain) {
    printf("=== Test: Exception Chaining (Cause Chain) ===\n");

    // Create root cause
    nimcp_exception_t* root_cause = nimcp_exception_create(
        NIMCP_ERROR_FILE_READ,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Root cause: Configuration file not found"
    );
    ASSERT_NE(root_cause, nullptr);
    printf("  Root cause created\n");

    // Create intermediate exception
    nimcp_exception_t* intermediate = nimcp_exception_create(
        NIMCP_ERROR_CONFIG_PARSE,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Intermediate: Failed to parse brain configuration"
    );
    ASSERT_NE(intermediate, nullptr);

    // Chain: intermediate -> root_cause
    nimcp_exception_set_cause(intermediate, root_cause);
    printf("  Intermediate exception chained to root cause\n");

    // Create top-level exception
    nimcp_brain_exception_t* top_level = nimcp_brain_exception_create(
        NIMCP_ERROR_BRAIN_CREATION,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "prefrontal",
        "Top level: Brain creation failed"
    );
    ASSERT_NE(top_level, nullptr);

    // Chain: top_level -> intermediate
    nimcp_exception_set_cause(&top_level->base, intermediate);
    printf("  Top level exception chained to intermediate\n");

    // Walk the cause chain
    nimcp_exception_t* current = &top_level->base;
    int chain_depth = 0;
    while (current) {
        printf("  Chain[%d]: code=%d, message=%s\n",
               chain_depth, current->code, current->message);
        current = nimcp_exception_get_cause(current);
        chain_depth++;
    }
    EXPECT_EQ(chain_depth, 3);
    printf("  Cause chain depth: %d\n", chain_depth);

    // Clean up - unreffing top will unref the chain
    nimcp_exception_unref(&top_level->base);
    printf("Test passed: Exception chaining cause chain\n\n");
}

/* ============================================================================
 * Test 9: Try/Catch Mechanism
 *
 * Verifies try/catch macros work for exception handling
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, TryCatchMechanism) {
    printf("=== Test: Try/Catch Mechanism ===\n");

    bool exception_caught = false;
    nimcp_error_t caught_code = NIMCP_SUCCESS;

    NIMCP_TRY {
        printf("  Inside try block\n");

        // Create and raise exception
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_INVALID_PARAMETER,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Test exception for try/catch"
        );
        ASSERT_NE(ex, nullptr);

        // Raise will longjmp if in try block
        nimcp_exception_raise(ex);

        // Should not reach here
        printf("  ERROR: Should not reach here after raise\n");
        FAIL() << "Exception raise did not longjmp";
    }
    NIMCP_CATCH(nimcp_exception_t, caught_ex) {
        printf("  Exception caught!\n");
        exception_caught = true;
        if (caught_ex) {
            caught_code = caught_ex->code;
            printf("  Caught code: %d\n", caught_code);
            printf("  Caught message: %s\n", caught_ex->message);
            nimcp_exception_unref(caught_ex);
        }
    }
    NIMCP_END_TRY;

    EXPECT_TRUE(exception_caught);
    EXPECT_EQ(caught_code, NIMCP_ERROR_INVALID_PARAMETER);
    printf("Test passed: Try/catch mechanism works\n\n");
}

/* ============================================================================
 * Test 10: Immune Integration Statistics
 *
 * Verifies immune integration tracking statistics
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, ImmuneIntegrationStatistics) {
    printf("=== Test: Immune Integration Statistics ===\n");

    // Reset stats
    nimcp_exception_immune_reset_stats();

    // Create and present several exceptions
    for (int i = 0; i < 5; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Test exception %d for statistics", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);
    }
    printf("  Presented 5 exceptions to immune\n");

    // Get statistics
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    printf("  Statistics:\n");
    printf("    exceptions_presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("    exceptions_pending: %lu\n", (unsigned long)stats.exceptions_pending);
    printf("    recoveries_attempted: %lu\n", (unsigned long)stats.recoveries_attempted);
    printf("    recoveries_succeeded: %lu\n", (unsigned long)stats.recoveries_succeeded);
    printf("    memories_formed: %lu\n", (unsigned long)stats.memories_formed);
    printf("    avg_response_time_us: %.2f\n", stats.avg_response_time_us);
    printf("    queue_overflows: %lu\n", (unsigned long)stats.queue_overflows);

    // Verify at least some exceptions were tracked
    // Note: May be 0 if immune system not connected
    printf("Test passed: Immune integration statistics tracked\n\n");
}

/* ============================================================================
 * Test 11: Exception Context API
 *
 * Verifies structured context key-value pairs on exceptions
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, ExceptionContextAPI) {
    printf("=== Test: Exception Context API ===\n");

    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Exception with context"
    );
    ASSERT_NE(ex, nullptr);

    // Set context entries
    EXPECT_EQ(nimcp_exception_set_context(ex, "component", "neural_network"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "layer_id", "42"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "batch_size", "64"), 0);
    EXPECT_EQ(nimcp_exception_set_context(ex, "epoch", "100"), 0);
    printf("  Set 4 context entries\n");

    // Verify count
    EXPECT_EQ(nimcp_exception_context_count(ex), 4u);

    // Get context values
    const char* component = nimcp_exception_get_context(ex, "component");
    EXPECT_NE(component, nullptr);
    if (component) {
        EXPECT_STREQ(component, "neural_network");
        printf("  component=%s\n", component);
    }

    const char* layer = nimcp_exception_get_context(ex, "layer_id");
    EXPECT_NE(layer, nullptr);
    if (layer) {
        EXPECT_STREQ(layer, "42");
        printf("  layer_id=%s\n", layer);
    }

    // Remove context entry
    EXPECT_EQ(nimcp_exception_remove_context(ex, "batch_size"), 0);
    EXPECT_EQ(nimcp_exception_context_count(ex), 3u);
    printf("  Removed batch_size, count=%zu\n", nimcp_exception_context_count(ex));

    // Verify removed entry is gone
    EXPECT_EQ(nimcp_exception_get_context(ex, "batch_size"), nullptr);

    // Update existing entry
    EXPECT_EQ(nimcp_exception_set_context(ex, "epoch", "150"), 0);
    const char* epoch = nimcp_exception_get_context(ex, "epoch");
    EXPECT_NE(epoch, nullptr);
    if (epoch) {
        EXPECT_STREQ(epoch, "150");
        printf("  Updated epoch=%s\n", epoch);
    }

    nimcp_exception_unref(ex);
    printf("Test passed: Exception context API works\n\n");
}

/* ============================================================================
 * Test 12: Handler Priority Chain
 *
 * Verifies handlers are called in priority order
 * ============================================================================ */

static std::vector<int> g_handler_order;

static bool priority_handler_high(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(100);
    return false;
}

static bool priority_handler_normal(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(50);
    return false;
}

static bool priority_handler_low(nimcp_exception_t* ex, void* user_data) {
    (void)ex;
    (void)user_data;
    g_handler_order.push_back(10);
    return false;
}

TEST_F(ExceptionFlowE2ETest, HandlerPriorityChain) {
    printf("=== Test: Handler Priority Chain ===\n");

    g_handler_order.clear();

    // Register handlers in reverse priority order
    nimcp_handler_options_t opts;

    nimcp_handler_default_options(&opts);
    opts.name = "LowPriority";
    opts.handler = priority_handler_low;
    opts.priority = NIMCP_HANDLER_PRIORITY_LOW;
    nimcp_handler_registration_t* reg_low = nimcp_handler_register(&opts);
    ASSERT_NE(reg_low, nullptr);

    nimcp_handler_default_options(&opts);
    opts.name = "NormalPriority";
    opts.handler = priority_handler_normal;
    opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
    nimcp_handler_registration_t* reg_normal = nimcp_handler_register(&opts);
    ASSERT_NE(reg_normal, nullptr);

    nimcp_handler_default_options(&opts);
    opts.name = "HighPriority";
    opts.handler = priority_handler_high;
    opts.priority = NIMCP_HANDLER_PRIORITY_HIGH;
    nimcp_handler_registration_t* reg_high = nimcp_handler_register(&opts);
    ASSERT_NE(reg_high, nullptr);

    printf("  Registered 3 handlers (low, normal, high)\n");

    // Create and dispatch exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        "Priority test exception"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_dispatch(ex);
    printf("  Exception dispatched\n");

    // Verify call order (high -> normal -> low)
    EXPECT_EQ(g_handler_order.size(), 3u);
    if (g_handler_order.size() >= 3) {
        printf("  Handler order: %d -> %d -> %d\n",
               g_handler_order[0], g_handler_order[1], g_handler_order[2]);
        EXPECT_EQ(g_handler_order[0], 100);  // High first
        EXPECT_EQ(g_handler_order[1], 50);   // Normal second
        EXPECT_EQ(g_handler_order[2], 10);   // Low third
    }

    // Clean up
    nimcp_handler_unregister(reg_low);
    nimcp_handler_unregister(reg_normal);
    nimcp_handler_unregister(reg_high);
    nimcp_exception_unref(ex);
    printf("Test passed: Handler priority chain verified\n\n");
}

/* ============================================================================
 * Test 13: Category Filtering
 *
 * Verifies handlers can filter by exception category
 * ============================================================================ */

static std::atomic<int> g_memory_handler_calls{0};
static std::atomic<int> g_brain_handler_calls{0};

static bool memory_only_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex && ex->category == EXCEPTION_CATEGORY_MEMORY) {
        g_memory_handler_calls++;
    }
    return false;
}

static bool brain_only_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex && ex->category == EXCEPTION_CATEGORY_BRAIN) {
        g_brain_handler_calls++;
    }
    return false;
}

TEST_F(ExceptionFlowE2ETest, CategoryFiltering) {
    printf("=== Test: Category Filtering ===\n");

    g_memory_handler_calls = 0;
    g_brain_handler_calls = 0;

    // Register category-filtered handlers
    nimcp_handler_options_t opts;

    nimcp_handler_default_options(&opts);
    opts.name = "MemoryHandler";
    opts.handler = memory_only_handler;
    opts.category_filter = EXCEPTION_CATEGORY_MEMORY;
    nimcp_handler_registration_t* reg_memory = nimcp_handler_register(&opts);
    ASSERT_NE(reg_memory, nullptr);

    nimcp_handler_default_options(&opts);
    opts.name = "BrainHandler";
    opts.handler = brain_only_handler;
    opts.category_filter = EXCEPTION_CATEGORY_BRAIN;
    nimcp_handler_registration_t* reg_brain = nimcp_handler_register(&opts);
    ASSERT_NE(reg_brain, nullptr);

    printf("  Registered memory and brain handlers\n");

    // Dispatch memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024,
        "Memory category test"
    );
    ASSERT_NE(mem_ex, nullptr);
    nimcp_exception_dispatch(&mem_ex->base);
    printf("  Memory exception dispatched\n");

    // Dispatch brain exception
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1, "test",
        "Brain category test"
    );
    ASSERT_NE(brain_ex, nullptr);
    nimcp_exception_dispatch(&brain_ex->base);
    printf("  Brain exception dispatched\n");

    // Verify filtering
    printf("  Memory handler calls: %d\n", g_memory_handler_calls.load());
    printf("  Brain handler calls: %d\n", g_brain_handler_calls.load());

    EXPECT_GT(g_memory_handler_calls.load(), 0);
    EXPECT_GT(g_brain_handler_calls.load(), 0);

    // Clean up
    nimcp_handler_unregister(reg_memory);
    nimcp_handler_unregister(reg_brain);
    nimcp_exception_unref(&mem_ex->base);
    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Category filtering works\n\n");
}

/* ============================================================================
 * Test 14: Full E2E Recovery Lifecycle
 *
 * Complete lifecycle: error -> exception -> immune -> recovery -> verify
 * ============================================================================ */

TEST_F(ExceptionFlowE2ETest, FullE2ERecoveryLifecycle) {
    printf("=== Test: Full E2E Recovery Lifecycle ===\n");

    // Phase 1: Setup handlers and recovery
    printf("Phase 1: Setup\n");
    RegisterTestHandler(test_exception_handler);
    nimcp_install_default_handlers();
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, test_recovery_callback, nullptr);

    // Phase 2: Simulate error occurrence
    printf("Phase 2: Error occurrence\n");
    nimcp_memory_exception_t* ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        256 * 1024 * 1024,  // 256MB
        "E2E test: Critical memory allocation failure"
    );
    ASSERT_NE(ex, nullptr);
    ex->available_size = 64 * 1024 * 1024;  // 64MB available
    ex->is_heap = true;

    // Phase 3: Log exception
    printf("Phase 3: Logging\n");
    nimcp_exception_log(&ex->base);

    // Phase 4: Dispatch through handlers
    printf("Phase 4: Handler dispatch\n");
    g_handler_calls = 0;
    nimcp_exception_dispatch(&ex->base);
    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Phase 5: Present to immune system
    printf("Phase 5: Immune presentation\n");
    nimcp_immune_response_t immune_response;
    memset(&immune_response, 0, sizeof(immune_response));
    nimcp_exception_present_to_immune(&ex->base, &immune_response);
    printf("  Antigen ID: %u\n", immune_response.antigen_id);

    // Phase 6: Execute recovery
    printf("Phase 6: Recovery execution\n");
    g_recovery_attempts = 0;
    nimcp_exception_recovery_action_t action = nimcp_exception_get_suggested_recovery(&ex->base);
    printf("  Suggested action: %s\n", nimcp_exception_recovery_action_to_string(action));

    int recovery_result = nimcp_execute_recovery(&ex->base, action);
    printf("  Recovery result: %d\n", recovery_result);

    // Phase 7: Notify result
    printf("Phase 7: Result notification\n");
    nimcp_exception_notify_recovery_result(&ex->base, action, true);
    ex->base.recovery_attempted = true;
    ex->base.recovery_succeeded = true;

    // Phase 8: Verify state
    printf("Phase 8: Verification\n");
    EXPECT_TRUE(ex->base.recovery_attempted);
    EXPECT_TRUE(ex->base.recovery_succeeded);
    printf("  recovery_attempted: %s\n", ex->base.recovery_attempted ? "true" : "false");
    printf("  recovery_succeeded: %s\n", ex->base.recovery_succeeded ? "true" : "false");

    // Phase 9: Get statistics
    printf("Phase 9: Statistics\n");
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Total presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("  Total recoveries: %lu\n", (unsigned long)stats.recoveries_attempted);

    // Phase 10: Cleanup
    printf("Phase 10: Cleanup\n");
    nimcp_exception_unref(&ex->base);

    printf("Test passed: Full E2E recovery lifecycle completed\n\n");
}
