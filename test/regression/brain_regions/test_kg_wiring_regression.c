/**
 * @file test_kg_wiring_regression.c
 * @brief Regression tests for brain region KG wiring modules
 * @date 2026-02-02
 *
 * Tests for previously fixed issues and edge cases:
 * - API stability tests
 * - Node ID consistency
 * - Edge weight bounds
 * - State transition validity
 * - Memory leak prevention patterns
 * - Constants definition verification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
#include "core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.h"
#include "core/brain/nimcp_brain_kg.h"
#include "nimcp.h"

//=============================================================================
// Test Helpers
//=============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("  PASS: %s\n", msg); \
    g_tests_passed++; \
} while(0)

#define TEST_ADMIN_TOKEN 0x12345678ULL

/**
 * @brief Create a KG with security disabled for testing
 * @return KG handle with access control disabled
 */
static brain_kg_t* create_test_kg(void)
{
    brain_kg_config_t config;
    brain_kg_default_config(&config);
    config.enable_security = false;
    config.enable_access_control = false;
    config.enable_immune_integration = false;
    return brain_kg_create(&config);
}

//=============================================================================
// Regression Tests - API Stability
//=============================================================================

/**
 * Regression: Verify BRAIN_KG_INVALID_NODE constant is used correctly
 * (Previously there was confusion with BRAIN_KG_INVALID_NODE_ID)
 */
void test_invalid_node_constant(void)
{
    printf("\n=== test_invalid_node_constant ===\n");

    /* Verify the constant exists and has expected value */
    TEST_ASSERT(BRAIN_KG_INVALID_NODE == UINT32_MAX, "BRAIN_KG_INVALID_NODE should be UINT32_MAX");

    /* Verify all get_root functions return this on NULL */
    brain_kg_node_id_t h = hippocampus_kg_get_root(NULL);
    brain_kg_node_id_t a = amygdala_kg_get_root(NULL);
    brain_kg_node_id_t p = pfc_kg_get_root(NULL);

    TEST_ASSERT(h == BRAIN_KG_INVALID_NODE, "Hippocampus get_root(NULL) should return INVALID_NODE");
    TEST_ASSERT(a == BRAIN_KG_INVALID_NODE, "Amygdala get_root(NULL) should return INVALID_NODE");
    TEST_ASSERT(p == BRAIN_KG_INVALID_NODE, "PFC get_root(NULL) should return INVALID_NODE");

    TEST_PASS("BRAIN_KG_INVALID_NODE constant used correctly");
}

/**
 * Regression: Verify module names match expected constants
 */
void test_module_name_constants(void)
{
    printf("\n=== test_module_name_constants ===\n");

    /* Hippocampus constants */
    TEST_ASSERT(strcmp(HIPPOCAMPUS_KG_ROOT_NAME, "hippocampus") == 0, "Hippocampus root name");
    TEST_ASSERT(strcmp(HIPPOCAMPUS_KG_CA1_NAME, "ca1") == 0, "CA1 name");
    TEST_ASSERT(strcmp(HIPPOCAMPUS_KG_CA3_NAME, "ca3") == 0, "CA3 name");
    TEST_ASSERT(strcmp(HIPPOCAMPUS_KG_DG_NAME, "dentate_gyrus") == 0, "DG name");

    /* Amygdala constants */
    TEST_ASSERT(strcmp(AMYGDALA_KG_ROOT_NAME, "amygdala") == 0, "Amygdala root name");
    TEST_ASSERT(strcmp(AMYGDALA_KG_BLA_NAME, "basolateral_complex") == 0, "BLA name");
    TEST_ASSERT(strcmp(AMYGDALA_KG_CEA_NAME, "central_nucleus") == 0, "CeA name");

    /* PFC constants */
    TEST_ASSERT(strcmp(PFC_KG_ROOT_NAME, "prefrontal_cortex") == 0, "PFC root name");
    TEST_ASSERT(strcmp(PFC_KG_DLPFC_NAME, "dorsolateral_pfc") == 0, "dlPFC name");
    TEST_ASSERT(strcmp(PFC_KG_ACC_NAME, "anterior_cingulate") == 0, "ACC name");

    TEST_PASS("Module name constants are correct");
}

/**
 * Regression: Verify node type ranges don't overlap
 */
void test_node_type_ranges(void)
{
    printf("\n=== test_node_type_ranges ===\n");

    /* Hippocampus types start at 0x2100 */
    TEST_ASSERT(HIPPOCAMPUS_KG_NODE_SUBFIELD >= 0x2100, "Hippocampus types start at 0x2100");
    TEST_ASSERT(HIPPOCAMPUS_KG_NODE_THETA_RHYTHM < 0x2200, "Hippocampus types end before 0x2200");

    /* Amygdala types start at 0x2200 */
    TEST_ASSERT(AMYGDALA_KG_NODE_NUCLEUS >= 0x2200, "Amygdala types start at 0x2200");
    TEST_ASSERT(AMYGDALA_KG_NODE_AROUSAL_STATE < 0x2300, "Amygdala types end before 0x2300");

    /* PFC types start at 0x2300 */
    TEST_ASSERT(PFC_KG_NODE_SUBREGION >= 0x2300, "PFC types start at 0x2300");
    TEST_ASSERT(PFC_KG_NODE_ATTENTION_COMPONENT < 0x2400, "PFC types end before 0x2400");

    TEST_PASS("Node type ranges don't overlap");
}

/**
 * Regression: Verify edge type ranges don't overlap
 */
void test_edge_type_ranges(void)
{
    printf("\n=== test_edge_type_ranges ===\n");

    /* Hippocampus edge types start at 0x2100 */
    TEST_ASSERT(HIPPOCAMPUS_KG_EDGE_TRIGGERS >= 0x2100, "Hippocampus edges start at 0x2100");
    TEST_ASSERT(HIPPOCAMPUS_KG_EDGE_RECALLS < 0x2200, "Hippocampus edges end before 0x2200");

    /* Amygdala edge types start at 0x2200 */
    TEST_ASSERT(AMYGDALA_KG_EDGE_TRIGGERS_FEAR >= 0x2200, "Amygdala edges start at 0x2200");
    TEST_ASSERT(AMYGDALA_KG_EDGE_INFLUENCES_SOCIAL < 0x2300, "Amygdala edges end before 0x2300");

    /* PFC edge types start at 0x2300 */
    TEST_ASSERT(PFC_KG_EDGE_INFORMS >= 0x2300, "PFC edges start at 0x2300");
    TEST_ASSERT(PFC_KG_EDGE_SEQUENCES < 0x2400, "PFC edges end before 0x2400");

    TEST_PASS("Edge type ranges don't overlap");
}

//=============================================================================
// Regression Tests - State Consistency
//=============================================================================

/**
 * Regression: Node IDs remain consistent after state updates
 */
void test_node_id_consistency(void)
{
    printf("\n=== test_node_id_consistency ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_id_t original_root = state.root_id;
    brain_kg_node_id_t original_ca1 = state.ca1_id;

    /* Update state multiple times */
    for (int i = 0; i < 10; i++) {
        hippocampus_kg_update_state(kg, &state,
            (float)i / 10.0f, 0.5f, 0.5f, 0.5f, TEST_ADMIN_TOKEN);
    }

    /* Verify IDs haven't changed */
    TEST_ASSERT(state.root_id == original_root, "Root ID should remain consistent");
    TEST_ASSERT(state.ca1_id == original_ca1, "CA1 ID should remain consistent");

    brain_kg_destroy(kg);
    TEST_PASS("Node IDs remain consistent after state updates");
}

/**
 * Regression: State counters are accurate
 */
void test_state_counter_accuracy(void)
{
    printf("\n=== test_state_counter_accuracy ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    /* Count non-invalid node IDs in state */
    uint32_t counted_nodes = 0;
    if (state.root_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.ca1_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.ca3_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.dg_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.subiculum_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.ec_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.memory_system_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.episodic_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.spatial_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.working_buffer_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.encoding_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.retrieval_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.consolidation_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.pattern_sep_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.pattern_comp_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.nav_system_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.place_cells_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.grid_cells_id != BRAIN_KG_INVALID_NODE) counted_nodes++;
    if (state.head_dir_id != BRAIN_KG_INVALID_NODE) counted_nodes++;

    /* State counter should match */
    TEST_ASSERT(state.node_count == counted_nodes, "node_count should match actual valid nodes");

    brain_kg_destroy(kg);
    TEST_PASS("State counters are accurate");
}

/**
 * Regression: Registered flag transitions correctly
 */
void test_registered_flag_transitions(void)
{
    printf("\n=== test_registered_flag_transitions ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    /* Initially unregistered */
    TEST_ASSERT(state.registered == false, "Should start unregistered");

    /* Register */
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(state.registered == true, "Should be registered after register_all");

    /* Unregister */
    hippocampus_kg_unregister_all(kg, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(state.registered == false, "Should be unregistered after unregister_all");

    brain_kg_destroy(kg);
    TEST_PASS("Registered flag transitions correctly");
}

//=============================================================================
// Regression Tests - Error Handling
//=============================================================================

/**
 * Regression: All NULL parameter checks return -1
 */
void test_null_parameter_returns(void)
{
    printf("\n=== test_null_parameter_returns ===\n");

    /* Config functions */
    TEST_ASSERT(hippocampus_kg_default_config(NULL) == -1, "hippocampus config NULL");
    TEST_ASSERT(amygdala_kg_default_config(NULL) == -1, "amygdala config NULL");
    TEST_ASSERT(pfc_kg_default_config(NULL) == -1, "pfc config NULL");

    /* Register functions */
    TEST_ASSERT(hippocampus_kg_register_all(NULL, NULL, NULL, 0) == -1, "hippocampus register NULL");
    TEST_ASSERT(amygdala_kg_register_all(NULL, NULL, NULL, 0) == -1, "amygdala register NULL");
    TEST_ASSERT(pfc_kg_register_all(NULL, NULL, NULL, 0) == -1, "pfc register NULL");

    /* Update state functions */
    TEST_ASSERT(hippocampus_kg_update_state(NULL, NULL, 0, 0, 0, 0, 0) == -1, "hippocampus update NULL");
    TEST_ASSERT(amygdala_kg_update_state(NULL, NULL, 0, 0, 0, 0, 0) == -1, "amygdala update NULL");
    TEST_ASSERT(pfc_kg_update_state(NULL, NULL, 0, 0, 0, 0, 0) == -1, "pfc update NULL");

    /* Unregister functions */
    TEST_ASSERT(hippocampus_kg_unregister_all(NULL, NULL, 0) == -1, "hippocampus unregister NULL");
    TEST_ASSERT(amygdala_kg_unregister_all(NULL, NULL, 0) == -1, "amygdala unregister NULL");
    TEST_ASSERT(pfc_kg_unregister_all(NULL, NULL, 0) == -1, "pfc unregister NULL");

    TEST_PASS("All NULL parameter checks return -1");
}

/**
 * Regression: Find subsystem with NULL name returns INVALID_NODE
 */
void test_find_null_name(void)
{
    printf("\n=== test_find_null_name ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_id_t result = hippocampus_kg_find_subsystem(kg, NULL);
    TEST_ASSERT(result == BRAIN_KG_INVALID_NODE, "find_subsystem(kg, NULL) should return INVALID_NODE");

    brain_kg_destroy(kg);
    TEST_PASS("Find subsystem with NULL name returns INVALID_NODE");
}

/**
 * Regression: Empty string name returns INVALID_NODE
 */
void test_find_empty_name(void)
{
    printf("\n=== test_find_empty_name ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_id_t result = hippocampus_kg_find_subsystem(kg, "");
    TEST_ASSERT(result == BRAIN_KG_INVALID_NODE, "find_subsystem(kg, \"\") should return INVALID_NODE");

    brain_kg_destroy(kg);
    TEST_PASS("Find subsystem with empty name returns INVALID_NODE");
}

//=============================================================================
// Regression Tests - Bounds Checking
//=============================================================================

/**
 * Regression: State update with extreme float values
 */
void test_extreme_float_values(void)
{
    printf("\n=== test_extreme_float_values ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    /* Test with 0 values */
    int r1 = hippocampus_kg_update_state(kg, &state, 0.0f, 0.0f, 0.0f, 0.0f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r1 == 0, "Update with 0.0 values should succeed");

    /* Test with 1 values */
    int r2 = hippocampus_kg_update_state(kg, &state, 1.0f, 1.0f, 1.0f, 1.0f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r2 == 0, "Update with 1.0 values should succeed");

    /* Test with values outside normal range (implementation may clamp or reject) */
    int r3 = hippocampus_kg_update_state(kg, &state, -0.5f, 1.5f, -1.0f, 2.0f, TEST_ADMIN_TOKEN);
    /* Either succeed with clamping or fail - both are valid */
    TEST_ASSERT(r3 == 0 || r3 == -1, "Update with out-of-range values should return 0 or -1");

    brain_kg_destroy(kg);
    TEST_PASS("Extreme float values handled correctly");
}

/**
 * Regression: Config with all false flags
 */
void test_all_false_config(void)
{
    printf("\n=== test_all_false_config ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_config_t config;
    memset(&config, 0, sizeof(config));  /* All false */

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    /* Should still succeed, just register fewer nodes */
    TEST_ASSERT(result == 0, "Registration with all-false config should succeed");

    brain_kg_destroy(kg);
    TEST_PASS("All-false config handled correctly");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("KG Wiring Regression Tests\n");
    printf("============================================\n");

    /* API stability tests */
    test_invalid_node_constant();
    test_module_name_constants();
    test_node_type_ranges();
    test_edge_type_ranges();

    /* State consistency tests */
    test_node_id_consistency();
    test_state_counter_accuracy();
    test_registered_flag_transitions();

    /* Error handling tests */
    test_null_parameter_returns();
    test_find_null_name();
    test_find_empty_name();

    /* Bounds checking tests */
    test_extreme_float_values();
    test_all_false_config();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
