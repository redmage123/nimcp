/**
 * @file nimcp_eagle_vision.c
 * @brief Enhanced Long-Range Pattern Detection - Implementation
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Superhuman visual processing with eagle-like acuity and spectrum sensitivity
 * WHY:  Enable enhanced pattern detection capabilities beyond human visual limits
 * HOW:  High-resolution foveal simulation, motion detection, UV spectrum processing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Eagles possess 4-8x human visual acuity through specialized foveal structures.
 * Their dual-fovea system (deep central, shallow temporal) enables simultaneous
 * sharp focus and wide-field motion detection. UV-sensitive cones expand visible
 * spectrum for prey detection (urine trails visible as UV-reflecting).
 *
 * REFERENCES:
 * - Tucker (2000) "The deep fovea, sideways vision and spiral flight"
 * - Potier et al. (2018) "Visual abilities in raptors"
 * - Hart (2001) "The visual ecology of avian photoreceptors"
 */

#include "superhuman/nimcp_eagle_vision.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eagle_vision)

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief Frame history for motion detection
 */
typedef struct {
    float* data;                /**< Frame data copy */
    uint32_t width;             /**< Frame width */
    uint32_t height;            /**< Frame height */
    float timestamp_ms;         /**< Frame timestamp */
} frame_history_entry_t;

/**
 * @brief Internal target tracking state
 */
typedef struct {
    eagle_target_t target;      /**< Public target data */
    float prediction_x;         /**< Predicted next X */
    float prediction_y;         /**< Predicted next Y */
    float velocity_history[8];  /**< Recent velocities */
    uint32_t history_idx;       /**< Circular history index */
    bool active;                /**< Tracking active */
} internal_target_t;

/**
 * @brief Eagle vision system internal structure
 */
struct eagle_vision_system {
    /* Configuration */
    eagle_vision_config_t config;

    /* State */
    eagle_vision_state_t state;
    eagle_vision_stats_t stats;

    /* Fovea simulation */
    eagle_fovea_state_t foveae[EAGLE_VISION_FOVEA_COUNT];
    float* foveal_buffer;           /**< Enhanced resolution buffer */
    uint32_t foveal_buffer_size;    /**< Buffer size in floats */

    /* Motion detection */
    frame_history_entry_t* frame_history;   /**< Circular frame buffer */
    uint32_t history_size;                  /**< History capacity */
    uint32_t history_count;                 /**< Current history count */
    uint32_t history_head;                  /**< Circular buffer head */
    eagle_motion_vector_t* motion_buffer;   /**< Motion detection buffer */
    uint32_t motion_buffer_size;            /**< Motion buffer capacity */

    /* UV processing */
    float* uv_buffer;               /**< UV channel buffer */
    uint32_t uv_buffer_size;        /**< UV buffer size */
    float uv_baseline;              /**< Baseline UV level */

    /* Target tracking */
    internal_target_t* targets;     /**< Target tracking array */
    uint32_t max_targets;           /**< Maximum targets */
    uint32_t next_target_id;        /**< Next target ID */

    /* Pattern detection */
    eagle_pattern_result_t* pattern_buffer; /**< Pattern detection buffer */
    uint32_t pattern_buffer_size;   /**< Pattern buffer capacity */

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t last_frame_time_ms;
    float last_delta_ms;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Track temporal dynamics
 * HOW:  Use CLOCK_MONOTONIC for stable timing
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * WHAT: Compute Euclidean distance between points
 * WHY:  Target tracking and motion analysis
 * HOW:  Standard 2D distance formula
 */
static inline float point_distance(eagle_point_t a, eagle_point_t b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

/**
 * WHAT: Initialize fovea state
 * WHY:  Set up dual-fovea simulation
 * HOW:  Configure each fovea with biological parameters
 */
static void init_fovea(eagle_fovea_state_t* fovea, eagle_fovea_type_t type) {
    if (!fovea) return;

    fovea->type = type;
    fovea->gaze_point.x = 0.5f;
    fovea->gaze_point.y = 0.5f;

    if (type == EAGLE_FOVEA_CENTRAL) {
        /* Deep central fovea - narrow FOV, high acuity */
        fovea->field_of_view = 15.0f;
        fovea->acuity_current = EAGLE_VISION_DEFAULT_ACUITY;
        fovea->depth_factor = 1.0f;
        fovea->active_neurons = 500000;
    } else {
        /* Shallow temporal fovea - wider FOV, motion sensitivity */
        fovea->field_of_view = 45.0f;
        fovea->acuity_current = EAGLE_VISION_DEFAULT_ACUITY * 0.5f;
        fovea->depth_factor = 0.5f;
        fovea->active_neurons = 300000;
    }
}

/**
 * WHAT: Allocate frame history buffer
 * WHY:  Motion detection requires temporal context
 * HOW:  Circular buffer of frame data
 */
static int alloc_frame_history(eagle_vision_system_t* sys, uint32_t size) {
    sys->frame_history = nimcp_calloc(size, sizeof(frame_history_entry_t));
    if (!sys->frame_history) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, size * sizeof(frame_history_entry_t),
                           "alloc_frame_history: Failed to allocate frame history");
        return EAGLE_VISION_ERROR_NO_MEMORY;
    }

    sys->history_size = size;
    sys->history_count = 0;
    sys->history_head = 0;
    return EAGLE_VISION_SUCCESS;
}

/**
 * WHAT: Free frame history buffer
 * WHY:  Clean resource release
 * HOW:  Free each entry then the array
 */
static void free_frame_history(eagle_vision_system_t* sys) {
    if (!sys->frame_history) return;

    for (uint32_t i = 0; i < sys->history_size; i++) {
        if (sys->frame_history[i].data) {
            nimcp_free(sys->frame_history[i].data);
        }
    }
    nimcp_free(sys->frame_history);
    sys->frame_history = NULL;
    sys->history_size = 0;
    sys->history_count = 0;
}

/**
 * WHAT: Add frame to history buffer
 * WHY:  Build temporal context for motion detection
 * HOW:  Copy frame data to circular buffer slot
 */
static int add_frame_to_history(eagle_vision_system_t* sys,
                                const eagle_vision_input_t* input) {
    if (!sys || !input || !input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "add_frame_to_history: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    frame_history_entry_t* entry = &sys->frame_history[sys->history_head];

    /* Compute required size */
    uint32_t required_size = input->width * input->height * input->channels;

    /* Reallocate if needed */
    if (!entry->data || entry->width != input->width ||
        entry->height != input->height) {
        if (entry->data) nimcp_free(entry->data);
        entry->data = nimcp_malloc(required_size * sizeof(float));
        if (!entry->data) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, required_size * sizeof(float),
                               "add_frame_to_history: Failed to allocate frame data");
            return EAGLE_VISION_ERROR_NO_MEMORY;
        }
    }

    /* Copy frame data */
    memcpy(entry->data, input->data, required_size * sizeof(float));
    entry->width = input->width;
    entry->height = input->height;
    entry->timestamp_ms = input->timestamp_ms;

    /* Advance circular buffer */
    sys->history_head = (sys->history_head + 1) % sys->history_size;
    if (sys->history_count < sys->history_size) {
        sys->history_count++;
    }

    return EAGLE_VISION_SUCCESS;
}

/**
 * WHAT: Compute foveal enhancement for region
 * WHY:  Simulate high-acuity foveal processing
 * HOW:  Apply acuity multiplier based on distance from gaze point
 */
static float compute_foveal_weight(const eagle_fovea_state_t* fovea,
                                   float x, float y) {
    if (!fovea) return 1.0f;

    /* Distance from gaze center (normalized) */
    float dx = x - fovea->gaze_point.x;
    float dy = y - fovea->gaze_point.y;
    float dist = sqrtf(dx * dx + dy * dy);

    /* Convert FOV to normalized radius */
    float fov_radius = fovea->field_of_view / 180.0f;

    /* Weight falls off outside foveal region */
    if (dist < fov_radius * 0.5f) {
        return fovea->acuity_current;
    } else if (dist < fov_radius) {
        float t = (dist - fov_radius * 0.5f) / (fov_radius * 0.5f);
        return fovea->acuity_current * (1.0f - t * 0.5f);
    } else {
        return fovea->acuity_current * 0.5f;
    }
}

/**
 * WHAT: Find best matching target for position
 * WHY:  Associate detections with tracked targets
 * HOW:  Nearest neighbor search with prediction
 */
static internal_target_t* find_matching_target(eagle_vision_system_t* sys,
                                               eagle_point_t position,
                                               float max_distance) {
    internal_target_t* best = NULL;
    float best_dist = max_distance;

    for (uint32_t i = 0; i < sys->max_targets; i++) {
        if (!sys->targets[i].active) continue;

        /* Check against predicted position */
        eagle_point_t pred = {
            sys->targets[i].prediction_x,
            sys->targets[i].prediction_y
        };

        float dist = point_distance(position, pred);
        if (dist < best_dist) {
            best_dist = dist;
            best = &sys->targets[i];
        }
    }

    return best;
}

/**
 * WHAT: Allocate new target slot
 * WHY:  Track newly detected objects
 * HOW:  Find inactive slot, initialize tracking
 */
static internal_target_t* allocate_target(eagle_vision_system_t* sys,
                                          eagle_point_t position) {
    for (uint32_t i = 0; i < sys->max_targets; i++) {
        if (!sys->targets[i].active) {
            internal_target_t* t = &sys->targets[i];
            memset(t, 0, sizeof(internal_target_t));

            t->target.target_id = sys->next_target_id++;
            t->target.position = position;
            t->target.confidence = 1.0f;
            t->target.priority = EAGLE_TARGET_PRIORITY_LOW;
            t->target.frames_tracked = 1;
            t->prediction_x = position.x;
            t->prediction_y = position.y;
            t->active = true;

            return t;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_frame_history: operation failed");
    return NULL;
}

/**
 * WHAT: Update target prediction
 * WHY:  Anticipate target movement for tracking
 * HOW:  Kalman-like velocity estimation
 */
static void update_target_prediction(internal_target_t* t) {
    if (!t || !t->active) return;

    /* Simple velocity averaging */
    float avg_vx = 0.0f, avg_vy = 0.0f;
    for (int i = 0; i < 8; i++) {
        avg_vx += t->velocity_history[i];
    }
    avg_vx /= 8.0f;

    /* Predict next position */
    t->prediction_x = t->target.position.x + avg_vx;
    t->prediction_y = t->target.position.y + t->target.motion.velocity_y;

    /* Clamp to valid range */
    t->prediction_x = nimcp_clampf(t->prediction_x, 0.0f, 1.0f);
    t->prediction_y = nimcp_clampf(t->prediction_y, 0.0f, 1.0f);
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int eagle_vision_default_config(eagle_vision_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_default_config: config is NULL");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    /* Acuity settings */
    config->acuity_multiplier = EAGLE_VISION_DEFAULT_ACUITY;
    config->enable_dual_fovea = true;
    config->central_fovea_weight = 0.7f;
    config->temporal_fovea_weight = 0.3f;

    /* Motion detection */
    config->enable_motion_detection = true;
    config->motion_sensitivity = EAGLE_MOTION_HIGH;
    config->motion_threshold = EAGLE_VISION_MOTION_THRESHOLD;
    config->motion_history_frames = EAGLE_VISION_HISTORY_SIZE;

    /* UV spectrum */
    config->enable_uv_sensitivity = true;
    config->spectrum_mode = EAGLE_SPECTRUM_UV_ENHANCED;
    config->uv_sensitivity_factor = 1.0f;

    /* Pattern detection */
    config->enable_pattern_detection = true;
    config->pattern_threshold = 0.5f;
    config->max_patterns = 32;

    /* Target tracking */
    config->enable_target_tracking = true;
    config->max_targets = EAGLE_VISION_MAX_TARGETS;
    config->target_persistence_frames = 30.0f;

    /* Performance */
    config->enable_parallel_processing = false;
    config->processing_threads = 1;

    return EAGLE_VISION_SUCCESS;
}

eagle_vision_system_t* eagle_vision_create(const eagle_vision_config_t* config) {
    eagle_vision_system_t* sys = nimcp_calloc(1, sizeof(eagle_vision_system_t));
    if (!sys) {
        NIMCP_LOGGING_ERROR("Failed to allocate eagle vision system");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(eagle_vision_system_t),
                           "eagle_vision_create: Failed to allocate system");
        return NULL;
    }

    /* Apply configuration */
    eagle_vision_config_t default_cfg;
    if (!config) {
        eagle_vision_default_config(&default_cfg);
        config = &default_cfg;
    }
    sys->config = *config;

    /* Initialize foveae */
    init_fovea(&sys->foveae[0], EAGLE_FOVEA_CENTRAL);
    init_fovea(&sys->foveae[1], EAGLE_FOVEA_TEMPORAL);
    sys->state.foveae[0] = sys->foveae[0];
    sys->state.foveae[1] = sys->foveae[1];

    /* Allocate frame history */
    if (alloc_frame_history(sys, config->motion_history_frames) != 0) {
        NIMCP_LOGGING_ERROR("Failed to allocate frame history");
        /* Exception already thrown in alloc_frame_history */
        eagle_vision_destroy(sys);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "eagle_vision_create: validation failed");
        return NULL;
    }

    /* Allocate motion buffer */
    sys->motion_buffer_size = 1024;
    sys->motion_buffer = nimcp_calloc(sys->motion_buffer_size,
                                      sizeof(eagle_motion_vector_t));
    if (!sys->motion_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate motion buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->motion_buffer_size * sizeof(eagle_motion_vector_t),
                           "eagle_vision_create: Failed to allocate motion buffer");
        eagle_vision_destroy(sys);
        return NULL;
    }

    /* Allocate target tracking */
    sys->max_targets = config->max_targets;
    sys->targets = nimcp_calloc(sys->max_targets, sizeof(internal_target_t));
    if (!sys->targets) {
        NIMCP_LOGGING_ERROR("Failed to allocate target tracking");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->max_targets * sizeof(internal_target_t),
                           "eagle_vision_create: Failed to allocate target tracking");
        eagle_vision_destroy(sys);
        return NULL;
    }
    sys->next_target_id = 1;

    /* Allocate pattern buffer */
    sys->pattern_buffer_size = config->max_patterns;
    sys->pattern_buffer = nimcp_calloc(sys->pattern_buffer_size,
                                       sizeof(eagle_pattern_result_t));
    if (!sys->pattern_buffer) {
        NIMCP_LOGGING_ERROR("Failed to allocate pattern buffer");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY,
                           sys->pattern_buffer_size * sizeof(eagle_pattern_result_t),
                           "eagle_vision_create: Failed to allocate pattern buffer");
        eagle_vision_destroy(sys);
        return NULL;
    }

    /* Create mutex */
    sys->mutex = nimcp_platform_mutex_create();
    if (!sys->mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        NIMCP_THROW_THREADING(NIMCP_ERROR_THREAD_CREATE, 0,
                              "eagle_vision_create: Failed to create mutex%s", "");
        eagle_vision_destroy(sys);
        return NULL;
    }

    /* Initialize state */
    sys->state.current_acuity = config->acuity_multiplier;
    sys->state.is_initialized = true;
    sys->last_frame_time_ms = get_time_ms();

    NIMCP_LOGGING_INFO("Eagle vision system created with acuity %.1fx",
                       config->acuity_multiplier);
    return sys;
}

void eagle_vision_destroy(eagle_vision_system_t* system) {
    if (!system) return;

    /* Free frame history */
    free_frame_history(system);

    /* Free buffers */
    if (system->foveal_buffer) nimcp_free(system->foveal_buffer);
    if (system->motion_buffer) nimcp_free(system->motion_buffer);
    if (system->uv_buffer) nimcp_free(system->uv_buffer);
    if (system->targets) nimcp_free(system->targets);
    if (system->pattern_buffer) nimcp_free(system->pattern_buffer);

    /* Destroy mutex */
    if (system->mutex) {
        nimcp_platform_mutex_destroy(system->mutex);
        nimcp_free(system->mutex);
        system->mutex = NULL;
    }

    nimcp_free(system);
    NIMCP_LOGGING_INFO("Eagle vision system destroyed");
}

int eagle_vision_reset(eagle_vision_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_reset: system is NULL");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);

    /* Reset state */
    system->state.active_motion_vectors = 0;
    system->state.max_motion_magnitude = 0.0f;
    system->state.avg_motion_magnitude = 0.0f;
    system->state.uv_ambient_level = 0.0f;
    system->state.uv_trails_detected = 0;
    system->state.active_targets = 0;
    system->state.high_priority_targets = 0;
    system->state.new_targets_this_frame = 0;
    system->state.lost_targets_this_frame = 0;
    system->state.patterns_detected = 0;
    system->state.avg_pattern_confidence = 0.0f;
    system->state.frames_processed = 0;
    system->state.processing_load = 0.0f;

    /* Reset foveae */
    init_fovea(&system->foveae[0], EAGLE_FOVEA_CENTRAL);
    init_fovea(&system->foveae[1], EAGLE_FOVEA_TEMPORAL);

    /* Clear frame history */
    system->history_count = 0;
    system->history_head = 0;

    /* Clear targets */
    for (uint32_t i = 0; i < system->max_targets; i++) {
        system->targets[i].active = false;
    }

    nimcp_platform_mutex_unlock(system->mutex);

    NIMCP_LOGGING_DEBUG("Eagle vision system reset");
    return EAGLE_VISION_SUCCESS;
}

/* ============================================================================
 * Configuration Implementation
 * ============================================================================ */

int eagle_vision_set_config(eagle_vision_system_t* system,
                            const eagle_vision_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_set_config: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    system->config = *config;

    /* Update fovea acuity */
    system->foveae[0].acuity_current = config->acuity_multiplier;
    system->foveae[1].acuity_current = config->acuity_multiplier * 0.5f;
    system->state.current_acuity = config->acuity_multiplier;

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_get_config(const eagle_vision_system_t* system,
                            eagle_vision_config_t* config) {
    if (!system || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_get_config: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *config = system->config;
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_set_acuity(eagle_vision_system_t* system, float acuity) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_set_acuity: system is NULL");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    acuity = nimcp_clampf(acuity, EAGLE_VISION_MIN_ACUITY, EAGLE_VISION_MAX_ACUITY);

    nimcp_platform_mutex_lock(system->mutex);

    system->config.acuity_multiplier = acuity;
    system->foveae[0].acuity_current = acuity;
    system->foveae[1].acuity_current = acuity * 0.5f;
    system->state.current_acuity = acuity;
    system->stats.acuity_adjustments++;

    if (acuity > system->stats.peak_acuity_achieved) {
        system->stats.peak_acuity_achieved = acuity;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_set_gaze(eagle_vision_system_t* system,
                          eagle_fovea_type_t fovea,
                          eagle_point_t point) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_set_gaze: system is NULL");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    /* Clamp point to valid range */
    point.x = nimcp_clampf(point.x, 0.0f, 1.0f);
    point.y = nimcp_clampf(point.y, 0.0f, 1.0f);

    nimcp_platform_mutex_lock(system->mutex);

    if (fovea == EAGLE_FOVEA_CENTRAL || fovea == EAGLE_FOVEA_BOTH) {
        system->foveae[0].gaze_point = point;
    }
    if (fovea == EAGLE_FOVEA_TEMPORAL || fovea == EAGLE_FOVEA_BOTH) {
        system->foveae[1].gaze_point = point;
    }

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

/* ============================================================================
 * Processing Implementation
 * ============================================================================ */

int eagle_vision_process_frame(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_vision_output_t* output) {
    if (!system || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_process_frame: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }
    if (!input->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "eagle_vision_process_frame: input->data is NULL");
        return EAGLE_VISION_ERROR_NO_INPUT;
    }

    uint64_t start_time = get_time_ms();

    nimcp_platform_mutex_lock(system->mutex);

    /* Add frame to history for motion detection */
    add_frame_to_history(system, input);

    /* Reset per-frame state */
    system->state.new_targets_this_frame = 0;
    system->state.lost_targets_this_frame = 0;
    output->num_targets = 0;
    output->num_motion_vectors = 0;
    output->num_patterns = 0;

    nimcp_platform_mutex_unlock(system->mutex);

    /* Motion detection */
    if (system->config.enable_motion_detection) {
        eagle_vision_detect_motion(system, input,
                                   output->motion_vectors,
                                   output->max_motion_vectors,
                                   &output->num_motion_vectors);
    }

    /* UV processing */
    if (system->config.enable_uv_sensitivity && input->has_uv_channel) {
        eagle_vision_process_uv(system, input, &output->uv_result);
    }

    /* Pattern detection */
    if (system->config.enable_pattern_detection) {
        eagle_vision_detect_patterns(system, input,
                                     output->patterns,
                                     output->max_patterns,
                                     &output->num_patterns);
    }

    /* Target tracking */
    if (system->config.enable_target_tracking) {
        eagle_vision_track_targets(system, input,
                                   output->targets,
                                   output->max_targets,
                                   &output->num_targets);
    }

    /* Update statistics */
    nimcp_platform_mutex_lock(system->mutex);

    uint64_t end_time = get_time_ms();
    float processing_time = (float)(end_time - start_time);

    output->processing_time_ms = processing_time;
    output->frame_number = system->state.frames_processed;

    system->state.frames_processed++;
    system->stats.total_frames_processed++;
    system->stats.avg_processing_time_ms =
        system->stats.avg_processing_time_ms * 0.99f + processing_time * 0.01f;
    if (processing_time > system->stats.max_processing_time_ms) {
        system->stats.max_processing_time_ms = processing_time;
    }

    system->last_frame_time_ms = end_time;
    system->last_delta_ms = processing_time;

    nimcp_platform_mutex_unlock(system->mutex);

    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_detect_motion(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_motion_vector_t* vectors,
                               uint32_t max_vectors,
                               uint32_t* num_vectors) {
    if (!system || !input || !vectors || !num_vectors) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_detect_motion: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *num_vectors = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Need at least 2 frames for motion detection */
    if (system->history_count < 2) {
        nimcp_platform_mutex_unlock(system->mutex);
        return EAGLE_VISION_SUCCESS;
    }

    /* Get previous frame */
    uint32_t prev_idx = (system->history_head + system->history_size - 2) %
                        system->history_size;
    frame_history_entry_t* prev = &system->frame_history[prev_idx];

    /* Simple block-based motion detection */
    uint32_t block_size = 16;
    uint32_t blocks_x = input->width / block_size;
    uint32_t blocks_y = input->height / block_size;

    float motion_threshold = system->config.motion_threshold;
    switch (system->config.motion_sensitivity) {
        case EAGLE_MOTION_LOW:    motion_threshold *= 4.0f; break;
        case EAGLE_MOTION_MEDIUM: motion_threshold *= 2.0f; break;
        case EAGLE_MOTION_HIGH:   motion_threshold *= 1.0f; break;
        case EAGLE_MOTION_ULTRA:  motion_threshold *= 0.25f; break;
    }

    float max_magnitude = 0.0f;
    float total_magnitude = 0.0f;
    uint32_t motion_count = 0;

    for (uint32_t by = 0; by < blocks_y && *num_vectors < max_vectors; by++) {
        for (uint32_t bx = 0; bx < blocks_x && *num_vectors < max_vectors; bx++) {
            /* Compute block difference */
            float diff = 0.0f;
            float flow_x = 0.0f, flow_y = 0.0f;

            for (uint32_t y = 0; y < block_size && (by * block_size + y) < input->height; y++) {
                for (uint32_t x = 0; x < block_size && (bx * block_size + x) < input->width; x++) {
                    uint32_t px = bx * block_size + x;
                    uint32_t py = by * block_size + y;
                    uint32_t idx = py * input->width + px;

                    if (idx < input->width * input->height && prev->data) {
                        float curr_val = input->data[idx * input->channels];
                        float prev_val = prev->data[idx * input->channels];
                        float d = fabsf(curr_val - prev_val);
                        diff += d;

                        /* Simple optical flow approximation */
                        if (d > motion_threshold) {
                            flow_x += (x - block_size / 2) * d;
                            flow_y += (y - block_size / 2) * d;
                        }
                    }
                }
            }

            diff /= (float)(block_size * block_size);

            /* Apply foveal weighting */
            float norm_x = (float)(bx * block_size + block_size / 2) / (float)input->width;
            float norm_y = (float)(by * block_size + block_size / 2) / (float)input->height;
            float foveal_weight = compute_foveal_weight(&system->foveae[1], norm_x, norm_y);
            diff *= foveal_weight;

            if (diff > motion_threshold) {
                eagle_motion_vector_t* mv = &vectors[*num_vectors];

                mv->position.x = norm_x;
                mv->position.y = norm_y;

                float flow_norm = sqrtf(flow_x * flow_x + flow_y * flow_y);
                if (flow_norm > 1e-6f) {
                    mv->velocity_x = flow_x / flow_norm * diff;
                    mv->velocity_y = flow_y / flow_norm * diff;
                } else {
                    mv->velocity_x = 0.0f;
                    mv->velocity_y = 0.0f;
                }

                mv->magnitude = diff;
                mv->direction = atan2f(mv->velocity_y, mv->velocity_x);
                mv->confidence = nimcp_clampf(diff / (motion_threshold * 4.0f), 0.0f, 1.0f);

                (*num_vectors)++;
                motion_count++;
                total_magnitude += diff;
                if (diff > max_magnitude) max_magnitude = diff;
            }
        }
    }

    /* Update state */
    system->state.active_motion_vectors = *num_vectors;
    system->state.max_motion_magnitude = max_magnitude;
    system->state.avg_motion_magnitude = motion_count > 0 ?
                                         total_magnitude / motion_count : 0.0f;
    system->stats.total_motion_events += *num_vectors;

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_process_uv(eagle_vision_system_t* system,
                            const eagle_vision_input_t* input,
                            eagle_uv_result_t* result) {
    if (!system || !input || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_process_uv: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }
    if (!input->has_uv_channel || input->channels < 4) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "eagle_vision_process_uv: UV channel not available");
        return EAGLE_VISION_ERROR_INVALID_PARAM;
    }

    nimcp_platform_mutex_lock(system->mutex);

    memset(result, 0, sizeof(eagle_uv_result_t));

    /* Process UV channel (assumed to be channel 3) */
    float total_uv = 0.0f;
    float max_uv = 0.0f;
    uint32_t uv_pixels = 0;

    /* Simple UV trail detection - find connected high-UV regions */
    float trail_threshold = 0.3f * system->config.uv_sensitivity_factor;
    float min_trail_x = 1.0f, max_trail_x = 0.0f;
    float min_trail_y = 1.0f, max_trail_y = 0.0f;
    bool found_trail = false;

    for (uint32_t y = 0; y < input->height; y++) {
        for (uint32_t x = 0; x < input->width; x++) {
            uint32_t idx = (y * input->width + x) * input->channels + 3;
            float uv_val = input->data[idx] * system->config.uv_sensitivity_factor;

            total_uv += uv_val;
            uv_pixels++;
            if (uv_val > max_uv) max_uv = uv_val;

            /* Trail detection */
            if (uv_val > trail_threshold) {
                float norm_x = (float)x / (float)input->width;
                float norm_y = (float)y / (float)input->height;

                if (norm_x < min_trail_x) min_trail_x = norm_x;
                if (norm_x > max_trail_x) max_trail_x = norm_x;
                if (norm_y < min_trail_y) min_trail_y = norm_y;
                if (norm_y > max_trail_y) max_trail_y = norm_y;
                found_trail = true;
            }
        }
    }

    result->uv_intensity = uv_pixels > 0 ? total_uv / uv_pixels : 0.0f;
    result->wavelength_peak = (float)(EAGLE_VISION_UV_WAVELENGTH_MIN +
                              (EAGLE_VISION_UV_WAVELENGTH_MAX - EAGLE_VISION_UV_WAVELENGTH_MIN) *
                              (1.0f - result->uv_intensity));
    result->contrast_boost = 1.0f + result->uv_intensity * 0.5f;

    if (found_trail && (max_trail_x - min_trail_x) > 0.05f) {
        result->trail_detected = true;
        result->trail_start.x = min_trail_x;
        result->trail_start.y = (min_trail_y + max_trail_y) * 0.5f;
        result->trail_end.x = max_trail_x;
        result->trail_end.y = (min_trail_y + max_trail_y) * 0.5f;

        system->state.uv_trails_detected++;
        system->stats.total_uv_trails++;
    }

    system->state.uv_ambient_level = result->uv_intensity;

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_detect_patterns(eagle_vision_system_t* system,
                                 const eagle_vision_input_t* input,
                                 eagle_pattern_result_t* patterns,
                                 uint32_t max_patterns,
                                 uint32_t* num_patterns) {
    if (!system || !input || !patterns || !num_patterns) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_detect_patterns: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *num_patterns = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Pattern detection with foveal enhancement */
    /* This is a simplified implementation - real system would use CNNs */

    float threshold = system->config.pattern_threshold;
    float total_confidence = 0.0f;

    /* Simple edge-based pattern detection */
    uint32_t window = 8;
    for (uint32_t y = window; y < input->height - window && *num_patterns < max_patterns; y += window) {
        for (uint32_t x = window; x < input->width - window && *num_patterns < max_patterns; x += window) {
            /* Compute local variance as pattern indicator */
            float sum = 0.0f, sum_sq = 0.0f;
            uint32_t count = 0;

            for (uint32_t dy = 0; dy < window; dy++) {
                for (uint32_t dx = 0; dx < window; dx++) {
                    uint32_t idx = ((y + dy) * input->width + (x + dx)) * input->channels;
                    float val = input->data[idx];
                    sum += val;
                    sum_sq += val * val;
                    count++;
                }
            }

            float mean = sum / count;
            float variance = (sum_sq / count) - (mean * mean);

            /* Apply foveal enhancement */
            float norm_x = (float)x / (float)input->width;
            float norm_y = (float)y / (float)input->height;
            float acuity = compute_foveal_weight(&system->foveae[0], norm_x, norm_y);
            variance *= acuity;

            /* High variance regions indicate patterns */
            if (variance > threshold * 0.1f) {
                eagle_pattern_result_t* p = &patterns[*num_patterns];

                p->pattern_id = *num_patterns + 1;
                p->match_confidence = nimcp_clampf(variance, 0.0f, 1.0f);
                p->location.x = norm_x;
                p->location.y = norm_y;
                p->scale = 1.0f / acuity;
                p->rotation = 0.0f;
                p->pattern_name = "edge_cluster";

                total_confidence += p->match_confidence;
                (*num_patterns)++;
            }
        }
    }

    /* Update state */
    system->state.patterns_detected = *num_patterns;
    system->state.avg_pattern_confidence = *num_patterns > 0 ?
                                           total_confidence / *num_patterns : 0.0f;
    system->stats.total_patterns_detected += *num_patterns;
    system->stats.avg_pattern_confidence =
        system->stats.avg_pattern_confidence * 0.99f +
        system->state.avg_pattern_confidence * 0.01f;

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_track_targets(eagle_vision_system_t* system,
                               const eagle_vision_input_t* input,
                               eagle_target_t* targets,
                               uint32_t max_targets,
                               uint32_t* num_targets) {
    if (!system || !input || !targets || !num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_track_targets: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *num_targets = 0;

    nimcp_platform_mutex_lock(system->mutex);

    /* Age all existing targets */
    uint32_t active_count = 0;
    uint32_t high_priority_count = 0;

    for (uint32_t i = 0; i < system->max_targets; i++) {
        if (!system->targets[i].active) continue;

        internal_target_t* t = &system->targets[i];

        /* Decay confidence over time */
        t->target.confidence *= 0.95f;

        /* Deactivate if confidence too low */
        if (t->target.confidence < 0.1f) {
            t->active = false;
            system->state.lost_targets_this_frame++;
            continue;
        }

        /* Update prediction */
        update_target_prediction(t);

        active_count++;
        if (t->target.priority >= EAGLE_TARGET_PRIORITY_HIGH) {
            high_priority_count++;
        }
    }

    /* Detect new targets from motion vectors */
    if (system->state.active_motion_vectors > 0) {
        for (uint32_t i = 0; i < system->motion_buffer_size && i < system->state.active_motion_vectors; i++) {
            eagle_motion_vector_t* mv = &system->motion_buffer[i];

            /* Try to match to existing target */
            internal_target_t* match = find_matching_target(system, mv->position, 0.1f);

            if (match) {
                /* Update existing target */
                match->target.position = mv->position;
                match->target.confidence = nimcp_clampf(match->target.confidence + 0.1f, 0.0f, 1.0f);
                match->target.motion = *mv;
                match->target.is_moving = true;
                match->target.frames_tracked++;

                /* Update velocity history */
                match->velocity_history[match->history_idx] = mv->velocity_x;
                match->history_idx = (match->history_idx + 1) % 8;

                /* Promote priority if consistently tracked */
                if (match->target.frames_tracked > 10 &&
                    match->target.priority < EAGLE_TARGET_PRIORITY_MEDIUM) {
                    match->target.priority = EAGLE_TARGET_PRIORITY_MEDIUM;
                }
                if (match->target.frames_tracked > 30 &&
                    mv->magnitude > 0.1f &&
                    match->target.priority < EAGLE_TARGET_PRIORITY_HIGH) {
                    match->target.priority = EAGLE_TARGET_PRIORITY_HIGH;
                    high_priority_count++;
                }
            } else if (active_count < system->max_targets && mv->confidence > 0.5f) {
                /* Create new target */
                internal_target_t* new_target = allocate_target(system, mv->position);
                if (new_target) {
                    new_target->target.motion = *mv;
                    new_target->target.is_moving = true;
                    active_count++;
                    system->state.new_targets_this_frame++;
                    system->stats.total_targets_detected++;
                }
            }
        }
    }

    /* Copy active targets to output */
    for (uint32_t i = 0; i < system->max_targets && *num_targets < max_targets; i++) {
        if (system->targets[i].active) {
            targets[*num_targets] = system->targets[i].target;
            (*num_targets)++;
        }
    }

    /* Update state */
    system->state.active_targets = active_count;
    system->state.high_priority_targets = high_priority_count;

    /* Update average confidence */
    float total_conf = 0.0f;
    for (uint32_t i = 0; i < *num_targets; i++) {
        total_conf += targets[i].confidence;
    }
    system->stats.avg_target_confidence =
        system->stats.avg_target_confidence * 0.99f +
        (*num_targets > 0 ? total_conf / *num_targets : 0.0f) * 0.01f;

    nimcp_platform_mutex_unlock(system->mutex);
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_estimate_distance(eagle_vision_system_t* system,
                                   const eagle_target_t* target,
                                   float* distance) {
    if (!system || !target || !distance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_estimate_distance: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    /* Simple distance estimation based on apparent size and motion parallax */
    /* Real system would use stereo vision or learned priors */

    float size_factor = 1.0f / (target->size_estimate + 0.01f);
    float motion_factor = 1.0f;

    if (target->is_moving && target->motion.magnitude > 0.01f) {
        /* Objects moving faster at same apparent speed are closer */
        motion_factor = target->motion.magnitude * 2.0f;
    }

    *distance = size_factor * motion_factor;
    *distance = nimcp_clampf(*distance, 0.1f, 100.0f);

    return EAGLE_VISION_SUCCESS;
}

/* ============================================================================
 * State and Statistics Implementation
 * ============================================================================ */

int eagle_vision_get_state(const eagle_vision_system_t* system,
                           eagle_vision_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_get_state: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *state = system->state;
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_get_stats(const eagle_vision_system_t* system,
                           eagle_vision_stats_t* stats) {
    if (!system || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_get_stats: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    *stats = system->stats;
    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_reset_stats(eagle_vision_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_reset_stats: system is NULL");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(system->mutex);
    memset(&system->stats, 0, sizeof(eagle_vision_stats_t));
    nimcp_platform_mutex_unlock(system->mutex);

    return EAGLE_VISION_SUCCESS;
}

int eagle_vision_get_fovea_state(const eagle_vision_system_t* system,
                                 eagle_fovea_type_t fovea,
                                 eagle_fovea_state_t* state) {
    if (!system || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "eagle_vision_get_fovea_state: NULL parameter");
        return EAGLE_VISION_ERROR_NULL_POINTER;
    }
    if (fovea == EAGLE_FOVEA_BOTH) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                              "eagle_vision_get_fovea_state: EAGLE_FOVEA_BOTH not allowed");
        return EAGLE_VISION_ERROR_INVALID_PARAM;
    }

    uint32_t idx = (fovea == EAGLE_FOVEA_CENTRAL) ? 0 : 1;
    *state = system->foveae[idx];

    return EAGLE_VISION_SUCCESS;
}

/* ============================================================================
 * Utility Implementation
 * ============================================================================ */

eagle_vision_output_t* eagle_vision_output_create(uint32_t max_targets,
                                                  uint32_t max_motions,
                                                  uint32_t max_patterns) {
    eagle_vision_output_t* output = nimcp_calloc(1, sizeof(eagle_vision_output_t));
    if (!output) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(eagle_vision_output_t),
                           "eagle_vision_output_create: Failed to allocate output");
        return NULL;
    }

    output->targets = nimcp_calloc(max_targets, sizeof(eagle_target_t));
    output->motion_vectors = nimcp_calloc(max_motions, sizeof(eagle_motion_vector_t));
    output->patterns = nimcp_calloc(max_patterns, sizeof(eagle_pattern_result_t));

    if (!output->targets || !output->motion_vectors || !output->patterns) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, 0,
                           "eagle_vision_output_create: Failed to allocate buffers");
        eagle_vision_output_destroy(output);
        return NULL;
    }

    output->max_targets = max_targets;
    output->max_motion_vectors = max_motions;
    output->max_patterns = max_patterns;

    return output;
}

void eagle_vision_output_destroy(eagle_vision_output_t* output) {
    if (!output) return;

    if (output->targets) nimcp_free(output->targets);
    if (output->motion_vectors) nimcp_free(output->motion_vectors);
    if (output->patterns) nimcp_free(output->patterns);
    nimcp_free(output);
}

const char* eagle_vision_error_string(eagle_vision_error_t error) {
    switch (error) {
        case EAGLE_VISION_SUCCESS:               return "Success";
        case EAGLE_VISION_ERROR_NULL_POINTER:    return "Null pointer";
        case EAGLE_VISION_ERROR_INVALID_PARAM:   return "Invalid parameter";
        case EAGLE_VISION_ERROR_NO_MEMORY:       return "Memory allocation failed";
        case EAGLE_VISION_ERROR_NOT_INITIALIZED: return "System not initialized";
        case EAGLE_VISION_ERROR_INVALID_STATE:   return "Invalid state";
        case EAGLE_VISION_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case EAGLE_VISION_ERROR_NO_INPUT:        return "No input provided";
        case EAGLE_VISION_ERROR_PROCESSING_FAILED: return "Processing failed";
        default:                                  return "Unknown error";
    }
}
