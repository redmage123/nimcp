/**
 * @file nimcp_surprise_fep_bridge.c
 * @brief Bridge between Surprise Amplifier and Free Energy Principle system
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional integration: FEP PE → surprise amplifier; surprise → FEP precision
 * WHY:  FEP prediction errors are the primary driver of surprise signals;
 *       high surprise modulates FEP precision (attention = precision weighting)
 * HOW:  Forwards PE/KL to amplifier; reads surprise level to modulate precision
 *
 * NIMCP INTEGRATION PATTERNS:
 * 1. Exception handling (NIMCP_FEP_CHECK_THROW)
 * 2. Health agent heartbeat
 * 3. Thread safety (mutex)
 * 4. Logging
 * 5. Memory management (nimcp_calloc/free)
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_fep_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_surprise_fep_bridge_health_agent = NULL;


/* Stub heartbeat for migration compatibility */
static inline void surprise_fep_bridge_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_surprise_fep_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_surprise_fep_bridge_mesh_registry = NULL;

nimcp_error_t surprise_fep_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return -1;
    if (g_surprise_fep_bridge_mesh_id != 0) return 0;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "surprise_fep_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "surprise_fep_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_surprise_fep_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_surprise_fep_bridge_mesh_registry = registry;
    return err;
}

void surprise_fep_bridge_mesh_unregister(void) {
    if (g_surprise_fep_bridge_mesh_registry && g_surprise_fep_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_surprise_fep_bridge_mesh_registry, g_surprise_fep_bridge_mesh_id);
        g_surprise_fep_bridge_mesh_id = 0;
        g_surprise_fep_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from surprise_fep_bridge module (instance-level) */
static inline void surprise_fep_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_surprise_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surprise_fep_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_surprise_fep_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_fep_bridge {
    surprise_fep_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* fep_system;

    /* State */
    surprise_fep_effects_t effects;
    surprise_fep_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent (per-instance) */
    nimcp_health_agent_t* health_agent;

    bool initialized;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_fep_config_t surprise_fep_bridge_default_config(void) {
    surprise_fep_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.precision_gain = SURPRISE_FEP_DEFAULT_PRECISION_GAIN;
    cfg.pe_weight = SURPRISE_FEP_DEFAULT_PE_WEIGHT;
    cfg.bayesian_weight = SURPRISE_FEP_DEFAULT_BAYESIAN_WEIGHT;
    cfg.precision_floor = SURPRISE_FEP_DEFAULT_PRECISION_FLOOR;
    cfg.precision_ceiling = SURPRISE_FEP_DEFAULT_PRECISION_CEILING;
    cfg.pe_threshold = SURPRISE_FEP_DEFAULT_PE_THRESHOLD;
    cfg.enable_precision_modulation = true;
    cfg.enable_pe_forwarding = true;
    cfg.enable_bayesian_forwarding = true;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_fep_bridge_t* surprise_fep_bridge_create(
    const surprise_fep_config_t* config)
{
    surprise_fep_bridge_t* bridge = (surprise_fep_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_fep_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_FEP_ERROR_NO_MEMORY,
                           sizeof(surprise_fep_bridge_t),
                           "surprise_fep_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_fep_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_fep_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_FEP_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_fep_bridge mutex allocation failed (%zu bytes)",
                           sizeof(nimcp_mutex_t));
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->effects.current_precision_boost = 1.0f;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-FEP bridge (pe_thresh=%.2f, prec_gain=%.1f)",
                           bridge->config.pe_threshold, bridge->config.precision_gain);
    }

    return bridge;
}

void surprise_fep_bridge_destroy(surprise_fep_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-FEP bridge (forwarded=%lu PE, %lu Bayesian)",
                           (unsigned long)bridge->stats.pe_events_forwarded,
                           (unsigned long)bridge->stats.bayesian_events_forwarded);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_fep_bridge_reset(surprise_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_fep_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.current_precision_boost = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_fep_bridge_connect_amplifier(
    surprise_fep_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_FEP_CHECK_THROW(amp != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-FEP bridge connected to amplifier");
    }
    return 0;
}

int surprise_fep_bridge_connect_fep(
    surprise_fep_bridge_t* bridge,
    void* fep)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in connect_fep");
    NIMCP_FEP_CHECK_THROW(fep != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL fep in connect_fep");

    nimcp_mutex_lock(bridge->mutex);
    bridge->fep_system = fep;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-FEP bridge connected to FEP system");
    }
    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_fep_forward_pe(
    surprise_fep_bridge_t* bridge,
    float prediction_error,
    uint32_t source_module)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in forward_pe");

    surprise_fep_bridge_heartbeat("forward_pe", 0.0f);

    if (!bridge->config.enable_pe_forwarding) {
        return 0;
    }

    float pe = nimcp_clampf(prediction_error, 0.0f, 1.0f);

    /* Threshold check */
    if (pe < bridge->config.pe_threshold) {
        nimcp_mutex_lock(bridge->mutex);
        bridge->stats.pe_below_threshold++;
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Forward to amplifier if connected */
    nimcp_mutex_lock(bridge->mutex);
    if (bridge->amplifier) {
        float weighted = pe * bridge->config.pe_weight;
        int rc = surprise_amplifier_on_prediction_error(
            bridge->amplifier, weighted, source_module);
        if (rc == 0) {
            bridge->stats.pe_events_forwarded++;
            bridge->effects.last_pe_forwarded = weighted;

            /* Running average */
            float n = (float)bridge->stats.pe_events_forwarded;
            bridge->stats.avg_pe_forwarded =
                bridge->stats.avg_pe_forwarded * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + weighted / (fabsf(n) > 1e-7f ? n : 1e-7f);
        }
    }
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_fep_forward_bayesian(
    surprise_fep_bridge_t* bridge,
    float kl_divergence,
    uint32_t source_module)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in forward_bayesian");

    surprise_fep_bridge_heartbeat("forward_bayesian", 0.0f);

    if (!bridge->config.enable_bayesian_forwarding) {
        return 0;
    }

    if (kl_divergence < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_SURPRISE_FEP_ERROR_INVALID_PARAM,
                              "Negative KL divergence: %f", kl_divergence);
        return NIMCP_SURPRISE_FEP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(bridge->mutex);
    if (bridge->amplifier) {
        float weighted = kl_divergence * bridge->config.bayesian_weight;
        int rc = surprise_amplifier_on_bayesian_surprise(
            bridge->amplifier, weighted, source_module);
        if (rc == 0) {
            bridge->stats.bayesian_events_forwarded++;
            bridge->effects.last_bayesian_forwarded = weighted;
        }
    }
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_fep_modulate_precision(surprise_fep_bridge_t* bridge) {
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in modulate_precision");

    surprise_fep_bridge_heartbeat("modulate_precision", 0.0f);

    if (!bridge->config.enable_precision_modulation) {
        return 0;
    }

    nimcp_mutex_lock(bridge->mutex);

    /* Read current surprise level from amplifier */
    float surprise_level = 0.0f;
    if (bridge->amplifier) {
        surprise_level = surprise_amplifier_get_current_level(bridge->amplifier);
    }

    /* Compute precision boost: base + surprise * gain */
    float boost = 1.0f + surprise_level * bridge->config.precision_gain;
    boost = nimcp_clampf(boost, bridge->config.precision_floor,
                    bridge->config.precision_ceiling);

    bridge->effects.current_precision_boost = boost;
    bridge->effects.integrated_surprise = surprise_level;
    bridge->stats.precision_modulations++;

    /* Running avg */
    float n = (float)bridge->stats.precision_modulations;
    bridge->stats.avg_precision_boost =
        bridge->stats.avg_precision_boost * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + boost / (fabsf(n) > 1e-7f ? n : 1e-7f);
    if (boost > bridge->stats.max_precision_boost) {
        bridge->stats.max_precision_boost = boost;
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

float surprise_fep_get_precision_boost(surprise_fep_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->effects.current_precision_boost;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int surprise_fep_bridge_update(surprise_fep_bridge_t* bridge, float dt_seconds) {
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_fep_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Auto-modulate precision on update */
    if (bridge->config.enable_precision_modulation && bridge->amplifier) {
        float surprise_level = surprise_amplifier_get_current_level(bridge->amplifier);
        float boost = 1.0f + surprise_level * bridge->config.precision_gain;
        boost = nimcp_clampf(boost, bridge->config.precision_floor,
                        bridge->config.precision_ceiling);
        bridge->effects.current_precision_boost = boost;
        bridge->effects.integrated_surprise = surprise_level;
    }

    bridge->stats.total_updates++;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_fep_bridge_get_effects(
    const surprise_fep_bridge_t* bridge,
    surprise_fep_effects_t* effects_out)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_FEP_CHECK_THROW(effects_out != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_fep_bridge_get_stats(
    const surprise_fep_bridge_t* bridge,
    surprise_fep_stats_t* stats_out)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_FEP_CHECK_THROW(stats_out != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

/* ============================================================================
 * Health Agent Integration (per-instance)
 * ============================================================================ */

int surprise_fep_bridge_set_health_agent(
    surprise_fep_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_FEP_CHECK_THROW(bridge != NULL,
                             NIMCP_SURPRISE_FEP_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_fep_bridge_set_instance_health_agent(surprise_fep_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "surprise_fep_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_fep_bridge_training_begin(surprise_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_fep_bridge_training_begin: NULL argument");
        return -1;
    }
    surprise_fep_bridge_heartbeat_instance(bridge->health_agent, "surprise_fep_bridge_training_begin", 0.0f);
    return 0;
}

int surprise_fep_bridge_training_end(surprise_fep_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_fep_bridge_training_end: NULL argument");
        return -1;
    }
    surprise_fep_bridge_heartbeat_instance(bridge->health_agent, "surprise_fep_bridge_training_end", 1.0f);
    return 0;
}

int surprise_fep_bridge_training_step(surprise_fep_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_fep_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_fep_bridge_heartbeat_instance(bridge->health_agent, "surprise_fep_bridge_training_step", progress);
    return 0;
}
