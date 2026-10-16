//=============================================================================
// nimcp_claustrum_brain_init.h - Claustrum Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_claustrum_brain_init.h
 * @brief Brain factory integration for Claustrum module
 *
 * WHAT: Provides initialization functions that integrate claustrum module
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  Claustrum needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize cross-modal binding subsystems
 *
 * HOW:  Provides claustrum_brain_init_register() that:
 *       1. Creates claustrum instance
 *       2. Initializes modalities and oscillators
 *       3. Registers with KG and BBB
 *       4. Returns claustrum handle for brain struct
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (!claustrum_brain_init_register(brain)) {
 *     NIMCP_LOG_WARN("claustrum", "Claustrum initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_CLAUSTRUM_BRAIN_INIT_H
#define NIMCP_CLAUSTRUM_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct nimcp_claustrum_s;
typedef struct nimcp_claustrum_s nimcp_claustrum_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define CLAUSTRUM_INIT_MODULE_NAME    "claustrum_brain_init"

/** Claustrum subsystem version */
#define CLAUSTRUM_INIT_VERSION_MAJOR  1
#define CLAUSTRUM_INIT_VERSION_MINOR  0
#define CLAUSTRUM_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Claustrum initialization configuration
 */
typedef struct {
    /** Enable cross-modal binding */
    bool enable_binding;

    /** Enable temporal synchronization */
    bool enable_synchronization;

    /** Enable global workspace gating */
    bool enable_workspace;

    /** Enable task switching */
    bool enable_task_switching;

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

    /** Binding threshold [0-1] */
    float binding_threshold;

    /** Gamma frequency (Hz) */
    float gamma_frequency;

    /** Alpha frequency (Hz) */
    float alpha_frequency;
} claustrum_init_config_t;

/**
 * @brief Claustrum initialization result
 */
typedef struct {
    /** Claustrum instance created */
    bool claustrum_initialized;

    /** Modalities initialized */
    bool modalities_initialized;

    /** Oscillators initialized */
    bool oscillators_initialized;

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
} claustrum_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default claustrum initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_init_default_config(claustrum_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register claustrum subsystem with brain factory
 *
 * WHAT: Main entry point for claustrum initialization
 * WHY:  Called by nimcp_brain_factory during brain creation
 * HOW:  Creates and connects claustrum module and bridges
 *
 * @param brain Brain instance being created
 * @return 0 on success, -1 on failure
 *
 * @note This uses default configuration. Use claustrum_brain_init_create()
 *       for custom configuration.
 */
NIMCP_EXPORT int claustrum_brain_init_register(brain_t brain);

/**
 * @brief Create claustrum subsystem with custom configuration
 *
 * WHAT: Full claustrum initialization with configuration
 * WHY:  Allows customization of claustrum parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_brain_init_create(
    brain_t brain,
    const claustrum_init_config_t* config,
    claustrum_init_result_t* result
);

/**
 * @brief Destroy claustrum subsystem (cleanup)
 *
 * WHAT: Cleans up claustrum during brain destruction
 * WHY:  Release claustrum resources when brain is destroyed
 * HOW:  Disconnects bridges, destroys instance, frees memory
 *
 * @param brain Brain instance being destroyed
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int claustrum_brain_init_destroy(brain_t brain);

//=============================================================================
// Component Initialization
//=============================================================================

/**
 * @brief Initialize claustrum modalities
 *
 * @param claustrum Claustrum instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_modalities(
    nimcp_claustrum_t* claustrum,
    const claustrum_init_config_t* config
);

/**
 * @brief Initialize claustrum oscillators
 *
 * @param claustrum Claustrum instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_oscillators(
    nimcp_claustrum_t* claustrum,
    const claustrum_init_config_t* config
);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize bio-async bridges for claustrum
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_bio_async_bridges(brain_t brain);

/**
 * @brief Initialize KG wiring for claustrum
 *
 * @param brain Brain instance
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_kg_wiring(brain_t brain, uint64_t admin_token);

/**
 * @brief Initialize security registration for claustrum
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_security(brain_t brain);

/**
 * @brief Initialize immune bridge for claustrum
 *
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool claustrum_init_immune_bridge(brain_t brain);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if claustrum subsystem is initialized
 *
 * @param brain Brain instance
 * @return true if claustrum is initialized
 */
NIMCP_EXPORT bool claustrum_is_initialized(brain_t brain);

/**
 * @brief Get claustrum subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* claustrum_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CLAUSTRUM_BRAIN_INIT_H */
