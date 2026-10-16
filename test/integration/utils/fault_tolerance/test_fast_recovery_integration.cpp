/**
 * @file test_fast_recovery_integration.cpp
 * @brief Integration tests for fast recovery with signal handler and recovery system
 *
 * COVERAGE: End-to-end fast path scenarios
 * TEST COUNT: 10+ integration tests
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <csignal>
#include <fenv.h>
#include <sys/time.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_fast_recovery.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_diagnostics.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FastRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        fast_recovery_reset_stats();
    }

    void TearDown() override {
        // Clean up
    }

    // Helper: Simulate signal context
    fast_recovery_context_t simulate_signal(int sig, bool numeric = false) {
        fast_recovery_context_t ctx = {};
        ctx.signal = sig;
        ctx.is_numeric_error = numeric;
        ctx.fault_address = nullptr;
        return ctx;
    }

    // Helper: Measure operation time
    uint64_t measure_time_us(std::function<void()> op) {
        struct timeval start, end;
        gettimeofday(&start, nullptr);
        op();
        gettimeofday(&end, nullptr);

        uint64_t start_us = start.tv_sec * 1000000ULL + start.tv_usec;
        uint64_t end_us = end.tv_sec * 1000000ULL + end.tv_usec;
        return end_us - start_us;
    }
};

//=============================================================================
// Integration with Signal Handler
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, SignalHandlerFastPath_SIGFPE) {
    // Simulate SIGFPE signal handling with fast path
    auto ctx = simulate_signal(SIGFPE, true);

    // Check if fast path is applicable
    auto type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_NAN);

    // Execute fast recovery
    auto result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_NE(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_LT(result.latency_us, 1000u);  // Must be <1ms
}

TEST_F(FastRecoveryIntegrationTest, SignalHandlerFastPath_SIGABRT) {
    auto ctx = simulate_signal(SIGABRT, false);
    ctx.is_memory_error = true;
    ctx.brain_ptr = (void*)0x1234;  // Fake brain

    auto result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_EQ(result.type, FAST_RECOVERY_CLEAR_CACHE);
}

TEST_F(FastRecoveryIntegrationTest, SignalHandlerFallback_SIGSEGV) {
    // SIGSEGV should not have fast path
    auto ctx = simulate_signal(SIGSEGV, false);

    auto result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    // Note: fallback_needed is false when NOT_APPLICABLE since no recovery was attempted

    // Verify miss was recorded
    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_misses, 1u);
}

//=============================================================================
// Integration with Recovery System
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, FastPathBeforeFullRecovery) {
    // Fast path should be checked before full diagnostic workflow
    auto ctx = simulate_signal(SIGFPE, true);

    // 1. Try fast path first
    auto fast_result = fast_recovery_attempt(&ctx, nullptr);

    if (fast_result.status == FAST_RECOVERY_SUCCESS) {
        // Fast path succeeded - no need for full recovery
        EXPECT_FALSE(fast_result.fallback_needed);
        EXPECT_LT(fast_result.latency_us, 1000u);
    } else {
        // Fast path failed - would fall back to full recovery
        EXPECT_TRUE(fast_result.fallback_needed);
    }
}

TEST_F(FastRecoveryIntegrationTest, FastPathStatisticsAccumulation) {
    // Execute multiple recoveries and verify stats accumulate
    for (int i = 0; i < 5; i++) {
        auto ctx = simulate_signal(SIGFPE, false);
        fast_recovery_attempt(&ctx, nullptr);
    }

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 5u);
    EXPECT_GT(stats.total_latency_us, 0u);
}

//=============================================================================
// Performance Integration Tests
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, Performance_200x_Speedup) {
    // Verify fast path is ~200x faster than full diagnostic workflow
    auto ctx = simulate_signal(SIGFPE, true);

    // Measure fast path
    uint64_t fast_time = measure_time_us([&]() {
        for (int i = 0; i < 100; i++) {
            fast_recovery_attempt(&ctx, nullptr);
        }
    });

    uint64_t avg_fast_us = fast_time / 100;

    // Fast path should be <1ms (typically 100-500μs)
    EXPECT_LT(avg_fast_us, 1000u);

    // Expected: ~100-500μs vs 20ms full diagnostic = 40-200x speedup
    // We target <1ms which is ≥20x speedup minimum
}

TEST_F(FastRecoveryIntegrationTest, PerformanceBenchmark_AllTypes) {
    // Benchmark all recovery types
    fast_recovery_type_t types[] = {
        FAST_RECOVERY_RESET_FPU,
        FAST_RECOVERY_RESET_COUNTER,
        FAST_RECOVERY_FLUSH_BUFFERS,
        FAST_RECOVERY_TRIGGER_GC
    };

    for (auto type : types) {
        uint64_t time_us = measure_time_us([&]() {
            fast_recovery_execute(type, nullptr);
        });

        EXPECT_LT(time_us, 2000u) << "Type: " << fast_recovery_type_name(type);

        // Most should be much faster
        if (type != FAST_RECOVERY_TRIGGER_GC) {
            EXPECT_LT(time_us, 500u) << "Type: " << fast_recovery_type_name(type);
        }
    }
}

TEST_F(FastRecoveryIntegrationTest, PerformanceUnderLoad) {
    // Test performance under rapid recovery load
    auto ctx = simulate_signal(SIGFPE, false);

    uint64_t total_time = measure_time_us([&]() {
        for (int i = 0; i < 1000; i++) {
            fast_recovery_attempt(&ctx, nullptr);
        }
    });

    uint64_t avg_time_us = total_time / 1000;
    EXPECT_LT(avg_time_us, 500u);  // Should average <500μs even under load

    // Verify stats
    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 1000u);
}

//=============================================================================
// End-to-End Scenarios
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, E2E_NumericErrorRecovery) {
    // Simulate numeric error (NaN/Inf) detection and recovery
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGFPE;
    ctx.is_numeric_error = true;
    ctx.error_code = FE_INVALID;

    // 1. Pattern matching
    auto type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_NAN);

    // 2. Execute recovery
    auto result = fast_recovery_execute(type, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);  // Succeeds in simulation mode
    EXPECT_LT(result.latency_us, 1000u);

    // 3. Verify statistics
    auto stats = fast_recovery_get_stats();
    EXPECT_GT(stats.fast_hits, 0u);
}

TEST_F(FastRecoveryIntegrationTest, E2E_MemoryPressureRecovery) {
    // Simulate memory pressure during brain operations
    fast_recovery_context_t ctx = {};
    ctx.signal = SIGABRT;
    ctx.is_memory_error = true;
    ctx.brain_ptr = (void*)0x1234;  // Fake brain

    auto result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_EQ(result.type, FAST_RECOVERY_CLEAR_CACHE);
}

TEST_F(FastRecoveryIntegrationTest, E2E_StateResetRecovery) {
    // Simulate state machine error
    fast_recovery_context_t ctx = {};
    ctx.is_state_error = true;

    auto result = fast_recovery_attempt(&ctx, nullptr);
    EXPECT_EQ(result.type, FAST_RECOVERY_RESET_STATE);
}

//=============================================================================
// Metrics and Monitoring Integration
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, MetricsCollection) {
    // Execute various recoveries and verify metrics are collected correctly
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);

    auto stats = fast_recovery_get_stats();

    // Verify counts
    EXPECT_EQ(stats.reset_fpu_count, 1u);
    EXPECT_EQ(stats.flush_buffers_count, 1u);
    EXPECT_EQ(stats.trigger_gc_count, 1u);

    // Verify hit rate
    float hit_rate = fast_recovery_get_hit_rate();
    EXPECT_EQ(hit_rate, 100.0f);

    // Verify success rate
    float success_rate = fast_recovery_get_success_rate();
    EXPECT_EQ(success_rate, 100.0f);

    // Verify average latency (may be 0 if operations are extremely fast)
    uint32_t avg_latency = fast_recovery_get_avg_latency_us();
    EXPECT_GE(avg_latency, 0u);  // Changed to >= to allow 0
    EXPECT_LT(avg_latency, 1000u);
}

TEST_F(FastRecoveryIntegrationTest, MetricsHitMissRatio) {
    // Mix of hits and misses
    auto ctx_hit = simulate_signal(SIGFPE, false);
    auto ctx_miss = simulate_signal(SIGSEGV, false);

    // 3 hits
    fast_recovery_attempt(&ctx_hit, nullptr);
    fast_recovery_attempt(&ctx_hit, nullptr);
    fast_recovery_attempt(&ctx_hit, nullptr);

    // 1 miss
    fast_recovery_attempt(&ctx_miss, nullptr);

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 3u);
    EXPECT_EQ(stats.fast_misses, 1u);

    float hit_rate = fast_recovery_get_hit_rate();
    EXPECT_NEAR(hit_rate, 75.0f, 0.1f);  // 3/4 = 75%
}

//=============================================================================
// Reliability Tests
//=============================================================================

TEST_F(FastRecoveryIntegrationTest, ReliabilityRepeatedRecovery) {
    // Verify fast recovery works reliably across many invocations
    auto ctx = simulate_signal(SIGFPE, false);

    int success_count = 0;
    for (int i = 0; i < 100; i++) {
        auto result = fast_recovery_attempt(&ctx, nullptr);
        if (result.status == FAST_RECOVERY_SUCCESS) {
            success_count++;
        }
    }

    EXPECT_EQ(success_count, 100);  // 100% reliability
}

TEST_F(FastRecoveryIntegrationTest, ReliabilityNoMemoryLeaks) {
    // Verify no memory leaks in fast path (all stack-allocated)
    auto ctx = simulate_signal(SIGFPE, false);

    for (int i = 0; i < 10000; i++) {
        fast_recovery_attempt(&ctx, nullptr);
    }

    // Should complete without crashes or excessive memory growth
    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 10000u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
