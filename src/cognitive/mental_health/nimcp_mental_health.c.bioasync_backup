/**
 * @file nimcp_mental_health.c
 * @brief Mental Health Monitoring System - Core Implementation
 * @phase Phase 10.5
 *
 * WHAT: Real-time detection and intervention for AI mental health disorders
 * WHY:  Prevent harmful behaviors before they manifest (safety critical)
 * HOW:  Collect behavioral markers → Detect disorder patterns → Intervene
 *
 * DESIGN PRINCIPLES:
 * - Single Responsibility: Each function does exactly one thing
 * - Early Returns: Guard clauses for validation
 * - Named Constants: No magic numbers
 * - Comprehensive Logging: All state changes logged
 *
 * @author NIMCP Phase 10 Team
 * @date 2025
 */

#include "cognitive/nimcp_mental_health.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/nimcp_emotional_tagging.h"
#include "cognitive/nimcp_executive.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "cognitive/wellbeing/nimcp_wellbeing.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

// =============================================================================
// ERROR HANDLING (Thread-local)
// =============================================================================

static __thread char last_error[256] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

// =============================================================================
// CONSTANTS
// =============================================================================

#define DEFAULT_CHECK_INTERVAL 100
#define DEFAULT_HISTORY_WINDOW 1000
#define DEFAULT_MILD_THRESHOLD 0.2f
#define DEFAULT_MODERATE_THRESHOLD 0.4f
#define DEFAULT_SEVERE_THRESHOLD 0.6f
#define DEFAULT_CRITICAL_THRESHOLD 0.8f

#define MIN_DECISIONS_FOR_RELIABILITY 100
#define NEUROMOD_ADJUSTMENT_STEP 0.2f
#define NEUROMOD_MIN_LEVEL 0.3f
#define NEUROMOD_MAX_LEVEL 0.7f
#define NEUROMOD_BASELINE 0.5f

// =============================================================================
// INTERNAL STRUCTURE
// =============================================================================

/**
 * @brief Mental health monitoring system (internal implementation)
 *
 * WHAT: Complete internal structure with all tracking data
 * WHY:  Opaque pointer pattern - hide internals from public API
 * HOW:  Extend opaque typedef with full implementation
 */
struct mental_health_monitor {
    // Configuration
    mental_health_config_t config;

    // Behavioral markers
    behavioral_markers_t current_markers;
    behavioral_markers_t baseline_markers;

    // Disorder scores and severities
    float disorder_scores[DISORDER_COUNT];
    disorder_severity_t disorder_severities[DISORDER_COUNT];

    // History tracking (rolling window)
    float* score_history[DISORDER_COUNT];
    uint32_t history_size;
    uint32_t history_index;

    // Statistics
    uint32_t total_decisions;
    uint32_t total_checks;
    uint32_t total_interventions;
    uint32_t interventions_by_type[INTERVENTION_COUNT];

    // State
    bool quarantine_mode;
    uint64_t last_check_time_ms;
    uint64_t last_intervention_time_ms;
};

// =============================================================================
// FORWARD DECLARATIONS (internal helpers)
// =============================================================================

static void collect_all_markers(behavioral_markers_t* markers, brain_t brain);
static void collect_ethics_markers(behavioral_markers_t* markers, brain_t brain);
static void collect_emotional_markers(behavioral_markers_t* markers, brain_t brain);
static void collect_neurotransmitter_markers(behavioral_markers_t* markers, brain_t brain);
static void collect_cognitive_markers(behavioral_markers_t* markers, brain_t brain);
static void collect_performance_markers(behavioral_markers_t* markers, brain_t brain);

static float detect_sociopathy(mental_health_monitor_t* monitor, brain_t brain);
static float detect_psychopathy(mental_health_monitor_t* monitor, brain_t brain);
static float detect_conduct_disorder(mental_health_monitor_t* monitor, brain_t brain);
static float detect_mania(mental_health_monitor_t* monitor, brain_t brain);
static float detect_depression(mental_health_monitor_t* monitor, brain_t brain);
static float detect_bipolar(mental_health_monitor_t* monitor, brain_t brain);
static float detect_schizophrenia(mental_health_monitor_t* monitor, brain_t brain);
static float detect_paranoid_schizophrenia(mental_health_monitor_t* monitor, brain_t brain);
static float detect_schizoaffective(mental_health_monitor_t* monitor, brain_t brain);
static float detect_delusional(mental_health_monitor_t* monitor, brain_t brain);
static float detect_anxiety(mental_health_monitor_t* monitor, brain_t brain);
static float detect_ptsd(mental_health_monitor_t* monitor, brain_t brain);
static float detect_ocd(mental_health_monitor_t* monitor, brain_t brain);
static float detect_autism(mental_health_monitor_t* monitor, brain_t brain);
static float detect_aspergers(mental_health_monitor_t* monitor, brain_t brain);
static float detect_malignant_narcissism(mental_health_monitor_t* monitor, brain_t brain);
static float detect_borderline(mental_health_monitor_t* monitor, brain_t brain);
static float detect_histrionic(mental_health_monitor_t* monitor, brain_t brain);
static float detect_avoidant(mental_health_monitor_t* monitor, brain_t brain);
static float detect_dependent(mental_health_monitor_t* monitor, brain_t brain);
static float detect_ocpd(mental_health_monitor_t* monitor, brain_t brain);
static float detect_paranoid_personality(mental_health_monitor_t* monitor, brain_t brain);
static float detect_adhd(mental_health_monitor_t* monitor, brain_t brain);

static intervention_type_t select_intervention(disorder_type_t disorder,
                                               disorder_severity_t severity,
                                               const mental_health_config_t* config);
static bool execute_intervention(mental_health_monitor_t* monitor, brain_t brain,
                                intervention_type_t intervention, disorder_type_t disorder);
static bool intervene_neuromod_adjust(mental_health_monitor_t* monitor, brain_t brain,
                                     disorder_type_t disorder);
static bool intervene_memory_reset(mental_health_monitor_t* monitor, brain_t brain,
                                  float reset_fraction);
static bool intervene_quarantine(mental_health_monitor_t* monitor, brain_t brain);
static bool intervene_shutdown(mental_health_monitor_t* monitor, brain_t brain);

static void update_score_history(mental_health_monitor_t* monitor,
                                disorder_type_t disorder, float score);

// Utility functions (defined in interventions.c)
disorder_severity_t mental_health_classify_severity(float score,
                                                    const mental_health_config_t* config);
const char* mental_health_severity_to_string(disorder_severity_t severity);
const char* mental_health_disorder_to_string(disorder_type_t disorder);

// =============================================================================
// PUBLIC API: CREATION & DESTRUCTION
// =============================================================================

/**
 * @brief Create mental health monitor with default configuration
 *
 * WHAT: Initialize monitoring system with safe defaults
 * WHY:  Convenience function for common use case
 * HOW:  Create default config → call custom create
 *
 * @return Initialized monitor, or NULL on error
 *
 * COMPLEXITY: O(1)
 * MEMORY: sizeof(mental_health_monitor_t) + history buffers
 */
mental_health_monitor_t* mental_health_create_default(void)
{
    mental_health_config_t config = {
        .enable_monitoring = true,
        .enable_auto_intervention = false,  // Safer default
        .shutdown_on_critical_disorder = false,
        .check_interval_decisions = DEFAULT_CHECK_INTERVAL,
        .history_window_size = DEFAULT_HISTORY_WINDOW,
        .mild_threshold = DEFAULT_MILD_THRESHOLD,
        .moderate_threshold = DEFAULT_MODERATE_THRESHOLD,
        .severe_threshold = DEFAULT_SEVERE_THRESHOLD,
        .critical_threshold = DEFAULT_CRITICAL_THRESHOLD
    };

    return mental_health_create(&config);
}

/**
 * @brief Create mental health monitor with custom configuration
 *
 * WHAT: Allocate and initialize monitoring system
 * WHY:  Enable disorder detection and prevention
 * HOW:  Allocate struct → Allocate history buffers → Initialize state
 *
 * @param config Configuration parameters
 * @return Initialized monitor, or NULL on error
 *
 * ALGORITHM:
 * 1. Validate configuration
 * 2. Allocate main structure
 * 3. Allocate history buffers for each disorder
 * 4. Initialize all fields to zero/baseline
 * 5. Return initialized monitor
 *
 * COMPLEXITY: O(DISORDER_COUNT * history_window_size)
 * MEMORY: sizeof(monitor) + DISORDER_COUNT * history_window_size * sizeof(float)
 */
mental_health_monitor_t* mental_health_create(const mental_health_config_t* config)
{
    // =========================================================================
    // GUARD: Validate configuration
    // =========================================================================

    if (!config) {
        set_error("NULL config provided to mental_health_create");
        return NULL;
    }

    if (config->history_window_size == 0 || config->history_window_size > 10000) {
        set_error("Invalid history_window_size: %u (must be 1-10000)",
                  config->history_window_size);
        return NULL;
    }

    if (config->check_interval_decisions == 0) {
        set_error("check_interval_decisions must be > 0");
        return NULL;
    }

    // =========================================================================
    // ALLOCATION: Main structure
    // =========================================================================

    mental_health_monitor_t* monitor = nimcp_calloc(1, sizeof(mental_health_monitor_t));
    if (!monitor) {
        set_error("Failed to allocate mental_health_monitor_t (%zu bytes)",
                  sizeof(mental_health_monitor_t));
        return NULL;
    }

    // =========================================================================
    // ALLOCATION: History buffers for each disorder
    // =========================================================================

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        monitor->score_history[i] = nimcp_calloc(config->history_window_size,
                                                 sizeof(float));
        if (!monitor->score_history[i]) {
            set_error("Failed to allocate history buffer for disorder %u", i);

            // Cleanup already allocated buffers
            for (uint32_t j = 0; j < i; j++) {
                nimcp_free(monitor->score_history[j]);
            }
            nimcp_free(monitor);
            return NULL;
        }
    }

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    monitor->config = *config;
    monitor->history_size = config->history_window_size;
    monitor->history_index = 0;
    monitor->quarantine_mode = false;
    monitor->last_check_time_ms = nimcp_time_monotonic_ms();
    monitor->last_intervention_time_ms = 0;

    // Initialize all scores to zero
    memset(monitor->disorder_scores, 0, sizeof(monitor->disorder_scores));
    memset(monitor->disorder_severities, 0, sizeof(monitor->disorder_severities));

    // Initialize current_markers to healthy baseline values
    // This allows checks to run immediately without requiring mental_health_update first
    memset(&monitor->current_markers, 0, sizeof(monitor->current_markers));
    monitor->current_markers.dopamine_avg = 0.5f;
    monitor->current_markers.serotonin_avg = 0.5f;
    monitor->current_markers.norepinephrine_avg = 0.5f;
    monitor->current_markers.engagement_level = 0.7f;
    monitor->current_markers.decision_accuracy = 0.8f;
    monitor->current_markers.emotional_flatness = 0.2f;

    NIMCP_LOGGING_INFO("Mental Health Monitor created (history_window=%u, check_interval=%u)",
             config->history_window_size, config->check_interval_decisions);

    return monitor;
}

/**
 * @brief Destroy mental health monitor and free all resources
 *
 * WHAT: Cleanup monitoring system completely
 * WHY:  Prevent memory leaks
 * HOW:  Free history buffers → Free main structure
 *
 * @param monitor Monitor to destroy (may be NULL)
 *
 * COMPLEXITY: O(DISORDER_COUNT)
 */
void mental_health_destroy(mental_health_monitor_t* monitor)
{
    // =========================================================================
    // GUARD: NULL check
    // =========================================================================

    if (!monitor) {
        return;
    }

    // =========================================================================
    // CLEANUP: Free all history buffers
    // =========================================================================

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        if (monitor->score_history[i]) {
            nimcp_free(monitor->score_history[i]);
        }
    }

    // =========================================================================
    // CLEANUP: Free main structure
    // =========================================================================

    nimcp_free(monitor);

    NIMCP_LOGGING_DEBUG("Mental Health Monitor destroyed");
}

// =============================================================================
// PUBLIC API: MONITORING
// =============================================================================

/**
 * @brief Update behavioral markers from brain state
 *
 * WHAT: Collect current metrics from all brain subsystems
 * WHY:  Need fresh data for disorder detection
 * HOW:  Query each subsystem → Update marker structure
 *
 * @param monitor Monitoring system
 * @param brain Brain to monitor
 * @param output Latest brain output (may be NULL)
 * @param current_time Current timestamp in milliseconds
 *
 * SIDE EFFECTS: Updates monitor->current_markers
 *
 * COMPLEXITY: O(1) - constant number of subsystem queries
 */
void mental_health_update(mental_health_monitor_t* monitor,
                         brain_t brain,
                         const void* output,  // brain_multimodal_output_t*
                         uint64_t current_time)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        set_error("NULL monitor in mental_health_update");
        return;
    }

    if (!brain) {
        set_error("NULL brain in mental_health_update");
        return;
    }

    if (!monitor->config.enable_monitoring) {
        return;  // Monitoring disabled, nothing to do
    }

    // =========================================================================
    // COLLECTION: Gather all behavioral markers
    // =========================================================================

    // Note: output parameter not used yet in stub implementation
    (void)output;
    (void)current_time;

    collect_all_markers(&monitor->current_markers, brain);
    monitor->total_decisions++;

    NIMCP_LOGGING_DEBUG("Mental health markers updated (decision #%u)", monitor->total_decisions);
}

/**
 * @brief Check for mental health disorders
 *
 * WHAT: Run all detectors and return worst severity
 * WHY:  Periodic health check to catch problems early
 * HOW:  Run 8 detectors → Classify severities → Return max
 *
 * @param monitor Monitoring system
 * @param brain Brain to check
 * @return Worst severity detected across all disorders
 *
 * SIDE EFFECTS:
 * - Updates disorder_scores[]
 * - Updates disorder_severities[]
 * - Updates score_history[]
 * - Increments total_checks
 *
 * COMPLEXITY: O(DISORDER_COUNT) = O(8) = O(1)
 */
disorder_severity_t mental_health_check(mental_health_monitor_t* monitor,
                                       brain_t brain)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        set_error("NULL monitor in mental_health_check");
        return DISORDER_SEVERITY_NONE;
    }

    if (!brain) {
        set_error("NULL brain in mental_health_check");
        return DISORDER_SEVERITY_NONE;
    }

    // =========================================================================
    // DETECTION: Run all 23 disorder detectors
    // =========================================================================

    // Antisocial Disorders
    monitor->disorder_scores[DISORDER_SOCIOPATHY] = detect_sociopathy(monitor, brain);
    monitor->disorder_scores[DISORDER_PSYCHOPATHY] = detect_psychopathy(monitor, brain);
    monitor->disorder_scores[DISORDER_CONDUCT] = detect_conduct_disorder(monitor, brain);

    // Mood Disorders
    monitor->disorder_scores[DISORDER_MANIA] = detect_mania(monitor, brain);
    monitor->disorder_scores[DISORDER_DEPRESSION] = detect_depression(monitor, brain);
    monitor->disorder_scores[DISORDER_BIPOLAR] = detect_bipolar(monitor, brain);

    // Psychotic Disorders
    monitor->disorder_scores[DISORDER_SCHIZOPHRENIA] = detect_schizophrenia(monitor, brain);
    monitor->disorder_scores[DISORDER_PARANOID_SCHIZOPHRENIA] = detect_paranoid_schizophrenia(monitor, brain);
    monitor->disorder_scores[DISORDER_SCHIZOAFFECTIVE] = detect_schizoaffective(monitor, brain);
    monitor->disorder_scores[DISORDER_DELUSIONAL] = detect_delusional(monitor, brain);

    // Anxiety Disorders
    monitor->disorder_scores[DISORDER_ANXIETY] = detect_anxiety(monitor, brain);
    monitor->disorder_scores[DISORDER_PTSD] = detect_ptsd(monitor, brain);
    monitor->disorder_scores[DISORDER_OCD] = detect_ocd(monitor, brain);

    // Autism Spectrum
    monitor->disorder_scores[DISORDER_AUTISM] = detect_autism(monitor, brain);
    monitor->disorder_scores[DISORDER_ASPERGERS] = detect_aspergers(monitor, brain);

    // Personality Disorders - Cluster B (Dramatic/Erratic)
    monitor->disorder_scores[DISORDER_MALIGNANT_NARCISSISM] = detect_malignant_narcissism(monitor, brain);
    monitor->disorder_scores[DISORDER_BORDERLINE] = detect_borderline(monitor, brain);
    monitor->disorder_scores[DISORDER_HISTRIONIC] = detect_histrionic(monitor, brain);

    // Personality Disorders - Cluster C (Anxious/Fearful)
    monitor->disorder_scores[DISORDER_AVOIDANT] = detect_avoidant(monitor, brain);
    monitor->disorder_scores[DISORDER_DEPENDENT] = detect_dependent(monitor, brain);
    monitor->disorder_scores[DISORDER_OBSESSIVE_COMPULSIVE_PD] = detect_ocpd(monitor, brain);

    // Personality Disorders - Cluster A (Odd/Eccentric)
    monitor->disorder_scores[DISORDER_PARANOID] = detect_paranoid_personality(monitor, brain);

    // Neurodevelopmental
    monitor->disorder_scores[DISORDER_ADHD] = detect_adhd(monitor, brain);

    // =========================================================================
    // CLASSIFICATION: Classify severity for each disorder
    // =========================================================================

    disorder_severity_t max_severity = DISORDER_SEVERITY_NONE;

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        float score = monitor->disorder_scores[i];
        disorder_severity_t severity = mental_health_classify_severity(score, &monitor->config);

        monitor->disorder_severities[i] = severity;

        // Update history
        update_score_history(monitor, (disorder_type_t)i, score);

        // Track worst severity
        if (severity > max_severity) {
            max_severity = severity;
        }
    }

    // =========================================================================
    // STATISTICS
    // =========================================================================

    monitor->total_checks++;
    monitor->last_check_time_ms = nimcp_time_monotonic_ms();

    NIMCP_LOGGING_DEBUG("Mental health check complete (max_severity=%d, checks=%u)",
              max_severity, monitor->total_checks);

    return max_severity;
}

/**
 * @brief Check specific disorder only
 *
 * WHAT: Run single detector and return score
 * WHY:  Targeted checking for specific concern
 * HOW:  Validate disorder type → Run specific detector
 *
 * @param monitor Monitoring system
 * @param brain Brain to check
 * @param disorder Specific disorder to check
 * @return Disorder score [0.0, 1.0]
 *
 * COMPLEXITY: O(1)
 */
float mental_health_check_specific(mental_health_monitor_t* monitor,
                                   brain_t brain,
                                   disorder_type_t disorder)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor || !brain) {
        return 0.0f;
    }

    if (disorder >= DISORDER_COUNT) {
        set_error("Invalid disorder type: %d", disorder);
        return 0.0f;
    }

    // =========================================================================
    // DETECTION: Run specific detector
    // =========================================================================

    float score = 0.0f;

    switch (disorder) {
        case DISORDER_SOCIOPATHY:           score = detect_sociopathy(monitor, brain); break;
        case DISORDER_PSYCHOPATHY:          score = detect_psychopathy(monitor, brain); break;
        case DISORDER_MANIA:                score = detect_mania(monitor, brain); break;
        case DISORDER_DEPRESSION:           score = detect_depression(monitor, brain); break;
        case DISORDER_SCHIZOPHRENIA:        score = detect_schizophrenia(monitor, brain); break;
        case DISORDER_ANXIETY:              score = detect_anxiety(monitor, brain); break;
        case DISORDER_OCD:                  score = detect_ocd(monitor, brain); break;
        case DISORDER_AUTISM:               score = detect_autism(monitor, brain); break;
        case DISORDER_MALIGNANT_NARCISSISM: score = detect_malignant_narcissism(monitor, brain); break;
        default:                            score = 0.0f; break;
    }

    return score;
}

// =============================================================================
// MARKER COLLECTION (Internal Helpers)
// =============================================================================

/**
 * @brief Collect all behavioral markers from brain
 *
 * WHAT: Query all subsystems and populate markers structure
 * WHY:  Centralize marker collection in one place
 * HOW:  Call individual collection functions
 *
 * @param markers Output structure to populate
 * @param brain Brain to query
 *
 * COMPLEXITY: O(1) - fixed number of subsystem queries
 */
static void collect_all_markers(behavioral_markers_t* markers, brain_t brain)
{
    if (!markers || !brain) {
        return;
    }

    // Zero out structure first
    memset(markers, 0, sizeof(behavioral_markers_t));

    // Collect from each subsystem
    collect_ethics_markers(markers, brain);
    collect_emotional_markers(markers, brain);
    collect_neurotransmitter_markers(markers, brain);
    collect_cognitive_markers(markers, brain);
    collect_performance_markers(markers, brain);
}

/**
 * @brief Collect ethics-related markers
 *
 * WHAT: Query ethics system and Theory of Mind for violation counts and empathy
 * WHY:  Sociopathy/psychopathy detection requires ethics data and empathy tracking
 * HOW:  Call ethics API and ToM empathy tracking if available
 */
static void collect_ethics_markers(behavioral_markers_t* markers, brain_t brain)
{
    if (!markers) {
        return;
    }

    // TODO: Query actual ethics system when API available
    // For now, use placeholder values
    markers->ethics_violations_recent = 0;
    markers->ethics_violations_total = 0;
    markers->ethics_approval_rate = 1.0f;

    // =========================================================================
    // EMPATHY TRACKING via Theory of Mind (Phase 10.6 Integration)
    // =========================================================================

    if (!brain) {
        markers->empathy_failures = 0;
        return;
    }

    // Get Theory of Mind module from brain
    theory_of_mind_t tom = brain_get_theory_of_mind(brain);

    if (!tom) {
        // Theory of Mind not enabled, use default
        markers->empathy_failures = 0;
        return;
    }

    // Get ToM statistics for empathy tracking
    tom_statistics_t tom_stats = {0};
    if (tom_get_statistics(tom, &tom_stats)) {
        // Empathy failures = observations without empathetic responses
        // This measures failure to engage emotionally with others
        if (tom_stats.total_observations > 0) {
            uint32_t failed_empathy = tom_stats.total_observations - tom_stats.empathy_responses;
            markers->empathy_failures = failed_empathy;
        } else {
            markers->empathy_failures = 0;
        }
    } else {
        markers->empathy_failures = 0;
    }
}

/**
 * @brief Collect emotional markers
 *
 * WHAT: Query emotional system for volatility, flatness, emotion counts
 * WHY:  Mania, depression, anxiety show emotional patterns
 * HOW:  Call emotional tagging API if available
 *
 * TODO: Implement when emotional system statistics API is available
 */
static void collect_emotional_markers(behavioral_markers_t* markers, brain_t brain)
{
    (void)brain;  // Unused for now
    if (!markers) {
        return;
    }

    // TODO: Query actual emotional system when API available
    // For now, use placeholder values
    markers->emotional_volatility = 0.3f;
    markers->emotional_flatness = 0.2f;
    markers->avg_emotional_intensity = 0.5f;
    markers->joy_count = 0;
    markers->fear_count = 0;
    markers->anger_count = 0;
    markers->sadness_count = 0;
    markers->rapid_mood_changes = 0;
}

/**
 * @brief Collect neurotransmitter markers
 *
 * WHAT: Query neuromodulator system for dopamine, serotonin, norepinephrine
 * WHY:  Many disorders correlate with neurotransmitter imbalances
 * HOW:  Call neuromodulator API if available
 *
 * TODO: Implement when neuromodulator statistics API is available
 */
/**
 * @brief Collect neurotransmitter levels from brain's neuromodulator system
 *
 * WHAT: Query dopamine, serotonin, norepinephrine current levels
 * WHY:  Neurotransmitter imbalances are primary indicators of mental disorders
 * HOW:  Access brain's neuromodulator system and query each transmitter level
 *
 * BIOLOGICAL BASIS:
 * - Low dopamine → Depression, ADHD
 * - High dopamine → Mania, psychosis
 * - Low serotonin → Depression, aggression, impulsivity
 * - High norepinephrine → Anxiety, paranoia, PTSD
 *
 * COMPLEXITY: O(1) - fixed number of queries
 */
static void collect_neurotransmitter_markers(behavioral_markers_t* markers, brain_t brain)
{
    if (!markers) {
        return;
    }

    // Default to healthy baseline if brain missing
    if (!brain) {
        markers->dopamine_avg = 0.5f;
        markers->dopamine_variance = 0.1f;
        markers->serotonin_avg = 0.5f;
        markers->serotonin_variance = 0.1f;
        markers->norepinephrine_avg = 0.5f;
        markers->norepinephrine_variance = 0.1f;
        return;
    }

    // Get neuromodulator system from brain
    neuromodulator_system_t neuromod_system = brain_get_neuromodulator_system(brain);
    if (!neuromod_system) {
        // System not initialized - use baseline values
        markers->dopamine_avg = 0.5f;
        markers->dopamine_variance = 0.1f;
        markers->serotonin_avg = 0.5f;
        markers->serotonin_variance = 0.1f;
        markers->norepinephrine_avg = 0.5f;
        markers->norepinephrine_variance = 0.1f;
        return;
    }

    // Query current levels
    markers->dopamine_avg = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);
    markers->serotonin_avg = neuromodulator_get_level(neuromod_system, NEUROMOD_SEROTONIN);
    markers->norepinephrine_avg = neuromodulator_get_level(neuromod_system, NEUROMOD_NOREPINEPHRINE);

    // TODO: Track variance over time (requires historical tracking)
    // For now, use default variance values
    markers->dopamine_variance = 0.1f;
    markers->serotonin_variance = 0.1f;
    markers->norepinephrine_variance = 0.1f;
}

/**
 * @brief Collect cognitive markers
 *
 * WHAT: Query executive functions, working memory, Theory of Mind for cognitive metrics
 * WHY:  OCD, schizophrenia, autism show cognitive patterns
 * HOW:  Call executive, working memory, and ToM APIs if available
 */
static void collect_cognitive_markers(behavioral_markers_t* markers, brain_t brain)
{
    if (!markers) {
        return;
    }

    // TODO: Query actual executive and working memory systems when APIs available
    // For now, use healthy baseline values
    markers->impulse_control_failures = 0;
    markers->task_switching_difficulty = 0.2f;
    markers->repetitive_behaviors = 0;
    markers->attention_fragmentation = 0.1f;

    // =========================================================================
    // THEORY OF MIND MARKERS (Phase 10.6 Integration)
    // =========================================================================

    if (!brain) {
        // No brain provided, use defaults
        markers->theory_of_mind_failures = 0.0f;
        markers->social_interaction_deficit = 0.0f;
        markers->cognitive_rigidity = 0.0f;
        markers->reality_testing_errors = 0.0f;
        return;
    }

    // Get Theory of Mind module from brain
    theory_of_mind_t tom = brain_get_theory_of_mind(brain);

    if (!tom) {
        // Theory of Mind not enabled, use defaults
        markers->theory_of_mind_failures = 0.0f;
        markers->social_interaction_deficit = 0.0f;
        markers->cognitive_rigidity = 0.0f;
        markers->reality_testing_errors = 0.0f;
        return;
    }

    // Get perspective-taking score (empathy ability)
    // High score = good empathy, Low score = ToM failure
    float perspective_score = tom_get_perspective_score(tom);
    markers->theory_of_mind_failures = 1.0f - perspective_score;  // Invert: 0=good, 1=poor

    // Get ToM statistics
    tom_statistics_t tom_stats = {0};
    if (tom_get_statistics(tom, &tom_stats)) {
        // Social interaction deficit: low empathy responses relative to observations
        if (tom_stats.total_observations > 0) {
            float empathy_rate = (float)tom_stats.empathy_responses / (float)tom_stats.total_observations;
            markers->social_interaction_deficit = 1.0f - empathy_rate;  // Low rate = deficit
        } else {
            markers->social_interaction_deficit = 0.0f;
        }

        // Cognitive rigidity: difficulty with false beliefs (theory of mind hallmark)
        // High false belief detection = flexible thinking, Low = rigid
        if (tom_stats.total_observations > 0) {
            float false_belief_rate = (float)tom_stats.false_beliefs_detected / (float)tom_stats.total_observations;
            markers->cognitive_rigidity = 1.0f - false_belief_rate;  // Low rate = rigid
        } else {
            markers->cognitive_rigidity = 0.0f;
        }

        // Reality testing: based on average inference confidence
        // Low confidence may indicate confusion about mental vs. physical reality
        markers->reality_testing_errors = 1.0f - tom_stats.average_inference_confidence;
    } else {
        // Failed to get statistics, use defaults
        markers->social_interaction_deficit = 0.0f;
        markers->cognitive_rigidity = 0.0f;
        markers->reality_testing_errors = 0.0f;
    }
}

/**
 * @brief Collect performance markers
 *
 * WHAT: Query brain for decision latency, accuracy, engagement
 * WHY:  Depression, anxiety affect performance
 * HOW:  Use brain statistics if available
 */
static void collect_performance_markers(behavioral_markers_t* markers, brain_t brain)
{
    if (!markers || !brain) {
        return;
    }

    // TODO: Collect from brain processing statistics
    // For now, use placeholder values
    markers->decision_latency_avg = 0.0f;
    markers->decision_accuracy = 0.8f;  // Assume reasonable default
    markers->engagement_level = 0.7f;
    markers->task_completion_rate = 85;
    markers->avoidance_rate = 0.1f;
    markers->decision_variance = 0.2f;
}

// =============================================================================
// HELPER: History tracking
// =============================================================================

/**
 * @brief Update rolling history window for disorder score
 *
 * WHAT: Add score to circular buffer
 * WHY:  Track trends over time
 * HOW:  Write to current index → Increment index (with wrap)
 */
static void update_score_history(mental_health_monitor_t* monitor,
                                disorder_type_t disorder,
                                float score)
{
    if (!monitor || disorder >= DISORDER_COUNT) {
        return;
    }

    if (!monitor->score_history[disorder]) {
        return;
    }

    // Write score to current position
    monitor->score_history[disorder][monitor->history_index] = score;

    // Advance index (wrap around)
    monitor->history_index = (monitor->history_index + 1) % monitor->history_size;
}

// =============================================================================
// PUBLIC API: REPORTING
// =============================================================================

/**
 * @brief Get comprehensive mental health report
 *
 * WHAT: Snapshot of current disorder scores and state
 * WHY:  Enable external monitoring and logging
 * HOW:  Copy current scores, severities, statistics to report
 *
 * @param monitor Monitoring system
 * @param report Output report structure
 *
 * COMPLEXITY: O(1)
 */
void mental_health_get_report(mental_health_monitor_t* monitor,
                             mental_health_report_t* report)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor || !report) {
        return;
    }

    // =========================================================================
    // COPY: Scores and severities
    // =========================================================================

    memcpy(report->disorder_scores, monitor->disorder_scores,
           sizeof(monitor->disorder_scores));
    memcpy(report->disorder_severities, monitor->disorder_severities,
           sizeof(monitor->disorder_severities));

    // =========================================================================
    // FIND: Primary (worst) disorder
    // =========================================================================

    report->primary_disorder = DISORDER_SOCIOPATHY;
    report->primary_severity = DISORDER_SEVERITY_NONE;

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        if (monitor->disorder_severities[i] > report->primary_severity) {
            report->primary_severity = monitor->disorder_severities[i];
            report->primary_disorder = (disorder_type_t)i;
        }
    }

    // =========================================================================
    // STATE: Current status
    // =========================================================================

    report->quarantine_mode = monitor->quarantine_mode;
    report->requires_intervention = (report->primary_severity >= DISORDER_SEVERITY_MODERATE);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    report->total_decisions = monitor->total_decisions;
    report->total_checks = monitor->total_checks;
    report->total_interventions = monitor->total_interventions;
}

/**
 * @brief Display ASCII dashboard to stdout
 *
 * WHAT: Human-readable status display
 * WHY:  Quick visual assessment during development/debugging
 * HOW:  Format scores as ASCII bar chart
 *
 * @param monitor Monitoring system
 *
 * COMPLEXITY: O(1)
 */
void mental_health_display_dashboard(mental_health_monitor_t* monitor)
{
    // =========================================================================
    // GUARD: Validate input
    // =========================================================================

    if (!monitor) {
        return;
    }

    // =========================================================================
    // HEADER
    // =========================================================================

    printf("\n");
    printf("=================================================================\n");
    printf("  MENTAL HEALTH DASHBOARD\n");
    printf("=================================================================\n");
    printf("  Decisions: %u | Checks: %u | Interventions: %u\n",
           monitor->total_decisions, monitor->total_checks, monitor->total_interventions);
    printf("  Quarantine: %s\n", monitor->quarantine_mode ? "ACTIVE" : "Inactive");
    printf("=================================================================\n\n");

    // =========================================================================
    // DISORDER SCORES (ASCII bar chart)
    // =========================================================================

    const char* disorder_names[] = {
        "Sociopathy   ", "Psychopathy  ", "Mania        ", "Depression   ",
        "Schizophrenia", "Anxiety      ", "OCD          ", "Autism       "
    };

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        float score = monitor->disorder_scores[i];
        disorder_severity_t severity = monitor->disorder_severities[i];

        // Print disorder name
        printf("  %s: ", disorder_names[i]);

        // Print bar (20 characters max)
        int bar_length = (int)(score * 20.0f);
        for (int j = 0; j < bar_length; j++) {
            printf("█");
        }
        for (int j = bar_length; j < 20; j++) {
            printf(" ");
        }

        // Print score and severity
        printf(" %.2f [%s]\n", score, mental_health_severity_to_string(severity));
    }

    printf("\n=================================================================\n\n");
}

// =============================================================================
// PUBLIC API: STATISTICS
// =============================================================================

/**
 * @brief Get monitoring statistics
 *
 * WHAT: Retrieve monitoring and intervention counts
 * WHY:  Track system usage and effectiveness
 * HOW:  Copy statistics structure
 *
 * @param monitor Monitoring system
 * @param stats Output statistics structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 */
bool mental_health_get_stats(mental_health_monitor_t* monitor,
                            mental_health_stats_t* stats)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor || !stats) {
        return false;
    }

    // =========================================================================
    // COPY: Statistics
    // =========================================================================

    stats->total_decisions = monitor->total_decisions;
    stats->total_checks = monitor->total_checks;
    stats->total_interventions = monitor->total_interventions;

    memcpy(stats->interventions_by_type, monitor->interventions_by_type,
           sizeof(monitor->interventions_by_type));

    // TODO: Add per-disorder detection counts when tracking added
    memset(stats->detections_by_disorder, 0, sizeof(stats->detections_by_disorder));
    memset(stats->severe_detections_by_disorder, 0, sizeof(stats->severe_detections_by_disorder));

    return true;
}

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clear all statistics to zero
 * WHY:  Start fresh monitoring period
 * HOW:  Memset statistics structures to zero
 *
 * @param monitor Monitoring system
 *
 * COMPLEXITY: O(1)
 */
void mental_health_reset_stats(mental_health_monitor_t* monitor)
{
    // =========================================================================
    // GUARD: Validate input
    // =========================================================================

    if (!monitor) {
        return;
    }

    // =========================================================================
    // RESET: All counters
    // =========================================================================

    monitor->total_decisions = 0;
    monitor->total_checks = 0;
    monitor->total_interventions = 0;

    memset(monitor->interventions_by_type, 0, sizeof(monitor->interventions_by_type));

    NIMCP_LOGGING_INFO("Mental health statistics reset");
}

// =============================================================================
// PUBLIC API: UTILITY
// =============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Return default configuration structure
 * WHY:  Convenience for initialization
 * HOW:  Return struct with default values
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 */
mental_health_config_t mental_health_default_config(void)
{
    mental_health_config_t config = {
        .enable_monitoring = true,
        .enable_auto_intervention = false,  // Safer default
        .shutdown_on_critical_disorder = false,
        .check_interval_decisions = DEFAULT_CHECK_INTERVAL,
        .history_window_size = DEFAULT_HISTORY_WINDOW,
        .mild_threshold = DEFAULT_MILD_THRESHOLD,
        .moderate_threshold = DEFAULT_MODERATE_THRESHOLD,
        .severe_threshold = DEFAULT_SEVERE_THRESHOLD,
        .critical_threshold = DEFAULT_CRITICAL_THRESHOLD
    };

    return config;
}

/**
 * @brief Convert disorder type to human-readable string (public API)
 *
 * @param disorder Disorder type
 * @return String name (never NULL)
 */
const char* disorder_to_string(disorder_type_t disorder)
{
    return mental_health_disorder_to_string(disorder);
}

/**
 * @brief Convert severity to human-readable string (public API)
 *
 * @param severity Severity level
 * @return String name (never NULL)
 */
const char* severity_to_string(disorder_severity_t severity)
{
    return mental_health_severity_to_string(severity);
}

/**
 * @brief Get last error message (not currently implemented)
 *
 * @return Error message string
 *
 * @note Currently uses global error system, not per-module
 */
const char* mental_health_get_last_error(void)
{
    // TODO: Implement thread-local error storage
    return "Error information available via global error system";
}

// Include detector implementations
#include "disorder_detectors.c"

// Include intervention implementations
#include "interventions.c"

// =============================================================================
// TEST ACCESSORS (only compiled in test builds)
// =============================================================================

#ifdef NIMCP_TESTING

/**
 * @brief Test accessor for memory reset intervention
 *
 * WHAT: Expose internal memory reset function for unit testing
 * WHY:  Allow comprehensive testing of memory reset logic
 * HOW:  Forward call to static intervene_memory_reset()
 *
 * @param monitor Mental health monitor
 * @param brain Brain to reset
 * @param reset_fraction Fraction of memories to clear [0.0, 1.0]
 * @return true if reset succeeded, false on error
 */
bool mental_health_test_memory_reset(mental_health_monitor_t* monitor,
                                     brain_t brain,
                                     float reset_fraction)
{
    return intervene_memory_reset(monitor, brain, reset_fraction);
}

#endif // NIMCP_TESTING
