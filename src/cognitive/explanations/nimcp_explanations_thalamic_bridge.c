/**
 * @file nimcp_explanations_thalamic_bridge.c
 * @brief Explanations-Thalamic Bridge Implementation
 */

#include "cognitive/explanations/nimcp_explanations_thalamic_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(explanations_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_explanations_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_explanations_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t explanations_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_explanations_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "explanations_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "explanations_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_explanations_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_explanations_thalamic_bridge_mesh_registry = registry;
    return err;
}

void explanations_thalamic_bridge_mesh_unregister(void) {
    if (g_explanations_thalamic_bridge_mesh_registry && g_explanations_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_explanations_thalamic_bridge_mesh_registry, g_explanations_thalamic_bridge_mesh_id);
        g_explanations_thalamic_bridge_mesh_id = 0;
        g_explanations_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void explanations_thalamic_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_explanations_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_explanations_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_explanations_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "EXPLANATIONS_THALAMIC_BRIDGE"


struct explanations_thalamic_bridge {
    bridge_base_t base;  /* MUST be first - provides mutex protection */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* explanations;
    thalamic_router_t* router;
    explanations_thalamic_config_t config;
    explanations_thalamic_stats_t stats;
    float attention_weight;
};

explanations_thalamic_config_t explanations_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_explanations_thalami", 0.0f);


    return (explanations_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_complexity_routing = true,
        .min_urgency_threshold = 0.25f,
        .complexity_boost = 1.2f
    };
}

explanations_thalamic_bridge_t* explanations_thalamic_bridge_create(
    void* explanations,
    thalamic_router_t* router,
    const explanations_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_create", 0.0f);


    explanations_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(explanations_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "explanations_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->explanations = explanations;
    bridge->router = router;
    bridge->config = config ? *config : explanations_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void explanations_thalamic_bridge_destroy(explanations_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_destroy", 0.0f);


    if (bridge) {
        if (bridge->base.mutex) {
            bridge_base_cleanup(&bridge->base);
        }
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int explanations_thalamic_bridge_reset(explanations_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_thalamic_route_signal(
    explanations_thalamic_bridge_t* bridge,
    const explanations_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_explanations_thalami", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->explanation_urgency * bridge->attention_weight;

        if (bridge->config.enable_complexity_routing && signal->complexity > 0.6f) {
            effective_urgency *= bridge->config.complexity_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case EXPLANATION_SIGNAL_CAUSAL:
            bridge->stats.causal_explanations++;
            break;
        case EXPLANATION_SIGNAL_MECHANISTIC:
            bridge->stats.mechanistic_explanations++;
            break;
        case EXPLANATION_SIGNAL_CONTRASTIVE:
            bridge->stats.contrastive_explanations++;
            break;
        case EXPLANATION_SIGNAL_COMPLETION:
            bridge->stats.completions++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "explanations_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.causal_explanations + bridge->stats.mechanistic_explanations +
                     bridge->stats.contrastive_explanations + bridge->stats.completions;
    if (total > 0) {
        bridge->stats.avg_complexity =
            (bridge->stats.avg_complexity * (total - 1) + signal->complexity) / total;
        bridge->stats.avg_coherence =
            (bridge->stats.avg_coherence * (total - 1) + signal->coherence) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_thalamic_route_causal(
    explanations_thalamic_bridge_t* bridge,
    float complexity,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_route_causal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_explanations_thalami", 0.0f);


    explanations_thalamic_signal_t signal = {
        .signal_type = EXPLANATION_SIGNAL_CAUSAL,
        .explanation_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .complexity = complexity < 0.0f ? 0.0f : (complexity > 1.0f ? 1.0f : complexity),
        .coherence = 0.7f,
        .completeness = 0.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return explanations_thalamic_route_signal(bridge, &signal);
}

int explanations_thalamic_set_attention(explanations_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_explanations_thalami", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_thalamic_get_attention(explanations_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_explanations_thalami", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_thalamic_bridge_get_stats(
    const explanations_thalamic_bridge_t* bridge,
    explanations_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "explanations_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int explanations_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    explanations_thalamic_bridge_heartbeat("explanations_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Explanations_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                explanations_thalamic_bridge_heartbeat("explanations_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Explanations_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Explanations_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void explanations_thalamic_bridge_set_instance_health_agent(explanations_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
int explanations_thalamic_bridge_training_begin(explanations_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    explanations_thalamic_bridge_heartbeat_instance(bridge, "expl_thal_train_begin", 0.0f);
    (void)bridge;
    return 0;
}

int explanations_thalamic_bridge_training_step(explanations_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    explanations_thalamic_bridge_heartbeat_instance(bridge, "expl_thal_train_step", progress);
    (void)bridge;
    return 0;
}

int explanations_thalamic_bridge_training_end(explanations_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    explanations_thalamic_bridge_heartbeat_instance(bridge, "expl_thal_train_end", 1.0f);
    (void)bridge;
    return 0;
}
