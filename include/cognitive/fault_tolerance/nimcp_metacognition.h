/**
 * @file nimcp_metacognition.h
 * @brief Metacognition for Self-Monitoring - Brain monitoring its own cognitive processes
 * @version 1.0.0
 * @date 2025-01-20
 *
 * WHAT: Self-monitoring system for cognitive health and performance tracking
 * WHY: Detect degradation before catastrophic failure, enable self-awareness
 * HOW: Track performance baselines, detect anomalies, self-diagnose, calibrate confidence
 *
 * BIOLOGICAL BASIS:
 * - Metacognition: Thinking about thinking (frontal cortex function)
 * - Self-monitoring: Executive control system tracking own performance
 * - Confidence calibration: Adjusting certainty based on outcomes
 * - Degradation detection: Early warning system for cognitive decline
 *
 * INTEGRATION POINTS:
 * 1. Brain Structure (src/core/brain/nimcp_brain.c)
 *    - Add: metacognition_t* metacognition;
 *    - Init: brain_create_custom()
 *    - Cleanup: brain_destroy()
 *
 * 2. Configuration (src/core/brain/nimcp_brain.h)
 *    - Add: bool enable_metacognition;
 *    - Add: metacognition_config_t metacognition_config;
 *
 * 3. Processing Pipeline (apply_cognitive_processing())
 *    - Stage: After all cognitive processing
 *    - Purpose: Monitor overall cognitive health
 *
 * DEPENDENCIES:
 * - utils/memory/nimcp_memory.h (memory management)
 * - utils/logging/nimcp_logging.h (logging)
 *
 * DEPENDENT MODULES:
 * - Recovery systems (use diagnosis for recovery decisions)
 * - Health monitors (aggregate metacognition data)
 *
 * PERFORMANCE:
 * - Monitor operation: <100μs (lock-free recording)
 * - Self-diagnosis: <1ms (simple rule-based)
 * - Memory footprint: ~50KB (baseline tracking + metrics)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_METACOGNITION_H
#define NIMCP_METACOGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define METACOGNITION_DEFAULT_BASELINE_WINDOW 100    /**< Default baseline samples */
#define METACOGNITION_DEFAULT_DEGRADATION_THRESHOLD 0.7f  /**< 70% of baseline */
#define METACOGNITION_DEFAULT_CONFIDENCE 0.5f        /**< Initial confidence */
#define METACOGNITION_DEFAULT_LEARNING_RATE 0.1f     /**< Confidence learning rate */
#define METACOGNITION_HIGH_UNCERTAINTY_THRESHOLD 0.7f /**< When to request help */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Diagnosis types for self-identified issues
 */
typedef enum {
    DIAGNOSIS_HEALTHY = 0,              /**< No issues detected */
    DIAGNOSIS_COGNITIVE_SLOWDOWN,       /**< Reasoning speed degraded */
    DIAGNOSIS_MEMORY_CORRUPTION,        /**< Memory recall issues */
    DIAGNOSIS_DECISION_QUALITY_LOW,     /**< Poor decision making */
    DIAGNOSIS_LEARNING_IMPAIRED,        /**< Learning not progressing */
    DIAGNOSIS_ATTENTION_DEFICIT,        /**< Cannot maintain focus */
    DIAGNOSIS_MULTIPLE_ISSUES,          /**< Multiple problems detected */
    DIAGNOSIS_UNKNOWN                   /**< Uncertain state */
} diagnosis_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Cognitive state snapshot for monitoring
 *
 * WHAT: Current cognitive performance metrics
 * WHY: Track multiple dimensions of cognitive health
 * HOW: Aggregate from brain subsystems
 */
typedef struct {
    float reasoning_speed;              /**< Inference time (1.0 = baseline) */
    float memory_recall_accuracy;       /**< Memory retrieval accuracy [0,1] */
    float decision_quality;             /**< Success rate of decisions [0,1] */
    float learning_rate_actual;         /**< Actual learning vs expected [0,1] */
    float attention_focus;              /**< Ability to prioritize [0,1] */
} cognitive_state_t;

/**
 * @brief Performance baseline for comparison
 *
 * WHAT: Expected performance under normal conditions
 * WHY: Detect degradation by comparing to baseline
 * HOW: Rolling average over baseline window
 */
typedef struct {
    float reasoning_speed;              /**< Baseline reasoning speed */
    float memory_recall_accuracy;       /**< Baseline memory accuracy */
    float decision_quality;             /**< Baseline decision quality */
    float learning_rate_actual;         /**< Baseline learning rate */
    float attention_focus;              /**< Baseline attention focus */

    uint32_t sample_count;              /**< Number of samples in baseline */
    bool established;                   /**< true if baseline is valid */
} performance_baseline_t;

/**
 * @brief Current cognitive health metrics
 *
 * WHAT: Real-time health assessment
 * WHY: External visibility into cognitive state
 * HOW: Copy of latest monitored state
 */
typedef struct {
    float reasoning_speed;              /**< Current reasoning speed */
    float memory_recall_accuracy;       /**< Current memory accuracy */
    float decision_quality;             /**< Current decision quality */
    float learning_rate_actual;         /**< Current learning rate */
    float attention_focus;              /**< Current attention focus */

    uint64_t timestamp_us;              /**< When measured */
    float overall_health;               /**< Aggregate health score [0,1] */
} cognitive_health_t;

/**
 * @brief Self-diagnosis result
 *
 * WHAT: Identified cognitive issues and recommended actions
 * WHY: Enable targeted recovery
 * HOW: Rule-based analysis of metrics vs baseline
 */
typedef struct {
    diagnosis_type_t primary_issue;     /**< Primary diagnosed issue */
    float severity;                     /**< How severe [0,1] */

    // Specific issue flags
    bool has_memory_issues;             /**< Memory problems detected */
    bool has_attention_issues;          /**< Attention problems detected */
    bool has_reasoning_issues;          /**< Reasoning problems detected */
    bool has_learning_issues;           /**< Learning problems detected */

    // Recommendations
    bool recommend_recovery;            /**< Should trigger recovery */
    bool recommend_help;                /**< Should request external help */
    bool recommend_rest;                /**< Should reduce workload */

    char description[256];              /**< Human-readable description */
} diagnosis_t;

/**
 * @brief Metacognition configuration
 *
 * WHAT: Configurable parameters for self-monitoring
 * WHY: Allow tuning for different applications
 * HOW: Pass to metacognition_create()
 */
typedef struct {
    uint32_t baseline_window_size;      /**< Samples for baseline (default: 100) */
    float degradation_threshold;        /**< Threshold for degradation (default: 0.7) */
    float confidence_learning_rate;     /**< Confidence update rate (default: 0.1) */
    float high_uncertainty_threshold;   /**< When to ask for help (default: 0.7) */
    bool enable_adaptive_baseline;      /**< Adapt baseline over time (default: true) */
    bool enable_logging;                /**< Log monitoring events (default: false) */
} metacognition_config_t;

/**
 * @brief Metacognition system (opaque)
 *
 * WHAT: Complete self-monitoring system
 * WHY: Encapsulate implementation details
 * HOW: Opaque pointer, use accessor functions
 */
typedef struct metacognition metacognition_t;

//=============================================================================
// Core API Functions
//=============================================================================

/**
 * @brief Create metacognition system with configuration
 *
 * WHAT: Initialize self-monitoring system
 * WHY: Enable brain to monitor its own cognitive processes
 * HOW: Allocate structures, initialize baseline tracking, set defaults
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~50KB (baseline buffers + tracking structures)
 *
 * @param config Configuration (NULL for defaults)
 * @return Metacognition system or NULL on failure
 *
 * @note Caller must call metacognition_destroy() to free resources
 */
metacognition_t* metacognition_create(const metacognition_config_t* config);

/**
 * @brief Destroy metacognition system
 *
 * WHAT: Free all resources used by metacognition
 * WHY: Prevent memory leaks
 * HOW: Free baseline buffers, tracking structures, and main structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~50KB
 *
 * @param meta Metacognition system (NULL safe)
 */
void metacognition_destroy(metacognition_t* meta);

/**
 * @brief Get default configuration
 *
 * WHAT: Return sensible default configuration
 * WHY: Convenience for users who want defaults
 * HOW: Return static config with default values
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - stack allocation
 *
 * @return Default configuration
 */
metacognition_config_t metacognition_default_config(void);

//=============================================================================
// Self-Monitoring Functions
//=============================================================================

/**
 * @brief Monitor own cognitive state
 *
 * WHAT: Update self-monitoring with current cognitive state
 * WHY: Track performance over time, detect degradation
 * HOW: Compare to baseline, update metrics, adjust confidence
 *
 * ALGORITHM:
 * 1. Validate input parameters
 * 2. Update baseline if needed
 * 3. Compare current state to baseline
 * 4. Update health metrics
 * 5. Detect anomalies
 * 6. Update uncertainty
 *
 * COMPLEXITY: O(1) for monitoring, O(n) for baseline update where n=window size
 * MEMORY: O(1)
 *
 * PERFORMANCE: <100μs typical
 *
 * @param meta Metacognition system (non-NULL)
 * @param state Current cognitive state (non-NULL)
 * @return true on success, false on error
 *
 * @note Call this regularly (e.g., after each cognitive operation)
 * @note Thread-safe if compiled with thread safety enabled
 */
bool metacognition_monitor_self(
    metacognition_t* meta,
    const cognitive_state_t* state
);

/**
 * @brief Check if cognitive performance is degraded
 *
 * WHAT: Determine if current performance is below baseline
 * WHY: Early warning of cognitive issues
 * HOW: Compare current metrics to baseline with threshold
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 * @param threshold Degradation threshold (e.g., 0.7 = 70% of baseline)
 * @return true if degraded, false if healthy or error
 *
 * @note Returns false if baseline not yet established
 */
bool metacognition_is_degraded(
    metacognition_t* meta,
    float threshold
);

/**
 * @brief Perform self-diagnosis
 *
 * WHAT: Analyze current state and identify issues
 * WHY: Provide actionable diagnosis for recovery
 * HOW: Rule-based analysis of metrics vs baseline
 *
 * ALGORITHM:
 * 1. Check each cognitive metric
 * 2. Identify primary issue
 * 3. Compute severity
 * 4. Generate recommendations
 * 5. Create human-readable description
 *
 * COMPLEXITY: O(1)
 * MEMORY: Allocates diagnosis_t (~300 bytes)
 *
 * PERFORMANCE: <1ms typical
 *
 * @param meta Metacognition system (non-NULL)
 * @return Diagnosis result (caller must free with diagnosis_destroy())
 *
 * @note Returns NULL on error
 * @note Caller must call diagnosis_destroy() to free result
 */
diagnosis_t* metacognition_self_diagnose(metacognition_t* meta);

//=============================================================================
// Confidence Calibration Functions
//=============================================================================

/**
 * @brief Calibrate confidence based on outcome
 *
 * WHAT: Adjust confidence based on prediction success/failure
 * WHY: Self-calibrating confidence prevents over/under confidence
 * HOW: Increase confidence on success, decrease on failure (learning rate)
 *
 * ALGORITHM:
 * - If success: confidence += learning_rate * (1 - confidence)
 * - If failure: confidence -= learning_rate * confidence
 * - Clamp to [0, 1]
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * BIOLOGICAL BASIS:
 * - Similar to dopamine-based learning
 * - Reward prediction error adjusts confidence
 *
 * @param meta Metacognition system (non-NULL)
 * @param initial_confidence Current confidence [0,1]
 * @param success true if prediction was correct, false otherwise
 * @return Updated confidence [0,1]
 *
 * @note Returns initial_confidence on error (NULL meta)
 */
float metacognition_calibrate_confidence(
    metacognition_t* meta,
    float initial_confidence,
    bool success
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current self-confidence level
 *
 * WHAT: Return current confidence in own cognitive state
 * WHY: External systems can use this for decision making
 * HOW: Return tracked confidence value
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 * @return Confidence level [0,1], 0.0 on error
 */
float metacognition_get_self_confidence(const metacognition_t* meta);

/**
 * @brief Get current uncertainty level
 *
 * WHAT: Return uncertainty about own state
 * WHY: High uncertainty indicates need for help or caution
 * HOW: Compute from performance variance
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 * @return Uncertainty level [0,1], 0.0 on error
 *
 * @note High uncertainty (>0.7) suggests requesting external help
 */
float metacognition_get_uncertainty(const metacognition_t* meta);

/**
 * @brief Check if uncertainty is high
 *
 * WHAT: Determine if uncertainty exceeds threshold
 * WHY: Trigger help requests when too uncertain
 * HOW: Compare uncertainty to threshold
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 * @param threshold Uncertainty threshold [0,1]
 * @return true if uncertainty > threshold
 */
bool metacognition_has_high_uncertainty(
    const metacognition_t* meta,
    float threshold
);

/**
 * @brief Get current baseline
 *
 * WHAT: Retrieve current performance baseline
 * WHY: External visibility into expected performance
 * HOW: Copy baseline structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - stack allocation
 *
 * @param meta Metacognition system (non-NULL)
 * @param baseline Output parameter for baseline (non-NULL)
 * @return true if baseline established, false otherwise
 */
bool metacognition_get_baseline(
    const metacognition_t* meta,
    performance_baseline_t* baseline
);

/**
 * @brief Get current cognitive health snapshot
 *
 * WHAT: Retrieve latest health metrics
 * WHY: External monitoring and reporting
 * HOW: Copy current health structure
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - stack allocation
 *
 * @param meta Metacognition system (non-NULL)
 * @param health Output parameter for health (non-NULL)
 * @return true on success, false on error
 */
bool metacognition_get_current_health(
    const metacognition_t* meta,
    cognitive_health_t* health
);

/**
 * @brief Check if metacognition is initialized
 *
 * WHAT: Verify system is ready for use
 * WHY: Safety check before operations
 * HOW: Check internal state flags
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system
 * @return true if initialized, false otherwise
 */
bool metacognition_is_initialized(const metacognition_t* meta);

//=============================================================================
// Diagnosis Utilities
//=============================================================================

/**
 * @brief Destroy diagnosis result
 *
 * WHAT: Free diagnosis structure
 * WHY: Prevent memory leaks
 * HOW: Call nimcp_free()
 *
 * COMPLEXITY: O(1)
 * MEMORY: Frees ~300 bytes
 *
 * @param diagnosis Diagnosis to free (NULL safe)
 */
void diagnosis_destroy(diagnosis_t* diagnosis);

/**
 * @brief Get diagnosis type name
 *
 * WHAT: Convert diagnosis enum to string
 * WHY: Human-readable diagnosis reporting
 * HOW: Lookup table
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param type Diagnosis type
 * @return String name (static, do not free)
 */
const char* diagnosis_type_to_string(diagnosis_type_t type);

//=============================================================================
// Debug and Testing Functions
//=============================================================================

/**
 * @brief Reset metacognition state for testing
 *
 * WHAT: Clear all tracking data, reset to initial state
 * WHY: Test isolation
 * HOW: Clear baseline, metrics, reset confidence
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 *
 * @note FOR TESTING ONLY - do not use in production
 */
void metacognition_reset_for_testing(metacognition_t* meta);

/**
 * @brief Print metacognition state for debugging
 *
 * WHAT: Dump current state to file
 * WHY: Debugging and diagnostics
 * HOW: Print all metrics and baseline
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1)
 *
 * @param meta Metacognition system (non-NULL)
 * @param fp File to write to (e.g., stdout, stderr)
 */
void metacognition_print_state(const metacognition_t* meta, FILE* fp);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_METACOGNITION_H
