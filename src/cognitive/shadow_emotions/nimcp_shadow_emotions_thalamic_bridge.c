/**
 * @file nimcp_shadow_emotions_thalamic_bridge.c
 * @brief Shadow Emotions-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/shadow_emotions/nimcp_shadow_emotions_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(shadow_emotions_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_shadow_emotions_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_shadow_emotions_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t shadow_emotions_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_shadow_emotions_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "shadow_emotions_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "shadow_emotions_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_shadow_emotions_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_shadow_emotions_thalamic_bridge_mesh_registry = registry;
    return err;
}

void shadow_emotions_thalamic_bridge_mesh_unregister(void) {
    if (g_shadow_emotions_thalamic_bridge_mesh_registry && g_shadow_emotions_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_shadow_emotions_thalamic_bridge_mesh_registry, g_shadow_emotions_thalamic_bridge_mesh_id);
        g_shadow_emotions_thalamic_bridge_mesh_id = 0;
        g_shadow_emotions_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from shadow_emotions_thalamic_bridge module (instance-level) */
static inline void shadow_emotions_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_shadow_emotions_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_shadow_emotions_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_shadow_emotions_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "SHADOW_EMOTIONS_THALAMIC_BRIDGE"


struct shadow_emotions_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* shadow_emotions;
    thalamic_router_t* router;
    shadow_emotions_thalamic_config_t config;
    shadow_emotions_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

shadow_emotions_thalamic_config_t shadow_emotions_thalamic_default_config(void) {
    return (shadow_emotions_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_emergence_priority = true,
        .min_urgency_threshold = 0.15f,
        .emergence_boost = 1.4f
    };
}

shadow_emotions_thalamic_bridge_t* shadow_emotions_thalamic_bridge_create(
    void* shadow_emotions,
    thalamic_router_t* router,
    const shadow_emotions_thalamic_config_t* config
) {
    shadow_emotions_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(shadow_emotions_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->shadow_emotions = shadow_emotions;
    bridge->router = router;
    bridge->config = config ? *config : shadow_emotions_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "shadow_emotions_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    return bridge;
}

void shadow_emotions_thalamic_bridge_destroy(shadow_emotions_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int shadow_emotions_thalamic_bridge_reset(shadow_emotions_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_thalamic_route_signal(
    shadow_emotions_thalamic_bridge_t* bridge,
    const shadow_emotions_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->shadow_urgency * bridge->attention_weight;

        /* Emergence gets priority (breaking through suppression) */
        if (bridge->config.enable_emergence_priority &&
            signal->signal_type == SHADOW_EMO_SIGNAL_EMERGENCE) {
            effective_urgency *= bridge->config.emergence_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case SHADOW_EMO_SIGNAL_EMERGENCE:
            bridge->stats.emergences++;
            break;
        case SHADOW_EMO_SIGNAL_SUPPRESSION:
            bridge->stats.suppressions++;
            break;
        case SHADOW_EMO_SIGNAL_INTEGRATION:
            bridge->stats.integrations++;
            break;
        case SHADOW_EMO_SIGNAL_PROJECTION:
            bridge->stats.projections++;
            break;
        default:
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "shadow_emotions_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.emergences + bridge->stats.suppressions +
                     bridge->stats.integrations + bridge->stats.projections;
    if (total > 0) {
        bridge->stats.avg_suppression_strength =
            (bridge->stats.avg_suppression_strength * (total - 1) + signal->suppression_strength) / total;
        bridge->stats.avg_emergence_pressure =
            (bridge->stats.avg_emergence_pressure * (total - 1) + signal->emergence_pressure) / total;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int shadow_emotions_thalamic_route_emergence(
    shadow_emotions_thalamic_bridge_t* bridge,
    float pressure,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_route_emergence: bridge is NULL");
        return -1;
    }

    shadow_emotions_thalamic_signal_t signal = {
        .signal_type = SHADOW_EMO_SIGNAL_EMERGENCE,
        .shadow_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .suppression_strength = 0.3f,
        .emergence_pressure = pressure < 0.0f ? 0.0f : (pressure > 1.0f ? 1.0f : pressure),
        .integration_readiness = 0.5f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return shadow_emotions_thalamic_route_signal(bridge, &signal);
}

int shadow_emotions_thalamic_set_attention(shadow_emotions_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_thalamic_get_attention(const shadow_emotions_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int shadow_emotions_thalamic_bridge_get_stats(
    const shadow_emotions_thalamic_bridge_t* bridge,
    shadow_emotions_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "shadow_emotions_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
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

int shadow_emotions_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Shadow_Emotions_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Shadow_Emotions_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Shadow_Emotions_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void shadow_emotions_thalamic_bridge_set_instance_health_agent(shadow_emotions_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "shadow_emotions_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int shadow_emotions_thalamic_bridge_training_begin(shadow_emotions_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_emotions_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    shadow_emotions_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_training_begin", 0.0f);
    return 0;
}

int shadow_emotions_thalamic_bridge_training_end(shadow_emotions_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_emotions_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    shadow_emotions_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_training_end", 1.0f);
    return 0;
}

int shadow_emotions_thalamic_bridge_training_step(shadow_emotions_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "shadow_emotions_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    shadow_emotions_thalamic_bridge_heartbeat_instance(bridge->health_agent, "shadow_thalamic_training_step", progress);
    return 0;
}
