/**
 * @file nimcp_self_model_thalamic_bridge.c
 * @brief Self-Model-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/self_model/nimcp_self_model_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(self_model_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_self_model_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_self_model_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t self_model_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_self_model_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "self_model_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "self_model_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_self_model_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_self_model_thalamic_bridge_mesh_registry = registry;
    return err;
}

void self_model_thalamic_bridge_mesh_unregister(void) {
    if (g_self_model_thalamic_bridge_mesh_registry && g_self_model_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_self_model_thalamic_bridge_mesh_registry, g_self_model_thalamic_bridge_mesh_id);
        g_self_model_thalamic_bridge_mesh_id = 0;
        g_self_model_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from self_model_thalamic_bridge module (instance-level) */
static inline void self_model_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_self_model_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_self_model_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_self_model_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct self_model_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* self_model;
    thalamic_router_t* router;
    self_model_thalamic_config_t config;
    self_model_thalamic_stats_t stats;
    float attention_weight;
};

BRIDGE_DEFINE_SECURITY_SETTERS(self_model_thalamic_bridge)

self_model_thalamic_config_t self_model_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);


    return (self_model_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_conflict_priority = true,
        .min_urgency_threshold = 0.2f,
        .conflict_boost = 1.5f
    };
}

self_model_thalamic_bridge_t* self_model_thalamic_bridge_create(
    void* self_model,
    thalamic_router_t* router,
    const self_model_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_create", 0.0f);


    self_model_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(self_model_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->self_model = self_model;
    bridge->router = router;
    bridge->config = config ? *config : self_model_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "self_model_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    return bridge;
}

void self_model_thalamic_bridge_destroy(self_model_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int self_model_thalamic_bridge_reset(self_model_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_thalamic_route_signal(
    self_model_thalamic_bridge_t* bridge,
    const self_model_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(self_model_thalamic_signal_t));

    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->model_urgency * bridge->attention_weight;

        /* Conflicts get priority boost (require attention) */
        if (bridge->config.enable_conflict_priority &&
            signal->signal_type == SELF_MODEL_SIGNAL_CONFLICT) {
            effective_urgency *= bridge->config.conflict_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case SELF_MODEL_SIGNAL_UPDATE:
            bridge->stats.updates_routed++;
            break;
        case SELF_MODEL_SIGNAL_PREDICTION:
            bridge->stats.predictions++;
            break;
        case SELF_MODEL_SIGNAL_CONFLICT:
            bridge->stats.conflicts_detected++;
            break;
        case SELF_MODEL_SIGNAL_INTEGRATION:
            bridge->stats.integrations++;
            break;
        default:
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_model_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.updates_routed + bridge->stats.predictions +
                     bridge->stats.conflicts_detected + bridge->stats.integrations;
    if (total > 0) {
        bridge->stats.avg_coherence =
            (bridge->stats.avg_coherence * (total - 1) + signal->coherence) / total;
        bridge->stats.avg_prediction_error =
            (bridge->stats.avg_prediction_error * (total - 1) + signal->prediction_error) / total;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int self_model_thalamic_route_update(
    self_model_thalamic_bridge_t* bridge,
    float coherence,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_route_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);


    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "self_model_thalamic_route_update");
    BRIDGE_LGSS_GATE(bridge, "self_model_thalamic_route_update");

    self_model_thalamic_signal_t signal = {
        .signal_type = SELF_MODEL_SIGNAL_UPDATE,
        .model_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .coherence = coherence < 0.0f ? 0.0f : (coherence > 1.0f ? 1.0f : coherence),
        .prediction_error = 0.0f,
        .self_relevance = 0.8f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return self_model_thalamic_route_signal(bridge, &signal);
}

int self_model_thalamic_route_conflict(
    self_model_thalamic_bridge_t* bridge,
    float prediction_error,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_route_conflict: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);


    self_model_thalamic_signal_t signal = {
        .signal_type = SELF_MODEL_SIGNAL_CONFLICT,
        .model_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .coherence = 1.0f - prediction_error,
        .prediction_error = prediction_error < 0.0f ? 0.0f : (prediction_error > 1.0f ? 1.0f : prediction_error),
        .self_relevance = 1.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return self_model_thalamic_route_signal(bridge, &signal);
}

int self_model_thalamic_set_attention(self_model_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int self_model_thalamic_get_attention(const self_model_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_self_model_thalamic_", 0.0f);


    return 0;
}

int self_model_thalamic_bridge_get_stats(
    const self_model_thalamic_bridge_t* bridge,
    self_model_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_model_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_get_stats", 0.0f);


    return 0;
}

/* ========================================================================
 * KG SELF-AWARENESS INTEGRATION
 * ======================================================================== */

/**
 * WHAT: Query knowledge graph for self-knowledge about self-model thalamic bridge
 * WHY:  Enable self-awareness - module can introspect its own capabilities
 * HOW:  Query entity by name, get relations from/to
 *
 * @param kg Knowledge graph reader
 * @return 1 if entity found, 0 if not
 */
int self_model_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Query our own entity from the knowledge graph */
    /* Phase 8: Heartbeat at operation start */
    self_model_thalamic_bridge_heartbeat("self_model_t_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Self_Model_Thalamic_Bridge");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                self_model_thalamic_bridge_heartbeat("self_model_t_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Self-model thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Self_Model_Thalamic_Bridge");
    if (connections) {
        NIMCP_LOGGING_DEBUG("Self-model thalamic bridge has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Self_Model_Thalamic_Bridge");
    if (incoming) {
        NIMCP_LOGGING_DEBUG("Self-model thalamic bridge has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_model_thalamic_bridge_set_instance_health_agent(self_model_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "self_model_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_model_thalamic_bridge_training_begin(self_model_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    self_model_thalamic_bridge_heartbeat_instance(bridge->health_agent, "self_model_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int self_model_thalamic_bridge_training_end(self_model_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    self_model_thalamic_bridge_heartbeat_instance(bridge->health_agent, "self_model_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int self_model_thalamic_bridge_training_step(self_model_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_model_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_model_thalamic_bridge_heartbeat_instance(bridge->health_agent, "self_model_thalamic_bridge_training_step", progress);
    return 0;
}
