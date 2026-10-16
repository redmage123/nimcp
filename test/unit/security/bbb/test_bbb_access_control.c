/**
 * @file test_bbb_access_control.c
 * @brief Unit tests for BBB Access Control module
 * @date 2026-02-02
 *
 * WHAT: Comprehensive tests for access control and capability management
 * WHY:  Ensure BBB access control properly enforces security policies
 * HOW:  Test subject/object registration, capability grant/revoke, access checks
 *
 * Tests covered:
 * - NULL parameter handling
 * - Subject registration and management
 * - Object registration and management
 * - Capability grant and revoke
 * - Access check with various privilege levels
 * - Role-based access control
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

/* Access type constants */
#define BBB_ACCESS_READ    0x01
#define BBB_ACCESS_WRITE   0x02
#define BBB_ACCESS_EXECUTE 0x04

/* Capability bits */
#define BBB_CAP_READ          (1ULL << 0)
#define BBB_CAP_WRITE         (1ULL << 1)
#define BBB_CAP_EXECUTE       (1ULL << 2)
#define BBB_CAP_DELETE        (1ULL << 3)
#define BBB_CAP_CREATE        (1ULL << 4)
#define BBB_CAP_ADMIN         (1ULL << 5)

/* Role bits */
#define BBB_ROLE_READER       (1U << 0)
#define BBB_ROLE_WRITER       (1U << 1)
#define BBB_ROLE_OPERATOR     (1U << 2)
#define BBB_ROLE_ADMIN        (1U << 3)

/**
 * @brief Setup function called before each test
 *
 * WHAT: Reset ALL BBB subsystem state before each test
 * WHY:  Prevent state leakage (registrations, counters, etc.) between tests
 * HOW:  Uses the unified bbb_reset_test_state() from the public header
 *       which resets access control, memory boundary, input gate, and
 *       code signing subsystems
 */
static void setup(void)
{
    bbb_reset_test_state();
}

//=============================================================================
// Unit Tests - NULL Parameter Handling
//=============================================================================

/**
 * Test: bbb_check_access with NULL subject
 */
void test_check_access_null_subject(void)
{
    printf("\n=== test_check_access_null_subject ===\n");
    setup();

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, NULL, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == false, "bbb_check_access(NULL subject) should return false");

    TEST_PASS("NULL subject handling correct");
}

/**
 * Test: bbb_check_access with NULL object
 */
void test_check_access_null_object(void)
{
    printf("\n=== test_check_access_null_object ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_READER,
        .capabilities = BBB_CAP_READ
    };

    bool result = bbb_check_access(NULL, &subject, NULL, BBB_ACCESS_READ);
    TEST_ASSERT(result == false, "bbb_check_access(NULL object) should return false");

    TEST_PASS("NULL object handling correct");
}

/**
 * Test: bbb_register_subject with NULL subject
 */
void test_register_subject_null(void)
{
    printf("\n=== test_register_subject_null ===\n");
    setup();

    bool result = bbb_register_subject(NULL, NULL);
    TEST_ASSERT(result == false, "bbb_register_subject(NULL) should return false");

    TEST_PASS("NULL subject registration handling correct");
}

/**
 * Test: bbb_register_object with NULL object
 */
void test_register_object_null(void)
{
    printf("\n=== test_register_object_null ===\n");
    setup();

    bool result = bbb_register_object(NULL, NULL);
    TEST_ASSERT(result == false, "bbb_register_object(NULL) should return false");

    TEST_PASS("NULL object registration handling correct");
}

//=============================================================================
// Unit Tests - Subject Registration
//=============================================================================

/**
 * Test: Register a valid subject
 */
void test_register_subject_valid(void)
{
    printf("\n=== test_register_subject_valid ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 100,
        .privilege_level = 2,
        .roles = BBB_ROLE_WRITER,
        .capabilities = BBB_CAP_READ | BBB_CAP_WRITE
    };

    bool result = bbb_register_subject(NULL, &subject);
    TEST_ASSERT(result == true, "bbb_register_subject should succeed for valid subject");

    TEST_PASS("Valid subject registration succeeded");
}

/**
 * Test: Register subject with invalid ID (0)
 */
void test_register_subject_invalid_id(void)
{
    printf("\n=== test_register_subject_invalid_id ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 0,  /* Invalid - 0 is reserved */
        .privilege_level = 1,
        .roles = BBB_ROLE_READER,
        .capabilities = BBB_CAP_READ
    };

    bool result = bbb_register_subject(NULL, &subject);
    TEST_ASSERT(result == false, "bbb_register_subject should fail for ID 0");

    TEST_PASS("Invalid subject ID (0) handling correct");
}

/**
 * Test: Register duplicate subject
 */
void test_register_subject_duplicate(void)
{
    printf("\n=== test_register_subject_duplicate ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 200,
        .privilege_level = 1,
        .roles = BBB_ROLE_READER,
        .capabilities = BBB_CAP_READ
    };

    bool result1 = bbb_register_subject(NULL, &subject);
    TEST_ASSERT(result1 == true, "First registration should succeed");

    bool result2 = bbb_register_subject(NULL, &subject);
    TEST_ASSERT(result2 == false, "Duplicate registration should fail");

    TEST_PASS("Duplicate subject registration handling correct");
}

//=============================================================================
// Unit Tests - Object Registration
//=============================================================================

/**
 * Test: Register a valid object
 */
void test_register_object_valid(void)
{
    printf("\n=== test_register_object_valid ===\n");
    setup();

    bbb_object_t object = {
        .id = 100,
        .required_privilege = 1,
        .required_roles = BBB_ROLE_READER,
        .required_capabilities = BBB_CAP_READ
    };

    bool result = bbb_register_object(NULL, &object);
    TEST_ASSERT(result == true, "bbb_register_object should succeed for valid object");

    TEST_PASS("Valid object registration succeeded");
}

/**
 * Test: Register object with invalid ID (0)
 */
void test_register_object_invalid_id(void)
{
    printf("\n=== test_register_object_invalid_id ===\n");
    setup();

    bbb_object_t object = {
        .id = 0,  /* Invalid - 0 is reserved */
        .required_privilege = 1,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_register_object(NULL, &object);
    TEST_ASSERT(result == false, "bbb_register_object should fail for ID 0");

    TEST_PASS("Invalid object ID (0) handling correct");
}

/**
 * Test: Register duplicate object
 */
void test_register_object_duplicate(void)
{
    printf("\n=== test_register_object_duplicate ===\n");
    setup();

    bbb_object_t object = {
        .id = 300,
        .required_privilege = 1,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result1 = bbb_register_object(NULL, &object);
    TEST_ASSERT(result1 == true, "First registration should succeed");

    bool result2 = bbb_register_object(NULL, &object);
    TEST_ASSERT(result2 == false, "Duplicate registration should fail");

    TEST_PASS("Duplicate object registration handling correct");
}

//=============================================================================
// Unit Tests - Access Control Checks
//=============================================================================

/**
 * Test: Access allowed with matching privilege level
 */
void test_access_check_privilege_pass(void)
{
    printf("\n=== test_access_check_privilege_pass ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 3,  /* Higher than required */
        .roles = 0,
        .capabilities = BBB_CAP_READ
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 2,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == true, "Access should be allowed with higher privilege");

    TEST_PASS("Privilege level access check passed");
}

/**
 * Test: Access denied with insufficient privilege level
 */
void test_access_check_privilege_fail(void)
{
    printf("\n=== test_access_check_privilege_fail ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,  /* Lower than required */
        .roles = 0,
        .capabilities = BBB_CAP_READ
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 5,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == false, "Access should be denied with lower privilege");

    TEST_PASS("Insufficient privilege denial correct");
}

/**
 * Test: Access allowed with matching roles
 */
void test_access_check_roles_pass(void)
{
    printf("\n=== test_access_check_roles_pass ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_READER | BBB_ROLE_WRITER,
        .capabilities = 0  /* Roles provide capabilities */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = BBB_ROLE_READER,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == true, "Access should be allowed with matching roles");

    TEST_PASS("Role-based access check passed");
}

/**
 * Test: Access denied with missing roles
 */
void test_access_check_roles_fail(void)
{
    printf("\n=== test_access_check_roles_fail ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_READER,  /* Missing WRITER role */
        .capabilities = BBB_CAP_READ
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = BBB_ROLE_READER | BBB_ROLE_WRITER,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == false, "Access should be denied with missing roles");

    TEST_PASS("Missing roles denial correct");
}

/**
 * Test: Access allowed with matching capabilities
 */
void test_access_check_capabilities_pass(void)
{
    printf("\n=== test_access_check_capabilities_pass ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_EXECUTE
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = BBB_CAP_READ | BBB_CAP_WRITE
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == true, "Access should be allowed with matching capabilities");

    TEST_PASS("Capability-based access check passed");
}

/**
 * Test: Access denied with missing capabilities
 */
void test_access_check_capabilities_fail(void)
{
    printf("\n=== test_access_check_capabilities_fail ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ  /* Missing WRITE capability */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = BBB_CAP_READ | BBB_CAP_WRITE
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == false, "Access should be denied with missing capabilities");

    TEST_PASS("Missing capabilities denial correct");
}

/**
 * Test: Write access denied with read-only capability
 */
void test_access_check_write_denied(void)
{
    printf("\n=== test_access_check_write_denied ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ  /* No WRITE capability */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_WRITE);
    TEST_ASSERT(result == false, "Write access should be denied without WRITE capability");

    TEST_PASS("Write access denial without capability correct");
}

/**
 * Test: Execute access denied without execute capability
 */
void test_access_check_execute_denied(void)
{
    printf("\n=== test_access_check_execute_denied ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ | BBB_CAP_WRITE  /* No EXECUTE */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_EXECUTE);
    TEST_ASSERT(result == false, "Execute access should be denied without EXECUTE capability");

    TEST_PASS("Execute access denial without capability correct");
}

//=============================================================================
// Unit Tests - Capability Grant/Revoke
//=============================================================================

/**
 * Test: Grant capability to registered subject
 */
void test_grant_capability_valid(void)
{
    printf("\n=== test_grant_capability_valid ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 500,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = 0  /* Start with no capabilities */
    };

    bool reg_result = bbb_register_subject(NULL, &subject);
    TEST_ASSERT(reg_result == true, "Subject registration should succeed");

    bool grant_result = bbb_grant_capability(NULL, 500, BBB_CAP_READ | BBB_CAP_WRITE);
    TEST_ASSERT(grant_result == true, "Capability grant should succeed");

    TEST_PASS("Capability grant to registered subject succeeded");
}

/**
 * Test: Grant capability to unregistered subject
 */
void test_grant_capability_unregistered(void)
{
    printf("\n=== test_grant_capability_unregistered ===\n");
    setup();

    bool result = bbb_grant_capability(NULL, 999, BBB_CAP_READ);
    TEST_ASSERT(result == false, "Capability grant to unregistered subject should fail");

    TEST_PASS("Capability grant to unregistered subject handling correct");
}

/**
 * Test: Grant zero capability (no-op)
 */
void test_grant_capability_zero(void)
{
    printf("\n=== test_grant_capability_zero ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 600,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ
    };

    bbb_register_subject(NULL, &subject);

    bool result = bbb_grant_capability(NULL, 600, 0);
    TEST_ASSERT(result == true, "Granting zero capability should succeed (no-op)");

    TEST_PASS("Zero capability grant handling correct");
}

/**
 * Test: Grant capability to invalid subject ID (0)
 */
void test_grant_capability_invalid_id(void)
{
    printf("\n=== test_grant_capability_invalid_id ===\n");
    setup();

    bool result = bbb_grant_capability(NULL, 0, BBB_CAP_READ);
    TEST_ASSERT(result == false, "Capability grant to ID 0 should fail");

    TEST_PASS("Invalid subject ID for capability grant handling correct");
}

/**
 * Test: Revoke capability from registered subject
 */
void test_revoke_capability_valid(void)
{
    printf("\n=== test_revoke_capability_valid ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 700,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_EXECUTE
    };

    bbb_register_subject(NULL, &subject);

    bool result = bbb_revoke_capability(NULL, 700, BBB_CAP_WRITE);
    TEST_ASSERT(result == true, "Capability revoke should succeed");

    TEST_PASS("Capability revoke from registered subject succeeded");
}

/**
 * Test: Revoke capability from unregistered subject
 */
void test_revoke_capability_unregistered(void)
{
    printf("\n=== test_revoke_capability_unregistered ===\n");
    setup();

    bool result = bbb_revoke_capability(NULL, 888, BBB_CAP_READ);
    TEST_ASSERT(result == false, "Capability revoke from unregistered subject should fail");

    TEST_PASS("Capability revoke from unregistered subject handling correct");
}

/**
 * Test: Revoke zero capability (no-op)
 */
void test_revoke_capability_zero(void)
{
    printf("\n=== test_revoke_capability_zero ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 800,
        .privilege_level = 1,
        .roles = 0,
        .capabilities = BBB_CAP_READ
    };

    bbb_register_subject(NULL, &subject);

    bool result = bbb_revoke_capability(NULL, 800, 0);
    TEST_ASSERT(result == true, "Revoking zero capability should succeed (no-op)");

    TEST_PASS("Zero capability revoke handling correct");
}

/**
 * Test: Revoke capability from invalid subject ID (0)
 */
void test_revoke_capability_invalid_id(void)
{
    printf("\n=== test_revoke_capability_invalid_id ===\n");
    setup();

    bool result = bbb_revoke_capability(NULL, 0, BBB_CAP_READ);
    TEST_ASSERT(result == false, "Capability revoke from ID 0 should fail");

    TEST_PASS("Invalid subject ID for capability revoke handling correct");
}

//=============================================================================
// Unit Tests - Role-Based Access Control
//=============================================================================

/**
 * Test: Admin role provides all capabilities
 */
void test_admin_role_access(void)
{
    printf("\n=== test_admin_role_access ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_ADMIN,
        .capabilities = 0  /* Admin role should provide capabilities */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = BBB_CAP_READ | BBB_CAP_WRITE | BBB_CAP_EXECUTE
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_WRITE);
    TEST_ASSERT(result == true, "Admin role should provide all capabilities");

    TEST_PASS("Admin role access check passed");
}

/**
 * Test: Reader role provides read capability
 */
void test_reader_role_access(void)
{
    printf("\n=== test_reader_role_access ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_READER,
        .capabilities = 0  /* Reader role should provide read capability */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = 0
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(result == true, "Reader role should allow read access");

    TEST_PASS("Reader role access check passed");
}

/**
 * Test: Writer role provides read and write capabilities
 */
void test_writer_role_access(void)
{
    printf("\n=== test_writer_role_access ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 1,
        .roles = BBB_ROLE_WRITER,
        .capabilities = 0  /* Writer role should provide read+write */
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 0,
        .required_roles = 0,
        .required_capabilities = 0
    };

    /* Test read access */
    bool read_result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_READ);
    TEST_ASSERT(read_result == true, "Writer role should allow read access");

    /* Test write access */
    bool write_result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_WRITE);
    TEST_ASSERT(write_result == true, "Writer role should allow write access");

    TEST_PASS("Writer role access check passed");
}

//=============================================================================
// Unit Tests - Combined Access Requirements
//=============================================================================

/**
 * Test: Access with combined privilege, roles, and capabilities
 */
void test_combined_access_requirements(void)
{
    printf("\n=== test_combined_access_requirements ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 5,
        .roles = BBB_ROLE_OPERATOR,
        .capabilities = BBB_CAP_DELETE
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 3,
        .required_roles = BBB_ROLE_OPERATOR,
        .required_capabilities = BBB_CAP_DELETE
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_WRITE);
    TEST_ASSERT(result == true, "Access should be allowed when all requirements are met");

    TEST_PASS("Combined access requirements check passed");
}

/**
 * Test: Access denied when one requirement fails
 */
void test_combined_access_partial_fail(void)
{
    printf("\n=== test_combined_access_partial_fail ===\n");
    setup();

    bbb_subject_t subject = {
        .id = 1,
        .privilege_level = 5,
        .roles = BBB_ROLE_READER,  /* Missing OPERATOR role */
        .capabilities = BBB_CAP_DELETE
    };

    bbb_object_t object = {
        .id = 1,
        .required_privilege = 3,
        .required_roles = BBB_ROLE_OPERATOR,
        .required_capabilities = BBB_CAP_DELETE
    };

    bool result = bbb_check_access(NULL, &subject, &object, BBB_ACCESS_WRITE);
    TEST_ASSERT(result == false, "Access should be denied when roles requirement fails");

    TEST_PASS("Partial requirement failure correctly denies access");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("=================================================\n");
    printf("BBB Access Control Unit Tests\n");
    printf("=================================================\n");

    /* NULL parameter handling tests */
    test_check_access_null_subject();
    test_check_access_null_object();
    test_register_subject_null();
    test_register_object_null();

    /* Subject registration tests */
    test_register_subject_valid();
    test_register_subject_invalid_id();
    test_register_subject_duplicate();

    /* Object registration tests */
    test_register_object_valid();
    test_register_object_invalid_id();
    test_register_object_duplicate();

    /* Access control check tests */
    test_access_check_privilege_pass();
    test_access_check_privilege_fail();
    test_access_check_roles_pass();
    test_access_check_roles_fail();
    test_access_check_capabilities_pass();
    test_access_check_capabilities_fail();
    test_access_check_write_denied();
    test_access_check_execute_denied();

    /* Capability grant/revoke tests */
    test_grant_capability_valid();
    test_grant_capability_unregistered();
    test_grant_capability_zero();
    test_grant_capability_invalid_id();
    test_revoke_capability_valid();
    test_revoke_capability_unregistered();
    test_revoke_capability_zero();
    test_revoke_capability_invalid_id();

    /* Role-based access control tests */
    test_admin_role_access();
    test_reader_role_access();
    test_writer_role_access();

    /* Combined requirements tests */
    test_combined_access_requirements();
    test_combined_access_partial_fail();

    /* Print summary */
    printf("\n=================================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
