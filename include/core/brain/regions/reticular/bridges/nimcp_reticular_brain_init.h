//=============================================================================
// nimcp_reticular_brain_init.h - Reticular Formation Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_reticular_brain_init.h
 * @brief Brain factory integration for Reticular Formation module
 *
 * WHAT: Provides initialization functions that integrate reticular formation
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  Reticular formation needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize to biologically plausible defaults
 *
 * HOW:  Provides reticular_brain_init_register() that:
 *       1. Creates reticular formation subsystem
 *       2. Initializes all nuclei and modulators
 *       3. Creates integration bridges
 *       4. Registers with KG and BBB
 *       5. Returns reticular handle for brain struct
 *
 * USAGE:
 * ```c
 * // In brain factory:
 * if (reticular_brain_init_register(brain) < 0) {
 *     NIMCP_LOG_WARN("reticular", "Reticular subsystem initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RETICULAR_BRAIN_INIT_H
#define NIMCP_RETICULAR_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct nimcp_reticular;
typedef struct nimcp_reticular nimcp_reticular_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define RETICULAR_INIT_MODULE_NAME    "reticular_brain_init"

/** Reticular subsystem version */
#define RETICULAR_INIT_VERSION_MAJOR  1
#define RETICULAR_INIT_VERSION_MINOR  0
#define RETICULAR_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Reticular initialization configuration
 */
typedef struct {
    /** Initial arousal level (0-1) */
    float initial_arousal;

    /** Default temperature (Celsius) */
    float default_temperature;

    /** Enable all nuclei */
    bool enable_all_nuclei;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable BBB registration */
    bool enable_security;

    /** Enable immune integration */
    bool enable_immune_bridge;

    /** Enable quantum optimization */
    bool enable_quantum;

    /** Enable logging */
    bool enable_logging;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;

    /** Platform tier */
    uint32_t platform_tier;
} reticular_init_config_t;

/**
 * @brief Reticular initialization result
 */
typedef struct {
    /** Reticular formation created */
    bool reticular_initialized;

    /** Nuclei initialized */
    bool nuclei_initialized;

    /** Neuromodulators initialized */
    bool modulators_initialized;

    /** Autonomic system initialized */
    bool autonomic_initialized;

    /** Bio-async bridges connected */
    bool bio_async_connected;

    /** KG nodes registered */
    bool kg_registered;

    /** BBB subjects registered */
    bool security_registered;

    /** Immune bridge connected */
    bool immune_connected;

    /** Number of warnings during init */
    uint32_t warning_count;

    /** Number of errors during init */
    uint32_t error_count;
} reticular_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default reticular initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_init_default_config(reticular_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register reticular formation with brain factory
 *
 * WHAT: Main entry point for reticular formation initialization
 * WHY:  Called by brain factory during brain creation
 * HOW:  Creates and connects reticular formation subsystem
 *
 * @param brain Brain instance being created
 * @return 0 on success, -1 on failure
 *
 * @note This uses default configuration. Use reticular_brain_init_create()
 *       for custom configuration.
 */
NIMCP_EXPORT int reticular_brain_init_register(brain_t brain);

/**
 * @brief Create reticular formation with custom configuration
 *
 * WHAT: Full reticular formation creation with configuration
 * WHY:  Allows customization of reticular parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return Reticular formation instance, or NULL on error
 */
NIMCP_EXPORT nimcp_reticular_t* reticular_brain_init_create(
    brain_t brain,
    const reticular_init_config_t* config,
    reticular_init_result_t* result
);

/**
 * @brief Destroy reticular formation (cleanup)
 *
 * WHAT: Cleans up reticular formation during brain destruction
 * WHY:  Release reticular resources when brain is destroyed
 * HOW:  Disconnects bridges, destroys subsystems, frees memory
 *
 * @param reticular Reticular formation instance to destroy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int reticular_brain_init_destroy(nimcp_reticular_t* reticular);

//=============================================================================
// Component Initialization
//=============================================================================

/**
 * @brief Initialize nuclei subsystem
 *
 * @param reticular Reticular formation instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_nuclei(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
);

/**
 * @brief Initialize neuromodulators subsystem
 *
 * @param reticular Reticular formation instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_modulators(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
);

/**
 * @brief Initialize autonomic subsystem
 *
 * @param reticular Reticular formation instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_autonomic(
    nimcp_reticular_t* reticular,
    const reticular_init_config_t* config
);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize bio-async bridges for reticular formation
 *
 * @param reticular Reticular formation instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_bio_async_bridges(
    nimcp_reticular_t* reticular,
    brain_t brain
);

/**
 * @brief Initialize KG wiring for reticular formation
 *
 * @param reticular Reticular formation instance
 * @param brain Brain instance
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_kg_wiring(
    nimcp_reticular_t* reticular,
    brain_t brain,
    uint64_t admin_token
);

/**
 * @brief Initialize security registration for reticular formation
 *
 * @param reticular Reticular formation instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_security(
    nimcp_reticular_t* reticular,
    brain_t brain
);

/**
 * @brief Initialize immune bridge for reticular formation
 *
 * @param reticular Reticular formation instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool reticular_init_immune_bridge(
    nimcp_reticular_t* reticular,
    brain_t brain
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if reticular formation is initialized
 *
 * @param reticular Reticular formation instance
 * @return true if initialized
 */
NIMCP_EXPORT bool reticular_is_initialized(nimcp_reticular_t* reticular);

/**
 * @brief Get reticular subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* reticular_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETICULAR_BRAIN_INIT_H */
