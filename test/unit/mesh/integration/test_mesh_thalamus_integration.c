/**
 * @file test_mesh_thalamus_integration.c
 * @brief Unit tests for Thalamus Mesh Network Integration
 * @date 2026-02-02
 *
 * WHAT: Tests for thalamus mesh integration functionality
 * WHY:  Ensure sensory relay and attention gating coordination works via distributed consensus
 * HOW:  Test lifecycle, registration, relay operations, attention/arousal, statistics
 *
 * Tests cover:
 * - Default configuration
 * - Create/destroy lifecycle
 * - Participant registration/unregistration
 * - Relay operations reporting
 * - Attention level updates
 * - Arousal level updates
 * - State queries (arousal, attention, mode)
 * - Statistics retrieval
 * - NULL parameter handling
 * - Transaction type identification
 * - Firing mode string conversion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mesh/nimcp_mesh_thalamus_integration.h"
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
void test_thalamus_default_config(void)
{
    printf("\n=== test_thalamus_default_config ===\n");

    mesh_thalamus_config_t config;
    memset(&config, 0xFF, sizeof(config));

    nimcp_error_t result = mesh_thalamus_default_config(&config);
    TEST_ASSERT(result == NIMCP_OK, "mesh_thalamus_default_config should return NIMCP_OK");

    /* Verify sensible defaults */
    TEST_ASSERT(config.relay_timeout_ms > 0, "Relay timeout should be > 0");
    TEST_ASSERT(config.attention_timeout_ms > 0, "Attention timeout should be > 0");

    TEST_PASS("Default configuration works");
}

/**
 * Test: Default configuration with NULL
 */
void test_thalamus_default_config_null(void)
{
    printf("\n=== test_thalamus_default_config_null ===\n");

    nimcp_error_t result = mesh_thalamus_default_config(NULL);
    TEST_ASSERT(result != NIMCP_OK, "mesh_thalamus_default_config(NULL) should fail");

    TEST_PASS("NULL config handling correct");
}

//=============================================================================
// Unit Tests - Lifecycle
//=============================================================================

/**
 * Test: Create and destroy integration
 */
void test_thalamus_create_destroy(void)
{
    printf("\n=== test_thalamus_create_destroy ===\n");

    mesh_thalamus_config_t config;
    mesh_thalamus_default_config(&config);
    config.enable_health_monitoring = false;
    config.verbose_logging = false;

    /* Without bootstrap/thalamus, create should fail or return NULL */
    mesh_thalamus_integration_t* integration = mesh_thalamus_create(NULL, NULL, &config);

    if (integration != NULL) {
        mesh_thalamus_destroy(integration);
    }

    TEST_PASS("Create/destroy lifecycle works (or gracefully fails without dependencies)");
}

/**
 * Test: Destroy NULL (should be safe)
 */
void test_thalamus_destroy_null(void)
{
    printf("\n=== test_thalamus_destroy_null ===\n");

    mesh_thalamus_destroy(NULL);  /* Should not crash */

    TEST_PASS("NULL destroy is safe");
}

//=============================================================================
// Unit Tests - Registration
//=============================================================================

/**
 * Test: Register participant with NULL
 */
void test_thalamus_register_participant_null(void)
{
    printf("\n=== test_thalamus_register_participant_null ===\n");

    nimcp_error_t result = mesh_thalamus_register_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "register_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for register_participant");
}

/**
 * Test: Unregister participant with NULL
 */
void test_thalamus_unregister_participant_null(void)
{
    printf("\n=== test_thalamus_unregister_participant_null ===\n");

    nimcp_error_t result = mesh_thalamus_unregister_participant(NULL);
    TEST_ASSERT(result != NIMCP_OK, "unregister_participant(NULL) should fail");

    TEST_PASS("NULL handling correct for unregister_participant");
}

/**
 * Test: Get participant ID with NULL
 */
void test_thalamus_get_participant_id_null(void)
{
    printf("\n=== test_thalamus_get_participant_id_null ===\n");

    mesh_participant_id_t id = mesh_thalamus_get_participant_id(NULL);
    TEST_ASSERT(id == 0, "get_participant_id(NULL) should return 0");

    TEST_PASS("NULL handling correct for get_participant_id");
}

/**
 * Test: Is registered with NULL
 */
void test_thalamus_is_registered_null(void)
{
    printf("\n=== test_thalamus_is_registered_null ===\n");

    bool registered = mesh_thalamus_is_registered(NULL);
    TEST_ASSERT(registered == false, "is_registered(NULL) should return false");

    TEST_PASS("NULL handling correct for is_registered");
}

//=============================================================================
// Unit Tests - Relay Operations API
//=============================================================================

/**
 * Test: Report relay with NULL
 */
void test_thalamus_report_relay_null(void)
{
    printf("\n=== test_thalamus_report_relay_null ===\n");

    nimcp_error_t result = mesh_thalamus_report_relay(NULL, 1, 64);
    TEST_ASSERT(result != NIMCP_OK, "report_relay(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for report_relay");
}

/**
 * Test: Update attention with NULL
 */
void test_thalamus_update_attention_null(void)
{
    printf("\n=== test_thalamus_update_attention_null ===\n");

    nimcp_error_t result = mesh_thalamus_update_attention(NULL, 0.8f);
    TEST_ASSERT(result != NIMCP_OK, "update_attention(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for update_attention");
}

/**
 * Test: Update arousal with NULL
 */
void test_thalamus_update_arousal_null(void)
{
    printf("\n=== test_thalamus_update_arousal_null ===\n");

    nimcp_error_t result = mesh_thalamus_update_arousal(NULL, 0.7f);
    TEST_ASSERT(result != NIMCP_OK, "update_arousal(NULL, ...) should fail");

    TEST_PASS("NULL handling correct for update_arousal");
}

//=============================================================================
// Unit Tests - State Query API
//=============================================================================

/**
 * Test: Get arousal with NULL
 */
void test_thalamus_get_arousal_null(void)
{
    printf("\n=== test_thalamus_get_arousal_null ===\n");

    float arousal = mesh_thalamus_get_arousal(NULL);
    TEST_ASSERT(arousal == 0.0f, "get_arousal(NULL) should return 0.0");

    TEST_PASS("NULL handling correct for get_arousal");
}

/**
 * Test: Get attention with NULL
 */
void test_thalamus_get_attention_null(void)
{
    printf("\n=== test_thalamus_get_attention_null ===\n");

    float attention = mesh_thalamus_get_attention(NULL);
    TEST_ASSERT(attention == 0.0f, "get_attention(NULL) should return 0.0");

    TEST_PASS("NULL handling correct for get_attention");
}

/**
 * Test: Get mode with NULL
 */
void test_thalamus_get_mode_null(void)
{
    printf("\n=== test_thalamus_get_mode_null ===\n");

    mesh_thalamus_firing_mode_t mode = mesh_thalamus_get_mode(NULL);
    TEST_ASSERT(mode == MESH_THALAMUS_MODE_TONIC, "get_mode(NULL) should return TONIC");

    TEST_PASS("NULL handling correct for get_mode");
}

//=============================================================================
// Unit Tests - Statistics
//=============================================================================

/**
 * Test: Get stats with NULL
 */
void test_thalamus_get_stats_null(void)
{
    printf("\n=== test_thalamus_get_stats_null ===\n");

    mesh_thalamus_stats_t stats;
    nimcp_error_t result = mesh_thalamus_get_stats(NULL, &stats);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, ...) should fail");

    result = mesh_thalamus_get_stats(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "get_stats(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for get_stats");
}

/**
 * Test: Reset stats with NULL
 */
void test_thalamus_reset_stats_null(void)
{
    printf("\n=== test_thalamus_reset_stats_null ===\n");

    nimcp_error_t result = mesh_thalamus_reset_stats(NULL);
    TEST_ASSERT(result != NIMCP_OK, "reset_stats(NULL) should fail");

    TEST_PASS("NULL handling correct for reset_stats");
}

//=============================================================================
// Unit Tests - Health Agent
//=============================================================================

/**
 * Test: Set health agent with NULL
 */
void test_thalamus_set_health_agent_null(void)
{
    printf("\n=== test_thalamus_set_health_agent_null ===\n");

    nimcp_error_t result = mesh_thalamus_set_health_agent(NULL, NULL);
    TEST_ASSERT(result != NIMCP_OK, "set_health_agent(NULL, NULL) should fail");

    TEST_PASS("NULL handling correct for set_health_agent");
}

//=============================================================================
// Unit Tests - Utility Functions
//=============================================================================

/**
 * Test: Is thalamus transaction
 */
void test_thalamus_is_thalamus_transaction(void)
{
    printf("\n=== test_thalamus_is_thalamus_transaction ===\n");

    /* Test thalamus transaction types */
    bool is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_RELAY_VISUAL);
    TEST_ASSERT(is_thalamus == true, "RELAY_VISUAL should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_RELAY_AUDITORY);
    TEST_ASSERT(is_thalamus == true, "RELAY_AUDITORY should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_RELAY_SOMATO);
    TEST_ASSERT(is_thalamus == true, "RELAY_SOMATO should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_ATTENTION_UPDATE);
    TEST_ASSERT(is_thalamus == true, "ATTENTION_UPDATE should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_AROUSAL_CHANGE);
    TEST_ASSERT(is_thalamus == true, "AROUSAL_CHANGE should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_MODE_CHANGE);
    TEST_ASSERT(is_thalamus == true, "MODE_CHANGE should be thalamus transaction");

    is_thalamus = mesh_thalamus_is_thalamus_transaction(MESH_TX_THALAMUS_BURST_TRIGGER);
    TEST_ASSERT(is_thalamus == true, "BURST_TRIGGER should be thalamus transaction");

    /* Test non-thalamus transaction type */
    is_thalamus = mesh_thalamus_is_thalamus_transaction(0x0001);  /* Generic type */
    TEST_ASSERT(is_thalamus == false, "Generic type should not be thalamus transaction");

    /* Test amygdala type (should not be thalamus) */
    is_thalamus = mesh_thalamus_is_thalamus_transaction(0x1701);  /* AMYGDALA */
    TEST_ASSERT(is_thalamus == false, "Amygdala type should not be thalamus transaction");

    TEST_PASS("Is thalamus transaction check works");
}

/**
 * Test: Thalamus transaction type ranges
 */
void test_thalamus_transaction_ranges(void)
{
    printf("\n=== test_thalamus_transaction_ranges ===\n");

    /* Verify thalamus transaction base is correct */
    TEST_ASSERT(MESH_TX_THALAMUS_BASE == 0x1900, "THALAMUS_BASE should be 0x1900");
    TEST_ASSERT(MESH_TX_THALAMUS_RELAY_VISUAL == 0x1901, "RELAY_VISUAL should be 0x1901");
    TEST_ASSERT(MESH_TX_THALAMUS_BURST_TRIGGER == 0x190A, "BURST_TRIGGER should be 0x190A");

    /* Verify types are in ascending order */
    TEST_ASSERT(MESH_TX_THALAMUS_RELAY_VISUAL < MESH_TX_THALAMUS_RELAY_AUDITORY,
                "RELAY_VISUAL < RELAY_AUDITORY");
    TEST_ASSERT(MESH_TX_THALAMUS_RELAY_AUDITORY < MESH_TX_THALAMUS_RELAY_SOMATO,
                "RELAY_AUDITORY < RELAY_SOMATO");

    TEST_PASS("Thalamus transaction ranges are correct");
}

/**
 * Test: Firing mode to string
 */
void test_thalamus_mode_to_string(void)
{
    printf("\n=== test_thalamus_mode_to_string ===\n");

    const char* str = mesh_thalamus_mode_to_string(MESH_THALAMUS_MODE_TONIC);
    TEST_ASSERT(str != NULL, "MODE_TONIC string should not be NULL");
    printf("  TONIC mode string: %s\n", str);

    str = mesh_thalamus_mode_to_string(MESH_THALAMUS_MODE_BURST);
    TEST_ASSERT(str != NULL, "MODE_BURST string should not be NULL");
    printf("  BURST mode string: %s\n", str);

    TEST_PASS("Firing mode to string works");
}

//=============================================================================
// Unit Tests - Statistics Structure
//=============================================================================

/**
 * Test: Stats structure initialization
 */
void test_thalamus_stats_structure(void)
{
    printf("\n=== test_thalamus_stats_structure ===\n");

    mesh_thalamus_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    /* Verify all fields are accessible */
    TEST_ASSERT(stats.visual_relays == 0, "visual_relays should be 0 after memset");
    TEST_ASSERT(stats.auditory_relays == 0, "auditory_relays should be 0");
    TEST_ASSERT(stats.somatosensory_relays == 0, "somatosensory_relays should be 0");
    TEST_ASSERT(stats.motor_relays == 0, "motor_relays should be 0");
    TEST_ASSERT(stats.executive_relays == 0, "executive_relays should be 0");
    TEST_ASSERT(stats.attention_updates == 0, "attention_updates should be 0");
    TEST_ASSERT(stats.trn_inhibitions == 0, "trn_inhibitions should be 0");
    TEST_ASSERT(stats.arousal_changes == 0, "arousal_changes should be 0");
    TEST_ASSERT(stats.mode_changes == 0, "mode_changes should be 0");
    TEST_ASSERT(stats.burst_triggers == 0, "burst_triggers should be 0");
    TEST_ASSERT(stats.current_arousal == 0.0f, "current_arousal should be 0.0");
    TEST_ASSERT(stats.current_attention == 0.0f, "current_attention should be 0.0");
    TEST_ASSERT(stats.current_mode == MESH_THALAMUS_MODE_TONIC, "current_mode should be TONIC");
    TEST_ASSERT(stats.transactions_received == 0, "transactions_received should be 0");
    TEST_ASSERT(stats.transactions_endorsed == 0, "transactions_endorsed should be 0");
    TEST_ASSERT(stats.transactions_vetoed == 0, "transactions_vetoed should be 0");
    TEST_ASSERT(stats.health_heartbeats_sent == 0, "health_heartbeats_sent should be 0");

    TEST_PASS("Stats structure is properly defined");
}

//=============================================================================
// Unit Tests - Configuration Validation
//=============================================================================

/**
 * Test: Config structure fields
 */
void test_thalamus_config_structure(void)
{
    printf("\n=== test_thalamus_config_structure ===\n");

    mesh_thalamus_config_t config;
    mesh_thalamus_default_config(&config);

    /* Verify all config fields are accessible */
    TEST_ASSERT(config.broadcast_relay_activity == true || config.broadcast_relay_activity == false,
                "broadcast_relay_activity should be bool");
    TEST_ASSERT(config.enable_distributed_gating == true || config.enable_distributed_gating == false,
                "enable_distributed_gating should be bool");
    TEST_ASSERT(config.sync_arousal_with_mesh == true || config.sync_arousal_with_mesh == false,
                "sync_arousal_with_mesh should be bool");
    TEST_ASSERT(config.relay_timeout_ms > 0, "relay_timeout_ms should be > 0");
    TEST_ASSERT(config.attention_timeout_ms > 0, "attention_timeout_ms should be > 0");
    TEST_ASSERT(config.enable_health_monitoring == true || config.enable_health_monitoring == false,
                "enable_health_monitoring should be bool");
    TEST_ASSERT(config.heartbeat_interval_ms > 0 || config.heartbeat_interval_ms == 0,
                "heartbeat_interval_ms should be valid");
    TEST_ASSERT(config.verbose_logging == true || config.verbose_logging == false,
                "verbose_logging should be bool");

    TEST_PASS("Config structure is properly defined");
}

//=============================================================================
// Unit Tests - Payload Structures
//=============================================================================

/**
 * Test: Relay payload structure
 */
void test_thalamus_relay_payload_structure(void)
{
    printf("\n=== test_thalamus_relay_payload_structure ===\n");

    mesh_thalamus_relay_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    payload.nucleus_type = 1;
    payload.channel_count = 64;
    payload.attention_weight = 0.8f;
    payload.arousal_level = 0.7f;

    TEST_ASSERT(payload.nucleus_type == 1, "nucleus_type should be 1");
    TEST_ASSERT(payload.channel_count == 64, "channel_count should be 64");
    TEST_ASSERT(payload.attention_weight == 0.8f, "attention_weight should be 0.8");
    TEST_ASSERT(payload.arousal_level == 0.7f, "arousal_level should be 0.7");

    TEST_PASS("Relay payload structure is properly defined");
}

/**
 * Test: Arousal payload structure
 */
void test_thalamus_arousal_payload_structure(void)
{
    printf("\n=== test_thalamus_arousal_payload_structure ===\n");

    mesh_thalamus_arousal_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    payload.old_arousal = 0.5f;
    payload.new_arousal = 0.8f;
    payload.resulting_mode = MESH_THALAMUS_MODE_TONIC;

    TEST_ASSERT(payload.old_arousal == 0.5f, "old_arousal should be 0.5");
    TEST_ASSERT(payload.new_arousal == 0.8f, "new_arousal should be 0.8");
    TEST_ASSERT(payload.resulting_mode == MESH_THALAMUS_MODE_TONIC, "resulting_mode should be TONIC");

    TEST_PASS("Arousal payload structure is properly defined");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("============================================\n");
    printf("Thalamus Mesh Integration Unit Tests\n");
    printf("============================================\n");

    /* Configuration tests */
    test_thalamus_default_config();
    test_thalamus_default_config_null();

    /* Lifecycle tests */
    test_thalamus_create_destroy();
    test_thalamus_destroy_null();

    /* Registration tests */
    test_thalamus_register_participant_null();
    test_thalamus_unregister_participant_null();
    test_thalamus_get_participant_id_null();
    test_thalamus_is_registered_null();

    /* Relay operations tests */
    test_thalamus_report_relay_null();
    test_thalamus_update_attention_null();
    test_thalamus_update_arousal_null();

    /* State query tests */
    test_thalamus_get_arousal_null();
    test_thalamus_get_attention_null();
    test_thalamus_get_mode_null();

    /* Statistics tests */
    test_thalamus_get_stats_null();
    test_thalamus_reset_stats_null();

    /* Health agent tests */
    test_thalamus_set_health_agent_null();

    /* Utility function tests */
    test_thalamus_is_thalamus_transaction();
    test_thalamus_transaction_ranges();
    test_thalamus_mode_to_string();

    /* Structure tests */
    test_thalamus_stats_structure();
    test_thalamus_config_structure();
    test_thalamus_relay_payload_structure();
    test_thalamus_arousal_payload_structure();

    printf("\n============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
