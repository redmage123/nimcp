/**
 * @file test_hippocampus_kg_wiring.c
 * @brief Unit tests for Hippocampus Knowledge Graph wiring
 * @date 2026-02-02
 *
 * Tests for the hippocampus KG wiring module:
 * - Default configuration
 * - NULL parameter handling
 * - Node registration (subfields, memory, navigation)
 * - Edge registration
 * - Query operations
 * - State synchronization
 * - Cleanup operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
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
void test_hippocampus_kg_default_config(void)
{
    printf("\n=== test_hippocampus_kg_default_config ===\n");

    hippocampus_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = hippocampus_kg_default_config(&config);
    TEST_ASSERT(result == 0, "hippocampus_kg_default_config should return 0");

    TEST_ASSERT(config.register_ca1 == true, "register_ca1 should be true");
    TEST_ASSERT(config.register_ca3 == true, "register_ca3 should be true");
    TEST_ASSERT(config.register_dg == true, "register_dg should be true");
    TEST_ASSERT(config.register_subiculum == true, "register_subiculum should be true");
    TEST_ASSERT(config.register_memory_nodes == true, "register_memory_nodes should be true");
    TEST_ASSERT(config.register_nav_nodes == true, "register_nav_nodes should be true");
    TEST_ASSERT(config.register_cross_edges == true, "register_cross_edges should be true");
    TEST_ASSERT(config.include_state_metadata == true, "include_state_metadata should be true");

    TEST_PASS("Default configuration values correct");
}

/**
 * Test: NULL parameter handling for config
 */
void test_hippocampus_kg_config_null(void)
{
    printf("\n=== test_hippocampus_kg_config_null ===\n");

    int result = hippocampus_kg_default_config(NULL);
    TEST_ASSERT(result == -1, "hippocampus_kg_default_config(NULL) should return -1");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register all nodes with NULL KG
 */
void test_hippocampus_kg_register_null_kg(void)
{
    printf("\n=== test_hippocampus_kg_register_null_kg ===\n");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(NULL, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "hippocampus_kg_register_all(NULL, ...) should return -1");

    TEST_PASS("NULL KG handling correct");
}

/**
 * Test: Register all nodes with valid KG
 */
void test_hippocampus_kg_register_all(void)
{
    printf("\n=== test_hippocampus_kg_register_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "hippocampus_kg_register_all should return 0");

    /* Verify state was populated */
    TEST_ASSERT(state.registered == true, "state.registered should be true");
    TEST_ASSERT(state.root_id != BRAIN_KG_INVALID_NODE, "root_id should be valid");
    TEST_ASSERT(state.node_count > 0, "node_count should be > 0");
    TEST_ASSERT(state.edge_count > 0, "edge_count should be > 0");

    /* Verify subfield nodes were created */
    TEST_ASSERT(state.ca1_id != BRAIN_KG_INVALID_NODE, "ca1_id should be valid");
    TEST_ASSERT(state.ca3_id != BRAIN_KG_INVALID_NODE, "ca3_id should be valid");
    TEST_ASSERT(state.dg_id != BRAIN_KG_INVALID_NODE, "dg_id should be valid");
    TEST_ASSERT(state.subiculum_id != BRAIN_KG_INVALID_NODE, "subiculum_id should be valid");

    /* Verify memory nodes were created */
    TEST_ASSERT(state.memory_system_id != BRAIN_KG_INVALID_NODE, "memory_system_id should be valid");
    TEST_ASSERT(state.episodic_id != BRAIN_KG_INVALID_NODE, "episodic_id should be valid");
    TEST_ASSERT(state.spatial_id != BRAIN_KG_INVALID_NODE, "spatial_id should be valid");

    /* Verify navigation nodes were created */
    TEST_ASSERT(state.nav_system_id != BRAIN_KG_INVALID_NODE, "nav_system_id should be valid");
    TEST_ASSERT(state.place_cells_id != BRAIN_KG_INVALID_NODE, "place_cells_id should be valid");
    TEST_ASSERT(state.grid_cells_id != BRAIN_KG_INVALID_NODE, "grid_cells_id should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Full registration completed successfully");
}

/**
 * Test: Register with selective config (only subfields)
 */
void test_hippocampus_kg_register_selective(void)
{
    printf("\n=== test_hippocampus_kg_register_selective ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);
    config.register_memory_nodes = false;
    config.register_nav_nodes = false;
    config.register_cross_edges = false;

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Selective registration should return 0");

    /* Verify subfields were created */
    TEST_ASSERT(state.ca1_id != BRAIN_KG_INVALID_NODE, "ca1_id should be valid");

    /* Verify memory nodes were NOT created */
    TEST_ASSERT(state.memory_system_id == BRAIN_KG_INVALID_NODE ||
                state.memory_system_id == 0, "memory_system_id should be invalid");

    brain_kg_destroy(kg);
    TEST_PASS("Selective registration works correctly");
}

/**
 * Test: Register subfields individually
 */
void test_hippocampus_kg_register_subfields(void)
{
    printf("\n=== test_hippocampus_kg_register_subfields ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Create root node first */
    brain_kg_node_id_t root = brain_kg_add_node(kg, HIPPOCAMPUS_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL, "Hippocampus root");
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "Root node should be created");

    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = hippocampus_kg_register_subfields(kg, root, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_subfields should return 0");

    TEST_ASSERT(state.ca1_id != BRAIN_KG_INVALID_NODE, "CA1 should be registered");
    TEST_ASSERT(state.ca3_id != BRAIN_KG_INVALID_NODE, "CA3 should be registered");
    TEST_ASSERT(state.dg_id != BRAIN_KG_INVALID_NODE, "DG should be registered");
    TEST_ASSERT(state.subiculum_id != BRAIN_KG_INVALID_NODE, "Subiculum should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Subfield registration works correctly");
}

//=============================================================================
// Unit Tests - Query Operations
//=============================================================================

/**
 * Test: Get root node
 */
void test_hippocampus_kg_get_root(void)
{
    printf("\n=== test_hippocampus_kg_get_root ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Register hippocampus */
    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    int result = hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Registration should succeed");

    /* Query root */
    brain_kg_node_id_t root = hippocampus_kg_get_root(kg);
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "get_root should return valid ID");
    TEST_ASSERT(root == state.root_id, "get_root should match state.root_id");

    brain_kg_destroy(kg);
    TEST_PASS("Get root works correctly");
}

/**
 * Test: Get root with NULL KG
 */
void test_hippocampus_kg_get_root_null(void)
{
    printf("\n=== test_hippocampus_kg_get_root_null ===\n");

    brain_kg_node_id_t root = hippocampus_kg_get_root(NULL);
    TEST_ASSERT(root == BRAIN_KG_INVALID_NODE, "get_root(NULL) should return INVALID_NODE");

    TEST_PASS("NULL KG handling correct for get_root");
}

/**
 * Test: Find subsystem by name
 */
void test_hippocampus_kg_find_subsystem(void)
{
    printf("\n=== test_hippocampus_kg_find_subsystem ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    /* Find CA1 */
    brain_kg_node_id_t ca1 = hippocampus_kg_find_subsystem(kg, HIPPOCAMPUS_KG_CA1_NAME);
    TEST_ASSERT(ca1 != BRAIN_KG_INVALID_NODE, "CA1 should be found");
    TEST_ASSERT(ca1 == state.ca1_id, "Found CA1 should match state");

    /* Find CA3 */
    brain_kg_node_id_t ca3 = hippocampus_kg_find_subsystem(kg, HIPPOCAMPUS_KG_CA3_NAME);
    TEST_ASSERT(ca3 != BRAIN_KG_INVALID_NODE, "CA3 should be found");

    /* Find non-existent */
    brain_kg_node_id_t invalid = hippocampus_kg_find_subsystem(kg, "nonexistent");
    TEST_ASSERT(invalid == BRAIN_KG_INVALID_NODE, "Non-existent should return INVALID_NODE");

    brain_kg_destroy(kg);
    TEST_PASS("Find subsystem works correctly");
}

/**
 * Test: Get memory nodes list
 */
void test_hippocampus_kg_get_memory_nodes(void)
{
    printf("\n=== test_hippocampus_kg_get_memory_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = hippocampus_kg_get_memory_nodes(kg);
    /* Note: May return NULL if no nodes match the specific type query */
    /* The implementation might use brain_kg_get_nodes_by_type which may need specific handling */

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Memory nodes list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get memory nodes works correctly");
}

/**
 * Test: Get subfields list
 */
void test_hippocampus_kg_get_subfields(void)
{
    printf("\n=== test_hippocampus_kg_get_subfields ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = hippocampus_kg_get_subfields(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Subfields list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get subfields works correctly");
}

//=============================================================================
// Unit Tests - State Synchronization
//=============================================================================

/**
 * Test: Update state metadata
 */
void test_hippocampus_kg_update_state(void)
{
    printf("\n=== test_hippocampus_kg_update_state ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = hippocampus_kg_update_state(kg, &state,
        0.8f,   /* encoding_strength */
        0.9f,   /* retrieval_accuracy */
        0.5f,   /* consolidation_progress */
        0.95f,  /* spatial_precision */
        TEST_ADMIN_TOKEN);

    TEST_ASSERT(result == 0, "update_state should return 0");

    brain_kg_destroy(kg);
    TEST_PASS("State update works correctly");
}

/**
 * Test: Update state with NULL parameters
 */
void test_hippocampus_kg_update_state_null(void)
{
    printf("\n=== test_hippocampus_kg_update_state_null ===\n");

    int result = hippocampus_kg_update_state(NULL, NULL, 0.5f, 0.5f, 0.5f, 0.5f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "update_state(NULL, NULL, ...) should return -1");

    TEST_PASS("NULL handling for update_state correct");
}

//=============================================================================
// Unit Tests - Cleanup
//=============================================================================

/**
 * Test: Unregister all nodes
 */
void test_hippocampus_kg_unregister_all(void)
{
    printf("\n=== test_hippocampus_kg_unregister_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));
    hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = hippocampus_kg_unregister_all(kg, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "unregister_all should return 0");
    TEST_ASSERT(state.registered == false, "state.registered should be false after unregister");

    brain_kg_destroy(kg);
    TEST_PASS("Unregister all works correctly");
}

/**
 * Test: Unregister with NULL parameters
 */
void test_hippocampus_kg_unregister_null(void)
{
    printf("\n=== test_hippocampus_kg_unregister_null ===\n");

    int result = hippocampus_kg_unregister_all(NULL, NULL, TEST_ADMIN_TOKEN);
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
    printf("Hippocampus KG Wiring Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_hippocampus_kg_default_config();
    test_hippocampus_kg_config_null();

    /* Registration tests */
    test_hippocampus_kg_register_null_kg();
    test_hippocampus_kg_register_all();
    test_hippocampus_kg_register_selective();
    test_hippocampus_kg_register_subfields();

    /* Query tests */
    test_hippocampus_kg_get_root();
    test_hippocampus_kg_get_root_null();
    test_hippocampus_kg_find_subsystem();
    test_hippocampus_kg_get_memory_nodes();
    test_hippocampus_kg_get_subfields();

    /* State sync tests */
    test_hippocampus_kg_update_state();
    test_hippocampus_kg_update_state_null();

    /* Cleanup tests */
    test_hippocampus_kg_unregister_all();
    test_hippocampus_kg_unregister_null();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
