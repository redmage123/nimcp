/**
 * @file nimcp_arousal_modulation.c
 * @brief Arousal Modulation System Implementation
 * @version 1.0.0
 * @date 2026-01-11
 */

#include "core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(arousal_modulation, MESH_ADAPTER_CATEGORY_COGNITIVE)

//=============================================================================
// Lifecycle Implementation
//=============================================================================

nimcp_arousal_config_t nimcp_arousal_default_config(void) {
    nimcp_arousal_config_t config;
    memset(&config, 0, sizeof(config));

    /* NE-arousal mapping */
    config.optimal_ne = AROUSAL_OPTIMAL_NE;
    config.curve_type = AROUSAL_CURVE_INVERTED_U;
    config.curve_steepness = 2.0f;

    /* Time constants */
    config.arousal_tau_ms = AROUSAL_DECAY_TAU_MS;
    config.vigilance_tau_ms = AROUSAL_VIGILANCE_TAU_MS;
    config.alertness_tau_ms = 200.0f;

    /* Gain parameters */
    config.max_gain = AROUSAL_MAX_GAIN;
    config.min_gain = AROUSAL_MIN_GAIN;
    config.baseline_gain = 1.0f;

    /* Circadian */
    config.enable_circadian = false;
    config.circadian_amplitude = 0.2f;

    /* Thresholds */
    config.drowsy_threshold = 0.2f;
    config.alert_threshold = 0.5f;
    config.hyperaroused_threshold = 0.85f;

    return config;
}

int nimcp_arousal_init(
    nimcp_arousal_system_t* system,
    const nimcp_arousal_config_t* config
) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(nimcp_arousal_system_t));

    nimcp_arousal_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = nimcp_arousal_default_config();
    }

    /* Set configuration values */
    system->curve_type = cfg.curve_type;
    system->optimal_ne = cfg.optimal_ne;
    system->arousal_tau = cfg.arousal_tau_ms;
    system->vigilance_tau = cfg.vigilance_tau_ms;

    /* Initialize state */
    system->state = AROUSAL_STATE_RELAXED;

    /* Initialize dimensions */
    system->dimensions.arousal = 0.5f;
    system->dimensions.alertness = 0.5f;
    system->dimensions.vigilance = 0.5f;
    system->dimensions.activation = 0.5f;

    /* Initialize gain */
    system->gain.signal_gain = cfg.baseline_gain;
    system->gain.noise_suppression = 0.5f;
    system->gain.signal_to_noise = 1.0f;
    system->gain.responsiveness = 0.5f;

    /* Initialize performance */
    system->performance.cognitive_efficiency = 0.7f;
    system->performance.reaction_time_modifier = 1.0f;
    system->performance.accuracy_modifier = 1.0f;
    system->performance.fatigue_level = 0.0f;

    /* Initialize internal state */
    system->ne_input = 0.0f;
    system->target_arousal = 0.5f;
    system->arousal_velocity = 0.0f;

    /* Initialize history */
    system->history_index = 0;
    system->mean_arousal = 0.5f;

    /* Initialize circadian/homeostatic */
    system->circadian_drive = 1.0f;
    system->homeostatic_pressure = 0.0f;

    system->initialized = true;
    system->current_time = 0.0f;

    return 0;
}

int nimcp_arousal_shutdown(nimcp_arousal_system_t* system) {
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "system is NULL");

        return -1;
    }

    memset(system, 0, sizeof(nimcp_arousal_system_t));
    return 0;
}

int nimcp_arousal_reset(nimcp_arousal_system_t* system) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_reset: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Reset to defaults while keeping configuration */
    float opt = system->optimal_ne;
    float arousal_tau = system->arousal_tau;
    float vigilance_tau = system->vigilance_tau;
    nimcp_arousal_curve_t curve = system->curve_type;

    nimcp_arousal_config_t cfg = nimcp_arousal_default_config();
    cfg.optimal_ne = opt;
    cfg.arousal_tau_ms = arousal_tau;
    cfg.vigilance_tau_ms = vigilance_tau;
    cfg.curve_type = curve;

    system->initialized = false;
    return nimcp_arousal_init(system, &cfg);
}

//=============================================================================
// Update Implementation
//=============================================================================

int nimcp_arousal_update(
    nimcp_arousal_system_t* system,
    float ne_concentration,
    float dt
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_update: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_arousal_update: validation failed");
        return -1;
    }

    system->ne_input = ne_concentration;

    /* Compute target arousal from NE */
    float normalized_ne = (fabsf(system->optimal_ne) > 1e-10f) ?
        ne_concentration / system->optimal_ne : 0.0f;
    system->target_arousal = nimcp_arousal_ne_to_performance(ne_concentration,
                                                              system->optimal_ne,
                                                              system->curve_type);

    /* Apply circadian modulation */
    system->target_arousal *= system->circadian_drive;

    /* Apply homeostatic pressure (fatigue decreases arousal) */
    system->target_arousal *= (1.0f - system->homeostatic_pressure * 0.5f);

    system->target_arousal = nimcp_clampf(system->target_arousal, 0.0f, 1.0f);

    /* Smooth arousal transition */
    float alpha = (fabsf(system->arousal_tau) > 1e-10f) ?
        1.0f - expf(-dt / system->arousal_tau) : 1.0f;
    float old_arousal = system->dimensions.arousal;
    system->dimensions.arousal += alpha * (system->target_arousal - system->dimensions.arousal);

    system->arousal_velocity = (system->dimensions.arousal - old_arousal) / dt;

    /* Update alertness (faster dynamics) */
    float alert_tau = 200.0f;
    alpha = 1.0f - expf(-dt / alert_tau);
    float target_alertness = system->dimensions.arousal * 1.2f;
    target_alertness = nimcp_clampf(target_alertness, 0.0f, 1.0f);
    system->dimensions.alertness += alpha * (target_alertness - system->dimensions.alertness);

    /* Update vigilance (slower dynamics, decays without input) */
    alpha = (fabsf(system->vigilance_tau) > 1e-10f) ?
        1.0f - expf(-dt / system->vigilance_tau) : 1.0f;
    float target_vigilance = system->dimensions.arousal * 0.8f;
    system->dimensions.vigilance += alpha * (target_vigilance - system->dimensions.vigilance);

    /* Update activation */
    system->dimensions.activation = (system->dimensions.arousal + system->dimensions.alertness) * 0.5f;

    /* Update gain modulation */
    /* Gain follows inverted-U: optimal at moderate arousal */
    float gain_optimal = 0.6f;
    float arousal_deviation = fabsf(system->dimensions.arousal - gain_optimal);
    float base_gain = 1.0f + 0.5f * (1.0f - arousal_deviation);
    system->gain.signal_gain = nimcp_clampf(base_gain, AROUSAL_MIN_GAIN, AROUSAL_MAX_GAIN);

    /* Noise suppression increases with NE (up to a point) */
    system->gain.noise_suppression = 0.3f + 0.5f * system->dimensions.arousal;
    system->gain.noise_suppression = nimcp_clampf(system->gain.noise_suppression, 0.0f, 1.0f);

    /* Signal to noise */
    system->gain.signal_to_noise = system->gain.signal_gain *
                                    (0.5f + 0.5f * system->gain.noise_suppression);

    /* Responsiveness */
    system->gain.responsiveness = system->dimensions.alertness * system->dimensions.arousal;

    /* Update performance metrics */
    system->performance.cognitive_efficiency = nimcp_arousal_ne_to_performance(
        ne_concentration, system->optimal_ne, AROUSAL_CURVE_INVERTED_U);

    /* Reaction time: faster at optimal arousal */
    float rt_modifier = 1.0f + 0.5f * arousal_deviation;
    system->performance.reaction_time_modifier = nimcp_clampf(rt_modifier, 0.5f, 2.0f);

    /* Accuracy: decreases at extremes */
    system->performance.accuracy_modifier = system->performance.cognitive_efficiency;

    /* Fatigue accumulates with time and high arousal */
    float fatigue_rate = 0.0001f * (1.0f + system->dimensions.arousal);
    float fatigue_recovery = 0.0002f * (1.0f - system->dimensions.arousal);
    system->performance.fatigue_level += dt * (fatigue_rate - fatigue_recovery);
    system->performance.fatigue_level = nimcp_clampf(system->performance.fatigue_level, 0.0f, 1.0f);

    /* Update homeostatic pressure from fatigue */
    system->homeostatic_pressure = system->performance.fatigue_level * 0.5f;

    /* Update arousal state label (for logging/display only).
     * All behavioral dispatch above uses continuous dimensions directly.
     * This label is derived FROM the continuous arousal value. */
    float arousal = system->dimensions.arousal;
    if (arousal < 0.1f) {
        system->state = AROUSAL_STATE_SLEEP;
    } else if (arousal < 0.2f) {
        system->state = AROUSAL_STATE_DROWSY;
    } else if (arousal < 0.4f) {
        system->state = AROUSAL_STATE_RELAXED;
    } else if (arousal < 0.7f) {
        system->state = AROUSAL_STATE_ALERT;
    } else if (arousal < 0.85f) {
        system->state = AROUSAL_STATE_VIGILANT;
    } else if (arousal < 0.95f) {
        system->state = AROUSAL_STATE_HYPERAROUSED;
    } else {
        system->state = AROUSAL_STATE_STRESSED;
    }

    /* Update history */
    system->arousal_history[system->history_index] = system->dimensions.arousal;
    system->history_index = (system->history_index + 1) % 32;

    /* Update mean arousal */
    float sum = 0.0f;
    for (int i = 0; i < 32; i++) {
        sum += system->arousal_history[i];
    }
    system->mean_arousal = sum / 32.0f;

    system->current_time += dt;
    return 0;
}

int nimcp_arousal_set_target(nimcp_arousal_system_t* system, float target) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_set_target: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    system->target_arousal = nimcp_clampf(target, 0.0f, 1.0f);
    return 0;
}

int nimcp_arousal_apply_circadian(nimcp_arousal_system_t* system, float time_of_day) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_apply_circadian: required parameter is NULL (system, system->initialized)");
        return -1;
    }

    /* Circadian rhythm: peaks around 10am and 6pm, dips at 2-4am and 2-4pm */
    float hour = fmodf(time_of_day, 24.0f);

    /* Simple sinusoidal model with two peaks */
    float morning_peak = 10.0f;
    float afternoon_dip = 14.0f;
    float evening_peak = 18.0f;

    float drive = 0.0f;
    if (hour < morning_peak) {
        drive = 0.5f + 0.5f * sinf((hour / morning_peak) * NIMCP_PI_F / 2.0f);
    } else if (hour < afternoon_dip) {
        drive = 1.0f - 0.2f * (hour - morning_peak) / (afternoon_dip - morning_peak);
    } else if (hour < evening_peak) {
        drive = 0.8f + 0.2f * (hour - afternoon_dip) / (evening_peak - afternoon_dip);
    } else {
        drive = 1.0f - 0.5f * (hour - evening_peak) / (24.0f - evening_peak);
    }

    system->circadian_drive = nimcp_clampf(drive, 0.3f, 1.0f);
    return 0;
}

//=============================================================================
// Query Implementation
//=============================================================================

nimcp_arousal_state_t nimcp_arousal_get_state(const nimcp_arousal_system_t* system) {
    if (!system || !system->initialized) {
        return AROUSAL_STATE_RELAXED;
    }
    return system->state;
}

int nimcp_arousal_get_dimensions(
    const nimcp_arousal_system_t* system,
    nimcp_arousal_dimensions_t* dimensions
) {
    if (!system || !dimensions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_dimensions: required parameter is NULL (system, dimensions)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_dimensions: system->initialized is NULL");
        return -1;
    }

    *dimensions = system->dimensions;
    return 0;
}

int nimcp_arousal_get_gain(
    const nimcp_arousal_system_t* system,
    nimcp_gain_modulation_t* gain
) {
    if (!system || !gain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_gain: required parameter is NULL (system, gain)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_gain: system->initialized is NULL");
        return -1;
    }

    *gain = system->gain;
    return 0;
}

int nimcp_arousal_get_performance(
    const nimcp_arousal_system_t* system,
    nimcp_arousal_performance_t* performance
) {
    if (!system || !performance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_performance: required parameter is NULL (system, performance)");
        return -1;
    }

    if (!system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_arousal_get_performance: system->initialized is NULL");
        return -1;
    }

    *performance = system->performance;
    return 0;
}

float nimcp_arousal_ne_to_performance(
    float ne_level,
    float optimal_ne,
    nimcp_arousal_curve_t curve_type
) {
    if (optimal_ne <= 0.0f) {
        return 0.5f;
    }

    float normalized = ne_level / optimal_ne;

    switch (curve_type) {
        case AROUSAL_CURVE_LINEAR: {
            /* Simple linear increase then decrease */
            if (normalized < 1.0f) {
                return normalized;
            } else {
                return 2.0f - normalized;
            }
        }

        case AROUSAL_CURVE_INVERTED_U: {
            /* Yerkes-Dodson law: optimal at moderate arousal */
            /* Gaussian-like curve centered at optimal */
            float dev = normalized - 1.0f;
            float sigma = 0.5f;  /* Width of optimal range */
            return expf(-(dev * dev) / (2.0f * sigma * sigma));
        }

        case AROUSAL_CURVE_SIGMOID: {
            /* Sigmoidal: slow increase, plateau */
            float k = 3.0f;  /* Steepness */
            return 1.0f / (1.0f + expf(-k * (normalized - 0.5f)));
        }

        default:
            return 0.5f;
    }
}
