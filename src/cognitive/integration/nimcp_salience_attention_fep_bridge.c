/**
 * @file nimcp_salience_attention_fep_bridge.c
 * @brief Salience-Attention - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for salience-attention
 * WHY:  Enable coordinated free energy minimization for attention allocation
 * HOW:  Compute free energy from salience prediction, attention allocation,
 *       and priority estimation errors
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_salience_attention_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_salience_attention_bridge.h"
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
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(salience_attention_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_salience_attention_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_salience_attention_fep_bridge_mesh_registry = NULL;

nimcp_error_t salience_attention_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_salience_attention_fep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "salience_attention_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "salience_attention_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_salience_attention_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_salience_attention_fep_bridge_mesh_registry = registry;
    return err;
}

void salience_attention_fep_bridge_mesh_unregister(void) {
    if (g_salience_attention_fep_bridge_mesh_registry && g_salience_attention_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_salience_attention_fep_bridge_mesh_registry, g_salience_attention_fep_bridge_mesh_id);
        g_salience_attention_fep_bridge_mesh_id = 0;
        g_salience_attention_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from salience_attention_fep_bridge module (instance-level) */
static inline void salience_attention_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_salience_attention_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_salience_attention_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_salience_attention_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SALIENCE_ATTENTION_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct sa_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    sa_fep_config_t config;

    /* State */
    sa_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    salience_attention_bridge_t* sa_bridge;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    sa_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_salience_error;
    float prev_attention_error;
    float prev_priority_error;

    /* Running averages */
    float running_avg_fe;
    uint64_t running_count;

    /* Statistics */
    sa_fep_stats_t stats;

    /* Callbacks */
    sa_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    sa_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    sa_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from salience-attention metrics
 *
 * Free energy model for attention allocation:
 * FE = baseline + salience_contrib + attention_contrib + priority_contrib
 *
 * Where:
 * - salience_contrib = salience_prediction_error * salience_weight
 *   (higher mismatch in salience prediction = higher FE)
 * - attention_contrib = attention_allocation_error * attention_weight
 *   (worse attention allocation = higher FE)
 * - priority_contrib = priority_estimation_error * priority_weight
 *   (worse priority estimation = higher FE)
 *
 * Efficient attention represents minimum free energy state where
 * salience is accurately predicted and attention is optimally allocated.
 */
static void compute_free_energy(sa_fep_bridge_t* bridge) {
    sa_fep_metrics_t* m = &bridge->metrics;
    const sa_fep_config_t* cfg = &bridge->config;

    /* Clamp input metrics */
    m->salience_prediction_error = nimcp_clampf(m->salience_prediction_error, 0.0f, 1.0f);
    m->attention_allocation_error = nimcp_clampf(m->attention_allocation_error, 0.0f, 1.0f);
    m->priority_estimation_error = nimcp_clampf(m->priority_estimation_error, 0.0f, 1.0f);
    m->attention_efficiency = nimcp_clampf(m->attention_efficiency, 0.0f, 1.0f);

    /* Salience prediction contribution:
     * High error in predicting salient stimuli = prediction error */
    m->salience_contribution = m->salience_prediction_error * cfg->salience_prediction_weight;

    /* Attention allocation contribution:
     * Poor attention allocation = prediction error */
    m->attention_contribution = m->attention_allocation_error * cfg->attention_allocation_weight;

    /* Priority estimation contribution:
     * Error in estimating priorities = prediction error
     * Good priority estimation means accurate predictions of importance */
    m->priority_contribution = m->priority_estimation_error * cfg->priority_estimation_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     (m->salience_contribution +
                      m->attention_contribution +
                      m->priority_contribution) * cfg->free_energy_weight;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* Attention captured check - attention is captured when salience is high
     * and attention successfully shifted */
    m->attention_captured = (m->attention_efficiency > cfg->attention_efficiency_threshold &&
                             m->salience_prediction_error < 0.3f);

    /* Prediction error: weighted combination of all uncertainty sources */
    float new_prediction_error = (m->salience_prediction_error * 0.4f +
                                   m->attention_allocation_error * 0.4f +
                                   m->priority_estimation_error * 0.2f);

    /* Apply decay to smooth transitions */
    m->prediction_error = nimcp_clampf(
        new_prediction_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in attention state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float salience_change = fabsf(m->salience_prediction_error - bridge->prev_salience_error);
    float attention_change = fabsf(m->attention_allocation_error - bridge->prev_attention_error);
    float priority_change = fabsf(m->priority_estimation_error - bridge->prev_priority_error);

    m->surprise = nimcp_clampf(
        (fe_change * 0.3f + salience_change * 0.3f +
         attention_change * 0.2f + priority_change * 0.2f),
        0.0f, 1.0f
    );

    /* Entropy: based on attention distribution
     * More spread attention = higher entropy = higher free energy
     * Approximate based on allocation error */
    m->entropy = nimcp_clampf(
        m->attention_allocation_error * 0.7f + m->salience_prediction_error * 0.3f,
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
    void (*high_fe_cb)(sa_fep_bridge_t*, float, void*);
    float high_fe_value;
    void* high_fe_ud;

    void (*surprise_cb)(sa_fep_bridge_t*, float, const char*, void*);
    float surprise_value;
    const char* surprise_source;
    void* surprise_ud;

    void (*metrics_cb)(sa_fep_bridge_t*, const sa_fep_metrics_t*, void*);
    sa_fep_metrics_t metrics_snap;
    void* metrics_ud;

    sa_fep_bridge_t* bridge;
} sa_fep_deferred_cbs_t;

/**
 * @brief Check thresholds and stage callbacks (MUST be called under lock).
 */
static void check_callbacks_locked(sa_fep_bridge_t* bridge, sa_fep_deferred_cbs_t* out) {
    memset(out, 0, sizeof(*out));
    out->bridge = bridge;

    sa_fep_metrics_t* m = &bridge->metrics;
    const sa_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != SA_FEP_STATE_DEGRADED) {
            bridge->state = SA_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                out->high_fe_cb = bridge->high_fe_callback;
                out->high_fe_value = m->free_energy;
                out->high_fe_ud = bridge->high_fe_user_data;
            }
        }
    } else if (bridge->state == SA_FEP_STATE_DEGRADED) {
        bridge->state = SA_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->salience_contribution > m->attention_contribution &&
                m->salience_contribution > m->priority_contribution) {
                source = "salience";
            } else if (m->attention_contribution > m->priority_contribution) {
                source = "attention";
            } else {
                source = "priority";
            }
            out->surprise_cb = bridge->surprise_callback;
            out->surprise_value = m->surprise;
            out->surprise_source = source;
            out->surprise_ud = bridge->surprise_user_data;
        }
    }

    /* Track attention captures */
    if (m->attention_captured) {
        bridge->stats.attention_captures++;
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
static void sa_fire_deferred_callbacks(const sa_fep_deferred_cbs_t* d) {
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
static void update_stats(sa_fep_bridge_t* bridge, uint64_t update_time_us) {
    sa_fep_stats_t* s = &bridge->stats;
    sa_fep_metrics_t* m = &bridge->metrics;

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
static void store_previous_state(sa_fep_bridge_t* bridge) {
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_salience_error = bridge->metrics.salience_prediction_error;
    bridge->prev_attention_error = bridge->metrics.attention_allocation_error;
    bridge->prev_priority_error = bridge->metrics.priority_estimation_error;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

sa_fep_config_t sa_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_config_defaul", 0.0f);


    sa_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_logging = false;
    config.update_interval_ms = 50;  /* Cognitive timescale */

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.salience_prediction_weight = SA_FEP_SALIENCE_WEIGHT;
    config.attention_allocation_weight = SA_FEP_ATTENTION_WEIGHT;
    config.priority_estimation_weight = SA_FEP_PRIORITY_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.attention_efficiency_threshold = 0.7f;

    /* Normalization */
    config.baseline_free_energy = SA_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = SA_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = SA_FEP_ERROR_DECAY_RATE;

    return config;
}

sa_fep_bridge_t* sa_fep_bridge_create(const sa_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_create", 0.0f);


    sa_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(sa_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = sa_fep_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "salience_attention_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "sa_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = SA_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.salience_prediction_error = 0.0f;
    bridge->metrics.attention_allocation_error = 0.5f;  /* Start at neutral */
    bridge->metrics.priority_estimation_error = 0.5f;   /* Start at neutral */
    bridge->metrics.attention_efficiency = 0.5f;        /* Start at neutral */
    bridge->metrics.attention_captured = false;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_salience_error = 0.0f;
    bridge->prev_attention_error = 0.5f;
    bridge->prev_priority_error = 0.5f;

    bridge->state = SA_FEP_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "salience_attention_fep");
    return bridge;
}

void sa_fep_bridge_destroy(sa_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "salience_attention_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_destro", 0.0f);


    if (bridge->registered) {
        sa_fep_bridge_unregister(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int sa_fep_bridge_reset(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_reset: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(sa_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.attention_allocation_error = 0.5f;
    bridge->metrics.priority_estimation_error = 0.5f;
    bridge->metrics.attention_efficiency = 0.5f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_salience_error = 0.0f;
    bridge->prev_attention_error = 0.5f;
    bridge->prev_priority_error = 0.5f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(sa_fep_stats_t));

    bridge->state = SA_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int sa_fep_bridge_register(
    sa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    salience_attention_bridge_t* sa_bridge,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_register: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_regist", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check if already registered */
    if (bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        if (bridge_id_out) {
            *bridge_id_out = bridge->bridge_id;
        }
        return 0;  /* Already registered, success */
    }

    /* Store references under lock before registration attempt */
    bridge->orchestrator = orchestrator;
    bridge->sa_bridge = sa_bridge;

    /* Register with FEP orchestrator while holding lock.
     * Keeps refs + registered flag consistent (fixes TOCTOU where another
     * thread could see orchestrator set but registered still false). */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "salience_attention",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        sa_fep_update_callback,
        sa_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->sa_bridge = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "sa_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = SA_FEP_STATE_ACTIVE;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    return 0;
}

int sa_fep_bridge_unregister(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_unregister: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_unregi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not registered, nothing to do */
    }

    /* Snapshot orchestrator+bridge_id under lock, then call
     * fep_orchestrator_unregister_bridge OUTSIDE lock to prevent deadlock. */
    fep_orchestrator_t* orchestrator = bridge->orchestrator;
    uint32_t bridge_id = bridge->bridge_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unregister from orchestrator WITHOUT holding the lock */
    if (orchestrator) {
        fep_orchestrator_unregister_bridge(orchestrator, bridge_id);
    }

    /* Re-lock to clear state */
    nimcp_mutex_lock(bridge->base.mutex);
    bridge->registered = false;
    bridge->bridge_id = 0;
    bridge->orchestrator = NULL;
    bridge->sa_bridge = NULL;
    bridge->state = SA_FEP_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool sa_fep_bridge_is_registered(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_is_reg", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t sa_fep_bridge_get_id(sa_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_id", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int sa_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_update_callba", 0.0f);


    sa_fep_bridge_t* bridge = (sa_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_update_callback: NULL argument");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered */
    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "sa_fep_update_callback: bridge->registered is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    store_previous_state(bridge);

    /* Snapshot sa_bridge ref under lock, query it OUTSIDE lock
     * to prevent nested lock deadlock. */
    salience_attention_bridge_t* sa_bridge = (salience_attention_bridge_t*)bridge->sa_bridge;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Query salience-attention bridge WITHOUT holding the lock */
    if (sa_bridge) {
        salience_attention_stats_t sa_stats;
        memset(&sa_stats, 0, sizeof(sa_stats));

        int err = salience_attention_bridge_get_stats(sa_bridge, &sa_stats);
        if (err == 0) {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update metrics from salience-attention statistics */
            bridge->stats.salience_computations++;

            /* Estimate salience prediction error from detection accuracy
             * Use average salience score as a proxy - low average means
             * many false alarms or missed detections */
            if (sa_stats.salience_detections > 0) {
                /* Lower average salience = higher prediction error
                 * (detecting non-salient items or missing salient ones) */
                bridge->metrics.salience_prediction_error = nimcp_clampf(
                    1.0f - sa_stats.avg_salience_score,
                    0.0f, 1.0f
                );
            }

            /* Estimate attention allocation error from attention shifts
             * More shifts might indicate inefficient allocation */
            if (sa_stats.attention_shifts > 0 && sa_stats.total_events > 0) {
                float shift_rate = (float)sa_stats.attention_shifts /
                                   (float)sa_stats.total_events;
                /* Higher shift rate = less stable = higher error */
                bridge->metrics.attention_allocation_error = nimcp_clampf(
                    shift_rate,
                    0.0f, 1.0f
                );
                bridge->stats.attention_computations++;
            }

            /* Estimate attention efficiency from average attention strength
             * and focus notifications */
            if (sa_stats.focus_notifications > 0) {
                bridge->metrics.attention_efficiency = nimcp_clampf(
                    sa_stats.avg_attention_strength,
                    0.0f, 1.0f
                );
            }

            /* Priority estimation error from priority update frequency
             * Frequent priority updates suggest estimation difficulty */
            if (sa_stats.total_events > 0) {
                float priority_update_rate = (float)sa_stats.priority_updates /
                                             (float)sa_stats.total_events;
                /* Higher update rate = more volatile = higher estimation error */
                bridge->metrics.priority_estimation_error = nimcp_clampf(
                    priority_update_rate * 2.0f,  /* Scale factor */
                    0.0f, 1.0f
                );
                bridge->stats.priority_computations++;
            }

            /* Track active targets */
            bridge->metrics.salience_detections = (uint32_t)sa_stats.salience_detections;
            bridge->metrics.active_targets = (uint32_t)sa_stats.attention_shifts;

            nimcp_mutex_unlock(bridge->base.mutex);
        }
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Stage callbacks under lock, fire after unlock */
    sa_fep_deferred_cbs_t deferred;
    check_callbacks_locked(bridge, &deferred);

    nimcp_mutex_unlock(bridge->base.mutex);

    sa_fire_deferred_callbacks(&deferred);
    return 0;
}

void sa_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via sa_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_destroy_callb", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int sa_fep_bridge_force_update(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_force_update: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_force_", 0.0f);


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
    sa_fep_deferred_cbs_t deferred2;
    check_callbacks_locked(bridge, &deferred2);

    nimcp_mutex_unlock(bridge->base.mutex);

    sa_fire_deferred_callbacks(&deferred2);
    return 0;
}

int sa_fep_bridge_update_salience_error(
    sa_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_update_salience_error: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.salience_prediction_error = nimcp_clampf(error, 0.0f, 1.0f);
    bridge->stats.salience_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_update_attention_error(
    sa_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_update_attention_error: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.attention_allocation_error = nimcp_clampf(error, 0.0f, 1.0f);
    bridge->stats.attention_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_update_priority_error(
    sa_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_update_priority_error: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.priority_estimation_error = nimcp_clampf(error, 0.0f, 1.0f);
    bridge->stats.priority_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_update_attention_efficiency(
    sa_fep_bridge_t* bridge,
    float efficiency
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_update_attention_efficiency: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.attention_efficiency = nimcp_clampf(efficiency, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int sa_fep_bridge_get_metrics(
    const sa_fep_bridge_t* bridge,
    sa_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_get_metrics: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_me", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int sa_fep_bridge_get_stats(
    const sa_fep_bridge_t* bridge,
    sa_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_get_stats: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int sa_fep_bridge_reset_stats(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_reset_stats: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(sa_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float sa_fep_bridge_get_free_energy(sa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_fr", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float sa_fep_bridge_get_salience_error(sa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_sa", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    float se = bridge->metrics.salience_prediction_error;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return se;
}

float sa_fep_bridge_get_prediction_error(sa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_pr", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

float sa_fep_bridge_get_attention_efficiency(sa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_at", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    float eff = bridge->metrics.attention_efficiency;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return eff;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

sa_fep_state_t sa_fep_bridge_get_state(sa_fep_bridge_t* bridge) {
    if (!bridge) return SA_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    sa_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool sa_fep_bridge_is_degraded(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_is_deg", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == SA_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool sa_fep_bridge_is_efficient(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_is_eff", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    bool efficient = (bridge->metrics.attention_efficiency >=
                      bridge->config.attention_efficiency_threshold);
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return efficient;
}

const char* sa_fep_state_name(sa_fep_state_t state) {
    switch (state) {
        case SA_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case SA_FEP_STATE_IDLE:          return "idle";
        case SA_FEP_STATE_ACTIVE:        return "active";
        case SA_FEP_STATE_DEGRADED:      return "degraded";
        case SA_FEP_STATE_ERROR:         return "error";
        default:                          return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int sa_fep_bridge_set_high_fe_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_set_high_fe_callback: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_set_hi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_set_surprise_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_set_surprise_callback: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_set_su", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_set_metrics_callback(
    sa_fep_bridge_t* bridge,
    sa_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_set_metrics_callback: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_set_me", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int sa_fep_bridge_set_config(
    sa_fep_bridge_t* bridge,
    const sa_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_set_config: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_set_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int sa_fep_bridge_get_config(
    const sa_fep_bridge_t* bridge,
    sa_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "sa_fep_bridge_get_config: NULL argument");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    salience_attention_fep_bridge_heartbeat("salience_att_sa_fep_bridge_get_co", 0.0f);


    nimcp_mutex_lock(((sa_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((sa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void salience_attention_fep_bridge_set_instance_health_agent(sa_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "salience_attention_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int salience_attention_fep_bridge_training_begin(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    salience_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_fep_bridge_training_begin", 0.0f);
    return 0;
}

int salience_attention_fep_bridge_training_end(sa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_fep_bridge_training_end: NULL argument");
        return -1;
    }
    salience_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_fep_bridge_training_end", 1.0f);
    return 0;
}

int salience_attention_fep_bridge_training_step(sa_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "salience_attention_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    salience_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "salience_attention_fep_bridge_training_step", progress);
    return 0;
}
