/**
 * @file nimcp_collective_hub_bridge.h
 * @brief Collective Cognition - Cognitive Integration Hub Bridge
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Bridge connecting collective cognition system to the cognitive integration hub,
 *       enabling distributed consciousness to participate in cross-module event routing.
 *
 * WHY: Collective cognition needs to:
 *      - Receive events from other cognitive modules (social signals, state changes)
 *      - Publish collective consensus events for system-wide coordination
 *      - Broadcast integrated information updates when phi/coherence changes
 *      - Respond to queries about collective state
 *
 * HOW: Registers collective cognition with the hub under COG_CATEGORY_SOCIAL,
 *      subscribes to relevant events, and publishes consensus/state events.
 *
 * THEORETICAL BASIS:
 * - Integrated Information Theory (IIT): Collective phi represents integrated information
 *   across distributed brain instances
 * - Global Workspace Theory: Consensus events broadcast to global workspace
 * - Predictive Processing: Collective predictions propagate through event system
 *
 * INTEGRATION PATTERN:
 * +-----------------------+          +-----------------------+
 * |  Collective Cognition |<-------->|  Cognitive Hub Bridge |
 * |   - Hyperscanning     |          |   - Event routing     |
 * |   - Collective Phi    |          |   - Query handling    |
 * |   - Shared Intent     |          |   - Stats tracking    |
 * +-----------------------+          +-----------------------+
 *                                              |
 *                                              v
 *                                    +-------------------+
 *                                    | Integration Hub   |
 *                                    | - Other modules   |
 *                                    | - Event broadcast |
 *                                    +-------------------+
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_COLLECTIVE_HUB_BRIDGE_H
#define NIMCP_COLLECTIVE_HUB_BRIDGE_H

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
struct collective_cognition;
typedef struct collective_cognition collective_cognition_t;

/* ============================================================================
 * Opaque Type
 * ============================================================================ */

/**
 * @brief Opaque Collective-Hub bridge structure
 *
 * WHAT: Forward declaration for bridge instance
 * WHY: Encapsulates implementation details
 * HOW: Full definition in implementation file
 */
typedef struct collective_hub_bridge collective_hub_bridge_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/**
 * @brief Module ID for collective cognition in the integration hub
 */
#define COLLECTIVE_HUB_MODULE_ID 0x2001

/**
 * @brief Module name for registration
 */
#define COLLECTIVE_HUB_MODULE_NAME "CollectiveCognition"

/**
 * @brief Default threshold for phi change to trigger event
 */
#define COLLECTIVE_HUB_PHI_THRESHOLD 0.05f

/**
 * @brief Default threshold for coherence change to trigger event
 */
#define COLLECTIVE_HUB_COHERENCE_THRESHOLD 0.05f

/**
 * @brief Maximum queued events before dropping
 */
#define COLLECTIVE_HUB_MAX_QUEUED_EVENTS 128

/* ============================================================================
 * Event Type Definitions
 * ============================================================================ */

/**
 * @brief Types of collective events that can be published
 *
 * WHAT: Enumeration of collective-specific event subtypes
 * WHY: Allow subscribers to filter on specific collective event types
 * HOW: Included in event payload for detailed routing
 */
typedef enum {
    /** Collective consensus reached on a decision */
    COLLECTIVE_EVENT_CONSENSUS_REACHED = 0,

    /** Phi value changed significantly */
    COLLECTIVE_EVENT_PHI_CHANGED,

    /** Coherence level changed significantly */
    COLLECTIVE_EVENT_COHERENCE_CHANGED,

    /** Neural synchronization achieved (entrainment) */
    COLLECTIVE_EVENT_ENTRAINMENT_ACHIEVED,

    /** Instance joined the collective */
    COLLECTIVE_EVENT_INSTANCE_JOINED,

    /** Instance left the collective */
    COLLECTIVE_EVENT_INSTANCE_LEFT,

    /** Shared goal proposed */
    COLLECTIVE_EVENT_GOAL_PROPOSED,

    /** Shared goal accepted */
    COLLECTIVE_EVENT_GOAL_ACCEPTED,

    /** Shared goal completed */
    COLLECTIVE_EVENT_GOAL_COMPLETED,

    /** Cognitive load rebalanced */
    COLLECTIVE_EVENT_LOAD_REBALANCED,

    /** Count of event types */
    COLLECTIVE_EVENT_COUNT
} collective_event_subtype_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Configuration for Collective-Hub bridge
 *
 * WHAT: Parameters controlling bridge behavior
 * WHY: Customize event thresholds and publishing behavior
 * HOW: Applied at bridge creation time
 */
typedef struct {
    /** Custom module ID (0 = use default) */
    uint32_t module_id;

    /** Phi change threshold to trigger state change event [0-1] */
    float phi_change_threshold;

    /** Coherence change threshold to trigger state change event [0-1] */
    float coherence_change_threshold;

    /** Enable automatic event publishing on state changes */
    bool enable_auto_publish;

    /** Enable subscription to social signals */
    bool enable_social_subscription;

    /** Enable subscription to state changes */
    bool enable_state_subscription;

    /** Enable subscription to decision events */
    bool enable_decision_subscription;

    /** Enable query handling */
    bool enable_query_handling;

    /** Priority for published events */
    uint32_t event_priority;
} collective_hub_bridge_config_t;

/**
 * @brief Consensus data for publishing consensus events
 *
 * WHAT: Data describing a collective consensus event
 * WHY: Communicate consensus details to other modules
 * HOW: Payload for COG_EVENT_CONSOLIDATION events
 */
typedef struct {
    /** Type of consensus reached */
    uint32_t consensus_type;

    /** Confidence in the consensus [0-1] */
    float confidence;

    /** Number of instances that agreed */
    uint32_t agreeing_instances;

    /** Total instances in collective */
    uint32_t total_instances;

    /** Phi value at consensus */
    float phi_at_consensus;

    /** Coherence at consensus */
    float coherence_at_consensus;

    /** Consensus data payload */
    void* data;

    /** Size of consensus data */
    size_t data_size;
} collective_consensus_data_t;

/**
 * @brief State change payload for collective state events
 *
 * WHAT: Data describing a collective state change
 * WHY: Communicate state changes to interested modules
 * HOW: Payload for COG_EVENT_STATE_CHANGE events
 */
typedef struct {
    /** Subtype of the collective event */
    collective_event_subtype_t subtype;

    /** Previous phi value */
    float phi_old;

    /** New phi value */
    float phi_new;

    /** Previous coherence value */
    float coherence_old;

    /** New coherence value */
    float coherence_new;

    /** Number of active instances */
    uint32_t active_instances;

    /** Is collective synchronized */
    bool is_entrained;

    /** Timestamp of change (microseconds) */
    uint64_t timestamp_us;
} collective_state_change_t;

/**
 * @brief Statistics for Collective-Hub bridge
 *
 * WHAT: Operational metrics for the bridge
 * WHY: Monitor bridge health and activity
 * HOW: Accumulated during bridge operation
 */
typedef struct {
    /** Total events received from hub */
    uint32_t events_received;

    /** Total events published to hub */
    uint32_t events_published;

    /** Total queries handled */
    uint32_t queries_handled;

    /** Number of times consensus was reached */
    uint32_t consensus_reached;

    /** Number of phi update events published */
    uint32_t phi_updates;

    /** Average coherence across all updates */
    float avg_coherence;

    /** Social signals received */
    uint32_t social_signals_received;

    /** State changes received */
    uint32_t state_changes_received;

    /** Decisions received */
    uint32_t decisions_received;

    /** Events dropped due to queue full */
    uint32_t events_dropped;

    /** Query errors */
    uint32_t query_errors;
} collective_hub_bridge_stats_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default Collective-Hub bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY: Provide easy starting point for bridge setup
 * HOW: Sets balanced thresholds and enables core functionality
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 *
 * DEFAULT VALUES:
 * - module_id: COLLECTIVE_HUB_MODULE_ID
 * - phi_change_threshold: 0.05
 * - coherence_change_threshold: 0.05
 * - enable_auto_publish: true
 * - enable_social_subscription: true
 * - enable_state_subscription: true
 * - enable_decision_subscription: true
 * - enable_query_handling: true
 * - event_priority: COG_PRIORITY_NORMAL
 */
int collective_hub_bridge_default_config(collective_hub_bridge_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create Collective-Hub bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY: Create the connection point between collective cognition and hub
 * HOW: Allocate memory, initialize state, prepare for connection
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge instance or NULL on failure
 *
 * MEMORY: Caller must call collective_hub_bridge_destroy() when done
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
collective_hub_bridge_t* collective_hub_bridge_create(
    const collective_hub_bridge_config_t* config
);

/**
 * @brief Destroy Collective-Hub bridge
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
void collective_hub_bridge_destroy(collective_hub_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to hub and collective cognition
 *
 * WHAT: Register collective cognition with hub and set up subscriptions
 * WHY: Enable bidirectional communication between collective and hub
 * HOW: Register module, subscribe to events, register query handler
 *
 * @param bridge Bridge instance
 * @param hub Cognitive integration hub
 * @param collective Collective cognition system
 * @return 0 on success, -1 on error
 *
 * ACTIONS:
 * - Registers collective cognition as COG_CATEGORY_SOCIAL module
 * - Subscribes to COG_EVENT_SOCIAL_SIGNAL, COG_EVENT_STATE_CHANGE,
 *   COG_EVENT_DECISION_MADE (based on config)
 * - Registers query handler for collective state queries
 *
 * ERRORS:
 * - Returns -1 if bridge is NULL
 * - Returns -1 if hub is NULL
 * - Returns -1 if collective is NULL
 * - Returns -1 if already connected
 * - Returns -1 if registration fails
 *
 * COMPLEXITY: O(subscriptions)
 * THREAD-SAFE: Yes
 */
int collective_hub_bridge_connect(
    collective_hub_bridge_t* bridge,
    cognitive_integration_hub_t hub,
    collective_cognition_t* collective
);

/**
 * @brief Disconnect bridge from hub
 *
 * WHAT: Unregister from hub and clean up subscriptions
 * WHY: Clean disconnection when collective is shutting down
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
int collective_hub_bridge_disconnect(collective_hub_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bridge instance
 * @return true if connected, false otherwise
 */
bool collective_hub_bridge_is_connected(collective_hub_bridge_t* bridge);

/* ============================================================================
 * Event Callback API
 * ============================================================================ */

/**
 * @brief Event callback for subscribed hub events
 *
 * WHAT: Callback invoked when subscribed events occur
 * WHY: Process events from other cognitive modules
 * HOW: Routes events to collective cognition for processing
 *
 * @param event Event data from hub
 * @param user_data Bridge instance (passed during subscription)
 * @return 0 on success, -1 on error
 *
 * EVENT HANDLING:
 * - COG_EVENT_SOCIAL_SIGNAL: Integrates social information into collective
 * - COG_EVENT_STATE_CHANGE: Updates collective based on individual changes
 * - COG_EVENT_DECISION_MADE: Propagates decisions through collective
 *
 * THREAD SAFETY: May be called from any thread
 * BLOCKING: Should not block - queues for later processing if needed
 */
int collective_hub_on_event(
    const void* event,
    void* user_data
);

/* ============================================================================
 * Publishing API
 * ============================================================================ */

/**
 * @brief Publish consensus event to hub
 *
 * WHAT: Broadcast that collective consensus has been reached
 * WHY: Notify other modules of collective decision
 * HOW: Publishes COG_EVENT_CONSOLIDATION with consensus payload
 *
 * @param bridge Bridge instance
 * @param consensus_data Consensus information to publish
 * @return 0 on success, -1 on error
 *
 * ERRORS:
 * - Returns -1 if bridge is NULL
 * - Returns -1 if not connected
 * - Returns -1 if consensus_data is NULL
 * - Returns -1 if publish fails
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int collective_hub_publish_consensus(
    collective_hub_bridge_t* bridge,
    const collective_consensus_data_t* consensus_data
);

/**
 * @brief Publish state change event to hub
 *
 * WHAT: Broadcast collective state change (phi, coherence, etc.)
 * WHY: Notify interested modules of collective state transitions
 * HOW: Publishes COG_EVENT_STATE_CHANGE with state payload
 *
 * @param bridge Bridge instance
 * @param state_change State change information to publish
 * @return 0 on success, -1 on error
 *
 * AUTO-PUBLISH: If enable_auto_publish is true, this is called
 * automatically when phi/coherence changes exceed thresholds.
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int collective_hub_publish_state_change(
    collective_hub_bridge_t* bridge,
    const collective_state_change_t* state_change
);

/**
 * @brief Publish collective event by subtype
 *
 * WHAT: Publish a specific type of collective event
 * WHY: Flexible event publishing for various collective events
 * HOW: Creates appropriate event data and publishes to hub
 *
 * @param bridge Bridge instance
 * @param subtype Type of collective event
 * @param data Event-specific data
 * @param data_size Size of event data
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(subscribers)
 * THREAD-SAFE: Yes
 */
int collective_hub_publish_event(
    collective_hub_bridge_t* bridge,
    collective_event_subtype_t subtype,
    const void* data,
    size_t data_size
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge with current collective state
 *
 * WHAT: Check for state changes and publish events if thresholds exceeded
 * WHY: Automatic event publishing based on collective state changes
 * HOW: Compares current state to previous, publishes if changed significantly
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 *
 * RECOMMENDED: Call this periodically (e.g., after collective_cognition_update)
 *
 * COMPLEXITY: O(1) for check, O(subscribers) if publishing
 * THREAD-SAFE: Yes
 */
int collective_hub_bridge_update(collective_hub_bridge_t* bridge);

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
int collective_hub_bridge_get_stats(
    const collective_hub_bridge_t* bridge,
    collective_hub_bridge_stats_t* stats
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
int collective_hub_bridge_reset_stats(collective_hub_bridge_t* bridge);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Get string name for collective event subtype
 *
 * @param subtype Event subtype
 * @return Human-readable name or "UNKNOWN"
 */
const char* collective_event_subtype_to_string(collective_event_subtype_t subtype);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COLLECTIVE_HUB_BRIDGE_H */
