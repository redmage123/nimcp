/**
 * @file nimcp_neural_substrate.c
 * @brief Neural Substrate Module Implementation
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Low-level computational substrate modeling
 * WHY:  Physical/metabolic constraints affect neural computation
 * HOW:  Track energy, temperature, ions; compute modulation factors
 */

#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/imagination/nimcp_imagination_callbacks.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_substrate)

/* ============================================================================
 * Imagination Integration - Static Variables
 * ============================================================================ */

/**
 * @brief Bio-async context for substrate-imagination communication
 */
static bio_module_context_t g_substrate_imag_ctx = NULL;

/**
 * @brief Threshold for significant metabolic state change
 *
 * BIOLOGICAL: ATP changes > 5% are significant for imagination capacity
 */
#define SUBSTRATE_IMAG_CHANGE_THRESHOLD 0.05f

/* Forward declaration for imagination capacity computation */
static float compute_imagination_capacity_modifier(const neural_substrate_t* substrate);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute Q10 temperature effect
 *
 * WHAT: Calculate temperature scaling factor
 * WHY:  Biological processes have Q10 temperature dependence
 * HOW:  Q10 formula: factor = Q10^((T - T_ref) / 10)
 */
static float compute_q10_factor(float temperature, float q10, float t_ref) {
    float delta = (temperature - t_ref) / 10.0f;
    return powf(q10, delta);
}

/**
 * @brief Update metabolic capacity
 */
static void update_metabolic_capacity(substrate_metabolic_state_t* metabolic) {
    if (!metabolic) return;

    /* Weighted average of metabolic components */
    metabolic->metabolic_capacity =
        metabolic->atp_level * 0.5f +
        metabolic->oxygen_saturation * 0.3f +
        metabolic->glucose_level * 0.2f;
}

/**
 * @brief Update physical capacity
 */
static void update_physical_capacity(substrate_physical_state_t* physical) {
    if (!physical) return;

    /* Temperature factor (optimal at 37°C) */
    float temp_factor = 1.0f;
    if (physical->temperature < SUBSTRATE_HYPOTHERMIA_THRESHOLD) {
        temp_factor = 0.5f + (physical->temperature - 28.0f) /
            (SUBSTRATE_HYPOTHERMIA_THRESHOLD - 28.0f) * 0.5f;
    } else if (physical->temperature > SUBSTRATE_HYPERTHERMIA_THRESHOLD) {
        temp_factor = 1.0f - (physical->temperature - SUBSTRATE_HYPERTHERMIA_THRESHOLD) / 5.0f;
    }
    temp_factor = nimcp_clampf(temp_factor, 0.1f, 1.0f);

    /* Weighted average including temperature */
    physical->physical_capacity =
        physical->membrane_integrity * 0.35f +
        physical->ion_balance * 0.35f +
        temp_factor * 0.3f;
}

/**
 * @brief Compute modulation factors from state
 */
static void compute_modulation(neural_substrate_t* substrate) {
    if (!substrate) return;

    float metabolic = substrate->metabolic.metabolic_capacity;
    float physical = substrate->physical.physical_capacity;
    float temp = substrate->physical.temperature;

    /* Firing rate modulation */
    float temp_q10_firing = compute_q10_factor(temp, SUBSTRATE_Q10_FIRING,
                                                SUBSTRATE_NORMAL_TEMPERATURE);
    substrate->modulation.firing_rate_mod =
        metabolic * physical * nimcp_clampf(temp_q10_firing, 0.5f, 1.5f);

    /* Transmission efficiency */
    float temp_q10_trans = compute_q10_factor(temp, SUBSTRATE_Q10_TRANSMISSION,
                                               SUBSTRATE_NORMAL_TEMPERATURE);
    substrate->modulation.transmission_efficiency =
        metabolic * substrate->physical.membrane_integrity *
        nimcp_clampf(temp_q10_trans, 0.5f, 1.2f);

    /* Conduction velocity */
    float temp_q10_cond = compute_q10_factor(temp, SUBSTRATE_Q10_CONDUCTION,
                                              SUBSTRATE_NORMAL_TEMPERATURE);
    substrate->modulation.conduction_velocity =
        physical * nimcp_clampf(temp_q10_cond, 0.6f, 1.4f);

    /* Plasticity capacity */
    float temp_q10_plast = compute_q10_factor(temp, SUBSTRATE_Q10_PLASTICITY,
                                               SUBSTRATE_NORMAL_TEMPERATURE);
    substrate->modulation.plasticity_capacity =
        metabolic * physical * nimcp_clampf(temp_q10_plast, 0.5f, 1.3f);

    /* Overall capacity */
    substrate->modulation.overall_capacity = (metabolic + physical) / 2.0f;

    /* Clamp all values */
    substrate->modulation.firing_rate_mod =
        nimcp_clampf(substrate->modulation.firing_rate_mod, 0.0f, 1.5f);
    substrate->modulation.transmission_efficiency =
        nimcp_clampf(substrate->modulation.transmission_efficiency, 0.0f, 1.0f);
    substrate->modulation.conduction_velocity =
        nimcp_clampf(substrate->modulation.conduction_velocity, 0.5f, 1.5f);
    substrate->modulation.plasticity_capacity =
        nimcp_clampf(substrate->modulation.plasticity_capacity, 0.0f, 1.0f);
    substrate->modulation.overall_capacity =
        nimcp_clampf(substrate->modulation.overall_capacity, 0.0f, 1.0f);
}

/**
 * @brief Update health level from state
 */
static void update_health_level(neural_substrate_t* substrate) {
    if (!substrate) return;

    float overall = substrate->modulation.overall_capacity;

    if (overall >= 0.9f) {
        substrate->health_level = SUBSTRATE_HEALTH_OPTIMAL;
    } else if (overall >= 0.7f) {
        substrate->health_level = SUBSTRATE_HEALTH_STRESSED;
    } else if (overall >= 0.5f) {
        substrate->health_level = SUBSTRATE_HEALTH_COMPROMISED;
    } else if (overall >= 0.3f) {
        substrate->health_level = SUBSTRATE_HEALTH_CRITICAL;
    } else {
        substrate->health_level = SUBSTRATE_HEALTH_FAILING;
    }
}

/**
 * @brief Check and generate alerts
 */
static void check_alerts(neural_substrate_t* substrate) {
    if (!substrate || !substrate->config.enable_alerts) return;

    substrate->alert_count = 0;

    if (substrate->metabolic.atp_level < SUBSTRATE_CRITICAL_ATP) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_LOW_ATP;
    }
    if (substrate->metabolic.oxygen_saturation < SUBSTRATE_CRITICAL_O2) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_HYPOXIA;
    }
    if (substrate->metabolic.glucose_level < SUBSTRATE_CRITICAL_GLUCOSE) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_HYPOGLYCEMIA;
    }
    if (substrate->physical.temperature > SUBSTRATE_HYPERTHERMIA_THRESHOLD) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_HYPERTHERMIA;
    }
    if (substrate->physical.temperature < SUBSTRATE_HYPOTHERMIA_THRESHOLD) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_HYPOTHERMIA;
    }
    if (substrate->physical.ion_balance < SUBSTRATE_CRITICAL_ION_IMBALANCE) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_ION_IMBALANCE;
    }
    if (substrate->physical.membrane_integrity < SUBSTRATE_CRITICAL_MEMBRANE) {
        substrate->active_alerts[substrate->alert_count++] = SUBSTRATE_ALERT_MEMBRANE_DAMAGE;
    }

    if (substrate->alert_count > 0) {
        substrate->stats.alerts_generated += substrate->alert_count;
    }
}

/* ============================================================================
 * Lifecycle Implementation
 * ============================================================================ */

int substrate_default_config(substrate_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_default_config: config is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* Initial values - normal healthy state */
    config->initial_atp = SUBSTRATE_NORMAL_ATP;
    config->initial_o2 = SUBSTRATE_NORMAL_O2_SAT;
    config->initial_glucose = SUBSTRATE_NORMAL_GLUCOSE;
    config->initial_temperature = SUBSTRATE_NORMAL_TEMPERATURE;
    config->initial_membrane = SUBSTRATE_NORMAL_MEMBRANE;
    config->initial_ion_balance = SUBSTRATE_NORMAL_ION_BALANCE;

    /* Recovery parameters */
    config->atp_recovery_rate = SUBSTRATE_ATP_RECOVERY_RATE;
    config->ion_recovery_rate = SUBSTRATE_ION_RECOVERY_RATE;
    config->membrane_repair_rate = SUBSTRATE_MEMBRANE_REPAIR_RATE;

    /* Energy costs */
    config->cost_per_spike = SUBSTRATE_COST_PER_SPIKE;
    config->cost_per_transmission = SUBSTRATE_COST_PER_TRANSMISSION;
    config->baseline_cost = SUBSTRATE_COST_BASELINE;

    /* All features enabled */
    config->enable_metabolic_model = true;
    config->enable_temperature_effects = true;
    config->enable_ion_dynamics = true;
    config->enable_alerts = true;

    return 0;
}

neural_substrate_t* substrate_create(const substrate_config_t* config) {
    neural_substrate_t* substrate = (neural_substrate_t*)
        nimcp_calloc(1, sizeof(neural_substrate_t));
    if (!substrate) {
        NIMCP_LOGGING_ERROR("Substrate allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_create: substrate allocation failed");
        return NULL;
    }

    /* Apply configuration */
    substrate_config_t default_cfg;
    if (!config) {
        substrate_default_config(&default_cfg);
        config = &default_cfg;
    }
    substrate->config = *config;

    /* Initialize metabolic state */
    substrate->metabolic.atp_level = config->initial_atp;
    substrate->metabolic.oxygen_saturation = config->initial_o2;
    substrate->metabolic.glucose_level = config->initial_glucose;
    substrate->metabolic.metabolic_rate = config->baseline_cost;
    substrate->metabolic.recovery_rate = config->atp_recovery_rate;
    update_metabolic_capacity(&substrate->metabolic);

    /* Initialize physical state */
    substrate->physical.temperature = config->initial_temperature;
    substrate->physical.membrane_integrity = config->initial_membrane;
    substrate->physical.ion_balance = config->initial_ion_balance;
    substrate->physical.na_k_pump_activity = 0.95f;
    substrate->physical.ca_homeostasis = 0.95f;
    update_physical_capacity(&substrate->physical);

    /* Create mutex */
    substrate->mutex = nimcp_platform_mutex_create();
    if (!substrate->mutex) {
        NIMCP_LOGGING_ERROR("Mutex allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "substrate_create: mutex allocation failed");
        substrate_destroy(substrate);
        return NULL;
    }

    /* Compute initial modulation */
    compute_modulation(substrate);
    update_health_level(substrate);

    NIMCP_LOGGING_INFO("Neural substrate created");
    return substrate;
}

void substrate_destroy(neural_substrate_t* substrate) {
    if (!substrate) return;

    if (substrate->mutex) {
        nimcp_platform_mutex_destroy(substrate->mutex);
        nimcp_free(substrate->mutex);
        substrate->mutex = NULL;
    }

    nimcp_free(substrate);
    NIMCP_LOGGING_INFO("Neural substrate destroyed");
}

int substrate_reset(neural_substrate_t* substrate) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_reset: substrate is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(substrate->mutex);

    /* Reset to initial values */
    substrate->metabolic.atp_level = substrate->config.initial_atp;
    substrate->metabolic.oxygen_saturation = substrate->config.initial_o2;
    substrate->metabolic.glucose_level = substrate->config.initial_glucose;
    substrate->metabolic.metabolic_rate = substrate->config.baseline_cost;
    update_metabolic_capacity(&substrate->metabolic);

    substrate->physical.temperature = substrate->config.initial_temperature;
    substrate->physical.membrane_integrity = substrate->config.initial_membrane;
    substrate->physical.ion_balance = substrate->config.initial_ion_balance;
    substrate->physical.na_k_pump_activity = 0.95f;
    substrate->physical.ca_homeostasis = 0.95f;
    update_physical_capacity(&substrate->physical);

    substrate->alert_count = 0;
    memset(&substrate->stats, 0, sizeof(substrate_stats_t));

    compute_modulation(substrate);
    update_health_level(substrate);

    nimcp_platform_mutex_unlock(substrate->mutex);
    return 0;
}

/* ============================================================================
 * Update Implementation
 * ============================================================================ */

int substrate_update(neural_substrate_t* substrate, uint64_t delta_ms) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_update: substrate is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_platform_mutex_lock(substrate->mutex);

    float delta_sec = delta_ms / 1000.0f;

    /* Metabolic recovery */
    if (substrate->config.enable_metabolic_model) {
        /* Baseline consumption */
        substrate->metabolic.atp_level -=
            substrate->config.baseline_cost * delta_sec;

        /* ATP recovery (depends on O2 and glucose) */
        float recovery = substrate->config.atp_recovery_rate * delta_sec;
        recovery *= substrate->metabolic.oxygen_saturation;
        recovery *= substrate->metabolic.glucose_level;
        substrate->metabolic.atp_level += recovery;

        /* Clamp */
        substrate->metabolic.atp_level =
            nimcp_clampf(substrate->metabolic.atp_level, 0.0f, 1.0f);

        /* Slow glucose/O2 recovery toward normal */
        substrate->metabolic.glucose_level +=
            (SUBSTRATE_NORMAL_GLUCOSE - substrate->metabolic.glucose_level) * 0.001f * delta_sec;
        substrate->metabolic.oxygen_saturation +=
            (SUBSTRATE_NORMAL_O2_SAT - substrate->metabolic.oxygen_saturation) * 0.005f * delta_sec;

        update_metabolic_capacity(&substrate->metabolic);
    }

    /* Ion dynamics */
    if (substrate->config.enable_ion_dynamics) {
        /* Ion recovery */
        float ion_recovery = substrate->config.ion_recovery_rate * delta_sec;
        ion_recovery *= substrate->metabolic.atp_level;  /* Na/K pump needs ATP */
        substrate->physical.ion_balance +=
            (SUBSTRATE_NORMAL_ION_BALANCE - substrate->physical.ion_balance) * ion_recovery;
        substrate->physical.ion_balance =
            nimcp_clampf(substrate->physical.ion_balance, 0.0f, 1.0f);

        /* Update pump activity based on ATP */
        substrate->physical.na_k_pump_activity = substrate->metabolic.atp_level * 0.95f;
    }

    /* Membrane repair */
    float membrane_repair = substrate->config.membrane_repair_rate * delta_sec;
    substrate->physical.membrane_integrity +=
        (SUBSTRATE_NORMAL_MEMBRANE - substrate->physical.membrane_integrity) * membrane_repair;
    substrate->physical.membrane_integrity =
        nimcp_clampf(substrate->physical.membrane_integrity, 0.0f, 1.0f);

    /* Temperature drift toward normal */
    if (substrate->config.enable_temperature_effects) {
        substrate->physical.temperature +=
            (SUBSTRATE_NORMAL_TEMPERATURE - substrate->physical.temperature) * 0.001f * delta_sec;
    }

    update_physical_capacity(&substrate->physical);

    /* Compute modulation and health */
    compute_modulation(substrate);
    update_health_level(substrate);
    check_alerts(substrate);

    /* Track critical events */
    if (substrate->health_level >= SUBSTRATE_HEALTH_CRITICAL) {
        substrate->stats.critical_events++;
    }

    substrate->stats.total_updates++;
    substrate->last_update_ms += delta_ms;

    /* Update average health */
    float health_score = substrate->modulation.overall_capacity;
    substrate->stats.avg_health_score =
        (substrate->stats.avg_health_score * (substrate->stats.total_updates - 1) + health_score) /
        substrate->stats.total_updates;

    /*
     * Imagination capacity notification
     *
     * BIOLOGICAL: Imagination is metabolically expensive. When ATP or overall
     * capacity changes significantly (>5%), notify the imagination engine so
     * it can adjust scenario complexity, vividness targets, and step counts.
     *
     * We track the previous capacity and only send updates on significant change
     * to avoid flooding the message system.
     */
    static _Thread_local float s_prev_imagination_capacity = 1.0f;
    float current_capacity = compute_imagination_capacity_modifier(substrate);
    float delta_capacity = current_capacity - s_prev_imagination_capacity;

    if (delta_capacity < 0) delta_capacity = -delta_capacity;  /* fabsf without math.h float fn */

    if (delta_capacity >= SUBSTRATE_IMAG_CHANGE_THRESHOLD) {
        s_prev_imagination_capacity = current_capacity;
        /* Send update outside of lock to avoid potential deadlock */
        nimcp_platform_mutex_unlock(substrate->mutex);
        neural_substrate_send_imagination_capacity(substrate);
        return 0;
    }

    nimcp_platform_mutex_unlock(substrate->mutex);
    return 0;
}

int substrate_record_spikes(neural_substrate_t* substrate, uint32_t neuron_count) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_record_spikes: substrate is NULL");
        return -1;
    }
    if (!substrate->config.enable_metabolic_model) return -1;

    nimcp_platform_mutex_lock(substrate->mutex);

    float cost = substrate->config.cost_per_spike * neuron_count;
    substrate->metabolic.atp_level -= cost;
    substrate->metabolic.atp_level = nimcp_clampf(substrate->metabolic.atp_level, 0.0f, 1.0f);

    substrate->stats.spikes_processed += neuron_count;
    substrate->stats.total_atp_consumed += cost;

    if (substrate->metabolic.metabolic_rate < cost) {
        substrate->metabolic.metabolic_rate = cost;
    }
    if (cost > substrate->stats.peak_metabolic_rate) {
        substrate->stats.peak_metabolic_rate = cost;
    }

    update_metabolic_capacity(&substrate->metabolic);
    compute_modulation(substrate);

    nimcp_platform_mutex_unlock(substrate->mutex);
    return 0;
}

int substrate_record_transmissions(neural_substrate_t* substrate, uint32_t transmission_count) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_record_transmissions: substrate is NULL");
        return -1;
    }
    if (!substrate->config.enable_metabolic_model) return -1;

    nimcp_platform_mutex_lock(substrate->mutex);

    float cost = substrate->config.cost_per_transmission * transmission_count;
    substrate->metabolic.atp_level -= cost;
    substrate->metabolic.atp_level = nimcp_clampf(substrate->metabolic.atp_level, 0.0f, 1.0f);

    substrate->stats.transmissions_processed += transmission_count;
    substrate->stats.total_atp_consumed += cost;

    update_metabolic_capacity(&substrate->metabolic);
    compute_modulation(substrate);

    nimcp_platform_mutex_unlock(substrate->mutex);
    return 0;
}

/* ============================================================================
 * Setter Implementation
 * ============================================================================ */

int substrate_set_atp(neural_substrate_t* substrate, float atp_level) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_atp: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->metabolic.atp_level = nimcp_clampf(atp_level, 0.0f, 1.0f);
    update_metabolic_capacity(&substrate->metabolic);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

int substrate_set_oxygen(neural_substrate_t* substrate, float o2_sat) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_oxygen: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->metabolic.oxygen_saturation = nimcp_clampf(o2_sat, 0.0f, 1.0f);
    update_metabolic_capacity(&substrate->metabolic);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

int substrate_set_glucose(neural_substrate_t* substrate, float glucose) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_glucose: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->metabolic.glucose_level = nimcp_clampf(glucose, 0.0f, 1.0f);
    update_metabolic_capacity(&substrate->metabolic);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

int substrate_set_temperature(neural_substrate_t* substrate, float temperature) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_temperature: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->physical.temperature = nimcp_clampf(temperature, 20.0f, 45.0f);
    update_physical_capacity(&substrate->physical);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

int substrate_set_membrane_integrity(neural_substrate_t* substrate, float integrity) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_membrane_integrity: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->physical.membrane_integrity = nimcp_clampf(integrity, 0.0f, 1.0f);
    update_physical_capacity(&substrate->physical);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

int substrate_set_ion_balance(neural_substrate_t* substrate, float balance) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_set_ion_balance: substrate is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(substrate->mutex);
    substrate->physical.ion_balance = nimcp_clampf(balance, 0.0f, 1.0f);
    update_physical_capacity(&substrate->physical);
    compute_modulation(substrate);
    nimcp_platform_mutex_unlock(substrate->mutex);

    return 0;
}

/* ============================================================================
 * Query Implementation
 * ============================================================================ */

int substrate_get_metabolic_state(
    const neural_substrate_t* substrate,
    substrate_metabolic_state_t* state
) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_metabolic_state: substrate is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_metabolic_state: state is NULL");
        return -1;
    }
    *state = substrate->metabolic;
    return 0;
}

int substrate_get_physical_state(
    const neural_substrate_t* substrate,
    substrate_physical_state_t* state
) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_physical_state: substrate is NULL");
        return -1;
    }
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_physical_state: state is NULL");
        return -1;
    }
    *state = substrate->physical;
    return 0;
}

int substrate_get_modulation(
    const neural_substrate_t* substrate,
    substrate_modulation_t* mod
) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_modulation: substrate is NULL");
        return -1;
    }
    if (!mod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_modulation: mod is NULL");
        return -1;
    }
    *mod = substrate->modulation;
    return 0;
}

substrate_health_level_t substrate_get_health_level(const neural_substrate_t* substrate) {
    if (!substrate) return SUBSTRATE_HEALTH_FAILING;
    return substrate->health_level;
}

float substrate_get_capacity(const neural_substrate_t* substrate) {
    if (!substrate) return 0.0f;
    return substrate->modulation.overall_capacity;
}

float substrate_get_firing_modulation(const neural_substrate_t* substrate) {
    if (!substrate) return 1.0f;
    return substrate->modulation.firing_rate_mod;
}

float substrate_get_transmission_efficiency(const neural_substrate_t* substrate) {
    if (!substrate) return 1.0f;
    return substrate->modulation.transmission_efficiency;
}

int substrate_get_alerts(
    const neural_substrate_t* substrate,
    substrate_alert_type_t* alerts,
    uint32_t* count
) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_alerts: substrate is NULL");
        return -1;
    }
    if (!alerts) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_alerts: alerts is NULL");
        return -1;
    }
    if (!count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_alerts: count is NULL");
        return -1;
    }

    *count = substrate->alert_count;
    for (uint32_t i = 0; i < substrate->alert_count && i < 8; i++) {
        alerts[i] = substrate->active_alerts[i];
    }

    return 0;
}

int substrate_get_stats(
    const neural_substrate_t* substrate,
    substrate_stats_t* stats
) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_stats: substrate is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "substrate_get_stats: stats is NULL");
        return -1;
    }
    *stats = substrate->stats;
    return 0;
}

/* ============================================================================
 * String Conversion Implementation
 * ============================================================================ */

const char* substrate_health_level_to_string(substrate_health_level_t level) {
    switch (level) {
        case SUBSTRATE_HEALTH_OPTIMAL:     return "OPTIMAL";
        case SUBSTRATE_HEALTH_STRESSED:    return "STRESSED";
        case SUBSTRATE_HEALTH_COMPROMISED: return "COMPROMISED";
        case SUBSTRATE_HEALTH_CRITICAL:    return "CRITICAL";
        case SUBSTRATE_HEALTH_FAILING:     return "FAILING";
        default:                           return "UNKNOWN";
    }
}

const char* substrate_alert_type_to_string(substrate_alert_type_t alert) {
    switch (alert) {
        case SUBSTRATE_ALERT_NONE:           return "NONE";
        case SUBSTRATE_ALERT_LOW_ATP:        return "LOW_ATP";
        case SUBSTRATE_ALERT_HYPOXIA:        return "HYPOXIA";
        case SUBSTRATE_ALERT_HYPOGLYCEMIA:   return "HYPOGLYCEMIA";
        case SUBSTRATE_ALERT_HYPERTHERMIA:   return "HYPERTHERMIA";
        case SUBSTRATE_ALERT_HYPOTHERMIA:    return "HYPOTHERMIA";
        case SUBSTRATE_ALERT_ION_IMBALANCE:  return "ION_IMBALANCE";
        case SUBSTRATE_ALERT_MEMBRANE_DAMAGE:return "MEMBRANE_DAMAGE";
        default:                             return "UNKNOWN";
    }
}

/* ============================================================================
 * Imagination Engine Integration
 *
 * BIOLOGICAL BASIS:
 * Imagination is metabolically expensive, requiring sustained prefrontal and
 * default mode network activity. ATP depletion and fatigue reduce imaginative
 * capacity - a phenomenon observed in mental fatigue studies where creative
 * thinking and mental simulation abilities decline with energy depletion.
 *
 * References:
 * - Smallwood & Schooler (2015) "The Science of Mind Wandering"
 * - Andrews-Hanna et al. (2014) "The Default Network and Self-Generated Thought"
 * ============================================================================ */

/**
 * @brief Compute imagination capacity modifier from substrate state
 *
 * WHAT: Calculate how much substrate state limits imagination
 * WHY:  Imagination requires ATP for DMN/PFC activity; fatigue impairs it
 * HOW:  capacity_mod = atp_level * (1 - fatigue_level * 0.5)
 *
 * BIOLOGICAL: Low ATP reduces neural firing in DMN; fatigue accumulates
 * metabolites that impair cognitive flexibility and creative thought.
 *
 * @param substrate Neural substrate to query
 * @return Capacity modifier [0.0-1.0], where 1.0 = optimal imagination capacity
 */
static float compute_imagination_capacity_modifier(const neural_substrate_t* substrate) {
    if (!substrate) return 1.0f;

    /* Get ATP level - primary driver of imagination capacity */
    float atp = substrate->metabolic.atp_level;

    /*
     * Fatigue level approximation:
     * - Low metabolic capacity indicates accumulated fatigue
     * - Physical stress (membrane/ion issues) adds to fatigue
     * - Scale: 0 = rested, 1 = exhausted
     */
    float metabolic_fatigue = 1.0f - substrate->metabolic.metabolic_capacity;
    float physical_fatigue = 1.0f - substrate->physical.physical_capacity;
    float fatigue = (metabolic_fatigue * 0.7f + physical_fatigue * 0.3f);

    /*
     * Capacity formula:
     * - Base capacity from ATP (imagination needs energy)
     * - Fatigue reduces capacity by up to 50% (even with ATP available,
     *   accumulated metabolites impair prefrontal function)
     * - Minimum 10% capacity (consciousness persists even when exhausted)
     */
    float capacity_mod = atp * (1.0f - fatigue * 0.5f);

    return nimcp_clampf(capacity_mod, 0.1f, 1.0f);
}

int neural_substrate_send_imagination_capacity(neural_substrate_t* substrate) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_substrate_send_imagination_capacity: substrate is NULL");
        return -1;
    }

    /* Check if bio-async is connected */
    if (!g_substrate_imag_ctx) {
        /* Not connected - silently succeed (imagination may not be present) */
        return 0;
    }

    /* Compute capacity modifier */
    float capacity_mod = compute_imagination_capacity_modifier(substrate);

    /* Compute fatigue level for the message */
    float metabolic_fatigue = 1.0f - substrate->metabolic.metabolic_capacity;
    float physical_fatigue = 1.0f - substrate->physical.physical_capacity;
    float fatigue = (metabolic_fatigue * 0.7f + physical_fatigue * 0.3f);

    /* Build modulation message */
    bio_msg_imagination_modulation_t msg;
    memset(&msg, 0, sizeof(msg));

    bio_msg_init_header(
        &msg.header,
        BIO_MSG_IMAGINATION_CAPACITY_UPDATE,
        BIO_MODULE_SUBSTRATE_IMAGINATION,
        BIO_MODULE_IMAGINATION,
        sizeof(msg) - sizeof(bio_message_header_t)
    );

    msg.modulation_type = 1;  /* 1 = capacity modulation (from substrate) */
    msg.modifier = capacity_mod;
    msg.source_level = substrate->metabolic.atp_level;
    msg.secondary_level = fatigue;

    /* Send to imagination engine */
    nimcp_error_t err = bio_router_send(g_substrate_imag_ctx, &msg, sizeof(msg), 0);
    if (err != NIMCP_SUCCESS) {
        NIMCP_LOGGING_DEBUG("Failed to send imagination capacity update: %d", err);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neural_substrate_send_imagination_capacity: bio_router_send failed");
        return -1;
    }

    NIMCP_LOGGING_DEBUG("Sent imagination capacity update: modifier=%.3f, atp=%.3f, fatigue=%.3f",
                        capacity_mod, substrate->metabolic.atp_level, fatigue);

    return 0;
}

/**
 * @brief Handler for imagination capacity request messages
 *
 * WHAT: Respond to imagination engine requests for current capacity
 * WHY:  Imagination may query substrate before initiating expensive scenarios
 * HOW:  Compute and return current capacity modifier
 */
static nimcp_error_t handle_imagination_capacity_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    (void)msg;
    (void)msg_size;
    (void)response_promise;

    neural_substrate_t* substrate = (neural_substrate_t*)user_data;
    if (!substrate) return NIMCP_ERROR_NULL_POINTER;

    /* Send current capacity state */
    int result = neural_substrate_send_imagination_capacity(substrate);
    return (result == 0) ? NIMCP_SUCCESS : NIMCP_ERROR_OPERATION_FAILED;
}

/**
 * @brief KG-driven wiring handler callback for Neural Substrate Imagination
 *
 * WHAT: Register message handlers based on KG-discovered message types
 * WHY:  Enable dynamic wiring driven by knowledge graph
 * HOW:  Iterate discovered message types and register appropriate handlers
 *
 * @param ctx Bio-async module context
 * @param message_types Array of message types to handle (from KG)
 * @param message_count Number of message types
 * @param user_data Neural substrate pointer
 * @return 0 on success, -1 on error
 */
static int neural_substrate_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    if (!ctx || !message_types || message_count == 0) {
        return 0;  /* No handlers to register */
    }

    NIMCP_LOGGING_INFO("neural_substrate_wiring_handler_callback: registering %u handlers from KG",
                       message_count);

    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_IMAGINATION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_imagination_capacity_request);
                NIMCP_LOGGING_DEBUG("  Registered handler for BIO_MSG_IMAGINATION_REQUEST");
                break;

            default:
                NIMCP_LOGGING_DEBUG("  Unknown message type %u - skipping", message_types[i]);
                break;
        }
    }

    return 0;
}

int neural_substrate_register_imagination_handler(neural_substrate_t* substrate) {
    if (!substrate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "neural_substrate_register_imagination_handler: substrate is NULL");
        return -1;
    }

    /* Check if already registered */
    if (g_substrate_imag_ctx) {
        return 0;  /* Already connected */
    }

    /* Register with bio-async router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_SUBSTRATE_IMAGINATION,
        .module_name = "neural_substrate_imagination",
        .inbox_capacity = 16,
        .user_data = substrate
    };

    g_substrate_imag_ctx = bio_router_register_module(&info);
    if (!g_substrate_imag_ctx) {
        NIMCP_LOGGING_WARN("Bio-async router not available for imagination integration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "neural_substrate_register_imagination_handler: bio_router_register_module failed");
        return -1;
    }

    /* KG-Driven Wiring: Register callback for orchestrator to invoke
     * When orchestrator starts, it discovers HANDLES_MESSAGE relations
     * from the KG and invokes this callback with the message types */
    nimcp_error_t cb_result = bio_router_register_wiring_callback(
        BIO_MODULE_SUBSTRATE_IMAGINATION,
        (void*)neural_substrate_wiring_handler_callback,
        substrate
    );

    if (cb_result == NIMCP_SUCCESS) {
        NIMCP_LOGGING_INFO("Neural substrate bio-async registered with KG-driven wiring (module_id=0x%04X)",
                          BIO_MODULE_SUBSTRATE_IMAGINATION);
    } else {
        /* Fallback: Direct registration if orchestrator not available */
        LEGACY_HANDLER_REGISTRATION(
            nimcp_error_t err = bio_router_register_handler(
                g_substrate_imag_ctx,
                BIO_MSG_IMAGINATION_REQUEST,
                handle_imagination_capacity_request
            )
        );

        if (err != NIMCP_SUCCESS) {
            NIMCP_LOGGING_WARN("Failed to register imagination request handler: %d", err);
            /* Continue anyway - we can still send updates */
        }

        NIMCP_LOGGING_INFO("Neural substrate bio-async registered with legacy handlers (module_id=0x%04X)",
                          BIO_MODULE_SUBSTRATE_IMAGINATION);
    }

    /* Send initial capacity state */
    neural_substrate_send_imagination_capacity(substrate);

    return 0;
}

int neural_substrate_unregister_imagination_handler(void) {
    if (!g_substrate_imag_ctx) {
        return 0;  /* Not connected */
    }

    bio_router_unregister_module(g_substrate_imag_ctx);
    g_substrate_imag_ctx = NULL;

    NIMCP_LOGGING_INFO("Unregistered neural substrate imagination handler");

    return 0;
}
