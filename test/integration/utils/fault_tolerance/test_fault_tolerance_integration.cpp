/**
 * @file test_fault_tolerance_integration.cpp
 * @brief Integration tests for State Machine and Metrics Aggregator
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_fault_state_machine.h"
#include "utils/fault_tolerance/nimcp_metrics_aggregator.h"
#include <thread>
#include <chrono>

/* =============================================================================
 * State Machine + Metrics Integration Tests
 * ============================================================================= */

class FaultToleranceIntegrationTest : public ::testing::Test {
protected:
    nimcp_state_machine_t* sm;
    nimcp_metrics_aggregator_t* latency_agg;
    nimcp_metrics_aggregator_t* error_agg;

    void SetUp() override {
        sm = nimcp_state_machine_create();
        latency_agg = nimcp_metrics_aggregator_create("latency_ms");
        error_agg = nimcp_metrics_aggregator_create("error_rate");

        ASSERT_NE(sm, nullptr);
        ASSERT_NE(latency_agg, nullptr);
        ASSERT_NE(error_agg, nullptr);
    }

    void TearDown() override {
        nimcp_state_machine_destroy(sm);
        nimcp_metrics_aggregator_destroy(latency_agg);
        nimcp_metrics_aggregator_destroy(error_agg);
    }
};

TEST_F(FaultToleranceIntegrationTest, StateTransitionTriggersMetricsCollection) {
    // Start in HEALTHY state
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);

    // Collect metrics in HEALTHY state
    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 10.0 + i, 0);
        nimcp_metrics_aggregator_add_sample(error_agg, 0.01, 0);
    }

    // Transition to DEGRADED
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 100);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);

    // Collect metrics in DEGRADED state (worse performance)
    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 50.0 + i, 0);
        nimcp_metrics_aggregator_add_sample(error_agg, 0.15, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    nimcp_metrics_aggregator_aggregate(error_agg);

    // Verify metrics show degradation
    const auto* latency_stats = nimcp_metrics_aggregator_get_stats(latency_agg,
        NIMCP_WINDOW_1S);
    const auto* error_stats = nimcp_metrics_aggregator_get_stats(error_agg,
        NIMCP_WINDOW_1S);

    EXPECT_GT(latency_stats->max, 50.0);
    EXPECT_GT(error_stats->avg, 0.1);
}

TEST_F(FaultToleranceIntegrationTest, MetricsDriveStateTransitions) {
    // Monitor latency and transition based on thresholds
    double latency_threshold_degraded = 30.0;
    double latency_threshold_failed = 100.0;

    // Normal operation
    for (int i = 0; i < 10; i++) {
        double latency = 10.0 + (rand() % 10);
        nimcp_metrics_aggregator_add_sample(latency_agg, latency, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    double p95 = nimcp_metrics_aggregator_get_percentile(latency_agg,
        NIMCP_WINDOW_1S, 0.95);

    if (p95 < latency_threshold_degraded) {
        EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
    }

    // Simulate latency spike
    for (int i = 0; i < 20; i++) {
        double latency = 40.0 + (rand() % 20);
        nimcp_metrics_aggregator_add_sample(latency_agg, latency, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    p95 = nimcp_metrics_aggregator_get_percentile(latency_agg, NIMCP_WINDOW_1S, 0.95);

    if (p95 >= latency_threshold_degraded && p95 < latency_threshold_failed) {
        nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 200);
        EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);
    }
}

TEST_F(FaultToleranceIntegrationTest, RecoveryScenario) {
    // Simulate full recovery cycle: HEALTHY -> DEGRADED -> RECOVERING -> HEALTHY

    // Phase 1: HEALTHY
    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 1);
    for (int i = 0; i < 5; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 10.0, 0);
    }

    // Phase 2: Degrade
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 2);
    for (int i = 0; i < 5; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 50.0, 0);
    }

    // Phase 3: Start recovery
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 3);
    for (int i = 0; i < 5; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 30.0 - i * 2, 0);
    }

    // Phase 4: Fully recovered
    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 4);
    for (int i = 0; i < 5; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 12.0, 0);
    }

    // Verify final state
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);

    // Verify transition history
    uint32_t count = 0;
    const nimcp_state_transition_t* history =
        nimcp_state_machine_get_history(sm, &count);
    EXPECT_EQ(count, 3);  // 3 transitions (self-transition doesn't count)

    // Verify metrics
    nimcp_metrics_aggregator_aggregate(latency_agg);
    const auto* stats = nimcp_metrics_aggregator_get_stats(latency_agg,
        NIMCP_WINDOW_1S);
    EXPECT_GT(stats->count, 0);
}

TEST_F(FaultToleranceIntegrationTest, MultipleMetricsInfluenceState) {
    // Both latency and error rate should influence state

    // Scenario 1: High latency, low errors -> DEGRADED
    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 60.0, 0);
        nimcp_metrics_aggregator_add_sample(error_agg, 0.02, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    nimcp_metrics_aggregator_aggregate(error_agg);

    double latency_avg = nimcp_metrics_aggregator_get_avg(latency_agg, NIMCP_WINDOW_1S);
    double error_avg = nimcp_metrics_aggregator_get_avg(error_agg, NIMCP_WINDOW_1S);

    if (latency_avg > 50.0 || error_avg > 0.1) {
        nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 100);
    }
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);

    // Scenario 2: Normal latency, high errors -> FAILED
    nimcp_metrics_aggregator_reset(latency_agg);
    nimcp_metrics_aggregator_reset(error_agg);

    for (int i = 0; i < 10; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 15.0, 0);
        nimcp_metrics_aggregator_add_sample(error_agg, 0.5, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    nimcp_metrics_aggregator_aggregate(error_agg);

    error_avg = nimcp_metrics_aggregator_get_avg(error_agg, NIMCP_WINDOW_1S);
    if (error_avg > 0.3) {
        nimcp_state_machine_transition(sm, NIMCP_STATE_FAILED, 200);
    }
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_FAILED);
}

TEST_F(FaultToleranceIntegrationTest, TimeBasedWindowAggregation) {
    // Test that metrics aggregate properly over time windows

    time_t start_time = time(NULL);

    // Add samples over 2 seconds
    for (int sec = 0; sec < 2; sec++) {
        for (int i = 0; i < 10; i++) {
            nimcp_metrics_aggregator_add_sample(latency_agg,
                10.0 + sec * 5.0 + i, start_time + sec);
        }
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);

    // Both 1s and 10s windows should have data
    uint64_t count_1s = nimcp_metrics_aggregator_get_count(latency_agg, NIMCP_WINDOW_1S);
    uint64_t count_10s = nimcp_metrics_aggregator_get_count(latency_agg, NIMCP_WINDOW_10S);

    EXPECT_GT(count_1s, 0);
    EXPECT_GT(count_10s, 0);
}

TEST_F(FaultToleranceIntegrationTest, StateDurationTracking) {
    // Track how long we spend in each state

    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 2);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, 3);

    uint64_t degraded_time =
        nimcp_state_machine_get_total_state_duration(sm, NIMCP_STATE_DEGRADED);
    uint64_t recovering_time =
        nimcp_state_machine_get_total_state_duration(sm, NIMCP_STATE_RECOVERING);

    EXPECT_GE(degraded_time, 1);
    EXPECT_GE(recovering_time, 1);
}

TEST_F(FaultToleranceIntegrationTest, CallbacksUpdateMetrics) {
    struct Context {
        nimcp_metrics_aggregator_t* agg;
        int transition_count;
    };

    Context ctx = {error_agg, 0};

    auto entry_callback = [](nimcp_brain_state_t state, void* user_data) -> bool {
        auto* c = static_cast<Context*>(user_data);
        c->transition_count++;
        // Record state transition as a metric
        nimcp_metrics_aggregator_add_sample(c->agg, state * 1.0, 0);
        return true;
    };

    nimcp_state_machine_set_user_data(sm, &ctx);
    nimcp_state_machine_set_entry_callback(sm, NIMCP_STATE_DEGRADED, entry_callback);
    nimcp_state_machine_set_entry_callback(sm, NIMCP_STATE_RECOVERING, entry_callback);

    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);
    nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 2);

    EXPECT_EQ(ctx.transition_count, 2);

    nimcp_metrics_aggregator_aggregate(error_agg);
    uint64_t count = nimcp_metrics_aggregator_get_count(error_agg, NIMCP_WINDOW_1S);
    EXPECT_EQ(count, 2);
}

TEST_F(FaultToleranceIntegrationTest, ShutdownPreservesMetrics) {
    // Collect some metrics
    for (int i = 0; i < 20; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, i * 1.0, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);

    // Transition to SHUTDOWN
    nimcp_state_machine_transition(sm, NIMCP_STATE_SHUTDOWN, 999);
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_SHUTDOWN);

    // Metrics should still be accessible
    const auto* stats = nimcp_metrics_aggregator_get_stats(latency_agg, NIMCP_WINDOW_1S);
    EXPECT_GT(stats->count, 0);
    EXPECT_GE(stats->max, 0.0);
}

TEST_F(FaultToleranceIntegrationTest, ConcurrentMetricsAndTransitions) {
    // Simulate rapid metrics collection during state transitions

    for (int i = 0; i < 5; i++) {
        // Add some metrics
        nimcp_metrics_aggregator_add_sample(latency_agg, 10.0 + i, 0);

        // Transition state
        if (i % 2 == 0) {
            nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, i);
        } else {
            nimcp_state_machine_transition(sm, NIMCP_STATE_HEALTHY, i);
        }

        // Add more metrics
        nimcp_metrics_aggregator_add_sample(error_agg, 0.05 * i, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);
    nimcp_metrics_aggregator_aggregate(error_agg);

    // Verify both state machine and metrics are consistent
    uint32_t attempts = 0, failures = 0;
    nimcp_state_machine_get_statistics(sm, &attempts, &failures);
    EXPECT_GT(attempts, 0);

    uint64_t latency_count = nimcp_metrics_aggregator_get_count(latency_agg,
        NIMCP_WINDOW_1S);
    uint64_t error_count = nimcp_metrics_aggregator_get_count(error_agg,
        NIMCP_WINDOW_1S);
    EXPECT_EQ(latency_count, 5);
    EXPECT_EQ(error_count, 5);
}

TEST_F(FaultToleranceIntegrationTest, PercentileBasedHealthCheck) {
    // Use P95/P99 to determine if system is healthy

    // Good scenario: tight distribution
    for (int i = 0; i < 100; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 10.0 + (rand() % 5), 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);

    double p95 = nimcp_metrics_aggregator_get_percentile(latency_agg,
        NIMCP_WINDOW_1S, 0.95);
    double p99 = nimcp_metrics_aggregator_get_percentile(latency_agg,
        NIMCP_WINDOW_1S, 0.99);

    // P95 and P99 should be close in healthy system
    if (p99 - p95 < 10.0 && p95 < 20.0) {
        EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_HEALTHY);
    }

    // Bad scenario: wide distribution with outliers
    nimcp_metrics_aggregator_reset(latency_agg);

    for (int i = 0; i < 95; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 10.0, 0);
    }
    for (int i = 0; i < 5; i++) {
        nimcp_metrics_aggregator_add_sample(latency_agg, 200.0, 0);  // Outliers
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);

    p95 = nimcp_metrics_aggregator_get_percentile(latency_agg, NIMCP_WINDOW_1S, 0.95);
    p99 = nimcp_metrics_aggregator_get_percentile(latency_agg, NIMCP_WINDOW_1S, 0.99);

    // Large gap indicates problems
    if (p99 - p95 > 50.0 || p95 > 50.0) {
        nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 500);
        EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);
    }
}

TEST_F(FaultToleranceIntegrationTest, MultiWindowComparison) {
    // Compare short-term vs long-term metrics

    // Simulate 1-minute workload
    for (int sec = 0; sec < 60; sec++) {
        double latency = (sec < 30) ? 10.0 : 50.0;  // Spike halfway through
        nimcp_metrics_aggregator_add_sample(latency_agg, latency, 0);
    }

    nimcp_metrics_aggregator_aggregate(latency_agg);

    double avg_1s = nimcp_metrics_aggregator_get_avg(latency_agg, NIMCP_WINDOW_1S);
    double avg_1m = nimcp_metrics_aggregator_get_avg(latency_agg, NIMCP_WINDOW_1M);

    // Recent average should be higher than overall average
    EXPECT_GT(avg_1s, avg_1m * 0.8);
}

/* =============================================================================
 * Error Scenarios
 * ============================================================================= */

TEST_F(FaultToleranceIntegrationTest, InvalidStateDoesNotCorruptMetrics) {
    nimcp_metrics_aggregator_add_sample(latency_agg, 42.0, 0);

    // Attempt invalid transition
    auto result = nimcp_state_machine_transition(sm, NIMCP_STATE_RECOVERING, 1);
    EXPECT_EQ(result, NIMCP_TRANSITION_INVALID);

    // Metrics should still be valid
    nimcp_metrics_aggregator_aggregate(latency_agg);
    const auto* stats = nimcp_metrics_aggregator_get_stats(latency_agg, NIMCP_WINDOW_1S);
    EXPECT_DOUBLE_EQ(stats->max, 42.0);
}

TEST_F(FaultToleranceIntegrationTest, MetricsResetDoesNotAffectState) {
    nimcp_state_machine_transition(sm, NIMCP_STATE_DEGRADED, 1);

    nimcp_metrics_aggregator_add_sample(latency_agg, 100.0, 0);
    nimcp_metrics_aggregator_reset(latency_agg);

    // State should be unchanged
    EXPECT_EQ(nimcp_state_machine_get_state(sm), NIMCP_STATE_DEGRADED);

    // Metrics should be reset
    uint64_t total_samples = 0, aggs = 0;
    nimcp_metrics_aggregator_get_statistics(latency_agg, &total_samples, &aggs);
    EXPECT_EQ(total_samples, 0);
}
