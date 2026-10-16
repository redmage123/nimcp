/**
 * @file unit_utils_thread_test_rwlock_extended.cpp
 * @brief Unit tests for extended RW lock and thread naming features
 *
 * WHAT: Tests for trylock, timed lock, thread naming, and CPU affinity
 * WHY:  Verify new thread utility features work correctly
 * HOW:  Google Test framework with concurrent access patterns
 *
 * DESIGN PATTERNS:
 * - Fixture Pattern: Common setup/teardown for tests
 * - Template Method: Reusable test patterns
 * - Strategy Pattern: Different lock acquisition strategies
 *
 * TEST PHILOSOPHY:
 * - TDD: Tests drive implementation
 * - Concurrent testing: Verify thread-safe operations
 * - Timeout testing: Verify timed operations work correctly
 * - Platform awareness: Skip tests on unsupported platforms
 *
 * @author NIMCP Development Team
 * @date 2025-11-26
 */

#include <gtest/gtest.h>
#include "utils/thread/nimcp_thread.h"
#include <pthread.h>
#include <thread>
#include <chrono>
#include <atomic>

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Test fixture for RW lock extended operations
 *
 * WHAT: Provides common setup/teardown for rwlock tests
 * WHY:  Avoid code duplication, ensure clean state
 */
class RWLockExtendedTest : public ::testing::Test {
protected:
    nimcp_rwlock_t lock;

    void SetUp() override {
        /* WHAT: Initialize RW lock for each test
         * WHY:  Clean state, no interference between tests
         */
        ASSERT_EQ(nimcp_rwlock_init(&lock), NIMCP_SUCCESS);
    }

    void TearDown() override {
        /* WHAT: Destroy RW lock after each test
         * WHY:  Free resources, prevent leaks
         */
        nimcp_rwlock_destroy(&lock);
    }
};

/**
 * @brief Test fixture for thread naming and affinity
 *
 * WHAT: Provides common setup for thread-related tests
 * WHY:  Consistent test environment
 */
class ThreadNamingTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* WHAT: Initialize thread subsystem
         * WHY:  Ensure threading utilities are ready
         */
        nimcp_thread_init();
    }
};

//=============================================================================
// RW Lock Try Operations
//=============================================================================

/**
 * @brief Test tryrdlock when lock is available
 *
 * WHAT: Acquire read lock without blocking when available
 * WHY:  Verify tryrdlock succeeds when no writer holds lock
 */
TEST_F(RWLockExtendedTest, TryRdLock_Success) {
    /* WHAT: Try to acquire read lock (should succeed immediately)
     * WHY:  No writer active, lock available
     */
    EXPECT_EQ(nimcp_rwlock_tryrdlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Release the lock
     * WHY:  Clean up for test completion
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test tryrdlock when write lock is held
 *
 * WHAT: Try to acquire read lock when writer holds lock
 * WHY:  Verify tryrdlock returns NIMCP_BUSY (doesn't block)
 */
TEST_F(RWLockExtendedTest, TryRdLock_WhenWriteLocked) {
    /* WHAT: Acquire write lock first
     * WHY:  Block read lock acquisition
     */
    ASSERT_EQ(nimcp_rwlock_wrlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Try to acquire read lock (should fail immediately)
     * WHY:  Writer holds lock, tryrdlock should return BUSY
     */
    EXPECT_EQ(nimcp_rwlock_tryrdlock(&lock), NIMCP_BUSY);

    /* WHAT: Release write lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test trywrlock when lock is available
 *
 * WHAT: Acquire write lock without blocking when available
 * WHY:  Verify trywrlock succeeds when no lock held
 */
TEST_F(RWLockExtendedTest, TryWrLock_Success) {
    /* WHAT: Try to acquire write lock (should succeed immediately)
     * WHY:  No lock active, lock available
     */
    EXPECT_EQ(nimcp_rwlock_trywrlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Release the lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test trywrlock when read lock is held
 *
 * WHAT: Try to acquire write lock when reader holds lock
 * WHY:  Verify trywrlock returns NIMCP_BUSY (doesn't block)
 */
TEST_F(RWLockExtendedTest, TryWrLock_WhenReadLocked) {
    /* WHAT: Acquire read lock first
     * WHY:  Block write lock acquisition
     */
    ASSERT_EQ(nimcp_rwlock_rdlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Try to acquire write lock (should fail immediately)
     * WHY:  Reader holds lock, trywrlock should return BUSY
     */
    EXPECT_EQ(nimcp_rwlock_trywrlock(&lock), NIMCP_BUSY);

    /* WHAT: Release read lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test trywrlock when write lock is already held
 *
 * WHAT: Try to acquire write lock when writer holds lock
 * WHY:  Verify trywrlock returns NIMCP_BUSY (doesn't block)
 */
TEST_F(RWLockExtendedTest, TryWrLock_WhenWriteLocked) {
    /* WHAT: Acquire write lock first
     * WHY:  Block second write lock acquisition
     */
    ASSERT_EQ(nimcp_rwlock_wrlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Try to acquire write lock again (should fail immediately)
     * WHY:  Writer already holds lock, trywrlock should return BUSY
     */
    EXPECT_EQ(nimcp_rwlock_trywrlock(&lock), NIMCP_BUSY);

    /* WHAT: Release write lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

//=============================================================================
// RW Lock Timed Operations
//=============================================================================

/**
 * @brief Test timedrdlock with immediate success
 *
 * WHAT: Acquire read lock with timeout when available
 * WHY:  Verify timedrdlock succeeds immediately when no writer
 */
TEST_F(RWLockExtendedTest, TimedRdLock_Success) {
    /* WHAT: Try to acquire read lock with 1 second timeout
     * WHY:  Should succeed immediately (no writer active)
     */
    EXPECT_EQ(nimcp_rwlock_timedrdlock(&lock, 1000), NIMCP_SUCCESS);

    /* WHAT: Release the lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test timedrdlock timeout when writer holds lock
 *
 * WHAT: Acquire read lock with timeout when writer holds lock
 * WHY:  Verify timedrdlock times out correctly
 */
TEST_F(RWLockExtendedTest, TimedRdLock_Timeout) {
    /* WHAT: Acquire write lock to block read lock
     * WHY:  Create contention scenario
     */
    ASSERT_EQ(nimcp_rwlock_wrlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Launch thread to try timed read lock
     * WHY:  Test timeout behavior in separate thread
     */
    std::atomic<nimcp_result_t> result{NIMCP_SUCCESS};
    std::thread reader([this, &result]() {
        /* WHAT: Try to acquire read lock with short timeout (100ms)
         * WHY:  Should timeout since write lock is held
         */
        result = nimcp_rwlock_timedrdlock(&lock, 100);
    });

    /* WHAT: Wait for reader thread to complete
     * WHY:  Ensure timeout occurs
     */
    reader.join();

    /* WHAT: Verify timeout occurred
     * WHY:  Result should be NIMCP_BUSY (timeout)
     */
    EXPECT_EQ(result.load(), NIMCP_BUSY);

    /* WHAT: Release write lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test timedwrlock with immediate success
 *
 * WHAT: Acquire write lock with timeout when available
 * WHY:  Verify timedwrlock succeeds immediately when no lock held
 */
TEST_F(RWLockExtendedTest, TimedWrLock_Success) {
    /* WHAT: Try to acquire write lock with 1 second timeout
     * WHY:  Should succeed immediately (no lock active)
     */
    EXPECT_EQ(nimcp_rwlock_timedwrlock(&lock, 1000), NIMCP_SUCCESS);

    /* WHAT: Release the lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test timedwrlock timeout when reader holds lock
 *
 * WHAT: Acquire write lock with timeout when reader holds lock
 * WHY:  Verify timedwrlock times out correctly
 */
TEST_F(RWLockExtendedTest, TimedWrLock_Timeout) {
    /* WHAT: Acquire read lock to block write lock
     * WHY:  Create contention scenario
     */
    ASSERT_EQ(nimcp_rwlock_rdlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Launch thread to try timed write lock
     * WHY:  Test timeout behavior in separate thread
     */
    std::atomic<nimcp_result_t> result{NIMCP_SUCCESS};
    std::thread writer([this, &result]() {
        /* WHAT: Try to acquire write lock with short timeout (100ms)
         * WHY:  Should timeout since read lock is held
         */
        result = nimcp_rwlock_timedwrlock(&lock, 100);
    });

    /* WHAT: Wait for writer thread to complete
     * WHY:  Ensure timeout occurs
     */
    writer.join();

    /* WHAT: Verify timeout occurred
     * WHY:  Result should be NIMCP_BUSY (timeout)
     */
    EXPECT_EQ(result.load(), NIMCP_BUSY);

    /* WHAT: Release read lock
     * WHY:  Clean up
     */
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);
}

/**
 * @brief Test timedrdlock succeeds after writer releases
 *
 * WHAT: Timed read lock acquisition succeeds when writer releases before timeout
 * WHY:  Verify timedrdlock waits and succeeds when lock becomes available
 */
TEST_F(RWLockExtendedTest, TimedRdLock_SuccessAfterRelease) {
    /* WHAT: Acquire write lock
     * WHY:  Create initial contention
     */
    ASSERT_EQ(nimcp_rwlock_wrlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Launch thread to acquire read lock with timeout
     * WHY:  Test that lock is acquired after writer releases
     */
    std::atomic<nimcp_result_t> result{NIMCP_ERROR_SYSTEM};
    std::thread reader([this, &result]() {
        /* WHAT: Try to acquire read lock with 2 second timeout
         * WHY:  Long enough for writer to release (500ms delay)
         */
        result = nimcp_rwlock_timedrdlock(&lock, 2000);
    });

    /* WHAT: Hold lock briefly then release
     * WHY:  Allow reader to acquire after delay
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_EQ(nimcp_rwlock_unlock(&lock), NIMCP_SUCCESS);

    /* WHAT: Wait for reader to complete
     * WHY:  Ensure reader acquired lock
     */
    reader.join();

    /* WHAT: Verify reader succeeded
     * WHY:  Lock was released before timeout
     */
    EXPECT_EQ(result.load(), NIMCP_SUCCESS);
}

//=============================================================================
// Thread Naming Tests
//=============================================================================

/**
 * @brief Test setting thread name
 *
 * WHAT: Set name of current thread
 * WHY:  Verify thread naming API works
 */
TEST_F(ThreadNamingTest, SetThreadName) {
    /* WHAT: Set thread name to "test_thread"
     * WHY:  Test basic naming functionality
     */
    EXPECT_EQ(nimcp_thread_set_name("test_thread"), NIMCP_SUCCESS);
}

/**
 * @brief Test getting thread name after setting
 *
 * WHAT: Set thread name, then retrieve it
 * WHY:  Verify set/get name round-trip works
 */
TEST_F(ThreadNamingTest, GetThreadName) {
    /* WHAT: Set thread name
     * WHY:  Establish known name for retrieval
     */
    const char* test_name = "get_test";
    ASSERT_EQ(nimcp_thread_set_name(test_name), NIMCP_SUCCESS);

    /* WHAT: Retrieve thread name
     * WHY:  Verify name was set correctly
     */
    char retrieved_name[NIMCP_THREAD_NAME_MAX];
    ASSERT_EQ(nimcp_thread_get_name(retrieved_name, sizeof(retrieved_name)), NIMCP_SUCCESS);

    /* WHAT: Compare names
     * WHY:  Names should match
     */
    EXPECT_STREQ(retrieved_name, test_name);
}

/**
 * @brief Test long thread name truncation
 *
 * WHAT: Set thread name longer than limit
 * WHY:  Verify name is truncated to 15 chars + null
 */
TEST_F(ThreadNamingTest, LongThreadName) {
    /* WHAT: Set very long thread name
     * WHY:  Test truncation behavior
     */
    const char* long_name = "this_is_a_very_long_thread_name_that_exceeds_limit";
    EXPECT_EQ(nimcp_thread_set_name(long_name), NIMCP_SUCCESS);

    /* WHAT: Retrieve name
     * WHY:  Verify it was truncated
     */
    char retrieved_name[NIMCP_THREAD_NAME_MAX];
    ASSERT_EQ(nimcp_thread_get_name(retrieved_name, sizeof(retrieved_name)), NIMCP_SUCCESS);

    /* WHAT: Check name length
     * WHY:  Should be at most 15 characters (plus null)
     */
    EXPECT_LE(strlen(retrieved_name), NIMCP_THREAD_NAME_MAX - 1);

    /* WHAT: Verify truncation matches prefix
     * WHY:  First 15 chars should match
     */
    EXPECT_EQ(strncmp(retrieved_name, long_name, NIMCP_THREAD_NAME_MAX - 1), 0);
}

/**
 * @brief Test thread naming in spawned thread
 *
 * WHAT: Set and get thread name in different thread
 * WHY:  Verify naming works correctly in spawned threads
 */
TEST_F(ThreadNamingTest, SpawnedThreadName) {
    /* WHAT: Launch thread with specific name
     * WHY:  Test naming in spawned thread context
     */
    std::atomic<bool> success{false};
    std::thread worker([&success]() {
        /* WHAT: Set thread name in worker
         * WHY:  Each thread can have its own name
         */
        if (nimcp_thread_set_name("worker_1") != NIMCP_SUCCESS) {
            return;
        }

        /* WHAT: Retrieve and verify name
         * WHY:  Ensure round-trip works in spawned thread
         */
        char name[NIMCP_THREAD_NAME_MAX];
        if (nimcp_thread_get_name(name, sizeof(name)) != NIMCP_SUCCESS) {
            return;
        }

        /* WHAT: Check name matches
         * WHY:  Verify naming worked correctly
         */
        success = (strcmp(name, "worker_1") == 0);
    });

    worker.join();
    EXPECT_TRUE(success.load());
}

//=============================================================================
// CPU Affinity Tests
//=============================================================================

#ifdef __linux__
/**
 * @brief Test setting CPU affinity
 *
 * WHAT: Bind thread to specific CPU core
 * WHY:  Verify CPU affinity API works (Linux only)
 */
TEST_F(ThreadNamingTest, SetCpuAffinity) {
    /* WHAT: Get current thread handle
     * WHY:  Need thread handle for affinity operations
     */
    nimcp_thread_t current = nimcp_thread_self();

    /* WHAT: Set affinity to CPU 0
     * WHY:  Test basic affinity functionality
     */
    EXPECT_EQ(nimcp_thread_set_affinity(current, 0), NIMCP_SUCCESS);
}

/**
 * @brief Test getting CPU affinity after setting
 *
 * WHAT: Set CPU affinity, then retrieve it
 * WHY:  Verify set/get affinity round-trip works
 */
TEST_F(ThreadNamingTest, GetCpuAffinity) {
    /* WHAT: Get current thread handle
     * WHY:  Need thread handle for affinity operations
     */
    nimcp_thread_t current = nimcp_thread_self();

    /* WHAT: Set affinity to CPU 1
     * WHY:  Establish known affinity for retrieval
     */
    ASSERT_EQ(nimcp_thread_set_affinity(current, 1), NIMCP_SUCCESS);

    /* WHAT: Retrieve CPU affinity
     * WHY:  Verify affinity was set correctly
     */
    uint32_t cpu_id = 999;  // Invalid initial value
    ASSERT_EQ(nimcp_thread_get_affinity(current, &cpu_id), NIMCP_SUCCESS);

    /* WHAT: Verify CPU ID matches
     * WHY:  Should be CPU 1
     */
    EXPECT_EQ(cpu_id, 1u);
}

/**
 * @brief Test CPU affinity in spawned thread
 *
 * WHAT: Set CPU affinity for spawned thread
 * WHY:  Verify affinity works for worker threads
 */
TEST_F(ThreadNamingTest, SpawnedThreadAffinity) {
    /* WHAT: Launch thread and set its affinity
     * WHY:  Test affinity setting for worker threads
     */
    std::atomic<bool> success{false};
    nimcp_thread_t thread_handle;

    auto worker_func = +[](void* arg) -> void* {
        auto* success_ptr = static_cast<std::atomic<bool>*>(arg);

        /* WHAT: Get own thread handle
         * WHY:  Need handle for affinity operations
         */
        nimcp_thread_t self = nimcp_thread_self();

        /* WHAT: Set affinity to CPU 0
         * WHY:  Pin worker to specific core
         */
        if (nimcp_thread_set_affinity(self, 0) != NIMCP_SUCCESS) {
            return nullptr;
        }

        /* WHAT: Verify affinity was set
         * WHY:  Ensure round-trip works
         */
        uint32_t cpu_id;
        if (nimcp_thread_get_affinity(self, &cpu_id) != NIMCP_SUCCESS) {
            return nullptr;
        }

        /* WHAT: Check CPU ID is correct
         * WHY:  Should be CPU 0
         */
        *success_ptr = (cpu_id == 0);
        return nullptr;
    };

    ASSERT_EQ(nimcp_thread_create(&thread_handle, worker_func, &success, nullptr), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_thread_join(thread_handle, nullptr), NIMCP_SUCCESS);

    EXPECT_TRUE(success.load());
}
#else
/**
 * @brief Test CPU affinity graceful degradation on non-Linux
 *
 * WHAT: Verify affinity operations succeed but are no-ops on non-Linux
 * WHY:  Ensure portable code works across platforms
 */
TEST_F(ThreadNamingTest, CpuAffinityGracefulDegradation) {
    /* WHAT: Get current thread handle
     * WHY:  Need thread handle for affinity operations
     */
    nimcp_thread_t current = nimcp_thread_self();

    /* WHAT: Set affinity (should succeed but be no-op)
     * WHY:  Graceful degradation on unsupported platforms
     */
    EXPECT_EQ(nimcp_thread_set_affinity(current, 0), NIMCP_SUCCESS);

    /* WHAT: Get affinity (should return 0 as default)
     * WHY:  Provide consistent API across platforms
     */
    uint32_t cpu_id = 999;
    EXPECT_EQ(nimcp_thread_get_affinity(current, &cpu_id), NIMCP_SUCCESS);
    EXPECT_EQ(cpu_id, 0u);  // Default value on non-Linux
}
#endif

//=============================================================================
// Error Handling Tests
//=============================================================================

/**
 * @brief Test NULL parameter handling for tryrdlock
 *
 * WHAT: Call tryrdlock with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(RWLockErrorTest, TryRdLock_NullPointer) {
    EXPECT_EQ(nimcp_rwlock_tryrdlock(nullptr), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for trywrlock
 *
 * WHAT: Call trywrlock with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(RWLockErrorTest, TryWrLock_NullPointer) {
    EXPECT_EQ(nimcp_rwlock_trywrlock(nullptr), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for timedrdlock
 *
 * WHAT: Call timedrdlock with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(RWLockErrorTest, TimedRdLock_NullPointer) {
    EXPECT_EQ(nimcp_rwlock_timedrdlock(nullptr, 1000), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for timedwrlock
 *
 * WHAT: Call timedwrlock with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(RWLockErrorTest, TimedWrLock_NullPointer) {
    EXPECT_EQ(nimcp_rwlock_timedwrlock(nullptr, 1000), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for set_name
 *
 * WHAT: Call set_name with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(ThreadNamingErrorTest, SetName_NullPointer) {
    EXPECT_EQ(nimcp_thread_set_name(nullptr), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for get_name
 *
 * WHAT: Call get_name with NULL pointer
 * WHY:  Verify proper error handling
 */
TEST(ThreadNamingErrorTest, GetName_NullPointer) {
    EXPECT_EQ(nimcp_thread_get_name(nullptr, 16), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test insufficient buffer size for get_name
 *
 * WHAT: Call get_name with too small buffer
 * WHY:  Verify proper error handling
 */
TEST(ThreadNamingErrorTest, GetName_InsufficientBuffer) {
    char small_buffer[8];
    EXPECT_EQ(nimcp_thread_get_name(small_buffer, sizeof(small_buffer)), NIMCP_ERROR_INVALID_PARAM);
}

/**
 * @brief Test NULL parameter handling for get_affinity
 *
 * WHAT: Call get_affinity with NULL cpu_id pointer
 * WHY:  Verify proper error handling
 */
TEST(ThreadAffinityErrorTest, GetAffinity_NullPointer) {
    nimcp_thread_t current = nimcp_thread_self();
    EXPECT_EQ(nimcp_thread_get_affinity(current, nullptr), NIMCP_ERROR_INVALID_PARAM);
}
