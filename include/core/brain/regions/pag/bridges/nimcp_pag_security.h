//=============================================================================
// nimcp_pag_security.h - PAG Security Registration (BBB Integration)
//=============================================================================
/**
 * @file nimcp_pag_security.h
 * @brief Blood-Brain Barrier (BBB) registration for PAG module
 *
 * WHAT: Registers the PAG module with the BBB security system for
 *       perimeter defense and access control.
 *
 * WHY:  PAG handles critical survival behavior computations and needs:
 *       - Access control for defense state modification
 *       - Memory boundary protection for pain/threat data
 *       - Audit logging for security-relevant operations
 *       - Integration with immune system threat detection
 *
 * HOW:  - Registers PAG as a BBB subject
 *       - Defines access requirements for PAG operations
 *       - Provides capability checking for sensitive operations
 *       - Validates access tokens for KG operations
 *
 * MODULE ID: PAG_MODULE_ID = 0x5003
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PAG_SECURITY_H
#define NIMCP_PAG_SECURITY_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "security/nimcp_blood_brain_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PAG_SECURITY_MODULE_NAME    "pag_security"

/** PAG module ID (unique identifier for BBB registration) */
#define PAG_MODULE_ID               0x5003

/** PAG module privilege level (elevated - controls survival behaviors) */
#define PAG_PRIVILEGE_LEVEL         3

/** PAG module roles */
#define PAG_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define PAG_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define PAG_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define PAG_ROLE_PARAM_READ         (1U << 3)   /**< Can read parameters */
#define PAG_ROLE_PARAM_WRITE        (1U << 4)   /**< Can modify parameters */
#define PAG_ROLE_BROADCAST          (1U << 5)   /**< Can broadcast messages */
#define PAG_ROLE_DEFENSE_CONTROL    (1U << 6)   /**< Can control defense states */
#define PAG_ROLE_PAIN_MODULATE      (1U << 7)   /**< Can modulate pain pathways */

/** Default PAG module roles */
#define PAG_DEFAULT_ROLES           (PAG_ROLE_COMPUTE | \
                                     PAG_ROLE_STATE_READ | \
                                     PAG_ROLE_STATE_WRITE | \
                                     PAG_ROLE_PARAM_READ | \
                                     PAG_ROLE_BROADCAST | \
                                     PAG_ROLE_DEFENSE_CONTROL | \
                                     PAG_ROLE_PAIN_MODULATE)

/** PAG module capabilities */
#define PAG_CAP_DEFENSE_MODIFY      (1ULL << 0)   /**< Modify defense states */
#define PAG_CAP_PAIN_MODIFY         (1ULL << 1)   /**< Modify pain modulation */
#define PAG_CAP_AUTONOMIC_MODIFY    (1ULL << 2)   /**< Modify autonomic outputs */
#define PAG_CAP_COLUMN_MODIFY       (1ULL << 3)   /**< Modify column activity */
#define PAG_CAP_EMOTION_MODIFY      (1ULL << 4)   /**< Modify emotional state */
#define PAG_CAP_KG_WRITE            (1ULL << 5)   /**< Write to knowledge graph */
#define PAG_CAP_VOCAL_CONTROL       (1ULL << 6)   /**< Control vocalizations */

/** Default PAG capabilities */
#define PAG_DEFAULT_CAPS            (PAG_CAP_DEFENSE_MODIFY | \
                                     PAG_CAP_PAIN_MODIFY | \
                                     PAG_CAP_AUTONOMIC_MODIFY | \
                                     PAG_CAP_COLUMN_MODIFY | \
                                     PAG_CAP_EMOTION_MODIFY | \
                                     PAG_CAP_VOCAL_CONTROL)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief PAG security operation types
 */
typedef enum {
    PAG_SEC_OP_NONE = 0,

    /** Read operations */
    PAG_SEC_OP_READ_STATE,          /**< Read module state */
    PAG_SEC_OP_READ_PARAMS,         /**< Read parameters */
    PAG_SEC_OP_READ_DEFENSE,        /**< Read defense state */
    PAG_SEC_OP_READ_PAIN,           /**< Read pain state */

    /** Write operations */
    PAG_SEC_OP_WRITE_STATE,         /**< Modify state */
    PAG_SEC_OP_WRITE_PARAMS,        /**< Modify parameters */
    PAG_SEC_OP_WRITE_DEFENSE,       /**< Modify defense state */
    PAG_SEC_OP_WRITE_PAIN,          /**< Modify pain modulation */
    PAG_SEC_OP_RESET,               /**< Reset module */

    /** Compute operations */
    PAG_SEC_OP_UPDATE,              /**< Run update step */
    PAG_SEC_OP_PROCESS_THREAT,      /**< Process threat signal */
    PAG_SEC_OP_PROCESS_PAIN,        /**< Process pain input */

    /** Communication operations */
    PAG_SEC_OP_BROADCAST,           /**< Broadcast message */
    PAG_SEC_OP_SUBSCRIBE,           /**< Subscribe to messages */

    /** Administrative operations */
    PAG_SEC_OP_KG_REGISTER,         /**< Register with KG */
    PAG_SEC_OP_KG_UPDATE,           /**< Update KG metadata */

    PAG_SEC_OP_COUNT
} pag_security_op_t;

/**
 * @brief PAG security configuration
 */
typedef struct {
    /** BBB system handle */
    bbb_system_t bbb;

    /** Admin token for KG write operations */
    uint64_t admin_token;

    /** Enable strict access control */
    bool strict_mode;

    /** Log all access attempts */
    bool log_access;
} pag_security_config_t;

/**
 * @brief PAG security registration state
 */
typedef struct {
    /** BBB subject for PAG */
    bbb_subject_t pag_subject;

    /** BBB object for PAG memory region */
    bbb_object_t pag_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Admin token for KG operations */
    uint64_t admin_token;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;
} pag_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default PAG security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_default_config(pag_security_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register PAG module with BBB
 *
 * WHAT: Registers PAG as a BBB subject with appropriate privileges
 * WHY:  Enable access control and audit logging for survival behaviors
 * HOW:  Creates subject with PAG-specific privileges and roles
 *
 * @param bbb BBB system handle
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_register(
    bbb_system_t bbb,
    pag_security_state_t* state
);

/**
 * @brief Unregister PAG module from BBB
 *
 * WHAT: Removes PAG registration from BBB
 * WHY:  Clean up security state during shutdown
 * HOW:  Unregisters subject and memory regions
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_unregister(
    bbb_system_t bbb,
    pag_security_state_t* state
);

/**
 * @brief Register PAG memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    pag_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if PAG operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool pag_security_check_access(
    bbb_system_t bbb,
    pag_security_op_t op
);

/**
 * @brief Check if PAG has specific capability
 *
 * @param bbb BBB system handle
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool pag_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Validate access token for KG operations
 *
 * WHAT: Validates that a token has permission for KG writes
 * WHY:  Prevent unauthorized modifications to PAG KG nodes
 * HOW:  Compares token against stored admin token
 *
 * @param state Security state with stored admin token
 * @param token Token to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool pag_security_validate_kg_token(
    const pag_security_state_t* state,
    uint64_t token
);

/**
 * @brief Grant capability to PAG module
 *
 * @param bbb BBB system handle
 * @param capability Capability to grant
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Revoke capability from PAG module
 *
 * @param bbb BBB system handle
 * @param capability Capability to revoke
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect PAG security to immune system
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    pag_security_state_t* state
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
static inline bool pag_security_is_registered(
    const pag_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get PAG module ID
 *
 * @return PAG module ID (0x5003)
 */
static inline uint32_t pag_security_get_module_id(void) {
    return PAG_MODULE_ID;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAG_SECURITY_H */
