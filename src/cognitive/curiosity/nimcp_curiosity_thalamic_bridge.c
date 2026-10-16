/**
 * @file nimcp_curiosity_thalamic_bridge.c
 * @brief Curiosity-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/curiosity/nimcp_curiosity_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(curiosity_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_curiosity_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_curiosity_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t curiosity_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_curiosity_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "curiosity_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "curiosity_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_curiosity_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_curiosity_thalamic_bridge_mesh_registry = registry;
    return err;
}

void curiosity_thalamic_bridge_mesh_unregister(void) {
    if (g_curiosity_thalamic_bridge_mesh_registry && g_curiosity_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_curiosity_thalamic_bridge_mesh_registry, g_curiosity_thalamic_bridge_mesh_id);
        g_curiosity_thalamic_bridge_mesh_id = 0;
        g_curiosity_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from curiosity_thalamic_bridge module (instance-level) */
static inline void curiosity_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_curiosity_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_curiosity_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_curiosity_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "CURIOSITY_THALAMIC_BRIDGE"


struct curiosity_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* curiosity;
    thalamic_router_t* router;
    curiosity_thalamic_config_t config;
    curiosity_thalamic_stats_t stats;
    float attention_weight;
};

curiosity_thalamic_config_t curiosity_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_curiosity_thalamic_d", 0.0f);


    curiosity_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_novelty_boost = true,
        .min_novelty_threshold = 0.3f,
        .exploration_threshold = 0.5f
    };
    return cfg;
}

curiosity_thalamic_bridge_t* curiosity_thalamic_bridge_create(void* curiosity, thalamic_router_t* router, const curiosity_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_create", 0.0f);


    curiosity_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(curiosity_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->curiosity = curiosity;
    bridge->router = router;
    bridge->config = config ? *config : curiosity_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "curiosity_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "curiosity_thalamic");
    return bridge;
}

void curiosity_thalamic_bridge_destroy(curiosity_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_destroy", 0.0f);


    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int curiosity_thalamic_bridge_reset(curiosity_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_thalamic_route_novelty(curiosity_thalamic_bridge_t* bridge, const curiosity_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_route_novelty: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_curiosity_thalamic_r", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->novelty_value < bridge->config.min_novelty_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.novelties_detected++;
    bridge->stats.avg_novelty_value = (bridge->stats.avg_novelty_value * (bridge->stats.novelties_detected - 1) +
                                       signal->novelty_value) / bridge->stats.novelties_detected;
    if (signal->signal_type == CURIOSITY_SIGNAL_INFORMATION) {
        bridge->stats.information_gains++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_thalamic_route_exploration(curiosity_thalamic_bridge_t* bridge, const void* target, float drive) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_route_exploration: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_curiosity_thalamic_r", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (drive >= bridge->config.exploration_threshold) {
        bridge->stats.explorations_initiated++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_thalamic_set_attention(curiosity_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_curiosity_thalamic_s", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int curiosity_thalamic_get_attention(const curiosity_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_curiosity_thalamic_g", 0.0f);


    return 0;
}

int curiosity_thalamic_bridge_get_stats(const curiosity_thalamic_bridge_t* bridge, curiosity_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "curiosity_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int curiosity_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    curiosity_thalamic_bridge_heartbeat("curiosity_th_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Curiosity_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                curiosity_thalamic_bridge_heartbeat("curiosity_th_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Curiosity_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Curiosity_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void curiosity_thalamic_bridge_set_instance_health_agent(curiosity_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "curiosity_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int curiosity_thalamic_bridge_training_begin(curiosity_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    curiosity_thalamic_bridge_heartbeat_instance(bridge->health_agent, "curiosity_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int curiosity_thalamic_bridge_training_end(curiosity_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    curiosity_thalamic_bridge_heartbeat_instance(bridge->health_agent, "curiosity_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int curiosity_thalamic_bridge_training_step(curiosity_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "curiosity_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    curiosity_thalamic_bridge_heartbeat_instance(bridge->health_agent, "curiosity_thalamic_bridge_training_step", progress);
    return 0;
}
