/**
 * @file nimcp_rcog_health.h
 * @brief Recursive Cognition Health Integration - Intelligent Diagnosis and Recovery
 * @version 1.0.0
 * @date 2026-01-18
 *
 * WHAT: Integration of recursive cognition engine for intelligent health diagnosis
 * WHY:  Enable complex multi-step reasoning about health issues and recovery planning
 * HOW:  RCOG decomposes health goals, delegates investigation, and synthesizes answers
 *
 * PHASE 8: Section 27 - Collective & Recursive Cognition Integration
 *
 * KEY FEATURES:
 * 1. Health Goal Submission - Submit health-specific goals to RCOG engine
 * 2. Intelligent Diagnosis - Multi-step root cause analysis
 * 3. Recovery Planning - Recursive decomposition for complex recovery strategies
 * 4. Health Tools - Register health-specific tools for RCOG
 *
 * PROCESSING FLOW:
 * ```
 * +----------------------------------------------------------------------+
 * |                     RCOG HEALTH PROCESSING                           |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |  GOAL: "Diagnose and recover from anomaly X"                        |
 * |       |                                                              |
 * |       v                                                              |
 * |  +----------------+                                                  |
 * |  | Orchestrator   |  Decomposes into subtasks:                      |
 * |  | (Decompose)    |  - Verify anomaly exists                        |
 * |  +----------------+  - Analyze recent metrics                       |
 * |       |              - Check related components                     |
 * |       v              - Identify root cause                          |
 * |  +----------------+                                                  |
 * |  | Delegation     |  Executes subtasks in parallel:                 |
 * |  | Pool           |  [SCAN_CORRUPTION] [ANALYZE_METRICS]           |
 * |  +----------------+  [CHECK_MEMORY] [PREDICT_FAILURE]              |
 * |       |                                                              |
 * |       v                                                              |
 * |  +----------------+                                                  |
 * |  | Tool Router    |  Routes to health tools:                        |
 * |  |                |  - Memory analyzer                              |
 * |  +----------------+  - Deadlock detector                            |
 * |       |              - Tensor validator                             |
 * |       v                                                              |
 * |  +----------------+                                                  |
 * |  | Answer Refiner |  Combines results:                              |
 * |  |                |  "Root cause: Buffer overflow in KG expansion"  |
 * |  +----------------+  "Recovery: Quarantine + restore checkpoint"   |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RCOG_HEALTH_H
#define NIMCP_RCOG_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include dependencies */
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum evidence items in health answer */
#define RCOG_HEALTH_MAX_EVIDENCE            8

/** Maximum recovery plan length */
#define RCOG_HEALTH_MAX_PLAN_SIZE           1024

/** Default RCOG health timeout (ms) */
#define RCOG_HEALTH_DEFAULT_TIMEOUT_MS      30000

/** Default confidence threshold */
#define RCOG_HEALTH_DEFAULT_CONFIDENCE      0.8f

/** Maximum recursion depth for health reasoning */
#define RCOG_HEALTH_MAX_RECURSION_DEPTH     5

/* ============================================================================
 * Health Goal Types
 * ============================================================================ */

/**
 * @brief Health-specific goal types for RCOG
 */
typedef enum {
    /** Diagnose root cause of an anomaly */
    RCOG_HEALTH_GOAL_DIAGNOSE = 0,

    /** Plan recovery from anomaly */
    RCOG_HEALTH_GOAL_PLAN_RECOVERY,

    /** Predict potential failures */
    RCOG_HEALTH_GOAL_PREDICT_FAILURE,

    /** Verify system health state */
    RCOG_HEALTH_GOAL_VERIFY_HEALTH,

    /** Optimize health monitoring */
    RCOG_HEALTH_GOAL_OPTIMIZE_MONITORING,

    /** Analyze patterns in health data */
    RCOG_HEALTH_GOAL_ANALYZE_PATTERNS,

    RCOG_HEALTH_GOAL_COUNT
} rcog_health_goal_type_t;

/**
 * @brief Health goal for recursive processing
 */
typedef struct {
    /** Base RCOG goal type */
    rcog_goal_type_t base_type;

    /** Health-specific goal type */
    rcog_health_goal_type_t health_type;

    /** Query/goal description */
    char query[512];

    /* Health-specific context */
    /** Anomaly to diagnose/recover (if applicable) */
    health_agent_msg_type_t anomaly_type;
    health_agent_source_t anomaly_source;
    health_agent_severity_t anomaly_severity;

    /** Additional context data */
    uint8_t context_data[256];
    size_t context_size;

    /** Timestamp of anomaly */
    uint64_t anomaly_timestamp_us;

    /* Processing parameters */
    /** Minimum confidence for answer (0.0-1.0) */
    float confidence_threshold;

    /** Maximum decomposition depth */
    uint32_t max_recursion_depth;

    /** Enable imagination for what-if scenarios */
    bool enable_imagination;

    /** Timeout (ms, 0 = use default) */
    uint32_t timeout_ms;

} rcog_health_goal_t;

/* ============================================================================
 * Health Answer Types
 * ============================================================================ */

/**
 * @brief Diagnosis result from RCOG
 */
typedef struct {
    /** Root cause source component */
    health_agent_source_t root_cause_source;

    /** Description of root cause */
    char root_cause_description[256];

    /** Diagnosis confidence (0.0-1.0) */
    float diagnosis_confidence;

    /** Whether diagnosis is certain */
    bool is_certain;

    /** Alternative possible causes */
    char alternative_causes[3][128];
    uint32_t num_alternatives;

} rcog_health_diagnosis_t;

/**
 * @brief Recovery plan from RCOG
 */
typedef struct {
    /** Primary recommended action */
    health_agent_recovery_t primary_action;

    /** Fallback action if primary fails */
    health_agent_recovery_t fallback_action;

    /** Detailed recovery plan description */
    char recovery_plan[RCOG_HEALTH_MAX_PLAN_SIZE];

    /** Success probability estimate (0.0-1.0) */
    float success_probability;

    /** Estimated recovery time (ms) */
    uint32_t estimated_recovery_time_ms;

    /** Whether immediate action is required */
    bool requires_immediate_action;

    /** Risk level of recovery action */
    health_agent_severity_t action_risk;

    /** Recovery steps as list */
    char recovery_steps[8][128];
    uint32_t num_steps;

} rcog_health_recovery_plan_t;

/**
 * @brief Evidence item supporting diagnosis/recovery
 */
typedef struct {
    /** Evidence description */
    char description[256];

    /** Confidence in this evidence (0.0-1.0) */
    float confidence;

    /** Source of evidence */
    health_agent_source_t source;

    /** Evidence type (metric, observation, inference) */
    char evidence_type[32];

} rcog_health_evidence_t;

/**
 * @brief Complete health answer from RCOG
 */
typedef struct {
    /** Whether processing succeeded */
    bool success;

    /** Error message if failed */
    char error_message[256];

    /** Overall confidence in answer (0.0-1.0) */
    float overall_confidence;

    /** Diagnosis results */
    rcog_health_diagnosis_t diagnosis;

    /** Recovery plan */
    rcog_health_recovery_plan_t recovery;

    /** Supporting evidence */
    rcog_health_evidence_t evidence[RCOG_HEALTH_MAX_EVIDENCE];
    uint32_t num_evidence;

    /** Processing statistics */
    uint32_t subtasks_executed;
    uint32_t max_depth_used;
    uint32_t refinement_iterations;
    uint64_t processing_time_ms;

    /** Goal that was processed */
    rcog_health_goal_t goal;

} rcog_health_answer_t;

/* ============================================================================
 * Health Tool Definitions
 * ============================================================================ */

/**
 * @brief Health tool definition for RCOG
 */
typedef struct {
    /** Tool name */
    const char* tool_name;

    /** Description for RCOG orchestrator */
    const char* description;

    /** Tool handler function */
    int (*invoke)(const void* params, void* result, void* context);

    /** Tool context */
    void* context;

    /** Capability tier required */
    rcog_capability_tier_t required_tier;

} rcog_health_tool_t;

/**
 * @brief Pre-defined health tool IDs
 */
typedef enum {
    /** Check memory for corruption/leaks */
    RCOG_TOOL_ID_CHECK_MEMORY = 0,

    /** Check tensors for NaN/Inf */
    RCOG_TOOL_ID_CHECK_TENSORS,

    /** Detect deadlocks */
    RCOG_TOOL_ID_CHECK_DEADLOCKS,

    /** Analyze metrics history */
    RCOG_TOOL_ID_ANALYZE_METRICS,

    /** Predict failure probability */
    RCOG_TOOL_ID_PREDICT_FAILURE,

    /** Suggest recovery actions */
    RCOG_TOOL_ID_SUGGEST_RECOVERY,

    /** Verify checksum integrity */
    RCOG_TOOL_ID_VERIFY_CHECKSUM,

    /** Scan for corruption */
    RCOG_TOOL_ID_SCAN_CORRUPTION,

    /** Get system resource usage */
    RCOG_TOOL_ID_GET_RESOURCES,

    /** Query knowledge graph */
    RCOG_TOOL_ID_QUERY_KG,

    /** Check component connectivity */
    RCOG_TOOL_ID_CHECK_CONNECTIVITY,

    /** Analyze thread states */
    RCOG_TOOL_ID_ANALYZE_THREADS,

    RCOG_TOOL_ID_COUNT
} rcog_health_tool_id_t;

/* ============================================================================
 * RCOG Health Integration Handle
 * ============================================================================ */

/** RCOG health integration handle */
typedef struct rcog_health_integration rcog_health_integration_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief RCOG health integration configuration
 */
typedef struct {
    /** Default confidence threshold */
    float confidence_threshold;

    /** Default timeout (ms) */
    uint32_t default_timeout_ms;

    /** Maximum recursion depth */
    uint32_t max_recursion_depth;

    /** Enable imagination for planning */
    bool enable_imagination;

    /** Enable caching of diagnoses */
    bool enable_diagnosis_cache;

    /** Cache TTL (ms) */
    uint32_t cache_ttl_ms;

    /** Register builtin health tools */
    bool register_builtin_tools;

    /** Enable async processing */
    bool enable_async;

    /** Maximum concurrent health goals */
    uint32_t max_concurrent_goals;

} rcog_health_config_t;

/**
 * @brief Get default RCOG health configuration
 * @return Default configuration
 */
rcog_health_config_t rcog_health_default_config(void);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create RCOG health integration
 *
 * @param engine RCOG engine
 * @param health_agent Health agent for context
 * @param config Configuration (NULL for defaults)
 * @return Integration handle or NULL on failure
 */
rcog_health_integration_t* rcog_health_create(
    rcog_engine_t* engine,
    nimcp_health_agent_t* health_agent,
    const rcog_health_config_t* config
);

/**
 * @brief Destroy RCOG health integration
 * @param integration Integration to destroy
 */
void rcog_health_destroy(rcog_health_integration_t* integration);

/* ============================================================================
 * Goal Submission API
 * ============================================================================ */

/**
 * @brief Submit health goal to RCOG (synchronous)
 *
 * @param integration RCOG health integration
 * @param goal Health goal
 * @param answer Output answer
 * @return 0 on success, -1 on error
 */
int rcog_health_submit_goal(
    rcog_health_integration_t* integration,
    const rcog_health_goal_t* goal,
    rcog_health_answer_t* answer
);

/**
 * @brief Submit health goal to RCOG (asynchronous)
 *
 * @param integration RCOG health integration
 * @param goal Health goal
 * @param goal_id Output goal ID for tracking
 * @return 0 on success, -1 on error
 */
int rcog_health_submit_goal_async(
    rcog_health_integration_t* integration,
    const rcog_health_goal_t* goal,
    uint64_t* goal_id
);

/**
 * @brief Get health answer (blocking)
 *
 * @param integration RCOG health integration
 * @param goal_id Goal ID
 * @param answer Output answer
 * @param timeout_ms Timeout (0 = infinite)
 * @return 0 on success, -1 on error/timeout
 */
int rcog_health_get_answer(
    rcog_health_integration_t* integration,
    uint64_t goal_id,
    rcog_health_answer_t* answer,
    uint32_t timeout_ms
);

/**
 * @brief Check if health goal is complete
 *
 * @param integration RCOG health integration
 * @param goal_id Goal ID
 * @return true if complete
 */
bool rcog_health_is_complete(
    const rcog_health_integration_t* integration,
    uint64_t goal_id
);

/**
 * @brief Cancel pending health goal
 *
 * @param integration RCOG health integration
 * @param goal_id Goal ID
 * @return 0 on success, -1 on error
 */
int rcog_health_cancel_goal(
    rcog_health_integration_t* integration,
    uint64_t goal_id
);

/* ============================================================================
 * Convenience API
 * ============================================================================ */

/**
 * @brief Quick diagnosis of anomaly
 *
 * Convenience function that creates goal and submits for diagnosis.
 *
 * @param integration RCOG health integration
 * @param anomaly_type Type of anomaly
 * @param source Source of anomaly
 * @param severity Severity level
 * @param answer Output answer
 * @return 0 on success, -1 on error
 */
int rcog_health_diagnose_anomaly(
    rcog_health_integration_t* integration,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_answer_t* answer
);

/**
 * @brief Quick recovery planning
 *
 * @param integration RCOG health integration
 * @param anomaly_type Type of anomaly to recover from
 * @param source Source of anomaly
 * @param severity Severity level
 * @param plan Output recovery plan
 * @return 0 on success, -1 on error
 */
int rcog_health_plan_recovery(
    rcog_health_integration_t* integration,
    health_agent_msg_type_t anomaly_type,
    health_agent_source_t source,
    health_agent_severity_t severity,
    rcog_health_recovery_plan_t* plan
);

/**
 * @brief Predict failure for source
 *
 * @param integration RCOG health integration
 * @param source Source to analyze
 * @param failure_probability Output failure probability
 * @param time_to_failure_ms Output estimated time to failure
 * @return 0 on success, -1 on error
 */
int rcog_health_predict_failure(
    rcog_health_integration_t* integration,
    health_agent_source_t source,
    float* failure_probability,
    uint32_t* time_to_failure_ms
);

/* ============================================================================
 * Tool Management API
 * ============================================================================ */

/**
 * @brief Register health tools with RCOG
 *
 * Registers all builtin health tools with the RCOG tool router.
 *
 * @param integration RCOG health integration
 * @return 0 on success, -1 on error
 */
int rcog_health_register_builtin_tools(rcog_health_integration_t* integration);

/**
 * @brief Register custom health tool
 *
 * @param integration RCOG health integration
 * @param tool Tool definition
 * @return 0 on success, -1 on error
 */
int rcog_health_register_tool(
    rcog_health_integration_t* integration,
    const rcog_health_tool_t* tool
);

/**
 * @brief Unregister health tool
 *
 * @param integration RCOG health integration
 * @param tool_name Tool name
 * @return 0 on success, -1 on error
 */
int rcog_health_unregister_tool(
    rcog_health_integration_t* integration,
    const char* tool_name
);

/**
 * @brief Get builtin tool definition
 *
 * @param tool_id Builtin tool ID
 * @return Tool definition or NULL if not found
 */
const rcog_health_tool_t* rcog_health_get_builtin_tool(rcog_health_tool_id_t tool_id);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief RCOG health statistics
 */
typedef struct {
    /** Goals submitted */
    uint64_t goals_submitted;

    /** Goals completed successfully */
    uint64_t goals_completed;

    /** Goals failed */
    uint64_t goals_failed;

    /** Goals cancelled */
    uint64_t goals_cancelled;

    /** Goals timed out */
    uint64_t goals_timeout;

    /** Average processing time (ms) */
    float avg_processing_time_ms;

    /** Average confidence */
    float avg_confidence;

    /** Cache hits */
    uint64_t cache_hits;

    /** Cache misses */
    uint64_t cache_misses;

    /** Tools invoked */
    uint64_t tools_invoked;

    /** Maximum recursion depth used */
    uint32_t max_depth_used;

    /** Currently active goals */
    uint32_t active_goals;

} rcog_health_stats_t;

/**
 * @brief Get RCOG health statistics
 *
 * @param integration RCOG health integration
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int rcog_health_get_stats(
    const rcog_health_integration_t* integration,
    rcog_health_stats_t* stats
);

/**
 * @brief Reset RCOG health statistics
 * @param integration RCOG health integration
 */
void rcog_health_reset_stats(rcog_health_integration_t* integration);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get health goal type name
 * @param type Goal type
 * @return Human-readable name
 */
const char* rcog_health_goal_type_name(rcog_health_goal_type_t type);

/**
 * @brief Get health tool ID name
 * @param tool_id Tool ID
 * @return Human-readable name
 */
const char* rcog_health_tool_id_name(rcog_health_tool_id_t tool_id);

/**
 * @brief Initialize health goal with defaults
 * @param goal Goal to initialize
 */
void rcog_health_init_goal(rcog_health_goal_t* goal);

/**
 * @brief Initialize health answer
 * @param answer Answer to initialize
 */
void rcog_health_init_answer(rcog_health_answer_t* answer);

/**
 * @brief Free health answer resources
 * @param answer Answer to free
 */
void rcog_health_free_answer(rcog_health_answer_t* answer);

/**
 * @brief Dump health answer for debugging
 * @param answer Answer to dump
 */
void rcog_health_dump_answer(const rcog_health_answer_t* answer);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_HEALTH_H */
