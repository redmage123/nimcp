/**
 * @file nimcp_training_event_adapter.h
 * @brief Middleware adapter for training event handling
 *
 * WHAT: Connects middleware event bus to training event handling
 * WHY:  Enable event-driven training and learning coordination
 * HOW:  Subscribe to neural events, dispatch to training modules
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#ifndef NIMCP_TRAINING_EVENT_ADAPTER_H
#define NIMCP_TRAINING_EVENT_ADAPTER_H

#include "core/events/nimcp_event_bus.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WHAT: Training event adapter instance
 * WHY:  Manage middleware-to-training event connection
 * HOW:  Maintain event subscriptions and dispatch
 */
typedef struct training_event_adapter_struct* training_event_adapter_t;

/**
 * WHAT: Training event types
 * WHY:  Categorize training-related events
 * HOW:  Enum of event categories
 */
typedef enum {
    TRAINING_EVENT_PATTERN_DETECTED,   /**< Significant pattern detected */
    TRAINING_EVENT_BURST_ONSET,        /**< Burst firing started */
    TRAINING_EVENT_SYNCHRONY_HIGH,     /**< High synchrony detected */
    TRAINING_EVENT_REWARD_RECEIVED,    /**< Reward signal received */
    TRAINING_EVENT_ERROR_COMPUTED,     /**< Prediction error computed */
    TRAINING_EVENT_CONSOLIDATION_DUE,  /**< Time to consolidate */
    TRAINING_EVENT_CUSTOM              /**< Custom training event */
} training_event_type_t;

/**
 * WHAT: Training event data
 * WHY:  Provide detailed event information
 * HOW:  Bundle type, timestamp, payload
 */
typedef struct {
    training_event_type_t type;  /**< Event type */
    uint64_t timestamp;          /**< When event occurred */
    float strength;              /**< Event strength/magnitude [0-1] */
    uint32_t num_channels;       /**< Number of channels involved */
    uint32_t* channel_ids;       /**< IDs of involved channels */
    void* custom_data;           /**< Custom event data */
} training_event_data_t;

/**
 * WHAT: Training event handler callback
 * WHY:  Allow custom event handling
 * HOW:  Function pointer for event callbacks
 *
 * @param event Training event data
 * @param user_data User-provided context
 */
typedef void (*training_event_handler_fn)(
    const training_event_data_t* event,
    void* user_data
);

/**
 * WHAT: Training event adapter configuration
 * WHY:  Customize event handling
 * HOW:  Specify which events to monitor
 */
typedef struct {
    event_bus_t* event_bus;          /**< Event bus to subscribe to */
    bool monitor_patterns;           /**< Monitor pattern events */
    bool monitor_bursts;             /**< Monitor burst events */
    bool monitor_synchrony;          /**< Monitor synchrony events */
    bool monitor_rewards;            /**< Monitor reward events */
    bool monitor_errors;             /**< Monitor error events */
    float pattern_threshold;         /**< Pattern detection threshold [0-1] */
    float synchrony_threshold;       /**< Synchrony detection threshold [0-1] */
} training_event_adapter_config_t;

/**
 * WHAT: Create training event adapter
 * WHY:  Initialize event handling
 * HOW:  Subscribe to relevant events on bus
 *
 * @param config Adapter configuration
 * @return Adapter handle or NULL on error
 */
training_event_adapter_t training_event_adapter_create(
    const training_event_adapter_config_t* config
);

/**
 * WHAT: Destroy training event adapter
 * WHY:  Clean memory cleanup
 * HOW:  Unsubscribe from events, free resources
 *
 * @param adapter Adapter to destroy (NULL is safe)
 */
void training_event_adapter_destroy(training_event_adapter_t adapter);

/**
 * WHAT: Register event handler
 * WHY:  Add custom event processing
 * HOW:  Add handler to callback list
 *
 * @param adapter Adapter instance
 * @param event_type Type of event to handle
 * @param handler Handler function
 * @param user_data User context for handler
 * @return true on success
 */
bool training_event_adapter_register_handler(
    training_event_adapter_t adapter,
    training_event_type_t event_type,
    training_event_handler_fn handler,
    void* user_data
);

/**
 * WHAT: Publish training event
 * WHY:  Emit event to all subscribers
 * HOW:  Create event, publish to bus
 *
 * @param adapter Adapter instance
 * @param event Event data to publish
 * @return true on success
 */
bool training_event_adapter_publish(
    training_event_adapter_t adapter,
    const training_event_data_t* event
);

/**
 * WHAT: Process pending events
 * WHY:  Handle accumulated events
 * HOW:  Dispatch to registered handlers
 *
 * @param adapter Adapter instance
 * @return Number of events processed
 */
uint32_t training_event_adapter_process_events(
    training_event_adapter_t adapter
);

/**
 * WHAT: Get default training event adapter configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @param event_bus Event bus to use
 * @return Default configuration
 */
training_event_adapter_config_t training_event_adapter_default_config(
    event_bus_t* event_bus
);

/**
 * WHAT: Create training event data structure
 * WHY:  Allocate storage for event
 * HOW:  Allocate and initialize
 *
 * @param type Event type
 * @param num_channels Number of channels involved
 * @return Allocated event or NULL on error
 */
training_event_data_t* training_event_data_create(
    training_event_type_t type,
    uint32_t num_channels
);

/**
 * WHAT: Destroy training event data structure
 * WHY:  Free allocated memory
 * HOW:  Free arrays and structure
 *
 * @param event Event to destroy (NULL is safe)
 */
void training_event_data_destroy(training_event_data_t* event);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TRAINING_EVENT_ADAPTER_H
