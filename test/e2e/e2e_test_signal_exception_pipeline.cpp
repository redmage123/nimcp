/**
 * @file e2e_test_signal_exception_pipeline.cpp
 * @brief End-to-end tests for signal exception recovery pipeline
 *
 * WHAT: Test complete signal crash → exception → immune → recovery pipeline
 * WHY:  Verify the full system works end-to-end in realistic scenarios
 * HOW:  Simulate actual crashes, test recovery macros, verify immune response
 *
 * E2E SCENARIOS:
 * - Simulated SIGSEGV with full recovery flow
 * - Multiple crash recovery in sequence
 * - Crash recovery with immune-directed action
 * - Handler chain receives and processes exceptions
 * - Crash telemetry collection
 *
 * SAFETY NOTE:
 * These tests carefully avoid triggering actual crashes that would terminate
 * the test process. Instead, they use the mock crash context path or
 * controlled recovery scenarios.
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/signal/nimcp_signal_exception_queue.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SignalExceptionE2ETest : public ::testing::Test {
protected:
    static std::atomic<int> exceptions_handled;
    static std::atomic<int> immune_presentations;
    static std::atomic<nimcp_error_t> last_error_code;
    static std::atomic<nimcp_exception_recovery_action_t> last_recovery_action;
    static std::vector<std::string> exception_log;
    static nimcp_handler_registration_t* handler_registration;

    void SetUp() override {
        exceptions_handled = 0;
        immune_presentations = 0;
        last_error_code = 0;
        last_recovery_action = EXCEPTION_RECOVERY_NONE;
        exception_log.clear();

        nimcp_exception_system_init();
        signal_exception_queue_init();

        // Register our test handler
        nimcp_handler_options_t options;
        nimcp_handler_default_options(&options);
        options.name = "e2e_test_handler";
        options.handler = test_exception_handler;
        options.priority = 50;
        options.user_data = NULL;
        handler_registration = nimcp_handler_register(&options);
    }

    void TearDown() override {
        if (handler_registration) {
            nimcp_handler_unregister(handler_registration);
            handler_registration = NULL;
        }
        signal_exception_queue_process(0);
        signal_exception_queue_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        exceptions_handled++;
        last_error_code = ex->code;

        char log_entry[256];
        snprintf(log_entry, sizeof(log_entry),
                 "Exception: type=%d, code=%d, severity=%d, msg=%s",
                 ex->type, ex->code, ex->severity, ex->message);
        exception_log.push_back(log_entry);
        return false;  // Don't consume - allow other handlers to process
    }

    // Simulate a crash context without actually crashing
    signal_crash_context_t create_mock_crash(int signal, void* fault_addr) {
        signal_crash_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.signal = signal;
        ctx.fault_address = fault_addr;
        ctx.instruction_pointer = (void*)0x400100;
        ctx.stack_pointer = (void*)0x7FFF1000;
        ctx.base_pointer = (void*)0x7FFF1010;
        strncpy(ctx.memory_region, "[heap] rw-p 00000000-00001000",
                sizeof(ctx.memory_region) - 1);
        ctx.backtrace_depth = 3;
        ctx.backtrace[0] = (void*)0x400100;
        ctx.backtrace[1] = (void*)0x400200;
        ctx.backtrace[2] = (void*)0x400300;
        return ctx;
    }
};

std::atomic<int> SignalExceptionE2ETest::exceptions_handled(0);
std::atomic<int> SignalExceptionE2ETest::immune_presentations(0);
std::atomic<nimcp_error_t> SignalExceptionE2ETest::last_error_code(0);
std::atomic<nimcp_exception_recovery_action_t> SignalExceptionE2ETest::last_recovery_action(EXCEPTION_RECOVERY_NONE);
std::vector<std::string> SignalExceptionE2ETest::exception_log;
nimcp_handler_registration_t* SignalExceptionE2ETest::handler_registration = NULL;

//=============================================================================
// Full Pipeline E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, FullPipelineSIGSEGV) {
    // E2E: Crash → Queue → Exception → Immune → Handler
    // Simulates complete flow for SIGSEGV

    // Step 1: Create mock crash context (simulating signal handler capture)
    signal_crash_context_t ctx = create_mock_crash(SIGSEGV, (void*)0xDEADBEEF);

    // Step 2: Enqueue to exception queue (as signal handler would do)
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));
    EXPECT_EQ(signal_exception_queue_pending_count(), 1u);

    // Step 3: Process queue (as main thread would do)
    size_t processed = signal_exception_queue_process(0);
    EXPECT_EQ(processed, 1u);

    // Step 4: Verify handler was called
    EXPECT_GE(exceptions_handled.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_SIGSEGV);

    // Step 5: Verify exception was logged
    EXPECT_FALSE(exception_log.empty());
    EXPECT_TRUE(exception_log[0].find("SIGNAL") != std::string::npos ||
                exception_log[0].find("7001") != std::string::npos);
}

TEST_F(SignalExceptionE2ETest, FullPipelineAllSignalTypes) {
    // E2E: Test pipeline for all supported signal types

    struct {
        int signal;
        nimcp_error_t expected_code;
        void* fault_addr;
    } test_cases[] = {
        { SIGSEGV, NIMCP_ERROR_SIGSEGV, (void*)0x1000 },
        { SIGFPE,  NIMCP_ERROR_SIGFPE,  (void*)0x2000 },
        { SIGBUS,  NIMCP_ERROR_SIGBUS,  (void*)0x3000 },
        { SIGILL,  NIMCP_ERROR_SIGILL,  (void*)0x4000 },
        { SIGABRT, NIMCP_ERROR_SIGABRT, (void*)0x5000 }
    };

    for (const auto& tc : test_cases) {
        exceptions_handled = 0;
        last_error_code = 0;

        signal_crash_context_t ctx = create_mock_crash(tc.signal, tc.fault_addr);
        ASSERT_TRUE(signal_exception_queue_enqueue(tc.signal, &ctx));

        size_t processed = signal_exception_queue_process(0);
        EXPECT_EQ(processed, 1u);

        EXPECT_GE(exceptions_handled.load(), 1)
            << "Handler not called for signal " << tc.signal;
        EXPECT_EQ(last_error_code.load(), tc.expected_code)
            << "Wrong error code for signal " << tc.signal;
    }
}

//=============================================================================
// Multi-Crash E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, MultiCrashSequence) {
    // E2E: Multiple crashes in sequence

    // Queue 5 different crashes
    for (int i = 0; i < 5; i++) {
        signal_crash_context_t ctx = create_mock_crash(
            SIGSEGV, (void*)(uintptr_t)(0x1000 * (i + 1))
        );
        ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));
    }

    EXPECT_EQ(signal_exception_queue_pending_count(), 5u);

    // Process all
    size_t processed = signal_exception_queue_process(0);
    EXPECT_EQ(processed, 5u);
    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);

    // Verify all were handled
    EXPECT_GE(exceptions_handled.load(), 5);
}

TEST_F(SignalExceptionE2ETest, InterleavedCrashAndProcess) {
    // E2E: Interleaved enqueue and process (simulating ongoing operation)

    int total_processed = 0;

    for (int round = 0; round < 3; round++) {
        // Enqueue 2 crashes
        for (int i = 0; i < 2; i++) {
            signal_crash_context_t ctx = create_mock_crash(
                SIGSEGV, (void*)(uintptr_t)((round * 100) + i)
            );
            ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));
        }

        // Process 1
        size_t processed = signal_exception_queue_process(1);
        total_processed += processed;
    }

    // Process remaining
    total_processed += signal_exception_queue_process(0);

    EXPECT_EQ(total_processed, 6);
    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);
}

//=============================================================================
// Immune Integration E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, ImmuneRecoveryDecision) {
    // E2E: Verify immune system provides recovery decision

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, (void*)0xCAFE, __FILE__, __LINE__, __func__,
        "E2E immune test crash"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // Get recovery strategy
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // Signal category should recommend emergency save
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);

    // Dispatch to handlers
    nimcp_exception_dispatch((nimcp_exception_t*)ex);
    EXPECT_GE(exceptions_handled.load(), 1);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Crash Telemetry E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, CrashTelemetryCollection) {
    // E2E: Verify crash details are preserved for telemetry

    signal_crash_context_t ctx = create_mock_crash(SIGSEGV, (void*)0xBADC0DE);
    ctx.instruction_pointer = (void*)0x12345678;
    ctx.stack_pointer = (void*)0x7FFFFFFFD000;
    strncpy(ctx.memory_region, "/lib/libc.so.6 [r-xp]", sizeof(ctx.memory_region) - 1);

    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));

    // Dequeue and verify telemetry data
    signal_exception_entry_t entry;
    ASSERT_TRUE(signal_exception_queue_dequeue(&entry));

    // Create exception
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&entry.ctx);
    ASSERT_NE(ex, nullptr);

    // Verify all telemetry fields
    EXPECT_EQ(ex->signal_number, SIGSEGV);
    EXPECT_EQ(ex->fault_address, (void*)0xBADC0DE);
    EXPECT_EQ(ex->instruction_pointer, (void*)0x12345678);
    EXPECT_EQ(ex->stack_pointer, (void*)0x7FFFFFFFD000);
    EXPECT_STREQ(ex->memory_region, "/lib/libc.so.6 [r-xp]");
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_SIGSEGV);
    EXPECT_GT(ex->base.timestamp_us, 0u);
    EXPECT_GT(entry.timestamp_us, 0u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Exception Creation E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, DirectExceptionCreationAndDispatch) {
    // E2E: Direct exception creation without queue

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGFPE,
        NULL,
        __FILE__,
        __LINE__,
        __func__,
        "Division by zero in calculation module"
    );
    ASSERT_NE(ex, nullptr);

    // Verify exception properties
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_SIGNAL);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SIGNAL);
    EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_FATAL);
    EXPECT_EQ(ex->signal_number, SIGFPE);
    EXPECT_TRUE(strstr(ex->base.message, "Division by zero") != NULL);

    // Present to immune and dispatch
    nimcp_exception_present_to_immune((nimcp_exception_t*)ex, NULL);
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_GE(exceptions_handled.load(), 1);
    EXPECT_EQ(last_error_code.load(), NIMCP_ERROR_SIGFPE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Performance E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, HighThroughputProcessing) {
    // E2E: Verify performance under load

    const int NUM_CRASHES = 100;
    auto start = std::chrono::high_resolution_clock::now();

    // Enqueue many crashes
    for (int i = 0; i < NUM_CRASHES; i++) {
        signal_crash_context_t ctx = create_mock_crash(SIGSEGV, (void*)(uintptr_t)i);
        bool success = signal_exception_queue_enqueue(SIGSEGV, &ctx);
        // Queue may overflow, that's OK for this test
        if (!success) break;
    }

    // Process all
    size_t processed = signal_exception_queue_process(0);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should process in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000000);

    // Verify all processed
    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);

    printf("Processed %zu exceptions in %ld microseconds\n",
           processed, (long)duration.count());
}

//=============================================================================
// Error Path E2E Tests
//=============================================================================

TEST_F(SignalExceptionE2ETest, GracefulDegradation) {
    // E2E: System degrades gracefully under error conditions

    // Shutdown queue
    signal_exception_queue_shutdown();

    // Operations should fail gracefully, not crash
    signal_crash_context_t ctx = create_mock_crash(SIGSEGV, (void*)0x1234);
    bool result = signal_exception_queue_enqueue(SIGSEGV, &ctx);
    EXPECT_FALSE(result);

    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);
    EXPECT_TRUE(signal_exception_queue_is_empty());

    // Re-init should work
    EXPECT_EQ(signal_exception_queue_init(), 0);
    result = signal_exception_queue_enqueue(SIGSEGV, &ctx);
    EXPECT_TRUE(result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
