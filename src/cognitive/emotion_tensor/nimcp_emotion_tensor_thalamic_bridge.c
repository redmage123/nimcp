/**
 * @file nimcp_emotion_tensor_thalamic_bridge.c
 * @brief Emotion Tensor-Thalamic Bridge Implementation
 */

#include "cognitive/emotion_tensor/nimcp_emotion_tensor_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(emotion_tensor_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_emotion_tensor_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_emotion_tensor_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t emotion_tensor_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_emotion_tensor_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "emotion_tensor_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "emotion_tensor_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_emotion_tensor_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_emotion_tensor_thalamic_bridge_mesh_registry = registry;
    return err;
}

void emotion_tensor_thalamic_bridge_mesh_unregister(void) {
    if (g_emotion_tensor_thalamic_bridge_mesh_registry && g_emotion_tensor_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_emotion_tensor_thalamic_bridge_mesh_registry, g_emotion_tensor_thalamic_bridge_mesh_id);
        g_emotion_tensor_thalamic_bridge_mesh_id = 0;
        g_emotion_tensor_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from emotion_tensor_thalamic_bridge module (instance-level) */
static inline void emotion_tensor_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_emotion_tensor_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_emotion_tensor_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_emotion_tensor_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "EMOTION_TENSOR_THALAMIC_BRIDGE"


struct emotion_tensor_thalamic_bridge {
    bridge_base_t base;  /* MUST be first - provides mutex protection */
    void* emotion_tensor;
    thalamic_router_t* router;
    emotion_tensor_thalamic_config_t config;
    emotion_tensor_thalamic_stats_t stats;
    float attention_weight;
    nimcp_health_agent_t* health_agent;  /**< Instance-level health agent */
};

emotion_tensor_thalamic_config_t emotion_tensor_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_emotion_tensor_thala", 0.0f);


    emotion_tensor_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_arousal_boost = true,
        .min_arousal_threshold = 0.25f,
        .contagion_boost = 0.35f
    };
    return cfg;
}

emotion_tensor_thalamic_bridge_t* emotion_tensor_thalamic_bridge_create(void* emotion_tensor, thalamic_router_t* router, const emotion_tensor_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_create", 0.0f);


    emotion_tensor_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(emotion_tensor_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Initialize mutex for thread safety */
    if (bridge_base_init(&bridge->base, 0, "emotion_tensor_thalamic") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_bridge_create: bridge->base is NULL");
        return NULL;
    }

    bridge->emotion_tensor = emotion_tensor;
    bridge->router = router;
    bridge->config = config ? *config : emotion_tensor_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "emotion_tensor_thalamic");
    return bridge;
}

void emotion_tensor_thalamic_bridge_destroy(emotion_tensor_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_destroy", 0.0f);


    if (bridge) {
        if (bridge->base.mutex) {
            bridge_base_cleanup(&bridge->base);
        }
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int emotion_tensor_thalamic_bridge_reset(emotion_tensor_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_tensor_thalamic_route_update(emotion_tensor_thalamic_bridge_t* bridge, const emotion_tensor_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_route_update: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_emotion_tensor_thala", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->arousal_level < bridge->config.min_arousal_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.updates_routed++;
    bridge->stats.avg_arousal_level = (bridge->stats.avg_arousal_level * (bridge->stats.updates_routed - 1) +
                                       signal->arousal_level) / bridge->stats.updates_routed;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_tensor_thalamic_route_blend(emotion_tensor_thalamic_bridge_t* bridge, const void* tensor, float blend_weight) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_route_blend: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_emotion_tensor_thala", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.blends_processed++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_tensor_thalamic_set_attention(emotion_tensor_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_emotion_tensor_thala", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_tensor_thalamic_get_attention(emotion_tensor_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_emotion_tensor_thala", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int emotion_tensor_thalamic_bridge_get_stats(emotion_tensor_thalamic_bridge_t* bridge, emotion_tensor_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "emotion_tensor_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int emotion_tensor_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Emotion_Tensor_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                emotion_tensor_thalamic_bridge_heartbeat("emotion_tens_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Emotion_Tensor_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Emotion_Tensor_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Instance-Level Health Agent (Phase 8 Utility Integration)
 * ============================================================================ */

void emotion_tensor_thalamic_bridge_set_instance_health_agent(emotion_tensor_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "emotion_tensor_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Training Stubs (Phase 8 Utility Integration)
 *
 * Stub: training integration planned — these are intentional no-ops that
 * provide heartbeat signaling only. Full training hooks will wire into the
 * training-immune bridge when per-module gradient propagation is implemented.
 * ============================================================================ */

int emotion_tensor_thalamic_bridge_training_begin(emotion_tensor_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    emotion_tensor_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int emotion_tensor_thalamic_bridge_training_end(emotion_tensor_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    emotion_tensor_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int emotion_tensor_thalamic_bridge_training_step(emotion_tensor_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "emotion_tensor_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    emotion_tensor_thalamic_bridge_heartbeat_instance(bridge->health_agent, "emotion_tensor_thalamic_bridge_training_step", progress);
    return 0;
}
