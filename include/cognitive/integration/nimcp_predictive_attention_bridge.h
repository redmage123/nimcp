/**
 * @file nimcp_predictive_attention_bridge.h
 * @brief Predictive Processing - Attention Cognitive Hub Bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Bridge connecting predictive processing to attention systems via the
 *       cognitive integration hub, enabling bidirectional communication.
 *
 * WHY: Predictive processing and attention are tightly coupled:
 *      - Prediction errors signal where attention is needed
 *      - Precision weighting determines attention allocation
 *      - Surprise/unexpected events automatically capture attention
 *      - Active inference: attention samples to minimize prediction error
 *
 * HOW: Registers both predictive and attention modules with the hub,
 *      subscribes to relevant events, and routes information bidirectionally.
 *
 * THEORETICAL BASIS:
 * - Predictive Coding: Brain constantly generates predictions, attention is
 *   allocated to minimize prediction error (Karl Friston's Free Energy Principle)
 * - Active Inference: Attention sampling reduces uncertainty through precision
 * - Salience: Prediction errors create salience signals that capture attention
 *
 * INTEGRATION PATTERN:
 * +-----------------------+          +----------------------------+
 * | Predictive Processing |<-------->| Predictive-Attention Bridge|
 * |   - Predictions       |          |   - Error routing          |
 * |   - Prediction Errors |          |   - Precision weighting    |
 * |   - Precision         |          |   - Surprise detection     |
 * +-----------------------+          +----------------------------+
 *                                              ^
 *                                              |
 *                                              v
 *                                    +-------------------+
 *                                    | Attention System  |
 *                                    | - Focus shifts    |
 *                                    | - Priority alloc  |
 *                                    +-------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PREDICTIVE_ATTENTION_BRIDGE_H
#define NIMCP_PREDICTIVE_ATTENTION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct cognitive_integration_hub_struct;
typedef struct cognitive_integration_hub_struct* cognitive_integration_hub_t;

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief Opaque Predictive-Attention bridge structure
 *
 * WHAT: Forward declaration for bridge instance
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct predictive_attention_bridge predictive_attention_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Module ID for predictive-attention bridge in the integration hub
 */
#define PRED_ATTN_BRIDGE_MODULE_ID 0x5041  /* "PA" */

/**
 * @brief Module name for registration
 */
#define PRED_ATTN_BRIDGE_MODULE_NAME "PredictiveAttention"

/**
 * @brief Default threshold for prediction error to trigger attention
 */
#define PRED_ATTN_DEFAULT_ERROR_THRESHOLD 0.3f

/**
 * @brief Default weight for surprise-based attention
 */
#define PRED_ATTN_DEFAULT_SURPRISE_WEIGHT 0.7f

/**
 * @brief Default precision weight for attention allocation
 */
#define PRED_ATTN_DEFAULT_PRECISION_WEIGHT 0.5f

/**
 * @brief Maximum queued events before dropping
 */
#define PRED_ATTN_MAX_QUEUED_EVENTS 128

/* ============================================================================
 * Event Type Definitions
 * ============================================================================ */

/**
 * @brief Types of predictive-attention events that can be published
 *
 * WHAT: Enumeration of bridge-specific event subtypes
 * WHY: Allow subscribers to filter on specific event types
 * HOW: Included in event payload for detailed routing
 */
typedef enum {
    /** Prediction error detected at a location */
    PRED_ATTN_EVENT_PREDICTION_ERROR = 0,

    /** Attention requested for error source */
    PRED_ATTN_EVENT_ATTENTION_REQUEST,

    /** Precision estimate updated */
    PRED_ATTN_EVENT_PRECISION_UPDATE,

    /** Prediction made for attended item */
    PRED_ATTN_EVENT_ATTENDED_PREDICTION,

    /** Prediction requested for focus target */
    PRED_ATTN_EVENT_FOCUS_PREDICTION_REQUEST,

    /** Surprise event detected */
    PRED_ATTN_EVENT_SURPRISE_DETECTED,

    /** Attention shift completed */
    PRED_ATTN_EVENT_ATTENTION_SHIFTED,

    /** Active inference sampling started */
    PRED_ATTN_EVENT_SAMPLING_STARTED,

    /** Active inference sampling completed */
    PRED_ATTN_EVENT_SAMPLING_COMPLETED,

    /** Count of event types */
    PRED_ATTN_EVENT_COUNT
} pred_attn_event_subtype_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for Predictive-Attention bridge
 *
 * WHAT: Parameters controlling bridge behavior
 * WHY: Customize error thresholds, weights, and publishing behavior
 * HOW: Applied at bridge creation time
 */
typedef struct {
    /** Enable logging for debugging */
    bool enable_logging;

    /** Weight for prediction error in attention allocation [0-1] */
    float prediction_error_weight;

    /** Weight for surprise in attention capture [0-1] */
    float surprise_attention_weight;

    /** Weight for precision in attention modulation [0-1] */
    float precision_weight;

    /** Threshold for prediction error to trigger attention shift */
    float error_attention_threshold;

    /** Enable automatic event publishing on state changes */
    bool enable_auto_publish;

    /** Enable subscription to prediction events */
    bool enable_prediction_subscription;

    /** Enable subscription to attention events */
    bool enable_attention_subscription;

    /** Enable subscription to error events */
    bool enable_error_subscription;

    /** Enable subscription to state change events */
    bool enable_state_subscription;

    /** Enable query handling */
    bool enable_query_handling;

    /** Priority for published events */
    uint32_t event_priority;
} predictive_attention_bridge_config_t;

/**
 * @brief Prediction error data for publishing
 *
 * WHAT: Data describing a prediction error event
 * WHY: Communicate prediction errors to attention system
 * HOW: Payload for prediction error events
 */
typedef struct {
    /** Unique error identifier */
    uint64_t error_id;

    /** Magnitude of prediction error [0-1] */
    float error_magnitude;

    /** Location/source of the error (spatial or conceptual) */
    uint64_t error_location;

    /** Expected value */
    float expected_value;

    /** Actual observed value */
    float observed_value;

    /** Precision at this location [0-1] */
    float precision;

    /** Timestamp of error detection (microseconds) */
    uint64_t timestamp_us;
} pred_attn_error_data_t;

/**
 * @brief Precision estimate data for publishing
 *
 * WHAT: Data describing a precision weighting update
 * WHY: Communicate precision changes that affect attention
 * HOW: Payload for precision update events
 */
typedef struct {
    /** Location/source for precision estimate */
    uint64_t location;

    /** Old precision value [0-1] */
    float precision_old;

    /** New precision value [0-1] */
    float precision_new;

    /** Confidence in precision estimate [0-1] */
    float confidence;

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} pred_attn_precision_data_t;

/**
 * @brief Prediction data for attended item
 *
 * WHAT: Prediction generated for an attended location/item
 * WHY: Communicate predictions to attention system for validation
 * HOW: Payload for attended prediction events
 */
typedef struct {
    /** Focus item/location being predicted */
    uint64_t focus_id;

    /** Predicted value */
    float prediction;

    /** Prediction confidence [0-1] */
    float confidence;

    /** Expected precision if prediction correct [0-1] */
    float expected_precision;

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} pred_attn_prediction_data_t;

/**
 * @brief Focus request data for prediction
 *
 * WHAT: Request for prediction at current attention focus
 * WHY: Allow attention system to request predictions
 * HOW: Payload for focus prediction request events
 */
typedef struct {
    /** Current focus location/item */
    uint64_t focus_id;

    /** Urgency of request [0-1] */
    float urgency;

    /** Duration of focus (microseconds) */
    uint64_t focus_duration_us;

    /** Timestamp (microseconds) */
    uint64_t timestamp_us;
} pred_attn_focus_request_t;

/**
 * @brief Statistics for Predictive-Attention bridge
 *
 * WHAT: Operational metrics for the bridge
 * WHY: Monitor bridge health and activity
 * HOW: Accumulated during bridge operation
 */
typedef struct {
    /** Total events processed */
    uint32_t total_events;

    /** Total prediction errors reported */
    uint32_t prediction_errors;

    /** Attention shifts triggered by surprise */
    uint32_t surprise_shifts;

    /** Precision updates processed */
    uint32_t precision_updates;

    /** Events received from hub */
    uint32_t events_received;

    /** Events published to hub */
    uint32_t events_published;

    /** Queries handled */
    uint32_t queries_handled;

    /** Attention requests made */
    uint32_t attention_requests;

    /** Predictions for focus delivered */
    uint32_t focus_predictions;

    /** Events dropped due to queue full */
    uint32_t events_dropped;

    /** Average prediction error magnitude */
    float avg_error_magnitude;

    /** Average precision across updates */
    float avg_precision;

    /** Query errors */
    uint32_t query_errors;
} predictive_attention_bridge_stats_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default Predictive-Attention bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY: Provide easy starting point for bridge setup
 * HOW: Sets balanced weights and enables core functionality
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * DEFAULT VALUES:
 * - enable_logging: false
 * - prediction_error_weight: 0.6
 * - surprise_attention_weight: 0.7
 * - precision_weight: 0.5
 * - error_attention_threshold: 0.3
 * - enable_auto_publish: true
 * - enable_prediction_subscription: true
 * - enable_attention_subscription: true
 * - enable_error_subscription: true
 * - enable_state_subscription: true
 * - enable_query_handling: true
 * - event_priority: COG_PRIORITY_NORMAL
 */
int predictive_attention_bridge_default_config(
    predictive_attention_bridge_config_t* config
);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Predictive-Attention bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY: Create the connection point between predictive and attention systems
 * HOW: Allocate memory, initialize state, prepare for connection
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on failure
 *
 * MEMORY: Caller must call predictive_attention_bridge_destroy() when done
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
predictive_attention_bridge_t* predictive_attention_bridge_create(
    const predictive_attention_bridge_config_t* config
);

/**
 * @brief Destroy Predictive-Attention bridge
 *
 * WHAT: Clean up bridge resources
 * WHY: Prevent memory leaks and clean disconnection
 * HOW: Disconnect if connected, free all resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * NOTE: Will automatically disconnect from hub if still connected
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
void predictive_attention_bridge_destroy(predictive_attention_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Register bridge with cognitive hub
 *
 * WHAT: Register predictive-attention bridge with hub and set up subscriptions
 * WHY: Enable bidirectional communication through event system
 * HOW: Register module, subscribe to events, register query handler
 *
 * @param bridge Bridge instance
 * @param hub Cognitive integration hub
 * @return 0 on success, -1 on error
 *
 * ACTIONS:
 * - Registers bridge as COG_CATEGORY_PERCEPTION module
 * - Subscribes to COG_EVENT_INPUT_RECEIVED (predictions)
 * - Subscribes to COG_EVENT_ATTENTION_SHIFT
 * - Subscribes to COG_EVENT_STATE_CHANGE (errors, precision)
 * - Registers query handler for bridge state queries
 *
 * ERRORS:
 * - Returns -1 if bridge is NULL
 * - Returns -1 if hub is NULL
 * - Returns -1 if already connected
 * - Returns -1 if registration fails
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int predictive_attention_bridge_register_with_hub(
    predictive_attention_bridge_t* bridge,
    cognitive_integration_hub_t hub
);

/**
 * @brief Unregister bridge from cognitive hub
 *
 * WHAT: Unregister from hub and clean up subscriptions
 * WHY: Clean disconnection when bridge is shutting down
 * HOW: Unsubscribe from all events, unregister module
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge is NULL
 * - Returns -1 if not connected
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int predictive_attention_bridge_unregister_from_hub(
    predictive_attention_bridge_t* bridge
);

/**
 * @brief Check if bridge is connected to hub
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool predictive_attention_bridge_is_connected(
    const predictive_attention_bridge_t* bridge
);

/* ============================================================================
 * Predictive -> Attention Operations
 * ============================================================================ */

/**
 * @brief Publish prediction error to hub
 *
 * WHAT: Report a prediction error that may require attention
 * WHY: Prediction errors signal where attention should be directed
 * HOW: Create and publish error event through hub
 *
 * @param bridge Bridge instance
 * @param error Error magnitude [0-1]
 * @param location Location/source of the error
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - If error exceeds threshold, may trigger attention shift
 * - Updates average error statistics
 * - May trigger surprise detection
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int predictive_attention_publish_prediction_error(
    predictive_attention_bridge_t* bridge,
    float error,
    uint64_t location
);

/**
 * @brief Request attention to error source
 *
 * WHAT: Explicitly request attention allocation to a prediction error
 * WHY: High-importance errors should capture attention immediately
 * HOW: Publish attention request through hub
 *
 * @param bridge Bridge instance
 * @param error_data Prediction error data
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - Directly requests attention shift to error location
 * - Updates attention request statistics
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int predictive_attention_request_attention_to_error(
    predictive_attention_bridge_t* bridge,
    const pred_attn_error_data_t* error_data
);

/**
 * @brief Publish precision estimate
 *
 * WHAT: Share precision weighting information with attention system
 * WHY: Precision determines attention allocation in predictive coding
 * HOW: Publish precision update through hub
 *
 * @param bridge Bridge instance
 * @param precision_data Precision estimate data
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - High precision = more reliable predictions = less attention needed
 * - Low precision = uncertain predictions = more attention allocated
 * - Updates precision statistics
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int predictive_attention_publish_precision_estimate(
    predictive_attention_bridge_t* bridge,
    const pred_attn_precision_data_t* precision_data
);

/* ============================================================================
 * Attention -> Predictive Operations
 * ============================================================================ */

/**
 * @brief Notify bridge of prediction for attended item
 *
 * WHAT: Report prediction generated for currently attended location
 * WHY: Attention focuses prediction generation for validation
 * HOW: Publish attended prediction through hub
 *
 * @param bridge Bridge instance
 * @param prediction Prediction data for attended item
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - Provides prediction for attention system to track
 * - Enables prediction error detection at focus
 * - Updates focus prediction statistics
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int predictive_attention_notify_attended_prediction(
    predictive_attention_bridge_t* bridge,
    const pred_attn_prediction_data_t* prediction
);

/**
 * @brief Request prediction for current attention focus
 *
 * WHAT: Request predictive system generate prediction for focus target
 * WHY: Active inference: attention samples to generate/validate predictions
 * HOW: Publish prediction request through hub
 *
 * @param bridge Bridge instance
 * @param focus Focus request data
 * @return 0 on success, -1 on error
 *
 * EFFECTS:
 * - Triggers prediction generation for focus location
 * - Supports active inference sampling
 * - Updates query statistics
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int predictive_attention_request_prediction_for_focus(
    predictive_attention_bridge_t* bridge,
    const pred_attn_focus_request_t* focus
);

/* ============================================================================
 * Event Handling Callbacks
 * ============================================================================ */

/**
 * @brief Callback for prediction events from hub
 *
 * WHAT: Handler invoked when prediction events are received
 * WHY: Process predictions and route to attention system
 * HOW: Routes events based on prediction type
 *
 * @param prediction Prediction value
 * @param location Location/source of prediction
 * @param confidence Prediction confidence [0-1]
 * @param user_data Bridge instance
 * @return 0 on success, -1 on error
 */
typedef int (*pred_attn_prediction_callback_t)(
    float prediction,
    uint64_t location,
    float confidence,
    void* user_data
);

/**
 * @brief Callback for attention shift events from hub
 *
 * WHAT: Handler invoked when attention shifts occur
 * WHY: Update predictions for new focus, track attention allocation
 * HOW: Routes attention events to predictive system
 *
 * @param old_focus Previous focus location
 * @param new_focus New focus location
 * @param urgency Shift urgency [0-1]
 * @param user_data Bridge instance
 * @return 0 on success, -1 on error
 */
typedef int (*pred_attn_attention_callback_t)(
    uint64_t old_focus,
    uint64_t new_focus,
    float urgency,
    void* user_data
);

/**
 * @brief Callback for error events from hub
 *
 * WHAT: Handler invoked when error events are received
 * WHY: Process errors and trigger attention allocation
 * HOW: Routes error events based on severity
 *
 * @param error_magnitude Error magnitude [0-1]
 * @param location Error location
 * @param user_data Bridge instance
 * @return 0 on success, -1 on error
 */
typedef int (*pred_attn_error_callback_t)(
    float error_magnitude,
    uint64_t location,
    void* user_data
);

/**
 * @brief Set prediction event callback
 *
 * @param bridge Bridge instance
 * @param callback Prediction callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int predictive_attention_set_prediction_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_prediction_callback_t callback,
    void* user_data
);

/**
 * @brief Set attention event callback
 *
 * @param bridge Bridge instance
 * @param callback Attention callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int predictive_attention_set_attention_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_attention_callback_t callback,
    void* user_data
);

/**
 * @brief Set error event callback
 *
 * @param bridge Bridge instance
 * @param callback Error callback (NULL to clear)
 * @param user_data User context for callback
 * @return 0 on success, -1 on error
 */
int predictive_attention_set_error_callback(
    predictive_attention_bridge_t* bridge,
    pred_attn_error_callback_t callback,
    void* user_data
);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve operational metrics for the bridge
 * WHY: Monitor bridge health and activity levels
 * HOW: Copy current statistics to output structure
 *
 * @param bridge Bridge instance
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int predictive_attention_bridge_get_stats(
    const predictive_attention_bridge_t* bridge,
    predictive_attention_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Zero all statistical counters
 * WHY: Start fresh measurement period
 * HOW: Reset all counters to zero
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int predictive_attention_bridge_reset_stats(
    predictive_attention_bridge_t* bridge
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get string name for predictive-attention event subtype
 *
 * @param subtype Event subtype
 * @return Human-readable name or "UNKNOWN"
 */
const char* pred_attn_event_subtype_to_string(pred_attn_event_subtype_t subtype);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PREDICTIVE_ATTENTION_BRIDGE_H */
