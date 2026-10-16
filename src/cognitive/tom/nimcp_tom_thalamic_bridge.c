/**
 * @file nimcp_tom_thalamic_bridge.c
 * @brief Theory of Mind-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/tom/nimcp_tom_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(tom_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_tom_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_tom_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t tom_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_tom_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "tom_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "tom_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_tom_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_tom_thalamic_bridge_mesh_registry = registry;
    return err;
}

void tom_thalamic_bridge_mesh_unregister(void) {
    if (g_tom_thalamic_bridge_mesh_registry && g_tom_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_tom_thalamic_bridge_mesh_registry, g_tom_thalamic_bridge_mesh_id);
        g_tom_thalamic_bridge_mesh_id = 0;
        g_tom_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from tom_thalamic_bridge module (instance-level) */
static inline void tom_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_tom_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_tom_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_tom_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "TOM_THALAMIC_BRIDGE"


struct tom_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* tom;
    thalamic_router_t* router;
    tom_thalamic_config_t config;
    tom_thalamic_stats_t stats;
    float attention_weight;
};

tom_thalamic_config_t tom_thalamic_default_config(void) {
    return (tom_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_social_boost = true,
        .min_urgency_threshold = 0.2f,
        .social_boost_factor = 1.3f
    };
}

tom_thalamic_bridge_t* tom_thalamic_bridge_create(
    void* tom,
    thalamic_router_t* router,
    const tom_thalamic_config_t* config
) {
    tom_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(tom_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->tom = tom;
    bridge->router = router;
    bridge->config = config ? *config : tom_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "tom_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    return bridge;
}

void tom_thalamic_bridge_destroy(tom_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int tom_thalamic_bridge_reset(tom_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_thalamic_route_signal(
    tom_thalamic_bridge_t* bridge,
    const tom_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->tom_urgency * bridge->attention_weight;

        /* Social relevance gets boost */
        if (bridge->config.enable_social_boost && signal->social_relevance > 0.6f) {
            effective_urgency *= bridge->config.social_boost_factor;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case TOM_SIGNAL_BELIEF_ATTR:
            bridge->stats.belief_attributions++;
            break;
        case TOM_SIGNAL_INTENT_INFER:
            bridge->stats.intent_inferences++;
            break;
        case TOM_SIGNAL_EMOTION_ATTR:
            bridge->stats.emotion_attributions++;
            break;
        case TOM_SIGNAL_PERSPECTIVE:
            bridge->stats.perspective_takes++;
            break;
        default:
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "tom_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.belief_attributions + bridge->stats.intent_inferences +
                     bridge->stats.emotion_attributions + bridge->stats.perspective_takes;
    if (total > 0) {
        bridge->stats.avg_mentalizing_depth =
            (bridge->stats.avg_mentalizing_depth * (total - 1) + signal->mentalizing_depth) / total;
        bridge->stats.avg_confidence =
            (bridge->stats.avg_confidence * (total - 1) + signal->confidence) / total;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int tom_thalamic_route_belief_attribution(
    tom_thalamic_bridge_t* bridge,
    float depth,
    float confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_route_belief_attribution: bridge is NULL");
        return -1;
    }

    tom_thalamic_signal_t signal = {
        .signal_type = TOM_SIGNAL_BELIEF_ATTR,
        .tom_urgency = 0.5f + (depth * 0.3f),
        .mentalizing_depth = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth),
        .confidence = confidence < 0.0f ? 0.0f : (confidence > 1.0f ? 1.0f : confidence),
        .social_relevance = 0.7f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return tom_thalamic_route_signal(bridge, &signal);
}

int tom_thalamic_route_perspective(
    tom_thalamic_bridge_t* bridge,
    float social_relevance,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_route_perspective: bridge is NULL");
        return -1;
    }

    tom_thalamic_signal_t signal = {
        .signal_type = TOM_SIGNAL_PERSPECTIVE,
        .tom_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .mentalizing_depth = 0.6f,
        .confidence = 0.7f,
        .social_relevance = social_relevance < 0.0f ? 0.0f : (social_relevance > 1.0f ? 1.0f : social_relevance),
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return tom_thalamic_route_signal(bridge, &signal);
}

int tom_thalamic_route_inference(tom_thalamic_bridge_t* bridge, const tom_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_route_inference: required parameter is NULL (bridge, signal)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Apply attention gating */
    if (bridge->config.enable_attention_gating &&
        signal->social_relevance < bridge->config.min_urgency_threshold) {
        bridge->stats.signals_gated++;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    /* Count the inference based on signal type */
    switch (signal->signal_type) {
        case TOM_SIGNAL_BELIEF_ATTR:
            bridge->stats.belief_attributions++;
            break;
        case TOM_SIGNAL_INTENT_INFER:
            bridge->stats.intent_inferences++;
            break;
        case TOM_SIGNAL_EMOTION_ATTR:
            bridge->stats.emotion_attributions++;
            break;
        case TOM_SIGNAL_PERSPECTIVE:
            bridge->stats.perspective_takes++;
            break;
    }
    /* Update average confidence */
    uint64_t total = bridge->stats.belief_attributions + bridge->stats.intent_inferences +
                     bridge->stats.emotion_attributions + bridge->stats.perspective_takes;
    if (total > 0) {
        bridge->stats.avg_confidence = (bridge->stats.avg_confidence * (total - 1) +
                                        signal->confidence) / total;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_thalamic_set_attention(tom_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_thalamic_get_attention(const tom_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int tom_thalamic_bridge_get_stats(
    const tom_thalamic_bridge_t* bridge,
    tom_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tom_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int tom_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "ToM_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "ToM_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "ToM_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void tom_thalamic_bridge_set_instance_health_agent(tom_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "tom_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int tom_thalamic_bridge_training_begin(tom_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    tom_thalamic_bridge_heartbeat_instance(bridge->health_agent, "tom_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int tom_thalamic_bridge_training_end(tom_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    tom_thalamic_bridge_heartbeat_instance(bridge->health_agent, "tom_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int tom_thalamic_bridge_training_step(tom_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "tom_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    tom_thalamic_bridge_heartbeat_instance(bridge->health_agent, "tom_thalamic_bridge_training_step", progress);
    return 0;
}
