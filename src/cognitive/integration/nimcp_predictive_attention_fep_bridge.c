/**
 * @file nimcp_predictive_attention_fep_bridge.c
 * @brief Predictive-Attention - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for predictive-attention
 * WHY:  Enable coordinated free energy minimization for predictive coding and attention
 * HOW:  Compute free energy from prediction accuracy, attention precision, error quality
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_predictive_attention_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_predictive_attention_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(predictive_attention_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_predictive_attention_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_predictive_attention_fep_bridge_mesh_registry = NULL;

nimcp_error_t predictive_attention_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_predictive_attention_fep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "predictive_attention_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "predictive_attention_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_predictive_attention_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_predictive_attention_fep_bridge_mesh_registry = registry;
    return err;
}

void predictive_attention_fep_bridge_mesh_unregister(void) {
    if (g_predictive_attention_fep_bridge_mesh_registry && g_predictive_attention_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_predictive_attention_fep_bridge_mesh_registry, g_predictive_attention_fep_bridge_mesh_id);
        g_predictive_attention_fep_bridge_mesh_id = 0;
        g_predictive_attention_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from predictive_attention_fep_bridge module (instance-level) */
static inline void predictive_attention_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_predictive_attention_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_predictive_attention_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_predictive_attention_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "PREDICTIVE_ATTENTION_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct pa_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    pa_fep_config_t config;

    /* State */
    pa_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    predictive_attention_bridge_t* pa_bridge;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    pa_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_prediction_accuracy;
    float prev_attention_precision;
    float prev_error_signal_quality;

    /* Running averages */
    float running_avg_fe;
    uint64_t running_count;

    /* Statistics */
    pa_fep_stats_t stats;

    /* Callbacks */
    pa_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    pa_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    pa_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from predictive-attention metrics
 *
 * Free energy model for predictive-attention:
 * FE = baseline + prediction_contrib + precision_contrib + error_quality_contrib
 *
 * Where:
 * - prediction_contrib = (1 - prediction_accuracy) * prediction_weight
 *   (lower accuracy = higher free energy)
 * - precision_contrib = (1 - attention_precision) * precision_weight
 *   (lower precision = higher free energy)
 * - error_quality_contrib = (1 - error_signal_quality) * error_quality_weight
 *   (lower quality = higher free energy)
 *
 * High precision mode is achieved when attention_precision > (1 - precision_epsilon)
 */
static void compute_free_energy(pa_fep_bridge_t* bridge) {
    pa_fep_metrics_t* m = &bridge->metrics;
    const pa_fep_config_t* cfg = &bridge->config;

    /* Clamp input metrics */
    m->prediction_accuracy = nimcp_clampf(m->prediction_accuracy, 0.0f, 1.0f);
    m->attention_precision = nimcp_clampf(m->attention_precision, 0.0f, 1.0f);
    m->error_signal_quality = nimcp_clampf(m->error_signal_quality, 0.0f, 1.0f);

    /* Prediction accuracy contribution:
     * Low prediction accuracy = high free energy (model doesn't match reality) */
    m->prediction_contribution = (1.0f - m->prediction_accuracy) * cfg->prediction_accuracy_weight;

    /* Attention precision contribution:
     * Low precision = poorly targeted attention = high free energy */
    m->precision_contribution = (1.0f - m->attention_precision) * cfg->attention_precision_weight;

    /* Error signal quality contribution:
     * Poor error signals = cannot learn effectively = high free energy */
    m->error_quality_contribution = (1.0f - m->error_signal_quality) * cfg->error_signal_quality_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     (m->prediction_contribution +
                      m->precision_contribution +
                      m->error_quality_contribution) * cfg->free_energy_weight;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* High precision mode check */
    m->high_precision_mode = (m->attention_precision > (1.0f - cfg->precision_epsilon));

    /* Prediction error: inverse of accuracy with decay smoothing */
    float new_prediction_error = (1.0f - m->prediction_accuracy * 0.6f -
                                   m->attention_precision * 0.25f -
                                   m->error_signal_quality * 0.15f);

    /* Apply decay to smooth transitions */
    m->prediction_error = nimcp_clampf(
        new_prediction_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float accuracy_change = fabsf(m->prediction_accuracy - bridge->prev_prediction_accuracy);
    float precision_change = fabsf(m->attention_precision - bridge->prev_attention_precision);
    float quality_change = fabsf(m->error_signal_quality - bridge->prev_error_signal_quality);

    m->surprise = nimcp_clampf(
        (fe_change * 0.3f + accuracy_change * 0.3f +
         precision_change * 0.2f + quality_change * 0.2f),
        0.0f, 1.0f
    );

    /* Entropy: based on precision variance and prediction uncertainty
     * Higher variance in precision = higher entropy */
    m->entropy = nimcp_clampf(
        (1.0f - m->attention_precision) * 0.5f +
        (1.0f - m->prediction_accuracy) * 0.3f +
        m->precision_variance * 0.2f,
        0.0f, 1.0f
    );
}

/**
 * @brief Check and trigger callbacks
 */
/**
 * @brief Deferred callback info — filled under lock, invoked after unlock
 */
typedef struct {
    /* high-FE callback */
    void (*high_fe_cb)(pa_fep_bridge_t*, float, void*);
    float high_fe_value;
    void* high_fe_ud;

    /* surprise callback */
    void (*surprise_cb)(pa_fep_bridge_t*, float, const char*, void*);
    float surprise_value;
    const char* surprise_source;
    void* surprise_ud;

    /* metrics callback */
    void (*metrics_cb)(pa_fep_bridge_t*, const pa_fep_metrics_t*, void*);
    pa_fep_metrics_t metrics_snap;
    void* metrics_ud;

    pa_fep_bridge_t* bridge;
} pa_fep_deferred_cbs_t;

/**
 * @brief Check thresholds and stage callbacks (MUST be called under lock).
 *
 * State updates happen here under the lock. Callback function pointers and
 * their arguments are snapshotted into `out` for deferred invocation.
 */
static void check_callbacks_locked(pa_fep_bridge_t* bridge, pa_fep_deferred_cbs_t* out) {
    memset(out, 0, sizeof(*out));
    out->bridge = bridge;

    pa_fep_metrics_t* m = &bridge->metrics;
    const pa_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != PA_FEP_STATE_DEGRADED) {
            bridge->state = PA_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                out->high_fe_cb = bridge->high_fe_callback;
                out->high_fe_value = m->free_energy;
                out->high_fe_ud = bridge->high_fe_user_data;
            }
        }
    } else if (bridge->state == PA_FEP_STATE_DEGRADED) {
        bridge->state = PA_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->prediction_contribution > m->precision_contribution &&
                m->prediction_contribution > m->error_quality_contribution) {
                source = "prediction";
            } else if (m->precision_contribution > m->error_quality_contribution) {
                source = "precision";
            } else {
                source = "error_quality";
            }
            out->surprise_cb = bridge->surprise_callback;
            out->surprise_value = m->surprise;
            out->surprise_source = source;
            out->surprise_ud = bridge->surprise_user_data;
        }
    }

    /* Metrics callback */
    if (bridge->metrics_callback) {
        out->metrics_cb = bridge->metrics_callback;
        out->metrics_snap = *m;  /* Snapshot metrics under lock */
        out->metrics_ud = bridge->metrics_user_data;
    }
}

/**
 * @brief Fire deferred callbacks outside the lock.
 */
static void fire_deferred_callbacks(const pa_fep_deferred_cbs_t* d) {
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
static void update_stats(pa_fep_bridge_t* bridge, uint64_t update_time_us) {
    pa_fep_stats_t* s = &bridge->stats;
    pa_fep_metrics_t* m = &bridge->metrics;

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
static void store_previous_state(pa_fep_bridge_t* bridge) {
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_prediction_accuracy = bridge->metrics.prediction_accuracy;
    bridge->prev_attention_precision = bridge->metrics.attention_precision;
    bridge->prev_error_signal_quality = bridge->metrics.error_signal_quality;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

pa_fep_config_t pa_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_config_defaul", 0.0f);


    pa_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_logging = false;
    config.update_interval_ms = 50;  /* Cognitive timescale */

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.prediction_accuracy_weight = PA_FEP_PREDICTION_WEIGHT;
    config.attention_precision_weight = PA_FEP_PRECISION_WEIGHT;
    config.error_signal_quality_weight = PA_FEP_ERROR_QUALITY_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.precision_epsilon = 0.05f;

    /* Normalization */
    config.baseline_free_energy = PA_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = PA_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = PA_FEP_ERROR_DECAY_RATE;

    return config;
}

pa_fep_bridge_t* pa_fep_bridge_create(const pa_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_create", 0.0f);


    pa_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(pa_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = pa_fep_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "predictive_attention_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "pa_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = PA_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline/defaults */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.prediction_accuracy = 0.5f;  /* Start with neutral accuracy */
    bridge->metrics.attention_precision = 0.5f;  /* Start with moderate precision */
    bridge->metrics.error_signal_quality = 0.5f; /* Start with moderate quality */
    bridge->metrics.high_precision_mode = false;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_prediction_accuracy = 0.5f;
    bridge->prev_attention_precision = 0.5f;
    bridge->prev_error_signal_quality = 0.5f;

    bridge->state = PA_FEP_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "predictive_attention_fep");
    return bridge;
}

void pa_fep_bridge_destroy(pa_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "predictive_attention_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_destro", 0.0f);


    if (bridge->registered) {
        pa_fep_bridge_unregister(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int pa_fep_bridge_reset(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(pa_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_accuracy = 0.5f;
    bridge->metrics.attention_precision = 0.5f;
    bridge->metrics.error_signal_quality = 0.5f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_prediction_accuracy = 0.5f;
    bridge->prev_attention_precision = 0.5f;
    bridge->prev_error_signal_quality = 0.5f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(pa_fep_stats_t));

    bridge->state = PA_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int pa_fep_bridge_register(
    pa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    predictive_attention_bridge_t* pa_bridge,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_register: required parameter is NULL (bridge, orchestrator)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_regist", 0.0f);


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
    bridge->pa_bridge = pa_bridge;

    /* Register with FEP orchestrator while holding lock.
     * Keeps refs + registered flag consistent (fixes TOCTOU where another
     * thread could see orchestrator set but registered still false). */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "predictive_attention",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        pa_fep_update_callback,
        pa_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->pa_bridge = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pa_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = PA_FEP_STATE_ACTIVE;
    nimcp_mutex_unlock(bridge->base.mutex);

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    return 0;
}

int pa_fep_bridge_unregister(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_unregi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;  /* Not registered, nothing to do */
    }

    fep_orchestrator_t* orchestrator = bridge->orchestrator;
    uint32_t bridge_id = bridge->bridge_id;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Unregister from orchestrator */
    if (orchestrator) {
        fep_orchestrator_unregister_bridge(orchestrator, bridge_id);
    }

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->registered = false;
    bridge->bridge_id = 0;
    bridge->orchestrator = NULL;
    bridge->pa_bridge = NULL;
    bridge->state = PA_FEP_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool pa_fep_bridge_is_registered(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_is_reg", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t pa_fep_bridge_get_id(pa_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_id", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int pa_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_update_callba", 0.0f);


    pa_fep_bridge_t* bridge = (pa_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered */
    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_update_callback: bridge->registered is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    store_previous_state(bridge);

    /* If we have a predictive-attention bridge, query it for statistics */
    predictive_attention_bridge_t* pa_bridge = bridge->pa_bridge;

    nimcp_mutex_unlock(bridge->base.mutex);

    if (pa_bridge) {
        predictive_attention_bridge_stats_t pa_stats;
        memset(&pa_stats, 0, sizeof(pa_stats));

        int err = predictive_attention_bridge_get_stats(pa_bridge, &pa_stats);
        if (err == 0) {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update prediction accuracy from bridge statistics */
            bridge->stats.prediction_computations++;

            /* Estimate prediction accuracy from error statistics */
            if (pa_stats.total_events > 0) {
                /* More prediction errors = lower accuracy */
                float error_rate = (float)pa_stats.prediction_errors /
                                  (float)pa_stats.total_events;
                bridge->metrics.prediction_accuracy = nimcp_clampf(
                    1.0f - error_rate,
                    0.0f, 1.0f
                );
            }

            /* Estimate attention precision from surprise shifts and avg error */
            if (pa_stats.surprise_shifts > 0 && pa_stats.total_events > 0) {
                /* More surprise shifts = attention was less precise */
                float shift_rate = (float)pa_stats.surprise_shifts /
                                  (float)pa_stats.total_events;
                bridge->metrics.attention_precision = nimcp_clampf(
                    1.0f - shift_rate * 2.0f,
                    0.0f, 1.0f
                );
            }

            /* Error signal quality from average precision and error magnitude */
            float avg_precision = pa_stats.avg_precision;
            float avg_error = pa_stats.avg_error_magnitude;
            if (avg_precision > 0.0f || avg_error > 0.0f) {
                /* High precision + low error = good error signals */
                bridge->metrics.error_signal_quality = nimcp_clampf(
                    avg_precision * 0.5f + (1.0f - avg_error) * 0.5f,
                    0.0f, 1.0f
                );
            }

            /* Update active predictions and attention shifts */
            bridge->metrics.active_predictions = pa_stats.focus_predictions;
            bridge->metrics.attention_shifts = pa_stats.surprise_shifts;

            bridge->stats.precision_updates++;
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
    pa_fep_deferred_cbs_t deferred;
    check_callbacks_locked(bridge, &deferred);

    nimcp_mutex_unlock(bridge->base.mutex);

    fire_deferred_callbacks(&deferred);
    return 0;
}

void pa_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via pa_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_destroy_callb", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int pa_fep_bridge_force_update(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_force_", 0.0f);


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
    pa_fep_deferred_cbs_t deferred2;
    check_callbacks_locked(bridge, &deferred2);

    nimcp_mutex_unlock(bridge->base.mutex);

    fire_deferred_callbacks(&deferred2);
    return 0;
}

int pa_fep_bridge_update_prediction_accuracy(
    pa_fep_bridge_t* bridge,
    float accuracy
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_update_prediction_accuracy: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.prediction_accuracy = nimcp_clampf(accuracy, 0.0f, 1.0f);
    bridge->stats.prediction_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pa_fep_bridge_update_attention_precision(
    pa_fep_bridge_t* bridge,
    float precision
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_update_attention_precision: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.attention_precision = nimcp_clampf(precision, 0.0f, 1.0f);
    bridge->stats.precision_updates++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pa_fep_bridge_update_error_signal_quality(
    pa_fep_bridge_t* bridge,
    float quality
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_update_error_signal_quality: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.error_signal_quality = nimcp_clampf(quality, 0.0f, 1.0f);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int pa_fep_bridge_get_metrics(
    const pa_fep_bridge_t* bridge,
    pa_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_me", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int pa_fep_bridge_get_stats(
    const pa_fep_bridge_t* bridge,
    pa_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int pa_fep_bridge_reset_stats(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(pa_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float pa_fep_bridge_get_free_energy(pa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_fr", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float pa_fep_bridge_get_prediction_accuracy(pa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_pr", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    float pa = bridge->metrics.prediction_accuracy;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return pa;
}

float pa_fep_bridge_get_prediction_error(pa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_pr", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

float pa_fep_bridge_get_attention_precision(pa_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_at", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    float ap = bridge->metrics.attention_precision;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return ap;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

pa_fep_state_t pa_fep_bridge_get_state(pa_fep_bridge_t* bridge) {
    if (!bridge) return PA_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    pa_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool pa_fep_bridge_is_degraded(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_is_deg", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == PA_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool pa_fep_bridge_is_high_precision(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_is_hig", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    bool high_precision = bridge->metrics.high_precision_mode;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return high_precision;
}

const char* pa_fep_state_name(pa_fep_state_t state) {
    switch (state) {
        case PA_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case PA_FEP_STATE_IDLE:          return "idle";
        case PA_FEP_STATE_ACTIVE:        return "active";
        case PA_FEP_STATE_DEGRADED:      return "degraded";
        case PA_FEP_STATE_ERROR:         return "error";
        default:                          return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int pa_fep_bridge_set_high_fe_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_set_hi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pa_fep_bridge_set_surprise_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_set_su", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pa_fep_bridge_set_metrics_callback(
    pa_fep_bridge_t* bridge,
    pa_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_set_me", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int pa_fep_bridge_set_config(
    pa_fep_bridge_t* bridge,
    const pa_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_set_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int pa_fep_bridge_get_config(
    const pa_fep_bridge_t* bridge,
    pa_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pa_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    predictive_attention_fep_bridge_heartbeat("predictive_a_pa_fep_bridge_get_co", 0.0f);


    nimcp_mutex_lock(((pa_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((pa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void predictive_attention_fep_bridge_set_instance_health_agent(pa_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "predictive_attention_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int predictive_attention_fep_bridge_training_begin(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    predictive_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_fep_bridge_training_begin", 0.0f);
    return 0;
}

int predictive_attention_fep_bridge_training_end(pa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_fep_bridge_training_end: NULL argument");
        return -1;
    }
    predictive_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_fep_bridge_training_end", 1.0f);
    return 0;
}

int predictive_attention_fep_bridge_training_step(pa_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "predictive_attention_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    predictive_attention_fep_bridge_heartbeat_instance(bridge->health_agent, "predictive_attention_fep_bridge_training_step", progress);
    return 0;
}
