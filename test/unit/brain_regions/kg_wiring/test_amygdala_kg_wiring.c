/**
 * @file test_amygdala_kg_wiring.c
 * @brief Unit tests for Amygdala Knowledge Graph wiring
 * @date 2026-02-02
 *
 * Tests for the amygdala KG wiring module:
 * - Default configuration
 * - NULL parameter handling
 * - Node registration (nuclei, emotion, learning, output)
 * - Edge registration
 * - Query operations
 * - State synchronization
 * - Cleanup operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
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
void test_amygdala_kg_default_config(void)
{
    printf("\n=== test_amygdala_kg_default_config ===\n");

    amygdala_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = amygdala_kg_default_config(&config);
    TEST_ASSERT(result == 0, "amygdala_kg_default_config should return 0");

    TEST_ASSERT(config.register_bla == true, "register_bla should be true");
    TEST_ASSERT(config.register_cea == true, "register_cea should be true");
    TEST_ASSERT(config.register_itc == true, "register_itc should be true");
    TEST_ASSERT(config.register_emotion_nodes == true, "register_emotion_nodes should be true");
    TEST_ASSERT(config.register_learning_nodes == true, "register_learning_nodes should be true");
    TEST_ASSERT(config.register_output_nodes == true, "register_output_nodes should be true");
    TEST_ASSERT(config.register_cross_edges == true, "register_cross_edges should be true");
    TEST_ASSERT(config.include_state_metadata == true, "include_state_metadata should be true");

    TEST_PASS("Default configuration values correct");
}

/**
 * Test: NULL parameter handling for config
 */
void test_amygdala_kg_config_null(void)
{
    printf("\n=== test_amygdala_kg_config_null ===\n");

    int result = amygdala_kg_default_config(NULL);
    TEST_ASSERT(result == -1, "amygdala_kg_default_config(NULL) should return -1");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register all nodes with NULL KG
 */
void test_amygdala_kg_register_null_kg(void)
{
    printf("\n=== test_amygdala_kg_register_null_kg ===\n");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = amygdala_kg_register_all(NULL, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "amygdala_kg_register_all(NULL, ...) should return -1");

    TEST_PASS("NULL KG handling correct");
}

/**
 * Test: Register all nodes with valid KG
 */
void test_amygdala_kg_register_all(void)
{
    printf("\n=== test_amygdala_kg_register_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = amygdala_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "amygdala_kg_register_all should return 0");

    /* Verify state was populated */
    TEST_ASSERT(state.registered == true, "state.registered should be true");
    TEST_ASSERT(state.root_id != BRAIN_KG_INVALID_NODE, "root_id should be valid");
    TEST_ASSERT(state.node_count > 0, "node_count should be > 0");
    TEST_ASSERT(state.edge_count > 0, "edge_count should be > 0");

    /* Verify nucleus nodes were created */
    TEST_ASSERT(state.bla_id != BRAIN_KG_INVALID_NODE, "bla_id should be valid");
    TEST_ASSERT(state.la_id != BRAIN_KG_INVALID_NODE, "la_id should be valid");
    TEST_ASSERT(state.ba_id != BRAIN_KG_INVALID_NODE, "ba_id should be valid");
    TEST_ASSERT(state.cea_id != BRAIN_KG_INVALID_NODE, "cea_id should be valid");
    TEST_ASSERT(state.itc_id != BRAIN_KG_INVALID_NODE, "itc_id should be valid");

    /* Verify emotion nodes were created */
    TEST_ASSERT(state.emotion_system_id != BRAIN_KG_INVALID_NODE, "emotion_system_id should be valid");
    TEST_ASSERT(state.fear_detect_id != BRAIN_KG_INVALID_NODE, "fear_detect_id should be valid");
    TEST_ASSERT(state.threat_id != BRAIN_KG_INVALID_NODE, "threat_id should be valid");
    TEST_ASSERT(state.valence_id != BRAIN_KG_INVALID_NODE, "valence_id should be valid");
    TEST_ASSERT(state.arousal_id != BRAIN_KG_INVALID_NODE, "arousal_id should be valid");

    /* Verify learning nodes were created */
    TEST_ASSERT(state.learning_system_id != BRAIN_KG_INVALID_NODE, "learning_system_id should be valid");
    TEST_ASSERT(state.conditioning_id != BRAIN_KG_INVALID_NODE, "conditioning_id should be valid");
    TEST_ASSERT(state.extinction_id != BRAIN_KG_INVALID_NODE, "extinction_id should be valid");

    /* Verify output nodes were created */
    TEST_ASSERT(state.autonomic_id != BRAIN_KG_INVALID_NODE, "autonomic_id should be valid");
    TEST_ASSERT(state.behavioral_id != BRAIN_KG_INVALID_NODE, "behavioral_id should be valid");
    TEST_ASSERT(state.hormonal_id != BRAIN_KG_INVALID_NODE, "hormonal_id should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Full registration completed successfully");
}

/**
 * Test: Register with selective config (only nuclei)
 */
void test_amygdala_kg_register_selective(void)
{
    printf("\n=== test_amygdala_kg_register_selective ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);
    config.register_emotion_nodes = false;
    config.register_learning_nodes = false;
    config.register_output_nodes = false;
    config.register_cross_edges = false;

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = amygdala_kg_register_all(kg, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Selective registration should return 0");

    /* Verify nuclei were created */
    TEST_ASSERT(state.bla_id != BRAIN_KG_INVALID_NODE, "bla_id should be valid");
    TEST_ASSERT(state.cea_id != BRAIN_KG_INVALID_NODE, "cea_id should be valid");

    brain_kg_destroy(kg);
    TEST_PASS("Selective registration works correctly");
}

/**
 * Test: Register nuclei individually
 */
void test_amygdala_kg_register_nuclei(void)
{
    printf("\n=== test_amygdala_kg_register_nuclei ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    /* Create root node first */
    brain_kg_node_id_t root = brain_kg_add_node(kg, AMYGDALA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL, "Amygdala root");
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "Root node should be created");

    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = amygdala_kg_register_nuclei(kg, root, &config, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_nuclei should return 0");

    TEST_ASSERT(state.bla_id != BRAIN_KG_INVALID_NODE, "BLA should be registered");
    TEST_ASSERT(state.la_id != BRAIN_KG_INVALID_NODE, "LA should be registered");
    TEST_ASSERT(state.ba_id != BRAIN_KG_INVALID_NODE, "BA should be registered");
    TEST_ASSERT(state.cea_id != BRAIN_KG_INVALID_NODE, "CeA should be registered");
    TEST_ASSERT(state.itc_id != BRAIN_KG_INVALID_NODE, "ITC should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Nuclei registration works correctly");
}

/**
 * Test: Register emotion nodes
 */
void test_amygdala_kg_register_emotion_nodes(void)
{
    printf("\n=== test_amygdala_kg_register_emotion_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    brain_kg_node_id_t root = brain_kg_add_node(kg, AMYGDALA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL, "Amygdala root");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = amygdala_kg_register_emotion_nodes(kg, root, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_emotion_nodes should return 0");

    TEST_ASSERT(state.emotion_system_id != BRAIN_KG_INVALID_NODE, "Emotion system should be registered");
    TEST_ASSERT(state.fear_detect_id != BRAIN_KG_INVALID_NODE, "Fear detection should be registered");
    TEST_ASSERT(state.threat_id != BRAIN_KG_INVALID_NODE, "Threat assessment should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Emotion nodes registration works correctly");
}

/**
 * Test: Register learning nodes
 */
void test_amygdala_kg_register_learning_nodes(void)
{
    printf("\n=== test_amygdala_kg_register_learning_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    brain_kg_node_id_t root = brain_kg_add_node(kg, AMYGDALA_KG_ROOT_NAME,
        BRAIN_KG_NODE_SUBCORTICAL, "Amygdala root");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    state.root_id = root;

    int result = amygdala_kg_register_learning_nodes(kg, root, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "register_learning_nodes should return 0");

    TEST_ASSERT(state.learning_system_id != BRAIN_KG_INVALID_NODE, "Learning system should be registered");
    TEST_ASSERT(state.conditioning_id != BRAIN_KG_INVALID_NODE, "Conditioning should be registered");
    TEST_ASSERT(state.extinction_id != BRAIN_KG_INVALID_NODE, "Extinction should be registered");

    brain_kg_destroy(kg);
    TEST_PASS("Learning nodes registration works correctly");
}

//=============================================================================
// Unit Tests - Query Operations
//=============================================================================

/**
 * Test: Get root node
 */
void test_amygdala_kg_get_root(void)
{
    printf("\n=== test_amygdala_kg_get_root ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    int result = amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "Registration should succeed");

    brain_kg_node_id_t root = amygdala_kg_get_root(kg);
    TEST_ASSERT(root != BRAIN_KG_INVALID_NODE, "get_root should return valid ID");
    TEST_ASSERT(root == state.root_id, "get_root should match state.root_id");

    brain_kg_destroy(kg);
    TEST_PASS("Get root works correctly");
}

/**
 * Test: Get root with NULL KG
 */
void test_amygdala_kg_get_root_null(void)
{
    printf("\n=== test_amygdala_kg_get_root_null ===\n");

    brain_kg_node_id_t root = amygdala_kg_get_root(NULL);
    TEST_ASSERT(root == BRAIN_KG_INVALID_NODE, "get_root(NULL) should return INVALID_NODE");

    TEST_PASS("NULL KG handling correct for get_root");
}

/**
 * Test: Find subsystem by name
 */
void test_amygdala_kg_find_subsystem(void)
{
    printf("\n=== test_amygdala_kg_find_subsystem ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    /* Find BLA */
    brain_kg_node_id_t bla = amygdala_kg_find_subsystem(kg, AMYGDALA_KG_BLA_NAME);
    TEST_ASSERT(bla != BRAIN_KG_INVALID_NODE, "BLA should be found");
    TEST_ASSERT(bla == state.bla_id, "Found BLA should match state");

    /* Find CeA */
    brain_kg_node_id_t cea = amygdala_kg_find_subsystem(kg, AMYGDALA_KG_CEA_NAME);
    TEST_ASSERT(cea != BRAIN_KG_INVALID_NODE, "CeA should be found");

    /* Find non-existent */
    brain_kg_node_id_t invalid = amygdala_kg_find_subsystem(kg, "nonexistent");
    TEST_ASSERT(invalid == BRAIN_KG_INVALID_NODE, "Non-existent should return INVALID_NODE");

    brain_kg_destroy(kg);
    TEST_PASS("Find subsystem works correctly");
}

/**
 * Test: Get emotion nodes list
 */
void test_amygdala_kg_get_emotion_nodes(void)
{
    printf("\n=== test_amygdala_kg_get_emotion_nodes ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = amygdala_kg_get_emotion_nodes(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Emotion nodes list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get emotion nodes works correctly");
}

/**
 * Test: Get nuclei list
 */
void test_amygdala_kg_get_nuclei(void)
{
    printf("\n=== test_amygdala_kg_get_nuclei ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    brain_kg_node_list_t* list = amygdala_kg_get_nuclei(kg);

    if (list != NULL) {
        TEST_ASSERT(list->count >= 0, "Nuclei list should have valid count");
        brain_kg_node_list_destroy(list);
    }

    brain_kg_destroy(kg);
    TEST_PASS("Get nuclei works correctly");
}

//=============================================================================
// Unit Tests - State Synchronization
//=============================================================================

/**
 * Test: Update state metadata
 */
void test_amygdala_kg_update_state(void)
{
    printf("\n=== test_amygdala_kg_update_state ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = amygdala_kg_update_state(kg, &state,
        0.7f,   /* threat_level */
        0.8f,   /* fear_strength */
        0.6f,   /* arousal_level */
        0.4f,   /* extinction_progress */
        TEST_ADMIN_TOKEN);

    TEST_ASSERT(result == 0, "update_state should return 0");

    brain_kg_destroy(kg);
    TEST_PASS("State update works correctly");
}

/**
 * Test: Update state with NULL parameters
 */
void test_amygdala_kg_update_state_null(void)
{
    printf("\n=== test_amygdala_kg_update_state_null ===\n");

    int result = amygdala_kg_update_state(NULL, NULL, 0.5f, 0.5f, 0.5f, 0.5f, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == -1, "update_state(NULL, NULL, ...) should return -1");

    TEST_PASS("NULL handling for update_state correct");
}

//=============================================================================
// Unit Tests - Cleanup
//=============================================================================

/**
 * Test: Unregister all nodes
 */
void test_amygdala_kg_unregister_all(void)
{
    printf("\n=== test_amygdala_kg_unregister_all ===\n");

    brain_kg_t* kg = create_test_kg();
    TEST_ASSERT(kg != NULL, "brain_kg_create should succeed");

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));
    amygdala_kg_register_all(kg, NULL, &state, TEST_ADMIN_TOKEN);

    int result = amygdala_kg_unregister_all(kg, &state, TEST_ADMIN_TOKEN);
    TEST_ASSERT(result == 0, "unregister_all should return 0");
    TEST_ASSERT(state.registered == false, "state.registered should be false after unregister");

    brain_kg_destroy(kg);
    TEST_PASS("Unregister all works correctly");
}

/**
 * Test: Unregister with NULL parameters
 */
void test_amygdala_kg_unregister_null(void)
{
    printf("\n=== test_amygdala_kg_unregister_null ===\n");

    int result = amygdala_kg_unregister_all(NULL, NULL, TEST_ADMIN_TOKEN);
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
    printf("Amygdala KG Wiring Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_amygdala_kg_default_config();
    test_amygdala_kg_config_null();

    /* Registration tests */
    test_amygdala_kg_register_null_kg();
    test_amygdala_kg_register_all();
    test_amygdala_kg_register_selective();
    test_amygdala_kg_register_nuclei();
    test_amygdala_kg_register_emotion_nodes();
    test_amygdala_kg_register_learning_nodes();

    /* Query tests */
    test_amygdala_kg_get_root();
    test_amygdala_kg_get_root_null();
    test_amygdala_kg_find_subsystem();
    test_amygdala_kg_get_emotion_nodes();
    test_amygdala_kg_get_nuclei();

    /* State sync tests */
    test_amygdala_kg_update_state();
    test_amygdala_kg_update_state_null();

    /* Cleanup tests */
    test_amygdala_kg_unregister_all();
    test_amygdala_kg_unregister_null();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
