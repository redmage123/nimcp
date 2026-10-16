/**
 * @file nimcp_security_orchestrator.h
 * @brief Central orchestrator for security module integration and threat coordination
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Central orchestrator that coordinates all security bridges, enabling
 *       event-driven threat detection and unified security response.
 *
 * WHY: Security bridges need to communicate without tight coupling. The orchestrator
 *      provides threat event routing, unified threat assessment, and coordinated response.
 *
 * HOW: Bridges register with the orchestrator, subscribe to threat events, and publish
 *      security events. The orchestrator aggregates threats and coordinates responses.
 *
 * DESIGN PATTERNS:
 * - Mediator: Orchestrator mediates all inter-bridge communication
 * - Observer: Publish-subscribe threat event system
 * - Registry: Bridge registration and lookup
 * - Strategy: Pluggable threat aggregation strategies
 * - Chain of Responsibility: Cascading threat response
 *
 * THREAD SAFETY: All functions are thread-safe. The orchestrator uses internal
 * synchronization for concurrent access from multiple bridges.
 *
 * PERFORMANCE:
 * - Event publish: O(s) where s = number of subscribers
 * - Bridge lookup: O(1) with hash-based registry
 * - Threat aggregation: O(b) where b = number of bridges
 * - Async publish: O(1) - queued for later processing
 *
 * BRIDGES COORDINATED:
 * - Distributed Training Bridge (Byzantine fault detection)
 * - Knowledge Graph Bridge (Query injection prevention)
 * - Game Theory Bridge (Strategy manipulation detection)
 * - Imagination Bridge (Confabulation detection)
 * - Continual Learning Bridge (Catastrophic forgetting protection)
 * - Epistemic Bridge (Belief integrity verification)
 * - Collective Bridge (Swarm consensus validation)
 * - Hippocampus Bridge (Memory consolidation security)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_ORCHESTRATOR_H
#define NIMCP_SECURITY_ORCHESTRATOR_H

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
 * WHAT: Opaque handle to security orchestrator
 * WHY: Encapsulation - hide internal implementation details
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct security_orchestrator_struct* security_orchestrator_t;

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Maximum number of bridges that can register */
#define SEC_ORCH_MAX_BRIDGES 16

/** Maximum number of event subscriptions */
#define SEC_ORCH_MAX_SUBSCRIPTIONS 128

/** Maximum async event queue size */
#define SEC_ORCH_MAX_EVENT_QUEUE 512

/** Maximum threat history entries */
#define SEC_ORCH_MAX_THREAT_HISTORY 256

/** Maximum bridge name length */
#define SEC_ORCH_MAX_NAME_LEN 64

/** Default threat decay rate (per second) */
#define SEC_ORCH_DEFAULT_THREAT_DECAY 0.1f

/** Default critical threat threshold */
#define SEC_ORCH_DEFAULT_CRITICAL_THRESHOLD 0.9f

/** Default high threat threshold */
#define SEC_ORCH_DEFAULT_HIGH_THRESHOLD 0.7f

/** Default medium threat threshold */
#define SEC_ORCH_DEFAULT_MEDIUM_THRESHOLD 0.4f

/* ============================================================================
 * SECURITY BRIDGE TYPES
 * ============================================================================ */

/**
 * WHAT: Enumeration of security bridge types
 * WHY: Identify and categorize registered bridges
 * HOW: Each bridge type has unique identifier for routing
 */
typedef enum {
    SEC_BRIDGE_UNKNOWN = 0,
    SEC_BRIDGE_DISTRIBUTED_TRAINING,    /**< Byzantine fault detection */
    SEC_BRIDGE_KNOWLEDGE_GRAPH,         /**< Query injection prevention */
    SEC_BRIDGE_GAME_THEORY,             /**< Strategy manipulation detection */
    SEC_BRIDGE_IMAGINATION,             /**< Confabulation detection */
    SEC_BRIDGE_CONTINUAL_LEARNING,      /**< Catastrophic forgetting protection */
    SEC_BRIDGE_EPISTEMIC,               /**< Belief integrity verification */
    SEC_BRIDGE_COLLECTIVE,              /**< Swarm consensus validation */
    SEC_BRIDGE_HIPPOCAMPUS,             /**< Memory consolidation security */
    SEC_BRIDGE_BBB,                     /**< Blood-brain barrier */
    SEC_BRIDGE_ANOMALY_DETECTOR,        /**< Anomaly detection */
    SEC_BRIDGE_PATTERN_DB,              /**< Pattern database */
    SEC_BRIDGE_RATE_LIMITER,            /**< Rate limiting */
    SEC_BRIDGE_IMMUNE,                  /**< Immune system integration */
    SEC_BRIDGE_COUNT                    /**< Total bridge type count */
} security_bridge_type_t;

/* ============================================================================
 * SECURITY EVENT TYPES
 * ============================================================================ */

/**
 * WHAT: Security event types for inter-bridge communication
 * WHY: Categorize events for routing and filtering
 * HOW: Each event type represents a security-relevant occurrence
 */
typedef enum {
    /* Threat Detection Events */
    SEC_EVENT_THREAT_DETECTED = 0,      /**< General threat detected */
    SEC_EVENT_THREAT_ESCALATED,         /**< Threat level increased */
    SEC_EVENT_THREAT_MITIGATED,         /**< Threat successfully handled */
    SEC_EVENT_THREAT_CLEARED,           /**< Threat no longer present */

    /* Attack Events */
    SEC_EVENT_ATTACK_STARTED,           /**< Attack sequence began */
    SEC_EVENT_ATTACK_ONGOING,           /**< Attack in progress */
    SEC_EVENT_ATTACK_BLOCKED,           /**< Attack successfully blocked */
    SEC_EVENT_ATTACK_COMPLETED,         /**< Attack ended (blocked or not) */

    /* Byzantine/Distributed Events */
    SEC_EVENT_BYZANTINE_DETECTED,       /**< Byzantine behavior detected */
    SEC_EVENT_WORKER_QUARANTINED,       /**< Distributed worker quarantined */
    SEC_EVENT_CONSENSUS_VIOLATED,       /**< Consensus mechanism violated */
    SEC_EVENT_GRADIENT_POISONED,        /**< Gradient poisoning detected */

    /* Knowledge/Epistemic Events */
    SEC_EVENT_INJECTION_ATTEMPT,        /**< Query/data injection attempted */
    SEC_EVENT_SCHEMA_VIOLATION,         /**< Schema integrity violated */
    SEC_EVENT_BELIEF_CORRUPTED,         /**< Belief system corrupted */
    SEC_EVENT_EVIDENCE_TAMPERED,        /**< Evidence chain tampered */

    /* Cognitive Security Events */
    SEC_EVENT_CONFABULATION_DETECTED,   /**< False memory/imagination detected */
    SEC_EVENT_REALITY_DRIFT,            /**< Reality grounding lost */
    SEC_EVENT_STRATEGY_MANIPULATION,    /**< Game theory manipulation */
    SEC_EVENT_COALITION_ATTACK,         /**< Malicious coalition detected */

    /* Memory Security Events */
    SEC_EVENT_MEMORY_CORRUPTION,        /**< Memory integrity violated */
    SEC_EVENT_FORGETTING_ATTACK,        /**< Catastrophic forgetting induced */
    SEC_EVENT_REPLAY_POISONED,          /**< Replay buffer poisoned */
    SEC_EVENT_CONSOLIDATION_TAMPERED,   /**< Memory consolidation tampered */

    /* System Events */
    SEC_EVENT_BRIDGE_REGISTERED,        /**< New bridge registered */
    SEC_EVENT_BRIDGE_UNREGISTERED,      /**< Bridge unregistered */
    SEC_EVENT_BRIDGE_CONNECTED,         /**< Bridge connected to target */
    SEC_EVENT_BRIDGE_DISCONNECTED,      /**< Bridge disconnected */
    SEC_EVENT_ORCHESTRATOR_STATE,       /**< Orchestrator state change */

    SEC_EVENT_TYPE_COUNT                /**< Total event type count */
} security_event_type_t;

/**
 * WHAT: Threat severity levels
 * WHY: Categorize threat urgency for response prioritization
 * HOW: Numerical levels mapped to response actions
 */
typedef enum {
    SEC_SEVERITY_NONE = 0,              /**< No threat */
    SEC_SEVERITY_LOW,                   /**< Low severity - monitor */
    SEC_SEVERITY_MEDIUM,                /**< Medium severity - alert */
    SEC_SEVERITY_HIGH,                  /**< High severity - respond */
    SEC_SEVERITY_CRITICAL               /**< Critical - immediate action */
} security_severity_t;

/**
 * WHAT: Orchestrator operational states
 * WHY: Track orchestrator lifecycle and mode
 * HOW: State machine for orchestrator behavior
 */
typedef enum {
    SEC_ORCH_STATE_UNINITIALIZED = 0,   /**< Not yet initialized */
    SEC_ORCH_STATE_IDLE,                /**< Initialized, no active threats */
    SEC_ORCH_STATE_MONITORING,          /**< Actively monitoring */
    SEC_ORCH_STATE_ALERT,               /**< Elevated threat detected */
    SEC_ORCH_STATE_RESPONDING,          /**< Actively responding to threat */
    SEC_ORCH_STATE_LOCKDOWN,            /**< System in lockdown mode */
    SEC_ORCH_STATE_RECOVERY,            /**< Recovering from incident */
    SEC_ORCH_STATE_ERROR                /**< Error state */
} security_orch_state_t;

/* ============================================================================
 * EVENT DATA STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Security event data payload
 * WHY: Carry event-specific information
 * HOW: Union with type-specific fields
 */
typedef struct {
    security_event_type_t event_type;   /**< Type of event */
    security_bridge_type_t source;      /**< Bridge that generated event */
    security_severity_t severity;       /**< Event severity level */
    uint64_t timestamp;                 /**< Event timestamp (microseconds) */
    uint32_t event_id;                  /**< Unique event identifier */

    union {
        /** Threat event data */
        struct {
            float threat_level;         /**< Threat level [0, 1] */
            uint32_t threat_id;         /**< Threat identifier */
            uint32_t affected_module;   /**< Affected module ID */
            char description[128];      /**< Threat description */
        } threat;

        /** Attack event data */
        struct {
            uint32_t attack_type;       /**< Type of attack */
            uint32_t attacker_id;       /**< Attacker identifier (if known) */
            uint32_t target_id;         /**< Target module/component */
            float confidence;           /**< Detection confidence */
        } attack;

        /** Byzantine event data */
        struct {
            uint32_t worker_id;         /**< Worker identifier */
            float anomaly_score;        /**< Anomaly score */
            uint32_t detection_count;   /**< Number of detections */
            bool is_quarantined;        /**< Whether worker is quarantined */
        } byzantine;

        /** Memory event data */
        struct {
            uint32_t memory_region;     /**< Affected memory region */
            uint64_t memory_address;    /**< Memory address (if applicable) */
            float integrity_score;      /**< Memory integrity score */
            bool is_recoverable;        /**< Whether corruption is recoverable */
        } memory;

        /** Bridge lifecycle event data */
        struct {
            security_bridge_type_t bridge_type;  /**< Bridge type */
            uint32_t bridge_id;                  /**< Bridge instance ID */
            char bridge_name[64];                /**< Bridge name */
        } bridge;

        /** Generic data for custom events */
        struct {
            uint32_t code;              /**< Custom event code */
            uint32_t flags;             /**< Custom flags */
            void* data;                 /**< Custom data pointer */
            size_t data_size;           /**< Size of custom data */
        } custom;
    };
} security_event_data_t;

/**
 * WHAT: Callback function type for security events
 * WHY: Allow bridges to receive event notifications
 * HOW: Function pointer with event data and user context
 *
 * @param event Event data
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
typedef int (*security_event_callback_t)(
    const security_event_data_t* event,
    void* user_data
);

/* ============================================================================
 * CONFIGURATION STRUCTURES
 * ============================================================================ */

/**
 * WHAT: Configuration for security orchestrator
 * WHY: Customize orchestrator behavior at creation time
 * HOW: Struct with all orchestrator configuration parameters
 */
typedef struct {
    uint32_t max_bridges;               /**< Maximum registered bridges */
    uint32_t max_subscriptions;         /**< Maximum total subscriptions */
    bool enable_async;                  /**< Enable async event delivery */
    uint32_t event_queue_size;          /**< Async event queue size */

    /* Threat thresholds */
    float critical_threshold;           /**< Critical threat level threshold */
    float high_threshold;               /**< High threat level threshold */
    float medium_threshold;             /**< Medium threat level threshold */
    float threat_decay_rate;            /**< Threat level decay per second */

    /* Response configuration */
    bool auto_lockdown;                 /**< Auto-lockdown on critical threats */
    bool enable_cascade;                /**< Enable cascading threat propagation */
    bool enable_recovery;               /**< Enable auto-recovery after threats */
    uint32_t lockdown_timeout_ms;       /**< Lockdown timeout (0 = manual) */

    /* Integration options */
    bool connect_immune;                /**< Auto-connect to immune system */
    bool connect_cognitive_hub;         /**< Auto-connect to cognitive hub */
    bool enable_audit;                  /**< Enable security audit logging */
} security_orch_config_t;

/**
 * WHAT: Information about a registered security bridge
 * WHY: Track bridge metadata for routing and coordination
 * HOW: Struct with identification and state
 */
typedef struct {
    uint32_t bridge_id;                 /**< Unique bridge instance ID */
    security_bridge_type_t type;        /**< Bridge type */
    char name[SEC_ORCH_MAX_NAME_LEN];   /**< Human-readable name */
    void* bridge_handle;                /**< Bridge handle pointer */
    void* context;                      /**< Bridge-provided context */
    bool is_active;                     /**< Whether bridge is active */
    bool is_connected;                  /**< Whether bridge is connected */
    float current_threat_level;         /**< Bridge's current threat level */
    uint64_t last_event_time;           /**< Last event timestamp */
    uint32_t events_published;          /**< Events published by bridge */
    uint32_t threats_detected;          /**< Threats detected by bridge */
} security_bridge_info_t;

/**
 * WHAT: Unified threat assessment from all bridges
 * WHY: Aggregate threat information for coordinated response
 * HOW: Weighted combination of bridge threat levels
 */
typedef struct {
    float unified_threat_level;         /**< Combined threat level [0, 1] */
    security_severity_t severity;       /**< Overall severity assessment */
    uint32_t active_threats;            /**< Number of active threats */
    uint32_t bridges_reporting;         /**< Number of bridges reporting threats */

    /* Per-bridge threat breakdown */
    struct {
        security_bridge_type_t type;    /**< Bridge type */
        float threat_level;             /**< Bridge threat level */
        uint32_t threat_count;          /**< Number of threats */
        bool is_primary_source;         /**< Is primary threat source */
    } bridge_threats[SEC_BRIDGE_COUNT];

    /* Highest threats */
    security_event_type_t primary_threat_type;   /**< Primary threat type */
    security_bridge_type_t primary_threat_source; /**< Primary threat source */
    char threat_summary[256];           /**< Human-readable threat summary */

    uint64_t assessment_time;           /**< When assessment was made */
    uint64_t next_decay_time;           /**< When next decay occurs */
} security_threat_assessment_t;

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

    /* Threat statistics */
    uint64_t threats_detected;          /**< Total threats detected */
    uint64_t threats_mitigated;         /**< Threats successfully mitigated */
    uint64_t attacks_blocked;           /**< Attacks successfully blocked */
    uint32_t lockdowns_triggered;       /**< Number of lockdowns triggered */
    float avg_threat_level;             /**< Average threat level */
    float peak_threat_level;            /**< Peak threat level seen */

    /* Timing statistics */
    uint64_t avg_event_latency_us;      /**< Average event delivery latency */
    uint64_t avg_response_time_us;      /**< Average threat response time */
    uint64_t uptime_us;                 /**< Orchestrator uptime */
} security_orch_stats_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default orchestrator configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - max_bridges: 16
 * - max_subscriptions: 128
 * - enable_async: true
 * - event_queue_size: 512
 * - critical_threshold: 0.9
 * - high_threshold: 0.7
 * - medium_threshold: 0.4
 * - threat_decay_rate: 0.1
 * - auto_lockdown: true
 * - enable_cascade: true
 * - enable_recovery: true
 * - lockdown_timeout_ms: 30000
 *
 * @param config Output: configuration structure
 * @return 0 on success, error code on failure
 */
int security_orch_default_config(security_orch_config_t* config);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new security orchestrator
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
 * MEMORY: Caller must call security_orch_destroy() when done
 */
security_orchestrator_t security_orch_create(const security_orch_config_t* config);

/**
 * WHAT: Destroy security orchestrator
 * WHY: Release all resources and cleanup
 * HOW: Stop async thread, unregister bridges, free memory
 *
 * @param orch Orchestrator to destroy
 *
 * BLOCKING: May block waiting for async queue to drain
 * SAFETY: Safe to call with NULL
 */
void security_orch_destroy(security_orchestrator_t orch);

/**
 * WHAT: Reset orchestrator to initial state
 * WHY: Clear all state without destroying orchestrator
 * HOW: Clear threats, reset statistics, maintain registrations
 *
 * @param orch Orchestrator to reset
 * @return 0 on success, error code on failure
 */
int security_orch_reset(security_orchestrator_t orch);

/* ============================================================================
 * BRIDGE REGISTRATION
 * ============================================================================ */

/**
 * WHAT: Register a security bridge with the orchestrator
 * WHY: Enable bridge to publish events and receive subscriptions
 * HOW: Add bridge to registry with metadata
 *
 * @param orch Security orchestrator
 * @param bridge_type Type of bridge being registered
 * @param name Human-readable bridge name (max 63 chars)
 * @param bridge_handle Bridge handle pointer
 * @param context Bridge-provided context (can be NULL)
 * @param bridge_id_out Output: assigned bridge ID
 * @return 0 on success, error code on failure
 *
 * ERRORS:
 * - NIMCP_ERROR_NULL_POINTER if orch is NULL
 * - NIMCP_ERROR_INVALID_PARAM if bridge_type already registered
 * - NIMCP_ERROR_OUT_OF_RANGE if max_bridges limit reached
 */
int security_orch_register_bridge(
    security_orchestrator_t orch,
    security_bridge_type_t bridge_type,
    const char* name,
    void* bridge_handle,
    void* context,
    uint32_t* bridge_id_out
);

/**
 * WHAT: Unregister a security bridge from the orchestrator
 * WHY: Remove bridge when shutting down
 * HOW: Remove from registry, cancel subscriptions
 *
 * @param orch Security orchestrator
 * @param bridge_id Bridge ID to unregister
 * @return 0 on success, error code on failure
 */
int security_orch_unregister_bridge(
    security_orchestrator_t orch,
    uint32_t bridge_id
);

/**
 * WHAT: Get information about a registered bridge
 * WHY: Query bridge metadata and status
 * HOW: Lookup bridge in registry, copy info
 *
 * @param orch Security orchestrator
 * @param bridge_id Bridge ID to query
 * @param info Output: bridge information
 * @return 0 on success, error code on failure
 */
int security_orch_get_bridge_info(
    security_orchestrator_t orch,
    uint32_t bridge_id,
    security_bridge_info_t* info
);

/**
 * WHAT: Get bridge by type
 * WHY: Lookup bridge handle for direct communication
 * HOW: Search registry by type
 *
 * @param orch Security orchestrator
 * @param bridge_type Type of bridge to find
 * @param bridge_handle_out Output: bridge handle
 * @return 0 on success, error code on failure
 */
int security_orch_get_bridge_by_type(
    security_orchestrator_t orch,
    security_bridge_type_t bridge_type,
    void** bridge_handle_out
);

/* ============================================================================
 * EVENT SUBSCRIPTION
 * ============================================================================ */

/**
 * WHAT: Subscribe to security events of a specific type
 * WHY: Enable bridges to receive threat notifications
 * HOW: Register callback for event type
 *
 * @param orch Security orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param event_type Type of events to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context passed to callback
 * @return 0 on success, error code on failure
 */
int security_orch_subscribe(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_event_type_t event_type,
    security_event_callback_t callback,
    void* user_data
);

/**
 * WHAT: Subscribe to all events from a specific bridge type
 * WHY: Monitor all activity from a bridge
 * HOW: Register callback for all events from bridge
 *
 * @param orch Security orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param source_type Bridge type to subscribe to
 * @param callback Function to call when event occurs
 * @param user_data User-provided context
 * @return 0 on success, error code on failure
 */
int security_orch_subscribe_to_bridge(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_bridge_type_t source_type,
    security_event_callback_t callback,
    void* user_data
);

/**
 * WHAT: Unsubscribe from security events
 * WHY: Stop receiving notifications for events
 * HOW: Remove callback registration
 *
 * @param orch Security orchestrator
 * @param subscriber_id Bridge ID of subscriber
 * @param event_type Type of events to unsubscribe from
 * @return 0 on success, error code on failure
 */
int security_orch_unsubscribe(
    security_orchestrator_t orch,
    uint32_t subscriber_id,
    security_event_type_t event_type
);

/* ============================================================================
 * EVENT PUBLISHING
 * ============================================================================ */

/**
 * WHAT: Publish security event synchronously
 * WHY: Notify all subscribers of an event immediately
 * HOW: Iterate through subscribers, invoke callbacks
 *
 * @param orch Security orchestrator
 * @param publisher_id Bridge ID of publisher
 * @param event Event data to publish
 * @return 0 on success, error code on failure
 *
 * BLOCKING: Yes - waits for all callbacks to complete
 */
int security_orch_publish(
    security_orchestrator_t orch,
    uint32_t publisher_id,
    const security_event_data_t* event
);

/**
 * WHAT: Publish security event asynchronously
 * WHY: Non-blocking event delivery for time-critical paths
 * HOW: Queue event for background delivery
 *
 * @param orch Security orchestrator
 * @param publisher_id Bridge ID of publisher
 * @param event Event data to publish (deep copied)
 * @return 0 on success, error code on failure
 *
 * NON-BLOCKING: Returns immediately after queueing
 */
int security_orch_publish_async(
    security_orchestrator_t orch,
    uint32_t publisher_id,
    const security_event_data_t* event
);

/**
 * WHAT: Report a threat to the orchestrator
 * WHY: Simplified threat reporting without full event creation
 * HOW: Create threat event and publish
 *
 * @param orch Security orchestrator
 * @param bridge_id Reporting bridge ID
 * @param threat_level Threat level [0, 1]
 * @param severity Threat severity
 * @param description Human-readable description
 * @return 0 on success, error code on failure
 */
int security_orch_report_threat(
    security_orchestrator_t orch,
    uint32_t bridge_id,
    float threat_level,
    security_severity_t severity,
    const char* description
);

/* ============================================================================
 * THREAT ASSESSMENT
 * ============================================================================ */

/**
 * WHAT: Get unified threat assessment from all bridges
 * WHY: Aggregate threat information for decision making
 * HOW: Combine bridge threat levels with weighted average
 *
 * @param orch Security orchestrator
 * @param assessment Output: unified threat assessment
 * @return 0 on success, error code on failure
 */
int security_orch_get_threat_assessment(
    security_orchestrator_t orch,
    security_threat_assessment_t* assessment
);

/**
 * WHAT: Get current unified threat level
 * WHY: Quick threat level check without full assessment
 * HOW: Return cached unified threat level
 *
 * @param orch Security orchestrator
 * @param threat_level Output: unified threat level [0, 1]
 * @return 0 on success, error code on failure
 */
int security_orch_get_threat_level(
    security_orchestrator_t orch,
    float* threat_level
);

/**
 * WHAT: Update threat level decay
 * WHY: Allow threats to naturally decrease over time
 * HOW: Apply exponential decay to threat levels
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 *
 * NOTE: Called automatically if async is enabled
 */
int security_orch_update_threat_decay(security_orchestrator_t orch);

/**
 * WHAT: Clear all active threats
 * WHY: Manual threat clearing after incident resolution
 * HOW: Reset all bridge threat levels to zero
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 */
int security_orch_clear_threats(security_orchestrator_t orch);

/* ============================================================================
 * RESPONSE COORDINATION
 * ============================================================================ */

/**
 * WHAT: Trigger system lockdown
 * WHY: Respond to critical threats by restricting operations
 * HOW: Notify all bridges to enter lockdown mode
 *
 * @param orch Security orchestrator
 * @param reason Reason for lockdown
 * @return 0 on success, error code on failure
 */
int security_orch_trigger_lockdown(
    security_orchestrator_t orch,
    const char* reason
);

/**
 * WHAT: Release system lockdown
 * WHY: Return to normal operation after threat cleared
 * HOW: Notify all bridges to exit lockdown mode
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 */
int security_orch_release_lockdown(security_orchestrator_t orch);

/**
 * WHAT: Check if system is in lockdown
 * WHY: Query lockdown state for decision making
 * HOW: Return current lockdown status
 *
 * @param orch Security orchestrator
 * @param is_locked Output: true if in lockdown
 * @return 0 on success, error code on failure
 */
int security_orch_is_locked_down(
    security_orchestrator_t orch,
    bool* is_locked
);

/**
 * WHAT: Broadcast response action to all bridges
 * WHY: Coordinate response across all bridges
 * HOW: Send response event to all registered bridges
 *
 * @param orch Security orchestrator
 * @param response_type Type of response action
 * @param data Response-specific data
 * @return 0 on success, error code on failure
 */
int security_orch_broadcast_response(
    security_orchestrator_t orch,
    security_event_type_t response_type,
    const security_event_data_t* data
);

/* ============================================================================
 * STATE AND STATISTICS
 * ============================================================================ */

/**
 * WHAT: Get orchestrator state
 * WHY: Query current operational state
 * HOW: Return current state enum
 *
 * @param orch Security orchestrator
 * @param state Output: current state
 * @return 0 on success, error code on failure
 */
int security_orch_get_state(
    security_orchestrator_t orch,
    security_orch_state_t* state
);

/**
 * WHAT: Get orchestrator statistics
 * WHY: Monitor orchestrator performance
 * HOW: Copy accumulated statistics
 *
 * @param orch Security orchestrator
 * @param stats Output: statistics
 * @return 0 on success, error code on failure
 */
int security_orch_get_stats(
    security_orchestrator_t orch,
    security_orch_stats_t* stats
);

/**
 * WHAT: Reset orchestrator statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all statistic counters
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 */
int security_orch_reset_stats(security_orchestrator_t orch);

/* ============================================================================
 * INTEGRATION CONNECTIONS
 * ============================================================================ */

/**
 * WHAT: Connect orchestrator to immune system
 * WHY: Enable immune-security coordination
 * HOW: Register with immune system for bidirectional events
 *
 * @param orch Security orchestrator
 * @param immune_system Immune system handle
 * @return 0 on success, error code on failure
 */
int security_orch_connect_immune(
    security_orchestrator_t orch,
    void* immune_system
);

/**
 * WHAT: Connect orchestrator to cognitive hub
 * WHY: Enable cognitive-security coordination
 * HOW: Register with cognitive hub for bidirectional events
 *
 * @param orch Security orchestrator
 * @param cognitive_hub Cognitive hub handle
 * @return 0 on success, error code on failure
 */
int security_orch_connect_cognitive_hub(
    security_orchestrator_t orch,
    void* cognitive_hub
);

/**
 * WHAT: Connect orchestrator to bio-async router
 * WHY: Enable event-driven async integration
 * HOW: Register as bio-async module
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 */
int security_orch_connect_bio_async(security_orchestrator_t orch);

/**
 * WHAT: Disconnect orchestrator from bio-async router
 * WHY: Cleanup bio-async integration
 * HOW: Unregister from bio-async
 *
 * @param orch Security orchestrator
 * @return 0 on success, error code on failure
 */
int security_orch_disconnect_bio_async(security_orchestrator_t orch);

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
const char* security_bridge_type_name(security_bridge_type_t type);

/**
 * WHAT: Get string name for event type
 * WHY: Human-readable event identification
 * HOW: Lookup table
 *
 * @param type Event type
 * @return String name (never NULL)
 */
const char* security_event_type_name(security_event_type_t type);

/**
 * WHAT: Get string name for severity level
 * WHY: Human-readable severity identification
 * HOW: Lookup table
 *
 * @param severity Severity level
 * @return String name (never NULL)
 */
const char* security_severity_name(security_severity_t severity);

/**
 * WHAT: Get string name for orchestrator state
 * WHY: Human-readable state identification
 * HOW: Lookup table
 *
 * @param state Orchestrator state
 * @return String name (never NULL)
 */
const char* security_orch_state_name(security_orch_state_t state);

/**
 * WHAT: Print orchestrator summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print orchestrator state
 *
 * @param orch Security orchestrator (NULL safe)
 */
void security_orch_print_summary(security_orchestrator_t orch);

/**
 * WHAT: Print orchestrator statistics to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print statistics
 *
 * @param stats Statistics to print (NULL safe)
 */
void security_orch_print_stats(const security_orch_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_ORCHESTRATOR_H */
