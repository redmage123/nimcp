/**
 * @file nimcp_hypo_curiosity_fep_bridge.c
 * @brief Implementation of Hypothalamus-Curiosity FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus CURIOSITY drive and curiosity system
 * WHY:  CURIOSITY drive motivates exploration; information gain reduces free energy
 * HOW:  Map CURIOSITY drive to exploration weight, information gain to FE reduction
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_curiosity_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_curiosity_fep_default_config(hypo_curiosity_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.5f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;
    config->curiosity_exploration_scale = HYPO_CURIOSITY_FEP_EXPLORATION_WEIGHT;
    config->info_gain_fe_reduction = HYPO_CURIOSITY_FEP_INFO_GAIN_SCALE;
    config->novelty_pe_scale = HYPO_CURIOSITY_FEP_NOVELTY_PE_SCALE;
    config->epistemic_value_weight = 0.5f;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

hypo_curiosity_fep_bridge_t* hypo_curiosity_fep_create(
    const hypo_curiosity_fep_config_t* config,
    fep_system_t* fep_system) {

    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo-Curiosity FEP bridge: NULL FEP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_curiosity_fep_default_config: fep_system is NULL");
        return NULL;
    }

    hypo_curiosity_fep_bridge_t* bridge = (hypo_curiosity_fep_bridge_t*)
        nimcp_malloc(sizeof(hypo_curiosity_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_curiosity_fep_create: allocation failed");

        return NULL;

    }

    memset(bridge, 0, sizeof(hypo_curiosity_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        hypo_curiosity_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    if (bridge_base_init(&bridge->base, 0, "hypo_curiosity_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_curiosity_fep_default_config: bridge->base is NULL");
        return NULL;
    }

    bridge->state.active = true;
    bridge->fep_effects.precision = 1.0f;
    bridge->fep_effects.exploration_weight = 0.5f;

    NIMCP_LOGGING_INFO("Hypo-Curiosity FEP bridge created");
    return bridge;
}

void hypo_curiosity_fep_destroy(hypo_curiosity_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        hypo_curiosity_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int hypo_curiosity_fep_reset(hypo_curiosity_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    memset(&bridge->fep_effects, 0, sizeof(hypo_curiosity_fep_effects_t));
    memset(&bridge->curiosity_effects, 0, sizeof(curiosity_hypo_effects_t));
    memset(&bridge->state, 0, sizeof(hypo_curiosity_fep_state_t));
    memset(&bridge->stats, 0, sizeof(hypo_curiosity_fep_stats_t));

    bridge->state.active = true;
    bridge->fep_effects.precision = 1.0f;
    bridge->fep_effects.exploration_weight = 0.5f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Processing
 * ============================================================================ */

int hypo_curiosity_fep_update(hypo_curiosity_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->state.active, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or inactive");
    (void)delta_ms; /* Currently unused but available for time-based updates */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current curiosity drive from drive system if connected */
    float curiosity_drive = bridge->state.current_curiosity_drive;
    if (bridge->drive_system) {
        hypo_drive_state_t curiosity_state;
        if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_CURIOSITY, &curiosity_state)) {
            curiosity_drive = curiosity_state.level;
            bridge->state.current_curiosity_drive = curiosity_drive;
        }
    }

    /* Compute exploration weight from curiosity drive */
    float exploration_weight;
    hypo_curiosity_fep_compute_exploration(bridge, curiosity_drive, &exploration_weight);
    bridge->fep_effects.exploration_weight = exploration_weight;

    /* Track exploration triggers */
    if (exploration_weight > HYPO_CURIOSITY_FEP_DRIVE_THRESHOLD) {
        bridge->state.exploration_triggers++;
    }

    /* Compute epistemic value (expected information gain) */
    float epistemic_value = curiosity_drive * bridge->config.epistemic_value_weight;
    bridge->fep_effects.epistemic_value = epistemic_value;

    /* Compute free energy from current state
     * Base FE from curiosity drive (unsatisfied curiosity = high FE) */
    float fe = curiosity_drive * bridge->config.drive_fe_weight;

    /* Apply any FE reduction from recent information gain */
    float fe_reduction = bridge->state.current_info_gain * bridge->config.info_gain_fe_reduction;
    fe -= fe_reduction;
    if (fe < 0.0f) fe = 0.0f;
    bridge->fep_effects.free_energy = fe;
    bridge->fep_effects.info_gain_fe_reduction = fe_reduction;

    /* Modulate precision based on novelty */
    float novelty = bridge->state.current_novelty;
    float precision;
    hypo_curiosity_fep_modulate_precision(bridge, novelty, &precision);
    bridge->fep_effects.precision = precision;

    /* Set novelty signal */
    bridge->fep_effects.novelty_signal = novelty;

    /* Active inference response */
    if (bridge->config.enable_active_inference) {
        float ai_strength = 0.0f;
        if (curiosity_drive > HYPO_CURIOSITY_FEP_DRIVE_THRESHOLD) {
            /* High curiosity: signal need for exploration */
            ai_strength = (curiosity_drive - HYPO_CURIOSITY_FEP_DRIVE_THRESHOLD) /
                         (1.0f - HYPO_CURIOSITY_FEP_DRIVE_THRESHOLD);
            ai_strength = ai_strength > 1.0f ? 1.0f : ai_strength;
            bridge->stats.exploration_policies++;
        }
        bridge->fep_effects.active_inference_strength = ai_strength;
    }

    /* Update reverse effects */
    bridge->curiosity_effects.info_gain_satisfaction =
        bridge->state.current_info_gain * 0.5f;
    bridge->curiosity_effects.learning_progress = (bridge->config.info_gain_fe_reduction > 1e-6f)
        ? fe_reduction / bridge->config.info_gain_fe_reduction : 0.0f;
    bridge->curiosity_effects.novelty_drive_boost = novelty * 0.2f;

    /* Update stats */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    float new_avg_fe = 0.95f * bridge->stats.avg_free_energy + 0.05f * fe;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    float new_avg_ew = 0.95f * bridge->stats.avg_exploration_weight + 0.05f * exploration_weight;
    if (isfinite(new_avg_ew)) bridge->stats.avg_exploration_weight = new_avg_ew;
    float new_avg_ig = 0.95f * bridge->stats.avg_info_gain + 0.05f * bridge->state.current_info_gain;
    if (isfinite(new_avg_ig)) bridge->stats.avg_info_gain = new_avg_ig;

    /* Decay current info gain and novelty */
    bridge->state.current_info_gain *= 0.95f;
    bridge->state.current_novelty *= 0.9f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_curiosity_fep_compute_exploration(hypo_curiosity_fep_bridge_t* bridge,
    float curiosity_drive, float* exploration_weight) {

    NIMCP_FEP_CHECK_THROW(bridge && exploration_weight, NIMCP_ERROR_NULL_POINTER, "bridge or exploration_weight is NULL");

    /* Curiosity drive directly scales exploration weight
     * Higher drive = stronger motivation for exploratory policies */
    float ew = curiosity_drive * bridge->config.curiosity_exploration_scale;

    /* Bound exploration weight */
    if (ew < 0.0f) ew = 0.0f;
    if (ew > 1.0f) ew = 1.0f;

    *exploration_weight = ew;
    return 0;
}

int hypo_curiosity_fep_compute_fe_reduction(hypo_curiosity_fep_bridge_t* bridge,
    float info_gain, float* fe_reduction) {

    NIMCP_FEP_CHECK_THROW(bridge && fe_reduction, NIMCP_ERROR_NULL_POINTER, "bridge or fe_reduction is NULL");

    /* Information gain reduces free energy
     * Learning new information satisfies curiosity and reduces uncertainty */
    float reduction = info_gain * bridge->config.info_gain_fe_reduction;

    /* Bound reduction */
    if (reduction < 0.0f) reduction = 0.0f;
    if (reduction > 5.0f) reduction = 5.0f;

    *fe_reduction = reduction;
    return 0;
}

int hypo_curiosity_fep_modulate_precision(hypo_curiosity_fep_bridge_t* bridge,
    float novelty_level, float* precision) {

    NIMCP_FEP_CHECK_THROW(bridge && precision, NIMCP_ERROR_NULL_POINTER, "bridge or precision is NULL");

    /* Novelty increases precision (heightened attention to novel stimuli)
     * This reflects the biological response of increased attention to novelty */
    float p = 1.0f + (novelty_level * 0.5f);

    /* Apply precision modulation factor */
    p *= bridge->config.precision_modulation;

    /* Bound precision */
    if (p < 0.5f) p = 0.5f;
    if (p > 2.0f) p = 2.0f;

    *precision = p;
    return 0;
}

/* ============================================================================
 * Event Reporting
 * ============================================================================ */

int hypo_curiosity_fep_report_info_gain(hypo_curiosity_fep_bridge_t* bridge,
    float info_gain) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Bound info gain */
    if (info_gain < 0.0f) info_gain = 0.0f;
    if (info_gain > 1.0f) info_gain = 1.0f;

    bridge->state.current_info_gain = info_gain;
    bridge->state.info_gain_events++;

    /* Compute FE reduction */
    float fe_reduction;
    hypo_curiosity_fep_compute_fe_reduction(bridge, info_gain, &fe_reduction);
    bridge->state.cumulative_fe_reduction += fe_reduction;
    bridge->stats.total_fe_reduction += fe_reduction;
    bridge->stats.info_gain_fe_reductions++;

    /* Update satisfaction effect */
    bridge->curiosity_effects.info_gain_satisfaction = info_gain * 0.5f;

    /* Satisfy the curiosity drive if connected */
    if (bridge->drive_system && info_gain > 0.3f) {
        hypo_drive_satisfy(bridge->drive_system, HYPO_DRIVE_CURIOSITY, info_gain);
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_curiosity_fep_report_novelty(hypo_curiosity_fep_bridge_t* bridge,
    float novelty_magnitude) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Bound novelty */
    if (novelty_magnitude < 0.0f) novelty_magnitude = 0.0f;
    if (novelty_magnitude > 1.0f) novelty_magnitude = 1.0f;

    bridge->state.current_novelty = novelty_magnitude;

    /* Novelty generates prediction error (surprise) */
    float pe = novelty_magnitude * bridge->config.novelty_pe_scale *
               bridge->config.prediction_error_gain;
    bridge->fep_effects.prediction_error = pe;
    bridge->stats.novelty_pe_events++;

    /* Novelty boosts curiosity drive */
    bridge->curiosity_effects.novelty_drive_boost = novelty_magnitude * 0.2f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int hypo_curiosity_fep_get_effects(const hypo_curiosity_fep_bridge_t* bridge,
    hypo_curiosity_fep_effects_t* effects) {

    NIMCP_FEP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");
    *effects = bridge->fep_effects;
    return 0;
}

int hypo_curiosity_fep_get_stats(const hypo_curiosity_fep_bridge_t* bridge,
    hypo_curiosity_fep_stats_t* stats) {

    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int hypo_curiosity_fep_connect_bio_async(hypo_curiosity_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,
        .module_name = "hypo_curiosity_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo-Curiosity FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_curiosity_fep_disconnect_bio_async(hypo_curiosity_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool hypo_curiosity_fep_is_bio_async_connected(const hypo_curiosity_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int hypo_curiosity_fep_process_messages(hypo_curiosity_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Message processing uses handler-based callbacks registered with bio_router_register_handler.
     * Handlers are invoked automatically when messages arrive - no polling needed here.
     * Future: Register handlers in connect_bio_async for specific message types. */

    return 0;
}

/* ============================================================================
 * Drive System Connection
 * ============================================================================ */

int hypo_curiosity_fep_connect_drives(hypo_curiosity_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->drive_system = drives;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo-Curiosity FEP bridge connected to drive system");
    return 0;
}
