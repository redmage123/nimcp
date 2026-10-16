//=============================================================================
// nimcp_reticular_security.h - Reticular Formation Security Registration
//=============================================================================
/**
 * @file nimcp_reticular_security.h
 * @brief Blood-Brain Barrier (BBB) registration for Reticular Formation module
 *
 * WHAT: Registers reticular formation module with the BBB security system for
 *       perimeter defense and access control.
 *
 * WHY:  Reticular formation handles critical arousal and consciousness control:
 *       - Access control for arousal state modification
 *       - Memory boundary protection for nucleus state data
 *       - Audit logging for security-relevant operations
 *       - Integration with immune system threat detection
 *       - Validation of access tokens for KG operations
 *
 * HOW:  - Registers reticular formation as a BBB subject
 *       - Defines access requirements for reticular operations
 *       - Provides capability checking for sensitive operations
 *       - Connects to immune system for threat response
 *
 * MODULE ID: RETICULAR_MODULE_ID = 0x5005
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RETICULAR_SECURITY_H
#define NIMCP_RETICULAR_SECURITY_H

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
#define RETICULAR_SECURITY_MODULE_NAME    "reticular_security"

/** Reticular formation module ID (matching brain region definitions) */
#ifndef RETICULAR_MODULE_ID
#define RETICULAR_MODULE_ID               0x5100
#endif

/** Reticular module privilege level (high - controls arousal/consciousness) */
#define RETICULAR_PRIVILEGE_LEVEL         3

/** Reticular module roles */
#define RETICULAR_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define RETICULAR_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define RETICULAR_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define RETICULAR_ROLE_AROUSAL_READ       (1U << 3)   /**< Can read arousal state */
#define RETICULAR_ROLE_AROUSAL_WRITE      (1U << 4)   /**< Can modify arousal state */
#define RETICULAR_ROLE_MODULATOR_READ     (1U << 5)   /**< Can read modulators */
#define RETICULAR_ROLE_MODULATOR_WRITE    (1U << 6)   /**< Can modify modulators */
#define RETICULAR_ROLE_BROADCAST          (1U << 7)   /**< Can broadcast messages */

/** Default reticular module roles */
#define RETICULAR_DEFAULT_ROLES           (RETICULAR_ROLE_COMPUTE | \
                                           RETICULAR_ROLE_STATE_READ | \
                                           RETICULAR_ROLE_STATE_WRITE | \
                                           RETICULAR_ROLE_AROUSAL_READ | \
                                           RETICULAR_ROLE_MODULATOR_READ | \
                                           RETICULAR_ROLE_BROADCAST)

/** Reticular module capabilities */
#define RETICULAR_CAP_AROUSAL_MODIFY      (1ULL << 0)   /**< Modify arousal state */
#define RETICULAR_CAP_NUCLEUS_MODIFY      (1ULL << 1)   /**< Modify nucleus state */
#define RETICULAR_CAP_MODULATOR_MODIFY    (1ULL << 2)   /**< Modify neuromodulators */
#define RETICULAR_CAP_AUTONOMIC_MODIFY    (1ULL << 3)   /**< Modify autonomic state */
#define RETICULAR_CAP_REFLEX_MODIFY       (1ULL << 4)   /**< Modify reflex state */
#define RETICULAR_CAP_MOTOR_MODIFY        (1ULL << 5)   /**< Modify motor control */
#define RETICULAR_CAP_PAIN_MODIFY         (1ULL << 6)   /**< Modify pain modulation */
#define RETICULAR_CAP_KG_WRITE            (1ULL << 7)   /**< Write to knowledge graph */

/** Default reticular capabilities */
#define RETICULAR_DEFAULT_CAPS            (RETICULAR_CAP_AROUSAL_MODIFY | \
                                           RETICULAR_CAP_NUCLEUS_MODIFY | \
                                           RETICULAR_CAP_MODULATOR_MODIFY | \
                                           RETICULAR_CAP_AUTONOMIC_MODIFY | \
                                           RETICULAR_CAP_REFLEX_MODIFY | \
                                           RETICULAR_CAP_MOTOR_MODIFY | \
                                           RETICULAR_CAP_PAIN_MODIFY)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Reticular security operation types
 */
typedef enum {
    RETICULAR_SEC_OP_NONE = 0,

    /** Read operations */
    RETICULAR_SEC_OP_READ_STATE,          /**< Read module state */
    RETICULAR_SEC_OP_READ_AROUSAL,        /**< Read arousal state */
    RETICULAR_SEC_OP_READ_MODULATORS,     /**< Read neuromodulators */
    RETICULAR_SEC_OP_READ_AUTONOMIC,      /**< Read autonomic state */
    RETICULAR_SEC_OP_READ_STATS,          /**< Read statistics */

    /** Write operations */
    RETICULAR_SEC_OP_WRITE_STATE,         /**< Modify state */
    RETICULAR_SEC_OP_WRITE_AROUSAL,       /**< Modify arousal */
    RETICULAR_SEC_OP_WRITE_MODULATORS,    /**< Modify neuromodulators */
    RETICULAR_SEC_OP_WRITE_AUTONOMIC,     /**< Modify autonomic state */
    RETICULAR_SEC_OP_RESET,               /**< Reset module */

    /** Compute operations */
    RETICULAR_SEC_OP_UPDATE,              /**< Run update step */
    RETICULAR_SEC_OP_COMPUTE_AROUSAL,     /**< Compute arousal effects */
    RETICULAR_SEC_OP_TRIGGER_REFLEX,      /**< Trigger a reflex */

    /** Communication operations */
    RETICULAR_SEC_OP_BROADCAST,           /**< Broadcast message */
    RETICULAR_SEC_OP_SUBSCRIBE,           /**< Subscribe to messages */

    /** Administrative operations */
    RETICULAR_SEC_OP_KG_REGISTER,         /**< Register with KG */
    RETICULAR_SEC_OP_KG_UPDATE,           /**< Update KG metadata */

    RETICULAR_SEC_OP_COUNT
} reticular_security_op_t;

/**
 * @brief Security configuration for reticular formation
 */
typedef struct {
    /** Enable strict mode (fail on any security violation) */
    bool strict_mode;

    /** Enable audit logging */
    bool enable_audit_log;

    /** Require token validation for KG operations */
    bool require_kg_token;

    /** Admin token for privileged operations */
    uint64_t admin_token;

    /** Minimum privilege level for arousal modification */
    uint32_t arousal_min_privilege;

    /** Minimum privilege level for modulator modification */
    uint32_t modulator_min_privilege;
} reticular_security_config_t;

/**
 * @brief Security registration state
 */
typedef struct {
    /** BBB subject for reticular formation */
    bbb_subject_t reticular_subject;

    /** BBB object for reticular memory region */
    bbb_object_t reticular_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;

    /** Admin token stored for validation */
    uint64_t admin_token;
} reticular_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_default_config(
    reticular_security_config_t* config
);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register reticular formation module with BBB
 *
 * WHAT: Registers reticular formation as BBB subject
 * WHY:  Enable access control and audit logging
 * HOW:  Creates subject with appropriate privileges and roles
 *
 * @param bbb BBB system handle
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_register(
    bbb_system_t bbb,
    reticular_security_state_t* state
);

/**
 * @brief Unregister reticular formation from BBB
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_unregister(
    bbb_system_t bbb,
    reticular_security_state_t* state
);

/**
 * @brief Register reticular memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    reticular_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if reticular operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool reticular_security_check_access(
    bbb_system_t bbb,
    reticular_security_op_t op
);

/**
 * @brief Check if module has capability
 *
 * @param bbb BBB system handle
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool reticular_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Validate access token for KG operations
 *
 * WHAT: Validates that provided token matches stored admin token
 * WHY:  Ensures only authorized operations can modify KG nodes
 * HOW:  Compares provided token against registered admin token
 *
 * @param state Security state with stored admin token
 * @param token Token to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool reticular_security_validate_kg_token(
    const reticular_security_state_t* state,
    uint64_t token
);

/**
 * @brief Grant capability to reticular module
 *
 * @param bbb BBB system handle
 * @param capability Capability to grant
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_grant_capability(
    bbb_system_t bbb,
    uint64_t capability
);

/**
 * @brief Revoke capability from reticular module
 *
 * @param bbb BBB system handle
 * @param capability Capability to revoke
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_revoke_capability(
    bbb_system_t bbb,
    uint64_t capability
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect reticular security to immune system
 *
 * WHAT: Links reticular security events to immune response
 * WHY:  Arousal anomalies may indicate attacks
 * HOW:  Registers callback for security events
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    reticular_security_state_t* state
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
static inline bool reticular_security_is_registered(
    const reticular_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get security operation name
 *
 * @param op Security operation
 * @return Human-readable name
 */
NIMCP_EXPORT const char* reticular_security_op_name(reticular_security_op_t op);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETICULAR_SECURITY_H */
