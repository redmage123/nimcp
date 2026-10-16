/**
 * @file nimcp_security_cognitive_hub_bridge.h
 * @brief Bridge between Security Orchestrator and Cognitive Integration Hub
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional bridge connecting security orchestrator to cognitive hub,
 *       enabling coordinated security-cognitive responses.
 *
 * WHY: Security threats can affect cognitive processing (e.g., memory corruption,
 *      reasoning manipulation). Cognitive anomalies can indicate security issues.
 *      This bridge enables unified threat response across both domains.
 *
 * HOW: Registers security as a cognitive module, translates events between
 *      security and cognitive domains, provides query handlers for cross-domain
 *      information requests.
 *
 * BIDIRECTIONAL EFFECTS:
 *
 * Security → Cognitive:
 * - Threat alerts trigger attention shifts
 * - Lockdown restricts cognitive processing
 * - Attack detection triggers protective memory consolidation
 * - Byzantine detection invalidates collaborative decisions
 *
 * Cognitive → Security:
 * - Anomalous reasoning patterns trigger security checks
 * - Memory access anomalies report to anomaly detector
 * - Emotional dysregulation can indicate manipulation
 * - Unusual learning patterns suggest data poisoning
 *
 * DESIGN PATTERNS:
 * - Adapter: Translates between security and cognitive event systems
 * - Mediator: Coordinates cross-domain responses
 * - Observer: Subscribes to events from both domains
 * - Proxy: Represents security in cognitive domain and vice versa
 *
 * THREAD SAFETY: All functions are thread-safe.
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_COGNITIVE_HUB_BRIDGE_H
#define NIMCP_SECURITY_COGNITIVE_HUB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FORWARD DECLARATIONS / INCLUDES
 * ============================================================================ */

/* Include security orchestrator header for type definitions */
#ifndef NIMCP_SECURITY_ORCHESTRATOR_H
#include "security/nimcp_security_orchestrator.h"
#endif

/* Include cognitive hub header for type definitions */
#ifndef NIMCP_COGNITIVE_INTEGRATION_HUB_H
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#endif

/* ============================================================================
 * OPAQUE TYPE DECLARATIONS
 * ============================================================================ */

/**
 * WHAT: Opaque handle to security-cognitive hub bridge
 * WHY: Encapsulation - hide internal implementation
 * HOW: Pimpl idiom
 */
typedef struct security_cognitive_bridge_struct* security_cognitive_bridge_t;

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum queued cross-domain events */
#define SEC_COG_MAX_EVENT_QUEUE 256

/** Maximum registered translation rules */
#define SEC_COG_MAX_TRANSLATIONS 64

/** Security module ID in cognitive hub */
#define SEC_COG_MODULE_ID 0x5EC00001

/** Bridge name in cognitive hub */
#define SEC_COG_MODULE_NAME "security_orchestrator"

/* ============================================================================
 * CROSS-DOMAIN EVENT MAPPING
 * ============================================================================ */

/**
 * WHAT: Mapping from security events to cognitive events
 * WHY: Translate security events into cognitive-meaningful events
 * HOW: Lookup table structure
 */
typedef struct {
    security_event_type_t security_event;   /**< Source security event */
    cognitive_event_type_t cognitive_event; /**< Target cognitive event */
    cognitive_category_t target_category;   /**< Target cognitive category */
    bool is_critical;                       /**< Whether event requires immediate attention */
    const char* description;                /**< Human-readable description */
} security_to_cognitive_mapping_t;

/**
 * WHAT: Mapping from cognitive events to security events
 * WHY: Translate cognitive anomalies into security alerts
 * HOW: Lookup table structure
 */
typedef struct {
    cognitive_event_type_t cognitive_event; /**< Source cognitive event */
    cognitive_category_t source_category;   /**< Source cognitive category */
    security_event_type_t security_event;   /**< Target security event */
    float threat_threshold;                 /**< Threshold for triggering security event */
    const char* description;                /**< Human-readable description */
} cognitive_to_security_mapping_t;

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Bridge configuration
 * WHY: Customize bridge behavior
 * HOW: Configuration struct with tuneable parameters
 */
typedef struct {
    /* Event translation */
    bool translate_security_to_cognitive;   /**< Translate security events to cognitive */
    bool translate_cognitive_to_security;   /**< Translate cognitive events to security */
    bool enable_async_translation;          /**< Use async event queue */

    /* Response coordination */
    bool coordinate_lockdown;               /**< Coordinate lockdown with cognitive hub */
    bool protect_memory_on_attack;          /**< Trigger memory consolidation on attack */
    bool restrict_reasoning_on_threat;      /**< Restrict reasoning under threat */

    /* Query handling */
    bool enable_security_queries;           /**< Allow cognitive modules to query security */
    bool enable_cognitive_queries;          /**< Allow security to query cognitive modules */

    /* Thresholds */
    float attention_shift_threshold;        /**< Threat level to trigger attention shift */
    float reasoning_restrict_threshold;     /**< Threat level to restrict reasoning */
    float lockdown_notify_threshold;        /**< Threat level to notify cognitive modules */

    /* Anomaly detection */
    float cognitive_anomaly_threshold;      /**< Cognitive anomaly score to trigger security */
    bool report_memory_anomalies;           /**< Report memory access anomalies to security */
    bool report_reasoning_anomalies;        /**< Report reasoning anomalies to security */
} security_cognitive_config_t;

/**
 * WHAT: Bridge state enumeration
 * WHY: Track bridge operational state
 * HOW: State machine for bridge lifecycle
 */
typedef enum {
    SEC_COG_STATE_UNINITIALIZED = 0,        /**< Not initialized */
    SEC_COG_STATE_DISCONNECTED,              /**< Initialized but not connected */
    SEC_COG_STATE_CONNECTED,                 /**< Connected to both systems */
    SEC_COG_STATE_ACTIVE,                    /**< Actively translating events */
    SEC_COG_STATE_COORDINATING,              /**< Coordinating cross-domain response */
    SEC_COG_STATE_LOCKDOWN,                  /**< In lockdown coordination mode */
    SEC_COG_STATE_ERROR                      /**< Error state */
} security_cognitive_state_t;

/**
 * WHAT: Bridge statistics
 * WHY: Monitor bridge performance
 * HOW: Counters for various operations
 */
typedef struct {
    /* Event translation */
    uint64_t security_events_translated;    /**< Security events sent to cognitive */
    uint64_t cognitive_events_translated;   /**< Cognitive events sent to security */
    uint64_t events_dropped;                /**< Events dropped (queue full, etc.) */

    /* Query handling */
    uint64_t security_queries_handled;      /**< Security queries from cognitive modules */
    uint64_t cognitive_queries_made;        /**< Queries to cognitive modules */
    uint64_t query_failures;                /**< Failed queries */

    /* Coordination */
    uint32_t lockdowns_coordinated;         /**< Lockdowns coordinated with cognitive */
    uint32_t attention_shifts_triggered;    /**< Attention shifts triggered by security */
    uint32_t memory_protections_triggered;  /**< Memory consolidations triggered */

    /* Timing */
    uint64_t avg_translation_latency_us;    /**< Average event translation latency */
    uint64_t uptime_us;                     /**< Bridge uptime */
} security_cognitive_stats_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default bridge configuration
 * WHY: Provide sensible defaults
 * HOW: Return pre-configured struct
 *
 * @param config Output: configuration structure
 * @return 0 on success, error code on failure
 */
int security_cognitive_default_config(security_cognitive_config_t* config);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create security-cognitive hub bridge
 * WHY: Initialize bridge for cross-domain communication
 * HOW: Allocate resources, initialize state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge handle, or NULL on error
 */
security_cognitive_bridge_t security_cognitive_bridge_create(
    const security_cognitive_config_t* config
);

/**
 * WHAT: Destroy bridge
 * WHY: Release all resources
 * HOW: Disconnect from both systems, free memory
 *
 * @param bridge Bridge to destroy
 */
void security_cognitive_bridge_destroy(security_cognitive_bridge_t bridge);

/**
 * WHAT: Reset bridge to initial state
 * WHY: Clear state without destroying
 * HOW: Reset statistics, clear event queue
 *
 * @param bridge Bridge to reset
 * @return 0 on success, error code on failure
 */
int security_cognitive_bridge_reset(security_cognitive_bridge_t bridge);

/* ============================================================================
 * CONNECTION MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Connect bridge to security orchestrator
 * WHY: Enable security event translation
 * HOW: Register with orchestrator for events
 *
 * @param bridge Bridge handle
 * @param orchestrator Security orchestrator handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_connect_security(
    security_cognitive_bridge_t bridge,
    security_orchestrator_t orchestrator
);

/**
 * WHAT: Connect bridge to cognitive hub
 * WHY: Enable cognitive event translation
 * HOW: Register as cognitive module, subscribe to events
 *
 * @param bridge Bridge handle
 * @param cognitive_hub Cognitive hub handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_connect_cognitive(
    security_cognitive_bridge_t bridge,
    cognitive_integration_hub_t cognitive_hub
);

/**
 * WHAT: Disconnect from security orchestrator
 * WHY: Stop security event translation
 * HOW: Unsubscribe from orchestrator
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_disconnect_security(security_cognitive_bridge_t bridge);

/**
 * WHAT: Disconnect from cognitive hub
 * WHY: Stop cognitive event translation
 * HOW: Unregister from cognitive hub
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_disconnect_cognitive(security_cognitive_bridge_t bridge);

/**
 * WHAT: Check if bridge is fully connected
 * WHY: Verify both connections are active
 * HOW: Check internal connection flags
 *
 * @param bridge Bridge handle
 * @return true if connected to both systems
 */
bool security_cognitive_is_connected(security_cognitive_bridge_t bridge);

/* ============================================================================
 * EVENT TRANSLATION
 * ============================================================================ */

/**
 * WHAT: Translate security event to cognitive event
 * WHY: Propagate security alerts to cognitive processing
 * HOW: Map security event type to cognitive event, publish to hub
 *
 * @param bridge Bridge handle
 * @param security_event Security event to translate
 * @return 0 on success, error code on failure
 */
int security_cognitive_translate_security_event(
    security_cognitive_bridge_t bridge,
    const security_event_data_t* security_event
);

/**
 * WHAT: Translate cognitive event to security event
 * WHY: Report cognitive anomalies to security
 * HOW: Map cognitive event to security event, publish to orchestrator
 *
 * @param bridge Bridge handle
 * @param cognitive_event_type Cognitive event type
 * @param category Source cognitive category
 * @param anomaly_score Anomaly score (0-1)
 * @param description Human-readable description
 * @return 0 on success, error code on failure
 */
int security_cognitive_translate_cognitive_event(
    security_cognitive_bridge_t bridge,
    cognitive_event_type_t cognitive_event_type,
    cognitive_category_t category,
    float anomaly_score,
    const char* description
);

/* ============================================================================
 * COORDINATION FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Coordinate lockdown with cognitive hub
 * WHY: Ensure cognitive modules respond to lockdown
 * HOW: Publish lockdown event, restrict processing
 *
 * @param bridge Bridge handle
 * @param reason Lockdown reason
 * @return 0 on success, error code on failure
 */
int security_cognitive_coordinate_lockdown(
    security_cognitive_bridge_t bridge,
    const char* reason
);

/**
 * WHAT: Release lockdown coordination
 * WHY: Allow cognitive processing to resume
 * HOW: Publish recovery event, release restrictions
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_release_lockdown(security_cognitive_bridge_t bridge);

/**
 * WHAT: Trigger protective memory consolidation
 * WHY: Protect memory under attack
 * HOW: Publish memory consolidation event to cognitive hub
 *
 * @param bridge Bridge handle
 * @param threat_level Current threat level
 * @return 0 on success, error code on failure
 */
int security_cognitive_protect_memory(
    security_cognitive_bridge_t bridge,
    float threat_level
);

/**
 * WHAT: Shift cognitive attention to security concern
 * WHY: Prioritize security-relevant processing
 * HOW: Publish attention shift event
 *
 * @param bridge Bridge handle
 * @param priority Attention priority
 * @param target_category Target cognitive category
 * @return 0 on success, error code on failure
 */
int security_cognitive_shift_attention(
    security_cognitive_bridge_t bridge,
    uint32_t priority,
    cognitive_category_t target_category
);

/* ============================================================================
 * QUERY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Query cognitive module through bridge
 * WHY: Get cognitive state for security decisions
 * HOW: Route query through cognitive hub
 *
 * @param bridge Bridge handle
 * @param target_module Target cognitive module ID
 * @param query_type Type of query
 * @param result_out Output: query result
 * @return 0 on success, error code on failure
 */
int security_cognitive_query_cognitive(
    security_cognitive_bridge_t bridge,
    uint32_t target_module,
    uint32_t query_type,
    void* result_out
);

/**
 * WHAT: Get current security assessment for cognitive modules
 * WHY: Provide security context to cognitive processing
 * HOW: Return current threat assessment
 *
 * @param bridge Bridge handle
 * @param assessment_out Output: current threat assessment
 * @return 0 on success, error code on failure
 */
int security_cognitive_get_security_assessment(
    security_cognitive_bridge_t bridge,
    security_threat_assessment_t* assessment_out
);

/* ============================================================================
 * STATE AND STATISTICS
 * ============================================================================ */

/**
 * WHAT: Get bridge state
 * WHY: Query current operational state
 * HOW: Return state enum
 *
 * @param bridge Bridge handle
 * @param state_out Output: current state
 * @return 0 on success, error code on failure
 */
int security_cognitive_get_state(
    security_cognitive_bridge_t bridge,
    security_cognitive_state_t* state_out
);

/**
 * WHAT: Get bridge statistics
 * WHY: Monitor bridge performance
 * HOW: Copy statistics to output
 *
 * @param bridge Bridge handle
 * @param stats_out Output: statistics
 * @return 0 on success, error code on failure
 */
int security_cognitive_get_stats(
    security_cognitive_bridge_t bridge,
    security_cognitive_stats_t* stats_out
);

/**
 * WHAT: Reset bridge statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all counters
 *
 * @param bridge Bridge handle
 * @return 0 on success, error code on failure
 */
int security_cognitive_reset_stats(security_cognitive_bridge_t bridge);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get state name
 * WHY: Human-readable state
 * HOW: Lookup table
 *
 * @param state State enum
 * @return State name string
 */
const char* security_cognitive_state_name(security_cognitive_state_t state);

/**
 * WHAT: Print bridge summary
 * WHY: Debug output
 * HOW: Format and print state
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_cognitive_print_summary(security_cognitive_bridge_t bridge);

/**
 * WHAT: Print bridge statistics
 * WHY: Debug output
 * HOW: Format and print stats
 *
 * @param stats Statistics (NULL safe)
 */
void security_cognitive_print_stats(const security_cognitive_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_COGNITIVE_HUB_BRIDGE_H */
