/**
 * @file nimcp_dragonfly_immune_bridge.c
 * @brief Immune-Dragonfly Integration Bridge Implementation
 *
 * WHAT: Integrates immune system state with hunting behavior
 * WHY:  Enables realistic energy conservation and stress responses
 * HOW:  Bidirectional communication with BBB and immune system
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/rng/nimcp_rand.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_immune_bridge)

#define LOG_MODULE "DRAGONFLY_IMMUNE_BRIDGE"


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

struct dragonfly_immune_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    dragonfly_immune_config_t config;

    /* Connected systems */
    dragonfly_system_t* dragonfly;
    bbb_system_t bbb;
    bool connected;

    /* Current state */
    dragonfly_immune_state_t state;

    /* Statistics */
    dragonfly_immune_stats_t stats;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_health_status_name(health_status_t status) {
    switch (status) {
        case HEALTH_OPTIMAL:           return "optimal";
        case HEALTH_MILD_IMPAIRMENT:   return "mild_impairment";
        case HEALTH_MODERATE_IMPAIRMENT: return "moderate_impairment";
        case HEALTH_SEVERE_IMPAIRMENT: return "severe_impairment";
        case HEALTH_CRITICAL:          return "critical";
        default:                       return "unknown";
    }
}

const char* dragonfly_stress_level_name(stress_level_t level) {
    switch (level) {
        case STRESS_NONE:     return "none";
        case STRESS_LOW:      return "low";
        case STRESS_MODERATE: return "moderate";
        case STRESS_HIGH:     return "high";
        case STRESS_CHRONIC:  return "chronic";
        default:              return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

dragonfly_immune_config_t dragonfly_immune_default_config(void) {
    dragonfly_immune_config_t config = {
        /* Health thresholds */
        .mild_impairment_threshold = 0.8f,
        .moderate_impairment_threshold = 0.6f,
        .severe_impairment_threshold = 0.4f,
        .critical_threshold = 0.2f,

        /* Stress accumulation */
        .failure_stress_increment = 0.1f,
        .success_stress_decrement = 0.15f,
        .stress_decay_rate = 0.05f,
        .failure_frustration_threshold = 3,

        /* Energy management */
        .energy_per_pursuit_j = 0.1f,
        .energy_recovery_rate = 0.02f,
        .min_energy_for_hunt = 0.3f,

        /* Injury modeling */
        .injury_probability_base = 0.01f,
        .injury_fatigue_factor = 2.0f,
        .injury_recovery_time_s = 60.0f,

        /* Feedback to immune */
        .enable_immune_feedback = true,
        .immune_stress_weight = 0.5f
    };
    return config;
}

bool dragonfly_immune_validate_config(const dragonfly_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_validate_config: config is NULL");
        return false;
    }

    /* Check threshold ordering */
    if (config->mild_impairment_threshold < config->moderate_impairment_threshold) {
        return false;
    }
    if (config->moderate_impairment_threshold < config->severe_impairment_threshold) {
        return false;
    }
    if (config->severe_impairment_threshold < config->critical_threshold) {
        return false;
    }

    /* Check thresholds in valid range */
    if (config->critical_threshold < 0.0f) {
        return false;
    }
    if (config->mild_impairment_threshold > 1.0f) {
        return false;
    }

    /* Check positive values */
    if (config->failure_stress_increment < 0.0f) {
        return false;
    }
    if (config->success_stress_decrement < 0.0f) {
        return false;
    }
    if (config->stress_decay_rate < 0.0f) {
        return false;
    }
    if (config->energy_per_pursuit_j < 0.0f) {
        return false;
    }
    if (config->energy_recovery_rate < 0.0f) {
        return false;
    }
    if (config->min_energy_for_hunt < 0.0f || config->min_energy_for_hunt > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static health_status_t compute_health_status(
    const dragonfly_immune_bridge_t bridge,
    float health_level
) {
    if (health_level >= bridge->config.mild_impairment_threshold) {
        return HEALTH_OPTIMAL;
    } else if (health_level >= bridge->config.moderate_impairment_threshold) {
        return HEALTH_MILD_IMPAIRMENT;
    } else if (health_level >= bridge->config.severe_impairment_threshold) {
        return HEALTH_MODERATE_IMPAIRMENT;
    } else if (health_level >= bridge->config.critical_threshold) {
        return HEALTH_SEVERE_IMPAIRMENT;
    } else {
        return HEALTH_CRITICAL;
    }
}

static stress_level_t compute_stress_level(float stress_value) {
    if (stress_value < 0.1f) return STRESS_NONE;
    if (stress_value < 0.3f) return STRESS_LOW;
    if (stress_value < 0.5f) return STRESS_MODERATE;
    if (stress_value < 0.8f) return STRESS_HIGH;
    return STRESS_CHRONIC;
}

static void update_modulation(dragonfly_immune_bridge_t bridge) {
    immune_modulation_t* mod = &bridge->state.modulation;
    hunting_stress_t* stress = &bridge->state.stress_report;
    health_status_t health = bridge->state.health_status;

    /* Base modifiers from health status */
    float health_factor = 1.0f;
    switch (health) {
        case HEALTH_OPTIMAL:           health_factor = 1.0f; break;
        case HEALTH_MILD_IMPAIRMENT:   health_factor = 0.9f; break;
        case HEALTH_MODERATE_IMPAIRMENT: health_factor = 0.7f; break;
        case HEALTH_SEVERE_IMPAIRMENT: health_factor = 0.4f; break;
        case HEALTH_CRITICAL:          health_factor = 0.1f; break;
    }

    /* Fatigue effect */
    float fatigue_factor = 1.0f - stress->fatigue_level * 0.5f;

    /* Stress effect (low stress can be energizing) */
    float stress_factor = 1.0f;
    switch (bridge->state.stress_level) {
        case STRESS_NONE:     stress_factor = 1.0f; break;
        case STRESS_LOW:      stress_factor = 1.1f; break;  /* Energizing */
        case STRESS_MODERATE: stress_factor = 0.95f; break;
        case STRESS_HIGH:     stress_factor = 0.8f; break;
        case STRESS_CHRONIC:  stress_factor = 0.5f; break;
    }

    /* Compute final modifiers */
    float combined = health_factor * fatigue_factor * stress_factor;

    mod->speed_modifier = nimcp_clampf(combined, 0.0f, 1.0f);
    mod->accuracy_modifier = nimcp_clampf(combined * 1.05f, 0.0f, 1.0f);
    mod->endurance_modifier = nimcp_clampf(health_factor * (1.0f - stress->fatigue_level * 0.7f), 0.0f, 1.0f);
    mod->reaction_modifier = nimcp_clampf(combined * stress_factor, 0.0f, 1.0f);

    /* Behavioral modifiers */
    mod->hunting_recommended = (health >= HEALTH_MODERATE_IMPAIRMENT) &&
                               (stress->energy_reserves >= bridge->config.min_energy_for_hunt) &&
                               !bridge->state.is_injured;

    mod->max_pursuit_duration_s = 5.0f * mod->endurance_modifier;
    mod->energy_conservation = 1.0f - mod->endurance_modifier;

    /* Recovery */
    mod->recovery_rate = health_factor * bridge->config.energy_recovery_rate;
    mod->rest_urgency = stress->fatigue_level + (1.0f - stress->energy_reserves) * 0.5f;
    mod->rest_urgency = nimcp_clampf(mod->rest_urgency, 0.0f, 1.0f);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_immune_bridge_t dragonfly_immune_bridge_create(
    const dragonfly_immune_config_t* config
) {
    dragonfly_immune_config_t cfg = config ? *config : dragonfly_immune_default_config();

    if (!dragonfly_immune_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_immune_bridge_create: invalid config");
        return NULL;
    }

    dragonfly_immune_bridge_t bridge = nimcp_calloc(1, sizeof(struct dragonfly_immune_bridge_s));
    NIMCP_API_CHECK_ALLOC(bridge, "dragonfly_immune_bridge_create: failed to allocate bridge");

    bridge->config = cfg;
    bridge->creation_time_us = get_time_us();

    /* Initialize state */
    bridge->state.health_status = HEALTH_OPTIMAL;
    bridge->state.stress_level = STRESS_NONE;
    bridge->state.stress_report.energy_reserves = 1.0f;
    bridge->state.modulation.hunting_recommended = true;
    bridge->state.modulation.speed_modifier = 1.0f;
    bridge->state.modulation.accuracy_modifier = 1.0f;
    bridge->state.modulation.endurance_modifier = 1.0f;
    bridge->state.modulation.reaction_modifier = 1.0f;
    bridge->state.modulation.max_pursuit_duration_s = 5.0f;

    if (bridge_base_init(&bridge->base, 0, "dragonfly_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "dragonfly_immune_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_immune_bridge_destroy(dragonfly_immune_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_immune");

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int dragonfly_immune_bridge_connect(
    dragonfly_immune_bridge_t bridge,
    dragonfly_system_t* dragonfly,
    bbb_system_t bbb
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_bridge_connect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = dragonfly;
    bridge->bbb = bbb;
    bridge->connected = (dragonfly != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_immune_bridge_disconnect(dragonfly_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_bridge_disconnect: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->dragonfly = NULL;
    bridge->bbb = NULL;
    bridge->connected = false;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_immune_bridge_update(
    dragonfly_immune_bridge_t bridge,
    float dt_s
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_bridge_update: bridge is NULL");
        return -1;
    }
    if (dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_immune_bridge_update: invalid dt_s");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t now = get_time_us();
    hunting_stress_t* stress = &bridge->state.stress_report;

    /* Natural stress decay */
    stress->frustration_level *= (1.0f - bridge->config.stress_decay_rate * dt_s);
    stress->frustration_level = nimcp_clampf(stress->frustration_level, 0.0f, 1.0f);

    /* Energy recovery when not hunting */
    stress->energy_reserves += bridge->config.energy_recovery_rate * dt_s;
    stress->energy_reserves = nimcp_clampf(stress->energy_reserves, 0.0f, 1.0f);

    /* Fatigue recovery */
    stress->fatigue_level *= (1.0f - 0.05f * dt_s);
    stress->fatigue_level = nimcp_clampf(stress->fatigue_level, 0.0f, 1.0f);

    /* Injury recovery */
    if (bridge->state.is_injured) {
        bridge->state.time_to_recovery_s -= dt_s;
        if (bridge->state.time_to_recovery_s <= 0.0f) {
            bridge->state.is_injured = false;
            bridge->state.injury_severity = 0.0f;
            bridge->state.time_to_recovery_s = 0.0f;
            bridge->stats.recovery_events++;
        }
    }

    /* Update stress level */
    float stress_value = stress->frustration_level * 0.4f +
                         stress->fatigue_level * 0.3f +
                         (1.0f - stress->energy_reserves) * 0.3f;
    bridge->state.stress_level = compute_stress_level(stress_value);

    /* Update health status (simple model based on energy and injury) */
    float health_level = stress->energy_reserves * (bridge->state.is_injured ? 0.5f : 1.0f);
    bridge->state.health_status = compute_health_status(bridge, health_level);

    /* Update modulation */
    update_modulation(bridge);

    bridge->last_update_us = now;
    bridge->stats.modulations_applied++;

    /* Track average health modifier */
    float avg_mod = (bridge->state.modulation.speed_modifier +
                     bridge->state.modulation.accuracy_modifier +
                     bridge->state.modulation.endurance_modifier +
                     bridge->state.modulation.reaction_modifier) / 4.0f;
    bridge->stats.avg_health_modifier =
        (bridge->stats.avg_health_modifier * (bridge->stats.modulations_applied - 1) + avg_mod) /
        bridge->stats.modulations_applied;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_immune_report_hunt(
    dragonfly_immune_bridge_t bridge,
    bool success,
    float duration_s,
    float energy_used
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_report_hunt: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    hunting_stress_t* stress = &bridge->state.stress_report;

    /* Update hunt statistics */
    stress->hunts_attempted++;
    if (success) {
        stress->hunts_successful++;
        stress->consecutive_failures = 0;

        /* Reduce stress on success */
        stress->frustration_level -= bridge->config.success_stress_decrement;
        stress->frustration_level = nimcp_clampf(stress->frustration_level, 0.0f, 1.0f);
    } else {
        stress->consecutive_failures++;

        /* Increase stress on failure */
        stress->frustration_level += bridge->config.failure_stress_increment;
        stress->frustration_level = nimcp_clampf(stress->frustration_level, 0.0f, 1.0f);

        bridge->stats.stress_events++;
    }

    /* Energy expenditure */
    stress->energy_reserves -= energy_used;
    stress->energy_reserves = nimcp_clampf(stress->energy_reserves, 0.0f, 1.0f);
    bridge->stats.total_energy_expended_j += energy_used;

    /* Fatigue from pursuit */
    stress->fatigue_level += duration_s * 0.05f;
    stress->fatigue_level = nimcp_clampf(stress->fatigue_level, 0.0f, 1.0f);

    /* Check for injury (probability increases with fatigue) */
    float injury_prob = bridge->config.injury_probability_base *
                        (1.0f + stress->fatigue_level * bridge->config.injury_fatigue_factor);
    if (!bridge->state.is_injured && nimcp_rand_uniform() < injury_prob) {
        bridge->state.is_injured = true;
        bridge->state.injury_severity = 0.3f + nimcp_rand_uniform() * 0.4f;
        bridge->state.time_to_recovery_s = bridge->config.injury_recovery_time_s *
                                           bridge->state.injury_severity;
        bridge->stats.injuries_sustained++;
    }

    /* Update stress indicators */
    stress->injury_risk = injury_prob;
    stress->cortisol_proxy = stress->frustration_level * 0.5f + stress->fatigue_level * 0.5f;
    stress->adrenaline_proxy = success ? 0.8f : 0.3f;

    /* Check if hunting should be blocked */
    if (!bridge->state.modulation.hunting_recommended) {
        bridge->stats.hunts_blocked++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_immune_report_stress(
    dragonfly_immune_bridge_t bridge,
    float pursuit_intensity,
    float duration_s
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_report_stress: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    hunting_stress_t* stress = &bridge->state.stress_report;

    /* Add stress from pursuit */
    float stress_add = pursuit_intensity * duration_s * 0.1f;
    stress->fatigue_level += stress_add;
    stress->fatigue_level = nimcp_clampf(stress->fatigue_level, 0.0f, 1.0f);

    /* Adrenaline surge */
    stress->adrenaline_proxy = nimcp_clampf(stress->adrenaline_proxy + pursuit_intensity * 0.2f, 0.0f, 1.0f);

    bridge->stats.stress_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_immune_report_rest(
    dragonfly_immune_bridge_t bridge,
    float duration_s
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_report_rest: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    hunting_stress_t* stress = &bridge->state.stress_report;

    /* Recovery during rest */
    float recovery = duration_s * bridge->config.energy_recovery_rate;

    stress->energy_reserves += recovery;
    stress->energy_reserves = nimcp_clampf(stress->energy_reserves, 0.0f, 1.0f);

    stress->fatigue_level *= (1.0f - duration_s * 0.1f);
    stress->fatigue_level = nimcp_clampf(stress->fatigue_level, 0.0f, 1.0f);

    stress->frustration_level *= (1.0f - duration_s * 0.05f);
    stress->frustration_level = nimcp_clampf(stress->frustration_level, 0.0f, 1.0f);

    /* Adrenaline decay */
    stress->adrenaline_proxy *= (1.0f - duration_s * 0.2f);
    stress->cortisol_proxy *= (1.0f - duration_s * 0.1f);

    bridge->stats.recovery_events++;

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_immune_get_modulation(
    const dragonfly_immune_bridge_t bridge,
    immune_modulation_t* modulation
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_modulation: bridge is NULL");
        return -1;
    }
    if (!modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_modulation: modulation is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *modulation = bridge->state.modulation;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool dragonfly_immune_hunting_safe(const dragonfly_immune_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_hunting_safe: bridge is NULL");
        return false;
    }
    return bridge->state.modulation.hunting_recommended;
}

health_status_t dragonfly_immune_get_health(const dragonfly_immune_bridge_t bridge) {
    if (!bridge) return HEALTH_CRITICAL;
    return bridge->state.health_status;
}

stress_level_t dragonfly_immune_get_stress(const dragonfly_immune_bridge_t bridge) {
    if (!bridge) return STRESS_CHRONIC;
    return bridge->state.stress_level;
}

int dragonfly_immune_get_state(
    const dragonfly_immune_bridge_t bridge,
    dragonfly_immune_state_t* state
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_state: bridge is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_state: state is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int dragonfly_immune_get_stats(
    const dragonfly_immune_bridge_t bridge,
    dragonfly_immune_stats_t* stats
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_stats: bridge is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_immune_get_stats: stats is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}
