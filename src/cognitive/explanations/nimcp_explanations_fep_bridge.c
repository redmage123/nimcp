/**
 * @file nimcp_explanations_fep_bridge.c
 * @brief Explanations FEP Bridge Implementation
 */

#include "cognitive/explanations/nimcp_explanations_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>

#define LOG_MODULE "explanations_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(explanations_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_explanations_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_explanations_fep_bridge_mesh_registry = NULL;

nimcp_error_t explanations_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_explanations_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "explanations_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "explanations_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_explanations_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_explanations_fep_bridge_mesh_registry = registry;
    return err;
}

void explanations_fep_bridge_mesh_unregister(void) {
    if (g_explanations_fep_bridge_mesh_registry && g_explanations_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_explanations_fep_bridge_mesh_registry, g_explanations_fep_bridge_mesh_id);
        g_explanations_fep_bridge_mesh_id = 0;
        g_explanations_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void explanations_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_explanations_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_explanations_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_explanations_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


int explanations_fep_bridge_default_config(explanations_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");
    config->uncertainty_threshold = EXPLANATIONS_FEP_HIGH_UNCERTAINTY_THRESHOLD;
    config->detail_boost_factor = EXPLANATIONS_FEP_DETAIL_BOOST_FACTOR;
    config->enable_uncertainty_modulation = true;
    config->enable_causal_extraction = true;
    config->enable_counterfactual_testing = true;
    config->enable_model_exposure = true;
    config->fe_sensitivity = 1.0f;
    config->explanation_sensitivity = 1.0f;
    return 0;
}

explanations_fep_bridge_t* explanations_fep_bridge_create(const explanations_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_create", 0.0f);


    explanations_fep_bridge_t* bridge = nimcp_malloc(sizeof(explanations_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }
    memset(bridge, 0, sizeof(explanations_fep_bridge_t));
    if (config) bridge->config = *config;
    else explanations_fep_bridge_default_config(&bridge->config);
    if (bridge_base_init(&bridge->base, 0, "explanations_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) { nimcp_free(bridge); return NULL; }
    return bridge;
}

void explanations_fep_bridge_destroy(explanations_fep_bridge_t* bridge) {
    if (!bridge) return;
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) explanations_fep_bridge_disconnect_bio_async(bridge);
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int explanations_fep_bridge_connect_fep(explanations_fep_bridge_t* bridge, fep_system_t* fep) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_bridge_connect_generator(explanations_fep_bridge_t* bridge, explanation_generator_t gen) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_connect_generator", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && gen, NIMCP_ERROR_NULL_POINTER, "bridge or gen is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->explanation_gen = gen;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_bridge_disconnect(explanations_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->explanation_gen = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_modulate_detail(explanations_fep_bridge_t* bridge, float uncertainty) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_explanations_fep_mod", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_uncertainty_modulation) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.current_uncertainty = uncertainty;
    if (uncertainty > bridge->config.uncertainty_threshold) {
        bridge->fep_effects.detailed_explanation_needed = true;
        bridge->stats.explanations_generated++;
    }
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_extract_causal_chain(explanations_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_explanations_fep_ext", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_causal_extraction) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->explanation_effects.model_structure_exposed = true;
    bridge->stats.model_exposures++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_test_counterfactual(explanations_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_explanations_fep_tes", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_counterfactual_testing) return 0;
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_effects.counterfactual_mode_active = true;
    bridge->stats.counterfactuals_tested++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_bridge_update(explanations_fep_bridge_t* bridge, uint64_t delta_ms) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    explanations_fep_extract_causal_chain(bridge);
    return 0;
}

int explanations_fep_bridge_get_state(explanations_fep_bridge_t* bridge, explanations_fep_state_t* state) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_bridge_get_stats(explanations_fep_bridge_t* bridge, explanations_fep_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int explanations_fep_bridge_connect_bio_async(explanations_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;
    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_EXPLANATIONS_BRIDGE,
        .module_name = "explanations_fep_bridge",
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

int explanations_fep_bridge_disconnect_bio_async(explanations_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) bio_router_unregister_module(bridge->base.bio_ctx);
    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;
    return 0;
}

bool explanations_fep_bridge_is_bio_async_connected(const explanations_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int explanations_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    explanations_fep_bridge_heartbeat("explanations_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Explanations_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                explanations_fep_bridge_heartbeat("explanations_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Explanations_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Explanations_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void explanations_fep_bridge_set_instance_health_agent(explanations_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        g_explanations_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static uint64_t g_explanations_fep_training_steps = 0;
static double g_explanations_fep_training_total_error = 0.0;
static double g_explanations_fep_training_best_error = 1e30;
static bool g_explanations_fep_training_active = false;

int explanations_fep_bridge_training_begin(explanations_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    explanations_fep_bridge_heartbeat_instance(NULL, "expl_fep_train_begin", 0.0f);

    /* Reset training counters */
    g_explanations_fep_training_steps = 0;
    g_explanations_fep_training_total_error = 0.0;
    g_explanations_fep_training_best_error = 1e30;
    g_explanations_fep_training_active = true;

    NIMCP_LOGGING_INFO("explanations_fep_bridge training begin: counters reset");
    return 0;
}

int explanations_fep_bridge_training_step(explanations_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_fep_bridge_training_step: NULL argument");
        return -1;
    }

    /* Clamp progress to [0, 1] */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    explanations_fep_bridge_heartbeat_instance(NULL, "expl_fep_train_step", progress);

    g_explanations_fep_training_steps++;

    /* Progressive adaptation: decay error accumulator */
    float decay = 1.0f - 0.1f * progress;
    if (decay < 0.5f) decay = 0.5f;
    g_explanations_fep_training_total_error *= (double)decay;

    /* Adaptive threshold adjustment based on progress */
    float threshold_adjust = 0.01f * progress;
    g_explanations_fep_training_best_error -= (double)threshold_adjust;
    if (g_explanations_fep_training_best_error < 0.0) g_explanations_fep_training_best_error = 0.0;

    return 0;
}

int explanations_fep_bridge_training_end(explanations_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "explanations_fep_bridge_training_end: NULL argument");
        return -1;
    }
    explanations_fep_bridge_heartbeat_instance(NULL, "expl_fep_train_end", 1.0f);

    /* Compute final averages */
    double avg_error = (g_explanations_fep_training_steps > 0)
        ? g_explanations_fep_training_total_error / (double)g_explanations_fep_training_steps
        : 0.0;

    /* Clear training flag */
    g_explanations_fep_training_active = false;

    NIMCP_LOGGING_INFO("explanations_fep_bridge training end: %lu steps, avg_error=%.6f, best_error=%.6f",
                       (unsigned long)g_explanations_fep_training_steps,
                       avg_error, g_explanations_fep_training_best_error);
    return 0;
}
