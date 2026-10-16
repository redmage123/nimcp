//=============================================================================
// nimcp_executive_middleware_adapter.h - Executive-Middleware Integration
//=============================================================================
/**
 * @file nimcp_executive_middleware_adapter.h
 * @brief Executive Controller integration with middleware event bus
 *
 * WHAT: Connects Executive Controller to middleware for pattern events & commands
 * WHY:  Enable Executive to receive patterns and control attention/task switching
 * HOW:  Event subscription, Shannon monitoring, flow tracking
 *
 * INTEGRATION PATHS:
 * - Middleware → Executive: PATTERN_DETECTED, OSCILLATION_CHANGE, SALIENCE_PEAK
 * - Executive → Middleware: ATTENTION_SHIFT, CONFIGURE_ATTENTION, TASK_SWITCH
 *
 * @author NIMCP Development Team
 * @date 2025-11-21
 */

#ifndef NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H
#define NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/nimcp_executive.h"
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
 * @brief Executive-middleware adapter configuration
 */
typedef struct {
    float pattern_info_threshold;        /**< Min info bits for pattern events */
    float salience_info_threshold;       /**< Min info bits for salience events */
    uint32_t max_events_per_second;      /**< Rate limit for Executive */
    bool enable_adaptive_filtering;      /**< Adjust thresholds dynamically */
    bool enable_command_tracking;        /**< Track Executive commands */
} executive_adapter_config_t;

/**
 * @brief Executive-middleware adapter handle
 */
typedef struct executive_middleware_adapter executive_middleware_adapter_t;

/**
 * @brief Executive adapter statistics
 */
typedef struct {
    uint64_t patterns_received;
    uint64_t patterns_filtered;
    uint64_t commands_sent;
    uint64_t attention_shifts;
    float avg_pattern_info_bits;
    float avg_command_latency_us;
} executive_adapter_stats_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create Executive-middleware adapter
 *
 * WHAT: Initialize adapter connecting Executive to middleware
 * WHY:  Enable Executive to receive patterns and send commands
 * HOW:  Creates adapter with Shannon & flow tracking
 *
 * @param executive Executive controller instance
 * @param shannon Shannon monitor (optional, can be NULL)
 * @param flow Flow tracker (optional, can be NULL)
 * @param config Adapter configuration (NULL for defaults)
 * @return Adapter handle or NULL on error
 */
executive_middleware_adapter_t* executive_adapter_create(
    executive_controller_t* executive,
    shannon_monitor_t* shannon,
    flow_tracker_t* flow,
    const executive_adapter_config_t* config
);

/**
 * @brief Destroy Executive-middleware adapter
 *
 * @param adapter Adapter to destroy
 */
void executive_adapter_destroy(executive_middleware_adapter_t* adapter);

/**
 * @brief Get default adapter configuration
 *
 * @return Default configuration
 */
executive_adapter_config_t executive_adapter_default_config(void);

//=============================================================================
// Event Handling API
//=============================================================================

/**
 * @brief Handle pattern detected event from middleware
 *
 * WHAT: Process pattern event and route to Executive
 * WHY:  Executive needs pattern information for task management
 * HOW:  Measures info content, filters if needed, forwards to Executive
 *
 * @param adapter Executive adapter
 * @param event Pattern detected event
 */
void executive_adapter_handle_pattern_event(
    executive_middleware_adapter_t* adapter,
    const event_t* event
);

/**
 * @brief Handle salience peak event from middleware
 *
 * @param adapter Executive adapter
 * @param event Salience peak event
 */
void executive_adapter_handle_salience_event(
    executive_middleware_adapter_t* adapter,
    const event_t* event
);

/**
 * @brief Handle oscillation change event from middleware
 *
 * @param adapter Executive adapter
 * @param event Oscillation change event
 */
void executive_adapter_handle_oscillation_event(
    executive_middleware_adapter_t* adapter,
    const event_t* event
);

//=============================================================================
// Command Sending API
//=============================================================================

/**
 * @brief Send attention shift command to middleware
 *
 * WHAT: Executive commands middleware to shift attention
 * WHY:  Executive controls attention allocation
 * HOW:  Creates command event, tracks with Shannon/flow
 *
 * @param adapter Executive adapter
 * @param target_neuron Target neuron for attention
 * @param attention_strength Attention weight [0-1]
 * @return true on success, false on error
 */
bool executive_adapter_send_attention_command(
    executive_middleware_adapter_t* adapter,
    uint32_t target_neuron,
    float attention_strength
);

/**
 * @brief Send task switch command to middleware
 *
 * @param adapter Executive adapter
 * @param new_task_id New task ID
 * @return true on success, false on error
 */
bool executive_adapter_send_task_switch_command(
    executive_middleware_adapter_t* adapter,
    uint32_t new_task_id
);

/**
 * @brief Send inhibition command to middleware
 *
 * @param adapter Executive adapter
 * @param target_neuron Target to inhibit
 * @param inhibition_strength Inhibition level [0-1]
 * @return true on success, false on error
 */
bool executive_adapter_send_inhibition_command(
    executive_middleware_adapter_t* adapter,
    uint32_t target_neuron,
    float inhibition_strength
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get adapter statistics
 *
 * @param adapter Executive adapter
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool executive_adapter_get_stats(
    const executive_middleware_adapter_t* adapter,
    executive_adapter_stats_t* stats
);

/**
 * @brief Reset adapter statistics
 *
 * @param adapter Executive adapter
 */
void executive_adapter_reset_stats(
    executive_middleware_adapter_t* adapter
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H
