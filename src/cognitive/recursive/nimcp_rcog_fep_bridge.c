/**
 * @file nimcp_rcog_fep_bridge.c
 * @brief Recursive Cognition - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for recursive cognition
 * WHY: Enable coordinated free energy minimization across recursive processing
 * HOW: Compute free energy from recursion depth, decomposition, and refinement
 *
 * @author NIMCP Development Team
 */

#include "cognitive/recursive/nimcp_rcog_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(rcog_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/** @brief Send heartbeat from rcog_fep_bridge module (instance-level) */
static inline void rcog_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_rcog_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_rcog_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_rcog_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

static nimcp_health_agent_t* g_rcog_fep_bridge_instance_health_agent = NULL;

void rcog_fep_bridge_set_instance_health_agent(
    rcog_fep_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    (void)bridge;
    g_rcog_fep_bridge_instance_health_agent = agent;
}

/* ============================================================================
 * Phase 8: Instance-level Training Functions
 * ============================================================================ */

int rcog_fep_bridge_training_begin(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    rcog_fep_bridge_heartbeat_instance(
        g_rcog_fep_bridge_instance_health_agent, "rcog_fep_training_begin", 0.0f);
    return 0;
}

int rcog_fep_bridge_training_step(rcog_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_fep_bridge_training_step: NULL argument");
        return -1;
    }
    float clamped = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
    rcog_fep_bridge_heartbeat_instance(
        g_rcog_fep_bridge_instance_health_agent, "rcog_fep_training_step", clamped);
    return 0;
}

int rcog_fep_bridge_training_end(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "rcog_fep_bridge_training_end: NULL argument");
        return -1;
    }
    rcog_fep_bridge_heartbeat_instance(
        g_rcog_fep_bridge_instance_health_agent, "rcog_fep_training_end", 1.0f);
    return 0;
}

#define LOG_MODULE "RCOG_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct rcog_fep_bridge {
    bridge_base_t base;

    /* Configuration */
    rcog_fep_config_t config;

    /* State */
    rcog_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    rcog_engine_t* engine;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    rcog_fep_metrics_t metrics;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_depth;
    float prev_success_rate;

    /* Running averages */
    float running_avg_fe;
    float running_avg_pe;
    uint64_t running_count;

    /* Statistics */
    rcog_fep_stats_t stats;

    /* Callbacks */
    rcog_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    rcog_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    rcog_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from recursion metrics
 *
 * Free energy model:
 * FE = baseline + depth_contrib + decomp_contrib + refine_contrib
 *
 * Where:
 * - depth_contrib = normalized_depth * depth_weight (deeper = more uncertainty)
 * - decomp_contrib = (1 - success_rate) * decomp_weight (failures increase FE)
 * - refine_contrib = (1 - progress) * refine_weight (incomplete increases FE)
 */
static void compute_free_energy(
    rcog_fep_bridge_t* bridge,
    const rcog_engine_stats_t* engine_stats
) {
    rcog_fep_metrics_t* m = &bridge->metrics;
    const rcog_fep_config_t* cfg = &bridge->config;

    /* Compute normalized depth */
    float max_depth = (float)cfg->max_depth_norm;
    if (max_depth < 1.0f) max_depth = 1.0f;

    m->normalized_depth = nimcp_clampf(
        (float)engine_stats->max_depth_reached / max_depth,
        0.0f, 1.0f
    );

    /* Depth contribution: deeper recursion = more uncertainty */
    m->depth_contribution = m->normalized_depth * cfg->depth_weight;

    /* Decomposition success rate */
    if (engine_stats->subtasks_created > 0) {
        m->decomp_success_rate = nimcp_clampf(
            (float)engine_stats->subtasks_completed /
            (float)engine_stats->subtasks_created,
            0.0f, 1.0f
        );
        m->decomp_efficiency = (engine_stats->subtasks_failed > 0) ?
            1.0f - ((float)engine_stats->subtasks_failed /
                   (float)engine_stats->subtasks_created) : 1.0f;
    } else {
        m->decomp_success_rate = 1.0f;
        m->decomp_efficiency = 1.0f;
    }

    /* Decomposition contribution: failures increase free energy */
    m->decomp_contribution = (1.0f - m->decomp_success_rate) * cfg->decomp_weight;

    /* Refinement progress from average confidence and refinement steps */
    m->refinement_progress = nimcp_clampf(engine_stats->avg_confidence, 0.0f, 1.0f);
    m->refinement_steps = (uint32_t)engine_stats->avg_refinement_steps;

    /* Confidence delta for convergence detection */
    float delta = m->refinement_progress - bridge->prev_depth;
    m->confidence_delta = delta;
    m->answer_converging = (delta > cfg->convergence_threshold) ||
                          (m->refinement_progress > 0.9f);

    /* Refinement contribution: incomplete refinement increases free energy */
    m->refine_contribution = (1.0f - m->refinement_progress) * cfg->refine_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     m->depth_contribution +
                     m->decomp_contribution +
                     m->refine_contribution;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* Prediction error: difference from expected state */
    float expected_success = 0.9f;  /* Expect high success rate */
    float success_error = fabsf(m->decomp_success_rate - expected_success);

    float expected_progress = 0.8f;  /* Expect good progress */
    float progress_error = fabsf(m->refinement_progress - expected_progress);

    m->prediction_error = nimcp_clampf(
        (success_error + progress_error) / 2.0f * bridge->config.error_decay_rate +
        bridge->prev_prediction_error * (1.0f - bridge->config.error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float depth_change = fabsf(m->normalized_depth - bridge->prev_depth);
    float success_change = fabsf(m->decomp_success_rate - bridge->prev_success_rate);

    m->surprise = nimcp_clampf(
        (fe_change * 0.4f + depth_change * 0.3f + success_change * 0.3f),
        0.0f, 1.0f
    );

    /* Entropy: state uncertainty based on active processing */
    float active_ratio = (engine_stats->active_goals > 0) ? 0.5f : 0.0f;
    float pending_ratio = (engine_stats->pending_goals > 0) ? 0.3f : 0.0f;
    m->entropy = nimcp_clampf(
        active_ratio + pending_ratio + (1.0f - m->refinement_progress) * 0.2f,
        0.0f, 1.0f
    );

    /* Update subtask counts */
    m->current_subtasks = engine_stats->active_goals;
    m->completed_subtasks = (uint32_t)(
        engine_stats->subtasks_completed - bridge->prev_success_rate * 1000
    );  /* Delta approximation */
}

/**
 * @brief Check and trigger callbacks
 */
static void check_callbacks(rcog_fep_bridge_t* bridge) {
    rcog_fep_metrics_t* m = &bridge->metrics;
    const rcog_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != RCOG_FEP_STATE_DEGRADED) {
            bridge->state = RCOG_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                bridge->high_fe_callback(bridge, m->free_energy,
                                         bridge->high_fe_user_data);
            }
        }
    } else if (bridge->state == RCOG_FEP_STATE_DEGRADED) {
        bridge->state = RCOG_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (cfg->enable_surprise_callbacks &&
        m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->depth_contribution > m->decomp_contribution &&
                m->depth_contribution > m->refine_contribution) {
                source = "depth";
            } else if (m->decomp_contribution > m->refine_contribution) {
                source = "decomposition";
            } else {
                source = "refinement";
            }
            bridge->surprise_callback(bridge, m->surprise, source,
                                      bridge->surprise_user_data);
        }
    }

    /* Metrics callback */
    if (bridge->metrics_callback) {
        bridge->metrics_callback(bridge, m, bridge->metrics_user_data);
    }
}

/**
 * @brief Update statistics
 */
static void update_stats(rcog_fep_bridge_t* bridge, uint64_t update_time_us) {
    rcog_fep_stats_t* s = &bridge->stats;
    rcog_fep_metrics_t* m = &bridge->metrics;

    s->total_updates++;
    s->total_update_time_us += update_time_us;

    /* Peak tracking */
    if (m->free_energy > s->peak_free_energy) {
        s->peak_free_energy = m->free_energy;
    }

    /* Running averages */
    bridge->running_count++;
    bridge->running_avg_fe = (bridge->running_avg_fe * (bridge->running_count - 1) +
                              m->free_energy) / bridge->running_count;
    bridge->running_avg_pe = (bridge->running_avg_pe * (bridge->running_count - 1) +
                              m->prediction_error) / bridge->running_count;

    s->avg_free_energy = bridge->running_avg_fe;
    s->avg_prediction_error = bridge->running_avg_pe;

    /* Update metrics timing */
    m->last_update_time_ms = get_time_ms();
    m->update_count++;
    m->avg_update_time_us = (float)s->total_update_time_us / (float)s->total_updates;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

rcog_fep_config_t rcog_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_rcog_fep_config_defa", 0.0f);


    rcog_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Weighting parameters */
    config.depth_weight = RCOG_FEP_DEPTH_WEIGHT;
    config.decomp_weight = RCOG_FEP_DECOMP_WEIGHT;
    config.refine_weight = RCOG_FEP_REFINE_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.convergence_threshold = 0.01f;

    /* Normalization */
    config.max_depth_norm = RCOG_FEP_MAX_DEPTH_NORM;
    config.baseline_free_energy = RCOG_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = RCOG_FEP_MAX_FREE_ENERGY;

    /* Behavior */
    config.enable_adaptive_weights = true;
    config.enable_degraded_mode = true;
    config.enable_surprise_callbacks = true;
    config.error_decay_rate = RCOG_FEP_ERROR_DECAY_RATE;

    return config;
}

rcog_fep_bridge_t* rcog_fep_bridge_create(const rcog_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_create", 0.0f);


    rcog_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(rcog_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = rcog_fep_config_default();
    }

    /* Initialize bridge base */
    if (bridge_base_init(&bridge->base, 0, "rcog_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "rcog_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = RCOG_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.decomp_success_rate = 1.0f;
    bridge->metrics.decomp_efficiency = 1.0f;
    bridge->metrics.refinement_progress = 0.0f;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_depth = 0.0f;
    bridge->prev_success_rate = 1.0f;

    bridge->state = RCOG_FEP_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "rcog_fep");
    return bridge;
}

void rcog_fep_bridge_destroy(rcog_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "rcog_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_destroy", 0.0f);


    if (bridge->registered) {
        rcog_fep_bridge_unregister(bridge);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int rcog_fep_bridge_reset(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(rcog_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.decomp_success_rate = 1.0f;
    bridge->metrics.decomp_efficiency = 1.0f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_depth = 0.0f;
    bridge->prev_success_rate = 1.0f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_avg_pe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(rcog_fep_stats_t));

    bridge->state = RCOG_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int rcog_fep_bridge_register(
    fep_orchestrator_t* orchestrator,
    rcog_engine_t* engine,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_register: required parameter is NULL (orchestrator, engine)");
        return -1;
    }

    /* Create bridge with default config */
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_register", 0.0f);


    rcog_fep_bridge_t* bridge = rcog_fep_bridge_create(NULL);
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_register: bridge is NULL");
        return -1;
    }

    int ret = rcog_fep_bridge_register_ex(bridge, orchestrator, engine, bridge_id_out);
    if (ret != 0) {
        rcog_fep_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_fep_bridge_register: validation failed");
        return -1;
    }

    return 0;
}

int rcog_fep_bridge_register_ex(
    rcog_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    rcog_engine_t* engine,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator || !engine) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_register_ex: required parameter is NULL (bridge, orchestrator, engine)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_register_ex", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        if (bridge_id_out) {
            *bridge_id_out = bridge->bridge_id;
        }
        return 0;  /* Already registered, success */
    }

    /* Store references */
    bridge->orchestrator = orchestrator;
    bridge->engine = engine;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "recursive_cognition",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        rcog_fep_update_callback,
        rcog_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->engine = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "rcog_fep_bridge_register_ex: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = RCOG_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int rcog_fep_bridge_unregister(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_unregister", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not registered, nothing to do */
    }

    /* Unregister from orchestrator */
    if (bridge->orchestrator) {
        fep_orchestrator_unregister_bridge(bridge->orchestrator, bridge->bridge_id);
    }

    bridge->registered = false;
    bridge->bridge_id = 0;
    bridge->orchestrator = NULL;
    bridge->engine = NULL;
    bridge->state = RCOG_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool rcog_fep_bridge_is_registered(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_is_registered", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t rcog_fep_bridge_get_id(rcog_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_id", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int rcog_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_rcog_fep_update_call", 0.0f);


    rcog_fep_bridge_t* bridge = (rcog_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered and have an engine */
    if (!bridge->registered || !bridge->engine) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_update_callback: required parameter is NULL (bridge->registered, bridge->engine)");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Get engine statistics */
    rcog_engine_stats_t engine_stats;
    memset(&engine_stats, 0, sizeof(engine_stats));

    int ret = rcog_engine_get_stats(bridge->engine, &engine_stats);
    if (ret != 0) {
        /* Engine stats not available, use minimal update */
        bridge->metrics.last_update_time_ms = get_time_ms();
        bridge->metrics.update_count++;
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Non-critical failure */
    }

    /* Store previous state for delta computation */
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_depth = bridge->metrics.normalized_depth;
    bridge->prev_success_rate = bridge->metrics.decomp_success_rate;

    /* Compute free energy from engine stats */
    compute_free_energy(bridge, &engine_stats);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Check and trigger callbacks */
    check_callbacks(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

void rcog_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via rcog_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_rcog_fep_destroy_cal", 0.0f);


    (void)handle;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int rcog_fep_bridge_get_metrics(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_metrics", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int rcog_fep_bridge_get_stats(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_stats", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int rcog_fep_bridge_reset_stats(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_reset_stats", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(rcog_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_avg_pe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float rcog_fep_bridge_get_free_energy(rcog_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_free_energy", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float rcog_fep_bridge_get_prediction_error(rcog_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_prediction_error", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

rcog_fep_state_t rcog_fep_bridge_get_state(rcog_fep_bridge_t* bridge) {
    if (!bridge) return RCOG_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_state", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    rcog_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

bool rcog_fep_bridge_is_degraded(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_is_degraded", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == RCOG_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool rcog_fep_bridge_is_converging(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_is_converging", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    bool converging = bridge->metrics.answer_converging;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return converging;
}

float rcog_fep_bridge_get_normalized_depth(rcog_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_normalized_depth", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    float depth = bridge->metrics.normalized_depth;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return depth;
}

float rcog_fep_bridge_get_decomp_success_rate(rcog_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_decomp_success_r", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    float rate = bridge->metrics.decomp_success_rate;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return rate;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int rcog_fep_bridge_set_high_fe_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_set_high_fe_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_fep_bridge_set_surprise_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_set_surprise_callbac", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_fep_bridge_set_metrics_callback(
    rcog_fep_bridge_t* bridge,
    rcog_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_set_metrics_callback", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int rcog_fep_bridge_set_config(
    rcog_fep_bridge_t* bridge,
    const rcog_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_set_config", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int rcog_fep_bridge_get_config(
    const rcog_fep_bridge_t* bridge,
    rcog_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_get_config", 0.0f);


    nimcp_mutex_lock(((rcog_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((rcog_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* rcog_fep_state_name(rcog_fep_state_t state) {
    switch (state) {
        case RCOG_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case RCOG_FEP_STATE_IDLE:          return "idle";
        case RCOG_FEP_STATE_ACTIVE:        return "active";
        case RCOG_FEP_STATE_DEGRADED:      return "degraded";
        case RCOG_FEP_STATE_ERROR:         return "error";
        default:                            return "unknown";
    }
}

int rcog_fep_bridge_force_update(rcog_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "rcog_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* If registered with orchestrator and has engine, do full update */
    /* Phase 8: Heartbeat at operation start */
    rcog_fep_bridge_heartbeat("rcog_fep_bri_force_update", 0.0f);


    if (bridge->registered && bridge->engine) {
        return rcog_fep_update_callback(bridge);
    }

    /* For unit testing: perform minimal update with callback invocation */
    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state */
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;

    /* Simulate minimal metrics update */
    bridge->metrics.last_update_time_ms = get_time_ms();
    bridge->metrics.update_count++;

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update stats */
    bridge->stats.total_updates++;
    bridge->stats.total_update_time_us += update_time_us;

    /* Invoke callbacks even without full engine data */
    check_callbacks(bridge);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}
