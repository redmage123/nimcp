/**
 * @file nimcp_dragonfly_energy.c
 * @brief Energy-Optimal Pursuit Planning Implementation
 *
 * WHAT: Models metabolic costs and optimizes pursuit energy
 * WHY:  Enables biologically realistic pursuit decisions
 * HOW:  Energy budget tracking with pursuit optimization
 *
 * @author NIMCP Team
 * @date 2024-12-29
 */

#include "dragonfly/nimcp_dragonfly_energy.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_energy)

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

struct dragonfly_energy_s {
    /* Configuration */
    energy_config_t config;

    /* Current budget */
    energy_budget_t budget;

    /* Statistics */
    energy_stats_t stats;

    /* Activity tracking */
    activity_type_t current_activity;
    float activity_start_time_s;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Timing */
    uint64_t creation_time_us;
    uint64_t last_update_us;
};

//=============================================================================
// Name Functions
//=============================================================================

const char* dragonfly_energy_state_name(energy_state_t state) {
    switch (state) {
        case ENERGY_STATE_FULL:     return "full";
        case ENERGY_STATE_ADEQUATE: return "adequate";
        case ENERGY_STATE_LOW:      return "low";
        case ENERGY_STATE_CRITICAL: return "critical";
        case ENERGY_STATE_DEPLETED: return "depleted";
        default:                    return "unknown";
    }
}

const char* dragonfly_activity_name(activity_type_t activity) {
    switch (activity) {
        case ACTIVITY_REST:      return "rest";
        case ACTIVITY_HOVER:     return "hover";
        case ACTIVITY_PATROL:    return "patrol";
        case ACTIVITY_PURSUIT:   return "pursuit";
        case ACTIVITY_INTERCEPT: return "intercept";
        case ACTIVITY_EVASION:   return "evasion";
        default:                 return "unknown";
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

energy_config_t energy_default_config(void) {
    energy_config_t config = {
        /* Capacity */
        .max_energy_j = 1.0f,
        .reserve_fraction = 0.2f,

        /* Metabolic rates (Watts) */
        .rest_power_w = 0.01f,
        .hover_power_w = 0.05f,
        .patrol_power_w = 0.08f,
        .pursuit_power_w = 0.15f,
        .max_power_w = 0.25f,

        /* Acceleration cost */
        .accel_cost_j_per_ms2 = 0.001f,
        .turn_cost_j_per_rad = 0.002f,

        /* Prey values */
        .small_prey_value_j = 0.1f,
        .medium_prey_value_j = 0.3f,
        .large_prey_value_j = 0.5f,

        /* Decision thresholds */
        .min_roi_threshold = 1.0f,
        .abort_roi_threshold = 0.5f,
        .critical_reserve_j = 0.1f,

        /* Recovery */
        .recovery_rate_w = 0.02f
    };
    return config;
}

bool energy_validate_config(const energy_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "energy_validate_config: config is NULL");
        return false;
    }

    if (config->max_energy_j <= 0.0f) {
        return false;
    }
    if (config->reserve_fraction < 0.0f || config->reserve_fraction > 1.0f) {
        return false;
    }

    if (config->rest_power_w < 0.0f) {
        return false;
    }
    if (config->hover_power_w < config->rest_power_w) {
        return false;
    }
    if (config->patrol_power_w < config->hover_power_w) {
        return false;
    }
    if (config->pursuit_power_w < config->patrol_power_w) {
        return false;
    }
    if (config->max_power_w < config->pursuit_power_w) {
        return false;
    }

    if (config->small_prey_value_j < 0.0f) {
        return false;
    }
    if (config->medium_prey_value_j < config->small_prey_value_j) {
        return false;
    }
    if (config->large_prey_value_j < config->medium_prey_value_j) {
        return false;
    }

    if (config->min_roi_threshold < 0.0f) {
        return false;
    }
    if (config->recovery_rate_w < 0.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Internal Helpers
//=============================================================================

static float get_activity_power(const dragonfly_energy_t energy, activity_type_t activity) {
    switch (activity) {
        case ACTIVITY_REST:      return energy->config.rest_power_w;
        case ACTIVITY_HOVER:     return energy->config.hover_power_w;
        case ACTIVITY_PATROL:    return energy->config.patrol_power_w;
        case ACTIVITY_PURSUIT:   return energy->config.pursuit_power_w;
        case ACTIVITY_INTERCEPT: return energy->config.max_power_w;
        case ACTIVITY_EVASION:   return energy->config.max_power_w * 0.9f;
        default:                 return energy->config.rest_power_w;
    }
}

static energy_state_t compute_energy_state(const dragonfly_energy_t energy) {
    float fraction = energy->budget.current_energy_j / energy->config.max_energy_j;

    if (fraction >= 0.9f) return ENERGY_STATE_FULL;
    if (fraction >= 0.5f) return ENERGY_STATE_ADEQUATE;
    if (fraction >= energy->config.reserve_fraction) return ENERGY_STATE_LOW;
    if (fraction > 0.05f) return ENERGY_STATE_CRITICAL;
    return ENERGY_STATE_DEPLETED;
}

static float get_prey_value(const dragonfly_energy_t energy, float prey_size) {
    if (prey_size < 0.02f) return energy->config.small_prey_value_j;
    if (prey_size < 0.05f) return energy->config.medium_prey_value_j;
    return energy->config.large_prey_value_j;
}

static void update_time_estimates(dragonfly_energy_t energy) {
    float current_rate = energy->budget.current_rate_w;

    if (current_rate > 0.0f) {
        float to_critical = energy->budget.current_energy_j -
                            energy->config.critical_reserve_j;
        energy->budget.time_to_critical_s = to_critical / current_rate;

        energy->budget.time_to_depletion_s = energy->budget.current_energy_j / current_rate;
    } else {
        energy->budget.time_to_critical_s = INFINITY;
        energy->budget.time_to_depletion_s = INFINITY;
    }

    energy->budget.time_to_critical_s = nimcp_clampf(energy->budget.time_to_critical_s, 0.0f, 3600.0f);
    energy->budget.time_to_depletion_s = nimcp_clampf(energy->budget.time_to_depletion_s, 0.0f, 3600.0f);
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_energy_t dragonfly_energy_create(const energy_config_t* config) {
    energy_config_t cfg = config ? *config : energy_default_config();

    if (!energy_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_energy_create: invalid configuration");
        return NULL;
    }

    dragonfly_energy_t energy = nimcp_calloc(1, sizeof(struct dragonfly_energy_s));
    if (!energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_energy_create: failed to allocate energy");
        return NULL;
    }

    energy->config = cfg;
    energy->creation_time_us = get_time_us();

    /* Initialize with full energy */
    energy->budget.current_energy_j = cfg.max_energy_j;
    energy->budget.max_energy_j = cfg.max_energy_j;
    energy->budget.reserve_minimum_j = cfg.max_energy_j * cfg.reserve_fraction;
    energy->budget.resting_rate_w = cfg.rest_power_w;
    energy->budget.current_rate_w = cfg.rest_power_w;
    energy->budget.state = ENERGY_STATE_FULL;

    energy->current_activity = ACTIVITY_REST;

    update_time_estimates(energy);

    energy->mutex = nimcp_mutex_create(NULL);
    if (!energy->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_energy_create: failed to create mutex");
        nimcp_free(energy);
        return NULL;
    }

    return energy;
}

void dragonfly_energy_destroy(dragonfly_energy_t energy) {
    if (!energy) return;

    if (energy->mutex) {
        nimcp_mutex_destroy(energy->mutex);
    }

    nimcp_free(energy);
}

int dragonfly_energy_reset(dragonfly_energy_t energy) {
    if (!energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_reset: energy is NULL");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    energy->budget.current_energy_j = energy->config.max_energy_j;
    energy->budget.current_rate_w = energy->config.rest_power_w;
    energy->budget.state = ENERGY_STATE_FULL;
    energy->budget.energy_spent_j = 0.0f;
    energy->budget.energy_gained_j = 0.0f;
    energy->current_activity = ACTIVITY_REST;

    update_time_estimates(energy);

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int dragonfly_energy_update(
    dragonfly_energy_t energy,
    activity_type_t activity,
    float dt_s
) {
    if (!energy || dt_s <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_energy_update: energy is NULL");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    energy->current_activity = activity;
    float power = get_activity_power(energy, activity);
    energy->budget.current_rate_w = power;

    /* Energy expenditure */
    float expenditure = power * dt_s;
    energy->budget.current_energy_j -= expenditure;
    energy->budget.energy_spent_j += expenditure;
    energy->stats.total_energy_spent_j += expenditure;

    /* Recovery during rest */
    if (activity == ACTIVITY_REST) {
        float recovery = energy->config.recovery_rate_w * dt_s;
        energy->budget.current_energy_j += recovery;
    }

    /* Clamp energy */
    energy->budget.current_energy_j = nimcp_clampf(energy->budget.current_energy_j,
                                               0.0f, energy->config.max_energy_j);

    /* Update state */
    energy->budget.state = compute_energy_state(energy);
    update_time_estimates(energy);

    energy->last_update_us = get_time_us();

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

int dragonfly_energy_spend(
    dragonfly_energy_t energy,
    float amount_j,
    const char* reason
) {
    if (!energy || amount_j < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_energy_spend: energy is NULL");
        return -1;
    }
    (void)reason;  /* For logging */

    nimcp_mutex_lock(energy->mutex);

    energy->budget.current_energy_j -= amount_j;
    energy->budget.energy_spent_j += amount_j;
    energy->stats.total_energy_spent_j += amount_j;

    energy->budget.current_energy_j = nimcp_clampf(energy->budget.current_energy_j,
                                               0.0f, energy->config.max_energy_j);
    energy->budget.state = compute_energy_state(energy);
    update_time_estimates(energy);

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

int dragonfly_energy_gain(
    dragonfly_energy_t energy,
    float amount_j
) {
    if (!energy || amount_j < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_energy_gain: energy is NULL");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    energy->budget.current_energy_j += amount_j;
    energy->budget.energy_gained_j += amount_j;
    energy->stats.total_energy_gained_j += amount_j;

    energy->budget.current_energy_j = nimcp_clampf(energy->budget.current_energy_j,
                                               0.0f, energy->config.max_energy_j);
    energy->budget.state = compute_energy_state(energy);
    update_time_estimates(energy);

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

int dragonfly_energy_capture_prey(
    dragonfly_energy_t energy,
    float prey_size,
    float pursuit_energy_j
) {
    if (!energy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_capture_prey: energy is NULL");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    float prey_value = get_prey_value(energy, prey_size);
    float net_gain = prey_value - pursuit_energy_j;

    energy->budget.current_energy_j += net_gain;
    energy->budget.energy_gained_j += prey_value;
    energy->stats.total_energy_gained_j += prey_value;

    if (net_gain > 0) {
        energy->stats.economically_successful++;
    }

    /* Update ROI statistics */
    float roi = (pursuit_energy_j > 0) ? prey_value / pursuit_energy_j : 0.0f;
    energy->stats.avg_roi = (energy->stats.avg_roi * energy->stats.pursuits_attempted + roi) /
                            (energy->stats.pursuits_attempted + 1);
    energy->stats.pursuits_attempted++;

    energy->budget.current_energy_j = nimcp_clampf(energy->budget.current_energy_j,
                                               0.0f, energy->config.max_energy_j);
    energy->budget.state = compute_energy_state(energy);
    update_time_estimates(energy);

    /* Update efficiency */
    if (energy->stats.total_energy_spent_j > 0) {
        energy->stats.hunt_efficiency = energy->stats.total_energy_gained_j /
                                         energy->stats.total_energy_spent_j;
    }

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

//=============================================================================
// Estimation Functions
//=============================================================================

int dragonfly_energy_estimate_pursuit(
    const dragonfly_energy_t energy,
    const intercept_solution_t* solution,
    float prey_size,
    float success_probability,
    pursuit_energy_t* estimate
) {
    if (!energy || !solution || !estimate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_estimate_pursuit: required parameter is NULL (energy, solution, estimate)");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    /* Basic cost estimate */
    float duration = solution->intercept_time_s;
    float power = energy->config.pursuit_power_w;
    estimate->estimated_energy_j = power * duration;
    estimate->energy_per_second_w = power;
    estimate->pursuit_duration_s = duration;

    /* Outcome-based estimates */
    estimate->energy_if_success_j = estimate->estimated_energy_j * 0.8f;  /* Less if quick catch */
    estimate->energy_if_failure_j = estimate->estimated_energy_j * 1.2f;  /* More if chase extends */
    estimate->expected_energy_j = success_probability * estimate->energy_if_success_j +
                                  (1.0f - success_probability) * estimate->energy_if_failure_j;

    /* Value assessment */
    estimate->prey_energy_value_j = get_prey_value(energy, prey_size);
    estimate->net_energy_expected_j = success_probability * estimate->prey_energy_value_j -
                                       estimate->expected_energy_j;

    /* ROI and viability */
    if (estimate->expected_energy_j > 0) {
        estimate->roi = (success_probability * estimate->prey_energy_value_j) /
                        estimate->expected_energy_j;
    } else {
        estimate->roi = 0.0f;
    }

    estimate->economically_viable = (estimate->roi >= energy->config.min_roi_threshold);

    /* Budget check */
    float available = energy->budget.current_energy_j - energy->budget.reserve_minimum_j;
    estimate->within_budget = (estimate->estimated_energy_j < available);
    estimate->reserve_after_j = energy->budget.current_energy_j - estimate->estimated_energy_j;

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

int dragonfly_energy_optimize_pursuit(
    const dragonfly_energy_t energy,
    const interceptor_state_t* self,
    const target_state_t* target,
    float prey_size,
    energy_optimization_t* optimization
) {
    if (!energy || !self || !target || !optimization) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_optimize_pursuit: required parameter is NULL (energy, self, target, optimization)");
        return -1;
    }
    (void)self;  /* For future distance-based optimization */
    (void)target;

    nimcp_mutex_lock(energy->mutex);

    float prey_value = get_prey_value(energy, prey_size);

    /* Find optimal strategy based on ROI */
    optimization->best_strategy = INTERCEPT_PURSUIT;  /* Default */

    /* Optimal speed balances time cost vs energy cost */
    optimization->optimal_speed = 0.8f;  /* Fraction of max */
    optimization->optimal_pursuit_time_s = 3.0f;  /* Reasonable limit */

    /* Trade-off points */
    optimization->speed_vs_efficiency = 0.7f;
    optimization->success_vs_cost = 0.6f;

    /* Overall recommendation */
    float available = energy->budget.current_energy_j - energy->budget.reserve_minimum_j;
    float min_cost = energy->config.pursuit_power_w * 1.0f;  /* 1 second minimum */

    optimization->should_pursue = (available > min_cost) &&
                                  (prey_value > min_cost * energy->config.min_roi_threshold);

    if (!optimization->should_pursue) {
        if (available <= min_cost) {
            optimization->decision_reason = "insufficient_energy";
        } else {
            optimization->decision_reason = "low_roi";
        }
    } else {
        optimization->decision_reason = "viable";
    }

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

bool dragonfly_energy_can_afford(
    const dragonfly_energy_t energy,
    float estimated_cost_j
) {
    if (!energy) {
        return false;
    }

    float available = energy->budget.current_energy_j - energy->budget.reserve_minimum_j;
    return estimated_cost_j < available;
}

//=============================================================================
// Query Functions
//=============================================================================

int dragonfly_energy_get_budget(
    const dragonfly_energy_t energy,
    energy_budget_t* budget
) {
    if (!energy || !budget) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_get_budget: required parameter is NULL (energy, budget)");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);
    *budget = energy->budget;
    nimcp_mutex_unlock(energy->mutex);

    return 0;
}

energy_state_t dragonfly_energy_get_state(const dragonfly_energy_t energy) {
    if (!energy) return ENERGY_STATE_DEPLETED;
    return energy->budget.state;
}

float dragonfly_energy_get_level(const dragonfly_energy_t energy) {
    if (!energy) return 0.0f;
    return energy->budget.current_energy_j / energy->config.max_energy_j;
}

int dragonfly_energy_get_stats(
    const dragonfly_energy_t energy,
    energy_stats_t* stats
) {
    if (!energy || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_energy_get_stats: required parameter is NULL (energy, stats)");
        return -1;
    }

    nimcp_mutex_lock(energy->mutex);

    *stats = energy->stats;
    stats->net_energy_j = stats->total_energy_gained_j - stats->total_energy_spent_j;

    nimcp_mutex_unlock(energy->mutex);

    return 0;
}
