/**
 * @file nimcp_emotion_thalamic_bridge.c
 * @brief Emotion-Thalamic Bridge Implementation
 */

#include "cognitive/emotion/nimcp_emotion_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t emotion_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_thalamic_bridge_mesh_registry = registry;
    return err;
}

void emotion_thalamic_bridge_mesh_unregister(void) {
    if (g_emotion_thalamic_bridge_mesh_registry && g_emotion_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_thalamic_bridge_mesh_registry, g_emotion_thalamic_bridge_mesh_id);
        g_emotion_thalamic_bridge_mesh_id = 0;
        g_emotion_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_thalamic_bridge module (instance-level) */
static inline void emotion_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


struct emotion_thalamic_bridge {
    bridge_base_t base;  /* MUST be first - provides mutex protection */
    void* emotion;
    thalamic_router_t* router;
    emotion_thalamic_config_t config;
    emotion_thalamic_stats_t stats;
    float attention_weight;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

emotion_thalamic_config_t emotion_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_def", 0.0f);


    return (emotion_thalamic_config_t){
        .enable_attention_gating = true,
        .enable_intensity_boost = true,
        .enable_regulation_priority = true,
        .min_intensity_threshold = 0.15f,
        .regulation_boost = 1.3f
    };
}

emotion_thalamic_bridge_t* emotion_thalamic_bridge_create(
    void* emotion,
    thalamic_router_t* router,
    const emotion_thalamic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_create", 0.0f);


    emotion_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "emotion_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->emotion = emotion;
    bridge->router = router;
    bridge->config = config ? *config : emotion_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void emotion_thalamic_bridge_destroy(emotion_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_destroy", 0.0f);


    if (bridge) {
        if (bridge->base.mutex) {
            bridge_base_cleanup(&bridge->base);
        }
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int emotion_thalamic_bridge_reset(emotion_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_thalamic_route_signal(
    emotion_thalamic_bridge_t* bridge,
    const emotion_thalamic_signal_t* signal
) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_route_signal: required parameter is NULL (bridge, signal)");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, signal, sizeof(emotion_thalamic_signal_t));

    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_rou", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->config.enable_attention_gating) {
        float effective_intensity = signal->emotional_intensity * bridge->attention_weight;

        if (bridge->config.enable_regulation_priority &&
            signal->signal_type == EMOTION_SIGNAL_REGULATION) {
            effective_intensity *= bridge->config.regulation_boost;
            if (effective_intensity > 1.0f) effective_intensity = 1.0f;
        }

        if (effective_intensity < bridge->config.min_intensity_threshold) {
            bridge->stats.signals_gated++;
            nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    switch (signal->signal_type) {
        case EMOTION_SIGNAL_AROUSAL:
            bridge->stats.arousal_signals++;
            break;
        case EMOTION_SIGNAL_VALENCE:
            bridge->stats.valence_updates++;
            break;
        case EMOTION_SIGNAL_REGULATION:
            bridge->stats.regulations_attempted++;
            break;
        case EMOTION_SIGNAL_EXPRESSION:
            bridge->stats.expressions_routed++;
            break;
        default:
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "emotion_thalamic_route_signal: operation failed");
            return -1;
    }

    uint64_t total = bridge->stats.arousal_signals + bridge->stats.valence_updates +
                     bridge->stats.regulations_attempted + bridge->stats.expressions_routed;
    if (total > 0) {
        bridge->stats.avg_intensity =
            (bridge->stats.avg_intensity * (total - 1) + signal->emotional_intensity) / total;
        bridge->stats.avg_arousal =
            (bridge->stats.avg_arousal * (total - 1) + signal->arousal_level) / total;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_thalamic_route_arousal(
    emotion_thalamic_bridge_t* bridge,
    float intensity,
    float arousal
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_route_arousal: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_rou", 0.0f);


    emotion_thalamic_signal_t signal = {
        .signal_type = EMOTION_SIGNAL_AROUSAL,
        .emotional_intensity = intensity < 0.0f ? 0.0f : (intensity > 1.0f ? 1.0f : intensity),
        .valence = 0.0f,
        .arousal_level = arousal < 0.0f ? 0.0f : (arousal > 1.0f ? 1.0f : arousal),
        .regulation_effort = 0.0f,
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return emotion_thalamic_route_signal(bridge, &signal);
}

int emotion_thalamic_route_regulation(
    emotion_thalamic_bridge_t* bridge,
    float effort,
    float urgency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_route_regulation: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_rou", 0.0f);


    emotion_thalamic_signal_t signal = {
        .signal_type = EMOTION_SIGNAL_REGULATION,
        .emotional_intensity = urgency,
        .valence = 0.0f,
        .arousal_level = 0.5f,
        .regulation_effort = effort < 0.0f ? 0.0f : (effort > 1.0f ? 1.0f : effort),
        .content = NULL,
        .content_size = 0,
        .timestamp_us = nimcp_time_get_us()
    };

    return emotion_thalamic_route_signal(bridge, &signal);
}

int emotion_thalamic_set_attention(emotion_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_set", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_thalamic_get_attention(emotion_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_emotion_thalamic_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_thalamic_bridge_get_stats(
    const emotion_thalamic_bridge_t* bridge,
    emotion_thalamic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Emotion Thalamic Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int emotion_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    emotion_thalamic_bridge_heartbeat("emotion_thal_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_thalamic_bridge_heartbeat("emotion_thal_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Emotion Thalamic Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Thalamic_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Thalamic_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void emotion_thalamic_bridge_set_instance_health_agent(emotion_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int emotion_thalamic_bridge_training_begin(emotion_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_thal_training_begin", 0.0f);
    return 0;
}

int emotion_thalamic_bridge_training_end(emotion_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_thal_training_end", 1.0f);
    return 0;
}

int emotion_thalamic_bridge_training_step(emotion_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_thal_training_step", progress);
    return 0;
}

/* ============================================================================
 * Security Integration (BBB)
 * ============================================================================ */
BRIDGE_DEFINE_SECURITY_SETTERS(emotion_thalamic_bridge)
