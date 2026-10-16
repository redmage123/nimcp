//=============================================================================
// test_brain_cycle_coordinator_e2e.cpp - Brain Cycle Coordinator GTest E2E
//=============================================================================
/**
 * @file test_brain_cycle_coordinator_e2e.cpp
 * @brief GTest end-to-end tests for the Brain Cycle Coordinator
 *
 * WHAT: E2E tests verifying the brain cycle coordinator manages all brain
 *       subsystem cycles, detects stalls, tracks health, and fires callbacks
 * WHY:  Validate that the coordinator correctly orchestrates the full set of
 *       brain cycles (immune tick, health agent, sleep-wake, circadian,
 *       arousal, oscillations, gc agent, io dispatcher, brain update)
 *       and responds to degradation / recovery in a realistic pipeline
 * HOW:  GoogleTest with mock health functions and callback tracking to
 *       exercise the full coordinator lifecycle
 *
 * @author NIMCP Development Team
 * @date 2026-01-26
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <unistd.h>

#include "core/brain/nimcp_brain_cycle_coordinator.h"

//=============================================================================
// Callback Tracking
//=============================================================================

struct E2ECallbackTracker {
    int health_changed = 0;
    int stall_detected = 0;
    int dep_violated = 0;
    int overall_changed = 0;
};

static void e2e_on_health_changed(brain_cycle_type_t,
                                   brain_cycle_health_t,
                                   brain_cycle_health_t,
                                   void* ud)
{
    ((E2ECallbackTracker*)ud)->health_changed++;
}

static void e2e_on_stall(brain_cycle_type_t, uint64_t, void* ud)
{
    ((E2ECallbackTracker*)ud)->stall_detected++;
}

static void e2e_on_dep_violated(brain_cycle_type_t, brain_cycle_type_t, void* ud)
{
    ((E2ECallbackTracker*)ud)->dep_violated++;
}

static void e2e_on_overall(float, float, void* ud)
{
    ((E2ECallbackTracker*)ud)->overall_changed++;
}

//=============================================================================
// Mock Health Functions
//=============================================================================

static brain_cycle_health_t mock_return_error(void*)
{
    return BRAIN_CYCLE_HEALTH_ERROR;
}

static brain_cycle_health_t mock_return_degraded(void*)
{
    return BRAIN_CYCLE_HEALTH_DEGRADED;
}

static brain_cycle_health_t mock_return_healthy(void*)
{
    return BRAIN_CYCLE_HEALTH_HEALTHY;
}

static brain_cycle_health_t mock_return_disabled(void*)
{
    return BRAIN_CYCLE_HEALTH_DISABLED;
}

/** Configurable per-cycle health states (indexed by handle cast to int) */
static brain_cycle_health_t g_per_cycle_health[BRAIN_CYCLE_COUNT];

static brain_cycle_health_t mock_per_cycle_health(void* handle)
{
    int idx = (int)(uintptr_t)handle - 1;
    if (idx >= 0 && idx < BRAIN_CYCLE_COUNT) {
        return g_per_cycle_health[idx];
    }
    return BRAIN_CYCLE_HEALTH_HEALTHY;
}

//=============================================================================
// Test Fixture
//=============================================================================

class CycleCoordinatorE2ETest : public ::testing::Test {
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
            ASSERT_EQ(0, brain_cycle_coordinator_register(
                coord, (brain_cycle_type_t)i, (void*)(uintptr_t)(i + 1), nullptr));
        }
    }

    void tickAllCycles(uint64_t duration_us) {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            brain_cycle_coordinator_notify_tick(
                coord, (brain_cycle_type_t)i, duration_us);
        }
    }

    /**
     * Register all cycles with the per-cycle mock health function.
     * The handle is (void*)(uintptr_t)(i+1) so mock_per_cycle_health
     * can look up g_per_cycle_health[i].
     */
    void registerAllCyclesWithPerCycleHealth() {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            ASSERT_EQ(0, brain_cycle_coordinator_register(
                coord, (brain_cycle_type_t)i,
                (void*)(uintptr_t)(i + 1),
                mock_per_cycle_health));
        }
    }

    /**
     * Reset g_per_cycle_health to all HEALTHY.
     */
    void resetPerCycleHealth() {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            g_per_cycle_health[i] = BRAIN_CYCLE_HEALTH_HEALTHY;
        }
    }
};

//=============================================================================
// GROUP 1: Full System (4 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_brain_with_all_cycles_healthy) {
    // Register all 9 cycle types with no custom health_fn (timing-inferred)
    registerAllCycles();

    // Tick all cycles so they are considered active and recent
    tickAllCycles(1000);

    // Health check should report 0 issues
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_EQ(0, issues);

    // Verify overall health near 1.0
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_NEAR(1.0f, stats.overall_health, 0.1f);
    EXPECT_EQ((uint32_t)BRAIN_CYCLE_COUNT, stats.total_cycles_registered);
    EXPECT_EQ((uint32_t)BRAIN_CYCLE_COUNT, stats.total_cycles_healthy);
    EXPECT_EQ(0u, stats.total_cycles_degraded);
    EXPECT_EQ(0u, stats.total_cycles_stalled);
}

TEST_F(CycleCoordinatorE2ETest, test_detect_stalled_immune_tick) {
    // Register only the immune tick cycle
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, nullptr));

    // Tick once to establish a last_tick timestamp
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);

    // Sleep long enough to exceed stall threshold.
    // Default immune interval is 50000us, stall multiplier is 3 => 150000us stall.
    // Sleep 200ms to be safe.
    usleep(200000);

    // Health check should detect at least 1 issue (stalled immune tick)
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    // Verify the status shows stalled
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
}

TEST_F(CycleCoordinatorE2ETest, test_detect_stalled_health_agent) {
    // Register only the health agent cycle
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, (void*)(uintptr_t)1, nullptr));

    // Tick once to establish a last_tick timestamp
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_HEALTH_AGENT, 500);

    // Sleep long enough to exceed stall threshold.
    // Default health agent interval is 100000us, stall multiplier is 3 => 300000us.
    // Sleep 400ms to be safe.
    usleep(400000);

    // Health check should detect at least 1 issue (stalled health agent)
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    // Verify the status shows stalled
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_HEALTH_AGENT, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
}

TEST_F(CycleCoordinatorE2ETest, test_recovery_after_stall) {
    // Register immune tick
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, nullptr));

    // Tick once then let it stall
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    usleep(200000);

    // Verify stall is detected
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);

    // Now tick again to recover
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);

    // Health check should now show recovery
    issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_EQ(0, issues);

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_HEALTHY, status.health);
}

//=============================================================================
// GROUP 2: Observability (3 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_diagnose_full_system) {
    // Register all 9 cycles and tick them all
    registerAllCycles();
    tickAllCycles(1000);

    // Run health check to populate state
    brain_cycle_coordinator_check_health(coord);

    // Diagnose into a buffer
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int issue_count = brain_cycle_coordinator_diagnose(coord, buffer, sizeof(buffer));
    EXPECT_GE(issue_count, 0);

    // Verify the buffer contains all cycle type names
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        const char* name = brain_cycle_type_name((brain_cycle_type_t)i);
        EXPECT_NE(nullptr, strstr(buffer, name))
            << "Diagnostic buffer missing cycle: " << name;
    }
}

TEST_F(CycleCoordinatorE2ETest, test_log_state_output) {
    // Register some cycles and tick them
    registerAllCycles();
    tickAllCycles(1000);
    brain_cycle_coordinator_check_health(coord);

    // Call log_state - just verify no crash (NULL-safe too)
    brain_cycle_coordinator_log_state(coord);
    brain_cycle_coordinator_log_state(nullptr);  // NULL safe per API
}

TEST_F(CycleCoordinatorE2ETest, test_stats_accuracy_under_load) {
    // Register 5 cycles
    const int NUM_CYCLES = 5;
    const int TICKS_PER_CYCLE = 1000;
    const uint64_t DURATION_US = 500;

    for (int i = 0; i < NUM_CYCLES; i++) {
        ASSERT_EQ(0, brain_cycle_coordinator_register(
            coord, (brain_cycle_type_t)i, (void*)(uintptr_t)(i + 1), nullptr));
    }

    // Tick each cycle 1000 times
    for (int t = 0; t < TICKS_PER_CYCLE; t++) {
        for (int i = 0; i < NUM_CYCLES; i++) {
            brain_cycle_coordinator_notify_tick(
                coord, (brain_cycle_type_t)i, DURATION_US);
        }
    }

    // Verify stats
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ((uint32_t)NUM_CYCLES, stats.total_cycles_registered);

    // Verify per-cycle status shows correct tick count
    for (int i = 0; i < NUM_CYCLES; i++) {
        brain_cycle_status_t status;
        ASSERT_EQ(0, brain_cycle_coordinator_get_status(
            coord, (brain_cycle_type_t)i, &status));
        EXPECT_EQ((uint64_t)TICKS_PER_CYCLE, status.ticks_executed)
            << "Cycle " << i << " tick count mismatch";
        // Average duration should be close to DURATION_US
        EXPECT_NEAR((double)DURATION_US, status.avg_duration_us, (double)DURATION_US * 0.01)
            << "Cycle " << i << " average duration mismatch";
    }
}

//=============================================================================
// GROUP 3: Stress (3 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_high_frequency_notifications) {
    // Register immune tick
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, nullptr));

    // Tick 100000 times rapidly
    const uint64_t TOTAL_TICKS = 100000;
    for (uint64_t t = 0; t < TOTAL_TICKS; t++) {
        brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 100);
    }

    // Verify tick count matches
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(TOTAL_TICKS, status.ticks_executed);
}

TEST_F(CycleCoordinatorE2ETest, test_concurrent_health_checks) {
    // Register all cycles and tick them
    registerAllCycles();
    tickAllCycles(500);

    // Interleave ticks and health checks 1000 times
    for (int iter = 0; iter < 1000; iter++) {
        int cycle_idx = iter % BRAIN_CYCLE_COUNT;
        brain_cycle_coordinator_notify_tick(
            coord, (brain_cycle_type_t)cycle_idx, 200);
        int issues = brain_cycle_coordinator_check_health(coord);
        EXPECT_EQ(0, issues);
    }

    // All should still be healthy
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_NEAR(1.0f, stats.overall_health, 0.1f);
}

TEST_F(CycleCoordinatorE2ETest, test_long_running_stability) {
    // Register all cycles and tick them
    registerAllCycles();

    // Tick each cycle 50000 times total (rotating through cycles)
    const int TOTAL_ITERATIONS = 50000;
    for (int t = 0; t < TOTAL_ITERATIONS; t++) {
        for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
            brain_cycle_coordinator_notify_tick(
                coord, (brain_cycle_type_t)i, 100 + (t % 50));
        }
    }

    // Verify no issues
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_EQ(0, issues);

    // Verify stats are consistent
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_NEAR(1.0f, stats.overall_health, 0.1f);

    // Each cycle should have exactly TOTAL_ITERATIONS ticks
    for (int i = 0; i < BRAIN_CYCLE_COUNT; i++) {
        brain_cycle_status_t status;
        ASSERT_EQ(0, brain_cycle_coordinator_get_status(
            coord, (brain_cycle_type_t)i, &status));
        EXPECT_EQ((uint64_t)TOTAL_ITERATIONS, status.ticks_executed)
            << "Cycle " << i << " tick mismatch after long run";
    }
}

//=============================================================================
// GROUP 4: Full Integration Pipeline (4 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_exception_to_immune_flow) {
    // Register immune tick with a health_fn that returns ERROR
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_return_error));

    // Tick and check health
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    int issues = brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(issues, 1);

    // Verify stats reflect the error
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_LT(stats.overall_health, 1.0f);

    // Verify status shows ERROR
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_ERROR, status.health);
}

TEST_F(CycleCoordinatorE2ETest, test_callback_chain_full_pipeline) {
    E2ECallbackTracker tracker = {};

    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_stall_detected = e2e_on_stall;
    cbs.on_dependency_violated = e2e_on_dep_violated;
    cbs.on_overall_health_changed = e2e_on_overall;
    cbs.user_data = &tracker;

    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register immune tick with error health_fn to trigger health_changed callback
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_return_error));

    // Tick and check health -- should trigger on_health_changed (from UNKNOWN to ERROR)
    // and on_overall_health_changed
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    // Health changed should fire (transition from UNKNOWN)
    EXPECT_GE(tracker.health_changed, 1);
    // Overall changed should fire
    EXPECT_GE(tracker.overall_changed, 1);

    // Now set up a stall scenario for stall callback
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_HEALTH_AGENT, (void*)(uintptr_t)2, nullptr));
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_HEALTH_AGENT, 500);
    usleep(400000);  // exceed 3x 100000us = 300000us stall threshold
    brain_cycle_coordinator_check_health(coord);

    EXPECT_GE(tracker.stall_detected, 1);

    // Set up dependency violation
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, (void*)(uintptr_t)3, nullptr));
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_OSCILLATIONS, 500);

    // Oscillations depends on immune tick; immune tick is in ERROR state
    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_OSCILLATIONS, BRAIN_CYCLE_IMMUNE_TICK));

    brain_cycle_coordinator_check_health(coord);
    EXPECT_GE(tracker.dep_violated, 1);
}

TEST_F(CycleCoordinatorE2ETest, test_cycle_stall_full_recovery_flow) {
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_stall_detected = e2e_on_stall;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register immune tick
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, nullptr));

    // Initial healthy state
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_HEALTHY, status.health);

    // Let it stall
    usleep(200000);
    brain_cycle_coordinator_check_health(coord);

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_STALLED, status.health);
    EXPECT_GE(tracker.stall_detected, 1);

    // Recover by ticking again
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_HEALTHY, status.health);

    // Health changed should have fired for stall transition and recovery
    EXPECT_GE(tracker.health_changed, 2);
}

TEST_F(CycleCoordinatorE2ETest, test_pattern_detection_recurring_issues) {
    // Register immune tick with a health_fn that returns degraded
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_return_degraded));

    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);

    // Run health checks many times to accumulate patterns
    for (int round = 0; round < 50; round++) {
        brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
        brain_cycle_coordinator_check_health(coord);
    }

    // Verify stats reflect persistent degradation
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(1u, stats.total_cycles_degraded);
    EXPECT_EQ(0u, stats.total_cycles_healthy);
    EXPECT_LT(stats.overall_health, 1.0f);

    // Timing anomalies may or may not be detected, but the system remains stable
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_DEGRADED, status.health);
}

//=============================================================================
// GROUP 5: Bidirectional E2E (4 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_all_connections_accept_mock_pointers) {
    // Connect all 11 subsystem integration points with dummy non-NULL pointers.
    // These functions should accept any non-NULL pointer without crashing.
    void* dummy = (void*)(uintptr_t)0xDEAD;

    EXPECT_EQ(0, brain_cycle_coordinator_connect_bio_async(
        coord, (bio_module_context_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_immune(
        coord, (brain_immune_system_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_kg(
        coord, (kg_io_dispatcher_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_introspection(
        coord, (introspection_context_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_hemispheric(
        coord, (hemispheric_brain_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_fep(
        coord, (oscillations_fep_bridge_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_meta_learning(
        coord, (meta_learning_substrate_bridge_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_pink_noise(
        coord, (sfa_pink_noise_bridge_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_global_workspace(
        coord, (snn_global_workspace_bridge_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_attention(
        coord, (snn_attention_bridge_t*)dummy));
    EXPECT_EQ(0, brain_cycle_coordinator_connect_world_model(
        coord, (world_model_multimodal_t*)dummy));
}

TEST_F(CycleCoordinatorE2ETest, test_callback_cascade_system_degradation) {
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_overall_health_changed = e2e_on_overall;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register multiple cycles with degraded health_fn
    resetPerCycleHealth();
    g_per_cycle_health[0] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[1] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[2] = BRAIN_CYCLE_HEALTH_DEGRADED;

    registerAllCyclesWithPerCycleHealth();
    tickAllCycles(500);

    brain_cycle_coordinator_check_health(coord);

    // Should see health_changed callbacks for the degraded cycles
    EXPECT_GE(tracker.health_changed, 3);
    // Overall health should have changed
    EXPECT_GE(tracker.overall_changed, 1);

    // Verify stats
    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(3u, stats.total_cycles_degraded);
    EXPECT_LT(stats.overall_health, 1.0f);
}

TEST_F(CycleCoordinatorE2ETest, test_full_system_recovery_flow) {
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_overall_health_changed = e2e_on_overall;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Start all healthy
    resetPerCycleHealth();
    registerAllCyclesWithPerCycleHealth();
    tickAllCycles(500);
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    float initial_health = stats.overall_health;
    EXPECT_NEAR(1.0f, initial_health, 0.1f);

    int events_before_degrade = tracker.health_changed;

    // Degrade some cycles
    g_per_cycle_health[0] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[1] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[2] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[3] = BRAIN_CYCLE_HEALTH_DEGRADED;

    brain_cycle_coordinator_check_health(coord);
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(4u, stats.total_cycles_degraded);
    float degraded_health = stats.overall_health;
    EXPECT_LT(degraded_health, initial_health);

    int events_after_degrade = tracker.health_changed;
    EXPECT_GE(events_after_degrade - events_before_degrade, 4);

    // Recover
    g_per_cycle_health[0] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[1] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[2] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[3] = BRAIN_CYCLE_HEALTH_HEALTHY;

    tickAllCycles(500);
    brain_cycle_coordinator_check_health(coord);

    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));
    EXPECT_EQ(0u, stats.total_cycles_degraded);
    EXPECT_EQ((uint32_t)BRAIN_CYCLE_COUNT, stats.total_cycles_healthy);
    EXPECT_NEAR(initial_health, stats.overall_health, 0.15f);

    // Recovery events should have fired
    int recovery_events = tracker.health_changed - events_after_degrade;
    EXPECT_GE(recovery_events, 4);
}

TEST_F(CycleCoordinatorE2ETest, test_dependency_violation_full_flow) {
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_dependency_violated = e2e_on_dep_violated;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register immune_tick and oscillations
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, (void*)(uintptr_t)2, nullptr));

    // Set dependency: oscillations depends on immune_tick
    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_OSCILLATIONS, BRAIN_CYCLE_IMMUNE_TICK));

    // Both healthy initially
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_OSCILLATIONS, 500);
    brain_cycle_coordinator_check_health(coord);

    // Dependencies should be satisfied
    bool satisfied = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_OSCILLATIONS, &satisfied));
    EXPECT_TRUE(satisfied);

    // Now let immune_tick stall
    usleep(200000);
    brain_cycle_coordinator_check_health(coord);

    // Dependency should now be violated
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_OSCILLATIONS, &satisfied));
    EXPECT_FALSE(satisfied);
    EXPECT_GE(tracker.dep_violated, 1);

    // Recover immune_tick
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_OSCILLATIONS, &satisfied));
    EXPECT_TRUE(satisfied);
}

//=============================================================================
// GROUP 6: Callback Stress E2E (6 tests)
//=============================================================================

TEST_F(CycleCoordinatorE2ETest, test_many_callbacks_rapid_health_changes) {
    // Register max (16) callback sets
    E2ECallbackTracker trackers[BRAIN_CYCLE_MAX_CALLBACKS];
    for (int i = 0; i < BRAIN_CYCLE_MAX_CALLBACKS; i++) {
        trackers[i] = {};
        brain_cycle_coordinator_callbacks_t cbs = {};
        cbs.on_health_changed = e2e_on_health_changed;
        cbs.on_overall_health_changed = e2e_on_overall;
        cbs.user_data = &trackers[i];
        ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));
    }

    // Register immune tick with per-cycle health
    resetPerCycleHealth();
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_per_cycle_health));

    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    // Rapidly toggle between healthy and degraded
    for (int c = 0; c < 100; c++) {
        g_per_cycle_health[0] = (c % 2 == 0)
            ? BRAIN_CYCLE_HEALTH_DEGRADED
            : BRAIN_CYCLE_HEALTH_HEALTHY;
        brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
        brain_cycle_coordinator_check_health(coord);
    }

    // All 16 callback sets should have the same count
    for (int i = 1; i < BRAIN_CYCLE_MAX_CALLBACKS; i++) {
        EXPECT_EQ(trackers[0].health_changed, trackers[i].health_changed)
            << "Callback set " << i << " diverged from set 0";
    }

    // Each set should have received many health change events
    EXPECT_GE(trackers[0].health_changed, 50);
}

TEST_F(CycleCoordinatorE2ETest, test_callback_exceeds_max) {
    // Register exactly BRAIN_CYCLE_MAX_CALLBACKS (16) callback sets
    E2ECallbackTracker trackers[BRAIN_CYCLE_MAX_CALLBACKS + 1];
    for (int i = 0; i < BRAIN_CYCLE_MAX_CALLBACKS; i++) {
        trackers[i] = {};
        brain_cycle_coordinator_callbacks_t cbs = {};
        cbs.on_health_changed = e2e_on_health_changed;
        cbs.user_data = &trackers[i];
        ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));
    }

    // The 17th registration should fail
    trackers[BRAIN_CYCLE_MAX_CALLBACKS] = {};
    brain_cycle_coordinator_callbacks_t cbs_extra = {};
    cbs_extra.on_health_changed = e2e_on_health_changed;
    cbs_extra.user_data = &trackers[BRAIN_CYCLE_MAX_CALLBACKS];
    int result = brain_cycle_coordinator_register_callbacks(coord, &cbs_extra);
    EXPECT_EQ(-1, result);
}

TEST_F(CycleCoordinatorE2ETest, test_callback_during_health_check) {
    // Register a callback, then during health check the callback fires.
    // Inside the callback we do NOT re-lock (implementation fires under lock),
    // but after the health check returns we verify we can still query status.
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_health_changed = e2e_on_health_changed;
    cbs.on_overall_health_changed = e2e_on_overall;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register immune tick with error health_fn
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_return_error));

    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);

    // This check_health will fire callbacks under the lock
    brain_cycle_coordinator_check_health(coord);

    // Verify we can still safely query status after callbacks have fired
    brain_cycle_status_t status;
    ASSERT_EQ(0, brain_cycle_coordinator_get_status(
        coord, BRAIN_CYCLE_IMMUNE_TICK, &status));
    EXPECT_EQ(BRAIN_CYCLE_HEALTH_ERROR, status.health);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));

    // Callbacks should have fired
    EXPECT_GE(tracker.health_changed, 1);
    EXPECT_GE(tracker.overall_changed, 1);
}

TEST_F(CycleCoordinatorE2ETest, test_callback_unregister_all) {
    // Register 5 callback sets, each with distinct user_data
    E2ECallbackTracker trackers[5];
    for (int i = 0; i < 5; i++) {
        trackers[i] = {};
        brain_cycle_coordinator_callbacks_t cbs = {};
        cbs.on_health_changed = e2e_on_health_changed;
        cbs.user_data = &trackers[i];
        ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));
    }

    // Unregister all by user_data
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, brain_cycle_coordinator_unregister_callbacks(
            coord, &trackers[i]));
    }

    // Now register a cycle with error health_fn and trigger a health check
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_IMMUNE_TICK, (void*)(uintptr_t)1, mock_return_error));
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_IMMUNE_TICK, 500);
    brain_cycle_coordinator_check_health(coord);

    // No callbacks should have fired since all were unregistered
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, trackers[i].health_changed)
            << "Tracker " << i << " received unexpected callback after unregister";
    }
}

TEST_F(CycleCoordinatorE2ETest, test_mixed_health_states_overall) {
    // Set different health states across all 9 cycles
    resetPerCycleHealth();
    // 3 healthy, 3 degraded, 2 stalled, 1 error
    g_per_cycle_health[0] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[1] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[2] = BRAIN_CYCLE_HEALTH_HEALTHY;
    g_per_cycle_health[3] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[4] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[5] = BRAIN_CYCLE_HEALTH_DEGRADED;
    g_per_cycle_health[6] = BRAIN_CYCLE_HEALTH_STALLED;
    g_per_cycle_health[7] = BRAIN_CYCLE_HEALTH_STALLED;
    g_per_cycle_health[8] = BRAIN_CYCLE_HEALTH_ERROR;

    registerAllCyclesWithPerCycleHealth();
    tickAllCycles(500);
    brain_cycle_coordinator_check_health(coord);

    brain_cycle_coordinator_stats_t stats;
    ASSERT_EQ(0, brain_cycle_coordinator_get_stats(coord, &stats));

    // Overall health should be a weighted average:
    // HEALTHY=1.0, DEGRADED=0.5, STALLED=0.1, ERROR=0.0
    // (3*1.0 + 3*0.5 + 2*0.1 + 1*0.0) / 9 = (3.0 + 1.5 + 0.2) / 9 = 4.7/9 ~= 0.522
    EXPECT_GT(stats.overall_health, 0.0f);
    EXPECT_LT(stats.overall_health, 1.0f);
    // Allow generous tolerance since weight formula may differ
    EXPECT_NEAR(0.52f, stats.overall_health, 0.2f);

    EXPECT_EQ(3u, stats.total_cycles_healthy);
    EXPECT_EQ(3u, stats.total_cycles_degraded);
    EXPECT_EQ(2u, stats.total_cycles_stalled);
}

TEST_F(CycleCoordinatorE2ETest, test_dependency_with_disabled_cycle) {
    E2ECallbackTracker tracker = {};
    brain_cycle_coordinator_callbacks_t cbs = {};
    cbs.on_dependency_violated = e2e_on_dep_violated;
    cbs.user_data = &tracker;
    ASSERT_EQ(0, brain_cycle_coordinator_register_callbacks(coord, &cbs));

    // Register oscillations and GC agent
    // GC agent will use a health_fn that returns DISABLED
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_OSCILLATIONS, (void*)(uintptr_t)1, nullptr));
    ASSERT_EQ(0, brain_cycle_coordinator_register(
        coord, BRAIN_CYCLE_GC_AGENT, (void*)(uintptr_t)2, mock_return_disabled));

    // Set dependency: oscillations depends on gc_agent
    ASSERT_EQ(0, brain_cycle_coordinator_add_dependency(
        coord, BRAIN_CYCLE_OSCILLATIONS, BRAIN_CYCLE_GC_AGENT));

    // Tick both
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_OSCILLATIONS, 500);
    brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_GC_AGENT, 500);

    brain_cycle_coordinator_check_health(coord);

    // DISABLED should not count as a dependency violation
    EXPECT_EQ(0, tracker.dep_violated);

    // Dependencies should be considered satisfied (DISABLED is not a violation)
    bool satisfied = false;
    ASSERT_EQ(0, brain_cycle_coordinator_check_dependencies(
        coord, BRAIN_CYCLE_OSCILLATIONS, &satisfied));
    EXPECT_TRUE(satisfied);
}
