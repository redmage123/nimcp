/**
 * @file test_bbb_code_signing.c
 * @brief Unit tests for BBB Code Signing module
 * @date 2026-02-02
 *
 * WHAT: Comprehensive tests for code signing and signature verification
 * WHY:  Ensure BBB code signing properly validates code integrity
 * HOW:  Test signing, verification, key management, and hash calculation
 *
 * Tests covered:
 * - NULL parameter handling
 * - Signing key configuration
 * - Code signing
 * - Signature verification
 * - Tampered data detection
 * - Hash calculation
 * - Trusted key management
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

/* Test signing key - 32 bytes for testing */
static const uint8_t TEST_SIGNING_KEY[32] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

/* Signature size constant */
#define SIGNATURE_SIZE 32

/**
 * @brief Setup function called before each test
 */
static void setup(void)
{
    bbb_clear_signing_key();
}

/**
 * @brief Setup with signing key configured
 */
static void setup_with_key(void)
{
    bbb_clear_signing_key();
    bbb_set_signing_key(TEST_SIGNING_KEY, sizeof(TEST_SIGNING_KEY));
}

//=============================================================================
// Unit Tests - NULL Parameter Handling
//=============================================================================

/**
 * Test: bbb_sign_code with NULL data
 */
void test_sign_code_null_data(void)
{
    printf("\n=== test_sign_code_null_data ===\n");
    setup_with_key();

    uint8_t signature[SIGNATURE_SIZE];
    ssize_t result = bbb_sign_code(NULL, NULL, 100, signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == -1, "bbb_sign_code(NULL data) should return -1");

    TEST_PASS("NULL data handling correct");
}

/**
 * Test: bbb_sign_code with NULL signature buffer
 */
void test_sign_code_null_signature(void)
{
    printf("\n=== test_sign_code_null_signature ===\n");
    setup_with_key();

    const char* test_data = "Test code data";
    ssize_t result = bbb_sign_code(NULL, test_data, strlen(test_data), NULL, SIGNATURE_SIZE);
    TEST_ASSERT(result == -1, "bbb_sign_code(NULL signature) should return -1");

    TEST_PASS("NULL signature buffer handling correct");
}

/**
 * Test: bbb_verify_signature with NULL data
 */
void test_verify_signature_null_data(void)
{
    printf("\n=== test_verify_signature_null_data ===\n");
    setup_with_key();

    uint8_t signature[SIGNATURE_SIZE] = {0};
    bool result = bbb_verify_signature(NULL, NULL, 100, signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == false, "bbb_verify_signature(NULL data) should return false");

    TEST_PASS("NULL data verification handling correct");
}

/**
 * Test: bbb_verify_signature with NULL signature
 */
void test_verify_signature_null_signature(void)
{
    printf("\n=== test_verify_signature_null_signature ===\n");
    setup_with_key();

    const char* test_data = "Test code data";
    bool result = bbb_verify_signature(NULL, test_data, strlen(test_data), NULL, SIGNATURE_SIZE);
    TEST_ASSERT(result == false, "bbb_verify_signature(NULL signature) should return false");

    TEST_PASS("NULL signature verification handling correct");
}

/**
 * Test: bbb_calculate_hash with NULL data
 */
void test_calculate_hash_null_data(void)
{
    printf("\n=== test_calculate_hash_null_data ===\n");
    setup();

    uint8_t hash[32];
    bool result = bbb_calculate_hash(NULL, 100, hash);
    TEST_ASSERT(result == false, "bbb_calculate_hash(NULL data) should return false");

    TEST_PASS("NULL data hash handling correct");
}

/**
 * Test: bbb_calculate_hash with NULL hash buffer
 */
void test_calculate_hash_null_hash(void)
{
    printf("\n=== test_calculate_hash_null_hash ===\n");
    setup();

    const char* test_data = "Test data";
    bool result = bbb_calculate_hash(test_data, strlen(test_data), NULL);
    TEST_ASSERT(result == false, "bbb_calculate_hash(NULL hash) should return false");

    TEST_PASS("NULL hash buffer handling correct");
}

//=============================================================================
// Unit Tests - Signing Key Configuration
//=============================================================================

/**
 * Test: Set signing key with valid parameters
 */
void test_set_signing_key_valid(void)
{
    printf("\n=== test_set_signing_key_valid ===\n");
    setup();

    bool result = bbb_set_signing_key(TEST_SIGNING_KEY, sizeof(TEST_SIGNING_KEY));
    TEST_ASSERT(result == true, "bbb_set_signing_key should succeed with valid key");

    TEST_PASS("Valid signing key configuration succeeded");
}

/**
 * Test: Set signing key with NULL key
 */
void test_set_signing_key_null(void)
{
    printf("\n=== test_set_signing_key_null ===\n");
    setup();

    bool result = bbb_set_signing_key(NULL, 32);
    TEST_ASSERT(result == false, "bbb_set_signing_key(NULL) should return false");

    TEST_PASS("NULL signing key handling correct");
}

/**
 * Test: Set signing key with zero size
 */
void test_set_signing_key_zero_size(void)
{
    printf("\n=== test_set_signing_key_zero_size ===\n");
    setup();

    bool result = bbb_set_signing_key(TEST_SIGNING_KEY, 0);
    TEST_ASSERT(result == false, "bbb_set_signing_key(size=0) should return false");

    TEST_PASS("Zero size signing key handling correct");
}

/**
 * Test: Set signing key too short (< 16 bytes)
 */
void test_set_signing_key_too_short(void)
{
    printf("\n=== test_set_signing_key_too_short ===\n");
    setup();

    uint8_t short_key[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    bool result = bbb_set_signing_key(short_key, sizeof(short_key));
    TEST_ASSERT(result == false, "bbb_set_signing_key with key < 16 bytes should fail");

    TEST_PASS("Short signing key handling correct");
}

/**
 * Test: Clear signing key
 */
void test_clear_signing_key(void)
{
    printf("\n=== test_clear_signing_key ===\n");
    setup_with_key();

    bbb_clear_signing_key();

    /* After clearing, signing should fail */
    const char* test_data = "Test data";
    uint8_t signature[SIGNATURE_SIZE];
    ssize_t result = bbb_sign_code(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == -1, "Signing should fail after key is cleared");

    TEST_PASS("Clear signing key succeeded");
}

//=============================================================================
// Unit Tests - Code Signing
//=============================================================================

/**
 * Test: Sign code with valid parameters
 */
void test_sign_code_valid(void)
{
    printf("\n=== test_sign_code_valid ===\n");
    setup_with_key();

    const char* test_data = "This is test code to be signed";
    uint8_t signature[SIGNATURE_SIZE];

    ssize_t result = bbb_sign_code(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == SIGNATURE_SIZE, "bbb_sign_code should return signature size");

    /* Verify signature is not all zeros */
    bool all_zeros = true;
    for (int i = 0; i < SIGNATURE_SIZE; i++) {
        if (signature[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    TEST_ASSERT(all_zeros == false, "Signature should not be all zeros");

    TEST_PASS("Valid code signing succeeded");
}

/**
 * Test: Sign code with zero size
 */
void test_sign_code_zero_size(void)
{
    printf("\n=== test_sign_code_zero_size ===\n");
    setup_with_key();

    const char* test_data = "Some data";
    uint8_t signature[SIGNATURE_SIZE];

    ssize_t result = bbb_sign_code(NULL, test_data, 0, signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == -1, "bbb_sign_code with size=0 should return -1");

    TEST_PASS("Zero size data signing handling correct");
}

/**
 * Test: Sign code with small signature buffer
 */
void test_sign_code_small_buffer(void)
{
    printf("\n=== test_sign_code_small_buffer ===\n");
    setup_with_key();

    const char* test_data = "Test data";
    uint8_t small_sig[16];  /* Too small */

    ssize_t result = bbb_sign_code(NULL, test_data, strlen(test_data), small_sig, sizeof(small_sig));
    TEST_ASSERT(result == -1, "bbb_sign_code with small buffer should return -1");

    TEST_PASS("Small signature buffer handling correct");
}

/**
 * Test: Sign code without configured key
 */
void test_sign_code_no_key(void)
{
    printf("\n=== test_sign_code_no_key ===\n");
    setup();  /* No key configured */

    const char* test_data = "Test data";
    uint8_t signature[SIGNATURE_SIZE];

    ssize_t result = bbb_sign_code(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == -1, "bbb_sign_code without key should return -1");

    TEST_PASS("Signing without key handling correct");
}

/**
 * Test: Sign binary data
 */
void test_sign_binary_data(void)
{
    printf("\n=== test_sign_binary_data ===\n");
    setup_with_key();

    uint8_t binary_data[256];
    for (int i = 0; i < 256; i++) {
        binary_data[i] = (uint8_t)(i ^ 0xAA);
    }

    uint8_t signature[SIGNATURE_SIZE];
    ssize_t result = bbb_sign_code(NULL, binary_data, sizeof(binary_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == SIGNATURE_SIZE, "Binary data signing should succeed");

    TEST_PASS("Binary data signing succeeded");
}

//=============================================================================
// Unit Tests - Signature Verification
//=============================================================================

/**
 * Test: Verify valid signature
 */
void test_verify_signature_valid(void)
{
    printf("\n=== test_verify_signature_valid ===\n");
    setup_with_key();

    const char* test_data = "Code that has been signed";
    uint8_t signature[SIGNATURE_SIZE];

    /* Sign the data */
    ssize_t sign_result = bbb_sign_code(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(sign_result == SIGNATURE_SIZE, "Signing should succeed");

    /* Verify the signature */
    bool verify_result = bbb_verify_signature(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(verify_result == true, "Valid signature should verify successfully");

    TEST_PASS("Valid signature verification succeeded");
}

/**
 * Test: Verify signature of tampered data
 */
void test_verify_signature_tampered_data(void)
{
    printf("\n=== test_verify_signature_tampered_data ===\n");
    setup_with_key();

    char test_data[64] = "Original code data to sign";
    uint8_t signature[SIGNATURE_SIZE];

    /* Sign the original data */
    ssize_t sign_result = bbb_sign_code(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(sign_result == SIGNATURE_SIZE, "Signing should succeed");

    /* Tamper with the data */
    test_data[0] = 'X';  /* Change first character */

    /* Verification should fail */
    bool verify_result = bbb_verify_signature(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(verify_result == false, "Tampered data signature should not verify");

    TEST_PASS("Tampered data detection succeeded");
}

/**
 * Test: Verify signature with wrong size
 */
void test_verify_signature_wrong_size(void)
{
    printf("\n=== test_verify_signature_wrong_size ===\n");
    setup_with_key();

    const char* test_data = "Test data";
    uint8_t signature[SIGNATURE_SIZE] = {0};

    /* Try to verify with wrong signature size */
    bool result = bbb_verify_signature(NULL, test_data, strlen(test_data), signature, 16);  /* Wrong size */
    TEST_ASSERT(result == false, "Wrong signature size should fail verification");

    TEST_PASS("Wrong signature size handling correct");
}

/**
 * Test: Verify invalid signature (random bytes)
 */
void test_verify_signature_invalid(void)
{
    printf("\n=== test_verify_signature_invalid ===\n");
    setup_with_key();

    const char* test_data = "Test data to verify";
    uint8_t fake_signature[SIGNATURE_SIZE] = {
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
    };

    bool result = bbb_verify_signature(NULL, test_data, strlen(test_data), fake_signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == false, "Random signature should not verify");

    TEST_PASS("Invalid signature detection correct");
}

/**
 * Test: Verify signature without configured key
 */
void test_verify_signature_no_key(void)
{
    printf("\n=== test_verify_signature_no_key ===\n");
    setup();  /* No key configured */

    const char* test_data = "Test data";
    uint8_t signature[SIGNATURE_SIZE] = {0};

    bool result = bbb_verify_signature(NULL, test_data, strlen(test_data), signature, SIGNATURE_SIZE);
    TEST_ASSERT(result == false, "Verification without key should fail");

    TEST_PASS("Verification without key handling correct");
}

//=============================================================================
// Unit Tests - Hash Calculation
//=============================================================================

/**
 * Test: Calculate hash with valid data
 */
void test_calculate_hash_valid(void)
{
    printf("\n=== test_calculate_hash_valid ===\n");
    setup();

    const char* test_data = "Data to hash";
    uint8_t hash[32];

    bool result = bbb_calculate_hash(test_data, strlen(test_data), hash);
    TEST_ASSERT(result == true, "Hash calculation should succeed");

    /* Verify hash is not all zeros */
    bool all_zeros = true;
    for (int i = 0; i < 32; i++) {
        if (hash[i] != 0) {
            all_zeros = false;
            break;
        }
    }
    TEST_ASSERT(all_zeros == false, "Hash should not be all zeros");

    TEST_PASS("Valid hash calculation succeeded");
}

/**
 * Test: Hash consistency - same data produces same hash
 */
void test_calculate_hash_consistency(void)
{
    printf("\n=== test_calculate_hash_consistency ===\n");
    setup();

    const char* test_data = "Consistent data";
    uint8_t hash1[32];
    uint8_t hash2[32];

    bool result1 = bbb_calculate_hash(test_data, strlen(test_data), hash1);
    bool result2 = bbb_calculate_hash(test_data, strlen(test_data), hash2);

    TEST_ASSERT(result1 == true, "First hash calculation should succeed");
    TEST_ASSERT(result2 == true, "Second hash calculation should succeed");
    TEST_ASSERT(memcmp(hash1, hash2, 32) == 0, "Same data should produce same hash");

    TEST_PASS("Hash consistency verified");
}

/**
 * Test: Different data produces different hash
 */
void test_calculate_hash_different(void)
{
    printf("\n=== test_calculate_hash_different ===\n");
    setup();

    const char* data1 = "First data";
    const char* data2 = "Second data";
    uint8_t hash1[32];
    uint8_t hash2[32];

    bbb_calculate_hash(data1, strlen(data1), hash1);
    bbb_calculate_hash(data2, strlen(data2), hash2);

    TEST_ASSERT(memcmp(hash1, hash2, 32) != 0, "Different data should produce different hash");

    TEST_PASS("Different data produces different hash");
}

/**
 * Test: Hash of zero-size data
 */
void test_calculate_hash_zero_size(void)
{
    printf("\n=== test_calculate_hash_zero_size ===\n");
    setup();

    const char* data = "Some data";
    uint8_t hash[32];

    bool result = bbb_calculate_hash(data, 0, hash);
    TEST_ASSERT(result == false, "Hash of zero-size data should fail");

    TEST_PASS("Zero-size data hash handling correct");
}

//=============================================================================
// Unit Tests - Trusted Key Management
//=============================================================================

/**
 * Test: Add trusted key
 */
void test_add_trusted_key_valid(void)
{
    printf("\n=== test_add_trusted_key_valid ===\n");
    setup();

    uint8_t key_data[32] = {0x01, 0x02, 0x03, 0x04};
    bool result = bbb_add_trusted_key(NULL, key_data, 32, "test_key_1");
    TEST_ASSERT(result == true, "Adding trusted key should succeed");

    TEST_PASS("Valid trusted key addition succeeded");
}

/**
 * Test: Add trusted key with NULL data
 */
void test_add_trusted_key_null_data(void)
{
    printf("\n=== test_add_trusted_key_null_data ===\n");
    setup();

    bool result = bbb_add_trusted_key(NULL, NULL, 32, "test_key");
    TEST_ASSERT(result == false, "Adding NULL key data should fail");

    TEST_PASS("NULL key data handling correct");
}

/**
 * Test: Add trusted key with NULL ID
 */
void test_add_trusted_key_null_id(void)
{
    printf("\n=== test_add_trusted_key_null_id ===\n");
    setup();

    uint8_t key_data[32] = {0x01, 0x02, 0x03, 0x04};
    bool result = bbb_add_trusted_key(NULL, key_data, 32, NULL);
    TEST_ASSERT(result == false, "Adding key with NULL ID should fail");

    TEST_PASS("NULL key ID handling correct");
}

/**
 * Test: Add trusted key with zero size
 */
void test_add_trusted_key_zero_size(void)
{
    printf("\n=== test_add_trusted_key_zero_size ===\n");
    setup();

    uint8_t key_data[32] = {0x01, 0x02, 0x03, 0x04};
    bool result = bbb_add_trusted_key(NULL, key_data, 0, "test_key");
    TEST_ASSERT(result == false, "Adding zero-size key should fail");

    TEST_PASS("Zero-size key handling correct");
}

/**
 * Test: Add duplicate trusted key
 */
void test_add_trusted_key_duplicate(void)
{
    printf("\n=== test_add_trusted_key_duplicate ===\n");
    setup();

    uint8_t key_data[32] = {0x01, 0x02, 0x03, 0x04};
    bool result1 = bbb_add_trusted_key(NULL, key_data, 32, "duplicate_key");
    TEST_ASSERT(result1 == true, "First key addition should succeed");

    bool result2 = bbb_add_trusted_key(NULL, key_data, 32, "duplicate_key");
    TEST_ASSERT(result2 == false, "Duplicate key addition should fail");

    TEST_PASS("Duplicate key handling correct");
}

/**
 * Test: Remove trusted key
 */
void test_remove_trusted_key_valid(void)
{
    printf("\n=== test_remove_trusted_key_valid ===\n");
    setup();

    uint8_t key_data[32] = {0x01, 0x02, 0x03, 0x04};
    bbb_add_trusted_key(NULL, key_data, 32, "key_to_remove");

    bool result = bbb_remove_trusted_key(NULL, "key_to_remove");
    TEST_ASSERT(result == true, "Removing existing key should succeed");

    TEST_PASS("Trusted key removal succeeded");
}

/**
 * Test: Remove non-existent trusted key
 */
void test_remove_trusted_key_nonexistent(void)
{
    printf("\n=== test_remove_trusted_key_nonexistent ===\n");
    setup();

    bool result = bbb_remove_trusted_key(NULL, "nonexistent_key");
    TEST_ASSERT(result == false, "Removing non-existent key should fail");

    TEST_PASS("Non-existent key removal handling correct");
}

/**
 * Test: Remove trusted key with NULL ID
 */
void test_remove_trusted_key_null(void)
{
    printf("\n=== test_remove_trusted_key_null ===\n");
    setup();

    bool result = bbb_remove_trusted_key(NULL, NULL);
    TEST_ASSERT(result == false, "Removing with NULL ID should fail");

    TEST_PASS("NULL key ID removal handling correct");
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("=================================================\n");
    printf("BBB Code Signing Unit Tests\n");
    printf("=================================================\n");

    /* NULL parameter handling tests */
    test_sign_code_null_data();
    test_sign_code_null_signature();
    test_verify_signature_null_data();
    test_verify_signature_null_signature();
    test_calculate_hash_null_data();
    test_calculate_hash_null_hash();

    /* Signing key configuration tests */
    test_set_signing_key_valid();
    test_set_signing_key_null();
    test_set_signing_key_zero_size();
    test_set_signing_key_too_short();
    test_clear_signing_key();

    /* Code signing tests */
    test_sign_code_valid();
    test_sign_code_zero_size();
    test_sign_code_small_buffer();
    test_sign_code_no_key();
    test_sign_binary_data();

    /* Signature verification tests */
    test_verify_signature_valid();
    test_verify_signature_tampered_data();
    test_verify_signature_wrong_size();
    test_verify_signature_invalid();
    test_verify_signature_no_key();

    /* Hash calculation tests */
    test_calculate_hash_valid();
    test_calculate_hash_consistency();
    test_calculate_hash_different();
    test_calculate_hash_zero_size();

    /* Trusted key management tests */
    test_add_trusted_key_valid();
    test_add_trusted_key_null_data();
    test_add_trusted_key_null_id();
    test_add_trusted_key_zero_size();
    test_add_trusted_key_duplicate();
    test_remove_trusted_key_valid();
    test_remove_trusted_key_nonexistent();
    test_remove_trusted_key_null();

    /* Print summary */
    printf("\n=================================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
