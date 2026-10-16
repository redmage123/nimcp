/**
 * @file nimcp_layer_coordinator.h
 * @brief Layer Coordinator - Central orchestrator for all layer operations
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Central coordinator managing all intra-layer and inter-layer operations
 * WHY:  Single point of control for layer lifecycle, updates, and synchronization
 * HOW:  Integrates registry, router, and layer-specific coordinators
 *
 * RESPONSIBILITIES:
 * =================
 * 1. Layer lifecycle management (init, update, shutdown)
 * 2. Global synchronization across layers
 * 3. Coherence tracking and reporting
 * 4. Integration with brain factory
 * 5. Bio-async and immune system integration
 *
 * USAGE:
 * ======
 *   // Create coordinator (typically done by brain factory)
 *   nimcp_layer_coordinator_t* coord = nimcp_layer_coordinator_create(brain);
 *
 *   // Register standard layers
 *   nimcp_layer_coordinator_register_standard_layers(coord);
 *
 *   // Initialize all layers
 *   nimcp_layer_coordinator_init_all(coord);
 *
 *   // Update (called each tick)
 *   nimcp_layer_coordinator_update(coord, dt);
 *
 *   // Shutdown
 *   nimcp_layer_coordinator_shutdown(coord);
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LAYER_COORDINATOR_H
#define NIMCP_LAYER_COORDINATOR_H

#include "nimcp_layer_types.h"
#include "nimcp_layer_registry.h"
#include "nimcp_inter_layer_router.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* Brain type from nimcp_brain.h */
typedef struct brain_struct* brain_t;

/* Bio-async router from nimcp_bio_router.h */
typedef struct bio_router_struct* bio_router_t;

/* Immune system from nimcp_brain_immune.h */
typedef struct nimcp_brain_immune_struct* nimcp_brain_immune_t;

//=============================================================================
// Opaque Handle
//=============================================================================

/**
 * @brief Opaque layer coordinator handle
 */
typedef struct nimcp_layer_coordinator_struct* nimcp_layer_coordinator_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Layer coordinator configuration
 */
typedef struct {
    /* Registry config */
    nimcp_layer_registry_config_t registry_config;

    /* Router config */
    nimcp_inter_layer_router_config_t router_config;

    /* Update configuration */
    uint32_t update_interval_ms;    /**< Update interval */
    bool parallel_layer_update;     /**< Update layers in parallel */
    bool enable_coherence_tracking; /**< Track global coherence */

    /* Integration enables */
    bool enable_bio_async;          /**< Enable bio-async messaging */
    bool enable_immune_integration; /**< Enable immune system integration */
    bool enable_logging;            /**< Enable coordinator logging */
    bool enable_metrics;            /**< Enable metrics collection */

    /* Synchronization */
    float coherence_threshold;      /**< Min coherence for stable operation (0-1) */
    uint32_t sync_timeout_ms;       /**< Sync operation timeout */
} nimcp_layer_coordinator_config_t;

/**
 * @brief Layer coordinator state
 */
typedef enum {
    NIMCP_COORD_STATE_UNINITIALIZED = 0,
    NIMCP_COORD_STATE_INITIALIZING,
    NIMCP_COORD_STATE_RUNNING,
    NIMCP_COORD_STATE_PAUSED,
    NIMCP_COORD_STATE_SHUTTING_DOWN,
    NIMCP_COORD_STATE_ERROR
} nimcp_layer_coordinator_state_t;

/**
 * @brief Layer coordinator statistics
 */
typedef struct {
    nimcp_layer_coordinator_state_t state; /**< Current state */
    uint32_t layers_registered;     /**< Number of registered layers */
    uint32_t modules_registered;    /**< Total modules registered */
    uint32_t connections_active;    /**< Active inter-layer connections */
    float global_coherence;         /**< Global coherence (0-1) */
    float avg_layer_coherence;      /**< Average layer coherence */
    uint64_t update_count;          /**< Number of updates */
    uint64_t messages_routed;       /**< Total messages routed */
    float avg_update_time_ms;       /**< Average update time */
    uint64_t last_update_ns;        /**< Last update timestamp */
} nimcp_layer_coordinator_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default coordinator configuration
 */
NIMCP_EXPORT nimcp_layer_coordinator_config_t nimcp_layer_coordinator_default_config(void);

/**
 * @brief Create layer coordinator
 *
 * WHAT: Creates the central layer coordination system
 * WHY:  Single point of control for all layer operations
 * HOW:  Creates registry, router, initializes coordination structures
 *
 * @param config Configuration (NULL for defaults)
 * @param brain Brain instance (can be NULL for standalone)
 * @return Coordinator handle or NULL on failure
 */
NIMCP_EXPORT nimcp_layer_coordinator_t nimcp_layer_coordinator_create(
    const nimcp_layer_coordinator_config_t* config,
    brain_t brain
);

/**
 * @brief Destroy coordinator
 *
 * @param coord Coordinator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_layer_coordinator_destroy(nimcp_layer_coordinator_t coord);

/**
 * @brief Initialize all registered layers
 *
 * WHAT: Initializes all layers in dependency order
 * WHY:  Proper initialization sequence is critical
 * HOW:  Bottom-up initialization (Physics first, Superhuman last)
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_init_all(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Shutdown all layers
 *
 * WHAT: Shuts down all layers in reverse order
 * WHY:  Clean shutdown prevents resource leaks
 * HOW:  Top-down shutdown (Superhuman first, Physics last)
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_shutdown(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Reset coordinator to initial state
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_reset(
    nimcp_layer_coordinator_t coord
);

//=============================================================================
// Layer Registration API
//=============================================================================

/**
 * @brief Register a layer
 *
 * @param coord Coordinator handle
 * @param config Layer configuration
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_register_layer(
    nimcp_layer_coordinator_t coord,
    const nimcp_layer_config_t* config
);

/**
 * @brief Register standard layers
 *
 * WHAT: Registers all 9 standard layers with default configs
 * WHY:  Convenience function for typical usage
 * HOW:  Calls register_layer for each layer type
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_register_standard_layers(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Register standard inter-layer connections
 *
 * WHAT: Sets up default connections between layers
 * WHY:  Standard connectivity pattern from architecture
 * HOW:  Creates connections as defined in the layer hierarchy
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_register_standard_connections(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Register a module in a layer
 *
 * @param coord Coordinator handle
 * @param layer_id Layer to register in
 * @param module_ptr Module instance
 * @param interface Module interface
 * @param name Module name
 * @param module_id_out Output: assigned module ID
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_register_module(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t layer_id,
    void* module_ptr,
    nimcp_module_interface_t* interface,
    const char* name,
    uint32_t* module_id_out
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update all layers
 *
 * WHAT: Performs a single update tick on all layers
 * WHY:  Regular updates keep the system running
 * HOW:  Updates each layer, processes inter-layer messages
 *
 * @param coord Coordinator handle
 * @param dt Delta time in seconds
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_update(
    nimcp_layer_coordinator_t coord,
    float dt
);

/**
 * @brief Update a specific layer
 *
 * @param coord Coordinator handle
 * @param layer_id Layer to update
 * @param dt Delta time in seconds
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_update_layer(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t layer_id,
    float dt
);

/**
 * @brief Pause all layer updates
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_pause(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Resume layer updates
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_resume(
    nimcp_layer_coordinator_t coord
);

//=============================================================================
// Messaging API
//=============================================================================

/**
 * @brief Send a message between layers
 *
 * @param coord Coordinator handle
 * @param msg Message to send (ownership transferred)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_send_message(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_msg_t* msg
);

/**
 * @brief Broadcast message to all layers
 *
 * @param coord Coordinator handle
 * @param source_layer Source layer
 * @param msg Message to broadcast (cloned for each target)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_broadcast(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg
);

/**
 * @brief Broadcast bottom-up
 *
 * @param coord Coordinator handle
 * @param source_layer Source layer
 * @param msg Message to broadcast
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_broadcast_bottom_up(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg
);

/**
 * @brief Broadcast top-down
 *
 * @param coord Coordinator handle
 * @param source_layer Source layer
 * @param msg Message to broadcast
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_broadcast_top_down(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t source_layer,
    const nimcp_layer_msg_t* msg
);

//=============================================================================
// Synchronization API
//=============================================================================

/**
 * @brief Request global synchronization
 *
 * WHAT: Synchronizes all layers
 * WHY:  Ensure coherent state across layers
 * HOW:  Barrier sync with timeout
 *
 * @param coord Coordinator handle
 * @param timeout_ms Timeout in milliseconds (0 = use default)
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_sync(
    nimcp_layer_coordinator_t coord,
    uint32_t timeout_ms
);

/**
 * @brief Get global coherence
 *
 * WHAT: Returns measure of cross-layer coherence
 * WHY:  Indicates system stability
 * HOW:  Combines individual layer coherences
 *
 * @param coord Coordinator handle
 * @return Coherence value (0-1) or -1 on error
 */
NIMCP_EXPORT float nimcp_layer_coordinator_get_coherence(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Get layer coherence
 *
 * @param coord Coordinator handle
 * @param layer_id Layer ID
 * @return Coherence value (0-1) or -1 on error
 */
NIMCP_EXPORT float nimcp_layer_coordinator_get_layer_coherence(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_id_t layer_id
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to bio-async router
 *
 * @param coord Coordinator handle
 * @param router Bio-async router
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_connect_bio_async(
    nimcp_layer_coordinator_t coord,
    bio_router_t router
);

/**
 * @brief Connect to immune system
 *
 * @param coord Coordinator handle
 * @param immune Immune system
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_connect_immune(
    nimcp_layer_coordinator_t coord,
    nimcp_brain_immune_t immune
);

/**
 * @brief Get brain reference
 *
 * @param coord Coordinator handle
 * @return Brain pointer or NULL
 */
NIMCP_EXPORT brain_t nimcp_layer_coordinator_get_brain(
    nimcp_layer_coordinator_t coord
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get coordinator state
 *
 * @param coord Coordinator handle
 * @return Current state
 */
NIMCP_EXPORT nimcp_layer_coordinator_state_t nimcp_layer_coordinator_get_state(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Get coordinator statistics
 *
 * @param coord Coordinator handle
 * @param stats_out Output: statistics
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_get_stats(
    nimcp_layer_coordinator_t coord,
    nimcp_layer_coordinator_stats_t* stats_out
);

/**
 * @brief Reset coordinator statistics
 *
 * @param coord Coordinator handle
 * @return NIMCP_LAYER_OK or error
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_reset_stats(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Get layer registry
 *
 * @param coord Coordinator handle
 * @return Registry handle
 */
NIMCP_EXPORT nimcp_layer_registry_t nimcp_layer_coordinator_get_registry(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Get inter-layer router
 *
 * @param coord Coordinator handle
 * @return Router handle
 */
NIMCP_EXPORT nimcp_inter_layer_router_t nimcp_layer_coordinator_get_router(
    nimcp_layer_coordinator_t coord
);

//=============================================================================
// Error Handling API
//=============================================================================

/**
 * @brief Get last error
 *
 * @param coord Coordinator handle
 * @return Last error code
 */
NIMCP_EXPORT nimcp_layer_error_t nimcp_layer_coordinator_get_last_error(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Get last error message
 *
 * @param coord Coordinator handle
 * @return Error message string (static, don't free)
 */
NIMCP_EXPORT const char* nimcp_layer_coordinator_get_last_error_msg(
    nimcp_layer_coordinator_t coord
);

/**
 * @brief Clear last error
 *
 * @param coord Coordinator handle
 */
NIMCP_EXPORT void nimcp_layer_coordinator_clear_error(
    nimcp_layer_coordinator_t coord
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LAYER_COORDINATOR_H */
