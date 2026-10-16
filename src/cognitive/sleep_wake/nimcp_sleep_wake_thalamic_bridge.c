/**
 * @file nimcp_sleep_wake_thalamic_bridge.c
 * @brief Sleep-Wake-Thalamic Bridge Implementation
 */

#include "cognitive/sleep_wake/nimcp_sleep_wake_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(sleep_wake_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_sleep_wake_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_sleep_wake_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t sleep_wake_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_sleep_wake_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "sleep_wake_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "sleep_wake_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_sleep_wake_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_sleep_wake_thalamic_bridge_mesh_registry = registry;
    return err;
}

void sleep_wake_thalamic_bridge_mesh_unregister(void) {
    if (g_sleep_wake_thalamic_bridge_mesh_registry && g_sleep_wake_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_sleep_wake_thalamic_bridge_mesh_registry, g_sleep_wake_thalamic_bridge_mesh_id);
        g_sleep_wake_thalamic_bridge_mesh_id = 0;
        g_sleep_wake_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from sleep_wake_thalamic_bridge module (instance-level) */
static inline void sleep_wake_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_sleep_wake_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_sleep_wake_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_sleep_wake_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SLEEP_WAKE_THALAMIC_BRIDGE"


struct sleep_wake_thalamic_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* sleep_wake;
    thalamic_router_t* router;
    sleep_wake_thalamic_config_t config;
    sleep_wake_thalamic_stats_t stats;
    float attention_weight;
};

sleep_wake_thalamic_config_t sleep_wake_thalamic_default_config(void) {
    sleep_wake_thalamic_config_t cfg = {
        .enable_arousal_modulation = true,
        .enable_transition_gating = true,
        .min_arousal_threshold = 0.3f,
        .transition_threshold = 0.5f
    };
    return cfg;
}

sleep_wake_thalamic_bridge_t* sleep_wake_thalamic_bridge_create(void* sleep_wake, thalamic_router_t* router, const sleep_wake_thalamic_config_t* config) {
    sleep_wake_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(sleep_wake_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "sleep_wake_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }
    bridge->sleep_wake = sleep_wake;
    bridge->router = router;
    bridge->config = config ? *config : sleep_wake_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "sleep_wake_thalamic");
    return bridge;
}

void sleep_wake_thalamic_bridge_destroy(sleep_wake_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "sleep_wake_thalamic");
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int sleep_wake_thalamic_bridge_reset(sleep_wake_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_thalamic_route_arousal(sleep_wake_thalamic_bridge_t* bridge, const sleep_wake_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_route_arousal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.arousal_updates++;
    bridge->stats.avg_arousal_level = (bridge->stats.avg_arousal_level * (bridge->stats.arousal_updates - 1) +
                                       signal->arousal_level) / bridge->stats.arousal_updates;
    if (signal->signal_type == SLEEP_WAKE_SIGNAL_TRANSITION) {
        bridge->stats.state_transitions++;
    }
    if (signal->signal_type == SLEEP_WAKE_SIGNAL_CIRCADIAN) {
        bridge->stats.circadian_updates++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_thalamic_modulate_gating(sleep_wake_thalamic_bridge_t* bridge, float arousal_level) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_modulate_gating: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    /* Modulate attention based on arousal level */
    if (bridge->config.enable_arousal_modulation) {
        bridge->attention_weight = arousal_level < 0.0f ? 0.0f : (arousal_level > 1.0f ? 1.0f : arousal_level);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_thalamic_set_attention(sleep_wake_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_thalamic_get_attention(const sleep_wake_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int sleep_wake_thalamic_bridge_get_stats(const sleep_wake_thalamic_bridge_t* bridge, sleep_wake_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sleep_wake_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int sleep_wake_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Sleep_Wake_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Sleep_Wake_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Sleep_Wake_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void sleep_wake_thalamic_bridge_set_instance_health_agent(sleep_wake_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "sleep_wake_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int sleep_wake_thalamic_bridge_training_begin(sleep_wake_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    sleep_wake_thalamic_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int sleep_wake_thalamic_bridge_training_end(sleep_wake_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    sleep_wake_thalamic_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int sleep_wake_thalamic_bridge_training_step(sleep_wake_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sleep_wake_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    sleep_wake_thalamic_bridge_heartbeat_instance(bridge->health_agent, "sleep_wake_thalamic_bridge_training_step", progress);
    return 0;
}
