/**
 * @file nimcp_imagination_reasoning_fep_bridge.c
 * @brief Imagination-Reasoning Bridge - FEP Orchestrator Integration Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for imagination-reasoning
 * WHY:  Enable coordinated free energy minimization for imagination-reasoning integration
 * HOW:  Compute free energy from scenario quality, reasoning coherence, counterfactual validity
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_imagination_reasoning_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_imagination_reasoning_bridge.h"
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
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(imagination_reasoning_fep_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "IMAGINATION_REASONING_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct imag_reason_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    imag_reason_fep_config_t config;

    /* State */
    imag_reason_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    imagination_reasoning_bridge_t* imag_reason_bridge;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    imag_reason_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_scenario_quality;
    float prev_reasoning_coherence;
    float prev_counterfactual_validity;

    /* Running averages */
    float running_avg_fe;
    uint64_t running_count;

    /* Statistics */
    imag_reason_fep_stats_t stats;

    /* Callbacks */
    imag_reason_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    imag_reason_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    imag_reason_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from imagination-reasoning metrics
 *
 * Free energy model for imagination-reasoning:
 * FE = baseline + scenario_contrib + coherence_contrib + counterfactual_contrib
 *
 * Where:
 * - scenario_contrib = (1 - scenario_quality) * scenario_weight
 *   (lower quality scenarios = higher FE)
 * - coherence_contrib = (1 - reasoning_coherence) * coherence_weight
 *   (incoherent reasoning = higher FE)
 * - counterfactual_contrib = (1 - counterfactual_validity) * counterfactual_weight
 *   (invalid counterfactuals = higher FE)
 *
 * Coherent imagination-reasoning represents minimum free energy state where
 * imagined scenarios align well with reasoning inferences.
 */
static void compute_free_energy(imag_reason_fep_bridge_t* bridge) {
    imag_reason_fep_metrics_t* m = &bridge->metrics;
    const imag_reason_fep_config_t* cfg = &bridge->config;

    /* Clamp input metrics */
    m->scenario_quality = nimcp_clampf(m->scenario_quality, 0.0f, 1.0f);
    m->reasoning_coherence = nimcp_clampf(m->reasoning_coherence, 0.0f, 1.0f);
    m->counterfactual_validity = nimcp_clampf(m->counterfactual_validity, 0.0f, 1.0f);

    /* Scenario quality contribution:
     * Low quality = high prediction error about scene plausibility */
    m->scenario_contribution = (1.0f - m->scenario_quality) * cfg->scenario_quality_weight;

    /* Reasoning coherence contribution:
     * Incoherent reasoning = prediction error about logical consistency */
    m->coherence_contribution = (1.0f - m->reasoning_coherence) * cfg->reasoning_coherence_weight;

    /* Counterfactual validity contribution:
     * Invalid counterfactuals = prediction error about alternative outcomes */
    m->counterfactual_contribution = (1.0f - m->counterfactual_validity) *
                                      cfg->counterfactual_validity_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     (m->scenario_contribution +
                      m->coherence_contribution +
                      m->counterfactual_contribution) * cfg->free_energy_weight;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* Coherence check */
    m->is_coherent = (m->reasoning_coherence >= (1.0f - cfg->coherence_epsilon));

    /* Prediction error: weighted combination of all quality sources */
    float new_prediction_error = ((1.0f - m->scenario_quality) * 0.35f +
                                   (1.0f - m->reasoning_coherence) * 0.35f +
                                   (1.0f - m->counterfactual_validity) * 0.30f);

    /* Apply decay to smooth transitions */
    m->prediction_error = nimcp_clampf(
        new_prediction_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in integration state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float scenario_change = fabsf(m->scenario_quality - bridge->prev_scenario_quality);
    float coherence_change = fabsf(m->reasoning_coherence - bridge->prev_reasoning_coherence);
    float cf_change = fabsf(m->counterfactual_validity - bridge->prev_counterfactual_validity);

    m->surprise = nimcp_clampf(
        (fe_change * 0.25f + scenario_change * 0.25f +
         coherence_change * 0.25f + cf_change * 0.25f),
        0.0f, 1.0f
    );

    /* Entropy: based on scenario diversity and creative novelty */
    m->entropy = nimcp_clampf(
        m->creative_novelty * 0.6f + (1.0f - m->scenario_quality) * 0.4f,
        0.0f, 1.0f
    );
}

/**
 * @brief Check and trigger callbacks
 */
/**
 * @brief Deferred callback info -- filled under lock, invoked after unlock
 */
typedef struct {
    void (*high_fe_cb)(imag_reason_fep_bridge_t*, float, void*);
    float high_fe_value;
    void* high_fe_ud;

    void (*surprise_cb)(imag_reason_fep_bridge_t*, float, const char*, void*);
    float surprise_value;
    const char* surprise_source;
    void* surprise_ud;

    void (*metrics_cb)(imag_reason_fep_bridge_t*, const imag_reason_fep_metrics_t*, void*);
    imag_reason_fep_metrics_t metrics_snap;
    void* metrics_ud;

    imag_reason_fep_bridge_t* bridge;
} ir_fep_deferred_cbs_t;

/**
 * @brief Check thresholds and stage callbacks (MUST be called under lock).
 */
static void check_callbacks_locked(imag_reason_fep_bridge_t* bridge, ir_fep_deferred_cbs_t* out) {
    memset(out, 0, sizeof(*out));
    out->bridge = bridge;

    imag_reason_fep_metrics_t* m = &bridge->metrics;
    const imag_reason_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != IMAG_REASON_FEP_STATE_DEGRADED) {
            bridge->state = IMAG_REASON_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                out->high_fe_cb = bridge->high_fe_callback;
                out->high_fe_value = m->free_energy;
                out->high_fe_ud = bridge->high_fe_user_data;
            }
        }
    } else if (bridge->state == IMAG_REASON_FEP_STATE_DEGRADED) {
        bridge->state = IMAG_REASON_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->scenario_contribution > m->coherence_contribution &&
                m->scenario_contribution > m->counterfactual_contribution) {
                source = "scenario";
            } else if (m->coherence_contribution > m->counterfactual_contribution) {
                source = "coherence";
            } else {
                source = "counterfactual";
            }
            out->surprise_cb = bridge->surprise_callback;
            out->surprise_value = m->surprise;
            out->surprise_source = source;
            out->surprise_ud = bridge->surprise_user_data;
        }
    }

    /* Track creative insights */
    if (m->creative_novelty > 0.7f) {
        bridge->stats.creative_insights++;
    }

    /* Metrics callback */
    if (bridge->metrics_callback) {
        out->metrics_cb = bridge->metrics_callback;
        out->metrics_snap = *m;
        out->metrics_ud = bridge->metrics_user_data;
    }
}

/**
 * @brief Fire deferred callbacks outside the lock.
 */
static void ir_fire_deferred_callbacks(const ir_fep_deferred_cbs_t* d) {
    if (d->high_fe_cb) {
        d->high_fe_cb(d->bridge, d->high_fe_value, d->high_fe_ud);
    }
    if (d->surprise_cb) {
        d->surprise_cb(d->bridge, d->surprise_value, d->surprise_source,
                        d->surprise_ud);
    }
    if (d->metrics_cb) {
        d->metrics_cb(d->bridge, &d->metrics_snap, d->metrics_ud);
    }
}

/**
 * @brief Update statistics
 */
static void update_stats(imag_reason_fep_bridge_t* bridge, uint64_t update_time_us) {
    imag_reason_fep_stats_t* s = &bridge->stats;
    imag_reason_fep_metrics_t* m = &bridge->metrics;

    s->total_updates++;
    s->total_update_time_us += update_time_us;
    s->avg_update_time_us = (float)s->total_update_time_us / (float)s->total_updates;

    /* Track FE contribution */
    s->total_free_energy_contribution += m->free_energy;

    /* Peak tracking */
    if (m->free_energy > s->peak_free_energy) {
        s->peak_free_energy = m->free_energy;
    }

    /* Running averages */
    bridge->running_count++;
    bridge->running_avg_fe = (bridge->running_avg_fe * (bridge->running_count - 1) +
                              m->free_energy) / bridge->running_count;
    s->avg_free_energy = bridge->running_avg_fe;

    /* Update metrics timing */
    m->last_update_time_ms = get_time_ms();
    m->update_count++;
}

/**
 * @brief Store previous state for delta computation
 */
static void store_previous_state(imag_reason_fep_bridge_t* bridge) {
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_scenario_quality = bridge->metrics.scenario_quality;
    bridge->prev_reasoning_coherence = bridge->metrics.reasoning_coherence;
    bridge->prev_counterfactual_validity = bridge->metrics.counterfactual_validity;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

imag_reason_fep_config_t imag_reason_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_conf", 0.0f);


    imag_reason_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_logging = false;
    config.update_interval_ms = 50;  /* Cognitive timescale */

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.scenario_quality_weight = IMAG_REASON_FEP_SCENARIO_WEIGHT;
    config.reasoning_coherence_weight = IMAG_REASON_FEP_COHERENCE_WEIGHT;
    config.counterfactual_validity_weight = IMAG_REASON_FEP_COUNTERFACTUAL_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.coherence_epsilon = 0.05f;

    /* Normalization */
    config.baseline_free_energy = IMAG_REASON_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = IMAG_REASON_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = IMAG_REASON_FEP_ERROR_DECAY_RATE;

    return config;
}

imag_reason_fep_bridge_t* imag_reason_fep_bridge_create(
    const imag_reason_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    imag_reason_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(imag_reason_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = imag_reason_fep_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "imagination_reasoning_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "imag_reason_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = IMAG_REASON_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline/defaults */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.scenario_quality = 0.5f;        /* Start neutral */
    bridge->metrics.reasoning_coherence = 0.5f;     /* Start neutral */
    bridge->metrics.counterfactual_validity = 0.5f; /* Start neutral */
    bridge->metrics.creative_novelty = 0.0f;
    bridge->metrics.is_coherent = false;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_scenario_quality = 0.5f;
    bridge->prev_reasoning_coherence = 0.5f;
    bridge->prev_counterfactual_validity = 0.5f;

    bridge->state = IMAG_REASON_FEP_STATE_IDLE;

    return bridge;
}

void imag_reason_fep_bridge_destroy(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "imagination_reasoning_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    if (bridge->registered) {
        imag_reason_fep_bridge_unregister(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int imag_reason_fep_bridge_reset(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(imag_reason_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.scenario_quality = 0.5f;
    bridge->metrics.reasoning_coherence = 0.5f;
    bridge->metrics.counterfactual_validity = 0.5f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_scenario_quality = 0.5f;
    bridge->prev_reasoning_coherence = 0.5f;
    bridge->prev_counterfactual_validity = 0.5f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(imag_reason_fep_stats_t));

    bridge->state = IMAG_REASON_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int imag_reason_fep_bridge_register(
    imag_reason_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    imagination_reasoning_bridge_t* imag_reason_bridge,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_register: required parameter is NULL (bridge, orchestrator)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


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
    bridge->imag_reason_bridge = imag_reason_bridge;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "imagination_reasoning",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        imag_reason_fep_update_callback,
        imag_reason_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->imag_reason_bridge = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "imag_reason_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = IMAG_REASON_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int imag_reason_fep_bridge_unregister(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


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
    bridge->imag_reason_bridge = NULL;
    bridge->state = IMAG_REASON_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool imag_reason_fep_bridge_is_registered(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t imag_reason_fep_bridge_get_id(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int imag_reason_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_upda", 0.0f);


    imag_reason_fep_bridge_t* bridge = (imag_reason_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered */
    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_update_callback: bridge->registered is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    store_previous_state(bridge);

    /* If we have an imagination-reasoning bridge, query it for statistics */
    if (bridge->imag_reason_bridge) {
        imagination_reasoning_stats_t ir_stats;
        memset(&ir_stats, 0, sizeof(ir_stats));

        int err = imagination_reasoning_bridge_get_stats(
            bridge->imag_reason_bridge, &ir_stats);

        if (err == 0) {
            /* Update metrics from imagination-reasoning statistics */
            bridge->stats.scenario_evaluations++;

            /* Estimate scenario quality from plausibility */
            bridge->metrics.scenario_quality = nimcp_clampf(
                ir_stats.avg_plausibility,
                0.0f, 1.0f
            );

            /* Estimate reasoning coherence from confidence metrics */
            if (ir_stats.counterfactual_queries > 0) {
                bridge->metrics.reasoning_coherence = nimcp_clampf(
                    ir_stats.avg_counterfactual_confidence,
                    0.0f, 1.0f
                );
                bridge->stats.coherence_checks++;
            }

            /* Estimate counterfactual validity from success rates */
            if (ir_stats.scenarios_analyzed > 0) {
                float accept_rate = (float)ir_stats.scenarios_accepted /
                                   (float)ir_stats.scenarios_analyzed;
                bridge->metrics.counterfactual_validity = nimcp_clampf(
                    accept_rate,
                    0.0f, 1.0f
                );
                bridge->stats.counterfactual_analyses++;
            }

            /* Track creative novelty from inference metrics */
            bridge->metrics.creative_novelty = nimcp_clampf(
                ir_stats.avg_creative_novelty,
                0.0f, 1.0f
            );

            /* Track active scenarios */
            bridge->metrics.active_scenarios = ir_stats.scenarios_generated -
                                               ir_stats.scenarios_analyzed;
            bridge->metrics.completed_analyses = (uint32_t)ir_stats.scenarios_analyzed;
        }
    }

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Stage callbacks under lock, fire after unlock */
    ir_fep_deferred_cbs_t deferred;
    check_callbacks_locked(bridge, &deferred);

    nimcp_mutex_unlock(bridge->base.mutex);

    ir_fire_deferred_callbacks(&deferred);
    return 0;
}

void imag_reason_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via imag_reason_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_dest", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int imag_reason_fep_bridge_force_update(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state */
    store_previous_state(bridge);

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Stage callbacks under lock, fire after unlock */
    ir_fep_deferred_cbs_t deferred2;
    check_callbacks_locked(bridge, &deferred2);

    nimcp_mutex_unlock(bridge->base.mutex);

    ir_fire_deferred_callbacks(&deferred2);
    return 0;
}

int imag_reason_fep_bridge_update_scenario_quality(
    imag_reason_fep_bridge_t* bridge,
    float quality
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_update_scenario_quality: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.scenario_quality = nimcp_clampf(quality, 0.0f, 1.0f);
    bridge->stats.scenario_evaluations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_update_reasoning_coherence(
    imag_reason_fep_bridge_t* bridge,
    float coherence
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_update_reasoning_coherence: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.reasoning_coherence = nimcp_clampf(coherence, 0.0f, 1.0f);
    bridge->stats.coherence_checks++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_update_counterfactual_validity(
    imag_reason_fep_bridge_t* bridge,
    float validity
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_update_counterfactual_validity: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.counterfactual_validity = nimcp_clampf(validity, 0.0f, 1.0f);
    bridge->stats.counterfactual_analyses++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_update_creative_novelty(
    imag_reason_fep_bridge_t* bridge,
    float novelty
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_update_creative_novelty: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.creative_novelty = nimcp_clampf(novelty, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int imag_reason_fep_bridge_get_metrics(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_get_stats(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_reset_stats(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(imag_reason_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float imag_reason_fep_bridge_get_free_energy(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float imag_reason_fep_bridge_get_scenario_quality(
    const imag_reason_fep_bridge_t* bridge
) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    float sq = bridge->metrics.scenario_quality;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return sq;
}

float imag_reason_fep_bridge_get_reasoning_coherence(
    const imag_reason_fep_bridge_t* bridge
) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    float rc = bridge->metrics.reasoning_coherence;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return rc;
}

float imag_reason_fep_bridge_get_prediction_error(
    const imag_reason_fep_bridge_t* bridge
) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

imag_reason_fep_state_t imag_reason_fep_bridge_get_state(
    const imag_reason_fep_bridge_t* bridge
) {
    if (!bridge) return IMAG_REASON_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    imag_reason_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool imag_reason_fep_bridge_is_degraded(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == IMAG_REASON_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool imag_reason_fep_bridge_is_coherent(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    bool coherent = bridge->metrics.is_coherent;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return coherent;
}

const char* imag_reason_fep_state_name(imag_reason_fep_state_t state) {
    switch (state) {
        case IMAG_REASON_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case IMAG_REASON_FEP_STATE_IDLE:          return "idle";
        case IMAG_REASON_FEP_STATE_ACTIVE:        return "active";
        case IMAG_REASON_FEP_STATE_DEGRADED:      return "degraded";
        case IMAG_REASON_FEP_STATE_ERROR:         return "error";
        default:                                   return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int imag_reason_fep_bridge_set_high_fe_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_set_surprise_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_set_metrics_callback(
    imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int imag_reason_fep_bridge_set_config(
    imag_reason_fep_bridge_t* bridge,
    const imag_reason_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int imag_reason_fep_bridge_get_config(
    const imag_reason_fep_bridge_t* bridge,
    imag_reason_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "imag_reason_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    imagination_reasoning_fep_bridge_heartbeat("imagination__imag_reason_fep_brid", 0.0f);


    nimcp_mutex_lock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((imag_reason_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}


void imagination_reasoning_fep_bridge_set_instance_health_agent(imag_reason_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "imagination_reasoning_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int imagination_reasoning_fep_bridge_training_begin(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    imagination_reasoning_fep_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_fep_bridge_training_begin", 0.0f);
    return 0;
}

int imagination_reasoning_fep_bridge_training_end(imag_reason_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_fep_bridge_training_end: NULL argument");
        return -1;
    }
    imagination_reasoning_fep_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_fep_bridge_training_end", 1.0f);
    return 0;
}

int imagination_reasoning_fep_bridge_training_step(imag_reason_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "imagination_reasoning_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    imagination_reasoning_fep_bridge_heartbeat_instance(bridge->health_agent, "imagination_reasoning_fep_bridge_training_step", progress);
    return 0;
}
