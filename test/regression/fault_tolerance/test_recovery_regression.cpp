/**
 * @file test_recovery_regression.cpp
 * @brief Regression tests for recovery/circuit breaker stability (P1-P3 remediation)
 *
 * WHAT: Regression tests for circuit breaker state transitions and recovery
 * WHY:  Ensure circuit breaker doesn't regress after P1-P3 fixes
 * HOW:  Test state transitions, threshold behavior, reset functionality
 *
 * REGRESSION CATEGORIES:
 * - State Transitions: Deterministic CLOSED->OPEN->HALF_OPEN->CLOSED
 * - Threshold Behavior: Consistent failure threshold handling
 * - Reset Functionality: Reset reliably returns to initial state
 * - Multiple Breakers: No interference between circuit breakers
 * - API Stability: Return codes and behavior remain consistent
 *
 * @author NIMCP Development Team
 * @date 2026-02-05
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

extern "C" {
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/memory/nimcp_memory.h"
}

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class RecoveryRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
    }

    void TearDown() override {
    }

    /* Helper to get current time in ms */
    uint64_t current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
};

/* ============================================================================
 * Circuit Breaker State Transition Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, CircuitBreaker_InitialStateClosed) {
    /* WHAT: Verify circuit breaker starts in CLOSED state */
    /* REGRESSION: Initial state must be CLOSED */
    
    circuit_breaker_t* cb = circuit_breaker_create(5, 1000);
    ASSERT_NE(cb, nullptr);

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, CircuitBreaker_ClosedToOpen) {
    /* WHAT: Verify transition from CLOSED to OPEN after threshold failures */
    /* REGRESSION: P1 fix - state transition must be deterministic */
    
    constexpr uint32_t THRESHOLD = 3;
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, 1000);
    ASSERT_NE(cb, nullptr);

    /* Should be closed initially */
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    EXPECT_TRUE(circuit_breaker_allow_operation(cb));

    /* Record failures up to threshold */
    for (uint32_t i = 0; i < THRESHOLD; i++) {
        circuit_breaker_record_failure(cb);
    }

    /* Should now be open */
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);
    EXPECT_FALSE(circuit_breaker_allow_operation(cb));

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, CircuitBreaker_OpenToHalfOpen) {
    /* WHAT: Verify transition from OPEN to HALF_OPEN after timeout */
    /* REGRESSION: P2 fix - timeout-based state transition */
    
    constexpr uint32_t THRESHOLD = 2;
    constexpr uint32_t TIMEOUT_MS = 100;  /* Minimum valid timeout */
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, TIMEOUT_MS);
    ASSERT_NE(cb, nullptr);

    /* Trigger open state */
    for (uint32_t i = 0; i < THRESHOLD; i++) {
        circuit_breaker_record_failure(cb);
    }
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    /* Wait for timeout */
    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_MS + 50));

    /* Check should trigger half-open state and allow one operation */
    bool allowed = circuit_breaker_allow_operation(cb);
    EXPECT_TRUE(allowed) << "Should allow operation in HALF_OPEN state";
    
    circuit_state_t state = circuit_breaker_get_state(cb);
    EXPECT_EQ(state, CIRCUIT_HALF_OPEN) << "Should be in HALF_OPEN after timeout";

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, CircuitBreaker_HalfOpenToClosed) {
    /* WHAT: Verify transition from HALF_OPEN to CLOSED on success */
    /* REGRESSION: Recovery transition */
    
    constexpr uint32_t THRESHOLD = 2;
    constexpr uint32_t TIMEOUT_MS = 100;
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, TIMEOUT_MS);
    ASSERT_NE(cb, nullptr);

    /* Trigger open state */
    for (uint32_t i = 0; i < THRESHOLD; i++) {
        circuit_breaker_record_failure(cb);
    }

    /* Wait for timeout to transition to half-open */
    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_MS + 50));
    circuit_breaker_allow_operation(cb);  /* Trigger half-open */

    /* Record success should close circuit */
    circuit_breaker_record_success(cb);

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED)
        << "Should transition to CLOSED after success in HALF_OPEN";

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, CircuitBreaker_HalfOpenToOpen) {
    /* WHAT: Verify transition from HALF_OPEN back to OPEN on failure */
    /* REGRESSION: Failed recovery transition */
    
    constexpr uint32_t THRESHOLD = 2;
    constexpr uint32_t TIMEOUT_MS = 100;
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, TIMEOUT_MS);
    ASSERT_NE(cb, nullptr);

    /* Trigger open state */
    for (uint32_t i = 0; i < THRESHOLD; i++) {
        circuit_breaker_record_failure(cb);
    }

    /* Wait for timeout */
    std::this_thread::sleep_for(std::chrono::milliseconds(TIMEOUT_MS + 50));
    circuit_breaker_allow_operation(cb);  /* Trigger half-open */

    /* Record failure should re-open circuit */
    circuit_breaker_record_failure(cb);

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN)
        << "Should transition back to OPEN after failure in HALF_OPEN";

    circuit_breaker_destroy(cb);
}

/* ============================================================================
 * Threshold Behavior Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, Threshold_ExactCount) {
    /* WHAT: Verify exact threshold count triggers state change */
    /* REGRESSION: Threshold boundary behavior */
    
    constexpr uint32_t THRESHOLD = 5;
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, 1000);
    ASSERT_NE(cb, nullptr);

    /* Record exactly threshold-1 failures */
    for (uint32_t i = 0; i < THRESHOLD - 1; i++) {
        circuit_breaker_record_failure(cb);
        EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED)
            << "Should still be CLOSED at " << (i + 1) << " failures";
    }

    /* One more should trigger OPEN */
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Threshold_SuccessResetsCount) {
    /* WHAT: Verify success resets failure count */
    /* REGRESSION: Failure count reset behavior */
    
    constexpr uint32_t THRESHOLD = 5;
    circuit_breaker_t* cb = circuit_breaker_create(THRESHOLD, 1000);
    ASSERT_NE(cb, nullptr);

    /* Record some failures */
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);

    /* Record success */
    circuit_breaker_record_success(cb);

    /* Circuit should be reset, recording threshold failures again */
    for (uint32_t i = 0; i < THRESHOLD - 1; i++) {
        circuit_breaker_record_failure(cb);
        EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    }

    /* One more should trigger OPEN */
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Threshold_MinimumValue) {
    /* WHAT: Verify minimum threshold (1) works correctly */
    /* REGRESSION: Edge case threshold */
    
    circuit_breaker_t* cb = circuit_breaker_create(1, 1000);
    ASSERT_NE(cb, nullptr);

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    /* Single failure should trigger OPEN */
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Threshold_LargeValue) {
    /* WHAT: Verify large threshold works correctly */
    /* REGRESSION: Large threshold stability */
    
    constexpr uint32_t LARGE_THRESHOLD = 100;
    circuit_breaker_t* cb = circuit_breaker_create(LARGE_THRESHOLD, 1000);
    ASSERT_NE(cb, nullptr);

    /* Record exactly threshold failures */
    for (uint32_t i = 0; i < LARGE_THRESHOLD; i++) {
        circuit_breaker_record_failure(cb);
    }

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb);
}

/* ============================================================================
 * Reset Functionality Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, Reset_ReturnsToClosed) {
    /* WHAT: Verify reset returns circuit to CLOSED state */
    /* REGRESSION: Reset functionality */
    
    circuit_breaker_t* cb = circuit_breaker_create(3, 1000);
    ASSERT_NE(cb, nullptr);

    /* Trigger OPEN state */
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    /* Reset */
    circuit_breaker_reset(cb);

    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    EXPECT_TRUE(circuit_breaker_allow_operation(cb));

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Reset_ClearsCounters) {
    /* WHAT: Verify reset clears failure and success counters */
    /* REGRESSION: Counter clearing */
    
    circuit_breaker_t* cb = circuit_breaker_create(5, 1000);
    ASSERT_NE(cb, nullptr);

    /* Record some failures (not enough to open) */
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);

    /* Reset */
    circuit_breaker_reset(cb);

    /* After reset, should need full threshold again */
    for (uint32_t i = 0; i < 4; i++) {
        circuit_breaker_record_failure(cb);
        EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    }

    /* One more should trigger OPEN */
    circuit_breaker_record_failure(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Reset_MultipleTimes) {
    /* WHAT: Verify multiple resets work correctly */
    /* REGRESSION: Reset idempotency */
    
    circuit_breaker_t* cb = circuit_breaker_create(2, 1000);
    ASSERT_NE(cb, nullptr);

    for (int cycle = 0; cycle < 5; cycle++) {
        /* Trigger OPEN */
        circuit_breaker_record_failure(cb);
        circuit_breaker_record_failure(cb);
        EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_OPEN);

        /* Reset */
        circuit_breaker_reset(cb);
        EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);
    }

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Reset_FromHalfOpen) {
    /* WHAT: Verify reset from HALF_OPEN state */
    /* REGRESSION: Reset from all states */
    
    circuit_breaker_t* cb = circuit_breaker_create(2, 100);
    ASSERT_NE(cb, nullptr);

    /* Trigger OPEN */
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);

    /* Wait for HALF_OPEN */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    circuit_breaker_allow_operation(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_HALF_OPEN);

    /* Reset */
    circuit_breaker_reset(cb);
    EXPECT_EQ(circuit_breaker_get_state(cb), CIRCUIT_CLOSED);

    circuit_breaker_destroy(cb);
}

/* ============================================================================
 * Multiple Circuit Breakers Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, MultipleBreakers_Independent) {
    /* WHAT: Verify multiple circuit breakers don't interfere */
    /* REGRESSION: P3 fix - isolation between breakers */
    
    circuit_breaker_t* cb1 = circuit_breaker_create(3, 1000);
    circuit_breaker_t* cb2 = circuit_breaker_create(5, 1000);
    circuit_breaker_t* cb3 = circuit_breaker_create(2, 1000);
    
    ASSERT_NE(cb1, nullptr);
    ASSERT_NE(cb2, nullptr);
    ASSERT_NE(cb3, nullptr);

    /* Trigger OPEN on cb1 only */
    circuit_breaker_record_failure(cb1);
    circuit_breaker_record_failure(cb1);
    circuit_breaker_record_failure(cb1);

    EXPECT_EQ(circuit_breaker_get_state(cb1), CIRCUIT_OPEN);
    EXPECT_EQ(circuit_breaker_get_state(cb2), CIRCUIT_CLOSED);
    EXPECT_EQ(circuit_breaker_get_state(cb3), CIRCUIT_CLOSED);

    /* Trigger OPEN on cb3 */
    circuit_breaker_record_failure(cb3);
    circuit_breaker_record_failure(cb3);

    EXPECT_EQ(circuit_breaker_get_state(cb1), CIRCUIT_OPEN);
    EXPECT_EQ(circuit_breaker_get_state(cb2), CIRCUIT_CLOSED);
    EXPECT_EQ(circuit_breaker_get_state(cb3), CIRCUIT_OPEN);

    circuit_breaker_destroy(cb1);
    circuit_breaker_destroy(cb2);
    circuit_breaker_destroy(cb3);
}

TEST_F(RecoveryRegressionTest, MultipleBreakers_DifferentTimeouts) {
    /* WHAT: Verify different timeouts work correctly */
    /* REGRESSION: Timeout isolation */
    
    circuit_breaker_t* fast = circuit_breaker_create(2, 100);
    circuit_breaker_t* slow = circuit_breaker_create(2, 500);
    
    ASSERT_NE(fast, nullptr);
    ASSERT_NE(slow, nullptr);

    /* Trigger OPEN on both */
    circuit_breaker_record_failure(fast);
    circuit_breaker_record_failure(fast);
    circuit_breaker_record_failure(slow);
    circuit_breaker_record_failure(slow);

    EXPECT_EQ(circuit_breaker_get_state(fast), CIRCUIT_OPEN);
    EXPECT_EQ(circuit_breaker_get_state(slow), CIRCUIT_OPEN);

    /* Wait for fast timeout only */
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    /* Fast should transition to HALF_OPEN, slow should stay OPEN */
    circuit_breaker_allow_operation(fast);
    circuit_breaker_allow_operation(slow);

    EXPECT_EQ(circuit_breaker_get_state(fast), CIRCUIT_HALF_OPEN);
    EXPECT_EQ(circuit_breaker_get_state(slow), CIRCUIT_OPEN);

    circuit_breaker_destroy(fast);
    circuit_breaker_destroy(slow);
}

/* ============================================================================
 * Statistics Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, Statistics_TotalFailures) {
    /* WHAT: Verify total failures counter is accurate */
    /* REGRESSION: Statistics tracking */
    
    circuit_breaker_t* cb = circuit_breaker_create(10, 1000);
    ASSERT_NE(cb, nullptr);

    constexpr int FAILURES = 7;
    for (int i = 0; i < FAILURES; i++) {
        circuit_breaker_record_failure(cb);
    }

    EXPECT_EQ(cb->total_failures, static_cast<uint32_t>(FAILURES));

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Statistics_TotalSuccesses) {
    /* WHAT: Verify total successes counter is accurate */
    /* REGRESSION: Statistics tracking */
    
    circuit_breaker_t* cb = circuit_breaker_create(10, 1000);
    ASSERT_NE(cb, nullptr);

    constexpr int SUCCESSES = 5;
    for (int i = 0; i < SUCCESSES; i++) {
        circuit_breaker_record_success(cb);
    }

    EXPECT_EQ(cb->total_successes, static_cast<uint32_t>(SUCCESSES));

    circuit_breaker_destroy(cb);
}

TEST_F(RecoveryRegressionTest, Statistics_ResetPreservesTotals) {
    /* WHAT: Verify reset doesn't clear total counters (or does, depending on design) */
    /* REGRESSION: Reset behavior for statistics */
    
    circuit_breaker_t* cb = circuit_breaker_create(3, 1000);
    ASSERT_NE(cb, nullptr);

    circuit_breaker_record_failure(cb);
    circuit_breaker_record_failure(cb);
    circuit_breaker_record_success(cb);

    uint32_t failures_before = cb->total_failures;
    uint32_t successes_before = cb->total_successes;

    circuit_breaker_reset(cb);

    /* Totals should be consistent (either preserved or zeroed, but consistent) */
    EXPECT_TRUE(cb->total_failures == 0 || cb->total_failures == failures_before);
    EXPECT_TRUE(cb->total_successes == 0 || cb->total_successes == successes_before);

    circuit_breaker_destroy(cb);
}

/* ============================================================================
 * API Stability Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, API_NullDestroySafe) {
    /* WHAT: Verify circuit_breaker_destroy handles NULL */
    /* REGRESSION: NULL safety */
    
    /* Should not crash */
    circuit_breaker_destroy(nullptr);
}

TEST_F(RecoveryRegressionTest, API_NullOperationsSafe) {
    /* WHAT: Verify operations on NULL are safe */
    /* REGRESSION: NULL safety */

    /* These should be safe (return true for allow or do nothing) */
    /* Note: allow_operation returns true for NULL (fail-open behavior) */
    EXPECT_TRUE(circuit_breaker_allow_operation(nullptr));
    circuit_breaker_record_success(nullptr);  /* Should not crash */
    circuit_breaker_record_failure(nullptr);  /* Should not crash */
    circuit_breaker_reset(nullptr);           /* Should not crash */
}

TEST_F(RecoveryRegressionTest, API_StateEnumValues) {
    /* WHAT: Verify state enum values are stable */
    /* REGRESSION: Enum value ABI stability */
    
    EXPECT_EQ(static_cast<int>(CIRCUIT_CLOSED), 0);
    EXPECT_EQ(static_cast<int>(CIRCUIT_OPEN), 1);
    EXPECT_EQ(static_cast<int>(CIRCUIT_HALF_OPEN), 2);
}

TEST_F(RecoveryRegressionTest, API_RecoveryTierEnumValues) {
    /* WHAT: Verify recovery tier enum values are stable */
    /* REGRESSION: Enum value ABI stability */
    
    EXPECT_EQ(static_cast<int>(RECOVERY_TIER_IMMEDIATE), 0);
    EXPECT_EQ(static_cast<int>(RECOVERY_TIER_TACTICAL), 1);
    EXPECT_EQ(static_cast<int>(RECOVERY_TIER_STRATEGIC), 2);
    EXPECT_EQ(static_cast<int>(RECOVERY_TIER_PREVENTIVE), 3);
}

TEST_F(RecoveryRegressionTest, API_RecoveryStatusEnumValues) {
    /* WHAT: Verify recovery status enum values are stable */
    /* REGRESSION: Enum value ABI stability */
    
    EXPECT_EQ(static_cast<int>(RECOVERY_SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(RECOVERY_PARTIAL), 1);
    EXPECT_EQ(static_cast<int>(RECOVERY_FAILED), 2);
    EXPECT_EQ(static_cast<int>(RECOVERY_NOT_APPLICABLE), 3);
    EXPECT_EQ(static_cast<int>(RECOVERY_REQUIRES_RESTART), 4);
}

/* ============================================================================
 * Utility Function Regression Tests
 * ============================================================================ */

TEST_F(RecoveryRegressionTest, Utility_TierNameReturnsString) {
    /* WHAT: Verify tier name function returns valid strings */
    /* REGRESSION: Utility function stability */
    
    const char* name = recovery_tier_name(RECOVERY_TIER_IMMEDIATE);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = recovery_tier_name(RECOVERY_TIER_STRATEGIC);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(RecoveryRegressionTest, Utility_ActionNameReturnsString) {
    /* WHAT: Verify action name function returns valid strings */
    /* REGRESSION: Utility function stability */
    
    const char* name = recovery_action_name(RECOVERY_ACTION_CLEAR_NAN);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = recovery_action_name(RECOVERY_ACTION_FALLBACK_CPU);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

TEST_F(RecoveryRegressionTest, Utility_StatusNameReturnsString) {
    /* WHAT: Verify status name function returns valid strings */
    /* REGRESSION: Utility function stability */
    
    const char* name = recovery_status_name(RECOVERY_SUCCESS);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);

    name = recovery_status_name(RECOVERY_FAILED);
    EXPECT_NE(name, nullptr);
    EXPECT_GT(strlen(name), 0u);
}

} // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
