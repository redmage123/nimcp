/**
 * @file nimcp_dragonfly_ocelli.c
 * @brief Ocelli-based Attitude Stabilization System Implementation
 *
 * WHAT: Implements horizon detection and attitude correction
 * WHY:  Maintains stable flight platform during high-speed pursuit maneuvers
 * HOW:  Processes ocelli input to generate compensatory motor commands
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_ocelli.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_ocelli)

//=============================================================================
// Constants
//=============================================================================


//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_ocelli_s {
    /* Configuration */
    ocelli_config_t config;

    /* Current state */
    ocelli_attitude_t attitude;
    ocelli_input_t last_input;
    bool has_input;

    /* Filtering state */
    float filtered_pitch;
    float filtered_roll;
    float prev_pitch;
    float prev_roll;
    uint64_t prev_time_us;

    /* Statistics */
    ocelli_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
};

//=============================================================================
// Configuration Functions
//=============================================================================

ocelli_config_t ocelli_default_config(void) {
    ocelli_config_t config = {
        /* Sensor geometry */
        .median_elevation_rad = (float)(30.0 * M_PI / 180.0),
        .lateral_separation_rad = (float)(60.0 * M_PI / 180.0),

        /* Processing */
        .horizon_threshold = 0.5f,
        .smoothing_factor = 0.3f,

        /* Control gains */
        .pitch_gain = 1.0f,
        .roll_gain = 1.0f,
        .rate_damping = 0.5f,

        /* Limits */
        .max_pitch_correction_rad = (float)(45.0 * M_PI / 180.0),
        .max_roll_correction_rad = (float)(60.0 * M_PI / 180.0),

        /* Integration */
        .enable_rate_estimation = true,
        .enable_predictive = false,
        .prediction_horizon_ms = 20.0f
    };
    return config;
}

bool ocelli_validate_config(const ocelli_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ocelli_validate_config: config is NULL");
        return false;
    }

    if (config->median_elevation_rad < 0.0f ||
        config->median_elevation_rad > (float)(M_PI / 2.0)) return false;
    if (config->lateral_separation_rad < 0.0f ||
        config->lateral_separation_rad > (float)M_PI) return false;

    if (config->horizon_threshold < 0.0f ||
        config->horizon_threshold > 1.0f) return false;
    if (config->smoothing_factor < 0.0f ||
        config->smoothing_factor > 1.0f) return false;

    if (config->pitch_gain < 0.0f) {
        return false;
    }
    if (config->roll_gain < 0.0f) {
        return false;
    }
    if (config->rate_damping < 0.0f) {
        return false;
    }

    if (config->max_pitch_correction_rad <= 0.0f) {
        return false;
    }
    if (config->max_roll_correction_rad <= 0.0f) {
        return false;
    }

    if (config->prediction_horizon_ms < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_ocelli_t dragonfly_ocelli_create(const ocelli_config_t* config) {
    ocelli_config_t cfg = config ? *config : ocelli_default_config();

    if (!ocelli_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_ocelli_create: invalid configuration");
        return NULL;
    }

    dragonfly_ocelli_t ocelli = nimcp_calloc(1, sizeof(struct dragonfly_ocelli_s));
    if (!ocelli) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_ocelli_create: failed to allocate ocelli");
        return NULL;
    }

    ocelli->config = cfg;
    ocelli->creation_time_us = get_time_us();

    ocelli->mutex = nimcp_mutex_create(NULL);
    if (!ocelli->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_ocelli_create: failed to create mutex");
        nimcp_free(ocelli);
        return NULL;
    }

    return ocelli;
}

void dragonfly_ocelli_destroy(dragonfly_ocelli_t ocelli) {
    if (!ocelli) return;

    if (ocelli->mutex) {
        nimcp_mutex_destroy(ocelli->mutex);
    }

    nimcp_free(ocelli);
}

int dragonfly_ocelli_reset(dragonfly_ocelli_t ocelli) {
    if (!ocelli) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_reset: ocelli is NULL");
        return -1;
    }

    nimcp_mutex_lock(ocelli->mutex);

    memset(&ocelli->attitude, 0, sizeof(ocelli->attitude));
    memset(&ocelli->last_input, 0, sizeof(ocelli->last_input));
    ocelli->has_input = false;
    ocelli->filtered_pitch = 0.0f;
    ocelli->filtered_roll = 0.0f;
    ocelli->prev_pitch = 0.0f;
    ocelli->prev_roll = 0.0f;
    ocelli->prev_time_us = 0;

    nimcp_mutex_unlock(ocelli->mutex);

    return 0;
}

//=============================================================================
// Processing Functions
//=============================================================================

int dragonfly_ocelli_process(
    dragonfly_ocelli_t ocelli,
    const ocelli_input_t* input
) {
    if (!ocelli || !input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_process: required parameter is NULL (ocelli, input)");
        return -1;
    }

    uint64_t start_time = get_time_us();

    nimcp_mutex_lock(ocelli->mutex);

    /* Store input */
    ocelli->last_input = *input;
    ocelli->has_input = true;

    /* Extract attitude from ocelli readings */
    /* Median ocellus: pitch from sky/ground ratio */
    /* Lateral ocelli: roll from left/right balance */

    float median = input->intensity[OCELLUS_MEDIAN];
    float left = input->intensity[OCELLUS_LEFT];
    float right = input->intensity[OCELLUS_RIGHT];

    /* Convert intensity to angle estimate */
    /* Higher median intensity = looking up (negative pitch) */
    /* Higher left intensity = rolled right (positive roll) */

    float raw_pitch = (ocelli->config.horizon_threshold - median) *
                      (float)(M_PI / 2.0);
    float raw_roll = (left - right) * ocelli->config.lateral_separation_rad;

    /* Apply low-pass filtering */
    float alpha = ocelli->config.smoothing_factor;
    ocelli->filtered_pitch = alpha * raw_pitch +
                             (1.0f - alpha) * ocelli->filtered_pitch;
    ocelli->filtered_roll = alpha * raw_roll +
                            (1.0f - alpha) * ocelli->filtered_roll;

    /* Update attitude estimate */
    ocelli->attitude.pitch_rad = ocelli->filtered_pitch;
    ocelli->attitude.roll_rad = ocelli->filtered_roll;

    /* Estimate rates if enabled */
    if (ocelli->config.enable_rate_estimation && ocelli->prev_time_us > 0) {
        float dt = (float)(input->timestamp_us - ocelli->prev_time_us) / 1000000.0f;
        if (dt > 0.0001f) {
            ocelli->attitude.pitch_rate =
                (ocelli->filtered_pitch - ocelli->prev_pitch) / dt;
            ocelli->attitude.roll_rate =
                (ocelli->filtered_roll - ocelli->prev_roll) / dt;
        }
    }

    /* Update previous values */
    ocelli->prev_pitch = ocelli->filtered_pitch;
    ocelli->prev_roll = ocelli->filtered_roll;
    ocelli->prev_time_us = input->timestamp_us;

    /* Compute confidence based on input consistency */
    float avg_intensity = (median + left + right) / 3.0f;
    float intensity_variance = (fabsf(median - avg_intensity) +
                                fabsf(left - avg_intensity) +
                                fabsf(right - avg_intensity)) / 3.0f;
    ocelli->attitude.confidence = nimcp_clampf(1.0f - intensity_variance * 2.0f, 0.0f, 1.0f);
    ocelli->attitude.timestamp_us = input->timestamp_us;

    /* Update statistics */
    ocelli->stats.updates_processed++;
    ocelli->stats.avg_pitch_error_rad =
        (ocelli->stats.avg_pitch_error_rad * (ocelli->stats.updates_processed - 1) +
         fabsf(ocelli->filtered_pitch)) / ocelli->stats.updates_processed;
    ocelli->stats.avg_roll_error_rad =
        (ocelli->stats.avg_roll_error_rad * (ocelli->stats.updates_processed - 1) +
         fabsf(ocelli->filtered_roll)) / ocelli->stats.updates_processed;

    if (fabsf(ocelli->filtered_pitch) > ocelli->stats.max_pitch_error_rad) {
        ocelli->stats.max_pitch_error_rad = fabsf(ocelli->filtered_pitch);
    }
    if (fabsf(ocelli->filtered_roll) > ocelli->stats.max_roll_error_rad) {
        ocelli->stats.max_roll_error_rad = fabsf(ocelli->filtered_roll);
    }

    uint64_t processing_time = get_time_us() - start_time;
    ocelli->stats.avg_response_time_us =
        (ocelli->stats.avg_response_time_us * (ocelli->stats.updates_processed - 1) +
         (float)processing_time) / ocelli->stats.updates_processed;

    nimcp_mutex_unlock(ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_get_attitude(
    const dragonfly_ocelli_t ocelli,
    ocelli_attitude_t* attitude
) {
    if (!ocelli || !attitude) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_get_attitude: required parameter is NULL (ocelli, attitude)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ocelli->mutex);
    *attitude = ocelli->attitude;
    nimcp_mutex_unlock((nimcp_mutex_t*)ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_get_correction(
    const dragonfly_ocelli_t ocelli,
    float target_pitch,
    float target_roll,
    ocelli_correction_t* correction
) {
    if (!ocelli || !correction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_get_correction: required parameter is NULL (ocelli, correction)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ocelli->mutex);

    if (!ocelli->has_input || ocelli->attitude.confidence < 0.1f) {
        correction->is_valid = false;
        nimcp_mutex_unlock((nimcp_mutex_t*)ocelli->mutex);
        return 0;
    }

    /* Compute errors */
    float pitch_error = target_pitch - ocelli->attitude.pitch_rad;
    float roll_error = target_roll - ocelli->attitude.roll_rad;

    /* Apply gains */
    float pitch_correction = pitch_error * ocelli->config.pitch_gain;
    float roll_correction = roll_error * ocelli->config.roll_gain;

    /* Add rate damping */
    if (ocelli->config.enable_rate_estimation) {
        pitch_correction -= ocelli->attitude.pitch_rate * ocelli->config.rate_damping;
        roll_correction -= ocelli->attitude.roll_rate * ocelli->config.rate_damping;
    }

    /* Apply limits */
    correction->pitch_correction_rad = nimcp_clampf(
        pitch_correction,
        -ocelli->config.max_pitch_correction_rad,
        ocelli->config.max_pitch_correction_rad
    );
    correction->roll_correction_rad = nimcp_clampf(
        roll_correction,
        -ocelli->config.max_roll_correction_rad,
        ocelli->config.max_roll_correction_rad
    );

    /* Compute urgency based on error magnitude */
    float max_error = fmaxf(fabsf(pitch_error), fabsf(roll_error));
    correction->urgency = nimcp_clampf(max_error / (float)(M_PI / 4.0), 0.0f, 1.0f);
    correction->is_valid = true;

    /* Update stats */
    ((dragonfly_ocelli_t)ocelli)->stats.corrections_issued++;

    nimcp_mutex_unlock((nimcp_mutex_t*)ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_simulate_input(
    float pitch_rad,
    float roll_rad,
    float sun_elevation_rad,
    ocelli_input_t* input
) {
    if (!input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_simulate_input: input is NULL");
        return -1;
    }

    /* Simulate ocelli readings based on body attitude */
    /* Median ocellus sees more sky when pitched down */
    float median_sky_fraction = 0.5f + sinf(pitch_rad + sun_elevation_rad) * 0.5f;
    input->intensity[OCELLUS_MEDIAN] = nimcp_clampf(median_sky_fraction, 0.0f, 1.0f);

    /* Lateral ocelli see different amounts based on roll */
    float roll_effect = sinf(roll_rad) * 0.3f;
    input->intensity[OCELLUS_LEFT] = nimcp_clampf(0.5f + roll_effect, 0.0f, 1.0f);
    input->intensity[OCELLUS_RIGHT] = nimcp_clampf(0.5f - roll_effect, 0.0f, 1.0f);

    input->timestamp_us = get_time_us();

    return 0;
}

//=============================================================================
// Statistics and Configuration Functions
//=============================================================================

int dragonfly_ocelli_get_stats(
    const dragonfly_ocelli_t ocelli,
    ocelli_stats_t* stats
) {
    if (!ocelli || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_get_stats: required parameter is NULL (ocelli, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ocelli->mutex);
    *stats = ocelli->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_reset_stats(dragonfly_ocelli_t ocelli) {
    if (!ocelli) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_reset_stats: ocelli is NULL");
        return -1;
    }

    nimcp_mutex_lock(ocelli->mutex);
    memset(&ocelli->stats, 0, sizeof(ocelli->stats));
    nimcp_mutex_unlock(ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_set_config(
    dragonfly_ocelli_t ocelli,
    const ocelli_config_t* config
) {
    if (!ocelli || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_set_config: required parameter is NULL (ocelli, config)");
        return -1;
    }
    if (!ocelli_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_ocelli_set_config: ocelli_validate_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(ocelli->mutex);
    ocelli->config = *config;
    nimcp_mutex_unlock(ocelli->mutex);

    return 0;
}

int dragonfly_ocelli_get_config(
    const dragonfly_ocelli_t ocelli,
    ocelli_config_t* config
) {
    if (!ocelli || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ocelli_get_config: required parameter is NULL (ocelli, config)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ocelli->mutex);
    *config = ocelli->config;
    nimcp_mutex_unlock((nimcp_mutex_t*)ocelli->mutex);

    return 0;
}
