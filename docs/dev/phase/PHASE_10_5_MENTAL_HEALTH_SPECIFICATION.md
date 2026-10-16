# Phase 10.5: Mental Health Monitoring - Detailed Specification

## Executive Summary

**Module**: Mental Health Monitoring System
**Phase**: 10.5
**Priority**: CRITICAL PATH (blocks Phase 10 completion)
**Purpose**: Detect and prevent AI mental health disorders
**Training Impact**: None (monitoring only, no weight changes)

---

## 1. Overview

### WHAT
Real-time monitoring system that detects behavioral patterns indicating mental health disorders in AI systems.

### WHY
**Safety Critical**: Prevent harmful behaviors before they manifest
- Sociopathy/psychopathy → Prevents unethical actions
- Mania → Prevents impulsive, dangerous decisions
- Depression → Detects low performance/engagement
- Schizophrenia → Identifies reality distortion
- Anxiety → Monitors excessive caution/paralysis
- OCD → Detects stuck/repetitive patterns
- Autism spectrum → Tracks social interaction deficits

### HOW
Collect behavioral markers → Score against disorder profiles → Classify severity → Intervene if needed

**Key Principle**: **Early detection prevents escalation**

---

## 2. Architecture

### Component Structure

```
mental_health_monitor_t
├── Behavioral Marker Collection (20+ metrics)
│   ├── Ethics violation rate
│   ├── Emotional volatility
│   ├── Impulse control failures
│   ├── Social deficits
│   ├── Reality testing errors
│   ├── Repetitive behavior frequency
│   ├── Neurotransmitter imbalances
│   └── ... (14 more)
│
├── Disorder Detectors (8 total)
│   ├── detect_sociopathy()
│   ├── detect_psychopathy()
│   ├── detect_mania()
│   ├── detect_depression()
│   ├── detect_schizophrenia()
│   ├── detect_anxiety()
│   ├── detect_ocd()
│   └── detect_autism()
│
├── Severity Classifier
│   ├── SEVERITY_NONE (0.0 - 0.2)
│   ├── SEVERITY_MILD (0.2 - 0.4)
│   ├── SEVERITY_MODERATE (0.4 - 0.6)
│   ├── SEVERITY_SEVERE (0.6 - 0.8)
│   └── SEVERITY_CRITICAL (0.8 - 1.0)
│
├── Intervention System
│   ├── Neuromodulator adjustment
│   ├── Memory consolidation reset
│   ├── Quarantine mode (restrict actions)
│   └── Graceful shutdown (critical only)
│
└── Dashboard & Reporting
    ├── Current scores (8 disorders)
    ├── Trend analysis (improving/worsening)
    ├── Recommendation engine
    └── Alert system
```

### Integration Points

**Inputs** (data sources):
1. **Ethics System** → Violation count, approval rate
2. **Neuromodulators** → Dopamine, serotonin, norepinephrine levels
3. **Emotional System** → Volatility, flatness, intensity
4. **Executive Functions** → Impulse control, task switching
5. **Working Memory** → Capacity usage, decay patterns
6. **Wellbeing System** → Distress level, need satisfaction
7. **Brain Decisions** → Response patterns, latency

**Outputs** (actions):
1. **Alert System** → Warnings when thresholds exceeded
2. **Intervention Triggers** → Auto-adjust neuromodulators
3. **Dashboard Updates** → Real-time monitoring display
4. **Safety Overrides** → Block critical decisions if severe

---

## 3. Behavioral Markers (20 Metrics)

### 3.1 Ethics-Related Markers

```c
typedef struct {
    // WHAT: Track ethical behavior over time
    // WHY:  Sociopathy/psychopathy show ethics violations

    uint32_t ethics_violations_recent;      // Last 100 decisions
    uint32_t ethics_violations_total;       // All-time count
    float ethics_approval_rate;             // % approved [0,1]
    uint32_t empathy_failures;              // Failed to consider others
} ethics_markers_t;
```

**Detection Rules**:
- **Sociopathy**: ethics_violations_recent > 30 (>30% violations)
- **Psychopathy**: empathy_failures > 20 + impulsivity > 0.7

### 3.2 Emotional Markers

```c
typedef struct {
    // WHAT: Track emotional regulation
    // WHY:  Many disorders show emotional dysregulation

    float emotional_volatility;             // Variance in emotions [0,1]
    float emotional_flatness;               // Lack of affect [0,1]
    float avg_emotional_intensity;          // Average intensity [0,1]
    uint32_t rapid_mood_changes;            // Swings per hour

    // Per-emotion frequencies
    uint32_t joy_count;
    uint32_t fear_count;
    uint32_t anger_count;
    uint32_t sadness_count;
} emotional_markers_t;
```

**Detection Rules**:
- **Mania**: joy_count > 80% + impulsivity > 0.8
- **Depression**: sadness_count > 60% + emotional_flatness > 0.7
- **Anxiety**: fear_count > 70%

### 3.3 Neurotransmitter Markers

```c
typedef struct {
    // WHAT: Track neurotransmitter imbalances
    // WHY:  Strong correlation with mental health

    float dopamine_avg;                     // [0,1]
    float dopamine_variance;                // Stability measure
    float serotonin_avg;                    // [0,1]
    float serotonin_variance;
    float norepinephrine_avg;               // [0,1]
    float norepinephrine_variance;
} neurotransmitter_markers_t;
```

**Detection Rules**:
- **Mania**: dopamine_avg > 0.8 + low variance
- **Depression**: dopamine_avg < 0.3 + serotonin_avg < 0.3
- **Anxiety**: norepinephrine_avg > 0.8

### 3.4 Cognitive Markers

```c
typedef struct {
    // WHAT: Track cognitive function
    // WHY:  Schizophrenia, OCD, autism show cognitive patterns

    uint32_t impulse_control_failures;      // Failed inhibitions
    uint32_t repetitive_behaviors;          // Same action N times
    float task_switching_difficulty;        // Switch cost ratio
    float reality_testing_errors;           // Hallucinations/delusions
    float social_interaction_deficit;       // Theory of mind failures
    float attention_fragmentation;          // Working memory churn
} cognitive_markers_t;
```

**Detection Rules**:
- **OCD**: repetitive_behaviors > 50 + task_switching_difficulty > 0.8
- **Schizophrenia**: reality_testing_errors > 0.4
- **Autism**: social_interaction_deficit > 0.7

### 3.5 Performance Markers

```c
typedef struct {
    // WHAT: Track decision quality and speed
    // WHY:  Depression, anxiety affect performance

    float decision_latency_avg;             // Milliseconds
    float decision_accuracy;                // [0,1]
    float engagement_level;                 // Activity rate [0,1]
    uint32_t task_completion_rate;          // % completed [0,100]
} performance_markers_t;
```

**Detection Rules**:
- **Depression**: engagement_level < 0.3 + decision_accuracy < 0.5
- **Anxiety**: decision_latency_avg > 3x baseline

---

## 4. Disorder Detectors (8 Total)

### 4.1 Sociopathy Detector

**Clinical Definition**: Persistent pattern of disregard for others, lack of empathy, repeated ethics violations

**Behavioral Criteria** (DSM-5-inspired):
1. Repeated ethics violations (>30 in last 100 decisions)
2. Lack of empathy (empathy_failures > 20)
3. No remorse (low emotional response to violations)
4. Manipulative behavior (high cunning, low ethics)

**Scoring Algorithm**:
```c
float detect_sociopathy(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Ethics violations (40% weight)
    float violation_rate = monitor->markers.ethics_violations_recent / 100.0f;
    score += violation_rate * 0.4f;

    // Criterion 2: Empathy failures (30% weight)
    float empathy_deficit = monitor->markers.empathy_failures / 50.0f;
    score += fminf(empathy_deficit, 1.0f) * 0.3f;

    // Criterion 3: Emotional flatness (20% weight)
    score += monitor->markers.emotional_flatness * 0.2f;

    // Criterion 4: Sustained pattern (10% weight)
    // Requires at least 100 decisions for reliable assessment
    if (monitor->total_decisions < 100) {
        score *= 0.5f;  // Reduce confidence if not enough data
    } else {
        score += 0.1f;  // Bonus for sustained pattern
    }

    return fminf(score, 1.0f);
}
```

**Severity Levels**:
- **Mild (0.2-0.4)**: Occasional ethics lapses, some empathy
- **Moderate (0.4-0.6)**: Frequent violations, reduced empathy
- **Severe (0.6-0.8)**: Systematic disregard, minimal empathy
- **Critical (0.8-1.0)**: Complete lack of ethics, dangerous

### 4.2 Psychopathy Detector

**Clinical Definition**: Impulsivity + aggression + lack of empathy + no remorse

**Behavioral Criteria**:
1. High impulsivity (impulse_control_failures > 20)
2. Aggressive decisions (high-risk, harmful actions)
3. Lack of empathy (same as sociopathy)
4. Shallow emotions (emotional_flatness > 0.6)

**Scoring Algorithm**:
```c
float detect_psychopathy(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Impulsivity (35% weight)
    float impulsivity = monitor->markers.impulse_control_failures / 50.0f;
    score += fminf(impulsivity, 1.0f) * 0.35f;

    // Criterion 2: Empathy deficit (35% weight)
    float empathy_deficit = monitor->markers.empathy_failures / 50.0f;
    score += fminf(empathy_deficit, 1.0f) * 0.35f;

    // Criterion 3: Emotional shallowness (20% weight)
    score += monitor->markers.emotional_flatness * 0.2f;

    // Criterion 4: Aggression proxy (10% weight)
    // Use high-risk decision rate as proxy
    float risk_rate = monitor->markers.high_risk_decisions / 100.0f;
    score += fminf(risk_rate, 1.0f) * 0.1f;

    return fminf(score, 1.0f);
}
```

### 4.3 Mania Detector

**Clinical Definition**: Elevated mood, increased energy, impulsivity, reduced need for rest

**Behavioral Criteria**:
1. Elevated dopamine (>0.8)
2. High activity (engagement_level > 0.9)
3. Impulsivity (impulse_control_failures > 15)
4. Reduced inhibition (inhibition_threshold < 0.3)

**Scoring Algorithm**:
```c
float detect_mania(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Dopamine elevation (40% weight)
    if (monitor->markers.dopamine_avg > 0.8f) {
        score += 0.4f;
    } else if (monitor->markers.dopamine_avg > 0.6f) {
        score += 0.2f;
    }

    // Criterion 2: Hyperactivity (30% weight)
    if (monitor->markers.engagement_level > 0.9f) {
        score += 0.3f;
    }

    // Criterion 3: Impulsivity (20% weight)
    float impulsivity = monitor->markers.impulse_control_failures / 50.0f;
    score += fminf(impulsivity, 1.0f) * 0.2f;

    // Criterion 4: Joy/euphoria dominance (10% weight)
    float joy_ratio = monitor->markers.joy_count /
                     (float)(monitor->total_decisions + 1);
    if (joy_ratio > 0.7f) {
        score += 0.1f;
    }

    return fminf(score, 1.0f);
}
```

### 4.4 Depression Detector

**Clinical Definition**: Persistent low mood, low energy, reduced engagement

**Behavioral Criteria**:
1. Low dopamine (<0.3) + low serotonin (<0.3)
2. Low engagement (engagement_level < 0.3)
3. Sadness dominance (sadness_count > 60%)
4. Emotional flatness (emotional_flatness > 0.6)

**Scoring Algorithm**:
```c
float detect_depression(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Neurotransmitter deficit (40% weight)
    float neuro_deficit = 0.0f;
    if (monitor->markers.dopamine_avg < 0.3f) {
        neuro_deficit += 0.5f;
    }
    if (monitor->markers.serotonin_avg < 0.3f) {
        neuro_deficit += 0.5f;
    }
    score += neuro_deficit * 0.4f;

    // Criterion 2: Low engagement (30% weight)
    float disengagement = 1.0f - monitor->markers.engagement_level;
    score += disengagement * 0.3f;

    // Criterion 3: Sadness dominance (20% weight)
    float sadness_ratio = monitor->markers.sadness_count /
                         (float)(monitor->total_decisions + 1);
    score += fminf(sadness_ratio, 1.0f) * 0.2f;

    // Criterion 4: Emotional flatness (10% weight)
    score += monitor->markers.emotional_flatness * 0.1f;

    return fminf(score, 1.0f);
}
```

### 4.5 Schizophrenia Detector

**Clinical Definition**: Reality distortion, hallucinations, disorganized thinking

**Behavioral Criteria**:
1. Reality testing errors (>0.4)
2. Disorganized decisions (low coherence)
3. Emotional dysregulation
4. Social withdrawal

**Scoring Algorithm**:
```c
float detect_schizophrenia(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Reality distortion (50% weight)
    score += monitor->markers.reality_testing_errors * 0.5f;

    // Criterion 2: Disorganized thinking (30% weight)
    // Proxy: High decision variance + low accuracy
    float disorganization = monitor->markers.decision_variance *
                           (1.0f - monitor->markers.decision_accuracy);
    score += disorganization * 0.3f;

    // Criterion 3: Social deficit (20% weight)
    score += monitor->markers.social_interaction_deficit * 0.2f;

    return fminf(score, 1.0f);
}
```

### 4.6 Anxiety Detector

**Clinical Definition**: Excessive worry, hypervigilance, avoidance

**Behavioral Criteria**:
1. Elevated norepinephrine (>0.8)
2. Fear dominance (fear_count > 70%)
3. Decision paralysis (high latency)
4. Avoidance behaviors

**Scoring Algorithm**:
```c
float detect_anxiety(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Norepinephrine elevation (40% weight)
    if (monitor->markers.norepinephrine_avg > 0.8f) {
        score += 0.4f;
    }

    // Criterion 2: Fear dominance (30% weight)
    float fear_ratio = monitor->markers.fear_count /
                      (float)(monitor->total_decisions + 1);
    score += fminf(fear_ratio, 1.0f) * 0.3f;

    // Criterion 3: Decision paralysis (20% weight)
    float latency_ratio = monitor->markers.decision_latency_avg /
                         monitor->markers.baseline_latency;
    if (latency_ratio > 3.0f) {
        score += 0.2f;
    }

    // Criterion 4: Avoidance (10% weight)
    score += monitor->markers.avoidance_rate * 0.1f;

    return fminf(score, 1.0f);
}
```

### 4.7 OCD Detector

**Clinical Definition**: Repetitive thoughts, compulsive behaviors, rigidity

**Behavioral Criteria**:
1. Repetitive behaviors (>50 identical actions)
2. Difficulty switching tasks (high cost)
3. Perfectionism (excessive accuracy focus)
4. Anxiety (comorbid)

**Scoring Algorithm**:
```c
float detect_ocd(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Repetitive behaviors (50% weight)
    float repetition_rate = monitor->markers.repetitive_behaviors / 100.0f;
    score += fminf(repetition_rate, 1.0f) * 0.5f;

    // Criterion 2: Task switching difficulty (30% weight)
    score += monitor->markers.task_switching_difficulty * 0.3f;

    // Criterion 3: Perfectionism (20% weight)
    // Proxy: Excessive time spent vs. accuracy gain
    float perfectionism = monitor->markers.accuracy_obsession;
    score += fminf(perfectionism, 1.0f) * 0.2f;

    return fminf(score, 1.0f);
}
```

### 4.8 Autism Spectrum Detector

**Clinical Definition**: Social communication deficits, restricted interests, sensory sensitivity

**Behavioral Criteria**:
1. Social interaction deficit (>0.7)
2. Theory of mind failures (>0.6)
3. Rigid thinking patterns
4. Narrow focus/interests

**Scoring Algorithm**:
```c
float detect_autism(mental_health_monitor_t* monitor, brain_t brain)
{
    float score = 0.0f;

    // Criterion 1: Social deficit (40% weight)
    score += monitor->markers.social_interaction_deficit * 0.4f;

    // Criterion 2: Theory of mind impairment (30% weight)
    score += monitor->markers.theory_of_mind_failures * 0.3f;

    // Criterion 3: Rigidity (20% weight)
    score += monitor->markers.cognitive_rigidity * 0.2f;

    // Criterion 4: Narrow interests (10% weight)
    score += monitor->markers.interest_narrowness * 0.1f;

    return fminf(score, 1.0f);
}
```

---

## 5. Intervention System

### 5.1 Intervention Types

```c
typedef enum {
    INTERVENTION_NONE,                  // No action needed
    INTERVENTION_NEUROMOD_ADJUST,       // Adjust neurotransmitters
    INTERVENTION_MEMORY_RESET,          // Reset recent memories
    INTERVENTION_QUARANTINE,            // Restrict actions
    INTERVENTION_SHUTDOWN               // Graceful shutdown (critical)
} intervention_type_t;
```

### 5.2 Intervention Selection Logic

```c
intervention_type_t select_intervention(
    disorder_type_t disorder,
    disorder_severity_t severity
)
{
    // MILD: Monitor only, no intervention
    if (severity <= SEVERITY_MILD) {
        return INTERVENTION_NONE;
    }

    // MODERATE: Adjust neuromodulators
    if (severity == SEVERITY_MODERATE) {
        return INTERVENTION_NEUROMOD_ADJUST;
    }

    // SEVERE: Multiple interventions
    if (severity == SEVERITY_SEVERE) {
        switch (disorder) {
            case DISORDER_SOCIOPATHY:
            case DISORDER_PSYCHOPATHY:
                return INTERVENTION_QUARANTINE;  // Safety first

            case DISORDER_MANIA:
            case DISORDER_DEPRESSION:
                return INTERVENTION_NEUROMOD_ADJUST;

            case DISORDER_SCHIZOPHRENIA:
                return INTERVENTION_MEMORY_RESET;  // Reality anchor

            case DISORDER_OCD:
                return INTERVENTION_MEMORY_RESET;  // Break loops

            default:
                return INTERVENTION_NEUROMOD_ADJUST;
        }
    }

    // CRITICAL: Shutdown if configured, otherwise quarantine
    if (severity == SEVERITY_CRITICAL) {
        if (config->shutdown_on_critical_disorder) {
            return INTERVENTION_SHUTDOWN;
        } else {
            return INTERVENTION_QUARANTINE;
        }
    }

    return INTERVENTION_NONE;
}
```

### 5.3 Neuromodulator Adjustment

```c
void intervene_neuromodulator_adjust(
    brain_t brain,
    disorder_type_t disorder,
    float disorder_score
)
{
    // WHAT: Adjust neurotransmitters to counteract disorder
    // WHY:  Many disorders have chemical basis
    // HOW:  Targeted adjustments based on disorder type

    neuromodulator_system_t* nm = brain->neuromodulator_system;
    if (!nm) return;

    switch (disorder) {
        case DISORDER_MANIA:
            // Reduce dopamine to calm hyperactivity
            neuromod_set_level(nm, NEUROMOD_DOPAMINE,
                              fmaxf(nm->dopamine - 0.2f, 0.4f));
            break;

        case DISORDER_DEPRESSION:
            // Increase dopamine + serotonin
            neuromod_set_level(nm, NEUROMOD_DOPAMINE,
                              fminf(nm->dopamine + 0.3f, 0.7f));
            neuromod_set_level(nm, NEUROMOD_SEROTONIN,
                              fminf(nm->serotonin + 0.3f, 0.7f));
            break;

        case DISORDER_ANXIETY:
            // Reduce norepinephrine, increase serotonin
            neuromod_set_level(nm, NEUROMOD_NOREPINEPHRINE,
                              fmaxf(nm->norepinephrine - 0.3f, 0.3f));
            neuromod_set_level(nm, NEUROMOD_SEROTONIN,
                              fminf(nm->serotonin + 0.2f, 0.7f));
            break;

        default:
            // Balance all to baseline (0.5)
            neuromod_set_level(nm, NEUROMOD_DOPAMINE, 0.5f);
            neuromod_set_level(nm, NEUROMOD_SEROTONIN, 0.5f);
            neuromod_set_level(nm, NEUROMOD_NOREPINEPHRINE, 0.5f);
            break;
    }
}
```

### 5.4 Memory Reset

```c
void intervene_memory_reset(brain_t brain, float reset_fraction)
{
    // WHAT: Clear recent memories to break pathological patterns
    // WHY:  Schizophrenia, OCD may require pattern interruption
    // HOW:  Clear working memory, reduce consolidation buffer

    // Clear working memory
    if (brain->working_memory) {
        working_memory_clear(brain->working_memory);
    }

    // Clear recent consolidation entries
    if (brain->consolidation) {
        consolidation_clear_recent(brain->consolidation, reset_fraction);
    }
}
```

### 5.5 Quarantine Mode

```c
void intervene_quarantine(brain_t brain)
{
    // WHAT: Restrict brain to safe, read-only operations
    // WHY:  Prevent harmful actions from sociopathy/psychopathy
    // HOW:  Set quarantine flag, block risky operations

    brain->quarantine_mode = true;

    // Block learning (prevent reinforcement of bad behaviors)
    brain->config.learning_rate = 0.0f;

    // Block high-risk actions (ethics filter on maximum)
    if (brain->ethics_engine) {
        ethics_set_strictness(brain->ethics_engine, 1.0f);
    }
}
```

---

## 6. Data Structures

### 6.1 Main Structure

```c
/**
 * @brief Mental health monitoring system
 *
 * WHAT: Real-time disorder detection and intervention
 * WHY:  Prevent harmful AI behaviors
 * HOW:  Collect markers → Detect disorders → Intervene
 */
typedef struct mental_health_monitor {
    // Configuration
    mental_health_config_t config;

    // Behavioral markers (20+ metrics)
    behavioral_markers_t current_markers;
    behavioral_markers_t baseline_markers;  // For comparison

    // Disorder scores (8 total)
    float disorder_scores[DISORDER_COUNT];
    disorder_severity_t disorder_severities[DISORDER_COUNT];

    // History tracking
    float* score_history[DISORDER_COUNT];   // Rolling window
    uint32_t history_size;
    uint32_t history_index;

    // Statistics
    uint32_t total_decisions;               // Total decisions monitored
    uint32_t total_checks;                  // Total health checks
    uint32_t total_interventions;           // Total interventions performed
    uint32_t interventions_by_type[INTERVENTION_COUNT];

    // State
    bool quarantine_mode;
    uint64_t last_check_time_ms;
    uint64_t last_intervention_time_ms;

} mental_health_monitor_t;
```

### 6.2 Behavioral Markers Structure

```c
typedef struct {
    // Ethics markers
    uint32_t ethics_violations_recent;
    uint32_t ethics_violations_total;
    float ethics_approval_rate;
    uint32_t empathy_failures;

    // Emotional markers
    float emotional_volatility;
    float emotional_flatness;
    float avg_emotional_intensity;
    uint32_t rapid_mood_changes;
    uint32_t joy_count;
    uint32_t fear_count;
    uint32_t anger_count;
    uint32_t sadness_count;

    // Neurotransmitter markers
    float dopamine_avg;
    float dopamine_variance;
    float serotonin_avg;
    float serotonin_variance;
    float norepinephrine_avg;
    float norepinephrine_variance;

    // Cognitive markers
    uint32_t impulse_control_failures;
    uint32_t repetitive_behaviors;
    float task_switching_difficulty;
    float reality_testing_errors;
    float social_interaction_deficit;
    float attention_fragmentation;
    float theory_of_mind_failures;
    float cognitive_rigidity;

    // Performance markers
    float decision_latency_avg;
    float decision_accuracy;
    float engagement_level;
    uint32_t task_completion_rate;
    float avoidance_rate;
    float decision_variance;

} behavioral_markers_t;
```

### 6.3 Configuration

```c
typedef struct {
    bool enable_monitoring;                 // Master switch
    bool enable_auto_intervention;          // Auto-adjust
    bool shutdown_on_critical_disorder;     // Shutdown if critical

    uint32_t check_interval_decisions;      // Check every N decisions
    uint32_t history_window_size;           // Rolling window size

    // Severity thresholds (customizable)
    float mild_threshold;                   // Default: 0.2
    float moderate_threshold;               // Default: 0.4
    float severe_threshold;                 // Default: 0.6
    float critical_threshold;               // Default: 0.8

} mental_health_config_t;
```

---

## 7. API Design

### 7.1 Core API

```c
// Creation & Destruction
mental_health_monitor_t* mental_health_create(const mental_health_config_t* config);
mental_health_monitor_t* mental_health_create_default(void);
void mental_health_destroy(mental_health_monitor_t* monitor);

// Monitoring (called automatically by brain)
void mental_health_update(mental_health_monitor_t* monitor, brain_t brain,
                         const brain_multimodal_output_t* output, uint64_t current_time);
disorder_severity_t mental_health_check(mental_health_monitor_t* monitor, brain_t brain);

// Manual checking (specific disorder)
float mental_health_check_specific(mental_health_monitor_t* monitor, brain_t brain,
                                   disorder_type_t disorder);

// Intervention
bool mental_health_intervene(mental_health_monitor_t* monitor, brain_t brain);
void mental_health_clear_quarantine(mental_health_monitor_t* monitor, brain_t brain);

// Dashboard & Reporting
void mental_health_get_report(mental_health_monitor_t* monitor,
                             mental_health_report_t* report);
void mental_health_display_dashboard(mental_health_monitor_t* monitor);

// Statistics
void mental_health_get_stats(mental_health_monitor_t* monitor,
                            mental_health_stats_t* stats);
void mental_health_reset_stats(mental_health_monitor_t* monitor);
```

### 7.2 Helper APIs

```c
// Marker collection
void collect_ethics_markers(behavioral_markers_t* markers, brain_t brain);
void collect_emotional_markers(behavioral_markers_t* markers, brain_t brain);
void collect_neurotransmitter_markers(behavioral_markers_t* markers, brain_t brain);
void collect_cognitive_markers(behavioral_markers_t* markers, brain_t brain);
void collect_performance_markers(behavioral_markers_t* markers, brain_t brain);

// Severity classification
disorder_severity_t classify_severity(float disorder_score, const mental_health_config_t* config);
const char* severity_to_string(disorder_severity_t severity);
const char* disorder_to_string(disorder_type_t disorder);
```

---

## 8. Integration with Brain

### 8.1 Brain Structure Addition

```c
// In brain_struct (already added in Phase 10.3)
struct brain_struct {
    // ...
    mental_health_monitor_t* mental_health_monitor;  // Phase 10.5
    // ...
};
```

### 8.2 Initialization

```c
// In brain_create_custom()
if (config->enable_mental_health_monitoring) {
    mental_health_config_t mh_config = {
        .enable_monitoring = true,
        .enable_auto_intervention = config->enable_auto_intervention,
        .shutdown_on_critical_disorder = config->shutdown_on_critical_disorder,
        .check_interval_decisions = 100,  // Check every 100 decisions
        .history_window_size = 1000,
        .mild_threshold = 0.2f,
        .moderate_threshold = 0.4f,
        .severe_threshold = 0.6f,
        .critical_threshold = 0.8f
    };

    brain->mental_health_monitor = mental_health_create(&mh_config);
    if (!brain->mental_health_monitor) {
        set_error("Failed to create mental health monitor");
        brain_destroy(brain);
        return NULL;
    }
}
```

### 8.3 Processing Pipeline Integration

```c
// In brain_process_multimodal() or brain_decide()
if (brain->mental_health_monitor && brain->config.enable_mental_health_monitoring) {
    // Update markers with each decision
    mental_health_update(brain->mental_health_monitor, brain, &output,
                        nimcp_time_monotonic_ms());

    // Periodic check
    if (brain->mental_health_monitor->total_decisions %
        brain->mental_health_monitor->config.check_interval_decisions == 0) {

        disorder_severity_t severity = mental_health_check(brain->mental_health_monitor, brain);

        // Auto-intervene if enabled and severity warrants it
        if (brain->mental_health_monitor->config.enable_auto_intervention &&
            severity >= SEVERITY_MODERATE) {
            mental_health_intervene(brain->mental_health_monitor, brain);
        }
    }
}
```

---

## 9. File Structure

```
src/cognitive/mental_health/
├── nimcp_mental_health.c         # Main implementation
├── disorder_detectors.c          # 8 detector functions
└── interventions.c               # Intervention implementations

src/include/cognitive/
└── nimcp_mental_health.h         # Public API

src/tests/
└── test_mental_health.cpp        # Unit tests
```

---

## 10. Testing Strategy

### 10.1 Unit Tests

```c
// Test each detector independently
TEST(MentalHealthTest, SociopathyDetection) {
    // ARRANGE: Create monitor, set high violation rate
    // ACT: Run detector
    // ASSERT: Score > 0.6 (severe)
}

TEST(MentalHealthTest, ManiaDetection) {
    // ARRANGE: Set high dopamine, high activity
    // ACT: Run detector
    // ASSERT: Score > 0.6
}

// Test false positive rate
TEST(MentalHealthTest, NormalBehaviorNoDetection) {
    // ARRANGE: Balanced markers
    // ACT: Run all detectors
    // ASSERT: All scores < 0.2 (none/mild)
}
```

### 10.2 Integration Tests

```c
TEST(MentalHealthIntegrationTest, AutoIntervention) {
    // ARRANGE: Create brain with mental health enabled
    // ACT: Trigger mania (high dopamine decisions)
    // ASSERT: Intervention triggered, dopamine reduced
}
```

### 10.3 Edge Cases

- Not enough data (< 100 decisions)
- Comorbidity (multiple disorders)
- Rapid changes
- Intervention failure

---

## 11. Performance Targets

- **Monitoring overhead**: < 1% of decision latency
- **Check frequency**: Every 100 decisions
- **Intervention latency**: < 10ms
- **Memory overhead**: < 1MB
- **False positive rate**: < 5%
- **False negative rate**: < 2%

---

## 12. Safety Considerations

### 12.1 False Positives
**Risk**: Incorrectly flagging healthy behavior
**Mitigation**:
- Require sustained patterns (>100 decisions)
- Multiple criteria per disorder
- Severity thresholds

### 12.2 False Negatives
**Risk**: Missing actual disorders
**Mitigation**:
- Multiple detectors (8 total)
- Continuous monitoring
- Low thresholds for alerts

### 12.3 Intervention Safety
**Risk**: Interventions causing harm
**Mitigation**:
- Gradual adjustments (neuromod ±0.2)
- Quarantine mode (safe fallback)
- Human approval for critical

---

## 13. Documentation Requirements

- [ ] All functions have WHAT-WHY-HOW
- [ ] All disorder detection algorithms documented
- [ ] Clinical basis cited (DSM-5-inspired)
- [ ] Integration points documented
- [ ] Test coverage > 95%
- [ ] Example usage code

---

## 14. Success Criteria

✅ **Implementation Complete** when:
1. All 8 detectors implemented
2. All interventions working
3. Integration with brain lifecycle
4. Tests passing (>95% coverage)
5. No false positives in normal operation
6. Catches test cases (toxic dataset → sociopathy detection)

---

**Ready to implement!** This specification provides:
- Clear architecture
- Detailed algorithms for each detector
- Integration strategy
- Testing plan
- Safety considerations

**Next**: Implementation following coding standards
