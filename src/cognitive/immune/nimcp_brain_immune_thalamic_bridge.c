/**
 * @file nimcp_brain_immune_thalamic_bridge.c
 * @brief Brain Immune-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/immune/nimcp_brain_immune_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(brain_immune_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_brain_immune_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_brain_immune_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t brain_immune_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_brain_immune_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "brain_immune_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "brain_immune_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_brain_immune_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_brain_immune_thalamic_bridge_mesh_registry = registry;
    return err;
}

void brain_immune_thalamic_bridge_mesh_unregister(void) {
    if (g_brain_immune_thalamic_bridge_mesh_registry && g_brain_immune_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_brain_immune_thalamic_bridge_mesh_registry, g_brain_immune_thalamic_bridge_mesh_id);
        g_brain_immune_thalamic_bridge_mesh_id = 0;
        g_brain_immune_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from brain_immune_thalamic_bridge module (instance-level) */
static inline void brain_immune_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_brain_immune_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_brain_immune_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_brain_immune_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



struct brain_immune_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* brain_immune;
    thalamic_router_t* router;
    brain_immune_thalamic_config_t config;
    brain_immune_thalamic_stats_t stats;
    float attention_weight;
};

brain_immune_thalamic_config_t brain_immune_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);


    brain_immune_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_severity_boost = true,
        .min_threat_threshold = 0.2f,
        .inflammation_boost = 0.4f
    };
    return cfg;
}

brain_immune_thalamic_bridge_t* brain_immune_thalamic_bridge_create(void* brain_immune, thalamic_router_t* router, const brain_immune_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_create", 0.0f);


    brain_immune_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(brain_immune_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->brain_immune = brain_immune;
    bridge->router = router;
    bridge->config = config ? *config : brain_immune_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "brain_immune_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    return bridge;
}

void brain_immune_thalamic_bridge_destroy(brain_immune_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int brain_immune_thalamic_bridge_reset(brain_immune_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int brain_immune_thalamic_route_threat(brain_immune_thalamic_bridge_t* bridge, const brain_immune_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_route_threat: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* High-severity threats bypass attention gating */
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating &&
        signal->threat_severity < bridge->config.min_threat_threshold &&
        signal->inflammation_level < 0.6f) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.threats_routed++;
    bridge->stats.avg_threat_severity = (bridge->stats.avg_threat_severity * (bridge->stats.threats_routed - 1) +
                                         signal->threat_severity) / bridge->stats.threats_routed;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int brain_immune_thalamic_route_response(brain_immune_thalamic_bridge_t* bridge, const void* response, float intensity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_route_response: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.responses_triggered++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int brain_immune_thalamic_set_attention(brain_immune_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int brain_immune_thalamic_get_attention(const brain_immune_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);


    return 0;
}

int brain_immune_thalamic_bridge_get_stats(const brain_immune_thalamic_bridge_t* bridge, brain_immune_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain_immune_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Query KG for module self-awareness information
 * WHY:  Enable introspective self-knowledge about brain immune thalamic bridge
 * HOW:  Look up entity and relations in KG
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 otherwise
 */
int brain_immune_thalamic_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    brain_immune_thalamic_bridge_heartbeat("brain_immune_brain_immune_thalami", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Brain_Immune_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                brain_immune_thalamic_bridge_heartbeat("brain_immune_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Brain immune thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Brain_Immune_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Brain_Immune_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void brain_immune_thalamic_bridge_set_instance_health_agent(brain_immune_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "brain_immune_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int brain_immune_thalamic_bridge_training_begin(brain_immune_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    brain_immune_thalamic_bridge_heartbeat_instance(bridge->health_agent, "brain_immune_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int brain_immune_thalamic_bridge_training_end(brain_immune_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    brain_immune_thalamic_bridge_heartbeat_instance(bridge->health_agent, "brain_immune_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int brain_immune_thalamic_bridge_training_step(brain_immune_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "brain_immune_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    brain_immune_thalamic_bridge_heartbeat_instance(bridge->health_agent, "brain_immune_thalamic_bridge_training_step", progress);
    return 0;
}
