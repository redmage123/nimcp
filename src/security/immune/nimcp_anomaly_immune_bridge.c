/**
 * @file nimcp_anomaly_immune_bridge.c
 * @brief Anomaly Detector-Immune System Integration Bridge Implementation
 */

#include "security/immune/nimcp_anomaly_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(anomaly_immune_bridge, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Compute cytokine effects on thresholds
 *
 * WHAT: Calculate threshold modulation from cytokine levels
 * WHY:  Inflammation affects detection sensitivity
 * HOW:  Sum individual cytokine impacts
 */
static void compute_cytokine_effects(
    anomaly_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(anomaly_cytokine_effects_t));

    /* Get immune statistics for cytokine levels (placeholder - would read actual levels) */
    brain_immune_stats_t stats;
    if (brain_immune_get_stats((brain_immune_system_t*)immune, &stats) != 0) {
        return;
    }

    /* Compute individual cytokine effects (placeholder - normalized impact) */
    bridge->cytokine_effects.il1_threshold_reduction = CYTOKINE_IL1_THRESHOLD_IMPACT;
    bridge->cytokine_effects.il6_threshold_reduction = CYTOKINE_IL6_THRESHOLD_IMPACT;
    bridge->cytokine_effects.tnf_threshold_reduction = CYTOKINE_TNF_THRESHOLD_IMPACT;
    bridge->cytokine_effects.ifn_gamma_threshold_reduction = CYTOKINE_IFN_GAMMA_THRESHOLD_IMPACT;
    bridge->cytokine_effects.il10_threshold_increase = CYTOKINE_IL10_THRESHOLD_IMPACT;

    /* Aggregate total modulation */
    bridge->cytokine_effects.total_threshold_modulation =
        bridge->cytokine_effects.il1_threshold_reduction +
        bridge->cytokine_effects.il6_threshold_reduction +
        bridge->cytokine_effects.tnf_threshold_reduction +
        bridge->cytokine_effects.ifn_gamma_threshold_reduction +
        bridge->cytokine_effects.il10_threshold_increase;

    /* Compute effective threshold */
    bridge->cytokine_effects.effective_threshold =
        bridge->config.base_overall_threshold *
        (1.0f + bridge->cytokine_effects.total_threshold_modulation);

    /* Paranoid mode if very low threshold */
    bridge->cytokine_effects.paranoid_mode =
        (bridge->cytokine_effects.effective_threshold < 0.3f);
}

/**
 * @brief Compute inflammation effects
 */
static void compute_inflammation_effects(
    anomaly_immune_bridge_t* bridge,
    const brain_immune_system_t* immune
) {
    if (!bridge || !immune) return;

    /* Get current inflammation level */
    brain_immune_phase_t phase = brain_immune_get_phase((brain_immune_system_t*)immune);

    /* Map phase to inflammation (simplified) */
    brain_inflammation_level_t level = INFLAMMATION_NONE;
    if (phase >= IMMUNE_PHASE_ACTIVATION) {
        level = INFLAMMATION_LOCAL;
    }
    if (phase >= IMMUNE_PHASE_EFFECTOR) {
        level = INFLAMMATION_REGIONAL;
    }

    bridge->inflammation_state.current_level = level;

    /* Set threshold factor based on level */
    switch (level) {
        case INFLAMMATION_NONE:
            bridge->inflammation_state.threshold_factor = INFLAMMATION_NONE_THRESHOLD_FACTOR;
            break;
        case INFLAMMATION_LOCAL:
            bridge->inflammation_state.threshold_factor = INFLAMMATION_LOCAL_THRESHOLD_FACTOR;
            break;
        case INFLAMMATION_REGIONAL:
            bridge->inflammation_state.threshold_factor = INFLAMMATION_REGIONAL_THRESHOLD_FACTOR;
            break;
        case INFLAMMATION_SYSTEMIC:
            bridge->inflammation_state.threshold_factor = INFLAMMATION_SYSTEMIC_THRESHOLD_FACTOR;
            break;
        case INFLAMMATION_STORM:
            bridge->inflammation_state.threshold_factor = INFLAMMATION_STORM_THRESHOLD_FACTOR;
            break;
    }

    /* Compute sensitivity boost */
    bridge->inflammation_state.sensitivity_boost =
        1.0f - bridge->inflammation_state.threshold_factor;

    /* Set mode flags */
    bridge->inflammation_state.hypervigilant_mode =
        (level >= INFLAMMATION_SYSTEMIC);
    bridge->inflammation_state.emergency_mode =
        (level == INFLAMMATION_STORM);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int anomaly_immune_default_config(anomaly_immune_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    memset(config, 0, sizeof(anomaly_immune_config_t));

    /* Feature enables */
    config->enable_cytokine_threshold_modulation = true;
    config->enable_inflammation_sensitivity_boost = true;
    config->enable_anomaly_antigen_presentation = true;
    config->enable_immune_training_feedback = true;
    config->enable_auto_inflammation_trigger = true;

    /* Threshold config */
    config->base_content_threshold = 0.7f;
    config->base_behavior_threshold = 0.7f;
    config->base_overall_threshold = 0.7f;
    config->paranoid_mode_multiplier = 0.5f;

    /* Antigen presentation config */
    config->min_score_for_antigen = ANOMALY_ANTIGEN_PRESENTATION_THRESHOLD;
    config->severity_multiplier = ANOMALY_SEVERITY_MULTIPLIER;

    /* Training feedback */
    config->auto_train_from_neutralization = true;
    config->auto_train_from_false_positives = true;

    return 0;
}

anomaly_immune_bridge_t* anomaly_immune_create(
    const anomaly_immune_config_t* config,
    nimcp_anomaly_detector_t anomaly_detector,
    brain_immune_system_t* immune_system
) {
    /* Guard clauses */
    if (!anomaly_detector || !immune_system) {
        NIMCP_LOGGING_ERROR("Invalid parameters for anomaly immune bridge creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_immune_create: required parameter is NULL (anomaly_detector, immune_system)");
        return NULL;
    }

    /* Allocate bridge */
    anomaly_immune_bridge_t* bridge = (anomaly_immune_bridge_t*)
        nimcp_malloc(sizeof(anomaly_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate anomaly immune bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anomaly_immune_create: failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(anomaly_immune_bridge_t));

    /* Set handles */
    bridge->anomaly_detector = anomaly_detector;
    bridge->immune_system = immune_system;

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        anomaly_immune_default_config(&bridge->config);
    }

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "anomaly_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex for anomaly immune bridge");
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "anomaly_immune_create: bridge->base is NULL");
        return NULL;
    }

    NIMCP_LOGGING_INFO("Created anomaly immune bridge");
    return bridge;
}

void anomaly_immune_destroy(anomaly_immune_bridge_t* bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        anomaly_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOGGING_INFO("Destroyed anomaly immune bridge");
}

/* ============================================================================
 * Update and Modulation API
 * ============================================================================ */

int anomaly_immune_update(anomaly_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute cytokine effects */
    if (bridge->config.enable_cytokine_threshold_modulation) {
        compute_cytokine_effects(bridge, bridge->immune_system);
        bridge->threshold_modulations++;
    }

    /* Compute inflammation effects */
    if (bridge->config.enable_inflammation_sensitivity_boost) {
        compute_inflammation_effects(bridge, bridge->immune_system);
    }

    bridge->total_updates++;
    bridge->last_update_time = 0; /* Would use actual timestamp */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_immune_apply_modulation(anomaly_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Compute effective threshold combining cytokine and inflammation effects */
    float effective_threshold = bridge->config.base_overall_threshold;

    effective_threshold *= bridge->inflammation_state.threshold_factor;
    effective_threshold *= (1.0f + bridge->cytokine_effects.total_threshold_modulation);

    /* Clamp to reasonable range */
    if (effective_threshold < 0.1f) effective_threshold = 0.1f;
    if (effective_threshold > 1.0f) effective_threshold = 1.0f;

    bridge->cytokine_effects.effective_threshold = effective_threshold;

    /* Would update anomaly detector thresholds here */
    /* nimcp_anomaly_detector_set_threshold(bridge->anomaly_detector, effective_threshold); */

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int anomaly_immune_present_anomaly(
    anomaly_immune_bridge_t* bridge,
    const nimcp_anomaly_result_t* result,
    uint32_t* antigen_id
) {
    if (!bridge || !result || !antigen_id) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "anomaly_immune_present_anomaly: required parameter is NULL (bridge, result, antigen_id)");
        return -1;
    }
    if (!bridge->config.enable_anomaly_antigen_presentation) return 0;

    /* Check if score is high enough */
    if (result->anomaly_score < bridge->config.min_score_for_antigen) {
        return 0; /* Not presented, but not an error */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Map anomaly score to severity */
    uint32_t severity = (uint32_t)(result->anomaly_score * bridge->config.severity_multiplier);
    if (severity > 10) severity = 10;

    /* Create epitope from triggered features (simplified) */
    uint8_t epitope[32];
    memset(epitope, 0, sizeof(epitope));
    uint32_t* features_ptr = (uint32_t*)epitope;
    *features_ptr = result->triggered_features;

    /* Present to immune system */
    int ret = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope,
        sizeof(epitope),
        severity,
        0, /* source node */
        antigen_id
    );

    if (ret == 0) {
        bridge->antigens_presented++;
        bridge->immune_modulation.antigens_presented++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return ret;
}

int anomaly_immune_training_feedback(
    anomaly_immune_bridge_t* bridge,
    uint32_t antigen_id,
    bool was_neutralized
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (!bridge->config.enable_immune_training_feedback) return 0;

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Would train anomaly detector here based on feedback */
    /* nimcp_anomaly_train(bridge->anomaly_detector, input, len, was_neutralized); */

    bridge->training_updates++;

    if (was_neutralized) {
        bridge->immune_modulation.true_positives++;
    } else {
        bridge->immune_modulation.false_positives++;
    }

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int anomaly_immune_connect_bio_async(anomaly_immune_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NIMCP_ERROR_NULL_POINTER;

    }
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_ANOMALY,
        .module_name = "anomaly_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Anomaly immune bridge connected to bio-async router");
    }

    return bridge->base.bio_ctx ? 0 : -1;
}

int anomaly_immune_disconnect_bio_async(anomaly_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return -1;
    }

    bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    NIMCP_LOGGING_INFO("Anomaly immune bridge disconnected from bio-async router");
    return 0;
}

bool anomaly_immune_is_bio_async_connected(const anomaly_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

float anomaly_immune_get_effective_threshold(const anomaly_immune_bridge_t* bridge) {
    return bridge ? bridge->cytokine_effects.effective_threshold : 1.0f;
}

bool anomaly_immune_is_paranoid_mode(const anomaly_immune_bridge_t* bridge) {
    return bridge ? bridge->cytokine_effects.paranoid_mode : false;
}
