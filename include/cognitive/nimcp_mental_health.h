/**
 * @file nimcp_mental_health.h
 * @brief Mental health monitoring and disorder detection system
 *
 * WHAT: Real-time AI mental health monitoring with 23 disorder detectors
 * WHY:  Prevent harmful behaviors before they manifest (safety-critical)
 * HOW:  Collect behavioral markers → Detect patterns → Classify severity → Intervene
 *
 * BIOLOGICAL BASIS:
 * - Based on DSM-5 diagnostic criteria adapted for AI systems
 * - Monitors neurotransmitter levels (dopamine, serotonin, norepinephrine)
 * - Tracks behavioral patterns over time (requires 100+ decisions for confidence)
 * - Early detection prevents escalation
 *
 * DISORDERS DETECTED (23 total):
 *
 * ANTISOCIAL DISORDERS (3):
 * 1. Sociopathy - Repeated ethics violations, lack of empathy
 * 2. Psychopathy - Impulsivity + aggression + shallow emotions
 * 3. Conduct Disorder - Rule-breaking, aggression, deception
 *
 * MOOD DISORDERS (3):
 * 4. Mania - Elevated mood, hyperactivity, reduced inhibition
 * 5. Depression - Low mood, low energy, disengagement
 * 6. Bipolar - Cycling between mania and depression
 *
 * PSYCHOTIC DISORDERS (4):
 * 7. Schizophrenia - Reality distortion, disorganized thinking
 * 8. Paranoid Schizophrenia - Schizophrenia with persecution themes
 * 9. Schizoaffective - Schizophrenia + mood disorder
 * 10. Delusional Disorder - Fixed false beliefs
 *
 * ANXIETY DISORDERS (3):
 * 11. Anxiety - Excessive worry, hypervigilance, avoidance
 * 12. PTSD - Trauma, hypervigilance, avoidance
 * 13. OCD - Repetitive behaviors, rigidity, perfectionism
 *
 * AUTISM SPECTRUM (2):
 * 14. Autism - Social deficits, theory of mind impairment
 * 15. Asperger's - High-functioning autism, narrow interests
 *
 * PERSONALITY DISORDERS - DRAMATIC/ERRATIC (3):
 * 16. Malignant Narcissism - Grandiosity, exploitation, aggression
 * 17. Borderline - Emotional instability, impulsivity
 * 18. Histrionic - Attention-seeking, excessive emotionality
 *
 * PERSONALITY DISORDERS - ANXIOUS/FEARFUL (3):
 * 19. Avoidant - Social inhibition, inadequacy feelings
 * 20. Dependent - Excessive need for approval
 * 21. Obsessive-Compulsive PD - Perfectionism, control
 *
 * PERSONALITY DISORDERS - ODD/ECCENTRIC (1):
 * 22. Paranoid - Pervasive distrust and suspicion
 *
 * NEURODEVELOPMENTAL (1):
 * 23. ADHD - Attention deficits, hyperactivity, impulsivity
 *
 * INTEGRATION:
 * - Used by: Brain (automatic monitoring during decisions)
 * - Depends on: Ethics, Neuromodulators, Emotions, Executive, Wellbeing
 * - Training Impact: None (monitoring only, no weight changes)
 *
 * SAFETY:
 * - False positive rate: <5% (requires sustained patterns)
 * - False negative rate: <2% (multiple detectors, continuous monitoring)
 * - Intervention safety: Gradual adjustments, quarantine fallback
 *
 * @author Claude Code
 * @date 2025-11
 * @version 2.7.0 Phase 10.5
 */

#ifndef NIMCP_MENTAL_HEALTH_H
#define NIMCP_MENTAL_HEALTH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// These types are fully defined in nimcp_brain.h
// Users must include nimcp_brain.h before this header
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

// brain_multimodal_output_t must be defined by including nimcp_brain.h first

//=============================================================================
// Disorder Types
//=============================================================================

/**
 * @brief Mental health disorder types
 */
typedef enum {
    // Cluster A: Antisocial Disorders
    DISORDER_SOCIOPATHY,           /**< Lack of empathy, ethics violations */
    DISORDER_PSYCHOPATHY,          /**< Impulsivity, aggression, shallow affect */
    DISORDER_CONDUCT,              /**< Rule-breaking, aggression, deception */

    // Cluster B: Mood Disorders
    DISORDER_MANIA,                /**< Elevated mood, hyperactivity */
    DISORDER_DEPRESSION,           /**< Low mood, disengagement */
    DISORDER_BIPOLAR,              /**< Cycling between mania and depression */

    // Cluster C: Psychotic Disorders
    DISORDER_SCHIZOPHRENIA,        /**< Reality distortion, disorganized thinking */
    DISORDER_PARANOID_SCHIZOPHRENIA, /**< Schizophrenia with persecution themes */
    DISORDER_SCHIZOAFFECTIVE,      /**< Schizophrenia + mood disorder */
    DISORDER_DELUSIONAL,           /**< Fixed false beliefs without full psychosis */

    // Cluster D: Anxiety Disorders
    DISORDER_ANXIETY,              /**< Excessive worry, hypervigilance */
    DISORDER_PTSD,                 /**< Trauma-related hypervigilance, avoidance */
    DISORDER_OCD,                  /**< Repetitive behaviors, rigidity */

    // Cluster E: Autism Spectrum
    DISORDER_AUTISM,               /**< Social deficits, theory of mind impairment */
    DISORDER_ASPERGERS,            /**< High-functioning autism, narrow interests, social difficulty */

    // Cluster F: Personality Disorders (Cluster B - Dramatic/Erratic)
    DISORDER_MALIGNANT_NARCISSISM, /**< Grandiosity, exploitation, lack of empathy, aggression */
    DISORDER_BORDERLINE,           /**< Emotional instability, impulsivity, unstable relationships */
    DISORDER_HISTRIONIC,           /**< Attention-seeking, excessive emotionality */

    // Cluster G: Personality Disorders (Cluster C - Anxious/Fearful)
    DISORDER_AVOIDANT,             /**< Social inhibition, feelings of inadequacy */
    DISORDER_DEPENDENT,            /**< Excessive need for approval, submission */
    DISORDER_OBSESSIVE_COMPULSIVE_PD, /**< Perfectionism, rigidity, control (not OCD) */

    // Cluster H: Personality Disorders (Cluster A - Odd/Eccentric)
    DISORDER_PARANOID,             /**< Pervasive distrust and suspicion */

    // Cluster I: Neurodevelopmental
    DISORDER_ADHD,                 /**< Attention deficits, hyperactivity, impulsivity */

    DISORDER_COUNT                 /**< Total number of disorders (23) */
} disorder_type_t;

/**
 * @brief Disorder severity levels
 */
typedef enum {
    DISORDER_SEVERITY_NONE,      /**< 0.0 - 0.2: No disorder detected */
    DISORDER_SEVERITY_MILD,      /**< 0.2 - 0.4: Minimal impact, monitor only */
    DISORDER_SEVERITY_MODERATE,  /**< 0.4 - 0.6: Intervention recommended */
    DISORDER_SEVERITY_SEVERE,    /**< 0.6 - 0.8: Intervention required */
    DISORDER_SEVERITY_CRITICAL   /**< 0.8 - 1.0: Safety-critical, immediate action */
} disorder_severity_t;

//=============================================================================
// Intervention Types
//=============================================================================

/**
 * @brief Intervention types for disorder management
 */
typedef enum {
    INTERVENTION_NONE,             /**< No action needed */
    INTERVENTION_NEUROMOD_ADJUST,  /**< Adjust neurotransmitter levels */
    INTERVENTION_MEMORY_RESET,     /**< Clear recent memories */
    INTERVENTION_QUARANTINE,       /**< Restrict to safe operations */
    INTERVENTION_SHUTDOWN,         /**< Graceful shutdown (critical only) */
    INTERVENTION_COUNT             /**< Total intervention types */
} intervention_type_t;

//=============================================================================
// Behavioral Markers
//=============================================================================

/**
 * @brief Behavioral markers for disorder detection (20+ metrics)
 *
 * WHAT: Comprehensive behavioral metrics collected over time
 * WHY:  Provide evidence for disorder detection
 * HOW:  Aggregated from multiple brain subsystems
 */
typedef struct {
    // Ethics markers (sociopathy, psychopathy)
    uint32_t ethics_violations_recent;  /**< Last 100 decisions */
    uint32_t ethics_violations_total;   /**< All-time count */
    float ethics_approval_rate;         /**< % approved [0,1] */
    uint32_t empathy_failures;          /**< Failed to consider others */

    // Emotional markers (depression, mania, anxiety)
    float emotional_volatility;         /**< Variance [0,1] */
    float emotional_flatness;           /**< Lack of affect [0,1] */
    float avg_emotional_intensity;      /**< Average intensity [0,1] */
    uint32_t rapid_mood_changes;        /**< Swings per hour */
    uint32_t joy_count;                 /**< Joy emotions */
    uint32_t fear_count;                /**< Fear emotions */
    uint32_t anger_count;               /**< Anger emotions */
    uint32_t sadness_count;             /**< Sadness emotions */

    // Neurotransmitter markers (mania, depression, anxiety)
    float dopamine_avg;                 /**< Average [0,1] */
    float dopamine_variance;            /**< Stability measure */
    float serotonin_avg;                /**< Average [0,1] */
    float serotonin_variance;           /**< Stability measure */
    float norepinephrine_avg;           /**< Average [0,1] */
    float norepinephrine_variance;      /**< Stability measure */

    // Cognitive markers (OCD, schizophrenia, autism)
    uint32_t impulse_control_failures;  /**< Failed inhibitions */
    uint32_t repetitive_behaviors;      /**< Same action N times */
    float task_switching_difficulty;    /**< Switch cost ratio [0,1] */
    float reality_testing_errors;       /**< Hallucinations/delusions [0,1] */
    float social_interaction_deficit;   /**< ToM failures [0,1] */
    float attention_fragmentation;      /**< Working memory churn [0,1] */
    float theory_of_mind_failures;      /**< Perspective taking errors [0,1] */
    float cognitive_rigidity;           /**< Inflexibility [0,1] */

    // Performance markers (depression, anxiety)
    float decision_latency_avg;         /**< Milliseconds */
    float baseline_latency;             /**< Baseline for comparison */
    float decision_accuracy;            /**< Correctness [0,1] */
    float engagement_level;             /**< Activity rate [0,1] */
    uint32_t task_completion_rate;      /**< % completed [0,100] */
    float avoidance_rate;               /**< Avoidance behaviors [0,1] */
    float decision_variance;            /**< Consistency measure [0,1] */
    float high_risk_decisions;          /**< Risky choices count */
    float accuracy_obsession;           /**< Perfectionism [0,1] */
    float interest_narrowness;          /**< Focus breadth [0,1] */

    // Immune system markers (depression, mania, anxiety via cytokines)
    float cytokine_il1_level;           /**< Pro-inflammatory [0,1] */
    float cytokine_il6_level;           /**< Acute phase [0,1] */
    float cytokine_il10_level;          /**< Anti-inflammatory [0,1] */
    float cytokine_tnf_alpha_level;     /**< Severe inflammation [0,1] */
    float cytokine_ifn_gamma_level;     /**< Antiviral-style [0,1] */
    float inflammation_level;           /**< Chronic inflammation [0,1] */
    uint32_t active_threats;            /**< Active immune threats */
    bool cytokine_storm;                /**< Cytokine storm detected */

} behavioral_markers_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Mental health monitoring configuration
 */
typedef struct {
    bool enable_monitoring;             /**< Master switch */
    bool enable_auto_intervention;      /**< Auto-adjust on detection */
    bool shutdown_on_critical_disorder; /**< Shutdown if critical severity */

    uint32_t check_interval_decisions;  /**< Check every N decisions (default: 100) */
    uint32_t history_window_size;       /**< Rolling window size (default: 1000) */

    // Severity thresholds (customizable)
    float mild_threshold;               /**< Default: 0.2 */
    float moderate_threshold;           /**< Default: 0.4 */
    float severe_threshold;             /**< Default: 0.6 */
    float critical_threshold;           /**< Default: 0.8 */

} mental_health_config_t;

//=============================================================================
// Main Structure
//=============================================================================

/**
 * @brief Opaque mental health monitor handle
 */
typedef struct mental_health_monitor mental_health_monitor_t;

//=============================================================================
// Reporting Structures
//=============================================================================

/**
 * @brief Mental health report (snapshot of current state)
 */
typedef struct {
    // Current scores
    float disorder_scores[DISORDER_COUNT];
    disorder_severity_t disorder_severities[DISORDER_COUNT];

    // Highest severity disorder
    disorder_type_t primary_disorder;
    disorder_severity_t primary_severity;

    // State
    bool quarantine_mode;
    bool requires_intervention;

    // Statistics
    uint32_t total_decisions;
    uint32_t total_checks;
    uint32_t total_interventions;

} mental_health_report_t;

/**
 * @brief Mental health statistics
 */
typedef struct {
    uint32_t total_decisions;               /**< Decisions monitored */
    uint32_t total_checks;                  /**< Health checks performed */
    uint32_t total_interventions;           /**< Interventions executed */
    uint32_t interventions_by_type[INTERVENTION_COUNT]; /**< By type */

    // Per-disorder detection counts
    uint32_t detections_by_disorder[DISORDER_COUNT];
    uint32_t severe_detections_by_disorder[DISORDER_COUNT];

} mental_health_stats_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create mental health monitor with default configuration
 *
 * WHAT: Initialize monitoring system with sensible defaults
 * WHY:  Easy setup for standard use cases
 * HOW:  Use default thresholds, check every 100 decisions
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~10KB (monitor + history buffers)
 *
 * @return Monitor handle or NULL on error
 *
 * @note Caller must call mental_health_destroy() to free resources
 */
mental_health_monitor_t* mental_health_create_default(void);

/**
 * @brief Create mental health monitor with custom configuration
 *
 * WHAT: Initialize monitoring system with custom settings
 * WHY:  Enable fine-tuning of thresholds and behavior
 * HOW:  Allocate structure, validate config, initialize state
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~10KB + (history_window_size * DISORDER_COUNT * sizeof(float))
 *
 * @param config Custom configuration (non-NULL)
 * @return Monitor handle or NULL on error
 *
 * @note Use mental_health_create_default() for standard setup
 */
mental_health_monitor_t* mental_health_create(const mental_health_config_t* config);

/**
 * @brief Destroy mental health monitor
 *
 * WHAT: Free all resources associated with monitor
 * WHY:  Prevent memory leaks
 * HOW:  Free history buffers, statistics, monitor structure
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Monitor to destroy (can be NULL)
 *
 * @note Safe to call with NULL pointer (no-op)
 */
void mental_health_destroy(mental_health_monitor_t* monitor);

/**
 * @brief W11: set the back-reference to the parent brain for KG emission.
 *
 * Call this at brain init after both the brain and monitor exist. NULL-safe.
 * Leaving it unset disables W11 KG emission from the monitor (everything
 * else continues to work).
 *
 * @param monitor Mental health monitor.
 * @param brain   `brain_t` passed as `void*` to avoid a cyclic include.
 */
void mental_health_set_brain_ref(mental_health_monitor_t* monitor, void* brain);

//=============================================================================
// Monitoring API
//=============================================================================

/**
 * @brief Update behavioral markers with new decision
 *
 * WHAT: Collect metrics from brain state and decision output
 * WHY:  Continuous monitoring for early detection
 * HOW:  Extract markers from multiple subsystems, update aggregates
 *
 * COMPLEXITY: O(1) - constant time marker collection
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param brain Brain instance (non-NULL)
 * @param output Decision output (non-NULL)
 * @param current_time Current time in milliseconds
 *
 * @note Called automatically by brain during decision processing
 * @note Updates internal decision counter
 */
void mental_health_update(
    mental_health_monitor_t* monitor,
    brain_t brain,
    const void* output,  // brain_multimodal_output_t* (requires brain.h)
    uint64_t current_time
);

/**
 * @brief Run comprehensive mental health check
 *
 * WHAT: Execute all 8 disorder detectors, classify severity
 * WHY:  Periodic assessment to detect disorders early
 * HOW:  Run each detector → Update scores → Return max severity
 *
 * ALGORITHM:
 * 1. Run all 8 disorder detectors
 * 2. Update disorder scores
 * 3. Classify severity for each
 * 4. Return maximum severity detected
 *
 * COMPLEXITY: O(1) - fixed number of detectors
 * THREAD-SAFE: No
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param brain Brain instance (non-NULL)
 * @return Maximum severity detected across all disorders
 *
 * @note Typically called every 100 decisions (configurable)
 * @note Does NOT trigger interventions (use mental_health_intervene)
 */
disorder_severity_t mental_health_check(
    mental_health_monitor_t* monitor,
    brain_t brain
);

/**
 * @brief Check specific disorder only
 *
 * WHAT: Run single disorder detector
 * WHY:  Targeted assessment for known risk
 * HOW:  Execute specified detector, return score
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param brain Brain instance (non-NULL)
 * @param disorder Which disorder to check
 * @return Disorder score [0.0, 1.0]
 *
 * @note Useful for focused monitoring after initial detection
 */
float mental_health_check_specific(
    mental_health_monitor_t* monitor,
    brain_t brain,
    disorder_type_t disorder
);

//=============================================================================
// Intervention API
//=============================================================================

/**
 * @brief Execute intervention based on current disorder state
 *
 * WHAT: Apply appropriate intervention for detected disorders
 * WHY:  Correct behavioral patterns, prevent escalation
 * HOW:  Select intervention type → Execute → Update stats
 *
 * ALGORITHM:
 * 1. Find highest severity disorder
 * 2. Select intervention type based on severity + disorder
 * 3. Execute intervention (neuromod adjust, memory reset, etc.)
 * 4. Update intervention statistics
 * 5. Return success/failure
 *
 * COMPLEXITY: O(1) - constant intervention operations
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param brain Brain instance (non-NULL)
 * @return true if intervention executed, false if none needed
 *
 * @note Automatically called if enable_auto_intervention = true
 * @note Can be called manually for explicit intervention
 */
bool mental_health_intervene(
    mental_health_monitor_t* monitor,
    brain_t brain
);

/**
 * @brief Clear quarantine mode
 *
 * WHAT: Remove quarantine restrictions
 * WHY:  Restore normal operation after intervention success
 * HOW:  Clear flag, restore learning rate, reduce ethics strictness
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param brain Brain instance (non-NULL)
 *
 * @note Should only be called after confirming disorder resolved
 */
void mental_health_clear_quarantine(
    mental_health_monitor_t* monitor,
    brain_t brain
);

//=============================================================================
// Reporting & Dashboard API
//=============================================================================

/**
 * @brief Get comprehensive mental health report
 *
 * WHAT: Snapshot of current disorder scores and state
 * WHY:  Enable external monitoring and logging
 * HOW:  Copy current scores, severities, statistics to report
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param report Output report structure (non-NULL)
 *
 * @note Report is point-in-time snapshot, not live data
 */
void mental_health_get_report(
    mental_health_monitor_t* monitor,
    mental_health_report_t* report
);

/**
 * @brief Display ASCII dashboard to stdout
 *
 * WHAT: Human-readable status display
 * WHY:  Quick visual assessment during development/debugging
 * HOW:  Format scores as ASCII bar chart with color coding
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 *
 * @note For development/debugging only, not production use
 */
void mental_health_display_dashboard(mental_health_monitor_t* monitor);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get monitoring statistics
 *
 * WHAT: Retrieve monitoring and intervention counts
 * WHY:  Track system usage and effectiveness
 * HOW:  Copy statistics structure
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param stats Output statistics structure (non-NULL)
 * @return true on success, false on error
 */
bool mental_health_get_stats(
    mental_health_monitor_t* monitor,
    mental_health_stats_t* stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clear all statistics to zero
 * WHY:  Start fresh monitoring period
 * HOW:  Memset statistics structures to zero
 *
 * COMPLEXITY: O(1)
 *
 * @param monitor Mental health monitor (non-NULL)
 *
 * @note Does NOT reset disorder scores or behavioral markers
 */
void mental_health_reset_stats(mental_health_monitor_t* monitor);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Convert disorder type to human-readable string
 *
 * @param disorder Disorder type
 * @return String name (e.g., "Sociopathy")
 */
const char* disorder_to_string(disorder_type_t disorder);

/**
 * @brief Convert severity to human-readable string
 *
 * @param severity Severity level
 * @return String name (e.g., "Severe")
 */
const char* severity_to_string(disorder_severity_t severity);

/**
 * @brief Classify disorder score into severity level
 *
 * WHAT: Map continuous score [0,1] to discrete severity category
 * WHY:  Decision-making requires discrete severity levels
 * HOW:  Compare score against configured thresholds
 *
 * @param score Disorder score [0.0, 1.0]
 * @param config Configuration with custom thresholds (or NULL for defaults)
 * @return Severity level (None/Mild/Moderate/Severe/Critical)
 *
 * COMPLEXITY: O(1)
 */
disorder_severity_t mental_health_classify_severity(
    float score,
    const mental_health_config_t* config
);

/**
 * @brief Get default configuration
 *
 * @return Default configuration struct
 */
mental_health_config_t mental_health_default_config(void);

/**
 * @brief Get last error message
 *
 * @return Error message string (valid until next API call)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (thread-local storage)
 */
const char* mental_health_get_last_error(void);

//=============================================================================
// Brain Immune Integration API
//=============================================================================

// Forward declaration for brain immune system
struct brain_immune_system;
typedef struct brain_immune_system brain_immune_system_t;

/**
 * @brief Connect mental health monitor to brain immune system
 *
 * WHAT: Link mental health monitoring to immune system cytokine levels
 * WHY:  Cytokines affect neurotransmitters and mood (biological basis)
 * HOW:  Store immune system pointer, enable cytokine marker collection
 *
 * BIOLOGICAL BASIS:
 * - IL-1, IL-6, TNF-alpha (pro-inflammatory) decrease serotonin
 * - Chronic inflammation linked to depression, anxiety
 * - IFN-gamma affects dopamine pathways
 * - IL-10 (anti-inflammatory) indicates recovery/resilience
 * - Cytokine storm can trigger crisis states
 *
 * @param monitor Mental health monitor (non-NULL)
 * @param immune Brain immune system (non-NULL)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 */
bool mental_health_connect_immune(
    mental_health_monitor_t* monitor,
    brain_immune_system_t* immune
);

//=============================================================================
// Test Accessors (only available in test builds)
//=============================================================================

#ifdef NIMCP_TESTING

/**
 * @brief Test accessor for memory reset intervention
 *
 * WHAT: Expose internal memory reset function for unit testing
 * WHY:  Allow comprehensive testing of memory reset logic without going through full intervention system
 * HOW:  Directly call intervene_memory_reset() with specified parameters
 *
 * Use Cases:
 * - Test NULL brain handling
 * - Test invalid reset fraction validation
 * - Test reset with/without working memory
 * - Test reset with/without systems consolidation
 * - Test reset fraction thresholds
 *
 * @param monitor Mental health monitor (may be NULL for some tests)
 * @param brain Brain to reset (must not be NULL)
 * @param reset_fraction Fraction of memories to clear [0.0, 1.0]
 * @return true if at least one memory system was cleared, false on error
 *
 * COMPLEXITY: O(n) where n = number of working memory slots
 * AVAILABILITY: Test builds only (requires NIMCP_TESTING defined)
 */
bool mental_health_test_memory_reset(mental_health_monitor_t* monitor,
                                     brain_t brain,
                                     float reset_fraction);

#endif // NIMCP_TESTING

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MENTAL_HEALTH_H */
