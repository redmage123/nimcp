/**
 * @file nimcp_security_level.h
 * @brief NIMCP Security Level Management
 *
 * WHAT: Security level enforcement with downgrade prevention
 * WHY: Ensures security cannot be weakened once elevated
 * HOW: State locking, level inheritance, and transition validation
 *
 * Features:
 * - Five security levels from MINIMAL to PARANOID
 * - Level locking to prevent downgrades
 * - Component-specific security levels
 * - Level inheritance for child components
 * - Emergency override with audit trail
 * - Feature enablement queries
 * - Bio-async integration for notifications
 * - Comprehensive logging
 */

#ifndef NIMCP_SECURITY_LEVEL_H
#define NIMCP_SECURITY_LEVEL_H

#include "utils/error/nimcp_error_codes.h"
#include "async/nimcp_bio_router.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic number for security state validation */
#define NIMCP_SECURITY_STATE_MAGIC 0x53454355  /* "SECU" */

/**
 * Security level enumeration
 *
 * WHAT: Hierarchical security levels
 * WHY: Different operational contexts require different security
 * HOW: Ordered enum from minimal to paranoid
 */
typedef enum {
    NIMCP_SECURITY_LEVEL_MINIMAL = 0,   /**< Development only, minimal checks */
    NIMCP_SECURITY_LEVEL_STANDARD = 1,  /**< Normal operation, standard checks */
    NIMCP_SECURITY_LEVEL_ELEVATED = 2,  /**< High-security mode, strict checks */
    NIMCP_SECURITY_LEVEL_MAXIMUM = 3,   /**< Maximum restrictions, all checks */
    NIMCP_SECURITY_LEVEL_PARANOID = 4   /**< Everything locked, maximum paranoia */
} nimcp_security_level_t;

/**
 * Security features that can be enabled/disabled
 *
 * WHAT: Individual security features
 * WHY: Different levels enable different features
 * HOW: Bitmap of feature flags
 */
typedef enum {
    NIMCP_SECURITY_FEATURE_NAN_CHECK = 0,
    NIMCP_SECURITY_FEATURE_RANGE_CHECK = 1,
    NIMCP_SECURITY_FEATURE_BUFFER_CHECK = 2,
    NIMCP_SECURITY_FEATURE_RATE_LIMIT = 3,
    NIMCP_SECURITY_FEATURE_AUTHENTICATION = 4,
    NIMCP_SECURITY_FEATURE_ENCRYPTION = 5,
    NIMCP_SECURITY_FEATURE_AUDIT_LOG = 6,
    NIMCP_SECURITY_FEATURE_MEMORY_ZEROING = 7,
    NIMCP_SECURITY_FEATURE_CANARY_CHECK = 8,
    NIMCP_SECURITY_FEATURE_STACK_PROTECTION = 9,
    NIMCP_SECURITY_FEATURE_COUNT
} nimcp_security_feature_t;

/**
 * Security state configuration
 *
 * WHAT: Configuration for security state creation
 * WHY: Customizable security behavior
 * HOW: Initial level, locking policy, component limits
 */
typedef struct {
    nimcp_security_level_t initial_level;     /**< Starting security level */
    bool lock_on_create;                       /**< Lock level immediately */
    size_t max_components;                     /**< Maximum tracked components */
    size_t max_audit_entries;                  /**< Audit trail size */
    bio_router_t* router;                      /**< Bio-async router for notifications */
    void* user_data;                           /**< User-provided context */
} nimcp_security_state_config_t;

/**
 * Audit trail entry
 *
 * WHAT: Record of security level changes
 * WHY: Accountability and forensics
 * HOW: Timestamp, old/new levels, reason
 */
typedef struct {
    time_t timestamp;                          /**< When change occurred */
    nimcp_security_level_t old_level;          /**< Previous level */
    nimcp_security_level_t new_level;          /**< New level */
    char component[64];                        /**< Component name (or empty for global) */
    char reason[256];                          /**< Reason for change */
    char authorization[64];                    /**< Authorization token used */
    bool is_override;                          /**< Was this an emergency override */
} nimcp_security_audit_entry_t;

/**
 * Security state statistics
 *
 * WHAT: Runtime statistics
 * WHY: Monitoring and diagnostics
 * HOW: Counters for transitions, queries, overrides
 */
typedef struct {
    uint64_t level_upgrades;                   /**< Number of upgrades */
    uint64_t level_downgrades_blocked;         /**< Downgrade attempts blocked */
    uint64_t emergency_overrides;              /**< Emergency override count */
    uint64_t component_levels_set;             /**< Component level changes */
    uint64_t feature_queries;                  /**< Feature enabled queries */
    nimcp_security_level_t current_level;      /**< Current global level */
    bool is_locked;                            /**< Is level locked */
    size_t component_count;                    /**< Number of tracked components */
    size_t audit_entry_count;                  /**< Audit entries */
} nimcp_security_state_stats_t;

/* Opaque security state handle */
typedef struct nimcp_security_state* nimcp_security_state_t;

/**
 * Create security state
 *
 * WHAT: Allocates and initializes security state
 * WHY: Manage security levels across system
 * HOW: Allocates state, sets initial level, optionally locks
 *
 * @param config Configuration (NULL for defaults)
 * @return Security state handle or NULL on failure
 */
nimcp_security_state_t nimcp_security_state_create(const nimcp_security_state_config_t* config);

/**
 * Destroy security state
 *
 * WHAT: Frees security state resources
 * WHY: Clean shutdown and memory management
 * HOW: Validates magic, frees components, zeroes memory
 *
 * @param state Security state to destroy
 */
void nimcp_security_state_destroy(nimcp_security_state_t state);

/**
 * Set global security level
 *
 * WHAT: Changes global security level
 * WHY: Respond to operational requirements
 * HOW: Validates upgrade-only transition, updates state, logs change
 *
 * @param state Security state
 * @param level New security level
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_set_level(
    nimcp_security_state_t state,
    nimcp_security_level_t level
);

/**
 * Get global security level
 *
 * WHAT: Retrieves current security level
 * WHY: Check security requirements
 * HOW: Returns current level from state
 *
 * @param state Security state
 * @return Current security level
 */
nimcp_security_level_t nimcp_security_get_level(nimcp_security_state_t state);

/**
 * Lock security level
 *
 * WHAT: Permanently locks security level
 * WHY: Prevent any further changes in production
 * HOW: Sets locked flag, logs action
 *
 * @param state Security state
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_lock_level(nimcp_security_state_t state);

/**
 * Check if level is locked
 *
 * WHAT: Query lock status
 * WHY: Determine if changes are possible
 * HOW: Returns locked flag
 *
 * @param state Security state
 * @return true if locked, false otherwise
 */
bool nimcp_security_is_locked(nimcp_security_state_t state);

/**
 * Set component-specific security level
 *
 * WHAT: Sets security level for individual component
 * WHY: Fine-grained security control
 * HOW: Creates/updates component entry, validates against global level
 *
 * @param state Security state
 * @param component Component name
 * @param level Security level for component
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_set_component_level(
    nimcp_security_state_t state,
    const char* component,
    nimcp_security_level_t level
);

/**
 * Get component security level
 *
 * WHAT: Retrieves component security level
 * WHY: Check component-specific requirements
 * HOW: Looks up component, returns level or inherits from global
 *
 * @param state Security state
 * @param component Component name
 * @return Component security level (or global if not set)
 */
nimcp_security_level_t nimcp_security_get_component_level(
    nimcp_security_state_t state,
    const char* component
);

/**
 * Emergency override
 *
 * WHAT: Allows level change with authorization
 * WHY: Handle emergency situations requiring downgrade
 * HOW: Validates token, logs override, changes level
 *
 * @param state Security state
 * @param level New security level
 * @param authorization_token Authorization token (checked if non-NULL)
 * @param reason Reason for override
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_emergency_override(
    nimcp_security_state_t state,
    nimcp_security_level_t level,
    const char* authorization_token,
    const char* reason
);

/**
 * Check if feature is enabled
 *
 * WHAT: Queries if security feature is enabled at current level
 * WHY: Conditional security checks based on level
 * HOW: Looks up feature table for current level
 *
 * @param state Security state
 * @param feature Feature to query
 * @return true if enabled, false otherwise
 */
bool nimcp_security_feature_enabled(
    nimcp_security_state_t state,
    nimcp_security_feature_t feature
);

/**
 * Get audit trail
 *
 * WHAT: Retrieves audit trail entries
 * WHY: Security forensics and compliance
 * HOW: Copies audit entries to buffer
 *
 * @param state Security state
 * @param entries Buffer for audit entries
 * @param max_entries Maximum entries to retrieve
 * @param count_out Number of entries returned
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_get_audit_trail(
    nimcp_security_state_t state,
    nimcp_security_audit_entry_t* entries,
    size_t max_entries,
    size_t* count_out
);

/**
 * Get statistics
 *
 * WHAT: Retrieves security state statistics
 * WHY: Monitoring and diagnostics
 * HOW: Copies stats structure
 *
 * @param state Security state
 * @param stats Output statistics
 * @return NIMCP_OK or error code
 */
nimcp_error_t nimcp_security_level_get_stats(
    nimcp_security_state_t state,
    nimcp_security_state_stats_t* stats
);

/**
 * Get level name
 *
 * WHAT: Converts level enum to string
 * WHY: Logging and display
 * HOW: Static string table lookup
 *
 * @param level Security level
 * @return String name of level
 */
const char* nimcp_security_level_name(nimcp_security_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_LEVEL_H */
