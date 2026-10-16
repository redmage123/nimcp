/**
 * @file nimcp_metabolic_plasticity.c
 * @brief Energy/Metabolic Constraints for Synaptic Plasticity Implementation
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "constants/nimcp_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(metabolic_plasticity)

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct metabolic_plasticity {
    /* Configuration */
    metabolic_config_t config;

    /* ATP pool state */
    atp_pool_state_t atp_state;

    /* Statistics */
    metabolic_stats_t stats;

    /* Running averages */
    float atp_sum;               /**< For computing average */
    uint64_t atp_sample_count;   /**< Number of samples */

    /* Thread safety */
    nimcp_platform_mutex_t* mutex;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Update energy state classification
 *
 * WHAT: Reclassify energy state based on current ATP
 * WHY:  Track state transitions for callbacks
 * HOW:  Compare ATP to thresholds, invoke callback on change
 */
static void update_energy_state(metabolic_plasticity_t* metabolic) {
    if (!metabolic) return;

    float atp_pct = metabolic->atp_state.current_atp / METABOLIC_ATP_FULL_CAPACITY;
    energy_state_t old_state = metabolic->atp_state.state;
    energy_state_t new_state = metabolic_classify_energy_state(metabolic->atp_state.current_atp);

    metabolic->atp_state.state = new_state;
    metabolic->atp_state.depletion_severity = 1.0f - atp_pct;

    /* Update gating flags */
    metabolic->atp_state.ltp_permitted =
        (metabolic->atp_state.current_atp >= metabolic->config.costs.ltp_threshold);
    metabolic->atp_state.ltd_permitted =
        (metabolic->atp_state.current_atp >= metabolic->config.costs.ltd_threshold);

    /* Invoke callback if state changed */
    if (new_state != old_state && metabolic->config.state_callback) {
        metabolic->config.state_callback(
            old_state,
            new_state,
            metabolic->atp_state.current_atp,
            metabolic->config.callback_user_data
        );
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int metabolic_plasticity_default_config(metabolic_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_default_config: config is NULL");
        return -1;
    }

    /* Initial state */
    config->initial_atp = METABOLIC_ATP_INITIAL;

    /* Energy costs */
    config->costs.ltp_cost = METABOLIC_COST_LTP_BASE;
    config->costs.ltd_cost = METABOLIC_COST_LTD_BASE;
    config->costs.spine_growth_cost = METABOLIC_COST_SPINE_GROWTH;
    config->costs.protein_synth_cost = METABOLIC_COST_PROTEIN_SYNTH;

    /* Thresholds */
    config->costs.ltp_threshold = METABOLIC_LTP_THRESHOLD;
    config->costs.ltd_threshold = METABOLIC_LTD_THRESHOLD;

    /* Recovery parameters */
    config->costs.base_recovery_rate = METABOLIC_RECOVERY_RATE_BASE;
    config->costs.glycolysis_rate = METABOLIC_RECOVERY_RATE_GLYCOLYSIS;
    config->costs.astrocyte_rate = METABOLIC_RECOVERY_RATE_ASTROCYTE;

    /* Feature enables */
    config->enable_ltp_gating = true;
    config->enable_ltd_gating = true;
    config->enable_dynamic_recovery = true;
    config->enable_activity_scaling = true;

    /* Sensitivity */
    config->cost_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->recovery_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Callbacks */
    config->state_callback = NULL;
    config->callback_user_data = NULL;

    return 0;
}

metabolic_plasticity_t* metabolic_plasticity_create(const metabolic_config_t* config) {
    /* Allocate structure */
    metabolic_plasticity_t* metabolic = (metabolic_plasticity_t*)
        nimcp_malloc(sizeof(metabolic_plasticity_t));
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_plasticity_create: failed to allocate structure");
        NIMCP_LOGGING_ERROR("Failed to allocate metabolic plasticity");
        return NULL;
    }

    /* Initialize to zero */
    memset(metabolic, 0, sizeof(metabolic_plasticity_t));

    /* Apply configuration */
    if (config) {
        memcpy(&metabolic->config, config, sizeof(metabolic_config_t));
    } else {
        metabolic_plasticity_default_config(&metabolic->config);
    }

    /* Initialize ATP pool */
    metabolic->atp_state.current_atp = metabolic->config.initial_atp;
    metabolic->atp_state.max_capacity = METABOLIC_ATP_FULL_CAPACITY;
    metabolic->atp_state.recovery_rate =
        metabolic->config.costs.base_recovery_rate +
        metabolic->config.costs.glycolysis_rate +
        metabolic->config.costs.astrocyte_rate;

    /* Initialize statistics */
    metabolic->stats.min_atp_reached = METABOLIC_ATP_FULL_CAPACITY;

    /* Create mutex */
    metabolic->mutex = nimcp_platform_mutex_create();
    if (!metabolic->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "metabolic_plasticity_create: failed to create mutex");
        nimcp_free(metabolic);
        NIMCP_LOGGING_ERROR("Failed to create metabolic mutex");
        return NULL;
    }

    /* Update initial state */
    update_energy_state(metabolic);

    NIMCP_LOGGING_INFO("Metabolic plasticity created (ATP=%.1f)", metabolic->atp_state.current_atp);
    return metabolic;
}

void metabolic_plasticity_destroy(metabolic_plasticity_t* metabolic) {
    if (!metabolic) return;

    /* Destroy mutex */
    if (metabolic->mutex) {
        nimcp_platform_mutex_destroy(metabolic->mutex);
        nimcp_free(metabolic->mutex);
        metabolic->mutex = NULL;
    }

    /* Free structure */
    nimcp_free(metabolic);
    NIMCP_LOGGING_INFO("Metabolic plasticity destroyed");
}

/* ============================================================================
 * Energy Accounting Implementation
 * ============================================================================ */

bool metabolic_plasticity_can_ltp(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) {
        return false;
    }
    if (!metabolic->config.enable_ltp_gating) return true;
    return metabolic->atp_state.ltp_permitted;
}

bool metabolic_plasticity_can_ltd(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) {
        return false;
    }
    if (!metabolic->config.enable_ltd_gating) return true;
    return metabolic->atp_state.ltd_permitted;
}

int metabolic_plasticity_consume_atp(
    metabolic_plasticity_t* metabolic,
    metabolic_event_type_t event_type,
    float magnitude
) {
    /* Guard clauses */
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_consume_atp: metabolic is NULL");
        return -1;
    }
    if (magnitude < 0.0f || magnitude > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_plasticity_consume_atp: magnitude out of range [0, 1]");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);

    /* Get base cost for event type */
    float base_cost = 0.0f;
    switch (event_type) {
        case METABOLIC_EVENT_LTP:
            base_cost = metabolic->config.costs.ltp_cost;
            metabolic->stats.total_ltp_events++;
            /* Check if blocked */
            if (metabolic->config.enable_ltp_gating && !metabolic->atp_state.ltp_permitted) {
                metabolic->stats.ltp_blocked_count++;
                nimcp_platform_mutex_unlock(metabolic->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_consume_atp: metabolic->atp_state is NULL");
                return -1;
            }
            break;

        case METABOLIC_EVENT_LTD:
            base_cost = metabolic->config.costs.ltd_cost;
            metabolic->stats.total_ltd_events++;
            /* Check if blocked */
            if (metabolic->config.enable_ltd_gating && !metabolic->atp_state.ltd_permitted) {
                metabolic->stats.ltd_blocked_count++;
                nimcp_platform_mutex_unlock(metabolic->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_consume_atp: metabolic->atp_state is NULL");
                return -1;
            }
            break;

        case METABOLIC_EVENT_SPINE_GROWTH:
            base_cost = metabolic->config.costs.spine_growth_cost;
            break;

        case METABOLIC_EVENT_PROTEIN_SYNTH:
            base_cost = metabolic->config.costs.protein_synth_cost;
            break;

        default:
            nimcp_platform_mutex_unlock(metabolic->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_plasticity_consume_atp: operation failed");
            return -1;
    }

    /* Scale cost by magnitude and sensitivity */
    float total_cost = base_cost * magnitude * metabolic->config.cost_sensitivity;

    /* Consume ATP */
    metabolic->atp_state.current_atp -= total_cost;
    metabolic->atp_state.current_atp = nimcp_clampf(
        metabolic->atp_state.current_atp,
        METABOLIC_ATP_MIN,
        METABOLIC_ATP_FULL_CAPACITY
    );

    /* Update statistics */
    metabolic->stats.total_atp_consumed += total_cost;
    if (metabolic->atp_state.current_atp < metabolic->stats.min_atp_reached) {
        metabolic->stats.min_atp_reached = metabolic->atp_state.current_atp;
    }

    /* Update energy state */
    update_energy_state(metabolic);

    nimcp_platform_mutex_unlock(metabolic->mutex);
    return 0;
}

float metabolic_plasticity_get_atp_level(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) return 0.0f;
    return metabolic->atp_state.current_atp;
}

energy_state_t metabolic_plasticity_get_energy_state(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) return ENERGY_STATE_EMERGENCY;
    return metabolic->atp_state.state;
}

int metabolic_plasticity_get_atp_state(
    const metabolic_plasticity_t* metabolic,
    atp_pool_state_t* state
) {
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_get_atp_state: metabolic is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_get_atp_state: state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);
    memcpy(state, &metabolic->atp_state, sizeof(atp_pool_state_t));
    nimcp_platform_mutex_unlock(metabolic->mutex);

    return 0;
}

/* ============================================================================
 * Recovery Implementation
 * ============================================================================ */

int metabolic_plasticity_update(metabolic_plasticity_t* metabolic, uint64_t delta_ms) {
    /* Guard clauses */
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_update: metabolic is NULL");
        return -1;
    }
    if (delta_ms == 0) return 0;

    nimcp_platform_mutex_lock(metabolic->mutex);

    /* Calculate recovery amount */
    float delta_sec = delta_ms / 1000.0f;
    float recovery_amount = metabolic->atp_state.recovery_rate * delta_sec *
                           metabolic->config.recovery_sensitivity;

    /* Recover ATP */
    float old_atp = metabolic->atp_state.current_atp;
    metabolic->atp_state.current_atp += recovery_amount;
    metabolic->atp_state.current_atp = nimcp_clampf(
        metabolic->atp_state.current_atp,
        METABOLIC_ATP_MIN,
        METABOLIC_ATP_FULL_CAPACITY
    );

    float actual_recovery = metabolic->atp_state.current_atp - old_atp;

    /* Update statistics */
    metabolic->stats.total_atp_recovered += actual_recovery;
    metabolic->stats.total_updates++;

    /* Update running average */
    metabolic->atp_sum += metabolic->atp_state.current_atp;
    metabolic->atp_sample_count++;
    metabolic->stats.avg_atp_level = metabolic->atp_sum / metabolic->atp_sample_count;

    /* Update state duration tracking */
    switch (metabolic->atp_state.state) {
        case ENERGY_STATE_HEALTHY:
            metabolic->stats.time_in_healthy_ms += delta_ms;
            break;
        case ENERGY_STATE_DEPLETED:
            metabolic->stats.time_in_depleted_ms += delta_ms;
            break;
        case ENERGY_STATE_CRITICAL:
        case ENERGY_STATE_EMERGENCY:
            metabolic->stats.time_in_critical_ms += delta_ms;
            break;
    }

    /* Update energy state */
    update_energy_state(metabolic);

    nimcp_platform_mutex_unlock(metabolic->mutex);
    return 0;
}

int metabolic_plasticity_set_recovery_rate(
    metabolic_plasticity_t* metabolic,
    float recovery_rate
) {
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_set_recovery_rate: metabolic is NULL");
        return -1;
    }
    if (recovery_rate < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "metabolic_plasticity_set_recovery_rate: recovery_rate must be non-negative");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);
    metabolic->atp_state.recovery_rate = recovery_rate;
    nimcp_platform_mutex_unlock(metabolic->mutex);

    return 0;
}

float metabolic_plasticity_get_recovery_rate(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) return 0.0f;
    return metabolic->atp_state.recovery_rate;
}

int metabolic_plasticity_restore_atp(metabolic_plasticity_t* metabolic, float atp_level) {
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_restore_atp: metabolic is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);

    metabolic->atp_state.current_atp = nimcp_clampf(
        atp_level,
        METABOLIC_ATP_MIN,
        METABOLIC_ATP_FULL_CAPACITY
    );

    /* Update min ATP tracking */
    if (metabolic->atp_state.current_atp < metabolic->stats.min_atp_reached) {
        metabolic->stats.min_atp_reached = metabolic->atp_state.current_atp;
    }

    update_energy_state(metabolic);

    nimcp_platform_mutex_unlock(metabolic->mutex);
    return 0;
}

/* ============================================================================
 * Modulation Implementation
 * ============================================================================ */

float metabolic_plasticity_get_effective_lr(
    const metabolic_plasticity_t* metabolic,
    float base_lr
) {
    if (!metabolic) return base_lr;

    /* Scale LR by ATP percentage (graceful degradation) */
    float atp_pct = metabolic->atp_state.current_atp / METABOLIC_ATP_FULL_CAPACITY;
    return base_lr * atp_pct;
}

float metabolic_plasticity_get_magnitude_scale(
    const metabolic_plasticity_t* metabolic,
    metabolic_event_type_t event_type
) {
    if (!metabolic) return 1.0f;

    /* Scale magnitude by ATP availability */
    float atp_pct = metabolic->atp_state.current_atp / METABOLIC_ATP_FULL_CAPACITY;

    /* Different events have different sensitivity to depletion */
    switch (event_type) {
        case METABOLIC_EVENT_LTP:
            /* LTP very sensitive to energy depletion */
            if (atp_pct < 0.5f) return 0.0f;
            return (atp_pct - 0.5f) / 0.5f;  /* Linear from 0.5 to 1.0 */

        case METABOLIC_EVENT_LTD:
            /* LTD less sensitive */
            if (atp_pct < 0.3f) return 0.0f;
            return (atp_pct - 0.3f) / 0.7f;  /* Linear from 0.3 to 1.0 */

        default:
            return atp_pct;
    }
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

int metabolic_plasticity_get_stats(
    const metabolic_plasticity_t* metabolic,
    metabolic_stats_t* stats
) {
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_get_stats: metabolic is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_get_stats: stats is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);
    memcpy(stats, &metabolic->stats, sizeof(metabolic_stats_t));
    nimcp_platform_mutex_unlock(metabolic->mutex);

    return 0;
}

int metabolic_plasticity_reset_stats(metabolic_plasticity_t* metabolic) {
    if (!metabolic) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "metabolic_plasticity_reset_stats: metabolic is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(metabolic->mutex);

    memset(&metabolic->stats, 0, sizeof(metabolic_stats_t));
    metabolic->stats.min_atp_reached = METABOLIC_ATP_FULL_CAPACITY;
    metabolic->atp_sum = 0.0f;
    metabolic->atp_sample_count = 0;

    nimcp_platform_mutex_unlock(metabolic->mutex);
    return 0;
}

float metabolic_plasticity_get_ltp_block_rate(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) return 0.0f;
    if (metabolic->stats.total_ltp_events == 0) return 0.0f;

    return (float)metabolic->stats.ltp_blocked_count / (float)metabolic->stats.total_ltp_events;
}

float metabolic_plasticity_get_avg_atp(const metabolic_plasticity_t* metabolic) {
    if (!metabolic) return 0.0f;
    return metabolic->stats.avg_atp_level;
}

/* ============================================================================
 * Helper Function Implementation
 * ============================================================================ */

energy_state_t metabolic_classify_energy_state(float atp_level) {
    float atp_pct = atp_level / METABOLIC_ATP_FULL_CAPACITY;

    if (atp_pct >= METABOLIC_ENERGY_HEALTHY) {
        return ENERGY_STATE_HEALTHY;
    } else if (atp_pct >= METABOLIC_ENERGY_DEPLETED) {
        return ENERGY_STATE_DEPLETED;
    } else if (atp_pct >= METABOLIC_ENERGY_CRITICAL) {
        return ENERGY_STATE_CRITICAL;
    } else {
        return ENERGY_STATE_EMERGENCY;
    }
}

float metabolic_get_event_cost(metabolic_event_type_t event_type) {
    switch (event_type) {
        case METABOLIC_EVENT_LTP:           return METABOLIC_COST_LTP_BASE;
        case METABOLIC_EVENT_LTD:           return METABOLIC_COST_LTD_BASE;
        case METABOLIC_EVENT_SPINE_GROWTH:  return METABOLIC_COST_SPINE_GROWTH;
        case METABOLIC_EVENT_PROTEIN_SYNTH: return METABOLIC_COST_PROTEIN_SYNTH;
        default:                             return 0.0f;
    }
}

const char* metabolic_energy_state_name(energy_state_t state) {
    switch (state) {
        case ENERGY_STATE_HEALTHY:   return "HEALTHY";
        case ENERGY_STATE_DEPLETED:  return "DEPLETED";
        case ENERGY_STATE_CRITICAL:  return "CRITICAL";
        case ENERGY_STATE_EMERGENCY: return "EMERGENCY";
        default:                     return "UNKNOWN";
    }
}

const char* metabolic_event_type_name(metabolic_event_type_t event_type) {
    switch (event_type) {
        case METABOLIC_EVENT_LTP:           return "LTP";
        case METABOLIC_EVENT_LTD:           return "LTD";
        case METABOLIC_EVENT_SPINE_GROWTH:  return "SPINE_GROWTH";
        case METABOLIC_EVENT_PROTEIN_SYNTH: return "PROTEIN_SYNTH";
        default:                             return "UNKNOWN";
    }
}
