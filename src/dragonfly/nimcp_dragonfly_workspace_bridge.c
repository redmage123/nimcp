/**
 * @file nimcp_dragonfly_workspace_bridge.c
 * @brief Dragonfly-to-Global Workspace Integration Bridge Implementation
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "dragonfly/nimcp_dragonfly_workspace_bridge.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_workspace_bridge)

#define LOG_MODULE "DRAGONFLY_WORKSPACE_BRIDGE"


//=============================================================================
// Internal Structures
//=============================================================================

#define WS_RECEIVE_QUEUE_SIZE 16

struct dragonfly_workspace_bridge_s {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    dragonfly_system_t* dragonfly;
    void* global_workspace;
    dragonfly_ws_config_t config;

    /* Current broadcast state */
    dragonfly_ws_broadcast_t current_broadcast;
    bool has_active_broadcast;
    float broadcast_age_ms;

    /* Receive queue */
    dragonfly_ws_received_t receive_queue[WS_RECEIVE_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Subscription state */
    bool subscribed[4];  /* One per subscribe mode */

    /* Context from workspace */
    float context[DRAGONFLY_WS_MAX_CONTENT_DIM];
    uint32_t context_dim;
    float context_relevance;

    /* Competition state */
    float current_salience;
    dragonfly_ws_compete_result_t last_result;

    /* Timing */
    float current_time_ms;

    /* Statistics */
    dragonfly_ws_stats_t stats;
};

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_ws_bridge_default_config(dragonfly_ws_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_bridge_default_config: config is NULL");
        return -1;
    }

    /* Competition settings */
    config->ignition_threshold = 0.5f;
    config->base_salience = DRAGONFLY_WS_DEFAULT_SALIENCE;
    config->target_detection_salience = 0.9f;
    config->pursuit_salience = 0.8f;

    /* Subscription settings */
    config->subscribe_mode = WS_SUBSCRIBE_ALL;
    config->subscribe_motor_commands = true;
    config->subscribe_executive_goals = true;

    /* Broadcast settings */
    config->auto_broadcast_targets = true;
    config->auto_broadcast_intercept = true;
    config->broadcast_decay_rate = 0.1f;

    /* Integration */
    config->context_influence_weight = 0.3f;

    return 0;
}

int dragonfly_ws_bridge_validate_config(const dragonfly_ws_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_bridge_validate_config: config is NULL");
        return -1;
    }

    if (config->ignition_threshold < 0.0f || config->ignition_threshold > 1.0f) {
        return -1;
    }
    if (config->base_salience < 0.0f || config->base_salience > 1.0f) {
        return -1;
    }
    if (config->target_detection_salience < 0.0f || config->target_detection_salience > 1.0f) {
        return -1;
    }
    if (config->pursuit_salience < 0.0f || config->pursuit_salience > 1.0f) {
        return -1;
    }
    if (config->broadcast_decay_rate < 0.0f) {
        return -1;
    }
    if (config->context_influence_weight < 0.0f || config->context_influence_weight > 1.0f) {
        return -1;
    }
    if (config->subscribe_mode > WS_SUBSCRIBE_EXECUTIVE) {
        return -1;
    }

    return 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_workspace_bridge_t* dragonfly_ws_bridge_create(
    dragonfly_system_t* dragonfly,
    void* global_workspace,
    const dragonfly_ws_config_t* config
) {
    dragonfly_workspace_bridge_t* bridge = nimcp_calloc(1, sizeof(dragonfly_workspace_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_ws_bridge_create: failed to allocate bridge");
        return NULL;
    }

    if (config) {
        if (dragonfly_ws_bridge_validate_config(config) != 0) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "dragonfly_ws_bridge_create: invalid configuration");
            nimcp_free(bridge);
            return NULL;
        }
        bridge->config = *config;
    } else {
        dragonfly_ws_bridge_default_config(&bridge->config);
    }

    bridge->dragonfly = dragonfly;
    bridge->global_workspace = global_workspace;

    /* Initialize subscription based on config */
    bridge->subscribed[bridge->config.subscribe_mode] = true;

    bridge->current_salience = bridge->config.base_salience;
    bridge->context_relevance = 0.5f;

    /* Initialize bridge base infrastructure (mutex + metrics) */
    if (bridge_base_init(&bridge->base, 0, "dragonfly_workspace") != 0) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void dragonfly_ws_bridge_destroy(dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "dragonfly_workspace");
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int dragonfly_ws_bridge_reset(dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_bridge_reset: bridge is NULL");
        return -1;
    }

    bridge->has_active_broadcast = false;
    bridge->broadcast_age_ms = 0.0f;
    bridge->queue_head = 0;
    bridge->queue_tail = 0;
    bridge->queue_count = 0;
    bridge->current_salience = bridge->config.base_salience;
    bridge->context_relevance = 0.5f;
    bridge->current_time_ms = 0.0f;
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->context, 0, sizeof(bridge->context));
    bridge->context_dim = 0;

    return 0;
}

//=============================================================================
// Competition and Broadcast
//=============================================================================

dragonfly_ws_compete_result_t dragonfly_ws_compete(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_content_t content_type,
    const float* content,
    uint32_t content_dim,
    float salience
) {
    if (!bridge) return WS_COMPETE_IGNORED;

    bridge->stats.competitions_entered++;

    /* Check if salience is above ignition threshold */
    if (salience < bridge->config.ignition_threshold) {
        bridge->last_result = WS_COMPETE_IGNORED;
        return WS_COMPETE_IGNORED;
    }

    /* Simulate competition - higher salience wins */
    float competition_score = salience + bridge->context_relevance * 0.1f;
    bridge->stats.avg_competition_score =
        (bridge->stats.avg_competition_score * (bridge->stats.competitions_entered - 1) + competition_score) /
        bridge->stats.competitions_entered;

    /* Threshold for winning (simulated) */
    if (competition_score > 0.7f) {
        bridge->stats.competitions_won++;

        /* Store broadcast content */
        bridge->current_broadcast.content_type = content_type;
        bridge->current_broadcast.data_dim = (content_dim > DRAGONFLY_WS_MAX_CONTENT_DIM)
            ? DRAGONFLY_WS_MAX_CONTENT_DIM : content_dim;
        if (content) {
            memcpy(bridge->current_broadcast.data, content,
                   bridge->current_broadcast.data_dim * sizeof(float));
        }
        bridge->current_broadcast.salience = salience;
        bridge->current_broadcast.timestamp_ms = bridge->current_time_ms;
        bridge->has_active_broadcast = true;
        bridge->broadcast_age_ms = 0.0f;

        bridge->stats.broadcasts_sent++;
        bridge->current_salience = salience;
        bridge->last_result = WS_COMPETE_WON;
        return WS_COMPETE_WON;
    } else if (competition_score > 0.5f) {
        bridge->last_result = WS_COMPETE_QUEUED;
        return WS_COMPETE_QUEUED;
    }

    bridge->last_result = WS_COMPETE_LOST;
    return WS_COMPETE_LOST;
}

int dragonfly_ws_broadcast_target(
    dragonfly_workspace_bridge_t* bridge,
    float x, float y, float z,
    float vx, float vy, float vz
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_broadcast_target: bridge is NULL");
        return -1;
    }

    float content[6] = {x, y, z, vx, vy, vz};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_TARGET_POSITION,
        content,
        6,
        bridge->config.target_detection_salience
    );

    return (result == WS_COMPETE_WON || result == WS_COMPETE_QUEUED) ? 0 : -1;
}

int dragonfly_ws_broadcast_intercept(
    dragonfly_workspace_bridge_t* bridge,
    float intercept_x, float intercept_y, float intercept_z,
    float time_to_intercept
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_broadcast_intercept: bridge is NULL");
        return -1;
    }

    float content[4] = {intercept_x, intercept_y, intercept_z, time_to_intercept};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_INTERCEPT_POINT,
        content,
        4,
        bridge->config.pursuit_salience
    );

    return (result == WS_COMPETE_WON || result == WS_COMPETE_QUEUED) ? 0 : -1;
}

int dragonfly_ws_broadcast_alert(
    dragonfly_workspace_bridge_t* bridge,
    float alert_level
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_broadcast_alert: bridge is NULL");
        return -1;
    }

    float content[1] = {alert_level};
    dragonfly_ws_compete_result_t result = dragonfly_ws_compete(
        bridge,
        WS_CONTENT_ALERT_SIGNAL,
        content,
        1,
        alert_level  /* Alert level is the salience */
    );

    return (result == WS_COMPETE_WON || result == WS_COMPETE_QUEUED) ? 0 : -1;
}

//=============================================================================
// Subscription and Reception
//=============================================================================

int dragonfly_ws_subscribe(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_subscribe_mode_t mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_subscribe: bridge is NULL");
        return -1;
    }
    if (mode > WS_SUBSCRIBE_EXECUTIVE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_ws_subscribe: validation failed");
        return -1;
    }

    bridge->subscribed[mode] = true;
    return 0;
}

int dragonfly_ws_unsubscribe(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_subscribe_mode_t mode
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_unsubscribe: bridge is NULL");
        return -1;
    }
    if (mode > WS_SUBSCRIBE_EXECUTIVE) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_ws_unsubscribe: validation failed");
        return -1;
    }

    bridge->subscribed[mode] = false;
    return 0;
}

bool dragonfly_ws_has_broadcast(const dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    return bridge->queue_count > 0;
}

int dragonfly_ws_receive(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_received_t* received
) {
    if (!bridge || !received) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_receive: required parameter is NULL (bridge, received)");
        return -1;
    }

    if (bridge->queue_count == 0) {
        return 1;  /* No broadcasts available */
    }

    *received = bridge->receive_queue[bridge->queue_head];
    bridge->queue_head = (bridge->queue_head + 1) % WS_RECEIVE_QUEUE_SIZE;
    bridge->queue_count--;
    bridge->stats.broadcasts_received++;

    return 0;
}

//=============================================================================
// Context Integration
//=============================================================================

int dragonfly_ws_get_context(
    const dragonfly_workspace_bridge_t* bridge,
    float* context,
    uint32_t context_dim
) {
    if (!bridge || !context) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_get_context: required parameter is NULL (bridge, context)");
        return -1;
    }

    uint32_t copy_dim = (context_dim < bridge->context_dim) ? context_dim : bridge->context_dim;
    memcpy(context, bridge->context, copy_dim * sizeof(float));

    return (int)copy_dim;
}

int dragonfly_ws_apply_context(dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_apply_context: bridge is NULL");
        return -1;
    }

    /* Apply context influence to current salience */
    bridge->current_salience = bridge->config.base_salience +
        bridge->context_relevance * bridge->config.context_influence_weight;

    if (bridge->current_salience > 1.0f) {
        bridge->current_salience = 1.0f;
    }

    return 0;
}

float dragonfly_ws_get_context_relevance(const dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) return 0.0f;
    return bridge->context_relevance;
}

//=============================================================================
// Integration
//=============================================================================

int dragonfly_ws_connect_dragonfly(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_system_t* dragonfly
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_connect_dragonfly: bridge is NULL");
        return -1;
    }
    bridge->dragonfly = dragonfly;
    return 0;
}

int dragonfly_ws_connect_workspace(
    dragonfly_workspace_bridge_t* bridge,
    void* global_workspace
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_connect_workspace: bridge is NULL");
        return -1;
    }
    bridge->global_workspace = global_workspace;
    return 0;
}

int dragonfly_ws_update(dragonfly_workspace_bridge_t* bridge, float dt_ms) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_update: bridge is NULL");
        return -1;
    }

    bridge->current_time_ms += dt_ms;

    /* Decay active broadcast */
    if (bridge->has_active_broadcast) {
        bridge->broadcast_age_ms += dt_ms;
        bridge->current_broadcast.salience *=
            (1.0f - bridge->config.broadcast_decay_rate * dt_ms * 0.001f);

        if (bridge->current_broadcast.salience < 0.1f) {
            bridge->has_active_broadcast = false;
        }
    }

    /* Update workspace occupancy estimate */
    bridge->stats.workspace_occupancy_pct = bridge->has_active_broadcast ?
        bridge->current_broadcast.salience * 100.0f : 0.0f;

    /* Update average salience */
    if (bridge->stats.broadcasts_sent > 0) {
        bridge->stats.avg_salience =
            (bridge->stats.avg_salience * (bridge->stats.broadcasts_sent - 1) +
             bridge->current_salience) / bridge->stats.broadcasts_sent;
    }

    return 0;
}

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_ws_bridge_get_stats(
    const dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

int dragonfly_ws_bridge_reset_stats(dragonfly_workspace_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_ws_bridge_reset_stats: bridge is NULL");
        return -1;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    return 0;
}

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_ws_content_name(dragonfly_ws_content_t content) {
    switch (content) {
        case WS_CONTENT_TARGET_POSITION: return "target_position";
        case WS_CONTENT_TARGET_VELOCITY: return "target_velocity";
        case WS_CONTENT_PREDICTED_TRAJECTORY: return "predicted_trajectory";
        case WS_CONTENT_INTERCEPT_POINT: return "intercept_point";
        case WS_CONTENT_TSDN_ACTIVATION: return "tsdn_activation";
        case WS_CONTENT_MODE_STATE: return "mode_state";
        case WS_CONTENT_ALERT_SIGNAL: return "alert_signal";
        default: return "unknown";
    }
}

const char* dragonfly_ws_result_name(dragonfly_ws_compete_result_t result) {
    switch (result) {
        case WS_COMPETE_WON: return "won";
        case WS_COMPETE_LOST: return "lost";
        case WS_COMPETE_QUEUED: return "queued";
        case WS_COMPETE_IGNORED: return "ignored";
        default: return "unknown";
    }
}
