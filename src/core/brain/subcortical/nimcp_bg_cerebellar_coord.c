//=============================================================================
// nimcp_bg_cerebellar_coord.c - Basal Ganglia-Cerebellar Coordination
//=============================================================================

#include "core/brain/subcortical/nimcp_bg_cerebellar_coord.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(bg_cerebellar_coord, MESH_ADAPTER_CATEGORY_SUBCORTICAL)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

struct bg_cerebellar_coord {
    /* Configuration */
    bgcb_config_t config;

    /* Current motor command from BG */
    bgcb_bg_command_t bg_command;
    bool bg_command_valid;

    /* Current refinement from cerebellum */
    bgcb_cb_refinement_t cb_refinement;
    bool cb_refinement_valid;

    /* Thalamic relay state */
    bgcb_thalamic_state_t thalamic;

    /* Error state */
    bgcb_error_state_t error;

    /* Timing state */
    bgcb_timing_state_t timing;

    /* Motor output */
    float* motor_output;
    uint32_t num_channels;

    /* Current phase */
    bgcb_motor_phase_t phase;

    /* Learning signals */
    float bg_learning_signal;
    float cb_learning_signal;

    /* Handoff state */
    uint32_t handoff_count;
    float last_handoff_time;

    /* Statistics */
    bgcb_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/* ============================================================================
 * LIFECYCLE IMPLEMENTATION
 * ============================================================================ */

void bgcb_default_config(bgcb_config_t* config) {
    if (!config) return;

    config->mode = BGCB_MODE_SEQUENTIAL;
    config->num_motor_channels = 16;

    config->bg_weight = BGCB_DEFAULT_BG_WEIGHT;
    config->cb_weight = BGCB_DEFAULT_CB_WEIGHT;
    config->handoff_threshold = BGCB_HANDOFF_THRESHOLD;

    config->error_sharing_weight = 0.3f;
    config->timing_integration_rate = 0.1f;

    config->enable_adaptive_weighting = true;
    config->enable_error_sharing = true;
    config->enable_timing_sync = true;
}

bg_cerebellar_coord_t* bgcb_create(const bgcb_config_t* config) {
    bg_cerebellar_coord_t* coord = nimcp_calloc(1, sizeof(bg_cerebellar_coord_t));
    if (!coord) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "coord is NULL");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        coord->config = *config;
    } else {
        bgcb_default_config(&coord->config);
    }

    coord->num_channels = coord->config.num_motor_channels;

    /* Allocate motor output */
    coord->motor_output = nimcp_calloc(coord->num_channels, sizeof(float));
    if (!coord->motor_output) {
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgcb_create: coord->motor_output is NULL");
        return NULL;
    }

    /* Allocate thalamic arrays */
    coord->thalamic.bg_contribution = nimcp_calloc(coord->num_channels, sizeof(float));
    coord->thalamic.cb_contribution = nimcp_calloc(coord->num_channels, sizeof(float));
    coord->thalamic.combined_output = nimcp_calloc(coord->num_channels, sizeof(float));

    if (!coord->thalamic.bg_contribution ||
        !coord->thalamic.cb_contribution ||
        !coord->thalamic.combined_output) {
        nimcp_free(coord->thalamic.bg_contribution);
        nimcp_free(coord->thalamic.cb_contribution);
        nimcp_free(coord->thalamic.combined_output);
        nimcp_free(coord->motor_output);
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgcb_create: thalamic allocation failed");
        return NULL;
    }

    coord->thalamic.num_channels = coord->num_channels;
    coord->thalamic.bg_weight = coord->config.bg_weight;
    coord->thalamic.cb_weight = coord->config.cb_weight;

    /* Allocate timing arrays */
    coord->timing.interval_predictions = nimcp_calloc(BGCB_MAX_TIMING_INTERVALS, sizeof(float));
    coord->timing.interval_actuals = nimcp_calloc(BGCB_MAX_TIMING_INTERVALS, sizeof(float));

    if (!coord->timing.interval_predictions || !coord->timing.interval_actuals) {
        nimcp_free(coord->timing.interval_predictions);
        nimcp_free(coord->timing.interval_actuals);
        nimcp_free(coord->thalamic.bg_contribution);
        nimcp_free(coord->thalamic.cb_contribution);
        nimcp_free(coord->thalamic.combined_output);
        nimcp_free(coord->motor_output);
        nimcp_free(coord);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bgcb_create: timing allocation failed");
        return NULL;
    }

    /* Initialize state */
    coord->phase = BGCB_PHASE_SELECTION;
    coord->bg_command_valid = false;
    coord->cb_refinement_valid = false;

    /* Create mutex */
    coord->mutex = nimcp_mutex_create(NULL);
    if (!coord->mutex) {
        bgcb_destroy(coord);
        return NULL;
    }

    return coord;
}

void bgcb_destroy(bg_cerebellar_coord_t* coord) {
    if (!coord) return;

    if (coord->mutex) {
        nimcp_mutex_free(coord->mutex);
    }

    nimcp_free(coord->motor_output);
    nimcp_free(coord->thalamic.bg_contribution);
    nimcp_free(coord->thalamic.cb_contribution);
    nimcp_free(coord->thalamic.combined_output);
    nimcp_free(coord->timing.interval_predictions);
    nimcp_free(coord->timing.interval_actuals);
    nimcp_free(coord);
}

int bgcb_reset(bg_cerebellar_coord_t* coord) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_reset: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    /* Reset motor output */
    memset(coord->motor_output, 0, coord->num_channels * sizeof(float));

    /* Reset thalamic state */
    memset(coord->thalamic.bg_contribution, 0, coord->num_channels * sizeof(float));
    memset(coord->thalamic.cb_contribution, 0, coord->num_channels * sizeof(float));
    memset(coord->thalamic.combined_output, 0, coord->num_channels * sizeof(float));
    coord->thalamic.bg_weight = coord->config.bg_weight;
    coord->thalamic.cb_weight = coord->config.cb_weight;

    /* Reset error state */
    memset(&coord->error, 0, sizeof(bgcb_error_state_t));

    /* Reset timing state */
    memset(coord->timing.interval_predictions, 0, BGCB_MAX_TIMING_INTERVALS * sizeof(float));
    memset(coord->timing.interval_actuals, 0, BGCB_MAX_TIMING_INTERVALS * sizeof(float));
    coord->timing.num_intervals = 0;
    coord->timing.timing_accuracy = 1.0f;

    /* Reset command states */
    coord->bg_command_valid = false;
    coord->cb_refinement_valid = false;

    /* Reset phase */
    coord->phase = BGCB_PHASE_SELECTION;

    /* Reset learning signals */
    coord->bg_learning_signal = 0.0f;
    coord->cb_learning_signal = 0.0f;

    /* Reset handoff */
    coord->handoff_count = 0;
    coord->last_handoff_time = 0.0f;

    /* Reset statistics */
    memset(&coord->stats, 0, sizeof(bgcb_stats_t));

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

/* ============================================================================
 * INPUT IMPLEMENTATION
 * ============================================================================ */

int bgcb_receive_bg_command(bg_cerebellar_coord_t* coord,
                             const bgcb_bg_command_t* command) {
    if (!coord || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_receive_bg_command: required parameter is NULL (coord, command)");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    coord->bg_command = *command;
    coord->bg_command_valid = true;

    /* Update thalamic BG contribution */
    for (uint32_t i = 0; i < coord->num_channels; i++) {
        coord->thalamic.bg_contribution[i] = command->strength;
    }

    /* Update phase */
    if (coord->phase == BGCB_PHASE_SELECTION) {
        coord->phase = BGCB_PHASE_PREPARATION;
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_receive_cb_refinement(bg_cerebellar_coord_t* coord,
                                const bgcb_cb_refinement_t* refinement) {
    if (!coord || !refinement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_receive_cb_refinement: required parameter is NULL (coord, refinement)");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    coord->cb_refinement = *refinement;
    coord->cb_refinement_valid = true;

    /* Update thalamic CB contribution */
    if (refinement->motor_gains && refinement->num_channels > 0) {
        uint32_t n = (refinement->num_channels < coord->num_channels) ?
                     refinement->num_channels : coord->num_channels;
        for (uint32_t i = 0; i < n; i++) {
            coord->thalamic.cb_contribution[i] = refinement->motor_gains[i];
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_update_bg_rpe(bg_cerebellar_coord_t* coord, float rpe) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_update_bg_rpe: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);
    coord->error.bg_rpe = rpe;

    /* Update shared error */
    if (coord->config.enable_error_sharing) {
        coord->error.shared_error =
            coord->error.bg_rpe * coord->config.error_sharing_weight +
            coord->error.cb_motor_error * (1.0f - coord->config.error_sharing_weight);
    }

    /* Generate learning signal for BG */
    coord->bg_learning_signal = rpe;

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_update_cb_error(bg_cerebellar_coord_t* coord, float error) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_update_cb_error: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);
    coord->error.cb_motor_error = error;

    /* Update shared error */
    if (coord->config.enable_error_sharing) {
        coord->error.shared_error =
            coord->error.bg_rpe * coord->config.error_sharing_weight +
            coord->error.cb_motor_error * (1.0f - coord->config.error_sharing_weight);
    }

    /* Generate learning signal for cerebellum */
    coord->cb_learning_signal = error;

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

/* ============================================================================
 * COORDINATION IMPLEMENTATION
 * ============================================================================ */

/* Internal unlocked version - caller must hold mutex */
static void bgcb_coordinate_unlocked(bg_cerebellar_coord_t* coord) {
    float bg_w = coord->thalamic.bg_weight;
    float cb_w = coord->thalamic.cb_weight;

    /* Combine BG and CB contributions in thalamus */
    for (uint32_t i = 0; i < coord->num_channels; i++) {
        coord->thalamic.combined_output[i] =
            bg_w * coord->thalamic.bg_contribution[i] +
            cb_w * coord->thalamic.cb_contribution[i];

        coord->motor_output[i] = coord->thalamic.combined_output[i];
    }

    /* Update statistics */
    coord->stats.bg_contribution_avg = 0.0f;
    coord->stats.cb_contribution_avg = 0.0f;
    for (uint32_t i = 0; i < coord->num_channels; i++) {
        coord->stats.bg_contribution_avg += coord->thalamic.bg_contribution[i];
        coord->stats.cb_contribution_avg += coord->thalamic.cb_contribution[i];
    }
    coord->stats.bg_contribution_avg /= coord->num_channels;
    coord->stats.cb_contribution_avg /= coord->num_channels;
}

int bgcb_coordinate(bg_cerebellar_coord_t* coord) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_coordinate: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);
    bgcb_coordinate_unlocked(coord);
    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_get_motor_output(const bg_cerebellar_coord_t* coord,
                           float* output,
                           uint32_t* num_channels) {
    if (!coord || !output || !num_channels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_get_motor_output: required parameter is NULL (coord, output, num_channels)");
        return -1;
    }

    *num_channels = coord->num_channels;
    memcpy(output, coord->motor_output, coord->num_channels * sizeof(float));
    return 0;
}

bgcb_motor_phase_t bgcb_get_phase(const bg_cerebellar_coord_t* coord) {
    if (!coord) return BGCB_PHASE_SELECTION;
    return coord->phase;
}

int bgcb_trigger_handoff(bg_cerebellar_coord_t* coord) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_trigger_handoff: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    /* Shift weight from BG to CB */
    coord->thalamic.bg_weight *= 0.5f;
    coord->thalamic.cb_weight = 1.0f - coord->thalamic.bg_weight;

    coord->phase = BGCB_PHASE_EXECUTION;
    coord->handoff_count++;
    coord->stats.handoffs = coord->handoff_count;

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_get_shared_error(const bg_cerebellar_coord_t* coord,
                           bgcb_error_state_t* error) {
    if (!coord || !error) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_get_shared_error: required parameter is NULL (coord, error)");
        return -1;
    }
    *error = coord->error;
    return 0;
}

/* ============================================================================
 * TIMING IMPLEMENTATION
 * ============================================================================ */

int bgcb_set_timing_prediction(bg_cerebellar_coord_t* coord,
                                float interval_ms) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_set_timing_prediction: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    if (coord->timing.num_intervals < BGCB_MAX_TIMING_INTERVALS) {
        coord->timing.interval_predictions[coord->timing.num_intervals] = interval_ms;
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_report_actual_timing(bg_cerebellar_coord_t* coord,
                               float interval_ms) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_report_actual_timing: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    if (coord->timing.num_intervals < BGCB_MAX_TIMING_INTERVALS) {
        coord->timing.interval_actuals[coord->timing.num_intervals] = interval_ms;

        /* Update timing accuracy */
        float predicted = coord->timing.interval_predictions[coord->timing.num_intervals];
        float error = fabsf(predicted - interval_ms) / (interval_ms + 1.0f);
        coord->timing.timing_accuracy =
            coord->timing.timing_accuracy * 0.9f + (1.0f - error) * 0.1f;

        coord->timing.num_intervals++;
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_get_timing_state(const bg_cerebellar_coord_t* coord,
                           bgcb_timing_state_t* timing) {
    if (!coord || !timing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_get_timing_state: required parameter is NULL (coord, timing)");
        return -1;
    }
    *timing = coord->timing;
    return 0;
}

/* ============================================================================
 * PROCESSING IMPLEMENTATION
 * ============================================================================ */

int bgcb_step(bg_cerebellar_coord_t* coord, float dt_ms) {
    if (!coord || dt_ms <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bgcb_step: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    /* Update phase based on time and state */
    switch (coord->phase) {
        case BGCB_PHASE_SELECTION:
            /* Waiting for BG command */
            break;

        case BGCB_PHASE_PREPARATION:
            /* Waiting for CB refinement */
            if (coord->cb_refinement_valid) {
                coord->phase = BGCB_PHASE_EXECUTION;
            }
            break;

        case BGCB_PHASE_EXECUTION:
            /* Motor execution in progress */
            coord->last_handoff_time += dt_ms;
            break;

        case BGCB_PHASE_MONITORING:
            /* Post-execution monitoring */
            break;

        default:
            break;
    }

    /* Coordinate outputs (use unlocked version since we hold mutex) */
    bgcb_coordinate_unlocked(coord);

    /* Compute coordination quality */
    float quality = 1.0f;
    if (coord->cb_refinement_valid && coord->cb_refinement.predicted_error > 0) {
        quality = 1.0f - coord->cb_refinement.predicted_error;
    }
    coord->stats.coordination_quality = nimcp_clampf(quality, 0.0f, 1.0f);
    coord->stats.timing_accuracy = coord->timing.timing_accuracy;
    coord->stats.current_phase = coord->phase;
    coord->stats.error_correlation = coord->error.error_correlation;

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_adapt_weights(bg_cerebellar_coord_t* coord, float performance) {
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_adapt_weights: coord is NULL");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    if (coord->config.enable_adaptive_weighting) {
        /* Shift weights based on performance */
        float perf = nimcp_clampf(performance, 0.0f, 1.0f);

        /* If performance is good, favor current balance */
        /* If poor, shift toward CB for refinement */
        if (perf < 0.5f) {
            coord->thalamic.bg_weight *= 0.99f;
            coord->thalamic.cb_weight = 1.0f - coord->thalamic.bg_weight;
        } else {
            /* Slowly restore BG weight */
            coord->thalamic.bg_weight =
                coord->thalamic.bg_weight * 0.99f + coord->config.bg_weight * 0.01f;
            coord->thalamic.cb_weight = 1.0f - coord->thalamic.bg_weight;
        }
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

int bgcb_get_stats(const bg_cerebellar_coord_t* coord, bgcb_stats_t* stats) {
    if (!coord || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bgcb_get_stats: required parameter is NULL (coord, stats)");
        return -1;
    }
    *stats = coord->stats;
    return 0;
}

/* ============================================================================
 * LEARNING IMPLEMENTATION
 * ============================================================================ */

int bgcb_update_learning(bg_cerebellar_coord_t* coord,
                          bgcb_learn_type_t type,
                          float signal) {
    if (!coord || type >= BGCB_LEARN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "bgcb_update_learning: invalid parameters");
        return -1;
    }

    nimcp_mutex_lock(coord->mutex);

    switch (type) {
        case BGCB_LEARN_REWARD:
            coord->bg_learning_signal = signal;
            break;

        case BGCB_LEARN_ERROR:
            coord->cb_learning_signal = signal;
            break;

        case BGCB_LEARN_COMBINED:
            coord->bg_learning_signal = signal * 0.5f;
            coord->cb_learning_signal = signal * 0.5f;
            break;

        default:
            break;
    }

    nimcp_mutex_unlock(coord->mutex);
    return 0;
}

float bgcb_get_bg_learning_signal(const bg_cerebellar_coord_t* coord) {
    if (!coord) return 0.0f;
    return coord->bg_learning_signal;
}

float bgcb_get_cb_learning_signal(const bg_cerebellar_coord_t* coord) {
    if (!coord) return 0.0f;
    return coord->cb_learning_signal;
}
