/**
 * @file nimcp_surface_geometry_bridge.h
 * @brief Surface Geometry Brain Integration Bridge
 *
 * WHAT: Bridge connecting surface geometry optimization to brain regions
 * WHY:  Enables brain-wide surface optimization for dendrite/axon branching
 *       based on Meng et al. Nature 2026 predictions
 * HOW:  Integrates via bridge_base_t pattern with bio-async messaging
 *
 * INTEGRATION:
 * - Bio-Async: Broadcasts geometry events, receives modulation requests
 * - Brain Regions: Connects to hippocampus, cortical columns, etc.
 * - Neural Structures: Modulates dendrite/axon branching geometry
 * - Immune System: Reports geometry anomalies for immune response
 *
 * @version 1.0.0
 * @date 2026-01-13
 */

#ifndef NIMCP_SURFACE_GEOMETRY_BRIDGE_H
#define NIMCP_SURFACE_GEOMETRY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"

//=============================================================================
// MODULE IDENTIFIERS
//=============================================================================

/** @brief Surface geometry module ID (0x1400 range) */
#define BIO_MODULE_SURFACE_GEOMETRY         0x1400
#define BIO_MODULE_SURFACE_GEOMETRY_BRAIN   0x1401
#define BIO_MODULE_SURFACE_GEOMETRY_SPINE   0x1402
#define BIO_MODULE_SURFACE_GEOMETRY_AXON    0x1403

//=============================================================================
// MESSAGE TYPES
//=============================================================================

/**
 * @brief Surface geometry bio-async message types
 */
typedef enum surface_bio_msg_type_enum {
    SURFACE_BIO_MSG_GEOMETRY_UPDATE = 0,        /**< Geometry parameter change */
    SURFACE_BIO_MSG_BRANCH_FORMED,              /**< New branch formed */
    SURFACE_BIO_MSG_TRIFURCATION_DETECTED,      /**< k=4 node detected (chi >= 0.83) */
    SURFACE_BIO_MSG_SPROUT_FORMED,              /**< Orthogonal sprout (rho < rho_th) */
    SURFACE_BIO_MSG_SYNAPSE_SPROUT,             /**< Sprout terminated at synapse */
    SURFACE_BIO_MSG_OPTIMIZATION_COMPLETE,      /**< Surface optimization finished */
    SURFACE_BIO_MSG_ANOMALY_DETECTED,           /**< Geometry anomaly found */
    SURFACE_BIO_MSG_MATERIAL_BUDGET_UPDATE,     /**< Material cost change */
    SURFACE_BIO_MSG_REQUEST_GEOMETRY,           /**< Request geometry computation */
    SURFACE_BIO_MSG_MODULATE_PARAMS,            /**< Modify geometry parameters */
    SURFACE_BIO_MSG_COUNT
} surface_bio_msg_type_t;

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Surface geometry bridge configuration
 */
typedef struct surface_geometry_bridge_config_struct {
    /* Base configuration */
    bool enable_modulation;                 /**< Enable parameter modulation */
    float sensitivity;                      /**< General sensitivity [0.5-2.0] */

    /* Geometry thresholds */
    float chi_trifurcation_threshold;       /**< Chi threshold for k=4 (default: 0.83) */
    float rho_threshold;                    /**< Rho threshold for sprouting (default: 0.6) */
    float angle_tolerance;                  /**< Tolerance for angle validation */

    /* Bio-async configuration */
    bool enable_bio_async;                  /**< Enable bio-async messaging */
    uint32_t update_interval_ms;            /**< Update broadcast interval */

    /* Optimization */
    surface_optimization_method_t default_method;  /**< Default optimization method */
    uint32_t max_optimization_iterations;   /**< Max optimization iterations */
} surface_geometry_bridge_config_t;

//=============================================================================
// BRIDGE STATISTICS
//=============================================================================

/**
 * @brief Surface geometry bridge statistics
 */
typedef struct surface_geometry_bridge_stats_struct {
    /* Computation stats */
    uint64_t total_geometry_computations;
    uint64_t total_optimizations;
    uint64_t total_validations;

    /* Branch type distribution */
    uint64_t bifurcations_detected;
    uint64_t trifurcations_detected;
    uint64_t higher_order_detected;

    /* Sprout statistics */
    uint64_t sprouts_formed;
    uint64_t synapse_sprouts;
    float sprout_synapse_ratio;

    /* Anomaly stats */
    uint64_t anomalies_detected;
    uint64_t anomalies_resolved;

    /* Message stats */
    uint64_t messages_sent;
    uint64_t messages_received;

    /* Performance */
    float avg_optimization_time_ms;
    float avg_computation_time_us;
} surface_geometry_bridge_stats_t;

//=============================================================================
// BRIDGE STRUCTURE
//=============================================================================

/**
 * @brief Surface geometry brain bridge
 *
 * Connects surface geometry optimization module to brain infrastructure.
 * Uses bridge_base_t pattern for consistent lifecycle management.
 */
typedef struct surface_geometry_bridge_struct {
    /* Base bridge (MUST be first) */
    bridge_base_t base;

    /* Surface geometry context */
    surface_geometry_ctx_t* geometry_ctx;

    /* Connected brain region (system_b) */
    /* system_a = geometry_ctx via base.system_a */

    /* Configuration */
    surface_geometry_bridge_config_t config;

    /* Statistics */
    surface_geometry_bridge_stats_t stats;

    /* Cached computations */
    spine_surface_cache_t* spine_cache;

    /* Active subscriptions for bio-async */
    uint32_t* message_subscriptions;
    uint32_t num_subscriptions;
    uint32_t max_subscriptions;
} surface_geometry_bridge_t;

//=============================================================================
// LIFECYCLE FUNCTIONS
//=============================================================================

/**
 * @brief Initialize bridge configuration with defaults
 *
 * Sets paper-derived defaults:
 * - chi_trifurcation_threshold = 0.83
 * - rho_threshold = 0.6
 * - enable_bio_async = true
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_default_config(surface_geometry_bridge_config_t* config);

/**
 * @brief Create surface geometry bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Created bridge or NULL on failure
 */
surface_geometry_bridge_t* surface_geometry_bridge_create(
    const surface_geometry_bridge_config_t* config
);

/**
 * @brief Destroy surface geometry bridge
 *
 * @param bridge Bridge to destroy
 */
void surface_geometry_bridge_destroy(surface_geometry_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_reset(surface_geometry_bridge_t* bridge);

//=============================================================================
// CONNECTION FUNCTIONS
//=============================================================================

/**
 * @brief Connect surface geometry context
 *
 * @param bridge Bridge
 * @param ctx Surface geometry context
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_connect_geometry(
    surface_geometry_bridge_t* bridge,
    surface_geometry_ctx_t* ctx
);

/**
 * @brief Connect brain region
 *
 * @param bridge Bridge
 * @param brain_region Brain region pointer
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_connect_brain(
    surface_geometry_bridge_t* bridge,
    void* brain_region
);

/**
 * @brief Disconnect geometry context
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_disconnect_geometry(surface_geometry_bridge_t* bridge);

/**
 * @brief Disconnect brain region
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_disconnect_brain(surface_geometry_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool surface_geometry_bridge_is_connected(const surface_geometry_bridge_t* bridge);

//=============================================================================
// BIO-ASYNC FUNCTIONS
//=============================================================================

/**
 * @brief Connect bridge to bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_connect_bio_async(surface_geometry_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_disconnect_bio_async(surface_geometry_bridge_t* bridge);

/**
 * @brief Check bio-async connection
 *
 * @param bridge Bridge
 * @return true if connected
 */
bool surface_geometry_bridge_is_bio_async_connected(const surface_geometry_bridge_t* bridge);

/**
 * @brief Subscribe to surface geometry messages
 *
 * @param bridge Bridge
 * @param msg_type Message type to subscribe
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_subscribe(
    surface_geometry_bridge_t* bridge,
    surface_bio_msg_type_t msg_type
);

/**
 * @brief Unsubscribe from messages
 *
 * @param bridge Bridge
 * @param msg_type Message type to unsubscribe
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_unsubscribe(
    surface_geometry_bridge_t* bridge,
    surface_bio_msg_type_t msg_type
);

//=============================================================================
// GEOMETRY COMPUTATION
//=============================================================================

/**
 * @brief Compute branch geometry for a branch point
 *
 * @param bridge Bridge
 * @param branch Branch point
 * @param params Output: computed parameters
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_compute_branch(
    surface_geometry_bridge_t* bridge,
    const surface_branch_point_t* branch,
    surface_geometry_params_t* params
);

/**
 * @brief Compute spine geometry
 *
 * @param bridge Bridge
 * @param parent_diameter Parent dendrite diameter
 * @param spine_diameter Spine neck diameter
 * @param spine_position Spine position
 * @param result Output: spine geometry
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_compute_spine(
    surface_geometry_bridge_t* bridge,
    float parent_diameter,
    float spine_diameter,
    const surface_vec3_t* spine_position,
    spine_surface_geometry_t* result
);

/**
 * @brief Compute axon branch geometry
 *
 * @param bridge Bridge
 * @param parent_diameter Parent axon diameter
 * @param child_diameters Child branch diameters
 * @param num_children Number of children
 * @param result Output: branch geometry
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_compute_axon_branch(
    surface_geometry_bridge_t* bridge,
    float parent_diameter,
    const float* child_diameters,
    uint32_t num_children,
    axon_branch_surface_geometry_t* result
);

//=============================================================================
// OPTIMIZATION
//=============================================================================

/**
 * @brief Optimize network geometry
 *
 * @param bridge Bridge
 * @param terminals Terminal positions [n][3]
 * @param num_terminals Number of terminals
 * @param min_circumference Minimum link circumference
 * @param result Output: optimization result
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_optimize(
    surface_geometry_bridge_t* bridge,
    const float (*terminals)[3],
    uint32_t num_terminals,
    float min_circumference,
    surface_optimization_result_t* result
);

/**
 * @brief Validate geometry against predictions
 *
 * @param bridge Bridge
 * @param params Parameters to validate
 * @param result Output: validation result
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_validate(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_params_t* params,
    surface_validation_result_t* result
);

//=============================================================================
// MESSAGING
//=============================================================================

/**
 * @brief Broadcast geometry update
 *
 * @param bridge Bridge
 * @param params Updated parameters
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_broadcast_update(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_params_t* params
);

/**
 * @brief Broadcast branch formation event
 *
 * @param bridge Bridge
 * @param branch_type Type of branch formed
 * @param position Position of new branch
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_broadcast_branch(
    surface_geometry_bridge_t* bridge,
    surface_branch_type_t branch_type,
    const float position[3]
);

/**
 * @brief Broadcast anomaly detection
 *
 * @param bridge Bridge
 * @param error_code Error code indicating anomaly type
 * @param branch Branch point with anomaly
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_broadcast_anomaly(
    surface_geometry_bridge_t* bridge,
    surface_error_t error_code,
    const surface_branch_point_t* branch
);

//=============================================================================
// STATISTICS
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_get_stats(
    const surface_geometry_bridge_t* bridge,
    surface_geometry_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_reset_stats(surface_geometry_bridge_t* bridge);

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief Get current configuration
 *
 * @param bridge Bridge
 * @param config Output: current configuration
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_get_config(
    const surface_geometry_bridge_t* bridge,
    surface_geometry_bridge_config_t* config
);

/**
 * @brief Update configuration
 *
 * @param bridge Bridge
 * @param config New configuration
 * @return 0 on success, -1 on error
 */
int surface_geometry_bridge_set_config(
    surface_geometry_bridge_t* bridge,
    const surface_geometry_bridge_config_t* config
);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name
 */
const char* surface_bio_msg_type_name(surface_bio_msg_type_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SURFACE_GEOMETRY_BRIDGE_H */
