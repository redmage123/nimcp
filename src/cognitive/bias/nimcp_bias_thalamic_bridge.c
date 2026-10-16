/**
 * @file nimcp_bias_thalamic_bridge.c
 * @brief Cognitive Bias-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/bias/nimcp_bias_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bias_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_bias_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_bias_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t bias_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_bias_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "bias_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "bias_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_bias_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_bias_thalamic_bridge_mesh_registry = registry;
    return err;
}

void bias_thalamic_bridge_mesh_unregister(void) {
    if (g_bias_thalamic_bridge_mesh_registry && g_bias_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_bias_thalamic_bridge_mesh_registry, g_bias_thalamic_bridge_mesh_id);
        g_bias_thalamic_bridge_mesh_id = 0;
        g_bias_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from bias_thalamic_bridge module (instance-level) */
static inline void bias_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_bias_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_bias_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_bias_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "BIAS_THALAMIC_BRIDGE"


struct bias_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* bias;
    thalamic_router_t* router;
    bias_thalamic_config_t config;
    bias_thalamic_stats_t stats;
    float attention_weight;
};

bias_thalamic_config_t bias_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_bias_thalamic_defaul", 0.0f);


    bias_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_automatic_correction = true,
        .min_detection_confidence = 0.3f,
        .correction_threshold = 0.5f
    };
    return cfg;
}

bias_thalamic_bridge_t* bias_thalamic_bridge_create(void* bias, thalamic_router_t* router, const bias_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_create", 0.0f);


    bias_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(bias_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->bias = bias;
    bridge->router = router;
    bridge->config = config ? *config : bias_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "bias_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    NIMCP_LOGGING_INFO("Created %s bridge", "bias_thalamic");
    return bridge;
}

void bias_thalamic_bridge_destroy(bias_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_destroy", 0.0f);


    if (!bridge) return;
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int bias_thalamic_bridge_reset(bias_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_reset", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bias_thalamic_route_detection(bias_thalamic_bridge_t* bridge, const bias_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_route_detection: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_bias_thalamic_route_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->detection_confidence < bridge->config.min_detection_confidence) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.biases_detected++;
    bridge->stats.avg_detection_confidence = (bridge->stats.avg_detection_confidence * (bridge->stats.biases_detected - 1) +
                                              signal->detection_confidence) / bridge->stats.biases_detected;
    if (signal->signal_type == BIAS_SIGNAL_OVERRIDE) {
        bridge->stats.overrides_successful++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bias_thalamic_route_correction(bias_thalamic_bridge_t* bridge, const void* correction, float strength) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_route_correction: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_bias_thalamic_route_", 0.0f);


    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_automatic_correction && strength >= bridge->config.correction_threshold) {
        bridge->stats.corrections_applied++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bias_thalamic_set_attention(bias_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_bias_thalamic_set_at", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int bias_thalamic_get_attention(const bias_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_bias_thalamic_get_at", 0.0f);


    return 0;
}

int bias_thalamic_bridge_get_stats(const bias_thalamic_bridge_t* bridge, bias_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bias_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_get_stats", 0.0f);


    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int bias_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    bias_thalamic_bridge_heartbeat("bias_thalami_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Bias_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                bias_thalamic_bridge_heartbeat("bias_thalami_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Bias_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Bias_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void bias_thalamic_bridge_set_instance_health_agent(bias_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "bias_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int bias_thalamic_bridge_training_begin(bias_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    bias_thalamic_bridge_heartbeat_instance(bridge->health_agent, "bias_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int bias_thalamic_bridge_training_end(bias_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    bias_thalamic_bridge_heartbeat_instance(bridge->health_agent, "bias_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int bias_thalamic_bridge_training_step(bias_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "bias_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    bias_thalamic_bridge_heartbeat_instance(bridge->health_agent, "bias_thalamic_bridge_training_step", progress);
    return 0;
}
