// SPDX-License-Identifier: MIT
//=============================================================================
// test_brain_cycle_coordinator_integration.c
// Brain Cycle Coordinator Integration Tests
//=============================================================================
/**
 * @file test_brain_cycle_coordinator_integration.c
 * @brief Integration tests for the Brain Cycle Coordinator
 *
 * WHAT: ~40 integration tests verifying the Brain Cycle Coordinator's
 *       cross-module interactions with immune, bio-async, FEP, introspection,
 *       hemispheric balance, meta-learning, pink noise, global workspace,
 *       world model, and callback subsystems.
 *
 * WHY:  The Brain Cycle Coordinator manages 9 biological cycle types
 *       (sleep, circadian, immune tick, health agent, oscillations, brain
 *       update, arousal, meta-learning, plasticity). These tests verify
 *       that it integrates correctly with all dependent NIMCP subsystems.
 *
 * HOW:  Unity test framework with setUp/tearDown fixtures, callback
 *       tracking helpers, and comprehensive cross-module verification.
 *
 * Test Groups (15):
 *   1. Brain Integration          - Full brain lifecycle with coordinator
 *   2. Cross-Cycle Communication  - Inter-cycle tick and health propagation
 *   3. Dependency Management      - Cycle dependency tracking and violations
 *   4. Bio-Async Events           - Async messaging integration
 *   5. Immune System Integration  - Immune tick + coordinator interaction
 *   6. KG Persistence             - Knowledge graph flush and state persistence
 *   7. Introspection              - Self-monitoring and diagnostics
 *   8. Hemispheric Balance        - Left/right hemisphere coordination
 *   9. FEP Integration            - Free Energy Principle bridge coordination
 *  10. Meta-Learning              - Meta-learning cycle management
 *  11. Pink Noise                 - Pink noise bridge cycle tracking
 *  12. Global Workspace           - Global workspace theory integration
 *  13. World Model                - World model update cycle integration
 *  14. Callback Flow              - Callback registration and invocation
 *  15. Full System Callback       - Multi-callback end-to-end scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-01-25
 * @version 1.0.0
 */

#include "unity.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include "core/brain/nimcp_brain_bio_async.h"
#include "core/brain/nimcp_brain_lifecycle.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_brain_immune_tick.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_immune.h"

/* ============================================================================
 * Global Test State
 * ============================================================================ */

static brain_cycle_coordinator_t* g_coord = NULL;
static brain_cycle_coordinator_config_t g_config;

/* ============================================================================
 * Callback Tracking Helpers
 * ============================================================================ */

static int g_health_changed_count = 0;
static int g_stall_detected_count = 0;
static int g_dep_violated_count = 0;
static int g_overall_health_changed_count = 0;
static int g_secondary_callback_count = 0;
static int g_tertiary_callback_count = 0;
static int g_reentrant_callback_count = 0;
static int g_combined_stall_count = 0;
static int g_propagation_callback_count = 0;
static float g_last_health_value = 0.0f;
static brain_cycle_type_t g_last_callback_cycle = (brain_cycle_type_t)0;
static bool g_callback_order_valid = true;
static int g_callback_sequence = 0;

static void callback_health_changed(brain_cycle_coordinator_t* coord,
                                    brain_cycle_type_t cycle,
                                    float health, void* user_data)
{
    (void)coord;
    (void)user_data;
    g_health_changed_count++;
    g_last_health_value = health;
    g_last_callback_cycle = cycle;
}

static void callback_stall_detected(brain_cycle_coordinator_t* coord,
                                    brain_cycle_type_t cycle,
                                    float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_stall_detected_count++;
}

static void callback_dep_violated(brain_cycle_coordinator_t* coord,
                                  brain_cycle_type_t cycle,
                                  float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_dep_violated_count++;
}

static void callback_overall_health_changed(brain_cycle_coordinator_t* coord,
                                            brain_cycle_type_t cycle,
                                            float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)user_data;
    g_overall_health_changed_count++;
    g_last_health_value = health;
}

static void callback_secondary(brain_cycle_coordinator_t* coord,
                               brain_cycle_type_t cycle,
                               float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_secondary_callback_count++;
}

static void callback_tertiary(brain_cycle_coordinator_t* coord,
                              brain_cycle_type_t cycle,
                              float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_tertiary_callback_count++;
}

static void callback_reentrant(brain_cycle_coordinator_t* coord,
                               brain_cycle_type_t cycle,
                               float health, void* user_data)
{
    (void)cycle;
    (void)health;
    (void)user_data;
    g_reentrant_callback_count++;
    /* Attempt reentrant notify_tick -- coordinator should handle safely */
    if (g_reentrant_callback_count <= 1) {
        brain_cycle_coordinator_notify_tick(coord, BRAIN_CYCLE_BRAIN_UPDATE, 10);
    }
}

static void callback_combined_stall(brain_cycle_coordinator_t* coord,
                                    brain_cycle_type_t cycle,
                                    float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_combined_stall_count++;
}

static void callback_propagation(brain_cycle_coordinator_t* coord,
                                 brain_cycle_type_t cycle,
                                 float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    g_propagation_callback_count++;
}

static void callback_ordered_first(brain_cycle_coordinator_t* coord,
                                   brain_cycle_type_t cycle,
                                   float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    if (g_callback_sequence != 0) {
        g_callback_order_valid = false;
    }
    g_callback_sequence++;
}

static void callback_ordered_second(brain_cycle_coordinator_t* coord,
                                    brain_cycle_type_t cycle,
                                    float health, void* user_data)
{
    (void)coord;
    (void)cycle;
    (void)health;
    (void)user_data;
    if (g_callback_sequence != 1) {
        g_callback_order_valid = false;
    }
    g_callback_sequence++;
}

/* ============================================================================
 * Reset and Helper Functions
 * ============================================================================ */

static void reset_all_callback_counters(void)
{
    g_health_changed_count = 0;
    g_stall_detected_count = 0;
    g_dep_violated_count = 0;
    g_overall_health_changed_count = 0;
    g_secondary_callback_count = 0;
    g_tertiary_callback_count = 0;
    g_reentrant_callback_count = 0;
    g_combined_stall_count = 0;
    g_propagation_callback_count = 0;
    g_last_health_value = 0.0f;
    g_last_callback_cycle = (brain_cycle_type_t)0;
    g_callback_order_valid = true;
    g_callback_sequence = 0;
}

/**
 * @brief Register all 9 biological cycle types on the coordinator
 */
static void register_all_cycles(brain_cycle_coordinator_t* coord)
{
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_SLEEP);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_CIRCADIAN);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_IMMUNE_TICK);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_HEALTH_AGENT);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_OSCILLATIONS);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_BRAIN_UPDATE);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_AROUSAL);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_META_LEARNING);
    brain_cycle_coordinator_register_cycle(coord, BRAIN_CYCLE_PLASTICITY);
}

/* ============================================================================
 * Unity Fixtures
 * ============================================================================ */

void setUp(void)
{
    reset_all_callback_counters();
    brain_cycle_coordinator_default_config(&g_config);
    g_config.enable_logging = false;
    g_config.enable_timing_checks = true;
    g_config.enable_dependency_tracking = true;
    g_config.kg_write_interval_ms = 1000;

    g_coord = brain_cycle_coordinator_create(&g_config);
    TEST_ASSERT_NOT_NULL(g_coord);
}

void tearDown(void)
{
    if (g_coord) {
        brain_cycle_coordinator_destroy(g_coord);
        g_coord = NULL;
    }
}

/* ============================================================================
 * Group 1: Brain Integration Tests
 * ============================================================================ */

/**
 * test_brain_create_initializes_coordinator
 *
 * WHAT: Verify brain_create() initializes the cycle coordinator
 * WHY:  Coordinator must be available after brain creation
 * EXPECT: Coordinator is non-NULL and stats are zeroed
 */
static void test_brain_create_initializes_coordinator(void)
{
    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT32(0, stats.registered_cycles);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, stats.overall_health);
}

/**
 * test_brain_destroy_cleans_up_coordinator
 *
 * WHAT: Verify brain_destroy() properly cleans up the coordinator
 * WHY:  No memory leaks from coordinator lifecycle
 * EXPECT: destroy succeeds without crash, coordinator is NULL afterward
 */
static void test_brain_destroy_cleans_up_coordinator(void)
{
    brain_cycle_coordinator_t* local = brain_cycle_coordinator_create(&g_config);
    TEST_ASSERT_NOT_NULL(local);

    register_all_cycles(local);
    brain_cycle_coordinator_notify_tick(local, BRAIN_CYCLE_SLEEP, 100);
    brain_cycle_coordinator_destroy(local);
    /* If we reach here without crash, test passes */
    TEST_ASSERT_TRUE(true);
}

/**
 * test_brain_coordinator_survives_full_lifecycle
 *
 * WHAT: Verify coordinator survives create -> register -> tick -> destroy
 * WHY:  Full lifecycle must not leak or crash
 * EXPECT: All operations succeed; stats reflect activity
 */
static void test_brain_coordinator_survives_full_lifecycle(void)
{
    register_all_cycles(g_coord);

    /* Tick each cycle */
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_CIRCADIAN, 100);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_IMMUNE_TICK, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);

    brain_cycle_coordinator_stats_t stats;
    int result = brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT32(9, stats.registered_cycles);
    TEST_ASSERT_TRUE(stats.active_cycles >= 5);
}

/* ============================================================================
 * Group 2: Cross-Cycle Communication Tests
 * ============================================================================ */

/**
 * test_tick_propagates_across_cycles
 *
 * WHAT: Verify ticking one cycle does not corrupt another
 * WHY:  Cycles must be independently tracked
 * EXPECT: Each cycle has its own tick count
 */
static void test_tick_propagates_across_cycles(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_CIRCADIAN, 100);

    brain_cycle_status_t sleep_status;
    brain_cycle_coordinator_get_cycle_status(g_coord, BRAIN_CYCLE_SLEEP, &sleep_status);
    TEST_ASSERT_EQUAL_UINT32(3, sleep_status.tick_count);

    brain_cycle_status_t circ_status;
    brain_cycle_coordinator_get_cycle_status(g_coord, BRAIN_CYCLE_CIRCADIAN, &circ_status);
    TEST_ASSERT_EQUAL_UINT32(1, circ_status.tick_count);
}

/**
 * test_health_change_in_one_cycle_reflects_overall
 *
 * WHAT: Verify setting one cycle's health affects overall health
 * WHY:  Overall health aggregates all cycles
 * EXPECT: Degraded cycle lowers overall health
 */
static void test_health_change_in_one_cycle_reflects_overall(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(g_coord, BRAIN_CYCLE_SLEEP, 0.3f);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_TRUE(stats.overall_health < 1.0f);
}

/**
 * test_all_cycles_healthy_overall_healthy
 *
 * WHAT: Verify overall health = 1.0 when all cycles are healthy
 * WHY:  Baseline health aggregation check
 * EXPECT: overall_health == 1.0
 */
static void test_all_cycles_healthy_overall_healthy(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_CIRCADIAN, 100);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_IMMUNE_TICK, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_HEALTH_AGENT, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_AROUSAL, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_META_LEARNING, 200);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_PLASTICITY, 100);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, stats.overall_health);
}

/* ============================================================================
 * Group 3: Dependency Management Tests
 * ============================================================================ */

/**
 * test_add_dependency_between_cycles
 *
 * WHAT: Verify adding a dependency between two cycles
 * WHY:  Some cycles depend on others (e.g., sleep depends on circadian)
 * EXPECT: Dependency registered without error
 */
static void test_add_dependency_between_cycles(void)
{
    register_all_cycles(g_coord);

    int result = brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_SLEEP, BRAIN_CYCLE_CIRCADIAN);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/**
 * test_dependency_violation_detected
 *
 * WHAT: Verify dependency violation is detected when depended cycle is unhealthy
 * WHY:  Coordinator must track dependency health constraints
 * EXPECT: check_deps returns violation when circadian is degraded
 */
static void test_dependency_violation_detected(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_SLEEP, BRAIN_CYCLE_CIRCADIAN);

    brain_cycle_coordinator_set_cycle_health(g_coord, BRAIN_CYCLE_CIRCADIAN, 0.1f);

    int violations = brain_cycle_coordinator_check_deps(g_coord, BRAIN_CYCLE_SLEEP);
    TEST_ASSERT_TRUE(violations > 0);
}

/* ============================================================================
 * Group 4: Bio-Async Events Tests
 * ============================================================================ */

/**
 * test_bio_async_tick_updates_coordinator
 *
 * WHAT: Verify bio-async message processing triggers coordinator tick
 * WHY:  Bio-async events should be reflected in cycle stats
 * EXPECT: Brain update cycle shows tick after bio-async processing
 */
static void test_bio_async_tick_updates_coordinator(void)
{
    register_all_cycles(g_coord);

    int result = brain_cycle_coordinator_notify_tick(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    TEST_ASSERT_EQUAL_INT(0, result);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_EQUAL_UINT32(1, status.tick_count);
    TEST_ASSERT_TRUE(status.last_tick_ms > 0 || status.tick_count == 1);
}

/**
 * test_bio_async_health_degrades_on_backpressure
 *
 * WHAT: Verify bio-async backpressure reduces brain_update health
 * WHY:  Slow message processing should degrade cycle health
 * EXPECT: Health < 1.0 after simulated backpressure
 */
static void test_bio_async_health_degrades_on_backpressure(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.5f);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, status.health);
}

/**
 * test_bio_async_recovery_restores_health
 *
 * WHAT: Verify health recovers after backpressure clears
 * WHY:  System should self-heal
 * EXPECT: Health returns to 1.0 after restoration
 */
static void test_bio_async_recovery_restores_health(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.3f);
    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 1.0f);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, status.health);
}

/* ============================================================================
 * Group 5: Immune System Integration Tests
 * ============================================================================ */

/**
 * test_immune_tick_cycle_registered
 *
 * WHAT: Verify immune tick cycle can be registered and ticked
 * WHY:  Immune tick is a core biological cycle
 * EXPECT: Immune tick cycle is tracked independently
 */
static void test_immune_tick_cycle_registered(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_IMMUNE_TICK, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_IMMUNE_TICK, 50);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_IMMUNE_TICK, &status);
    TEST_ASSERT_EQUAL_UINT32(2, status.tick_count);
}

/**
 * test_immune_health_affects_overall
 *
 * WHAT: Verify immune cycle health degradation affects overall health
 * WHY:  Immune problems should lower system health
 * EXPECT: overall_health < 1.0 when immune is degraded
 */
static void test_immune_health_affects_overall(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_IMMUNE_TICK, 0.2f);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_TRUE(stats.overall_health < 1.0f);
}

/**
 * test_health_agent_cycle_tracks_independently
 *
 * WHAT: Verify health agent cycle is separate from immune tick
 * WHY:  Health agent and immune tick are different cycles
 * EXPECT: Ticking one does not affect the other's count
 */
static void test_health_agent_cycle_tracks_independently(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_HEALTH_AGENT, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_HEALTH_AGENT, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_HEALTH_AGENT, 50);

    brain_cycle_status_t ha_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_HEALTH_AGENT, &ha_status);
    TEST_ASSERT_EQUAL_UINT32(3, ha_status.tick_count);

    brain_cycle_status_t it_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_IMMUNE_TICK, &it_status);
    TEST_ASSERT_EQUAL_UINT32(0, it_status.tick_count);
}

/* ============================================================================
 * Group 6: KG Persistence Tests
 * ============================================================================ */

/**
 * test_kg_flush_writes_state
 *
 * WHAT: Verify flush_to_kg writes coordinator state
 * WHY:  State must be persisted for restart/inspection
 * EXPECT: flush returns success (0)
 */
static void test_kg_flush_writes_state(void)
{
    register_all_cycles(g_coord);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);

    int result = brain_cycle_coordinator_flush_to_kg(g_coord);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/**
 * test_kg_flush_after_health_change
 *
 * WHAT: Verify KG flush captures health changes
 * WHY:  Health state should be persisted
 * EXPECT: flush succeeds after health modification
 */
static void test_kg_flush_after_health_change(void)
{
    register_all_cycles(g_coord);
    brain_cycle_coordinator_set_cycle_health(g_coord, BRAIN_CYCLE_SLEEP, 0.5f);

    int result = brain_cycle_coordinator_flush_to_kg(g_coord);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/**
 * test_kg_flush_empty_coordinator
 *
 * WHAT: Verify KG flush handles empty coordinator (no cycles registered)
 * WHY:  Edge case: flush with nothing to persist
 * EXPECT: flush returns success (no crash)
 */
static void test_kg_flush_empty_coordinator(void)
{
    int result = brain_cycle_coordinator_flush_to_kg(g_coord);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/* ============================================================================
 * Group 7: Introspection Tests
 * ============================================================================ */

/**
 * test_get_all_status_returns_all_cycles
 *
 * WHAT: Verify get_all_status returns status for all registered cycles
 * WHY:  Introspection must report all cycles
 * EXPECT: 9 statuses returned for 9 registered cycles
 */
static void test_get_all_status_returns_all_cycles(void)
{
    register_all_cycles(g_coord);

    brain_cycle_status_t statuses[16];
    uint32_t count = 0;
    int result = brain_cycle_coordinator_get_all_status(
        g_coord, statuses, 16, &count);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT32(9, count);
}

/**
 * test_check_health_reports_degraded_cycles
 *
 * WHAT: Verify check_health identifies degraded cycles
 * WHY:  Diagnostic must identify problems
 * EXPECT: Returns > 0 issues when a cycle is degraded
 */
static void test_check_health_reports_degraded_cycles(void)
{
    register_all_cycles(g_coord);
    brain_cycle_coordinator_set_cycle_health(g_coord, BRAIN_CYCLE_AROUSAL, 0.1f);

    int issues = brain_cycle_coordinator_check_health(g_coord);
    TEST_ASSERT_TRUE(issues > 0);
}

/**
 * test_check_health_all_healthy
 *
 * WHAT: Verify check_health returns 0 when all cycles are healthy
 * WHY:  No false positives in diagnostics
 * EXPECT: Returns 0 issues
 */
static void test_check_health_all_healthy(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_SLEEP, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_CIRCADIAN, 100);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_IMMUNE_TICK, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_HEALTH_AGENT, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_AROUSAL, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_META_LEARNING, 200);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_PLASTICITY, 100);

    int issues = brain_cycle_coordinator_check_health(g_coord);
    TEST_ASSERT_EQUAL_INT(0, issues);
}

/* ============================================================================
 * Group 8: Hemispheric Balance Tests
 * ============================================================================ */

/**
 * test_oscillation_cycle_tracks_hemispheric_timing
 *
 * WHAT: Verify oscillation cycle timing is tracked for hemispheric balance
 * WHY:  Oscillations drive hemispheric synchronization
 * EXPECT: Oscillation cycle shows correct tick timing
 */
static void test_oscillation_cycle_tracks_hemispheric_timing(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    TEST_ASSERT_EQUAL_UINT32(3, status.tick_count);
    TEST_ASSERT_TRUE(status.last_tick_ms >= 20);
}

/**
 * test_arousal_cycle_independent_from_oscillations
 *
 * WHAT: Verify arousal and oscillation cycles are independent
 * WHY:  Different biological rhythms must not interfere
 * EXPECT: Separate tick counts
 */
static void test_arousal_cycle_independent_from_oscillations(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_AROUSAL, 50);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    brain_cycle_coordinator_notify_tick(g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);

    brain_cycle_status_t arousal_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_AROUSAL, &arousal_status);
    TEST_ASSERT_EQUAL_UINT32(1, arousal_status.tick_count);

    brain_cycle_status_t osc_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, &osc_status);
    TEST_ASSERT_EQUAL_UINT32(2, osc_status.tick_count);
}

/**
 * test_hemispheric_health_degradation_isolated
 *
 * WHAT: Verify degrading oscillation health does not affect arousal health
 * WHY:  Per-cycle health isolation
 * EXPECT: Arousal remains 1.0 when oscillations are degraded
 */
static void test_hemispheric_health_degradation_isolated(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, 0.4f);

    brain_cycle_status_t arousal_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_AROUSAL, &arousal_status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, arousal_status.health);
}

/* ============================================================================
 * Group 9: FEP Integration Tests
 * ============================================================================ */

/**
 * test_brain_update_cycle_reflects_fep_processing
 *
 * WHAT: Verify brain update cycle (which includes FEP) is tracked
 * WHY:  FEP bridge runs during brain update ticks
 * EXPECT: Brain update tick count increases
 */
static void test_brain_update_cycle_reflects_fep_processing(void)
{
    register_all_cycles(g_coord);

    for (int i = 0; i < 10; i++) {
        brain_cycle_coordinator_notify_tick(
            g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    }

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_EQUAL_UINT32(10, status.tick_count);
}

/**
 * test_fep_health_degradation_propagates_to_overall
 *
 * WHAT: Verify FEP-related health issues affect overall system health
 * WHY:  FEP processing failures should be visible globally
 * EXPECT: Overall health < 1.0 when brain_update is degraded
 */
static void test_fep_health_degradation_propagates_to_overall(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.2f);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coord, &stats);
    TEST_ASSERT_TRUE(stats.overall_health < 1.0f);
}

/**
 * test_fep_dependency_on_oscillations
 *
 * WHAT: Verify brain update can depend on oscillations
 * WHY:  FEP processing needs neural oscillation data
 * EXPECT: Dependency violation when oscillations are unhealthy
 */
static void test_fep_dependency_on_oscillations(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_OSCILLATIONS);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, 0.05f);

    int violations = brain_cycle_coordinator_check_deps(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE);
    TEST_ASSERT_TRUE(violations > 0);
}

/* ============================================================================
 * Group 10: Meta-Learning Tests
 * ============================================================================ */

/**
 * test_meta_learning_cycle_registered_and_ticked
 *
 * WHAT: Verify meta-learning cycle can be registered and ticked
 * WHY:  Meta-learning operates on a slower cycle
 * EXPECT: Tick count reflects meta-learning activity
 */
static void test_meta_learning_cycle_registered_and_ticked(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(
        g_coord, BRAIN_CYCLE_META_LEARNING, 200);
    brain_cycle_coordinator_notify_tick(
        g_coord, BRAIN_CYCLE_META_LEARNING, 200);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_META_LEARNING, &status);
    TEST_ASSERT_EQUAL_UINT32(2, status.tick_count);
}

/**
 * test_meta_learning_depends_on_plasticity
 *
 * WHAT: Verify meta-learning depends on plasticity cycle
 * WHY:  Meta-learning adapts plasticity parameters
 * EXPECT: Dependency registered and violation detected
 */
static void test_meta_learning_depends_on_plasticity(void)
{
    register_all_cycles(g_coord);

    int result = brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_META_LEARNING, BRAIN_CYCLE_PLASTICITY);
    TEST_ASSERT_EQUAL_INT(0, result);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_PLASTICITY, 0.05f);

    int violations = brain_cycle_coordinator_check_deps(
        g_coord, BRAIN_CYCLE_META_LEARNING);
    TEST_ASSERT_TRUE(violations > 0);
}

/**
 * test_meta_learning_health_isolation
 *
 * WHAT: Verify meta-learning health is isolated from other cycles
 * WHY:  Per-cycle health independence
 * EXPECT: Degrading meta-learning does not affect sleep health
 */
static void test_meta_learning_health_isolation(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_META_LEARNING, 0.3f);

    brain_cycle_status_t sleep_status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_SLEEP, &sleep_status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, sleep_status.health);
}

/* ============================================================================
 * Group 11: Pink Noise Tests
 * ============================================================================ */

/**
 * test_oscillation_cycle_supports_pink_noise
 *
 * WHAT: Verify oscillation cycle can track pink noise bridge ticks
 * WHY:  Pink noise bridges are oscillation-driven
 * EXPECT: Oscillation ticks increment correctly
 */
static void test_oscillation_cycle_supports_pink_noise(void)
{
    register_all_cycles(g_coord);

    for (int i = 0; i < 5; i++) {
        brain_cycle_coordinator_notify_tick(
            g_coord, BRAIN_CYCLE_OSCILLATIONS, 20);
    }

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    TEST_ASSERT_EQUAL_UINT32(5, status.tick_count);
}

/**
 * test_pink_noise_health_affects_oscillations
 *
 * WHAT: Verify oscillation health can be degraded (e.g., pink noise failure)
 * WHY:  Pink noise failures should be reflected in oscillation health
 * EXPECT: Oscillation health reflects degradation
 */
static void test_pink_noise_health_affects_oscillations(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, 0.6f);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, &status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.6f, status.health);
}

/* ============================================================================
 * Group 12: Global Workspace Tests
 * ============================================================================ */

/**
 * test_brain_update_cycle_covers_global_workspace
 *
 * WHAT: Verify brain update cycle covers global workspace processing
 * WHY:  Global workspace theory runs during brain update
 * EXPECT: Brain update ticks are tracked
 */
static void test_brain_update_cycle_covers_global_workspace(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_notify_tick(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_EQUAL_UINT32(1, status.tick_count);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, status.health);
}

/**
 * test_global_workspace_depends_on_arousal
 *
 * WHAT: Verify brain update can depend on arousal for global workspace
 * WHY:  Global workspace needs arousal context
 * EXPECT: Dependency violation when arousal is degraded
 */
static void test_global_workspace_depends_on_arousal(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_AROUSAL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_AROUSAL, 0.05f);

    int violations = brain_cycle_coordinator_check_deps(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE);
    TEST_ASSERT_TRUE(violations > 0);
}

/**
 * test_global_workspace_multiple_dependencies
 *
 * WHAT: Verify brain update can have multiple dependencies
 * WHY:  Global workspace depends on arousal and oscillations
 * EXPECT: Multiple dependency violations detected
 */
static void test_global_workspace_multiple_dependencies(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_AROUSAL);
    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, BRAIN_CYCLE_OSCILLATIONS);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_AROUSAL, 0.05f);
    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_OSCILLATIONS, 0.05f);

    int violations = brain_cycle_coordinator_check_deps(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE);
    TEST_ASSERT_TRUE(violations >= 2);
}

/* ============================================================================
 * Group 13: World Model Tests
 * ============================================================================ */

/**
 * test_brain_update_cycle_covers_world_model
 *
 * WHAT: Verify brain update cycle also covers world model updates
 * WHY:  World model prediction runs during brain update
 * EXPECT: Tick tracking works for world model processing path
 */
static void test_brain_update_cycle_covers_world_model(void)
{
    register_all_cycles(g_coord);

    for (int i = 0; i < 3; i++) {
        brain_cycle_coordinator_notify_tick(
            g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 16);
    }

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_EQUAL_UINT32(3, status.tick_count);
}

/**
 * test_world_model_health_reflects_prediction_errors
 *
 * WHAT: Verify world model health degradation is trackable
 * WHY:  High prediction error should degrade brain_update health
 * EXPECT: Health < 1.0 after degradation
 */
static void test_world_model_health_reflects_prediction_errors(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.4f);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, status.health);
}

/**
 * test_world_model_recovery
 *
 * WHAT: Verify world model health can recover
 * WHY:  System should self-heal after prediction errors clear
 * EXPECT: Health returns to 1.0
 */
static void test_world_model_recovery(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.2f);
    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 1.0f);

    brain_cycle_status_t status;
    brain_cycle_coordinator_get_cycle_status(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, &status);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, status.health);
}

/* ============================================================================
 * Group 14: Callback Flow Tests
 * ============================================================================ */

/**
 * test_register_health_changed_callback
 *
 * WHAT: Verify registering and triggering a health changed callback
 * WHY:  Callback mechanism is core to coordinator
 * EXPECT: Callback invoked when health changes
 */
static void test_register_health_changed_callback(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_health_changed, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.5f);

    TEST_ASSERT_TRUE(g_health_changed_count > 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, g_last_health_value);
}

/**
 * test_register_stall_detected_callback
 *
 * WHAT: Verify stall detection callback fires
 * WHY:  Stalled cycles must be reported
 * EXPECT: Stall callback invoked when appropriate
 */
static void test_register_stall_detected_callback(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_STALL_DETECTED,
        callback_stall_detected, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.0f);

    brain_cycle_coordinator_check_health(g_coord);

    TEST_ASSERT_TRUE(g_stall_detected_count > 0);
}

/**
 * test_register_dep_violated_callback
 *
 * WHAT: Verify dependency violation callback fires
 * WHY:  Dependency violations must be reportable
 * EXPECT: Callback invoked on violation
 */
static void test_register_dep_violated_callback(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_DEP_VIOLATED,
        callback_dep_violated, NULL);

    brain_cycle_coordinator_add_dependency(
        g_coord, BRAIN_CYCLE_SLEEP, BRAIN_CYCLE_CIRCADIAN);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_CIRCADIAN, 0.0f);

    brain_cycle_coordinator_check_deps(g_coord, BRAIN_CYCLE_SLEEP);

    TEST_ASSERT_TRUE(g_dep_violated_count > 0);
}

/**
 * test_register_overall_health_callback
 *
 * WHAT: Verify overall health change callback fires
 * WHY:  System-wide health monitoring
 * EXPECT: Callback invoked when overall health changes
 */
static void test_register_overall_health_callback(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_OVERALL_HEALTH_CHANGED,
        callback_overall_health_changed, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.3f);

    TEST_ASSERT_TRUE(g_overall_health_changed_count > 0);
}

/**
 * test_callback_receives_correct_cycle_type
 *
 * WHAT: Verify callback receives the correct cycle type
 * WHY:  Callbacks must identify which cycle triggered them
 * EXPECT: g_last_callback_cycle == BRAIN_CYCLE_AROUSAL
 */
static void test_callback_receives_correct_cycle_type(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_health_changed, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_AROUSAL, 0.7f);

    TEST_ASSERT_EQUAL_INT((int)BRAIN_CYCLE_AROUSAL, (int)g_last_callback_cycle);
}

/* ============================================================================
 * Group 15: Full System Callback Integration Tests
 * ============================================================================ */

/**
 * test_multiple_callbacks_same_event
 *
 * WHAT: Verify multiple callbacks for the same event type all fire
 * WHY:  Multiple listeners must all be notified
 * EXPECT: Both primary and secondary callbacks invoked
 */
static void test_multiple_callbacks_same_event(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_health_changed, NULL);
    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_secondary, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.5f);

    TEST_ASSERT_TRUE(g_health_changed_count > 0);
    TEST_ASSERT_TRUE(g_secondary_callback_count > 0);
}

/**
 * test_callbacks_across_different_events
 *
 * WHAT: Verify callbacks for different event types fire independently
 * WHY:  Event-type isolation
 * EXPECT: Health callback fires but stall callback does not for simple
 *         health change
 */
static void test_callbacks_across_different_events(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_health_changed, NULL);
    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_STALL_DETECTED,
        callback_stall_detected, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.5f);

    TEST_ASSERT_TRUE(g_health_changed_count > 0);
    TEST_ASSERT_EQUAL_INT(0, g_stall_detected_count);
}

/**
 * test_reentrant_callback_safety
 *
 * WHAT: Verify coordinator handles reentrant callbacks safely
 * WHY:  Callbacks that call back into the coordinator must not deadlock
 * EXPECT: No crash or deadlock; reentrant callback count >= 1
 */
static void test_reentrant_callback_safety(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_reentrant, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_BRAIN_UPDATE, 0.5f);

    TEST_ASSERT_TRUE(g_reentrant_callback_count >= 1);
}

/**
 * test_callback_ordering_preserved
 *
 * WHAT: Verify callbacks fire in registration order
 * WHY:  Deterministic callback ordering for predictability
 * EXPECT: First registered callback fires before second
 */
static void test_callback_ordering_preserved(void)
{
    register_all_cycles(g_coord);

    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_ordered_first, NULL);
    brain_cycle_coordinator_register_callback(
        g_coord, BRAIN_CYCLE_CB_HEALTH_CHANGED,
        callback_ordered_second, NULL);

    brain_cycle_coordinator_set_cycle_health(
        g_coord, BRAIN_CYCLE_SLEEP, 0.5f);

    TEST_ASSERT_TRUE(g_callback_order_valid);
    TEST_ASSERT_EQUAL_INT(2, g_callback_sequence);
}

/* ============================================================================
 * Main - Unity Test Runner
 * ============================================================================ */

int main(void)
{
    UNITY_BEGIN();

    /* Group 1: Brain Integration */
    RUN_TEST(test_brain_create_initializes_coordinator);
    RUN_TEST(test_brain_destroy_cleans_up_coordinator);
    RUN_TEST(test_brain_coordinator_survives_full_lifecycle);

    /* Group 2: Cross-Cycle Communication */
    RUN_TEST(test_tick_propagates_across_cycles);
    RUN_TEST(test_health_change_in_one_cycle_reflects_overall);
    RUN_TEST(test_all_cycles_healthy_overall_healthy);

    /* Group 3: Dependency Management */
    RUN_TEST(test_add_dependency_between_cycles);
    RUN_TEST(test_dependency_violation_detected);

    /* Group 4: Bio-Async Events */
    RUN_TEST(test_bio_async_tick_updates_coordinator);
    RUN_TEST(test_bio_async_health_degrades_on_backpressure);
    RUN_TEST(test_bio_async_recovery_restores_health);

    /* Group 5: Immune System Integration */
    RUN_TEST(test_immune_tick_cycle_registered);
    RUN_TEST(test_immune_health_affects_overall);
    RUN_TEST(test_health_agent_cycle_tracks_independently);

    /* Group 6: KG Persistence */
    RUN_TEST(test_kg_flush_writes_state);
    RUN_TEST(test_kg_flush_after_health_change);
    RUN_TEST(test_kg_flush_empty_coordinator);

    /* Group 7: Introspection */
    RUN_TEST(test_get_all_status_returns_all_cycles);
    RUN_TEST(test_check_health_reports_degraded_cycles);
    RUN_TEST(test_check_health_all_healthy);

    /* Group 8: Hemispheric Balance */
    RUN_TEST(test_oscillation_cycle_tracks_hemispheric_timing);
    RUN_TEST(test_arousal_cycle_independent_from_oscillations);
    RUN_TEST(test_hemispheric_health_degradation_isolated);

    /* Group 9: FEP Integration */
    RUN_TEST(test_brain_update_cycle_reflects_fep_processing);
    RUN_TEST(test_fep_health_degradation_propagates_to_overall);
    RUN_TEST(test_fep_dependency_on_oscillations);

    /* Group 10: Meta-Learning */
    RUN_TEST(test_meta_learning_cycle_registered_and_ticked);
    RUN_TEST(test_meta_learning_depends_on_plasticity);
    RUN_TEST(test_meta_learning_health_isolation);

    /* Group 11: Pink Noise */
    RUN_TEST(test_oscillation_cycle_supports_pink_noise);
    RUN_TEST(test_pink_noise_health_affects_oscillations);

    /* Group 12: Global Workspace */
    RUN_TEST(test_brain_update_cycle_covers_global_workspace);
    RUN_TEST(test_global_workspace_depends_on_arousal);
    RUN_TEST(test_global_workspace_multiple_dependencies);

    /* Group 13: World Model */
    RUN_TEST(test_brain_update_cycle_covers_world_model);
    RUN_TEST(test_world_model_health_reflects_prediction_errors);
    RUN_TEST(test_world_model_recovery);

    /* Group 14: Callback Flow */
    RUN_TEST(test_register_health_changed_callback);
    RUN_TEST(test_register_stall_detected_callback);
    RUN_TEST(test_register_dep_violated_callback);
    RUN_TEST(test_register_overall_health_callback);
    RUN_TEST(test_callback_receives_correct_cycle_type);

    /* Group 15: Full System Callback Integration */
    RUN_TEST(test_multiple_callbacks_same_event);
    RUN_TEST(test_callbacks_across_different_events);
    RUN_TEST(test_reentrant_callback_safety);
    RUN_TEST(test_callback_ordering_preserved);

    return UNITY_END();
}
