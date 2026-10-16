/**
 * @file nimcp_ethics_thalamic_bridge.c
 * @brief Ethics-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/ethics/nimcp_ethics_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ethics_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_ethics_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_ethics_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t ethics_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_ethics_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "ethics_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "ethics_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_ethics_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_ethics_thalamic_bridge_mesh_registry = registry;
    return err;
}

void ethics_thalamic_bridge_mesh_unregister(void) {
    if (g_ethics_thalamic_bridge_mesh_registry && g_ethics_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_ethics_thalamic_bridge_mesh_registry, g_ethics_thalamic_bridge_mesh_id);
        g_ethics_thalamic_bridge_mesh_id = 0;
        g_ethics_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void ethics_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_ethics_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_ethics_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_ethics_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "ETHICS_THALAMIC_BRIDGE"


struct ethics_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* ethics;
    thalamic_router_t* router;
    ethics_thalamic_config_t config;
    ethics_thalamic_stats_t stats;
    float attention_weight;
};

ethics_thalamic_config_t ethics_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_ethics_thalamic_defa", 0.0f);


    ethics_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_emotional_boost = true,
        .min_moral_salience = 0.3f,
        .violation_boost = 0.3f
    };
    return cfg;
}

ethics_thalamic_bridge_t* ethics_thalamic_bridge_create(void* ethics, thalamic_router_t* router, const ethics_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_create", 0.0f);


    ethics_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(ethics_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->ethics = ethics;
    bridge->router = router;
    bridge->config = config ? *config : ethics_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "ethics_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "ethics_thalamic");
    return bridge;
}

void ethics_thalamic_bridge_destroy(ethics_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int ethics_thalamic_bridge_reset(ethics_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ethics_thalamic_route_judgment(ethics_thalamic_bridge_t* bridge, const ethics_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_route_judgment: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_ethics_thalamic_rout", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->moral_salience < bridge->config.min_moral_salience) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.judgments_routed++;
    bridge->stats.avg_moral_salience = (bridge->stats.avg_moral_salience * (bridge->stats.judgments_routed - 1) +
                                        signal->moral_salience) / bridge->stats.judgments_routed;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ethics_thalamic_route_violation(ethics_thalamic_bridge_t* bridge, const void* violation, float severity) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_route_violation: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_ethics_thalamic_rout", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.violations_flagged++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ethics_thalamic_set_attention(ethics_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_ethics_thalamic_set_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int ethics_thalamic_get_attention(const ethics_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_ethics_thalamic_get_", 0.0f);


    return 0;
}

int ethics_thalamic_bridge_get_stats(const ethics_thalamic_bridge_t* bridge, ethics_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ethics_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Ethics Thalamic Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int ethics_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    ethics_thalamic_bridge_heartbeat("ethics_thala_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Ethics_Thalamic_Bridge_Module");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                ethics_thalamic_bridge_heartbeat("ethics_thala_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            // No LOG_MODULE defined in this file, use direct printf or skip
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Ethics_Thalamic_Bridge_Module");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Ethics_Thalamic_Bridge_Module");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void ethics_thalamic_bridge_set_instance_health_agent(ethics_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "ethics_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int ethics_thalamic_bridge_training_begin(ethics_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    ethics_thalamic_bridge_heartbeat_instance(bridge->health_agent, "ethics_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int ethics_thalamic_bridge_training_end(ethics_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    ethics_thalamic_bridge_heartbeat_instance(bridge->health_agent, "ethics_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int ethics_thalamic_bridge_training_step(ethics_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "ethics_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    ethics_thalamic_bridge_heartbeat_instance(bridge->health_agent, "ethics_thalamic_bridge_training_step", progress);
    return 0;
}
