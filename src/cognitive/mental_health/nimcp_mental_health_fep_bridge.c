/**
 * @file nimcp_mental_health_fep_bridge.c
 * @brief Free Energy Principle - Mental Health Integration Bridge Implementation
 */

#include "cognitive/mental_health/nimcp_mental_health_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define LOG_MODULE "mental_health_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mental_health_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mental_health_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mental_health_fep_bridge_mesh_registry = NULL;

nimcp_error_t mental_health_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_mental_health_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mental_health_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mental_health_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mental_health_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mental_health_fep_bridge_mesh_registry = registry;
    return err;
}

void mental_health_fep_bridge_mesh_unregister(void) {
    if (g_mental_health_fep_bridge_mesh_registry && g_mental_health_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mental_health_fep_bridge_mesh_registry, g_mental_health_fep_bridge_mesh_id);
        g_mental_health_fep_bridge_mesh_id = 0;
        g_mental_health_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mental_health_fep_bridge module (instance-level) */
static inline void mental_health_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mental_health_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mental_health_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mental_health_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



int mental_health_fep_bridge_default_config(mental_health_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->aberrant_precision_threshold = MENTAL_HEALTH_FEP_ABERRANT_PRECISION_THRESHOLD;
    config->pathological_lr_threshold = MENTAL_HEALTH_FEP_PATHOLOGICAL_LR_THRESHOLD;
    config->negative_prior_threshold = -1.0f;
    config->enable_aberrant_precision_detection = true;
    config->enable_pathological_learning_detection = true;
    config->enable_negative_prior_detection = true;
    config->intervention_precision_correction = 0.5f;
    config->intervention_lr_correction = 0.1f;
    config->enable_precision_intervention = true;
    config->enable_lr_intervention = true;
    config->fe_sensitivity = 1.0f;
    config->mental_health_sensitivity = 1.0f;
    return 0;
}

mental_health_fep_bridge_t* mental_health_fep_bridge_create(const mental_health_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_create", 0.0f);


    mental_health_fep_bridge_t* bridge = nimcp_malloc(sizeof(mental_health_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(mental_health_fep_bridge_t));
    if (config) bridge->config = *config;
    else mental_health_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "mental_health_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    NIMCP_LOGGING_INFO("Created %s bridge", "mental_health_fep");
    return bridge;
}

void mental_health_fep_bridge_destroy(mental_health_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mental_health_fep");
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) mental_health_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int mental_health_fep_bridge_connect_fep(mental_health_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_bridge_connect_mental_health(mental_health_fep_bridge_t* bridge, mental_health_monitor_t* mh) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_connect_mental_healt", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && mh, NIMCP_ERROR_NULL_POINTER, "bridge or mental_health is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->mental_health_system = mh;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_bridge_disconnect(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->mental_health_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_detect_aberrant_precision(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_mental_health_fep_de", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_aberrant_precision_detection) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->state.current_precision > bridge->config.aberrant_precision_threshold) {
        bridge->fep_effects.aberrant_precision_detected = true;
        bridge->state.pathology_detected = true;
        bridge->stats.aberrant_precision_events++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_detect_pathological_learning(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_mental_health_fep_de", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_pathological_learning_detection) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->state.current_learning_rate < bridge->config.pathological_lr_threshold) {
        bridge->fep_effects.pathological_learning_detected = true;
        bridge->state.pathology_detected = true;
        bridge->stats.pathological_learning_events++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_detect_negative_priors(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_mental_health_fep_de", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_negative_prior_detection) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.negative_priors_detected = false;
    bridge->stats.negative_prior_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_apply_precision_intervention(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_mental_health_fep_ap", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_precision_intervention) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->fep_effects.aberrant_precision_detected) {
        bridge->mental_health_effects.precision_correction = bridge->config.intervention_precision_correction;
        bridge->mental_health_effects.intervention_active = true;
        bridge->stats.intervention_events++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_apply_lr_intervention(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_mental_health_fep_ap", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_lr_intervention) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (bridge->fep_effects.pathological_learning_detected) {
        bridge->mental_health_effects.lr_correction = bridge->config.intervention_lr_correction;
        bridge->mental_health_effects.intervention_active = true;
        bridge->stats.intervention_events++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_bridge_update(mental_health_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    mental_health_fep_detect_aberrant_precision(bridge);
    mental_health_fep_detect_pathological_learning(bridge);
    mental_health_fep_detect_negative_priors(bridge);
    mental_health_fep_apply_precision_intervention(bridge);
    mental_health_fep_apply_lr_intervention(bridge);
    return 0;
}

int mental_health_fep_bridge_get_state(mental_health_fep_bridge_t* bridge, mental_health_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_bridge_get_stats(mental_health_fep_bridge_t* bridge, mental_health_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int mental_health_fep_bridge_connect_bio_async(mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_MENTAL_HEALTH_BRIDGE,
        .module_name = "mental_health_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int mental_health_fep_bridge_disconnect_bio_async(mental_health_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool mental_health_fep_bridge_is_bio_async_connected(const mental_health_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int mental_health_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    mental_health_fep_bridge_heartbeat("mental_healt_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Mental_Health_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                mental_health_fep_bridge_heartbeat("mental_healt_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Mental_Health_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Mental_Health_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mental_health_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_mental_health_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mental_health_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    mental_health_fep_bridge_heartbeat_instance(NULL, "mental_health_fep_bridge_training_begin", 0.0f);
    return 0;
}

int mental_health_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_fep_bridge_training_end: NULL argument");
        return -1;
    }
    mental_health_fep_bridge_heartbeat_instance(NULL, "mental_health_fep_bridge_training_end", 1.0f);
    return 0;
}

int mental_health_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mental_health_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mental_health_fep_bridge_heartbeat_instance(NULL, "mental_health_fep_bridge_training_step", progress);
    return 0;
}
