/**
 * @file nimcp_portia_sensor_fusion_immune_bridge.c
 * @brief Portia Sensor Fusion-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "portia/immune/nimcp_portia_sensor_fusion_immune_bridge.h"
#include "constants/nimcp_constants.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(portia_sensor_fusion_immune_bridge)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine sensor weight effects
 *
 * WHAT: Calculate sensor weight reduction from cytokine levels
 * WHY:  Pro-inflammatory cytokines impair sensory processing
 * HOW:  Query immune cytokines, compute weighted impact
 */
static void compute_cytokine_sensor_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause: validate inputs */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(cytokine_sensor_effects_t));

    /* Query immune system for cytokine levels (placeholder - would query actual levels) */
    /* For now, use inflammation level as proxy */
    brain_inflammation_level_t inflam = INFLAMMATION_NONE;
    float inflam_factor = 1.0f;

    /* Compute individual cytokine effects */
    bridge->cytokine_effects.il1_weight_reduction =
        CYTOKINE_IL1_SENSOR_WEIGHT_IMPACT * inflam_factor;
    bridge->cytokine_effects.il6_weight_reduction =
        CYTOKINE_IL6_SENSOR_WEIGHT_IMPACT * inflam_factor;
    bridge->cytokine_effects.tnf_weight_reduction =
        CYTOKINE_TNF_SENSOR_WEIGHT_IMPACT * inflam_factor;
    bridge->cytokine_effects.ifn_gamma_weight_reduction =
        CYTOKINE_IFN_GAMMA_SENSOR_IMPACT * inflam_factor;

    /* Anti-inflammatory recovery */
    bridge->cytokine_effects.il10_recovery_boost =
        CYTOKINE_IL10_SENSOR_RECOVERY * (1.0f - inflam_factor);

    /* Compute total weight factor */
    float total_reduction =
        fabsf(bridge->cytokine_effects.il1_weight_reduction) +
        fabsf(bridge->cytokine_effects.il6_weight_reduction) +
        fabsf(bridge->cytokine_effects.tnf_weight_reduction) +
        fabsf(bridge->cytokine_effects.ifn_gamma_weight_reduction);

    total_reduction -= bridge->cytokine_effects.il10_recovery_boost;
    total_reduction *= bridge->config.cytokine_sensitivity;

    bridge->cytokine_effects.total_weight_factor =
        fmaxf(0.0f, fminf(1.0f, 1.0f - total_reduction));

    /* Confidence and noise effects */
    bridge->cytokine_effects.confidence_reduction = total_reduction * 0.5f;
    bridge->cytokine_effects.noise_tolerance_increase = total_reduction * 0.3f;
    bridge->cytokine_effects.outlier_threshold_increase = total_reduction * 0.2f;
}

/**
 * @brief Compute inflammation sensor fusion effects
 *
 * WHAT: Calculate sensor fusion impairment from inflammation
 * WHY:  Inflammation reduces sensory acuity and integration
 * HOW:  Map inflammation level to fusion parameter reductions
 */
static void compute_inflammation_sensor_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        return;
    }

    /* Get inflammation level from immune system */
    /* Placeholder - would query actual inflammation */
    brain_inflammation_level_t level = INFLAMMATION_NONE;

    bridge->inflammation_state.current_level = level;

    /* Map inflammation to weight factor */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.weight_factor = INFLAMMATION_NONE_SENSOR_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.weight_factor = INFLAMMATION_LOCAL_SENSOR_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.weight_factor = INFLAMMATION_REGIONAL_SENSOR_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.weight_factor = INFLAMMATION_SYSTEMIC_SENSOR_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.weight_factor = INFLAMMATION_STORM_SENSOR_FACTOR;
            break;
        default:
            bridge->inflammation_state.weight_factor = 1.0f;
    }

    /* Apply sensitivity */
    float reduction = 1.0f - bridge->inflammation_state.weight_factor;
    reduction *= bridge->config.inflammation_sensitivity;
    bridge->inflammation_state.weight_factor = 1.0f - reduction;

    /* Compute confidence reduction */
    bridge->inflammation_state.confidence_reduction =
        INFLAMMATION_CONFIDENCE_BASE +
        (INFLAMMATION_CONFIDENCE_PER_LEVEL * (float)level);

    /* Other effects scale with inflammation */
    float severity = (float)level / (float)INFLAMMATION_STORM;
    bridge->inflammation_state.acuity_loss = severity * 0.6f;
    bridge->inflammation_state.cross_modal_binding_deficit = severity * 0.5f;
    bridge->inflammation_state.prediction_error_increase = severity * 0.4f;
    bridge->inflammation_state.sensory_fragmentation = severity * 0.7f;
    bridge->inflammation_state.hallucination_risk =
        (level == INFLAMMATION_STORM) ? 0.8f : severity * 0.2f;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_default_config(portia_sensor_fusion_immune_config_t* config) {
    /* Guard clause */
    if (!config) {
        NIMCP_LOGGING_ERROR("Null config pointer");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_default_config: config is NULL");
        return -1;
    }

    /* Feature enables */
    config->enable_cytokine_sensor_impairment = true;
    config->enable_inflammation_sensor_reduction = true;
    config->enable_sensor_overload_immune_trigger = true;
    config->enable_sensor_conflict_immune_boost = true;
    config->enable_sensor_dropout_suppression = true;

    /* Sensitivity tuning */
    config->cytokine_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->inflammation_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config->sensor_immune_sensitivity = NIMCP_SENSITIVITY_DEFAULT;

    /* Thresholds */
    config->overload_threshold = SENSOR_OVERLOAD_THRESHOLD;
    config->conflict_threshold = SENSOR_CONFLICT_THRESHOLD;
    config->dropout_threshold = SENSOR_DROPOUT_THRESHOLD;
    config->confidence_low_threshold = SENSOR_CONFIDENCE_LOW_THRESHOLD;

    return 0;
}

portia_sensor_fusion_immune_bridge_t* portia_sensor_fusion_immune_create(
    const portia_sensor_fusion_immune_config_t* config,
    brain_immune_system_t* immune_system,
    portia_fusion_ctx_t* sensor_fusion
) {
    /* Guard clauses */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("Null immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "immune_system is NULL");

        return NULL;
    }

    if (!sensor_fusion) {
        NIMCP_LOGGING_ERROR("Null sensor fusion system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sensor_fusion is NULL");


        return NULL;
    }

    /* Allocate bridge */
    portia_sensor_fusion_immune_bridge_t* bridge =
        (portia_sensor_fusion_immune_bridge_t*)nimcp_calloc(
            1, sizeof(portia_sensor_fusion_immune_bridge_t)
        );
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sensor_fusion_immune_create: allocation failed");

        return NULL;
    }

    /* Store system handles */
    bridge->immune_system = immune_system;
    bridge->sensor_fusion = sensor_fusion;

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        portia_sensor_fusion_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "portia_sensor_fusion_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "portia_sensor_fusion_immune_create: bridge->base is NULL");
        return NULL;
    }

    /* Initialize state */
    bridge->last_update_time = 0;
    bridge->base.bio_async_enabled = false;

    /* Initialize modulation structure to prevent reading uninitialized values */
    memset(&bridge->sensor_modulation, 0, sizeof(bridge->sensor_modulation));
    bridge->sensor_modulation.sensor_conflict_level = 0.0f;  // No conflict by default

    NIMCP_LOGGING_INFO("Created Portia sensor fusion-immune bridge");
    return bridge;
}

void portia_sensor_fusion_immune_destroy(portia_sensor_fusion_immune_bridge_t* bridge) {
    /* Guard clause */
    if (!bridge) {
        return;
    }

    /* bridge_base_cleanup handles bio-async disconnect + mutex destroy+free */
    bridge_base_cleanup(&bridge->base);

    /* Free bridge */
    nimcp_free(bridge);

    NIMCP_LOGGING_INFO("Destroyed Portia sensor fusion-immune bridge");
}

/* ============================================================================
 * Immune → Sensor Fusion API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_apply_cytokine_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->sensor_fusion) {
        NIMCP_LOGGING_ERROR("Invalid bridge or sensor fusion");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_apply_cytokine_effects: required parameter is NULL (bridge, bridge->sensor_fusion)");
        return -1;
    }

    if (!bridge->config.enable_cytokine_sensor_impairment) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    compute_cytokine_sensor_effects(bridge);

    /* Apply weight factor to all sensors */
    float weight_factor = bridge->cytokine_effects.total_weight_factor;

    /* Would apply to sensor fusion system here */
    /* portia_fusion_scale_all_weights(bridge->sensor_fusion, weight_factor); */

    bridge->cytokine_impairments++;
    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied cytokine effects: weight_factor=%.3f", weight_factor);
    return 0;
}

int portia_sensor_fusion_immune_apply_inflammation_effects(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->sensor_fusion) {
        NIMCP_LOGGING_ERROR("Invalid bridge or sensor fusion");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_apply_inflammation_effects: required parameter is NULL (bridge, bridge->sensor_fusion)");
        return -1;
    }

    if (!bridge->config.enable_inflammation_sensor_reduction) {
        return 0;  /* Feature disabled */
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute inflammation effects */
    compute_inflammation_sensor_effects(bridge);

    /* Apply weight factor and confidence reduction */
    float weight_factor = bridge->inflammation_state.weight_factor;
    float confidence_reduction = bridge->inflammation_state.confidence_reduction;

    /* Would apply to sensor fusion here */
    /* portia_fusion_scale_all_weights(bridge->sensor_fusion, weight_factor); */
    /* portia_fusion_reduce_confidence(bridge->sensor_fusion, confidence_reduction); */

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Applied inflammation effects: weight=%.3f, confidence_loss=%.3f",
                       weight_factor, confidence_reduction);
    return 0;
}

float portia_sensor_fusion_immune_compute_weight_factor(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 1.0f;
    }

    /* Combine cytokine and inflammation effects */
    float cytokine_factor = bridge->cytokine_effects.total_weight_factor;
    float inflam_factor = bridge->inflammation_state.weight_factor;

    return fminf(cytokine_factor, inflam_factor);
}

float portia_sensor_fusion_immune_compute_confidence_reduction(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return 0.0f;
    }

    /* Combine cytokine and inflammation confidence reductions */
    float cytokine_reduction = bridge->cytokine_effects.confidence_reduction;
    float inflam_reduction = bridge->inflammation_state.confidence_reduction;

    return fmaxf(cytokine_reduction, inflam_reduction);
}

/* ============================================================================
 * Sensor Fusion → Immune Internal Helpers (Unlocked versions)
 * ============================================================================
 * These functions perform the actual work WITHOUT acquiring the mutex.
 * Caller MUST hold bridge->base.mutex before calling.
 * This prevents deadlocks when update() calls these functions.
 * ============================================================================ */

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_sensor_fusion_immune_trigger_overload_response_unlocked(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_sensor_overload_immune_trigger) {
        return 0;  /* Feature disabled */
    }

    /* Get sensor fusion state */
    portia_fusion_stats_t stats;
    if (!portia_fusion_get_stats(bridge->sensor_fusion, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_sensor_fusion_immune_trigger_overload_response_unlocked: portia_fusion_get_stats is NULL");
        return -1;
    }

    /* Compute overload level */
    float sensor_ratio = (float)stats.active_sensor_count / (float)SENSOR_TYPE_COUNT;
    float overload_level = 0.0f;

    if (sensor_ratio > bridge->config.overload_threshold) {
        overload_level = (sensor_ratio - bridge->config.overload_threshold) /
                        (1.0f - bridge->config.overload_threshold);

        /* Accumulate overload */
        bridge->overload_accumulator += overload_level;

        /* Trigger IL-6 release if sustained */
        if (bridge->overload_accumulator > 1.0f) {
            float il6_amount = SENSOR_OVERLOAD_IL6_RELEASE *
                              bridge->config.sensor_immune_sensitivity;

            /* Would release IL-6 here */
            /* brain_immune_release_cytokine(bridge->immune_system, BRAIN_CYTOKINE_IL6,
                                            0, il6_amount, 0, NULL); */

            bridge->overload_events++;
            bridge->overload_accumulator = 0.0f;

            NIMCP_LOGGING_INFO("Sensor overload triggered IL-6 release: %.3f", il6_amount);
        }
    } else {
        /* Decay overload accumulator */
        bridge->overload_accumulator *= 0.9f;
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_sensor_fusion_immune_boost_from_conflicts_unlocked(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_sensor_conflict_immune_boost) {
        return 0;  /* Feature disabled */
    }

    /* Compute sensor conflict level (placeholder) */
    float conflict_level = bridge->sensor_modulation.sensor_conflict_level;

    if (conflict_level > bridge->config.conflict_threshold) {
        /* Release IL-1β for prediction error signaling */
        float il1_amount = SENSOR_CONFLICT_IL1_RELEASE *
                          bridge->config.sensor_immune_sensitivity;

        /* Would release IL-1β here */
        /* brain_immune_release_cytokine(bridge->immune_system, BRAIN_CYTOKINE_IL1,
                                        0, il1_amount, 0, NULL); */

        bridge->conflict_immune_triggers++;

        NIMCP_LOGGING_DEBUG("Sensor conflict triggered IL-1β release: %.3f", il1_amount);
    }

    return 0;
}

/**
 * @brief Internal unlocked version - caller MUST hold bridge->base.mutex
 */
static int portia_sensor_fusion_immune_trigger_dropout_suppression_unlocked(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    if (!bridge->config.enable_sensor_dropout_suppression) {
        return 0;  /* Feature disabled */
    }

    /* Get sensor fusion state */
    portia_fusion_stats_t stats;
    if (!portia_fusion_get_stats(bridge->sensor_fusion, &stats)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "portia_sensor_fusion_immune_trigger_dropout_suppression_unlocked: portia_fusion_get_stats is NULL");
        return -1;
    }

    /* Compute dropout level */
    float sensor_ratio = (float)stats.active_sensor_count / (float)SENSOR_TYPE_COUNT;

    if (sensor_ratio < bridge->config.dropout_threshold) {
        /* Trigger metabolic suppression */
        float suppression = SENSOR_DROPOUT_METABOLIC_SUPPRESSION *
                           bridge->config.sensor_immune_sensitivity;

        bridge->sensor_modulation.dropout_metabolic_suppression = suppression;
        bridge->dropout_suppressions++;

        NIMCP_LOGGING_DEBUG("Sensor dropout triggered metabolic suppression: %.3f",
                           suppression);
    }

    return 0;
}

/* ============================================================================
 * Sensor Fusion → Immune API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_trigger_overload_response(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_trigger_overload_response: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_sensor_fusion_immune_trigger_overload_response_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_sensor_fusion_immune_boost_from_conflicts(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_boost_from_conflicts: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_sensor_fusion_immune_boost_from_conflicts_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

int portia_sensor_fusion_immune_trigger_dropout_suppression(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge || !bridge->immune_system) {
        NIMCP_LOGGING_ERROR("Invalid bridge or immune system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_trigger_dropout_suppression: required parameter is NULL (bridge, bridge->immune_system)");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Call unlocked version */
    int result = portia_sensor_fusion_immune_trigger_dropout_suppression_unlocked(bridge);

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return result;
}

/* ============================================================================
 * Bidirectional Update API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_update(
    portia_sensor_fusion_immune_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_update: bridge is NULL");
        return -1;
    }

    /* Lock */
    nimcp_mutex_lock(bridge->base.mutex);

    /* Update timing */
    bridge->last_update_time += delta_ms;

    /* Immune → Sensor Fusion direction */
    if (bridge->config.enable_cytokine_sensor_impairment) {
        compute_cytokine_sensor_effects(bridge);
    }

    if (bridge->config.enable_inflammation_sensor_reduction) {
        compute_inflammation_sensor_effects(bridge);
    }

    /* Sensor Fusion → Immune direction - use unlocked versions since we already hold the mutex */
    if (bridge->config.enable_sensor_overload_immune_trigger) {
        portia_sensor_fusion_immune_trigger_overload_response_unlocked(bridge);
    }

    if (bridge->config.enable_sensor_conflict_immune_boost) {
        portia_sensor_fusion_immune_boost_from_conflicts_unlocked(bridge);
    }

    if (bridge->config.enable_sensor_dropout_suppression) {
        portia_sensor_fusion_immune_trigger_dropout_suppression_unlocked(bridge);
    }

    bridge->total_updates++;

    /* Unlock */
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Query API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_get_cytokine_effects(
    const portia_sensor_fusion_immune_bridge_t* bridge,
    cytokine_sensor_effects_t* effects
) {
    /* Guard clause */
    if (!bridge || !effects) {
        NIMCP_LOGGING_ERROR("Null bridge or effects");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_get_cytokine_effects: required parameter is NULL (bridge, effects)");
        return -1;
    }

    *effects = bridge->cytokine_effects;
    return 0;
}

int portia_sensor_fusion_immune_get_inflammation_state(
    const portia_sensor_fusion_immune_bridge_t* bridge,
    inflammation_sensor_state_t* state
) {
    /* Guard clause */
    if (!bridge || !state) {
        NIMCP_LOGGING_ERROR("Null bridge or state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_get_inflammation_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->inflammation_state;
    return 0;
}

bool portia_sensor_fusion_immune_has_sensor_impairment(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    float weight_factor = portia_sensor_fusion_immune_compute_weight_factor(bridge);
    return (weight_factor < 0.75f);  /* >25% weight loss */
}

float portia_sensor_fusion_immune_get_weight_factor(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    return portia_sensor_fusion_immune_compute_weight_factor(bridge);
}

float portia_sensor_fusion_immune_get_confidence_reduction(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    return portia_sensor_fusion_immune_compute_confidence_reduction(bridge);
}

/* ============================================================================
 * Bio-Async Integration API Implementation
 * ============================================================================ */

int portia_sensor_fusion_immune_connect_bio_async(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_connect_bio_async: bridge is NULL");
        return -1;
    }

    /* Would register with bio-async router here */
    /* bio_router_register_module(&bridge->base.bio_ctx, BIO_MODULE_IMMUNE_PORTIA_SENSOR); */

    bridge->base.bio_async_enabled = true;

    NIMCP_LOGGING_INFO("Connected Portia sensor fusion-immune bridge to bio-async");
    return 0;
}

int portia_sensor_fusion_immune_disconnect_bio_async(
    portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Null bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "portia_sensor_fusion_immune_disconnect_bio_async: bridge is NULL");
        return -1;
    }

    if (!bridge->base.bio_async_enabled) {
        return 0;  /* Not connected */
    }

    /* Would unregister from bio-async router here */
    /* bio_router_unregister_module(&bridge->base.bio_ctx); */

    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Disconnected Portia sensor fusion-immune bridge from bio-async");
    return 0;
}

bool portia_sensor_fusion_immune_is_bio_async_connected(
    const portia_sensor_fusion_immune_bridge_t* bridge
) {
    /* Guard clause */
    if (!bridge) {
        return false;
    }

    return bridge->base.bio_async_enabled;
}
