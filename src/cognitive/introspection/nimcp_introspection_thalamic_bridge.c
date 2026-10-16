/**
 * @file nimcp_introspection_thalamic_bridge.c
 * @brief Introspection-Thalamic Bridge Implementation
 */

#include "cognitive/introspection/nimcp_introspection_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(introspection_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_introspection_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_introspection_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t introspection_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_introspection_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "introspection_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "introspection_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_introspection_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_introspection_thalamic_bridge_mesh_registry = registry;
    return err;
}

void introspection_thalamic_bridge_mesh_unregister(void) {
    if (g_introspection_thalamic_bridge_mesh_registry && g_introspection_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_introspection_thalamic_bridge_mesh_registry, g_introspection_thalamic_bridge_mesh_id);
        g_introspection_thalamic_bridge_mesh_id = 0;
        g_introspection_thalamic_bridge_mesh_registry = NULL;
    }
}


static inline void introspection_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_introspection_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_introspection_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_introspection_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct introspection_thalamic_bridge {
    bridge_base_t base;
    void* introspection;
    thalamic_router_t* router;
    introspection_thalamic_config_t config;
    introspection_thalamic_stats_t stats;
    float attention_weight;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

introspection_thalamic_config_t introspection_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    return (introspection_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_depth_routing = true,
        .min_urgency_threshold = 0.2f,
        .depth_boost = 1.25f
    };
}

introspection_thalamic_bridge_t* introspection_thalamic_bridge_create(
    void* introspection,
    thalamic_router_t* router,
    const introspection_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_create", 0.0f);


    introspection_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(introspection_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "introspection_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->introspection = introspection;
    bridge->router = router;
    bridge->config = config ? *config : introspection_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void introspection_thalamic_bridge_destroy(introspection_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int introspection_thalamic_bridge_reset(introspection_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_thalamic_route_signal(
    introspection_thalamic_bridge_t* bridge,
    const introspection_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->introspection_urgency * bridge->attention_weight;

        if (bridge->config.enable_depth_routing && signal->depth > 0.6f) {
            effective_urgency *= bridge->config.depth_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case INTROSPECTION_SIGNAL_MONITOR:
            bridge->stats.monitors_routed++;
            break;
        case INTROSPECTION_SIGNAL_REFLECT:
            bridge->stats.reflections++;
            break;
        case INTROSPECTION_SIGNAL_META:
            bridge->stats.meta_insights++;
            break;
        case INTROSPECTION_SIGNAL_AWARENESS:
            bridge->stats.awareness_updates++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "introspection_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.monitors_routed + bridge->stats.reflections +
                     bridge->stats.meta_insights + bridge->stats.awareness_updates;
    if (total > 0) {
        bridge->stats.avg_depth =
            (bridge->stats.avg_depth * (total - 1) + signal->depth) / total;
        bridge->stats.avg_clarity =
            (bridge->stats.avg_clarity * (total - 1) + signal->clarity) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_thalamic_route_monitor(
    introspection_thalamic_bridge_t* bridge,
    float depth,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_route_monitor: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    introspection_thalamic_signal_t signal = {
        .signal_type = INTROSPECTION_SIGNAL_MONITOR,
        .introspection_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .depth = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth),
        .clarity = 0.7f,
        .self_relevance = 0.8f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return introspection_thalamic_route_signal(bridge, &signal);
}

int introspection_thalamic_route_reflection(
    introspection_thalamic_bridge_t* bridge,
    float clarity,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_route_reflection: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    introspection_thalamic_signal_t signal = {
        .signal_type = INTROSPECTION_SIGNAL_REFLECT,
        .introspection_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .depth = 0.6f,
        .clarity = clarity < 0.0f ? 0.0f : (clarity > 1.0f ? 1.0f : clarity),
        .self_relevance = 0.9f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return introspection_thalamic_route_signal(bridge, &signal);
}

int introspection_thalamic_set_attention(introspection_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_thalamic_get_attention(introspection_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_introspection_thalam", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int introspection_thalamic_bridge_get_stats(
    const introspection_thalamic_bridge_t* bridge,
    introspection_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "introspection_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about introspection thalamic bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int introspection_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    introspection_thalamic_bridge_heartbeat("introspectio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Introspection_Thalamic_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                introspection_thalamic_bridge_heartbeat("introspectio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Introspection thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Introspection_Thalamic_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Introspection thalamic bridge has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Introspection_Thalamic_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Introspection thalamic bridge has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Full Training
 * ============================================================================ */

void introspection_thalamic_bridge_set_instance_health_agent(
    introspection_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int introspection_thalamic_bridge_training_begin(introspection_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    introspection_thalamic_bridge_heartbeat_instance(bridge->health_agent,
        "intro_thal_training_begin", 0.0f);
    bridge->stats.monitors_routed = 0;
    bridge->stats.avg_depth = 0.0f;
    bridge->attention_weight = 0.5f;
    NIMCP_LOGGING_INFO("[INTRO_THALAMIC] Training begin: counters reset, baseline state initialized");
    return 0;
}

int introspection_thalamic_bridge_training_step(introspection_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    introspection_thalamic_bridge_heartbeat_instance(bridge->health_agent,
        "intro_thal_training_step", progress);
    float lr = bridge->config.min_urgency_threshold;
    float adaptation = lr * (1.0f - progress) * 0.1f;
    bridge->config.min_urgency_threshold = lr + adaptation;
    if (bridge->config.min_urgency_threshold > 1.0f) bridge->config.min_urgency_threshold = 1.0f;
    if (bridge->config.min_urgency_threshold < 0.001f) bridge->config.min_urgency_threshold = 0.001f;
    bridge->attention_weight = bridge->attention_weight * 0.99f + progress * 0.01f;
    bridge->stats.monitors_routed++;
    return 0;
}

int introspection_thalamic_bridge_training_end(introspection_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "introspection_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    introspection_thalamic_bridge_heartbeat_instance(bridge->health_agent,
        "intro_thal_training_end", 1.0f);
    if (bridge->attention_weight < 0.0f) bridge->attention_weight = 0.0f;
    if (bridge->attention_weight > 1.0f) bridge->attention_weight = 1.0f;
    NIMCP_LOGGING_INFO("[INTRO_THALAMIC] Training end: attention=%.3f, routed=%u",
        bridge->attention_weight, bridge->stats.monitors_routed);
    return 0;
}
