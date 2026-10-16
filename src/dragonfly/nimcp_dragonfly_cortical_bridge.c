/**
 * @file nimcp_dragonfly_cortical_bridge.c
 * @brief Implementation of Dragonfly-to-Cortical Column Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_cortical_bridge.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_cortical_bridge)

#define LOG_MODULE "DRAGONFLY_CORTICAL_BRIDGE"



//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_cortical_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    bool initialized;

    /* References to source systems */
    dragonfly_system_t* dragonfly;
    tsdn_population_t* tsdn;

    /* Configuration */
    dragonfly_cortical_config_t config;

    /* Current state */
    cortical_direction_t current_direction;
    float minicolumn_activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    float inhibited_activations[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    /* Preferred directions for each minicolumn */
    float preferred_directions[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    /* Lateral inhibition matrix (circular) */
    float inhibition_weights[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    /* Adaptation state */
    float adaptation_state[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
    float current_gain;

    /* Statistics */
    cortical_bridge_stats_t stats;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static float normalize_angle(float angle) {
    while (angle < 0) angle += 2.0f * M_PI;
    while (angle >= 2.0f * M_PI) angle -= 2.0f * M_PI;
    return angle;
}

static float angular_distance(float a1, float a2) {
    float diff = fabsf(a1 - a2);
    if (diff > M_PI) diff = 2.0f * M_PI - diff;
    return diff;
}

static float softmax_element(float x, float temperature, float max_val) {
    return expf((x - max_val) / temperature);
}

static void initialize_preferred_directions(dragonfly_cortical_bridge_t* bridge) {
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        bridge->preferred_directions[i] = i * DRAGONFLY_CORTICAL_ANGULAR_SPACING;
    }
}

static void initialize_inhibition_weights(dragonfly_cortical_bridge_t* bridge) {
    float sigma = bridge->config.inhibition_sigma;
    if (sigma <= 0) sigma = 2.0f;

    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        /* Distance in circular topology */
        float dist = (float)i;
        if (dist > DRAGONFLY_CORTICAL_MINICOLUMN_COUNT / 2) {
            dist = DRAGONFLY_CORTICAL_MINICOLUMN_COUNT - dist;
        }

        /* Mexican hat: positive near, negative far */
        float narrow = expf(-dist * dist / (2.0f * 1.0f * 1.0f));
        float wide = expf(-dist * dist / (2.0f * sigma * sigma));
        bridge->inhibition_weights[i] = narrow - bridge->config.inhibition_strength * wide;

        /* Self-inhibition is zero */
        if (i == 0) bridge->inhibition_weights[i] = 0;
    }
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_cortical_bridge_default_config(dragonfly_cortical_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    config->mapping_mode = CORTICAL_MAP_DIRECT;
    config->neurons_per_minicolumn = DRAGONFLY_CORTICAL_NEURONS_PER_MINICOLUMN;

    config->competition_mode = CORTICAL_COMPETE_SOFTMAX;
    config->temperature = DRAGONFLY_CORTICAL_DEFAULT_TEMPERATURE;
    config->k_winners = 3;

    config->enable_lateral_inhibition = true;
    config->inhibition_strength = 0.5f;
    config->inhibition_sigma = 2.5f;

    config->hypercolumn_type = HYPERCOLUMN_AZIMUTH;
    config->create_elevation_hypercolumn = false;

    config->gain_modulation = 1.0f;
    config->enable_adaptation = true;
    config->adaptation_tau = 100.0f;

    config->use_external_pool = false;
    config->external_pool = NULL;

    config->interpolate_output = true;
    config->interpolation_window = M_PI / 8.0f;

    return 0;
}

int dragonfly_cortical_bridge_validate_config(const dragonfly_cortical_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->temperature <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid temperature");
        return -1;
    }
    if (config->k_winners == 0 || config->k_winners > DRAGONFLY_CORTICAL_MINICOLUMN_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid k_winners");
        return -1;
    }
    if (config->inhibition_strength < 0 || config->inhibition_strength > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid inhibition_strength");
        return -1;
    }
    if (config->inhibition_sigma <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid inhibition_sigma");
        return -1;
    }
    if (config->gain_modulation < 0.1f || config->gain_modulation > 10.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid gain_modulation");
        return -1;
    }
    if (config->adaptation_tau <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid adaptation_tau");
        return -1;
    }
    if (config->interpolation_window <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_bridge_validate_config: invalid interpolation_window");
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_cortical_bridge_t* dragonfly_cortical_bridge_create(
    dragonfly_system_t* dragonfly,
    tsdn_population_t* tsdn,
    const dragonfly_cortical_config_t* config
) {
    dragonfly_cortical_bridge_t* bridge = nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_cortical_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (dragonfly_cortical_bridge_validate_config(config) != 0) {
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_create: validation failed");
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_cortical_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->tsdn = tsdn;

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_cortical") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_cortical_bridge_create: failed to init base");
        nimcp_free(bridge);
        return NULL;
    }

    /* Initialize preferred directions */
    initialize_preferred_directions(bridge);

    /* Initialize lateral inhibition */
    initialize_inhibition_weights(bridge);

    /* Initialize adaptation state */
    bridge->current_gain = bridge->config.gain_modulation;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        bridge->adaptation_state[i] = 0;
    }

    /* Clear current state */
    memset(&bridge->current_direction, 0, sizeof(bridge->current_direction));
    memset(bridge->minicolumn_activations, 0, sizeof(bridge->minicolumn_activations));

    bridge->initialized = true;
    return bridge;
}

void dragonfly_cortical_bridge_destroy(dragonfly_cortical_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_cortical");
    bridge->initialized = false;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_cortical_bridge_reset(dragonfly_cortical_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_reset: bridge is NULL or not initialized");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    memset(&bridge->current_direction, 0, sizeof(bridge->current_direction));
    memset(bridge->minicolumn_activations, 0, sizeof(bridge->minicolumn_activations));
    memset(bridge->inhibited_activations, 0, sizeof(bridge->inhibited_activations));
    memset(bridge->adaptation_state, 0, sizeof(bridge->adaptation_state));

    bridge->current_gain = bridge->config.gain_modulation;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// TSDN to Cortical Conversion
//=============================================================================

int dragonfly_cortical_tsdn_to_column(
    dragonfly_cortical_bridge_t* bridge,
    const float* tsdn_firing_rates,
    cortical_direction_t* direction
) {
    if (!bridge || !bridge->initialized || !tsdn_firing_rates || !direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_tsdn_to_column: NULL argument");
        return -1;
    }

    uint64_t start_time = get_time_us();

    /* Copy TSDN firing rates directly to minicolumn activations */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        float rate = tsdn_firing_rates[i] * bridge->current_gain;
        if (rate < 0) rate = 0;
        if (rate > 1.0f) rate = 1.0f;
        bridge->minicolumn_activations[i] = rate;
    }

    /* Apply lateral inhibition if enabled */
    if (bridge->config.enable_lateral_inhibition) {
        dragonfly_cortical_apply_lateral_inhibition(bridge, bridge->minicolumn_activations);
    }

    /* Apply competition */
    float max_activation = 0;
    uint32_t winner_index = 0;
    float sum_activation = 0;

    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        if (bridge->minicolumn_activations[i] > max_activation) {
            max_activation = bridge->minicolumn_activations[i];
            winner_index = i;
        }
        sum_activation += bridge->minicolumn_activations[i];
    }

    /* Apply competition mode */
    switch (bridge->config.competition_mode) {
        case CORTICAL_COMPETE_WTA:
            for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                direction->activations[i] = (i == (int)winner_index) ? max_activation : 0;
            }
            break;

        case CORTICAL_COMPETE_SOFTMAX: {
            float softmax_sum = 0;
            for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                direction->activations[i] = softmax_element(
                    bridge->minicolumn_activations[i],
                    bridge->config.temperature,
                    max_activation
                );
                softmax_sum += direction->activations[i];
            }
            if (softmax_sum > 0) {
                for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                    direction->activations[i] /= softmax_sum;
                }
            }
            break;
        }

        case CORTICAL_COMPETE_K_WINNERS: {
            /* Sort and keep top K */
            float sorted[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];
            memcpy(sorted, bridge->minicolumn_activations, sizeof(sorted));

            /* Find kth largest (simple selection) */
            float threshold = 0;
            for (uint32_t k = 0; k < bridge->config.k_winners; k++) {
                float max_k = 0;
                for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                    if (sorted[i] > max_k) max_k = sorted[i];
                }
                if (k == bridge->config.k_winners - 1) threshold = max_k;
                /* Zero out max for next iteration */
                for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                    if (sorted[i] == max_k) { sorted[i] = 0; break; }
                }
            }

            for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                direction->activations[i] = (bridge->minicolumn_activations[i] >= threshold)
                    ? bridge->minicolumn_activations[i] : 0;
            }
            break;
        }

        case CORTICAL_COMPETE_NONE:
        default:
            memcpy(direction->activations, bridge->minicolumn_activations,
                   sizeof(direction->activations));
            break;
    }

    /* Find winner after competition */
    max_activation = 0;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        if (direction->activations[i] > max_activation) {
            max_activation = direction->activations[i];
            winner_index = i;
        }
    }

    direction->winner_index = winner_index;
    direction->winner_activation = max_activation;

    /* Interpolate direction if enabled */
    if (bridge->config.interpolate_output) {
        dragonfly_cortical_interpolate_direction(bridge, direction->activations,
                                                  &direction->azimuth, &direction->confidence);
    } else {
        direction->azimuth = bridge->preferred_directions[winner_index];
        direction->confidence = max_activation;
    }

    /* Compute statistics */
    direction->entropy = dragonfly_cortical_compute_entropy(direction->activations,
                                                            DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    direction->sparseness = dragonfly_cortical_compute_sparseness(direction->activations,
                                                                   DRAGONFLY_CORTICAL_MINICOLUMN_COUNT);
    direction->timestamp_us = get_time_us();
    direction->elevation = 0;  /* 2D mode only for now */

    /* Store current direction */
    bridge->current_direction = *direction;

    /* Update stats */
    uint64_t elapsed = get_time_us() - start_time;
    bridge->stats.tsdn_to_cortical_count++;
    bridge->stats.total_processing_time_us += elapsed;
    bridge->stats.avg_processing_time_us = (float)bridge->stats.total_processing_time_us /
                                           (float)bridge->stats.tsdn_to_cortical_count;
    bridge->stats.avg_winner_activation = (bridge->stats.avg_winner_activation *
        (bridge->stats.tsdn_to_cortical_count - 1) + max_activation) /
        bridge->stats.tsdn_to_cortical_count;
    bridge->stats.avg_entropy = (bridge->stats.avg_entropy *
        (bridge->stats.tsdn_to_cortical_count - 1) + direction->entropy) /
        bridge->stats.tsdn_to_cortical_count;
    bridge->stats.avg_confidence = (bridge->stats.avg_confidence *
        (bridge->stats.tsdn_to_cortical_count - 1) + direction->confidence) /
        bridge->stats.tsdn_to_cortical_count;

    return 0;
}

int dragonfly_cortical_sync_from_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    cortical_direction_t* direction
) {
    if (!bridge || !bridge->initialized || !direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_sync_from_tsdn: NULL argument");
        return -1;
    }

    /* Get TSDN firing rates from population */
    float firing_rates[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    if (bridge->tsdn) {
        /* Use TSDN population directly */
        tsdn_population_t* pop = bridge->tsdn;
        for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
            /* Get firing rate from TSDN - simplified, would use actual API */
            firing_rates[i] = 0;  /* Placeholder - needs TSDN API */
        }
    } else if (bridge->dragonfly) {
        /* Get from dragonfly system */
        dragonfly_stats_t stats;
        if (dragonfly_get_stats(bridge->dragonfly, &stats) == 0) {
            /* Use TSDN population from dragonfly */
            for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
                firing_rates[i] = 0;  /* Placeholder - needs TSDN integration */
            }
        } else {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_cortical_sync_from_tsdn: failed to get stats from dragonfly");
            return -1;
        }
    } else {
        bridge->stats.invalid_inputs++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_cortical_sync_from_tsdn: no source available");
        return -1;
    }

    return dragonfly_cortical_tsdn_to_column(bridge, firing_rates, direction);
}

//=============================================================================
// Cortical to TSDN Conversion
//=============================================================================

int dragonfly_cortical_column_to_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    const cortical_direction_t* direction,
    float* tsdn_firing_rates
) {
    if (!bridge || !bridge->initialized || !direction || !tsdn_firing_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_column_to_tsdn: NULL argument");
        return -1;
    }

    /* Direct mapping from minicolumn activations to TSDN rates */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        tsdn_firing_rates[i] = direction->activations[i] / bridge->current_gain;
        if (tsdn_firing_rates[i] < 0) tsdn_firing_rates[i] = 0;
        if (tsdn_firing_rates[i] > 1.0f) tsdn_firing_rates[i] = 1.0f;
    }

    bridge->stats.cortical_to_tsdn_count++;
    return 0;
}

int dragonfly_cortical_angle_to_tsdn(
    dragonfly_cortical_bridge_t* bridge,
    float azimuth,
    float elevation,
    float* tsdn_firing_rates
) {
    if (!bridge || !bridge->initialized || !tsdn_firing_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_angle_to_tsdn: NULL argument");
        return -1;
    }
    (void)elevation;  /* Not used in 2D mode */

    azimuth = normalize_angle(azimuth);

    /* Generate cosine tuning pattern */
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        float pref = bridge->preferred_directions[i];
        float dist = angular_distance(azimuth, pref);

        /* Cosine tuning with exponent 2 (typical TSDN) */
        float response = cosf(dist);
        if (response < 0) response = 0;
        tsdn_firing_rates[i] = response * response;  /* ^2 for sharper tuning */
    }

    return 0;
}

//=============================================================================
// Hypercolumn Operations
//=============================================================================

int dragonfly_cortical_update_hypercolumn(dragonfly_cortical_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_update_hypercolumn: bridge is NULL or not initialized");
        return -1;
    }

    cortical_direction_t direction;
    int ret = dragonfly_cortical_sync_from_tsdn(bridge, &direction);
    if (ret == 0) {
        bridge->stats.hypercolumn_updates++;
    }
    return ret;
}

int dragonfly_cortical_get_direction(
    const dragonfly_cortical_bridge_t* bridge,
    cortical_direction_t* direction
) {
    if (!bridge || !bridge->initialized || !direction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_get_direction: NULL argument");
        return -1;
    }
    *direction = bridge->current_direction;
    return 0;
}

int dragonfly_cortical_set_competition(
    dragonfly_cortical_bridge_t* bridge,
    cortical_competition_mode_t mode,
    float temperature
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_set_competition: bridge is NULL or not initialized");
        return -1;
    }
    if (temperature <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_set_competition: invalid temperature");
        return -1;
    }

    bridge->config.competition_mode = mode;
    bridge->config.temperature = temperature;
    return 0;
}

int dragonfly_cortical_apply_lateral_inhibition(
    dragonfly_cortical_bridge_t* bridge,
    float* activations
) {
    if (!bridge || !bridge->initialized || !activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_apply_lateral_inhibition: NULL argument");
        return -1;
    }

    float inhibited[DRAGONFLY_CORTICAL_MINICOLUMN_COUNT];

    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        float inhibition = 0;

        for (int j = 0; j < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; j++) {
            if (i == j) continue;

            /* Circular distance */
            int dist = abs(i - j);
            if (dist > DRAGONFLY_CORTICAL_MINICOLUMN_COUNT / 2) {
                dist = DRAGONFLY_CORTICAL_MINICOLUMN_COUNT - dist;
            }

            inhibition += bridge->inhibition_weights[dist] * activations[j];
        }

        inhibited[i] = activations[i] - inhibition;
        if (inhibited[i] < 0) inhibited[i] = 0;
    }

    memcpy(activations, inhibited, sizeof(inhibited));
    return 0;
}

//=============================================================================
// Interpolation and Readout
//=============================================================================

int dragonfly_cortical_interpolate_direction(
    const dragonfly_cortical_bridge_t* bridge,
    const float* activations,
    float* azimuth,
    float* confidence
) {
    if (!bridge || !activations || !azimuth || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_interpolate_direction: NULL argument");
        return -1;
    }

    /* Find winner */
    float max_act = 0;
    int winner = 0;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        if (activations[i] > max_act) {
            max_act = activations[i];
            winner = i;
        }
    }

    if (max_act <= 0) {
        *azimuth = 0;
        *confidence = 0;
        return 0;
    }

    /* Center-of-mass interpolation around winner */
    float sum_weight = 0;
    float sum_x = 0;
    float sum_y = 0;

    for (int di = -2; di <= 2; di++) {
        int idx = (winner + di + DRAGONFLY_CORTICAL_MINICOLUMN_COUNT) %
                  DRAGONFLY_CORTICAL_MINICOLUMN_COUNT;
        float weight = activations[idx];
        float angle = bridge->preferred_directions[idx];

        sum_x += weight * cosf(angle);
        sum_y += weight * sinf(angle);
        sum_weight += weight;
    }

    if (sum_weight > 0) {
        *azimuth = atan2f(sum_y, sum_x);
        *azimuth = normalize_angle(*azimuth);
        *confidence = sqrtf(sum_x * sum_x + sum_y * sum_y) / sum_weight;
    } else {
        *azimuth = bridge->preferred_directions[winner];
        *confidence = 0;
    }

    return 0;
}

float dragonfly_cortical_compute_entropy(const float* activations, uint32_t count) {
    if (!activations || count == 0) return 0;

    /* Normalize to probability distribution */
    float sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += activations[i];
    }
    if (sum <= 0) return 0;

    float entropy = 0;
    for (uint32_t i = 0; i < count; i++) {
        float p = activations[i] / sum;
        if (p > 1e-10f) {
            entropy -= p * log2f(p);
        }
    }

    return entropy;
}

float dragonfly_cortical_compute_sparseness(const float* activations, uint32_t count) {
    if (!activations || count == 0) return 0;

    /* Treves-Rolls sparseness measure */
    float sum = 0;
    float sum_sq = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += activations[i];
        sum_sq += activations[i] * activations[i];
    }

    if (sum_sq <= 0 || count <= 1) return 0;

    float mean = sum / count;
    float mean_sq = sum_sq / count;

    if (mean_sq <= 0) return 0;

    float sparseness = (1.0f - (mean * mean) / mean_sq) / (1.0f - 1.0f / count);
    if (sparseness < 0) sparseness = 0;
    if (sparseness > 1) sparseness = 1;

    return sparseness;
}

//=============================================================================
// Gain and Adaptation
//=============================================================================

int dragonfly_cortical_set_gain(dragonfly_cortical_bridge_t* bridge, float gain) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_set_gain: bridge is NULL or not initialized");
        return -1;
    }
    if (gain < 0.1f || gain > 10.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_cortical_set_gain: gain out of range");
        return -1;
    }

    bridge->current_gain = gain;
    bridge->stats.current_gain = gain;
    return 0;
}

float dragonfly_cortical_get_gain(const dragonfly_cortical_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return 1.0f;
    return bridge->current_gain;
}

int dragonfly_cortical_update_adaptation(dragonfly_cortical_bridge_t* bridge, float dt_ms) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_update_adaptation: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_adaptation) return 0;

    float tau = bridge->config.adaptation_tau;
    float decay = expf(-dt_ms / tau);

    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        /* Adaptation increases with activity, decays over time */
        bridge->adaptation_state[i] = bridge->adaptation_state[i] * decay +
            bridge->minicolumn_activations[i] * (1.0f - decay);
    }

    /* Global adaptation level affects gain */
    float mean_adaptation = 0;
    for (int i = 0; i < DRAGONFLY_CORTICAL_MINICOLUMN_COUNT; i++) {
        mean_adaptation += bridge->adaptation_state[i];
    }
    mean_adaptation /= DRAGONFLY_CORTICAL_MINICOLUMN_COUNT;

    bridge->stats.adaptation_level = mean_adaptation;

    /* Reduce gain with high adaptation */
    bridge->current_gain = bridge->config.gain_modulation / (1.0f + mean_adaptation);
    bridge->stats.current_gain = bridge->current_gain;

    return 0;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

int dragonfly_cortical_bridge_get_stats(
    const dragonfly_cortical_bridge_t* bridge,
    cortical_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_get_stats: NULL argument");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_cortical_bridge_reset_stats(dragonfly_cortical_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_reset_stats: bridge is NULL or not initialized");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.current_gain = bridge->current_gain;
    return 0;
}

int dragonfly_cortical_bridge_get_config(
    const dragonfly_cortical_bridge_t* bridge,
    dragonfly_cortical_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_get_config: NULL argument");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

int dragonfly_cortical_bridge_set_config(
    dragonfly_cortical_bridge_t* bridge,
    const dragonfly_cortical_config_t* config
) {
    if (!bridge || !bridge->initialized || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_cortical_bridge_set_config: NULL argument");
        return -1;
    }
    if (dragonfly_cortical_bridge_validate_config(config) != 0) {
        return -1;
    }

    bridge->config = *config;

    /* Reinitialize inhibition weights with new config */
    initialize_inhibition_weights(bridge);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_cortical_competition_name(cortical_competition_mode_t mode) {
    switch (mode) {
        case CORTICAL_COMPETE_WTA: return "winner-take-all";
        case CORTICAL_COMPETE_SOFTMAX: return "softmax";
        case CORTICAL_COMPETE_K_WINNERS: return "k-winners";
        case CORTICAL_COMPETE_NONE: return "none";
        default: return "unknown";
    }
}

const char* dragonfly_cortical_mapping_name(cortical_mapping_mode_t mode) {
    switch (mode) {
        case CORTICAL_MAP_DIRECT: return "direct";
        case CORTICAL_MAP_INTERPOLATED: return "interpolated";
        case CORTICAL_MAP_HIERARCHICAL: return "hierarchical";
        default: return "unknown";
    }
}

const char* dragonfly_cortical_hypercolumn_name(hypercolumn_type_t type) {
    switch (type) {
        case HYPERCOLUMN_AZIMUTH: return "azimuth";
        case HYPERCOLUMN_ELEVATION: return "elevation";
        case HYPERCOLUMN_HEADING: return "heading";
        default: return "unknown";
    }
}
