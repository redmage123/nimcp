//=============================================================================
// nimcp_training_adapters.h - Training Layer Middleware Adapters
//=============================================================================
/**
 * @file nimcp_training_adapters.h
 * @brief Middleware adapters for learning signals and weight update routing
 *
 * WHAT: Middleware layer connecting training events to brain learning systems
 * WHY:  Decouple training signal sources from learning subsystems
 * HOW:  Event-driven adapters with feature extraction, normalization, and routing
 *
 * ARCHITECTURE:
 * - Learning Signal Adapter: Extracts and normalizes training signals from events
 * - Weight Update Router: Routes weight updates to appropriate brain regions
 * - Training Event Manager: Coordinates training events and subscriptions
 *
 * INTEGRATION POINTS:
 * - core/brain/learning/nimcp_brain_learning.h: Brain learning API
 * - plasticity/stdp/nimcp_stdp.h: STDP weight updates
 * - middleware/events/*: Event bus and subscription
 * - middleware/routing/*: Routing tables and thalamic routing
 *
 * PERFORMANCE:
 * - Signal extraction: O(n) where n = feature count
 * - Weight routing: O(log m) where m = routing table size
 * - Event processing: O(k) where k = subscribers
 *
 * BIOLOGICAL BASIS:
 * - Thalamic gating of learning signals
 * - Cortical routing of plasticity signals
 * - Neuromodulator-gated learning
 * - Attention-weighted feature extraction
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_TRAINING_ADAPTERS_H
#define NIMCP_TRAINING_ADAPTERS_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"
#include "middleware/routing/nimcp_routing_table.h"
#include "security/nimcp_security_integration.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Learning Signal Adapter - Feature Extraction & Normalization
//=============================================================================

/**
 * @brief Learning signal source types
 *
 * WHAT: Different sources of learning signals
 * WHY:  Enable source-specific feature extraction
 * HOW:  Tagged union for polymorphic signal handling
 */
typedef enum {
    LEARNING_SIGNAL_ERROR,         /**< Prediction error signals */
    LEARNING_SIGNAL_REWARD,        /**< Reward/punishment signals */
    LEARNING_SIGNAL_SURPRISE,      /**< Novelty/surprise signals */
    LEARNING_SIGNAL_ATTENTION,     /**< Attention-modulated signals */
    LEARNING_SIGNAL_MEMORY,        /**< Memory consolidation signals */
    LEARNING_SIGNAL_CUSTOM         /**< User-defined signals */
} learning_signal_type_t;

/**
 * @brief Normalization strategy for learning signals
 *
 * WHAT: How to normalize feature values
 * WHY:  Prevent scale differences from dominating learning
 * HOW:  Statistical normalization methods
 */
typedef enum {
    NORMALIZE_NONE,           /**< No normalization */
    NORMALIZE_MIN_MAX,        /**< Scale to [0,1] range */
    NORMALIZE_Z_SCORE,        /**< Zero mean, unit variance */
    NORMALIZE_L2,             /**< L2 normalization (unit length) */
    NORMALIZE_ADAPTIVE        /**< Online adaptive normalization */
} normalization_strategy_t;

/**
 * @brief Learning signal data
 *
 * WHAT: Extracted and normalized learning signal
 * WHY:  Standardized format for learning subsystems
 * HOW:  Features + metadata + normalization stats
 */
typedef struct {
    learning_signal_type_t type;    /**< Signal type */
    float* features;                /**< Feature vector */
    uint32_t num_features;          /**< Feature count */
    float magnitude;                /**< Overall signal strength [0,1] */
    float confidence;               /**< Signal confidence [0,1] */
    uint64_t timestamp_us;          /**< When signal occurred */
    uint32_t source_id;             /**< Source component ID */
    void* metadata;                 /**< Type-specific metadata */
} learning_signal_t;

/**
 * @brief Learning signal adapter configuration
 */
typedef struct {
    normalization_strategy_t normalization;  /**< How to normalize */
    float learning_rate_scale;               /**< LR multiplier [0,1] */
    bool enable_attention_weighting;         /**< Weight by attention? */
    bool enable_novelty_boost;               /**< Boost novel signals? */
    float novelty_boost_factor;              /**< Novelty multiplier [1,5] */
    uint32_t history_window_size;            /**< Stats window size */
    float min_confidence_threshold;          /**< Drop below threshold */
    nimcp_sec_integration_t* security_ctx;   /**< Security context (optional) */
} learning_signal_adapter_config_t;

/**
 * @brief Opaque learning signal adapter handle
 */
typedef struct learning_signal_adapter_struct* learning_signal_adapter_t;

/**
 * @brief Create learning signal adapter
 *
 * WHAT: Initialize signal extraction and normalization adapter
 * WHY:  Convert raw events to standardized learning signals
 * HOW:  Allocate normalization stats, configure feature extraction
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param config Adapter configuration (NULL for defaults)
 * @return Adapter handle or NULL on error
 */
learning_signal_adapter_t learning_signal_adapter_create(
    const learning_signal_adapter_config_t* config);

/**
 * @brief Destroy learning signal adapter
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void learning_signal_adapter_destroy(learning_signal_adapter_t adapter);

/**
 * @brief Extract learning signal from event
 *
 * WHAT: Convert event to normalized learning signal
 * WHY:  Standardize diverse event types for learning
 * HOW:  Type-specific extraction + normalization + metadata
 *
 * COMPLEXITY: O(n) where n = num_features
 * THREAD-SAFE: Yes
 *
 * @param adapter Adapter handle
 * @param event Source event
 * @param signal Output learning signal (caller must free)
 * @return true on success, false if event not applicable
 */
bool learning_signal_adapter_extract(learning_signal_adapter_t adapter,
                                      const brain_event_t* event,
                                      learning_signal_t* signal);

/**
 * @brief Normalize feature vector
 *
 * WHAT: Apply normalization strategy to features
 * WHY:  Prevent scale differences from dominating
 * HOW:  Statistical normalization with online stats
 *
 * COMPLEXITY: O(n) where n = num_features
 * THREAD-SAFE: Yes
 *
 * @param adapter Adapter handle
 * @param features Feature vector (modified in-place)
 * @param num_features Feature count
 * @return true on success
 */
bool learning_signal_adapter_normalize(learning_signal_adapter_t adapter,
                                        float* features,
                                        uint32_t num_features);

/**
 * @brief Apply attention weighting to signal
 *
 * WHAT: Scale signal by attention strength
 * WHY:  Focus learning on attended stimuli
 * HOW:  Multiply features by attention weight
 *
 * COMPLEXITY: O(n)
 * THREAD-SAFE: Yes
 *
 * @param adapter Adapter handle
 * @param signal Learning signal (modified in-place)
 * @param attention_weight Attention strength [0,1]
 * @return true on success
 */
bool learning_signal_adapter_apply_attention(learning_signal_adapter_t adapter,
                                              learning_signal_t* signal,
                                              float attention_weight);

/**
 * @brief Free learning signal resources
 *
 * COMPLEXITY: O(1)
 */
void learning_signal_free(learning_signal_t* signal);

/**
 * @brief Get adapter statistics
 */
typedef struct {
    uint64_t signals_extracted;
    uint64_t signals_normalized;
    uint64_t signals_dropped;
    float avg_magnitude;
    float avg_confidence;
} learning_signal_adapter_stats_t;

bool learning_signal_adapter_get_stats(learning_signal_adapter_t adapter,
                                        learning_signal_adapter_stats_t* stats);

/**
 * @brief Get default configuration
 */
learning_signal_adapter_config_t learning_signal_adapter_default_config(void);

//=============================================================================
// Weight Update Router - Event Bus & Routing Table
//=============================================================================

/**
 * @brief Weight update target types
 *
 * WHAT: Where weight updates should be applied
 * WHY:  Route updates to appropriate brain regions
 * HOW:  Target identifiers for routing table
 */
typedef enum {
    WEIGHT_TARGET_CORTICAL,       /**< Cortical connections */
    WEIGHT_TARGET_SUBCORTICAL,    /**< Subcortical connections */
    WEIGHT_TARGET_HIPPOCAMPAL,    /**< Hippocampal connections */
    WEIGHT_TARGET_STRIATAL,       /**< Striatal connections */
    WEIGHT_TARGET_THALAMIC,       /**< Thalamic connections */
    WEIGHT_TARGET_CEREBELLAR,     /**< Cerebellar connections */
    WEIGHT_TARGET_CUSTOM          /**< User-defined target */
} weight_target_type_t;

/**
 * @brief Weight update data
 *
 * WHAT: Weight modification instructions
 * WHY:  Standardized format for plasticity updates
 * HOW:  Delta weights + metadata + routing info
 */
typedef struct {
    weight_target_type_t target_type; /**< Target region */
    uint32_t source_neuron;           /**< Source neuron ID */
    uint32_t target_neuron;           /**< Target neuron ID */
    float weight_delta;               /**< Weight change */
    float learning_rate;              /**< LR for this update */
    float modulation_factor;          /**< Neuromodulator factor */
    uint64_t timestamp_us;            /**< When update occurred */
    bool apply_stdp;                  /**< Use STDP timing? */
    void* metadata;                   /**< Type-specific metadata */
} weight_update_t;

/**
 * @brief Weight update router configuration
 */
typedef struct {
    uint32_t routing_table_capacity;  /**< Max routing rules */
    bool enable_dynamic_routing;      /**< Learn new routes? */
    bool enable_priority_routing;     /**< Use priority queues? */
    float route_learning_rate;        /**< Hebbian route learning */
    uint32_t max_batch_size;          /**< Max updates per batch */
    bool enable_update_coalescing;    /**< Merge similar updates? */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */
} weight_update_router_config_t;

/**
 * @brief Opaque weight update router handle
 */
typedef struct weight_update_router_struct* weight_update_router_t;

/**
 * @brief Create weight update router
 *
 * WHAT: Initialize routing table and event bus integration
 * WHY:  Route weight updates to correct brain regions
 * HOW:  Create routing table, subscribe to weight events
 *
 * COMPLEXITY: O(capacity)
 * THREAD-SAFE: Yes
 *
 * @param config Router configuration (NULL for defaults)
 * @param event_bus Event bus for publishing (NULL = create own)
 * @return Router handle or NULL on error
 */
weight_update_router_t weight_update_router_create(
    const weight_update_router_config_t* config,
    event_bus_t event_bus);

/**
 * @brief Destroy weight update router
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
void weight_update_router_destroy(weight_update_router_t router);

/**
 * @brief Route weight update to target
 *
 * WHAT: Deliver weight update to appropriate subsystem
 * WHY:  Ensure updates reach correct brain regions
 * HOW:  Query routing table, publish to event bus
 *
 * COMPLEXITY: O(log n) where n = routing rules
 * THREAD-SAFE: Yes
 *
 * @param router Router handle
 * @param update Weight update to route
 * @return true if routed successfully
 */
bool weight_update_router_route(weight_update_router_t router,
                                 const weight_update_t* update);

/**
 * @brief Batch route multiple weight updates
 *
 * WHAT: Route multiple updates efficiently
 * WHY:  Reduce routing overhead for batch learning
 * HOW:  Sort by target, batch publish
 *
 * COMPLEXITY: O(k log n) where k = num_updates
 * THREAD-SAFE: Yes
 *
 * @param router Router handle
 * @param updates Update array
 * @param num_updates Update count
 * @return Number successfully routed
 */
uint32_t weight_update_router_route_batch(weight_update_router_t router,
                                           const weight_update_t* updates,
                                           uint32_t num_updates);

/**
 * @brief Add routing rule
 *
 * WHAT: Register route from source to target region
 * WHY:  Define where updates should be sent
 * HOW:  Add rule to routing table
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param router Router handle
 * @param source_type Source signal type
 * @param target_type Target brain region
 * @param priority Route priority
 * @return true on success
 */
bool weight_update_router_add_route(weight_update_router_t router,
                                     learning_signal_type_t source_type,
                                     weight_target_type_t target_type,
                                     uint32_t priority);

/**
 * @brief Remove routing rule
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool weight_update_router_remove_route(weight_update_router_t router,
                                        learning_signal_type_t source_type,
                                        weight_target_type_t target_type);

/**
 * @brief Update route strength (Hebbian)
 *
 * WHAT: Strengthen frequently used routes
 * WHY:  Learn task-specific routing patterns
 * HOW:  Increment route strength on use
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param router Router handle
 * @param source_type Source signal type
 * @param target_type Target brain region
 * @return true if route exists
 */
bool weight_update_router_strengthen_route(weight_update_router_t router,
                                            learning_signal_type_t source_type,
                                            weight_target_type_t target_type);

/**
 * @brief Get router statistics
 */
typedef struct {
    uint64_t updates_routed;
    uint64_t updates_dropped;
    uint64_t batch_operations;
    uint32_t active_routes;
    float avg_routing_time_us;
} weight_update_router_stats_t;

bool weight_update_router_get_stats(weight_update_router_t router,
                                     weight_update_router_stats_t* stats);

/**
 * @brief Get default configuration
 */
weight_update_router_config_t weight_update_router_default_config(void);

//=============================================================================
// Training Event Manager - Event Queue & Subscriber
//=============================================================================

/**
 * @brief Training event types
 *
 * WHAT: Events related to training coordination
 * WHY:  Enable event-driven training orchestration
 * HOW:  Training-specific event types
 */
typedef enum {
    TRAINING_EVENT_EPOCH_START,     /**< Training epoch started */
    TRAINING_EVENT_EPOCH_END,       /**< Training epoch completed */
    TRAINING_EVENT_BATCH_START,     /**< Batch training started */
    TRAINING_EVENT_BATCH_END,       /**< Batch training completed */
    TRAINING_EVENT_LOSS_UPDATE,     /**< Loss value updated */
    TRAINING_EVENT_LR_UPDATE,       /**< Learning rate changed */
    TRAINING_EVENT_CONVERGENCE,     /**< Training converged */
    TRAINING_EVENT_DIVERGENCE,      /**< Training diverging */
    TRAINING_EVENT_CHECKPOINT       /**< Checkpoint created */
} training_event_type_t;

/**
 * @brief Training event data
 *
 * WHAT: Training coordination event payload
 * WHY:  Synchronize training across subsystems
 * HOW:  Event with training-specific metadata
 */
typedef struct {
    training_event_type_t type;     /**< Event type */
    uint32_t epoch;                 /**< Current epoch */
    uint32_t batch;                 /**< Current batch */
    float loss;                     /**< Current loss */
    float learning_rate;            /**< Current LR */
    uint64_t timestamp_us;          /**< When event occurred */
    void* metadata;                 /**< Type-specific metadata */
} training_event_data_t;

/**
 * @brief Training event callback
 *
 * WHAT: User callback for training events
 * WHY:  Enable custom training coordination logic
 * HOW:  Function pointer called on events
 *
 * @param event Training event
 * @param context User context
 */
typedef void (*training_event_callback_fn)(const training_event_data_t* event,
                                            void* context);

/**
 * @brief Training event manager configuration
 */
typedef struct {
    uint32_t event_queue_capacity;   /**< Max queued events */
    bool enable_async_delivery;      /**< Async event delivery? */
    bool enable_event_batching;      /**< Batch similar events? */
    uint32_t batch_timeout_ms;       /**< Batching timeout */
    bool enable_priority_scheduling; /**< Priority event scheduling? */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */
} training_event_manager_config_t;

/**
 * @brief Opaque training event manager handle
 */
typedef struct training_event_manager_struct* training_event_manager_t;

/**
 * @brief Create training event manager
 *
 * WHAT: Initialize event queue and subscriber system
 * WHY:  Coordinate training across subsystems
 * HOW:  Create event bus, queue, and subscriber manager
 *
 * COMPLEXITY: O(capacity)
 * THREAD-SAFE: Yes
 *
 * @param config Manager configuration (NULL for defaults)
 * @param event_bus Event bus for publishing (NULL = create own)
 * @return Manager handle or NULL on error
 */
training_event_manager_t training_event_manager_create(
    const training_event_manager_config_t* config,
    event_bus_t event_bus);

/**
 * @brief Destroy training event manager
 *
 * COMPLEXITY: O(n) where n = subscribers
 * THREAD-SAFE: Yes
 */
void training_event_manager_destroy(training_event_manager_t manager);

/**
 * @brief Publish training event
 *
 * WHAT: Broadcast training event to subscribers
 * WHY:  Notify subsystems of training state changes
 * HOW:  Convert to generic event, publish to event bus
 *
 * COMPLEXITY: O(log n) where n = queue size
 * THREAD-SAFE: Yes
 *
 * @param manager Manager handle
 * @param event Training event to publish
 * @return true on success
 */
bool training_event_manager_publish(training_event_manager_t manager,
                                     const training_event_data_t* event);

/**
 * @brief Subscribe to training events
 *
 * WHAT: Register callback for training events
 * WHY:  React to training state changes
 * HOW:  Add subscriber with optional filtering
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @param manager Manager handle
 * @param callback Callback function
 * @param context User context
 * @param event_types Event types to subscribe (NULL = all)
 * @param num_types Number of event types
 * @return Subscription handle or INVALID on error
 */
event_subscription_handle_t training_event_manager_subscribe(
    training_event_manager_t manager,
    training_event_callback_fn callback,
    void* context,
    const training_event_type_t* event_types,
    uint32_t num_types);

/**
 * @brief Unsubscribe from training events
 *
 * COMPLEXITY: O(n) where n = subscribers
 * THREAD-SAFE: Yes
 */
bool training_event_manager_unsubscribe(training_event_manager_t manager,
                                         event_subscription_handle_t handle);

/**
 * @brief Process pending training events
 *
 * WHAT: Manually process queued events
 * WHY:  For synchronous event delivery
 * HOW:  Dequeue and dispatch up to max_events
 *
 * COMPLEXITY: O(k log n) where k = max_events
 * THREAD-SAFE: Yes
 *
 * @param manager Manager handle
 * @param max_events Maximum events to process
 * @return Number of events processed
 */
uint32_t training_event_manager_process_events(training_event_manager_t manager,
                                                uint32_t max_events);

/**
 * @brief Get manager statistics
 */
typedef struct {
    uint64_t events_published;
    uint64_t events_delivered;
    uint64_t events_dropped;
    uint32_t active_subscribers;
    uint32_t queue_size;
} training_event_manager_stats_t;

bool training_event_manager_get_stats(training_event_manager_t manager,
                                       training_event_manager_stats_t* stats);

/**
 * @brief Get default configuration
 */
training_event_manager_config_t training_event_manager_default_config(void);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get learning signal type name
 */
const char* learning_signal_type_name(learning_signal_type_t type);

/**
 * @brief Get weight target type name
 */
const char* weight_target_type_name(weight_target_type_t type);

/**
 * @brief Get training event type name
 */
const char* training_event_type_name(training_event_type_t type);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TRAINING_ADAPTERS_H
