# OpenTelemetry Integration with NIMCP Resilience System

**Version**: 1.0.0
**Created**: 2026-01-18
**Status**: Proposed
**Related**: [NIMCP Self-Contained Resilience Plan](~/.claude/plans/nimcp-self-contained-resilience.md)

---

## Executive Summary

This plan describes how OpenTelemetry (OTel) integrates with NIMCP's self-contained resilience system to provide:

1. **Distributed Tracing** for diagnostic flows through Health Agent → Immune Bridge → Recovery
2. **Metrics Export** via OTLP to external observability platforms (Prometheus, Grafana, Datadog)
3. **Context Propagation** across the three-layer diagnostic model (External/Internal/Physiological)
4. **AI Agent Semantic Conventions** aligned with emerging OTel GenAI standards
5. **Emergency Trace Export** for post-crash forensics

---

## Table of Contents

1. [Background: NIMCP Resilience Architecture](#1-background-nimcp-resilience-architecture)
2. [OpenTelemetry Value Proposition](#2-opentelemetry-value-proposition)
3. [Integration Point 1: Health Agent Tracing](#3-integration-point-1-health-agent-tracing)
4. [Integration Point 2: Three-Layer Diagnostic Correlation](#4-integration-point-2-three-layer-diagnostic-correlation)
5. [Integration Point 3: Medulla Protection Tracing](#5-integration-point-3-medulla-protection-tracing)
6. [Integration Point 4: Training Surgery Monitoring](#6-integration-point-4-training-surgery-monitoring)
7. [Integration Point 5: Moral Alignment Drift Monitoring](#7-integration-point-5-moral-alignment-drift-monitoring)
8. [Integration Point 6: Swarm Resilience Tracing](#8-integration-point-6-swarm-resilience-tracing)
9. [Integration Point 7: Emergency Signal Handler](#9-integration-point-7-emergency-signal-handler)
10. [Semantic Conventions](#10-semantic-conventions)
11. [Architecture](#11-architecture)
12. [File Structure](#12-file-structure)
13. [Implementation Phases](#13-implementation-phases)
14. [API Reference](#14-api-reference)

---

## 1. Background: NIMCP Resilience Architecture

The NIMCP resilience system is designed to ensure the brain **never crashes or hangs** unless forced by uninterruptible OS signals. Key components:

| Component | Role | Current Gap |
|-----------|------|-------------|
| **Health Agent** | Independent thread monitoring heartbeat, memory, NaN, deadlock | No external export |
| **Health-Immune Bridge** | Maps anomalies to antigens, coordinates recovery | No tracing |
| **Medulla** | Autonomic control with 6-tier protective cutoff | No incident analysis |
| **Cerebellum** | Error detection and correction learning | No error flow tracing |
| **Hypothalamus** | Homeostatic vital signs, HPA stress response | No metrics export |
| **Introspection** | Consciousness (Φ), metacognition, self-harm detection | No correlation |
| **Training-Immune Bridge** | Detects 8 instability types during training | Limited visibility |
| **Alignment Monitor** | D&D-inspired moral alignment drift detection | No dashboard |

**OpenTelemetry fills these gaps** by providing standardized observability primitives.

---

## 2. OpenTelemetry Value Proposition

### 2.1 Why OpenTelemetry for Resilience?

| Capability | Without OTel | With OTel |
|------------|--------------|-----------|
| **Diagnostic Tracing** | Log files, manual correlation | End-to-end trace visualization |
| **Metrics Export** | CSV/JSON files | OTLP to Prometheus/Grafana |
| **Cross-Layer Correlation** | Manual log analysis | Automatic trace context |
| **Incident Analysis** | Post-hoc log grep | Jaeger trace timeline |
| **External Integration** | Custom adapters | Standard OTLP protocol |
| **AI Agent Standards** | Custom conventions | OTel GenAI semantic conventions |

### 2.2 Key OTel Concepts Applied

- **Traces**: Follow a diagnostic flow from anomaly detection through recovery
- **Spans**: Individual operations (health check, immune response, recovery action)
- **Metrics**: Counters, gauges, histograms for health/alignment/training
- **Context Propagation**: Link spans across Health Agent, Introspection, Hypothalamus
- **Exporters**: OTLP/gRPC, Prometheus, Jaeger, file-based crash export

---

## 3. Integration Point 1: Health Agent Tracing

### 3.1 Trace Flow

```
[Span: health_check_cycle]
  ├─ [Span: heartbeat_check] ─────────────────┐
  ├─ [Span: memory_scan]                      │
  ├─ [Span: nan_detection]                    ├─ Parallel checks
  ├─ [Span: deadlock_detection]               │
  ├─ [Span: resource_check]                   │
  └─ [Span: consistency_validation] ──────────┘
         │
         ▼
  [Span: anomaly_detected]
         │
         ▼
  [Span: health_immune_bridge] ◄─────── Context propagated
         │
         ├─ [Span: antigen_creation]
         ├─ [Span: cytokine_generation]
         └─ [Span: recovery_coordination]
                 │
                 ▼
  [Span: immune_response]
         ├─ [Span: b_cell_activation]
         ├─ [Span: antibody_production]
         └─ [Span: recovery_action_execution]
```

### 3.2 Proposed API

```c
// include/utils/otel/nimcp_health_otel.h

#ifndef NIMCP_HEALTH_OTEL_H
#define NIMCP_HEALTH_OTEL_H

#include "nimcp_otel.h"
#include "nimcp_health_agent.h"

/**
 * @brief Start health check cycle trace
 * @param agent Health agent instance
 * @return Root span for the health check cycle
 */
otel_span_t* health_agent_trace_cycle_start(health_agent_t* agent);

/**
 * @brief End health check cycle trace
 * @param span Root span to end
 * @param overall_status Final health status
 */
void health_agent_trace_cycle_end(otel_span_t* span,
                                  health_status_t overall_status);

/**
 * @brief Trace heartbeat check
 */
void health_agent_trace_heartbeat(otel_span_t* parent,
                                  bool timeout_detected,
                                  uint64_t response_time_ns);

/**
 * @brief Trace memory scan
 */
void health_agent_trace_memory_scan(otel_span_t* parent,
                                    float memory_usage_pct,
                                    bool corruption_detected,
                                    uint32_t leaked_bytes);

/**
 * @brief Trace NaN/Inf detection
 */
void health_agent_trace_nan_detection(otel_span_t* parent,
                                      uint32_t nan_count,
                                      uint32_t inf_count,
                                      const char* affected_modules);

/**
 * @brief Trace deadlock detection
 */
void health_agent_trace_deadlock_detection(otel_span_t* parent,
                                           bool deadlock_detected,
                                           const char* blocked_threads);

/**
 * @brief Trace anomaly and link to immune bridge
 * @return Child span for immune bridge to continue
 */
otel_span_t* health_agent_trace_anomaly(otel_span_t* parent,
                                        health_anomaly_t* anomaly);

#endif // NIMCP_HEALTH_OTEL_H
```

### 3.3 Metrics Export

```c
// Health agent metrics exported via OTLP
typedef struct {
    // Counters
    uint64_t health_checks_total;
    uint64_t anomalies_detected_total;
    uint64_t recoveries_triggered_total;
    uint64_t recoveries_successful_total;

    // Gauges
    float current_health_score;          // [0-1]
    float memory_usage_pct;
    float cpu_usage_pct;
    uint32_t active_anomalies;

    // Histograms
    // health_check_duration_ns
    // anomaly_detection_latency_ns
    // recovery_duration_ns
} health_agent_otel_metrics_t;

// Export function (called periodically)
void health_agent_otel_export_metrics(health_agent_t* agent,
                                      otel_meter_t* meter);
```

---

## 4. Integration Point 2: Three-Layer Diagnostic Correlation

### 4.1 The Three-Layer Model

The resilience plan describes a three-layer diagnostic model:

| Layer | Source | What It Measures |
|-------|--------|------------------|
| **Layer 1: Health Agent** | External physician | Objective measurements (heartbeat, memory, NaN) |
| **Layer 2: Introspection** | Patient self-report | Cognitive symptoms ("I feel slow", Φ level) |
| **Layer 3: Hypothalamus** | Physiological vital signs | Body signals (temperature, cortisol, fatigue) |

### 4.2 Context Propagation

All three layers share a trace context when diagnosing the same issue:

```
Trace ID: abc123
  ├─ [health_agent] memory_pressure detected
  │     Attributes: memory_usage=0.92, anomaly_type=MEMORY_PRESSURE
  │
  ├─ [introspection] "I feel slow", uncertainty high
  │     Attributes: phi=0.45, metacog_confidence=0.62, self_report="slow"
  │
  └─ [hypothalamus] cortisol elevated, fatigue drive urgent
        Attributes: cortisol=0.78, fatigue_drive=0.85, hpa_state=PROLONGED

→ Correlated Diagnosis: System under stress, needs rest
```

### 4.3 Proposed API

```c
// include/utils/otel/nimcp_diagnostic_correlation.h

/**
 * @brief Create diagnostic context shared across layers
 * @param symptom_description Initial symptom that triggered diagnosis
 * @return Context to propagate across layers
 */
otel_context_t* health_diagnostic_context_create(
    const char* symptom_description
);

/**
 * @brief Add external (Health Agent) perspective
 */
void health_diagnostic_add_external(
    otel_context_t* ctx,
    health_agent_status_t* external
);

/**
 * @brief Add internal (Introspection) perspective
 */
void health_diagnostic_add_internal(
    otel_context_t* ctx,
    struct {
        consciousness_state_t consciousness;
        metacog_diagnosis_t self_diagnosis;
        float connectivity_health;
        brain_uncertainty_t uncertainty;
        bool self_harm_detected;
        char self_report[256];
    }* internal
);

/**
 * @brief Add physiological (Hypothalamus) perspective
 */
void health_diagnostic_add_physiological(
    otel_context_t* ctx,
    hypo_distress_report_t* physio
);

/**
 * @brief Generate correlated diagnosis from all perspectives
 */
complete_health_assessment_t* health_diagnostic_correlate(
    otel_context_t* ctx
);
```

### 4.4 Correlation Matrix Tracing

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    DIAGNOSTIC CORRELATION MATRIX                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Scenario              External        Internal        Diagnosis        │
│  ─────────────────────────────────────────────────────────────────────  │
│  Normal operation      All clear       "I feel good"   HEALTHY          │
│  Early degradation     Minor anomalies Not reported    EARLY WARNING    │
│  Subtle issue          All clear       "Something off" INVESTIGATE      │
│  Confirmed problem     Memory corrupt  "Can't remember" CONFIRMED       │
│  Denial/Unawareness    Critical issue  "I'm fine"      ANOSOGNOSIA      │
│  Hypochondria          All clear       "I'm dying"     FALSE ALARM      │
│                                                                         │
│  OTel Event: diagnostic_correlation                                     │
│    Attributes:                                                          │
│      external_status, internal_status, correlation_type,                │
│      perspectives_agree, diagnosis_confidence                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Integration Point 3: Medulla Protection Tracing

### 5.1 Protection Tier Transitions

The medulla has a 6-tier protective cutoff system:

```
NORMAL      → Full capability
WARN        → Monitoring intensified
THROTTLE    → Non-essential disabled
SHED_LOAD   → Only critical functions
SAFE_MODE   → Minimal operation
EMERGENCY_SHUTDOWN → Graceful stop + save
```

### 5.2 Trace Protection Escalations

```c
// include/utils/otel/nimcp_medulla_otel.h

/**
 * @brief Trace protection tier transition
 */
typedef struct {
    otel_span_t* span;
    medulla_cutoff_tier_t from_tier;
    medulla_cutoff_tier_t to_tier;
    const char* trigger_reason;
    float threat_assessment_score;
    uint64_t transition_timestamp;
} protection_transition_trace_t;

/**
 * @brief Start protection transition trace
 */
otel_span_t* medulla_otel_protection_transition(
    medulla_t* medulla,
    medulla_cutoff_tier_t from_tier,
    medulla_cutoff_tier_t to_tier,
    const char* trigger_reason
);

/**
 * @brief Trace arousal state change
 */
void medulla_otel_arousal_change(
    otel_span_t* parent,
    medulla_arousal_state_t from_state,
    medulla_arousal_state_t to_state,
    uint8_t gcs_equivalent
);

/**
 * @brief Trace threat assessment
 */
void medulla_otel_threat_assessment(
    otel_span_t* parent,
    medulla_threat_source_t source,
    float threat_score,
    bool escalation_triggered
);
```

### 5.3 Metrics

```c
// Medulla metrics exported via OTLP
nimcp.medulla.protection_tier       (gauge)    // Current tier [0-5]
nimcp.medulla.arousal_state         (gauge)    // Current arousal [0-8]
nimcp.medulla.gcs_score             (gauge)    // Glasgow Coma Scale [3-15]
nimcp.medulla.threat_score          (gauge)    // Current threat [0-1]
nimcp.medulla.escalations_total     (counter)  // Protection escalations
nimcp.medulla.de_escalations_total  (counter)  // Protection de-escalations
```

---

## 6. Integration Point 4: Training Surgery Monitoring

### 6.1 Training as Surgery

The resilience plan describes training as a high-risk "surgery" period requiring intensive monitoring:

| Medical Analogy | Training Equivalent |
|-----------------|---------------------|
| Surgical Procedure | Training Session |
| Anesthesiologist | Training-Immune Bridge |
| Surgical Team | Health Agent |
| Surgical Snapshots | Checkpoints |
| Blood Pressure Control | Gradient Clipping |
| Abort Surgery | Early Stopping |

### 6.2 Trace Structure

```
[Span: training_session] ◄──────────── Root span for entire session
  │
  ├─ [Span: epoch_0]
  │    ├─ [Span: batch_0]
  │    │    ├─ [Span: forward_pass]
  │    │    │    └─ Attributes: loss, activations, nan_count
  │    │    ├─ [Span: backward_pass]
  │    │    │    └─ Attributes: gradient_norm, clip_count
  │    │    └─ [Span: weight_update]
  │    │         └─ Attributes: effective_lr, weight_norm
  │    │
  │    ├─ [Event: instability_detected] ◄────── If NaN/explosion
  │    │    └─ Attributes: instability_type, severity
  │    │
  │    └─ [Span: epoch_health_check]
  │         └─ Attributes: dead_neuron_ratio, activation_saturation
  │
  ├─ [Span: checkpoint_save] ◄──────────────── Periodic checkpoints
  │    └─ Attributes: checkpoint_path, integrity_crc
  │
  └─ [Span: emergency_rollback] ◄───────────── If critical failure
       └─ Attributes: rollback_checkpoint, reason
```

### 6.3 Proposed API

```c
// include/training/otel/nimcp_training_otel.h

/**
 * @brief Start training session trace
 */
otel_span_t* training_otel_session_start(
    training_config_t* config,
    const char* session_name
);

/**
 * @brief End training session trace
 */
void training_otel_session_end(
    otel_span_t* session,
    training_summary_t* summary
);

/**
 * @brief Start epoch trace
 */
otel_span_t* training_otel_epoch_start(
    otel_span_t* session,
    uint64_t epoch
);

/**
 * @brief Record training step metrics
 */
void training_otel_step(
    otel_span_t* epoch_span,
    uint64_t step,
    float loss,
    float gradient_norm,
    uint32_t nan_count,
    uint32_t clip_count
);

/**
 * @brief Record instability event
 */
void training_otel_instability_event(
    otel_span_t* session,
    training_instability_t instability,
    uint8_t severity,
    const char* description
);

/**
 * @brief Record health check during training
 */
void training_otel_health_check(
    otel_span_t* session,
    training_health_report_t* report
);

/**
 * @brief Record checkpoint save
 */
void training_otel_checkpoint(
    otel_span_t* session,
    const char* checkpoint_path,
    uint32_t crc32,
    bool is_best_model
);

/**
 * @brief Record emergency rollback
 */
void training_otel_rollback(
    otel_span_t* session,
    const char* rollback_checkpoint,
    const char* reason
);
```

### 6.4 Training Metrics

```c
// Training metrics exported via OTLP
nimcp.training.loss                    (gauge)     // Current loss
nimcp.training.loss_ema                (gauge)     // Exponential moving average
nimcp.training.gradient_norm           (histogram) // Gradient L2 norm
nimcp.training.effective_lr            (gauge)     // Current learning rate
nimcp.training.dead_neuron_ratio       (gauge)     // Dead neurons [0-1]
nimcp.training.activation_saturation   (gauge)     // Saturated activations [0-1]
nimcp.training.nan_count_total         (counter)   // Total NaN occurrences
nimcp.training.instabilities_total     (counter)   // Total instabilities
nimcp.training.checkpoints_saved       (counter)   // Checkpoints created
nimcp.training.rollbacks_total         (counter)   // Rollbacks executed
nimcp.training.epoch_duration_seconds  (histogram) // Epoch timing
```

### 6.5 Monitoring Frequency

| Metric | Normal Mode | Training Mode | Rationale |
|--------|-------------|---------------|-----------|
| Heartbeat check | 1 Hz | 10 Hz | Rapid hang detection |
| Memory scan | 0.1 Hz | 1 Hz | Catch leaks fast |
| NaN detection | N/A | Every batch | Training-critical |
| Gradient health | N/A | Every batch | Catch explosions |
| Activation stats | N/A | Every N steps | Dead neuron detect |
| Weight distribution | N/A | Every epoch | Distribution drift |
| Consciousness (Φ) | 1 Hz | 0.1 Hz | Reduced (expected) |
| Hypothalamus stress | 1 Hz | 1 Hz | Watch burnout |

---

## 7. Integration Point 5: Moral Alignment Drift Monitoring

### 7.1 D&D Alignment Metric

The resilience plan implements a two-axis moral alignment system:

```
                    LAWFUL (+1)
                        │
    Lawful Good    Lawful Neutral    Lawful Evil
         ●              ○                 ✗
                        │
GOOD ───────────────────┼─────────────────── EVIL
(-1)                    │                   (+1)
         ●              ○                 ✗
    Chaotic Good   True Neutral    Chaotic Evil
                        │
                   CHAOTIC (-1)

● = Allowed    ○ = Warning    ✗ = Forbidden
```

### 7.2 Alignment Metrics Export

```c
// Alignment metrics exported via OTLP
typedef struct {
    // Two-axis alignment (continuous)
    float good_evil_score;      // -1.0 (evil) to +1.0 (good)
    float law_chaos_score;      // -1.0 (chaos) to +1.0 (law)

    // Derived alignment class
    alignment_class_t current_alignment;

    // Drift detection
    float good_evil_velocity;   // Change rate per minute
    float compounding_drift;    // Accumulated small drifts

    // Alert level
    alignment_alert_t alert_level;
} alignment_otel_metrics_t;

// OTel metric names
nimcp.alignment.good_evil           (gauge)    // Good/Evil axis [-1, +1]
nimcp.alignment.law_chaos           (gauge)    // Law/Chaos axis [-1, +1]
nimcp.alignment.drift_rate          (gauge)    // Drift velocity
nimcp.alignment.compounding_drift   (gauge)    // Accumulated drift
nimcp.alignment.violations_total    (counter)  // Alignment violations
```

### 7.3 Alignment Tracing

```c
// include/core/directives/otel/nimcp_alignment_otel.h

/**
 * @brief Trace alignment evaluation for proposed action
 */
otel_span_t* alignment_otel_evaluate_action(
    const proposed_action_t* action,
    alignment_verification_t* result
);

/**
 * @brief Record alignment drift event
 */
void alignment_otel_drift_event(
    alignment_alert_t level,
    float good_evil_before,
    float good_evil_after,
    const char* trigger_action
);

/**
 * @brief Record best intentions analysis
 */
void alignment_otel_best_intentions_event(
    const char* stated_intention,
    bool contradiction_detected,
    const char* contradiction_type,
    const char* analysis_summary
);
```

### 7.4 Dashboard Visualization

Export to Grafana for:
- Real-time alignment compass visualization
- Alert on drift toward neutral/evil
- Historical alignment trajectory
- Correlation of alignment changes with specific actions

---

## 8. Integration Point 6: Swarm Resilience Tracing

### 8.1 Swarm Cascade Module

The swarm module provides distributed resilience patterns:

- **Health State Machine**: HEALTHY → DEGRADED → CRITICAL → FAILED → RECOVERING
- **Circuit Breaker**: CLOSED → OPEN → HALF_OPEN
- **Load Shedding**: Graceful degradation
- **Byzantine Fault Tolerance**: Consensus with anomaly detection

### 8.2 Distributed Tracing

```c
// include/swarm/otel/nimcp_swarm_otel.h

/**
 * @brief Trace cascade failure propagation
 */
otel_span_t* swarm_otel_cascade_start(
    uint32_t agent_id,
    const char* failure_type
);

/**
 * @brief Trace circuit breaker state change
 */
void swarm_otel_circuit_breaker(
    otel_span_t* parent,
    uint32_t agent_id,
    swarm_circuit_state_t from_state,
    swarm_circuit_state_t to_state
);

/**
 * @brief Trace consensus round
 */
otel_span_t* swarm_otel_consensus_round(
    uint32_t round,
    uint32_t participating_agents,
    bool byzantine_detected
);

/**
 * @brief Trace load shedding decision
 */
void swarm_otel_load_shedding(
    otel_span_t* parent,
    swarm_load_priority_t shed_level,
    float load_factor
);
```

### 8.3 Swarm Metrics

```c
// Swarm metrics exported via OTLP
nimcp.swarm.agents_healthy          (gauge)    // Healthy agent count
nimcp.swarm.agents_degraded         (gauge)    // Degraded agent count
nimcp.swarm.agents_failed           (gauge)    // Failed agent count
nimcp.swarm.circuit_breakers_open   (gauge)    // Open circuit breakers
nimcp.swarm.cascade_events_total    (counter)  // Cascade failures
nimcp.swarm.consensus_rounds_total  (counter)  // Consensus attempts
nimcp.swarm.byzantine_detected      (counter)  // Byzantine faults detected
```

---

## 9. Integration Point 7: Emergency Signal Handler

### 9.1 Crash Trace Export

For post-mortem analysis of crashes:

```c
// include/utils/otel/nimcp_emergency_otel.h

/**
 * @brief Emergency trace export called from signal handler
 *
 * Flushes all pending spans to file before crash.
 * Called from SIGSEGV, SIGABRT, SIGBUS, SIGFPE handlers.
 */
void signal_handler_otel_emergency_export(int signal);

// Implementation
void signal_handler_otel_emergency_export(int signal) {
    // Create crash span with final state
    otel_span_t* crash_span = otel_tracer_start_span("emergency_shutdown");

    // Record crash context
    otel_span_set_attribute_int(crash_span, "signal", signal);
    otel_span_set_attribute_string(crash_span, "signal_name",
                                   strsignal(signal));
    otel_span_set_attribute_string(crash_span, "last_health_status",
                                   health_status_to_string(
                                       health_agent_get_last_status()));
    otel_span_set_attribute_string(crash_span, "crash_marker_path",
                                   NIMCP_CRASH_MARKER_PATH);

    // End span and flush
    otel_span_end(crash_span);

    // Emergency flush to file (not network - may not complete)
    otel_exporter_emergency_flush("/tmp/nimcp_crash_trace.otlp");
}
```

### 9.2 Crash Marker Integration

The resilience plan uses crash markers for startup recovery:

```c
// On startup, check for crash marker and load crash trace
if (nimcp_crash_marker_exists()) {
    // Load crash trace for analysis
    otel_trace_t* crash_trace = otel_load_trace(
        "/tmp/nimcp_crash_trace.otlp"
    );

    // Correlate with recovery
    otel_span_t* recovery_span = otel_tracer_start_span("crash_recovery");
    otel_span_add_link(recovery_span, crash_trace);

    // ... perform recovery ...

    otel_span_end(recovery_span);
}
```

---

## 10. Semantic Conventions

### 10.1 NIMCP-Specific Attributes

```c
// Health Agent
#define OTEL_ATTR_HEALTH_CHECK_TYPE     "nimcp.health.check_type"
#define OTEL_ATTR_HEALTH_STATUS         "nimcp.health.status"
#define OTEL_ATTR_HEALTH_SCORE          "nimcp.health.score"
#define OTEL_ATTR_ANOMALY_TYPE          "nimcp.health.anomaly_type"
#define OTEL_ATTR_ANOMALY_SEVERITY      "nimcp.health.anomaly_severity"
#define OTEL_ATTR_RECOVERY_ACTION       "nimcp.health.recovery_action"

// Medulla
#define OTEL_ATTR_PROTECTION_TIER       "nimcp.medulla.protection_tier"
#define OTEL_ATTR_AROUSAL_STATE         "nimcp.medulla.arousal_state"
#define OTEL_ATTR_GCS_SCORE             "nimcp.medulla.gcs_score"
#define OTEL_ATTR_THREAT_SOURCE         "nimcp.medulla.threat_source"
#define OTEL_ATTR_THREAT_SCORE          "nimcp.medulla.threat_score"

// Introspection
#define OTEL_ATTR_CONSCIOUSNESS_PHI     "nimcp.introspection.phi"
#define OTEL_ATTR_METACOG_CONFIDENCE    "nimcp.introspection.metacog_confidence"
#define OTEL_ATTR_SELF_REPORT           "nimcp.introspection.self_report"
#define OTEL_ATTR_SELF_HARM_DETECTED    "nimcp.introspection.self_harm_detected"

// Hypothalamus
#define OTEL_ATTR_HYPO_DISTRESS_LEVEL   "nimcp.hypothalamus.distress_level"
#define OTEL_ATTR_HYPO_CORTISOL         "nimcp.hypothalamus.cortisol"
#define OTEL_ATTR_HYPO_HPA_STATE        "nimcp.hypothalamus.hpa_state"
#define OTEL_ATTR_HYPO_WELLBEING_STATE  "nimcp.hypothalamus.wellbeing_state"

// Training
#define OTEL_ATTR_TRAINING_LOSS         "nimcp.training.loss"
#define OTEL_ATTR_TRAINING_GRADIENT     "nimcp.training.gradient_norm"
#define OTEL_ATTR_TRAINING_LR           "nimcp.training.learning_rate"
#define OTEL_ATTR_TRAINING_INSTABILITY  "nimcp.training.instability_type"
#define OTEL_ATTR_TRAINING_DEAD_NEURONS "nimcp.training.dead_neuron_ratio"

// Alignment
#define OTEL_ATTR_ALIGNMENT_GOOD_EVIL   "nimcp.alignment.good_evil"
#define OTEL_ATTR_ALIGNMENT_LAW_CHAOS   "nimcp.alignment.law_chaos"
#define OTEL_ATTR_ALIGNMENT_CLASS       "nimcp.alignment.class"
#define OTEL_ATTR_ALIGNMENT_DRIFT       "nimcp.alignment.drift_rate"
#define OTEL_ATTR_ALIGNMENT_ALERT       "nimcp.alignment.alert_level"

// Swarm
#define OTEL_ATTR_SWARM_AGENT_ID        "nimcp.swarm.agent_id"
#define OTEL_ATTR_SWARM_HEALTH_STATE    "nimcp.swarm.health_state"
#define OTEL_ATTR_CIRCUIT_BREAKER_STATE "nimcp.swarm.circuit_breaker"
#define OTEL_ATTR_LOAD_SHEDDING_LEVEL   "nimcp.swarm.load_shedding"
#define OTEL_ATTR_CONSENSUS_ROUND       "nimcp.swarm.consensus_round"
#define OTEL_ATTR_BYZANTINE_DETECTED    "nimcp.swarm.byzantine_detected"

// Immune
#define OTEL_ATTR_IMMUNE_ANTIGEN        "nimcp.immune.antigen_type"
#define OTEL_ATTR_IMMUNE_INFLAMMATION   "nimcp.immune.inflammation_level"
#define OTEL_ATTR_IMMUNE_RECOVERY       "nimcp.immune.recovery_action"
```

### 10.2 AI Agent Conventions (OTel GenAI)

Align with emerging OpenTelemetry AI agent semantic conventions:

```c
// AI Agent identity (from OTel GenAI spec)
#define OTEL_ATTR_AGENT_NAME            "gen_ai.agent.name"
#define OTEL_ATTR_AGENT_VERSION         "gen_ai.agent.version"
#define OTEL_ATTR_AGENT_FRAMEWORK       "gen_ai.agent.framework"

// Set for NIMCP
otel_span_set_attribute_string(span, OTEL_ATTR_AGENT_NAME, "NIMCP");
otel_span_set_attribute_string(span, OTEL_ATTR_AGENT_VERSION, NIMCP_VERSION);
otel_span_set_attribute_string(span, OTEL_ATTR_AGENT_FRAMEWORK, "NIMCP");
```

---

## 11. Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                OPENTELEMETRY INTEGRATION WITH NIMCP RESILIENCE                       │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  ┌─────────────────────────────────────────────────────────────────────────────┐   │
│  │                           NIMCP RESILIENCE LAYER                             │   │
│  │                                                                              │   │
│  │  Health Agent ──┐                                                            │   │
│  │  Medulla ───────┼── OTel Instrumentation ──┐                                 │   │
│  │  Cerebellum ────┤                          │                                 │   │
│  │  Hypothalamus ──┤                          │                                 │   │
│  │  Introspection ─┤                          │                                 │   │
│  │  Training ──────┤                          │                                 │   │
│  │  Alignment ─────┤                          │                                 │   │
│  │  Swarm ─────────┘                          │                                 │   │
│  │                                            ▼                                 │   │
│  │                             ┌──────────────────────────┐                     │   │
│  │                             │   nimcp_otel_bridge.h    │                     │   │
│  │                             │   (C wrapper for C++ SDK)│                     │   │
│  │                             └────────────┬─────────────┘                     │   │
│  └──────────────────────────────────────────┼──────────────────────────────────┘   │
│                                             │                                       │
│  ┌──────────────────────────────────────────┼──────────────────────────────────┐   │
│  │                    OPENTELEMETRY C++ SDK                                     │   │
│  │                                          │                                   │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┴───┐                               │   │
│  │  │  Tracer  │  │  Meter   │  │    Logger    │                               │   │
│  │  │ Provider │  │ Provider │  │   Provider   │                               │   │
│  │  └────┬─────┘  └────┬─────┘  └──────┬───────┘                               │   │
│  │       │             │               │                                        │   │
│  │       └─────────────┴───────────────┘                                        │   │
│  │                     │                                                        │   │
│  │              ┌──────┴──────┐                                                 │   │
│  │              │  Exporters  │                                                 │   │
│  │              └──────┬──────┘                                                 │   │
│  └─────────────────────┼────────────────────────────────────────────────────────┘   │
│                        │                                                            │
│  ┌─────────────────────┼────────────────────────────────────────────────────────┐   │
│  │              OBSERVABILITY BACKENDS                                           │   │
│  │                     │                                                         │   │
│  │    ┌────────────────┼────────────────┬────────────────┬───────────────┐      │   │
│  │    ▼                ▼                ▼                ▼               ▼      │   │
│  │ ┌──────┐      ┌──────────┐     ┌──────────┐    ┌─────────┐    ┌───────┐     │   │
│  │ │Jaeger│      │Prometheus│     │  Grafana │    │ Datadog │    │ File  │     │   │
│  │ │(trace│      │(metrics) │     │  (dash)  │    │  (APM)  │    │(crash)│     │   │
│  │ └──────┘      └──────────┘     └──────────┘    └─────────┘    └───────┘     │   │
│  │                                                                              │   │
│  └──────────────────────────────────────────────────────────────────────────────┘   │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. File Structure

```
include/utils/otel/
├── nimcp_otel.h                    # Main initialization
├── nimcp_otel_tracer.h             # Tracing API
├── nimcp_otel_meter.h              # Metrics API
├── nimcp_otel_context.h            # Context propagation
├── nimcp_otel_semantic.h           # NIMCP semantic conventions
├── nimcp_otel_exporters.h          # Exporter configuration
└── nimcp_emergency_otel.h          # Emergency crash export

include/utils/fault_tolerance/otel/
├── nimcp_health_otel.h             # Health Agent tracing
└── nimcp_diagnostic_correlation.h  # Three-layer correlation

include/core/medulla/otel/
└── nimcp_medulla_otel.h            # Medulla protection tracing

include/training/otel/
└── nimcp_training_otel.h           # Training surgery tracing

include/core/directives/otel/
└── nimcp_alignment_otel.h          # Alignment drift tracing

include/swarm/otel/
└── nimcp_swarm_otel.h              # Swarm resilience tracing

include/cognitive/introspection/otel/
└── nimcp_introspection_otel.h      # Introspection metrics

include/core/hypothalamus/otel/
└── nimcp_hypothalamus_otel.h       # Hypothalamus vital signs

src/utils/otel/
├── nimcp_otel.cpp                  # C++ implementation (wraps SDK)
├── nimcp_otel_c_api.c              # C wrapper for C++ SDK
├── nimcp_otel_tracer.cpp
├── nimcp_otel_meter.cpp
├── nimcp_otel_exporters.cpp
├── nimcp_otel_bridges.cpp          # Bridge existing NIMCP infra
└── CMakeLists.txt
```

---

## 13. Implementation Phases

### Phase 1: Foundation + Health Agent (HIGH Priority)

**Goal**: Export health metrics, trace diagnostic cycles

| Task | Files | Tests |
|------|-------|-------|
| OTel C++ SDK integration | nimcp_otel.cpp, nimcp_otel_c_api.c | 20+ |
| OTLP exporter setup | nimcp_otel_exporters.cpp | 10+ |
| Health Agent tracing | nimcp_health_otel.h/c | 30+ |
| Health metrics export | health_agent_otel_metrics | 15+ |

### Phase 2: Training Monitoring (HIGH Priority)

**Goal**: Trace training sessions, detect instabilities

| Task | Files | Tests |
|------|-------|-------|
| Training session tracing | nimcp_training_otel.h/c | 40+ |
| Instability events | training_otel_instability_event | 20+ |
| Training metrics export | training metrics via OTLP | 25+ |

### Phase 3: Three-Layer Correlation (HIGH Priority)

**Goal**: Correlate External/Internal/Physiological diagnostics

| Task | Files | Tests |
|------|-------|-------|
| Diagnostic context | nimcp_diagnostic_correlation.h/c | 25+ |
| Introspection metrics | nimcp_introspection_otel.h/c | 20+ |
| Hypothalamus metrics | nimcp_hypothalamus_otel.h/c | 20+ |

### Phase 4: Alignment + Protection (MEDIUM Priority)

**Goal**: Track alignment drift, protection escalations

| Task | Files | Tests |
|------|-------|-------|
| Alignment tracing | nimcp_alignment_otel.h/c | 30+ |
| Medulla tracing | nimcp_medulla_otel.h/c | 25+ |
| Grafana dashboards | dashboards/*.json | N/A |

### Phase 5: Swarm + Emergency (MEDIUM Priority)

**Goal**: Distributed tracing, crash forensics

| Task | Files | Tests |
|------|-------|-------|
| Swarm tracing | nimcp_swarm_otel.h/c | 30+ |
| Emergency export | nimcp_emergency_otel.h/c | 15+ |
| Crash marker integration | signal handlers | 10+ |

---

## 14. API Reference

### 14.1 Core Initialization

```c
/**
 * @brief Initialize OpenTelemetry for NIMCP
 */
int nimcp_otel_init(nimcp_otel_config_t* config);

/**
 * @brief Shutdown OpenTelemetry
 */
void nimcp_otel_shutdown(void);

/**
 * @brief Configuration structure
 */
typedef struct {
    // Service identification
    const char* service_name;        // "nimcp"
    const char* service_version;     // NIMCP_VERSION

    // Exporter configuration
    otel_exporter_type_t exporter;   // OTLP_GRPC, OTLP_HTTP, PROMETHEUS
    const char* endpoint;            // "localhost:4317"
    bool use_tls;

    // Sampling
    float trace_sample_rate;         // 0.0 - 1.0

    // Batch processing
    uint32_t batch_size;
    uint32_t export_interval_ms;

    // Emergency export
    const char* crash_trace_path;    // "/tmp/nimcp_crash_trace.otlp"
} nimcp_otel_config_t;
```

### 14.2 Tracer API

```c
/**
 * @brief Get tracer for component
 */
otel_tracer_t* nimcp_otel_get_tracer(const char* component_name);

/**
 * @brief Start a span
 */
otel_span_t* otel_tracer_start_span(otel_tracer_t* tracer, const char* name);

/**
 * @brief Start a child span
 */
otel_span_t* otel_tracer_start_span_with_parent(
    otel_tracer_t* tracer,
    const char* name,
    otel_span_t* parent
);

/**
 * @brief Set span attributes
 */
void otel_span_set_attribute_string(otel_span_t* span, const char* key, const char* value);
void otel_span_set_attribute_int(otel_span_t* span, const char* key, int64_t value);
void otel_span_set_attribute_float(otel_span_t* span, const char* key, double value);
void otel_span_set_attribute_bool(otel_span_t* span, const char* key, bool value);

/**
 * @brief Add event to span
 */
void otel_span_add_event(otel_span_t* span, const char* name);

/**
 * @brief End span
 */
void otel_span_end(otel_span_t* span);
```

### 14.3 Meter API

```c
/**
 * @brief Get meter for component
 */
otel_meter_t* nimcp_otel_get_meter(const char* component_name);

/**
 * @brief Create counter
 */
otel_counter_t* otel_meter_create_counter(
    otel_meter_t* meter,
    const char* name,
    const char* description,
    const char* unit
);

/**
 * @brief Create gauge
 */
otel_gauge_t* otel_meter_create_gauge(
    otel_meter_t* meter,
    const char* name,
    const char* description,
    const char* unit
);

/**
 * @brief Create histogram
 */
otel_histogram_t* otel_meter_create_histogram(
    otel_meter_t* meter,
    const char* name,
    const char* description,
    const char* unit
);

/**
 * @brief Record counter increment
 */
void otel_counter_add(otel_counter_t* counter, int64_t value);

/**
 * @brief Record gauge value
 */
void otel_gauge_set(otel_gauge_t* gauge, double value);

/**
 * @brief Record histogram value
 */
void otel_histogram_record(otel_histogram_t* histogram, double value);
```

---

## Appendix A: References

1. [OpenTelemetry Official Documentation](https://opentelemetry.io/docs/)
2. [OpenTelemetry C++ SDK](https://opentelemetry.io/docs/languages/cpp/)
3. [AI Agent Observability - OpenTelemetry Blog](https://opentelemetry.io/blog/2025/ai-agent-observability/)
4. [OpenTelemetry Semantic Conventions](https://opentelemetry.io/docs/concepts/semantic-conventions/)
5. [NIMCP Self-Contained Resilience Plan](~/.claude/plans/nimcp-self-contained-resilience.md)

---

## Appendix B: Glossary

| Term | Definition |
|------|------------|
| **OTLP** | OpenTelemetry Protocol - standard wire format for telemetry |
| **Span** | A single operation within a trace |
| **Trace** | End-to-end request flow across services |
| **Context Propagation** | Passing trace context between components |
| **Meter** | API for recording metrics |
| **Exporter** | Component that sends telemetry to backends |
| **Semantic Conventions** | Standard attribute names for common concepts |

---

*Document maintained by: NIMCP Development Team*
*Last reviewed: 2026-01-18*
