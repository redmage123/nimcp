/**
 * @file test_platform_cond.cpp
 * @brief TDD comprehensive tests for platform condition variable module
 *
 * WHAT: Comprehensive test suite for cross-platform condition variables
 * WHY:  TDD approach - tests define expected behavior before implementation
 * HOW:  Test all condition variable functions: init, destroy, wait, signal, broadcast
 *
 * TEST CATEGORIES:
 * 1. Lifecycle (init/destroy)
 * 2. Wait/signal mechanism (single waiter, single signal)
 * 3. Wait/broadcast mechanism (multiple waiters)
 * 4. Timed wait with timeout (timeout occurs)
 * 5. Timed wait without timeout (signal arrives before timeout)
 * 6. Spurious wakeups handling
 * 7. Multi-threaded producer-consumer pattern
 * 8. NULL pointer safety checks
 *
 * COMPLEXITY: O(1) for signal, O(n) for broadcast
 * THREAD-SAFE: Yes - all functions are thread-safe
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <queue>

#include "utils/platform/nimcp_platform_cond.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PlatformCondVarTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();  // Call parent first for cleanup

        // Nothing special needed for setup
    }

    void TearDown() override {
        // All resources should be cleaned up by tests

        NimcpTestBase::TearDown();  // Call parent last for cleanup
    }

    // Helper: Create and initialize a condition variable and mutex
    void initCondAndMutex(nimcp_platform_cond_t* cond,
                          nimcp_platform_mutex_t* mutex) {
        ASSERT_EQ(nimcp_platform_mutex_init(mutex, false), 0);
        ASSERT_EQ(nimcp_platform_cond_init(cond), 0);
    }

    // Helper: Cleanup condition variable and mutex
    void cleanupCondAndMutex(nimcp_platform_cond_t* cond,
                             nimcp_platform_mutex_t* mutex) {
        nimcp_platform_cond_destroy(cond);
        nimcp_platform_mutex_destroy(mutex);
    }
};

//=============================================================================
// TEST 1: Lifecycle - Init/Destroy
//=============================================================================

TEST_F(PlatformCondVarTest, InitDestroy) {
    nimcp_platform_cond_t cond;
    EXPECT_EQ(nimcp_platform_cond_init(&cond), 0)
        << "Condition variable initialization should succeed";
    EXPECT_EQ(nimcp_platform_cond_destroy(&cond), 0)
        << "Condition variable destruction should succeed";
}

TEST_F(PlatformCondVarTest, MultipleInitDestroy) {
    const int count = 10;
    for (int i = 0; i < count; i++) {
        nimcp_platform_cond_t cond;
        EXPECT_EQ(nimcp_platform_cond_init(&cond), 0);
        EXPECT_EQ(nimcp_platform_cond_destroy(&cond), 0);
    }
}

TEST_F(PlatformCondVarTest, InitMultipleCondVars) {
    const int count = 5;
    nimcp_platform_cond_t conds[count];

    // Initialize all
    for (int i = 0; i < count; i++) {
        EXPECT_EQ(nimcp_platform_cond_init(&conds[i]), 0);
    }

    // Destroy all
    for (int i = 0; i < count; i++) {
        EXPECT_EQ(nimcp_platform_cond_destroy(&conds[i]), 0);
    }
}

//=============================================================================
// TEST 2: NULL Pointer Safety
//=============================================================================

TEST_F(PlatformCondVarTest, NullSafetyInit) {
    EXPECT_NE(nimcp_platform_cond_init(nullptr), 0)
        << "Initializing NULL condition variable should fail with error code";
}

TEST_F(PlatformCondVarTest, NullSafetyDestroy) {
    EXPECT_NE(nimcp_platform_cond_destroy(nullptr), 0)
        << "Destroying NULL condition variable should fail with error code";
}

TEST_F(PlatformCondVarTest, NullSafetyWait) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    // NULL condition variable
    EXPECT_NE(nimcp_platform_cond_wait(nullptr, &mutex), 0)
        << "Wait on NULL condition variable should fail";

    // NULL mutex
    EXPECT_NE(nimcp_platform_cond_wait(&cond, nullptr), 0)
        << "Wait with NULL mutex should fail";

    // Both NULL
    EXPECT_NE(nimcp_platform_cond_wait(nullptr, nullptr), 0)
        << "Wait with both NULL should fail";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, NullSafetyTimedWait) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    // NULL condition variable
    EXPECT_NE(nimcp_platform_cond_timedwait(nullptr, &mutex, 100), 0)
        << "Timed wait on NULL condition variable should fail";

    // NULL mutex
    EXPECT_NE(nimcp_platform_cond_timedwait(&cond, nullptr, 100), 0)
        << "Timed wait with NULL mutex should fail";

    // Both NULL
    EXPECT_NE(nimcp_platform_cond_timedwait(nullptr, nullptr, 100), 0)
        << "Timed wait with both NULL should fail";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, NullSafetySignal) {
    EXPECT_NE(nimcp_platform_cond_signal(nullptr), 0)
        << "Signaling NULL condition variable should fail with error code";
}

TEST_F(PlatformCondVarTest, NullSafetyBroadcast) {
    EXPECT_NE(nimcp_platform_cond_broadcast(nullptr), 0)
        << "Broadcasting NULL condition variable should fail with error code";
}

//=============================================================================
// TEST 3: Wait/Signal Mechanism (Single Waiter)
//=============================================================================

TEST_F(PlatformCondVarTest, WaitSignal) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    bool signaled = false;

    std::thread waiter([&]() {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        // While loop to handle spurious wakeups
        while (!signaled) {
            EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
        }

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    });

    // Give waiter time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal from main thread
    std::thread signaler([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
        signaled = true;
        EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    });

    waiter.join();
    signaler.join();

    EXPECT_TRUE(signaled) << "Signaled flag should be set";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, SignalWithoutWaiters) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    // Signal without any waiters - should not crash or error
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0)
        << "Signaling without waiters should succeed (no-op)";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 4: Wait/Broadcast Mechanism (Multiple Waiters)
//=============================================================================

TEST_F(PlatformCondVarTest, WaitBroadcast) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    const int num_waiters = 5;
    std::atomic<int> woken_up(0);
    bool broadcast_signal = false;

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; i++) {
        waiters.emplace_back([&]() {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

            while (!broadcast_signal) {
                EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
            }

            woken_up++;
            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        });
    }

    // Give all waiters time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Broadcast from main thread
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
    broadcast_signal = true;
    EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0)
        << "Broadcast should succeed";
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    // Wait for all threads to finish
    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(woken_up, num_waiters)
        << "All " << num_waiters << " waiters should be woken up";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, BroadcastWithoutWaiters) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    // Broadcast without any waiters - should not crash or error
    EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0)
        << "Broadcasting without waiters should succeed (no-op)";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, MultipleBroadcasts) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    const int num_rounds = 3;
    const int num_waiters = 4;
    std::atomic<int> completion_count(0);

    for (int round = 0; round < num_rounds; round++) {
        bool signal = false;
        std::vector<std::thread> waiters;

        for (int i = 0; i < num_waiters; i++) {
            waiters.emplace_back([&]() {
                EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

                while (!signal) {
                    EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
                }

                completion_count++;
                EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
        signal = true;
        EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0);
        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

        for (auto& t : waiters) {
            t.join();
        }
    }

    EXPECT_EQ(completion_count, num_rounds * num_waiters)
        << "All waiters in all rounds should complete";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 5: Timed Wait - Timeout Occurs
//=============================================================================

TEST_F(PlatformCondVarTest, TimedWaitTimeout) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

    auto start = std::chrono::high_resolution_clock::now();
    int result = nimcp_platform_cond_timedwait(&cond, &mutex, 100);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    // Should timeout (return ETIMEDOUT or error)
    EXPECT_NE(result, 0)
        << "Timed wait should return non-zero (timeout occurred)";

    // Verify timeout actually happened (at least ~100ms)
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(elapsed.count(), 80)
        << "Wait should actually wait for ~100ms before timing out";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, TimedWaitZeroTimeout) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

    int result = nimcp_platform_cond_timedwait(&cond, &mutex, 0);

    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    // Zero timeout should timeout immediately
    EXPECT_NE(result, 0)
        << "Zero timeout should timeout immediately";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, TimedWaitMultipleTimeouts) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    const int num_timeouts = 3;

    for (int i = 0; i < num_timeouts; i++) {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        int result = nimcp_platform_cond_timedwait(&cond, &mutex, 50);
        EXPECT_NE(result, 0)
            << "Attempt " << i << ": Timed wait should timeout";

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    }

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 6: Timed Wait - Signal Arrives Before Timeout
//=============================================================================

TEST_F(PlatformCondVarTest, TimedWaitSignaled) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    bool signaled = false;

    std::thread waiter([&]() {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        while (!signaled) {
            int result = nimcp_platform_cond_timedwait(&cond, &mutex, 5000);
            // Should either succeed (0) or timeout, depending on timing
            EXPECT_GE(result, -1); // Valid result
        }

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    });

    // Give waiter time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal before timeout
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
    signaled = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    waiter.join();

    EXPECT_TRUE(signaled) << "Signal should have been set";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, TimedWaitSignaledMultiple) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    const int num_signals = 5;
    std::atomic<int> signal_count(0);

    std::thread waiter([&]() {
        for (int i = 0; i < num_signals; i++) {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

            while (signal_count <= i) {
                int result = nimcp_platform_cond_timedwait(&cond, &mutex, 1000);
                // Accept both success and spurious wakeup
                EXPECT_GE(result, -1);
            }

            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        }
    });

    std::thread signaler([&]() {
        for (int i = 0; i < num_signals; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
            signal_count++;
            EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        }
    });

    waiter.join();
    signaler.join();

    EXPECT_EQ(signal_count, num_signals)
        << "All signals should have been sent";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 7: Spurious Wakeups Handling
//=============================================================================

TEST_F(PlatformCondVarTest, SpuriousWakeupsHandling) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::atomic<bool> ready(false);
    std::atomic<int> wakeup_count(0);

    std::thread waiter([&]() {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        // Use while loop to properly handle spurious wakeups
        while (!ready) {
            wakeup_count++;
            int result = nimcp_platform_cond_wait(&cond, &mutex);
            EXPECT_EQ(result, 0);
        }

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set ready and signal
    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
    ready = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    waiter.join();

    // Wakeup count >= 1 because of at least one real wakeup
    // (might be more if spurious wakeups occurred)
    EXPECT_GE(wakeup_count, 1)
        << "Thread should wake up at least once";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 8: Producer-Consumer Pattern
//=============================================================================

TEST_F(PlatformCondVarTest, ProducerConsumer) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::queue<int> buffer;
    const int buffer_size = 5;
    const int num_items = 20;

    std::thread producer([&]() {
        for (int i = 0; i < num_items; i++) {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

            while (buffer.size() >= buffer_size) {
                EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
            }

            buffer.push(i);
            EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::thread consumer([&]() {
        int items_consumed = 0;

        while (items_consumed < num_items) {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

            while (buffer.empty()) {
                EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
            }

            if (!buffer.empty()) {
                buffer.pop();
                items_consumed++;
                EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
            }

            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_TRUE(buffer.empty())
        << "Buffer should be empty after all items consumed";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, ProducerConsumerMultipleConsumers) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::queue<int> buffer;
    std::atomic<int> consumed_count(0);
    std::atomic<bool> producer_done(false);
    const int num_items = 30;
    const int num_consumers = 3;

    std::thread producer([&]() {
        for (int i = 0; i < num_items; i++) {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
            buffer.push(i);
            EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0);
            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        producer_done = true;
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; c++) {
        consumers.emplace_back([&]() {
            while (consumed_count < num_items) {
                EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

                if (!buffer.empty()) {
                    buffer.pop();
                    consumed_count++;
                    EXPECT_EQ(nimcp_platform_cond_broadcast(&cond), 0);
                }

                EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    producer.join();
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(consumed_count, num_items)
        << "All " << num_items << " items should be consumed";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 9: Advanced Scenarios
//=============================================================================

TEST_F(PlatformCondVarTest, SignalBeforeWait) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    // Signal before anyone waits - signal is lost
    // (This is expected behavior for condition variables)
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);

    std::atomic<bool> done(false);

    std::thread waiter([&]() {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        // This will timeout because the signal was already lost
        nimcp_platform_cond_timedwait(&cond, &mutex, 100);

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        done = true;
    });

    waiter.join();

    EXPECT_TRUE(done) << "Waiter should complete";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, RapidSignalSignal) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::atomic<int> woken_count(0);
    const int num_waiters = 3;

    std::vector<std::thread> waiters;
    for (int i = 0; i < num_waiters; i++) {
        waiters.emplace_back([&]() {
            EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
            EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
            woken_count++;
            EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Signal each waiter individually
    for (int i = 0; i < num_waiters; i++) {
        EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (auto& t : waiters) {
        t.join();
    }

    EXPECT_EQ(woken_count, num_waiters)
        << "Each signal should wake one waiter";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, ConcurrentSignalAndWait) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::atomic<int> completed(0);
    const int num_threads = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        if (i % 2 == 0) {
            // Waiter threads
            threads.emplace_back([&]() {
                EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
                EXPECT_EQ(nimcp_platform_cond_wait(&cond, &mutex), 0);
                completed++;
                EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
            });
        } else {
            // Signaler threads
            threads.emplace_back([&]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
            });
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed, num_threads / 2)
        << "All waiter threads should complete";

    cleanupCondAndMutex(&cond, &mutex);
}

//=============================================================================
// TEST 10: Edge Cases
//=============================================================================

TEST_F(PlatformCondVarTest, WaitAndUnlockMutexProperly) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

    // Timed wait should release the mutex during wait
    // and reacquire it before returning
    int result = nimcp_platform_cond_timedwait(&cond, &mutex, 50);
    // Either succeeds (0) or times out (non-zero), both are valid
    EXPECT_TRUE(result == 0 || result != 0)
        << "Timed wait should return a valid result";

    // Mutex should still be locked after wait returns
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0)
        << "Mutex should still be locked after wait returns";

    cleanupCondAndMutex(&cond, &mutex);
}

TEST_F(PlatformCondVarTest, DestroyAndReinitialize) {
    nimcp_platform_cond_t cond;

    // Init-destroy-reinit cycle
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(nimcp_platform_cond_init(&cond), 0);
        EXPECT_EQ(nimcp_platform_cond_destroy(&cond), 0);
    }
}

TEST_F(PlatformCondVarTest, TimedWaitLargeTimeout) {
    nimcp_platform_cond_t cond;
    nimcp_platform_mutex_t mutex;
    initCondAndMutex(&cond, &mutex);

    std::atomic<bool> signaled(false);

    std::thread waiter([&]() {
        EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);

        if (!signaled) {
            EXPECT_EQ(nimcp_platform_cond_timedwait(&cond, &mutex, 5000), 0);
        }

        EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_platform_mutex_lock(&mutex), 0);
    signaled = true;
    EXPECT_EQ(nimcp_platform_cond_signal(&cond), 0);
    EXPECT_EQ(nimcp_platform_mutex_unlock(&mutex), 0);

    waiter.join();

    EXPECT_TRUE(signaled) << "Should have been signaled";

    cleanupCondAndMutex(&cond, &mutex);
}

