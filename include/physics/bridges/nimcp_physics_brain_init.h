//=============================================================================
// nimcp_physics_brain_init.h - Physics Layer Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_physics_brain_init.h
 * @brief Brain factory integration for Phase 1 Physics modules
 *
 * WHAT: Provides initialization functions that integrate physics layer modules
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  Physics modules need to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize to biologically plausible defaults
 *
 * HOW:  Provides nimcp_brain_factory_init_physics_subsystem() that:
 *       1. Creates physics layer coordinator
 *       2. Initializes HH, Thermodynamics, Ephaptic modules
 *       3. Creates inter-module bridges (bio-async, QMC, FFT)
 *       4. Registers with KG and BBB
 *       5. Returns physics layer handle for brain struct
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (!nimcp_brain_factory_init_physics_subsystem(brain)) {
 *     NIMCP_LOG_WARN("physics", "Physics subsystem initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_BRAIN_INIT_H
#define NIMCP_PHYSICS_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_INIT_MODULE_NAME    "physics_brain_init"

/** Physics subsystem version */
#define PHYSICS_INIT_VERSION_MAJOR  1
#define PHYSICS_INIT_VERSION_MINOR  0
#define PHYSICS_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Physics initialization configuration
 */
typedef struct {
    /** Number of HH neurons in default population */
    uint32_t default_hh_population_size;

    /** Default temperature (Celsius) */
    float default_temperature;

    /** Default ATP pool size */
    float default_atp_pool;

    /** Enable ephaptic coupling */
    bool enable_ephaptic;

    /** Enable QMC bridges */
    bool enable_qmc;

    /** Enable FFT for LFP analysis */
    bool enable_fft;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable BBB registration */
    bool enable_security;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune integration */
    bool enable_immune_bridge;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;
} physics_init_config_t;

/**
 * @brief Physics initialization result
 */
typedef struct {
    /** HH population created */
    bool hh_initialized;

    /** Thermodynamics system created */
    bool thermo_initialized;

    /** Ephaptic system created */
    bool ephaptic_initialized;

    /** Bio-async bridges connected */
    bool bio_async_connected;

    /** QMC bridges created */
    bool qmc_created;

    /** FFT bridge created */
    bool fft_created;

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
} physics_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default physics initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_init_default_config(physics_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Initialize physics subsystem as part of brain creation
 *
 * WHAT: Main entry point for physics layer initialization
 * WHY:  Called by nimcp_brain_factory during brain creation
 * HOW:  Creates and connects all physics modules and bridges
 *
 * @param brain Brain instance being created
 * @return true on success, false on failure
 *
 * @note This uses default configuration. Use physics_init_modules()
 *       for custom configuration.
 */
NIMCP_EXPORT bool nimcp_brain_factory_init_physics_subsystem(brain_t brain);

/**
 * @brief Initialize physics modules with custom configuration
 *
 * WHAT: Full physics layer initialization with configuration
 * WHY:  Allows customization of physics parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_init_modules(
    brain_t brain,
    const physics_init_config_t* config,
    physics_init_result_t* result
);

/**
 * @brief Destroy physics subsystem (cleanup)
 *
 * WHAT: Cleans up physics layer during brain destruction
 * WHY:  Release physics resources when brain is destroyed
 * HOW:  Disconnects bridges, destroys modules, frees memory
 *
 * @param brain Brain instance being destroyed
 */
NIMCP_EXPORT void nimcp_brain_factory_destroy_physics_subsystem(brain_t brain);

//=============================================================================
// Individual Module Initialization
//=============================================================================

/**
 * @brief Initialize Hodgkin-Huxley population
 *
 * @param brain Brain instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_hodgkin_huxley(
    brain_t brain,
    const physics_init_config_t* config
);

/**
 * @brief Initialize Thermodynamics system
 *
 * @param brain Brain instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_thermodynamics(
    brain_t brain,
    const physics_init_config_t* config
);

/**
 * @brief Initialize Ephaptic coupling system
 *
 * @param brain Brain instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_ephaptic(
    brain_t brain,
    const physics_init_config_t* config
);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize bio-async bridges for physics modules
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_bio_async_bridges(brain_t brain);

/**
 * @brief Initialize QMC bridges for physics modules
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_qmc_bridges(brain_t brain);

/**
 * @brief Initialize FFT bridge for ephaptic LFP analysis
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_fft_bridge(brain_t brain);

/**
 * @brief Initialize KG wiring for physics layer
 *
 * @param brain Brain instance
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_kg_wiring(brain_t brain, uint64_t admin_token);

/**
 * @brief Initialize security registration for physics modules
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_security(brain_t brain);

/**
 * @brief Initialize immune bridge for physics layer
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool physics_init_immune_bridge(brain_t brain);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if physics subsystem is initialized
 *
 * @param brain Brain instance
 * @return true if physics is initialized
 */
NIMCP_EXPORT bool physics_is_initialized(brain_t brain);

/**
 * @brief Get physics subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* physics_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_BRAIN_INIT_H */
