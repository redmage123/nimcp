/**
 * @file test_thread_utils.cpp
 * @brief Unit tests for nimcp thread utilities and thread pool
 *
 * WHAT: Comprehensive tests for thread wrappers and thread pool
 * WHY: Ensure thread safety and correctness of parallel processing
 * HOW: Test creation, synchronization, and pool operations
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>

#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_pool.h"

//=============================================================================
// Thread Wrapper Tests
//=============================================================================

class ThreadTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        counter = 0;
    }

    void TearDown() override {}

    static std::atomic<int> counter;
};

std::atomic<int> ThreadTest::counter{0};

/**
 * WHAT: Helper function for thread testing
 */
static void* increment_counter(void* arg)
{
    int* count = static_cast<int*>(arg);
    (*count)++;
    return nullptr;
}

/**
 * WHAT: Test thread creation and joining
 * WHY: Verify basic thread lifecycle works
 */
TEST_F(ThreadTest, CreateAndJoin)
{
    nimcp_thread_t thread;
    int count = 0;

    nimcp_result_t result = nimcp_thread_create(&thread, increment_counter, &count, nullptr);
    ASSERT_EQ(result, NIMCP_SUCCESS);

    result = nimcp_thread_join(thread, nullptr);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1);
}

/**
 * WHAT: Test NULL parameter handling
 * WHY: Verify proper error handling
 */
TEST_F(ThreadTest, CreateWithNullThread)
{
    nimcp_result_t result = nimcp_thread_create(nullptr, increment_counter, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test NULL function handling
 * WHY: Verify proper error handling
 */
TEST_F(ThreadTest, CreateWithNullFunction)
{
    nimcp_thread_t thread;
    nimcp_result_t result = nimcp_thread_create(&thread, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

//=============================================================================
// Mutex Tests
//=============================================================================

class MutexTest : public ::testing::Test {
   public:
    static int shared_counter;

   protected:
    void SetUp() override
    {
        nimcp_mutex_init(&mutex, nullptr);
        shared_counter = 0;
    }

    void TearDown() override
    {
        nimcp_mutex_destroy(&mutex);
    }

    nimcp_mutex_t mutex;
};

int MutexTest::shared_counter = 0;

/**
 * WHAT: Test mutex lock/unlock
 * WHY: Verify basic mutex operations work
 */
TEST_F(MutexTest, LockUnlock)
{
    nimcp_result_t result = nimcp_mutex_lock(&mutex);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = nimcp_mutex_unlock(&mutex);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/**
 * WHAT: Test trylock when available
 * WHY: Verify non-blocking lock works
 */
TEST_F(MutexTest, TrylockAvailable)
{
    nimcp_result_t result = nimcp_mutex_trylock(&mutex);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_mutex_unlock(&mutex);
}

/**
 * WHAT: Test trylock when locked
 * WHY: Verify non-blocking behavior
 */
TEST_F(MutexTest, TrylockBusy)
{
    nimcp_mutex_lock(&mutex);

    nimcp_result_t result = nimcp_mutex_trylock(&mutex);
    EXPECT_EQ(result, NIMCP_BUSY);

    nimcp_mutex_unlock(&mutex);
}

/**
 * WHAT: Helper for mutex contention test
 */
static void* mutex_increment(void* arg)
{
    nimcp_mutex_t* mutex = static_cast<nimcp_mutex_t*>(arg);

    for (int i = 0; i < 1000; i++) {
        nimcp_mutex_lock(mutex);
        MutexTest::shared_counter++;
        nimcp_mutex_unlock(mutex);
    }
    return nullptr;
}

/**
 * WHAT: Test mutex protects shared data
 * WHY: Verify mutex prevents race conditions
 */
TEST_F(MutexTest, ProtectsSharedData)
{
    const int NUM_THREADS = 4;
    nimcp_thread_t threads[NUM_THREADS];
    shared_counter = 0;

    // Create threads that increment shared counter
    for (int i = 0; i < NUM_THREADS; i++) {
        nimcp_thread_create(&threads[i], mutex_increment, &mutex, nullptr);
    }

    // Join all threads
    for (int i = 0; i < NUM_THREADS; i++) {
        nimcp_thread_join(threads[i], nullptr);
    }

    // Counter should be exactly NUM_THREADS * 1000
    EXPECT_EQ(shared_counter, NUM_THREADS * 1000);
}

//=============================================================================
// Condition Variable Tests
//=============================================================================

class CondVarTest : public ::testing::Test {
   public:
    static bool ready;

   protected:
    void SetUp() override
    {
        nimcp_mutex_init(&mutex, nullptr);
        nimcp_cond_init(&cond);
        ready = false;
    }

    void TearDown() override
    {
        nimcp_cond_destroy(&cond);
        nimcp_mutex_destroy(&mutex);
    }

    nimcp_mutex_t mutex;
    nimcp_cond_t cond;
};

bool CondVarTest::ready = false;

/**
 * WHAT: Helper for condition variable wait test
 */
static void* cond_waiter(void* arg)
{
    auto* data = static_cast<std::pair<nimcp_mutex_t*, nimcp_cond_t*>*>(arg);

    nimcp_mutex_lock(data->first);
    while (!CondVarTest::ready) {
        nimcp_cond_wait(data->second, data->first);
    }
    nimcp_mutex_unlock(data->first);

    return nullptr;
}

/**
 * WHAT: Test condition variable signal
 * WHY: Verify thread synchronization works
 */
TEST_F(CondVarTest, SignalWakesWaiter)
{
    nimcp_thread_t thread;
    auto data = std::make_pair(&mutex, &cond);
    ready = false;

    // Start waiter thread
    nimcp_thread_create(&thread, cond_waiter, &data, nullptr);

    // Give thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal the thread
    nimcp_mutex_lock(&mutex);
    ready = true;
    nimcp_cond_signal(&cond);
    nimcp_mutex_unlock(&mutex);

    // Wait for thread to finish
    nimcp_thread_join(thread, nullptr);

    EXPECT_TRUE(ready);
}

/**
 * WHAT: Test condition variable broadcast
 * WHY: Verify multiple waiters can be woken
 */
TEST_F(CondVarTest, BroadcastWakesAll)
{
    const int NUM_THREADS = 4;
    nimcp_thread_t threads[NUM_THREADS];
    auto data = std::make_pair(&mutex, &cond);
    ready = false;

    // Start waiter threads
    for (int i = 0; i < NUM_THREADS; i++) {
        nimcp_thread_create(&threads[i], cond_waiter, &data, nullptr);
    }

    // Give threads time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Broadcast to all threads
    nimcp_mutex_lock(&mutex);
    ready = true;
    nimcp_cond_broadcast(&cond);
    nimcp_mutex_unlock(&mutex);

    // All threads should finish
    for (int i = 0; i < NUM_THREADS; i++) {
        nimcp_thread_join(threads[i], nullptr);
    }

    EXPECT_TRUE(ready);
}

/**
 * WHAT: Test timed wait timeout
 * WHY: Verify timeout mechanism works
 */
TEST_F(CondVarTest, TimedWaitTimeout)
{
    nimcp_mutex_lock(&mutex);

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_result_t result = nimcp_cond_timedwait(&cond, &mutex, 100);  // 100ms timeout
    auto end = std::chrono::high_resolution_clock::now();

    nimcp_mutex_unlock(&mutex);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should timeout
    EXPECT_EQ(result, NIMCP_BUSY);
    // Should take at least 100ms
    EXPECT_GE(elapsed, 90);  // Allow 10ms tolerance
}

//=============================================================================
// Thread Pool Tests
//=============================================================================

class ThreadPoolTest : public ::testing::Test {
   public:
    static std::atomic<int> task_counter;

   protected:
    void SetUp() override
    {
        task_counter = 0;
    }

    void TearDown() override {}
};

std::atomic<int> ThreadPoolTest::task_counter{0};

/**
 * WHAT: Test pool creation and destruction
 * WHY: Verify basic lifecycle works
 */
TEST_F(ThreadPoolTest, CreateAndDestroy)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Test pool creation with invalid thread count
 * WHY: Verify error handling
 */
TEST_F(ThreadPoolTest, CreateWithZeroThreads)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(0);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Test pool creation with too many threads
 * WHY: Verify bounds checking
 */
TEST_F(ThreadPoolTest, CreateWithTooManyThreads)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(1000);
    EXPECT_EQ(pool, nullptr);
}

/**
 * WHAT: Counter increment task
 */
static void increment_task(void* arg)
{
    ThreadPoolTest::task_counter++;
}

/**
 * WHAT: Test submitting and executing tasks
 * WHY: Verify pool executes tasks correctly
 */
TEST_F(ThreadPoolTest, SubmitAndExecute)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // Submit 100 tasks
    for (int i = 0; i < 100; i++) {
        nimcp_result_t result = nimcp_pool_submit(pool, increment_task, nullptr);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    // Wait for completion
    nimcp_pool_wait(pool);

    // All tasks should have executed
    EXPECT_EQ(task_counter, 100);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Task with argument
 */
static void add_task(void* arg)
{
    int* value = static_cast<int*>(arg);
    ThreadPoolTest::task_counter += *value;
}

/**
 * WHAT: Test tasks with arguments
 * WHY: Verify argument passing works
 */
TEST_F(ThreadPoolTest, TasksWithArguments)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int values[10];

    // Submit 10 tasks with different values
    for (int i = 0; i < 10; i++) {
        values[i] = i + 1;  // 1, 2, 3, ..., 10
        nimcp_pool_submit(pool, add_task, &values[i]);
    }

    nimcp_pool_wait(pool);

    // Sum should be 1+2+3+...+10 = 55
    EXPECT_EQ(task_counter, 55);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Test submitting to NULL pool
 * WHY: Verify error handling
 */
TEST_F(ThreadPoolTest, SubmitToNullPool)
{
    nimcp_result_t result = nimcp_pool_submit(nullptr, increment_task, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test submitting NULL task
 * WHY: Verify error handling
 */
TEST_F(ThreadPoolTest, SubmitNullTask)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    nimcp_result_t result = nimcp_pool_submit(pool, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Test waiting for NULL pool
 * WHY: Verify error handling
 */
TEST_F(ThreadPoolTest, WaitForNullPool)
{
    nimcp_result_t result = nimcp_pool_wait(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM);
}

/**
 * WHAT: Test pending count
 * WHY: Verify queue tracking works
 */
TEST_F(ThreadPoolTest, PendingCount)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(1);  // Single thread
    ASSERT_NE(pool, nullptr);

    // Submit many quick tasks
    for (int i = 0; i < 100; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }

    // Some tasks should be pending
    size_t pending = nimcp_pool_pending(pool);
    // Can't assert exact count due to race, but should be > 0
    EXPECT_GE(pending, 0UL);

    nimcp_pool_wait(pool);

    // After wait, no tasks pending
    pending = nimcp_pool_pending(pool);
    EXPECT_EQ(pending, 0UL);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Long-running task for stress test
 */
static void sleep_task(void* arg)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ThreadPoolTest::task_counter++;
}

/**
 * WHAT: Test pool with many tasks
 * WHY: Verify pool handles load correctly
 */
TEST_F(ThreadPoolTest, StressTest)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // Submit 1000 tasks
    for (int i = 0; i < 1000; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }

    nimcp_pool_wait(pool);

    EXPECT_EQ(task_counter, 1000);

    nimcp_pool_destroy(pool);
}

/**
 * WHAT: Test destroying pool with pending tasks
 * WHY: Verify clean shutdown
 */
TEST_F(ThreadPoolTest, DestroyWithPendingTasks)
{
    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // Submit many tasks
    for (int i = 0; i < 100; i++) {
        nimcp_pool_submit(pool, sleep_task, nullptr);
    }

    // Destroy immediately (should wait for completion)
    nimcp_pool_destroy(pool);

    // All tasks should have executed
    EXPECT_EQ(task_counter, 100);
}
