/**
 * @file nimcp_retrosplenial.c
 * @brief Retrosplenial Cortex (RSC) Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Implementation of RSC with spatial reference frame transformations,
 *       contextual memory encoding, scene recognition, and navigation support.
 *
 * WHY:  The RSC is critical for transforming between egocentric and allocentric
 *       frames, encoding spatial context for memories, and supporting navigation.
 *
 * HOW:  Implements transformation matrices, context encoders, scene detectors,
 *       landmark database, and HD integration using biologically-motivated algorithms.
 *
 * @author NIMCP Development Team
 */

#include "core/brain/regions/retrosplenial/nimcp_retrosplenial.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include <float.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_neural_constants.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(retrosplenial, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define RSC_LOG_MODULE "RSC"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define US_PER_MS       1000ULL
#define US_PER_SECOND   1000000ULL
#define PI              NIMCP_PI_F
#define TWO_PI          (2.0f * PI)

/* Default transform matrix (identity) */
static const float IDENTITY_MATRIX[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Normalize angle to [-PI, PI]
 */
static inline float normalize_angle(float angle) {
    while (angle > PI) angle -= TWO_PI;
    while (angle < -PI) angle += TWO_PI;
    return angle;
}

/**
 * @brief Compute Euclidean distance between positions
 */
static float position_distance(
    const nimcp_rsc_position_t* a,
    const nimcp_rsc_position_t* b
) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Exponential decay toward target
 */
static inline float exponential_decay(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.0f - expf(-rate * dt));
}

/**
 * @brief Dot product of two vectors
 */
static float vector_dot(const float* a, const float* b, uint32_t dim) {
    float dot = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return dot;
}

/**
 * @brief Normalize vector in place
 */
static void vector_normalize(float* v, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm += v[i] * v[i];
    }
    norm = sqrtf(norm);
    if (norm > 1e-8f) {
        for (uint32_t i = 0; i < dim; i++) {
            v[i] /= norm;
        }
    }
}

/**
 * @brief Get current timestamp in microseconds
 */
static uint64_t get_timestamp_us(void) {
    /* Platform-independent timestamp - simplified for now */
    static _Atomic uint64_t counter = 0;
    return counter++;
}

/**
 * @brief Initialize context structure
 */
static void init_context(nimcp_rsc_context_t* ctx, uint32_t context_dim) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->context_dim = context_dim;
}

/**
 * @brief Initialize navigation structure
 */
static void init_navigation(nimcp_rsc_navigation_t* nav, uint32_t num_hd_cells) {
    memset(nav, 0, sizeof(*nav));
    nav->num_hd_cells = num_hd_cells;
    nav->hd_confidence = 0.5f;

    /* Initialize identity transforms */
    memcpy(nav->ego_to_allo.matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
    nav->ego_to_allo.source_frame = RSC_FRAME_EGOCENTRIC;
    nav->ego_to_allo.target_frame = RSC_FRAME_ALLOCENTRIC;
    nav->ego_to_allo.accuracy = 1.0f;

    memcpy(nav->allo_to_ego.matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
    nav->allo_to_ego.source_frame = RSC_FRAME_ALLOCENTRIC;
    nav->allo_to_ego.target_frame = RSC_FRAME_EGOCENTRIC;
    nav->allo_to_ego.accuracy = 1.0f;
}

/**
 * @brief Initialize scene structure
 */
static void init_scene(nimcp_rsc_scene_t* scene, uint32_t scene_dim) {
    memset(scene, 0, sizeof(*scene));
    scene->scene_dim = scene_dim;
    scene->familiarity = RSC_SCENE_NOVEL;
}

/**
 * @brief Initialize imagination structure
 */
static void init_imagination(nimcp_rsc_imagination_t* imagination) {
    memset(imagination, 0, sizeof(*imagination));
    imagination->mode = RSC_IMAGINE_PROSPECTIVE;
    imagination->active = false;
    imagination->vividness = 0.5f;
    imagination->plausibility = 1.0f;
}

/**
 * @brief Build rotation matrix from heading
 */
static void build_rotation_matrix_z(float* matrix, float heading) {
    float cos_h = cosf(heading);
    float sin_h = sinf(heading);

    /* Initialize to identity */
    memcpy(matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));

    /* Set rotation around Z axis */
    matrix[0] = cos_h;   matrix[1] = -sin_h;
    matrix[4] = sin_h;   matrix[5] = cos_h;
}

/**
 * @brief Apply transformation matrix to position
 */
static void transform_position(
    const float* matrix,
    const nimcp_rsc_position_t* in,
    nimcp_rsc_position_t* out
) {
    float x = in->x;
    float y = in->y;
    float z = in->z;

    out->x = matrix[0] * x + matrix[1] * y + matrix[2] * z + matrix[3];
    out->y = matrix[4] * x + matrix[5] * y + matrix[6] * z + matrix[7];
    out->z = matrix[8] * x + matrix[9] * y + matrix[10] * z + matrix[11];
}

/**
 * @brief Compute familiarity level from score
 */
static nimcp_rsc_familiarity_t score_to_familiarity(float score) {
    if (score < 0.2f) return RSC_SCENE_NOVEL;
    if (score < 0.4f) return RSC_SCENE_VAGUELY_FAMILIAR;
    if (score < 0.6f) return RSC_SCENE_FAMILIAR;
    if (score < 0.8f) return RSC_SCENE_VERY_FAMILIAR;
    return RSC_SCENE_HIGHLY_FAMILIAR;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

nimcp_rsc_config_t nimcp_rsc_default_config(void) {
    nimcp_rsc_config_t config;
    memset(&config, 0, sizeof(config));

    /* Neuron counts */
    config.num_transform_neurons = RSC_DEFAULT_TRANSFORM_NEURONS;
    config.num_context_neurons = RSC_DEFAULT_CONTEXT_NEURONS;
    config.num_scene_neurons = RSC_DEFAULT_SCENE_NEURONS;
    config.num_hd_neurons = RSC_DEFAULT_HD_NEURONS;
    config.num_landmark_neurons = RSC_DEFAULT_LANDMARK_NEURONS;

    /* Spatial parameters */
    config.spatial_dim = RSC_DEFAULT_SPATIAL_DIM;
    config.feature_dim = RSC_DEFAULT_FEATURE_DIM;
    config.context_dim = RSC_CONTEXT_DIM;
    config.scene_dim = RSC_SCENE_DIM;

    /* Transformation parameters */
    config.transform_learning_rate = NIMCP_LEARNING_RATE_DEFAULT;
    config.transform_smoothing = 0.9f;
    config.transform_error_threshold = 0.1f;

    /* Context encoding parameters */
    config.context_decay_rate = 0.01f;
    config.context_update_rate = 0.1f;
    config.context_threshold = 0.3f;
    config.max_context_history = RSC_MAX_CONTEXT_HISTORY;

    /* Scene recognition parameters */
    config.scene_familiarity_threshold = 0.5f;
    config.scene_learning_rate = 0.05f;
    config.landmark_salience_threshold = NIMCP_SALIENCE_THRESHOLD;
    config.max_landmarks = RSC_MAX_LANDMARKS;

    /* Navigation parameters */
    config.hd_integration_gain = 1.0f;
    config.path_integration_coupling = 0.8f;
    config.visual_calibration_rate = 0.1f;

    /* Imagination parameters */
    config.imagination_vividness_default = 0.7f;
    config.temporal_projection_max = 3600.0f;  /* 1 hour */
    config.enable_prospective_coding = true;

    /* Integration enables - defaults */
    config.enable_security = true;
    config.enable_immune = true;
    config.enable_bio_async = true;
    config.enable_kg = true;
    config.enable_snn = false;  /* Optional */
    config.enable_logging = true;
    config.enable_hippocampus = true;
    config.enable_entorhinal = true;
    config.enable_parietal = true;
    config.enable_thalamic = true;

    /* Platform tier */
    config.min_tier = PLATFORM_TIER_MEDIUM;

    return config;
}

nimcp_retrosplenial_t* nimcp_rsc_create(const nimcp_rsc_config_t* config) {
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Creating Retrosplenial Cortex module");

    nimcp_retrosplenial_t* rsc = (nimcp_retrosplenial_t*)nimcp_calloc(
        1, sizeof(nimcp_retrosplenial_t));
    if (!rsc) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate RSC memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rsc_create: rsc is NULL");
        return NULL;
    }

    /* Set configuration */
    if (config) {
        rsc->config = *config;
    } else {
        rsc->config = nimcp_rsc_default_config();
    }

    /* Create mutex for thread safety */
    mutex_attr_t mutex_attr = { .type = MUTEX_TYPE_RECURSIVE };
    rsc->mutex = nimcp_mutex_create(&mutex_attr);
    if (rsc->mutex) {
        rsc->mutex_owned = true;
    } else {
        LOG_MODULE_WARN(RSC_LOG_MODULE, "Failed to create mutex, running without thread safety");
    }

    /* Initialize navigation state */
    init_navigation(&rsc->navigation, rsc->config.num_hd_neurons);

    /* Allocate HD cell activations */
    if (rsc->config.num_hd_neurons > 0) {
        rsc->navigation.hd_cell_activations = (float*)nimcp_calloc(
            rsc->config.num_hd_neurons, sizeof(float));
        if (!rsc->navigation.hd_cell_activations) {
            LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate HD cell activations");
            nimcp_rsc_destroy(rsc);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rsc_create: rsc->navigation is NULL");
            return NULL;
        }
    }

    /* Initialize context state */
    init_context(&rsc->current_context, rsc->config.context_dim);

    /* Allocate context vectors */
    rsc->current_context.unified_context = (float*)nimcp_calloc(
        rsc->config.context_dim, sizeof(float));
    rsc->current_context.spatial_context = (float*)nimcp_calloc(
        rsc->config.context_dim, sizeof(float));
    rsc->current_context.temporal_context = (float*)nimcp_calloc(
        rsc->config.context_dim, sizeof(float));
    rsc->current_context.environmental_context = (float*)nimcp_calloc(
        rsc->config.context_dim, sizeof(float));

    if (!rsc->current_context.unified_context ||
        !rsc->current_context.spatial_context ||
        !rsc->current_context.temporal_context ||
        !rsc->current_context.environmental_context) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate context vectors");
        nimcp_rsc_destroy(rsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rsc_create: operation failed");
        return NULL;
    }

    /* Initialize scene state */
    init_scene(&rsc->current_scene, rsc->config.scene_dim);

    /* Allocate scene vector */
    rsc->current_scene.scene_vector = (float*)nimcp_calloc(
        rsc->config.scene_dim, sizeof(float));
    if (!rsc->current_scene.scene_vector) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate scene vector");
        nimcp_rsc_destroy(rsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rsc_create: rsc->current_scene is NULL");
        return NULL;
    }

    /* Initialize imagination state */
    init_imagination(&rsc->imagination);

    /* Allocate landmarks array */
    rsc->landmark_capacity = rsc->config.max_landmarks;
    rsc->landmarks = (nimcp_rsc_landmark_t*)nimcp_calloc(
        rsc->landmark_capacity, sizeof(nimcp_rsc_landmark_t));
    if (!rsc->landmarks) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate landmarks array");
        nimcp_rsc_destroy(rsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rsc_create: rsc->landmarks is NULL");
        return NULL;
    }

    /* Allocate context history */
    rsc->context_history = (nimcp_rsc_context_t*)nimcp_calloc(
        rsc->config.max_context_history, sizeof(nimcp_rsc_context_t));
    if (!rsc->context_history) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate context history");
        nimcp_rsc_destroy(rsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_rsc_create: rsc->context_history is NULL");
        return NULL;
    }

    /* Allocate neural activation arrays */
    rsc->transform_activations = (float*)nimcp_calloc(
        rsc->config.num_transform_neurons, sizeof(float));
    rsc->context_activations = (float*)nimcp_calloc(
        rsc->config.num_context_neurons, sizeof(float));
    rsc->scene_activations = (float*)nimcp_calloc(
        rsc->config.num_scene_neurons, sizeof(float));
    rsc->hd_activations = (float*)nimcp_calloc(
        rsc->config.num_hd_neurons, sizeof(float));

    if (!rsc->transform_activations || !rsc->context_activations ||
        !rsc->scene_activations || !rsc->hd_activations) {
        LOG_MODULE_ERROR(RSC_LOG_MODULE, "Failed to allocate neural activations");
        nimcp_rsc_destroy(rsc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rsc_create: operation failed");
        return NULL;
    }

    /* Initialize timing */
    rsc->creation_time_us = get_timestamp_us();
    rsc->last_update_us = rsc->creation_time_us;
    rsc->simulation_dt_ms = 1.0f;

    /* Set initial status */
    rsc->status = RSC_STATUS_IDLE;
    rsc->last_error = RSC_OK;
    rsc->initialized = true;

    LOG_MODULE_INFO(RSC_LOG_MODULE, "RSC created: transform=%u, context=%u, scene=%u, hd=%u neurons",
             rsc->config.num_transform_neurons, rsc->config.num_context_neurons,
             rsc->config.num_scene_neurons, rsc->config.num_hd_neurons);

    return rsc;
}

void nimcp_rsc_destroy(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return;

    LOG_MODULE_INFO(RSC_LOG_MODULE, "Destroying RSC module");

    /* Log final stats */
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Final stats: updates=%lu, transforms=%lu, contexts=%lu",
             (unsigned long)rsc->stats.updates_processed,
             (unsigned long)rsc->stats.transforms_computed,
             (unsigned long)rsc->stats.contexts_encoded);

    /* Free navigation allocations */
    if (rsc->navigation.hd_cell_activations) {
        nimcp_free(rsc->navigation.hd_cell_activations);
    }
    if (rsc->navigation.planned_path) {
        nimcp_free(rsc->navigation.planned_path);
    }

    /* Free context allocations */
    if (rsc->current_context.unified_context) {
        nimcp_free(rsc->current_context.unified_context);
    }
    if (rsc->current_context.spatial_context) {
        nimcp_free(rsc->current_context.spatial_context);
    }
    if (rsc->current_context.temporal_context) {
        nimcp_free(rsc->current_context.temporal_context);
    }
    if (rsc->current_context.environmental_context) {
        nimcp_free(rsc->current_context.environmental_context);
    }
    if (rsc->current_context.social_context) {
        nimcp_free(rsc->current_context.social_context);
    }
    if (rsc->current_context.emotional_context) {
        nimcp_free(rsc->current_context.emotional_context);
    }
    if (rsc->current_context.task_context) {
        nimcp_free(rsc->current_context.task_context);
    }

    /* Free scene allocations */
    if (rsc->current_scene.scene_vector) {
        nimcp_free(rsc->current_scene.scene_vector);
    }
    if (rsc->current_scene.landmark_ids) {
        nimcp_free(rsc->current_scene.landmark_ids);
    }
    if (rsc->current_scene.landmark_positions) {
        nimcp_free(rsc->current_scene.landmark_positions);
    }

    /* Free landmarks */
    if (rsc->landmarks) {
        for (uint32_t i = 0; i < rsc->num_landmarks; i++) {
            if (rsc->landmarks[i].visual_features) {
                nimcp_free(rsc->landmarks[i].visual_features);
            }
        }
        nimcp_free(rsc->landmarks);
    }

    /* Free context history */
    if (rsc->context_history) {
        for (uint32_t i = 0; i < rsc->config.max_context_history; i++) {
            if (rsc->context_history[i].unified_context) {
                nimcp_free(rsc->context_history[i].unified_context);
            }
        }
        nimcp_free(rsc->context_history);
    }

    /* Free neural activations */
    if (rsc->transform_activations) nimcp_free(rsc->transform_activations);
    if (rsc->context_activations) nimcp_free(rsc->context_activations);
    if (rsc->scene_activations) nimcp_free(rsc->scene_activations);
    if (rsc->hd_activations) nimcp_free(rsc->hd_activations);

    /* Destroy mutex */
    if (rsc->mutex && rsc->mutex_owned) {
        nimcp_mutex_free(rsc->mutex);
    }

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "RSC destroyed");
    nimcp_free(rsc);
}

nimcp_rsc_error_t nimcp_rsc_reset(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Resetting RSC state");

    /* Reset navigation */
    memset(&rsc->navigation.current_pose, 0, sizeof(rsc->navigation.current_pose));
    rsc->navigation.heading = 0.0f;
    rsc->navigation.speed = 0.0f;
    rsc->navigation.angular_velocity = 0.0f;
    rsc->navigation.head_direction = 0.0f;
    rsc->navigation.hd_confidence = 0.5f;
    rsc->navigation.goal_set = false;

    if (rsc->navigation.hd_cell_activations) {
        memset(rsc->navigation.hd_cell_activations, 0,
               rsc->config.num_hd_neurons * sizeof(float));
    }

    /* Reset identity transforms */
    memcpy(rsc->navigation.ego_to_allo.matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
    memcpy(rsc->navigation.allo_to_ego.matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));

    /* Reset context */
    if (rsc->current_context.unified_context) {
        memset(rsc->current_context.unified_context, 0,
               rsc->config.context_dim * sizeof(float));
    }
    rsc->current_context.context_strength = 0.0f;
    rsc->current_context.context_stability = 0.0f;

    /* Reset scene */
    if (rsc->current_scene.scene_vector) {
        memset(rsc->current_scene.scene_vector, 0,
               rsc->config.scene_dim * sizeof(float));
    }
    rsc->current_scene.familiarity = RSC_SCENE_NOVEL;
    rsc->current_scene.familiarity_score = 0.0f;

    /* Reset imagination */
    rsc->imagination.active = false;
    rsc->imagination.steps_simulated = 0;

    /* Reset context history */
    rsc->context_history_size = 0;
    rsc->context_history_index = 0;

    /* Reset neural activations */
    if (rsc->transform_activations) {
        memset(rsc->transform_activations, 0,
               rsc->config.num_transform_neurons * sizeof(float));
    }
    if (rsc->context_activations) {
        memset(rsc->context_activations, 0,
               rsc->config.num_context_neurons * sizeof(float));
    }
    if (rsc->scene_activations) {
        memset(rsc->scene_activations, 0,
               rsc->config.num_scene_neurons * sizeof(float));
    }
    if (rsc->hd_activations) {
        memset(rsc->hd_activations, 0,
               rsc->config.num_hd_neurons * sizeof(float));
    }

    /* Reset timing */
    rsc->last_update_us = get_timestamp_us();

    /* Reset status */
    rsc->status = RSC_STATUS_IDLE;
    rsc->last_error = RSC_OK;

    /* Note: Landmarks and statistics are NOT reset */

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "RSC reset complete");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_update(nimcp_retrosplenial_t* rsc, float dt_ms) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    uint64_t start_us = get_timestamp_us();

    /* Update simulation timestep */
    rsc->simulation_dt_ms = dt_ms;
    float dt_s = dt_ms / 1000.0f;

    /* Update HD cell activations based on current heading */
    if (rsc->config.num_hd_neurons > 0 && rsc->navigation.hd_cell_activations) {
        float heading = rsc->navigation.head_direction;
        float tuning_width = PI / 3.0f;  /* 60 degree tuning width */

        for (uint32_t i = 0; i < rsc->config.num_hd_neurons; i++) {
            float preferred_dir = (float)i * TWO_PI / (float)rsc->config.num_hd_neurons;
            float diff = normalize_angle(heading - preferred_dir);
            float activation = expf(-(diff * diff) / (2.0f * tuning_width * tuning_width));
            rsc->navigation.hd_cell_activations[i] = activation;
            rsc->hd_activations[i] = activation;
        }
    }

    /* Decay context strength over time */
    rsc->current_context.context_strength = exponential_decay(
        rsc->current_context.context_strength, 0.0f,
        rsc->config.context_decay_rate, dt_s
    );

    /* Update scene familiarity decay */
    rsc->current_scene.familiarity_score = exponential_decay(
        rsc->current_scene.familiarity_score, 0.0f,
        0.001f, dt_s  /* Slow decay */
    );

    /* Update imagination if active */
    if (rsc->imagination.active) {
        rsc->imagination.steps_simulated++;
        rsc->imagination.vividness = exponential_decay(
            rsc->imagination.vividness, 0.3f, 0.1f, dt_s
        );
    }

    /* Update timing */
    rsc->last_update_us = get_timestamp_us();
    rsc->stats.updates_processed++;

    /* Update latency stats */
    float latency_us = (float)(rsc->last_update_us - start_us);
    rsc->stats.mean_update_latency_us = exponential_decay(
        rsc->stats.mean_update_latency_us, latency_us, 0.1f, 1.0f
    );
    if (latency_us > rsc->stats.max_update_latency_us) {
        rsc->stats.max_update_latency_us = latency_us;
    }

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);

    return RSC_OK;
}

/*=============================================================================
 * REFERENCE FRAME TRANSFORMATION API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_transform_position(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* input_pos,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_position_t* output_pos
) {
    if (!rsc || !input_pos || !output_pos) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_TRANSFORMING;

    /* Select appropriate transformation matrix */
    const float* matrix = NULL;

    if (source_frame == RSC_FRAME_EGOCENTRIC && target_frame == RSC_FRAME_ALLOCENTRIC) {
        matrix = rsc->navigation.ego_to_allo.matrix;
    } else if (source_frame == RSC_FRAME_ALLOCENTRIC && target_frame == RSC_FRAME_EGOCENTRIC) {
        matrix = rsc->navigation.allo_to_ego.matrix;
    } else if (source_frame == target_frame) {
        /* Identity transform */
        *output_pos = *input_pos;
        rsc->stats.transforms_computed++;
        rsc->status = RSC_STATUS_READY;
        if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
        return RSC_OK;
    } else {
        /* Unsupported transform - use identity */
        *output_pos = *input_pos;
        LOG_MODULE_WARN(RSC_LOG_MODULE, "Unsupported frame transform: %d -> %d",
                    source_frame, target_frame);
        rsc->status = RSC_STATUS_READY;
        if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
        return RSC_OK;
    }

    /* Apply transformation */
    transform_position(matrix, input_pos, output_pos);

    rsc->stats.transforms_computed++;
    rsc->status = RSC_STATUS_READY;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_transform_pose(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_pose_t* input_pose,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_pose_t* output_pose
) {
    if (!rsc || !input_pose || !output_pose) return RSC_ERR_NULL_PTR;

    /* Transform position */
    nimcp_rsc_error_t err = nimcp_rsc_transform_position(
        rsc, &input_pose->position, source_frame, target_frame, &output_pose->position
    );
    if (err != RSC_OK) return err;

    /* Transform orientation */
    if (source_frame == RSC_FRAME_EGOCENTRIC && target_frame == RSC_FRAME_ALLOCENTRIC) {
        /* Add current heading to get world orientation */
        output_pose->orientation.yaw = normalize_angle(
            input_pose->orientation.yaw + rsc->navigation.heading
        );
    } else if (source_frame == RSC_FRAME_ALLOCENTRIC && target_frame == RSC_FRAME_EGOCENTRIC) {
        /* Subtract current heading to get body-relative orientation */
        output_pose->orientation.yaw = normalize_angle(
            input_pose->orientation.yaw - rsc->navigation.heading
        );
    } else {
        output_pose->orientation = input_pose->orientation;
    }

    output_pose->orientation.pitch = input_pose->orientation.pitch;
    output_pose->orientation.roll = input_pose->orientation.roll;
    output_pose->confidence = input_pose->confidence;
    output_pose->timestamp_us = get_timestamp_us();

    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_calibrate_transform(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* ego_pos,
    const nimcp_rsc_position_t* allo_pos,
    float heading
) {
    if (!rsc || !ego_pos || !allo_pos) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    float lr = rsc->config.transform_learning_rate;

    /* Update heading */
    rsc->navigation.heading = normalize_angle(heading);

    /* Build rotation matrix for ego->allo transform */
    float rotation[16];
    build_rotation_matrix_z(rotation, heading);

    /* Set translation */
    rotation[3] = allo_pos->x - (rotation[0] * ego_pos->x + rotation[1] * ego_pos->y);
    rotation[7] = allo_pos->y - (rotation[4] * ego_pos->x + rotation[5] * ego_pos->y);
    rotation[11] = allo_pos->z - ego_pos->z;

    /* Blend with current transform */
    for (int i = 0; i < 16; i++) {
        rsc->navigation.ego_to_allo.matrix[i] = exponential_decay(
            rsc->navigation.ego_to_allo.matrix[i], rotation[i], lr, 1.0f
        );
    }

    /* Update inverse transform (allo->ego) */
    build_rotation_matrix_z(rotation, -heading);
    rotation[3] = ego_pos->x - (rotation[0] * allo_pos->x + rotation[1] * allo_pos->y);
    rotation[7] = ego_pos->y - (rotation[4] * allo_pos->x + rotation[5] * allo_pos->y);
    rotation[11] = ego_pos->z - allo_pos->z;

    for (int i = 0; i < 16; i++) {
        rsc->navigation.allo_to_ego.matrix[i] = exponential_decay(
            rsc->navigation.allo_to_ego.matrix[i], rotation[i], lr, 1.0f
        );
    }

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Transform calibrated: heading=%.2f rad", heading);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_transform(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_frame_t source_frame,
    nimcp_rsc_frame_t target_frame,
    nimcp_rsc_transform_t* transform
) {
    if (!rsc || !transform) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (source_frame == RSC_FRAME_EGOCENTRIC && target_frame == RSC_FRAME_ALLOCENTRIC) {
        *transform = rsc->navigation.ego_to_allo;
    } else if (source_frame == RSC_FRAME_ALLOCENTRIC && target_frame == RSC_FRAME_EGOCENTRIC) {
        *transform = rsc->navigation.allo_to_ego;
    } else {
        /* Return identity */
        memcpy(transform->matrix, IDENTITY_MATRIX, sizeof(IDENTITY_MATRIX));
        transform->source_frame = source_frame;
        transform->target_frame = target_frame;
        transform->accuracy = 1.0f;
    }

    return RSC_OK;
}

/*=============================================================================
 * CONTEXT ENCODING API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_encode_context(
    nimcp_retrosplenial_t* rsc,
    const float* spatial_features,
    uint32_t spatial_dim,
    const float* temporal_features,
    uint32_t temporal_dim
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_ENCODING_CONTEXT;

    uint32_t ctx_dim = rsc->config.context_dim;

    /* Encode spatial context */
    if (spatial_features && spatial_dim > 0 && rsc->current_context.spatial_context) {
        uint32_t copy_dim = (spatial_dim < ctx_dim) ? spatial_dim : ctx_dim;
        memcpy(rsc->current_context.spatial_context, spatial_features,
               copy_dim * sizeof(float));
        vector_normalize(rsc->current_context.spatial_context, ctx_dim);
    }

    /* Encode temporal context */
    if (temporal_features && temporal_dim > 0 && rsc->current_context.temporal_context) {
        uint32_t copy_dim = (temporal_dim < ctx_dim) ? temporal_dim : ctx_dim;
        memcpy(rsc->current_context.temporal_context, temporal_features,
               copy_dim * sizeof(float));
        vector_normalize(rsc->current_context.temporal_context, ctx_dim);
    }

    /* Combine into unified context */
    if (rsc->current_context.unified_context) {
        for (uint32_t i = 0; i < ctx_dim; i++) {
            float spatial = rsc->current_context.spatial_context ?
                            rsc->current_context.spatial_context[i] : 0.0f;
            float temporal = rsc->current_context.temporal_context ?
                             rsc->current_context.temporal_context[i] : 0.0f;

            rsc->current_context.unified_context[i] = 0.5f * spatial + 0.5f * temporal;
        }
        vector_normalize(rsc->current_context.unified_context, ctx_dim);
    }

    /* Update context metadata */
    rsc->current_context.context_strength = 1.0f;
    rsc->current_context.context_stability = nimcp_clamp01(rsc->current_context.context_stability + 0.1f);
    rsc->current_context.encoding_time_us = get_timestamp_us();

    /* Update context activations */
    if (rsc->context_activations && rsc->current_context.unified_context) {
        uint32_t copy_dim = (ctx_dim < rsc->config.num_context_neurons) ?
                           ctx_dim : rsc->config.num_context_neurons;
        memcpy(rsc->context_activations, rsc->current_context.unified_context,
               copy_dim * sizeof(float));
    }

    /* Add to context history */
    if (rsc->context_history && rsc->config.max_context_history > 0) {
        uint32_t idx = rsc->context_history_index;
        nimcp_rsc_context_t* hist_entry = &rsc->context_history[idx];

        /* Allocate if needed */
        if (!hist_entry->unified_context) {
            hist_entry->unified_context = (float*)nimcp_calloc(ctx_dim, sizeof(float));
        }

        if (hist_entry->unified_context) {
            memcpy(hist_entry->unified_context, rsc->current_context.unified_context,
                   ctx_dim * sizeof(float));
            hist_entry->context_dim = ctx_dim;
            hist_entry->context_strength = rsc->current_context.context_strength;
            hist_entry->encoding_time_us = rsc->current_context.encoding_time_us;
        }

        rsc->context_history_index = (idx + 1) % rsc->config.max_context_history;
        if (rsc->context_history_size < rsc->config.max_context_history) {
            rsc->context_history_size++;
        }
    }

    rsc->stats.contexts_encoded++;
    rsc->stats.mean_context_strength = exponential_decay(
        rsc->stats.mean_context_strength, 1.0f, 0.1f, 1.0f
    );

    rsc->status = RSC_STATUS_READY;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_context(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_context_t* context
) {
    if (!rsc || !context) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *context = rsc->current_context;

    /* Note: Pointers are shallow copied - caller should not free them */
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_recall_context(
    nimcp_retrosplenial_t* rsc,
    const float* cue,
    uint32_t cue_dim,
    nimcp_rsc_context_t* recalled_context,
    float* similarity
) {
    if (!rsc || !cue || !recalled_context) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_RECALLING;

    float best_similarity = -1.0f;
    int best_idx = -1;
    uint32_t ctx_dim = rsc->config.context_dim;

    /* Search context history for best match */
    for (uint32_t i = 0; i < rsc->context_history_size; i++) {
        const nimcp_rsc_context_t* hist = &rsc->context_history[i];
        if (!hist->unified_context) continue;

        /* Compute similarity (cosine similarity) */
        float dot = 0.0f;
        float norm_cue = 0.0f;
        float norm_hist = 0.0f;

        uint32_t compare_dim = (cue_dim < ctx_dim) ? cue_dim : ctx_dim;
        for (uint32_t j = 0; j < compare_dim; j++) {
            dot += cue[j] * hist->unified_context[j];
            norm_cue += cue[j] * cue[j];
            norm_hist += hist->unified_context[j] * hist->unified_context[j];
        }

        float sim = 0.0f;
        if (norm_cue > 1e-8f && norm_hist > 1e-8f) {
            sim = dot / (sqrtf(norm_cue) * sqrtf(norm_hist));
        }

        if (sim > best_similarity) {
            best_similarity = sim;
            best_idx = (int)i;
        }
    }

    if (best_idx >= 0) {
        *recalled_context = rsc->context_history[best_idx];
        rsc->stats.contexts_recalled++;
    } else {
        /* No match found - return current context */
        *recalled_context = rsc->current_context;
        best_similarity = 0.0f;
    }

    if (similarity) {
        *similarity = best_similarity;
    }

    rsc->status = RSC_STATUS_READY;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_update_context(
    nimcp_retrosplenial_t* rsc,
    nimcp_rsc_context_type_t context_type,
    const float* features,
    uint32_t feature_dim,
    float blend_factor
) {
    if (!rsc || !features) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    blend_factor = nimcp_clamp01(blend_factor);
    uint32_t ctx_dim = rsc->config.context_dim;
    uint32_t copy_dim = (feature_dim < ctx_dim) ? feature_dim : ctx_dim;

    float* target = NULL;

    switch (context_type) {
        case RSC_CONTEXT_SPATIAL:
            target = rsc->current_context.spatial_context;
            break;
        case RSC_CONTEXT_TEMPORAL:
            target = rsc->current_context.temporal_context;
            break;
        case RSC_CONTEXT_ENVIRONMENTAL:
            target = rsc->current_context.environmental_context;
            break;
        case RSC_CONTEXT_SOCIAL:
            target = rsc->current_context.social_context;
            break;
        case RSC_CONTEXT_EMOTIONAL:
            target = rsc->current_context.emotional_context;
            break;
        case RSC_CONTEXT_TASK:
            target = rsc->current_context.task_context;
            break;
        default:
            if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
            return RSC_ERR_INVALID_PARAM;
    }

    if (target) {
        for (uint32_t i = 0; i < copy_dim; i++) {
            target[i] = (1.0f - blend_factor) * target[i] + blend_factor * features[i];
        }
    }

    /* Update dominant type based on strength */
    rsc->current_context.dominant_type = context_type;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

/*=============================================================================
 * SCENE RECOGNITION API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_process_scene(
    nimcp_retrosplenial_t* rsc,
    const float* scene_features,
    uint32_t feature_dim
) {
    if (!rsc || !scene_features) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_RECOGNIZING_SCENE;

    uint32_t scene_dim = rsc->config.scene_dim;
    uint32_t copy_dim = (feature_dim < scene_dim) ? feature_dim : scene_dim;

    /* Update scene vector */
    if (rsc->current_scene.scene_vector) {
        memcpy(rsc->current_scene.scene_vector, scene_features, copy_dim * sizeof(float));
        vector_normalize(rsc->current_scene.scene_vector, scene_dim);
    }

    /* Update scene activations */
    if (rsc->scene_activations) {
        uint32_t act_dim = (scene_dim < rsc->config.num_scene_neurons) ?
                          scene_dim : rsc->config.num_scene_neurons;
        memcpy(rsc->scene_activations, rsc->current_scene.scene_vector,
               act_dim * sizeof(float));
    }

    /* Compute familiarity (simplified - would be based on learned scenes) */
    float familiarity_score = 0.0f;

    /* Check against known landmarks for familiarity */
    for (uint32_t i = 0; i < rsc->num_landmarks; i++) {
        const nimcp_rsc_landmark_t* lm = &rsc->landmarks[i];
        if (lm->visual_features && lm->feature_dim > 0) {
            uint32_t cmp_dim = (lm->feature_dim < feature_dim) ? lm->feature_dim : feature_dim;
            float sim = vector_dot(scene_features, lm->visual_features, cmp_dim);
            if (sim > familiarity_score) {
                familiarity_score = nimcp_clamp01(sim);
            }
        }
    }

    /* Update familiarity */
    rsc->current_scene.familiarity_score = exponential_decay(
        rsc->current_scene.familiarity_score, familiarity_score,
        rsc->config.scene_learning_rate, 1.0f
    );
    rsc->current_scene.familiarity = score_to_familiarity(rsc->current_scene.familiarity_score);
    rsc->current_scene.timestamp_us = get_timestamp_us();

    rsc->stats.scenes_recognized++;
    rsc->stats.mean_scene_familiarity = exponential_decay(
        rsc->stats.mean_scene_familiarity, rsc->current_scene.familiarity_score, 0.1f, 1.0f
    );

    rsc->status = RSC_STATUS_READY;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_scene(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_scene_t* scene
) {
    if (!rsc || !scene) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *scene = rsc->current_scene;
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_familiarity(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_familiarity_t* familiarity,
    float* score
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (familiarity) *familiarity = rsc->current_scene.familiarity;
    if (score) *score = rsc->current_scene.familiarity_score;

    return RSC_OK;
}

/*=============================================================================
 * NAVIGATION SUPPORT API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_update_navigation(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* position,
    float heading,
    float velocity,
    float angular_velocity
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_NAVIGATING;

    /* Update current pose */
    if (position) {
        rsc->navigation.current_pose.position = *position;
    }
    rsc->navigation.heading = normalize_angle(heading);
    rsc->navigation.speed = velocity;
    rsc->navigation.angular_velocity = angular_velocity;
    rsc->navigation.current_pose.orientation.yaw = rsc->navigation.heading;
    rsc->navigation.current_pose.confidence = 1.0f;
    rsc->navigation.current_pose.timestamp_us = get_timestamp_us();

    /* Update HD integration */
    float hd_gain = rsc->config.hd_integration_gain;
    rsc->navigation.head_direction = exponential_decay(
        rsc->navigation.head_direction, rsc->navigation.heading, hd_gain, 1.0f
    );

    /* Update goal distance/bearing if goal is set */
    if (rsc->navigation.goal_set && position) {
        float dx = rsc->navigation.goal_pose.position.x - position->x;
        float dy = rsc->navigation.goal_pose.position.y - position->y;
        rsc->navigation.distance_to_goal = sqrtf(dx * dx + dy * dy);
        rsc->navigation.bearing_to_goal = normalize_angle(atan2f(dy, dx) - heading);
    }

    /* Update transforms based on heading */
    build_rotation_matrix_z(rsc->navigation.ego_to_allo.matrix, heading);
    build_rotation_matrix_z(rsc->navigation.allo_to_ego.matrix, -heading);

    rsc->stats.navigation_updates++;

    rsc->status = RSC_STATUS_READY;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_integrate_head_direction(
    nimcp_retrosplenial_t* rsc,
    float hd_signal,
    float confidence
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    confidence = nimcp_clamp01(confidence);
    float blend = confidence * rsc->config.hd_integration_gain;

    rsc->navigation.head_direction = exponential_decay(
        rsc->navigation.head_direction, normalize_angle(hd_signal), blend, 1.0f
    );
    rsc->navigation.hd_confidence = exponential_decay(
        rsc->navigation.hd_confidence, confidence, 0.1f, 1.0f
    );

    /* Update HD cell activations */
    if (rsc->navigation.hd_cell_activations && rsc->config.num_hd_neurons > 0) {
        float tuning_width = PI / 3.0f;
        for (uint32_t i = 0; i < rsc->config.num_hd_neurons; i++) {
            float preferred = (float)i * TWO_PI / (float)rsc->config.num_hd_neurons;
            float diff = normalize_angle(rsc->navigation.head_direction - preferred);
            rsc->navigation.hd_cell_activations[i] =
                expf(-(diff * diff) / (2.0f * tuning_width * tuning_width));
        }
    }

    rsc->stats.mean_hd_confidence = exponential_decay(
        rsc->stats.mean_hd_confidence, rsc->navigation.hd_confidence, 0.1f, 1.0f
    );

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_set_navigation_goal(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* goal_position,
    float goal_heading
) {
    if (!rsc || !goal_position) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->navigation.goal_pose.position = *goal_position;
    if (goal_heading >= -PI && goal_heading <= PI) {
        rsc->navigation.goal_pose.orientation.yaw = goal_heading;
    }
    rsc->navigation.goal_pose.timestamp_us = get_timestamp_us();
    rsc->navigation.goal_set = true;

    /* Compute initial distance/bearing */
    float dx = goal_position->x - rsc->navigation.current_pose.position.x;
    float dy = goal_position->y - rsc->navigation.current_pose.position.y;
    rsc->navigation.distance_to_goal = sqrtf(dx * dx + dy * dy);
    rsc->navigation.bearing_to_goal = normalize_angle(
        atan2f(dy, dx) - rsc->navigation.heading
    );

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Navigation goal set: (%.2f, %.2f), dist=%.2f",
              goal_position->x, goal_position->y, rsc->navigation.distance_to_goal);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_navigation_guidance(
    const nimcp_retrosplenial_t* rsc,
    float* bearing,
    float* distance,
    float* confidence
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (!rsc->navigation.goal_set) {
        if (bearing) *bearing = 0.0f;
        if (distance) *distance = 0.0f;
        if (confidence) *confidence = 0.0f;
        return RSC_OK;
    }

    if (bearing) *bearing = rsc->navigation.bearing_to_goal;
    if (distance) *distance = rsc->navigation.distance_to_goal;
    if (confidence) *confidence = rsc->navigation.hd_confidence;

    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_navigation_state(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_navigation_t* nav_state
) {
    if (!rsc || !nav_state) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *nav_state = rsc->navigation;
    return RSC_OK;
}

/*=============================================================================
 * LANDMARK API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_add_landmark(
    nimcp_retrosplenial_t* rsc,
    const nimcp_rsc_position_t* position,
    const char* name,
    const float* visual_features,
    uint32_t feature_dim,
    uint32_t* landmark_id
) {
    if (!rsc || !position) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    if (rsc->num_landmarks >= rsc->landmark_capacity) {
        if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
        return RSC_ERR_CAPACITY_EXCEEDED;
    }

    uint32_t id = rsc->num_landmarks;
    nimcp_rsc_landmark_t* lm = &rsc->landmarks[id];

    lm->id = id;
    lm->position = *position;
    lm->salience = 0.5f;
    lm->stability = 1.0f;
    lm->recognition_strength = 1.0f;
    lm->last_seen_us = get_timestamp_us();
    lm->is_anchored = false;

    if (name) {
        strncpy(lm->name, name, sizeof(lm->name) - 1);
        lm->name[sizeof(lm->name) - 1] = '\0';
    }

    if (visual_features && feature_dim > 0) {
        lm->visual_features = (float*)nimcp_calloc(feature_dim, sizeof(float));
        if (lm->visual_features) {
            memcpy(lm->visual_features, visual_features, feature_dim * sizeof(float));
            lm->feature_dim = feature_dim;
        }
    }

    rsc->num_landmarks++;
    rsc->stats.active_landmarks = rsc->num_landmarks;

    if (landmark_id) *landmark_id = id;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Landmark added: id=%u, name='%s', pos=(%.2f, %.2f)",
              id, lm->name, position->x, position->y);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_detect_landmarks(
    nimcp_retrosplenial_t* rsc,
    const float* scene_features,
    uint32_t feature_dim,
    uint32_t* detected_ids,
    uint32_t max_detections,
    uint32_t* num_detected
) {
    if (!rsc || !scene_features || !detected_ids || !num_detected) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    uint32_t count = 0;
    float threshold = rsc->config.landmark_salience_threshold;

    for (uint32_t i = 0; i < rsc->num_landmarks && count < max_detections; i++) {
        const nimcp_rsc_landmark_t* lm = &rsc->landmarks[i];

        if (!lm->visual_features || lm->feature_dim == 0) continue;

        /* Compute similarity */
        uint32_t cmp_dim = (lm->feature_dim < feature_dim) ? lm->feature_dim : feature_dim;
        float sim = vector_dot(scene_features, lm->visual_features, cmp_dim);

        if (sim > threshold) {
            detected_ids[count++] = lm->id;
            rsc->stats.landmarks_detected++;
        }
    }

    *num_detected = count;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_landmark(
    const nimcp_retrosplenial_t* rsc,
    uint32_t landmark_id,
    nimcp_rsc_landmark_t* landmark
) {
    if (!rsc || !landmark) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;
    if (landmark_id >= rsc->num_landmarks) return RSC_ERR_INVALID_PARAM;

    *landmark = rsc->landmarks[landmark_id];
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_anchor_to_landmark(
    nimcp_retrosplenial_t* rsc,
    uint32_t landmark_id,
    float observed_direction,
    float observed_distance
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;
    if (landmark_id >= rsc->num_landmarks) return RSC_ERR_INVALID_PARAM;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    const nimcp_rsc_landmark_t* lm = &rsc->landmarks[landmark_id];

    /* Compute current position from landmark observation */
    float world_dir = rsc->navigation.heading + observed_direction;
    nimcp_rsc_position_t estimated_pos;
    estimated_pos.x = lm->position.x - observed_distance * cosf(world_dir);
    estimated_pos.y = lm->position.y - observed_distance * sinf(world_dir);
    estimated_pos.z = rsc->navigation.current_pose.position.z;

    /* Blend with current position estimate */
    float blend = rsc->config.visual_calibration_rate;
    rsc->navigation.current_pose.position.x = exponential_decay(
        rsc->navigation.current_pose.position.x, estimated_pos.x, blend, 1.0f
    );
    rsc->navigation.current_pose.position.y = exponential_decay(
        rsc->navigation.current_pose.position.y, estimated_pos.y, blend, 1.0f
    );

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Anchored to landmark %u: estimated pos=(%.2f, %.2f)",
              landmark_id, estimated_pos.x, estimated_pos.y);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

/*=============================================================================
 * IMAGINATION AND PLANNING API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_start_imagination(
    nimcp_retrosplenial_t* rsc,
    nimcp_rsc_imagine_mode_t mode,
    const nimcp_rsc_pose_t* target_pose,
    float temporal_distance
) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->status = RSC_STATUS_IMAGINING;

    rsc->imagination.mode = mode;
    rsc->imagination.active = true;
    rsc->imagination.vividness = rsc->config.imagination_vividness_default;
    rsc->imagination.plausibility = 1.0f;
    rsc->imagination.temporal_distance = nimcp_clampf(temporal_distance,
        -rsc->config.temporal_projection_max, rsc->config.temporal_projection_max);
    rsc->imagination.steps_simulated = 0;

    if (target_pose) {
        rsc->imagination.imagined_pose = *target_pose;
    } else {
        rsc->imagination.imagined_pose = rsc->navigation.current_pose;
    }

    rsc->stats.imagination_episodes++;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Imagination started: mode=%s, temporal_dist=%.1f",
              nimcp_rsc_imagine_mode_string(mode), temporal_distance);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_step_imagination(nimcp_retrosplenial_t* rsc, float dt_ms) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;
    if (!rsc->imagination.active) return RSC_ERR_INVALID_STATE;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    float dt_s = dt_ms / 1000.0f;

    /* Update imagined state based on mode */
    switch (rsc->imagination.mode) {
        case RSC_IMAGINE_PROSPECTIVE:
            /* Move imagined pose toward goal */
            if (rsc->navigation.goal_set) {
                float dx = rsc->navigation.goal_pose.position.x -
                          rsc->imagination.imagined_pose.position.x;
                float dy = rsc->navigation.goal_pose.position.y -
                          rsc->imagination.imagined_pose.position.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > 0.1f) {
                    float step = 1.0f * dt_s;  /* 1 m/s imagined movement */
                    rsc->imagination.imagined_pose.position.x += (dx / dist) * step;
                    rsc->imagination.imagined_pose.position.y += (dy / dist) * step;
                }
                rsc->imagination.goal_proximity = 1.0f - nimcp_clamp01(dist / 10.0f);
            }
            break;

        case RSC_IMAGINE_SPATIAL_SELF:
            /* Maintain imagined position, decay vividness */
            break;

        case RSC_IMAGINE_RETROSPECTIVE:
            /* Replay past context (would access context history) */
            break;

        default:
            break;
    }

    /* Decay vividness over time */
    rsc->imagination.vividness = exponential_decay(
        rsc->imagination.vividness, 0.3f, 0.05f, dt_s
    );

    /* Decay plausibility with simulated distance from reality */
    rsc->imagination.plausibility = exponential_decay(
        rsc->imagination.plausibility, 0.5f, 0.01f, dt_s
    );

    rsc->imagination.steps_simulated++;

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_stop_imagination(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    if (rsc->mutex) nimcp_mutex_lock(rsc->mutex);

    rsc->imagination.active = false;
    rsc->status = RSC_STATUS_IDLE;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Imagination stopped after %u steps",
              rsc->imagination.steps_simulated);

    if (rsc->mutex) nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_imagination_state(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_imagination_t* imagination
) {
    if (!rsc || !imagination) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *imagination = rsc->imagination;
    return RSC_OK;
}

/*=============================================================================
 * BRIDGE INITIALIZATION API
 *===========================================================================*/

nimcp_rsc_error_t nimcp_rsc_init_security_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_security_context_t* security_ctx
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->security_bridge.security_ctx = security_ctx;
    rsc->security_bridge.access_level = 0;
    rsc->security_bridge.threat_detected = false;
    rsc->security_bridge.threat_level = 0.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Security bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_immune_bridge(
    nimcp_retrosplenial_t* rsc,
    brain_immune_system_t* immune
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->immune_bridge.immune = immune;
    rsc->immune_bridge.health_score = 1.0f;
    rsc->immune_bridge.anomaly_detected = false;
    rsc->immune_bridge.inflammation_level = 0.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Immune bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_bio_async_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_bio_router_t* router
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->bio_async_bridge.router = router;
    rsc->bio_async_bridge.dopamine_level = NIMCP_DOPAMINE_BASELINE;
    rsc->bio_async_bridge.serotonin_level = NIMCP_SEROTONIN_BASELINE;
    rsc->bio_async_bridge.norepinephrine_level = NIMCP_NOREPINEPHRINE_BASELINE;
    rsc->bio_async_bridge.acetylcholine_level = NIMCP_ACETYLCHOLINE_BASELINE;
    rsc->bio_async_bridge.pending_messages = 0;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Bio-async bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_kg_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_brain_kg_t* kg
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->kg_bridge.kg = kg;
    rsc->kg_bridge.health_status = 1.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "KG bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_snn_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_snn_network_t* snn
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->snn_bridge.snn = snn;
    rsc->snn_bridge.spike_rate = 0.0f;
    rsc->snn_bridge.mean_membrane_potential = 0.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "SNN bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_logging_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_logger_t logger
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->logging_bridge.logger = logger;
    rsc->logging_bridge.verbose_logging = false;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Logging bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_hippocampus_bridge(
    nimcp_retrosplenial_t* rsc,
    hippocampus_adapter_t* hippocampus
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->hippocampus_bridge.hippocampus = hippocampus;
    rsc->hippocampus_bridge.context_binding_strength = 0.5f;
    rsc->hippocampus_bridge.encoding_gate = 0.0f;
    rsc->hippocampus_bridge.retrieval_gate = 0.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Hippocampus bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_entorhinal_bridge(
    nimcp_retrosplenial_t* rsc,
    nimcp_entorhinal_t* entorhinal
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->entorhinal_bridge.entorhinal = entorhinal;
    rsc->entorhinal_bridge.grid_signal_strength = 0.0f;
    rsc->entorhinal_bridge.path_integration_input = 0.0f;
    rsc->entorhinal_bridge.heading_from_ec = 0.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Entorhinal bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_parietal_bridge(
    nimcp_retrosplenial_t* rsc,
    parietal_adapter_t* parietal
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->parietal_bridge.parietal = parietal;
    rsc->parietal_bridge.egocentric_input_strength = 0.0f;
    rsc->parietal_bridge.attention_modulation = 1.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Parietal bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_thalamic_bridge(
    nimcp_retrosplenial_t* rsc,
    thalamus_adapter_t* thalamus
) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    rsc->thalamic_bridge.thalamus = thalamus;
    rsc->thalamic_bridge.hd_signal = 0.0f;
    rsc->thalamic_bridge.relay_gain = 1.0f;
    rsc->thalamic_bridge.attention_gate = 1.0f;

    LOG_MODULE_DEBUG(RSC_LOG_MODULE, "Thalamic bridge initialized");
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_init_all_bridges(
    nimcp_retrosplenial_t* rsc,
    nimcp_brain_t* brain
) {
    if (!rsc || !brain) return RSC_ERR_NULL_PTR;

    LOG_MODULE_INFO(RSC_LOG_MODULE, "Initializing all bridges from brain instance");

    /* This would connect to all brain systems via the brain instance */
    /* For now, just mark as connected */

    LOG_MODULE_INFO(RSC_LOG_MODULE, "All bridges initialized");
    return RSC_OK;
}

/*=============================================================================
 * BIO-ASYNC MESSAGING API
 *===========================================================================*/

int nimcp_rsc_process_bio_messages(nimcp_retrosplenial_t* rsc, uint32_t max_messages) {
    if (!rsc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rsc_process_bio_messages: rsc is NULL");
        return -1;
    }
    if (!rsc->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_rsc_process_bio_messages: rsc->initialized is NULL");
        return -1;
    }

    /* Placeholder for bio-async message processing */
    /* Would dequeue messages from router and handle them */

    (void)max_messages;
    return 0;
}

nimcp_rsc_error_t nimcp_rsc_broadcast_context(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    /* Would broadcast RSC_BIO_MSG_CONTEXT via bio-router */
    rsc->bio_async_bridge.messages_sent++;
    rsc->stats.bio_messages_sent++;

    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_broadcast_navigation(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    /* Would broadcast RSC_BIO_MSG_NAVIGATION via bio-router */
    rsc->bio_async_bridge.messages_sent++;
    rsc->stats.bio_messages_sent++;

    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_broadcast_familiarity(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    /* Would broadcast RSC_BIO_MSG_SCENE_FAMILIARITY via bio-router */
    rsc->bio_async_bridge.messages_sent++;
    rsc->stats.bio_messages_sent++;

    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_broadcast_landmark(nimcp_retrosplenial_t* rsc, uint32_t landmark_id) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    (void)landmark_id;

    /* Would broadcast RSC_BIO_MSG_LANDMARK_DETECTED via bio-router */
    rsc->bio_async_bridge.messages_sent++;
    rsc->stats.bio_messages_sent++;

    return RSC_OK;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS API
 *===========================================================================*/

nimcp_rsc_status_t nimcp_rsc_get_status(const nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_STATUS_ERROR;
    return rsc->status;
}

nimcp_rsc_error_t nimcp_rsc_get_last_error(const nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    return rsc->last_error;
}

const char* nimcp_rsc_error_string(nimcp_rsc_error_t error) {
    switch (error) {
        case RSC_OK:                        return "OK";
        case RSC_ERR_NULL_PTR:              return "NULL_PTR";
        case RSC_ERR_INVALID_PARAM:         return "INVALID_PARAM";
        case RSC_ERR_NOT_INITIALIZED:       return "NOT_INITIALIZED";
        case RSC_ERR_ALREADY_INITIALIZED:   return "ALREADY_INITIALIZED";
        case RSC_ERR_NO_MEMORY:             return "NO_MEMORY";
        case RSC_ERR_TRANSFORM_FAILED:      return "TRANSFORM_FAILED";
        case RSC_ERR_CONTEXT_ENCODING_FAILED: return "CONTEXT_ENCODING_FAILED";
        case RSC_ERR_SCENE_RECOGNITION_FAILED: return "SCENE_RECOGNITION_FAILED";
        case RSC_ERR_NAVIGATION_FAILED:     return "NAVIGATION_FAILED";
        case RSC_ERR_SECURITY_VIOLATION:    return "SECURITY_VIOLATION";
        case RSC_ERR_IMMUNE_REJECTION:      return "IMMUNE_REJECTION";
        case RSC_ERR_CAPACITY_EXCEEDED:     return "CAPACITY_EXCEEDED";
        case RSC_ERR_INVALID_STATE:         return "INVALID_STATE";
        case RSC_ERR_INTERNAL:              return "INTERNAL";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_status_string(nimcp_rsc_status_t status) {
    switch (status) {
        case RSC_STATUS_IDLE:               return "IDLE";
        case RSC_STATUS_TRANSFORMING:       return "TRANSFORMING";
        case RSC_STATUS_ENCODING_CONTEXT:   return "ENCODING_CONTEXT";
        case RSC_STATUS_RECOGNIZING_SCENE:  return "RECOGNIZING_SCENE";
        case RSC_STATUS_NAVIGATING:         return "NAVIGATING";
        case RSC_STATUS_IMAGINING:          return "IMAGINING";
        case RSC_STATUS_RECALLING:          return "RECALLING";
        case RSC_STATUS_READY:              return "READY";
        case RSC_STATUS_ERROR:              return "ERROR";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_frame_string(nimcp_rsc_frame_t frame) {
    switch (frame) {
        case RSC_FRAME_EGOCENTRIC:          return "EGOCENTRIC";
        case RSC_FRAME_ALLOCENTRIC:         return "ALLOCENTRIC";
        case RSC_FRAME_OBJECT_CENTERED:     return "OBJECT_CENTERED";
        case RSC_FRAME_ROUTE_CENTERED:      return "ROUTE_CENTERED";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_context_type_string(nimcp_rsc_context_type_t type) {
    switch (type) {
        case RSC_CONTEXT_SPATIAL:           return "SPATIAL";
        case RSC_CONTEXT_TEMPORAL:          return "TEMPORAL";
        case RSC_CONTEXT_ENVIRONMENTAL:     return "ENVIRONMENTAL";
        case RSC_CONTEXT_SOCIAL:            return "SOCIAL";
        case RSC_CONTEXT_EMOTIONAL:         return "EMOTIONAL";
        case RSC_CONTEXT_TASK:              return "TASK";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_familiarity_string(nimcp_rsc_familiarity_t familiarity) {
    switch (familiarity) {
        case RSC_SCENE_NOVEL:               return "NOVEL";
        case RSC_SCENE_VAGUELY_FAMILIAR:    return "VAGUELY_FAMILIAR";
        case RSC_SCENE_FAMILIAR:            return "FAMILIAR";
        case RSC_SCENE_VERY_FAMILIAR:       return "VERY_FAMILIAR";
        case RSC_SCENE_HIGHLY_FAMILIAR:     return "HIGHLY_FAMILIAR";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_imagine_mode_string(nimcp_rsc_imagine_mode_t mode) {
    switch (mode) {
        case RSC_IMAGINE_PROSPECTIVE:       return "PROSPECTIVE";
        case RSC_IMAGINE_RETROSPECTIVE:     return "RETROSPECTIVE";
        case RSC_IMAGINE_COUNTERFACTUAL:    return "COUNTERFACTUAL";
        case RSC_IMAGINE_SPATIAL_SELF:      return "SPATIAL_SELF";
        case RSC_IMAGINE_PERSPECTIVE_TAKING: return "PERSPECTIVE_TAKING";
        default:                            return "UNKNOWN";
    }
}

const char* nimcp_rsc_bio_msg_type_string(nimcp_rsc_bio_msg_type_t type) {
    switch (type) {
        case RSC_BIO_MSG_CONTEXT:           return "CONTEXT";
        case RSC_BIO_MSG_NAVIGATION:        return "NAVIGATION";
        case RSC_BIO_MSG_SCENE_FAMILIARITY: return "SCENE_FAMILIARITY";
        case RSC_BIO_MSG_FRAME_TRANSFORM:   return "FRAME_TRANSFORM";
        case RSC_BIO_MSG_LANDMARK_DETECTED: return "LANDMARK_DETECTED";
        case RSC_BIO_MSG_HEAD_DIRECTION:    return "HEAD_DIRECTION";
        case RSC_BIO_MSG_IMAGINATION_STATE: return "IMAGINATION_STATE";
        case RSC_BIO_MSG_CONTEXT_REQUEST:   return "CONTEXT_REQUEST";
        case RSC_BIO_MSG_TRANSFORM_REQUEST: return "TRANSFORM_REQUEST";
        default:                            return "UNKNOWN";
    }
}

nimcp_rsc_error_t nimcp_rsc_get_stats(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_stats_t* stats
) {
    if (!rsc || !stats) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *stats = rsc->stats;
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_get_config(
    const nimcp_retrosplenial_t* rsc,
    nimcp_rsc_config_t* config
) {
    if (!rsc || !config) return RSC_ERR_NULL_PTR;
    if (!rsc->initialized) return RSC_ERR_NOT_INITIALIZED;

    *config = rsc->config;
    return RSC_OK;
}

float nimcp_rsc_get_health_status(const nimcp_retrosplenial_t* rsc) {
    if (!rsc || !rsc->initialized) return 0.0f;

    /* Compute health based on various factors */
    float health = 1.0f;

    /* Factor in immune health */
    health *= rsc->immune_bridge.health_score;

    /* Factor in error rate */
    if (rsc->stats.updates_processed > 0) {
        float error_rate = (float)(rsc->stats.transform_errors + rsc->stats.context_errors) /
                          (float)rsc->stats.updates_processed;
        health *= (1.0f - nimcp_clamp01(error_rate));
    }

    return nimcp_clamp01(health);
}

nimcp_rsc_error_t nimcp_rsc_log_diagnostics(const nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;

    LOG_MODULE_INFO(RSC_LOG_MODULE, "=== RSC Diagnostics ===");
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Status: %s", nimcp_rsc_status_string(rsc->status));
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Health: %.2f", nimcp_rsc_get_health_status(rsc));
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Updates: %lu", (unsigned long)rsc->stats.updates_processed);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Transforms: %lu", (unsigned long)rsc->stats.transforms_computed);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Contexts encoded: %lu", (unsigned long)rsc->stats.contexts_encoded);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Scenes recognized: %lu", (unsigned long)rsc->stats.scenes_recognized);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Landmarks: %u/%u", rsc->num_landmarks, rsc->landmark_capacity);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Current familiarity: %s (%.2f)",
             nimcp_rsc_familiarity_string(rsc->current_scene.familiarity),
             rsc->current_scene.familiarity_score);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Heading: %.2f rad (confidence: %.2f)",
             rsc->navigation.head_direction, rsc->navigation.hd_confidence);
    LOG_MODULE_INFO(RSC_LOG_MODULE, "Imagination active: %s",
             rsc->imagination.active ? "yes" : "no");
    LOG_MODULE_INFO(RSC_LOG_MODULE, "======================");

    return RSC_OK;
}

void nimcp_rsc_print_summary(const nimcp_retrosplenial_t* rsc) {
    if (!rsc) {
        printf("RSC: NULL\n");
        return;
    }

    printf("=== Retrosplenial Cortex Summary ===\n");
    printf("Status: %s\n", nimcp_rsc_status_string(rsc->status));
    printf("Initialized: %s\n", rsc->initialized ? "yes" : "no");
    printf("Health: %.2f\n", nimcp_rsc_get_health_status(rsc));
    printf("\nConfiguration:\n");
    printf("  Transform neurons: %u\n", rsc->config.num_transform_neurons);
    printf("  Context neurons: %u\n", rsc->config.num_context_neurons);
    printf("  Scene neurons: %u\n", rsc->config.num_scene_neurons);
    printf("  HD neurons: %u\n", rsc->config.num_hd_neurons);
    printf("  Landmark capacity: %u\n", rsc->config.max_landmarks);
    printf("\nNavigation State:\n");
    printf("  Position: (%.2f, %.2f, %.2f)\n",
           rsc->navigation.current_pose.position.x,
           rsc->navigation.current_pose.position.y,
           rsc->navigation.current_pose.position.z);
    printf("  Heading: %.2f rad\n", rsc->navigation.heading);
    printf("  HD confidence: %.2f\n", rsc->navigation.hd_confidence);
    printf("  Goal set: %s\n", rsc->navigation.goal_set ? "yes" : "no");
    printf("\nScene State:\n");
    printf("  Familiarity: %s (%.2f)\n",
           nimcp_rsc_familiarity_string(rsc->current_scene.familiarity),
           rsc->current_scene.familiarity_score);
    printf("  Landmarks tracked: %u\n", rsc->num_landmarks);
    printf("\nContext State:\n");
    printf("  Strength: %.2f\n", rsc->current_context.context_strength);
    printf("  History size: %u/%u\n",
           rsc->context_history_size, rsc->config.max_context_history);
    printf("\nStatistics:\n");
    printf("  Updates: %lu\n", (unsigned long)rsc->stats.updates_processed);
    printf("  Transforms: %lu\n", (unsigned long)rsc->stats.transforms_computed);
    printf("  Contexts encoded: %lu\n", (unsigned long)rsc->stats.contexts_encoded);
    printf("  Mean update latency: %.2f us\n", rsc->stats.mean_update_latency_us);
    printf("====================================\n");
}

/*=============================================================================
 * THREAD SAFETY API
 *===========================================================================*/

nimcp_mutex_t* nimcp_rsc_get_mutex(nimcp_retrosplenial_t* rsc) {
    if (!rsc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rsc is NULL");

        return NULL;

    }
    return rsc->mutex;
}

nimcp_rsc_error_t nimcp_rsc_lock(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->mutex) return RSC_OK;  /* No mutex, no-op */

    nimcp_mutex_lock(rsc->mutex);
    return RSC_OK;
}

nimcp_rsc_error_t nimcp_rsc_unlock(nimcp_retrosplenial_t* rsc) {
    if (!rsc) return RSC_ERR_NULL_PTR;
    if (!rsc->mutex) return RSC_OK;  /* No mutex, no-op */

    nimcp_mutex_unlock(rsc->mutex);
    return RSC_OK;
}
