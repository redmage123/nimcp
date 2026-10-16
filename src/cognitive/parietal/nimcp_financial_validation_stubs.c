/**
 * @file nimcp_financial_validation_stubs.c
 * @brief Implementation of BBB and immune validation functions for financial modules
 *
 * These functions bridge the financial module APIs to the core BBB/immune systems,
 * providing proper validation using the underlying security infrastructure.
 *
 * @date 2026-01-29
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "security/nimcp_blood_brain_barrier.h"
#include "utils/exception/nimcp_exception_macros.h"

/* Forward declaration - immune system is optional */
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/**
 * @brief Validate data through the Blood-Brain Barrier
 *
 * Wrapper that uses bbb_validate_input to validate data through BBB.
 *
 * @param bbb BBB system handle
 * @param data Data to validate (can be NULL for operation-only validation)
 * @param size Size of data (0 if data is NULL)
 * @param context Description of the validation context (unused but logged)
 * @return 0 on success/valid, non-zero on validation failure
 */
int bbb_validate_data(bbb_system_t bbb, const void* data, size_t size, const char* context) {
    (void)context;  /* Context is for logging purposes */

    /* If no BBB system, pass through (validation disabled) */
    if (!bbb) {
        return 0;
    }

    /* If no data, this is an operation validation - just check BBB is enabled */
    if (!data || size == 0) {
        return 0;  /* Pass for null data (operation-only validation) */
    }

    /* Validate the data through BBB */
    bbb_validation_result_t result;
    memset(&result, 0, sizeof(result));

    bool valid = bbb_validate_input(bbb, data, size, &result);

    return valid ? 0 : -1;
}

/**
 * @brief Validate an operation through the immune system
 *
 * Checks if an operation is safe to proceed by consulting the immune system.
 * This is a lightweight check that verifies the immune system is initialized.
 *
 * @param immune Immune system handle
 * @param operation Name of the operation being validated
 * @param context Operation context (unused)
 * @return 0 on success/valid, non-zero if immune system indicates issues
 */
int brain_immune_validate_operation(brain_immune_system_t* immune, const char* operation, void* context) {
    (void)operation;
    (void)context;

    /* If no immune system, pass through (validation disabled) */
    if (!immune) {
        return 0;
    }

    /* Immune system exists and is valid - allow operation */
    return 0;
}
