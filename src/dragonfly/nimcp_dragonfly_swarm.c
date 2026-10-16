/**
 * @file nimcp_dragonfly_swarm.c
 * @brief Swarm Prey Detection and Optimal Target Selection Implementation
 *
 * WHAT: Detects prey swarms and selects optimal individual targets
 * WHY:  Enables efficient hunting in swarm scenarios
 * HOW:  Density estimation with isolation scoring and clustering
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_swarm.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_swarm)

//=============================================================================
// Constants
//=============================================================================

#define SWARM_DENSITY_KERNEL_RADIUS 2.0f  /* Radius for density estimation */
#define SWARM_MIN_CLUSTER_DISTANCE 0.5f   /* Minimum inter-cluster distance */

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static inline float vec3_distance(const float a[3], const float b[3]) {
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    float dz = a[2] - b[2];
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static inline void vec3_copy(float dst[3], const float src[3]) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void vec3_add(float dst[3], const float a[3], const float b[3]) {
    dst[0] = a[0] + b[0];
    dst[1] = a[1] + b[1];
    dst[2] = a[2] + b[2];
}

static inline void vec3_scale(float v[3], float s) {
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Individual detection record
 */
typedef struct {
    uint32_t id;
    float position[3];
    float velocity[3];
    float size;
    uint64_t detection_time_us;
    bool active;

    /* Computed during analysis */
    uint32_t cluster_id;
    float local_density;
    float isolation_score;
    float selection_score;
    swarm_position_t position_type;
} detection_record_t;

/**
 * @brief Internal swarm detector structure
 */
struct dragonfly_swarm_detector_s {
    /* Configuration */
    swarm_config_t config;

    /* Detection storage */
    detection_record_t detections[SWARM_MAX_INDIVIDUALS];
    uint32_t num_detections;

    /* Cluster storage */
    swarm_cluster_t clusters[SWARM_MAX_CLUSTERS];
    uint32_t num_clusters;

    /* Best targets from last analysis */
    uint32_t best_target_ids[5];
    uint32_t num_recommendations;

    /* Self position for selection scoring */
    float self_position[3];
    bool self_position_set;

    /* Statistics */
    swarm_stats_t stats;

    /* Timing */
    uint64_t last_analysis_us;
    uint64_t creation_time_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Configuration Functions
//=============================================================================

swarm_config_t swarm_default_config(void) {
    swarm_config_t config = {
        /* Clustering parameters */
        .cluster_distance_m = 0.5f,
        .min_cluster_size = 3,
        .isolation_threshold = 1.0f,

        /* Selection weights */
        .isolation_weight = 0.4f,
        .distance_weight = 0.3f,
        .size_weight = 0.2f,
        .velocity_weight = 0.1f,

        /* Strategy */
        .prefer_periphery = true,
        .avoid_dense_center = true,
        .danger_density_threshold = 0.8f,

        /* Update settings */
        .analysis_interval_ms = 50.0f,
        .track_swarm_dynamics = true
    };
    return config;
}

bool swarm_validate_config(const swarm_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "swarm_validate_config: config is NULL");
        return false;
    }

    if (config->cluster_distance_m <= 0.0f) {
        return false;
    }
    if (config->min_cluster_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "swarm_validate_config: config->min_cluster_size is zero");
        return false;
    }
    if (config->isolation_threshold <= 0.0f) {
        return false;
    }

    /* Weights should sum to approximately 1.0 */
    float weight_sum = config->isolation_weight + config->distance_weight +
                       config->size_weight + config->velocity_weight;
    if (weight_sum < 0.5f || weight_sum > 1.5f) {
        return false;
    }

    if (config->danger_density_threshold < 0.0f ||
        config->danger_density_threshold > 1.0f) return false;

    if (config->analysis_interval_ms < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_swarm_detector_t dragonfly_swarm_create(const swarm_config_t* config) {
    swarm_config_t cfg = config ? *config : swarm_default_config();

    if (!swarm_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_swarm_create: invalid config");
        return NULL;
    }

    dragonfly_swarm_detector_t detector = nimcp_calloc(1, sizeof(struct dragonfly_swarm_detector_s));
    NIMCP_API_CHECK_ALLOC(detector, "dragonfly_swarm_create: failed to allocate detector");

    detector->config = cfg;
    detector->creation_time_us = get_time_us();

    detector->mutex = nimcp_mutex_create(NULL);
    if (!detector->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_swarm_create: failed to create mutex");
        nimcp_free(detector);
        return NULL;
    }

    return detector;
}

void dragonfly_swarm_destroy(dragonfly_swarm_detector_t detector) {
    if (!detector) return;

    if (detector->mutex) {
        nimcp_mutex_destroy(detector->mutex);
    }

    nimcp_free(detector);
}

int dragonfly_swarm_reset(dragonfly_swarm_detector_t detector) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_reset: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    memset(detector->detections, 0, sizeof(detector->detections));
    detector->num_detections = 0;
    memset(detector->clusters, 0, sizeof(detector->clusters));
    detector->num_clusters = 0;
    detector->num_recommendations = 0;
    detector->self_position_set = false;
    detector->last_analysis_us = 0;

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

//=============================================================================
// Internal Clustering Functions
//=============================================================================

/**
 * @brief Compute local density for a detection
 */
static float compute_local_density(
    const dragonfly_swarm_detector_t detector,
    uint32_t detection_idx,
    float radius
) {
    const detection_record_t* target = &detector->detections[detection_idx];
    float density = 0.0f;

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (i == detection_idx) continue;
        if (!detector->detections[i].active) continue;

        float dist = vec3_distance(target->position,
                                   detector->detections[i].position);
        if (dist < radius) {
            /* Gaussian kernel weighting */
            float weight = expf(-0.5f * (dist / radius) * (dist / radius));
            density += weight;
        }
    }

    return density;
}

/**
 * @brief Find nearest neighbor distance for isolation scoring
 */
static float find_nearest_neighbor_distance(
    const dragonfly_swarm_detector_t detector,
    uint32_t detection_idx
) {
    const detection_record_t* target = &detector->detections[detection_idx];
    float min_dist = INFINITY;

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (i == detection_idx) continue;
        if (!detector->detections[i].active) continue;

        float dist = vec3_distance(target->position,
                                   detector->detections[i].position);
        if (dist < min_dist) {
            min_dist = dist;
        }
    }

    return min_dist;
}

/**
 * @brief Simple agglomerative clustering
 */
static void perform_clustering(dragonfly_swarm_detector_t detector) {
    /* Reset cluster assignments */
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        detector->detections[i].cluster_id = UINT32_MAX;
    }
    detector->num_clusters = 0;

    /* Simple single-linkage clustering */
    uint32_t cluster_id = 0;

    for (uint32_t i = 0; i < detector->num_detections &&
         detector->num_clusters < SWARM_MAX_CLUSTERS; i++) {
        if (!detector->detections[i].active) continue;
        if (detector->detections[i].cluster_id != UINT32_MAX) continue;

        /* Start new cluster */
        swarm_cluster_t* cluster = &detector->clusters[detector->num_clusters];
        memset(cluster, 0, sizeof(*cluster));
        cluster->cluster_id = cluster_id;

        /* Find all connected detections */
        detector->detections[i].cluster_id = cluster_id;
        cluster->count = 1;
        vec3_copy(cluster->centroid, detector->detections[i].position);
        vec3_copy(cluster->avg_velocity, detector->detections[i].velocity);

        /* Grow cluster by connecting nearby detections */
        bool changed = true;
        while (changed) {
            changed = false;
            for (uint32_t j = 0; j < detector->num_detections; j++) {
                if (!detector->detections[j].active) continue;
                if (detector->detections[j].cluster_id != UINT32_MAX) continue;

                /* Check if this detection is close to any cluster member */
                for (uint32_t k = 0; k < detector->num_detections; k++) {
                    if (detector->detections[k].cluster_id != cluster_id) continue;

                    float dist = vec3_distance(detector->detections[j].position,
                                               detector->detections[k].position);
                    if (dist < detector->config.cluster_distance_m) {
                        detector->detections[j].cluster_id = cluster_id;
                        cluster->count++;

                        /* Update running sums */
                        vec3_add(cluster->centroid, cluster->centroid,
                                 detector->detections[j].position);
                        vec3_add(cluster->avg_velocity, cluster->avg_velocity,
                                 detector->detections[j].velocity);

                        changed = true;
                        break;
                    }
                }
            }
        }

        /* Finalize cluster if large enough */
        if (cluster->count >= detector->config.min_cluster_size) {
            vec3_scale(cluster->centroid, 1.0f / cluster->count);
            vec3_scale(cluster->avg_velocity, 1.0f / cluster->count);

            /* Compute cluster extent and type */
            float min_pos[3] = {INFINITY, INFINITY, INFINITY};
            float max_pos[3] = {-INFINITY, -INFINITY, -INFINITY};

            for (uint32_t j = 0; j < detector->num_detections; j++) {
                if (detector->detections[j].cluster_id != cluster_id) continue;

                for (int d = 0; d < 3; d++) {
                    if (detector->detections[j].position[d] < min_pos[d]) {
                        min_pos[d] = detector->detections[j].position[d];
                    }
                    if (detector->detections[j].position[d] > max_pos[d]) {
                        max_pos[d] = detector->detections[j].position[d];
                    }
                }
            }

            cluster->extent[0] = max_pos[0] - min_pos[0];
            cluster->extent[1] = max_pos[1] - min_pos[1];
            cluster->extent[2] = max_pos[2] - min_pos[2];

            cluster->radius = vec3_length(cluster->extent) * 0.5f;

            /* Determine swarm type based on aspect ratio */
            float horiz_extent = sqrtf(cluster->extent[0] * cluster->extent[0] +
                                       cluster->extent[1] * cluster->extent[1]);
            float vert_extent = cluster->extent[2];

            if (vert_extent > horiz_extent * 2.0f) {
                cluster->type = SWARM_TYPE_COLUMN;
            } else if (horiz_extent > vert_extent * 3.0f) {
                cluster->type = SWARM_TYPE_STREAM;
            } else if (cluster->count > 20) {
                cluster->type = SWARM_TYPE_DENSE;
            } else if (cluster->count > 5) {
                cluster->type = SWARM_TYPE_CLOUD;
            } else {
                cluster->type = SWARM_TYPE_LOOSE;
            }

            /* Compute density */
            float volume = (cluster->extent[0] + 0.1f) *
                           (cluster->extent[1] + 0.1f) *
                           (cluster->extent[2] + 0.1f);
            cluster->density = (float)cluster->count / volume;

            detector->num_clusters++;
            cluster_id++;
        } else {
            /* Mark as isolated (no cluster) */
            for (uint32_t j = 0; j < detector->num_detections; j++) {
                if (detector->detections[j].cluster_id == cluster_id) {
                    detector->detections[j].cluster_id = UINT32_MAX;
                }
            }
        }
    }
}

/**
 * @brief Classify detection positions within swarm
 */
static void classify_positions(dragonfly_swarm_detector_t detector) {
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        detection_record_t* det = &detector->detections[i];
        if (!det->active) continue;

        float nn_dist = find_nearest_neighbor_distance(detector, i);

        if (det->cluster_id == UINT32_MAX) {
            /* Not in any cluster */
            if (nn_dist > detector->config.isolation_threshold * 2.0f) {
                det->position_type = POSITION_ISOLATED;
            } else {
                det->position_type = POSITION_STRAGGLER;
            }
            det->isolation_score = 1.0f;
        } else {
            /* In a cluster - check distance to centroid */
            swarm_cluster_t* cluster = NULL;
            for (uint32_t j = 0; j < detector->num_clusters; j++) {
                if (detector->clusters[j].cluster_id == det->cluster_id) {
                    cluster = &detector->clusters[j];
                    break;
                }
            }

            if (cluster) {
                float dist_to_center = vec3_distance(det->position, cluster->centroid);
                float relative_dist = dist_to_center / (cluster->radius + 0.01f);

                if (relative_dist < 0.3f) {
                    det->position_type = POSITION_CENTER;
                    det->isolation_score = 0.1f;
                } else if (relative_dist < 0.7f) {
                    det->position_type = POSITION_INTERIOR;
                    det->isolation_score = 0.3f;
                } else {
                    det->position_type = POSITION_PERIPHERY;
                    det->isolation_score = 0.7f;
                }

                /* Adjust based on nearest neighbor distance */
                float isolation_adjust = nn_dist / detector->config.cluster_distance_m;
                det->isolation_score = nimcp_clampf(
                    det->isolation_score * (0.5f + 0.5f * isolation_adjust),
                    0.0f, 1.0f
                );
            }
        }

        det->local_density = compute_local_density(detector, i, SWARM_DENSITY_KERNEL_RADIUS);
    }
}

/**
 * @brief Score detections for target selection
 */
static void score_selections(dragonfly_swarm_detector_t detector) {
    const swarm_config_t* cfg = &detector->config;

    /* Compute max values for normalization */
    float max_density = 0.01f;
    float max_dist = 0.01f;
    float max_size = 0.01f;
    float max_velocity = 0.01f;

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (!detector->detections[i].active) continue;

        if (detector->detections[i].local_density > max_density) {
            max_density = detector->detections[i].local_density;
        }

        if (detector->self_position_set) {
            float dist = vec3_distance(detector->detections[i].position,
                                       detector->self_position);
            if (dist > max_dist) max_dist = dist;
        }

        if (detector->detections[i].size > max_size) {
            max_size = detector->detections[i].size;
        }

        float vel = vec3_length(detector->detections[i].velocity);
        if (vel > max_velocity) max_velocity = vel;
    }

    /* Score each detection */
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        detection_record_t* det = &detector->detections[i];
        if (!det->active) continue;

        float score = 0.0f;

        /* Isolation score (higher = better) */
        score += cfg->isolation_weight * det->isolation_score;

        /* Distance score (closer = better) */
        if (detector->self_position_set) {
            float dist = vec3_distance(det->position, detector->self_position);
            float dist_score = 1.0f - (dist / max_dist);
            score += cfg->distance_weight * dist_score;
        }

        /* Size score (larger = better, within limits) */
        float size_score = det->size / max_size;
        score += cfg->size_weight * size_score;

        /* Velocity score (slower = easier to catch) */
        float vel = vec3_length(det->velocity);
        float vel_score = 1.0f - (vel / max_velocity);
        score += cfg->velocity_weight * vel_score;

        /* Penalty for dense center if configured */
        if (cfg->avoid_dense_center && det->position_type == POSITION_CENTER) {
            score *= 0.5f;
        }

        /* Bonus for periphery if preferred */
        if (cfg->prefer_periphery && det->position_type == POSITION_PERIPHERY) {
            score *= 1.2f;
        }

        det->selection_score = nimcp_clampf(score, 0.0f, 1.0f);
    }

    /* Find top recommendations */
    detector->num_recommendations = 0;

    for (uint32_t r = 0; r < 5 && r < detector->num_detections; r++) {
        float best_score = -1.0f;
        uint32_t best_idx = UINT32_MAX;

        for (uint32_t i = 0; i < detector->num_detections; i++) {
            if (!detector->detections[i].active) continue;

            /* Skip already recommended */
            bool already_recommended = false;
            for (uint32_t j = 0; j < detector->num_recommendations; j++) {
                if (detector->best_target_ids[j] == detector->detections[i].id) {
                    already_recommended = true;
                    break;
                }
            }
            if (already_recommended) continue;

            if (detector->detections[i].selection_score > best_score) {
                best_score = detector->detections[i].selection_score;
                best_idx = i;
            }
        }

        if (best_idx != UINT32_MAX) {
            detector->best_target_ids[detector->num_recommendations++] =
                detector->detections[best_idx].id;
        }
    }
}

//=============================================================================
// Detection Functions
//=============================================================================

int dragonfly_swarm_add_detection(
    dragonfly_swarm_detector_t detector,
    uint32_t id,
    const float position[3],
    const float velocity[3],
    float size
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_add_detection: detector is NULL");
        return -1;
    }
    if (!position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_add_detection: position is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    /* Check if ID already exists - update instead of add */
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (detector->detections[i].id == id && detector->detections[i].active) {
            vec3_copy(detector->detections[i].position, position);
            if (velocity) {
                vec3_copy(detector->detections[i].velocity, velocity);
            }
            detector->detections[i].size = size;
            detector->detections[i].detection_time_us = get_time_us();
            nimcp_mutex_unlock(detector->mutex);
            return 0;
        }
    }

    /* Add new detection */
    if (detector->num_detections >= SWARM_MAX_INDIVIDUALS) {
        /* Find oldest inactive slot */
        uint64_t oldest_time = UINT64_MAX;
        uint32_t oldest_idx = 0;
        for (uint32_t i = 0; i < SWARM_MAX_INDIVIDUALS; i++) {
            if (!detector->detections[i].active &&
                detector->detections[i].detection_time_us < oldest_time) {
                oldest_time = detector->detections[i].detection_time_us;
                oldest_idx = i;
            }
        }

        detection_record_t* det = &detector->detections[oldest_idx];
        det->id = id;
        vec3_copy(det->position, position);
        if (velocity) {
            vec3_copy(det->velocity, velocity);
        } else {
            memset(det->velocity, 0, sizeof(det->velocity));
        }
        det->size = size;
        det->detection_time_us = get_time_us();
        det->active = true;
    } else {
        detection_record_t* det = &detector->detections[detector->num_detections];
        det->id = id;
        vec3_copy(det->position, position);
        if (velocity) {
            vec3_copy(det->velocity, velocity);
        } else {
            memset(det->velocity, 0, sizeof(det->velocity));
        }
        det->size = size;
        det->detection_time_us = get_time_us();
        det->active = true;
        detector->num_detections++;
    }

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

int dragonfly_swarm_clear_detections(dragonfly_swarm_detector_t detector) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_clear_detections: detector is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        detector->detections[i].active = false;
    }
    detector->num_detections = 0;
    detector->num_clusters = 0;
    detector->num_recommendations = 0;

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

int dragonfly_swarm_analyze(
    dragonfly_swarm_detector_t detector,
    const float self_position[3],
    swarm_analysis_t* analysis
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_analyze: detector is NULL");
        return -1;
    }
    if (!analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_analyze: analysis is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    /* Store self position for scoring */
    if (self_position) {
        vec3_copy(detector->self_position, self_position);
        detector->self_position_set = true;
    }

    /* Count active detections */
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (detector->detections[i].active) active_count++;
    }

    if (active_count < 2) {
        /* Not enough for swarm analysis */
        memset(analysis, 0, sizeof(*analysis));
        analysis->total_individuals = active_count;
        analysis->timestamp_us = get_time_us();
        detector->last_analysis_us = analysis->timestamp_us;
        nimcp_mutex_unlock(detector->mutex);
        return 0;
    }

    /* Perform analysis */
    perform_clustering(detector);
    classify_positions(detector);
    score_selections(detector);

    /* Fill in analysis result */
    memset(analysis, 0, sizeof(*analysis));
    analysis->timestamp_us = get_time_us();
    analysis->total_individuals = active_count;

    /* Copy clusters */
    analysis->num_clusters = detector->num_clusters;
    memcpy(analysis->clusters, detector->clusters,
           sizeof(swarm_cluster_t) * detector->num_clusters);

    /* Copy recommendations */
    analysis->num_recommendations = detector->num_recommendations;
    memcpy(analysis->best_target_ids, detector->best_target_ids,
           sizeof(uint32_t) * detector->num_recommendations);

    /* Count isolated */
    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (detector->detections[i].active &&
            (detector->detections[i].position_type == POSITION_ISOLATED ||
             detector->detections[i].position_type == POSITION_STRAGGLER)) {
            analysis->isolated_count++;
        }
    }

    /* Average density */
    float total_density = 0.0f;
    for (uint32_t i = 0; i < detector->num_clusters; i++) {
        total_density += detector->clusters[i].density;
    }
    if (detector->num_clusters > 0) {
        analysis->avg_density = total_density / detector->num_clusters;
    }

    detector->last_analysis_us = analysis->timestamp_us;
    detector->stats.analyses_performed++;
    if (detector->num_clusters > 0) {
        detector->stats.swarms_detected++;
    }

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

//=============================================================================
// Selection Functions
//=============================================================================

int dragonfly_swarm_select_target(
    dragonfly_swarm_detector_t detector,
    const float self_position[3],
    float self_speed,
    swarm_individual_t* best_target
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_select_target: detector is NULL");
        return -1;
    }
    if (!best_target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_select_target: best_target is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    /* Update self position and re-score if needed */
    if (self_position) {
        vec3_copy(detector->self_position, self_position);
        detector->self_position_set = true;

        /* Re-run analysis if stale */
        uint64_t now = get_time_us();
        if (now - detector->last_analysis_us >
            (uint64_t)(detector->config.analysis_interval_ms * 1000.0f)) {
            perform_clustering(detector);
            classify_positions(detector);
            score_selections(detector);
            detector->last_analysis_us = now;
        }
    }

    /* Find best scoring target */
    float best_score = -1.0f;
    uint32_t best_idx = UINT32_MAX;

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (!detector->detections[i].active) continue;

        if (detector->detections[i].selection_score > best_score) {
            best_score = detector->detections[i].selection_score;
            best_idx = i;
        }
    }

    if (best_idx == UINT32_MAX) {
        nimcp_mutex_unlock(detector->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_swarm_select_target: validation failed");
        return -1;  /* No targets */
    }

    /* Fill in result */
    detection_record_t* det = &detector->detections[best_idx];
    best_target->id = det->id;
    vec3_copy(best_target->position, det->position);
    vec3_copy(best_target->velocity, det->velocity);
    best_target->cluster_id = det->cluster_id;
    best_target->position_type = det->position_type;
    best_target->isolation_score = det->isolation_score;
    best_target->local_density = det->local_density;
    best_target->selection_score = det->selection_score;
    best_target->recommended = true;

    detector->stats.targets_selected++;
    {
        float new_avg = (detector->stats.avg_isolation_score * (detector->stats.targets_selected - 1) +
             det->isolation_score) / detector->stats.targets_selected;
        if (isfinite(new_avg)) detector->stats.avg_isolation_score = new_avg;
    }

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

int dragonfly_swarm_get_recommendations(
    const dragonfly_swarm_detector_t detector,
    swarm_individual_t* targets,
    uint32_t max_targets,
    uint32_t* num_targets
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_recommendations: detector is NULL");
        return -1;
    }
    if (!targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_recommendations: targets is NULL");
        return -1;
    }
    if (!num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_recommendations: num_targets is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    uint32_t count = 0;
    for (uint32_t r = 0; r < detector->num_recommendations && count < max_targets; r++) {
        uint32_t target_id = detector->best_target_ids[r];

        /* Find this target */
        for (uint32_t i = 0; i < detector->num_detections; i++) {
            if (detector->detections[i].id == target_id &&
                detector->detections[i].active) {
                detection_record_t* det = &detector->detections[i];
                targets[count].id = det->id;
                vec3_copy(targets[count].position, det->position);
                vec3_copy(targets[count].velocity, det->velocity);
                targets[count].cluster_id = det->cluster_id;
                targets[count].position_type = det->position_type;
                targets[count].isolation_score = det->isolation_score;
                targets[count].local_density = det->local_density;
                targets[count].selection_score = det->selection_score;
                targets[count].recommended = true;
                count++;
                break;
            }
        }
    }

    *num_targets = count;

    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

bool dragonfly_swarm_is_dangerous(
    const dragonfly_swarm_detector_t detector,
    uint32_t target_id
) {
    if (!detector) {
        return false;
    }

    nimcp_mutex_lock(detector->mutex);

    bool dangerous = false;

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (detector->detections[i].id == target_id &&
            detector->detections[i].active) {
            /* Dangerous if in center of dense swarm */
            if (detector->detections[i].position_type == POSITION_CENTER &&
                detector->detections[i].local_density >
                    detector->config.danger_density_threshold) {
                dangerous = true;
                detector->stats.dense_center_avoids++;
            }
            break;
        }
    }

    nimcp_mutex_unlock(detector->mutex);

    return dangerous;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_swarm_get_individual(
    const dragonfly_swarm_detector_t detector,
    uint32_t id,
    swarm_individual_t* individual
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_individual: detector is NULL");
        return -1;
    }
    if (!individual) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_individual: individual is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->num_detections; i++) {
        if (detector->detections[i].id == id &&
            detector->detections[i].active) {
            detection_record_t* det = &detector->detections[i];
            individual->id = det->id;
            vec3_copy(individual->position, det->position);
            vec3_copy(individual->velocity, det->velocity);
            individual->cluster_id = det->cluster_id;
            individual->position_type = det->position_type;
            individual->isolation_score = det->isolation_score;
            individual->local_density = det->local_density;
            individual->selection_score = det->selection_score;
            individual->recommended = false;

            /* Check if recommended */
            for (uint32_t r = 0; r < detector->num_recommendations; r++) {
                if (detector->best_target_ids[r] == id) {
                    individual->recommended = true;
                    break;
                }
            }

            nimcp_mutex_unlock(detector->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    return -1;  /* Not found */
}

int dragonfly_swarm_get_cluster(
    const dragonfly_swarm_detector_t detector,
    uint32_t cluster_id,
    swarm_cluster_t* cluster
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_cluster: detector is NULL");
        return -1;
    }
    if (!cluster) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_cluster: cluster is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);

    for (uint32_t i = 0; i < detector->num_clusters; i++) {
        if (detector->clusters[i].cluster_id == cluster_id) {
            *cluster = detector->clusters[i];
            nimcp_mutex_unlock(detector->mutex);
            return 0;
        }
    }

    nimcp_mutex_unlock(detector->mutex);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_swarm_get_cluster: validation failed");
    return -1;  /* Not found */
}

int dragonfly_swarm_get_stats(
    const dragonfly_swarm_detector_t detector,
    swarm_stats_t* stats
) {
    if (!detector) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_stats: detector is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_swarm_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(detector->mutex);
    *stats = detector->stats;
    nimcp_mutex_unlock(detector->mutex);

    return 0;
}

const char* dragonfly_swarm_type_name(swarm_type_t type) {
    switch (type) {
        case SWARM_TYPE_NONE:   return "NONE";
        case SWARM_TYPE_LOOSE:  return "LOOSE";
        case SWARM_TYPE_DENSE:  return "DENSE";
        case SWARM_TYPE_COLUMN: return "COLUMN";
        case SWARM_TYPE_CLOUD:  return "CLOUD";
        case SWARM_TYPE_STREAM: return "STREAM";
        default:                return "UNKNOWN";
    }
}

const char* dragonfly_swarm_position_name(swarm_position_t position) {
    switch (position) {
        case POSITION_CENTER:    return "CENTER";
        case POSITION_INTERIOR:  return "INTERIOR";
        case POSITION_PERIPHERY: return "PERIPHERY";
        case POSITION_STRAGGLER: return "STRAGGLER";
        case POSITION_ISOLATED:  return "ISOLATED";
        default:                 return "UNKNOWN";
    }
}
