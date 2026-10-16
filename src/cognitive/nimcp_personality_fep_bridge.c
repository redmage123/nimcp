/**
 * @file nimcp_personality_fep_bridge.c
 * @brief Free Energy Principle - Personality Integration Bridge Implementation
 */

#include "cognitive/nimcp_personality_fep_bridge.h"
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

#define LOG_MODULE "personality_fep_bridge"

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

BRIDGE_BOILERPLATE_MESH_ONLY(personality_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from personality_fep_bridge module (instance-level) */
static inline void personality_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_personality_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_personality_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_personality_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



int personality_fep_bridge_default_config(personality_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->enable_openness_exploration = true;
    config->enable_neuroticism_precision = true;
    config->enable_conscientiousness_planning = true;
    config->personality_sensitivity = 1.0f;
    config->fep_sensitivity = 1.0f;
    return 0;
}

personality_fep_bridge_t* personality_fep_bridge_create(const personality_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__create", 0.0f);


    personality_fep_bridge_t* bridge = nimcp_malloc(sizeof(personality_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate personality FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(personality_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        personality_fep_bridge_default_config(&bridge->config);
    }
    if (bridge_base_init(&bridge->base, 0, "personality_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "personality_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }
    NIMCP_LOGGING_INFO("Created personality FEP bridge");
    return bridge;
}

void personality_fep_bridge_destroy(personality_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        personality_fep_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed personality FEP bridge");
}

int personality_fep_bridge_connect_fep(personality_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected FEP system to personality bridge");
    return 0;
}

int personality_fep_bridge_connect_personality(personality_fep_bridge_t* bridge,
                                                personality_profile_t* personality) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__connect_personality", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && personality, NIMCP_ERROR_NULL_POINTER, "bridge or personality is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->personality = personality;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected personality to FEP bridge");
    return 0;
}

int personality_fep_bridge_disconnect(personality_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->personality = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Disconnected all systems from personality FEP bridge");
    return 0;
}

int personality_fep_bridge_update(personality_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system && bridge->personality, NIMCP_ERROR_INVALID_STATE, "bridge, fep_system, or personality is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_free_energy = fep_get_free_energy(bridge->fep_system);
    if (bridge->config.enable_openness_exploration) {
        bridge->fep_effects.openness_exploration_boost =
            (bridge->personality->traits.openness - 0.5f);
        bridge->state.exploration_rate = 0.5f + bridge->fep_effects.openness_exploration_boost;
    }
    if (bridge->config.enable_neuroticism_precision) {
        bridge->fep_effects.neuroticism_precision_modifier =
            1.0f + (bridge->personality->traits.neuroticism - 0.5f);
        bridge->state.precision_modifier = bridge->fep_effects.neuroticism_precision_modifier;
    }
    /* EWMA update for running average free energy (alpha=0.01, ~99-sample window) */
    NIMCP_EWMA_UPDATE(bridge->stats.avg_free_energy, bridge->state.current_free_energy, 0.01f);
    bridge->stats.modulation_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_fep_bridge_get_state(personality_fep_bridge_t* bridge,
                                      personality_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_fep_bridge_get_stats(personality_fep_bridge_t* bridge,
                                      personality_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int personality_fep_bridge_connect_bio_async(personality_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_PERSONALITY_BRIDGE,
        .module_name = "personality_fep_bridge",
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

int personality_fep_bridge_disconnect_bio_async(personality_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool personality_fep_bridge_is_bio_async_connected(const personality_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__is_bio_async_connect", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int personality_fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    personality_fep_bridge_heartbeat("personality__personality_fep_quer", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Personality_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                personality_fep_bridge_heartbeat("personality__loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Personality_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Personality_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void personality_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_personality_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int personality_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    personality_fep_bridge_heartbeat_instance(NULL, "personality_fep_bridge_training_begin", 0.0f);
    return 0;
}

int personality_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_fep_bridge_training_end: NULL argument");
        return -1;
    }
    personality_fep_bridge_heartbeat_instance(NULL, "personality_fep_bridge_training_end", 1.0f);
    return 0;
}

int personality_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "personality_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    personality_fep_bridge_heartbeat_instance(NULL, "personality_fep_bridge_training_step", progress);
    return 0;
}
