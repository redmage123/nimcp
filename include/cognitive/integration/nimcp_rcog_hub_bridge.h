/**
 * @file nimcp_rcog_hub_bridge.h
 * @brief Recursive Cognition - Cognitive Integration Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting recursive cognition engine to the cognitive integration hub
 * WHY:  Enables rcog to participate in cross-module cognitive events: subscribe to
 *       input/memory/attention events, publish recursion lifecycle events, and
 *       handle queries about recursive processing state.
 * HOW:  Registers rcog as COG_CATEGORY_REASONING module, subscribes to relevant
 *       event types, publishes recursion start/complete/subtask events, and
 *       provides query handlers for rcog state inspection.
 *
 * BIOLOGICAL BASIS:
 * - Models prefrontal cortex connections to other brain regions
 * - Goal-directed behavior requires integration with memory and attention
 * - Executive function coordinates with sensory and motor systems
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_INPUT_RECEIVED for new goals
 * - Subscribe: COG_EVENT_MEMORY_ACCESS for context during decomposition
 * - Subscribe: COG_EVENT_ATTENTION_SHIFT for priority changes
 * - Publish: COG_EVENT_OUTPUT_READY when answer refined
 * - Query: Expose rcog processing state to other modules
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RCOG_HUB_BRIDGE_H
#define NIMCP_RCOG_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum recursion events to buffer */
#define RCOG_HUB_MAX_EVENT_BUFFER       64

/** Default module ID for rcog in the hub */
#define RCOG_HUB_DEFAULT_MODULE_ID      0x52434F47  /* "RCOG" */

/** Maximum subscribed event types */
#define RCOG_HUB_MAX_SUBSCRIPTIONS      16

/* ============================================================================
 * Recursion Event Types (extensions to cognitive event types)
 * ============================================================================ */

/**
 * @brief Recursion-specific event types for hub communication
 *
 * WHAT: Events specific to recursive cognition processing
 * WHY:  Inform other modules about rcog lifecycle stages
 * HOW:  Published through cognitive hub with specific payloads
 */
typedef enum {
    RCOG_HUB_EVENT_RECURSION_START = 0,     /**< Beginning task processing */
    RCOG_HUB_EVENT_RECURSION_COMPLETE,      /**< Task processing finished */
    RCOG_HUB_EVENT_SUBTASK_SPAWNED,         /**< New subtask created */
    RCOG_HUB_EVENT_SUBTASK_COMPLETE,        /**< Subtask finished */
    RCOG_HUB_EVENT_DEPTH_INCREASED,         /**< Recursion went deeper */
    RCOG_HUB_EVENT_ANSWER_REFINED,          /**< Answer improved */
    RCOG_HUB_EVENT_CONFIDENCE_CHANGED,      /**< Confidence level updated */
    RCOG_HUB_EVENT_STRATEGY_CHANGED,        /**< Decomposition strategy changed */
    RCOG_HUB_EVENT_COUNT
} rcog_hub_event_type_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct rcog_hub_bridge rcog_hub_bridge_t;

/* Forward declare external types */
struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;
struct rcog_engine;
typedef struct rcog_engine rcog_engine_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for rcog hub bridge
 */
typedef struct {
    uint32_t module_id;                  /**< Module ID for hub registration */
    bool auto_subscribe_input;           /**< Subscribe to INPUT_RECEIVED */
    bool auto_subscribe_memory;          /**< Subscribe to MEMORY_ACCESS */
    bool auto_subscribe_attention;       /**< Subscribe to ATTENTION_SHIFT */
    bool publish_subtask_events;         /**< Publish individual subtask events */
    bool enable_query_handler;           /**< Register query handler */
    uint32_t event_buffer_size;          /**< Size of internal event buffer */
} rcog_hub_bridge_config_t;

/**
 * @brief Statistics for rcog hub bridge operations
 */
typedef struct {
    uint32_t events_received;            /**< Total events received from hub */
    uint32_t events_published;           /**< Total events published to hub */
    uint32_t queries_handled;            /**< Total queries handled */
    uint32_t recursions_triggered;       /**< Recursions triggered by events */
    uint32_t subtasks_spawned;           /**< Subtasks spawned (reported) */
    uint32_t goals_from_input_events;    /**< Goals from INPUT_RECEIVED */
    uint32_t priority_changes;           /**< Priority changes from attention */
    uint32_t context_updates;            /**< Context updates from memory */
    uint64_t last_event_timestamp;       /**< Timestamp of last event */
} rcog_hub_bridge_stats_t;

/**
 * @brief Payload for recursion start events
 */
typedef struct {
    uint64_t goal_id;                    /**< Unique goal identifier */
    uint32_t goal_type;                  /**< Goal type enum value */
    uint32_t max_depth;                  /**< Maximum recursion depth */
    float priority;                      /**< Goal priority */
    uint64_t timestamp;                  /**< Event timestamp */
} rcog_hub_recursion_start_payload_t;

/**
 * @brief Payload for recursion complete events
 */
typedef struct {
    uint64_t goal_id;                    /**< Goal identifier */
    bool success;                        /**< Whether processing succeeded */
    float final_confidence;              /**< Final answer confidence */
    uint32_t subtasks_total;             /**< Total subtasks created */
    uint32_t subtasks_completed;         /**< Subtasks completed successfully */
    uint32_t max_depth_reached;          /**< Maximum depth reached */
    uint64_t processing_time_ms;         /**< Total processing time */
    uint64_t timestamp;                  /**< Event timestamp */
} rcog_hub_recursion_complete_payload_t;

/**
 * @brief Payload for subtask spawned events
 */
typedef struct {
    uint64_t parent_goal_id;             /**< Parent goal identifier */
    uint64_t subtask_id;                 /**< Subtask identifier */
    uint32_t current_depth;              /**< Current recursion depth */
    uint32_t subtask_type;               /**< Type of subtask */
    float priority;                      /**< Subtask priority */
    uint64_t timestamp;                  /**< Event timestamp */
} rcog_hub_subtask_spawned_payload_t;

/**
 * @brief Callback for input events received from hub
 *
 * @param goal_query Goal query string from event
 * @param goal_type Suggested goal type
 * @param priority Event priority
 * @param user_data User context
 * @return 0 on success, -1 to reject goal
 */
typedef int (*rcog_hub_input_callback_t)(
    const char* goal_query,
    uint32_t goal_type,
    float priority,
    void* user_data
);

/**
 * @brief Callback for attention shift events
 *
 * @param new_focus_id ID of new attention focus
 * @param old_focus_id ID of previous focus
 * @param urgency Urgency level of shift
 * @param user_data User context
 */
typedef void (*rcog_hub_attention_callback_t)(
    uint64_t new_focus_id,
    uint64_t old_focus_id,
    float urgency,
    void* user_data
);

/**
 * @brief Callback for memory access events
 *
 * @param memory_id Accessed memory identifier
 * @param access_type Type of access (read/write)
 * @param relevance Relevance to current processing
 * @param user_data User context
 */
typedef void (*rcog_hub_memory_callback_t)(
    uint64_t memory_id,
    uint32_t access_type,
    float relevance,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default rcog hub bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard settings
 * HOW:  Set auto-subscribe options and buffer sizes
 *
 * DEFAULT VALUES:
 * - module_id: RCOG_HUB_DEFAULT_MODULE_ID
 * - auto_subscribe_input: true
 * - auto_subscribe_memory: true
 * - auto_subscribe_attention: true
 * - publish_subtask_events: false (can be noisy)
 * - enable_query_handler: true
 * - event_buffer_size: RCOG_HUB_MAX_EVENT_BUFFER
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int rcog_hub_bridge_default_config(rcog_hub_bridge_config_t* config);

/**
 * @brief Create rcog hub bridge
 *
 * WHAT: Initialize bridge for rcog-hub integration
 * WHY:  Enable rcog to participate in cognitive event system
 * HOW:  Allocate bridge, initialize state, prepare callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
rcog_hub_bridge_t* rcog_hub_bridge_create(
    const rcog_hub_bridge_config_t* config
);

/**
 * @brief Destroy rcog hub bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from hub, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTES: Automatically disconnects from hub if connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void rcog_hub_bridge_destroy(rcog_hub_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to cognitive hub and rcog engine
 *
 * WHAT: Register rcog with hub and establish bidirectional connection
 * WHY:  Enable event subscription/publication and query handling
 * HOW:  Register module, subscribe to configured events, set up handlers
 *
 * @param bridge Rcog hub bridge
 * @param hub Cognitive integration hub
 * @param engine Rcog engine (may be NULL if not yet created)
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge or hub is NULL
 * - Returns -1 if already connected
 * - Returns -1 if hub registration fails
 *
 * SIDE EFFECTS:
 * - Registers rcog as COG_CATEGORY_REASONING module
 * - Subscribes to configured event types
 * - Registers query handler if enabled
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int rcog_hub_bridge_connect(
    rcog_hub_bridge_t* bridge,
    cognitive_integration_hub_t hub,
    rcog_engine_t* engine
);

/**
 * @brief Disconnect bridge from cognitive hub
 *
 * WHAT: Unregister rcog from hub and cleanup subscriptions
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unsubscribe from events, unregister module
 *
 * @param bridge Rcog hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int rcog_hub_bridge_disconnect(rcog_hub_bridge_t* bridge);

/**
 * @brief Update engine reference
 *
 * WHAT: Update the rcog engine pointer after engine creation
 * WHY:  Bridge may be created before engine
 * HOW:  Atomic update of engine reference
 *
 * @param bridge Rcog hub bridge
 * @param engine New rcog engine reference
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int rcog_hub_bridge_set_engine(
    rcog_hub_bridge_t* bridge,
    rcog_engine_t* engine
);

/**
 * @brief Check if bridge is connected to hub
 *
 * @param bridge Rcog hub bridge
 * @return true if connected, false otherwise
 */
bool rcog_hub_bridge_is_connected(rcog_hub_bridge_t* bridge);

/* ============================================================================
 * Event Callback Registration
 * ============================================================================ */

/**
 * @brief Set callback for input events
 *
 * WHAT: Register handler for COG_EVENT_INPUT_RECEIVED
 * WHY:  Process new goals from other cognitive modules
 * HOW:  Store callback, invoke on input events
 *
 * @param bridge Rcog hub bridge
 * @param callback Input event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int rcog_hub_bridge_set_input_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_input_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for attention shift events
 *
 * WHAT: Register handler for COG_EVENT_ATTENTION_SHIFT
 * WHY:  Adjust processing priorities based on attention
 * HOW:  Store callback, invoke on attention events
 *
 * @param bridge Rcog hub bridge
 * @param callback Attention event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int rcog_hub_bridge_set_attention_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_attention_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for memory access events
 *
 * WHAT: Register handler for COG_EVENT_MEMORY_ACCESS
 * WHY:  Update context based on memory operations
 * HOW:  Store callback, invoke on memory events
 *
 * @param bridge Rcog hub bridge
 * @param callback Memory event callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int rcog_hub_bridge_set_memory_callback(
    rcog_hub_bridge_t* bridge,
    rcog_hub_memory_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Event Publication API
 * ============================================================================ */

/**
 * @brief Publish recursion start event
 *
 * WHAT: Notify hub that rcog is beginning task processing
 * WHY:  Allow other modules to prepare/respond to recursion
 * HOW:  Create event with payload, publish through hub
 *
 * @param bridge Rcog hub bridge
 * @param goal_id Unique goal identifier
 * @param goal_type Goal type enum value
 * @param max_depth Maximum recursion depth
 * @param priority Goal priority
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int rcog_hub_publish_recursion_start(
    rcog_hub_bridge_t* bridge,
    uint64_t goal_id,
    uint32_t goal_type,
    uint32_t max_depth,
    float priority
);

/**
 * @brief Publish recursion complete event
 *
 * WHAT: Notify hub that rcog has finished task processing
 * WHY:  Allow other modules to process results
 * HOW:  Create event with result payload, publish through hub
 *
 * @param bridge Rcog hub bridge
 * @param goal_id Goal identifier
 * @param success Whether processing succeeded
 * @param final_confidence Final answer confidence
 * @param subtasks_total Total subtasks created
 * @param subtasks_completed Subtasks completed successfully
 * @param max_depth_reached Maximum depth reached
 * @param processing_time_ms Total processing time
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int rcog_hub_publish_recursion_complete(
    rcog_hub_bridge_t* bridge,
    uint64_t goal_id,
    bool success,
    float final_confidence,
    uint32_t subtasks_total,
    uint32_t subtasks_completed,
    uint32_t max_depth_reached,
    uint64_t processing_time_ms
);

/**
 * @brief Publish subtask spawned event
 *
 * WHAT: Notify hub that a new subtask was created
 * WHY:  Allow other modules to track decomposition
 * HOW:  Create event with subtask info, publish through hub
 *
 * @param bridge Rcog hub bridge
 * @param parent_goal_id Parent goal identifier
 * @param subtask_id New subtask identifier
 * @param current_depth Current recursion depth
 * @param subtask_type Type of subtask
 * @param priority Subtask priority
 * @return 0 on success, -1 on error
 *
 * NOTES: Only published if config.publish_subtask_events is true
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int rcog_hub_publish_subtask_spawned(
    rcog_hub_bridge_t* bridge,
    uint64_t parent_goal_id,
    uint64_t subtask_id,
    uint32_t current_depth,
    uint32_t subtask_type,
    float priority
);

/**
 * @brief Publish generic recursion event
 *
 * WHAT: Publish any rcog-specific event type
 * WHY:  Flexibility for custom event types
 * HOW:  Create event with provided payload, publish through hub
 *
 * @param bridge Rcog hub bridge
 * @param event_type Rcog event type
 * @param payload Event payload (type-specific)
 * @param payload_size Size of payload in bytes
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int rcog_hub_publish_recursion_event(
    rcog_hub_bridge_t* bridge,
    rcog_hub_event_type_t event_type,
    const void* payload,
    size_t payload_size
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Query current recursion state
 *
 * WHAT: Get current state of recursive processing
 * WHY:  Allow other modules to inspect rcog state
 * HOW:  Read state from connected engine
 *
 * @param bridge Rcog hub bridge
 * @param active_goals Output: number of active goals
 * @param current_depth Output: current maximum depth
 * @param avg_confidence Output: average confidence
 * @return 0 on success, -1 on error
 */
int rcog_hub_bridge_get_state(
    const rcog_hub_bridge_t* bridge,
    uint32_t* active_goals,
    uint32_t* current_depth,
    float* avg_confidence
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational statistics
 * WHY:  Monitor bridge performance and activity
 * HOW:  Copy current stats to output struct
 *
 * @param bridge Rcog hub bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int rcog_hub_bridge_get_stats(
    const rcog_hub_bridge_t* bridge,
    rcog_hub_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat counters
 *
 * @param bridge Rcog hub bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int rcog_hub_bridge_reset_stats(rcog_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_HUB_BRIDGE_H */
