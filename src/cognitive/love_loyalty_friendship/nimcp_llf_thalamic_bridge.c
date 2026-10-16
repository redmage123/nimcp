/**
 * @file nimcp_llf_thalamic_bridge.c
 * @brief Love/Loyalty/Friendship-Thalamic Bridge Implementation
 */

#include "cognitive/love_loyalty_friendship/nimcp_llf_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(llf_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_llf_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_llf_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t llf_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_llf_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "llf_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "llf_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_llf_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_llf_thalamic_bridge_mesh_registry = registry;
    return err;
}

void llf_thalamic_bridge_mesh_unregister(void) {
    if (g_llf_thalamic_bridge_mesh_registry && g_llf_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_llf_thalamic_bridge_mesh_registry, g_llf_thalamic_bridge_mesh_id);
        g_llf_thalamic_bridge_mesh_id = 0;
        g_llf_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from llf_thalamic_bridge module (instance-level) */
static inline void llf_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_llf_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_llf_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_llf_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "LLF_THALAMIC_BRIDGE"


struct llf_thalamic_bridge {
    bridge_base_t base;
    void* llf;
    thalamic_router_t* router;
    llf_thalamic_config_t config;
    llf_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

llf_thalamic_config_t llf_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_llf_thalamic_default", 0.0f);


    llf_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_bond_strengthening = true,
        .min_bond_threshold = 0.3f,
        .trust_threshold = 0.5f
    };
    return cfg;
}

llf_thalamic_bridge_t* llf_thalamic_bridge_create(void* llf, thalamic_router_t* router, const llf_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_create", 0.0f);


    llf_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(llf_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "llf_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->llf = llf;
    bridge->router = router;
    bridge->config = config ? *config : llf_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "llf_thalamic");
    return bridge;
}

void llf_thalamic_bridge_destroy(llf_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "llf_thalamic");
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int llf_thalamic_bridge_reset(llf_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int llf_thalamic_route_attachment(llf_thalamic_bridge_t* bridge, const llf_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_route_attachment: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_llf_thalamic_route_a", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->bond_strength < bridge->config.min_bond_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.attachments_routed++;
    bridge->stats.avg_bond_strength = (bridge->stats.avg_bond_strength * (bridge->stats.attachments_routed - 1) +
                                       signal->bond_strength) / bridge->stats.attachments_routed;
    if (signal->signal_type == LLF_SIGNAL_TRUST) {
        bridge->stats.trust_updates++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int llf_thalamic_route_care(llf_thalamic_bridge_t* bridge, const void* target, float motivation) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_route_care: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_llf_thalamic_route_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.care_expressions++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int llf_thalamic_set_attention(llf_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_llf_thalamic_set_att", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int llf_thalamic_get_attention(const llf_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    *attention = bridge->attention_weight;
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_llf_thalamic_get_att", 0.0f);


    return 0;
}

int llf_thalamic_bridge_get_stats(const llf_thalamic_bridge_t* bridge, llf_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "llf_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int llf_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    llf_thalamic_bridge_heartbeat("llf_thalamic_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "LLF_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                llf_thalamic_bridge_heartbeat("llf_thalamic_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "LLF_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "LLF_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void llf_thalamic_bridge_set_instance_health_agent(llf_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "llf_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int llf_thalamic_bridge_training_begin(llf_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "llf_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    llf_thalamic_bridge_heartbeat_instance(bridge->health_agent, "llf_thalamic_training_begin", 0.0f);
    return 0;
}

int llf_thalamic_bridge_training_end(llf_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "llf_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    llf_thalamic_bridge_heartbeat_instance(bridge->health_agent, "llf_thalamic_training_end", 1.0f);
    return 0;
}

int llf_thalamic_bridge_training_step(llf_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "llf_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    llf_thalamic_bridge_heartbeat_instance(bridge->health_agent, "llf_thalamic_training_step", progress);
    return 0;
}
