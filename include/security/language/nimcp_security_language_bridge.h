/**
 * @file nimcp_security_language_bridge.h
 * @brief Security-Language Bridge for Input Sanitization and Injection Detection
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Bidirectional bridge connecting security systems with language processing
 * WHY:  Enable input sanitization, prompt injection detection, content filtering,
 *       and policy-based validation of language processing pipelines
 * HOW:  Security gates language input/output; language provides context for detection
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 * Modeled on the brain's protective mechanisms for language processing:
 * - Blood-Brain Barrier (BBB) filters harmful inputs before reaching language areas
 * - Thalamic gating controls what linguistic information reaches cortical processing
 * - Prefrontal executive function monitors for inappropriate content
 * - Semantic memory provides context for detecting anomalous language patterns
 *
 * ARCHITECTURE:
 * ==================================================================================
 * ```
 * +===========================================================================+
 * |     SECURITY-LANGUAGE BRIDGE (Sanitization & Injection Detection)         |
 * +===========================================================================+
 * |                                                                           |
 * |   SECURITY SYSTEMS                          LANGUAGE SYSTEMS              |
 * |   +-------------------+                    +-------------------+          |
 * |   | Blood-Brain       |                    | Language          |          |
 * |   | Barrier (BBB)     |----+          +--->| Orchestrator      |          |
 * |   +-------------------+    |          |    +-------------------+          |
 * |   +-------------------+    |    BI    |    +-------------------+          |
 * |   | Pattern Database  |----+    DI    +--->| Wernicke          |          |
 * |   | (Injection Sigs)  |    |    RE    |    | (Comprehension)   |          |
 * |   +-------------------+    |    CT    |    +-------------------+          |
 * |   +-------------------+    |    IO    |    +-------------------+          |
 * |   | Policy Engine     |----+----NA----+--->| Broca             |          |
 * |   +-------------------+         L          | (Production)      |          |
 * |                                 |          +-------------------+          |
 * |                                 v                                         |
 * |                    +------------------------+                             |
 * |                    | Security Analysis      |                             |
 * |                    | - Injection detection  |                             |
 * |                    | - Content filtering    |                             |
 * |                    | - Threat scoring       |                             |
 * |                    +------------------------+                             |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DATA FLOW:
 * ==================================================================================
 *
 * Security -> Language (Protection):
 * - Blocked pattern signatures for input validation
 * - Sanitization rules for dangerous content removal
 * - Content policy restrictions for generation
 * - Threat level modulation of language processing
 *
 * Language -> Security (Context & Detection):
 * - Detected injection attempts from parsed input
 * - Suspicious pattern alerts from language analysis
 * - Semantic anomalies indicating adversarial input
 * - Production content for output validation
 *
 * INJECTION TYPES DETECTED:
 * ==================================================================================
 * - PROMPT_INJECTION:  LLM prompt manipulation attempts
 * - SQL_INJECTION:     Database query injection
 * - CODE_INJECTION:    Executable code insertion
 * - SHELL_INJECTION:   OS command injection
 * - XSS_INJECTION:     Cross-site scripting patterns
 * - TEMPLATE_INJECTION: Template engine exploitation
 * - LDAP_INJECTION:    Directory service injection
 * - XML_INJECTION:     XML/XXE injection patterns
 *
 * FEATURES:
 * ==================================================================================
 * - Multi-layer injection detection with confidence scoring
 * - Configurable sanitization with escape/removal options
 * - Content policy validation for generated output
 * - Real-time threat scoring for input streams
 * - Bi-async integration for distributed security events
 * - Pattern database integration for signature matching
 * - Policy engine integration for rule-based filtering
 *
 * THREAD SAFETY:
 * ==================================================================================
 * - All public functions are thread-safe via internal mutex
 * - Lock-free fast path for sanitization operations
 * - Atomic counters for statistics
 *
 * PERFORMANCE:
 * ==================================================================================
 * - Sanitization: O(n) where n = input length
 * - Injection detection: O(n * p) where p = active patterns
 * - Content policy check: O(r) where r = active rules
 * - Memory: ~16KB base + pattern cache (configurable)
 *
 * @author NIMCP Security Team
 */

#ifndef NIMCP_SECURITY_LANGUAGE_BRIDGE_H
#define NIMCP_SECURITY_LANGUAGE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base infrastructure - MUST come first for struct embedding */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security system dependencies */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_policy_engine.h"

/* Language system dependencies */
#include "language/nimcp_language_orchestrator.h"
#include "language/nimcp_language_types.h"

/* Async integration */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** @brief Magic number for validation */
#define SECURITY_LANGUAGE_BRIDGE_MAGIC       0x53454C41  /* 'SELA' */

/** @brief Bridge version */
#define SECURITY_LANGUAGE_BRIDGE_VERSION     1

/** @brief Maximum input length for sanitization */
#define SECURITY_LANGUAGE_MAX_INPUT_LEN      65536

/** @brief Maximum sanitized output length */
#define SECURITY_LANGUAGE_MAX_OUTPUT_LEN     65536

/** @brief Maximum pattern count per detection */
#define SECURITY_LANGUAGE_MAX_PATTERNS       64

/** @brief Maximum detected injections per scan */
#define SECURITY_LANGUAGE_MAX_DETECTIONS     32

/** @brief Maximum policy violations per check */
#define SECURITY_LANGUAGE_MAX_VIOLATIONS     16

/** @brief Default threat score threshold */
#define SECURITY_LANGUAGE_DEFAULT_THRESHOLD  0.7f

/** @brief Module ID for bio-async registration */
#define BIO_MODULE_SECURITY_LANGUAGE         0x00008600

/*=============================================================================
 * INJECTION TYPE ENUMERATION
 *============================================================================*/

/**
 * @brief Types of injection attacks detected
 *
 * WHAT: Classification of injection attack vectors
 * WHY:  Enable targeted detection and response strategies
 * HOW:  Pattern matching and heuristic analysis per type
 */
typedef enum {
    INJECTION_TYPE_NONE = 0,            /**< No injection detected */
    INJECTION_TYPE_PROMPT,              /**< LLM prompt injection */
    INJECTION_TYPE_SQL,                 /**< SQL injection */
    INJECTION_TYPE_CODE,                /**< Code injection (eval, exec) */
    INJECTION_TYPE_SHELL,               /**< Shell command injection */
    INJECTION_TYPE_XSS,                 /**< Cross-site scripting */
    INJECTION_TYPE_TEMPLATE,            /**< Template engine injection */
    INJECTION_TYPE_LDAP,                /**< LDAP injection */
    INJECTION_TYPE_XML,                 /**< XML/XXE injection */
    INJECTION_TYPE_PATH_TRAVERSAL,      /**< Path traversal (../) */
    INJECTION_TYPE_FORMAT_STRING,       /**< Format string attack */
    INJECTION_TYPE_HEADER,              /**< HTTP header injection */
    INJECTION_TYPE_CUSTOM,              /**< Custom/unknown type */
    INJECTION_TYPE_COUNT                /**< Number of injection types */
} security_language_injection_type_t;

/**
 * @brief Sanitization action types
 *
 * WHAT: Actions taken during sanitization
 * WHY:  Track what modifications were made to input
 */
typedef enum {
    SANITIZE_ACTION_NONE = 0,           /**< No action needed */
    SANITIZE_ACTION_ESCAPE,             /**< Characters escaped */
    SANITIZE_ACTION_REMOVE,             /**< Content removed */
    SANITIZE_ACTION_REPLACE,            /**< Content replaced */
    SANITIZE_ACTION_TRUNCATE,           /**< Input truncated */
    SANITIZE_ACTION_BLOCK,              /**< Input blocked entirely */
    SANITIZE_ACTION_COUNT               /**< Number of action types */
} security_language_sanitize_action_t;

/**
 * @brief Content policy violation types
 *
 * WHAT: Types of content policy violations
 * WHY:  Categorize policy enforcement decisions
 */
typedef enum {
    POLICY_VIOLATION_NONE = 0,          /**< No violation */
    POLICY_VIOLATION_HARMFUL,           /**< Harmful content */
    POLICY_VIOLATION_EXPLICIT,          /**< Explicit/adult content */
    POLICY_VIOLATION_HATE,              /**< Hate speech */
    POLICY_VIOLATION_VIOLENCE,          /**< Violence/threats */
    POLICY_VIOLATION_PII,               /**< Personal information */
    POLICY_VIOLATION_CONFIDENTIAL,      /**< Confidential data */
    POLICY_VIOLATION_COPYRIGHTED,       /**< Copyrighted content */
    POLICY_VIOLATION_CUSTOM,            /**< Custom policy violation */
    POLICY_VIOLATION_COUNT              /**< Number of violation types */
} security_language_policy_violation_t;

/**
 * @brief Threat severity levels
 *
 * WHAT: Severity classification for detected threats
 * WHY:  Prioritize response actions
 */
typedef enum {
    THREAT_SEVERITY_NONE = 0,           /**< No threat */
    THREAT_SEVERITY_LOW,                /**< Low - log only */
    THREAT_SEVERITY_MEDIUM,             /**< Medium - sanitize */
    THREAT_SEVERITY_HIGH,               /**< High - block and alert */
    THREAT_SEVERITY_CRITICAL,           /**< Critical - immediate action */
    THREAT_SEVERITY_COUNT               /**< Number of severity levels */
} security_language_threat_severity_t;

/*=============================================================================
 * DETECTION AND RESULT STRUCTURES
 *============================================================================*/

/**
 * @brief Single injection detection result
 *
 * WHAT: Details of a detected injection attempt
 * WHY:  Provide actionable information for response
 */
typedef struct {
    security_language_injection_type_t type;    /**< Injection type */
    security_language_threat_severity_t severity; /**< Threat severity */
    float confidence;                           /**< Detection confidence [0.0, 1.0] */
    size_t offset;                              /**< Position in input */
    size_t length;                              /**< Length of suspicious segment */
    char pattern_matched[128];                  /**< Pattern that matched */
    char description[256];                      /**< Human-readable description */
    nimcp_pattern_id_t pattern_id;              /**< Pattern database ID */
} security_language_detection_t;

/**
 * @brief Injection detection result set
 *
 * WHAT: Complete results from injection detection scan
 * WHY:  Aggregate all detections from single scan
 */
typedef struct {
    bool injection_detected;                    /**< Any injection found */
    uint32_t detection_count;                   /**< Number of detections */
    security_language_detection_t detections[SECURITY_LANGUAGE_MAX_DETECTIONS];
    float aggregate_threat_score;               /**< Combined threat score */
    security_language_threat_severity_t max_severity; /**< Highest severity found */
    uint64_t scan_time_ns;                      /**< Time taken to scan */
} security_language_detection_result_t;

/**
 * @brief Sanitization result
 *
 * WHAT: Result of input sanitization operation
 * WHY:  Provide sanitized content and change details
 */
typedef struct {
    bool modified;                              /**< Input was modified */
    bool blocked;                               /**< Input was blocked */
    uint32_t changes_made;                      /**< Number of changes */
    security_language_sanitize_action_t actions[16]; /**< Actions taken */
    uint32_t action_count;                      /**< Number of actions */
    char* sanitized_output;                     /**< Sanitized content (caller frees) */
    size_t sanitized_length;                    /**< Length of sanitized output */
    char warning_message[256];                  /**< Warning if applicable */
    uint64_t processing_time_ns;                /**< Time taken */
} security_language_sanitize_result_t;

/**
 * @brief Content policy check result
 *
 * WHAT: Result of content policy validation
 * WHY:  Determine if content passes policy requirements
 */
typedef struct {
    bool passed;                                /**< Content passed policy */
    uint32_t violation_count;                   /**< Number of violations */
    security_language_policy_violation_t violations[SECURITY_LANGUAGE_MAX_VIOLATIONS];
    float violation_scores[SECURITY_LANGUAGE_MAX_VIOLATIONS]; /**< Per-violation scores */
    char policy_ids[SECURITY_LANGUAGE_MAX_VIOLATIONS][64];    /**< Violated policy IDs */
    float aggregate_score;                      /**< Aggregate violation score */
    char recommendation[256];                   /**< Suggested action */
    uint64_t check_time_ns;                     /**< Time taken */
} security_language_policy_result_t;

/**
 * @brief Output validation result
 *
 * WHAT: Result of generated output validation
 * WHY:  Ensure generated content is safe for delivery
 */
typedef struct {
    bool valid;                                 /**< Output is valid */
    bool requires_modification;                 /**< Output needs changes */
    security_language_policy_result_t policy_result; /**< Policy check result */
    security_language_detection_result_t injection_result; /**< Injection check */
    char modified_output[SECURITY_LANGUAGE_MAX_OUTPUT_LEN]; /**< Modified output if needed */
    size_t modified_length;                     /**< Modified output length */
    float safety_score;                         /**< Overall safety score [0.0, 1.0] */
} security_language_output_validation_t;

/**
 * @brief Threat score result
 *
 * WHAT: Comprehensive threat assessment for input
 * WHY:  Single metric for security decision making
 */
typedef struct {
    float overall_score;                        /**< Overall threat score [0.0, 1.0] */
    float injection_score;                      /**< Injection threat component */
    float pattern_score;                        /**< Pattern match component */
    float anomaly_score;                        /**< Anomaly detection component */
    float policy_score;                         /**< Policy violation component */
    security_language_threat_severity_t severity; /**< Computed severity */
    bool requires_action;                       /**< Score exceeds threshold */
    char summary[256];                          /**< Threat summary */
} security_language_threat_score_t;

/*=============================================================================
 * BIDIRECTIONAL EFFECTS STRUCTURES
 *============================================================================*/

/**
 * @brief Effects from security system to language processing
 *
 * WHAT: Security constraints applied to language processing
 * WHY:  Track security -> language influence
 */
typedef struct {
    /* Pattern blocking */
    uint32_t blocked_pattern_count;             /**< Number of blocked patterns */
    nimcp_pattern_id_t blocked_patterns[SECURITY_LANGUAGE_MAX_PATTERNS]; /**< Blocked IDs */

    /* Sanitization level */
    float sanitization_level;                   /**< Current sanitization intensity [0.0, 1.0] */
    bool aggressive_sanitization;               /**< Aggressive mode enabled */

    /* Processing modulation */
    float processing_inhibition;                /**< Inhibition of language processing [0.0, 1.0] */
    bool block_external_input;                  /**< Block external language input */
    bool restrict_generation;                   /**< Restrict language generation */

    /* Threat state */
    security_language_threat_severity_t current_threat_level; /**< Current threat level */
    uint64_t threat_events_pending;             /**< Pending threat events */
} security_to_language_effects_t;

/**
 * @brief Effects from language processing to security
 *
 * WHAT: Security signals from language analysis
 * WHY:  Track language -> security feedback
 */
typedef struct {
    /* Detected threats */
    uint32_t detected_injection_count;          /**< Injections detected this cycle */
    security_language_injection_type_t detected_types[INJECTION_TYPE_COUNT]; /**< Types seen */
    uint32_t type_counts[INJECTION_TYPE_COUNT]; /**< Count per type */

    /* Suspicious patterns */
    uint32_t suspicious_pattern_count;          /**< Suspicious patterns found */
    float max_pattern_confidence;               /**< Highest pattern confidence */

    /* Semantic analysis */
    float semantic_anomaly_score;               /**< Semantic anomaly detection */
    bool context_manipulation_detected;         /**< Context manipulation attempt */

    /* Statistics */
    uint64_t inputs_scanned;                    /**< Inputs scanned this cycle */
    uint64_t inputs_blocked;                    /**< Inputs blocked this cycle */
    uint64_t outputs_validated;                 /**< Outputs validated this cycle */
    float avg_threat_score;                     /**< Average threat score */
} language_to_security_effects_t;

/*=============================================================================
 * CONFIGURATION STRUCTURES
 *============================================================================*/

/**
 * @brief Sanitization configuration
 *
 * WHAT: Settings for input sanitization behavior
 */
typedef struct {
    bool enable_html_sanitization;              /**< Sanitize HTML tags */
    bool enable_sql_sanitization;               /**< Sanitize SQL metacharacters */
    bool enable_shell_sanitization;             /**< Sanitize shell metacharacters */
    bool enable_unicode_normalization;          /**< Normalize Unicode */
    bool strip_control_characters;              /**< Remove control chars */
    bool escape_special_characters;             /**< Escape vs remove */
    size_t max_input_length;                    /**< Maximum allowed length */
    float sanitization_intensity;               /**< Intensity [0.0, 1.0] */
} security_language_sanitize_config_t;

/**
 * @brief Injection detection configuration
 */
typedef struct {
    bool enable_prompt_injection_detection;     /**< Detect prompt injection */
    bool enable_sql_injection_detection;        /**< Detect SQL injection */
    bool enable_code_injection_detection;       /**< Detect code injection */
    bool enable_shell_injection_detection;      /**< Detect shell injection */
    bool enable_xss_detection;                  /**< Detect XSS */
    bool enable_heuristic_detection;            /**< Use heuristics */
    float confidence_threshold;                 /**< Detection threshold [0.0, 1.0] */
    uint32_t max_detections;                    /**< Max detections to return */
    bool use_pattern_database;                  /**< Use pattern DB signatures */
} security_language_detection_config_t;

/**
 * @brief Content policy configuration
 */
typedef struct {
    bool enable_content_filtering;              /**< Enable content filter */
    bool block_harmful_content;                 /**< Block harmful content */
    bool block_explicit_content;                /**< Block explicit content */
    bool block_pii;                             /**< Block personal info */
    bool enable_custom_policies;                /**< Enable custom rules */
    float policy_strictness;                    /**< Strictness [0.0, 1.0] */
    const char* policy_file_path;               /**< Path to policy file */
} security_language_policy_config_t;

/**
 * @brief Security language bridge configuration
 *
 * WHAT: Complete configuration for security-language bridge
 */
typedef struct {
    /* Feature enables */
    bool enable_sanitization;                   /**< Enable input sanitization */
    bool enable_injection_detection;            /**< Enable injection detection */
    bool enable_content_filtering;              /**< Enable content filtering */
    bool enable_policy_checking;                /**< Enable policy validation */
    bool enable_output_validation;              /**< Validate generated output */

    /* Sub-configurations */
    security_language_sanitize_config_t sanitize;    /**< Sanitization config */
    security_language_detection_config_t detection;  /**< Detection config */
    security_language_policy_config_t policy;        /**< Policy config */

    /* Threshold settings */
    float threat_threshold;                     /**< Threat score threshold */
    float block_threshold;                      /**< Score to block input */

    /* Bio-async integration */
    bool enable_bio_async;                      /**< Enable bio-async messaging */

    /* Performance settings */
    bool enable_caching;                        /**< Cache detection results */
    size_t cache_size;                          /**< Cache size (entries) */
    bool enable_parallel_detection;             /**< Parallel pattern matching */
    uint32_t worker_threads;                    /**< Worker thread count */
} security_language_bridge_config_t;

/*=============================================================================
 * STATE AND STATISTICS STRUCTURES
 *============================================================================*/

/**
 * @brief Bridge operational state
 */
typedef struct {
    bool active;                                /**< Bridge is active */
    bool sanitization_enabled;                  /**< Sanitization currently on */
    bool detection_enabled;                     /**< Detection currently on */
    bool filtering_enabled;                     /**< Filtering currently on */
    bool in_lockdown_mode;                      /**< Security lockdown active */
    security_language_threat_severity_t current_threat_level; /**< Current level */
    uint64_t last_update_time_ns;               /**< Last update timestamp */
    uint64_t last_detection_time_ns;            /**< Last injection detected */
} security_language_bridge_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Input processing */
    uint64_t total_inputs_processed;            /**< Total inputs processed */
    uint64_t inputs_sanitized;                  /**< Inputs requiring sanitization */
    uint64_t inputs_blocked;                    /**< Inputs blocked */
    uint64_t inputs_passed;                     /**< Inputs passed through */

    /* Injection detection */
    uint64_t injections_detected;               /**< Total injections detected */
    uint64_t injections_by_type[INJECTION_TYPE_COUNT]; /**< Per-type counts */
    float avg_detection_confidence;             /**< Average confidence */
    float max_detection_confidence;             /**< Maximum confidence seen */

    /* Content policy */
    uint64_t policy_checks_performed;           /**< Total policy checks */
    uint64_t policy_violations;                 /**< Total violations */
    uint64_t violations_by_type[POLICY_VIOLATION_COUNT]; /**< Per-type counts */

    /* Output validation */
    uint64_t outputs_validated;                 /**< Total outputs validated */
    uint64_t outputs_modified;                  /**< Outputs requiring modification */
    uint64_t outputs_blocked;                   /**< Outputs blocked */

    /* Threat scoring */
    uint64_t threat_scores_computed;            /**< Threat scores computed */
    float avg_threat_score;                     /**< Average threat score */
    float max_threat_score;                     /**< Maximum threat score */
    uint64_t threshold_exceeded_count;          /**< Times threshold exceeded */

    /* Performance */
    float avg_sanitize_time_ns;                 /**< Average sanitization time */
    float avg_detection_time_ns;                /**< Average detection time */
    float avg_policy_check_time_ns;             /**< Average policy check time */
    float max_processing_time_ns;               /**< Maximum processing time */

    /* Cache statistics */
    uint64_t cache_hits;                        /**< Cache hits */
    uint64_t cache_misses;                      /**< Cache misses */
    float cache_hit_ratio;                      /**< Hit ratio */
} security_language_bridge_stats_t;

/*=============================================================================
 * MAIN BRIDGE STRUCTURE
 *============================================================================*/

/**
 * @brief Security-Language Bridge
 *
 * WHAT: Main bridge structure connecting security and language systems
 * WHY:  Enable comprehensive input sanitization and injection detection
 *
 * NOTE: bridge_base_t MUST be the first member for proper casting
 */
typedef struct {
    bridge_base_t base;                         /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    security_language_bridge_config_t config;   /**< Bridge configuration */

    /* Connected systems - Language side */
    language_orchestrator_t* language_orchestrator; /**< Language orchestrator */

    /* Connected systems - Security side */
    bbb_system_t bbb_system;                    /**< Blood-brain barrier */
    nimcp_pattern_db_t pattern_db;              /**< Pattern database */
    nimcp_policy_engine_t policy_engine;        /**< Policy engine */

    /* Bidirectional effects */
    security_to_language_effects_t security_effects; /**< Security -> Language */
    language_to_security_effects_t language_effects; /**< Language -> Security */

    /* State and statistics */
    security_language_bridge_state_t state;     /**< Current state */
    security_language_bridge_stats_t stats;     /**< Statistics */
} security_language_bridge_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible defaults for security-language bridge
 * WHY:  Simplifies initialization with secure defaults
 *
 * @param config Output configuration structure (must not be NULL)
 * @return 0 on success, error code on failure
 *
 * DEFAULTS:
 * - All detection types enabled
 * - Sanitization enabled with moderate intensity
 * - Content filtering enabled
 * - Threat threshold: 0.7
 * - Block threshold: 0.9
 */
int security_language_default_config(security_language_bridge_config_t* config);

/**
 * @brief Create security-language bridge
 *
 * WHAT: Allocates and initializes the bridge
 * WHY:  Entry point for security-language integration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge pointer or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~16KB base + cache (configurable)
 */
security_language_bridge_t* security_language_bridge_create(
    const security_language_bridge_config_t* config
);

/**
 * @brief Destroy security-language bridge
 *
 * WHAT: Cleanup and free all resources
 * WHY:  Prevent memory leaks
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_language_bridge_destroy(security_language_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear statistics and state, preserve configuration
 * WHY:  Enable reuse without reconnection
 *
 * @param bridge Bridge to reset
 * @return 0 on success, error code on failure
 */
int security_language_bridge_reset(security_language_bridge_t* bridge);

/*=============================================================================
 * CONNECTION FUNCTIONS - LANGUAGE SIDE
 *============================================================================*/

/**
 * @brief Connect language orchestrator
 *
 * WHAT: Connect to central language processing coordinator
 * WHY:  Enable security gating of language input/output
 *
 * @param bridge Security-language bridge
 * @param orchestrator Language orchestrator instance
 * @return 0 on success, error code on failure
 */
int security_language_connect_orchestrator(
    security_language_bridge_t* bridge,
    language_orchestrator_t* orchestrator
);

/**
 * @brief Disconnect language orchestrator
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_disconnect_orchestrator(security_language_bridge_t* bridge);

/*=============================================================================
 * CONNECTION FUNCTIONS - SECURITY SIDE
 *============================================================================*/

/**
 * @brief Connect blood-brain barrier system
 *
 * WHAT: Connect to BBB for coordinated perimeter defense
 * WHY:  Share threat information and coordinate responses
 *
 * @param bridge Security-language bridge
 * @param bbb Blood-brain barrier system
 * @return 0 on success, error code on failure
 */
int security_language_connect_bbb(
    security_language_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect pattern database
 *
 * WHAT: Connect to pattern DB for signature-based detection
 * WHY:  Leverage known injection signatures
 *
 * @param bridge Security-language bridge
 * @param pattern_db Pattern database instance
 * @return 0 on success, error code on failure
 */
int security_language_connect_pattern_db(
    security_language_bridge_t* bridge,
    nimcp_pattern_db_t pattern_db
);

/**
 * @brief Connect policy engine
 *
 * WHAT: Connect to policy engine for rule-based filtering
 * WHY:  Enable policy-driven content decisions
 *
 * @param bridge Security-language bridge
 * @param policy_engine Policy engine instance
 * @return 0 on success, error code on failure
 */
int security_language_connect_policy_engine(
    security_language_bridge_t* bridge,
    nimcp_policy_engine_t policy_engine
);

/**
 * @brief Disconnect all security systems
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_disconnect_security(security_language_bridge_t* bridge);

/**
 * @brief Check if bridge is fully connected
 *
 * @param bridge Security-language bridge
 * @return true if all required systems connected
 */
bool security_language_is_connected(const security_language_bridge_t* bridge);

/*=============================================================================
 * CORE SECURITY FUNCTIONS - INPUT SANITIZATION
 *============================================================================*/

/**
 * @brief Sanitize input text
 *
 * WHAT: Remove or escape dangerous content from input
 * WHY:  Neutralize potential injection payloads
 *
 * @param bridge Security-language bridge
 * @param input Input text to sanitize
 * @param input_len Length of input (0 for null-terminated)
 * @param result Output sanitization result
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(n) where n = input length
 * THREAD SAFETY: Thread-safe
 *
 * NOTE: Caller must free result->sanitized_output if result->modified is true
 */
int security_language_sanitize_input(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_sanitize_result_t* result
);

/**
 * @brief Sanitize input with custom configuration
 *
 * @param bridge Security-language bridge
 * @param input Input text to sanitize
 * @param input_len Length of input
 * @param config Custom sanitization config
 * @param result Output sanitization result
 * @return 0 on success, error code on failure
 */
int security_language_sanitize_input_ex(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    const security_language_sanitize_config_t* config,
    security_language_sanitize_result_t* result
);

/**
 * @brief Free sanitization result
 *
 * @param result Result to free
 */
void security_language_sanitize_result_free(security_language_sanitize_result_t* result);

/*=============================================================================
 * CORE SECURITY FUNCTIONS - INJECTION DETECTION
 *============================================================================*/

/**
 * @brief Detect injection attempts in input
 *
 * WHAT: Scan input for various injection attack patterns
 * WHY:  Identify and report potential security threats
 *
 * @param bridge Security-language bridge
 * @param input Input text to scan
 * @param input_len Length of input (0 for null-terminated)
 * @param result Output detection result
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(n * p) where n = input length, p = pattern count
 * THREAD SAFETY: Thread-safe
 */
int security_language_detect_injection(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_detection_result_t* result
);

/**
 * @brief Detect specific injection type
 *
 * @param bridge Security-language bridge
 * @param input Input text to scan
 * @param input_len Length of input
 * @param type Specific injection type to detect
 * @param result Output detection result
 * @return 0 on success, error code on failure
 */
int security_language_detect_injection_type(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    security_language_injection_type_t type,
    security_language_detection_result_t* result
);

/**
 * @brief Detect injection with custom configuration
 *
 * @param bridge Security-language bridge
 * @param input Input text to scan
 * @param input_len Length of input
 * @param config Custom detection config
 * @param result Output detection result
 * @return 0 on success, error code on failure
 */
int security_language_detect_injection_ex(
    security_language_bridge_t* bridge,
    const char* input,
    size_t input_len,
    const security_language_detection_config_t* config,
    security_language_detection_result_t* result
);

/*=============================================================================
 * CORE SECURITY FUNCTIONS - CONTENT POLICY
 *============================================================================*/

/**
 * @brief Check content against policies
 *
 * WHAT: Validate content against defined policies
 * WHY:  Ensure content meets safety and compliance requirements
 *
 * @param bridge Security-language bridge
 * @param content Content to check
 * @param content_len Length of content (0 for null-terminated)
 * @param result Output policy check result
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(n * r) where n = content length, r = rule count
 * THREAD SAFETY: Thread-safe
 */
int security_language_check_content_policy(
    security_language_bridge_t* bridge,
    const char* content,
    size_t content_len,
    security_language_policy_result_t* result
);

/**
 * @brief Check content against specific policy
 *
 * @param bridge Security-language bridge
 * @param content Content to check
 * @param content_len Length of content
 * @param policy_id Specific policy ID to check
 * @param result Output policy check result
 * @return 0 on success, error code on failure
 */
int security_language_check_policy_id(
    security_language_bridge_t* bridge,
    const char* content,
    size_t content_len,
    const char* policy_id,
    security_language_policy_result_t* result
);

/*=============================================================================
 * CORE SECURITY FUNCTIONS - OUTPUT VALIDATION
 *============================================================================*/

/**
 * @brief Validate generated output
 *
 * WHAT: Validate output before delivery
 * WHY:  Ensure generated content is safe and policy-compliant
 *
 * @param bridge Security-language bridge
 * @param output Generated output to validate
 * @param output_len Length of output (0 for null-terminated)
 * @param result Output validation result
 * @return 0 on success, error code on failure
 *
 * COMPLEXITY: O(n) for sanitization + O(n * p) for injection + O(n * r) for policy
 * THREAD SAFETY: Thread-safe
 */
int security_language_validate_output(
    security_language_bridge_t* bridge,
    const char* output,
    size_t output_len,
    security_language_output_validation_t* result
);

/**
 * @brief Validate and optionally modify output
 *
 * @param bridge Security-language bridge
 * @param output Generated output to validate
 * @param output_len Length of output
 * @param allow_modification Allow automatic modification
 * @param result Output validation result
 * @return 0 on success, error code on failure
 */
int security_language_validate_output_ex(
    security_language_bridge_t* bridge,
    const char* output,
    size_t output_len,
    bool allow_modification,
    security_language_output_validation_t* result
);

/*=============================================================================
 * CORE SECURITY FUNCTIONS - THREAT SCORING
 *============================================================================*/

/**
 * @brief Compute overall threat score for text
 *
 * WHAT: Calculate comprehensive threat assessment
 * WHY:  Single metric for security decisions
 *
 * @param bridge Security-language bridge
 * @param text Text to assess
 * @param text_len Length of text (0 for null-terminated)
 * @param score Output threat score result
 * @return 0 on success, error code on failure
 *
 * SCORING COMPONENTS:
 * - Injection detection results
 * - Pattern database matches
 * - Anomaly analysis
 * - Policy violations
 *
 * COMPLEXITY: O(n * (p + r)) where n = text length, p = patterns, r = rules
 * THREAD SAFETY: Thread-safe
 */
int security_language_get_threat_score(
    security_language_bridge_t* bridge,
    const char* text,
    size_t text_len,
    security_language_threat_score_t* score
);

/**
 * @brief Check if text exceeds threat threshold
 *
 * @param bridge Security-language bridge
 * @param text Text to check
 * @param text_len Length of text
 * @return true if threat score exceeds configured threshold
 */
bool security_language_exceeds_threshold(
    security_language_bridge_t* bridge,
    const char* text,
    size_t text_len
);

/*=============================================================================
 * UPDATE AND QUERY FUNCTIONS
 *============================================================================*/

/**
 * @brief Update bridge state
 *
 * WHAT: Main update cycle for bridge
 * WHY:  Process pending events and update effects
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 *
 * SIDE EFFECTS:
 * - Updates bidirectional effects
 * - Processes bio-async messages
 * - Updates statistics
 */
int security_language_update(security_language_bridge_t* bridge);

/**
 * @brief Apply security modulation to language processing
 *
 * WHAT: Push security effects to language system
 * WHY:  Modulate language processing based on threat level
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_apply_modulation(security_language_bridge_t* bridge);

/**
 * @brief Query current security effects
 *
 * @param bridge Security-language bridge
 * @param security_effects Output: Security -> Language effects (can be NULL)
 * @param language_effects Output: Language -> Security effects (can be NULL)
 * @return 0 on success, error code on failure
 */
int security_language_query_effects(
    const security_language_bridge_t* bridge,
    security_to_language_effects_t* security_effects,
    language_to_security_effects_t* language_effects
);

/**
 * @brief Get current threat level
 *
 * @param bridge Security-language bridge
 * @return Current threat severity level
 */
security_language_threat_severity_t security_language_get_threat_level(
    const security_language_bridge_t* bridge
);

/**
 * @brief Set threat level manually
 *
 * @param bridge Security-language bridge
 * @param level Threat level to set
 * @return 0 on success, error code on failure
 */
int security_language_set_threat_level(
    security_language_bridge_t* bridge,
    security_language_threat_severity_t level
);

/**
 * @brief Enter lockdown mode
 *
 * WHAT: Activate maximum security restrictions
 * WHY:  Response to critical threat detection
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_enter_lockdown(security_language_bridge_t* bridge);

/**
 * @brief Exit lockdown mode
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_exit_lockdown(security_language_bridge_t* bridge);

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-language bridge
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int security_language_get_stats(
    const security_language_bridge_t* bridge,
    security_language_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_reset_stats(security_language_bridge_t* bridge);

/**
 * @brief Get current state
 *
 * @param bridge Security-language bridge
 * @param state Output state
 * @return 0 on success, error code on failure
 */
int security_language_get_state(
    const security_language_bridge_t* bridge,
    security_language_bridge_state_t* state
);

/*=============================================================================
 * BIO-ASYNC FUNCTIONS
 *============================================================================*/

/**
 * @brief Connect to bio-async router
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_connect_bio_async(security_language_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Security-language bridge
 * @return 0 on success, error code on failure
 */
int security_language_disconnect_bio_async(security_language_bridge_t* bridge);

/**
 * @brief Check bio-async connection status
 *
 * @param bridge Security-language bridge
 * @return true if connected
 */
bool security_language_is_bio_async_connected(const security_language_bridge_t* bridge);

/**
 * @brief Process bio-async inbox messages
 *
 * @param bridge Security-language bridge
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t security_language_process_inbox(
    security_language_bridge_t* bridge,
    uint32_t max_messages
);

/**
 * @brief Broadcast security event via bio-async
 *
 * @param bridge Security-language bridge
 * @param detection Detection to broadcast
 * @return 0 on success, error code on failure
 */
int security_language_broadcast_detection(
    security_language_bridge_t* bridge,
    const security_language_detection_t* detection
);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get injection type name
 *
 * @param type Injection type enum
 * @return Human-readable name
 */
const char* security_language_injection_type_name(security_language_injection_type_t type);

/**
 * @brief Get sanitize action name
 *
 * @param action Sanitize action enum
 * @return Human-readable name
 */
const char* security_language_sanitize_action_name(security_language_sanitize_action_t action);

/**
 * @brief Get policy violation name
 *
 * @param violation Policy violation enum
 * @return Human-readable name
 */
const char* security_language_policy_violation_name(security_language_policy_violation_t violation);

/**
 * @brief Get threat severity name
 *
 * @param severity Threat severity enum
 * @return Human-readable name
 */
const char* security_language_threat_severity_name(security_language_threat_severity_t severity);

/**
 * @brief Print detection result to stdout (debug)
 *
 * @param result Detection result to print
 */
void security_language_print_detection(const security_language_detection_result_t* result);

/**
 * @brief Print bridge summary to stdout (debug)
 *
 * @param bridge Security-language bridge
 */
void security_language_print_summary(const security_language_bridge_t* bridge);

/**
 * @brief Convert threat severity to BBB severity
 *
 * @param severity Security-language threat severity
 * @return Equivalent BBB severity level
 */
bbb_severity_t security_language_to_bbb_severity(security_language_threat_severity_t severity);

/**
 * @brief Convert BBB threat type to injection type
 *
 * @param bbb_threat BBB threat type
 * @return Equivalent injection type (or INJECTION_TYPE_NONE)
 */
security_language_injection_type_t security_language_from_bbb_threat(bbb_threat_type_t bbb_threat);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_LANGUAGE_BRIDGE_H */
