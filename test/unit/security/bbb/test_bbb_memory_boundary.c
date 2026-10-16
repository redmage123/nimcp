/**
 * @file test_bbb_memory_boundary.c
 * @brief Unit tests for BBB Memory Boundary module
 * @date 2026-02-02
 *
 * WHAT: Comprehensive tests for memory region protection
 * WHY:  Ensure BBB memory boundary properly protects memory regions
 * HOW:  Test region registration, access checking, stack canaries, quarantine
 *
 * Tests covered:
 * - NULL parameter handling
 * - Memory region registration
 * - Memory region unregistration
 * - Memory access checking
 * - Read-only region enforcement
 * - Stack canary installation and verification
 * - Quarantine functionality
 * - Edge cases and boundary conditions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "security/nimcp_blood_brain_barrier.h"

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

/* External reset function */
extern void bbb_memory_boundary_reset_internal(void);

static bbb_system_t g_test_system = NULL;

/**
 * @brief Setup function called before each test
 *
 * Creates a fresh BBB system for each test to ensure clean state
 */
static void setup(void)
{
    bbb_memory_boundary_reset_internal();
    /* Destroy existing system if any, and create fresh one */
    if (g_test_system != NULL) {
        bbb_system_destroy(g_test_system);
    }
    g_test_system = bbb_system_create(NULL);
}

/**
 * @brief Teardown function called after all tests
 */
static void teardown(void)
{
    if (g_test_system != NULL) {
        bbb_system_destroy(g_test_system);
        g_test_system = NULL;
    }
}

//=============================================================================
// Unit Tests - NULL Parameter Handling
//=============================================================================

/**
 * Test: bbb_register_memory_region with NULL address
 */
void test_register_region_null_address(void)
{
    printf("\n=== test_register_region_null_address ===\n");
    setup();

    uint32_t region_id = bbb_register_memory_region(g_test_system, NULL, 1024, false);
    TEST_ASSERT(region_id == 0, "bbb_register_memory_region(NULL) should return 0");

    TEST_PASS("NULL address handling correct");
}

/**
 * Test: bbb_check_memory_access with NULL address
 */
void test_check_memory_access_null(void)
{
    printf("\n=== test_check_memory_access_null ===\n");
    setup();

    bool result = bbb_check_memory_access(g_test_system, NULL, 100, false);
    TEST_ASSERT(result == false, "bbb_check_memory_access(NULL) should return false");

    TEST_PASS("NULL address access check handling correct");
}

/**
 * Test: bbb_protect_memory with NULL address
 */
void test_protect_memory_null(void)
{
    printf("\n=== test_protect_memory_null ===\n");
    setup();

    bool result = bbb_protect_memory(g_test_system, NULL, 4096, true, false, false);
    TEST_ASSERT(result == false, "bbb_protect_memory(NULL) should return false");

    TEST_PASS("NULL address protect memory handling correct");
}

/**
 * Test: bbb_install_stack_canary with NULL pointer
 */
void test_install_canary_null(void)
{
    printf("\n=== test_install_canary_null ===\n");
    setup();

    uint64_t canary = bbb_install_stack_canary(g_test_system, NULL);
    TEST_ASSERT(canary == 0, "bbb_install_stack_canary(NULL) should return 0");

    TEST_PASS("NULL pointer canary installation handling correct");
}

/**
 * Test: bbb_verify_stack_canary with NULL pointer
 */
void test_verify_canary_null(void)
{
    printf("\n=== test_verify_canary_null ===\n");
    setup();

    bool result = bbb_verify_stack_canary(g_test_system, NULL, 0x12345678);
    TEST_ASSERT(result == false, "bbb_verify_stack_canary(NULL) should return false");

    TEST_PASS("NULL pointer canary verification handling correct");
}

//=============================================================================
// Unit Tests - Memory Region Registration
//=============================================================================

/**
 * Test: Register valid memory region
 */
void test_register_region_valid(void)
{
    printf("\n=== test_register_region_valid ===\n");
    setup();

    uint8_t buffer[1024];
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);
    TEST_ASSERT(region_id != 0, "Valid region registration should return non-zero ID");

    TEST_PASS("Valid memory region registration succeeded");
}

/**
 * Test: Register memory region with zero size
 */
void test_register_region_zero_size(void)
{
    printf("\n=== test_register_region_zero_size ===\n");
    setup();

    uint8_t buffer[1024];
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, 0, false);
    TEST_ASSERT(region_id == 0, "Zero-size region registration should return 0");

    TEST_PASS("Zero-size region registration handling correct");
}

/**
 * Test: Register read-only memory region
 */
void test_register_region_readonly(void)
{
    printf("\n=== test_register_region_readonly ===\n");
    setup();

    uint8_t buffer[1024];
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), true);
    TEST_ASSERT(region_id != 0, "Read-only region registration should succeed");

    TEST_PASS("Read-only memory region registration succeeded");
}

/**
 * Test: Register multiple memory regions
 */
void test_register_multiple_regions(void)
{
    printf("\n=== test_register_multiple_regions ===\n");
    setup();

    uint8_t buffer1[256];
    uint8_t buffer2[512];
    uint8_t buffer3[128];

    uint32_t id1 = bbb_register_memory_region(g_test_system, buffer1, sizeof(buffer1), false);
    uint32_t id2 = bbb_register_memory_region(g_test_system, buffer2, sizeof(buffer2), true);
    uint32_t id3 = bbb_register_memory_region(g_test_system, buffer3, sizeof(buffer3), false);

    TEST_ASSERT(id1 != 0, "First region registration should succeed");
    TEST_ASSERT(id2 != 0, "Second region registration should succeed");
    TEST_ASSERT(id3 != 0, "Third region registration should succeed");
    TEST_ASSERT(id1 != id2 && id2 != id3 && id1 != id3, "Region IDs should be unique");

    TEST_PASS("Multiple memory region registration succeeded");
}

//=============================================================================
// Unit Tests - Memory Region Unregistration
//=============================================================================

/**
 * Test: Unregister valid memory region
 */
void test_unregister_region_valid(void)
{
    printf("\n=== test_unregister_region_valid ===\n");
    setup();

    uint8_t buffer[1024];
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);
    TEST_ASSERT(region_id != 0, "Registration should succeed");

    bool result = bbb_unregister_memory_region(g_test_system, region_id);
    TEST_ASSERT(result == true, "Unregistration should succeed");

    TEST_PASS("Memory region unregistration succeeded");
}

/**
 * Test: Unregister with invalid region ID (0)
 */
void test_unregister_region_invalid_id(void)
{
    printf("\n=== test_unregister_region_invalid_id ===\n");
    setup();

    bool result = bbb_unregister_memory_region(g_test_system, 0);
    TEST_ASSERT(result == false, "Unregister with ID 0 should fail");

    TEST_PASS("Invalid region ID unregistration handling correct");
}

/**
 * Test: Unregister non-existent region
 */
void test_unregister_region_nonexistent(void)
{
    printf("\n=== test_unregister_region_nonexistent ===\n");
    setup();

    bool result = bbb_unregister_memory_region(g_test_system, 999);
    TEST_ASSERT(result == false, "Unregister non-existent region should fail");

    TEST_PASS("Non-existent region unregistration handling correct");
}

/**
 * Test: Double unregistration
 */
void test_unregister_region_double(void)
{
    printf("\n=== test_unregister_region_double ===\n");
    setup();

    uint8_t buffer[1024];
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);

    bool result1 = bbb_unregister_memory_region(g_test_system, region_id);
    TEST_ASSERT(result1 == true, "First unregistration should succeed");

    bool result2 = bbb_unregister_memory_region(g_test_system, region_id);
    TEST_ASSERT(result2 == false, "Second unregistration should fail");

    TEST_PASS("Double unregistration handling correct");
}

//=============================================================================
// Unit Tests - Memory Access Checking
//=============================================================================

/**
 * Test: Check access within registered region
 */
void test_check_access_within_region(void)
{
    printf("\n=== test_check_access_within_region ===\n");
    setup();

    uint8_t buffer[1024];
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);

    /* Access at start of region */
    bool result1 = bbb_check_memory_access(g_test_system, buffer, 100, false);
    TEST_ASSERT(result1 == true, "Access at start of region should be allowed");

    /* Access in middle of region */
    bool result2 = bbb_check_memory_access(g_test_system, buffer + 500, 100, false);
    TEST_ASSERT(result2 == true, "Access in middle of region should be allowed");

    TEST_PASS("Access within registered region succeeded");
}

/**
 * Test: Check access outside registered region
 */
void test_check_access_outside_region(void)
{
    printf("\n=== test_check_access_outside_region ===\n");
    setup();

    uint8_t buffer1[1024];
    uint8_t buffer2[1024];
    bbb_register_memory_region(g_test_system, buffer1, sizeof(buffer1), false);

    /* Access to unregistered region */
    bool result = bbb_check_memory_access(g_test_system, buffer2, 100, false);
    TEST_ASSERT(result == false, "Access outside registered region should be denied");

    TEST_PASS("Access outside registered region denied");
}

/**
 * Test: Check zero-size access (valid - no access)
 */
void test_check_access_zero_size(void)
{
    printf("\n=== test_check_access_zero_size ===\n");
    setup();

    uint8_t buffer[1024];
    bool result = bbb_check_memory_access(g_test_system, buffer, 0, false);
    TEST_ASSERT(result == true, "Zero-size access should be valid");

    TEST_PASS("Zero-size access handling correct");
}

/**
 * Test: Write access to read-only region denied
 */
void test_check_access_readonly_write(void)
{
    printf("\n=== test_check_access_readonly_write ===\n");
    setup();

    uint8_t buffer[1024];
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), true);  /* Read-only */

    /* Read access should be allowed */
    bool read_result = bbb_check_memory_access(g_test_system, buffer, 100, false);
    TEST_ASSERT(read_result == true, "Read access to read-only region should be allowed");

    /* Write access should be denied */
    bool write_result = bbb_check_memory_access(g_test_system, buffer, 100, true);
    TEST_ASSERT(write_result == false, "Write access to read-only region should be denied");

    TEST_PASS("Read-only region write protection correct");
}

/**
 * Test: Partial overlap access (boundary violation)
 */
void test_check_access_partial_overlap(void)
{
    printf("\n=== test_check_access_partial_overlap ===\n");
    setup();

    uint8_t buffer[1024];
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);

    /* Access that extends past end of region */
    bool result = bbb_check_memory_access(g_test_system, buffer + 900, 200, false);
    TEST_ASSERT(result == false, "Access extending past region boundary should be denied");

    TEST_PASS("Partial overlap access denied");
}

//=============================================================================
// Unit Tests - Stack Canary
//=============================================================================

/**
 * Test: Install and verify stack canary
 */
void test_stack_canary_valid(void)
{
    printf("\n=== test_stack_canary_valid ===\n");
    setup();

    uint64_t canary_location;
    uint64_t canary = bbb_install_stack_canary(g_test_system, &canary_location);

    TEST_ASSERT(canary != 0, "Installed canary should be non-zero");
    TEST_ASSERT(canary_location == canary, "Canary should be written to location");

    bool verify_result = bbb_verify_stack_canary(g_test_system, &canary_location, canary);
    TEST_ASSERT(verify_result == true, "Valid canary should verify successfully");

    TEST_PASS("Stack canary installation and verification succeeded");
}

/**
 * Test: Verify corrupted stack canary
 */
void test_stack_canary_corrupted(void)
{
    printf("\n=== test_stack_canary_corrupted ===\n");
    setup();

    uint64_t canary_location;
    uint64_t original_canary = bbb_install_stack_canary(g_test_system, &canary_location);

    /* Corrupt the canary */
    canary_location ^= 0xFFFFFFFF;

    bool verify_result = bbb_verify_stack_canary(g_test_system, &canary_location, original_canary);
    TEST_ASSERT(verify_result == false, "Corrupted canary should fail verification");

    TEST_PASS("Corrupted stack canary detection succeeded");
}

/**
 * Test: Verify with zero expected canary
 */
void test_stack_canary_zero_expected(void)
{
    printf("\n=== test_stack_canary_zero_expected ===\n");
    setup();

    uint64_t canary_location = 0x12345678;
    bool verify_result = bbb_verify_stack_canary(g_test_system, &canary_location, 0);
    TEST_ASSERT(verify_result == false, "Zero expected canary should fail verification");

    TEST_PASS("Zero expected canary handling correct");
}

/**
 * Test: Canary uniqueness
 */
void test_stack_canary_unique(void)
{
    printf("\n=== test_stack_canary_unique ===\n");
    setup();

    uint64_t loc1, loc2, loc3;
    uint64_t canary1 = bbb_install_stack_canary(g_test_system, &loc1);
    uint64_t canary2 = bbb_install_stack_canary(g_test_system, &loc2);
    uint64_t canary3 = bbb_install_stack_canary(g_test_system, &loc3);

    /* Note: There's a small chance canaries could be equal by chance,
     * but with 64-bit random values, collision is extremely unlikely */
    bool all_unique = (canary1 != canary2) && (canary2 != canary3) && (canary1 != canary3);
    TEST_ASSERT(all_unique == true, "Canaries should be unique");

    TEST_PASS("Stack canary uniqueness verified");
}

//=============================================================================
// Unit Tests - Memory Protection
//=============================================================================

/**
 * Test: Protect memory with zero size
 */
void test_protect_memory_zero_size(void)
{
    printf("\n=== test_protect_memory_zero_size ===\n");
    setup();

    uint8_t buffer[4096];
    bool result = bbb_protect_memory(g_test_system, buffer, 0, true, false, false);
    TEST_ASSERT(result == false, "Zero-size protection should fail");

    TEST_PASS("Zero-size memory protection handling correct");
}

/**
 * Test: W^X violation (write and execute both enabled)
 * Note: This test checks the W^X enforcement logic
 */
void test_protect_memory_wx_violation(void)
{
    printf("\n=== test_protect_memory_wx_violation ===\n");
    setup();

    /* Allocate page-aligned memory for mprotect */
    void* buffer = NULL;
    size_t page_size = 4096;

    /* Try to allocate aligned memory */
    if (posix_memalign(&buffer, page_size, page_size) != 0) {
        printf("  SKIP: Could not allocate aligned memory\n");
        g_tests_passed++;
        return;
    }

    bool result = bbb_protect_memory(g_test_system, buffer, page_size, true, true, true);  /* W+X */
    free(buffer);

    TEST_ASSERT(result == false, "W^X violation should be rejected");

    TEST_PASS("W^X violation detection correct");
}

//=============================================================================
// Unit Tests - Quarantine
//=============================================================================

/**
 * Test: Quarantine memory region
 */
void test_quarantine_region_valid(void)
{
    printf("\n=== test_quarantine_region_valid ===\n");
    setup();

    uint8_t buffer[1024];
    bool result = bbb_quarantine_region(g_test_system, buffer, sizeof(buffer));
    TEST_ASSERT(result == true, "Quarantine should succeed");

    TEST_PASS("Memory region quarantine succeeded");
}

/**
 * Test: Check quarantined region
 */
void test_is_quarantined(void)
{
    printf("\n=== test_is_quarantined ===\n");
    setup();

    uint8_t buffer[1024];
    bbb_quarantine_region(g_test_system, buffer, sizeof(buffer));

    bool result = bbb_is_quarantined(g_test_system, buffer, 100);
    TEST_ASSERT(result == true, "Quarantined region should be detected");

    TEST_PASS("Quarantine detection succeeded");
}

/**
 * Test: Check non-quarantined region
 */
void test_is_not_quarantined(void)
{
    printf("\n=== test_is_not_quarantined ===\n");
    setup();

    uint8_t buffer[1024];
    bool result = bbb_is_quarantined(g_test_system, buffer, 100);
    TEST_ASSERT(result == false, "Non-quarantined region should not be detected");

    TEST_PASS("Non-quarantine detection correct");
}

/**
 * Test: Release quarantine
 */
void test_release_quarantine(void)
{
    printf("\n=== test_release_quarantine ===\n");
    setup();

    uint8_t buffer[1024];
    bbb_quarantine_region(g_test_system, buffer, sizeof(buffer));

    bool release_result = bbb_release_quarantine(g_test_system, buffer);
    TEST_ASSERT(release_result == true, "Quarantine release should succeed");

    bool still_quarantined = bbb_is_quarantined(g_test_system, buffer, 100);
    TEST_ASSERT(still_quarantined == false, "Released region should not be quarantined");

    TEST_PASS("Quarantine release succeeded");
}

/**
 * Test: Access check on quarantined region
 */
void test_access_quarantined_region(void)
{
    printf("\n=== test_access_quarantined_region ===\n");
    setup();

    uint8_t buffer[1024];

    /* Register and quarantine the region */
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);
    bbb_quarantine_region(g_test_system, buffer, sizeof(buffer));

    /* Access should be denied */
    bool result = bbb_check_memory_access(g_test_system, buffer, 100, false);
    TEST_ASSERT(result == false, "Access to quarantined region should be denied");

    TEST_PASS("Quarantined region access denied");
}

//=============================================================================
// Unit Tests - Edge Cases
//=============================================================================

/**
 * Test: Register region at memory boundary
 */
void test_register_region_boundary(void)
{
    printf("\n=== test_register_region_boundary ===\n");
    setup();

    uint8_t buffer[1];  /* Minimum size */
    uint32_t region_id = bbb_register_memory_region(g_test_system, buffer, 1, false);
    TEST_ASSERT(region_id != 0, "Single-byte region registration should succeed");

    bool result = bbb_check_memory_access(g_test_system, buffer, 1, false);
    TEST_ASSERT(result == true, "Access to single-byte region should be allowed");

    TEST_PASS("Minimum size region handling correct");
}

/**
 * Test: Access exact region boundary
 */
void test_access_exact_boundary(void)
{
    printf("\n=== test_access_exact_boundary ===\n");
    setup();

    uint8_t buffer[100];
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);

    /* Access entire region exactly */
    bool result = bbb_check_memory_access(g_test_system, buffer, sizeof(buffer), false);
    TEST_ASSERT(result == true, "Access of exact region size should be allowed");

    TEST_PASS("Exact boundary access handling correct");
}

/**
 * Test: Access one byte past boundary
 */
void test_access_one_past_boundary(void)
{
    printf("\n=== test_access_one_past_boundary ===\n");
    setup();

    uint8_t buffer[100];
    bbb_register_memory_region(g_test_system, buffer, sizeof(buffer), false);

    /* Access one byte past end */
    bool result = bbb_check_memory_access(g_test_system, buffer, sizeof(buffer) + 1, false);
    TEST_ASSERT(result == false, "Access one byte past boundary should be denied");

    TEST_PASS("One byte past boundary access denied");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("=================================================\n");
    printf("BBB Memory Boundary Unit Tests\n");
    printf("=================================================\n");

    /* NULL parameter handling tests */
    test_register_region_null_address();
    test_check_memory_access_null();
    test_protect_memory_null();
    test_install_canary_null();
    test_verify_canary_null();

    /* Memory region registration tests */
    test_register_region_valid();
    test_register_region_zero_size();
    test_register_region_readonly();
    test_register_multiple_regions();

    /* Memory region unregistration tests */
    test_unregister_region_valid();
    test_unregister_region_invalid_id();
    test_unregister_region_nonexistent();
    test_unregister_region_double();

    /* Memory access checking tests */
    test_check_access_within_region();
    test_check_access_outside_region();
    test_check_access_zero_size();
    test_check_access_readonly_write();
    test_check_access_partial_overlap();

    /* Stack canary tests */
    test_stack_canary_valid();
    test_stack_canary_corrupted();
    test_stack_canary_zero_expected();
    test_stack_canary_unique();

    /* Memory protection tests */
    test_protect_memory_zero_size();
    test_protect_memory_wx_violation();

    /* Quarantine tests */
    test_quarantine_region_valid();
    test_is_quarantined();
    test_is_not_quarantined();
    test_release_quarantine();
    test_access_quarantined_region();

    /* Edge case tests */
    test_register_region_boundary();
    test_access_exact_boundary();
    test_access_one_past_boundary();

    /* Cleanup */
    teardown();

    /* Print summary */
    printf("\n=================================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
