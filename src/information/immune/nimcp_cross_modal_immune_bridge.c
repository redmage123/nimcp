/**
 * @file nimcp_cross_modal_immune_bridge.c
 * @brief Cross-Modal Integration-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "information/immune/nimcp_cross_modal_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cross_modal_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get current time in milliseconds
 *
 * WHAT: Get system time for duration tracking
 * WHY:  Track chronic inflammation and bottleneck duration
 * HOW:  Platform-specific time retrieval
 */
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief Get inflammation transfer efficiency factor
 *
 * WHAT: Map inflammation level to cross-modal transfer reduction
 * WHY:  Different inflammation levels have different integration impacts
 * HOW:  Return predefined factor based on level
 */
static float get_inflammation_transfer_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_TRANSFER_FACTOR,
        INFLAMMATION_STORM_TRANSFER_FACTOR);
}

/**
 * @brief Compute temporal binding window from inflammation
 *
 * WHAT: Calculate binding window duration based on inflammation level
 * WHY:  Inflammation narrows the temporal window for multi-sensory integration
 * HOW:  Base window (200ms) reduced by inflammation
 */
static float compute_binding_window_ms(brain_inflammation_level_t level) {
    const float BASE_WINDOW_MS = 200.0f;  /* Normal binding window */
    float narrowing = INFLAMMATION_WINDOW_BASE +
                     (level * INFLAMMATION_WINDOW_PER_LEVEL);
    narrowing = nimcp_clamp01(narrowing);
    return BASE_WINDOW_MS * (1.0f - narrowing);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int cross_modal_immune_default_config(cross_modal_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Enable all features by default */
    config->enable_cytokine_binding_impairment = true;
    config->enable_inflammation_transfer_reduction = true;
    config->enable_binding_failure_immune = true;
    config->enable_mismatch_detection = true;
    config->enable_bottleneck_stress_response = true;

    /* Default sensitivity (1.0 = normal) */
    config->cytokine_sensitivity = 1.0f;
    config->inflammation_sensitivity = 1.0f;
    config->mismatch_sensitivity = MISMATCH_SENSITIVITY;

    /* Default thresholds */
    config->binding_failure_threshold = BINDING_FAILURE_THRESHOLD;
    config->mutual_info_threshold = MUTUAL_INFO_COLLAPSE_THRESHOLD;
    config->bottleneck_threshold = BOTTLENECK_THRESHOLD;

    return 0;
}

cross_modal_immune_bridge_t* cross_modal_immune_create(
    const cross_modal_immune_config_t* config,
    brain_immune_system_t* immune_system,
    cross_modal_channel_t* cross_modal_channel
) {
    /* Guard: require immune system */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("cross_modal_immune_create: immune_system required");
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "cross_modal_immune_create: immune_system required");
        return NULL;
    }

    /* Allocate bridge */
    cross_modal_immune_bridge_t* bridge = nimcp_malloc(sizeof(cross_modal_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("cross_modal_immune_create: allocation failed");
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(cross_modal_immune_bridge_t),
                          "cross_modal_immune_create: Failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(cross_modal_immune_bridge_t));

    /* Apply configuration */
    if (config) {
        memcpy(&bridge->config, config, sizeof(cross_modal_immune_config_t));
    } else {
        cross_modal_immune_default_config(&bridge->config);
    }

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->cross_modal_channel = cross_modal_channel;

    /* Initialize timing */
    bridge->last_update_time = get_time_ms();

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "cross_modal_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("cross_modal_immune_create: mutex creation failed");
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "cross_modal_immune_create: mutex creation failed");
        /* Continue without mutex - non-fatal but log warning */
    }

    NIMCP_LOGGING_INFO("cross_modal_immune_bridge: created successfully");
    return bridge;
}

void cross_modal_immune_destroy(cross_modal_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        cross_modal_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → Cross-Modal API
 * ============================================================================ */

int cross_modal_immune_apply_cytokine_effects(cross_modal_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_apply_cytokine_effects: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_cytokine_binding_impairment) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(cross_modal_cytokine_effects_t));

    /* Compute cytokine-induced binding disruption from actual cytokine levels */
    bridge->cytokine_effects.il1_binding_disruption =
        stats.cytokine_il1 * CYTOKINE_IL1_BINDING_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.il6_binding_disruption =
        stats.cytokine_il6 * CYTOKINE_IL6_BINDING_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.tnf_binding_disruption =
        stats.cytokine_tnf * CYTOKINE_TNF_BINDING_IMPACT * bridge->config.cytokine_sensitivity;
    bridge->cytokine_effects.ifn_gamma_binding_disruption =
        stats.cytokine_ifn_gamma * CYTOKINE_IFN_GAMMA_BINDING_IMPACT * bridge->config.cytokine_sensitivity;

    /* Total binding impairment */
    bridge->cytokine_effects.total_binding_impairment = nimcp_clamp01(
        fabsf(bridge->cytokine_effects.il1_binding_disruption) +
        fabsf(bridge->cytokine_effects.il6_binding_disruption) +
        fabsf(bridge->cytokine_effects.tnf_binding_disruption) +
        fabsf(bridge->cytokine_effects.ifn_gamma_binding_disruption)
    );

    /* Transfer efficiency loss */
    bridge->cytokine_effects.transfer_efficiency_loss =
        bridge->cytokine_effects.total_binding_impairment * 0.75f;

    /* Mutual information degradation */
    bridge->cytokine_effects.mutual_info_degradation =
        bridge->cytokine_effects.total_binding_impairment * 0.8f;

    /* Temporal window narrowing */
    bridge->cytokine_effects.temporal_window_narrowing =
        bridge->cytokine_effects.total_binding_impairment * 0.6f;

    bridge->cytokine_impairments++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cross_modal_immune_apply_inflammation_effects(cross_modal_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_apply_inflammation_effects: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_inflammation_transfer_reduction) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    bridge->inflammation_state.current_level = level;

    /* Transfer efficiency factor based on inflammation */
    bridge->inflammation_state.transfer_efficiency_factor =
        get_inflammation_transfer_factor(level) * bridge->config.inflammation_sensitivity;

    /* Binding strength reduction */
    bridge->inflammation_state.binding_strength_reduction =
        1.0f - bridge->inflammation_state.transfer_efficiency_factor;

    /* Temporal binding window */
    bridge->inflammation_state.temporal_window_ms = compute_binding_window_ms(level);

    /* Enhancement suppression */
    bridge->inflammation_state.enhancement_suppression =
        (1.0f - bridge->inflammation_state.transfer_efficiency_factor) * 0.85f;

    /* Coherence degradation */
    bridge->inflammation_state.coherence_degradation =
        bridge->inflammation_state.binding_strength_reduction * 0.9f;

    /* Synchrony impairment */
    bridge->inflammation_state.synchrony_impairment =
        bridge->inflammation_state.binding_strength_reduction * 0.8f;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

float cross_modal_immune_compute_efficiency(const cross_modal_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;  /* Normal efficiency */
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_loss = bridge->cytokine_effects.transfer_efficiency_loss;
    float inflammation_factor = bridge->inflammation_state.transfer_efficiency_factor;

    /* Combined efficiency: multiplicative reduction */
    float efficiency = inflammation_factor * (1.0f - cytokine_loss);
    return nimcp_clamp01(efficiency);
}

float cross_modal_immune_compute_binding_window(const cross_modal_immune_bridge_t* bridge) {
    if (!bridge) {
        return 200.0f;  /* Normal 200ms window */
    }

    /* Get window from inflammation state */
    float window_ms = bridge->inflammation_state.temporal_window_ms;

    /* Further reduce by cytokine effects */
    float cytokine_narrowing = bridge->cytokine_effects.temporal_window_narrowing;
    window_ms *= (1.0f - cytokine_narrowing);

    return window_ms;
}

/* ============================================================================
 * Cross-Modal → Immune API
 * ============================================================================ */

int cross_modal_immune_detect_binding_failure(
    cross_modal_immune_bridge_t* bridge,
    float binding_strength
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_detect_binding_failure: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_binding_failure_immune) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_modulation.binding_strength = binding_strength;

    /* Detect binding failure */
    if (binding_strength < bridge->config.binding_failure_threshold) {
        bridge->immune_modulation.binding_failure_detected = true;
        bridge->immune_modulation.threat_signal_level =
            1.0f - (binding_strength / bridge->config.binding_failure_threshold);
        bridge->immune_modulation.immune_alert_level =
            bridge->immune_modulation.threat_signal_level * 0.6f;
        bridge->binding_failures++;
    } else {
        bridge->immune_modulation.binding_failure_detected = false;
        bridge->immune_modulation.threat_signal_level = 0.0f;
        bridge->immune_modulation.immune_alert_level = 0.0f;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cross_modal_immune_detect_mismatch(
    cross_modal_immune_bridge_t* bridge,
    float coherence
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_detect_mismatch: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_mismatch_detection) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Detect sensory mismatch (low coherence) */
    float mismatch_threshold = 0.5f;  /* Below this = mismatch */
    if (coherence < mismatch_threshold) {
        bridge->immune_modulation.sensory_mismatch_detected = true;
        bridge->mismatch_detections++;
        /* Could trigger immune response here */
    } else {
        bridge->immune_modulation.sensory_mismatch_detected = false;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cross_modal_immune_trigger_bottleneck_stress(
    cross_modal_immune_bridge_t* bridge,
    float efficiency
) {
    /* Guard clauses */
    if (!bridge || !bridge->immune_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_trigger_bottleneck_stress: bridge or immune_system is NULL");
        return -1;
    }

    if (!bridge->config.enable_bottleneck_stress_response) {
        return 0;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    bridge->immune_modulation.transfer_efficiency = efficiency;

    /* Detect bottleneck */
    if (efficiency < bridge->config.bottleneck_threshold) {
        bridge->immune_modulation.bottleneck_detected = true;
        bridge->immune_modulation.stress_inflammation_trigger = true;
        bridge->bottleneck_events++;
        /* Could trigger cytokine release here */
    } else {
        bridge->immune_modulation.bottleneck_detected = false;
        bridge->immune_modulation.stress_inflammation_trigger = false;

        /* Stable integration benefit */
        if (efficiency > 0.8f) {
            bridge->immune_modulation.stable_integration_benefit = (efficiency - 0.8f) / 0.2f;
        }
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int cross_modal_immune_update(
    cross_modal_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time = get_time_ms();
    bridge->total_updates++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);

    /* IMMUNE → CROSS-MODAL pathways */
    cross_modal_immune_apply_cytokine_effects(bridge);
    cross_modal_immune_apply_inflammation_effects(bridge);

    /* CROSS-MODAL → IMMUNE pathways */
    float current_binding = bridge->immune_modulation.binding_strength;
    float current_efficiency = bridge->immune_modulation.transfer_efficiency;
    float coherence = 1.0f - bridge->inflammation_state.coherence_degradation;

    cross_modal_immune_detect_binding_failure(bridge, current_binding);
    cross_modal_immune_detect_mismatch(bridge, coherence);
    cross_modal_immune_trigger_bottleneck_stress(bridge, current_efficiency);

    return 0;
}

int cross_modal_immune_apply_modulation(cross_modal_immune_bridge_t* bridge) {
    /* Guard clauses */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* If we have a cross_modal_channel, apply modulation to it */
    if (bridge->cross_modal_channel) {
        /* Would modify cross_modal_channel fields based on immune state */
        /* For example: adjust transfer_efficiency, mutual_information, etc. */
    }

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int cross_modal_immune_get_cytokine_effects(
    const cross_modal_immune_bridge_t* bridge,
    cross_modal_cytokine_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_get_cytokine_effects: bridge or effects is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->cytokine_effects, sizeof(cross_modal_cytokine_effects_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int cross_modal_immune_get_inflammation_state(
    const cross_modal_immune_bridge_t* bridge,
    cross_modal_inflammation_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_get_inflammation_state: bridge or state is NULL");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memcpy(state, &bridge->inflammation_state, sizeof(cross_modal_inflammation_state_t));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool cross_modal_immune_has_binding_deficit(const cross_modal_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    float efficiency = cross_modal_immune_compute_efficiency(bridge);
    return efficiency < 0.5f;  /* >50% binding reduction */
}

float cross_modal_immune_get_efficiency_factor(const cross_modal_immune_bridge_t* bridge) {
    return cross_modal_immune_compute_efficiency(bridge);
}

float cross_modal_immune_get_binding_strength(const cross_modal_immune_bridge_t* bridge) {
    if (!bridge) {
        return 1.0f;
    }
    return bridge->immune_modulation.binding_strength;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/** Module name for logging */
#define CROSS_MODAL_IMMUNE_MODULE_NAME "cross_modal_immune_bridge"

int cross_modal_immune_connect_bio_async(cross_modal_immune_bridge_t* bridge) {
    /**
     * WHAT: Register bridge with bio-async router
     * WHY:  Enable distributed immune signaling via NOREPINEPHRINE channel
     * HOW:  Use bio_router_register_module with immune module ID
     */

    /* Guard: null check */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("cross_modal_immune_connect_bio_async: NULL bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "cross_modal_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Guard: already connected */
    if (bridge->base.bio_async_enabled) {
        NIMCP_LOGGING_WARN("cross_modal_immune_bridge: Already connected to bio-async");
        return 0;
    }

    /* Build module info */
    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_CROSS_MODAL,
        .module_name = CROSS_MODAL_IMMUNE_MODULE_NAME,
        .inbox_capacity = 32,
        .user_data = bridge
    };

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Register with router */
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("cross_modal_immune_bridge: Connected to bio-async router");
    } else {
        NIMCP_LOGGING_WARN("cross_modal_immune_bridge: Bio-async router not available, skipping registration");
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int cross_modal_immune_disconnect_bio_async(cross_modal_immune_bridge_t* bridge) {
    /**
     * WHAT: Unregister bridge from bio-async router
     * WHY:  Clean shutdown of messaging infrastructure
     * HOW:  Use bio_router_unregister_module
     */

    /* Guard: null check */
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Guard: not connected */
    if (!bridge->base.bio_async_enabled || !bridge->base.bio_ctx) {
        return 0;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Unregister */
    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("cross_modal_immune_bridge: Disconnected from bio-async router");

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool cross_modal_immune_is_bio_async_connected(const cross_modal_immune_bridge_t* bridge) {
    /**
     * WHAT: Check bio-async connection status
     * WHY:  Allow callers to verify messaging capability
     * HOW:  Return internal flag
     */
    if (!bridge) {
        return false;
    }
    return bridge->base.bio_async_enabled;
}
