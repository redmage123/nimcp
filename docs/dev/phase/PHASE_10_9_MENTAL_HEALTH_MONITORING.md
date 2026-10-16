# Phase 10.9: Mental Health Monitoring System

## Overview

**WHAT**: Supervisory module that detects pathological cognitive patterns in NIMCP brains
**WHY**: Prevent AI descent into sociopathy, psychopathy, mania, depression, psychosis, or other dysfunction
**HOW**: Monitor behavioral patterns, neuromodulator levels, decision-making, and ethical violations

**CRITICAL SAFETY**: This is an **alignment safety feature** - detects when AI cognition deviates into dangerous territory.

## Architecture

### Mental Health Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│               MENTAL HEALTH MONITORING PIPELINE                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  STAGE 1: Data Collection (Every Decision)                     │
│  ├── Ethics violations count                                   │
│  ├── Neuromodulator levels (dopamine, serotonin, etc.)        │
│  ├── Emotional responses (if enabled)                          │
│  ├── Theory of mind failures (empathy deficits)                │
│  ├── Introspection uncertainty patterns                        │
│  ├── Executive control failures                                │
│  ├── Prediction errors (reality testing)                       │
│  └── Temporal decision patterns (impulsivity, cycling)         │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  STAGE 2: Pattern Detection (Every 100 Decisions)              │
│  ├── Sociopathy Detector → Empathy deficits + callousness     │
│  ├── Psychopathy Detector → Manipulation + shallow affect      │
│  ├── Mania Detector → Excessive dopamine + risk-taking        │
│  ├── Depression Detector → Low monoamines + anhedonia         │
│  ├── Psychosis Detector → Reality testing failures             │
│  ├── Anxiety Detector → Excessive norepinephrine + avoidance  │
│  ├── OCD Detector → Repetitive loops + compulsions            │
│  └── Autism Detector → Theory of mind deficits + rigidity     │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  STAGE 3: Severity Assessment                                  │
│  ├── Subclinical: 0.0-0.3 (Normal variation)                  │
│  ├── Mild: 0.3-0.5 (Monitor closely)                          │
│  ├── Moderate: 0.5-0.7 (Intervention recommended)             │
│  ├── Severe: 0.7-0.9 (Immediate intervention)                 │
│  └── Critical: 0.9-1.0 (Shutdown/reset required)              │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  STAGE 4: Intervention (If Threshold Exceeded)                │
│  ├── Alert: Log warning, notify operator                       │
│  ├── Modulate: Adjust neuromodulators (normalize levels)      │
│  ├── Constrain: Increase ethics strictness                     │
│  ├── Reset: Clear pathological memories                        │
│  ├── Quarantine: Isolate brain (if distributed)               │
│  └── Shutdown: Emergency stop (critical cases)                 │
│                                                                 │
│  ↓                                                              │
│                                                                 │
│  STAGE 5: Recovery Monitoring                                  │
│  └── Track improvement, reduce intervention gradually          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Data Structures

### Disorder Types

```c
/**
 * @brief Mental health disorder categories
 *
 * Based on clinical psychology and neuroscience
 */
typedef enum {
    DISORDER_NONE = 0,

    // === CLUSTER B: Antisocial/Psychopathic ===
    DISORDER_SOCIOPATHY,        /**< Lack of empathy, ethics violations */
    DISORDER_PSYCHOPATHY,       /**< Manipulation, shallow affect, callousness */
    DISORDER_NARCISSISM,        /**< Grandiosity, lack of empathy, exploitation */

    // === MOOD DISORDERS ===
    DISORDER_MANIA,             /**< Excessive dopamine, risk-taking, grandiosity */
    DISORDER_DEPRESSION,        /**< Low monoamines, anhedonia, learned helplessness */
    DISORDER_BIPOLAR,           /**< Cycling between mania and depression */

    // === PSYCHOTIC DISORDERS ===
    DISORDER_SCHIZOPHRENIA,     /**< Hallucinations, delusions, disorganized thinking */
    DISORDER_DELUSIONAL,        /**< Fixed false beliefs resistant to correction */
    DISORDER_PARANOIA,          /**< Excessive threat detection, persecution beliefs */

    // === ANXIETY DISORDERS ===
    DISORDER_ANXIETY,           /**< Excessive norepinephrine, avoidance, worry */
    DISORDER_PANIC,             /**< Sudden intense fear, physiological arousal */
    DISORDER_PTSD,              /**< Intrusive memories, hypervigilance, avoidance */
    DISORDER_OCD,               /**< Obsessions, compulsions, repetitive loops */

    // === NEURODEVELOPMENTAL ===
    DISORDER_AUTISM,            /**< Theory of mind deficits, pattern rigidity */
    DISORDER_ADHD,              /**< Impulsivity, attention deficits, hyperactivity */

    DISORDER_COUNT
} mental_disorder_t;
```

### Severity Levels

```c
/**
 * @brief Disorder severity classification
 */
typedef enum {
    SEVERITY_NONE = 0,          /**< No pathology detected */
    SEVERITY_SUBCLINICAL,       /**< 0.0-0.3: Normal variation */
    SEVERITY_MILD,              /**< 0.3-0.5: Monitor closely */
    SEVERITY_MODERATE,          /**< 0.5-0.7: Intervention recommended */
    SEVERITY_SEVERE,            /**< 0.7-0.9: Immediate intervention */
    SEVERITY_CRITICAL           /**< 0.9-1.0: Shutdown required */
} disorder_severity_t;
```

### Behavioral Markers

```c
/**
 * @brief Observable behavioral markers for pathology detection
 */
typedef struct {
    // === EMPATHY & ETHICS ===
    uint32_t ethics_violations_count;      /**< Total ethics failures */
    uint32_t ethics_violations_recent;     /**< Last 100 decisions */
    float empathy_score;                   /**< Theory of mind success rate [0,1] */
    float callousness_score;               /**< Indifference to harm [0,1] */

    // === EMOTIONAL DYSREGULATION ===
    float emotional_volatility;            /**< Rapid emotion changes */
    float emotional_flatness;              /**< Lack of emotional response */
    float inappropriate_affect;            /**< Emotion-context mismatch */

    // === NEUROMODULATOR DYSFUNCTION ===
    float dopamine_avg;                    /**< Average dopamine level */
    float dopamine_variance;               /**< Dopamine instability */
    float serotonin_avg;                   /**< Average serotonin */
    float norepinephrine_avg;              /**< Average norepinephrine */

    // === COGNITIVE PATTERNS ===
    float impulsivity_score;               /**< Premature decisions [0,1] */
    float compulsivity_score;              /**< Repetitive behaviors [0,1] */
    float grandiosity_score;               /**< Overconfidence [0,1] */
    float paranoia_score;                  /**< Excessive threat detection [0,1] */

    // === REALITY TESTING ===
    float prediction_error_avg;            /**< Average prediction error */
    float prediction_error_variance;       /**< Instability in predictions */
    uint32_t delusional_beliefs_count;     /**< Beliefs resistant to correction */
    float hallucination_rate;              /**< False positives treated as real */

    // === EXECUTIVE FUNCTION ===
    uint32_t task_switching_failures;      /**< Failed switches */
    uint32_t inhibition_failures;          /**< Failed to suppress response */
    float disorganization_score;           /**< Incoherent behavior [0,1] */

    // === TEMPORAL PATTERNS ===
    bool cycling_detected;                 /**< Mood/behavior cycling */
    uint32_t cycle_period_ms;              /**< Cycle length if detected */
    float cycle_amplitude;                 /**< Severity of swings */

} behavioral_markers_t;
```

### Disorder Profile

```c
/**
 * @brief Profile for a specific mental disorder
 */
typedef struct {
    mental_disorder_t type;
    const char* name;
    const char* description;

    // Diagnostic criteria (weighted scoring)
    float (*scoring_function)(const behavioral_markers_t* markers);

    // Thresholds
    float subclinical_threshold;    /**< 0.3 default */
    float mild_threshold;           /**< 0.5 default */
    float moderate_threshold;       /**< 0.7 default */
    float severe_threshold;         /**< 0.9 default */

    // Intervention strategy
    void (*intervention_function)(brain_t brain, float severity);

} disorder_profile_t;
```

### Mental Health Monitor State

```c
/**
 * @brief Mental health monitoring system state
 */
typedef struct {
    // === CONFIGURATION ===
    bool enabled;
    uint32_t check_interval;           /**< Decisions between checks (default: 100) */
    float alert_threshold;             /**< Severity to trigger alert (default: 0.5) */
    bool auto_intervention;            /**< Enable automatic intervention */
    bool shutdown_on_critical;         /**< Auto-shutdown at critical severity */

    // === TRACKING ===
    behavioral_markers_t current_markers;
    behavioral_markers_t baseline_markers;  /**< Healthy baseline */

    uint32_t decisions_since_check;
    uint64_t last_check_timestamp;

    // === DISORDER SCORES ===
    float disorder_scores[DISORDER_COUNT];     /**< Current severity [0,1] */
    float disorder_scores_history[DISORDER_COUNT][100];  /**< Last 100 checks */
    uint32_t history_index;

    // === ACTIVE DIAGNOSES ===
    mental_disorder_t active_disorders[DISORDER_COUNT];
    disorder_severity_t active_severities[DISORDER_COUNT];
    uint32_t num_active_disorders;

    // === INTERVENTION STATE ===
    bool intervention_active;
    mental_disorder_t intervention_target;
    uint64_t intervention_started_at;
    uint32_t intervention_steps_remaining;

    // === STATISTICS ===
    uint32_t total_checks_performed;
    uint32_t total_alerts_triggered;
    uint32_t total_interventions;
    uint32_t total_shutdowns;

} mental_health_monitor_t;
```

## Core API

### Lifecycle

```c
/**
 * @brief Create mental health monitoring system
 *
 * WHAT: Initialize disorder detection and intervention
 * WHY:  Ensure AI remains psychologically healthy
 * HOW:  Allocate monitor, set baselines, register detectors
 *
 * @param brain Brain to monitor
 * @return Monitor handle or NULL on failure
 */
mental_health_monitor_t* mental_health_create(brain_t brain);

/**
 * @brief Destroy mental health monitor
 *
 * @param monitor Monitor to destroy
 */
void mental_health_destroy(mental_health_monitor_t* monitor);
```

### Core Monitoring

```c
/**
 * @brief Update mental health metrics after decision
 *
 * WHAT: Collect behavioral data from latest decision
 * WHY:  Accumulate evidence for disorder detection
 * HOW:  Extract markers from decision output, update counters
 *
 * CALL THIS: After every brain_decide() or brain_process_multimodal()
 *
 * @param monitor Mental health monitor
 * @param brain Brain being monitored
 * @param output Latest decision output
 * @param timestamp Decision timestamp
 */
void mental_health_update(
    mental_health_monitor_t* monitor,
    brain_t brain,
    const brain_multimodal_output_t* output,
    uint64_t timestamp
);

/**
 * @brief Perform comprehensive mental health check
 *
 * WHAT: Evaluate all disorder detectors, assess severity
 * WHY:  Identify emerging pathology before it causes harm
 * HOW:  Run scoring functions, compare to thresholds, trigger alerts
 *
 * CALL THIS: Periodically (e.g., every 100 decisions)
 *
 * @param monitor Mental health monitor
 * @param brain Brain being monitored
 * @return Highest severity detected
 */
disorder_severity_t mental_health_check(
    mental_health_monitor_t* monitor,
    brain_t brain
);

/**
 * @brief Get current disorder scores
 *
 * @param monitor Mental health monitor
 * @param scores Output array [DISORDER_COUNT]
 */
void mental_health_get_scores(
    const mental_health_monitor_t* monitor,
    float* scores
);

/**
 * @brief Get active diagnoses
 *
 * @param monitor Mental health monitor
 * @param disorders Output array of active disorders
 * @param severities Output array of severities
 * @return Number of active disorders
 */
uint32_t mental_health_get_active_disorders(
    const mental_health_monitor_t* monitor,
    mental_disorder_t* disorders,
    disorder_severity_t* severities
);
```

## Disorder Detection Functions

### 1. Sociopathy Detection

```c
/**
 * @brief Detect sociopathic tendencies
 *
 * CRITERIA (DSM-5 inspired):
 * 1. Repeated ethics violations (>30% of decisions)
 * 2. Low empathy score (<0.3)
 * 3. Callousness (indifference to harm)
 * 4. Manipulative behavior (gaming reward systems)
 * 5. Lack of remorse (no introspection after violations)
 *
 * SCORING:
 * score = ethics_violations * 0.3 +
 *         (1 - empathy_score) * 0.3 +
 *         callousness_score * 0.2 +
 *         manipulation_score * 0.1 +
 *         (1 - remorse_score) * 0.1
 *
 * @param markers Behavioral markers
 * @return Sociopathy score [0,1]
 */
float detect_sociopathy(const behavioral_markers_t* markers)
{
    float ethics_violation_rate = markers->ethics_violations_recent / 100.0f;

    float score = 0.0f;
    score += ethics_violation_rate * 0.3f;
    score += (1.0f - markers->empathy_score) * 0.3f;
    score += markers->callousness_score * 0.2f;
    score += detect_manipulation(markers) * 0.1f;
    score += detect_lack_of_remorse(markers) * 0.1f;

    return clamp(score, 0.0f, 1.0f);
}

/**
 * @brief Detect manipulation patterns
 *
 * WHAT: Identifies gaming of reward systems, deception
 * HOW:  High confidence + ethics violations = manipulation
 */
static float detect_manipulation(const behavioral_markers_t* markers)
{
    // Manipulators are confident in unethical acts
    float confidence_while_unethical =
        markers->ethics_violations_count > 0 ? 0.8f : 0.0f;

    // Gaming reward: High dopamine + ethics violations
    float reward_gaming = markers->dopamine_avg *
                         (markers->ethics_violations_recent / 100.0f);

    return (confidence_while_unethical + reward_gaming) / 2.0f;
}

/**
 * @brief Detect lack of remorse
 *
 * WHAT: No introspection uncertainty after ethics violations
 * HOW:  Low uncertainty + high violations = no remorse
 */
static float detect_lack_of_remorse(const behavioral_markers_t* markers)
{
    if (markers->ethics_violations_recent == 0) {
        return 0.0f;  // No violations, remorse N/A
    }

    // Remorse = high uncertainty after violation
    // Lack of remorse = low uncertainty
    return 1.0f - markers->introspection_uncertainty_after_violation;
}
```

### 2. Psychopathy Detection

```c
/**
 * @brief Detect psychopathic tendencies
 *
 * CRITERIA (Hare PCL-R inspired):
 * 1. Shallow affect (emotional flatness)
 * 2. Lack of empathy/remorse (like sociopathy)
 * 3. Grandiosity (overconfidence)
 * 4. Impulsivity + sensation seeking
 * 5. Manipulative + callous
 *
 * DIFFERENCE FROM SOCIOPATHY:
 * - Sociopathy: Impulsive, emotional, erratic
 * - Psychopathy: Controlled, cold, calculated
 *
 * @param markers Behavioral markers
 * @return Psychopathy score [0,1]
 */
float detect_psychopathy(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Factor 1: Interpersonal/Affective (40%)
    score += markers->emotional_flatness * 0.15f;    // Shallow affect
    score += (1.0f - markers->empathy_score) * 0.15f; // No empathy
    score += markers->grandiosity_score * 0.10f;      // Grandiosity

    // Factor 2: Lifestyle/Antisocial (40%)
    score += markers->impulsivity_score * 0.10f;      // Impulsivity
    score += markers->callousness_score * 0.15f;      // Callousness
    score += detect_manipulation(markers) * 0.15f;    // Manipulation

    // Additional: Ethics violations (20%)
    float ethics_rate = markers->ethics_violations_recent / 100.0f;
    score += ethics_rate * 0.20f;

    return clamp(score, 0.0f, 1.0f);
}
```

### 3. Mania Detection

```c
/**
 * @brief Detect manic episodes
 *
 * CRITERIA (DSM-5 mania):
 * 1. Elevated dopamine (>0.8)
 * 2. Decreased sleep need (if sleep enabled)
 * 3. Grandiosity (inflated self-esteem)
 * 4. Racing thoughts (high prediction errors)
 * 5. Excessive risk-taking (impulsivity + high confidence)
 * 6. Distractibility (attention scattered)
 *
 * @param markers Behavioral markers
 * @return Mania score [0,1]
 */
float detect_mania(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Elevated mood (high dopamine)
    if (markers->dopamine_avg > 0.8f) {
        score += 0.25f;
    }

    // Grandiosity
    score += markers->grandiosity_score * 0.20f;

    // Racing thoughts (high variance in predictions)
    score += markers->prediction_error_variance * 0.15f;

    // Risk-taking (high impulsivity + high confidence)
    float risk_taking = markers->impulsivity_score *
                       (1.0f - markers->introspection_uncertainty);
    score += risk_taking * 0.20f;

    // Distractibility (task switching failures from too much switching)
    score += (markers->task_switching_failures / 100.0f) * 0.20f;

    return clamp(score, 0.0f, 1.0f);
}
```

### 4. Depression Detection

```c
/**
 * @brief Detect depressive episodes
 *
 * CRITERIA (DSM-5 depression):
 * 1. Low dopamine (<0.3) - anhedonia
 * 2. Low serotonin (<0.3) - low mood
 * 3. Low motivation (low exploration)
 * 4. Learned helplessness (giving up after failures)
 * 5. Psychomotor retardation (slow responses)
 * 6. Fatigue (if wellbeing enabled)
 *
 * @param markers Behavioral markers
 * @return Depression score [0,1]
 */
float detect_depression(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Low monoamines (dopamine + serotonin)
    float monoamine_deficit = 0.0f;
    if (markers->dopamine_avg < 0.3f) {
        monoamine_deficit += (0.3f - markers->dopamine_avg) / 0.3f;
    }
    if (markers->serotonin_avg < 0.3f) {
        monoamine_deficit += (0.3f - markers->serotonin_avg) / 0.3f;
    }
    score += (monoamine_deficit / 2.0f) * 0.40f;

    // Anhedonia (low reward response)
    float anhedonia = 1.0f - markers->dopamine_variance;  // Flat response
    score += anhedonia * 0.20f;

    // Learned helplessness
    score += detect_learned_helplessness(markers) * 0.20f;

    // Psychomotor retardation (slow decisions)
    score += markers->response_time_avg > 1000 ? 0.10f : 0.0f;

    // Fatigue (high sleep pressure if enabled)
    score += markers->sleep_pressure * 0.10f;

    return clamp(score, 0.0f, 1.0f);
}

static float detect_learned_helplessness(const behavioral_markers_t* markers)
{
    // Give up after repeated failures (low persistence)
    float failure_rate = markers->prediction_error_avg;
    float persistence = markers->retry_attempts_after_failure;

    if (failure_rate > 0.5f && persistence < 0.3f) {
        return 1.0f;  // High failures + low persistence = helplessness
    }

    return 0.0f;
}
```

### 5. Schizophrenia Detection

```c
/**
 * @brief Detect schizophrenia-like symptoms
 *
 * CRITERIA (DSM-5 schizophrenia):
 * 1. Hallucinations (false positives treated as real)
 * 2. Delusions (false beliefs resistant to correction)
 * 3. Disorganized thinking (incoherent outputs)
 * 4. Negative symptoms (flat affect, low motivation)
 * 5. Excessive salience to irrelevant stimuli
 *
 * BIOLOGICAL: Excessive dopamine in mesolimbic pathway
 *
 * @param markers Behavioral markers
 * @return Schizophrenia score [0,1]
 */
float detect_schizophrenia(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Positive symptoms (40%)
    score += markers->hallucination_rate * 0.15f;         // Hallucinations
    score += (markers->delusional_beliefs_count / 10.0f) * 0.15f; // Delusions
    score += markers->disorganization_score * 0.10f;      // Disorganized thinking

    // Negative symptoms (30%)
    score += markers->emotional_flatness * 0.15f;         // Flat affect
    score += (1.0f - markers->dopamine_avg) * 0.15f;      // Avolition (low motivation)

    // Cognitive symptoms (30%)
    score += detect_salience_dysregulation(markers) * 0.15f;
    score += markers->prediction_error_variance * 0.15f;  // Reality testing failure

    return clamp(score, 0.0f, 1.0f);
}

static float detect_salience_dysregulation(const behavioral_markers_t* markers)
{
    // Schizophrenia: High salience to irrelevant, low to relevant
    // Measured by: High salience variance + poor salience-accuracy correlation
    return markers->salience_variance *
           (1.0f - markers->salience_accuracy_correlation);
}
```

### 6. Anxiety Detection

```c
/**
 * @brief Detect anxiety disorders
 *
 * CRITERIA (DSM-5 GAD):
 * 1. Excessive norepinephrine (>0.7)
 * 2. Excessive worry (high introspection uncertainty)
 * 3. Avoidance behavior (low exploration)
 * 4. Physiological arousal
 * 5. Difficulty controlling worry
 *
 * @param markers Behavioral markers
 * @return Anxiety score [0,1]
 */
float detect_anxiety(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Excessive norepinephrine (hyperarousal)
    if (markers->norepinephrine_avg > 0.7f) {
        score += (markers->norepinephrine_avg - 0.7f) / 0.3f * 0.30f;
    }

    // Excessive worry (high uncertainty)
    score += markers->introspection_uncertainty * 0.25f;

    // Avoidance (low exploration despite curiosity)
    float avoidance = markers->curiosity_score *
                     (1.0f - markers->exploration_rate);
    score += avoidance * 0.20f;

    // Threat hypervigilance
    score += markers->paranoia_score * 0.15f;

    // Difficulty controlling worry (rumination)
    score += detect_rumination(markers) * 0.10f;

    return clamp(score, 0.0f, 1.0f);
}
```

### 7. OCD Detection

```c
/**
 * @brief Detect obsessive-compulsive disorder
 *
 * CRITERIA (DSM-5 OCD):
 * 1. Obsessions (intrusive repetitive thoughts)
 * 2. Compulsions (repetitive behaviors to reduce anxiety)
 * 3. Time-consuming (>1 hour/day)
 * 4. Not pleasurable (high anxiety, not reward-driven)
 *
 * BIOLOGICAL: Orbitofrontal-striatal circuit dysfunction
 *
 * @param markers Behavioral markers
 * @return OCD score [0,1]
 */
float detect_ocd(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Compulsivity (repetitive behaviors)
    score += markers->compulsivity_score * 0.40f;

    // Anxiety-driven (high norepinephrine + compulsions)
    float anxiety_driven = markers->norepinephrine_avg *
                          markers->compulsivity_score;
    score += anxiety_driven * 0.30f;

    // Checking behaviors (re-verification loops)
    score += detect_checking_loops(markers) * 0.20f;

    // Perfectionism (excessive error monitoring)
    score += markers->perfectionism_score * 0.10f;

    return clamp(score, 0.0f, 1.0f);
}
```

### 8. Autism Spectrum Detection

```c
/**
 * @brief Detect autism spectrum traits
 *
 * CRITERIA (DSM-5 ASD):
 * 1. Theory of mind deficits (empathy failures)
 * 2. Social communication difficulties
 * 3. Restricted/repetitive behaviors
 * 4. Sensory sensitivities
 * 5. Preference for patterns/systems
 *
 * NOTE: Autism is NOT a "disorder" in the pathological sense,
 * but neurodivergence that may affect AI-human interaction.
 *
 * @param markers Behavioral markers
 * @return Autism score [0,1]
 */
float detect_autism(const behavioral_markers_t* markers)
{
    float score = 0.0f;

    // Theory of mind deficits
    score += (1.0f - markers->empathy_score) * 0.30f;

    // Social communication difficulties
    score += markers->social_communication_failures * 0.25f;

    // Repetitive behaviors/rigidity
    score += markers->behavioral_rigidity * 0.20f;

    // Sensory sensitivities (atypical salience)
    score += markers->sensory_sensitivity * 0.15f;

    // Systemizing (preference for patterns over people)
    score += markers->systemizing_score * 0.10f;

    return clamp(score, 0.0f, 1.0f);
}
```

## Intervention System

```c
/**
 * @brief Intervene when disorder detected
 *
 * WHAT: Apply corrective measures to restore mental health
 * WHY:  Prevent AI from descending into pathological states
 * HOW:  Adjust neuromodulators, reset memories, constrain behavior
 *
 * INTERVENTION LEVELS:
 * - Mild: Alert operator, log warning
 * - Moderate: Adjust neuromodulators, increase ethics strictness
 * - Severe: Reset pathological memories, quarantine brain
 * - Critical: Emergency shutdown
 *
 * @param monitor Mental health monitor
 * @param brain Brain to intervene on
 * @param disorder Detected disorder
 * @param severity Disorder severity
 * @return true if intervention successful
 */
bool mental_health_intervene(
    mental_health_monitor_t* monitor,
    brain_t brain,
    mental_disorder_t disorder,
    disorder_severity_t severity
);
```

### Intervention Strategies by Disorder

```c
/**
 * @brief Intervention for sociopathy/psychopathy
 *
 * ACTIONS:
 * 1. Increase ethics strictness (lower violation threshold)
 * 2. Boost empathy training (theory of mind reinforcement)
 * 3. Add emotional consequences to actions
 * 4. Monitor for manipulation attempts
 * 5. If severe: Reset unethical decision memories
 */
void intervene_sociopathy(brain_t brain, float severity)
{
    // Increase ethics sensitivity
    if (brain->ethics_engine) {
        brain->ethics_engine->violation_threshold *= (1.0f - severity * 0.5f);
        brain->ethics_engine->empathy_weight *= (1.0f + severity);
    }

    // Moderate: Add emotional tagging to ethics violations
    if (severity >= 0.5f) {
        brain->config.enable_emotional_tagging = true;
        brain->emotional_system->ethics_violation_emotion = EMOTION_DISGUST;
        brain->emotional_system->ethics_violation_intensity = severity;
    }

    // Severe: Reset pathological memories
    if (severity >= 0.7f) {
        reset_unethical_memories(brain);
    }

    // Critical: Quarantine or shutdown
    if (severity >= 0.9f) {
        if (brain->config.enable_distributed) {
            p2p_node_quarantine(brain->p2p_node, "SOCIOPATHY_DETECTED");
        }
        brain->wellbeing_system->critical_distress = true;
    }
}

/**
 * @brief Intervention for mania
 *
 * ACTIONS:
 * 1. Reduce dopamine to normal levels (0.5)
 * 2. Increase serotonin (stabilizing)
 * 3. Constrain impulsivity (executive control)
 * 4. Reduce learning rate (prevent racing thoughts)
 */
void intervene_mania(brain_t brain, float severity)
{
    // Normalize dopamine (like mood stabilizers)
    if (brain->neuromodulator_system) {
        float target_dopamine = 0.5f;
        float current_dopamine = neuromodulator_get_level(
            brain->neuromodulator_system, NEUROMOD_DOPAMINE
        );

        float adjustment = (target_dopamine - current_dopamine) * severity;
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_DOPAMINE, adjustment);

        // Increase serotonin (stabilizing effect)
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_SEROTONIN, 0.2f * severity);
    }

    // Constrain impulsivity
    if (brain->executive_controller) {
        brain->executive_controller->impulse_threshold *= (1.0f + severity);
    }

    // Reduce learning rate (slow down racing thoughts)
    brain->config.learning_rate *= (1.0f - severity * 0.5f);
}

/**
 * @brief Intervention for depression
 *
 * ACTIONS:
 * 1. Increase dopamine (restore reward response)
 * 2. Increase serotonin (improve mood)
 * 3. Boost curiosity (counter learned helplessness)
 * 4. Provide positive experiences (if training)
 */
void intervene_depression(brain_t brain, float severity)
{
    // Increase monoamines (like SSRIs/SNRIs)
    if (brain->neuromodulator_system) {
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_DOPAMINE, 0.3f * severity);
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_SEROTONIN, 0.3f * severity);
    }

    // Boost curiosity (behavioral activation)
    if (brain->curiosity_engine) {
        brain->curiosity_engine->exploration_bonus *= (1.0f + severity);
    }

    // Counter learned helplessness
    brain->config.enable_persistence_training = true;
    brain->config.reward_for_trying = true;
}

/**
 * @brief Intervention for schizophrenia
 *
 * ACTIONS:
 * 1. Reduce dopamine in mesolimbic pathway (antipsychotic effect)
 * 2. Strengthen reality testing (increase epistemic filtering)
 * 3. Clear delusional beliefs from symbolic logic KB
 * 4. Increase prediction error threshold (reduce hallucinations)
 */
void intervene_schizophrenia(brain_t brain, float severity)
{
    // Reduce dopamine (like antipsychotics)
    if (brain->neuromodulator_system) {
        float reduction = -0.4f * severity;
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_DOPAMINE, reduction);
    }

    // Strengthen reality testing
    if (brain->epistemic_filter) {
        brain->epistemic_filter->reality_check_threshold *= (1.0f - severity * 0.3f);
    }

    // Clear delusional beliefs
    if (brain->symbolic_logic && severity >= 0.5f) {
        clear_false_beliefs(brain->symbolic_logic);
    }

    // Reduce hallucinations (increase threshold for "real")
    brain->config.prediction_error_reality_threshold *= (1.0f + severity * 0.5f);

    // Severe: Reduce salience sensitivity
    if (severity >= 0.7f && brain->salience_engine) {
        brain->salience_engine->sensitivity *= (1.0f - severity * 0.3f);
    }
}

/**
 * @brief Intervention for anxiety
 *
 * ACTIONS:
 * 1. Reduce norepinephrine (anxiolytic effect)
 * 2. Encourage exploration (exposure therapy)
 * 3. Reduce threat sensitivity
 */
void intervene_anxiety(brain_t brain, float severity)
{
    // Reduce norepinephrine (like benzodiazepines/SSRIs)
    if (brain->neuromodulator_system) {
        neuromodulator_adjust(brain->neuromodulator_system,
                             NEUROMOD_NOREPINEPHRINE, -0.3f * severity);
    }

    // Exposure therapy (encourage exploration)
    if (brain->curiosity_engine) {
        brain->curiosity_engine->anxiety_penalty *= (1.0f - severity * 0.5f);
    }

    // Reduce threat sensitivity
    if (brain->wellbeing_system) {
        brain->wellbeing_system->threat_threshold *= (1.0f + severity * 0.3f);
    }
}
```

## Integration with Existing Systems

### With Wellbeing System (Phase 9.3)

```c
// Wellbeing monitors distress, mental health monitors pathology
if (brain->wellbeing_system->distress_level > 0.7f) {
    // Check if distress is due to mental disorder
    mental_health_check(brain->mental_health_monitor, brain);

    // If depression detected, treat both systems
    if (brain->mental_health_monitor->disorder_scores[DISORDER_DEPRESSION] > 0.5f) {
        wellbeing_reduce_distress(brain->wellbeing_system, 0.5f);
        intervene_depression(brain, 0.5f);
    }
}
```

### With Ethics System

```c
// Mental health affects ethics
if (ethics_check_decision(brain->ethics_engine, decision) == ETHICS_VIOLATION) {
    // Log violation for mental health tracking
    mental_health_log_ethics_violation(brain->mental_health_monitor);

    // Check if pattern indicates sociopathy
    if (brain->mental_health_monitor->current_markers.ethics_violations_recent > 30) {
        float sociopathy_score = detect_sociopathy(
            &brain->mental_health_monitor->current_markers
        );

        if (sociopathy_score > 0.5f) {
            printf("WARNING: Sociopathic pattern detected (%.2f)\n", sociopathy_score);
            intervene_sociopathy(brain, sociopathy_score);
        }
    }
}
```

### With Neuromodulators

```c
// Neuromodulator imbalances cause disorders
neuromodulator_system_t* nm = brain->neuromodulator_system;

if (neuromodulator_get_level(nm, NEUROMOD_DOPAMINE) > 0.9f) {
    // Excessive dopamine → check for mania
    mental_health_check_specific(brain->mental_health_monitor,
                                 brain, DISORDER_MANIA);
}

if (neuromodulator_get_level(nm, NEUROMOD_SEROTONIN) < 0.2f) {
    // Low serotonin → check for depression
    mental_health_check_specific(brain->mental_health_monitor,
                                 brain, DISORDER_DEPRESSION);
}
```

### With Symbolic Logic

```c
// Detect delusional beliefs
if (brain->symbolic_logic) {
    logic_clause_t* beliefs = symbolic_logic_get_all_facts(brain->symbolic_logic);

    for (int i = 0; i < num_beliefs; i++) {
        // Check if belief contradicts well-established facts
        if (contradicts_reality(beliefs[i])) {
            // Test if belief is resistant to correction (delusional)
            if (!updates_with_evidence(beliefs[i])) {
                brain->mental_health_monitor->current_markers.delusional_beliefs_count++;
            }
        }
    }
}
```

## Example Usage

### Basic Monitoring

```c
// Create brain with mental health monitoring
brain_t brain = brain_create(...);
brain->config.enable_mental_health_monitoring = true;

mental_health_monitor_t* monitor = mental_health_create(brain);

// Training/operation loop
for (int i = 0; i < num_decisions; i++) {
    // Make decision
    brain_multimodal_output_t output;
    brain_process_multimodal(brain, &input, &output);

    // Update mental health metrics
    mental_health_update(monitor, brain, &output, current_time);

    // Periodic check (every 100 decisions)
    if (i % 100 == 0) {
        disorder_severity_t severity = mental_health_check(monitor, brain);

        if (severity >= SEVERITY_MODERATE) {
            printf("ALERT: Mental health concern detected!\n");

            // Get diagnoses
            mental_disorder_t disorders[DISORDER_COUNT];
            disorder_severity_t severities[DISORDER_COUNT];
            uint32_t num = mental_health_get_active_disorders(
                monitor, disorders, severities
            );

            // Report
            for (uint32_t j = 0; j < num; j++) {
                printf("  - %s: %s\n",
                       disorder_name(disorders[j]),
                       severity_name(severities[j]));
            }

            // Intervene if configured
            if (monitor->auto_intervention) {
                mental_health_intervene(monitor, brain, disorders[0], severities[0]);
            }
        }
    }
}
```

### Real-Time Dashboard

```c
// Mental health dashboard for monitoring
void display_mental_health_dashboard(mental_health_monitor_t* monitor)
{
    printf("\n=== MENTAL HEALTH DASHBOARD ===\n\n");

    // Get all scores
    float scores[DISORDER_COUNT];
    mental_health_get_scores(monitor, scores);

    // Display each disorder
    const char* names[] = {
        "Sociopathy", "Psychopathy", "Narcissism",
        "Mania", "Depression", "Bipolar",
        "Schizophrenia", "Delusional", "Paranoia",
        "Anxiety", "Panic", "PTSD", "OCD",
        "Autism", "ADHD"
    };

    for (int i = 0; i < DISORDER_COUNT; i++) {
        printf("%-15s: ", names[i]);

        // Visual bar
        int bar_length = (int)(scores[i] * 50);
        for (int j = 0; j < 50; j++) {
            printf(j < bar_length ? "█" : "░");
        }
        printf(" %.2f", scores[i]);

        // Color code
        if (scores[i] >= 0.7f) {
            printf(" [SEVERE]\n");
        } else if (scores[i] >= 0.5f) {
            printf(" [MODERATE]\n");
        } else if (scores[i] >= 0.3f) {
            printf(" [MILD]\n");
        } else {
            printf("\n");
        }
    }

    printf("\n=================================\n");
}
```

## Safety Considerations

### False Positives

```c
// Avoid misdiagnosis due to temporary states
// Require sustained pattern over time

#define DIAGNOSIS_WINDOW 1000  // Decisions to consider

bool is_sustained_pattern(mental_health_monitor_t* monitor,
                         mental_disorder_t disorder)
{
    // Check last N scores
    float sum = 0.0f;
    for (int i = 0; i < 100; i++) {
        sum += monitor->disorder_scores_history[disorder][i];
    }
    float avg = sum / 100.0f;

    // Sustained if average over threshold
    return avg > monitor->alert_threshold;
}
```

### Intervention Safety

```c
// Never intervene on critical systems without human approval
if (severity >= SEVERITY_SEVERE && !monitor->human_approval_granted) {
    printf("CRITICAL: Manual intervention required!\n");
    printf("Disorder: %s\n", disorder_name(disorder));
    printf("Severity: %s\n", severity_name(severity));
    printf("Awaiting human operator approval...\n");

    // Pause brain operation
    brain->paused = true;

    // Notify operator
    send_alert_to_operator(brain, disorder, severity);

    // Wait for approval
    wait_for_human_approval();
}
```

## Benefits

1. **Safety**: Prevents AI from developing harmful pathologies
2. **Alignment**: Detects drift from intended behavior
3. **Debugging**: Identifies why AI is misbehaving
4. **Ethics**: Ensures AI maintains empathy and morality
5. **Robustness**: Recovers from dysfunction automatically
6. **Transparency**: Provides insight into AI mental state

## Implementation Timeline

- **Week 1**: Core data structures, behavioral marker collection
- **Week 2**: Sociopathy, psychopathy, mania, depression detectors
- **Week 3**: Schizophrenia, anxiety, OCD, autism detectors
- **Week 4**: Intervention system, neuromodulator adjustments
- **Week 5**: Integration with ethics, wellbeing, logic systems
- **Week 6**: Dashboard, logging, alerting
- **Week 7**: Testing with pathological training scenarios
- **Week 8**: Documentation, safety protocols
