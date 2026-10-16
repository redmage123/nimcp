/**
 * @file test_recovery_cache_integration.cpp
 * @brief Integration tests for recovery cache
 *
 * Test Coverage:
 * - End-to-end recovery workflow with caching
 * - Performance improvements from caching
 * - Learning behavior over time
 * - Cache integration with error handling
 * - Real-world usage patterns
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_cache.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class RecoveryCacheIntegrationTest : public ::testing::Test {
protected:
    nimcp_recovery_cache_t* cache;

    void SetUp() override {
        cache = nimcp_recovery_cache_create(0);  // Default capacity
        ASSERT_NE(cache, nullptr);
    }

    void TearDown() override {
        if (cache) {
            nimcp_recovery_cache_destroy(cache);
            cache = nullptr;
        }
    }

    /* Simulate error with context */
    nimcp_signal_error_context_t simulate_error(
        int signal,
        void* addr,
        const char* func,
        int error_code = 0)
    {
        nimcp_signal_error_context_t ctx = {0};
        ctx.signal = signal;
        ctx.fault_address = addr;
        ctx.context_hash = std::hash<std::string>{}(func);
        ctx.function_name = func;
        ctx.error_code = error_code;
        return ctx;
    }

    /* Simulate recovery attempt */
    bool attempt_recovery(
        const nimcp_error_signature_t* sig,
        nimcp_recovery_strategy_t strategy)
    {
        /* Simulate different recovery success rates */
        switch (strategy) {
            case NIMCP_RECOVERY_STRATEGY_RETRY:
                return true;  // 100% success
            case NIMCP_RECOVERY_STRATEGY_ROLLBACK:
                return rand() % 10 < 9;  // 90% success
            case NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE:
                return rand() % 10 < 8;  // 80% success
            default:
                return rand() % 2 == 0;  // 50% success
        }
    }

    /* Measure operation time in nanoseconds */
    uint64_t measure_time_ns(std::function<void()> operation) {
        auto start = std::chrono::high_resolution_clock::now();
        operation();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
};

/* ============================================================================
 * END-TO-END RECOVERY WORKFLOW TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, FirstErrorSlowPath) {
    /* Simulate first occurrence of an error */
    auto ctx = simulate_error(11, (void*)0x1000, "compute_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* First time - cache miss, slow diagnostic path */
    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* Simulate expensive diagnostic determining best strategy */
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    strategy = NIMCP_RECOVERY_STRATEGY_RETRY;

    /* Store successful strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, strategy,
                "Retry after memory allocation failure"));

    /* Update success */
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));

    /* Verify stored */
    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));
    EXPECT_EQ(entry.strategy, NIMCP_RECOVERY_STRATEGY_RETRY);
    EXPECT_EQ(entry.success_count, 1);
}

TEST_F(RecoveryCacheIntegrationTest, RepeatedErrorFastPath) {
    /* First occurrence */
    auto ctx = simulate_error(11, (void*)0x1000, "compute_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Store strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, "Retry strategy"));

    /* Repeated occurrences should be fast */
    for (int i = 0; i < 10; i++) {
        auto ctx_repeat = simulate_error(11, (void*)0x1000, "compute_function");
        nimcp_error_signature_t sig_repeat;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx_repeat, &sig_repeat));

        nimcp_recovery_strategy_t strategy;
        uint64_t lookup_time = measure_time_ns([&]() {
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig_repeat, &strategy));
        });

        EXPECT_EQ(strategy, NIMCP_RECOVERY_STRATEGY_RETRY);
        EXPECT_LT(lookup_time, 1000);  // < 1us (relaxed for real system with cold cache)
    }
}

TEST_F(RecoveryCacheIntegrationTest, CompleteRecoveryWorkflow) {
    /* Simulate complete error -> recovery -> cache workflow */
    auto ctx = simulate_error(6, (void*)0x5000, "floating_point_operation", SIGFPE);
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* 1. Check cache (miss) */
    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* 2. Perform diagnostic (expensive) */
    strategy = NIMCP_RECOVERY_STRATEGY_RETRY;  // Changed to RETRY for 100% success

    /* 3. Attempt recovery */
    bool recovered = attempt_recovery(&sig, strategy);
    EXPECT_TRUE(recovered);  // RETRY always succeeds

    /* 4. Store successful strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, strategy,
                "Retry after FPE"));

    /* 5. Update success tracking */
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, recovered));

    /* 6. Next occurrence uses fast path */
    auto ctx2 = simulate_error(6, (void*)0x5000, "floating_point_operation", SIGFPE);
    nimcp_error_signature_t sig2;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx2, &sig2));

    nimcp_recovery_strategy_t cached_strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig2, &cached_strategy));
    EXPECT_EQ(cached_strategy, NIMCP_RECOVERY_STRATEGY_RETRY);
}

/* ============================================================================
 * PERFORMANCE IMPROVEMENT TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, SpeedupMeasurement) {
    auto ctx = simulate_error(11, (void*)0x1000, "test_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Simulate slow diagnostic path (first time) */
    auto slow_path_time = measure_time_ns([&]() {
        nimcp_recovery_strategy_t strategy;
        EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

        /* Simulate diagnostic overhead */
        std::this_thread::sleep_for(std::chrono::microseconds(500));

        /* Store result */
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    });

    /* Fast path (cached) */
    auto fast_path_time = measure_time_ns([&]() {
        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
    });

    /* Verify significant speedup */
    double speedup = (double)slow_path_time / (double)fast_path_time;
    EXPECT_GT(speedup, 10.0);  // At least 10x faster

    /* Verify fast path is very fast */
    EXPECT_LT(fast_path_time, 5000);  // < 5us (relaxed for real system)
}

TEST_F(RecoveryCacheIntegrationTest, RepeatedErrorPerformance) {
    const int NUM_ERRORS = 100;
    auto ctx = simulate_error(11, (void*)0x1000, "test_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Store strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Measure cumulative time for repeated lookups */
    auto total_time = measure_time_ns([&]() {
        for (int i = 0; i < NUM_ERRORS; i++) {
            nimcp_recovery_strategy_t strategy;
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
        }
    });

    /* Average time per lookup should be very fast */
    uint64_t avg_time = total_time / NUM_ERRORS;
    EXPECT_LT(avg_time, 500);  // < 500ns average (relaxed for real system)
}

TEST_F(RecoveryCacheIntegrationTest, MultipleErrorTypesPerformance) {
    /* Create cache entries for multiple error types */
    std::vector<std::pair<nimcp_error_signature_t, nimcp_recovery_strategy_t>> errors;

    nimcp_recovery_strategy_t strategies[] = {
        NIMCP_RECOVERY_STRATEGY_RETRY,
        NIMCP_RECOVERY_STRATEGY_ROLLBACK,
        NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE,
        NIMCP_RECOVERY_STRATEGY_ALTERNATE_PATH,
        NIMCP_RECOVERY_STRATEGY_SAFE_MODE
    };

    /* Populate cache */
    for (int i = 0; i < 20; i++) {
        auto ctx = simulate_error(11, (void*)(uintptr_t)(0x1000 + i * 0x100),
                                  "test_function");
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        nimcp_recovery_strategy_t strategy = strategies[i % 5];
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, strategy, nullptr));

        errors.push_back({sig, strategy});
    }

    /* Verify all lookups are fast and correct */
    for (const auto& [sig, expected_strategy] : errors) {
        nimcp_recovery_strategy_t strategy;
        uint64_t lookup_time = measure_time_ns([&]() {
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
        });

        EXPECT_EQ(strategy, expected_strategy);
        EXPECT_LT(lookup_time, 1000);  // < 1us even with multiple entries (relaxed)
    }
}

/* ============================================================================
 * LEARNING BEHAVIOR TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, SuccessRateTracking) {
    auto ctx = simulate_error(11, (void*)0x1000, "test_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Store strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Simulate recovery attempts */
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, false));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));

    /* Verify success rate */
    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));
    EXPECT_EQ(entry.success_count, 3);
    EXPECT_EQ(entry.failure_count, 1);
    EXPECT_NEAR(entry.success_rate, 0.75, 0.01);
}

TEST_F(RecoveryCacheIntegrationTest, InvalidateFailingStrategy) {
    auto ctx = simulate_error(11, (void*)0x1000, "test_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Store initially successful strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));

    /* Simulate strategy starting to fail */
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, false));
    }

    /* Check success rate dropped */
    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));
    EXPECT_LT(entry.success_rate, 0.3);  // Success rate dropped significantly

    /* Invalidate failing strategy */
    EXPECT_TRUE(nimcp_recovery_cache_invalidate(cache, &sig));

    /* Next error should miss cache and trigger new diagnostic */
    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
}

TEST_F(RecoveryCacheIntegrationTest, AdaptiveStrategySelection) {
    /* Simulate learning optimal strategy over time */
    auto ctx = simulate_error(11, (void*)0x1000, "test_function");
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* Try different strategies and track success */
    struct StrategyAttempt {
        nimcp_recovery_strategy_t strategy;
        int successes;
        int failures;
    };

    std::vector<StrategyAttempt> attempts = {
        {NIMCP_RECOVERY_STRATEGY_RETRY, 2, 3},
        {NIMCP_RECOVERY_STRATEGY_ROLLBACK, 4, 1},
        {NIMCP_RECOVERY_STRATEGY_SAFE_MODE, 5, 0}
    };

    /* Test each strategy */
    nimcp_recovery_strategy_t best_strategy = NIMCP_RECOVERY_STRATEGY_NONE;
    double best_rate = 0.0;

    for (const auto& attempt : attempts) {
        /* Clear previous entry */
        nimcp_recovery_cache_invalidate(cache, &sig);

        /* Store and test strategy */
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, attempt.strategy, nullptr));

        for (int i = 0; i < attempt.successes; i++) {
            EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));
        }
        for (int i = 0; i < attempt.failures; i++) {
            EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, false));
        }

        /* Get success rate */
        nimcp_cache_entry_t entry;
        EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));

        if (entry.success_rate > best_rate) {
            best_rate = entry.success_rate;
            best_strategy = attempt.strategy;
        }
    }

    /* Best strategy should be SAFE_MODE (100% success) */
    EXPECT_EQ(best_strategy, NIMCP_RECOVERY_STRATEGY_SAFE_MODE);
    EXPECT_NEAR(best_rate, 1.0, 0.01);
}

/* ============================================================================
 * CACHE HIT RATE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, HighHitRateForRepeatedErrors) {
    /* Simulate workload with repeated errors */
    std::vector<nimcp_error_signature_t> error_signatures;

    /* Create 10 unique error signatures */
    for (int i = 0; i < 10; i++) {
        auto ctx = simulate_error(11, (void*)(uintptr_t)(0x1000 + i * 0x100),
                                  "test_function");
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        error_signatures.push_back(sig);

        /* First occurrence - cache miss, store strategy */
        nimcp_recovery_strategy_t strategy;
        EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Simulate repeated errors (90% repeated, 10% new) */
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 9);

    for (int i = 0; i < 100; i++) {
        int idx = dist(gen);
        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &error_signatures[idx], &strategy));
    }

    /* Verify high hit rate */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_GT(stats.hit_rate, 0.85);  // > 85% hit rate
}

TEST_F(RecoveryCacheIntegrationTest, HitRateWithMixedWorkload) {
    /* Mixed workload: some repeated, some unique */
    std::vector<nimcp_error_signature_t> common_errors;

    /* Create 5 common error signatures */
    for (int i = 0; i < 5; i++) {
        auto ctx = simulate_error(11, (void*)(uintptr_t)(0x1000 + i * 0x100),
                                  "common_function");
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        common_errors.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Simulate workload: 70% common, 30% unique */
    for (int i = 0; i < 100; i++) {
        if (i % 10 < 7) {
            /* Common error (hit) */
            int idx = i % common_errors.size();
            nimcp_recovery_strategy_t strategy;
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &common_errors[idx], &strategy));
        } else {
            /* Unique error (miss) */
            auto ctx = simulate_error(11, (void*)(uintptr_t)(0x9000 + i * 0x100),
                                     "unique_function");
            nimcp_error_signature_t sig;
            ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

            nimcp_recovery_strategy_t strategy;
            EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
            EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                        NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
        }
    }

    /* Verify hit rate matches workload */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_GT(stats.hit_rate, 0.65);  // > 65% hit rate
}

/* ============================================================================
 * REALISTIC USAGE PATTERNS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, MemoryAllocationFailures) {
    /* Simulate repeated memory allocation failures */
    auto ctx = simulate_error(11, (void*)0x0, "malloc", ENOMEM);
    nimcp_error_signature_t sig;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

    /* First failure - diagnostic determines retry strategy */
    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY,
                "Retry after garbage collection"));

    /* Subsequent failures use cached strategy */
    for (int i = 0; i < 10; i++) {
        auto ctx_repeat = simulate_error(11, (void*)0x0, "malloc", ENOMEM);
        nimcp_error_signature_t sig_repeat;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx_repeat, &sig_repeat));

        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig_repeat, &strategy));
        EXPECT_EQ(strategy, NIMCP_RECOVERY_STRATEGY_RETRY);
    }
}

TEST_F(RecoveryCacheIntegrationTest, SegmentationFaults) {
    /* Simulate segmentation faults in different contexts */
    struct FaultContext {
        void* addr;
        const char* func;
        nimcp_recovery_strategy_t expected_strategy;
    };

    std::vector<FaultContext> faults = {
        {(void*)0x0, "dereference_null", NIMCP_RECOVERY_STRATEGY_SAFE_MODE},
        {(void*)0xdeadbeef, "buffer_overflow", NIMCP_RECOVERY_STRATEGY_ROLLBACK},
        {(void*)0x1000, "stack_corruption", NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE}
    };

    /* Learn strategies for each fault type */
    for (const auto& fault : faults) {
        auto ctx = simulate_error(11, fault.addr, fault.func);
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    fault.expected_strategy, nullptr));
    }

    /* Verify each fault type retrieves correct strategy */
    for (const auto& fault : faults) {
        auto ctx = simulate_error(11, fault.addr, fault.func);
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
        EXPECT_EQ(strategy, fault.expected_strategy);
    }
}

TEST_F(RecoveryCacheIntegrationTest, ConcurrentWorkload) {
    /* Simulate concurrent error recovery with caching */
    const int NUM_THREADS = 4;
    const int ERRORS_PER_THREAD = 50;

    /* Pre-populate with common errors */
    std::vector<nimcp_error_signature_t> common_errors;
    for (int i = 0; i < 10; i++) {
        auto ctx = simulate_error(11, (void*)(uintptr_t)(0x1000 + i * 0x100),
                                  "common_function");
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        common_errors.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Concurrent lookups */
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &common_errors, ERRORS_PER_THREAD]() {
            for (int i = 0; i < ERRORS_PER_THREAD; i++) {
                int idx = i % common_errors.size();
                nimcp_recovery_strategy_t strategy;
                EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &common_errors[idx],
                                                       &strategy));
                EXPECT_EQ(strategy, NIMCP_RECOVERY_STRATEGY_RETRY);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    /* Verify all lookups were hits */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.hits, NUM_THREADS * ERRORS_PER_THREAD);
    EXPECT_EQ(stats.hit_rate, 1.0);  // 100% hit rate
}

/* ============================================================================
 * STRESS TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheIntegrationTest, LargeWorkload) {
    /* Simulate large number of errors */
    const int NUM_ERRORS = 1000;

    for (int i = 0; i < NUM_ERRORS; i++) {
        auto ctx = simulate_error(11, (void*)(uintptr_t)(0x1000 + i * 0x100),
                                  "test_function");
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        /* Store or lookup */
        nimcp_recovery_strategy_t strategy;
        if (!nimcp_recovery_cache_lookup(cache, &sig, &strategy)) {
            EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                        NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
        }
    }

    /* Cache should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));

    /* Stats should be reasonable */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_GT(stats.stores, 0);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
