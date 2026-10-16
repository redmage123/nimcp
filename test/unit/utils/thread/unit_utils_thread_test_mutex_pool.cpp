//=============================================================================
// unit_utils_thread_test_mutex_pool.cpp - Mutex Pool Unit Tests
//=============================================================================
/**
 * @file unit_utils_thread_test_mutex_pool.cpp
 * @brief TDD test suite for shared mutex pool implementation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation quality
 * - Guard clause verification: Test all error conditions and NULL safety
 * - Lifecycle testing: Init, destroy, and proper cleanup
 * - Concurrency testing: Multi-threaded scenarios with verification
 * - Edge case coverage: Pool exhaustion, hash collisions, statistics
 *
 * COVERAGE:
 * 1. Pool initialization and destruction
 * 2. Slot acquire by name and by ID
 * 3. Slot release and reference counting
 * 4. Lock/unlock operations
 * 5. Trylock behavior
 * 6. Concurrent acquire/release
 * 7. Hash distribution (slot assignment)
 * 8. Statistics tracking
 * 9. NULL parameter handling
 * 10. Pool auto-initialization
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <set>

// Headers have their own extern "C" guards
#include "utils/thread/nimcp_mutex_pool.h"

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for mutex pool tests
 * WHY: Set up/tear down resources for each test
 */
class MutexPoolTest : public ::testing::Test {
public:
    // Test parameters
    static const int THREAD_COUNT = 8;
    static const int ITERATIONS = 100;

protected:
    void SetUp() override {
        // Ensure pool is destroyed before each test for clean state
        nimcp_mutex_pool_destroy();
        // Initialize pool
        ASSERT_EQ(0, nimcp_mutex_pool_init());
    }

    void TearDown() override {
        // Clean up
        nimcp_mutex_pool_destroy();
    }
};

/**
 * WHAT: Fixture for lifecycle tests (no auto-init)
 * WHY: Test init/destroy behavior explicitly
 */
class MutexPoolLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure pool is destroyed for clean slate
        nimcp_mutex_pool_destroy();
    }

    void TearDown() override {
        nimcp_mutex_pool_destroy();
    }
};

/**
 * WHAT: Helper to sleep for a short duration
 * WHY: Ensure threads have time to interact
 */
static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * TEST: Pool initialization
 * WHY: Verify pool can be created
 */
TEST_F(MutexPoolLifecycleTest, InitSuccess) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Double initialization is safe
 * WHY: Verify idempotent initialization
 */
TEST_F(MutexPoolLifecycleTest, DoubleInitSafe) {
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_EQ(0, nimcp_mutex_pool_init());  // Should succeed (already init)
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Pool destruction
 * WHY: Verify pool can be destroyed
 */
TEST_F(MutexPoolLifecycleTest, DestroySuccess) {
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
    EXPECT_EQ(0, nimcp_mutex_pool_destroy());
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Destroy without init is safe
 * WHY: Verify safe destroy on uninitialized pool
 */
TEST_F(MutexPoolLifecycleTest, DestroyWithoutInitSafe) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
    EXPECT_EQ(0, nimcp_mutex_pool_destroy());
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Double destruction is safe
 * WHY: Verify idempotent destruction
 */
TEST_F(MutexPoolLifecycleTest, DoubleDestroySafe) {
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_EQ(0, nimcp_mutex_pool_destroy());
    EXPECT_EQ(0, nimcp_mutex_pool_destroy());  // Should succeed
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Reinitialize after destroy
 * WHY: Verify pool can be reused
 */
TEST_F(MutexPoolLifecycleTest, ReinitAfterDestroy) {
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_EQ(0, nimcp_mutex_pool_destroy());
    EXPECT_EQ(0, nimcp_mutex_pool_init());
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

//=============================================================================
// Slot Acquisition Tests
//=============================================================================

/**
 * TEST: Acquire slot by name
 * WHY: Verify acquire returns valid slot
 */
TEST_F(MutexPoolTest, AcquireByName) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("test_bridge");
    EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);
    EXPECT_LT(slot, NIMCP_MUTEX_POOL_SIZE);
}

/**
 * TEST: Acquire slot with NULL name
 * WHY: Verify error handling for NULL
 */
TEST_F(MutexPoolTest, AcquireNullName) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(NULL);
    EXPECT_EQ(NIMCP_MUTEX_SLOT_INVALID, slot);
}

/**
 * TEST: Same name returns same slot (deterministic hashing)
 * WHY: Verify consistent slot assignment
 */
TEST_F(MutexPoolTest, SameNameSameSlot) {
    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire("my_bridge");
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire("my_bridge");
    EXPECT_EQ(slot1, slot2);
}

/**
 * TEST: Different names may get different slots
 * WHY: Verify hash distribution
 */
TEST_F(MutexPoolTest, DifferentNamesDifferentSlots) {
    std::set<nimcp_mutex_slot_t> slots;
    const char* names[] = {
        "bridge_a", "bridge_b", "bridge_c", "bridge_d",
        "bridge_e", "bridge_f", "bridge_g", "bridge_h"
    };

    for (const char* name : names) {
        nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(name);
        EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);
        slots.insert(slot);
    }

    // Should have some distribution (not all same slot)
    // With 8 names and 8 slots, expect some diversity
    EXPECT_GT(slots.size(), 1u);
}

/**
 * TEST: Acquire slot by ID
 * WHY: Verify acquire by numeric ID
 */
TEST_F(MutexPoolTest, AcquireById) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire_by_id(42);
    EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);
    EXPECT_EQ(42 % NIMCP_MUTEX_POOL_SIZE, slot);
}

/**
 * TEST: Same ID returns same slot
 * WHY: Verify deterministic ID-based assignment
 */
TEST_F(MutexPoolTest, SameIdSameSlot) {
    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire_by_id(100);
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire_by_id(100);
    EXPECT_EQ(slot1, slot2);
}

/**
 * TEST: IDs modulo pool size give same slot
 * WHY: Verify modulo behavior
 */
TEST_F(MutexPoolTest, ModuloIdsSameSlot) {
    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire_by_id(5);
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire_by_id(5 + NIMCP_MUTEX_POOL_SIZE);
    nimcp_mutex_slot_t slot3 = nimcp_mutex_pool_acquire_by_id(5 + 2 * NIMCP_MUTEX_POOL_SIZE);
    EXPECT_EQ(slot1, slot2);
    EXPECT_EQ(slot2, slot3);
}

/**
 * TEST: Auto-initialize on acquire
 * WHY: Verify pool auto-initializes when needed
 */
TEST_F(MutexPoolLifecycleTest, AutoInitOnAcquire) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("auto_init_test");
    EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

/**
 * TEST: Auto-initialize on acquire_by_id
 * WHY: Verify pool auto-initializes for ID-based acquire
 */
TEST_F(MutexPoolLifecycleTest, AutoInitOnAcquireById) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire_by_id(99);
    EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);
    EXPECT_TRUE(nimcp_mutex_pool_is_initialized());
}

//=============================================================================
// Slot Release Tests
//=============================================================================

/**
 * TEST: Release slot (basic)
 * WHY: Verify release doesn't crash
 */
TEST_F(MutexPoolTest, ReleaseSlot) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("release_test");
    EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);

    // Release should not crash or error
    nimcp_mutex_pool_release(slot);
}

/**
 * TEST: Release invalid slot is safe
 * WHY: Verify safe handling of invalid slot
 */
TEST_F(MutexPoolTest, ReleaseInvalidSlot) {
    nimcp_mutex_pool_release(NIMCP_MUTEX_SLOT_INVALID);  // Should not crash
    nimcp_mutex_pool_release(NIMCP_MUTEX_POOL_SIZE);     // Out of range
    nimcp_mutex_pool_release(NIMCP_MUTEX_POOL_SIZE + 100);  // Way out of range
}

/**
 * TEST: Multiple acquires increment ref count
 * WHY: Verify reference counting behavior
 */
TEST_F(MutexPoolTest, ReferenceCountingMultipleAcquires) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("refcount_test");

    // Acquire same slot multiple times (same name = same slot)
    nimcp_mutex_pool_acquire("refcount_test");
    nimcp_mutex_pool_acquire("refcount_test");

    // Slot should still be usable after releasing once
    nimcp_mutex_pool_release(slot);
    EXPECT_EQ(0, nimcp_mutex_pool_lock(slot));
    EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));

    // Release remaining refs
    nimcp_mutex_pool_release(slot);
    nimcp_mutex_pool_release(slot);
}

//=============================================================================
// Lock/Unlock Tests
//=============================================================================

/**
 * TEST: Lock and unlock slot
 * WHY: Verify basic lock/unlock works
 */
TEST_F(MutexPoolTest, LockUnlockBasic) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("lock_test");
    ASSERT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);

    EXPECT_EQ(0, nimcp_mutex_pool_lock(slot));
    EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));
}

/**
 * TEST: Lock invalid slot
 * WHY: Verify error handling for invalid slot
 */
TEST_F(MutexPoolTest, LockInvalidSlot) {
    EXPECT_NE(0, nimcp_mutex_pool_lock(NIMCP_MUTEX_SLOT_INVALID));
    EXPECT_NE(0, nimcp_mutex_pool_lock(NIMCP_MUTEX_POOL_SIZE));
}

/**
 * TEST: Unlock invalid slot
 * WHY: Verify error handling for invalid slot
 */
TEST_F(MutexPoolTest, UnlockInvalidSlot) {
    EXPECT_NE(0, nimcp_mutex_pool_unlock(NIMCP_MUTEX_SLOT_INVALID));
    EXPECT_NE(0, nimcp_mutex_pool_unlock(NIMCP_MUTEX_POOL_SIZE));
}

/**
 * TEST: Lock on uninitialized pool fails
 * WHY: Verify proper error on uninitialized pool
 */
TEST_F(MutexPoolLifecycleTest, LockUninitialized) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());
    EXPECT_NE(0, nimcp_mutex_pool_lock(0));
}

/**
 * TEST: Multiple lock/unlock cycles
 * WHY: Verify reusability
 */
TEST_F(MutexPoolTest, MultipleLockUnlockCycles) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("cycle_test");

    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(0, nimcp_mutex_pool_lock(slot));
        EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));
    }
}

//=============================================================================
// Trylock Tests
//=============================================================================

/**
 * TEST: Trylock succeeds when not locked
 * WHY: Verify trylock acquires unlocked mutex
 */
TEST_F(MutexPoolTest, TrylockSuccess) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("trylock_test");

    EXPECT_EQ(0, nimcp_mutex_pool_trylock(slot));
    EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));
}

/**
 * TEST: Trylock returns BUSY when locked
 * WHY: Verify trylock doesn't block
 */
TEST_F(MutexPoolTest, TrylockBusyWhenLocked) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("trylock_busy_test");

    EXPECT_EQ(0, nimcp_mutex_pool_lock(slot));

    // Trylock should fail (slot already locked by us, but mutex is non-recursive)
    // Actually, platform mutex may or may not be recursive
    // Let's test from another thread
    std::atomic<int> trylock_result{-100};

    std::thread other([&]() {
        trylock_result = nimcp_mutex_pool_trylock(slot);
    });

    other.join();

    // Should be NIMCP_BUSY (or positive error code)
    EXPECT_NE(0, trylock_result.load());

    EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));
}

/**
 * TEST: Trylock invalid slot
 * WHY: Verify error handling for invalid slot
 */
TEST_F(MutexPoolTest, TrylockInvalidSlot) {
    EXPECT_NE(0, nimcp_mutex_pool_trylock(NIMCP_MUTEX_SLOT_INVALID));
}

//=============================================================================
// Concurrency Tests
//=============================================================================

/**
 * TEST: Concurrent acquire/release
 * WHY: Verify thread-safe slot management
 */
TEST_F(MutexPoolTest, ConcurrentAcquireRelease) {
    const int NUM_THREADS = 8;
    const int ITERATIONS = 100;

    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            std::string name = "bridge_" + std::to_string(t);

            for (int i = 0; i < ITERATIONS; ++i) {
                nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(name.c_str());
                if (slot != NIMCP_MUTEX_SLOT_INVALID) {
                    success_count++;
                    nimcp_mutex_pool_release(slot);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(NUM_THREADS * ITERATIONS, success_count.load());
}

/**
 * TEST: Concurrent lock/unlock with shared slot
 * WHY: Verify mutual exclusion works
 */
TEST_F(MutexPoolTest, ConcurrentLockUnlockSharedSlot) {
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("shared_lock_test");

    std::atomic<int> counter{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current_in_cs{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERATIONS; ++i) {
                nimcp_mutex_pool_lock(slot);

                // Critical section
                int in_cs = ++current_in_cs;
                if (in_cs > max_concurrent) {
                    max_concurrent = in_cs;
                }

                counter++;
                sleep_ms(1);

                current_in_cs--;
                nimcp_mutex_pool_unlock(slot);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify mutual exclusion (max 1 thread in critical section)
    EXPECT_EQ(1, max_concurrent.load());
    EXPECT_EQ(THREAD_COUNT * ITERATIONS, counter.load());
}

/**
 * TEST: Concurrent lock/unlock with different slots
 * WHY: Verify different slots don't interfere
 */
TEST_F(MutexPoolTest, ConcurrentLockUnlockDifferentSlots) {
    std::atomic<int> total_locks{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&, t]() {
            std::string name = "independent_bridge_" + std::to_string(t);
            nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(name.c_str());

            for (int i = 0; i < ITERATIONS; ++i) {
                nimcp_mutex_pool_lock(slot);
                total_locks++;
                nimcp_mutex_pool_unlock(slot);
            }

            nimcp_mutex_pool_release(slot);
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(THREAD_COUNT * ITERATIONS, total_locks.load());
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * TEST: Get stats with NULL parameter
 * WHY: Verify error handling for NULL
 */
TEST_F(MutexPoolTest, GetStatsNullParameter) {
    EXPECT_NE(0, nimcp_mutex_pool_get_stats(NULL));
}

/**
 * TEST: Initial stats are zero
 * WHY: Verify clean statistics on fresh pool
 */
TEST_F(MutexPoolLifecycleTest, InitialStatsZero) {
    EXPECT_EQ(0, nimcp_mutex_pool_init());

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    EXPECT_EQ(0u, stats.total_acquires);
    EXPECT_EQ(0u, stats.total_releases);
    EXPECT_EQ(0u, stats.total_locks);
    EXPECT_EQ(0u, stats.total_unlocks);
}

/**
 * TEST: Stats track acquires
 * WHY: Verify acquire operations are counted
 */
TEST_F(MutexPoolTest, StatsTrackAcquires) {
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_pool_acquire("stats_test_1");
    nimcp_mutex_pool_acquire("stats_test_2");
    nimcp_mutex_pool_acquire("stats_test_3");

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    EXPECT_EQ(3u, stats.total_acquires);
}

/**
 * TEST: Stats track releases
 * WHY: Verify release operations are counted
 */
TEST_F(MutexPoolTest, StatsTrackReleases) {
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_slot_t slot1 = nimcp_mutex_pool_acquire("release_stats_1");
    nimcp_mutex_slot_t slot2 = nimcp_mutex_pool_acquire("release_stats_2");

    nimcp_mutex_pool_release(slot1);
    nimcp_mutex_pool_release(slot2);

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    EXPECT_EQ(2u, stats.total_releases);
}

/**
 * TEST: Stats track locks
 * WHY: Verify lock operations are counted
 */
TEST_F(MutexPoolTest, StatsTrackLocks) {
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("lock_stats_test");

    nimcp_mutex_pool_lock(slot);
    nimcp_mutex_pool_unlock(slot);
    nimcp_mutex_pool_lock(slot);
    nimcp_mutex_pool_unlock(slot);

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    EXPECT_EQ(2u, stats.total_locks);
    EXPECT_EQ(2u, stats.total_unlocks);
}

/**
 * TEST: Stats track slot usage
 * WHY: Verify per-slot usage is tracked
 */
TEST_F(MutexPoolTest, StatsTrackSlotUsage) {
    nimcp_mutex_pool_reset_stats();

    // Acquire same slot multiple times
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("slot_usage_test");
    nimcp_mutex_pool_acquire("slot_usage_test");
    nimcp_mutex_pool_acquire("slot_usage_test");

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    // The slot should have usage count
    EXPECT_EQ(3u, stats.slot_usage[slot]);
}

/**
 * TEST: Reset stats clears all
 * WHY: Verify reset functionality
 */
TEST_F(MutexPoolTest, ResetStatsClearsAll) {
    // Generate some statistics
    nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("reset_test");
    nimcp_mutex_pool_lock(slot);
    nimcp_mutex_pool_unlock(slot);
    nimcp_mutex_pool_release(slot);

    nimcp_mutex_pool_stats_t before;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&before));
    EXPECT_GT(before.total_acquires, 0u);

    // Reset
    nimcp_mutex_pool_reset_stats();

    nimcp_mutex_pool_stats_t after;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&after));

    EXPECT_EQ(0u, after.total_acquires);
    EXPECT_EQ(0u, after.total_releases);
    EXPECT_EQ(0u, after.total_locks);
    EXPECT_EQ(0u, after.total_unlocks);
    EXPECT_EQ(0u, after.contention_events);

    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; ++i) {
        EXPECT_EQ(0u, after.slot_usage[i]);
    }
}

/**
 * TEST: Stats on uninitialized pool
 * WHY: Verify safe behavior when pool not initialized
 */
TEST_F(MutexPoolLifecycleTest, StatsUninitializedPool) {
    EXPECT_FALSE(nimcp_mutex_pool_is_initialized());

    nimcp_mutex_pool_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  // Fill with garbage

    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    // Should be zeroed
    EXPECT_EQ(0u, stats.total_acquires);
    EXPECT_EQ(0u, stats.total_locks);
}

//=============================================================================
// Hash Distribution Tests
//=============================================================================

/**
 * TEST: Hash distribution across pool
 * WHY: Verify names distribute across slots
 */
TEST_F(MutexPoolTest, HashDistribution) {
    nimcp_mutex_pool_reset_stats();

    // Acquire many different bridges
    const int NUM_BRIDGES = 100;
    for (int i = 0; i < NUM_BRIDGES; ++i) {
        std::string name = "bridge_" + std::to_string(i);
        nimcp_mutex_pool_acquire(name.c_str());
    }

    nimcp_mutex_pool_stats_t stats;
    EXPECT_EQ(0, nimcp_mutex_pool_get_stats(&stats));

    // Check that usage is distributed across slots
    int slots_used = 0;
    uint32_t max_usage = 0;
    uint32_t min_usage = UINT32_MAX;

    for (int i = 0; i < NIMCP_MUTEX_POOL_SIZE; ++i) {
        if (stats.slot_usage[i] > 0) {
            slots_used++;
            if (stats.slot_usage[i] > max_usage) max_usage = stats.slot_usage[i];
            if (stats.slot_usage[i] < min_usage) min_usage = stats.slot_usage[i];
        }
    }

    // Expect most slots to be used
    EXPECT_GE(slots_used, NIMCP_MUTEX_POOL_SIZE / 2);

    // Expect reasonable distribution (max not more than 5x min)
    if (min_usage > 0) {
        EXPECT_LE(max_usage, min_usage * 5);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * TEST: High contention stress test
 * WHY: Verify pool handles heavy concurrent load
 */
TEST_F(MutexPoolTest, HighContentionStress) {
    const int STRESS_THREADS = 16;
    const int STRESS_ITERATIONS = 200;

    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < STRESS_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < STRESS_ITERATIONS; ++i) {
                nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire("stress_test");
                if (slot != NIMCP_MUTEX_SLOT_INVALID) {
                    nimcp_mutex_pool_lock(slot);
                    success_count++;
                    nimcp_mutex_pool_unlock(slot);
                    nimcp_mutex_pool_release(slot);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(STRESS_THREADS * STRESS_ITERATIONS, success_count.load());
}

/**
 * TEST: Rapid acquire/lock/unlock/release cycles
 * WHY: Verify no leaks or corruption under rapid cycling
 */
TEST_F(MutexPoolTest, RapidCycles) {
    const int CYCLES = 1000;

    for (int i = 0; i < CYCLES; ++i) {
        std::string name = "rapid_" + std::to_string(i % 20);
        nimcp_mutex_slot_t slot = nimcp_mutex_pool_acquire(name.c_str());
        EXPECT_NE(NIMCP_MUTEX_SLOT_INVALID, slot);

        EXPECT_EQ(0, nimcp_mutex_pool_lock(slot));
        EXPECT_EQ(0, nimcp_mutex_pool_unlock(slot));

        nimcp_mutex_pool_release(slot);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
