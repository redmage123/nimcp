/**
 * @file unit_utils_thread_test_barrier.cpp
 * @brief Comprehensive unit tests for NIMCP barrier synchronization primitive
 *
 * WHAT: Tests barrier init/destroy, wait, reset, statistics, edge cases
 * WHY:  Ensure correct synchronization behavior under various conditions
 *
 * TEST PHILOSOPHY:
 * - TDD: Tests drive implementation
 * - Comprehensive coverage: Normal cases, edge cases, error cases
 * - Concurrent testing: Verify actual thread synchronization
 * - Performance testing: Measure barrier overhead
 * - Deterministic: Repeatable results despite concurrency
 *
 * COVERAGE TARGETS:
 * - All API functions (100% function coverage)
 * - All error paths (NULL params, invalid args, busy state)
 * - All success paths (various thread counts, multiple cycles)
 * - Concurrent scenarios (race conditions, timing variations)
 *
 * @author NIMCP Development Team
 * @date 2025-11-26
 */

#include <gtest/gtest.h>
#include "utils/thread/nimcp_barrier.h"
#include "utils/thread/nimcp_thread.h"
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Basic barrier test fixture
 *
 * WHAT: Setup/teardown for single-threaded barrier tests
 * WHY:  Clean state for each test, automatic cleanup
 */
class BarrierBasicTest : public ::testing::Test {
protected:
    nimcp_barrier_t* barrier;

    void SetUp() override {
        /* WHAT: Initialize barrier to NULL
         * WHY:  Safe initial state, detect init failures
         */
        barrier = nullptr;
    }

    void TearDown() override {
        /* WHAT: Destroy barrier if allocated
         * WHY:  Prevent memory leaks
         */
        if (barrier) {
            nimcp_barrier_destroy(&barrier);
        }
    }
};

/**
 * @brief Multi-threaded barrier test fixture
 *
 * WHAT: Infrastructure for concurrent barrier testing
 * WHY:  Test actual thread synchronization behavior
 */
class BarrierConcurrentTest : public ::testing::Test {
protected:
    nimcp_barrier_t* barrier;
    std::atomic<int> serial_count;    // Count serial thread returns
    std::atomic<int> success_count;   // Count normal returns
    std::atomic<int> error_count;     // Count errors

    void SetUp() override {
        barrier = nullptr;
        serial_count = 0;
        success_count = 0;
        error_count = 0;
    }

    void TearDown() override {
        if (barrier) {
            nimcp_barrier_destroy(&barrier);
        }
    }

    /**
     * @brief Worker thread function for barrier testing
     *
     * WHAT: Thread that waits at barrier and records result
     * WHY:  Verify barrier synchronization behavior
     */
    static void* barrier_worker(void* arg) {
        BarrierConcurrentTest* test = static_cast<BarrierConcurrentTest*>(arg);

        nimcp_result_t result = nimcp_barrier_wait(test->barrier);

        if (result == NIMCP_BARRIER_SERIAL_THREAD) {
            test->serial_count++;
        } else if (result == NIMCP_SUCCESS) {
            test->success_count++;
        } else {
            test->error_count++;
        }

        return nullptr;
    }
};

//=============================================================================
// Initialization and Destruction Tests
//=============================================================================

/**
 * @brief Test barrier initialization with valid parameters
 *
 * WHAT: Create barrier with various thread counts
 * WHY:  Verify successful initialization
 */
TEST_F(BarrierBasicTest, InitSuccess) {
    // Test count=1 (edge case: single thread)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 1));
    EXPECT_NE(nullptr, barrier);
    EXPECT_EQ(0, nimcp_barrier_get_waiting(barrier));
    nimcp_barrier_destroy(&barrier);

    // Test count=2 (minimal useful barrier)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 2));
    EXPECT_NE(nullptr, barrier);
    nimcp_barrier_destroy(&barrier);

    // Test count=4 (typical case)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));
    EXPECT_NE(nullptr, barrier);
    nimcp_barrier_destroy(&barrier);

    // Test count=8 (larger thread count)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 8));
    EXPECT_NE(nullptr, barrier);
    nimcp_barrier_destroy(&barrier);

    // Test count=100 (stress test)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 100));
    EXPECT_NE(nullptr, barrier);
    nimcp_barrier_destroy(&barrier);

    barrier = nullptr;  // Prevent double-free in TearDown
}

/**
 * @brief Test barrier initialization with NULL barrier pointer
 *
 * WHAT: Call init with NULL barrier parameter
 * WHY:  Verify proper error handling
 */
TEST_F(BarrierBasicTest, InitNullBarrier) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_init(nullptr, 4));
}

/**
 * @brief Test barrier initialization with zero count
 *
 * WHAT: Call init with count=0
 * WHY:  Verify rejection of invalid count (would deadlock)
 */
TEST_F(BarrierBasicTest, InitZeroCount) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_init(&barrier, 0));
    EXPECT_EQ(nullptr, barrier);
}

/**
 * @brief Test barrier destruction with valid barrier
 *
 * WHAT: Destroy properly initialized barrier
 * WHY:  Verify clean destruction
 */
TEST_F(BarrierBasicTest, DestroySuccess) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_destroy(&barrier));
    EXPECT_EQ(nullptr, barrier);
}

/**
 * @brief Test barrier destruction with NULL pointer
 *
 * WHAT: Call destroy with NULL barrier parameter
 * WHY:  Verify error handling
 */
TEST_F(BarrierBasicTest, DestroyNullBarrier) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_destroy(nullptr));
}

/**
 * @brief Test barrier destruction with NULL dereferenced pointer
 *
 * WHAT: Call destroy with pointer to NULL
 * WHY:  Verify error handling for already-freed barrier
 */
TEST_F(BarrierBasicTest, DestroyNullDereference) {
    barrier = nullptr;
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_destroy(&barrier));
}

//=============================================================================
// Statistics and Query Tests
//=============================================================================

/**
 * @brief Test get_waiting on newly initialized barrier
 *
 * WHAT: Query waiting count on fresh barrier
 * WHY:  Verify initial state is zero
 */
TEST_F(BarrierBasicTest, GetWaitingInitial) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));
    EXPECT_EQ(0, nimcp_barrier_get_waiting(barrier));
}

/**
 * @brief Test get_waiting with NULL barrier
 *
 * WHAT: Query waiting count on NULL barrier
 * WHY:  Verify error handling (returns 0)
 */
TEST_F(BarrierBasicTest, GetWaitingNull) {
    EXPECT_EQ(0, nimcp_barrier_get_waiting(nullptr));
}

/**
 * @brief Test statistics initial values
 *
 * WHAT: Query total_waits and total_cycles on fresh barrier
 * WHY:  Verify statistics initialized to zero
 */
TEST_F(BarrierBasicTest, StatisticsInitial) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));
    EXPECT_EQ(0, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(0, nimcp_barrier_get_total_cycles(barrier));
}

/**
 * @brief Test statistics with NULL barrier
 *
 * WHAT: Query statistics on NULL barrier
 * WHY:  Verify error handling (returns 0)
 */
TEST_F(BarrierBasicTest, StatisticsNull) {
    EXPECT_EQ(0, nimcp_barrier_get_total_waits(nullptr));
    EXPECT_EQ(0, nimcp_barrier_get_total_cycles(nullptr));
}

//=============================================================================
// Reset Tests
//=============================================================================

/**
 * @brief Test barrier reset with valid parameters
 *
 * WHAT: Reset barrier to different thread count
 * WHY:  Verify reset functionality
 */
TEST_F(BarrierBasicTest, ResetSuccess) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));

    // Reset to smaller count
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_reset(barrier, 2));

    // Reset to larger count
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_reset(barrier, 8));

    // Reset to same count
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_reset(barrier, 8));

    // Verify statistics reset
    EXPECT_EQ(0, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(0, nimcp_barrier_get_total_cycles(barrier));
}

/**
 * @brief Test reset with NULL barrier
 *
 * WHAT: Reset NULL barrier
 * WHY:  Verify error handling
 */
TEST_F(BarrierBasicTest, ResetNullBarrier) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_reset(nullptr, 4));
}

/**
 * @brief Test reset with zero count
 *
 * WHAT: Reset barrier to count=0
 * WHY:  Verify rejection of invalid count
 */
TEST_F(BarrierBasicTest, ResetZeroCount) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_reset(barrier, 0));
}

//=============================================================================
// Single-Threaded Wait Tests
//=============================================================================

/**
 * @brief Test wait on NULL barrier
 *
 * WHAT: Wait on NULL barrier
 * WHY:  Verify error handling
 */
TEST_F(BarrierBasicTest, WaitNullBarrier) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_barrier_wait(nullptr));
}

/**
 * @brief Test wait with count=1 (immediate release)
 *
 * WHAT: Single thread barrier (edge case)
 * WHY:  Verify immediate release with serial thread return
 */
TEST_F(BarrierBasicTest, WaitSingleThread) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 1));

    // First wait should return serial thread
    EXPECT_EQ(NIMCP_BARRIER_SERIAL_THREAD, nimcp_barrier_wait(barrier));

    // Verify statistics
    EXPECT_EQ(1, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(1, nimcp_barrier_get_total_cycles(barrier));

    // Second wait (barrier is cyclic)
    EXPECT_EQ(NIMCP_BARRIER_SERIAL_THREAD, nimcp_barrier_wait(barrier));
    EXPECT_EQ(2, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(2, nimcp_barrier_get_total_cycles(barrier));
}

//=============================================================================
// Multi-Threaded Wait Tests
//=============================================================================

/**
 * @brief Test barrier with 2 threads
 *
 * WHAT: Minimal useful barrier (2 threads synchronize)
 * WHY:  Verify basic barrier functionality
 */
TEST_F(BarrierConcurrentTest, Wait2Threads) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 2));

    // Create 2 threads
    nimcp_thread_t threads[2];
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], barrier_worker, this, nullptr));
    }

    // Wait for threads to complete
    for (int i = 0; i < 2; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    // Verify results: 1 serial, 1 success, 0 errors
    EXPECT_EQ(1, serial_count);
    EXPECT_EQ(1, success_count);
    EXPECT_EQ(0, error_count);

    // Verify statistics
    EXPECT_EQ(2, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(1, nimcp_barrier_get_total_cycles(barrier));
}

/**
 * @brief Test barrier with 4 threads
 *
 * WHAT: Typical barrier use case
 * WHY:  Verify multi-thread synchronization
 */
TEST_F(BarrierConcurrentTest, Wait4Threads) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 4));

    // Create 4 threads
    nimcp_thread_t threads[4];
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], barrier_worker, this, nullptr));
    }

    // Wait for threads
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    // Verify: 1 serial, 3 success, 0 errors
    EXPECT_EQ(1, serial_count);
    EXPECT_EQ(3, success_count);
    EXPECT_EQ(0, error_count);

    // Verify statistics
    EXPECT_EQ(4, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(1, nimcp_barrier_get_total_cycles(barrier));
}

/**
 * @brief Test barrier with 8 threads
 *
 * WHAT: Larger thread count barrier
 * WHY:  Verify scalability and correctness
 */
TEST_F(BarrierConcurrentTest, Wait8Threads) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 8));

    nimcp_thread_t threads[8];
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], barrier_worker, this, nullptr));
    }

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    EXPECT_EQ(1, serial_count);
    EXPECT_EQ(7, success_count);
    EXPECT_EQ(0, error_count);

    EXPECT_EQ(8, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(1, nimcp_barrier_get_total_cycles(barrier));
}

//=============================================================================
// Cyclic Barrier Tests (Multiple Cycles)
//=============================================================================

/**
 * @brief Cyclic barrier fixture for multi-cycle testing
 */
class BarrierCyclicTest : public ::testing::Test {
protected:
    nimcp_barrier_t* barrier;
    static constexpr int NUM_THREADS = 4;
    static constexpr int NUM_CYCLES = 5;

    std::atomic<int> cycle_counts[NUM_CYCLES];
    std::atomic<int> serial_counts[NUM_CYCLES];

    void SetUp() override {
        barrier = nullptr;
        for (int i = 0; i < NUM_CYCLES; i++) {
            cycle_counts[i] = 0;
            serial_counts[i] = 0;
        }
    }

    void TearDown() override {
        if (barrier) {
            nimcp_barrier_destroy(&barrier);
        }
    }

    /**
     * @brief Worker that goes through multiple barrier cycles
     */
    static void* multi_cycle_worker(void* arg) {
        BarrierCyclicTest* test = static_cast<BarrierCyclicTest*>(arg);

        for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
            // Increment counter before barrier
            test->cycle_counts[cycle]++;

            // Wait at barrier
            nimcp_result_t result = nimcp_barrier_wait(test->barrier);

            // Track serial thread
            if (result == NIMCP_BARRIER_SERIAL_THREAD) {
                test->serial_counts[cycle]++;
            }
        }

        return nullptr;
    }
};

/**
 * @brief Test barrier reuse over multiple cycles
 *
 * WHAT: Multiple threads go through barrier N times
 * WHY:  Verify cyclic barrier functionality
 */
TEST_F(BarrierCyclicTest, MultipleCycles) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, NUM_THREADS));

    nimcp_thread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], multi_cycle_worker, this, nullptr));
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    // Verify each cycle had all threads arrive
    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        EXPECT_EQ(NUM_THREADS, cycle_counts[cycle])
            << "Cycle " << cycle << " incomplete";
        EXPECT_EQ(1, serial_counts[cycle])
            << "Cycle " << cycle << " serial count wrong";
    }

    // Verify statistics
    EXPECT_EQ(NUM_THREADS * NUM_CYCLES, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(NUM_CYCLES, nimcp_barrier_get_total_cycles(barrier));
}

//=============================================================================
// Get Waiting Count Tests
//=============================================================================

/**
 * @brief Test fixture for monitoring waiting count
 */
class BarrierWaitingCountTest : public ::testing::Test {
protected:
    nimcp_barrier_t* barrier;
    std::atomic<bool> start_waiting;
    std::atomic<int> waiting_snapshots[10];
    static constexpr int NUM_THREADS = 4;

    void SetUp() override {
        barrier = nullptr;
        start_waiting = false;
        for (int i = 0; i < 10; i++) {
            waiting_snapshots[i] = -1;
        }
    }

    void TearDown() override {
        if (barrier) {
            nimcp_barrier_destroy(&barrier);
        }
    }

    /**
     * @brief Worker that waits for signal then enters barrier
     */
    static void* delayed_worker(void* arg) {
        BarrierWaitingCountTest* test = static_cast<BarrierWaitingCountTest*>(arg);

        // Wait for start signal
        while (!test->start_waiting) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Enter barrier
        nimcp_barrier_wait(test->barrier);

        return nullptr;
    }
};

/**
 * @brief Test get_waiting during barrier wait
 *
 * WHAT: Monitor waiting count as threads arrive
 * WHY:  Verify waiting count tracking
 */
TEST_F(BarrierWaitingCountTest, MonitorWaitingCount) {
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, NUM_THREADS));

    // Initial count should be 0
    EXPECT_EQ(0, nimcp_barrier_get_waiting(barrier));

    // Create threads but don't let them proceed yet
    nimcp_thread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], delayed_worker, this, nullptr));
    }

    // Give threads time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Signal threads to proceed (they will wait at barrier)
    start_waiting = true;

    // Monitor waiting count (should increase to NUM_THREADS-1, then reset to 0)
    // Note: This is inherently racy, but we can verify it never exceeds NUM_THREADS
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        uint32_t count = nimcp_barrier_get_waiting(barrier);
        waiting_snapshots[i] = count;
        EXPECT_LE(count, NUM_THREADS);
    }

    // Wait for threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    // Final count should be 0
    EXPECT_EQ(0, nimcp_barrier_get_waiting(barrier));
}

//=============================================================================
// Reset with Waiting Threads Test
//=============================================================================

/**
 * @brief Test fixture for reset while threads waiting
 */
class BarrierResetBusyTest : public ::testing::Test {
protected:
    nimcp_barrier_t* barrier;
    std::atomic<bool> threads_started;
    static constexpr int NUM_THREADS = 4;

    void SetUp() override {
        barrier = nullptr;
        threads_started = false;
    }

    void TearDown() override {
        if (barrier) {
            // Note: We may have waiting threads, need to release them first
            // For this test, we'll just destroy (tests error handling)
            nimcp_barrier_destroy(&barrier);
        }
    }

    static void* waiting_worker(void* arg) {
        BarrierResetBusyTest* test = static_cast<BarrierResetBusyTest*>(arg);
        test->threads_started = true;

        // This will block since we don't have enough threads
        nimcp_barrier_wait(test->barrier);

        return nullptr;
    }
};

/**
 * @brief Test reset while threads are waiting
 *
 * WHAT: Try to reset barrier while threads blocked
 * WHY:  Verify BUSY error when unsafe to reset
 */
TEST_F(BarrierResetBusyTest, ResetWhileWaiting) {
    // Create barrier requiring 2 threads (worker + main can complete it)
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, 2));

    // Create worker thread that will block at barrier
    nimcp_thread_t thread;
    ASSERT_EQ(NIMCP_SUCCESS,
              nimcp_thread_create(&thread, waiting_worker, this, nullptr));

    // Wait for thread to start and enter barrier
    while (!threads_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Verify thread is waiting
    EXPECT_GT(nimcp_barrier_get_waiting(barrier), 0);

    // Try to reset - should fail with BUSY (thread is blocked)
    EXPECT_EQ(NIMCP_BUSY, nimcp_barrier_reset(barrier, 2));

    // Complete the barrier by arriving ourselves (2 threads now met)
    nimcp_barrier_wait(barrier);

    // Join thread
    nimcp_thread_join(thread, nullptr);

    // Now reset should succeed (no waiting threads)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_barrier_reset(barrier, 2));
}

//=============================================================================
// Performance and Stress Tests
//=============================================================================

/**
 * @brief Test barrier with many threads (stress test)
 *
 * WHAT: 16 threads synchronizing at barrier
 * WHY:  Verify correctness under higher contention
 */
TEST_F(BarrierConcurrentTest, StressTest16Threads) {
    constexpr int THREAD_COUNT = 16;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, THREAD_COUNT));

    nimcp_thread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], barrier_worker, this, nullptr));
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_thread_join(threads[i], nullptr));
    }

    EXPECT_EQ(1, serial_count);
    EXPECT_EQ(THREAD_COUNT - 1, success_count);
    EXPECT_EQ(0, error_count);
}

/**
 * @brief Test barrier latency measurement
 *
 * WHAT: Measure time to release N threads
 * WHY:  Verify acceptable performance
 */
TEST_F(BarrierConcurrentTest, PerformanceTest) {
    constexpr int THREAD_COUNT = 8;
    constexpr int ITERATIONS = 100;

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_barrier_init(&barrier, THREAD_COUNT));

    std::atomic<bool> start_flag{false};

    auto timed_worker = [](void* arg) -> void* {
        struct Args {
            nimcp_barrier_t* barrier_ptr;
            std::atomic<bool>* start;
            int iterations;
        };
        Args* args = static_cast<Args*>(arg);

        // Wait for start signal
        while (!args->start->load()) {
            std::this_thread::yield();
        }

        // Run multiple iterations
        for (int i = 0; i < args->iterations; i++) {
            nimcp_barrier_wait(args->barrier_ptr);
        }

        return nullptr;
    };

    struct WorkerArgs {
        nimcp_barrier_t* barrier_ptr;
        std::atomic<bool>* start;
        int iterations;
    } args = {barrier, &start_flag, ITERATIONS};

    nimcp_thread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        ASSERT_EQ(NIMCP_SUCCESS,
                  nimcp_thread_create(&threads[i], timed_worker, &args, nullptr));
    }

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();
    start_flag = true;

    // Wait for completion
    for (int i = 0; i < THREAD_COUNT; i++) {
        nimcp_thread_join(threads[i], nullptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Calculate average barrier latency
    double avg_latency_us = static_cast<double>(duration.count()) / ITERATIONS;

    // Report performance (should be < 1000µs for 8 threads)
    // Note: This is informational, not a hard requirement
    std::cout << "Barrier latency (" << THREAD_COUNT << " threads): "
              << avg_latency_us << " µs/cycle" << std::endl;

    // Verify correctness
    EXPECT_EQ(THREAD_COUNT * ITERATIONS, nimcp_barrier_get_total_waits(barrier));
    EXPECT_EQ(ITERATIONS, nimcp_barrier_get_total_cycles(barrier));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
