/**
 * @file test_bbb_helpers.c
 * @brief Unit tests for BBB Helpers module
 * @date 2026-02-02
 *
 * WHAT: Comprehensive tests for BBB helper utility functions
 * WHY:  Ensure BBB helpers provide correct simplified security validation
 * HOW:  Test initialization, validation helpers, threat detection, statistics
 *
 * Tests covered:
 * - Initialization and shutdown
 * - Module registration
 * - Pointer validation (bbb_check_pointer)
 * - String validation (bbb_check_string)
 * - Range validation (bbb_validate_range, bbb_validate_range_u)
 * - Buffer access validation
 * - Network data validation
 * - Threat detection
 * - Message integrity
 * - Privileged operations
 * - Statistics tracking
 * - Edge cases and boundary conditions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "security/nimcp_bbb_helpers.h"

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

/**
 * @brief Setup function called before each test
 */
static void setup(void)
{
    bbb_reset_statistics();
}

//=============================================================================
// Unit Tests - Initialization
//=============================================================================

/**
 * Test: Initialize BBB helpers
 */
void test_helpers_init(void)
{
    printf("\n=== test_helpers_init ===\n");

    bool result = bbb_helpers_init();
    TEST_ASSERT(result == true, "bbb_helpers_init should succeed");
    TEST_ASSERT(bbb_helpers_is_initialized() == true, "Helpers should be initialized");

    TEST_PASS("BBB helpers initialization succeeded");
}

/**
 * Test: Double initialization (should be idempotent)
 */
void test_helpers_double_init(void)
{
    printf("\n=== test_helpers_double_init ===\n");

    bool result1 = bbb_helpers_init();
    bool result2 = bbb_helpers_init();

    TEST_ASSERT(result1 == true, "First init should succeed");
    TEST_ASSERT(result2 == true, "Second init should also succeed (idempotent)");

    TEST_PASS("Double initialization handling correct");
}

/**
 * Test: Check initialization status
 */
void test_helpers_is_initialized(void)
{
    printf("\n=== test_helpers_is_initialized ===\n");

    bbb_helpers_init();
    bool result = bbb_helpers_is_initialized();
    TEST_ASSERT(result == true, "bbb_helpers_is_initialized should return true after init");

    TEST_PASS("Initialization status check correct");
}

//=============================================================================
// Unit Tests - Module Registration
//=============================================================================

/**
 * Test: Register module
 */
void test_register_module_valid(void)
{
    printf("\n=== test_register_module_valid ===\n");
    setup();

    bool result = bbb_register_module("test_module", BBB_MODULE_TYPE_CORE);
    TEST_ASSERT(result == true, "Module registration should succeed");

    TEST_PASS("Module registration succeeded");
}

/**
 * Test: Register module with NULL name
 */
void test_register_module_null(void)
{
    printf("\n=== test_register_module_null ===\n");
    setup();

    bool result = bbb_register_module(NULL, BBB_MODULE_TYPE_CORE);
    TEST_ASSERT(result == false, "Module registration with NULL name should fail");

    TEST_PASS("NULL module name handling correct");
}

/**
 * Test: Unregister module
 */
void test_unregister_module_valid(void)
{
    printf("\n=== test_unregister_module_valid ===\n");
    setup();

    bbb_register_module("test_module", BBB_MODULE_TYPE_CORE);
    bool result = bbb_unregister_module("test_module");
    TEST_ASSERT(result == true, "Module unregistration should succeed");

    TEST_PASS("Module unregistration succeeded");
}

/**
 * Test: Unregister module with NULL name
 */
void test_unregister_module_null(void)
{
    printf("\n=== test_unregister_module_null ===\n");
    setup();

    bool result = bbb_unregister_module(NULL);
    TEST_ASSERT(result == false, "Module unregistration with NULL name should fail");

    TEST_PASS("NULL module unregistration handling correct");
}

/**
 * Test: Register different module types
 */
void test_register_module_types(void)
{
    printf("\n=== test_register_module_types ===\n");
    setup();

    bool r1 = bbb_register_module("cognitive", BBB_MODULE_TYPE_COGNITIVE);
    bool r2 = bbb_register_module("swarm", BBB_MODULE_TYPE_SWARM);
    bool r3 = bbb_register_module("platform", BBB_MODULE_TYPE_PLATFORM);
    bool r4 = bbb_register_module("network", BBB_MODULE_TYPE_NETWORK);
    bool r5 = bbb_register_module("plasticity", BBB_MODULE_TYPE_PLASTICITY);
    bool r6 = bbb_register_module("middleware", BBB_MODULE_TYPE_MIDDLEWARE);

    TEST_ASSERT(r1 && r2 && r3 && r4 && r5 && r6, "All module types should register");

    TEST_PASS("All module types registered successfully");
}

//=============================================================================
// Unit Tests - Pointer Validation
//=============================================================================

/**
 * Test: Valid pointer passes validation
 */
void test_check_pointer_valid(void)
{
    printf("\n=== test_check_pointer_valid ===\n");
    setup();

    int buffer[10];
    bool result = bbb_check_pointer(buffer, "test_func");
    TEST_ASSERT(result == true, "Valid pointer should pass validation");

    TEST_PASS("Valid pointer validation succeeded");
}

/**
 * Test: NULL pointer fails validation
 */
void test_check_pointer_null(void)
{
    printf("\n=== test_check_pointer_null ===\n");
    setup();

    bool result = bbb_check_pointer(NULL, "test_func");
    TEST_ASSERT(result == false, "NULL pointer should fail validation");

    TEST_PASS("NULL pointer detection succeeded");
}

/**
 * Test: NULL function name handled gracefully
 */
void test_check_pointer_null_funcname(void)
{
    printf("\n=== test_check_pointer_null_funcname ===\n");
    setup();

    int buffer[10];
    bool result = bbb_check_pointer(buffer, NULL);
    TEST_ASSERT(result == true, "Valid pointer with NULL function name should pass");

    TEST_PASS("NULL function name handling correct");
}

//=============================================================================
// Unit Tests - String Validation
//=============================================================================

/**
 * Test: Valid string passes validation
 */
void test_check_string_valid(void)
{
    printf("\n=== test_check_string_valid ===\n");
    setup();

    const char* str = "Hello World";
    bool result = bbb_check_string(str, 100, "test_func");
    TEST_ASSERT(result == true, "Valid string should pass validation");

    TEST_PASS("Valid string validation succeeded");
}

/**
 * Test: NULL string fails validation
 */
void test_check_string_null(void)
{
    printf("\n=== test_check_string_null ===\n");
    setup();

    bool result = bbb_check_string(NULL, 100, "test_func");
    TEST_ASSERT(result == false, "NULL string should fail validation");

    TEST_PASS("NULL string detection succeeded");
}

/**
 * Test: String exceeding max length fails
 */
void test_check_string_too_long(void)
{
    printf("\n=== test_check_string_too_long ===\n");
    setup();

    const char* str = "This is a long string";
    bool result = bbb_check_string(str, 5, "test_func");  /* Max 5 chars */
    TEST_ASSERT(result == false, "String exceeding max length should fail");

    TEST_PASS("Long string detection succeeded");
}

/**
 * Test: Empty string passes validation
 */
void test_check_string_empty(void)
{
    printf("\n=== test_check_string_empty ===\n");
    setup();

    bool result = bbb_check_string("", 100, "test_func");
    TEST_ASSERT(result == true, "Empty string should pass validation");

    TEST_PASS("Empty string validation succeeded");
}

/**
 * Test: String at exact max length
 */
void test_check_string_exact_length(void)
{
    printf("\n=== test_check_string_exact_length ===\n");
    setup();

    const char* str = "12345";  /* 5 chars */
    bool result = bbb_check_string(str, 5, "test_func");
    TEST_ASSERT(result == true, "String at exact max length should pass");

    TEST_PASS("Exact length string validation succeeded");
}

//=============================================================================
// Unit Tests - Range Validation
//=============================================================================

/**
 * Test: Value within range passes
 */
void test_validate_range_within(void)
{
    printf("\n=== test_validate_range_within ===\n");
    setup();

    bool result = bbb_validate_range(50, 0, 100, "test_func");
    TEST_ASSERT(result == true, "Value within range should pass");

    TEST_PASS("Within range validation succeeded");
}

/**
 * Test: Value below range fails
 */
void test_validate_range_below(void)
{
    printf("\n=== test_validate_range_below ===\n");
    setup();

    bool result = bbb_validate_range(-10, 0, 100, "test_func");
    TEST_ASSERT(result == false, "Value below range should fail");

    TEST_PASS("Below range detection succeeded");
}

/**
 * Test: Value above range fails
 */
void test_validate_range_above(void)
{
    printf("\n=== test_validate_range_above ===\n");
    setup();

    bool result = bbb_validate_range(150, 0, 100, "test_func");
    TEST_ASSERT(result == false, "Value above range should fail");

    TEST_PASS("Above range detection succeeded");
}

/**
 * Test: Value at min boundary passes
 */
void test_validate_range_at_min(void)
{
    printf("\n=== test_validate_range_at_min ===\n");
    setup();

    bool result = bbb_validate_range(0, 0, 100, "test_func");
    TEST_ASSERT(result == true, "Value at min boundary should pass");

    TEST_PASS("Min boundary validation succeeded");
}

/**
 * Test: Value at max boundary passes
 */
void test_validate_range_at_max(void)
{
    printf("\n=== test_validate_range_at_max ===\n");
    setup();

    bool result = bbb_validate_range(100, 0, 100, "test_func");
    TEST_ASSERT(result == true, "Value at max boundary should pass");

    TEST_PASS("Max boundary validation succeeded");
}

/**
 * Test: Unsigned range validation
 */
void test_validate_range_u_valid(void)
{
    printf("\n=== test_validate_range_u_valid ===\n");
    setup();

    bool result = bbb_validate_range_u(500, 0, 1000, "test_func");
    TEST_ASSERT(result == true, "Unsigned value within range should pass");

    TEST_PASS("Unsigned range validation succeeded");
}

/**
 * Test: Unsigned range validation out of range
 */
void test_validate_range_u_above(void)
{
    printf("\n=== test_validate_range_u_above ===\n");
    setup();

    bool result = bbb_validate_range_u(2000, 0, 1000, "test_func");
    TEST_ASSERT(result == false, "Unsigned value above range should fail");

    TEST_PASS("Unsigned above range detection succeeded");
}

//=============================================================================
// Unit Tests - Buffer Access Validation
//=============================================================================

/**
 * Test: Valid buffer access
 */
void test_validate_buffer_access_valid(void)
{
    printf("\n=== test_validate_buffer_access_valid ===\n");
    setup();

    uint8_t buffer[100];
    bool result = bbb_validate_buffer_access(buffer, 10, 20, 100, "test_func");
    TEST_ASSERT(result == true, "Valid buffer access should pass");

    TEST_PASS("Valid buffer access validation succeeded");
}

/**
 * Test: NULL buffer fails
 */
void test_validate_buffer_access_null(void)
{
    printf("\n=== test_validate_buffer_access_null ===\n");
    setup();

    bool result = bbb_validate_buffer_access(NULL, 0, 10, 100, "test_func");
    TEST_ASSERT(result == false, "NULL buffer should fail");

    TEST_PASS("NULL buffer detection succeeded");
}

/**
 * Test: Offset overflow detection
 */
void test_validate_buffer_access_offset_overflow(void)
{
    printf("\n=== test_validate_buffer_access_offset_overflow ===\n");
    setup();

    uint8_t buffer[100];
    bool result = bbb_validate_buffer_access(buffer, 90, 20, 100, "test_func");
    TEST_ASSERT(result == false, "Offset + size overflow should fail");

    TEST_PASS("Offset overflow detection succeeded");
}

/**
 * Test: Offset beyond buffer
 */
void test_validate_buffer_access_offset_beyond(void)
{
    printf("\n=== test_validate_buffer_access_offset_beyond ===\n");
    setup();

    uint8_t buffer[100];
    bool result = bbb_validate_buffer_access(buffer, 150, 10, 100, "test_func");
    TEST_ASSERT(result == false, "Offset beyond buffer should fail");

    TEST_PASS("Beyond buffer offset detection succeeded");
}

/**
 * Test: Access at exact boundary
 */
void test_validate_buffer_access_boundary(void)
{
    printf("\n=== test_validate_buffer_access_boundary ===\n");
    setup();

    uint8_t buffer[100];
    bool result = bbb_validate_buffer_access(buffer, 90, 10, 100, "test_func");
    TEST_ASSERT(result == true, "Access at exact boundary should pass");

    TEST_PASS("Exact boundary access validation succeeded");
}

//=============================================================================
// Unit Tests - Network Data Validation
//=============================================================================

/**
 * Test: Valid network data
 */
void test_validate_network_data_valid(void)
{
    printf("\n=== test_validate_network_data_valid ===\n");
    setup();

    uint8_t data[1024];
    bool result = bbb_validate_network_data(data, sizeof(data), "test_func");
    TEST_ASSERT(result == true, "Valid network data should pass");

    TEST_PASS("Valid network data validation succeeded");
}

/**
 * Test: NULL network data
 */
void test_validate_network_data_null(void)
{
    printf("\n=== test_validate_network_data_null ===\n");
    setup();

    bool result = bbb_validate_network_data(NULL, 100, "test_func");
    TEST_ASSERT(result == false, "NULL network data should fail");

    TEST_PASS("NULL network data detection succeeded");
}

/**
 * Test: Zero-length network data
 */
void test_validate_network_data_zero(void)
{
    printf("\n=== test_validate_network_data_zero ===\n");
    setup();

    uint8_t data[100];
    bool result = bbb_validate_network_data(data, 0, "test_func");
    TEST_ASSERT(result == false, "Zero-length network data should fail");

    TEST_PASS("Zero-length network data detection succeeded");
}

/**
 * Test: Oversized network data
 */
void test_validate_network_data_oversized(void)
{
    printf("\n=== test_validate_network_data_oversized ===\n");
    setup();

    uint8_t data[100];
    bool result = bbb_validate_network_data(data, 100000, "test_func");  /* > 65536 */
    TEST_ASSERT(result == false, "Oversized network data should fail");

    TEST_PASS("Oversized network data detection succeeded");
}

//=============================================================================
// Unit Tests - Threat Detection
//=============================================================================

/**
 * Test: Detect no threat in clean data
 */
void test_detect_threat_none(void)
{
    printf("\n=== test_detect_threat_none ===\n");
    setup();

    const char* data = "This is clean data with no threats";
    bbb_threat_type_t threat = bbb_detect_threat(data, strlen(data));
    TEST_ASSERT(threat == BBB_THREAT_NONE, "Clean data should have no threats");

    TEST_PASS("No threat detection correct");
}

/**
 * Test: Detect NOP sled shellcode
 */
void test_detect_threat_shellcode(void)
{
    printf("\n=== test_detect_threat_shellcode ===\n");
    setup();

    uint8_t data[10] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00};
    bbb_threat_type_t threat = bbb_detect_threat(data, sizeof(data));
    TEST_ASSERT(threat == BBB_THREAT_SHELLCODE, "NOP sled should be detected as shellcode");

    TEST_PASS("Shellcode detection succeeded");
}

/**
 * Test: Detect SQL injection pattern
 */
void test_detect_threat_sql_injection(void)
{
    printf("\n=== test_detect_threat_sql_injection ===\n");
    setup();

    const char* data = "user' OR 1=1--";
    bbb_threat_type_t threat = bbb_detect_threat(data, strlen(data) + 1);
    TEST_ASSERT(threat == BBB_THREAT_SQL_INJECTION, "SQL injection should be detected");

    TEST_PASS("SQL injection detection succeeded");
}

/**
 * Test: Detect format string attack
 */
void test_detect_threat_format_string(void)
{
    printf("\n=== test_detect_threat_format_string ===\n");
    setup();

    const char* data = "%s%s%s%s%n";
    bbb_threat_type_t threat = bbb_detect_threat(data, strlen(data) + 1);
    TEST_ASSERT(threat == BBB_THREAT_FORMAT_STRING, "Format string should be detected");

    TEST_PASS("Format string detection succeeded");
}

/**
 * Test: Detect threat with NULL data
 */
void test_detect_threat_null(void)
{
    printf("\n=== test_detect_threat_null ===\n");
    setup();

    bbb_threat_type_t threat = bbb_detect_threat(NULL, 100);
    TEST_ASSERT(threat == BBB_THREAT_NONE, "NULL data should return THREAT_NONE");

    TEST_PASS("NULL data threat detection handling correct");
}

/**
 * Test: Detect threat with zero length
 */
void test_detect_threat_zero_length(void)
{
    printf("\n=== test_detect_threat_zero_length ===\n");
    setup();

    const char* data = "Some data";
    bbb_threat_type_t threat = bbb_detect_threat(data, 0);
    TEST_ASSERT(threat == BBB_THREAT_NONE, "Zero-length data should return THREAT_NONE");

    TEST_PASS("Zero-length threat detection handling correct");
}

//=============================================================================
// Unit Tests - Message Integrity
//=============================================================================

/**
 * Test: Valid message integrity
 */
void test_verify_message_integrity_valid(void)
{
    printf("\n=== test_verify_message_integrity_valid ===\n");
    setup();

    const char* message = "This is a valid message without threats";
    bool result = bbb_verify_message_integrity(message, strlen(message));
    TEST_ASSERT(result == true, "Valid message should pass integrity check");

    TEST_PASS("Valid message integrity verification succeeded");
}

/**
 * Test: NULL message integrity
 */
void test_verify_message_integrity_null(void)
{
    printf("\n=== test_verify_message_integrity_null ===\n");
    setup();

    bool result = bbb_verify_message_integrity(NULL, 100);
    TEST_ASSERT(result == false, "NULL message should fail integrity check");

    TEST_PASS("NULL message integrity check handling correct");
}

/**
 * Test: Zero-length message integrity
 */
void test_verify_message_integrity_zero(void)
{
    printf("\n=== test_verify_message_integrity_zero ===\n");
    setup();

    const char* message = "Some message";
    bool result = bbb_verify_message_integrity(message, 0);
    TEST_ASSERT(result == false, "Zero-length message should fail integrity check");

    TEST_PASS("Zero-length message integrity check handling correct");
}

/**
 * Test: Oversized message integrity
 */
void test_verify_message_integrity_oversized(void)
{
    printf("\n=== test_verify_message_integrity_oversized ===\n");
    setup();

    const char* message = "Some message";
    bool result = bbb_verify_message_integrity(message, 2000000);  /* > 1MB */
    TEST_ASSERT(result == false, "Oversized message should fail integrity check");

    TEST_PASS("Oversized message integrity check handling correct");
}

//=============================================================================
// Unit Tests - Privileged Operations
//=============================================================================

/**
 * Test: Validate privileged operation
 */
void test_validate_privileged_operation_valid(void)
{
    printf("\n=== test_validate_privileged_operation_valid ===\n");
    setup();

    int operation_data = 42;
    bool result = bbb_validate_privileged_operation(&operation_data, BBB_PRIV_PUBLIC);
    TEST_ASSERT(result == true, "Valid privileged operation should pass");

    TEST_PASS("Privileged operation validation succeeded");
}

/**
 * Test: Validate privileged operation with NULL data
 */
void test_validate_privileged_operation_null(void)
{
    printf("\n=== test_validate_privileged_operation_null ===\n");
    setup();

    bool result = bbb_validate_privileged_operation(NULL, BBB_PRIV_PUBLIC);
    TEST_ASSERT(result == false, "NULL operation data should fail");

    TEST_PASS("NULL privileged operation handling correct");
}

//=============================================================================
// Unit Tests - Statistics
//=============================================================================

/**
 * Test: Statistics after validations
 */
void test_statistics_validations(void)
{
    printf("\n=== test_statistics_validations ===\n");

    bbb_reset_statistics();

    /* Perform several validations */
    int buffer[10];
    bbb_check_pointer(buffer, "test");
    bbb_check_pointer(buffer, "test");
    bbb_check_pointer(buffer, "test");

    uint64_t validations = bbb_get_validations_performed();
    TEST_ASSERT(validations >= 3, "Validation count should be at least 3");

    TEST_PASS("Validation statistics tracking correct");
}

/**
 * Test: Statistics for threats detected
 */
void test_statistics_threats(void)
{
    printf("\n=== test_statistics_threats ===\n");

    bbb_reset_statistics();

    /* Cause threat detections */
    bbb_check_pointer(NULL, "test");
    bbb_check_pointer(NULL, "test");

    uint64_t threats = bbb_get_threats_detected();
    TEST_ASSERT(threats >= 2, "Threat count should be at least 2");

    TEST_PASS("Threat statistics tracking correct");
}

/**
 * Test: Reset statistics
 */
void test_reset_statistics(void)
{
    printf("\n=== test_reset_statistics ===\n");

    /* Generate some statistics */
    int buffer[10];
    bbb_check_pointer(buffer, "test");
    bbb_check_pointer(NULL, "test");

    /* Reset */
    bbb_reset_statistics();

    uint64_t validations = bbb_get_validations_performed();
    uint64_t threats = bbb_get_threats_detected();

    TEST_ASSERT(validations == 0, "Validations should be 0 after reset");
    TEST_ASSERT(threats == 0, "Threats should be 0 after reset");

    TEST_PASS("Statistics reset succeeded");
}

//=============================================================================
// Unit Tests - Audit Logging
//=============================================================================

/**
 * Test: Audit log with various levels
 */
void test_audit_log_levels(void)
{
    printf("\n=== test_audit_log_levels ===\n");
    setup();

    /* These should not crash - we can't easily verify output */
    bbb_audit_log(BBB_AUDIT_DEBUG, "test_module", "debug_event", "Debug message %d", 1);
    bbb_audit_log(BBB_AUDIT_INFO, "test_module", "info_event", "Info message %d", 2);
    bbb_audit_log(BBB_AUDIT_WARNING, "test_module", "warning_event", "Warning message %d", 3);
    bbb_audit_log(BBB_AUDIT_ERROR, "test_module", "error_event", "Error message %d", 4);
    bbb_audit_log(BBB_AUDIT_CRITICAL, "test_module", "critical_event", "Critical message %d", 5);

    TEST_PASS("Audit logging at all levels succeeded");
}

/**
 * Test: Audit log with NULL parameters
 */
void test_audit_log_null_params(void)
{
    printf("\n=== test_audit_log_null_params ===\n");
    setup();

    /* Should not crash with NULL parameters */
    bbb_audit_log(BBB_AUDIT_INFO, NULL, "event", "Message");
    bbb_audit_log(BBB_AUDIT_INFO, "module", NULL, "Message");

    TEST_PASS("NULL parameter audit logging handling correct");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("=================================================\n");
    printf("BBB Helpers Unit Tests\n");
    printf("=================================================\n");

    /* Initialize helpers for all tests */
    bbb_helpers_init();

    /* Initialization tests */
    test_helpers_init();
    test_helpers_double_init();
    test_helpers_is_initialized();

    /* Module registration tests */
    test_register_module_valid();
    test_register_module_null();
    test_unregister_module_valid();
    test_unregister_module_null();
    test_register_module_types();

    /* Pointer validation tests */
    test_check_pointer_valid();
    test_check_pointer_null();
    test_check_pointer_null_funcname();

    /* String validation tests */
    test_check_string_valid();
    test_check_string_null();
    test_check_string_too_long();
    test_check_string_empty();
    test_check_string_exact_length();

    /* Range validation tests */
    test_validate_range_within();
    test_validate_range_below();
    test_validate_range_above();
    test_validate_range_at_min();
    test_validate_range_at_max();
    test_validate_range_u_valid();
    test_validate_range_u_above();

    /* Buffer access validation tests */
    test_validate_buffer_access_valid();
    test_validate_buffer_access_null();
    test_validate_buffer_access_offset_overflow();
    test_validate_buffer_access_offset_beyond();
    test_validate_buffer_access_boundary();

    /* Network data validation tests */
    test_validate_network_data_valid();
    test_validate_network_data_null();
    test_validate_network_data_zero();
    test_validate_network_data_oversized();

    /* Threat detection tests */
    test_detect_threat_none();
    test_detect_threat_shellcode();
    test_detect_threat_sql_injection();
    test_detect_threat_format_string();
    test_detect_threat_null();
    test_detect_threat_zero_length();

    /* Message integrity tests */
    test_verify_message_integrity_valid();
    test_verify_message_integrity_null();
    test_verify_message_integrity_zero();
    test_verify_message_integrity_oversized();

    /* Privileged operation tests */
    test_validate_privileged_operation_valid();
    test_validate_privileged_operation_null();

    /* Statistics tests */
    test_statistics_validations();
    test_statistics_threats();
    test_reset_statistics();

    /* Audit logging tests */
    test_audit_log_levels();
    test_audit_log_null_params();

    /* Shutdown helpers */
    bbb_helpers_shutdown();

    /* Print summary */
    printf("\n=================================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
