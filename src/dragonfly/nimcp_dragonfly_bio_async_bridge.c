/**
 * @file nimcp_dragonfly_bio_async_bridge.c
 * @brief Dragonfly-to-Bio-Async Integration Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_bio_async_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_bio_async_bridge)

#define LOG_MODULE "DRAGONFLY_BIO_ASYNC_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

struct dragonfly_bio_future_s {
    dragonfly_async_op_t operation;
    float input[16];
    uint32_t input_size;
    dragonfly_async_result_t result;
    bool ready;
    float start_time_ms;
    float priority;
    struct dragonfly_bio_async_bridge_s* parent;  /* Back-pointer for cleanup */
};

struct dragonfly_bio_async_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    dragonfly_system_t* dragonfly;
    void* bio_async_system;
    dragonfly_bio_async_config_t config;

    /* Neuromodulator levels */
    float dopamine_level;
    float norepinephrine_level;
    float acetylcholine_level;
    float serotonin_level;

    /* Phase synchronization */
    dragonfly_phase_mode_t current_phase;
    float phase_angle;
    float coherence_level;

    /* Active futures */
    dragonfly_bio_future_t* futures[DRAGONFLY_BIO_MAX_FUTURES];
    uint32_t num_futures;

    /* Timing */
    float current_time_ms;

    /* Statistics */
    dragonfly_bio_async_stats_t stats;
    bool training_mode;
};

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_bio_async_bridge_default_config(dragonfly_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_bridge_default_config: config is NULL");
        return -1;
    }

    /* Neuromodulator settings */
    config->dopamine_decay_rate = 0.1f;
    config->norepinephrine_threshold = 0.5f;
    config->acetylcholine_focus_gain = 2.0f;

    /* Phase coupling */
    config->default_phase = DRAGONFLY_PHASE_GAMMA;
    config->phase_coherence_threshold = 0.7f;

    /* Priority */
    config->base_priority = DRAGONFLY_BIO_DEFAULT_PRIORITY;
    config->pursuit_priority_boost = 0.3f;
    config->intercept_priority_boost = 0.4f;

    /* Timeouts */
    config->default_timeout_ms = 100.0f;
    config->enable_timeout_escalation = true;

    return 0;
}

int dragonfly_bio_async_bridge_validate_config(const dragonfly_bio_async_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->dopamine_decay_rate < 0.0f) {
        return -1;
    }
    if (config->norepinephrine_threshold < 0.0f || config->norepinephrine_threshold > 1.0f) {
        return -1;
    }
    if (config->acetylcholine_focus_gain < 0.0f) {
        return -1;
    }
    if (config->phase_coherence_threshold < 0.0f || config->phase_coherence_threshold > 1.0f) {
        return -1;
    }
    if (config->base_priority < 0.0f || config->base_priority > 1.0f) {
        return -1;
    }
    if (config->default_timeout_ms <= 0.0f) {
        return -1;
    }
    if (config->default_phase > DRAGONFLY_PHASE_THETA) {
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_bio_async_bridge_t* dragonfly_bio_async_bridge_create(
    dragonfly_system_t* dragonfly,
    void* bio_async_system,
    const dragonfly_bio_async_config_t* config
) {
    dragonfly_bio_async_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_bio_async_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_bio_async_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        if (dragonfly_bio_async_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_bio_async_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_bio_async_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->bio_async_system = bio_async_system;

    /* Initialize neuromodulator levels to baseline */
    bridge->dopamine_level = 0.3f;
    bridge->norepinephrine_level = 0.2f;
    bridge->acetylcholine_level = 0.4f;
    bridge->serotonin_level = 0.5f;

    /* Initialize phase */
    bridge->current_phase = bridge->config.default_phase;
    bridge->phase_angle = 0.0f;
    bridge->coherence_level = 0.8f;

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_bio_async") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_bio_async_bridge_destroy(dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_bio_async");

    /* Destroy any active futures */
    for (uint32_t i = 0; i < bridge->num_futures; i++) {
        if (bridge->futures[i]) {
            nimcp_free(bridge->futures[i]);
        }
    }

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_bio_async_bridge_reset(dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Reset neuromodulator levels */
    bridge->dopamine_level = 0.3f;
    bridge->norepinephrine_level = 0.2f;
    bridge->acetylcholine_level = 0.4f;
    bridge->serotonin_level = 0.5f;

    /* Reset phase */
    bridge->current_phase = bridge->config.default_phase;
    bridge->phase_angle = 0.0f;
    bridge->coherence_level = 0.8f;

    /* Destroy futures */
    for (uint32_t i = 0; i < bridge->num_futures; i++) {
        if (bridge->futures[i]) {
            nimcp_free(bridge->futures[i]);
            bridge->futures[i] = NULL;
        }
    }
    bridge->num_futures = 0;

    bridge->current_time_ms = 0.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

//=============================================================================
// Async Operations
//=============================================================================

dragonfly_bio_future_t* dragonfly_bio_async_start(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_async_op_t operation,
    const float* input,
    uint32_t input_size
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_start: bridge is NULL");
        return NULL;
    }
    if (bridge->num_futures >= DRAGONFLY_BIO_MAX_FUTURES) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "dragonfly_bio_async_start: capacity exceeded");
        return NULL;
    }

    dragonfly_bio_future_t* future = nimcp_calloc(1, sizeof(dragonfly_bio_future_t));
    if (!future) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "dragonfly_bio_async_start: future is NULL");
        return NULL;
    }

    future->parent = bridge;
    future->operation = operation;
    future->input_size = (input_size > 16) ? 16 : input_size;
    if (input && future->input_size > 0) {
        memcpy(future->input, input, future->input_size * sizeof(float));
    }
    future->ready = false;
    future->start_time_ms = bridge->current_time_ms;

    /* Calculate priority based on operation type and neuromodulator state */
    future->priority = bridge->config.base_priority;
    if (operation == DRAGONFLY_ASYNC_INTERCEPT) {
        future->priority += bridge->config.intercept_priority_boost;
    } else if (operation == DRAGONFLY_ASYNC_TRACKING) {
        future->priority += bridge->config.pursuit_priority_boost;
    }

    /* Norepinephrine boosts priority */
    future->priority += bridge->norepinephrine_level * 0.2f;
    if (future->priority > 1.0f) future->priority = 1.0f;

    /* Add to futures list */
    bridge->futures[bridge->num_futures++] = future;
    bridge->stats.operations_started++;

    return future;
}

int dragonfly_bio_future_wait(
    dragonfly_bio_future_t* future,
    dragonfly_async_result_t* result,
    float timeout_ms
) {
    if (!future || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_future_wait: required parameter is NULL (future, result)");
        return -1;
    }

    /* Simulate completion - in real implementation this would block */
    (void)timeout_ms;

    /* Mark as ready and populate result */
    future->ready = true;
    future->result.operation = future->operation;
    future->result.success = true;
    future->result.confidence = 0.85f;
    future->result.latency_ms = 5.0f;
    future->result.result_size = 4;
    for (uint32_t i = 0; i < future->result.result_size; i++) {
        future->result.result_data[i] = (float)i * 0.1f;
    }

    *result = future->result;
    return 0;
}

bool dragonfly_bio_future_is_ready(const dragonfly_bio_future_t* future) {
    if (!future) {
        return false;
    }
    return future->ready;
}

float dragonfly_bio_future_get_confidence(const dragonfly_bio_future_t* future) {
    if (!future) return 0.0f;
    return future->result.confidence;
}

void dragonfly_bio_future_destroy(dragonfly_bio_future_t* future) {
    if (!future) return;

    /* Remove from parent's futures list to prevent double-free */
    if (future->parent) {
        dragonfly_bio_async_bridge_t* bridge = future->parent;
        for (uint32_t i = 0; i < bridge->num_futures; i++) {
            if (bridge->futures[i] == future) {
                /* Shift remaining futures down */
                for (uint32_t j = i; j < bridge->num_futures - 1; j++) {
                    bridge->futures[j] = bridge->futures[j + 1];
                }
                bridge->futures[bridge->num_futures - 1] = NULL;
                bridge->num_futures--;
                break;
            }
        }
    }

    nimcp_free(future);
}

//=============================================================================
// Neuromodulator Signaling
//=============================================================================

int dragonfly_bio_async_signal_reward(
    dragonfly_bio_async_bridge_t* bridge,
    float reward_magnitude
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_signal_reward: bridge is NULL");
        return -1;
    }

    /* Dopamine burst for successful interception */
    bridge->dopamine_level += reward_magnitude * 0.5f;
    if (bridge->dopamine_level > 1.0f) {
        bridge->dopamine_level = 1.0f;
    }

    return 0;
}

int dragonfly_bio_async_signal_alert(
    dragonfly_bio_async_bridge_t* bridge,
    float alert_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_signal_alert: bridge is NULL");
        return -1;
    }

    /* Norepinephrine for alert/arousal */
    bridge->norepinephrine_level = alert_level;
    if (bridge->norepinephrine_level > 1.0f) {
        bridge->norepinephrine_level = 1.0f;
    }

    /* High alert also slightly boosts dopamine anticipation */
    if (alert_level > 0.7f) {
        bridge->dopamine_level += 0.1f;
        if (bridge->dopamine_level > 1.0f) {
            bridge->dopamine_level = 1.0f;
        }
    }

    return 0;
}

int dragonfly_bio_async_signal_focus(
    dragonfly_bio_async_bridge_t* bridge,
    float focus_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_signal_focus: bridge is NULL");
        return -1;
    }

    /* Acetylcholine for attention focus */
    bridge->acetylcholine_level = focus_level * bridge->config.acetylcholine_focus_gain;
    if (bridge->acetylcholine_level > 1.0f) {
        bridge->acetylcholine_level = 1.0f;
    }

    return 0;
}

float dragonfly_bio_async_get_dopamine(const dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->dopamine_level;
}

float dragonfly_bio_async_get_norepinephrine(const dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->norepinephrine_level;
}

float dragonfly_bio_async_get_acetylcholine(const dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->acetylcholine_level;
}

//=============================================================================
// Phase Synchronization
//=============================================================================

int dragonfly_bio_async_set_phase_mode(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_phase_mode_t mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_set_phase_mode: bridge is NULL");
        return -1;
    }
    if (mode > DRAGONFLY_PHASE_THETA) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_bio_async_set_phase_mode: validation failed");
        return -1;
    }

    bridge->current_phase = mode;
    return 0;
}

float dragonfly_bio_async_get_coherence(const dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->coherence_level;
}

int dragonfly_bio_async_sync_futures(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_bio_future_t** futures,
    uint32_t num_futures,
    float coherence_threshold
) {
    if (!bridge || !futures || num_futures == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_sync_futures: required parameter is NULL (bridge, futures)");
        return -1;
    }

    /* Check if coherence is above threshold for synchronization */
    if (bridge->coherence_level < coherence_threshold) {
        return 1;  /* Not synchronized */
    }

    /* Mark all futures as synchronized (ready) */
    for (uint32_t i = 0; i < num_futures; i++) {
        if (futures[i]) {
            futures[i]->ready = true;
            futures[i]->result.confidence = bridge->coherence_level;
        }
    }

    return 0;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_bio_async_connect_dragonfly(
    dragonfly_bio_async_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_connect_dragonfly: bridge is NULL");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_bio_async_connect_system(
    dragonfly_bio_async_bridge_t* bridge,
    void* bio_async_system
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_connect_system: bridge is NULL");
        return -1;
    }
    bridge->bio_async_system = bio_async_system;
    return 0;
}

int dragonfly_bio_async_update(dragonfly_bio_async_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_update: bridge is NULL");
        return -1;
    }

    bridge->current_time_ms += dt_ms;

    /* Decay neuromodulator levels */
    float decay = 1.0f - bridge->config.dopamine_decay_rate * dt_ms * 0.001f;
    if (decay < 0.0f) decay = 0.0f;

    bridge->dopamine_level *= decay;
    bridge->norepinephrine_level *= decay;
    bridge->acetylcholine_level *= decay;

    /* Update phase angle based on mode */
    float freq_hz;
    switch (bridge->current_phase) {
        case DRAGONFLY_PHASE_GAMMA: freq_hz = 60.0f; break;
        case DRAGONFLY_PHASE_BETA: freq_hz = 20.0f; break;
        case DRAGONFLY_PHASE_ALPHA: freq_hz = 10.0f; break;
        case DRAGONFLY_PHASE_THETA: freq_hz = 6.0f; break;
        default: freq_hz = 60.0f;
    }

    bridge->phase_angle += NIMCP_TWO_PI_F * freq_hz * dt_ms * 0.001f;
    if (bridge->phase_angle > NIMCP_TWO_PI_F) {
        bridge->phase_angle -= NIMCP_TWO_PI_F;
    }

    /* Process pending futures */
    for (uint32_t i = 0; i < bridge->num_futures; i++) {
        dragonfly_bio_future_t* future = bridge->futures[i];
        if (future && !future->ready) {
            float elapsed = bridge->current_time_ms - future->start_time_ms;
            /* Complete after simulated processing time */
            if (elapsed > 5.0f) {
                future->ready = true;
                future->result.operation = future->operation;
                future->result.success = true;
                future->result.confidence = 0.85f + bridge->acetylcholine_level * 0.1f;
                future->result.latency_ms = elapsed;
                bridge->stats.operations_completed++;
            }
        }
    }

    /* Update average stats */
    bridge->stats.avg_latency_ms = 5.0f;
    bridge->stats.avg_confidence = 0.85f;
    bridge->stats.dopamine_level = bridge->dopamine_level;
    bridge->stats.norepinephrine_level = bridge->norepinephrine_level;
    bridge->stats.coherence_level = bridge->coherence_level;

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_bio_async_bridge_get_stats(
    const dragonfly_bio_async_bridge_t* bridge,
    dragonfly_bio_async_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_bio_async_bridge_reset_stats(dragonfly_bio_async_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_bio_async_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_bio_async_channel_name(dragonfly_neuromod_channel_t channel) {
    switch (channel) {
        case DRAGONFLY_CHANNEL_DOPAMINE: return "dopamine";
        case DRAGONFLY_CHANNEL_NOREPINEPHRINE: return "norepinephrine";
        case DRAGONFLY_CHANNEL_ACETYLCHOLINE: return "acetylcholine";
        case DRAGONFLY_CHANNEL_SEROTONIN: return "serotonin";
        default: return "unknown";
    }
}

const char* dragonfly_bio_async_op_name(dragonfly_async_op_t op) {
    switch (op) {
        case DRAGONFLY_ASYNC_TSDN_UPDATE: return "tsdn_update";
        case DRAGONFLY_ASYNC_TRACKING: return "tracking";
        case DRAGONFLY_ASYNC_PREDICTION: return "prediction";
        case DRAGONFLY_ASYNC_INTERCEPT: return "intercept";
        case DRAGONFLY_ASYNC_MODE_SWITCH: return "mode_switch";
        case DRAGONFLY_ASYNC_FULL_CYCLE: return "full_cycle";
        default: return "unknown";
    }
}
