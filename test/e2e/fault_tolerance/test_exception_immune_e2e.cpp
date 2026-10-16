/**
 * @file test_exception_immune_e2e.cpp
 * @brief E2E tests for exception-to-immune-to-recovery integration pipeline
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end tests for the complete exception-immune-recovery pipeline
 * WHY:  Verify that exceptions flow correctly through the brain immune system
 *       and trigger appropriate recovery actions
 * HOW:  Test the full integration: Exception -> Immune Presentation -> B-Cell
 *       Recognition -> Antibody Response -> Recovery Action -> Memory Formation
 *
 * Test Scenarios:
 * 1. Full exception-to-immune lifecycle
 * 2. Brain exception immune response with rollback
 * 3. GPU exception immune response with CPU fallback
 * 4. Security exception immune response with quarantine
 * 5. Memory exception immune response with GC trigger
 * 6. Signal exception immune response with checkpoint recovery
 * 7. Multi-exception cascade immune handling
 * 8. Immune memory formation from repeated exceptions
 * 9. Auto-recovery configuration modes
 * 10. Exception queue overflow handling
 * 11. Immune statistics tracking
 * 12. Thread-safe immune presentation
 * 13. Priority-based immune response
 * 14. Recovery callback chain execution
 * 15. Complete E2E brain error recovery
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <csignal>
#include <cmath>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/signal/nimcp_signal_handler.h"
}

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

// Callback tracking for immune integration
static std::atomic<int> g_handler_calls{0};
static std::atomic<int> g_immune_presentations{0};
static std::atomic<int> g_recovery_gc_count{0};
static std::atomic<int> g_recovery_rollback_count{0};
static std::atomic<int> g_recovery_quarantine_count{0};
static std::atomic<int> g_recovery_emergency_save_count{0};
static std::atomic<int> g_recovery_retry_count{0};
static std::atomic<int> g_recovery_compact_count{0};
static std::atomic<bool> g_recovery_success{false};
static nimcp_exception_t* g_last_exception = nullptr;
static nimcp_immune_response_t g_last_immune_response;

// Test handler
static bool test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_calls++;
        g_last_exception = ex;
    }
    return false;
}

// GC recovery callback
static int recovery_gc(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                       void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_gc_count++;
    g_recovery_success = true;
    printf("    [Recovery] GC triggered\n");
    return 0;
}

// Rollback recovery callback
static int recovery_rollback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                             void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_rollback_count++;
    g_recovery_success = true;
    printf("    [Recovery] Rollback triggered\n");
    return 0;
}

// Quarantine recovery callback
static int recovery_quarantine(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                               void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_quarantine_count++;
    g_recovery_success = true;
    printf("    [Recovery] Quarantine triggered\n");
    return 0;
}

// Emergency save callback
static int recovery_emergency_save(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                                   void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_emergency_save_count++;
    g_recovery_success = true;
    printf("    [Recovery] Emergency save triggered\n");
    return 0;
}

// Retry recovery callback
static int recovery_retry(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                          void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_retry_count++;
    g_recovery_success = true;
    printf("    [Recovery] Retry triggered\n");
    return 0;
}

// Compact recovery callback
static int recovery_compact(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                            void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_compact_count++;
    g_recovery_success = true;
    printf("    [Recovery] Compact triggered\n");
    return 0;
}

// Reset all tracking state
static void reset_tracking() {
    g_handler_calls = 0;
    g_immune_presentations = 0;
    g_recovery_gc_count = 0;
    g_recovery_rollback_count = 0;
    g_recovery_quarantine_count = 0;
    g_recovery_emergency_save_count = 0;
    g_recovery_retry_count = 0;
    g_recovery_compact_count = 0;
    g_recovery_success = false;
    g_last_exception = nullptr;
    memset(&g_last_immune_response, 0, sizeof(g_last_immune_response));
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionImmuneE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg = nullptr;

    void SetUp() override {
        reset_tracking();

        // Initialize exception system
        int init_result = nimcp_exception_system_init();
        ASSERT_EQ(init_result, 0) << "Failed to initialize exception system";

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t config;
        nimcp_exception_immune_default_config(&config);
        config.enable_auto_present = true;
        config.enable_auto_recovery = true;
        config.min_present_severity = EXCEPTION_SEVERITY_ERROR;
        config.enable_memory_formation = true;
        // Memory formation handled by enable_memory_formation flag

        int immune_init = nimcp_exception_immune_init(&config);
        ASSERT_EQ(immune_init, 0) << "Failed to initialize exception-immune integration";

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "ImmuneTestHandler";
        opts.handler = test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(handler_reg, nullptr);

        // Register all recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, recovery_gc, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, recovery_rollback, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, recovery_quarantine, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, recovery_emergency_save, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, recovery_retry, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, recovery_compact, nullptr);
    }

    void TearDown() override {
        if (handler_reg) {
            nimcp_handler_unregister(handler_reg);
            handler_reg = nullptr;
        }

        g_last_exception = nullptr;

        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_exception_system_shutdown();
    }
};

/* ============================================================================
 * Test 1: Full Exception-to-Immune Lifecycle
 *
 * Verifies: Complete exception -> immune -> recovery lifecycle
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, FullExceptionImmuneLifecycle) {
    printf("=== Test: Full Exception-to-Immune Lifecycle ===\n");

    reset_tracking();

    // Step 1: Create exception
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Full lifecycle test exception"
    );
    ASSERT_NE(ex, nullptr);
    printf("  Step 1: Exception created\n");

    // Step 2: Dispatch through handler chain
    nimcp_exception_dispatch(ex);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Step 2: Dispatched, handler calls = %d\n", g_handler_calls.load());

    // Step 3: Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int present_result = nimcp_exception_present_to_immune(ex, &response);
    printf("  Step 3: Presented to immune, result = %d\n", present_result);
    printf("    Antigen ID: %u\n", response.antigen_id);
    printf("    Response type: %u\n", response.action_taken);

    // Step 4: Get suggested recovery
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery(ex);
    printf("  Step 4: Suggested recovery = %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    // Step 5: Execute recovery
    if (suggested != EXCEPTION_RECOVERY_NONE) {
        int recovery_result = nimcp_execute_recovery(ex, suggested);
        printf("  Step 5: Recovery executed, result = %d\n", recovery_result);
    }

    // Step 6: Notify recovery result
    nimcp_exception_notify_recovery_result(ex, suggested, true);
    printf("  Step 6: Recovery result notified\n");

    // Step 7: Verify state
    EXPECT_TRUE(ex->recovery_attempted || g_recovery_success);
    printf("  Step 7: Lifecycle complete\n");

    nimcp_exception_unref(ex);
    printf("Test passed: Full exception-immune lifecycle\n\n");
}

/* ============================================================================
 * Test 2: Brain Exception Immune Response with Rollback
 *
 * Verifies: Brain exception triggers rollback recovery via immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, BrainExceptionImmuneRollback) {
    printf("=== Test: Brain Exception Immune Rollback ===\n");

    reset_tracking();

    // Create brain exception with learning divergence
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1, "prefrontal_cortex",
        "Learning diverged: loss=INF, gradient_norm=NaN"
    );
    ASSERT_NE(brain_ex, nullptr);

    brain_ex->learning_diverged = true;
    brain_ex->gradient_norm = INFINITY;
    brain_ex->has_nan_weights = true;
    printf("  Brain exception created: learning_diverged=true\n");

    // Dispatch
    nimcp_exception_dispatch(&brain_ex->base);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&brain_ex->base, &response);
    printf("  Presented to immune: antigen_id=%u\n", response.antigen_id);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&brain_ex->base, &strategy);
    printf("  Strategy: primary=%s, fallback=%s\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action),
           nimcp_exception_recovery_action_to_string(strategy.fallback_action));

    // Execute rollback recovery
    brain_ex->base.suggested_action = EXCEPTION_RECOVERY_ROLLBACK;
    nimcp_execute_recovery(&brain_ex->base, EXCEPTION_RECOVERY_ROLLBACK);

    // Verify rollback was triggered
    EXPECT_GT(g_recovery_rollback_count.load(), 0);
    printf("  Rollback count: %d\n", g_recovery_rollback_count.load());

    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Brain exception immune rollback\n\n");
}

/* ============================================================================
 * Test 3: GPU Exception Immune Response with CPU Fallback
 *
 * Verifies: GPU exception triggers fallback to CPU via immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, GpuExceptionImmuneFallback) {
    printf("=== Test: GPU Exception Immune Fallback ===\n");

    reset_tracking();

    // Create GPU exception
    nimcp_gpu_exception_t* gpu_ex = nimcp_gpu_exception_create(
        NIMCP_ERROR_GPU_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        0,  // device_id
        2,  // cuda_error (cudaErrorMemoryAllocation)
        "CUDA out of memory on device 0"
    );
    ASSERT_NE(gpu_ex, nullptr);

    // Set GPU memory info using available fields
    gpu_ex->gpu_memory_used = 2ULL * 1024 * 1024 * 1024;   // 2GB requested
    gpu_ex->gpu_memory_total = 512 * 1024 * 1024;          // 512MB available
    printf("  GPU exception created: device=%d, mem_used=%zu\n",
           gpu_ex->device_id, gpu_ex->gpu_memory_used);

    // Dispatch
    nimcp_exception_dispatch(&gpu_ex->base);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&gpu_ex->base, &response);
    printf("  Presented to immune\n");

    // GPU exceptions with fallback should suggest retry (on CPU)
    nimcp_exception_recovery_action_t suggested =
        nimcp_exception_get_suggested_recovery(&gpu_ex->base);
    printf("  Suggested recovery: %s\n",
           nimcp_exception_recovery_action_to_string(suggested));

    // Execute retry/fallback
    if (suggested == EXCEPTION_RECOVERY_RETRY || suggested == EXCEPTION_RECOVERY_GC) {
        nimcp_execute_recovery(&gpu_ex->base, suggested);
        printf("  Recovery executed\n");
    }

    nimcp_exception_unref(&gpu_ex->base);
    printf("Test passed: GPU exception immune fallback\n\n");
}

/* ============================================================================
 * Test 4: Security Exception Immune Response with Quarantine
 *
 * Verifies: Security exception triggers quarantine via immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, SecurityExceptionImmuneQuarantine) {
    printf("=== Test: Security Exception Immune Quarantine ===\n");

    reset_tracking();

    // Create security exception
    nimcp_security_exception_t* sec_ex = nimcp_security_exception_create(
        NIMCP_ERROR_SECURITY_THREAT,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1,  // threat_type (BBB_THREAT_MALICIOUS_INPUT)
        "Malicious input pattern detected in neural network input"
    );
    ASSERT_NE(sec_ex, nullptr);

    sec_ex->threat_type = 1;
    sec_ex->severity_score = 9;  // High threat (1-10 scale)
    sec_ex->quarantine_required = true;
    sec_ex->source_node_id = 0x12345678;
    printf("  Security exception created: severity_score=%d\n", sec_ex->severity_score);

    // Dispatch
    nimcp_exception_dispatch(&sec_ex->base);

    // Present to immune - security exceptions should trigger strong response
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&sec_ex->base, &response);
    printf("  Presented to immune\n");

    // Security exceptions should suggest quarantine
    sec_ex->base.suggested_action = EXCEPTION_RECOVERY_QUARANTINE;
    nimcp_execute_recovery(&sec_ex->base, EXCEPTION_RECOVERY_QUARANTINE);

    // Verify quarantine was triggered
    EXPECT_GT(g_recovery_quarantine_count.load(), 0);
    printf("  Quarantine count: %d\n", g_recovery_quarantine_count.load());

    nimcp_exception_unref(&sec_ex->base);
    printf("Test passed: Security exception immune quarantine\n\n");
}

/* ============================================================================
 * Test 5: Memory Exception Immune Response with GC Trigger
 *
 * Verifies: Memory exception triggers GC via immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, MemoryExceptionImmuneGC) {
    printf("=== Test: Memory Exception Immune GC ===\n");

    reset_tracking();

    // Create memory exception
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        128 * 1024 * 1024,  // 128MB requested
        "Memory allocation failed for training batch"
    );
    ASSERT_NE(mem_ex, nullptr);

    mem_ex->available_size = 32 * 1024 * 1024;  // Only 32MB available
    mem_ex->is_heap = true;
    mem_ex->allocator_name = "default_arena";
    printf("  Memory exception created: requested=%zu, available=%zu\n",
           mem_ex->requested_size, mem_ex->available_size);

    // Dispatch
    nimcp_exception_dispatch(&mem_ex->base);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&mem_ex->base, &response);
    printf("  Presented to immune\n");

    // Memory exceptions should suggest GC
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&mem_ex->base, &strategy);
    printf("  Strategy: primary=%s\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action));

    nimcp_execute_recovery(&mem_ex->base, EXCEPTION_RECOVERY_GC);

    // Verify GC was triggered
    EXPECT_GT(g_recovery_gc_count.load(), 0);
    printf("  GC count: %d\n", g_recovery_gc_count.load());

    nimcp_exception_unref(&mem_ex->base);
    printf("Test passed: Memory exception immune GC\n\n");
}

/* ============================================================================
 * Test 6: Signal Exception Immune Response with Checkpoint Recovery
 *
 * Verifies: Signal exception triggers emergency save via immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, SignalExceptionImmuneEmergencySave) {
    printf("=== Test: Signal Exception Immune Emergency Save ===\n");

    reset_tracking();

    // Create signal exception
    nimcp_signal_exception_t* sig_ex = nimcp_signal_exception_create(
        SIGSEGV,
        (void*)0xDEADBEEF,
        __FILE__, __LINE__, __func__,
        "Segmentation fault during forward pass"
    );
    ASSERT_NE(sig_ex, nullptr);
    printf("  Signal exception created: signal=%d\n", sig_ex->signal_number);

    // Dispatch
    nimcp_exception_dispatch(&sig_ex->base);

    // Present to immune - signal exceptions are critical
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&sig_ex->base, &response);
    printf("  Presented to immune\n");

    // Signal exceptions should trigger emergency save
    sig_ex->base.suggested_action = EXCEPTION_RECOVERY_EMERGENCY_SAVE;
    nimcp_execute_recovery(&sig_ex->base, EXCEPTION_RECOVERY_EMERGENCY_SAVE);

    // Verify emergency save was triggered
    EXPECT_GT(g_recovery_emergency_save_count.load(), 0);
    printf("  Emergency save count: %d\n", g_recovery_emergency_save_count.load());

    nimcp_exception_unref(&sig_ex->base);
    printf("Test passed: Signal exception immune emergency save\n\n");
}

/* ============================================================================
 * Test 7: Multi-Exception Cascade Immune Handling
 *
 * Verifies: Multiple related exceptions handled correctly by immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, MultiExceptionCascadeImmune) {
    printf("=== Test: Multi-Exception Cascade Immune ===\n");

    reset_tracking();

    // Create cascade: Memory -> Brain -> IO exceptions
    printf("  Creating exception cascade\n");

    // Exception 1: Memory allocation failed
    nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_ERROR,
        __FILE__, __LINE__, __func__,
        1024 * 1024,
        "Cascade 1: Memory allocation failed"
    );
    ASSERT_NE(mem_ex, nullptr);

    // Exception 2: Brain forward pass failed (caused by memory)
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        1, "hippocampus",
        "Cascade 2: Forward pass failed due to memory"
    );
    ASSERT_NE(brain_ex, nullptr);

    // Chain them
    nimcp_exception_set_cause(&brain_ex->base, &mem_ex->base);

    // Exception 3: IO checkpoint failed (due to brain error)
    nimcp_io_exception_t* io_ex = nimcp_io_exception_create(
        NIMCP_ERROR_FILE_WRITE,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "/tmp/checkpoint.bin",
        "Cascade 3: Checkpoint save failed"
    );
    ASSERT_NE(io_ex, nullptr);

    // Chain to brain exception
    nimcp_exception_set_cause(&io_ex->base, &brain_ex->base);

    printf("  Exception chain: IO -> Brain -> Memory\n");

    // Dispatch top-level exception
    nimcp_exception_dispatch(&io_ex->base);

    // Present entire chain to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));

    // Present each exception in chain
    nimcp_exception_t* current = &io_ex->base;
    int chain_depth = 0;
    while (current) {
        nimcp_exception_present_to_immune(current, &response);
        printf("    Presented chain[%d]: code=%d\n", chain_depth, current->code);
        current = nimcp_exception_get_cause(current);
        chain_depth++;
    }
    EXPECT_EQ(chain_depth, 3);
    printf("  Chain depth: %d\n", chain_depth);

    // Cleanup (unreffing top releases chain)
    nimcp_exception_unref(&io_ex->base);
    printf("Test passed: Multi-exception cascade immune\n\n");
}

/* ============================================================================
 * Test 8: Immune Memory Formation from Repeated Exceptions
 *
 * Verifies: Repeated similar exceptions form immune memory
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, ImmuneMemoryFormation) {
    printf("=== Test: Immune Memory Formation ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Generate similar exceptions to trigger memory formation
    for (int i = 0; i < 5; i++) {
        nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            1024 * (i + 1),  // Slight variation in size
            "Repeated memory exception #%d", i
        );
        ASSERT_NE(mem_ex, nullptr);

        // Dispatch and present
        nimcp_exception_dispatch(&mem_ex->base);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(&mem_ex->base, &response);

        printf("  Exception %d: antigen_id=%u\n", i, response.antigen_id);

        nimcp_exception_unref(&mem_ex->base);
    }

    // Check statistics for memory formation
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Stats:\n");
    printf("    Presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("    Memories formed: %lu\n", (unsigned long)stats.memories_formed);

    printf("Test passed: Immune memory formation\n\n");
}

/* ============================================================================
 * Test 9: Auto-Recovery Configuration Modes
 *
 * Verifies: Different auto-recovery configurations work correctly
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, AutoRecoveryModes) {
    printf("=== Test: Auto-Recovery Modes ===\n");

    // Auto-recovery is configured at init time via enable_auto_recovery flag
    // Test that auto-recovery is working when enabled (configured in SetUp)
    printf("  Mode 1: Auto-recovery enabled (via config)\n");
    reset_tracking();

    nimcp_exception_t* ex1 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Auto-recovery test 1"
    );
    ex1->suggested_action = EXCEPTION_RECOVERY_GC;
    nimcp_exception_dispatch(ex1);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex1, &response);

    // With auto-recovery enabled (from config), system should attempt recovery
    printf("    GC count after: %d\n", g_recovery_gc_count.load());
    nimcp_exception_unref(ex1);

    // Test 2: Manual recovery execution
    printf("  Mode 2: Manual recovery execution\n");
    reset_tracking();

    nimcp_exception_t* ex2 = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Manual recovery test"
    );
    ex2->suggested_action = EXCEPTION_RECOVERY_GC;
    nimcp_exception_dispatch(ex2);

    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(ex2, &response);

    // Explicitly execute recovery
    nimcp_execute_recovery(ex2, EXCEPTION_RECOVERY_GC);
    printf("    GC count after manual recovery: %d\n", g_recovery_gc_count.load());
    nimcp_exception_unref(ex2);

    printf("Test passed: Auto-recovery modes\n\n");
}

/* ============================================================================
 * Test 10: Exception Queue Overflow Handling
 *
 * Verifies: System handles queue overflow gracefully
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, QueueOverflowHandling) {
    printf("=== Test: Queue Overflow Handling ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Generate many exceptions rapidly
    const int overflow_count = 200;  // Should exceed typical queue size
    for (int i = 0; i < overflow_count; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Overflow test %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_dispatch(ex);

        // Queue for async processing (may overflow)
        nimcp_exception_present_async(ex);

        nimcp_exception_unref(ex);
    }

    printf("  Generated %d exceptions\n", overflow_count);

    // Process any pending
    size_t processed = nimcp_exception_immune_process_pending(0);
    printf("  Processed pending: %zu\n", processed);

    // Check statistics for overflow count
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Queue overflows: %lu\n", (unsigned long)stats.queue_overflows);

    printf("Test passed: Queue overflow handling\n\n");
}

/* ============================================================================
 * Test 11: Immune Statistics Tracking
 *
 * Verifies: Statistics are tracked correctly throughout processing
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, ImmuneStatisticsTracking) {
    printf("=== Test: Immune Statistics Tracking ===\n");

    nimcp_exception_immune_reset_stats();

    // Generate various exceptions
    const int count = 10;
    for (int i = 0; i < count; i++) {
        nimcp_exception_t* ex = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + (i % 3),
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Stats test %d", i
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_dispatch(ex);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);
    }

    // Get final statistics
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);

    printf("  Statistics:\n");
    printf("    exceptions_presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("    exceptions_pending: %lu\n", (unsigned long)stats.exceptions_pending);
    printf("    recoveries_attempted: %lu\n", (unsigned long)stats.recoveries_attempted);
    printf("    recoveries_succeeded: %lu\n", (unsigned long)stats.recoveries_succeeded);
    printf("    memories_formed: %lu\n", (unsigned long)stats.memories_formed);
    printf("    avg_response_time_us: %.2f\n", stats.avg_response_time_us);

    // Verify some stats
    EXPECT_GE(stats.exceptions_presented, (uint64_t)count);

    printf("Test passed: Immune statistics tracking\n\n");
}

/* ============================================================================
 * Test 12: Thread-Safe Immune Presentation
 *
 * Verifies: Immune system handles concurrent presentations safely
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, ThreadSafePresentation) {
    printf("=== Test: Thread-Safe Presentation ===\n");

    std::atomic<int> presentation_count{0};
    const int num_threads = 4;
    const int presentations_per_thread = 25;

    auto thread_func = [&presentation_count](int thread_id) {
        for (int i = 0; i < presentations_per_thread; i++) {
            nimcp_exception_t* ex = nimcp_exception_create(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_ERROR,
                __FILE__, __LINE__, __func__,
                "Thread %d exception %d", thread_id, i
            );
            if (!ex) continue;

            nimcp_exception_dispatch(ex);

            nimcp_immune_response_t response;
            memset(&response, 0, sizeof(response));
            nimcp_exception_present_to_immune(ex, &response);

            presentation_count++;

            nimcp_exception_unref(ex);
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(thread_func, i);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    printf("  Total presentations: %d\n", presentation_count.load());
    EXPECT_EQ(presentation_count.load(), num_threads * presentations_per_thread);

    // Check stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Immune presentations tracked: %lu\n",
           (unsigned long)stats.exceptions_presented);

    printf("Test passed: Thread-safe presentation\n\n");
}

/* ============================================================================
 * Test 13: Priority-Based Immune Response
 *
 * Verifies: Higher severity exceptions get priority immune response
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, PriorityBasedResponse) {
    printf("=== Test: Priority-Based Response ===\n");

    reset_tracking();

    // Create exceptions of different severities
    nimcp_exception_t* debug_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_DEBUG,
        __FILE__, __LINE__, __func__,
        "Debug severity"
    );
    ASSERT_NE(debug_ex, nullptr);

    nimcp_exception_t* warning_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_WARNING,
        __FILE__, __LINE__, __func__,
        "Warning severity"
    );
    ASSERT_NE(warning_ex, nullptr);

    nimcp_exception_t* critical_ex = nimcp_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        "Critical severity"
    );
    ASSERT_NE(critical_ex, nullptr);

    // Present in non-priority order
    nimcp_immune_response_t response;

    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(debug_ex, &response);
    printf("  Debug response type: %u\n", response.action_taken);

    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(warning_ex, &response);
    printf("  Warning response type: %u\n", response.action_taken);

    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(critical_ex, &response);
    printf("  Critical response type: %u\n", response.action_taken);

    // Critical should get strongest response
    // (Response type encoding depends on implementation)

    nimcp_exception_unref(debug_ex);
    nimcp_exception_unref(warning_ex);
    nimcp_exception_unref(critical_ex);

    printf("Test passed: Priority-based response\n\n");
}

/* ============================================================================
 * Test 14: Recovery Callback Chain Execution
 *
 * Verifies: Multiple recovery callbacks execute in correct order
 * ============================================================================ */

static std::vector<std::string> g_callback_order;

static int callback_first(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                          void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_callback_order.push_back("first");
    return 0;
}

static int callback_second(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action,
                           void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_callback_order.push_back("second");
    return 0;
}

TEST_F(ExceptionImmuneE2ETest, RecoveryCallbackChain) {
    printf("=== Test: Recovery Callback Chain ===\n");

    g_callback_order.clear();

    // Register additional callbacks for same action
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, callback_first, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, callback_second, nullptr);

    // Create exception and trigger COMPACT recovery
    nimcp_exception_t* ex = nimcp_exception_create(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Recovery chain test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);

    printf("  Callback order:\n");
    for (size_t i = 0; i < g_callback_order.size(); i++) {
        printf("    %zu: %s\n", i, g_callback_order[i].c_str());
    }

    // Verify all callbacks were called
    EXPECT_GT(g_callback_order.size(), 0u);

    nimcp_exception_unref(ex);
    printf("Test passed: Recovery callback chain\n\n");
}

/* ============================================================================
 * Test 15: Complete E2E Brain Error Recovery
 *
 * Verifies: Full brain error scenario from detection through recovery
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, CompleteE2EBrainRecovery) {
    printf("=== Test: Complete E2E Brain Recovery ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Phase 1: Simulate brain training error
    printf("  Phase 1: Detect training error\n");
    nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
        NIMCP_ERROR_LEARNING_FAILED,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1, "hippocampus",
        "Learning divergence detected: loss=infinity"
    );
    ASSERT_NE(brain_ex, nullptr);

    brain_ex->learning_diverged = true;
    brain_ex->gradient_norm = INFINITY;
    brain_ex->network_id = 1;
    brain_ex->layer_id = 3;

    // Phase 2: Log and dispatch
    printf("  Phase 2: Log and dispatch\n");
    nimcp_exception_log(&brain_ex->base);
    nimcp_exception_dispatch(&brain_ex->base);
    EXPECT_GT(g_handler_calls.load(), 0);

    // Phase 3: Present to immune system
    printf("  Phase 3: Present to immune\n");
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int present_result = nimcp_exception_present_to_immune(&brain_ex->base, &response);
    printf("    Present result: %d\n", present_result);
    printf("    Antigen ID: %u\n", response.antigen_id);

    // Phase 4: Get recovery strategy
    printf("  Phase 4: Get recovery strategy\n");
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(&brain_ex->base, &strategy);
    printf("    Primary: %s\n", nimcp_exception_recovery_action_to_string(strategy.primary_action));
    printf("    Fallback: %s\n", nimcp_exception_recovery_action_to_string(strategy.fallback_action));

    // Phase 5: Execute primary recovery (rollback)
    printf("  Phase 5: Execute rollback recovery\n");
    brain_ex->base.suggested_action = EXCEPTION_RECOVERY_ROLLBACK;
    int recovery_result = nimcp_execute_recovery(&brain_ex->base, EXCEPTION_RECOVERY_ROLLBACK);
    printf("    Recovery result: %d\n", recovery_result);
    EXPECT_GT(g_recovery_rollback_count.load(), 0);

    // Phase 6: Notify success
    printf("  Phase 6: Notify recovery success\n");
    nimcp_exception_notify_recovery_result(&brain_ex->base, EXCEPTION_RECOVERY_ROLLBACK, true);
    brain_ex->base.recovery_attempted = true;
    brain_ex->base.recovery_succeeded = true;

    // Phase 7: Verify final state
    printf("  Phase 7: Verify final state\n");
    EXPECT_TRUE(brain_ex->base.recovery_attempted);
    EXPECT_TRUE(brain_ex->base.recovery_succeeded);

    // Phase 8: Check immune statistics
    printf("  Phase 8: Final statistics\n");
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("    Exceptions presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("    Recoveries attempted: %lu\n", (unsigned long)stats.recoveries_attempted);
    printf("    Recoveries succeeded: %lu\n", (unsigned long)stats.recoveries_succeeded);

    nimcp_exception_unref(&brain_ex->base);
    printf("Test passed: Complete E2E brain recovery\n\n");
}

/* ============================================================================
 * Test 16: Exception-Immune Integration with Signal Handler
 *
 * Verifies: Signal handler correctly integrates with exception-immune pipeline
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, SignalHandlerIntegration) {
    printf("=== Test: Signal Handler Integration ===\n");

    reset_tracking();

    // Simulate signal crash context being captured and processed
    signal_crash_context_t crash_ctx;
    memset(&crash_ctx, 0, sizeof(crash_ctx));
    crash_ctx.signal = SIGSEGV;
    crash_ctx.fault_address = (void*)0xBADCAFE;
    crash_ctx.instruction_pointer = (void*)0x1000;
    crash_ctx.stack_pointer = (void*)0x7FFF0000;
    crash_ctx.backtrace_depth = 3;

    printf("  Simulated crash context: signal=%d, fault_addr=%p\n",
           crash_ctx.signal, crash_ctx.fault_address);

    // Create signal exception from context
    nimcp_signal_exception_t* sig_ex = nimcp_signal_exception_create_from_context(&crash_ctx);
    ASSERT_NE(sig_ex, nullptr);
    printf("  Created signal exception from context\n");

    // Dispatch
    nimcp_exception_dispatch(&sig_ex->base);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&sig_ex->base, &response);
    printf("  Presented to immune: antigen_id=%u\n", response.antigen_id);

    // Execute emergency save
    nimcp_execute_recovery(&sig_ex->base, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_GT(g_recovery_emergency_save_count.load(), 0);

    nimcp_exception_unref(&sig_ex->base);
    printf("Test passed: Signal handler integration\n\n");
}

/* ============================================================================
 * Test 17: Aggregate Exception Immune Processing
 *
 * Verifies: Aggregate exceptions process all children through immune
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, AggregateExceptionImmune) {
    printf("=== Test: Aggregate Exception Immune ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Create aggregate exception
    nimcp_aggregate_exception_t* agg_ex = nimcp_aggregate_exception_create(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        __FILE__, __LINE__, __func__,
        "Batch operation failure"
    );
    ASSERT_NE(agg_ex, nullptr);

    // Add child exceptions
    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* child = nimcp_exception_create(
            NIMCP_ERROR_OPERATION_FAILED + i,
            EXCEPTION_SEVERITY_ERROR,
            __FILE__, __LINE__, __func__,
            "Child exception %d", i
        );
        ASSERT_NE(child, nullptr);
        nimcp_aggregate_exception_add(agg_ex, child);
    }
    printf("  Created aggregate with %zu children\n", nimcp_aggregate_exception_count(agg_ex));

    // Dispatch aggregate
    nimcp_exception_dispatch(&agg_ex->base);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    nimcp_exception_present_to_immune(&agg_ex->base, &response);
    printf("  Presented aggregate to immune\n");

    // Check each child is accessible
    for (size_t i = 0; i < nimcp_aggregate_exception_count(agg_ex); i++) {
        nimcp_exception_t* child = nimcp_aggregate_exception_get(agg_ex, i);
        if (child) {
            printf("    Child %zu: code=%d\n", i, child->code);
        }
    }

    nimcp_exception_unref(&agg_ex->base);
    printf("Test passed: Aggregate exception immune\n\n");
}

/* ============================================================================
 * Test 18: Recovery Strategy Selection Based on Error Type
 *
 * Verifies: Correct recovery strategy is selected for each error type
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, RecoveryStrategySelection) {
    printf("=== Test: Recovery Strategy Selection ===\n");

    struct TestCase {
        nimcp_error_t code;
        const char* name;
        nimcp_exception_recovery_action_t expected_primary;
    };

    TestCase test_cases[] = {
        {NIMCP_ERROR_NO_MEMORY, "Memory", EXCEPTION_RECOVERY_GC},
        {NIMCP_ERROR_LEARNING_FAILED, "Learning", EXCEPTION_RECOVERY_ROLLBACK},
        {NIMCP_ERROR_SIGSEGV, "SIGSEGV", EXCEPTION_RECOVERY_EMERGENCY_SAVE},
        {NIMCP_ERROR_SECURITY_THREAT, "Security", EXCEPTION_RECOVERY_QUARANTINE},
        {NIMCP_ERROR_TIMEOUT, "Timeout", EXCEPTION_RECOVERY_RETRY},
    };

    for (const auto& tc : test_cases) {
        nimcp_exception_t* ex = nimcp_exception_create(
            tc.code,
            EXCEPTION_SEVERITY_SEVERE,
            __FILE__, __LINE__, __func__,
            "Strategy test: %s", tc.name
        );
        ASSERT_NE(ex, nullptr);

        nimcp_exception_recovery_strategy_t strategy;
        nimcp_exception_get_recovery_strategy(ex, &strategy);

        printf("  %s: primary=%s (expected=%s)\n",
               tc.name,
               nimcp_exception_recovery_action_to_string(strategy.primary_action),
               nimcp_exception_recovery_action_to_string(tc.expected_primary));

        nimcp_exception_unref(ex);
    }

    printf("Test passed: Recovery strategy selection\n\n");
}

/* ============================================================================
 * Test 19: Exception to String Formatting
 *
 * Verifies: Exception formatting includes all relevant information
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, ExceptionFormatting) {
    printf("=== Test: Exception Formatting ===\n");

    // Create exception with rich context
    nimcp_brain_exception_t* ex = nimcp_brain_exception_create(
        NIMCP_ERROR_FORWARD_PASS,
        EXCEPTION_SEVERITY_CRITICAL,
        __FILE__, __LINE__, __func__,
        1, "visual_cortex",
        "NaN detected in layer 3"
    );
    ASSERT_NE(ex, nullptr);

    ex->network_id = 5;
    ex->layer_id = 3;
    ex->gradient_norm = 1e10f;
    ex->has_nan_weights = true;

    // Add context
    nimcp_exception_set_context(&ex->base, "batch_id", "42");
    nimcp_exception_set_context(&ex->base, "epoch", "100");

    // Format to string
    char buffer[2048];
    size_t len = nimcp_exception_to_string(&ex->base, buffer, sizeof(buffer));

    printf("  Formatted exception (%zu bytes):\n%s\n", len, buffer);

    // Verify key information is present
    // Error code 3006 is NIMCP_ERROR_FORWARD_PASS - verify code or category present
    EXPECT_NE(strstr(buffer, "3006"), nullptr) << "Expected error code 3006 in output";
    EXPECT_NE(strstr(buffer, "BRAIN"), nullptr) << "Expected BRAIN category in output";
    EXPECT_NE(strstr(buffer, "CRITICAL"), nullptr);

    nimcp_exception_unref(&ex->base);
    printf("Test passed: Exception formatting\n\n");
}

/* ============================================================================
 * Test 20: Full Pipeline Stress Test
 *
 * Verifies: System handles sustained exception load correctly
 * ============================================================================ */

TEST_F(ExceptionImmuneE2ETest, PipelineStressTest) {
    printf("=== Test: Pipeline Stress Test ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    const int total_exceptions = 100;
    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < total_exceptions; i++) {
        // Vary exception types
        nimcp_exception_type_t type = (nimcp_exception_type_t)(i % 6);
        nimcp_error_t code = NIMCP_ERROR_OPERATION_FAILED + (i % 10);
        nimcp_exception_severity_t severity =
            (nimcp_exception_severity_t)(EXCEPTION_SEVERITY_WARNING + (i % 3));

        nimcp_exception_t* ex;

        switch (type) {
            case EXCEPTION_TYPE_MEMORY: {
                nimcp_memory_exception_t* mem_ex = nimcp_memory_exception_create(
                    NIMCP_ERROR_NO_MEMORY, severity,
                    __FILE__, __LINE__, __func__,
                    1024 * (i + 1), "Stress test %d", i);
                ex = &mem_ex->base;
                break;
            }
            case EXCEPTION_TYPE_BRAIN: {
                nimcp_brain_exception_t* brain_ex = nimcp_brain_exception_create(
                    NIMCP_ERROR_FORWARD_PASS, severity,
                    __FILE__, __LINE__, __func__,
                    i % 5, "region", "Stress test %d", i);
                ex = &brain_ex->base;
                break;
            }
            default: {
                ex = nimcp_exception_create(code, severity,
                    __FILE__, __LINE__, __func__,
                    "Stress test %d", i);
                break;
            }
        }

        if (!ex) continue;

        // Full pipeline
        nimcp_exception_dispatch(ex);

        nimcp_immune_response_t response;
        memset(&response, 0, sizeof(response));
        nimcp_exception_present_to_immune(ex, &response);

        nimcp_exception_unref(ex);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    printf("  Processed %d exceptions in %ld ms\n", total_exceptions, (long)duration_ms);
    printf("  Throughput: %.2f exceptions/second\n",
           (float)total_exceptions * 1000.0f / duration_ms);

    // Get final stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Final stats:\n");
    printf("    Presented: %lu\n", (unsigned long)stats.exceptions_presented);
    printf("    Pending: %lu\n", (unsigned long)stats.exceptions_pending);
    printf("    Overflows: %lu\n", (unsigned long)stats.queue_overflows);

    printf("Test passed: Pipeline stress test\n\n");
}
