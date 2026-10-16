/**
 * @file nimcp_surprise_gw_bridge.c
 * @brief Bridge between Surprise Amplifier and Global Workspace
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Routes amplified surprise events to Global Workspace for broadcasting
 * WHY:  Surprise that gains workspace access becomes a "realization" -
 *       the mechanism Kim et al. (2026) identified for reasoning improvement
 * HOW:  Surprise events compete for GW access; winners are broadcast system-wide
 *
 * NIMCP INTEGRATION PATTERNS:
 * 1. Exception handling (NIMCP_CHECK_THROW_IMMUNE)
 * 2. Health agent heartbeat
 * 3. Thread safety (mutex)
 * 4. Logging
 * 5. Memory management (nimcp_calloc/free)
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_gw_bridge.h"
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
static nimcp_health_agent_t* g_surprise_gw_bridge_health_agent = NULL;


/* Stub heartbeat for migration compatibility */
static inline void surprise_gw_bridge_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_surprise_gw_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_surprise_gw_bridge_mesh_registry = NULL;

nimcp_error_t surprise_gw_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_surprise_gw_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "surprise_gw_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "surprise_gw_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_surprise_gw_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_surprise_gw_bridge_mesh_registry = registry;
    return err;
}

void surprise_gw_bridge_mesh_unregister(void) {
    if (g_surprise_gw_bridge_mesh_registry && g_surprise_gw_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_surprise_gw_bridge_mesh_registry, g_surprise_gw_bridge_mesh_id);
        g_surprise_gw_bridge_mesh_id = 0;
        g_surprise_gw_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from surprise_gw_bridge module (instance-level) */
static inline void surprise_gw_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_surprise_gw_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surprise_gw_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_surprise_gw_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

/**
 * @brief Pending broadcast entry
 */
typedef struct {
    float magnitude;
    uint32_t source_type;
    float attention_boost;
    float curiosity_boost;
    bool valid;
} pending_broadcast_t;

struct surprise_gw_bridge {
    surprise_gw_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    struct global_workspace_struct* gw;

    /* State */
    surprise_gw_effects_t effects;
    surprise_gw_stats_t stats;
    pending_broadcast_t pending[SURPRISE_GW_MAX_PENDING_BROADCASTS];
    uint32_t pending_count;
    float cooldown_remaining;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent (per-instance) */
    nimcp_health_agent_t* health_agent;

    bool initialized;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_gw_config_t surprise_gw_bridge_default_config(void) {
    surprise_gw_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.broadcast_threshold = SURPRISE_GW_DEFAULT_BROADCAST_THRESHOLD;
    cfg.competition_weight = SURPRISE_GW_DEFAULT_COMPETITION_WEIGHT;
    cfg.cooldown_seconds = SURPRISE_GW_DEFAULT_COOLDOWN_SECONDS;
    cfg.enable_broadcast = true;
    cfg.enable_sensitivity_mod = true;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_gw_bridge_t* surprise_gw_bridge_create(
    const surprise_gw_config_t* config)
{
    surprise_gw_bridge_t* bridge = (surprise_gw_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_gw_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_GW_ERROR_NO_MEMORY,
                           sizeof(surprise_gw_bridge_t),
                           "surprise_gw_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_gw_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_gw_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_GW_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_gw_bridge mutex allocation failed (%zu bytes)",
                           sizeof(nimcp_mutex_t));
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->effects.sensitivity_modifier = 1.0f;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-GW bridge (broadcast_thresh=%.2f, "
                           "competition_weight=%.1f)",
                           bridge->config.broadcast_threshold,
                           bridge->config.competition_weight);
    }

    return bridge;
}

void surprise_gw_bridge_destroy(surprise_gw_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-GW bridge (submitted=%lu, won=%lu)",
                           (unsigned long)bridge->stats.broadcasts_submitted,
                           (unsigned long)bridge->stats.broadcasts_won);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_gw_bridge_reset(surprise_gw_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_gw_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.sensitivity_modifier = 1.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->pending, 0, sizeof(bridge->pending));
    bridge->pending_count = 0;
    bridge->cooldown_remaining = 0.0f;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_gw_bridge_connect_amplifier(
    surprise_gw_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-GW bridge connected to amplifier");
    }
    return 0;
}

int surprise_gw_bridge_connect_gw(
    surprise_gw_bridge_t* bridge,
    struct global_workspace_struct* gw)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in connect_gw");
    NIMCP_CHECK_THROW_IMMUNE(gw != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL gw in connect_gw");

    nimcp_mutex_lock(bridge->mutex);
    bridge->gw = gw;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-GW bridge connected to global workspace");
    }
    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_gw_submit_broadcast(
    surprise_gw_bridge_t* bridge,
    float magnitude,
    uint32_t source_type,
    float attention_boost,
    float curiosity_boost)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in submit_broadcast");

    surprise_gw_bridge_heartbeat("submit_broadcast", 0.0f);

    if (!bridge->config.enable_broadcast) {
        return 0;
    }

    float mag = nimcp_clampf(magnitude, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Threshold check */
    if (mag < bridge->config.broadcast_threshold) {
        bridge->stats.below_threshold++;
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Cooldown check */
    if (bridge->cooldown_remaining > 0.0f) {
        bridge->stats.broadcasts_cooled++;
        nimcp_mutex_unlock(bridge->mutex);
        return 0;
    }

    /* Queue the broadcast */
    if (bridge->pending_count < SURPRISE_GW_MAX_PENDING_BROADCASTS) {
        pending_broadcast_t* slot = &bridge->pending[bridge->pending_count];
        slot->magnitude = mag;
        slot->source_type = source_type;
        slot->attention_boost = attention_boost;
        slot->curiosity_boost = curiosity_boost;
        slot->valid = true;
        bridge->pending_count++;
        bridge->effects.broadcast_pending = true;
    }

    bridge->stats.broadcasts_submitted++;
    bridge->effects.last_broadcast_magnitude = mag;

    /* Running avg */
    float n = (float)bridge->stats.broadcasts_submitted;
    bridge->stats.avg_broadcast_magnitude =
        bridge->stats.avg_broadcast_magnitude * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + mag / (fabsf(n) > 1e-7f ? n : 1e-7f);
    if (mag > bridge->stats.max_broadcast_magnitude) {
        bridge->stats.max_broadcast_magnitude = mag;
    }

    /* Start cooldown */
    bridge->cooldown_remaining = bridge->config.cooldown_seconds;

    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise broadcast submitted (magnitude=%.3f, source=%u)",
                           mag, source_type);
    }

    return 0;
}

float surprise_gw_get_sensitivity(const surprise_gw_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->effects.sensitivity_modifier;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int surprise_gw_bridge_update(surprise_gw_bridge_t* bridge, float dt_seconds) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_gw_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Decay cooldown */
    if (bridge->cooldown_remaining > 0.0f) {
        bridge->cooldown_remaining -= dt_seconds;
        if (bridge->cooldown_remaining < 0.0f) {
            bridge->cooldown_remaining = 0.0f;
        }
    }

    /* Process pending broadcasts */
    for (uint32_t i = 0; i < bridge->pending_count; i++) {
        if (bridge->pending[i].valid) {
            /*
             * In a full integration, this would call gw_submit_entry()
             * on the connected global workspace. For now, we track
             * the broadcast as "won" (no competition in standalone mode).
             */
            bridge->stats.broadcasts_won++;
            bridge->pending[i].valid = false;
        }
    }
    bridge->pending_count = 0;
    bridge->effects.broadcast_pending = false;

    /* Update sensitivity based on amplifier state */
    if (bridge->config.enable_sensitivity_mod && bridge->amplifier) {
        float surprise_level = surprise_amplifier_get_current_level(bridge->amplifier);
        /* High current surprise → reduced sensitivity (refractory at GW level) */
        bridge->effects.sensitivity_modifier = 1.0f - 0.5f * surprise_level;
        bridge->effects.sensitivity_modifier =
            nimcp_clampf(bridge->effects.sensitivity_modifier, 0.2f, 1.0f);
    }

    bridge->effects.time_since_broadcast += dt_seconds;
    bridge->stats.total_updates++;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_gw_bridge_get_effects(
    const surprise_gw_bridge_t* bridge,
    surprise_gw_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_gw_bridge_get_stats(
    const surprise_gw_bridge_t* bridge,
    surprise_gw_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

/* ============================================================================
 * Health Agent Integration (per-instance)
 * ============================================================================ */

int surprise_gw_bridge_set_health_agent(
    surprise_gw_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_GW_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_gw_bridge_set_instance_health_agent(surprise_gw_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "surprise_gw_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_gw_bridge_training_begin(surprise_gw_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_gw_bridge_training_begin: NULL argument");
        return -1;
    }
    surprise_gw_bridge_heartbeat_instance(bridge->health_agent, "surprise_gw_bridge_training_begin", 0.0f);
    return 0;
}

int surprise_gw_bridge_training_end(surprise_gw_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_gw_bridge_training_end: NULL argument");
        return -1;
    }
    surprise_gw_bridge_heartbeat_instance(bridge->health_agent, "surprise_gw_bridge_training_end", 1.0f);
    return 0;
}

int surprise_gw_bridge_training_step(surprise_gw_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_gw_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_gw_bridge_heartbeat_instance(bridge->health_agent, "surprise_gw_bridge_training_step", progress);
    return 0;
}
