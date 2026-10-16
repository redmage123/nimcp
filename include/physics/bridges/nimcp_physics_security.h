//=============================================================================
// nimcp_physics_security.h - Physics Layer Security Registration
//=============================================================================
/**
 * @file nimcp_physics_security.h
 * @brief Blood-Brain Barrier (BBB) registration for Phase 1 Physics modules
 *
 * WHAT: Registers physics layer modules with the BBB security system for
 *       perimeter defense and access control.
 *
 * WHY:  Physics modules handle critical biophysical computations and need:
 *       - Access control for parameter modification
 *       - Memory boundary protection for state data
 *       - Audit logging for security-relevant operations
 *       - Integration with immune system threat detection
 *
 * HOW:  - Registers each physics module as a BBB subject
 *       - Defines access requirements for physics operations
 *       - Provides capability checking for sensitive operations
 *       - Connects to immune system for threat response
 *
 * MODULE IDS:
 *   BIO_MODULE_PHYSICS_HODGKIN_HUXLEY = 0x4500
 *   BIO_MODULE_PHYSICS_THERMODYNAMICS = 0x4501
 *   BIO_MODULE_PHYSICS_EPHAPTIC       = 0x4502
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_SECURITY_H
#define NIMCP_PHYSICS_SECURITY_H

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
#define PHYSICS_SECURITY_MODULE_NAME    "physics_security"

/** Physics module IDs (matching bio-async definitions) */
#define PHYSICS_MODULE_ID_HH            0x4500
#define PHYSICS_MODULE_ID_THERMO        0x4501
#define PHYSICS_MODULE_ID_EPHAPTIC      0x4502

/** Physics module privilege level (moderate - can modify neural state) */
#define PHYSICS_PRIVILEGE_LEVEL         2

/** Physics module roles */
#define PHYSICS_ROLE_COMPUTE            (1U << 0)   /**< Can perform computations */
#define PHYSICS_ROLE_STATE_READ         (1U << 1)   /**< Can read state */
#define PHYSICS_ROLE_STATE_WRITE        (1U << 2)   /**< Can modify state */
#define PHYSICS_ROLE_PARAM_READ         (1U << 3)   /**< Can read parameters */
#define PHYSICS_ROLE_PARAM_WRITE        (1U << 4)   /**< Can modify parameters */
#define PHYSICS_ROLE_BROADCAST          (1U << 5)   /**< Can broadcast messages */

/** Default physics module roles */
#define PHYSICS_DEFAULT_ROLES           (PHYSICS_ROLE_COMPUTE | \
                                         PHYSICS_ROLE_STATE_READ | \
                                         PHYSICS_ROLE_STATE_WRITE | \
                                         PHYSICS_ROLE_PARAM_READ | \
                                         PHYSICS_ROLE_BROADCAST)

/** Physics module capabilities */
#define PHYSICS_CAP_MEMBRANE_MODIFY     (1ULL << 0)   /**< Modify membrane params */
#define PHYSICS_CAP_TEMPERATURE_MODIFY  (1ULL << 1)   /**< Modify temperature */
#define PHYSICS_CAP_FIELD_MODIFY        (1ULL << 2)   /**< Modify ephaptic fields */
#define PHYSICS_CAP_POPULATION_MODIFY   (1ULL << 3)   /**< Modify neuron populations */
#define PHYSICS_CAP_ENERGY_MODIFY       (1ULL << 4)   /**< Modify energy pools */
#define PHYSICS_CAP_KG_WRITE            (1ULL << 5)   /**< Write to knowledge graph */

/** Default physics capabilities */
#define PHYSICS_DEFAULT_CAPS            (PHYSICS_CAP_MEMBRANE_MODIFY | \
                                         PHYSICS_CAP_TEMPERATURE_MODIFY | \
                                         PHYSICS_CAP_FIELD_MODIFY | \
                                         PHYSICS_CAP_POPULATION_MODIFY | \
                                         PHYSICS_CAP_ENERGY_MODIFY)

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Physics security operation types
 */
typedef enum {
    PHYSICS_SEC_OP_NONE = 0,

    /** Read operations */
    PHYSICS_SEC_OP_READ_STATE,          /**< Read module state */
    PHYSICS_SEC_OP_READ_PARAMS,         /**< Read parameters */
    PHYSICS_SEC_OP_READ_STATS,          /**< Read statistics */

    /** Write operations */
    PHYSICS_SEC_OP_WRITE_STATE,         /**< Modify state */
    PHYSICS_SEC_OP_WRITE_PARAMS,        /**< Modify parameters */
    PHYSICS_SEC_OP_RESET,               /**< Reset module */

    /** Compute operations */
    PHYSICS_SEC_OP_UPDATE,              /**< Run update step */
    PHYSICS_SEC_OP_COMPUTE_LFP,         /**< Compute LFP */
    PHYSICS_SEC_OP_SYNC_PHASE,          /**< Synchronize phases */

    /** Communication operations */
    PHYSICS_SEC_OP_BROADCAST,           /**< Broadcast message */
    PHYSICS_SEC_OP_SUBSCRIBE,           /**< Subscribe to messages */

    /** Administrative operations */
    PHYSICS_SEC_OP_KG_REGISTER,         /**< Register with KG */
    PHYSICS_SEC_OP_KG_UPDATE,           /**< Update KG metadata */

    PHYSICS_SEC_OP_COUNT
} physics_security_op_t;

/**
 * @brief Security registration state
 */
typedef struct {
    /** BBB subject for Hodgkin-Huxley */
    bbb_subject_t hh_subject;

    /** BBB subject for Thermodynamics */
    bbb_subject_t thermo_subject;

    /** BBB subject for Ephaptic */
    bbb_subject_t ephaptic_subject;

    /** BBB object for physics memory region */
    bbb_object_t physics_memory;

    /** Memory region ID */
    uint32_t memory_region_id;

    /** Registration successful */
    bool registered;

    /** Connected to immune system */
    bool immune_connected;
} physics_security_state_t;

//=============================================================================
// Registration API
//=============================================================================

/**
 * @brief Register all physics modules with BBB
 *
 * WHAT: Registers HH, Thermo, Ephaptic as BBB subjects
 * WHY:  Enable access control and audit logging
 * HOW:  Creates subjects with appropriate privileges and roles
 *
 * @param bbb BBB system handle
 * @param state Output registration state (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_register_all(
    bbb_system_t bbb,
    physics_security_state_t* state
);

/**
 * @brief Register Hodgkin-Huxley module with BBB
 *
 * @param bbb BBB system handle
 * @param subject Output subject (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_register_hh(
    bbb_system_t bbb,
    bbb_subject_t* subject
);

/**
 * @brief Register Thermodynamics module with BBB
 *
 * @param bbb BBB system handle
 * @param subject Output subject (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_register_thermo(
    bbb_system_t bbb,
    bbb_subject_t* subject
);

/**
 * @brief Register Ephaptic module with BBB
 *
 * @param bbb BBB system handle
 * @param subject Output subject (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_register_ephaptic(
    bbb_system_t bbb,
    bbb_subject_t* subject
);

/**
 * @brief Register physics memory region for protection
 *
 * @param bbb BBB system handle
 * @param address Memory region start address
 * @param size Memory region size
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_register_memory(
    bbb_system_t bbb,
    void* address,
    size_t size,
    physics_security_state_t* state
);

//=============================================================================
// Access Control API
//=============================================================================

/**
 * @brief Check if physics operation is permitted
 *
 * @param bbb BBB system handle
 * @param module_id Physics module ID
 * @param op Operation to check
 * @return true if permitted, false otherwise
 */
NIMCP_EXPORT bool physics_security_check_access(
    bbb_system_t bbb,
    uint32_t module_id,
    physics_security_op_t op
);

/**
 * @brief Check if module has capability
 *
 * @param bbb BBB system handle
 * @param module_id Physics module ID
 * @param capability Capability to check
 * @return true if has capability, false otherwise
 */
NIMCP_EXPORT bool physics_security_has_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
);

/**
 * @brief Grant capability to physics module
 *
 * @param bbb BBB system handle
 * @param module_id Physics module ID
 * @param capability Capability to grant
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_grant_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
);

/**
 * @brief Revoke capability from physics module
 *
 * @param bbb BBB system handle
 * @param module_id Physics module ID
 * @param capability Capability to revoke
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_revoke_capability(
    bbb_system_t bbb,
    uint32_t module_id,
    uint64_t capability
);

//=============================================================================
// Immune Integration API
//=============================================================================

/**
 * @brief Connect physics security to immune system
 *
 * WHAT: Links physics security events to immune response
 * WHY:  Physics anomalies may indicate attacks
 * HOW:  Registers callback for security events
 *
 * @param bbb BBB system handle
 * @param immune Brain immune system handle
 * @param state Security state to update
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_connect_immune(
    bbb_system_t bbb,
    void* immune,  /* brain_immune_system_t* */
    physics_security_state_t* state
);

//=============================================================================
// Cleanup API
//=============================================================================

/**
 * @brief Unregister all physics modules from BBB
 *
 * @param bbb BBB system handle
 * @param state Security state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_security_unregister_all(
    bbb_system_t bbb,
    physics_security_state_t* state
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
static inline bool physics_security_is_registered(
    const physics_security_state_t* state
) {
    return state && state->registered;
}

/**
 * @brief Get module ID by name
 *
 * @param name Module name ("hh", "thermo", "ephaptic")
 * @return Module ID or 0 if not found
 */
NIMCP_EXPORT uint32_t physics_security_get_module_id(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_SECURITY_H */
