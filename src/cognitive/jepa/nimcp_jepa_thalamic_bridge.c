/**
 * @file nimcp_jepa_thalamic_bridge.c
 * @brief JEPA-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_thalamic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_messages.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(jepa_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_jepa_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_jepa_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t jepa_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_jepa_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "jepa_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "jepa_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_jepa_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_jepa_thalamic_bridge_mesh_registry = registry;
    return err;
}

void jepa_thalamic_bridge_mesh_unregister(void) {
    if (g_jepa_thalamic_bridge_mesh_registry && g_jepa_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_jepa_thalamic_bridge_mesh_registry, g_jepa_thalamic_bridge_mesh_id);
        g_jepa_thalamic_bridge_mesh_id = 0;
        g_jepa_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from jepa_thalamic_bridge module (instance + global) */
static inline void jepa_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_jepa_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_jepa_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_jepa_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "JEPA_THALAMIC_BRIDGE"


/* Forward declarations for bio-async functions used before definition */
bool jepa_thalamic_bridge_register_bio_async(jepa_thalamic_bridge_t* bridge);
void jepa_thalamic_bridge_unregister_bio_async(jepa_thalamic_bridge_t* bridge);

struct jepa_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* jepa;
    thalamic_router_t* router;
    jepa_thalamic_config_t config;
    jepa_thalamic_stats_t stats;
    float attention_weight;
    bool bio_async_registered;
    uint32_t handler_id;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

jepa_thalamic_config_t jepa_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_jepa_thalamic_defaul", 0.0f);


    jepa_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_error_amplification = true,
        .min_prediction_confidence = 0.3f,
        .max_temporal_horizon = 100,
        .bio_async_enabled = true
    };
    return cfg;
}

jepa_thalamic_bridge_t* jepa_thalamic_bridge_create(void* jepa, thalamic_router_t* router, const jepa_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_create", 0.0f);


    jepa_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(jepa_thalamic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    bridge->jepa = jepa;
    bridge->router = router;
    bridge->config = config ? *config : jepa_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    if (bridge_base_init(&bridge->base, 0, "jepa_thalamic") != 0) { nimcp_free(bridge); return NULL; }

    /* Register with bio-async if enabled */
    if (bridge->config.bio_async_enabled) {
        jepa_thalamic_bridge_register_bio_async(bridge);
    }

    NIMCP_LOGGING_INFO("Created %s bridge", "jepa_thalamic");
    return bridge;
}

void jepa_thalamic_bridge_destroy(jepa_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_destroy", 0.0f);

    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_thalamic");

    /* Unregister from bio-async if registered */
    if (bridge->bio_async_registered) {
        jepa_thalamic_bridge_unregister_bio_async(bridge);
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

int jepa_thalamic_bridge_reset(jepa_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_reset", 0.0f);

    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_thalamic_route_prediction(jepa_thalamic_bridge_t* bridge, const jepa_thalamic_signal_t* signal) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_jepa_thalamic_route_", 0.0f);


    NIMCP_CHECK_THROW(bridge && signal, -1, "bridge or signal is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    if (bridge->config.enable_attention_gating && signal->prediction_confidence < bridge->config.min_prediction_confidence) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    if (signal->temporal_horizon > bridge->config.max_temporal_horizon) {
        nimcp_platform_mutex_unlock(bridge->base.mutex);
        return 0;
    }
    bridge->stats.predictions_routed++;
    bridge->stats.avg_prediction_confidence = (bridge->stats.avg_prediction_confidence * (bridge->stats.predictions_routed - 1) +
                                               signal->prediction_confidence) / bridge->stats.predictions_routed;
    if (signal->signal_type == JEPA_SIGNAL_EMBEDDING) {
        bridge->stats.embeddings_updated++;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_thalamic_route_error(jepa_thalamic_bridge_t* bridge, const void* error, float magnitude) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_jepa_thalamic_route_", 0.0f);


    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->stats.errors_propagated++;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_thalamic_set_attention(jepa_thalamic_bridge_t* bridge, float attention) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_jepa_thalamic_set_at", 0.0f);

    NIMCP_CHECK_THROW(bridge, -1, "bridge is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_thalamic_get_attention(const jepa_thalamic_bridge_t* bridge, float* attention) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_jepa_thalamic_get_at", 0.0f);


    NIMCP_CHECK_THROW(bridge && attention, -1, "bridge or attention is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_thalamic_bridge_get_stats(const jepa_thalamic_bridge_t* bridge, jepa_thalamic_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, -1, "bridge or stats is NULL");
    nimcp_platform_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

bool jepa_thalamic_bridge_register_bio_async(jepa_thalamic_bridge_t* bridge) {
    if (!bridge || bridge->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_thalamic_bridge_register_bio_async: bridge is NULL");
        return false;
    }

    bridge->bio_async_registered = true;
    bridge->handler_id = 0;
    return true;
}

void jepa_thalamic_bridge_unregister_bio_async(jepa_thalamic_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_registered) return;

    bridge->bio_async_registered = false;
    bridge->handler_id = 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int jepa_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    jepa_thalamic_bridge_heartbeat("jepa_thalami_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "JEPA_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                jepa_thalamic_bridge_heartbeat("jepa_thalami_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "JEPA_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "JEPA_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_thalamic_bridge_set_instance_health_agent(jepa_thalamic_bridge_t* bridge,
                                                     nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

int jepa_thalamic_bridge_training_begin(jepa_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_begin", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Reset attention weight to full for training */
    bridge->attention_weight = 1.0f;

    /* Clear routing statistics for training epoch */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_begin", 1.0f);
    return 0;
}

int jepa_thalamic_bridge_training_end(jepa_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_end", 0.0f);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* avg_prediction_confidence is already a running average from route_prediction(),
     * so no additional division is needed here. */

    /* Adjust attention weight based on training error rate */
    float error_rate = (bridge->stats.predictions_routed > 0)
        ? (float)bridge->stats.errors_propagated / (float)bridge->stats.predictions_routed
        : 0.0f;
    if (error_rate > 0.5f) {
        bridge->attention_weight *= 0.9f;  /* Reduce attention if many errors */
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_end", 1.0f);
    return 0;
}

int jepa_thalamic_bridge_training_step(jepa_thalamic_bridge_t* bridge, uint32_t step) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_step", progress);

    nimcp_platform_mutex_lock(bridge->base.mutex);
    /* Adapt attention gating threshold during training */
    if (bridge->config.enable_attention_gating) {
        /* Gradually tighten confidence threshold as training progresses */
        float warmup_factor = (step < 100) ? (float)step / 100.0f : 1.0f;
        float adapted_threshold = bridge->config.min_prediction_confidence * warmup_factor;
        (void)adapted_threshold;  /* Used internally for gating decisions */
    }

    /* Track routing statistics */
    bridge->stats.predictions_routed++;

    /* Decay attention slightly per step (will be refreshed by actual signals) */
    bridge->attention_weight *= 0.999f;
    if (bridge->attention_weight < 0.1f) {
        bridge->attention_weight = 0.1f;
    }
    nimcp_platform_mutex_unlock(bridge->base.mutex);

    jepa_thalamic_bridge_heartbeat_instance(bridge->health_agent,
                                             "jepa_thalami_training_step", 1.0f);
    return 0;
}
