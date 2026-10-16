/**
 * @file test_bbb_input_gate.c
 * @brief Unit tests for BBB Input Gate module
 * @date 2026-02-02
 *
 * WHAT: Comprehensive tests for input validation and sanitization
 * WHY:  Ensure BBB input gate properly detects and blocks malicious input
 * HOW:  Test SQL injection, XSS, format string, shellcode, and sanitization
 *
 * Tests covered:
 * - NULL parameter handling
 * - SQL injection detection
 * - XSS/code injection detection
 * - Format string attack detection
 * - Shellcode detection
 * - Integer validation
 * - Pointer validation
 * - String sanitization
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

static bbb_system_t g_test_system = NULL;

/**
 * @brief Setup function called before tests
 */
static void setup(void)
{
    if (g_test_system == NULL) {
        g_test_system = bbb_system_create(NULL);
    }
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
 * Test: bbb_validate_input with NULL result
 */
void test_validate_input_null_result(void)
{
    printf("\n=== test_validate_input_null_result ===\n");
    setup();

    const char* test_data = "Valid input";
    bool result = bbb_validate_input(g_test_system, test_data, strlen(test_data), NULL);
    TEST_ASSERT(result == false, "bbb_validate_input(NULL result) should return false");

    TEST_PASS("NULL result handling correct");
}

/**
 * Test: bbb_validate_input with NULL data
 */
void test_validate_input_null_data(void)
{
    printf("\n=== test_validate_input_null_data ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_input(g_test_system, NULL, 100, &result);
    TEST_ASSERT(valid == false, "bbb_validate_input(NULL data) should return false");
    TEST_ASSERT(result.valid == false, "result.valid should be false");

    TEST_PASS("NULL data handling correct");
}

/**
 * Test: bbb_validate_string with NULL result
 */
void test_validate_string_null_result(void)
{
    printf("\n=== test_validate_string_null_result ===\n");
    setup();

    const char* test_str = "Valid string";
    bool result = bbb_validate_string(g_test_system, test_str, NULL);
    TEST_ASSERT(result == false, "bbb_validate_string(NULL result) should return false");

    TEST_PASS("NULL result for string validation handling correct");
}

/**
 * Test: bbb_validate_string with NULL string
 */
void test_validate_string_null_string(void)
{
    printf("\n=== test_validate_string_null_string ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(g_test_system, NULL, &result);
    TEST_ASSERT(valid == false, "bbb_validate_string(NULL) should return false");
    TEST_ASSERT(result.valid == false, "result.valid should be false");

    TEST_PASS("NULL string handling correct");
}

/**
 * Test: bbb_sanitize_string with NULL parameters
 */
void test_sanitize_string_null_params(void)
{
    printf("\n=== test_sanitize_string_null_params ===\n");
    setup();

    char output[128];

    /* NULL input */
    ssize_t result1 = bbb_sanitize_string(g_test_system, NULL, output, sizeof(output));
    TEST_ASSERT(result1 == -1, "bbb_sanitize_string(NULL input) should return -1");

    /* NULL output */
    ssize_t result2 = bbb_sanitize_string(g_test_system, "test", NULL, 128);
    TEST_ASSERT(result2 == -1, "bbb_sanitize_string(NULL output) should return -1");

    /* Zero output size */
    ssize_t result3 = bbb_sanitize_string(g_test_system, "test", output, 0);
    TEST_ASSERT(result3 == -1, "bbb_sanitize_string(size=0) should return -1");

    TEST_PASS("NULL parameter handling for sanitization correct");
}

//=============================================================================
// Unit Tests - Valid Input
//=============================================================================

/**
 * Test: Valid input passes validation
 */
void test_validate_input_valid(void)
{
    printf("\n=== test_validate_input_valid ===\n");
    setup();

    const char* test_data = "This is a perfectly normal string with no attacks.";
    bbb_validation_result_t result;

    bool valid = bbb_validate_input(g_test_system, test_data, strlen(test_data), &result);
    TEST_ASSERT(valid == true, "Valid input should pass validation");
    TEST_ASSERT(result.valid == true, "result.valid should be true");
    TEST_ASSERT(result.threat == BBB_THREAT_NONE, "No threat should be detected");

    TEST_PASS("Valid input validation succeeded");
}

/**
 * Test: Empty string is valid
 */
void test_validate_string_empty(void)
{
    printf("\n=== test_validate_string_empty ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_string(g_test_system, "", &result);
    TEST_ASSERT(valid == true, "Empty string should be valid");

    TEST_PASS("Empty string handling correct");
}

/**
 * Test: Zero-size input is valid
 */
void test_validate_input_zero_size(void)
{
    printf("\n=== test_validate_input_zero_size ===\n");
    setup();

    const char* test_data = "Some data";
    bbb_validation_result_t result;

    bool valid = bbb_validate_input(g_test_system, test_data, 0, &result);
    TEST_ASSERT(valid == true, "Zero-size input should be valid (nothing to validate)");

    TEST_PASS("Zero-size input handling correct");
}

//=============================================================================
// Unit Tests - SQL Injection Detection
//=============================================================================

/**
 * Test: Detect SQL UNION injection
 */
void test_detect_sql_union_injection(void)
{
    printf("\n=== test_detect_sql_union_injection ===\n");
    setup();

    const char* malicious = "admin' UNION SELECT * FROM users--";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "SQL UNION injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION, "Threat should be SQL_INJECTION");

    TEST_PASS("SQL UNION injection detection succeeded");
}

/**
 * Test: Detect SQL OR injection
 */
void test_detect_sql_or_injection(void)
{
    printf("\n=== test_detect_sql_or_injection ===\n");
    setup();

    const char* malicious = "user' OR 1=1--";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "SQL OR injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION, "Threat should be SQL_INJECTION");

    TEST_PASS("SQL OR injection detection succeeded");
}

/**
 * Test: Detect SQL DROP injection
 */
void test_detect_sql_drop_injection(void)
{
    printf("\n=== test_detect_sql_drop_injection ===\n");
    setup();

    const char* malicious = "'; DROP TABLE users;--";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "SQL DROP injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION, "Threat should be SQL_INJECTION");

    TEST_PASS("SQL DROP injection detection succeeded");
}

/**
 * Test: Detect SQL comment injection
 */
void test_detect_sql_comment_injection(void)
{
    printf("\n=== test_detect_sql_comment_injection ===\n");
    setup();

    const char* malicious = "admin'--";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "SQL comment injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_SQL_INJECTION, "Threat should be SQL_INJECTION");

    TEST_PASS("SQL comment injection detection succeeded");
}

//=============================================================================
// Unit Tests - XSS/Code Injection Detection
//=============================================================================

/**
 * Test: Detect script tag injection
 */
void test_detect_xss_script_tag(void)
{
    printf("\n=== test_detect_xss_script_tag ===\n");
    setup();

    const char* malicious = "Hello <script>alert('XSS')</script>";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Script tag injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_CODE_INJECTION, "Threat should be CODE_INJECTION");

    TEST_PASS("Script tag injection detection succeeded");
}

/**
 * Test: Detect javascript: protocol injection
 */
void test_detect_xss_javascript_protocol(void)
{
    printf("\n=== test_detect_xss_javascript_protocol ===\n");
    setup();

    const char* malicious = "Click <a href='javascript:void(0)'>here</a>";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "javascript: protocol should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_CODE_INJECTION, "Threat should be CODE_INJECTION");

    TEST_PASS("javascript: protocol injection detection succeeded");
}

/**
 * Test: Detect event handler injection
 */
void test_detect_xss_event_handler(void)
{
    printf("\n=== test_detect_xss_event_handler ===\n");
    setup();

    const char* malicious = "<img src=x onerror=alert('XSS')>";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Event handler injection should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_CODE_INJECTION, "Threat should be CODE_INJECTION");

    TEST_PASS("Event handler injection detection succeeded");
}

/**
 * Test: Detect document.cookie access
 */
void test_detect_xss_document_cookie(void)
{
    printf("\n=== test_detect_xss_document_cookie ===\n");
    setup();

    const char* malicious = "var x = document.cookie;";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "document.cookie access should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_CODE_INJECTION, "Threat should be CODE_INJECTION");

    TEST_PASS("document.cookie access detection succeeded");
}

//=============================================================================
// Unit Tests - Format String Attack Detection
//=============================================================================

/**
 * Test: Detect %n format string attack
 */
void test_detect_format_string_n(void)
{
    printf("\n=== test_detect_format_string_n ===\n");
    setup();

    const char* malicious = "AAAA%n%n%n%n";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "%%n format string should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_FORMAT_STRING, "Threat should be FORMAT_STRING");

    TEST_PASS("%%n format string attack detection succeeded");
}

/**
 * Test: Detect excessive format specifiers
 */
void test_detect_format_string_excessive(void)
{
    printf("\n=== test_detect_format_string_excessive ===\n");
    setup();

    const char* malicious = "%s%s%s%s%s%s%s%s%s%s%s%s";  /* More than 10 specifiers */
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Excessive format specifiers should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_FORMAT_STRING, "Threat should be FORMAT_STRING");

    TEST_PASS("Excessive format specifiers detection succeeded");
}

/**
 * Test: Detect dangerous width specifier
 */
void test_detect_format_string_width(void)
{
    printf("\n=== test_detect_format_string_width ===\n");
    setup();

    const char* malicious = "%.100000s";  /* Dangerous width */
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Dangerous width specifier should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_FORMAT_STRING, "Threat should be FORMAT_STRING");

    TEST_PASS("Dangerous width specifier detection succeeded");
}

/**
 * Test: Detect memory dump pattern
 */
void test_detect_format_string_dump(void)
{
    printf("\n=== test_detect_format_string_dump ===\n");
    setup();

    const char* malicious = "%08x.%08x.%08x.%08x";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Memory dump pattern should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_FORMAT_STRING, "Threat should be FORMAT_STRING");

    TEST_PASS("Memory dump pattern detection succeeded");
}

//=============================================================================
// Unit Tests - Shellcode Detection
//=============================================================================

/**
 * Test: Detect NOP sled
 */
void test_detect_shellcode_nop_sled(void)
{
    printf("\n=== test_detect_shellcode_nop_sled ===\n");
    setup();

    /* NOP sled: four or more 0x90 bytes */
    uint8_t payload[16] = {0x90, 0x90, 0x90, 0x90, 0x90, 0xCC, 0xCC, 0xCC};
    bbb_validation_result_t result;

    bool valid = bbb_validate_input(g_test_system, payload, sizeof(payload), &result);
    TEST_ASSERT(valid == false, "NOP sled should be detected");
    TEST_ASSERT(result.threat == BBB_THREAT_SHELLCODE, "Threat should be SHELLCODE");

    TEST_PASS("NOP sled detection succeeded");
}

//=============================================================================
// Unit Tests - Integer Validation
//=============================================================================

/**
 * Test: Valid integer passes validation
 */
void test_validate_integer_valid(void)
{
    printf("\n=== test_validate_integer_valid ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(g_test_system, 12345, &result);
    TEST_ASSERT(valid == true, "Normal integer should be valid");
    TEST_ASSERT(result.valid == true, "result.valid should be true");

    TEST_PASS("Valid integer validation succeeded");
}

/**
 * Test: Detect INT64_MAX boundary value
 */
void test_validate_integer_max(void)
{
    printf("\n=== test_validate_integer_max ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(g_test_system, INT64_MAX, &result);
    TEST_ASSERT(valid == false, "INT64_MAX should trigger overflow warning");
    TEST_ASSERT(result.threat == BBB_THREAT_INTEGER_OVERFLOW, "Threat should be INTEGER_OVERFLOW");

    TEST_PASS("INT64_MAX detection succeeded");
}

/**
 * Test: Detect INT64_MIN boundary value
 */
void test_validate_integer_min(void)
{
    printf("\n=== test_validate_integer_min ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_integer(g_test_system, INT64_MIN, &result);
    TEST_ASSERT(valid == false, "INT64_MIN should trigger overflow warning");
    TEST_ASSERT(result.threat == BBB_THREAT_INTEGER_OVERFLOW, "Threat should be INTEGER_OVERFLOW");

    TEST_PASS("INT64_MIN detection succeeded");
}

/**
 * Test: bbb_validate_integer with NULL result
 */
void test_validate_integer_null_result(void)
{
    printf("\n=== test_validate_integer_null_result ===\n");
    setup();

    bool valid = bbb_validate_integer(g_test_system, 100, NULL);
    TEST_ASSERT(valid == false, "bbb_validate_integer(NULL result) should return false");

    TEST_PASS("NULL result for integer validation handling correct");
}

//=============================================================================
// Unit Tests - Pointer Validation
//=============================================================================

/**
 * Test: Valid pointer passes validation
 */
void test_validate_pointer_valid(void)
{
    printf("\n=== test_validate_pointer_valid ===\n");
    setup();

    int buffer[10];
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(g_test_system, buffer, sizeof(buffer), &result);
    TEST_ASSERT(valid == true, "Valid pointer should pass validation");
    TEST_ASSERT(result.valid == true, "result.valid should be true");

    TEST_PASS("Valid pointer validation succeeded");
}

/**
 * Test: NULL pointer fails validation
 */
void test_validate_pointer_null(void)
{
    printf("\n=== test_validate_pointer_null ===\n");
    setup();

    bbb_validation_result_t result;
    bool valid = bbb_validate_pointer(g_test_system, NULL, 100, &result);
    TEST_ASSERT(valid == false, "NULL pointer should fail validation");
    TEST_ASSERT(result.threat == BBB_THREAT_MEMORY_VIOLATION, "Threat should be MEMORY_VIOLATION");

    TEST_PASS("NULL pointer detection succeeded");
}

/**
 * Test: Zero size fails validation
 */
void test_validate_pointer_zero_size(void)
{
    printf("\n=== test_validate_pointer_zero_size ===\n");
    setup();

    int buffer[10];
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(g_test_system, buffer, 0, &result);
    TEST_ASSERT(valid == false, "Zero size should fail validation");
    TEST_ASSERT(result.threat == BBB_THREAT_MEMORY_VIOLATION, "Threat should be MEMORY_VIOLATION");

    TEST_PASS("Zero size pointer validation handling correct");
}

/**
 * Test: Low address fails validation
 */
void test_validate_pointer_low_address(void)
{
    printf("\n=== test_validate_pointer_low_address ===\n");
    setup();

    void* low_ptr = (void*)0x100;  /* Very low address */
    bbb_validation_result_t result;

    bool valid = bbb_validate_pointer(g_test_system, low_ptr, 100, &result);
    TEST_ASSERT(valid == false, "Low address pointer should fail validation");
    TEST_ASSERT(result.threat == BBB_THREAT_MEMORY_VIOLATION, "Threat should be MEMORY_VIOLATION");

    TEST_PASS("Low address pointer detection succeeded");
}

//=============================================================================
// Unit Tests - String Sanitization
//=============================================================================

/**
 * Test: Sanitize clean string
 */
void test_sanitize_string_clean(void)
{
    printf("\n=== test_sanitize_string_clean ===\n");
    setup();

    const char* input = "Hello World 123!";
    char output[64];

    ssize_t len = bbb_sanitize_string(g_test_system, input, output, sizeof(output));
    TEST_ASSERT(len > 0, "Sanitization should return positive length");
    TEST_ASSERT(strcmp(output, input) == 0, "Clean string should be unchanged");

    TEST_PASS("Clean string sanitization succeeded");
}

/**
 * Test: Sanitize string with dangerous characters
 */
void test_sanitize_string_dangerous(void)
{
    printf("\n=== test_sanitize_string_dangerous ===\n");
    setup();

    const char* input = "Hello<script>alert('XSS')</script>";
    char output[64];

    ssize_t len = bbb_sanitize_string(g_test_system, input, output, sizeof(output));
    TEST_ASSERT(len > 0, "Sanitization should succeed");
    TEST_ASSERT(strstr(output, "<") == NULL, "< should be removed");
    TEST_ASSERT(strstr(output, ">") == NULL, "> should be removed");
    TEST_ASSERT(strstr(output, "'") == NULL, "' should be removed");

    TEST_PASS("Dangerous character sanitization succeeded");
}

/**
 * Test: Sanitize string with SQL characters
 */
void test_sanitize_string_sql(void)
{
    printf("\n=== test_sanitize_string_sql ===\n");
    setup();

    const char* input = "admin'; DROP TABLE users;--";
    char output[64];

    ssize_t len = bbb_sanitize_string(g_test_system, input, output, sizeof(output));
    TEST_ASSERT(len > 0, "Sanitization should succeed");
    TEST_ASSERT(strstr(output, "'") == NULL, "' should be removed");
    TEST_ASSERT(strstr(output, ";") == NULL, "; should be removed");

    TEST_PASS("SQL character sanitization succeeded");
}

/**
 * Test: Sanitize string preserves alphanumeric
 */
void test_sanitize_string_alphanumeric(void)
{
    printf("\n=== test_sanitize_string_alphanumeric ===\n");
    setup();

    const char* input = "ABCdef123";
    char output[64];

    ssize_t len = bbb_sanitize_string(g_test_system, input, output, sizeof(output));
    TEST_ASSERT(len == (ssize_t)strlen(input), "Length should match");
    TEST_ASSERT(strcmp(output, input) == 0, "Alphanumeric should be preserved");

    TEST_PASS("Alphanumeric preservation succeeded");
}

/**
 * Test: Sanitize string with small buffer
 */
void test_sanitize_string_small_buffer(void)
{
    printf("\n=== test_sanitize_string_small_buffer ===\n");
    setup();

    const char* input = "This is a very long string that won't fit";
    char output[10];

    ssize_t len = bbb_sanitize_string(g_test_system, input, output, sizeof(output));
    TEST_ASSERT(len > 0, "Sanitization should succeed with truncation");
    TEST_ASSERT(len < (ssize_t)strlen(input), "Output should be truncated");
    TEST_ASSERT(strlen(output) < sizeof(output), "Output should be null-terminated");

    TEST_PASS("Small buffer sanitization handling correct");
}

//=============================================================================
// Unit Tests - Case Insensitive Detection
//=============================================================================

/**
 * Test: Detect mixed case SQL injection
 */
void test_detect_sql_mixed_case(void)
{
    printf("\n=== test_detect_sql_mixed_case ===\n");
    setup();

    const char* malicious = "admin' UnIoN SeLeCt * FROM users--";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Mixed case SQL injection should be detected");

    TEST_PASS("Mixed case SQL injection detection succeeded");
}

/**
 * Test: Detect mixed case XSS
 */
void test_detect_xss_mixed_case(void)
{
    printf("\n=== test_detect_xss_mixed_case ===\n");
    setup();

    const char* malicious = "Hello <ScRiPt>alert('XSS')</sCrIpT>";
    bbb_validation_result_t result;

    bool valid = bbb_validate_string(g_test_system, malicious, &result);
    TEST_ASSERT(valid == false, "Mixed case XSS should be detected");

    TEST_PASS("Mixed case XSS detection succeeded");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("=================================================\n");
    printf("BBB Input Gate Unit Tests\n");
    printf("=================================================\n");

    /* NULL parameter handling tests */
    test_validate_input_null_result();
    test_validate_input_null_data();
    test_validate_string_null_result();
    test_validate_string_null_string();
    test_sanitize_string_null_params();

    /* Valid input tests */
    test_validate_input_valid();
    test_validate_string_empty();
    test_validate_input_zero_size();

    /* SQL injection detection tests */
    test_detect_sql_union_injection();
    test_detect_sql_or_injection();
    test_detect_sql_drop_injection();
    test_detect_sql_comment_injection();

    /* XSS/code injection detection tests */
    test_detect_xss_script_tag();
    test_detect_xss_javascript_protocol();
    test_detect_xss_event_handler();
    test_detect_xss_document_cookie();

    /* Format string attack detection tests */
    test_detect_format_string_n();
    test_detect_format_string_excessive();
    test_detect_format_string_width();
    test_detect_format_string_dump();

    /* Shellcode detection tests */
    test_detect_shellcode_nop_sled();

    /* Integer validation tests */
    test_validate_integer_valid();
    test_validate_integer_max();
    test_validate_integer_min();
    test_validate_integer_null_result();

    /* Pointer validation tests */
    test_validate_pointer_valid();
    test_validate_pointer_null();
    test_validate_pointer_zero_size();
    test_validate_pointer_low_address();

    /* String sanitization tests */
    test_sanitize_string_clean();
    test_sanitize_string_dangerous();
    test_sanitize_string_sql();
    test_sanitize_string_alphanumeric();
    test_sanitize_string_small_buffer();

    /* Case insensitive detection tests */
    test_detect_sql_mixed_case();
    test_detect_xss_mixed_case();

    /* Cleanup */
    teardown();

    /* Print summary */
    printf("\n=================================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
