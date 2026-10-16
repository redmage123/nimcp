/**
 * @file nimcp_consolidation_fep_bridge.c
 * @brief Free Energy Principle - Consolidation Integration Bridge Implementation
 */

#include "cognitive/consolidation/nimcp_consolidation_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "consolidation_fep_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(consolidation_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


int consolidation_fep_bridge_default_config(consolidation_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->complexity_replay_scaling = 1.0f;
    config->fe_urgency_threshold = CONSOLIDATION_FEP_HIGH_FE_THRESHOLD;
    config->enable_complexity_guided_consolidation = true;
    config->enable_pe_replay_selection = true;
    config->enable_fe_urgency = true;
    config->replay_fe_reduction = CONSOLIDATION_FEP_REPLAY_FE_REDUCTION;
    config->pruning_complexity_reduction = CONSOLIDATION_FEP_PRUNING_COMPLEXITY_REDUCTION;
    config->scaling_precision_boost = CONSOLIDATION_FEP_SCALING_PRECISION_BOOST;
    config->enable_replay_fe_reduction = true;
    config->enable_pruning_complexity = true;
    config->enable_scaling_precision = true;
    config->fe_sensitivity = 1.0f;
    config->consolidation_sensitivity = 1.0f;
    return 0;
}

consolidation_fep_bridge_t* consolidation_fep_bridge_create(const consolidation_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_create", 0.0f);


    consolidation_fep_bridge_t* bridge = nimcp_malloc(sizeof(consolidation_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate consolidation FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(consolidation_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        consolidation_fep_bridge_default_config(&bridge->config);
    }
    if (bridge_base_init(&bridge->base, 0, "consolidation_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "consolidation_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }
    NIMCP_LOGGING_INFO("Created consolidation FEP bridge");
    return bridge;
}

void consolidation_fep_bridge_destroy(consolidation_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        consolidation_fep_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed consolidation FEP bridge");
}

int consolidation_fep_bridge_connect_fep(consolidation_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected FEP system to consolidation bridge");
    return 0;
}

int consolidation_fep_bridge_disconnect(consolidation_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Disconnected all systems from consolidation FEP bridge");
    return 0;
}

int consolidation_fep_bridge_update(consolidation_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "bridge or fep_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.current_free_energy = bridge->state.current_free_energy;
    if (bridge->config.enable_fe_urgency) {
        if (bridge->state.current_free_energy > bridge->config.fe_urgency_threshold) {
            bridge->fep_effects.consolidation_urgency = 1.0f;
        }
    }
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * 0.99f) + (bridge->state.current_free_energy * 0.01f);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_fep_bridge_get_state(consolidation_fep_bridge_t* bridge,
                                        consolidation_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_fep_bridge_get_stats(consolidation_fep_bridge_t* bridge,
                                        consolidation_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int consolidation_fep_bridge_connect_bio_async(consolidation_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_CONSOLIDATION_BRIDGE,
        .module_name = "consolidation_fep_bridge",
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

int consolidation_fep_bridge_disconnect_bio_async(consolidation_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool consolidation_fep_bridge_is_bio_async_connected(const consolidation_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_is_bio_async_connect", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int consolidation_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    consolidation_fep_bridge_heartbeat("consolidatio_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Consolidation_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                consolidation_fep_bridge_heartbeat("consolidatio_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Consolidation_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Consolidation_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

void consolidation_fep_bridge_set_instance_health_agent(consolidation_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "consolidation_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
    g_consolidation_fep_bridge_instance_health_agent = agent;
    NIMCP_LOGGING_DEBUG("consolidation_fep_bridge: instance health agent %s", agent ? "set" : "cleared");
}

int consolidation_fep_bridge_training_begin(consolidation_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    consolidation_fep_bridge_heartbeat_instance(bridge, "consol_fep_training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int consolidation_fep_bridge_training_end(consolidation_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_fep_bridge_training_end: NULL argument");
        return -1;
    }
    consolidation_fep_bridge_heartbeat_instance(bridge, "consol_fep_training_end", 1.0f);
    (void)bridge;
    return 0;
}

int consolidation_fep_bridge_training_step(consolidation_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "consolidation_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    consolidation_fep_bridge_heartbeat_instance(bridge, "consol_fep_training_step", progress);
    (void)bridge;
    return 0;
}
