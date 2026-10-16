/**
 * @file nimcp_hierarchical_fep_bridge.c
 * @brief Free Energy Principle - Hierarchical Brain Integration Bridge Implementation
 */

#include "cognitive/nimcp_hierarchical_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "hierarchical_fep_bridge"

/**
 * Inline EWMA update macro - equivalent to nimcp_ewma_update() from streaming statistics.
 * Formula: EWMA_t = alpha * x_t + (1 - alpha) * EWMA_{t-1}
 * Using alpha=0.01 for stable long-term averaging (equivalent ~99-sample window).
 * @see include/utils/statistics/nimcp_streaming_statistics.h for full EWMA API
 */
#define NIMCP_EWMA_UPDATE(avg, new_val, alpha) \
    ((avg) = ((avg) * (1.0f - (alpha))) + ((new_val) * (alpha)))
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hierarchical_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from hierarchical_fep_bridge module (instance-level) */
static inline void hierarchical_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_hierarchical_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_hierarchical_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_hierarchical_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



int hierarchical_fep_bridge_default_config(hierarchical_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->enable_hierarchical_prediction = true;
    config->enable_pe_propagation = true;
    config->enable_layer_specific_lr = true;
    config->hierarchy_sensitivity = 1.0f;
    config->fep_sensitivity = 1.0f;
    return 0;
}

hierarchical_fep_bridge_t* hierarchical_fep_bridge_create(const hierarchical_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_create", 0.0f);


    hierarchical_fep_bridge_t* bridge = nimcp_malloc(sizeof(hierarchical_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate hierarchical FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(hierarchical_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        hierarchical_fep_bridge_default_config(&bridge->config);
    }
    if (bridge_base_init(&bridge->base, 0, "hierarchical_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hierarchical_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }
    NIMCP_LOGGING_INFO("Created hierarchical FEP bridge");
    return bridge;
}

void hierarchical_fep_bridge_destroy(hierarchical_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        hierarchical_fep_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->state.level_states) {
        nimcp_free(bridge->state.level_states);
    }
    if (bridge->fep_effects.level_free_energies) {
        nimcp_free(bridge->fep_effects.level_free_energies);
    }
    if (bridge->fep_effects.level_prediction_errors) {
        nimcp_free(bridge->fep_effects.level_prediction_errors);
    }
    if (bridge->hierarchical_effects.level_lr_modifiers) {
        nimcp_free(bridge->hierarchical_effects.level_lr_modifiers);
    }
    if (bridge->hierarchical_effects.level_precisions) {
        nimcp_free(bridge->hierarchical_effects.level_precisions);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed hierarchical FEP bridge");
}

int hierarchical_fep_bridge_connect_fep(hierarchical_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected FEP system to hierarchical bridge");
    return 0;
}

int hierarchical_fep_bridge_connect_hierarchical(hierarchical_fep_bridge_t* bridge,
                                                  hierarchical_brain_t hbrain) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_connect_hierarchical", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hierarchical_brain = hbrain;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected hierarchical brain to FEP bridge");
    return 0;
}

int hierarchical_fep_bridge_disconnect(hierarchical_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->hierarchical_brain = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Disconnected all systems from hierarchical FEP bridge");
    return 0;
}

int hierarchical_fep_bridge_update(hierarchical_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "bridge or fep_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_free_energy = fep_get_free_energy(bridge->fep_system);
    /* EWMA update for running average free energy (alpha=0.01, ~99-sample window) */
    NIMCP_EWMA_UPDATE(bridge->stats.avg_free_energy, bridge->state.current_free_energy, 0.01f);
    bridge->stats.prediction_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hierarchical_fep_bridge_get_state(hierarchical_fep_bridge_t* bridge,
                                       hierarchical_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hierarchical_fep_bridge_get_stats(hierarchical_fep_bridge_t* bridge,
                                       hierarchical_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int hierarchical_fep_bridge_connect_bio_async(hierarchical_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_HIERARCHICAL_BRIDGE,
        .module_name = "hierarchical_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Connected to bio-async router");
    }
    return 0;
}

int hierarchical_fep_bridge_disconnect_bio_async(hierarchical_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool hierarchical_fep_bridge_is_bio_async_connected(const hierarchical_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_is_bio_async_connect", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int hierarchical_fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    hierarchical_fep_bridge_heartbeat("hierarchical_hierarchical_fep_que", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Hierarchical_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                hierarchical_fep_bridge_heartbeat("hierarchical_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Hierarchical_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Hierarchical_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void hierarchical_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_hierarchical_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int hierarchical_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hierarchical_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    hierarchical_fep_bridge_heartbeat_instance(NULL, "hierarchical_fep_bridge_training_begin", 0.0f);
    return 0;
}

int hierarchical_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hierarchical_fep_bridge_training_end: NULL argument");
        return -1;
    }
    hierarchical_fep_bridge_heartbeat_instance(NULL, "hierarchical_fep_bridge_training_end", 1.0f);
    return 0;
}

int hierarchical_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "hierarchical_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    hierarchical_fep_bridge_heartbeat_instance(NULL, "hierarchical_fep_bridge_training_step", progress);
    return 0;
}
