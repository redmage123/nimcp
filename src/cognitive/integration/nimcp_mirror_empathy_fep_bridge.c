/**
 * @file nimcp_mirror_empathy_fep_bridge.c
 * @brief Mirror-Empathy - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for mirror-empathy
 * WHY:  Enable coordinated free energy minimization for social cognition
 * HOW:  Compute free energy from mirroring error, empathy prediction, resonance
 *
 * @author NIMCP Development Team
 */

#include "cognitive/integration/nimcp_mirror_empathy_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/integration/nimcp_mirror_empathy_bridge.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_empathy_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_empathy_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_empathy_fep_bridge_mesh_registry = NULL;

nimcp_error_t mirror_empathy_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_empathy_fep_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_empathy_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_empathy_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_empathy_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_empathy_fep_bridge_mesh_registry = registry;
    return err;
}

void mirror_empathy_fep_bridge_mesh_unregister(void) {
    if (g_mirror_empathy_fep_bridge_mesh_registry && g_mirror_empathy_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_empathy_fep_bridge_mesh_registry, g_mirror_empathy_fep_bridge_mesh_id);
        g_mirror_empathy_fep_bridge_mesh_id = 0;
        g_mirror_empathy_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from mirror_empathy_fep_bridge module (instance-level) */
static inline void mirror_empathy_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_empathy_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_empathy_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_empathy_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "MIRROR_EMPATHY_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct me_fep_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */

    /* Configuration */
    me_fep_config_t config;

    /* State */
    me_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    mirror_empathy_bridge_t* me_bridge;
    uint32_t bridge_id;
    bool registered;

    /* Metrics */
    me_fep_metrics_t metrics;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_mirroring_error;
    float prev_empathy_error;
    float prev_resonance_deficit;

    /* Running averages */
    float running_avg_fe;
    uint64_t running_count;

    /* Statistics */
    me_fep_stats_t stats;

    /* Callbacks */
    me_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    me_fep_surprise_callback_t surprise_callback;
    void* surprise_user_data;
    me_fep_metrics_callback_t metrics_callback;
    void* metrics_user_data;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from mirror-empathy metrics
 *
 * Free energy model for social cognition:
 * FE = baseline + mirroring_contrib + empathy_contrib + resonance_contrib
 *
 * Where:
 * - mirroring_contrib = mirroring_error * mirroring_weight
 *   (higher action mirroring error = higher FE)
 * - empathy_contrib = empathy_prediction_error * empathy_weight
 *   (worse empathy prediction = higher FE)
 * - resonance_contrib = resonance_deficit * resonance_weight
 *   (lower emotional resonance = higher FE)
 *
 * High resonance represents minimum free energy state where social
 * predictions are accurate and emotional connection is strong.
 */
static void compute_free_energy(me_fep_bridge_t* bridge) {
    me_fep_metrics_t* m = &bridge->metrics;
    const me_fep_config_t* cfg = &bridge->config;

    /* Clamp input metrics */
    m->mirroring_error = nimcp_clampf(m->mirroring_error, 0.0f, 1.0f);
    m->empathy_prediction_error = nimcp_clampf(m->empathy_prediction_error, 0.0f, 1.0f);
    m->resonance_deficit = nimcp_clampf(m->resonance_deficit, 0.0f, 1.0f);

    /* Mirroring accuracy contribution:
     * High error in understanding observed actions = prediction error */
    m->mirroring_contribution = m->mirroring_error * cfg->mirroring_accuracy_weight;

    /* Empathy prediction contribution:
     * Poor predictions of others' emotional states = prediction error */
    m->empathy_contribution = m->empathy_prediction_error * cfg->empathy_prediction_weight;

    /* Emotional resonance contribution:
     * Deficit in resonance represents failure to achieve shared affect
     * High resonance = successful social prediction = low free energy */
    m->resonance_contribution = m->resonance_deficit * cfg->emotional_resonance_weight;

    /* Total free energy */
    float total_fe = cfg->baseline_free_energy +
                     (m->mirroring_contribution +
                      m->empathy_contribution +
                      m->resonance_contribution) * cfg->free_energy_weight;

    m->free_energy = nimcp_clampf(total_fe, 0.0f, cfg->max_free_energy);

    /* High resonance state check (low deficit = high resonance) */
    m->high_resonance_state = (m->resonance_deficit < cfg->resonance_epsilon);

    /* Prediction error: weighted combination of all uncertainty sources */
    float new_prediction_error = (m->mirroring_error * 0.35f +
                                   m->empathy_prediction_error * 0.35f +
                                   m->resonance_deficit * 0.30f);

    /* Apply decay to smooth transitions */
    m->prediction_error = nimcp_clampf(
        new_prediction_error * cfg->error_decay_rate +
        bridge->prev_prediction_error * (1.0f - cfg->error_decay_rate),
        0.0f, 1.0f
    );

    /* Surprise: unexpected changes in social state */
    float fe_change = fabsf(m->free_energy - bridge->prev_free_energy);
    float mirroring_change = fabsf(m->mirroring_error - bridge->prev_mirroring_error);
    float empathy_change = fabsf(m->empathy_prediction_error - bridge->prev_empathy_error);
    float resonance_change = fabsf(m->resonance_deficit - bridge->prev_resonance_deficit);

    m->surprise = nimcp_clampf(
        (fe_change * 0.3f + mirroring_change * 0.25f +
         empathy_change * 0.25f + resonance_change * 0.2f),
        0.0f, 1.0f
    );

    /* Entropy: based on prediction uncertainty
     * Higher uncertainty about social states = higher entropy */
    m->entropy = nimcp_clampf(
        m->mirroring_error * 0.4f + m->empathy_prediction_error * 0.4f +
        m->resonance_deficit * 0.2f,
        0.0f, 1.0f
    );

    /* Intention uncertainty approximation from social prediction errors */
    m->intention_uncertainty = m->mirroring_error * m->empathy_prediction_error;
}

/**
 * @brief Check and trigger callbacks
 */
/**
 * @brief Deferred callback info -- filled under lock, invoked after unlock
 */
typedef struct {
    void (*high_fe_cb)(me_fep_bridge_t*, float, void*);
    float high_fe_value;
    void* high_fe_ud;

    void (*surprise_cb)(me_fep_bridge_t*, float, const char*, void*);
    float surprise_value;
    const char* surprise_source;
    void* surprise_ud;

    void (*metrics_cb)(me_fep_bridge_t*, const me_fep_metrics_t*, void*);
    me_fep_metrics_t metrics_snap;
    void* metrics_ud;

    me_fep_bridge_t* bridge;
} me_fep_deferred_cbs_t;

/**
 * @brief Check thresholds and stage callbacks (MUST be called under lock).
 */
static void check_callbacks_locked(me_fep_bridge_t* bridge, me_fep_deferred_cbs_t* out) {
    memset(out, 0, sizeof(*out));
    out->bridge = bridge;

    me_fep_metrics_t* m = &bridge->metrics;
    const me_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (m->free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != ME_FEP_STATE_DEGRADED) {
            bridge->state = ME_FEP_STATE_DEGRADED;
            bridge->stats.degraded_mode_entries++;

            if (bridge->high_fe_callback) {
                out->high_fe_cb = bridge->high_fe_callback;
                out->high_fe_value = m->free_energy;
                out->high_fe_ud = bridge->high_fe_user_data;
            }
        }
    } else if (bridge->state == ME_FEP_STATE_DEGRADED) {
        bridge->state = ME_FEP_STATE_ACTIVE;
    }

    /* Check for surprise events */
    if (m->surprise > cfg->prediction_error_threshold) {
        bridge->stats.surprise_events++;

        if (bridge->surprise_callback) {
            const char* source = "unknown";
            if (m->mirroring_contribution > m->empathy_contribution &&
                m->mirroring_contribution > m->resonance_contribution) {
                source = "mirroring";
            } else if (m->empathy_contribution > m->resonance_contribution) {
                source = "empathy";
            } else {
                source = "resonance";
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
        out->metrics_snap = *m;
        out->metrics_ud = bridge->metrics_user_data;
    }
}

/**
 * @brief Fire deferred callbacks outside the lock.
 */
static void me_fire_deferred_callbacks(const me_fep_deferred_cbs_t* d) {
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
static void update_stats(me_fep_bridge_t* bridge, uint64_t update_time_us) {
    me_fep_stats_t* s = &bridge->stats;
    me_fep_metrics_t* m = &bridge->metrics;

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
static void store_previous_state(me_fep_bridge_t* bridge) {
    bridge->prev_free_energy = bridge->metrics.free_energy;
    bridge->prev_prediction_error = bridge->metrics.prediction_error;
    bridge->prev_mirroring_error = bridge->metrics.mirroring_error;
    bridge->prev_empathy_error = bridge->metrics.empathy_prediction_error;
    bridge->prev_resonance_deficit = bridge->metrics.resonance_deficit;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

me_fep_config_t me_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_config_defaul", 0.0f);


    me_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Feature enables */
    config.enable_logging = false;
    config.update_interval_ms = 50;  /* Cognitive timescale */

    /* Weighting parameters - weight must be high enough for max errors to exceed threshold */
    config.free_energy_weight = 2.0f;
    config.mirroring_accuracy_weight = ME_FEP_MIRRORING_WEIGHT;
    config.empathy_prediction_weight = ME_FEP_EMPATHY_WEIGHT;
    config.emotional_resonance_weight = ME_FEP_RESONANCE_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.prediction_error_threshold = 0.5f;
    config.resonance_epsilon = 0.1f;

    /* Normalization */
    config.baseline_free_energy = ME_FEP_BASELINE_FREE_ENERGY;
    config.max_free_energy = ME_FEP_MAX_FREE_ENERGY;
    config.error_decay_rate = ME_FEP_ERROR_DECAY_RATE;

    return config;
}

me_fep_bridge_t* me_fep_bridge_create(const me_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_create", 0.0f);


    me_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(me_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = me_fep_config_default();
    }

    /* Initialize base bridge infrastructure */
    if (bridge_base_init(&bridge->base, 0, "mirror_empathy_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "me_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = ME_FEP_STATE_UNINITIALIZED;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics to baseline */
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.0f;
    bridge->metrics.mirroring_error = 0.0f;
    bridge->metrics.empathy_prediction_error = 0.0f;
    bridge->metrics.resonance_deficit = 1.0f;  /* Start with low resonance */
    bridge->metrics.high_resonance_state = false;

    /* Initialize previous state tracking */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_mirroring_error = 0.0f;
    bridge->prev_empathy_error = 0.0f;
    bridge->prev_resonance_deficit = 1.0f;

    bridge->state = ME_FEP_STATE_IDLE;

    NIMCP_LOGGING_INFO("Created %s bridge", "mirror_empathy_fep");
    return bridge;
}

void me_fep_bridge_destroy(me_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_empathy_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_destro", 0.0f);


    if (bridge->registered) {
        me_fep_bridge_unregister(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int me_fep_bridge_reset(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    memset(&bridge->metrics, 0, sizeof(me_fep_metrics_t));
    bridge->metrics.free_energy = bridge->config.baseline_free_energy;
    bridge->metrics.resonance_deficit = 1.0f;

    /* Reset previous state */
    bridge->prev_free_energy = bridge->config.baseline_free_energy;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_mirroring_error = 0.0f;
    bridge->prev_empathy_error = 0.0f;
    bridge->prev_resonance_deficit = 1.0f;

    /* Reset running averages */
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(me_fep_stats_t));

    bridge->state = ME_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int me_fep_bridge_register(
    me_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    mirror_empathy_bridge_t* me_bridge,
    uint32_t* bridge_id_out
) {
    if (!bridge || !orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_register: required parameter is NULL (bridge, orchestrator)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_regist", 0.0f);


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
    bridge->me_bridge = me_bridge;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "mirror_empathy",
        FEP_BRIDGE_CATEGORY_COGNITIVE,  /* 50ms cognitive timescale */
        (fep_bridge_handle_t)bridge,
        me_fep_update_callback,
        me_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->me_bridge = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "me_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = ME_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int me_fep_bridge_unregister(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_unregister: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_unregi", 0.0f);


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
    bridge->me_bridge = NULL;
    bridge->state = ME_FEP_STATE_IDLE;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

bool me_fep_bridge_is_registered(me_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_is_reg", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t me_fep_bridge_get_id(me_fep_bridge_t* bridge) {
    if (!bridge) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_id", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int me_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_update_callba", 0.0f);


    me_fep_bridge_t* bridge = (me_fep_bridge_t*)handle;
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_update_callback: bridge is NULL");
        return -1;
    }

    nimcp_mutex_lock(bridge->base.mutex);

    /* Ensure we're registered */
    if (!bridge->registered) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_update_callback: bridge->registered is NULL");
        return -1;
    }

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    store_previous_state(bridge);

    /* Snapshot me_bridge ref under lock, query it OUTSIDE lock
     * to prevent nested lock deadlock. */
    mirror_empathy_bridge_t* me_bridge = (mirror_empathy_bridge_t*)bridge->me_bridge;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Query mirror-empathy bridge WITHOUT holding the lock */
    if (me_bridge) {
        mirror_empathy_stats_t me_stats;
        memset(&me_stats, 0, sizeof(me_stats));

        int err = mirror_empathy_bridge_get_stats(me_bridge, &me_stats);
        if (err == 0) {
            nimcp_mutex_lock(bridge->base.mutex);

            /* Update metrics from mirror-empathy statistics */
            bridge->stats.mirroring_computations++;

            /* Estimate mirroring error from action understanding rate */
            if (me_stats.total_events > 0) {
                float action_success_rate = (float)me_stats.actions_mirrored /
                                           (float)me_stats.total_events;
                /* Lower success rate = higher mirroring error */
                bridge->metrics.mirroring_error = nimcp_clampf(
                    1.0f - action_success_rate * 2.0f,  /* Scale up */
                    0.0f, 1.0f
                );
            }

            /* Estimate empathy prediction from response rate */
            if (me_stats.events_received > 0) {
                float response_rate = (float)me_stats.empathetic_responses /
                                     (float)me_stats.events_received;
                /* Lower response rate = higher prediction error */
                bridge->metrics.empathy_prediction_error = nimcp_clampf(
                    1.0f - response_rate,
                    0.0f, 1.0f
                );
            }

            /* Resonance deficit from average resonance strength */
            bridge->metrics.resonance_deficit = nimcp_clampf(
                1.0f - me_stats.avg_resonance_strength,
                0.0f, 1.0f
            );

            /* Update active interactions count */
            bridge->metrics.active_interactions = (uint32_t)me_stats.empathetic_responses;
            bridge->metrics.successful_predictions = (uint32_t)me_stats.intentions_predicted;

            bridge->stats.empathy_computations++;
            bridge->stats.resonance_computations++;

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
    me_fep_deferred_cbs_t deferred;
    check_callbacks_locked(bridge, &deferred);

    nimcp_mutex_unlock(bridge->base.mutex);

    me_fire_deferred_callbacks(&deferred);
    return 0;
}

void me_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via me_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_destroy_callb", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int me_fep_bridge_force_update(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_force_update: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_force_", 0.0f);


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
    me_fep_deferred_cbs_t deferred2;
    check_callbacks_locked(bridge, &deferred2);

    nimcp_mutex_unlock(bridge->base.mutex);

    me_fire_deferred_callbacks(&deferred2);
    return 0;
}

int me_fep_bridge_update_mirroring_error(
    me_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_update_mirroring_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.mirroring_error = nimcp_clampf(error, 0.0f, 1.0f);
    bridge->stats.mirroring_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int me_fep_bridge_update_empathy_error(
    me_fep_bridge_t* bridge,
    float error
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_update_empathy_error: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.empathy_prediction_error = nimcp_clampf(error, 0.0f, 1.0f);
    bridge->stats.empathy_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int me_fep_bridge_update_resonance_deficit(
    me_fep_bridge_t* bridge,
    float deficit
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_update_resonance_deficit: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_update", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics.resonance_deficit = nimcp_clampf(deficit, 0.0f, 1.0f);
    bridge->stats.resonance_computations++;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * METRICS AND STATISTICS
 *===========================================================================*/

int me_fep_bridge_get_metrics(
    const me_fep_bridge_t* bridge,
    me_fep_metrics_t* metrics_out
) {
    if (!bridge || !metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_get_metrics: required parameter is NULL (bridge, metrics_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_me", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    *metrics_out = bridge->metrics;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int me_fep_bridge_get_stats(
    const me_fep_bridge_t* bridge,
    me_fep_stats_t* stats_out
) {
    if (!bridge || !stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_get_stats: required parameter is NULL (bridge, stats_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int me_fep_bridge_reset_stats(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_reset_stats: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_reset_", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(me_fep_stats_t));
    bridge->running_avg_fe = 0.0f;
    bridge->running_count = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float me_fep_bridge_get_free_energy(me_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_fr", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->metrics.free_energy;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float me_fep_bridge_get_mirroring_error(me_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_mi", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    float me = bridge->metrics.mirroring_error;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return me;
}

float me_fep_bridge_get_prediction_error(me_fep_bridge_t* bridge) {
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_pr", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    float pe = bridge->metrics.prediction_error;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return pe;
}

/*=============================================================================
 * STATE QUERY FUNCTIONS
 *===========================================================================*/

me_fep_state_t me_fep_bridge_get_state(me_fep_bridge_t* bridge) {
    if (!bridge) return ME_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_st", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    me_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool me_fep_bridge_is_degraded(me_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_is_deg", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == ME_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

bool me_fep_bridge_is_high_resonance(me_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_is_hig", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    bool high_res = bridge->metrics.high_resonance_state;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return high_res;
}

const char* me_fep_state_name(me_fep_state_t state) {
    switch (state) {
        case ME_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case ME_FEP_STATE_IDLE:          return "idle";
        case ME_FEP_STATE_ACTIVE:        return "active";
        case ME_FEP_STATE_DEGRADED:      return "degraded";
        case ME_FEP_STATE_ERROR:         return "error";
        default:                          return "unknown";
    }
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int me_fep_bridge_set_high_fe_callback(
    me_fep_bridge_t* bridge,
    me_fep_high_fe_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_set_high_fe_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_set_hi", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int me_fep_bridge_set_surprise_callback(
    me_fep_bridge_t* bridge,
    me_fep_surprise_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_set_surprise_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_set_su", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->surprise_callback = callback;
    bridge->surprise_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int me_fep_bridge_set_metrics_callback(
    me_fep_bridge_t* bridge,
    me_fep_metrics_callback_t callback,
    void* user_data
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_set_metrics_callback: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_set_me", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->metrics_callback = callback;
    bridge->metrics_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int me_fep_bridge_set_config(
    me_fep_bridge_t* bridge,
    const me_fep_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_set_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int me_fep_bridge_get_config(
    const me_fep_bridge_t* bridge,
    me_fep_config_t* config_out
) {
    if (!bridge || !config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "me_fep_bridge_get_config: required parameter is NULL (bridge, config_out)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_empathy_fep_bridge_heartbeat("mirror_empat_me_fep_bridge_get_co", 0.0f);


    nimcp_mutex_lock(((me_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((me_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void mirror_empathy_fep_bridge_set_instance_health_agent(me_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "mirror_empathy_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int mirror_empathy_fep_bridge_training_begin(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_empathy_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_fep_bridge_training_begin", 0.0f);
    return 0;
}

int mirror_empathy_fep_bridge_training_end(me_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_fep_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_empathy_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_fep_bridge_training_end", 1.0f);
    return 0;
}

int mirror_empathy_fep_bridge_training_step(me_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_empathy_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_empathy_fep_bridge_heartbeat_instance(bridge->health_agent, "mirror_empathy_fep_bridge_training_step", progress);
    return 0;
}
