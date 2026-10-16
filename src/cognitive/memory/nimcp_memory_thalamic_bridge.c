/**
 * @file nimcp_memory_thalamic_bridge.c
 * @brief Memory-Thalamic Bridge Implementation
 */

#include "cognitive/memory/nimcp_memory_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_memory_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_memory_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t memory_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_memory_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "memory_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "memory_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_memory_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_memory_thalamic_bridge_mesh_registry = registry;
    return err;
}

void memory_thalamic_bridge_mesh_unregister(void) {
    if (g_memory_thalamic_bridge_mesh_registry && g_memory_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_memory_thalamic_bridge_mesh_registry, g_memory_thalamic_bridge_mesh_id);
        g_memory_thalamic_bridge_mesh_id = 0;
        g_memory_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from memory_thalamic_bridge module (instance-level) */
static inline void memory_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_memory_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_memory_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_memory_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
struct memory_thalamic_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* memory;
    thalamic_router_t* router;
    memory_thalamic_config_t config;
    memory_thalamic_stats_t stats;
    float attention_weight;
};

BRIDGE_DEFINE_SECURITY_SETTERS(memory_thalamic_bridge)

memory_thalamic_config_t memory_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_defa", 0.0f);


    return (memory_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_emotional_boost = true,
        .enable_consolidation_routing = true,
        .min_urgency_threshold = 0.2f,
        .emotional_boost_factor = 1.4f
    };
}

memory_thalamic_bridge_t* memory_thalamic_bridge_create(
    void* memory,
    thalamic_router_t* router,
    const memory_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_create", 0.0f);


    memory_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(memory_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "memory_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->memory = memory;
    bridge->router = router;
    bridge->config = config ? *config : memory_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void memory_thalamic_bridge_destroy(memory_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int memory_thalamic_bridge_reset(memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_thalamic_route_signal(
    memory_thalamic_bridge_t* bridge,
    const memory_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_rout", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(*signal));

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->memory_urgency * bridge->attention_weight;

        /* Emotional memories get boost */
        if (bridge->config.enable_emotional_boost && signal->emotional_weight > 0.5f) {
            effective_urgency *= bridge->config.emotional_boost_factor;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case MEMORY_SIGNAL_ENCODE:
            bridge->stats.encodings_routed++;
            break;
        case MEMORY_SIGNAL_RETRIEVE:
            bridge->stats.retrievals_routed++;
            break;
        case MEMORY_SIGNAL_CONSOLIDATE:
            bridge->stats.consolidations++;
            break;
        case MEMORY_SIGNAL_UPDATE:
            bridge->stats.updates++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "memory_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.encodings_routed + bridge->stats.retrievals_routed +
                     bridge->stats.consolidations + bridge->stats.updates;
    if (total > 0) {
        bridge->stats.avg_strength =
            (bridge->stats.avg_strength * (total - 1) + signal->strength) / total;
        bridge->stats.avg_emotional_weight =
            (bridge->stats.avg_emotional_weight * (total - 1) + signal->emotional_weight) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_thalamic_route_encode(
    memory_thalamic_bridge_t* bridge,
    float strength,
    float emotional_weight
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_route_encode: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_rout", 0.0f);


    memory_thalamic_signal_t signal = {
        .signal_type = MEMORY_SIGNAL_ENCODE,
        .memory_urgency = 0.6f + (emotional_weight * 0.3f),
        .strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength),
        .salience = 0.5f,
        .emotional_weight = emotional_weight < 0.0f ? 0.0f : (emotional_weight > 1.0f ? 1.0f : emotional_weight),
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return memory_thalamic_route_signal(bridge, &signal);
}

int memory_thalamic_route_retrieve(
    memory_thalamic_bridge_t* bridge,
    float salience,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_route_retrieve: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_rout", 0.0f);


    memory_thalamic_signal_t signal = {
        .signal_type = MEMORY_SIGNAL_RETRIEVE,
        .memory_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .strength = 0.5f,
        .salience = salience < 0.0f ? 0.0f : (salience > 1.0f ? 1.0f : salience),
        .emotional_weight = 0.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return memory_thalamic_route_signal(bridge, &signal);
}

int memory_thalamic_set_attention(memory_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_set_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_thalamic_get_attention(memory_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_memory_thalamic_get_", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, attention, sizeof(*attention));

    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_thalamic_bridge_get_stats(
    const memory_thalamic_bridge_t* bridge,
    memory_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "memory_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int memory_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    memory_thalamic_bridge_heartbeat("memory_thala_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Memory_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                memory_thalamic_bridge_heartbeat("memory_thala_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Memory thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Memory_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Memory_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void memory_thalamic_bridge_set_instance_health_agent(memory_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "memory_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int memory_thalamic_bridge_training_begin(memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "memory_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int memory_thalamic_bridge_training_end(memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "memory_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int memory_thalamic_bridge_training_step(memory_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "memory_thalamic_bridge_training_step", progress);
    return 0;
}
