/**
 * @file nimcp_security_coverage.h
 * @brief Security Coverage Framework - 100% coverage verification system
 *
 * WHAT: Provides comprehensive security coverage verification across all
 *       system dimensions to ensure no blind spots in protection.
 *
 * WHY:  Security gaps are attack vectors. This module verifies that all
 *       critical areas have active protection mechanisms in place.
 *
 * HOW:  Tracks coverage across 8 dimensions (memory, code paths, I/O, etc.)
 *       and provides real-time verification that coverage remains at 100%.
 *
 * COVERAGE DIMENSIONS:
 *   1. Memory Regions     - mprotect + continuous hash verification
 *   2. Code Paths         - CFI + shadow stack + code signing
 *   3. Input Channels     - Validation gates (BBB)
 *   4. Output Channels    - Sanitization + rate limiting
 *   5. Inter-Module IPC   - Authenticated channels + encryption
 *   6. Temporal           - Continuous monitoring (no gaps in time)
 *   7. Thread/Process     - Isolation + capability-based access
 *   8. External Interfaces - Protocol validation + firewalling
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_SECURITY_COVERAGE_H
#define NIMCP_SECURITY_COVERAGE_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of protected memory regions */
#define NIMCP_MAX_PROTECTED_REGIONS 64

/** Maximum number of monitored code paths */
#define NIMCP_MAX_CODE_PATHS 128

/** Maximum number of input/output channels */
#define NIMCP_MAX_CHANNELS 32

/** Maximum number of IPC endpoints */
#define NIMCP_MAX_IPC_ENDPOINTS 64

/** Hash size for integrity verification */
#define NIMCP_COVERAGE_HASH_SIZE 32

/** Coverage check interval in milliseconds */
#define NIMCP_COVERAGE_CHECK_INTERVAL_MS 100

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Security coverage dimensions
 */
typedef enum {
    NIMCP_COVERAGE_MEMORY_REGIONS = 0,    /**< Protected memory regions */
    NIMCP_COVERAGE_CODE_PATHS,            /**< CFI-protected code paths */
    NIMCP_COVERAGE_INPUT_CHANNELS,        /**< Validated input channels */
    NIMCP_COVERAGE_OUTPUT_CHANNELS,       /**< Sanitized output channels */
    NIMCP_COVERAGE_IPC_CHANNELS,          /**< Authenticated IPC */
    NIMCP_COVERAGE_TEMPORAL,              /**< Time-continuous monitoring */
    NIMCP_COVERAGE_THREAD_PROCESS,        /**< Thread/process isolation */
    NIMCP_COVERAGE_EXTERNAL_INTERFACES,   /**< External protocol validation */
    NIMCP_COVERAGE_DIMENSION_COUNT        /**< Total number of dimensions */
} nimcp_coverage_dimension_t;

/**
 * @brief Coverage status for a dimension
 */
typedef enum {
    NIMCP_COVERAGE_STATUS_UNKNOWN = 0,    /**< Not yet verified */
    NIMCP_COVERAGE_STATUS_FULL,           /**< 100% coverage achieved */
    NIMCP_COVERAGE_STATUS_PARTIAL,        /**< Some gaps exist */
    NIMCP_COVERAGE_STATUS_NONE,           /**< No coverage */
    NIMCP_COVERAGE_STATUS_DEGRADED        /**< Was full, now degraded */
} nimcp_coverage_status_t;

/**
 * @brief Protection level for registered items
 */
typedef enum {
    NIMCP_PROTECTION_NONE = 0,
    NIMCP_PROTECTION_READ_ONLY,           /**< mprotect PROT_READ */
    NIMCP_PROTECTION_EXECUTE_ONLY,        /**< mprotect PROT_EXEC */
    NIMCP_PROTECTION_HASH_VERIFIED,       /**< Hash integrity check */
    NIMCP_PROTECTION_ENCRYPTED,           /**< Encrypted storage */
    NIMCP_PROTECTION_FULL                 /**< All protections active */
} nimcp_protection_level_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Protected memory region descriptor
 */
typedef struct {
    void* base_address;                   /**< Start of protected region */
    size_t size;                          /**< Size in bytes */
    nimcp_protection_level_t protection;  /**< Protection level */
    uint8_t hash[NIMCP_COVERAGE_HASH_SIZE]; /**< Integrity hash */
    uint64_t last_verified;               /**< Timestamp of last verification */
    const char* name;                     /**< Human-readable name */
    bool active;                          /**< Is region actively protected */
} nimcp_protected_region_t;

/**
 * @brief Code path descriptor for CFI
 */
typedef struct {
    void* entry_point;                    /**< Function entry address */
    void* expected_return;                /**< Expected return address */
    uint32_t path_id;                     /**< Unique path identifier */
    bool cfi_enabled;                     /**< CFI protection active */
    bool shadow_stack_enabled;            /**< Shadow stack protection */
    uint64_t call_count;                  /**< Number of times called */
    uint64_t violation_count;             /**< Number of CFI violations */
} nimcp_code_path_t;

/**
 * @brief I/O channel descriptor
 */
typedef struct {
    uint32_t channel_id;                  /**< Unique channel ID */
    const char* name;                     /**< Channel name */
    bool is_input;                        /**< true = input, false = output */
    bool validation_enabled;              /**< Input validation active */
    bool sanitization_enabled;            /**< Output sanitization active */
    bool rate_limited;                    /**< Rate limiting active */
    uint64_t bytes_processed;             /**< Total bytes through channel */
    uint64_t violations_blocked;          /**< Violations caught */
} nimcp_channel_t;

/**
 * @brief IPC endpoint descriptor
 */
typedef struct {
    uint32_t endpoint_id;                 /**< Unique endpoint ID */
    const char* name;                     /**< Endpoint name */
    bool authenticated;                   /**< Authentication required */
    bool encrypted;                       /**< Encryption enabled */
    uint32_t capability_mask;             /**< Required capabilities */
    uint64_t messages_sent;               /**< Messages transmitted */
    uint64_t messages_received;           /**< Messages received */
    uint64_t auth_failures;               /**< Authentication failures */
} nimcp_ipc_endpoint_t;

/**
 * @brief Coverage statistics for a single dimension
 */
typedef struct {
    nimcp_coverage_dimension_t dimension; /**< Which dimension */
    nimcp_coverage_status_t status;       /**< Current status */
    uint32_t total_items;                 /**< Total items to protect */
    uint32_t protected_items;             /**< Items with active protection */
    float coverage_percent;               /**< Coverage percentage (0-100) */
    uint64_t last_check;                  /**< Timestamp of last check */
    uint64_t violations_detected;         /**< Total violations found */
} nimcp_dimension_stats_t;

/**
 * @brief Overall security coverage report
 */
typedef struct {
    nimcp_dimension_stats_t dimensions[NIMCP_COVERAGE_DIMENSION_COUNT];
    float overall_coverage;               /**< Aggregate coverage % */
    bool all_dimensions_full;             /**< True if 100% everywhere */
    uint64_t total_violations;            /**< Sum of all violations */
    uint64_t report_timestamp;            /**< When report was generated */
    uint64_t monitoring_uptime_ms;        /**< How long monitoring active */
} nimcp_coverage_report_t;

/**
 * @brief Security coverage context (opaque handle)
 */
typedef struct nimcp_security_coverage nimcp_security_coverage_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create security coverage monitoring context
 *
 * @return Coverage context or NULL on failure
 */
nimcp_security_coverage_t* nimcp_security_coverage_create(void);

/**
 * @brief Initialize coverage system with default protections
 *
 * @param coverage Coverage context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_security_coverage_init(nimcp_security_coverage_t* coverage);

/**
 * @brief Destroy coverage context and free resources
 *
 * @param coverage Coverage context
 */
void nimcp_security_coverage_destroy(nimcp_security_coverage_t* coverage);

//=============================================================================
// Memory Region Registration
//=============================================================================

/**
 * @brief Register a memory region for protection
 *
 * @param coverage Coverage context
 * @param base Base address of region
 * @param size Size in bytes
 * @param protection Protection level to apply
 * @param name Human-readable name for logging
 * @return Region ID or -1 on failure
 */
int32_t nimcp_coverage_register_region(
    nimcp_security_coverage_t* coverage,
    void* base,
    size_t size,
    nimcp_protection_level_t protection,
    const char* name
);

/**
 * @brief Unregister a protected memory region
 *
 * @param coverage Coverage context
 * @param region_id Region ID from registration
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_unregister_region(
    nimcp_security_coverage_t* coverage,
    int32_t region_id
);

/**
 * @brief Verify integrity of a specific region
 *
 * @param coverage Coverage context
 * @param region_id Region to verify
 * @return true if intact, false if tampered
 */
bool nimcp_coverage_verify_region(
    nimcp_security_coverage_t* coverage,
    int32_t region_id
);

/**
 * @brief Verify all registered memory regions
 *
 * @param coverage Coverage context
 * @return true if all intact, false if any tampered
 */
bool nimcp_coverage_verify_all_regions(nimcp_security_coverage_t* coverage);

//=============================================================================
// Code Path Registration (CFI)
//=============================================================================

/**
 * @brief Register a code path for CFI protection
 *
 * @param coverage Coverage context
 * @param entry_point Function entry address
 * @param path_id Unique identifier for this path
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_register_code_path(
    nimcp_security_coverage_t* coverage,
    void* entry_point,
    uint32_t path_id
);

/**
 * @brief Record code path execution (for verification)
 *
 * @param coverage Coverage context
 * @param path_id Path being executed
 * @param return_address Expected return address
 * @return true if path is valid, false if CFI violation
 */
bool nimcp_coverage_record_code_path(
    nimcp_security_coverage_t* coverage,
    uint32_t path_id,
    void* return_address
);

//=============================================================================
// I/O Channel Registration
//=============================================================================

/**
 * @brief Register an input channel
 *
 * @param coverage Coverage context
 * @param name Channel name
 * @param validation_enabled Whether input validation is active
 * @return Channel ID or -1 on failure
 */
int32_t nimcp_coverage_register_input_channel(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool validation_enabled
);

/**
 * @brief Register an output channel
 *
 * @param coverage Coverage context
 * @param name Channel name
 * @param sanitization_enabled Whether output sanitization is active
 * @param rate_limited Whether rate limiting is active
 * @return Channel ID or -1 on failure
 */
int32_t nimcp_coverage_register_output_channel(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool sanitization_enabled,
    bool rate_limited
);

/**
 * @brief Record data passing through a channel
 *
 * @param coverage Coverage context
 * @param channel_id Channel ID
 * @param bytes Number of bytes
 * @param had_violation Whether a violation was detected
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_record_channel_activity(
    nimcp_security_coverage_t* coverage,
    int32_t channel_id,
    size_t bytes,
    bool had_violation
);

//=============================================================================
// IPC Endpoint Registration
//=============================================================================

/**
 * @brief Register an IPC endpoint
 *
 * @param coverage Coverage context
 * @param name Endpoint name
 * @param authenticated Whether authentication is required
 * @param encrypted Whether encryption is enabled
 * @param capability_mask Required capabilities bitmask
 * @return Endpoint ID or -1 on failure
 */
int32_t nimcp_coverage_register_ipc_endpoint(
    nimcp_security_coverage_t* coverage,
    const char* name,
    bool authenticated,
    bool encrypted,
    uint32_t capability_mask
);

/**
 * @brief Record IPC message
 *
 * @param coverage Coverage context
 * @param endpoint_id Endpoint ID
 * @param is_send true for send, false for receive
 * @param auth_success Whether authentication succeeded
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_record_ipc_message(
    nimcp_security_coverage_t* coverage,
    int32_t endpoint_id,
    bool is_send,
    bool auth_success
);

//=============================================================================
// Coverage Verification
//=============================================================================

/**
 * @brief Run full coverage verification across all dimensions
 *
 * @param coverage Coverage context
 * @param report Output: detailed coverage report
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_verify_all(
    nimcp_security_coverage_t* coverage,
    nimcp_coverage_report_t* report
);

/**
 * @brief Verify coverage for a specific dimension
 *
 * @param coverage Coverage context
 * @param dimension Dimension to verify
 * @param stats Output: dimension statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_verify_dimension(
    nimcp_security_coverage_t* coverage,
    nimcp_coverage_dimension_t dimension,
    nimcp_dimension_stats_t* stats
);

/**
 * @brief Check if system has 100% coverage (no blind spots)
 *
 * @param coverage Coverage context
 * @return true if 100% coverage, false if gaps exist
 */
bool nimcp_coverage_is_complete(nimcp_security_coverage_t* coverage);

/**
 * @brief Get current overall coverage percentage
 *
 * @param coverage Coverage context
 * @return Coverage percentage (0.0 - 100.0)
 */
float nimcp_coverage_get_percentage(nimcp_security_coverage_t* coverage);

//=============================================================================
// Coverage Gap Detection
//=============================================================================

/**
 * @brief Identify unprotected memory regions
 *
 * @param coverage Coverage context
 * @param gap_count Output: number of gaps found
 * @return Array of gap addresses (caller must free) or NULL
 */
void** nimcp_coverage_find_memory_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count
);

/**
 * @brief Identify unprotected code paths
 *
 * @param coverage Coverage context
 * @param gap_count Output: number of unprotected paths
 * @return Array of path IDs (caller must free) or NULL
 */
uint32_t* nimcp_coverage_find_code_path_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count
);

/**
 * @brief Identify unprotected channels
 *
 * @param coverage Coverage context
 * @param gap_count Output: number of unprotected channels
 * @return Array of channel IDs (caller must free) or NULL
 */
int32_t* nimcp_coverage_find_channel_gaps(
    nimcp_security_coverage_t* coverage,
    uint32_t* gap_count
);

//=============================================================================
// Temporal Coverage
//=============================================================================

/**
 * @brief Record monitoring heartbeat (proves continuous coverage)
 *
 * @param coverage Coverage context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_coverage_heartbeat(nimcp_security_coverage_t* coverage);

/**
 * @brief Check for temporal gaps in monitoring
 *
 * @param coverage Coverage context
 * @param max_gap_ms Maximum allowed gap in milliseconds
 * @return true if no gaps, false if temporal gap detected
 */
bool nimcp_coverage_check_temporal(
    nimcp_security_coverage_t* coverage,
    uint64_t max_gap_ms
);

/**
 * @brief Get monitoring uptime
 *
 * @param coverage Coverage context
 * @return Uptime in milliseconds
 */
uint64_t nimcp_coverage_get_uptime(nimcp_security_coverage_t* coverage);

//=============================================================================
// Reporting
//=============================================================================

/**
 * @brief Generate human-readable coverage report
 *
 * @param coverage Coverage context
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or -1 on error
 */
int32_t nimcp_coverage_generate_report(
    nimcp_security_coverage_t* coverage,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Get dimension name as string
 *
 * @param dimension Dimension enum value
 * @return Dimension name string
 */
const char* nimcp_coverage_dimension_name(nimcp_coverage_dimension_t dimension);

/**
 * @brief Get status name as string
 *
 * @param status Status enum value
 * @return Status name string
 */
const char* nimcp_coverage_status_name(nimcp_coverage_status_t status);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SECURITY_COVERAGE_H
