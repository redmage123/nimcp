/**
 * @file nimcp_knowledge_thalamic_bridge.c
 * @brief Knowledge-Thalamic Bridge Implementation
 *
 * WHAT: Routes knowledge retrieval signals through the thalamic router
 * WHY: Semantic access requires attention-gated conscious retrieval
 * HOW: Packages knowledge signals into routed_signal_t and calls thalamic_router_route_signal
 */

#include "cognitive/knowledge/nimcp_knowledge_thalamic_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(knowledge_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_knowledge_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_knowledge_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t knowledge_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_knowledge_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "knowledge_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "knowledge_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_knowledge_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_knowledge_thalamic_bridge_mesh_registry = registry;
    return err;
}

void knowledge_thalamic_bridge_mesh_unregister(void) {
    if (g_knowledge_thalamic_bridge_mesh_registry && g_knowledge_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_knowledge_thalamic_bridge_mesh_registry, g_knowledge_thalamic_bridge_mesh_id);
        g_knowledge_thalamic_bridge_mesh_id = 0;
        g_knowledge_thalamic_bridge_mesh_registry = NULL;
    }
}


static inline void knowledge_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_knowledge_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_knowledge_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_knowledge_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "KNOWLEDGE_THALAMIC_BRIDGE"


/* Source ID for knowledge signals in thalamic routing */
#define KNOWLEDGE_THALAMIC_SOURCE_ID 0x0300

/* Default destination IDs for knowledge signals */
#define KNOWLEDGE_DEST_TEMPORAL      0x3001
#define KNOWLEDGE_DEST_PREFRONTAL    0x3002
#define KNOWLEDGE_DEST_HIPPOCAMPUS   0x3003

struct knowledge_thalamic_bridge {
    bridge_base_t base;
    void* knowledge;
    thalamic_router_t* router;
    knowledge_thalamic_config_t config;
    knowledge_thalamic_stats_t stats;
    float attention_weight;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

knowledge_thalamic_config_t knowledge_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_knowledge_thalamic_d", 0.0f);


    knowledge_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_relevance_boost = true,
        .min_retrieval_strength = 0.3f,
        .inference_threshold = 0.5f
    };
    return cfg;
}

knowledge_thalamic_bridge_t* knowledge_thalamic_bridge_create(void* knowledge, thalamic_router_t* router, const knowledge_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_create", 0.0f);


    knowledge_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(knowledge_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_thalamic_bridge_create: failed to allocate bridge");
        return NULL;
    }
    if (bridge_base_init(&bridge->base, 0, "knowledge_thalamic") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_thalamic_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_thalamic_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->knowledge = knowledge;
    bridge->router = router;
    bridge->config = config ? *config : knowledge_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "knowledge_thalamic");
    return bridge;
}

void knowledge_thalamic_bridge_destroy(knowledge_thalamic_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "knowledge_thalamic");
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int knowledge_thalamic_bridge_reset(knowledge_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route knowledge retrieval signal through thalamic router
 *
 * WHAT: Package knowledge retrieval signal and route through thalamic mechanism
 * WHY: Knowledge retrieval needs attention-gated conscious access
 * HOW: Create routed_signal_t, apply relevance boost if enabled, call router
 */
int knowledge_thalamic_route_retrieval(knowledge_thalamic_bridge_t* bridge, const knowledge_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_route_retrieval: required parameter is NULL (bridge, signal)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_knowledge_thalamic_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Attention gating: filter signals below threshold */
    if (bridge->config.enable_attention_gating &&
        signal->retrieval_strength < bridge->config.min_retrieval_strength) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Signal gated, not an error */
    }

    /* Route signal through thalamic router if available */
    if (bridge->router) {
        /* Define destinations for knowledge signals */
        uint32_t dest_ids[] = {
            KNOWLEDGE_DEST_TEMPORAL,
            KNOWLEDGE_DEST_PREFRONTAL,
            KNOWLEDGE_DEST_HIPPOCAMPUS
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package signal data: relevance, confidence, retrieval_strength */
        float signal_data[3] = {
            signal->relevance,
            signal->confidence,
            signal->retrieval_strength
        };

        /* Apply relevance boost to attention if enabled */
        float attention = bridge->attention_weight;
        if (bridge->config.enable_relevance_boost && signal->relevance > 0.5f) {
            attention = attention * (1.0f + (signal->relevance - 0.5f));
            if (attention > 1.0f) attention = 1.0f;
        }

        /* Determine priority based on retrieval strength */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        if (signal->retrieval_strength > 0.8f) {
            priority = SIGNAL_PRIORITY_HIGH;
        } else if (signal->retrieval_strength < 0.4f) {
            priority = SIGNAL_PRIORITY_LOW;
        }

        /* Create routed signal packet */
        routed_signal_t routed = {
            .source_id = KNOWLEDGE_THALAMIC_SOURCE_ID | signal->signal_type,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 3,
            .attention_weight = attention,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = (priority == SIGNAL_PRIORITY_HIGH)
        };

        /* Route through thalamic router */
        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_route_retrieval: routed_ok is NULL");
            return -1;  /* Routing failed */
        }
    }

    /* Update statistics AFTER successful routing */
    bridge->stats.retrievals_routed++;
    bridge->stats.avg_retrieval_strength = (bridge->stats.avg_retrieval_strength * (bridge->stats.retrievals_routed - 1) +
                                            signal->retrieval_strength) / bridge->stats.retrievals_routed;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * @brief Route inference signal through thalamic router
 *
 * WHAT: Route inference result through attention-gated pathway
 * WHY: Inferences need conscious awareness for integration
 * HOW: Package confidence data, route with appropriate priority
 */
int knowledge_thalamic_route_inference(knowledge_thalamic_bridge_t* bridge, const void* inference, float confidence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_route_inference: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_knowledge_thalamic_r", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Filter low-confidence inferences */
    if (confidence < bridge->config.inference_threshold) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Route through thalamic router if available */
    if (bridge->router) {
        uint32_t dest_ids[] = {
            KNOWLEDGE_DEST_PREFRONTAL,
            KNOWLEDGE_DEST_TEMPORAL
        };
        uint32_t num_dests = sizeof(dest_ids) / sizeof(dest_ids[0]);

        /* Package confidence as signal data */
        float signal_data[1] = { confidence };

        /* High confidence inferences get higher priority */
        signal_priority_t priority = SIGNAL_PRIORITY_NORMAL;
        if (confidence > 0.8f) {
            priority = SIGNAL_PRIORITY_HIGH;
        }

        routed_signal_t routed = {
            .source_id = KNOWLEDGE_THALAMIC_SOURCE_ID | KNOWLEDGE_SIGNAL_INFERENCE,
            .dest_ids = dest_ids,
            .num_dests = num_dests,
            .signal_data = signal_data,
            .signal_size = 1,
            .attention_weight = bridge->attention_weight,
            .priority = priority,
            .timestamp_ms = nimcp_time_get_ms(),
            .bypass_queue = (priority == SIGNAL_PRIORITY_HIGH)
        };

        bool routed_ok = thalamic_router_route_signal(bridge->router, &routed);
        if (!routed_ok) {
            nimcp_mutex_unlock(bridge->base.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_route_inference: routed_ok is NULL");
            return -1;
        }
    }

    /* Update statistics after successful routing */
    bridge->stats.inferences_routed++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_thalamic_set_attention(knowledge_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_knowledge_thalamic_s", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_thalamic_get_attention(knowledge_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_knowledge_thalamic_g", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_thalamic_bridge_get_stats(knowledge_thalamic_bridge_t* bridge, knowledge_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "knowledge_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for self-knowledge about thalamic knowledge bridge
 *
 * WHAT: Retrieves entity observations and relations for thalamic-knowledge bridge
 * WHY: Enables self-aware introspection of module capabilities
 * HOW: Uses kg_reader to query JSONL knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_thalamic_bridge_heartbeat("knowledge_th_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_thalamic_bridge_heartbeat("knowledge_th_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_thalamic_bridge_set_instance_health_agent(knowledge_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "knowledge_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_thalamic_bridge_training_begin(knowledge_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    knowledge_thalamic_bridge_heartbeat_instance(bridge->health_agent, "knowledge_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int knowledge_thalamic_bridge_training_end(knowledge_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    knowledge_thalamic_bridge_heartbeat_instance(bridge->health_agent, "knowledge_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int knowledge_thalamic_bridge_training_step(knowledge_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_thalamic_bridge_heartbeat_instance(bridge->health_agent, "knowledge_thalamic_bridge_training_step", progress);
    return 0;
}
