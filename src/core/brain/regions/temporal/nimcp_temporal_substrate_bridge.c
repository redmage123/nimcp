/**
 * @file nimcp_temporal_substrate_bridge.c
 * @brief Implementation of temporal cortex-substrate bridge
 *
 * WHAT: Links auditory/object/semantic processing to metabolic state
 * WHY:  Perception and memory require sustained neural activation
 * HOW:  Monitors ATP/fatigue; modulates recognition accuracy, memory retrieval
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/temporal/nimcp_temporal_substrate_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "TEMPORAL_SUBSTRATE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(temporal_substrate_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct temporal_substrate_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* temporal;                      /**< Temporal adapter handle */
    neural_substrate_t* substrate;       /**< Neural substrate handle */
    temporal_substrate_config_t config;  /**< Configuration */
    temporal_substrate_effects_t effects; /**< Current effects */
    temporal_substrate_stats_t stats;    /**< Statistics */

    /* Cached substrate values */
    float last_atp_level;
    float last_fatigue_level;
};

/*=============================================================================
 * Configuration API
 *===========================================================================*/

temporal_substrate_config_t temporal_substrate_default_config(void) {
    temporal_substrate_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_atp_modulation = true;
    config.enable_fatigue_modulation = true;
    config.enable_bio_async = true;
    config.atp_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.fatigue_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    config.min_capacity = 0.2f;
    config.auditory_atp_weight = 1.0f;
    config.semantic_fatigue_weight = 1.0f;

    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

temporal_substrate_bridge_t* temporal_substrate_bridge_create(
    void* temporal,
    neural_substrate_t* substrate,
    const temporal_substrate_config_t* config
) {
    temporal_substrate_bridge_t* bridge = (temporal_substrate_bridge_t*)nimcp_calloc(
        1, sizeof(temporal_substrate_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    bridge->temporal = temporal;
    bridge->substrate = substrate;
    bridge->config = config ? *config : temporal_substrate_default_config();

    /* Initialize effects to full capacity */
    bridge->effects.auditory_acuity = 1.0f;
    bridge->effects.speech_recognition = 1.0f;
    bridge->effects.object_recognition = 1.0f;
    bridge->effects.face_processing = 1.0f;
    bridge->effects.semantic_retrieval = 1.0f;
    bridge->effects.spreading_activation = 1.0f;
    bridge->effects.overall_capacity = 1.0f;

    bridge->last_atp_level = 1.0f;
    bridge->last_fatigue_level = 0.0f;

    LOG_INFO("[%s] Temporal substrate bridge created", LOG_MODULE);
    return bridge;
}

void temporal_substrate_bridge_destroy(temporal_substrate_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "temporal_substrate");
    nimcp_free(bridge);
    LOG_DEBUG("[%s] Temporal substrate bridge destroyed", LOG_MODULE);
}

/*=============================================================================
 * Update API
 *===========================================================================*/

int temporal_substrate_bridge_update(temporal_substrate_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Get current substrate state */
    float atp_level = 1.0f;
    float fatigue_level = 0.0f;

    if (bridge->substrate) {
        /* TODO: Get actual values from substrate */
        /* atp_level = neural_substrate_get_atp(bridge->substrate); */
        /* fatigue_level = neural_substrate_get_fatigue(bridge->substrate); */
    }

    bridge->last_atp_level = atp_level;
    bridge->last_fatigue_level = fatigue_level;

    /* Compute effects based on metabolic state */
    float atp_factor = 1.0f;
    float fatigue_factor = 1.0f;

    if (bridge->config.enable_atp_modulation) {
        /* Low ATP reduces processing capacity */
        atp_factor = atp_level * bridge->config.atp_sensitivity;
        atp_factor = fmaxf(bridge->config.min_capacity, fminf(1.0f, atp_factor));
    }

    if (bridge->config.enable_fatigue_modulation) {
        /* High fatigue reduces processing capacity */
        fatigue_factor = (1.0f - fatigue_level) * bridge->config.fatigue_sensitivity;
        fatigue_factor = fmaxf(bridge->config.min_capacity, fminf(1.0f, fatigue_factor));
    }

    /* Update individual effects */
    bridge->effects.auditory_acuity = atp_factor * bridge->config.auditory_atp_weight;
    bridge->effects.speech_recognition = atp_factor * 0.9f + fatigue_factor * 0.1f;
    bridge->effects.object_recognition = atp_factor * 0.7f + fatigue_factor * 0.3f;
    bridge->effects.face_processing = atp_factor * 0.8f + fatigue_factor * 0.2f;
    bridge->effects.semantic_retrieval = fatigue_factor * bridge->config.semantic_fatigue_weight;
    bridge->effects.spreading_activation = fatigue_factor * 0.8f + atp_factor * 0.2f;

    /* Clamp all effects */
    bridge->effects.auditory_acuity = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.auditory_acuity));
    bridge->effects.speech_recognition = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.speech_recognition));
    bridge->effects.object_recognition = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.object_recognition));
    bridge->effects.face_processing = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.face_processing));
    bridge->effects.semantic_retrieval = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.semantic_retrieval));
    bridge->effects.spreading_activation = fmaxf(bridge->config.min_capacity,
        fminf(1.0f, bridge->effects.spreading_activation));

    /* Compute overall capacity */
    bridge->effects.overall_capacity = (
        bridge->effects.auditory_acuity +
        bridge->effects.object_recognition +
        bridge->effects.semantic_retrieval
    ) / 3.0f;

    /* Update statistics */
    bridge->stats.updates_processed++;

    if (atp_level < 0.5f) {
        bridge->stats.low_atp_events++;
    }
    if (fatigue_level > 0.5f) {
        bridge->stats.high_fatigue_events++;
    }

    /* Update running averages */
    float alpha = 0.1f; /* Exponential moving average factor */
    bridge->stats.avg_auditory_acuity = bridge->stats.avg_auditory_acuity * (1.0f - alpha) +
        bridge->effects.auditory_acuity * alpha;
    bridge->stats.avg_object_recognition = bridge->stats.avg_object_recognition * (1.0f - alpha) +
        bridge->effects.object_recognition * alpha;
    bridge->stats.avg_semantic_retrieval = bridge->stats.avg_semantic_retrieval * (1.0f - alpha) +
        bridge->effects.semantic_retrieval * alpha;

    if (bridge->effects.overall_capacity < bridge->stats.min_observed_capacity ||
        bridge->stats.min_observed_capacity == 0.0f) {
        bridge->stats.min_observed_capacity = bridge->effects.overall_capacity;
    }

    return 0;
}

int temporal_substrate_bridge_get_effects(
    const temporal_substrate_bridge_t* bridge,
    temporal_substrate_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_substrate_bridge_get_effects: required parameter is NULL");
        return -1;
    }
    memcpy(effects, &bridge->effects, sizeof(temporal_substrate_effects_t));
    return 0;
}

int temporal_substrate_bridge_apply_effects(temporal_substrate_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* TODO: Apply effects to temporal adapter */
    /* This would modulate recognition thresholds, retrieval speeds, etc. */

    return 0;
}

/*=============================================================================
 * Bio-Async API
 *===========================================================================*/

int temporal_substrate_bridge_register_bio_async(
    temporal_substrate_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_substrate_bridge_register_bio_async: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_bio_async) return 0;

    /* TODO: Register message handlers */

    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int temporal_substrate_bridge_get_stats(
    const temporal_substrate_bridge_t* bridge,
    temporal_substrate_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_substrate_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(temporal_substrate_stats_t));
    return 0;
}

void temporal_substrate_bridge_reset_stats(temporal_substrate_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(temporal_substrate_stats_t));
    }
}
