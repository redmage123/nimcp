/**
 * @file nimcp_swarm_pheromone.h
 * @brief Stigmergic Pheromone Trails for swarm coordination
 *
 * Biological Inspiration: Ant colony pheromone communication
 *
 * This module implements a spatial pheromone system for indirect communication
 * between swarm agents. Pheromones are deposited in the environment, decay over
 * time, and influence agent behavior through gradient following.
 *
 * Key features:
 * - Multiple pheromone types (danger, resource, path, etc.)
 * - 3D voxel-based spatial grid with efficient queries
 * - Exponential decay with configurable rates
 * - Environmental modifiers and reinforcement
 * - Bio-async integration for distributed coordination
 * - BBB security validation
 *
 * @author NIMCP Team
 * @date 2025-12-08
 */

#ifndef NIMCP_SWARM_PHEROMONE_H
#define NIMCP_SWARM_PHEROMONE_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_blood_brain_barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pheromone types for different signaling purposes
 */
typedef enum {
    PHEROMONE_DANGER = 0,    /**< Marks dangerous areas to avoid */
    PHEROMONE_RESOURCE,      /**< Marks resource locations (food, energy) */
    PHEROMONE_PATH,          /**< Marks successful navigation paths */
    PHEROMONE_AVOID,         /**< Marks areas to generally avoid */
    PHEROMONE_RALLY,         /**< Marks rally/gathering points */
    PHEROMONE_TARGET,        /**< Marks target destinations */
    PHEROMONE_TYPE_COUNT     /**< Total number of pheromone types */
} nimcp_pheromone_type_t;

/**
 * @brief 3D position in space
 */
typedef struct {
    float x;  /**< X coordinate */
    float y;  /**< Y coordinate */
    float z;  /**< Z coordinate */
} nimcp_position3d_t;

/**
 * @brief Pheromone concentration at a voxel
 */
typedef struct {
    float concentration[PHEROMONE_TYPE_COUNT];  /**< Concentration per type */
    uint64_t last_update;                        /**< Timestamp of last update */
    float environmental_modifier;                /**< Environmental factor (0.0-1.0) */
} nimcp_pheromone_voxel_t;

/**
 * @brief Pheromone gradient information
 */
typedef struct {
    nimcp_pheromone_type_t type;     /**< Type of pheromone */
    nimcp_position3d_t direction;     /**< Direction of strongest gradient */
    float magnitude;                  /**< Gradient magnitude */
    float concentration;              /**< Concentration at current location */
} nimcp_pheromone_gradient_t;

/**
 * @brief Pheromone trail segment
 */
typedef struct {
    nimcp_position3d_t position;     /**< Position in space */
    nimcp_pheromone_type_t type;     /**< Pheromone type */
    float concentration;              /**< Concentration value */
    uint64_t timestamp;               /**< When deposited */
} nimcp_pheromone_trail_t;

/**
 * @brief Configuration for pheromone system
 */
typedef struct {
    /* Grid parameters */
    nimcp_position3d_t world_min;     /**< Minimum world coordinates */
    nimcp_position3d_t world_max;     /**< Maximum world coordinates */
    float voxel_size;                 /**< Size of each voxel */

    /* Decay parameters */
    float decay_rates[PHEROMONE_TYPE_COUNT];  /**< Decay rate per type (per second) */
    float evaporation_rate;                    /**< Base evaporation rate */

    /* Deposit parameters */
    float max_concentration;          /**< Maximum concentration per voxel */
    float deposit_amount;             /**< Default deposit amount */
    float reinforcement_factor;       /**< Path reinforcement multiplier */

    /* Query parameters */
    float detection_threshold;        /**< Minimum detectable concentration */
    float gradient_sample_distance;   /**< Distance for gradient calculation */

    /* Bio-async parameters */
    bool enable_bio_async;            /**< Enable bio-async integration */
    uint32_t broadcast_interval_ms;   /**< Broadcast update interval */
} nimcp_pheromone_config_t;

/**
 * @brief Pheromone system state
 */
typedef struct nimcp_pheromone_system_t nimcp_pheromone_system_t;

/**
 * @brief Statistics for pheromone system
 */
typedef struct {
    uint64_t total_deposits;          /**< Total pheromone deposits */
    uint64_t total_queries;           /**< Total queries performed */
    uint64_t active_voxels;           /**< Number of voxels with pheromones */
    float avg_concentration[PHEROMONE_TYPE_COUNT];  /**< Average per type */
    uint64_t last_decay_time;         /**< Last decay update timestamp */
    uint64_t broadcasts_sent;         /**< Bio-async broadcasts sent */
} nimcp_pheromone_stats_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a new pheromone system
 *
 * @param config Configuration parameters
 * @param bbb Blood-brain barrier for security (can be NULL)
 * @return Pointer to new pheromone system, or NULL on failure
 */
nimcp_pheromone_system_t* nimcp_pheromone_create(
    const nimcp_pheromone_config_t* config,
    bbb_system_t bbb
);

/**
 * @brief Destroy pheromone system and free resources
 *
 * @param system Pheromone system to destroy
 */
void nimcp_pheromone_destroy(nimcp_pheromone_system_t* system);

/**
 * @brief Reset pheromone system (clear all pheromones)
 *
 * @param system Pheromone system to reset
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_reset(nimcp_pheromone_system_t* system);

/* ============================================================================
 * Pheromone Deposit Functions
 * ============================================================================ */

/**
 * @brief Deposit pheromone at a location
 *
 * @param system Pheromone system
 * @param position Position to deposit at
 * @param type Type of pheromone
 * @param amount Amount to deposit
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_deposit(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float amount
);

/**
 * @brief Deposit pheromone with environmental modifier
 *
 * @param system Pheromone system
 * @param position Position to deposit at
 * @param type Type of pheromone
 * @param amount Amount to deposit
 * @param env_modifier Environmental modifier (0.0-1.0)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_deposit_modified(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float amount,
    float env_modifier
);

/**
 * @brief Reinforce a path with pheromone
 *
 * Applies reinforcement factor to strengthen successful paths
 *
 * @param system Pheromone system
 * @param path Array of positions forming the path
 * @param path_length Number of positions in path
 * @param type Type of pheromone
 * @param success_factor Success multiplier (1.0 = normal, >1.0 = stronger)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_reinforce_path(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* path,
    size_t path_length,
    nimcp_pheromone_type_t type,
    float success_factor
);

/* ============================================================================
 * Pheromone Query Functions
 * ============================================================================ */

/**
 * @brief Get pheromone concentration at a location
 *
 * @param system Pheromone system
 * @param position Position to query
 * @param type Type of pheromone
 * @param out_concentration Output concentration value
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_get_concentration(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float* out_concentration
);

/**
 * @brief Find strongest pheromone gradient at a location
 *
 * Calculates the direction of steepest concentration increase
 *
 * @param system Pheromone system
 * @param position Current position
 * @param type Type of pheromone to follow
 * @param out_gradient Output gradient information
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_get_gradient(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    nimcp_pheromone_gradient_t* out_gradient
);

/**
 * @brief Get all pheromone trails within radius
 *
 * @param system Pheromone system
 * @param center Center position
 * @param radius Search radius
 * @param type Type of pheromone (use PHEROMONE_TYPE_COUNT for all types)
 * @param out_trails Output array of trails (caller must allocate)
 * @param max_trails Maximum number of trails to return
 * @param out_count Actual number of trails returned
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_query_radius(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* center,
    float radius,
    nimcp_pheromone_type_t type,
    nimcp_pheromone_trail_t* out_trails,
    size_t max_trails,
    size_t* out_count
);

/**
 * @brief Plan a path following pheromone trails
 *
 * Uses gradient descent to find path from start to area with high pheromone
 *
 * @param system Pheromone system
 * @param start Start position
 * @param type Type of pheromone to follow
 * @param max_steps Maximum path steps
 * @param out_path Output path array (caller must allocate)
 * @param out_path_length Actual path length
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_plan_path(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* start,
    nimcp_pheromone_type_t type,
    size_t max_steps,
    nimcp_position3d_t* out_path,
    size_t* out_path_length
);

/* ============================================================================
 * Update and Maintenance Functions
 * ============================================================================ */

/**
 * @brief Update pheromone system (decay, evaporation)
 *
 * Should be called regularly to update pheromone concentrations
 *
 * @param system Pheromone system
 * @param delta_time_ms Time elapsed since last update (milliseconds)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_update(
    nimcp_pheromone_system_t* system,
    uint64_t delta_time_ms
);

/**
 * @brief Set environmental modifier for a region
 *
 * Environmental modifiers affect decay rates and diffusion
 *
 * @param system Pheromone system
 * @param center Center of region
 * @param radius Radius of region
 * @param modifier Modifier value (0.0 = fast decay, 1.0 = normal)
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_set_environment(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* center,
    float radius,
    float modifier
);

/* ============================================================================
 * Bio-Async Integration Functions
 * ============================================================================ */

/**
 * @brief Register pheromone system with bio-async router
 *
 * @param system Pheromone system
 * @param router Bio-async router
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_register_bioasync(
    nimcp_pheromone_system_t* system,
    bio_router_t* router
);

/**
 * @brief Broadcast pheromone update to swarm
 *
 * @param system Pheromone system
 * @param position Position of update
 * @param type Type of pheromone
 * @param concentration New concentration value
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_broadcast_update(
    nimcp_pheromone_system_t* system,
    const nimcp_position3d_t* position,
    nimcp_pheromone_type_t type,
    float concentration
);

/**
 * @brief Handle incoming pheromone message from swarm
 *
 * @param system Pheromone system
 * @param message Bio-async message (void* for handler compatibility)
 * @param msg_size Size of message
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_handle_message(
    nimcp_pheromone_system_t* system,
    const void* message,
    size_t msg_size
);

/* ============================================================================
 * Utility and Statistics Functions
 * ============================================================================ */

/**
 * @brief Get pheromone system statistics
 *
 * @param system Pheromone system
 * @param out_stats Output statistics structure
 * @return NIMCP_OK on success, error code otherwise
 */
nimcp_result_t nimcp_pheromone_get_stats(
    nimcp_pheromone_system_t* system,
    nimcp_pheromone_stats_t* out_stats
);

/**
 * @brief Get default configuration
 *
 * @param out_config Output configuration with default values
 */
void nimcp_pheromone_default_config(nimcp_pheromone_config_t* out_config);

/**
 * @brief Get pheromone type name string
 *
 * @param type Pheromone type
 * @return String name of type
 */
const char* nimcp_pheromone_type_name(nimcp_pheromone_type_t type);

/**
 * @brief Validate pheromone system configuration
 *
 * @param config Configuration to validate
 * @return NIMCP_OK if valid, error code otherwise
 */
nimcp_result_t nimcp_pheromone_validate_config(
    const nimcp_pheromone_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_PHEROMONE_H */
