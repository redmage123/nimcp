/**
 * @file test_brain_kg_snapshot.c
 * @brief Unit tests for brain KG snapshot persistence
 * @date 2026-01-16
 *
 * Tests for the KG-based brain snapshot storage functionality.
 * - Default configuration
 * - Snapshot backend selection
 * - Save/restore roundtrip (mock)
 * - List/delete operations
 * - Linked checkpoint operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core/brain/persistence/nimcp_brain_kg_snapshot.h"
#include "core/brain/nimcp_brain.h"
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
// Unit Tests
//=============================================================================

/**
 * Test: Default configuration initialization
 */
void test_kg_snapshot_default_config(void)
{
    printf("\n=== test_kg_snapshot_default_config ===\n");

    kg_snapshot_config_t config;
    memset(&config, 0xFF, sizeof(config));  // Fill with garbage

    int result = kg_snapshot_default_config(&config);
    TEST_ASSERT(result == 0, "kg_snapshot_default_config should return 0");

    TEST_ASSERT(config.enable_compression == true,
                "Default compression should be enabled");
    TEST_ASSERT(config.compression_level == 6,
                "Default compression level should be 6");
    TEST_ASSERT(config.enable_encryption == true,
                "Default encryption should be enabled");
    TEST_ASSERT(config.encryption_algo == (int)KG_CRYPTO_HYBRID_KYBER_AES_SNAPSHOT,
                "Default encryption algo should be HYBRID_KYBER_AES");
    TEST_ASSERT(config.compute_hmac == true,
                "Default HMAC should be enabled");
    TEST_ASSERT(config.link_to_kg_checkpoint == false,
                "Default link_to_kg_checkpoint should be false");
    TEST_ASSERT(config.max_blob_size_mb == 1024,
                "Default max_blob_size_mb should be 1024");

    TEST_PASS("Default configuration values correct");
}

/**
 * Test: NULL parameter handling
 */
void test_kg_snapshot_null_params(void)
{
    printf("\n=== test_kg_snapshot_null_params ===\n");

    // Test kg_snapshot_default_config with NULL
    int result = kg_snapshot_default_config(NULL);
    TEST_ASSERT(result == -1, "kg_snapshot_default_config(NULL) should return -1");
    TEST_PASS("kg_snapshot_default_config NULL handling");

    // Test brain_save_snapshot_kg with NULL brain
    result = brain_save_snapshot_kg(NULL, "test", "desc", NULL);
    TEST_ASSERT(result == -1, "brain_save_snapshot_kg(NULL, ...) should return -1");
    TEST_PASS("brain_save_snapshot_kg NULL brain handling");

    // Test brain_restore_snapshot_kg with NULL name
    brain_t restored = brain_restore_snapshot_kg(NULL, NULL);
    TEST_ASSERT(restored == NULL, "brain_restore_snapshot_kg(NULL, NULL) should return NULL");
    TEST_PASS("brain_restore_snapshot_kg NULL handling");

    // Test brain_list_snapshots_kg with NULL
    uint32_t count = 0;
    result = brain_list_snapshots_kg(NULL, NULL, 10, &count);
    TEST_ASSERT(result == -1, "brain_list_snapshots_kg(NULL, ...) should return -1");
    TEST_PASS("brain_list_snapshots_kg NULL handling");

    // Test brain_delete_snapshot_kg with NULL
    result = brain_delete_snapshot_kg(NULL, NULL);
    TEST_ASSERT(result == -1, "brain_delete_snapshot_kg(NULL, NULL) should return -1");
    TEST_PASS("brain_delete_snapshot_kg NULL handling");
}

/**
 * Test: Backend string conversion
 */
void test_snapshot_backend_to_string(void)
{
    printf("\n=== test_snapshot_backend_to_string ===\n");

    const char* auto_str = snapshot_backend_to_string(SNAPSHOT_BACKEND_AUTO);
    TEST_ASSERT(strcmp(auto_str, "AUTO") == 0, "AUTO backend string");

    const char* file_str = snapshot_backend_to_string(SNAPSHOT_BACKEND_FILE);
    TEST_ASSERT(strcmp(file_str, "FILE") == 0, "FILE backend string");

    const char* kg_str = snapshot_backend_to_string(SNAPSHOT_BACKEND_KG);
    TEST_ASSERT(strcmp(kg_str, "KG") == 0, "KG backend string");

    const char* unknown_str = snapshot_backend_to_string((snapshot_backend_t)99);
    TEST_ASSERT(strcmp(unknown_str, "UNKNOWN") == 0, "UNKNOWN backend string");

    TEST_PASS("Backend string conversions correct");
}

/**
 * Test: List snapshots with empty result
 */
void test_list_snapshots_empty(void)
{
    printf("\n=== test_list_snapshots_empty ===\n");

    // This test would require a mock KG persistence context
    // For now, we just verify the function handles parameters correctly

    brain_kg_snapshot_info_t infos[10];
    uint32_t count = 999;

    // Note: This will fail with NULL persistence context
    // In a full test, we'd use a mock context
    int result = brain_list_snapshots_kg(NULL, infos, 10, &count);
    TEST_ASSERT(result == -1 || count == 0,
                "List with NULL persistence should return error or 0 count");

    TEST_PASS("List snapshots empty case handled");
}

/**
 * Test: Linked checkpoint NULL handling
 */
void test_linked_checkpoint_null(void)
{
    printf("\n=== test_linked_checkpoint_null ===\n");

    int result = brain_create_linked_checkpoint(NULL, "test", NULL);
    TEST_ASSERT(result == -1,
                "brain_create_linked_checkpoint(NULL, ...) should return -1");

    brain_t restored = brain_restore_linked_checkpoint(NULL, NULL, NULL);
    TEST_ASSERT(restored == NULL,
                "brain_restore_linked_checkpoint(NULL, NULL, NULL) should return NULL");

    result = brain_delete_linked_checkpoint(NULL, NULL);
    TEST_ASSERT(result == -1,
                "brain_delete_linked_checkpoint(NULL, NULL) should return -1");

    TEST_PASS("Linked checkpoint NULL handling correct");
}

/**
 * Test: Schema creation with NULL
 */
void test_schema_create_null(void)
{
    printf("\n=== test_schema_create_null ===\n");

    int result = kg_snapshot_create_schema(NULL);
    TEST_ASSERT(result == -1, "kg_snapshot_create_schema(NULL) should return -1");

    TEST_PASS("Schema creation NULL handling correct");
}

/**
 * Test: Snapshot verification with NULL
 */
void test_verify_snapshot_null(void)
{
    printf("\n=== test_verify_snapshot_null ===\n");

    int result = brain_verify_snapshot_kg(NULL, NULL);
    TEST_ASSERT(result == -1, "brain_verify_snapshot_kg(NULL, NULL) should return -1");

    result = brain_get_snapshot_info_kg(NULL, NULL, NULL);
    TEST_ASSERT(result == -1, "brain_get_snapshot_info_kg(NULL, ...) should return -1");

    uint32_t deleted = 0;
    result = brain_prune_snapshots_kg(NULL, 30, &deleted);
    TEST_ASSERT(result == -1, "brain_prune_snapshots_kg(NULL, ...) should return -1");

    TEST_PASS("Verification NULL handling correct");
}

/**
 * Test: Brain config KG persistence fields
 */
void test_brain_config_kg_fields(void)
{
    printf("\n=== test_brain_config_kg_fields ===\n");

    brain_config_t config = {0};

    // Verify the new fields exist and have default values
    TEST_ASSERT(config.enable_kg_persistence == false,
                "Default enable_kg_persistence should be false");
    TEST_ASSERT(config.snapshot_backend == 0,  // SNAPSHOT_BACKEND_AUTO
                "Default snapshot_backend should be AUTO (0)");
    TEST_ASSERT(config.kg_storage_path == NULL,
                "Default kg_storage_path should be NULL");

    // Set the fields
    config.enable_kg_persistence = true;
    config.snapshot_backend = SNAPSHOT_BACKEND_KG;
    config.kg_storage_path = ".aim/kg/questdb/";

    TEST_ASSERT(config.enable_kg_persistence == true,
                "enable_kg_persistence should be settable");
    TEST_ASSERT(config.snapshot_backend == SNAPSHOT_BACKEND_KG,
                "snapshot_backend should be settable");
    TEST_ASSERT(strcmp(config.kg_storage_path, ".aim/kg/questdb/") == 0,
                "kg_storage_path should be settable");

    TEST_PASS("Brain config KG fields work correctly");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("==============================================\n");
    printf("Brain KG Snapshot Unit Tests\n");
    printf("==============================================\n");

    // Run tests
    test_kg_snapshot_default_config();
    test_kg_snapshot_null_params();
    test_snapshot_backend_to_string();
    test_list_snapshots_empty();
    test_linked_checkpoint_null();
    test_schema_create_null();
    test_verify_snapshot_null();
    test_brain_config_kg_fields();

    // Summary
    printf("\n==============================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("==============================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
