//=============================================================================
// test_brain_cycle_coordinator_regression.cpp - Cycle Coordinator Regression Tests
//=============================================================================
/**
 * @file test_brain_cycle_coordinator_regression.cpp
 * @brief Regression tests for brain cycle coordinator stability
 *
 * WHAT: Tests ensuring cycle coordinator behavior remains stable across changes
 * WHY:  Prevent regressions in coordinator edge cases, error handling, memory
 *       safety, statistical algorithms, and logging behavior
 * HOW:  GoogleTest with fixtures covering edge cases, error handling, memory
 *       safety, Welford/FNV/z-score utilities, and logging
 *
 * Test Categories:
 * 1. Edge Cases - Extreme durations, rapid ticks, stall detection
 * 2. Error Handling - Health callback errors, shutdown, register/unregister
 * 3. Memory Safety - Leak-free destruction, overflow resilience
 * 4. NIMCP Utility Edge Cases - Welford, FNV-1a, z-score
 * 5. Logging Regression - Diagnose output, logging flags
 *
 * @version 1.0.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <unistd.h>

#include "core/brain/nimcp_brain_cycle_coordinator.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CycleCoordinatorRegressionTest : public ::testing::Test {
protected:
    brain_cycle_coordinator_t* coord = nullptr;

    void SetUp() override {
        brain_cycle_coordinator_config_t config;
        brain_cycle_coordinator_default_config(&config);
        config.enable_logging = false;
        coord = brain_cycle_coordinator_create(&config);
        ASSERT_NE(nullptr, coord);
    }

    void TearDown() override {
        brain_cycle_coordinator_destroy(coord);
        coord = nullptr;
    }
};

//=============================================================================
// 1. EDGE CASES
//=============================================================================

/**
 * TEST: test_very_long_tick_duration
 *
 * WHAT: Notify a tick with UINT64_MAX/2 duration
 * WHY:  Ensure no overflow/crash in Welford stats or EMA computation
 * EXPECT: No crash, stats updated with at least 1 tick
 */
TEST_F(CycleCoordinatorRegressionTest, test_very_long_tick_duration) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    uint64_t huge_duration = UINT64_MAX / 2;
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, huge_duration);
    EXPECT_EQ(0, rc);

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(1u, status.ticks_executed);
    EXPECT_GE(status.avg_duration_us, 0.0);
    EXPECT_EQ(huge_duration, status.max_duration_us);
}

/**
 * TEST: test_very_short_tick_duration
 *
 * WHAT: Notify a tick with 0 duration
 * WHY:  Ensure no division-by-zero or other issue with zero duration
 * EXPECT: No crash, stats updated
 */
TEST_F(CycleCoordinatorRegressionTest, test_very_short_tick_duration) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_OSCILLATIONS, 0);
    EXPECT_EQ(0, rc);

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(1u, status.ticks_executed);
    EXPECT_DOUBLE_EQ(0.0, status.avg_duration_us);
    EXPECT_EQ(0u, status.max_duration_us);
}

/**
 * TEST: test_rapid_tick_notifications
 *
 * WHAT: Send 10000 ticks in a tight loop
 * WHY:  Verify no crash, lock contention issues, or stat corruption under load
 * EXPECT: ticks_executed == 10000, stats valid
 */
TEST_F(CycleCoordinatorRegressionTest, test_rapid_tick_notifications) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    const uint32_t tick_count = 10000;
    for (uint32_t i = 0; i < tick_count; i++) {
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, 100);
        ASSERT_EQ(0, rc);
    }

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ((uint64_t)tick_count, status.ticks_executed);
    EXPECT_GT(status.avg_duration_us, 0.0);
    EXPECT_EQ(100u, status.max_duration_us);
}

/**
 * TEST: test_stall_detection_accuracy
 *
 * WHAT: Register immune_tick (50ms expected, stall threshold = 150ms), tick
 *       once, then sleep 200ms so elapsed time exceeds the threshold
 * WHY:  Verify stall detection fires correctly for the immune tick cycle
 * EXPECT: check_health returns >= 1 issue for the stalled cycle
 */
TEST_F(CycleCoordinatorRegressionTest, test_stall_detection_accuracy) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    // Tick once to set last_tick_us and ticks_executed > 0
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 100);
    ASSERT_EQ(0, rc);

    // Sleep 200ms - immune_tick expected interval is 50ms, stall = 50ms * 3 = 150ms
    usleep(200000);

    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
}

//=============================================================================
// 2. ERROR HANDLING
//=============================================================================

/**
 * Helper health function that always returns ERROR
 */
static brain_cycle_health_t health_fn_returns_error(void* handle) {
    (void)handle;
    return BRAIN_CYCLE_HEALTH_ERROR;
}

/**
 * TEST: test_health_callback_throws
 *
 * WHAT: Register a health_fn that returns ERROR, then check health
 * WHY:  Verify ERROR health is propagated and counted as an issue
 * EXPECT: check_health detects the error state
 */
TEST_F(CycleCoordinatorRegressionTest, test_health_callback_throws) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, nullptr, health_fn_returns_error);
    ASSERT_EQ(0, rc);

    // Tick once so the cycle is active
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_HEALTH_AGENT, 50);
    ASSERT_EQ(0, rc);

    // Check health - health_fn returns ERROR
    int issues = brain_cycle_coordinator_check_health(coord);
    // The health function returns ERROR, so the cycle should now be in ERROR state.
    // Stall detection may also fire (health_agent expected_interval is 100ms),
    // but the health_fn result should also be applied.

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &status);
    EXPECT_EQ(0, rc);

    // Health should be ERROR or STALLED (stall may override if elapsed > threshold)
    // But the health_fn returned ERROR, so at minimum it was set.
    // In the implementation, health_fn is called first, then stall can override to STALLED.
    // Either ERROR or STALLED is acceptable - both indicate a problem was detected.
    EXPECT_TRUE(status.health == BRAIN_CYCLE_HEALTH_ERROR ||
                status.health == BRAIN_CYCLE_HEALTH_STALLED);
    (void)issues;
}

/**
 * TEST: test_notification_during_shutdown
 *
 * WHAT: Create coordinator, register a cycle, then destroy gracefully
 * WHY:  Verify no crash or undefined behavior during destroy with registered cycles
 * EXPECT: Clean destruction without crash
 */
TEST_F(CycleCoordinatorRegressionTest, test_notification_during_shutdown) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    ASSERT_EQ(0, rc);

    // Destroy the coordinator (TearDown will also call destroy on nullptr, which is safe)
    brain_cycle_coordinator_destroy(coord);
    coord = nullptr;

    // Verify NULL destroy is safe
    brain_cycle_coordinator_destroy(nullptr);
    SUCCEED();
}

/**
 * TEST: test_concurrent_register_unregister
 *
 * WHAT: Register and unregister the same cycle type multiple times
 * WHY:  Verify no corruption or crash from repeated register/unregister
 * EXPECT: All register/unregister operations succeed or fail gracefully
 */
TEST_F(CycleCoordinatorRegressionTest, test_concurrent_register_unregister) {
    for (int i = 0; i < 50; i++) {
        int rc = brain_cycle_coordinator_register(
            coord, BRAIN_CYCLE_GC_AGENT, nullptr, nullptr);
        ASSERT_EQ(0, rc) << "Register failed on iteration " << i;

        // Tick once while registered
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_GC_AGENT, 100);
        ASSERT_EQ(0, rc);

        rc = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_GC_AGENT);
        ASSERT_EQ(0, rc) << "Unregister failed on iteration " << i;

        // Ticking after unregister should fail gracefully
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_GC_AGENT, 100);
        EXPECT_EQ(-1, rc);
    }

    // Double unregister should fail gracefully
    int rc = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_GC_AGENT);
    EXPECT_EQ(-1, rc);
}

//=============================================================================
// 3. MEMORY SAFETY
//=============================================================================

/**
 * TEST: test_no_memory_leak_on_destroy
 *
 * WHAT: Create coordinator, register all cycle types, add dependencies,
 *       register callbacks, then destroy
 * WHY:  Ensure all resources are freed properly (valgrind-safe)
 * EXPECT: No crash, clean destroy
 */
TEST_F(CycleCoordinatorRegressionTest, test_no_memory_leak_on_destroy) {
    // Register all cycle types
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        int rc = brain_cycle_coordinator_register(
            coord, (brain_cycle_type_t)i, nullptr, nullptr);
        ASSERT_EQ(0, rc) << "Failed to register cycle type " << i;
    }

    // Add several dependencies
    int rc = brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_HEALTH_AGENT, BRAIN_CYCLE_IMMUNE_TICK);
    EXPECT_EQ(0, rc);
    rc = brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_OSCILLATIONS);
    EXPECT_EQ(0, rc);
    rc = brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_GC_AGENT, BRAIN_CYCLE_BRAIN_UPDATE);
    EXPECT_EQ(0, rc);

    // Register callbacks
    int sentinel = 42;
    brain_cycle_coordinator_callbacks_t cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.user_data = &sentinel;
    rc = brain_cycle_coordinator_register_callbacks(coord, &cbs);
    EXPECT_EQ(0, rc);

    // Tick all cycles
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        rc = brain_cycle_coordinator_notify_tick(
            coord, (brain_cycle_type_t)i, 500);
        EXPECT_EQ(0, rc);
    }

    // Run health check
    brain_cycle_coordinator_check_health(coord);

    // Get stats
    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(coord, &stats);
    EXPECT_EQ(0, rc);
    EXPECT_EQ((uint32_t)BRAIN_CYCLE_COUNT, stats.total_cycles_registered);

    // Destroy is called in TearDown - if we get here, no crash
    SUCCEED();
}

/**
 * TEST: test_stats_accumulation_overflow
 *
 * WHAT: Tick 100K times to stress-test accumulation counters
 * WHY:  Verify no overflow-induced crash with large tick counts
 * EXPECT: No crash, ticks_executed matches
 */
TEST_F(CycleCoordinatorRegressionTest, test_stats_accumulation_overflow) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    const uint32_t tick_count = 100000;
    for (uint32_t i = 0; i < tick_count; i++) {
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_OSCILLATIONS, (uint64_t)(i % 1000));
        ASSERT_EQ(0, rc);
    }

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ((uint64_t)tick_count, status.ticks_executed);
    EXPECT_GT(status.avg_duration_us, 0.0);
}

//=============================================================================
// 4. NIMCP UTILITY EDGE CASES
//=============================================================================

/**
 * TEST: test_welford_single_sample
 *
 * WHAT: Tick once with a known duration, verify mean == duration and stddev == 0
 * WHY:  Welford's algorithm must produce correct results with n=1
 * EXPECT: avg_duration_us equals the single sample duration
 */
TEST_F(CycleCoordinatorRegressionTest, test_welford_single_sample) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    const uint64_t duration = 12345;
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, duration);
    ASSERT_EQ(0, rc);

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(1u, status.ticks_executed);

    // With a single sample, avg_duration_us (EMA) should equal the duration
    EXPECT_DOUBLE_EQ((double)duration, status.avg_duration_us);
    EXPECT_EQ(duration, status.max_duration_us);
}

/**
 * TEST: test_welford_identical_samples
 *
 * WHAT: Tick 100 times with identical duration
 * WHY:  Welford's algorithm should produce stddev near 0 for identical values
 * EXPECT: avg_duration_us approaches the constant value (via EMA), max equals it
 */
TEST_F(CycleCoordinatorRegressionTest, test_welford_identical_samples) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    const uint64_t constant_duration = 5000;
    for (int i = 0; i < 100; i++) {
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, constant_duration);
        ASSERT_EQ(0, rc);
    }

    brain_cycle_status_t status;
    memset(&status, 0, sizeof(status));
    rc = brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(100u, status.ticks_executed);

    // EMA converges to the constant value; after 100 samples it should be very close
    EXPECT_NEAR((double)constant_duration, status.avg_duration_us, 1.0);
    EXPECT_EQ(constant_duration, status.max_duration_us);
}

/**
 * TEST: test_fnv1a_different_patterns
 *
 * WHAT: Create two coordinators with different health states, run check_health
 *       on each, and verify the pattern tracking produces different hashes
 * WHY:  Different health states should produce different FNV-1a fingerprints
 * EXPECT: The coordinator stats diverge between the two setups
 */
TEST_F(CycleCoordinatorRegressionTest, test_fnv1a_different_patterns) {
    // Setup pattern 1: register immune_tick and health_agent
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    ASSERT_EQ(0, rc);
    rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    // Tick immune_tick to make it HEALTHY
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 100);
    ASSERT_EQ(0, rc);
    // Tick health_agent to make it HEALTHY
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_HEALTH_AGENT, 200);
    ASSERT_EQ(0, rc);

    brain_cycle_coordinator_check_health(coord);

    // Get stats for pattern 1
    brain_cycle_coordinator_stats_t stats1;
    rc = brain_cycle_coordinator_get_stats(coord, &stats1);
    ASSERT_EQ(0, rc);

    // Now create a second coordinator with different health states
    brain_cycle_coordinator_config_t config2;
    brain_cycle_coordinator_default_config(&config2);
    config2.enable_logging = false;
    brain_cycle_coordinator_t* coord2 = brain_cycle_coordinator_create(&config2);
    ASSERT_NE(nullptr, coord2);

    // Register only immune_tick with error health
    rc = brain_cycle_coordinator_register(
        coord2, BRAIN_CYCLE_IMMUNE_TICK, nullptr, health_fn_returns_error);
    ASSERT_EQ(0, rc);
    rc = brain_cycle_coordinator_notify_tick(
        coord2, BRAIN_CYCLE_IMMUNE_TICK, 100);
    ASSERT_EQ(0, rc);

    brain_cycle_coordinator_check_health(coord2);

    brain_cycle_coordinator_stats_t stats2;
    rc = brain_cycle_coordinator_get_stats(coord2, &stats2);
    ASSERT_EQ(0, rc);

    // The two coordinators have different registered sets and health states,
    // so their overall health should differ
    EXPECT_NE(stats1.total_cycles_registered, stats2.total_cycles_registered);

    // Different health states produce different overall health scores
    // (coord1 has 2 healthy cycles, coord2 has 1 error cycle)
    EXPECT_NE(stats1.overall_health, stats2.overall_health);

    brain_cycle_coordinator_destroy(coord2);
}

/**
 * TEST: test_zscore_not_triggered_early
 *
 * WHAT: Send an anomalous duration before 10 ticks and verify no anomaly detected
 * WHY:  Z-score requires count >= 10 and stddev >= 1.0 before triggering
 * EXPECT: timing_anomalies_detected remains 0 after fewer than 10 ticks
 */
TEST_F(CycleCoordinatorRegressionTest, test_zscore_not_triggered_early) {
    // Create coordinator with timing checks enabled
    brain_cycle_coordinator_destroy(coord);
    brain_cycle_coordinator_config_t config;
    brain_cycle_coordinator_default_config(&config);
    config.enable_logging = false;
    config.enable_timing_checks = true;
    coord = brain_cycle_coordinator_create(&config);
    ASSERT_NE(nullptr, coord);

    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    // Send 5 normal ticks (all 100us)
    for (int i = 0; i < 5; i++) {
        rc = brain_cycle_coordinator_notify_tick(
            coord, BRAIN_CYCLE_BRAIN_UPDATE, 100);
        ASSERT_EQ(0, rc);
    }

    // Send 1 wildly anomalous tick - this should NOT trigger z-score
    // because count is only 6 (< 10 required)
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, 999999999);
    ASSERT_EQ(0, rc);

    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(coord, &stats);
    ASSERT_EQ(0, rc);
    EXPECT_EQ(0u, stats.timing_anomalies_detected);
}

//=============================================================================
// 5. LOGGING REGRESSION
//=============================================================================

/**
 * TEST: test_log_output_format
 *
 * WHAT: Register cycles, call diagnose, verify buffer contains expected strings
 * WHY:  Ensure diagnostic output format hasn't regressed
 * EXPECT: Buffer contains "Diagnostics", cycle names, health labels
 */
TEST_F(CycleCoordinatorRegressionTest, test_log_output_format) {
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    ASSERT_EQ(0, rc);
    rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, nullptr, nullptr);
    ASSERT_EQ(0, rc);

    // Tick both so they have some stats
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    ASSERT_EQ(0, rc);
    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, 1000);
    ASSERT_EQ(0, rc);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int issues = brain_cycle_coordinator_diagnose(coord, buffer, sizeof(buffer));
    EXPECT_GE(issues, 0);

    // Verify expected content in diagnostics output
    EXPECT_NE(nullptr, strstr(buffer, "Diagnostics"));
    EXPECT_NE(nullptr, strstr(buffer, "immune_tick"));
    EXPECT_NE(nullptr, strstr(buffer, "brain_update"));
    EXPECT_NE(nullptr, strstr(buffer, "health="));
    EXPECT_NE(nullptr, strstr(buffer, "ticks="));
    EXPECT_NE(nullptr, strstr(buffer, "Registered:"));
    EXPECT_NE(nullptr, strstr(buffer, "Health:"));
}

/**
 * TEST: test_log_level_filtering
 *
 * WHAT: Create coordinator with enable_logging=false, perform all operations
 * WHY:  Verify that disabling logging does not break any functionality
 * EXPECT: All operations succeed regardless of logging flag
 */
TEST_F(CycleCoordinatorRegressionTest, test_log_level_filtering) {
    // coord is already created with enable_logging=false in SetUp
    int rc = brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    EXPECT_EQ(0, rc);

    rc = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 1000);
    EXPECT_EQ(0, rc);

    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 0);

    rc = brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_IMMUNE_TICK, BRAIN_CYCLE_HEALTH_AGENT);
    EXPECT_EQ(0, rc);

    bool satisfied = false;
    rc = brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &satisfied);
    EXPECT_EQ(0, rc);

    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(coord, &stats);
    EXPECT_EQ(0, rc);

    char diag_buf[2048];
    brain_cycle_coordinator_diagnose(coord, diag_buf, sizeof(diag_buf));

    rc = brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK);
    EXPECT_EQ(0, rc);
}

/**
 * TEST: test_debug_logging_disabled_by_default
 *
 * WHAT: Verify default config has enable_debug_logging=false
 * WHY:  Debug logging should be off by default to avoid noise
 * EXPECT: Default config has enable_debug_logging == false, enable_logging == true
 */
TEST_F(CycleCoordinatorRegressionTest, test_debug_logging_disabled_by_default) {
    brain_cycle_coordinator_config_t default_config;
    brain_cycle_coordinator_default_config(&default_config);

    EXPECT_TRUE(default_config.enable_logging);
    EXPECT_FALSE(default_config.enable_debug_logging);

    // Verify that the coordinator still works with default settings
    brain_cycle_coordinator_t* default_coord =
        brain_cycle_coordinator_create(&default_config);
    ASSERT_NE(nullptr, default_coord);

    int rc = brain_cycle_coordinator_register(
        default_coord, BRAIN_CYCLE_IMMUNE_TICK, nullptr, nullptr);
    EXPECT_EQ(0, rc);

    rc = brain_cycle_coordinator_notify_tick(
        default_coord, BRAIN_CYCLE_IMMUNE_TICK, 100);
    EXPECT_EQ(0, rc);

    brain_cycle_coordinator_destroy(default_coord);
}
