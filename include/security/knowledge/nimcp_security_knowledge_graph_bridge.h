/**
 * @file nimcp_security_knowledge_graph_bridge.h
 * @brief Security-Knowledge Graph Bridge - Protection for Graph Operations
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bridge for protecting knowledge graph operations from security threats
 *       including query injection, unauthorized traversal, and data corruption.
 * WHY:  Knowledge graphs contain sensitive information and structural relationships
 *       that could be exploited through malicious queries or traversal patterns.
 * HOW:  Integrate security validation with KG reader, validate queries, enforce
 *       access control, verify node integrity, and isolate private data.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * MEMORY PROTECTION MECHANISMS:
 * -----------------------------
 * The brain protects memory structures through multiple mechanisms:
 * - Hippocampal gating: Controls access to memory formation/retrieval
 * - Sleep consolidation: Validates and strengthens important memories
 * - Semantic boundaries: Prevents cross-contamination of memory traces
 * - Prefrontal control: Executive oversight of memory access
 *
 * This bridge implements computational analogues:
 * - Query Validation: Detect and prevent injection attacks
 * - Traversal Access Control: Limit graph exploration depth and scope
 * - Node Integrity: Verify nodes haven't been tampered with
 * - Consistency Enforcement: Maintain graph structural invariants
 * - Privacy Isolation: Mark and protect sensitive information nodes
 *
 * KNOWLEDGE GRAPH THREATS:
 * ------------------------
 * - Injection Attacks: Malicious queries containing escape sequences
 * - Traversal Attacks: Exploiting graph structure to access restricted data
 * - Integrity Attacks: Modifying nodes to corrupt knowledge representation
 * - Consistency Attacks: Breaking referential integrity or creating cycles
 * - Privacy Leaks: Inferring private data through query patterns
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |          SECURITY-KNOWLEDGE GRAPH BRIDGE (Graph Protection)               |
 * +===========================================================================+
 * |                                                                           |
 * |   +-------------------+                      +-------------------+        |
 * |   |   Query Input     |                      |   Traversal Req   |        |
 * |   | (graph queries)   |                      |  (path requests)  |        |
 * |   +--------+----------+                      +--------+----------+        |
 * |            |                                          |                   |
 * |            v                                          v                   |
 * |   +--------+----------+                      +--------+----------+        |
 * |   | Query Validation  |                      | Access Control    |        |
 * |   | - Injection check |                      | - Depth limits    |        |
 * |   | - Pattern match   |                      | - Scope restrict  |        |
 * |   | - Length limits   |                      | - Auth check      |        |
 * |   +--------+----------+                      +--------+----------+        |
 * |            |                                          |                   |
 * |            +----------------+     +-------------------+                   |
 * |                             |     |                                       |
 * |                             v     v                                       |
 * |                    +--------+-----+--------+                              |
 * |                    |  Consistency Layer    |                              |
 * |                    |  (Integrity Check)    |                              |
 * |                    +-----------+-----------+                              |
 * |                                |                                          |
 * |            +-------------------+-------------------+                      |
 * |            |                   |                   |                      |
 * |            v                   v                   v                      |
 * |   +--------+------+   +-------+-------+   +-------+-------+               |
 * |   |   BBB         |   | Privacy       |   | Effects       |               |
 * |   | (validation)  |   | Isolation     |   | (bidirectional|               |
 * |   +---------------+   | (data mask)   |   |  flow)        |               |
 * |                       +---------------+   +---------------+               |
 * |                                                                           |
 * +===========================================================================+
 * ```
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects security layer to knowledge graph operations
 * - Strategy: Pluggable validation algorithms per query type
 * - Observer: Notification of security violations
 * - Decorator: Wrap graph operations with security checks
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

#ifndef NIMCP_SECURITY_KNOWLEDGE_GRAPH_BRIDGE_H
#define NIMCP_SECURITY_KNOWLEDGE_GRAPH_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bridge base */
#include "utils/bridge/nimcp_bridge_base.h"

/* Security modules */
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"

/* Knowledge graph modules */
#include "cognitive/knowledge/nimcp_kg_reader.h"

/* Utilities */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum query string length */
#define SEC_KG_MAX_QUERY_LENGTH             4096

/** Maximum traversal depth allowed */
#define SEC_KG_MAX_TRAVERSAL_DEPTH          32

/** Maximum nodes per query result */
#define SEC_KG_MAX_RESULT_NODES             1024

/** Maximum relations per query result */
#define SEC_KG_MAX_RESULT_RELATIONS         2048

/** Module identification */
#define SEC_KG_MODULE_NAME                  "security_knowledge_graph_bridge"
#define BIO_MODULE_SEC_KG                   0x4020

/** Default thresholds */
#define SEC_KG_DEFAULT_INJECTION_THRESHOLD  0.7f
#define SEC_KG_DEFAULT_TRAVERSAL_THRESHOLD  0.8f
#define SEC_KG_DEFAULT_INTEGRITY_THRESHOLD  0.9f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct security_kg_bridge security_kg_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Query validation result codes
 *
 * WHAT: Outcome of query security validation
 * WHY:  Distinguish between valid queries and various attack types
 */
typedef enum {
    SEC_KG_QUERY_VALID = 0,            /**< Query passed all validation */
    SEC_KG_QUERY_INJECTION_DETECTED,   /**< Injection attempt detected */
    SEC_KG_QUERY_TOO_LONG,             /**< Query exceeds length limit */
    SEC_KG_QUERY_MALFORMED,            /**< Query structure invalid */
    SEC_KG_QUERY_FORBIDDEN_PATTERN,    /**< Contains forbidden pattern */
    SEC_KG_QUERY_RATE_LIMITED,         /**< Too many queries from source */
    SEC_KG_QUERY_UNAUTHORIZED,         /**< Query not authorized for user */
    SEC_KG_QUERY_REJECTED              /**< Query rejected by policy */
} sec_kg_query_result_t;

/**
 * @brief Traversal access result codes
 *
 * WHAT: Outcome of traversal access control check
 * WHY:  Control graph exploration to prevent unauthorized access
 */
typedef enum {
    SEC_KG_TRAVERSAL_ALLOWED = 0,      /**< Traversal permitted */
    SEC_KG_TRAVERSAL_DEPTH_EXCEEDED,   /**< Depth limit exceeded */
    SEC_KG_TRAVERSAL_SCOPE_DENIED,     /**< Outside allowed scope */
    SEC_KG_TRAVERSAL_NODE_PRIVATE,     /**< Target node is private */
    SEC_KG_TRAVERSAL_EDGE_FORBIDDEN,   /**< Edge type not allowed */
    SEC_KG_TRAVERSAL_CYCLE_DETECTED,   /**< Infinite cycle detected */
    SEC_KG_TRAVERSAL_RATE_LIMITED,     /**< Too many traversals */
    SEC_KG_TRAVERSAL_DENIED            /**< Generic denial */
} sec_kg_traversal_result_t;

/**
 * @brief Node integrity status
 *
 * WHAT: Result of node integrity verification
 * WHY:  Detect tampering with knowledge graph nodes
 */
typedef enum {
    SEC_KG_NODE_VALID = 0,             /**< Node integrity verified */
    SEC_KG_NODE_HASH_MISMATCH,         /**< Content hash doesn't match */
    SEC_KG_NODE_SIGNATURE_INVALID,     /**< Signature verification failed */
    SEC_KG_NODE_TIMESTAMP_ANOMALY,     /**< Suspicious timestamp pattern */
    SEC_KG_NODE_RELATION_MISMATCH,     /**< Inbound/outbound mismatch */
    SEC_KG_NODE_CORRUPTED,             /**< Node data is corrupted */
    SEC_KG_NODE_NOT_FOUND              /**< Node doesn't exist */
} sec_kg_integrity_result_t;

/**
 * @brief Consistency check result
 *
 * WHAT: Result of graph consistency enforcement
 * WHY:  Ensure knowledge graph maintains structural invariants
 */
typedef enum {
    SEC_KG_CONSISTENT = 0,             /**< Graph is consistent */
    SEC_KG_ORPHANED_NODE,              /**< Node has no relations */
    SEC_KG_DANGLING_RELATION,          /**< Relation points to missing node */
    SEC_KG_DUPLICATE_ENTRY,            /**< Duplicate node/relation exists */
    SEC_KG_CYCLE_VIOLATION,            /**< Illegal cycle detected */
    SEC_KG_TYPE_VIOLATION,             /**< Type constraint violated */
    SEC_KG_CONSTRAINT_VIOLATION        /**< Generic constraint violation */
} sec_kg_consistency_result_t;

/**
 * @brief Privacy isolation level
 *
 * WHAT: Level of privacy protection for nodes
 * WHY:  Control access to sensitive information
 */
typedef enum {
    SEC_KG_PRIVACY_PUBLIC = 0,         /**< Publicly accessible */
    SEC_KG_PRIVACY_INTERNAL,           /**< Internal system access only */
    SEC_KG_PRIVACY_RESTRICTED,         /**< Requires explicit permission */
    SEC_KG_PRIVACY_CONFIDENTIAL,       /**< Limited to specific roles */
    SEC_KG_PRIVACY_SECRET              /**< Maximum protection */
} sec_kg_privacy_level_t;

/**
 * @brief Bridge operational state
 */
typedef enum {
    SEC_KG_STATE_UNINITIALIZED = 0,    /**< Not yet initialized */
    SEC_KG_STATE_READY,                /**< Ready for operations */
    SEC_KG_STATE_PROCESSING,           /**< Currently processing */
    SEC_KG_STATE_LOCKDOWN,             /**< Security lockdown active */
    SEC_KG_STATE_DEGRADED,             /**< Partial functionality */
    SEC_KG_STATE_ERROR                 /**< Error state */
} sec_kg_state_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-Knowledge Graph Bridge Configuration
 *
 * WHAT: Configuration for knowledge graph security bridge
 * WHY:  Control validation strictness, features, and thresholds
 * HOW:  Set flags and thresholds for each security component
 */
typedef struct {
    /* Query validation settings */
    bool enable_query_validation;      /**< Enable query injection checking */
    float injection_threshold;         /**< Injection detection threshold [0-1] */
    uint32_t max_query_length;         /**< Maximum allowed query length */
    bool enable_pattern_matching;      /**< Enable pattern-based detection */
    bool enable_rate_limiting;         /**< Enable query rate limiting */
    uint32_t queries_per_second_limit; /**< Max queries per second */

    /* Traversal settings */
    bool enable_traversal_control;     /**< Enable traversal access control */
    uint32_t max_traversal_depth;      /**< Maximum traversal depth */
    uint32_t max_nodes_per_query;      /**< Maximum nodes in result */
    bool enable_scope_restriction;     /**< Restrict traversal scope */
    bool enable_cycle_detection;       /**< Detect traversal cycles */

    /* Integrity settings */
    bool enable_integrity_verification;/**< Enable node integrity checks */
    float integrity_threshold;         /**< Integrity check threshold [0-1] */
    bool enable_hash_verification;     /**< Verify content hashes */
    bool enable_signature_verification;/**< Verify digital signatures */

    /* Consistency settings */
    bool enable_consistency_checks;    /**< Enable graph consistency checks */
    bool enforce_referential_integrity;/**< Enforce referential integrity */
    bool detect_orphaned_nodes;        /**< Detect nodes without relations */
    bool prevent_cycles;               /**< Prevent cycle creation */

    /* Privacy settings */
    bool enable_privacy_isolation;     /**< Enable privacy controls */
    sec_kg_privacy_level_t default_privacy; /**< Default privacy level */
    bool enable_privacy_inference_protection; /**< Prevent inference attacks */
    bool log_access_attempts;          /**< Log all access attempts */

    /* Integration settings */
    bool enable_bbb;                   /**< Enable BBB integration */
    bool enable_anomaly_detector;      /**< Enable anomaly detection */
    bool enable_bio_async;             /**< Enable bio-async messaging */
    bool enable_logging;               /**< Enable detailed logging */
} sec_kg_config_t;

/* ============================================================================
 * Effects Structures (Bidirectional Flow)
 * ============================================================================ */

/**
 * @brief Security to Knowledge Graph Effects
 *
 * WHAT: Effects flowing from security to knowledge graph operations
 * WHY:  Allow security to modulate graph access and operations
 * HOW:  Threat levels restrict queries and traversals
 */
typedef struct {
    /* Threat assessment */
    float query_threat_level;          /**< Current query threat level [0-1] */
    float traversal_threat_level;      /**< Current traversal threat level [0-1] */
    float combined_threat_level;       /**< Combined threat assessment [0-1] */

    /* Access restrictions */
    bool queries_blocked;              /**< All queries blocked */
    bool traversals_blocked;           /**< All traversals blocked */
    bool writes_blocked;               /**< Write operations blocked */
    uint32_t current_traversal_limit;  /**< Current depth limit (may be reduced) */

    /* Processing guidance */
    bool require_enhanced_validation;  /**< Request enhanced validation */
    bool require_integrity_check;      /**< Require integrity verification */
    sec_kg_privacy_level_t min_privacy; /**< Minimum privacy level to access */
    uint32_t restricted_node_mask;     /**< Bitmask of restricted node types */
} sec_to_kg_effects_t;

/**
 * @brief Knowledge Graph to Security Effects
 *
 * WHAT: Information flowing from knowledge graph to security
 * WHY:  Provide security with graph operation statistics for analysis
 * HOW:  Report query patterns, access attempts, and anomalies
 */
typedef struct {
    /* Query statistics */
    uint64_t queries_processed;        /**< Total queries this period */
    uint64_t query_rate_per_second;    /**< Current query rate */
    float avg_query_length;            /**< Average query length */
    float query_complexity_score;      /**< Query complexity metric [0-1] */

    /* Traversal statistics */
    uint64_t traversals_processed;     /**< Total traversals this period */
    float avg_traversal_depth;         /**< Average traversal depth */
    uint32_t max_depth_reached;        /**< Maximum depth reached */
    float traversal_fan_out;           /**< Average edges per hop */

    /* Anomaly indicators */
    float query_anomaly_score;         /**< Query pattern anomaly [0-1] */
    float traversal_anomaly_score;     /**< Traversal pattern anomaly [0-1] */
    bool anomaly_flag_raised;          /**< Whether anomaly threshold exceeded */
    uint64_t anomaly_timestamp_us;     /**< When anomaly was detected */

    /* Privacy access */
    uint64_t private_access_attempts;  /**< Attempts to access private nodes */
    uint64_t private_access_denied;    /**< Private access denials */
} kg_to_sec_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Bridge State
 *
 * WHAT: Current operational state of the bridge
 * WHY:  Track validation state and connection status
 */
typedef struct {
    sec_kg_state_t operational_state;  /**< Current operational state */

    /* Connection status */
    bool kg_reader_connected;          /**< KG reader connection status */
    bool bbb_connected;                /**< BBB connection status */
    bool anomaly_detector_connected;   /**< Anomaly detector connection status */

    /* Current validation state */
    sec_kg_query_result_t last_query_result;     /**< Last query result */
    sec_kg_traversal_result_t last_traversal_result; /**< Last traversal result */
    sec_kg_integrity_result_t last_integrity_result; /**< Last integrity result */
    uint64_t last_validation_time_us;  /**< Timestamp of last validation */

    /* Active threat tracking */
    bool query_threat_active;          /**< Query threat currently active */
    bool traversal_threat_active;      /**< Traversal threat currently active */
    float current_query_threat;        /**< Current query threat score */
    float current_traversal_threat;    /**< Current traversal threat score */

    /* Lockdown state */
    bool lockdown_active;              /**< Security lockdown active */
    uint64_t lockdown_start_time;      /**< When lockdown started */
    const char* lockdown_reason;       /**< Reason for lockdown */
} sec_kg_bridge_state_t;

/**
 * @brief Bridge Statistics
 *
 * WHAT: Cumulative statistics for bridge operation
 * WHY:  Monitor security effectiveness and performance
 */
typedef struct {
    /* Query validation counts */
    uint64_t queries_validated_total;  /**< Total queries validated */
    uint64_t queries_passed;           /**< Queries that passed validation */
    uint64_t queries_rejected;         /**< Queries rejected */
    uint64_t injections_detected;      /**< Injection attempts detected */

    /* Traversal counts */
    uint64_t traversals_validated_total; /**< Total traversals validated */
    uint64_t traversals_allowed;       /**< Traversals allowed */
    uint64_t traversals_denied;        /**< Traversals denied */
    uint64_t depth_limit_violations;   /**< Depth limit exceeded count */

    /* Integrity counts */
    uint64_t integrity_checks_total;   /**< Total integrity checks */
    uint64_t integrity_verified;       /**< Nodes verified successfully */
    uint64_t integrity_failed;         /**< Integrity verification failures */

    /* Consistency counts */
    uint64_t consistency_checks_total; /**< Total consistency checks */
    uint64_t consistency_passed;       /**< Consistency checks passed */
    uint64_t consistency_violations;   /**< Consistency violations found */

    /* Privacy counts */
    uint64_t private_access_total;     /**< Total private access requests */
    uint64_t private_access_granted;   /**< Private access granted */
    uint64_t private_access_denied;    /**< Private access denied */

    /* Performance metrics */
    float avg_query_validation_time_us;   /**< Average query validation time */
    float avg_traversal_check_time_us;    /**< Average traversal check time */
    float avg_integrity_check_time_us;    /**< Average integrity check time */
    float max_validation_time_us;         /**< Maximum validation time */

    /* False positive tracking */
    uint64_t false_positives_reported; /**< False positives reported */
    float estimated_precision;         /**< Estimated precision score */
} sec_kg_stats_t;

/* ============================================================================
 * Private Node Registry (for privacy isolation)
 * ============================================================================ */

/**
 * @brief Private node entry
 *
 * WHAT: Entry in the private node registry
 * WHY:  Track nodes requiring privacy protection
 */
typedef struct {
    char node_name[KG_MAX_NAME_LENGTH];  /**< Node identifier */
    sec_kg_privacy_level_t privacy_level; /**< Privacy level */
    uint64_t isolation_timestamp;        /**< When isolation was set */
    uint32_t access_role_mask;           /**< Roles permitted to access */
} sec_kg_private_node_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-Knowledge Graph Bridge
 *
 * WHAT: Main bridge structure for KG security
 * WHY:  Coordinate security validation of knowledge graph operations
 * HOW:  Connect to KG reader, BBB, anomaly detector
 */
struct security_kg_bridge {
    /* Base bridge - MUST be first member */
    bridge_base_t base;

    /* Connected systems */
    kg_reader_t* kg_reader;            /**< Connected knowledge graph reader */
    bbb_system_t bbb;                  /**< Connected BBB system */
    nimcp_anomaly_detector_t anomaly_detector; /**< Connected anomaly detector */

    /* Configuration */
    sec_kg_config_t config;            /**< Bridge configuration */

    /* Bidirectional effects */
    sec_to_kg_effects_t sec_to_kg;     /**< Security->KG effects */
    kg_to_sec_effects_t kg_to_sec;     /**< KG->Security effects */

    /* State and statistics */
    sec_kg_bridge_state_t state;       /**< Current state */
    sec_kg_stats_t stats;              /**< Cumulative statistics */

    /* Private node registry */
    sec_kg_private_node_t* private_nodes;  /**< Array of private nodes */
    uint32_t private_node_count;           /**< Number of private nodes */
    uint32_t private_node_capacity;        /**< Allocated capacity */
};

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced security/performance
 * HOW:  Return pre-configured structure with moderate thresholds
 *
 * @param config Output configuration structure
 * @return 0 on success, -1 on error (null pointer)
 */
int security_kg_default_config(sec_kg_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create security-knowledge graph bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enable security validation of knowledge graph operations
 * HOW:  Allocate structure, initialize base, apply configuration
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on failure
 */
security_kg_bridge_t* security_kg_bridge_create(
    const sec_kg_config_t* config
);

/**
 * @brief Destroy security-knowledge graph bridge
 *
 * WHAT: Clean up and free bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, cleanup base, free memory
 *
 * @param bridge Bridge handle (NULL safe)
 */
void security_kg_bridge_destroy(
    security_kg_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect knowledge graph reader
 *
 * WHAT: Link KG reader module to bridge
 * WHY:  Enable security validation of KG operations
 * HOW:  Store reader handle, update connection state
 *
 * @param bridge Bridge handle
 * @param kg_reader KG reader handle
 * @return 0 on success, -1 on error
 */
int security_kg_connect_reader(
    security_kg_bridge_t* bridge,
    kg_reader_t* kg_reader
);

/**
 * @brief Connect BBB for security validation
 *
 * WHAT: Link BBB system to bridge
 * WHY:  Use BBB's validation capabilities
 * HOW:  Store BBB handle for validation calls
 *
 * @param bridge Bridge handle
 * @param bbb BBB system handle
 * @return 0 on success, -1 on error
 */
int security_kg_connect_bbb(
    security_kg_bridge_t* bridge,
    bbb_system_t bbb
);

/**
 * @brief Connect anomaly detector
 *
 * WHAT: Link anomaly detector to bridge
 * WHY:  Enable ML-based anomaly detection on queries
 * HOW:  Store detector handle for analysis calls
 *
 * @param bridge Bridge handle
 * @param detector Anomaly detector handle
 * @return 0 on success, -1 on error
 */
int security_kg_connect_anomaly_detector(
    security_kg_bridge_t* bridge,
    nimcp_anomaly_detector_t detector
);

/* ============================================================================
 * Query Validation API
 * ============================================================================ */

/**
 * @brief Validate graph query for injection attacks
 *
 * WHAT: Validate query string for injection attempts
 * WHY:  Prevent malicious queries from accessing/modifying graph
 * HOW:  Pattern matching, length check, escape sequence detection
 *
 * @param bridge Bridge handle
 * @param query Query string to validate
 * @param query_len Length of query string
 * @param result Output: validation result code
 * @return 0 on success, -1 on error
 */
int security_kg_validate_query(
    security_kg_bridge_t* bridge,
    const char* query,
    size_t query_len,
    sec_kg_query_result_t* result
);

/**
 * @brief Validate entity name query
 *
 * WHAT: Validate entity name for safe lookup
 * WHY:  Entity names could contain injection attempts
 * HOW:  Character validation, length check, reserved name check
 *
 * @param bridge Bridge handle
 * @param entity_name Entity name to validate
 * @param result Output: validation result code
 * @return 0 on success, -1 on error
 */
int security_kg_validate_entity_name(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_query_result_t* result
);

/**
 * @brief Validate search query
 *
 * WHAT: Validate search text for safe searching
 * WHY:  Search queries could be used for injection or DoS
 * HOW:  Regex check, complexity limit, length check
 *
 * @param bridge Bridge handle
 * @param search_text Search text to validate
 * @param result Output: validation result code
 * @return 0 on success, -1 on error
 */
int security_kg_validate_search(
    security_kg_bridge_t* bridge,
    const char* search_text,
    sec_kg_query_result_t* result
);

/* ============================================================================
 * Traversal Access Control API
 * ============================================================================ */

/**
 * @brief Check traversal access permissions
 *
 * WHAT: Verify permission to traverse from source to target
 * WHY:  Control graph exploration to prevent unauthorized access
 * HOW:  Check depth, scope, privacy levels, edge types
 *
 * @param bridge Bridge handle
 * @param source_entity Source entity name
 * @param target_entity Target entity name (NULL for any)
 * @param relation_type Relation type to traverse (NULL for any)
 * @param current_depth Current traversal depth
 * @param result Output: traversal result code
 * @return 0 on success, -1 on error
 */
int security_kg_check_traversal_access(
    security_kg_bridge_t* bridge,
    const char* source_entity,
    const char* target_entity,
    const char* relation_type,
    uint32_t current_depth,
    sec_kg_traversal_result_t* result
);

/**
 * @brief Check if entity is accessible
 *
 * WHAT: Check if entity can be accessed at current security level
 * WHY:  Quick access check before detailed operations
 * HOW:  Privacy level check, role verification
 *
 * @param bridge Bridge handle
 * @param entity_name Entity to check
 * @param accessible Output: true if accessible
 * @return 0 on success, -1 on error
 */
int security_kg_is_entity_accessible(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    bool* accessible
);

/**
 * @brief Set maximum traversal depth dynamically
 *
 * WHAT: Adjust maximum traversal depth based on threat level
 * WHY:  Reduce attack surface under threat conditions
 * HOW:  Update config, log change
 *
 * @param bridge Bridge handle
 * @param max_depth New maximum depth
 * @return 0 on success, -1 on error
 */
int security_kg_set_max_traversal_depth(
    security_kg_bridge_t* bridge,
    uint32_t max_depth
);

/* ============================================================================
 * Node Integrity API
 * ============================================================================ */

/**
 * @brief Verify node integrity
 *
 * WHAT: Verify node hasn't been tampered with
 * WHY:  Detect modifications to knowledge graph nodes
 * HOW:  Hash verification, signature check, timestamp validation
 *
 * @param bridge Bridge handle
 * @param entity_name Entity name to verify
 * @param result Output: integrity result code
 * @return 0 on success, -1 on error
 */
int security_kg_verify_node_integrity(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_integrity_result_t* result
);

/**
 * @brief Verify relation integrity
 *
 * WHAT: Verify relation between nodes is valid
 * WHY:  Detect tampering with graph structure
 * HOW:  Check both endpoints, verify relation type
 *
 * @param bridge Bridge handle
 * @param from_entity Source entity name
 * @param to_entity Target entity name
 * @param relation_type Relation type
 * @param result Output: integrity result code
 * @return 0 on success, -1 on error
 */
int security_kg_verify_relation_integrity(
    security_kg_bridge_t* bridge,
    const char* from_entity,
    const char* to_entity,
    const char* relation_type,
    sec_kg_integrity_result_t* result
);

/* ============================================================================
 * Consistency Enforcement API
 * ============================================================================ */

/**
 * @brief Enforce graph consistency
 *
 * WHAT: Check and enforce graph structural consistency
 * WHY:  Maintain integrity of knowledge representation
 * HOW:  Check referential integrity, detect violations
 *
 * @param bridge Bridge handle
 * @param result Output: consistency result code
 * @return 0 on success, -1 on error
 */
int security_kg_enforce_consistency(
    security_kg_bridge_t* bridge,
    sec_kg_consistency_result_t* result
);

/**
 * @brief Check for orphaned nodes
 *
 * WHAT: Find nodes with no incoming or outgoing relations
 * WHY:  Orphaned nodes may indicate data corruption
 * HOW:  Scan all nodes, check relation counts
 *
 * @param bridge Bridge handle
 * @param orphan_count Output: number of orphaned nodes found
 * @return 0 on success, -1 on error
 */
int security_kg_check_orphaned_nodes(
    security_kg_bridge_t* bridge,
    uint32_t* orphan_count
);

/**
 * @brief Check for dangling relations
 *
 * WHAT: Find relations pointing to non-existent nodes
 * WHY:  Dangling relations indicate structural corruption
 * HOW:  Verify both endpoints of all relations exist
 *
 * @param bridge Bridge handle
 * @param dangling_count Output: number of dangling relations found
 * @return 0 on success, -1 on error
 */
int security_kg_check_dangling_relations(
    security_kg_bridge_t* bridge,
    uint32_t* dangling_count
);

/* ============================================================================
 * Privacy Isolation API
 * ============================================================================ */

/**
 * @brief Isolate private data node
 *
 * WHAT: Mark node as private with specified protection level
 * WHY:  Protect sensitive information from unauthorized access
 * HOW:  Add to private registry, set access controls
 *
 * @param bridge Bridge handle
 * @param entity_name Entity to mark as private
 * @param privacy_level Privacy protection level
 * @return 0 on success, -1 on error
 */
int security_kg_isolate_private_data(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_privacy_level_t privacy_level
);

/**
 * @brief Remove privacy isolation
 *
 * WHAT: Remove private marking from node
 * WHY:  Allow controlled declassification
 * HOW:  Remove from private registry
 *
 * @param bridge Bridge handle
 * @param entity_name Entity to declassify
 * @return 0 on success, -1 on error
 */
int security_kg_remove_isolation(
    security_kg_bridge_t* bridge,
    const char* entity_name
);

/**
 * @brief Get privacy level for entity
 *
 * WHAT: Query privacy level of an entity
 * WHY:  Determine access requirements
 * HOW:  Lookup in private registry
 *
 * @param bridge Bridge handle
 * @param entity_name Entity to query
 * @param privacy_level Output: privacy level
 * @return 0 on success, -1 on error
 */
int security_kg_get_privacy_level(
    security_kg_bridge_t* bridge,
    const char* entity_name,
    sec_kg_privacy_level_t* privacy_level
);

/**
 * @brief Check if access is allowed for privacy level
 *
 * WHAT: Check if current context can access given privacy level
 * WHY:  Simple access decision for privacy controls
 * HOW:  Compare required level with current permissions
 *
 * @param bridge Bridge handle
 * @param required_level Required privacy level
 * @param allowed Output: true if access allowed
 * @return 0 on success, -1 on error
 */
int security_kg_check_privacy_access(
    security_kg_bridge_t* bridge,
    sec_kg_privacy_level_t required_level,
    bool* allowed
);

/* ============================================================================
 * Lockdown API
 * ============================================================================ */

/**
 * @brief Enter security lockdown mode
 *
 * WHAT: Block all KG operations due to security threat
 * WHY:  Protect graph during active attack
 * HOW:  Set lockdown flag, record reason
 *
 * @param bridge Bridge handle
 * @param reason Reason for lockdown
 * @return 0 on success, -1 on error
 */
int security_kg_enter_lockdown(
    security_kg_bridge_t* bridge,
    const char* reason
);

/**
 * @brief Exit security lockdown mode
 *
 * WHAT: Resume normal KG operations
 * WHY:  Return to normal after threat resolved
 * HOW:  Clear lockdown flag
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_kg_exit_lockdown(
    security_kg_bridge_t* bridge
);

/**
 * @brief Check if lockdown is active
 *
 * WHAT: Query lockdown status
 * WHY:  Determine if operations are blocked
 * HOW:  Return lockdown flag value
 *
 * @param bridge Bridge handle
 * @param active Output: true if lockdown active
 * @return 0 on success, -1 on error
 */
int security_kg_is_lockdown_active(
    security_kg_bridge_t* bridge,
    bool* active
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security to KG effects
 *
 * WHAT: Update effects flowing from security to KG
 * WHY:  Propagate current threat assessment to KG operations
 * HOW:  Compute effects from current state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_kg_update_sec_to_kg(
    security_kg_bridge_t* bridge
);

/**
 * @brief Update KG to security effects
 *
 * WHAT: Update effects flowing from KG to security
 * WHY:  Provide security with latest operation statistics
 * HOW:  Collect statistics from operations
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_kg_update_kg_to_sec(
    security_kg_bridge_t* bridge
);

/**
 * @brief Perform full bidirectional update cycle
 *
 * WHAT: Update both directions in single call
 * WHY:  Convenience for regular update cycles
 * HOW:  Call both update functions in sequence
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_kg_update(
    security_kg_bridge_t* bridge
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get security to KG effects
 *
 * WHAT: Retrieve current security->KG effects
 * WHY:  Allow KG operations to query current security state
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output: security->KG effects
 * @return 0 on success, -1 on error
 */
int security_kg_get_sec_to_kg_effects(
    const security_kg_bridge_t* bridge,
    sec_to_kg_effects_t* effects
);

/**
 * @brief Get KG to security effects
 *
 * WHAT: Retrieve current KG->security effects
 * WHY:  Allow security to query KG operation statistics
 * HOW:  Copy current effects to output structure
 *
 * @param bridge Bridge handle
 * @param effects Output: KG->security effects
 * @return 0 on success, -1 on error
 */
int security_kg_get_kg_to_sec_effects(
    const security_kg_bridge_t* bridge,
    kg_to_sec_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * WHAT: Retrieve current bridge operational state
 * WHY:  Monitor bridge health and connections
 * HOW:  Copy current state to output structure
 *
 * @param bridge Bridge handle
 * @param state Output: bridge state
 * @return 0 on success, -1 on error
 */
int security_kg_get_state(
    const security_kg_bridge_t* bridge,
    sec_kg_bridge_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve cumulative bridge statistics
 * WHY:  Monitor security effectiveness
 * HOW:  Copy current statistics to output structure
 *
 * @param bridge Bridge handle
 * @param stats Output: bridge statistics
 * @return 0 on success, -1 on error
 */
int security_kg_get_stats(
    const security_kg_bridge_t* bridge,
    sec_kg_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Reset all cumulative statistics to zero
 * WHY:  Start fresh measurement period
 * HOW:  Zero out statistics structure
 *
 * @param bridge Bridge handle
 */
void security_kg_reset_stats(
    security_kg_bridge_t* bridge
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get query result name
 *
 * @param result Query result code
 * @return Human-readable name string
 */
const char* security_kg_query_result_name(
    sec_kg_query_result_t result
);

/**
 * @brief Get traversal result name
 *
 * @param result Traversal result code
 * @return Human-readable name string
 */
const char* security_kg_traversal_result_name(
    sec_kg_traversal_result_t result
);

/**
 * @brief Get integrity result name
 *
 * @param result Integrity result code
 * @return Human-readable name string
 */
const char* security_kg_integrity_result_name(
    sec_kg_integrity_result_t result
);

/**
 * @brief Get consistency result name
 *
 * @param result Consistency result code
 * @return Human-readable name string
 */
const char* security_kg_consistency_result_name(
    sec_kg_consistency_result_t result
);

/**
 * @brief Get privacy level name
 *
 * @param level Privacy level code
 * @return Human-readable name string
 */
const char* security_kg_privacy_level_name(
    sec_kg_privacy_level_t level
);

/**
 * @brief Get state name
 *
 * @param state Bridge state code
 * @return Human-readable name string
 */
const char* security_kg_state_name(
    sec_kg_state_t state
);

/**
 * @brief Report false positive
 *
 * WHAT: Report that a detection was a false positive
 * WHY:  Enable precision tracking and threshold adjustment
 * HOW:  Increment false positive counter
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int security_kg_report_false_positive(
    security_kg_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_KNOWLEDGE_GRAPH_BRIDGE_H */
