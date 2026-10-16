/**
 * @file nimcp_omni_immune_bridge.c
 * @brief Implementation of Omnidirectional Inference to Immune System Bridge
 */

#include "cognitive/immune/nimcp_omni_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(omni_immune_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_omni_immune_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_omni_immune_bridge_mesh_registry = NULL;

nimcp_error_t omni_immune_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_omni_immune_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "omni_immune_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_SECURITY);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "omni_immune_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_omni_immune_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_omni_immune_bridge_mesh_registry = registry;
    return err;
}

void omni_immune_bridge_mesh_unregister(void) {
    if (g_omni_immune_bridge_mesh_registry && g_omni_immune_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_omni_immune_bridge_mesh_registry, g_omni_immune_bridge_mesh_id);
        g_omni_immune_bridge_mesh_id = 0;
        g_omni_immune_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from omni_immune_bridge module (instance-level) */
static inline void omni_immune_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_omni_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_omni_immune_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_omni_immune_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "OMNI_IMMUNE_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static float compute_aggregated_pe(omni_immune_bridge_t* bridge) {
    float total_pe = 0.0f;
    float total_weight = 0.0f;

    if (bridge->jepa) {
        jepa_bidir_stats_t stats;
        if (jepa_bidirectional_get_stats(bridge->jepa, &stats) == NIMCP_SUCCESS) {
            /* Use free energy as a proxy for prediction error */
            float fe = stats.avg_free_energy;
            total_pe += fe * bridge->config.jepa_forward_weight;
            total_pe += fe * bridge->config.jepa_backward_weight;
            total_weight += bridge->config.jepa_forward_weight + bridge->config.jepa_backward_weight;
        }
    }

    if (bridge->hopfield) {
        hopfield_stats_t stats;
        if (hopfield_memory_get_stats(bridge->hopfield, &stats) == NIMCP_SUCCESS) {
            float hopfield_error = 1.0f - stats.avg_similarity;
            total_pe += hopfield_error * bridge->config.hopfield_weight;
            total_weight += bridge->config.hopfield_weight;
        }
    }

    if (bridge->pred_hier) {
        float hier_fe = pred_hier_compute_free_energy(bridge->pred_hier);
        if (!isnan(hier_fe)) {
            total_pe += hier_fe * bridge->config.pred_hier_weight;
            total_weight += bridge->config.pred_hier_weight;
        }
    }

    if (bridge->replay) {
        replay_stats_t stats;
        if (temporal_replay_get_stats(bridge->replay, &stats) == NIMCP_SUCCESS) {
            float replay_error = stats.avg_priority;
            total_pe += replay_error * bridge->config.replay_weight;
            total_weight += bridge->config.replay_weight;
        }
    }

    return total_weight > 0.0f ? total_pe / total_weight : 0.0f;
}

static omni_immune_response_t compute_response_level(float pe, float fe,
                                                      const omni_immune_config_t* config) {
    if (pe < config->pe_threshold * 0.5f) {
        return OMNI_IMMUNE_NONE;
    } else if (pe < config->pe_threshold) {
        return OMNI_IMMUNE_LOCAL;
    } else if (fe < config->fe_threshold) {
        return OMNI_IMMUNE_REGIONAL;
    } else if (fe < config->fe_threshold * 2.0f) {
        return OMNI_IMMUNE_SYSTEMIC;
    } else {
        return OMNI_IMMUNE_MEMORY;
    }
}

static omni_immune_source_t find_dominant_source(omni_immune_bridge_t* bridge) {
    float max_error = 0.0f;
    omni_immune_source_t source = OMNI_SOURCE_AGGREGATED;

    if (bridge->jepa) {
        jepa_bidir_stats_t stats;
        if (jepa_bidirectional_get_stats(bridge->jepa, &stats) == NIMCP_SUCCESS) {
            /* Use free energy as proxy for error */
            float fe = stats.avg_free_energy;
            if (fe > max_error) {
                max_error = fe;
                source = OMNI_SOURCE_JEPA_FORWARD;
            }
        }
    }

    if (bridge->pred_hier) {
        float hier_fe = pred_hier_compute_free_energy(bridge->pred_hier);
        if (!isnan(hier_fe) && hier_fe > max_error) {
            max_error = hier_fe;
            source = OMNI_SOURCE_PRED_HIERARCHY;
        }
    }

    return source;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_immune_default_config(omni_immune_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_default_", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_immune_config_t));

    config->pe_threshold = OMNI_IMMUNE_DEFAULT_PE_THRESHOLD;
    config->fe_threshold = OMNI_IMMUNE_DEFAULT_FE_THRESHOLD;
    config->inflammation_scale = OMNI_IMMUNE_DEFAULT_INFLAMMATION_SCALE;

    config->jepa_forward_weight = 1.0f;
    config->jepa_backward_weight = 1.2f;
    config->hopfield_weight = 0.8f;
    config->pred_hier_weight = 1.5f;
    config->replay_weight = 0.6f;

    config->enable_microglia = true;
    config->enable_memory_immune = true;
    config->enable_precision_feedback = true;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_immune_bridge_t* omni_immune_bridge_create(const omni_immune_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__create", 0.0f);


    omni_immune_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_immune_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_immune_config_t));
    } else {
        omni_immune_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_immune_bridge_create: bridge->base is NULL");
        return NULL;
    }

    memset(&bridge->stats, 0, sizeof(omni_immune_stats_t));

    NIMCP_LOGGING_INFO("Created %s bridge", "omni_immune");
    return bridge;
}

void omni_immune_bridge_destroy(omni_immune_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_immune");

    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__destroy", 0.0f);


    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_immune_connect_jepa(omni_immune_bridge_t* bridge,
                              jepa_bidirectional_t* jepa) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_connect_hopfield(omni_immune_bridge_t* bridge,
                                  hopfield_memory_t* hopfield) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->hopfield = hopfield;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_connect_pred_hier(omni_immune_bridge_t* bridge,
                                   predictive_hierarchy_t* pred_hier) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_connect_replay(omni_immune_bridge_t* bridge,
                                temporal_replay_t* replay) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->replay = replay;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_connect_immune(omni_immune_bridge_t* bridge,
                                brain_immune_system_t* immune) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->immune = immune;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_immune_update(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute omni → immune effects */
    float pe_magnitude = compute_aggregated_pe(bridge);
    float free_energy = 0.0f;
    if (bridge->pred_hier) {
        free_energy = pred_hier_compute_free_energy(bridge->pred_hier);
        if (isnan(free_energy)) free_energy = 0.0f;
    }

    bridge->omni_effects.pe_magnitude = pe_magnitude;
    bridge->omni_effects.free_energy = free_energy;
    bridge->omni_effects.inflammation_signal =
        fminf(pe_magnitude * bridge->config.inflammation_scale, OMNI_IMMUNE_MAX_PE_INFLAMMATION);
    bridge->omni_effects.source = find_dominant_source(bridge);
    bridge->omni_effects.response = compute_response_level(pe_magnitude, free_energy,
                                                           &bridge->config);

    if (bridge->pred_hier) {
        bridge->omni_effects.affected_levels = pred_hier_num_levels(bridge->pred_hier);
    }

    /* Compute immune → omni effects */
    bridge->immune_effects.precision_modulation = 1.0f;
    bridge->immune_effects.pe_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->immune_effects.retrieval_threshold = 0.0f;
    bridge->immune_effects.replay_priority = 0.0f;
    bridge->immune_effects.suppress_backward = false;
    bridge->immune_effects.boost_consolidation = false;

    if (bridge->omni_effects.response >= OMNI_IMMUNE_REGIONAL) {
        bridge->immune_effects.precision_modulation = 1.5f;
        bridge->immune_effects.pe_sensitivity = 1.2f;
    }
    if (bridge->omni_effects.response >= OMNI_IMMUNE_SYSTEMIC) {
        bridge->immune_effects.precision_modulation = 2.0f;
        bridge->immune_effects.boost_consolidation = true;
    }

    /* Update statistics */
    bridge->stats.total_updates++;
    if (bridge->omni_effects.response != OMNI_IMMUNE_NONE) {
        bridge->stats.immune_activations++;
    }
    switch (bridge->omni_effects.response) {
        case OMNI_IMMUNE_LOCAL: bridge->stats.local_responses++; break;
        case OMNI_IMMUNE_REGIONAL: bridge->stats.regional_responses++; break;
        case OMNI_IMMUNE_SYSTEMIC: bridge->stats.systemic_responses++; break;
        case OMNI_IMMUNE_MEMORY: bridge->stats.memory_responses++; break;
        default: break;
    }

    float n = (float)bridge->stats.total_updates;
    bridge->stats.avg_pe_magnitude =
        (bridge->stats.avg_pe_magnitude * (n - 1) + pe_magnitude) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_inflammation =
        (bridge->stats.avg_inflammation * (n - 1) + bridge->omni_effects.inflammation_signal) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    if (bridge->omni_effects.inflammation_signal > bridge->stats.max_inflammation) {
        bridge->stats.max_inflammation = bridge->omni_effects.inflammation_signal;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_apply_to_immune(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_apply_to", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    /* In a full implementation, this would call brain_immune_system APIs */
    return NIMCP_SUCCESS;
}

int omni_immune_apply_to_omni(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_apply_to", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.precision_feedbacks++;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_immune_get_omni_effects(omni_immune_bridge_t* bridge,
                                  omni_to_immune_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_get_omni", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_immune_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_get_immune_effects(omni_immune_bridge_t* bridge,
                                    immune_to_omni_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_get_immu", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(effects, &bridge->immune_effects, sizeof(immune_to_omni_effects_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_get_stats(omni_immune_bridge_t* bridge,
                           omni_immune_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_get_stat", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "bridge or stats is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_immune_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_immune_reset_stats(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_reset_st", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_immune_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_immune_free_energy_report(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_immune_bridge_t* bridge = (omni_immune_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    omni_immune_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_immune_error_propagate(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_immune_bridge_t* bridge = (omni_immune_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    /* Process error propagation for immune response */
    omni_immune_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_immune_connect_bio_async(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_connect_", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_IMMUNE_BRIDGE,
        .module_name = "omni_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_OMNI_FREE_ENERGY_REPORT,
                                 handle_immune_free_energy_report);
    bio_router_register_handler(ctx, BIO_MSG_PRED_HIER_ERROR_PROPAGATE,
                                 handle_immune_error_propagate);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_immune_disconnect_bio_async(omni_immune_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_disconne", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_immune_is_bio_async_connected(const omni_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_immune_bridge_heartbeat("omni_immune__omni_immune_is_bio_a", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_immune_source_to_string(omni_immune_source_t source) {
    switch (source) {
        case OMNI_SOURCE_JEPA_FORWARD: return "JEPA_FORWARD";
        case OMNI_SOURCE_JEPA_BACKWARD: return "JEPA_BACKWARD";
        case OMNI_SOURCE_JEPA_LATERAL: return "JEPA_LATERAL";
        case OMNI_SOURCE_HOPFIELD: return "HOPFIELD";
        case OMNI_SOURCE_PRED_HIERARCHY: return "PRED_HIERARCHY";
        case OMNI_SOURCE_TEMPORAL_REPLAY: return "TEMPORAL_REPLAY";
        case OMNI_SOURCE_AGGREGATED: return "AGGREGATED";
        default: return "UNKNOWN";
    }
}

const char* omni_immune_response_to_string(omni_immune_response_t response) {
    switch (response) {
        case OMNI_IMMUNE_NONE: return "NONE";
        case OMNI_IMMUNE_LOCAL: return "LOCAL";
        case OMNI_IMMUNE_REGIONAL: return "REGIONAL";
        case OMNI_IMMUNE_SYSTEMIC: return "SYSTEMIC";
        case OMNI_IMMUNE_MEMORY: return "MEMORY";
        default: return "UNKNOWN";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void omni_immune_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_omni_immune_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int omni_immune_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_immune_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_immune_bridge_heartbeat_instance(NULL, "omni_immune_bridge_training_begin", 0.0f);
    return 0;
}

int omni_immune_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_immune_bridge_training_end: NULL argument");
        return -1;
    }
    omni_immune_bridge_heartbeat_instance(NULL, "omni_immune_bridge_training_end", 1.0f);
    return 0;
}

int omni_immune_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_immune_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_immune_bridge_heartbeat_instance(NULL, "omni_immune_bridge_training_step", progress);
    return 0;
}
