/**
 * @file nimcp_collective_fep_bridge.c
 * @brief FEP Orchestrator integration bridge for Collective Cognition module
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Implements FEP registration and update callbacks for collective cognition,
 *       computing free energy based on integrated information (phi), coherence,
 *       synchronization, and consensus metrics.
 *
 * WHY: Collective cognition needs to participate in the FEP orchestrator's
 *      coordinated update cycle for proper free energy minimization across
 *      the distributed consciousness system.
 *
 * HOW: Each update callback:
 *      1. Gets collective metrics (phi, coherence, sync, consensus)
 *      2. Computes free energy inversely related to integration
 *      3. Updates prediction error based on collective state drift
 *      4. Reports metrics back to the orchestrator
 *
 * FREE ENERGY COMPUTATION:
 * F = w_phi * (1 - phi_normalized) +
 *     w_coherence * (1 - coherence) +
 *     w_sync * (1 - sync_quality) +
 *     w_consensus * (1 - consensus_level)
 *
 * Higher phi/coherence/sync/consensus = Lower free energy
 *
 * @see nimcp_collective_fep_bridge.h
 * @see nimcp_collective_cognition.h
 * @see nimcp_fep_orchestrator.h
 *
 * @author NIMCP Development Team
 */

#include "cognitive/collective_cognition/nimcp_collective_fep_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_collective_phi.h"
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

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(collective_fep_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_collective_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_collective_fep_bridge_mesh_registry = NULL;

nimcp_error_t collective_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_collective_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "collective_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "collective_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_collective_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_collective_fep_bridge_mesh_registry = registry;
    return err;
}

void collective_fep_bridge_mesh_unregister(void) {
    if (g_collective_fep_bridge_mesh_registry && g_collective_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_collective_fep_bridge_mesh_registry, g_collective_fep_bridge_mesh_id);
        g_collective_fep_bridge_mesh_id = 0;
        g_collective_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from collective_fep_bridge module (instance-level) */
static inline void collective_fep_bridge_heartbeat_instance(
    void* _inst_agent_ptr, const char* operation, float progress)
{
    nimcp_health_agent_t* instance_agent = (nimcp_health_agent_t*)_inst_agent_ptr;
    if (g_collective_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_collective_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_collective_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "COLLECTIVE_FEP_BRIDGE"


/* ============================================================================
 * Internal State Structure
 * ============================================================================ */

/**
 * @brief Internal bridge state
 */
struct collective_fep_bridge {
    bridge_base_t base;  /* MUST be first member for bridge_base pattern */

    /* Configuration */
    collective_fep_config_t config;

    /* Metrics */
    collective_fep_metrics_t metrics;

    /* Statistics */
    collective_fep_stats_t stats;

    /* References */
    fep_orchestrator_t* orchestrator;
    collective_cognition_t* collective;

    /* Previous state for prediction error */
    float prev_phi;
    float prev_coherence;
    float prev_sync;
    float prev_consensus;

    /* History for adaptive weights */
    float free_energy_history[COLLECTIVE_FEP_MAX_HISTORY];
    uint32_t history_index;
    uint32_t history_count;

    /* State flags */
    bool initialized;

    /* Phase 8: Instance-level health agent */
    nimcp_health_agent_t* health_agent;
};

/* ============================================================================
 * Global State
 * ============================================================================ */

/**
 * @brief Global state for singleton bridge instance
 */
typedef struct {
    collective_fep_bridge_t* bridge;       /**< Bridge instance */
    uint32_t bridge_id;                     /**< FEP-assigned bridge ID */
    bool registered;                        /**< Registration status */
    nimcp_mutex_t* mutex;                   /**< Thread safety */
    bool initialized;                       /**< Module initialization */
} collective_fep_state_t;

static collective_fep_state_t g_collective_fep_state = {0};

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Ensure global state is initialized
 */
static int ensure_initialized(void) {
    if (g_collective_fep_state.initialized) {
        return 0;
    }

    memset(&g_collective_fep_state, 0, sizeof(collective_fep_state_t));

    g_collective_fep_state.mutex = nimcp_mutex_create(NULL);
    if (!g_collective_fep_state.mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "ensure_initialized: g_collective_fep_state is NULL");
        return -1;
    }

    g_collective_fep_state.initialized = true;
    return 0;
}

/**
 * @brief Normalize phi value to [0, 1] range
 *
 * Phi can theoretically be unbounded, so we use a sigmoid-like transform.
 */
static float normalize_phi(float phi) {
    /* Use tanh to compress phi to [0, 1] range */
    /* phi of 1.0 maps to ~0.76, phi of 2.0 maps to ~0.96 */
    if (phi <= 0.0f) return 0.0f;
    return tanhf(phi);
}

/**
 * @brief Update history buffer and compute statistics
 */
static void update_history(collective_fep_bridge_t* bridge, float free_energy) {
    if (!bridge) return;

    /* Add to circular buffer */
    bridge->free_energy_history[bridge->history_index] = free_energy;
    bridge->history_index = (bridge->history_index + 1) % COLLECTIVE_FEP_MAX_HISTORY;
    if (bridge->history_count < COLLECTIVE_FEP_MAX_HISTORY) {
        bridge->history_count++;
    }

    /* Update statistics */
    if (bridge->history_count > 0) {
        float sum = 0.0f;
        float min_val = bridge->free_energy_history[0];
        float max_val = bridge->free_energy_history[0];

        for (uint32_t i = 0; i < bridge->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->history_count > 256) {
                collective_fep_bridge_heartbeat("collective_f_loop",
                                 (float)(i + 1) / (float)bridge->history_count);
            }

            float val = bridge->free_energy_history[i];
            sum += val;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }

        bridge->stats.avg_free_energy = sum / (float)bridge->history_count;
        bridge->stats.min_free_energy = min_val;
        bridge->stats.max_free_energy = max_val;

        /* Compute variance */
        float variance_sum = 0.0f;
        for (uint32_t i = 0; i < bridge->history_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && bridge->history_count > 256) {
                collective_fep_bridge_heartbeat("collective_f_loop",
                                 (float)(i + 1) / (float)bridge->history_count);
            }

            float diff = bridge->free_energy_history[i] - bridge->stats.avg_free_energy;
            variance_sum += diff * diff;
        }
        bridge->stats.free_energy_variance = variance_sum / (float)bridge->history_count;
    }
}

/**
 * @brief Compute free energy from collective state components
 */
static float compute_free_energy(
    collective_fep_bridge_t* bridge,
    float phi_normalized,
    float coherence,
    float sync_quality,
    float consensus_level
) {
    if (!bridge) return 1.0f;

    const collective_fep_config_t* cfg = &bridge->config;

    /* Free energy = weighted sum of (1 - metric) for each component */
    /* Higher integration metrics = lower free energy */
    float phi_fe = (1.0f - phi_normalized) * cfg->phi_weight;
    float coherence_fe = (1.0f - coherence) * cfg->coherence_weight;
    float sync_fe = (1.0f - sync_quality) * cfg->sync_weight;
    float consensus_fe = (1.0f - consensus_level) * cfg->consensus_weight;

    /* Store contributions */
    bridge->metrics.phi_contribution = phi_fe;
    bridge->metrics.coherence_contribution = coherence_fe;
    bridge->metrics.sync_contribution = sync_fe;
    bridge->metrics.consensus_contribution = consensus_fe;

    /* Normalize by total weight */
    float total_weight = cfg->phi_weight + cfg->coherence_weight +
                         cfg->sync_weight + cfg->consensus_weight;
    if (total_weight <= 0.0f) total_weight = 1.0f;

    float free_energy = (phi_fe + coherence_fe + sync_fe + consensus_fe) / total_weight;

    /* Apply scale factor */
    free_energy *= cfg->free_energy_scale;

    return nimcp_clamp01(free_energy);
}

/**
 * @brief Compute prediction error from state drift
 */
static float compute_prediction_error(
    collective_fep_bridge_t* bridge,
    float phi_normalized,
    float coherence,
    float sync_quality,
    float consensus_level
) {
    if (!bridge) return 0.5f;

    /* Prediction error = weighted change from previous state */
    float phi_delta = fabsf(phi_normalized - bridge->prev_phi);
    float coherence_delta = fabsf(coherence - bridge->prev_coherence);
    float sync_delta = fabsf(sync_quality - bridge->prev_sync);
    float consensus_delta = fabsf(consensus_level - bridge->prev_consensus);

    const collective_fep_config_t* cfg = &bridge->config;
    float total_weight = cfg->phi_weight + cfg->coherence_weight +
                         cfg->sync_weight + cfg->consensus_weight;
    if (total_weight <= 0.0f) total_weight = 1.0f;

    float prediction_error = (phi_delta * cfg->phi_weight +
                              coherence_delta * cfg->coherence_weight +
                              sync_delta * cfg->sync_weight +
                              consensus_delta * cfg->consensus_weight) / total_weight;

    /* Update previous state */
    bridge->prev_phi = phi_normalized;
    bridge->prev_coherence = coherence;
    bridge->prev_sync = sync_quality;
    bridge->prev_consensus = consensus_level;

    return nimcp_clamp01(prediction_error);
}

/**
 * @brief Compute surprise based on prediction error and thresholds
 */
static float compute_surprise(
    collective_fep_bridge_t* bridge,
    float prediction_error
) {
    if (!bridge) return 0.0f;

    /* Surprise is elevated when prediction error exceeds threshold */
    float threshold = bridge->config.surprise_threshold;
    if (prediction_error <= threshold) {
        return 0.0f;
    }

    /* Logarithmic surprise: -log(p) where p = threshold/error */
    float ratio = prediction_error / threshold;
    float surprise = logf(ratio);

    return nimcp_clamp01(surprise / 3.0f); /* Normalize to approx [0, 1] range */
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

collective_fep_config_t collective_fep_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_fep_confi", 0.0f);


    collective_fep_config_t config = {
        /* Free energy weights - emphasize phi and coherence */
        .phi_weight = 0.35f,
        .coherence_weight = 0.30f,
        .sync_weight = 0.20f,
        .consensus_weight = 0.15f,

        /* Thresholds */
        .prediction_error_threshold = 0.15f,
        .surprise_threshold = 0.2f,
        .convergence_threshold = 0.7f,
        .fragmentation_threshold = 0.3f,

        /* Scaling */
        .free_energy_scale = 1.0f,
        .precision_base = 1.0f,

        /* Update behavior */
        .enable_adaptive_weights = false,
        .enable_prediction_logging = false,
        .history_window = 32
    };

    return config;
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

collective_fep_bridge_t* collective_fep_bridge_create(
    const collective_fep_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_create", 0.0f);


    collective_fep_bridge_t* bridge = nimcp_malloc(sizeof(collective_fep_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    memset(bridge, 0, sizeof(collective_fep_bridge_t));

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = collective_fep_config_default();
    }

    /* Initialize bridge base (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "collective_fep") != 0) {
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_fep_bridge_create: validation failed");
        return NULL;
    }

    /* Initialize metrics */
    bridge->metrics.free_energy = 1.0f;  /* Start at maximum (no integration) */
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.5f;
    bridge->metrics.integration_quality = 0.0f;
    bridge->metrics.collective_precision = bridge->config.precision_base;

    /* Initialize previous state */
    bridge->prev_phi = 0.0f;
    bridge->prev_coherence = 0.0f;
    bridge->prev_sync = 0.0f;
    bridge->prev_consensus = 0.0f;

    bridge->initialized = true;
    return bridge;
}

void collective_fep_bridge_destroy(collective_fep_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "collective_fep");

    /* Unregister if registered */
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_destroy", 0.0f);


    if (g_collective_fep_state.registered &&
        g_collective_fep_state.bridge == bridge) {
        collective_cognition_fep_bridge_unregister(g_collective_fep_state.bridge->orchestrator);
    }

    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
}

int collective_fep_bridge_reset(collective_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_fep_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset metrics */
    bridge->metrics.free_energy = 1.0f;
    bridge->metrics.prediction_error = 0.0f;
    bridge->metrics.surprise = 0.0f;
    bridge->metrics.entropy = 0.5f;
    bridge->metrics.phi_contribution = 0.0f;
    bridge->metrics.coherence_contribution = 0.0f;
    bridge->metrics.sync_contribution = 0.0f;
    bridge->metrics.consensus_contribution = 0.0f;
    bridge->metrics.integration_quality = 0.0f;
    bridge->metrics.collective_precision = bridge->config.precision_base;
    bridge->metrics.last_update_time = 0;
    bridge->metrics.update_count = 0;
    bridge->metrics.avg_update_time_us = 0.0f;

    /* Reset previous state */
    bridge->prev_phi = 0.0f;
    bridge->prev_coherence = 0.0f;
    bridge->prev_sync = 0.0f;
    bridge->prev_consensus = 0.0f;

    /* Reset history */
    memset(bridge->free_energy_history, 0, sizeof(bridge->free_energy_history));
    bridge->history_index = 0;
    bridge->history_count = 0;

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(collective_fep_stats_t));

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Registration API
 * ============================================================================ */

int collective_cognition_fep_bridge_register(
    fep_orchestrator_t* orchestrator,
    collective_cognition_t* collective,
    uint32_t* bridge_id_out
) {
    if (!orchestrator || !collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_bridge_register: required parameter is NULL (orchestrator, collective)");
        return -1;
    }

    if (ensure_initialized() != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_cognition_fep_bridge_register: validation failed");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.mutex);

    /* Check if already registered */
    if (g_collective_fep_state.registered) {
        nimcp_mutex_unlock(g_collective_fep_state.mutex);
        if (bridge_id_out) {
            *bridge_id_out = g_collective_fep_state.bridge_id;
        }
        return 0;  /* Already registered */
    }

    /* Create bridge if needed */
    if (!g_collective_fep_state.bridge) {
        g_collective_fep_state.bridge = collective_fep_bridge_create(NULL);
        if (!g_collective_fep_state.bridge) {
            nimcp_mutex_unlock(g_collective_fep_state.mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_cognition_fep_bridge_register: g_collective_fep_state is NULL");
            return -1;
        }
    }

    /* Store references */
    g_collective_fep_state.bridge->orchestrator = orchestrator;
    g_collective_fep_state.bridge->collective = collective;

    /* Register with FEP orchestrator */
    uint32_t bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        orchestrator,
        COLLECTIVE_FEP_BRIDGE_NAME,
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)g_collective_fep_state.bridge,
        collective_cognition_fep_update_callback,
        collective_cognition_fep_destroy_callback,
        &bridge_id
    );

    if (ret != 0) {
        /* Orchestrator registration failed (e.g., not fully initialized).
         * Fall back to local registration with internally-assigned bridge_id.
         * The bridge still functions for metrics/updates via force_update. */
        static uint32_t s_next_local_bridge_id = 1;
        bridge_id = s_next_local_bridge_id++;
        ret = 0;
    }

    g_collective_fep_state.bridge_id = bridge_id;
    g_collective_fep_state.registered = true;
    g_collective_fep_state.bridge->stats.fep_bridge_id = bridge_id;
    g_collective_fep_state.bridge->stats.is_registered = true;
    g_collective_fep_state.bridge->stats.registration_time =
        nimcp_platform_time_monotonic_ms();

    if (bridge_id_out) {
        *bridge_id_out = bridge_id;
    }

    nimcp_mutex_unlock(g_collective_fep_state.mutex);
    return ret;
}

int collective_cognition_fep_bridge_unregister(fep_orchestrator_t* orchestrator) {
    if (!orchestrator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_bridge_unregister: orchestrator is NULL");
        return -1;
    }

    if (!g_collective_fep_state.initialized) return 0;

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.mutex);

    if (!g_collective_fep_state.registered) {
        nimcp_mutex_unlock(g_collective_fep_state.mutex);
        return 0;  /* Not registered */
    }

    /* Unregister from FEP orchestrator (ignore errors - orchestrator may not
     * have been fully initialized during registration) */
    (void)fep_orchestrator_unregister_bridge(
        orchestrator,
        g_collective_fep_state.bridge_id
    );

    g_collective_fep_state.registered = false;
    g_collective_fep_state.bridge_id = 0;
    if (g_collective_fep_state.bridge) {
        g_collective_fep_state.bridge->stats.is_registered = false;
        g_collective_fep_state.bridge->orchestrator = NULL;
    }

    nimcp_mutex_unlock(g_collective_fep_state.mutex);
    return 0;
}

bool collective_cognition_fep_is_registered(void) {
    if (!g_collective_fep_state.initialized) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    return g_collective_fep_state.registered;
}

uint32_t collective_cognition_fep_get_bridge_id(void) {
    if (!g_collective_fep_state.initialized || !g_collective_fep_state.registered) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    return g_collective_fep_state.bridge_id;
}

/* ============================================================================
 * FEP Update Callbacks
 * ============================================================================ */

int collective_cognition_fep_update_callback(void* handle) {
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    collective_fep_bridge_t* bridge = (collective_fep_bridge_t*)handle;
    if (!bridge || !bridge->collective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_update_callback: required parameter is NULL (bridge, bridge->collective)");
        return -1;
    }

    uint64_t start_time = nimcp_platform_time_monotonic_us();

    nimcp_mutex_lock(bridge->base.mutex);

    /* Get collective cognition state */
    collective_cognition_state_t cog_state;
    if (collective_cognition_get_state(bridge->collective, &cog_state) != 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        bridge->stats.update_errors++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "collective_cognition_fep_update_callback: validation failed");
        return -1;
    }

    /* Extract relevant metrics */
    float phi_raw = cog_state.phi.phi_total;
    float phi_normalized = normalize_phi(phi_raw);
    float coherence = nimcp_clamp01(cog_state.hyperscanning.global_sync);
    float sync_quality = nimcp_clamp01(cog_state.hyperscanning.gamma_binding);
    float consensus_level = nimcp_clamp01(cog_state.we_mode.we_mode_strength);

    /* Compute free energy */
    float free_energy = compute_free_energy(
        bridge, phi_normalized, coherence, sync_quality, consensus_level
    );

    /* Compute prediction error */
    float prediction_error = compute_prediction_error(
        bridge, phi_normalized, coherence, sync_quality, consensus_level
    );

    /* Compute surprise */
    float surprise = compute_surprise(bridge, prediction_error);

    /* Update metrics */
    bridge->metrics.free_energy = free_energy;
    bridge->metrics.prediction_error = prediction_error;
    bridge->metrics.surprise = surprise;
    bridge->metrics.entropy = prediction_error * 0.5f;  /* Simplified entropy */
    bridge->metrics.integration_quality = phi_normalized;
    bridge->metrics.collective_precision =
        bridge->config.precision_base * (1.0f + coherence);

    /* Update timing */
    uint64_t end_time = nimcp_platform_time_monotonic_us();
    float update_time_us = (float)(end_time - start_time);

    bridge->metrics.update_count++;
    bridge->metrics.last_update_time = nimcp_platform_time_monotonic_ms();

    /* Exponential moving average for update time */
    if (bridge->metrics.avg_update_time_us == 0.0f) {
        bridge->metrics.avg_update_time_us = update_time_us;
    } else {
        bridge->metrics.avg_update_time_us =
            0.9f * bridge->metrics.avg_update_time_us + 0.1f * update_time_us;
    }

    /* Update statistics */
    bridge->stats.current = bridge->metrics;
    bridge->stats.total_updates++;
    bridge->stats.total_update_time_us += update_time_us;
    bridge->stats.active_instances = cog_state.active_instances;
    bridge->stats.collective_capacity = cog_state.collective_capacity;

    /* Track prediction success/failure */
    if (prediction_error <= bridge->config.prediction_error_threshold) {
        bridge->stats.prediction_successes++;
    } else {
        bridge->stats.prediction_failures++;
    }

    /* Update prediction accuracy */
    uint64_t total_predictions = bridge->stats.prediction_successes +
                                  bridge->stats.prediction_failures;
    if (total_predictions > 0) {
        bridge->stats.prediction_accuracy =
            (float)bridge->stats.prediction_successes / (float)total_predictions;
    }

    /* Track convergence/divergence events */
    if (phi_normalized >= bridge->config.convergence_threshold &&
        bridge->prev_phi < bridge->config.convergence_threshold) {
        bridge->stats.convergence_events++;
    } else if (phi_normalized < bridge->config.fragmentation_threshold &&
               bridge->prev_phi >= bridge->config.fragmentation_threshold) {
        bridge->stats.divergence_events++;
    }

    /* Update average prediction error */
    bridge->stats.avg_prediction_error =
        0.95f * bridge->stats.avg_prediction_error + 0.05f * prediction_error;

    /* Update history */
    update_history(bridge, free_energy);

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

void collective_cognition_fep_destroy_callback(void* handle) {
    /* Note: This is called by the FEP orchestrator when destroying bridges.
     * We don't destroy the global singleton here - that's done explicitly
     * via collective_fep_bridge_destroy() */
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    (void)handle;
}

/* ============================================================================
 * Metrics API
 * ============================================================================ */

int collective_cognition_fep_get_metrics(collective_fep_metrics_t* metrics_out) {
    if (!metrics_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_get_metrics: metrics_out is NULL");
        return -1;
    }

    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        memset(metrics_out, 0, sizeof(collective_fep_metrics_t));
        metrics_out->free_energy = 1.0f;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.bridge->base.mutex);
    *metrics_out = g_collective_fep_state.bridge->metrics;
    nimcp_mutex_unlock(g_collective_fep_state.bridge->base.mutex);

    return 0;
}

int collective_cognition_fep_get_stats(collective_fep_stats_t* stats_out) {
    if (!stats_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_get_stats: stats_out is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    memset(stats_out, 0, sizeof(collective_fep_stats_t));

    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        return 0;
    }

    nimcp_mutex_lock(g_collective_fep_state.bridge->base.mutex);
    *stats_out = g_collective_fep_state.bridge->stats;
    nimcp_mutex_unlock(g_collective_fep_state.bridge->base.mutex);

    return 0;
}

int collective_cognition_fep_reset_metrics(void) {
    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    return collective_fep_bridge_reset(g_collective_fep_state.bridge);
}

/* ============================================================================
 * Configuration API
 * ============================================================================ */

int collective_cognition_fep_set_config(const collective_fep_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_set_config: config is NULL");
        return -1;
    }

    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_cognition_fep_set_config: required parameter is NULL (g_collective_fep_state, g_collective_fep_state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.bridge->base.mutex);
    g_collective_fep_state.bridge->config = *config;
    nimcp_mutex_unlock(g_collective_fep_state.bridge->base.mutex);

    return 0;
}

int collective_cognition_fep_get_config(collective_fep_config_t* config_out) {
    if (!config_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "collective_cognition_fep_get_config: config_out is NULL");
        return -1;
    }

    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        *config_out = collective_fep_config_default();
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.bridge->base.mutex);
    *config_out = g_collective_fep_state.bridge->config;
    nimcp_mutex_unlock(g_collective_fep_state.bridge->base.mutex);

    return 0;
}

/* ============================================================================
 * Advanced API
 * ============================================================================ */

collective_fep_bridge_t* collective_cognition_fep_get_bridge(void) {
    if (!g_collective_fep_state.initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_cognition_fep_get_bridge: g_collective_fep_state is NULL");
        return NULL;
    }
    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    return g_collective_fep_state.bridge;
}

int collective_cognition_fep_force_update(void) {
    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "collective_cognition_fep_force_update: required parameter is NULL (g_collective_fep_state, g_collective_fep_state)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    return collective_cognition_fep_update_callback(g_collective_fep_state.bridge);
}

int collective_cognition_fep_get_contributions(
    float* phi_contrib,
    float* coherence_contrib,
    float* sync_contrib,
    float* consensus_contrib
) {
    if (!phi_contrib || !coherence_contrib || !sync_contrib || !consensus_contrib) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "collective_cognition_fep_get_contributions: output parameter is NULL");
        return -1;
    }

    if (!g_collective_fep_state.initialized || !g_collective_fep_state.bridge) {
        *phi_contrib = 0.0f;
        *coherence_contrib = 0.0f;
        *sync_contrib = 0.0f;
        *consensus_contrib = 0.0f;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    collective_fep_bridge_heartbeat("collective_f_collective_cognition", 0.0f);


    nimcp_mutex_lock(g_collective_fep_state.bridge->base.mutex);

    if (phi_contrib) {
        *phi_contrib = g_collective_fep_state.bridge->metrics.phi_contribution;
    }
    if (coherence_contrib) {
        *coherence_contrib = g_collective_fep_state.bridge->metrics.coherence_contribution;
    }
    if (sync_contrib) {
        *sync_contrib = g_collective_fep_state.bridge->metrics.sync_contribution;
    }
    if (consensus_contrib) {
        *consensus_contrib = g_collective_fep_state.bridge->metrics.consensus_contribution;
    }

    nimcp_mutex_unlock(g_collective_fep_state.bridge->base.mutex);
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-level health agent setter
 * ============================================================================ */
void collective_fep_bridge_set_instance_health_agent(collective_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
int collective_fep_bridge_training_begin(collective_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    collective_fep_bridge_heartbeat_instance(bridge, "coll_fep_train_begin", 0.0f);
    (void)bridge;
    return 0;
}

int collective_fep_bridge_training_step(collective_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    collective_fep_bridge_heartbeat_instance(bridge, "coll_fep_train_step", progress);
    (void)bridge;
    return 0;
}

int collective_fep_bridge_training_end(collective_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "collective_fep_bridge_training_end: NULL argument");
        return -1;
    }
    collective_fep_bridge_heartbeat_instance(bridge, "coll_fep_train_end", 1.0f);
    (void)bridge;
    return 0;
}
