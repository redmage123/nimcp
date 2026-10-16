/**
 * @file test_mutex_safety.cpp
 * @brief Tests for pthread_mutex to nimcp_mutex migration (P1-1)
 *
 * WHAT: Verify nimcp_mutex API works correctly after migration
 * WHY:  P1-1 migrated raw pthread_mutex calls to nimcp_mutex wrapper layer
 * HOW:  Test creation, locking, unlocking, trylock, recursive, and destruction
 *
 * Function signatures tested (from include/utils/thread/nimcp_thread.h):
 *   nimcp_result_t nimcp_mutex_init(nimcp_mutex_t* mutex, const mutex_attr_t* attr);
 *   nimcp_mutex_t* nimcp_mutex_create(const mutex_attr_t* attr);
 *   nimcp_result_t nimcp_mutex_destroy(nimcp_mutex_t* mutex);
 *   nimcp_result_t nimcp_mutex_free(nimcp_mutex_t* mutex);
 *   nimcp_result_t nimcp_mutex_lock(nimcp_mutex_t* mutex);
 *   nimcp_result_t nimcp_mutex_trylock(nimcp_mutex_t* mutex);
 *   nimcp_result_t nimcp_mutex_unlock(nimcp_mutex_t* mutex);
 *   nimcp_result_t nimcp_thread_init(void);
 *   void nimcp_thread_cleanup(void);
 *
 * Constants/Types:
 *   NIMCP_SUCCESS (0)
 *   NIMCP_BUSY (-5)
 *   mutex_type_t: MUTEX_TYPE_NORMAL, MUTEX_TYPE_RECURSIVE, MUTEX_TYPE_ERRORCHECK
 *   mutex_attr_t: { mutex_type_t type; }
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/thread/nimcp_thread.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MutexSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_thread_init();
    }

    void TearDown() override {
        nimcp_thread_cleanup();
    }
};

/* ============================================================================
 * nimcp_mutex_create / nimcp_mutex_free Lifecycle
 * ============================================================================ */

TEST_F(MutexSafetyTest, CreateDefaultMutex) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(nullptr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_free(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST_F(MutexSafetyTest, CreateNormalMutex) {
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;

    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_free(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST_F(MutexSafetyTest, CreateRecursiveMutex) {
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_RECURSIVE;

    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_free(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST_F(MutexSafetyTest, CreateErrorCheckMutex) {
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_ERRORCHECK;

    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_free(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

/* ============================================================================
 * nimcp_mutex_init / nimcp_mutex_destroy (embedded mutex)
 * ============================================================================ */

TEST_F(MutexSafetyTest, InitDestroyEmbeddedMutex) {
    nimcp_mutex_t mutex;

    nimcp_result_t rc = nimcp_mutex_init(&mutex, nullptr);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    rc = nimcp_mutex_destroy(&mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

TEST_F(MutexSafetyTest, InitWithAttributes) {
    nimcp_mutex_t mutex;
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_RECURSIVE;

    nimcp_result_t rc = nimcp_mutex_init(&mutex, &attr);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    rc = nimcp_mutex_destroy(&mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);
}

/* ============================================================================
 * Lock / Unlock Basic Operation
 * ============================================================================ */

TEST_F(MutexSafetyTest, LockUnlockBasic) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(nullptr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_lock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    rc = nimcp_mutex_unlock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    nimcp_mutex_free(mutex);
}

TEST_F(MutexSafetyTest, MultipleLockUnlockCycles) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(nullptr);
    ASSERT_NE(mutex, nullptr);

    for (int i = 0; i < 10; i++) {
        nimcp_result_t rc = nimcp_mutex_lock(mutex);
        EXPECT_EQ(rc, NIMCP_SUCCESS) << "Lock failed on iteration " << i;

        rc = nimcp_mutex_unlock(mutex);
        EXPECT_EQ(rc, NIMCP_SUCCESS) << "Unlock failed on iteration " << i;
    }

    nimcp_mutex_free(mutex);
}

/* ============================================================================
 * nimcp_mutex_trylock
 * ============================================================================ */

TEST_F(MutexSafetyTest, TrylockSucceedsWhenUnlocked) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(nullptr);
    ASSERT_NE(mutex, nullptr);

    nimcp_result_t rc = nimcp_mutex_trylock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    nimcp_mutex_unlock(mutex);
    nimcp_mutex_free(mutex);
}

TEST_F(MutexSafetyTest, TrylockReturnsBusyWhenLocked) {
    nimcp_mutex_t* mutex = nimcp_mutex_create(nullptr);
    ASSERT_NE(mutex, nullptr);

    // Lock the mutex first
    nimcp_result_t rc = nimcp_mutex_lock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Trylock should return NIMCP_BUSY (normal mutex, same thread)
    rc = nimcp_mutex_trylock(mutex);
    EXPECT_EQ(rc, NIMCP_BUSY);

    nimcp_mutex_unlock(mutex);
    nimcp_mutex_free(mutex);
}

/* ============================================================================
 * Recursive Mutex Tests
 * ============================================================================ */

TEST_F(MutexSafetyTest, RecursiveMutexAllowsDoubleLock) {
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_RECURSIVE;

    nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
    ASSERT_NE(mutex, nullptr);

    // First lock
    nimcp_result_t rc = nimcp_mutex_lock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Second lock from same thread (recursive) should succeed
    rc = nimcp_mutex_lock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Trylock should also succeed for recursive mutex
    rc = nimcp_mutex_trylock(mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    // Unlock three times (one for each lock)
    nimcp_mutex_unlock(mutex);
    nimcp_mutex_unlock(mutex);
    nimcp_mutex_unlock(mutex);

    nimcp_mutex_free(mutex);
}

/* ============================================================================
 * Mutex Attribute Creation Tests
 * ============================================================================ */

TEST_F(MutexSafetyTest, MutexAttributeValues) {
    // Verify enum values are distinct
    EXPECT_NE(MUTEX_TYPE_NORMAL, MUTEX_TYPE_RECURSIVE);
    EXPECT_NE(MUTEX_TYPE_NORMAL, MUTEX_TYPE_ERRORCHECK);
    EXPECT_NE(MUTEX_TYPE_RECURSIVE, MUTEX_TYPE_ERRORCHECK);
}

TEST_F(MutexSafetyTest, AllMutexTypesCreateSuccessfully) {
    mutex_type_t types[] = {
        MUTEX_TYPE_NORMAL,
        MUTEX_TYPE_RECURSIVE,
        MUTEX_TYPE_ERRORCHECK
    };

    for (int i = 0; i < 3; i++) {
        mutex_attr_t attr;
        attr.type = types[i];

        nimcp_mutex_t* mutex = nimcp_mutex_create(&attr);
        ASSERT_NE(mutex, nullptr) << "Failed to create mutex type " << i;

        // Basic lock/unlock should work for all types
        nimcp_result_t rc = nimcp_mutex_lock(mutex);
        EXPECT_EQ(rc, NIMCP_SUCCESS);

        rc = nimcp_mutex_unlock(mutex);
        EXPECT_EQ(rc, NIMCP_SUCCESS);

        nimcp_mutex_free(mutex);
    }
}

/* ============================================================================
 * Embedded Mutex Lock/Unlock
 * ============================================================================ */

TEST_F(MutexSafetyTest, EmbeddedMutexLockUnlock) {
    nimcp_mutex_t mutex;
    nimcp_result_t rc = nimcp_mutex_init(&mutex, nullptr);
    ASSERT_EQ(rc, NIMCP_SUCCESS);

    rc = nimcp_mutex_lock(&mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    rc = nimcp_mutex_unlock(&mutex);
    EXPECT_EQ(rc, NIMCP_SUCCESS);

    nimcp_mutex_destroy(&mutex);
}
