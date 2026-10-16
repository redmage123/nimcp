/**
 * @file test_recovery_cache.cpp
 * @brief Unit tests for recovery cache (100% coverage target)
 *
 * Test Coverage:
 * - Cache lifecycle (create, destroy, clear)
 * - Signature operations (compute, compare)
 * - Cache operations (lookup, store, update, invalidate)
 * - LRU eviction
 * - Success tracking
 * - Thread safety
 * - Statistics
 * - Edge cases and error handling
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

class RecoveryCacheTest : public ::testing::Test {
protected:
    nimcp_recovery_cache_t* cache;

    void SetUp() override {
        cache = nullptr;
    }

    void TearDown() override {
        if (cache) {
            nimcp_recovery_cache_destroy(cache);
            cache = nullptr;
        }
    }

    /* Helper: Create test error context */
    nimcp_signal_error_context_t create_test_context(
        int signal = 11,
        void* addr = (void*)0x1000,
        const char* func = "test_function")
    {
        nimcp_signal_error_context_t ctx = {0};
        ctx.signal = signal;
        ctx.fault_address = addr;
        ctx.context_hash = 0x12345678;
        ctx.function_name = func;
        ctx.error_code = 0;
        return ctx;
    }

    /* Helper: Create signature from context */
    nimcp_error_signature_t create_signature(const nimcp_signal_error_context_t& ctx) {
        nimcp_error_signature_t sig;
        EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
        return sig;
    }
};

/* ============================================================================
 * CACHE LIFECYCLE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, CreateDefaultCapacity) {
    cache = nimcp_recovery_cache_create(0);
    ASSERT_NE(cache, nullptr);
    EXPECT_EQ(cache->capacity, NIMCP_RECOVERY_CACHE_DEFAULT_CAPACITY);
    EXPECT_EQ(cache->current_size, 0);
    EXPECT_TRUE(cache->enable_lru);
    EXPECT_TRUE(cache->track_stats);
}

TEST_F(RecoveryCacheTest, CreateCustomCapacity) {
    cache = nimcp_recovery_cache_create(512);
    ASSERT_NE(cache, nullptr);
    EXPECT_EQ(cache->capacity, 512);
    EXPECT_EQ(cache->current_size, 0);
}

TEST_F(RecoveryCacheTest, DestroyNullCache) {
    nimcp_recovery_cache_destroy(nullptr);  // Should not crash
}

TEST_F(RecoveryCacheTest, DestroyEmptyCache) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);
    nimcp_recovery_cache_destroy(cache);
    cache = nullptr;  // Prevent double-free in TearDown
}

TEST_F(RecoveryCacheTest, DestroyNonEmptyCache) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Add some entries */
    auto ctx = create_test_context();
    auto sig = create_signature(ctx);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, NIMCP_RECOVERY_STRATEGY_RETRY, "test"));

    /* Should log warning but still destroy */
    nimcp_recovery_cache_destroy(cache);
    cache = nullptr;
}

TEST_F(RecoveryCacheTest, ClearCache) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Add entries */
    for (int i = 0; i < 5; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }
    EXPECT_EQ(cache->current_size, 5);

    /* Clear */
    EXPECT_TRUE(nimcp_recovery_cache_clear(cache));
    EXPECT_EQ(cache->current_size, 0);

    /* Verify empty */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.current_size, 0);
}

TEST_F(RecoveryCacheTest, ClearNullCache) {
    EXPECT_FALSE(nimcp_recovery_cache_clear(nullptr));
}

/* ============================================================================
 * SIGNATURE OPERATION TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, ComputeSignatureBasic) {
    auto ctx = create_test_context();
    nimcp_error_signature_t sig;

    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
    EXPECT_NE(sig.hash, 0);
    EXPECT_GT(sig.size, 0);
}

TEST_F(RecoveryCacheTest, ComputeSignatureDeterministic) {
    auto ctx = create_test_context();

    nimcp_error_signature_t sig1, sig2;
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig1));
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig2));

    /* Same context should produce same signature */
    EXPECT_EQ(sig1.hash, sig2.hash);
    EXPECT_EQ(sig1.size, sig2.size);
    EXPECT_EQ(memcmp(sig1.data, sig2.data, sig1.size), 0);
}

TEST_F(RecoveryCacheTest, ComputeSignatureDifferentSignals) {
    auto ctx1 = create_test_context(11);
    auto ctx2 = create_test_context(6);

    nimcp_error_signature_t sig1, sig2;
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx1, &sig1));
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx2, &sig2));

    /* Different signals should produce different signatures */
    EXPECT_NE(sig1.hash, sig2.hash);
}

TEST_F(RecoveryCacheTest, ComputeSignatureDifferentAddresses) {
    auto ctx1 = create_test_context(11, (void*)0x1000);
    auto ctx2 = create_test_context(11, (void*)0x2000);

    nimcp_error_signature_t sig1, sig2;
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx1, &sig1));
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx2, &sig2));

    /* Different addresses should produce different signatures */
    EXPECT_NE(sig1.hash, sig2.hash);
}

TEST_F(RecoveryCacheTest, ComputeSignatureWithoutFunctionName) {
    auto ctx = create_test_context(11, (void*)0x1000, nullptr);

    nimcp_error_signature_t sig;
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig));
    EXPECT_NE(sig.hash, 0);
}

TEST_F(RecoveryCacheTest, ComputeSignatureNullInputs) {
    nimcp_signal_error_context_t ctx = create_test_context();
    nimcp_error_signature_t sig;

    EXPECT_FALSE(nimcp_recovery_cache_compute_signature(nullptr, &sig));
    EXPECT_FALSE(nimcp_recovery_cache_compute_signature(&ctx, nullptr));
    EXPECT_FALSE(nimcp_recovery_cache_compute_signature(nullptr, nullptr));
}

TEST_F(RecoveryCacheTest, SignaturesEqual) {
    auto ctx = create_test_context();
    nimcp_error_signature_t sig1, sig2;

    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig1));
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx, &sig2));

    EXPECT_TRUE(nimcp_recovery_cache_signatures_equal(&sig1, &sig2));
}

TEST_F(RecoveryCacheTest, SignaturesNotEqual) {
    auto ctx1 = create_test_context(11);
    auto ctx2 = create_test_context(6);

    nimcp_error_signature_t sig1, sig2;
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx1, &sig1));
    EXPECT_TRUE(nimcp_recovery_cache_compute_signature(&ctx2, &sig2));

    EXPECT_FALSE(nimcp_recovery_cache_signatures_equal(&sig1, &sig2));
}

TEST_F(RecoveryCacheTest, SignaturesEqualNullInputs) {
    nimcp_error_signature_t sig;
    EXPECT_FALSE(nimcp_recovery_cache_signatures_equal(nullptr, &sig));
    EXPECT_FALSE(nimcp_recovery_cache_signatures_equal(&sig, nullptr));
    EXPECT_FALSE(nimcp_recovery_cache_signatures_equal(nullptr, nullptr));
}

/* ============================================================================
 * CACHE OPERATION TESTS - LOOKUP/STORE
 * ============================================================================ */

TEST_F(RecoveryCacheTest, LookupMiss) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* Verify stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.misses, 1);
    EXPECT_EQ(stats.hits, 0);
}

TEST_F(RecoveryCacheTest, StoreAndLookupHit) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Store */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, "Test retry"));

    /* Lookup */
    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
    EXPECT_EQ(strategy, NIMCP_RECOVERY_STRATEGY_RETRY);

    /* Verify stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.stores, 1);
}

TEST_F(RecoveryCacheTest, StoreMultipleStrategies) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Store different strategies for different errors */
    nimcp_recovery_strategy_t strategies[] = {
        NIMCP_RECOVERY_STRATEGY_RETRY,
        NIMCP_RECOVERY_STRATEGY_ROLLBACK,
        NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE,
        NIMCP_RECOVERY_STRATEGY_ALTERNATE_PATH
    };

    for (int i = 0; i < 4; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig, strategies[i], nullptr));
    }

    /* Verify all can be retrieved */
    for (int i = 0; i < 4; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);

        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
        EXPECT_EQ(strategy, strategies[i]);
    }
}

TEST_F(RecoveryCacheTest, StoreUpdateExisting) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Store initial strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, "First"));
    EXPECT_EQ(cache->current_size, 1);

    /* Update with different strategy */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_ROLLBACK, "Second"));
    EXPECT_EQ(cache->current_size, 1);  // Size shouldn't increase

    /* Verify updated strategy */
    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));
    EXPECT_EQ(strategy, NIMCP_RECOVERY_STRATEGY_ROLLBACK);
}

TEST_F(RecoveryCacheTest, StoreNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    EXPECT_FALSE(nimcp_recovery_cache_store(nullptr, &sig,
                 NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_FALSE(nimcp_recovery_cache_store(cache, nullptr,
                 NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
}

TEST_F(RecoveryCacheTest, LookupNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);
    nimcp_recovery_strategy_t strategy;

    EXPECT_FALSE(nimcp_recovery_cache_lookup(nullptr, &sig, &strategy));
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, nullptr, &strategy));
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, nullptr));
}

/* ============================================================================
 * LRU EVICTION TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, LRUEvictionWhenFull) {
    cache = nimcp_recovery_cache_create(3);  // Small capacity
    ASSERT_NE(cache, nullptr);

    /* Fill cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < 3; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }
    EXPECT_EQ(cache->current_size, 3);

    /* Add one more - should evict LRU */
    auto ctx_new = create_test_context(11, (void*)0x9000);
    auto sig_new = create_signature(ctx_new);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig_new,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    EXPECT_EQ(cache->current_size, 3);  // Size should stay at capacity

    /* Verify eviction occurred */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.evictions, 1);
}

TEST_F(RecoveryCacheTest, LRUOrderMaintained) {
    cache = nimcp_recovery_cache_create(3);
    ASSERT_NE(cache, nullptr);

    /* Add 3 entries */
    auto ctx1 = create_test_context(11, (void*)0x1000);
    auto ctx2 = create_test_context(11, (void*)0x2000);
    auto ctx3 = create_test_context(11, (void*)0x3000);

    auto sig1 = create_signature(ctx1);
    auto sig2 = create_signature(ctx2);
    auto sig3 = create_signature(ctx3);

    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig1,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig2,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig3,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Access sig1 to move it to front */
    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig1, &strategy));

    /* Add new entry - sig2 should be evicted (oldest non-accessed) */
    auto ctx4 = create_test_context(11, (void*)0x4000);
    auto sig4 = create_signature(ctx4);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig4,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Verify sig2 was evicted */
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig2, &strategy));

    /* Verify others still present */
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig1, &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig3, &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig4, &strategy));
}

/* ============================================================================
 * SUCCESS TRACKING TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, UpdateSuccessTracking) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Store entry */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    /* Record successes */
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, true));
    EXPECT_TRUE(nimcp_recovery_cache_update_success(cache, &sig, false));

    /* Get entry and verify tracking */
    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));
    EXPECT_EQ(entry.success_count, 2);
    EXPECT_EQ(entry.failure_count, 1);
    EXPECT_NEAR(entry.success_rate, 2.0/3.0, 0.01);
}

TEST_F(RecoveryCacheTest, UpdateSuccessNonExistentEntry) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Try to update non-existent entry */
    EXPECT_FALSE(nimcp_recovery_cache_update_success(cache, &sig, true));
}

TEST_F(RecoveryCacheTest, UpdateSuccessNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    EXPECT_FALSE(nimcp_recovery_cache_update_success(nullptr, &sig, true));
    EXPECT_FALSE(nimcp_recovery_cache_update_success(cache, nullptr, true));
}

/* ============================================================================
 * INVALIDATION TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, InvalidateEntry) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Store and verify */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_EQ(cache->current_size, 1);

    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* Invalidate */
    EXPECT_TRUE(nimcp_recovery_cache_invalidate(cache, &sig));
    EXPECT_EQ(cache->current_size, 0);

    /* Verify removed */
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* Verify stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.invalidations, 1);
}

TEST_F(RecoveryCacheTest, InvalidateNonExistent) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    EXPECT_FALSE(nimcp_recovery_cache_invalidate(cache, &sig));
}

TEST_F(RecoveryCacheTest, InvalidateNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    EXPECT_FALSE(nimcp_recovery_cache_invalidate(nullptr, &sig));
    EXPECT_FALSE(nimcp_recovery_cache_invalidate(cache, nullptr));
}

/* ============================================================================
 * ENTRY RETRIEVAL TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, GetEntryDetails) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Store with description */
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE,
                "Restore from checkpoint"));

    /* Get entry */
    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));

    EXPECT_EQ(entry.strategy, NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE);
    EXPECT_STREQ(entry.strategy_desc, "Restore from checkpoint");
    EXPECT_GT(entry.created_timestamp, 0);
    EXPECT_GT(entry.last_access_timestamp, 0);
}

TEST_F(RecoveryCacheTest, GetEntryNonExistent) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    nimcp_cache_entry_t entry;
    EXPECT_FALSE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));
}

TEST_F(RecoveryCacheTest, GetEntryNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);
    nimcp_cache_entry_t entry;

    EXPECT_FALSE(nimcp_recovery_cache_get_entry(nullptr, &sig, &entry));
    EXPECT_FALSE(nimcp_recovery_cache_get_entry(cache, nullptr, &entry));
    EXPECT_FALSE(nimcp_recovery_cache_get_entry(cache, &sig, nullptr));
}

/* ============================================================================
 * STATISTICS TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, GetStatistics) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));

    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
    EXPECT_EQ(stats.stores, 0);
    EXPECT_EQ(stats.current_size, 0);
    EXPECT_EQ(stats.max_size, 10);
}

TEST_F(RecoveryCacheTest, StatisticsAccuracy) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Perform operations */
    auto ctx1 = create_test_context(11, (void*)0x1000);
    auto sig1 = create_signature(ctx1);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig1,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig1, &strategy));  // Hit

    auto ctx2 = create_test_context(11, (void*)0x2000);
    auto sig2 = create_signature(ctx2);
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig2, &strategy));  // Miss

    /* Check stats */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));

    EXPECT_EQ(stats.stores, 1);
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);
    EXPECT_NEAR(stats.hit_rate, 0.5, 0.01);
}

TEST_F(RecoveryCacheTest, ResetStatistics) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Generate some stats */
    auto ctx = create_test_context();
    auto sig = create_signature(ctx);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));

    nimcp_recovery_strategy_t strategy;
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig, &strategy));

    /* Reset */
    EXPECT_TRUE(nimcp_recovery_cache_reset_stats(cache));

    /* Verify reset */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 0);
    EXPECT_EQ(stats.stores, 0);
}

TEST_F(RecoveryCacheTest, GetStatsNullInputs) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    nimcp_recovery_cache_stats_t stats;
    EXPECT_FALSE(nimcp_recovery_cache_get_stats(nullptr, &stats));
    EXPECT_FALSE(nimcp_recovery_cache_get_stats(cache, nullptr));
}

TEST_F(RecoveryCacheTest, ResetStatsNullCache) {
    EXPECT_FALSE(nimcp_recovery_cache_reset_stats(nullptr));
}

/* ============================================================================
 * RESIZE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, ResizeExpand) {
    cache = nimcp_recovery_cache_create(5);
    ASSERT_NE(cache, nullptr);

    EXPECT_TRUE(nimcp_recovery_cache_resize(cache, 10));
    EXPECT_EQ(cache->capacity, 10);
}

TEST_F(RecoveryCacheTest, ResizeShrinkWithEviction) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Fill cache */
    for (int i = 0; i < 5; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }
    EXPECT_EQ(cache->current_size, 5);

    /* Shrink to 3 - should evict 2 entries */
    EXPECT_TRUE(nimcp_recovery_cache_resize(cache, 3));
    EXPECT_EQ(cache->capacity, 3);
    EXPECT_EQ(cache->current_size, 3);
}

TEST_F(RecoveryCacheTest, ResizeNullCache) {
    EXPECT_FALSE(nimcp_recovery_cache_resize(nullptr, 100));
}

TEST_F(RecoveryCacheTest, ResizeZeroCapacity) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    EXPECT_FALSE(nimcp_recovery_cache_resize(cache, 0));
}

/* ============================================================================
 * UTILITY FUNCTION TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, StrategyNames) {
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_NONE), "NONE");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_RETRY), "RETRY");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_ROLLBACK), "ROLLBACK");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_CHECKPOINT_RESTORE),
                 "CHECKPOINT_RESTORE");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_ALTERNATE_PATH),
                 "ALTERNATE_PATH");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_SAFE_MODE), "SAFE_MODE");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_RESET), "RESET");
    EXPECT_STREQ(nimcp_recovery_strategy_name(NIMCP_RECOVERY_STRATEGY_CUSTOM), "CUSTOM");
    EXPECT_STREQ(nimcp_recovery_strategy_name((nimcp_recovery_strategy_t)999), "UNKNOWN");
}

TEST_F(RecoveryCacheTest, PrintStats) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Should not crash */
    nimcp_recovery_cache_print_stats(cache);
    nimcp_recovery_cache_print_stats(nullptr);
}

TEST_F(RecoveryCacheTest, ValidateConsistency) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    /* Empty cache should be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));

    /* Add entries */
    for (int i = 0; i < 5; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
}

TEST_F(RecoveryCacheTest, ValidateNullCache) {
    EXPECT_FALSE(nimcp_recovery_cache_validate(nullptr));
}

/* ============================================================================
 * THREAD SAFETY TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, ConcurrentStores) {
    cache = nimcp_recovery_cache_create(1000);
    ASSERT_NE(cache, nullptr);

    const int NUM_THREADS = 4;
    const int STORES_PER_THREAD = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, STORES_PER_THREAD]() {
            for (int i = 0; i < STORES_PER_THREAD; i++) {
                auto ctx = create_test_context(11,
                    (void*)(uintptr_t)(0x1000 + (t * 1000 + i) * 0x100));
                auto sig = create_signature(ctx);
                EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                            NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(cache->current_size, NUM_THREADS * STORES_PER_THREAD);
}

TEST_F(RecoveryCacheTest, ConcurrentLookupsAndStores) {
    cache = nimcp_recovery_cache_create(100);
    ASSERT_NE(cache, nullptr);

    /* Pre-populate cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < 50; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Concurrent lookups and stores */
    std::vector<std::thread> threads;

    /* Lookup threads */
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &signatures]() {
            for (int i = 0; i < 100; i++) {
                int idx = i % signatures.size();
                nimcp_recovery_strategy_t strategy;
                nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy);
            }
        });
    }

    /* Store threads */
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 50; i++) {
                auto ctx = create_test_context(11,
                    (void*)(uintptr_t)(0x9000 + (t * 100 + i) * 0x100));
                auto sig = create_signature(ctx);
                nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    /* Cache should still be valid */
    EXPECT_TRUE(nimcp_recovery_cache_validate(cache));
}

/* ============================================================================
 * PERFORMANCE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, LookupPerformance) {
    cache = nimcp_recovery_cache_create(1000);
    ASSERT_NE(cache, nullptr);

    /* Populate cache */
    std::vector<nimcp_error_signature_t> signatures;
    for (int i = 0; i < 100; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        signatures.push_back(sig);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Perform many lookups */
    for (int i = 0; i < 1000; i++) {
        int idx = i % signatures.size();
        nimcp_recovery_strategy_t strategy;
        EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &signatures[idx], &strategy));
    }

    /* Check average lookup time */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_LT(stats.avg_lookup_time_ns, 500);  // Should be < 500ns (relaxed for CI environments)
}

TEST_F(RecoveryCacheTest, StorePerformance) {
    cache = nimcp_recovery_cache_create(1000);
    ASSERT_NE(cache, nullptr);

    /* Perform many stores */
    for (int i = 0; i < 100; i++) {
        auto ctx = create_test_context(11, (void*)(uintptr_t)(0x1000 + i * 0x100));
        auto sig = create_signature(ctx);
        EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                    NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    }

    /* Check average store time */
    nimcp_recovery_cache_stats_t stats;
    EXPECT_TRUE(nimcp_recovery_cache_get_stats(cache, &stats));
    EXPECT_LT(stats.avg_store_time_ns, 1000);  // Should be < 1us
}

/* ============================================================================
 * EDGE CASE TESTS
 * ============================================================================ */

TEST_F(RecoveryCacheTest, SingleEntryCache) {
    cache = nimcp_recovery_cache_create(1);
    ASSERT_NE(cache, nullptr);

    auto ctx1 = create_test_context(11, (void*)0x1000);
    auto sig1 = create_signature(ctx1);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig1,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_EQ(cache->current_size, 1);

    /* Adding another should evict first */
    auto ctx2 = create_test_context(11, (void*)0x2000);
    auto sig2 = create_signature(ctx2);
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig2,
                NIMCP_RECOVERY_STRATEGY_RETRY, nullptr));
    EXPECT_EQ(cache->current_size, 1);

    /* First should be gone */
    nimcp_recovery_strategy_t strategy;
    EXPECT_FALSE(nimcp_recovery_cache_lookup(cache, &sig1, &strategy));
    EXPECT_TRUE(nimcp_recovery_cache_lookup(cache, &sig2, &strategy));
}

TEST_F(RecoveryCacheTest, LargeDescription) {
    cache = nimcp_recovery_cache_create(10);
    ASSERT_NE(cache, nullptr);

    auto ctx = create_test_context();
    auto sig = create_signature(ctx);

    /* Very long description (should be truncated) */
    std::string long_desc(500, 'X');
    EXPECT_TRUE(nimcp_recovery_cache_store(cache, &sig,
                NIMCP_RECOVERY_STRATEGY_RETRY, long_desc.c_str()));

    nimcp_cache_entry_t entry;
    EXPECT_TRUE(nimcp_recovery_cache_get_entry(cache, &sig, &entry));

    /* Should be truncated to max size */
    EXPECT_LT(strlen(entry.strategy_desc), NIMCP_RECOVERY_CACHE_MAX_STRATEGY_SIZE);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
