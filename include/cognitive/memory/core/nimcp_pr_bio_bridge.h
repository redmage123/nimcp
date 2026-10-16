//=============================================================================
// nimcp_pr_bio_bridge.h - Prime Resonant Bio-Async Bridge
//=============================================================================
/**
 * @file nimcp_pr_bio_bridge.h
 * @brief Integration bridge between PR Memory System and Bio-Async messaging
 *
 * WHAT: Routes PR memory operations through bio-async infrastructure with
 *       asynchronous consolidation, event notification, and priority handling
 * WHY:  Enable biologically-realistic memory system coordination through
 *       neuromodulator-based messaging, supporting distributed memory operations
 * HOW:  Maps PR memory events to bio-async channels, manages subscriber
 *       callbacks, and implements priority-based message queuing
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Memory Events and Neuromodulator Channels:
 *   +-----------------------------------------------------------------------+
 *   |  Memory Operation      | Channel           | Biological Basis        |
 *   |------------------------|-------------------|-------------------------|
 *   |  PR_MSG_ENCODE         | DOPAMINE          | Reward signal for new   |
 *   |                        |                   | information acquisition |
 *   |  PR_MSG_RETRIEVE       | ACETYLCHOLINE     | Fast attention/retrieval|
 *   |  PR_MSG_CONSOLIDATE    | SEROTONIN         | Slow state coordination |
 *   |  PR_MSG_ENTANGLE       | DOPAMINE          | Association reward      |
 *   |  PR_MSG_DECAY          | NOREPINEPHRINE    | Alert for forgetting    |
 *   |  PR_MSG_PROMOTE        | DOPAMINE          | Reward for consolidation|
 *   |  PR_MSG_FLASHBULB      | NOREPINEPHRINE    | High-priority emotional |
 *   +-----------------------------------------------------------------------+
 *
 *   Message Priority Mapping:
 *   +-----------------------------------------------------------------------+
 *   |  Priority Level   | Use Case                    | Biological Analog   |
 *   |-------------------|-----------------------------|--------------------|
 *   |  CRITICAL (0)     | Flashbulb, trauma           | Amygdala alarm     |
 *   |  HIGH (1)         | Active retrieval, encoding  | Attention focus    |
 *   |  NORMAL (2)       | Consolidation, entanglement | Background process |
 *   |  LOW (3)          | Decay events, statistics    | Housekeeping       |
 *   +-----------------------------------------------------------------------+
 *
 *   Async Consolidation Pipeline:
 *   +-----------------------------------------------------------------------+
 *   |  1. Memory event occurs (encode, retrieve, etc.)                      |
 *   |  2. Bridge creates bio message with appropriate channel               |
 *   |  3. Message queued with priority (urgent = higher priority)           |
 *   |  4. Bio-async system routes to registered subscribers                 |
 *   |  5. Subscribers receive notification with memory context              |
 *   |  6. Consolidation operations execute asynchronously                   |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Message creation: ~50ns
 * - Queue insertion: O(1) amortized
 * - Subscriber notification: O(S) where S = subscribers for message type
 * - Priority queue operations: O(log N) for N pending messages
 *
 * MEMORY:
 * - pr_bio_bridge_t: ~2KB base structure
 * - Message queue: ~64 bytes per pending message
 * - Subscriber array: ~16 bytes per subscriber
 *
 * THREAD SAFETY:
 * - All public functions are thread-safe via internal mutex
 * - Callbacks invoked without lock held to prevent deadlock
 * - Message queue protected by separate lock for concurrent send/receive
 *
 * INTEGRATION:
 * - Core: nimcp_pr_memory_node.h for memory nodes
 * - Core: nimcp_entanglement.h for entanglement graph
 * - Core: nimcp_z_ladder.h for tier management
 * - Async: nimcp_bio_async.h for bio-async infrastructure
 * - Async: nimcp_bio_messages.h for message types
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_BIO_BRIDGE_H
#define NIMCP_PR_BIO_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum pending messages in queue */
#define PR_BIO_MAX_PENDING_MESSAGES         4096

/** Maximum subscribers per message type */
#define PR_BIO_MAX_SUBSCRIBERS_PER_TYPE     64

/** Maximum total subscribers */
#define PR_BIO_MAX_TOTAL_SUBSCRIBERS        256

/** Default message timeout in milliseconds */
#define PR_BIO_DEFAULT_TIMEOUT_MS           5000

/** Default batch size for processing */
#define PR_BIO_DEFAULT_BATCH_SIZE           32

/** Message queue growth factor */
#define PR_BIO_QUEUE_GROWTH_FACTOR          2

/** Initial message queue capacity */
#define PR_BIO_INITIAL_QUEUE_CAPACITY       128

/** Epsilon for floating-point comparisons */
#define PR_BIO_EPSILON                      1e-6f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief PR memory message types
 *
 * WHAT: Types of memory operations communicated via bio-async
 * WHY:  Different operations require different handling and subscribers
 * HOW:  Each type maps to specific bio-async channel for routing
 */
typedef enum {
    PR_MSG_ENCODE = 0,          /**< New memory encoded */
    PR_MSG_RETRIEVE,            /**< Memory retrieved */
    PR_MSG_CONSOLIDATE,         /**< Consolidation event (promotion/demotion) */
    PR_MSG_ENTANGLE,            /**< Memories entangled (new association) */
    PR_MSG_DECAY,               /**< Memory decayed (strength reduced) */
    PR_MSG_PROMOTE,             /**< Z-ladder promotion event */
    PR_MSG_DEMOTE,              /**< Z-ladder demotion event */
    PR_MSG_EVICT,               /**< Memory evicted from tier */
    PR_MSG_FLASHBULB,           /**< Flashbulb memory created */
    PR_MSG_REINFORCE,           /**< Memory reinforced */
    PR_MSG_RECONSOLIDATE,       /**< Memory reconsolidation */
    PR_MSG_TYPE_COUNT           /**< Number of message types (for arrays) */
} pr_message_type_t;

/**
 * @brief Message priority levels
 *
 * WHAT: Priority for message processing
 * WHY:  Urgent memory operations should be processed first
 * HOW:  Priority queue orders messages by level
 */
typedef enum {
    PR_PRIORITY_CRITICAL = 0,   /**< Highest priority (flashbulb, trauma) */
    PR_PRIORITY_HIGH,           /**< High priority (active operations) */
    PR_PRIORITY_NORMAL,         /**< Normal priority (background) */
    PR_PRIORITY_LOW,            /**< Low priority (housekeeping) */
    PR_PRIORITY_COUNT           /**< Number of priority levels */
} pr_message_priority_t;

/**
 * @brief Bridge error codes
 */
typedef enum {
    PR_BIO_SUCCESS = 0,                 /**< Operation succeeded */
    PR_BIO_ERROR_NULL_POINTER = -1,     /**< NULL pointer argument */
    PR_BIO_ERROR_NOT_CONNECTED = -2,    /**< Not connected to bio-async */
    PR_BIO_ERROR_QUEUE_FULL = -3,       /**< Message queue is full */
    PR_BIO_ERROR_INVALID_TYPE = -4,     /**< Invalid message type */
    PR_BIO_ERROR_SUBSCRIBER_LIMIT = -5, /**< Maximum subscribers reached */
    PR_BIO_ERROR_NOT_FOUND = -6,        /**< Subscriber not found */
    PR_BIO_ERROR_TIMEOUT = -7,          /**< Operation timed out */
    PR_BIO_ERROR_NO_MEMORY = -8,        /**< Memory allocation failed */
    PR_BIO_ERROR_INVALID_CONFIG = -9,   /**< Invalid configuration */
    PR_BIO_ERROR_ALREADY_CONNECTED = -10 /**< Already connected to bio-async */
} pr_bio_bridge_error_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief PR bio-async message
 *
 * WHAT: Message encapsulating a PR memory operation for bio-async transport
 * WHY:  Standardized format for inter-module communication
 * HOW:  Contains operation type, memory reference, and semantic state
 *
 * Memory layout: ~192 bytes
 */
typedef struct {
    pr_message_type_t type;             /**< Message type */
    pr_message_priority_t priority;     /**< Message priority */
    uint64_t memory_id;                 /**< Memory node ID */
    prime_signature_t signature;        /**< Content signature */
    nimcp_quaternion_t quaternion;      /**< Semantic state at event time */
    float importance;                   /**< Event importance [0-1] */
    float timestamp;                    /**< Event timestamp (ms) */
    pr_memory_tier_t tier;              /**< Current memory tier */
    pr_memory_tier_t previous_tier;     /**< Previous tier (for promotion/demotion) */
    uint64_t correlation_id;            /**< For request/response correlation */
    uint32_t sequence_number;           /**< Message sequence for ordering */
    bool requires_response;             /**< Whether response is expected */
    void* user_data;                    /**< Optional user context */
} pr_bio_message_t;

/**
 * @brief Subscriber callback function type
 *
 * @param message The received message
 * @param user_data User-provided context
 * @return true if message was handled, false to continue propagation
 */
typedef bool (*pr_bio_subscriber_callback_t)(
    const pr_bio_message_t* message,
    void* user_data
);

/**
 * @brief Subscriber registration
 *
 * WHAT: Registered subscriber for specific message types
 * WHY:  Track who receives which message types
 */
typedef struct {
    uint64_t subscriber_id;             /**< Unique subscriber identifier */
    pr_message_type_t type_filter;      /**< Which message type to receive */
    pr_bio_subscriber_callback_t callback; /**< Callback function */
    void* user_data;                    /**< User context for callback */
    pr_message_priority_t min_priority; /**< Minimum priority to receive */
    bool is_active;                     /**< Whether subscription is active */
} pr_bio_subscriber_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling bridge behavior
 * WHY:  Allow customization of queue sizes, timeouts, and behavior
 */
typedef struct {
    size_t max_pending_messages;        /**< Maximum queue size */
    size_t max_subscribers;             /**< Maximum total subscribers */
    uint64_t default_timeout_ms;        /**< Default operation timeout */
    size_t batch_process_size;          /**< Messages per batch processing */
    bool enable_statistics;             /**< Track performance statistics */
    bool enable_logging;                /**< Enable debug logging */
    bool auto_process_queue;            /**< Auto-process on send */
    nimcp_bio_channel_type_t default_channel; /**< Default bio-async channel */
} pr_bio_bridge_config_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Performance and operational metrics
 * WHY:  Monitor bridge health and performance
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_dropped;          /**< Messages dropped (queue full) */
    uint64_t messages_by_type[PR_MSG_TYPE_COUNT]; /**< Per-type counts */

    /* Queue statistics */
    size_t current_queue_depth;         /**< Current pending messages */
    size_t peak_queue_depth;            /**< Maximum queue depth seen */

    /* Subscriber statistics */
    size_t active_subscribers;          /**< Currently active subscribers */
    uint64_t notifications_sent;        /**< Total subscriber notifications */
    uint64_t callbacks_succeeded;       /**< Callbacks returning true */
    uint64_t callbacks_failed;          /**< Callbacks returning false */

    /* Timing statistics */
    float avg_latency_ms;               /**< Average message latency */
    float max_latency_ms;               /**< Maximum message latency */
    float avg_process_time_ms;          /**< Average processing time */

    /* Connection statistics */
    uint64_t reconnection_count;        /**< Times reconnected to bio-async */
    uint64_t last_activity_time_ms;     /**< Time of last activity */
} pr_bio_bridge_stats_t;

/**
 * @brief Opaque bridge handle
 */
typedef struct pr_bio_bridge_struct* pr_bio_bridge_t;

//=============================================================================
// Forward Declarations (for integration with PR components)
//=============================================================================

/* Forward declare entanglement graph for optional integration */
typedef struct entangle_graph_struct* entangle_graph_t;

/* Forward declare Z-ladder for optional integration */
typedef struct z_ladder_struct* z_ladder_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets balanced queue sizes and timeouts
 *
 * @return Default configuration with:
 *         - max_pending_messages: 4096
 *         - max_subscribers: 256
 *         - default_timeout_ms: 5000
 *         - batch_process_size: 32
 *         - enable_statistics: true
 *         - auto_process_queue: false
 *         - default_channel: BIO_CHANNEL_DOPAMINE
 *
 * Performance: ~5ns
 *
 * Example:
 *   pr_bio_bridge_config_t config = pr_bio_bridge_config_default();
 *   config.max_pending_messages = 8192;  // Larger queue
 *   pr_bio_bridge_t bridge = pr_bio_bridge_create(&config);
 */
NIMCP_EXPORT pr_bio_bridge_config_t pr_bio_bridge_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool pr_bio_bridge_config_validate(const pr_bio_bridge_config_t* config);

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

/**
 * @brief Create PR bio-async bridge
 *
 * WHAT: Allocates and initializes bridge for bio-async integration
 * WHY:  Entry point for PR-bio-async communication
 * HOW:  Creates message queue, subscriber registry, and state
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 *
 * Performance: O(max_pending) for queue allocation
 * Memory: ~2KB base + queue + subscriber arrays
 *
 * Example:
 *   pr_bio_bridge_t bridge = pr_bio_bridge_create(NULL);
 *   if (!bridge) {
 *       fprintf(stderr, "Failed to create bridge\n");
 *   }
 */
NIMCP_EXPORT pr_bio_bridge_t pr_bio_bridge_create(
    const pr_bio_bridge_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * WHAT: Deallocates bridge and all resources
 * WHY:  Clean shutdown
 * HOW:  Disconnects from bio-async, frees queue and subscribers
 *
 * @param bridge Bridge to destroy (NULL safe)
 *
 * Performance: O(pending + subscribers)
 *
 * WARNING: Pending messages will be dropped
 */
NIMCP_EXPORT void pr_bio_bridge_destroy(pr_bio_bridge_t bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect bridge to bio-async context
 *
 * WHAT: Establishes connection to bio-async infrastructure
 * WHY:  Required before sending/receiving messages
 * HOW:  Registers with bio-async system, starts message routing
 *
 * @param bridge Bridge handle
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Notes:
 * - Bio-async must be initialized before calling
 * - Returns error if already connected
 *
 * Example:
 *   pr_bio_bridge_error_t err = pr_bio_bridge_connect(bridge);
 *   if (err != PR_BIO_SUCCESS) {
 *       printf("Connect failed: %s\n", pr_bio_bridge_error_string(err));
 *   }
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_connect(pr_bio_bridge_t bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * WHAT: Disconnects from bio-async infrastructure
 * WHY:  Clean disconnection before shutdown or reconnection
 *
 * @param bridge Bridge handle
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_disconnect(pr_bio_bridge_t bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge handle
 * @return true if connected to bio-async
 *
 * Performance: O(1)
 */
NIMCP_EXPORT bool pr_bio_bridge_is_connected(const pr_bio_bridge_t bridge);

//=============================================================================
// Message Sending Functions
//=============================================================================

/**
 * @brief Send PR message through bio-async
 *
 * WHAT: Queues message for delivery via bio-async
 * WHY:  Primary method for inter-module communication
 * HOW:  Adds to priority queue, optionally triggers processing
 *
 * @param bridge Bridge handle
 * @param message Message to send (copied into queue)
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(log N) for priority queue insertion
 *
 * Notes:
 * - Message is copied; caller retains ownership of original
 * - If auto_process_queue is enabled, may trigger immediate delivery
 *
 * Example:
 *   pr_bio_message_t msg = {
 *       .type = PR_MSG_ENCODE,
 *       .memory_id = node->node_id,
 *       .importance = 0.8f
 *   };
 *   pr_bio_bridge_send(bridge, &msg);
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_send(
    pr_bio_bridge_t bridge,
    const pr_bio_message_t* message
);

/**
 * @brief Receive pending messages
 *
 * WHAT: Retrieves messages from queue
 * WHY:  Pull-based message retrieval
 * HOW:  Returns highest priority messages first
 *
 * @param bridge Bridge handle
 * @param messages Output array (caller-allocated)
 * @param max_messages Maximum messages to return
 * @param count Output: actual count retrieved
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(max_messages * log N)
 *
 * Example:
 *   pr_bio_message_t messages[32];
 *   size_t count;
 *   pr_bio_bridge_receive(bridge, messages, 32, &count);
 *   for (size_t i = 0; i < count; i++) {
 *       handle_message(&messages[i]);
 *   }
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_receive(
    pr_bio_bridge_t bridge,
    pr_bio_message_t* messages,
    size_t max_messages,
    size_t* count
);

/**
 * @brief Process pending messages (dispatch to subscribers)
 *
 * WHAT: Processes queued messages and dispatches to subscribers
 * WHY:  Batch processing for efficiency
 * HOW:  Dequeues by priority, invokes subscriber callbacks
 *
 * @param bridge Bridge handle
 * @return Number of messages processed
 *
 * Performance: O(processed * subscribers_per_type)
 *
 * Notes:
 * - Processes up to batch_process_size messages per call
 * - Higher priority messages processed first
 * - Callbacks invoked without lock held
 *
 * Example:
 *   // Process in main loop
 *   while (running) {
 *       size_t processed = pr_bio_bridge_process_pending(bridge);
 *       if (processed == 0) {
 *           sleep_ms(10);  // Nothing to process
 *       }
 *   }
 */
NIMCP_EXPORT size_t pr_bio_bridge_process_pending(pr_bio_bridge_t bridge);

//=============================================================================
// Subscriber Functions
//=============================================================================

/**
 * @brief Subscribe to message type
 *
 * WHAT: Register callback for specific message type
 * WHY:  Modules receive only messages they care about
 * HOW:  Adds subscriber to registry for message type
 *
 * @param bridge Bridge handle
 * @param type Message type to subscribe to
 * @param callback Callback function
 * @param user_data Context passed to callback
 * @param subscriber_id Output: assigned subscriber ID
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 *
 * Notes:
 * - Callback invoked for each matching message
 * - Callback receives message copy (can modify without affecting others)
 * - Return true from callback to mark handled, false to continue
 *
 * Example:
 *   bool on_encode(const pr_bio_message_t* msg, void* ctx) {
 *       printf("Memory %lu encoded\n", msg->memory_id);
 *       return true;
 *   }
 *   uint64_t sub_id;
 *   pr_bio_bridge_subscribe(bridge, PR_MSG_ENCODE, on_encode, NULL, &sub_id);
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_subscribe(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_bio_subscriber_callback_t callback,
    void* user_data,
    uint64_t* subscriber_id
);

/**
 * @brief Subscribe with priority filter
 *
 * WHAT: Subscribe with minimum priority threshold
 * WHY:  Receive only messages above certain priority
 *
 * @param bridge Bridge handle
 * @param type Message type
 * @param min_priority Minimum priority to receive
 * @param callback Callback function
 * @param user_data Context
 * @param subscriber_id Output: assigned ID
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_subscribe_with_priority(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_message_priority_t min_priority,
    pr_bio_subscriber_callback_t callback,
    void* user_data,
    uint64_t* subscriber_id
);

/**
 * @brief Unsubscribe from messages
 *
 * WHAT: Remove subscription by ID
 * WHY:  Stop receiving messages when no longer needed
 *
 * @param bridge Bridge handle
 * @param subscriber_id Subscriber ID from subscribe
 * @return PR_BIO_SUCCESS or PR_BIO_ERROR_NOT_FOUND
 *
 * Performance: O(S) where S = total subscribers
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_unsubscribe(
    pr_bio_bridge_t bridge,
    uint64_t subscriber_id
);

/**
 * @brief Unsubscribe all callbacks for a type
 *
 * @param bridge Bridge handle
 * @param type Message type
 * @return Number of subscriptions removed
 *
 * Performance: O(S)
 */
NIMCP_EXPORT size_t pr_bio_bridge_unsubscribe_all(
    pr_bio_bridge_t bridge,
    pr_message_type_t type
);

//=============================================================================
// Notification Helper Functions
//=============================================================================

/**
 * @brief Notify of memory encoding
 *
 * WHAT: Send notification that a new memory was encoded
 * WHY:  Convenience function for common operation
 * HOW:  Creates and sends PR_MSG_ENCODE message
 *
 * @param bridge Bridge handle
 * @param node Encoded memory node
 * @param importance Event importance [0-1]
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 *
 * Example:
 *   pr_memory_node_t* node = pr_memory_node_create(...);
 *   pr_bio_bridge_notify_encode(bridge, node, 0.7f);
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_encode(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float importance
);

/**
 * @brief Notify of memory retrieval
 *
 * WHAT: Send notification that a memory was retrieved
 * WHY:  Track memory access patterns
 *
 * @param bridge Bridge handle
 * @param node Retrieved memory node
 * @param resonance_score Retrieval resonance score [0-1]
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_retrieve(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float resonance_score
);

/**
 * @brief Notify of consolidation event
 *
 * WHAT: Send notification of tier transition
 * WHY:  Track memory consolidation process
 *
 * @param bridge Bridge handle
 * @param node Consolidated memory node
 * @param from_tier Previous tier
 * @param to_tier New tier
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_consolidate(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_memory_tier_t from_tier,
    pr_memory_tier_t to_tier
);

/**
 * @brief Notify of entanglement creation
 *
 * WHAT: Send notification that memories were entangled
 * WHY:  Track association formation
 *
 * @param bridge Bridge handle
 * @param node1_id First memory ID
 * @param node2_id Second memory ID
 * @param resonance Association strength [0-1]
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_entangle(
    pr_bio_bridge_t bridge,
    uint64_t node1_id,
    uint64_t node2_id,
    float resonance
);

/**
 * @brief Notify of memory decay
 *
 * WHAT: Send notification that memory strength decayed
 * WHY:  Track forgetting process
 *
 * @param bridge Bridge handle
 * @param memory_id Memory node ID
 * @param old_strength Previous strength
 * @param new_strength Current strength
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_decay(
    pr_bio_bridge_t bridge,
    uint64_t memory_id,
    float old_strength,
    float new_strength
);

/**
 * @brief Notify of flashbulb memory creation
 *
 * WHAT: Send high-priority notification for flashbulb memory
 * WHY:  Flashbulb events require immediate attention
 *
 * @param bridge Bridge handle
 * @param node Flashbulb memory node
 * @param arousal Arousal level at encoding [0-1]
 * @param valence Emotional valence [-1, +1]
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: ~100ns
 *
 * Notes:
 * - Uses PR_PRIORITY_CRITICAL
 * - Routes through NOREPINEPHRINE channel
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_notify_flashbulb(
    pr_bio_bridge_t bridge,
    const pr_memory_node_t* node,
    float arousal,
    float valence
);

//=============================================================================
// Priority Functions
//=============================================================================

/**
 * @brief Set default priority for message type
 *
 * WHAT: Configure default priority for a message type
 * WHY:  Customize priority mapping per application needs
 *
 * @param bridge Bridge handle
 * @param type Message type
 * @param priority Default priority for type
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_set_priority(
    pr_bio_bridge_t bridge,
    pr_message_type_t type,
    pr_message_priority_t priority
);

/**
 * @brief Get default priority for message type
 *
 * @param bridge Bridge handle
 * @param type Message type
 * @return Default priority for type
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_message_priority_t pr_bio_bridge_get_priority(
    const pr_bio_bridge_t bridge,
    pr_message_type_t type
);

/**
 * @brief Get bio-async channel for message type
 *
 * WHAT: Returns which neuromodulator channel is used for type
 * WHY:  Understanding message routing
 *
 * @param type Message type
 * @return Bio-async channel for this type
 *
 * Performance: O(1)
 */
NIMCP_EXPORT nimcp_bio_channel_type_t pr_bio_bridge_get_channel(
    pr_message_type_t type
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return PR_BIO_SUCCESS or error code
 *
 * Performance: O(1)
 */
NIMCP_EXPORT pr_bio_bridge_error_t pr_bio_bridge_get_stats(
    const pr_bio_bridge_t bridge,
    pr_bio_bridge_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * @param bridge Bridge handle
 */
NIMCP_EXPORT void pr_bio_bridge_reset_stats(pr_bio_bridge_t bridge);

/**
 * @brief Get current queue depth
 *
 * @param bridge Bridge handle
 * @return Number of pending messages
 *
 * Performance: O(1)
 */
NIMCP_EXPORT size_t pr_bio_bridge_get_queue_depth(const pr_bio_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* pr_bio_bridge_error_string(pr_bio_bridge_error_t error);

/**
 * @brief Get message type name as string
 *
 * @param type Message type
 * @return Static string name (e.g., "ENCODE", "RETRIEVE")
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* pr_bio_bridge_type_name(pr_message_type_t type);

/**
 * @brief Get priority name as string
 *
 * @param priority Priority level
 * @return Static string name (e.g., "CRITICAL", "HIGH")
 *
 * Performance: ~5ns
 */
NIMCP_EXPORT const char* pr_bio_bridge_priority_name(pr_message_priority_t priority);

/**
 * @brief Initialize message with defaults
 *
 * @param message Message to initialize
 * @param type Message type
 */
NIMCP_EXPORT void pr_bio_bridge_init_message(
    pr_bio_message_t* message,
    pr_message_type_t type
);

/**
 * @brief Create message from memory node
 *
 * WHAT: Populate message fields from memory node
 * WHY:  Convenience for creating notification messages
 *
 * @param message Message to populate
 * @param node Source memory node
 * @param type Message type
 */
NIMCP_EXPORT void pr_bio_bridge_message_from_node(
    pr_bio_message_t* message,
    const pr_memory_node_t* node,
    pr_message_type_t type
);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since epoch
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT uint64_t pr_bio_bridge_current_time_ms(void);

/**
 * @brief Print message to stdout for debugging
 *
 * @param message Message to print
 */
NIMCP_EXPORT void pr_bio_bridge_print_message(const pr_bio_message_t* message);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bridge to summarize
 */
NIMCP_EXPORT void pr_bio_bridge_print_summary(const pr_bio_bridge_t bridge);

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Check if message type is high priority
 *
 * @param type Message type
 * @return true if typically high priority
 */
static inline bool pr_bio_is_high_priority_type(pr_message_type_t type) {
    return type == PR_MSG_FLASHBULB || type == PR_MSG_ENCODE ||
           type == PR_MSG_RETRIEVE;
}

/**
 * @brief Check if message requires immediate processing
 *
 * @param msg Message to check
 * @return true if critical priority
 */
static inline bool pr_bio_is_urgent(const pr_bio_message_t* msg) {
    return msg && msg->priority == PR_PRIORITY_CRITICAL;
}

/**
 * @brief Get bio-async channel for message
 *
 * @param msg Message
 * @return Appropriate bio-async channel
 */
static inline nimcp_bio_channel_type_t pr_bio_message_channel(
    const pr_bio_message_t* msg
) {
    if (!msg) return BIO_CHANNEL_DOPAMINE;
    return pr_bio_bridge_get_channel(msg->type);
}

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_BIO_BRIDGE_H
