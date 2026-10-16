//=============================================================================
// nimcp_rsc_brain_init.h - RSC Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_rsc_brain_init.h
 * @brief Brain factory integration for Retrosplenial Cortex (RSC) module
 *
 * WHAT: Provides initialization functions that integrate RSC module
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  RSC needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize to biologically plausible defaults
 *
 * HOW:  Provides rsc_brain_init_register() that:
 *       1. Registers RSC factory function with brain factory
 *       2. Provides rsc_brain_init_create() for RSC instantiation
 *       3. Provides rsc_brain_init_destroy() for cleanup
 *       4. Integrates with KG and BBB
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (!nimcp_brain_factory_init_rsc_subsystem(brain)) {
 *     NIMCP_LOG_WARN("rsc", "RSC subsystem initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RSC_BRAIN_INIT_H
#define NIMCP_RSC_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;
struct nimcp_retrosplenial;
typedef struct nimcp_retrosplenial nimcp_retrosplenial_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define RSC_INIT_MODULE_NAME    "rsc_brain_init"

/** RSC subsystem version */
#define RSC_INIT_VERSION_MAJOR  1
#define RSC_INIT_VERSION_MINOR  0
#define RSC_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief RSC initialization configuration
 */
typedef struct {
    /** Number of reference frame transform neurons */
    uint32_t num_transform_neurons;

    /** Number of context encoding neurons */
    uint32_t num_context_neurons;

    /** Number of scene recognition neurons */
    uint32_t num_scene_neurons;

    /** Number of head direction neurons */
    uint32_t num_hd_neurons;

    /** Enable security registration */
    bool enable_security;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune integration */
    bool enable_immune_bridge;

    /** Enable hippocampus bridge */
    bool enable_hippocampus;

    /** Enable entorhinal bridge */
    bool enable_entorhinal;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;
} rsc_init_config_t;

/**
 * @brief RSC initialization result
 */
typedef struct {
    /** RSC system created successfully */
    bool rsc_initialized;

    /** Security registered */
    bool security_registered;

    /** KG nodes registered */
    bool kg_registered;

    /** Bio-async bridges connected */
    bool bio_async_connected;

    /** Immune bridge connected */
    bool immune_connected;

    /** Hippocampus bridge connected */
    bool hippocampus_connected;

    /** Entorhinal bridge connected */
    bool entorhinal_connected;

    /** Number of warnings during init */
    uint32_t warning_count;

    /** Number of errors during init */
    uint32_t error_count;
} rsc_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default RSC initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_init_default_config(rsc_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register RSC factory functions with brain factory
 *
 * WHAT: Registers RSC creation/destruction with brain factory system
 * WHY:  Allows brain factory to manage RSC lifecycle
 * HOW:  Registers callbacks for RSC subsystem initialization
 *
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_brain_init_register(void);

/**
 * @brief Create RSC subsystem for brain
 *
 * WHAT: Main entry point for RSC initialization
 * WHY:  Called by brain factory during brain creation
 * HOW:  Creates RSC with default or provided config
 *
 * @param brain Brain instance being created
 * @param config Initialization configuration (NULL for defaults)
 * @param result Output initialization result (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_brain_init_create(
    brain_t brain,
    const rsc_init_config_t* config,
    rsc_init_result_t* result
);

/**
 * @brief Destroy RSC subsystem
 *
 * WHAT: Cleans up RSC during brain destruction
 * WHY:  Release RSC resources when brain is destroyed
 * HOW:  Disconnects bridges, unregisters from KG/BBB, frees memory
 *
 * @param brain Brain instance being destroyed
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rsc_brain_init_destroy(brain_t brain);

/**
 * @brief Initialize RSC subsystem (convenience wrapper)
 *
 * WHAT: Simple initialization with defaults
 * WHY:  Provides nimcp_brain_factory_init_*_subsystem pattern
 * HOW:  Calls rsc_brain_init_create with default config
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool nimcp_brain_factory_init_rsc_subsystem(brain_t brain);

/**
 * @brief Destroy RSC subsystem (convenience wrapper)
 *
 * WHAT: Simple cleanup wrapper
 * WHY:  Provides nimcp_brain_factory_destroy_*_subsystem pattern
 * HOW:  Calls rsc_brain_init_destroy
 *
 * @param brain Brain instance being destroyed
 */
NIMCP_EXPORT void nimcp_brain_factory_destroy_rsc_subsystem(brain_t brain);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize RSC security registration
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_security(brain_t brain, nimcp_retrosplenial_t* rsc);

/**
 * @brief Initialize RSC KG wiring
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_kg_wiring(
    brain_t brain,
    nimcp_retrosplenial_t* rsc,
    uint64_t admin_token
);

/**
 * @brief Initialize RSC bio-async bridges
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_bio_async_bridges(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
);

/**
 * @brief Initialize RSC immune bridge
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_immune_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
);

/**
 * @brief Initialize RSC hippocampus bridge
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_hippocampus_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
);

/**
 * @brief Initialize RSC entorhinal bridge
 *
 * @param brain Brain instance
 * @param rsc RSC instance
 * @return true on success
 */
NIMCP_EXPORT bool rsc_init_entorhinal_bridge(
    brain_t brain,
    nimcp_retrosplenial_t* rsc
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if RSC subsystem is initialized
 *
 * @param brain Brain instance
 * @return true if RSC is initialized
 */
NIMCP_EXPORT bool rsc_is_initialized(brain_t brain);

/**
 * @brief Get RSC subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* rsc_get_version(void);

/**
 * @brief Get RSC instance from brain
 *
 * @param brain Brain instance
 * @return RSC instance or NULL if not initialized
 */
NIMCP_EXPORT nimcp_retrosplenial_t* rsc_get_from_brain(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RSC_BRAIN_INIT_H */
