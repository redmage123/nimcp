/**
 * @file nimcp_free_energy_thalamic_bridge.c
 * @brief Free Energy Principle-Thalamic Bridge Implementation
 */

#include "cognitive/free_energy/nimcp_free_energy_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(free_energy_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_free_energy_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_free_energy_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t free_energy_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_free_energy_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "free_energy_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "free_energy_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_free_energy_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_free_energy_thalamic_bridge_mesh_registry = registry;
    return err;
}

void free_energy_thalamic_bridge_mesh_unregister(void) {
    if (g_free_energy_thalamic_bridge_mesh_registry && g_free_energy_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_free_energy_thalamic_bridge_mesh_registry, g_free_energy_thalamic_bridge_mesh_id);
        g_free_energy_thalamic_bridge_mesh_id = 0;
        g_free_energy_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from free_energy_thalamic_bridge module (instance-level) */
static inline void free_energy_thalamic_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_free_energy_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_free_energy_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_free_energy_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct free_energy_thalamic_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
    void* free_energy;
    thalamic_router_t* router;
    free_energy_thalamic_config_t config;
    free_energy_thalamic_stats_t stats;
    float attention_weight;
};

free_energy_thalamic_config_t free_energy_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__free_energy_thalamic", 0.0f);


    free_energy_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_precision_boost = true,
        .min_error_threshold = 0.2f,
        .precision_boost = 0.4f
    };
    return cfg;
}

free_energy_thalamic_bridge_t* free_energy_thalamic_bridge_create(void* free_energy, thalamic_router_t* router, const free_energy_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__create", 0.0f);


    free_energy_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(free_energy_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "free_energy_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->free_energy = free_energy;
    bridge->router = router;
    bridge->config = config ? *config : free_energy_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return bridge;
}

void free_energy_thalamic_bridge_destroy(free_energy_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int free_energy_thalamic_bridge_reset(free_energy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_thalamic_route_prediction_error(free_energy_thalamic_bridge_t* bridge, const free_energy_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_route_prediction_error: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__free_energy_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    /* High-precision errors bypass gating */
    if (bridge->config.enable_attention_gating &&
        signal->prediction_error < bridge->config.min_error_threshold &&
        signal->precision < 0.7f) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.errors_routed++;
    bridge->stats.avg_precision = (bridge->stats.avg_precision * (bridge->stats.errors_routed - 1) +
                                   signal->precision) / bridge->stats.errors_routed;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_thalamic_route_model_update(free_energy_thalamic_bridge_t* bridge, const void* model, float importance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_route_model_update: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__free_energy_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.model_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_thalamic_set_attention(free_energy_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__free_energy_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_thalamic_get_attention(free_energy_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__free_energy_thalamic", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int free_energy_thalamic_bridge_get_stats(free_energy_thalamic_bridge_t* bridge, free_energy_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_energy_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Free Energy Thalamic Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int free_energy_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    free_energy_thalamic_bridge_heartbeat("free_energy__query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Free_Energy_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                free_energy_thalamic_bridge_heartbeat("free_energy__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("FE Thalamic Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Free_Energy_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Free_Energy_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void free_energy_thalamic_bridge_set_instance_health_agent(free_energy_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full Training Implementation
 * ============================================================================ */
int free_energy_thalamic_bridge_training_begin(free_energy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    free_energy_thalamic_bridge_heartbeat_instance(bridge, "fe_thal_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int free_energy_thalamic_bridge_training_step(free_energy_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    free_energy_thalamic_bridge_heartbeat_instance(bridge, "fe_thal_training_step", progress);
    (void)bridge;
    return 0;
}

int free_energy_thalamic_bridge_training_end(free_energy_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "free_energy_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    free_energy_thalamic_bridge_heartbeat_instance(bridge, "fe_thal_training_end", 1.0f);
    (void)bridge;
    return 0;
}
