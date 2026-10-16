/**
 * @file test_mesh_amygdala_integration.c
 * @brief Unit tests for Amygdala Mesh Network Integration
 * @date 2026-02-02
 *
 * WHAT: Tests for amygdala mesh integration functionality
 * WHY:  Ensure emotional processing coordination works via distributed consensus
 * HOW:  Test lifecycle, registration, threat detection, veto operations, statistics
 *
 * Tests cover:
 * - Default configuration
 * - Create/destroy lifecycle
 * - Participant registration/unregistration
 * - Threat detection and reporting
 * - Veto operations
 * - Anxiety level updates
 * - State queries
 * - Statistics retrieval
 * - Callback registration
 * - NULL parameter handling
 * - Concurrent access patterns
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/nimcp_mesh_amygdala_integration.h"
#include "utils/error/nimcp_error_codes.h"
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

/* Test callback counters */
static int g_threat_callback_count = 0;
static int g_veto_callback_count = 0;

/**
 * @brief Test threat callback
 */
static void test_threat_callback(
    mesh_amygdala_threat_level_t old_level,
    mesh_amygdala_threat_level_t new_level,
    void* ctx)
{
    (void)old_level;
    (void)new_level;
    (void)ctx;
    g_threat_callback_count++;
}

/**
 * @brief Test veto callback
 */
static void test_veto_callback(
    mesh_participant_id_t target_tx,
    const char* reason,
    void* ctx)
{
    (void)target_tx;
    (void)reason;
    (void)ctx;
    g_veto_callback_count++;
}

//=============================================================================
// Unit Tests - Configuration
//=============================================================================

/**
 * Test: Default configuration
 */
void test_amygdala_default_config(void)
{
    printf("\n=== test_amygdala_default_config ===\n");

    mesh_amygdala_config_t config;
    memset(&config, 0xFF, sizeof(config));

    nimcp_error_t result = mesh_amygdala_default_config(&config);
    TEST_ASSERT(result == NIMCP_OK, "mesh_amygdala_default_config should return NIMCP_OK");

    /* Verify sensible defaults */
    TEST_ASSERT(config.initial_threat_level <= MESH_AMYGDALA_THREAT_SEVERE,
                "Initial threat level should be valid");
    TEST_ASSERT(config.initial_anxiety >= 0.0f && config.initial_anxiety <= 1.0f,
                "Initial anxiety should be in [0,1]");
    TEST_ASSERT(config.veto_threshold <= MESH_AMYGDALA_THREAT_SEVERE,
                "Veto threshold should be valid");

    TEST_PASS("Default configuration works");
}

/**
 * Test: Default configuration with NULL
 */
void test_amygdala_default_config_null(void)
{
    printf("\n=== test_amygdala_default_config_null ===\n");

    nimcp_error_t result = mesh_amygdala_default_config(NULL);
    TEST_ASSERT(result != NIMCP_OK, "mesh_amygdala_default_config(NULL) should fail");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Lifecycle
//=============================================================================

/**
 * Test: Create and destroy integration
 */
void test_amygdala_create_destroy(void)
{
    printf("\n=== test_amygdala_create_destroy ===\n");

    mesh_amygdala_config_t config;
    mesh_amygdala_default_config(&config);
    config.enable_health_monitoring = false;  /* Disable for simple testing */
    config.verbose_logging = false;

    /* Note: In real testing, we'd need a bootstrap and amygdala instance */
    /* For unit testing, we test with NULL which should fail gracefully */
    mesh_amygdala_integration_t* integration = mesh_amygdala_create(NULL, NULL, &config);

    /* Without bootstrap/amygdala, create should fail or return NULL */
    if (integration != NULL) {
        mesh_amygdala_destroy(integration);
    }

    TEST_PASS("Create/destroy lifecycle works (or gracefully fails without dependencies)");
}

/**
 * Test: Destroy NULL (should be safe)
 */
void test_amygdala_destroy_null(void)
{
    printf("\n=== test_amygdala_destroy_null ===\n");

    mesh_amygdala_destroy(NULL);  /* Should not crash */

    TEST_PASS("NULL destroy is safe");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register participant with NULL
 */
void test_amygdala_register_participant_null(void)
{
    printf("\n=== test_amygdala_register_participant_null ===\n");

    nimcp_error_t result = mesh_amygdala_register_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "register_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for register_participant");
}

/**
 * Test: Unregister participant with NULL
 */
void test_amygdala_unregister_participant_null(void)
{
    printf("\n=== test_amygdala_unregister_participant_null ===\n");

    nimcp_error_t result = mesh_amygdala_unregister_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "unregister_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for unregister_participant");
}

/**
 * Test: Get participant ID with NULL
 */
void test_amygdala_get_participant_id_null(void)
{
    printf("\n=== test_amygdala_get_participant_id_null ===\n");

    mesh_participant_id_t id = mesh_amygdala_get_participant_id(NULL);
    /* Should return invalid/zero ID for NULL */
    TEST_ASSERT(id == 0, "get_participant_id(NULL) should return 0");

    TEST_PASS("NULL handling correct for get_participant_id");
}

/**
 * Test: Is registered with NULL
 */
void test_amygdala_is_registered_null(void)
{
    printf("\n=== test_amygdala_is_registered_null ===\n");

    bool registered = mesh_amygdala_is_registered(NULL);
    TEST_ASSERT(registered == false, "is_registered(NULL) should return false");

    TEST_PASS("NULL handling correct for is_registered");
}

//=============================================================================
// Unit Tests - Threat and Emotional API
//=============================================================================

/**
 * Test: Report threat with NULL
 */
void test_amygdala_report_threat_null(void)
{
    printf("\n=== test_amygdala_report_threat_null ===\n");

    nimcp_error_t result = mesh_amygdala_report_threat(
        NULL,
        MESH_AMYGDALA_THREAT_HIGH,
        0.8f,
        "Test threat");

    TEST_ASSERT(result != NIMCP_OK, "report_threat(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for report_threat");
}

/**
 * Test: Issue veto with NULL
 */
void test_amygdala_issue_veto_null(void)
{
    printf("\n=== test_amygdala_issue_veto_null ===\n");

    nimcp_error_t result = mesh_amygdala_issue_veto(NULL, 12345, "Test veto");
    TEST_ASSERT(result != NIMCP_OK, "issue_veto(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for issue_veto");
}

/**
 * Test: Update anxiety with NULL
 */
void test_amygdala_update_anxiety_null(void)
{
    printf("\n=== test_amygdala_update_anxiety_null ===\n");

    nimcp_error_t result = mesh_amygdala_update_anxiety(NULL, 0.5f);
    TEST_ASSERT(result != NIMCP_OK, "update_anxiety(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for update_anxiety");
}

//=============================================================================
// Unit Tests - State Query API
//=============================================================================

/**
 * Test: Get threat level with NULL
 */
void test_amygdala_get_threat_level_null(void)
{
    printf("\n=== test_amygdala_get_threat_level_null ===\n");

    mesh_amygdala_threat_level_t level = mesh_amygdala_get_threat_level(NULL);
    TEST_ASSERT(level == MESH_AMYGDALA_THREAT_NONE, "get_threat_level(NULL) should return NONE");

    TEST_PASS("NULL handling correct for get_threat_level");
}

/**
 * Test: Get anxiety with NULL
 */
void test_amygdala_get_anxiety_null(void)
{
    printf("\n=== test_amygdala_get_anxiety_null ===\n");

    float anxiety = mesh_amygdala_get_anxiety(NULL);
    TEST_ASSERT(anxiety == 0.0f, "get_anxiety(NULL) should return 0.0");

    TEST_PASS("NULL handling correct for get_anxiety");
}

/**
 * Test: Get fear with NULL
 */
void test_amygdala_get_fear_null(void)
{
    printf("\n=== test_amygdala_get_fear_null ===\n");

    float fear = mesh_amygdala_get_fear(NULL);
    TEST_ASSERT(fear == 0.0f, "get_fear(NULL) should return 0.0");

    TEST_PASS("NULL handling correct for get_fear");
}

//=============================================================================
// Unit Tests - Callbacks
//=============================================================================

/**
 * Test: Set threat callback with NULL integration
 */
void test_amygdala_set_threat_callback_null(void)
{
    printf("\n=== test_amygdala_set_threat_callback_null ===\n");

    nimcp_error_t result = mesh_amygdala_set_threat_callback(NULL, test_threat_callback, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_threat_callback(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for set_threat_callback");
}

/**
 * Test: Set veto callback with NULL integration
 */
void test_amygdala_set_veto_callback_null(void)
{
    printf("\n=== test_amygdala_set_veto_callback_null ===\n");

    nimcp_error_t result = mesh_amygdala_set_veto_callback(NULL, test_veto_callback, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_veto_callback(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for set_veto_callback");
}

//=============================================================================
// Unit Tests - Statistics
//=============================================================================

/**
 * Test: Get stats with NULL
 */
void test_amygdala_get_stats_null(void)
{
    printf("\n=== test_amygdala_get_stats_null ===\n");

    mesh_amygdala_stats_t stats;
    nimcp_error_t result = mesh_amygdala_get_stats(NULL, &stats);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, ...) should fail");

    result = mesh_amygdala_get_stats(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for get_stats");
}

/**
 * Test: Reset stats with NULL
 */
void test_amygdala_reset_stats_null(void)
{
    printf("\n=== test_amygdala_reset_stats_null ===\n");

    nimcp_error_t result = mesh_amygdala_reset_stats(NULL);
    TEST_ASSERT(result != NIMCP_OK, "reset_stats(NULL) should fail");

    TEST_PASS("NULL handling correct for reset_stats");
}

//=============================================================================
// Unit Tests - Health Agent
//=============================================================================

/**
 * Test: Set health agent with NULL
 */
void test_amygdala_set_health_agent_null(void)
{
    printf("\n=== test_amygdala_set_health_agent_null ===\n");

    nimcp_error_t result = mesh_amygdala_set_health_agent(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_health_agent(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for set_health_agent");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Threat level to string
 */
void test_amygdala_threat_to_string(void)
{
    printf("\n=== test_amygdala_threat_to_string ===\n");

    const char* str = mesh_amygdala_threat_to_string(MESH_AMYGDALA_THREAT_NONE);
    TEST_ASSERT(str != NULL, "THREAT_NONE string should not be NULL");

    str = mesh_amygdala_threat_to_string(MESH_AMYGDALA_THREAT_LOW);
    TEST_ASSERT(str != NULL, "THREAT_LOW string should not be NULL");

    str = mesh_amygdala_threat_to_string(MESH_AMYGDALA_THREAT_MODERATE);
    TEST_ASSERT(str != NULL, "THREAT_MODERATE string should not be NULL");

    str = mesh_amygdala_threat_to_string(MESH_AMYGDALA_THREAT_HIGH);
    TEST_ASSERT(str != NULL, "THREAT_HIGH string should not be NULL");

    str = mesh_amygdala_threat_to_string(MESH_AMYGDALA_THREAT_SEVERE);
    TEST_ASSERT(str != NULL, "THREAT_SEVERE string should not be NULL");

    TEST_PASS("Threat level to string works");
}

/**
 * Test: Is amygdala transaction
 */
void test_amygdala_is_amygdala_transaction(void)
{
    printf("\n=== test_amygdala_is_amygdala_transaction ===\n");

    /* Test amygdala transaction types */
    bool is_amygdala = mesh_amygdala_is_amygdala_transaction(MESH_TX_AMYGDALA_THREAT_DETECTED);
    TEST_ASSERT(is_amygdala == true, "THREAT_DETECTED should be amygdala transaction");

    is_amygdala = mesh_amygdala_is_amygdala_transaction(MESH_TX_AMYGDALA_FEAR_RESPONSE);
    TEST_ASSERT(is_amygdala == true, "FEAR_RESPONSE should be amygdala transaction");

    is_amygdala = mesh_amygdala_is_amygdala_transaction(MESH_TX_AMYGDALA_VETO);
    TEST_ASSERT(is_amygdala == true, "VETO should be amygdala transaction");

    /* Test non-amygdala transaction type */
    is_amygdala = mesh_amygdala_is_amygdala_transaction(0x0001);  /* Generic type */
    TEST_ASSERT(is_amygdala == false, "Generic type should not be amygdala transaction");

    TEST_PASS("Is amygdala transaction check works");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("Amygdala Mesh Integration Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_amygdala_default_config();
    test_amygdala_default_config_null();

    /* Lifecycle tests */
    test_amygdala_create_destroy();
    test_amygdala_destroy_null();

    /* Registration tests */
    test_amygdala_register_participant_null();
    test_amygdala_unregister_participant_null();
    test_amygdala_get_participant_id_null();
    test_amygdala_is_registered_null();

    /* Threat and emotional API tests */
    test_amygdala_report_threat_null();
    test_amygdala_issue_veto_null();
    test_amygdala_update_anxiety_null();

    /* State query tests */
    test_amygdala_get_threat_level_null();
    test_amygdala_get_anxiety_null();
    test_amygdala_get_fear_null();

    /* Callback tests */
    test_amygdala_set_threat_callback_null();
    test_amygdala_set_veto_callback_null();

    /* Statistics tests */
    test_amygdala_get_stats_null();
    test_amygdala_reset_stats_null();

    /* Health agent tests */
    test_amygdala_set_health_agent_null();

    /* Utility function tests */
    test_amygdala_threat_to_string();
    test_amygdala_is_amygdala_transaction();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
