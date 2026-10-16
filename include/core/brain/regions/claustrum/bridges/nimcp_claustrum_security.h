//=============================================================================
// nimcp_claustrum_security.h - Claustrum Security Registration
//=============================================================================
/**
 * @file nimcp_claustrum_security.h
 * @brief Blood-Brain Barrier (BBB) registration for Claustrum module
 *
 * WHAT: Registers claustrum module with the BBB security system for
 *       perimeter defense and access control.
 *
 * WHY:  Claustrum handles consciousness integration and cross-modal binding:
 *       - Access control for modality state modification
 *       - Memory boundary protection for binding state data
 *       - Audit logging for workspace gating operations
 *       - Integration with immune system for anomaly detection
 *
 * HOW:  - Registers claustrum as a BBB subject
 *       - Defines access requirements for claustrum operations
 *       - Provides capability checking for sensitive operations
 *       - Validates access tokens for KG operations
 *
 * MODULE ID: BIO_MODULE_CLAUSTRUM = 0x5002
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_CLAUSTRUM_SECURITY_H
#define NIMCP_CLAUSTRUM_SECURITY_H

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
#define CLAUSTRUM_SECURITY_MODULE_NAME    "claustrum_security"

/** Claustrum module ID for BBB registration */
#define CLAUSTRUM_MODULE_ID               0x5002

/** Claustrum module privilege level (moderate - can modify binding state) */
#define CLAUSTRUM_PRIVILEGE_LEVEL         2

/** Claustrum module roles */
#define CLAUSTRUM_ROLE_COMPUTE            (1U << 0)   /**< Can perform binding */
#define CLAUSTRUM_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define CLAUSTRUM_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define CLAUSTRUM_ROLE_MODALITY_READ      (1U << 3)   /**< Can read modalities */
#define CLAUSTRUM_ROLE_MODALITY_WRITE     (1U << 4)   /**< Can modify modalities */
#define CLAUSTRUM_ROLE_WORKSPACE_ACCESS   (1U << 5)   /**< Can access workspace */
#define CLAUSTRUM_ROLE_BROADCAST          (1U << 6)   /**< Can broadcast */

/** Default claustrum module roles */
#define CLAUSTRUM_DEFAULT_ROLES           (CLAUSTRUM_ROLE_COMPUTE | \
                                           CLAUSTRUM_ROLE_STATE_READ | \
                                           CLAUSTRUM_ROLE_STATE_WRITE | \
                                           CLAUSTRUM_ROLE_MODALITY_READ | \
                                           CLAUSTRUM_ROLE_BROADCAST)

/** Claustrum module capabilities */
#define CLAUSTRUM_CAP_BINDING_MODIFY      (1ULL << 0)  /**< Modify binding state */
#define CLAUSTRUM_CAP_OSCILLATION_MODIFY  (1ULL << 1)  /**< Modify oscillations */
#define CLAUSTRUM_CAP_WORKSPACE_GATE      (1ULL << 2)  /**< Gate workspace access */
#define CLAUSTRUM_CAP_STATE_SWITCH        (1ULL << 3)  /**< Switch brain states */
#define CLAUSTRUM_CAP_ATTENTION_MODIFY    (1ULL << 4)  /**< Modify attention bias */
#define CLAUSTRUM_CAP_KG_WRITE            (1ULL << 5)  /**< Write to KG */

/** Default claustrum capabilities */
#define CLAUSTRUM_DEFAULT_CAPS            (CLAUSTRUM_CAP_BINDING_MODIFY | \
                                           CLAUSTRUM_CAP_OSCILLATION_MODIFY | \
                                           CLAUSTRUM_CAP_WORKSPACE_GATE | \
                                           CLAUSTRUM_CAP_STATE_SWITCH | \
                                           CLAUSTRUM_CAP_ATTENTION_MODIFY)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Claustrum security operation types
 */
typedef enum {
    CLAUSTRUM_SEC_OP_NONE = 0,

    /** Read operations */
    CLAUSTRUM_SEC_OP_READ_STATE,          /**< Read module state */
    CLAUSTRUM_SEC_OP_READ_MODALITY,       /**< Read modality state */
    CLAUSTRUM_SEC_OP_READ_BINDING,        /**< Read binding state */
    CLAUSTRUM_SEC_OP_READ_WORKSPACE,      /**< Read workspace state */

    /** Write operations */
    CLAUSTRUM_SEC_OP_WRITE_STATE,         /**< Modify state */
    CLAUSTRUM_SEC_OP_WRITE_MODALITY,      /**< Modify modality */
    CLAUSTRUM_SEC_OP_WRITE_BINDING,       /**< Create/destroy binding */
    CLAUSTRUM_SEC_OP_WRITE_WORKSPACE,     /**< Modify workspace */

    /** Compute operations */
    CLAUSTRUM_SEC_OP_UPDATE,              /**< Run update step */
    CLAUSTRUM_SEC_OP_SYNCHRONIZE,         /**< Synchronize modalities */
    CLAUSTRUM_SEC_OP_SWITCH_STATE,        /**< Switch brain state */

    /** Communication operations */
    CLAUSTRUM_SEC_OP_BROADCAST,           /**< Broadcast to workspace */
    CLAUSTRUM_SEC_OP_GATE_ACCESS,         /**< Gate workspace access */

    /** Administrative operations */
    CLAUSTRUM_SEC_OP_KG_REGISTER,         /**< Register with KG */
    CLAUSTRUM_SEC_OP_KG_UPDATE,           /**< Update KG metadata */

    CLAUSTRUM_SEC_OP_COUNT
} claustrum_security_op_t;

/**
 * @brief Claustrum security configuration
 */
typedef struct {
    /** Enable strict access validation */
    bool strict_mode;

    /** Enable KG write access */
    bool enable_kg_write;

    /** Enable workspace gating */
    bool enable_workspace_gate;

    /** Admin token for privileged operations */
    uint64_t admin_token;
} claustrum_security_config_t;

/**
 * @brief Security registration state
 */
typedef struct {
    /** BBB subject for claustrum */
    bbb_subject_t claustrum_subject;

    /** BBB object for claustrum memory region */
    bbb_object_t claustrum_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;

    /** Configuration */
    claustrum_security_config_t config;
} claustrum_security_state_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default security configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_security_default_config(
    claustrum_security_config_t* config
);

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register claustrum module with BBB
 *
 * WHAT: Registers claustrum as BBB subject
 * WHY:  Enable access control and audit logging
 * HOW:  Creates subject with appropriate privileges and roles
 *
 * @param bbb BBB system handle
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_security_register(
    bbb_system_t bbb,
    claustrum_security_state_t* state
);

/**
 * @brief Unregister claustrum module from BBB
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_security_unregister(
    bbb_system_t bbb,
    claustrum_security_state_t* state
);

/**
 * @brief Register claustrum memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    claustrum_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if claustrum operation is permitted
 *
 * @param bbb BBB system handle
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool claustrum_security_check_access(
    bbb_system_t bbb,
    claustrum_security_op_t op
);

/**
 * @brief Validate access token for KG operations
 *
 * WHAT: Validates token for KG write access
 * WHY:  Protect KG from unauthorized modifications
 * HOW:  Compare token against stored admin token
 *
 * @param state Security state
 * @param token Token to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool claustrum_security_validate_kg_token(
    const claustrum_security_state_t* state,
    uint64_t token
);

/**
 * @brief Check if module has capability
 *
 * @param bbb BBB system handle
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool claustrum_security_has_capability(
    bbb_system_t bbb,
    uint64_t capability
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect claustrum security to immune system
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_security_connect_immune(
    bbb_system_t bbb,
    void* immune,
    claustrum_security_state_t* state
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
static inline bool claustrum_security_is_registered(
    const claustrum_security_state_t* state
) {
    return state && state->registered;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUSTRUM_SECURITY_H */
