/**
 * @file nimcp_body_ownership.c
 * @brief Implementation of Body Schema and Ownership
 *
 * This implementation provides body schema representation and ownership
 * mechanisms including rubber hand illusion-like plasticity.
 *
 * Biological basis:
 * - Parietal cortex integrates multimodal body signals
 * - Ownership emerges from visual-proprioceptive-tactile congruence
 * - Tool use extends body schema through neural plasticity
 */

#include "embodiment/nimcp_body_ownership.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/geometry/nimcp_lie_group.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define LOG_MODULE "body_ownership"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(body_ownership)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Internal body ownership context
 */
struct nimcp_body_context {
    nimcp_body_config_t config;       /**< Configuration */
    bool initialized;                  /**< Initialization flag */

    /* Body parts */
    nimcp_body_part_t parts[NIMCP_BODY_MAX_PARTS];
    bool part_active[NIMCP_BODY_MAX_PARTS];
    uint32_t num_parts;

    /* External objects */
    nimcp_external_object_t external_objects[NIMCP_BODY_MAX_EXTERNAL_OBJECTS];
    bool object_active[NIMCP_BODY_MAX_EXTERNAL_OBJECTS];
    uint32_t num_external_objects;

    /* Peripersonal space */
    nimcp_peripersonal_space_t peripersonal;

    /* Body center */
    nimcp_body_position_t center_of_mass;
    double total_mass;

    /* Statistics */
    nimcp_body_stats_t stats;

    /* Timing */
    uint64_t last_update_time;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get current timestamp in nanoseconds
 */
static inline uint64_t get_timestamp_ns(void) {
    return nimcp_time_get_us() * 1000ULL;
}

/**
 * @brief Calculate distance between positions
 */
static double position_distance(
    const nimcp_body_position_t* p1,
    const nimcp_body_position_t* p2
) {
    double dx = p2->x - p1->x;
    double dy = p2->y - p1->y;
    double dz = p2->z - p1->z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Normalize quaternion
 */
static void normalize_quaternion(nimcp_body_quaternion_t* q) {
    double mag = sqrt(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
    if (mag > 1e-9) {
        q->w /= mag;
        q->x /= mag;
        q->y /= mag;
        q->z /= mag;
    }
}

/**
 * @brief Find body part by ID
 */
static nimcp_body_part_t* find_part(
    nimcp_body_context_t* ctx,
    uint32_t part_id
) {
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i] && ctx->parts[i].part_id == part_id) {
            return &ctx->parts[i];
        }
    }
    return NULL;
}

/**
 * @brief Find external object by ID
 */
static nimcp_external_object_t* find_external_object(
    nimcp_body_context_t* ctx,
    uint32_t object_id
) {
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_EXTERNAL_OBJECTS; i++) {
        if (ctx->object_active[i] && ctx->external_objects[i].object_id == object_id) {
            return &ctx->external_objects[i];
        }
    }
    return NULL;
}

/**
 * @brief Calculate weighted position integration
 */
static void integrate_position(
    nimcp_body_position_t* result,
    const nimcp_body_position_t* proprio,
    double proprio_weight,
    const nimcp_body_position_t* visual,
    double visual_weight
) {
    double total_weight = proprio_weight + visual_weight;
    if (total_weight < 1e-9) {
        return;
    }

    result->x = (proprio->x * proprio_weight + visual->x * visual_weight) / total_weight;
    result->y = (proprio->y * proprio_weight + visual->y * visual_weight) / total_weight;
    result->z = (proprio->z * proprio_weight + visual->z * visual_weight) / total_weight;
}

/**
 * @brief Update ownership based on multisensory congruence
 */
static void update_ownership_internal(
    nimcp_body_part_t* part,
    double visual_proprio_distance,
    double threshold
) {
    /* Ownership decreases with visual-proprioceptive mismatch */
    double mismatch_factor = visual_proprio_distance / threshold;

    if (mismatch_factor < 0.5) {
        /* Good match - increase ownership */
        part->ownership_confidence += 0.1 * (1.0 - mismatch_factor);
        if (part->ownership_confidence > 0.8) {
            part->ownership_state = NIMCP_OWNERSHIP_FULL;
        } else if (part->ownership_confidence > 0.5) {
            part->ownership_state = NIMCP_OWNERSHIP_PARTIAL;
        }
    } else if (mismatch_factor > 1.0) {
        /* Poor match - decrease ownership */
        part->ownership_confidence -= 0.05 * (mismatch_factor - 1.0);
        if (part->ownership_confidence < 0.3) {
            part->ownership_state = NIMCP_OWNERSHIP_UNCERTAIN;
        }
    }

    part->ownership_confidence = fmax(0.0, fmin(1.0, part->ownership_confidence));
}

/**
 * @brief Update center of mass
 */
static void update_center_of_mass(nimcp_body_context_t* ctx) {
    double total_mass = 0.0;
    double com_x = 0.0, com_y = 0.0, com_z = 0.0;

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i]) {
            total_mass += ctx->parts[i].mass;
            com_x += ctx->parts[i].position.x * ctx->parts[i].mass;
            com_y += ctx->parts[i].position.y * ctx->parts[i].mass;
            com_z += ctx->parts[i].position.z * ctx->parts[i].mass;
        }
    }

    if (total_mass > 1e-9) {
        ctx->center_of_mass.x = com_x / total_mass;
        ctx->center_of_mass.y = com_y / total_mass;
        ctx->center_of_mass.z = com_z / total_mass;
    }
    ctx->total_mass = total_mass;
}

/**
 * @brief Convert position to peripersonal grid indices
 */
static bool position_to_grid(
    const nimcp_body_context_t* ctx,
    const nimcp_body_position_t* pos,
    int* ix, int* iy, int* iz
) {
    double dx = pos->x - ctx->peripersonal.grid_origin[0];
    double dy = pos->y - ctx->peripersonal.grid_origin[1];
    double dz = pos->z - ctx->peripersonal.grid_origin[2];

    *ix = (int)(dx / ctx->peripersonal.grid_resolution);
    *iy = (int)(dy / ctx->peripersonal.grid_resolution);
    *iz = (int)(dz / ctx->peripersonal.grid_resolution);

    return (*ix >= 0 && *ix < NIMCP_BODY_PERIPERSONAL_GRID_SIZE &&
            *iy >= 0 && *iy < NIMCP_BODY_PERIPERSONAL_GRID_SIZE &&
            *iz >= 0 && *iz < NIMCP_BODY_PERIPERSONAL_GRID_SIZE);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

void nimcp_body_default_config(nimcp_body_config_t* config) {
    if (!config) {
        return;
    }

    memset(config, 0, sizeof(*config));

    /* Integration weights */
    config->proprio_weight = 0.6;
    config->visual_weight = 0.3;
    config->tactile_weight = 0.1;

    /* Prediction */
    config->prediction_learning_rate = 0.1;
    config->prediction_error_threshold = 0.1;

    /* Ownership */
    config->ownership_threshold = 0.5;
    config->ownership_decay_rate = 0.05;
    config->sync_window_ms = 200.0;

    /* Body schema */
    config->enable_tool_incorporation = true;
    config->incorporation_threshold = 0.7;
    config->boundary_margin = 0.1;

    /* Peripersonal space */
    config->enable_peripersonal = true;
    config->peripersonal_range = 1.0;

    /* Limits */
    config->max_body_parts = NIMCP_BODY_MAX_PARTS;

    /* Update rate */
    config->update_rate_hz = 60.0;
}

nimcp_body_context_t* nimcp_body_create(const nimcp_body_config_t* config) {
    /* Use defaults if config is NULL */
    nimcp_body_config_t default_config;
    if (!config) {
        nimcp_body_default_config(&default_config);
        config = &default_config;
    }

    nimcp_body_context_t* ctx = nimcp_malloc(sizeof(nimcp_body_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate body context");
        return NULL;
    }

    nimcp_body_error_t err = nimcp_body_init(ctx, config);
    if (err != NIMCP_BODY_OK) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_body_create: validation failed");
        return NULL;
    }

    return ctx;
}

nimcp_body_error_t nimcp_body_init(
    nimcp_body_context_t* ctx,
    const nimcp_body_config_t* config
) {
    if (!ctx || !config) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->config = *config;
    ctx->initialized = true;
    ctx->stats.creation_time = get_timestamp_ns();
    ctx->last_update_time = ctx->stats.creation_time;

    /* Initialize peripersonal space */
    if (config->enable_peripersonal) {
        ctx->peripersonal.grid_resolution = config->peripersonal_range * 2.0 /
                                            NIMCP_BODY_PERIPERSONAL_GRID_SIZE;
        ctx->peripersonal.grid_origin[0] = -config->peripersonal_range;
        ctx->peripersonal.grid_origin[1] = -config->peripersonal_range;
        ctx->peripersonal.grid_origin[2] = -config->peripersonal_range;
        ctx->peripersonal.extent[0] = config->peripersonal_range * 2.0;
        ctx->peripersonal.extent[1] = config->peripersonal_range * 2.0;
        ctx->peripersonal.extent[2] = config->peripersonal_range * 2.0;
    }

    LOG_INFO("Initialized body ownership context");

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_reset(nimcp_body_context_t* ctx) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Clear parts */
    memset(ctx->parts, 0, sizeof(ctx->parts));
    memset(ctx->part_active, 0, sizeof(ctx->part_active));
    ctx->num_parts = 0;

    /* Clear external objects */
    memset(ctx->external_objects, 0, sizeof(ctx->external_objects));
    memset(ctx->object_active, 0, sizeof(ctx->object_active));
    ctx->num_external_objects = 0;

    /* Clear peripersonal space */
    memset(ctx->peripersonal.activation, 0, sizeof(ctx->peripersonal.activation));

    /* Reset center of mass */
    memset(&ctx->center_of_mass, 0, sizeof(ctx->center_of_mass));
    ctx->total_mass = 0.0;

    /* Reset statistics */
    uint64_t creation_time = ctx->stats.creation_time;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.creation_time = creation_time;

    ctx->last_update_time = get_timestamp_ns();

    LOG_INFO("Reset body ownership context");

    return NIMCP_BODY_OK;
}

void nimcp_body_destroy(nimcp_body_context_t* ctx) {
    if (!ctx) {
        return;
    }

    LOG_INFO("Destroying body context (parts: %u, proprio_updates: %llu)",
             ctx->num_parts, (unsigned long long)ctx->stats.total_proprio_updates);

    nimcp_free(ctx);
}

/* ============================================================================
 * Body Part Management
 * ============================================================================ */

nimcp_body_error_t nimcp_body_add_part(
    nimcp_body_context_t* ctx,
    const nimcp_body_part_t* part
) {
    if (!ctx || !part) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Check if part already exists */
    if (find_part(ctx, part->part_id)) {
        return nimcp_body_update_part(ctx, part);
    }

    /* Find free slot */
    int free_slot = -1;
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (!ctx->part_active[i]) {
            free_slot = (int)i;
            break;
        }
    }

    if (free_slot < 0) {
        LOG_WARN("Body part limit reached (%u)", NIMCP_BODY_MAX_PARTS);
        return NIMCP_BODY_ERROR_PART_LIMIT;
    }

    /* Add part */
    ctx->parts[free_slot] = *part;
    ctx->parts[free_slot].creation_time = get_timestamp_ns();
    ctx->parts[free_slot].last_update = ctx->parts[free_slot].creation_time;
    ctx->parts[free_slot].is_active = true;
    ctx->part_active[free_slot] = true;
    ctx->num_parts++;
    ctx->stats.active_parts = ctx->num_parts;

    /* Update center of mass */
    update_center_of_mass(ctx);

    LOG_DEBUG("Added body part %u (%s)",
              part->part_id, nimcp_body_part_type_name(part->type));

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_update_part(
    nimcp_body_context_t* ctx,
    const nimcp_body_part_t* part
) {
    if (!ctx || !part) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_body_part_t* existing = find_part(ctx, part->part_id);
    if (!existing) {
        return nimcp_body_add_part(ctx, part);
    }

    /* Preserve timing info */
    uint64_t creation_time = existing->creation_time;

    *existing = *part;
    existing->creation_time = creation_time;
    existing->last_update = get_timestamp_ns();
    existing->is_active = true;

    /* Update center of mass */
    update_center_of_mass(ctx);

    ctx->stats.schema_updates++;

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_remove_part(
    nimcp_body_context_t* ctx,
    uint32_t part_id
) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i] && ctx->parts[i].part_id == part_id) {
            ctx->part_active[i] = false;
            ctx->parts[i].is_active = false;
            ctx->num_parts--;
            ctx->stats.active_parts = ctx->num_parts;

            update_center_of_mass(ctx);

            LOG_DEBUG("Removed body part %u", part_id);
            return NIMCP_BODY_OK;
        }
    }

    return NIMCP_BODY_ERROR_INVALID_PART;
}

nimcp_body_error_t nimcp_body_get_part(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    nimcp_body_part_t* part
) {
    if (!ctx || !part) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i] && ctx->parts[i].part_id == part_id) {
            *part = ctx->parts[i];
            return NIMCP_BODY_OK;
        }
    }

    return NIMCP_BODY_ERROR_INVALID_PART;
}

nimcp_body_error_t nimcp_body_get_all_parts(
    const nimcp_body_context_t* ctx,
    nimcp_body_part_t* parts,
    uint32_t max_parts,
    uint32_t* num_parts
) {
    if (!ctx || !parts || !num_parts) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *num_parts = 0;
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS && *num_parts < max_parts; i++) {
        if (ctx->part_active[i]) {
            parts[*num_parts] = ctx->parts[i];
            (*num_parts)++;
        }
    }

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_init_human_schema(nimcp_body_context_t* ctx) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Define standard human body parts */
    static const struct {
        uint32_t id;
        nimcp_body_part_type_t type;
        const char* name;
        double pos[3];
        double dim[3];
        double mass;
        uint32_t parent;
    } human_parts[] = {
        {1, NIMCP_BODY_PART_TORSO, "Torso", {0, 0, 1.0}, {0.4, 0.25, 0.5}, 25.0, 0},
        {2, NIMCP_BODY_PART_HEAD, "Head", {0, 0, 1.6}, {0.2, 0.2, 0.25}, 5.0, 1},
        {3, NIMCP_BODY_PART_NECK, "Neck", {0, 0, 1.45}, {0.1, 0.1, 0.1}, 1.0, 1},
        {4, NIMCP_BODY_PART_LEFT_ARM, "Left Arm", {-0.35, 0, 1.2}, {0.08, 0.08, 0.3}, 3.5, 1},
        {5, NIMCP_BODY_PART_RIGHT_ARM, "Right Arm", {0.35, 0, 1.2}, {0.08, 0.08, 0.3}, 3.5, 1},
        {6, NIMCP_BODY_PART_LEFT_HAND, "Left Hand", {-0.45, 0, 0.9}, {0.1, 0.03, 0.18}, 0.5, 4},
        {7, NIMCP_BODY_PART_RIGHT_HAND, "Right Hand", {0.45, 0, 0.9}, {0.1, 0.03, 0.18}, 0.5, 5},
        {8, NIMCP_BODY_PART_PELVIS, "Pelvis", {0, 0, 0.9}, {0.35, 0.2, 0.15}, 8.0, 1},
        {9, NIMCP_BODY_PART_LEFT_LEG, "Left Leg", {-0.15, 0, 0.5}, {0.1, 0.1, 0.45}, 8.0, 8},
        {10, NIMCP_BODY_PART_RIGHT_LEG, "Right Leg", {0.15, 0, 0.5}, {0.1, 0.1, 0.45}, 8.0, 8},
        {11, NIMCP_BODY_PART_LEFT_FOOT, "Left Foot", {-0.15, 0.08, 0.05}, {0.1, 0.25, 0.06}, 1.0, 9},
        {12, NIMCP_BODY_PART_RIGHT_FOOT, "Right Foot", {0.15, 0.08, 0.05}, {0.1, 0.25, 0.06}, 1.0, 10},
    };

    size_t num_human_parts = sizeof(human_parts) / sizeof(human_parts[0]);

    for (size_t i = 0; i < num_human_parts; i++) {
        nimcp_body_part_t part;
        memset(&part, 0, sizeof(part));

        part.part_id = human_parts[i].id;
        part.type = human_parts[i].type;
        strncpy(part.name, human_parts[i].name, sizeof(part.name) - 1);

        part.position.x = human_parts[i].pos[0];
        part.position.y = human_parts[i].pos[1];
        part.position.z = human_parts[i].pos[2];

        part.orientation.w = 1.0;  /* Identity quaternion */

        part.dimensions[0] = human_parts[i].dim[0];
        part.dimensions[1] = human_parts[i].dim[1];
        part.dimensions[2] = human_parts[i].dim[2];

        part.mass = human_parts[i].mass;
        part.parent_part_id = human_parts[i].parent;

        part.ownership_state = NIMCP_OWNERSHIP_FULL;
        part.ownership_confidence = 1.0;
        part.agency = 1.0;

        /* Default sensory weights */
        part.sensory_weights[NIMCP_SENSORY_PROPRIOCEPTION] = 0.6;
        part.sensory_weights[NIMCP_SENSORY_VISION] = 0.3;
        part.sensory_weights[NIMCP_SENSORY_TACTILE] = 0.1;

        nimcp_body_error_t err = nimcp_body_add_part(ctx, &part);
        if (err != NIMCP_BODY_OK) {
            LOG_ERROR("Failed to add body part %s", part.name);
            return err;
        }
    }

    LOG_INFO("Initialized human body schema with %zu parts", num_human_parts);

    return NIMCP_BODY_OK;
}

/* ============================================================================
 * Proprioceptive Integration
 * ============================================================================ */

nimcp_body_error_t nimcp_body_process_proprio(
    nimcp_body_context_t* ctx,
    const nimcp_proprio_signal_t* signal
) {
    if (!ctx || !signal) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_body_part_t* part = find_part(ctx, signal->part_id);
    if (!part) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    /* Update position with proprioceptive input */
    nimcp_body_position_t sensed_pos = {
        signal->position[0],
        signal->position[1],
        signal->position[2]
    };

    /* Weighted integration with current estimate */
    double proprio_weight = ctx->config.proprio_weight * signal->confidence;
    double current_weight = 1.0 - proprio_weight;

    part->position.x = part->position.x * current_weight + sensed_pos.x * proprio_weight;
    part->position.y = part->position.y * current_weight + sensed_pos.y * proprio_weight;
    part->position.z = part->position.z * current_weight + sensed_pos.z * proprio_weight;

    /* Update joint if specified */
    if (signal->joint_id > 0) {
        for (uint32_t i = 0; i < part->num_joints; i++) {
            if (part->joints[i].joint_id == signal->joint_id) {
                /* Calculate prediction error */
                double error = 0.0;
                for (int j = 0; j < 3; j++) {
                    double diff = signal->position[j] - part->joints[i].predicted_angles[j];
                    error += diff * diff;
                }
                error = sqrt(error);
                part->joints[i].prediction_error = error;

                /* Update predicted angles */
                double lr = ctx->config.prediction_learning_rate;
                for (int j = 0; j < 3; j++) {
                    part->joints[i].predicted_angles[j] += lr *
                        (signal->position[j] - part->joints[i].predicted_angles[j]);
                }

                part->joints[i].proprioceptive_confidence = signal->confidence;
                part->joints[i].last_update = signal->timestamp;

                if (error > ctx->config.prediction_error_threshold) {
                    ctx->stats.prediction_errors++;
                }

                break;
            }
        }
    }

    part->last_update = get_timestamp_ns();
    ctx->stats.total_proprio_updates++;

    /* Update center of mass */
    update_center_of_mass(ctx);

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_process_visual(
    nimcp_body_context_t* ctx,
    const nimcp_visual_feedback_t* feedback
) {
    if (!ctx || !feedback) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_body_part_t* part = find_part(ctx, feedback->part_id);
    if (!part) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    if (!feedback->is_visible) {
        /* Part not visible - rely on proprioception */
        return NIMCP_BODY_OK;
    }

    /* Calculate visual-proprioceptive distance (position + orientation) */
    double vp_pos_distance = position_distance(&part->position, &feedback->seen_position);

    /* Use SO(3) geodesic distance for orientation mismatch */
    double vp_orient_distance = 0.0;
    {
        nimcp_body_quaternion_t qa = part->orientation;
        nimcp_body_quaternion_t qb = feedback->seen_orientation;
        float ha = (float)acos(fmin(1.0, fabs(qa.w)));
        float sha = sinf(ha);
        float ax_a[3] = {0,0,1}, ax_b[3] = {0,0,1};
        if (sha > 1e-6f) { ax_a[0]=(float)(qa.x/sha); ax_a[1]=(float)(qa.y/sha); ax_a[2]=(float)(qa.z/sha); }
        float hb = (float)acos(fmin(1.0, fabs(qb.w)));
        float shb = sinf(hb);
        if (shb > 1e-6f) { ax_b[0]=(float)(qb.x/shb); ax_b[1]=(float)(qb.y/shb); ax_b[2]=(float)(qb.z/shb); }
        so3_rotation_t R_a = so3_from_axis_angle(ax_a, 2.0f*ha);
        so3_rotation_t R_b = so3_from_axis_angle(ax_b, 2.0f*hb);
        vp_orient_distance = (double)so3_distance(&R_a, &R_b);
    }

    /* Combined distance: position + scaled orientation (radians → comparable units) */
    double vp_distance = vp_pos_distance + 0.1 * vp_orient_distance;

    /* Weighted integration */
    double visual_weight = ctx->config.visual_weight * feedback->confidence;
    double current_weight = 1.0 - visual_weight;

    nimcp_body_position_t new_pos;
    new_pos.x = part->position.x * current_weight + feedback->seen_position.x * visual_weight;
    new_pos.y = part->position.y * current_weight + feedback->seen_position.y * visual_weight;
    new_pos.z = part->position.z * current_weight + feedback->seen_position.z * visual_weight;

    /* Update ownership based on visual-proprioceptive congruence */
    update_ownership_internal(part, vp_distance, ctx->config.boundary_margin);

    part->position = new_pos;
    part->last_update = get_timestamp_ns();
    ctx->stats.total_visual_updates++;
    ctx->stats.total_integrations++;

    /* Update average prediction error */
    ctx->stats.avg_prediction_error = (ctx->stats.avg_prediction_error *
                                       (ctx->stats.total_integrations - 1) +
                                       vp_distance) / ctx->stats.total_integrations;

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_update_joint(
    nimcp_body_context_t* ctx,
    uint32_t part_id,
    const nimcp_joint_state_t* joint
) {
    if (!ctx || !joint) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_body_part_t* part = find_part(ctx, part_id);
    if (!part) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    /* Find or add joint */
    nimcp_joint_state_t* existing = NULL;
    for (uint32_t i = 0; i < part->num_joints; i++) {
        if (part->joints[i].joint_id == joint->joint_id) {
            existing = &part->joints[i];
            break;
        }
    }

    if (!existing) {
        if (part->num_joints >= NIMCP_BODY_MAX_JOINTS) {
            return NIMCP_BODY_ERROR_INVALID_JOINT;
        }
        existing = &part->joints[part->num_joints++];
    }

    *existing = *joint;
    existing->last_update = get_timestamp_ns();

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_get_prediction_error(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    uint32_t joint_id,
    double* error
) {
    if (!ctx || !error) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i] && ctx->parts[i].part_id == part_id) {
            for (uint32_t j = 0; j < ctx->parts[i].num_joints; j++) {
                if (ctx->parts[i].joints[j].joint_id == joint_id) {
                    *error = ctx->parts[i].joints[j].prediction_error;
                    return NIMCP_BODY_OK;
                }
            }
            return NIMCP_BODY_ERROR_INVALID_JOINT;
        }
    }

    return NIMCP_BODY_ERROR_INVALID_PART;
}

/* ============================================================================
 * Body Ownership
 * ============================================================================ */

nimcp_body_error_t nimcp_body_get_ownership(
    const nimcp_body_context_t* ctx,
    uint32_t part_id,
    nimcp_ownership_state_t* state,
    double* confidence
) {
    if (!ctx || !state || !confidence) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i] && ctx->parts[i].part_id == part_id) {
            *state = ctx->parts[i].ownership_state;
            *confidence = ctx->parts[i].ownership_confidence;
            return NIMCP_BODY_OK;
        }
    }

    return NIMCP_BODY_ERROR_INVALID_PART;
}

nimcp_body_error_t nimcp_body_update_ownership_sync(
    nimcp_body_context_t* ctx,
    uint32_t part_id,
    const nimcp_body_position_t* visual_position,
    const nimcp_body_position_t* tactile_position,
    bool is_synchronous
) {
    if (!ctx || !visual_position || !tactile_position) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_body_part_t* part = find_part(ctx, part_id);
    if (!part) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    if (is_synchronous) {
        /* Synchronous stimulation increases ownership (rubber hand illusion) */
        double visual_tactile_dist = position_distance(visual_position, tactile_position);

        /* If visual and tactile are close and synchronous, ownership increases */
        if (visual_tactile_dist < ctx->config.boundary_margin) {
            part->ownership_confidence += 0.05;
            if (part->ownership_confidence > 0.8) {
                part->ownership_state = NIMCP_OWNERSHIP_FULL;
            } else if (part->ownership_confidence > 0.5) {
                part->ownership_state = NIMCP_OWNERSHIP_PARTIAL;
            }
        }
    } else {
        /* Asynchronous stimulation decreases ownership */
        part->ownership_confidence -= 0.02;
        if (part->ownership_confidence < 0.3) {
            part->ownership_state = NIMCP_OWNERSHIP_UNCERTAIN;
        }
    }

    part->ownership_confidence = fmax(0.0, fmin(1.0, part->ownership_confidence));
    ctx->stats.ownership_changes++;

    LOG_DEBUG("Ownership update for part %u: state=%s, confidence=%.2f",
              part_id, nimcp_body_ownership_state_name(part->ownership_state),
              part->ownership_confidence);

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_process_external_object(
    nimcp_body_context_t* ctx,
    const nimcp_external_object_t* object
) {
    if (!ctx || !object) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Find or add external object */
    nimcp_external_object_t* existing = find_external_object(ctx, object->object_id);

    if (!existing) {
        /* Find free slot */
        int free_slot = -1;
        for (uint32_t i = 0; i < NIMCP_BODY_MAX_EXTERNAL_OBJECTS; i++) {
            if (!ctx->object_active[i]) {
                free_slot = (int)i;
                break;
            }
        }

        if (free_slot < 0) {
            LOG_WARN("External object limit reached");
            return NIMCP_BODY_ERROR_MEMORY;
        }

        existing = &ctx->external_objects[free_slot];
        ctx->object_active[free_slot] = true;
        ctx->num_external_objects++;
        existing->first_contact = get_timestamp_ns();
    }

    *existing = *object;
    existing->last_update = get_timestamp_ns();

    /* Check for incorporation based on synchrony */
    if (object->visual_tactile_sync > ctx->config.incorporation_threshold &&
        object->sync_duration > ctx->config.sync_window_ms / 1000.0) {

        existing->ownership_score += 0.1;
        if (existing->ownership_score > ctx->config.incorporation_threshold) {
            existing->is_incorporated = true;
            ctx->stats.incorporated_objects++;
            LOG_INFO("External object %u incorporated into body schema",
                     object->object_id);
        }
    }

    existing->ownership_score = fmax(0.0, fmin(1.0, existing->ownership_score));

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_incorporate_tool(
    nimcp_body_context_t* ctx,
    uint32_t object_id,
    uint32_t attach_part_id
) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    if (!ctx->config.enable_tool_incorporation) {
        return NIMCP_BODY_ERROR_OWNERSHIP_FAILED;
    }

    nimcp_external_object_t* obj = find_external_object(ctx, object_id);
    if (!obj) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    nimcp_body_part_t* attach_part = find_part(ctx, attach_part_id);
    if (!attach_part) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    /* Create new body part for tool */
    nimcp_body_part_t tool_part;
    memset(&tool_part, 0, sizeof(tool_part));

    tool_part.part_id = 1000 + object_id;  /* Tool parts start at 1000 */
    tool_part.type = NIMCP_BODY_PART_TOOL_EXTENSION;
    snprintf(tool_part.name, sizeof(tool_part.name), "Tool_%u", object_id);

    tool_part.position = obj->position;
    tool_part.orientation = obj->orientation;
    memcpy(tool_part.dimensions, obj->dimensions, sizeof(tool_part.dimensions));
    tool_part.mass = 0.5;  /* Default tool mass */

    tool_part.parent_part_id = attach_part_id;
    tool_part.ownership_state = NIMCP_OWNERSHIP_EXTENDED;
    tool_part.ownership_confidence = obj->ownership_score;
    tool_part.agency = 0.8;

    nimcp_body_error_t err = nimcp_body_add_part(ctx, &tool_part);
    if (err != NIMCP_BODY_OK) {
        return err;
    }

    obj->is_incorporated = true;
    obj->replaces_part_id = tool_part.part_id;

    LOG_INFO("Incorporated tool %u attached to part %u", object_id, attach_part_id);

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_release_tool(
    nimcp_body_context_t* ctx,
    uint32_t object_id
) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    nimcp_external_object_t* obj = find_external_object(ctx, object_id);
    if (!obj || !obj->is_incorporated) {
        return NIMCP_BODY_ERROR_INVALID_PART;
    }

    /* Remove the tool body part */
    nimcp_body_error_t err = nimcp_body_remove_part(ctx, obj->replaces_part_id);
    if (err != NIMCP_BODY_OK) {
        return err;
    }

    obj->is_incorporated = false;
    obj->ownership_score *= 0.5;  /* Reduce ownership on release */

    LOG_INFO("Released tool %u", object_id);

    return NIMCP_BODY_OK;
}

/* ============================================================================
 * Body Boundary
 * ============================================================================ */

nimcp_body_error_t nimcp_body_update_boundaries(nimcp_body_context_t* ctx) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Boundaries are implicitly defined by body parts + margin */
    /* This function triggers recalculation */
    update_center_of_mass(ctx);

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_check_boundary(
    const nimcp_body_context_t* ctx,
    const nimcp_body_position_t* position,
    bool* is_inside,
    double* nearest_distance
) {
    if (!ctx || !position || !is_inside || !nearest_distance) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *is_inside = false;
    *nearest_distance = DBL_MAX;

    /* Check distance to each body part */
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i]) {
            double dist = position_distance(position, &ctx->parts[i].position);

            /* Account for part dimensions */
            double part_radius = fmax(ctx->parts[i].dimensions[0],
                              fmax(ctx->parts[i].dimensions[1],
                                   ctx->parts[i].dimensions[2])) / 2.0;

            double boundary_dist = dist - part_radius - ctx->config.boundary_margin;

            if (boundary_dist < *nearest_distance) {
                *nearest_distance = boundary_dist;
            }

            if (boundary_dist < 0) {
                *is_inside = true;
            }
        }
    }

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_get_center_of_mass(
    const nimcp_body_context_t* ctx,
    nimcp_body_position_t* center_of_mass
) {
    if (!ctx || !center_of_mass) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *center_of_mass = ctx->center_of_mass;
    return NIMCP_BODY_OK;
}

/* ============================================================================
 * Peripersonal Space
 * ============================================================================ */

nimcp_body_error_t nimcp_body_update_peripersonal(
    nimcp_body_context_t* ctx,
    const nimcp_body_position_t* object_positions,
    uint32_t num_objects
) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    if (!ctx->config.enable_peripersonal) {
        return NIMCP_BODY_OK;
    }

    /* Decay existing activation */
    for (int i = 0; i < NIMCP_BODY_PERIPERSONAL_GRID_SIZE; i++) {
        for (int j = 0; j < NIMCP_BODY_PERIPERSONAL_GRID_SIZE; j++) {
            for (int k = 0; k < NIMCP_BODY_PERIPERSONAL_GRID_SIZE; k++) {
                ctx->peripersonal.activation[i][j][k] *= 0.9;
            }
        }
    }

    /* Update with new object positions */
    ctx->peripersonal.nearest_object_distance = DBL_MAX;

    for (uint32_t n = 0; n < num_objects; n++) {
        if (!object_positions) break;

        /* Distance from body center */
        double dist = position_distance(&ctx->center_of_mass, &object_positions[n]);

        if (dist < ctx->peripersonal.nearest_object_distance) {
            ctx->peripersonal.nearest_object_distance = dist;
            ctx->peripersonal.nearest_object_direction[0] =
                object_positions[n].x - ctx->center_of_mass.x;
            ctx->peripersonal.nearest_object_direction[1] =
                object_positions[n].y - ctx->center_of_mass.y;
            ctx->peripersonal.nearest_object_direction[2] =
                object_positions[n].z - ctx->center_of_mass.z;
        }

        /* Update grid */
        int ix, iy, iz;
        if (position_to_grid(ctx, &object_positions[n], &ix, &iy, &iz)) {
            /* Gaussian activation around object */
            for (int di = -2; di <= 2; di++) {
                for (int dj = -2; dj <= 2; dj++) {
                    for (int dk = -2; dk <= 2; dk++) {
                        int ni = ix + di;
                        int nj = iy + dj;
                        int nk = iz + dk;

                        if (ni >= 0 && ni < NIMCP_BODY_PERIPERSONAL_GRID_SIZE &&
                            nj >= 0 && nj < NIMCP_BODY_PERIPERSONAL_GRID_SIZE &&
                            nk >= 0 && nk < NIMCP_BODY_PERIPERSONAL_GRID_SIZE) {
                            double gauss_dist = sqrt(di*di + dj*dj + dk*dk);
                            double activation = exp(-gauss_dist * gauss_dist / 2.0);
                            ctx->peripersonal.activation[ni][nj][nk] =
                                fmax(ctx->peripersonal.activation[ni][nj][nk], activation);
                        }
                    }
                }
            }
        }
    }

    /* Calculate threat level */
    if (ctx->peripersonal.nearest_object_distance < ctx->config.peripersonal_range) {
        ctx->peripersonal.threat_level = 1.0 -
            (ctx->peripersonal.nearest_object_distance / ctx->config.peripersonal_range);
    } else {
        ctx->peripersonal.threat_level = 0.0;
    }

    ctx->peripersonal.last_update = get_timestamp_ns();

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_get_peripersonal(
    const nimcp_body_context_t* ctx,
    nimcp_peripersonal_space_t* space
) {
    if (!ctx || !space) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *space = ctx->peripersonal;
    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_check_peripersonal(
    const nimcp_body_context_t* ctx,
    const nimcp_body_position_t* position,
    bool* is_in_space,
    double* activation
) {
    if (!ctx || !position || !is_in_space || !activation) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *is_in_space = false;
    *activation = 0.0;

    double dist = position_distance(&ctx->center_of_mass, position);
    if (dist > ctx->config.peripersonal_range) {
        return NIMCP_BODY_OK;
    }

    *is_in_space = true;

    int ix, iy, iz;
    if (position_to_grid(ctx, position, &ix, &iy, &iz)) {
        *activation = ctx->peripersonal.activation[ix][iy][iz];
    }

    return NIMCP_BODY_OK;
}

/* ============================================================================
 * Update and Processing
 * ============================================================================ */

nimcp_body_error_t nimcp_body_update(
    nimcp_body_context_t* ctx,
    double delta_time
) {
    if (!ctx) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    if (!ctx->initialized) {
        return NIMCP_BODY_ERROR_NOT_INITIALIZED;
    }

    /* Decay ownership confidence */
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i]) {
            /* Only decay non-full ownership */
            if (ctx->parts[i].ownership_state != NIMCP_OWNERSHIP_FULL) {
                ctx->parts[i].ownership_confidence -=
                    ctx->config.ownership_decay_rate * delta_time;

                if (ctx->parts[i].ownership_confidence < 0.0) {
                    ctx->parts[i].ownership_confidence = 0.0;
                    ctx->parts[i].ownership_state = NIMCP_OWNERSHIP_NONE;
                }
            }
        }
    }

    /* Decay external object ownership */
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_EXTERNAL_OBJECTS; i++) {
        if (ctx->object_active[i]) {
            ctx->external_objects[i].ownership_score -=
                ctx->config.ownership_decay_rate * delta_time * 0.5;

            if (ctx->external_objects[i].ownership_score < 0.0) {
                ctx->external_objects[i].ownership_score = 0.0;
                if (ctx->external_objects[i].is_incorporated) {
                    /* Release tool if ownership drops */
                    nimcp_body_release_tool(ctx, ctx->external_objects[i].object_id);
                }
            }
        }
    }

    /* Update statistics */
    double total_ownership = 0.0;
    double total_agency = 0.0;
    uint32_t count = 0;

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS; i++) {
        if (ctx->part_active[i]) {
            total_ownership += ctx->parts[i].ownership_confidence;
            total_agency += ctx->parts[i].agency;
            count++;
        }
    }

    if (count > 0) {
        ctx->stats.avg_ownership_confidence = total_ownership / count;
        ctx->stats.avg_agency = total_agency / count;
    }

    ctx->last_update_time = get_timestamp_ns();

    return NIMCP_BODY_OK;
}

nimcp_body_error_t nimcp_body_predict(
    const nimcp_body_context_t* ctx,
    double delta_time,
    nimcp_body_part_t* predicted_parts,
    uint32_t max_parts,
    uint32_t* num_predicted
) {
    if (!ctx || !predicted_parts || !num_predicted) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *num_predicted = 0;

    for (uint32_t i = 0; i < NIMCP_BODY_MAX_PARTS && *num_predicted < max_parts; i++) {
        if (ctx->part_active[i]) {
            predicted_parts[*num_predicted] = ctx->parts[i];

            /* Simple linear prediction based on joint velocities */
            nimcp_body_part_t* pred = &predicted_parts[*num_predicted];

            for (uint32_t j = 0; j < pred->num_joints; j++) {
                for (int k = 0; k < 3; k++) {
                    pred->joints[j].predicted_angles[k] =
                        pred->joints[j].angles[k] +
                        pred->joints[j].velocities[k] * delta_time;

                    /* Clamp to limits */
                    pred->joints[j].predicted_angles[k] = fmax(
                        pred->joints[j].limits_min[k],
                        fmin(pred->joints[j].limits_max[k],
                             pred->joints[j].predicted_angles[k])
                    );
                }
            }

            (*num_predicted)++;
        }
    }

    return NIMCP_BODY_OK;
}

/* ============================================================================
 * Statistics and Utility
 * ============================================================================ */

nimcp_body_error_t nimcp_body_get_stats(
    const nimcp_body_context_t* ctx,
    nimcp_body_stats_t* stats
) {
    if (!ctx || !stats) {
        return NIMCP_BODY_ERROR_NULL_PARAM;
    }

    *stats = ctx->stats;
    stats->active_parts = ctx->num_parts;

    /* Count incorporated objects */
    uint32_t incorporated = 0;
    for (uint32_t i = 0; i < NIMCP_BODY_MAX_EXTERNAL_OBJECTS; i++) {
        if (ctx->object_active[i] && ctx->external_objects[i].is_incorporated) {
            incorporated++;
        }
    }
    stats->incorporated_objects = incorporated;

    return NIMCP_BODY_OK;
}

const char* nimcp_body_part_type_name(nimcp_body_part_type_t type) {
    static const char* names[] = {
        "Unknown", "Head", "Torso", "Left Arm", "Right Arm",
        "Left Hand", "Right Hand", "Left Leg", "Right Leg",
        "Left Foot", "Right Foot", "Neck", "Pelvis",
        "Finger", "Toe", "Tool Extension", "Prosthetic", "Virtual"
    };

    if (type >= 0 && type < NIMCP_BODY_PART_COUNT) {
        return names[type];
    }
    return "Unknown";
}

const char* nimcp_body_ownership_state_name(nimcp_ownership_state_t state) {
    static const char* names[] = {
        "None", "Uncertain", "Partial", "Full", "Extended"
    };

    if (state >= 0 && state < NIMCP_OWNERSHIP_COUNT) {
        return names[state];
    }
    return "Unknown";
}

const char* nimcp_body_joint_type_name(nimcp_joint_type_t type) {
    static const char* names[] = {
        "Unknown", "Ball", "Hinge", "Saddle", "Pivot", "Gliding", "Condyloid"
    };

    if (type >= 0 && type < NIMCP_JOINT_COUNT) {
        return names[type];
    }
    return "Unknown";
}
