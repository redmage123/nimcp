/**
 * @file nimcp_working_memory_thalamic_bridge.c
 * @brief Working Memory-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/working_memory/nimcp_working_memory_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(working_memory_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
struct working_memory_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* working_memory;
    thalamic_router_t* router;
    working_memory_thalamic_config_t config;
    working_memory_thalamic_stats_t stats;
    float attention_weight;
};

BRIDGE_DEFINE_SECURITY_SETTERS(working_memory_thalamic_bridge)

working_memory_thalamic_config_t working_memory_thalamic_default_config(void) {
    return (working_memory_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_priority_routing = true,
        .enable_capacity_check = true,
        .min_urgency_threshold = 0.2f,
        .priority_boost = 1.3f
    };
}

working_memory_thalamic_bridge_t* working_memory_thalamic_bridge_create(
    void* working_memory,
    thalamic_router_t* router,
    const working_memory_thalamic_config_t* config
) {
    working_memory_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(working_memory_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->working_memory = working_memory;
    bridge->router = router;
    bridge->config = config ? *config : working_memory_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "working_memory_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    return bridge;
}

void working_memory_thalamic_bridge_destroy(working_memory_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int working_memory_thalamic_bridge_reset(working_memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int working_memory_thalamic_route_signal(
    working_memory_thalamic_bridge_t* bridge,
    const working_memory_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(*signal));

    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->wm_urgency * bridge->attention_weight;

        /* High priority items get boost */
        if (bridge->config.enable_priority_routing && signal->item_priority > 0.7f) {
            effective_urgency *= bridge->config.priority_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case WM_SIGNAL_ENCODE:
            bridge->stats.encodings++;
            break;
        case WM_SIGNAL_MAINTAIN:
            bridge->stats.maintenances++;
            break;
        case WM_SIGNAL_UPDATE:
            bridge->stats.updates++;
            break;
        case WM_SIGNAL_RETRIEVE:
            bridge->stats.retrievals++;
            break;
        case WM_SIGNAL_CLEAR:
            bridge->stats.clears++;
            break;
        default:
            nimcp_platform_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "working_memory_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.encodings + bridge->stats.maintenances +
                     bridge->stats.updates + bridge->stats.retrievals +
                     bridge->stats.clears;
    if (total > 0) {
        bridge->stats.avg_capacity_used =
            (bridge->stats.avg_capacity_used * (total - 1) + signal->capacity_used) / total;
        bridge->stats.avg_item_priority =
            (bridge->stats.avg_item_priority * (total - 1) + signal->item_priority) / total;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}

int working_memory_thalamic_route_encode(
    working_memory_thalamic_bridge_t* bridge,
    float priority,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_route_encode: bridge is NULL");
        return -1;
    }

    working_memory_thalamic_signal_t signal = {
        .signal_type = WM_SIGNAL_ENCODE,
        .wm_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .capacity_used = 0.5f,
        .item_priority = priority < 0.0f ? 0.0f : (priority > 1.0f ? 1.0f : priority),
        .decay_rate = 0.1f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return working_memory_thalamic_route_signal(bridge, &signal);
}

int working_memory_thalamic_route_update(
    working_memory_thalamic_bridge_t* bridge,
    float priority,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_route_update: bridge is NULL");
        return -1;
    }

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "working_memory_thalamic_route_update");
    BRIDGE_LGSS_GATE(bridge, "working_memory_thalamic_route_update");

    working_memory_thalamic_signal_t signal = {
        .signal_type = WM_SIGNAL_UPDATE,
        .wm_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .capacity_used = 0.5f,
        .item_priority = priority < 0.0f ? 0.0f : (priority > 1.0f ? 1.0f : priority),
        .decay_rate = 0.1f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return working_memory_thalamic_route_signal(bridge, &signal);
}

int working_memory_thalamic_set_attention(working_memory_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int working_memory_thalamic_get_attention(const working_memory_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    BRIDGE_BBB_VALIDATE(bridge, attention, sizeof(*attention));
    return 0;
}

int working_memory_thalamic_bridge_get_stats(
    const working_memory_thalamic_bridge_t* bridge,
    working_memory_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "working_memory_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int working_memory_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    const kg_entity_t* self = kg_reader_get_entity(kg, "Working_Memory_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            NIMCP_LOGGING_DEBUG("Working memory thalamic bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Working_Memory_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Working_Memory_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void working_memory_thalamic_bridge_set_instance_health_agent(working_memory_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "working_memory_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int working_memory_thalamic_bridge_training_begin(working_memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    working_memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "working_memory_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int working_memory_thalamic_bridge_training_end(working_memory_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    working_memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "working_memory_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int working_memory_thalamic_bridge_training_step(working_memory_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "working_memory_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    working_memory_thalamic_bridge_heartbeat_instance(bridge->health_agent, "working_memory_thalamic_bridge_training_step", progress);
    return 0;
}
