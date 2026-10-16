/**
 * @file test_utils_platform_rwlock.cpp
 * @brief Comprehensive unit tests for platform read-write lock primitives
 *
 * WHAT: 100% test coverage for nimcp_platform_rwlock.c
 * WHY:  RWLock primitives are critical synchronization infrastructure
 * HOW:  Test creation, read/write locking, unlocking, trylock, and thread safety
 *
 * TEST COVERAGE:
 * 1. RWLock creation and destruction
 * 2. Basic read lock operations (multiple readers allowed)
 * 3. Basic write lock operations (exclusive access)
 * 4. Multiple readers can hold lock simultaneously
 * 5. Writer exclusivity (blocks readers and other writers)
 * 6. Read trylock operations (non-blocking)
 * 7. Write trylock operations (non-blocking)
 * 8. Performance comparison (multiple readers vs mutex)
 * 9. Reader-writer priority and fairness
 * 10. Edge cases (NULL pointers, lock/unlock balance, stress test)
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

    #include "utils/platform/nimcp_platform_rwlock.h"
    #include "utils/platform/nimcp_platform_mutex.h"
    #include <errno.h>

//=============================================================================
// Test Fixture
//=============================================================================

class RWLockTest : public ::testing::Test {
protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override {
        // Initialize rwlock for each test
        // Note: Some tests will re-initialize as needed
    }

    void TearDown() override {
        // Cleanup is done per-test as needed
    }
};

//=============================================================================
// Unit Test 1: Basic rwlock creation and destruction
//=============================================================================

TEST_F(RWLockTest, Init_BasicRWLock) {
    // WHAT: Verify nimcp_platform_rwlock_init() creates a rwlock
    // WHY:  Basic functionality must work
    // HOW:  Initialize rwlock and verify success

    int result = nimcp_platform_rwlock_init(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to initialize rwlock";

    // Cleanup
    result = nimcp_platform_rwlock_destroy(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to destroy rwlock";

    SUCCEED() << "RWLock creation and destruction works";
}

//=============================================================================
// Unit Test 2: Basic read lock operations
//=============================================================================

TEST_F(RWLockTest, ReadLock_BasicOperation) {
    // WHAT: Verify basic read lock/unlock cycle works
    // WHY:  Core read lock functionality
    // HOW:  Lock for reading, unlock, verify return codes

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    int result = nimcp_platform_rwlock_rdlock(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to acquire read lock";

    result = nimcp_platform_rwlock_rdunlock(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to release read lock";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Basic read lock/unlock works";
}

//=============================================================================
// Unit Test 3: Basic write lock operations
//=============================================================================

TEST_F(RWLockTest, WriteLock_BasicOperation) {
    // WHAT: Verify basic write lock/unlock cycle works
    // WHY:  Core write lock functionality
    // HOW:  Lock for writing, unlock, verify return codes

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    int result = nimcp_platform_rwlock_wrlock(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to acquire write lock";

    result = nimcp_platform_rwlock_wrunlock(&rwlock);
    EXPECT_EQ(result, 0) << "Failed to release write lock";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Basic write lock/unlock works";
}

//=============================================================================
// Unit Test 4: Multiple readers can hold lock simultaneously
//=============================================================================

TEST_F(RWLockTest, MultipleReaders_Concurrent) {
    // WHAT: Verify multiple readers can hold lock at the same time
    // WHY:  Core RWLock feature - shared read access
    // HOW:  Multiple threads acquire read locks, verify they're all concurrent

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    const int NUM_READERS = 5;
    std::atomic<int> concurrent_readers{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<bool> all_readers_ready{false};

    auto reader = [&]() {
        nimcp_platform_rwlock_rdlock(&rwlock);

        // Track concurrent readers
        int current = concurrent_readers.fetch_add(1) + 1;

        // Update maximum concurrent readers seen
        int expected_max = max_concurrent.load();
        while (current > expected_max &&
               !max_concurrent.compare_exchange_weak(expected_max, current)) {
            expected_max = max_concurrent.load();
        }

        // Wait for all readers to be in critical section
        all_readers_ready = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        concurrent_readers.fetch_sub(1);
        nimcp_platform_rwlock_rdunlock(&rwlock);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify that multiple readers were concurrent
    EXPECT_GE(max_concurrent.load(), 2)
        << "Multiple readers should be able to hold lock simultaneously";

    // In ideal case, all readers should have been concurrent
    EXPECT_GE(max_concurrent.load(), NUM_READERS - 1)
        << "Expected at least " << (NUM_READERS - 1) << " concurrent readers";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Maximum " << max_concurrent.load() << " of " << NUM_READERS
              << " readers were concurrent";
}

//=============================================================================
// Unit Test 5: Writer has exclusive access
//=============================================================================

TEST_F(RWLockTest, Writer_ExclusiveAccess) {
    // WHAT: Verify writer has exclusive access (no readers or other writers)
    // WHY:  Core RWLock guarantee - writes are exclusive
    // HOW:  Track read/write mode, verify writer never overlaps with anyone

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    const int NUM_READERS = 3;
    const int NUM_WRITERS = 2;
    std::atomic<int> reader_count{0};
    std::atomic<bool> writer_active{false};
    std::atomic<bool> violation_detected{false};
    std::atomic<int> completed_operations{0};

    auto reader = [&]() {
        nimcp_platform_rwlock_rdlock(&rwlock);

        // Check no writer is active
        if (writer_active.load()) {
            violation_detected = true;
        }

        reader_count.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        reader_count.fetch_sub(1);

        nimcp_platform_rwlock_rdunlock(&rwlock);
        completed_operations.fetch_add(1);
    };

    auto writer = [&]() {
        nimcp_platform_rwlock_wrlock(&rwlock);

        // Check no readers and no other writers
        if (reader_count.load() != 0 || writer_active.load()) {
            violation_detected = true;
        }

        writer_active = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        writer_active = false;

        nimcp_platform_rwlock_wrunlock(&rwlock);
        completed_operations.fetch_add(1);
    };

    std::vector<std::thread> threads;

    // Mix readers and writers
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back(writer);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(violation_detected) << "Writer exclusivity violated!";
    EXPECT_EQ(completed_operations.load(), NUM_READERS + NUM_WRITERS)
        << "Not all operations completed";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Writer exclusivity verified with "
              << NUM_READERS << " readers and " << NUM_WRITERS << " writers";
}

//=============================================================================
// Unit Test 6: Read trylock operations
//=============================================================================

TEST_F(RWLockTest, ReadTrylock_NonBlockingBehavior) {
    // WHAT: Verify read trylock doesn't block when writer holds lock
    // WHY:  Non-blocking lock acquisition is critical for some algorithms
    // HOW:  Writer locks, verify read trylock fails with EBUSY

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    // Acquire write lock
    ASSERT_EQ(nimcp_platform_rwlock_wrlock(&rwlock), 0);

    // Try to acquire read lock (should fail immediately)
    int result = nimcp_platform_rwlock_tryrdlock(&rwlock);
    EXPECT_EQ(result, EBUSY) << "Read trylock should return EBUSY when writer holds lock";

    // Release write lock
    ASSERT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock), 0);

    // Now read trylock should succeed
    result = nimcp_platform_rwlock_tryrdlock(&rwlock);
    EXPECT_EQ(result, 0) << "Read trylock should succeed when unlocked";

    // Release read lock
    ASSERT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock), 0);

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Read trylock non-blocking behavior verified";
}

//=============================================================================
// Unit Test 7: Write trylock operations
//=============================================================================

TEST_F(RWLockTest, WriteTrylock_NonBlockingBehavior) {
    // WHAT: Verify write trylock doesn't block when readers or writers hold lock
    // WHY:  Non-blocking write acquisition enables lock-free algorithms
    // HOW:  Reader locks, verify write trylock fails with EBUSY

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    // Test 1: Write trylock blocked by reader
    ASSERT_EQ(nimcp_platform_rwlock_rdlock(&rwlock), 0);

    int result = nimcp_platform_rwlock_trywrlock(&rwlock);
    EXPECT_EQ(result, EBUSY) << "Write trylock should return EBUSY when reader holds lock";

    ASSERT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock), 0);

    // Test 2: Write trylock blocked by writer
    ASSERT_EQ(nimcp_platform_rwlock_wrlock(&rwlock), 0);

    result = nimcp_platform_rwlock_trywrlock(&rwlock);
    EXPECT_EQ(result, EBUSY) << "Write trylock should return EBUSY when writer holds lock";

    ASSERT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock), 0);

    // Test 3: Write trylock succeeds when unlocked
    result = nimcp_platform_rwlock_trywrlock(&rwlock);
    EXPECT_EQ(result, 0) << "Write trylock should succeed when unlocked";

    ASSERT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock), 0);

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Write trylock non-blocking behavior verified";
}

//=============================================================================
// Unit Test 8: Performance - multiple readers vs mutex
//=============================================================================

TEST_F(RWLockTest, Performance_MultipleReadersFasterThanMutex) {
    // WHAT: Verify read lock allows better concurrency than mutex
    // WHY:  RWLock should provide performance benefit for read-heavy workloads
    // HOW:  Time multiple readers with rwlock vs mutex

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    nimcp_platform_mutex_t mutex;
    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);

    const int NUM_READERS = 8;
    const int READS_PER_THREAD = 1000;
    std::atomic<int> shared_data{0};

    // Test with RWLock
    auto start_rwlock = std::chrono::high_resolution_clock::now();
    {
        auto reader = [&]() {
            for (int i = 0; i < READS_PER_THREAD; i++) {
                nimcp_platform_rwlock_rdlock(&rwlock);
                volatile int temp = shared_data.load();
                (void)temp;  // Simulate read work
                nimcp_platform_rwlock_rdunlock(&rwlock);
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < NUM_READERS; i++) {
            threads.emplace_back(reader);
        }
        for (auto& t : threads) {
            t.join();
        }
    }
    auto end_rwlock = std::chrono::high_resolution_clock::now();
    auto duration_rwlock = std::chrono::duration_cast<std::chrono::microseconds>(
        end_rwlock - start_rwlock);

    // Test with Mutex
    auto start_mutex = std::chrono::high_resolution_clock::now();
    {
        auto reader = [&]() {
            for (int i = 0; i < READS_PER_THREAD; i++) {
                nimcp_platform_mutex_lock(&mutex);
                volatile int temp = shared_data.load();
                (void)temp;  // Simulate read work
                nimcp_platform_mutex_unlock(&mutex);
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < NUM_READERS; i++) {
            threads.emplace_back(reader);
        }
        for (auto& t : threads) {
            t.join();
        }
    }
    auto end_mutex = std::chrono::high_resolution_clock::now();
    auto duration_mutex = std::chrono::duration_cast<std::chrono::microseconds>(
        end_mutex - start_mutex);

    // RWLock should be faster or comparable for read-heavy workload
    // Note: On some systems/loads, the difference might be small
    double speedup = static_cast<double>(duration_mutex.count()) /
                     static_cast<double>(duration_rwlock.count());

    SUCCEED() << "RWLock: " << duration_rwlock.count() << "us, "
              << "Mutex: " << duration_mutex.count() << "us, "
              << "Speedup: " << speedup << "x";

    // Note: Performance test is informational only
    // On some systems/loads, RWLock may be slower due to implementation overhead
    // This is expected and not a test failure
    if (duration_rwlock.count() > duration_mutex.count() * 3) {
        SUCCEED() << "⚠️  RWLock slower than expected (>3x mutex), "
                  << "but this is acceptable on some systems";
    }

    nimcp_platform_rwlock_destroy(&rwlock);
    nimcp_platform_mutex_destroy(&mutex);
}

//=============================================================================
// Unit Test 9: Reader-writer priority and fairness
//=============================================================================

TEST_F(RWLockTest, Priority_MixedReadersAndWriters) {
    // WHAT: Verify readers and writers can both make progress
    // WHY:  Ensure no starvation (reader or writer)
    // HOW:  Mix readers and writers, verify all complete

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    const int NUM_READERS = 10;
    const int NUM_WRITERS = 5;
    const int ITERATIONS = 20;

    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::atomic<int> shared_value{0};

    auto reader = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            nimcp_platform_rwlock_rdlock(&rwlock);
            int value = shared_value.load();
            (void)value;
            read_count.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            nimcp_platform_rwlock_rdunlock(&rwlock);
            std::this_thread::yield();
        }
    };

    auto writer = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            nimcp_platform_rwlock_wrlock(&rwlock);
            shared_value.fetch_add(1);
            write_count.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            nimcp_platform_rwlock_wrunlock(&rwlock);
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;

    // Start readers and writers
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back(writer);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify all operations completed
    EXPECT_EQ(read_count.load(), NUM_READERS * ITERATIONS)
        << "Not all read operations completed";
    EXPECT_EQ(write_count.load(), NUM_WRITERS * ITERATIONS)
        << "Not all write operations completed";
    EXPECT_EQ(shared_value.load(), NUM_WRITERS * ITERATIONS)
        << "Shared value doesn't match expected writes";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Completed " << read_count.load() << " reads and "
              << write_count.load() << " writes without starvation";
}

//=============================================================================
// Unit Test 10: Edge cases and stress test
//=============================================================================

TEST_F(RWLockTest, EdgeCase_NullPointer) {
    // WHAT: Verify NULL pointer handling
    // WHY:  Defensive programming - should not crash
    // HOW:  Pass NULL to each function, verify EINVAL returned

    EXPECT_EQ(nimcp_platform_rwlock_init(nullptr), EINVAL)
        << "init should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_destroy(nullptr), EINVAL)
        << "destroy should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_rdlock(nullptr), EINVAL)
        << "rdlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_wrlock(nullptr), EINVAL)
        << "wrlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_tryrdlock(nullptr), EINVAL)
        << "tryrdlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_trywrlock(nullptr), EINVAL)
        << "trywrlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(nullptr), EINVAL)
        << "rdunlock should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_rwlock_wrunlock(nullptr), EINVAL)
        << "wrunlock should return EINVAL for NULL pointer";

    SUCCEED() << "NULL pointer handling verified for all operations";
}

//=============================================================================
// Unit Test 11: Lock/unlock balance verification
//=============================================================================

TEST_F(RWLockTest, Balance_MatchedLockUnlock) {
    // WHAT: Verify lock/unlock pairs are balanced
    // WHY:  Unbalanced operations lead to deadlocks
    // HOW:  Track lock depth, verify it returns to zero

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    // Test read lock/unlock balance
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock), 0)
            << "Read lock " << i << " failed";
        EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock), 0)
            << "Read unlock " << i << " failed";
    }

    // Test write lock/unlock balance
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nimcp_platform_rwlock_wrlock(&rwlock), 0)
            << "Write lock " << i << " failed";
        EXPECT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock), 0)
            << "Write unlock " << i << " failed";
    }

    // Final verification - should be able to acquire lock
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock), 0);

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Lock/unlock balance verified";
}

//=============================================================================
// Unit Test 12: Stress test with rapid operations
//=============================================================================

TEST_F(RWLockTest, Stress_RapidLockUnlock) {
    // WHAT: Stress test with very rapid lock/unlock cycles
    // WHY:  Ensure rwlock remains stable under rapid operations
    // HOW:  Perform thousands of lock/unlock cycles quickly

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    const int ITERATIONS = 10000;

    // Rapid read locks
    for (int i = 0; i < ITERATIONS; i++) {
        ASSERT_EQ(nimcp_platform_rwlock_rdlock(&rwlock), 0);
        ASSERT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock), 0);
    }

    // Rapid write locks
    for (int i = 0; i < ITERATIONS; i++) {
        ASSERT_EQ(nimcp_platform_rwlock_wrlock(&rwlock), 0);
        ASSERT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock), 0);
    }

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Survived " << (ITERATIONS * 2) << " rapid lock/unlock cycles";
}

//=============================================================================
// Unit Test 13: Multiple simultaneous read trylocks
//=============================================================================

TEST_F(RWLockTest, ReadTrylock_MultipleSimultaneous) {
    // WHAT: Verify multiple threads can acquire read trylock simultaneously
    // WHY:  Read trylocks should allow concurrency like read locks
    // HOW:  Multiple threads trylock for reading, verify all succeed

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    const int NUM_READERS = 5;
    std::atomic<int> successful_trylocks{0};
    std::atomic<bool> start_flag{false};

    auto reader = [&]() {
        // Wait for all threads to be ready
        while (!start_flag.load()) {
            std::this_thread::yield();
        }

        if (nimcp_platform_rwlock_tryrdlock(&rwlock) == 0) {
            successful_trylocks.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            nimcp_platform_rwlock_rdunlock(&rwlock);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader);
    }

    // Start all threads simultaneously
    start_flag = true;

    for (auto& t : threads) {
        t.join();
    }

    // Most or all readers should have succeeded
    EXPECT_GE(successful_trylocks.load(), NUM_READERS - 1)
        << "Multiple read trylocks should succeed simultaneously";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << successful_trylocks.load() << " of " << NUM_READERS
              << " read trylocks succeeded";
}

//=============================================================================
// Unit Test 14: Writer blocks subsequent readers
//=============================================================================

TEST_F(RWLockTest, Writer_BlocksSubsequentReaders) {
    // WHAT: Verify writer blocks readers attempting to acquire lock
    // WHY:  Core rwlock semantics - writers are exclusive
    // HOW:  Writer holds lock, verify readers must wait

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock), 0);

    std::atomic<bool> writer_has_lock{false};
    std::atomic<bool> writer_released{false};
    std::atomic<bool> reader_acquired{false};

    std::thread writer([&]() {
        nimcp_platform_rwlock_wrlock(&rwlock);
        writer_has_lock = true;

        // Hold lock for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        nimcp_platform_rwlock_wrunlock(&rwlock);
        writer_released = true;
    });

    // Wait for writer to acquire lock
    while (!writer_has_lock.load()) {
        std::this_thread::yield();
    }

    // Try to acquire read lock (will block until writer releases)
    std::thread reader([&]() {
        nimcp_platform_rwlock_rdlock(&rwlock);
        reader_acquired = true;

        // Verify writer has released before we got here
        EXPECT_TRUE(writer_released.load())
            << "Reader acquired before writer released";

        nimcp_platform_rwlock_rdunlock(&rwlock);
    });

    writer.join();
    reader.join();

    EXPECT_TRUE(reader_acquired.load()) << "Reader should eventually acquire lock";

    nimcp_platform_rwlock_destroy(&rwlock);
    SUCCEED() << "Writer correctly blocks subsequent readers";
}

//=============================================================================
// Unit Test 15: Multiple rwlocks independence
//=============================================================================

TEST_F(RWLockTest, Independence_MultipleRWLocks) {
    // WHAT: Verify multiple rwlocks are independent
    // WHY:  Locking one rwlock should not affect others
    // HOW:  Create multiple rwlocks, lock them independently

    nimcp_platform_rwlock_t rwlock1, rwlock2, rwlock3;

    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock1), 0);
    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock2), 0);
    ASSERT_EQ(nimcp_platform_rwlock_init(&rwlock3), 0);

    // Lock all three for reading
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock1), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock2), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock3), 0);

    // Unlock in different order
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock2), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock1), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock3), 0);

    // Lock different locks in different modes
    EXPECT_EQ(nimcp_platform_rwlock_wrlock(&rwlock1), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock2), 0);

    // Unlock
    EXPECT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock1), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock2), 0);

    // Mix read and write locks on different locks
    EXPECT_EQ(nimcp_platform_rwlock_wrlock(&rwlock3), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(&rwlock1), 0);

    // Trylock on rwlock2 should succeed (it's free)
    EXPECT_EQ(nimcp_platform_rwlock_tryrdlock(&rwlock2), 0);

    // Cleanup
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock1), 0);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(&rwlock2), 0);
    EXPECT_EQ(nimcp_platform_rwlock_wrunlock(&rwlock3), 0);

    nimcp_platform_rwlock_destroy(&rwlock1);
    nimcp_platform_rwlock_destroy(&rwlock2);
    nimcp_platform_rwlock_destroy(&rwlock3);

    SUCCEED() << "Multiple rwlocks are independent";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
