/**
 * @file nimcp_hypo_reasoning_fep_bridge.c
 * @brief Implementation of Hypothalamus-Reasoning FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus drives and reasoning system
 * WHY:  Fatigue drives affect reasoning precision; cognitive load generates free energy
 * HOW:  Map fatigue to precision reduction, cognitive load to free energy, errors to PE
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_reasoning_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_reasoning_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_reasoning_fep_default_config(hypo_reasoning_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.5f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;
    config->fatigue_precision_scale = 0.8f;
    config->load_fe_scale = 2.0f;
    config->error_pe_scale = HYPO_REASONING_FEP_ERROR_PE_SCALE;
    config->arousal_inference_scale = 1.2f;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

hypo_reasoning_fep_bridge_t* hypo_reasoning_fep_create(
    const hypo_reasoning_fep_config_t* config,
    fep_system_t* fep_system) {

    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo-Reasoning FEP bridge: NULL FEP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_reasoning_fep_default_config: fep_system is NULL");
        return NULL;
    }

    hypo_reasoning_fep_bridge_t* bridge = (hypo_reasoning_fep_bridge_t*)
        nimcp_malloc(sizeof(hypo_reasoning_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_reasoning_fep_create: allocation failed");

        return NULL;

    }

    memset(bridge, 0, sizeof(hypo_reasoning_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        hypo_reasoning_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    if (bridge_base_init(&bridge->base, 0, "hypo_reasoning_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_reasoning_fep_default_config: bridge->base is NULL");
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_precision = 1.0f;
    bridge->fep_effects.precision = 1.0f;

    NIMCP_LOGGING_INFO("Hypo-Reasoning FEP bridge created");
    return bridge;
}

void hypo_reasoning_fep_destroy(hypo_reasoning_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        hypo_reasoning_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int hypo_reasoning_fep_reset(hypo_reasoning_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    memset(&bridge->fep_effects, 0, sizeof(hypo_reasoning_fep_effects_t));
    memset(&bridge->reasoning_effects, 0, sizeof(reasoning_hypo_effects_t));
    memset(&bridge->state, 0, sizeof(hypo_reasoning_fep_state_t));
    memset(&bridge->stats, 0, sizeof(hypo_reasoning_fep_stats_t));

    bridge->state.active = true;
    bridge->state.current_precision = 1.0f;
    bridge->fep_effects.precision = 1.0f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Processing
 * ============================================================================ */

int hypo_reasoning_fep_update(hypo_reasoning_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->state.active, NIMCP_ERROR_NULL_POINTER, "bridge is not active");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current fatigue from drive system if connected */
    float fatigue_level = bridge->state.current_fatigue;
    if (bridge->drive_system) {
        hypo_drive_state_t fatigue_state;
        if (hypo_drive_get_state(bridge->drive_system, HYPO_DRIVE_FATIGUE, &fatigue_state)) {
            fatigue_level = fatigue_state.level;
            bridge->state.current_fatigue = fatigue_level;
        }
    }

    /* Compute precision reduction from fatigue */
    float precision;
    hypo_reasoning_fep_modulate_precision(bridge, fatigue_level, &precision);
    bridge->fep_effects.precision = precision;
    bridge->state.current_precision = precision;

    /* Track precision reductions */
    if (precision < 0.7f) {
        bridge->stats.precision_reductions++;
    }

    /* Compute free energy from cognitive load */
    float cognitive_load = bridge->state.current_load;
    float fe;
    hypo_reasoning_fep_compute_fe(bridge, cognitive_load, &fe);
    bridge->fep_effects.free_energy = fe;

    /* Track FE spikes */
    if (fe > HYPO_REASONING_FEP_LOAD_THRESHOLD * bridge->config.load_fe_scale) {
        bridge->stats.fe_spikes++;
    }

    /* Compute fatigue effect on reasoning */
    bridge->fep_effects.fatigue_effect = fatigue_level * bridge->config.fatigue_precision_scale;

    /* Active inference response */
    if (bridge->config.enable_active_inference) {
        float ai_strength = 0.0f;
        if (fatigue_level > HYPO_REASONING_FEP_FATIGUE_THRESHOLD) {
            /* High fatigue: signal need for rest */
            ai_strength = (fatigue_level - HYPO_REASONING_FEP_FATIGUE_THRESHOLD) /
                         (1.0f - HYPO_REASONING_FEP_FATIGUE_THRESHOLD);
            bridge->stats.active_inference_triggers++;
        }
        bridge->fep_effects.active_inference_strength = ai_strength;
    }

    /* Update reverse effects: load induces fatigue */
    bridge->reasoning_effects.load_induced_fatigue =
        cognitive_load * 0.1f * (float)delta_ms / 1000.0f;

    /* Update stats */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    float new_avg_fe = 0.95f * bridge->stats.avg_free_energy + 0.05f * fe;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    float new_avg_p = 0.95f * bridge->stats.avg_precision + 0.05f * precision;
    if (isfinite(new_avg_p)) bridge->stats.avg_precision = new_avg_p;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_reasoning_fep_compute_fe(hypo_reasoning_fep_bridge_t* bridge,
    float cognitive_load, float* free_energy) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(free_energy, NIMCP_ERROR_NULL_POINTER, "free_energy is NULL");

    /* Cognitive load maps directly to free energy
     * Higher load = more computational surprise = higher FE */
    float fe = cognitive_load * bridge->config.load_fe_scale *
               bridge->config.drive_fe_weight;

    /* Bound free energy */
    if (fe < 0.0f) fe = 0.0f;
    if (fe > 10.0f) fe = 10.0f;

    *free_energy = fe;
    bridge->fep_effects.cognitive_load_fe = fe;
    bridge->state.current_load = cognitive_load;

    return 0;
}

int hypo_reasoning_fep_modulate_precision(hypo_reasoning_fep_bridge_t* bridge,
    float fatigue_level, float* precision) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(precision, NIMCP_ERROR_NULL_POINTER, "precision is NULL");

    /* Fatigue reduces precision
     * precision = 1.0 - (fatigue * scale)
     * Bounded to minimum precision */
    float p = 1.0f - (fatigue_level * bridge->config.fatigue_precision_scale);

    /* Apply precision modulation factor */
    p *= bridge->config.precision_modulation;

    /* Enforce minimum precision */
    if (p < HYPO_REASONING_FEP_PRECISION_MIN) {
        p = HYPO_REASONING_FEP_PRECISION_MIN;
    }
    if (p > 1.0f) p = 1.0f;

    *precision = p;
    return 0;
}

/* ============================================================================
 * Event Reporting
 * ============================================================================ */

int hypo_reasoning_fep_report_error(hypo_reasoning_fep_bridge_t* bridge,
    float error_magnitude) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Errors generate prediction error */
    float pe = error_magnitude * bridge->config.error_pe_scale *
               bridge->config.prediction_error_gain;

    bridge->fep_effects.prediction_error = pe;
    bridge->fep_effects.error_pe = pe;
    bridge->state.reasoning_errors++;

    /* Errors increase drive urgency */
    bridge->reasoning_effects.error_urgency = pe * 0.2f;

    /* Update stats */
    float new_avg_pe = 0.9f * bridge->stats.avg_prediction_error + 0.1f * pe;
    if (isfinite(new_avg_pe)) bridge->stats.avg_prediction_error = new_avg_pe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_reasoning_fep_report_success(hypo_reasoning_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Success satisfies drive slightly */
    bridge->reasoning_effects.success_satisfaction += 0.1f;
    if (bridge->reasoning_effects.success_satisfaction > 1.0f) {
        bridge->reasoning_effects.success_satisfaction = 1.0f;
    }

    bridge->state.reasoning_successes++;

    /* Reduce prediction error on success */
    bridge->fep_effects.prediction_error *= 0.9f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int hypo_reasoning_fep_get_effects(const hypo_reasoning_fep_bridge_t* bridge,
    hypo_reasoning_fep_effects_t* effects) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    *effects = bridge->fep_effects;
    return 0;
}

int hypo_reasoning_fep_get_stats(const hypo_reasoning_fep_bridge_t* bridge,
    hypo_reasoning_fep_stats_t* stats) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int hypo_reasoning_fep_connect_bio_async(hypo_reasoning_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,
        .module_name = "hypo_reasoning_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo-Reasoning FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_reasoning_fep_disconnect_bio_async(hypo_reasoning_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool hypo_reasoning_fep_is_bio_async_connected(const hypo_reasoning_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int hypo_reasoning_fep_process_messages(hypo_reasoning_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Message processing uses handler-based callbacks registered with bio_router_register_handler.
     * Handlers are invoked automatically when messages arrive - no polling needed here.
     * Future: Register handlers in connect_bio_async for specific message types. */

    return 0;
}

/* ============================================================================
 * Drive System Connection
 * ============================================================================ */

int hypo_reasoning_fep_connect_drives(hypo_reasoning_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->drive_system = drives;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo-Reasoning FEP bridge connected to drive system");
    return 0;
}
