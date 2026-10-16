/**
 * @file test_recovery_cache_regression.cpp
 * @brief Regression tests for recovery cache
 *
 * Test Coverage:
 * - Performance benchmarks (lookup, store times)
 * - Hit rate stability
 * - Memory usage
 * - Thread safety under load
 * - Edge case handling
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_recovery_cache.h"

/* ============================================================================
 * TEST FIXTURE
 * ============================================================================ */

class RecoveryCacheRegressionTest : public ::testing::Test {
protected:
    nimcp_recovery_cache_t* cache;

    void SetUp() override {
        cache = nimcp_recovery_cache_create(0);
        ASSERT_NE(cache, nullptr);
    }

    void TearDown() override {
        if (cache) {
            nimcp_recovery_cache_destroy(cache);
            cache = nullptr;
        }
    }

    nimcp_signal_error_context_t create_context(int signal, void* addr) {
        nimcp_signal_error_context_t ctx = {0};
        ctx.signal = signal;
        ctx.fault_address = addr;
        ctx.context_hash = 0x12345678;
        ctx.function_name = "test_function";
        ctx.error_code = 0;
        return ctx;
    }

    uint64_t measure_time_ns(std::function<void()> operation) {
        auto start = std::chrono::high_resolution_clock::now();
        operation();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    double compute_stddev(const std::vector<uint64_t>& values) {
        if (values.empty()) return 0.0;

        double mean = 0.0;
        for (auto v : values) mean += v;
        mean /= values.size();

        double variance = 0.0;
        for (auto v : values) {
            double diff = v - mean;
            variance += diff * diff;
        }
        variance /= values.size();

        return std::sqrt(variance);
    }
};

/* ============================================================================
 * PERFORMANCE BENCHMARK TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, LookupTimeBenchmark) {
    /* Benchmark: Lookup performance with realistic thresholds */
    const int NUM_ENTRIES = 100;
    const int NUM_WARMUP = 200;
    const int NUM_LOOKUPS = 1000;

    /* Populate cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < NUM_ENTRIES; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Warmup phase to eliminate cold cache effects */
    for (int i = 0; i < NUM_WARMUP; i++) {
        int idx = i % signatures.size();
        nimcp_recovery_strategy_t strategy;
        nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy);
    }

    /* Measure lookup times */
    std::vector<uint64_t> lookup_times;
    for (int i = 0; i < NUM_LOOKUPS; i++) {
        int idx = i % signatures.size();
        nimcp_recovery_strategy_t strategy;

        auto time = measure_time_ns([&]() {
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy));
        });

        lookup_times.push_back(time);
    }

    /* Compute statistics */
    std::sort(lookup_times.begin(), lookup_times.end());
    uint64_t median = lookup_times[lookup_times.size() / 2];
    uint64_t p95 = lookup_times[(lookup_times.size() * 95) / 100];
    uint64_t p99 = lookup_times[(lookup_times.size() * 99) / 100];

    /* Verify performance targets (relaxed for system variability) */
    EXPECT_LT(median, 300) << "Median lookup time should be < 300ns";
    EXPECT_LT(p95, 600) << "95th percentile should be < 600ns";
    EXPECT_LT(p99, 10000) << "99th percentile should be < 10000ns";

    /* Verify from stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_LT(stats.avg_lookup_time_ns, 450);
}

TEST_F(RecoveryCacheRegressionTest, StoreTimeBenchmark) {
    /* Benchmark: Store should be < 1us */
    const int NUM_STORES = 100;

    std::vector<uint64_t> store_times;
    for (int i = 0; i < NUM_STORES; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        auto time = measure_time_ns([&]() {
            EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                        NIMCP_RECOVERY_STRATEGY_RETRY, "Test store"));
        });

        store_times.push_back(time);
    }

    /* Compute statistics */
    std::sort(store_times.begin(), store_times.end());
    uint64_t median = store_times[store_times.size() / 2];
    uint64_t p95 = store_times[(store_times.size() * 95) / 100];

    /* Verify performance targets */
    EXPECT_LT(median, 1000) << "Median store time should be < 1us";
    EXPECT_LT(p95, 2000) << "95th percentile should be < 2us";

    /* Verify from stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_LT(stats.avg_store_time_ns, 1500);
}

TEST_F(RecoveryCacheRegressionTest, SignatureComputationBenchmark) {
    /* Benchmark: Signature computation performance */
    const int NUM_WARMUP = 100;
    const int NUM_COMPUTATIONS = 1000;

    auto ctx = create_context(11, (void*)0x1000);

    /* Warmup phase */
    for (int i = 0; i < NUM_WARMUP; i++) {
        nimcp_error_signature_t sig;
        nimcp_recovery_cache_compute_signature(&ctx, &sig);
    }

    std::vector<uint64_t> compute_times;

    for (int i = 0; i < NUM_COMPUTATIONS; i++) {
        nimcp_error_signature_t sig;
        auto time = measure_time_ns([&]() {
            EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        });
        compute_times.push_back(time);
    }

    /* Compute statistics */
    std::sort(compute_times.begin(), compute_times.end());
    uint64_t median = compute_times[compute_times.size() / 2];
    uint64_t p95 = compute_times[(compute_times.size() * 95) / 100];

    /* Verify performance targets (relaxed for system variability and test suite load) */
    EXPECT_LT(median, 350) << "Median signature computation should be < 350ns";
    EXPECT_LT(p95, 700) << "95th percentile should be < 700ns";
}

/* ============================================================================
 * HIT RATE STABILITY TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, HitRateStability) {
    /* Regression: Hit rate should be stable over time */
    const int NUM_UNIQUE = 50;
    const int NUM_ROUNDS = 10;

    /* Create unique error signatures */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < NUM_UNIQUE; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Measure hit rate over multiple rounds */
    std::vector<double> hit_rates;
    for (int round = 0; round < NUM_ROUNDS; round++) {
        nimcp_recovery_cache_reset_stats(cache);

        /* Perform lookups */
        for (int i = 0; i < 1000; i++) {
            int idx = i % signatures.size();
            nimcp_recovery_strategy_t strategy;
            EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy));
        }

        /* Get hit rate */
        nimcp_recovery_cache_stats_t stats;
        EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
        hit_rates.push_back(stats.hit_rate);
    }

    /* Verify all hit rates are 100% */
    for (double rate : hit_rates) {
        EXPECT_NEAR(rate, 1.0, 0.01);
    }

    /* Verify stability (low variance) */
    double stddev = compute_stddev(
        std::vector<uint64_t>(hit_rates.begin(), hit_rates.end())
    );
    EXPECT_LT(stddev, 0.01);
}

TEST_F(RecoveryCacheRegressionTest, HitRateWithEviction) {
    /* Regression: Hit rate should remain high even with eviction */
    const int CAPACITY = 50;
    cache = nimcp_recovery_cache_create(CAPACITY);
    ASSERT_NE(cache, nullptr);

    /* Create more signatures than capacity */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < CAPACITY * 2; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
    }

    /* Simulate workload with hot set (frequently accessed) */
    std::vector<nimcp_error_signature_t> hot_set(
        signatures.begin(), signatures.begin() + CAPACITY / 2
    );

    /* 80% accesses to hot set, 20% to cold set */
    nimcp_recovery_cache_reset_stats(cache);
    for (int i = 0; i < 1000; i++) {
        nimcp_error_signature_t* sig;
        if (i % 10 < 8) {
            /* Hot set */
            int idx = i % hot_set.size();
            sig = &hot_set[idx];
        } else {
            /* Cold set */
            int idx = (CAPACITY / 2) + (i % (signatures.size() - CAPACITY / 2));
            sig = &signatures[idx];
        }

        /* Lookup or store */
        nimcp_recovery_strategy_t strategy;
        if (!nimcp_recovery_cache_lookup(cache, sig, &strategy)) {
            EXPECT_TRUE(nimcp_recovery_cache_store(cache, sig,
                        NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
        }
    }

    /* Hit rate should be high for hot set */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_GT(stats.hit_rate, 0.7);  // > 70% hit rate
}

/* ============================================================================
 * MEMORY USAGE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, MemoryBoundedByCapacity) {
    /* Regression: Cache should not grow beyond capacity */
    const int CAPACITY = 100;
    nimcp_recovery_cache_destroy(cache);
    cache = nimcp_recovery_cache_create(CAPACITY);
    ASSERT_NE(cache, nullptr);

    /* Add more entries than capacity */
    for (int i = 0; i < CAPACITY * 2; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Verify size is bounded */
    EXPECT_EQ(cache->current_size, CAPACITY);

    /* Verify stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.current_size, CAPACITY);
    EXPECT_GT(stats.evictions, 0);  // Evictions should have occurred
}

TEST_F(RecoveryCacheRegressionTest, NoMemoryLeaksOnResize) {
    /* Regression: Resizing should not leak memory */
    const int INITIAL_SIZE = 100;

    /* Fill cache */
    for (int i = 0; i < INITIAL_SIZE; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Resize multiple times */
    EXPECT_TRUE(nimcp_recovery_cache_resize(cache, 200));
    EXPECT_TRUE(nimcp_recovery_cache_resize(cache, 50));
    EXPECT_TRUE(nimcp_recovery_cache_resize(cache, 150));

    /* Verify cache is still valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
    EXPECT_LE(cache->current_size, cache->capacity);
}

/* ============================================================================
 * THREAD SAFETY UNDER LOAD
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, ConcurrentStressTest) {
    /* Regression: Cache should be thread-safe under heavy load */
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 500;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, OPS_PER_THREAD, &errors]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                auto ctx = create_context(11,
                    (void*)(uintptr_t)(0x1000 + ((t * OPS_PER_THREAD + i) % 100) * 0x100));
                nimcp_error_signature_t sig;
                if (!nimcp_recovery_cache_compute_signature(&ctx, &sig)) {
                    errors++;
                    continue;
                }

                /* Mix of operations */
                if (i % 3 == 0) {
                    /* Store */
                    if (!nimcp_recovery_cache_store(cache, &sig,
                                                   NIMCP_RECOVERY_STRATEGY_RETRY, nullptr)) {
                        errors++;
                    }
                } else if (i % 3 == 1) {
                    /* Lookup */
                    nimcp_recovery_strategy_t strategy;
                    nimcp_recovery_cache_lookup(cache, &sig, &strategy);
                } else {
                    /* Update success */
                    nimcp_recovery_cache_update_success(cache, &sig, (i % 2) == 0);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    /* No errors should have occurred */
    EXPECT_EQ(errors, 0);

    /* Cache should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
}

TEST_F(RecoveryCacheRegressionTest, ConcurrentInvalidationStressTest) {
    /* Regression: Concurrent invalidations should be safe */
    const int NUM_ENTRIES = 100;

    /* Populate cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < NUM_ENTRIES; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Concurrent invalidations */
    const int NUM_THREADS = 4;
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &signatures, t]() {
            for (size_t i = t; i < signatures.size(); i += 4) {
                nimcp_recovery_cache_invalidate(cache, &signatures[i]);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    /* All entries should be invalidated */
    EXPECT_EQ(cache->current_size, 0);

    /* Cache should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
}

/* ============================================================================
 * EDGE CASE HANDLING
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, HashCollisionHandling) {
    /* Regression: Cache should handle hash collisions correctly */
    /* Create entries that may collide in hash table */
    const int NUM_ENTRIES = 100;

    for (int i = 0; i < NUM_ENTRIES; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Verify all entries can be retrieved */
    for (int i = 0; i < NUM_ENTRIES; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));

        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
    }

    EXPECT_EQ(cache->current_size, NUM_ENTRIES);
}

TEST_F(RecoveryCacheRegressionTest, RapidClearOperations) {
    /* Regression: Rapid clears should be safe */
    for (int round = 0; round < 10; round++) {
        /* Fill cache */
        for (int i = 0; i < 50; i++) {
            auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
            nimcp_error_signature_t sig;
            ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
            EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                        NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
        }

        /* Clear */
        EXPECT_TRUE(nimcp_recovery_cache_clear(cache));
        EXPECT_EQ(cache->current_size, 0);
    }

    /* Cache should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
}

TEST_F(RecoveryCacheRegressionTest, LRUOrderMaintenance) {
    /* Regression: LRU order should be maintained correctly */
    const int CAPACITY = 5;
    nimcp_recovery_cache_destroy(cache);
    cache = nimcp_recovery_cache_create(CAPACITY);
    ASSERT_NE(cache, nullptr);

    /* Fill cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < CAPACITY; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Access entries in specific order */
    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[0], &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[2], &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[4], &strategy));

    /* Add new entry - should evict LRU (entry 1) */
    auto ctx_new = create_context(11, (void*)0x9000);
    nimcp_error_signature_t sig_new;
    ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx_new, &sig_new));
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig_new,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Verify entry 1 was evicted */
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &signatures[1], &strategy));

    /* Verify others still present */
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[0], &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[2], &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[4], &strategy));
}

/* ============================================================================
 * PERFORMANCE CONSISTENCY TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheRegressionTest, ConsistentPerformanceOverTime) {
    /* Regression: Performance should not degrade over time */
    const int NUM_ROUNDS = 10;
    const int OPS_PER_ROUND = 100;

    std::vector<uint64_t> round_times;

    /* Create test signatures */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < 50; i++) {
        auto ctx = create_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        nimcp_error_signature_t sig;
        ASSERT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Measure performance over multiple rounds */
    for (int round = 0; round < NUM_ROUNDS; round++) {
        auto round_time = measure_time_ns([&]() {
            for (int i = 0; i < OPS_PER_ROUND; i++) {
                int idx = i % signatures.size();
                nimcp_recovery_strategy_t strategy;
                EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy));
            }
        });
        round_times.push_back(round_time);
    }

    /* Performance should be consistent (low variance) */
    double stddev = compute_stddev(round_times);
    uint64_t mean = 0;
    for (auto t : round_times) mean += t;
    mean /= round_times.size();

    /* Coefficient of variation should be low */
    double cv = stddev / mean;
    EXPECT_LT(cv, 0.2);  // < 20% variation
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
