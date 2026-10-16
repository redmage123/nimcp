/**
 * @file nimcp_cfi.h
 * @brief Control Flow Integrity (CFI) Implementation
 *
 * WHAT: Provides software-based Control Flow Integrity protection to prevent
 *       control-flow hijacking attacks (ROP, JOP, COP).
 *
 * WHY:  Memory corruption vulnerabilities can be exploited to redirect program
 *       execution to arbitrary locations. CFI ensures only valid control flow
 *       transfers occur, blocking exploitation of such vulnerabilities.
 *
 * HOW:  Implements forward-edge (indirect calls) and backward-edge (returns)
 *       CFI through:
 *       - Function pointer validation before indirect calls
 *       - Return address verification via shadow stack integration
 *       - Call site validation for legitimate targets
 *       - Runtime violation detection and response
 *
 * ATTACK VECTORS MITIGATED:
 *   - Return-Oriented Programming (ROP)
 *   - Jump-Oriented Programming (JOP)
 *   - Call-Oriented Programming (COP)
 *   - Function pointer overwrites
 *   - vtable hijacking
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_CFI_H
#define NIMCP_CFI_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of registered valid call targets */
#define NIMCP_CFI_MAX_TARGETS 1024

/** Maximum number of call sites */
#define NIMCP_CFI_MAX_CALL_SITES 512

/** CFI type identifier magic number */
#define NIMCP_CFI_TYPE_MAGIC 0xCF1707EC

/** Invalid target marker (for pointer comparison) */
#define NIMCP_CFI_INVALID_TARGET_PTR ((void*)0xDEADCF1)

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief CFI validation result
 */
typedef enum {
    NIMCP_CFI_VALID = 0,              /**< Transfer is valid */
    NIMCP_CFI_INVALID_TARGET,         /**< Target not in valid set */
    NIMCP_CFI_TYPE_MISMATCH,          /**< Type signature mismatch */
    NIMCP_CFI_CALL_SITE_INVALID,      /**< Call site not registered */
    NIMCP_CFI_RETURN_MISMATCH,        /**< Return address mismatch */
    NIMCP_CFI_NULL_TARGET,            /**< NULL function pointer */
    NIMCP_CFI_UNALIGNED_TARGET        /**< Target not properly aligned */
} nimcp_cfi_result_t;

/**
 * @brief CFI enforcement mode
 */
typedef enum {
    NIMCP_CFI_MODE_DISABLED = 0,      /**< CFI disabled */
    NIMCP_CFI_MODE_DETECT,            /**< Log violations but allow */
    NIMCP_CFI_MODE_ENFORCE            /**< Block invalid transfers */
} nimcp_cfi_mode_t;

/**
 * @brief Function type category for coarse-grained CFI
 */
typedef enum {
    NIMCP_FUNC_TYPE_VOID = 0,         /**< void func(void) */
    NIMCP_FUNC_TYPE_INT,              /**< int func(...) */
    NIMCP_FUNC_TYPE_PTR,              /**< void* func(...) */
    NIMCP_FUNC_TYPE_CALLBACK,         /**< callback function */
    NIMCP_FUNC_TYPE_NEURAL,           /**< neural network function */
    NIMCP_FUNC_TYPE_SECURITY,         /**< security-critical function */
    NIMCP_FUNC_TYPE_UNKNOWN           /**< unknown type */
} nimcp_func_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief CFI target descriptor
 */
typedef struct {
    void* address;                    /**< Function address */
    uint32_t type_id;                 /**< Type identifier (hash of signature) */
    nimcp_func_type_t category;       /**< Function category */
    const char* name;                 /**< Function name (debug) */
    bool valid;                       /**< Is target valid */
    uint64_t call_count;              /**< Times called */
} nimcp_cfi_target_t;

/**
 * @brief CFI call site descriptor
 */
typedef struct {
    void* call_address;               /**< Address of call instruction */
    uint32_t allowed_type_id;         /**< Allowed target type ID */
    uint32_t target_count;            /**< Number of valid targets */
    uint32_t* valid_target_ids;       /**< Array of valid target indices */
    uint64_t call_count;              /**< Times executed */
    uint64_t violation_count;         /**< CFI violations at this site */
} nimcp_cfi_call_site_t;

/**
 * @brief CFI statistics
 */
typedef struct {
    uint64_t total_checks;            /**< Total CFI checks performed */
    uint64_t valid_transfers;         /**< Valid control transfers */
    uint64_t violations_detected;     /**< Violations detected */
    uint64_t violations_blocked;      /**< Violations blocked (enforce mode) */
    uint64_t type_mismatches;         /**< Type mismatch violations */
    uint64_t target_not_found;        /**< Unknown target violations */
    uint64_t return_mismatches;       /**< Return address mismatches */
} nimcp_cfi_stats_t;

/**
 * @brief CFI context (opaque handle)
 */
typedef struct nimcp_cfi_context nimcp_cfi_context_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create CFI context
 *
 * @return CFI context or NULL on failure
 */
nimcp_cfi_context_t* nimcp_cfi_create(void);

/**
 * @brief Initialize CFI with enforcement mode
 *
 * @param cfi CFI context
 * @param mode Enforcement mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_init(nimcp_cfi_context_t* cfi, nimcp_cfi_mode_t mode);

/**
 * @brief Destroy CFI context
 *
 * @param cfi CFI context
 */
void nimcp_cfi_destroy(nimcp_cfi_context_t* cfi);

//=============================================================================
// Target Registration
//=============================================================================

/**
 * @brief Register a valid call target
 *
 * @param cfi CFI context
 * @param address Function address
 * @param type_id Type identifier (use NIMCP_CFI_TYPE_* macro)
 * @param category Function category
 * @param name Function name for debugging
 * @return Target index or -1 on failure
 */
int32_t nimcp_cfi_register_target(
    nimcp_cfi_context_t* cfi,
    void* address,
    uint32_t type_id,
    nimcp_func_type_t category,
    const char* name
);

/**
 * @brief Unregister a call target
 *
 * @param cfi CFI context
 * @param target_index Target index from registration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_unregister_target(
    nimcp_cfi_context_t* cfi,
    int32_t target_index
);

/**
 * @brief Register a call site with allowed targets
 *
 * @param cfi CFI context
 * @param call_address Address of call instruction
 * @param type_id Allowed target type
 * @return Call site index or -1 on failure
 */
int32_t nimcp_cfi_register_call_site(
    nimcp_cfi_context_t* cfi,
    void* call_address,
    uint32_t type_id
);

//=============================================================================
// CFI Validation
//=============================================================================

/**
 * @brief Validate an indirect call target (forward-edge CFI)
 *
 * @param cfi CFI context
 * @param target Target function address
 * @param type_id Expected type identifier
 * @return CFI validation result
 */
nimcp_cfi_result_t nimcp_cfi_check_call(
    nimcp_cfi_context_t* cfi,
    void* target,
    uint32_t type_id
);

/**
 * @brief Validate a return address (backward-edge CFI)
 *
 * @param cfi CFI context
 * @param return_addr Return address to validate
 * @param expected_addr Expected return address (from shadow stack)
 * @return CFI validation result
 */
nimcp_cfi_result_t nimcp_cfi_check_return(
    nimcp_cfi_context_t* cfi,
    void* return_addr,
    void* expected_addr
);

/**
 * @brief Check if target is in valid target set
 *
 * @param cfi CFI context
 * @param target Target address
 * @return true if valid target, false otherwise
 */
bool nimcp_cfi_is_valid_target(nimcp_cfi_context_t* cfi, void* target);

/**
 * @brief Validate function pointer before call
 *
 * Use this macro before indirect calls:
 * NIMCP_CFI_VALIDATE(cfi, func_ptr, TYPE_ID) && func_ptr(args)
 *
 * @param cfi CFI context
 * @param ptr Function pointer to validate
 * @param type_id Expected type
 * @return true if valid, false if should not call
 */
bool nimcp_cfi_validate_ptr(
    nimcp_cfi_context_t* cfi,
    void* ptr,
    uint32_t type_id
);

//=============================================================================
// Type ID Generation
//=============================================================================

/**
 * @brief Generate type ID from function signature
 *
 * @param signature Function signature string (e.g., "int(int,int)")
 * @return Type ID hash
 */
uint32_t nimcp_cfi_type_id(const char* signature);

/**
 * @brief Generate type ID for common patterns
 */
#define NIMCP_CFI_TYPE_VOID_VOID     nimcp_cfi_type_id("void(void)")
#define NIMCP_CFI_TYPE_INT_VOID      nimcp_cfi_type_id("int(void)")
#define NIMCP_CFI_TYPE_PTR_VOID      nimcp_cfi_type_id("void*(void)")
#define NIMCP_CFI_TYPE_VOID_PTR      nimcp_cfi_type_id("void(void*)")
#define NIMCP_CFI_TYPE_INT_PTR       nimcp_cfi_type_id("int(void*)")
#define NIMCP_CFI_TYPE_CALLBACK      nimcp_cfi_type_id("void(void*,void*)")

//=============================================================================
// Violation Handling
//=============================================================================

/**
 * @brief CFI violation callback type
 *
 * @param result Violation type
 * @param target Target address
 * @param call_site Call site address
 * @param user_data User context
 * @return true to allow transfer anyway, false to block
 */
typedef bool (*nimcp_cfi_violation_callback_t)(
    nimcp_cfi_result_t result,
    void* target,
    void* call_site,
    void* user_data
);

/**
 * @brief Set CFI violation callback
 *
 * @param cfi CFI context
 * @param callback Violation handler
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_set_violation_handler(
    nimcp_cfi_context_t* cfi,
    nimcp_cfi_violation_callback_t callback,
    void* user_data
);

/**
 * @brief Get CFI violation count
 *
 * @param cfi CFI context
 * @return Number of violations detected
 */
uint64_t nimcp_cfi_get_violation_count(nimcp_cfi_context_t* cfi);

//=============================================================================
// Statistics and Configuration
//=============================================================================

/**
 * @brief Get CFI statistics
 *
 * @param cfi CFI context
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_get_stats(
    nimcp_cfi_context_t* cfi,
    nimcp_cfi_stats_t* stats
);

/**
 * @brief Reset CFI statistics
 *
 * @param cfi CFI context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_reset_stats(nimcp_cfi_context_t* cfi);

/**
 * @brief Set enforcement mode
 *
 * @param cfi CFI context
 * @param mode New enforcement mode
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_cfi_set_mode(nimcp_cfi_context_t* cfi, nimcp_cfi_mode_t mode);

/**
 * @brief Get current enforcement mode
 *
 * @param cfi CFI context
 * @return Current mode
 */
nimcp_cfi_mode_t nimcp_cfi_get_mode(nimcp_cfi_context_t* cfi);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get result name as string
 *
 * @param result CFI result
 * @return Result name string
 */
const char* nimcp_cfi_result_name(nimcp_cfi_result_t result);

/**
 * @brief Get mode name as string
 *
 * @param mode CFI mode
 * @return Mode name string
 */
const char* nimcp_cfi_mode_name(nimcp_cfi_mode_t mode);

/**
 * @brief Get function category name as string
 *
 * @param category Function category
 * @return Category name string
 */
const char* nimcp_cfi_category_name(nimcp_func_type_t category);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Validate indirect call and execute if valid
 *
 * Usage: NIMCP_CFI_CALL(cfi, func_ptr, TYPE_ID, args...)
 */
#define NIMCP_CFI_CALL(cfi, func, type, ...) \
    (nimcp_cfi_validate_ptr((cfi), (void*)(func), (type)) ? \
        (func)(__VA_ARGS__) : (void)0)

/**
 * @brief Check CFI and abort on violation
 */
#define NIMCP_CFI_CHECK_OR_ABORT(cfi, target, type) \
    do { \
        if (!nimcp_cfi_validate_ptr((cfi), (void*)(target), (type))) { \
            nimcp_security_log_event( \
                NIMCP_SECURITY_EVENT_THREAT_DETECTED, \
                NIMCP_THREAT_CRITICAL, \
                "CFI violation - aborting"); \
            abort(); \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CFI_H
