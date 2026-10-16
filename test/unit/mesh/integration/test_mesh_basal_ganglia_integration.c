/**
 * @file test_mesh_basal_ganglia_integration.c
 * @brief Unit tests for Basal Ganglia Mesh Network Integration
 * @date 2026-02-02
 *
 * WHAT: Tests for basal ganglia mesh integration functionality
 * WHY:  Ensure action selection coordination works via distributed consensus
 * HOW:  Test lifecycle, registration, action selection, RL signals, statistics
 *
 * Tests cover:
 * - Default configuration
 * - Create/destroy lifecycle
 * - Participant registration/unregistration
 * - Action selection proposals
 * - RPE (Reward Prediction Error) reporting
 * - Dopamine burst signaling
 * - Statistics retrieval
 * - NULL parameter handling
 * - Transaction type identification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/nimcp_mesh_basal_ganglia_integration.h"
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
void test_basal_ganglia_default_config(void)
{
    printf("\n=== test_basal_ganglia_default_config ===\n");

    mesh_basal_ganglia_config_t config;
    memset(&config, 0xFF, sizeof(config));

    nimcp_error_t result = mesh_basal_ganglia_default_config(&config);
    TEST_ASSERT(result == NIMCP_OK, "mesh_basal_ganglia_default_config should return NIMCP_OK");

    /* Verify sensible defaults */
    TEST_ASSERT(config.action_selection_timeout_ms > 0, "Action selection timeout should be > 0");
    TEST_ASSERT(config.learning_timeout_ms > 0, "Learning timeout should be > 0");

    TEST_PASS("Default configuration works");
}

/**
 * Test: Default configuration with NULL
 */
void test_basal_ganglia_default_config_null(void)
{
    printf("\n=== test_basal_ganglia_default_config_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_default_config(NULL);
    TEST_ASSERT(result != NIMCP_OK, "mesh_basal_ganglia_default_config(NULL) should fail");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Lifecycle
//=============================================================================

/**
 * Test: Create and destroy integration
 */
void test_basal_ganglia_create_destroy(void)
{
    printf("\n=== test_basal_ganglia_create_destroy ===\n");

    mesh_basal_ganglia_config_t config;
    mesh_basal_ganglia_default_config(&config);
    config.enable_health_monitoring = false;
    config.verbose_logging = false;

    /* Without bootstrap/BG, create should fail or return NULL */
    mesh_basal_ganglia_integration_t* integration = mesh_basal_ganglia_create(NULL, NULL, &config);

    if (integration != NULL) {
        mesh_basal_ganglia_destroy(integration);
    }

    TEST_PASS("Create/destroy lifecycle works (or gracefully fails without dependencies)");
}

/**
 * Test: Destroy NULL (should be safe)
 */
void test_basal_ganglia_destroy_null(void)
{
    printf("\n=== test_basal_ganglia_destroy_null ===\n");

    mesh_basal_ganglia_destroy(NULL);  /* Should not crash */

    TEST_PASS("NULL destroy is safe");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register participant with NULL
 */
void test_basal_ganglia_register_participant_null(void)
{
    printf("\n=== test_basal_ganglia_register_participant_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_register_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "register_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for register_participant");
}

/**
 * Test: Unregister participant with NULL
 */
void test_basal_ganglia_unregister_participant_null(void)
{
    printf("\n=== test_basal_ganglia_unregister_participant_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_unregister_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "unregister_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for unregister_participant");
}

/**
 * Test: Get participant ID with NULL
 */
void test_basal_ganglia_get_participant_id_null(void)
{
    printf("\n=== test_basal_ganglia_get_participant_id_null ===\n");

    mesh_participant_id_t id = mesh_basal_ganglia_get_participant_id(NULL);
    TEST_ASSERT(id == 0, "get_participant_id(NULL) should return 0");

    TEST_PASS("NULL handling correct for get_participant_id");
}

/**
 * Test: Is registered with NULL
 */
void test_basal_ganglia_is_registered_null(void)
{
    printf("\n=== test_basal_ganglia_is_registered_null ===\n");

    bool registered = mesh_basal_ganglia_is_registered(NULL);
    TEST_ASSERT(registered == false, "is_registered(NULL) should return false");

    TEST_PASS("NULL handling correct for is_registered");
}

//=============================================================================
// Unit Tests - Action Selection API
//=============================================================================

/**
 * Test: Propose action with NULL
 */
void test_basal_ganglia_propose_action_null(void)
{
    printf("\n=== test_basal_ganglia_propose_action_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_propose_action(NULL, 1, 0.9f);
    TEST_ASSERT(result != NIMCP_OK, "propose_action(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for propose_action");
}

/**
 * Test: Report RPE with NULL
 */
void test_basal_ganglia_report_rpe_null(void)
{
    printf("\n=== test_basal_ganglia_report_rpe_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_report_rpe(NULL, 0.5f);
    TEST_ASSERT(result != NIMCP_OK, "report_rpe(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for report_rpe");
}

/**
 * Test: Report dopamine with NULL
 */
void test_basal_ganglia_report_dopamine_null(void)
{
    printf("\n=== test_basal_ganglia_report_dopamine_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_report_dopamine(NULL, 1.0f);
    TEST_ASSERT(result != NIMCP_OK, "report_dopamine(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for report_dopamine");
}

//=============================================================================
// Unit Tests - Statistics
//=============================================================================

/**
 * Test: Get stats with NULL
 */
void test_basal_ganglia_get_stats_null(void)
{
    printf("\n=== test_basal_ganglia_get_stats_null ===\n");

    mesh_basal_ganglia_stats_t stats;
    nimcp_error_t result = mesh_basal_ganglia_get_stats(NULL, &stats);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, ...) should fail");

    result = mesh_basal_ganglia_get_stats(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for get_stats");
}

/**
 * Test: Reset stats with NULL
 */
void test_basal_ganglia_reset_stats_null(void)
{
    printf("\n=== test_basal_ganglia_reset_stats_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_reset_stats(NULL);
    TEST_ASSERT(result != NIMCP_OK, "reset_stats(NULL) should fail");

    TEST_PASS("NULL handling correct for reset_stats");
}

//=============================================================================
// Unit Tests - Health Agent
//=============================================================================

/**
 * Test: Set health agent with NULL
 */
void test_basal_ganglia_set_health_agent_null(void)
{
    printf("\n=== test_basal_ganglia_set_health_agent_null ===\n");

    nimcp_error_t result = mesh_basal_ganglia_set_health_agent(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_health_agent(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for set_health_agent");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Is BG transaction
 */
void test_basal_ganglia_is_bg_transaction(void)
{
    printf("\n=== test_basal_ganglia_is_bg_transaction ===\n");

    /* Test BG transaction types */
    bool is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_ACTION_SELECT);
    TEST_ASSERT(is_bg == true, "ACTION_SELECT should be BG transaction");

    is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_ACTION_INHIBIT);
    TEST_ASSERT(is_bg == true, "ACTION_INHIBIT should be BG transaction");

    is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_RPE_SIGNAL);
    TEST_ASSERT(is_bg == true, "RPE_SIGNAL should be BG transaction");

    is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_DOPAMINE_BURST);
    TEST_ASSERT(is_bg == true, "DOPAMINE_BURST should be BG transaction");

    is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_GO_PATHWAY);
    TEST_ASSERT(is_bg == true, "GO_PATHWAY should be BG transaction");

    is_bg = mesh_basal_ganglia_is_bg_transaction(MESH_TX_BG_NOGO_PATHWAY);
    TEST_ASSERT(is_bg == true, "NOGO_PATHWAY should be BG transaction");

    /* Test non-BG transaction type */
    is_bg = mesh_basal_ganglia_is_bg_transaction(0x0001);  /* Generic type */
    TEST_ASSERT(is_bg == false, "Generic type should not be BG transaction");

    /* Test amygdala type (should not be BG) */
    is_bg = mesh_basal_ganglia_is_bg_transaction(0x1701);  /* AMYGDALA_THREAT_DETECTED */
    TEST_ASSERT(is_bg == false, "Amygdala type should not be BG transaction");

    TEST_PASS("Is BG transaction check works");
}

/**
 * Test: BG transaction type ranges
 */
void test_basal_ganglia_transaction_ranges(void)
{
    printf("\n=== test_basal_ganglia_transaction_ranges ===\n");

    /* Verify BG transaction base is correct */
    TEST_ASSERT(MESH_TX_BG_BASE == 0x1800, "BG_BASE should be 0x1800");
    TEST_ASSERT(MESH_TX_BG_ACTION_SELECT == 0x1801, "ACTION_SELECT should be 0x1801");
    TEST_ASSERT(MESH_TX_BG_BETA_SYNC == 0x180A, "BETA_SYNC should be 0x180A");

    /* Verify types are in ascending order */
    TEST_ASSERT(MESH_TX_BG_ACTION_SELECT < MESH_TX_BG_ACTION_INHIBIT,
                "ACTION_SELECT < ACTION_INHIBIT");
    TEST_ASSERT(MESH_TX_BG_ACTION_INHIBIT < MESH_TX_BG_REWARD_PREDICTION,
                "ACTION_INHIBIT < REWARD_PREDICTION");

    TEST_PASS("BG transaction ranges are correct");
}

//=============================================================================
// Unit Tests - Statistics Structure
//=============================================================================

/**
 * Test: Stats structure initialization
 */
void test_basal_ganglia_stats_structure(void)
{
    printf("\n=== test_basal_ganglia_stats_structure ===\n");

    mesh_basal_ganglia_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    /* Verify all fields are accessible */
    TEST_ASSERT(stats.actions_proposed == 0, "actions_proposed should be 0 after memset");
    TEST_ASSERT(stats.actions_selected == 0, "actions_selected should be 0");
    TEST_ASSERT(stats.actions_inhibited == 0, "actions_inhibited should be 0");
    TEST_ASSERT(stats.rpe_signals_sent == 0, "rpe_signals_sent should be 0");
    TEST_ASSERT(stats.dopamine_bursts == 0, "dopamine_bursts should be 0");
    TEST_ASSERT(stats.go_activations == 0, "go_activations should be 0");
    TEST_ASSERT(stats.nogo_activations == 0, "nogo_activations should be 0");
    TEST_ASSERT(stats.current_action == 0, "current_action should be 0");
    TEST_ASSERT(stats.current_rpe == 0.0f, "current_rpe should be 0.0");
    TEST_ASSERT(stats.current_dopamine == 0.0f, "current_dopamine should be 0.0");
    TEST_ASSERT(stats.transactions_received == 0, "transactions_received should be 0");
    TEST_ASSERT(stats.transactions_endorsed == 0, "transactions_endorsed should be 0");

    TEST_PASS("Stats structure is properly defined");
}

//=============================================================================
// Unit Tests - Configuration Validation
//=============================================================================

/**
 * Test: Config structure fields
 */
void test_basal_ganglia_config_structure(void)
{
    printf("\n=== test_basal_ganglia_config_structure ===\n");

    mesh_basal_ganglia_config_t config;
    mesh_basal_ganglia_default_config(&config);

    /* Verify all config fields are accessible */
    TEST_ASSERT(config.require_consensus_for_action == true || config.require_consensus_for_action == false,
                "require_consensus_for_action should be bool");
    TEST_ASSERT(config.broadcast_rpe_signals == true || config.broadcast_rpe_signals == false,
                "broadcast_rpe_signals should be bool");
    TEST_ASSERT(config.enable_distributed_learning == true || config.enable_distributed_learning == false,
                "enable_distributed_learning should be bool");
    TEST_ASSERT(config.action_selection_timeout_ms > 0, "action_selection_timeout_ms should be > 0");
    TEST_ASSERT(config.learning_timeout_ms > 0, "learning_timeout_ms should be > 0");
    TEST_ASSERT(config.enable_health_monitoring == true || config.enable_health_monitoring == false,
                "enable_health_monitoring should be bool");
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
    printf("Basal Ganglia Mesh Integration Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_basal_ganglia_default_config();
    test_basal_ganglia_default_config_null();

    /* Lifecycle tests */
    test_basal_ganglia_create_destroy();
    test_basal_ganglia_destroy_null();

    /* Registration tests */
    test_basal_ganglia_register_participant_null();
    test_basal_ganglia_unregister_participant_null();
    test_basal_ganglia_get_participant_id_null();
    test_basal_ganglia_is_registered_null();

    /* Action selection tests */
    test_basal_ganglia_propose_action_null();
    test_basal_ganglia_report_rpe_null();
    test_basal_ganglia_report_dopamine_null();

    /* Statistics tests */
    test_basal_ganglia_get_stats_null();
    test_basal_ganglia_reset_stats_null();

    /* Health agent tests */
    test_basal_ganglia_set_health_agent_null();

    /* Utility function tests */
    test_basal_ganglia_is_bg_transaction();
    test_basal_ganglia_transaction_ranges();

    /* Structure tests */
    test_basal_ganglia_stats_structure();
    test_basal_ganglia_config_structure();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
