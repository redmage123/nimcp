/**
 * @file test_signal_exception_regression.cpp
 * @brief Regression tests for signal exception system
 *
 * WHAT: Prevent regressions in signal exception handling
 * WHY:  Ensure backward compatibility and consistent behavior
 * HOW:  Test known edge cases, historical bugs, and API contracts
 *
 * REGRESSION CATEGORIES:
 * - Error code mapping consistency
 * - Queue behavior under stress
 * - Exception field preservation
 * - Immune integration contracts
 * - Memory safety
 * - API backward compatibility
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <signal.h>
#include <vector>
#include <thread>

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

class SignalExceptionRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_system_init();
        signal_exception_queue_init();
    }

    void TearDown() override {
        signal_exception_queue_process(0);
        signal_exception_queue_shutdown();
        nimcp_exception_clear_current();
        nimcp_exception_system_shutdown();
    }
};

//=============================================================================
// Error Code Mapping Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, ErrorCodeMappingConsistency) {
    // REGRESSION: Ensure error codes never change (API contract)
    EXPECT_EQ(nimcp_signal_to_error_code(SIGSEGV), 7001);  // NIMCP_ERROR_SIGSEGV
    EXPECT_EQ(nimcp_signal_to_error_code(SIGABRT), 7002);  // NIMCP_ERROR_SIGABRT
    EXPECT_EQ(nimcp_signal_to_error_code(SIGFPE),  7003);  // NIMCP_ERROR_SIGFPE
    EXPECT_EQ(nimcp_signal_to_error_code(SIGBUS),  7004);  // NIMCP_ERROR_SIGBUS
    EXPECT_EQ(nimcp_signal_to_error_code(SIGILL),  7005);  // NIMCP_ERROR_SIGILL
}

TEST_F(SignalExceptionRegressionTest, SignalNameConsistency) {
    // REGRESSION: Signal names must be stable for logging/metrics
    EXPECT_STREQ(nimcp_signal_name(SIGSEGV), "SIGSEGV");
    EXPECT_STREQ(nimcp_signal_name(SIGABRT), "SIGABRT");
    EXPECT_STREQ(nimcp_signal_name(SIGFPE), "SIGFPE");
    EXPECT_STREQ(nimcp_signal_name(SIGBUS), "SIGBUS");
    EXPECT_STREQ(nimcp_signal_name(SIGILL), "SIGILL");
    EXPECT_STREQ(nimcp_signal_name(SIGTERM), "SIGTERM");
    EXPECT_STREQ(nimcp_signal_name(SIGINT), "SIGINT");
    EXPECT_STREQ(nimcp_signal_name(SIGHUP), "SIGHUP");
    EXPECT_STREQ(nimcp_signal_name(999), "UNKNOWN");
}

//=============================================================================
// Exception Type Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, ExceptionTypeIsSignal) {
    // REGRESSION: Signal exceptions must have correct type
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, NULL, __FILE__, __LINE__, __func__, "test"
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.type, EXCEPTION_TYPE_SIGNAL);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, ExceptionCategoryIsSignal) {
    // REGRESSION: Signal exceptions must have SIGNAL category
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, NULL, __FILE__, __LINE__, __func__, "test"
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->base.category, EXCEPTION_CATEGORY_SIGNAL);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, ExceptionSeverityIsFatal) {
    // REGRESSION: All signal exceptions must be FATAL severity
    int signals[] = { SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL };
    for (int sig : signals) {
        nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
            sig, NULL, __FILE__, __LINE__, __func__, "test"
        );
        ASSERT_NE(ex, nullptr);
        EXPECT_EQ(ex->base.severity, EXCEPTION_SEVERITY_FATAL)
            << "Signal " << sig << " should have FATAL severity";
        nimcp_exception_unref((nimcp_exception_t*)ex);
    }
}

//=============================================================================
// Queue Behavior Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, QueueFIFOOrder) {
    // REGRESSION: Queue must maintain FIFO order
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    // Enqueue in order
    for (int i = 0; i < 8; i++) {
        ctx.signal = SIGSEGV;
        ctx.fault_address = (void*)(uintptr_t)(i + 1);
        ASSERT_TRUE(signal_exception_queue_enqueue(ctx.signal, &ctx));
    }

    // Dequeue and verify FIFO order
    for (int i = 0; i < 8; i++) {
        signal_exception_entry_t entry;
        ASSERT_TRUE(signal_exception_queue_dequeue(&entry));
        EXPECT_EQ(entry.ctx.fault_address, (void*)(uintptr_t)(i + 1))
            << "FIFO order violated at index " << i;
    }
}

TEST_F(SignalExceptionRegressionTest, QueueEmptyDequeue) {
    // REGRESSION: Dequeue from empty queue must return false
    signal_exception_entry_t entry;
    EXPECT_FALSE(signal_exception_queue_dequeue(&entry));
}

TEST_F(SignalExceptionRegressionTest, QueueStatsAccuracy) {
    // REGRESSION: Statistics must be accurate
    signal_exception_queue_reset_stats();

    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;

    // Enqueue 5
    for (int i = 0; i < 5; i++) {
        signal_exception_queue_enqueue(ctx.signal, &ctx);
    }

    // Dequeue 3
    signal_exception_entry_t entry;
    for (int i = 0; i < 3; i++) {
        signal_exception_queue_dequeue(&entry);
    }

    signal_exception_queue_stats_t stats;
    signal_exception_queue_get_stats(&stats);
    EXPECT_EQ(stats.enqueue_count, 5u);
    EXPECT_EQ(stats.dequeue_count, 3u);
    EXPECT_EQ(stats.pending_count, 2u);
}

//=============================================================================
// Memory Safety Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, NullContextHandling) {
    // REGRESSION: NULL context must return NULL, not crash
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(NULL);
    EXPECT_EQ(ex, nullptr);
}

TEST_F(SignalExceptionRegressionTest, NullFaultAddressAccepted) {
    // REGRESSION: NULL fault address must be accepted
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGFPE, NULL, __FILE__, __LINE__, __func__, "Division by zero"
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_EQ(ex->fault_address, nullptr);
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, NullFormatStringHandling) {
    // REGRESSION: NULL format string must generate default message
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, (void*)0x1234, __FILE__, __LINE__, __func__, NULL
    );
    ASSERT_NE(ex, nullptr);
    EXPECT_NE(ex->base.message[0], '\0');  // Message should not be empty
    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, ReferenceCountingWorks) {
    // REGRESSION: Reference counting must work correctly
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, NULL, __FILE__, __LINE__, __func__, "test"
    );
    ASSERT_NE(ex, nullptr);

    // Add reference
    nimcp_exception_ref((nimcp_exception_t*)ex);

    // First unref shouldn't free
    nimcp_exception_unref((nimcp_exception_t*)ex);

    // Should still be valid
    EXPECT_EQ(ex->signal_number, SIGSEGV);

    // Second unref frees
    nimcp_exception_unref((nimcp_exception_t*)ex);
    // After this, ex is invalid - don't access it
}

//=============================================================================
// Immune Integration Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, ImmuneRecoveryStrategyContract) {
    // REGRESSION: SIGNAL category recovery strategy must be consistent
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, NULL, __FILE__, __LINE__, __func__, "test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy((nimcp_exception_t*)ex, &strategy);

    // SIGNAL category contract:
    EXPECT_EQ(strategy.primary_action, EXCEPTION_RECOVERY_EMERGENCY_SAVE);
    EXPECT_EQ(strategy.fallback_action, EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN);
    EXPECT_EQ(strategy.retry_count, 1);
    EXPECT_EQ(strategy.cooldown_ms, 0u);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, ImmunePresentationSucceeds) {
    // REGRESSION: Immune presentation must succeed for signal exceptions
    nimcp_signal_exception_t* ex = nimcp_signal_exception_create(
        SIGSEGV, NULL, __FILE__, __LINE__, __func__, "test"
    );
    ASSERT_NE(ex, nullptr);

    nimcp_immune_response_t response;
    memset(&response, 0, sizeof(response));
    int result = nimcp_exception_present_to_immune((nimcp_exception_t*)ex, &response);
    EXPECT_EQ(result, 0);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// API Backward Compatibility Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, TypeToStringIncludesSignal) {
    // REGRESSION: Type to string must return "SIGNAL" for signal type
    const char* name = nimcp_exception_type_to_string(EXCEPTION_TYPE_SIGNAL);
    EXPECT_STREQ(name, "SIGNAL");
}

TEST_F(SignalExceptionRegressionTest, QueueInitShutdownIdempotent) {
    // REGRESSION: Multiple init/shutdown calls must be safe
    // Note: Already init'd in SetUp

    // Shutdown
    signal_exception_queue_shutdown();
    EXPECT_FALSE(signal_exception_queue_is_initialized());

    // Init again
    EXPECT_EQ(signal_exception_queue_init(), 0);
    EXPECT_TRUE(signal_exception_queue_is_initialized());

    // Multiple shutdowns should be safe
    signal_exception_queue_shutdown();
    signal_exception_queue_shutdown();  // Should not crash

    // Re-init for TearDown
    signal_exception_queue_init();
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

TEST_F(SignalExceptionRegressionTest, LongMemoryRegionTruncation) {
    // REGRESSION: Long memory region strings must be safely truncated
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;

    // Create very long string
    std::string long_region(1024, 'x');
    strncpy(ctx.memory_region, long_region.c_str(), sizeof(ctx.memory_region) - 1);
    ctx.memory_region[sizeof(ctx.memory_region) - 1] = '\0';

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&ctx);
    ASSERT_NE(ex, nullptr);

    // Should be truncated but not overflow
    size_t len = strlen(ex->memory_region);
    EXPECT_LT(len, NIMCP_SIGNAL_EXCEPTION_MEMORY_REGION_SIZE);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

TEST_F(SignalExceptionRegressionTest, MaxBacktraceDepth) {
    // REGRESSION: Backtrace depth must be capped
    signal_crash_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.signal = SIGSEGV;
    ctx.backtrace_depth = 1000;  // Excessive depth

    for (int i = 0; i < 64 && i < 1000; i++) {
        ctx.backtrace[i] = (void*)(uintptr_t)(0x400000 + i * 0x100);
    }

    nimcp_signal_exception_t* ex = nimcp_signal_exception_create_from_context(&ctx);
    ASSERT_NE(ex, nullptr);

    // Depth should be capped
    EXPECT_LE(ex->base.stack_trace.depth, (size_t)NIMCP_EXCEPTION_MAX_STACK_DEPTH);

    nimcp_exception_unref((nimcp_exception_t*)ex);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
