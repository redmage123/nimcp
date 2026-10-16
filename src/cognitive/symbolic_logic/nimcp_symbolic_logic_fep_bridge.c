/**
 * @file nimcp_symbolic_logic_fep_bridge.c
 * @brief Symbolic Logic FEP Bridge Implementation
 */

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define LOG_MODULE "symbolic_logic_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(symbolic_logic_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_symbolic_logic_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_symbolic_logic_fep_bridge_mesh_registry = NULL;

nimcp_error_t symbolic_logic_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_symbolic_logic_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "symbolic_logic_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "symbolic_logic_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_symbolic_logic_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_symbolic_logic_fep_bridge_mesh_registry = registry;
    return err;
}

void symbolic_logic_fep_bridge_mesh_unregister(void) {
    if (g_symbolic_logic_fep_bridge_mesh_registry && g_symbolic_logic_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_symbolic_logic_fep_bridge_mesh_registry, g_symbolic_logic_fep_bridge_mesh_id);
        g_symbolic_logic_fep_bridge_mesh_id = 0;
        g_symbolic_logic_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from symbolic_logic_fep_bridge module (instance-level) */
static inline void symbolic_logic_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_symbolic_logic_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_symbolic_logic_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_symbolic_logic_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


int symbolic_logic_fep_bridge_default_config(symbolic_logic_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->pe_exploration_threshold = LOGIC_FEP_HIGH_PE_THRESHOLD;
    config->proof_precision_factor = LOGIC_FEP_PROOF_PRECISION_FACTOR;
    config->enable_pe_exploration = true;
    config->enable_proof_validation = true;
    config->enable_novelty_weighting = true;
    config->enable_salience_precision = true;
    config->fe_sensitivity = 1.0f;
    config->logic_sensitivity = 1.0f;
    return 0;
}

symbolic_logic_fep_bridge_t* symbolic_logic_fep_bridge_create(const symbolic_logic_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_create", 0.0f);


    symbolic_logic_fep_bridge_t* bridge = nimcp_malloc(sizeof(symbolic_logic_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(symbolic_logic_fep_bridge_t));
    if (config) bridge->config = *config;
    else symbolic_logic_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "symbolic_logic_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void symbolic_logic_fep_bridge_destroy(symbolic_logic_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) symbolic_logic_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int symbolic_logic_fep_bridge_connect_fep(symbolic_logic_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_bridge_connect_logic(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_t* logic) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_connect_logic", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && logic, NIMCP_ERROR_NULL_POINTER, "bridge or logic is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logic_system = logic;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_bridge_disconnect(symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->logic_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_trigger_exploration(symbolic_logic_fep_bridge_t* bridge, float pe_magnitude) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_symbolic_logic_fep_t", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_exploration) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.current_prediction_error = pe_magnitude;
    if (pe_magnitude > bridge->config.pe_exploration_threshold) {
        bridge->fep_effects.logical_exploration_triggered = true;
        bridge->state.exploration_active = true;
        bridge->stats.exploration_events++;
        NIMCP_LOGGING_INFO("Logical exploration triggered (PE=%.2f)", pe_magnitude);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_weight_facts_by_confidence(symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_symbolic_logic_fep_w", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_salience_precision) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.num_salient_facts = 0;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_validate_beliefs_by_proof(symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_symbolic_logic_fep_v", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_proof_validation) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logic_effects.logic_validating_beliefs = true;
    bridge->stats.proof_validations++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_trigger_revision_from_contradiction(symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_symbolic_logic_fep_t", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->logic_effects.belief_revision_triggered = true;
    bridge->stats.belief_revisions++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_bridge_update(symbolic_logic_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    symbolic_logic_fep_weight_facts_by_confidence(bridge);
    symbolic_logic_fep_validate_beliefs_by_proof(bridge);
    return 0;
}

int symbolic_logic_fep_bridge_get_state(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_bridge_get_stats(symbolic_logic_fep_bridge_t* bridge, symbolic_logic_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int symbolic_logic_fep_bridge_connect_bio_async(symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_LOGIC_BRIDGE,
        .module_name = "symbolic_logic_fep_bridge",
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

int symbolic_logic_fep_bridge_disconnect_bio_async(symbolic_logic_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool symbolic_logic_fep_bridge_is_bio_async_connected(const symbolic_logic_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int symbolic_logic_fep_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_symbolic_logic_fep_q", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Symbolic_Logic_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                symbolic_logic_fep_bridge_heartbeat("symbolic_log_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Symbolic_Logic_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Symbolic_Logic_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * FEP Orchestrator Integration
 * ============================================================================ */

#include "cognitive/free_energy/nimcp_fep_orchestrator.h"

int symbolic_logic_fep_bridge_update_wrapper(void* bridge_handle) {
    if (!bridge_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "symbolic_logic_fep_bridge_update_wrapper: bridge_handle is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_update_wrapper", 0.0f);


    symbolic_logic_fep_bridge_t* bridge = (symbolic_logic_fep_bridge_t*)bridge_handle;
    return symbolic_logic_fep_bridge_update(bridge, 0);
}

int symbolic_logic_fep_bridge_register_with_orchestrator(
    symbolic_logic_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    uint32_t* bridge_id_out)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_register_with_orches", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && orchestrator, NIMCP_ERROR_NULL_POINTER, "bridge or orchestrator is NULL");

    int result = fep_orchestrator_register_bridge(
        orchestrator,
        "symbolic_logic_fep_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)bridge,
        symbolic_logic_fep_bridge_update_wrapper,
        NULL,  /* Don't auto-destroy - caller manages lifecycle */
        bridge_id_out);

    if (result == 0) {
        NIMCP_LOGGING_INFO("Registered with FEP orchestrator (bridge_id=%u)",
            bridge_id_out ? *bridge_id_out : 0);
    } else {
        NIMCP_LOGGING_ERROR("Failed to register with FEP orchestrator");
    }

    return result;
}

int symbolic_logic_fep_bridge_unregister_from_orchestrator(
    symbolic_logic_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator)
{
    /* Phase 8: Heartbeat at operation start */
    symbolic_logic_fep_bridge_heartbeat("symbolic_log_unregister_from_orch", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && orchestrator, NIMCP_ERROR_NULL_POINTER, "bridge or orchestrator is NULL");

    /* Note: Need to track bridge_id to unregister properly */
    /* For now, this is a placeholder - full implementation would store bridge_id */
    NIMCP_LOGGING_DEBUG("Unregister from FEP orchestrator requested");
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void symbolic_logic_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_symbolic_logic_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training stubs
 * ============================================================================ */
int symbolic_logic_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    symbolic_logic_fep_bridge_heartbeat_instance(NULL, "symbolic_logic_fep_bridge_training_begin", 0.0f);
    return 0;
}

int symbolic_logic_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_fep_bridge_training_end: NULL argument");
        return -1;
    }
    symbolic_logic_fep_bridge_heartbeat_instance(NULL, "symbolic_logic_fep_bridge_training_end", 1.0f);
    return 0;
}

int symbolic_logic_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "symbolic_logic_fep_bridge_training_step: NULL argument");
        return -1;
    }
    symbolic_logic_fep_bridge_heartbeat_instance(NULL, "symbolic_logic_fep_bridge_training_step", progress);
    return 0;
}
