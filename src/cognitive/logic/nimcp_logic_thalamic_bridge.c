/**
 * @file nimcp_logic_thalamic_bridge.c
 * @brief Logic-Thalamic Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/logic/nimcp_logic_thalamic_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(logic_thalamic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_logic_thalamic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_logic_thalamic_bridge_mesh_registry = NULL;

nimcp_error_t logic_thalamic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_logic_thalamic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "logic_thalamic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "logic_thalamic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_logic_thalamic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_logic_thalamic_bridge_mesh_registry = registry;
    return err;
}

void logic_thalamic_bridge_mesh_unregister(void) {
    if (g_logic_thalamic_bridge_mesh_registry && g_logic_thalamic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_logic_thalamic_bridge_mesh_registry, g_logic_thalamic_bridge_mesh_id);
        g_logic_thalamic_bridge_mesh_id = 0;
        g_logic_thalamic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from logic_thalamic_bridge module (instance-level) */
static inline void logic_thalamic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_logic_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_logic_thalamic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_logic_thalamic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "LOGIC_THALAMIC_BRIDGE"


struct logic_thalamic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* logic;
    thalamic_router_t* router;
    logic_thalamic_config_t config;
    logic_thalamic_stats_t stats;
    float attention_weight;
};

logic_thalamic_config_t logic_thalamic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_logic_thalamic_defau", 0.0f);


    logic_thalamic_config_t cfg = {
        .enable_attention_gating = true,
        .enable_contradiction_alert = true,
        .min_logical_strength = 0.3f,
        .max_inference_depth = 10
    };
    return cfg;
}

logic_thalamic_bridge_t* logic_thalamic_bridge_create(void* logic, thalamic_router_t* router, const logic_thalamic_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_create", 0.0f);


    logic_thalamic_bridge_t* bridge = nimcp_calloc(1, sizeof(logic_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "logic_thalamic_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Initialize bridge base (provides mutex for thread safety) */
    if (bridge_base_init(&bridge->base, 0, "logic_thalamic_bridge") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    bridge->logic = logic;
    bridge->router = router;
    bridge->config = config ? *config : logic_thalamic_default_config();
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    NIMCP_LOGGING_INFO("Created %s bridge", "logic_thalamic");
    return bridge;
}

void logic_thalamic_bridge_destroy(logic_thalamic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_destroy", 0.0f);

    if (bridge) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
    }
}

int logic_thalamic_bridge_reset(logic_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_bridge_reset: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_reset", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int logic_thalamic_route_inference(logic_thalamic_bridge_t* bridge, const logic_thalamic_signal_t* signal) {
    if (!bridge || !signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_route_inference: required parameter is NULL (bridge, signal)");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_logic_thalamic_route", 0.0f);

    if (bridge->config.enable_attention_gating && signal->logical_strength < bridge->config.min_logical_strength) {
        return 0;
    }
    if (signal->inference_depth > bridge->config.max_inference_depth) {
        return 0;
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.inferences_routed++;
    bridge->stats.avg_inference_depth = (bridge->stats.avg_inference_depth * (bridge->stats.inferences_routed - 1) +
                                         signal->inference_depth) / bridge->stats.inferences_routed;
    if (bridge->config.enable_contradiction_alert && signal->signal_type == LOGIC_SIGNAL_CONTRADICTION) {
        bridge->stats.contradictions_flagged++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int logic_thalamic_route_conclusion(logic_thalamic_bridge_t* bridge, const void* conclusion, float confidence) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_route_conclusion: bridge is NULL");
        return -1;
    }
    (void)conclusion;
    (void)confidence;
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_logic_thalamic_route", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.conclusions_routed++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int logic_thalamic_set_attention(logic_thalamic_bridge_t* bridge, float attention) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_set_attention: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_logic_thalamic_set_a", 0.0f);

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->attention_weight = attention < 0.0f ? 0.0f : (attention > 1.0f ? 1.0f : attention);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int logic_thalamic_get_attention(const logic_thalamic_bridge_t* bridge, float* attention) {
    if (!bridge || !attention) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_get_attention: required parameter is NULL (bridge, attention)");
        return -1;
    }
    /* Cast away const for mutex lock (mutex is logically mutable) */
    nimcp_mutex_lock(((logic_thalamic_bridge_t*)bridge)->base.mutex);
    *attention = bridge->attention_weight;
    nimcp_mutex_unlock(((logic_thalamic_bridge_t*)bridge)->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_logic_thalamic_get_a", 0.0f);

    return 0;
}

int logic_thalamic_bridge_get_stats(const logic_thalamic_bridge_t* bridge, logic_thalamic_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "logic_thalamic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    /* Cast away const for mutex lock (mutex is logically mutable) */
    nimcp_mutex_lock(((logic_thalamic_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((logic_thalamic_bridge_t*)bridge)->base.mutex);
    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_get_stats", 0.0f);

    return 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int logic_thalamic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    logic_thalamic_bridge_heartbeat("logic_thalam_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Logic_Thalamic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                logic_thalamic_bridge_heartbeat("logic_thalam_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Logic_Thalamic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Logic_Thalamic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void logic_thalamic_bridge_set_instance_health_agent(logic_thalamic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "logic_thalamic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int logic_thalamic_bridge_training_begin(logic_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_thalamic_bridge_training_begin: NULL argument");
        return -1;
    }
    logic_thalamic_bridge_heartbeat_instance(bridge->health_agent, "logic_thalamic_bridge_training_begin", 0.0f);
    return 0;
}

int logic_thalamic_bridge_training_end(logic_thalamic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_thalamic_bridge_training_end: NULL argument");
        return -1;
    }
    logic_thalamic_bridge_heartbeat_instance(bridge->health_agent, "logic_thalamic_bridge_training_end", 1.0f);
    return 0;
}

int logic_thalamic_bridge_training_step(logic_thalamic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "logic_thalamic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    logic_thalamic_bridge_heartbeat_instance(bridge->health_agent, "logic_thalamic_bridge_training_step", progress);
    return 0;
}
