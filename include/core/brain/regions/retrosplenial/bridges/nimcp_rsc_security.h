//=============================================================================
// nimcp_rsc_security.h - Retrosplenial Cortex Security Registration
//=============================================================================
/**
 * @file nimcp_rsc_security.h
 * @brief Blood-Brain Barrier (BBB) registration for RSC module
 *
 * WHAT: Registers RSC module with the BBB security system for perimeter
 *       defense and access control.
 *
 * WHY:  RSC handles critical spatial-contextual processing and needs:
 *       - Access control for spatial memory operations
 *       - Memory boundary protection for navigation state
 *       - Audit logging for KG write operations
 *       - Integration with immune system threat detection
 *
 * HOW:  - Registers RSC as a BBB subject with module ID 0x5001
 *       - Defines access requirements for RSC operations
 *       - Provides capability checking for KG operations
 *       - Validates access tokens for sensitive operations
 *
 * MODULE ID: RSC_MODULE_ID = 0x5001
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RSC_SECURITY_H
#define NIMCP_RSC_SECURITY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "security/nimcp_blood_brain_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define RSC_SECURITY_MODULE_NAME    "rsc_security"

/** RSC module ID (in brain regions range 0x5000+) */
#define RSC_MODULE_ID               0x5001

/** RSC module privilege level (moderate - can modify spatial state) */
#define RSC_PRIVILEGE_LEVEL         2

/** RSC module roles */
#define RSC_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define RSC_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define RSC_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define RSC_ROLE_PARAM_READ         (1U << 3)   /**< Can read parameters */
#define RSC_ROLE_PARAM_WRITE        (1U << 4)   /**< Can modify parameters */
#define RSC_ROLE_KG_READ            (1U << 5)   /**< Can read from KG */
#define RSC_ROLE_KG_WRITE           (1U << 6)   /**< Can write to KG */

/** Default RSC module roles */
#define RSC_DEFAULT_ROLES           (RSC_ROLE_COMPUTE | \
                                     RSC_ROLE_STATE_READ | \
                                     RSC_ROLE_STATE_WRITE | \
                                     RSC_ROLE_PARAM_READ | \
                                     RSC_ROLE_KG_READ)

/** RSC module capabilities */
#define RSC_CAP_TRANSFORM_MODIFY    (1ULL << 0)   /**< Modify reference frames */
#define RSC_CAP_CONTEXT_MODIFY      (1ULL << 1)   /**< Modify context encoding */
#define RSC_CAP_NAVIGATION_MODIFY   (1ULL << 2)   /**< Modify navigation state */
#define RSC_CAP_LANDMARK_MODIFY     (1ULL << 3)   /**< Modify landmarks */
#define RSC_CAP_IMAGINATION_MODIFY  (1ULL << 4)   /**< Modify imagination state */
#define RSC_CAP_KG_WRITE            (1ULL << 5)   /**< Write to knowledge graph */

/** Default RSC capabilities */
#define RSC_DEFAULT_CAPS            (RSC_CAP_TRANSFORM_MODIFY | \
                                     RSC_CAP_CONTEXT_MODIFY | \
                                     RSC_CAP_NAVIGATION_MODIFY | \
                                     RSC_CAP_LANDMARK_MODIFY | \
                                     RSC_CAP_IMAGINATION_MODIFY)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief RSC security operation types
 */
typedef enum {
    RSC_SEC_OP_NONE = 0,

    /** Read operations */
    RSC_SEC_OP_READ_STATE,          /**< Read module state */
    RSC_SEC_OP_READ_PARAMS,         /**< Read parameters */
    RSC_SEC_OP_READ_NAVIGATION,     /**< Read navigation state */
    RSC_SEC_OP_READ_CONTEXT,        /**< Read context encoding */

    /** Write operations */
    RSC_SEC_OP_WRITE_STATE,         /**< Modify state */
    RSC_SEC_OP_WRITE_PARAMS,        /**< Modify parameters */
    RSC_SEC_OP_WRITE_NAVIGATION,    /**< Modify navigation */
    RSC_SEC_OP_WRITE_CONTEXT,       /**< Modify context */
    RSC_SEC_OP_RESET,               /**< Reset module */

    /** KG operations */
    RSC_SEC_OP_KG_READ,             /**< Read from KG */
    RSC_SEC_OP_KG_REGISTER,         /**< Register with KG */
    RSC_SEC_OP_KG_UPDATE,           /**< Update KG metadata */

    RSC_SEC_OP_COUNT
} rsc_security_op_t;

/**
 * @brief RSC security configuration
 */
typedef struct {
    /** Module ID override (0 for default) */
    uint32_t module_id;

    /** Privilege level override (0 for default) */
    uint32_t privilege_level;

    /** Role bitmask override (0 for default) */
    uint32_t roles;

    /** Capability bitmask override (0 for default) */
    uint64_t capabilities;

    /** Enable KG write capability */
    bool enable_kg_write;

    /** Admin token for KG operations */
    uint64_t admin_token;
} rsc_security_config_t;

/**
 * @brief Security registration state
 */
typedef struct {
    /** BBB subject for RSC */
    bbb_subject_t rsc_subject;

    /** BBB object for RSC memory region */
    bbb_object_t rsc_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;

    /** Admin token for KG operations */
    uint64_t admin_token;
} rsc_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default RSC security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_default_config(rsc_security_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register RSC module with BBB
 *
 * WHAT: Registers RSC as BBB subject
 * WHY:  Enable access control and audit logging
 * HOW:  Creates subject with appropriate privileges and roles
 *
 * @param bbb BBB system handle
 * @param config Security configuration (NULL for defaults)
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_register(
    bbb_system_t bbb,
    const rsc_security_config_t* config,
    rsc_security_state_t* state
);

/**
 * @brief Unregister RSC module from BBB
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_unregister(
    bbb_system_t bbb,
    rsc_security_state_t* state
);

/**
 * @brief Register RSC memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    rsc_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if RSC operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool rsc_security_check_access(
    bbb_system_t bbb,
    rsc_security_op_t op
);

/**
 * @brief Validate access token for KG operations
 *
 * WHAT: Validates that provided token matches admin token
 * WHY:  KG write operations require elevated privileges
 * HOW:  Compares token against stored admin token
 *
 * @param state Security state with admin token
 * @param token Token to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool rsc_security_validate_kg_token(
    const rsc_security_state_t* state,
    uint64_t token
);

/**
 * @brief Check if RSC has capability
 *
 * @param bbb BBB system handle
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool rsc_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Grant capability to RSC module
 *
 * @param bbb BBB system handle
 * @param capability Capability to grant
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Revoke capability from RSC module
 *
 * @param bbb BBB system handle
 * @param capability Capability to revoke
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect RSC security to immune system
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_security_connect_immune(
    bbb_system_t bbb,
    void* immune,  /* brain_immune_system_t* */
    rsc_security_state_t* state
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get registration state
 *
 * @param state Security state
 * @return true if registered
 */
static inline bool rsc_security_is_registered(
    const rsc_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get RSC module ID
 *
 * @return Module ID (0x5001)
 */
static inline uint32_t rsc_security_get_module_id(void) {
    return RSC_MODULE_ID;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RSC_SECURITY_H */
