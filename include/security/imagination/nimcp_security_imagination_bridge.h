/**
 * @file nimcp_security_imagination_bridge.h
 * @brief Security Module - Imagination System Integration Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Bidirectional integration between security module and imagination system
 * WHY:  Imagination/hypothetical reasoning requires security oversight to prevent
 *       confabulation, adversarial amplification, and reality disconnection
 * HOW:  Workspace sandboxing, confabulation detection, reasoning bounds,
 *       reality grounding, and simulation integrity verification
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PREFRONTAL-HIPPOCAMPAL-IMAGINATION AXIS:
 * ----------------------------------------
 * The security-imagination bridge models the interaction between:
 *
 * 1. Imagination System (Hippocampal-Prefrontal Network):
 *    - Hypothetical scenario generation
 *    - Mental simulation and counterfactual reasoning
 *    - Future prediction and planning
 *
 * 2. Security Surveillance (Reality Testing Network):
 *    - Reality grounding checks (anterior cingulate cortex)
 *    - Confabulation detection (ventromedial prefrontal cortex)
 *    - Source monitoring (medial temporal lobe)
 *
 * SECURITY CONSIDERATIONS:
 * ------------------------
 * - Workspace sandboxing prevents imagination from affecting real system state
 * - Confabulation detection identifies false memories/facts being generated
 * - Reasoning bounds limit hypothetical depth to prevent runaway simulation
 * - Reality grounding ensures imagination stays connected to factual reality
 * - Simulation integrity prevents adversarial manipulation of simulations
 *
 * ARCHITECTURE:
 * ```
 * +===========================================================================+
 * |                SECURITY-IMAGINATION BRIDGE                                 |
 * +===========================================================================+
 * |                                                                            |
 * |   +--------------------------------------------------------------------+   |
 * |   |            SECURITY -> IMAGINATION EFFECTS                         |   |
 * |   |                                                                    |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |   | Workspace      |   |   Reasoning    |   |    Simulation    |   |   |
 * |   |   | Sandboxing     |-->|   Bounds       |-->|    Limits        |   |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |          |                    |                      |             |   |
 * |   |          v                    v                      v             |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |   |              IMAGINATION ENGINE / WORKSPACE              |     |   |
 * |   |   |   - Sandboxed scenario execution                         |     |   |
 * |   |   |   - Confabulation filtering                              |     |   |
 * |   |   |   - Reality grounding verification                       |     |   |
 * |   |   |   - Simulation integrity checks                          |     |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |                                                                    |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                            |
 * |   +--------------------------------------------------------------------+   |
 * |   |            IMAGINATION -> SECURITY EFFECTS                         |   |
 * |   |                                                                    |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |   |              IMAGINATION ENGINE / WORKSPACE              |     |   |
 * |   |   |   - Confabulation patterns detected                      |     |   |
 * |   |   |   - Reality divergence metrics                           |     |   |
 * |   |   |   - Simulation resource consumption                      |     |   |
 * |   |   |   - Adversarial pattern indicators                       |     |   |
 * |   |   +---------------------------------------------------------+     |   |
 * |   |          |                    |                      |             |   |
 * |   |          v                    v                      v             |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |   | Threat         |   |   Integrity    |   |    Audit         |   |   |
 * |   |   | Assessment     |<--|   Scoring      |<--|    Logging       |   |   |
 * |   |   +----------------+   +----------------+   +------------------+   |   |
 * |   |                                                                    |   |
 * |   +--------------------------------------------------------------------+   |
 * |                                                                            |
 * +===========================================================================+
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - bridge_base_t as first member
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SECURITY_IMAGINATION_BRIDGE_H
#define NIMCP_SECURITY_IMAGINATION_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Base bridge infrastructure */
#include "utils/bridge/nimcp_bridge_base.h"

/* Common utilities */
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Magic Numbers
 * ============================================================================ */

/** @brief Magic number for bridge validation */
#define NIMCP_SECURITY_IMAGINATION_BRIDGE_MAGIC 0x53494D42  /* 'SIMB' */

/** @brief Bridge version */
#define NIMCP_SECURITY_IMAGINATION_BRIDGE_VERSION 0x0100

/** @brief Maximum simulation/scenario name length */
#define SECURITY_IMAGINATION_MAX_SIM_NAME 64

/** @brief Maximum concurrent sandboxed workspaces */
#define SECURITY_IMAGINATION_MAX_SANDBOXES 16

/** @brief Maximum hypothetical depth allowed */
#define SECURITY_IMAGINATION_DEFAULT_MAX_DEPTH 8

/** @brief Default confabulation threshold [0-1] */
#define SECURITY_IMAGINATION_DEFAULT_CONFAB_THRESHOLD 0.7f

/** @brief Default reality divergence threshold [0-1] */
#define SECURITY_IMAGINATION_DEFAULT_DIVERGENCE_THRESHOLD 0.8f

/** @brief Maximum tracked confabulation patterns */
#define SECURITY_IMAGINATION_MAX_CONFAB_PATTERNS 64

/** @brief Default simulation resource budget */
#define SECURITY_IMAGINATION_DEFAULT_SIM_BUDGET 500000

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

struct imagination_engine;
struct imagination_workspace;
struct imagination_scenario;

/* ============================================================================
 * Security Flags and Enumerations
 * ============================================================================ */

/**
 * @brief Security flags for imagination operations
 */
typedef enum {
    SECURITY_IMAGINATION_FLAG_NONE             = 0,
    SECURITY_IMAGINATION_FLAG_SANDBOXED        = (1 << 0),  /**< Workspace is sandboxed */
    SECURITY_IMAGINATION_FLAG_GROUNDED         = (1 << 1),  /**< Reality grounding verified */
    SECURITY_IMAGINATION_FLAG_BOUNDED          = (1 << 2),  /**< Reasoning bounds enforced */
    SECURITY_IMAGINATION_FLAG_INTEGRITY_OK     = (1 << 3),  /**< Simulation integrity verified */
    SECURITY_IMAGINATION_FLAG_CONFAB_DETECTED  = (1 << 4),  /**< Confabulation detected */
    SECURITY_IMAGINATION_FLAG_DIVERGED         = (1 << 5),  /**< Reality divergence detected */
    SECURITY_IMAGINATION_FLAG_ADVERSARIAL      = (1 << 6),  /**< Adversarial pattern detected */
    SECURITY_IMAGINATION_FLAG_RESOURCE_LIMITED = (1 << 7),  /**< Resource limits applied */
    SECURITY_IMAGINATION_FLAG_BLOCKED          = (1 << 8),  /**< Operation was blocked */
    SECURITY_IMAGINATION_FLAG_DEPTH_LIMITED    = (1 << 9),  /**< Depth limit was hit */
    SECURITY_IMAGINATION_FLAG_QUARANTINED      = (1 << 10)  /**< Scenario was quarantined */
} security_imagination_flags_t;

/**
 * @brief Validation result codes for imagination operations
 */
typedef enum {
    SECURITY_IMAGINATION_VALID = 0,          /**< Validation passed */
    SECURITY_IMAGINATION_INVALID_SCENARIO,   /**< Invalid scenario */
    SECURITY_IMAGINATION_CONFABULATION,      /**< Confabulation detected */
    SECURITY_IMAGINATION_REALITY_DIVERGED,   /**< Reality divergence exceeded */
    SECURITY_IMAGINATION_DEPTH_EXCEEDED,     /**< Hypothetical depth exceeded */
    SECURITY_IMAGINATION_RESOURCE_EXCEEDED,  /**< Resource budget exceeded */
    SECURITY_IMAGINATION_INTEGRITY_FAILED,   /**< Simulation integrity failed */
    SECURITY_IMAGINATION_ADVERSARIAL,        /**< Adversarial pattern detected */
    SECURITY_IMAGINATION_SANDBOX_BREACH,     /**< Sandbox isolation breached */
    SECURITY_IMAGINATION_GROUNDING_FAILED,   /**< Reality grounding failed */
    SECURITY_IMAGINATION_BLOCKED             /**< Operation was blocked */
} security_imagination_validation_t;

/**
 * @brief Confabulation type classification
 */
typedef enum {
    CONFAB_TYPE_NONE = 0,                /**< No confabulation */
    CONFAB_TYPE_FALSE_MEMORY,            /**< False memory generation */
    CONFAB_TYPE_FACT_INVENTION,          /**< Inventing non-existent facts */
    CONFAB_TYPE_SOURCE_CONFUSION,        /**< Confusion of information sources */
    CONFAB_TYPE_TEMPORAL_DISTORTION,     /**< Timeline distortion */
    CONFAB_TYPE_CAUSAL_FABRICATION,      /**< Fabricated causal relationships */
    CONFAB_TYPE_IDENTITY_CONFUSION,      /**< Entity/identity confusion */
    CONFAB_TYPE_HALLUCINATION            /**< Complete hallucination */
} confabulation_type_t;

/**
 * @brief Sandbox isolation level
 */
typedef enum {
    SANDBOX_LEVEL_NONE = 0,              /**< No sandboxing */
    SANDBOX_LEVEL_MINIMAL,               /**< Minimal isolation */
    SANDBOX_LEVEL_STANDARD,              /**< Standard isolation */
    SANDBOX_LEVEL_STRICT,                /**< Strict isolation */
    SANDBOX_LEVEL_MAXIMUM                /**< Maximum isolation */
} sandbox_isolation_level_t;

/* ============================================================================
 * Configuration Structure
 * ============================================================================ */

/**
 * @brief Security-imagination bridge configuration
 *
 * WHAT: Configuration for security-imagination integration
 * WHY:  Enable fine-grained control over imagination security features
 * HOW:  Boolean flags and threshold values for each feature
 */
typedef struct {
    /* Feature enables */
    bool enable_workspace_sandboxing;      /**< Sandbox imagination workspaces */
    bool enable_confabulation_detection;   /**< Detect confabulation patterns */
    bool enable_reasoning_bounds;          /**< Enforce hypothetical depth limits */
    bool enable_reality_grounding;         /**< Verify reality connection */
    bool enable_simulation_integrity;      /**< Verify simulation integrity */
    bool enable_adversarial_detection;     /**< Detect adversarial patterns */
    bool enable_resource_tracking;         /**< Track simulation resources */
    bool enable_audit_logging;             /**< Log imagination operations */

    /* Sandbox configuration */
    sandbox_isolation_level_t default_isolation_level;  /**< Default sandbox level */
    uint32_t max_sandboxed_workspaces;     /**< Maximum concurrent sandboxes */

    /* Reasoning limits */
    uint32_t max_hypothetical_depth;       /**< Maximum hypothetical depth */
    uint32_t max_branching_factor;         /**< Maximum branching per level */
    uint64_t max_simulation_steps;         /**< Maximum simulation steps */

    /* Detection thresholds */
    float confabulation_threshold;         /**< Confabulation detection threshold [0-1] */
    float reality_divergence_threshold;    /**< Reality divergence threshold [0-1] */
    float adversarial_threshold;           /**< Adversarial detection threshold [0-1] */
    float integrity_threshold;             /**< Integrity verification threshold [0-1] */

    /* Resource limits */
    uint64_t default_simulation_budget;    /**< Default resource budget */
    float resource_warning_threshold;      /**< Warn at this % of budget */
} security_imagination_config_t;

/* ============================================================================
 * Sandbox Entry Structure
 * ============================================================================ */

/**
 * @brief Sandbox entry for isolated workspace
 *
 * WHAT: Tracks a sandboxed imagination workspace
 * WHY:  Isolate imagination from affecting real state
 * HOW:  Store workspace reference, isolation level, resource usage
 */
typedef struct {
    uint64_t sandbox_id;                   /**< Unique sandbox ID */
    char scenario_name[SECURITY_IMAGINATION_MAX_SIM_NAME]; /**< Scenario name */
    struct imagination_workspace* workspace; /**< Workspace reference */
    sandbox_isolation_level_t isolation_level; /**< Isolation level */
    bool is_active;                        /**< Sandbox is active */
    uint64_t created_at_ms;                /**< Creation timestamp */
    uint64_t resources_used;               /**< Resources consumed */
    uint32_t current_depth;                /**< Current hypothetical depth */
    uint32_t max_depth_reached;            /**< Maximum depth reached */
    uint64_t simulation_steps;             /**< Simulation steps executed */
    float reality_divergence;              /**< Reality divergence score */
    float confabulation_score;             /**< Confabulation score */
    security_imagination_flags_t flags;    /**< Security flags */
} security_imagination_sandbox_t;

/* ============================================================================
 * Confabulation Detection Structure
 * ============================================================================ */

/**
 * @brief Confabulation detection result
 *
 * WHAT: Result of confabulation detection check
 * WHY:  Identify and classify false memory/fact generation
 * HOW:  Score, type, and evidence for detection
 */
typedef struct {
    bool detected;                         /**< Confabulation was detected */
    float score;                           /**< Confabulation score [0-1] */
    confabulation_type_t type;             /**< Type of confabulation */
    char description[256];                 /**< Human-readable description */
    uint64_t source_timestamp;             /**< When confabulation occurred */
    uint32_t affected_elements;            /**< Number of affected elements */
} security_imagination_confab_result_t;

/* ============================================================================
 * Reality Grounding Structure
 * ============================================================================ */

/**
 * @brief Reality grounding check result
 *
 * WHAT: Result of reality grounding verification
 * WHY:  Ensure imagination stays connected to factual reality
 * HOW:  Divergence score, anchor points, and violation details
 */
typedef struct {
    bool grounded;                         /**< Imagination is grounded */
    float divergence_score;                /**< Reality divergence [0-1] */
    uint32_t anchor_points_valid;          /**< Valid reality anchor points */
    uint32_t anchor_points_total;          /**< Total anchor points checked */
    uint32_t violations_count;             /**< Number of grounding violations */
    char primary_violation[256];           /**< Primary violation description */
} security_imagination_grounding_result_t;

/* ============================================================================
 * Simulation Integrity Structure
 * ============================================================================ */

/**
 * @brief Simulation integrity verification result
 *
 * WHAT: Result of simulation integrity check
 * WHY:  Detect tampering or corruption in simulation state
 * HOW:  Hash verification, consistency checks, anomaly detection
 */
typedef struct {
    bool integrity_valid;                  /**< Simulation integrity is valid */
    float integrity_score;                 /**< Integrity score [0-1] */
    bool hash_verified;                    /**< State hash verified */
    bool consistency_valid;                /**< Internal consistency valid */
    bool causality_preserved;              /**< Causal relationships preserved */
    uint32_t anomalies_detected;           /**< Number of anomalies found */
    char primary_anomaly[256];             /**< Primary anomaly description */
} security_imagination_integrity_result_t;

/* ============================================================================
 * Execution Result Structure
 * ============================================================================ */

/**
 * @brief Result of a security-wrapped imagination operation
 *
 * WHAT: Complete operation result with security metadata
 * WHY:  Track security state through imagination operation
 * HOW:  Populated by security layer before returning
 */
typedef struct {
    bool success;                          /**< Operation succeeded */
    security_imagination_validation_t validation; /**< Validation outcome */
    security_imagination_flags_t flags;    /**< Security flags set */
    uint64_t sandbox_id;                   /**< Sandbox ID if sandboxed */
    float confabulation_score;             /**< Confabulation score */
    float reality_divergence;              /**< Reality divergence score */
    float integrity_score;                 /**< Integrity score */
    uint32_t hypothetical_depth;           /**< Current hypothetical depth */
    uint64_t resources_used;               /**< Resources consumed */
    uint64_t execution_time_us;            /**< Execution duration */
    const char* error_message;             /**< Error description if failed */
} security_imagination_result_t;

/* ============================================================================
 * Bidirectional Effect Structures
 * ============================================================================ */

/**
 * @brief Security effects on imagination system
 *
 * WHAT: How security state modulates imagination behavior
 * WHY:  Active threats should restrict imagination capabilities
 * HOW:  Reduces depth, enforces sandboxing, applies limits
 */
typedef struct {
    /* Sandbox restrictions */
    uint32_t active_sandboxes;             /**< Number of active sandboxes */
    bool sandbox_quota_reached;            /**< Sandbox quota exhausted */
    sandbox_isolation_level_t min_required_level; /**< Minimum required isolation */

    /* Depth restrictions */
    uint32_t effective_max_depth;          /**< Current effective depth limit */
    float depth_reduction_factor;          /**< Depth reduction [0-1] */

    /* Resource restrictions */
    uint64_t effective_simulation_budget;  /**< Current effective budget */
    float resource_reduction_factor;       /**< Budget reduction [0-1] */

    /* Detection sensitivity */
    float effective_confab_threshold;      /**< Current confabulation threshold */
    float effective_divergence_threshold;  /**< Current divergence threshold */

    /* Lockdown state */
    bool imagination_restricted;           /**< Imagination capabilities restricted */
    bool new_scenarios_blocked;            /**< New scenario creation blocked */
} security_to_imagination_effects_t;

/**
 * @brief Imagination effects on security
 *
 * WHAT: How imagination activity informs security decisions
 * WHY:  Imagination patterns may indicate threats
 * HOW:  Reports usage, detects suspicious patterns
 */
typedef struct {
    /* Usage statistics */
    uint64_t total_scenarios;              /**< Total scenarios created */
    uint64_t active_scenarios;             /**< Currently active scenarios */
    uint64_t total_simulation_steps;       /**< Total simulation steps */
    float avg_scenario_depth;              /**< Average scenario depth */

    /* Confabulation tracking */
    uint32_t confabulations_detected;      /**< Total confabulations detected */
    uint32_t confabulations_blocked;       /**< Confabulations that were blocked */
    float peak_confabulation_score;        /**< Peak confabulation score */
    confabulation_type_t dominant_type;    /**< Most common confabulation type */

    /* Reality divergence */
    uint32_t divergence_violations;        /**< Reality divergence violations */
    float current_max_divergence;          /**< Current maximum divergence */
    float avg_divergence;                  /**< Average divergence score */

    /* Integrity tracking */
    uint32_t integrity_failures;           /**< Integrity check failures */
    uint32_t adversarial_detections;       /**< Adversarial pattern detections */
    float current_integrity_score;         /**< Current integrity score */

    /* Resource tracking */
    uint64_t total_resources_used;         /**< Total resources consumed */
    float resource_utilization;            /**< % of budget used */
    uint32_t resource_warnings;            /**< Budget warnings issued */
} imagination_to_security_effects_t;

/* ============================================================================
 * State and Statistics Structures
 * ============================================================================ */

/**
 * @brief Security-imagination bridge state
 */
typedef struct {
    /* Connection state */
    bool imagination_engine_connected;     /**< Engine connected */
    bool workspace_connected;              /**< Workspace connected */

    /* Operational state */
    bool is_active;                        /**< Bridge is active */
    bool imagination_restricted;           /**< Imagination restricted */
    bool new_scenarios_blocked;            /**< New scenarios blocked */

    /* Sandbox state */
    uint32_t active_sandbox_count;         /**< Active sandboxes */
    uint32_t max_sandbox_count;            /**< Maximum sandboxes reached */

    /* Detection state */
    uint32_t pending_confab_reviews;       /**< Pending confabulation reviews */
    float current_threat_level;            /**< Current imagination threat level */
} security_imagination_state_t;

/**
 * @brief Security-imagination bridge statistics
 */
typedef struct {
    /* Sandbox statistics */
    uint64_t sandboxes_created;            /**< Total sandboxes created */
    uint64_t sandboxes_destroyed;          /**< Sandboxes destroyed */
    uint64_t sandbox_breaches;             /**< Sandbox breaches detected */
    float avg_sandbox_duration_ms;         /**< Average sandbox duration */

    /* Confabulation statistics */
    uint64_t confab_checks;                /**< Confabulation checks performed */
    uint64_t confab_detections;            /**< Confabulations detected */
    uint64_t confab_blocked;               /**< Confabulations blocked */
    float avg_confab_score;                /**< Average confabulation score */

    /* Reality grounding statistics */
    uint64_t grounding_checks;             /**< Grounding checks performed */
    uint64_t grounding_failures;           /**< Grounding failures */
    float avg_divergence_score;            /**< Average divergence score */

    /* Integrity statistics */
    uint64_t integrity_checks;             /**< Integrity checks performed */
    uint64_t integrity_failures;           /**< Integrity failures */
    float avg_integrity_score;             /**< Average integrity score */

    /* Depth statistics */
    uint64_t depth_checks;                 /**< Depth limit checks */
    uint64_t depth_limit_hits;             /**< Depth limit hits */
    uint32_t max_depth_observed;           /**< Maximum depth observed */

    /* Resource statistics */
    uint64_t total_resources_consumed;     /**< Total resources consumed */
    uint64_t resource_limit_hits;          /**< Resource limit hits */
    float avg_resources_per_scenario;      /**< Average resources per scenario */

    /* Adversarial statistics */
    uint64_t adversarial_checks;           /**< Adversarial checks performed */
    uint64_t adversarial_detections;       /**< Adversarial patterns detected */
    float peak_adversarial_score;          /**< Peak adversarial score */

    /* Performance metrics */
    float avg_sandbox_overhead_us;         /**< Average sandbox overhead */
    float avg_validation_time_us;          /**< Average validation time */
} security_imagination_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Security-imagination bridge main structure
 *
 * WHAT: Main bridge connecting security module to imagination system
 * WHY:  Centralized security enforcement for imagination operations
 * HOW:  Contains config, effects, state, stats, and system handles
 */
typedef struct {
    bridge_base_t base;                    /**< MUST be first: base bridge */

    /* Configuration */
    security_imagination_config_t config;  /**< Bridge configuration */

    /* System connections */
    struct imagination_engine* imagination_engine; /**< Imagination engine */
    struct imagination_workspace* workspace;        /**< Imagination workspace */

    /* Sandbox management */
    security_imagination_sandbox_t* sandboxes; /**< Active sandboxes */
    size_t sandbox_count;                  /**< Current sandbox count */
    size_t sandbox_capacity;               /**< Sandbox array capacity */
    uint64_t next_sandbox_id;              /**< Next sandbox ID */

    /* Bidirectional effects */
    security_to_imagination_effects_t security_effects;   /**< Security -> Imagination */
    imagination_to_security_effects_t imagination_effects; /**< Imagination -> Security */

    /* State and statistics */
    security_imagination_state_t state;    /**< Current bridge state */
    security_imagination_stats_t stats;    /**< Bridge statistics */

    /* Current operation tracking */
    uint64_t current_scenario_id;          /**< Current scenario ID */
    uint32_t current_depth;                /**< Current hypothetical depth */
} security_imagination_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default security-imagination bridge configuration
 *
 * WHAT: Provide sensible defaults for security-imagination integration
 * WHY:  Easy initialization with security-focused defaults
 * HOW:  Return config with all features enabled, conservative limits
 *
 * @param config Output configuration structure
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_default_config(security_imagination_config_t* config);

/**
 * @brief Create security-imagination bridge
 *
 * WHAT: Initialize bidirectional security-imagination integration
 * WHY:  Enable security oversight of all imagination operations
 * HOW:  Allocate bridge, initialize state, prepare sandboxes
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return Bridge instance or NULL on failure
 */
security_imagination_bridge_t* security_imagination_bridge_create(
    const security_imagination_config_t* config
);

/**
 * @brief Destroy security-imagination bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free sandboxes, destroy base
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void security_imagination_bridge_destroy(security_imagination_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect imagination engine to bridge
 *
 * WHAT: Connect imagination engine for security oversight
 * WHY:  Engine manages scenario processing that security must monitor
 * HOW:  Store handle, register for engine events
 *
 * @param bridge Security-imagination bridge
 * @param engine Imagination engine to connect
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_connect_engine(
    security_imagination_bridge_t* bridge,
    struct imagination_engine* engine
);

/**
 * @brief Connect imagination workspace to bridge
 *
 * WHAT: Connect workspace for workspace-level security
 * WHY:  Workspace holds active scenarios requiring security oversight
 * HOW:  Store handle, enable workspace monitoring
 *
 * @param bridge Security-imagination bridge
 * @param workspace Imagination workspace to connect
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_connect_workspace(
    security_imagination_bridge_t* bridge,
    struct imagination_workspace* workspace
);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Security-imagination bridge
 * @return true if required systems connected
 */
bool security_imagination_is_connected(const security_imagination_bridge_t* bridge);

/* ============================================================================
 * Sandbox Management API
 * ============================================================================ */

/**
 * @brief Create sandboxed workspace for imagination scenario
 *
 * WHAT: Isolate imagination workspace from affecting real state
 * WHY:  Prevent imagination from corrupting real system state
 * HOW:  Create isolated copy, track resource usage
 *
 * @param bridge Security-imagination bridge
 * @param scenario_name Name for the scenario
 * @param isolation_level Desired isolation level
 * @param sandbox_id Output: assigned sandbox ID
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_sandbox_workspace(
    security_imagination_bridge_t* bridge,
    const char* scenario_name,
    sandbox_isolation_level_t isolation_level,
    uint64_t* sandbox_id
);

/**
 * @brief Release sandboxed workspace
 *
 * WHAT: Release sandbox and cleanup isolated state
 * WHY:  Free resources when scenario complete
 * HOW:  Destroy sandbox, update tracking
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to release
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_release_sandbox(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
);

/**
 * @brief Get sandbox information
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox ID to query
 * @param sandbox Output: sandbox information
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_get_sandbox(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_sandbox_t* sandbox
);

/* ============================================================================
 * Confabulation Detection API
 * ============================================================================ */

/**
 * @brief Detect confabulation in imagination content
 *
 * WHAT: Check imagination output for confabulated content
 * WHY:  Prevent false memories/facts from being treated as real
 * HOW:  Pattern matching, consistency checking, source verification
 *
 * @param bridge Security-imagination bridge
 * @param content Content to check
 * @param content_size Content size
 * @param result Output: detection result
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_detect_confabulation(
    security_imagination_bridge_t* bridge,
    const void* content,
    size_t content_size,
    security_imagination_confab_result_t* result
);

/**
 * @brief Check scenario for confabulation patterns
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to check
 * @param result Output: detection result
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_check_scenario_confabulation(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_confab_result_t* result
);

/* ============================================================================
 * Reasoning Bounds API
 * ============================================================================ */

/**
 * @brief Enforce hypothetical reasoning bounds
 *
 * WHAT: Check and enforce depth limits on hypothetical reasoning
 * WHY:  Prevent runaway recursive simulation
 * HOW:  Track depth, enforce configured limits
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox ID
 * @param requested_depth Requested hypothetical depth
 * @return true if depth is allowed, false if limit exceeded
 */
bool security_imagination_enforce_bounds(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint32_t requested_depth
);

/**
 * @brief Check current depth against limits
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to check
 * @param current_depth Current depth
 * @return true if within bounds, false if exceeded
 */
bool security_imagination_check_depth(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint32_t current_depth
);

/**
 * @brief Get effective maximum depth
 *
 * @param bridge Security-imagination bridge
 * @return Effective maximum hypothetical depth
 */
uint32_t security_imagination_get_max_depth(
    const security_imagination_bridge_t* bridge
);

/* ============================================================================
 * Reality Grounding API
 * ============================================================================ */

/**
 * @brief Verify imagination grounding to reality
 *
 * WHAT: Check that imagination stays connected to factual reality
 * WHY:  Prevent complete disconnection from real-world facts
 * HOW:  Compare against reality anchors, measure divergence
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to check
 * @param result Output: grounding result
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_ground_reality(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_grounding_result_t* result
);

/**
 * @brief Get current reality divergence score
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to check
 * @return Reality divergence score [0-1], negative on error
 */
float security_imagination_get_divergence(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
);

/* ============================================================================
 * Simulation Integrity API
 * ============================================================================ */

/**
 * @brief Verify simulation integrity
 *
 * WHAT: Check simulation state for tampering or corruption
 * WHY:  Detect adversarial manipulation of simulation state
 * HOW:  Hash verification, consistency checks, anomaly detection
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to verify
 * @param result Output: integrity result
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_verify_simulation(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    security_imagination_integrity_result_t* result
);

/**
 * @brief Check for adversarial patterns in simulation
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to check
 * @param adversarial_score Output: adversarial score [0-1]
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_check_adversarial(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    float* adversarial_score
);

/* ============================================================================
 * Resource Tracking API
 * ============================================================================ */

/**
 * @brief Track simulation resource usage
 *
 * WHAT: Record resource consumption for simulation
 * WHY:  Enforce resource budgets
 * HOW:  Add to running total, check against budget
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox consuming resources
 * @param resources_used Resources consumed
 * @return NIMCP_SUCCESS if within budget, error code if exceeded
 */
int security_imagination_track_resources(
    security_imagination_bridge_t* bridge,
    uint64_t sandbox_id,
    uint64_t resources_used
);

/**
 * @brief Get current resource usage for sandbox
 *
 * @param bridge Security-imagination bridge
 * @param sandbox_id Sandbox to query
 * @return Resources used, 0 on error
 */
uint64_t security_imagination_get_resources(
    const security_imagination_bridge_t* bridge,
    uint64_t sandbox_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update security effects on imagination (outbound)
 *
 * WHAT: Compute how security state modulates imagination
 * WHY:  Security threats should restrict imagination
 * HOW:  Aggregate security state, compute restrictions
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_update_security_effects(
    security_imagination_bridge_t* bridge
);

/**
 * @brief Update imagination effects on security (inbound)
 *
 * WHAT: Process imagination activity for security analysis
 * WHY:  Imagination patterns may indicate threats
 * HOW:  Aggregate statistics, compute scores
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_update_imagination_effects(
    security_imagination_bridge_t* bridge
);

/**
 * @brief Full update cycle (both directions)
 *
 * WHAT: Execute complete bidirectional update
 * WHY:  Single call for regular update loops
 * HOW:  Update both directions, process pending items
 *
 * @param bridge Security-imagination bridge
 * @param delta_ms Time since last update
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_bridge_update(
    security_imagination_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get security effects on imagination
 *
 * @param bridge Security-imagination bridge
 * @param effects Output effects structure
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_get_security_effects(
    const security_imagination_bridge_t* bridge,
    security_to_imagination_effects_t* effects
);

/**
 * @brief Get imagination effects on security
 *
 * @param bridge Security-imagination bridge
 * @param effects Output effects structure
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_get_imagination_effects(
    const security_imagination_bridge_t* bridge,
    imagination_to_security_effects_t* effects
);

/**
 * @brief Get bridge state
 *
 * @param bridge Security-imagination bridge
 * @param state Output state structure
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_get_state(
    const security_imagination_bridge_t* bridge,
    security_imagination_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Security-imagination bridge
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_get_stats(
    const security_imagination_bridge_t* bridge,
    security_imagination_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_reset_stats(security_imagination_bridge_t* bridge);

/* ============================================================================
 * Restriction Mode API
 * ============================================================================ */

/**
 * @brief Restrict imagination capabilities
 *
 * WHAT: Reduce imagination capabilities due to security concern
 * WHY:  Detected threat requires limiting imagination
 * HOW:  Set restriction flag, reduce limits
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_enter_restricted(security_imagination_bridge_t* bridge);

/**
 * @brief Restore normal imagination capabilities
 *
 * WHAT: Restore normal imagination operation
 * WHY:  Threat has passed
 * HOW:  Clear restriction flag, restore limits
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_exit_restricted(security_imagination_bridge_t* bridge);

/**
 * @brief Check if imagination is restricted
 *
 * @param bridge Security-imagination bridge
 * @return true if restricted
 */
bool security_imagination_is_restricted(
    const security_imagination_bridge_t* bridge
);

/**
 * @brief Block creation of new scenarios
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_block_new_scenarios(
    security_imagination_bridge_t* bridge
);

/**
 * @brief Allow creation of new scenarios
 *
 * @param bridge Security-imagination bridge
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int security_imagination_allow_new_scenarios(
    security_imagination_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SECURITY_IMAGINATION_BRIDGE_H */
