/**
 * @file nimcp_surprise_attention_bridge.c
 * @brief Bridge between Surprise Amplifier and Attention System
 * @version 1.0.0
 * @date 2026-01-27
 *
 * WHAT: Bidirectional: surprise → attention boost/shift; attention → sensitivity
 * WHY:  Surprise redirects attention (exogenous capture); attention state
 *       modulates surprise sensitivity (attended vs unattended channels)
 * HOW:  High surprise → boost/shift attention; attention focus → sensitivity map
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

#include "cognitive/salience/nimcp_surprise_attention_bridge.h"
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
#include "constants/nimcp_threshold_constants.h"
#include "utils/math/nimcp_math_helpers.h"

/* Health agent: using pre-existing custom implementation */
static nimcp_health_agent_t* g_surprise_att_bridge_health_agent = NULL;


/* Stub heartbeat for migration compatibility */
static inline void surprise_att_bridge_heartbeat(const char* op, float progress) {
    (void)op; (void)progress;
}
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_surprise_att_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_surprise_att_bridge_mesh_registry = NULL;

nimcp_error_t surprise_att_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_surprise_att_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "surprise_att_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "surprise_att_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_surprise_att_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_surprise_att_bridge_mesh_registry = registry;
    return err;
}

void surprise_att_bridge_mesh_unregister(void) {
    if (g_surprise_att_bridge_mesh_registry && g_surprise_att_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_surprise_att_bridge_mesh_registry, g_surprise_att_bridge_mesh_id);
        g_surprise_att_bridge_mesh_id = 0;
        g_surprise_att_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from surprise_att_bridge module (instance-level) */
static inline void surprise_att_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_surprise_att_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_surprise_att_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_surprise_att_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_att_bridge {
    surprise_att_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    struct nimcp_attention_system* attention;

    /* State */
    surprise_att_effects_t effects;
    surprise_att_stats_t stats;

    /* Per-channel sensitivity */
    surprise_att_channel_t channels[SURPRISE_ATT_MAX_CHANNEL_SENSITIVITY];
    uint32_t channel_count;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent (per-instance) */
    nimcp_health_agent_t* health_agent;

    bool initialized;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_att_config_t surprise_att_bridge_default_config(void) {
    surprise_att_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.boost_gain = SURPRISE_ATT_DEFAULT_BOOST_GAIN;
    cfg.shift_threshold = SURPRISE_ATT_DEFAULT_SHIFT_THRESHOLD;
    cfg.sensitivity_floor = SURPRISE_ATT_DEFAULT_SENSITIVITY_FLOOR;
    cfg.attention_decay_rate = SURPRISE_ATT_DEFAULT_DECAY_RATE;
    cfg.enable_attention_boost = true;
    cfg.enable_attention_shift = true;
    cfg.enable_sensitivity_mod = true;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_att_bridge_t* surprise_att_bridge_create(
    const surprise_att_config_t* config)
{
    surprise_att_bridge_t* bridge = (surprise_att_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_att_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_ATT_ERROR_NO_MEMORY,
                           sizeof(surprise_att_bridge_t),
                           "surprise_att_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_att_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_att_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_ATT_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_att_bridge mutex allocation failed (%zu bytes)",
                           sizeof(nimcp_mutex_t));
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    bridge->effects.current_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-attention bridge (boost_gain=%.1f, "
                           "shift_thresh=%.2f)",
                           bridge->config.boost_gain,
                           bridge->config.shift_threshold);
    }

    return bridge;
}

void surprise_att_bridge_destroy(surprise_att_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-attention bridge "
                           "(boosts=%lu, shifts=%lu)",
                           (unsigned long)bridge->stats.attention_boosts,
                           (unsigned long)bridge->stats.attention_shifts);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_att_bridge_reset(surprise_att_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_att_bridge_heartbeat("reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->effects, 0, sizeof(bridge->effects));
    bridge->effects.current_sensitivity = NIMCP_SENSITIVITY_DEFAULT;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->channels, 0, sizeof(bridge->channels));
    bridge->channel_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_att_bridge_connect_amplifier(
    surprise_att_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-attention bridge connected to amplifier");
    }
    return 0;
}

int surprise_att_bridge_connect_attention(
    surprise_att_bridge_t* bridge,
    struct nimcp_attention_system* attention)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in connect_attention");
    NIMCP_CHECK_THROW_IMMUNE(attention != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL attention in connect_attention");

    nimcp_mutex_lock(bridge->mutex);
    bridge->attention = attention;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-attention bridge connected to attention system");
    }
    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_att_apply_boost(
    surprise_att_bridge_t* bridge,
    float surprise_magnitude,
    float attention_boost)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in apply_boost");

    surprise_att_bridge_heartbeat("apply_boost", 0.0f);

    if (!bridge->config.enable_attention_boost) {
        return 0;
    }

    float mag = nimcp_clampf(surprise_magnitude, 0.0f, 1.0f);
    float boost = nimcp_clampf(attention_boost, 0.0f, 5.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Apply gain */
    float effective_boost = boost * bridge->config.boost_gain;
    effective_boost = nimcp_clampf(effective_boost, 0.0f, 5.0f);

    /* Set current boost (max of existing and new) */
    if (effective_boost > bridge->effects.current_attention_boost) {
        bridge->effects.current_attention_boost = effective_boost;
    }
    bridge->effects.boost_remaining = bridge->effects.current_attention_boost;

    bridge->stats.attention_boosts++;

    /* Running avg */
    float n = (float)bridge->stats.attention_boosts;
    bridge->stats.avg_boost_magnitude =
        bridge->stats.avg_boost_magnitude * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + effective_boost / (fabsf(n) > 1e-7f ? n : 1e-7f);
    if (effective_boost > bridge->stats.max_boost_magnitude) {
        bridge->stats.max_boost_magnitude = effective_boost;
    }

    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Attention boost applied: surprise=%.3f, "
                            "effective_boost=%.3f",
                            mag, effective_boost);
    }

    return 0;
}

int surprise_att_request_shift(
    surprise_att_bridge_t* bridge,
    float surprise_magnitude,
    uint32_t source_module)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in request_shift");

    surprise_att_bridge_heartbeat("request_shift", 0.0f);

    if (!bridge->config.enable_attention_shift) {
        return 0;
    }

    float mag = nimcp_clampf(surprise_magnitude, 0.0f, 1.0f);

    /* Only shift on high surprise */
    if (mag < bridge->config.shift_threshold) {
        return 0;
    }

    nimcp_mutex_lock(bridge->mutex);

    bridge->effects.shift_active = true;
    bridge->effects.shift_target = source_module;
    bridge->stats.attention_shifts++;

    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Attention shift requested: surprise=%.3f → module=0x%04X",
                           mag, source_module);
    }

    return 0;
}

float surprise_att_get_sensitivity(const surprise_att_bridge_t* bridge) {
    if (!bridge) return 1.0f;
    return bridge->effects.current_sensitivity;
}

int surprise_att_set_channel_sensitivity(
    surprise_att_bridge_t* bridge,
    uint32_t channel_id,
    float sensitivity)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in set_channel_sensitivity");

    float sens = nimcp_clampf(sensitivity, 0.0f, 1.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Find existing channel or add new */
    bool found = false;
    for (uint32_t i = 0; i < bridge->channel_count; i++) {
        if (bridge->channels[i].channel_id == channel_id) {
            bridge->channels[i].sensitivity = sens;
            found = true;
            break;
        }
    }

    if (!found && bridge->channel_count < SURPRISE_ATT_MAX_CHANNEL_SENSITIVITY) {
        bridge->channels[bridge->channel_count].channel_id = channel_id;
        bridge->channels[bridge->channel_count].sensitivity = sens;
        bridge->channel_count++;
    }

    nimcp_mutex_unlock(bridge->mutex);
    return 0;
}

/* ============================================================================
 * Update API
 * ============================================================================ */

int surprise_att_bridge_update(surprise_att_bridge_t* bridge, float dt_seconds) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in update");

    if (dt_seconds <= 0.0f) return 0;

    surprise_att_bridge_heartbeat("update", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    /* Decay attention boost */
    if (bridge->effects.current_attention_boost > 0.0f) {
        float decay = powf(bridge->config.attention_decay_rate, dt_seconds);
        bridge->effects.current_attention_boost *= decay;
        bridge->effects.boost_remaining = bridge->effects.current_attention_boost;

        if (bridge->effects.current_attention_boost < 0.001f) {
            bridge->effects.current_attention_boost = 0.0f;
            bridge->effects.boost_remaining = 0.0f;
        }
    }

    /* Clear attention shift after one update cycle */
    if (bridge->effects.shift_active) {
        bridge->effects.shift_active = false;
    }

    /* Update sensitivity based on amplifier state */
    if (bridge->config.enable_sensitivity_mod && bridge->amplifier) {
        float surprise_level = surprise_amplifier_get_current_level(bridge->amplifier);
        /*
         * Attention is currently redirected to surprise source,
         * so surprise sensitivity on attended channel is high,
         * overall baseline sensitivity reduces slightly (resource allocation)
         */
        float sensitivity = 1.0f - 0.3f * surprise_level;
        sensitivity = nimcp_clampf(sensitivity, bridge->config.sensitivity_floor, 1.0f);
        bridge->effects.current_sensitivity = sensitivity;
        bridge->stats.sensitivity_updates++;

        /* Running avg */
        float n = (float)bridge->stats.sensitivity_updates;
        bridge->stats.avg_sensitivity =
            bridge->stats.avg_sensitivity * ((n - 1.0f) / (fabsf(n) > 1e-7f ? n : 1e-7f)) + sensitivity / (fabsf(n) > 1e-7f ? n : 1e-7f);
    }

    bridge->stats.total_updates++;

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_att_bridge_get_effects(
    const surprise_att_bridge_t* bridge,
    surprise_att_effects_t* effects_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in get_effects");
    NIMCP_CHECK_THROW_IMMUNE(effects_out != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL effects_out in get_effects");

    *effects_out = bridge->effects;
    return 0;
}

int surprise_att_bridge_get_stats(
    const surprise_att_bridge_t* bridge,
    surprise_att_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

/* ============================================================================
 * Health Agent Integration (per-instance)
 * ============================================================================ */

int surprise_att_bridge_set_health_agent(
    surprise_att_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_ATT_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_att_bridge_set_instance_health_agent(surprise_att_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "surprise_att_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_att_bridge_training_begin(surprise_att_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_att_bridge_training_begin: NULL argument");
        return -1;
    }
    surprise_att_bridge_heartbeat_instance(bridge->health_agent, "surprise_att_bridge_training_begin", 0.0f);
    return 0;
}

int surprise_att_bridge_training_end(surprise_att_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_att_bridge_training_end: NULL argument");
        return -1;
    }
    surprise_att_bridge_heartbeat_instance(bridge->health_agent, "surprise_att_bridge_training_end", 1.0f);
    return 0;
}

int surprise_att_bridge_training_step(surprise_att_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_att_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_att_bridge_heartbeat_instance(bridge->health_agent, "surprise_att_bridge_training_step", progress);
    return 0;
}
