//=============================================================================
// nimcp_ofc_security.h - OFC Security Registration
//=============================================================================
/**
 * @file nimcp_ofc_security.h
 * @brief Blood-Brain Barrier (BBB) registration for Orbitofrontal Cortex (OFC)
 *
 * WHAT: Registers OFC module with the BBB security system for
 *       perimeter defense and access control.
 *
 * WHY:  OFC handles value-based decisions and needs:
 *       - Access control for value computation modification
 *       - Memory boundary protection for decision state data
 *       - Audit logging for security-relevant operations
 *       - Integration with immune system threat detection
 *       - Token validation for KG operations
 *
 * HOW:  - Registers OFC module as a BBB subject
 *       - Defines access requirements for OFC operations
 *       - Provides capability checking for sensitive operations
 *       - Validates access tokens for KG read/write
 *
 * MODULE ID:
 *   OFC_MODULE_ID = 0x5000
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_OFC_SECURITY_H
#define NIMCP_OFC_SECURITY_H

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
#define OFC_SECURITY_MODULE_NAME    "ofc_security"

/** OFC module ID (0x5000 as specified) */
#define OFC_MODULE_ID               0x5000

/** OFC module privilege level (moderate - can modify decision state) */
#define OFC_PRIVILEGE_LEVEL         2

/** OFC module roles */
#define OFC_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define OFC_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define OFC_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define OFC_ROLE_PARAM_READ         (1U << 3)   /**< Can read parameters */
#define OFC_ROLE_PARAM_WRITE        (1U << 4)   /**< Can modify parameters */
#define OFC_ROLE_BROADCAST          (1U << 5)   /**< Can broadcast messages */
#define OFC_ROLE_KG_ACCESS          (1U << 6)   /**< Can access knowledge graph */

/** Default OFC module roles */
#define OFC_DEFAULT_ROLES           (OFC_ROLE_COMPUTE | \
                                     OFC_ROLE_STATE_READ | \
                                     OFC_ROLE_STATE_WRITE | \
                                     OFC_ROLE_PARAM_READ | \
                                     OFC_ROLE_BROADCAST | \
                                     OFC_ROLE_KG_ACCESS)

/** OFC module capabilities */
#define OFC_CAP_VALUE_MODIFY        (1ULL << 0)   /**< Modify value computations */
#define OFC_CAP_DECISION_MODIFY     (1ULL << 1)   /**< Modify decision parameters */
#define OFC_CAP_EMOTION_MODIFY      (1ULL << 2)   /**< Modify emotion modulation */
#define OFC_CAP_REVERSAL_TRIGGER    (1ULL << 3)   /**< Trigger reversal learning */
#define OFC_CAP_RISK_MODIFY         (1ULL << 4)   /**< Modify risk assessment */
#define OFC_CAP_KG_READ             (1ULL << 5)   /**< Read from knowledge graph */
#define OFC_CAP_KG_WRITE            (1ULL << 6)   /**< Write to knowledge graph */

/** Default OFC capabilities */
#define OFC_DEFAULT_CAPS            (OFC_CAP_VALUE_MODIFY | \
                                     OFC_CAP_DECISION_MODIFY | \
                                     OFC_CAP_EMOTION_MODIFY | \
                                     OFC_CAP_REVERSAL_TRIGGER | \
                                     OFC_CAP_RISK_MODIFY | \
                                     OFC_CAP_KG_READ)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief OFC security operation types
 */
typedef enum {
    OFC_SEC_OP_NONE = 0,

    /** Read operations */
    OFC_SEC_OP_READ_STATE,          /**< Read module state */
    OFC_SEC_OP_READ_PARAMS,         /**< Read parameters */
    OFC_SEC_OP_READ_STATS,          /**< Read statistics */
    OFC_SEC_OP_READ_VALUE,          /**< Read value computation */

    /** Write operations */
    OFC_SEC_OP_WRITE_STATE,         /**< Modify state */
    OFC_SEC_OP_WRITE_PARAMS,        /**< Modify parameters */
    OFC_SEC_OP_RESET,               /**< Reset module */

    /** Compute operations */
    OFC_SEC_OP_COMPUTE_VALUE,       /**< Compute value */
    OFC_SEC_OP_MAKE_DECISION,       /**< Make decision */
    OFC_SEC_OP_ASSESS_RISK,         /**< Assess risk */

    /** Communication operations */
    OFC_SEC_OP_BROADCAST,           /**< Broadcast message */
    OFC_SEC_OP_SUBSCRIBE,           /**< Subscribe to messages */

    /** KG operations */
    OFC_SEC_OP_KG_READ,             /**< Read from KG */
    OFC_SEC_OP_KG_WRITE,            /**< Write to KG */
    OFC_SEC_OP_KG_QUERY,            /**< Query KG */

    OFC_SEC_OP_COUNT
} ofc_security_op_t;

/**
 * @brief OFC security configuration
 */
typedef struct {
    /** Enable BBB registration */
    bool enable_bbb;

    /** Enable immune integration */
    bool enable_immune;

    /** Enable KG token validation */
    bool enable_kg_validation;

    /** Admin token for privileged operations */
    uint64_t admin_token;

    /** Custom privilege level (0 = use default) */
    uint32_t privilege_level;

    /** Custom roles (0 = use default) */
    uint32_t roles;

    /** Custom capabilities (0 = use default) */
    uint64_t capabilities;
} ofc_security_config_t;

/**
 * @brief OFC security registration state
 */
typedef struct {
    /** BBB subject for OFC */
    bbb_subject_t ofc_subject;

    /** BBB object for OFC memory region */
    bbb_object_t ofc_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Validated admin token */
    uint64_t admin_token;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;

    /** KG validation enabled */
    bool kg_validation_enabled;
} ofc_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default OFC security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_security_default_config(ofc_security_config_t* config);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register OFC module with BBB
 *
 * WHAT: Registers OFC as a BBB subject with appropriate privileges
 * WHY:  Enable access control and audit logging for OFC operations
 * HOW:  Creates subject with OFC-specific privileges and roles
 *
 * @param bbb BBB system handle
 * @param config Security configuration (NULL for defaults)
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_security_register(
    bbb_system_t bbb,
    const ofc_security_config_t* config,
    ofc_security_state_t* state
);

/**
 * @brief Unregister OFC module from BBB
 *
 * WHAT: Removes OFC registration from BBB
 * WHY:  Clean shutdown and resource release
 * HOW:  Unregisters memory regions and clears state
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_security_unregister(
    bbb_system_t bbb,
    ofc_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if OFC operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool ofc_security_check_access(
    bbb_system_t bbb,
    ofc_security_op_t op
);

/**
 * @brief Check if OFC has capability
 *
 * @param state Security state
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool ofc_security_has_capability(
    const ofc_security_state_t* state,
    uint64_t capability
);

//=============================================================================
// Token Validation API
//=============================================================================

/**
 * @brief Validate access token for KG operations
 *
 * WHAT: Validates provided token against stored admin token
 * WHY:  Protect KG operations from unauthorized access
 * HOW:  Constant-time comparison to prevent timing attacks
 *
 * @param state Security state with admin token
 * @param token Token to validate
 * @return true if token is valid, false otherwise
 */
NIMCP_EXPORT bool ofc_security_validate_kg_token(
    const ofc_security_state_t* state,
    uint64_t token
);

/**
 * @brief Set admin token for KG operations
 *
 * @param state Security state to update
 * @param token New admin token
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_security_set_admin_token(
    ofc_security_state_t* state,
    uint64_t token
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect OFC security to immune system
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_security_connect_immune(
    bbb_system_t bbb,
    void* immune,  /* brain_immune_system_t* */
    ofc_security_state_t* state
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if OFC is registered with BBB
 *
 * @param state Security state
 * @return true if registered
 */
static inline bool ofc_security_is_registered(
    const ofc_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get OFC module ID
 *
 * @return OFC module ID (0x5000)
 */
static inline uint32_t ofc_security_get_module_id(void) {
    return OFC_MODULE_ID;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OFC_SECURITY_H */
