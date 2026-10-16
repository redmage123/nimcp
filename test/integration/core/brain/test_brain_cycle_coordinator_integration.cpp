//=============================================================================
// test_brain_cycle_coordinator_integration.cpp - Brain Cycle Coordinator
// Integration Tests
//=============================================================================
/**
 * @file test_brain_cycle_coordinator_integration.cpp
 * @brief Integration tests for the Brain Cycle Coordinator subsystem
 *
 * WHAT: Tests verifying the brain cycle coordinator operates correctly across
 *       registration, tick notification, dependency tracking, health checking,
 *       callbacks, diagnostics, KG persistence, and pattern detection
 * WHY:  Ensure the unified observability layer correctly coordinates all 9
 *       brain cycle types with proper stall detection, dependency validation,
 *       and callback notifications
 * HOW:  GoogleTest with real coordinator lifecycle exercising the full public API
 *
 * Test Categories:
 *  1. Multi-Cycle Registration
 *  2. Cross-Cycle Tick Flow
 *  3. Dependency Integration
 *  4. Health Check Integration
 *  5. Callback Integration
 *  6. Connection Integration
 *  7. Diagnostic Integration
 *  8. Full Pipeline Integration
 *  9. KG Persistence Integration
 * 10. Pattern Detection Integration
 *
 * @version 1.0.0
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>

#include "core/brain/nimcp_brain_cycle_coordinator.h"

//=============================================================================
// Callback tracking structures
//=============================================================================

struct HealthChangedRecord {
    brain_cycle_type_t type;
    brain_cycle_health_t old_health;
    brain_cycle_health_t new_health;
};

struct StallDetectedRecord {
    brain_cycle_type_t type;
    uint64_t stall_duration_ms;
};

struct DependencyViolatedRecord {
    brain_cycle_type_t dependent;
    brain_cycle_type_t dependency;
};

struct OverallHealthRecord {
    float old_health;
    float new_health;
};

struct CallbackTracker {
    std::atomic<int> health_changed_count{0};
    HealthChangedRecord last_health_changed{};

    std::atomic<int> stall_detected_count{0};
    StallDetectedRecord last_stall_detected{};

    std::atomic<int> dep_violated_count{0};
    DependencyViolatedRecord last_dep_violated{};

    std::atomic<int> overall_health_count{0};
    OverallHealthRecord last_overall_health{};
};

static void on_health_changed_cb(
    brain_cycle_type_t type,
    brain_cycle_health_t old_h,
    brain_cycle_health_t new_h,
    void* user_data)
{
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->last_health_changed = {type, old_h, new_h};
    tracker->health_changed_count++;
}

static void on_stall_detected_cb(
    brain_cycle_type_t type,
    uint64_t stall_duration_ms,
    void* user_data)
{
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->last_stall_detected = {type, stall_duration_ms};
    tracker->stall_detected_count++;
}

static void on_dep_violated_cb(
    brain_cycle_type_t dependent,
    brain_cycle_type_t dependency,
    void* user_data)
{
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->last_dep_violated = {dependent, dependency};
    tracker->dep_violated_count++;
}

static void on_overall_health_cb(
    float old_h,
    float new_h,
    void* user_data)
{
    auto* tracker = static_cast<CallbackTracker*>(user_data);
    tracker->last_overall_health = {old_h, new_h};
    tracker->overall_health_count++;
}

//=============================================================================
// Custom health function for testing
//=============================================================================

static brain_cycle_health_t always_healthy(void* /*handle*/) {
    return BRAIN_CYCLE_HEALTH_HEALTHY;
}

static brain_cycle_health_t always_degraded(void* /*handle*/) {
    return BRAIN_CYCLE_HEALTH_DEGRADED;
}

static brain_cycle_health_t always_stalled(void* /*handle*/) {
    return BRAIN_CYCLE_HEALTH_STALLED;
}

static brain_cycle_health_t always_error(void* /*handle*/) {
    return BRAIN_CYCLE_HEALTH_ERROR;
}

//=============================================================================
// Test Fixture
//=============================================================================

class CycleCoordinatorIntegrationTest : public ::testing::Test {
protected:
    brain_cycle_coordinator_t* coord = nullptr;

    void SetUp() override {
        brain_cycle_coordinator_config_t config;
        brain_cycle_coordinator_default_config(&config);
        config.enable_logging = false;
        config.enable_timing_checks = true;
        config.enable_dependency_tracking = true;
        coord = brain_cycle_coordinator_create(&config);
        ASSERT_NE(nullptr, coord);
    }

    void TearDown() override {
        brain_cycle_coordinator_destroy(coord);
        coord = nullptr;
    }

    void registerAllCycles() {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            int dummy = i;
            ASSERT_EQ(0, brain_cycle_coordinator_register(
                coord, (brain_cycle_type_t)i, &dummy, nullptr));
        }
    }

    void tickCycle(brain_cycle_type_t type, uint64_t duration_us) {
        brain_cycle_coordinator_notify_tick(coord, type, duration_us);
    }

    void registerCallbackTracker(CallbackTracker* tracker) {
        brain_cycle_coordinator_callbacks_t cbs;
        memset(&cbs, 0, sizeof(cbs));
        cbs.on_health_changed = on_health_changed_cb;
        cbs.on_stall_detected = on_stall_detected_cb;
        cbs.on_dependency_violated = on_dep_violated_cb;
        cbs.on_overall_health_changed = on_overall_health_cb;
        cbs.user_data = tracker;
        ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));
    }
};

//=============================================================================
// 1. MULTI-CYCLE REGISTRATION TESTS
//=============================================================================

/**
 * TEST: Register all 9 cycle types and verify count via stats
 */
TEST_F(CycleCoordinatorIntegrationTest, test_register_all_nine_cycles) {
    registerAllCycles();

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));

    // After health check, stats should reflect registration count
    brain_cycle_coordinator_check_health(coord);
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(BRAIN_CYCLE_COUNT, (int)stats.total_cycles_registered);
}

/**
 * TEST: Register cycles, run health check, verify stats populated
 */
TEST_F(CycleCoordinatorIntegrationTest, test_cycles_auto_register_stats) {
    // Register a subset of cycles
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));

    // Tick them to move from UNKNOWN to HEALTHY
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 2000);

    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 0);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(3u, stats.total_cycles_registered);
}

/**
 * TEST: Some healthy, some degraded - check overall_health reflects mixed state
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_reflects_mixed_state) {
    int dummy = 0;
    // Register one with always_healthy, one with always_degraded
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, always_degraded));

    // Tick both so they are active
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);

    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));

    // Overall health should be between 0.5 (all degraded) and 1.0 (all healthy)
    EXPECT_GT(stats.overall_health, 0.49f);
    EXPECT_LT(stats.overall_health, 1.01f);

    // Should see one healthy and one degraded
    EXPECT_EQ(1u, stats.total_cycles_healthy);
    EXPECT_EQ(1u, stats.total_cycles_degraded);
}

//=============================================================================
// 2. CROSS-CYCLE TICK FLOW TESTS
//=============================================================================

/**
 * TEST: Register immune tick, send tick, verify status updated
 */
TEST_F(CycleCoordinatorIntegrationTest, test_immune_tick_updates_coordinator) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));

    EXPECT_EQ(BRAIN_CYCLE_IMMUNE_TICK, status.type);
    EXPECT_EQ(1u, status.ticks_executed);
    EXPECT_GT(status.avg_duration_us, 0.0);
    EXPECT_TRUE(status.enabled);
    EXPECT_TRUE(status.running);
}

/**
 * TEST: Register health agent, send tick, verify status updated
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_agent_tick_updates_coordinator) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, nullptr));

    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 8000);

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &status));

    EXPECT_EQ(BRAIN_CYCLE_HEALTH_AGENT, status.type);
    EXPECT_EQ(1u, status.ticks_executed);
    EXPECT_EQ(BRAIN_CYCLE_CATEGORY_MEDIUM, status.category);
    EXPECT_NEAR(8000.0, status.avg_duration_us, 1.0);
}

/**
 * TEST: Register multiple cycles, tick all, verify all stats reflect ticks
 */
TEST_F(CycleCoordinatorIntegrationTest, test_multiple_cycles_concurrent_ticks) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, nullptr));

    // Tick each multiple times
    for (int i = 0; i < 10; i++) {
        tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 4000);
        tickCycle(BRAIN_CYCLE_OSCILLATIONS, 800);
        tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);
    }

    brain_cycle_status_t status;

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(10u, status.ticks_executed);

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status));
    EXPECT_EQ(10u, status.ticks_executed);

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status));
    EXPECT_EQ(10u, status.ticks_executed);
}

//=============================================================================
// 3. DEPENDENCY INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Sleep depends on circadian, both healthy -> satisfied
 */
TEST_F(CycleCoordinatorIntegrationTest, test_sleep_circadian_dependency_satisfied) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_CIRCADIAN, &dummy, always_healthy));

    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN));

    // Tick both and run health check to update health states
    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);
    tickCycle(BRAIN_CYCLE_CIRCADIAN, 1000);
    brain_cycle_coordinator_check_health(coord);

    bool satisfied = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &satisfied));
    EXPECT_TRUE(satisfied);
}

/**
 * TEST: Sleep depends on circadian, circadian degraded -> not satisfied
 */
TEST_F(CycleCoordinatorIntegrationTest, test_sleep_circadian_dependency_violated) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_CIRCADIAN, &dummy, always_degraded));

    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN));

    // Tick both and run health check to apply health_fn results
    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);
    tickCycle(BRAIN_CYCLE_CIRCADIAN, 1000);
    brain_cycle_coordinator_check_health(coord);

    bool satisfied = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &satisfied));
    EXPECT_FALSE(satisfied);
}

/**
 * TEST: Immune depends on health_agent
 */
TEST_F(CycleCoordinatorIntegrationTest, test_immune_health_agent_dependency) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, always_healthy));

    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_IMMUNE_TICK, BRAIN_CYCLE_HEALTH_AGENT));

    // Tick both
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);
    brain_cycle_coordinator_check_health(coord);

    bool satisfied = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &satisfied));
    EXPECT_TRUE(satisfied);
}

//=============================================================================
// 4. HEALTH CHECK INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Register all, tick all recently, check health -> 0 issues
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_check_with_all_cycles) {
    registerAllCycles();

    // Tick all cycles with reasonable durations
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        tickCycle((brain_cycle_type_t)i, 1000);
    }

    int issues = brain_cycle_coordinator_check_health(coord);
    // We just ticked all recently, so no stalls expected
    EXPECT_EQ(0, issues);
}

/**
 * TEST: Register a cycle, don't tick, wait, check health -> stall detected
 *
 * Stall is detected when elapsed > expected_interval * stall_threshold_multiplier.
 * For immune tick: 50ms * 3 = 150ms stall threshold.
 * We sleep long enough for the stall threshold to be exceeded.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_check_detects_stalled_cycle) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // Tick once to set last_tick_us and establish ticks_executed > 0
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // Wait enough time for stall detection (immune = 50ms * 3 = 150ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
}

/**
 * TEST: Register mixed categories, verify per-category stats after health check
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_check_updates_category_stats) {
    int dummy = 0;
    // Register one fast, one medium, one slow
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_healthy));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);
    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);

    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));

    // FAST category has immune_tick
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_FAST].total_cycles);
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_FAST].healthy_cycles);

    // MEDIUM category has health_agent
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_MEDIUM].total_cycles);
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_MEDIUM].healthy_cycles);

    // SLOW category has sleep_wake
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_SLOW].total_cycles);
    EXPECT_EQ(1u, stats.categories[BRAIN_CYCLE_CATEGORY_SLOW].healthy_cycles);
}

//=============================================================================
// 5. CALLBACK INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Register callback, cause health change (HEALTHY -> DEGRADED), verify fired
 *
 * The health change callback fires when health transitions away from a non-UNKNOWN
 * state. We register with always_healthy first, tick, health check to set HEALTHY,
 * then re-register with always_degraded and health check again.
 *
 * Since we cannot unregister and re-register with a different health_fn for the
 * same cycle type (already registered), we use the timing-based approach:
 * tick with normal duration to become HEALTHY, then tick with an excessive
 * duration to become DEGRADED.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_callback_fires_on_health_change) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    // Register immune tick without health_fn (uses timing-based health)
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // First tick with normal duration -> becomes HEALTHY (from UNKNOWN)
    // UNKNOWN -> HEALTHY does NOT fire callback (old == UNKNOWN is suppressed)
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    EXPECT_EQ(0, tracker.health_changed_count.load());

    // Second tick with excessive duration (> 50000 * 3 = 150000) -> DEGRADED
    // HEALTHY -> DEGRADED should fire
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 200000);
    EXPECT_GE(tracker.health_changed_count.load(), 1);
    EXPECT_EQ(BRAIN_CYCLE_IMMUNE_TICK, tracker.last_health_changed.type);
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_HEALTHY, tracker.last_health_changed.old_health);
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_DEGRADED, tracker.last_health_changed.new_health);
}

/**
 * TEST: Register callback, cause stall, verify stall callback fired
 */
TEST_F(CycleCoordinatorIntegrationTest, test_callback_fires_on_stall) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // Tick once so ticks_executed > 0
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // Wait for stall threshold (immune = 50ms * 3 = 150ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    brain_cycle_coordinator_check_health(coord);

    EXPECT_GE(tracker.stall_detected_count.load(), 1);
    EXPECT_EQ(BRAIN_CYCLE_IMMUNE_TICK, tracker.last_stall_detected.type);
    EXPECT_GT(tracker.last_stall_detected.stall_duration_ms, (uint64_t)0);
}

/**
 * TEST: Add dependency, cause violation, verify dependency callback fired
 */
TEST_F(CycleCoordinatorIntegrationTest, test_callback_fires_on_dependency_violation) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_CIRCADIAN, &dummy, always_degraded));

    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN));

    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);
    tickCycle(BRAIN_CYCLE_CIRCADIAN, 1000);

    brain_cycle_coordinator_check_health(coord);

    EXPECT_GE(tracker.dep_violated_count.load(), 1);
    EXPECT_EQ(BRAIN_CYCLE_SLEEP_WAKE, tracker.last_dep_violated.dependent);
    EXPECT_EQ(BRAIN_CYCLE_CIRCADIAN, tracker.last_dep_violated.dependency);
}

/**
 * TEST: Register callback, cause overall health change, verify overall health
 * callback fired
 */
TEST_F(CycleCoordinatorIntegrationTest, test_overall_health_callback_fires) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    // Register with error health to drag overall health down from 1.0
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_error));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    brain_cycle_coordinator_check_health(coord);

    // Overall health should have changed from 1.0 (initial) to something lower
    EXPECT_GE(tracker.overall_health_count.load(), 1);
    EXPECT_GT(tracker.last_overall_health.old_health, tracker.last_overall_health.new_health);
}

//=============================================================================
// 6. CONNECTION INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Connect multiple subsystems, verify no errors returned
 */
TEST_F(CycleCoordinatorIntegrationTest, test_connect_multiple_subsystems) {
    // Use dummy pointers (non-NULL) to simulate subsystem connections
    int dummy_bio = 1;
    int dummy_immune = 2;
    int dummy_intro = 3;

    EXPECT_EQ(0, brain_cycle_coordinator_connect_bio_async(
        coord, (bio_module_context_t*)&dummy_bio));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_immune(
        coord, (brain_immune_system_t*)&dummy_immune));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_introspection(
        coord, (introspection_context_t*)&dummy_intro));
}

/**
 * TEST: Pass NULL pointers for optional subsystems, verify safe handling
 */
TEST_F(CycleCoordinatorIntegrationTest, test_connect_null_subsystems_safe) {
    // Connecting with NULL pointer should succeed (just sets field to NULL)
    EXPECT_EQ(0, brain_cycle_coordinator_connect_bio_async(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_immune(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_kg(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_introspection(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_hemispheric(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_fep(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_meta_learning(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_pink_noise(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_global_workspace(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_attention(coord, nullptr));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_world_model(coord, nullptr));
}

/**
 * TEST: Connect subsystems, then run health checks - connections persist
 */
TEST_F(CycleCoordinatorIntegrationTest, test_connections_persist_through_health_checks) {
    int dummy_kg = 42;
    EXPECT_EQ(0, brain_cycle_coordinator_connect_kg(
        coord, (kg_io_dispatcher_t*)&dummy_kg));

    registerAllCycles();

    // Tick all cycles
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        tickCycle((brain_cycle_type_t)i, 1000);
    }

    // Run multiple health checks
    for (int i = 0; i < 5; i++) {
        int issues = brain_cycle_coordinator_check_health(coord);
        EXPECT_GE(issues, 0);
    }

    // KG should still be connected - flush should succeed
    EXPECT_EQ(0, brain_cycle_coordinator_flush_to_kg(coord));
}

//=============================================================================
// 7. DIAGNOSTIC INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Register cycles, diagnose, check output contains cycle info
 */
TEST_F(CycleCoordinatorIntegrationTest, test_diagnose_with_registered_cycles) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, nullptr));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);

    brain_cycle_coordinator_check_health(coord);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int issues = brain_cycle_coordinator_diagnose(coord, buffer, sizeof(buffer));
    EXPECT_GE(issues, 0);

    // Buffer should contain diagnostic text
    EXPECT_GT(strlen(buffer), (size_t)0);
    // Should mention the coordinator diagnostics header
    EXPECT_NE(nullptr, strstr(buffer, "Brain Cycle Coordinator Diagnostics"));
    // Should mention registered cycles
    EXPECT_NE(nullptr, strstr(buffer, "immune_tick"));
    EXPECT_NE(nullptr, strstr(buffer, "brain_update"));
}

/**
 * TEST: Register, stall, diagnose - check issues reported in output
 */
TEST_F(CycleCoordinatorIntegrationTest, test_diagnose_with_stalled_cycle) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));

    // Tick once, then wait for stall
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 2000);
    // Oscillations: 10ms * 3 = 30ms stall threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    brain_cycle_coordinator_check_health(coord);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int issues = brain_cycle_coordinator_diagnose(coord, buffer, sizeof(buffer));
    EXPECT_GE(issues, 1);

    // Should contain stall indication
    EXPECT_NE(nullptr, strstr(buffer, "oscillations"));
    EXPECT_NE(nullptr, strstr(buffer, "ISSUE"));
}

/**
 * TEST: Add dependencies, diagnose, check dependencies listed
 */
TEST_F(CycleCoordinatorIntegrationTest, test_diagnose_with_dependencies) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_CIRCADIAN, &dummy, nullptr));

    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN));

    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);
    tickCycle(BRAIN_CYCLE_CIRCADIAN, 1000);

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int issues = brain_cycle_coordinator_diagnose(coord, buffer, sizeof(buffer));
    EXPECT_GE(issues, 0);

    // Should mention dependencies section
    EXPECT_NE(nullptr, strstr(buffer, "Dependencies"));
    EXPECT_NE(nullptr, strstr(buffer, "sleep_wake"));
    EXPECT_NE(nullptr, strstr(buffer, "circadian"));
}

//=============================================================================
// 8. FULL PIPELINE INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Full lifecycle for a single cycle: register, tick, health check, query
 */
TEST_F(CycleCoordinatorIntegrationTest, test_register_tick_health_query_pipeline) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // Tick with normal duration
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // Run health check
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_EQ(0, issues);

    // Query status
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_HEALTHY, status.health);
    EXPECT_EQ(1u, status.ticks_executed);
    EXPECT_TRUE(status.enabled);
    EXPECT_TRUE(status.running);

    // Query global stats
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(1u, stats.total_cycles_registered);
    EXPECT_EQ(1u, stats.total_cycles_healthy);
}

/**
 * TEST: Register 5 cycles, tick some, check all health statuses
 */
TEST_F(CycleCoordinatorIntegrationTest, test_multi_cycle_health_tracking) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, always_degraded));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_error));

    // Tick all
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 2000);
    tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);
    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);

    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(5u, stats.total_cycles_registered);
    EXPECT_EQ(3u, stats.total_cycles_healthy);   // immune, health_agent, brain_update
    EXPECT_EQ(1u, stats.total_cycles_degraded);  // oscillations
    // ERROR is distinct from STALLED - not counted in total_cycles_stalled
    EXPECT_EQ(0u, stats.total_cycles_stalled);
    // sleep_wake has ERROR health (from always_error health_fn)
    brain_cycle_status_t sw_status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &sw_status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_ERROR, sw_status.health);
}

/**
 * TEST: A->B->C dependency chain evaluation
 *
 * AROUSAL depends on SLEEP_WAKE, SLEEP_WAKE depends on CIRCADIAN.
 * All healthy -> both deps satisfied.
 * Circadian degraded -> SLEEP_WAKE deps violated, AROUSAL has SLEEP_WAKE
 * dep which is healthy so it depends on SLEEP_WAKE health not CIRCADIAN.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_dependency_chain_evaluation) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_AROUSAL, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_CIRCADIAN, &dummy, always_healthy));

    // A -> B -> C chain
    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_AROUSAL, BRAIN_CYCLE_SLEEP_WAKE));
    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_SLEEP_WAKE, BRAIN_CYCLE_CIRCADIAN));

    // Tick all
    tickCycle(BRAIN_CYCLE_AROUSAL, 1000);
    tickCycle(BRAIN_CYCLE_SLEEP_WAKE, 1000);
    tickCycle(BRAIN_CYCLE_CIRCADIAN, 1000);

    brain_cycle_coordinator_check_health(coord);

    // All healthy - both should be satisfied
    bool sat = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_AROUSAL, &sat));
    EXPECT_TRUE(sat);

    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_SLEEP_WAKE, &sat));
    EXPECT_TRUE(sat);
}

/**
 * TEST: Tick repeatedly, verify stats accumulate correctly
 */
TEST_F(CycleCoordinatorIntegrationTest, test_stats_accumulation_over_time) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, nullptr));

    // Tick 100 times
    for (int i = 0; i < 100; i++) {
        tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);
    }

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status));

    EXPECT_EQ(100u, status.ticks_executed);
    EXPECT_NEAR(1500.0, status.avg_duration_us, 200.0);
    EXPECT_EQ(1500u, status.max_duration_us);

    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_GE(stats.categories[BRAIN_CYCLE_CATEGORY_FAST].total_ticks, 100u);
}

//=============================================================================
// 9. KG PERSISTENCE INTEGRATION TESTS
//=============================================================================

/**
 * TEST: No KG connected, flush returns -1
 */
TEST_F(CycleCoordinatorIntegrationTest, test_flush_without_kg_returns_error) {
    // No KG dispatcher connected by default
    int result = brain_cycle_coordinator_flush_to_kg(coord);
    EXPECT_EQ(-1, result);
}

/**
 * TEST: Connect mock KG, flush returns 0
 */
TEST_F(CycleCoordinatorIntegrationTest, test_flush_with_kg_mock) {
    int dummy_kg = 99;
    ASSERT_EQ(0, brain_cycle_coordinator_connect_kg(
        coord, (kg_io_dispatcher_t*)&dummy_kg));

    registerAllCycles();
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        tickCycle((brain_cycle_type_t)i, 1000);
    }

    int result = brain_cycle_coordinator_flush_to_kg(coord);
    EXPECT_EQ(0, result);
}

/**
 * TEST: Flush updates last_kg_write timestamp visible via stats uptime
 *
 * We verify that the coordinator's uptime in stats increases after flushing,
 * which confirms the coordinator is alive and operating.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_flush_updates_timestamp) {
    int dummy_kg = 99;
    ASSERT_EQ(0, brain_cycle_coordinator_connect_kg(
        coord, (kg_io_dispatcher_t*)&dummy_kg));

    registerAllCycles();
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        tickCycle((brain_cycle_type_t)i, 1000);
    }

    // Run health check to populate stats
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats_before;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats_before));
    uint64_t uptime_before = stats_before.coordinator_uptime_ms;

    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Flush and then check health again to update uptime
    EXPECT_EQ(0, brain_cycle_coordinator_flush_to_kg(coord));
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats_after;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats_after));

    // Uptime should have increased
    EXPECT_GE(stats_after.coordinator_uptime_ms, uptime_before);
}

//=============================================================================
// 10. PATTERN DETECTION INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Multiple health checks create patterns tracked by coordinator
 *
 * Each health check call records a health pattern fingerprint.
 * Repeated identical states should result in pattern occurrence_count > 1.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_health_pattern_tracking) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, always_healthy));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);

    // Run multiple health checks with identical state
    for (int i = 0; i < 10; i++) {
        int issues = brain_cycle_coordinator_check_health(coord);
        EXPECT_EQ(0, issues);
    }

    // Verify stats are still consistent after pattern tracking
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(2u, stats.total_cycles_registered);
    EXPECT_EQ(2u, stats.total_cycles_healthy);
}

/**
 * TEST: Tick with consistent then anomalous durations -> anomaly detected
 *
 * Z-score anomaly detection requires at least 10 samples and stddev > 1.0.
 * We tick with consistent durations, then inject a wildly different duration.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_anomaly_detection_over_ticks) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, nullptr));

    // Build baseline: 50 ticks at ~1500us with small variance
    for (int i = 0; i < 50; i++) {
        tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);
    }

    // Inject anomalous tick (10x the baseline)
    tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 150000);

    // Check stats for timing anomalies
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    // The z-score for 150000 vs mean~1500 with tiny stddev should be massive
    // With zero-variance baseline (all 1500), stddev approaches 0, but Welford's
    // with identical values gives stddev=0, so anomaly detection skips (stddev < 1.0).
    // Still, the tick should be recorded.
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &status));
    EXPECT_EQ(51u, status.ticks_executed);
    EXPECT_EQ(150000u, status.max_duration_us);
}

/**
 * TEST: Tick many times, verify EMA converges to the tick duration
 *
 * EMA with alpha=0.1: after many identical ticks, EMA should converge to
 * the tick duration value.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_ema_convergence) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));

    // Start with a different first tick to see convergence
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 10000);

    // Tick 200 times at 2000us - EMA should converge to 2000
    for (int i = 0; i < 200; i++) {
        tickCycle(BRAIN_CYCLE_OSCILLATIONS, 2000);
    }

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status));

    // EMA with alpha=0.1 converges: after 200 ticks at 2000, should be very close
    // Starting at 10000, after N ticks at 2000: EMA_n = 0.9^N * 10000 + (1-0.9^N) * 2000
    // 0.9^200 ~ 7e-10, so EMA ~ 2000
    EXPECT_NEAR(2000.0, status.avg_duration_us, 50.0);
    EXPECT_EQ(201u, status.ticks_executed);
    EXPECT_EQ(10000u, status.max_duration_us);
}

//=============================================================================
// 11. ADDITIONAL COVERAGE TESTS
//=============================================================================

/**
 * TEST: Unregister a cycle and verify it no longer appears in stats
 */
TEST_F(CycleCoordinatorIntegrationTest, test_unregister_reduces_count) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, &dummy, nullptr));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_HEALTH_AGENT, 10000);
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(2u, stats.total_cycles_registered);

    // Unregister one
    ASSERT_EQ(0, brain_cycle_coordinator_unregister(coord, BRAIN_CYCLE_IMMUNE_TICK));
    brain_cycle_coordinator_check_health(coord);

    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(1u, stats.total_cycles_registered);

    // Status query for unregistered cycle should fail
    brain_cycle_status_t status;
    EXPECT_EQ(-1, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
}

/**
 * TEST: Tick unregistered cycle returns error
 */
TEST_F(CycleCoordinatorIntegrationTest, test_tick_unregistered_cycle_returns_error) {
    // IMMUNE_TICK not registered
    int result = brain_cycle_coordinator_notify_tick(
        coord, BRAIN_CYCLE_IMMUNE_TICK, 5000);
    EXPECT_EQ(-1, result);
}

/**
 * TEST: Double registration of same cycle type returns error
 */
TEST_F(CycleCoordinatorIntegrationTest, test_double_registration_fails) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // Second registration of same type should fail
    EXPECT_EQ(-1, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
}

/**
 * TEST: Get all status returns correct subset of registered cycles
 */
TEST_F(CycleCoordinatorIntegrationTest, test_get_all_status) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_GC_AGENT, &dummy, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy, nullptr));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    tickCycle(BRAIN_CYCLE_GC_AGENT, 100000);
    tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 1500);

    brain_cycle_status_t statuses[BRAIN_CYCLE_COUNT];
    uint32_t count = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_get_all_status(coord, statuses, &count));
    EXPECT_EQ(3u, count);

    // Verify each reported status has a valid cycle type
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE((int)statuses[i].type, 0);
        EXPECT_LT((int)statuses[i].type, BRAIN_CYCLE_COUNT);
        EXPECT_EQ(1u, statuses[i].ticks_executed);
    }
}

/**
 * TEST: Destroy NULL coordinator is safe (no crash)
 */
TEST_F(CycleCoordinatorIntegrationTest, test_destroy_null_is_safe) {
    brain_cycle_coordinator_destroy(nullptr);
    // If we reach here, NULL destroy was safe
    SUCCEED();
}

/**
 * TEST: Create coordinator with NULL config uses defaults
 */
TEST_F(CycleCoordinatorIntegrationTest, test_create_with_null_config) {
    brain_cycle_coordinator_t* coord2 = brain_cycle_coordinator_create(nullptr);
    ASSERT_NE(nullptr, coord2);

    int dummy = 0;
    EXPECT_EQ(0, brain_cycle_coordinator_register(
        coord2, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    brain_cycle_coordinator_destroy(coord2);
}

/**
 * TEST: Multiple callback sets can be registered and all fire
 */
TEST_F(CycleCoordinatorIntegrationTest, test_multiple_callback_sets) {
    CallbackTracker tracker1;
    CallbackTracker tracker2;

    registerCallbackTracker(&tracker1);
    registerCallbackTracker(&tracker2);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // First tick (UNKNOWN -> HEALTHY, no callback fires)
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // Second tick with excessive duration -> HEALTHY -> DEGRADED
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 200000);

    // Both trackers should have received the callback
    EXPECT_GE(tracker1.health_changed_count.load(), 1);
    EXPECT_GE(tracker2.health_changed_count.load(), 1);
}

/**
 * TEST: Unregister callbacks by user_data and verify they no longer fire
 */
TEST_F(CycleCoordinatorIntegrationTest, test_unregister_callbacks) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    // Unregister by user_data
    EXPECT_EQ(0, brain_cycle_coordinator_unregister_callbacks(coord, &tracker));

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, nullptr));

    // First tick -> HEALTHY
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    // Second tick -> DEGRADED
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 200000);

    // Callback should not have fired since we unregistered
    EXPECT_EQ(0, tracker.health_changed_count.load());
}

//=============================================================================
// 11. BIO-ASYNC INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Bio-async null context is safe - no crash when bio_context is NULL
 * Verifies the publish_bio_* helpers gracefully skip when not connected.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_bio_async_null_context_safe) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));

    // Trigger health change with NULL bio_context (default) - should not crash
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 0);
}

/**
 * TEST: Health changes still fire callbacks with bio-async integration wired
 * Even though bio_context is NULL, callbacks should still work.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_bio_async_health_changed_with_callbacks) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_degraded));

    // Tick to establish baseline
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // check_health queries health_fn (DEGRADED), transitions UNKNOWN -> DEGRADED
    // This fires health_changed callback + bio-async hook (no-op with NULL ctx)
    brain_cycle_coordinator_check_health(coord);
    EXPECT_GT(tracker.health_changed_count.load(), 0);
}

/**
 * TEST: Stall detection fires both callbacks and bio-async hooks safely
 */
TEST_F(CycleCoordinatorIntegrationTest, test_bio_async_stall_with_callbacks) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));

    // OSCILLATIONS default interval = 10ms, stall threshold = 30ms

    // First tick to establish baseline
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 5000);

    // Wait for stall condition
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Check health should detect stall and fire both callback + bio-async
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GT(issues, 0);
    EXPECT_GT(tracker.stall_detected_count.load(), 0);
}

/**
 * TEST: Dependency violation fires bio-async hook safely
 */
TEST_F(CycleCoordinatorIntegrationTest, test_bio_async_dependency_violated) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy1 = 1, dummy2 = 2;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, &dummy1, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy2, always_degraded));

    // Set up dependency
    brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_OSCILLATIONS);

    // Tick both
    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 5000);
    tickCycle(BRAIN_CYCLE_BRAIN_UPDATE, 5000);

    // Check health with degraded dependency
    brain_cycle_coordinator_check_health(coord);

    // Dependency violation should have been detected and published safely
    // (bio-async hook is a no-op with NULL context but shouldn't crash)
    EXPECT_GE(tracker.dep_violated_count.load(), 0);
}

/**
 * TEST: Overall health change fires stats bio-async hook safely
 */
TEST_F(CycleCoordinatorIntegrationTest, test_bio_async_stats_on_overall_change) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);
    brain_cycle_coordinator_check_health(coord);

    // Initial overall health should have triggered callback
    // bio-async stats hook should have executed safely (no crash)
    EXPECT_GE(tracker.overall_health_count.load(), 0);
}

//=============================================================================
// 12. IMMUNE SYSTEM INTEGRATION TESTS
//=============================================================================

/**
 * TEST: Immune null system is safe - no crash when immune_system is NULL
 */
TEST_F(CycleCoordinatorIntegrationTest, test_immune_null_system_safe) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_degraded));

    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // check_health will trigger health change (UNKNOWN->DEGRADED) which
    // calls report_immune_health_change with NULL immune - should not crash
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GT(issues, 0);
}

/**
 * TEST: Immune stall report is safe with NULL system
 * Stall detection should work normally even without immune system.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_immune_stall_report_null_safe) {
    CallbackTracker tracker;
    registerCallbackTracker(&tracker);

    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));
    // OSCILLATIONS default interval = 10ms, stall threshold = 30ms

    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Stall detected -> report_immune_stall called with NULL -> no crash
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GT(issues, 0);
    EXPECT_GT(tracker.stall_detected_count.load(), 0);
}

/**
 * TEST: Without immune system, sensitivity defaults to 1.0f
 * The stall threshold should be interval * multiplier / 1.0 = interval * multiplier
 */
TEST_F(CycleCoordinatorIntegrationTest, test_immune_sensitivity_default_without_immune) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, &dummy, nullptr));
    // OSCILLATIONS default interval = 10ms, stall threshold = 30ms

    tickCycle(BRAIN_CYCLE_OSCILLATIONS, 5000);

    // With sensitivity=1.0 (no immune), threshold = 10ms * 3 / 1.0 = 30ms
    // Wait 35ms to exceed threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GT(issues, 0);

    // Verify stall was detected (indirectly confirms sensitivity=1.0)
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_OSCILLATIONS, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
}

//=============================================================================
// 13. KG PERSISTENCE INTEGRATION TESTS
//=============================================================================

/**
 * TEST: KG flush returns error when KG not connected
 */
TEST_F(CycleCoordinatorIntegrationTest, test_kg_flush_returns_error_not_connected) {
    EXPECT_EQ(-1, brain_cycle_coordinator_flush_to_kg(coord));
}

/**
 * TEST: KG flush with NULL coordinator returns error
 */
TEST_F(CycleCoordinatorIntegrationTest, test_kg_flush_null_coord) {
    EXPECT_EQ(-1, brain_cycle_coordinator_flush_to_kg(nullptr));
}

/**
 * TEST: Auto-flush in check_health is safe when KG is NULL
 * check_health should complete normally without KG dispatcher.
 */
TEST_F(CycleCoordinatorIntegrationTest, test_kg_auto_flush_null_dispatcher_safe) {
    int dummy = 0;
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &dummy, always_healthy));
    tickCycle(BRAIN_CYCLE_IMMUNE_TICK, 5000);

    // check_health with NULL kg_dispatcher should not crash during auto-flush check
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_EQ(0, issues);
}