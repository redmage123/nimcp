//=============================================================================
// nimcp_ofc_brain_init.h - OFC Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_ofc_brain_init.h
 * @brief Brain factory integration for Orbitofrontal Cortex (OFC) module
 *
 * WHAT: Provides initialization functions that integrate OFC module
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  OFC module needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize to biologically plausible defaults
 *
 * HOW:  Provides ofc_brain_init_register() that:
 *       1. Creates OFC instance with config
 *       2. Registers with KG
 *       3. Registers with BBB
 *       4. Connects to bio-async router
 *       5. Returns OFC handle for brain struct
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (!ofc_brain_init_register(brain)) {
 *     NIMCP_LOG_WARN("ofc", "OFC subsystem registration failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_OFC_BRAIN_INIT_H
#define NIMCP_OFC_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct nimcp_ofc;
typedef struct nimcp_ofc nimcp_ofc_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define OFC_INIT_MODULE_NAME    "ofc_brain_init"

/** OFC subsystem version */
#define OFC_INIT_VERSION_MAJOR  1
#define OFC_INIT_VERSION_MINOR  0
#define OFC_INIT_VERSION_PATCH  0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief OFC initialization configuration
 */
typedef struct {
    /** Learning rate for value updates */
    float learning_rate;

    /** Temporal discount rate (gamma) */
    float discount_rate;

    /** Risk sensitivity [-1=averse, 1=seeking] */
    float risk_sensitivity;

    /** Weight for social rewards */
    float social_weight;

    /** Decision threshold */
    float decision_threshold;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable BBB registration */
    bool enable_security;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune integration */
    bool enable_immune_bridge;

    /** Enable quantum optimization */
    bool enable_quantum;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;
} ofc_init_config_t;

/**
 * @brief OFC initialization result
 */
typedef struct {
    /** OFC instance created */
    bool ofc_created;

    /** Bio-async connected */
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
} ofc_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default OFC initialization configuration
 *
 * WHAT: Initializes configuration with sensible defaults
 * WHY:  Provides consistent starting point for OFC initialization
 * HOW:  Sets biologically plausible default values
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_init_default_config(ofc_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register OFC subsystem with brain factory
 *
 * WHAT: Main entry point for OFC initialization
 * WHY:  Called by nimcp_brain_factory during brain creation
 * HOW:  Creates OFC and connects all bridges
 *
 * @param brain Brain instance being created
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_brain_init_register(brain_t brain);

/**
 * @brief Create OFC subsystem with custom configuration
 *
 * WHAT: Full OFC initialization with configuration
 * WHY:  Allows customization of OFC parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return Pointer to created OFC instance, or NULL on error
 */
NIMCP_EXPORT nimcp_ofc_t* ofc_brain_init_create(
    brain_t brain,
    const ofc_init_config_t* config,
    ofc_init_result_t* result
);

/**
 * @brief Destroy OFC subsystem (cleanup)
 *
 * WHAT: Cleans up OFC during brain destruction
 * WHY:  Release OFC resources when brain is destroyed
 * HOW:  Disconnects bridges, unregisters from KG/BBB, frees memory
 *
 * @param ofc OFC instance to destroy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ofc_brain_init_destroy(nimcp_ofc_t* ofc);

//=============================================================================
// Bridge Initialization
//=============================================================================

/**
 * @brief Initialize bio-async bridge for OFC
 *
 * @param ofc OFC instance
 * @param brain Brain instance (for router access)
 * @return true on success
 */
NIMCP_EXPORT bool ofc_init_bio_async_bridge(nimcp_ofc_t* ofc, brain_t brain);

/**
 * @brief Initialize KG wiring for OFC
 *
 * @param ofc OFC instance
 * @param brain Brain instance (for KG access)
 * @param admin_token Admin token for KG write
 * @return true on success
 */
NIMCP_EXPORT bool ofc_init_kg_wiring(
    nimcp_ofc_t* ofc,
    brain_t brain,
    uint64_t admin_token
);

/**
 * @brief Initialize security registration for OFC
 *
 * @param ofc OFC instance
 * @param brain Brain instance (for BBB access)
 * @return true on success
 */
NIMCP_EXPORT bool ofc_init_security(nimcp_ofc_t* ofc, brain_t brain);

/**
 * @brief Initialize immune bridge for OFC
 *
 * @param ofc OFC instance
 * @param brain Brain instance (for immune system access)
 * @return true on success
 */
NIMCP_EXPORT bool ofc_init_immune_bridge(nimcp_ofc_t* ofc, brain_t brain);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if OFC subsystem is initialized
 *
 * @param ofc OFC instance
 * @return true if OFC is initialized
 */
NIMCP_EXPORT bool ofc_is_initialized(nimcp_ofc_t* ofc);

/**
 * @brief Get OFC subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* ofc_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OFC_BRAIN_INIT_H */
