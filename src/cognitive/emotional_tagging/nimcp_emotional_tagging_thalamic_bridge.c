/**
 * @file nimcp_emotional_tagging_thalamic_bridge.c
 * @brief Emotional Tagging-Thalamic Bridge Implementation
 */

#include "cognitive/emotional_tagging/nimcp_emotional_tagging_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(emotional_tagging_thalamic_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

#define LOG_MODULE "EMOTIONAL_TAGGING_THALAMIC_BRIDGE"


struct emotional_tagging_thalamic_bridge {
    bridge_base_t base;  /* MUST be first - provides mutex protection */
    void* emotional_tagging;
    thalamic_router_t* router;
    emotional_tagging_thalamic_config_t config;
    emotional_tagging_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent (Phase 8) */
};

emotional_tagging_thalamic_config_t emotional_tagging_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_emotional_tagging_th", 0.0f);


    return (emotional_tagging_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_intensity_boost = true,
        .min_intensity_threshold = 0.2f,
        .salience_boost_factor = 1.4f
    };
}

emotional_tagging_thalamic_bridge_t* emotional_tagging_thalamic_bridge_create(
    void* emotional_tagging,
    thalamic_router_t* router,
    const emotional_tagging_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_create", 0.0f);


    emotional_tagging_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(emotional_tagging_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "emotional_tagging_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->emotional_tagging = emotional_tagging;
    bridge->router = router;
    bridge->config = config ? *config : emotional_tagging_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void emotional_tagging_thalamic_bridge_destroy(emotional_tagging_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_destroy", 0.0f);


    if (bridge) {
        if (bridge->base.mutex) {
            bridge_base_cleanup(&bridge->base);
        }
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int emotional_tagging_thalamic_bridge_reset(emotional_tagging_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_tagging_thalamic_route_signal(
    emotional_tagging_thalamic_bridge_t* bridge,
    const emotional_tagging_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_emotional_tagging_th", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_intensity = signal->emotional_intensity * bridge->attention_weight;

        if (bridge->config.enable_intensity_boost &&
            signal->signal_type == ETAG_SIGNAL_SALIENCE_BOOST) {
            effective_intensity *= bridge->config.salience_boost_factor;
            if (effective_intensity > 1.0f) effective_intensity = 1.0f;
        }

        if (effective_intensity < bridge->config.min_intensity_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case ETAG_SIGNAL_TAG_APPLY:
            bridge->stats.tags_applied++;
            break;
        case ETAG_SIGNAL_TAG_UPDATE:
            bridge->stats.tags_updated++;
            break;
        case ETAG_SIGNAL_TAG_RETRIEVE:
            bridge->stats.retrievals++;
            break;
        case ETAG_SIGNAL_SALIENCE_BOOST:
            bridge->stats.salience_boosts++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotional_tagging_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.tags_applied + bridge->stats.tags_updated +
                     bridge->stats.retrievals + bridge->stats.salience_boosts;
    if (total > 0) {
        bridge->stats.avg_emotional_intensity =
            (bridge->stats.avg_emotional_intensity * (total - 1) +
             signal->emotional_intensity) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_tagging_thalamic_apply_tag(
    emotional_tagging_thalamic_bridge_t* bridge,
    float intensity,
    float valence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_apply_tag: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_emotional_tagging_th", 0.0f);


    emotional_tagging_thalamic_signal_t signal = {
        .signal_type = ETAG_SIGNAL_TAG_APPLY,
        .emotional_intensity = intensity < 0.0f ? 0.0f : (intensity > 1.0f ? 1.0f : intensity),
        .valence = valence < -1.0f ? -1.0f : (valence > 1.0f ? 1.0f : valence),
        .memory_strength = 0.7f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return emotional_tagging_thalamic_route_signal(bridge, &signal);
}

int emotional_tagging_thalamic_set_attention(emotional_tagging_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_emotional_tagging_th", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_tagging_thalamic_get_attention(emotional_tagging_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_emotional_tagging_th", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotional_tagging_thalamic_bridge_get_stats(
    const emotional_tagging_thalamic_bridge_t* bridge,
    emotional_tagging_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotional_tagging_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotional_tagging_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotional_Tagging_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotional_tagging_thalamic_bridge_heartbeat("emotional_ta_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotional_Tagging_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotional_Tagging_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}


void emotional_tagging_thalamic_bridge_set_instance_health_agent(emotional_tagging_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int emotional_tagging_thalamic_bridge_training_begin(emotional_tagging_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    emotional_tagging_thalamic_bridge_heartbeat_instance(bridge->health_agent, "etag_thal_training_begin", 0.0f);
    return 0;
}

int emotional_tagging_thalamic_bridge_training_end(emotional_tagging_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    emotional_tagging_thalamic_bridge_heartbeat_instance(bridge->health_agent, "etag_thal_training_end", 1.0f);
    return 0;
}

int emotional_tagging_thalamic_bridge_training_step(emotional_tagging_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotional_tagging_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    emotional_tagging_thalamic_bridge_heartbeat_instance(bridge->health_agent, "etag_thal_training_step", progress);
    return 0;
}
