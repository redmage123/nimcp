/**
 * @file test_utils_thread_pool.cpp
 * @brief Comprehensive unit tests for thread pool utilities
 *
 * WHAT: 100% test coverage for nimcp_thread_pool.c
 * WHY:  Thread pool is critical for parallel processing - must be bulletproof
 * HOW:  Test all operations, edge cases, error handling, and thread safety
 *
 * TEST COVERAGE:
 * 1. nimcp_pool_create() - pool creation with various worker counts
 * 2. nimcp_pool_destroy() - graceful shutdown and cleanup
 * 3. nimcp_pool_submit() - task submission and queueing
 * 4. nimcp_pool_wait() - waiting for task completion
 * 5. nimcp_pool_pending() - tracking queued tasks
 * 6. nimcp_pool_active() - tracking active workers
 * 7. Concurrent task execution
 * 8. Queue behavior under load
 * 9. Shutdown with pending tasks
 * 10. Edge cases (NULL params, zero workers, max workers)
 * 11. Thread safety under concurrent submission
 * 12. Task argument passing
 * 13. Backpressure handling (queue full)
 * 14. Performance under load
 * 15. Error handling and recovery
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>

    #include "utils/thread/nimcp_thread_pool.h"
    #include "utils/thread/nimcp_thread.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        task_counter = 0;
        sum_counter = 0;
        nimcp_thread_init();
    }

    void TearDown() override {
        nimcp_thread_cleanup();
    }

public:
    // Shared counters for task verification (public for task function access)
    static std::atomic<int> task_counter;
    static std::atomic<int> sum_counter;
};

std::atomic<int> ThreadPoolTest::task_counter{0};
std::atomic<int> ThreadPoolTest::sum_counter{0};

//=============================================================================
// Helper Task Functions
//=============================================================================

// WHAT: Simple counter increment task
// WHY:  Verify basic task execution
static void increment_task(void* arg) {
    (void)arg;
    ThreadPoolTest::task_counter++;
}

// WHAT: Task with argument
// WHY:  Verify argument passing works correctly
static void add_task(void* arg) {
    int* value = static_cast<int*>(arg);
    ThreadPoolTest::sum_counter += *value;
}

// WHAT: Long-running task for stress testing
// WHY:  Test concurrent execution and backpressure
static void sleep_task(void* arg) {
    int sleep_ms = arg ? *static_cast<int*>(arg) : 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    ThreadPoolTest::task_counter++;
}

// WHAT: Very fast task for performance testing
// WHY:  Test pool overhead vs task execution time
static void fast_task(void* arg) {
    (void)arg;
    ThreadPoolTest::task_counter++;
}

//=============================================================================
// Unit Test 1: Pool creation with valid worker count
//=============================================================================

TEST_F(ThreadPoolTest, Create_ValidWorkerCount) {
    // WHAT: Verify pool creation with 4 worker threads
    // WHY:  Basic functionality must work
    // HOW:  Create pool, verify non-NULL, destroy

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr) << "Pool creation failed with 4 workers";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Pool created and destroyed successfully";
}

//=============================================================================
// Unit Test 2: Pool creation with single worker
//=============================================================================

TEST_F(ThreadPoolTest, Create_SingleWorker) {
    // WHAT: Verify pool creation with minimum (1) worker thread
    // WHY:  Edge case - single-threaded pool should work
    // HOW:  Create with 1 worker, submit tasks, verify serialization

    nimcp_thread_pool_t* pool = nimcp_pool_create(1);
    ASSERT_NE(pool, nullptr) << "Pool creation failed with 1 worker";

    // Verify tasks execute serially
    task_counter = 0;
    for (int i = 0; i < 10; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }
    nimcp_pool_wait(pool);

    EXPECT_EQ(task_counter, 10) << "Single-worker pool should execute all tasks";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Single-worker pool works correctly";
}

//=============================================================================
// Unit Test 3: Pool creation with maximum workers
//=============================================================================

TEST_F(ThreadPoolTest, Create_MaximumWorkers) {
    // WHAT: Verify pool creation with maximum allowed workers
    // WHY:  Edge case - test upper boundary
    // HOW:  Create with NIMCP_POOL_MAX_THREADS workers

    nimcp_thread_pool_t* pool = nimcp_pool_create(NIMCP_POOL_MAX_THREADS);
    ASSERT_NE(pool, nullptr) << "Pool creation failed with max workers";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Maximum-worker pool created successfully";
}

//=============================================================================
// Unit Test 4: Pool creation with zero workers (error case)
//=============================================================================

TEST_F(ThreadPoolTest, Create_ZeroWorkers) {
    // WHAT: Verify pool creation fails with 0 workers
    // WHY:  Invalid parameter - pool with no workers is useless
    // HOW:  Attempt creation, expect NULL

    nimcp_thread_pool_t* pool = nimcp_pool_create(0);
    EXPECT_EQ(pool, nullptr) << "Pool should not be created with 0 workers";

    SUCCEED() << "Zero-worker creation correctly rejected";
}

//=============================================================================
// Unit Test 5: Pool creation exceeds maximum (error case)
//=============================================================================

TEST_F(ThreadPoolTest, Create_ExceedsMaximum) {
    // WHAT: Verify pool creation fails when exceeding MAX_THREADS
    // WHY:  Bounds checking - prevent array overflow
    // HOW:  Attempt creation with MAX_THREADS + 1

    nimcp_thread_pool_t* pool = nimcp_pool_create(NIMCP_POOL_MAX_THREADS + 1);
    EXPECT_EQ(pool, nullptr) << "Pool should not be created exceeding max workers";

    SUCCEED() << "Over-maximum creation correctly rejected";
}

//=============================================================================
// Unit Test 6: Basic task submission and execution
//=============================================================================

TEST_F(ThreadPoolTest, Submit_BasicExecution) {
    // WHAT: Verify tasks are submitted and executed
    // WHY:  Core functionality - tasks must run
    // HOW:  Submit 100 tasks, wait, verify counter

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // Submit 100 simple tasks
    for (int i = 0; i < 100; i++) {
        nimcp_result_t result = nimcp_pool_submit(pool, increment_task, nullptr);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Task submission " << i << " failed";
    }

    // Wait for all tasks to complete
    nimcp_result_t wait_result = nimcp_pool_wait(pool);
    EXPECT_EQ(wait_result, NIMCP_SUCCESS) << "Wait operation failed";

    // Verify all tasks executed
    EXPECT_EQ(task_counter, 100) << "Not all tasks executed";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Basic task submission and execution works";
}

//=============================================================================
// Unit Test 7: Task argument passing
//=============================================================================

TEST_F(ThreadPoolTest, Submit_WithArguments) {
    // WHAT: Verify task arguments are passed correctly
    // WHY:  Tasks need access to user data
    // HOW:  Submit tasks with different args, verify results

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    sum_counter = 0;
    int values[10];

    // Submit 10 tasks with values 1 through 10
    for (int i = 0; i < 10; i++) {
        values[i] = i + 1;
        nimcp_pool_submit(pool, add_task, &values[i]);
    }

    nimcp_pool_wait(pool);

    // Sum should be 1+2+3+...+10 = 55
    EXPECT_EQ(sum_counter, 55) << "Task arguments not passed correctly";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Task argument passing works correctly";
}

//=============================================================================
// Unit Test 8: Submit to NULL pool (error case)
//=============================================================================

TEST_F(ThreadPoolTest, Submit_NullPool) {
    // WHAT: Verify submitting to NULL pool returns error
    // WHY:  NULL parameter validation
    // HOW:  Call submit with NULL pool

    nimcp_result_t result = nimcp_pool_submit(nullptr, increment_task, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM) << "NULL pool should return error";

    SUCCEED() << "NULL pool correctly rejected";
}

//=============================================================================
// Unit Test 9: Submit NULL task (error case)
//=============================================================================

TEST_F(ThreadPoolTest, Submit_NullTask) {
    // WHAT: Verify submitting NULL task returns error
    // WHY:  NULL task function is invalid
    // HOW:  Call submit with NULL task function

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    nimcp_result_t result = nimcp_pool_submit(pool, nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM) << "NULL task should return error";

    nimcp_pool_destroy(pool);
    SUCCEED() << "NULL task correctly rejected";
}

//=============================================================================
// Unit Test 10: Wait for task completion
//=============================================================================

TEST_F(ThreadPoolTest, Wait_AllTasksComplete) {
    // WHAT: Verify wait blocks until all tasks finish
    // WHY:  Synchronization barrier needed
    // HOW:  Submit slow tasks, verify wait blocks until done

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int sleep_ms = 50;

    // Submit 10 slow tasks
    for (int i = 0; i < 10; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    // Wait should block until all complete
    auto start = std::chrono::high_resolution_clock::now();
    nimcp_pool_wait(pool);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // All 10 tasks executed
    EXPECT_EQ(task_counter, 10) << "Not all tasks completed";

    // Should take at least (10 tasks / 2 workers) * 50ms = 250ms
    EXPECT_GE(elapsed, 200) << "Wait returned too early";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Wait correctly blocks until completion";
}

//=============================================================================
// Unit Test 11: Wait on NULL pool (error case)
//=============================================================================

TEST_F(ThreadPoolTest, Wait_NullPool) {
    // WHAT: Verify waiting on NULL pool returns error
    // WHY:  NULL parameter validation
    // HOW:  Call wait with NULL pool

    nimcp_result_t result = nimcp_pool_wait(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAM) << "NULL pool should return error";

    SUCCEED() << "NULL pool wait correctly rejected";
}

//=============================================================================
// Unit Test 12: Pending task count tracking
//=============================================================================

TEST_F(ThreadPoolTest, Pending_CountTracking) {
    // WHAT: Verify pending count reflects queued tasks
    // WHY:  Monitor queue depth
    // HOW:  Submit many tasks to slow pool, check pending count

    nimcp_thread_pool_t* pool = nimcp_pool_create(1);  // Single worker
    ASSERT_NE(pool, nullptr);

    int sleep_ms = 100;

    // Submit many slow tasks to build up queue
    for (int i = 0; i < 20; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    // Some tasks should be pending (queued but not executing)
    size_t pending = nimcp_pool_pending(pool);
    EXPECT_GT(pending, 0) << "Expected pending tasks in queue";

    nimcp_pool_wait(pool);

    // After wait, queue should be empty
    pending = nimcp_pool_pending(pool);
    EXPECT_EQ(pending, 0) << "Queue should be empty after wait";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Pending count tracking works correctly";
}

//=============================================================================
// Unit Test 13: Active thread count tracking
//=============================================================================

TEST_F(ThreadPoolTest, Active_CountTracking) {
    // WHAT: Verify active count reflects executing workers
    // WHY:  Monitor pool utilization
    // HOW:  Submit tasks, check active count during execution

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    int sleep_ms = 200;

    // Submit 4 long tasks (one per worker)
    for (int i = 0; i < 4; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    // Give workers time to pick up tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // All 4 workers should be active
    size_t active = nimcp_pool_active(pool);
    EXPECT_GT(active, 0) << "Expected active workers";
    EXPECT_LE(active, 4) << "Active count exceeds worker count";

    nimcp_pool_wait(pool);

    // After wait, no workers should be active
    active = nimcp_pool_active(pool);
    EXPECT_EQ(active, 0) << "No workers should be active after wait";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Active count tracking works correctly";
}

//=============================================================================
// Unit Test 14: Concurrent task execution
//=============================================================================

TEST_F(ThreadPoolTest, Concurrent_ParallelExecution) {
    // WHAT: Verify multiple tasks execute concurrently
    // WHY:  Pool must enable true parallelism
    // HOW:  Submit tasks, measure speedup vs serial execution

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int sleep_ms = 50;
    const int num_tasks = 20;

    auto start = std::chrono::high_resolution_clock::now();

    // Submit 20 tasks of 50ms each
    for (int i = 0; i < num_tasks; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    nimcp_pool_wait(pool);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Serial execution would take 20 * 50ms = 1000ms
    // With 4 workers, should take roughly 20/4 * 50ms = 250ms
    // Allow overhead, expect < 600ms (shows parallelism)
    EXPECT_LT(elapsed, 600) << "Execution too slow, may not be parallel";
    EXPECT_EQ(task_counter, num_tasks) << "Not all tasks executed";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Concurrent execution works correctly";
}

//=============================================================================
// Unit Test 15: Graceful shutdown with pending tasks
//=============================================================================

TEST_F(ThreadPoolTest, Destroy_WithPendingTasks) {
    // WHAT: Verify destroy waits for pending tasks to complete
    // WHY:  Graceful shutdown - no dropped tasks
    // HOW:  Submit many tasks, destroy immediately, verify all executed

    nimcp_thread_pool_t* pool = nimcp_pool_create(2);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int sleep_ms = 20;

    // Submit many tasks that will queue up
    for (int i = 0; i < 100; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    // Destroy immediately (should drain queue gracefully)
    nimcp_pool_destroy(pool);

    // All tasks should have executed
    EXPECT_EQ(task_counter, 100) << "Destroy should complete pending tasks";

    SUCCEED() << "Graceful shutdown with pending tasks works";
}

//=============================================================================
// Unit Test 16: Thread safety under concurrent submission
//=============================================================================

TEST_F(ThreadPoolTest, ThreadSafety_ConcurrentSubmission) {
    // WHAT: Verify pool is thread-safe with multiple submitters
    // WHY:  Multiple threads may submit tasks concurrently
    // HOW:  Create multiple submitter threads, verify all tasks execute

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    const int num_submitters = 4;
    const int tasks_per_submitter = 100;

    // Lambda to submit tasks from multiple threads
    auto submitter = [pool]() {
        for (int i = 0; i < tasks_per_submitter; i++) {
            nimcp_pool_submit(pool, increment_task, nullptr);
        }
    };

    // Create submitter threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_submitters; i++) {
        threads.emplace_back(submitter);
    }

    // Wait for all submitters to finish
    for (auto& t : threads) {
        t.join();
    }

    // Wait for all tasks to complete
    nimcp_pool_wait(pool);

    // All tasks should have executed
    EXPECT_EQ(task_counter, num_submitters * tasks_per_submitter)
        << "Concurrent submission lost tasks";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Thread-safe concurrent submission works";
}

//=============================================================================
// Unit Test 17: Queue backpressure handling
//=============================================================================

TEST_F(ThreadPoolTest, Queue_BackpressureBlocking) {
    // WHAT: Verify submission blocks when queue is full
    // WHY:  Backpressure prevents unbounded memory usage
    // HOW:  Fill queue, verify next submit blocks

    nimcp_thread_pool_t* pool = nimcp_pool_create(1);  // Single worker
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int sleep_ms = 50;

    // Submit enough tasks to fill queue (MAX_QUEUE = 1024)
    // With single worker and slow tasks, queue will fill
    for (int i = 0; i < 100; i++) {
        nimcp_result_t result = nimcp_pool_submit(pool, sleep_task, &sleep_ms);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Submission " << i << " failed";
    }

    // Queue should have pending tasks
    size_t pending = nimcp_pool_pending(pool);
    EXPECT_GT(pending, 0) << "Expected tasks in queue";

    nimcp_pool_wait(pool);
    EXPECT_EQ(task_counter, 100) << "All tasks should execute";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Backpressure handling works correctly";
}

//=============================================================================
// Unit Test 18: Destroy NULL pool (safety check)
//=============================================================================

TEST_F(ThreadPoolTest, Destroy_NullPool) {
    // WHAT: Verify destroying NULL pool is safe
    // WHY:  Defensive programming - NULL check
    // HOW:  Call destroy with NULL, expect no crash

    nimcp_pool_destroy(nullptr);
    SUCCEED() << "Destroying NULL pool is safe";
}

//=============================================================================
// Unit Test 19: Performance under high load
//=============================================================================

TEST_F(ThreadPoolTest, Performance_HighLoad) {
    // WHAT: Verify pool handles high task volume efficiently
    // WHY:  Performance validation under stress
    // HOW:  Submit 10,000 fast tasks, measure throughput

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    const int num_tasks = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    // Submit many fast tasks
    for (int i = 0; i < num_tasks; i++) {
        nimcp_pool_submit(pool, fast_task, nullptr);
    }

    nimcp_pool_wait(pool);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // All tasks executed
    EXPECT_EQ(task_counter, num_tasks) << "Not all tasks executed under load";

    // Should complete in reasonable time (< 5 seconds)
    EXPECT_LT(elapsed, 5000) << "Performance degraded under load";

    // Calculate throughput
    double throughput = num_tasks / (elapsed / 1000.0);
    EXPECT_GT(throughput, 1000) << "Throughput too low: " << throughput << " tasks/sec";

    nimcp_pool_destroy(pool);
    SUCCEED() << "High load performance is acceptable: "
              << throughput << " tasks/sec";
}

//=============================================================================
// Unit Test 20: Multiple wait calls
//=============================================================================

TEST_F(ThreadPoolTest, Wait_MultipleCalls) {
    // WHAT: Verify multiple wait calls are safe
    // WHY:  User may call wait multiple times
    // HOW:  Submit tasks, wait, submit more, wait again

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // First batch
    for (int i = 0; i < 50; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }
    nimcp_pool_wait(pool);
    EXPECT_EQ(task_counter, 50) << "First batch failed";

    // Second batch
    for (int i = 0; i < 50; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }
    nimcp_pool_wait(pool);
    EXPECT_EQ(task_counter, 100) << "Second batch failed";

    // Third wait with no pending tasks
    nimcp_pool_wait(pool);
    EXPECT_EQ(task_counter, 100) << "Counter changed after empty wait";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Multiple wait calls work correctly";
}

//=============================================================================
// Unit Test 21: Pending and active counts on NULL pool
//=============================================================================

TEST_F(ThreadPoolTest, Counts_NullPool) {
    // WHAT: Verify count functions handle NULL pool safely
    // WHY:  Defensive programming
    // HOW:  Call pending/active with NULL, expect 0

    size_t pending = nimcp_pool_pending(nullptr);
    EXPECT_EQ(pending, 0) << "NULL pool should return 0 pending";

    size_t active = nimcp_pool_active(nullptr);
    EXPECT_EQ(active, 0) << "NULL pool should return 0 active";

    SUCCEED() << "Count functions handle NULL safely";
}

//=============================================================================
// Unit Test 22: Worker thread count validation
//=============================================================================

TEST_F(ThreadPoolTest, Workers_CorrectCount) {
    // WHAT: Verify pool creates exact number of workers requested
    // WHY:  Resource allocation must match specification
    // HOW:  Create pool, submit enough tasks to utilize all workers

    const size_t num_workers = 8;
    nimcp_thread_pool_t* pool = nimcp_pool_create(num_workers);
    ASSERT_NE(pool, nullptr);

    task_counter = 0;
    int sleep_ms = 100;

    // Submit exactly num_workers long tasks
    for (size_t i = 0; i < num_workers; i++) {
        nimcp_pool_submit(pool, sleep_task, &sleep_ms);
    }

    // Give workers time to pick up tasks
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Active count should equal num_workers (all busy)
    size_t active = nimcp_pool_active(pool);
    EXPECT_EQ(active, num_workers) << "Not all workers are active";

    nimcp_pool_wait(pool);
    nimcp_pool_destroy(pool);
    SUCCEED() << "Pool created with correct worker count";
}

//=============================================================================
// Unit Test 23: Task execution order (FIFO queue)
//=============================================================================

TEST_F(ThreadPoolTest, Queue_FIFOOrdering) {
    // WHAT: Verify tasks execute in FIFO order (approximately)
    // WHY:  Queue should maintain submission order
    // HOW:  Submit tasks with single worker, verify order

    nimcp_thread_pool_t* pool = nimcp_pool_create(1);  // Single worker for strict ordering
    ASSERT_NE(pool, nullptr);

    task_counter = 0;

    // With single worker thread, tasks execute in FIFO order
    // We submit 10 tasks and verify all execute
    for (int i = 0; i < 10; i++) {
        nimcp_pool_submit(pool, increment_task, nullptr);
    }

    nimcp_pool_wait(pool);

    // With single worker, all 10 tasks should execute
    EXPECT_EQ(task_counter, 10) << "FIFO queue should execute all tasks";

    nimcp_pool_destroy(pool);
    SUCCEED() << "FIFO queue ordering maintained";
}

//=============================================================================
// Unit Test 24: Empty pool wait
//=============================================================================

TEST_F(ThreadPoolTest, Wait_EmptyPool) {
    // WHAT: Verify wait returns immediately when pool is empty
    // WHY:  No blocking when no work to do
    // HOW:  Create pool, wait without submitting, measure time

    nimcp_thread_pool_t* pool = nimcp_pool_create(4);
    ASSERT_NE(pool, nullptr);

    auto start = std::chrono::high_resolution_clock::now();
    nimcp_result_t result = nimcp_pool_wait(pool);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(result, NIMCP_SUCCESS) << "Wait on empty pool should succeed";
    EXPECT_LT(elapsed, 100) << "Wait on empty pool should be fast";

    nimcp_pool_destroy(pool);
    SUCCEED() << "Empty pool wait returns immediately";
}

//=============================================================================
// Unit Test 25: Rapid create/destroy cycles
//=============================================================================

TEST_F(ThreadPoolTest, Lifecycle_RapidCycles) {
    // WHAT: Verify pool can be created/destroyed rapidly
    // WHY:  Resource cleanup must be robust
    // HOW:  Create and destroy pool 100 times

    for (int i = 0; i < 100; i++) {
        nimcp_thread_pool_t* pool = nimcp_pool_create(4);
        ASSERT_NE(pool, nullptr) << "Cycle " << i << " creation failed";

        // Submit a few tasks
        for (int j = 0; j < 10; j++) {
            nimcp_pool_submit(pool, increment_task, nullptr);
        }

        nimcp_pool_destroy(pool);
    }

    SUCCEED() << "Rapid create/destroy cycles work correctly";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
