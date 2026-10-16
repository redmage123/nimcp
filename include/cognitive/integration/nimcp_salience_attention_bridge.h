/**
 * @file nimcp_salience_attention_bridge.h
 * @brief Salience-Attention Cognitive Integration Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting salience detection to attention allocation via the cognitive hub
 * WHY:  Enable bidirectional communication between salience and attention modules,
 *       allowing salient stimuli to capture attention and attention to modulate salience
 * HOW:  Registers with cognitive hub, subscribes to relevant events, publishes
 *       salience detections and attention shift requests, handles queries
 *
 * BIOLOGICAL BASIS:
 * - Models bottom-up attention capture by salient stimuli (superior colliculus)
 * - Models top-down attention modulation of salience sensitivity (prefrontal cortex)
 * - Priority maps integrate bottom-up salience with top-down goals (parietal cortex)
 * - Bidirectional feedback between salience and attention circuits
 *
 * INTEGRATION PATTERNS:
 * - Subscribe: COG_EVENT_ATTENTION_SHIFT for attention focus changes
 * - Subscribe: COG_EVENT_STATE_CHANGE for salience updates
 * - Publish: Salience detections to drive attention
 * - Publish: Attention focus notifications
 * - Query: Expose salience/attention state to other modules
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SALIENCE_ATTENTION_BRIDGE_H
#define NIMCP_SALIENCE_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum items in a salience evaluation request */
#define SALIENCE_ATTENTION_MAX_ITEMS        32

/** Maximum priorities in an update */
#define SALIENCE_ATTENTION_MAX_PRIORITIES   16

/** Default module ID for salience-attention bridge in the hub */
#define SALIENCE_ATTENTION_DEFAULT_MODULE_ID  0x53414C41  /* "SALA" */

/** Default salience threshold for attention capture */
#define SALIENCE_ATTENTION_DEFAULT_THRESHOLD  0.7f

/* ============================================================================
 * Event Types (extensions to cognitive event types)
 * ============================================================================ */

/**
 * @brief Salience-attention specific event types
 *
 * WHAT: Events specific to salience-attention communication
 * WHY:  Inform attention system about salient items and vice versa
 * HOW:  Published through cognitive hub with specific payloads
 */
typedef enum {
    SA_EVENT_SALIENCE_DETECTED = 0,     /**< Salient item detected */
    SA_EVENT_ATTENTION_REQUESTED,       /**< Attention shift requested */
    SA_EVENT_PRIORITY_UPDATED,          /**< Attention priorities updated */
    SA_EVENT_FOCUS_CHANGED,             /**< Attention focus changed */
    SA_EVENT_EVALUATION_REQUESTED,      /**< Salience evaluation requested */
    SA_EVENT_EVALUATION_COMPLETE,       /**< Salience evaluation complete */
    SA_EVENT_COUNT
} salience_attention_event_type_t;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct salience_attention_bridge salience_attention_bridge_t;

/* Forward declare external types */
struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;
struct salience_evaluator_struct;
typedef struct salience_evaluator_struct* salience_evaluator_t;
struct multihead_attention_struct;
typedef struct multihead_attention_struct* multihead_attention_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for salience-attention bridge
 */
typedef struct {
    uint32_t module_id;                  /**< Module ID for hub registration */
    bool enable_logging;                 /**< Enable debug logging */
    float salience_threshold;            /**< Threshold for attention capture (0-1) */
    float attention_shift_weight;        /**< Weight for attention shift requests (0-1) */
    float priority_weight;               /**< Weight for priority updates (0-1) */
    bool auto_subscribe_attention;       /**< Subscribe to ATTENTION_SHIFT events */
    bool auto_subscribe_state;           /**< Subscribe to STATE_CHANGE events */
    bool enable_query_handler;           /**< Register query handler */
} salience_attention_config_t;

/**
 * @brief Statistics for salience-attention bridge operations
 */
typedef struct {
    uint64_t total_events;               /**< Total events processed */
    uint64_t salience_detections;        /**< Salience detections published */
    uint64_t attention_shifts;           /**< Attention shift requests made */
    uint64_t priority_updates;           /**< Priority updates published */
    uint64_t focus_notifications;        /**< Focus change notifications */
    uint64_t evaluation_requests;        /**< Salience evaluation requests */
    uint64_t events_received;            /**< Events received from hub */
    uint64_t events_published;           /**< Events published to hub */
    uint64_t queries_handled;            /**< Queries handled */
    float avg_salience_score;            /**< Average salience score detected */
    float avg_attention_strength;        /**< Average attention strength */
    uint64_t last_event_timestamp;       /**< Timestamp of last event */
} salience_attention_stats_t;

/**
 * @brief Salient item information
 */
typedef struct {
    uint64_t item_id;                    /**< Unique item identifier */
    float salience_score;                /**< Overall salience score (0-1) */
    float novelty;                       /**< Novelty score (0-1) */
    float surprise;                      /**< Surprise score (0-1) */
    float urgency;                       /**< Urgency score (0-1) */
    uint32_t modality;                   /**< Sensory modality (visual, audio, etc.) */
    uint64_t timestamp;                  /**< Detection timestamp */
} salient_item_t;

/**
 * @brief Attention target information
 */
typedef struct {
    uint64_t target_id;                  /**< Target identifier */
    float priority;                      /**< Target priority (0-1) */
    float urgency;                       /**< Urgency of shift (0-1) */
    uint32_t modality;                   /**< Target modality */
    uint64_t timestamp;                  /**< Request timestamp */
} attention_target_t;

/**
 * @brief Attention priority entry
 */
typedef struct {
    uint64_t item_id;                    /**< Item identifier */
    float priority;                      /**< Priority value (0-1) */
} attention_priority_t;

/**
 * @brief Attention focus information
 */
typedef struct {
    uint64_t focus_id;                   /**< Current focus identifier */
    uint64_t previous_focus_id;          /**< Previous focus identifier */
    float focus_strength;                /**< Focus strength (0-1) */
    float duration_ms;                   /**< Focus duration in milliseconds */
    uint64_t timestamp;                  /**< Focus change timestamp */
} attention_focus_t;

/**
 * @brief Salience evaluation request
 */
typedef struct {
    uint64_t request_id;                 /**< Request identifier */
    uint64_t item_ids[SALIENCE_ATTENTION_MAX_ITEMS];  /**< Items to evaluate */
    uint32_t num_items;                  /**< Number of items */
    uint32_t modality;                   /**< Modality for evaluation */
} salience_eval_request_t;

/**
 * @brief Payload for salience detection events
 */
typedef struct {
    salient_item_t item;                 /**< Salient item information */
    bool captured_attention;             /**< Whether attention was captured */
} salience_detection_payload_t;

/**
 * @brief Payload for attention shift request events
 */
typedef struct {
    attention_target_t target;           /**< Target information */
    float shift_weight;                  /**< Shift weight/urgency */
} attention_shift_payload_t;

/**
 * @brief Payload for priority update events
 */
typedef struct {
    attention_priority_t priorities[SALIENCE_ATTENTION_MAX_PRIORITIES];
    uint32_t num_priorities;             /**< Number of priority entries */
    uint64_t timestamp;                  /**< Update timestamp */
} priority_update_payload_t;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * @brief Callback for salience detection events from attention system
 *
 * @param item Detected salient item
 * @param user_data User context
 * @return 0 on success, -1 to reject
 */
typedef int (*sa_salience_callback_t)(
    const salient_item_t* item,
    void* user_data
);

/**
 * @brief Callback for attention focus changes
 *
 * @param focus New focus information
 * @param user_data User context
 */
typedef void (*sa_attention_callback_t)(
    const attention_focus_t* focus,
    void* user_data
);

/**
 * @brief Callback for priority updates
 *
 * @param priorities Array of priorities
 * @param num_priorities Number of priority entries
 * @param user_data User context
 */
typedef void (*sa_priority_callback_t)(
    const attention_priority_t* priorities,
    uint32_t num_priorities,
    void* user_data
);

/**
 * @brief Callback for salience evaluation requests
 *
 * @param request Evaluation request
 * @param user_data User context
 * @return 0 on success, -1 on error
 */
typedef int (*sa_eval_request_callback_t)(
    const salience_eval_request_t* request,
    void* user_data
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default salience-attention bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with standard settings
 * HOW:  Set auto-subscribe options and thresholds
 *
 * DEFAULT VALUES:
 * - module_id: SALIENCE_ATTENTION_DEFAULT_MODULE_ID
 * - enable_logging: false
 * - salience_threshold: 0.7
 * - attention_shift_weight: 0.8
 * - priority_weight: 0.5
 * - auto_subscribe_attention: true
 * - auto_subscribe_state: true
 * - enable_query_handler: true
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_default_config(salience_attention_config_t* config);

/**
 * @brief Create salience-attention hub bridge
 *
 * WHAT: Initialize bridge for salience-attention integration
 * WHY:  Enable bidirectional communication between salience and attention
 * HOW:  Allocate bridge, initialize state, prepare callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
salience_attention_bridge_t* salience_attention_bridge_create(
    const salience_attention_config_t* config
);

/**
 * @brief Destroy salience-attention hub bridge
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
void salience_attention_bridge_destroy(salience_attention_bridge_t* bridge);

/* ============================================================================
 * Hub Registration API
 * ============================================================================ */

/**
 * @brief Register bridge with cognitive hub
 *
 * WHAT: Register salience-attention bridge with the cognitive hub
 * WHY:  Enable event subscription/publication and query handling
 * HOW:  Register module, subscribe to configured events, set up handlers
 *
 * @param bridge Salience-attention bridge
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge or hub is NULL
 * - Returns -1 if already registered
 * - Returns -1 if hub registration fails
 *
 * SIDE EFFECTS:
 * - Registers as COG_CATEGORY_PERCEPTION module
 * - Subscribes to configured event types
 * - Registers query handler if enabled
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int salience_attention_bridge_register_with_hub(
    salience_attention_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Unregister bridge from cognitive hub
 *
 * WHAT: Unregister from hub and cleanup subscriptions
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Unsubscribe from events, unregister module
 *
 * @param bridge Salience-attention bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int salience_attention_bridge_unregister_from_hub(
    salience_attention_bridge_t* bridge
);

/**
 * @brief Check if bridge is registered with hub
 *
 * @param bridge Salience-attention bridge
 * @return true if registered, false otherwise
 */
bool salience_attention_bridge_is_registered(
    const salience_attention_bridge_t* bridge
);

/* ============================================================================
 * Module Connection API
 * ============================================================================ */

/**
 * @brief Set salience evaluator reference
 *
 * WHAT: Connect bridge to salience evaluator module
 * WHY:  Enable direct salience evaluation operations
 * HOW:  Store evaluator reference for later use
 *
 * @param bridge Salience-attention bridge
 * @param evaluator Salience evaluator (can be NULL)
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_salience(
    salience_attention_bridge_t* bridge,
    salience_evaluator_t evaluator
);

/**
 * @brief Set attention module reference
 *
 * WHAT: Connect bridge to multihead attention module
 * WHY:  Enable direct attention operations
 * HOW:  Store attention reference for later use
 *
 * @param bridge Salience-attention bridge
 * @param attention Multihead attention (can be NULL)
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_attention(
    salience_attention_bridge_t* bridge,
    multihead_attention_t attention
);

/* ============================================================================
 * Event Callback Registration
 * ============================================================================ */

/**
 * @brief Set callback for salience detection events
 *
 * @param bridge Salience-attention bridge
 * @param callback Salience callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_salience_callback(
    salience_attention_bridge_t* bridge,
    sa_salience_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for attention focus changes
 *
 * @param bridge Salience-attention bridge
 * @param callback Attention callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_attention_callback(
    salience_attention_bridge_t* bridge,
    sa_attention_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for priority updates
 *
 * @param bridge Salience-attention bridge
 * @param callback Priority callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_priority_callback(
    salience_attention_bridge_t* bridge,
    sa_priority_callback_t callback,
    void* user_data
);

/**
 * @brief Set callback for salience evaluation requests
 *
 * @param bridge Salience-attention bridge
 * @param callback Evaluation request callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_eval_callback(
    salience_attention_bridge_t* bridge,
    sa_eval_request_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Salience Publication API
 * ============================================================================ */

/**
 * @brief Publish salience detection event
 *
 * WHAT: Report that a salient item was detected
 * WHY:  Allow attention system to respond to salience
 * HOW:  Create event with item info, publish through hub
 *
 * @param bridge Salience-attention bridge
 * @param item Salient item information
 * @param score Overall salience score (0-1)
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Bottom-up salience captures attention automatically
 * - High salience items receive priority processing
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int salience_attention_publish_salience_detection(
    salience_attention_bridge_t* bridge,
    const salient_item_t* item,
    float score
);

/**
 * @brief Request attention shift to target
 *
 * WHAT: Request that attention shifts to a specific target
 * WHY:  Allow salience system to direct attention
 * HOW:  Create shift request event, publish through hub
 *
 * @param bridge Salience-attention bridge
 * @param target Attention target information
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Salient stimuli trigger exogenous attention shifts
 * - Competition between targets resolved by priority
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int salience_attention_request_attention_shift(
    salience_attention_bridge_t* bridge,
    const attention_target_t* target
);

/**
 * @brief Publish priority update
 *
 * WHAT: Update attention priorities for items
 * WHY:  Allow salience to influence attention allocation
 * HOW:  Create priority update event, publish through hub
 *
 * @param bridge Salience-attention bridge
 * @param priorities Array of priority updates
 * @param num_priorities Number of priority entries
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Priority maps in parietal cortex guide attention
 * - Salience contributes to priority alongside goals
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int salience_attention_publish_priority_update(
    salience_attention_bridge_t* bridge,
    const attention_priority_t* priorities,
    uint32_t num_priorities
);

/**
 * @brief Notify attention focus change
 *
 * WHAT: Report current attention focus
 * WHY:  Allow salience system to track attention state
 * HOW:  Create focus notification event, publish through hub
 *
 * @param bridge Salience-attention bridge
 * @param focus Current focus information
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Attention focus modulates salience sensitivity
 * - Attended locations have enhanced salience detection
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int salience_attention_notify_attention_focus(
    salience_attention_bridge_t* bridge,
    const attention_focus_t* focus
);

/**
 * @brief Request salience evaluation of items
 *
 * WHAT: Request salience evaluation for a set of items
 * WHY:  Allow attention system to query salience
 * HOW:  Create evaluation request event, publish through hub
 *
 * @param bridge Salience-attention bridge
 * @param request Evaluation request with items
 * @return 0 on success, -1 on error
 *
 * BIOLOGICAL BASIS:
 * - Top-down attention can bias salience computation
 * - Goal-relevant items have enhanced salience
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int salience_attention_request_salience_evaluation(
    salience_attention_bridge_t* bridge,
    const salience_eval_request_t* request
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current salience state
 *
 * WHAT: Query current salience detection state
 * WHY:  Allow other modules to inspect salience state
 * HOW:  Read state from connected evaluator
 *
 * @param bridge Salience-attention bridge
 * @param avg_salience Output: average salience score
 * @param peak_salience Output: peak salience score
 * @param detection_count Output: number of detections
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_get_salience_state(
    const salience_attention_bridge_t* bridge,
    float* avg_salience,
    float* peak_salience,
    uint32_t* detection_count
);

/**
 * @brief Get current attention state
 *
 * WHAT: Query current attention allocation state
 * WHY:  Allow other modules to inspect attention state
 * HOW:  Read state from connected attention module
 *
 * @param bridge Salience-attention bridge
 * @param current_focus Output: current focus ID
 * @param focus_strength Output: focus strength
 * @param num_targets Output: number of active targets
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_get_attention_state(
    const salience_attention_bridge_t* bridge,
    uint64_t* current_focus,
    float* focus_strength,
    uint32_t* num_targets
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
 * @param bridge Salience-attention bridge
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int salience_attention_bridge_get_stats(
    const salience_attention_bridge_t* bridge,
    salience_attention_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear all statistics counters
 * WHY:  Start fresh measurement period
 * HOW:  Zero all stat counters
 *
 * @param bridge Salience-attention bridge
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int salience_attention_bridge_reset_stats(salience_attention_bridge_t* bridge);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update salience threshold
 *
 * WHAT: Change threshold for attention capture
 * WHY:  Adapt sensitivity to current context
 * HOW:  Update internal threshold value
 *
 * @param bridge Salience-attention bridge
 * @param threshold New threshold (0-1)
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_threshold(
    salience_attention_bridge_t* bridge,
    float threshold
);

/**
 * @brief Update attention shift weight
 *
 * WHAT: Change weight for attention shift requests
 * WHY:  Control how strongly salience influences attention
 * HOW:  Update internal weight value
 *
 * @param bridge Salience-attention bridge
 * @param weight New weight (0-1)
 * @return 0 on success, -1 on error
 */
int salience_attention_bridge_set_shift_weight(
    salience_attention_bridge_t* bridge,
    float weight
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SALIENCE_ATTENTION_BRIDGE_H */
