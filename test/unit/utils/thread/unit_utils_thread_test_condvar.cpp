//=============================================================================
// unit_utils_thread_test_condvar.cpp - Condition Variable Unit Tests
//=============================================================================
/**
 * @file unit_utils_thread_test_condvar.cpp
 * @brief TDD test suite for condition variable implementation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation quality
 * - Guard clause verification: Test all error conditions and NULL safety
 * - Lifecycle testing: Init, destroy, and proper cleanup
 * - Concurrency testing: Multi-threaded scenarios with verification
 * - Edge case coverage: Spurious wakeups, timeout, broadcast vs signal
 *
 * COVERAGE:
 * 1. Initialization and destruction
 * 2. Basic wait/signal pattern
 * 3. Broadcast wakes all waiters
 * 4. Timed wait with timeout
 * 5. Spurious wakeup handling (predicate pattern)
 * 6. Producer-consumer coordination
 * 7. Multiple waiters single signal
 * 8. NULL parameter handling
 * 9. Thread safety and race prevention
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
#include <queue>

// Headers have their own extern "C" guards
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for condition variable tests
 * WHY: Set up/tear down resources for each test
 */
class CondVarTest : public ::testing::Test {
public:
    // Test parameters
    static const int THREAD_COUNT = 4;
    static const int ITERATIONS = 50;
    static const int TIMEOUT_MS = 100;

protected:
    nimcp_cond_t cond;
    nimcp_mutex_t mutex;
    bool initialized_cond;
    bool initialized_mutex;

    void SetUp() override {
        initialized_cond = false;
        initialized_mutex = false;

        // Initialize mutex first
        if (nimcp_mutex_init(&mutex, NULL) == NIMCP_SUCCESS) {
            initialized_mutex = true;
        }

        // Initialize condition variable
        if (nimcp_cond_init(&cond) == NIMCP_SUCCESS) {
            initialized_cond = true;
        }
    }

    void TearDown() override {
        // Destroy in reverse order
        if (initialized_cond) {
            nimcp_cond_destroy(&cond);
        }
        if (initialized_mutex) {
            nimcp_mutex_destroy(&mutex);
        }
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
 * TEST: Condition variable initialization
 * WHY: Verify condition variable can be created
 */
TEST_F(CondVarTest, InitializationSuccess) {
    ASSERT_TRUE(initialized_cond);
    ASSERT_TRUE(initialized_mutex);
}

/**
 * TEST: Condition variable init with NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST(CondVarErrorTest, InitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_init(NULL));
}

/**
 * TEST: Condition variable destroy with NULL parameter
 * WHY: Verify error handling for invalid parameters
 */
TEST(CondVarErrorTest, DestroyNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_destroy(NULL));
}

/**
 * TEST: nimcp_cond_create allocates and initializes
 * WHY: Verify the create helper works correctly
 */
TEST(CondVarCreateTest, CreateSuccess) {
    nimcp_cond_t* cond = nimcp_cond_create();
    ASSERT_NE(nullptr, cond);

    // Clean up - need to destroy and free
    nimcp_cond_destroy(cond);
    // Note: nimcp_cond_create may allocate memory that needs freeing
    // depending on implementation
}

//=============================================================================
// Wait and Signal Tests
//=============================================================================

/**
 * TEST: Wait NULL cond parameter
 * WHY: Verify error handling for NULL condition variable
 */
TEST_F(CondVarTest, WaitNullCondParameter) {
    nimcp_mutex_lock(&mutex);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_wait(NULL, &mutex));
    nimcp_mutex_unlock(&mutex);
}

/**
 * TEST: Wait NULL mutex parameter
 * WHY: Verify error handling for NULL mutex
 */
TEST_F(CondVarTest, WaitNullMutexParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_wait(&cond, NULL));
}

/**
 * TEST: Signal NULL parameter
 * WHY: Verify error handling for NULL condition variable
 */
TEST(CondVarErrorTest, SignalNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_signal(NULL));
}

/**
 * TEST: Broadcast NULL parameter
 * WHY: Verify error handling for NULL condition variable
 */
TEST(CondVarErrorTest, BroadcastNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_broadcast(NULL));
}

/**
 * TEST: Basic signal wakes one waiter
 * WHY: Verify signal properly wakes a waiting thread
 */
TEST_F(CondVarTest, SignalWakesWaiter) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    std::atomic<bool> thread_woke{false};
    std::atomic<bool> thread_started{false};
    bool predicate = false;

    // Spawn thread that will block on wait
    std::thread waiter([&]() {
        nimcp_mutex_lock(&mutex);
        thread_started = true;

        // Wait with predicate pattern (handles spurious wakeups)
        while (!predicate) {
            nimcp_cond_wait(&cond, &mutex);
        }

        thread_woke = true;
        nimcp_mutex_unlock(&mutex);
    });

    // Wait for thread to start and enter wait
    while (!thread_started) {
        sleep_ms(10);
    }
    sleep_ms(50);  // Give time to enter wait

    EXPECT_FALSE(thread_woke);

    // Set predicate and signal
    nimcp_mutex_lock(&mutex);
    predicate = true;
    nimcp_mutex_unlock(&mutex);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));

    waiter.join();
    EXPECT_TRUE(thread_woke);
}

/**
 * TEST: Signal with no waiters does not block
 * WHY: Verify signal on empty condvar is safe
 */
TEST_F(CondVarTest, SignalNoWaiters) {
    ASSERT_TRUE(initialized_cond);

    // Signal with no waiters - should not block or error
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));
}

/**
 * TEST: Broadcast with no waiters does not block
 * WHY: Verify broadcast on empty condvar is safe
 */
TEST_F(CondVarTest, BroadcastNoWaiters) {
    ASSERT_TRUE(initialized_cond);

    // Broadcast with no waiters - should not block or error
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_broadcast(&cond));
}

//=============================================================================
// Broadcast Tests
//=============================================================================

/**
 * TEST: Broadcast wakes all waiting threads
 * WHY: Verify broadcast wakes all waiters, not just one
 */
TEST_F(CondVarTest, BroadcastWakesAllWaiters) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int NUM_WAITERS = 4;
    std::atomic<int> threads_woke{0};
    std::atomic<int> threads_started{0};
    bool predicate = false;

    // Spawn multiple waiting threads
    std::vector<std::thread> waiters;
    for (int i = 0; i < NUM_WAITERS; ++i) {
        waiters.emplace_back([&]() {
            nimcp_mutex_lock(&mutex);
            threads_started++;

            while (!predicate) {
                nimcp_cond_wait(&cond, &mutex);
            }

            threads_woke++;
            nimcp_mutex_unlock(&mutex);
        });
    }

    // Wait for all threads to enter wait
    while (threads_started < NUM_WAITERS) {
        sleep_ms(10);
    }
    sleep_ms(50);

    EXPECT_EQ(0, threads_woke.load());

    // Set predicate and broadcast
    nimcp_mutex_lock(&mutex);
    predicate = true;
    nimcp_mutex_unlock(&mutex);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_broadcast(&cond));

    // Join all threads
    for (auto& t : waiters) {
        t.join();
    }

    // All threads should have woken
    EXPECT_EQ(NUM_WAITERS, threads_woke.load());
}

/**
 * TEST: Signal wakes only one waiter when multiple waiting
 * WHY: Verify signal wakes exactly one waiter
 */
TEST_F(CondVarTest, SignalWakesOnlyOneWaiter) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int NUM_WAITERS = 3;
    std::atomic<int> threads_woke{0};
    std::atomic<int> threads_started{0};
    std::atomic<int> signal_count{0};

    // Spawn multiple waiting threads
    std::vector<std::thread> waiters;
    for (int i = 0; i < NUM_WAITERS; ++i) {
        waiters.emplace_back([&]() {
            nimcp_mutex_lock(&mutex);
            threads_started++;

            // Wait for a signal targeted at us
            while (signal_count <= threads_woke) {
                nimcp_cond_wait(&cond, &mutex);
            }

            threads_woke++;
            nimcp_mutex_unlock(&mutex);
        });
    }

    // Wait for all threads to enter wait
    while (threads_started < NUM_WAITERS) {
        sleep_ms(10);
    }
    sleep_ms(50);

    EXPECT_EQ(0, threads_woke.load());

    // Signal once - should wake exactly one
    signal_count = 1;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));
    sleep_ms(50);
    EXPECT_EQ(1, threads_woke.load());

    // Signal again - should wake second thread
    signal_count = 2;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));
    sleep_ms(50);
    EXPECT_EQ(2, threads_woke.load());

    // Signal third time - should wake last thread
    signal_count = 3;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));

    // Join all threads
    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(NUM_WAITERS, threads_woke.load());
}

//=============================================================================
// Timed Wait Tests
//=============================================================================

/**
 * TEST: Timedwait NULL cond parameter
 * WHY: Verify error handling for NULL condition variable
 */
TEST_F(CondVarTest, TimedwaitNullCondParameter) {
    nimcp_mutex_lock(&mutex);
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_timedwait(NULL, &mutex, 100));
    nimcp_mutex_unlock(&mutex);
}

/**
 * TEST: Timedwait NULL mutex parameter
 * WHY: Verify error handling for NULL mutex
 */
TEST_F(CondVarTest, TimedwaitNullMutexParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_cond_timedwait(&cond, NULL, 100));
}

/**
 * TEST: Timedwait times out when not signaled
 * WHY: Verify timedwait returns after timeout
 */
TEST_F(CondVarTest, TimedwaitTimeout) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    nimcp_mutex_lock(&mutex);

    auto start = std::chrono::steady_clock::now();
    nimcp_result_t result = nimcp_cond_timedwait(&cond, &mutex, TIMEOUT_MS);
    auto end = std::chrono::steady_clock::now();

    nimcp_mutex_unlock(&mutex);

    // Should return BUSY (timeout)
    EXPECT_EQ(NIMCP_BUSY, result);

    // Verify timeout actually occurred
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_GE(elapsed_ms, TIMEOUT_MS - 10);  // Allow 10ms tolerance
}

/**
 * TEST: Timedwait succeeds when signaled before timeout
 * WHY: Verify timedwait wakes on signal
 */
TEST_F(CondVarTest, TimedwaitSignaledBeforeTimeout) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    std::atomic<bool> wait_succeeded{false};
    std::atomic<bool> thread_started{false};
    bool predicate = false;

    std::thread waiter([&]() {
        nimcp_mutex_lock(&mutex);
        thread_started = true;

        while (!predicate) {
            nimcp_result_t result = nimcp_cond_timedwait(&cond, &mutex, 5000);
            if (result != NIMCP_SUCCESS && result != NIMCP_BUSY) {
                nimcp_mutex_unlock(&mutex);
                return;
            }
        }

        wait_succeeded = true;
        nimcp_mutex_unlock(&mutex);
    });

    // Wait for thread to start
    while (!thread_started) {
        sleep_ms(10);
    }
    sleep_ms(50);

    // Signal before timeout
    nimcp_mutex_lock(&mutex);
    predicate = true;
    nimcp_mutex_unlock(&mutex);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_cond_signal(&cond));

    waiter.join();
    EXPECT_TRUE(wait_succeeded);
}

/**
 * TEST: Timedwait with zero timeout (immediate return)
 * WHY: Verify zero timeout behaves correctly
 */
TEST_F(CondVarTest, TimedwaitZeroTimeout) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    nimcp_mutex_lock(&mutex);

    auto start = std::chrono::steady_clock::now();
    nimcp_result_t result = nimcp_cond_timedwait(&cond, &mutex, 0);
    auto end = std::chrono::steady_clock::now();

    nimcp_mutex_unlock(&mutex);

    // Should timeout immediately
    EXPECT_EQ(NIMCP_BUSY, result);

    // Should complete quickly (within 50ms)
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(elapsed_ms, 50);
}

//=============================================================================
// Predicate Pattern Tests (Spurious Wakeup Handling)
//=============================================================================

/**
 * TEST: Predicate pattern handles spurious wakeups
 * WHY: Verify correct condvar usage pattern
 */
TEST_F(CondVarTest, PredicatePatternHandlesSpuriousWakeup) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    std::atomic<int> predicate_value{0};
    std::atomic<bool> thread_done{false};
    std::atomic<bool> thread_started{false};

    // Thread waits for predicate_value == 2
    std::thread waiter([&]() {
        nimcp_mutex_lock(&mutex);
        thread_started = true;

        // Proper predicate loop pattern
        while (predicate_value != 2) {
            nimcp_cond_wait(&cond, &mutex);
        }

        thread_done = true;
        nimcp_mutex_unlock(&mutex);
    });

    // Wait for thread to start
    while (!thread_started) {
        sleep_ms(10);
    }
    sleep_ms(50);

    // First signal - predicate still false (simulates spurious wakeup handling)
    nimcp_mutex_lock(&mutex);
    predicate_value = 1;  // Not 2, so thread should go back to wait
    nimcp_mutex_unlock(&mutex);
    nimcp_cond_signal(&cond);

    sleep_ms(50);
    EXPECT_FALSE(thread_done);  // Thread should not be done

    // Second signal - predicate now true
    nimcp_mutex_lock(&mutex);
    predicate_value = 2;
    nimcp_mutex_unlock(&mutex);
    nimcp_cond_signal(&cond);

    waiter.join();
    EXPECT_TRUE(thread_done);
}

//=============================================================================
// Producer-Consumer Pattern Tests
//=============================================================================

/**
 * TEST: Producer-consumer with bounded buffer
 * WHY: Verify condvar coordinates producers and consumers
 */
TEST_F(CondVarTest, ProducerConsumerPattern) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int BUFFER_SIZE = 5;
    const int TOTAL_ITEMS = 20;

    std::queue<int> buffer;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> done{false};

    // We need a separate condvar for "not full" condition
    nimcp_cond_t not_full_cond;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_cond_init(&not_full_cond));

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < TOTAL_ITEMS; ++i) {
            nimcp_mutex_lock(&mutex);

            // Wait while buffer is full
            while (buffer.size() >= BUFFER_SIZE) {
                nimcp_cond_wait(&not_full_cond, &mutex);
            }

            buffer.push(i);
            produced++;

            nimcp_mutex_unlock(&mutex);
            nimcp_cond_signal(&cond);  // Signal consumer
        }

        // Signal done
        nimcp_mutex_lock(&mutex);
        done = true;
        nimcp_mutex_unlock(&mutex);
        nimcp_cond_broadcast(&cond);
    });

    // Consumer thread
    std::thread consumer([&]() {
        while (true) {
            nimcp_mutex_lock(&mutex);

            // Wait while buffer is empty and not done
            while (buffer.empty() && !done) {
                nimcp_cond_wait(&cond, &mutex);
            }

            if (buffer.empty() && done) {
                nimcp_mutex_unlock(&mutex);
                break;
            }

            buffer.pop();
            consumed++;

            nimcp_mutex_unlock(&mutex);
            nimcp_cond_signal(&not_full_cond);  // Signal producer
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(TOTAL_ITEMS, produced.load());
    EXPECT_EQ(TOTAL_ITEMS, consumed.load());
    EXPECT_TRUE(buffer.empty());

    nimcp_cond_destroy(&not_full_cond);
}

/**
 * TEST: Multiple producers and consumers
 * WHY: Verify condvar handles concurrent producers/consumers
 */
TEST_F(CondVarTest, MultipleProducersConsumers) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int BUFFER_SIZE = 10;
    const int ITEMS_PER_PRODUCER = 25;
    const int PRODUCER_COUNT = 2;
    const int CONSUMER_COUNT = 2;
    const int TOTAL_ITEMS = PRODUCER_COUNT * ITEMS_PER_PRODUCER;

    std::queue<int> buffer;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<int> active_producers{PRODUCER_COUNT};

    nimcp_cond_t not_full_cond;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_cond_init(&not_full_cond));

    // Producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < PRODUCER_COUNT; ++p) {
        producers.emplace_back([&]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                nimcp_mutex_lock(&mutex);

                while (buffer.size() >= BUFFER_SIZE) {
                    nimcp_cond_wait(&not_full_cond, &mutex);
                }

                buffer.push(i);
                produced++;

                nimcp_mutex_unlock(&mutex);
                nimcp_cond_signal(&cond);
            }

            // Mark producer as done
            active_producers--;
            nimcp_cond_broadcast(&cond);  // Wake consumers
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < CONSUMER_COUNT; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                nimcp_mutex_lock(&mutex);

                while (buffer.empty() && active_producers > 0) {
                    nimcp_cond_wait(&cond, &mutex);
                }

                if (buffer.empty() && active_producers == 0) {
                    nimcp_mutex_unlock(&mutex);
                    break;
                }

                if (!buffer.empty()) {
                    buffer.pop();
                    consumed++;
                }

                nimcp_mutex_unlock(&mutex);
                nimcp_cond_signal(&not_full_cond);
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(TOTAL_ITEMS, produced.load());
    EXPECT_EQ(TOTAL_ITEMS, consumed.load());

    nimcp_cond_destroy(&not_full_cond);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * TEST: High contention condvar stress test
 * WHY: Verify condvar handles heavy concurrent load
 */
TEST_F(CondVarTest, HighContentionStress) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int STRESS_THREADS = 8;
    const int CYCLES = 50;

    std::atomic<int> current_turn{0};
    std::atomic<int> total_completions{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < STRESS_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int c = 0; c < CYCLES; ++c) {
                nimcp_mutex_lock(&mutex);

                // Wait for our turn (t, t+STRESS_THREADS, etc.)
                int expected_turn = c * STRESS_THREADS + t;
                while (current_turn < expected_turn) {
                    nimcp_cond_wait(&cond, &mutex);
                }

                // Take our turn
                if (current_turn == expected_turn) {
                    current_turn++;
                    total_completions++;
                }

                nimcp_mutex_unlock(&mutex);
                nimcp_cond_broadcast(&cond);  // Wake all to check
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(STRESS_THREADS * CYCLES, total_completions.load());
}

/**
 * TEST: Rapid signal/wait cycles
 * WHY: Verify condvar handles rapid state changes
 */
TEST_F(CondVarTest, RapidSignalWaitCycles) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    const int CYCLES = 500;
    std::atomic<int> counter{0};
    std::atomic<bool> done{false};

    // Receiver thread
    std::thread receiver([&]() {
        for (int i = 0; i < CYCLES; ++i) {
            nimcp_mutex_lock(&mutex);

            while (counter <= i && !done) {
                nimcp_cond_wait(&cond, &mutex);
            }

            nimcp_mutex_unlock(&mutex);
        }
    });

    // Sender rapidly signals
    for (int i = 0; i < CYCLES; ++i) {
        nimcp_mutex_lock(&mutex);
        counter = i + 1;
        nimcp_mutex_unlock(&mutex);
        nimcp_cond_signal(&cond);
    }

    nimcp_mutex_lock(&mutex);
    done = true;
    nimcp_mutex_unlock(&mutex);
    nimcp_cond_broadcast(&cond);

    receiver.join();
    EXPECT_EQ(CYCLES, counter.load());
}

//=============================================================================
// Edge Cases
//=============================================================================

/**
 * TEST: Wait releases mutex during wait
 * WHY: Verify mutex is released while waiting (allowing signal)
 */
TEST_F(CondVarTest, WaitReleasesMutex) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    std::atomic<bool> mutex_acquired{false};
    std::atomic<bool> thread_waiting{false};
    bool predicate = false;

    std::thread waiter([&]() {
        nimcp_mutex_lock(&mutex);
        thread_waiting = true;

        while (!predicate) {
            nimcp_cond_wait(&cond, &mutex);
        }

        nimcp_mutex_unlock(&mutex);
    });

    // Wait for thread to start waiting
    while (!thread_waiting) {
        sleep_ms(10);
    }
    sleep_ms(50);

    // Try to acquire mutex - should succeed if wait released it
    nimcp_result_t result = nimcp_mutex_trylock(&mutex);
    if (result == NIMCP_SUCCESS) {
        mutex_acquired = true;
        predicate = true;
        nimcp_mutex_unlock(&mutex);
        nimcp_cond_signal(&cond);
    }

    waiter.join();
    EXPECT_TRUE(mutex_acquired);
}

/**
 * TEST: Multiple condvars with same mutex
 * WHY: Verify different condvars can share a mutex
 */
TEST_F(CondVarTest, MultipleCondvarsShareMutex) {
    ASSERT_TRUE(initialized_cond && initialized_mutex);

    nimcp_cond_t cond2;
    ASSERT_EQ(NIMCP_SUCCESS, nimcp_cond_init(&cond2));

    std::atomic<bool> thread1_woke{false};
    std::atomic<bool> thread2_woke{false};
    bool pred1 = false;
    bool pred2 = false;

    std::thread waiter1([&]() {
        nimcp_mutex_lock(&mutex);
        while (!pred1) {
            nimcp_cond_wait(&cond, &mutex);
        }
        thread1_woke = true;
        nimcp_mutex_unlock(&mutex);
    });

    std::thread waiter2([&]() {
        nimcp_mutex_lock(&mutex);
        while (!pred2) {
            nimcp_cond_wait(&cond2, &mutex);
        }
        thread2_woke = true;
        nimcp_mutex_unlock(&mutex);
    });

    sleep_ms(100);

    // Signal cond1 only - should wake only thread1
    nimcp_mutex_lock(&mutex);
    pred1 = true;
    nimcp_mutex_unlock(&mutex);
    nimcp_cond_signal(&cond);

    sleep_ms(50);
    EXPECT_TRUE(thread1_woke);
    EXPECT_FALSE(thread2_woke);

    // Now signal cond2
    nimcp_mutex_lock(&mutex);
    pred2 = true;
    nimcp_mutex_unlock(&mutex);
    nimcp_cond_signal(&cond2);

    waiter1.join();
    waiter2.join();

    EXPECT_TRUE(thread1_woke);
    EXPECT_TRUE(thread2_woke);

    nimcp_cond_destroy(&cond2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
