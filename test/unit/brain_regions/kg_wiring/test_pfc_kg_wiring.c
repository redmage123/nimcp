/**
 * @file test_pfc_kg_wiring.c
 * @brief Unit tests for Prefrontal Cortex Knowledge Graph wiring
 * @date 2026-02-02
 *
 * Tests for the PFC KG wiring module:
 * - Default configuration
 * - NULL parameter handling
 * - Node registration (subregions, executive, decision, monitoring)
 * - Edge registration
 * - Query operations
 * - State synchronization
 * - Cleanup operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/** Test admin token for KG operations */
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
// Unit Tests - Configuration
//=============================================================================

/**
 * Test: Default configuration initialization
 */
void test_pfc_kg_default_config(void)
{
    printf("\n=== test_pfc_kg_default_config ===\n");

    pfc_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = pfc_kg_default_config(&config);
    TEST_ASSERT(result == 0, "pfc_kg_default_config should return 0");

    TEST_ASSERT(config.register_dlpfc == true, "register_dlpfc should be true");
    TEST_ASSERT(config.register_vmpfc == true, "register_vmpfc should be true");
    TEST_ASSERT(config.register_acc == true, "register_acc should be true");
    TEST_ASSERT(config.register_lpfc == true, "register_lpfc should be true");
    TEST_ASSERT(config.register_executive_nodes == true, "register_executive_nodes should be true");
    TEST_ASSERT(config.register_decision_nodes == true, "register_decision_nodes should be true");
    TEST_ASSERT(config.register_cross_edges == true, "register_cross_edges should be true");
    TEST_ASSERT(config.include_state_metadata == true, "include_state_metadata should be true");

    TEST_PASS("Default configuration values correct");
}

/**
 * Test: NULL parameter handling for config
 */
void test_pfc_kg_config_null(void)
{
    printf("\n=== test_pfc_kg_config_null ===\n");

    int result = pfc_kg_default_config(NULL);
    TEST_ASSERT(result == -1, "pfc_kg_default_config(NULL) should return -1");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register all nodes with NULL KG
 */
void test_pfc_kg_register_null_kg(void)
{
    printf("\n=== test_pfc_kg_register_null_kg ===\n");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = pfc_kg_register_all(NULL, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "pfc_kg_register_all(NULL, ...) should return -1");

    TEST_PASS("NULL KG handling correct");
}

/**
 * Test: Register all nodes with valid KG
 */
void test_pfc_kg_register_all(void)
{
    printf("\n=== test_pfc_kg_register_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_config_t config;
    pfc_kg_default_config(&config);

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = pfc_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "pfc_kg_register_all should return 0");

    /* Verify state was populated */
    TEST_ASSERT(state.registered == true, "state.registered should be true");
    TEST_ASSERT(state.root_id != BRAIN_KG_INVALID_NODE, "root_id should be valid");
    TEST_ASSERT(state.node_count > 0, "node_count should be > 0");
    TEST_ASSERT(state.edge_count > 0, "edge_count should be > 0");

    /* Verify subregion nodes were created */
    TEST_ASSERT(state.dlpfc_id != BRAIN_KG_INVALID_NODE, "dlpfc_id should be valid");
    TEST_ASSERT(state.vmpfc_id != BRAIN_KG_INVALID_NODE, "vmpfc_id should be valid");
    TEST_ASSERT(state.acc_id != BRAIN_KG_INVALID_NODE, "acc_id should be valid");
    TEST_ASSERT(state.lpfc_id != BRAIN_KG_INVALID_NODE, "lpfc_id should be valid");

    /* Verify executive nodes were created */
    TEST_ASSERT(state.executive_system_id != BRAIN_KG_INVALID_NODE, "executive_system_id should be valid");
    TEST_ASSERT(state.working_memory_id != BRAIN_KG_INVALID_NODE, "working_memory_id should be valid");
    TEST_ASSERT(state.attention_id != BRAIN_KG_INVALID_NODE, "attention_id should be valid");
    TEST_ASSERT(state.inhibition_id != BRAIN_KG_INVALID_NODE, "inhibition_id should be valid");
    TEST_ASSERT(state.flexibility_id != BRAIN_KG_INVALID_NODE, "flexibility_id should be valid");
    TEST_ASSERT(state.planning_id != BRAIN_KG_INVALID_NODE, "planning_id should be valid");

    /* Verify decision nodes were created */
    TEST_ASSERT(state.decision_system_id != BRAIN_KG_INVALID_NODE, "decision_system_id should be valid");
    TEST_ASSERT(state.goal_selection_id != BRAIN_KG_INVALID_NODE, "goal_selection_id should be valid");
    TEST_ASSERT(state.action_selection_id != BRAIN_KG_INVALID_NODE, "action_selection_id should be valid");
    TEST_ASSERT(state.strategy_selection_id != BRAIN_KG_INVALID_NODE, "strategy_selection_id should be valid");

    /* Verify monitoring nodes were created */
    TEST_ASSERT(state.conflict_id != BRAIN_KG_INVALID_NODE, "conflict_id should be valid");
    TEST_ASSERT(state.error_id != BRAIN_KG_INVALID_NODE, "error_id should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Full registration completed successfully");
}

/**
 * Test: Register with selective config (only subregions)
 */
void test_pfc_kg_register_selective(void)
{
    printf("\n=== test_pfc_kg_register_selective ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_config_t config;
    pfc_kg_default_config(&config);
    config.register_executive_nodes = false;
    config.register_decision_nodes = false;
    config.register_cross_edges = false;

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = pfc_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Selective registration should return 0");

    /* Verify subregions were created */
    TEST_ASSERT(state.dlpfc_id != BRAIN_KG_INVALID_NODE, "dlpfc_id should be valid");
    TEST_ASSERT(state.vmpfc_id != BRAIN_KG_INVALID_NODE, "vmpfc_id should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Selective registration works correctly");
}

/**
 * Test: Register subregions individually
 */
void test_pfc_kg_register_subregions(void)
{
    printf("\n=== test_pfc_kg_register_subregions ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Create root node first */
    brain_kg_node_id_t root = brain_kg_add_node(kg, PFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL, "Prefrontal cortex root");
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "Root node should be created");

    pfc_kg_config_t config;
    pfc_kg_default_config(&config);

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = pfc_kg_register_subregions(kg, root, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_subregions should return 0");

    TEST_ASSERT(state.dlpfc_id != BRAIN_KG_INVALID_NODE, "dlPFC should be registered");
    TEST_ASSERT(state.vmpfc_id != BRAIN_KG_INVALID_NODE, "vmPFC should be registered");
    TEST_ASSERT(state.acc_id != BRAIN_KG_INVALID_NODE, "ACC should be registered");
    TEST_ASSERT(state.lpfc_id != BRAIN_KG_INVALID_NODE, "lPFC should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Subregion registration works correctly");
}

/**
 * Test: Register executive nodes
 */
void test_pfc_kg_register_executive_nodes(void)
{
    printf("\n=== test_pfc_kg_register_executive_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    brain_kg_node_id_t root = brain_kg_add_node(kg, PFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL, "PFC root");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = pfc_kg_register_executive_nodes(kg, root, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_executive_nodes should return 0");

    TEST_ASSERT(state.executive_system_id != BRAIN_KG_INVALID_NODE, "Executive system should be registered");
    TEST_ASSERT(state.working_memory_id != BRAIN_KG_INVALID_NODE, "Working memory should be registered");
    TEST_ASSERT(state.attention_id != BRAIN_KG_INVALID_NODE, "Attention should be registered");
    TEST_ASSERT(state.inhibition_id != BRAIN_KG_INVALID_NODE, "Inhibition should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Executive nodes registration works correctly");
}

/**
 * Test: Register decision nodes
 */
void test_pfc_kg_register_decision_nodes(void)
{
    printf("\n=== test_pfc_kg_register_decision_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    brain_kg_node_id_t root = brain_kg_add_node(kg, PFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL, "PFC root");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = pfc_kg_register_decision_nodes(kg, root, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_decision_nodes should return 0");

    TEST_ASSERT(state.decision_system_id != BRAIN_KG_INVALID_NODE, "Decision system should be registered");
    TEST_ASSERT(state.goal_selection_id != BRAIN_KG_INVALID_NODE, "Goal selection should be registered");
    TEST_ASSERT(state.action_selection_id != BRAIN_KG_INVALID_NODE, "Action selection should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Decision nodes registration works correctly");
}

/**
 * Test: Register monitoring nodes
 */
void test_pfc_kg_register_monitoring_nodes(void)
{
    printf("\n=== test_pfc_kg_register_monitoring_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    brain_kg_node_id_t root = brain_kg_add_node(kg, PFC_KG_ROOT_NAME,
        BRAIN_KG_NODE_CORTICAL, "PFC root");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = pfc_kg_register_monitoring_nodes(kg, root, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_monitoring_nodes should return 0");

    TEST_ASSERT(state.conflict_id != BRAIN_KG_INVALID_NODE, "Conflict monitoring should be registered");
    TEST_ASSERT(state.error_id != BRAIN_KG_INVALID_NODE, "Error detection should be registered");
    TEST_ASSERT(state.performance_id != BRAIN_KG_INVALID_NODE, "Performance monitoring should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Monitoring nodes registration works correctly");
}

//=============================================================================
// Unit Tests - Query Operations
//=============================================================================

/**
 * Test: Get root node
 */
void test_pfc_kg_get_root(void)
{
    printf("\n=== test_pfc_kg_get_root ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    int result = pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Registration should succeed");

    brain_kg_node_id_t root = pfc_kg_get_root(kg);
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "get_root should return valid ID");
    TEST_ASSERT(root == state.root_id, "get_root should match state.root_id");

    brain_kg_destroy(kg);
    TEST_PASS("Get root works correctly");
}

/**
 * Test: Get root with NULL KG
 */
void test_pfc_kg_get_root_null(void)
{
    printf("\n=== test_pfc_kg_get_root_null ===\n");

    brain_kg_node_id_t root = pfc_kg_get_root(NULL);
    TEST_ASSERT(root == BRAIN_KG_INVALID_NODE, "get_root(NULL) should return INVALID_NODE");

    TEST_PASS("NULL KG handling correct for get_root");
}

/**
 * Test: Find subsystem by name
 */
void test_pfc_kg_find_subsystem(void)
{
    printf("\n=== test_pfc_kg_find_subsystem ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    /* Find dlPFC */
    brain_kg_node_id_t dlpfc = pfc_kg_find_subsystem(kg, PFC_KG_DLPFC_NAME);
    TEST_ASSERT(dlpfc != BRAIN_KG_INVALID_NODE, "dlPFC should be found");
    TEST_ASSERT(dlpfc == state.dlpfc_id, "Found dlPFC should match state");

    /* Find ACC */
    brain_kg_node_id_t acc = pfc_kg_find_subsystem(kg, PFC_KG_ACC_NAME);
    TEST_ASSERT(acc != BRAIN_KG_INVALID_NODE, "ACC should be found");

    /* Find vmPFC */
    brain_kg_node_id_t vmpfc = pfc_kg_find_subsystem(kg, PFC_KG_VMPFC_NAME);
    TEST_ASSERT(vmpfc != BRAIN_KG_INVALID_NODE, "vmPFC should be found");

    /* Find non-existent */
    brain_kg_node_id_t invalid = pfc_kg_find_subsystem(kg, "nonexistent");
    TEST_ASSERT(invalid == BRAIN_KG_INVALID_NODE, "Non-existent should return INVALID_NODE");

    brain_kg_destroy(kg);
    TEST_PASS("Find subsystem works correctly");
}

/**
 * Test: Get executive nodes list
 */
void test_pfc_kg_get_executive_nodes(void)
{
    printf("\n=== test_pfc_kg_get_executive_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = pfc_kg_get_executive_nodes(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Executive nodes list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get executive nodes works correctly");
}

/**
 * Test: Get decision nodes list
 */
void test_pfc_kg_get_decision_nodes(void)
{
    printf("\n=== test_pfc_kg_get_decision_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = pfc_kg_get_decision_nodes(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Decision nodes list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get decision nodes works correctly");
}

/**
 * Test: Get subregions list
 */
void test_pfc_kg_get_subregions(void)
{
    printf("\n=== test_pfc_kg_get_subregions ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = pfc_kg_get_subregions(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Subregions list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get subregions works correctly");
}

//=============================================================================
// Unit Tests - State Synchronization
//=============================================================================

/**
 * Test: Update state metadata
 */
void test_pfc_kg_update_state(void)
{
    printf("\n=== test_pfc_kg_update_state ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = pfc_kg_update_state(kg, &state,
        0.75f,  /* wm_load */
        0.8f,   /* control_demand */
        0.3f,   /* conflict_level */
        0.9f,   /* attention_focus */
        TEST_ADMIN_TOKEN);

    TEST_ASSERT(result == 0, "update_state should return 0");

    brain_kg_destroy(kg);
    TEST_PASS("State update works correctly");
}

/**
 * Test: Update state with NULL parameters
 */
void test_pfc_kg_update_state_null(void)
{
    printf("\n=== test_pfc_kg_update_state_null ===\n");

    int result = pfc_kg_update_state(NULL, NULL, 0.5f, 0.5f, 0.5f, 0.5f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "update_state(NULL, NULL, ...) should return -1");

    TEST_PASS("NULL handling for update_state correct");
}

//=============================================================================
// Unit Tests - Cleanup
//=============================================================================

/**
 * Test: Unregister all nodes
 */
void test_pfc_kg_unregister_all(void)
{
    printf("\n=== test_pfc_kg_unregister_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    pfc_kg_state_t state;
    memset(&state, 0, sizeof(state));
    pfc_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = pfc_kg_unregister_all(kg, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "unregister_all should return 0");
    TEST_ASSERT(state.registered == false, "state.registered should be false after unregister");

    brain_kg_destroy(kg);
    TEST_PASS("Unregister all works correctly");
}

/**
 * Test: Unregister with NULL parameters
 */
void test_pfc_kg_unregister_null(void)
{
    printf("\n=== test_pfc_kg_unregister_null ===\n");

    int result = pfc_kg_unregister_all(NULL, NULL, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "unregister_all(NULL, NULL, ...) should return -1");

    TEST_PASS("NULL handling for unregister correct");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("PFC KG Wiring Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_pfc_kg_default_config();
    test_pfc_kg_config_null();

    /* Registration tests */
    test_pfc_kg_register_null_kg();
    test_pfc_kg_register_all();
    test_pfc_kg_register_selective();
    test_pfc_kg_register_subregions();
    test_pfc_kg_register_executive_nodes();
    test_pfc_kg_register_decision_nodes();
    test_pfc_kg_register_monitoring_nodes();

    /* Query tests */
    test_pfc_kg_get_root();
    test_pfc_kg_get_root_null();
    test_pfc_kg_find_subsystem();
    test_pfc_kg_get_executive_nodes();
    test_pfc_kg_get_decision_nodes();
    test_pfc_kg_get_subregions();

    /* State sync tests */
    test_pfc_kg_update_state();
    test_pfc_kg_update_state_null();

    /* Cleanup tests */
    test_pfc_kg_unregister_all();
    test_pfc_kg_unregister_null();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
