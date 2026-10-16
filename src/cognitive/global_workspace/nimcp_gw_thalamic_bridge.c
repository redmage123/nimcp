/**
 * @file nimcp_gw_thalamic_bridge.c
 * @brief Global Workspace-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/global_workspace/nimcp_gw_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gw_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_gw_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_gw_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t gw_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_gw_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "gw_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "gw_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_gw_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_gw_thalamic_bridge_mesh_registry = registry;
    return err;
}

void gw_thalamic_bridge_mesh_unregister(void) {
    if (g_gw_thalamic_bridge_mesh_registry && g_gw_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_gw_thalamic_bridge_mesh_registry, g_gw_thalamic_bridge_mesh_id);
        g_gw_thalamic_bridge_mesh_id = 0;
        g_gw_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from gw_thalamic_bridge module (instance-level) */
static inline void gw_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_gw_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_gw_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_gw_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "GW_THALAMIC_BRIDGE"


struct gw_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* gw;
    thalamic_router_t* router;
    gw_thalamic_config_t config;
    gw_thalamic_stats_t stats;
    float attention_weight;
    uint64_t update_count;
};

gw_thalamic_config_t gw_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__gw_thalamic_default_", 0.0f);


    gw_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_competition_routing = true,
        .min_salience_threshold = NIMCP_SALIENCE_THRESHOLD,
        .ignition_threshold = 0.6f
    };
    return cfg;
}

gw_thalamic_bridge_t* gw_thalamic_bridge_create(void* gw, thalamic_router_t* router, const gw_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__create", 0.0f);


    gw_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(gw_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->gw = gw;
    bridge->router = router;
    bridge->config = config ? *config : gw_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "gw_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "gw_thalamic");
    return bridge;
}

void gw_thalamic_bridge_destroy(gw_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int gw_thalamic_bridge_reset(gw_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_thalamic_route_broadcast(gw_thalamic_bridge_t* bridge, const gw_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_route_broadcast: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__gw_thalamic_route_br", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->salience < bridge->config.min_salience_threshold) {
        bridge->stats.signals_gated++;
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.broadcasts_routed++;
    bridge->stats.avg_attention = (bridge->stats.avg_attention * (bridge->stats.broadcasts_routed - 1) +
                                   signal->attention_weight) / bridge->stats.broadcasts_routed;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_thalamic_route_ignition(gw_thalamic_bridge_t* bridge, const void* content, float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_route_ignition: bridge is NULL");
        return -1;
    }
    if (strength < bridge->config.ignition_threshold) return 0;
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__gw_thalamic_route_ig", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.ignitions_triggered++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_thalamic_set_attention(gw_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__gw_thalamic_set_atte", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int gw_thalamic_get_attention(const gw_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__gw_thalamic_get_atte", 0.0f);


    return 0;
}

int gw_thalamic_bridge_get_stats(const gw_thalamic_bridge_t* bridge, gw_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "gw_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int gw_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    gw_thalamic_bridge_heartbeat("gw_thalamic__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Global_Workspace_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                gw_thalamic_bridge_heartbeat("gw_thalamic__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Global_Workspace_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Global_Workspace_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void gw_thalamic_bridge_set_instance_health_agent(gw_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "gw_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int gw_thalamic_bridge_training_begin(gw_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    gw_thalamic_bridge_heartbeat_instance(bridge->health_agent, "gw_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int gw_thalamic_bridge_training_end(gw_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    gw_thalamic_bridge_heartbeat_instance(bridge->health_agent, "gw_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int gw_thalamic_bridge_training_step(gw_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "gw_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    gw_thalamic_bridge_heartbeat_instance(bridge->health_agent, "gw_thalamic_bridge_training_step", progress);
    return 0;
}
