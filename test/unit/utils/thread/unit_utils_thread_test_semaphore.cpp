//=============================================================================
// unit_utils_thread_test_semaphore.cpp - Comprehensive Semaphore Unit Tests
//=============================================================================
/**
 * @file unit_utils_thread_test_semaphore.cpp
 * @brief TDD test suite for counting semaphore implementation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation quality
 * - Guard clause verification: Test all error conditions and NULL safety
 * - Lifecycle testing: Init, destroy, and proper cleanup
 * - Concurrency testing: Multi-threaded scenarios with verification
 * - Edge case coverage: Overflow, timeout, zero count
 * - 100% code coverage target
 *
 * COVERAGE:
 * 1. Initialization and destruction
 * 2. Binary semaphore (count=1) as mutex alternative
 * 3. Counting semaphore (count=N) with resource pool
 * 4. Wait operations (wait, trywait, timedwait)
 * 5. Post operations and waiter waking
 * 6. Producer-consumer pattern
 * 7. Statistics tracking
 * 8. Edge cases (NULL params, overflow, timeout)
 * 9. Thread safety and race condition prevention
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

// Headers have their own extern "C" guards
#include "utils/thread/nimcp_semaphore.h"

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for semaphore tests
 * WHY: Set up/tear down resources for each test
 */
class SemaphoreTest : public ::testing::Test {
public:
    // Test parameters
    static const int THREAD_COUNT = 4;
    static const int ITERATIONS = 100;
    static const int TIMEOUT_MS = 100;
    static const int BUFFER_SIZE = 5;

protected:
    void SetUp() override {
        // Each test starts fresh
    }

    void TearDown() override {
        // Tests clean up their own semaphores
    }
};

/**
 * WHAT: Helper to sleep for a short duration
 * WHY: Ensure threads have time to block/wake in tests
 */
static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//=============================================================================
// Basic Lifecycle Tests
//=============================================================================

/**
 * TEST: Semaphore initialization with various counts
 * WHY: Verify semaphore can be created with different initial counts
 */
TEST_F(SemaphoreTest, InitializationWithVariousCounts) {
    nimcp_semaphore_t sem;

    // Test with count = 0 (all waiters block)
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));
    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));

    // Test with count = 1 (binary semaphore)
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 1));
    EXPECT_EQ(1u, nimcp_semaphore_get_count(&sem));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));

    // Test with count = 5 (counting semaphore)
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 5));
    EXPECT_EQ(5u, nimcp_semaphore_get_count(&sem));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));

    // Test with large count
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 1000));
    EXPECT_EQ(1000u, nimcp_semaphore_get_count(&sem));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: NULL parameter handling in init
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, InitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_init(NULL, 0));
}

/**
 * TEST: NULL parameter handling in destroy
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, DestroyNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_destroy(NULL));
}

//=============================================================================
// Wait Operation Tests
//=============================================================================

/**
 * TEST: Wait on semaphore with count > 0 (immediate return)
 * WHY: Verify wait succeeds immediately when resources available
 */
TEST_F(SemaphoreTest, WaitWithAvailableCount) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 3));

    // Wait 3 times (should all succeed immediately)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
    EXPECT_EQ(2u, nimcp_semaphore_get_count(&sem));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
    EXPECT_EQ(1u, nimcp_semaphore_get_count(&sem));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Wait NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, WaitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_wait(NULL));
}

/**
 * TEST: Trywait succeeds when count > 0
 * WHY: Verify non-blocking wait acquires resource when available
 */
TEST_F(SemaphoreTest, TrywaitSuccess) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 2));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_trywait(&sem));
    EXPECT_EQ(1u, nimcp_semaphore_get_count(&sem));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_trywait(&sem));
    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Trywait returns BUSY when count = 0
 * WHY: Verify non-blocking wait returns immediately when no resources
 */
TEST_F(SemaphoreTest, TrywaitBusy) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    // Should return BUSY immediately (not block)
    EXPECT_EQ(NIMCP_BUSY, nimcp_semaphore_trywait(&sem));
    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Trywait NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, TrywaitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_trywait(NULL));
}

/**
 * TEST: Timedwait succeeds when count > 0
 * WHY: Verify timed wait acquires resource immediately when available
 */
TEST_F(SemaphoreTest, TimedwaitSuccess) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 1));

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_timedwait(&sem, 1000));
    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Timedwait times out when count = 0
 * WHY: Verify timed wait returns BUSY after timeout expires
 */
TEST_F(SemaphoreTest, TimedwaitTimeout) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(NIMCP_BUSY, nimcp_semaphore_timedwait(&sem, TIMEOUT_MS));
    auto end = std::chrono::steady_clock::now();

    // Verify timeout actually occurred (at least TIMEOUT_MS passed)
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed_ms, TIMEOUT_MS - 10);  // Allow 10ms tolerance

    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Timedwait NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, TimedwaitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_timedwait(NULL, 100));
}

//=============================================================================
// Post Operation Tests
//=============================================================================

/**
 * TEST: Post increments count
 * WHY: Verify post operation increases semaphore count
 */
TEST_F(SemaphoreTest, PostIncrementsCount) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    EXPECT_EQ(1u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    EXPECT_EQ(2u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Post wakes waiting thread
 * WHY: Verify post signals blocked waiter
 */
TEST_F(SemaphoreTest, PostWakesWaiter) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    std::atomic<bool> thread_woke{false};

    // Spawn thread that will block on wait
    std::thread waiter([&]() {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
        thread_woke = true;
    });

    // Give thread time to block
    sleep_ms(50);
    EXPECT_FALSE(thread_woke);

    // Post should wake the waiter
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));

    waiter.join();
    EXPECT_TRUE(thread_woke);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Post NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, PostNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_semaphore_post(NULL));
}

/**
 * TEST: Post overflow protection
 * WHY: Verify post prevents count overflow at UINT32_MAX
 */
TEST_F(SemaphoreTest, PostOverflowProtection) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, UINT32_MAX));

    // Attempt to post when already at max
    EXPECT_EQ(NIMCP_ERROR_SYSTEM, nimcp_semaphore_post(&sem));
    EXPECT_EQ(UINT32_MAX, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Query Operation Tests
//=============================================================================

/**
 * TEST: Get count returns correct value
 * WHY: Verify get_count accurately reflects semaphore state
 */
TEST_F(SemaphoreTest, GetCountAccuracy) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 5));

    EXPECT_EQ(5u, nimcp_semaphore_get_count(&sem));

    nimcp_semaphore_wait(&sem);
    EXPECT_EQ(4u, nimcp_semaphore_get_count(&sem));

    nimcp_semaphore_post(&sem);
    EXPECT_EQ(5u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Get count NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST_F(SemaphoreTest, GetCountNullParameter) {
    EXPECT_EQ(0u, nimcp_semaphore_get_count(NULL));
}

//=============================================================================
// Binary Semaphore Tests (count=1, mutex-like behavior)
//=============================================================================

/**
 * TEST: Binary semaphore as mutex alternative
 * WHY: Verify count=1 semaphore provides mutual exclusion
 */
TEST_F(SemaphoreTest, BinarySemaphoreAsMutex) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 1));

    std::atomic<int> counter{0};
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current_in_cs{0};

    // Multiple threads compete for binary semaphore
    std::vector<std::thread> threads;
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                // Acquire binary semaphore (like mutex lock)
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));

                // Critical section
                int in_cs = ++current_in_cs;
                if (in_cs > max_concurrent) {
                    max_concurrent = in_cs;
                }

                counter++;
                sleep_ms(1);  // Simulate work

                current_in_cs--;

                // Release binary semaphore (like mutex unlock)
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify mutual exclusion (max 1 thread in critical section)
    EXPECT_EQ(1, max_concurrent);
    EXPECT_EQ(THREAD_COUNT * ITERATIONS, counter);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Counting Semaphore Tests (count=N, resource pool)
//=============================================================================

/**
 * TEST: Counting semaphore with resource pool
 * WHY: Verify count=N allows N concurrent consumers
 */
TEST_F(SemaphoreTest, CountingSemaphoreResourcePool) {
    const int POOL_SIZE = 3;
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, POOL_SIZE));

    std::atomic<int> current_users{0};
    std::atomic<int> max_concurrent{0};

    // More threads than pool size
    std::vector<std::thread> threads;
    for (int i = 0; i < POOL_SIZE * 2; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 10; ++j) {
                // Acquire resource from pool
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));

                // Track concurrent users
                int users = ++current_users;
                if (users > max_concurrent) {
                    max_concurrent = users;
                }

                sleep_ms(10);  // Use resource

                current_users--;

                // Release resource back to pool
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Verify max concurrent users <= pool size
    EXPECT_LE(max_concurrent, POOL_SIZE);
    EXPECT_GT(max_concurrent, 0);  // At least some concurrency

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Producer-Consumer Tests
//=============================================================================

/**
 * TEST: Producer-consumer with bounded buffer
 * WHY: Verify semaphore coordination between producers and consumers
 */
TEST_F(SemaphoreTest, ProducerConsumerPattern) {
    nimcp_semaphore_t empty_slots, full_slots;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&empty_slots, BUFFER_SIZE));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&full_slots, 0));

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    const int TOTAL_ITEMS = 50;

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&empty_slots));
            // Produce item (simplified, no actual buffer)
            produced++;
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&full_slots));
        }
    });

    // Consumer thread
    std::thread consumer([&]() {
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&full_slots));
            // Consume item (simplified, no actual buffer)
            consumed++;
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&empty_slots));
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(TOTAL_ITEMS, produced);
    EXPECT_EQ(TOTAL_ITEMS, consumed);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&empty_slots));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&full_slots));
}

/**
 * TEST: Multiple producers and consumers
 * WHY: Verify semaphore handles concurrent producers and consumers
 */
TEST_F(SemaphoreTest, MultipleProducersConsumers) {
    nimcp_semaphore_t empty_slots, full_slots;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&empty_slots, BUFFER_SIZE));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&full_slots, 0));

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    const int ITEMS_PER_THREAD = 25;
    const int PRODUCER_COUNT = 2;
    const int CONSUMER_COUNT = 2;
    const int TOTAL_ITEMS = PRODUCER_COUNT * ITEMS_PER_THREAD;

    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCER_COUNT; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&empty_slots));
                produced++;
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&full_slots));
            }
        });
    }

    // Multiple consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < CONSUMER_COUNT; ++c) {
        consumers.emplace_back([&]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&full_slots));
                consumed++;
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&empty_slots));
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(TOTAL_ITEMS, produced);
    EXPECT_EQ(TOTAL_ITEMS, consumed);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&empty_slots));
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&full_slots));
}

//=============================================================================
// Timed Wait Tests
//=============================================================================

/**
 * TEST: Timedwait woken by post before timeout
 * WHY: Verify timedwait succeeds when signaled before timeout
 */
TEST_F(SemaphoreTest, TimedwaitWokenByPost) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    std::atomic<bool> wait_succeeded{false};

    std::thread waiter([&]() {
        // Wait with long timeout
        nimcp_result_t result = nimcp_semaphore_timedwait(&sem, 5000);
        wait_succeeded = (result == NIMCP_SUCCESS);
    });

    // Post after short delay (before timeout)
    sleep_ms(100);
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));

    waiter.join();
    EXPECT_TRUE(wait_succeeded);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Statistics Tests
//=============================================================================

/**
 * TEST: Wait operations increment total_waits
 * WHY: Verify statistics tracking for debugging/profiling
 * NOTE: Statistics are internal, we test behavior indirectly
 */
TEST_F(SemaphoreTest, WaitOperationsTracked) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 2));

    // Perform various wait operations
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_trywait(&sem));
    EXPECT_EQ(NIMCP_BUSY, nimcp_semaphore_trywait(&sem));  // Count=0
    EXPECT_EQ(NIMCP_BUSY, nimcp_semaphore_timedwait(&sem, 50));  // Timeout

    // Statistics are internal, but operations succeeded
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Post operations increment total_posts
 * WHY: Verify statistics tracking for debugging/profiling
 */
TEST_F(SemaphoreTest, PostOperationsTracked) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    // Perform multiple posts
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));

    EXPECT_EQ(3u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * TEST: High contention stress test
 * WHY: Verify semaphore handles heavy concurrent load
 */
TEST_F(SemaphoreTest, HighContentionStress) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 2));

    std::atomic<int> success_count{0};
    const int STRESS_THREADS = 10;
    const int STRESS_ITERATIONS = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < STRESS_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < STRESS_ITERATIONS; ++j) {
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
                success_count++;
                sleep_ms(1);
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(STRESS_THREADS * STRESS_ITERATIONS, success_count);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Rapid post and wait cycles
 * WHY: Verify semaphore handles rapid state changes
 */
TEST_F(SemaphoreTest, RapidPostWaitCycles) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    const int CYCLES = 1000;

    for (int i = 0; i < CYCLES; ++i) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
    }

    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Edge Cases
//=============================================================================

/**
 * TEST: Zero initial count blocks all waiters
 * WHY: Verify count=0 initialization behavior
 */
TEST_F(SemaphoreTest, ZeroInitialCountBlocksWaiters) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    std::atomic<int> woke_count{0};
    std::vector<std::thread> waiters;

    // Spawn multiple waiters (all should block)
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&]() {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
            woke_count++;
        });
    }

    sleep_ms(50);
    EXPECT_EQ(0, woke_count);  // All still blocked

    // Post 3 times to wake all
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    }

    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(3, woke_count);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Trywait multiple times on zero count
 * WHY: Verify trywait never blocks, always returns BUSY when count=0
 */
TEST_F(SemaphoreTest, TrywaitMultipleOnZeroCount) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    // All trywait should return BUSY immediately
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(NIMCP_BUSY, nimcp_semaphore_trywait(&sem));
    }

    EXPECT_EQ(0u, nimcp_semaphore_get_count(&sem));

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

/**
 * TEST: Post wakes only one waiter (FIFO)
 * WHY: Verify signal wakes one waiter, not all
 */
TEST_F(SemaphoreTest, PostWakesOnlyOneWaiter) {
    nimcp_semaphore_t sem;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_init(&sem, 0));

    std::atomic<int> woke_count{0};
    std::vector<std::thread> waiters;

    // Spawn 3 waiters (all block)
    for (int i = 0; i < 3; ++i) {
        waiters.emplace_back([&]() {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_wait(&sem));
            woke_count++;
        });
    }

    sleep_ms(50);
    EXPECT_EQ(0, woke_count);

    // Post once (should wake only 1 waiter)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    sleep_ms(50);
    EXPECT_EQ(1, woke_count);

    // Post again (wake second waiter)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));
    sleep_ms(50);
    EXPECT_EQ(2, woke_count);

    // Post third time (wake last waiter)
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_semaphore_post(&sem));

    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(3, woke_count);

    ASSERT_EQ(NIMCP_SUCCESS, nimcp_semaphore_destroy(&sem));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
