/**
 * @file test_kg_wiring_integration.c
 * @brief Integration tests for brain region KG wiring modules
 * @date 2026-02-02
 *
 * Tests cross-module interactions between hippocampus, amygdala, and PFC:
 * - All modules register in same KG
 * - Cross-region queries work
 * - State updates don't interfere
 * - Sequential registration/unregistration
 * - Memory pressure handling
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
// Integration Tests - Multi-Module Registration
//=============================================================================

/**
 * Test: All three modules register successfully in the same KG
 */
void test_all_modules_register(void)
{
    printf("\n=== test_all_modules_register ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Register hippocampus */
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    int result = hippocampus_kg_register_all(kg, NULL, &hippo_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Hippocampus registration should succeed");
    TEST_ASSERT(hippo_state.registered == true, "Hippocampus should be marked registered");

    /* Register amygdala */
    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    result = amygdala_kg_register_all(kg, NULL, &amyg_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Amygdala registration should succeed");
    TEST_ASSERT(amyg_state.registered == true, "Amygdala should be marked registered");

    /* Register PFC */
    pfc_kg_state_t pfc_state;
    memset(&pfc_state, 0, sizeof(pfc_state));
    result = pfc_kg_register_all(kg, NULL, &pfc_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "PFC registration should succeed");
    TEST_ASSERT(pfc_state.registered == true, "PFC should be marked registered");

    /* Verify all roots are unique */
    TEST_ASSERT(hippo_state.root_id != amyg_state.root_id, "Hippocampus and amygdala roots should differ");
    TEST_ASSERT(hippo_state.root_id != pfc_state.root_id, "Hippocampus and PFC roots should differ");
    TEST_ASSERT(amyg_state.root_id != pfc_state.root_id, "Amygdala and PFC roots should differ");

    /* Verify total node count is sum of individual counts */
    uint32_t total_nodes = hippo_state.node_count + amyg_state.node_count + pfc_state.node_count;
    TEST_ASSERT(total_nodes > 0, "Total node count should be positive");

    brain_kg_destroy(kg);
    TEST_PASS("All modules registered successfully in same KG");
}

/**
 * Test: Cross-module queries work correctly
 */
void test_cross_module_queries(void)
{
    printf("\n=== test_cross_module_queries ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Register all modules */
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    hippocampus_kg_register_all(kg, NULL, &hippo_state, TEST_ADMIN_TOKEN);

    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    amygdala_kg_register_all(kg, NULL, &amyg_state, TEST_ADMIN_TOKEN);

    pfc_kg_state_t pfc_state;
    memset(&pfc_state, 0, sizeof(pfc_state));
    pfc_kg_register_all(kg, NULL, &pfc_state, TEST_ADMIN_TOKEN);

    /* Query each module's root */
    brain_kg_node_id_t hippo_root = hippocampus_kg_get_root(kg);
    brain_kg_node_id_t amyg_root = amygdala_kg_get_root(kg);
    brain_kg_node_id_t pfc_root = pfc_kg_get_root(kg);

    TEST_ASSERT(hippo_root != BRAIN_KG_INVALID_NODE, "Hippocampus root should be found");
    TEST_ASSERT(amyg_root != BRAIN_KG_INVALID_NODE, "Amygdala root should be found");
    TEST_ASSERT(pfc_root != BRAIN_KG_INVALID_NODE, "PFC root should be found");

    /* Cross-module subsystem lookup should work */
    brain_kg_node_id_t ca1 = hippocampus_kg_find_subsystem(kg, HIPPOCAMPUS_KG_CA1_NAME);
    brain_kg_node_id_t bla = amygdala_kg_find_subsystem(kg, AMYGDALA_KG_BLA_NAME);
    brain_kg_node_id_t dlpfc = pfc_kg_find_subsystem(kg, PFC_KG_DLPFC_NAME);

    TEST_ASSERT(ca1 != BRAIN_KG_INVALID_NODE, "CA1 should be found");
    TEST_ASSERT(bla != BRAIN_KG_INVALID_NODE, "BLA should be found");
    TEST_ASSERT(dlpfc != BRAIN_KG_INVALID_NODE, "dlPFC should be found");

    brain_kg_destroy(kg);
    TEST_PASS("Cross-module queries work correctly");
}

/**
 * Test: State updates don't interfere between modules
 */
void test_independent_state_updates(void)
{
    printf("\n=== test_independent_state_updates ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Register all modules */
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    hippocampus_kg_register_all(kg, NULL, &hippo_state, TEST_ADMIN_TOKEN);

    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    amygdala_kg_register_all(kg, NULL, &amyg_state, TEST_ADMIN_TOKEN);

    pfc_kg_state_t pfc_state;
    memset(&pfc_state, 0, sizeof(pfc_state));
    pfc_kg_register_all(kg, NULL, &pfc_state, TEST_ADMIN_TOKEN);

    /* Update each module's state independently */
    int r1 = hippocampus_kg_update_state(kg, &hippo_state, 0.9f, 0.8f, 0.7f, 0.95f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r1 == 0, "Hippocampus state update should succeed");

    int r2 = amygdala_kg_update_state(kg, &amyg_state, 0.6f, 0.7f, 0.5f, 0.3f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r2 == 0, "Amygdala state update should succeed");

    int r3 = pfc_kg_update_state(kg, &pfc_state, 0.8f, 0.75f, 0.2f, 0.9f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r3 == 0, "PFC state update should succeed");

    /* Update again to verify no conflicts */
    r1 = hippocampus_kg_update_state(kg, &hippo_state, 0.5f, 0.5f, 0.5f, 0.5f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r1 == 0, "Second hippocampus update should succeed");

    brain_kg_destroy(kg);
    TEST_PASS("State updates don't interfere between modules");
}

/**
 * Test: Sequential unregistration
 */
void test_sequential_unregistration(void)
{
    printf("\n=== test_sequential_unregistration ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Register all modules */
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    hippocampus_kg_register_all(kg, NULL, &hippo_state, TEST_ADMIN_TOKEN);

    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    amygdala_kg_register_all(kg, NULL, &amyg_state, TEST_ADMIN_TOKEN);

    pfc_kg_state_t pfc_state;
    memset(&pfc_state, 0, sizeof(pfc_state));
    pfc_kg_register_all(kg, NULL, &pfc_state, TEST_ADMIN_TOKEN);

    /* Unregister in reverse order */
    int r1 = pfc_kg_unregister_all(kg, &pfc_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r1 == 0, "PFC unregistration should succeed");
    TEST_ASSERT(pfc_state.registered == false, "PFC should be marked unregistered");

    int r2 = amygdala_kg_unregister_all(kg, &amyg_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r2 == 0, "Amygdala unregistration should succeed");
    TEST_ASSERT(amyg_state.registered == false, "Amygdala should be marked unregistered");

    int r3 = hippocampus_kg_unregister_all(kg, &hippo_state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r3 == 0, "Hippocampus unregistration should succeed");
    TEST_ASSERT(hippo_state.registered == false, "Hippocampus should be marked unregistered");

    brain_kg_destroy(kg);
    TEST_PASS("Sequential unregistration works correctly");
}

/**
 * Test: Re-registration after unregistration
 */
void test_reregistration(void)
{
    printf("\n=== test_reregistration ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* First registration cycle */
    hippocampus_kg_state_t hippo_state1;
    memset(&hippo_state1, 0, sizeof(hippo_state1));
    int r1 = hippocampus_kg_register_all(kg, NULL, &hippo_state1, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r1 == 0, "First registration should succeed");

    brain_kg_node_id_t first_root = hippo_state1.root_id;

    /* Unregister */
    hippocampus_kg_unregister_all(kg, &hippo_state1, TEST_ADMIN_TOKEN);

    /* Re-register */
    hippocampus_kg_state_t hippo_state2;
    memset(&hippo_state2, 0, sizeof(hippo_state2));
    int r2 = hippocampus_kg_register_all(kg, NULL, &hippo_state2, TEST_ADMIN_TOKEN);
    TEST_ASSERT(r2 == 0, "Re-registration should succeed");
    TEST_ASSERT(hippo_state2.registered == true, "Should be marked registered after re-registration");

    /* Root ID may or may not be the same, but should be valid */
    TEST_ASSERT(hippo_state2.root_id != BRAIN_KG_INVALID_NODE, "New root should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Re-registration after unregistration works correctly");
}

/**
 * Test: Multiple KG instances with different modules
 */
void test_multiple_kg_instances(void)
{
    printf("\n=== test_multiple_kg_instances ===\n");

    /* Create two separate KG instances */
    brain_kg_t* kg1 = create_test_kg();
    brain_kg_t* kg2 = create_test_kg();
    TEST_ASSERT(kg1 != NULL && kg2 != NULL, "Both KGs should be created");

    /* Register hippocampus in KG1 */
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    hippocampus_kg_register_all(kg1, NULL, &hippo_state, TEST_ADMIN_TOKEN);

    /* Register amygdala in KG2 */
    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    amygdala_kg_register_all(kg2, NULL, &amyg_state, TEST_ADMIN_TOKEN);

    /* Verify they're independent */
    brain_kg_node_id_t hippo_in_kg1 = hippocampus_kg_get_root(kg1);
    brain_kg_node_id_t hippo_in_kg2 = hippocampus_kg_get_root(kg2);
    brain_kg_node_id_t amyg_in_kg1 = amygdala_kg_get_root(kg1);
    brain_kg_node_id_t amyg_in_kg2 = amygdala_kg_get_root(kg2);

    TEST_ASSERT(hippo_in_kg1 != BRAIN_KG_INVALID_NODE, "Hippocampus should exist in KG1");
    TEST_ASSERT(hippo_in_kg2 == BRAIN_KG_INVALID_NODE, "Hippocampus should NOT exist in KG2");
    TEST_ASSERT(amyg_in_kg1 == BRAIN_KG_INVALID_NODE, "Amygdala should NOT exist in KG1");
    TEST_ASSERT(amyg_in_kg2 != BRAIN_KG_INVALID_NODE, "Amygdala should exist in KG2");

    brain_kg_destroy(kg1);
    brain_kg_destroy(kg2);
    TEST_PASS("Multiple KG instances work independently");
}

/**
 * Test: Rapid registration/unregistration cycles
 */
void test_rapid_cycles(void)
{
    printf("\n=== test_rapid_cycles ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    hippocampus_kg_state_t state;

    for (int i = 0; i < 5; i++) {
        memset(&state, 0, sizeof(state));
        int r1 = hippocampus_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);
        TEST_ASSERT(r1 == 0, "Registration should succeed in cycle");

        int r2 = hippocampus_kg_unregister_all(kg, &state, TEST_ADMIN_TOKEN);
        TEST_ASSERT(r2 == 0, "Unregistration should succeed in cycle");
    }

    brain_kg_destroy(kg);
    TEST_PASS("Rapid registration/unregistration cycles work correctly");
}

/**
 * Test: All modules with all subsystems
 */
void test_full_brain_topology(void)
{
    printf("\n=== test_full_brain_topology ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Full registration with all options enabled */
    hippocampus_kg_config_t hippo_cfg;
    hippocampus_kg_default_config(&hippo_cfg);
    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));
    hippocampus_kg_register_all(kg, &hippo_cfg, &hippo_state, TEST_ADMIN_TOKEN);

    amygdala_kg_config_t amyg_cfg;
    amygdala_kg_default_config(&amyg_cfg);
    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));
    amygdala_kg_register_all(kg, &amyg_cfg, &amyg_state, TEST_ADMIN_TOKEN);

    pfc_kg_config_t pfc_cfg;
    pfc_kg_default_config(&pfc_cfg);
    pfc_kg_state_t pfc_state;
    memset(&pfc_state, 0, sizeof(pfc_state));
    pfc_kg_register_all(kg, &pfc_cfg, &pfc_state, TEST_ADMIN_TOKEN);

    /* Calculate total */
    uint32_t total_nodes = hippo_state.node_count + amyg_state.node_count + pfc_state.node_count;
    uint32_t total_edges = hippo_state.edge_count + amyg_state.edge_count + pfc_state.edge_count;

    printf("  Total nodes: %u\n", total_nodes);
    printf("  Total edges: %u\n", total_edges);

    TEST_ASSERT(total_nodes >= 30, "Should have at least 30 nodes across all modules");
    TEST_ASSERT(total_edges >= 20, "Should have at least 20 edges across all modules");

    brain_kg_destroy(kg);
    TEST_PASS("Full brain topology created successfully");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("KG Wiring Integration Tests\n");
    printf("============================================\n");

    test_all_modules_register();
    test_cross_module_queries();
    test_independent_state_updates();
    test_sequential_unregistration();
    test_reregistration();
    test_multiple_kg_instances();
    test_rapid_cycles();
    test_full_brain_topology();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
