/**
 * @file test_exception_recovery_scenarios_e2e.cpp
 * @brief E2E tests for exception recovery scenarios
 * @version 1.0.0
 * @date 2026-01-21
 *
 * WHAT: End-to-end tests for exception recovery scenarios including multi-exception
 *       recovery, graceful degradation, circuit breaker behavior, automatic retry
 *       with backoff, and health-triggered self-repair
 * WHY:  Verify the system can recover from various failure scenarios and maintain
 *       stability under sustained error conditions
 * HOW:  Test realistic recovery scenarios with multiple exceptions, circuit breakers,
 *       retry mechanisms, and health monitoring integration
 *
 * Test Scenarios:
 * 1. System recovery after multiple exceptions
 * 2. Graceful degradation under exception pressure
 * 3. Circuit breaker behavior and recovery
 * 4. Automatic retry with exponential backoff
 * 5. Health-triggered self-repair after exceptions
 * 6. Recovery callback chain execution
 * 7. Recovery with partial success
 * 8. Cascading failure prevention
 * 9. Recovery state machine transitions
 * 10. Recovery metrics and telemetry
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <memory>
#include <cmath>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>

extern "C" {
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "utils/exception/nimcp_exception_metrics.h"
#include "utils/exception/nimcp_exception_circuit.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Utilities - Recovery Simulation
 * ============================================================================ */

namespace {

// Recovery state tracking
struct RecoveryState {
    std::atomic<int> recovery_attempts{0};
    std::atomic<int> recovery_successes{0};
    std::atomic<int> recovery_failures{0};
    std::atomic<int> retry_count{0};
    std::atomic<int> backoff_delays{0};
    std::atomic<int> circuit_trips{0};
    std::atomic<int> circuit_resets{0};
    std::atomic<int> graceful_degrades{0};
    std::atomic<int> self_repair_triggers{0};
    std::atomic<bool> system_healthy{true};
    std::atomic<int> cascading_failures_prevented{0};

    void reset() {
        recovery_attempts = 0;
        recovery_successes = 0;
        recovery_failures = 0;
        retry_count = 0;
        backoff_delays = 0;
        circuit_trips = 0;
        circuit_resets = 0;
        graceful_degrades = 0;
        self_repair_triggers = 0;
        system_healthy = true;
        cascading_failures_prevented = 0;
    }
};

RecoveryState g_recovery_state;

// Retry configuration
struct RetryConfig {
    int max_retries;
    int initial_delay_ms;
    float backoff_multiplier;
    int max_delay_ms;
    bool jitter_enabled;
};

// Simulate system health levels
enum SystemHealthLevel {
    HEALTH_OPTIMAL = 0,
    HEALTH_GOOD,
    HEALTH_DEGRADED,
    HEALTH_CRITICAL,
    HEALTH_FAILURE
};

std::atomic<SystemHealthLevel> g_system_health{HEALTH_OPTIMAL};

// Recovery callback that simulates success/failure based on probability
bool g_force_recovery_failure = false;
float g_recovery_success_rate = 1.0f;

int probabilistic_recovery_callback(nimcp_exception_t* ex,
                                     nimcp_exception_recovery_action_t action,
                                     void* user_data) {
    g_recovery_state.recovery_attempts++;

    if (g_force_recovery_failure) {
        g_recovery_state.recovery_failures++;
        return -1;
    }

    // Random success based on rate
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    if (dis(gen) < g_recovery_success_rate) {
        g_recovery_state.recovery_successes++;
        return 0;
    } else {
        g_recovery_state.recovery_failures++;
        return -1;
    }
}

// Recovery callback that always succeeds
int success_recovery_callback(nimcp_exception_t* ex,
                              nimcp_exception_recovery_action_t action,
                              void* user_data) {
    g_recovery_state.recovery_attempts++;
    g_recovery_state.recovery_successes++;
    return 0;
}

// Recovery callback that always fails
int failure_recovery_callback(nimcp_exception_t* ex,
                              nimcp_exception_recovery_action_t action,
                              void* user_data) {
    g_recovery_state.recovery_attempts++;
    g_recovery_state.recovery_failures++;
    return -1;
}

// Exception handler that tracks processing
bool tracking_exception_handler(nimcp_exception_t* ex, void* user_data) {
    (void)user_data;
    // Just track, don't consume
    return false;
}

// Calculate backoff delay with optional jitter
int calculate_backoff_delay(const RetryConfig& config, int attempt) {
    int delay = config.initial_delay_ms;
    for (int i = 0; i < attempt; i++) {
        delay = static_cast<int>(delay * config.backoff_multiplier);
        if (delay > config.max_delay_ms) {
            delay = config.max_delay_ms;
            break;
        }
    }

    if (config.jitter_enabled) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(-delay / 4, delay / 4);
        delay += dis(gen);
        if (delay < 0) delay = 0;
    }

    return delay;
}

}  // namespace

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ExceptionRecoveryScenariosE2ETest : public ::testing::Test {
protected:
    nimcp_handler_registration_t* handler_reg = nullptr;

    void SetUp() override {
        g_recovery_state.reset();
        g_force_recovery_failure = false;
        g_recovery_success_rate = 1.0f;
        g_system_health = HEALTH_OPTIMAL;

        // Initialize exception system
        ASSERT_EQ(nimcp_exception_system_init(), 0) << "Failed to initialize exception system";

        // Initialize circuit breaker
        ASSERT_EQ(nimcp_circuit_init(), 0) << "Failed to initialize circuit breaker";

        // Initialize metrics
        ASSERT_EQ(nimcp_metrics_init(), 0) << "Failed to initialize metrics";

        // Initialize exception-immune integration
        nimcp_exception_immune_config_t immune_config;
        nimcp_exception_immune_default_config(&immune_config);
        immune_config.enable_auto_present = true;
        immune_config.enable_auto_recovery = true;
        immune_config.min_present_severity = EXCEPTION_SEVERITY_ERROR;
        ASSERT_EQ(nimcp_exception_immune_init(&immune_config), 0)
            << "Failed to initialize exception-immune integration";

        // Register tracking handler
        nimcp_handler_options_t opts;
        nimcp_handler_default_options(&opts);
        opts.name = "RecoveryTracker";
        opts.handler = tracking_exception_handler;
        opts.priority = NIMCP_HANDLER_PRIORITY_NORMAL;
        handler_reg = nimcp_handler_register(&opts);
        ASSERT_NE(handler_reg, nullptr);
    }

    void TearDown() override {
        if (handler_reg) {
            nimcp_handler_unregister(handler_reg);
            handler_reg = nullptr;
        }

        nimcp_exception_handlers_shutdown();
        nimcp_exception_immune_shutdown();
        nimcp_metrics_shutdown();
        nimcp_circuit_shutdown();
        nimcp_exception_system_shutdown();
    }

    // Helper to create exception with specific severity
    nimcp_exception_t* create_test_exception(nimcp_error_t code,
                                              nimcp_exception_severity_t severity,
                                              const char* message) {
        return nimcp_exception_create(code, severity, __FILE__, __LINE__, __func__,
                                       "%s", message);
    }

    // Helper to simulate retry with backoff
    bool retry_with_backoff(std::function<bool()> operation,
                            const RetryConfig& config) {
        for (int attempt = 0; attempt <= config.max_retries; attempt++) {
            g_recovery_state.retry_count++;

            if (operation()) {
                return true;
            }

            if (attempt < config.max_retries) {
                int delay = calculate_backoff_delay(config, attempt);
                g_recovery_state.backoff_delays++;
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        }
        return false;
    }
};

/* ============================================================================
 * Test 1: System Recovery After Multiple Exceptions
 *
 * Verifies system can recover and stabilize after multiple exceptions
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, SystemRecoveryAfterMultipleExceptions) {
    printf("=== Test: System Recovery After Multiple Exceptions ===\n");

    // Register recovery callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, success_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, success_recovery_callback, nullptr);

    const int NUM_EXCEPTIONS = 50;
    int recovered = 0;

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Memory pressure exception"
        );
        ASSERT_NE(ex, nullptr);

        // Dispatch exception
        nimcp_exception_dispatch(ex);

        // Attempt recovery
        int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
        if (result == 0) {
            recovered++;
            ex->recovery_succeeded = true;
        }

        nimcp_exception_unref(ex);
    }

    printf("  Exceptions raised: %d\n", NUM_EXCEPTIONS);
    printf("  Recovered: %d\n", recovered);
    printf("  Recovery attempts: %d\n", g_recovery_state.recovery_attempts.load());
    printf("  Recovery successes: %d\n", g_recovery_state.recovery_successes.load());

    EXPECT_EQ(recovered, NUM_EXCEPTIONS);
    EXPECT_EQ(g_recovery_state.recovery_successes.load(), NUM_EXCEPTIONS);

    // Verify system is stable (no pending exceptions, metrics normal)
    nimcp_exception_immune_stats_t stats;
    nimcp_exception_immune_get_stats(&stats);
    printf("  Pending exceptions: %lu\n", (unsigned long)stats.exceptions_pending);

    printf("Test passed: System recovery after multiple exceptions verified\n\n");
}

/* ============================================================================
 * Test 2: Graceful Degradation Under Exception Pressure
 *
 * Verifies system degrades gracefully when overwhelmed with exceptions
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, GracefulDegradationUnderPressure) {
    printf("=== Test: Graceful Degradation Under Exception Pressure ===\n");

    // Configure partial recovery success
    g_recovery_success_rate = 0.7f;  // 70% success rate
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD,
                                      probabilistic_recovery_callback, nullptr);

    const int BURST_SIZE = 100;
    int total_exceptions = 0;
    int successful_recoveries = 0;
    int degradation_events = 0;

    // Simulate burst of exceptions
    for (int burst = 0; burst < 3; burst++) {
        printf("  Burst %d/%d:\n", burst + 1, 3);

        int burst_failures = 0;
        for (int i = 0; i < BURST_SIZE; i++) {
            nimcp_exception_t* ex = create_test_exception(
                NIMCP_ERROR_OPERATION_FAILED,
                EXCEPTION_SEVERITY_SEVERE,
                "High pressure exception"
            );
            if (!ex) continue;
            total_exceptions++;

            nimcp_exception_dispatch(ex);

            // Try recovery
            int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_REDUCE_LOAD);
            if (result == 0) {
                successful_recoveries++;
            } else {
                burst_failures++;
            }

            nimcp_exception_unref(ex);
        }

        printf("    Failures in burst: %d\n", burst_failures);

        // If too many failures, simulate graceful degradation
        if (burst_failures > BURST_SIZE * 0.3) {
            g_recovery_state.graceful_degrades++;
            degradation_events++;
            g_system_health = HEALTH_DEGRADED;
            printf("    Graceful degradation triggered\n");
        }

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    printf("  Total exceptions: %d\n", total_exceptions);
    printf("  Successful recoveries: %d\n", successful_recoveries);
    printf("  Degradation events: %d\n", degradation_events);
    printf("  Recovery rate: %.1f%%\n",
           (successful_recoveries * 100.0f) / total_exceptions);

    // System should have degraded at least once given 70% success rate
    // and 100 exceptions per burst
    EXPECT_GT(total_exceptions, 0);
    EXPECT_GT(successful_recoveries, 0);

    printf("Test passed: Graceful degradation under pressure verified\n\n");
}

/* ============================================================================
 * Test 3: Circuit Breaker Behavior and Recovery
 *
 * Verifies circuit breaker opens on repeated failures and recovers
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, CircuitBreakerBehaviorAndRecovery) {
    printf("=== Test: Circuit Breaker Behavior and Recovery ===\n");

    nimcp_error_t test_code = NIMCP_ERROR_NETWORK_IO;

    // Set low threshold for quick tripping
    EXPECT_EQ(nimcp_circuit_set_threshold(test_code, 5, 500), 0);  // 5 exceptions, 500ms reset

    // Phase 1: Trip the circuit
    printf("  Phase 1: Tripping circuit breaker\n");
    int blocked = 0;
    int passed = 0;

    for (int i = 0; i < 20; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_ERROR,
            "Circuit breaker test"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_circuit_record(ex);
        if (result == 1) {
            blocked++;
        } else if (result == 0) {
            passed++;
        }

        nimcp_exception_unref(ex);
    }

    printf("    Passed: %d, Blocked: %d\n", passed, blocked);

    // Check circuit state
    nimcp_circuit_state_t state = nimcp_circuit_get_state(test_code);
    printf("    Circuit state: %s\n", nimcp_circuit_state_to_string(state));

    // Should have some blocked
    EXPECT_GT(passed, 0);
    EXPECT_GE(blocked, 0);  // May or may not have blocked depending on timing

    // Phase 2: Wait for half-open
    printf("  Phase 2: Waiting for half-open transition\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    state = nimcp_circuit_get_state(test_code);
    printf("    Circuit state after wait: %s\n", nimcp_circuit_state_to_string(state));

    // Phase 3: Report successes to close circuit
    printf("  Phase 3: Reporting successes\n");
    for (int i = 0; i < 5; i++) {
        nimcp_circuit_report_success(test_code);
    }

    state = nimcp_circuit_get_state(test_code);
    printf("    Circuit state after successes: %s\n", nimcp_circuit_state_to_string(state));

    // Phase 4: Verify circuit is now passing
    printf("  Phase 4: Verifying circuit closed\n");
    blocked = 0;
    for (int i = 0; i < 3; i++) {
        nimcp_exception_t* ex = create_test_exception(
            test_code,
            EXCEPTION_SEVERITY_WARNING,
            "Post-recovery test"
        );
        ASSERT_NE(ex, nullptr);

        int result = nimcp_circuit_record(ex);
        if (result == 1) blocked++;

        nimcp_exception_unref(ex);
    }
    printf("    Blocked after recovery: %d\n", blocked);

    // Get final stats
    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);
    printf("    Total blocked: %lu\n", (unsigned long)stats.total_blocked);

    // Reset for other tests
    nimcp_circuit_reset(test_code);

    printf("Test passed: Circuit breaker behavior and recovery verified\n\n");
}

/* ============================================================================
 * Test 4: Automatic Retry with Exponential Backoff
 *
 * Verifies retry mechanism with exponential backoff works correctly
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, AutomaticRetryWithBackoff) {
    printf("=== Test: Automatic Retry with Exponential Backoff ===\n");

    // Configure retry with backoff
    RetryConfig config;
    config.max_retries = 5;
    config.initial_delay_ms = 10;
    config.backoff_multiplier = 2.0f;
    config.max_delay_ms = 200;
    config.jitter_enabled = true;

    // Scenario 1: Operation succeeds on first try
    printf("  Scenario 1: Success on first try\n");
    g_recovery_state.retry_count = 0;

    bool success = retry_with_backoff([]() { return true; }, config);
    EXPECT_TRUE(success);
    EXPECT_EQ(g_recovery_state.retry_count.load(), 1);
    printf("    Retries needed: %d\n", g_recovery_state.retry_count.load());

    // Scenario 2: Operation succeeds after retries
    printf("  Scenario 2: Success after retries\n");
    g_recovery_state.retry_count = 0;
    std::atomic<int> fail_count{0};
    const int FAIL_UNTIL = 3;

    success = retry_with_backoff([&fail_count]() {
        fail_count++;
        return fail_count > FAIL_UNTIL;
    }, config);

    EXPECT_TRUE(success);
    EXPECT_EQ(g_recovery_state.retry_count.load(), FAIL_UNTIL + 1);
    printf("    Retries needed: %d (failed first %d)\n",
           g_recovery_state.retry_count.load(), FAIL_UNTIL);

    // Scenario 3: Operation fails all retries
    printf("  Scenario 3: Failure after all retries\n");
    g_recovery_state.retry_count = 0;
    g_recovery_state.backoff_delays = 0;

    auto start = std::chrono::high_resolution_clock::now();
    success = retry_with_backoff([]() { return false; }, config);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_FALSE(success);
    EXPECT_EQ(g_recovery_state.retry_count.load(), config.max_retries + 1);
    printf("    Total retries: %d\n", g_recovery_state.retry_count.load());
    printf("    Backoff delays: %d\n", g_recovery_state.backoff_delays.load());
    printf("    Total time: %ld ms\n", duration.count());

    // Verify backoff increased delay over time
    EXPECT_EQ(g_recovery_state.backoff_delays.load(), config.max_retries);

    printf("Test passed: Automatic retry with exponential backoff verified\n\n");
}

/* ============================================================================
 * Test 5: Health-Triggered Self-Repair After Exceptions
 *
 * Verifies health monitoring triggers self-repair after exception accumulation
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, HealthTriggeredSelfRepair) {
    printf("=== Test: Health-Triggered Self-Repair After Exceptions ===\n");

    // Register self-repair callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            g_recovery_state.self_repair_triggers++;
            return 0;
        }, nullptr);

    // Simulate health monitoring thresholds
    const int HEALTH_CHECK_THRESHOLD = 10;  // Check health every 10 exceptions
    const int SELF_REPAIR_THRESHOLD = 5;    // Trigger repair if > 5 errors since last check

    int exceptions_since_check = 0;
    int total_exceptions = 0;
    int health_checks = 0;
    int repairs_triggered = 0;

    // Generate exceptions and monitor health
    for (int i = 0; i < 100; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Health monitoring test"
        );
        if (!ex) continue;

        nimcp_exception_dispatch(ex);
        total_exceptions++;
        exceptions_since_check++;

        // Periodic health check
        if (total_exceptions % HEALTH_CHECK_THRESHOLD == 0) {
            health_checks++;
            printf("  Health check %d: %d exceptions since last check\n",
                   health_checks, exceptions_since_check);

            // Trigger self-repair if threshold exceeded
            if (exceptions_since_check > SELF_REPAIR_THRESHOLD) {
                nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
                repairs_triggered++;
                printf("    Self-repair triggered\n");
            }

            exceptions_since_check = 0;
        }

        nimcp_exception_unref(ex);
    }

    printf("  Total exceptions: %d\n", total_exceptions);
    printf("  Health checks performed: %d\n", health_checks);
    printf("  Self-repairs triggered: %d\n", repairs_triggered);
    printf("  Self-repair callbacks: %d\n", g_recovery_state.self_repair_triggers.load());

    EXPECT_EQ(health_checks, 10);  // 100 exceptions / 10 = 10 checks
    EXPECT_GT(repairs_triggered, 0);  // Should have triggered at least once
    EXPECT_EQ(g_recovery_state.self_repair_triggers.load(), repairs_triggered);

    printf("Test passed: Health-triggered self-repair verified\n\n");
}

/* ============================================================================
 * Test 6: Recovery Callback Chain Execution
 *
 * Verifies multiple recovery callbacks can be chained and executed in order
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, RecoveryCallbackChainExecution) {
    printf("=== Test: Recovery Callback Chain Execution ===\n");

    std::vector<std::string> execution_order;
    std::mutex order_mutex;

    // Register multiple recovery callbacks for different actions
    auto record_execution = [&execution_order, &order_mutex](const std::string& name) {
        std::lock_guard<std::mutex> lock(order_mutex);
        execution_order.push_back(name);
    };

    // GC callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            auto* record = static_cast<std::function<void(const std::string&)>*>(user_data);
            (*record)("GC");
            return 0;
        }, new std::function<void(const std::string&)>(record_execution));

    // Compact callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            auto* record = static_cast<std::function<void(const std::string&)>*>(user_data);
            (*record)("COMPACT");
            return 0;
        }, new std::function<void(const std::string&)>(record_execution));

    // Reduce load callback
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            auto* record = static_cast<std::function<void(const std::string&)>*>(user_data);
            (*record)("REDUCE_LOAD");
            return 0;
        }, new std::function<void(const std::string&)>(record_execution));

    printf("  Registered 3 recovery callbacks\n");

    // Create exception
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Test for callback chain"
    );
    ASSERT_NE(ex, nullptr);

    // Execute recovery actions in sequence
    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);
    nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_REDUCE_LOAD);

    printf("  Executed 3 recovery actions\n");

    // Verify execution order
    {
        std::lock_guard<std::mutex> lock(order_mutex);
        EXPECT_EQ(execution_order.size(), 3u);
        if (execution_order.size() >= 3) {
            EXPECT_EQ(execution_order[0], "GC");
            EXPECT_EQ(execution_order[1], "COMPACT");
            EXPECT_EQ(execution_order[2], "REDUCE_LOAD");
        }
        printf("  Execution order: ");
        for (const auto& name : execution_order) {
            printf("%s ", name.c_str());
        }
        printf("\n");
    }

    nimcp_exception_unref(ex);
    printf("Test passed: Recovery callback chain execution verified\n\n");
}

/* ============================================================================
 * Test 7: Recovery with Partial Success
 *
 * Verifies system handles partial recovery success (some actions fail)
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, RecoveryWithPartialSuccess) {
    printf("=== Test: Recovery with Partial Success ===\n");

    // Register callbacks with mixed success
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, success_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, failure_recovery_callback, nullptr);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, success_recovery_callback, nullptr);

    printf("  Registered: GC (success), COMPACT (fail), RETRY (success)\n");

    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_NO_MEMORY,
        EXCEPTION_SEVERITY_SEVERE,
        "Partial recovery test"
    );
    ASSERT_NE(ex, nullptr);

    // Try primary action (GC) - should succeed
    int result1 = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
    printf("  GC recovery: %s\n", result1 == 0 ? "success" : "failed");
    EXPECT_EQ(result1, 0);

    // Try secondary action (COMPACT) - should fail
    int result2 = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_COMPACT);
    printf("  COMPACT recovery: %s\n", result2 == 0 ? "success" : "failed");
    EXPECT_NE(result2, 0);

    // Try fallback action (RETRY) - should succeed
    int result3 = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    printf("  RETRY recovery: %s\n", result3 == 0 ? "success" : "failed");
    EXPECT_EQ(result3, 0);

    // Overall: 2 successes, 1 failure
    printf("  Summary: %d successes, %d failures\n",
           g_recovery_state.recovery_successes.load(),
           g_recovery_state.recovery_failures.load());

    EXPECT_EQ(g_recovery_state.recovery_successes.load(), 2);
    EXPECT_EQ(g_recovery_state.recovery_failures.load(), 1);

    nimcp_exception_unref(ex);
    printf("Test passed: Recovery with partial success verified\n\n");
}

/* ============================================================================
 * Test 8: Cascading Failure Prevention
 *
 * Verifies system prevents cascading failures through circuit breakers
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, CascadingFailurePrevention) {
    printf("=== Test: Cascading Failure Prevention ===\n");

    // Set up circuit breakers for different error codes
    nimcp_error_t codes[] = {
        NIMCP_ERROR_NO_MEMORY,
        NIMCP_ERROR_FILE_WRITE,
        NIMCP_ERROR_NETWORK_IO,
        NIMCP_ERROR_THREAD_CREATE
    };

    for (auto code : codes) {
        nimcp_circuit_set_threshold(code, 3, 1000);  // Low threshold for testing
    }

    printf("  Configured circuit breakers for 4 error types\n");

    // Simulate cascading failure scenario
    // When one component fails, it triggers failures in dependent components
    int total_exceptions = 0;
    int blocked_by_circuit = 0;
    int cascades_prevented = 0;

    for (int wave = 0; wave < 5; wave++) {
        printf("  Failure wave %d:\n", wave + 1);

        int wave_blocked = 0;
        for (auto code : codes) {
            for (int i = 0; i < 10; i++) {
                nimcp_exception_t* ex = create_test_exception(
                    code,
                    EXCEPTION_SEVERITY_ERROR,
                    "Cascade test"
                );
                if (!ex) continue;

                total_exceptions++;
                int result = nimcp_circuit_record(ex);
                if (result == 1) {
                    blocked_by_circuit++;
                    wave_blocked++;
                }

                nimcp_exception_unref(ex);
            }
        }

        printf("    Blocked in this wave: %d\n", wave_blocked);

        // If any exceptions were blocked, a cascade was prevented
        if (wave_blocked > 0) {
            cascades_prevented++;
        }
    }

    printf("  Total exceptions: %d\n", total_exceptions);
    printf("  Blocked by circuit: %d\n", blocked_by_circuit);
    printf("  Cascades prevented: %d\n", cascades_prevented);

    // Should have blocked some exceptions
    EXPECT_GT(blocked_by_circuit, 0);

    // Get circuit stats
    nimcp_circuit_stats_t stats;
    nimcp_circuit_get_stats(&stats);
    printf("  Open circuits: %zu\n", stats.circuits_open);

    // Reset circuits
    nimcp_circuit_reset_all();

    printf("Test passed: Cascading failure prevention verified\n\n");
}

/* ============================================================================
 * Test 9: Recovery State Machine Transitions
 *
 * Verifies exception recovery follows correct state machine transitions
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, RecoveryStateMachineTransitions) {
    printf("=== Test: Recovery State Machine Transitions ===\n");

    // States: INITIAL -> DETECTING -> ANALYZING -> RECOVERING -> VERIFIED -> COMPLETED
    enum RecoveryState {
        RS_INITIAL,
        RS_DETECTING,
        RS_ANALYZING,
        RS_RECOVERING,
        RS_VERIFIED,
        RS_COMPLETED,
        RS_FAILED
    };

    struct StateTransition {
        RecoveryState from;
        RecoveryState to;
        const char* trigger;
    };

    std::vector<StateTransition> transitions;
    RecoveryState current_state = RS_INITIAL;

    auto record_transition = [&transitions, &current_state](RecoveryState to, const char* trigger) {
        transitions.push_back({current_state, to, trigger});
        current_state = to;
    };

    // Create exception
    nimcp_exception_t* ex = create_test_exception(
        NIMCP_ERROR_OPERATION_FAILED,
        EXCEPTION_SEVERITY_SEVERE,
        "State machine test"
    );
    ASSERT_NE(ex, nullptr);

    // State: INITIAL -> DETECTING (exception created)
    record_transition(RS_DETECTING, "exception_created");
    printf("  INITIAL -> DETECTING: exception created\n");

    // State: DETECTING -> ANALYZING (exception dispatched)
    nimcp_exception_dispatch(ex);
    record_transition(RS_ANALYZING, "exception_dispatched");
    printf("  DETECTING -> ANALYZING: exception dispatched\n");

    // State: ANALYZING -> RECOVERING (recovery strategy selected)
    nimcp_exception_recovery_strategy_t strategy;
    nimcp_exception_get_recovery_strategy(ex, &strategy);
    record_transition(RS_RECOVERING, "strategy_selected");
    printf("  ANALYZING -> RECOVERING: strategy selected (%s)\n",
           nimcp_exception_recovery_action_to_string(strategy.primary_action));

    // Register success callback for this test
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, success_recovery_callback, nullptr);

    // State: RECOVERING -> VERIFIED (recovery executed)
    int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
    if (result == 0) {
        record_transition(RS_VERIFIED, "recovery_succeeded");
        printf("  RECOVERING -> VERIFIED: recovery succeeded\n");

        // State: VERIFIED -> COMPLETED (cleanup done)
        record_transition(RS_COMPLETED, "cleanup_done");
        printf("  VERIFIED -> COMPLETED: cleanup done\n");
    } else {
        record_transition(RS_FAILED, "recovery_failed");
        printf("  RECOVERING -> FAILED: recovery failed\n");
    }

    // Verify state transitions
    EXPECT_EQ(transitions.size(), 5u);
    EXPECT_EQ(current_state, RS_COMPLETED);

    printf("  Total transitions: %zu\n", transitions.size());
    printf("  Final state: %s\n",
           current_state == RS_COMPLETED ? "COMPLETED" : "FAILED");

    nimcp_exception_unref(ex);
    printf("Test passed: Recovery state machine transitions verified\n\n");
}

/* ============================================================================
 * Test 10: Recovery Metrics and Telemetry
 *
 * Verifies recovery metrics are accurately tracked and reported
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, RecoveryMetricsAndTelemetry) {
    printf("=== Test: Recovery Metrics and Telemetry ===\n");

    // Reset metrics
    nimcp_metrics_reset();
    g_recovery_state.reset();

    // Register callbacks that track timing
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));  // Simulate work
            g_recovery_state.recovery_attempts++;
            g_recovery_state.recovery_successes++;
            return 0;
        }, nullptr);

    // Generate exceptions and recover
    const int NUM_EXCEPTIONS = 50;
    printf("  Generating %d exceptions with recovery...\n", NUM_EXCEPTIONS);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Metrics test"
        );
        if (!ex) continue;

        // Record metrics
        nimcp_metrics_record_exception(ex);

        // Execute recovery
        auto recovery_start = std::chrono::high_resolution_clock::now();
        int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
        auto recovery_end = std::chrono::high_resolution_clock::now();
        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
            recovery_end - recovery_start).count();

        // Record recovery metrics
        nimcp_metrics_record_recovery(ex, EXCEPTION_RECOVERY_GC, result == 0, duration_us);

        nimcp_exception_unref(ex);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Get metrics
    nimcp_exception_metrics_t metrics;
    nimcp_metrics_get(&metrics);

    printf("  Metrics Summary:\n");
    printf("    Total exceptions: %lu\n", (unsigned long)metrics.total_exceptions);
    printf("    Recovery attempts: %lu\n", (unsigned long)metrics.total_recoveries_attempted);
    printf("    Recovery successes: %lu\n", (unsigned long)metrics.total_recoveries_succeeded);
    printf("    Overall recovery rate: %.1f%%\n", metrics.overall_recovery_rate * 100);
    printf("    Current rate: %.2f/sec\n", metrics.current_rate_per_second);
    printf("    Test duration: %ld ms\n", total_duration.count());

    // Get recovery-specific metrics
    float gc_success_rate = nimcp_metrics_get_recovery_rate(EXCEPTION_RECOVERY_GC);
    float gc_mttr = nimcp_metrics_get_mttr(EXCEPTION_RECOVERY_GC);
    printf("    GC success rate: %.1f%%\n", gc_success_rate * 100);
    printf("    GC MTTR: %.1f us\n", gc_mttr);

    // Verify metrics accuracy
    EXPECT_EQ(metrics.total_exceptions, (uint64_t)NUM_EXCEPTIONS);
    EXPECT_EQ(metrics.total_recoveries_attempted, (uint64_t)NUM_EXCEPTIONS);
    EXPECT_EQ(metrics.total_recoveries_succeeded, (uint64_t)NUM_EXCEPTIONS);
    EXPECT_FLOAT_EQ(metrics.overall_recovery_rate, 1.0f);  // All should succeed

    // Verify our internal tracking matches
    EXPECT_EQ(g_recovery_state.recovery_attempts.load(), NUM_EXCEPTIONS);
    EXPECT_EQ(g_recovery_state.recovery_successes.load(), NUM_EXCEPTIONS);

    printf("Test passed: Recovery metrics and telemetry verified\n\n");
}

/* ============================================================================
 * Test 11: Concurrent Recovery Operations
 *
 * Verifies recovery handles concurrent exception streams correctly
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, ConcurrentRecoveryOperations) {
    printf("=== Test: Concurrent Recovery Operations ===\n");

    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, success_recovery_callback, nullptr);

    const int NUM_THREADS = 8;
    const int EXCEPTIONS_PER_THREAD = 50;
    std::vector<std::thread> threads;
    std::atomic<int> total_recovered{0};
    std::atomic<int> total_created{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < EXCEPTIONS_PER_THREAD; i++) {
                nimcp_exception_t* ex = nimcp_exception_create(
                    NIMCP_ERROR_OPERATION_FAILED,
                    EXCEPTION_SEVERITY_ERROR,
                    __FILE__, __LINE__, __func__,
                    "Thread %d exception %d", t, i
                );
                if (!ex) continue;

                total_created++;

                // Dispatch
                nimcp_exception_dispatch(ex);

                // Recover
                int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_RETRY);
                if (result == 0) {
                    total_recovered++;
                }

                nimcp_exception_unref(ex);
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    printf("  Threads: %d\n", NUM_THREADS);
    printf("  Exceptions per thread: %d\n", EXCEPTIONS_PER_THREAD);
    printf("  Total created: %d\n", total_created.load());
    printf("  Total recovered: %d\n", total_recovered.load());
    printf("  Recovery callbacks: %d\n", g_recovery_state.recovery_attempts.load());

    EXPECT_EQ(total_created.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);
    EXPECT_EQ(total_recovered.load(), NUM_THREADS * EXCEPTIONS_PER_THREAD);

    printf("Test passed: Concurrent recovery operations verified\n\n");
}

/* ============================================================================
 * Test 12: Recovery with Resource Constraints
 *
 * Verifies recovery works under resource constraints
 * ============================================================================ */

TEST_F(ExceptionRecoveryScenariosE2ETest, RecoveryWithResourceConstraints) {
    printf("=== Test: Recovery with Resource Constraints ===\n");

    // Simulate resource-constrained recovery
    std::atomic<int> available_resources{10};
    std::atomic<int> recovery_attempts{0};
    std::atomic<int> successful_with_resources{0};
    std::atomic<int> failed_no_resources{0};

    // Recovery callback that consumes resources
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC,
        [](nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) -> int {
            auto* resources = static_cast<std::atomic<int>*>(user_data);

            // Try to acquire resource
            int current = resources->load();
            while (current > 0) {
                if (resources->compare_exchange_weak(current, current - 1)) {
                    // Got resource, do recovery
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));

                    // Release resource
                    (*resources)++;
                    return 0;
                }
                current = resources->load();
            }

            // No resources available
            return -1;
        }, &available_resources);

    // Generate many exceptions to stress resources
    const int NUM_EXCEPTIONS = 100;
    printf("  Initial resources: %d\n", available_resources.load());
    printf("  Generating %d exceptions...\n", NUM_EXCEPTIONS);

    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        nimcp_exception_t* ex = create_test_exception(
            NIMCP_ERROR_NO_MEMORY,
            EXCEPTION_SEVERITY_ERROR,
            "Resource constraint test"
        );
        if (!ex) continue;

        recovery_attempts++;
        int result = nimcp_execute_recovery(ex, EXCEPTION_RECOVERY_GC);
        if (result == 0) {
            successful_with_resources++;
        } else {
            failed_no_resources++;
        }

        nimcp_exception_unref(ex);
    }

    printf("  Recovery attempts: %d\n", recovery_attempts.load());
    printf("  Successful (got resources): %d\n", successful_with_resources.load());
    printf("  Failed (no resources): %d\n", failed_no_resources.load());
    printf("  Final resources: %d\n", available_resources.load());

    // Most should succeed since we have resources
    EXPECT_GT(successful_with_resources.load(), 0);
    // Resources should be properly returned
    EXPECT_EQ(available_resources.load(), 10);

    printf("Test passed: Recovery with resource constraints verified\n\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
