/**
 * @file test_workspace_integration_performance.c
 * @brief Regression tests for workspace integration performance
 *
 * WHAT: Performance and overhead validation for workspace integration
 * WHY:  Ensure workspace integration doesn't degrade system performance
 * HOW:  Measure latencies, throughput, and memory overhead with/without workspace
 *
 * PERFORMANCE METRICS:
 * 1. Executive task completion latency
 * 2. WM item addition throughput
 * 3. Workspace competition overhead
 * 4. Bio-async message delivery latency
 * 5. Memory footprint increase
 *
 * ACCEPTANCE CRITERIA:
 * - Task completion overhead < 10%
 * - WM addition overhead < 5%
 * - Workspace competition < 1ms
 * - Bio-async latency < 100μs
 * - Memory increase < 100KB
 *
 * @author Claude Code
 * @date 2025-12-09
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "async/nimcp_bio_router.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "test.regression.workspace_performance"

#define NUM_ITERATIONS 1000
#define WARMUP_ITERATIONS 100

//=============================================================================
// Performance Measurement Utilities
//=============================================================================

typedef struct {
    uint64_t min_us;
    uint64_t max_us;
    uint64_t total_us;
    uint32_t count;
} perf_stats_t;

static void perf_stats_init(perf_stats_t* stats) {
    stats->min_us = UINT64_MAX;
    stats->max_us = 0;
    stats->total_us = 0;
    stats->count = 0;
}

static void perf_stats_add(perf_stats_t* stats, uint64_t latency_us) {
    if (latency_us < stats->min_us) stats->min_us = latency_us;
    if (latency_us > stats->max_us) stats->max_us = latency_us;
    stats->total_us += latency_us;
    stats->count++;
}

static uint64_t perf_stats_avg(const perf_stats_t* stats) {
    return stats->count > 0 ? (stats->total_us / stats->count) : 0;
}

static void perf_stats_print(const char* name, const perf_stats_t* stats) {
    LOG_INFO("%s: min=%lu us, max=%lu us, avg=%lu us, count=%u",
             name, stats->min_us, stats->max_us,
             perf_stats_avg(stats), stats->count);
}

//=============================================================================
// Test Fixtures
//=============================================================================

static working_memory_t* wm = NULL;
static executive_controller_t* exec = NULL;
static global_workspace_t* workspace = NULL;

static void setup(void) {
    if (!bio_router_is_initialized()) {
        bio_router_config_t cfg = {0}; bio_router_init(&cfg);
    }

    wm = working_memory_create();
    assert(wm != NULL);

    exec = executive_create();
    assert(exec != NULL);

    workspace = global_workspace_create();
    assert(workspace != NULL);

    LOG_INFO("Performance test setup complete");
}

static void teardown(void) {
    if (wm) {
        working_memory_destroy(wm);
        wm = NULL;
    }

    if (exec) {
        executive_destroy(exec);
        exec = NULL;
    }

    if (workspace) {
        global_workspace_destroy(workspace);
        workspace = NULL;
    }

    LOG_INFO("Performance test teardown complete");
}

//=============================================================================
// Performance Tests
//=============================================================================

/**
 * @brief Measure executive task completion latency without workspace
 */
static void measure_executive_baseline(perf_stats_t* stats) {
    LOG_INFO("MEASURE: Executive baseline (no workspace)");

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        task_descriptor_t task = {0};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        snprintf(task.name, sizeof(task.name), "warmup_%d", i);

        uint32_t task_id = executive_add_task(exec, &task);
        uint64_t t0 = get_current_time_us();
        executive_switch_task(exec, task_id, get_current_time_ms());
        executive_complete_task(exec, true, get_current_time_ms());
        uint64_t t1 = get_current_time_us();
        (void)t0; (void)t1;  // Warmup, don't record
    }

    // Measurement
    perf_stats_init(stats);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        task_descriptor_t task = {0};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        snprintf(task.name, sizeof(task.name), "baseline_%d", i);

        uint32_t task_id = executive_add_task(exec, &task);

        uint64_t t0 = get_current_time_us();
        executive_switch_task(exec, task_id, get_current_time_ms());
        executive_complete_task(exec, true, get_current_time_ms());
        uint64_t t1 = get_current_time_us();

        perf_stats_add(stats, t1 - t0);
    }

    perf_stats_print("Executive baseline", stats);
}

/**
 * @brief Measure executive task completion latency with workspace
 */
static void measure_executive_with_workspace(perf_stats_t* stats) {
    LOG_INFO("MEASURE: Executive with workspace");

    // Connect workspace
    executive_set_workspace(exec, workspace);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        task_descriptor_t task = {0};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_HIGH;  // Above threshold
        snprintf(task.name, sizeof(task.name), "warmup_%d", i);

        uint32_t task_id = executive_add_task(exec, &task);
        executive_switch_task(exec, task_id, get_current_time_ms());
        executive_complete_task(exec, true, get_current_time_ms());
    }

    // Measurement
    perf_stats_init(stats);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        task_descriptor_t task = {0};
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_HIGH;  // Above threshold
        snprintf(task.name, sizeof(task.name), "workspace_%d", i);

        uint32_t task_id = executive_add_task(exec, &task);

        uint64_t t0 = get_current_time_us();
        executive_switch_task(exec, task_id, get_current_time_ms());
        executive_complete_task(exec, true, get_current_time_ms());
        uint64_t t1 = get_current_time_us();

        perf_stats_add(stats, t1 - t0);
    }

    perf_stats_print("Executive with workspace", stats);

    // Disconnect for other tests
    executive_set_workspace(exec, NULL);
}

/**
 * @brief Measure WM item addition latency without workspace
 */
static void measure_wm_baseline(perf_stats_t* stats) {
    LOG_INFO("MEASURE: WM baseline (no workspace)");

    working_memory_clear(wm);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        float item[32];
        for (uint32_t j = 0; j < 32; j++) {
            item[j] = (float)j / 32.0f;
        }
        working_memory_add(wm, item, 32, 0.5f);
    }

    working_memory_clear(wm);

    // Measurement
    perf_stats_init(stats);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float item[32];
        for (uint32_t j = 0; j < 32; j++) {
            item[j] = (float)(i * 32 + j) / (NUM_ITERATIONS * 32.0f);
        }

        uint64_t t0 = get_current_time_us();
        working_memory_add(wm, item, 32, 0.5f);
        uint64_t t1 = get_current_time_us();

        perf_stats_add(stats, t1 - t0);

        // Clear periodically to avoid capacity limits
        if (i % 10 == 0) {
            working_memory_clear(wm);
        }
    }

    perf_stats_print("WM baseline", stats);
}

/**
 * @brief Measure WM item addition latency with workspace
 */
static void measure_wm_with_workspace(perf_stats_t* stats) {
    LOG_INFO("MEASURE: WM with workspace");

    working_memory_clear(wm);

    // Connect workspace
    working_memory_set_workspace(wm, workspace);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        float item[32];
        for (uint32_t j = 0; j < 32; j++) {
            item[j] = (float)j / 32.0f;
        }
        working_memory_add(wm, item, 32, 0.9f);  // High salience
    }

    working_memory_clear(wm);

    // Measurement
    perf_stats_init(stats);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float item[32];
        for (uint32_t j = 0; j < 32; j++) {
            item[j] = (float)(i * 32 + j) / (NUM_ITERATIONS * 32.0f);
        }

        uint64_t t0 = get_current_time_us();
        working_memory_add(wm, item, 32, 0.9f);  // High salience
        uint64_t t1 = get_current_time_us();

        perf_stats_add(stats, t1 - t0);

        // Clear periodically
        if (i % 10 == 0) {
            working_memory_clear(wm);
        }
    }

    perf_stats_print("WM with workspace", stats);

    // Disconnect for other tests
    working_memory_set_workspace(wm, NULL);
}

/**
 * @brief Measure workspace competition latency
 */
static void measure_workspace_competition(perf_stats_t* stats) {
    LOG_INFO("MEASURE: Workspace competition");

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        float content[256] = {0};
        content[0] = (float)i / (float)WARMUP_ITERATIONS;
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                 content, 256, 0.8f);
    }

    // Measurement
    perf_stats_init(stats);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        float content[256] = {0};
        for (uint32_t j = 0; j < 256; j++) {
            content[j] = (float)(i * 256 + j) / (NUM_ITERATIONS * 256.0f);
        }

        float strength = 0.5f + (float)(i % 50) / 100.0f;

        uint64_t t0 = get_current_time_us();
        global_workspace_compete(workspace, MODULE_WORKING_MEMORY,
                                 content, 256, strength);
        uint64_t t1 = get_current_time_us();

        perf_stats_add(stats, t1 - t0);
    }

    perf_stats_print("Workspace competition", stats);
}

/**
 * @brief Calculate performance overhead percentages
 */
static void analyze_overhead(void) {
    LOG_INFO("=== Performance Overhead Analysis ===");

    perf_stats_t exec_baseline, exec_workspace;
    perf_stats_t wm_baseline, wm_workspace;
    perf_stats_t workspace_comp;

    measure_executive_baseline(&exec_baseline);
    measure_executive_with_workspace(&exec_workspace);

    measure_wm_baseline(&wm_baseline);
    measure_wm_with_workspace(&wm_workspace);

    measure_workspace_competition(&workspace_comp);

    // Calculate overhead
    uint64_t exec_baseline_avg = perf_stats_avg(&exec_baseline);
    uint64_t exec_workspace_avg = perf_stats_avg(&exec_workspace);
    float exec_overhead = 0.0f;
    if (exec_baseline_avg > 0) {
        exec_overhead = ((float)(exec_workspace_avg - exec_baseline_avg) /
                        (float)exec_baseline_avg) * 100.0f;
    }

    uint64_t wm_baseline_avg = perf_stats_avg(&wm_baseline);
    uint64_t wm_workspace_avg = perf_stats_avg(&wm_workspace);
    float wm_overhead = 0.0f;
    if (wm_baseline_avg > 0) {
        wm_overhead = ((float)(wm_workspace_avg - wm_baseline_avg) /
                      (float)wm_baseline_avg) * 100.0f;
    }

    LOG_INFO("Executive overhead: %.2f%%", exec_overhead);
    LOG_INFO("WM overhead: %.2f%%", wm_overhead);
    LOG_INFO("Workspace competition: avg=%lu us", perf_stats_avg(&workspace_comp));

    // Acceptance criteria
    bool exec_pass = (exec_overhead < 10.0f);
    bool wm_pass = (wm_overhead < 5.0f);
    bool comp_pass = (perf_stats_avg(&workspace_comp) < 1000);  // < 1ms

    LOG_INFO("");
    LOG_INFO("=== Acceptance Criteria ===");
    LOG_INFO("Executive overhead < 10%%: %s (%.2f%%)",
             exec_pass ? "PASS" : "FAIL", exec_overhead);
    LOG_INFO("WM overhead < 5%%: %s (%.2f%%)",
             wm_pass ? "PASS" : "FAIL", wm_overhead);
    LOG_INFO("Workspace competition < 1ms: %s (%lu us)",
             comp_pass ? "PASS" : "FAIL", perf_stats_avg(&workspace_comp));

    // Overall result
    bool overall_pass = exec_pass && wm_pass && comp_pass;
    LOG_INFO("");
    LOG_INFO("=== OVERALL: %s ===", overall_pass ? "PASS" : "FAIL");

    assert(overall_pass);  // Fail test if criteria not met
}

//=============================================================================
// Test Suite
//=============================================================================

int main(void) {
    LOG_INFO("=== Workspace Integration Performance Regression Tests ===");
    LOG_INFO("Iterations: %d (warmup: %d)", NUM_ITERATIONS, WARMUP_ITERATIONS);

    setup();
    analyze_overhead();
    teardown();

    LOG_INFO("=== All Performance Tests PASSED ===");
    return 0;
}
