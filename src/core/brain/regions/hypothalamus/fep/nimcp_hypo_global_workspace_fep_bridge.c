/**
 * @file nimcp_hypo_global_workspace_fep_bridge.c
 * @brief Implementation of Hypothalamus-Global Workspace FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus drives and global workspace
 * WHY:  Drive priority influences broadcast priority; attention demand generates FE
 * HOW:  Map drive urgency to broadcast weight, attention demand to free energy
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_global_workspace_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_global_workspace_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_gw_fep_default_config(hypo_gw_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.5f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;
    config->priority_broadcast_scale = 1.5f;
    config->attention_fe_scale = HYPO_GW_FEP_ATTENTION_FE_SCALE;
    config->arousal_availability_scale = 0.8f;
    config->urgency_competition_scale = 1.2f;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

hypo_gw_fep_bridge_t* hypo_gw_fep_create(
    const hypo_gw_fep_config_t* config,
    fep_system_t* fep_system) {

    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo-GW FEP bridge: NULL FEP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gw_fep_default_config: fep_system is NULL");
        return NULL;
    }

    hypo_gw_fep_bridge_t* bridge = (hypo_gw_fep_bridge_t*)
        nimcp_malloc(sizeof(hypo_gw_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_gw_fep_create: allocation failed");

        return NULL;

    }

    memset(bridge, 0, sizeof(hypo_gw_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        hypo_gw_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    if (bridge_base_init(&bridge->base, 0, "hypo_global_workspace_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_gw_fep_default_config: bridge->base is NULL");
        return NULL;
    }

    bridge->state.active = true;
    bridge->fep_effects.precision = 1.0f;
    bridge->fep_effects.workspace_availability = 1.0f;

    NIMCP_LOGGING_INFO("Hypo-GW FEP bridge created");
    return bridge;
}

void hypo_gw_fep_destroy(hypo_gw_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        hypo_gw_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int hypo_gw_fep_reset(hypo_gw_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    memset(&bridge->fep_effects, 0, sizeof(hypo_gw_fep_effects_t));
    memset(&bridge->gw_effects, 0, sizeof(gw_hypo_effects_t));
    memset(&bridge->state, 0, sizeof(hypo_gw_fep_state_t));
    memset(&bridge->stats, 0, sizeof(hypo_gw_fep_stats_t));

    bridge->state.active = true;
    bridge->fep_effects.precision = 1.0f;
    bridge->fep_effects.workspace_availability = 1.0f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Processing
 * ============================================================================ */

int hypo_gw_fep_update(hypo_gw_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(bridge->state.active, NIMCP_ERROR_NULL_POINTER, "bridge is not active");
    (void)delta_ms; /* Currently unused but available for time-based updates */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current arousal and priority from drive system if connected */
    float arousal_level = bridge->state.current_arousal;
    float drive_urgency = bridge->state.current_priority;
    hypo_drive_type_t highest_drive = HYPO_DRIVE_CURIOSITY;

    if (bridge->drive_system) {
        hypo_drive_system_t drive_state;
        if (hypo_drive_get_system_state(bridge->drive_system, &drive_state)) {
            arousal_level = drive_state.arousal_level;
            bridge->state.current_arousal = arousal_level;
            highest_drive = drive_state.highest_priority;
            bridge->state.dominant_drive = highest_drive;

            /* Get urgency of highest priority drive */
            hypo_drive_state_t priority_drive;
            if (hypo_drive_get_state(bridge->drive_system, highest_drive, &priority_drive)) {
                drive_urgency = priority_drive.urgency;
                bridge->state.current_priority = drive_urgency;
            }
        }
    }

    /* Compute broadcast priority from drive urgency */
    float broadcast_priority;
    hypo_gw_fep_compute_broadcast_priority(bridge, drive_urgency, &broadcast_priority);
    bridge->fep_effects.broadcast_priority = broadcast_priority;

    /* Compute free energy from attention demand */
    float attention_demand = bridge->state.current_attention_demand;
    float fe;
    hypo_gw_fep_compute_fe(bridge, attention_demand, &fe);
    bridge->fep_effects.free_energy = fe;

    /* Track attention FE spikes */
    if (fe > HYPO_GW_FEP_PRIORITY_THRESHOLD * bridge->config.attention_fe_scale) {
        bridge->stats.attention_fe_spikes++;
    }

    /* Compute workspace availability from arousal */
    float precision;
    hypo_gw_fep_modulate_precision(bridge, arousal_level, &precision);
    bridge->fep_effects.precision = precision;
    bridge->fep_effects.workspace_availability = precision;

    /* Competition strength based on urgency */
    bridge->fep_effects.competition_strength =
        drive_urgency * bridge->config.urgency_competition_scale;

    /* Active inference response */
    if (bridge->config.enable_active_inference) {
        float ai_strength = 0.0f;
        if (drive_urgency > HYPO_GW_FEP_PRIORITY_THRESHOLD) {
            /* High urgency: push for workspace access */
            ai_strength = (drive_urgency - HYPO_GW_FEP_PRIORITY_THRESHOLD) /
                         (1.0f - HYPO_GW_FEP_PRIORITY_THRESHOLD);
            bridge->stats.active_inference_triggers++;
        }
        bridge->fep_effects.active_inference_strength = ai_strength;
    }

    /* Update reverse effects */
    bridge->gw_effects.drive_awareness = broadcast_priority;
    bridge->gw_effects.action_selection_bias = drive_urgency * 0.5f;

    /* Update stats */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    float new_avg_fe = 0.95f * bridge->stats.avg_free_energy + 0.05f * fe;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    float new_avg_bp = 0.95f * bridge->stats.avg_broadcast_priority + 0.05f * broadcast_priority;
    if (isfinite(new_avg_bp)) bridge->stats.avg_broadcast_priority = new_avg_bp;
    float new_avg_wa = 0.95f * bridge->stats.avg_workspace_availability + 0.05f * precision;
    if (isfinite(new_avg_wa)) bridge->stats.avg_workspace_availability = new_avg_wa;

    /* Update broadcast win rate */
    uint64_t total_broadcasts = bridge->state.broadcast_wins + bridge->state.broadcast_losses;
    if (total_broadcasts > 0) {
        bridge->stats.broadcast_win_rate =
            (float)bridge->state.broadcast_wins / (float)total_broadcasts;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gw_fep_compute_fe(hypo_gw_fep_bridge_t* bridge,
    float attention_demand, float* free_energy) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(free_energy, NIMCP_ERROR_NULL_POINTER, "free_energy is NULL");

    /* Attention demand maps to free energy
     * Higher demand = more competition = higher FE */
    float fe = attention_demand * bridge->config.attention_fe_scale *
               bridge->config.drive_fe_weight;

    /* Bound free energy */
    if (fe < 0.0f) fe = 0.0f;
    if (fe > 10.0f) fe = 10.0f;

    *free_energy = fe;
    bridge->state.current_attention_demand = attention_demand;

    return 0;
}

int hypo_gw_fep_modulate_precision(hypo_gw_fep_bridge_t* bridge,
    float arousal_level, float* precision) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(precision, NIMCP_ERROR_NULL_POINTER, "precision is NULL");

    /* Arousal modulates workspace availability/precision
     * Inverted-U relationship: optimal arousal = best precision
     * Too low or too high arousal reduces precision */
    float optimal_arousal = 0.6f;
    float distance = fabsf(arousal_level - optimal_arousal);
    float p = 1.0f - (distance * bridge->config.arousal_availability_scale);

    /* Apply precision modulation factor */
    p *= bridge->config.precision_modulation;

    /* Enforce bounds */
    if (p < HYPO_GW_FEP_BROADCAST_PRECISION_MIN) {
        p = HYPO_GW_FEP_BROADCAST_PRECISION_MIN;
    }
    if (p > 1.0f) p = 1.0f;

    *precision = p;
    return 0;
}

int hypo_gw_fep_compute_broadcast_priority(hypo_gw_fep_bridge_t* bridge,
    float drive_urgency, float* priority) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(priority, NIMCP_ERROR_NULL_POINTER, "priority is NULL");

    /* Drive urgency directly scales broadcast priority
     * Higher urgency = higher priority for workspace access */
    float p = drive_urgency * bridge->config.priority_broadcast_scale;

    /* Bound priority */
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    *priority = p;

    /* Track priority broadcasts */
    if (p > HYPO_GW_FEP_PRIORITY_THRESHOLD) {
        bridge->stats.priority_broadcasts++;
    }

    return 0;
}

/* ============================================================================
 * Event Reporting
 * ============================================================================ */

int hypo_gw_fep_report_broadcast_win(hypo_gw_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.broadcast_wins++;
    bridge->gw_effects.drive_in_broadcast = true;

    /* Reduce prediction error on successful broadcast */
    bridge->fep_effects.prediction_error *= 0.8f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_gw_fep_report_broadcast_loss(hypo_gw_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.broadcast_losses++;
    bridge->gw_effects.drive_in_broadcast = false;

    /* Increase prediction error on failed broadcast */
    float pe = bridge->fep_effects.prediction_error + 0.1f;
    if (pe > 1.0f) pe = 1.0f;
    bridge->fep_effects.prediction_error = pe;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int hypo_gw_fep_get_effects(const hypo_gw_fep_bridge_t* bridge,
    hypo_gw_fep_effects_t* effects) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");
    *effects = bridge->fep_effects;
    return 0;
}

int hypo_gw_fep_get_stats(const hypo_gw_fep_bridge_t* bridge,
    hypo_gw_fep_stats_t* stats) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(stats, NIMCP_ERROR_NULL_POINTER, "stats is NULL");
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int hypo_gw_fep_connect_bio_async(hypo_gw_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,
        .module_name = "hypo_gw_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo-GW FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_gw_fep_disconnect_bio_async(hypo_gw_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool hypo_gw_fep_is_bio_async_connected(const hypo_gw_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int hypo_gw_fep_process_messages(hypo_gw_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Message processing uses handler-based callbacks registered with bio_router_register_handler.
     * Handlers are invoked automatically when messages arrive - no polling needed here.
     * Future: Register handlers in connect_bio_async for specific message types. */

    return 0;
}

/* ============================================================================
 * Drive System Connection
 * ============================================================================ */

int hypo_gw_fep_connect_drives(hypo_gw_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->drive_system = drives;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo-GW FEP bridge connected to drive system");
    return 0;
}
