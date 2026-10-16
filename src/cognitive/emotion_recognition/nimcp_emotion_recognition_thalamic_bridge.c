/**
 * @file nimcp_emotion_recognition_thalamic_bridge.c
 * @brief Emotion Recognition-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/emotion_recognition/nimcp_emotion_recognition_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_recognition_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_recognition_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_recognition_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t emotion_recognition_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_recognition_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_recognition_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_recognition_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_recognition_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_recognition_thalamic_bridge_mesh_registry = registry;
    return err;
}

void emotion_recognition_thalamic_bridge_mesh_unregister(void) {
    if (g_emotion_recognition_thalamic_bridge_mesh_registry && g_emotion_recognition_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_recognition_thalamic_bridge_mesh_registry, g_emotion_recognition_thalamic_bridge_mesh_id);
        g_emotion_recognition_thalamic_bridge_mesh_id = 0;
        g_emotion_recognition_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_recognition_thalamic_bridge module (instance-level) */
static inline void emotion_recognition_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_recognition_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_recognition_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_recognition_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "EMOTION_RECOGNITION_THALAMIC_BRIDGE"


struct emotion_recognition_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* emotion_rec;
    thalamic_router_t* router;
    emotion_recognition_thalamic_config_t config;
    emotion_recognition_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent (Phase 8) */
};

emotion_recognition_thalamic_config_t emotion_recognition_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    emotion_recognition_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_threat_priority = true,
        .min_recognition_confidence = 0.3f,
        .threat_boost = 0.3f
    };
    return cfg;
}

emotion_recognition_thalamic_bridge_t* emotion_recognition_thalamic_bridge_create(void* emotion_rec, thalamic_router_t* router, const emotion_recognition_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_create", 0.0f);


    emotion_recognition_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_recognition_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->emotion_rec = emotion_rec;
    bridge->router = router;
    bridge->config = config ? *config : emotion_recognition_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "emotion_recognition_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "emotion_recognition_thalamic");
    return bridge;
}

void emotion_recognition_thalamic_bridge_destroy(emotion_recognition_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_destroy", 0.0f);

    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int emotion_recognition_thalamic_bridge_reset(emotion_recognition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_recognition_thalamic_route_recognition(emotion_recognition_thalamic_bridge_t* bridge, const emotion_recognition_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_route_recognition: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->recognition_confidence < bridge->config.min_recognition_confidence) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.recognitions_routed++;
    bridge->stats.avg_recognition_confidence = (bridge->stats.avg_recognition_confidence * (bridge->stats.recognitions_routed - 1) +
                                                signal->recognition_confidence) / bridge->stats.recognitions_routed;
    /* Threat detection based on high emotional intensity with threat priority enabled */
    if (bridge->config.enable_threat_priority && signal->emotional_intensity > 0.7f) {
        bridge->stats.threat_detections++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_recognition_thalamic_route_context(emotion_recognition_thalamic_bridge_t* bridge, const void* context, float relevance) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_route_context: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.context_integrations++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_recognition_thalamic_set_attention(emotion_recognition_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_recognition_thalamic_get_attention(const emotion_recognition_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_emotion_recognition_", 0.0f);


    return 0;
}

int emotion_recognition_thalamic_bridge_get_stats(const emotion_recognition_thalamic_bridge_t* bridge, emotion_recognition_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_recognition_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_recognition_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Recognition_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_recognition_thalamic_bridge_heartbeat("emotion_reco_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Recognition_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Recognition_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Stubs
 * ============================================================================ */

void emotion_recognition_thalamic_bridge_set_instance_health_agent(emotion_recognition_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

int emotion_recognition_thalamic_bridge_training_begin(emotion_recognition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_recognition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "erec_thal_training_begin", 0.0f);
    return 0;
}

int emotion_recognition_thalamic_bridge_training_end(emotion_recognition_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_recognition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "erec_thal_training_end", 1.0f);
    return 0;
}

int emotion_recognition_thalamic_bridge_training_step(emotion_recognition_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_recognition_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_recognition_thalamic_bridge_heartbeat_instance(bridge->health_agent, "erec_thal_training_step", progress);
    return 0;
}
