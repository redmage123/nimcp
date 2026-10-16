/**
 * @file nimcp_hypothalamus_orchestrator.h
 * @brief Central orchestrator for hypothalamus module integration and drive coordination
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Central orchestrator that coordinates all hypothalamus bridges, enabling
 *       event-driven drive regulation and unified homeostatic response.
 *
 * WHY: Hypothalamus bridges need to communicate without tight coupling. The orchestrator
 *      provides drive event routing, unified state assessment, and coordinated response.
 *      This implements Steve Byrnes' "Steering Subsystem" concept for AGI alignment.
 *
 * HOW: Bridges register with the orchestrator, subscribe to drive events, and publish
 *      homeostatic events. The orchestrator aggregates drive states and coordinates responses.
 *
 * DESIGN PATTERNS:
 * - Mediator: Orchestrator mediates all inter-bridge communication
 * - Observer: Publish-subscribe drive event system
 * - Registry: Bridge registration and lookup
 * - Strategy: Pluggable drive aggregation strategies
 * - Chain of Responsibility: Cascading homeostatic response
 *
 * THREAD SAFETY: All functions are thread-safe. The orchestrator uses internal
 * synchronization for concurrent access from multiple bridges.
 *
 * PERFORMANCE:
 * - Event publish: O(s) where s = number of subscribers
 * - Bridge lookup: O(1) with hash-based registry
 * - Drive aggregation: O(b) where b = number of bridges
 * - Async publish: O(1) - queued for later processing
 *
 * BRIDGES COORDINATED:
 * - Emotion Bridge (Affective drive modulation)
 * - Executive Bridge (Drive-goal integration)
 * - Attention Bridge (Salience-driven focusing)
 * - Sleep Bridge (Circadian regulation)
 * - Immune Bridge (Neuroimmune coordination)
 * - Wellbeing Bridge (Hedonic/eudaimonic balance)
 * - Memory Bridge (Drive-memory consolidation)
 * - Perception Bridge (Interoceptive processing)
 * - Salience Bridge (Drive prioritization)
 * - Global Workspace Bridge (Conscious drive access)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HYPOTHALAMUS_ORCHESTRATOR_H
#define NIMCP_HYPOTHALAMUS_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * OPAQUE TYPE DECLARATIONS
 * ============================================================================ */

/**
 * WHAT: Opaque handle to hypothalamus orchestrator
 * WHY: Encapsulation - hide internal implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct hypo_orchestrator_struct* hypo_orchestrator_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Maximum number of bridges that can register */
#define HYPO_ORCH_MAX_BRIDGES 32

/** Maximum number of event subscriptions */
#define HYPO_ORCH_MAX_SUBSCRIPTIONS 256

/** Maximum async event queue size */
#define HYPO_ORCH_MAX_EVENT_QUEUE 512

/** Maximum drive history entries */
#define HYPO_ORCH_MAX_DRIVE_HISTORY 256

/** Maximum bridge name length */
#define HYPO_ORCH_MAX_NAME_LEN 64

/** Default drive decay rate (per second) */
#define HYPO_ORCH_DEFAULT_DRIVE_DECAY 0.05f

/** Default urgent drive threshold */
#define HYPO_ORCH_DEFAULT_URGENT_THRESHOLD 0.9f

/** Default elevated drive threshold */
#define HYPO_ORCH_DEFAULT_ELEVATED_THRESHOLD 0.7f

/** Default moderate drive threshold */
#define HYPO_ORCH_DEFAULT_MODERATE_THRESHOLD 0.4f

/* ============================================================================
 * HYPOTHALAMUS BRIDGE TYPES
 * ============================================================================ */

/**
 * WHAT: Enumeration of hypothalamus bridge types
 * WHY: Identify and categorize registered bridges
 * HOW: Each bridge type has unique identifier for routing
 */
typedef enum {
    HYPO_BRIDGE_UNKNOWN = 0,
    HYPO_BRIDGE_EMOTION,            /**< Affective drive modulation */
    HYPO_BRIDGE_EXECUTIVE,          /**< Drive-goal integration */
    HYPO_BRIDGE_ATTENTION,          /**< Salience-driven focusing */
    HYPO_BRIDGE_SLEEP,              /**< Circadian regulation */
    HYPO_BRIDGE_IMMUNE,             /**< Neuroimmune coordination */
    HYPO_BRIDGE_WELLBEING,          /**< Hedonic/eudaimonic balance */
    HYPO_BRIDGE_MEMORY,             /**< Drive-memory consolidation */
    HYPO_BRIDGE_PERCEPTION,         /**< Interoceptive processing */
    HYPO_BRIDGE_SALIENCE,           /**< Drive prioritization */
    HYPO_BRIDGE_REASONING,          /**< Drive-reasoning integration */
    HYPO_BRIDGE_GLOBAL_WORKSPACE,   /**< Conscious drive access */
    HYPO_BRIDGE_INTROSPECTION,      /**< Self-monitoring of drives */
    HYPO_BRIDGE_CURIOSITY,          /**< Exploration drive coordination */
    HYPO_BRIDGE_GAME_THEORY,        /**< Strategic drive management */
    HYPO_BRIDGE_IMAGINATION,        /**< Prospective drive simulation */
    HYPO_BRIDGE_EPISTEMIC,          /**< Knowledge-seeking drive */
    HYPO_BRIDGE_COLLECTIVE,         /**< Social drive coordination */
    HYPO_BRIDGE_BIAS,               /**< Drive bias regulation */
    HYPO_BRIDGE_THEORY_OF_MIND,     /**< Social drive modeling */
    HYPO_BRIDGE_PREDICTIVE,         /**< Predictive drive processing */
    HYPO_BRIDGE_LOGGING,            /**< Logging integration */
    HYPO_BRIDGE_BIO_ASYNC,          /**< Bio-async router integration */
    HYPO_BRIDGE_COUNT               /**< Total bridge type count */
} hypo_bridge_type_t;

/* ============================================================================
 * HYPOTHALAMUS EVENT TYPES
 * ============================================================================ */

/**
 * WHAT: Hypothalamus event types for inter-bridge communication
 * WHY: Categorize events for routing and filtering
 * HOW: Each event type represents a drive-relevant occurrence
 */
typedef enum {
    /* Drive State Events */
    HYPO_EVENT_DRIVE_ACTIVATED = 0, /**< Drive became active/urgent */
    HYPO_EVENT_DRIVE_SATISFIED,     /**< Drive reached satisfaction */
    HYPO_EVENT_DRIVE_CONFLICT,      /**< Multiple drives in conflict */
    HYPO_EVENT_HOMEOSTATIC_ALERT,   /**< Homeostatic deviation detected */
    HYPO_EVENT_CIRCADIAN_PHASE,     /**< Circadian phase transition */
    HYPO_EVENT_STRESS_RESPONSE,     /**< Stress response initiated */
    HYPO_EVENT_AUTONOMIC_SHIFT,     /**< Autonomic state change */
    HYPO_EVENT_ALIGNMENT_CHECK,     /**< Alignment verification event */
    HYPO_EVENT_REWARD_SIGNAL,       /**< Reward signal broadcast */
    HYPO_EVENT_SETPOINT_CHANGE,     /**< Drive setpoint modified */
    HYPO_EVENT_COUNT                /**< Total event type count */
} hypo_event_type_t;

/**
 * WHAT: Drive urgency levels
 * WHY: Categorize drive urgency for response prioritization
 * HOW: Numerical levels mapped to response actions
 */
typedef enum {
    HYPO_URGENCY_NONE = 0,          /**< No urgency */
    HYPO_URGENCY_LOW,               /**< Low urgency - monitor */
    HYPO_URGENCY_MODERATE,          /**< Moderate urgency - attend */
    HYPO_URGENCY_ELEVATED,          /**< Elevated urgency - prioritize */
    HYPO_URGENCY_URGENT             /**< Urgent - immediate action */
} hypo_urgency_t;

/**
 * WHAT: Orchestrator operational states
 * WHY: Track orchestrator lifecycle and mode
 * HOW: State machine for orchestrator behavior
 */
typedef enum {
    HYPO_ORCH_STATE_UNINITIALIZED = 0,  /**< Not yet initialized */
    HYPO_ORCH_STATE_IDLE,               /**< Initialized, drives balanced */
    HYPO_ORCH_STATE_MONITORING,         /**< Actively monitoring drives */
    HYPO_ORCH_STATE_REGULATING,         /**< Active drive regulation */
    HYPO_ORCH_STATE_CONFLICT,           /**< Resolving drive conflict */
    HYPO_ORCH_STATE_STRESS,             /**< Stress response active */
    HYPO_ORCH_STATE_RECOVERY,           /**< Recovering from stress */
    HYPO_ORCH_STATE_ERROR               /**< Error state */
} hypo_orch_state_t;

/* ============================================================================
 * EVENT DATA STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Hypothalamus event data payload
 * WHY: Carry event-specific information
 * HOW: Union with type-specific fields
 */
typedef struct {
    hypo_event_type_t event_type;       /**< Type of event */
    hypo_bridge_type_t source;          /**< Bridge that generated event */
    hypo_urgency_t urgency;             /**< Event urgency level */
    uint64_t timestamp;                 /**< Event timestamp (microseconds) */
    uint32_t event_id;                  /**< Unique event identifier */

    union {
        /** Drive activation event data */
        struct {
            uint32_t drive_type;        /**< Type of drive activated */
            float drive_level;          /**< Current drive level [0, 1] */
            float deviation;            /**< Deviation from setpoint */
            float urgency_weight;       /**< Urgency weighting */
            char description[128];      /**< Drive description */
        } drive;

        /** Homeostatic event data */
        struct {
            uint32_t variable_id;       /**< Homeostatic variable */
            float current_value;        /**< Current value */
            float setpoint;             /**< Target setpoint */
            float deviation;            /**< Deviation amount */
            bool is_critical;           /**< Whether deviation is critical */
        } homeostatic;

        /** Circadian event data */
        struct {
            uint32_t phase;             /**< Current circadian phase */
            float phase_progress;       /**< Progress within phase [0, 1] */
            float alertness;            /**< Current alertness level */
            uint64_t next_transition;   /**< Time of next transition */
        } circadian;

        /** Stress event data */
        struct {
            float stress_level;         /**< Current stress level [0, 1] */
            uint32_t stressor_type;     /**< Type of stressor */
            float cortisol_level;       /**< Simulated cortisol level */
            bool is_acute;              /**< Acute vs chronic stress */
        } stress;

        /** Reward event data */
        struct {
            float reward_magnitude;     /**< Reward signal magnitude */
            uint32_t source_drive;      /**< Drive that generated reward */
            float prediction_error;     /**< Reward prediction error */
            bool is_intrinsic;          /**< Intrinsic vs extrinsic */
        } reward;

        /** Alignment event data */
        struct {
            float alignment_score;      /**< Overall alignment score */
            uint32_t checked_drives;    /**< Number of drives checked */
            uint32_t violations;        /**< Number of violations */
            bool locked;                /**< Whether drives are locked */
        } alignment;

        /** Bridge lifecycle event data */
        struct {
            hypo_bridge_type_t bridge_type;  /**< Bridge type */
            uint32_t bridge_id;              /**< Bridge instance ID */
            char bridge_name[64];            /**< Bridge name */
        } bridge;

        /** Generic data for custom events */
        struct {
            uint32_t code;              /**< Custom event code */
            uint32_t flags;             /**< Custom flags */
            void* data;                 /**< Custom data pointer */
            size_t data_size;           /**< Size of custom data */
        } custom;
    };
} hypo_event_data_t;

/**
 * WHAT: Callback function type for hypothalamus events
 * WHY: Allow bridges to receive event notifications
 * HOW: Function pointer with event data and user context
 *
 * @param event Event data
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
typedef int (*hypo_event_callback_t)(
    const hypo_event_data_t* event,
    void* user_data
);

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for hypothalamus orchestrator
 * WHY: Customize orchestrator behavior at creation time
 * HOW: Struct with all orchestrator configuration parameters
 */
typedef struct {
    uint32_t max_bridges;               /**< Maximum registered bridges */
    uint32_t max_subscriptions;         /**< Maximum total subscriptions */
    bool enable_async;                  /**< Enable async event delivery */
    uint32_t event_queue_size;          /**< Async event queue size */

    /* Drive thresholds */
    float urgent_threshold;             /**< Urgent drive level threshold */
    float elevated_threshold;           /**< Elevated drive level threshold */
    float moderate_threshold;           /**< Moderate drive level threshold */
    float drive_decay_rate;             /**< Drive level decay per second */

    /* Response configuration */
    bool auto_regulate;                 /**< Auto-regulate on urgent drives */
    bool enable_cascade;                /**< Enable cascading drive propagation */
    bool enable_recovery;               /**< Enable auto-recovery after stress */
    uint32_t regulation_timeout_ms;     /**< Regulation timeout (0 = manual) */

    /* Integration options */
    bool connect_immune;                /**< Auto-connect to immune system */
    bool connect_bio_async;             /**< Auto-connect to bio-async router */
    bool enable_logging;                /**< Enable drive event logging */
} hypo_orch_config_t;

/**
 * WHAT: Information about a registered hypothalamus bridge
 * WHY: Track bridge metadata for routing and coordination
 * HOW: Struct with identification and state
 */
typedef struct {
    uint32_t bridge_id;                 /**< Unique bridge instance ID */
    hypo_bridge_type_t type;            /**< Bridge type */
    char name[HYPO_ORCH_MAX_NAME_LEN];  /**< Human-readable name */
    void* bridge_handle;                /**< Bridge handle pointer */
    void* context;                      /**< Bridge-provided context */
    bool is_active;                     /**< Whether bridge is active */
    bool is_connected;                  /**< Whether bridge is connected */
    float current_drive_level;          /**< Bridge's current drive level */
    uint64_t last_event_time;           /**< Last event timestamp */
    uint32_t events_published;          /**< Events published by bridge */
    uint32_t drives_reported;           /**< Drives reported by bridge */
} hypo_bridge_info_t;

/**
 * WHAT: Unified drive state from all bridges
 * WHY: Aggregate drive information for coordinated response
 * HOW: Weighted combination of bridge drive levels
 * NOTE: Named hypo_unified_drive_state_t to avoid conflict with hypo_drive_state_t in drives.h
 */
typedef struct {
    float unified_drive_level;          /**< Combined drive level [0, 1] */
    hypo_urgency_t urgency;             /**< Overall urgency assessment */
    uint32_t active_drives;             /**< Number of active drives */
    uint32_t bridges_reporting;         /**< Number of bridges reporting */

    /* Per-bridge drive breakdown */
    struct {
        hypo_bridge_type_t type;        /**< Bridge type */
        float drive_level;              /**< Bridge drive level */
        uint32_t drive_count;           /**< Number of drives */
        bool is_primary_source;         /**< Is primary drive source */
    } bridge_drives[HYPO_BRIDGE_COUNT];

    /* Highest priority drives */
    uint32_t primary_drive_type;        /**< Primary active drive */
    hypo_bridge_type_t primary_source;  /**< Primary drive source */
    char drive_summary[256];            /**< Human-readable drive summary */

    uint64_t assessment_time;           /**< When assessment was made */
    uint64_t next_decay_time;           /**< When next decay occurs */
} hypo_unified_drive_state_t;

/**
 * WHAT: Statistics about orchestrator operation
 * WHY: Monitor orchestrator performance and health
 * HOW: Counters and metrics collected during operation
 */
typedef struct {
    /* Bridge statistics */
    uint32_t registered_bridges;        /**< Currently registered bridges */
    uint32_t active_bridges;            /**< Currently active bridges */
    uint32_t connected_bridges;         /**< Currently connected bridges */

    /* Event statistics */
    uint32_t active_subscriptions;      /**< Currently active subscriptions */
    uint64_t events_published;          /**< Total events published */
    uint64_t events_delivered;          /**< Total events delivered */
    uint64_t events_dropped;            /**< Events dropped (queue full) */
    uint32_t async_queue_depth;         /**< Current async queue depth */
    uint32_t async_queue_max;           /**< Maximum queue depth seen */

    /* Drive statistics */
    uint64_t drives_activated;          /**< Total drives activated */
    uint64_t drives_satisfied;          /**< Drives successfully satisfied */
    uint64_t conflicts_detected;        /**< Drive conflicts detected */
    uint32_t stress_responses;          /**< Number of stress responses */
    float avg_drive_level;              /**< Average drive level */
    float peak_drive_level;             /**< Peak drive level seen */

    /* Timing statistics */
    uint64_t avg_event_latency_us;      /**< Average event delivery latency */
    uint64_t avg_response_time_us;      /**< Average drive response time */
    uint64_t uptime_us;                 /**< Orchestrator uptime */
} hypo_orch_stats_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default orchestrator configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - max_bridges: 32
 * - max_subscriptions: 256
 * - enable_async: true
 * - event_queue_size: 512
 * - urgent_threshold: 0.9
 * - elevated_threshold: 0.7
 * - moderate_threshold: 0.4
 * - drive_decay_rate: 0.05
 * - auto_regulate: true
 * - enable_cascade: true
 * - enable_recovery: true
 * - regulation_timeout_ms: 60000
 *
 * @param config Output: configuration structure
 * @return 0 on success, -1 on failure
 */
int hypo_orch_default_config(hypo_orch_config_t* config);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new hypothalamus orchestrator
 * WHY: Initialize orchestrator for bridge registration and event routing
 * HOW: Allocate resources, initialize internal structures
 *
 * @param config Orchestrator configuration (NULL for defaults)
 * @return Orchestrator handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if async thread creation fails (when async enabled)
 *
 * MEMORY: Caller must call hypo_orch_destroy() when done
 */
hypo_orchestrator_t hypo_orch_create(const hypo_orch_config_t* config);

/**
 * WHAT: Destroy hypothalamus orchestrator
 * WHY: Release all resources and cleanup
 * HOW: Stop async thread, unregister bridges, free memory
 *
 * @param orch Orchestrator to destroy
 *
 * BLOCKING: May block waiting for async queue to drain
 * SAFETY: Safe to call with NULL
 */
void hypo_orch_destroy(hypo_orchestrator_t orch);

/**
 * WHAT: Reset orchestrator to initial state
 * WHY: Clear all state without destroying orchestrator
 * HOW: Clear drives, reset statistics, maintain registrations
 *
 * @param orch Orchestrator to reset
 * @return 0 on success, -1 on failure
 */
int hypo_orch_reset(hypo_orchestrator_t orch);

/* ============================================================================
 * BRIDGE REGISTRATION
 * ============================================================================ */

/**
 * WHAT: Register a hypothalamus bridge with the orchestrator
 * WHY: Enable bridge to publish events and receive subscriptions
 * HOW: Add bridge to registry with metadata
 *
 * @param orch Hypothalamus orchestrator
 * @param bridge_type Type of bridge being registered
 * @param name Human-readable bridge name (max 63 chars)
 * @param bridge_handle Bridge handle pointer
 * @param context Bridge-provided context (can be NULL)
 * @param bridge_id_out Output: assigned bridge ID
 * @return 0 on success, -1 on failure
 */
int hypo_orch_register_bridge(
    hypo_orchestrator_t orch,
    hypo_bridge_type_t bridge_type,
    const char* name,
    void* bridge_handle,
    void* context,
    uint32_t* bridge_id_out
);

/**
 * WHAT: Unregister a hypothalamus bridge from the orchestrator
 * WHY: Remove bridge when shutting down
 * HOW: Remove from registry, cancel subscriptions
 *
 * @param orch Hypothalamus orchestrator
 * @param bridge_id Bridge ID to unregister
 * @return 0 on success, -1 on failure
 */
int hypo_orch_unregister_bridge(
    hypo_orchestrator_t orch,
    uint32_t bridge_id
);

/**
 * WHAT: Get information about a registered bridge
 * WHY: Query bridge metadata and status
 * HOW: Lookup bridge in registry, copy info
 *
 * @param orch Hypothalamus orchestrator
 * @param bridge_id Bridge ID to query
 * @param info Output: bridge information
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_bridge_info(
    hypo_orchestrator_t orch,
    uint32_t bridge_id,
    hypo_bridge_info_t* info
);

/**
 * WHAT: Get bridge by type
 * WHY: Lookup bridge handle for direct communication
 * HOW: Search registry by type
 *
 * @param orch Hypothalamus orchestrator
 * @param bridge_type Type of bridge to find
 * @param bridge_handle_out Output: bridge handle
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_bridge_by_type(
    hypo_orchestrator_t orch,
    hypo_bridge_type_t bridge_type,
    void** bridge_handle_out
);

/* ============================================================================
 * EVENT SUBSCRIPTION
 * ============================================================================ */

/**
 * WHAT: Subscribe to hypothalamus events of a specific type
 * WHY: Enable bridges to receive drive notifications
 * HOW: Register callback for event type
 *
 * @param orch Hypothalamus orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param event_type Type of events to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context passed to callback
 * @return 0 on success, -1 on failure
 */
int hypo_orch_subscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_t event_type,
    hypo_event_callback_t callback,
    void* user_data
);

/**
 * WHAT: Subscribe to all events from a specific bridge type
 * WHY: Monitor all activity from a bridge
 * HOW: Register callback for all events from bridge
 *
 * @param orch Hypothalamus orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param source_type Bridge type to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context
 * @return 0 on success, -1 on failure
 */
int hypo_orch_subscribe_to_bridge(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_bridge_type_t source_type,
    hypo_event_callback_t callback,
    void* user_data
);

/**
 * WHAT: Unsubscribe from hypothalamus events
 * WHY: Stop receiving notifications for events
 * HOW: Remove callback registration
 *
 * @param orch Hypothalamus orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param event_type Type of events to unsubscribe from
 * @return 0 on success, -1 on failure
 */
int hypo_orch_unsubscribe(
    hypo_orchestrator_t orch,
    uint32_t subscriber_id,
    hypo_event_type_t event_type
);

/* ============================================================================
 * EVENT PUBLISHING
 * ============================================================================ */

/**
 * WHAT: Publish hypothalamus event synchronously
 * WHY: Notify all subscribers of an event immediately
 * HOW: Iterate through subscribers, invoke callbacks
 *
 * @param orch Hypothalamus orchestrator
 * @param publisher_id Bridge ID of publisher
 * @param event Event data to publish
 * @return 0 on success, -1 on failure
 *
 * BLOCKING: Yes - waits for all callbacks to complete
 */
int hypo_orch_publish(
    hypo_orchestrator_t orch,
    uint32_t publisher_id,
    const hypo_event_data_t* event
);

/**
 * WHAT: Publish hypothalamus event asynchronously
 * WHY: Non-blocking event delivery for time-critical paths
 * HOW: Queue event for background delivery
 *
 * @param orch Hypothalamus orchestrator
 * @param publisher_id Bridge ID of publisher
 * @param event Event data to publish (deep copied)
 * @return 0 on success, -1 on failure
 *
 * NON-BLOCKING: Returns immediately after queueing
 */
int hypo_orch_publish_async(
    hypo_orchestrator_t orch,
    uint32_t publisher_id,
    const hypo_event_data_t* event
);

/**
 * WHAT: Report a drive activation to the orchestrator
 * WHY: Simplified drive reporting without full event creation
 * HOW: Create drive event and publish
 *
 * @param orch Hypothalamus orchestrator
 * @param bridge_id Reporting bridge ID
 * @param drive_type Type of drive
 * @param drive_level Drive level [0, 1]
 * @param urgency Drive urgency
 * @param description Human-readable description
 * @return 0 on success, -1 on failure
 */
int hypo_orch_report_drive(
    hypo_orchestrator_t orch,
    uint32_t bridge_id,
    uint32_t drive_type,
    float drive_level,
    hypo_urgency_t urgency,
    const char* description
);

/* ============================================================================
 * DRIVE STATE ASSESSMENT
 * ============================================================================ */

/**
 * WHAT: Get unified drive state from all bridges
 * WHY: Aggregate drive information for decision making
 * HOW: Combine bridge drive levels with weighted average
 *
 * @param orch Hypothalamus orchestrator
 * @param state Output: unified drive state
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_drive_state(
    hypo_orchestrator_t orch,
    hypo_unified_drive_state_t* state
);

/**
 * WHAT: Get current unified drive level
 * WHY: Quick drive level check without full assessment
 * HOW: Return cached unified drive level
 *
 * @param orch Hypothalamus orchestrator
 * @param drive_level Output: unified drive level [0, 1]
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_drive_level(
    hypo_orchestrator_t orch,
    float* drive_level
);

/**
 * WHAT: Update drive level decay
 * WHY: Allow drives to naturally decrease over time
 * HOW: Apply exponential decay to drive levels
 *
 * @param orch Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 *
 * NOTE: Called automatically if async is enabled
 */
int hypo_orch_update_drive_decay(hypo_orchestrator_t orch);

/**
 * WHAT: Clear all active drives
 * WHY: Manual drive clearing after satisfaction
 * HOW: Reset all bridge drive levels to baseline
 *
 * @param orch Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 */
int hypo_orch_clear_drives(hypo_orchestrator_t orch);

/* ============================================================================
 * RESPONSE COORDINATION
 * ============================================================================ */

/**
 * WHAT: Trigger stress response
 * WHY: Respond to urgent drives by initiating stress mode
 * HOW: Notify all bridges to enter stress response
 *
 * @param orch Hypothalamus orchestrator
 * @param reason Reason for stress response
 * @return 0 on success, -1 on failure
 */
int hypo_orch_trigger_stress(
    hypo_orchestrator_t orch,
    const char* reason
);

/**
 * WHAT: Release stress response
 * WHY: Return to normal operation after stress cleared
 * HOW: Notify all bridges to exit stress mode
 *
 * @param orch Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 */
int hypo_orch_release_stress(hypo_orchestrator_t orch);

/**
 * WHAT: Check if system is in stress response
 * WHY: Query stress state for decision making
 * HOW: Return current stress status
 *
 * @param orch Hypothalamus orchestrator
 * @param in_stress Output: true if in stress response
 * @return 0 on success, -1 on failure
 */
int hypo_orch_is_stressed(
    hypo_orchestrator_t orch,
    bool* in_stress
);

/**
 * WHAT: Broadcast response action to all bridges
 * WHY: Coordinate response across all bridges
 * HOW: Send response event to all registered bridges
 *
 * @param orch Hypothalamus orchestrator
 * @param response_type Type of response action
 * @param data Response-specific data
 * @return 0 on success, -1 on failure
 */
int hypo_orch_broadcast_response(
    hypo_orchestrator_t orch,
    hypo_event_type_t response_type,
    const hypo_event_data_t* data
);

/* ============================================================================
 * STATE AND STATISTICS
 * ============================================================================ */

/**
 * WHAT: Get orchestrator state
 * WHY: Query current operational state
 * HOW: Return current state enum
 *
 * @param orch Hypothalamus orchestrator
 * @param state Output: current state
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_state(
    hypo_orchestrator_t orch,
    hypo_orch_state_t* state
);

/**
 * WHAT: Get orchestrator statistics
 * WHY: Monitor orchestrator performance
 * HOW: Copy accumulated statistics
 *
 * @param orch Hypothalamus orchestrator
 * @param stats Output: statistics
 * @return 0 on success, -1 on failure
 */
int hypo_orch_get_stats(
    hypo_orchestrator_t orch,
    hypo_orch_stats_t* stats
);

/**
 * WHAT: Reset orchestrator statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all statistic counters
 *
 * @param orch Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 */
int hypo_orch_reset_stats(hypo_orchestrator_t orch);

/* ============================================================================
 * INTEGRATION CONNECTIONS
 * ============================================================================ */

/**
 * WHAT: Connect orchestrator to bio-async router
 * WHY: Enable event-driven async integration
 * HOW: Register as bio-async module
 *
 * @param orch Hypothalamus orchestrator
 * @param router Bio-async router handle (NULL to use global)
 * @return 0 on success, -1 on failure
 */
int hypo_orch_connect_bio_async(
    hypo_orchestrator_t orch,
    void* router
);

/**
 * WHAT: Disconnect orchestrator from bio-async router
 * WHY: Cleanup bio-async integration
 * HOW: Unregister from bio-async
 *
 * @param orch Hypothalamus orchestrator
 * @return 0 on success, -1 on failure
 */
int hypo_orch_disconnect_bio_async(hypo_orchestrator_t orch);

/**
 * WHAT: Connect orchestrator to immune system
 * WHY: Enable neuroimmune coordination
 * HOW: Register with immune system for bidirectional events
 *
 * @param orch Hypothalamus orchestrator
 * @param immune_system Immune system handle
 * @return 0 on success, -1 on failure
 */
int hypo_orch_connect_immune(
    hypo_orchestrator_t orch,
    void* immune_system
);

/**
 * WHAT: Connect orchestrator to logging system
 * WHY: Enable drive event logging
 * HOW: Register with logging for event capture
 *
 * @param orch Hypothalamus orchestrator
 * @param logger Logger handle (NULL to use global)
 * @return 0 on success, -1 on failure
 */
int hypo_orch_connect_logging(
    hypo_orchestrator_t orch,
    void* logger
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for bridge type
 * WHY: Human-readable bridge identification
 * HOW: Lookup table
 *
 * @param type Bridge type
 * @return String name (never NULL)
 */
const char* hypo_bridge_type_name(hypo_bridge_type_t type);

/**
 * WHAT: Get string name for event type
 * WHY: Human-readable event identification
 * HOW: Lookup table
 *
 * @param type Event type
 * @return String name (never NULL)
 */
const char* hypo_event_type_name(hypo_event_type_t type);

/**
 * WHAT: Get string name for urgency level
 * WHY: Human-readable urgency identification
 * HOW: Lookup table
 *
 * @param urgency Urgency level
 * @return String name (never NULL)
 */
const char* hypo_urgency_name(hypo_urgency_t urgency);

/**
 * WHAT: Get string name for orchestrator state
 * WHY: Human-readable state identification
 * HOW: Lookup table
 *
 * @param state Orchestrator state
 * @return String name (never NULL)
 */
const char* hypo_orch_state_name(hypo_orch_state_t state);

/**
 * WHAT: Print orchestrator summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print orchestrator state
 *
 * @param orch Hypothalamus orchestrator (NULL safe)
 */
void hypo_orch_print_summary(hypo_orchestrator_t orch);

/**
 * WHAT: Print orchestrator statistics to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print statistics
 *
 * @param stats Statistics to print (NULL safe)
 */
void hypo_orch_print_stats(const hypo_orch_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HYPOTHALAMUS_ORCHESTRATOR_H */
