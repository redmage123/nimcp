/**
 * @file nimcp_middleware_controller.h
 * @brief Unified middleware control interface for cognitive modules
 *
 * WHAT: Direct API for cognitive → middleware command execution
 * WHY:  Enable top-down cognitive control with <5µs latency
 * HOW:  Unified controller wrapping attention, routing, and pattern subsystems
 *
 * PHASE: 1.5.5 (Command Interface - Cognitive → Middleware)
 * SRP: Command interface only (delegates to specialized subsystems)
 *
 * ARCHITECTURE:
 *
 *   Cognitive Modules (Executive, Global Workspace, Introspection)
 *         ↓ direct API calls
 *   Middleware Controller ← Shannon Monitor (information tracking)
 *         ↓ commands
 *   ┌─────────────┬─────────────┬─────────────┐
 *   │ Attention   │ Routing     │ Pattern     │
 *   │ Gates       │ Tables      │ Library     │
 *   └─────────────┴─────────────┴─────────────┘
 *
 * COMMAND CATEGORIES:
 * 1. Attention Control - Set thresholds, priorities, focus regions
 * 2. Routing Control - Configure paths, weights, priorities
 * 3. Pattern Control - Subscribe, unsubscribe, configure matching
 * 4. Activity Control - Modulate processing intensity
 * 5. Buffer Control - Reset, configure temporal buffers
 *
 * SHANNON INTEGRATION:
 * - Each command carries information content (bits)
 * - Track mutual information between commands and middleware responses
 * - Bottleneck detection triggers adaptive command routing
 * - Command efficiency: I(command) / latency
 *
 * MATHEMATICAL FOUNDATION:
 * - Information content: I(cmd) = -log2(P(cmd)) bits
 * - Command efficiency: η = I(cmd) / τ_cmd
 * - Capacity utilization: U = Σ I(cmd_i) / C_channel
 *
 * PERFORMANCE TARGETS:
 * - Single command: <5µs
 * - Batch command: <10µs for up to 8 commands
 * - Memory overhead: <2KB per controller
 *
 * EXAMPLE:
 * ```c
 * // Create controller
 * middleware_controller_t* ctrl = middleware_controller_create(brain);
 *
 * // Set attention threshold for visual cortex
 * middleware_controller_set_attention_threshold(ctrl,
 *     TARGET_VISUAL_CORTEX, 0.7f);
 *
 * // Adjust routing priority
 * middleware_controller_set_routing_priority(ctrl,
 *     TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.9f);
 *
 * // Subscribe to pattern notifications
 * middleware_controller_subscribe_pattern(ctrl, pattern_id, 0.8f,
 *     my_callback, user_data);
 *
 * // Get controller metrics
 * middleware_controller_metrics_t metrics;
 * middleware_controller_get_metrics(ctrl, &metrics);
 * printf("Commands executed: %u, avg latency: %.2f µs\n",
 *        metrics.total_commands, metrics.avg_latency_us);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-23
 */

#ifndef NIMCP_MIDDLEWARE_CONTROLLER_H
#define NIMCP_MIDDLEWARE_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "middleware/integration/nimcp_middleware_command.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_struct* brain_t;
typedef struct attention_gate attention_gate_t;
typedef struct routing_table routing_table_t;
typedef struct pattern_library pattern_library_t;
typedef struct shannon_monitor shannon_monitor_t;

//=============================================================================
// Constants
//=============================================================================

/** Maximum commands in a batch */
#define MIDDLEWARE_CTRL_MAX_BATCH_SIZE 8

/** Maximum pattern subscriptions per controller */
#define MIDDLEWARE_CTRL_MAX_SUBSCRIPTIONS 64

/** Default attention threshold */
#define MIDDLEWARE_CTRL_DEFAULT_ATTENTION 0.5f

/** Default routing weight */
#define MIDDLEWARE_CTRL_DEFAULT_ROUTING_WEIGHT 0.5f

/** Command latency target in microseconds */
#define MIDDLEWARE_CTRL_LATENCY_TARGET_US 5.0f

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Middleware controller configuration
 */
typedef struct {
    /* Attention defaults */
    float default_attention_threshold;     /**< Default attention [0-1] */
    float attention_decay_rate;            /**< Per-step decay [0-1] */
    bool enable_adaptive_attention;        /**< Auto-adjust based on load */

    /* Routing defaults */
    float default_routing_weight;          /**< Default route weight [0-1] */
    bool enable_route_learning;            /**< Hebbian route strengthening */
    float route_learning_rate;             /**< Learning rate [0-1] */

    /* Pattern matching */
    float default_pattern_threshold;       /**< Default match threshold [0-1] */
    uint32_t max_subscriptions;            /**< Max pattern subscriptions */
    bool enable_pattern_notifications;     /**< Send match events */

    /* Performance tuning */
    bool enable_command_batching;          /**< Batch similar commands */
    uint32_t batch_timeout_us;             /**< Max batch wait time */
    bool enable_shannon_tracking;          /**< Track information flow */

    /* Safety */
    float max_activity_scale;              /**< Maximum activity multiplier */
    float min_activity_scale;              /**< Minimum activity multiplier */
} middleware_controller_config_t;

/**
 * @brief Get default controller configuration
 * @return Default configuration
 */
middleware_controller_config_t middleware_controller_default_config(void);

//=============================================================================
// Metrics
//=============================================================================

/**
 * @brief Controller performance metrics
 */
typedef struct {
    /* Command statistics */
    uint32_t total_commands;               /**< Total commands executed */
    uint32_t attention_commands;           /**< Attention control commands */
    uint32_t routing_commands;             /**< Routing control commands */
    uint32_t pattern_commands;             /**< Pattern control commands */
    uint32_t activity_commands;            /**< Activity control commands */
    uint32_t failed_commands;              /**< Commands that failed */

    /* Performance */
    float total_latency_us;                /**< Cumulative latency */
    float avg_latency_us;                  /**< Average command latency */
    float max_latency_us;                  /**< Worst-case latency */
    float min_latency_us;                  /**< Best-case latency */
    uint32_t commands_exceeding_target;    /**< Commands > 5µs */

    /* Shannon information */
    float total_information_bits;          /**< Total information delivered */
    float avg_information_per_command;     /**< Bits per command */
    float command_efficiency;              /**< Bits per microsecond */
    float channel_utilization;             /**< Fraction of capacity used */

    /* Batching */
    uint32_t batches_created;              /**< Command batches */
    float avg_batch_size;                  /**< Commands per batch */
    float batching_speedup;                /**< Time saved by batching */

    /* Subscriptions */
    uint32_t active_subscriptions;         /**< Current pattern subs */
    uint32_t pattern_notifications_sent;   /**< Match callbacks fired */
} middleware_controller_metrics_t;

//=============================================================================
// Pattern Subscription
//=============================================================================

/**
 * @brief Pattern match callback function type
 *
 * @param pattern_id Matched pattern ID
 * @param similarity Match similarity [0-1]
 * @param region_id Brain region where match occurred
 * @param user_data User-provided context
 */
typedef void (*pattern_match_callback_t)(
    uint32_t pattern_id,
    float similarity,
    uint32_t region_id,
    void* user_data
);

/**
 * @brief Pattern subscription handle
 */
typedef struct {
    uint32_t subscription_id;              /**< Unique subscription ID */
    uint32_t pattern_id;                   /**< Subscribed pattern */
    float confidence_threshold;            /**< Min confidence to notify */
    pattern_match_callback_t callback;     /**< Notification callback */
    void* user_data;                       /**< Callback context */
    uint64_t created_at_us;                /**< Creation timestamp */
    uint32_t notifications_sent;           /**< Callbacks fired */
    bool active;                           /**< Subscription active */
} pattern_subscription_t;

//=============================================================================
// Controller Handle
//=============================================================================

/**
 * @brief Opaque middleware controller handle
 */
typedef struct middleware_controller middleware_controller_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create middleware controller
 *
 * WHAT: Initialize unified middleware control interface
 * WHY:  Enable cognitive modules to control middleware behavior
 * HOW:  Connect to attention gates, routing tables, pattern library
 *
 * @param brain Brain instance (required for subsystem access)
 * @return Controller handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 * MALLOC: Yes (~2KB)
 */
middleware_controller_t* middleware_controller_create(brain_t brain);

/**
 * @brief Create controller with custom configuration
 *
 * @param brain Brain instance
 * @param config Custom configuration
 * @return Controller handle or NULL on error
 */
middleware_controller_t* middleware_controller_create_custom(
    brain_t brain,
    const middleware_controller_config_t* config
);

/**
 * @brief Destroy middleware controller
 *
 * @param controller Controller to destroy
 *
 * COMPLEXITY: O(n) where n = active subscriptions
 * THREAD-SAFE: Yes (destruction)
 */
void middleware_controller_destroy(middleware_controller_t* controller);

//=============================================================================
// Attention Control API
//=============================================================================

/**
 * @brief Set attention threshold for brain region
 *
 * WHAT: Configure minimum signal strength for attention gate
 * WHY:  Filter weak signals, focus on task-relevant inputs
 * HOW:  Update attention gate threshold parameter
 *
 * Mathematical model:
 *   attention_output = input * sigmoid((salience - threshold) / temperature)
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @param threshold Attention threshold [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_attention_threshold(
    middleware_controller_t* controller,
    command_target_region_t region,
    float threshold
);

/**
 * @brief Set attention priority for region
 *
 * WHAT: Configure relative attention weight for region
 * WHY:  Allocate more attention to task-critical regions
 * HOW:  Adjust priority in attention gate competition
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @param priority Attention priority [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_attention_priority(
    middleware_controller_t* controller,
    command_target_region_t region,
    float priority
);

/**
 * @brief Configure attention selectivity
 *
 * WHAT: Set attention spotlight size
 * WHY:  Control breadth vs depth of attention
 * HOW:  Adjust top-K parameter in attention gate
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @param selectivity Selectivity [0-1] (1=narrow focus, 0=broad)
 * @param top_k Number of channels to attend
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_attention_selectivity(
    middleware_controller_t* controller,
    command_target_region_t region,
    float selectivity,
    uint32_t top_k
);

/**
 * @brief Reset attention to default state
 *
 * WHAT: Clear all attention modifications
 * WHY:  Return to baseline for new task
 * HOW:  Reset all attention gates to defaults
 *
 * @param controller Controller handle
 * @param region Target region (TARGET_ALL_REGIONS for all)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) for all regions, O(1) for single
 * LATENCY: <10µs
 */
bool middleware_controller_reset_attention(
    middleware_controller_t* controller,
    command_target_region_t region
);

//=============================================================================
// Routing Control API
//=============================================================================

/**
 * @brief Set routing priority between regions
 *
 * WHAT: Configure route weight from source to destination
 * WHY:  Control information flow paths through brain
 * HOW:  Update routing table weight
 *
 * Mathematical model:
 *   effective_signal = signal * route_weight * attention_weight
 *
 * @param controller Controller handle
 * @param source Source region
 * @param destination Destination region
 * @param weight Routing weight [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_routing_priority(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination,
    float weight
);

/**
 * @brief Enable/disable route learning
 *
 * WHAT: Control Hebbian route strengthening
 * WHY:  Allow/prevent automatic route adaptation
 * HOW:  Toggle learning flag in routing table
 *
 * @param controller Controller handle
 * @param source Source region
 * @param destination Destination region
 * @param enable Enable learning
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_route_learning(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination,
    bool enable
);

/**
 * @brief Block route between regions
 *
 * WHAT: Disable information flow on route
 * WHY:  Prevent interference, implement inhibition
 * HOW:  Set route weight to 0
 *
 * @param controller Controller handle
 * @param source Source region
 * @param destination Destination region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_block_route(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination
);

/**
 * @brief Unblock route between regions
 *
 * WHAT: Re-enable information flow
 * WHY:  Restore routing after inhibition
 * HOW:  Set route weight to default
 *
 * @param controller Controller handle
 * @param source Source region
 * @param destination Destination region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_unblock_route(
    middleware_controller_t* controller,
    command_target_region_t source,
    command_target_region_t destination
);

//=============================================================================
// Pattern Control API
//=============================================================================

/**
 * @brief Subscribe to pattern match notifications
 *
 * WHAT: Register callback for pattern matches
 * WHY:  Get notified when specific patterns are detected
 * HOW:  Add subscription to pattern library listener
 *
 * @param controller Controller handle
 * @param pattern_id Pattern to monitor
 * @param confidence_threshold Minimum match confidence [0-1]
 * @param callback Notification callback function
 * @param user_data Context passed to callback
 * @param subscription_id Output: subscription handle
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_subscribe_pattern(
    middleware_controller_t* controller,
    uint32_t pattern_id,
    float confidence_threshold,
    pattern_match_callback_t callback,
    void* user_data,
    uint32_t* subscription_id
);

/**
 * @brief Unsubscribe from pattern notifications
 *
 * WHAT: Remove pattern subscription
 * WHY:  Stop receiving notifications for pattern
 * HOW:  Deactivate subscription entry
 *
 * @param controller Controller handle
 * @param subscription_id Subscription to cancel
 * @return true on success, false if not found
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_unsubscribe_pattern(
    middleware_controller_t* controller,
    uint32_t subscription_id
);

/**
 * @brief Set pattern matching threshold
 *
 * WHAT: Configure minimum similarity for pattern matches
 * WHY:  Control precision vs recall in pattern detection
 * HOW:  Update threshold in pattern library
 *
 * @param controller Controller handle
 * @param threshold Similarity threshold [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_pattern_threshold(
    middleware_controller_t* controller,
    float threshold
);

/**
 * @brief Get active subscription info
 *
 * @param controller Controller handle
 * @param subscription_id Subscription ID
 * @param subscription Output: subscription info
 * @return true if found, false otherwise
 */
bool middleware_controller_get_subscription(
    const middleware_controller_t* controller,
    uint32_t subscription_id,
    pattern_subscription_t* subscription
);

//=============================================================================
// Activity Control API
//=============================================================================

/**
 * @brief Scale processing activity in region
 *
 * WHAT: Multiply processing intensity by factor
 * WHY:  Increase/decrease computational resources
 * HOW:  Apply gain to neural activity
 *
 * Mathematical model:
 *   output = input * activity_scale * baseline_activity
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @param scale Activity scale factor [0.1-2.0]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_set_activity_scale(
    middleware_controller_t* controller,
    command_target_region_t region,
    float scale
);

/**
 * @brief Reduce activity (for high cognitive load)
 *
 * WHAT: Decrease processing to free resources
 * WHY:  Handle high cognitive load situations
 * HOW:  Scale activity down by 50%
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_reduce_activity(
    middleware_controller_t* controller,
    command_target_region_t region
);

/**
 * @brief Boost activity (for salient stimuli)
 *
 * WHAT: Increase processing for important signals
 * WHY:  Allocate more resources to salient inputs
 * HOW:  Scale activity up by 50%
 *
 * @param controller Controller handle
 * @param region Target brain region
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * LATENCY: <5µs
 */
bool middleware_controller_boost_activity(
    middleware_controller_t* controller,
    command_target_region_t region
);

//=============================================================================
// Buffer Control API
//=============================================================================

/**
 * @brief Reset temporal buffers
 *
 * WHAT: Clear accumulated history
 * WHY:  Start fresh for new context
 * HOW:  Reset circular buffers to empty
 *
 * @param controller Controller handle
 * @param region Target region (TARGET_ALL_REGIONS for all)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) for all regions, O(1) for single
 * LATENCY: <10µs
 */
bool middleware_controller_reset_buffers(
    middleware_controller_t* controller,
    command_target_region_t region
);

//=============================================================================
// Batch Command API
//=============================================================================

/**
 * @brief Command batch for efficient execution
 */
typedef struct {
    middleware_command_t commands[MIDDLEWARE_CTRL_MAX_BATCH_SIZE];
    uint32_t num_commands;                 /**< Commands in batch */
    float total_information_bits;          /**< Total Shannon info */
} middleware_command_batch_t;

/**
 * @brief Begin command batch
 *
 * WHAT: Start accumulating commands for batch execution
 * WHY:  Reduce overhead for multiple commands
 * HOW:  Create batch buffer
 *
 * @param controller Controller handle
 * @param batch Output: batch handle
 * @return true on success, false on error
 */
bool middleware_controller_begin_batch(
    middleware_controller_t* controller,
    middleware_command_batch_t* batch
);

/**
 * @brief Execute command batch
 *
 * WHAT: Execute all batched commands atomically
 * WHY:  Efficient multi-command execution
 * HOW:  Process all commands, return aggregate result
 *
 * @param controller Controller handle
 * @param batch Batch to execute
 * @param results Output: per-command results (optional)
 * @return Number of successful commands
 *
 * COMPLEXITY: O(n) where n = batch size
 * LATENCY: <10µs for 8 commands
 */
uint32_t middleware_controller_execute_batch(
    middleware_controller_t* controller,
    const middleware_command_batch_t* batch,
    command_execution_result_t* results
);

//=============================================================================
// Metrics and Diagnostics
//=============================================================================

/**
 * @brief Get controller metrics
 *
 * @param controller Controller handle
 * @param metrics Output: metrics structure
 * @return true on success, false on error
 */
bool middleware_controller_get_metrics(
    const middleware_controller_t* controller,
    middleware_controller_metrics_t* metrics
);

/**
 * @brief Get Shannon information efficiency
 *
 * WHAT: Calculate bits delivered per microsecond
 * WHY:  Measure command interface efficiency
 * HOW:  efficiency = total_bits / total_latency
 *
 * @param controller Controller handle
 * @return Information efficiency (bits/µs)
 */
float middleware_controller_get_efficiency(
    const middleware_controller_t* controller
);

/**
 * @brief Get average command latency
 *
 * @param controller Controller handle
 * @return Average latency in microseconds
 */
float middleware_controller_get_avg_latency(
    const middleware_controller_t* controller
);

/**
 * @brief Check if meeting performance targets
 *
 * @param controller Controller handle
 * @return true if avg latency < 5µs
 */
bool middleware_controller_is_performant(
    const middleware_controller_t* controller
);

/**
 * @brief Reset controller statistics
 *
 * @param controller Controller handle
 */
void middleware_controller_reset_stats(
    middleware_controller_t* controller
);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Enable/disable Shannon information tracking
 *
 * @param controller Controller handle
 * @param enable Enable tracking
 */
void middleware_controller_enable_shannon_tracking(
    middleware_controller_t* controller,
    bool enable
);

/**
 * @brief Enable/disable command batching
 *
 * @param controller Controller handle
 * @param enable Enable batching
 */
void middleware_controller_enable_batching(
    middleware_controller_t* controller,
    bool enable
);

/**
 * @brief Set activity scale limits
 *
 * @param controller Controller handle
 * @param min_scale Minimum scale [0.1-1.0]
 * @param max_scale Maximum scale [1.0-2.0]
 */
void middleware_controller_set_activity_limits(
    middleware_controller_t* controller,
    float min_scale,
    float max_scale
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to Shannon monitor for information tracking
 *
 * @param controller Controller handle
 * @param monitor Shannon monitor instance
 * @return true on success, false on error
 */
bool middleware_controller_connect_shannon(
    middleware_controller_t* controller,
    shannon_monitor_t* monitor
);

/**
 * @brief Notify controller of pattern match (from pattern library)
 *
 * WHAT: Internal callback for pattern library integration
 * WHY:  Forward matches to subscribed callbacks
 * HOW:  Check subscriptions, fire matching callbacks
 *
 * @param controller Controller handle
 * @param pattern_id Matched pattern
 * @param similarity Match similarity
 * @param region_id Region where match occurred
 */
void middleware_controller_on_pattern_match(
    middleware_controller_t* controller,
    uint32_t pattern_id,
    float similarity,
    uint32_t region_id
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIDDLEWARE_CONTROLLER_H
