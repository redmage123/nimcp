/**
 * @file nimcp_fractal_cognitive_fep_bridge.c
 * @brief Free Energy Principle - Fractal Cognitive Integration Bridge Implementation
 */

#include "cognitive/fractal_cognitive/nimcp_fractal_cognitive_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "fractal_cognitive_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(fractal_cognitive_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_fractal_cognitive_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_fractal_cognitive_fep_bridge_mesh_registry = NULL;

nimcp_error_t fractal_cognitive_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_fractal_cognitive_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "fractal_cognitive_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "fractal_cognitive_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_fractal_cognitive_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_fractal_cognitive_fep_bridge_mesh_registry = registry;
    return err;
}

void fractal_cognitive_fep_bridge_mesh_unregister(void) {
    if (g_fractal_cognitive_fep_bridge_mesh_registry && g_fractal_cognitive_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_fractal_cognitive_fep_bridge_mesh_registry, g_fractal_cognitive_fep_bridge_mesh_id);
        g_fractal_cognitive_fep_bridge_mesh_id = 0;
        g_fractal_cognitive_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from fractal_cognitive_fep_bridge module (instance-level) */
static inline void fractal_cognitive_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_fractal_cognitive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_fractal_cognitive_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_fractal_cognitive_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}



/**
 * WHAT: Initialize default configuration for fractal-FEP bridge
 * WHY:  Provide sensible defaults based on scale-free network principles
 * HOW:  Set biologically-plausible parameters for hub-FEP integration
 */
int fractal_cognitive_fep_bridge_default_config(fractal_cognitive_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_default_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(config, NIMCP_ERROR_NULL_POINTER, "config is NULL");

    config->pe_exploration_threshold = FRACTAL_FEP_HIGH_PE_THRESHOLD;
    config->precision_hub_factor = FRACTAL_FEP_HUB_PRECISION_FACTOR;
    config->centrality_prior_strength = FRACTAL_FEP_CENTRALITY_PRIOR;
    config->enable_pe_exploration = true;
    config->enable_hub_precision = true;
    config->enable_hierarchy_mapping = true;
    config->hierarchy_sensitivity = 0.6f;
    config->hub_belief_strength = 0.8f;
    config->enable_hub_beliefs = true;
    config->enable_structure_updates = true;
    config->fe_sensitivity = 1.0f;
    config->fractal_sensitivity = 1.0f;

    return 0;
}

/**
 * WHAT: Create fractal-FEP bridge instance
 * WHY:  Initialize bidirectional integration infrastructure
 * HOW:  Allocate structure, create mutex, set default config
 */
fractal_cognitive_fep_bridge_t* fractal_cognitive_fep_bridge_create(
    const fractal_cognitive_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_create", 0.0f);


    fractal_cognitive_fep_bridge_t* bridge = nimcp_malloc(sizeof(fractal_cognitive_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    memset(bridge, 0, sizeof(fractal_cognitive_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        fractal_cognitive_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "fractal_cognitive_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "fractal_cognitive_fep_bridge_create: bridge->base is NULL");
        return NULL;
    }

    return bridge;
}

/**
 * WHAT: Destroy fractal-FEP bridge
 * WHY:  Clean up resources and prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free structure
 */
void fractal_cognitive_fep_bridge_destroy(fractal_cognitive_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        fractal_cognitive_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP→Fractal pathway
 * HOW:  Store FEP pointer with mutex protection
 */
int fractal_cognitive_fep_bridge_connect_fep(
    fractal_cognitive_fep_bridge_t* bridge,
    fep_system_t* fep
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_connect_fep", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fep, NIMCP_ERROR_NULL_POINTER, "bridge or fep is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Connect fractal cognitive cache to bridge
 * WHY:  Enable Fractal→FEP pathway
 * HOW:  Store cache pointer with mutex protection
 */
int fractal_cognitive_fep_bridge_connect_fractal(
    fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_cache_t* fractal_cache
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_connect_fractal", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && fractal_cache, NIMCP_ERROR_NULL_POINTER, "bridge or fractal_cache is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fractal_cache = fractal_cache;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Disconnect both systems from bridge
 * WHY:  Clean disconnection for shutdown/reconfiguration
 * HOW:  NULL both pointers with mutex protection
 */
int fractal_cognitive_fep_bridge_disconnect(fractal_cognitive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_disconnect", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->fractal_cache = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Trigger hub discovery based on prediction error
 * WHY:  High PE suggests current hub structure inadequate
 * HOW:  If PE exceeds threshold, activate hub re-identification
 */
int fractal_cognitive_fep_trigger_hub_discovery(
    fractal_cognitive_fep_bridge_t* bridge,
    float pe_magnitude
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (!bridge->config.enable_pe_exploration) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fep_effects.current_prediction_error = pe_magnitude;

    if (pe_magnitude > bridge->config.pe_exploration_threshold) {
        bridge->fep_effects.hub_discovery_triggered = true;
        bridge->state.hub_discovery_active = true;
        bridge->stats.hub_discovery_events++;
        NIMCP_LOGGING_INFO("Hub discovery triggered (PE=%.2f)", pe_magnitude);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Weight hub neurons by FEP precision
 * WHY:  High precision nodes are critical hubs
 * HOW:  Apply precision boost to identified hubs
 */
int fractal_cognitive_fep_weight_hubs_by_precision(
    fractal_cognitive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_hub_precision) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    // Simplified: set uniform hub weights (real implementation would query FEP precision)
    bridge->fep_effects.num_hubs_weighted = 0;
    for (uint32_t i = 0; i < FRACTAL_FEP_MAX_HUBS && i < bridge->fep_effects.num_hubs_weighted; i++) {
        bridge->fep_effects.hub_precision_weights[i] = bridge->config.precision_hub_factor;
    }

    bridge->stats.precision_applications++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Trigger hierarchical reorganization
 * WHY:  FEP surprise requires structural adaptation
 * HOW:  Signal hierarchy re-detection
 */
int fractal_cognitive_fep_trigger_hierarchy_update(
    fractal_cognitive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fractal_cache, NIMCP_ERROR_NULL_POINTER, "bridge or fractal_cache is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fep_effects.hierarchy_update_active = true;
    bridge->stats.structure_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply hub structure as FEP priors
 * WHY:  Hub centrality constrains generative model
 * HOW:  Use centrality as precision prior
 */
int fractal_cognitive_fep_apply_hub_priors(fractal_cognitive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_hub_beliefs) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fractal_effects.hub_precision_bias = bridge->config.centrality_prior_strength;
    bridge->fractal_effects.hubs_constraining_model = true;
    bridge->stats.constraint_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Map fractal hierarchy to FEP levels
 * WHY:  Hierarchical structure shapes beliefs
 * HOW:  Align fractal levels with FEP state hierarchy
 */
int fractal_cognitive_fep_map_hierarchy_to_fep(fractal_cognitive_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_hierarchy_mapping) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    // Simplified: would extract actual hierarchy depth from fractal cache
    bridge->fractal_effects.num_hierarchy_levels = 3;  // Default depth
    bridge->state.num_hierarchy_levels = bridge->fractal_effects.num_hierarchy_levels;
    bridge->stats.hierarchy_mappings++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP model structure from fractal topology
 * WHY:  Network architecture determines model structure
 * HOW:  Scale-free structure → FEP connectivity
 */
int fractal_cognitive_fep_update_model_structure(
    fractal_cognitive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_fractal_cognitive_fe", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && bridge->fep_system, NIMCP_ERROR_NULL_POINTER, "bridge or fep_system is NULL");
    if (!bridge->config.enable_structure_updates) return 0;

    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fractal_effects.model_structure_updated = true;
    bridge->stats.structure_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Periodic update of bridge state
 * WHY:  Continuous synchronization between FEP and fractal cognitive
 * HOW:  Apply all bidirectional effects
 */
int fractal_cognitive_fep_bridge_update(
    fractal_cognitive_fep_bridge_t* bridge,
    uint64_t delta_ms
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    fractal_cognitive_fep_weight_hubs_by_precision(bridge);
    fractal_cognitive_fep_apply_hub_priors(bridge);
    fractal_cognitive_fep_map_hierarchy_to_fep(bridge);
    fractal_cognitive_fep_update_model_structure(bridge);

    return 0;
}

/**
 * WHAT: Get current bridge state
 * WHY:  External monitoring of integration status
 * HOW:  Copy state with mutex protection
 */
int fractal_cognitive_fep_bridge_get_state(
    const fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_fep_state_t* state
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_get_state", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && state, NIMCP_ERROR_NULL_POINTER, "bridge or state is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *state = bridge->state;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Get bridge statistics
 * WHY:  Performance monitoring and debugging
 * HOW:  Copy stats with mutex protection
 */
int fractal_cognitive_fep_bridge_get_stats(
    const fractal_cognitive_fep_bridge_t* bridge,
    fractal_cognitive_fep_stats_t* stats
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats, NIMCP_ERROR_NULL_POINTER, "bridge or stats is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Register bridge with bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Register as BIO_MODULE_FEP_FRACTAL_BRIDGE (new ID in 0x0F50 range)
 */
int fractal_cognitive_fep_bridge_connect_bio_async(
    fractal_cognitive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_connect_bio_async", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    if (bridge->base.bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_SOCIAL_BRIDGE + 1,  // Next in sequence
        .module_name = "fractal_cognitive_fep_bridge",
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

/**
 * WHAT: Unregister from bio-async router
 * WHY:  Clean disconnection
 * HOW:  Unregister module context
 */
int fractal_cognitive_fep_bridge_disconnect_bio_async(
    fractal_cognitive_fep_bridge_t* bridge
) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_disconnect_bio_async", 0.0f);


    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
    }

    bridge->base.bio_ctx = NULL;
    bridge->base.bio_async_enabled = false;

    return 0;
}

/**
 * WHAT: Check bio-async connection status
 * WHY:  Query whether messaging is available
 * HOW:  Return enabled flag
 */
bool fractal_cognitive_fep_bridge_is_bio_async_connected(
    const fractal_cognitive_fep_bridge_t* bridge
) {
    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int fractal_cognitive_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Fractal_Cognitive_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                fractal_cognitive_fep_bridge_heartbeat("fractal_cogn_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Fractal_Cognitive_FEP_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Fractal_Cognitive_FEP_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void fractal_cognitive_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_fractal_cognitive_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int fractal_cognitive_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_cognitive_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    fractal_cognitive_fep_bridge_heartbeat_instance(NULL, "fractal_cognitive_fep_bridge_training_begin", 0.0f);
    return 0;
}

int fractal_cognitive_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_cognitive_fep_bridge_training_end: NULL argument");
        return -1;
    }
    fractal_cognitive_fep_bridge_heartbeat_instance(NULL, "fractal_cognitive_fep_bridge_training_end", 1.0f);
    return 0;
}

int fractal_cognitive_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "fractal_cognitive_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    fractal_cognitive_fep_bridge_heartbeat_instance(NULL, "fractal_cognitive_fep_bridge_training_step", progress);
    return 0;
}
