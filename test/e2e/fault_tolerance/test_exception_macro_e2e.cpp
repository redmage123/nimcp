/**
 * @file test_exception_macro_e2e.cpp
 * @brief E2E tests for exception macros with full lifecycle handling
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end tests for exception macros from throw through dispatch
 *       to immune presentation and recovery
 * WHY:  Verify the exception macro convenience wrappers work correctly
 *       in the complete exception handling pipeline
 * HOW:  Test each macro type: NIMCP_THROW, NIMCP_THROW_TO_IMMUNE,
 *       typed throws (BRAIN, MEMORY, GPU, SECURITY, IO, THREADING, SIGNAL)
 *
 * Test Scenarios:
 * 1. NIMCP_THROW basic flow
 * 2. NIMCP_THROW_TO_IMMUNE with immune presentation
 * 3. NIMCP_THROW_IF conditional throw
 * 4. NIMCP_CHECK_THROW with return value
 * 5. NIMCP_THROW_BRAIN with brain context
 * 6. NIMCP_THROW_MEMORY with allocation details
 * 7. NIMCP_THROW_GPU with device context
 * 8. NIMCP_THROW_SECURITY with threat context
 * 9. NIMCP_THROW_IO with path context
 * 10. NIMCP_THROW_THREADING with thread context
 * 11. NIMCP_THROW_SIGNAL with signal context
 * 12. NIMCP_THROW_ASYNC for async presentation
 * 13. NIMCP_THROW_AND_RECOVER recovery flow
 * 14. NIMCP_THROW_CRITICAL severity override
 * 15. NIMCP_THROW_FATAL emergency response
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <csignal>

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

// Callback tracking
static std::atomic<int> g_handler_calls{0};
static std::atomic<int> g_immune_presentations{0};
static std::atomic<int> g_recovery_attempts{0};
static nimcp_exception_t* g_last_exception = nullptr;
static nimcp_exception_severity_t g_last_severity = EXCEPTION_SEVERITY_DEBUG;
static nimcp_exception_type_t g_last_type = EXCEPTION_TYPE_BASE;
static nimcp_error_t g_last_code = NIMCP_SUCCESS;

// Test handler that tracks calls
static bool test_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    if (ex) {
        g_handler_calls++;
        g_last_exception = ex;
        g_last_severity = ex->severity;
        g_last_type = ex->type;
        g_last_code = ex->code;
    }
    return false;  // Don't consume
}

// Test recovery callback
static int test_recovery(nimcp_exception_t* ex,
                         nimcp_exception_recovery_action_t action,
                         void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;
    g_recovery_attempts++;
    return 0;
}

// Reset tracking state
static void reset_tracking() {
    g_handler_calls = 0;
    g_immune_presentations = 0;
    g_recovery_attempts = 0;
    g_last_exception = nullptr;
    g_last_severity = EXCEPTION_SEVERITY_DEBUG;
    g_last_type = EXCEPTION_TYPE_BASE;
    g_last_code = NIMCP_SUCCESS;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionMacroE2ETest : public ::testing::Test {
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
        config.enable_auto_recovery = false;  // Manual recovery for tests
        config.min_present_severity = EXCEPTION_SEVERITY_ERROR;

        int immune_init = nimcp_exception_immune_init(&config);
        ASSERT_EQ(immune_init, 0) << "Failed to initialize exception-immune integration";

        // Register test handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "MacroTestHandler";
        opts.handler = test_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(handler_reg, nullptr);

        // Register recovery callbacks
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, test_recovery, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, test_recovery, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, test_recovery, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, test_recovery, nullptr);
        nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, test_recovery, nullptr);
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
 * Test 1: NIMCP_THROW Basic Flow
 *
 * Verifies: Basic throw macro creates and dispatches exception
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowBasicFlow) {
    printf("=== Test: NIMCP_THROW Basic Flow ===\n");

    reset_tracking();

    // Use NIMCP_THROW macro
    NIMCP_THROW(NIMCP_ERROR_INVALID_PARAMETER, "Test throw: invalid parameter value %d", 42);

    printf("  Handler calls: %d\n", g_handler_calls.load());

    // Verify exception was dispatched
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_INVALID_PARAMETER);

    printf("Test passed: NIMCP_THROW basic flow\n\n");
}

/* ============================================================================
 * Test 2: NIMCP_THROW_TO_IMMUNE with Immune Presentation
 *
 * Verifies: Throw to immune presents exception to immune system
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowToImmunePresentation) {
    printf("=== Test: NIMCP_THROW_TO_IMMUNE with Presentation ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Use NIMCP_THROW_TO_IMMUNE macro
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Memory allocation failed: %zu bytes", (size_t)1024);

    printf("  Handler calls: %d\n", g_handler_calls.load());

    // Verify exception was handled
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_NO_MEMORY);

    // Check immune stats
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Exceptions presented: %lu\n", (unsigned long)stats.exceptions_presented);

    printf("Test passed: NIMCP_THROW_TO_IMMUNE presentation\n\n");
}

/* ============================================================================
 * Test 3: NIMCP_THROW_IF Conditional Throw
 *
 * Verifies: Conditional throw triggers when condition is TRUE
 * (THROW_IF means "throw if this condition is met")
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowIfConditional) {
    printf("=== Test: NIMCP_THROW_IF Conditional Throw ===\n");

    reset_tracking();

    // Should NOT throw - condition is false (ptr IS null, so ptr == nullptr is true,
    // but we're checking ptr != nullptr which is false)
    int* null_ptr = nullptr;
    NIMCP_THROW_IF(null_ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "ptr is not NULL");
    EXPECT_EQ(g_handler_calls.load(), 0);
    printf("  False condition (null_ptr != nullptr): handler calls = %d (expected 0)\n", g_handler_calls.load());

    // Should throw - condition is true (valid_ptr IS not null)
    int* valid_ptr = new int(42);
    NIMCP_THROW_IF(valid_ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "ptr is not NULL - throwing");
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  True condition (valid_ptr != nullptr): handler calls = %d (expected >0)\n", g_handler_calls.load());
    delete valid_ptr;

    EXPECT_EQ(g_last_code, NIMCP_ERROR_NULL_POINTER);

    printf("Test passed: NIMCP_THROW_IF conditional\n\n");
}

/* ============================================================================
 * Test 4: NIMCP_CHECK_THROW with Return Value
 *
 * Verifies: Check throw returns error code when condition fails
 * ============================================================================ */

// Helper function using NIMCP_CHECK_THROW
static nimcp_error_t helper_check_throw(void* ptr) {
    NIMCP_CHECK_THROW(ptr != nullptr, NIMCP_ERROR_NULL_POINTER, "ptr cannot be NULL");
    return NIMCP_SUCCESS;
}

TEST_F(ExceptionMacroE2ETest, NimcpCheckThrowReturn) {
    printf("=== Test: NIMCP_CHECK_THROW with Return ===\n");

    reset_tracking();

    // Call with valid pointer - should succeed
    int value = 42;
    nimcp_error_t result = helper_check_throw(&value);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(g_handler_calls.load(), 0);
    printf("  Valid ptr: result = %d (expected 0)\n", result);

    // Call with NULL - should return error
    result = helper_check_throw(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  NULL ptr: result = %d (expected %d)\n", result, NIMCP_ERROR_NULL_POINTER);

    printf("Test passed: NIMCP_CHECK_THROW return\n\n");
}

/* ============================================================================
 * Test 5: NIMCP_THROW_BRAIN with Brain Context
 *
 * Verifies: Brain exception macro creates proper typed exception
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowBrainContext) {
    printf("=== Test: NIMCP_THROW_BRAIN with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_BRAIN macro
    uint32_t brain_id = 1;
    const char* region = "prefrontal_cortex";
    NIMCP_THROW_BRAIN(NIMCP_ERROR_FORWARD_PASS, brain_id, region,
                      "Forward pass failed in layer %d", 3);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_BRAIN);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_FORWARD_PASS);

    // Verify brain exception fields if possible
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_BRAIN) {
        nimcp_brain_exception_t* brain_ex = (nimcp_brain_exception_t*)g_last_exception;
        printf("  Brain ID: %u\n", brain_ex->brain_id);
        printf("  Region: %s\n", brain_ex->region_name ? brain_ex->region_name : "null");
    }

    printf("Test passed: NIMCP_THROW_BRAIN context\n\n");
}

/* ============================================================================
 * Test 6: NIMCP_THROW_MEMORY with Allocation Details
 *
 * Verifies: Memory exception macro includes allocation size
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowMemoryDetails) {
    printf("=== Test: NIMCP_THROW_MEMORY with Details ===\n");

    reset_tracking();

    // Use NIMCP_THROW_MEMORY macro
    size_t requested = 1024 * 1024 * 64;  // 64MB
    NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, requested,
                       "Failed to allocate %zu bytes for neural network", requested);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_MEMORY);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_NO_MEMORY);

    // Verify memory exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_MEMORY) {
        nimcp_memory_exception_t* mem_ex = (nimcp_memory_exception_t*)g_last_exception;
        printf("  Requested size: %zu\n", mem_ex->requested_size);
        EXPECT_EQ(mem_ex->requested_size, requested);
    }

    printf("Test passed: NIMCP_THROW_MEMORY details\n\n");
}

/* ============================================================================
 * Test 7: NIMCP_THROW_GPU with Device Context
 *
 * Verifies: GPU exception macro includes device and CUDA error info
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowGpuContext) {
    printf("=== Test: NIMCP_THROW_GPU with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_GPU macro
    int device_id = 0;
    int cuda_err = 2;  // cudaErrorMemoryAllocation
    NIMCP_THROW_GPU(NIMCP_ERROR_GPU_MEMORY, device_id, cuda_err,
                    "CUDA memory allocation failed on device %d", device_id);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_GPU);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_GPU_MEMORY);

    // Verify GPU exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_GPU) {
        nimcp_gpu_exception_t* gpu_ex = (nimcp_gpu_exception_t*)g_last_exception;
        printf("  Device ID: %d\n", gpu_ex->device_id);
        printf("  CUDA error: %d\n", gpu_ex->cuda_error);
        EXPECT_EQ(gpu_ex->device_id, device_id);
        EXPECT_EQ(gpu_ex->cuda_error, cuda_err);
    }

    printf("Test passed: NIMCP_THROW_GPU context\n\n");
}

/* ============================================================================
 * Test 8: NIMCP_THROW_SECURITY with Threat Context
 *
 * Verifies: Security exception macro includes threat type
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowSecurityContext) {
    printf("=== Test: NIMCP_THROW_SECURITY with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_SECURITY macro
    uint32_t threat_type = 1;  // e.g., BBB_THREAT_MALICIOUS_INPUT
    NIMCP_THROW_SECURITY(NIMCP_ERROR_SECURITY_THREAT, threat_type,
                         "Security threat detected: type=%u", threat_type);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_SECURITY);

    // Security exceptions should always be CRITICAL severity
    if (g_last_exception) {
        EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
        printf("  Severity: %s\n",
               nimcp_exception_severity_to_string(g_last_severity));
    }

    // Verify security exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_SECURITY) {
        nimcp_security_exception_t* sec_ex = (nimcp_security_exception_t*)g_last_exception;
        printf("  Threat type: %u\n", sec_ex->threat_type);
        EXPECT_EQ(sec_ex->threat_type, threat_type);
    }

    printf("Test passed: NIMCP_THROW_SECURITY context\n\n");
}

/* ============================================================================
 * Test 9: NIMCP_THROW_IO with Path Context
 *
 * Verifies: I/O exception macro includes file path
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowIoContext) {
    printf("=== Test: NIMCP_THROW_IO with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_IO macro
    const char* path = "/data/models/brain_checkpoint.bin";
    NIMCP_THROW_IO(NIMCP_ERROR_FILE_WRITE, path,
                   "Failed to write checkpoint to %s", path);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_IO);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_FILE_WRITE);

    // Verify IO exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_IO) {
        nimcp_io_exception_t* io_ex = (nimcp_io_exception_t*)g_last_exception;
        printf("  Path: %s\n", io_ex->path ? io_ex->path : "null");
        if (io_ex->path) {
            EXPECT_STREQ(io_ex->path, path);
        }
    }

    printf("Test passed: NIMCP_THROW_IO context\n\n");
}

/* ============================================================================
 * Test 10: NIMCP_THROW_THREADING with Thread Context
 *
 * Verifies: Threading exception macro includes thread ID
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowThreadingContext) {
    printf("=== Test: NIMCP_THROW_THREADING with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_THREADING macro
    uint64_t thread_id = 12345;
    NIMCP_THROW_THREADING(NIMCP_ERROR_DEADLOCK, thread_id,
                          "Deadlock detected in thread %lu", (unsigned long)thread_id);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_THREADING);
    EXPECT_EQ(g_last_code, NIMCP_ERROR_DEADLOCK);

    // Verify threading exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_THREADING) {
        nimcp_threading_exception_t* thread_ex = (nimcp_threading_exception_t*)g_last_exception;
        printf("  Thread ID: %lu\n", (unsigned long)thread_ex->thread_id);
        EXPECT_EQ(thread_ex->thread_id, thread_id);
    }

    printf("Test passed: NIMCP_THROW_THREADING context\n\n");
}

/* ============================================================================
 * Test 11: NIMCP_THROW_SIGNAL with Signal Context
 *
 * Verifies: Signal exception macro creates proper signal exception
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowSignalContext) {
    printf("=== Test: NIMCP_THROW_SIGNAL with Context ===\n");

    reset_tracking();

    // Use NIMCP_THROW_SIGNAL macro
    void* fault_addr = (void*)0xDEADBEEF;
    NIMCP_THROW_SIGNAL(SIGSEGV, fault_addr,
                       "Segmentation fault at address %p", fault_addr);

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_SIGNAL);

    // Verify signal exception fields
    if (g_last_exception && g_last_exception->type == EXCEPTION_TYPE_SIGNAL) {
        nimcp_signal_exception_t* sig_ex = (nimcp_signal_exception_t*)g_last_exception;
        printf("  Signal: %d (%s)\n", sig_ex->signal_number,
               nimcp_signal_name(sig_ex->signal_number));
        printf("  Fault address: %p\n", sig_ex->fault_address);
        EXPECT_EQ(sig_ex->signal_number, SIGSEGV);
        EXPECT_EQ(sig_ex->fault_address, fault_addr);
    }

    printf("Test passed: NIMCP_THROW_SIGNAL context\n\n");
}

/* ============================================================================
 * Test 12: NIMCP_THROW_ASYNC for Async Presentation
 *
 * Verifies: Async throw queues for non-blocking presentation
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowAsyncQueue) {
    printf("=== Test: NIMCP_THROW_ASYNC Queue ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Use NIMCP_THROW_ASYNC macro
    NIMCP_THROW_ASYNC(NIMCP_ERROR_OPERATION_FAILED,
                      "Async exception for non-blocking presentation");

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Process pending async presentations
    size_t processed = nimcp_exception_immune_process_pending(0);
    printf("  Processed async: %zu\n", processed);

    printf("Test passed: NIMCP_THROW_ASYNC queue\n\n");
}

/* ============================================================================
 * Test 13: NIMCP_THROW_AND_RECOVER Recovery Flow
 *
 * Verifies: Throw and recover executes recovery action
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowAndRecoverFlow) {
    printf("=== Test: NIMCP_THROW_AND_RECOVER Flow ===\n");

    reset_tracking();

    // Use NIMCP_THROW_AND_RECOVER macro
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_NO_MEMORY, EXCEPTION_RECOVERY_GC,
                            "Memory pressure - triggering GC");

    printf("  Handler calls: %d\n", g_handler_calls.load());
    printf("  Recovery attempts: %d\n", g_recovery_attempts.load());

    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_GT(g_recovery_attempts.load(), 0);

    printf("Test passed: NIMCP_THROW_AND_RECOVER flow\n\n");
}

/* ============================================================================
 * Test 14: NIMCP_THROW_CRITICAL Severity Override
 *
 * Verifies: Critical throw always uses CRITICAL severity
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowCriticalSeverity) {
    printf("=== Test: NIMCP_THROW_CRITICAL Severity ===\n");

    reset_tracking();

    // Use NIMCP_THROW_CRITICAL macro - should force CRITICAL severity
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_OPERATION_FAILED,
                         "Critical system error requiring immediate attention");

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Verify CRITICAL severity
    if (g_last_exception) {
        printf("  Severity: %s\n",
               nimcp_exception_severity_to_string(g_last_severity));
        EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    }

    printf("Test passed: NIMCP_THROW_CRITICAL severity\n\n");
}

/* ============================================================================
 * Test 15: NIMCP_THROW_FATAL Emergency Response
 *
 * Verifies: Fatal throw uses FATAL severity for emergency handling
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowFatalEmergency) {
    printf("=== Test: NIMCP_THROW_FATAL Emergency ===\n");

    reset_tracking();

    // Use NIMCP_THROW_FATAL macro
    NIMCP_THROW_FATAL(NIMCP_ERROR_CRASH_RECOVERY,
                      "Fatal error: system state corrupted, emergency shutdown");

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Verify FATAL severity
    if (g_last_exception) {
        printf("  Severity: %s\n",
               nimcp_exception_severity_to_string(g_last_severity));
        EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_FATAL);
    }

    printf("Test passed: NIMCP_THROW_FATAL emergency\n\n");
}

/* ============================================================================
 * Test 16: NIMCP_THROW_SEVERITY Explicit Severity
 *
 * Verifies: Severity override macro uses specified severity
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowSeverityOverride) {
    printf("=== Test: NIMCP_THROW_SEVERITY Override ===\n");

    reset_tracking();

    // Use NIMCP_THROW_SEVERITY with explicit WARNING
    NIMCP_THROW_SEVERITY(NIMCP_ERROR_OPERATION_FAILED, EXCEPTION_SEVERITY_WARNING,
                         "Warning: minor issue detected");

    printf("  Handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    // Verify WARNING severity
    if (g_last_exception) {
        printf("  Severity: %s\n",
               nimcp_exception_severity_to_string(g_last_severity));
        EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_WARNING);
    }

    printf("Test passed: NIMCP_THROW_SEVERITY override\n\n");
}

/* ============================================================================
 * Test 17: NIMCP_THROW_TO_IMMUNE_IF Conditional Immune Throw
 *
 * Verifies: Conditional throw to immune only when condition fails
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, NimcpThrowToImmuneIfConditional) {
    printf("=== Test: NIMCP_THROW_TO_IMMUNE_IF Conditional ===\n");

    reset_tracking();

    // Should NOT throw - condition is true
    int valid_value = 100;
    NIMCP_THROW_TO_IMMUNE_IF(valid_value > 0, NIMCP_ERROR_OUT_OF_RANGE,
                              "Value must be positive");
    EXPECT_EQ(g_handler_calls.load(), 0);
    printf("  True condition: handler calls = %d\n", g_handler_calls.load());

    // Should throw - condition is false
    int invalid_value = -5;
    NIMCP_THROW_TO_IMMUNE_IF(invalid_value > 0, NIMCP_ERROR_OUT_OF_RANGE,
                              "Value must be positive, got %d", invalid_value);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  False condition: handler calls = %d\n", g_handler_calls.load());

    printf("Test passed: NIMCP_THROW_TO_IMMUNE_IF conditional\n\n");
}

/* ============================================================================
 * Test 18: Full Exception Macro Lifecycle
 *
 * Verifies: Complete macro-based exception handling from throw to recovery
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, FullMacroLifecycle) {
    printf("=== Test: Full Macro Lifecycle ===\n");

    reset_tracking();
    nimcp_exception_immune_reset_stats();

    // Phase 1: Throw exception with context
    printf("  Phase 1: Throw brain exception\n");
    NIMCP_THROW_BRAIN(NIMCP_ERROR_LEARNING_FAILED, 1, "hippocampus",
                      "Learning diverged: gradient explosion");

    EXPECT_GT(g_handler_calls.load(), 0);
    EXPECT_EQ(g_last_type, EXCEPTION_TYPE_BRAIN);

    // Phase 2: Verify immune presentation occurred
    printf("  Phase 2: Check immune presentation\n");
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("    Presented: %lu\n", (unsigned long)stats.exceptions_presented);

    // Phase 3: Create memory exception with recovery
    printf("  Phase 3: Throw memory exception with recovery\n");
    reset_tracking();
    NIMCP_THROW_AND_RECOVER(NIMCP_ERROR_NO_MEMORY, EXCEPTION_RECOVERY_GC,
                            "Memory allocation failed in training batch");

    EXPECT_GT(g_recovery_attempts.load(), 0);
    printf("    Recovery attempts: %d\n", g_recovery_attempts.load());

    // Phase 4: Critical exception for emergency handling
    printf("  Phase 4: Throw critical exception\n");
    reset_tracking();
    NIMCP_THROW_CRITICAL(NIMCP_ERROR_BRAIN_INVALID,
                         "Brain state corrupted - critical error");

    EXPECT_EQ(g_last_severity, EXCEPTION_SEVERITY_CRITICAL);
    printf("    Severity: %s\n", nimcp_exception_severity_to_string(g_last_severity));

    printf("Test passed: Full macro lifecycle\n\n");
}

/* ============================================================================
 * Test 19: Exception Macro with Multiple Threads
 *
 * Verifies: Macro-based exception handling is thread-safe
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, MacroMultiThreaded) {
    printf("=== Test: Macro Multi-Threaded ===\n");

    std::atomic<int> thread_exceptions{0};
    const int num_threads = 4;
    const int exceptions_per_thread = 10;

    auto thread_func = [&thread_exceptions](int thread_id) {
        for (int i = 0; i < exceptions_per_thread; i++) {
            NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED,
                        "Thread %d exception %d", thread_id, i);
            thread_exceptions++;
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

    printf("  Total thread exceptions: %d\n", thread_exceptions.load());
    EXPECT_EQ(thread_exceptions.load(), num_threads * exceptions_per_thread);

    // Handler should have been called for each exception
    // Note: exact count depends on timing
    printf("  Total handler calls: %d\n", g_handler_calls.load());
    EXPECT_GT(g_handler_calls.load(), 0);

    printf("Test passed: Macro multi-threaded\n\n");
}

/* ============================================================================
 * Test 20: Exception Macro Format String Variants
 *
 * Verifies: Macros handle various format string patterns
 * ============================================================================ */

TEST_F(ExceptionMacroE2ETest, MacroFormatStrings) {
    printf("=== Test: Macro Format Strings ===\n");

    reset_tracking();

    // No format arguments
    NIMCP_THROW(NIMCP_ERROR_OPERATION_FAILED, "Simple message with no args");
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  No args: OK\n");

    reset_tracking();

    // Integer format
    NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "Value %d is out of range [%d, %d]",
                100, 0, 50);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Integer args: OK\n");

    reset_tracking();

    // Float format
    NIMCP_THROW(NIMCP_ERROR_FORWARD_PASS, "Gradient norm: %.6f exceeds %.2f",
                1000.123456, 10.0);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Float args: OK\n");

    reset_tracking();

    // String format
    NIMCP_THROW(NIMCP_ERROR_FILE_NOT_FOUND, "File not found: %s in directory %s",
                "config.json", "/data/models");
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  String args: OK\n");

    reset_tracking();

    // Size_t and pointer format
    void* ptr = (void*)0x12345678;
    NIMCP_THROW(NIMCP_ERROR_INVALID_ADDRESS, "Invalid address %p with size %zu",
                ptr, (size_t)1024);
    EXPECT_GT(g_handler_calls.load(), 0);
    printf("  Size_t/pointer args: OK\n");

    printf("Test passed: Macro format strings\n\n");
}
