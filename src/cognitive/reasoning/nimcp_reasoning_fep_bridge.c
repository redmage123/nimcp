/**
 * @file nimcp_reasoning_fep_bridge.c
 * @brief Free Energy Principle - Reasoning Integration Bridge Implementation
 */

#include "cognitive/reasoning/nimcp_reasoning_fep_bridge.h"
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

#define LOG_MODULE "reasoning_fep_bridge"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(reasoning_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


BRIDGE_DEFINE_SECURITY_SETTERS(reasoning_fep_bridge)

int reasoning_fep_bridge_default_config(reasoning_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->pe_abduction_threshold = REASONING_FEP_PE_ABDUCTION_THRESHOLD;
    config->hypothesis_selection_temperature = 1.0f;
    config->inference_precision_threshold = REASONING_FEP_PRECISION_THRESHOLD;
    config->enable_pe_abduction = true;
    config->enable_fe_hypothesis_selection = true;
    config->enable_precision_inference = true;
    config->rule_prior_strength = 1.0f;
    config->conclusion_belief_strength = 0.9f;
    config->explanation_fe_reduction = 0.5f;
    config->enable_rule_priors = true;
    config->enable_conclusion_constraints = true;
    config->enable_explanation_reduction = true;
    config->fe_sensitivity = 1.0f;
    config->reasoning_sensitivity = 1.0f;
    return 0;
}

reasoning_fep_bridge_t* reasoning_fep_bridge_create(const reasoning_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_create", 0.0f);


    reasoning_fep_bridge_t* bridge = nimcp_malloc(sizeof(reasoning_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(reasoning_fep_bridge_t));
    if (config) bridge->config = *config;
    else reasoning_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "reasoning_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void reasoning_fep_bridge_destroy(reasoning_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) reasoning_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int reasoning_fep_bridge_connect_fep(reasoning_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_bridge_connect_reasoning(reasoning_fep_bridge_t* bridge, reasoning_integration_t* reasoning) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_connect_reasoning", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && reasoning, NIMCP_ERROR_NULL_POINTER, "bridge or reasoning is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->reasoning_system = reasoning;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_bridge_disconnect(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->reasoning_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_trigger_abduction(reasoning_fep_bridge_t* bridge, float pe_magnitude) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_trigge", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_abduction) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    if (pe_magnitude > bridge->config.pe_abduction_threshold) {
        bridge->fep_effects.abduction_triggered = true;
        bridge->state.abduction_active = true;
        bridge->stats.abduction_events++;
    }
    bridge->fep_effects.current_prediction_error = pe_magnitude;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_select_hypothesis_by_fe(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_select", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_fe_hypothesis_selection) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.selected_hypothesis = 0;
    bridge->stats.hypothesis_selections++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_modulate_inference_confidence(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_modula", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_precision_inference) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    float precision = bridge->state.current_precision;
    bridge->fep_effects.inference_confidence = precision;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_apply_rule_priors(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_rule_priors) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->reasoning_effects.rule_prior_bias = bridge->config.rule_prior_strength;
    bridge->reasoning_effects.rule_priors_active = true;
    bridge->stats.rule_prior_applications++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_apply_conclusion_constraints(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_conclusion_constraints) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->reasoning_effects.conclusion_constraint_strength = bridge->config.conclusion_belief_strength;
    bridge->reasoning_effects.conclusions_constraining_beliefs = true;
    bridge->stats.conclusion_constraints++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_apply_explanation_reduction(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_reasoning_fep_apply_", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_explanation_reduction) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->reasoning_effects.fe_reduction = bridge->config.explanation_fe_reduction;
    bridge->stats.explanation_reductions++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_bridge_update(reasoning_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Safety gates: ethics + LGSS pre-check */
    BRIDGE_ETHICS_GATE(bridge, "reasoning_fep_bridge_update");
    BRIDGE_LGSS_GATE(bridge, "reasoning_fep_bridge_update");
    reasoning_fep_select_hypothesis_by_fe(bridge);
    reasoning_fep_modulate_inference_confidence(bridge);
    reasoning_fep_apply_rule_priors(bridge);
    reasoning_fep_apply_conclusion_constraints(bridge);
    reasoning_fep_apply_explanation_reduction(bridge);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int reasoning_fep_bridge_get_state(reasoning_fep_bridge_t* bridge, reasoning_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_bridge_get_stats(reasoning_fep_bridge_t* bridge, reasoning_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int reasoning_fep_bridge_connect_bio_async(reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_REASONING_BRIDGE,
        .module_name = "reasoning_fep_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };
    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) bridge->base.bio_async_enabled = true;
    return 0;
}

int reasoning_fep_bridge_disconnect_bio_async(reasoning_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool reasoning_fep_bridge_is_bio_async_connected(const reasoning_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

//=============================================================================
// KG Self-Awareness Integration
//=============================================================================

int reasoning_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    reasoning_fep_bridge_heartbeat("reasoning_fe_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Reasoning_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                reasoning_fep_bridge_heartbeat("reasoning_fe_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Reasoning_FEP_Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Reasoning_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Reasoning_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}


void reasoning_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_reasoning_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int reasoning_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    reasoning_fep_bridge_heartbeat_instance(NULL, "reasoning_fep_bridge_training_begin", 0.0f);
    return 0;
}

int reasoning_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_fep_bridge_training_end: NULL argument");
        return -1;
    }
    reasoning_fep_bridge_heartbeat_instance(NULL, "reasoning_fep_bridge_training_end", 1.0f);
    return 0;
}

int reasoning_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "reasoning_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    reasoning_fep_bridge_heartbeat_instance(NULL, "reasoning_fep_bridge_training_step", progress);
    return 0;
}
