/**
 * @file nimcp_hypo_introspection_fep_bridge.c
 * @brief Implementation of Hypothalamus-Introspection FEP Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: FEP integration between hypothalamus interoception and introspection
 * WHY:  Interoception accuracy affects prediction error; body awareness generates FE
 * HOW:  Map interoceptive accuracy to PE, body awareness to free energy
 */

#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_introspection_fep_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hypo_introspection_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * Default Configuration
 * ============================================================================ */

int hypo_intro_fep_default_config(hypo_intro_fep_config_t* config) {
    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->drive_fe_weight = 1.0f;
    config->prediction_error_gain = 1.5f;
    config->precision_modulation = 1.0f;
    config->enable_active_inference = true;
    config->enable_bio_async = true;
    config->interoception_pe_scale = HYPO_INTRO_FEP_DEVIATION_PE_SCALE;
    config->body_awareness_fe_scale = HYPO_INTRO_FEP_AWARENESS_SCALE;
    config->deviation_surprise_scale = 2.0f;
    config->homeostatic_error_scale = HYPO_INTRO_FEP_HOMEOSTATIC_WEIGHT;

    return 0;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

hypo_intro_fep_bridge_t* hypo_intro_fep_create(
    const hypo_intro_fep_config_t* config,
    fep_system_t* fep_system) {

    if (!fep_system) {
        NIMCP_LOGGING_ERROR("Hypo-Intro FEP bridge: NULL FEP system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_intro_fep_default_config: fep_system is NULL");
        return NULL;
    }

    hypo_intro_fep_bridge_t* bridge = (hypo_intro_fep_bridge_t*)
        nimcp_malloc(sizeof(hypo_intro_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_intro_fep_create: allocation failed");

        return NULL;

    }

    memset(bridge, 0, sizeof(hypo_intro_fep_bridge_t));

    if (config) {
        bridge->config = *config;
    } else {
        hypo_intro_fep_default_config(&bridge->config);
    }

    bridge->fep_system = fep_system;
    if (bridge_base_init(&bridge->base, 0, "hypo_introspection_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_intro_fep_default_config: bridge->base is NULL");
        return NULL;
    }

    bridge->state.active = true;
    bridge->state.current_interoception_accuracy = 0.8f;
    bridge->fep_effects.precision = 1.0f;

    NIMCP_LOGGING_INFO("Hypo-Intro FEP bridge created");
    return bridge;
}

void hypo_intro_fep_destroy(hypo_intro_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.bio_async_enabled) {
        hypo_intro_fep_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int hypo_intro_fep_reset(hypo_intro_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    memset(&bridge->fep_effects, 0, sizeof(hypo_intro_fep_effects_t));
    memset(&bridge->intro_effects, 0, sizeof(intro_hypo_effects_t));
    memset(&bridge->state, 0, sizeof(hypo_intro_fep_state_t));
    memset(&bridge->stats, 0, sizeof(hypo_intro_fep_stats_t));

    bridge->state.active = true;
    bridge->state.current_interoception_accuracy = 0.8f;
    bridge->fep_effects.precision = 1.0f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Core Processing
 * ============================================================================ */

int hypo_intro_fep_update(hypo_intro_fep_bridge_t* bridge, uint64_t delta_ms) {
    NIMCP_FEP_CHECK_THROW(bridge && bridge->state.active, NIMCP_ERROR_NULL_POINTER, "bridge is NULL or inactive");
    (void)delta_ms; /* Currently unused but available for time-based updates */

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get current homeostatic state from drive system if connected */
    float homeostatic_error = bridge->state.current_homeostatic_error;
    if (bridge->drive_system) {
        /* Compute aggregate homeostatic error from all drives */
        float total_deviation = 0.0f;
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            hypo_drive_state_t drive_state;
            if (hypo_drive_get_state(bridge->drive_system, (hypo_drive_type_t)i, &drive_state)) {
                total_deviation += fabsf(drive_state.deviation);
            }
        }
        homeostatic_error = total_deviation / (float)HYPO_DRIVE_COUNT;
        bridge->state.current_homeostatic_error = homeostatic_error;
    }

    /* Compute prediction error from interoception accuracy */
    float interoception_accuracy = bridge->state.current_interoception_accuracy;
    float pe;
    hypo_intro_fep_compute_pe(bridge, interoception_accuracy, &pe);
    bridge->fep_effects.prediction_error = pe;
    bridge->fep_effects.interoceptive_pe = pe;

    /* Track interoception PE events */
    if (pe > HYPO_INTRO_FEP_ACCURACY_THRESHOLD * bridge->config.interoception_pe_scale) {
        bridge->stats.interoception_pe_events++;
    }

    /* Compute free energy from body awareness */
    float body_awareness = bridge->state.current_body_awareness;
    float fe;
    hypo_intro_fep_compute_fe(bridge, body_awareness, &fe);
    bridge->fep_effects.free_energy = fe;
    bridge->fep_effects.body_awareness_fe = fe;

    /* Track awareness FE spikes */
    if (fe > HYPO_INTRO_FEP_ACCURACY_THRESHOLD * bridge->config.body_awareness_fe_scale) {
        bridge->stats.awareness_fe_spikes++;
    }

    /* Modulate precision based on homeostatic state */
    float precision;
    hypo_intro_fep_modulate_precision(bridge, homeostatic_error, &precision);
    bridge->fep_effects.precision = precision;

    /* Compute metacognitive load */
    bridge->fep_effects.metacognitive_load =
        (1.0f - interoception_accuracy) * bridge->config.interoception_pe_scale;

    /* Add surprise from deviation if present */
    float deviation = bridge->state.current_deviation;
    if (deviation > 0.0f) {
        bridge->fep_effects.surprise = deviation * bridge->config.deviation_surprise_scale;
    }

    /* Active inference response */
    if (bridge->config.enable_active_inference) {
        float ai_strength = 0.0f;
        if (homeostatic_error > HYPO_INTRO_FEP_ACCURACY_THRESHOLD) {
            /* High homeostatic error: signal need for regulation */
            ai_strength = (homeostatic_error - HYPO_INTRO_FEP_ACCURACY_THRESHOLD) /
                         (1.0f - HYPO_INTRO_FEP_ACCURACY_THRESHOLD);
            ai_strength = ai_strength > 1.0f ? 1.0f : ai_strength;
            bridge->stats.active_inference_triggers++;
        }
        bridge->fep_effects.active_inference_strength = ai_strength;
    }

    /* Update reverse effects */
    bridge->intro_effects.self_model_predictions = precision;
    bridge->intro_effects.regulation_signal = bridge->fep_effects.active_inference_strength;
    bridge->intro_effects.monitoring_intensity = bridge->fep_effects.metacognitive_load;

    /* Update stats */
    bridge->state.update_count++;
    bridge->stats.total_updates++;
    float new_avg_fe = 0.95f * bridge->stats.avg_free_energy + 0.05f * fe;
    if (isfinite(new_avg_fe)) bridge->stats.avg_free_energy = new_avg_fe;
    float new_avg_pe = 0.95f * bridge->stats.avg_prediction_error + 0.05f * pe;
    if (isfinite(new_avg_pe)) bridge->stats.avg_prediction_error = new_avg_pe;
    float new_avg_ia = 0.95f * bridge->stats.avg_interoception_accuracy + 0.05f * interoception_accuracy;
    if (isfinite(new_avg_ia)) bridge->stats.avg_interoception_accuracy = new_avg_ia;
    float new_avg_ba = 0.95f * bridge->stats.avg_body_awareness + 0.05f * body_awareness;
    if (isfinite(new_avg_ba)) bridge->stats.avg_body_awareness = new_avg_ba;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_intro_fep_compute_fe(hypo_intro_fep_bridge_t* bridge,
    float body_awareness, float* free_energy) {

    NIMCP_FEP_CHECK_THROW(bridge && free_energy, NIMCP_ERROR_NULL_POINTER, "bridge or free_energy is NULL");

    /* Body awareness generates free energy
     * Higher awareness = more interoceptive processing = higher FE load */
    float fe = body_awareness * bridge->config.body_awareness_fe_scale *
               bridge->config.drive_fe_weight;

    /* Bound free energy */
    if (fe < 0.0f) fe = 0.0f;
    if (fe > 10.0f) fe = 10.0f;

    *free_energy = fe;
    bridge->state.current_body_awareness = body_awareness;

    return 0;
}

int hypo_intro_fep_compute_pe(hypo_intro_fep_bridge_t* bridge,
    float interoception_accuracy, float* prediction_error) {

    NIMCP_FEP_CHECK_THROW(bridge && prediction_error, NIMCP_ERROR_NULL_POINTER, "bridge or prediction_error is NULL");

    /* Interoception accuracy inversely maps to prediction error
     * Lower accuracy = higher PE (more mismatch between expected and actual) */
    float pe = (1.0f - interoception_accuracy) * bridge->config.interoception_pe_scale *
               bridge->config.prediction_error_gain;

    /* Bound prediction error */
    if (pe < 0.0f) pe = 0.0f;
    if (pe > 10.0f) pe = 10.0f;

    *prediction_error = pe;
    bridge->state.current_interoception_accuracy = interoception_accuracy;

    return 0;
}

int hypo_intro_fep_modulate_precision(hypo_intro_fep_bridge_t* bridge,
    float homeostatic_error, float* precision) {

    NIMCP_FEP_CHECK_THROW(bridge && precision, NIMCP_ERROR_NULL_POINTER, "bridge or precision is NULL");

    /* Homeostatic error reduces precision
     * Higher error = less reliable self-model = lower precision */
    float p = 1.0f - (homeostatic_error * bridge->config.homeostatic_error_scale);

    /* Apply precision modulation factor */
    p *= bridge->config.precision_modulation;

    /* Enforce bounds */
    if (p < 0.2f) p = 0.2f;
    if (p > 1.0f) p = 1.0f;

    *precision = p;
    return 0;
}

/* ============================================================================
 * Event Reporting
 * ============================================================================ */

int hypo_intro_fep_report_deviation(hypo_intro_fep_bridge_t* bridge,
    float deviation_magnitude) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->state.current_deviation = deviation_magnitude;
    bridge->state.surprise_events++;

    /* Compute surprise from deviation */
    float surprise = deviation_magnitude * bridge->config.deviation_surprise_scale;
    bridge->fep_effects.surprise = surprise;

    /* Increase prediction error from surprise */
    bridge->fep_effects.prediction_error += surprise * 0.1f;
    if (bridge->fep_effects.prediction_error > 10.0f) {
        bridge->fep_effects.prediction_error = 10.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hypo_intro_fep_update_interoception(hypo_intro_fep_bridge_t* bridge,
    float accuracy) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Bound accuracy */
    if (accuracy < 0.0f) accuracy = 0.0f;
    if (accuracy > 1.0f) accuracy = 1.0f;

    bridge->state.current_interoception_accuracy = accuracy;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * State Access
 * ============================================================================ */

int hypo_intro_fep_get_effects(const hypo_intro_fep_bridge_t* bridge,
    hypo_intro_fep_effects_t* effects) {

    NIMCP_FEP_CHECK_THROW(bridge && effects, NIMCP_ERROR_NULL_POINTER, "bridge or effects is NULL");
    *effects = bridge->fep_effects;
    return 0;
}

int hypo_intro_fep_get_stats(const hypo_intro_fep_bridge_t* bridge,
    hypo_intro_fep_stats_t* stats) {

    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    *stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int hypo_intro_fep_connect_bio_async(hypo_intro_fep_bridge_t* bridge) {
    if (!bridge || bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HYPOTHALAMUS,
        .module_name = "hypo_intro_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hypo-Intro FEP bridge connected to bio-async");
    }

    return 0;
}

int hypo_intro_fep_disconnect_bio_async(hypo_intro_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    bridge->base.bio_ctx = NULL;

    return 0;
}

bool hypo_intro_fep_is_bio_async_connected(const hypo_intro_fep_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

int hypo_intro_fep_process_messages(hypo_intro_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Message processing uses handler-based callbacks registered with bio_router_register_handler.
     * Handlers are invoked automatically when messages arrive - no polling needed here.
     * Future: Register handlers in connect_bio_async for specific message types. */

    return 0;
}

/* ============================================================================
 * Drive System Connection
 * ============================================================================ */

int hypo_intro_fep_connect_drives(hypo_intro_fep_bridge_t* bridge,
    hypo_drive_system_handle_t* drives) {

    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->drive_system = drives;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Hypo-Intro FEP bridge connected to drive system");
    return 0;
}
