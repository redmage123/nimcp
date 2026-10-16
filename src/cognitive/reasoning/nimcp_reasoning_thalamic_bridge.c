/**
 * @file nimcp_reasoning_thalamic_bridge.c
 * @brief Reasoning-Thalamic Bridge Implementation
 */

#include "cognitive/reasoning/nimcp_reasoning_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reasoning_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

struct reasoning_thalamic_bridge {
    bridge_base_t base;
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* reasoning;
    thalamic_router_t* router;
    reasoning_thalamic_config_t config;
    reasoning_thalamic_stats_t stats;
    float attention_weight;
};

BRIDGE_DEFINE_SECURITY_SETTERS(reasoning_thalamic_bridge)

reasoning_thalamic_config_t reasoning_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_d", 0.0f);


    return (reasoning_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_depth_routing = true,
        .enable_confidence_boost = true,
        .min_urgency_threshold = 0.25f,
        .depth_boost = 1.3f
    };
}

reasoning_thalamic_bridge_t* reasoning_thalamic_bridge_create(
    void* reasoning,
    thalamic_router_t* router,
    const reasoning_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_create", 0.0f);


    reasoning_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(reasoning_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    if (bridge_base_init(&bridge->base, 0, "reasoning_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->reasoning = reasoning;
    bridge->router = router;
    bridge->config = config ? *config : reasoning_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void reasoning_thalamic_bridge_destroy(reasoning_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int reasoning_thalamic_bridge_reset(reasoning_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_thalamic_route_signal(
    reasoning_thalamic_bridge_t* bridge,
    const reasoning_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(reasoning_thalamic_signal_t));
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_urgency = signal->reasoning_urgency * bridge->attention_weight;

        if (bridge->config.enable_depth_routing && signal->inference_depth > 0.6f) {
            effective_urgency *= bridge->config.depth_boost;
            if (effective_urgency > 1.0f) effective_urgency = 1.0f;
        }

        if (effective_urgency < bridge->config.min_urgency_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case REASONING_SIGNAL_INFERENCE:
            bridge->stats.inferences_routed++;
            break;
        case REASONING_SIGNAL_DEDUCTION:
            bridge->stats.deductions++;
            break;
        case REASONING_SIGNAL_INDUCTION:
            bridge->stats.inductions++;
            break;
        case REASONING_SIGNAL_ANALOGY:
            bridge->stats.analogies++;
            break;
        case REASONING_SIGNAL_CONCLUSION:
            bridge->stats.conclusions++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "reasoning_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.inferences_routed + bridge->stats.deductions +
                     bridge->stats.inductions + bridge->stats.analogies +
                     bridge->stats.conclusions;
    if (total > 0) {
        bridge->stats.avg_inference_depth =
            (bridge->stats.avg_inference_depth * (total - 1) + signal->inference_depth) / total;
        bridge->stats.avg_confidence =
            (bridge->stats.avg_confidence * (total - 1) + signal->confidence) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_thalamic_route_inference(
    reasoning_thalamic_bridge_t* bridge,
    float depth,
    float confidence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_route_inference: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_r", 0.0f);


    reasoning_thalamic_signal_t signal = {
        .signal_type = REASONING_SIGNAL_INFERENCE,
        .reasoning_urgency = 0.5f + (depth * 0.3f),
        .inference_depth = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth),
        .confidence = confidence < 0.0f ? 0.0f : (confidence > 1.0f ? 1.0f : confidence),
        .complexity = depth,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return reasoning_thalamic_route_signal(bridge, &signal);
}

int reasoning_thalamic_route_conclusion(
    reasoning_thalamic_bridge_t* bridge,
    float confidence,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_route_conclusion: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_r", 0.0f);


    reasoning_thalamic_signal_t signal = {
        .signal_type = REASONING_SIGNAL_CONCLUSION,
        .reasoning_urgency = urgency < 0.0f ? 0.0f : (urgency > 1.0f ? 1.0f : urgency),
        .inference_depth = 1.0f,
        .confidence = confidence < 0.0f ? 0.0f : (confidence > 1.0f ? 1.0f : confidence),
        .complexity = 0.5f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return reasoning_thalamic_route_signal(bridge, &signal);
}

int reasoning_thalamic_set_attention(reasoning_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_thalamic_get_attention(reasoning_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_reasoning_thalamic_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_thalamic_bridge_get_stats(
    const reasoning_thalamic_bridge_t* bridge,
    reasoning_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "reasoning_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int reasoning_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_thalamic_bridge_heartbeat("reasoning_th_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Reasoning_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                reasoning_thalamic_bridge_heartbeat("reasoning_th_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Reasoning_Thalamic_Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Reasoning_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Reasoning_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void reasoning_thalamic_bridge_set_instance_health_agent(reasoning_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "reasoning_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_thalamic_bridge_training_begin(reasoning_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    reasoning_thalamic_bridge_heartbeat_instance(bridge->health_agent, "reasoning_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int reasoning_thalamic_bridge_training_end(reasoning_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    reasoning_thalamic_bridge_heartbeat_instance(bridge->health_agent, "reasoning_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int reasoning_thalamic_bridge_training_step(reasoning_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_thalamic_bridge_heartbeat_instance(bridge->health_agent, "reasoning_thalamic_bridge_training_step", progress);
    return 0;
}
