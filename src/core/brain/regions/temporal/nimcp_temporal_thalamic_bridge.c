/**
 * @file nimcp_temporal_thalamic_bridge.c
 * @brief Implementation of temporal cortex-thalamic bridge
 *
 * WHAT: Links temporal cortex to thalamic signal routing
 * WHY:  Sensory signals flow through thalamus (MGN) to temporal cortex
 * HOW:  Routes auditory through MGN, modulates attention gating
 *
 * @author NIMCP Team
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/temporal/nimcp_temporal_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define LOG_MODULE "TEMPORAL_THALAMIC"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(temporal_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct temporal_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* temporal;                      /**< Temporal adapter handle */
    void* router;                        /**< Thalamic router handle */
    temporal_thalamic_config_t config;   /**< Configuration */
    temporal_thalamic_state_t state;     /**< Current state */
    temporal_thalamic_stats_t stats;     /**< Statistics */

    /* Internal buffers */
    float* routed_signal_buffer;
    uint32_t buffer_size;
};

/*=============================================================================
 * Configuration API
 *===========================================================================*/

temporal_thalamic_config_t temporal_thalamic_default_config(void) {
    temporal_thalamic_config_t config;
    memset(&config, 0, sizeof(config));

    config.enable_attention_gating = true;
    config.enable_auditory_priority = true;
    config.enable_visual_priority = false;
    config.enable_feedback_modulation = true;
    config.min_urgency_threshold = 0.3f;
    config.auditory_boost = 1.2f;
    config.visual_boost = 1.0f;
    config.attention_decay_rate = 0.1f;

    return config;
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

temporal_thalamic_bridge_t* temporal_thalamic_bridge_create(
    void* temporal,
    void* router,
    const temporal_thalamic_config_t* config
) {
    temporal_thalamic_bridge_t* bridge = (temporal_thalamic_bridge_t*)nimcp_calloc(
        1, sizeof(temporal_thalamic_bridge_t));
    if (!bridge) {
        LOG_ERROR("[%s] Failed to allocate bridge", LOG_MODULE);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;
    }

    bridge->temporal = temporal;
    bridge->router = router;
    bridge->config = config ? *config : temporal_thalamic_default_config();

    /* Initialize state */
    bridge->state.mgn_gain = 1.0f;
    bridge->state.pulvinar_attention = 0.5f;
    bridge->state.md_feedback = 0.5f;
    bridge->state.reticular_gate = 1.0f;
    bridge->state.auditory_pathway_active = true;
    bridge->state.visual_pathway_active = true;
    bridge->state.semantic_feedback_active = true;

    /* Allocate routing buffer */
    bridge->buffer_size = 1024;
    bridge->routed_signal_buffer = (float*)nimcp_calloc(bridge->buffer_size, sizeof(float));
    if (!bridge->routed_signal_buffer) {
        LOG_ERROR("[%s] Failed to allocate routing buffer", LOG_MODULE);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "temporal_thalamic_bridge_create: bridge->routed_signal_buffer is NULL");
        return NULL;
    }

    LOG_INFO("[%s] Temporal thalamic bridge created", LOG_MODULE);
    return bridge;
}

void temporal_thalamic_bridge_destroy(temporal_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "temporal_thalamic");

    if (bridge->routed_signal_buffer) {
        nimcp_free(bridge->routed_signal_buffer);
    }

    nimcp_free(bridge);
    LOG_DEBUG("[%s] Temporal thalamic bridge destroyed", LOG_MODULE);
}

int temporal_thalamic_bridge_reset(temporal_thalamic_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    bridge->state.mgn_gain = 1.0f;
    bridge->state.pulvinar_attention = 0.5f;
    bridge->state.md_feedback = 0.5f;
    bridge->state.reticular_gate = 1.0f;
    bridge->state.auditory_pathway_active = true;
    bridge->state.visual_pathway_active = true;
    bridge->state.semantic_feedback_active = true;

    if (bridge->routed_signal_buffer) {
        memset(bridge->routed_signal_buffer, 0, bridge->buffer_size * sizeof(float));
    }

    return 0;
}

/*=============================================================================
 * Routing API
 *===========================================================================*/

int temporal_thalamic_route_signal(
    temporal_thalamic_bridge_t* bridge,
    const temporal_thalamic_request_t* request,
    temporal_thalamic_response_t* response
) {
    if (!bridge || !request || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_route_signal: required parameter is NULL");
        return -1;
    }

    memset(response, 0, sizeof(temporal_thalamic_response_t));

    /* Check urgency threshold */
    if (request->urgency < bridge->config.min_urgency_threshold) {
        response->was_suppressed = true;
        bridge->stats.signals_suppressed++;
        return 0;
    }

    /* Determine routing gain based on pathway */
    float gain = 1.0f;
    float gating = bridge->state.reticular_gate;

    switch (request->source) {
        case TEMPORAL_THALAMIC_MGN:
            gain = bridge->state.mgn_gain;
            if (bridge->config.enable_auditory_priority) {
                gain *= bridge->config.auditory_boost;
            }
            bridge->stats.auditory_signals++;
            break;

        case TEMPORAL_THALAMIC_PULVINAR:
            gain = bridge->state.pulvinar_attention;
            if (bridge->config.enable_visual_priority) {
                gain *= bridge->config.visual_boost;
            }
            bridge->stats.visual_signals++;
            break;

        case TEMPORAL_THALAMIC_MD:
            gain = bridge->state.md_feedback;
            break;

        case TEMPORAL_THALAMIC_RETICULAR:
            /* Reticular modulates gating, not signal */
            gating = request->attention_boost;
            bridge->state.reticular_gate = gating;
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_route_signal: operation failed");
            return -1;
    }

    /* Apply attention modulation */
    if (bridge->config.enable_attention_gating) {
        gain *= gating;
        gain *= (1.0f + request->attention_boost);
    }

    /* Route signal through buffer */
    uint32_t copy_dim = request->signal_dim < bridge->buffer_size ?
                        request->signal_dim : bridge->buffer_size;

    for (uint32_t i = 0; i < copy_dim; i++) {
        bridge->routed_signal_buffer[i] = request->signal[i] * gain;
    }

    /* Fill response */
    response->routed_signal = bridge->routed_signal_buffer;
    response->signal_dim = copy_dim;
    response->effective_gain = gain;
    response->gating_applied = gating;
    response->was_suppressed = false;
    response->latency_ms = 2.0f; /* Simulated thalamic latency */

    /* Update statistics */
    bridge->stats.signals_routed++;
    bridge->stats.avg_mgn_gain = bridge->stats.avg_mgn_gain * 0.9f +
        bridge->state.mgn_gain * 0.1f;
    bridge->stats.avg_pulvinar_attention = bridge->stats.avg_pulvinar_attention * 0.9f +
        bridge->state.pulvinar_attention * 0.1f;
    bridge->stats.avg_routing_latency_ms = bridge->stats.avg_routing_latency_ms * 0.9f +
        response->latency_ms * 0.1f;

    return 0;
}

int temporal_thalamic_route_auditory(
    temporal_thalamic_bridge_t* bridge,
    const float* auditory_signal,
    uint32_t signal_dim,
    temporal_thalamic_response_t* response
) {
    if (!bridge || !auditory_signal || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_route_auditory: required parameter is NULL");
        return -1;
    }

    temporal_thalamic_request_t request;
    memset(&request, 0, sizeof(request));
    request.source = TEMPORAL_THALAMIC_MGN;
    request.signal = (float*)auditory_signal;
    request.signal_dim = signal_dim;
    request.urgency = 0.8f;
    request.attention_boost = 0.0f;

    return temporal_thalamic_route_signal(bridge, &request, response);
}

int temporal_thalamic_route_visual(
    temporal_thalamic_bridge_t* bridge,
    const float* visual_signal,
    uint32_t signal_dim,
    temporal_thalamic_response_t* response
) {
    if (!bridge || !visual_signal || !response) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_route_visual: required parameter is NULL");
        return -1;
    }

    temporal_thalamic_request_t request;
    memset(&request, 0, sizeof(request));
    request.source = TEMPORAL_THALAMIC_PULVINAR;
    request.signal = (float*)visual_signal;
    request.signal_dim = signal_dim;
    request.urgency = 0.7f;
    request.attention_boost = 0.0f;

    return temporal_thalamic_route_signal(bridge, &request, response);
}

/*=============================================================================
 * Attention Modulation API
 *===========================================================================*/

int temporal_thalamic_set_attention(
    temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_nucleus_t nucleus,
    float attention
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    attention = fmaxf(0.0f, fminf(1.0f, attention));

    switch (nucleus) {
        case TEMPORAL_THALAMIC_MGN:
            bridge->state.mgn_gain = 0.5f + attention * 0.5f;
            break;

        case TEMPORAL_THALAMIC_PULVINAR:
            bridge->state.pulvinar_attention = attention;
            break;

        case TEMPORAL_THALAMIC_MD:
            bridge->state.md_feedback = attention;
            break;

        case TEMPORAL_THALAMIC_RETICULAR:
            bridge->state.reticular_gate = attention;
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_set_attention: operation failed");
            return -1;
    }

    return 0;
}

int temporal_thalamic_get_state(
    const temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_state_t* state
) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_get_state: required parameter is NULL");
        return -1;
    }
    memcpy(state, &bridge->state, sizeof(temporal_thalamic_state_t));
    return 0;
}

int temporal_thalamic_apply_feedback(
    temporal_thalamic_bridge_t* bridge,
    const float* feedback_signal,
    uint32_t signal_dim
) {
    if (!bridge || !feedback_signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_apply_feedback: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_feedback_modulation) return 0;

    /* Compute feedback modulation from signal */
    float feedback_strength = 0.0f;
    for (uint32_t i = 0; i < signal_dim && i < 64; i++) {
        feedback_strength += fabsf(feedback_signal[i]);
    }
    feedback_strength /= (float)signal_dim;

    /* Apply to MD feedback */
    bridge->state.md_feedback = 0.7f * bridge->state.md_feedback +
                                0.3f * feedback_strength;
    bridge->state.md_feedback = fmaxf(0.0f, fminf(1.0f, bridge->state.md_feedback));

    return 0;
}

/*=============================================================================
 * Bio-Async API
 *===========================================================================*/

int temporal_thalamic_bridge_register_bio_async(
    temporal_thalamic_bridge_t* bridge,
    bio_router_t* router
) {
    if (!bridge || !router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_bridge_register_bio_async: required parameter is NULL");
        return -1;
    }
    /* TODO: Register message handlers */
    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int temporal_thalamic_bridge_get_stats(
    const temporal_thalamic_bridge_t* bridge,
    temporal_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "temporal_thalamic_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    memcpy(stats, &bridge->stats, sizeof(temporal_thalamic_stats_t));
    return 0;
}

void temporal_thalamic_bridge_reset_stats(temporal_thalamic_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(temporal_thalamic_stats_t));
    }
}
