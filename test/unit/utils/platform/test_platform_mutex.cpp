//=============================================================================
// test_platform_mutex.cpp - Comprehensive Unit Tests for Platform Mutex
//=============================================================================
/**
 * @file test_platform_mutex.cpp
 * @brief TDD test suite for cross-platform mutex abstraction
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation quality
 * - Guard clause verification: Test all error conditions and NULL safety
 * - Lifecycle testing: Initialization, usage, and cleanup
 * - Mutex semantics: Lock/unlock behavior, recursive locking
 * - Concurrency testing: Multi-threaded contention and race conditions
 * - Platform neutrality: Tests should pass on both POSIX and Windows
 *
 * COVERAGE:
 * 1. Lifecycle tests (init/destroy)
 * 2. Normal (non-recursive) mutex behavior
 * 3. Recursive mutex behavior
 * 4. Lock/unlock semantics
 * 5. Trylock behavior (blocking vs non-blocking)
 * 6. Multi-threaded contention scenarios
 * 7. NULL pointer safety
 * 8. Error handling and recovery
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>

#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlatformMutexTest : public NimcpTestBase {
protected:
    nimcp_platform_mutex_t mutex;
    nimcp_platform_mutex_t recursive_mutex;

    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        // Initialize mutexes to zero (safe state)
        memset(&mutex, 0, sizeof(mutex));
        memset(&recursive_mutex, 0, sizeof(recursive_mutex));
    }

    void TearDown() override {
        // Clean up any initialized mutexes
        // (test cleanup is handled per-test)

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * TEST: InitDestroy - Normal mutex initialization and destruction
 * WHY: Verify basic lifecycle - mutex must initialize successfully
 * WHAT: Create and destroy a normal mutex
 */
TEST_F(PlatformMutexTest, InitDestroy) {
    // Initialize normal mutex
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0) << "Mutex init should succeed";

    // Should be usable immediately
    result = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(result, 0) << "Mutex lock should succeed after init";

    result = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(result, 0) << "Mutex unlock should succeed";

    // Cleanup
    result = nimcp_platform_mutex_destroy(&mutex);
    EXPECT_EQ(result, 0) << "Mutex destroy should succeed";
}

/**
 * TEST: InitDestroyRecursive - Recursive mutex initialization
 * WHY: Verify recursive mutex creation works correctly
 * WHAT: Create and destroy a recursive mutex
 */
TEST_F(PlatformMutexTest, InitDestroyRecursive) {
    // Initialize recursive mutex
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0) << "Recursive mutex init should succeed";

    // Should be usable immediately
    result = nimcp_platform_mutex_lock(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Recursive mutex lock should succeed";

    result = nimcp_platform_mutex_unlock(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Recursive mutex unlock should succeed";

    // Cleanup
    result = nimcp_platform_mutex_destroy(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Recursive mutex destroy should succeed";
}

/**
 * TEST: MultipleInitDestroy - Multiple init/destroy cycles
 * WHY: Verify mutex can be reused after destruction
 * WHAT: Initialize, destroy, reinitialize same mutex
 */
TEST_F(PlatformMutexTest, MultipleInitDestroy) {
    for (int i = 0; i < 3; i++) {
        int result = nimcp_platform_mutex_init(&mutex, false);
        EXPECT_EQ(result, 0) << "Init cycle " << i << " should succeed";

        result = nimcp_platform_mutex_destroy(&mutex);
        EXPECT_EQ(result, 0) << "Destroy cycle " << i << " should succeed";

        memset(&mutex, 0, sizeof(mutex));
    }
}

//=============================================================================
// Basic Lock/Unlock Tests (Non-Recursive)
//=============================================================================

/**
 * TEST: LockUnlock - Basic lock/unlock sequence
 * WHY: Verify fundamental mutex semantics
 * WHAT: Lock, then unlock a normal mutex
 */
TEST_F(PlatformMutexTest, LockUnlock) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Lock should succeed
    result = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(result, 0) << "First lock should succeed";

    // Unlock should succeed
    result = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(result, 0) << "Unlock should succeed";

    // Should be able to lock again
    result = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(result, 0) << "Relock should succeed";

    result = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(result, 0);

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: MultipleLockUnlockCycles - Multiple lock/unlock sequences
 * WHY: Verify mutex state consistency over time
 * WHAT: Perform multiple lock/unlock cycles
 */
TEST_F(PlatformMutexTest, MultipleLockUnlockCycles) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    for (int i = 0; i < 10; i++) {
        result = nimcp_platform_mutex_lock(&mutex);
        EXPECT_EQ(result, 0) << "Lock cycle " << i << " should succeed";

        // Do some work
        volatile int x = i * 2;
        (void)x;

        result = nimcp_platform_mutex_unlock(&mutex);
        EXPECT_EQ(result, 0) << "Unlock cycle " << i << " should succeed";
    }

    nimcp_platform_mutex_destroy(&mutex);
}

//=============================================================================
// Trylock Tests (Non-Blocking)
//=============================================================================

/**
 * TEST: TryLockSuccess - Trylock succeeds when mutex is free
 * WHY: Non-blocking lock should succeed if no contention
 * WHAT: Trylock on unlocked mutex
 */
TEST_F(PlatformMutexTest, TryLockSuccess) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Trylock on free mutex should succeed
    result = nimcp_platform_mutex_trylock(&mutex);
    EXPECT_EQ(result, 0) << "Trylock on free mutex should succeed";

    // Unlock for cleanup
    result = nimcp_platform_mutex_unlock(&mutex);
    EXPECT_EQ(result, 0);

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: TryLockFails - Trylock fails when mutex is locked
 * WHY: Non-blocking lock should fail if already locked
 * WHAT: Trylock on locked mutex returns EBUSY
 */
TEST_F(PlatformMutexTest, TryLockFails) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Lock the mutex
    result = nimcp_platform_mutex_lock(&mutex);
    EXPECT_EQ(result, 0);

    // Trylock on locked mutex should fail with EBUSY
    result = nimcp_platform_mutex_trylock(&mutex);
    EXPECT_EQ(result, EBUSY) << "Trylock on locked mutex should return EBUSY";

    // Cleanup
    nimcp_platform_mutex_unlock(&mutex);
    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: TryLockMultiple - Multiple trylock attempts
 * WHY: Verify trylock behavior consistency
 * WHAT: Repeatedly trylock while locked/unlocked
 */
TEST_F(PlatformMutexTest, TryLockMultiple) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Multiple trylock attempts on free mutex
    for (int i = 0; i < 5; i++) {
        result = nimcp_platform_mutex_trylock(&mutex);
        EXPECT_EQ(result, 0) << "Trylock " << i << " should succeed";

        result = nimcp_platform_mutex_unlock(&mutex);
        EXPECT_EQ(result, 0);
    }

    nimcp_platform_mutex_destroy(&mutex);
}

//=============================================================================
// Recursive Mutex Tests
//=============================================================================

/**
 * TEST: RecursiveLocking - Same thread locks recursively
 * WHY: Recursive mutex must support multiple locks from same thread
 * WHAT: Lock same recursive mutex multiple times
 */
TEST_F(PlatformMutexTest, RecursiveLocking) {
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0) << "Recursive mutex init should succeed";

    // Lock multiple times
    const int lock_count = 5;
    for (int i = 0; i < lock_count; i++) {
        result = nimcp_platform_mutex_lock(&recursive_mutex);
        EXPECT_EQ(result, 0) << "Recursive lock " << i << " should succeed";
    }

    // Unlock same number of times
    for (int i = 0; i < lock_count; i++) {
        result = nimcp_platform_mutex_unlock(&recursive_mutex);
        EXPECT_EQ(result, 0) << "Recursive unlock " << i << " should succeed";
    }

    nimcp_platform_mutex_destroy(&recursive_mutex);
}

/**
 * TEST: RecursiveLockingNested - Lock in nested function calls
 * WHY: Verify recursive locking works in call stack context
 * WHAT: Acquire lock in outer function, then inner function
 */
TEST_F(PlatformMutexTest, RecursiveLockingNested) {
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0);

    result = nimcp_platform_mutex_lock(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Outer lock should succeed";

    {
        // Nested scope
        result = nimcp_platform_mutex_lock(&recursive_mutex);
        EXPECT_EQ(result, 0) << "Inner lock should succeed";

        result = nimcp_platform_mutex_unlock(&recursive_mutex);
        EXPECT_EQ(result, 0) << "Inner unlock should succeed";
    }

    result = nimcp_platform_mutex_unlock(&recursive_mutex);
    EXPECT_EQ(result, 0) << "Outer unlock should succeed";

    nimcp_platform_mutex_destroy(&recursive_mutex);
}

/**
 * TEST: RecursiveTrylock - Trylock on recursive mutex
 * WHY: Recursive mutex should support non-blocking locks
 * WHAT: Trylock multiple times on same recursive mutex
 */
TEST_F(PlatformMutexTest, RecursiveTrylock) {
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0);

    // Multiple trylocks should all succeed
    for (int i = 0; i < 3; i++) {
        result = nimcp_platform_mutex_trylock(&recursive_mutex);
        EXPECT_EQ(result, 0) << "Recursive trylock " << i << " should succeed";
    }

    // Unlock same number of times
    for (int i = 0; i < 3; i++) {
        result = nimcp_platform_mutex_unlock(&recursive_mutex);
        EXPECT_EQ(result, 0);
    }

    nimcp_platform_mutex_destroy(&recursive_mutex);
}

//=============================================================================
// NULL Pointer Safety Tests
//=============================================================================

/**
 * TEST: NullPointerInit - Init with NULL mutex pointer
 * WHY: Guard against NULL pointer dereference
 * WHAT: Call init with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerInit) {
    int result = nimcp_platform_mutex_init(nullptr, false);
    EXPECT_EQ(result, EINVAL) << "Init with NULL mutex should return EINVAL";
}

/**
 * TEST: NullPointerInitRecursive - Init recursive with NULL pointer
 * WHY: Guard against NULL pointer in recursive path
 * WHAT: Call recursive init with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerInitRecursive) {
    int result = nimcp_platform_mutex_init(nullptr, true);
    EXPECT_EQ(result, EINVAL) << "Recursive init with NULL should return EINVAL";
}

/**
 * TEST: NullPointerDestroy - Destroy with NULL mutex pointer
 * WHY: Guard against NULL pointer dereference in destroy
 * WHAT: Call destroy with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerDestroy) {
    int result = nimcp_platform_mutex_destroy(nullptr);
    EXPECT_EQ(result, EINVAL) << "Destroy with NULL mutex should return EINVAL";
}

/**
 * TEST: NullPointerLock - Lock with NULL mutex pointer
 * WHY: Guard against NULL pointer dereference in lock
 * WHAT: Call lock with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerLock) {
    int result = nimcp_platform_mutex_lock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Lock with NULL mutex should return EINVAL";
}

/**
 * TEST: NullPointerUnlock - Unlock with NULL mutex pointer
 * WHY: Guard against NULL pointer dereference in unlock
 * WHAT: Call unlock with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerUnlock) {
    int result = nimcp_platform_mutex_unlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Unlock with NULL mutex should return EINVAL";
}

/**
 * TEST: NullPointerTrylock - Trylock with NULL mutex pointer
 * WHY: Guard against NULL pointer dereference in trylock
 * WHAT: Call trylock with NULL mutex
 */
TEST_F(PlatformMutexTest, NullPointerTrylock) {
    int result = nimcp_platform_mutex_trylock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Trylock with NULL mutex should return EINVAL";
}

//=============================================================================
// Multi-Threaded Tests: Mutex Contention
//=============================================================================

/**
 * TEST: MultiThreadedContention - Multiple threads competing for lock
 * WHY: Verify mutex properly serializes access under contention
 * WHAT: Multiple threads increment shared counter with mutex protection
 */
TEST_F(PlatformMutexTest, MultiThreadedContention) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Shared data
    std::atomic<int> counter(0);
    int increment_count = 1000;
    int thread_count = 4;

    // Lambda to increment counter with mutex protection
    auto worker = [&]() {
        for (int i = 0; i < increment_count; i++) {
            nimcp_platform_mutex_lock(&mutex);
            counter++;
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    // Start threads
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; i++) {
        threads.emplace_back(worker);
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify counter is correct
    int expected = increment_count * thread_count;
    EXPECT_EQ(counter.load(), expected)
        << "Counter should be " << expected << " after contention test";

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: MultiThreadedContentionTrylock - Threads with trylock
 * WHY: Verify trylock behaves correctly under contention
 * WHAT: Threads using trylock + spin retry pattern
 */
TEST_F(PlatformMutexTest, MultiThreadedContentionTrylock) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    std::atomic<int> counter(0);
    std::atomic<bool> done(false);
    int thread_count = 3;
    int target_increments = 500;

    auto worker = [&]() {
        int local_count = 0;
        while (local_count < target_increments) {
            int ret = nimcp_platform_mutex_trylock(&mutex);
            if (ret == 0) {
                counter++;
                local_count++;
                nimcp_platform_mutex_unlock(&mutex);
            }
            // Busy wait on failure - acceptable for this test
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all increments succeeded
    int expected = target_increments * thread_count;
    EXPECT_EQ(counter.load(), expected)
        << "Trylock contention should succeed for all increments";

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: MultiThreadedRecursiveContention - Multiple threads with recursive mutex
 * WHY: Verify recursive mutex works under multi-threaded contention
 * WHAT: Multiple threads lock recursively
 */
TEST_F(PlatformMutexTest, MultiThreadedRecursiveContention) {
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0);

    std::atomic<int> counter(0);
    int increments_per_thread = 500;
    int thread_count = 3;

    auto worker = [&]() {
        for (int i = 0; i < increments_per_thread; i++) {
            // Simulate nested locking within same thread
            nimcp_platform_mutex_lock(&recursive_mutex);
            {
                nimcp_platform_mutex_lock(&recursive_mutex);
                counter++;
                nimcp_platform_mutex_unlock(&recursive_mutex);
            }
            nimcp_platform_mutex_unlock(&recursive_mutex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    int expected = increments_per_thread * thread_count;
    EXPECT_EQ(counter.load(), expected);

    nimcp_platform_mutex_destroy(&recursive_mutex);
}

/**
 * TEST: MultiThreadedStressTest - Long-running stress test
 * WHY: Verify mutex stability under sustained load
 * WHAT: Many threads, many iterations of lock/unlock
 */
TEST_F(PlatformMutexTest, MultiThreadedStressTest) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    std::atomic<int> operations(0);
    int thread_count = 8;
    int iterations_per_thread = 500;

    auto worker = [&]() {
        for (int i = 0; i < iterations_per_thread; i++) {
            nimcp_platform_mutex_lock(&mutex);
            operations++;
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    int expected = thread_count * iterations_per_thread;
    EXPECT_EQ(operations.load(), expected)
        << "Stress test should complete all operations";

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: MultiThreadedAlternatingLocks - Alternating lock patterns
 * WHY: Verify mutex behavior with alternating lock/unlock patterns
 * WHAT: Two threads alternately lock and unlock
 */
TEST_F(PlatformMutexTest, MultiThreadedAlternatingLocks) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    std::atomic<int> sequence(0);
    std::atomic<bool> ready(false);

    auto worker1 = [&]() {
        for (int i = 0; i < 10; i++) {
            nimcp_platform_mutex_lock(&mutex);
            // Update sequence marker
            int val = sequence.load();
            sequence.store(val + 1);
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    auto worker2 = [&]() {
        for (int i = 0; i < 10; i++) {
            nimcp_platform_mutex_lock(&mutex);
            // Update sequence marker
            int val = sequence.load();
            sequence.store(val + 1);
            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::thread t1(worker1);
    std::thread t2(worker2);

    t1.join();
    t2.join();

    // Both threads should have executed
    EXPECT_EQ(sequence.load(), 20) << "Both threads should complete";

    nimcp_platform_mutex_destroy(&mutex);
}

//=============================================================================
// Error Handling and Edge Cases
//=============================================================================

/**
 * TEST: DestroyWithoutInit - Destroy uninitialized mutex
 * WHY: Test robustness when destroy called on invalid mutex
 * WHAT: Create uninitialized mutex on stack, destroy it
 * NOTE: This test might cause undefined behavior, so it's informational
 */
// DISABLED: Undefined behavior - uncommenting may crash
// TEST_F(PlatformMutexTest, DISABLED_DestroyWithoutInit) {
//     nimcp_platform_mutex_t uninit_mutex;
//     // Not calling init
//     int result = nimcp_platform_mutex_destroy(&uninit_mutex);
//     // Result is undefined
// }

/**
 * TEST: UnlockWithoutLock - Unlock without lock
 * WHY: Test behavior when unlock called without corresponding lock
 * NOTE: This test may cause undefined behavior on some platforms
 */
// DISABLED: Platform-dependent behavior
// TEST_F(PlatformMutexTest, DISABLED_UnlockWithoutLock) {
//     int result = nimcp_platform_mutex_init(&mutex, false);
//     EXPECT_EQ(result, 0);
//
//     // Unlock without lock - undefined behavior on many platforms
//     int ret = nimcp_platform_mutex_unlock(&mutex);
//
//     nimcp_platform_mutex_destroy(&mutex);
// }

/**
 * TEST: TryLockAfterDestroy - Trylock on destroyed mutex
 * WHY: Test behavior with destroyed mutex
 * NOTE: This may cause undefined behavior
 */
// DISABLED: Undefined behavior after destroy
// TEST_F(PlatformMutexTest, DISABLED_TryLockAfterDestroy) {
//     int result = nimcp_platform_mutex_init(&mutex, false);
//     nimcp_platform_mutex_destroy(&mutex);
//     // Using destroyed mutex is undefined
//     int ret = nimcp_platform_mutex_trylock(&mutex);
// }

//=============================================================================
// Performance Characteristics (Informational)
//=============================================================================

/**
 * TEST: LockUnlockPerformance - Measure lock/unlock performance
 * WHY: Establish baseline performance expectations
 * WHAT: Time many lock/unlock cycles
 */
TEST_F(PlatformMutexTest, LockUnlockPerformance) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        nimcp_platform_mutex_lock(&mutex);
        nimcp_platform_mutex_unlock(&mutex);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (iterations * 1000000.0) / duration.count();
    GTEST_LOG_(INFO) << "Performance: " << ops_per_sec << " lock/unlock pairs per second";

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: TryLockPerformance - Measure trylock performance
 * WHY: Establish baseline trylock performance
 * WHAT: Time many trylock attempts
 */
TEST_F(PlatformMutexTest, TryLockPerformance) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        int ret = nimcp_platform_mutex_trylock(&mutex);
        if (ret == 0) {
            nimcp_platform_mutex_unlock(&mutex);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_sec = (iterations * 1000000.0) / duration.count();
    GTEST_LOG_(INFO) << "Trylock performance: " << ops_per_sec << " trylock/unlock pairs per second";

    nimcp_platform_mutex_destroy(&mutex);
}

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * TEST: MutexAsSynchronizer - Using mutex as general synchronizer
 * WHY: Verify mutex works as building block for higher-level sync
 * WHAT: Simulate critical section protection with multiple data
 */
TEST_F(PlatformMutexTest, MutexAsSynchronizer) {
    int result = nimcp_platform_mutex_init(&mutex, false);
    EXPECT_EQ(result, 0);

    // Shared state
    struct {
        int value1;
        int value2;
        float value3;
    } shared_data = {0, 0, 0.0f};

    const int threads = 4;
    const int iterations = 100;

    auto worker = [&]() {
        for (int i = 0; i < iterations; i++) {
            nimcp_platform_mutex_lock(&mutex);

            // Update all fields atomically (from mutex perspective)
            shared_data.value1++;
            shared_data.value2 += 2;
            shared_data.value3 += 1.5f;

            nimcp_platform_mutex_unlock(&mutex);
        }
    };

    std::vector<std::thread> thread_vec;
    for (int i = 0; i < threads; i++) {
        thread_vec.emplace_back(worker);
    }

    for (auto& t : thread_vec) {
        t.join();
    }

    // Verify invariants
    int expected_total = threads * iterations;
    EXPECT_EQ(shared_data.value1, expected_total);
    EXPECT_EQ(shared_data.value2, expected_total * 2);
    EXPECT_FLOAT_EQ(shared_data.value3, expected_total * 1.5f);

    nimcp_platform_mutex_destroy(&mutex);
}

/**
 * TEST: RecursiveMutexAsProducer - Recursive mutex in producer pattern
 * WHY: Verify recursive mutex works in real-world producer scenario
 * WHAT: Function that acquires lock multiple times at different levels
 */
TEST_F(PlatformMutexTest, RecursiveMutexAsProducer) {
    int result = nimcp_platform_mutex_init(&recursive_mutex, true);
    EXPECT_EQ(result, 0);

    int shared_counter = 0;

    // Helper functions that internally lock
    auto increment_helper = [&]() {
        nimcp_platform_mutex_lock(&recursive_mutex);
        shared_counter++;
        nimcp_platform_mutex_unlock(&recursive_mutex);
    };

    auto increment_twice = [&]() {
        nimcp_platform_mutex_lock(&recursive_mutex);
        increment_helper();  // Nested lock in same thread
        increment_helper();  // Another nested lock
        nimcp_platform_mutex_unlock(&recursive_mutex);
    };

    // Call nested function
    increment_twice();

    EXPECT_EQ(shared_counter, 2) << "Nested recursive locks should succeed";

    nimcp_platform_mutex_destroy(&recursive_mutex);
}

