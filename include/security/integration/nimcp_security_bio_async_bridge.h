/**
 * @file nimcp_security_bio_async_bridge.h
 * @brief Unified Security Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Central bio-async integration for the security subsystem that provides
 *       comprehensive message routing between all 77 security modules and NIMCP.
 *
 * WHY: The security system must coordinate across multiple components:
 *      - Event-driven threat detection broadcasts to all modules
 *      - Centralized access control coordination
 *      - Real-time security alert propagation
 *      - Policy enforcement across the system
 *      - Security consensus coordination
 *
 * HOW: Registers security subsystem as a bio-router module, maintains subscription
 *      registry, provides typed message broadcast APIs, and processes incoming
 *      security requests from all modules.
 *
 * SECURITY ARCHITECTURE:
 * =================================================================================
 *
 * SECURITY OUTPUT PATHWAYS:
 * -------------------------
 * 1. Threat Detection (Proactive):
 *    - Anomaly detection alerts
 *    - Pattern matching hits
 *    - BBB breach attempts
 *    - Mapped to: SECURITY_MSG_THREAT_DETECTED, SECURITY_MSG_ANOMALY_ALERT
 *
 * 2. Access Control (Reactive):
 *    - Access grant/deny decisions
 *    - Capability validations
 *    - Policy evaluations
 *    - Mapped to: SECURITY_MSG_ACCESS_RESPONSE, SECURITY_MSG_POLICY_DECISION
 *
 * 3. Security State (Informational):
 *    - Security level changes
 *    - Consensus status updates
 *    - Rate limiter states
 *    - Mapped to: SECURITY_MSG_LEVEL_CHANGE, SECURITY_MSG_CONSENSUS_UPDATE
 *
 * SECURITY INPUT PATHWAYS:
 * ------------------------
 * 1. Access Requests:
 *    - Module access requests
 *    - Resource capability checks
 *    - Mapped to: SECURITY_MSG_ACCESS_REQUEST
 *
 * 2. Threat Reports:
 *    - External threat notifications
 *    - Suspicious activity reports
 *    - Mapped to: SECURITY_MSG_THREAT_REPORT
 *
 * 3. Policy Updates:
 *    - Policy modification requests
 *    - Consensus proposals
 *    - Mapped to: SECURITY_MSG_POLICY_UPDATE
 *
 * MESSAGE ROUTING ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                 SECURITY BIO-ASYNC BRIDGE                               |
 * +=========================================================================+
 * |                                                                         |
 * |   OUTBOUND (Security -> Modules)                                        |
 * |   -----------------------------                                         |
 * |   +------------------+     +---------------------------------------+    |
 * |   | Threat Alerts    |---->| BIO_ROUTER: Broadcast to subscribers  |    |
 * |   | Access Decisions |     |  - All cognitive modules              |    |
 * |   | Policy Updates   |     |  - Brain regions                      |    |
 * |   | Level Changes    |     |  - Training subsystem                 |    |
 * |   +------------------+     +---------------------------------------+    |
 * |                                                                         |
 * |   INBOUND (Modules -> Security)                                         |
 * |   -----------------------------                                         |
 * |   +---------------------------------------+     +------------------+    |
 * |   | BIO_ROUTER: Receive from all modules  |---->| Security         |    |
 * |   |  - Access requests                    |     | Processing       |    |
 * |   |  - Threat reports                     |     | & Response       |    |
 * |   |  - Policy proposals                   |     | Generation       |    |
 * |   +---------------------------------------+     +------------------+    |
 * |                                                                         |
 * +=========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_BIO_ASYNC_BRIDGE_H
#define NIMCP_SECURITY_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes (0x5E00 - 0x5EFF range for security bio-async)
 * ============================================================================ */

/**
 * @brief Security bio-async bridge error codes
 */
typedef enum {
    SECURITY_BIO_OK = 0,                           /**< Success */
    SECURITY_BIO_ERROR_NULL_PARAM = 0x5E01,        /**< NULL parameter */
    SECURITY_BIO_ERROR_NOT_CONNECTED = 0x5E02,     /**< Bridge not connected */
    SECURITY_BIO_ERROR_ALREADY_CONNECTED = 0x5E03, /**< Already connected */
    SECURITY_BIO_ERROR_NO_MEMORY = 0x5E04,         /**< Memory allocation failed */
    SECURITY_BIO_ERROR_SUBSCRIPTION_FULL = 0x5E05, /**< Max subscriptions reached */
    SECURITY_BIO_ERROR_NOT_FOUND = 0x5E06,         /**< Entity not found */
    SECURITY_BIO_ERROR_INVALID_MSG_TYPE = 0x5E07,  /**< Invalid message type */
    SECURITY_BIO_ERROR_ROUTER_FAILED = 0x5E08,     /**< Router operation failed */
    SECURITY_BIO_ERROR_INVALID_CONFIG = 0x5E09,    /**< Invalid configuration */
    SECURITY_BIO_ERROR_TIMEOUT = 0x5E0A,           /**< Operation timed out */
    SECURITY_BIO_ERROR_ACCESS_DENIED = 0x5E0B,     /**< Access denied */
    SECURITY_BIO_ERROR_THREAT_ACTIVE = 0x5E0C      /**< Active threat blocks operation */
} security_bio_error_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum number of module subscriptions */
#define SECURITY_BIO_MAX_SUBSCRIPTIONS      128

/** Maximum pending messages in inbox */
#define SECURITY_BIO_MAX_INBOX_SIZE         512

/** Maximum pending messages in outbox */
#define SECURITY_BIO_MAX_OUTBOX_SIZE        256

/** Default broadcast interval for security state (ms) */
#define SECURITY_BIO_DEFAULT_BROADCAST_INTERVAL_MS  200

/** Message expiry time (ms) */
#define SECURITY_BIO_MESSAGE_TTL_MS         10000

/** Urgent threat response timeout (ms) */
#define SECURITY_BIO_URGENT_TIMEOUT_MS      50

/** Maximum threat details length */
#define SECURITY_BIO_MAX_THREAT_DETAILS     256

/** Maximum policy name length */
#define SECURITY_BIO_MAX_POLICY_NAME        64

/** Module ID for security subsystem (0x5E00 range) */
#define BIO_MODULE_SECURITY                 0x5E00

/* ============================================================================
 * Message Types
 * ============================================================================ */

/**
 * @brief Security bio-async message types
 *
 * WHAT: Message type enumeration for security bio-async routing
 * WHY:  Enables typed message handling and subscription filtering
 * HOW:  Each type corresponds to a specific security pathway
 */
typedef enum {
    /* Threat Detection (0x00-0x0F) */
    SECURITY_MSG_THREAT_DETECTED = 0,       /**< Threat detection alert */
    SECURITY_MSG_ANOMALY_ALERT,             /**< Anomaly detector alert */
    SECURITY_MSG_PATTERN_MATCH,             /**< Pattern DB match detected */
    SECURITY_MSG_BBB_BREACH,                /**< Blood-brain barrier breach attempt */
    SECURITY_MSG_RATE_LIMIT_HIT,            /**< Rate limiter triggered */
    SECURITY_MSG_INJECTION_ATTEMPT,         /**< Injection attack detected */

    /* Access Control (0x10-0x1F) */
    SECURITY_MSG_ACCESS_REQUEST = 0x10,     /**< Request access to resource */
    SECURITY_MSG_ACCESS_RESPONSE,           /**< Access decision response */
    SECURITY_MSG_CAPABILITY_CHECK,          /**< Capability validation request */
    SECURITY_MSG_CAPABILITY_RESULT,         /**< Capability validation result */
    SECURITY_MSG_POLICY_DECISION,           /**< Policy evaluation result */

    /* Security State (0x20-0x2F) */
    SECURITY_MSG_LEVEL_CHANGE = 0x20,       /**< Security level changed */
    SECURITY_MSG_CONSENSUS_UPDATE,          /**< Consensus status update */
    SECURITY_MSG_LOCKDOWN_START,            /**< Security lockdown initiated */
    SECURITY_MSG_LOCKDOWN_END,              /**< Security lockdown ended */
    SECURITY_MSG_AUDIT_EVENT,               /**< Security audit event */

    /* Policy Management (0x30-0x3F) */
    SECURITY_MSG_POLICY_UPDATE = 0x30,      /**< Policy update notification */
    SECURITY_MSG_POLICY_REQUEST,            /**< Policy evaluation request */
    SECURITY_MSG_CONSENSUS_PROPOSAL,        /**< Consensus proposal */
    SECURITY_MSG_CONSENSUS_VOTE,            /**< Consensus vote */
    SECURITY_MSG_CONSENSUS_RESULT,          /**< Consensus final result */

    /* Monitoring (0x40-0x4F) */
    SECURITY_MSG_HEARTBEAT = 0x40,          /**< Security subsystem heartbeat */
    SECURITY_MSG_METRICS_UPDATE,            /**< Security metrics broadcast */
    SECURITY_MSG_COVERAGE_REPORT,           /**< Coverage analysis report */
    SECURITY_MSG_HEALTH_STATUS,             /**< Security health status */

    SECURITY_MSG_COUNT
} security_bio_msg_type_t;

/**
 * @brief Bitmask for message type subscriptions
 */
#define SECURITY_BIO_SUB_THREAT_DETECTED    (1U << SECURITY_MSG_THREAT_DETECTED)
#define SECURITY_BIO_SUB_ANOMALY_ALERT      (1U << SECURITY_MSG_ANOMALY_ALERT)
#define SECURITY_BIO_SUB_PATTERN_MATCH      (1U << SECURITY_MSG_PATTERN_MATCH)
#define SECURITY_BIO_SUB_BBB_BREACH         (1U << SECURITY_MSG_BBB_BREACH)
#define SECURITY_BIO_SUB_RATE_LIMIT         (1U << SECURITY_MSG_RATE_LIMIT_HIT)
#define SECURITY_BIO_SUB_INJECTION          (1U << SECURITY_MSG_INJECTION_ATTEMPT)
#define SECURITY_BIO_SUB_ACCESS_REQUEST     (1U << SECURITY_MSG_ACCESS_REQUEST)
#define SECURITY_BIO_SUB_ACCESS_RESPONSE    (1U << SECURITY_MSG_ACCESS_RESPONSE)
#define SECURITY_BIO_SUB_LEVEL_CHANGE       (1U << SECURITY_MSG_LEVEL_CHANGE)
#define SECURITY_BIO_SUB_LOCKDOWN           ((1U << SECURITY_MSG_LOCKDOWN_START) | \
                                             (1U << SECURITY_MSG_LOCKDOWN_END))
#define SECURITY_BIO_SUB_ALL_THREATS        (SECURITY_BIO_SUB_THREAT_DETECTED | \
                                             SECURITY_BIO_SUB_ANOMALY_ALERT | \
                                             SECURITY_BIO_SUB_PATTERN_MATCH | \
                                             SECURITY_BIO_SUB_BBB_BREACH | \
                                             SECURITY_BIO_SUB_RATE_LIMIT | \
                                             SECURITY_BIO_SUB_INJECTION)
#define SECURITY_BIO_SUB_ALL                (0xFFFFFFFFU)

/* ============================================================================
 * Threat Type Enumeration
 * ============================================================================ */

/**
 * @brief Security threat types
 *
 * WHAT: Categorization of detected threats
 * WHY:  Enable appropriate response based on threat type
 * HOW:  Enum covers all major security threat categories
 */
typedef enum {
    SECURITY_THREAT_NONE = 0,               /**< No threat detected */
    SECURITY_THREAT_INJECTION,              /**< Code/prompt injection */
    SECURITY_THREAT_ANOMALY,                /**< Behavioral anomaly */
    SECURITY_THREAT_PATTERN_MATCH,          /**< Known malicious pattern */
    SECURITY_THREAT_RATE_VIOLATION,         /**< Rate limit violation */
    SECURITY_THREAT_BBB_BREACH,             /**< BBB boundary violation */
    SECURITY_THREAT_CAPABILITY_ABUSE,       /**< Capability misuse */
    SECURITY_THREAT_CONSENSUS_ATTACK,       /**< Consensus manipulation */
    SECURITY_THREAT_EXCITOTOXICITY,         /**< Neural excitotoxicity */
    SECURITY_THREAT_SYNAPTIC_POISON,        /**< Synaptic weight poisoning */
    SECURITY_THREAT_NEUROMOD_HIJACK,        /**< Neuromodulator hijacking */
    SECURITY_THREAT_HEBBIAN_POISON,         /**< STDP exploitation */
    SECURITY_THREAT_UNKNOWN                 /**< Unknown threat type */
} security_threat_type_t;

/* ============================================================================
 * Access Request Type Enumeration
 * ============================================================================ */

/**
 * @brief Access request types
 *
 * WHAT: Types of resource access requests
 * WHY:  Different access types require different security checks
 * HOW:  Enum covers common access patterns
 */
typedef enum {
    SECURITY_ACCESS_READ = 0,               /**< Read access */
    SECURITY_ACCESS_WRITE,                  /**< Write access */
    SECURITY_ACCESS_EXECUTE,                /**< Execute access */
    SECURITY_ACCESS_CREATE,                 /**< Create resource */
    SECURITY_ACCESS_DELETE,                 /**< Delete resource */
    SECURITY_ACCESS_ADMIN,                  /**< Administrative access */
    SECURITY_ACCESS_NETWORK,                /**< Network access */
    SECURITY_ACCESS_MEMORY,                 /**< Direct memory access */
    SECURITY_ACCESS_WEIGHT_UPDATE,          /**< Neural weight modification */
    SECURITY_ACCESS_NEUROMOD,               /**< Neuromodulator access */
    SECURITY_ACCESS_CONSENSUS               /**< Consensus participation */
} security_access_type_t;

/* ============================================================================
 * Access Decision Enumeration
 * ============================================================================ */

/**
 * @brief Access decision results
 */
typedef enum {
    SECURITY_DECISION_PENDING = 0,          /**< Decision pending */
    SECURITY_DECISION_GRANTED,              /**< Access granted */
    SECURITY_DECISION_DENIED,               /**< Access denied */
    SECURITY_DECISION_RATE_LIMITED,         /**< Temporarily rate limited */
    SECURITY_DECISION_REQUIRES_CONSENSUS,   /**< Requires consensus approval */
    SECURITY_DECISION_LOCKDOWN              /**< Denied due to lockdown */
} security_decision_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief Threat detection message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    security_threat_type_t threat_type;     /**< Type of threat detected */
    nimcp_threat_level_t severity;          /**< Threat severity level */
    uint32_t source_module;                 /**< Module that detected threat */
    uint32_t target_module;                 /**< Module being targeted */

    float confidence;                       /**< Detection confidence [0, 1] */
    float anomaly_score;                    /**< Anomaly score if applicable */

    char details[SECURITY_BIO_MAX_THREAT_DETAILS]; /**< Threat details */

    uint64_t timestamp_us;                  /**< Detection timestamp */
} security_bio_threat_msg_t;

/**
 * @brief Anomaly alert message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    float content_anomaly_score;            /**< Content-based anomaly [0, 1] */
    float behavior_anomaly_score;           /**< Behavior-based anomaly [0, 1] */
    float timing_anomaly_score;             /**< Timing-based anomaly [0, 1] */
    float overall_score;                    /**< Combined anomaly score [0, 1] */

    uint32_t trigger_flags;                 /**< Which features triggered */
    uint32_t source_module;                 /**< Source of anomalous activity */

    bool requires_action;                   /**< Immediate action needed */

    uint64_t timestamp_us;                  /**< Alert timestamp */
} security_bio_anomaly_msg_t;

/**
 * @brief Access request message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t requester_module;              /**< Module requesting access */
    uint32_t target_resource;               /**< Resource being accessed */
    security_access_type_t access_type;     /**< Type of access requested */

    uint32_t capability_id;                 /**< Capability being used */
    uint32_t request_id;                    /**< Unique request ID for tracking */

    char resource_name[64];                 /**< Human-readable resource name */

    uint64_t timestamp_us;                  /**< Request timestamp */
} security_bio_access_request_msg_t;

/**
 * @brief Access response message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t request_id;                    /**< Original request ID */
    security_decision_t decision;           /**< Access decision */

    char reason[128];                       /**< Reason for decision */
    uint32_t retry_after_ms;                /**< Retry delay if rate limited */

    float trust_score;                      /**< Requester's trust score [0, 1] */

    uint64_t timestamp_us;                  /**< Decision timestamp */
} security_bio_access_response_msg_t;

/**
 * @brief Security level change message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t old_level;                     /**< Previous security level */
    uint32_t new_level;                     /**< New security level */

    security_threat_type_t trigger_threat;  /**< Threat that triggered change */
    uint32_t trigger_module;                /**< Module that triggered change */

    char reason[128];                       /**< Reason for level change */

    uint64_t timestamp_us;                  /**< Change timestamp */
} security_bio_level_change_msg_t;

/**
 * @brief Security lockdown message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    bool is_start;                          /**< true = start, false = end */
    uint32_t lockdown_level;                /**< Lockdown severity level */
    security_threat_type_t trigger_threat;  /**< Threat that caused lockdown */

    uint32_t affected_modules;              /**< Bitmask of affected modules */
    uint32_t duration_estimate_ms;          /**< Estimated lockdown duration */

    char reason[128];                       /**< Reason for lockdown */

    uint64_t timestamp_us;                  /**< Lockdown timestamp */
} security_bio_lockdown_msg_t;

/**
 * @brief Consensus update message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t proposal_id;                   /**< Consensus proposal ID */
    uint32_t votes_for;                     /**< Votes in favor */
    uint32_t votes_against;                 /**< Votes against */
    uint32_t votes_pending;                 /**< Votes still pending */

    float agreement_ratio;                  /**< Current agreement ratio [0, 1] */
    bool is_complete;                       /**< Consensus reached */
    bool result;                            /**< Final result if complete */

    uint64_t timestamp_us;                  /**< Update timestamp */
} security_bio_consensus_msg_t;

/**
 * @brief Security metrics message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint64_t threats_detected;              /**< Total threats detected */
    uint64_t threats_mitigated;             /**< Threats successfully mitigated */
    uint64_t access_requests;               /**< Total access requests */
    uint64_t access_denied;                 /**< Access requests denied */

    float avg_response_time_us;             /**< Average response time */
    float detection_rate;                   /**< Detection success rate [0, 1] */
    float false_positive_rate;              /**< False positive rate [0, 1] */

    uint32_t active_lockdowns;              /**< Currently active lockdowns */
    uint32_t current_security_level;        /**< Current security level */

    uint64_t timestamp_us;                  /**< Metrics timestamp */
} security_bio_metrics_msg_t;

/**
 * @brief Security heartbeat message payload
 */
typedef struct {
    bio_message_header_t header;            /**< Standard bio-async header */

    uint32_t security_level;                /**< Current security level */
    bool is_healthy;                        /**< Subsystem health status */
    bool lockdown_active;                   /**< Any lockdown active */

    uint32_t active_monitors;               /**< Active monitoring threads */
    uint64_t uptime_ms;                     /**< Security subsystem uptime */

    uint64_t timestamp_us;                  /**< Heartbeat timestamp */
} security_bio_heartbeat_msg_t;

/* ============================================================================
 * Subscription Structure
 * ============================================================================ */

/**
 * @brief Module subscription entry
 */
typedef struct {
    bio_module_id_t module_id;              /**< Subscribed module ID */
    uint64_t msg_type_mask;                 /**< Bitmask of subscribed types */
    bool active;                            /**< Subscription active */
    uint64_t subscription_time;             /**< When subscribed */
    uint64_t messages_sent;                 /**< Messages sent to this sub */
    uint32_t trust_level;                   /**< Module's trust level */
} security_bio_subscription_t;

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief Security bio-async bridge configuration
 */
typedef struct {
    /* Broadcast timing */
    uint32_t heartbeat_interval_ms;         /**< Heartbeat broadcast interval */
    uint32_t metrics_interval_ms;           /**< Metrics broadcast interval */
    bool enable_auto_broadcast;             /**< Auto-broadcast security state */

    /* Message handling */
    uint32_t max_inbox_process_per_update;  /**< Max inbox messages per update */
    uint32_t message_ttl_ms;                /**< Message time-to-live */
    uint32_t urgent_timeout_ms;             /**< Timeout for urgent messages */

    /* Priority settings */
    nimcp_bio_channel_type_t default_channel; /**< Default channel */
    nimcp_bio_channel_type_t alert_channel;   /**< Channel for alerts */
    nimcp_bio_channel_type_t urgent_channel;  /**< Channel for urgent threats */

    /* Subscription limits */
    uint32_t max_subscriptions;             /**< Maximum module subscriptions */

    /* Threat response */
    float threat_escalation_threshold;      /**< Threshold for escalation [0, 1] */
    bool enable_auto_lockdown;              /**< Auto-lockdown on critical threats */
    uint32_t lockdown_cooldown_ms;          /**< Minimum time between lockdowns */

    /* Feature flags */
    bool enable_threat_routing;             /**< Enable threat signal routing */
    bool enable_access_control;             /**< Enable access control coordination */
    bool enable_consensus;                  /**< Enable consensus protocol */
    bool enable_metrics_broadcast;          /**< Broadcast security metrics */
    bool enable_logging;                    /**< Enable message logging */
} security_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;                 /**< Total messages sent */
    uint64_t messages_received;             /**< Total messages received */
    uint64_t messages_dropped;              /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;               /**< Broadcast messages sent */

    /* Per-type counts */
    uint64_t threat_alerts_sent;            /**< Threat alerts broadcast */
    uint64_t anomaly_alerts_sent;           /**< Anomaly alerts sent */
    uint64_t access_requests_processed;     /**< Access requests handled */
    uint64_t access_denied_count;           /**< Access denials */
    uint64_t lockdowns_initiated;           /**< Lockdowns started */
    uint64_t consensus_proposals;           /**< Consensus proposals processed */

    /* Subscription stats */
    uint32_t active_subscriptions;          /**< Currently active subs */
    uint32_t peak_subscriptions;            /**< Peak subscription count */

    /* Timing stats */
    uint64_t last_broadcast_time_us;        /**< Last broadcast timestamp */
    float avg_message_latency_us;           /**< Average message latency */
    float max_message_latency_us;           /**< Peak message latency */
    float avg_threat_response_us;           /**< Average threat response time */

    /* Error counts */
    uint64_t handler_errors;                /**< Message handler errors */
    uint64_t routing_errors;                /**< Routing failures */
} security_bio_bridge_stats_t;

/* ============================================================================
 * Bridge Structure (Opaque Handle)
 * ============================================================================ */

/**
 * @brief Security bio-async bridge handle
 */
typedef struct security_bio_bridge_struct security_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for security bio-async bridge configuration
 * WHY:  Easy initialization with security-appropriate parameters
 * HOW:  Return struct with conservative security defaults
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_default_config(security_bio_bridge_config_t* config);

/**
 * @brief Create security bio-async bridge
 *
 * WHAT: Initialize bio-async integration for security subsystem
 * WHY:  Enable message routing between security and all modules
 * HOW:  Allocate structure, initialize subscription registry, prepare handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
security_bio_bridge_t* security_bio_bridge_create(
    const security_bio_bridge_config_t* config
);

/**
 * @brief Destroy security bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from router, free subscriptions, release memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void security_bio_bridge_destroy(security_bio_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-router
 *
 * WHAT: Establish connection to bio-router for message routing
 * WHY:  Enable bidirectional message flow with all modules
 * HOW:  Register with router, set up handlers
 *
 * @param bridge Bio-async bridge
 * @param router Bio-router (NULL to use global)
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_connect(
    security_bio_bridge_t* bridge,
    bio_router_t router
);

/**
 * @brief Disconnect bridge from router
 *
 * WHAT: Disconnect from bio-router
 * WHY:  Clean disconnection before shutdown
 * HOW:  Unregister handlers, clear module context
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_disconnect(security_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio-async bridge
 * @return true if connected to router
 */
bool security_bio_bridge_is_connected(const security_bio_bridge_t* bridge);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages from bio-router inbox
 * WHY:  Handle incoming security requests and reports
 * HOW:  Pop messages, dispatch to appropriate handlers, update state
 *
 * @param bridge Bio-async bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int security_bio_bridge_process_inbox(
    security_bio_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Update bridge state and auto-broadcasts
 *
 * WHAT: Perform periodic bridge updates
 * WHY:  Send scheduled broadcasts, check timeouts
 * HOW:  Check timers, send due broadcasts, cleanup expired entries
 *
 * @param bridge Bio-async bridge
 * @param delta_ms Time since last update
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_update(
    security_bio_bridge_t* bridge,
    uint32_t delta_ms
);

/* ============================================================================
 * Threat Notification API
 * ============================================================================ */

/**
 * @brief Broadcast threat detection alert
 *
 * WHAT: Send threat detection to all subscribed modules
 * WHY:  System-wide threat awareness for coordinated response
 * HOW:  Package threat info, broadcast on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param threat_type Type of threat detected
 * @param severity Threat severity level
 * @param source_module Module that detected the threat
 * @param confidence Detection confidence [0, 1]
 * @param details Human-readable details (can be NULL)
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_threat(
    security_bio_bridge_t* bridge,
    security_threat_type_t threat_type,
    nimcp_threat_level_t severity,
    uint32_t source_module,
    float confidence,
    const char* details
);

/**
 * @brief Broadcast anomaly alert
 *
 * WHAT: Send anomaly detection alert to subscribers
 * WHY:  Enable coordinated response to behavioral anomalies
 * HOW:  Package anomaly scores, broadcast on alert channel
 *
 * @param bridge Bio-async bridge
 * @param content_score Content anomaly score [0, 1]
 * @param behavior_score Behavior anomaly score [0, 1]
 * @param timing_score Timing anomaly score [0, 1]
 * @param source_module Source of anomalous activity
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_anomaly(
    security_bio_bridge_t* bridge,
    float content_score,
    float behavior_score,
    float timing_score,
    uint32_t source_module
);

/**
 * @brief Initiate security lockdown
 *
 * WHAT: Broadcast lockdown initiation to all modules
 * WHY:  Coordinate system-wide security response
 * HOW:  Send lockdown message on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param level Lockdown severity level
 * @param trigger_threat Threat that triggered lockdown
 * @param reason Reason for lockdown
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_initiate_lockdown(
    security_bio_bridge_t* bridge,
    uint32_t level,
    security_threat_type_t trigger_threat,
    const char* reason
);

/**
 * @brief End security lockdown
 *
 * WHAT: Broadcast lockdown end to all modules
 * WHY:  Allow normal operations to resume
 * HOW:  Send lockdown end message
 *
 * @param bridge Bio-async bridge
 * @param reason Reason for ending lockdown
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_end_lockdown(
    security_bio_bridge_t* bridge,
    const char* reason
);

/* ============================================================================
 * Access Control Coordination API
 * ============================================================================ */

/**
 * @brief Send access request
 *
 * WHAT: Request access to a resource through security subsystem
 * WHY:  Centralized access control coordination
 * HOW:  Package request, send to security coordinator
 *
 * @param bridge Bio-async bridge
 * @param requester_module Requesting module ID
 * @param target_resource Target resource ID
 * @param access_type Type of access requested
 * @param request_id Output: unique request ID for tracking
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_request_access(
    security_bio_bridge_t* bridge,
    uint32_t requester_module,
    uint32_t target_resource,
    security_access_type_t access_type,
    uint32_t* request_id
);

/**
 * @brief Send access decision response
 *
 * WHAT: Respond to an access request with decision
 * WHY:  Complete access control flow
 * HOW:  Package decision, send to requester
 *
 * @param bridge Bio-async bridge
 * @param request_id Original request ID
 * @param decision Access decision
 * @param reason Reason for decision
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_respond_access(
    security_bio_bridge_t* bridge,
    uint32_t request_id,
    security_decision_t decision,
    const char* reason
);

/**
 * @brief Broadcast security level change
 *
 * WHAT: Notify all modules of security level change
 * WHY:  Coordinate security posture across system
 * HOW:  Broadcast level change message
 *
 * @param bridge Bio-async bridge
 * @param old_level Previous security level
 * @param new_level New security level
 * @param trigger_threat Threat that triggered change (can be NONE)
 * @param reason Reason for level change
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_level_change(
    security_bio_bridge_t* bridge,
    uint32_t old_level,
    uint32_t new_level,
    security_threat_type_t trigger_threat,
    const char* reason
);

/* ============================================================================
 * Consensus Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast consensus proposal
 *
 * WHAT: Start a security consensus proposal
 * WHY:  Distributed security decisions
 * HOW:  Broadcast proposal to eligible voters
 *
 * @param bridge Bio-async bridge
 * @param proposal_id Unique proposal ID
 * @param description Proposal description
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_propose_consensus(
    security_bio_bridge_t* bridge,
    uint32_t proposal_id,
    const char* description
);

/**
 * @brief Broadcast consensus update
 *
 * WHAT: Update all modules on consensus progress
 * WHY:  Transparency in distributed decisions
 * HOW:  Broadcast current vote tallies
 *
 * @param bridge Bio-async bridge
 * @param proposal_id Proposal ID
 * @param votes_for Votes in favor
 * @param votes_against Votes against
 * @param is_complete Whether consensus reached
 * @param result Final result if complete
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_consensus_update(
    security_bio_bridge_t* bridge,
    uint32_t proposal_id,
    uint32_t votes_for,
    uint32_t votes_against,
    bool is_complete,
    bool result
);

/* ============================================================================
 * Monitoring API
 * ============================================================================ */

/**
 * @brief Broadcast security heartbeat
 *
 * WHAT: Send periodic heartbeat to indicate security subsystem health
 * WHY:  Liveness monitoring
 * HOW:  Package health status, broadcast periodically
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_heartbeat(security_bio_bridge_t* bridge);

/**
 * @brief Broadcast security metrics
 *
 * WHAT: Send current security metrics to subscribers
 * WHY:  Visibility into security performance
 * HOW:  Package metrics, broadcast periodically
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_broadcast_metrics(security_bio_bridge_t* bridge);

/* ============================================================================
 * Subscription Management API
 * ============================================================================ */

/**
 * @brief Subscribe module to security messages
 *
 * WHAT: Register module to receive specific security message types
 * WHY:  Enable selective message routing to interested modules
 * HOW:  Add entry to subscription registry with type mask
 *
 * @param bridge Bio-async bridge
 * @param module_id Module requesting subscription
 * @param msg_types Bitmask of message types (SECURITY_BIO_SUB_*)
 * @param trust_level Module's trust level
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_subscribe_module(
    security_bio_bridge_t* bridge,
    uint32_t module_id,
    uint64_t msg_types,
    uint32_t trust_level
);

/**
 * @brief Unsubscribe module from security messages
 *
 * WHAT: Remove module subscription
 * WHY:  Clean unsubscription when module no longer needs messages
 * HOW:  Remove entry from subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to unsubscribe
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_unsubscribe_module(
    security_bio_bridge_t* bridge,
    uint32_t module_id
);

/**
 * @brief Update module trust level
 *
 * WHAT: Modify trust level for existing subscription
 * WHY:  Dynamic trust adjustment based on behavior
 * HOW:  Update trust level in subscription registry
 *
 * @param bridge Bio-async bridge
 * @param module_id Module to update
 * @param trust_level New trust level
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_update_trust_level(
    security_bio_bridge_t* bridge,
    uint32_t module_id,
    uint32_t trust_level
);

/**
 * @brief Get subscription count for message type
 *
 * @param bridge Bio-async bridge
 * @param msg_type Message type to query
 * @return Number of subscribers for this type
 */
uint32_t security_bio_bridge_get_subscriber_count(
    const security_bio_bridge_t* bridge,
    security_bio_msg_type_t msg_type
);

/* ============================================================================
 * Statistics and Diagnostics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bio-async bridge
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_get_stats(
    const security_bio_bridge_t* bridge,
    security_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int security_bio_bridge_reset_stats(security_bio_bridge_t* bridge);

/**
 * @brief Get message type name
 *
 * @param msg_type Message type
 * @return Human-readable name string
 */
const char* security_bio_msg_type_name(security_bio_msg_type_t msg_type);

/**
 * @brief Get threat type name
 *
 * @param threat_type Threat type
 * @return Human-readable name string
 */
const char* security_threat_type_name(security_threat_type_t threat_type);

/**
 * @brief Get access type name
 *
 * @param access_type Access type
 * @return Human-readable name string
 */
const char* security_access_type_name(security_access_type_t access_type);

/**
 * @brief Get decision name
 *
 * @param decision Access decision
 * @return Human-readable name string
 */
const char* security_decision_name(security_decision_t decision);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bio-async bridge (NULL-safe)
 */
void security_bio_bridge_print_summary(const security_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_BIO_ASYNC_BRIDGE_H */
