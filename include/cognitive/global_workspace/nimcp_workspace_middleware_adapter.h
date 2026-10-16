//=============================================================================
// nimcp_workspace_middleware_adapter.h - Workspace-Middleware Integration
//=============================================================================
/**
 * @file nimcp_workspace_middleware_adapter.h
 * @brief Global Workspace integration with middleware event bus
 *
 * WHAT: Connects Global Workspace to middleware for competition & broadcast
 * WHY:  Enable conscious access to high-salience patterns
 * HOW:  Event subscription, Shannon monitoring, broadcast tracking
 *
 * INTEGRATION PATHS:
 * - Middleware → Workspace: SALIENCE_PEAK, PATTERN_DETECTED, ATTENTION_SHIFT
 * - Workspace → Middleware: Winning coalition broadcasts
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#ifndef NIMCP_WORKSPACE_MIDDLEWARE_ADAPTER_H
#define NIMCP_WORKSPACE_MIDDLEWARE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "middleware/integration/nimcp_flow_tracker.h"
#include "middleware/events/nimcp_event_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Workspace-middleware adapter configuration
 */
typedef struct {
    float ignition_threshold;            /**< Threshold for conscious access */
    float min_salience_for_competition;  /**< Min salience to compete */
    uint32_t max_broadcasts_per_second;  /**< Rate limit for broadcasts */
    bool enable_shannon_filtering;       /**< Filter low-info events */
    bool enable_broadcast_tracking;      /**< Track broadcast efficiency */
} workspace_adapter_config_t;

/**
 * @brief Workspace-middleware adapter handle
 */
typedef struct workspace_middleware_adapter workspace_middleware_adapter_t;

/**
 * @brief Workspace adapter statistics
 */
typedef struct {
    uint64_t salience_events_received;
    uint64_t patterns_received;
    uint64_t competitions_triggered;
    uint64_t ignitions_occurred;
    uint64_t broadcasts_sent;
    float avg_winning_salience;
    float broadcast_efficiency;
} workspace_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Workspace-middleware adapter
 *
 * WHAT: Initialize adapter connecting Workspace to middleware
 * WHY:  Enable conscious access to salient patterns
 * HOW:  Creates adapter with competition mechanism & tracking
 *
 * @param workspace Global workspace instance
 * @param shannon Shannon monitor (optional)
 * @param flow Flow tracker (optional)
 * @param config Adapter configuration (NULL for defaults)
 * @return Adapter handle or NULL on error
 */
workspace_middleware_adapter_t* workspace_adapter_create(
    global_workspace_t* workspace,
    shannon_monitor_t* shannon,
    flow_tracker_t* flow,
    const workspace_adapter_config_t* config
);

/**
 * @brief Destroy Workspace-middleware adapter
 *
 * @param adapter Adapter to destroy
 */
void workspace_adapter_destroy(workspace_middleware_adapter_t* adapter);

/**
 * @brief Get default adapter configuration
 *
 * @return Default configuration
 */
workspace_adapter_config_t workspace_adapter_default_config(void);

//=============================================================================
// Event Handling API
//=============================================================================

/**
 * @brief Handle salience peak event from middleware
 *
 * WHAT: Process high-salience event for competition
 * WHY:  Salience peaks compete for conscious access
 * HOW:  Adds to competition queue, triggers if threshold exceeded
 *
 * @param adapter Workspace adapter
 * @param event Salience peak event
 */
void workspace_adapter_handle_salience_event(
    workspace_middleware_adapter_t* adapter,
    const event_t* event
);

/**
 * @brief Handle pattern match event from middleware
 *
 * @param adapter Workspace adapter
 * @param event Pattern detected event
 */
void workspace_adapter_handle_pattern_event(
    workspace_middleware_adapter_t* adapter,
    const event_t* event
);

/**
 * @brief Handle attention shift from Executive
 *
 * @param adapter Workspace adapter
 * @param event Attention shift event
 */
void workspace_adapter_handle_attention_event(
    workspace_middleware_adapter_t* adapter,
    const event_t* event
);

//=============================================================================
// Broadcasting API
//=============================================================================

/**
 * @brief Broadcast winning coalition to all subscribers
 *
 * WHAT: Send conscious content to all subscribed modules
 * WHY:  Enable global information sharing (GWT mechanism)
 * HOW:  Creates broadcast event, tracks with Shannon/flow
 *
 * @param adapter Workspace adapter
 * @param content Winning coalition content
 * @param salience Salience of winner [0-1]
 * @return true on success, false on error
 */
bool workspace_adapter_broadcast_winner(
    workspace_middleware_adapter_t* adapter,
    const float* content,
    uint32_t content_size,
    float salience
);

/**
 * @brief Check if ignition threshold reached
 *
 * @param adapter Workspace adapter
 * @return true if workspace ignited, false otherwise
 */
bool workspace_adapter_is_ignited(
    const workspace_middleware_adapter_t* adapter
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get adapter statistics
 *
 * @param adapter Workspace adapter
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool workspace_adapter_get_stats(
    const workspace_middleware_adapter_t* adapter,
    workspace_adapter_stats_t* stats
);

/**
 * @brief Reset adapter statistics
 *
 * @param adapter Workspace adapter
 */
void workspace_adapter_reset_stats(
    workspace_middleware_adapter_t* adapter
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_WORKSPACE_MIDDLEWARE_ADAPTER_H
