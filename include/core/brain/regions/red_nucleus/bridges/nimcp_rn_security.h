//=============================================================================
// nimcp_rn_security.h - Red Nucleus Security Registration
//=============================================================================
/**
 * @file nimcp_rn_security.h
 * @brief Blood-Brain Barrier (BBB) registration for Red Nucleus module
 *
 * WHAT: Registers Red Nucleus motor coordination module with the BBB security
 *       system for perimeter defense and access control.
 *
 * WHY:  Red Nucleus handles critical motor coordination and needs:
 *       - Access control for motor command modification
 *       - Memory boundary protection for motor state data
 *       - Audit logging for security-relevant operations
 *       - Token validation for KG operations
 *
 * HOW:  - Registers Red Nucleus as a BBB subject
 *       - Defines access requirements for motor operations
 *       - Provides capability checking for sensitive operations
 *       - Validates admin tokens for KG write access
 *
 * MODULE ID: 0x5004 (Red Nucleus)
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RN_SECURITY_H
#define NIMCP_RN_SECURITY_H

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
#define RN_SECURITY_MODULE_NAME    "rn_security"

/** Red Nucleus module ID */
#define RN_MODULE_ID               0x5004

/** Red Nucleus privilege level (moderate - can modify motor state) */
#define RN_PRIVILEGE_LEVEL         2

/** Red Nucleus roles */
#define RN_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define RN_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define RN_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define RN_ROLE_MOTOR_CMD          (1U << 3)   /**< Can issue motor commands */
#define RN_ROLE_LEARNING           (1U << 4)   /**< Can modify learning state */
#define RN_ROLE_CEREBELLAR         (1U << 5)   /**< Can access cerebellar loop */

/** Default Red Nucleus roles */
#define RN_DEFAULT_ROLES           (RN_ROLE_COMPUTE | \
                                    RN_ROLE_STATE_READ | \
                                    RN_ROLE_STATE_WRITE | \
                                    RN_ROLE_MOTOR_CMD)

/** Red Nucleus capabilities */
#define RN_CAP_MOTOR_MODIFY        (1ULL << 0)   /**< Modify motor commands */
#define RN_CAP_LEARNING_MODIFY     (1ULL << 1)   /**< Modify learning state */
#define RN_CAP_CEREBELLAR_ACCESS   (1ULL << 2)   /**< Access cerebellar loop */
#define RN_CAP_TRAJECTORY_MODIFY   (1ULL << 3)   /**< Modify trajectories */
#define RN_CAP_EFFECTOR_MODIFY     (1ULL << 4)   /**< Modify effector outputs */
#define RN_CAP_KG_WRITE            (1ULL << 5)   /**< Write to knowledge graph */

/** Default Red Nucleus capabilities */
#define RN_DEFAULT_CAPS            (RN_CAP_MOTOR_MODIFY | \
                                    RN_CAP_LEARNING_MODIFY | \
                                    RN_CAP_EFFECTOR_MODIFY)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Red Nucleus security operation types
 */
typedef enum {
    RN_SEC_OP_NONE = 0,

    /** Read operations */
    RN_SEC_OP_READ_STATE,           /**< Read module state */
    RN_SEC_OP_READ_MOTOR,           /**< Read motor commands */
    RN_SEC_OP_READ_LEARNING,        /**< Read learning state */

    /** Write operations */
    RN_SEC_OP_WRITE_STATE,          /**< Modify state */
    RN_SEC_OP_WRITE_MOTOR,          /**< Modify motor commands */
    RN_SEC_OP_WRITE_LEARNING,       /**< Modify learning parameters */
    RN_SEC_OP_RESET,                /**< Reset module */

    /** Motor operations */
    RN_SEC_OP_ISSUE_CMD,            /**< Issue motor command */
    RN_SEC_OP_ABORT_CMD,            /**< Abort motor command */
    RN_SEC_OP_TRAJECTORY,           /**< Execute trajectory */

    /** KG operations */
    RN_SEC_OP_KG_READ,              /**< Read from KG */
    RN_SEC_OP_KG_WRITE,             /**< Write to KG */

    RN_SEC_OP_COUNT
} rn_security_op_t;

/**
 * @brief Security configuration for Red Nucleus
 */
typedef struct {
    /** Enable security checks */
    bool enabled;

    /** Require token validation for KG writes */
    bool require_kg_token;

    /** Privilege level override */
    uint32_t privilege_level;

    /** Role mask override */
    uint32_t roles;

    /** Capability mask override */
    uint64_t capabilities;
} rn_security_config_t;

/**
 * @brief Security registration state
 */
typedef struct {
    /** BBB subject for Red Nucleus */
    bbb_subject_t rn_subject;

    /** BBB object for Red Nucleus memory region */
    bbb_object_t rn_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;

    /** Admin token for KG operations */
    uint64_t admin_token;
} rn_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_security_default_config(rn_security_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register Red Nucleus module with BBB
 *
 * WHAT: Registers RN as BBB subject
 * WHY:  Enable access control and audit logging
 * HOW:  Creates subject with appropriate privileges and roles
 *
 * @param bbb BBB system handle
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_security_register(
    bbb_system_t bbb,
    rn_security_state_t* state
);

/**
 * @brief Unregister Red Nucleus from BBB
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_security_unregister(
    bbb_system_t bbb,
    rn_security_state_t* state
);

/**
 * @brief Register Red Nucleus memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    rn_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if Red Nucleus operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool rn_security_check_access(
    bbb_system_t bbb,
    rn_security_op_t op
);

/**
 * @brief Check if module has capability
 *
 * @param state Security state
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool rn_security_has_capability(
    const rn_security_state_t* state,
    uint64_t capability
);

/**
 * @brief Validate token for KG operations
 *
 * WHAT: Validates admin token for KG write operations
 * WHY:  Prevent unauthorized KG modifications
 * HOW:  Compares token against stored admin token
 *
 * @param state Security state with admin token
 * @param token Token to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool rn_security_validate_kg_token(
    const rn_security_state_t* state,
    uint64_t token
);

/**
 * @brief Set admin token for KG operations
 *
 * @param state Security state to update
 * @param token Admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_security_set_admin_token(
    rn_security_state_t* state,
    uint64_t token
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
static inline bool rn_security_is_registered(
    const rn_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get module ID
 *
 * @return Red Nucleus module ID (0x5004)
 */
static inline uint32_t rn_security_get_module_id(void) {
    return RN_MODULE_ID;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RN_SECURITY_H */
