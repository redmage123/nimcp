/**
 * @file nimcp_autobio_thalamic_bridge.c
 * @brief Autobiographical Memory-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/autobiographical_memory/nimcp_autobio_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(autobio_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "AUTOBIO_THALAMIC_BRIDGE"


struct autobio_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* autobio;
    thalamic_router_t* router;
    autobio_thalamic_config_t config;
    autobio_thalamic_stats_t stats;
    float attention_weight;
};

autobio_thalamic_config_t autobio_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_autobio_thalamic_def", 0.0f);


    autobio_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_emotional_boost = true,
        .min_vividness_threshold = 0.3f,
        .emotional_threshold = 0.5f
    };
    return cfg;
}

autobio_thalamic_bridge_t* autobio_thalamic_bridge_create(void* autobio, thalamic_router_t* router, const autobio_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_create", 0.0f);


    autobio_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(autobio_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->autobio = autobio;
    bridge->router = router;
    bridge->config = config ? *config : autobio_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "autobio_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "autobio_thalamic");
    return bridge;
}

void autobio_thalamic_bridge_destroy(autobio_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int autobio_thalamic_bridge_reset(autobio_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_thalamic_route_recall(autobio_thalamic_bridge_t* bridge, const autobio_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_route_recall: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_autobio_thalamic_rou", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->vividness < bridge->config.min_vividness_threshold) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.recalls_routed++;
    bridge->stats.avg_vividness = (bridge->stats.avg_vividness * (bridge->stats.recalls_routed - 1) +
                                   signal->vividness) / bridge->stats.recalls_routed;
    if (signal->signal_type == AUTOBIO_SIGNAL_NARRATIVE) {
        bridge->stats.narrative_updates++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_thalamic_route_encoding(autobio_thalamic_bridge_t* bridge, const void* event, float importance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_route_encoding: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_autobio_thalamic_rou", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.encodings_routed++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_thalamic_set_attention(autobio_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_autobio_thalamic_set", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int autobio_thalamic_get_attention(const autobio_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_autobio_thalamic_get", 0.0f);


    return 0;
}

int autobio_thalamic_bridge_get_stats(const autobio_thalamic_bridge_t* bridge, autobio_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "autobio_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int autobio_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    autobio_thalamic_bridge_heartbeat("autobio_thal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Autobiographical_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                autobio_thalamic_bridge_heartbeat("autobio_thal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Autobiographical_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Autobiographical_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent Setter
 * ============================================================================ */

void autobio_thalamic_bridge_set_instance_health_agent(autobio_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
    g_autobio_thalamic_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("autobio_thalamic_bridge: instance health agent %s",
                        agent ? "set" : "cleared");
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int autobio_thalamic_bridge_training_begin(autobio_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    autobio_thalamic_bridge_heartbeat_instance(bridge->health_agent, "autobio_thl_training_begin", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->attention_weight = 1.0f;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("autobio_thalamic_bridge: training session begun, stats reset");
    return 0;
}

int autobio_thalamic_bridge_training_end(autobio_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    autobio_thalamic_bridge_heartbeat_instance(bridge->health_agent, "autobio_thl_training_end", 1.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float avg_attention = bridge->attention_weight;
    uint32_t recalls_routed = bridge->stats.recalls_routed;
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("autobio_thalamic_bridge: training ended, routed=%u avg_attention=%.3f threshold=%.3f",
                       recalls_routed,
                       avg_attention,
                       bridge->config.min_vividness_threshold);
    return 0;
}

int autobio_thalamic_bridge_training_step(autobio_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "autobio_thalamic_bridge_training_step: NULL argument");
        return -1;
    }

    float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    autobio_thalamic_bridge_heartbeat_instance(bridge->health_agent, "autobio_thl_training_step", p);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    float base_threshold = 0.3f;
    bridge->config.min_vividness_threshold = base_threshold * (1.0f - 0.15f * p);
    bridge->attention_weight = 1.0f - 0.1f * (1.0f - p);
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    return 0;
}
