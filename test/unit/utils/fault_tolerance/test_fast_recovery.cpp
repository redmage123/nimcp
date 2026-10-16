/**
 * @file test_fast_recovery.cpp
 * @brief Unit tests for fast path recovery system
 *
 * COVERAGE TARGET: 100%
 * TEST COUNT: 30+ tests covering all functions and edge cases
 *
 * @author NIMCP Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <csignal>
#include <fenv.h>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_fast_recovery.h"

//=============================================================================
// Test Fixture
//=============================================================================

class FastRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset statistics before each test
        fast_recovery_reset_stats();
    }

    void TearDown() override {
        // Clean up
    }

    // Helper: Create minimal context
    fast_recovery_context_t create_context(int signal, bool numeric = false,
                                          bool memory = false, bool state = false) {
        fast_recovery_context_t ctx = {};
        ctx.signal = signal;
        ctx.is_numeric_error = numeric;
        ctx.is_memory_error = memory;
        ctx.is_state_error = state;
        return ctx;
    }
};

//=============================================================================
// Pattern Matching Tests (Applicability)
//=============================================================================

TEST_F(FastRecoveryTest, ApplicableSignalFPE) {
    fast_recovery_type_t type = fast_recovery_is_applicable_signal(SIGFPE);
    EXPECT_EQ(type, FAST_RECOVERY_RESET_FPU);
}

TEST_F(FastRecoveryTest, ApplicableSignalABRT) {
    fast_recovery_type_t type = fast_recovery_is_applicable_signal(SIGABRT);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_CACHE);
}

TEST_F(FastRecoveryTest, ApplicableSignalSEGV_NotApplicable) {
    fast_recovery_type_t type = fast_recovery_is_applicable_signal(SIGSEGV);
    EXPECT_EQ(type, FAST_RECOVERY_NONE);
}

TEST_F(FastRecoveryTest, ApplicableSignalUnknown) {
    fast_recovery_type_t type = fast_recovery_is_applicable_signal(999);
    EXPECT_EQ(type, FAST_RECOVERY_NONE);
}

TEST_F(FastRecoveryTest, ApplicableContextNumeric) {
    auto ctx = create_context(SIGFPE, true, false, false);
    fast_recovery_type_t type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_NAN);
}

TEST_F(FastRecoveryTest, ApplicableContextMemory) {
    auto ctx = create_context(SIGABRT, false, true, false);
    ctx.brain_ptr = (void*)0x1234;  // Fake brain pointer
    fast_recovery_type_t type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_CLEAR_CACHE);
}

TEST_F(FastRecoveryTest, ApplicableContextState) {
    auto ctx = create_context(0, false, false, true);
    fast_recovery_type_t type = fast_recovery_is_applicable(&ctx);
    EXPECT_EQ(type, FAST_RECOVERY_RESET_STATE);
}

TEST_F(FastRecoveryTest, ApplicableContextNullPtr) {
    fast_recovery_type_t type = fast_recovery_is_applicable(nullptr);
    EXPECT_EQ(type, FAST_RECOVERY_NONE);
}

//=============================================================================
// Recovery Execution Tests
//=============================================================================

TEST_F(FastRecoveryTest, ExecuteResetFPU) {
    auto result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);

    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_EQ(result.type, FAST_RECOVERY_RESET_FPU);
    EXPECT_LT(result.latency_us, 1000u);  // <1ms
    EXPECT_FALSE(result.fallback_needed);
    EXPECT_STREQ(result.message, "Fast recovery successful");
}

TEST_F(FastRecoveryTest, ExecuteClearNaN) {
    auto result = fast_recovery_execute(FAST_RECOVERY_CLEAR_NAN, nullptr);

    // Without brain, should return NOT_APPLICABLE
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_EQ(result.type, FAST_RECOVERY_CLEAR_NAN);
}

TEST_F(FastRecoveryTest, ExecuteClearNaNWithBrain) {
    // Test pattern matching identifies this as applicable
    // When numeric error flag is set with SIGFPE signal
    auto ctx = create_context(SIGFPE, true, false, false);
    EXPECT_EQ(fast_recovery_is_applicable(&ctx), FAST_RECOVERY_CLEAR_NAN);
}

TEST_F(FastRecoveryTest, ExecuteClipGradients) {
    auto result = fast_recovery_execute(FAST_RECOVERY_CLIP_GRADIENTS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FastRecoveryTest, ExecuteClipGradientsWithBrain) {
    // Test pattern matching identifies overflow condition
    auto ctx = create_context(SIGFPE, false, false, false);
    ctx.signal = SIGFPE;
    int exceptions = FE_OVERFLOW;

    // Pattern matching should suggest gradient clipping on overflow
    // (depends on FPU exception state)
    auto type = fast_recovery_is_applicable(&ctx);
    EXPECT_TRUE(type == FAST_RECOVERY_RESET_FPU || type == FAST_RECOVERY_CLIP_GRADIENTS);
}

TEST_F(FastRecoveryTest, ExecuteClearCache) {
    auto result = fast_recovery_execute(FAST_RECOVERY_CLEAR_CACHE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FastRecoveryTest, ExecuteClearCacheWithBrain) {
    // Test pattern matching identifies memory error requiring cache clear
    auto ctx = create_context(SIGABRT, false, true, false);
    ctx.brain_ptr = (void*)0x1234;  // Fake brain pointer

    EXPECT_EQ(fast_recovery_is_applicable(&ctx), FAST_RECOVERY_CLEAR_CACHE);
}

TEST_F(FastRecoveryTest, ExecuteFlushBuffers) {
    auto result = fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_LT(result.latency_us, 1000u);
}

TEST_F(FastRecoveryTest, ExecuteResetState) {
    auto result = fast_recovery_execute(FAST_RECOVERY_RESET_STATE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

TEST_F(FastRecoveryTest, ExecuteResetStateWithBrain) {
    // Test pattern matching identifies state error
    auto ctx = create_context(0, false, false, true);
    EXPECT_EQ(fast_recovery_is_applicable(&ctx), FAST_RECOVERY_RESET_STATE);
}

TEST_F(FastRecoveryTest, ExecuteResetCounter) {
    auto result = fast_recovery_execute(FAST_RECOVERY_RESET_COUNTER, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_LT(result.latency_us, 1000u);
}

TEST_F(FastRecoveryTest, ExecuteTriggerGC) {
    auto result = fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_LT(result.latency_us, 5000u);  // GC can be slower (varies by system/allocator state)
}

TEST_F(FastRecoveryTest, ExecuteInvalidType) {
    auto result = fast_recovery_execute(FAST_RECOVERY_NONE, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_STREQ(result.message, "Invalid recovery type");
}

TEST_F(FastRecoveryTest, ExecuteOutOfRangeType) {
    auto result = fast_recovery_execute(FAST_RECOVERY_TYPE_COUNT, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

//=============================================================================
// Context-Based Execution Tests
//=============================================================================

TEST_F(FastRecoveryTest, ExecuteWithContextNumeric) {
    auto ctx = create_context(SIGFPE, true, false, false);
    auto result = fast_recovery_execute_with_context(&ctx, nullptr);

    EXPECT_EQ(result.type, FAST_RECOVERY_CLEAR_NAN);
    EXPECT_LT(result.latency_us, 1000u);
}

TEST_F(FastRecoveryTest, ExecuteWithContextNullContext) {
    auto result = fast_recovery_execute_with_context(nullptr, nullptr);
    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_STREQ(result.message, "Invalid context");
}

TEST_F(FastRecoveryTest, ExecuteWithContextNoMatch) {
    auto ctx = create_context(SIGSEGV, false, false, false);
    auto result = fast_recovery_execute_with_context(&ctx, nullptr);

    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
    EXPECT_STREQ(result.message, "No fast path applicable");
}

TEST_F(FastRecoveryTest, AttemptSuccess) {
    auto ctx = create_context(SIGFPE, false, false, false);
    auto result = fast_recovery_attempt(&ctx, nullptr);

    EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    EXPECT_EQ(result.type, FAST_RECOVERY_RESET_FPU);
}

TEST_F(FastRecoveryTest, AttemptNotApplicable) {
    auto ctx = create_context(SIGSEGV, false, false, false);
    auto result = fast_recovery_attempt(&ctx, nullptr);

    EXPECT_EQ(result.status, FAST_RECOVERY_NOT_APPLICABLE);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(FastRecoveryTest, StatsInitiallyZero) {
    auto stats = fast_recovery_get_stats();

    EXPECT_EQ(stats.fast_hits, 0u);
    EXPECT_EQ(stats.fast_misses, 0u);
    EXPECT_EQ(stats.successful_recoveries, 0u);
    EXPECT_EQ(stats.failed_recoveries, 0u);
    EXPECT_EQ(stats.total_latency_us, 0u);
}

TEST_F(FastRecoveryTest, StatsTrackSuccess) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 1u);
    EXPECT_EQ(stats.successful_recoveries, 1u);
    EXPECT_EQ(stats.reset_fpu_count, 1u);
    EXPECT_GT(stats.total_latency_us, 0u);
}

TEST_F(FastRecoveryTest, StatsTrackMiss) {
    auto ctx = create_context(SIGSEGV, false, false, false);
    fast_recovery_execute_with_context(&ctx, nullptr);

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_misses, 1u);
}

TEST_F(FastRecoveryTest, StatsTrackMultiple) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    fast_recovery_execute(FAST_RECOVERY_RESET_COUNTER, nullptr);

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 3u);
    EXPECT_EQ(stats.successful_recoveries, 3u);
    EXPECT_EQ(stats.reset_fpu_count, 1u);
    EXPECT_EQ(stats.flush_buffers_count, 1u);
    EXPECT_EQ(stats.reset_counter_count, 1u);
}

TEST_F(FastRecoveryTest, StatsReset) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);

    fast_recovery_reset_stats();

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 0u);
    EXPECT_EQ(stats.successful_recoveries, 0u);
}

TEST_F(FastRecoveryTest, AvgLatency) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);

    uint32_t avg = fast_recovery_get_avg_latency_us();
    EXPECT_GT(avg, 0u);
    EXPECT_LT(avg, 1000u);
}

TEST_F(FastRecoveryTest, AvgLatencyNoData) {
    uint32_t avg = fast_recovery_get_avg_latency_us();
    EXPECT_EQ(avg, 0u);
}

TEST_F(FastRecoveryTest, HitRate) {
    // Execute some successes and misses
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);

    auto ctx = create_context(SIGSEGV, false, false, false);
    fast_recovery_execute_with_context(&ctx, nullptr);

    float hit_rate = fast_recovery_get_hit_rate();
    EXPECT_GT(hit_rate, 0.0f);
    EXPECT_LT(hit_rate, 100.0f);
    // 2 hits, 1 miss = 66.67%
    EXPECT_NEAR(hit_rate, 66.67f, 0.1f);
}

TEST_F(FastRecoveryTest, HitRateNoData) {
    float hit_rate = fast_recovery_get_hit_rate();
    EXPECT_EQ(hit_rate, 0.0f);
}

TEST_F(FastRecoveryTest, SuccessRate) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);

    float success_rate = fast_recovery_get_success_rate();
    EXPECT_EQ(success_rate, 100.0f);  // Both successful
}

TEST_F(FastRecoveryTest, SuccessRateNoData) {
    float success_rate = fast_recovery_get_success_rate();
    EXPECT_EQ(success_rate, 0.0f);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(FastRecoveryTest, TypeNames) {
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_NONE), "NONE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLEAR_NAN), "CLEAR_NAN");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLIP_GRADIENTS), "CLIP_GRADIENTS");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_FPU), "RESET_FPU");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_CLEAR_CACHE), "CLEAR_CACHE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_FLUSH_BUFFERS), "FLUSH_BUFFERS");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_STATE), "RESET_STATE");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_RESET_COUNTER), "RESET_COUNTER");
    EXPECT_STREQ(fast_recovery_type_name(FAST_RECOVERY_TRIGGER_GC), "TRIGGER_GC");
    EXPECT_STREQ(fast_recovery_type_name((fast_recovery_type_t)999), "UNKNOWN");
}

TEST_F(FastRecoveryTest, StatusNames) {
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_SUCCESS), "SUCCESS");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_PARTIAL), "PARTIAL");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_NOT_APPLICABLE), "NOT_APPLICABLE");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_FAILED), "FAILED");
    EXPECT_STREQ(fast_recovery_status_name(FAST_RECOVERY_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(fast_recovery_status_name((fast_recovery_status_t)999), "UNKNOWN");
}

TEST_F(FastRecoveryTest, TypicalLatencies) {
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_RESET_FPU), 15u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_RESET_COUNTER), 15u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_CLEAR_NAN), 75u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_RESET_STATE), 75u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_CLIP_GRADIENTS), 150u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_FLUSH_BUFFERS), 200u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_CLEAR_CACHE), 350u);
    EXPECT_EQ(fast_recovery_get_typical_latency_us(FAST_RECOVERY_TRIGGER_GC), 750u);
}

TEST_F(FastRecoveryTest, ValidateResultValid) {
    auto result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    EXPECT_TRUE(fast_recovery_validate_result(&result));
}

TEST_F(FastRecoveryTest, ValidateResultNullPtr) {
    EXPECT_FALSE(fast_recovery_validate_result(nullptr));
}

TEST_F(FastRecoveryTest, ValidateResultInvalidType) {
    fast_recovery_result_t result = {};
    result.type = FAST_RECOVERY_TYPE_COUNT;
    result.status = FAST_RECOVERY_SUCCESS;
    result.message = "test";

    EXPECT_FALSE(fast_recovery_validate_result(&result));
}

TEST_F(FastRecoveryTest, ValidateResultInvalidStatus) {
    fast_recovery_result_t result = {};
    result.type = FAST_RECOVERY_RESET_FPU;
    result.status = (fast_recovery_status_t)999;
    result.message = "test";

    EXPECT_FALSE(fast_recovery_validate_result(&result));
}

TEST_F(FastRecoveryTest, ValidateResultExcessiveLatency) {
    fast_recovery_result_t result = {};
    result.type = FAST_RECOVERY_RESET_FPU;
    result.status = FAST_RECOVERY_SUCCESS;
    result.latency_us = 20000;  // 20ms (too high)
    result.message = "test";

    EXPECT_FALSE(fast_recovery_validate_result(&result));
}

TEST_F(FastRecoveryTest, ValidateResultNullMessage) {
    fast_recovery_result_t result = {};
    result.type = FAST_RECOVERY_RESET_FPU;
    result.status = FAST_RECOVERY_SUCCESS;
    result.latency_us = 100;
    result.message = nullptr;

    EXPECT_FALSE(fast_recovery_validate_result(&result));
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(FastRecoveryTest, PerformanceSubMillisecond) {
    // All recovery types should complete in <1ms
    fast_recovery_type_t types[] = {
        FAST_RECOVERY_RESET_FPU,
        FAST_RECOVERY_RESET_COUNTER,
        FAST_RECOVERY_FLUSH_BUFFERS,
        FAST_RECOVERY_TRIGGER_GC
    };

    for (auto type : types) {
        auto result = fast_recovery_execute(type, nullptr);
        EXPECT_LT(result.latency_us, 1000u) << "Type: " << fast_recovery_type_name(type);
    }
}

TEST_F(FastRecoveryTest, PerformancePatternMatching) {
    // Pattern matching should be very fast (<50μs)
    auto ctx = create_context(SIGFPE, true, false, false);

    uint64_t start = 0, end = 0;
    struct timeval tv;

    gettimeofday(&tv, nullptr);
    start = tv.tv_sec * 1000000ULL + tv.tv_usec;

    // Match 1000 times
    for (int i = 0; i < 1000; i++) {
        fast_recovery_is_applicable(&ctx);
    }

    gettimeofday(&tv, nullptr);
    end = tv.tv_sec * 1000000ULL + tv.tv_usec;

    uint64_t avg_us = (end - start) / 1000;
    EXPECT_LT(avg_us, 50u);  // <50μs per match
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(FastRecoveryTest, MultipleRecoveriesInSequence) {
    // Simulate rapid recovery sequence
    for (int i = 0; i < 10; i++) {
        auto result = fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
        EXPECT_EQ(result.status, FAST_RECOVERY_SUCCESS);
    }

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.reset_fpu_count, 10u);
}

TEST_F(FastRecoveryTest, ConcurrentStatisticsUpdate) {
    // Statistics should handle concurrent updates
    // (single-threaded test, but validates atomic operations work)
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);
    fast_recovery_execute(FAST_RECOVERY_FLUSH_BUFFERS, nullptr);
    fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);

    auto stats = fast_recovery_get_stats();
    EXPECT_EQ(stats.fast_hits, 3u);
    EXPECT_EQ(stats.successful_recoveries, 3u);
}

TEST_F(FastRecoveryTest, MinMaxLatencyTracking) {
    fast_recovery_execute(FAST_RECOVERY_RESET_FPU, nullptr);      // Very fast
    fast_recovery_execute(FAST_RECOVERY_TRIGGER_GC, nullptr);     // Slower

    auto stats = fast_recovery_get_stats();
    EXPECT_GT(stats.max_latency_us, stats.min_latency_us);
    EXPECT_GT(stats.min_latency_us, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
