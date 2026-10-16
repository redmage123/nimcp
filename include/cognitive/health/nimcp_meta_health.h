/**
 * @file nimcp_meta_health.h
 * @brief Meta-Health Self-Reflection - Health System Self-Improvement
 * @version 1.0.0
 * @date 2026-01-18
 *
 * WHAT: Self-reflection capabilities for the health monitoring system
 * WHY:  Enable the health system to learn from its own performance and improve
 * HOW:  Recursive cognition analyzes past decisions and suggests improvements
 *
 * PHASE 8: Section 27 - Collective & Recursive Cognition Integration
 *
 * KEY FEATURES:
 * 1. Decision History - Track past health decisions and outcomes
 * 2. Performance Analysis - Evaluate accuracy and effectiveness
 * 3. Pattern Discovery - Identify recurring issues and solutions
 * 4. Parameter Tuning - Suggest threshold and configuration adjustments
 * 5. Continuous Learning - Apply learnings to improve future decisions
 *
 * SELF-REFLECTION PROCESS:
 * ```
 * +----------------------------------------------------------------------+
 * |                    META-HEALTH REFLECTION CYCLE                      |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |  1. COLLECT DECISIONS                                                |
 * |     +----------------------------------------------------------+    |
 * |     | Decision Log: anomaly detected -> action taken -> outcome|    |
 * |     | - Memory corruption -> Quarantine -> Success (500ms)     |    |
 * |     | - Deadlock detected -> Restart thread -> Success (200ms) |    |
 * |     | - NaN propagation -> Clear NaN -> Failed (data loss)     |    |
 * |     +----------------------------------------------------------+    |
 * |                              |                                       |
 * |                              v                                       |
 * |  2. ANALYZE PERFORMANCE                                              |
 * |     +----------------------------------------------------------+    |
 * |     | Accuracy Rate: 85%    False Positive Rate: 5%            |    |
 * |     | Recovery Success: 90%  Avg Response Time: 150ms          |    |
 * |     | Weakest Area: NaN detection (65% accuracy)               |    |
 * |     +----------------------------------------------------------+    |
 * |                              |                                       |
 * |                              v                                       |
 * |  3. REFLECT (via RCOG)                                               |
 * |     +----------------------------------------------------------+    |
 * |     | Key Insight: NaN detection threshold too aggressive      |    |
 * |     | Recommendation: Increase threshold from 0.001 to 0.01    |    |
 * |     | New Pattern: Deadlocks correlate with high memory usage  |    |
 * |     +----------------------------------------------------------+    |
 * |                              |                                       |
 * |                              v                                       |
 * |  4. APPLY LEARNINGS                                                  |
 * |     +----------------------------------------------------------+    |
 * |     | Updated: NaN detection threshold                         |    |
 * |     | Added: Deadlock predictor based on memory usage          |    |
 * |     | Enhanced: Memory corruption response strategy            |    |
 * |     +----------------------------------------------------------+    |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_META_HEALTH_H
#define NIMCP_META_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Include dependencies */
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum past decisions to store for reflection */
#define META_HEALTH_MAX_DECISIONS               100

/** Maximum parameter adjustments per reflection */
#define META_HEALTH_MAX_ADJUSTMENTS             10

/** Maximum new patterns discovered per reflection */
#define META_HEALTH_MAX_NEW_PATTERNS            5

/** Default reflection interval (ms) */
#define META_HEALTH_DEFAULT_REFLECTION_INTERVAL_MS 300000  /* 5 minutes */

/** Minimum decisions for meaningful reflection */
#define META_HEALTH_MIN_DECISIONS_FOR_REFLECTION 10

/* ============================================================================
 * Decision History Types
 * ============================================================================ */

/**
 * @brief Outcome of a health decision
 */
typedef enum {
    /** Recovery fully succeeded */
    META_HEALTH_OUTCOME_SUCCESS = 0,

    /** Recovery partially succeeded */
    META_HEALTH_OUTCOME_PARTIAL_SUCCESS,

    /** Recovery failed */
    META_HEALTH_OUTCOME_FAILURE,

    /** Outcome unknown (still in progress or not tracked) */
    META_HEALTH_OUTCOME_UNKNOWN,

    /** False positive - no actual problem existed */
    META_HEALTH_OUTCOME_FALSE_POSITIVE,

    /** False negative - problem existed but was missed */
    META_HEALTH_OUTCOME_FALSE_NEGATIVE

} meta_health_outcome_t;

/**
 * @brief Record of a single health decision
 */
typedef struct {
    /** Timestamp of decision */
    uint64_t timestamp_us;

    /** Anomaly that triggered the decision */
    health_agent_msg_type_t anomaly_type;
    health_agent_source_t anomaly_source;
    health_agent_severity_t anomaly_severity;

    /** Detection confidence at decision time */
    float detection_confidence;

    /** Action taken */
    health_agent_recovery_t action_taken;

    /** Outcome of the action */
    meta_health_outcome_t outcome;

    /** Whether recovery succeeded */
    bool recovery_succeeded;

    /** Time to recovery (ms) */
    uint64_t time_to_recovery_ms;

    /** Health score after recovery */
    float post_recovery_health;

    /** Additional context */
    char context[128];

} meta_health_decision_t;

/**
 * @brief Collection of past decisions for reflection
 */
typedef struct {
    /** Array of past decisions */
    meta_health_decision_t decisions[META_HEALTH_MAX_DECISIONS];

    /** Number of decisions recorded */
    uint32_t num_decisions;

    /** Circular buffer write position */
    uint32_t write_pos;

    /** Total decisions ever recorded (may exceed buffer) */
    uint64_t total_decisions;

} meta_health_decision_log_t;

/* ============================================================================
 * Performance Assessment Types
 * ============================================================================ */

/**
 * @brief Self-assessment of health system performance
 */
typedef struct {
    /** Correct diagnoses / total diagnoses */
    float accuracy_rate;

    /** Successful recoveries / total recovery attempts */
    float recovery_success_rate;

    /** False alarms / total detections */
    float false_positive_rate;

    /** Missed anomalies / total actual anomalies */
    float false_negative_rate;

    /** Average response time (ms) */
    float avg_response_time_ms;

    /** Median response time (ms) */
    float median_response_time_ms;

    /** 95th percentile response time (ms) */
    float p95_response_time_ms;

    /** Average recovery time (ms) */
    float avg_recovery_time_ms;

    /** Average post-recovery health score */
    float avg_post_recovery_health;

    /** Time period covered by this assessment */
    uint64_t assessment_period_ms;

    /** Number of decisions in assessment period */
    uint32_t decisions_in_period;

} meta_health_assessment_t;

/**
 * @brief Identified weakness in health monitoring
 */
typedef struct {
    /** Source where detection is weakest */
    health_agent_source_t weakest_detection_area;

    /** Detection accuracy for this area */
    float detection_accuracy;

    /** Recovery action that is least effective */
    health_agent_recovery_t least_effective_recovery;

    /** Success rate for this recovery */
    float recovery_success_rate;

    /** Most common false positive source */
    health_agent_source_t most_false_positives;

    /** False positive rate for this source */
    float false_positive_rate;

    /** Anomaly type with slowest response */
    health_agent_msg_type_t slowest_response_type;

    /** Average response time for this type (ms) */
    float slowest_response_time_ms;

} meta_health_weakness_t;

/* ============================================================================
 * Reflection Result Types
 * ============================================================================ */

/**
 * @brief Parameter adjustment recommendation
 */
typedef struct {
    /** Detector/component name */
    char detector_name[64];

    /** Parameter name */
    char param_name[64];

    /** Current value */
    float current_value;

    /** Recommended new value */
    float recommended_value;

    /** Reason for recommendation */
    char reason[128];

    /** Expected improvement percentage */
    float expected_improvement;

    /** Confidence in recommendation (0.0-1.0) */
    float confidence;

} meta_health_adjustment_t;

/**
 * @brief Newly discovered pattern
 */
typedef struct {
    /** Pattern description */
    char pattern_description[256];

    /** Source this pattern applies to */
    health_agent_source_t applies_to;

    /** Suggested response to this pattern */
    health_agent_recovery_t suggested_response;

    /** Confidence in pattern (0.0-1.0) */
    float confidence;

    /** Number of occurrences observed */
    uint32_t occurrences;

    /** Predictor (e.g., "high memory usage predicts deadlock") */
    char predictor[128];

} meta_health_pattern_t;

/**
 * @brief Complete reflection result
 */
typedef struct {
    /** Reflection timestamp */
    uint64_t timestamp_us;

    /** Key insight from reflection */
    char key_insight[256];

    /** Confidence in insight (0.0-1.0) */
    float insight_confidence;

    /** Performance assessment */
    meta_health_assessment_t assessment;

    /** Identified weaknesses */
    meta_health_weakness_t weaknesses;

    /** Recommended parameter adjustments */
    meta_health_adjustment_t adjustments[META_HEALTH_MAX_ADJUSTMENTS];
    uint32_t num_adjustments;

    /** Newly discovered patterns */
    meta_health_pattern_t new_patterns[META_HEALTH_MAX_NEW_PATTERNS];
    uint32_t num_new_patterns;

    /** Overall improvement potential score */
    float improvement_potential;

    /** Processing time for reflection (ms) */
    uint32_t reflection_time_ms;

    /** Number of decisions analyzed */
    uint32_t decisions_analyzed;

} meta_health_reflection_result_t;

/* ============================================================================
 * Meta-Health Handle
 * ============================================================================ */

/** Meta-health reflection system handle */
typedef struct meta_health_reflector meta_health_reflector_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Meta-health configuration
 */
typedef struct {
    /** Enable automatic periodic reflection */
    bool enable_auto_reflection;

    /** Reflection interval (ms) */
    uint32_t reflection_interval_ms;

    /** Minimum decisions before reflection */
    uint32_t min_decisions_for_reflection;

    /** Maximum decisions to analyze */
    uint32_t max_decisions_to_analyze;

    /** Enable automatic application of learnings */
    bool enable_auto_apply;

    /** Confidence threshold for applying adjustments */
    float auto_apply_confidence_threshold;

    /** Enable pattern learning */
    bool enable_pattern_learning;

    /** Timeout for reflection processing (ms) */
    uint32_t reflection_timeout_ms;

    /** Enable RCOG for deep reflection */
    bool use_rcog_for_reflection;

} meta_health_config_t;

/**
 * @brief Get default meta-health configuration
 * @return Default configuration
 */
meta_health_config_t meta_health_default_config(void);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create meta-health reflector
 *
 * @param health_agent Health agent to reflect on
 * @param rcog RCOG engine for deep reflection (optional, can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Reflector handle or NULL on failure
 */
meta_health_reflector_t* meta_health_create(
    nimcp_health_agent_t* health_agent,
    rcog_engine_t* rcog,
    const meta_health_config_t* config
);

/**
 * @brief Destroy meta-health reflector
 * @param reflector Reflector to destroy
 */
void meta_health_destroy(meta_health_reflector_t* reflector);

/**
 * @brief Start automatic reflection
 * @param reflector Reflector handle
 * @return 0 on success, -1 on error
 */
int meta_health_start(meta_health_reflector_t* reflector);

/**
 * @brief Stop automatic reflection
 * @param reflector Reflector handle
 * @return 0 on success, -1 on error
 */
int meta_health_stop(meta_health_reflector_t* reflector);

/* ============================================================================
 * Decision Recording API
 * ============================================================================ */

/**
 * @brief Record a health decision
 *
 * Called by health agent after making a decision.
 *
 * @param reflector Meta-health reflector
 * @param decision Decision to record
 * @return 0 on success, -1 on error
 */
int meta_health_record_decision(
    meta_health_reflector_t* reflector,
    const meta_health_decision_t* decision
);

/**
 * @brief Record decision outcome
 *
 * Updates a previously recorded decision with its outcome.
 *
 * @param reflector Meta-health reflector
 * @param timestamp_us Timestamp of original decision
 * @param outcome Outcome to record
 * @param recovery_succeeded Whether recovery succeeded
 * @param time_to_recovery_ms Time taken for recovery
 * @param post_recovery_health Health score after recovery
 * @return 0 on success, -1 on error
 */
int meta_health_record_outcome(
    meta_health_reflector_t* reflector,
    uint64_t timestamp_us,
    meta_health_outcome_t outcome,
    bool recovery_succeeded,
    uint64_t time_to_recovery_ms,
    float post_recovery_health
);

/**
 * @brief Get decision log
 *
 * @param reflector Meta-health reflector
 * @param log Output decision log
 * @return 0 on success, -1 on error
 */
int meta_health_get_decision_log(
    const meta_health_reflector_t* reflector,
    meta_health_decision_log_t* log
);

/* ============================================================================
 * Reflection API
 * ============================================================================ */

/**
 * @brief Perform self-reflection
 *
 * Analyzes past decisions and generates improvement recommendations.
 *
 * @param reflector Meta-health reflector
 * @param result Output reflection result
 * @return 0 on success, -1 on error
 */
int meta_health_reflect(
    meta_health_reflector_t* reflector,
    meta_health_reflection_result_t* result
);

/**
 * @brief Perform reflection asynchronously
 *
 * @param reflector Meta-health reflector
 * @param request_id Output request ID
 * @return 0 on success, -1 on error
 */
int meta_health_reflect_async(
    meta_health_reflector_t* reflector,
    uint64_t* request_id
);

/**
 * @brief Get async reflection result
 *
 * @param reflector Meta-health reflector
 * @param request_id Request ID
 * @param result Output result (if complete)
 * @return 1 if complete, 0 if pending, -1 on error
 */
int meta_health_get_reflection_result(
    meta_health_reflector_t* reflector,
    uint64_t request_id,
    meta_health_reflection_result_t* result
);

/**
 * @brief Get current performance assessment
 *
 * Quick assessment without full reflection.
 *
 * @param reflector Meta-health reflector
 * @param assessment Output assessment
 * @return 0 on success, -1 on error
 */
int meta_health_get_assessment(
    const meta_health_reflector_t* reflector,
    meta_health_assessment_t* assessment
);

/**
 * @brief Get identified weaknesses
 *
 * @param reflector Meta-health reflector
 * @param weaknesses Output weaknesses
 * @return 0 on success, -1 on error
 */
int meta_health_get_weaknesses(
    const meta_health_reflector_t* reflector,
    meta_health_weakness_t* weaknesses
);

/* ============================================================================
 * Learning Application API
 * ============================================================================ */

/**
 * @brief Apply learnings from reflection to health agent
 *
 * @param reflector Meta-health reflector
 * @param result Reflection result to apply
 * @return Number of adjustments applied, -1 on error
 */
int meta_health_apply_learnings(
    meta_health_reflector_t* reflector,
    const meta_health_reflection_result_t* result
);

/**
 * @brief Apply specific adjustment
 *
 * @param reflector Meta-health reflector
 * @param adjustment Adjustment to apply
 * @return 0 on success, -1 on error
 */
int meta_health_apply_adjustment(
    meta_health_reflector_t* reflector,
    const meta_health_adjustment_t* adjustment
);

/**
 * @brief Register discovered pattern with health agent
 *
 * @param reflector Meta-health reflector
 * @param pattern Pattern to register
 * @return 0 on success, -1 on error
 */
int meta_health_register_pattern(
    meta_health_reflector_t* reflector,
    const meta_health_pattern_t* pattern
);

/**
 * @brief Revert last applied learnings
 *
 * @param reflector Meta-health reflector
 * @return 0 on success, -1 on error
 */
int meta_health_revert_learnings(meta_health_reflector_t* reflector);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Meta-health statistics
 */
typedef struct {
    /** Total reflections performed */
    uint64_t reflections_performed;

    /** Total decisions recorded */
    uint64_t decisions_recorded;

    /** Adjustments applied */
    uint64_t adjustments_applied;

    /** Patterns discovered */
    uint64_t patterns_discovered;

    /** Average improvement per reflection */
    float avg_improvement;

    /** Current accuracy trend (positive = improving) */
    float accuracy_trend;

    /** Current response time trend (negative = improving) */
    float response_time_trend;

    /** Last reflection timestamp */
    uint64_t last_reflection_us;

    /** Average reflection time (ms) */
    float avg_reflection_time_ms;

} meta_health_stats_t;

/**
 * @brief Get meta-health statistics
 *
 * @param reflector Meta-health reflector
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int meta_health_get_stats(
    const meta_health_reflector_t* reflector,
    meta_health_stats_t* stats
);

/**
 * @brief Reset meta-health statistics
 * @param reflector Meta-health reflector
 */
void meta_health_reset_stats(meta_health_reflector_t* reflector);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get outcome name
 * @param outcome Outcome type
 * @return Human-readable name
 */
const char* meta_health_outcome_name(meta_health_outcome_t outcome);

/**
 * @brief Initialize decision with defaults
 * @param decision Decision to initialize
 */
void meta_health_init_decision(meta_health_decision_t* decision);

/**
 * @brief Initialize reflection result
 * @param result Result to initialize
 */
void meta_health_init_reflection_result(meta_health_reflection_result_t* result);

/**
 * @brief Dump reflection result for debugging
 * @param result Result to dump
 */
void meta_health_dump_reflection(const meta_health_reflection_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_HEALTH_H */
