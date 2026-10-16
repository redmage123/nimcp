/**
 * @file nimcp_remorse_thalamic_bridge.c
 * @brief Remorse-Thalamic Bridge Implementation
 */

#include "cognitive/remorse/nimcp_remorse_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(remorse_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_remorse_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_remorse_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t remorse_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_remorse_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "remorse_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "remorse_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_remorse_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_remorse_thalamic_bridge_mesh_registry = registry;
    return err;
}

void remorse_thalamic_bridge_mesh_unregister(void) {
    if (g_remorse_thalamic_bridge_mesh_registry && g_remorse_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_remorse_thalamic_bridge_mesh_registry, g_remorse_thalamic_bridge_mesh_id);
        g_remorse_thalamic_bridge_mesh_id = 0;
        g_remorse_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from remorse_thalamic_bridge module (instance-level) */
static inline void remorse_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_remorse_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_remorse_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_remorse_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "REMORSE_THALAMIC_BRIDGE"


struct remorse_thalamic_bridge {
    bridge_base_t base;
    void* remorse;
    thalamic_router_t* router;
    remorse_thalamic_config_t config;
    remorse_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

remorse_thalamic_config_t remorse_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_remorse_thalamic_def", 0.0f);


    remorse_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_repair_routing = true,
        .min_guilt_threshold = 0.3f,
        .repair_threshold = 0.5f
    };
    return cfg;
}

remorse_thalamic_bridge_t* remorse_thalamic_bridge_create(void* remorse, thalamic_router_t* router, const remorse_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_create", 0.0f);


    remorse_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(remorse_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "remorse_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->remorse = remorse;
    bridge->router = router;
    bridge->config = config ? *config : remorse_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "remorse_thalamic");
    return bridge;
}

void remorse_thalamic_bridge_destroy(remorse_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "remorse_thalamic");
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int remorse_thalamic_bridge_reset(remorse_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int remorse_thalamic_route_guilt(remorse_thalamic_bridge_t* bridge, const remorse_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_route_guilt: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_remorse_thalamic_rou", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->guilt_intensity < bridge->config.min_guilt_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.guilt_signals_routed++;
    bridge->stats.avg_guilt_intensity = (bridge->stats.avg_guilt_intensity * (bridge->stats.guilt_signals_routed - 1) +
                                         signal->guilt_intensity) / bridge->stats.guilt_signals_routed;
    if (signal->signal_type == REMORSE_SIGNAL_FORGIVENESS) {
        bridge->stats.forgiveness_achieved++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int remorse_thalamic_route_repair(remorse_thalamic_bridge_t* bridge, const void* action, float motivation) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_route_repair: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_remorse_thalamic_rou", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_repair_routing && motivation >= bridge->config.repair_threshold) {
        bridge->stats.repair_actions++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int remorse_thalamic_set_attention(remorse_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_remorse_thalamic_set", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int remorse_thalamic_get_attention(const remorse_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    *attention = bridge->attention_weight;
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_remorse_thalamic_get", 0.0f);


    return 0;
}

int remorse_thalamic_bridge_get_stats(const remorse_thalamic_bridge_t* bridge, remorse_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "remorse_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int remorse_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    remorse_thalamic_bridge_heartbeat("remorse_thal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Remorse_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                remorse_thalamic_bridge_heartbeat("remorse_thal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Remorse_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Remorse_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void remorse_thalamic_bridge_set_instance_health_agent(remorse_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "remorse_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int remorse_thalamic_bridge_training_begin(remorse_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    remorse_thalamic_bridge_heartbeat_instance(bridge->health_agent, "remorse_thalamic_training_begin", 0.0f);
    return 0;
}

int remorse_thalamic_bridge_training_end(remorse_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    remorse_thalamic_bridge_heartbeat_instance(bridge->health_agent, "remorse_thalamic_training_end", 1.0f);
    return 0;
}

int remorse_thalamic_bridge_training_step(remorse_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "remorse_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    remorse_thalamic_bridge_heartbeat_instance(bridge->health_agent, "remorse_thalamic_training_step", progress);
    return 0;
}
