/**
 * @file nimcp_executive_middleware_adapter.h
 * @brief Executive controller → middleware event adapter
 *
 * WHAT: Bidirectional integration between executive and middleware layers
 * WHY:  Enable top-down cognitive control of middleware processing
 * HOW:  Event handlers convert executive decisions to middleware commands
 *
 * PHASE: 1.5.2 (Executive Integration)
 * SRP: Event routing and adaptation only (no command execution)
 *
 * ARCHITECTURE:
 *
 *   Executive Controller (cognitive/executive)
 *         ↓ events (pattern detected, task switched, etc.)
 *   Executive Middleware Adapter ← Shannon Monitor (information tracking)
 *         ↓ middleware commands
 *   Quantum Command Propagator
 *         ↓ quantum walk
 *   Brain Regions (neurons receive commands)
 *
 * EVENT FLOW:
 * 1. Executive detects pattern → fires event
 * 2. Adapter receives event → converts to middleware command
 * 3. Shannon monitor calculates information content
 * 4. Adapter sends command to quantum propagator
 * 5. Quantum propagator distributes to brain regions
 *
 * SHANNON INTEGRATION:
 * - Measure information content of each command
 * - Track mutual information between executive decisions and middleware responses
 * - Detect bottlenecks in executive→middleware channel
 * - Adapt command routing based on capacity
 *
 * EXAMPLE:
 * ```c
 * // Create adapter
 * executive_middleware_adapter_t* adapter = executive_middleware_adapter_create(
 *     executive, qcp, shannon_monitor
 * );
 *
 * // Register event handlers
 * executive_middleware_adapter_register_handlers(adapter);
 *
 * // Executive fires pattern detected event → adapter converts to command
 * // ... (happens automatically via event bus)
 *
 * // Get integration metrics
 * executive_middleware_metrics_t metrics;
 * executive_middleware_adapter_get_metrics(adapter, &metrics);
 * printf("Commands issued: %u\n", metrics.total_commands_issued);
 * printf("Information delivered: %.2f bits\n", metrics.total_information_delivered);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#ifndef NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H
#define NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "middleware/integration/nimcp_middleware_command.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct executive_controller executive_controller_t;
typedef struct quantum_command_propagator quantum_command_propagator_t;

// Use event_bus_t from core events header (typedef struct event_bus_internal* event_bus_t)
// Note: Must match the typedef in core/events/nimcp_event_bus.h
typedef struct event_bus_internal* event_bus_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Executive middleware adapter configuration
 */
typedef struct {
    bool enable_adaptive_routing;            /**< Adapt routing based on Shannon feedback */
    float command_priority_threshold;        /**< Min priority to issue command [0-1] */
    float information_threshold_bits;        /**< Min information to propagate command */
    bool enable_command_batching;            /**< Batch similar commands */
    uint32_t batch_window_ms;                /**< Time window for batching */
    bool enable_bottleneck_avoidance;        /**< Reroute commands around bottlenecks */
} executive_middleware_config_t;

/**
 * @brief Get default executive middleware adapter configuration
 *
 * @return Default configuration
 */
executive_middleware_config_t executive_middleware_adapter_default_config(void);

//=============================================================================
// Integration Metrics
//=============================================================================

/**
 * @brief Executive-middleware integration metrics
 */
typedef struct {
    // Command statistics
    uint32_t total_commands_issued;          /**< Total commands sent */
    uint32_t total_commands_executed;        /**< Commands successfully executed */
    uint32_t total_commands_failed;          /**< Commands that failed */
    float command_success_rate;              /**< Executed / issued [0-1] */

    // Event handling
    uint32_t total_events_received;          /**< Events from executive */
    uint32_t total_events_converted;         /**< Events converted to commands */
    float event_conversion_rate;             /**< Converted / received [0-1] */

    // Shannon information
    float total_information_delivered;       /**< Bits delivered to middleware */
    float average_information_per_command;   /**< Bits per command */
    float information_delivery_rate;         /**< Bits per second */
    float mutual_information_exec_mw;        /**< I(executive;middleware) bits */

    // Performance
    uint64_t total_adaptation_time_us;       /**< Time spent in adapter */
    float average_adaptation_latency_us;     /**< Average conversion time */

    // Bottleneck tracking
    uint32_t bottlenecks_detected;           /**< Shannon bottlenecks found */
    uint32_t commands_rerouted;              /**< Commands rerouted */
    uint32_t commands_dropped;               /**< Commands dropped (low info) */

    // Batching statistics
    uint32_t batches_created;                /**< Number of batches */
    float average_batch_size;                /**< Commands per batch */
    float batching_efficiency;               /**< Space saved by batching */
} executive_middleware_metrics_t;

//=============================================================================
// Event Handler Types
//=============================================================================

/**
 * @brief Event types from executive controller
 */
typedef enum {
    EXECUTIVE_EVENT_TASK_SWITCHED,       /**< Task switching occurred */
    EXECUTIVE_EVENT_TASK_COMPLETED,      /**< Task completed */
    EXECUTIVE_EVENT_INHIBITION_TRIGGERED,/**< Response inhibited */
    EXECUTIVE_EVENT_PLAN_CREATED,        /**< New plan generated */
    EXECUTIVE_EVENT_PRIORITY_CHANGED,    /**< Task priority changed */
    EXECUTIVE_EVENT_COGNITIVE_LOAD_HIGH, /**< High cognitive load detected */
    EXECUTIVE_EVENT_COGNITIVE_LOAD_LOW,  /**< Low cognitive load detected */
    EXECUTIVE_EVENT_CUSTOM               /**< Custom event */
} executive_event_type_t;

/**
 * @brief Middleware pattern event types (from pattern detectors)
 */
typedef enum {
    MIDDLEWARE_EVENT_PATTERN_DETECTED,   /**< Pattern recognized */
    MIDDLEWARE_EVENT_OSCILLATION_CHANGE, /**< Oscillation frequency changed */
    MIDDLEWARE_EVENT_SALIENCE_PEAK,      /**< High salience detected */
    MIDDLEWARE_EVENT_SEQUENCE_COMPLETE,  /**< Sequence pattern completed */
    MIDDLEWARE_EVENT_SYNCHRONY_DETECTED, /**< Neural synchrony detected */
    MIDDLEWARE_EVENT_CUSTOM              /**< Custom event */
} middleware_event_type_t;

//=============================================================================
// Executive Middleware Adapter
//=============================================================================

/**
 * @brief Opaque executive middleware adapter handle
 */
typedef struct executive_middleware_adapter executive_middleware_adapter_t;

/**
 * @brief Create executive middleware adapter
 *
 * WHAT: Initialize executive→middleware integration
 * WHY:  Enable cognitive control of middleware processing
 * HOW:  Connects executive controller, command propagator, Shannon monitor
 *
 * @param executive Executive controller
 * @param propagator Quantum command propagator
 * @param shannon_monitor Shannon monitor for information tracking (can be NULL)
 * @return Adapter handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 * MALLOC: Yes (adapter structure)
 */
executive_middleware_adapter_t* executive_middleware_adapter_create(
    executive_controller_t* executive,
    quantum_command_propagator_t* propagator,
    shannon_monitor_t* shannon_monitor
);

/**
 * @brief Create executive middleware adapter with custom config
 *
 * @param executive Executive controller
 * @param propagator Quantum command propagator
 * @param shannon_monitor Shannon monitor (can be NULL)
 * @param config Custom configuration
 * @return Adapter handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 */
executive_middleware_adapter_t* executive_middleware_adapter_create_custom(
    executive_controller_t* executive,
    quantum_command_propagator_t* propagator,
    shannon_monitor_t* shannon_monitor,
    const executive_middleware_config_t* config
);

/**
 * @brief Destroy executive middleware adapter
 *
 * @param adapter Adapter to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (destruction)
 */
void executive_middleware_adapter_destroy(executive_middleware_adapter_t* adapter);

//=============================================================================
// Event Handler Registration
//=============================================================================

/**
 * @brief Register all event handlers with event bus
 *
 * WHAT: Subscribe to executive and middleware events
 * WHY:  Enable automatic event-to-command conversion
 * HOW:  Registers handlers for each event type
 *
 * Event handlers registered:
 * - executive_on_task_switched → COMMAND_CONFIGURE_ATTENTION
 * - executive_on_cognitive_load_high → COMMAND_REDUCE_ACTIVITY
 * - middleware_on_pattern_detected → COMMAND_SUBSCRIBE_PATTERN
 * - middleware_on_oscillation_change → COMMAND_ADJUST_ROUTING
 * - middleware_on_salience_peak → COMMAND_INCREASE_ACTIVITY
 *
 * @param adapter Executive middleware adapter
 * @param event_bus Event bus to register with
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_register_handlers(
    executive_middleware_adapter_t* adapter,
    event_bus_t event_bus
);

/**
 * @brief Unregister all event handlers
 *
 * @param adapter Executive middleware adapter
 * @param event_bus Event bus to unregister from
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_middleware_adapter_unregister_handlers(
    executive_middleware_adapter_t* adapter,
    event_bus_t event_bus
);

//=============================================================================
// Manual Event Handling (for testing or custom integration)
//=============================================================================

/**
 * @brief Handle executive task switched event
 *
 * WHAT: Convert task switch to attention reconfiguration
 * WHY:  New task may need different attention settings
 * HOW:  Creates COMMAND_CONFIGURE_ATTENTION based on task type
 *
 * @param adapter Executive middleware adapter
 * @param task_id New task ID
 * @param task_type Task type
 * @param priority Task priority
 * @return true on success, false on error
 *
 * COMPLEXITY: O(√N) (quantum propagation)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_on_task_switched(
    executive_middleware_adapter_t* adapter,
    uint32_t task_id,
    uint32_t task_type,
    float priority
);

/**
 * @brief Handle cognitive load change event
 *
 * WHAT: Adjust middleware activity based on cognitive load
 * WHY:  High load → reduce middleware processing to free resources
 * HOW:  Creates COMMAND_REDUCE_ACTIVITY or COMMAND_INCREASE_ACTIVITY
 *
 * @param adapter Executive middleware adapter
 * @param cognitive_load Load level [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(√N)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_on_cognitive_load_changed(
    executive_middleware_adapter_t* adapter,
    float cognitive_load
);

/**
 * @brief Handle pattern detected event
 *
 * WHAT: Subscribe to interesting patterns
 * WHY:  Executive may want to monitor specific patterns
 * HOW:  Creates COMMAND_SUBSCRIBE_PATTERN
 *
 * @param adapter Executive middleware adapter
 * @param pattern_id Pattern that was detected
 * @param confidence Detection confidence [0-1]
 * @param target_region Brain region where pattern was found
 * @return true on success, false on error
 *
 * COMPLEXITY: O(√N)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_on_pattern_detected(
    executive_middleware_adapter_t* adapter,
    uint32_t pattern_id,
    float confidence,
    uint32_t target_region
);

/**
 * @brief Handle oscillation change event
 *
 * WHAT: Adjust routing based on oscillatory state
 * WHY:  Different oscillations need different routing strategies
 * HOW:  Creates COMMAND_ADJUST_ROUTING
 *
 * @param adapter Executive middleware adapter
 * @param frequency_hz New oscillation frequency
 * @param power Oscillation power
 * @param target_region Brain region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(√N)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_on_oscillation_changed(
    executive_middleware_adapter_t* adapter,
    float frequency_hz,
    float power,
    uint32_t target_region
);

/**
 * @brief Handle salience peak event
 *
 * WHAT: Boost activity in region with high salience
 * WHY:  Salient stimuli deserve more processing resources
 * HOW:  Creates COMMAND_INCREASE_ACTIVITY
 *
 * @param adapter Executive middleware adapter
 * @param salience Salience level [0-1]
 * @param target_region Brain region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(√N)
 * THREAD-SAFE: No
 */
bool executive_middleware_adapter_on_salience_peak(
    executive_middleware_adapter_t* adapter,
    float salience,
    uint32_t target_region
);

//=============================================================================
// Metrics and Diagnostics
//=============================================================================

/**
 * @brief Get executive-middleware integration metrics
 *
 * @param adapter Executive middleware adapter
 * @param metrics Output metrics structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (returns copy)
 */
bool executive_middleware_adapter_get_metrics(
    const executive_middleware_adapter_t* adapter,
    executive_middleware_metrics_t* metrics
);

/**
 * @brief Get mutual information between executive and middleware
 *
 * WHAT: Measure how well executive decisions inform middleware
 * WHY:  High I(X;Y) = effective integration
 * HOW:  I(exec;mw) = H(exec) + H(mw) - H(exec,mw)
 *
 * @param adapter Executive middleware adapter
 * @return Mutual information in bits
 *
 * COMPLEXITY: O(1) (cached)
 * THREAD-SAFE: Yes (read-only)
 */
float executive_middleware_adapter_get_mutual_information(
    const executive_middleware_adapter_t* adapter
);

/**
 * @brief Get command success rate
 *
 * @param adapter Executive middleware adapter
 * @return Success rate [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
float executive_middleware_adapter_get_success_rate(
    const executive_middleware_adapter_t* adapter
);

/**
 * @brief Reset adapter statistics
 *
 * @param adapter Executive middleware adapter
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void executive_middleware_adapter_reset_stats(
    executive_middleware_adapter_t* adapter
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Enable adaptive routing based on Shannon feedback
 *
 * WHAT: Use Shannon bottleneck detection to adapt command routing
 * WHY:  Avoid congested paths, improve information delivery
 * HOW:  Consults Shannon monitor before issuing commands
 *
 * @param adapter Executive middleware adapter
 * @param enable true to enable, false to disable
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void executive_middleware_adapter_enable_adaptive_routing(
    executive_middleware_adapter_t* adapter,
    bool enable
);

/**
 * @brief Set command priority threshold
 *
 * WHAT: Minimum priority to issue command
 * WHY:  Filter low-priority commands to reduce load
 * HOW:  Only issue commands with priority ≥ threshold
 *
 * @param adapter Executive middleware adapter
 * @param threshold Priority threshold [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void executive_middleware_adapter_set_priority_threshold(
    executive_middleware_adapter_t* adapter,
    float threshold
);

/**
 * @brief Set information threshold for command propagation
 *
 * WHAT: Minimum Shannon information to propagate command
 * WHY:  Drop low-information commands to save bandwidth
 * HOW:  Only propagate commands with I ≥ threshold bits
 *
 * @param adapter Executive middleware adapter
 * @param threshold_bits Information threshold in bits
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex protected)
 */
void executive_middleware_adapter_set_information_threshold(
    executive_middleware_adapter_t* adapter,
    float threshold_bits
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EXECUTIVE_MIDDLEWARE_ADAPTER_H
