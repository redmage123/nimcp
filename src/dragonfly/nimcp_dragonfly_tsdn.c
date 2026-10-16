/**
 * @file nimcp_dragonfly_tsdn.c
 * @brief Target-Selective Descending Neuron (TSDN) Population Vector Implementation
 *
 * WHAT: Implements population vector encoding of target direction using 16 TSDNs
 * WHY:  Dragonflies encode prey direction efficiently with 16 neurons spanning 360 degrees
 * HOW:  Each TSDN has cosine tuning; population vector = weighted sum of preferred directions
 *
 * BIOLOGICAL REFERENCE:
 * - Olberg et al. (2007) "Prey pursuit and interception in dragonflies"
 * - Gonzalez-Bellido et al. (2013) "Eight pairs of descending visual neurons"
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/rng/nimcp_rand.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_tsdn)

//=============================================================================
// Constants
//=============================================================================


#define TWO_PI (2.0f * M_PI)

//=============================================================================
// Local Helpers
//=============================================================================

/**
 * @brief Get current time in microseconds
 */
static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

//=============================================================================
// Internal Structure
//=============================================================================

/**
 * @brief Internal TSDN population structure
 */
struct tsdn_population_s {
    /* Configuration */
    tsdn_config_t config;

    /* Neuron state */
    float preferred_direction[TSDN_NEURON_COUNT];  /**< Preferred directions (radians) */
    float firing_rate[TSDN_NEURON_COUNT];          /**< Current firing rates [0,1] */
    float adaptation[TSDN_NEURON_COUNT];           /**< Adaptation state */
    uint64_t spike_count[TSDN_NEURON_COUNT];       /**< Total spikes per neuron */

    /* Gain modulation */
    float gain;                                     /**< Global gain multiplier */
    float facilitation[TSDN_NEURON_COUNT];         /**< Per-neuron facilitation */
    bool facilitation_active;                       /**< Facilitation enabled */

    /* Elevation neurons (3D mode) */
    float* elevation_preferred;                     /**< Elevation preferred directions */
    float* elevation_firing_rate;                   /**< Elevation neuron firing rates */

    /* Statistics */
    tsdn_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float compute_tuning_response(float angle_diff, const tsdn_config_t* config);
static void update_firing_rates(tsdn_population_t* pop, float target_direction);
static tsdn_vector_t compute_population_vector(const tsdn_population_t* pop);

//=============================================================================
// Configuration Functions
//=============================================================================

void tsdn_config_default(tsdn_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(tsdn_config_t));

    /* Tuning parameters - biologically plausible defaults */
    config->tuning_type = TSDN_TUNING_COSINE;
    config->tuning_width = TSDN_DEFAULT_TUNING_WIDTH;      /* ~60 degrees */
    config->tuning_exponent = TSDN_DEFAULT_TUNING_EXPONENT; /* Sharpens selectivity */

    /* Dimensionality */
    config->mode = TSDN_MODE_2D;

    /* Noise and adaptation */
    config->baseline_noise = 0.02f;    /* Small baseline noise */
    config->adaptation_rate = 1.0f;    /* 1 Hz adaptation (biological range 0.5-100 Hz) */
    config->enable_adaptation = true;

    /* Gain modulation */
    config->gain = 1.0f;
    config->enable_gain_modulation = true;

    /* Elevation neurons (3D mode) */
    config->elevation_neurons = 8;      /* 8 elevation-tuned neurons */
    config->elevation_range = M_PI / 3.0f;  /* +/- 60 degrees from horizontal */
}

int tsdn_config_validate(const tsdn_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_config_validate: config is NULL");
        return -1;
    }

    if (config->tuning_width <= 0.0f || config->tuning_width > M_PI) {
        return -2;  /* Invalid tuning width */
    }

    if (config->tuning_exponent < 0.5f || config->tuning_exponent > 10.0f) {
        return -3;  /* Invalid tuning exponent */
    }

    if (config->gain <= 0.0f) {
        return -4;  /* Invalid gain */
    }

    if (config->mode == TSDN_MODE_3D) {
        if (config->elevation_neurons == 0 || config->elevation_neurons > 32) {
            return -5;  /* Invalid elevation neuron count */
        }
        if (config->elevation_range <= 0.0f || config->elevation_range > M_PI / 2.0f) {
            return -6;  /* Invalid elevation range */
        }
    }

    return 0;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

tsdn_population_t* tsdn_create(const tsdn_config_t* config) {
    tsdn_config_t default_config;

    if (!config) {
        tsdn_config_default(&default_config);
        config = &default_config;
    }

    if (tsdn_config_validate(config) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "tsdn_create: invalid configuration");
        return NULL;
    }

    tsdn_population_t* pop = nimcp_calloc(1, sizeof(tsdn_population_t));
    if (!pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "tsdn_create: failed to allocate population");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&pop->config, config, sizeof(tsdn_config_t));

    /* Initialize preferred directions - evenly spaced around 360 degrees */
    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        pop->preferred_direction[i] = (float)i * TSDN_ANGULAR_SPACING;
        pop->firing_rate[i] = 0.0f;
        pop->adaptation[i] = 0.0f;
        pop->spike_count[i] = 0;
        pop->facilitation[i] = 1.0f;  /* No facilitation by default */
    }

    pop->gain = config->gain;
    pop->facilitation_active = false;

    /* Allocate elevation neurons for 3D mode */
    if (config->mode == TSDN_MODE_3D && config->elevation_neurons > 0) {
        pop->elevation_preferred = nimcp_calloc(config->elevation_neurons, sizeof(float));
        pop->elevation_firing_rate = nimcp_calloc(config->elevation_neurons, sizeof(float));

        if (!pop->elevation_preferred || !pop->elevation_firing_rate) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "tsdn_create: failed to allocate elevation arrays");
            tsdn_destroy(pop);
            return NULL;
        }

        /* Initialize elevation preferred directions */
        if (config->elevation_neurons > 1) {
            float elev_spacing = (2.0f * config->elevation_range) / (float)(config->elevation_neurons - 1);
            for (uint32_t i = 0; i < config->elevation_neurons; i++) {
                pop->elevation_preferred[i] = -config->elevation_range + (float)i * elev_spacing;
            }
        } else {
            /* Single neuron: center at 0 elevation */
            pop->elevation_preferred[0] = 0.0f;
        }
    }

    /* Initialize statistics */
    memset(&pop->stats, 0, sizeof(tsdn_stats_t));

    /* Initialize timing */
    pop->creation_time_us = get_time_us();
    pop->last_update_us = pop->creation_time_us;

    /* Create mutex for thread safety */
    pop->mutex = nimcp_mutex_create(NULL);
    if (!pop->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "tsdn_create: failed to create mutex");
        tsdn_destroy(pop);
        return NULL;
    }

    return pop;
}

void tsdn_destroy(tsdn_population_t* pop) {
    if (!pop) return;

    if (pop->mutex) {
        nimcp_mutex_destroy(pop->mutex);
    }

    if (pop->elevation_preferred) {
        nimcp_free(pop->elevation_preferred);
    }

    if (pop->elevation_firing_rate) {
        nimcp_free(pop->elevation_firing_rate);
    }

    nimcp_free(pop);
}

int tsdn_reset(tsdn_population_t* pop) {
    if (!pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_reset: pop is NULL");
        return -1;
    }

    nimcp_mutex_lock(pop->mutex);

    /* Reset firing rates and adaptation */
    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        pop->firing_rate[i] = 0.0f;
        pop->adaptation[i] = 0.0f;
        pop->facilitation[i] = 1.0f;
    }

    pop->facilitation_active = false;
    pop->gain = pop->config.gain;

    /* Reset elevation neurons */
    if (pop->config.mode == TSDN_MODE_3D && pop->elevation_firing_rate) {
        for (uint32_t i = 0; i < pop->config.elevation_neurons; i++) {
            pop->elevation_firing_rate[i] = 0.0f;
        }
    }

    pop->last_update_us = get_time_us();

    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

//=============================================================================
// Encoding Functions
//=============================================================================

tsdn_vector_t tsdn_encode(tsdn_population_t* pop, float target_x, float target_y) {
    tsdn_vector_t result = {0};
    result.valid = false;

    if (!pop) return result;

    /* Compute target direction */
    float direction = atan2f(target_y, target_x);

    nimcp_mutex_lock(pop->mutex);

    /* Update firing rates based on target direction */
    update_firing_rates(pop, direction);

    /* Compute population vector */
    result = compute_population_vector(pop);

    /* Update statistics */
    pop->stats.encode_calls++;

    nimcp_mutex_unlock(pop->mutex);

    return result;
}

tsdn_vector_t tsdn_encode_3d(
    tsdn_population_t* pop,
    float target_x,
    float target_y,
    float target_z
) {
    tsdn_vector_t result = {0};
    result.valid = false;

    if (!pop || pop->config.mode != TSDN_MODE_3D) {
        return result;
    }

    /* Compute azimuth and elevation */
    float horizontal_dist = sqrtf(target_x * target_x + target_y * target_y);
    float azimuth = atan2f(target_y, target_x);
    float elevation = atan2f(target_z, horizontal_dist);

    nimcp_mutex_lock(pop->mutex);

    /* Update azimuth firing rates */
    update_firing_rates(pop, azimuth);

    /* Update elevation firing rates */
    if (pop->elevation_firing_rate && pop->elevation_preferred) {
        for (uint32_t i = 0; i < pop->config.elevation_neurons; i++) {
            float angle_diff = elevation - pop->elevation_preferred[i];
            pop->elevation_firing_rate[i] = compute_tuning_response(angle_diff, &pop->config);
        }
    }

    /* Compute population vector */
    result = compute_population_vector(pop);

    /* Add elevation from elevation neurons */
    if (pop->elevation_firing_rate && pop->elevation_preferred) {
        float elev_sum_x = 0.0f, elev_sum_y = 0.0f;
        for (uint32_t i = 0; i < pop->config.elevation_neurons; i++) {
            elev_sum_x += pop->elevation_firing_rate[i] * cosf(pop->elevation_preferred[i]);
            elev_sum_y += pop->elevation_firing_rate[i] * sinf(pop->elevation_preferred[i]);
        }
        result.elevation = atan2f(elev_sum_y, elev_sum_x);
    }

    pop->stats.encode_calls++;

    nimcp_mutex_unlock(pop->mutex);

    return result;
}

tsdn_vector_t tsdn_encode_direction(tsdn_population_t* pop, float direction_rad) {
    tsdn_vector_t result = {0};
    result.valid = false;

    if (!pop) return result;

    nimcp_mutex_lock(pop->mutex);

    update_firing_rates(pop, direction_rad);
    result = compute_population_vector(pop);
    pop->stats.encode_calls++;

    nimcp_mutex_unlock(pop->mutex);

    return result;
}

tsdn_vector_t tsdn_encode_direction_3d(
    tsdn_population_t* pop,
    float azimuth_rad,
    float elevation_rad
) {
    /* Convert to Cartesian and use encode_3d */
    float cos_elev = cosf(elevation_rad);
    float target_x = cosf(azimuth_rad) * cos_elev;
    float target_y = sinf(azimuth_rad) * cos_elev;
    float target_z = sinf(elevation_rad);

    return tsdn_encode_3d(pop, target_x, target_y, target_z);
}

//=============================================================================
// Decoding Functions
//=============================================================================

tsdn_vector_t tsdn_decode(const tsdn_population_t* pop) {
    tsdn_vector_t result = {0};
    result.valid = false;

    if (!pop) return result;

    nimcp_mutex_lock(((tsdn_population_t*)pop)->mutex);
    result = compute_population_vector(pop);
    ((tsdn_population_t*)pop)->stats.decode_calls++;
    nimcp_mutex_unlock(((tsdn_population_t*)pop)->mutex);

    return result;
}

tsdn_vector_t tsdn_decode_external(
    const tsdn_population_t* pop,
    const float* firing_rates
) {
    tsdn_vector_t result = {0};
    result.valid = false;

    if (!pop || !firing_rates) return result;

    /* Compute population vector from external firing rates */
    float sum_x = 0.0f, sum_y = 0.0f;
    float total_rate = 0.0f;

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        float rate = firing_rates[i];
        if (rate > TSDN_MIN_FIRING_RATE) {
            sum_x += rate * cosf(pop->preferred_direction[i]);
            sum_y += rate * sinf(pop->preferred_direction[i]);
            total_rate += rate;
        }
    }

    if (total_rate > TSDN_MIN_FIRING_RATE) {
        result.direction = atan2f(sum_y, sum_x);
        result.magnitude = sqrtf(sum_x * sum_x + sum_y * sum_y) / total_rate;
        result.valid = true;
    }

    result.timestamp_us = get_time_us();

    return result;
}

//=============================================================================
// State Access Functions
//=============================================================================

int tsdn_get_state(const tsdn_population_t* pop, tsdn_state_t* state) {
    if (!pop || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_get_state: required parameter is NULL (pop, state)");
        return -1;
    }

    nimcp_mutex_lock(((tsdn_population_t*)pop)->mutex);

    memcpy(state->firing_rate, pop->firing_rate, sizeof(pop->firing_rate));
    memcpy(state->preferred_direction, pop->preferred_direction, sizeof(pop->preferred_direction));
    memcpy(state->adaptation, pop->adaptation, sizeof(pop->adaptation));
    memcpy(state->spike_count, pop->spike_count, sizeof(pop->spike_count));
    state->last_update_us = pop->last_update_us;

    nimcp_mutex_unlock(((tsdn_population_t*)pop)->mutex);

    return 0;
}

int tsdn_get_firing_rate(
    const tsdn_population_t* pop,
    uint32_t neuron_id,
    float* rate
) {
    if (!pop || !rate || neuron_id >= TSDN_NEURON_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_get_firing_rate: required parameter is NULL (pop, rate)");
        return -1;
    }

    nimcp_mutex_lock(((tsdn_population_t*)pop)->mutex);
    *rate = pop->firing_rate[neuron_id];
    nimcp_mutex_unlock(((tsdn_population_t*)pop)->mutex);

    return 0;
}

int tsdn_get_preferred_direction(
    const tsdn_population_t* pop,
    uint32_t neuron_id,
    float* direction
) {
    if (!pop || !direction || neuron_id >= TSDN_NEURON_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_get_preferred_direction: required parameter is NULL (pop, direction)");
        return -1;
    }

    /* Preferred directions are constant, no lock needed */
    *direction = pop->preferred_direction[neuron_id];
    return 0;
}

int tsdn_set_firing_rates(tsdn_population_t* pop, const float* firing_rates) {
    if (!pop || !firing_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_set_firing_rates: required parameter is NULL (pop, firing_rates)");
        return -1;
    }

    nimcp_mutex_lock(pop->mutex);

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        pop->firing_rate[i] = fmaxf(0.0f, fminf(TSDN_MAX_FIRING_RATE, firing_rates[i]));
    }

    pop->last_update_us = get_time_us();

    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

//=============================================================================
// Gain Control Functions
//=============================================================================

int tsdn_set_gain(tsdn_population_t* pop, float gain) {
    if (!pop || gain < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tsdn_set_gain: pop is NULL");
        return -1;
    }

    nimcp_mutex_lock(pop->mutex);
    pop->gain = gain;
    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

int tsdn_get_gain(const tsdn_population_t* pop, float* gain) {
    if (!pop || !gain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_get_gain: required parameter is NULL (pop, gain)");
        return -1;
    }

    nimcp_mutex_lock(((tsdn_population_t*)pop)->mutex);
    *gain = pop->gain;
    nimcp_mutex_unlock(((tsdn_population_t*)pop)->mutex);

    return 0;
}

int tsdn_apply_facilitation(
    tsdn_population_t* pop,
    float predicted_direction,
    float facilitation_strength,
    float facilitation_width
) {
    if (!pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_apply_facilitation: pop is NULL");
        return -1;
    }
    if (facilitation_strength < 0.0f || facilitation_strength > 1.0f) return -2;
    if (facilitation_width <= 0.0f) return -3;

    nimcp_mutex_lock(pop->mutex);

    /* Apply Gaussian facilitation centered on predicted direction */
    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        float angle_diff = tsdn_angular_diff(pop->preferred_direction[i], predicted_direction);
        float gaussian = expf(-(angle_diff * angle_diff) / (2.0f * facilitation_width * facilitation_width));
        pop->facilitation[i] = 1.0f + facilitation_strength * gaussian;
    }

    pop->facilitation_active = true;

    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

int tsdn_clear_facilitation(tsdn_population_t* pop) {
    if (!pop) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_clear_facilitation: pop is NULL");
        return -1;
    }

    nimcp_mutex_lock(pop->mutex);

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        pop->facilitation[i] = 1.0f;
    }
    pop->facilitation_active = false;

    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int tsdn_update(tsdn_population_t* pop, float dt) {
    if (!pop || dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tsdn_update: pop is NULL");
        return -1;
    }

    nimcp_mutex_lock(pop->mutex);

    if (pop->config.enable_adaptation) {
        /* Apply adaptation - firing rates decay over time */
        float decay = expf(-dt * pop->config.adaptation_rate);
        for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
            pop->adaptation[i] *= decay;
            /* Adaptation reduces effective firing rate */
            /* (actual reduction applied during encoding) */
        }
    }

    pop->last_update_us = get_time_us();

    nimcp_mutex_unlock(pop->mutex);

    return 0;
}

//=============================================================================
// Statistics Functions
//=============================================================================

int tsdn_get_stats(const tsdn_population_t* pop, tsdn_stats_t* stats) {
    if (!pop || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tsdn_get_stats: required parameter is NULL (pop, stats)");
        return -1;
    }

    nimcp_mutex_lock(((tsdn_population_t*)pop)->mutex);
    memcpy(stats, &pop->stats, sizeof(tsdn_stats_t));
    nimcp_mutex_unlock(((tsdn_population_t*)pop)->mutex);

    return 0;
}

void tsdn_reset_stats(tsdn_population_t* pop) {
    if (!pop) return;

    nimcp_mutex_lock(pop->mutex);
    memset(&pop->stats, 0, sizeof(tsdn_stats_t));
    nimcp_mutex_unlock(pop->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

float tsdn_normalize_angle(float angle) {
    while (angle > M_PI) angle -= NIMCP_TWO_PI_F;
    while (angle < -M_PI) angle += NIMCP_TWO_PI_F;
    return angle;
}

float tsdn_angular_diff(float angle1, float angle2) {
    float diff = angle1 - angle2;
    return tsdn_normalize_angle(diff);
}

float tsdn_tuning_response(
    float angle_diff,
    tsdn_tuning_type_t tuning_type,
    float tuning_width,
    float tuning_exponent
) {
    tsdn_config_t config = {
        .tuning_type = tuning_type,
        .tuning_width = tuning_width,
        .tuning_exponent = tuning_exponent
    };
    return compute_tuning_response(angle_diff, &config);
}

//=============================================================================
// Internal Functions
//=============================================================================

/**
 * @brief Compute tuning curve response for given angular difference
 */
static float compute_tuning_response(float angle_diff, const tsdn_config_t* config) {
    angle_diff = tsdn_normalize_angle(angle_diff);

    float response = 0.0f;

    switch (config->tuning_type) {
        case TSDN_TUNING_COSINE: {
            /* Cosine tuning: response = max(0, cos(diff))^exponent */
            float cos_diff = cosf(angle_diff);
            if (cos_diff > 0.0f) {
                response = powf(cos_diff, config->tuning_exponent);
            }
            break;
        }

        case TSDN_TUNING_GAUSSIAN: {
            /* Gaussian tuning: response = exp(-diff^2 / (2*sigma^2)) */
            float sigma = config->tuning_width;
            response = expf(-(angle_diff * angle_diff) / (2.0f * sigma * sigma));
            break;
        }

        case TSDN_TUNING_VON_MISES: {
            /* Von Mises (circular Gaussian): response = exp(kappa * cos(diff)) */
            /* kappa ≈ 1/sigma^2 for small sigma */
            float kappa = 1.0f / (config->tuning_width * config->tuning_width);
            response = expf(kappa * (cosf(angle_diff) - 1.0f));
            break;
        }

        default:
            response = 0.0f;
    }

    return fmaxf(0.0f, fminf(1.0f, response));
}

/**
 * @brief Update firing rates based on target direction
 */
static void update_firing_rates(tsdn_population_t* pop, float target_direction) {
    target_direction = tsdn_normalize_angle(target_direction);

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        float angle_diff = tsdn_angular_diff(pop->preferred_direction[i], target_direction);

        /* Compute base response from tuning curve */
        float response = compute_tuning_response(angle_diff, &pop->config);

        /* Apply gain modulation */
        if (pop->config.enable_gain_modulation) {
            response *= pop->gain;
        }

        /* Apply facilitation */
        if (pop->facilitation_active) {
            response *= pop->facilitation[i];
        }

        /* Apply adaptation (reduces response over sustained activation) */
        if (pop->config.enable_adaptation) {
            float adaptation_factor = 1.0f - 0.3f * pop->adaptation[i];
            response *= fmaxf(0.0f, adaptation_factor);

            /* Increase adaptation for active neurons */
            if (response > 0.1f) {
                pop->adaptation[i] += response * 0.1f;
                pop->adaptation[i] = fminf(1.0f, pop->adaptation[i]);
            }
        }

        /* Add baseline noise */
        if (pop->config.baseline_noise > 0.0f) {
            /* Simple pseudo-random noise */
            float noise = (nimcp_rand_uniform() - 0.5f) * 2.0f * pop->config.baseline_noise;
            response += noise;
        }

        /* Clamp to valid range */
        pop->firing_rate[i] = fmaxf(0.0f, fminf(TSDN_MAX_FIRING_RATE, response));

        /* Update spike count */
        if (pop->firing_rate[i] > TSDN_MIN_FIRING_RATE) {
            pop->stats.total_spikes++;
            pop->spike_count[i]++;
        }
    }

    pop->last_update_us = get_time_us();
}

/**
 * @brief Compute population vector from current firing rates
 */
static tsdn_vector_t compute_population_vector(const tsdn_population_t* pop) {
    tsdn_vector_t result = {0};
    result.valid = false;
    result.timestamp_us = get_time_us();

    float sum_x = 0.0f, sum_y = 0.0f;
    float total_rate = 0.0f;
    float max_rate = 0.0f;

    for (uint32_t i = 0; i < TSDN_NEURON_COUNT; i++) {
        float rate = pop->firing_rate[i];
        if (rate > TSDN_MIN_FIRING_RATE) {
            /* Population vector: weighted sum of unit vectors */
            sum_x += rate * cosf(pop->preferred_direction[i]);
            sum_y += rate * sinf(pop->preferred_direction[i]);
            total_rate += rate;
            if (rate > max_rate) max_rate = rate;
        }
    }

    if (total_rate > TSDN_MIN_FIRING_RATE) {
        /* Direction from population vector */
        result.direction = atan2f(sum_y, sum_x);

        /* Magnitude: normalized by maximum possible (all neurons at max) */
        float vector_length = sqrtf(sum_x * sum_x + sum_y * sum_y);
        result.magnitude = vector_length / (float)TSDN_NEURON_COUNT;

        /* Clamp magnitude to [0, 1] */
        result.magnitude = fminf(1.0f, result.magnitude);

        result.valid = true;

        /* Update statistics */
        ((tsdn_population_t*)pop)->stats.avg_magnitude =
            0.9f * pop->stats.avg_magnitude + 0.1f * result.magnitude;
        ((tsdn_population_t*)pop)->stats.avg_firing_rate =
            0.9f * pop->stats.avg_firing_rate + 0.1f * (total_rate / (float)TSDN_NEURON_COUNT);
    }

    return result;
}
