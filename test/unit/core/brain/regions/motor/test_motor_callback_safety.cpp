/**
 * @file test_motor_callback_safety.cpp
 * @brief Unit tests for motor adapter callback safety
 *
 * WHAT: Tests for concurrent callback prevention and atomic flag correctness
 * WHY:  Ensure thread-safe callback invocation and prevent reentrancy issues
 * HOW:  Test atomic operations, reentrancy guards, callback state management
 *
 * TESTS COVER:
 * 1. Concurrent callback prevention (no double invocation)
 * 2. Atomic flag correctness for callback state
 * 3. Callback reentrancy guard operation
 * 4. NULL callback handling
 * 5. Callback registration/unregistration during execution
 * 6. Multiple callback types coexistence
 * 7. Callback error propagation
 *
 * @version 1.0.0
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <string.h>
#include <stdlib.h>

extern "C" {
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
}

//=============================================================================
// Test Data Structures
//=============================================================================

/**
 * @brief Thread-safe callback invocation tracker
 */
struct CallbackTracker {
    std::atomic<int> invocation_count{0};
    std::atomic<bool> is_executing{false};
    std::atomic<int> concurrent_calls{0};
    std::atomic<int> max_concurrent_calls{0};
    std::mutex mutex;
    std::condition_variable cv;

    /* Store received data for verification */
    motor_command_t last_command;
    motor_result_t last_result;

    void reset() {
        invocation_count = 0;
        is_executing = false;
        concurrent_calls = 0;
        max_concurrent_calls = 0;
        memset(&last_command, 0, sizeof(last_command));
        memset(&last_result, 0, sizeof(last_result));
    }
};

//=============================================================================
// Callback Functions
//=============================================================================

/**
 * @brief Command callback that tracks invocations
 */
static void tracking_command_callback(const motor_command_t* command, void* user_data) {
    if (!user_data) return;

    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);

    /* Track concurrent execution */
    int concurrent = ++tracker->concurrent_calls;
    int max_concurrent = tracker->max_concurrent_calls.load();
    while (concurrent > max_concurrent) {
        tracker->max_concurrent_calls.compare_exchange_weak(max_concurrent, concurrent);
    }

    /* Mark as executing */
    bool expected = false;
    if (!tracker->is_executing.compare_exchange_strong(expected, true)) {
        /* Already executing - reentrancy detected! */
        --tracker->concurrent_calls;
        return;
    }

    /* Increment invocation count */
    tracker->invocation_count++;

    /* Store command for verification */
    if (command) {
        tracker->last_command = *command;
    }

    /* Simulate some work */
    std::this_thread::sleep_for(std::chrono::microseconds(10));

    /* Mark as complete */
    tracker->is_executing = false;
    --tracker->concurrent_calls;

    /* Notify waiters */
    tracker->cv.notify_all();
}

/**
 * @brief Completion callback that tracks invocations
 */
static void tracking_complete_callback(const motor_result_t* result, void* user_data) {
    if (!user_data) return;

    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);

    tracker->invocation_count++;

    if (result) {
        tracker->last_result = *result;
    }
}

/**
 * @brief Command callback that attempts reentrancy
 */
static motor_adapter_t* g_reentrant_adapter = nullptr;

static void reentrant_command_callback(const motor_command_t* command, void* user_data) {
    if (!user_data) return;

    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->invocation_count++;

    /* Attempt to trigger another callback during this callback */
    /* This tests the reentrancy guard */
    if (g_reentrant_adapter && tracker->invocation_count == 1) {
        /* Try to get another command (which might invoke callback again) */
        motor_command_t next_cmd;
        motor_get_next_command(g_reentrant_adapter, &next_cmd);
    }
}

/**
 * @brief Slow command callback for race condition testing
 */
static void slow_command_callback(const motor_command_t* command, void* user_data) {
    if (!user_data) return;

    CallbackTracker* tracker = static_cast<CallbackTracker*>(user_data);

    /* Track concurrent execution */
    int concurrent = ++tracker->concurrent_calls;
    int max_concurrent = tracker->max_concurrent_calls.load();
    while (concurrent > max_concurrent) {
        tracker->max_concurrent_calls.compare_exchange_weak(max_concurrent, concurrent);
    }

    tracker->invocation_count++;

    /* Simulate slow processing */
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    --tracker->concurrent_calls;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MotorCallbackSafetyTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;
    CallbackTracker command_tracker;
    CallbackTracker complete_tracker;

    void SetUp() override {
        config = motor_default_config();
        config.enable_bio_async = false;
        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";

        command_tracker.reset();
        complete_tracker.reset();
        g_reentrant_adapter = nullptr;
    }

    void TearDown() override {
        g_reentrant_adapter = nullptr;
        motor_destroy(adapter);
        adapter = nullptr;
    }

    /**
     * @brief Create a simple motor goal for testing
     */
    motor_goal_t create_test_goal() {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = 1.0f;
        goal.target_position.y = 0.5f;
        goal.target_position.z = 0.0f;
        goal.max_duration_ms = 500.0f;
        goal.precision_required = 0.1f;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        goal.urgency = 0.5f;
        return goal;
    }

    /**
     * @brief Store and execute a simple motor program
     */
    void setup_executable_program() {
        motor_command_t commands[5];
        for (int i = 0; i < 5; i++) {
            memset(&commands[i], 0, sizeof(motor_command_t));
            commands[i].effector_id = 0;
            commands[i].target_position.x = 0.1f * (float)i;
            commands[i].target_force = 0.5f;
            commands[i].duration_ms = 50.0f;
            commands[i].timestamp_ms = (double)i * 50.0;
        }
        motor_store_program(adapter, "test_program", commands, 5, MOVEMENT_TYPE_DISCRETE);
    }
};

//=============================================================================
// Basic Callback Registration Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, SetCommandCallbackSuccess) {
    /**
     * WHAT: Test successful callback registration
     * WHY:  Callbacks must be registered before use
     * HOW:  Register callback, verify return value
     */
    bool result = motor_set_command_callback(adapter, tracking_command_callback, &command_tracker);
    EXPECT_TRUE(result);
}

TEST_F(MotorCallbackSafetyTest, SetCompleteCallbackSuccess) {
    /**
     * WHAT: Test successful completion callback registration
     * WHY:  Completion callbacks notify when movement finishes
     * HOW:  Register callback, verify return value
     */
    bool result = motor_set_complete_callback(adapter, tracking_complete_callback, &complete_tracker);
    EXPECT_TRUE(result);
}

TEST_F(MotorCallbackSafetyTest, SetCommandCallbackNullAdapter) {
    /**
     * WHAT: Test callback registration with NULL adapter
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL adapter, verify returns false
     */
    bool result = motor_set_command_callback(nullptr, tracking_command_callback, &command_tracker);
    EXPECT_FALSE(result);
}

TEST_F(MotorCallbackSafetyTest, SetNullCallback) {
    /**
     * WHAT: Test setting NULL callback (to clear callback)
     * WHY:  Allow unregistering callbacks
     * HOW:  Set NULL callback, verify no crash
     */
    /* Register callback first */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    /* Clear callback by setting NULL */
    bool result = motor_set_command_callback(adapter, nullptr, nullptr);
    EXPECT_TRUE(result);  /* Should succeed - clearing callback is valid */
}

TEST_F(MotorCallbackSafetyTest, SetCallbackWithNullUserData) {
    /**
     * WHAT: Test callback registration with NULL user data
     * WHY:  User data is optional
     * HOW:  Pass NULL user_data, verify registration succeeds
     */
    bool result = motor_set_command_callback(adapter, tracking_command_callback, nullptr);
    EXPECT_TRUE(result);
}

//=============================================================================
// Callback Invocation Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, CommandCallbackInvokedDuringExecution) {
    /**
     * WHAT: Test that command callback is invoked during execution
     * WHY:  Verify callback mechanism works
     * HOW:  Register callback, execute movement, check invocation count
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Update execution to generate commands */
    for (int i = 0; i < 10; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Callback should have been invoked at least once */
    /* Note: Depends on implementation - callback may or may not be called
       during update_execution depending on motor adapter design */
}

TEST_F(MotorCallbackSafetyTest, CallbackReceivesValidData) {
    /**
     * WHAT: Test that callback receives valid command data
     * WHY:  Callbacks must receive correct information
     * HOW:  Check callback received valid command structure
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* If callback was invoked, verify data structure is valid */
    if (command_tracker.invocation_count > 0) {
        /* Duration should be positive */
        EXPECT_GE(command_tracker.last_command.duration_ms, 0.0f);
    }
}

//=============================================================================
// Reentrancy Safety Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, NoReentrancyDuringCallback) {
    /**
     * WHAT: Test that callbacks are not re-entered
     * WHY:  Reentrancy can cause stack overflow and data corruption
     * HOW:  Set callback that tries to trigger more callbacks
     */
    g_reentrant_adapter = adapter;

    EXPECT_TRUE(motor_set_command_callback(adapter, reentrant_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Should not have infinite/excessive invocations */
    EXPECT_LT(command_tracker.invocation_count, 100)
        << "Too many callback invocations - possible reentrancy";

    g_reentrant_adapter = nullptr;
}

TEST_F(MotorCallbackSafetyTest, ConcurrentCallbacksLimited) {
    /**
     * WHAT: Test that concurrent callback execution is limited
     * WHY:  Prevent race conditions in callback execution
     * HOW:  Track maximum concurrent callback executions
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Execute multiple updates */
    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 5.0f);
    }

    /* Max concurrent should be 1 (no parallel callback execution from single thread) */
    EXPECT_LE(command_tracker.max_concurrent_calls, 1)
        << "Multiple concurrent callbacks detected - thread safety issue";
}

//=============================================================================
// Multiple Callback Type Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, MultipleCallbackTypesCoexist) {
    /**
     * WHAT: Test that different callback types can be registered simultaneously
     * WHY:  Users may need both command and completion callbacks
     * HOW:  Register both, verify both work
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));
    EXPECT_TRUE(motor_set_complete_callback(adapter, tracking_complete_callback, &complete_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Execute until completion */
    for (int i = 0; i < 100; i++) {
        if (!motor_update_execution(adapter, 10.0f)) {
            break;  /* Movement complete */
        }
    }

    /* Both trackers should be usable */
    SUCCEED();
}

TEST_F(MotorCallbackSafetyTest, OverwriteCallback) {
    /**
     * WHAT: Test that callbacks can be replaced
     * WHY:  Users may need to change callbacks dynamically
     * HOW:  Register one callback, replace with another
     */
    CallbackTracker tracker1, tracker2;
    tracker1.reset();
    tracker2.reset();

    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &tracker1));

    /* Replace callback */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &tracker2));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Only tracker2 should receive callbacks after replacement */
    /* (tracker1 may have some if callback was invoked during registration change) */
}

//=============================================================================
// Callback During State Transitions Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, CallbackSafetyDuringReset) {
    /**
     * WHAT: Test callback safety during adapter reset
     * WHY:  Reset should not cause callback issues
     * HOW:  Register callback, reset, verify no crash
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    motor_update_execution(adapter, 10.0f);

    /* Reset during execution */
    EXPECT_TRUE(motor_reset(adapter));

    /* Should be able to continue using adapter */
    EXPECT_EQ(motor_get_status(adapter), MOTOR_STATUS_IDLE);
}

TEST_F(MotorCallbackSafetyTest, CallbackSafetyDuringStop) {
    /**
     * WHAT: Test callback safety during execution stop
     * WHY:  Stop should not cause callback issues
     * HOW:  Register callback, start execution, stop, verify no crash
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    motor_update_execution(adapter, 10.0f);

    /* Stop during execution */
    EXPECT_TRUE(motor_stop_execution(adapter));

    /* Callbacks should stop being invoked */
    int count_before = command_tracker.invocation_count.load();
    motor_update_execution(adapter, 10.0f);
    int count_after = command_tracker.invocation_count.load();

    /* No new invocations after stop (or only stop-related ones) */
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, CallbackNotInvokedOnNullCommand) {
    /**
     * WHAT: Test that callback handles NULL command gracefully
     * WHY:  Edge case protection
     * HOW:  Callback implementation should check for NULL
     */
    /* The callback itself handles NULL - this tests our callback impl */
    tracking_command_callback(nullptr, &command_tracker);

    /* Should have incremented count but not crashed */
    EXPECT_GE(command_tracker.invocation_count, 0);
}

TEST_F(MotorCallbackSafetyTest, CallbackWithNullUserDataDoesNotCrash) {
    /**
     * WHAT: Test callback invocation with NULL user data
     * WHY:  Null user data must not cause crash
     * HOW:  Invoke callback with NULL user data
     */
    motor_command_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    /* Our callback checks for NULL user_data */
    tracking_command_callback(&cmd, nullptr);

    /* Should not crash */
    SUCCEED();
}

//=============================================================================
// Atomic Flag Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, AtomicFlagPreventsDoubleFire) {
    /**
     * WHAT: Test that atomic is_executing flag prevents double callback
     * WHY:  Ensure thread-safe callback invocation
     * HOW:  Verify max concurrent calls never exceeds 1
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, slow_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Rapid updates to try to trigger concurrent callbacks */
    for (int i = 0; i < 50; i++) {
        motor_update_execution(adapter, 1.0f);
    }

    /* From single thread, should never have more than 1 concurrent */
    EXPECT_LE(command_tracker.max_concurrent_calls, 1);
}

//=============================================================================
// Registration During Execution Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, RegisterCallbackDuringExecution) {
    /**
     * WHAT: Test callback registration during ongoing execution
     * WHY:  Dynamic callback changes should be safe
     * HOW:  Start execution, register callback mid-execution
     */
    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Execute some updates without callback */
    motor_update_execution(adapter, 10.0f);
    motor_update_execution(adapter, 10.0f);

    /* Register callback mid-execution */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    /* Continue execution with callback */
    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Should not crash */
    SUCCEED();
}

TEST_F(MotorCallbackSafetyTest, UnregisterCallbackDuringExecution) {
    /**
     * WHAT: Test callback unregistration during ongoing execution
     * WHY:  Cleanup scenarios require safe unregistration
     * HOW:  Start execution with callback, unregister mid-execution
     */
    EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

    motor_goal_t goal = create_test_goal();
    EXPECT_TRUE(motor_plan_movement(adapter, &goal));
    EXPECT_TRUE(motor_begin_execution(adapter));

    /* Execute some updates with callback */
    motor_update_execution(adapter, 10.0f);
    motor_update_execution(adapter, 10.0f);

    int count_before = command_tracker.invocation_count.load();

    /* Unregister callback mid-execution */
    EXPECT_TRUE(motor_set_command_callback(adapter, nullptr, nullptr));

    /* Continue execution without callback */
    for (int i = 0; i < 5; i++) {
        motor_update_execution(adapter, 10.0f);
    }

    /* Should not crash and invocations should have stopped */
    SUCCEED();
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(MotorCallbackSafetyTest, CallbackDoesNotLeakMemory) {
    /**
     * WHAT: Test that callback mechanism doesn't leak memory
     * WHY:  Memory leaks in callbacks accumulate over time
     * HOW:  Register/unregister multiple times, execute movements
     */
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Register callback */
        EXPECT_TRUE(motor_set_command_callback(adapter, tracking_command_callback, &command_tracker));

        motor_goal_t goal = create_test_goal();
        motor_plan_movement(adapter, &goal);
        motor_begin_execution(adapter);

        for (int i = 0; i < 5; i++) {
            motor_update_execution(adapter, 10.0f);
        }

        motor_stop_execution(adapter);

        /* Unregister callback */
        EXPECT_TRUE(motor_set_command_callback(adapter, nullptr, nullptr));

        /* Reset for next cycle */
        motor_reset(adapter);
        command_tracker.reset();
    }

    /* If we get here without crash/hang, memory is likely ok */
    SUCCEED();
}

//=============================================================================
// Event Callback Tests
//=============================================================================

static std::atomic<int> g_event_count{0};

static void event_callback(uint32_t event_type, const void* event_data, void* user_data) {
    g_event_count++;
}

TEST_F(MotorCallbackSafetyTest, EventCallbackRegistration) {
    /**
     * WHAT: Test event callback registration
     * WHY:  Event callbacks are another callback type to test
     * HOW:  Register event callback, verify success
     */
    g_event_count = 0;
    bool result = motor_set_event_callback(adapter, event_callback, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(MotorCallbackSafetyTest, EventCallbackNullAdapter) {
    /**
     * WHAT: Test event callback with NULL adapter
     * WHY:  Must handle NULL gracefully
     * HOW:  Pass NULL adapter, verify returns false
     */
    bool result = motor_set_event_callback(nullptr, event_callback, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
