/**
 * @file test_recovery.cpp
 * @brief Tests for recovery framework (circuit breaker)
 *
 * WHAT: Verify circuit breaker pattern and recovery utilities
 * WHY:  Recovery framework provides fault tolerance via circuit breakers
 * HOW:  Test circuit breaker creation, state transitions, success/failure recording
 *
 * Function signatures tested (from include/utils/fault_tolerance/nimcp_recovery.h):
 *   circuit_breaker_t* circuit_breaker_create(uint32_t failure_threshold, uint32_t timeout_ms);
 *   void circuit_breaker_destroy(circuit_breaker_t* cb);
 *   bool circuit_breaker_allow_operation(circuit_breaker_t* cb);
 *   void circuit_breaker_record_success(circuit_breaker_t* cb);
 *   void circuit_breaker_record_failure(circuit_breaker_t* cb);
 *   void circuit_breaker_reset(circuit_breaker_t* cb);
 *   circuit_state_t circuit_breaker_get_state(circuit_breaker_t* cb);
 *
 * Types:
 *   circuit_state_t: CIRCUIT_CLOSED, CIRCUIT_OPEN, CIRCUIT_HALF_OPEN
 *   recovery_tier_t: RECOVERY_TIER_IMMEDIATE, _TACTICAL, _STRATEGIC, _PREVENTIVE
 *   recovery_status_t: RECOVERY_SUCCESS, _PARTIAL, _FAILED, _NOT_APPLICABLE
 *
 * Utility:
 *   const char* recovery_tier_name(recovery_tier_t tier);
 *   const char* recovery_action_name(recovery_action_t action);
 *   const char* recovery_status_name(recovery_status_t status);
 */

#include <gtest/gtest.h>

extern "C" {
#include "utils/fault_tolerance/nimcp_recovery.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RecoveryTest : public ::testing::Test {
protected:
    circuit_breaker_t* cb = nullptr;

    void SetUp() override {
        cb = circuit_breaker_create(3, 1000); // 3 failures, 1s timeout
        ASSERT_NE(cb, nullptr);
    }

    void TearDown() override {
        circuit_breaker_destroy(cb);
        cb = nullptr;
    }
};

/* ============================================================================
 * Circuit Breaker Creation / Destruction
 * ============================================================================ */

TEST_F(RecoveryTest, CreateCircuitBreaker) {
    EXPECT_NE(cb, nullptr);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
}

TEST(RecoveryBasicTest, CreateWithDifferentThresholds) {
    circuit_breaker_t* cb1 = circuit_breaker_create(1, 500);
    ASSERT_NE(cb1, nullptr);
    EXPECT_EQ(circuit_breaker_get_state(cb1), CIRCUIT_CLOSED);
    circuit_breaker_destroy(cb1);

    circuit_breaker_t* cb2 = circuit_breaker_create(10, 5000);
    ASSERT_NE(cb2, nullptr);
    EXPECT_EQ(circuit_breaker_get_state(cb2), CIRCUIT_CLOSED);
    circuit_breaker_destroy(cb2);
}

TEST(RecoveryBasicTest, DestroyNull) {
    circuit_breaker_destroy(nullptr);
    SUCCEED() << "Destroying NULL circuit breaker did not crash";
}

/* ============================================================================
 * Initial State Tests
 * ============================================================================ */

TEST_F(RecoveryTest, InitialStateClosed) {
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
}

TEST_F(RecoveryTest, OperationAllowedInClosedState) {
    EXPECT_TRUE(circuit_breaker_allow_operation(cb));
}

/* ============================================================================
 * State Transition: CLOSED -> OPEN
 * ============================================================================ */

TEST_F(RecoveryTest, OpensAfterThresholdFailures) {
    // Circuit breaker has failure_threshold=3
    // Record 3 failures to trip the circuit
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    circuit_breaker_record_failure(cb);
    // After 3 failures, circuit should be OPEN
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);
}

TEST_F(RecoveryTest, OperationBlockedInOpenState) {
    // Trip the circuit
    for (uint32_t i = 0; i < 3; i++) {
        circuit_breaker_record_failure(cb);
    }

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);
    EXPECT_FALSE(circuit_breaker_allow_operation(cb));
}

/* ============================================================================
 * Success Recording Tests
 * ============================================================================ */

TEST_F(RecoveryTest, SuccessResetsfailureCount) {
    // Record some failures (below threshold)
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    // Success should reset failure count
    circuit_breaker_record_success(cb);

    // Now 3 more failures should be needed to open
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    // These 2 failures should not open since success reset the count
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
}

TEST_F(RecoveryTest, SuccessInClosedState) {
    circuit_breaker_record_success(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    EXPECT_TRUE(circuit_breaker_allow_operation(cb));
}

/* ============================================================================
 * Circuit Breaker Reset
 * ============================================================================ */

TEST_F(RecoveryTest, ResetClosesCircuit) {
    // Trip the circuit
    for (uint32_t i = 0; i < 3; i++) {
        circuit_breaker_record_failure(cb);
    }
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    // Reset should close it
    circuit_breaker_reset(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    EXPECT_TRUE(circuit_breaker_allow_operation(cb));
}

TEST_F(RecoveryTest, ResetClearsCounters) {
    // Record some activity
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_success(cb);

    circuit_breaker_reset(cb);

    // After reset, state should be clean
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    EXPECT_EQ(cb->failure_count, 0u);
    EXPECT_EQ(cb->success_count, 0u);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST(RecoveryUtilityTest, TierNames) {
    const char* name;

    name = recovery_tier_name(RECOVERY_TIER_IMMEDIATE);
    ASSERT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = recovery_tier_name(RECOVERY_TIER_TACTICAL);
    ASSERT_NE(name, nullptr);

    name = recovery_tier_name(RECOVERY_TIER_STRATEGIC);
    ASSERT_NE(name, nullptr);

    name = recovery_tier_name(RECOVERY_TIER_PREVENTIVE);
    ASSERT_NE(name, nullptr);
}

TEST(RecoveryUtilityTest, ActionNames) {
    const char* name;

    name = recovery_action_name(RECOVERY_ACTION_NONE);
    ASSERT_NE(name, nullptr);

    name = recovery_action_name(RECOVERY_ACTION_CLEAR_NAN);
    ASSERT_NE(name, nullptr);

    name = recovery_action_name(RECOVERY_ACTION_FALLBACK_CPU);
    ASSERT_NE(name, nullptr);
}

TEST(RecoveryUtilityTest, StatusNames) {
    const char* name;

    name = recovery_status_name(RECOVERY_SUCCESS);
    ASSERT_NE(name, nullptr);

    name = recovery_status_name(RECOVERY_FAILED);
    ASSERT_NE(name, nullptr);

    name = recovery_status_name(RECOVERY_PARTIAL);
    ASSERT_NE(name, nullptr);

    name = recovery_status_name(RECOVERY_NOT_APPLICABLE);
    ASSERT_NE(name, nullptr);
}

/* ============================================================================
 * Enum Value Tests
 * ============================================================================ */

TEST(RecoveryEnumTest, CircuitStates) {
    EXPECT_NE(CIRCUIT_CLOSED, CIRCUIT_OPEN);
    EXPECT_NE(CIRCUIT_OPEN, CIRCUIT_HALF_OPEN);
    EXPECT_NE(CIRCUIT_CLOSED, CIRCUIT_HALF_OPEN);
}

TEST(RecoveryEnumTest, RecoveryTiers) {
    EXPECT_NE(RECOVERY_TIER_IMMEDIATE, RECOVERY_TIER_TACTICAL);
    EXPECT_NE(RECOVERY_TIER_TACTICAL, RECOVERY_TIER_STRATEGIC);
    EXPECT_NE(RECOVERY_TIER_STRATEGIC, RECOVERY_TIER_PREVENTIVE);
}

TEST(RecoveryEnumTest, RecoveryActions) {
    EXPECT_EQ(RECOVERY_ACTION_NONE, 0);
    EXPECT_NE(RECOVERY_ACTION_CLEAR_NAN, RECOVERY_ACTION_NONE);
    EXPECT_NE(RECOVERY_ACTION_RELOAD_CHECKPOINT, RECOVERY_ACTION_NONE);
    EXPECT_NE(RECOVERY_ACTION_FALLBACK_CPU, RECOVERY_ACTION_NONE);
}
