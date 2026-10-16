/**
 * @file nimcp_knowledge_fep_bridge.c
 * @brief Knowledge FEP Bridge Implementation
 */

#include "cognitive/knowledge/nimcp_knowledge_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE_KNOWLEDGE_FEP "[KNOWLEDGE_FEP]"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(knowledge_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_knowledge_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_knowledge_fep_bridge_mesh_registry = NULL;

nimcp_error_t knowledge_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_knowledge_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "knowledge_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "knowledge_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_knowledge_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_knowledge_fep_bridge_mesh_registry = registry;
    return err;
}

void knowledge_fep_bridge_mesh_unregister(void) {
    if (g_knowledge_fep_bridge_mesh_registry && g_knowledge_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_knowledge_fep_bridge_mesh_registry, g_knowledge_fep_bridge_mesh_id);
        g_knowledge_fep_bridge_mesh_id = 0;
        g_knowledge_fep_bridge_mesh_registry = NULL;
    }
}


static inline void knowledge_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_knowledge_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_knowledge_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_knowledge_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


int knowledge_fep_bridge_default_config(knowledge_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->knowledge_update_threshold = KNOWLEDGE_FEP_UPDATE_THRESHOLD;
    config->semantic_prior_weight = KNOWLEDGE_FEP_SEMANTIC_PRIOR_WEIGHT;
    config->enable_knowledge_updates = true;
    config->enable_semantic_priors = true;
    config->pe_sensitivity = 1.0f;
    return 0;
}

knowledge_fep_bridge_t* knowledge_fep_bridge_create(const knowledge_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_create", 0.0f);


    knowledge_fep_bridge_t* bridge = (knowledge_fep_bridge_t*)nimcp_malloc(sizeof(knowledge_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_fep_bridge_create: failed to allocate bridge");
        return NULL;
    }
    memset(bridge, 0, sizeof(knowledge_fep_bridge_t));
    if (config) bridge->config = *config;
    else knowledge_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "knowledge_fep") != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_fep_bridge_create: bridge_base_init failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "knowledge_fep_bridge_create: mutex creation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }
    bridge->state.current_semantic_prior = 0.5f;
    NIMCP_LOGGING_INFO(LOG_MODULE_KNOWLEDGE_FEP " Bridge created");
    return bridge;
}

void knowledge_fep_bridge_destroy(knowledge_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) knowledge_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int knowledge_fep_bridge_connect_fep(knowledge_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_bridge_connect_knowledge(knowledge_fep_bridge_t* bridge, knowledge_system_t knowledge) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_connect_knowledge", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && knowledge, NIMCP_ERROR_NULL_POINTER, "bridge or knowledge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->knowledge_system = knowledge;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_update_knowledge(knowledge_fep_bridge_t* bridge, float prediction_error) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_knowledge_fep_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_knowledge_updates) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (fabsf(prediction_error) > bridge->config.knowledge_update_threshold) {
        bridge->state.knowledge_updates++;
        bridge->stats.updates_total++;
        bridge->effects.semantic_pe = fabsf(prediction_error);
    }
    if (isfinite(prediction_error)) {
        bridge->stats.avg_semantic_pe = (bridge->stats.avg_semantic_pe * 0.9f) + (fabsf(prediction_error) * 0.1f);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_apply_semantic_priors(knowledge_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_knowledge_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    /* Apply knowledge as semantic priors to FEP */
    bridge->state.current_semantic_prior = bridge->config.semantic_prior_weight;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_bridge_update(knowledge_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->effects.knowledge_confidence = 1.0f - (bridge->effects.semantic_pe / 10.0f);
    if (bridge->effects.knowledge_confidence < 0.0f) bridge->effects.knowledge_confidence = 0.0f;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_bridge_get_state(knowledge_fep_bridge_t* bridge, knowledge_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_bridge_get_stats(knowledge_fep_bridge_t* bridge, knowledge_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int knowledge_fep_bridge_connect_bio_async(knowledge_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_KNOWLEDGE_BRIDGE,
        .module_name = "knowledge_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        return 0;
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "knowledge_fep_bridge_connect_bio_async: validation failed");
    return -1;
}

int knowledge_fep_bridge_disconnect_bio_async(knowledge_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool knowledge_fep_bridge_is_bio_async_connected(const knowledge_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query knowledge graph for self-knowledge about FEP knowledge bridge
 *
 * WHAT: Retrieves entity observations and relations for FEP-knowledge bridge
 * WHY: Enables self-aware introspection of module capabilities
 * HOW: Uses kg_reader to query JSONL knowledge graph
 *
 * @param kg Knowledge graph reader instance
 * @return 1 if self-knowledge found, 0 otherwise
 */
int knowledge_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    knowledge_fep_bridge_heartbeat("knowledge_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Knowledge_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                knowledge_fep_bridge_heartbeat("knowledge_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Knowledge_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Knowledge_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void knowledge_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_knowledge_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int knowledge_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    knowledge_fep_bridge_heartbeat_instance(NULL, "knowledge_fep_bridge_training_begin", 0.0f);
    return 0;
}

int knowledge_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fep_bridge_training_end: NULL argument");
        return -1;
    }
    knowledge_fep_bridge_heartbeat_instance(NULL, "knowledge_fep_bridge_training_end", 1.0f);
    return 0;
}

int knowledge_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "knowledge_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    knowledge_fep_bridge_heartbeat_instance(NULL, "knowledge_fep_bridge_training_step", progress);
    return 0;
}
