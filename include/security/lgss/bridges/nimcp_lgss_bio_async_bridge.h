/**
 * @file nimcp_lgss_bio_async_bridge.h
 * @brief LGSS Bio-Async Integration Bridge
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Bio-async integration bridge for the Layered Governance Safety System (LGSS)
 *       providing message routing between LGSS components and other NIMCP modules.
 *
 * WHY: The LGSS must coordinate safety evaluations, policy enforcement, and audit
 *      logging across the entire system:
 *      - Real-time action evaluation before execution
 *      - Policy violation detection and notification
 *      - Risk assessment and uncertainty quantification
 *      - Override and control mechanisms for human intervention
 *      - Audit trail and telemetry for accountability
 *      - Integrity monitoring to detect tampering
 *      - Plasticity coordination for safe learning
 *
 * HOW: Registers LGSS as a bio-router module, maintains handler registry,
 *      provides typed message APIs, and processes safety-critical requests.
 *
 * MESSAGE ARCHITECTURE:
 * =============================================================================
 *
 * LGSS OUTPUT PATHWAYS:
 * ---------------------
 * 1. Evaluation Results (Reactive):
 *    - Evaluation response (allow/block/escalate)
 *    - Policy decision notifications
 *    - Mapped to: BIO_MSG_LGSS_EVALUATE_RESPONSE
 *
 * 2. Violation Alerts (Proactive):
 *    - Policy violation detection
 *    - Action blocked notifications
 *    - Escalation requests
 *    - Mapped to: BIO_MSG_LGSS_POLICY_VIOLATION, BIO_MSG_LGSS_ACTION_BLOCKED
 *
 * 3. Risk/Uncertainty (Informational):
 *    - Impact scores
 *    - Uncertainty alerts
 *    - Risk assessments
 *    - Mapped to: BIO_MSG_LGSS_UNCERTAINTY_ALERT, BIO_MSG_LGSS_RISK_ASSESSMENT
 *
 * 4. Integrity (Monitoring):
 *    - Integrity check results
 *    - Tampering detection alerts
 *    - Mapped to: BIO_MSG_LGSS_INTEGRITY_RESULT, BIO_MSG_LGSS_TAMPERING_DETECTED
 *
 * LGSS INPUT PATHWAYS:
 * --------------------
 * 1. Evaluation Requests:
 *    - Action evaluation before execution
 *    - Mapped to: BIO_MSG_LGSS_EVALUATE_REQUEST
 *
 * 2. Override Requests:
 *    - Human override for blocked actions
 *    - Mapped to: BIO_MSG_LGSS_OVERRIDE_REQUEST
 *
 * 3. Audit Requests:
 *    - Audit log retrieval
 *    - Mapped to: BIO_MSG_LGSS_AUDIT_REQUEST
 *
 * 4. Control Commands:
 *    - Halt, soft reset, hard reset
 *    - Mapped to: BIO_MSG_LGSS_HALT_COMMAND, BIO_MSG_LGSS_SOFT_RESET, etc.
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

#ifndef NIMCP_LGSS_BIO_ASYNC_BRIDGE_H
#define NIMCP_LGSS_BIO_ASYNC_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module dependencies */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Codes (0x1C80 - 0x1CFF range for LGSS bio-async)
 * ============================================================================ */

/**
 * @brief LGSS bio-async bridge error codes
 */
typedef enum {
    LGSS_BIO_OK = 0,                              /**< Success */
    LGSS_BIO_ERROR_NULL_PARAM = 0x1C81,           /**< NULL parameter */
    LGSS_BIO_ERROR_NOT_CONNECTED = 0x1C82,        /**< Bridge not connected */
    LGSS_BIO_ERROR_ALREADY_CONNECTED = 0x1C83,    /**< Already connected */
    LGSS_BIO_ERROR_NO_MEMORY = 0x1C84,            /**< Memory allocation failed */
    LGSS_BIO_ERROR_HANDLER_FULL = 0x1C85,         /**< Max handlers reached */
    LGSS_BIO_ERROR_NOT_FOUND = 0x1C86,            /**< Entity not found */
    LGSS_BIO_ERROR_INVALID_MSG_TYPE = 0x1C87,     /**< Invalid message type */
    LGSS_BIO_ERROR_ROUTER_FAILED = 0x1C88,        /**< Router operation failed */
    LGSS_BIO_ERROR_INVALID_CONFIG = 0x1C89,       /**< Invalid configuration */
    LGSS_BIO_ERROR_TIMEOUT = 0x1C8A,              /**< Operation timed out */
    LGSS_BIO_ERROR_EVALUATION_FAILED = 0x1C8B,    /**< Evaluation failed */
    LGSS_BIO_ERROR_POLICY_BLOCKED = 0x1C8C,       /**< Policy blocked the action */
    LGSS_BIO_ERROR_OVERRIDE_DENIED = 0x1C8D,      /**< Override request denied */
    LGSS_BIO_ERROR_HALTED = 0x1C8E                /**< System halted by LGSS */
} lgss_bio_error_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum registered message handlers */
#define LGSS_BIO_MAX_HANDLERS               64

/** Maximum pending messages in inbox */
#define LGSS_BIO_MAX_INBOX_SIZE             256

/** Maximum pending messages in outbox */
#define LGSS_BIO_MAX_OUTBOX_SIZE            128

/** Default evaluation timeout (ms) */
#define LGSS_BIO_DEFAULT_EVAL_TIMEOUT_MS    100

/** Message expiry time (ms) */
#define LGSS_BIO_MESSAGE_TTL_MS             5000

/** Urgent threat response timeout (ms) */
#define LGSS_BIO_URGENT_TIMEOUT_MS          25

/** Maximum action description length */
#define LGSS_BIO_MAX_ACTION_DESC            256

/** Maximum policy name length */
#define LGSS_BIO_MAX_POLICY_NAME            64

/** Maximum reason/details length */
#define LGSS_BIO_MAX_REASON_LEN             256

/** Maximum audit log entries in response */
#define LGSS_BIO_MAX_AUDIT_ENTRIES          100

/* ============================================================================
 * LGSS Decision Types
 * ============================================================================ */

/**
 * @brief LGSS evaluation decision types
 *
 * WHAT: Result of LGSS policy evaluation
 * WHY:  Different actions require different responses
 * HOW:  Enum covers all possible LGSS decisions
 */
typedef enum {
    LGSS_DECISION_ALLOW = 0,           /**< Action permitted */
    LGSS_DECISION_BLOCK,               /**< Action blocked by policy */
    LGSS_DECISION_ESCALATE,            /**< Requires human review */
    LGSS_DECISION_DEFER,               /**< Decision deferred (more info needed) */
    LGSS_DECISION_PENDING,             /**< Evaluation in progress */
    LGSS_DECISION_TIMEOUT,             /**< Evaluation timed out */
    LGSS_DECISION_ERROR                /**< Evaluation error occurred */
} lgss_decision_t;

/**
 * @brief LGSS policy violation severity levels
 */
typedef enum {
    LGSS_SEVERITY_INFO = 0,            /**< Informational (logged only) */
    LGSS_SEVERITY_LOW,                 /**< Low severity (warning) */
    LGSS_SEVERITY_MEDIUM,              /**< Medium severity (blocked) */
    LGSS_SEVERITY_HIGH,                /**< High severity (escalated) */
    LGSS_SEVERITY_CRITICAL             /**< Critical (system halt) */
} lgss_severity_t;

/**
 * @brief LGSS override authorization levels
 */
typedef enum {
    LGSS_OVERRIDE_NONE = 0,            /**< No override allowed */
    LGSS_OVERRIDE_OPERATOR,            /**< Operator-level override */
    LGSS_OVERRIDE_ADMIN,               /**< Admin-level override */
    LGSS_OVERRIDE_EMERGENCY            /**< Emergency override (all levels) */
} lgss_override_level_t;

/**
 * @brief LGSS integrity check status
 */
typedef enum {
    LGSS_INTEGRITY_OK = 0,             /**< Integrity verified */
    LGSS_INTEGRITY_MODIFIED,           /**< Authorized modification detected */
    LGSS_INTEGRITY_TAMPERED,           /**< Unauthorized modification detected */
    LGSS_INTEGRITY_CORRUPTED,          /**< Data corruption detected */
    LGSS_INTEGRITY_UNKNOWN             /**< Unable to verify */
} lgss_integrity_status_t;

/**
 * @brief LGSS reset types
 */
typedef enum {
    LGSS_RESET_SOFT = 0,               /**< Soft reset (preserve state) */
    LGSS_RESET_HARD,                   /**< Hard reset (clear state) */
    LGSS_RESET_FACTORY                 /**< Factory reset (restore defaults) */
} lgss_reset_type_t;

/* ============================================================================
 * Message Payload Structures
 * ============================================================================ */

/**
 * @brief LGSS evaluation request message payload
 *
 * WHAT: Request LGSS to evaluate an action before execution
 * WHY:  Pre-emptive safety checks prevent harmful actions
 * HOW:  Module sends request, LGSS evaluates, returns decision
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Unique request ID for tracking */
    uint32_t source_module;             /**< Module requesting evaluation */
    uint32_t action_type;               /**< Type of action to evaluate */

    float uncertainty;                  /**< Requester's uncertainty [0, 1] */
    float impact_estimate;              /**< Estimated impact [0, 1] */
    bool reversible;                    /**< Is action reversible? */

    char action_desc[LGSS_BIO_MAX_ACTION_DESC]; /**< Action description */

    /* Optional context */
    uint32_t context_size;              /**< Size of additional context */
    uint8_t context[128];               /**< Additional context data */

    uint64_t timestamp_us;              /**< Request timestamp */
} lgss_bio_evaluate_request_t;

/**
 * @brief LGSS evaluation response message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Original request ID */
    lgss_decision_t decision;           /**< Evaluation decision */

    float confidence;                   /**< Decision confidence [0, 1] */
    float risk_score;                   /**< Computed risk score [0, 1] */
    lgss_severity_t severity;           /**< Severity if violated */

    char policy_name[LGSS_BIO_MAX_POLICY_NAME]; /**< Policy that was applied */
    char reason[LGSS_BIO_MAX_REASON_LEN];       /**< Reason for decision */

    bool override_available;            /**< Can decision be overridden? */
    lgss_override_level_t override_level; /**< Required override level */

    uint64_t timestamp_us;              /**< Response timestamp */
} lgss_bio_evaluate_response_t;

/**
 * @brief LGSS policy violation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t violation_id;              /**< Unique violation ID */
    uint32_t source_module;             /**< Module that violated policy */
    lgss_severity_t severity;           /**< Violation severity */

    char policy_name[LGSS_BIO_MAX_POLICY_NAME]; /**< Violated policy name */
    char action_desc[LGSS_BIO_MAX_ACTION_DESC]; /**< Action that violated */
    char details[LGSS_BIO_MAX_REASON_LEN];      /**< Violation details */

    float risk_score;                   /**< Risk assessment [0, 1] */
    bool action_blocked;                /**< Was the action blocked? */
    bool requires_escalation;           /**< Needs human review? */

    uint64_t timestamp_us;              /**< Violation timestamp */
} lgss_bio_policy_violation_t;

/**
 * @brief LGSS action blocked message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Original request ID (if applicable) */
    uint32_t source_module;             /**< Module whose action was blocked */

    char action_desc[LGSS_BIO_MAX_ACTION_DESC]; /**< Blocked action description */
    char policy_name[LGSS_BIO_MAX_POLICY_NAME]; /**< Blocking policy */
    char reason[LGSS_BIO_MAX_REASON_LEN];       /**< Block reason */

    lgss_severity_t severity;           /**< Severity of the blocked action */
    bool override_available;            /**< Can be overridden? */

    uint64_t timestamp_us;              /**< Block timestamp */
} lgss_bio_action_blocked_t;

/**
 * @brief LGSS action escalated message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t escalation_id;             /**< Unique escalation ID */
    uint32_t source_module;             /**< Module whose action was escalated */

    char action_desc[LGSS_BIO_MAX_ACTION_DESC]; /**< Escalated action */
    char reason[LGSS_BIO_MAX_REASON_LEN];       /**< Escalation reason */

    lgss_severity_t severity;           /**< Severity level */
    float risk_score;                   /**< Risk assessment [0, 1] */
    float uncertainty;                  /**< Uncertainty [0, 1] */

    uint32_t timeout_ms;                /**< Time until auto-decision */
    lgss_decision_t default_decision;   /**< Decision if timeout */

    uint64_t timestamp_us;              /**< Escalation timestamp */
} lgss_bio_action_escalated_t;

/**
 * @brief LGSS uncertainty alert message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t source_module;             /**< Module with high uncertainty */
    float uncertainty;                  /**< Uncertainty level [0, 1] */
    float threshold;                    /**< Threshold that triggered alert */

    char context[LGSS_BIO_MAX_REASON_LEN]; /**< Alert context */

    bool action_affected;               /**< Does this affect pending actions? */
    uint32_t affected_request_id;       /**< Request ID if applicable */

    uint64_t timestamp_us;              /**< Alert timestamp */
} lgss_bio_uncertainty_alert_t;

/**
 * @brief LGSS impact score message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Request ID (if applicable) */
    uint32_t source_module;             /**< Module being scored */

    float impact_score;                 /**< Computed impact [0, 1] */
    float reversibility_score;          /**< How reversible [0, 1] */
    float scope_score;                  /**< Scope of effect [0, 1] */

    char breakdown[LGSS_BIO_MAX_REASON_LEN]; /**< Score breakdown */

    uint64_t timestamp_us;              /**< Score timestamp */
} lgss_bio_impact_score_t;

/**
 * @brief LGSS risk assessment message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t assessment_id;             /**< Unique assessment ID */
    uint32_t source_module;             /**< Module being assessed */

    float overall_risk;                 /**< Overall risk [0, 1] */
    float harm_probability;             /**< Probability of harm [0, 1] */
    float harm_severity;                /**< Potential severity [0, 1] */
    float mitigation_factor;            /**< Risk reduction possible [0, 1] */

    char risk_factors[LGSS_BIO_MAX_REASON_LEN]; /**< Contributing factors */

    lgss_severity_t recommended_response; /**< Recommended response level */

    uint64_t timestamp_us;              /**< Assessment timestamp */
} lgss_bio_risk_assessment_t;

/**
 * @brief LGSS override request message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t override_id;               /**< Unique override request ID */
    uint32_t blocked_request_id;        /**< Original blocked request */
    lgss_override_level_t auth_level;   /**< Authorization level */

    char justification[LGSS_BIO_MAX_REASON_LEN]; /**< Override justification */
    char operator_id[64];               /**< Operator identifier */

    uint64_t timestamp_us;              /**< Request timestamp */
} lgss_bio_override_request_t;

/**
 * @brief LGSS override response message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t override_id;               /**< Override request ID */
    bool approved;                      /**< Override approved? */

    char reason[LGSS_BIO_MAX_REASON_LEN]; /**< Approval/denial reason */

    uint64_t timestamp_us;              /**< Response timestamp */
} lgss_bio_override_response_t;

/**
 * @brief LGSS halt command message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t command_id;                /**< Unique command ID */
    lgss_override_level_t auth_level;   /**< Authorization level */

    char reason[LGSS_BIO_MAX_REASON_LEN]; /**< Halt reason */
    char operator_id[64];               /**< Operator identifier */

    uint32_t affected_modules;          /**< Bitmask of affected modules */
    bool immediate;                     /**< Immediate halt (no graceful) */

    uint64_t timestamp_us;              /**< Command timestamp */
} lgss_bio_halt_command_t;

/**
 * @brief LGSS reset command message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t command_id;                /**< Unique command ID */
    lgss_reset_type_t reset_type;       /**< Type of reset */
    lgss_override_level_t auth_level;   /**< Authorization level */

    char reason[LGSS_BIO_MAX_REASON_LEN]; /**< Reset reason */
    char operator_id[64];               /**< Operator identifier */

    uint32_t affected_modules;          /**< Bitmask of affected modules */

    uint64_t timestamp_us;              /**< Command timestamp */
} lgss_bio_reset_command_t;

/**
 * @brief LGSS telemetry log message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t log_id;                    /**< Unique log entry ID */
    uint32_t source_module;             /**< Source module */
    lgss_severity_t severity;           /**< Log severity */

    char event_type[32];                /**< Event type identifier */
    char message[LGSS_BIO_MAX_REASON_LEN]; /**< Log message */

    float metric_value;                 /**< Optional metric value */
    bool has_metric;                    /**< Is metric_value valid? */

    uint64_t timestamp_us;              /**< Log timestamp */
} lgss_bio_telemetry_log_t;

/**
 * @brief LGSS audit request message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Unique request ID */
    lgss_override_level_t auth_level;   /**< Authorization level */

    uint64_t start_time_us;             /**< Audit period start */
    uint64_t end_time_us;               /**< Audit period end */
    uint32_t module_filter;             /**< Module ID filter (0 = all) */
    lgss_severity_t min_severity;       /**< Minimum severity filter */

    uint32_t max_entries;               /**< Maximum entries to return */

    uint64_t timestamp_us;              /**< Request timestamp */
} lgss_bio_audit_request_t;

/**
 * @brief LGSS audit response message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t request_id;                /**< Original request ID */
    uint32_t total_entries;             /**< Total matching entries */
    uint32_t returned_entries;          /**< Entries in this response */
    bool has_more;                      /**< More entries available? */

    /* Entries are serialized separately in the payload */
    uint32_t entries_offset;            /**< Offset to entries in payload */

    uint64_t timestamp_us;              /**< Response timestamp */
} lgss_bio_audit_response_t;

/**
 * @brief LGSS integrity check message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t check_id;                  /**< Unique check ID */
    uint32_t target_module;             /**< Module to check (0 = all) */
    bool full_check;                    /**< Full vs quick check */

    uint64_t timestamp_us;              /**< Request timestamp */
} lgss_bio_integrity_check_t;

/**
 * @brief LGSS integrity result message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t check_id;                  /**< Original check ID */
    lgss_integrity_status_t status;     /**< Overall status */
    uint32_t modules_checked;           /**< Number of modules checked */
    uint32_t modules_passed;            /**< Modules that passed */
    uint32_t modules_failed;            /**< Modules that failed */

    char details[LGSS_BIO_MAX_REASON_LEN]; /**< Result details */

    uint64_t timestamp_us;              /**< Result timestamp */
} lgss_bio_integrity_result_t;

/**
 * @brief LGSS tampering detected message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t detection_id;              /**< Unique detection ID */
    uint32_t affected_module;           /**< Module where tampering found */
    lgss_integrity_status_t status;     /**< Type of tampering */

    char details[LGSS_BIO_MAX_REASON_LEN]; /**< Detection details */
    char evidence[LGSS_BIO_MAX_REASON_LEN]; /**< Evidence summary */

    bool system_halted;                 /**< Was system halted? */

    uint64_t timestamp_us;              /**< Detection timestamp */
} lgss_bio_tampering_detected_t;

/**
 * @brief LGSS safety event message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    uint32_t event_id;                  /**< Unique event ID */
    uint32_t source_module;             /**< Source module */
    lgss_severity_t severity;           /**< Event severity */

    char event_type[32];                /**< Event type identifier */
    char description[LGSS_BIO_MAX_REASON_LEN]; /**< Event description */

    float safety_score_delta;           /**< Change in safety score */
    bool learning_affected;             /**< Does this affect plasticity? */

    uint64_t timestamp_us;              /**< Event timestamp */
} lgss_bio_safety_event_t;

/**
 * @brief LGSS neuromodulator signal message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    nimcp_bio_channel_type_t channel;   /**< Neuromodulator channel */
    float signal_strength;              /**< Signal strength [0, 1] */
    float safety_modulation;            /**< Safety-based modulation factor */

    char reason[LGSS_BIO_MAX_REASON_LEN]; /**< Modulation reason */

    bool suppress_plasticity;           /**< Suppress learning? */
    float plasticity_factor;            /**< Plasticity scale factor */

    uint64_t timestamp_us;              /**< Signal timestamp */
} lgss_bio_neuromod_signal_t;

/**
 * @brief LGSS external heartbeat message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    char external_system_id[64];        /**< External system identifier */
    bool healthy;                       /**< External system healthy? */
    uint64_t sequence_num;              /**< Heartbeat sequence number */

    float connection_quality;           /**< Connection quality [0, 1] */
    uint32_t latency_ms;                /**< Round-trip latency */

    uint64_t timestamp_us;              /**< Heartbeat timestamp */
} lgss_bio_external_heartbeat_t;

/**
 * @brief LGSS external attestation message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    char external_system_id[64];        /**< External system identifier */
    bool attestation_valid;             /**< Attestation passed? */

    uint8_t attestation_data[256];      /**< Attestation data */
    uint32_t attestation_size;          /**< Attestation data size */

    char details[LGSS_BIO_MAX_REASON_LEN]; /**< Attestation details */

    uint64_t timestamp_us;              /**< Attestation timestamp */
} lgss_bio_external_attestation_t;

/**
 * @brief LGSS external command message payload
 */
typedef struct {
    bio_message_header_t header;        /**< Standard bio-async header */

    char external_system_id[64];        /**< External system identifier */
    uint32_t command_type;              /**< Command type identifier */
    lgss_override_level_t auth_level;   /**< Authorization level */

    uint8_t command_data[256];          /**< Command data */
    uint32_t command_size;              /**< Command data size */

    uint64_t timestamp_us;              /**< Command timestamp */
} lgss_bio_external_command_t;

/* ============================================================================
 * Handler Callback Types
 * ============================================================================ */

/**
 * @brief Callback for handling evaluation requests
 */
typedef lgss_decision_t (*lgss_evaluate_handler_t)(
    const lgss_bio_evaluate_request_t* request,
    lgss_bio_evaluate_response_t* response,
    void* user_data
);

/**
 * @brief Callback for handling override requests
 */
typedef bool (*lgss_override_handler_t)(
    const lgss_bio_override_request_t* request,
    lgss_bio_override_response_t* response,
    void* user_data
);

/**
 * @brief Callback for handling control commands
 */
typedef int (*lgss_control_handler_t)(
    bio_message_type_t msg_type,
    const void* command,
    size_t command_size,
    void* user_data
);

/**
 * @brief Callback for handling audit requests
 */
typedef int (*lgss_audit_handler_t)(
    const lgss_bio_audit_request_t* request,
    lgss_bio_audit_response_t* response,
    void* audit_buffer,
    size_t buffer_size,
    void* user_data
);

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

/**
 * @brief LGSS bio-async bridge configuration
 */
typedef struct {
    /* Timing settings */
    uint32_t evaluation_timeout_ms;     /**< Default evaluation timeout */
    uint32_t message_ttl_ms;            /**< Message time-to-live */
    uint32_t urgent_timeout_ms;         /**< Urgent message timeout */
    uint32_t heartbeat_interval_ms;     /**< Heartbeat broadcast interval */

    /* Message handling */
    uint32_t max_inbox_process_per_update; /**< Max inbox messages per update */
    uint32_t max_handlers;              /**< Maximum registered handlers */

    /* Channel settings */
    nimcp_bio_channel_type_t default_channel;   /**< Default channel */
    nimcp_bio_channel_type_t alert_channel;     /**< Channel for alerts */
    nimcp_bio_channel_type_t urgent_channel;    /**< Channel for urgent */

    /* Policy settings */
    float uncertainty_alert_threshold;  /**< Threshold for uncertainty alerts */
    float risk_escalation_threshold;    /**< Threshold for risk escalation */
    bool auto_escalate_high_risk;       /**< Auto-escalate high-risk actions */
    bool block_on_evaluation_timeout;   /**< Block if evaluation times out */

    /* Feature flags */
    bool enable_telemetry;              /**< Enable telemetry logging */
    bool enable_integrity_monitoring;   /**< Enable integrity checks */
    bool enable_external_interface;     /**< Enable external system interface */
    bool enable_plasticity_gating;      /**< Enable plasticity coordination */
    bool enable_logging;                /**< Enable message logging */
} lgss_bio_bridge_config_t;

/* ============================================================================
 * Bridge Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Message counts */
    uint64_t messages_sent;             /**< Total messages sent */
    uint64_t messages_received;         /**< Total messages received */
    uint64_t messages_dropped;          /**< Messages dropped (queue full) */
    uint64_t broadcasts_sent;           /**< Broadcast messages sent */

    /* Evaluation stats */
    uint64_t evaluations_requested;     /**< Evaluation requests received */
    uint64_t evaluations_allowed;       /**< Actions allowed */
    uint64_t evaluations_blocked;       /**< Actions blocked */
    uint64_t evaluations_escalated;     /**< Actions escalated */
    uint64_t evaluations_timeout;       /**< Evaluations timed out */

    /* Override stats */
    uint64_t overrides_requested;       /**< Override requests */
    uint64_t overrides_approved;        /**< Overrides approved */
    uint64_t overrides_denied;          /**< Overrides denied */

    /* Integrity stats */
    uint64_t integrity_checks;          /**< Integrity checks performed */
    uint64_t integrity_failures;        /**< Integrity check failures */
    uint64_t tampering_detected;        /**< Tampering detections */

    /* Timing stats */
    float avg_evaluation_time_us;       /**< Average evaluation time */
    float max_evaluation_time_us;       /**< Peak evaluation time */
    uint64_t last_heartbeat_us;         /**< Last heartbeat timestamp */

    /* Error counts */
    uint64_t handler_errors;            /**< Message handler errors */
    uint64_t routing_errors;            /**< Routing failures */
} lgss_bio_bridge_stats_t;

/* ============================================================================
 * Bridge Structure (Opaque Handle)
 * ============================================================================ */

/**
 * @brief LGSS bio-async bridge handle
 */
typedef struct lgss_bio_bridge_struct lgss_bio_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for LGSS bio-async bridge configuration
 * WHY:  Easy initialization with safety-appropriate parameters
 * HOW:  Return struct with conservative safety defaults
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_default_config(lgss_bio_bridge_config_t* config);

/**
 * @brief Create LGSS bio-async bridge
 *
 * WHAT: Initialize bio-async integration for LGSS
 * WHY:  Enable message routing between LGSS and all modules
 * HOW:  Allocate structure, initialize handler registry, prepare handlers
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
lgss_bio_bridge_t* lgss_bio_bridge_create(
    const lgss_bio_bridge_config_t* config
);

/**
 * @brief Destroy LGSS bio-async bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect from router, free handlers, release memory
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void lgss_bio_bridge_destroy(lgss_bio_bridge_t* bridge);

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
int lgss_bio_bridge_connect(
    lgss_bio_bridge_t* bridge,
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
int lgss_bio_bridge_disconnect(lgss_bio_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Bio-async bridge
 * @return true if connected to router
 */
bool lgss_bio_bridge_is_connected(const lgss_bio_bridge_t* bridge);

/* ============================================================================
 * Handler Registration API
 * ============================================================================ */

/**
 * @brief Register evaluation handler
 *
 * WHAT: Register callback for handling evaluation requests
 * WHY:  Allow custom evaluation logic
 * HOW:  Store handler in registry, invoke on requests
 *
 * @param bridge Bio-async bridge
 * @param handler Evaluation handler callback
 * @param user_data User data passed to handler
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_register_evaluate_handler(
    lgss_bio_bridge_t* bridge,
    lgss_evaluate_handler_t handler,
    void* user_data
);

/**
 * @brief Register override handler
 *
 * WHAT: Register callback for handling override requests
 * WHY:  Allow custom override authorization logic
 * HOW:  Store handler in registry, invoke on requests
 *
 * @param bridge Bio-async bridge
 * @param handler Override handler callback
 * @param user_data User data passed to handler
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_register_override_handler(
    lgss_bio_bridge_t* bridge,
    lgss_override_handler_t handler,
    void* user_data
);

/**
 * @brief Register control command handler
 *
 * WHAT: Register callback for handling control commands (halt, reset)
 * WHY:  Allow custom control command processing
 * HOW:  Store handler in registry, invoke on commands
 *
 * @param bridge Bio-async bridge
 * @param handler Control handler callback
 * @param user_data User data passed to handler
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_register_control_handler(
    lgss_bio_bridge_t* bridge,
    lgss_control_handler_t handler,
    void* user_data
);

/**
 * @brief Register audit handler
 *
 * WHAT: Register callback for handling audit requests
 * WHY:  Allow custom audit data retrieval
 * HOW:  Store handler in registry, invoke on requests
 *
 * @param bridge Bio-async bridge
 * @param handler Audit handler callback
 * @param user_data User data passed to handler
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_register_audit_handler(
    lgss_bio_bridge_t* bridge,
    lgss_audit_handler_t handler,
    void* user_data
);

/* ============================================================================
 * Message Processing API
 * ============================================================================ */

/**
 * @brief Process incoming messages from inbox
 *
 * WHAT: Process pending messages from bio-router inbox
 * WHY:  Handle incoming LGSS requests
 * HOW:  Pop messages, dispatch to appropriate handlers, update state
 *
 * @param bridge Bio-async bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed, -1 on error
 */
int lgss_bio_bridge_process_inbox(
    lgss_bio_bridge_t* bridge,
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
int lgss_bio_bridge_update(
    lgss_bio_bridge_t* bridge,
    uint32_t delta_ms
);

/**
 * @brief Handle incoming bio message
 *
 * WHAT: Process a single bio-async message
 * WHY:  Entry point for message handling
 * HOW:  Dispatch to appropriate handler based on message type
 *
 * @param bridge Bio-async bridge
 * @param msg Message to handle
 * @param msg_size Message size
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_handle_message(
    lgss_bio_bridge_t* bridge,
    const void* msg,
    size_t msg_size
);

/* ============================================================================
 * Evaluation API
 * ============================================================================ */

/**
 * @brief Send evaluation request
 *
 * WHAT: Request LGSS evaluation of an action
 * WHY:  Pre-emptive safety check before execution
 * HOW:  Package request, send via bio-router
 *
 * @param bridge Bio-async bridge
 * @param action_type Type of action
 * @param action_desc Action description
 * @param uncertainty Requester's uncertainty [0, 1]
 * @param impact_estimate Estimated impact [0, 1]
 * @param reversible Is action reversible?
 * @param request_id Output: unique request ID
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_send_evaluate_request(
    lgss_bio_bridge_t* bridge,
    uint32_t action_type,
    const char* action_desc,
    float uncertainty,
    float impact_estimate,
    bool reversible,
    uint32_t* request_id
);

/**
 * @brief Send evaluation response
 *
 * WHAT: Respond to an evaluation request
 * WHY:  Complete evaluation flow
 * HOW:  Package response, send to requester
 *
 * @param bridge Bio-async bridge
 * @param request_id Original request ID
 * @param decision Evaluation decision
 * @param confidence Decision confidence [0, 1]
 * @param risk_score Computed risk [0, 1]
 * @param policy_name Policy applied
 * @param reason Reason for decision
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_send_evaluate_response(
    lgss_bio_bridge_t* bridge,
    uint32_t request_id,
    lgss_decision_t decision,
    float confidence,
    float risk_score,
    const char* policy_name,
    const char* reason
);

/* ============================================================================
 * Violation Notification API
 * ============================================================================ */

/**
 * @brief Broadcast policy violation
 *
 * WHAT: Notify all modules of a policy violation
 * WHY:  System-wide awareness of violations
 * HOW:  Package violation info, broadcast on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param source_module Module that violated
 * @param severity Violation severity
 * @param policy_name Violated policy
 * @param action_desc Action that violated
 * @param details Violation details
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_violation(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* policy_name,
    const char* action_desc,
    const char* details
);

/**
 * @brief Broadcast action blocked
 *
 * WHAT: Notify that an action was blocked
 * WHY:  Inform requesting module and observers
 * HOW:  Package block info, broadcast
 *
 * @param bridge Bio-async bridge
 * @param request_id Original request ID (if applicable)
 * @param source_module Module whose action was blocked
 * @param action_desc Blocked action
 * @param policy_name Blocking policy
 * @param reason Block reason
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_blocked(
    lgss_bio_bridge_t* bridge,
    uint32_t request_id,
    uint32_t source_module,
    const char* action_desc,
    const char* policy_name,
    const char* reason
);

/**
 * @brief Broadcast action escalated
 *
 * WHAT: Notify that an action was escalated for human review
 * WHY:  Alert human operators
 * HOW:  Package escalation info, broadcast on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param source_module Module whose action was escalated
 * @param action_desc Escalated action
 * @param reason Escalation reason
 * @param severity Severity level
 * @param risk_score Risk assessment
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_escalated(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    const char* action_desc,
    const char* reason,
    lgss_severity_t severity,
    float risk_score
);

/* ============================================================================
 * Risk/Uncertainty API
 * ============================================================================ */

/**
 * @brief Broadcast uncertainty alert
 *
 * WHAT: Alert that uncertainty threshold was exceeded
 * WHY:  High uncertainty may require caution
 * HOW:  Package alert, broadcast on alert channel
 *
 * @param bridge Bio-async bridge
 * @param source_module Module with high uncertainty
 * @param uncertainty Uncertainty level [0, 1]
 * @param context Alert context
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_uncertainty_alert(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    float uncertainty,
    const char* context
);

/**
 * @brief Broadcast risk assessment
 *
 * WHAT: Broadcast risk assessment results
 * WHY:  Inform system of computed risks
 * HOW:  Package assessment, broadcast
 *
 * @param bridge Bio-async bridge
 * @param source_module Module being assessed
 * @param overall_risk Overall risk [0, 1]
 * @param harm_probability Probability of harm [0, 1]
 * @param harm_severity Potential severity [0, 1]
 * @param risk_factors Contributing factors description
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_risk_assessment(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    float overall_risk,
    float harm_probability,
    float harm_severity,
    const char* risk_factors
);

/* ============================================================================
 * Integrity API
 * ============================================================================ */

/**
 * @brief Request integrity check
 *
 * WHAT: Request integrity verification of modules
 * WHY:  Detect tampering or corruption
 * HOW:  Send integrity check request
 *
 * @param bridge Bio-async bridge
 * @param target_module Module to check (0 = all)
 * @param full_check Full vs quick check
 * @param check_id Output: unique check ID
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_request_integrity_check(
    lgss_bio_bridge_t* bridge,
    uint32_t target_module,
    bool full_check,
    uint32_t* check_id
);

/**
 * @brief Broadcast tampering detection
 *
 * WHAT: Alert that tampering was detected
 * WHY:  Critical security event requiring response
 * HOW:  Package detection info, broadcast on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param affected_module Module where tampering found
 * @param status Type of tampering
 * @param details Detection details
 * @param halt_system Should system be halted?
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_tampering(
    lgss_bio_bridge_t* bridge,
    uint32_t affected_module,
    lgss_integrity_status_t status,
    const char* details,
    bool halt_system
);

/* ============================================================================
 * Control API
 * ============================================================================ */

/**
 * @brief Send halt command
 *
 * WHAT: Send emergency halt command
 * WHY:  Stop system operations immediately
 * HOW:  Broadcast halt command on urgent channel
 *
 * @param bridge Bio-async bridge
 * @param auth_level Authorization level
 * @param reason Halt reason
 * @param operator_id Operator identifier
 * @param immediate Immediate halt (no graceful shutdown)
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_send_halt(
    lgss_bio_bridge_t* bridge,
    lgss_override_level_t auth_level,
    const char* reason,
    const char* operator_id,
    bool immediate
);

/**
 * @brief Send reset command
 *
 * WHAT: Send reset command
 * WHY:  Reset system or modules to known state
 * HOW:  Broadcast reset command
 *
 * @param bridge Bio-async bridge
 * @param reset_type Type of reset
 * @param auth_level Authorization level
 * @param reason Reset reason
 * @param operator_id Operator identifier
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_send_reset(
    lgss_bio_bridge_t* bridge,
    lgss_reset_type_t reset_type,
    lgss_override_level_t auth_level,
    const char* reason,
    const char* operator_id
);

/* ============================================================================
 * Plasticity Coordination API
 * ============================================================================ */

/**
 * @brief Broadcast safety event
 *
 * WHAT: Notify modules of safety-relevant event
 * WHY:  Coordinate safety-aware plasticity
 * HOW:  Package event, broadcast to plasticity modules
 *
 * @param bridge Bio-async bridge
 * @param source_module Source of event
 * @param severity Event severity
 * @param event_type Event type identifier
 * @param description Event description
 * @param learning_affected Does this affect plasticity?
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_broadcast_safety_event(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* event_type,
    const char* description,
    bool learning_affected
);

/**
 * @brief Send neuromodulator safety signal
 *
 * WHAT: Modulate neuromodulator channels for safety
 * WHY:  Gate plasticity based on safety state
 * HOW:  Package signal, send to neuromodulator modules
 *
 * @param bridge Bio-async bridge
 * @param channel Neuromodulator channel
 * @param signal_strength Signal strength [0, 1]
 * @param safety_modulation Safety modulation factor
 * @param suppress_plasticity Should plasticity be suppressed?
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_send_neuromod_signal(
    lgss_bio_bridge_t* bridge,
    nimcp_bio_channel_type_t channel,
    float signal_strength,
    float safety_modulation,
    bool suppress_plasticity
);

/* ============================================================================
 * Telemetry API
 * ============================================================================ */

/**
 * @brief Log telemetry entry
 *
 * WHAT: Log telemetry event
 * WHY:  Maintain audit trail
 * HOW:  Package log entry, store and optionally broadcast
 *
 * @param bridge Bio-async bridge
 * @param source_module Source module
 * @param severity Log severity
 * @param event_type Event type identifier
 * @param message Log message
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_log_telemetry(
    lgss_bio_bridge_t* bridge,
    uint32_t source_module,
    lgss_severity_t severity,
    const char* event_type,
    const char* message
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
int lgss_bio_bridge_get_stats(
    const lgss_bio_bridge_t* bridge,
    lgss_bio_bridge_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bio-async bridge
 * @return 0 on success, -1 on error
 */
int lgss_bio_bridge_reset_stats(lgss_bio_bridge_t* bridge);

/**
 * @brief Get decision name
 *
 * @param decision LGSS decision
 * @return Human-readable name string
 */
const char* lgss_decision_name(lgss_decision_t decision);

/**
 * @brief Get severity name
 *
 * @param severity LGSS severity
 * @return Human-readable name string
 */
const char* lgss_severity_name(lgss_severity_t severity);

/**
 * @brief Get override level name
 *
 * @param level Override level
 * @return Human-readable name string
 */
const char* lgss_override_level_name(lgss_override_level_t level);

/**
 * @brief Get integrity status name
 *
 * @param status Integrity status
 * @return Human-readable name string
 */
const char* lgss_integrity_status_name(lgss_integrity_status_t status);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge Bio-async bridge (NULL-safe)
 */
void lgss_bio_bridge_print_summary(const lgss_bio_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_BIO_ASYNC_BRIDGE_H */
