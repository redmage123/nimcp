//=============================================================================
// nimcp_rn_brain_init.h - Red Nucleus Brain Factory Integration
//=============================================================================
/**
 * @file nimcp_rn_brain_init.h
 * @brief Brain factory integration for Red Nucleus motor coordination module
 *
 * WHAT: Provides initialization functions that integrate Red Nucleus
 *       with the brain factory and brain lifecycle.
 *
 * WHY:  Red Nucleus needs to be initialized as part of brain creation:
 *       - Register with KG for semantic queries
 *       - Register with BBB for security
 *       - Connect to bio-async router for messaging
 *       - Initialize motor coordination subsystems
 *
 * HOW:  Provides rn_brain_init_register() that:
 *       1. Creates Red Nucleus instance
 *       2. Initializes motor command subsystem
 *       3. Creates cerebellar integration bridges
 *       4. Registers with KG and BBB
 *       5. Returns Red Nucleus handle for brain struct
 *
 * USAGE:
 * ```c
 * // In nimcp_brain_factory.c:
 * if (rn_brain_init_register(brain) < 0) {
 *     NIMCP_LOG_WARN("rn_brain_init", "Red Nucleus initialization failed");
 * }
 * ```
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_RN_BRAIN_INIT_H
#define NIMCP_RN_BRAIN_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

/* Forward declarations */
struct brain_struct;
typedef struct brain_struct* brain_t;

struct nimcp_red_nucleus;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define RN_INIT_MODULE_NAME     "rn_brain_init"

/** Red Nucleus subsystem version */
#define RN_INIT_VERSION_MAJOR   1
#define RN_INIT_VERSION_MINOR   0
#define RN_INIT_VERSION_PATCH   0

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Red Nucleus initialization configuration
 */
typedef struct {
    /** Enable motor command queue */
    bool enable_command_queue;

    /** Enable motor learning */
    bool enable_learning;

    /** Enable cerebellar integration */
    bool enable_cerebellar;

    /** Enable KG registration */
    bool enable_kg_wiring;

    /** Enable BBB registration */
    bool enable_security;

    /** Enable bio-async messaging */
    bool enable_bio_async;

    /** Enable immune monitoring */
    bool enable_immune;

    /** Enable quantum optimization */
    bool enable_quantum;

    /** Max commands in queue */
    uint32_t max_commands_queued;

    /** Base learning rate */
    float base_learning_rate;

    /** Admin token for KG/security write operations */
    uint64_t admin_token;
} rn_init_config_t;

/**
 * @brief Red Nucleus initialization result
 */
typedef struct {
    /** Red Nucleus instance created */
    bool rn_created;

    /** Command queue initialized */
    bool command_queue_initialized;

    /** Learning subsystem initialized */
    bool learning_initialized;

    /** Cerebellar integration connected */
    bool cerebellar_connected;

    /** Bio-async bridges connected */
    bool bio_async_connected;

    /** KG nodes registered */
    bool kg_registered;

    /** BBB subjects registered */
    bool security_registered;

    /** Immune system connected */
    bool immune_connected;

    /** Number of warnings during init */
    uint32_t warning_count;

    /** Number of errors during init */
    uint32_t error_count;
} rn_init_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default Red Nucleus initialization configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_default_config(rn_init_config_t* config);

//=============================================================================
// Brain Factory Integration API
//=============================================================================

/**
 * @brief Register Red Nucleus subsystem with brain factory
 *
 * WHAT: Main entry point for Red Nucleus initialization
 * WHY:  Called by nimcp_brain_factory during brain creation
 * HOW:  Creates and connects Red Nucleus subsystem
 *
 * @param brain Brain instance being created
 * @return 0 on success, -1 on failure
 *
 * @note This uses default configuration. Use rn_brain_init_create()
 *       for custom configuration.
 */
NIMCP_EXPORT int rn_brain_init_register(brain_t brain);

/**
 * @brief Create Red Nucleus subsystem with custom configuration
 *
 * WHAT: Full Red Nucleus initialization with configuration
 * WHY:  Allows customization of motor coordination parameters
 * HOW:  Uses provided config for all initialization steps
 *
 * @param brain Brain instance
 * @param config Initialization configuration
 * @param result Output initialization result (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_brain_init_create(
    brain_t brain,
    const rn_init_config_t* config,
    rn_init_result_t* result
);

/**
 * @brief Destroy Red Nucleus subsystem (cleanup)
 *
 * WHAT: Cleans up Red Nucleus during brain destruction
 * WHY:  Release Red Nucleus resources when brain is destroyed
 * HOW:  Disconnects bridges, destroys modules, frees memory
 *
 * @param brain Brain instance being destroyed
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_brain_init_destroy(brain_t brain);

//=============================================================================
// Subsystem Initialization
//=============================================================================

/**
 * @brief Initialize motor command queue
 *
 * @param rn Red Nucleus instance
 * @param max_commands Maximum queued commands
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_command_queue(
    struct nimcp_red_nucleus* rn,
    uint32_t max_commands
);

/**
 * @brief Initialize motor learning subsystem
 *
 * @param rn Red Nucleus instance
 * @param base_learning_rate Base learning rate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_learning(
    struct nimcp_red_nucleus* rn,
    float base_learning_rate
);

/**
 * @brief Initialize cerebellar integration
 *
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_cerebellar(struct nimcp_red_nucleus* rn);

/**
 * @brief Initialize KG wiring for Red Nucleus
 *
 * @param brain Brain instance
 * @param rn Red Nucleus instance
 * @param admin_token Admin token for KG write
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_kg_wiring(
    brain_t brain,
    struct nimcp_red_nucleus* rn,
    uint64_t admin_token
);

/**
 * @brief Initialize security registration for Red Nucleus
 *
 * @param brain Brain instance
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_security(
    brain_t brain,
    struct nimcp_red_nucleus* rn
);

/**
 * @brief Initialize bio-async bridges for Red Nucleus
 *
 * @param brain Brain instance
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_bio_async(
    brain_t brain,
    struct nimcp_red_nucleus* rn
);

/**
 * @brief Initialize immune system connection
 *
 * @param brain Brain instance
 * @param rn Red Nucleus instance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int rn_init_immune(
    brain_t brain,
    struct nimcp_red_nucleus* rn
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Check if Red Nucleus subsystem is initialized
 *
 * @param brain Brain instance
 * @return true if Red Nucleus is initialized
 */
NIMCP_EXPORT bool rn_is_initialized(brain_t brain);

/**
 * @brief Get Red Nucleus subsystem version string
 *
 * @return Version string (e.g., "1.0.0")
 */
NIMCP_EXPORT const char* rn_get_version(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RN_BRAIN_INIT_H */
