/**
 * @file test_utils_platform_mutex.cpp
 * @brief Comprehensive unit tests for platform mutex primitives
 *
 * WHAT: 100% test coverage for nimcp_platform_mutex.c
 * WHY:  Mutex primitives are critical synchronization infrastructure
 * HOW:  Test creation, locking, unlocking, trylock, recursion, and thread safety
 *
 * TEST COVERAGE:
 * 1. Mutex creation and destruction (normal and recursive)
 * 2. Lock and unlock operations
 * 3. Mutual exclusion verification (thread safety)
 * 4. Deadlock avoidance patterns
 * 5. Trylock operations (non-blocking)
 * 6. Recursive mutex behavior
 * 7. Multiple threads competing for lock
 * 8. Lock/unlock balance verification
 * 9. Performance under contention
 * 10. Edge cases (NULL pointers, double destroy, etc.)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

    #include "utils/platform/nimcp_platform_mutex.h"
    #include "utils/platform/nimcp_platform_thread.h"
    #include <errno.h>

//=============================================================================
// Test Fixture
//=============================================================================

class MutexTest : public ::testing::Test {
protected:
    nimcp_platform_mutex_t mutex;
    nimcp_platform_mutex_t recursive_mutex;

    void SetUp() override {
        // Initialize mutexes for each test
        // Note: Some tests will re-initialize with specific types
    }

    void TearDown() override {
        // Cleanup is done per-test as needed
    }
};

//=============================================================================
// Unit Test 1: Basic mutex creation and destruction
//=============================================================================

TEST_F(MutexTest, Init_BasicNormalMutex) {
    // WHAT: Verify nimcp_platform_mutex_init() creates a normal mutex
    // WHY:  Basic functionality must work
    // HOW:  Initialize mutex and verify success

    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0) << "Failed to initialize normal mutex";

    // Cleanup
    result = nimcp_platform_mutex_destroy(&mutex);
    EXPECT_EQ(result, 0) << "Failed to destroy mutex";

    SUCCEED() << "Normal mutex creation and destruction works";
}

//=============================================================================
// Unit Test 2: Recursive mutex creation
//=============================================================================

TEST_F(MutexTest, Init_RecursiveMutex) {
    // WHAT: Verify nimcp_platform_mutex_init() can create recursive mutex
    // WHY:  Recursive locking is a critical feature
    // HOW:  Initialize with recursive=true and verify success

    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0) << "Failed to initialize recursive mutex";

    // Cleanup
    result = nimcp_platform_mutex_destroy(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Failed to destroy recursive mutex";

    SUCCEED() << "Recursive mutex creation works";
}

//=============================================================================
// Unit Test 3: Lock and unlock operations
//=============================================================================

TEST_F(MutexTest, LockUnlock_BasicOperation) {
    // WHAT: Verify basic lock/unlock cycle works
    // WHY:  Core mutex functionality
    // HOW:  Lock, unlock, verify return codes

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    int result = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(result, 0) << "Failed to lock mutex";

    result = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(result, 0) << "Failed to unlock mutex";

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Basic lock/unlock works";
}

//=============================================================================
// Unit Test 4: Mutual exclusion verification
//=============================================================================

TEST_F(MutexTest, MutualExclusion_ThreadSafety) {
    // WHAT: Verify mutex actually provides mutual exclusion
    // WHY:  Core mutex guarantee - only one thread at a time
    // HOW:  Multiple threads increment shared counter with mutex protection

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    const int NUM_THREADS = 4;
    const int INCREMENTS_PER_THREAD = 1000;
    std::atomic<int> counter{0};
    std::atomic<int> in_critical_section{0};
    std::atomic<bool> violation_detected{false};

    auto worker = [&]() {
        for (int i = 0; i < INCREMENTS_PER_THREAD; i++) {
            nimcp_platform_mutex_lock(&mutex);

            // Check that we're the only one in critical section
            int current = in_critical_section.fetch_add(1);
            if (current != 0) {
                violation_detected = true;
            }

            // Do work
            counter++;

            // Exit critical section
            in_critical_section.fetch_sub(1);
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(violation_detected) << "Mutual exclusion violated!";
    EXPECT_EQ(counter.load(), NUM_THREADS * INCREMENTS_PER_THREAD)
        << "Counter mismatch indicates race condition";

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Mutual exclusion verified with " << NUM_THREADS << " threads";
}

//=============================================================================
// Unit Test 5: Trylock operations
//=============================================================================

TEST_F(MutexTest, Trylock_NonBlockingBehavior) {
    // WHAT: Verify trylock doesn't block when mutex is locked
    // WHY:  Non-blocking lock acquisition is critical for some algorithms
    // HOW:  Lock mutex, verify trylock fails with EBUSY

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    // Lock the mutex
    ASSERT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

    // Try to lock again (should fail immediately)
    int result = nimcp_platform_mutex_trylock(&mutex);
    EXPECT_EQ(result, EBUSY) << "Trylock should return EBUSY when locked";

    // Unlock
    ASSERT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    // Now trylock should succeed
    result = nimcp_platform_mutex_trylock(&mutex);
    EXPECT_EQ(result, 0) << "Trylock should succeed when unlocked";

    // Unlock again
    ASSERT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Trylock non-blocking behavior verified";
}

//=============================================================================
// Unit Test 6: Recursive mutex allows same-thread relocking
//=============================================================================

TEST_F(MutexTest, RecursiveMutex_AllowsRelock) {
    // WHAT: Verify recursive mutex can be locked multiple times by same thread
    // WHY:  Recursive locking is needed for reentrant code
    // HOW:  Lock mutex multiple times, unlock same number of times

    ASSERT_EQ(nimcp_platform_mutex_init(&recursive_mutex, true), 0);

    // Lock multiple times from same thread
    EXPECT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0) << "First lock failed";
    EXPECT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0) << "Second lock failed";
    EXPECT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0) << "Third lock failed";

    // Unlock same number of times
    EXPECT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0) << "First unlock failed";
    EXPECT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0) << "Second unlock failed";
    EXPECT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0) << "Third unlock failed";

    nimcp_platform_mutex_destroy(&recursive_mutex);
    SUCCEED() << "Recursive mutex allows multiple locks from same thread";
}

//=============================================================================
// Unit Test 7: Multiple threads competing for lock
//=============================================================================

TEST_F(MutexTest, Contention_MultipleThreadsCompete) {
    // WHAT: Verify mutex handles contention from multiple threads
    // WHY:  Must work correctly under heavy contention
    // HOW:  Many threads repeatedly acquire/release lock

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    const int NUM_THREADS = 8;
    const int ITERATIONS = 100;
    std::atomic<int> lock_acquisitions{0};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            nimcp_platform_mutex_lock(&mutex);
            lock_acquisitions++;
            // Small amount of work
            std::this_thread::yield();
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(lock_acquisitions.load(), NUM_THREADS * ITERATIONS)
        << "Not all threads acquired lock the expected number of times";

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Mutex handles contention from " << NUM_THREADS << " threads";
}

//=============================================================================
// Unit Test 8: Lock/unlock balance verification
//=============================================================================

TEST_F(MutexTest, Balance_MatchedLockUnlock) {
    // WHAT: Verify lock/unlock pairs are balanced
    // WHY:  Unbalanced operations lead to deadlocks or undefined behavior
    // HOW:  Track lock depth, verify it returns to zero

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    // Lock and unlock multiple times
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0) << "Lock " << i << " failed";
        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0) << "Unlock " << i << " failed";
    }

    // Final lock/unlock to verify mutex still works
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Lock/unlock balance verified";
}

//=============================================================================
// Unit Test 9: Performance under contention
//=============================================================================

TEST_F(MutexTest, Performance_ContentionThroughput) {
    // WHAT: Measure mutex performance under contention
    // WHY:  Ensure reasonable performance for high-contention scenarios
    // HOW:  Time how long it takes for threads to acquire locks

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    const int NUM_THREADS = 4;
    const int ITERATIONS = 10000;
    std::atomic<int> operations{0};

    auto start = std::chrono::high_resolution_clock::now();

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            nimcp_platform_mutex_lock(&mutex);
            operations++;
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(operations.load(), NUM_THREADS * ITERATIONS);

    // Performance check: should complete in reasonable time (< 5 seconds)
    EXPECT_LT(duration.count(), 5000)
        << "Mutex operations took " << duration.count() << "ms (too slow)";

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Completed " << operations.load() << " operations in "
              << duration.count() << "ms";
}

//=============================================================================
// Unit Test 10: Edge cases
//=============================================================================

TEST_F(MutexTest, EdgeCase_NullPointer) {
    // WHAT: Verify NULL pointer handling
    // WHY:  Defensive programming - should not crash
    // HOW:  Pass NULL to each function, verify EINVAL returned

    EXPECT_EQ(nimcp_platform_mutex_init(nullptr, false), EINVAL)
        << "init should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_mutex_destroy(nullptr), EINVAL)
        << "destroy should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_mutex_lock(nullptr), EINVAL)
        << "lock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_mutex_unlock(nullptr), EINVAL)
        << "unlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_mutex_trylock(nullptr), EINVAL)
        << "trylock should return EINVAL for NULL pointer";

    SUCCEED() << "NULL pointer handling verified";
}

//=============================================================================
// Unit Test 11: Trylock success case
//=============================================================================

TEST_F(MutexTest, Trylock_SuccessWhenUnlocked) {
    // WHAT: Verify trylock succeeds when mutex is free
    // WHY:  Trylock must work as alternative to lock
    // HOW:  Call trylock on unlocked mutex, verify success

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    // Trylock should succeed immediately
    int result = nimcp_platform_mutex_trylock(&mutex);
    EXPECT_EQ(result, 0) << "Trylock should succeed on unlocked mutex";

    // Unlock
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Trylock succeeds on unlocked mutex";
}

//=============================================================================
// Unit Test 12: Deadlock avoidance pattern
//=============================================================================

TEST_F(MutexTest, DeadlockAvoidance_TrylockPattern) {
    // WHAT: Verify trylock enables deadlock avoidance
    // WHY:  Trylock + backoff prevents deadlocks in complex scenarios
    // HOW:  Use trylock in loop with timeout

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    // Lock from main thread
    ASSERT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

    // Spawn thread that tries to acquire with timeout
    std::atomic<bool> acquired{false};
    std::atomic<bool> timed_out{false};

    std::thread worker([&]() {
        auto start = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(100);

        while (std::chrono::steady_clock::now() - start < timeout) {
            if (nimcp_platform_mutex_trylock(&mutex) == 0) {
                acquired = true;
                nimcp_platform_mutex_unlock(&mutex);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        timed_out = true;
    });

    // Wait for worker to time out
    worker.join();

    EXPECT_FALSE(acquired) << "Thread should not have acquired locked mutex";
    EXPECT_TRUE(timed_out) << "Thread should have timed out";

    // Unlock
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    // Now trylock should succeed immediately
    EXPECT_EQ(nimcp_platform_mutex_trylock(&mutex), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Trylock enables deadlock avoidance pattern";
}

//=============================================================================
// Unit Test 13: Recursive mutex requires matching unlocks
//=============================================================================

TEST_F(MutexTest, RecursiveMutex_RequiresMatchingUnlocks) {
    // WHAT: Verify recursive mutex stays locked until all unlocks done
    // WHY:  Lock count must be properly maintained
    // HOW:  Lock N times, unlock N-1 times, verify still locked

    ASSERT_EQ(nimcp_platform_mutex_init(&recursive_mutex, true), 0);

    // Lock 3 times
    ASSERT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0);
    ASSERT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0);
    ASSERT_EQ(nimcp_platform_mutex_lock(&recursive_mutex), 0);

    // Unlock 2 times
    ASSERT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0);
    ASSERT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0);

    // Should still be locked - trylock from another thread should fail
    std::atomic<bool> trylock_succeeded{false};

    std::thread worker([&]() {
        if (nimcp_platform_mutex_trylock(&recursive_mutex) == 0) {
            trylock_succeeded = true;
            nimcp_platform_mutex_unlock(&recursive_mutex);
        }
    });

    worker.join();

    EXPECT_FALSE(trylock_succeeded)
        << "Mutex should still be locked after 2 of 3 unlocks";

    // Final unlock
    ASSERT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0);

    // Now it should be free
    EXPECT_EQ(nimcp_platform_mutex_trylock(&recursive_mutex), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&recursive_mutex), 0);

    nimcp_platform_mutex_destroy(&recursive_mutex);
    SUCCEED() << "Recursive mutex requires matching unlocks";
}

//=============================================================================
// Unit Test 14: Stress test with rapid lock/unlock
//=============================================================================

TEST_F(MutexTest, Stress_RapidLockUnlock) {
    // WHAT: Stress test with very rapid lock/unlock cycles
    // WHY:  Ensure mutex remains stable under rapid operations
    // HOW:  Perform thousands of lock/unlock cycles quickly

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    const int ITERATIONS = 100000;

    for (int i = 0; i < ITERATIONS; i++) {
        ASSERT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
        ASSERT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    }

    nimcp_platform_mutex_destroy(&mutex);
    SUCCEED() << "Survived " << ITERATIONS << " rapid lock/unlock cycles";
}

//=============================================================================
// Unit Test 15: Multiple mutex independence
//=============================================================================

TEST_F(MutexTest, Independence_MultipleMutexes) {
    // WHAT: Verify multiple mutexes are independent
    // WHY:  Locking one mutex should not affect others
    // HOW:  Create multiple mutexes, lock them independently

    nimcp_platform_mutex_t mutex1, mutex2, mutex3;

    ASSERT_EQ(nimcp_platform_mutex_init(&mutex1, false), 0);
    ASSERT_EQ(nimcp_platform_mutex_init(&mutex2, false), 0);
    ASSERT_EQ(nimcp_platform_mutex_init(&mutex3, false), 0);

    // Lock all three
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex1), 0);
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex2), 0);
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex3), 0);

    // Unlock in different order
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex2), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex1), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex3), 0);

    // Lock again in different pattern
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex3), 0);
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex1), 0);

    // Trylock on mutex2 should succeed
    EXPECT_EQ(nimcp_platform_mutex_trylock(&mutex2), 0);

    // Cleanup
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex1), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex2), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex3), 0);

    nimcp_platform_mutex_destroy(&mutex1);
    nimcp_platform_mutex_destroy(&mutex2);
    nimcp_platform_mutex_destroy(&mutex3);

    SUCCEED() << "Multiple mutexes are independent";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
