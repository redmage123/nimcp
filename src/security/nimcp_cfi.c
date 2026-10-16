/**
 * @file nimcp_cfi.c
 * @brief Implementation of Control Flow Integrity (CFI)
 *
 * WHAT: Software-based CFI implementation for protecting control flow
 *       integrity against exploitation attacks.
 *
 * WHY:  Even with memory safety, control flow hijacking remains possible.
 *       CFI ensures program execution follows intended paths only.
 *
 * HOW:  Maintains registry of valid targets, validates indirect calls
 *       against this registry, and integrates with shadow stack for
 *       return address protection.
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#include "security/nimcp_cfi.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "security_cfi"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cfi, MESH_ADAPTER_CATEGORY_SECURITY)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal CFI context
 */
struct nimcp_cfi_context {
    // Valid targets
    nimcp_cfi_target_t targets[NIMCP_CFI_MAX_TARGETS];
    uint32_t num_targets;

    // Call sites
    nimcp_cfi_call_site_t call_sites[NIMCP_CFI_MAX_CALL_SITES];
    uint32_t num_call_sites;

    // Configuration
    nimcp_cfi_mode_t mode;

    // Violation handler
    nimcp_cfi_violation_callback_t violation_callback;
    void* callback_user_data;

    // Statistics
    nimcp_cfi_stats_t stats;

    // State
    bool initialized;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief FNV-1a hash for type ID generation
 */
static uint32_t fnv1a_hash(const char* str)
{
    if (!str)
        return 0;

    uint32_t hash = 2166136261U;  // FNV offset basis

    for (const char* p = str; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619U;  // FNV prime
    }

    return hash;
}

/**
 * @brief Find target by address
 */
static nimcp_cfi_target_t* find_target_by_address(
    nimcp_cfi_context_t* cfi,
    void* address)
{
    if (!cfi || !address) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_target_by_address: required parameter is NULL (cfi, address)");
        return NULL;
    }

    for (uint32_t i = 0; i < cfi->num_targets; i++) {
        if (cfi->targets[i].valid && cfi->targets[i].address == address) {
            return &cfi->targets[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_target_by_address: validation failed");
    return NULL;
}

/**
 * @brief Handle CFI violation
 */
static bool handle_violation(
    nimcp_cfi_context_t* cfi,
    nimcp_cfi_result_t result,
    void* target,
    void* call_site)
{
    if (!cfi)
        {

            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "handle_violation: cfi is NULL");

            return false;

        }

    // Update statistics
    cfi->stats.violations_detected++;

    switch (result) {
        case NIMCP_CFI_TYPE_MISMATCH:
            cfi->stats.type_mismatches++;
            break;
        case NIMCP_CFI_INVALID_TARGET:
            cfi->stats.target_not_found++;
            break;
        case NIMCP_CFI_RETURN_MISMATCH:
            cfi->stats.return_mismatches++;
            break;
        default:
            break;
    }

    // Log violation
    char msg[NIMCP_ERROR_BUFFER_SIZE];
    snprintf(msg, sizeof(msg),
             "CFI violation: %s at %p targeting %p",
             nimcp_cfi_result_name(result),
             call_site, target);

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_THREAT_DETECTED,
        NIMCP_THREAT_CRITICAL,
        msg
    );

    // Call violation handler if registered
    if (cfi->violation_callback) {
        bool allow = cfi->violation_callback(
            result, target, call_site, cfi->callback_user_data
        );
        if (allow) {
            return true;  // Handler says allow
        }
    }

    // Check enforcement mode
    if (cfi->mode == NIMCP_CFI_MODE_ENFORCE) {
        cfi->stats.violations_blocked++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "handle_violation: validation failed");
        return false;  // Block transfer
    }

    return true;  // Detect mode - allow but logged
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_cfi_context_t* nimcp_cfi_create(void)
{
    nimcp_cfi_context_t* cfi =
        (nimcp_cfi_context_t*)nimcp_calloc(1, sizeof(nimcp_cfi_context_t));

    NIMCP_API_CHECK_ALLOC(cfi, "Failed to allocate CFI context");

    cfi->mode = NIMCP_CFI_MODE_DISABLED;
    cfi->initialized = false;

    return cfi;
}

nimcp_result_t nimcp_cfi_init(nimcp_cfi_context_t* cfi, nimcp_cfi_mode_t mode)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in init");
    NIMCP_API_CHECK(!cfi->initialized, NIMCP_INVALID_STATE, "CFI context already initialized");

    cfi->mode = mode;
    cfi->initialized = true;

    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        "CFI initialized"
    );

    return NIMCP_SUCCESS;
}

void nimcp_cfi_destroy(nimcp_cfi_context_t* cfi)
{
    if (!cfi)
        return;

    // Free any allocated target arrays in call sites
    for (uint32_t i = 0; i < cfi->num_call_sites; i++) {
        if (cfi->call_sites[i].valid_target_ids) {
            nimcp_free(cfi->call_sites[i].valid_target_ids);
        }
    }

    memset(cfi, 0, sizeof(nimcp_cfi_context_t));
    nimcp_free(cfi);
}

//=============================================================================
// Target Registration
//=============================================================================

int32_t nimcp_cfi_register_target(
    nimcp_cfi_context_t* cfi,
    void* address,
    uint32_t type_id,
    nimcp_func_type_t category,
    const char* name)
{
    if (!cfi || !address) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cfi_register_target: required parameter is NULL (cfi, address)");
        return -1;
    }

    if (cfi->num_targets >= NIMCP_CFI_MAX_TARGETS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_cfi_register_target: capacity exceeded");
        return -1;
    }

    // Check for duplicate
    if (find_target_by_address(cfi, address)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_cfi_register_target: validation failed");
        return -1;  // Already registered
    }

    int32_t index = (int32_t)cfi->num_targets;
    nimcp_cfi_target_t* target = &cfi->targets[index];

    target->address = address;
    target->type_id = type_id;
    target->category = category;
    target->name = name;
    target->valid = true;
    target->call_count = 0;

    cfi->num_targets++;

    return index;
}

nimcp_result_t nimcp_cfi_unregister_target(
    nimcp_cfi_context_t* cfi,
    int32_t target_index)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in unregister_target");
    NIMCP_API_CHECK(target_index >= 0 && (uint32_t)target_index < cfi->num_targets,
                   NIMCP_INVALID_PARAM, "Invalid target index in unregister_target");

    cfi->targets[target_index].valid = false;

    return NIMCP_SUCCESS;
}

int32_t nimcp_cfi_register_call_site(
    nimcp_cfi_context_t* cfi,
    void* call_address,
    uint32_t type_id)
{
    if (!cfi || !call_address) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cfi_register_call_site: required parameter is NULL (cfi, call_address)");
        return -1;
    }

    if (cfi->num_call_sites >= NIMCP_CFI_MAX_CALL_SITES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "nimcp_cfi_register_call_site: capacity exceeded");
        return -1;
    }

    int32_t index = (int32_t)cfi->num_call_sites;
    nimcp_cfi_call_site_t* site = &cfi->call_sites[index];

    site->call_address = call_address;
    site->allowed_type_id = type_id;
    site->target_count = 0;
    site->valid_target_ids = NULL;
    site->call_count = 0;
    site->violation_count = 0;

    cfi->num_call_sites++;

    return index;
}

//=============================================================================
// CFI Validation
//=============================================================================

nimcp_cfi_result_t nimcp_cfi_check_call(
    nimcp_cfi_context_t* cfi,
    void* target,
    uint32_t type_id)
{
    if (!cfi)
        return NIMCP_CFI_VALID;  // No context = no checking

    if (cfi->mode == NIMCP_CFI_MODE_DISABLED)
        return NIMCP_CFI_VALID;

    cfi->stats.total_checks++;

    // Check for NULL target
    if (!target) {
        handle_violation(cfi, NIMCP_CFI_NULL_TARGET, target, NULL);
        return NIMCP_CFI_NULL_TARGET;
    }

    // Note: x86-64 does not require function alignment - functions are byte-addressable
    // The alignment check has been removed as it was causing false positives with
    // Position Independent Code (PIC) where function addresses may not be 4-byte aligned

    // Find target in registry
    nimcp_cfi_target_t* reg_target = find_target_by_address(cfi, target);

    if (!reg_target) {
        bool allow = handle_violation(cfi, NIMCP_CFI_INVALID_TARGET, target, NULL);
        if (!allow) {
            return NIMCP_CFI_INVALID_TARGET;
        }
        // Detect mode - allow but return the result anyway
        cfi->stats.valid_transfers++;
        return NIMCP_CFI_VALID;
    }

    // Check type ID match
    if (type_id != 0 && reg_target->type_id != type_id) {
        bool allow = handle_violation(cfi, NIMCP_CFI_TYPE_MISMATCH, target, NULL);
        if (!allow) {
            return NIMCP_CFI_TYPE_MISMATCH;
        }
        cfi->stats.valid_transfers++;
        return NIMCP_CFI_VALID;
    }

    // Valid transfer
    reg_target->call_count++;
    cfi->stats.valid_transfers++;

    return NIMCP_CFI_VALID;
}

nimcp_cfi_result_t nimcp_cfi_check_return(
    nimcp_cfi_context_t* cfi,
    void* return_addr,
    void* expected_addr)
{
    if (!cfi)
        return NIMCP_CFI_VALID;

    if (cfi->mode == NIMCP_CFI_MODE_DISABLED)
        return NIMCP_CFI_VALID;

    cfi->stats.total_checks++;

    // Compare return addresses
    if (return_addr != expected_addr) {
        bool allow = handle_violation(
            cfi, NIMCP_CFI_RETURN_MISMATCH, return_addr, expected_addr
        );
        if (!allow) {
            return NIMCP_CFI_RETURN_MISMATCH;
        }
    }

    cfi->stats.valid_transfers++;
    return NIMCP_CFI_VALID;
}

bool nimcp_cfi_is_valid_target(nimcp_cfi_context_t* cfi, void* target)
{
    if (!cfi || !target) {
        return false;
    }

    return find_target_by_address(cfi, target) != NULL;
}

bool nimcp_cfi_validate_ptr(
    nimcp_cfi_context_t* cfi,
    void* ptr,
    uint32_t type_id)
{
    if (!cfi)
        return true;  // No CFI = allow

    if (cfi->mode == NIMCP_CFI_MODE_DISABLED)
        return true;

    nimcp_cfi_result_t result = nimcp_cfi_check_call(cfi, ptr, type_id);

    return (result == NIMCP_CFI_VALID);
}

//=============================================================================
// Type ID Generation
//=============================================================================

uint32_t nimcp_cfi_type_id(const char* signature)
{
    if (!signature)
        return 0;

    return fnv1a_hash(signature) | NIMCP_CFI_TYPE_MAGIC;
}

//=============================================================================
// Violation Handling
//=============================================================================

nimcp_result_t nimcp_cfi_set_violation_handler(
    nimcp_cfi_context_t* cfi,
    nimcp_cfi_violation_callback_t callback,
    void* user_data)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in set_violation_handler");

    cfi->violation_callback = callback;
    cfi->callback_user_data = user_data;

    return NIMCP_SUCCESS;
}

uint64_t nimcp_cfi_get_violation_count(nimcp_cfi_context_t* cfi)
{
    if (!cfi)
        return 0;

    return cfi->stats.violations_detected;
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

nimcp_result_t nimcp_cfi_get_stats(
    nimcp_cfi_context_t* cfi,
    nimcp_cfi_stats_t* stats)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in get_stats");
    NIMCP_API_CHECK_NULL(stats, NIMCP_INVALID_PARAM, "NULL stats output in get_stats");

    memcpy(stats, &cfi->stats, sizeof(nimcp_cfi_stats_t));

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cfi_reset_stats(nimcp_cfi_context_t* cfi)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in reset_stats");

    memset(&cfi->stats, 0, sizeof(nimcp_cfi_stats_t));

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_cfi_set_mode(nimcp_cfi_context_t* cfi, nimcp_cfi_mode_t mode)
{
    NIMCP_API_CHECK_NULL(cfi, NIMCP_INVALID_PARAM, "NULL CFI context in set_mode");

    cfi->mode = mode;

    char msg[NIMCP_ID_BUFFER_SIZE];
    snprintf(msg, sizeof(msg), "CFI mode changed to %s", nimcp_cfi_mode_name(mode));
    nimcp_security_log_event(
        NIMCP_SECURITY_EVENT_DIRECTIVE_VERIFIED,
        NIMCP_THREAT_NONE,
        msg
    );

    return NIMCP_SUCCESS;
}

nimcp_cfi_mode_t nimcp_cfi_get_mode(nimcp_cfi_context_t* cfi)
{
    if (!cfi)
        return NIMCP_CFI_MODE_DISABLED;

    return cfi->mode;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_cfi_result_name(nimcp_cfi_result_t result)
{
    static const char* names[] = {
        "Valid",
        "Invalid Target",
        "Type Mismatch",
        "Invalid Call Site",
        "Return Mismatch",
        "NULL Target",
        "Unaligned Target"
    };

    if (result > NIMCP_CFI_UNALIGNED_TARGET)
        return "Unknown";

    return names[result];
}

const char* nimcp_cfi_mode_name(nimcp_cfi_mode_t mode)
{
    static const char* names[] = {
        "Disabled",
        "Detect",
        "Enforce"
    };

    if (mode > NIMCP_CFI_MODE_ENFORCE)
        return "Unknown";

    return names[mode];
}

const char* nimcp_cfi_category_name(nimcp_func_type_t category)
{
    static const char* names[] = {
        "Void",
        "Int",
        "Ptr",
        "Callback",
        "Neural",
        "Security",
        "Unknown"
    };

    if (category > NIMCP_FUNC_TYPE_UNKNOWN)
        return "Invalid";

    return names[category];
}
