/**
 * @file nimcp_wellbeing_fep_bridge.c
 * @brief Free Energy Principle - Wellbeing Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-15
 */

#include "cognitive/wellbeing/nimcp_wellbeing_fep_bridge.h"
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

#define LOG_MODULE "wellbeing_fep_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(wellbeing_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

int wellbeing_fep_bridge_default_config(wellbeing_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->chronic_fe_threshold = WELLBEING_FEP_CHRONIC_FE_THRESHOLD;
    config->acute_surprise_threshold = WELLBEING_FEP_ACUTE_SURPRISE_THRESHOLD;
    config->fe_window_ms = WELLBEING_FEP_DISTRESS_FE_WINDOW_MS;
    config->enable_fe_distress_detection = true;
    config->enable_surprise_distress = true;
    config->enable_complexity_distress = true;
    config->distress_lr_reduction = WELLBEING_FEP_DISTRESS_LR_REDUCTION;
    config->relief_fe_reduction = WELLBEING_FEP_RELIEF_FE_REDUCTION;
    config->enable_distress_lr_modulation = true;
    config->enable_relief_fe_reduction = true;
    config->mild_fe_threshold = WELLBEING_FEP_MILD_FE_THRESHOLD;
    config->moderate_fe_threshold = WELLBEING_FEP_MODERATE_FE_THRESHOLD;
    config->severe_fe_threshold = WELLBEING_FEP_SEVERE_FE_THRESHOLD;
    config->critical_fe_threshold = WELLBEING_FEP_CRITICAL_FE_THRESHOLD;
    config->fe_sensitivity = 1.0f;
    config->wellbeing_sensitivity = 1.0f;
    return 0;
}

wellbeing_fep_bridge_t* wellbeing_fep_bridge_create(const wellbeing_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_create", 0.0f);


    wellbeing_fep_bridge_t* bridge = nimcp_malloc(sizeof(wellbeing_fep_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Failed to allocate wellbeing FEP bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }
    memset(bridge, 0, sizeof(wellbeing_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        wellbeing_fep_bridge_default_config(&bridge->config);
    }
    if (bridge_base_init(&bridge->base, 0, "wellbeing_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "wellbeing_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }
    NIMCP_LOGGING_INFO("Created wellbeing FEP bridge");
    return bridge;
}

void wellbeing_fep_bridge_destroy(wellbeing_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        wellbeing_fep_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Destroyed wellbeing FEP bridge");
}

int wellbeing_fep_bridge_connect_fep(wellbeing_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Connected FEP system to wellbeing bridge");
    return 0;
}

int wellbeing_fep_bridge_disconnect(wellbeing_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    NIMCP_LOGGING_INFO("Disconnected all systems from wellbeing FEP bridge");
    return 0;
}

int wellbeing_fep_bridge_update(wellbeing_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_INVALID_STATE, "bridge or fep_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->state.current_free_energy = fep_get_free_energy(bridge->fep_system);
    bridge->fep_effects.current_free_energy = bridge->state.current_free_energy;
    if (bridge->config.enable_fe_distress_detection) {
        if (bridge->state.current_free_energy > bridge->config.chronic_fe_threshold) {
            bridge->state.chronic_distress_detected = true;
            bridge->stats.chronic_distress_detections++;
        }
    }
    bridge->stats.avg_free_energy =
        (bridge->stats.avg_free_energy * 0.99f) + (bridge->state.current_free_energy * 0.01f);
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_fep_bridge_get_state(wellbeing_fep_bridge_t* bridge,
                                    wellbeing_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_fep_bridge_get_stats(wellbeing_fep_bridge_t* bridge,
                                    wellbeing_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int wellbeing_fep_bridge_connect_bio_async(wellbeing_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_WELLBEING_BRIDGE,
        .module_name = "wellbeing_fep_bridge",
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

int wellbeing_fep_bridge_disconnect_bio_async(wellbeing_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }
    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("Disconnected from bio-async router");
    return 0;
}

bool wellbeing_fep_bridge_is_bio_async_connected(const wellbeing_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_is_bio_async_connect", 0.0f);


    return bridge->base.bio_async_enabled;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for FEP Wellbeing Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int wellbeing_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    wellbeing_fep_bridge_heartbeat("wellbeing_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Wellbeing_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                wellbeing_fep_bridge_heartbeat("wellbeing_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Wellbeing FEP Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Wellbeing_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Wellbeing_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void wellbeing_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_wellbeing_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int wellbeing_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    wellbeing_fep_bridge_heartbeat_instance(g_wellbeing_fep_bridge_health_agent, "wellbeing_fep_bridge_training_begin", 0.0f);
    return 0;
}

int wellbeing_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_fep_bridge_training_end: NULL argument");
        return -1;
    }
    wellbeing_fep_bridge_heartbeat_instance(g_wellbeing_fep_bridge_health_agent, "wellbeing_fep_bridge_training_end", 1.0f);
    return 0;
}

int wellbeing_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "wellbeing_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    wellbeing_fep_bridge_heartbeat_instance(g_wellbeing_fep_bridge_health_agent, "wellbeing_fep_bridge_training_step", progress);
    return 0;
}
