//=============================================================================
// nimcp_pag_brain_init.h - PAG Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_pag_brain_init.h
 * @brief Brain factory integration for PAG (Periaqueductal Gray) module
 *
 * WHAT: Provides initialization functions that integrate the PAG module
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  PAG needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries about survival behaviors
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize columns, defense states, pain modulation
 *
 * HOW:  Provides pag_brain_init_register() that:
 *       1. Creates PAG instance with default config
 *       2. Registers with KG for survival behavior queries
 *       3. Registers with BBB for security
 *       4. Connects to bio-async router
 *       5. Returns PAG handle for brain struct
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (pag_brain_init_register(brain) < 0) {
 *     NIMCP_LOG_WARN("pag_brain_init", "PAG initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PAG_BRAIN_INIT_H
#define NIMCP_PAG_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct nimcp_pag;
typedef struct nimcp_pag nimcp_pag_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PAG_INIT_MODULE_NAME    "pag_brain_init"

/** PAG subsystem version */
#define PAG_INIT_VERSION_MAJOR  1
#define PAG_INIT_VERSION_MINOR  0
#define PAG_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief PAG initialization configuration
 */
typedef struct {
    /** Default threat activation threshold */
    float default_threat_threshold;

    /** Default defense decay rate */
    float default_defense_decay;

    /** Default analgesia gain */
    float default_analgesia_gain;

    /** Default column competition strength */
    float default_column_competition;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable BBB registration */
    bool enable_security;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune integration */
    bool enable_immune_bridge;

    /** Enable hypothalamus link */
    bool enable_hypothalamus;

    /** Enable QMC optimization */
    bool enable_qmc;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;
} pag_init_config_t;

/**
 * @brief PAG initialization result
 */
typedef struct {
    /** PAG instance created */
    bool pag_created;

    /** Columns initialized */
    bool columns_initialized;

    /** Defense system initialized */
    bool defense_initialized;

    /** Pain modulation initialized */
    bool pain_initialized;

    /** Bio-async connected */
    bool bio_async_connected;

    /** KG nodes registered */
    bool kg_registered;

    /** BBB subject registered */
    bool security_registered;

    /** Immune bridge connected */
    bool immune_connected;

    /** Hypothalamus connected */
    bool hypothalamus_connected;

    /** Number of warnings during init */
    uint32_t warning_count;

    /** Number of errors during init */
    uint32_t error_count;
} pag_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default PAG initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_init_default_config(pag_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register PAG subsystem with brain factory
 *
 * WHAT: Main entry point for PAG initialization during brain creation
 * WHY:  Called by nimcp_brain_factory during brain creation
 * HOW:  Creates PAG, initializes subsystems, connects bridges
 *
 * @param brain Brain instance being created
 * @return 0 on success, -1 on failure
 *
 * @note This uses default configuration. Use pag_brain_init_create()
 *       for custom configuration.
 */
NIMCP_EXPORT int pag_brain_init_register(brain_t brain);

/**
 * @brief Create PAG subsystem with custom configuration
 *
 * WHAT: Full PAG initialization with configuration
 * WHY:  Allows customization of PAG parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return Pointer to created PAG instance, or NULL on failure
 */
NIMCP_EXPORT nimcp_pag_t* pag_brain_init_create(
    brain_t brain,
    const pag_init_config_t* config,
    pag_init_result_t* result
);

/**
 * @brief Destroy PAG subsystem (cleanup)
 *
 * WHAT: Cleans up PAG during brain destruction
 * WHY:  Release PAG resources when brain is destroyed
 * HOW:  Disconnects bridges, unregisters from KG/BBB, frees memory
 *
 * @param pag PAG instance to destroy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int pag_brain_init_destroy(nimcp_pag_t* pag);

//=============================================================================
// Individual Subsystem Initialization
//=============================================================================

/**
 * @brief Initialize PAG columns
 *
 * @param pag PAG instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_columns(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
);

/**
 * @brief Initialize defense system
 *
 * @param pag PAG instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_defense(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
);

/**
 * @brief Initialize pain modulation system
 *
 * @param pag PAG instance
 * @param config Configuration
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_pain_modulation(
    nimcp_pag_t* pag,
    const pag_init_config_t* config
);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize bio-async bridges for PAG
 *
 * @param pag PAG instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_bio_async_bridges(
    nimcp_pag_t* pag,
    brain_t brain
);

/**
 * @brief Initialize KG wiring for PAG
 *
 * @param pag PAG instance
 * @param brain Brain instance
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_kg_wiring(
    nimcp_pag_t* pag,
    brain_t brain,
    uint64_t admin_token
);

/**
 * @brief Initialize security registration for PAG
 *
 * @param pag PAG instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_security(
    nimcp_pag_t* pag,
    brain_t brain
);

/**
 * @brief Initialize immune bridge for PAG
 *
 * @param pag PAG instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_immune_bridge(
    nimcp_pag_t* pag,
    brain_t brain
);

/**
 * @brief Initialize hypothalamus connection
 *
 * @param pag PAG instance
 * @param brain Brain instance
 * @return true on success
 */
NIMCP_EXPORT bool pag_init_hypothalamus_link(
    nimcp_pag_t* pag,
    brain_t brain
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if PAG subsystem is initialized
 *
 * @param pag PAG instance
 * @return true if PAG is initialized
 */
NIMCP_EXPORT bool pag_is_initialized(nimcp_pag_t* pag);

/**
 * @brief Get PAG subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* pag_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PAG_BRAIN_INIT_H */
