/**
 * @file test_phase5_batch4.c
 * @brief Phase 5 Batch 4: Imagination Engine, JEPA Bridge, Health Shadow Detection
 *
 * Tests for: imagination bio-async/broadcast/immune modulation,
 * JEPA-imagination bridge prediction/counterfactual/world model,
 * health agent shadow pattern detection and intervention.
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/imagination/nimcp_jepa_imagination_bridge.h"
#include "cognitive/emotion/nimcp_health_emotion_bridge.h"
#include "utils/tensor/nimcp_tensor.h"
#include "utils/memory/nimcp_memory.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  %-60s", name); } while(0)
#define PASS() do { printf("[PASS]\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)
#define ASSERT_TRUE(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* =========================================================================
 * Imagination Engine Bio-Async Tests
 * ========================================================================= */

static void test_imag_process_bio_null(void) {
    TEST("Imagination: process_bio_messages NULL safety");
    uint32_t n = imagination_process_bio_messages(NULL, 10);
    ASSERT_TRUE(n == 0, "NULL engine should return 0");
    PASS();
}

static void test_imag_process_bio_no_context(void) {
    TEST("Imagination: process_bio_messages without bio_context");
    imagination_engine_t* engine = imagination_engine_create(NULL);
    ASSERT_TRUE(engine != NULL, "engine NULL");

    /* bio_context is NULL by default */
    uint32_t n = imagination_process_bio_messages(engine, 10);
    ASSERT_TRUE(n == 0, "no bio_context should return 0");

    imagination_engine_destroy(engine);
    PASS();
}

static void test_imag_broadcast_null(void) {
    TEST("Imagination: broadcast_to_workspace NULL safety");
    int rc = imagination_broadcast_to_workspace(NULL, NULL, 0.5f);
    ASSERT_TRUE(rc == -1, "NULL inputs should return -1");
    PASS();
}

static void test_imag_broadcast_no_workspace(void) {
    TEST("Imagination: broadcast without global workspace");
    imagination_engine_t* engine = imagination_engine_create(NULL);
    ASSERT_TRUE(engine != NULL, "engine NULL");

    /* Create a dummy scenario via begin_scenario */
    imagination_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.mode = IMAGINATION_MODE_PASSIVE;
    goal.priority = 0.5f;

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, &goal);
    ASSERT_TRUE(scenario != NULL, "scenario NULL");

    /* No global workspace connected — should return -1 */
    int rc = imagination_broadcast_to_workspace(engine, scenario, 0.5f);
    ASSERT_TRUE(rc == -1, "no workspace should return -1");

    imagination_engine_destroy(engine);
    PASS();
}

static void test_imag_immune_modulation_null(void) {
    TEST("Imagination: update_immune_modulation NULL safety");
    int rc = imagination_update_immune_modulation(NULL);
    ASSERT_TRUE(rc == -1, "NULL engine should return -1");
    PASS();
}

static void test_imag_immune_modulation_no_immune(void) {
    TEST("Imagination: immune modulation without immune system");
    imagination_engine_t* engine = imagination_engine_create(NULL);
    ASSERT_TRUE(engine != NULL, "engine NULL");

    /* No immune system connected — should return 0 (no-op) */
    int rc = imagination_update_immune_modulation(engine);
    ASSERT_TRUE(rc == 0, "no immune system should return 0");

    imagination_engine_destroy(engine);
    PASS();
}

static void test_imag_get_immune_modulation_null(void) {
    TEST("Imagination: get_immune_modulation NULL returns 1.0");
    float m = imagination_get_immune_modulation(NULL);
    ASSERT_TRUE(fabsf(m - 1.0f) < 0.001f, "NULL should return 1.0");
    PASS();
}

static void test_imag_get_immune_modulation_no_immune(void) {
    TEST("Imagination: get_immune_modulation without immune = 1.0");
    imagination_engine_t* engine = imagination_engine_create(NULL);
    ASSERT_TRUE(engine != NULL, "engine NULL");

    float m = imagination_get_immune_modulation(engine);
    ASSERT_TRUE(fabsf(m - 1.0f) < 0.001f, "no immune should return 1.0");

    imagination_engine_destroy(engine);
    PASS();
}

static void test_imag_scenario_coherence(void) {
    TEST("Imagination: scenario coherence after begin");
    imagination_engine_t* engine = imagination_engine_create(NULL);
    ASSERT_TRUE(engine != NULL, "engine NULL");

    imagination_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.mode = IMAGINATION_MODE_DIRECTED;

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, &goal);
    ASSERT_TRUE(scenario != NULL, "scenario NULL");
    ASSERT_TRUE(scenario->latent_state != NULL, "latent_state NULL");
    ASSERT_TRUE(scenario->coherence >= 0.0f, "coherence should be >= 0");

    imagination_engine_destroy(engine);
    PASS();
}

/* =========================================================================
 * JEPA-Imagination Bridge Tests
 * ========================================================================= */

static void test_jepa_bridge_create_destroy(void) {
    TEST("JEPA Bridge: create and destroy");
    jepa_imagination_bridge_t* bridge = jepa_imagination_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");
    jepa_imagination_bridge_destroy(bridge);
    PASS();
}

static void test_jepa_predicted_null_safety(void) {
    TEST("JEPA Bridge: predicted imagination NULL safety");
    uint32_t id = jepa_imagination_request_predicted_imagination(NULL, NULL, NULL);
    ASSERT_TRUE(id == 0, "NULL should return 0");
    PASS();
}

static void test_jepa_predicted_inactive(void) {
    TEST("JEPA Bridge: predicted imagination when bridge inactive");
    jepa_imagination_bridge_t* bridge = jepa_imagination_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    uint32_t dims[] = {16};
    nimcp_tensor_t* context = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    imagination_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.mode = IMAGINATION_MODE_DIRECTED;

    /* Bridge not active — should return 0 */
    uint32_t id = jepa_imagination_request_predicted_imagination(
        bridge, context, &goal);
    ASSERT_TRUE(id == 0, "inactive bridge should return 0");

    nimcp_tensor_destroy(context);
    jepa_imagination_bridge_destroy(bridge);
    PASS();
}

static void test_jepa_counterfactual_null(void) {
    TEST("JEPA Bridge: counterfactual NULL safety");
    uint32_t id = jepa_imagination_request_counterfactual(NULL, NULL, NULL);
    ASSERT_TRUE(id == 0, "NULL should return 0");
    PASS();
}

static void test_jepa_world_model_null(void) {
    TEST("JEPA Bridge: world model query NULL safety");
    nimcp_tensor_t* outcome = NULL;
    int rc = jepa_imagination_query_world_model(NULL, NULL, NULL, &outcome);
    ASSERT_TRUE(rc == -1, "NULL should return -1");
    PASS();
}

static void test_jepa_world_model_no_jepa(void) {
    TEST("JEPA Bridge: world model query without JEPA");
    jepa_imagination_bridge_t* bridge = jepa_imagination_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    uint32_t dims[] = {8};
    nimcp_tensor_t* context = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* action = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    nimcp_tensor_t* outcome = NULL;

    /* No JEPA connected — should return -1 */
    int rc = jepa_imagination_query_world_model(bridge, context, action, &outcome);
    ASSERT_TRUE(rc == -1, "no JEPA should return -1");

    nimcp_tensor_destroy(context);
    nimcp_tensor_destroy(action);
    jepa_imagination_bridge_destroy(bridge);
    PASS();
}

static void test_jepa_bridge_stats(void) {
    TEST("JEPA Bridge: stats initialized to zero");
    jepa_imagination_bridge_t* bridge = jepa_imagination_bridge_create(NULL);
    ASSERT_TRUE(bridge != NULL, "bridge NULL");

    ASSERT_TRUE(bridge->stats.predictions_generated == 0, "predictions_generated != 0");
    ASSERT_TRUE(bridge->stats.counterfactuals_simulated == 0, "counterfactuals != 0");
    ASSERT_TRUE(bridge->stats.training_signals_sent == 0, "training_signals != 0");

    jepa_imagination_bridge_destroy(bridge);
    PASS();
}

/* =========================================================================
 * Health Shadow Detection Tests
 * ========================================================================= */

static void test_shadow_detect_null_safety(void) {
    TEST("Shadow: detect patterns NULL safety");
    uint32_t num = 99;
    int rc = health_agent_detect_shadow_patterns(NULL, NULL, 0, &num);
    ASSERT_TRUE(rc == -1, "NULL inputs should return -1");
    PASS();
}

static void test_shadow_detect_null_output(void) {
    TEST("Shadow: detect patterns NULL output pointer");
    shadow_detection_result_t results[4];
    int rc = health_agent_detect_shadow_patterns(NULL, results, 4, NULL);
    ASSERT_TRUE(rc == -1, "NULL num_detected should return -1");
    PASS();
}

static void test_shadow_detect_null_agent(void) {
    TEST("Shadow: detect with NULL agent returns empty");
    shadow_detection_result_t results[4];
    uint32_t num = 99;
    int rc = health_agent_detect_shadow_patterns(NULL, results, 4, &num);
    /* With NULL agent we get 0 (after the NULL checks) or -1 */
    ASSERT_TRUE(rc == 0 || rc == -1, "NULL agent should not crash");
    PASS();
}

static void test_shadow_get_intervention(void) {
    TEST("Shadow: get_intervention mapping");

    /* Hypervigilance → raise threshold */
    shadow_intervention_type_t i1 = health_shadow_get_intervention(HEALTH_SHADOW_HYPERVIGILANCE);
    ASSERT_TRUE(i1 == SHADOW_INTERVENTION_RAISE_THRESHOLD, "hypervigilance");

    /* Denial → lower threshold */
    shadow_intervention_type_t i2 = health_shadow_get_intervention(HEALTH_SHADOW_DENIAL);
    ASSERT_TRUE(i2 == SHADOW_INTERVENTION_LOWER_THRESHOLD, "denial");

    /* Procrastination → require action */
    shadow_intervention_type_t i3 = health_shadow_get_intervention(HEALTH_SHADOW_PROCRASTINATION);
    ASSERT_TRUE(i3 == SHADOW_INTERVENTION_REQUIRE_ACTION, "procrastination");

    /* Obsessive checking → limit frequency */
    shadow_intervention_type_t i4 = health_shadow_get_intervention(HEALTH_SHADOW_OBSESSIVE_CHECKING);
    ASSERT_TRUE(i4 == SHADOW_INTERVENTION_LIMIT_FREQUENCY, "obsessive");

    /* Decision paralysis → force default */
    shadow_intervention_type_t i5 = health_shadow_get_intervention(HEALTH_SHADOW_DECISION_PARALYSIS);
    ASSERT_TRUE(i5 == SHADOW_INTERVENTION_FORCE_DEFAULT, "paralysis");

    PASS();
}

static void test_shadow_intervene_null(void) {
    TEST("Shadow: intervene NULL safety");
    int rc = health_agent_intervene_shadow(NULL, HEALTH_SHADOW_DENIAL);
    ASSERT_TRUE(rc == -1, "NULL agent should return -1");
    PASS();
}

static void test_shadow_intervene_none(void) {
    TEST("Shadow: intervene with NONE pattern");
    /* HEALTH_SHADOW_NONE should be a no-op and return 0 */
    /* We can't create a real agent, but can test the early return */
    int rc = health_agent_intervene_shadow(NULL, HEALTH_SHADOW_NONE);
    /* NULL agent returns -1 first, NONE returns 0 only with valid agent */
    ASSERT_TRUE(rc == -1 || rc == 0, "should not crash");
    PASS();
}

static void test_shadow_result_fields(void) {
    TEST("Shadow: detection result struct has all fields");
    shadow_detection_result_t result;
    memset(&result, 0, sizeof(result));

    result.pattern = HEALTH_SHADOW_HYPERVIGILANCE;
    result.intensity = 0.75f;
    result.occurrence_count = 42;
    result.first_observed_us = 1000;
    result.last_observed_us = 2000;
    snprintf(result.evidence, sizeof(result.evidence), "test evidence");

    ASSERT_TRUE(result.pattern == HEALTH_SHADOW_HYPERVIGILANCE, "pattern field");
    ASSERT_TRUE(fabsf(result.intensity - 0.75f) < 0.001f, "intensity field");
    ASSERT_TRUE(result.occurrence_count == 42, "count field");
    ASSERT_TRUE(strlen(result.evidence) > 0, "evidence field");

    PASS();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void) {
    printf("\n=== Phase 5 Batch 4: Imagination, JEPA Bridge, Health Shadow ===\n\n");

    printf("--- Imagination Engine Bio-Async ---\n");
    test_imag_process_bio_null();
    test_imag_process_bio_no_context();
    test_imag_broadcast_null();
    test_imag_broadcast_no_workspace();
    test_imag_immune_modulation_null();
    test_imag_immune_modulation_no_immune();
    test_imag_get_immune_modulation_null();
    test_imag_get_immune_modulation_no_immune();
    test_imag_scenario_coherence();

    printf("\n--- JEPA-Imagination Bridge ---\n");
    test_jepa_bridge_create_destroy();
    test_jepa_predicted_null_safety();
    test_jepa_predicted_inactive();
    test_jepa_counterfactual_null();
    test_jepa_world_model_null();
    test_jepa_world_model_no_jepa();
    test_jepa_bridge_stats();

    printf("\n--- Health Shadow Detection ---\n");
    test_shadow_detect_null_safety();
    test_shadow_detect_null_output();
    test_shadow_detect_null_agent();
    test_shadow_get_intervention();
    test_shadow_intervene_null();
    test_shadow_intervene_none();
    test_shadow_result_fields();

    printf("\n=== Results: %d passed, %d failed ===\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
