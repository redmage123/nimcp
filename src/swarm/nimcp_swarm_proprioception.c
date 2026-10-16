/**
 * @file nimcp_swarm_proprioception.c
 * @brief Implementation of Collective Proprioception System
 *
 * This implementation provides distributed sensing capabilities inspired by:
 * - Spider web sensing: Vibrations propagate through the network
 * - Organism proprioception: Internal sense of position and movement
 * - Fish schooling: Local awareness enabling global coordination
 */

#include "swarm/nimcp_swarm_proprioception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "api/nimcp_api_exception.h"

#define LOG_MODULE "swarm_proprioception"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_proprioception)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "utils/exception/nimcp_exception_macros.h"

/* Helper: convert microseconds to nanoseconds */
#define nimcp_get_timestamp_ns() (nimcp_time_get_us() * 1000ULL)

/**
 * @brief Internal proprioception state structure
 */
struct nimcp_swarm_proprioception {
    uint32_t drone_id;                              /**< This drone's ID */
    nimcp_swarm_proprio_config_t config;            /**< Configuration */
    void* bio_ctx;                   /**< Bio-async context */

    /* Position tracking */
    nimcp_swarm_position_t position;                /**< Current position */
    nimcp_swarm_velocity_t velocity;                /**< Current velocity */
    nimcp_swarm_position_history_t history[NIMCP_SWARM_PROPRIO_HISTORY_SIZE];
    uint32_t history_index;                         /**< Current history index */
    uint64_t last_position_update;                  /**< Last position update time */

    /* Neighbor tracking */
    nimcp_swarm_neighbor_t neighbors[NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS];
    uint32_t num_neighbors;                         /**< Active neighbors count */
    uint64_t last_neighbor_update;                  /**< Last neighbor update time */

    /* Shape and formation */
    nimcp_swarm_shape_descriptor_t current_shape;   /**< Current shape */
    nimcp_swarm_formation_metrics_t formation;      /**< Formation metrics */
    uint64_t last_shape_classification;             /**< Last classification time */

    /* Deformation tracking */
    nimcp_swarm_deformation_metrics_t deformation;  /**< Current deformation */
    double reference_distances[NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS]; /**< Reference neighbor distances */
    bool has_reference;                             /**< Has reference formation */

    /* Boundary awareness */
    nimcp_swarm_boundary_descriptor_t boundary;     /**< Boundary descriptor */
    uint64_t last_boundary_update;                  /**< Last boundary update */

    /* Density mapping */
    nimcp_swarm_density_info_t density;             /**< Density information */
    uint64_t last_density_update;                   /**< Last density update */

    /* Center-of-mass */
    nimcp_swarm_com_estimate_t com_estimate;        /**< COM estimate */
    uint64_t last_com_update;                       /**< Last COM update */

    /* Vibration sensing */
    nimcp_swarm_vibration_data_t vibration;         /**< Vibration data */
    double vibration_history[NIMCP_SWARM_PROPRIO_HISTORY_SIZE]; /**< Signal history */
    uint32_t vibration_history_index;               /**< History index */

    /* Statistics */
    uint64_t total_updates;                         /**< Total position updates */
    uint64_t total_messages_sent;                   /**< Total messages sent */
    uint64_t total_messages_received;               /**< Total messages received */
    uint64_t creation_time;                         /**< Instance creation time */
};

/* ======================== Helper Functions ========================= */

/**
 * @brief Calculate magnitude of position vector
 */
static inline double position_magnitude(const nimcp_swarm_position_t* pos) {
    return sqrt(pos->x * pos->x + pos->y * pos->y + pos->z * pos->z);
}

/**
 * @brief Calculate velocity magnitude
 */
static inline double velocity_magnitude(const nimcp_swarm_velocity_t* vel) {
    return sqrt(vel->vx * vel->vx + vel->vy * vel->vy + vel->vz * vel->vz);
}

/**
 * @brief Find neighbor by ID
 */
static nimcp_swarm_neighbor_t* find_neighbor(
    nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id
) {
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].drone_id == neighbor_id &&
            proprio->neighbors[i].is_active) {
            return &proprio->neighbors[i];
        }
    }
    return NULL;  /* Not found - normal search miss */
}

/**
 * @brief Add or update neighbor
 */
static nimcp_swarm_neighbor_t* add_or_update_neighbor(
    nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id
) {
    /* Try to find existing neighbor */
    nimcp_swarm_neighbor_t* neighbor = find_neighbor(proprio, neighbor_id);
    if (neighbor) {
        return neighbor;
    }

    /* Find inactive slot or add new */
    for (uint32_t i = 0; i < proprio->config.max_neighbors; i++) {
        if (!proprio->neighbors[i].is_active) {
            proprio->neighbors[i].drone_id = neighbor_id;
            proprio->neighbors[i].is_active = true;
            if (i >= proprio->num_neighbors) {
                proprio->num_neighbors = i + 1;
            }
            return &proprio->neighbors[i];
        }
    }

    LOG_WARN("Maximum neighbors (%u) reached, cannot add neighbor %u",
                   proprio->config.max_neighbors, neighbor_id);
    return NULL;  /* Capacity reached - normal condition */
}

/**
 * @brief Prune stale neighbors
 */
static void prune_stale_neighbors(nimcp_swarm_proprioception_t* proprio) {
    uint64_t current_time = nimcp_get_timestamp_ns();
    const uint64_t timeout_ns = 5000000000ULL; /* 5 seconds */

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            uint64_t age = current_time - proprio->neighbors[i].last_update_time;
            if (age > timeout_ns) {
                LOG_DEBUG("Pruning stale neighbor %u (age: %llu ns)",
                               proprio->neighbors[i].drone_id, age);
                proprio->neighbors[i].is_active = false;
            }
        }
    }
}

/**
 * @brief Calculate principal components for shape analysis
 */
static void calculate_principal_components(
    const nimcp_swarm_proprioception_t* proprio,
    double principal_axes[3],
    double orientation[3]
) {
    /* Build covariance matrix from neighbor positions */
    double cov[9] = {0}; /* 3x3 covariance matrix */
    double mean[3] = {0};
    uint32_t count = 0;

    /* Calculate mean */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            mean[0] += proprio->neighbors[i].relative_pos.x;
            mean[1] += proprio->neighbors[i].relative_pos.y;
            mean[2] += proprio->neighbors[i].relative_pos.z;
            count++;
        }
    }

    if (count == 0) {
        principal_axes[0] = principal_axes[1] = principal_axes[2] = 0.0;
        orientation[0] = orientation[1] = orientation[2] = 0.0;
        return;
    }

    mean[0] /= count;
    mean[1] /= count;
    mean[2] /= count;

    /* Calculate covariance */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double dx = proprio->neighbors[i].relative_pos.x - mean[0];
            double dy = proprio->neighbors[i].relative_pos.y - mean[1];
            double dz = proprio->neighbors[i].relative_pos.z - mean[2];

            cov[0] += dx * dx; cov[1] += dx * dy; cov[2] += dx * dz;
            cov[3] += dy * dx; cov[4] += dy * dy; cov[5] += dy * dz;
            cov[6] += dz * dx; cov[7] += dz * dy; cov[8] += dz * dz;
        }
    }

    for (int i = 0; i < 9; i++) {
        cov[i] /= count;
    }

    /* Simplified eigenvalue estimation (trace and determinant) */
    double trace = cov[0] + cov[4] + cov[8];
    principal_axes[0] = trace / 3.0;
    principal_axes[1] = trace / 3.0;
    principal_axes[2] = trace / 3.0;

    /* Simple orientation from largest variance direction */
    orientation[0] = atan2(cov[1], cov[0]);
    orientation[1] = atan2(cov[5], cov[4]);
    orientation[2] = atan2(cov[6], cov[8]);
}

/**
 * @brief Classify shape based on neighbor distribution
 */
static nimcp_swarm_shape_t classify_shape_internal(
    const nimcp_swarm_proprioception_t* proprio,
    double* fitness_out
) {
    if (proprio->num_neighbors < 2) {
        *fitness_out = 0.0;
        return NIMCP_SWARM_SHAPE_DISPERSED;  /* No neighbors = dispersed/isolated */
    }

    double principal_axes[3];
    double orientation[3];
    calculate_principal_components(proprio, principal_axes, orientation);

    /* Calculate shape metrics */
    double max_axis = fmax(principal_axes[0], fmax(principal_axes[1], principal_axes[2]));
    double min_axis = fmin(principal_axes[0], fmin(principal_axes[1], principal_axes[2]));

    if (max_axis < 1e-9) {
        *fitness_out = 0.0;
        return NIMCP_SWARM_SHAPE_CLUSTER;
    }

    double ratio = min_axis / max_axis;

    /* Classify based on axis ratios */
    if (ratio > 0.8) {
        /* Nearly spherical */
        *fitness_out = ratio;
        return NIMCP_SWARM_SHAPE_SPHERE;
    } else if (ratio > 0.5) {
        /* Ellipsoidal */
        *fitness_out = ratio;
        return NIMCP_SWARM_SHAPE_ELLIPSOID;
    } else if (ratio < 0.2) {
        /* Linear */
        *fitness_out = 1.0 - ratio;
        return NIMCP_SWARM_SHAPE_LINE;
    } else {
        /* Check for planar formation */
        double mid_axis = principal_axes[0] + principal_axes[1] + principal_axes[2]
                         - max_axis - min_axis;
        double planar_ratio = min_axis / mid_axis;

        if (planar_ratio < 0.3) {
            *fitness_out = 1.0 - planar_ratio;
            return NIMCP_SWARM_SHAPE_WALL;
        }
    }

    *fitness_out = 0.5;
    return NIMCP_SWARM_SHAPE_CLUSTER;
}

/**
 * @brief Calculate formation quality metrics
 */
static void calculate_formation_metrics_internal(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_formation_metrics_t* metrics
) {
    memset(metrics, 0, sizeof(*metrics));

    if (proprio->num_neighbors == 0) {
        return;
    }

    double sum_dist = 0.0;
    double sum_sq_dist = 0.0;
    metrics->min_neighbor_distance = DBL_MAX;
    metrics->max_neighbor_distance = 0.0;
    uint32_t active_count = 0;

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double dist = proprio->neighbors[i].distance;
            sum_dist += dist;
            sum_sq_dist += dist * dist;

            if (dist < metrics->min_neighbor_distance) {
                metrics->min_neighbor_distance = dist;
            }
            if (dist > metrics->max_neighbor_distance) {
                metrics->max_neighbor_distance = dist;
            }

            active_count++;
        }
    }

    if (active_count > 0) {
        metrics->avg_neighbor_distance = sum_dist / active_count;
        metrics->distance_variance = (sum_sq_dist / active_count) -
                                    (metrics->avg_neighbor_distance * metrics->avg_neighbor_distance);
        metrics->active_connections = active_count;

        /* Calculate connectivity (normalized by max possible) */
        metrics->connectivity = (double)active_count / proprio->config.max_neighbors;

        /* Formation quality based on distance uniformity */
        if (metrics->avg_neighbor_distance > 0.0) {
            double cv = sqrt(metrics->distance_variance) / metrics->avg_neighbor_distance;
            metrics->formation_quality = 1.0 / (1.0 + cv);
        } else {
            metrics->formation_quality = 0.0;
        }
    }
}

/**
 * @brief Detect deformation from reference formation
 */
static bool detect_deformation_internal(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_deformation_metrics_t* metrics
) {
    if (!proprio->has_reference || proprio->num_neighbors < 2) {
        return false;  /* No reference or too few neighbors - normal condition */
    }

    memset(metrics, 0, sizeof(*metrics));

    double max_strain = 0.0;
    double total_strain = 0.0;
    uint32_t strain_count = 0;

    /* Calculate strain for each neighbor */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active && proprio->reference_distances[i] > 0.0) {
            double current_dist = proprio->neighbors[i].distance;
            double ref_dist = proprio->reference_distances[i];
            double strain = fabs(current_dist - ref_dist) / ref_dist;

            total_strain += strain;
            strain_count++;

            if (strain > max_strain) {
                max_strain = strain;
            }

            /* Build strain tensor (simplified) */
            if (strain_count <= 3) {
                metrics->strain_tensor[strain_count * 3] = strain;
            }
        }
    }

    if (strain_count == 0) {
        return false;  /* No valid strain data - normal condition */
    }

    double avg_strain = total_strain / strain_count;
    metrics->magnitude = avg_strain;

    /* Classify deformation type */
    if (max_strain < proprio->config.deformation_threshold) {
        metrics->deform_type = NIMCP_SWARM_DEFORM_NONE;
        return false;  /* No significant deformation - normal condition */
    }

    /* Determine deformation type based on strain pattern */
    double stretch_count = 0;
    double compress_count = 0;

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active && proprio->reference_distances[i] > 0.0) {
            double current_dist = proprio->neighbors[i].distance;
            double ref_dist = proprio->reference_distances[i];

            if (current_dist > ref_dist * 1.1) {
                stretch_count++;
            } else if (current_dist < ref_dist * 0.9) {
                compress_count++;
            }
        }
    }

    if (stretch_count > compress_count * 2) {
        metrics->deform_type = NIMCP_SWARM_DEFORM_STRETCH;
    } else if (compress_count > stretch_count * 2) {
        metrics->deform_type = NIMCP_SWARM_DEFORM_COMPRESS;
    } else if (stretch_count > 0 && compress_count > 0) {
        metrics->deform_type = NIMCP_SWARM_DEFORM_SHEAR;
    } else {
        metrics->deform_type = NIMCP_SWARM_DEFORM_ASYMMETRIC;
    }

    metrics->detection_time = nimcp_get_timestamp_ns();
    metrics->recovery_estimate = avg_strain * 5.0; /* Heuristic */

    return true;
}

/**
 * @brief Calculate local density
 */
static void calculate_density_internal(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_density_info_t* density
) {
    memset(density, 0, sizeof(*density));

    if (proprio->num_neighbors == 0) {
        return;
    }

    double kernel_width = proprio->config.density_kernel_width;
    double total_weight = 0.0;

    /* Gaussian kernel density estimation */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double dist = proprio->neighbors[i].distance;
            double weight = exp(-0.5 * (dist * dist) / (kernel_width * kernel_width));
            total_weight += weight;
        }
    }

    /* Density in drones per cubic meter */
    double volume = (4.0 / 3.0) * M_PI * kernel_width * kernel_width * kernel_width;
    density->local_density = total_weight / volume;

    /* Calculate density gradient */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double dist = proprio->neighbors[i].distance;
            if (dist > 1e-9) {
                double weight = exp(-0.5 * (dist * dist) / (kernel_width * kernel_width));
                double gradient_factor = -weight * dist / (kernel_width * kernel_width);

                density->gradient[0] += gradient_factor * proprio->neighbors[i].relative_pos.x / dist;
                density->gradient[1] += gradient_factor * proprio->neighbors[i].relative_pos.y / dist;
                density->gradient[2] += gradient_factor * proprio->neighbors[i].relative_pos.z / dist;
            }
        }
    }

    /* Calculate uniformity */
    double variance = 0.0;
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double dist = proprio->neighbors[i].distance;
            double weight = exp(-0.5 * (dist * dist) / (kernel_width * kernel_width));
            variance += (weight - total_weight / proprio->num_neighbors) *
                       (weight - total_weight / proprio->num_neighbors);
        }
    }
    variance /= proprio->num_neighbors;
    density->uniformity = 1.0 / (1.0 + sqrt(variance));
}

/* ======================== Public API Implementation ================ */

nimcp_swarm_proprioception_t* nimcp_swarm_proprioception_create(
    uint32_t drone_id,
    const nimcp_swarm_proprio_config_t* config,
    void* bio_ctx
) {
    if (!config) {
        LOG_ERROR("NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "NULL configuration provided in proprioception_create");
        return NULL;
    }

    nimcp_swarm_proprioception_t* proprio =
        (nimcp_swarm_proprioception_t*)nimcp_malloc(sizeof(nimcp_swarm_proprioception_t));

    if (!proprio) {
        LOG_ERROR("Failed to allocate proprioception instance");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate proprioception instance for drone %u", drone_id);
        return NULL;
    }

    memset(proprio, 0, sizeof(*proprio));

    proprio->drone_id = drone_id;
    proprio->config = *config;
    proprio->bio_ctx = bio_ctx;
    proprio->creation_time = nimcp_get_timestamp_ns();

    /* Initialize history */
    for (uint32_t i = 0; i < NIMCP_SWARM_PROPRIO_HISTORY_SIZE; i++) {
        proprio->history[i].is_valid = false;
    }

    LOG_INFO("Created proprioception system for drone %u", drone_id);

    return proprio;
}

void nimcp_swarm_proprioception_destroy(nimcp_swarm_proprioception_t* proprio) {
    if (!proprio) {
        return;
    }

    LOG_INFO("Destroying proprioception system for drone %u (updates: %llu, msgs sent: %llu, msgs received: %llu)",
                   proprio->drone_id, proprio->total_updates,
                   proprio->total_messages_sent, proprio->total_messages_received);

    nimcp_free(proprio);
}

nimcp_result_t nimcp_swarm_proprioception_reset(nimcp_swarm_proprioception_t* proprio) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");

    /* Reset all state except configuration */
    memset(&proprio->position, 0, sizeof(proprio->position));
    memset(&proprio->velocity, 0, sizeof(proprio->velocity));

    for (uint32_t i = 0; i < NIMCP_SWARM_PROPRIO_HISTORY_SIZE; i++) {
        proprio->history[i].is_valid = false;
    }

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        proprio->neighbors[i].is_active = false;
    }
    proprio->num_neighbors = 0;

    proprio->has_reference = false;
    proprio->history_index = 0;
    proprio->vibration_history_index = 0;

    LOG_INFO("Reset proprioception system for drone %u", proprio->drone_id);

    return NIMCP_SUCCESS;
}

/* ===================== Relative Positioning ======================== */

nimcp_result_t nimcp_swarm_proprio_update_position(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_position_t* position,
    const nimcp_swarm_velocity_t* velocity
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(position, NIMCP_INVALID_PARAM, "position pointer is NULL");

    uint64_t current_time = nimcp_get_timestamp_ns();

    /* Update position */
    proprio->position = *position;
    if (velocity) {
        proprio->velocity = *velocity;
    }

    /* Add to history if enabled */
    if (proprio->config.enable_history) {
        proprio->history[proprio->history_index].position = *position;
        proprio->history[proprio->history_index].timestamp = current_time;
        proprio->history[proprio->history_index].is_valid = true;
        proprio->history_index = (proprio->history_index + 1) % NIMCP_SWARM_PROPRIO_HISTORY_SIZE;
    }

    proprio->last_position_update = current_time;
    proprio->total_updates++;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_update_neighbor(
    nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id,
    const nimcp_swarm_position_t* relative_position,
    double signal_strength
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(relative_position, NIMCP_INVALID_PARAM, "relative_position pointer is NULL");

    if (neighbor_id == proprio->drone_id) {
        LOG_WARN("Cannot add self as neighbor");
        return NIMCP_INVALID_PARAM;
    }

    nimcp_swarm_neighbor_t* neighbor = add_or_update_neighbor(proprio, neighbor_id);
    if (!neighbor) {
        return NIMCP_NO_MEMORY;
    }

    neighbor->relative_pos = *relative_position;
    neighbor->distance = position_magnitude(relative_position);
    neighbor->signal_strength = signal_strength;
    neighbor->last_update_time = nimcp_get_timestamp_ns();

    /* Calculate relative velocity if we have history */
    if (proprio->config.enable_history) {
        /* Simple finite difference */
        neighbor->relative_vel.vx = 0.0;
        neighbor->relative_vel.vy = 0.0;
        neighbor->relative_vel.vz = 0.0;
    }

    proprio->last_neighbor_update = neighbor->last_update_time;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_get_neighbor(
    const nimcp_swarm_proprioception_t* proprio,
    uint32_t neighbor_id,
    nimcp_swarm_neighbor_t* neighbor
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(neighbor, NIMCP_INVALID_PARAM, "neighbor output pointer is NULL");

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].drone_id == neighbor_id &&
            proprio->neighbors[i].is_active) {
            *neighbor = proprio->neighbors[i];
            return NIMCP_SUCCESS;
        }
    }

    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_proprio_get_neighbors(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_neighbor_t* neighbors,
    uint32_t max_neighbors,
    uint32_t* num_neighbors
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(neighbors, NIMCP_INVALID_PARAM, "neighbors output array is NULL");
    NIMCP_CHECK_THROW(num_neighbors, NIMCP_INVALID_PARAM, "num_neighbors output pointer is NULL");

    uint32_t count = 0;
    for (uint32_t i = 0; i < proprio->num_neighbors && count < max_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            neighbors[count++] = proprio->neighbors[i];
        }
    }

    *num_neighbors = count;
    return NIMCP_SUCCESS;
}

/* ==================== Swarm Geometry Sensing ======================= */

nimcp_result_t nimcp_swarm_proprio_classify_shape(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_descriptor_t* descriptor
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(descriptor, NIMCP_INVALID_PARAM, "shape descriptor output pointer is NULL");

    prune_stale_neighbors(proprio);

    memset(descriptor, 0, sizeof(*descriptor));

    double fitness;
    descriptor->shape_type = classify_shape_internal(proprio, &fitness);
    descriptor->fitness = fitness;

    /* Calculate additional metrics */
    calculate_principal_components(proprio, descriptor->principal_axes,
                                  descriptor->orientation);

    /* Calculate symmetry */
    if (proprio->num_neighbors > 0) {
        double symmetry_score = 0.0;
        uint32_t pairs = 0;

        for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
            if (!proprio->neighbors[i].is_active) continue;

            for (uint32_t j = i + 1; j < proprio->num_neighbors; j++) {
                if (!proprio->neighbors[j].is_active) continue;

                double dist_ratio = proprio->neighbors[i].distance /
                                  (proprio->neighbors[j].distance + 1e-9);
                symmetry_score += 1.0 / (1.0 + fabs(1.0 - dist_ratio));
                pairs++;
            }
        }

        descriptor->symmetry = pairs > 0 ? symmetry_score / pairs : 0.0;
    }

    /* Calculate compactness */
    double max_dist = 0.0;
    double sum_dist = 0.0;
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            sum_dist += proprio->neighbors[i].distance;
            if (proprio->neighbors[i].distance > max_dist) {
                max_dist = proprio->neighbors[i].distance;
            }
        }
    }

    if (proprio->num_neighbors > 0 && max_dist > 0.0) {
        double avg_dist = sum_dist / proprio->num_neighbors;
        descriptor->compactness = avg_dist / max_dist;
    }

    /* Calculate aspect ratio */
    if (descriptor->principal_axes[0] > 1e-9) {
        descriptor->aspect_ratio = descriptor->principal_axes[2] /
                                  descriptor->principal_axes[0];
    }

    descriptor->classification_time = nimcp_get_timestamp_ns();
    proprio->current_shape = *descriptor;
    proprio->last_shape_classification = descriptor->classification_time;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_shape_fitness(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_t target_shape,
    double* fitness
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(fitness, NIMCP_INVALID_PARAM, "fitness output pointer is NULL");

    /* Use cached shape if recent */
    uint64_t current_time = nimcp_get_timestamp_ns();
    uint64_t age = current_time - proprio->last_shape_classification;

    if (age < 1000000000ULL) { /* 1 second cache */
        if (proprio->current_shape.shape_type == target_shape) {
            *fitness = proprio->current_shape.fitness;
            return NIMCP_SUCCESS;
        }
    }

    /* Otherwise return low fitness for non-matching shape */
    *fitness = (proprio->current_shape.shape_type == target_shape) ?
               proprio->current_shape.fitness : 0.0;

    return NIMCP_SUCCESS;
}

/* ==================== Deformation Detection ======================== */

nimcp_result_t nimcp_swarm_proprio_detect_deformation(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_deformation_metrics_t* metrics
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(metrics, NIMCP_INVALID_PARAM, "deformation metrics output pointer is NULL");

    prune_stale_neighbors(proprio);

    /* Set reference if we don't have one */
    if (!proprio->has_reference && proprio->num_neighbors >= 2) {
        for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
            proprio->reference_distances[i] = proprio->neighbors[i].distance;
        }
        proprio->has_reference = true;
        LOG_DEBUG("Set reference formation for drone %u", proprio->drone_id);
    }

    if (detect_deformation_internal(proprio, metrics)) {
        proprio->deformation = *metrics;
        return NIMCP_SUCCESS;
    }

    return NIMCP_NOT_FOUND;
}

nimcp_result_t nimcp_swarm_proprio_shape_deviation(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_shape_t target_shape,
    double* deviation
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(deviation, NIMCP_INVALID_PARAM, "deviation output pointer is NULL");

    double fitness;
    nimcp_swarm_proprio_shape_fitness(proprio, target_shape, &fitness);

    *deviation = 1.0 - fitness;

    return NIMCP_SUCCESS;
}

/* ===================== Boundary Awareness ========================== */

nimcp_result_t nimcp_swarm_proprio_boundary_role(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_boundary_descriptor_t* descriptor
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(descriptor, NIMCP_INVALID_PARAM, "boundary descriptor output pointer is NULL");

    prune_stale_neighbors(proprio);

    memset(descriptor, 0, sizeof(*descriptor));

    if (proprio->num_neighbors == 0) {
        descriptor->role = NIMCP_SWARM_ROLE_ISOLATED;
        return NIMCP_SUCCESS;
    }

    if (proprio->num_neighbors < 2) {
        descriptor->role = NIMCP_SWARM_ROLE_EDGE;
        return NIMCP_SUCCESS;
    }

    /* Calculate angular coverage of neighbors */
    double total_angle = 0.0;
    double max_gap = 0.0;

    /* Simple heuristic: check if neighbors surround us uniformly */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (!proprio->neighbors[i].is_active) continue;

        for (uint32_t j = i + 1; j < proprio->num_neighbors; j++) {
            if (!proprio->neighbors[j].is_active) continue;

            /* Calculate angle between neighbors */
            nimcp_swarm_position_t pos_i = proprio->neighbors[i].relative_pos;
            nimcp_swarm_position_t pos_j = proprio->neighbors[j].relative_pos;

            double dot = nimcp_swarm_position_dot(&pos_i, &pos_j);
            double mag_i = position_magnitude(&pos_i);
            double mag_j = position_magnitude(&pos_j);

            if (mag_i > 1e-9 && mag_j > 1e-9) {
                double cos_angle = dot / (mag_i * mag_j);
                cos_angle = fmax(-1.0, fmin(1.0, cos_angle));
                double angle = acos(cos_angle);

                if (angle > max_gap) {
                    max_gap = angle;
                }
            }
        }
    }

    /* If large angular gap, we're on the boundary */
    if (max_gap > M_PI * 0.6) {
        descriptor->role = NIMCP_SWARM_ROLE_EDGE;
        descriptor->boundary_neighbors = proprio->num_neighbors;
    } else if (max_gap > M_PI * 0.4) {
        descriptor->role = NIMCP_SWARM_ROLE_VERTEX;
    } else {
        descriptor->role = NIMCP_SWARM_ROLE_INTERIOR;
    }

    /* Calculate distance to boundary (heuristic) */
    descriptor->distance_to_boundary = proprio->formation.max_neighbor_distance * 0.5;

    proprio->boundary = *descriptor;
    proprio->last_boundary_update = nimcp_get_timestamp_ns();

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_get_boundary_drones(
    const nimcp_swarm_proprioception_t* proprio,
    uint32_t* boundary_ids,
    uint32_t max_ids,
    uint32_t* num_ids
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(boundary_ids, NIMCP_INVALID_PARAM, "boundary_ids output array is NULL");
    NIMCP_CHECK_THROW(num_ids, NIMCP_INVALID_PARAM, "num_ids output pointer is NULL");

    /* This requires information from other drones - for now return neighbors */
    uint32_t count = 0;
    for (uint32_t i = 0; i < proprio->num_neighbors && count < max_ids; i++) {
        if (proprio->neighbors[i].is_active) {
            boundary_ids[count++] = proprio->neighbors[i].drone_id;
        }
    }

    *num_ids = count;
    return NIMCP_SUCCESS;
}

/* ======================= Density Mapping =========================== */

nimcp_result_t nimcp_swarm_proprio_density(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_density_info_t* density_info
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(density_info, NIMCP_INVALID_PARAM, "density_info output pointer is NULL");

    prune_stale_neighbors(proprio);
    calculate_density_internal(proprio, density_info);

    proprio->density = *density_info;
    proprio->last_density_update = nimcp_get_timestamp_ns();

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_density_regions(
    const nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_position_t* sparse_direction,
    nimcp_swarm_position_t* dense_direction
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(sparse_direction, NIMCP_INVALID_PARAM, "sparse_direction output pointer is NULL");
    NIMCP_CHECK_THROW(dense_direction, NIMCP_INVALID_PARAM, "dense_direction output pointer is NULL");

    /* Gradient points toward increasing density */
    dense_direction->x = proprio->density.gradient[0];
    dense_direction->y = proprio->density.gradient[1];
    dense_direction->z = proprio->density.gradient[2];

    /* Opposite direction is toward sparse region */
    sparse_direction->x = -proprio->density.gradient[0];
    sparse_direction->y = -proprio->density.gradient[1];
    sparse_direction->z = -proprio->density.gradient[2];

    return NIMCP_SUCCESS;
}

/* =================== Center-of-Mass Tracking ======================= */

nimcp_result_t nimcp_swarm_proprio_estimate_com(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_com_estimate_t* com_estimate
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(com_estimate, NIMCP_INVALID_PARAM, "com_estimate output pointer is NULL");

    prune_stale_neighbors(proprio);

    memset(com_estimate, 0, sizeof(*com_estimate));

    /* Estimate COM from local neighborhood */
    double sum_x = proprio->position.x;
    double sum_y = proprio->position.y;
    double sum_z = proprio->position.z;
    double sum_vx = proprio->velocity.vx;
    double sum_vy = proprio->velocity.vy;
    double sum_vz = proprio->velocity.vz;
    uint32_t count = 1;

    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            sum_x += proprio->position.x + proprio->neighbors[i].relative_pos.x;
            sum_y += proprio->position.y + proprio->neighbors[i].relative_pos.y;
            sum_z += proprio->position.z + proprio->neighbors[i].relative_pos.z;
            sum_vx += proprio->velocity.vx + proprio->neighbors[i].relative_vel.vx;
            sum_vy += proprio->velocity.vy + proprio->neighbors[i].relative_vel.vy;
            sum_vz += proprio->velocity.vz + proprio->neighbors[i].relative_vel.vz;
            count++;
        }
    }

    com_estimate->position.x = sum_x / count;
    com_estimate->position.y = sum_y / count;
    com_estimate->position.z = sum_z / count;
    com_estimate->velocity.vx = sum_vx / count;
    com_estimate->velocity.vy = sum_vy / count;
    com_estimate->velocity.vz = sum_vz / count;
    com_estimate->contributing_drones = count;
    com_estimate->confidence = (double)count / (proprio->config.max_neighbors + 1);
    com_estimate->update_time = nimcp_get_timestamp_ns();

    proprio->com_estimate = *com_estimate;
    proprio->last_com_update = com_estimate->update_time;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_merge_com_estimate(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_com_estimate_t* neighbor_com
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(neighbor_com, NIMCP_INVALID_PARAM, "neighbor_com pointer is NULL");

    /* Weighted average based on confidence and contributing drones */
    double w1 = proprio->com_estimate.confidence * proprio->com_estimate.contributing_drones;
    double w2 = neighbor_com->confidence * neighbor_com->contributing_drones;
    double total_w = w1 + w2;

    if (total_w < 1e-9) {
        return NIMCP_SUCCESS;
    }

    proprio->com_estimate.position.x = (w1 * proprio->com_estimate.position.x +
                                        w2 * neighbor_com->position.x) / total_w;
    proprio->com_estimate.position.y = (w1 * proprio->com_estimate.position.y +
                                        w2 * neighbor_com->position.y) / total_w;
    proprio->com_estimate.position.z = (w1 * proprio->com_estimate.position.z +
                                        w2 * neighbor_com->position.z) / total_w;

    proprio->com_estimate.velocity.vx = (w1 * proprio->com_estimate.velocity.vx +
                                         w2 * neighbor_com->velocity.vx) / total_w;
    proprio->com_estimate.velocity.vy = (w1 * proprio->com_estimate.velocity.vy +
                                         w2 * neighbor_com->velocity.vy) / total_w;
    proprio->com_estimate.velocity.vz = (w1 * proprio->com_estimate.velocity.vz +
                                         w2 * neighbor_com->velocity.vz) / total_w;

    proprio->com_estimate.contributing_drones += neighbor_com->contributing_drones;
    proprio->com_estimate.confidence = (w1 + w2) / (proprio->com_estimate.contributing_drones * 2);
    proprio->com_estimate.update_time = nimcp_get_timestamp_ns();

    return NIMCP_SUCCESS;
}

/* ===================== Formation Metrics =========================== */

nimcp_result_t nimcp_swarm_proprio_formation_metrics(
    nimcp_swarm_proprioception_t* proprio,
    nimcp_swarm_formation_metrics_t* metrics
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(metrics, NIMCP_INVALID_PARAM, "formation metrics output pointer is NULL");

    prune_stale_neighbors(proprio);
    calculate_formation_metrics_internal(proprio, metrics);

    proprio->formation = *metrics;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_connectivity_graph(
    const nimcp_swarm_proprioception_t* proprio,
    bool* adjacency_matrix,
    uint32_t matrix_size
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(adjacency_matrix, NIMCP_INVALID_PARAM, "adjacency_matrix output pointer is NULL");

    /* Initialize matrix to false */
    memset(adjacency_matrix, 0, matrix_size * matrix_size * sizeof(bool));

    /* Fill in connections */
    for (uint32_t i = 0; i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            uint32_t neighbor_id = proprio->neighbors[i].drone_id;
            if (neighbor_id < matrix_size && proprio->drone_id < matrix_size) {
                adjacency_matrix[proprio->drone_id * matrix_size + neighbor_id] = true;
                adjacency_matrix[neighbor_id * matrix_size + proprio->drone_id] = true;
            }
        }
    }

    return NIMCP_SUCCESS;
}

/* ===================== Vibration Sensing =========================== */

nimcp_result_t nimcp_swarm_proprio_detect_vibration(
    nimcp_swarm_proprioception_t* proprio,
    const double* signal,
    uint32_t signal_length,
    nimcp_swarm_vibration_data_t* vibration_data
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(signal, NIMCP_INVALID_PARAM, "signal input array is NULL");
    NIMCP_CHECK_THROW(vibration_data, NIMCP_INVALID_PARAM, "vibration_data output pointer is NULL");
    NIMCP_CHECK_THROW(signal_length > 0, NIMCP_INVALID_PARAM, "signal_length must be greater than 0");

    if (!proprio->config.enable_vibration) {
        return NIMCP_ERROR;
    }

    memset(vibration_data, 0, sizeof(*vibration_data));

    /* Calculate total energy */
    double total_energy = 0.0;
    for (uint32_t i = 0; i < signal_length; i++) {
        total_energy += signal[i] * signal[i];
    }
    total_energy /= signal_length;

    vibration_data->total_energy = total_energy;

    /* Simple frequency detection using autocorrelation */
    if (signal_length >= 4) {
        double max_corr = 0.0;
        uint32_t best_lag = 0;

        for (uint32_t lag = 1; lag < signal_length / 2; lag++) {
            double corr = 0.0;
            for (uint32_t i = 0; i < signal_length - lag; i++) {
                corr += signal[i] * signal[i + lag];
            }
            corr /= (signal_length - lag);

            if (corr > max_corr) {
                max_corr = corr;
                best_lag = lag;
            }
        }

        if (best_lag > 0 && max_corr > proprio->config.vibration_sensitivity) {
            vibration_data->frequencies[0].frequency =
                proprio->config.position_update_rate / best_lag;
            vibration_data->frequencies[0].amplitude = sqrt(total_energy);
            vibration_data->frequencies[0].phase = 0.0;
            vibration_data->frequencies[0].is_active = true;
            vibration_data->num_frequencies = 1;
        }
    }

    vibration_data->detection_time = nimcp_get_timestamp_ns();
    vibration_data->propagation_speed = 343.0; /* Speed of sound in air */

    proprio->vibration = *vibration_data;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_swarm_proprio_localize_vibration(
    nimcp_swarm_proprioception_t* proprio,
    const uint64_t* arrival_times,
    uint32_t num_neighbors,
    nimcp_swarm_position_t* source_position
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(arrival_times, NIMCP_INVALID_PARAM, "arrival_times input array is NULL");
    NIMCP_CHECK_THROW(source_position, NIMCP_INVALID_PARAM, "source_position output pointer is NULL");
    NIMCP_CHECK_THROW(num_neighbors >= 3, NIMCP_INVALID_PARAM, "num_neighbors must be at least 3 for localization");

    /* Multilateration using time differences */
    /* Simplified: use centroid of neighbors weighted by arrival time */

    double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;
    double sum_weights = 0.0;
    uint64_t min_time = arrival_times[0];

    for (uint32_t i = 0; i < num_neighbors; i++) {
        if (arrival_times[i] < min_time) {
            min_time = arrival_times[i];
        }
    }

    for (uint32_t i = 0; i < num_neighbors && i < proprio->num_neighbors; i++) {
        if (proprio->neighbors[i].is_active) {
            double time_diff = (arrival_times[i] - min_time) * 1e-9; /* ns to s */
            double weight = 1.0 / (1.0 + time_diff);

            sum_x += proprio->neighbors[i].relative_pos.x * weight;
            sum_y += proprio->neighbors[i].relative_pos.y * weight;
            sum_z += proprio->neighbors[i].relative_pos.z * weight;
            sum_weights += weight;
        }
    }

    if (sum_weights > 1e-9) {
        source_position->x = sum_x / sum_weights;
        source_position->y = sum_y / sum_weights;
        source_position->z = sum_z / sum_weights;
        return NIMCP_SUCCESS;
    }

    return NIMCP_NOT_FOUND;
}

/* ==================== Bio-Async Integration ======================== */

/**
 * @brief Helper to send bio-async message
 */
static nimcp_result_t proprio_send_message(
    nimcp_swarm_proprioception_t* proprio,
    bio_message_type_t msg_type,
    const void* payload,
    size_t payload_size
) {
    if (!bio_router_is_initialized() || !proprio->bio_ctx) {
        return NIMCP_SUCCESS;  /* Silently succeed if not available */
    }

    size_t total_size = sizeof(bio_message_header_t) + payload_size;
    uint8_t* buffer = nimcp_malloc(total_size);
    if (!buffer) {
        return NIMCP_ERROR_MEMORY;
    }

    bio_message_header_t* header = (bio_message_header_t*)buffer;
    header->type = msg_type;
    header->sequence_id = 0;
    header->source_module = BIO_MODULE_SWARM_PROPRIOCEPTION;
    header->target_module = BIO_MODULE_ALL;
    header->timestamp_us = nimcp_time_get_us();
    header->channel = BIO_CHANNEL_ACETYLCHOLINE;  /* Fast coordination */
    header->payload_size = (uint32_t)payload_size;
    header->flags = BIO_MSG_FLAG_BROADCAST;

    if (payload && payload_size > 0) {
        memcpy(buffer + sizeof(bio_message_header_t), payload, payload_size);
    }

    nimcp_error_t err = bio_router_broadcast(
        (bio_module_context_t)proprio->bio_ctx, buffer, total_size
    );
    nimcp_free(buffer);

    return (err == NIMCP_SUCCESS) ? NIMCP_SUCCESS : NIMCP_ERROR;
}

nimcp_result_t nimcp_swarm_proprio_broadcast_position(
    nimcp_swarm_proprioception_t* proprio
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");

    if (!proprio->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not enabled, return success */
    }

    /* Build position payload */
    struct {
        uint32_t drone_id;
        nimcp_swarm_position_t position;
        nimcp_swarm_velocity_t velocity;
        uint64_t timestamp;
    } payload = {
        .drone_id = proprio->drone_id,
        .position = proprio->position,
        .velocity = proprio->velocity,
        .timestamp = nimcp_time_get_us()
    };

    nimcp_result_t result = proprio_send_message(
        proprio, BIO_MSG_SWARM_POSITION_UPDATE, &payload, sizeof(payload)
    );

    if (result == NIMCP_SUCCESS) {
        proprio->total_messages_sent++;
        LOG_DEBUG("Drone %u broadcast position (%.2f, %.2f, %.2f)",
                  proprio->drone_id,
                  proprio->position.x, proprio->position.y, proprio->position.z);
    }

    return result;
}

nimcp_result_t nimcp_swarm_proprio_broadcast_formation_state(
    nimcp_swarm_proprioception_t* proprio
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");

    if (!proprio->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not enabled, return success */
    }

    /* Build formation state payload */
    struct {
        uint32_t drone_id;
        uint32_t num_neighbors;
        nimcp_swarm_shape_descriptor_t shape;
        nimcp_swarm_density_info_t density;
        uint64_t timestamp;
    } payload = {
        .drone_id = proprio->drone_id,
        .num_neighbors = proprio->num_neighbors,
        .shape = proprio->current_shape,
        .density = proprio->density,
        .timestamp = nimcp_time_get_us()
    };

    nimcp_result_t result = proprio_send_message(
        proprio, BIO_MSG_SWARM_FORMATION_STATE, &payload, sizeof(payload)
    );

    if (result == NIMCP_SUCCESS) {
        proprio->total_messages_sent++;
        LOG_DEBUG("Drone %u broadcast formation state: neighbors=%u, local_density=%.3f",
                  proprio->drone_id, proprio->num_neighbors, proprio->density.local_density);
    }

    return result;
}

nimcp_result_t nimcp_swarm_proprio_send_deformation_alert(
    nimcp_swarm_proprioception_t* proprio,
    const nimcp_swarm_deformation_metrics_t* metrics
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(metrics, NIMCP_INVALID_PARAM, "deformation metrics pointer is NULL");

    if (!proprio->bio_ctx) {
        return NIMCP_SUCCESS;  /* Not enabled, return success */
    }

    /* Build deformation alert payload */
    struct {
        uint32_t drone_id;
        nimcp_swarm_deformation_metrics_t metrics;
        uint64_t timestamp;
    } payload = {
        .drone_id = proprio->drone_id,
        .metrics = *metrics,
        .timestamp = nimcp_time_get_us()
    };

    /* Use NOREPINEPHRINE channel for alerts */
    nimcp_result_t result = NIMCP_SUCCESS;
    if (bio_router_is_initialized()) {
        size_t total_size = sizeof(bio_message_header_t) + sizeof(payload);
        uint8_t* buffer = nimcp_malloc(total_size);
        if (buffer) {
            bio_message_header_t* header = (bio_message_header_t*)buffer;
            header->type = BIO_MSG_SWARM_DEFORMATION_ALERT;
            header->source_module = BIO_MODULE_SWARM_PROPRIOCEPTION;
            header->target_module = BIO_MODULE_ALL;
            header->timestamp_us = nimcp_time_get_us();
            header->channel = BIO_CHANNEL_NOREPINEPHRINE;  /* Alert channel */
            header->payload_size = sizeof(payload);
            header->flags = BIO_MSG_FLAG_BROADCAST | BIO_MSG_FLAG_URGENT;
            memcpy(buffer + sizeof(bio_message_header_t), &payload, sizeof(payload));

            bio_router_broadcast((bio_module_context_t)proprio->bio_ctx, buffer, total_size);
            nimcp_free(buffer);
        }
    }

    proprio->total_messages_sent++;
    LOG_WARN("Drone %u sending deformation alert: type=%d, magnitude=%.3f",
             proprio->drone_id, metrics->deform_type, metrics->magnitude);

    return result;
}

nimcp_result_t nimcp_swarm_proprio_process_message(
    nimcp_swarm_proprioception_t* proprio,
    const bio_message_header_t* msg
) {
    NIMCP_CHECK_THROW(proprio, NIMCP_INVALID_PARAM, "proprioception instance is NULL");
    NIMCP_CHECK_THROW(msg, NIMCP_INVALID_PARAM, "message pointer is NULL");

    proprio->total_messages_received++;

    /* Process based on message type */
    switch (msg->type) {
        case BIO_MSG_SWARM_POSITION_UPDATE: {
            /* Update neighbor position if from another drone */
            const uint8_t* payload = (const uint8_t*)(msg + 1);
            uint32_t sender_id = *(const uint32_t*)payload;
            if (sender_id != proprio->drone_id) {
                LOG_DEBUG("Drone %u received position update from drone %u",
                          proprio->drone_id, sender_id);
                /* TODO: Update neighbor tracking */
            }
            break;
        }

        case BIO_MSG_SWARM_FORMATION_STATE:
            LOG_DEBUG("Drone %u received formation state", proprio->drone_id);
            break;

        case BIO_MSG_SWARM_DEFORMATION_ALERT:
            LOG_WARN("Drone %u received deformation alert", proprio->drone_id);
            break;

        default:
            LOG_DEBUG("Drone %u received unknown message type 0x%x",
                      proprio->drone_id, msg->type);
            break;
    }

    return NIMCP_SUCCESS;
}

/* ========================= Utility Functions ======================= */

void nimcp_swarm_proprio_default_config(nimcp_swarm_proprio_config_t* config) {
    if (!config) {
        return;
    }

    config->neighbor_radius = 50.0;                  /* 50 meters */
    config->position_update_rate = 10.0;             /* 10 Hz */
    config->shape_classification_interval = 1.0;     /* 1 second */
    config->deformation_threshold = 0.15;            /* 15% strain */
    config->density_kernel_width = 10.0;             /* 10 meters */
    config->vibration_sensitivity = 0.1;
    config->enable_history = true;
    config->enable_vibration = true;
    config->max_neighbors = NIMCP_SWARM_PROPRIO_MAX_NEIGHBORS;
}

const char* nimcp_swarm_shape_name(nimcp_swarm_shape_t shape) {
    static const char* names[] = {
        "Unknown", "Sphere", "Ellipsoid", "Line", "Wedge",
        "Wall", "Ring", "Lattice", "Cluster", "Dispersed"
    };

    if (shape >= 0 && shape < NIMCP_SWARM_SHAPE_COUNT) {
        return names[shape];
    }
    return "Invalid";
}

const char* nimcp_swarm_deformation_name(nimcp_swarm_deformation_t deform) {
    static const char* names[] = {
        "None", "Stretch", "Compress", "Shear",
        "Twist", "Split", "Asymmetric"
    };

    if (deform >= 0 && deform < NIMCP_SWARM_DEFORM_COUNT) {
        return names[deform];
    }
    return "Invalid";
}

double nimcp_swarm_position_distance(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2
) {
    if (!pos1 || !pos2) {
        return 0.0;
    }

    double dx = pos2->x - pos1->x;
    double dy = pos2->y - pos1->y;
    double dz = pos2->z - pos1->z;

    return sqrt(dx * dx + dy * dy + dz * dz);
}

void nimcp_swarm_position_normalize(nimcp_swarm_position_t* pos) {
    if (!pos) {
        return;
    }

    double mag = position_magnitude(pos);
    if (mag > 1e-9) {
        pos->x /= mag;
        pos->y /= mag;
        pos->z /= mag;
    }
}

double nimcp_swarm_position_dot(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2
) {
    if (!pos1 || !pos2) {
        return 0.0;
    }

    return pos1->x * pos2->x + pos1->y * pos2->y + pos1->z * pos2->z;
}

void nimcp_swarm_position_cross(
    const nimcp_swarm_position_t* pos1,
    const nimcp_swarm_position_t* pos2,
    nimcp_swarm_position_t* result
) {
    if (!pos1 || !pos2 || !result) {
        return;
    }

    result->x = pos1->y * pos2->z - pos1->z * pos2->y;
    result->y = pos1->z * pos2->x - pos1->x * pos2->z;
    result->z = pos1->x * pos2->y - pos1->y * pos2->x;
}
