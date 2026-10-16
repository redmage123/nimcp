/**
 * @file nimcp_love_loyalty_friendship_fep_bridge.c
 * @brief Free Energy Principle - Social Bonding Integration Bridge Implementation
 */

#include "cognitive/love_loyalty_friendship/nimcp_love_loyalty_friendship_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "social_bond_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(love_loyalty_friendship_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from love_loyalty_friendship_fep_bridge module (instance-level) */
static inline void love_loyalty_friendship_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_love_loyalty_friendship_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_love_loyalty_friendship_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_love_loyalty_friendship_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



int social_bond_fep_bridge_default_config(social_bond_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->pe_anxiety_threshold = SOCIAL_FEP_HIGH_PE_THRESHOLD;
    config->trust_precision_factor = SOCIAL_FEP_TRUST_PRECISION_FACTOR;
    config->loyalty_prior_strength = SOCIAL_FEP_LOYALTY_PRIOR_STRENGTH;
    config->enable_pe_attachment = true;
    config->enable_trust_precision = true;
    config->enable_relationship_priors = true;
    config->attachment_sensitivity = 0.7f;
    config->closeness_belief_strength = 0.8f;
    config->enable_closeness_beliefs = true;
    config->enable_betrayal_updates = true;
    config->fe_sensitivity = 1.0f;
    config->social_sensitivity = 1.0f;
    return 0;
}

social_bond_fep_bridge_t* social_bond_fep_bridge_create(const social_bond_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    social_bond_fep_bridge_t* bridge = nimcp_malloc(sizeof(social_bond_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(social_bond_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        social_bond_fep_bridge_default_config(&bridge->config);
    }
    if (bridge_base_init(&bridge->base, 0, "love_loyalty_friendship_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "social_bond_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }
    return bridge;
}

void social_bond_fep_bridge_destroy(social_bond_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    if (bridge->base.bio_async_enabled) {
        social_bond_fep_bridge_disconnect_bio_async(bridge);
    }
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int social_bond_fep_bridge_connect_fep(social_bond_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_bridge_connect_social(social_bond_fep_bridge_t* bridge, social_bond_system_t* social) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && social, NIMCP_ERROR_NULL_POINTER, "bridge or social is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->social_system = social;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_bridge_disconnect(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->social_system = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_trigger_attachment_anxiety(social_bond_fep_bridge_t* bridge, float pe_magnitude) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_trig", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_attachment) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.current_prediction_error = pe_magnitude;
    if (pe_magnitude > bridge->config.pe_anxiety_threshold) {
        bridge->fep_effects.attachment_anxiety_triggered = true;
        bridge->state.attachment_anxiety_active = true;
        bridge->stats.attachment_anxiety_events++;
        NIMCP_LOGGING_INFO("Attachment anxiety triggered (PE=%.2f)", pe_magnitude);
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_modulate_trust_by_precision(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_modu", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_trust_precision) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    for (uint32_t i = 0; i < SOCIAL_FEP_MAX_RELATIONSHIPS && i < bridge->fep_effects.num_relationships_tracked; i++) {
        bridge->fep_effects.relationship_precision[i] = bridge->config.trust_precision_factor;
    }
    bridge->stats.precision_applications++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_trigger_relationship_revision(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_trig", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->social_system, NIMCP_ERROR_NULL_POINTER, "bridge or social_system is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.trust_update_active = true;
    bridge->stats.relationship_revisions++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_apply_attachment_priors(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_appl", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_relationship_priors) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->social_effects.attachment_security_bias = bridge->config.attachment_sensitivity;
    bridge->social_effects.trust_constraining_model = true;
    bridge->stats.belief_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_apply_closeness_beliefs(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_appl", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_closeness_beliefs) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->social_effects.closeness_prior_strength = bridge->config.closeness_belief_strength;
    bridge->state.num_close_relationships = 0;
    bridge->stats.belief_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_update_model_from_betrayal(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_upda", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_betrayal_updates) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->social_effects.model_beliefs_updated = true;
    bridge->stats.trust_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_bridge_update(social_bond_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    social_bond_fep_modulate_trust_by_precision(bridge);
    social_bond_fep_apply_attachment_priors(bridge);
    social_bond_fep_apply_closeness_beliefs(bridge);
    social_bond_fep_update_model_from_betrayal(bridge);
    return 0;
}

int social_bond_fep_bridge_get_state(social_bond_fep_bridge_t* bridge, social_bond_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_bridge_get_stats(social_bond_fep_bridge_t* bridge, social_bond_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int social_bond_fep_bridge_connect_bio_async(social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SOCIAL_BRIDGE,
        .module_name = "social_bond_fep_bridge",
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

int social_bond_fep_bridge_disconnect_bio_async(social_bond_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
    }
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool social_bond_fep_bridge_is_bio_async_connected(const social_bond_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int social_bond_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_social_bond_fep_brid", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Love_Loyalty_Friendship_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                love_loyalty_friendship_fep_bridge_heartbeat("love_loyalty_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Love_Loyalty_Friendship_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Love_Loyalty_Friendship_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

static nimcp_health_agent_t* g_love_loyalty_friendship_fep_bridge_instance_health_agent = NULL;

void love_loyalty_friendship_fep_bridge_set_instance_health_agent(nimcp_health_agent_t* agent) {
    g_love_loyalty_friendship_fep_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration Stubs
 * ============================================================================ */

int love_loyalty_friendship_fep_bridge_training_begin(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "love_loyalty_friendship_fep_bridge_training_begin: ctx is NULL");
        return -1;
    }
    love_loyalty_friendship_fep_bridge_heartbeat_instance(g_love_loyalty_friendship_fep_bridge_instance_health_agent, "love_loyalty_fep_training_begin", 0.0f);
    return 0;
}

int love_loyalty_friendship_fep_bridge_training_end(void* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "love_loyalty_friendship_fep_bridge_training_end: ctx is NULL");
        return -1;
    }
    love_loyalty_friendship_fep_bridge_heartbeat_instance(g_love_loyalty_friendship_fep_bridge_instance_health_agent, "love_loyalty_fep_training_end", 1.0f);
    return 0;
}

int love_loyalty_friendship_fep_bridge_training_step(void* ctx, float progress) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "love_loyalty_friendship_fep_bridge_training_step: ctx is NULL");
        return -1;
    }
    love_loyalty_friendship_fep_bridge_heartbeat_instance(g_love_loyalty_friendship_fep_bridge_instance_health_agent, "love_loyalty_fep_training_step", progress);
    return 0;
}
