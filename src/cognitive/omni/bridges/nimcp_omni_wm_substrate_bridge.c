/**
 * @file nimcp_omni_wm_substrate_bridge.c
 * @brief World Model Substrate Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-17
 *
 * WHAT: Bidirectional bridge connecting World Model (RSSM) with Neural Substrate
 * WHY:  Enable energy-aware world modeling with metabolic constraints
 * HOW:  Substrate state modulates prediction depth; WM signals computational demand
 *
 * IMPLEMENTATION NOTES:
 * =====================
 * This implementation integrates several key concepts:
 *
 * 1. METABOLIC CONSTRAINTS ON PREDICTION:
 *    - ATP depletion reduces prediction horizon
 *    - O2 saturation affects computation rate
 *    - Glucose availability gates training
 *
 * 2. Q10 TEMPERATURE EFFECTS:
 *    - All rates scale with temperature via Q10 coefficient
 *    - Hyperthermia triggers protective slowdown
 *
 * 3. BIDIRECTIONAL FEEDBACK:
 *    - Substrate informs WM of available resources
 *    - WM reports consumption back to substrate
 */

#include "cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================================
 * Module-level Constants
 * ============================================================================ */

#define LOG_MODULE "wm_substrate_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_wm_substrate_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_wm_substrate_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_wm_substrate_bridge_mesh_registry = NULL;

nimcp_error_t omni_wm_substrate_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_wm_substrate_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_wm_substrate_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_wm_substrate_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_wm_substrate_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_wm_substrate_bridge_mesh_registry = registry;
    return err;
}

void omni_wm_substrate_bridge_mesh_unregister(void) {
    if (g_omni_wm_substrate_bridge_mesh_registry && g_omni_wm_substrate_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_wm_substrate_bridge_mesh_registry, g_omni_wm_substrate_bridge_mesh_id);
        g_omni_wm_substrate_bridge_mesh_id = 0;
        g_omni_wm_substrate_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_wm_substrate_bridge module (instance-level) */
static inline void omni_wm_substrate_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_wm_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_wm_substrate_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_wm_substrate_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

void omni_wm_substrate_bridge_set_instance_health_agent(
    omni_wm_substrate_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_wm_substrate_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_wm_substrate_bridge_training_begin(omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_substrate_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_wm_substrate_bridge_heartbeat_instance(bridge->health_agent, "training_begin", 0.0f);
    return 0;
}

int omni_wm_substrate_bridge_training_step(omni_wm_substrate_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_substrate_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_wm_substrate_bridge_heartbeat_instance(bridge->health_agent, "training_step", progress);
    return 0;
}

int omni_wm_substrate_bridge_training_end(omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_wm_substrate_bridge_training_end: NULL argument");
        return -1;
    }
    omni_wm_substrate_bridge_heartbeat_instance(bridge->health_agent, "training_end", 1.0f);
    return 0;
}

/** Alert bit flags */
#define ALERT_BIT_LOW_ATP       (1 << 0)
#define ALERT_BIT_HYPOXIA       (1 << 1)
#define ALERT_BIT_LOW_GLUCOSE   (1 << 2)
#define ALERT_BIT_HYPERTHERMIA  (1 << 3)

/* ============================================================================
 * Internal Helper Forward Declarations
 * ============================================================================ */

static nimcp_error_t update_metabolic_availability(omni_wm_substrate_bridge_t* bridge);
static nimcp_error_t compute_substrate_to_wm_effects(omni_wm_substrate_bridge_t* bridge);
static nimcp_error_t update_wm_to_substrate_effects(omni_wm_substrate_bridge_t* bridge, float dt);
static nimcp_error_t check_and_update_alerts(omni_wm_substrate_bridge_t* bridge);
static nimcp_error_t apply_constraints_to_wm(omni_wm_substrate_bridge_t* bridge);
static nimcp_error_t log_metabolic_state(omni_wm_substrate_bridge_t* bridge);
static uint64_t get_current_time_us(void);

/* Bio-async handlers */
static nimcp_error_t handle_metabolic_update(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_demand_signal(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data);
static nimcp_error_t handle_constraint_response(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* user_data);

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_current_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

/**
 * @brief Update metabolic availability from substrate
 *
 * WHAT: Read current metabolic state from neural substrate
 * WHY:  Need current resource levels to compute constraints
 * HOW:  Query substrate API, compute derived metrics
 */
static nimcp_error_t update_metabolic_availability(omni_wm_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    wm_metabolic_availability_t* avail = &bridge->availability;

    /* Get state from substrate if connected */
    if (bridge->substrate) {
        /* TODO: Query actual metabolic state via substrate_get_metabolic_state().
         * Currently using placeholder values — real substrate API integration pending.
         * These hardcoded values make the metabolic pipeline a no-op; tracked as HIGH bug. */
        static bool warned = false;
        if (!warned) {
            NIMCP_LOGGING_WARN("omni_wm_substrate_bridge: Using placeholder metabolic values "
                               "(atp=0.9, o2=0.95, glucose=0.85) — substrate API not yet integrated");
            warned = true;
        }
        avail->atp_level = 0.9f;          /* Default healthy values */
        avail->oxygen_saturation = 0.95f;
        avail->glucose_level = 0.85f;
        avail->temperature = WM_SUBSTRATE_NORMAL_TEMP;
        avail->metabolic_rate = 1.0f;
        avail->recovery_rate = 1.0f;
        avail->health_level = 0;  /* OPTIMAL */
    } else {
        /* No substrate - assume healthy defaults */
        avail->atp_level = 1.0f;
        avail->oxygen_saturation = 1.0f;
        avail->glucose_level = 1.0f;
        avail->temperature = WM_SUBSTRATE_NORMAL_TEMP;
        avail->metabolic_rate = 1.0f;
        avail->recovery_rate = 1.0f;
        avail->health_level = 0;
    }

    /* Compute derived metrics */

    /* Metabolic capacity: geometric mean of primary resources */
    avail->metabolic_capacity = powf(
        avail->atp_level * avail->oxygen_saturation * avail->glucose_level,
        1.0f / 3.0f
    );

    /* Computational capacity: primarily ATP and O2 dependent */
    avail->computational_capacity = nimcp_clampf(
        avail->atp_level * 0.6f + avail->oxygen_saturation * 0.4f,
        0.0f, 1.0f
    );

    /* Learning capacity: requires all three resources */
    avail->learning_capacity = nimcp_clampf(
        avail->metabolic_capacity *
        (avail->glucose_level > bridge->config.glucose_learning_threshold ? 1.0f : 0.5f),
        0.0f, 1.0f
    );

    /* Stress detection */
    avail->is_stressed = (avail->atp_level < bridge->config.atp_training_threshold) ||
                         (avail->oxygen_saturation < bridge->config.o2_critical_threshold) ||
                         (avail->glucose_level < bridge->config.glucose_learning_threshold);

    avail->is_critical = (avail->atp_level < bridge->config.atp_critical_threshold) ||
                         (avail->oxygen_saturation < bridge->config.o2_critical_threshold * 0.8f);

    /* Update min observations for stats */
    if (avail->atp_level < bridge->stats.min_atp_observed ||
        bridge->stats.min_atp_observed == 0.0f) {
        bridge->stats.min_atp_observed = avail->atp_level;
    }
    if (avail->oxygen_saturation < bridge->stats.min_o2_observed ||
        bridge->stats.min_o2_observed == 0.0f) {
        bridge->stats.min_o2_observed = avail->oxygen_saturation;
    }
    if (avail->glucose_level < bridge->stats.min_glucose_observed ||
        bridge->stats.min_glucose_observed == 0.0f) {
        bridge->stats.min_glucose_observed = avail->glucose_level;
    }
    if (avail->temperature > bridge->stats.max_temp_observed) {
        bridge->stats.max_temp_observed = avail->temperature;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Compute effects from substrate to world model
 *
 * WHAT: Calculate metabolic constraints on WM operation
 * WHY:  WM needs to know its operational limits
 * HOW:  Apply thresholds and scaling to availability
 */
static nimcp_error_t compute_substrate_to_wm_effects(omni_wm_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    substrate_to_wm_effects_t* effects = &bridge->substrate_effects;
    const wm_metabolic_availability_t* avail = &bridge->availability;
    const omni_wm_substrate_bridge_config_t* cfg = &bridge->config;

    /* Compute Q10 temperature factor */
    if (cfg->enable_temperature_effects) {
        effects->q10_factor = omni_wm_substrate_compute_q10_factor(
            avail->temperature,
            cfg->normal_temperature,
            cfg->q10_coefficient
        );
        effects->temperature_mod = effects->q10_factor;

        /* Check for hyperthermia */
        if (avail->temperature > cfg->hyperthermia_threshold) {
            effects->hyperthermia_alert = true;
            /* Protective slowdown */
            effects->temperature_mod *= 0.5f;
        } else {
            effects->hyperthermia_alert = false;
        }
    } else {
        effects->q10_factor = 1.0f;
        effects->temperature_mod = 1.0f;
        effects->hyperthermia_alert = false;
    }

    /* Compute horizon scaling */
    if (cfg->adaptive_horizon) {
        /* Scale horizon by ATP availability */
        effects->horizon_scale = nimcp_clampf(
            avail->atp_level / cfg->atp_training_threshold,
            0.0f, 1.0f
        );

        /* Compute allowed horizon */
        float horizon_range = (float)(cfg->max_horizon - cfg->min_horizon);
        effects->allowed_horizon = cfg->min_horizon +
            (uint32_t)(horizon_range * effects->horizon_scale);

        /* Track if horizon was reduced */
        if (effects->allowed_horizon < bridge->current_horizon) {
            bridge->stats.horizon_reductions++;
        }
    } else {
        effects->horizon_scale = 1.0f;
        effects->allowed_horizon = cfg->max_horizon;
    }

    /* Compute computation rate scaling */
    if (cfg->adaptive_rate) {
        /* Rate depends on O2 and temperature */
        float o2_scale = 1.0f;
        if (cfg->enable_o2_modulation) {
            o2_scale = nimcp_clampf(
                avail->oxygen_saturation / cfg->o2_critical_threshold,
                0.0f, 1.0f
            );
            o2_scale = powf(o2_scale, cfg->o2_compute_scale);
        }

        effects->compute_rate_scale = o2_scale * effects->temperature_mod;

        /* Track if rate was reduced */
        float new_rate = cfg->base_compute_rate * effects->compute_rate_scale;
        if (new_rate < bridge->current_compute_rate * 0.9f) {
            bridge->stats.rate_reductions++;
        }
    } else {
        effects->compute_rate_scale = effects->temperature_mod;
    }

    /* Compute training constraints */
    if (cfg->enable_atp_modulation) {
        effects->training_permitted = (avail->atp_level >= cfg->atp_training_threshold);
        effects->low_atp_alert = (avail->atp_level < cfg->atp_critical_threshold);

        /* Track training pause/resume */
        if (!effects->training_permitted && bridge->training_was_active) {
            bridge->stats.training_pauses++;
        } else if (effects->training_permitted && !bridge->training_was_active &&
                   bridge->stats.training_pauses > 0) {
            bridge->stats.training_resumes++;
        }
    } else {
        effects->training_permitted = true;
        effects->low_atp_alert = false;
    }

    /* Compute learning rate scaling */
    if (cfg->enable_glucose_modulation) {
        effects->learning_rate_scale = nimcp_clampf(
            avail->glucose_level / cfg->glucose_learning_threshold,
            0.0f, 1.0f
        );
        effects->low_glucose_alert = (avail->glucose_level < cfg->glucose_learning_threshold);
    } else {
        effects->learning_rate_scale = 1.0f;
        effects->low_glucose_alert = false;
    }

    /* Compute O2 effects */
    if (cfg->enable_o2_modulation) {
        effects->hypoxia_alert = (avail->oxygen_saturation < cfg->o2_critical_threshold);
    } else {
        effects->hypoxia_alert = false;
    }

    /* Dreaming constraints: require healthy metabolic state */
    effects->dreaming_permitted = !avail->is_stressed && effects->training_permitted;
    effects->dream_length_scale = effects->horizon_scale * effects->learning_rate_scale;

    /* Energy-efficient mode detection */
    effects->use_simplified_dynamics = avail->is_stressed && !avail->is_critical;
    effects->use_cached_predictions = avail->is_critical;

    if (effects->use_simplified_dynamics) {
        bridge->stats.simplified_dynamics_activations++;
    }

    /* Update interval scaling: slow down updates under stress */
    effects->update_interval_scale = 1.0f / (effects->compute_rate_scale + 0.01f);
    effects->update_interval_scale = nimcp_clampf(effects->update_interval_scale, 1.0f, 10.0f);

    /* Prediction confidence modulation: lower confidence under stress */
    effects->prediction_confidence_mod = avail->metabolic_capacity;

    return NIMCP_SUCCESS;
}

/**
 * @brief Update effects from WM to substrate
 *
 * WHAT: Track WM's consumption of metabolic resources
 * WHY:  Substrate needs to know resource usage
 * HOW:  Accumulate consumption based on activity reports
 */
static nimcp_error_t update_wm_to_substrate_effects(omni_wm_substrate_bridge_t* bridge, float dt) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    wm_to_substrate_effects_t* effects = &bridge->wm_effects;
    const omni_wm_substrate_bridge_config_t* cfg = &bridge->config;

    /* Update consumption rates based on demand */
    wm_computational_demand_t* demand = &effects->demand;

    /* Estimate ATP consumption rate */
    effects->atp_consumption_rate =
        demand->prediction_demand * cfg->atp_per_prediction +
        demand->training_demand * cfg->atp_per_training_step +
        demand->rollout_demand * cfg->atp_per_rollout_step;

    /* O2 consumption proportional to ATP (oxidative phosphorylation) */
    effects->o2_consumption_rate = effects->atp_consumption_rate * 0.8f;

    /* Glucose consumption for training (protein synthesis) */
    effects->glucose_consumption_rate =
        demand->training_demand * cfg->atp_per_training_step * 0.5f;

    /* Accumulate totals over dt */
    effects->total_atp_consumed += effects->atp_consumption_rate * dt;
    effects->total_o2_consumed += effects->o2_consumption_rate * dt;
    effects->total_glucose_consumed += effects->glucose_consumption_rate * dt;

    /* Update stats */
    bridge->stats.total_atp_consumed = effects->total_atp_consumed;
    bridge->stats.total_o2_consumed = effects->total_o2_consumed;
    bridge->stats.total_glucose_consumed = effects->total_glucose_consumed;

    if (effects->atp_consumption_rate > bridge->stats.peak_consumption_rate) {
        bridge->stats.peak_consumption_rate = effects->atp_consumption_rate;
    }

    /* Compute efficiency metrics */
    if (effects->total_atp_consumed > 0.0f) {
        effects->energy_efficiency = (float)effects->predictions_made /
                                      effects->total_atp_consumed;
    }

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.mean_energy_efficiency =
        alpha * effects->energy_efficiency +
        (1.0f - alpha) * bridge->stats.mean_energy_efficiency;

    return NIMCP_SUCCESS;
}

/**
 * @brief Check and update alert states
 *
 * WHAT: Monitor for metabolic alert conditions
 * WHY:  Need to track alert transitions for stats and messaging
 * HOW:  Compare current alerts to previous, update counters
 */
static nimcp_error_t check_and_update_alerts(omni_wm_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    substrate_to_wm_effects_t* effects = &bridge->substrate_effects;
    uint32_t prev_alerts = bridge->active_alerts;
    uint32_t new_alerts = 0;

    /* Build alert bitmask */
    if (effects->low_atp_alert) {
        new_alerts |= ALERT_BIT_LOW_ATP;
        if (!(prev_alerts & ALERT_BIT_LOW_ATP)) {
            bridge->stats.low_atp_alerts++;
            NIMCP_LOGGING_WARN("WM Substrate: Low ATP alert triggered");
        }
    }

    if (effects->hypoxia_alert) {
        new_alerts |= ALERT_BIT_HYPOXIA;
        if (!(prev_alerts & ALERT_BIT_HYPOXIA)) {
            bridge->stats.hypoxia_alerts++;
            NIMCP_LOGGING_WARN("WM Substrate: Hypoxia alert triggered");
        }
    }

    if (effects->low_glucose_alert) {
        new_alerts |= ALERT_BIT_LOW_GLUCOSE;
        if (!(prev_alerts & ALERT_BIT_LOW_GLUCOSE)) {
            bridge->stats.low_glucose_alerts++;
            NIMCP_LOGGING_WARN("WM Substrate: Low glucose alert triggered");
        }
    }

    if (effects->hyperthermia_alert) {
        new_alerts |= ALERT_BIT_HYPERTHERMIA;
        if (!(prev_alerts & ALERT_BIT_HYPERTHERMIA)) {
            bridge->stats.hyperthermia_alerts++;
            NIMCP_LOGGING_WARN("WM Substrate: Hyperthermia alert triggered");
        }
    }

    /* Count resolved alerts */
    uint32_t resolved = prev_alerts & ~new_alerts;
    if (resolved) {
        uint32_t count = 0;
        while (resolved) {
            count += resolved & 1;
            resolved >>= 1;
        }
        bridge->stats.alerts_resolved += count;
        NIMCP_LOGGING_DEBUG("WM Substrate: %u alert(s) resolved", count);
    }

    bridge->active_alerts = new_alerts;
    bridge->is_constrained = (new_alerts != 0);

    return NIMCP_SUCCESS;
}

/**
 * @brief Apply metabolic constraints to world model
 *
 * WHAT: Enforce metabolic limits on WM operation
 * WHY:  WM must operate within energy budget
 * HOW:  Set WM parameters based on computed effects
 */
static nimcp_error_t apply_constraints_to_wm(omni_wm_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->world_model) return NIMCP_SUCCESS;

    const substrate_to_wm_effects_t* effects = &bridge->substrate_effects;

    /* Update operational parameters */
    bridge->current_horizon = effects->allowed_horizon;
    bridge->current_compute_rate = bridge->config.base_compute_rate *
                                    effects->compute_rate_scale;

    /* Enforce minimum rate */
    if (bridge->current_compute_rate < bridge->config.min_compute_rate) {
        bridge->current_compute_rate = bridge->config.min_compute_rate;
    }

    /* Track training state */
    bridge->training_was_active = effects->training_permitted;

    /* Update stats */
    float alpha = 0.05f;
    bridge->stats.mean_horizon_achieved =
        alpha * (float)bridge->current_horizon +
        (1.0f - alpha) * bridge->stats.mean_horizon_achieved;

    bridge->stats.mean_compute_rate =
        alpha * bridge->current_compute_rate +
        (1.0f - alpha) * bridge->stats.mean_compute_rate;

    /* In full implementation, would call WM APIs to set:
     * - omni_wm_set_max_horizon(wm, effects->allowed_horizon)
     * - omni_wm_set_learning_rate_scale(wm, effects->learning_rate_scale)
     * - omni_wm_set_prediction_mode(wm, effects->use_simplified_dynamics)
     */

    return NIMCP_SUCCESS;
}

/**
 * @brief Log metabolic state for monitoring
 *
 * WHAT: Periodically log metabolic state
 * WHY:  Enable monitoring and debugging
 * HOW:  Check interval, format and log state
 */
static nimcp_error_t log_metabolic_state(omni_wm_substrate_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_metabolic_logging) return NIMCP_SUCCESS;

    uint64_t now = get_current_time_us();
    uint64_t interval_us = bridge->config.log_interval_ms * 1000;

    if (now - bridge->last_log_time_us < interval_us) {
        return NIMCP_SUCCESS;
    }

    const wm_metabolic_availability_t* avail = &bridge->availability;

    NIMCP_LOGGING_DEBUG(
        "WM Substrate: ATP=%.2f O2=%.2f Gluc=%.2f Temp=%.1fC "
        "Capacity=%.2f Horizon=%u Rate=%.1f Constrained=%s",
        avail->atp_level,
        avail->oxygen_saturation,
        avail->glucose_level,
        avail->temperature,
        avail->computational_capacity,
        bridge->current_horizon,
        bridge->current_compute_rate,
        bridge->is_constrained ? "yes" : "no"
    );

    bridge->last_log_time_us = now;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

/**
 * @brief Handle metabolic state update message
 */
static nimcp_error_t handle_metabolic_update(const void* msg, size_t msg_size,
                                              nimcp_bio_promise_t promise, void* user_data) {
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(msg, NIMCP_ERROR_NULL_POINTER, "msg is NULL");
    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    omni_wm_substrate_bridge_t* bridge = (omni_wm_substrate_bridge_t*)user_data;

    nimcp_mutex_lock(bridge->base.mutex);
    /* Trigger immediate refresh on external metabolic update */
    update_metabolic_availability(bridge);
    compute_substrate_to_wm_effects(bridge);
    check_and_update_alerts(bridge);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle demand signal message
 */
static nimcp_error_t handle_demand_signal(const void* msg, size_t msg_size,
                                           nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* Demand signals typically come from WM, would update demand state */
    return NIMCP_SUCCESS;
}

/**
 * @brief Handle constraint response message
 */
static nimcp_error_t handle_constraint_response(const void* msg, size_t msg_size,
                                                 nimcp_bio_promise_t promise, void* user_data) {
    (void)msg;
    (void)msg_size;
    (void)promise;

    NIMCP_CHECK_THROW(user_data, NIMCP_ERROR_NULL_POINTER, "user_data is NULL");

    /* Would handle acknowledgement of constraints from WM */
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_default_config(
    omni_wm_substrate_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_default_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    memset(config, 0, sizeof(omni_wm_substrate_bridge_config_t));

    /* General settings */
    config->enable_modulation = true;
    config->sensitivity = 1.0f;

    /* ATP modulation settings */
    config->enable_atp_modulation = true;
    config->atp_training_threshold = WM_SUBSTRATE_ATP_TRAIN_THRESHOLD;
    config->atp_prediction_threshold = WM_SUBSTRATE_ATP_PREDICT_THRESHOLD;
    config->atp_critical_threshold = 0.2f;

    /* Oxygen modulation settings */
    config->enable_o2_modulation = true;
    config->o2_critical_threshold = WM_SUBSTRATE_O2_CRITICAL;
    config->o2_compute_scale = 1.0f;

    /* Glucose modulation settings */
    config->enable_glucose_modulation = true;
    config->glucose_learning_threshold = WM_SUBSTRATE_GLUCOSE_LEARN_THRESHOLD;

    /* Temperature settings */
    config->enable_temperature_effects = true;
    config->q10_coefficient = WM_SUBSTRATE_Q10_COMPUTATION;
    config->normal_temperature = WM_SUBSTRATE_NORMAL_TEMP;
    config->hyperthermia_threshold = 40.0f;

    /* Horizon settings */
    config->max_horizon = WM_SUBSTRATE_MAX_HORIZON;
    config->min_horizon = WM_SUBSTRATE_MIN_HORIZON;
    config->adaptive_horizon = true;

    /* Computation rate settings */
    config->base_compute_rate = WM_SUBSTRATE_DEFAULT_COMPUTE_RATE;
    config->min_compute_rate = 10.0f;
    config->adaptive_rate = true;

    /* Energy cost settings */
    config->atp_per_prediction = 0.001f;
    config->atp_per_training_step = 0.005f;
    config->atp_per_rollout_step = 0.002f;

    /* Logging settings */
    config->enable_metabolic_logging = true;
    config->log_interval_ms = WM_SUBSTRATE_LOG_INTERVAL_MS;

    /* Bio-async settings */
    config->enable_bio_async = true;

    return NIMCP_SUCCESS;
}

omni_wm_substrate_bridge_t* omni_wm_substrate_bridge_create(
    const omni_wm_substrate_bridge_config_t* config) {

    /* Allocate bridge structure */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_create", 0.0f);


    omni_wm_substrate_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_wm_substrate_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate WM substrate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "log_metabolic_state: bridge is NULL");
        return NULL;
    }

    /* Initialize base bridge */
    if (bridge_base_init(&bridge->base, BIO_MODULE_WM_SUBSTRATE_BRIDGE,
                         "wm_substrate_bridge") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_LOGGING_ERROR("Failed to initialize bridge base");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "log_metabolic_state: operation failed");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        omni_wm_substrate_bridge_default_config(&bridge->config);
    }

    /* Initialize operational state */
    bridge->current_horizon = bridge->config.max_horizon;
    bridge->current_compute_rate = bridge->config.base_compute_rate;
    bridge->is_constrained = false;
    bridge->active_alerts = 0;
    bridge->training_was_active = false;
    bridge->last_log_time_us = 0;

    /* Initialize availability with healthy defaults */
    bridge->availability.atp_level = 1.0f;
    bridge->availability.oxygen_saturation = 1.0f;
    bridge->availability.glucose_level = 1.0f;
    bridge->availability.temperature = bridge->config.normal_temperature;
    bridge->availability.metabolic_capacity = 1.0f;
    bridge->availability.computational_capacity = 1.0f;
    bridge->availability.learning_capacity = 1.0f;

    /* Initialize substrate effects with permissive defaults */
    bridge->substrate_effects.allowed_horizon = bridge->config.max_horizon;
    bridge->substrate_effects.horizon_scale = 1.0f;
    bridge->substrate_effects.compute_rate_scale = 1.0f;
    bridge->substrate_effects.training_permitted = true;
    bridge->substrate_effects.learning_rate_scale = 1.0f;
    bridge->substrate_effects.dreaming_permitted = true;
    bridge->substrate_effects.dream_length_scale = 1.0f;
    bridge->substrate_effects.q10_factor = 1.0f;
    bridge->substrate_effects.temperature_mod = 1.0f;
    bridge->substrate_effects.prediction_confidence_mod = 1.0f;

    /* Initialize stats observation values */
    bridge->stats.min_atp_observed = 1.0f;
    bridge->stats.min_o2_observed = 1.0f;
    bridge->stats.min_glucose_observed = 1.0f;
    bridge->stats.mean_horizon_achieved = (float)bridge->config.max_horizon;
    bridge->stats.mean_compute_rate = bridge->config.base_compute_rate;

    NIMCP_LOGGING_INFO("WM Substrate Bridge created successfully");
    return bridge;
}

void omni_wm_substrate_bridge_destroy(omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        omni_wm_substrate_bridge_disconnect_bio_async(bridge);
    }

    /* Cleanup base and free */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;

    NIMCP_LOGGING_INFO("WM Substrate Bridge destroyed");
}

nimcp_error_t omni_wm_substrate_bridge_reset(omni_wm_substrate_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_reset", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset effects */
    memset(&bridge->substrate_effects, 0, sizeof(substrate_to_wm_effects_t));
    memset(&bridge->wm_effects, 0, sizeof(wm_to_substrate_effects_t));

    /* Reset operational state */
    bridge->current_horizon = bridge->config.max_horizon;
    bridge->current_compute_rate = bridge->config.base_compute_rate;
    bridge->is_constrained = false;
    bridge->active_alerts = 0;
    bridge->training_was_active = false;

    /* Reset availability to healthy */
    bridge->availability.atp_level = 1.0f;
    bridge->availability.oxygen_saturation = 1.0f;
    bridge->availability.glucose_level = 1.0f;
    bridge->availability.temperature = bridge->config.normal_temperature;
    bridge->availability.metabolic_capacity = 1.0f;
    bridge->availability.computational_capacity = 1.0f;
    bridge->availability.learning_capacity = 1.0f;
    bridge->availability.is_stressed = false;
    bridge->availability.is_critical = false;

    /* Reset substrate effects to permissive */
    bridge->substrate_effects.allowed_horizon = bridge->config.max_horizon;
    bridge->substrate_effects.horizon_scale = 1.0f;
    bridge->substrate_effects.compute_rate_scale = 1.0f;
    bridge->substrate_effects.training_permitted = true;
    bridge->substrate_effects.learning_rate_scale = 1.0f;
    bridge->substrate_effects.dreaming_permitted = true;
    bridge->substrate_effects.dream_length_scale = 1.0f;
    bridge->substrate_effects.q10_factor = 1.0f;
    bridge->substrate_effects.temperature_mod = 1.0f;
    bridge->substrate_effects.prediction_confidence_mod = 1.0f;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(omni_wm_substrate_bridge_stats_t));
    bridge->stats.min_atp_observed = 1.0f;
    bridge->stats.min_o2_observed = 1.0f;
    bridge->stats.min_glucose_observed = 1.0f;
    bridge->stats.mean_horizon_achieved = (float)bridge->config.max_horizon;
    bridge->stats.mean_compute_rate = bridge->config.base_compute_rate;

    /* Reset base bridge (unlocked since we already hold the mutex) */
    bridge_base_reset_unlocked(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Connection API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_connect(
    omni_wm_substrate_bridge_t* bridge,
    omni_world_model_t* world_model,
    neural_substrate_t* substrate,
    metabolic_plasticity_t* metabolic) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_connect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_INVALID_PARAM, "world_model is required");
    NIMCP_CHECK_THROW(substrate, NIMCP_ERROR_INVALID_PARAM, "substrate is required");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->world_model = world_model;
    bridge->substrate = substrate;
    bridge->metabolic = metabolic;

    /* Update base connection state */
    bridge->base.system_a = world_model;
    bridge->base.system_b = substrate;
    bridge->base.system_a_connected = true;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = true;

    /* Initial metabolic state update */
    update_metabolic_availability(bridge);
    compute_substrate_to_wm_effects(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("WM Substrate Bridge connected: WM=%p, Substrate=%p, Metabolic=%p",
                       (void*)world_model, (void*)substrate, (void*)metabolic);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_connect_world_model(
    omni_wm_substrate_bridge_t* bridge,
    omni_world_model_t* world_model) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_connect_world_model", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(world_model, NIMCP_ERROR_NULL_POINTER, "world_model is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->world_model = world_model;
    bridge->base.system_a = world_model;
    bridge->base.system_a_connected = true;
    bridge->base.bridge_active = (bridge->substrate != NULL);
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_connect_substrate(
    omni_wm_substrate_bridge_t* bridge,
    neural_substrate_t* substrate) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_connect_substrate", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(substrate, NIMCP_ERROR_NULL_POINTER, "substrate is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->substrate = substrate;
    bridge->base.system_b = substrate;
    bridge->base.system_b_connected = true;
    bridge->base.bridge_active = (bridge->world_model != NULL);

    /* Update metabolic state from new substrate */
    update_metabolic_availability(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_connect_metabolic(
    omni_wm_substrate_bridge_t* bridge,
    metabolic_plasticity_t* metabolic) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_connect_metabolic", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(metabolic, NIMCP_ERROR_NULL_POINTER, "metabolic is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metabolic = metabolic;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_substrate_bridge_is_connected(const omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_is_connected", 0.0f);


    return bridge->world_model != NULL && bridge->substrate != NULL;
}

/* ============================================================================
 * Update API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_update(
    omni_wm_substrate_bridge_t* bridge,
    float dt) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_modulation) return NIMCP_SUCCESS;

    uint64_t start_time = get_current_time_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Step 1: Update metabolic availability from substrate */
    nimcp_error_t err = update_metabolic_availability(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_metabolic++;
    }

    /* Step 2: Compute constraints based on metabolic state */
    err = compute_substrate_to_wm_effects(bridge);
    if (err != NIMCP_SUCCESS) {
        bridge->stats.errors_constraint++;
    }

    /* Step 3: Check and update alerts */
    check_and_update_alerts(bridge);

    /* Step 4: Update WM-to-substrate effects (consumption tracking) */
    update_wm_to_substrate_effects(bridge, dt);

    /* Step 5: Apply constraints to world model */
    apply_constraints_to_wm(bridge);

    /* Step 6: Periodic logging */
    log_metabolic_state(bridge);

    /* Update timing statistics */
    bridge->stats.total_updates++;
    uint64_t elapsed = get_current_time_us() - start_time;
    bridge->stats.total_processing_time_ms += (double)elapsed / 1000.0;
    bridge->stats.mean_update_time_ms = bridge->stats.total_processing_time_ms /
                                         (double)bridge->stats.total_updates;
    bridge->stats.last_update_time_us = start_time;

    /* Record base update */
    bridge_base_record_update(&bridge->base);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_refresh_metabolic_state(
    omni_wm_substrate_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_refresh_metabolic_st", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    nimcp_error_t err = update_metabolic_availability(bridge);
    if (err == NIMCP_SUCCESS) {
        err = compute_substrate_to_wm_effects(bridge);
    }
    if (err == NIMCP_SUCCESS) {
        check_and_update_alerts(bridge);
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return err;
}

/* ============================================================================
 * Metabolic Constraint API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_get_availability(
    const omni_wm_substrate_bridge_t* bridge,
    wm_metabolic_availability_t* availability) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_availability", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(availability, NIMCP_ERROR_NULL_POINTER, "availability is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *availability = bridge->availability;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

bool omni_wm_substrate_bridge_can_train(const omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_can_train", 0.0f);


    return bridge->substrate_effects.training_permitted;
}

bool omni_wm_substrate_bridge_can_predict(const omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_can_predict", 0.0f);


    return bridge->availability.atp_level >= bridge->config.atp_prediction_threshold;
}

uint32_t omni_wm_substrate_bridge_get_allowed_horizon(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) return WM_SUBSTRATE_MIN_HORIZON;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_allowed_horizon", 0.0f);


    return bridge->substrate_effects.allowed_horizon;
}

float omni_wm_substrate_bridge_get_allowed_compute_rate(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) return 10.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_allowed_compute_", 0.0f);


    return bridge->current_compute_rate;
}

float omni_wm_substrate_bridge_get_learning_rate_scale(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_learning_rate_sc", 0.0f);


    return bridge->substrate_effects.learning_rate_scale;
}

/* ============================================================================
 * Demand Signaling API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_signal_demand(
    omni_wm_substrate_bridge_t* bridge,
    const wm_computational_demand_t* demand) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_signal_demand", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(demand, NIMCP_ERROR_NULL_POINTER, "demand is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->wm_effects.demand = *demand;

    /* Compute overall demand */
    bridge->wm_effects.demand.overall_demand =
        demand->prediction_demand * 0.4f +
        demand->training_demand * 0.4f +
        demand->rollout_demand * 0.2f;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Send bio-async message if connected */
    if (bridge->base.bio_async_enabled && bridge->base.bio_ctx) {
        /* Would send BIO_MSG_WM_SUBSTRATE_DEMAND */
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_report_predictions(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_predictions,
    uint32_t horizon) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_report_predictions", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->wm_effects.predictions_made += num_predictions;

    /* Update demand based on activity */
    float activity = (float)num_predictions * (float)horizon /
                      ((float)bridge->config.max_horizon * 100.0f);
    bridge->wm_effects.demand.prediction_demand = nimcp_clampf(activity, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_report_training(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_steps) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_report_training", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->wm_effects.training_steps += num_steps;

    /* Update demand */
    float activity = (float)num_steps / 100.0f;
    bridge->wm_effects.demand.training_demand = nimcp_clampf(activity, 0.0f, 1.0f);
    bridge->wm_effects.demand.training_active = (num_steps > 0);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_report_rollouts(
    omni_wm_substrate_bridge_t* bridge,
    uint32_t num_rollouts,
    uint32_t total_steps) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_report_rollouts", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->wm_effects.rollouts_completed += num_rollouts;

    /* Update demand */
    float activity = (float)total_steps / ((float)bridge->config.max_horizon * 10.0f);
    bridge->wm_effects.demand.rollout_demand = nimcp_clampf(activity, 0.0f, 1.0f);

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Alert Management API Implementation
 * ============================================================================ */

uint32_t omni_wm_substrate_bridge_get_active_alerts(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_active_alerts", 0.0f);


    return bridge->active_alerts;
}

bool omni_wm_substrate_bridge_has_alert(const omni_wm_substrate_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_has_alert", 0.0f);


    return bridge->active_alerts != 0;
}

bool omni_wm_substrate_bridge_check_alert(
    const omni_wm_substrate_bridge_t* bridge,
    uint32_t alert_bit) {

    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_check_alert", 0.0f);


    return (bridge->active_alerts & alert_bit) != 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

const substrate_to_wm_effects_t* omni_wm_substrate_bridge_get_substrate_effects(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_substrate_effect", 0.0f);


    return &bridge->substrate_effects;
}

const wm_to_substrate_effects_t* omni_wm_substrate_bridge_get_wm_effects(
    const omni_wm_substrate_bridge_t* bridge) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return NULL;


    }
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_wm_effects", 0.0f);


    return &bridge->wm_effects;
}

nimcp_error_t omni_wm_substrate_bridge_get_stats(
    const omni_wm_substrate_bridge_t* bridge,
    omni_wm_substrate_bridge_stats_t* stats) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_reset_stats(
    omni_wm_substrate_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_reset_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Preserve min observations */
    float min_atp = bridge->stats.min_atp_observed;
    float min_o2 = bridge->stats.min_o2_observed;
    float min_glucose = bridge->stats.min_glucose_observed;
    float max_temp = bridge->stats.max_temp_observed;

    memset(&bridge->stats, 0, sizeof(omni_wm_substrate_bridge_stats_t));

    /* Reset observation values to current */
    bridge->stats.min_atp_observed = min_atp;
    bridge->stats.min_o2_observed = min_o2;
    bridge->stats.min_glucose_observed = min_glucose;
    bridge->stats.max_temp_observed = max_temp;
    bridge->stats.mean_horizon_achieved = (float)bridge->config.max_horizon;
    bridge->stats.mean_compute_rate = bridge->config.base_compute_rate;

    nimcp_mutex_unlock(bridge->base.mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API Implementation
 * ============================================================================ */

nimcp_error_t omni_wm_substrate_bridge_connect_bio_async(
    omni_wm_substrate_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_connect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_bio_async) return NIMCP_SUCCESS;
    if (bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    /* Check if router is initialized */
    if (!bio_router_is_initialized()) {
        NIMCP_LOGGING_DEBUG("Bio-async router not initialized, skipping registration");
        return NIMCP_SUCCESS;
    }

    /* Register module with router */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_WM_SUBSTRATE_BRIDGE,
        .module_name = "wm_substrate_bridge",
        .inbox_capacity = 64,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (!bridge->base.bio_ctx) {
        NIMCP_LOGGING_WARN("Failed to register with bio-async router");
        return NIMCP_SUCCESS; /* Non-fatal */
    }

    /* Register message handlers */
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SUBSTRATE_METABOLIC,
                                handle_metabolic_update);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SUBSTRATE_DEMAND,
                                handle_demand_signal);
    bio_router_register_handler(bridge->base.bio_ctx,
                                BIO_MSG_WM_SUBSTRATE_CONSTRAINT,
                                handle_constraint_response);

    bridge->base.bio_async_enabled = true;
    NIMCP_LOGGING_INFO("WM Substrate Bridge connected to bio-async router");

    return NIMCP_SUCCESS;
}

nimcp_error_t omni_wm_substrate_bridge_disconnect_bio_async(
    omni_wm_substrate_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_disconnect_bio_async", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->base.bio_async_enabled) return NIMCP_SUCCESS;

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("WM Substrate Bridge disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool omni_wm_substrate_bridge_is_bio_async_connected(
    const omni_wm_substrate_bridge_t* bridge) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_is_bio_async_connect", 0.0f);


    return bridge_base_is_bio_async_connected(bridge ? &bridge->base : NULL);
}

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* omni_wm_substrate_msg_type_to_string(omni_wm_substrate_msg_type_t msg_type) {
    switch (msg_type) {
        /* Alert Messages (extended types) */
        case BIO_MSG_WM_SUBSTRATE_ALERT_LOW_ATP:
            return "ALERT_LOW_ATP";
        case BIO_MSG_WM_SUBSTRATE_ALERT_HYPOXIA:
            return "ALERT_HYPOXIA";
        case BIO_MSG_WM_SUBSTRATE_ALERT_LOW_GLUCOSE:
            return "ALERT_LOW_GLUCOSE";
        case BIO_MSG_WM_SUBSTRATE_ALERT_HYPERTHERMIA:
            return "ALERT_HYPERTHERMIA";
        case BIO_MSG_WM_SUBSTRATE_ALERT_RESOLVED:
            return "ALERT_RESOLVED";

        /* Bridge Status Messages */
        case BIO_MSG_WM_SUBSTRATE_BRIDGE_STATUS:
            return "BRIDGE_STATUS";
        case BIO_MSG_WM_SUBSTRATE_BRIDGE_ERROR:
            return "BRIDGE_ERROR";
        case BIO_MSG_WM_SUBSTRATE_STATS_UPDATE:
            return "STATS_UPDATE";

        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Convert bio_messages.h message types to string (for core types)
 */
const char* omni_wm_substrate_core_msg_to_string(uint32_t msg_type) {
    switch (msg_type) {
        case BIO_MSG_WM_SUBSTRATE_METABOLIC:
            return "METABOLIC";
        case BIO_MSG_WM_SUBSTRATE_DEMAND:
            return "DEMAND";
        case BIO_MSG_WM_SUBSTRATE_CONSTRAINT:
            return "CONSTRAINT";
        case BIO_MSG_WM_SUBSTRATE_HORIZON_ADJUST:
            return "HORIZON_ADJUST";
        default:
            return omni_wm_substrate_msg_type_to_string((omni_wm_substrate_msg_type_t)msg_type);
    }
}

nimcp_error_t omni_wm_substrate_bridge_validate_config(
    const omni_wm_substrate_bridge_config_t* config) {

    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_validate_config", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    /* Validate sensitivity range */
    if (config->sensitivity < 0.5f || config->sensitivity > 2.0f) {
        NIMCP_LOGGING_WARN("Sensitivity %.2f out of range [0.5, 2.0]",
                          config->sensitivity);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate ATP thresholds */
    if (config->enable_atp_modulation) {
        if (config->atp_training_threshold < 0.0f ||
            config->atp_training_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid atp_training_threshold: %.2f",
                              config->atp_training_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->atp_prediction_threshold < 0.0f ||
            config->atp_prediction_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid atp_prediction_threshold: %.2f",
                              config->atp_prediction_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->atp_critical_threshold < 0.0f ||
            config->atp_critical_threshold > config->atp_prediction_threshold) {
            NIMCP_LOGGING_WARN("Invalid atp_critical_threshold: %.2f",
                              config->atp_critical_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate O2 threshold */
    if (config->enable_o2_modulation) {
        if (config->o2_critical_threshold < 0.0f ||
            config->o2_critical_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid o2_critical_threshold: %.2f",
                              config->o2_critical_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate glucose threshold */
    if (config->enable_glucose_modulation) {
        if (config->glucose_learning_threshold < 0.0f ||
            config->glucose_learning_threshold > 1.0f) {
            NIMCP_LOGGING_WARN("Invalid glucose_learning_threshold: %.2f",
                              config->glucose_learning_threshold);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate temperature settings */
    if (config->enable_temperature_effects) {
        if (config->q10_coefficient < 1.0f || config->q10_coefficient > 5.0f) {
            NIMCP_LOGGING_WARN("Invalid q10_coefficient: %.2f",
                              config->q10_coefficient);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->normal_temperature < 30.0f || config->normal_temperature > 45.0f) {
            NIMCP_LOGGING_WARN("Invalid normal_temperature: %.1f",
                              config->normal_temperature);
            return NIMCP_ERROR_INVALID_PARAM;
        }
        if (config->hyperthermia_threshold <= config->normal_temperature) {
            NIMCP_LOGGING_WARN("Hyperthermia threshold %.1f <= normal temp %.1f",
                              config->hyperthermia_threshold,
                              config->normal_temperature);
            return NIMCP_ERROR_INVALID_PARAM;
        }
    }

    /* Validate horizon settings */
    if (config->max_horizon < config->min_horizon) {
        NIMCP_LOGGING_WARN("max_horizon %u < min_horizon %u",
                          config->max_horizon, config->min_horizon);
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->max_horizon > WM_SUBSTRATE_MAX_HORIZON * 2) {
        NIMCP_LOGGING_WARN("max_horizon %u too large", config->max_horizon);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate compute rate settings */
    if (config->base_compute_rate <= 0.0f) {
        NIMCP_LOGGING_WARN("Invalid base_compute_rate: %.1f",
                          config->base_compute_rate);
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (config->min_compute_rate < 0.0f ||
        config->min_compute_rate > config->base_compute_rate) {
        NIMCP_LOGGING_WARN("Invalid min_compute_rate: %.1f",
                          config->min_compute_rate);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Validate energy costs */
    if (config->atp_per_prediction < 0.0f ||
        config->atp_per_training_step < 0.0f ||
        config->atp_per_rollout_step < 0.0f) {
        NIMCP_LOGGING_WARN("Negative energy costs not allowed");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    return NIMCP_SUCCESS;
}

float omni_wm_substrate_compute_q10_factor(
    float temperature,
    float normal_temp,
    float q10) {

    /* Q10 formula: rate_scale = Q10^((T - T_normal) / 10) */
    /* Phase 8: Heartbeat at operation start */
    omni_wm_substrate_bridge_heartbeat("omni_wm_subs_omni_wm_substrate_co", 0.0f);


    float delta_t = temperature - normal_temp;
    float exponent = delta_t / 10.0f;
    float factor = powf(q10, exponent);

    /* Clamp to reasonable range */
    return nimcp_clampf(factor, 0.1f, 10.0f);
}
