/**
 * @file nimcp_jepa_fep_bridge.c
 * @brief JEPA - FEP Orchestrator Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for JEPA predictor
 * WHY:  Enable coordinated free energy minimization through embedding predictions
 * HOW:  Compute free energy from embedding error, representation quality, collapse
 *
 * @author NIMCP Development Team
 */

#include "cognitive/jepa/nimcp_jepa_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(jepa_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_jepa_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_jepa_fep_bridge_mesh_registry = NULL;

nimcp_error_t jepa_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_jepa_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "jepa_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "jepa_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_jepa_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_jepa_fep_bridge_mesh_registry = registry;
    return err;
}

void jepa_fep_bridge_mesh_unregister(void) {
    if (g_jepa_fep_bridge_mesh_registry && g_jepa_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_jepa_fep_bridge_mesh_registry, g_jepa_fep_bridge_mesh_id);
        g_jepa_fep_bridge_mesh_id = 0;
        g_jepa_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from jepa_fep_bridge module (instance + global) */
static inline void jepa_fep_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_jepa_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_jepa_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_jepa_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "JEPA_FEP_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct jepa_fep_bridge {
    bridge_base_t base;  /* MUST be first member */

    /* Configuration */
    jepa_fep_config_t config;

    /* State */
    jepa_fep_state_t state;

    /* References */
    fep_orchestrator_t* orchestrator;
    jepa_predictor_t* predictor;
    uint32_t bridge_id;
    bool registered;

    /* Current metrics */
    float current_free_energy;
    float embedding_prediction_error;
    float representation_quality;
    float collapse_severity;

    /* Previous state for change detection */
    float prev_free_energy;
    float prev_prediction_error;
    float prev_quality;

    /* Running averages */
    float running_avg_error;
    float running_avg_quality;
    uint64_t sample_count;

    /* Statistics */
    jepa_fep_stats_t stats;

    /* Timing */
    uint64_t last_update_time_ms;
    uint64_t total_update_time_us;

    /* Callbacks */
    jepa_fep_high_fe_callback_t high_fe_callback;
    void* high_fe_user_data;
    jepa_fep_collapse_callback_t collapse_callback;
    void* collapse_user_data;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

static inline uint64_t get_time_ms(void) {
    return nimcp_platform_time_monotonic_ms();
}

static inline uint64_t get_time_us(void) {
    return nimcp_platform_time_monotonic_us();
}

/**
 * @brief Compute free energy from JEPA metrics
 *
 * Free energy model for JEPA:
 * FE = baseline + embedding_error_contrib + quality_contrib + collapse_contrib
 *
 * Where:
 * - embedding_error_contrib = normalized_error * embedding_weight
 * - quality_contrib = (1 - quality) * representation_weight
 * - collapse_contrib = collapse_severity * collapse_penalty
 */
static void compute_free_energy(jepa_fep_bridge_t* bridge) {
    const jepa_fep_config_t* cfg = &bridge->config;

    /* Normalize embedding prediction error */
    float normalized_error = nimcp_clampf(
        bridge->embedding_prediction_error / JEPA_FEP_MAX_PRED_ERROR,
        0.0f, 1.0f
    );

    /* Embedding error contribution: higher error = more free energy */
    float error_contrib = normalized_error * cfg->embedding_prediction_error_weight;

    /* Representation quality contribution: lower quality = more free energy */
    float quality_contrib = (1.0f - bridge->representation_quality) *
                            (1.0f - cfg->embedding_prediction_error_weight -
                             cfg->representation_collapse_penalty);

    /* Collapse penalty contribution */
    float collapse_contrib = bridge->collapse_severity * cfg->representation_collapse_penalty;

    /* Total free energy */
    float total_fe = JEPA_FEP_BASELINE_FREE_ENERGY +
                     (error_contrib + quality_contrib + collapse_contrib) *
                     cfg->free_energy_weight;

    bridge->current_free_energy = nimcp_clampf(total_fe, 0.0f, JEPA_FEP_MAX_FREE_ENERGY);

    /* Update cumulative statistics */
    bridge->stats.total_free_energy_contribution += bridge->current_free_energy;

    if (bridge->current_free_energy > bridge->stats.peak_free_energy) {
        bridge->stats.peak_free_energy = bridge->current_free_energy;
    }
}

/**
 * @brief Check for representation collapse (state update only, no callback)
 */
static void check_collapse(jepa_fep_bridge_t* bridge) {
    if (!bridge->config.enable_collapse_detection) {
        return;
    }

    /* Collapse severity is inverse of representation quality below threshold */
    if (bridge->representation_quality < bridge->config.collapse_detection_threshold) {
        float threshold = bridge->config.collapse_detection_threshold;
        bridge->collapse_severity = (threshold - bridge->representation_quality) / threshold;
        bridge->collapse_severity = nimcp_clampf(bridge->collapse_severity, 0.0f, 1.0f);

        bridge->stats.collapse_detections++;
        /* Callback invoked outside mutex by caller */
    } else {
        bridge->collapse_severity = 0.0f;
    }
}

/**
 * @brief Check and update state for callbacks (no callback invocation)
 */
static void check_callbacks(jepa_fep_bridge_t* bridge) {
    const jepa_fep_config_t* cfg = &bridge->config;

    /* Check for high free energy */
    if (bridge->current_free_energy > cfg->high_free_energy_threshold) {
        if (bridge->state != JEPA_FEP_STATE_DEGRADED) {
            bridge->state = JEPA_FEP_STATE_DEGRADED;
            /* Callback invoked outside mutex by caller */
        }
    } else if (bridge->state == JEPA_FEP_STATE_DEGRADED) {
        bridge->state = JEPA_FEP_STATE_ACTIVE;
    }
}

/**
 * @brief Update statistics
 */
static void update_stats(jepa_fep_bridge_t* bridge, uint64_t update_time_us) {
    bridge->stats.total_updates++;
    bridge->total_update_time_us += update_time_us;

    if (bridge->stats.total_updates > 0) {
        bridge->stats.avg_update_time_us =
            (float)bridge->total_update_time_us / (float)bridge->stats.total_updates;
    }

    /* Track minimum representation quality */
    if (bridge->representation_quality < bridge->stats.min_representation_quality ||
        bridge->stats.min_representation_quality < 0.0f) {
        bridge->stats.min_representation_quality = bridge->representation_quality;
    }
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

jepa_fep_config_t jepa_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_jepa_fep_config_defa", 0.0f);


    jepa_fep_config_t config;
    memset(&config, 0, sizeof(config));

    /* Weighting parameters */
    config.free_energy_weight = 1.0f;
    config.embedding_prediction_error_weight = JEPA_FEP_EMBEDDING_ERROR_WEIGHT;
    config.representation_collapse_penalty = JEPA_FEP_COLLAPSE_PENALTY_WEIGHT;

    /* Thresholds */
    config.high_free_energy_threshold = 1.5f;
    config.collapse_detection_threshold = 0.2f;
    config.prediction_quality_threshold = 0.7f;

    /* Behavior */
    config.enable_logging = false;
    config.update_interval_ms = JEPA_FEP_DEFAULT_UPDATE_MS;
    config.enable_adaptive_weights = true;
    config.enable_collapse_detection = true;

    return config;
}

jepa_fep_bridge_t* jepa_fep_bridge_create(const jepa_fep_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_create", 0.0f);


    jepa_fep_bridge_t* bridge = nimcp_calloc(1, sizeof(jepa_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = jepa_fep_config_default();
    }

    /* Initialize base (includes mutex creation) */
    if (bridge_base_init(&bridge->base, 0, "jepa_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "jepa_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->state = JEPA_FEP_STATE_IDLE;
    bridge->registered = false;
    bridge->bridge_id = 0;

    /* Initialize metrics */
    bridge->current_free_energy = JEPA_FEP_BASELINE_FREE_ENERGY;
    bridge->embedding_prediction_error = 0.0f;
    bridge->representation_quality = 1.0f;  /* Start with good quality */
    bridge->collapse_severity = 0.0f;

    /* Initialize previous state */
    bridge->prev_free_energy = JEPA_FEP_BASELINE_FREE_ENERGY;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_quality = 1.0f;

    /* Initialize running averages */
    bridge->running_avg_error = 0.0f;
    bridge->running_avg_quality = 1.0f;
    bridge->sample_count = 0;

    /* Initialize statistics */
    bridge->stats.min_representation_quality = 1.0f;

    NIMCP_LOGGING_INFO("Created %s bridge", "jepa_fep");
    return bridge;
}

void jepa_fep_bridge_destroy(jepa_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "jepa_fep");

    /* Unregister if still registered */
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_destroy", 0.0f);


    if (bridge->registered) {
        jepa_fep_bridge_unregister(bridge);
    }

    /* Cleanup base (includes mutex destruction) */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int jepa_fep_bridge_reset(jepa_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_reset", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    bridge->current_free_energy = JEPA_FEP_BASELINE_FREE_ENERGY;
    bridge->embedding_prediction_error = 0.0f;
    bridge->representation_quality = 1.0f;
    bridge->collapse_severity = 0.0f;

    /* Reset previous state */
    bridge->prev_free_energy = JEPA_FEP_BASELINE_FREE_ENERGY;
    bridge->prev_prediction_error = 0.0f;
    bridge->prev_quality = 1.0f;

    /* Reset running averages */
    bridge->running_avg_error = 0.0f;
    bridge->running_avg_quality = 1.0f;
    bridge->sample_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(jepa_fep_stats_t));
    bridge->stats.min_representation_quality = 1.0f;

    /* Reset timing */
    bridge->total_update_time_us = 0;
    bridge->last_update_time_ms = 0;

    bridge->state = JEPA_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * REGISTRATION FUNCTIONS
 *===========================================================================*/

int jepa_fep_bridge_register(
    jepa_fep_bridge_t* bridge,
    fep_orchestrator_t* orchestrator,
    jepa_predictor_t* predictor,
    uint32_t* bridge_id_out
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_register", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && orchestrator, -1, "bridge or orchestrator is NULL");

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
    bridge->predictor = predictor;

    /* Register with FEP orchestrator */
    uint32_t assigned_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        "jepa_predictor",
        FEP_BRIDGE_CATEGORY_JEPA,  /* JEPA timescale (25ms) */
        (fep_bridge_handle_t)bridge,
        jepa_fep_update_callback,
        jepa_fep_destroy_callback,
        &assigned_id
    );

    if (ret != 0) {
        bridge->orchestrator = NULL;
        bridge->predictor = NULL;
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_fep_bridge_register: validation failed");
        return -1;
    }

    bridge->bridge_id = assigned_id;
    bridge->registered = true;
    bridge->state = JEPA_FEP_STATE_ACTIVE;

    if (bridge_id_out) {
        *bridge_id_out = assigned_id;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_fep_bridge_unregister(jepa_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_unregister", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

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
    bridge->predictor = NULL;
    bridge->state = JEPA_FEP_STATE_IDLE;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

bool jepa_fep_bridge_is_registered(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_is_registered", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    bool registered = bridge->registered;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return registered;
}

uint32_t jepa_fep_bridge_get_id(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_fep_bridge_get_id: bridge is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_id", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    uint32_t id = bridge->registered ? bridge->bridge_id : 0;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return id;
}

/*=============================================================================
 * FEP UPDATE CALLBACK
 *===========================================================================*/

int jepa_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_jepa_fep_update_call", 0.0f);


    jepa_fep_bridge_t* bridge = (jepa_fep_bridge_t*)handle;
    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge handle is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state for delta computation */
    bridge->prev_free_energy = bridge->current_free_energy;
    bridge->prev_prediction_error = bridge->embedding_prediction_error;
    bridge->prev_quality = bridge->representation_quality;

    /* If we have a predictor, get stats from it */
    if (bridge->predictor) {
        jepa_predictor_stats_t predictor_stats;
        if (jepa_predictor_get_stats(bridge->predictor, &predictor_stats) == 0) {
            /* Update embedding prediction error from predictor's average loss */
            bridge->embedding_prediction_error = predictor_stats.avg_loss;
            bridge->stats.avg_embedding_error = predictor_stats.avg_loss;
            bridge->stats.embedding_predictions = predictor_stats.predictions_made;
        }
    }

    /* Check for representation collapse */
    check_collapse(bridge);

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Update timing */
    bridge->last_update_time_ms = get_time_ms();

    /* Check state transitions (no callbacks under mutex) */
    check_callbacks(bridge);

    /* Snapshot callback state under mutex */
    jepa_fep_collapse_callback_t collapse_cb = bridge->collapse_callback;
    void* collapse_ud = bridge->collapse_user_data;
    float collapse_sev = bridge->collapse_severity;
    bool do_collapse_cb = (collapse_cb != NULL && collapse_sev > 0.0f &&
                           bridge->config.enable_collapse_detection);

    jepa_fep_high_fe_callback_t high_fe_cb = bridge->high_fe_callback;
    void* high_fe_ud = bridge->high_fe_user_data;
    float cur_fe = bridge->current_free_energy;
    bool do_high_fe_cb = (high_fe_cb != NULL &&
                          bridge->state == JEPA_FEP_STATE_DEGRADED &&
                          cur_fe > bridge->config.high_free_energy_threshold);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_collapse_cb) {
        collapse_cb(bridge, collapse_sev, collapse_ud);
    }
    if (do_high_fe_cb) {
        high_fe_cb(bridge, cur_fe, high_fe_ud);
    }

    return 0;
}

void jepa_fep_destroy_callback(void* handle) {
    /* No-op: Bridge is destroyed separately via jepa_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_jepa_fep_destroy_cal", 0.0f);


    (void)handle;
}

/*=============================================================================
 * MANUAL UPDATE OPERATIONS
 *===========================================================================*/

int jepa_fep_bridge_update(jepa_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_update", 0.0f);


    return jepa_fep_update_callback(bridge);
}

int jepa_fep_bridge_force_update(jepa_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_force_update", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_us = get_time_us();

    /* Store previous state */
    bridge->prev_free_energy = bridge->current_free_energy;
    bridge->prev_prediction_error = bridge->embedding_prediction_error;
    bridge->prev_quality = bridge->representation_quality;

    /* Check for representation collapse */
    check_collapse(bridge);

    /* Compute free energy from current metrics */
    compute_free_energy(bridge);

    /* Calculate update time */
    uint64_t end_us = get_time_us();
    uint64_t update_time_us = end_us - start_us;

    /* Update statistics */
    update_stats(bridge, update_time_us);

    /* Update timing */
    bridge->last_update_time_ms = get_time_ms();

    /* Check state transitions (no callbacks under mutex) */
    check_callbacks(bridge);

    /* Snapshot callback state under mutex */
    jepa_fep_collapse_callback_t collapse_cb = bridge->collapse_callback;
    void* collapse_ud = bridge->collapse_user_data;
    float collapse_sev = bridge->collapse_severity;
    bool do_collapse_cb = (collapse_cb != NULL && collapse_sev > 0.0f &&
                           bridge->config.enable_collapse_detection);

    jepa_fep_high_fe_callback_t high_fe_cb = bridge->high_fe_callback;
    void* high_fe_ud = bridge->high_fe_user_data;
    float cur_fe = bridge->current_free_energy;
    bool do_high_fe_cb = (high_fe_cb != NULL &&
                          bridge->state == JEPA_FEP_STATE_DEGRADED &&
                          cur_fe > bridge->config.high_free_energy_threshold);

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Invoke callbacks outside mutex to prevent deadlock */
    if (do_collapse_cb) {
        collapse_cb(bridge, collapse_sev, collapse_ud);
    }
    if (do_high_fe_cb) {
        high_fe_cb(bridge, cur_fe, high_fe_ud);
    }

    return 0;
}

/*=============================================================================
 * STATISTICS AND ACCESSORS
 *===========================================================================*/

int jepa_fep_bridge_get_stats(
    const jepa_fep_bridge_t* bridge,
    jepa_fep_stats_t* stats_out
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && stats_out, -1, "bridge or stats_out is NULL");

    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    *stats_out = bridge->stats;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

int jepa_fep_bridge_reset_stats(jepa_fep_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_reset_stats", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(jepa_fep_stats_t));
    bridge->stats.min_representation_quality = 1.0f;
    bridge->total_update_time_us = 0;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float jepa_fep_bridge_get_free_energy_contribution(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_fep_bridge_get_free_energy_contribution: bridge is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_free_energy_cont", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    float fe = bridge->current_free_energy;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return fe;
}

float jepa_fep_bridge_get_embedding_prediction_error(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_fep_bridge_get_embedding_prediction_error: bridge is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_embedding_predic", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    float error = bridge->embedding_prediction_error;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return error;
}

float jepa_fep_bridge_get_representation_quality(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "jepa_fep_bridge_get_representation_quality: bridge is NULL");
        return -1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_representation_q", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    float quality = bridge->representation_quality;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return quality;
}

jepa_fep_state_t jepa_fep_bridge_get_state(jepa_fep_bridge_t* bridge) {
    if (!bridge) return JEPA_FEP_STATE_ERROR;

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_state", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    jepa_fep_state_t state = bridge->state;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return state;
}

bool jepa_fep_bridge_is_degraded(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_is_degraded", 0.0f);


    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    bool degraded = (bridge->state == JEPA_FEP_STATE_DEGRADED);
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return degraded;
}

/*=============================================================================
 * INPUT FUNCTIONS
 *===========================================================================*/

int jepa_fep_bridge_record_prediction_error(
    jepa_fep_bridge_t* bridge,
    float prediction_error
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_record_prediction_er", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(prediction_error >= 0.0f, -1, "prediction_error must be non-negative");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update running average */
    bridge->sample_count++;
    float alpha = 1.0f / (float)bridge->sample_count;
    bridge->running_avg_error = bridge->running_avg_error * (1.0f - alpha) +
                                 prediction_error * alpha;

    bridge->embedding_prediction_error = prediction_error;
    bridge->stats.embedding_predictions++;
    bridge->stats.avg_embedding_error = bridge->running_avg_error;

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

int jepa_fep_bridge_record_representation_quality(
    jepa_fep_bridge_t* bridge,
    float quality
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_record_representatio", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");
    NIMCP_FEP_CHECK_THROW(quality >= 0.0f && quality <= 1.0f, -1, "quality must be in range [0,1]");

    nimcp_mutex_lock(bridge->base.mutex);

    /* Update running average */
    if (bridge->sample_count == 0) {
        bridge->sample_count = 1;
    }
    float alpha = 1.0f / (float)bridge->sample_count;
    bridge->running_avg_quality = bridge->running_avg_quality * (1.0f - alpha) +
                                   quality * alpha;

    bridge->representation_quality = quality;
    bridge->stats.representation_updates++;

    /* Track minimum quality */
    if (quality < bridge->stats.min_representation_quality) {
        bridge->stats.min_representation_quality = quality;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

int jepa_fep_bridge_set_high_fe_callback(
    jepa_fep_bridge_t* bridge,
    jepa_fep_high_fe_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_set_high_fe_callback", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->high_fe_callback = callback;
    bridge->high_fe_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_fep_bridge_set_collapse_callback(
    jepa_fep_bridge_t* bridge,
    jepa_fep_collapse_callback_t callback,
    void* user_data
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_set_collapse_callbac", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge, -1, "bridge is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->collapse_callback = callback;
    bridge->collapse_user_data = user_data;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

int jepa_fep_bridge_set_config(
    jepa_fep_bridge_t* bridge,
    const jepa_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_set_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && config, -1, "bridge or config is NULL");

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->config = *config;
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int jepa_fep_bridge_get_config(
    const jepa_fep_bridge_t* bridge,
    jepa_fep_config_t* config_out
) {
    /* Phase 8: Heartbeat at operation start */
    jepa_fep_bridge_heartbeat("jepa_fep_bri_get_config", 0.0f);


    NIMCP_FEP_CHECK_THROW(bridge && config_out, -1, "bridge or config_out is NULL");

    nimcp_mutex_lock(((jepa_fep_bridge_t*)bridge)->base.mutex);
    *config_out = bridge->config;
    nimcp_mutex_unlock(((jepa_fep_bridge_t*)bridge)->base.mutex);

    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* jepa_fep_state_name(jepa_fep_state_t state) {
    switch (state) {
        case JEPA_FEP_STATE_UNINITIALIZED: return "uninitialized";
        case JEPA_FEP_STATE_IDLE:          return "idle";
        case JEPA_FEP_STATE_ACTIVE:        return "active";
        case JEPA_FEP_STATE_DEGRADED:      return "degraded";
        case JEPA_FEP_STATE_ERROR:         return "error";
        default:                            return "unknown";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent + Training Lifecycle
 * ============================================================================ */

void jepa_fep_bridge_set_instance_health_agent(jepa_fep_bridge_t* bridge,
                                                nimcp_health_agent_t* agent) {
    if (!bridge) return;
    bridge->health_agent = agent;
}

int jepa_fep_bridge_training_begin(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    jepa_fep_bridge_heartbeat_instance(bridge, "training_begin", 0.0f);
    (void)bridge;
    return 0;
}

int jepa_fep_bridge_training_end(jepa_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_fep_bridge_training_end: NULL argument");
        return -1;
    }
    jepa_fep_bridge_heartbeat_instance(bridge, "training_end", 1.0f);
    (void)bridge;
    return 0;
}

int jepa_fep_bridge_training_step(jepa_fep_bridge_t* bridge, uint32_t step) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "jepa_fep_bridge_training_step: NULL argument");
        return -1;
    }
    float progress = (step % 100) / 100.0f;
    jepa_fep_bridge_heartbeat_instance(bridge, "training_step", progress);
    (void)bridge;
    return 0;
}
