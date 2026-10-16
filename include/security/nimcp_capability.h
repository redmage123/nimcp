/**
 * @file nimcp_capability.h
 * @brief Capability-Based Access Control
 *
 * WHAT: Provides fine-grained access control using unforgeable capability
 *       tokens rather than identity-based permissions.
 *
 * WHY:  Capability-based security enables the principle of least privilege
 *       by default. Components only have access to resources they've been
 *       explicitly granted capabilities for.
 *
 * HOW:  Capabilities are opaque tokens that grant specific permissions.
 *       Operations require presenting the appropriate capability.
 *       Capabilities can be attenuated (restricted) but not amplified.
 *
 * DESIGN PRINCIPLES:
 *   - No ambient authority: all access requires explicit capability
 *   - Capabilities are unforgeable: implemented as indices + generation IDs
 *   - Capabilities can be revoked
 *   - Supports delegation with attenuation
 *
 * Part of Phase SC-1: Security Coverage Framework (Tier 0.7)
 */

#ifndef NIMCP_CAPABILITY_H
#define NIMCP_CAPABILITY_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of capabilities */
#define NIMCP_CAP_MAX_CAPABILITIES 1024

/** Maximum number of capability holders */
#define NIMCP_CAP_MAX_HOLDERS 256

/** Invalid capability handle */
#define NIMCP_CAP_INVALID ((nimcp_capability_t){0, 0, 0})

/** Root capability (all permissions) */
#define NIMCP_CAP_ROOT_ID 0

//=============================================================================
// Permission Flags (Bitmask)
//=============================================================================

/** No permissions */
#define NIMCP_PERM_NONE          0x00000000

/** Read permission */
#define NIMCP_PERM_READ          0x00000001

/** Write permission */
#define NIMCP_PERM_WRITE         0x00000002

/** Execute permission */
#define NIMCP_PERM_EXECUTE       0x00000004

/** Delete permission */
#define NIMCP_PERM_DELETE        0x00000008

/** Create permission */
#define NIMCP_PERM_CREATE        0x00000010

/** Delegate capability to others */
#define NIMCP_PERM_DELEGATE      0x00000020

/** Revoke delegated capabilities */
#define NIMCP_PERM_REVOKE        0x00000040

/** Access neural network weights */
#define NIMCP_PERM_NEURAL_WEIGHTS 0x00000100

/** Modify learning parameters */
#define NIMCP_PERM_NEURAL_LEARN   0x00000200

/** Access security subsystem */
#define NIMCP_PERM_SECURITY      0x00001000

/** Access core directives */
#define NIMCP_PERM_DIRECTIVES    0x00002000

/** Modify system configuration */
#define NIMCP_PERM_CONFIG        0x00004000

/** All permissions */
#define NIMCP_PERM_ALL           0xFFFFFFFF

//=============================================================================
// Resource Types
//=============================================================================

/**
 * @brief Resource types that capabilities can protect
 */
typedef enum {
    NIMCP_RES_GENERIC = 0,           /**< Generic resource */
    NIMCP_RES_MEMORY,                /**< Memory region */
    NIMCP_RES_FILE,                  /**< File/data store */
    NIMCP_RES_NEURAL_NETWORK,        /**< Neural network */
    NIMCP_RES_NEURAL_LAYER,          /**< Specific layer */
    NIMCP_RES_NEURAL_SYNAPSE,        /**< Synapse connections */
    NIMCP_RES_SECURITY,              /**< Security subsystem */
    NIMCP_RES_DIRECTIVE,             /**< Core directive */
    NIMCP_RES_IPC_CHANNEL,           /**< IPC channel */
    NIMCP_RES_THREAD,                /**< Thread/execution context */
    NIMCP_RES_MODULE                 /**< Module/component */
} nimcp_resource_type_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Capability token (unforgeable reference)
 *
 * The combination of index + generation makes capabilities unforgeable.
 * Even if an attacker guesses the index, the generation must match.
 */
typedef struct {
    uint32_t index;                  /**< Index into capability table */
    uint32_t generation;             /**< Generation ID (increments on reuse) */
    uint32_t permissions;            /**< Granted permissions (bitmask) */
} nimcp_capability_t;

/**
 * @brief Capability metadata (internal)
 */
typedef struct {
    uint32_t generation;             /**< Current generation */
    uint32_t permissions;            /**< Granted permissions */
    nimcp_resource_type_t resource_type; /**< Type of protected resource */
    void* resource_ptr;              /**< Pointer to actual resource */
    uint32_t parent_cap;             /**< Parent capability (for delegation) */
    uint32_t holder_id;              /**< Who holds this capability */
    bool valid;                      /**< Is capability valid */
    bool revoked;                    /**< Has been revoked */
    uint64_t created_time;           /**< When created */
    uint64_t last_used;              /**< Last access time */
    uint64_t use_count;              /**< Number of times used */
} nimcp_cap_entry_t;

/**
 * @brief Capability holder (entity that can hold capabilities)
 */
typedef struct {
    uint32_t holder_id;              /**< Unique holder ID */
    const char* name;                /**< Holder name */
    uint32_t* capabilities;          /**< Array of held capability indices */
    uint32_t num_capabilities;       /**< Number of held capabilities */
    uint32_t max_capabilities;       /**< Maximum capabilities */
    bool active;                     /**< Is holder active */
} nimcp_cap_holder_t;

/**
 * @brief Capability system statistics
 */
typedef struct {
    uint32_t total_capabilities;     /**< Total capabilities created */
    uint32_t active_capabilities;    /**< Currently valid capabilities */
    uint32_t revoked_capabilities;   /**< Revoked capabilities */
    uint64_t checks_performed;       /**< Permission checks performed */
    uint64_t checks_passed;          /**< Checks that passed */
    uint64_t checks_failed;          /**< Checks that failed */
    uint64_t delegations;            /**< Capability delegations */
    uint64_t revocations;            /**< Capability revocations */
} nimcp_cap_stats_t;

/**
 * @brief Capability system context (opaque handle)
 */
typedef struct nimcp_capability_system nimcp_capability_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create capability system
 *
 * @return Capability system context or NULL on failure
 */
nimcp_capability_system_t* nimcp_capability_system_create(void);

/**
 * @brief Initialize capability system
 *
 * @param caps Capability system context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_system_init(nimcp_capability_system_t* caps);

/**
 * @brief Destroy capability system
 *
 * @param caps Capability system context
 */
void nimcp_capability_system_destroy(nimcp_capability_system_t* caps);

//=============================================================================
// Capability Creation
//=============================================================================

/**
 * @brief Create a new capability for a resource
 *
 * @param caps Capability system
 * @param resource_type Type of resource
 * @param resource_ptr Pointer to resource (can be NULL for abstract caps)
 * @param permissions Granted permissions (bitmask)
 * @param capability Output: created capability token
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_create(
    nimcp_capability_system_t* caps,
    nimcp_resource_type_t resource_type,
    void* resource_ptr,
    uint32_t permissions,
    nimcp_capability_t* capability
);

/**
 * @brief Create root capability (all permissions)
 *
 * Should only be called during system initialization.
 *
 * @param caps Capability system
 * @param capability Output: root capability
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_create_root(
    nimcp_capability_system_t* caps,
    nimcp_capability_t* capability
);

/**
 * @brief Delegate capability with attenuation
 *
 * Creates a new capability with equal or fewer permissions.
 *
 * @param caps Capability system
 * @param parent Parent capability (must have DELEGATE permission)
 * @param permissions New permissions (must be subset of parent)
 * @param child Output: delegated capability
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_delegate(
    nimcp_capability_system_t* caps,
    nimcp_capability_t parent,
    uint32_t permissions,
    nimcp_capability_t* child
);

//=============================================================================
// Capability Verification
//=============================================================================

/**
 * @brief Check if capability is valid
 *
 * @param caps Capability system
 * @param capability Capability to check
 * @return true if valid and not revoked
 */
bool nimcp_capability_is_valid(
    nimcp_capability_system_t* caps,
    nimcp_capability_t capability
);

/**
 * @brief Check if capability grants specific permission
 *
 * @param caps Capability system
 * @param capability Capability to check
 * @param permission Required permission
 * @return true if permission granted
 */
bool nimcp_capability_check(
    nimcp_capability_system_t* caps,
    nimcp_capability_t capability,
    uint32_t permission
);

/**
 * @brief Check capability for resource access
 *
 * @param caps Capability system
 * @param capability Capability to check
 * @param resource_ptr Resource being accessed
 * @param permission Required permission
 * @return true if access allowed
 */
bool nimcp_capability_check_access(
    nimcp_capability_system_t* caps,
    nimcp_capability_t capability,
    void* resource_ptr,
    uint32_t permission
);

/**
 * @brief Get permissions from capability
 *
 * @param caps Capability system
 * @param capability Capability to query
 * @return Permission bitmask or 0 if invalid
 */
uint32_t nimcp_capability_get_permissions(
    nimcp_capability_system_t* caps,
    nimcp_capability_t capability
);

//=============================================================================
// Capability Revocation
//=============================================================================

/**
 * @brief Revoke a capability
 *
 * Also revokes all capabilities delegated from this one.
 *
 * @param caps Capability system
 * @param capability Capability to revoke
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_revoke(
    nimcp_capability_system_t* caps,
    nimcp_capability_t capability
);

/**
 * @brief Revoke all capabilities for a resource
 *
 * @param caps Capability system
 * @param resource_ptr Resource pointer
 * @return Number of capabilities revoked
 */
uint32_t nimcp_capability_revoke_for_resource(
    nimcp_capability_system_t* caps,
    void* resource_ptr
);

/**
 * @brief Revoke all capabilities held by a holder
 *
 * @param caps Capability system
 * @param holder_id Holder ID
 * @return Number of capabilities revoked
 */
uint32_t nimcp_capability_revoke_holder(
    nimcp_capability_system_t* caps,
    uint32_t holder_id
);

//=============================================================================
// Holder Management
//=============================================================================

/**
 * @brief Register a capability holder
 *
 * @param caps Capability system
 * @param name Holder name
 * @param holder_id Output: assigned holder ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_register_holder(
    nimcp_capability_system_t* caps,
    const char* name,
    uint32_t* holder_id
);

/**
 * @brief Assign capability to holder
 *
 * @param caps Capability system
 * @param holder_id Holder ID
 * @param capability Capability to assign
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_assign(
    nimcp_capability_system_t* caps,
    uint32_t holder_id,
    nimcp_capability_t capability
);

/**
 * @brief Remove holder
 *
 * @param caps Capability system
 * @param holder_id Holder ID
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_remove_holder(
    nimcp_capability_system_t* caps,
    uint32_t holder_id
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get capability system statistics
 *
 * @param caps Capability system
 * @param stats Output: statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_get_stats(
    nimcp_capability_system_t* caps,
    nimcp_cap_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param caps Capability system
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_capability_reset_stats(nimcp_capability_system_t* caps);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if capability is the invalid capability
 *
 * @param capability Capability to check
 * @return true if invalid
 */
bool nimcp_capability_is_null(nimcp_capability_t capability);

/**
 * @brief Compare two capabilities
 *
 * @param a First capability
 * @param b Second capability
 * @return true if equal
 */
bool nimcp_capability_equals(nimcp_capability_t a, nimcp_capability_t b);

/**
 * @brief Get resource type name
 *
 * @param type Resource type
 * @return Type name string
 */
const char* nimcp_resource_type_name(nimcp_resource_type_t type);

/**
 * @brief Get permission name
 *
 * @param permission Permission flag
 * @return Permission name string
 */
const char* nimcp_permission_name(uint32_t permission);

/**
 * @brief Format permissions as string
 *
 * @param permissions Permission bitmask
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int nimcp_permissions_to_string(
    uint32_t permissions,
    char* buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CAPABILITY_H
