//=============================================================================
// nimcp_superior_colliculus.c - Superior Colliculus for Gaze/Orienting
//=============================================================================

#include "core/brain/subcortical/nimcp_superior_colliculus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(superior_colliculus, MESH_ADAPTER_CATEGORY_SUBCORTICAL)



/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

#define SC_MAX_TARGETS 32

struct superior_colliculus {
    /* Configuration */
    sc_config_t config;

    /* Layer maps */
    float* superficial_map;      /* Visual input */
    float* intermediate_map;     /* Motor output */
    float* deep_map;             /* Multimodal */
    uint32_t map_width;
    uint32_t map_height;

    /* Current gaze */
    sc_position_t current_gaze;
    sc_position_t pending_target;
    sc_fixation_state_t fixation_state;

    /* Saccade state */
    sc_saccade_t pending_saccade;
    bool saccade_ready;
    float saccade_timer;

    /* Fixation */
    float fixation_strength;

    /* SNr input (inhibition from BG) */
    float* snr_input;

    /* Targets */
    sc_target_t targets[SC_MAX_TARGETS];
    uint32_t num_targets;

    /* Statistics */
    sc_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

static float compute_distance(sc_position_t a, sc_position_t b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static float compute_direction(sc_position_t from, sc_position_t to) {
    return atan2f(to.y - from.y, to.x - from.x);
}

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void sc_default_config(sc_config_t* config) {
    if (!config) return;

    config->map_width = SC_MAP_WIDTH;
    config->map_height = SC_MAP_HEIGHT;

    config->saccade_threshold = 0.5f;
    config->fixation_strength = 0.7f;
    config->snr_gain = 1.0f;

    config->express_saccade_threshold = 0.9f;
    config->enable_antisaccades = true;
    config->enable_memory_saccades = true;

    config->visual_decay = 0.05f;
    config->motor_decay = 0.1f;
}

superior_colliculus_t* sc_create(const sc_config_t* config) {
    superior_colliculus_t* sc = nimcp_calloc(1, sizeof(superior_colliculus_t));
    if (!sc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sc is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        sc->config = *config;
    } else {
        sc_default_config(&sc->config);
    }

    sc->map_width = sc->config.map_width;
    sc->map_height = sc->config.map_height;
    uint32_t map_size = sc->map_width * sc->map_height;

    /* Allocate maps */
    sc->superficial_map = nimcp_calloc(map_size, sizeof(float));
    sc->intermediate_map = nimcp_calloc(map_size, sizeof(float));
    sc->deep_map = nimcp_calloc(map_size, sizeof(float));
    sc->snr_input = nimcp_calloc(map_size, sizeof(float));

    if (!sc->superficial_map || !sc->intermediate_map ||
        !sc->deep_map || !sc->snr_input) {
        nimcp_free(sc->superficial_map);
        nimcp_free(sc->intermediate_map);
        nimcp_free(sc->deep_map);
        nimcp_free(sc->snr_input);
        nimcp_free(sc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_create: operation failed");
        return NULL;
    }

    /* Initialize state */
    sc->current_gaze.x = 0.0f;
    sc->current_gaze.y = 0.0f;
    sc->fixation_state = SC_FIXATION_ACTIVE;
    sc->saccade_ready = false;
    sc->fixation_strength = sc->config.fixation_strength;
    sc->num_targets = 0;

    /* Create mutex */
    sc->mutex = nimcp_mutex_create(NULL);
    if (!sc->mutex) {
        sc_destroy(sc);
        return NULL;
    }

    return sc;
}

void sc_destroy(superior_colliculus_t* sc) {
    if (!sc) return;

    if (sc->mutex) {
        nimcp_mutex_free(sc->mutex);
    }

    nimcp_free(sc->superficial_map);
    nimcp_free(sc->intermediate_map);
    nimcp_free(sc->deep_map);
    nimcp_free(sc->snr_input);
    nimcp_free(sc);
}

int sc_reset(superior_colliculus_t* sc) {
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_reset: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    uint32_t map_size = sc->map_width * sc->map_height;
    memset(sc->superficial_map, 0, map_size * sizeof(float));
    memset(sc->intermediate_map, 0, map_size * sizeof(float));
    memset(sc->deep_map, 0, map_size * sizeof(float));
    memset(sc->snr_input, 0, map_size * sizeof(float));

    sc->current_gaze.x = 0.0f;
    sc->current_gaze.y = 0.0f;
    sc->fixation_state = SC_FIXATION_ACTIVE;
    sc->saccade_ready = false;
    sc->saccade_timer = 0.0f;
    sc->num_targets = 0;

    memset(&sc->stats, 0, sizeof(sc_stats_t));

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

/* ============================================================================
 * VISUAL INPUT IMPLEMENTATION
 * ============================================================================ */

int sc_set_visual_input(superior_colliculus_t* sc,
                         const float* visual_map,
                         uint32_t width,
                         uint32_t height) {
    if (!sc || !visual_map) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_set_visual_input: required parameter is NULL (sc, visual_map)");
        return -1;
    }
    if (width != sc->map_width || height != sc->map_height) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sc_set_visual_input: map dimensions mismatch");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    uint32_t map_size = width * height;
    memcpy(sc->superficial_map, visual_map, map_size * sizeof(float));

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_add_target(superior_colliculus_t* sc, const sc_target_t* target) {
    if (!sc || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_add_target: required parameter is NULL (sc, target)");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    if (sc->num_targets >= SC_MAX_TARGETS) {
        nimcp_mutex_unlock(sc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "sc_add_target: capacity exceeded");
        return -1;
    }

    sc->targets[sc->num_targets] = *target;
    sc->targets[sc->num_targets].id = sc->num_targets;
    sc->num_targets++;

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_update_target(superior_colliculus_t* sc,
                      uint32_t target_id,
                      const sc_position_t* position) {
    if (!sc || !position || target_id >= sc->num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_update_target: required parameter is NULL (sc, position)");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);
    sc->targets[target_id].position = *position;
    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_remove_target(superior_colliculus_t* sc, uint32_t target_id) {
    if (!sc || target_id >= sc->num_targets) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sc_remove_target: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    /* Shift remaining targets */
    for (uint32_t i = target_id; i < sc->num_targets - 1; i++) {
        sc->targets[i] = sc->targets[i + 1];
        sc->targets[i].id = i;
    }
    sc->num_targets--;

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

/* ============================================================================
 * SNr INPUT IMPLEMENTATION
 * ============================================================================ */

int sc_receive_snr_input(superior_colliculus_t* sc,
                          const float* snr_output,
                          uint32_t width,
                          uint32_t height) {
    if (!sc || !snr_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_receive_snr_input: required parameter is NULL (sc, snr_output)");
        return -1;
    }
    if (width != sc->map_width || height != sc->map_height) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sc_receive_snr_input: map dimensions mismatch");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    uint32_t map_size = width * height;
    memcpy(sc->snr_input, snr_output, map_size * sizeof(float));

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_set_snr_disinhibition(superior_colliculus_t* sc,
                              const sc_position_t* target,
                              float disinhibition) {
    if (!sc || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_set_snr_disinhibition: required parameter is NULL (sc, target)");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    /* Map position to map coordinates */
    int map_x = (int)((target->x + SC_MAX_SACCADE_DEG) / (2.0f * SC_MAX_SACCADE_DEG) * sc->map_width);
    int map_y = (int)((target->y + SC_MAX_SACCADE_DEG) / (2.0f * SC_MAX_SACCADE_DEG) * sc->map_height);

    map_x = (int)nimcp_clampf((float)map_x, 0, (float)(sc->map_width - 1));
    map_y = (int)nimcp_clampf((float)map_y, 0, (float)(sc->map_height - 1));

    uint32_t idx = (uint32_t)(map_y * sc->map_width + map_x);
    sc->snr_input[idx] = 1.0f - nimcp_clampf(disinhibition, 0.0f, 1.0f);

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

/* ============================================================================
 * SACCADE CONTROL IMPLEMENTATION
 * ============================================================================ */

bool sc_is_saccade_ready(const superior_colliculus_t* sc) {
    if (!sc) {
        return false;
    }
    return sc->saccade_ready;
}

int sc_get_saccade(const superior_colliculus_t* sc, sc_saccade_t* saccade) {
    if (!sc || !saccade) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_saccade: required parameter is NULL (sc, saccade)");
        return -1;
    }

    if (!sc->saccade_ready) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sc_get_saccade: no saccade ready");
        return -1;
    }

    *saccade = sc->pending_saccade;
    return 0;
}

int sc_execute_saccade(superior_colliculus_t* sc) {
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_execute_saccade: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    if (!sc->saccade_ready) {
        nimcp_mutex_unlock(sc->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sc_execute_saccade: no saccade ready");
        return -1;
    }

    /* Execute the saccade */
    sc->fixation_state = SC_FIXATION_EXECUTING;
    sc->current_gaze = sc->pending_saccade.target;
    sc->saccade_ready = false;

    /* Update statistics */
    sc->stats.saccade_count++;
    sc->stats.avg_saccade_amplitude =
        sc->stats.avg_saccade_amplitude * 0.9f + sc->pending_saccade.amplitude * 0.1f;

    if (sc->pending_saccade.type == SC_SACCADE_EXPRESS) {
        sc->stats.express_saccade_count++;
    }

    sc->fixation_state = SC_FIXATION_LANDING;

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_cancel_saccade(superior_colliculus_t* sc) {
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_cancel_saccade: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);
    sc->saccade_ready = false;
    sc->fixation_state = SC_FIXATION_ACTIVE;
    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

/**
 * @brief Internal unlocked variant - caller MUST hold sc->mutex.
 */
static int sc_command_saccade_unlocked(superior_colliculus_t* sc,
                                        const sc_position_t* target,
                                        sc_saccade_type_t type) {
    /* Prepare saccade */
    sc->pending_saccade.target = *target;
    sc->pending_saccade.start = sc->current_gaze;
    sc->pending_saccade.amplitude = compute_distance(sc->current_gaze, *target);
    sc->pending_saccade.direction = compute_direction(sc->current_gaze, *target);
    sc->pending_saccade.type = type;
    sc->pending_saccade.confidence = 0.8f;

    /* Calculate duration based on main sequence */
    sc->pending_saccade.duration = 20.0f + 2.5f * sc->pending_saccade.amplitude;
    sc->pending_saccade.velocity = sc->pending_saccade.amplitude /
                                    (sc->pending_saccade.duration / 1000.0f);

    sc->saccade_ready = true;
    sc->fixation_state = SC_FIXATION_PREPARING;

    return 0;
}

int sc_command_saccade(superior_colliculus_t* sc,
                        const sc_position_t* target,
                        sc_saccade_type_t type) {
    if (!sc || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_cancel_saccade: required parameter is NULL (sc, target)");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);
    int ret = sc_command_saccade_unlocked(sc, target, type);
    nimcp_mutex_unlock(sc->mutex);
    return ret;
}

int sc_get_gaze(const superior_colliculus_t* sc, sc_position_t* gaze) {
    if (!sc || !gaze) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_gaze: required parameter is NULL (sc, gaze)");
        return -1;
    }
    *gaze = sc->current_gaze;
    return 0;
}

/* ============================================================================
 * FIXATION IMPLEMENTATION
 * ============================================================================ */

int sc_strengthen_fixation(superior_colliculus_t* sc, float strength) {
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_strengthen_fixation: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);
    sc->fixation_strength = nimcp_clampf(strength, 0.0f, 1.0f);
    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_release_fixation(superior_colliculus_t* sc) {
    if (!sc) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_release_fixation: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);
    sc->fixation_strength = 0.0f;
    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

sc_fixation_state_t sc_get_fixation_state(const superior_colliculus_t* sc) {
    if (!sc) return SC_FIXATION_ACTIVE;
    return sc->fixation_state;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int sc_step(superior_colliculus_t* sc, float dt_ms) {
    if (!sc || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sc_step: sc is NULL");
        return -1;
    }

    nimcp_mutex_lock(sc->mutex);

    uint32_t map_size = sc->map_width * sc->map_height;

    /* Decay maps */
    for (uint32_t i = 0; i < map_size; i++) {
        sc->superficial_map[i] *= (1.0f - sc->config.visual_decay * dt_ms / 1000.0f);
        sc->intermediate_map[i] *= (1.0f - sc->config.motor_decay * dt_ms / 1000.0f);
    }

    /* Update motor map from visual input and SNr disinhibition */
    for (uint32_t i = 0; i < map_size; i++) {
        float visual = sc->superficial_map[i];
        float disinhibition = 1.0f - sc->snr_input[i] * sc->config.snr_gain;
        sc->intermediate_map[i] += visual * disinhibition * 0.1f;
        sc->intermediate_map[i] = nimcp_clampf(sc->intermediate_map[i], 0.0f, 1.0f);
    }

    /* Find peak in motor map */
    float max_activity = 0.0f;
    uint32_t max_idx = 0;
    for (uint32_t i = 0; i < map_size; i++) {
        if (sc->intermediate_map[i] > max_activity) {
            max_activity = sc->intermediate_map[i];
            max_idx = i;
        }
    }

    /* Check if saccade should be triggered */
    if (max_activity > sc->config.saccade_threshold &&
        sc->fixation_state == SC_FIXATION_ACTIVE &&
        !sc->saccade_ready) {

        /* Check fixation suppression */
        if (max_activity > sc->fixation_strength) {
            /* Convert map coordinates to visual degrees */
            uint32_t map_x = max_idx % sc->map_width;
            uint32_t map_y = max_idx / sc->map_width;

            sc_position_t target;
            target.x = (float)map_x / sc->map_width * 2.0f * SC_MAX_SACCADE_DEG - SC_MAX_SACCADE_DEG;
            target.y = (float)map_y / sc->map_height * 2.0f * SC_MAX_SACCADE_DEG - SC_MAX_SACCADE_DEG;

            /* Determine saccade type */
            sc_saccade_type_t type = SC_SACCADE_VOLUNTARY;
            if (max_activity > sc->config.express_saccade_threshold) {
                type = SC_SACCADE_EXPRESS;
            }

            sc_command_saccade_unlocked(sc, &target, type);
        }
    }

    /* Handle fixation state transitions */
    if (sc->fixation_state == SC_FIXATION_LANDING) {
        sc->saccade_timer += dt_ms;
        if (sc->saccade_timer > SC_FIXATION_DURATION_MS) {
            sc->fixation_state = SC_FIXATION_ACTIVE;
            sc->saccade_timer = 0.0f;
        }
    }

    /* Update statistics */
    sc->stats.fixation_state = sc->fixation_state;
    sc->stats.current_gaze = sc->current_gaze;
    sc->stats.fixation_stability = 1.0f - max_activity;

    nimcp_mutex_unlock(sc->mutex);
    return 0;
}

int sc_get_motor_map(const superior_colliculus_t* sc,
                      float* motor_map,
                      uint32_t* width,
                      uint32_t* height) {
    if (!sc || !motor_map || !width || !height) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_motor_map: required parameter is NULL (sc, motor_map, width, height)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)sc->mutex);

    *width = sc->map_width;
    *height = sc->map_height;
    memcpy(motor_map, sc->intermediate_map, sc->map_width * sc->map_height * sizeof(float));

    nimcp_mutex_unlock((nimcp_mutex_t*)sc->mutex);
    return 0;
}

int sc_get_stats(const superior_colliculus_t* sc, sc_stats_t* stats) {
    if (!sc || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_stats: required parameter is NULL (sc, stats)");
        return -1;
    }
    *stats = sc->stats;
    return 0;
}

/* ============================================================================
 * COROLLARY DISCHARGE IMPLEMENTATION
 * ============================================================================ */

int sc_get_corollary_discharge(const superior_colliculus_t* sc,
                                sc_saccade_t* cd) {
    if (!sc || !cd) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_corollary_discharge: required parameter is NULL (sc, cd)");
        return -1;
    }

    if (sc->saccade_ready) {
        *cd = sc->pending_saccade;
        return 0;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "sc_get_corollary_discharge: no saccade pending");
    return -1;
}

int sc_get_predicted_gaze(const superior_colliculus_t* sc,
                           sc_position_t* predicted) {
    if (!sc || !predicted) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sc_get_stats: required parameter is NULL (sc, predicted)");
        return -1;
    }

    if (sc->saccade_ready) {
        *predicted = sc->pending_saccade.target;
    } else {
        *predicted = sc->current_gaze;
    }
    return 0;
}
