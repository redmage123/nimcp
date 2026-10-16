/**
 * @file test_recovery_e2e.cpp
 * @brief End-to-End Tests for Circuit Breaker and Retry Framework
 *
 * WHAT: Full workflow E2E tests for fault tolerance recovery mechanisms
 * WHY:  Verify circuit breaker and retry framework work correctly together
 * HOW:  Test complete workflows: failures -> circuit open -> half-open -> recovery
 *
 * TEST PIPELINES:
 * - CircuitBreakerStateTransitions: Test CLOSED -> OPEN -> HALF_OPEN -> CLOSED
 * - RetryWithExponentialBackoff: Verify retry delays increase exponentially
 * - CircuitBreakerAndRetryIntegration: Combined circuit breaker + retry workflow
 * - CircuitBreakerResetAfterTimeout: Verify half-open state after timeout
 * - MultipleCircuitBreakers: Independent operation of multiple breakers
 * - RetryWithRollback: Verify rollback is called on exhausted retries
 * - CircuitBreakerStatistics: Verify failure/success tracking
 * - RecoveryUnderConcurrentLoad: Thread-safe operation verification
 *
 * @author NIMCP Development Team
 * @date 2026-02-05
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

extern "C" {
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "utils/fault_tolerance/nimcp_retry.h"
#include "utils/error/nimcp_error_codes.h"
}

#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <cstring>

//=============================================================================
// Test Fixture
//=============================================================================

class RecoveryE2ETest : public ::testing::Test {
protected:
    circuit_breaker_t* breaker_ = nullptr;

    // Counters for operation tracking
    static std::atomic<int> execute_count_;
    static std::atomic<int> rollback_count_;
    static std::atomic<int> success_count_;
    static std::atomic<int> failure_count_;
    static std::atomic<bool> should_succeed_;

    void SetUp() override {
        execute_count_.store(0);
        rollback_count_.store(0);
        success_count_.store(0);
        failure_count_.store(0);
        should_succeed_.store(false);
    }

    void TearDown() override {
        if (breaker_) {
            circuit_breaker_destroy(breaker_);
            breaker_ = nullptr;
        }
    }

    // Execute function that can be controlled to succeed or fail
    static bool controllable_execute(void* context) {
        (void)context;
        execute_count_.fetch_add(1);
        if (should_succeed_.load()) {
            success_count_.fetch_add(1);
            return true;
        }
        failure_count_.fetch_add(1);
        return false;
    }

    // Execute function that always fails
    static bool always_fail(void* context) {
        (void)context;
        execute_count_.fetch_add(1);
        failure_count_.fetch_add(1);
        return false;
    }

    // Execute function that always succeeds
    static bool always_succeed(void* context) {
        (void)context;
        execute_count_.fetch_add(1);
        success_count_.fetch_add(1);
        return true;
    }

    // Execute function that fails N times then succeeds
    static bool fail_then_succeed(void* context) {
        int* fail_count = static_cast<int*>(context);
        execute_count_.fetch_add(1);
        if (*fail_count > 0) {
            (*fail_count)--;
            failure_count_.fetch_add(1);
            return false;
        }
        success_count_.fetch_add(1);
        return true;
    }

    // Rollback function
    static void test_rollback(void* context) {
        (void)context;
        rollback_count_.fetch_add(1);
    }

    // Helper to wait for circuit breaker timeout
    void wait_for_timeout(uint32_t timeout_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms + 50));
    }
};

// Static member initialization
std::atomic<int> RecoveryE2ETest::execute_count_{0};
std::atomic<int> RecoveryE2ETest::rollback_count_{0};
std::atomic<int> RecoveryE2ETest::success_count_{0};
std::atomic<int> RecoveryE2ETest::failure_count_{0};
std::atomic<bool> RecoveryE2ETest::should_succeed_{false};

//=============================================================================
// Test 1: Circuit Breaker State Transitions
//=============================================================================

TEST_F(RecoveryE2ETest, CircuitBreakerStateTransitions) {
    E2E_PIPELINE_START("Circuit Breaker State Transitions");

    // Stage 1: Create circuit breaker
    E2E_STAGE_BEGIN("Create circuit breaker", 100);
    const uint32_t failure_threshold = 3;
    const uint32_t timeout_ms = 200;
    breaker_ = circuit_breaker_create(failure_threshold, timeout_ms);
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    // Stage 2: Verify initial CLOSED state allows operations
    E2E_STAGE_BEGIN("Verify CLOSED state", 100);
    EXPECT_TRUE(circuit_breaker_allow_operation(breaker_));
    circuit_breaker_record_success(breaker_);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    // Stage 3: Record failures to open circuit
    E2E_STAGE_BEGIN("Trigger circuit OPEN", 200);
    for (uint32_t i = 0; i < failure_threshold; i++) {
        EXPECT_TRUE(circuit_breaker_allow_operation(breaker_));
        circuit_breaker_record_failure(breaker_);
    }
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_OPEN);
    E2E_STAGE_END();

    // Stage 4: Verify OPEN state blocks operations
    E2E_STAGE_BEGIN("Verify OPEN blocks operations", 100);
    EXPECT_FALSE(circuit_breaker_allow_operation(breaker_));
    E2E_STAGE_END();

    // Stage 5: Wait for timeout and verify HALF_OPEN
    E2E_STAGE_BEGIN("Wait for HALF_OPEN state", 500);
    wait_for_timeout(timeout_ms);
    EXPECT_TRUE(circuit_breaker_allow_operation(breaker_));
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_HALF_OPEN);
    E2E_STAGE_END();

    // Stage 6: Success in HALF_OPEN closes circuit
    E2E_STAGE_BEGIN("Success closes circuit", 100);
    circuit_breaker_record_success(breaker_);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 2: Retry with Exponential Backoff
//=============================================================================

TEST_F(RecoveryE2ETest, RetryWithExponentialBackoff) {
    E2E_PIPELINE_START("Retry with Exponential Backoff");

    // Stage 1: Configure retry with short delays for testing
    E2E_STAGE_BEGIN("Configure retry", 100);
    nimcp_retry_config_t config = nimcp_retry_default_config();
    config.max_retries = 4;
    config.initial_delay_ms = 10;
    config.max_delay_ms = 200;
    config.backoff_factor = 2.0f;
    config.jitter_factor = 0.0f;  // Disable jitter for predictable timing
    E2E_STAGE_END();

    // Stage 2: Create operation that fails twice then succeeds
    E2E_STAGE_BEGIN("Setup operation", 100);
    int fail_count = 2;
    operation_t op = {
        .name = "retry_test_op",
        .execute = fail_then_succeed,
        .rollback = nullptr,
        .context = &fail_count,
        .execution_count = 0
    };
    E2E_STAGE_END();

    // Stage 3: Execute retry and measure timing
    E2E_STAGE_BEGIN("Execute retry with backoff", 2000);
    auto start = std::chrono::steady_clock::now();
    
    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&op, &config, nullptr, &result);
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.attempts, 3u);  // 2 failures + 1 success
    
    // Expected delay: 10ms (after 1st fail) + 20ms (after 2nd fail) = 30ms minimum
    EXPECT_GE(elapsed, 25);  // Allow some timing variance
    E2E_STAGE_END();

    // Stage 4: Verify execution count
    E2E_STAGE_BEGIN("Verify execution count", 100);
    EXPECT_EQ(execute_count_.load(), 3);
    EXPECT_EQ(success_count_.load(), 1);
    EXPECT_EQ(failure_count_.load(), 2);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 3: Circuit Breaker and Retry Integration
//=============================================================================

TEST_F(RecoveryE2ETest, CircuitBreakerAndRetryIntegration) {
    E2E_PIPELINE_START("Circuit Breaker and Retry Integration");

    // Stage 1: Create circuit breaker with low threshold
    E2E_STAGE_BEGIN("Create circuit breaker", 100);
    breaker_ = circuit_breaker_create(3, 300);
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    E2E_STAGE_END();

    // Stage 2: Configure retry
    E2E_STAGE_BEGIN("Configure retry", 100);
    nimcp_retry_config_t config = nimcp_retry_default_config();
    config.max_retries = 5;
    config.initial_delay_ms = 5;
    config.jitter_factor = 0.0f;
    E2E_STAGE_END();

    // Stage 3: First retry sequence - will open circuit
    E2E_STAGE_BEGIN("Retry until circuit opens", 1000);
    operation_t fail_op = {
        .name = "failing_op",
        .execute = always_fail,
        .rollback = nullptr,
        .context = nullptr,
        .execution_count = 0
    };
    
    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&fail_op, &config, breaker_, &result);
    
    // Should stop when circuit opens (after 3 failures) not after max_retries
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_STATE);  // Circuit is open
    EXPECT_FALSE(result.success);
    EXPECT_LE(result.attempts, 4u);  // 3 failures + possible 1 blocked
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_OPEN);
    E2E_STAGE_END();

    // Stage 4: Operations blocked while circuit is open
    E2E_STAGE_BEGIN("Verify operations blocked", 100);
    execute_count_.store(0);
    err = nimcp_retry_with_backoff(&fail_op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(execute_count_.load(), 0);  // No execution attempted
    E2E_STAGE_END();

    // Stage 5: Wait for half-open and recover
    E2E_STAGE_BEGIN("Recovery in half-open state", 600);
    wait_for_timeout(300);
    
    // Now use a successful operation
    should_succeed_.store(true);
    operation_t success_op = {
        .name = "success_op",
        .execute = controllable_execute,
        .rollback = nullptr,
        .context = nullptr,
        .execution_count = 0
    };
    
    execute_count_.store(0);
    err = nimcp_retry_with_backoff(&success_op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 4: Circuit Breaker Reset After Timeout
//=============================================================================

TEST_F(RecoveryE2ETest, CircuitBreakerResetAfterTimeout) {
    E2E_PIPELINE_START("Circuit Breaker Reset After Timeout");

    const uint32_t timeout_ms = 150;

    // Stage 1: Create and open circuit
    E2E_STAGE_BEGIN("Create and open circuit", 200);
    breaker_ = circuit_breaker_create(2, timeout_ms);
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    
    circuit_breaker_record_failure(breaker_);
    circuit_breaker_record_failure(breaker_);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_OPEN);
    E2E_STAGE_END();

    // Stage 2: Verify blocked before timeout
    E2E_STAGE_BEGIN("Verify blocked before timeout", 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(circuit_breaker_allow_operation(breaker_));
    E2E_STAGE_END();

    // Stage 3: Wait for timeout
    E2E_STAGE_BEGIN("Wait for timeout", 300);
    wait_for_timeout(timeout_ms);
    E2E_STAGE_END();

    // Stage 4: Verify half-open allows test operation
    E2E_STAGE_BEGIN("Verify half-open state", 100);
    EXPECT_TRUE(circuit_breaker_allow_operation(breaker_));
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_HALF_OPEN);
    E2E_STAGE_END();

    // Stage 5: Failure in half-open reopens circuit
    E2E_STAGE_BEGIN("Failure reopens circuit", 100);
    circuit_breaker_record_failure(breaker_);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_OPEN);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 5: Multiple Independent Circuit Breakers
//=============================================================================

TEST_F(RecoveryE2ETest, MultipleCircuitBreakers) {
    E2E_PIPELINE_START("Multiple Independent Circuit Breakers");

    circuit_breaker_t* breaker1 = nullptr;
    circuit_breaker_t* breaker2 = nullptr;

    // Stage 1: Create two independent breakers
    E2E_STAGE_BEGIN("Create two breakers", 100);
    breaker1 = circuit_breaker_create(2, 200);
    breaker2 = circuit_breaker_create(3, 300);
    E2E_ASSERT_NOT_NULL(breaker1, "Failed to create breaker1");
    E2E_ASSERT_NOT_NULL(breaker2, "Failed to create breaker2");
    E2E_STAGE_END();

    // Stage 2: Open breaker1 only
    E2E_STAGE_BEGIN("Open breaker1 only", 100);
    circuit_breaker_record_failure(breaker1);
    circuit_breaker_record_failure(breaker1);
    EXPECT_EQ(circuit_breaker_get_state(breaker1), CIRCUIT_OPEN);
    EXPECT_EQ(circuit_breaker_get_state(breaker2), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    // Stage 3: Breaker2 still allows operations
    E2E_STAGE_BEGIN("Breaker2 still operational", 100);
    EXPECT_FALSE(circuit_breaker_allow_operation(breaker1));
    EXPECT_TRUE(circuit_breaker_allow_operation(breaker2));
    E2E_STAGE_END();

    // Stage 4: Open breaker2
    E2E_STAGE_BEGIN("Open breaker2", 100);
    circuit_breaker_record_failure(breaker2);
    circuit_breaker_record_failure(breaker2);
    circuit_breaker_record_failure(breaker2);
    EXPECT_EQ(circuit_breaker_get_state(breaker2), CIRCUIT_OPEN);
    E2E_STAGE_END();

    // Stage 5: Both blocked
    E2E_STAGE_BEGIN("Both breakers blocked", 100);
    EXPECT_FALSE(circuit_breaker_allow_operation(breaker1));
    EXPECT_FALSE(circuit_breaker_allow_operation(breaker2));
    E2E_STAGE_END();

    // Cleanup
    circuit_breaker_destroy(breaker1);
    circuit_breaker_destroy(breaker2);

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 6: Retry with Rollback
//=============================================================================

TEST_F(RecoveryE2ETest, RetryWithRollback) {
    E2E_PIPELINE_START("Retry with Rollback");

    // Stage 1: Configure retry with rollback
    E2E_STAGE_BEGIN("Configure retry with rollback", 100);
    nimcp_retry_config_t config = nimcp_retry_default_config();
    config.max_retries = 3;
    config.initial_delay_ms = 5;
    config.jitter_factor = 0.0f;
    
    operation_t op = {
        .name = "rollback_test",
        .execute = always_fail,
        .rollback = test_rollback,
        .context = nullptr,
        .execution_count = 0
    };
    E2E_STAGE_END();

    // Stage 2: Execute retry - all attempts fail
    E2E_STAGE_BEGIN("Execute failing retry", 500);
    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&op, &config, nullptr, &result);
    
    EXPECT_EQ(err, NIMCP_ERROR_OPERATION_FAILED);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.attempts, 4u);  // 1 initial + 3 retries
    E2E_STAGE_END();

    // Stage 3: Verify rollback was called exactly once
    E2E_STAGE_BEGIN("Verify rollback called", 100);
    EXPECT_EQ(rollback_count_.load(), 1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 7: Circuit Breaker Statistics
//=============================================================================

TEST_F(RecoveryE2ETest, CircuitBreakerStatistics) {
    E2E_PIPELINE_START("Circuit Breaker Statistics");

    // Stage 1: Create breaker and perform mixed operations
    E2E_STAGE_BEGIN("Create breaker", 100);
    breaker_ = circuit_breaker_create(10, 500);  // High threshold for testing
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    E2E_STAGE_END();

    // Stage 2: Record mixed successes and failures
    E2E_STAGE_BEGIN("Record mixed results", 200);
    for (int i = 0; i < 5; i++) {
        circuit_breaker_record_success(breaker_);
    }
    for (int i = 0; i < 3; i++) {
        circuit_breaker_record_failure(breaker_);
    }
    for (int i = 0; i < 2; i++) {
        circuit_breaker_record_success(breaker_);
    }
    E2E_STAGE_END();

    // Stage 3: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 100);
    EXPECT_EQ(breaker_->total_successes, 7u);
    EXPECT_EQ(breaker_->total_failures, 3u);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);  // Didn't hit threshold
    E2E_STAGE_END();

    // Stage 4: Reset and verify
    E2E_STAGE_BEGIN("Reset and verify", 100);
    circuit_breaker_reset(breaker_);
    EXPECT_EQ(breaker_->failure_count, 0u);
    EXPECT_EQ(breaker_->success_count, 0u);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 8: Recovery Under Concurrent Load
//=============================================================================

TEST_F(RecoveryE2ETest, RecoveryUnderConcurrentLoad) {
    E2E_PIPELINE_START("Recovery Under Concurrent Load");

    // Stage 1: Create shared circuit breaker
    E2E_STAGE_BEGIN("Create shared circuit breaker", 100);
    breaker_ = circuit_breaker_create(50, 500);  // High threshold
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    E2E_STAGE_END();

    // Stage 2: Launch concurrent operations
    E2E_STAGE_BEGIN("Launch concurrent operations", 2000);
    const int num_threads = 4;
    const int ops_per_thread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> total_allowed{0};
    std::atomic<int> total_blocked{0};
    
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; i++) {
                if (circuit_breaker_allow_operation(breaker_)) {
                    total_allowed.fetch_add(1);
                    // Alternate success/failure
                    if ((t + i) % 3 == 0) {
                        circuit_breaker_record_failure(breaker_);
                    } else {
                        circuit_breaker_record_success(breaker_);
                    }
                } else {
                    total_blocked.fetch_add(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    E2E_STAGE_END();

    // Stage 3: Verify consistency
    E2E_STAGE_BEGIN("Verify consistency", 100);
    int total_ops = total_allowed.load() + total_blocked.load();
    EXPECT_EQ(total_ops, num_threads * ops_per_thread);
    
    // Statistics should be consistent
    uint32_t stats_total = breaker_->total_successes + breaker_->total_failures;
    EXPECT_EQ(stats_total, (uint32_t)total_allowed.load());
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 9: Retry Callback Invocation
//=============================================================================

TEST_F(RecoveryE2ETest, RetryCallbackInvocation) {
    E2E_PIPELINE_START("Retry Callback Invocation");

    static std::vector<std::pair<uint32_t, uint32_t>> callback_log;
    callback_log.clear();

    // Stage 1: Configure retry with callback
    E2E_STAGE_BEGIN("Configure retry with callback", 100);
    nimcp_retry_config_t config = nimcp_retry_default_config();
    config.max_retries = 3;
    config.initial_delay_ms = 10;
    config.jitter_factor = 0.0f;
    config.on_retry = [](uint32_t attempt, uint32_t delay_ms, void* ctx) {
        (void)ctx;
        callback_log.push_back({attempt, delay_ms});
    };
    E2E_STAGE_END();

    // Stage 2: Execute retry
    E2E_STAGE_BEGIN("Execute retry", 500);
    int fail_count = 2;
    operation_t op = {
        .name = "callback_test",
        .execute = fail_then_succeed,
        .rollback = nullptr,
        .context = &fail_count,
        .execution_count = 0
    };
    
    nimcp_retry_result_t result;
    nimcp_retry_with_backoff(&op, &config, nullptr, &result);
    EXPECT_TRUE(result.success);
    E2E_STAGE_END();

    // Stage 3: Verify callback log
    E2E_STAGE_BEGIN("Verify callback log", 100);
    EXPECT_EQ(callback_log.size(), 2u);  // Called before 2nd and 3rd attempts
    if (callback_log.size() >= 2) {
        // Note: attempt is 0-based as documented in nimcp_retry.h
        EXPECT_EQ(callback_log[0].first, 0u);  // Attempt 0 (first failure)
        EXPECT_EQ(callback_log[0].second, 10u);  // First delay
        EXPECT_EQ(callback_log[1].first, 1u);  // Attempt 1 (second failure)
        EXPECT_EQ(callback_log[1].second, 20u);  // Second delay (2x)
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Test 10: Full Recovery Workflow
//=============================================================================

TEST_F(RecoveryE2ETest, FullRecoveryWorkflow) {
    E2E_PIPELINE_START("Full Recovery Workflow");

    // Stage 1: Setup infrastructure
    E2E_STAGE_BEGIN("Setup infrastructure", 100);
    breaker_ = circuit_breaker_create(3, 200);
    E2E_ASSERT_NOT_NULL(breaker_, "Failed to create circuit breaker");
    
    nimcp_retry_config_t config = nimcp_retry_default_config();
    config.max_retries = 5;
    config.initial_delay_ms = 10;
    config.jitter_factor = 0.0f;
    E2E_STAGE_END();

    // Stage 2: Normal operation succeeds
    E2E_STAGE_BEGIN("Normal operation", 200);
    should_succeed_.store(true);
    operation_t op = {
        .name = "workflow_op",
        .execute = controllable_execute,
        .rollback = test_rollback,
        .context = nullptr,
        .execution_count = 0
    };
    
    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.attempts, 1u);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    // Stage 3: Service degradation - failures occur
    E2E_STAGE_BEGIN("Service degradation", 500);
    should_succeed_.store(false);
    
    err = nimcp_retry_with_backoff(&op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_STATE);  // Circuit opened
    EXPECT_FALSE(result.success);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_OPEN);
    E2E_STAGE_END();

    // Stage 4: Operations blocked during outage
    E2E_STAGE_BEGIN("Operations blocked", 100);
    execute_count_.store(0);
    err = nimcp_retry_with_backoff(&op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_STATE);
    EXPECT_EQ(execute_count_.load(), 0);  // No attempts made
    E2E_STAGE_END();

    // Stage 5: Service recovery
    E2E_STAGE_BEGIN("Service recovery", 500);
    wait_for_timeout(200);
    should_succeed_.store(true);
    
    err = nimcp_retry_with_backoff(&op, &config, breaker_, &result);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    // Stage 6: Verify system stable
    E2E_STAGE_BEGIN("Verify stability", 200);
    for (int i = 0; i < 5; i++) {
        err = nimcp_retry_with_backoff(&op, &config, breaker_, &result);
        EXPECT_EQ(err, NIMCP_OK);
        EXPECT_TRUE(result.success);
    }
    EXPECT_EQ(circuit_breaker_get_state(breaker_), CIRCUIT_CLOSED);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
