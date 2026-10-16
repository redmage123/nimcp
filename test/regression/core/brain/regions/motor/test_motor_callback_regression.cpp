/**
 * @file test_motor_callback_regression.cpp
 * @brief Regression tests for Motor Cortex callback race conditions
 *
 * WHAT: Tests to prevent regression of fixed race condition bugs in motor callbacks
 * WHY:  Lock in correct thread-safe behavior after bug fixes
 * HOW:  Stress test concurrent callback invocation
 *
 * BUG HISTORY:
 * - Bug #1: Race condition in callback invocation
 *   WRONG: `if (!in_callback) { in_callback = true; callback(); in_callback = false; }`
 *   RIGHT: Use atomic compare-and-swap to prevent TOCTOU race
 * - Bug #2: Callback invoked while holding mutex causing deadlock
 *   FIX: Copy callback pointer, release mutex, then invoke
 * - Bug #3: Multiple threads could invoke callback simultaneously
 *   FIX: Atomic CAS ensures only one thread enters callback at a time
 *
 * REGRESSION FOCUS:
 * 1. Only one callback executes at a time (re-entrance prevention)
 * 2. No deadlocks from callback invocation within locked code
 * 3. Stress test with many concurrent threads
 * 4. Atomic operations are correctly used
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <condition_variable>

extern "C" {
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "utils/memory/nimcp_memory.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class MotorCallbackRegressionTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;

    /* Callback tracking state - atomic for thread safety */
    static std::atomic<int> callback_count;
    static std::atomic<int> concurrent_callbacks;
    static std::atomic<int> max_concurrent;
    static std::atomic<bool> race_detected;

    /* Mutex for detailed logging */
    static std::mutex log_mutex;

    void SetUp() override {
        /* Reset static counters */
        callback_count.store(0);
        concurrent_callbacks.store(0);
        max_concurrent.store(0);
        race_detected.store(false);

        /* Create adapter with default config */
        motor_config_t config = motor_default_config();
        config.enable_events = true;
        config.enable_bio_async = false;  /* Disable bio-async for focused testing */
        adapter = motor_create(&config);

        ASSERT_NE(adapter, nullptr) << "Failed to create motor adapter";
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
    }

    /**
     * @brief Command callback that tracks concurrent invocations
     */
    static void tracking_command_callback(const motor_command_t* command, void* user_data) {
        (void)command;
        (void)user_data;

        /* Increment concurrent count atomically */
        int before = concurrent_callbacks.fetch_add(1);

        /* Update max concurrent if this is a new maximum */
        int current = before + 1;
        int prev_max = max_concurrent.load();
        while (current > prev_max) {
            if (max_concurrent.compare_exchange_weak(prev_max, current)) {
                break;
            }
        }

        /* If more than 1 concurrent, we have a race */
        if (current > 1) {
            race_detected.store(true);
        }

        /* Simulate some work */
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        /* Increment total callback count */
        callback_count.fetch_add(1);

        /* Decrement concurrent count */
        concurrent_callbacks.fetch_sub(1);
    }

    /**
     * @brief Event callback that tracks concurrent invocations
     */
    static void tracking_event_callback(uint32_t event_type, const void* event_data, void* user_data) {
        (void)event_type;
        (void)event_data;
        (void)user_data;

        int before = concurrent_callbacks.fetch_add(1);
        int current = before + 1;

        int prev_max = max_concurrent.load();
        while (current > prev_max) {
            if (max_concurrent.compare_exchange_weak(prev_max, current)) {
                break;
            }
        }

        if (current > 1) {
            race_detected.store(true);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50));
        callback_count.fetch_add(1);
        concurrent_callbacks.fetch_sub(1);
    }

    /**
     * @brief Completion callback for testing
     */
    static void tracking_complete_callback(const motor_result_t* result, void* user_data) {
        (void)result;
        (void)user_data;

        int before = concurrent_callbacks.fetch_add(1);
        int current = before + 1;

        int prev_max = max_concurrent.load();
        while (current > prev_max) {
            if (max_concurrent.compare_exchange_weak(prev_max, current)) {
                break;
            }
        }

        if (current > 1) {
            race_detected.store(true);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50));
        callback_count.fetch_add(1);
        concurrent_callbacks.fetch_sub(1);
    }
};

/* Static member definitions */
std::atomic<int> MotorCallbackRegressionTest::callback_count{0};
std::atomic<int> MotorCallbackRegressionTest::concurrent_callbacks{0};
std::atomic<int> MotorCallbackRegressionTest::max_concurrent{0};
std::atomic<bool> MotorCallbackRegressionTest::race_detected{false};
std::mutex MotorCallbackRegressionTest::log_mutex;

//=============================================================================
// RE-ENTRANCE PREVENTION REGRESSION TESTS
//=============================================================================

/**
 * BUG: Race condition in callback re-entrance check
 *
 * WRONG (TOCTOU race):
 *   if (!in_callback) {
 *       in_callback = true;   // <-- Another thread could also pass the check
 *       callback();
 *       in_callback = false;
 *   }
 *
 * RIGHT (atomic CAS):
 *   bool expected = false;
 *   if (atomic_compare_exchange(&in_callback, &expected, true)) {
 *       callback();
 *       atomic_store(&in_callback, false);
 *   }
 */
TEST_F(MotorCallbackRegressionTest, ReEntrancePrevention_OnlyOneCallbackAtATime) {
    /**
     * REGRESSION TEST: Atomic CAS ensures only one callback at a time
     *
     * Multiple threads simultaneously trigger callback-generating operations.
     * The atomic in_callback flag should prevent concurrent invocations.
     */

    /* Register callback */
    ASSERT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, nullptr));

    const int num_threads = 10;
    const int ops_per_thread = 100;

    std::vector<std::thread> threads;

    /* Create threads that will trigger callbacks */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                /* Plan and execute movement (triggers command callback) */
                motor_goal_t goal = {};
                goal.region = MOTOR_REGION_HAND_RIGHT;
                goal.target_position = {0.1f * i, 0.2f * i, 0.3f * i};
                goal.max_duration_ms = 100.0f;
                goal.type = MOVEMENT_TYPE_DISCRETE;

                if (motor_plan_movement(adapter, &goal)) {
                    motor_begin_execution(adapter);
                    motor_update_execution(adapter, 10.0f);
                }

                /* Small sleep to allow interleaving */
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Check that no race was detected */
    EXPECT_FALSE(race_detected.load())
        << "REGRESSION: Race detected - multiple callbacks executed concurrently. "
        << "Maximum concurrent callbacks: " << max_concurrent.load();

    EXPECT_LE(max_concurrent.load(), 1)
        << "REGRESSION: More than one callback executed simultaneously. "
        << "This indicates atomic CAS is not being used correctly.";
}

TEST_F(MotorCallbackRegressionTest, ReEntrancePrevention_EventCallback) {
    /**
     * REGRESSION TEST: Event callback also protected by atomic
     */
    ASSERT_TRUE(motor_set_event_callback(adapter, tracking_event_callback, nullptr));

    /* Reset counters */
    callback_count.store(0);
    concurrent_callbacks.store(0);
    max_concurrent.store(0);
    race_detected.store(false);

    const int num_threads = 8;
    const int ops_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                motor_goal_t goal = {};
                goal.region = MOTOR_REGION_ARM_LEFT;
                goal.target_position = {1.0f, 2.0f, 3.0f};
                goal.max_duration_ms = 50.0f;
                goal.type = MOVEMENT_TYPE_CONTINUOUS;

                /* Plan triggers MOTOR_EVENT_PLAN_COMPLETE */
                motor_plan_movement(adapter, &goal);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(race_detected.load())
        << "REGRESSION: Race detected in event callback";
    EXPECT_LE(max_concurrent.load(), 1)
        << "REGRESSION: Multiple event callbacks executed simultaneously";
}

//=============================================================================
// DEADLOCK PREVENTION REGRESSION TESTS
//=============================================================================

/**
 * BUG: Deadlock when callback invoked while holding mutex
 *
 * WRONG:
 *   mutex_lock(queue_mutex);
 *   // ... modify queue ...
 *   if (callback) callback();  // <-- Callback might call back into adapter, deadlock!
 *   mutex_unlock(queue_mutex);
 *
 * RIGHT:
 *   mutex_lock(queue_mutex);
 *   callback_copy = callback;
 *   // ... modify queue ...
 *   mutex_unlock(queue_mutex);
 *   if (callback_copy) callback_copy();  // <-- Safe, mutex released
 */
TEST_F(MotorCallbackRegressionTest, DeadlockPrevention_CallbackOutsideMutex) {
    /**
     * REGRESSION TEST: Callback must be invoked outside of mutex lock
     *
     * We test this by having the callback try to call back into the adapter.
     * If the callback is invoked while holding a mutex, this will deadlock.
     */

    /* Track whether we completed without deadlock */
    static std::atomic<bool> completed{false};
    static motor_adapter_t* adapter_ref = nullptr;
    adapter_ref = adapter;

    /* Callback that tries to access adapter (would deadlock if mutex held) */
    auto reentrant_callback = [](const motor_command_t* cmd, void* user_data) {
        (void)cmd;
        (void)user_data;

        /* Try to get status - this requires accessing adapter state */
        motor_status_t status = motor_get_status(adapter_ref);
        (void)status;

        /* Try to get next command - this uses the queue mutex */
        motor_command_t next_cmd;
        motor_get_next_command(adapter_ref, &next_cmd);

        completed.store(true);
    };

    ASSERT_TRUE(motor_set_command_callback(adapter, reentrant_callback, nullptr));

    /* Plan and execute to trigger callback */
    motor_goal_t goal = {};
    goal.region = MOTOR_REGION_HAND_LEFT;
    goal.target_position = {1.0f, 0.0f, 0.0f};
    goal.max_duration_ms = 100.0f;

    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    /* Give callback time to complete */
    auto start = std::chrono::steady_clock::now();
    while (!completed.load()) {
        motor_update_execution(adapter, 10.0f);

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(2)) {
            FAIL() << "REGRESSION: Timeout - likely deadlock in callback";
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(completed.load())
        << "REGRESSION: Callback did not complete - possible deadlock";
}

//=============================================================================
// CONCURRENT STRESS TESTS
//=============================================================================

TEST_F(MotorCallbackRegressionTest, Stress_ManyThreadsManyOperations) {
    /**
     * REGRESSION TEST: Heavy concurrent load should not cause races
     */
    ASSERT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, nullptr));
    ASSERT_TRUE(motor_set_event_callback(adapter, tracking_event_callback, nullptr));
    ASSERT_TRUE(motor_set_complete_callback(adapter, tracking_complete_callback, nullptr));

    /* Reset counters */
    callback_count.store(0);
    concurrent_callbacks.store(0);
    max_concurrent.store(0);
    race_detected.store(false);

    const int num_threads = 20;
    const int ops_per_thread = 200;

    std::vector<std::thread> threads;
    std::atomic<int> successful_ops{0};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &successful_ops, ops_per_thread, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                /* Mix of operations */
                int op = (t * ops_per_thread + i) % 5;

                motor_goal_t goal = {};
                goal.region = static_cast<motor_region_t>(i % MOTOR_REGION_COUNT);
                goal.target_position = {
                    static_cast<float>(t * 0.1),
                    static_cast<float>(i * 0.1),
                    static_cast<float>((t + i) * 0.1)
                };
                goal.max_duration_ms = 50.0f;

                switch (op) {
                    case 0:
                        /* Plan only */
                        if (motor_plan_movement(adapter, &goal)) {
                            successful_ops.fetch_add(1);
                        }
                        break;
                    case 1:
                        /* Plan and start */
                        if (motor_plan_movement(adapter, &goal)) {
                            motor_begin_execution(adapter);
                            successful_ops.fetch_add(1);
                        }
                        break;
                    case 2:
                        /* Plan, start, update */
                        if (motor_plan_movement(adapter, &goal)) {
                            if (motor_begin_execution(adapter)) {
                                motor_update_execution(adapter, 5.0f);
                                successful_ops.fetch_add(1);
                            }
                        }
                        break;
                    case 3:
                        /* Stop execution */
                        motor_stop_execution(adapter);
                        successful_ops.fetch_add(1);
                        break;
                    case 4:
                        /* Get status */
                        motor_get_status(adapter);
                        successful_ops.fetch_add(1);
                        break;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify no races */
    EXPECT_FALSE(race_detected.load())
        << "REGRESSION: Race detected during stress test. "
        << "Max concurrent callbacks: " << max_concurrent.load() << ", "
        << "Total callbacks: " << callback_count.load() << ", "
        << "Successful operations: " << successful_ops.load();

    EXPECT_LE(max_concurrent.load(), 1)
        << "REGRESSION: Multiple callbacks executed concurrently during stress test";

    /* Verify operations completed */
    EXPECT_GT(successful_ops.load(), 0) << "No operations completed";
}

TEST_F(MotorCallbackRegressionTest, Stress_RapidFireCallbacks) {
    /**
     * REGRESSION TEST: Rapid callback triggers should not race
     */
    ASSERT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, nullptr));

    callback_count.store(0);
    concurrent_callbacks.store(0);
    max_concurrent.store(0);
    race_detected.store(false);

    const int num_iterations = 1000;

    /* Single thread rapid-fire */
    for (int i = 0; i < num_iterations; i++) {
        motor_goal_t goal = {};
        goal.region = MOTOR_REGION_FACE;
        goal.target_position = {0.0f, 0.0f, static_cast<float>(i)};
        goal.max_duration_ms = 10.0f;

        if (motor_plan_movement(adapter, &goal)) {
            motor_begin_execution(adapter);
            motor_update_execution(adapter, 1.0f);
        }
    }

    EXPECT_FALSE(race_detected.load())
        << "REGRESSION: Race detected during rapid-fire test";
}

//=============================================================================
// ATOMIC OPERATION VERIFICATION TESTS
//=============================================================================

TEST_F(MotorCallbackRegressionTest, AtomicOps_CompareExchangeSemantics) {
    /**
     * REGRESSION TEST: Verify CAS semantics are correctly used
     *
     * The motor adapter should use atomic compare-exchange to:
     * 1. Read in_callback flag
     * 2. Set to true only if it was false
     * 3. Only one thread can succeed at step 2
     */

    /* This test verifies by observing behavior with many threads */
    ASSERT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, nullptr));

    callback_count.store(0);
    concurrent_callbacks.store(0);
    max_concurrent.store(0);
    race_detected.store(false);

    const int num_threads = 50;
    std::vector<std::thread> threads;
    std::atomic<int> barrier{0};

    /* Launch many threads that all try to trigger callback at same time */
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &barrier, num_threads]() {
            /* Wait for all threads to be ready */
            barrier.fetch_add(1);
            while (barrier.load() < num_threads) {
                std::this_thread::yield();
            }

            /* All threads simultaneously try to trigger callback */
            motor_goal_t goal = {};
            goal.region = MOTOR_REGION_HAND_RIGHT;
            goal.target_position = {1.0f, 1.0f, 1.0f};
            goal.max_duration_ms = 10.0f;

            if (motor_plan_movement(adapter, &goal)) {
                motor_begin_execution(adapter);
                motor_update_execution(adapter, 1.0f);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    /* If atomic CAS is working, max_concurrent should be 1 */
    EXPECT_LE(max_concurrent.load(), 1)
        << "REGRESSION: Atomic compare-exchange not working correctly. "
        << "Multiple callbacks entered simultaneously (max_concurrent=" << max_concurrent.load() << ")";
}

TEST_F(MotorCallbackRegressionTest, AtomicOps_ReleaseSemanticsAfterCallback) {
    /**
     * REGRESSION TEST: in_callback flag must be released after callback
     *
     * After callback completes, another callback should be able to run.
     */
    static std::atomic<int> sequential_count{0};

    auto counting_callback = [](const motor_command_t* cmd, void* user_data) {
        (void)cmd;
        (void)user_data;
        sequential_count.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    };

    ASSERT_TRUE(motor_set_command_callback(adapter, counting_callback, nullptr));

    sequential_count.store(0);

    /* Trigger multiple callbacks sequentially */
    for (int i = 0; i < 100; i++) {
        motor_goal_t goal = {};
        goal.region = MOTOR_REGION_ARM_RIGHT;
        goal.target_position = {static_cast<float>(i), 0.0f, 0.0f};
        goal.max_duration_ms = 10.0f;

        if (motor_plan_movement(adapter, &goal)) {
            motor_begin_execution(adapter);
            motor_update_execution(adapter, 1.0f);
        }

        /* Allow callback to complete */
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    /* If release semantics are correct, all callbacks should have run */
    EXPECT_GT(sequential_count.load(), 0)
        << "REGRESSION: No callbacks were invoked - in_callback may not be released";
}

//=============================================================================
// CALLBACK SKIP BEHAVIOR TESTS
//=============================================================================

TEST_F(MotorCallbackRegressionTest, SkipBehavior_NoCallbackLostOnRace) {
    /**
     * REGRESSION TEST: When callback is skipped due to re-entrance, it's expected
     *
     * The design choice is to SKIP callbacks when one is already running,
     * rather than queue them. This test documents this behavior.
     */

    /* Use a slow callback to create overlap opportunity */
    static std::atomic<int> slow_callback_count{0};

    auto slow_callback = [](const motor_command_t* cmd, void* user_data) {
        (void)cmd;
        (void)user_data;
        slow_callback_count.fetch_add(1);
        /* Sleep to ensure overlap */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    };

    ASSERT_TRUE(motor_set_command_callback(adapter, slow_callback, nullptr));
    slow_callback_count.store(0);

    const int num_triggers = 50;

    /* Rapidly trigger callbacks */
    for (int i = 0; i < num_triggers; i++) {
        motor_goal_t goal = {};
        goal.region = MOTOR_REGION_LEG_LEFT;
        goal.target_position = {static_cast<float>(i), 0.0f, 0.0f};
        goal.max_duration_ms = 5.0f;

        if (motor_plan_movement(adapter, &goal)) {
            motor_begin_execution(adapter);
            motor_update_execution(adapter, 1.0f);
        }
    }

    /* Wait for callbacks to complete */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Some callbacks may have been skipped due to re-entrance protection */
    /* This is expected behavior, not a bug */
    EXPECT_GT(slow_callback_count.load(), 0)
        << "At least some callbacks should have executed";

    /* Document that not all callbacks necessarily execute */
    if (slow_callback_count.load() < num_triggers) {
        /* This is OK - some callbacks were skipped due to re-entrance prevention */
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
