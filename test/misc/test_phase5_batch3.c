/**
 * @file test_phase5_batch3.c
 * @brief Phase 5 Batch 3: Executive, Hypergraph, Creative Orchestrator
 *
 * Tests for: executive decision handler, hypergraph pattern matching,
 * hypergraph from KB, creative orchestrator subsystem init.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cognitive/nimcp_executive.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/creative/nimcp_creative_orchestrator.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Hypergraph Pattern Matching Tests
 * ========================================================================= */

static void test_hg_pattern_empty(void) {
    TEST("Hypergraph: empty pattern returns 0 matches");
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    nimcp_hypergraph_t* pattern = nimcp_hypergraph_create();

    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "x", 1.0f);

    hypergraph_query_result_t result;
    uint32_t matches = nimcp_hypergraph_pattern_query(hg, pattern, &result, 10);
    ASSERT_TRUE(matches == 0, "empty pattern should match 0");

    nimcp_hypergraph_destroy(pattern);
    nimcp_hypergraph_destroy(hg);
    PASS();
}

static void test_hg_pattern_type_match(void) {
    TEST("Hypergraph: pattern matches vertices by type");
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "x", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "y", 0.9f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "z", 0.8f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "f", 0.7f);

    /* Pattern: find all CONSTANT vertices */
    nimcp_hypergraph_t* pattern = nimcp_hypergraph_create();
    nimcp_hypergraph_add_vertex(pattern, HYPERVERTEX_CONSTANT, "", 1.0f);

    hypergraph_query_result_t result;
    uint32_t matches = nimcp_hypergraph_pattern_query(hg, pattern, &result, 10);
    ASSERT_TRUE(matches == 2, "should find 2 CONSTANT vertices");
    ASSERT_TRUE(result.vertex_count == 2, "vertex_count should be 2");
    ASSERT_TRUE(result.avg_confidence > 0.0f, "avg_confidence should be positive");

    nimcp_free(result.vertex_ids);
    if (result.edge_ids) nimcp_free(result.edge_ids);
    nimcp_hypergraph_destroy(pattern);
    nimcp_hypergraph_destroy(hg);
    PASS();
}

static void test_hg_pattern_label_match(void) {
    TEST("Hypergraph: pattern matches by label prefix");
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "add", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "subtract", 0.9f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "add_complex", 0.8f);

    /* Pattern: find FUNCTION vertices starting with "add" */
    nimcp_hypergraph_t* pattern = nimcp_hypergraph_create();
    nimcp_hypergraph_add_vertex(pattern, HYPERVERTEX_FUNCTION, "add", 1.0f);

    hypergraph_query_result_t result;
    uint32_t matches = nimcp_hypergraph_pattern_query(hg, pattern, &result, 10);
    ASSERT_TRUE(matches == 2, "should find 2 vertices with label starting with 'add'");

    nimcp_free(result.vertex_ids);
    if (result.edge_ids) nimcp_free(result.edge_ids);
    nimcp_hypergraph_destroy(pattern);
    nimcp_hypergraph_destroy(hg);
    PASS();
}

static void test_hg_pattern_max_matches(void) {
    TEST("Hypergraph: max_matches limits results");
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    for (int i = 0; i < 10; i++) {
        nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "", 0.5f);
    }

    nimcp_hypergraph_t* pattern = nimcp_hypergraph_create();
    nimcp_hypergraph_add_vertex(pattern, HYPERVERTEX_VARIABLE, "", 1.0f);

    hypergraph_query_result_t result;
    uint32_t matches = nimcp_hypergraph_pattern_query(hg, pattern, &result, 3);
    ASSERT_TRUE(matches == 3, "should be limited to 3 matches");

    nimcp_free(result.vertex_ids);
    nimcp_hypergraph_destroy(pattern);
    nimcp_hypergraph_destroy(hg);
    PASS();
}

static void test_hg_pattern_null_safety(void) {
    TEST("Hypergraph: pattern query NULL safety");
    hypergraph_query_result_t result;
    uint32_t m = nimcp_hypergraph_pattern_query(NULL, NULL, &result, 10);
    ASSERT_TRUE(m == 0, "NULL inputs should return 0");
    PASS();
}

/* =========================================================================
 * Hypergraph from Knowledge Base Tests
 * ========================================================================= */

static void test_hg_from_kb(void) {
    TEST("Hypergraph: from_knowledge_base creates graph");
    /* Pass a non-NULL dummy pointer (the function takes const void*) */
    int dummy_kb = 1;
    nimcp_hypergraph_t* hg = nimcp_hypergraph_from_knowledge_base(&dummy_kb);
    ASSERT_TRUE(hg != NULL, "should create hypergraph");
    ASSERT_TRUE(nimcp_hypergraph_vertex_count(hg) >= 1, "should have root vertex");
    nimcp_hypergraph_destroy(hg);
    PASS();
}

static void test_hg_from_kb_null(void) {
    TEST("Hypergraph: from_knowledge_base rejects NULL");
    nimcp_hypergraph_t* hg = nimcp_hypergraph_from_knowledge_base(NULL);
    ASSERT_TRUE(hg == NULL, "NULL logic should return NULL");
    PASS();
}

/* =========================================================================
 * Creative Orchestrator Tests
 * ========================================================================= */

static void test_orch_create_destroy(void) {
    TEST("Orchestrator: create and destroy");
    creative_orchestrator_t* orch = creative_orchestrator_create(NULL);
    ASSERT_TRUE(orch != NULL, "orchestrator NULL");
    ASSERT_TRUE(creative_orchestrator_get_state(orch) == CREATIVE_STATE_READY,
                "should be READY after create");
    creative_orchestrator_destroy(orch);
    PASS();
}

static void test_orch_init_subsystems(void) {
    TEST("Orchestrator: init_subsystems creates components");
    creative_orchestrator_t* orch = creative_orchestrator_create(NULL);
    ASSERT_TRUE(orch != NULL, "orchestrator NULL");

    int rc = creative_orchestrator_init_subsystems(orch);
    /* May return -1 if some subsystems fail to init, but should not crash */
    ASSERT_TRUE(rc == 0 || rc == -1, "should return 0 or -1");

    /* Check that at least text generator was created */
    ASSERT_TRUE(orch->text_gen != NULL, "text_gen should be initialized");

    /* State should be READY after init */
    ASSERT_TRUE(creative_orchestrator_get_state(orch) == CREATIVE_STATE_READY,
                "should be READY after init");

    creative_orchestrator_destroy(orch);
    PASS();
}

static void test_orch_init_idempotent(void) {
    TEST("Orchestrator: init_subsystems is idempotent");
    creative_orchestrator_t* orch = creative_orchestrator_create(NULL);

    /* Call twice — should not double-allocate or crash */
    creative_orchestrator_init_subsystems(orch);
    int rc = creative_orchestrator_init_subsystems(orch);
    ASSERT_TRUE(rc == 0 || rc == -1, "second call should not crash");

    creative_orchestrator_destroy(orch);
    PASS();
}

static void test_orch_update(void) {
    TEST("Orchestrator: update increments cycles");
    creative_orchestrator_t* orch = creative_orchestrator_create(NULL);

    creative_orchestrator_update(orch, 1000);
    creative_orchestrator_update(orch, 2000);

    creative_orchestrator_stats_t stats;
    int rc = creative_orchestrator_get_stats(orch, &stats);
    ASSERT_TRUE(rc == 0, "get_stats failed");
    ASSERT_TRUE(stats.update_cycles == 2, "should have 2 update cycles");

    creative_orchestrator_destroy(orch);
    PASS();
}

static void test_orch_null_safety(void) {
    TEST("Orchestrator: NULL safety");
    ASSERT_TRUE(creative_orchestrator_init_subsystems(NULL) == -1, "NULL init");
    ASSERT_TRUE(creative_orchestrator_update(NULL, 0) == -1, "NULL update");
    ASSERT_TRUE(creative_orchestrator_get_state(NULL) == CREATIVE_STATE_UNINITIALIZED,
                "NULL state");
    PASS();
}

/* =========================================================================
 * Executive Controller Tests
 * ========================================================================= */

static void test_exec_create_destroy(void) {
    TEST("Executive: create and destroy");
    executive_controller_t* exec = executive_create();
    ASSERT_TRUE(exec != NULL, "executive NULL");
    executive_destroy(exec);
    PASS();
}

static void test_exec_add_task(void) {
    TEST("Executive: add and count tasks");
    executive_controller_t* exec = executive_create();
    ASSERT_TRUE(exec != NULL, "executive NULL");

    task_descriptor_t task;
    memset(&task, 0, sizeof(task));
    task.type = TASK_TYPE_REASONING;
    task.priority = PRIORITY_NORMAL;
    strncpy(task.name, "test_task", sizeof(task.name) - 1);

    uint32_t id = executive_add_task(exec, &task);
    ASSERT_TRUE(id > 0, "task ID should be positive");

    executive_destroy(exec);
    PASS();
}

static void test_exec_stats(void) {
    TEST("Executive: get and reset stats");
    executive_controller_t* exec = executive_create();

    executive_stats_t stats;
    bool ok = executive_get_stats(exec, &stats);
    ASSERT_TRUE(ok, "get_stats failed");
    ASSERT_TRUE(stats.total_tasks == 0, "initial total_tasks should be 0");

    executive_reset_stats(exec);
    ok = executive_get_stats(exec, &stats);
    ASSERT_TRUE(ok, "get_stats after reset failed");

    executive_destroy(exec);
    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 3: Executive, Hypergraph, Orchestrator ===\n\n");

    printf("--- Hypergraph Pattern Matching ---\n");
    test_hg_pattern_empty();
    test_hg_pattern_type_match();
    test_hg_pattern_label_match();
    test_hg_pattern_max_matches();
    test_hg_pattern_null_safety();

    printf("\n--- Hypergraph from KB ---\n");
    test_hg_from_kb();
    test_hg_from_kb_null();

    printf("\n--- Creative Orchestrator ---\n");
    test_orch_create_destroy();
    test_orch_init_subsystems();
    test_orch_init_idempotent();
    test_orch_update();
    test_orch_null_safety();

    printf("\n--- Executive Controller ---\n");
    test_exec_create_destroy();
    test_exec_add_task();
    test_exec_stats();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
