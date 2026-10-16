/**
 * @file nimcp_omni_rcog_bridge.c
 * @brief Implementation of Omnidirectional Inference to Recursive Cognition Bridge
 */

#include "cognitive/recursive/nimcp_omni_rcog_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(omni_rcog_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)

void omni_rcog_bridge_set_instance_health_agent(
    omni_rcog_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "omni_rcog_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int omni_rcog_bridge_training_begin(omni_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_rcog_bridge_training_begin: NULL argument");
        return -1;
    }
    omni_rcog_bridge_heartbeat_instance(g_omni_rcog_bridge_health_agent, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int omni_rcog_bridge_training_step(omni_rcog_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_rcog_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    omni_rcog_bridge_heartbeat_instance(g_omni_rcog_bridge_health_agent, "training_step", progress);
    (void)bridge;
    return 0;
}

int omni_rcog_bridge_training_end(omni_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "omni_rcog_bridge_training_end: NULL argument");
        return -1;
    }
    omni_rcog_bridge_heartbeat_instance(g_omni_rcog_bridge_health_agent, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

#define LOG_MODULE "OMNI_RCOG_BRIDGE"


/* ============================================================================
 * Static Helpers
 * ============================================================================ */

static omni_decomp_strategy_t compute_strategy(float pe,
                                               const omni_rcog_config_t* config) {
    if (pe < config->direct_exec_threshold) {
        return OMNI_DECOMP_DIRECT;
    } else if (pe < config->shallow_decomp_threshold) {
        return OMNI_DECOMP_SHALLOW;
    } else if (pe < config->deep_decomp_threshold) {
        return config->enable_backward_decomp ?
               OMNI_DECOMP_BACKWARD : OMNI_DECOMP_DEEP;
    } else if (pe < config->abort_threshold) {
        return config->enable_bidirectional ?
               OMNI_DECOMP_BIDIRECTIONAL : OMNI_DECOMP_DEEP;
    } else {
        return OMNI_DECOMP_ABORT;
    }
}

static uint32_t compute_depth(float pe, const omni_rcog_config_t* config) {
    if (pe < config->direct_exec_threshold) {
        return 0;
    }

    float ratio = (pe - config->direct_exec_threshold) /
                  (config->abort_threshold - config->direct_exec_threshold);
    uint32_t adjustment = (uint32_t)(ratio * config->max_depth_adjustment);
    return config->base_recursion_depth + adjustment;
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int omni_rcog_default_config(omni_rcog_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_default_co", 0.0f);


    NIMCP_CHECK_THROW(config, NIMCP_ERROR_INVALID_PARAM, "config is NULL");

    memset(config, 0, sizeof(omni_rcog_config_t));

    config->direct_exec_threshold = OMNI_RCOG_DIRECT_EXEC_THRESHOLD;
    config->shallow_decomp_threshold = 2.0f;
    config->deep_decomp_threshold = OMNI_RCOG_DEEP_DECOMP_THRESHOLD;
    config->abort_threshold = 10.0f;

    config->base_recursion_depth = 2;
    config->max_depth_adjustment = OMNI_RCOG_MAX_DEPTH_ADJUSTMENT;
    config->enable_backward_decomp = true;
    config->enable_bidirectional = true;

    config->goal_mode = OMNI_GOAL_HIERARCHICAL;
    config->use_hopfield_goals = true;

    config->enable_bio_async = true;
    config->enable_logging = false;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

omni_rcog_bridge_t* omni_rcog_bridge_create(const omni_rcog_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_create", 0.0f);


    omni_rcog_bridge_t* bridge = nimcp_calloc(1, sizeof(omni_rcog_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    if (config) {
        memcpy(&bridge->config, config, sizeof(omni_rcog_config_t));
    } else {
        omni_rcog_default_config(&bridge->config);
    }

    if (bridge_base_init(&bridge->base, 0, "omni_rcog") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_rcog_bridge_create: bridge->base is NULL");
        return NULL;
    }

    memset(&bridge->stats, 0, sizeof(omni_rcog_stats_t));

    NIMCP_LOGGING_INFO("Created %s bridge", "omni_rcog");
    return bridge;
}

void omni_rcog_bridge_destroy(omni_rcog_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "omni_rcog");

    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_destroy", 0.0f);


    if (bridge->omni_effects.subgoal_predictions) {
        nimcp_free(bridge->omni_effects.subgoal_predictions);
    }
    if (bridge->rcog_effects.goal_embedding) {
        nimcp_free(bridge->rcog_effects.goal_embedding);
    }

    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int omni_rcog_connect_jepa(omni_rcog_bridge_t* bridge,
                            jepa_bidirectional_t* jepa) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_connect_je", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->jepa = jepa;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_connect_pred_hier(omni_rcog_bridge_t* bridge,
                                 predictive_hierarchy_t* pred_hier) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_connect_pr", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->pred_hier = pred_hier;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_connect_orchestrator(omni_rcog_bridge_t* bridge,
                                    rcog_orchestrator_t* orchestrator) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_connect_or", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->orchestrator = orchestrator;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_connect_engine(omni_rcog_bridge_t* bridge,
                              rcog_engine_t* engine) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_connect_en", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->engine = engine;
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int omni_rcog_update(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_update", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get prediction error from connected systems */
    float pe = 0.0f;
    if (bridge->pred_hier) {
        float fe = pred_hier_compute_free_energy(bridge->pred_hier);
        pe = isnan(fe) ? 0.0f : fe;
    }

    /* Compute omni → rcog effects */
    bridge->omni_effects.strategy = compute_strategy(pe, &bridge->config);
    bridge->omni_effects.suggested_depth = compute_depth(pe, &bridge->config);
    bridge->omni_effects.execution_confidence = 1.0f / (1.0f + pe);
    bridge->omni_effects.needs_backward_inference =
        (bridge->omni_effects.strategy == OMNI_DECOMP_BACKWARD ||
         bridge->omni_effects.strategy == OMNI_DECOMP_BIDIRECTIONAL);

    /* Update rcog → omni effects (placeholder) */
    bridge->rcog_effects.current_depth = bridge->config.base_recursion_depth;
    bridge->rcog_effects.task_urgency = 0.5f;
    bridge->rcog_effects.in_backtracking = false;
    bridge->rcog_effects.working_memory_load = 0.3f;

    /* Update statistics */
    bridge->stats.total_updates++;
    switch (bridge->omni_effects.strategy) {
        case OMNI_DECOMP_DIRECT:
            bridge->stats.direct_executions++;
            break;
        case OMNI_DECOMP_SHALLOW:
            bridge->stats.shallow_decompositions++;
            break;
        case OMNI_DECOMP_DEEP:
            bridge->stats.deep_decompositions++;
            break;
        case OMNI_DECOMP_BACKWARD:
        case OMNI_DECOMP_BIDIRECTIONAL:
            bridge->stats.backward_inferences++;
            break;
        case OMNI_DECOMP_ABORT:
            bridge->stats.aborts++;
            break;
    }

    float n = (float)bridge->stats.total_updates;
    bridge->stats.avg_pe_at_decomp =
        (bridge->stats.avg_pe_at_decomp * (n - 1) + pe) / (fabsf(n) > 1e-7f ? n : 1e-7f);
    bridge->stats.avg_depth =
        (bridge->stats.avg_depth * (n - 1) + bridge->omni_effects.suggested_depth) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_apply_to_rcog(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_apply_to_r", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

int omni_rcog_apply_to_omni(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_apply_to_o", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Decomposition API
 * ============================================================================ */

int omni_rcog_get_strategy(omni_rcog_bridge_t* bridge,
                            omni_decomp_strategy_t* strategy) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_get_strate", 0.0f);


    NIMCP_CHECK_THROW(bridge && strategy, NIMCP_ERROR_INVALID_PARAM, "bridge or strategy is NULL");
    nimcp_mutex_lock(((omni_rcog_bridge_t*)bridge)->mutex);
    *strategy = bridge->omni_effects.strategy;
    nimcp_mutex_unlock(((omni_rcog_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

uint32_t omni_rcog_get_suggested_depth(const omni_rcog_bridge_t* bridge) {
    if (!bridge) return 0;
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_get_sugges", 0.0f);


    return bridge->omni_effects.suggested_depth;
}

bool omni_rcog_needs_backward(const omni_rcog_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "omni_rcog_needs_backward: bridge is NULL");
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_needs_back", 0.0f);


    return bridge->omni_effects.needs_backward_inference;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int omni_rcog_get_omni_effects(omni_rcog_bridge_t* bridge,
                                omni_to_rcog_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_get_omni_e", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(((omni_rcog_bridge_t*)bridge)->mutex);
    memcpy(effects, &bridge->omni_effects, sizeof(omni_to_rcog_effects_t));
    nimcp_mutex_unlock(((omni_rcog_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_get_rcog_effects(omni_rcog_bridge_t* bridge,
                                rcog_to_omni_effects_t* effects) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_get_rcog_e", 0.0f);


    NIMCP_CHECK_THROW(bridge && effects, NIMCP_ERROR_INVALID_PARAM, "bridge or effects is NULL");
    nimcp_mutex_lock(((omni_rcog_bridge_t*)bridge)->mutex);
    memcpy(effects, &bridge->rcog_effects, sizeof(rcog_to_omni_effects_t));
    nimcp_mutex_unlock(((omni_rcog_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_get_stats(omni_rcog_bridge_t* bridge,
                         omni_rcog_stats_t* stats) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_get_stats", 0.0f);


    NIMCP_CHECK_THROW(bridge && stats, NIMCP_ERROR_INVALID_PARAM, "bridge or stats is NULL");
    nimcp_mutex_lock(((omni_rcog_bridge_t*)bridge)->mutex);
    memcpy(stats, &bridge->stats, sizeof(omni_rcog_stats_t));
    nimcp_mutex_unlock(((omni_rcog_bridge_t*)bridge)->mutex);
    return NIMCP_SUCCESS;
}

int omni_rcog_reset_stats(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_reset_stat", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(omni_rcog_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async Message Handlers
 * ============================================================================ */

static nimcp_error_t handle_rcog_predict_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_rcog_bridge_t* bridge = (omni_rcog_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    omni_rcog_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

static nimcp_error_t handle_rcog_direction_switch(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    omni_rcog_bridge_t* bridge = (omni_rcog_bridge_t*)user_data;
    NIMCP_CHECK_THROW(bridge && msg, NIMCP_ERROR_INVALID_PARAM, "bridge or msg is NULL");

    /* Update decomposition strategy based on direction switch */
    omni_rcog_update(bridge);

    (void)response_promise;
    (void)msg_size;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bio-Async API
 * ============================================================================ */

int omni_rcog_connect_bio_async(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_connect_bi", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (bridge->bio_async_connected) return NIMCP_SUCCESS;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_OMNI_RCOG_BRIDGE,
        .module_name = "omni_rcog_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bio_module_context_t ctx = bio_router_register_module(&info);
    if (!ctx) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    bridge->bio_context = ctx;

    bio_router_register_handler(ctx, BIO_MSG_OMNI_PREDICT_REQUEST,
                                 handle_rcog_predict_request);
    bio_router_register_handler(ctx, BIO_MSG_OMNI_DIRECTION_SWITCH,
                                 handle_rcog_direction_switch);

    bridge->bio_async_connected = true;
    return NIMCP_SUCCESS;
}

int omni_rcog_disconnect_bio_async(omni_rcog_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_disconnect", 0.0f);


    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_INVALID_PARAM, "bridge is NULL");
    if (!bridge->bio_async_connected) return NIMCP_SUCCESS;

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }

    bridge->bio_async_connected = false;
    return NIMCP_SUCCESS;
}

bool omni_rcog_is_bio_async_connected(const omni_rcog_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    omni_rcog_bridge_heartbeat("omni_rcog_br_omni_rcog_is_bio_asy", 0.0f);


    return bridge->bio_async_connected;
}

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

const char* omni_rcog_strategy_to_string(omni_decomp_strategy_t strategy) {
    switch (strategy) {
        case OMNI_DECOMP_DIRECT: return "DIRECT";
        case OMNI_DECOMP_SHALLOW: return "SHALLOW";
        case OMNI_DECOMP_DEEP: return "DEEP";
        case OMNI_DECOMP_BACKWARD: return "BACKWARD";
        case OMNI_DECOMP_BIDIRECTIONAL: return "BIDIRECTIONAL";
        case OMNI_DECOMP_ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

const char* omni_rcog_goal_mode_to_string(omni_goal_mode_t mode) {
    switch (mode) {
        case OMNI_GOAL_FORWARD: return "FORWARD";
        case OMNI_GOAL_BACKWARD: return "BACKWARD";
        case OMNI_GOAL_HIERARCHICAL: return "HIERARCHICAL";
        case OMNI_GOAL_ASSOCIATIVE: return "ASSOCIATIVE";
        default: return "UNKNOWN";
    }
}
