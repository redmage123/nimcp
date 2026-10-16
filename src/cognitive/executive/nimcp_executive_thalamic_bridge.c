/**
 * @file nimcp_executive_thalamic_bridge.c
 * @brief Executive-Thalamic Bridge Implementation
 */

#include "cognitive/executive/nimcp_executive_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(executive_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_executive_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_executive_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t executive_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_executive_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "executive_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "executive_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_executive_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_executive_thalamic_bridge_mesh_registry = registry;
    return err;
}

void executive_thalamic_bridge_mesh_unregister(void) {
    if (g_executive_thalamic_bridge_mesh_registry && g_executive_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_executive_thalamic_bridge_mesh_registry, g_executive_thalamic_bridge_mesh_id);
        g_executive_thalamic_bridge_mesh_id = 0;
        g_executive_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void executive_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_executive_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_executive_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_executive_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

struct executive_thalamic_bridge {
    bridge_base_t base;  /* MUST be first - provides mutex protection */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* executive;
    thalamic_router_t* router;
    executive_thalamic_config_t config;
    executive_thalamic_stats_t stats;
    float attention_weight;
    float accumulated_switch_cost;
};

BRIDGE_DEFINE_SECURITY_SETTERS(executive_thalamic_bridge)

executive_thalamic_config_t executive_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_d", 0.0f);


    return (executive_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_load_routing = true,
        .enable_inhibition_priority = true,
        .min_urgency_threshold = 0.2f,
        .inhibition_boost = 1.4f,
        .switch_penalty = 0.15f
    };
}

executive_thalamic_bridge_t* executive_thalamic_bridge_create(
    void* executive,
    thalamic_router_t* router,
    const executive_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_create", 0.0f);


    executive_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(executive_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "executive_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->executive = executive;
    bridge->router = router;
    bridge->config = config ? *config : executive_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    bridge->accumulated_switch_cost = 0.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void executive_thalamic_bridge_destroy(executive_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_destroy", 0.0f);


    if (bridge) {
        if (bridge->base.mutex) {
            bridge_base_cleanup(&bridge->base);
        }
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int executive_thalamic_bridge_reset(executive_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    bridge->accumulated_switch_cost = 0.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_thalamic_route_signal(
    executive_thalamic_bridge_t* bridge,
    const executive_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_r", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(*signal));

    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->control_urgency * bridge->attention_weight;

        /* Inhibition gets priority boost */
        if (bridge->config.enable_inhibition_priority &&
            signal->signal_type == EXECUTIVE_SIGNAL_INHIBITION) {
            effective_urgency *= bridge->config.inhibition_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        /* Switching penalized by accumulated cost */
        if (signal->signal_type == EXECUTIVE_SIGNAL_SWITCHING) {
            effective_urgency -= bridge->accumulated_switch_cost;
            if (effective_urgency < 0.0f) effective_urgency = 0.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case EXECUTIVE_SIGNAL_INHIBITION:
            bridge->stats.inhibitions_routed++;
            break;
        case EXECUTIVE_SIGNAL_SWITCHING:
            bridge->stats.switches_executed++;
            bridge->accumulated_switch_cost += signal->switch_cost * bridge->config.switch_penalty;
            if (bridge->accumulated_switch_cost > 0.5f) bridge->accumulated_switch_cost = 0.5f;
            /* Update average switch cost */
            bridge->stats.avg_switch_cost =
                (bridge->stats.avg_switch_cost * (bridge->stats.switches_executed - 1) +
                 signal->switch_cost) / bridge->stats.switches_executed;
            break;
        case EXECUTIVE_SIGNAL_PLANNING:
            bridge->stats.plans_routed++;
            break;
        case EXECUTIVE_SIGNAL_MONITORING:
            bridge->stats.monitors_updated++;
            break;
        case EXECUTIVE_SIGNAL_DECISION:
            bridge->stats.decisions_routed++;
            bridge->accumulated_switch_cost *= 0.8f; /* Decay on decision */
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.inhibitions_routed + bridge->stats.switches_executed +
                     bridge->stats.plans_routed + bridge->stats.monitors_updated +
                     bridge->stats.decisions_routed;
    if (total > 0) {
        bridge->stats.avg_cognitive_load =
            (bridge->stats.avg_cognitive_load * (total - 1) + signal->cognitive_load) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_thalamic_route_inhibition(
    executive_thalamic_bridge_t* bridge,
    float strength,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_route_inhibition: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_r", 0.0f);


    executive_thalamic_signal_t signal = {
        .signal_type = EXECUTIVE_SIGNAL_INHIBITION,
        .control_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .cognitive_load = 0.5f,
        .inhibition_strength = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength),
        .switch_cost = 0.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return executive_thalamic_route_signal(bridge, &signal);
}

int executive_thalamic_route_switch(
    executive_thalamic_bridge_t* bridge,
    float cost,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_route_switch: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_r", 0.0f);


    executive_thalamic_signal_t signal = {
        .signal_type = EXECUTIVE_SIGNAL_SWITCHING,
        .control_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .cognitive_load = 0.7f,
        .inhibition_strength = 0.0f,
        .switch_cost = cost < 0.0f ? 0.0f : (cost > 1.0f ? 1.0f : cost),
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return executive_thalamic_route_signal(bridge, &signal);
}

int executive_thalamic_set_attention(executive_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_thalamic_get_attention(executive_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_executive_thalamic_g", 0.0f);

    BRIDGE_BBB_VALIDATE(bridge, attention, sizeof(*attention));

    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_thalamic_bridge_get_stats(
    const executive_thalamic_bridge_t* bridge,
    executive_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Executive Thalamic Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int executive_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    executive_thalamic_bridge_heartbeat("executive_th_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Executive_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                executive_thalamic_bridge_heartbeat("executive_th_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Executive Thalamic Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Executive_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Executive_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void executive_thalamic_bridge_set_instance_health_agent(executive_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "executive_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int executive_thalamic_bridge_training_begin(executive_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    executive_thalamic_bridge_heartbeat_instance(bridge->health_agent, "executive_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int executive_thalamic_bridge_training_end(executive_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    executive_thalamic_bridge_heartbeat_instance(bridge->health_agent, "executive_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int executive_thalamic_bridge_training_step(executive_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    executive_thalamic_bridge_heartbeat_instance(bridge->health_agent, "executive_thalamic_bridge_training_step", progress);
    return 0;
}
