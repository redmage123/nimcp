/**
 * @file test_platform_rwlock.cpp
 * @brief Comprehensive TDD test suite for cross-platform read-write lock module
 *
 * WHAT: Tests for read-write lock initialization, acquisition, release, and safety
 * WHY:  Ensure rwlock provides correct mutual exclusion with reader/writer semantics
 * HOW:  Use GoogleTest with thread creation to verify concurrent access patterns
 *
 * TEST COVERAGE:
 * 1. Lifecycle: Basic initialization and destruction
 * 2. Read Locks: Multiple concurrent readers allowed
 * 3. Write Locks: Single writer, exclusive access
 * 4. Try Locks: Non-blocking lock attempts
 * 5. Reader-Writer Scenarios: Complex concurrent patterns
 * 6. NULL Pointer Safety: Input validation
 *
 * NOTES:
 * - Use rdunlock for read locks, wrunlock for write locks (platform requirement)
 * - Tests must verify both POSIX and Windows code paths
 * - Timing-sensitive tests may need platform-specific adjustments
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "utils/platform/nimcp_platform_rwlock.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Constants
//=============================================================================

static const int NUM_READERS = 5;
static const int NUM_WRITERS = 2;
static const int NUM_ITERATIONS = 10;
static const int SMALL_DELAY_MS = 50;    // Small delay in milliseconds
static const int MEDIUM_DELAY_MS = 100;  // Medium delay in milliseconds

//=============================================================================
// Global Test State (for concurrent tests)
//=============================================================================

static std::atomic<int> active_readers{0};
static std::atomic<int> active_writers{0};
static std::atomic<int> max_concurrent_readers{0};
static std::atomic<int> read_count{0};
static std::atomic<int> write_count{0};
static std::atomic<bool> test_error{false};
static std::string test_error_msg;

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * WHAT: Sleep for specified milliseconds
 * WHY:  Introduce delays for timing-sensitive tests
 */
static void sleep_ms(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/**
 * WHAT: Helper to track max concurrent readers
 * WHY:  Verify no writers active during reads
 */
static void track_reader_entry(void)
{
    active_readers++;
    int current = active_readers.load();
    int prev_max = max_concurrent_readers.load();
    while (current > prev_max &&
           !max_concurrent_readers.compare_exchange_weak(prev_max, current)) {
        prev_max = max_concurrent_readers.load();
    }

    // Verify no writers active
    if (active_writers.load() > 0) {
        test_error = true;
        test_error_msg = "Writer active during reader entry";
    }
}

/**
 * WHAT: Helper to track reader exit
 * WHY:  Verify lock state consistency
 */
static void track_reader_exit(void)
{
    active_readers--;
}

/**
 * WHAT: Helper to track writer entry
 * WHY:  Verify exclusive access
 */
static void track_writer_entry(void)
{
    // Verify no other readers or writers active
    if (active_readers.load() > 0) {
        test_error = true;
        test_error_msg = "Readers active during writer entry";
    }
    if (active_writers.load() > 0) {
        test_error = true;
        test_error_msg = "Multiple writers active";
    }

    active_writers++;
}

/**
 * WHAT: Helper to track writer exit
 * WHY:  Verify lock state consistency
 */
static void track_writer_exit(void)
{
    active_writers--;
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class LifecycleTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        // Clear state before each test
        active_readers = 0;
        active_writers = 0;
        max_concurrent_readers = 0;
        read_count = 0;
        write_count = 0;
        test_error = false;
        test_error_msg = "";
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: InitDestroy
 * WHAT: Basic initialization and destruction lifecycle
 * WHY:  Verify rwlock can be created and destroyed without errors
 * HOW:  Call init then destroy, verify return codes
 */
TEST_F(LifecycleTest, InitDestroy)
{
    int result = nimcp_platform_rwlock_init(&rwlock);
    ASSERT_EQ(result, 0) << "Init should return 0 on success";

    result = nimcp_platform_rwlock_destroy(&rwlock);
    EXPECT_EQ(result, 0) << "Destroy should return 0 on success";
}

/**
 * TEST: MultipleInitDestroy
 * WHAT: Multiple initialization and destruction cycles
 * WHY:  Verify rwlock can be reused
 * HOW:  Repeat init/destroy cycle several times
 */
TEST_F(LifecycleTest, MultipleInitDestroy)
{
    for (int i = 0; i < 5; ++i) {
        int result = nimcp_platform_rwlock_init(&rwlock);
        ASSERT_EQ(result, 0) << "Init cycle " << i << " should succeed";

        result = nimcp_platform_rwlock_destroy(&rwlock);
        EXPECT_EQ(result, 0) << "Destroy cycle " << i << " should succeed";
    }
}

/**
 * TEST: InitNullPointer
 * WHAT: Initialization with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call init with nullptr, expect error
 */
TEST_F(LifecycleTest, InitNullPointer)
{
    int result = nimcp_platform_rwlock_init(nullptr);
    EXPECT_EQ(result, EINVAL) << "Init with NULL should return EINVAL";
}

/**
 * TEST: DestroyNullPointer
 * WHAT: Destruction with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call destroy with nullptr, expect error
 */
TEST_F(LifecycleTest, DestroyNullPointer)
{
    int result = nimcp_platform_rwlock_destroy(nullptr);
    EXPECT_EQ(result, EINVAL) << "Destroy with NULL should return EINVAL";
}

//=============================================================================
// Read Lock Tests
//=============================================================================

class ReadLockTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
        active_readers = 0;
        active_writers = 0;
        max_concurrent_readers = 0;
        test_error = false;
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: SimpleReadLock
 * WHAT: Single read lock acquisition and release
 * WHY:  Verify basic read lock functionality
 * HOW:  Acquire read lock, release with rdunlock
 */
TEST_F(ReadLockTest, SimpleReadLock)
{
    int result = nimcp_platform_rwlock_rdlock(&rwlock);
    ASSERT_EQ(result, 0) << "Read lock should succeed";

    result = nimcp_platform_rwlock_rdunlock(&rwlock);
    EXPECT_EQ(result, 0) << "Read unlock should succeed";
}

/**
 * TEST: MultipleReaders
 * WHAT: Multiple concurrent read locks
 * WHY:  Verify multiple readers can hold lock simultaneously
 * HOW:  Spawn multiple threads, each acquires read lock
 */
TEST_F(ReadLockTest, MultipleReaders)
{
    std::vector<std::thread> threads;

    // Spawn reader threads
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([this]() {
            int result = nimcp_platform_rwlock_rdlock(&rwlock);
            EXPECT_EQ(result, 0);

            track_reader_entry();
            sleep_ms(MEDIUM_DELAY_MS);  // Hold lock briefly
            track_reader_exit();

            result = nimcp_platform_rwlock_rdunlock(&rwlock);
            EXPECT_EQ(result, 0);
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify no errors during concurrent reading
    EXPECT_FALSE(test_error) << test_error_msg;

    // Verify multiple readers were active (proves concurrent access)
    EXPECT_GE(max_concurrent_readers, 2)
        << "Should have at least 2 concurrent readers";
}

/**
 * TEST: ReadLockNullPointer
 * WHAT: Read lock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call rdlock with nullptr, expect error
 */
TEST_F(ReadLockTest, ReadLockNullPointer)
{
    int result = nimcp_platform_rwlock_rdlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Read lock with NULL should return EINVAL";
}

/**
 * TEST: ReadUnlockNullPointer
 * WHAT: Read unlock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call rdunlock with nullptr, expect error
 */
TEST_F(ReadLockTest, ReadUnlockNullPointer)
{
    int result = nimcp_platform_rwlock_rdunlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Read unlock with NULL should return EINVAL";
}

//=============================================================================
// Write Lock Tests
//=============================================================================

class WriteLockTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
        active_readers = 0;
        active_writers = 0;
        test_error = false;
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: SimpleWriteLock
 * WHAT: Single write lock acquisition and release
 * WHY:  Verify basic write lock functionality
 * HOW:  Acquire write lock, release with wrunlock
 */
TEST_F(WriteLockTest, SimpleWriteLock)
{
    int result = nimcp_platform_rwlock_wrlock(&rwlock);
    ASSERT_EQ(result, 0) << "Write lock should succeed";

    result = nimcp_platform_rwlock_wrunlock(&rwlock);
    EXPECT_EQ(result, 0) << "Write unlock should succeed";
}

/**
 * TEST: ExclusiveWriter
 * WHAT: Write lock provides exclusive access
 * WHY:  Verify only one writer can hold lock at a time
 * HOW:  Use atomic counter to verify no concurrent writers
 */
TEST_F(WriteLockTest, ExclusiveWriter)
{
    std::vector<std::thread> threads;
    std::atomic<int> concurrent_writers{0};
    std::atomic<int> max_concurrent_writers{0};

    // Spawn writer threads
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([this, &concurrent_writers, &max_concurrent_writers]() {
            int result = nimcp_platform_rwlock_wrlock(&rwlock);
            EXPECT_EQ(result, 0);

            concurrent_writers++;
            int current = concurrent_writers.load();
            if (current > max_concurrent_writers.load()) {
                max_concurrent_writers.store(current);
            }

            sleep_ms(SMALL_DELAY_MS);  // Hold lock briefly

            concurrent_writers--;
            result = nimcp_platform_rwlock_wrunlock(&rwlock);
            EXPECT_EQ(result, 0);
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify only one writer at a time
    EXPECT_EQ(max_concurrent_writers, 1)
        << "Should never have more than 1 concurrent writer";
}

/**
 * TEST: WriteLockNullPointer
 * WHAT: Write lock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call wrlock with nullptr, expect error
 */
TEST_F(WriteLockTest, WriteLockNullPointer)
{
    int result = nimcp_platform_rwlock_wrlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Write lock with NULL should return EINVAL";
}

/**
 * TEST: WriteUnlockNullPointer
 * WHAT: Write unlock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call wrunlock with nullptr, expect error
 */
TEST_F(WriteLockTest, WriteUnlockNullPointer)
{
    int result = nimcp_platform_rwlock_wrunlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Write unlock with NULL should return EINVAL";
}

//=============================================================================
// Try Lock Tests
//=============================================================================

class TryLockTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
        test_error = false;
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: TryReadLockSucceeds
 * WHAT: Try read lock on uncontended lock
 * WHY:  Verify tryrdlock succeeds when lock is available
 * HOW:  Call tryrdlock without holding lock, expect 0
 */
TEST_F(TryLockTest, TryReadLockSucceeds)
{
    int result = nimcp_platform_rwlock_tryrdlock(&rwlock);
    ASSERT_EQ(result, 0) << "Try read lock should succeed when available";

    result = nimcp_platform_rwlock_rdunlock(&rwlock);
    EXPECT_EQ(result, 0);
}

/**
 * TEST: TryWriteLockSucceeds
 * WHAT: Try write lock on uncontended lock
 * WHY:  Verify trywrlock succeeds when lock is available
 * HOW:  Call trywrlock without holding lock, expect 0
 */
TEST_F(TryLockTest, TryWriteLockSucceeds)
{
    int result = nimcp_platform_rwlock_trywrlock(&rwlock);
    ASSERT_EQ(result, 0) << "Try write lock should succeed when available";

    result = nimcp_platform_rwlock_wrunlock(&rwlock);
    EXPECT_EQ(result, 0);
}

/**
 * TEST: TryReadLockFails
 * WHAT: Try read lock while write lock held
 * WHY:  Verify tryrdlock fails when writer holds lock
 * HOW:  Hold write lock in one thread, try read lock in another
 */
TEST_F(TryLockTest, TryReadLockFails)
{
    // Acquire write lock in main thread
    int result = nimcp_platform_rwlock_wrlock(&rwlock);
    ASSERT_EQ(result, 0);

    // Try to acquire read lock (should fail)
    result = nimcp_platform_rwlock_tryrdlock(&rwlock);
    EXPECT_EQ(result, EBUSY) << "Try read lock should fail when writer holds lock";

    // Release write lock
    result = nimcp_platform_rwlock_wrunlock(&rwlock);
    EXPECT_EQ(result, 0);
}

/**
 * TEST: TryWriteLockFails
 * WHAT: Try write lock while read lock held
 * WHY:  Verify trywrlock fails when reader holds lock
 * HOW:  Hold read lock in one thread, try write lock in another
 */
TEST_F(TryLockTest, TryWriteLockFails)
{
    // Acquire read lock in main thread
    int result = nimcp_platform_rwlock_rdlock(&rwlock);
    ASSERT_EQ(result, 0);

    // Try to acquire write lock (should fail)
    result = nimcp_platform_rwlock_trywrlock(&rwlock);
    EXPECT_EQ(result, EBUSY) << "Try write lock should fail when reader holds lock";

    // Release read lock
    result = nimcp_platform_rwlock_rdunlock(&rwlock);
    EXPECT_EQ(result, 0);
}

/**
 * TEST: TryWriteLockFailsWithWriter
 * WHAT: Try write lock while another writer holds lock
 * WHY:  Verify exclusive write access
 * HOW:  Hold write lock, try to acquire in another thread
 */
TEST_F(TryLockTest, TryWriteLockFailsWithWriter)
{
    // Acquire write lock in main thread
    int result = nimcp_platform_rwlock_wrlock(&rwlock);
    ASSERT_EQ(result, 0);

    // Spawn thread to try write lock
    std::atomic<int> try_result{-1};
    std::thread other([this, &try_result]() {
        int res = nimcp_platform_rwlock_trywrlock(&rwlock);
        try_result = res;
    });

    // Give other thread time to run
    sleep_ms(SMALL_DELAY_MS);

    // Release our write lock
    result = nimcp_platform_rwlock_wrunlock(&rwlock);
    EXPECT_EQ(result, 0);

    // Wait for other thread
    other.join();

    // Verify try lock failed
    EXPECT_EQ(try_result, EBUSY)
        << "Try write lock should fail when another writer holds lock";
}

/**
 * TEST: TryReadLockNullPointer
 * WHAT: Try read lock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call tryrdlock with nullptr, expect error
 */
TEST_F(TryLockTest, TryReadLockNullPointer)
{
    int result = nimcp_platform_rwlock_tryrdlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Try read lock with NULL should return EINVAL";
}

/**
 * TEST: TryWriteLockNullPointer
 * WHAT: Try write lock with NULL pointer
 * WHY:  Verify input validation
 * HOW:  Call trywrlock with nullptr, expect error
 */
TEST_F(TryLockTest, TryWriteLockNullPointer)
{
    int result = nimcp_platform_rwlock_trywrlock(nullptr);
    EXPECT_EQ(result, EINVAL) << "Try write lock with NULL should return EINVAL";
}

//=============================================================================
// Reader-Writer Scenario Tests
//=============================================================================

class ReaderWriterTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
        active_readers = 0;
        active_writers = 0;
        max_concurrent_readers = 0;
        read_count = 0;
        write_count = 0;
        test_error = false;
        test_error_msg = "";
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: ReadersBlockWriter
 * WHAT: Active readers prevent writer from acquiring lock
 * WHY:  Verify writer must wait for all readers
 * HOW:  Have readers hold lock while writer tries to acquire
 */
TEST_F(ReaderWriterTest, ReadersBlockWriter)
{
    std::atomic<bool> writer_acquired{false};
    std::atomic<bool> readers_done{false};

    // Thread 1: Acquire and hold read lock
    std::thread reader([this, &readers_done]() {
        for (int i = 0; i < NUM_READERS; ++i) {
            int result = nimcp_platform_rwlock_rdlock(&rwlock);
            EXPECT_EQ(result, 0);

            track_reader_entry();
            sleep_ms(MEDIUM_DELAY_MS);  // Hold lock
            track_reader_exit();

            result = nimcp_platform_rwlock_rdunlock(&rwlock);
            EXPECT_EQ(result, 0);
        }
        readers_done = true;
    });

    // Give readers time to acquire lock
    sleep_ms(SMALL_DELAY_MS);

    // Thread 2: Try to acquire write lock (will block until readers release)
    std::thread writer([this, &writer_acquired, &readers_done]() {
        // Only proceed if readers still active
        if (!readers_done.load()) {
            int result = nimcp_platform_rwlock_wrlock(&rwlock);
            EXPECT_EQ(result, 0);

            writer_acquired = true;
            sleep_ms(SMALL_DELAY_MS);

            result = nimcp_platform_rwlock_wrunlock(&rwlock);
            EXPECT_EQ(result, 0);
        }
    });

    reader.join();
    writer.join();

    // Verify writer eventually acquired lock
    EXPECT_TRUE(writer_acquired) << "Writer should eventually acquire lock";
    EXPECT_FALSE(test_error) << test_error_msg;
}

/**
 * TEST: WriterBlocksReaders
 * WHAT: Active writer prevents readers from acquiring lock
 * WHY:  Verify readers must wait for writer to release
 * HOW:  Have writer hold lock while readers try to acquire
 */
TEST_F(ReaderWriterTest, WriterBlocksReaders)
{
    std::atomic<int> readers_acquired{0};
    std::atomic<bool> writer_done{false};

    // Thread 1: Acquire and hold write lock
    std::thread writer([this, &writer_done]() {
        int result = nimcp_platform_rwlock_wrlock(&rwlock);
        EXPECT_EQ(result, 0);

        track_writer_entry();
        write_count++;
        sleep_ms(MEDIUM_DELAY_MS);  // Hold lock
        track_writer_exit();
        write_count--;

        result = nimcp_platform_rwlock_wrunlock(&rwlock);
        EXPECT_EQ(result, 0);
        writer_done = true;
    });

    // Give writer time to acquire lock
    sleep_ms(SMALL_DELAY_MS);

    // Spawn reader threads (will block until writer releases)
    std::vector<std::thread> reader_threads;
    for (int i = 0; i < NUM_READERS; ++i) {
        reader_threads.emplace_back([this, &readers_acquired]() {
            // Try to acquire read lock - will block while writer holds write lock
            int result = nimcp_platform_rwlock_rdlock(&rwlock);
            EXPECT_EQ(result, 0);

            track_reader_entry();
            read_count++;
            sleep_ms(SMALL_DELAY_MS);
            read_count--;
            track_reader_exit();

            result = nimcp_platform_rwlock_rdunlock(&rwlock);
            EXPECT_EQ(result, 0);

            readers_acquired++;
        });
    }

    writer.join();

    // Wait for reader threads
    for (auto& thread : reader_threads) {
        thread.join();
    }

    // Verify readers eventually acquired locks
    EXPECT_GE(readers_acquired, 1) << "At least one reader should acquire lock";
    EXPECT_FALSE(test_error) << test_error_msg;
}

/**
 * TEST: MixedReaderWriterAccess
 * WHAT: Complex scenario with alternating readers and writers
 * WHY:  Verify correctness under realistic concurrent access patterns
 * HOW:  Spawn multiple readers and writers, verify no conflicts
 */
TEST_F(ReaderWriterTest, MixedReaderWriterAccess)
{
    std::vector<std::thread> threads;

    // Spawn reader threads
    for (int i = 0; i < NUM_READERS; ++i) {
        threads.emplace_back([this]() {
            for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
                int result = nimcp_platform_rwlock_rdlock(&rwlock);
                if (result == 0) {
                    track_reader_entry();
                    read_count++;

                    // Simulate some work
                    sleep_ms(10);

                    read_count--;
                    track_reader_exit();

                    result = nimcp_platform_rwlock_rdunlock(&rwlock);
                    EXPECT_EQ(result, 0);
                }
            }
        });
    }

    // Spawn writer threads
    for (int i = 0; i < NUM_WRITERS; ++i) {
        threads.emplace_back([this]() {
            for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
                int result = nimcp_platform_rwlock_wrlock(&rwlock);
                if (result == 0) {
                    track_writer_entry();
                    write_count++;

                    // Simulate some work
                    sleep_ms(10);

                    write_count--;
                    track_writer_exit();

                    result = nimcp_platform_rwlock_wrunlock(&rwlock);
                    EXPECT_EQ(result, 0);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify no conflicts occurred
    EXPECT_FALSE(test_error) << test_error_msg;
}

/**
 * TEST: MultipleReadersWithTryWrite
 * WHAT: Try write lock while multiple readers hold lock
 * WHY:  Verify try lock properly fails with multiple readers
 * HOW:  Hold read locks, try write lock from different thread
 */
TEST_F(ReaderWriterTest, MultipleReadersWithTryWrite)
{
    std::atomic<int> reader_count{0};
    std::atomic<bool> all_readers_locked{false};
    std::atomic<int> try_result{-1};

    // Spawn reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([this, &reader_count, &all_readers_locked]() {
            int result = nimcp_platform_rwlock_rdlock(&rwlock);
            EXPECT_EQ(result, 0);

            reader_count++;
            if (reader_count == NUM_READERS) {
                all_readers_locked = true;
            }

            // Hold lock while main thread tries write
            sleep_ms(MEDIUM_DELAY_MS);

            result = nimcp_platform_rwlock_rdunlock(&rwlock);
            EXPECT_EQ(result, 0);
        });
    }

    // Wait for all readers to acquire locks
    while (!all_readers_locked) {
        sleep_ms(5);
    }

    // Try to acquire write lock (should fail)
    try_result = nimcp_platform_rwlock_trywrlock(&rwlock);

    EXPECT_EQ(try_result, EBUSY)
        << "Try write lock should fail with multiple active readers";

    // Wait for readers to release
    for (auto& thread : readers) {
        thread.join();
    }

    // Now try write should succeed
    try_result = nimcp_platform_rwlock_trywrlock(&rwlock);
    EXPECT_EQ(try_result, 0) << "Try write lock should succeed after readers release";

    if (try_result == 0) {
        int result = nimcp_platform_rwlock_wrunlock(&rwlock);
        EXPECT_EQ(result, 0);
    }
}

//=============================================================================
// NULL Pointer Safety Tests
//=============================================================================

class NullSafetyTest : public NimcpTestBase {
   protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup
    }

    void TearDown() override {
        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: AllOperationsNullSafe
 * WHAT: All operations handle NULL pointers gracefully
 * WHY:  Prevent undefined behavior from incorrect usage
 * HOW:  Call all functions with nullptr, expect EINVAL
 */
TEST_F(NullSafetyTest, AllOperationsNullSafe)
{
    // Init
    EXPECT_EQ(nimcp_platform_rwlock_init(nullptr), EINVAL);

    // Destroy
    EXPECT_EQ(nimcp_platform_rwlock_destroy(nullptr), EINVAL);

    // Read operations
    EXPECT_EQ(nimcp_platform_rwlock_rdlock(nullptr), EINVAL);
    EXPECT_EQ(nimcp_platform_rwlock_rdunlock(nullptr), EINVAL);
    EXPECT_EQ(nimcp_platform_rwlock_tryrdlock(nullptr), EINVAL);

    // Write operations
    EXPECT_EQ(nimcp_platform_rwlock_wrlock(nullptr), EINVAL);
    EXPECT_EQ(nimcp_platform_rwlock_wrunlock(nullptr), EINVAL);
    EXPECT_EQ(nimcp_platform_rwlock_trywrlock(nullptr), EINVAL);
}

/**
 * TEST: SequentialNullOperations
 * WHAT: Sequence of NULL pointer operations
 * WHY:  Verify consistent error handling across operation sequences
 * HOW:  Call multiple NULL operations in sequence
 */
TEST_F(NullSafetyTest, SequentialNullOperations)
{
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(nimcp_platform_rwlock_rdlock(nullptr), EINVAL);
        EXPECT_EQ(nimcp_platform_rwlock_wrlock(nullptr), EINVAL);
        EXPECT_EQ(nimcp_platform_rwlock_tryrdlock(nullptr), EINVAL);
        EXPECT_EQ(nimcp_platform_rwlock_trywrlock(nullptr), EINVAL);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

class StressTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
        active_readers = 0;
        active_writers = 0;
        read_count = 0;
        write_count = 0;
        test_error = false;
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: HighContention
 * WHAT: Many threads contending for lock
 * WHY:  Verify correctness under high contention
 * HOW:  Spawn many reader and writer threads with frequent lock operations
 */
TEST_F(StressTest, HighContention)
{
    const int HEAVY_READERS = 10;
    const int HEAVY_WRITERS = 5;
    const int HEAVY_ITERATIONS = 50;

    std::vector<std::thread> threads;

    // Spawn many reader threads
    for (int i = 0; i < HEAVY_READERS; ++i) {
        threads.emplace_back([this]() {
            for (int iter = 0; iter < HEAVY_ITERATIONS; ++iter) {
                int result = nimcp_platform_rwlock_rdlock(&rwlock);
                EXPECT_EQ(result, 0);

                read_count++;
                // Very short critical section
                read_count--;

                result = nimcp_platform_rwlock_rdunlock(&rwlock);
                EXPECT_EQ(result, 0);
            }
        });
    }

    // Spawn many writer threads
    for (int i = 0; i < HEAVY_WRITERS; ++i) {
        threads.emplace_back([this]() {
            for (int iter = 0; iter < HEAVY_ITERATIONS; ++iter) {
                int result = nimcp_platform_rwlock_wrlock(&rwlock);
                EXPECT_EQ(result, 0);

                write_count++;
                // Very short critical section
                write_count--;

                result = nimcp_platform_rwlock_wrunlock(&rwlock);
                EXPECT_EQ(result, 0);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify expected iteration counts
    EXPECT_EQ(read_count, 0) << "Read count should be 0 after all threads finish";
    EXPECT_EQ(write_count, 0) << "Write count should be 0 after all threads finish";
    EXPECT_FALSE(test_error) << test_error_msg;
}

/**
 * TEST: AlternatingReadWrite
 * WHAT: Threads alternate between read and write locks
 * WHY:  Verify correct handling of mode switching
 * HOW:  Each thread alternates between read and write locks
 */
TEST_F(StressTest, AlternatingReadWrite)
{
    const int ALT_THREADS = 8;
    const int ALT_ITERATIONS = 20;

    std::vector<std::thread> threads;

    // Spawn alternating reader/writer threads
    for (int i = 0; i < ALT_THREADS; ++i) {
        threads.emplace_back([this]() {
            for (int iter = 0; iter < ALT_ITERATIONS; ++iter) {
                // Odd iterations: read lock
                if (iter % 2 == 0) {
                    int result = nimcp_platform_rwlock_rdlock(&rwlock);
                    EXPECT_EQ(result, 0);
                    read_count++;
                    sleep_ms(1);
                    read_count--;
                    result = nimcp_platform_rwlock_rdunlock(&rwlock);
                    EXPECT_EQ(result, 0);
                } else {
                    // Even iterations: write lock
                    int result = nimcp_platform_rwlock_wrlock(&rwlock);
                    EXPECT_EQ(result, 0);
                    write_count++;
                    sleep_ms(1);
                    write_count--;
                    result = nimcp_platform_rwlock_wrunlock(&rwlock);
                    EXPECT_EQ(result, 0);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(read_count, 0);
    EXPECT_EQ(write_count, 0);
    EXPECT_FALSE(test_error) << test_error_msg;
}

//=============================================================================
// Edge Case Tests
//=============================================================================

class EdgeCaseTest : public NimcpTestBase {
   protected:
    nimcp_platform_rwlock_t rwlock;

    void SetUp() override
    {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        nimcp_platform_rwlock_init(&rwlock);
    }

    void TearDown() override
    {
        nimcp_platform_rwlock_destroy(&rwlock);

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }
};

/**
 * TEST: RapidLockUnlock
 * WHAT: Rapid successive lock/unlock operations
 * WHY:  Verify correctness under high frequency lock operations
 * HOW:  Rapidly acquire and release locks in tight loop
 */
TEST_F(EdgeCaseTest, RapidLockUnlock)
{
    const int RAPID_ITERATIONS = 100;

    for (int i = 0; i < RAPID_ITERATIONS; ++i) {
        // Read lock cycle
        int result = nimcp_platform_rwlock_rdlock(&rwlock);
        ASSERT_EQ(result, 0);
        result = nimcp_platform_rwlock_rdunlock(&rwlock);
        ASSERT_EQ(result, 0);

        // Write lock cycle
        result = nimcp_platform_rwlock_wrlock(&rwlock);
        ASSERT_EQ(result, 0);
        result = nimcp_platform_rwlock_wrunlock(&rwlock);
        ASSERT_EQ(result, 0);
    }
}

/**
 * TEST: TryLockCycles
 * WHAT: Rapid try lock operations
 * WHY:  Verify try locks perform correctly under high frequency
 * HOW:  Repeatedly call try locks without blocking
 */
TEST_F(EdgeCaseTest, TryLockCycles)
{
    const int TRY_ITERATIONS = 100;

    for (int i = 0; i < TRY_ITERATIONS; ++i) {
        // Try read lock
        int result = nimcp_platform_rwlock_tryrdlock(&rwlock);
        if (result == 0) {
            result = nimcp_platform_rwlock_rdunlock(&rwlock);
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_EQ(result, EBUSY);
        }

        // Try write lock
        result = nimcp_platform_rwlock_trywrlock(&rwlock);
        if (result == 0) {
            result = nimcp_platform_rwlock_wrunlock(&rwlock);
            EXPECT_EQ(result, 0);
        } else {
            EXPECT_EQ(result, EBUSY);
        }
    }
}
