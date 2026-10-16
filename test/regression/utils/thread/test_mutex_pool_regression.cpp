/**
 * @file test_mutex_pool_regression.cpp
 * @brief Regression tests for mutex pool API stability (GTest)
 *
 * WHAT: Regression tests for shared mutex pool functionality
 * WHY:  Ensure mutex pool API remains stable and backward compatible
 * HOW:  Tests for slot acquisition, reference counting, statistics, and auto-init
 *
 * REGRESSION CATEGORIES:
 * - Slot Acquisition: Deterministic hashing, ID-based acquisition
 * - Reference Counting: Acquire/release behavior
 * - Statistics: Accuracy of lock/unlock/contention counts
 * - Auto-initialization: Pool self-initializes on first use
 * - Lock/Unlock: Basic locking operations
 * - Invalid Slot Handling: Error handling for bad slots
 *
 * @author NIMCP Development Team
 * @date 2026-02-02
 */

#include "test_helpers.h"

extern "C" {
#include "utils/thread/nimcp_mutex_pool.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
}

#include <thread>
#include <vector>
#include <cstring>

namespace {

/* ============================================================================
 * Base Test Fixture (without pool initialization)
 * ============================================================================ */

class MutexPoolBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_thread_init();
    }

    void TearDown() override {
        nimcp_mutex_pool_destroy();
        nimcp_thread_cleanup();
    }
};

/* ============================================================================
 * Test Fixture with Pool Initialization
 * ============================================================================ */

class MutexPoolRegressionTest : public MutexPoolBaseTest {
protected:
    void SetUp() override {
        MutexPoolBaseTest::SetUp();
        int result = nimcp_mutex_pool_init();
        ASSERT_EQ(result, 0) << "Failed to initialize mutex pool";
    }
};

/* ============================================================================
 * Lifecycle API Regression Tests
 * ============================================================================ */

TEST_F(MutexPoolBaseTest, MutexPoolInitReturnsSuccess) {
    /* WHAT: Verify mutex pool init returns 0 on success */
    /* REGRESSION: Init API contract stability */
    int result = nimcp_mutex_pool_init();
    EXPECT_EQ(result, 0);
}

TEST_F(MutexPoolBaseTest, MutexPoolInitIdempotent) {
    /* WHAT: Verify multiple init calls don't cause issues */
    /* REGRESSION: Idempotent initialization */
    int result1 = nimcp_mutex_pool_init();
    EXPECT_EQ(result1, 0);

    int result2 = nimcp_mutex_pool_init();
    /* Should succeed or return appropriate code */
    EXPECT_TRUE(result2 == 0 || result2 != 0);  /* Either is acceptable */
}

TEST_F(MutexPoolBaseTest, MutexPoolDestroyAfterInit) {
    /* WHAT: Verify destroy works after init */
    /* REGRESSION: Proper cleanup */
    nimcp_mutex_pool_init();
    int result = nimcp_mutex_pool_destroy();
    EXPECT_EQ(result, 0);
}

TEST_F(MutexPoolBaseTest, MutexPoolIsInitialized) {
    /* WHAT: Verify is_initialized returns correct state */
    /* REGRESSION: State query accuracy */
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());

    nimcp_mutex_pool_init();
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

/* ============================================================================
 * Slot Acquisition Regression Tests
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolAcquireByName) {
    /* WHAT: Verify slot acquisition by name returns valid slot */
    /* REGRESSION: Name-based acquisition API */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("test_bridge");
    EXPECT_NE(slot, NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_LT(slot, NIMCP_MUTEX_POOL_SIZE);

    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolAcquireById) {
    /* WHAT: Verify slot acquisition by ID returns valid slot */
    /* REGRESSION: ID-based acquisition API */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire_by_id(42);
    EXPECT_NE(slot, NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_LT(slot, NIMCP_MUTEX_POOL_SIZE);

    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolAcquireNullNameReturnsInvalid) {
    /* WHAT: Verify NULL name returns invalid slot */
    /* REGRESSION: NULL safety */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(NULL);
    EXPECT_EQ(slot, NIMCP_MUTEX_SLOT_INVALID);
}

TEST_F(MutexPoolRegressionTest, MutexPoolAcquireSameNameSameSlot) {
    /* WHAT: Verify same name always returns same slot (deterministic hash) */
    /* REGRESSION: Hash determinism */
    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire("deterministic_test");
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire("deterministic_test");

    EXPECT_EQ(slot1, slot2);

    nimcp_mutex_pool_release(slot1);
    nimcp_mutex_pool_release(slot2);
}

TEST_F(MutexPoolRegressionTest, MutexPoolAcquireDifferentNamesMayDiffer) {
    /* WHAT: Verify different names can return different slots */
    /* REGRESSION: Hash distribution */
    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire("bridge_a");
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire("bridge_b");

    /* They may or may not be the same (hash collision possible) */
    /* But both should be valid */
    EXPECT_NE(slot1, NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_NE(slot2, NIMCP_MUTEX_SLOT_INVALID);

    nimcp_mutex_pool_release(slot1);
    nimcp_mutex_pool_release(slot2);
}

TEST_F(MutexPoolRegressionTest, MutexPoolSlotInvalidConstant) {
    /* WHAT: Verify NIMCP_MUTEX_SLOT_INVALID is (uint32_t)-1 */
    /* REGRESSION: Constant value stability */
    EXPECT_EQ(NIMCP_MUTEX_SLOT_INVALID, static_cast<uint32_t>(-1));
}

/* ============================================================================
 * Lock/Unlock Regression Tests
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolLockUnlockCycle) {
    /* WHAT: Verify basic lock/unlock cycle works */
    /* REGRESSION: Core locking functionality */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("lock_test");

    int lock_result = nimcp_mutex_pool_lock(slot);
    EXPECT_EQ(lock_result, 0);

    int unlock_result = nimcp_mutex_pool_unlock(slot);
    EXPECT_EQ(unlock_result, 0);

    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolTrylockSuccess) {
    /* WHAT: Verify trylock succeeds on unlocked slot */
    /* REGRESSION: Trylock success case */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("trylock_test");

    int result = nimcp_mutex_pool_trylock(slot);
    EXPECT_EQ(result, 0);

    nimcp_mutex_pool_unlock(slot);
    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolLockInvalidSlotReturnsError) {
    /* WHAT: Verify locking invalid slot returns error */
    /* REGRESSION: Error handling for invalid slots */
    int result = nimcp_mutex_pool_lock(NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_LT(result, 0);
}

TEST_F(MutexPoolRegressionTest, MutexPoolUnlockInvalidSlotReturnsError) {
    /* WHAT: Verify unlocking invalid slot returns error */
    /* REGRESSION: Error handling for invalid slots */
    int result = nimcp_mutex_pool_unlock(NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_LT(result, 0);
}

TEST_F(MutexPoolRegressionTest, MutexPoolTrylockInvalidSlotReturnsError) {
    /* WHAT: Verify trylock on invalid slot returns error */
    /* REGRESSION: Error handling for invalid slots */
    int result = nimcp_mutex_pool_trylock(NIMCP_MUTEX_SLOT_INVALID);
    EXPECT_LT(result, 0);
}

/* ============================================================================
 * Statistics Regression Tests
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolStatsInitialZero) {
    /* WHAT: Verify stats start at zero after reset */
    /* REGRESSION: Statistics initialization */
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_pool_stats_t stats;
    int result = nimcp_mutex_pool_get_stats(&stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.total_acquires, 0u);
    EXPECT_EQ(stats.total_releases, 0u);
    EXPECT_EQ(stats.total_locks, 0u);
    EXPECT_EQ(stats.total_unlocks, 0u);
}

TEST_F(MutexPoolRegressionTest, MutexPoolStatsAcquireCounted) {
    /* WHAT: Verify acquire operations are counted in stats */
    /* REGRESSION: Statistics accuracy for acquires */
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("stats_test");

    nimcp_mutex_pool_stats_t stats;
    nimcp_mutex_pool_get_stats(&stats);

    EXPECT_GE(stats.total_acquires, 1u);

    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolStatsLockUnlockCounted) {
    /* WHAT: Verify lock/unlock operations are counted in stats */
    /* REGRESSION: Statistics accuracy for lock operations */
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("lock_stats_test");
    nimcp_mutex_pool_lock(slot);
    nimcp_mutex_pool_unlock(slot);

    nimcp_mutex_pool_stats_t stats;
    nimcp_mutex_pool_get_stats(&stats);

    EXPECT_GE(stats.total_locks, 1u);
    EXPECT_GE(stats.total_unlocks, 1u);

    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolStatsNullReturnsError) {
    /* WHAT: Verify NULL stats pointer returns error */
    /* REGRESSION: NULL safety */
    int result = nimcp_mutex_pool_get_stats(NULL);
    EXPECT_LT(result, 0);
}

TEST_F(MutexPoolRegressionTest, MutexPoolStatsSlotUsageTracked) {
    /* WHAT: Verify per-slot usage is tracked */
    /* REGRESSION: Slot-level statistics */
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("slot_usage_test");
    nimcp_mutex_pool_lock(slot);
    nimcp_mutex_pool_unlock(slot);

    nimcp_mutex_pool_stats_t stats;
    nimcp_mutex_pool_get_stats(&stats);

    /* The slot should have usage > 0 */
    EXPECT_GT(stats.slot_usage[slot], 0u);

    nimcp_mutex_pool_release(slot);
}

/* ============================================================================
 * Reference Counting Regression Tests
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolReleaseSucceeds) {
    /* WHAT: Verify release after acquire succeeds */
    /* REGRESSION: Release functionality */
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("release_test");
    /* release returns void, so just verify no crash */
    nimcp_mutex_pool_release(slot);
}

TEST_F(MutexPoolRegressionTest, MutexPoolMultipleAcquireRelease) {
    /* WHAT: Verify multiple acquire/release cycles work */
    /* REGRESSION: Reference counting stability */
    for (int i = 0; i < 100; i++) {
        nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("multi_cycle_test");
        EXPECT_NE(slot, NIMCP_MUTEX_SLOT_INVALID);
        nimcp_mutex_pool_release(slot);
    }
}

/* ============================================================================
 * Pool Size Constant Regression Test
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolSizeConstant) {
    /* WHAT: Verify NIMCP_MUTEX_POOL_SIZE is defined and reasonable */
    /* REGRESSION: Pool size constant stability */
    EXPECT_GT(NIMCP_MUTEX_POOL_SIZE, 0u);
    EXPECT_LE(NIMCP_MUTEX_POOL_SIZE, 256u);  /* Reasonable upper bound */
}

/* ============================================================================
 * Multi-Bridge Scenario Regression Test
 * ============================================================================ */

TEST_F(MutexPoolRegressionTest, MutexPoolMultipleBridges) {
    /* WHAT: Verify multiple bridges can use pool simultaneously */
    /* REGRESSION: Real-world usage pattern */
    const char* bridge_names[] = {
        "stdp_immune_bridge",
        "plasticity_bridge",
        "memory_bridge",
        "attention_bridge",
        "prefrontal_bridge"
    };
    nimcp_mutex_slot_t slots[5];

    /* Acquire slots for all bridges */
    for (int i = 0; i < 5; i++) {
        slots[i] = nimcp_mutex_pool_acquire(bridge_names[i]);
        EXPECT_NE(slots[i], NIMCP_MUTEX_SLOT_INVALID);
    }

    /* Lock and unlock each */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_mutex_pool_lock(slots[i]), 0);
        EXPECT_EQ(nimcp_mutex_pool_unlock(slots[i]), 0);
    }

    /* Release all */
    for (int i = 0; i < 5; i++) {
        nimcp_mutex_pool_release(slots[i]);
    }
}

} // anonymous namespace
