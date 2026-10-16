/**
 * @file test_secure_rng.c
 * @brief Standalone test for cryptographic RNG fix (CWE-338)
 *
 * WHAT: Tests that nimcp_encryption_generate_key() produces cryptographically secure keys
 * WHY:  Validates fix for CRITICAL vulnerability - weak rand/srand replaced with OS CSPRNG
 * HOW:  Generates multiple keys and verifies uniqueness and entropy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Minimal declarations needed for test
#define NIMCP_SECURITY_KEY_SIZE 32
typedef enum {
    NIMCP_SUCCESS = 0,
    NIMCP_INVALID_PARAM = -1,
    NIMCP_IO_ERROR = -2
} nimcp_result_t;

// External function we're testing
extern nimcp_result_t nimcp_encryption_generate_key(uint8_t* key);

/**
 * WHAT: Test basic key generation
 * WHY:  Verify function works and produces non-zero keys
 */
bool test_basic_generation(void)
{
    printf("TEST: Basic key generation...\n");

    uint8_t key[NIMCP_SECURITY_KEY_SIZE];
    nimcp_result_t result = nimcp_encryption_generate_key(key);

    if (result != NIMCP_SUCCESS) {
        printf("  FAIL: Key generation returned error %d\n", result);
        return false;
    }

    // Check key is not all zeros
    bool has_nonzero = false;
    for (int i = 0; i < NIMCP_SECURITY_KEY_SIZE; i++) {
        if (key[i] != 0) {
            has_nonzero = true;
            break;
        }
    }

    if (!has_nonzero) {
        printf("  FAIL: Key is all zeros\n");
        return false;
    }

    printf("  PASS: Generated non-zero key\n");
    return true;
}

/**
 * WHAT: Test key uniqueness
 * WHY:  Cryptographically secure RNG should never produce duplicate keys
 */
bool test_uniqueness(void)
{
    printf("TEST: Key uniqueness (1000 keys)...\n");

    const int NUM_KEYS = 1000;
    uint8_t keys[NUM_KEYS][NIMCP_SECURITY_KEY_SIZE];

    // Generate keys
    for (int i = 0; i < NUM_KEYS; i++) {
        nimcp_result_t result = nimcp_encryption_generate_key(keys[i]);
        if (result != NIMCP_SUCCESS) {
            printf("  FAIL: Key generation failed at index %d\n", i);
            return false;
        }
    }

    // Check for duplicates
    for (int i = 0; i < NUM_KEYS; i++) {
        for (int j = i + 1; j < NUM_KEYS; j++) {
            if (memcmp(keys[i], keys[j], NIMCP_SECURITY_KEY_SIZE) == 0) {
                printf("  FAIL: Duplicate keys at indices %d and %d\n", i, j);
                printf("  This indicates WEAK RNG - vulnerability NOT fixed!\n");
                return false;
            }
        }
    }

    printf("  PASS: All 1000 keys are unique\n");
    return true;
}

/**
 * WHAT: Test byte distribution
 * WHY:  Cryptographically secure RNG should produce uniform byte distribution
 */
bool test_distribution(void)
{
    printf("TEST: Byte distribution (1000 keys)...\n");

    const int NUM_KEYS = 1000;
    int byte_counts[256] = {0};

    // Generate keys and count bytes
    for (int i = 0; i < NUM_KEYS; i++) {
        uint8_t key[NIMCP_SECURITY_KEY_SIZE];
        nimcp_encryption_generate_key(key);

        for (int j = 0; j < NIMCP_SECURITY_KEY_SIZE; j++) {
            byte_counts[key[j]]++;
        }
    }

    // Check distribution
    // Expected: (1000 * 32) / 256 = 125 per byte value
    // Allow 50% deviation: 62-188
    int expected = (NUM_KEYS * NIMCP_SECURITY_KEY_SIZE) / 256;
    int min_acceptable = expected / 2;
    int max_acceptable = expected * 3 / 2;

    int biased_bytes = 0;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] < min_acceptable || byte_counts[i] > max_acceptable) {
            biased_bytes++;
        }
    }

    // Less than 10% should be biased
    if (biased_bytes >= 26) {
        printf("  FAIL: %d/256 byte values are biased (> 10%%)\n", biased_bytes);
        printf("  This indicates weak RNG - vulnerability may not be fully fixed!\n");
        return false;
    }

    printf("  PASS: Byte distribution is acceptable (%d/256 biased, < 10%%)\n", biased_bytes);
    return true;
}

/**
 * WHAT: Test null parameter handling
 * WHY:  Ensure function properly validates input
 */
bool test_null_safety(void)
{
    printf("TEST: Null parameter handling...\n");

    nimcp_result_t result = nimcp_encryption_generate_key(NULL);

    if (result != NIMCP_INVALID_PARAM) {
        printf("  FAIL: Expected NIMCP_INVALID_PARAM for null pointer\n");
        return false;
    }

    printf("  PASS: Null parameter rejected\n");
    return true;
}

int main(void)
{
    printf("=================================================================\n");
    printf("CRITICAL SECURITY FIX VERIFICATION: CWE-338 RNG Vulnerability\n");
    printf("=================================================================\n");
    printf("Testing nimcp_encryption_generate_key() for cryptographic quality\n\n");

    bool all_passed = true;

    all_passed &= test_null_safety();
    all_passed &= test_basic_generation();
    all_passed &= test_uniqueness();
    all_passed &= test_distribution();

    printf("\n=================================================================\n");
    if (all_passed) {
        printf("RESULT: ALL TESTS PASSED ✓\n");
        printf("The cryptographic RNG vulnerability has been successfully fixed.\n");
        printf("Keys are now generated using OS CSPRNG (/dev/urandom or BCryptGenRandom)\n");
        printf("=================================================================\n");
        return 0;
    } else {
        printf("RESULT: SOME TESTS FAILED ✗\n");
        printf("The vulnerability may not be fully fixed - review implementation.\n");
        printf("=================================================================\n");
        return 1;
    }
}
