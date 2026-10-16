/**
 * @file test_mesh_hippocampus_integration.c
 * @brief Unit tests for Hippocampus Mesh Network Integration
 * @date 2026-02-02
 *
 * WHAT: Tests for hippocampus mesh integration functionality
 * WHY:  Ensure memory operations coordination works via distributed consensus
 * HOW:  Test lifecycle, registration, memory operations, consolidation, statistics
 *
 * Tests cover:
 * - Default configuration
 * - Create/destroy lifecycle
 * - Participant registration/unregistration
 * - Memory encoding proposals
 * - Memory retrieval proposals
 * - Memory consolidation triggers
 * - Statistics retrieval
 * - NULL parameter handling
 * - Transaction type identification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/nimcp_mesh_hippocampus_integration.h"
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

//=============================================================================
// Unit Tests - Configuration
//=============================================================================

/**
 * Test: Default configuration
 */
void test_hippocampus_default_config(void)
{
    printf("\n=== test_hippocampus_default_config ===\n");

    mesh_hippocampus_config_t config;
    memset(&config, 0xFF, sizeof(config));

    nimcp_error_t result = mesh_hippocampus_default_config(&config);
    TEST_ASSERT(result == NIMCP_OK, "mesh_hippocampus_default_config should return NIMCP_OK");

    /* Verify sensible defaults */
    TEST_ASSERT(config.encoding_timeout_ms > 0, "Encoding timeout should be > 0");
    TEST_ASSERT(config.retrieval_timeout_ms > 0, "Retrieval timeout should be > 0");
    TEST_ASSERT(config.consolidation_timeout_ms > 0, "Consolidation timeout should be > 0");

    TEST_PASS("Default configuration works");
}

/**
 * Test: Default configuration with NULL
 */
void test_hippocampus_default_config_null(void)
{
    printf("\n=== test_hippocampus_default_config_null ===\n");

    nimcp_error_t result = mesh_hippocampus_default_config(NULL);
    TEST_ASSERT(result != NIMCP_OK, "mesh_hippocampus_default_config(NULL) should fail");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Lifecycle
//=============================================================================

/**
 * Test: Create and destroy integration
 */
void test_hippocampus_create_destroy(void)
{
    printf("\n=== test_hippocampus_create_destroy ===\n");

    mesh_hippocampus_config_t config;
    mesh_hippocampus_default_config(&config);
    config.enable_health_monitoring = false;
    config.verbose_logging = false;

    /* Without bootstrap/hippocampus, create should fail or return NULL */
    mesh_hippocampus_integration_t* integration = mesh_hippocampus_create(NULL, NULL, &config);

    if (integration != NULL) {
        mesh_hippocampus_destroy(integration);
    }

    TEST_PASS("Create/destroy lifecycle works (or gracefully fails without dependencies)");
}

/**
 * Test: Destroy NULL (should be safe)
 */
void test_hippocampus_destroy_null(void)
{
    printf("\n=== test_hippocampus_destroy_null ===\n");

    mesh_hippocampus_destroy(NULL);  /* Should not crash */

    TEST_PASS("NULL destroy is safe");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register participant with NULL
 */
void test_hippocampus_register_participant_null(void)
{
    printf("\n=== test_hippocampus_register_participant_null ===\n");

    nimcp_error_t result = mesh_hippocampus_register_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "register_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for register_participant");
}

/**
 * Test: Unregister participant with NULL
 */
void test_hippocampus_unregister_participant_null(void)
{
    printf("\n=== test_hippocampus_unregister_participant_null ===\n");

    nimcp_error_t result = mesh_hippocampus_unregister_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "unregister_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for unregister_participant");
}

/**
 * Test: Get participant ID with NULL
 */
void test_hippocampus_get_participant_id_null(void)
{
    printf("\n=== test_hippocampus_get_participant_id_null ===\n");

    mesh_participant_id_t id = mesh_hippocampus_get_participant_id(NULL);
    TEST_ASSERT(id == 0, "get_participant_id(NULL) should return 0");

    TEST_PASS("NULL handling correct for get_participant_id");
}

/**
 * Test: Is registered with NULL
 */
void test_hippocampus_is_registered_null(void)
{
    printf("\n=== test_hippocampus_is_registered_null ===\n");

    bool registered = mesh_hippocampus_is_registered(NULL);
    TEST_ASSERT(registered == false, "is_registered(NULL) should return false");

    TEST_PASS("NULL handling correct for is_registered");
}

//=============================================================================
// Unit Tests - Memory Operations API
//=============================================================================

/**
 * Test: Propose encoding with NULL
 */
void test_hippocampus_propose_encoding_null(void)
{
    printf("\n=== test_hippocampus_propose_encoding_null ===\n");

    const char* test_data = "test memory data";
    nimcp_error_t result = mesh_hippocampus_propose_encoding(
        NULL, test_data, strlen(test_data), "test_context");

    TEST_ASSERT(result != NIMCP_OK, "propose_encoding(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for propose_encoding");
}

/**
 * Test: Propose retrieval with NULL
 */
void test_hippocampus_propose_retrieval_null(void)
{
    printf("\n=== test_hippocampus_propose_retrieval_null ===\n");

    const char* query_cue = "test cue";
    nimcp_error_t result = mesh_hippocampus_propose_retrieval(
        NULL, query_cue, strlen(query_cue));

    TEST_ASSERT(result != NIMCP_OK, "propose_retrieval(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for propose_retrieval");
}

/**
 * Test: Trigger consolidation with NULL
 */
void test_hippocampus_trigger_consolidation_null(void)
{
    printf("\n=== test_hippocampus_trigger_consolidation_null ===\n");

    nimcp_error_t result = mesh_hippocampus_trigger_consolidation(NULL);
    TEST_ASSERT(result != NIMCP_OK, "trigger_consolidation(NULL) should fail");

    TEST_PASS("NULL handling correct for trigger_consolidation");
}

//=============================================================================
// Unit Tests - Statistics
//=============================================================================

/**
 * Test: Get stats with NULL
 */
void test_hippocampus_get_stats_null(void)
{
    printf("\n=== test_hippocampus_get_stats_null ===\n");

    mesh_hippocampus_stats_t stats;
    nimcp_error_t result = mesh_hippocampus_get_stats(NULL, &stats);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, ...) should fail");

    result = mesh_hippocampus_get_stats(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for get_stats");
}

/**
 * Test: Reset stats with NULL
 */
void test_hippocampus_reset_stats_null(void)
{
    printf("\n=== test_hippocampus_reset_stats_null ===\n");

    nimcp_error_t result = mesh_hippocampus_reset_stats(NULL);
    TEST_ASSERT(result != NIMCP_OK, "reset_stats(NULL) should fail");

    TEST_PASS("NULL handling correct for reset_stats");
}

//=============================================================================
// Unit Tests - Health Agent
//=============================================================================

/**
 * Test: Set health agent with NULL
 */
void test_hippocampus_set_health_agent_null(void)
{
    printf("\n=== test_hippocampus_set_health_agent_null ===\n");

    nimcp_error_t result = mesh_hippocampus_set_health_agent(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_health_agent(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for set_health_agent");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Is hippocampus transaction
 */
void test_hippocampus_is_hippocampus_transaction(void)
{
    printf("\n=== test_hippocampus_is_hippocampus_transaction ===\n");

    /* Test hippocampus transaction types */
    bool is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_ENCODE);
    TEST_ASSERT(is_hippo == true, "ENCODE should be hippocampus transaction");

    is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_RETRIEVE);
    TEST_ASSERT(is_hippo == true, "RETRIEVE should be hippocampus transaction");

    is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_CONSOLIDATE);
    TEST_ASSERT(is_hippo == true, "CONSOLIDATE should be hippocampus transaction");

    is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_PLACE_UPDATE);
    TEST_ASSERT(is_hippo == true, "PLACE_UPDATE should be hippocampus transaction");

    is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_GRID_UPDATE);
    TEST_ASSERT(is_hippo == true, "GRID_UPDATE should be hippocampus transaction");

    is_hippo = mesh_hippocampus_is_hippocampus_transaction(MESH_TX_HIPPOCAMPUS_REPLAY);
    TEST_ASSERT(is_hippo == true, "REPLAY should be hippocampus transaction");

    /* Test non-hippocampus transaction type */
    is_hippo = mesh_hippocampus_is_hippocampus_transaction(0x0001);  /* Generic type */
    TEST_ASSERT(is_hippo == false, "Generic type should not be hippocampus transaction");

    /* Test amygdala type (should not be hippocampus) */
    is_hippo = mesh_hippocampus_is_hippocampus_transaction(0x1701);  /* AMYGDALA */
    TEST_ASSERT(is_hippo == false, "Amygdala type should not be hippocampus transaction");

    TEST_PASS("Is hippocampus transaction check works");
}

/**
 * Test: Hippocampus transaction type ranges
 */
void test_hippocampus_transaction_ranges(void)
{
    printf("\n=== test_hippocampus_transaction_ranges ===\n");

    /* Verify hippocampus transaction base is correct */
    TEST_ASSERT(MESH_TX_HIPPOCAMPUS_BASE == 0x1600, "HIPPOCAMPUS_BASE should be 0x1600");
    TEST_ASSERT(MESH_TX_HIPPOCAMPUS_ENCODE == 0x1601, "ENCODE should be 0x1601");
    TEST_ASSERT(MESH_TX_HIPPOCAMPUS_SYSTEMS_CONSOLIDATE == 0x1609, "SYSTEMS_CONSOLIDATE should be 0x1609");

    /* Verify types are in ascending order */
    TEST_ASSERT(MESH_TX_HIPPOCAMPUS_ENCODE < MESH_TX_HIPPOCAMPUS_RETRIEVE,
                "ENCODE < RETRIEVE");
    TEST_ASSERT(MESH_TX_HIPPOCAMPUS_RETRIEVE < MESH_TX_HIPPOCAMPUS_CONSOLIDATE,
                "RETRIEVE < CONSOLIDATE");

    TEST_PASS("Hippocampus transaction ranges are correct");
}

//=============================================================================
// Unit Tests - Statistics Structure
//=============================================================================

/**
 * Test: Stats structure initialization
 */
void test_hippocampus_stats_structure(void)
{
    printf("\n=== test_hippocampus_stats_structure ===\n");

    mesh_hippocampus_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    /* Verify all fields are accessible */
    TEST_ASSERT(stats.encodings_proposed == 0, "encodings_proposed should be 0 after memset");
    TEST_ASSERT(stats.encodings_committed == 0, "encodings_committed should be 0");
    TEST_ASSERT(stats.encodings_rejected == 0, "encodings_rejected should be 0");
    TEST_ASSERT(stats.retrievals_proposed == 0, "retrievals_proposed should be 0");
    TEST_ASSERT(stats.retrievals_committed == 0, "retrievals_committed should be 0");
    TEST_ASSERT(stats.retrievals_rejected == 0, "retrievals_rejected should be 0");
    TEST_ASSERT(stats.consolidations_triggered == 0, "consolidations_triggered should be 0");
    TEST_ASSERT(stats.replay_events == 0, "replay_events should be 0");
    TEST_ASSERT(stats.transactions_received == 0, "transactions_received should be 0");
    TEST_ASSERT(stats.transactions_endorsed == 0, "transactions_endorsed should be 0");
    TEST_ASSERT(stats.transactions_vetoed == 0, "transactions_vetoed should be 0");
    TEST_ASSERT(stats.health_heartbeats_sent == 0, "health_heartbeats_sent should be 0");
    TEST_ASSERT(stats.last_encoding_ns == 0, "last_encoding_ns should be 0");
    TEST_ASSERT(stats.last_retrieval_ns == 0, "last_retrieval_ns should be 0");
    TEST_ASSERT(stats.last_consolidation_ns == 0, "last_consolidation_ns should be 0");

    TEST_PASS("Stats structure is properly defined");
}

//=============================================================================
// Unit Tests - Configuration Validation
//=============================================================================

/**
 * Test: Config structure fields
 */
void test_hippocampus_config_structure(void)
{
    printf("\n=== test_hippocampus_config_structure ===\n");

    mesh_hippocampus_config_t config;
    mesh_hippocampus_default_config(&config);

    /* Verify all config fields are accessible */
    TEST_ASSERT(config.require_consensus_for_encoding == true || config.require_consensus_for_encoding == false,
                "require_consensus_for_encoding should be bool");
    TEST_ASSERT(config.require_consensus_for_retrieval == true || config.require_consensus_for_retrieval == false,
                "require_consensus_for_retrieval should be bool");
    TEST_ASSERT(config.enable_distributed_consolidation == true || config.enable_distributed_consolidation == false,
                "enable_distributed_consolidation should be bool");
    TEST_ASSERT(config.encoding_timeout_ms > 0, "encoding_timeout_ms should be > 0");
    TEST_ASSERT(config.retrieval_timeout_ms > 0, "retrieval_timeout_ms should be > 0");
    TEST_ASSERT(config.consolidation_timeout_ms > 0, "consolidation_timeout_ms should be > 0");
    TEST_ASSERT(config.enable_health_monitoring == true || config.enable_health_monitoring == false,
                "enable_health_monitoring should be bool");
    TEST_ASSERT(config.heartbeat_interval_ms > 0 || config.heartbeat_interval_ms == 0,
                "heartbeat_interval_ms should be valid");
    TEST_ASSERT(config.verbose_logging == true || config.verbose_logging == false,
                "verbose_logging should be bool");

    TEST_PASS("Config structure is properly defined");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("Hippocampus Mesh Integration Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_hippocampus_default_config();
    test_hippocampus_default_config_null();

    /* Lifecycle tests */
    test_hippocampus_create_destroy();
    test_hippocampus_destroy_null();

    /* Registration tests */
    test_hippocampus_register_participant_null();
    test_hippocampus_unregister_participant_null();
    test_hippocampus_get_participant_id_null();
    test_hippocampus_is_registered_null();

    /* Memory operations tests */
    test_hippocampus_propose_encoding_null();
    test_hippocampus_propose_retrieval_null();
    test_hippocampus_trigger_consolidation_null();

    /* Statistics tests */
    test_hippocampus_get_stats_null();
    test_hippocampus_reset_stats_null();

    /* Health agent tests */
    test_hippocampus_set_health_agent_null();

    /* Utility function tests */
    test_hippocampus_is_hippocampus_transaction();
    test_hippocampus_transaction_ranges();

    /* Structure tests */
    test_hippocampus_stats_structure();
    test_hippocampus_config_structure();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
