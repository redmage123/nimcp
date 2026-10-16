/**
 * @file nimcp_dragonfly_swarm.h
 * @brief Swarm Prey Detection and Optimal Target Selection
 *
 * BIOLOGICAL REFERENCE:
 * Dragonflies often hunt swarms of midges or mosquitoes. They select
 * isolated individuals from the swarm periphery for easier interception
 * rather than diving into the dense swarm center.
 *
 * WHAT: Detects prey swarms and selects optimal individual targets
 * WHY:  Enables efficient hunting in swarm scenarios
 * HOW:  Density estimation with isolation scoring
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#ifndef NIMCP_DRAGONFLY_SWARM_H
#define NIMCP_DRAGONFLY_SWARM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct dragonfly_swarm_detector_s* dragonfly_swarm_detector_t;

//=============================================================================
// Constants
//=============================================================================

#define SWARM_MAX_INDIVIDUALS 128   /**< Maximum tracked individuals */
#define SWARM_MAX_CLUSTERS 8        /**< Maximum swarm clusters */

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Swarm structure type
 */
typedef enum {
    SWARM_TYPE_NONE,          /**< No swarm detected */
    SWARM_TYPE_LOOSE,         /**< Loosely grouped */
    SWARM_TYPE_DENSE,         /**< Dense swarm */
    SWARM_TYPE_COLUMN,        /**< Vertical column (midges) */
    SWARM_TYPE_CLOUD,         /**< Amorphous cloud */
    SWARM_TYPE_STREAM         /**< Linear stream */
} swarm_type_t;

/**
 * @brief Individual position relative to swarm
 */
typedef enum {
    POSITION_CENTER,          /**< Near swarm center */
    POSITION_INTERIOR,        /**< Inside swarm */
    POSITION_PERIPHERY,       /**< Swarm edge */
    POSITION_STRAGGLER,       /**< Separated from swarm */
    POSITION_ISOLATED         /**< Fully isolated */
} swarm_position_t;

/**
 * @brief Individual prey in swarm
 */
typedef struct {
    uint32_t id;                  /**< Individual ID */
    float position[3];            /**< Position */
    float velocity[3];            /**< Velocity */

    /* Swarm relationship */
    uint32_t cluster_id;          /**< Belonging cluster */
    swarm_position_t position_type; /**< Position in swarm */
    float isolation_score;        /**< Isolation [0,1] (higher=easier) */
    float local_density;          /**< Local density */

    /* Selection metrics */
    float selection_score;        /**< Overall selection score */
    bool recommended;             /**< Recommended target? */
} swarm_individual_t;

/**
 * @brief Swarm cluster
 */
typedef struct {
    uint32_t cluster_id;          /**< Cluster ID */
    swarm_type_t type;            /**< Swarm type */

    /* Geometry */
    float centroid[3];            /**< Cluster centroid */
    float extent[3];              /**< Bounding box extent */
    float radius;                 /**< Approximate radius */

    /* Statistics */
    uint32_t count;               /**< Number of individuals */
    float density;                /**< Average density */
    float avg_velocity[3];        /**< Average swarm velocity */
    float velocity_variance;      /**< Velocity coherence */

    /* Dynamics */
    float expansion_rate;         /**< Expansion/contraction rate */
    float rotation_rate;          /**< Swarm rotation rate */
} swarm_cluster_t;

/**
 * @brief Swarm analysis result
 */
typedef struct {
    /* Clusters */
    swarm_cluster_t clusters[SWARM_MAX_CLUSTERS];
    uint32_t num_clusters;

    /* Best targets */
    uint32_t best_target_ids[5];  /**< Top 5 recommended targets */
    uint32_t num_recommendations;

    /* Overall metrics */
    uint32_t total_individuals;   /**< Total detected */
    uint32_t isolated_count;      /**< Isolated individuals */
    float avg_density;            /**< Overall density */

    /* Timestamp */
    uint64_t timestamp_us;        /**< Analysis timestamp */
} swarm_analysis_t;

/**
 * @brief Swarm detector configuration
 */
typedef struct {
    /* Clustering parameters */
    float cluster_distance_m;     /**< Distance for clustering */
    uint32_t min_cluster_size;    /**< Minimum individuals for cluster */
    float isolation_threshold;    /**< Distance to be "isolated" */

    /* Selection weights */
    float isolation_weight;       /**< Weight for isolation */
    float distance_weight;        /**< Weight for distance to self */
    float size_weight;            /**< Weight for target size */
    float velocity_weight;        /**< Weight for target velocity */

    /* Strategy */
    bool prefer_periphery;        /**< Prefer peripheral targets */
    bool avoid_dense_center;      /**< Avoid dense swarm centers */
    float danger_density_threshold; /**< Density considered dangerous */

    /* Update settings */
    float analysis_interval_ms;   /**< Re-analysis interval */
    bool track_swarm_dynamics;    /**< Track swarm movement patterns */
} swarm_config_t;

/**
 * @brief Swarm detector statistics
 */
typedef struct {
    uint64_t analyses_performed;  /**< Total analyses */
    uint64_t swarms_detected;     /**< Times swarm detected */
    uint64_t targets_selected;    /**< Targets selected from swarms */
    float avg_isolation_score;    /**< Average isolation of selected */
    float swarm_hunt_success_rate;/**< Success rate in swarms */
    uint32_t dense_center_avoids; /**< Times avoided dense center */
} swarm_stats_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default swarm configuration
 */
swarm_config_t swarm_default_config(void);

/**
 * @brief Validate swarm configuration
 */
bool swarm_validate_config(const swarm_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create swarm detector
 */
dragonfly_swarm_detector_t dragonfly_swarm_create(const swarm_config_t* config);

/**
 * @brief Destroy swarm detector
 */
void dragonfly_swarm_destroy(dragonfly_swarm_detector_t detector);

/**
 * @brief Reset swarm detector
 */
int dragonfly_swarm_reset(dragonfly_swarm_detector_t detector);

//=============================================================================
// Detection Functions
//=============================================================================

/**
 * @brief Add individual detection
 */
int dragonfly_swarm_add_detection(
    dragonfly_swarm_detector_t detector,
    uint32_t id,
    const float position[3],
    const float velocity[3],
    float size
);

/**
 * @brief Clear all detections
 */
int dragonfly_swarm_clear_detections(dragonfly_swarm_detector_t detector);

/**
 * @brief Perform swarm analysis
 */
int dragonfly_swarm_analyze(
    dragonfly_swarm_detector_t detector,
    const float self_position[3],
    swarm_analysis_t* analysis
);

//=============================================================================
// Selection Functions
//=============================================================================

/**
 * @brief Get best target from swarm
 */
int dragonfly_swarm_select_target(
    dragonfly_swarm_detector_t detector,
    const float self_position[3],
    float self_speed,
    swarm_individual_t* best_target
);

/**
 * @brief Get target recommendations
 */
int dragonfly_swarm_get_recommendations(
    const dragonfly_swarm_detector_t detector,
    swarm_individual_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
);

/**
 * @brief Check if target is in dangerous dense area
 */
bool dragonfly_swarm_is_dangerous(
    const dragonfly_swarm_detector_t detector,
    uint32_t target_id
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get individual info
 */
int dragonfly_swarm_get_individual(
    const dragonfly_swarm_detector_t detector,
    uint32_t id,
    swarm_individual_t* individual
);

/**
 * @brief Get cluster info
 */
int dragonfly_swarm_get_cluster(
    const dragonfly_swarm_detector_t detector,
    uint32_t cluster_id,
    swarm_cluster_t* cluster
);

/**
 * @brief Get swarm statistics
 */
int dragonfly_swarm_get_stats(
    const dragonfly_swarm_detector_t detector,
    swarm_stats_t* stats
);

/**
 * @brief Get swarm type name
 */
const char* dragonfly_swarm_type_name(swarm_type_t type);

/**
 * @brief Get position type name
 */
const char* dragonfly_swarm_position_name(swarm_position_t position);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_SWARM_H */
