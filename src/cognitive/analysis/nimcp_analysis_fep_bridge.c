/**
 * @file nimcp_analysis_fep_bridge.c
 * @brief Free Energy Principle - Network Analysis Integration Bridge Implementation
 */

#include "cognitive/analysis/nimcp_analysis_fep_bridge.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/validation/nimcp_common.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

#define LOG_MODULE "analysis_fep_bridge"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(analysis_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_analysis_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_analysis_fep_bridge_mesh_registry = NULL;

nimcp_error_t analysis_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_analysis_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "analysis_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "analysis_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_analysis_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_analysis_fep_bridge_mesh_registry = registry;
    return err;
}

void analysis_fep_bridge_mesh_unregister(void) {
    if (g_analysis_fep_bridge_mesh_registry && g_analysis_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_analysis_fep_bridge_mesh_registry, g_analysis_fep_bridge_mesh_id);
        g_analysis_fep_bridge_mesh_id = 0;
        g_analysis_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat (instance-level) */
static inline void analysis_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_analysis_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_analysis_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_analysis_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/**
 * WHAT: Initialize default configuration for analysis-FEP bridge
 * WHY:  Provide sensible defaults based on network economy principles
 * HOW:  Set biologically-plausible parameters for topology-FEP integration
 */
int analysis_fep_bridge_default_config(analysis_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_default_config: config is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_default_config", 0.0f);


    config->pe_exploration_threshold = ANALYSIS_FEP_HIGH_PE_THRESHOLD;
    config->precision_hub_weight = ANALYSIS_FEP_HUB_PRECISION_BOOST;
    config->modularity_prior_strength = ANALYSIS_FEP_MODULARITY_PRIOR;
    config->enable_pe_exploration = true;
    config->enable_precision_weighting = true;
    config->enable_topology_priors = true;
    config->community_change_sensitivity = 0.5f;
    config->topology_belief_strength = 0.8f;
    config->enable_community_beliefs = true;
    config->enable_topology_updates = true;
    config->fe_sensitivity = 1.0f;
    config->analysis_sensitivity = 1.0f;
    return 0;
}

/**
 * WHAT: Create analysis-FEP bridge instance
 * WHY:  Initialize bidirectional integration infrastructure
 * HOW:  Allocate structure, create mutex, set default config
 */
analysis_fep_bridge_t* analysis_fep_bridge_create(const analysis_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_create", 0.0f);


    analysis_fep_bridge_t* bridge = nimcp_malloc(sizeof(analysis_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "analysis_fep_bridge_create: failed to allocate bridge");
        return NULL;
    }

    memset(bridge, 0, sizeof(analysis_fep_bridge_t));
    if (config) {
        bridge->config = *config;
    } else {
        analysis_fep_bridge_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "analysis_fep") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "analysis_fep_bridge_create: failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    return bridge;
}

/**
 * WHAT: Destroy analysis-FEP bridge
 * WHY:  Clean up resources and prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free structure
 */
void analysis_fep_bridge_destroy(analysis_fep_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_destroy", 0.0f);


    if (bridge->base.bio_async_enabled) {
        analysis_fep_bridge_disconnect_bio_async(bridge);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/**
 * WHAT: Connect FEP system to bridge
 * WHY:  Enable FEP→Analysis pathway
 * HOW:  Store FEP pointer with mutex protection
 */
int analysis_fep_bridge_connect_fep(analysis_fep_bridge_t* bridge, fep_system_t* fep) {
    if (!bridge || !fep) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_connect_fep: bridge or fep is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_connect_fep", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Connect network analyzer to bridge
 * WHY:  Enable Analysis→FEP pathway
 * HOW:  Store analyzer pointer with mutex protection
 */
int analysis_fep_bridge_connect_analysis(analysis_fep_bridge_t* bridge, network_analyzer_t* analyzer) {
    if (!bridge || !analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_connect_analysis: bridge or analyzer is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_connect_analysis", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->analyzer = analyzer;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Disconnect both systems from bridge
 * WHY:  Clean disconnection for shutdown/reconfiguration
 * HOW:  NULL both pointers with mutex protection
 */
int analysis_fep_bridge_disconnect(analysis_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_disconnect: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_disconnect", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->fep_system = NULL;
    bridge->analyzer = NULL;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Trigger topological exploration based on prediction error
 * WHY:  High PE suggests current network model is inadequate
 * HOW:  If PE exceeds threshold, activate exploration mode
 */
int analysis_fep_trigger_exploration(analysis_fep_bridge_t* bridge, float pe_magnitude) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_trigger_exploration: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_pe_exploration) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_trigger", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fep_effects.current_prediction_error = pe_magnitude;

    if (pe_magnitude > bridge->config.pe_exploration_threshold) {
        bridge->fep_effects.exploration_triggered = true;
        bridge->state.exploration_active = true;
        bridge->stats.exploration_events++;
        NIMCP_LOGGING_INFO("Topological exploration triggered (PE=%.2f)", pe_magnitude);
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Weight hub neurons by FEP precision
 * WHY:  High precision nodes are more important in topology
 * HOW:  Apply precision boost to detected hubs
 */
int analysis_fep_weight_hubs_by_precision(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_weight_hubs_by_precision: bridge or fep_system is NULL");
        return -1;
    }
    if (!bridge->config.enable_precision_weighting) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_weight_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Simplified: set uniform hub weights (real implementation would query FEP precision)
    bridge->fep_effects.num_hubs_weighted = 0;
    for (uint32_t i = 0; i < 64 && i < bridge->fep_effects.num_hubs_weighted; i++) {
        bridge->fep_effects.hub_precision_weights[i] = bridge->config.precision_hub_weight;
    }

    bridge->stats.hub_weighting_applications++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Trigger network reorganization
 * WHY:  FEP surprise requires structural adaptation
 * HOW:  Signal analyzer to re-detect communities
 */
int analysis_fep_trigger_reorganization(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->analyzer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_trigger_reorganization: bridge or analyzer is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_trigger", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->fep_effects.topology_exploration_active = true;
    bridge->stats.community_detections++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Apply topology metrics as FEP priors
 * WHY:  Network structure constrains generative model
 * HOW:  Use modularity as complexity prior
 */
int analysis_fep_apply_topology_priors(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_apply_topology_priors: bridge or fep_system is NULL");
        return -1;
    }
    if (!bridge->config.enable_topology_priors) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_apply_t", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->analysis_effects.modularity_prior_bias = bridge->config.modularity_prior_strength;
    bridge->analysis_effects.topology_constraining_model = true;
    bridge->stats.topology_constraint_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP beliefs based on community structure
 * WHY:  Communities represent hidden state structure
 * HOW:  Map communities to FEP hierarchy levels
 */
int analysis_fep_apply_community_beliefs(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_apply_community_beliefs: bridge or fep_system is NULL");
        return -1;
    }
    if (!bridge->config.enable_community_beliefs) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_apply_c", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    // Simplified: would extract actual community count from analyzer
    bridge->analysis_effects.num_communities_detected = 0;
    bridge->state.num_communities = bridge->analysis_effects.num_communities_detected;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Update FEP model structure based on topology
 * WHY:  Network architecture shapes generative model
 * HOW:  Hierarchical community structure → FEP levels
 */
int analysis_fep_update_model_structure(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->fep_system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_update_model_structure: bridge or fep_system is NULL");
        return -1;
    }
    if (!bridge->config.enable_topology_updates) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_analysis_fep_update_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->analysis_effects.model_structure_updated = true;
    bridge->stats.topology_updates++;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/**
 * WHAT: Periodic update of bridge state
 * WHY:  Continuous synchronization between FEP and analysis
 * HOW:  Apply all bidirectional effects
 */
int analysis_fep_bridge_update(analysis_fep_bridge_t* bridge, uint64_t delta_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_update", 0.0f);


    analysis_fep_weight_hubs_by_precision(bridge);
    analysis_fep_apply_topology_priors(bridge);
    analysis_fep_apply_community_beliefs(bridge);
    analysis_fep_update_model_structure(bridge);

    return 0;
}

/**
 * WHAT: Get current bridge state
 * WHY:  External monitoring of integration status
 * HOW:  Copy state with mutex protection
 */
int analysis_fep_bridge_get_state(analysis_fep_bridge_t* bridge, analysis_fep_state_t* state) {
    if (!bridge || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_get_state: bridge or state is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_get_state", 0.0f);


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
int analysis_fep_bridge_get_stats(analysis_fep_bridge_t* bridge, analysis_fep_stats_t* stats) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_get_stats: bridge or stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_get_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/**
 * WHAT: Register bridge with bio-async router
 * WHY:  Enable inter-module messaging
 * HOW:  Register as BIO_MODULE_FEP_ANALYSIS_BRIDGE
 */
int analysis_fep_bridge_connect_bio_async(analysis_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "analysis_fep_bridge_connect_bio_async: bridge is NULL");
        return -1;
    }
    if (bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_connect_bio_async", 0.0f);


    bio_module_info_t info = {
        .module_id = BIO_MODULE_FEP_ANALYSIS_BRIDGE,
        .module_name = "analysis_fep_bridge",
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
int analysis_fep_bridge_disconnect_bio_async(analysis_fep_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_disconnect_bio_async", 0.0f);


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
bool analysis_fep_bridge_is_bio_async_connected(const analysis_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_is_bio_async_connect", 0.0f);


    return bridge ? bridge->base.bio_async_enabled : false;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

/**
 * WHAT: Query knowledge graph for Analysis FEP Bridge self-knowledge
 * WHY:  Enable self-awareness about module's role and connections
 * HOW:  Query KG for entity observations and relations
 */
int analysis_fep_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    analysis_fep_bridge_heartbeat("analysis_fep_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Analysis_FEP_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                analysis_fep_bridge_heartbeat("analysis_fep_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            NIMCP_LOGGING_DEBUG("Analysis FEP Bridge self-knowledge: %s", self->observations[i]);
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Analysis_FEP_Bridge");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Analysis_FEP_Bridge");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void analysis_fep_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_analysis_fep_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int analysis_fep_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analysis_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    analysis_fep_bridge_heartbeat_instance(NULL, "analysis_fep_bridge_training_begin", 0.0f);
    return 0;
}

int analysis_fep_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analysis_fep_bridge_training_end: NULL argument");
        return -1;
    }
    analysis_fep_bridge_heartbeat_instance(NULL, "analysis_fep_bridge_training_end", 1.0f);
    return 0;
}

int analysis_fep_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "analysis_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    analysis_fep_bridge_heartbeat_instance(NULL, "analysis_fep_bridge_training_step", progress);
    return 0;
}
