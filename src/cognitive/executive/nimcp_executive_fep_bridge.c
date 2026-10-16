/**
 * @file nimcp_executive_fep_bridge.c
 * @brief Free Energy Principle - Executive Function Integration Bridge Implementation
 */

#include "cognitive/executive/nimcp_executive_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "executive_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(executive_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_executive_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_executive_fep_bridge_mesh_registry = NULL;

nimcp_error_t executive_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_executive_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "executive_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "executive_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_executive_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_executive_fep_bridge_mesh_registry = registry;
    return err;
}

void executive_fep_bridge_mesh_unregister(void) {
    if (g_executive_fep_bridge_mesh_registry && g_executive_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_executive_fep_bridge_mesh_registry, g_executive_fep_bridge_mesh_id);
        g_executive_fep_bridge_mesh_id = 0;
        g_executive_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void executive_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_executive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_executive_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_executive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

BRIDGE_DEFINE_SECURITY_SETTERS(executive_fep_bridge)

int executive_fep_bridge_default_config(executive_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->efe_temperature = EXECUTIVE_FEP_DEFAULT_TEMPERATURE;
    config->precision_exploration_threshold = EXECUTIVE_FEP_PRECISION_THRESHOLD;
    config->pe_control_threshold = EXECUTIVE_FEP_PE_CONTROL_THRESHOLD;
    config->enable_efe_policy_selection = true;
    config->enable_precision_exploration = true;
    config->enable_pe_cognitive_control = true;
    config->goal_prior_strength = 1.0f;
    config->wm_belief_persistence = 0.9f;
    config->inhibition_precision_reduction = 0.5f;
    config->enable_goal_priors = true;
    config->enable_wm_belief_maintenance = true;
    config->enable_inhibition_precision = true;
    config->efe_sensitivity = 1.0f;
    config->executive_sensitivity = 1.0f;
    return 0;
}

executive_fep_bridge_t* executive_fep_bridge_create(const executive_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_create", 0.0f);


    executive_fep_bridge_t* bridge = nimcp_malloc(sizeof(executive_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(executive_fep_bridge_t));
    if (config) bridge->config = *config;
    else executive_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "executive_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void executive_fep_bridge_destroy(executive_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) executive_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int executive_fep_bridge_connect_fep(executive_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_bridge_connect_executive(executive_fep_bridge_t* bridge, executive_controller_t* executive) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_connect_executive", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && executive, NIMCP_ERROR_NULL_POINTER, "bridge or executive is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive_system = executive;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_bridge_disconnect(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->executive_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_select_policy_by_efe(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_select", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_efe_policy_selection) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.selected_policy = 0;
    bridge->stats.policy_selections++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_modulate_exploration(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_modula", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_precision_exploration) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    float precision = bridge->state.current_precision;
    if (precision < bridge->config.precision_exploration_threshold) {
        bridge->fep_effects.exploration_mode = true;
        bridge->fep_effects.exploration_probability = EXECUTIVE_FEP_LOW_PRECISION_EXPLORE;
        bridge->stats.exploration_events++;
    } else {
        bridge->fep_effects.exploration_mode = false;
        bridge->fep_effects.exploration_probability = 1.0f - EXECUTIVE_FEP_HIGH_PRECISION_EXPLOIT;
    }
    bridge->state.exploration_mode = bridge->fep_effects.exploration_mode;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_trigger_cognitive_control(executive_fep_bridge_t* bridge, float pe_magnitude) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_trigge", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_cognitive_control) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (pe_magnitude > bridge->config.pe_control_threshold) {
        bridge->fep_effects.control_active = true;
        bridge->fep_effects.control_signal = EXECUTIVE_FEP_CONTROL_BOOST;
        bridge->stats.cognitive_control_triggers++;
    } else {
        bridge->fep_effects.control_active = false;
        bridge->fep_effects.control_signal = 1.0f;
    }
    bridge->state.control_active = bridge->fep_effects.control_active;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_apply_goal_priors(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_goal_priors) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive_effects.goal_prior_bias = bridge->config.goal_prior_strength;
    bridge->executive_effects.goal_prior_active = true;
    bridge->stats.goal_prior_applications++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_maintain_wm_beliefs(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_mainta", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_wm_belief_maintenance) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive_effects.wm_belief_strength = bridge->config.wm_belief_persistence;
    bridge->executive_effects.wm_maintenance_active = true;
    bridge->stats.wm_maintenance_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_apply_inhibition_precision(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_executive_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_inhibition_precision) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->executive_effects.precision_suppression = bridge->config.inhibition_precision_reduction;
    bridge->stats.inhibition_events++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_bridge_update(executive_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "executive_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "executive_fep_bridge_update");
    executive_fep_select_policy_by_efe(bridge);
    executive_fep_modulate_exploration(bridge);
    executive_fep_apply_goal_priors(bridge);
    executive_fep_maintain_wm_beliefs(bridge);
    executive_fep_apply_inhibition_precision(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int executive_fep_bridge_get_state(executive_fep_bridge_t* bridge, executive_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_bridge_get_stats(executive_fep_bridge_t* bridge, executive_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int executive_fep_bridge_connect_bio_async(executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EXECUTIVE_BRIDGE,
        .module_name = "executive_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int executive_fep_bridge_disconnect_bio_async(executive_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool executive_fep_bridge_is_bio_async_connected(const executive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Executive FEP Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int executive_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    executive_fep_bridge_heartbeat("executive_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Executive_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                executive_fep_bridge_heartbeat("executive_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Executive FEP Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Executive_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Executive_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void executive_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_executive_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int executive_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    executive_fep_bridge_heartbeat_instance(NULL, "executive_fep_bridge_training_begin", 0.0f);
    return 0;
}

int executive_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_fep_bridge_training_end: NULL argument");
        return -1;
    }
    executive_fep_bridge_heartbeat_instance(NULL, "executive_fep_bridge_training_end", 1.0f);
    return 0;
}

int executive_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "executive_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    executive_fep_bridge_heartbeat_instance(NULL, "executive_fep_bridge_training_step", progress);
    return 0;
}
