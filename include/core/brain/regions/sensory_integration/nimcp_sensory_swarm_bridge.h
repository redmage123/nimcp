/**
 * @file nimcp_sensory_swarm_bridge.h
 * @brief Unified Sensory-Swarm Integration Bridge
 * @version 1.0.0
 * @date 2026-01-12
 *
 * WHAT: Consolidated swarm integration for all Phase 6 sensory modules
 *       (somatosensory, olfactory, gustatory) enabling distributed sensory
 *       processing across swarm nodes.
 *
 * WHY: Swarm computing enhances sensory processing by:
 *      - Distributed tactile exploration and mapping
 *      - Collective odor tracking and source localization
 *      - Swarm-based food quality evaluation
 *      - Parallel sensory pattern matching
 *      - Consensus-based sensory integration
 *
 * HOW: Registers sensory modules as swarm participants, distributes sensory
 *      tasks, aggregates distributed results, and maintains swarm coherence
 *      for sensory consensus.
 *
 * SWARM APPLICATIONS:
 * ===================
 * 1. SOMATOSENSORY:
 *    - Distributed surface exploration
 *    - Parallel texture classification
 *    - Collective body mapping
 *
 * 2. OLFACTORY:
 *    - Multi-agent odor tracking
 *    - Source localization via gradient following
 *    - Distributed odor classification
 *
 * 3. GUSTATORY:
 *    - Collective food evaluation
 *    - Parallel toxicity assessment
 *    - Swarm-based preference aggregation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SENSORY_SWARM_BRIDGE_H
#define NIMCP_SENSORY_SWARM_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "core/brain/regions/somatosensory/nimcp_somatosensory.h"
#include "core/brain/regions/olfactory/nimcp_olfactory.h"
#include "core/brain/regions/gustatory/nimcp_gustatory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SENSORY_SWARM_MAX_NODES         256
#define SENSORY_SWARM_MAX_TASKS         64
#define SENSORY_SWARM_CONSENSUS_QUORUM  0.6f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sensory modality for swarm tasks
 */
typedef enum {
    SENSORY_SWARM_MODALITY_TOUCH = 0,
    SENSORY_SWARM_MODALITY_SMELL,
    SENSORY_SWARM_MODALITY_TASTE,
    SENSORY_SWARM_MODALITY_MULTIMODAL,
    SENSORY_SWARM_MODALITY_COUNT
} sensory_swarm_modality_t;

/**
 * @brief Swarm task types
 */
typedef enum {
    SENSORY_SWARM_TASK_EXPLORE = 0,     /**< Distributed exploration */
    SENSORY_SWARM_TASK_CLASSIFY,        /**< Pattern classification */
    SENSORY_SWARM_TASK_TRACK,           /**< Source/gradient tracking */
    SENSORY_SWARM_TASK_EVALUATE,        /**< Quality evaluation */
    SENSORY_SWARM_TASK_CONSENSUS,       /**< Consensus building */
    SENSORY_SWARM_TASK_COUNT
} sensory_swarm_task_type_t;

/**
 * @brief Task status
 */
typedef enum {
    SENSORY_SWARM_STATUS_PENDING = 0,
    SENSORY_SWARM_STATUS_DISTRIBUTED,
    SENSORY_SWARM_STATUS_PROCESSING,
    SENSORY_SWARM_STATUS_AGGREGATING,
    SENSORY_SWARM_STATUS_COMPLETE,
    SENSORY_SWARM_STATUS_FAILED
} sensory_swarm_task_status_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Swarm node for sensory processing
 */
typedef struct {
    uint32_t node_id;                   /**< Node identifier */
    sensory_swarm_modality_t modality;  /**< Assigned modality */
    float position[3];                  /**< Node position */
    float sensory_value;                /**< Current sensory reading */
    float confidence;                   /**< Reading confidence */
    bool active;                        /**< Node active */
    uint64_t last_update;               /**< Last update time */
} sensory_swarm_node_t;

/**
 * @brief Distributed sensory task
 */
typedef struct {
    uint32_t task_id;                   /**< Task identifier */
    sensory_swarm_task_type_t type;     /**< Task type */
    sensory_swarm_modality_t modality;  /**< Target modality */
    sensory_swarm_task_status_t status; /**< Current status */

    /* Task parameters */
    float* input_data;                  /**< Input data */
    uint32_t input_dim;                 /**< Input dimensionality */
    float* target_position;             /**< Target position (if applicable) */

    /* Distributed results */
    uint32_t* assigned_nodes;           /**< Assigned node IDs */
    uint32_t num_assigned;              /**< Number of assigned nodes */
    float* node_results;                /**< Results from each node */
    float* node_confidences;            /**< Confidence from each node */

    /* Aggregated result */
    float aggregated_result;            /**< Aggregated result */
    float consensus_confidence;         /**< Consensus confidence */

    uint64_t start_time;                /**< Task start time */
    uint64_t completion_time;           /**< Completion time */
} sensory_swarm_task_t;

/**
 * @brief Exploration result
 */
typedef struct {
    float* explored_map;                /**< Explored sensory map */
    uint32_t map_dim[3];                /**< Map dimensions */
    float coverage;                     /**< Coverage percentage */
    uint32_t interesting_points;        /**< Number of interesting points */
    float* hotspot_positions;           /**< Hotspot positions */
    uint32_t num_hotspots;              /**< Number of hotspots */
} sensory_swarm_exploration_t;

/**
 * @brief Tracking result
 */
typedef struct {
    float source_position[3];           /**< Estimated source position */
    float source_confidence;            /**< Position confidence */
    float gradient_direction[3];        /**< Gradient direction */
    float gradient_magnitude;           /**< Gradient magnitude */
    bool source_found;                  /**< Source located */
} sensory_swarm_tracking_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t max_nodes;
    uint32_t max_tasks;
    float consensus_quorum;
    float node_timeout_ms;

    bool enable_touch_swarm;
    bool enable_smell_swarm;
    bool enable_taste_swarm;

    float exploration_step_size;
    float tracking_gain;
    float consensus_decay;

    bool enable_logging;
} sensory_swarm_config_t;

typedef struct {
    uint32_t active_nodes;
    uint32_t active_tasks;
    uint64_t tasks_completed;
    uint64_t tasks_failed;

    uint64_t touch_tasks;
    uint64_t smell_tasks;
    uint64_t taste_tasks;

    float avg_consensus_confidence;
    float avg_task_time_ms;
} sensory_swarm_stats_t;

/* ============================================================================
 * Handle
 * ============================================================================ */

typedef struct sensory_swarm_bridge_struct sensory_swarm_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int sensory_swarm_default_config(sensory_swarm_config_t* config);
sensory_swarm_bridge_t* sensory_swarm_bridge_create(const sensory_swarm_config_t* config);
void sensory_swarm_bridge_destroy(sensory_swarm_bridge_t* bridge);

/* ============================================================================
 * Module Registration API
 * ============================================================================ */

int sensory_swarm_register_somatosensory(sensory_swarm_bridge_t* bridge, nimcp_somatosensory_t* soma);
int sensory_swarm_register_olfactory(sensory_swarm_bridge_t* bridge, nimcp_olfactory_t* olfact);
int sensory_swarm_register_gustatory(sensory_swarm_bridge_t* bridge, nimcp_gustatory_t* gust);

/* ============================================================================
 * Node Management API
 * ============================================================================ */

int sensory_swarm_add_node(sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality, const float* position, uint32_t* node_id);
int sensory_swarm_remove_node(sensory_swarm_bridge_t* bridge, uint32_t node_id);
int sensory_swarm_update_node(sensory_swarm_bridge_t* bridge, uint32_t node_id, float sensory_value, float confidence);
int sensory_swarm_get_node_count(const sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality);

/* ============================================================================
 * Task Management API
 * ============================================================================ */

int sensory_swarm_submit_task(sensory_swarm_bridge_t* bridge, sensory_swarm_task_type_t type, sensory_swarm_modality_t modality, const float* input, uint32_t input_dim, uint32_t* task_id);
int sensory_swarm_get_task_status(sensory_swarm_bridge_t* bridge, uint32_t task_id, sensory_swarm_task_status_t* status);
int sensory_swarm_get_task_result(sensory_swarm_bridge_t* bridge, uint32_t task_id, float* result, float* confidence);
int sensory_swarm_cancel_task(sensory_swarm_bridge_t* bridge, uint32_t task_id);

/* ============================================================================
 * Exploration API
 * ============================================================================ */

int sensory_swarm_explore_tactile(sensory_swarm_bridge_t* bridge, const float* bounds, sensory_swarm_exploration_t* result);
int sensory_swarm_explore_olfactory(sensory_swarm_bridge_t* bridge, const float* bounds, sensory_swarm_exploration_t* result);

/* ============================================================================
 * Tracking API
 * ============================================================================ */

int sensory_swarm_track_odor(sensory_swarm_bridge_t* bridge, const float* initial_position, sensory_swarm_tracking_t* result);
int sensory_swarm_track_texture(sensory_swarm_bridge_t* bridge, float target_texture, sensory_swarm_tracking_t* result);

/* ============================================================================
 * Consensus API
 * ============================================================================ */

int sensory_swarm_build_consensus(sensory_swarm_bridge_t* bridge, sensory_swarm_modality_t modality, float* consensus_value, float* confidence);
int sensory_swarm_evaluate_food(sensory_swarm_bridge_t* bridge, const taste_stimulus_t* stimulus, float* quality, float* confidence);

/* ============================================================================
 * Update API
 * ============================================================================ */

int sensory_swarm_update(sensory_swarm_bridge_t* bridge, float dt);
int sensory_swarm_synchronize(sensory_swarm_bridge_t* bridge);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

int sensory_swarm_get_stats(const sensory_swarm_bridge_t* bridge, sensory_swarm_stats_t* stats);
int sensory_swarm_reset_stats(sensory_swarm_bridge_t* bridge);
void sensory_swarm_print_summary(const sensory_swarm_bridge_t* bridge);

/* ============================================================================
 * Result Cleanup
 * ============================================================================ */

void sensory_swarm_exploration_free(sensory_swarm_exploration_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SENSORY_SWARM_BRIDGE_H */
