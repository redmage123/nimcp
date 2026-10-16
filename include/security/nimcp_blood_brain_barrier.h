/**
 * @file nimcp_blood_brain_barrier.h
 * @brief Blood-Brain Barrier - Perimeter Defense Layer for NIMCP Brain
 *
 * WHAT: Unified API for perimeter security protecting the NIMCP brain
 * WHY:  Prevent code injection, memory corruption, and unauthorized access
 * HOW:  Four-layer defense: Input Gate, Code Signing, Memory Boundary, Access Control
 *
 * BIOLOGICAL MODEL:
 * ```
 * BIOLOGICAL                          NIMCP IMPLEMENTATION
 * ─────────────────────────────────────────────────────────────────
 * Endothelial cells (tight junctions) → Input validation gates
 * Basement membrane                   → Code signing verification layer
 * Astrocyte end-feet                  → Memory boundary monitors
 * Pericytes                           → Access control enforcers
 * ```
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════╗
 * ║              BLOOD-BRAIN BARRIER (Perimeter Defense)              ║
 * ║  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ║
 * ║  │   Input     │ │    Code     │ │   Memory    │ │   Access    │ ║
 * ║  │ Validation  │ │  Signing    │ │  Boundary   │ │   Control   │ ║
 * ║  │   Gates     │ │ Verifier    │ │  Monitor    │ │  Enforcer   │ ║
 * ║  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘ ║
 * ╚═══════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe operations
 *
 * @author NIMCP Team
 * @date 2025-11-24
 */

#ifndef NIMCP_BLOOD_BRAIN_BARRIER_H
#define NIMCP_BLOOD_BRAIN_BARRIER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>  /* For ssize_t */

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Export Macro
//=============================================================================

#include "common/nimcp_export.h"

//=============================================================================
// Forward Declarations
//=============================================================================

#ifndef BBB_SYSTEM_T_DEFINED
#define BBB_SYSTEM_T_DEFINED
typedef struct bbb_system_struct* bbb_system_t;
#endif
typedef struct bbb_input_gate_struct* bbb_input_gate_t;
typedef struct bbb_code_signer_struct* bbb_code_signer_t;
typedef struct bbb_memory_boundary_struct* bbb_memory_boundary_t;
typedef struct bbb_access_control_struct* bbb_access_control_t;
typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// BBB Threat Types
//=============================================================================

/**
 * @brief Categories of security threats detected by BBB
 */
#ifndef BBB_THREAT_TYPE_DEFINED
#define BBB_THREAT_TYPE_DEFINED
typedef enum {
    BBB_THREAT_NONE = 0,              /**< No threat detected */
    BBB_THREAT_BUFFER_OVERFLOW,       /**< Stack/heap buffer overflow */
    BBB_THREAT_FORMAT_STRING,         /**< Format string vulnerability */
    BBB_THREAT_INTEGER_OVERFLOW,      /**< Integer overflow/underflow */
    BBB_THREAT_SQL_INJECTION,         /**< SQL injection attempt */
    BBB_THREAT_CODE_INJECTION,        /**< Generic code injection */
    BBB_THREAT_SHELLCODE,             /**< Shellcode detected */
    BBB_THREAT_ROP_CHAIN,             /**< Return-Oriented Programming */
    BBB_THREAT_INVALID_SIGNATURE,     /**< Code signature invalid */
    BBB_THREAT_MEMORY_VIOLATION,      /**< Memory bounds violation */
    BBB_THREAT_UNAUTHORIZED_ACCESS,   /**< Access control violation */
    BBB_THREAT_DATA_TAMPERING,        /**< Data integrity violation */
    BBB_THREAT_PATH_TRAVERSAL,        /**< Path traversal attack */
    BBB_THREAT_SHELL_INJECTION,       /**< Shell command injection */
    BBB_THREAT_UNKNOWN                /**< Unknown threat type */
} bbb_threat_type_t;
#endif /* BBB_THREAT_TYPE_DEFINED */

/**
 * @brief Severity levels for detected threats
 */
typedef enum {
    BBB_SEVERITY_NONE = 0,            /**< No threat */
    BBB_SEVERITY_LOW = 1,             /**< Low severity - log only */
    BBB_SEVERITY_MEDIUM = 2,          /**< Medium - block and alert */
    BBB_SEVERITY_HIGH = 3,            /**< High - quarantine and alert */
    BBB_SEVERITY_CRITICAL = 4         /**< Critical - system lockdown */
} bbb_severity_t;

/**
 * @brief Actions to take when threat is detected
 */
typedef enum {
    BBB_ACTION_ALLOW = 0,             /**< Allow operation */
    BBB_ACTION_LOG,                   /**< Log but allow */
    BBB_ACTION_BLOCK,                 /**< Block operation */
    BBB_ACTION_QUARANTINE,            /**< Isolate threat */
    BBB_ACTION_TERMINATE,             /**< Terminate process */
    BBB_ACTION_LOCKDOWN               /**< Full system lockdown */
} bbb_action_t;

//=============================================================================
// BBB Configuration
//=============================================================================

/**
 * @brief Input gate configuration
 */
typedef struct {
    bool validate_strings;            /**< Enable string validation */
    bool validate_integers;           /**< Enable integer range checking */
    bool validate_pointers;           /**< Enable pointer validation */
    bool sanitize_html;               /**< Sanitize HTML in strings */
    bool sanitize_sql;                /**< Sanitize SQL in strings */
    size_t max_string_length;         /**< Maximum allowed string length */
    size_t max_array_size;            /**< Maximum allowed array size */
    int64_t min_integer;              /**< Minimum allowed integer value */
    int64_t max_integer;              /**< Maximum allowed integer value */
} bbb_input_config_t;

/**
 * @brief Code signing configuration
 */
typedef struct {
    bool require_signatures;          /**< Require all code to be signed */
    bool verify_on_load;              /**< Verify signatures when loading */
    bool verify_on_execute;           /**< Verify before execution */
    const char* trusted_keys_path;    /**< Path to trusted public keys */
    uint32_t hash_algorithm;          /**< Hash algorithm (SHA256, etc.) */
} bbb_signing_config_t;

/**
 * @brief Memory boundary configuration
 */
typedef struct {
    bool enable_stack_canaries;       /**< Enable stack canary protection */
    bool enable_heap_guards;          /**< Enable heap guard pages */
    bool enable_aslr;                 /**< Enable address randomization */
    bool enable_wx_protection;        /**< Enable W^X (write XOR execute) */
    bool enable_mprotect;             /**< Enable mprotect for regions */
    size_t guard_page_size;           /**< Size of guard pages */
    size_t stack_canary_size;         /**< Size of stack canaries */
} bbb_memory_config_t;

/**
 * @brief Access control configuration
 */
typedef struct {
    bool enable_rbac;                 /**< Enable role-based access control */
    bool enable_mac;                  /**< Enable mandatory access control */
    bool enable_capability;           /**< Enable capability-based control */
    bool log_access_attempts;         /**< Log all access attempts */
    uint32_t max_privilege_level;     /**< Maximum allowed privilege level */
    const char* policy_file;          /**< Path to access policy file */
} bbb_access_config_t;

/**
 * @brief Complete BBB system configuration
 */
typedef struct {
    bbb_input_config_t input;         /**< Input gate configuration */
    bbb_signing_config_t signing;     /**< Code signing configuration */
    bbb_memory_config_t memory;       /**< Memory boundary configuration */
    bbb_access_config_t access;       /**< Access control configuration */
    bool strict_mode;                 /**< Enable strict security mode */
    bbb_action_t default_action;      /**< Default action for threats */
    void (*alert_callback)(bbb_threat_type_t, bbb_severity_t, const char*);
} bbb_config_t;

//=============================================================================
// BBB Threat Report
//=============================================================================

/**
 * @brief Detailed report of a detected threat
 */
typedef struct {
    bbb_threat_type_t type;           /**< Type of threat */
    bbb_severity_t severity;          /**< Severity level */
    bbb_action_t action_taken;        /**< Action that was taken */
    uint64_t timestamp;               /**< When detected (Unix time) */
    const void* source_address;       /**< Memory address of threat source */
    size_t threat_size;               /**< Size of threatening data */
    char description[256];            /**< Human-readable description */
    uint8_t threat_hash[32];          /**< SHA256 hash of threat data */
    bool quarantined;                 /**< Whether threat was quarantined */
} bbb_threat_report_t;

//=============================================================================
// BBB Validation Result
//=============================================================================

/**
 * @brief Result of input/data validation
 */
typedef struct {
    bool valid;                       /**< Whether input is valid */
    bbb_threat_type_t threat;         /**< Detected threat (if any) */
    bbb_severity_t severity;          /**< Severity (if threat detected) */
    char reason[128];                 /**< Reason for rejection (if invalid) */
    void* sanitized_data;             /**< Sanitized version (if applicable) */
    size_t sanitized_size;            /**< Size of sanitized data */
} bbb_validation_result_t;

//=============================================================================
// BBB Statistics
//=============================================================================

/**
 * @brief BBB system statistics
 */
typedef struct {
    uint64_t total_validations;       /**< Total validations performed */
    uint64_t threats_detected;        /**< Total threats detected */
    uint64_t threats_blocked;         /**< Threats that were blocked */
    uint64_t threats_quarantined;     /**< Threats that were quarantined */
    uint64_t false_positives;         /**< Known false positives */
    uint64_t signatures_verified;     /**< Code signatures verified */
    uint64_t signatures_failed;       /**< Signature failures */
    uint64_t memory_violations;       /**< Memory boundary violations */
    uint64_t access_violations;       /**< Access control violations */
    uint64_t uptime_seconds;          /**< System uptime */
} bbb_statistics_t;

//=============================================================================
// BBB System API
//=============================================================================

/**
 * @brief Get default BBB configuration
 * @return Default configuration with conservative security settings
 */
NIMCP_EXPORT bbb_config_t bbb_default_config(void);

/**
 * @brief Create BBB system with configuration
 * @param config Configuration (NULL for defaults)
 * @return BBB system handle, or NULL on failure
 */
NIMCP_EXPORT bbb_system_t bbb_system_create(const bbb_config_t* config);

/**
 * @brief Destroy BBB system and free resources
 * @param system BBB system handle
 */
NIMCP_EXPORT void bbb_system_destroy(bbb_system_t system);

/**
 * @brief Enable/disable the BBB system
 * @param system BBB system handle
 * @param enabled Whether to enable
 * @return true on success
 */
NIMCP_EXPORT bool bbb_system_set_enabled(bbb_system_t system, bool enabled);

/**
 * @brief Check if BBB system is enabled
 * @param system BBB system handle
 * @return true if enabled
 */
NIMCP_EXPORT bool bbb_system_is_enabled(bbb_system_t system);

/**
 * @brief Get BBB system statistics
 * @param system BBB system handle
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool bbb_system_get_statistics(bbb_system_t system, bbb_statistics_t* stats);

/**
 * @brief Reset BBB system statistics
 * @param system BBB system handle
 */
NIMCP_EXPORT void bbb_system_reset_statistics(bbb_system_t system);

//=============================================================================
// Input Gate API
//=============================================================================

/**
 * @brief Validate input data
 * @param system BBB system handle
 * @param data Input data to validate
 * @param size Size of input data
 * @param result Output validation result
 * @return true if valid (no threats detected)
 */
NIMCP_EXPORT bool bbb_validate_input(bbb_system_t system, const void* data,
                                     size_t size, bbb_validation_result_t* result);

/**
 * @brief Validate string input
 * @param system BBB system handle
 * @param str String to validate
 * @param result Output validation result
 * @return true if valid
 */
NIMCP_EXPORT bool bbb_validate_string(bbb_system_t system, const char* str,
                                      bbb_validation_result_t* result);

/**
 * @brief Validate integer value
 * @param system BBB system handle
 * @param value Integer to validate
 * @param result Output validation result
 * @return true if valid
 */
NIMCP_EXPORT bool bbb_validate_integer(bbb_system_t system, int64_t value,
                                       bbb_validation_result_t* result);

/**
 * @brief Validate pointer
 * @param system BBB system handle
 * @param ptr Pointer to validate
 * @param expected_size Expected accessible size
 * @param result Output validation result
 * @return true if valid
 */
NIMCP_EXPORT bool bbb_validate_pointer(bbb_system_t system, const void* ptr,
                                       size_t expected_size, bbb_validation_result_t* result);

/**
 * @brief Sanitize input string (remove dangerous content)
 * @param system BBB system handle
 * @param input Input string
 * @param output Output buffer for sanitized string
 * @param output_size Size of output buffer
 * @return Length of sanitized string, or -1 on error
 */
NIMCP_EXPORT ssize_t bbb_sanitize_string(bbb_system_t system, const char* input,
                                         char* output, size_t output_size);

//=============================================================================
// Code Signing API
//=============================================================================

/**
 * @brief Sign code/data with private key
 * @param system BBB system handle
 * @param data Data to sign
 * @param size Size of data
 * @param signature Output signature buffer
 * @param sig_size Size of signature buffer
 * @return Signature length, or -1 on error
 */
NIMCP_EXPORT ssize_t bbb_sign_code(bbb_system_t system, const void* data,
                                   size_t size, uint8_t* signature, size_t sig_size);

/**
 * @brief Verify code/data signature
 * @param system BBB system handle
 * @param data Data that was signed
 * @param size Size of data
 * @param signature Signature to verify
 * @param sig_size Size of signature
 * @return true if signature is valid
 */
NIMCP_EXPORT bool bbb_verify_signature(bbb_system_t system, const void* data,
                                       size_t size, const uint8_t* signature, size_t sig_size);

/**
 * @brief Add trusted public key
 * @param system BBB system handle
 * @param key_data Public key data
 * @param key_size Size of key data
 * @param key_id Unique identifier for key
 * @return true on success
 */
NIMCP_EXPORT bool bbb_add_trusted_key(bbb_system_t system, const uint8_t* key_data,
                                      size_t key_size, const char* key_id);

/**
 * @brief Remove trusted public key
 * @param system BBB system handle
 * @param key_id Key identifier to remove
 * @return true on success
 */
NIMCP_EXPORT bool bbb_remove_trusted_key(bbb_system_t system, const char* key_id);

/**
 * @brief Calculate hash of data
 * @param data Data to hash
 * @param size Size of data
 * @param hash Output hash buffer (32 bytes for SHA256)
 * @return true on success
 */
NIMCP_EXPORT bool bbb_calculate_hash(const void* data, size_t size, uint8_t* hash);

/**
 * @brief Set HMAC signing key for code signing
 * @param key_data Key material (must remain valid for lifetime of use)
 * @param key_size Size of key in bytes (minimum 16, maximum 256)
 * @return true on success
 *
 * SECURITY: Must be called before bbb_sign_code() or bbb_verify_signature()
 * Key should be loaded from secure storage (HSM/TPM/encrypted config)
 */
NIMCP_EXPORT bool bbb_set_signing_key(const uint8_t* key_data, size_t key_size);

/**
 * @brief Clear HMAC signing key
 *
 * SECURITY: Call during shutdown or key rotation
 */
NIMCP_EXPORT void bbb_clear_signing_key(void);

//=============================================================================
// Memory Boundary API
//=============================================================================

/**
 * @brief Register memory region for protection
 * @param system BBB system handle
 * @param address Start address of region
 * @param size Size of region
 * @param read_only Whether region is read-only
 * @return Region ID, or 0 on failure
 */
NIMCP_EXPORT uint32_t bbb_register_memory_region(bbb_system_t system, void* address,
                                                  size_t size, bool read_only);

/**
 * @brief Unregister memory region
 * @param system BBB system handle
 * @param region_id Region ID to unregister
 * @return true on success
 */
NIMCP_EXPORT bool bbb_unregister_memory_region(bbb_system_t system, uint32_t region_id);

/**
 * @brief Check if memory access is valid
 * @param system BBB system handle
 * @param address Address to check
 * @param size Size of access
 * @param write Whether this is a write access
 * @return true if access is valid
 */
NIMCP_EXPORT bool bbb_check_memory_access(bbb_system_t system, const void* address,
                                          size_t size, bool write);

/**
 * @brief Protect memory region with mprotect
 * @param system BBB system handle
 * @param address Start address (page-aligned)
 * @param size Size of region
 * @param read Allow read
 * @param write Allow write
 * @param execute Allow execute
 * @return true on success
 */
NIMCP_EXPORT bool bbb_protect_memory(bbb_system_t system, void* address, size_t size,
                                     bool read, bool write, bool execute);

/**
 * @brief Install stack canary for function
 * @param system BBB system handle
 * @param stack_ptr Stack pointer
 * @return Canary value installed
 */
NIMCP_EXPORT uint64_t bbb_install_stack_canary(bbb_system_t system, void* stack_ptr);

/**
 * @brief Verify stack canary
 * @param system BBB system handle
 * @param stack_ptr Stack pointer
 * @param expected_canary Expected canary value
 * @return true if canary is intact
 */
NIMCP_EXPORT bool bbb_verify_stack_canary(bbb_system_t system, void* stack_ptr,
                                          uint64_t expected_canary);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Access control subject (entity requesting access)
 */
typedef struct {
    uint32_t id;                      /**< Subject identifier */
    uint32_t privilege_level;         /**< Privilege level (0=lowest) */
    uint32_t roles;                   /**< Bitmask of roles */
    uint64_t capabilities;            /**< Capability bitmask */
} bbb_subject_t;

/**
 * @brief Access control object (resource being accessed)
 */
typedef struct {
    uint32_t id;                      /**< Object identifier */
    uint32_t required_privilege;      /**< Minimum privilege required */
    uint32_t required_roles;          /**< Required roles (bitmask) */
    uint64_t required_capabilities;   /**< Required capabilities */
} bbb_object_t;

/**
 * @brief Check if access is permitted
 * @param system BBB system handle
 * @param subject Subject requesting access
 * @param object Object being accessed
 * @param access_type Type of access (read/write/execute)
 * @return true if access is permitted
 */
NIMCP_EXPORT bool bbb_check_access(bbb_system_t system, const bbb_subject_t* subject,
                                   const bbb_object_t* object, uint32_t access_type);

/**
 * @brief Register access control subject
 * @param system BBB system handle
 * @param subject Subject to register
 * @return true on success
 */
NIMCP_EXPORT bool bbb_register_subject(bbb_system_t system, const bbb_subject_t* subject);

/**
 * @brief Register access control object
 * @param system BBB system handle
 * @param object Object to register
 * @return true on success
 */
NIMCP_EXPORT bool bbb_register_object(bbb_system_t system, const bbb_object_t* object);

/**
 * @brief Grant capability to subject
 * @param system BBB system handle
 * @param subject_id Subject identifier
 * @param capability Capability to grant
 * @return true on success
 */
NIMCP_EXPORT bool bbb_grant_capability(bbb_system_t system, uint32_t subject_id,
                                       uint64_t capability);

/**
 * @brief Revoke capability from subject
 * @param system BBB system handle
 * @param subject_id Subject identifier
 * @param capability Capability to revoke
 * @return true on success
 */
NIMCP_EXPORT bool bbb_revoke_capability(bbb_system_t system, uint32_t subject_id,
                                        uint64_t capability);

/**
 * @brief Reset all BBB subsystem state (for testing)
 *
 * WHAT: Clear all registered subjects, objects, and memory regions
 * WHY:  Enable test isolation by resetting between test cases
 *
 * NOTE: This function is primarily for testing purposes.
 *       Resets: access control, memory boundary, and other stateful subsystems.
 */
void bbb_reset_test_state(void);

//=============================================================================
// Brain Immune System Integration API
//=============================================================================

/**
 * @brief Connect BBB to brain immune system
 *
 * WHAT: Link BBB threat detection to immune system
 * WHY:  Enable automatic threat forwarding and coordinated response
 * HOW:  Store immune system reference for automatic antigen presentation
 *
 * INTEGRATION FEATURES:
 * - BBB threats automatically presented as antigens
 * - BBB severity mapped to immune inflammation levels
 * - BBB quarantine actions trigger killer T cell activation
 * - Coordinated response between BBB and immune antibodies
 *
 * @param system BBB system handle
 * @param immune_system Brain immune system handle
 * @return true on success, false on error
 */
NIMCP_EXPORT bool bbb_connect_immune(bbb_system_t system, brain_immune_system_t* immune_system);

//=============================================================================
// Threat Response API
//=============================================================================

/**
 * @brief Report threat to BBB system
 * @param system BBB system handle
 * @param type Threat type
 * @param severity Severity level
 * @param description Human-readable description
 * @param source_address Source of threat
 * @param threat_data Threat data (for analysis)
 * @param threat_size Size of threat data
 * @return Threat report
 */
NIMCP_EXPORT bbb_threat_report_t bbb_report_threat(bbb_system_t system,
                                                    bbb_threat_type_t type,
                                                    bbb_severity_t severity,
                                                    const char* description,
                                                    const void* source_address,
                                                    const void* threat_data,
                                                    size_t threat_size);

/**
 * @brief Get recent threat reports
 * @param system BBB system handle
 * @param reports Output buffer for reports
 * @param max_reports Maximum reports to retrieve
 * @return Number of reports retrieved
 */
NIMCP_EXPORT size_t bbb_get_threat_reports(bbb_system_t system,
                                           bbb_threat_report_t* reports,
                                           size_t max_reports);

/**
 * @brief Clear threat reports
 * @param system BBB system handle
 */
NIMCP_EXPORT void bbb_clear_threat_reports(bbb_system_t system);

/**
 * @brief Quarantine memory region containing threat
 * @param system BBB system handle
 * @param address Start address of region
 * @param size Size of region
 * @return true on success
 */
NIMCP_EXPORT bool bbb_quarantine_region(bbb_system_t system, void* address, size_t size);

/**
 * @brief Check if a memory region is quarantined
 *
 * WHAT: Test if address overlaps any quarantined region
 * WHY:  Prevent access to quarantined memory
 *
 * @param system BBB system handle
 * @param address Address to check
 * @param size Size of region to check
 * @return true if region overlaps quarantine
 */
NIMCP_EXPORT bool bbb_is_quarantined(bbb_system_t system, const void* address, size_t size);

/**
 * @brief TOCTOU-safe quarantine check with optional reference acquisition
 *
 * WHAT: Atomically check quarantine status and optionally acquire reference
 * WHY:  Prevents race condition between check and use
 * HOW:  If acquire_ref is true and region is not quarantined, acquires reference
 *
 * @param system BBB system handle
 * @param address Address to check
 * @param size Size of region to check
 * @param acquire_ref If true and not quarantined, acquire reference (must call bbb_release_quarantine_ref)
 * @return true if quarantined, false if not quarantined (and reference acquired if requested)
 *
 * @note Caller MUST call bbb_release_quarantine_ref() when done if acquire_ref was true
 *       and this function returned false.
 */
NIMCP_EXPORT bool bbb_is_quarantined_safe(bbb_system_t system, const void* address, size_t size, bool acquire_ref);

/**
 * @brief Release quarantine reference for a specific region
 *
 * WHAT: Release reference acquired during TOCTOU-safe check for a specific region
 * WHY:  Targeted release that only decrements the matching region's ref_count
 * HOW:  Finds overlapping quarantine entry and atomically decrements ref_count
 *
 * @param system BBB system handle
 * @param address Address of the region to release
 * @param size Size of the region to release
 */
NIMCP_EXPORT void bbb_release_quarantine_ref_for_region(bbb_system_t system,
                                                          const void* address,
                                                          size_t size);

/**
 * @brief Release quarantine reference acquired by bbb_is_quarantined_safe (DEPRECATED)
 *
 * WHAT: Release reference acquired during TOCTOU-safe check
 * WHY:  Backward compatibility - prefer bbb_release_quarantine_ref_for_region()
 *
 * @param system BBB system handle
 * @deprecated Use bbb_release_quarantine_ref_for_region() instead
 */
NIMCP_EXPORT void bbb_release_quarantine_ref(bbb_system_t system);

/**
 * @brief Release quarantined region
 * @param system BBB system handle
 * @param address Start address of region
 * @return true on success, false if region has active references or not found
 */
NIMCP_EXPORT bool bbb_release_quarantine(bbb_system_t system, void* address);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get threat type name
 * @param type Threat type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* bbb_threat_type_name(bbb_threat_type_t type);

/**
 * @brief Get severity level name
 * @param severity Severity level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* bbb_severity_name(bbb_severity_t severity);

/**
 * @brief Get action name
 * @param action Action type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* bbb_action_name(bbb_action_t action);

/**
 * @brief Print BBB statistics to stdout
 * @param stats Statistics to print
 */
NIMCP_EXPORT void bbb_print_statistics(const bbb_statistics_t* stats);

/**
 * @brief Print threat report to stdout
 * @param report Report to print
 */
NIMCP_EXPORT void bbb_print_threat_report(const bbb_threat_report_t* report);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BLOOD_BRAIN_BARRIER_H */
