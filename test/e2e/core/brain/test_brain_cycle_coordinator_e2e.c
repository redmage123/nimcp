// SPDX-License-Identifier: MIT
// Copyright (c) 2025 NIMCP Project

/**
 * @file test_brain_cycle_coordinator_e2e.c
 * @brief End-to-end tests for the Brain Cycle Coordinator
 *
 * WHAT: E2E tests verifying the brain cycle coordinator manages all brain
 *       subsystem cycles, detects stalls, tracks health, and fires callbacks
 * WHY:  Validate that the coordinator correctly orchestrates the full set of
 *       brain cycles (immune tick, health agent, plasticity, oscillation, etc.)
 *       and responds to degradation / recovery in a realistic pipeline
 * HOW:  Uses Unity test framework with mock health functions and callback
 *       tracking to exercise the full coordinator lifecycle
 *
 * @author NIMCP Development Team
 * @date 2025-01-26
 */

#include "unity.h"
#include "core/brain/nimcp_brain_cycle_coordinator.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_TRACKED_EVENTS       256
#define MAX_CALLBACK_SETS        16
#define ALL_CYCLES_COUNT         9
#define DIAGNOSE_BUFFER_SIZE     4096

/* Cycle names matching brain subsystems */
static const char* ALL_CYCLE_NAMES[ALL_CYCLES_COUNT] = {
    "immune_tick",
    "health_agent",
    "plasticity",
    "oscillation",
    "homeostasis",
    "neuromodulation",
    "glial",
    "bio_async",
    "meta_learning"
};

/* ============================================================================
 * Callback Tracking Structures
 * ============================================================================ */

typedef struct {
    char cycle_name[64];
    cycle_health_state_t old_state;
    cycle_health_state_t new_state;
} health_changed_event_t;

typedef struct {
    char cycle_name[64];
    uint64_t last_tick_age_us;
} stall_detected_event_t;

typedef struct {
    char cycle_name[64];
    char dep_name[64];
} dep_violated_event_t;

typedef struct {
    float old_health;
    float new_health;
} overall_health_event_t;

/* Global callback tracking arrays */
static health_changed_event_t g_health_events[MAX_TRACKED_EVENTS];
static int g_health_event_count = 0;

static stall_detected_event_t g_stall_events[MAX_TRACKED_EVENTS];
static int g_stall_event_count = 0;

static dep_violated_event_t g_dep_events[MAX_TRACKED_EVENTS];
static int g_dep_event_count = 0;

static overall_health_event_t g_overall_events[MAX_TRACKED_EVENTS];
static int g_overall_event_count = 0;

/* ============================================================================
 * Mock Health Functions
 * ============================================================================ */

static cycle_health_state_t mock_health_healthy(void* ctx)
{
    (void)ctx;
    return CYCLE_HEALTH_HEALTHY;
}

static cycle_health_state_t mock_health_degraded(void* ctx)
{
    (void)ctx;
    return CYCLE_HEALTH_DEGRADED;
}

static cycle_health_state_t mock_health_stalled(void* ctx)
{
    (void)ctx;
    return CYCLE_HEALTH_STALLED;
}

static cycle_health_state_t mock_health_error(void* ctx)
{
    (void)ctx;
    return CYCLE_HEALTH_ERROR;
}

/* Configurable health function that reads from a global */
static cycle_health_state_t g_configurable_state = CYCLE_HEALTH_HEALTHY;

static cycle_health_state_t mock_health_configurable(void* ctx)
{
    (void)ctx;
    return g_configurable_state;
}

/* Per-cycle configurable states (indexed by user_data pointer cast) */
static cycle_health_state_t g_per_cycle_states[ALL_CYCLES_COUNT];

static cycle_health_state_t mock_health_per_cycle(void* ctx)
{
    int idx = (int)(intptr_t)ctx;
    if (idx >= 0 && idx < ALL_CYCLES_COUNT) {
        return g_per_cycle_states[idx];
    }
    return CYCLE_HEALTH_HEALTHY;
}

/* ============================================================================
 * Callback Implementations
 * ============================================================================ */

static void cb_health_changed(const char* cycle_name,
                              cycle_health_state_t old_state,
                              cycle_health_state_t new_state,
                              void* user_data)
{
    (void)user_data;
    if (g_health_event_count < MAX_TRACKED_EVENTS) {
        health_changed_event_t* evt = &g_health_events[g_health_event_count++];
        strncpy(evt->cycle_name, cycle_name, sizeof(evt->cycle_name) - 1);
        evt->cycle_name[sizeof(evt->cycle_name) - 1] = '\0';
        evt->old_state = old_state;
        evt->new_state = new_state;
    }
}

static void cb_stall_detected(const char* cycle_name,
                              uint64_t last_tick_age_us,
                              void* user_data)
{
    (void)user_data;
    if (g_stall_event_count < MAX_TRACKED_EVENTS) {
        stall_detected_event_t* evt = &g_stall_events[g_stall_event_count++];
        strncpy(evt->cycle_name, cycle_name, sizeof(evt->cycle_name) - 1);
        evt->cycle_name[sizeof(evt->cycle_name) - 1] = '\0';
        evt->last_tick_age_us = last_tick_age_us;
    }
}

static void cb_dep_violated(const char* cycle_name,
                            const char* dep_name,
                            void* user_data)
{
    (void)user_data;
    if (g_dep_event_count < MAX_TRACKED_EVENTS) {
        dep_violated_event_t* evt = &g_dep_events[g_dep_event_count++];
        strncpy(evt->cycle_name, cycle_name, sizeof(evt->cycle_name) - 1);
        evt->cycle_name[sizeof(evt->cycle_name) - 1] = '\0';
        strncpy(evt->dep_name, dep_name, sizeof(evt->dep_name) - 1);
        evt->dep_name[sizeof(evt->dep_name) - 1] = '\0';
    }
}

static void cb_overall_health_changed(float old_health,
                                      float new_health,
                                      void* user_data)
{
    (void)user_data;
    if (g_overall_event_count < MAX_TRACKED_EVENTS) {
        overall_health_event_t* evt = &g_overall_events[g_overall_event_count++];
        evt->old_health = old_health;
        evt->new_health = new_health;
    }
}

/* Multi-callback set tracking for bidirectional tests */
typedef struct {
    int health_changed_count;
    int stall_count;
    int dep_violated_count;
    int overall_health_count;
    const char* label;
} callback_set_tracker_t;

static callback_set_tracker_t g_cb_sets[MAX_CALLBACK_SETS];
static int g_cb_set_count = 0;

static void cb_set_health_changed(const char* cycle_name,
                                  cycle_health_state_t old_state,
                                  cycle_health_state_t new_state,
                                  void* user_data)
{
    (void)cycle_name;
    (void)old_state;
    (void)new_state;
    callback_set_tracker_t* tracker = (callback_set_tracker_t*)user_data;
    if (tracker) {
        tracker->health_changed_count++;
    }
}

static void cb_set_stall_detected(const char* cycle_name,
                                  uint64_t last_tick_age_us,
                                  void* user_data)
{
    (void)cycle_name;
    (void)last_tick_age_us;
    callback_set_tracker_t* tracker = (callback_set_tracker_t*)user_data;
    if (tracker) {
        tracker->stall_count++;
    }
}

static void cb_set_dep_violated(const char* cycle_name,
                                const char* dep_name,
                                void* user_data)
{
    (void)cycle_name;
    (void)dep_name;
    callback_set_tracker_t* tracker = (callback_set_tracker_t*)user_data;
    if (tracker) {
        tracker->dep_violated_count++;
    }
}

static void cb_set_overall_health(float old_health,
                                  float new_health,
                                  void* user_data)
{
    (void)old_health;
    (void)new_health;
    callback_set_tracker_t* tracker = (callback_set_tracker_t*)user_data;
    if (tracker) {
        tracker->overall_health_count++;
    }
}

/* ============================================================================
 * Callback that modifies coordinator state (for exception handling test)
 * ============================================================================ */

static brain_cycle_coordinator_t* g_reentrant_coordinator = NULL;

static void cb_reentrant_health_changed(const char* cycle_name,
                                        cycle_health_state_t old_state,
                                        cycle_health_state_t new_state,
                                        void* user_data)
{
    (void)cycle_name;
    (void)old_state;
    (void)new_state;
    (void)user_data;

    /* Attempt to register another cycle from within a callback */
    if (g_reentrant_coordinator) {
        cycle_registration_t reg;
        memset(&reg, 0, sizeof(reg));
        strncpy(reg.name, "reentrant_cycle", sizeof(reg.name) - 1);
        reg.health_fn = mock_health_healthy;
        reg.health_fn_ctx = NULL;
        reg.stall_timeout_us = 5000000;
        /* This should either succeed safely or be rejected -- no crash */
        brain_cycle_coordinator_register(g_reentrant_coordinator, &reg);
    }
}

/* ============================================================================
 * Fixtures
 * ============================================================================ */

static brain_cycle_coordinator_t* g_coordinator = NULL;

static void reset_tracking(void)
{
    g_health_event_count = 0;
    g_stall_event_count = 0;
    g_dep_event_count = 0;
    g_overall_event_count = 0;
    g_cb_set_count = 0;
    g_configurable_state = CYCLE_HEALTH_HEALTHY;
    g_reentrant_coordinator = NULL;
    memset(g_health_events, 0, sizeof(g_health_events));
    memset(g_stall_events, 0, sizeof(g_stall_events));
    memset(g_dep_events, 0, sizeof(g_dep_events));
    memset(g_overall_events, 0, sizeof(g_overall_events));
    memset(g_cb_sets, 0, sizeof(g_cb_sets));
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        g_per_cycle_states[i] = CYCLE_HEALTH_HEALTHY;
    }
}

void setUp(void)
{
    reset_tracking();

    brain_cycle_coordinator_config_t config;
    memset(&config, 0, sizeof(config));
    config.max_cycles = 32;
    config.default_stall_timeout_us = 5000000;  /* 5 seconds */
    config.enable_dependency_tracking = true;
    config.enable_health_callbacks = true;
    config.enable_stall_detection = true;

    g_coordinator = brain_cycle_coordinator_create(&config);
    TEST_ASSERT_NOT_NULL(g_coordinator);

    /* Register all callback types */
    brain_cycle_coordinator_on_health_changed(g_coordinator, cb_health_changed, NULL);
    brain_cycle_coordinator_on_stall_detected(g_coordinator, cb_stall_detected, NULL);
    brain_cycle_coordinator_on_dep_violated(g_coordinator, cb_dep_violated, NULL);
    brain_cycle_coordinator_on_overall_health_changed(g_coordinator,
                                                      cb_overall_health_changed, NULL);
}

void tearDown(void)
{
    if (g_coordinator) {
        brain_cycle_coordinator_destroy(g_coordinator);
        g_coordinator = NULL;
    }
    reset_tracking();
}

/* ============================================================================
 * Helper: register all 9 cycles with a given health function
 * ============================================================================ */

static void register_all_cycles_with_fn(cycle_health_fn_t fn, void* ctx)
{
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        cycle_registration_t reg;
        memset(&reg, 0, sizeof(reg));
        strncpy(reg.name, ALL_CYCLE_NAMES[i], sizeof(reg.name) - 1);
        reg.health_fn = fn;
        reg.health_fn_ctx = ctx;
        reg.stall_timeout_us = 5000000;

        int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }
}

static void register_all_cycles_per_cycle(void)
{
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        cycle_registration_t reg;
        memset(&reg, 0, sizeof(reg));
        strncpy(reg.name, ALL_CYCLE_NAMES[i], sizeof(reg.name) - 1);
        reg.health_fn = mock_health_per_cycle;
        reg.health_fn_ctx = (void*)(intptr_t)i;
        reg.stall_timeout_us = 5000000;

        int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }
}

/* ============================================================================
 * GROUP 1: Full System Tests (4 tests)
 * ============================================================================ */

void test_brain_with_all_cycles_healthy(void)
{
    register_all_cycles_with_fn(mock_health_healthy, NULL);

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 1000);
    }

    int rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, stats.overall_health);
    TEST_ASSERT_EQUAL_UINT32(ALL_CYCLES_COUNT, stats.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.degraded_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.stalled_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.error_count);
}

void test_detect_stalled_immune_tick(void)
{
    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = NULL;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 1;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_GREATER_OR_EQUAL(1, g_stall_event_count);
    TEST_ASSERT_EQUAL_STRING("immune_tick", g_stall_events[0].cycle_name);
}

void test_detect_stalled_health_agent(void)
{
    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "health_agent", sizeof(reg.name) - 1);
    reg.health_fn = NULL;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 1;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_GREATER_OR_EQUAL(1, g_stall_event_count);
    TEST_ASSERT_EQUAL_STRING("health_agent", g_stall_events[0].cycle_name);
}

void test_recovery_after_stall(void)
{
    g_configurable_state = CYCLE_HEALTH_STALLED;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "plasticity", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_GREATER_OR_EQUAL(1, (int)(stats.stalled_count + stats.error_count));

    g_configurable_state = CYCLE_HEALTH_HEALTHY;
    brain_cycle_coordinator_notify_tick(g_coordinator, "plasticity", 500);

    rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT32(1, stats.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.stalled_count);
    TEST_ASSERT_GREATER_OR_EQUAL(1, g_health_event_count);
}

/* ============================================================================
 * GROUP 2: Observability Tests (3 tests)
 * ============================================================================ */

void test_diagnose_full_system(void)
{
    g_per_cycle_states[0] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[1] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[2] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[3] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[4] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[5] = CYCLE_HEALTH_STALLED;
    g_per_cycle_states[6] = CYCLE_HEALTH_DISABLED;
    g_per_cycle_states[7] = CYCLE_HEALTH_DISABLED;
    g_per_cycle_states[8] = CYCLE_HEALTH_DISABLED;

    register_all_cycles_per_cycle();

    for (int i = 0; i < 6; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);

    char buffer[DIAGNOSE_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int issue_count = brain_cycle_coordinator_diagnose(g_coordinator, buffer,
                                                       sizeof(buffer));

    TEST_ASSERT_GREATER_OR_EQUAL(3, issue_count);
    TEST_ASSERT_NOT_NULL(strstr(buffer, "oscillation"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "homeostasis"));
    TEST_ASSERT_NOT_NULL(strstr(buffer, "neuromodulation"));
}

void test_log_state_output(void)
{
    register_all_cycles_with_fn(mock_health_healthy, NULL);

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 1000);
    }

    brain_cycle_coordinator_check_health(g_coordinator);
    int rc = brain_cycle_coordinator_log_state(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_stats_accuracy_under_load(void)
{
    const int NUM_CYCLES = 5;
    const int TICKS_PER_CYCLE = 1000;
    const uint64_t DURATION_US = 500;

    for (int i = 0; i < NUM_CYCLES; i++) {
        cycle_registration_t reg;
        memset(&reg, 0, sizeof(reg));
        strncpy(reg.name, ALL_CYCLE_NAMES[i], sizeof(reg.name) - 1);
        reg.health_fn = mock_health_healthy;
        reg.health_fn_ctx = NULL;
        reg.stall_timeout_us = 5000000;

        int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    for (int t = 0; t < TICKS_PER_CYCLE; t++) {
        for (int i = 0; i < NUM_CYCLES; i++) {
            brain_cycle_coordinator_notify_tick(g_coordinator,
                                                ALL_CYCLE_NAMES[i],
                                                DURATION_US);
        }
    }

    for (int i = 0; i < NUM_CYCLES; i++) {
        cycle_stats_t cs;
        memset(&cs, 0, sizeof(cs));
        int rc = brain_cycle_coordinator_get_cycle_stats(g_coordinator,
                                                          ALL_CYCLE_NAMES[i],
                                                          &cs);
        TEST_ASSERT_EQUAL_INT(0, rc);
        TEST_ASSERT_EQUAL_UINT64(TICKS_PER_CYCLE, cs.ticks_executed);

        double expected = (double)DURATION_US;
        double actual = cs.avg_duration_us;
        double tolerance = expected * 0.01;
        TEST_ASSERT_FLOAT_WITHIN(tolerance, expected, actual);
    }
}

/* ============================================================================
 * GROUP 3: Stress Tests (3 tests)
 * ============================================================================ */

void test_high_frequency_notifications(void)
{
    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_healthy;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    const uint64_t TOTAL_TICKS = 100000;
    for (uint64_t t = 0; t < TOTAL_TICKS; t++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 100);
    }

    cycle_stats_t cs;
    memset(&cs, 0, sizeof(cs));
    rc = brain_cycle_coordinator_get_cycle_stats(g_coordinator, "immune_tick", &cs);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT64(TOTAL_TICKS, cs.ticks_executed);
}

void test_concurrent_health_checks(void)
{
    register_all_cycles_with_fn(mock_health_healthy, NULL);

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    for (int iter = 0; iter < 1000; iter++) {
        int rc = brain_cycle_coordinator_check_health(g_coordinator);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    brain_cycle_coordinator_stats_t stats;
    int rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_UINT32(ALL_CYCLES_COUNT, stats.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.degraded_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.stalled_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.error_count);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, stats.overall_health);
}

void test_long_running_stability(void)
{
    register_all_cycles_with_fn(mock_health_healthy, NULL);

    const int TOTAL_TICKS = 10000;
    const int CHECK_INTERVAL = 100;

    for (int t = 0; t < TOTAL_TICKS; t++) {
        int cycle_idx = t % ALL_CYCLES_COUNT;
        brain_cycle_coordinator_notify_tick(g_coordinator,
                                            ALL_CYCLE_NAMES[cycle_idx],
                                            200 + (t % 100));

        if ((t + 1) % CHECK_INTERVAL == 0) {
            int rc = brain_cycle_coordinator_check_health(g_coordinator);
            TEST_ASSERT_EQUAL_INT(0, rc);

            brain_cycle_coordinator_stats_t stats;
            rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
            TEST_ASSERT_EQUAL_INT(0, rc);

            TEST_ASSERT_EQUAL_UINT32(ALL_CYCLES_COUNT, stats.healthy_count);
            TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, stats.overall_health);
        }
    }

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        cycle_stats_t cs;
        int rc = brain_cycle_coordinator_get_cycle_stats(g_coordinator,
                                                          ALL_CYCLE_NAMES[i], &cs);
        TEST_ASSERT_EQUAL_INT(0, rc);
        TEST_ASSERT_GREATER_OR_EQUAL(1000, (int)cs.ticks_executed);
    }
}

/* ============================================================================
 * GROUP 4: Full Integration Pipeline (4 tests)
 * ============================================================================ */

void test_exception_to_immune_to_kg_flow(void)
{
    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    int initial_events = g_health_event_count;

    g_configurable_state = CYCLE_HEALTH_ERROR;
    brain_cycle_coordinator_check_health(g_coordinator);

    TEST_ASSERT_GREATER_THAN(initial_events, g_health_event_count);

    int found = 0;
    for (int i = initial_events; i < g_health_event_count; i++) {
        if (strcmp(g_health_events[i].cycle_name, "immune_tick") == 0 &&
            g_health_events[i].new_state == CYCLE_HEALTH_ERROR) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, found);
}

void test_bio_async_cross_module_notification(void)
{
    callback_set_tracker_t bio_tracker;
    memset(&bio_tracker, 0, sizeof(bio_tracker));
    bio_tracker.label = "bio_async";

    brain_cycle_coordinator_on_health_changed(g_coordinator,
                                              cb_set_health_changed,
                                              &bio_tracker);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "bio_async", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_notify_tick(g_coordinator, "bio_async", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    g_configurable_state = CYCLE_HEALTH_DEGRADED;
    brain_cycle_coordinator_check_health(g_coordinator);

    TEST_ASSERT_GREATER_OR_EQUAL(1, g_health_event_count);
    TEST_ASSERT_GREATER_OR_EQUAL(1, bio_tracker.health_changed_count);
}

void test_cycle_stall_full_recovery_flow(void)
{
    cycle_registration_t reg_osc;
    memset(&reg_osc, 0, sizeof(reg_osc));
    strncpy(reg_osc.name, "oscillation", sizeof(reg_osc.name) - 1);
    reg_osc.health_fn = mock_health_configurable;
    reg_osc.health_fn_ctx = NULL;
    reg_osc.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg_osc);
    TEST_ASSERT_EQUAL_INT(0, rc);

    cycle_registration_t reg_plas;
    memset(&reg_plas, 0, sizeof(reg_plas));
    strncpy(reg_plas.name, "plasticity", sizeof(reg_plas.name) - 1);
    reg_plas.health_fn = mock_health_healthy;
    reg_plas.health_fn_ctx = NULL;
    reg_plas.stall_timeout_us = 5000000;
    strncpy(reg_plas.dependencies[0], "oscillation",
            sizeof(reg_plas.dependencies[0]) - 1);
    reg_plas.num_dependencies = 1;

    rc = brain_cycle_coordinator_register(g_coordinator, &reg_plas);
    TEST_ASSERT_EQUAL_INT(0, rc);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;
    brain_cycle_coordinator_notify_tick(g_coordinator, "oscillation", 500);
    brain_cycle_coordinator_notify_tick(g_coordinator, "plasticity", 500);
    brain_cycle_coordinator_check_health(g_coordinator);

    g_configurable_state = CYCLE_HEALTH_STALLED;
    brain_cycle_coordinator_check_health(g_coordinator);

    TEST_ASSERT_GREATER_OR_EQUAL(1, g_dep_event_count);
    TEST_ASSERT_EQUAL_STRING("plasticity", g_dep_events[0].cycle_name);
    TEST_ASSERT_EQUAL_STRING("oscillation", g_dep_events[0].dep_name);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;
    brain_cycle_coordinator_notify_tick(g_coordinator, "oscillation", 500);
    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_UINT32(2, stats.healthy_count);
}

void test_pattern_detection_recurring_issues(void)
{
    g_per_cycle_states[0] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[1] = CYCLE_HEALTH_DEGRADED;
    for (int i = 2; i < ALL_CYCLES_COUNT; i++) {
        g_per_cycle_states[i] = CYCLE_HEALTH_HEALTHY;
    }

    register_all_cycles_per_cycle();

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    for (int round = 0; round < 5; round++) {
        int rc = brain_cycle_coordinator_check_health(g_coordinator);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    brain_cycle_coordinator_stats_t stats;
    int rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_EQUAL_UINT32(7, stats.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(2, stats.degraded_count);
    TEST_ASSERT_GREATER_OR_EQUAL(5, (int)stats.health_checks_performed);
}

/* ============================================================================
 * GROUP 5: Bidirectional E2E Tests (6 tests)
 * ============================================================================ */

void test_all_integrations_receive_health_updates(void)
{
    callback_set_tracker_t trackers[4];
    const char* labels[] = {"introspection", "hemispheric", "fep", "meta_learning"};

    for (int i = 0; i < 4; i++) {
        memset(&trackers[i], 0, sizeof(trackers[i]));
        trackers[i].label = labels[i];
        brain_cycle_coordinator_on_health_changed(g_coordinator,
                                                  cb_set_health_changed,
                                                  &trackers[i]);
    }

    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    g_configurable_state = CYCLE_HEALTH_DEGRADED;
    brain_cycle_coordinator_check_health(g_coordinator);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
            1, trackers[i].health_changed_count,
            labels[i]);
    }
}

void test_all_integrations_can_query_coordinator(void)
{
    register_all_cycles_per_cycle();

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    g_per_cycle_states[0] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[3] = CYCLE_HEALTH_DEGRADED;

    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats1, stats2, stats3, stats4;

    brain_cycle_coordinator_get_stats(g_coordinator, &stats1);
    brain_cycle_coordinator_get_stats(g_coordinator, &stats2);
    brain_cycle_coordinator_get_stats(g_coordinator, &stats3);
    brain_cycle_coordinator_get_stats(g_coordinator, &stats4);

    TEST_ASSERT_EQUAL_UINT32(stats1.healthy_count, stats2.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(stats1.degraded_count, stats3.degraded_count);
    TEST_ASSERT_EQUAL_UINT32(stats1.stalled_count, stats4.stalled_count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, stats1.overall_health, stats2.overall_health);

    TEST_ASSERT_EQUAL_UINT32(7, stats1.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(2, stats1.degraded_count);
}

void test_callback_cascade_system_degradation(void)
{
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        g_per_cycle_states[i] = (i < 5) ? CYCLE_HEALTH_DEGRADED : CYCLE_HEALTH_HEALTHY;
    }

    register_all_cycles_per_cycle();

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);

    TEST_ASSERT_LESS_THAN(0.75f, stats.overall_health);
    TEST_ASSERT_EQUAL_UINT32(5, stats.degraded_count);
    TEST_ASSERT_EQUAL_UINT32(4, stats.healthy_count);
    TEST_ASSERT_GREATER_OR_EQUAL(1, g_overall_event_count);
}

void test_bilateral_mode_triggered_by_stall(void)
{
    for (int i = 0; i < 5; i++) {
        g_per_cycle_states[i] = (i < 3) ? CYCLE_HEALTH_STALLED : CYCLE_HEALTH_HEALTHY;
    }

    for (int i = 0; i < 5; i++) {
        cycle_registration_t reg;
        memset(&reg, 0, sizeof(reg));
        strncpy(reg.name, ALL_CYCLE_NAMES[i], sizeof(reg.name) - 1);
        reg.health_fn = mock_health_per_cycle;
        reg.health_fn_ctx = (void*)(intptr_t)i;
        reg.stall_timeout_us = 5000000;

        int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
        TEST_ASSERT_EQUAL_INT(0, rc);
    }

    for (int i = 0; i < 5; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_UINT32(3, stats.stalled_count);

    int stall_transitions = 0;
    for (int i = 0; i < g_health_event_count; i++) {
        if (g_health_events[i].new_state == CYCLE_HEALTH_STALLED) {
            stall_transitions++;
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL(3, stall_transitions);
}

void test_meta_learning_adapts_to_health(void)
{
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        g_per_cycle_states[i] = CYCLE_HEALTH_HEALTHY;
    }

    register_all_cycles_per_cycle();

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, stats.overall_health);

    int events_before = g_health_event_count;
    int overall_before = g_overall_event_count;

    g_per_cycle_states[0] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[2] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[4] = CYCLE_HEALTH_DEGRADED;

    brain_cycle_coordinator_check_health(g_coordinator);

    int new_events = g_health_event_count - events_before;
    TEST_ASSERT_GREATER_OR_EQUAL(3, new_events);
    TEST_ASSERT_GREATER_THAN(overall_before, g_overall_event_count);

    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_LESS_THAN(1.0f, stats.overall_health);
    TEST_ASSERT_EQUAL_UINT32(3, stats.degraded_count);
}

void test_full_system_recovery_flow(void)
{
    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        g_per_cycle_states[i] = CYCLE_HEALTH_HEALTHY;
    }

    register_all_cycles_per_cycle();

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);
    brain_cycle_coordinator_stats_t stats;
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_UINT32(ALL_CYCLES_COUNT, stats.healthy_count);
    float initial_health = stats.overall_health;

    g_per_cycle_states[0] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[1] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[2] = CYCLE_HEALTH_DEGRADED;
    g_per_cycle_states[3] = CYCLE_HEALTH_DEGRADED;

    brain_cycle_coordinator_check_health(g_coordinator);
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_UINT32(4, stats.degraded_count);
    TEST_ASSERT_EQUAL_UINT32(5, stats.healthy_count);
    float degraded_health = stats.overall_health;
    TEST_ASSERT_LESS_THAN(initial_health, degraded_health);

    int events_after_degrade = g_health_event_count;
    TEST_ASSERT_GREATER_OR_EQUAL(4, events_after_degrade);

    g_per_cycle_states[0] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[1] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[2] = CYCLE_HEALTH_HEALTHY;
    g_per_cycle_states[3] = CYCLE_HEALTH_HEALTHY;

    for (int i = 0; i < 4; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);
    brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_UINT32(ALL_CYCLES_COUNT, stats.healthy_count);
    TEST_ASSERT_EQUAL_UINT32(0, stats.degraded_count);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, initial_health, stats.overall_health);

    int recovery_events = g_health_event_count - events_after_degrade;
    TEST_ASSERT_GREATER_OR_EQUAL(4, recovery_events);
}

/* ============================================================================
 * GROUP 6: Callback Stress E2E Tests (4 tests)
 * ============================================================================ */

void test_many_callbacks_rapid_health_changes(void)
{
    const int NUM_SETS = 10;
    const int NUM_CHANGES = 100;

    callback_set_tracker_t trackers[NUM_SETS];
    for (int i = 0; i < NUM_SETS; i++) {
        memset(&trackers[i], 0, sizeof(trackers[i]));
        trackers[i].label = "rapid_change";
        brain_cycle_coordinator_on_health_changed(g_coordinator,
                                                  cb_set_health_changed,
                                                  &trackers[i]);
    }

    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    brain_cycle_coordinator_register(g_coordinator, &reg);
    brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    int expected_transitions = 0;
    for (int c = 0; c < NUM_CHANGES; c++) {
        cycle_health_state_t prev = g_configurable_state;
        g_configurable_state = (c % 2 == 0) ? CYCLE_HEALTH_DEGRADED
                                             : CYCLE_HEALTH_HEALTHY;
        if (g_configurable_state != prev) {
            expected_transitions++;
        }

        brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 500);
        brain_cycle_coordinator_check_health(g_coordinator);
    }

    for (int i = 0; i < NUM_SETS; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL(expected_transitions,
                                     trackers[i].health_changed_count);
    }

    for (int i = 1; i < NUM_SETS; i++) {
        TEST_ASSERT_EQUAL_INT(trackers[0].health_changed_count,
                              trackers[i].health_changed_count);
    }
}

void test_concurrent_callback_registration(void)
{
    callback_set_tracker_t tracker_a;
    callback_set_tracker_t tracker_b;
    memset(&tracker_a, 0, sizeof(tracker_a));
    memset(&tracker_b, 0, sizeof(tracker_b));
    tracker_a.label = "tracker_a";
    tracker_b.label = "tracker_b";

    int id_a = brain_cycle_coordinator_on_health_changed(g_coordinator,
                                                         cb_set_health_changed,
                                                         &tracker_a);
    int id_b = brain_cycle_coordinator_on_health_changed(g_coordinator,
                                                         cb_set_health_changed,
                                                         &tracker_b);
    TEST_ASSERT_GREATER_OR_EQUAL(0, id_a);
    TEST_ASSERT_GREATER_OR_EQUAL(0, id_b);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "plasticity", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;
    brain_cycle_coordinator_register(g_coordinator, &reg);

    brain_cycle_coordinator_notify_tick(g_coordinator, "plasticity", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    g_configurable_state = CYCLE_HEALTH_DEGRADED;
    brain_cycle_coordinator_check_health(g_coordinator);

    TEST_ASSERT_GREATER_OR_EQUAL(1, tracker_a.health_changed_count);
    TEST_ASSERT_GREATER_OR_EQUAL(1, tracker_b.health_changed_count);
    int a_count_after_first = tracker_a.health_changed_count;
    int b_count_after_first = tracker_b.health_changed_count;

    brain_cycle_coordinator_remove_health_changed(g_coordinator, id_a);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;
    brain_cycle_coordinator_check_health(g_coordinator);

    TEST_ASSERT_EQUAL_INT(a_count_after_first, tracker_a.health_changed_count);
    TEST_ASSERT_GREATER_THAN(b_count_after_first, tracker_b.health_changed_count);
}

void test_callback_during_shutdown(void)
{
    callback_set_tracker_t tracker;
    memset(&tracker, 0, sizeof(tracker));
    tracker.label = "shutdown_test";

    brain_cycle_coordinator_on_health_changed(g_coordinator,
                                              cb_set_health_changed,
                                              &tracker);
    brain_cycle_coordinator_on_overall_health_changed(g_coordinator,
                                                      cb_set_overall_health,
                                                      &tracker);

    register_all_cycles_with_fn(mock_health_healthy, NULL);

    for (int i = 0; i < ALL_CYCLES_COUNT; i++) {
        brain_cycle_coordinator_notify_tick(g_coordinator, ALL_CYCLE_NAMES[i], 500);
    }

    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_destroy(g_coordinator);
    g_coordinator = NULL;

    TEST_PASS();
}

void test_callback_exception_handling(void)
{
    g_reentrant_coordinator = g_coordinator;

    brain_cycle_coordinator_on_health_changed(g_coordinator,
                                              cb_reentrant_health_changed,
                                              NULL);

    g_configurable_state = CYCLE_HEALTH_HEALTHY;

    cycle_registration_t reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.name, "immune_tick", sizeof(reg.name) - 1);
    reg.health_fn = mock_health_configurable;
    reg.health_fn_ctx = NULL;
    reg.stall_timeout_us = 5000000;

    int rc = brain_cycle_coordinator_register(g_coordinator, &reg);
    TEST_ASSERT_EQUAL_INT(0, rc);

    brain_cycle_coordinator_notify_tick(g_coordinator, "immune_tick", 1000);
    brain_cycle_coordinator_check_health(g_coordinator);

    g_configurable_state = CYCLE_HEALTH_ERROR;
    brain_cycle_coordinator_check_health(g_coordinator);

    brain_cycle_coordinator_stats_t stats;
    rc = brain_cycle_coordinator_get_stats(g_coordinator, &stats);
    TEST_ASSERT_EQUAL_INT(0, rc);

    TEST_ASSERT_GREATER_OR_EQUAL(1, (int)stats.total_cycles);

    rc = brain_cycle_coordinator_check_health(g_coordinator);
    TEST_ASSERT_EQUAL_INT(0, rc);

    g_reentrant_coordinator = NULL;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    UNITY_BEGIN();

    /* Full System */
    RUN_TEST(test_brain_with_all_cycles_healthy);
    RUN_TEST(test_detect_stalled_immune_tick);
    RUN_TEST(test_detect_stalled_health_agent);
    RUN_TEST(test_recovery_after_stall);

    /* Observability */
    RUN_TEST(test_diagnose_full_system);
    RUN_TEST(test_log_state_output);
    RUN_TEST(test_stats_accuracy_under_load);

    /* Stress */
    RUN_TEST(test_high_frequency_notifications);
    RUN_TEST(test_concurrent_health_checks);
    RUN_TEST(test_long_running_stability);

    /* Full Integration Pipeline */
    RUN_TEST(test_exception_to_immune_to_kg_flow);
    RUN_TEST(test_bio_async_cross_module_notification);
    RUN_TEST(test_cycle_stall_full_recovery_flow);
    RUN_TEST(test_pattern_detection_recurring_issues);

    /* Bidirectional E2E */
    RUN_TEST(test_all_integrations_receive_health_updates);
    RUN_TEST(test_all_integrations_can_query_coordinator);
    RUN_TEST(test_callback_cascade_system_degradation);
    RUN_TEST(test_bilateral_mode_triggered_by_stall);
    RUN_TEST(test_meta_learning_adapts_to_health);
    RUN_TEST(test_full_system_recovery_flow);

    /* Callback Stress E2E */
    RUN_TEST(test_many_callbacks_rapid_health_changes);
    RUN_TEST(test_concurrent_callback_registration);
    RUN_TEST(test_callback_during_shutdown);
    RUN_TEST(test_callback_exception_handling);

    return UNITY_END();
}
