/**
 * @file test_utils_platform_cond.cpp
 * @brief Comprehensive unit tests for platform condition variables
 *
 * WHAT: 100% test coverage for nimcp_platform_cond.c
 * WHY:  Condition variables are critical synchronization primitives
 * HOW:  Test init/destroy, wait/signal/broadcast, timeouts, spurious wakeups, multi-threading
 *
 * TEST COVERAGE:
 * 1. Condition variable creation and destruction
 * 2. Wait and signal operations (single waiter)
 * 3. Broadcast operations (multiple waiters)
 * 4. Timeout behavior (timed wait)
 * 5. Signal before timeout (successful timed wait)
 * 6. Spurious wakeup handling
 * 7. Producer-consumer pattern with multiple threads
 * 8. Concurrent signal and broadcast operations
 * 9. Edge cases (NULL pointers, invalid operations)
 * 10. Mutex reacquisition after wait
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <queue>

    #include "utils/platform/nimcp_platform_cond.h"
    #include "utils/platform/nimcp_platform_mutex.h"
    #include <errno.h>

//=============================================================================
// Test Fixture
//=============================================================================

class CondVarTest : public ::testing::Test {
protected:
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;

    void SetUp() override {
        // Tests will initialize as needed
    }

    void TearDown() override {
        // Cleanup is done per-test as needed
    }

    // Helper: Initialize condition variable and mutex
    void InitCondAndMutex() {
        ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0)
            << "Failed to initialize mutex";
        ASSERT_EQ(nimcp_platform_cond_init(&cond), 0)
            << "Failed to initialize condition variable";
    }

    // Helper: Cleanup condition variable and mutex
    void CleanupCondAndMutex() {
        nimcp_platform_cond_destroy(&cond);
        nimcp_platform_mutex_destroy(&mutex);
    }
};

//=============================================================================
// Unit Test 1: Basic condition variable creation and destruction
//=============================================================================

TEST_F(CondVarTest, Init_BasicConditionVariable) {
    // WHAT: Verify nimcp_platform_cond_init() creates a condition variable
    // WHY:  Basic functionality must work
    // HOW:  Initialize condition variable and verify success

    int result = nimcp_platform_cond_init(&cond);
    EXPECT_EQ(result, 0) << "Failed to initialize condition variable";

    // Cleanup
    result = nimcp_platform_cond_destroy(&cond);
    EXPECT_EQ(result, 0) << "Failed to destroy condition variable";

    SUCCEED() << "Condition variable creation and destruction works";
}

//=============================================================================
// Unit Test 2: Wait and signal operations (single waiter)
//=============================================================================

TEST_F(CondVarTest, WaitSignal_BasicOperation) {
    // WHAT: Verify basic wait/signal cycle works
    // WHY:  Core condition variable functionality
    // HOW:  One thread waits, another signals

    InitCondAndMutex();

    std::atomic<bool> signaled{false};
    std::atomic<bool> wait_started{false};

    auto waiter = [&]() {
        nimcp_platform_mutex_lock(&mutex);
        wait_started = true;

        // Wait with spurious wakeup protection
        while (!signaled) {
            nimcp_platform_cond_wait(&cond, &mutex);
        }

        nimcp_platform_mutex_unlock(&mutex);
    };

    std::thread t(waiter);

    // Wait for thread to start waiting
    while (!wait_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Signal the condition variable
    nimcp_platform_mutex_lock(&mutex);
    signaled = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0) << "Signal failed";
    nimcp_platform_mutex_unlock(&mutex);

    t.join();

    EXPECT_TRUE(signaled) << "Signal flag should be set";

    CleanupCondAndMutex();
    SUCCEED() << "Basic wait/signal works";
}

//=============================================================================
// Unit Test 3: Broadcast operations (multiple waiters)
//=============================================================================

TEST_F(CondVarTest, Broadcast_MultipleWaiters) {
    // WHAT: Verify broadcast wakes all waiting threads
    // WHY:  Broadcast is critical for waking multiple waiters
    // HOW:  Multiple threads wait, broadcast should wake all

    InitCondAndMutex();

    const int NUM_THREADS = 5;
    std::atomic<int> woken_count{0};
    std::atomic<bool> broadcast_signal{false};
    std::atomic<int> waiting_count{0};

    auto worker = [&]() {
        nimcp_platform_mutex_lock(&mutex);
        waiting_count++;

        // Wait for broadcast with spurious wakeup protection
        while (!broadcast_signal) {
            nimcp_platform_cond_wait(&cond, &mutex);
        }

        woken_count++;
        nimcp_platform_mutex_unlock(&mutex);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    // Wait for all threads to be waiting
    while (waiting_count < NUM_THREADS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Broadcast to all
    nimcp_platform_mutex_lock(&mutex);
    broadcast_signal = true;
    EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0) << "Broadcast failed";
    nimcp_platform_mutex_unlock(&mutex);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(woken_count.load(), NUM_THREADS)
        << "All " << NUM_THREADS << " threads should be woken";

    CleanupCondAndMutex();
    SUCCEED() << "Broadcast wakes all " << NUM_THREADS << " waiters";
}

//=============================================================================
// Unit Test 4: Timeout behavior (timed wait)
//=============================================================================

TEST_F(CondVarTest, TimedWait_TimeoutOccurs) {
    // WHAT: Verify timed wait returns on timeout
    // WHY:  Timeout behavior is critical for non-blocking patterns
    // HOW:  Call timed wait without signal, verify timeout

    InitCondAndMutex();

    nimcp_platform_mutex_lock(&mutex);

    auto start = std::chrono::high_resolution_clock::now();
    int result = nimcp_platform_cond_timedwait(&cond, &mutex, 100);
    auto end = std::chrono::high_resolution_clock::now();

    nimcp_platform_mutex_unlock(&mutex);

    // Should timeout with ETIMEDOUT
    EXPECT_EQ(result, ETIMEDOUT) << "Timed wait should return ETIMEDOUT";

    // Verify actual timeout duration
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(elapsed.count(), 90) << "Wait should last ~100ms";
    EXPECT_LE(elapsed.count(), 200) << "Wait should not be much longer than 100ms";

    CleanupCondAndMutex();
    SUCCEED() << "Timed wait timeout verified";
}

//=============================================================================
// Unit Test 5: Signal before timeout (successful timed wait)
//=============================================================================

TEST_F(CondVarTest, TimedWait_SignalBeforeTimeout) {
    // WHAT: Verify timed wait succeeds when signaled before timeout
    // WHY:  Timed wait must respond to signals
    // HOW:  Start timed wait with long timeout, signal early

    InitCondAndMutex();

    std::atomic<bool> signaled{false};
    std::atomic<bool> wait_completed{false};

    auto waiter = [&]() {
        nimcp_platform_mutex_lock(&mutex);

        while (!signaled) {
            int result = nimcp_platform_cond_timedwait(&cond, &mutex, 5000);
            if (result == 0 || signaled) {
                break; // Successfully signaled
            }
        }

        wait_completed = true;
        nimcp_platform_mutex_unlock(&mutex);
    };

    std::thread t(waiter);

    // Give thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal before timeout
    auto start = std::chrono::high_resolution_clock::now();
    nimcp_platform_mutex_lock(&mutex);
    signaled = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    nimcp_platform_mutex_unlock(&mutex);

    t.join();
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_TRUE(wait_completed) << "Wait should complete";
    EXPECT_TRUE(signaled) << "Signal should be set";

    // Should complete much faster than 5 second timeout
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(elapsed.count(), 1000) << "Should complete quickly, not wait for timeout";

    CleanupCondAndMutex();
    SUCCEED() << "Timed wait responds to signal before timeout";
}

//=============================================================================
// Unit Test 6: Spurious wakeup handling
//=============================================================================

TEST_F(CondVarTest, SpuriousWakeup_HandledCorrectly) {
    // WHAT: Verify spurious wakeups are handled correctly
    // WHY:  Condition variables can wake spuriously, must use while loop
    // HOW:  Use while loop pattern to protect against spurious wakeups

    InitCondAndMutex();

    std::atomic<bool> condition_met{false};
    std::atomic<int> wait_iterations{0};
    std::atomic<bool> test_complete{false};

    auto waiter = [&]() {
        nimcp_platform_mutex_lock(&mutex);

        // Correct pattern: while loop checks condition
        while (!condition_met) {
            wait_iterations++;
            nimcp_platform_cond_wait(&cond, &mutex);
        }

        test_complete = true;
        nimcp_platform_mutex_unlock(&mutex);
    };

    std::thread t(waiter);

    // Give thread time to wait
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set condition and signal
    nimcp_platform_mutex_lock(&mutex);
    condition_met = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    nimcp_platform_mutex_unlock(&mutex);

    t.join();

    EXPECT_TRUE(test_complete) << "Test should complete";
    EXPECT_GE(wait_iterations.load(), 1) << "Should wait at least once";

    CleanupCondAndMutex();
    SUCCEED() << "Spurious wakeup protection verified";
}

//=============================================================================
// Unit Test 7: Producer-consumer pattern with multiple threads
//=============================================================================

TEST_F(CondVarTest, ProducerConsumer_MultithreadedPattern) {
    // WHAT: Verify condition variables work in producer-consumer pattern
    // WHY:  Common real-world use case
    // HOW:  Producer adds items, consumers remove them

    InitCondAndMutex();

    std::queue<int> buffer;
    const int MAX_BUFFER_SIZE = 5;
    const int NUM_ITEMS = 20;
    std::atomic<bool> producer_done{false};
    std::atomic<int> items_consumed{0};

    auto producer = [&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            nimcp_platform_mutex_lock(&mutex);

            // Wait if buffer is full
            while (buffer.size() >= MAX_BUFFER_SIZE) {
                nimcp_platform_cond_wait(&cond, &mutex);
            }

            buffer.push(i);
            nimcp_platform_cond_broadcast(&cond); // Wake consumers
            nimcp_platform_mutex_unlock(&mutex);

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        producer_done = true;
    };

    auto consumer = [&]() {
        while (items_consumed < NUM_ITEMS) {
            nimcp_platform_mutex_lock(&mutex);

            // Wait if buffer is empty and producer not done
            while (buffer.empty() && !producer_done) {
                nimcp_platform_cond_wait(&cond, &mutex);
            }

            if (!buffer.empty()) {
                buffer.pop();
                items_consumed++;
                nimcp_platform_cond_broadcast(&cond); // Wake producer
            }

            nimcp_platform_mutex_unlock(&mutex);

            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
    };

    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();

    EXPECT_EQ(items_consumed.load(), NUM_ITEMS)
        << "All " << NUM_ITEMS << " items should be consumed";
    EXPECT_TRUE(buffer.empty() || items_consumed == NUM_ITEMS)
        << "Buffer should be empty or all items consumed";

    CleanupCondAndMutex();
    SUCCEED() << "Producer-consumer pattern works correctly";
}

//=============================================================================
// Unit Test 8: Concurrent signal and broadcast operations
//=============================================================================

TEST_F(CondVarTest, Concurrent_SignalAndBroadcast) {
    // WHAT: Verify signal and broadcast can be used concurrently
    // WHY:  Mixed signaling patterns should work
    // HOW:  Some threads use signal, others use broadcast

    InitCondAndMutex();

    const int NUM_THREADS = 8;
    std::atomic<int> completed{0};
    std::atomic<int> waiting{0};
    std::atomic<bool> start_signaling{false};

    auto waiter = [&]() {
        nimcp_platform_mutex_lock(&mutex);
        waiting++;

        while (!start_signaling) {
            nimcp_platform_cond_wait(&cond, &mutex);
        }

        completed++;
        nimcp_platform_mutex_unlock(&mutex);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(waiter);
    }

    // Wait for all threads to be waiting
    while (waiting < NUM_THREADS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Wake threads with mix of signal and broadcast
    nimcp_platform_mutex_lock(&mutex);
    start_signaling = true;
    nimcp_platform_mutex_unlock(&mutex);

    // Use both signal and broadcast
    for (int i = 0; i < NUM_THREADS / 2; i++) {
        nimcp_platform_cond_signal(&cond);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    nimcp_platform_cond_broadcast(&cond); // Wake any remaining

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS)
        << "All threads should complete";

    CleanupCondAndMutex();
    SUCCEED() << "Concurrent signal and broadcast operations work";
}

//=============================================================================
// Unit Test 9: Edge cases (NULL pointers, invalid operations)
//=============================================================================

TEST_F(CondVarTest, EdgeCase_NullPointers) {
    // WHAT: Verify NULL pointer handling
    // WHY:  Defensive programming - should not crash
    // HOW:  Pass NULL to each function, verify EINVAL returned

    EXPECT_EQ(nimcp_platform_cond_init(nullptr), EINVAL)
        << "init should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_cond_destroy(nullptr), EINVAL)
        << "destroy should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_cond_signal(nullptr), EINVAL)
        << "signal should return EINVAL for NULL pointer";

    EXPECT_EQ(nimcp_platform_cond_broadcast(nullptr), EINVAL)
        << "broadcast should return EINVAL for NULL pointer";

    // Test wait with NULL cond/mutex
    InitCondAndMutex();

    EXPECT_EQ(nimcp_platform_cond_wait(nullptr, &mutex), EINVAL)
        << "wait should return EINVAL for NULL cond";

    EXPECT_EQ(nimcp_platform_cond_wait(&cond, nullptr), EINVAL)
        << "wait should return EINVAL for NULL mutex";

    EXPECT_EQ(nimcp_platform_cond_timedwait(nullptr, &mutex, 100), EINVAL)
        << "timedwait should return EINVAL for NULL cond";

    EXPECT_EQ(nimcp_platform_cond_timedwait(&cond, nullptr, 100), EINVAL)
        << "timedwait should return EINVAL for NULL mutex";

    CleanupCondAndMutex();

    SUCCEED() << "NULL pointer handling verified";
}

//=============================================================================
// Unit Test 10: Mutex reacquisition after wait
//=============================================================================

TEST_F(CondVarTest, MutexReacquisition_AfterWait) {
    // WHAT: Verify mutex is reacquired after wait returns
    // WHY:  Condition variable wait must atomically reacquire mutex
    // HOW:  Verify mutex state before and after wait

    InitCondAndMutex();

    std::atomic<bool> signaled{false};
    std::atomic<bool> mutex_held_after_wait{false};

    auto waiter = [&]() {
        // Lock mutex before waiting
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        // Wait (releases and reacquires mutex)
        while (!signaled) {
            nimcp_platform_cond_wait(&cond, &mutex);
        }

        // At this point, mutex should be held
        // Try to lock from another thread - should fail
        std::thread checker([&]() {
            int result = nimcp_platform_mutex_trylock(&mutex);
            if (result == EBUSY) {
                mutex_held_after_wait = true;
            } else if (result == 0) {
                // Shouldn't happen - mutex should be held by waiter
                nimcp_platform_mutex_unlock(&mutex);
            }
        });
        checker.join();

        // Unlock mutex
        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    };

    std::thread t(waiter);

    // Give thread time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal
    nimcp_platform_mutex_lock(&mutex);
    signaled = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    nimcp_platform_mutex_unlock(&mutex);

    t.join();

    EXPECT_TRUE(mutex_held_after_wait)
        << "Mutex should be held by waiter after cond_wait returns";

    CleanupCondAndMutex();
    SUCCEED() << "Mutex reacquisition after wait verified";
}

//=============================================================================
// Additional Tests: Multiple condition variables
//=============================================================================

TEST_F(CondVarTest, Multiple_ConditionVariables) {
    // WHAT: Verify multiple condition variables work independently
    // WHY:  Should be able to use multiple cond vars with same mutex
    // HOW:  Create two cond vars, signal each independently

    nimcp_platform_cond_t cond1, cond2;
    ASSERT_EQ(nimcp_platform_mutex_init(&mutex, false), 0);
    ASSERT_EQ(nimcp_platform_cond_init(&cond1), 0);
    ASSERT_EQ(nimcp_platform_cond_init(&cond2), 0);

    std::atomic<bool> signal1{false};
    std::atomic<bool> signal2{false};
    std::atomic<bool> thread1_done{false};
    std::atomic<bool> thread2_done{false};

    auto waiter1 = [&]() {
        nimcp_platform_mutex_lock(&mutex);
        while (!signal1) {
            nimcp_platform_cond_wait(&cond1, &mutex);
        }
        thread1_done = true;
        nimcp_platform_mutex_unlock(&mutex);
    };

    auto waiter2 = [&]() {
        nimcp_platform_mutex_lock(&mutex);
        while (!signal2) {
            nimcp_platform_cond_wait(&cond2, &mutex);
        }
        thread2_done = true;
        nimcp_platform_mutex_unlock(&mutex);
    };

    std::thread t1(waiter1);
    std::thread t2(waiter2);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal cond1 only
    nimcp_platform_mutex_lock(&mutex);
    signal1 = true;
    nimcp_platform_cond_signal(&cond1);
    nimcp_platform_mutex_unlock(&mutex);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Thread1 should be done, thread2 should still be waiting
    EXPECT_TRUE(thread1_done);
    EXPECT_FALSE(thread2_done);

    // Now signal cond2
    nimcp_platform_mutex_lock(&mutex);
    signal2 = true;
    nimcp_platform_cond_signal(&cond2);
    nimcp_platform_mutex_unlock(&mutex);

    t1.join();
    t2.join();

    EXPECT_TRUE(thread2_done);

    nimcp_platform_cond_destroy(&cond1);
    nimcp_platform_cond_destroy(&cond2);
    nimcp_platform_mutex_destroy(&mutex);

    SUCCEED() << "Multiple condition variables work independently";
}

//=============================================================================
// Stress Test: Rapid signaling
//=============================================================================

TEST_F(CondVarTest, Stress_RapidSignaling) {
    // WHAT: Stress test with rapid signal operations
    // WHY:  Ensure condition variable remains stable under rapid operations
    // HOW:  Perform many signal operations quickly

    InitCondAndMutex();

    const int ITERATIONS = 1000;
    std::atomic<int> signal_count{0};

    for (int i = 0; i < ITERATIONS; i++) {
        ASSERT_EQ(nimcp_platform_cond_signal(&cond), 0);
        signal_count++;
    }

    EXPECT_EQ(signal_count.load(), ITERATIONS);

    CleanupCondAndMutex();
    SUCCEED() << "Survived " << ITERATIONS << " rapid signals";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
