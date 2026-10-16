/**
 * @file nimcp_dragonfly_substrate_bridge.c
 * @brief Implementation of Dragonfly-to-Neural Substrate Bridge
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_substrate_bridge.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_substrate_bridge)

#define LOG_MODULE "DRAGONFLY_SUBSTRATE_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct dragonfly_substrate_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    bool initialized;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    neural_substrate_t* substrate;

    /* Configuration */
    dragonfly_substrate_config_t config;

    /* Current state */
    substrate_activity_level_t activity_level;
    float current_energy;           /* Internal ATP tracking [0-1] */
    float consumption_rate;         /* Current consumption rate */

    /* Performance modulation */
    dragonfly_substrate_modulation_t modulation;
    substrate_perf_impact_t impact;
    bool is_fatigued;

    /* Time tracking */
    float time_in_fatigue_ms;
    float last_update_dt_ms;

    /* Statistics */
    substrate_bridge_stats_t stats;
};

static void update_modulation(dragonfly_substrate_bridge_t* bridge) {
    float energy = bridge->current_energy;

    /* Compute performance modulation based on energy level */
    if (energy > bridge->config.mild_fatigue_threshold) {
        /* Normal performance */
        bridge->modulation.tracking_accuracy = 1.0f;
        bridge->modulation.reaction_time_factor = 1.0f;
        bridge->modulation.pursuit_speed = 1.0f;
        bridge->modulation.prediction_accuracy = 1.0f;
        bridge->modulation.decision_quality = 1.0f;
        bridge->impact = PERF_IMPACT_NONE;
        bridge->is_fatigued = false;
    } else if (energy > bridge->config.moderate_fatigue_threshold) {
        /* Mild fatigue */
        float factor = (energy - bridge->config.moderate_fatigue_threshold) /
                       (bridge->config.mild_fatigue_threshold - bridge->config.moderate_fatigue_threshold);
        float impact = 1.0f - (1.0f - factor) * bridge->config.fatigue_accuracy_impact * 0.3f;

        bridge->modulation.tracking_accuracy = impact;
        bridge->modulation.reaction_time_factor = 1.0f + (1.0f - factor) * 0.2f;
        bridge->modulation.pursuit_speed = 1.0f - (1.0f - factor) * bridge->config.fatigue_speed_impact * 0.2f;
        bridge->modulation.prediction_accuracy = impact;
        bridge->modulation.decision_quality = 1.0f - (1.0f - factor) * 0.1f;
        bridge->impact = PERF_IMPACT_MILD;
        bridge->is_fatigued = true;
    } else if (energy > bridge->config.severe_fatigue_threshold) {
        /* Moderate fatigue */
        float factor = (energy - bridge->config.severe_fatigue_threshold) /
                       (bridge->config.moderate_fatigue_threshold - bridge->config.severe_fatigue_threshold);
        float accuracy_impact = factor * 0.7f + 0.3f;
        float speed_impact = factor * 0.6f + 0.4f;

        bridge->modulation.tracking_accuracy = accuracy_impact;
        bridge->modulation.reaction_time_factor = 1.0f + (1.0f - factor) * 0.5f;
        bridge->modulation.pursuit_speed = speed_impact;
        bridge->modulation.prediction_accuracy = accuracy_impact * 0.9f;
        bridge->modulation.decision_quality = factor * 0.6f + 0.4f;
        bridge->impact = PERF_IMPACT_MODERATE;
        bridge->is_fatigued = true;
    } else if (energy > 0.1f) {
        /* Severe fatigue */
        float factor = (energy - 0.1f) / (bridge->config.severe_fatigue_threshold - 0.1f);

        bridge->modulation.tracking_accuracy = factor * 0.3f + 0.2f;
        bridge->modulation.reaction_time_factor = 1.0f + (1.0f - factor) * 1.0f;
        bridge->modulation.pursuit_speed = factor * 0.4f + 0.2f;
        bridge->modulation.prediction_accuracy = factor * 0.3f + 0.1f;
        bridge->modulation.decision_quality = factor * 0.4f + 0.2f;
        bridge->impact = PERF_IMPACT_SEVERE;
        bridge->is_fatigued = true;
    } else {
        /* Critical - near failure */
        bridge->modulation.tracking_accuracy = 0.1f;
        bridge->modulation.reaction_time_factor = 2.0f;
        bridge->modulation.pursuit_speed = 0.1f;
        bridge->modulation.prediction_accuracy = 0.05f;
        bridge->modulation.decision_quality = 0.1f;
        bridge->impact = PERF_IMPACT_CRITICAL;
        bridge->is_fatigued = true;
    }

    /* Compute overall performance */
    bridge->modulation.overall_performance =
        (bridge->modulation.tracking_accuracy +
         bridge->modulation.pursuit_speed +
         bridge->modulation.prediction_accuracy +
         bridge->modulation.decision_quality) / 4.0f;

    /* Update statistics */
    if (bridge->is_fatigued) {
        bridge->stats.time_in_fatigue_ms += bridge->last_update_dt_ms;
        bridge->stats.fatigue_events++;
    }

    if (bridge->modulation.overall_performance < bridge->stats.min_performance_level ||
        bridge->stats.min_performance_level == 0) {
        bridge->stats.min_performance_level = bridge->modulation.overall_performance;
    }

    float alpha = 0.01f;
    {
        float ema = bridge->stats.avg_performance_level * (1.0f - alpha) +
                    bridge->modulation.overall_performance * alpha;
        if (isfinite(ema)) bridge->stats.avg_performance_level = ema;
    }
}

static void consume_energy(dragonfly_substrate_bridge_t* bridge, float amount) {
    bridge->current_energy -= amount;
    if (bridge->current_energy < 0) bridge->current_energy = 0;

    bridge->stats.total_energy_consumed += amount;

    /* Update consumption rate (exponential moving average) */
    float rate = amount / (bridge->last_update_dt_ms + 0.001f);
    {
        float ema = bridge->consumption_rate * 0.9f + rate * 0.1f;
        if (isfinite(ema)) bridge->consumption_rate = ema;
    }

    if (rate > bridge->stats.peak_consumption_rate) {
        bridge->stats.peak_consumption_rate = rate;
    }

    /* Sync to substrate if connected */
    if (bridge->substrate && bridge->config.enable_substrate_feedback) {
        substrate_set_atp(bridge->substrate, bridge->current_energy);
    }

    /* Update modulation based on new energy level */
    update_modulation(bridge);
}

static void recover_energy(dragonfly_substrate_bridge_t* bridge, float dt_ms) {
    if (!bridge->config.enable_recovery) return;

    float rate = (bridge->activity_level == SUBSTRATE_ACTIVITY_IDLE) ?
        bridge->config.rest_recovery_rate :
        bridge->config.active_recovery_rate;

    bridge->current_energy += rate * dt_ms;
    if (bridge->current_energy > 1.0f) bridge->current_energy = 1.0f;
}

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_substrate_bridge_default_config(dragonfly_substrate_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_bridge_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(*config));

    /* Energy costs */
    config->costs.tsdn_update = SUBSTRATE_COST_TSDN_UPDATE;
    config->costs.tracking_step = SUBSTRATE_COST_TRACKING;
    config->costs.prediction_step = SUBSTRATE_COST_PREDICTION;
    config->costs.intercept_calc = SUBSTRATE_COST_INTERCEPT_CALC;
    config->costs.mode_switch = SUBSTRATE_COST_MODE_SWITCH;
    config->costs.pursuit_flight = SUBSTRATE_COST_PURSUIT_FLIGHT;
    config->costs.idle_baseline = 0.00001f;

    /* Fatigue thresholds */
    config->mild_fatigue_threshold = 0.7f;
    config->moderate_fatigue_threshold = 0.4f;
    config->severe_fatigue_threshold = 0.2f;

    /* Recovery */
    config->rest_recovery_rate = 0.0001f;   /* Recovery per ms at rest */
    config->active_recovery_rate = 0.00002f; /* Recovery per ms while active */

    /* Impact scales */
    config->fatigue_accuracy_impact = 0.5f;
    config->fatigue_speed_impact = 0.6f;

    /* Features */
    config->enable_fatigue_modeling = true;
    config->enable_recovery = true;
    config->enable_substrate_feedback = false;

    return 0;
}

int dragonfly_substrate_bridge_validate_config(const dragonfly_substrate_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->mild_fatigue_threshold <= config->moderate_fatigue_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_validate_config: mild_fatigue_threshold <= moderate_fatigue_threshold");
        return -1;
    }
    if (config->moderate_fatigue_threshold <= config->severe_fatigue_threshold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_validate_config: moderate_fatigue_threshold <= severe_fatigue_threshold");
        return -1;
    }
    if (config->severe_fatigue_threshold < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_validate_config: severe_fatigue_threshold < 0");
        return -1;
    }
    if (config->fatigue_accuracy_impact < 0 || config->fatigue_accuracy_impact > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_validate_config: fatigue_accuracy_impact out of range [0, 1]");
        return -1;
    }
    if (config->fatigue_speed_impact < 0 || config->fatigue_speed_impact > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_validate_config: fatigue_speed_impact out of range [0, 1]");
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_substrate_bridge_t* dragonfly_substrate_bridge_create(
    dragonfly_system_t* dragonfly,
    void* substrate,
    const dragonfly_substrate_config_t* config
) {
    dragonfly_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_substrate_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_substrate_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        if (dragonfly_substrate_bridge_validate_config(config) != 0) {
            nimcp_free(bridge);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_substrate_bridge_create: invalid config");
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_substrate_bridge_default_config(&bridge->config);
    }

    /* Connect systems */
    bridge->dragonfly = dragonfly;
    bridge->substrate = substrate;

    /* Initialize state */
    bridge->activity_level = SUBSTRATE_ACTIVITY_IDLE;
    bridge->current_energy = 1.0f;  /* Start fully charged */
    bridge->consumption_rate = 0.0f;
    bridge->is_fatigued = false;
    bridge->impact = PERF_IMPACT_NONE;
    bridge->last_update_dt_ms = 16.67f;  /* Assume ~60Hz */

    /* Initialize modulation to full performance */
    bridge->modulation.tracking_accuracy = 1.0f;
    bridge->modulation.reaction_time_factor = 1.0f;
    bridge->modulation.pursuit_speed = 1.0f;
    bridge->modulation.prediction_accuracy = 1.0f;
    bridge->modulation.decision_quality = 1.0f;
    bridge->modulation.overall_performance = 1.0f;

    bridge->stats.min_performance_level = 1.0f;
    bridge->stats.avg_performance_level = 1.0f;

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_substrate") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->initialized = true;
    return bridge;
}

void dragonfly_substrate_bridge_destroy(dragonfly_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_substrate");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_substrate_bridge_reset(dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_bridge_reset: bridge is NULL or not initialized");
        return -1;
    }

    bridge->activity_level = SUBSTRATE_ACTIVITY_IDLE;
    bridge->current_energy = 1.0f;
    bridge->consumption_rate = 0.0f;
    bridge->is_fatigued = false;
    bridge->impact = PERF_IMPACT_NONE;
    bridge->time_in_fatigue_ms = 0.0f;

    bridge->modulation.tracking_accuracy = 1.0f;
    bridge->modulation.reaction_time_factor = 1.0f;
    bridge->modulation.pursuit_speed = 1.0f;
    bridge->modulation.prediction_accuracy = 1.0f;
    bridge->modulation.decision_quality = 1.0f;
    bridge->modulation.overall_performance = 1.0f;

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_performance_level = 1.0f;
    bridge->stats.avg_performance_level = 1.0f;

    return 0;
}

//=============================================================================
// Energy Consumption
//=============================================================================

int dragonfly_substrate_record_tsdn_update(
    dragonfly_substrate_bridge_t* bridge,
    uint32_t population_size
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_tsdn_update: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    float cost = bridge->config.costs.tsdn_update * (float)population_size;
    consume_energy(bridge, cost);
    bridge->stats.tsdn_updates++;

    return 0;
}

int dragonfly_substrate_record_tracking(
    dragonfly_substrate_bridge_t* bridge,
    uint32_t num_targets
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_tracking: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    float cost = bridge->config.costs.tracking_step * (float)num_targets;
    consume_energy(bridge, cost);
    bridge->stats.tracking_steps++;

    return 0;
}

int dragonfly_substrate_record_prediction(
    dragonfly_substrate_bridge_t* bridge,
    float complexity
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_prediction: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    complexity = nimcp_clampf(complexity, 0.0f, 1.0f);
    float cost = bridge->config.costs.prediction_step * (0.5f + 0.5f * complexity);
    consume_energy(bridge, cost);
    bridge->stats.prediction_steps++;

    return 0;
}

int dragonfly_substrate_record_intercept_calc(
    dragonfly_substrate_bridge_t* bridge,
    float nav_complexity
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_intercept_calc: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    nav_complexity = nimcp_clampf(nav_complexity, 0.0f, 1.0f);
    float cost = bridge->config.costs.intercept_calc * (0.5f + 0.5f * nav_complexity);
    consume_energy(bridge, cost);
    bridge->stats.intercept_calcs++;

    return 0;
}

int dragonfly_substrate_record_mode_switch(dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_mode_switch: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    consume_energy(bridge, bridge->config.costs.mode_switch);
    bridge->stats.mode_switches++;

    return 0;
}

int dragonfly_substrate_record_pursuit(
    dragonfly_substrate_bridge_t* bridge,
    float intensity
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_record_pursuit: bridge is NULL or not initialized");
        return -1;
    }
    if (!bridge->config.enable_fatigue_modeling) return 0;

    intensity = nimcp_clampf(intensity, 0.0f, 1.0f);
    float cost = bridge->config.costs.pursuit_flight * intensity;
    consume_energy(bridge, cost);
    bridge->stats.pursuit_steps++;

    return 0;
}

//=============================================================================
// Performance Modulation
//=============================================================================

int dragonfly_substrate_get_modulation(
    const dragonfly_substrate_bridge_t* bridge,
    dragonfly_substrate_modulation_t* mod
) {
    if (!bridge || !bridge->initialized || !mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_get_modulation: bridge, mod is NULL or bridge not initialized");
        return -1;
    }
    *mod = bridge->modulation;
    return 0;
}

float dragonfly_substrate_get_tracking_accuracy(
    const dragonfly_substrate_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return 1.0f;
    return bridge->modulation.tracking_accuracy;
}

float dragonfly_substrate_get_pursuit_speed(
    const dragonfly_substrate_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return 1.0f;
    return bridge->modulation.pursuit_speed;
}

float dragonfly_substrate_get_reaction_factor(
    const dragonfly_substrate_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return 1.0f;
    return bridge->modulation.reaction_time_factor;
}

substrate_perf_impact_t dragonfly_substrate_get_impact(
    const dragonfly_substrate_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return PERF_IMPACT_NONE;
    return bridge->impact;
}

bool dragonfly_substrate_is_fatigued(const dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        return false;
    }
    return bridge->is_fatigued;
}

//=============================================================================
// Activity Tracking
//=============================================================================

int dragonfly_substrate_set_activity(
    dragonfly_substrate_bridge_t* bridge,
    substrate_activity_level_t level
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_set_activity: bridge is NULL or not initialized");
        return -1;
    }
    bridge->activity_level = level;
    return 0;
}

substrate_activity_level_t dragonfly_substrate_get_activity(
    const dragonfly_substrate_bridge_t* bridge
) {
    if (!bridge || !bridge->initialized) return SUBSTRATE_ACTIVITY_IDLE;
    return bridge->activity_level;
}

float dragonfly_substrate_get_energy(const dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) return 1.0f;
    return bridge->current_energy;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_substrate_connect_dragonfly(
    dragonfly_substrate_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_connect_dragonfly: bridge is NULL or not initialized");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_substrate_connect_substrate(
    dragonfly_substrate_bridge_t* bridge,
    void* substrate
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_connect_substrate: bridge is NULL or not initialized");
        return -1;
    }
    bridge->substrate = substrate;
    return 0;
}

bool dragonfly_substrate_has_dragonfly(const dragonfly_substrate_bridge_t* bridge) {
    return bridge && bridge->initialized && bridge->dragonfly != NULL;
}

bool dragonfly_substrate_has_substrate(const dragonfly_substrate_bridge_t* bridge) {
    return bridge && bridge->initialized && bridge->substrate != NULL;
}

//=============================================================================
// Update
//=============================================================================

int dragonfly_substrate_update(dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_update: bridge is NULL or not initialized");
        return -1;
    }

    /* Sync with substrate if connected */
    if (bridge->substrate) {
        substrate_modulation_t sub_mod;
        if (substrate_get_modulation(bridge->substrate, &sub_mod) == 0) {
            /* Use substrate's modulation factors as additional constraints */
            bridge->modulation.tracking_accuracy *= sub_mod.firing_rate_mod;
            bridge->modulation.pursuit_speed *= sub_mod.transmission_efficiency;
            bridge->modulation.prediction_accuracy *= sub_mod.plasticity_capacity;
        }
    }

    /* Update activity level based on dragonfly mode if connected */
    if (bridge->dragonfly) {
        dragonfly_mode_t mode = dragonfly_get_mode(bridge->dragonfly);
        switch (mode) {
            case DRAGONFLY_MODE_IDLE:
                bridge->activity_level = SUBSTRATE_ACTIVITY_IDLE;
                break;
            case DRAGONFLY_MODE_SCANNING:
                bridge->activity_level = SUBSTRATE_ACTIVITY_SCANNING;
                break;
            case DRAGONFLY_MODE_TRACKING:
                bridge->activity_level = SUBSTRATE_ACTIVITY_TRACKING;
                break;
            case DRAGONFLY_MODE_PURSUING:
                bridge->activity_level = SUBSTRATE_ACTIVITY_PURSUIT;
                break;
            case DRAGONFLY_MODE_INTERCEPTING:
                bridge->activity_level = SUBSTRATE_ACTIVITY_INTERCEPT;
                break;
        }
    }

    return 0;
}

int dragonfly_substrate_step(
    dragonfly_substrate_bridge_t* bridge,
    float dt_ms
) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_step: bridge is NULL or not initialized");
        return -1;
    }

    bridge->last_update_dt_ms = dt_ms;

    /* Baseline energy consumption */
    if (bridge->config.enable_fatigue_modeling) {
        float baseline_cost = bridge->config.costs.idle_baseline * dt_ms;

        /* Scale by activity level */
        switch (bridge->activity_level) {
            case SUBSTRATE_ACTIVITY_SCANNING:
                baseline_cost *= 2.0f;
                break;
            case SUBSTRATE_ACTIVITY_TRACKING:
                baseline_cost *= 4.0f;
                break;
            case SUBSTRATE_ACTIVITY_PURSUIT:
                baseline_cost *= 8.0f;
                break;
            case SUBSTRATE_ACTIVITY_INTERCEPT:
                baseline_cost *= 12.0f;
                break;
            default:
                break;
        }

        consume_energy(bridge, baseline_cost);
    }

    /* Recovery */
    recover_energy(bridge, dt_ms);

    /* Update from connected systems */
    dragonfly_substrate_update(bridge);

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_substrate_bridge_get_stats(
    const dragonfly_substrate_bridge_t* bridge,
    substrate_bridge_stats_t* stats
) {
    if (!bridge || !bridge->initialized || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_bridge_get_stats: bridge, stats is NULL or bridge not initialized");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_substrate_bridge_reset_stats(dragonfly_substrate_bridge_t* bridge) {
    if (!bridge || !bridge->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_substrate_bridge_reset_stats: bridge is NULL or not initialized");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.min_performance_level = 1.0f;
    bridge->stats.avg_performance_level = 1.0f;
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_substrate_activity_name(substrate_activity_level_t level) {
    switch (level) {
        case SUBSTRATE_ACTIVITY_IDLE:      return "idle";
        case SUBSTRATE_ACTIVITY_SCANNING:  return "scanning";
        case SUBSTRATE_ACTIVITY_TRACKING:  return "tracking";
        case SUBSTRATE_ACTIVITY_PURSUIT:   return "pursuit";
        case SUBSTRATE_ACTIVITY_INTERCEPT: return "intercept";
        default:                           return "unknown";
    }
}

const char* dragonfly_substrate_impact_name(substrate_perf_impact_t impact) {
    switch (impact) {
        case PERF_IMPACT_NONE:     return "none";
        case PERF_IMPACT_MILD:     return "mild";
        case PERF_IMPACT_MODERATE: return "moderate";
        case PERF_IMPACT_SEVERE:   return "severe";
        case PERF_IMPACT_CRITICAL: return "critical";
        default:                   return "unknown";
    }
}
