/**
 * @file nimcp_brain_init_surface_geometry.h
 * @brief Brain Initialization for Surface Geometry Subsystem
 *
 * WHAT: Factory initialization for surface geometry optimization
 * WHY:  Integrates surface geometry into brain factory lifecycle
 * HOW:  Creates geometry context, bridges, and connections during brain init
 *
 * USAGE:
 *   // Called during brain factory initialization
 *   bool success = nimcp_brain_factory_init_surface_geometry_subsystem(brain);
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_BRAIN_INIT_SURFACE_GEOMETRY_H
#define NIMCP_BRAIN_INIT_SURFACE_GEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct brain_struct* brain_t;
typedef struct surface_geometry_ctx_struct surface_geometry_ctx_t;
typedef struct surface_geometry_bridge_struct surface_geometry_bridge_t;

//=============================================================================
// INITIALIZATION FLAGS
//=============================================================================

/**
 * @brief Surface geometry initialization options
 */
typedef enum surface_geometry_init_flags_enum {
    SURFACE_INIT_DEFAULT = 0,               /**< Default initialization */
    SURFACE_INIT_NO_BIO_ASYNC = (1 << 0),   /**< Skip bio-async connection */
    SURFACE_INIT_NO_QUANTUM = (1 << 1),     /**< Skip quantum integration */
    SURFACE_INIT_NO_IMMUNE = (1 << 2),      /**< Skip immune integration */
    SURFACE_INIT_NO_CACHE = (1 << 3),       /**< Skip spine cache creation */
    SURFACE_INIT_MINIMAL = 0xF              /**< Minimal initialization */
} surface_geometry_init_flags_t;

//=============================================================================
// INITIALIZATION FUNCTIONS
//=============================================================================

/**
 * @brief Initialize surface geometry subsystem for brain
 *
 * Performs full initialization:
 * 1. Creates surface_geometry_ctx_t
 * 2. Creates surface_geometry_bridge_t
 * 3. Connects bio-async bridge (if enabled)
 * 4. Connects quantum bridge (if quantum enabled)
 * 5. Connects immune bridge
 * 6. Registers with KG wiring diagram
 * 7. Initializes dendrite/axon integration hooks
 *
 * @param brain Brain handle
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_surface_geometry_subsystem(brain_t brain);

/**
 * @brief Initialize surface geometry with custom flags
 *
 * @param brain Brain handle
 * @param flags Initialization flags
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_surface_geometry_with_flags(
    brain_t brain,
    surface_geometry_init_flags_t flags
);

/**
 * @brief Shutdown surface geometry subsystem
 *
 * @param brain Brain handle
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_shutdown_surface_geometry_subsystem(brain_t brain);

//=============================================================================
// ACCESSOR FUNCTIONS
//=============================================================================

/**
 * @brief Get surface geometry context from brain
 *
 * @param brain Brain handle
 * @return Surface geometry context or NULL
 */
surface_geometry_ctx_t* nimcp_brain_get_surface_geometry_ctx(brain_t brain);

/**
 * @brief Get surface geometry bridge from brain
 *
 * @param brain Brain handle
 * @return Surface geometry bridge or NULL
 */
surface_geometry_bridge_t* nimcp_brain_get_surface_geometry_bridge(brain_t brain);

/**
 * @brief Check if surface geometry is initialized
 *
 * @param brain Brain handle
 * @return true if initialized
 */
bool nimcp_brain_has_surface_geometry(brain_t brain);

//=============================================================================
// INTEGRATION HOOKS
//=============================================================================

/**
 * @brief Register dendrite surface geometry callback
 *
 * Called when dendrite needs geometry computation.
 *
 * @param brain Brain handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
typedef void (*dendrite_geometry_callback_t)(
    void* dendrite,
    void* geometry_result,
    void* user_data
);

int nimcp_brain_register_dendrite_geometry_callback(
    brain_t brain,
    dendrite_geometry_callback_t callback,
    void* user_data
);

/**
 * @brief Register axon surface geometry callback
 *
 * Called when axon needs geometry computation.
 *
 * @param brain Brain handle
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return 0 on success, -1 on error
 */
typedef void (*axon_geometry_callback_t)(
    void* axon,
    void* geometry_result,
    void* user_data
);

int nimcp_brain_register_axon_geometry_callback(
    brain_t brain,
    axon_geometry_callback_t callback,
    void* user_data
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_SURFACE_GEOMETRY_H */
