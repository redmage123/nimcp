/**
 * @file test_signal_exception_integration.cpp
 * @brief Integration tests for signal-to-exception pipeline
 *
 * WHAT: Test complete flow from signal handler to exception to immune system
 * WHY:  Verify all components work together correctly
 * HOW:  Simulate crashes and verify exception creation, queue processing, immune response
 *
 * TEST SCENARIOS:
 * - Signal -> Queue -> Exception creation
 * - Queue -> Exception -> Immune presentation
 * - Exception -> Handler chain dispatch
 * - Recovery macros with exception integration
 * - Multi-signal queue processing
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <signal.h>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/signal/nimcp_signal_handler.h"
#include "utils/signal/nimcp_signal_exception_queue.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class SignalExceptionIntegrationTest : public ::testing::Test {
protected:
    static std::atomic<int> handler_call_count;
    static std::atomic<int> last_exception_code;
    static std::atomic<bool> immune_was_called;

    void SetUp() override {
        handler_call_count = 0;
        last_exception_code = 0;
        immune_was_called = false;

        nimcp_exception_system_init();
        signal_exception_queue_init();
    }

    void TearDown() override {
        // Process any remaining exceptions
        signal_exception_queue_process(0);
        signal_exception_queue_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }

    static bool test_exception_handler(nimcp_exception_t* ex, void* user_data) {
        (void)user_data;
        handler_call_count++;
        last_exception_code = ex->code;
        return false;  // Don't consume - allow other handlers to process
    }

    static void test_callback(const signal_exception_entry_t* entry, void* user_data) {
        (void)user_data;
        (void)entry;
        immune_was_called = true;
    }
};

std::atomic<int> SignalExceptionIntegrationTest::handler_call_count(0);
std::atomic<int> SignalExceptionIntegrationTest::last_exception_code(0);
std::atomic<bool> SignalExceptionIntegrationTest::immune_was_called(false);

//=============================================================================
// Queue to Exception Flow Tests
//=============================================================================

TEST_F(SignalExceptionIntegrationTest, QueueToExceptionFlow) {
    // WHAT: Test queue enqueue -> dequeue -> exception creation flow
    // WHY:  Verify core pipeline works correctly

    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0xDEADBEEF;
    ctx.instruction_pointer = (void*)0x400000;
    ctx.stack_pointer = (void*)0x7FFF0000;

    // Enqueue crash context
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));
    EXPECT_EQ(signal_exception_queue_pending_count(), 1u);

    // Dequeue and create exception
    signal_exception_entry_t entry;
    ASSERT_TRUE(signal_exception_queue_dequeue(&entry));
    EXPECT_EQ(entry.ctx.signal, SIGSEGV);

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&entry.ctx);
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->signal_number, SIGSEGV);
    EXPECT_EQ(ex->fault_address, (void*)0xDEADBEEF);
    EXPECT_EQ(ex->base.code, NIMCP_ERROR_SIGSEGV);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_SIGNAL);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SIGNAL);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionIntegrationTest, ExceptionToImmuneFlow) {
    // WHAT: Test exception creation -> immune presentation flow
    // WHY:  Verify immune system receives signal exceptions

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, (void*)0x1234, __FILE__, __LINE__, __func__,
        "Test SIGSEGV for immune presentation"
    );
    ASSERT_NE(ex, nullptr);

    // Present to immune system
    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    // For SIGNAL category, primary action should be EMERGENCY_SAVE
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionIntegrationTest, ExceptionToHandlerChain) {
    // WHAT: Test exception dispatch through handler chain
    // WHY:  Verify handlers receive signal exceptions

    // Register test handler
    nimcp_handler_options_t options;
    nimcp_handler_default_options(&options);
    options.name = "test_signal_handler";
    options.handler = test_exception_handler;
    options.priority = 100;
    options.user_data = NULL;
    nimcp_handler_registration_t* reg = nimcp_handler_register(&options);

    // Create and dispatch signal exception
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGFPE, NULL, __FILE__, __LINE__, __func__,
        "Test SIGFPE for handler chain"
    );
    ASSERT_NE(ex, nullptr);

    handler_call_count = 0;
    nimcp_exception_dispatch((nimcp_exception_t*)ex);

    EXPECT_GE(handler_call_count.load(), 1);
    EXPECT_EQ(last_exception_code.load(), NIMCP_ERROR_SIGFPE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
    if (reg) nimcp_handler_unregister(reg);
}

//=============================================================================
// Queue Processing Tests
//=============================================================================

TEST_F(SignalExceptionIntegrationTest, QueueProcessMultiple) {
    // WHAT: Test processing multiple queued crashes
    // WHY:  Verify batch processing works correctly

    // Register callback
    signal_exception_queue_set_callback(test_callback, NULL);

    // Enqueue multiple crashes
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0x1000;
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));

    ctx.signal = SIGFPE;
    ctx.fault_address = (void*)0x2000;
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGFPE, &ctx));

    ctx.signal = SIGBUS;
    ctx.fault_address = (void*)0x3000;
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGBUS, &ctx));

    EXPECT_EQ(signal_exception_queue_pending_count(), 3u);

    // Process all
    immune_was_called = false;
    size_t processed = signal_exception_queue_process(0);
    EXPECT_EQ(processed, 3u);
    EXPECT_TRUE(immune_was_called.load());
    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);

    signal_exception_queue_set_callback(NULL, NULL);
}

TEST_F(SignalExceptionIntegrationTest, QueueProcessPartial) {
    // WHAT: Test partial queue processing (with max_count)
    // WHY:  Verify max_count limit works correctly

    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;

    // Enqueue 5 crashes
    for (int i = 0; i < 5; i++) {
        ctx.fault_address = (void*)(uintptr_t)(0x1000 + i * 0x100);
        ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));
    }

    EXPECT_EQ(signal_exception_queue_pending_count(), 5u);

    // Process only 2
    size_t processed = signal_exception_queue_process(2);
    EXPECT_EQ(processed, 2u);
    EXPECT_EQ(signal_exception_queue_pending_count(), 3u);

    // Process remaining
    processed = signal_exception_queue_process(0);
    EXPECT_EQ(processed, 3u);
    EXPECT_EQ(signal_exception_queue_pending_count(), 0u);
}

//=============================================================================
// Signal Handler Integration Tests
//=============================================================================

TEST_F(SignalExceptionIntegrationTest, SignalHandlerProcessPending) {
    // WHAT: Test signal_handler_process_pending_exceptions() wrapper
    // WHY:  Verify the signal handler API works correctly

    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0xDEAD;

    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));

    size_t count = signal_handler_get_pending_exception_count();
    EXPECT_EQ(count, 1u);

    size_t processed = signal_handler_process_pending_exceptions(0);
    EXPECT_EQ(processed, 1u);

    count = signal_handler_get_pending_exception_count();
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// Error Code Mapping Integration Tests
//=============================================================================

TEST_F(SignalExceptionIntegrationTest, AllSignalTypesMapping) {
    // WHAT: Test all signal types create correct exceptions
    // WHY:  Verify complete signal coverage

    struct {
        int signal;
        nimcp_error_t expected_code;
        const char* name;
    } signals[] = {
        { SIGSEGV, NIMCP_ERROR_SIGSEGV, "SIGSEGV" },
        { SIGABRT, NIMCP_ERROR_SIGABRT, "SIGABRT" },
        { SIGFPE,  NIMCP_ERROR_SIGFPE,  "SIGFPE" },
        { SIGBUS,  NIMCP_ERROR_SIGBUS,  "SIGBUS" },
        { SIGILL,  NIMCP_ERROR_SIGILL,  "SIGILL" }
    };

    for (const auto& sig : signals) {
        nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
            sig.signal, NULL, __FILE__, __LINE__, __func__, "%s test", sig.name
        );
        ASSERT_NE(ex, nullptr) << "Failed for signal: " << sig.name;
        EXPECT_EQ(ex->base.code, sig.expected_code) << "Wrong code for: " << sig.name;
        EXPECT_EQ(ex->signal_number, sig.signal) << "Wrong signal for: " << sig.name;
        EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_FATAL) << "Wrong severity for: " << sig.name;
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

//=============================================================================
// Context Preservation Tests
//=============================================================================

TEST_F(SignalExceptionIntegrationTest, ContextPreservedThroughQueue) {
    // WHAT: Test crash context is preserved through queue
    // WHY:  Verify no data loss in pipeline

    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.fault_address = (void*)0xCAFEBABE;
    ctx.instruction_pointer = (void*)0x12345678;
    ctx.stack_pointer = (void*)0x7FFFFF00;
    ctx.base_pointer = (void*)0x7FFFFF10;
    strncpy(ctx.memory_region, "[heap] rw-p", sizeof(ctx.memory_region) - 1);
    ctx.backtrace_depth = 3;
    ctx.backtrace[0] = (void*)0x400100;
    ctx.backtrace[1] = (void*)0x400200;
    ctx.backtrace[2] = (void*)0x400300;

    // Enqueue
    ASSERT_TRUE(signal_exception_queue_enqueue(SIGSEGV, &ctx));

    // Dequeue
    signal_exception_entry_t entry;
    ASSERT_TRUE(signal_exception_queue_dequeue(&entry));

    // Verify all fields preserved
    EXPECT_EQ(entry.ctx.signal, SIGSEGV);
    EXPECT_EQ(entry.ctx.fault_address, (void*)0xCAFEBABE);
    EXPECT_EQ(entry.ctx.instruction_pointer, (void*)0x12345678);
    EXPECT_EQ(entry.ctx.stack_pointer, (void*)0x7FFFFF00);
    EXPECT_EQ(entry.ctx.base_pointer, (void*)0x7FFFFF10);
    EXPECT_STREQ(entry.ctx.memory_region, "[heap] rw-p");
    EXPECT_EQ(entry.ctx.backtrace_depth, 3);
    EXPECT_EQ(entry.ctx.backtrace[0], (void*)0x400100);
    EXPECT_EQ(entry.ctx.backtrace[1], (void*)0x400200);
    EXPECT_EQ(entry.ctx.backtrace[2], (void*)0x400300);

    // Create exception and verify
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&entry.ctx);
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->fault_address, (void*)0xCAFEBABE);
    EXPECT_EQ(ex->instruction_pointer, (void*)0x12345678);
    EXPECT_EQ(ex->stack_pointer, (void*)0x7FFFFF00);
    EXPECT_EQ(ex->base_pointer, (void*)0x7FFFFF10);
    EXPECT_STRNE(ex->memory_region, "");
    EXPECT_EQ(ex->base.stack_trace.depth, 3u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
