/**
 * @file nimcp_dragonfly_workspace_bridge.h
 * @brief Dragonfly-to-Global Workspace Integration Bridge
 *
 * WHAT: Bridges dragonfly tracking to global workspace broadcast system
 * WHY:  Enable conscious access to target information across all modules
 * HOW:  Compete for workspace access, broadcast target state, receive context
 *
 * BIOLOGICAL BASIS:
 * - Target detection competes for conscious access (workspace entry)
 * - Successful ignition broadcasts target info to all cognitive modules
 * - Working memory, executive, motor planning all receive target updates
 * - Enables coordinated response across the cognitive system
 *
 * @author NIMCP Development Team
 * @date 2025-12-28
 */

#ifndef NIMCP_DRAGONFLY_WORKSPACE_BRIDGE_H
#define NIMCP_DRAGONFLY_WORKSPACE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dragonfly_system_s;
typedef struct dragonfly_system_s dragonfly_system_t;

//=============================================================================
// Constants
//=============================================================================

#define DRAGONFLY_WS_MAX_CONTENT_DIM 256    /**< Max broadcast dimension */
#define DRAGONFLY_WS_DEFAULT_SALIENCE 0.7f  /**< Default competition salience */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Content types for workspace broadcast
 */
typedef enum {
    WS_CONTENT_TARGET_POSITION = 0,     /**< Current target position */
    WS_CONTENT_TARGET_VELOCITY,         /**< Target velocity vector */
    WS_CONTENT_PREDICTED_TRAJECTORY,    /**< Predicted future positions */
    WS_CONTENT_INTERCEPT_POINT,         /**< Planned interception point */
    WS_CONTENT_TSDN_ACTIVATION,         /**< TSDN population response */
    WS_CONTENT_MODE_STATE,              /**< Current tracking mode */
    WS_CONTENT_ALERT_SIGNAL             /**< Target detection alert */
} dragonfly_ws_content_t;

/**
 * @brief Competition result
 */
typedef enum {
    WS_COMPETE_WON = 0,                 /**< Won workspace access */
    WS_COMPETE_LOST,                    /**< Lost competition */
    WS_COMPETE_QUEUED,                  /**< Queued for later */
    WS_COMPETE_IGNORED                  /**< Below ignition threshold */
} dragonfly_ws_compete_result_t;

/**
 * @brief Subscription mode
 */
typedef enum {
    WS_SUBSCRIBE_ALL = 0,               /**< All broadcasts */
    WS_SUBSCRIBE_MOTOR,                 /**< Motor-related broadcasts */
    WS_SUBSCRIBE_VISUAL,                /**< Visual-related broadcasts */
    WS_SUBSCRIBE_EXECUTIVE              /**< Executive-related broadcasts */
} dragonfly_ws_subscribe_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Broadcast content
 */
typedef struct {
    dragonfly_ws_content_t content_type; /**< Type of content */
    float data[DRAGONFLY_WS_MAX_CONTENT_DIM]; /**< Content data */
    uint32_t data_dim;                  /**< Data dimension */
    float salience;                     /**< Content salience/priority */
    float timestamp_ms;                 /**< Broadcast timestamp */
} dragonfly_ws_broadcast_t;

/**
 * @brief Received broadcast from other modules
 */
typedef struct {
    uint32_t source_module;             /**< Source module ID */
    float content[DRAGONFLY_WS_MAX_CONTENT_DIM]; /**< Content data */
    uint32_t content_dim;               /**< Content dimension */
    float salience;                     /**< Content salience */
    float relevance;                    /**< Relevance to dragonfly */
} dragonfly_ws_received_t;

/**
 * @brief Configuration
 */
typedef struct {
    /* Competition settings */
    float ignition_threshold;           /**< Threshold for broadcast access */
    float base_salience;                /**< Base content salience */
    float target_detection_salience;    /**< Salience boost for new targets */
    float pursuit_salience;             /**< Salience during active pursuit */

    /* Subscription settings */
    dragonfly_ws_subscribe_mode_t subscribe_mode; /**< What to subscribe to */
    bool subscribe_motor_commands;      /**< Subscribe to motor broadcasts */
    bool subscribe_executive_goals;     /**< Subscribe to executive goals */

    /* Broadcast settings */
    bool auto_broadcast_targets;        /**< Auto-broadcast on target detection */
    bool auto_broadcast_intercept;      /**< Auto-broadcast intercept plans */
    float broadcast_decay_rate;         /**< How fast broadcasts decay */

    /* Integration */
    float context_influence_weight;     /**< How much context affects tracking */
} dragonfly_ws_config_t;

/**
 * @brief Statistics
 */
typedef struct {
    uint64_t competitions_entered;
    uint64_t competitions_won;
    uint64_t broadcasts_sent;
    uint64_t broadcasts_received;
    float avg_salience;
    float avg_competition_score;
    float workspace_occupancy_pct;
} dragonfly_ws_stats_t;

/**
 * @brief Workspace bridge handle
 */
typedef struct dragonfly_workspace_bridge_s dragonfly_workspace_bridge_t;

//=============================================================================
// Configuration
//=============================================================================

int dragonfly_ws_bridge_default_config(dragonfly_ws_config_t* config);
int dragonfly_ws_bridge_validate_config(const dragonfly_ws_config_t* config);

//=============================================================================
// Lifecycle
//=============================================================================

dragonfly_workspace_bridge_t* dragonfly_ws_bridge_create(
    dragonfly_system_t* dragonfly,
    void* global_workspace,
    const dragonfly_ws_config_t* config
);

void dragonfly_ws_bridge_destroy(dragonfly_workspace_bridge_t* bridge);
int dragonfly_ws_bridge_reset(dragonfly_workspace_bridge_t* bridge);

//=============================================================================
// Competition and Broadcast
//=============================================================================

dragonfly_ws_compete_result_t dragonfly_ws_compete(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_content_t content_type,
    const float* content,
    uint32_t content_dim,
    float salience
);

int dragonfly_ws_broadcast_target(
    dragonfly_workspace_bridge_t* bridge,
    float x, float y, float z,
    float vx, float vy, float vz
);

int dragonfly_ws_broadcast_intercept(
    dragonfly_workspace_bridge_t* bridge,
    float intercept_x, float intercept_y, float intercept_z,
    float time_to_intercept
);

int dragonfly_ws_broadcast_alert(
    dragonfly_workspace_bridge_t* bridge,
    float alert_level
);

//=============================================================================
// Subscription and Reception
//=============================================================================

int dragonfly_ws_subscribe(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_subscribe_mode_t mode
);

int dragonfly_ws_unsubscribe(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_subscribe_mode_t mode
);

bool dragonfly_ws_has_broadcast(const dragonfly_workspace_bridge_t* bridge);

int dragonfly_ws_receive(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_received_t* received
);

//=============================================================================
// Context Integration
//=============================================================================

int dragonfly_ws_get_context(
    const dragonfly_workspace_bridge_t* bridge,
    float* context,
    uint32_t context_dim
);

int dragonfly_ws_apply_context(dragonfly_workspace_bridge_t* bridge);

float dragonfly_ws_get_context_relevance(const dragonfly_workspace_bridge_t* bridge);

//=============================================================================
// Integration
//=============================================================================

int dragonfly_ws_connect_dragonfly(
    dragonfly_workspace_bridge_t* bridge,
    dragonfly_system_t* dragonfly
);

int dragonfly_ws_connect_workspace(
    dragonfly_workspace_bridge_t* bridge,
    void* global_workspace
);

int dragonfly_ws_update(dragonfly_workspace_bridge_t* bridge, float dt_ms);

//=============================================================================
// Statistics
//=============================================================================

int dragonfly_ws_bridge_get_stats(
    const dragonfly_workspace_bridge_t* bridge,
    dragonfly_ws_stats_t* stats
);

int dragonfly_ws_bridge_reset_stats(dragonfly_workspace_bridge_t* bridge);

//=============================================================================
// Utility
//=============================================================================

const char* dragonfly_ws_content_name(dragonfly_ws_content_t content);
const char* dragonfly_ws_result_name(dragonfly_ws_compete_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DRAGONFLY_WORKSPACE_BRIDGE_H */
