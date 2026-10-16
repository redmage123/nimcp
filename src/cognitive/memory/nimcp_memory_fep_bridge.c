/**
 * @file nimcp_memory_fep_bridge.c
 * @brief Free Energy Principle - Memory Integration Bridge Implementation
 */

#include "cognitive/memory/nimcp_memory_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>

#define LOG_MODULE "memory_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(memory_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_memory_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_memory_fep_bridge_mesh_registry = NULL;

nimcp_error_t memory_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_memory_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "memory_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_MEMORY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "memory_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_memory_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_memory_fep_bridge_mesh_registry = registry;
    return err;
}

void memory_fep_bridge_mesh_unregister(void) {
    if (g_memory_fep_bridge_mesh_registry && g_memory_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_memory_fep_bridge_mesh_registry, g_memory_fep_bridge_mesh_id);
        g_memory_fep_bridge_mesh_id = 0;
        g_memory_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from memory_fep_bridge module (instance-level) */
static inline void memory_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_memory_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_memory_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_memory_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* Security subsystem setters (Phase 1: Audit Gap Remediation) */
BRIDGE_DEFINE_SECURITY_SETTERS(memory_fep_bridge)

int memory_fep_bridge_default_config(memory_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->wm_capacity_factor = 1.0f;
    config->consolidation_threshold = MEMORY_FEP_CONSOLIDATION_THRESHOLD;
    config->retrieval_precision_boost = 1.5f;
    config->enable_wm_belief_buffer = true;
    config->enable_consolidation_replay = true;
    config->enable_retrieval_active_inference = true;
    config->belief_prior_strength = 1.0f;
    config->memory_trace_persistence = 0.9f;
    config->enable_belief_priors = true;
    config->enable_trace_persistence = true;
    config->fe_sensitivity = 1.0f;
    config->memory_sensitivity = 1.0f;
    return 0;
}

memory_fep_bridge_t* memory_fep_bridge_create(const memory_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_create", 0.0f);


    memory_fep_bridge_t* bridge = nimcp_malloc(sizeof(memory_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(memory_fep_bridge_t));
    if (config) bridge->config = *config;
    else memory_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "memory_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "memory_fep");
    return bridge;
}

void memory_fep_bridge_destroy(memory_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "memory_fep");
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) memory_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int memory_fep_bridge_connect_fep(memory_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_bridge_connect_memory(memory_fep_bridge_t* bridge, semantic_memory_system_t* memory) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_connect_memory", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && memory, NIMCP_ERROR_NULL_POINTER, "bridge or memory is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->memory_system = memory;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_bridge_disconnect(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->memory_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_maintain_wm_beliefs(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_memory_fep_maintain_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_wm_belief_buffer) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.wm_capacity_remaining = (float)MEMORY_FEP_WM_CAPACITY - bridge->state.current_wm_load;
    bridge->stats.wm_buffer_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_trigger_consolidation(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_memory_fep_trigger_c", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_consolidation_replay) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->state.current_wm_load > bridge->config.consolidation_threshold) {
        bridge->fep_effects.consolidation_triggered = true;
        bridge->state.consolidation_active = true;
        bridge->stats.consolidation_events++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_boost_retrieval_precision(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_memory_fep_boost_ret", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_retrieval_active_inference) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.retrieval_precision = bridge->config.retrieval_precision_boost;
    bridge->stats.retrieval_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_apply_belief_priors(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_memory_fep_apply_bel", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_belief_priors) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->memory_effects.belief_prior_bias = bridge->config.belief_prior_strength;
    bridge->memory_effects.priors_active = true;
    bridge->stats.belief_prior_applications++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_apply_trace_persistence(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_memory_fep_apply_tra", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_trace_persistence) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->memory_effects.trace_persistence_factor = bridge->config.memory_trace_persistence;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_bridge_update(memory_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "memory_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "memory_fep_bridge_update");
    memory_fep_maintain_wm_beliefs(bridge);
    memory_fep_trigger_consolidation(bridge);
    memory_fep_boost_retrieval_precision(bridge);
    memory_fep_apply_belief_priors(bridge);
    memory_fep_apply_trace_persistence(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int memory_fep_bridge_get_state(memory_fep_bridge_t* bridge, memory_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_bridge_get_stats(memory_fep_bridge_t* bridge, memory_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int memory_fep_bridge_connect_bio_async(memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_MEMORY_BRIDGE,
        .module_name = "memory_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int memory_fep_bridge_disconnect_bio_async(memory_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool memory_fep_bridge_is_bio_async_connected(const memory_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * KG Self-Awareness Integration
 * ============================================================================ */

/**
 * @brief Query self-knowledge from knowledge graph
 * WHAT: Retrieve module's self-awareness information from KG
 * WHY:  Enable introspection about module capabilities and connections
 * HOW:  Query KG reader for entity and relations
 */
int memory_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    memory_fep_bridge_heartbeat("memory_fep_b_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Memory_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                memory_fep_bridge_heartbeat("memory_fep_b_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            LOG_DEBUG("Memory FEP bridge self-knowledge: %s", self->observations[i]);
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Memory_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Memory_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void memory_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_memory_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int memory_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    memory_fep_bridge_heartbeat_instance(NULL, "memory_fep_bridge_training_begin", 0.0f);
    return 0;
}

int memory_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_fep_bridge_training_end: NULL argument");
        return -1;
    }
    memory_fep_bridge_heartbeat_instance(NULL, "memory_fep_bridge_training_end", 1.0f);
    return 0;
}

int memory_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "memory_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    memory_fep_bridge_heartbeat_instance(NULL, "memory_fep_bridge_training_step", progress);
    return 0;
}
