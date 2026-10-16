//=============================================================================
// nimcp_introspection_middleware_adapter.h - Introspection-Middleware Integration
//=============================================================================
/**
 * @file nimcp_introspection_middleware_adapter.h
 * @brief Introspection integration with middleware diagnostics
 *
 * WHAT: Connects Introspection to middleware for system monitoring
 * WHY:  Enable introspective queries about middleware/brain state
 * HOW:  Event subscription, Shannon/flow metrics aggregation
 *
 * INTEGRATION PATHS:
 * - Middleware → Introspection: Signal stats, error events, performance metrics
 * - Introspection queries: Shannon capacity, flow efficiency, bottlenecks
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#ifndef NIMCP_INTROSPECTION_MIDDLEWARE_ADAPTER_H
#define NIMCP_INTROSPECTION_MIDDLEWARE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/introspection/nimcp_introspection.h"
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
 * @brief Introspection-middleware adapter configuration
 */
typedef struct {
    uint32_t stats_update_interval_ms;   /**< How often to update stats */
    bool enable_error_tracking;          /**< Track error events */
    bool enable_performance_tracking;    /**< Track performance metrics */
    bool enable_bottleneck_alerts;       /**< Alert on bottlenecks */
} introspection_adapter_config_t;

/**
 * @brief Introspection-middleware adapter handle
 */
typedef struct introspection_middleware_adapter introspection_middleware_adapter_t;

/**
 * @brief Introspection adapter statistics
 */
typedef struct {
    uint64_t stats_queries;
    uint64_t error_events_received;
    uint64_t performance_samples;
    uint64_t bottleneck_alerts;
    float avg_channel_utilization;
    float avg_flow_efficiency;
} introspection_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Introspection-middleware adapter
 *
 * WHAT: Initialize adapter connecting Introspection to middleware
 * WHY:  Enable system self-monitoring and diagnostics
 * HOW:  Creates adapter with Shannon & flow metric access
 *
 * @param introspection Introspection context instance
 * @param shannon Shannon monitor (optional)
 * @param flow Flow tracker (optional)
 * @param config Adapter configuration (NULL for defaults)
 * @return Adapter handle or NULL on error
 */
introspection_middleware_adapter_t* introspection_adapter_create(
    introspection_context_t* introspection,
    shannon_monitor_t* shannon,
    flow_tracker_t* flow,
    const introspection_adapter_config_t* config
);

/**
 * @brief Destroy Introspection-middleware adapter
 *
 * @param adapter Adapter to destroy
 */
void introspection_adapter_destroy(introspection_middleware_adapter_t* adapter);

/**
 * @brief Get default adapter configuration
 *
 * @return Default configuration
 */
introspection_adapter_config_t introspection_adapter_default_config(void);

//=============================================================================
// Event Handling API
//=============================================================================

/**
 * @brief Handle error event from middleware
 *
 * WHAT: Process error event for diagnostic logging
 * WHY:  Track system errors for introspection
 * HOW:  Logs error, updates error statistics
 *
 * @param adapter Introspection adapter
 * @param event Error event
 */
void introspection_adapter_handle_error_event(
    introspection_middleware_adapter_t* adapter,
    const event_t* event
);

/**
 * @brief Handle performance metric update
 *
 * @param adapter Introspection adapter
 * @param event Performance event
 */
void introspection_adapter_handle_performance_event(
    introspection_middleware_adapter_t* adapter,
    const event_t* event
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Query Shannon channel metrics
 *
 * WHAT: Get current Shannon monitoring metrics
 * WHY:  Introspection needs channel health info
 * HOW:  Returns Shannon monitor metrics
 *
 * @param adapter Introspection adapter
 * @param metrics Output Shannon metrics
 * @return true on success, false if no monitor
 */
bool introspection_adapter_query_shannon_metrics(
    const introspection_middleware_adapter_t* adapter,
    shannon_routing_metrics_t* metrics
);

/**
 * @brief Query flow efficiency metrics
 *
 * @param adapter Introspection adapter
 * @param metrics Output flow metrics
 * @return true on success, false if no tracker
 */
bool introspection_adapter_query_flow_metrics(
    const introspection_middleware_adapter_t* adapter,
    cross_modal_flow_metrics_t* metrics
);

/**
 * @brief Query if system is bottlenecked
 *
 * @param adapter Introspection adapter
 * @return true if bottleneck detected
 */
bool introspection_adapter_query_is_bottlenecked(
    const introspection_middleware_adapter_t* adapter
);

/**
 * @brief Query worst performing path
 *
 * @param adapter Introspection adapter
 * @param efficiency Output efficiency of worst path
 * @return Path with worst efficiency
 */
integration_path_t introspection_adapter_query_worst_path(
    const introspection_middleware_adapter_t* adapter,
    float* efficiency
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get adapter statistics
 *
 * @param adapter Introspection adapter
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool introspection_adapter_get_stats(
    const introspection_middleware_adapter_t* adapter,
    introspection_adapter_stats_t* stats
);

/**
 * @brief Reset adapter statistics
 *
 * @param adapter Introspection adapter
 */
void introspection_adapter_reset_stats(
    introspection_middleware_adapter_t* adapter
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_INTROSPECTION_MIDDLEWARE_ADAPTER_H
