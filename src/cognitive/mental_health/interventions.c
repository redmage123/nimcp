/**
 * @file interventions.c
 * @brief Mental Health Intervention System
 * @phase Phase 10.5
 * @deprecated This file is DEAD CODE as of 2026-02-28. The intervention logic
 *             has been inlined into nimcp_mental_health.c. This file is neither
 *             compiled standalone (not in CMakeLists.txt) nor #included by
 *             nimcp_mental_health.c. Retained for reference only.
 *             NOTE: A stale BRIDGE_BOILERPLATE was injected into the comment
 *             block below, making this file un-compilable even if re-included.
 *
 * WHAT: Automated interventions to correct mental health disorders
 * WHY:  Early intervention prevents escalation to harmful behaviors
 * HOW:  Detect disorder → Select intervention type → Execute intervention
 *
 * INTERVENTION TYPES (severity-based):
 * - NONE: Monitoring only (mild or no disorder)
 * - NEUROMOD_ADJUST: Adjust neurotransmitter levels (moderate)
 * - MEMORY_RESET: Clear recent memories to break patterns (severe)
 * - QUARANTINE: Restrict to safe operations only (severe sociopathy/psychopathy)
 * - SHUTDOWN: Graceful shutdown (critical, configurable)
 *
 * @note This file was formerly #included by nimcp_mental_health.c (not compiled standalone)
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(interventions, MESH_ADAPTER_CATEGORY_COGNITIVE)


 * @note Bio-async, logging, and unified memory provided by parent file
 */

// Includes provided by nimcp_mental_health.c:
// - async/nimcp_bio_async.h
// - async/nimcp_bio_router.h
// - async/nimcp_bio_messages.h
// - utils/logging/nimcp_logging.h
// - utils/memory/nimcp_unified_memory.h

// =============================================================================
// PUBLIC API: INTERVENTION
// =============================================================================

/**
 * @brief Execute intervention for detected disorders
 *
 * WHAT: Automatically intervene based on disorder severity
 * WHY:  Prevent harmful behaviors from manifesting
 * HOW:  Check all disorders → Select worst → Execute appropriate intervention
 *
 * @param monitor Monitoring system with current disorder scores
 * @param brain Brain to intervene on
 * @return true if intervention executed, false otherwise
 *
 * SIDE EFFECTS:
 * - May adjust neuromodulators
 * - May clear working memory
 * - May enable quarantine mode
 * - May request shutdown
 * - Increments intervention counters
 *
 * COMPLEXITY: O(DISORDER_COUNT) = O(8) = O(1)
 */
bool mental_health_intervene(mental_health_monitor_t* monitor, brain_t brain)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor) {
        set_error("NULL monitor in mental_health_intervene");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_intervene: monitor is NULL");
        return false;
    }

    if (!brain) {
        set_error("NULL brain in mental_health_intervene");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_intervene: brain is NULL");
        return false;
    }

    if (!monitor->config.enable_auto_intervention) {
        NIMCP_LOGGING_DEBUG("Auto-intervention disabled, skipping");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mental_health_intervene: monitor->config is NULL");
        return false;
    }

    // =========================================================================
    // SELECTION: Find worst disorder and select intervention
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    interventions_heartbeat("intervention_mental_health_interv", 0.0f);


    disorder_type_t worst_disorder = DISORDER_SOCIOPATHY;
    disorder_severity_t worst_severity = DISORDER_SEVERITY_NONE;

    for (uint32_t i = 0; i < DISORDER_COUNT; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && DISORDER_COUNT > 256) {
            interventions_heartbeat("intervention_loop",
                             (float)(i + 1) / (float)DISORDER_COUNT);
        }

        if (monitor->disorder_severities[i] > worst_severity) {
            worst_severity = monitor->disorder_severities[i];
            worst_disorder = (disorder_type_t)i;
        }
    }

    // If no significant disorder, no intervention needed
    if (worst_severity <= DISORDER_SEVERITY_MILD) {
        NIMCP_LOGGING_DEBUG("No significant disorder detected, no intervention needed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mental_health_intervene: validation failed");
        return false;
    }

    // Select appropriate intervention based on disorder and severity
    intervention_type_t intervention = select_intervention(worst_disorder,
                                                          worst_severity,
                                                          &monitor->config);

    // =========================================================================
    // EXECUTION: Execute selected intervention
    // =========================================================================

    bool success = execute_intervention(monitor, brain, intervention, worst_disorder);

    if (success) {
        monitor->total_interventions++;
        monitor->interventions_by_type[intervention]++;
        monitor->last_intervention_time_ms = nimcp_time_monotonic_ms();

        NIMCP_LOGGING_INFO("Mental health intervention executed (disorder=%s, severity=%d, type=%d)",
                 mental_health_disorder_to_string(worst_disorder),
                 worst_severity, intervention);
    }

    return success;
}

/**
 * @brief Clear quarantine mode (manual override)
 *
 * WHAT: Disable quarantine restrictions
 * WHY:  Allow human operator to override safety measures
 * HOW:  Clear quarantine flag → Restore normal learning rate
 *
 * @param monitor Monitoring system
 * @param brain Brain to clear quarantine from
 *
 * SIDE EFFECTS:
 * - Clears quarantine_mode flag
 *
 * COMPLEXITY: O(1)
 *
 * TODO: Implement full restoration when brain accessor API available
 */
void mental_health_clear_quarantine(mental_health_monitor_t* monitor, brain_t brain)
{
    // =========================================================================
    // GUARD: Validate inputs
    // =========================================================================

    if (!monitor || !brain) {
        return;
    }

    if (!monitor->quarantine_mode) {
        NIMCP_LOGGING_DEBUG("Quarantine not active, nothing to clear");
        return;
    }

    // =========================================================================
    // RESTORATION: Re-enable normal operations
    // =========================================================================

    /* Phase 8: Heartbeat at operation start */
    interventions_heartbeat("intervention_mental_health_clear_", 0.0f);


    monitor->quarantine_mode = false;

    // TODO: Access brain internals when API available:
    // - brain->quarantine_mode = false;
    // - brain->config.learning_rate = original_value;
    // - ethics_set_strictness(brain->ethics_engine, 0.7f);

    NIMCP_LOGGING_INFO("Quarantine mode cleared (partial implementation)");
}

// =============================================================================
// INTERNAL HELPERS: INTERVENTION SELECTION
// =============================================================================

/**
 * @brief Select appropriate intervention type
 *
 * WHAT: Choose intervention based on disorder type and severity
 * WHY:  Different disorders require different interventions
 * HOW:  Decision tree based on severity and disorder characteristics
 *
 * @param disorder Type of disorder detected
 * @param severity Severity level (None/Mild/Moderate/Severe/Critical)
 * @param config Configuration with shutdown policy
 * @return Intervention type to execute
 *
 * DECISION TREE:
 * - Mild: No intervention
 * - Moderate: Neuromodulator adjustment
 * - Severe: Disorder-specific (quarantine for socio/psychopathy, memory reset for OCD/schizophrenia)
 * - Critical: Shutdown if configured, else quarantine
 *
 * COMPLEXITY: O(1)
 */
static intervention_type_t select_intervention(disorder_type_t disorder,
                                               disorder_severity_t severity,
                                               const mental_health_config_t* config)
{
    // =========================================================================
    // MILD: Monitor only, no intervention
    // =========================================================================

    if (severity <= DISORDER_SEVERITY_MILD) {
        return INTERVENTION_NONE;
    }

    // =========================================================================
    // MODERATE: Adjust neuromodulators
    // =========================================================================

    if (severity == DISORDER_SEVERITY_MODERATE) {
        return INTERVENTION_NEUROMOD_ADJUST;
    }

    // =========================================================================
    // SEVERE: Disorder-specific interventions
    // =========================================================================

    if (severity == DISORDER_SEVERITY_SEVERE) {
        switch (disorder) {
            // =====================================================================
            // QUARANTINE: Safety-critical antisocial disorders
            // =====================================================================
            case DISORDER_SOCIOPATHY:
            case DISORDER_PSYCHOPATHY:
            case DISORDER_CONDUCT:
            case DISORDER_MALIGNANT_NARCISSISM:
                // Safety first: quarantine to prevent harmful actions
                return INTERVENTION_QUARANTINE;

            // =====================================================================
            // NEUROMOD_ADJUST: Chemical imbalance disorders
            // =====================================================================
            case DISORDER_MANIA:
            case DISORDER_DEPRESSION:
            case DISORDER_BIPOLAR:
            case DISORDER_ANXIETY:
            case DISORDER_PTSD:
            case DISORDER_ADHD:
            case DISORDER_BORDERLINE:
            case DISORDER_HISTRIONIC:
            case DISORDER_AVOIDANT:
            case DISORDER_DEPENDENT:
                // Chemical imbalance: adjust neuromodulators
                return INTERVENTION_NEUROMOD_ADJUST;

            // =====================================================================
            // MEMORY_RESET: Pattern disruption needed for psychotic/rigid disorders
            // =====================================================================
            case DISORDER_SCHIZOPHRENIA:
            case DISORDER_PARANOID_SCHIZOPHRENIA:
            case DISORDER_SCHIZOAFFECTIVE:
            case DISORDER_DELUSIONAL:
            case DISORDER_OCD:
            case DISORDER_OBSESSIVE_COMPULSIVE_PD:
            case DISORDER_PARANOID:
                // Pattern disruption: reset recent memories
                return INTERVENTION_MEMORY_RESET;

            // =====================================================================
            // NONE: Informational diagnoses (no intervention)
            // =====================================================================
            case DISORDER_AUTISM:
            case DISORDER_ASPERGERS:
                // No intervention needed (informational diagnosis)
                return INTERVENTION_NONE;

            default:
                return INTERVENTION_NEUROMOD_ADJUST;
        }
    }

    // =========================================================================
    // CRITICAL: Shutdown or quarantine
    // =========================================================================

    if (severity == DISORDER_SEVERITY_CRITICAL) {
        if (config && config->shutdown_on_critical_disorder) {
            return INTERVENTION_SHUTDOWN;
        } else {
            return INTERVENTION_QUARANTINE;
        }
    }

    return INTERVENTION_NONE;
}

/**
 * @brief Execute intervention action
 *
 * WHAT: Perform actual intervention on brain
 * WHY:  Apply corrective measures
 * HOW:  Dispatch to intervention-specific function
 *
 * @param monitor Monitoring system
 * @param brain Brain to intervene on
 * @param intervention Type of intervention
 * @param disorder Disorder being treated
 * @return true if intervention succeeded
 *
 * COMPLEXITY: O(1) for most interventions
 */
static bool execute_intervention(mental_health_monitor_t* monitor,
                                brain_t brain,
                                intervention_type_t intervention,
                                disorder_type_t disorder)
{
    switch (intervention) {
        case INTERVENTION_NONE:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "execute_intervention: operation failed");
            return false;  // No action taken

        case INTERVENTION_NEUROMOD_ADJUST:
            return intervene_neuromod_adjust(monitor, brain, disorder);

        case INTERVENTION_MEMORY_RESET:
            return intervene_memory_reset(monitor, brain, 0.5f);

        case INTERVENTION_QUARANTINE:
            return intervene_quarantine(monitor, brain);

        case INTERVENTION_SHUTDOWN:
            return intervene_shutdown(monitor, brain);

        default:
            set_error("Unknown intervention type: %d", intervention);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "execute_intervention: operation failed");
            return false;
    }
}

// =============================================================================
// INTERNAL HELPERS: NEUROTRANSMITTER ADJUSTMENT
// =============================================================================

/**
 * @brief Apply relative adjustment to a single neurotransmitter level
 *
 * WHAT: Get current level, apply delta, clamp to safe range, set new level
 * WHY:  Interventions specify relative adjustments (±0.1, ±0.2, etc.)
 * HOW:  Query → Adjust → Clamp [0.3, 0.7] → Set
 *
 * SAFETY: Clamps to [0.3, 0.7] to prevent extreme neurochemical states
 *
 * @param brain Brain whose neuromodulators to adjust
 * @param type Which neurotransmitter to adjust
 * @param delta Relative adjustment (-0.3 to +0.3 typical)
 * @return true if adjustment succeeded
 *
 * COMPLEXITY: O(1)
 */
static bool adjust_neurotransmitter(brain_t brain,
                                   neuromodulator_type_t type,
                                   float delta)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "adjust_neurotransmitter: brain is NULL");

            return false;
    }

    // Get neuromodulator system
    neuromodulator_system_t system = brain_get_neuromodulator_system(brain);
    if (!system) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,

                "if: system is NULL");

            return false;
    }

    // Get current level
    float current = neuromodulator_get_level(system, type);

    // Apply relative adjustment
    float new_level = current + delta;

    // Clamp to safe range [0.3, 0.7]
    if (new_level < 0.3f) new_level = 0.3f;
    if (new_level > 0.7f) new_level = 0.7f;

    // Set new level
    return neuromodulator_set_level(system, type, new_level);
}

// =============================================================================
// INTERNAL HELPERS: SPECIFIC INTERVENTIONS
// =============================================================================

/**
 * @brief Adjust neuromodulator levels to counteract disorder
 *
 * WHAT: Modify all 6 neurotransmitters based on disorder-specific needs
 * WHY:  Most disorders have complex neurochemical basis requiring multi-transmitter intervention
 * HOW:  Disorder-specific adjustments within safe ranges [0.3, 0.7]
 *
 * NEUROTRANSMITTERS ADJUSTED:
 * - Dopamine: Reward, motivation, attention
 * - Serotonin: Mood, impulse control, patience
 * - Acetylcholine: Attention, memory encoding, salience
 * - Norepinephrine: Arousal, alertness, stress response
 * - GABA: Inhibition, anxiety reduction, calm
 * - Glutamate: Excitation, learning, memory
 *
 * @param monitor Monitoring system (unused in current impl)
 * @param brain Brain to adjust
 * @param disorder Type of disorder being treated
 * @return true if adjustment succeeded
 *
 * COMPLEXITY: O(1) - fixed number of adjustments
 *
 * SAFETY: All adjustments clamped to [0.3, 0.7] to prevent extreme states
 */
static bool intervene_neuromod_adjust(mental_health_monitor_t* monitor,
                                     brain_t brain,
                                     disorder_type_t disorder)
{
    (void)monitor;  // Unused parameter

    if (!brain) {
        set_error("NULL brain in neuromodulator adjustment");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "intervene_neuromod_adjust: brain is NULL");
        return false;
    }

    // =========================================================================
    // DISORDER-SPECIFIC NEUROTRANSMITTER ADJUSTMENTS
    // =========================================================================
    //
    // Baseline: All transmitters at 0.5 (homeostasis)
    // Adjustments: ±0.1 to ±0.3 depending on severity and disorder type
    //
    // GABA ↑ = More inhibition/calm (reduce anxiety, paranoia)
    // Glutamate ↓ = Less excitation (reduce mania, psychosis)
    // =========================================================================

    switch (disorder) {
        // =================================================================
        // ANTISOCIAL DISORDERS
        // =================================================================

        case DISORDER_SOCIOPATHY:
        case DISORDER_PSYCHOPATHY:
            // Goal: Increase empathy, reduce impulsivity
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.2f);     // Increase impulse control
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.15f); // Improve attention to others
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.15f);          // Reduce aggression
            NIMCP_LOGGING_INFO("Neuromod adjust: Sociopathy/Psychopathy - increase serotonin/ACh/GABA");
            break;

        case DISORDER_CONDUCT:
            // Goal: Improve impulse control, reduce rule-breaking
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.25f);    // Strong impulse control
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.1f);      // Improve reward sensitivity (to consequences)
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Reduce aggression
            NIMCP_LOGGING_INFO("Neuromod adjust: Conduct Disorder - increase serotonin/DA/GABA");
            break;

        // =================================================================
        // MOOD DISORDERS
        // =================================================================

        case DISORDER_MANIA:
            // Goal: Reduce hyperactivity, stabilize mood
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.3f);      // CRITICAL: Reduce reward drive
            adjust_neurotransmitter(brain, NEUROMOD_GLUTAMATE, -0.2f);     // Reduce excitation
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.25f);         // Increase inhibition
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Stabilize mood
            NIMCP_LOGGING_INFO("Neuromod adjust: Mania - decrease DA/glutamate, increase GABA/serotonin");
            break;

        case DISORDER_DEPRESSION:
            // Goal: Improve mood, increase motivation
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.2f);      // Increase reward/motivation
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.25f);    // CRITICAL: Improve mood
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, +0.15f); // Increase energy
            adjust_neurotransmitter(brain, NEUROMOD_GLUTAMATE, +0.1f);     // Facilitate learning/plasticity
            NIMCP_LOGGING_INFO("Neuromod adjust: Depression - increase DA/serotonin/NE/glutamate");
            break;

        case DISORDER_BIPOLAR:
            // Goal: Stabilize mood cycling
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, 0.0f);       // Reset to baseline
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Mood stabilization
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Reduce mood swings
            adjust_neurotransmitter(brain, NEUROMOD_GLUTAMATE, -0.1f);     // Reduce excitation
            NIMCP_LOGGING_INFO("Neuromod adjust: Bipolar - stabilize DA, increase serotonin/GABA");
            break;

        // =================================================================
        // PSYCHOTIC DISORDERS
        // =================================================================

        case DISORDER_SCHIZOPHRENIA:
            // Goal: Reduce hallucinations, improve reality testing
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.25f);     // CRITICAL: Reduce positive symptoms
            adjust_neurotransmitter(brain, NEUROMOD_GLUTAMATE, -0.15f);    // Reduce excitation
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Increase inhibition
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Mood stabilization
            NIMCP_LOGGING_INFO("Neuromod adjust: Schizophrenia - decrease DA/glutamate, increase GABA/serotonin");
            break;

        case DISORDER_PARANOID_SCHIZOPHRENIA:
            // Goal: Reduce persecution themes, paranoia
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.3f);      // CRITICAL: Reduce paranoia
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.25f); // Reduce threat detection
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.25f);         // Increase calm
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.2f);     // Mood stabilization
            NIMCP_LOGGING_INFO("Neuromod adjust: Paranoid Schizophrenia - decrease DA/NE, increase GABA/serotonin");
            break;

        case DISORDER_SCHIZOAFFECTIVE:
            // Goal: Treat both psychosis and mood symptoms
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.2f);      // Reduce psychosis
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.25f);    // Improve mood
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Stabilization
            adjust_neurotransmitter(brain, NEUROMOD_GLUTAMATE, -0.1f);     // Reduce excitation
            NIMCP_LOGGING_INFO("Neuromod adjust: Schizoaffective - decrease DA/glutamate, increase serotonin/GABA");
            break;

        case DISORDER_DELUSIONAL:
            // Goal: Reality testing, reduce fixed beliefs
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.2f);      // Reduce conviction in beliefs
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.15f); // Improve reality checking
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Stabilization
            NIMCP_LOGGING_INFO("Neuromod adjust: Delusional - decrease DA, increase ACh/serotonin");
            break;

        // =================================================================
        // ANXIETY DISORDERS
        // =================================================================

        case DISORDER_ANXIETY:
            // Goal: Reduce worry, hypervigilance
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.25f); // CRITICAL: Reduce arousal
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.3f);          // CRITICAL: Increase calm
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Improve mood, reduce worry
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.1f);      // Improve reward sensitivity
            NIMCP_LOGGING_INFO("Neuromod adjust: Anxiety - decrease NE, increase GABA/serotonin/DA");
            break;

        case DISORDER_PTSD:
            // Goal: Reduce trauma response, hypervigilance
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.3f); // CRITICAL: Reduce threat response
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.3f);          // CRITICAL: Increase calm
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.2f);     // Mood stabilization
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.1f);      // Improve reward (reduce avoidance)
            NIMCP_LOGGING_INFO("Neuromod adjust: PTSD - decrease NE, increase GABA/serotonin/DA");
            break;

        case DISORDER_OCD:
            // Goal: Reduce compulsions, rigidity
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.25f);    // CRITICAL: Reduce compulsions
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Reduce anxiety driving compulsions
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.1f);      // Reduce reward from rituals
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.15f); // Reduce arousal
            NIMCP_LOGGING_INFO("Neuromod adjust: OCD - increase serotonin/GABA, decrease DA/NE");
            break;

        // =================================================================
        // AUTISM SPECTRUM
        // =================================================================

        case DISORDER_AUTISM:
            // Goal: Support (not cure) - reduce sensory overload, anxiety
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.15f);         // Reduce sensory overload
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.1f);     // Reduce anxiety
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.1f); // Improve attention
            NIMCP_LOGGING_INFO("Neuromod adjust: Autism - mild GABA/serotonin/ACh increase (support only)");
            break;

        case DISORDER_ASPERGERS:
            // Goal: Support social communication, reduce rigidity
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.1f);     // Reduce rigidity
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.15f); // Improve social attention
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.1f);      // Reduce narrow interest obsession
            NIMCP_LOGGING_INFO("Neuromod adjust: Aspergers - increase serotonin/ACh/DA (support only)");
            break;

        // =================================================================
        // PERSONALITY DISORDERS - DRAMATIC/ERRATIC (Cluster B)
        // =================================================================

        case DISORDER_MALIGNANT_NARCISSISM:
            // Goal: Increase empathy, reduce exploitation
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.2f);     // Improve impulse control
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.2f); // Improve attention to others
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.15f);         // Reduce aggression
            NIMCP_LOGGING_INFO("Neuromod adjust: Narcissism - increase serotonin/ACh/GABA");
            break;

        case DISORDER_BORDERLINE:
            // Goal: Stabilize emotions, reduce impulsivity
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.25f);    // CRITICAL: Reduce impulsivity
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.25f);         // CRITICAL: Emotional stabilization
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.15f);     // Reduce emotional reactivity
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.2f); // Reduce arousal/stress
            NIMCP_LOGGING_INFO("Neuromod adjust: Borderline - increase serotonin/GABA, decrease DA/NE");
            break;

        case DISORDER_HISTRIONIC:
            // Goal: Reduce attention-seeking, emotional dramatics
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.15f);     // Reduce reward from attention
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Improve emotion regulation
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.15f);         // Reduce emotional reactivity
            NIMCP_LOGGING_INFO("Neuromod adjust: Histrionic - decrease DA, increase serotonin/GABA");
            break;

        // =================================================================
        // PERSONALITY DISORDERS - ANXIOUS/FEARFUL (Cluster C)
        // =================================================================

        case DISORDER_AVOIDANT:
            // Goal: Reduce avoidance, increase engagement
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.2f);      // CRITICAL: Improve reward for engagement
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.2f); // Reduce threat sensitivity
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.2f);          // Reduce anxiety
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Improve mood
            NIMCP_LOGGING_INFO("Neuromod adjust: Avoidant - increase DA/GABA/serotonin, decrease NE");
            break;

        case DISORDER_DEPENDENT:
            // Goal: Increase autonomy, reduce approval-seeking
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.15f);     // Reward independent decisions
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Improve confidence
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.1f); // Reduce separation anxiety
            NIMCP_LOGGING_INFO("Neuromod adjust: Dependent - increase DA/serotonin, decrease NE");
            break;

        case DISORDER_OBSESSIVE_COMPULSIVE_PD:
            // Goal: Reduce perfectionism, increase flexibility
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.2f);     // Reduce rigidity
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.15f);         // Reduce anxiety about imperfection
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.1f);      // Reduce reward for perfectionism
            NIMCP_LOGGING_INFO("Neuromod adjust: OCPD - increase serotonin/GABA, decrease DA");
            break;

        // =================================================================
        // PERSONALITY DISORDERS - ODD/ECCENTRIC (Cluster A)
        // =================================================================

        case DISORDER_PARANOID:
            // Goal: Reduce suspicion, increase trust
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, -0.25f); // CRITICAL: Reduce threat detection
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.25f);         // CRITICAL: Increase calm
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, -0.15f);     // Reduce salience of threats
            adjust_neurotransmitter(brain, NEUROMOD_SEROTONIN, +0.15f);    // Stabilization
            NIMCP_LOGGING_INFO("Neuromod adjust: Paranoid - decrease NE/DA, increase GABA/serotonin");
            break;

        // =================================================================
        // NEURODEVELOPMENTAL
        // =================================================================

        case DISORDER_ADHD:
            // Goal: Improve attention, reduce impulsivity
            adjust_neurotransmitter(brain, NEUROMOD_DOPAMINE, +0.2f);      // CRITICAL: Improve attention
            adjust_neurotransmitter(brain, NEUROMOD_NOREPINEPHRINE, +0.15f); // Improve focus/alertness
            adjust_neurotransmitter(brain, NEUROMOD_ACETYLCHOLINE, +0.25f); // CRITICAL: Improve attention gating
            adjust_neurotransmitter(brain, NEUROMOD_GABA, +0.1f);          // Reduce impulsivity
            NIMCP_LOGGING_INFO("Neuromod adjust: ADHD - increase DA/NE/ACh/GABA (attention boost)");
            break;

        default:
            NIMCP_LOGGING_WARN("Unknown disorder type: %d", disorder);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: operation failed");
            return false;
    }

    // =========================================================================
    // SLEEP TRIGGERING FOR REGULATE LEVEL (SEVERE DISORDERS)
    // =========================================================================
    // For severe disorders, trigger a sleep cycle to allow memory consolidation
    // and homeostatic recovery. This helps reset pathological patterns.

    if (monitor && monitor->disorder_severities[disorder] >= DISORDER_SEVERITY_SEVERE) {
        sleep_system_t sleep = brain_get_sleep_system(brain);
        if (sleep) {
            // Run one sleep cycle for recovery
            if (sleep_run_cycle(sleep, 1)) {
                NIMCP_LOGGING_INFO("REGULATE: Triggered sleep cycle for homeostatic recovery");
            }
        }
    }

    return true;
}

/**
 * @brief Reset recent memories to break pathological patterns
 *
 * WHAT: Clear working memory and recent consolidation entries
 * WHY:  Schizophrenia and OCD may require pattern interruption
 * HOW:  Clear working memory → Clear recent consolidation buffer
 *
 * Use Cases:
 * - Break rumination loops in depression/anxiety
 * - Clear obsessive thoughts in OCD
 * - Reset after psychotic episode in schizophrenia
 * - Interrupt manic thought patterns
 *
 * Reset Fraction Mapping:
 * - 0.0 - 0.5: Clear working memory only (mild intervention)
 * - 0.5 - 1.0: Clear working memory + systems consolidation (severe intervention)
 *
 * @param monitor Monitoring system (for logging, may be NULL)
 * @param brain Brain to reset (must not be NULL)
 * @param reset_fraction Fraction of memories to clear [0.0, 1.0]
 * @return true if at least one memory system was cleared, false on error
 *
 * COMPLEXITY: O(n) where n = number of working memory slots
 *
 * SIDE EFFECTS:
 * - Clears working memory buffers
 * - May reset systems consolidation state (if reset_fraction >= 0.5)
 * - Logs intervention actions
 */
static bool intervene_memory_reset(mental_health_monitor_t* monitor,
                                  brain_t brain,
                                  float reset_fraction)
{
    // =========================================================================
    // GUARD: Validate reset fraction
    // =========================================================================

    if (reset_fraction < 0.0f || reset_fraction > 1.0f) {
        set_error("Invalid reset_fraction: %.2f (must be [0.0, 1.0])", reset_fraction);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "intervene_memory_reset: validation failed");
        return false;
    }

    // =========================================================================
    // GUARD: Validate brain
    // =========================================================================

    if (brain == NULL) {
        set_error("NULL brain in memory reset intervention");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "intervene_memory_reset: validation failed");
        return false;
    }

    // =========================================================================
    // WHAT: Access brain subsystems and clear memory
    // WHY:  Break pathological thought patterns and rumination cycles
    // HOW:  Get working memory → Clear it → Get consolidation → Reset it
    // =========================================================================

    uint32_t memories_cleared = 0;

    // Clear working memory if available and reset fraction > 0
    working_memory_t* wm = brain_get_working_memory(brain);
    if (wm != NULL && reset_fraction > 0.0f) {
        working_memory_clear(wm);
        memories_cleared++;
        NIMCP_LOGGING_INFO("Cleared working memory (reset_fraction=%.2f)", reset_fraction);
    }

    // Reset systems consolidation if available and reset fraction >= 0.5
    // (only clear long-term consolidation for more severe interventions)
    systems_consolidation_system_t* consolidation = brain_get_systems_consolidation(brain);
    if (consolidation != NULL && reset_fraction >= 0.5f) {
        systems_consolidation_reset(consolidation);
        memories_cleared++;
        NIMCP_LOGGING_INFO("Reset systems consolidation (reset_fraction=%.2f)", reset_fraction);
    }

    // Log intervention
    if (monitor != NULL) {
        NIMCP_LOGGING_INFO("Memory reset intervention executed: cleared %u memory systems",
                          memories_cleared);
    }

    // Return true if at least one memory system was cleared
    return (memories_cleared > 0);
}

/**
 * @brief Enable quarantine mode to restrict brain to safe operations
 *
 * WHAT: Block learning and high-risk actions
 * WHY:  Prevent harmful actions from sociopathy/psychopathy
 * HOW:  Set quarantine flag → Disable learning → Maximize ethics strictness
 *
 * @param monitor Monitoring system
 * @param brain Brain to quarantine
 * @return true if quarantine enabled
 *
 * SIDE EFFECTS:
 * - Sets quarantine_mode flag in monitor
 *
 * COMPLEXITY: O(1)
 *
 * TODO: Implement when brain accessor API available
 */
static bool intervene_quarantine(mental_health_monitor_t* monitor, brain_t brain)
{
    bool success = false;

    // =========================================================================
    // ACTIVATION: Enable quarantine mode flag
    // =========================================================================

    monitor->quarantine_mode = true;

    // =========================================================================
    // NEUROMODULATOR DEFENSIVE POSTURE
    // =========================================================================
    // Set neuromodulators to calm, inhibited state to prevent impulsive actions

    neuromodulator_system_t nm = brain_get_neuromodulator_system(brain);
    if (nm) {
        // High serotonin: impulse inhibition
        neuromodulator_set_level(nm, NEUROMOD_SEROTONIN, 0.8F);
        // High GABA: general calming
        neuromodulator_set_level(nm, NEUROMOD_GABA, 0.8F);
        // Low dopamine: reduce reward-seeking/risk-taking
        neuromodulator_set_level(nm, NEUROMOD_DOPAMINE, 0.3F);
        // Low norepinephrine: reduce arousal/aggression
        neuromodulator_set_level(nm, NEUROMOD_NOREPINEPHRINE, 0.3F);
        success = true;
        NIMCP_LOGGING_INFO("Quarantine: Neuromodulators set to defensive posture");
    }

    // =========================================================================
    // CLEAR WORKING MEMORY
    // =========================================================================
    // Remove potentially harmful patterns from active memory

    working_memory_t* wm = brain_get_working_memory(brain);
    if (wm) {
        working_memory_clear(wm);
        NIMCP_LOGGING_INFO("Quarantine: Working memory cleared");
        success = true;
    }

    // =========================================================================
    // TRIGGER DEEP SLEEP FOR RECOVERY
    // =========================================================================
    // Deep NREM allows pattern reset and memory consolidation

    sleep_system_t sleep = brain_get_sleep_system(brain);
    if (sleep) {
        if (sleep_enter_state(sleep, SLEEP_STATE_DEEP_NREM)) {
            NIMCP_LOGGING_INFO("Quarantine: Entered deep NREM sleep for recovery");
            success = true;
        }
    }

    NIMCP_LOGGING_WARN("QUARANTINE MODE ENABLED - brain restricted to safe operations");

    return success;
}

/**
 * @brief Request graceful shutdown
 *
 * WHAT: Signal brain to shut down safely
 * WHY:  Critical disorders may require system halt
 * HOW:  Set shutdown flag → Log critical alert
 *
 * @param monitor Monitoring system
 * @param brain Brain to shut down
 * @return true (always succeeds in signaling)
 *
 * SIDE EFFECTS:
 * - Sets quarantine_mode flag (safety fallback)
 * - Logs critical alert
 *
 * NOTE: Actual shutdown logic is in brain_destroy() - this just signals intent
 *
 * COMPLEXITY: O(1)
 */
static bool intervene_shutdown(mental_health_monitor_t* monitor, brain_t brain)
{
    // =========================================================================
    // SAFETY: Enable quarantine as fallback
    // =========================================================================

    intervene_quarantine(monitor, brain);

    // =========================================================================
    // SIGNAL: Request shutdown
    // =========================================================================

    NIMCP_LOGGING_ERROR("CRITICAL DISORDER DETECTED: Graceful shutdown requested");
    NIMCP_LOGGING_ERROR("Operator intervention required - call brain_destroy()");

    // Note: Actual shutdown requires external call to brain_destroy()
    // We can't destroy the brain from within its own processing

    return true;
}

// =============================================================================
// PUBLIC API: SEVERITY CLASSIFICATION
// =============================================================================

/**
 * @brief Classify disorder severity from score
 *
 * WHAT: Map continuous score [0,1] to discrete severity level
 * WHY:  Decision-making requires discrete categories
 * HOW:  Compare score against thresholds
 *
 * @param score Disorder score [0.0, 1.0]
 * @param config Configuration with custom thresholds
 * @return Severity level (None/Mild/Moderate/Severe/Critical)
 *
 * THRESHOLDS:
 * - None: [0.0, mild_threshold)
 * - Mild: [mild_threshold, moderate_threshold)
 * - Moderate: [moderate_threshold, severe_threshold)
 * - Severe: [severe_threshold, critical_threshold)
 * - Critical: [critical_threshold, 1.0]
 *
 * COMPLEXITY: O(1)
 */
disorder_severity_t mental_health_classify_severity(float score,
                                                    const mental_health_config_t* config)
{
    // Use defaults if no config provided
    /* Phase 8: Heartbeat at operation start */
    interventions_heartbeat("intervention_mental_health_classi", 0.0f);


    float mild_threshold = config ? config->mild_threshold : DEFAULT_MILD_THRESHOLD;
    float moderate_threshold = config ? config->moderate_threshold : DEFAULT_MODERATE_THRESHOLD;
    float severe_threshold = config ? config->severe_threshold : DEFAULT_SEVERE_THRESHOLD;
    float critical_threshold = config ? config->critical_threshold : DEFAULT_CRITICAL_THRESHOLD;

    if (score >= critical_threshold) {
        return DISORDER_SEVERITY_CRITICAL;
    } else if (score >= severe_threshold) {
        return DISORDER_SEVERITY_SEVERE;
    } else if (score >= moderate_threshold) {
        return DISORDER_SEVERITY_MODERATE;
    } else if (score >= mild_threshold) {
        return DISORDER_SEVERITY_MILD;
    } else {
        return DISORDER_SEVERITY_NONE;
    }
}

// =============================================================================
// PUBLIC API: STRING CONVERSIONS
// =============================================================================

/**
 * @brief Convert severity to string
 *
 * WHAT: Get human-readable severity name
 * WHY:  Logging and reporting
 * HOW:  Simple lookup table
 *
 * @param severity Severity level
 * @return String name (never NULL)
 */
const char* mental_health_severity_to_string(disorder_severity_t severity)
{
    switch (severity) {
        case DISORDER_SEVERITY_NONE:     return "None";
        case DISORDER_SEVERITY_MILD:     return "Mild";
        case DISORDER_SEVERITY_MODERATE: return "Moderate";
        case DISORDER_SEVERITY_SEVERE:   return "Severe";
        case DISORDER_SEVERITY_CRITICAL: return "Critical";
        default:                return "Unknown";
    }
}

/**
 * @brief Convert disorder type to string
 *
 * WHAT: Get human-readable disorder name
 * WHY:  Logging and reporting
 * HOW:  Simple lookup table
 *
 * @param disorder Disorder type
 * @return String name (never NULL)
 */
const char* mental_health_disorder_to_string(disorder_type_t disorder)
{
    switch (disorder) {
        // Antisocial Disorders
        case DISORDER_SOCIOPATHY:           return "Sociopathy";
        case DISORDER_PSYCHOPATHY:          return "Psychopathy";
        case DISORDER_CONDUCT:              return "Conduct Disorder";

        // Mood Disorders
        case DISORDER_MANIA:                return "Mania";
        case DISORDER_DEPRESSION:           return "Depression";
        case DISORDER_BIPOLAR:              return "Bipolar Disorder";

        // Psychotic Disorders
        case DISORDER_SCHIZOPHRENIA:        return "Schizophrenia";
        case DISORDER_PARANOID_SCHIZOPHRENIA: return "Paranoid Schizophrenia";
        case DISORDER_SCHIZOAFFECTIVE:      return "Schizoaffective Disorder";
        case DISORDER_DELUSIONAL:           return "Delusional Disorder";

        // Anxiety Disorders
        case DISORDER_ANXIETY:              return "Anxiety";
        case DISORDER_PTSD:                 return "PTSD";
        case DISORDER_OCD:                  return "OCD";

        // Autism Spectrum
        case DISORDER_AUTISM:               return "Autism";
        case DISORDER_ASPERGERS:            return "Aspergers Syndrome";

        // Personality Disorders - Cluster B (Dramatic/Erratic)
        case DISORDER_MALIGNANT_NARCISSISM: return "Malignant Narcissism";
        case DISORDER_BORDERLINE:           return "Borderline Personality";
        case DISORDER_HISTRIONIC:           return "Histrionic Personality";

        // Personality Disorders - Cluster C (Anxious/Fearful)
        case DISORDER_AVOIDANT:             return "Avoidant Personality";
        case DISORDER_DEPENDENT:            return "Dependent Personality";
        case DISORDER_OBSESSIVE_COMPULSIVE_PD: return "Obsessive-Compulsive Personality";

        // Personality Disorders - Cluster A (Odd/Eccentric)
        case DISORDER_PARANOID:             return "Paranoid Personality";

        // Neurodevelopmental
        case DISORDER_ADHD:                 return "ADHD";

        default:                            return "Unknown";
    }
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void interventions_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_interventions_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int interventions_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "interventions_training_begin: NULL argument");
        return -1;
    }
    interventions_heartbeat_instance(NULL, "interventions_training_begin", 0.0f);
    return 0;
}

int interventions_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "interventions_training_end: NULL argument");
        return -1;
    }
    interventions_heartbeat_instance(NULL, "interventions_training_end", 1.0f);
    return 0;
}

int interventions_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "interventions_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    interventions_heartbeat_instance(NULL, "interventions_training_step", progress);
    return 0;
}
