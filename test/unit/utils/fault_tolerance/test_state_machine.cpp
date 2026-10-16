/**
 * @file test_state_machine.cpp
 * @brief Unit tests for State Machine (100% coverage)
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_fault_state_machine.h"
#include <thread>
#include <chrono>

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class StateMachineTest : public ::testing::Test {
protected:
    nimcp_state_machine_t* sm;

    void SetUp() override {
        sm = nimcp_state_machine_create();
        ASSERT_NE(sm, nullptr);
    }

    void TearDown() override {
        nimcp_state_machine_destroy(sm);
    }
};

/* Callback tracking structure */
struct CallbackTracker {
    int entry_count;
    int exit_count;
    nimcp_brain_state_t last_entry_state;
    nimcp_brain_state_t last_exit_state;
    bool should_succeed;

    CallbackTracker() : entry_count(0), exit_count(0),
                        last_entry_state(NIMCP_STATE_HEALTHY),
                        last_exit_state(NIMCP_STATE_HEALTHY),
                        should_succeed(true) {}
};

static bool entry_callback(nimcp_brain_state_t state, void* user_data) {
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->entry_count++;
    tracker->last_entry_state = state;
    return tracker->should_succeed;
}

static bool exit_callback(nimcp_brain_state_t state, void* user_data) {
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->exit_count++;
    tracker->last_exit_state = state;
    return tracker->should_succeed;
}

static bool guard_function(nimcp_brain_state_t from_state,
                          nimcp_brain_state_t to_state,
                          void* user_data) {
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    return tracker->should_succeed;
}

/* =============================================================================
 * Creation and Destruction Tests
 * ============================================================================= */

TEST_F(StateMachineTest, CreateInitializesCorrectly) {
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, CreateReturnsNonNull) {
    nimcp_state_machine_t* sm2 = nimcp_state_machine_create();
    ASSERT_NE(sm2, nullptr);
    nimcp_state_machine_destroy(sm2);
}

TEST_F(StateMachineTest, DestroyHandlesNull) {
    nimcp_state_machine_destroy(nullptr);
    // Should not crash
}

TEST_F(StateMachineTest, GetStateReturnsFailedForNull) {
    EXPECT_EQ(nimcp_state_machine_get_state(nullptr), NIMCP_STATE_FAILED);
}

/* =============================================================================
 * Valid Transition Tests
 * ============================================================================= */

TEST_F(StateMachineTest, HealthyToDegraded) {
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);
}

TEST_F(StateMachineTest, HealthyToFailed) {
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 2);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_FAILED);
}

TEST_F(StateMachineTest, HealthyToShutdown) {
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_SHUTDOWN, 3);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_SHUTDOWN);
}

TEST_F(StateMachineTest, DegradedToHealthy) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 4);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, DegradedToRecovering) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 5);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_RECOVERING);
}

TEST_F(StateMachineTest, DegradedToFailed) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 6);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_FAILED);
}

TEST_F(StateMachineTest, RecoveringToHealthy) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 2);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 7);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, RecoveringToDegraded) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 2);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 8);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);
}

TEST_F(StateMachineTest, FailedToRecovering) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 1);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 9);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_RECOVERING);
}

TEST_F(StateMachineTest, AnyStateToShutdown) {
    // Test from each state
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    EXPECT_EQ(nimcp_state_machine_transition(sm, NIMCP_STATE_SHUTDOWN, 2),
              NIMCP_TRANSITION_SUCCESS);

    nimcp_state_machine_t* sm2 = nimcp_state_machine_create();
    nimcp_state_machine_transition(sm2, NIMCP_STATE_FAILED, 1);
    EXPECT_EQ(nimcp_state_machine_transition(sm2, NIMCP_STATE_SHUTDOWN, 2),
              NIMCP_TRANSITION_SUCCESS);
    nimcp_state_machine_destroy(sm2);
}

/* =============================================================================
 * Invalid Transition Tests
 * ============================================================================= */

TEST_F(StateMachineTest, HealthyToRecovering_Invalid) {
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_INVALID);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, FailedToHealthy_Invalid) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 1);
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 2);
    EXPECT_EQ(result, NIMCP_TRANSITION_INVALID);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_FAILED);
}

TEST_F(StateMachineTest, ShutdownToAny_Invalid) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_SHUTDOWN, 1);

    EXPECT_EQ(nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 2),
              NIMCP_TRANSITION_INVALID);
    EXPECT_EQ(nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 3),
              NIMCP_TRANSITION_INVALID);
    EXPECT_EQ(nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 4),
              NIMCP_TRANSITION_INVALID);
    EXPECT_EQ(nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 5),
              NIMCP_TRANSITION_INVALID);
}

TEST_F(StateMachineTest, SelfTransition_NoOp) {
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, InvalidStateEnum) {
    auto result = nimcp_state_machine_transition(sm,
        static_cast<nimcp_brain_state_t>(999), 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_INVALID);
}

TEST_F(StateMachineTest, TransitionNullStateMachine) {
    auto result = nimcp_state_machine_transition(nullptr, NIMCP_STATE_DEGRADED, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_ERROR);
}

/* =============================================================================
 * Callback Tests
 * ============================================================================= */

TEST_F(StateMachineTest, EntryCallbackExecuted) {
    CallbackTracker tracker;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_entry_callback(sm, NIMCP_STATE_DEGRADED, entry_callback);

    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    EXPECT_EQ(tracker.entry_count, 1);
    EXPECT_EQ(tracker.last_entry_state, NIMCP_STATE_DEGRADED);
}

TEST_F(StateMachineTest, ExitCallbackExecuted) {
    CallbackTracker tracker;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_exit_callback(sm, NIMCP_STATE_HEALTHY, exit_callback);

    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    EXPECT_EQ(tracker.exit_count, 1);
    EXPECT_EQ(tracker.last_exit_state, NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, EntryAndExitCallbacksExecuted) {
    CallbackTracker tracker;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_exit_callback(sm, NIMCP_STATE_HEALTHY, exit_callback);
    nimcp_state_machine_set_entry_callback(sm, NIMCP_STATE_DEGRADED, entry_callback);

    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    EXPECT_EQ(tracker.exit_count, 1);
    EXPECT_EQ(tracker.entry_count, 1);
}

TEST_F(StateMachineTest, ExitCallbackFails) {
    CallbackTracker tracker;
    tracker.should_succeed = false;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_exit_callback(sm, NIMCP_STATE_HEALTHY, exit_callback);

    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    EXPECT_EQ(result, NIMCP_TRANSITION_CALLBACK_FAILED);
    EXPECT_EQ(tracker.exit_count, 1);
}

TEST_F(StateMachineTest, EntryCallbackFails) {
    CallbackTracker tracker;
    tracker.should_succeed = false;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_entry_callback(sm, NIMCP_STATE_DEGRADED, entry_callback);

    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    EXPECT_EQ(result, NIMCP_TRANSITION_CALLBACK_FAILED);
    EXPECT_EQ(tracker.entry_count, 1);
    // State should still be DEGRADED even though callback failed
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);
}

TEST_F(StateMachineTest, SetCallbackNull) {
    EXPECT_FALSE(nimcp_state_machine_set_entry_callback(nullptr,
        NIMCP_STATE_HEALTHY, entry_callback));
    EXPECT_FALSE(nimcp_state_machine_set_exit_callback(nullptr,
        NIMCP_STATE_HEALTHY, exit_callback));
}

TEST_F(StateMachineTest, SetCallbackInvalidState) {
    EXPECT_FALSE(nimcp_state_machine_set_entry_callback(sm,
        static_cast<nimcp_brain_state_t>(999), entry_callback));
    EXPECT_FALSE(nimcp_state_machine_set_exit_callback(sm,
        static_cast<nimcp_brain_state_t>(999), exit_callback));
}

/* =============================================================================
 * Guard Tests
 * ============================================================================= */

TEST_F(StateMachineTest, GuardAllowsTransition) {
    CallbackTracker tracker;
    tracker.should_succeed = true;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_guard(sm, guard_function);

    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_SUCCESS);
}

TEST_F(StateMachineTest, GuardBlocksTransition) {
    CallbackTracker tracker;
    tracker.should_succeed = false;
    nimcp_state_machine_set_user_data(sm, &tracker);
    nimcp_state_machine_set_guard(sm, guard_function);

    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_BLOCKED);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
}

TEST_F(StateMachineTest, SetGuardNull) {
    EXPECT_FALSE(nimcp_state_machine_set_guard(nullptr, guard_function));
}

/* =============================================================================
 * History Tests
 * ============================================================================= */

TEST_F(StateMachineTest, HistoryRecordsTransitions) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 100);
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 200);
    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 300);

    uint32_t count = 0;
    const nimcp_state_transition_t* history =
        nimcp_state_machine_get_history(sm, &count);

    ASSERT_NE(history, nullptr);
    EXPECT_EQ(count, 3);

    EXPECT_EQ(history[0].from_state, NIMCP_STATE_HEALTHY);
    EXPECT_EQ(history[0].to_state, NIMCP_STATE_DEGRADED);
    EXPECT_EQ(history[0].reason_code, 100);

    EXPECT_EQ(history[1].from_state, NIMCP_STATE_DEGRADED);
    EXPECT_EQ(history[1].to_state, NIMCP_STATE_RECOVERING);
    EXPECT_EQ(history[1].reason_code, 200);

    EXPECT_EQ(history[2].from_state, NIMCP_STATE_RECOVERING);
    EXPECT_EQ(history[2].to_state, NIMCP_STATE_HEALTHY);
    EXPECT_EQ(history[2].reason_code, 300);
}

TEST_F(StateMachineTest, HistoryCircularBuffer) {
    // Fill history beyond capacity
    for (uint32_t i = 0; i < NIMCP_STATE_HISTORY_SIZE + 10; i++) {
        nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, i * 2);
        nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, i * 2 + 1);
    }

    uint32_t count = 0;
    nimcp_state_machine_get_history(sm, &count);
    EXPECT_EQ(count, NIMCP_STATE_HISTORY_SIZE);
}

TEST_F(StateMachineTest, HistoryEmptyInitially) {
    uint32_t count = 999;
    const nimcp_state_transition_t* history =
        nimcp_state_machine_get_history(sm, &count);

    EXPECT_EQ(history, nullptr);
    EXPECT_EQ(count, 0);
}

TEST_F(StateMachineTest, GetHistoryNull) {
    uint32_t count = 0;
    EXPECT_EQ(nimcp_state_machine_get_history(nullptr, &count), nullptr);
    EXPECT_EQ(count, 0);

    const nimcp_state_transition_t* history =
        nimcp_state_machine_get_history(sm, nullptr);
    EXPECT_EQ(history, nullptr);
}

/* =============================================================================
 * Duration and Statistics Tests
 * ============================================================================= */

TEST_F(StateMachineTest, CurrentStateDuration) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t duration = nimcp_state_machine_get_current_state_duration(sm);
    EXPECT_GE(duration, 1);
}

TEST_F(StateMachineTest, TotalStateDuration) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t healthy_duration =
        nimcp_state_machine_get_total_state_duration(sm, NIMCP_STATE_HEALTHY);
    uint64_t degraded_duration =
        nimcp_state_machine_get_total_state_duration(sm, NIMCP_STATE_DEGRADED);

    EXPECT_GE(healthy_duration, 1);
    EXPECT_GE(degraded_duration, 1);
}

TEST_F(StateMachineTest, TransitionStatistics) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);  // Valid
    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 2);   // Valid
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 3); // Invalid: HEALTHY -> RECOVERING is not allowed

    uint32_t attempts = 0, failures = 0;
    EXPECT_TRUE(nimcp_state_machine_get_statistics(sm, &attempts, &failures));

    EXPECT_EQ(attempts, 3);
    EXPECT_EQ(failures, 1);
}

TEST_F(StateMachineTest, ResetStatistics) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    nimcp_state_machine_reset_statistics(sm);

    uint32_t attempts = 0, failures = 0;
    nimcp_state_machine_get_statistics(sm, &attempts, &failures);
    EXPECT_EQ(attempts, 0);
    EXPECT_EQ(failures, 0);

    uint64_t duration =
        nimcp_state_machine_get_total_state_duration(sm, NIMCP_STATE_DEGRADED);
    EXPECT_EQ(duration, 0);
}

TEST_F(StateMachineTest, GetStatisticsNull) {
    uint32_t attempts = 0, failures = 0;
    EXPECT_FALSE(nimcp_state_machine_get_statistics(nullptr, &attempts, &failures));
    EXPECT_FALSE(nimcp_state_machine_get_statistics(sm, nullptr, &failures));
    EXPECT_FALSE(nimcp_state_machine_get_statistics(sm, &attempts, nullptr));
}

TEST_F(StateMachineTest, GetDurationNull) {
    EXPECT_EQ(nimcp_state_machine_get_current_state_duration(nullptr), 0);
    EXPECT_EQ(nimcp_state_machine_get_total_state_duration(nullptr,
        NIMCP_STATE_HEALTHY), 0);
}

TEST_F(StateMachineTest, GetDurationInvalidState) {
    EXPECT_EQ(nimcp_state_machine_get_total_state_duration(sm,
        static_cast<nimcp_brain_state_t>(999)), 0);
}

/* =============================================================================
 * Validation Tests
 * ============================================================================= */

TEST_F(StateMachineTest, IsValidTransition) {
    EXPECT_TRUE(nimcp_state_machine_is_valid_transition(
        NIMCP_STATE_HEALTHY, NIMCP_STATE_DEGRADED));
    EXPECT_TRUE(nimcp_state_machine_is_valid_transition(
        NIMCP_STATE_DEGRADED, NIMCP_STATE_RECOVERING));
    EXPECT_FALSE(nimcp_state_machine_is_valid_transition(
        NIMCP_STATE_HEALTHY, NIMCP_STATE_RECOVERING));
    EXPECT_FALSE(nimcp_state_machine_is_valid_transition(
        NIMCP_STATE_SHUTDOWN, NIMCP_STATE_HEALTHY));
}

TEST_F(StateMachineTest, IsValidTransitionInvalidStates) {
    EXPECT_FALSE(nimcp_state_machine_is_valid_transition(
        static_cast<nimcp_brain_state_t>(999), NIMCP_STATE_HEALTHY));
    EXPECT_FALSE(nimcp_state_machine_is_valid_transition(
        NIMCP_STATE_HEALTHY, static_cast<nimcp_brain_state_t>(999)));
}

TEST_F(StateMachineTest, IsTerminalState) {
    EXPECT_FALSE(nimcp_state_machine_is_terminal(NIMCP_STATE_HEALTHY));
    EXPECT_FALSE(nimcp_state_machine_is_terminal(NIMCP_STATE_DEGRADED));
    EXPECT_FALSE(nimcp_state_machine_is_terminal(NIMCP_STATE_RECOVERING));
    EXPECT_FALSE(nimcp_state_machine_is_terminal(NIMCP_STATE_FAILED));
    EXPECT_TRUE(nimcp_state_machine_is_terminal(NIMCP_STATE_SHUTDOWN));
}

/* =============================================================================
 * Utility Function Tests
 * ============================================================================= */

TEST_F(StateMachineTest, StateToString) {
    EXPECT_STREQ(nimcp_state_to_string(NIMCP_STATE_HEALTHY), "HEALTHY");
    EXPECT_STREQ(nimcp_state_to_string(NIMCP_STATE_DEGRADED), "DEGRADED");
    EXPECT_STREQ(nimcp_state_to_string(NIMCP_STATE_RECOVERING), "RECOVERING");
    EXPECT_STREQ(nimcp_state_to_string(NIMCP_STATE_FAILED), "FAILED");
    EXPECT_STREQ(nimcp_state_to_string(NIMCP_STATE_SHUTDOWN), "SHUTDOWN");
    EXPECT_STREQ(nimcp_state_to_string(static_cast<nimcp_brain_state_t>(999)), "UNKNOWN");
}

TEST_F(StateMachineTest, TransitionResultToString) {
    EXPECT_STREQ(nimcp_transition_result_to_string(NIMCP_TRANSITION_SUCCESS), "SUCCESS");
    EXPECT_STREQ(nimcp_transition_result_to_string(NIMCP_TRANSITION_INVALID), "INVALID");
    EXPECT_STREQ(nimcp_transition_result_to_string(NIMCP_TRANSITION_BLOCKED), "BLOCKED");
    EXPECT_STREQ(nimcp_transition_result_to_string(NIMCP_TRANSITION_CALLBACK_FAILED),
                 "CALLBACK_FAILED");
    EXPECT_STREQ(nimcp_transition_result_to_string(NIMCP_TRANSITION_ERROR), "ERROR");
    EXPECT_STREQ(nimcp_transition_result_to_string(
        static_cast<nimcp_transition_result_t>(999)), "UNKNOWN");
}

/* =============================================================================
 * Edge Cases and Error Handling
 * ============================================================================= */

TEST_F(StateMachineTest, ResetStatisticsNull) {
    nimcp_state_machine_reset_statistics(nullptr);
    // Should not crash
}

TEST_F(StateMachineTest, SetUserDataNull) {
    nimcp_state_machine_set_user_data(nullptr, nullptr);
    // Should not crash
}

TEST_F(StateMachineTest, MultipleTransitionsSameReasonCode) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 42);
    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 42);

    uint32_t count = 0;
    const nimcp_state_transition_t* history =
        nimcp_state_machine_get_history(sm, &count);

    EXPECT_EQ(history[0].reason_code, 42);
    EXPECT_EQ(history[1].reason_code, 42);
}
