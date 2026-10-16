# Brain Regions Implementation Plan
## Complete Neuroanatomical Architecture

**Version**: 3.0
**Status**: Planning
**Dependencies**: Memory Pool + COW (Phase 1 complete, Phase 2 in progress)

---

## Executive Summary

This plan implements **eleven specialized brain systems** as **orchestration layers** that integrate existing NIMCP systems rather than duplicating functionality. The architecture follows the established pattern from Broca's Region implementation.

**Phase 10-11 enhancements** add Einstein-inspired mathematical/scientific reasoning (Parietal Lobe) and a comprehensive Software Engineering Cortex for turbocharged programming capabilities.

### Phase 1-3: Core Regulatory Systems (Original Plan)

| Region | Existing Coverage | New Code Required | Primary Role |
|--------|------------------|-------------------|--------------|
| Medulla Oblongata | ~65% | ~35% | Autonomic control, homeostasis |
| Cerebellum | ~40% | ~60% | Motor coordination, timing, learning |
| Prefrontal Cortex | ~70% | ~30% | Executive orchestration, planning |

### Phase 4-9: Extended Brain Architecture (New)

| Region | Existing Coverage | New Code Required | Primary Role |
|--------|------------------|-------------------|--------------|
| Hippocampus | ~35% | ~65% | Spatial cognition, episodic memory |
| Basal Ganglia | ~50% | ~50% | Action selection, habits, reward |
| Wernicke's Area | ~40% | ~60% | Language comprehension |
| Amygdala | ~55% | ~45% | Fear, threat, emotional memory |
| Posterior Cingulate | ~0% | ~100% | Default mode, self-reference |
| Insula | ~40% | ~60% | Interoception, emotional awareness |

### Phase 10-11: Cognitive Enhancements ★ ENHANCED

| Region | Existing Coverage | New Code Required | Primary Role |
|--------|------------------|-------------------|--------------|
| Parietal Lobe | ~25% | ~75% | Mathematical/scientific reasoning (Einstein-inspired) |
| Software Eng Cortex | ~0% | ~100% | Programming, debugging, architecture reasoning |

**Implementation Order**: Medulla → Cerebellum → PFC → Hippocampus → Basal Ganglia → Wernicke's → Amygdala → PCC → Insula → Parietal → Software Eng

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         NIMCP Complete Brain Architecture                         │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  ┌─────────────────────────────────────────────────────────────────────────────┐ │
│  │                    CORTICAL SYSTEMS (Higher Cognition)                       │ │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  │ │
│  │  │ PREFRONTAL CTX  │  │ POST. CINGULATE │  │      LANGUAGE NETWORK       │  │ │
│  │  │ ┌────┐ ┌────┐   │  │ Default Mode    │  │ ┌─────────┐  ┌───────────┐  │  │ │
│  │  │ │DLPFC│ │ACC │   │  │ Self-Reference  │  │ │ BROCA'S │←→│WERNICKE'S │  │  │ │
│  │  │ ├────┤ ├────┤   │  │ Mind-Wandering  │  │ │Production│  │Comprehend │  │  │ │
│  │  │ │VMPFC│ │OFC │   │  │ Memory Retrieval│  │ └─────────┘  └───────────┘  │  │ │
│  │  │ └────┘ └────┘   │  └─────────────────┘  └─────────────────────────────┘  │ │
│  └─────────┬──────────────────────┬──────────────────────┬─────────────────────┘ │
│            │                      │                      │                        │
│  ┌─────────┴──────────────────────┴──────────────────────┴─────────────────────┐ │
│  │                    LIMBIC SYSTEMS (Emotion + Memory)                         │ │
│  │  ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐  │ │
│  │  │     HIPPOCAMPUS     │  │      AMYGDALA       │  │       INSULA        │  │ │
│  │  │ ┌─────┐ ┌─────────┐ │  │ ┌───────┐ ┌──────┐ │  │ ┌────────┐ ┌──────┐ │  │ │
│  │  │ │Place│ │Grid     │ │  │ │Fear   │ │Threat│ │  │ │Intero- │ │Homeo-│ │  │ │
│  │  │ │Cells│ │Cells    │ │  │ │Cond.  │ │Detect│ │  │ │ception │ │stasis│ │  │ │
│  │  │ ├─────┤ ├─────────┤ │  │ ├───────┤ ├──────┤ │  │ ├────────┤ ├──────┤ │  │ │
│  │  │ │CA3  │ │Episodic │ │  │ │Extinc-│ │Emot. │ │  │ │Salience│ │Drives│ │  │ │
│  │  │ │DG   │ │Binding  │ │  │ │tion   │ │Memory│ │  │ │Network │ │      │ │  │ │
│  │  │ └─────┘ └─────────┘ │  │ └───────┘ └──────┘ │  │ └────────┘ └──────┘ │  │ │
│  │  └─────────────────────┘  └─────────────────────┘  └─────────────────────┘  │ │
│  └─────────┬──────────────────────┬──────────────────────┬─────────────────────┘ │
│            │                      │                      │                        │
│  ┌─────────┴──────────────────────┴──────────────────────┴─────────────────────┐ │
│  │                    SUBCORTICAL SYSTEMS (Action + Timing)                     │ │
│  │  ┌─────────────────────────────────┐  ┌─────────────────────────────────┐   │ │
│  │  │         BASAL GANGLIA           │  │          CEREBELLUM             │   │ │
│  │  │ ┌────────┐ ┌────────┐ ┌──────┐  │  │ ┌────────┐ ┌────────┐ ┌──────┐ │   │ │
│  │  │ │Direct  │ │Indirect│ │Habit │  │  │ │Forward │ │Inverse │ │Timing│ │   │ │
│  │  │ │D1 (Go) │ │D2(NoGo)│ │Form. │  │  │ │Models  │ │Models  │ │Circ. │ │   │ │
│  │  │ └────────┘ └────────┘ └──────┘  │  │ └────────┘ └────────┘ └──────┘ │   │ │
│  │  └─────────────────────────────────┘  └─────────────────────────────────┘   │ │
│  └─────────────────────────────────────────────────────────────────────────────┘ │
│                                        ↕                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────┐ │
│  │                    BRAINSTEM (Autonomic Regulation)                          │ │
│  │  ┌───────────────────────────────────────────────────────────────────────┐  │ │
│  │  │                      MEDULLA OBLONGATA                                 │  │ │
│  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │  │ │
│  │  │  │ Arousal  │ │  Vital   │ │Emergency │ │Circadian │ │Brainstem │    │  │ │
│  │  │  │ Control  │ │  Signs   │ │ Response │ │Modulation│ │ Coupling │    │  │ │
│  │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘    │  │ │
│  │  └───────────────────────────────────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────────────────────────────┘ │
│                                        ↕                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────┐ │
│  │                    MEMORY POOL + COW (Shared Infrastructure)                 │ │
│  └─────────────────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Medulla Oblongata Implementation

### 1.1 Overview

The medulla oblongata serves as the **autonomic regulator** - managing vital signs, arousal states, and emergency responses. It provides the foundation that higher brain regions depend on.

### 1.2 Existing Systems to ORCHESTRATE (NOT Reimplement)

| System | File | Functions to Call |
|--------|------|-------------------|
| Health Monitor | `nimcp_health_monitor.h` | `nimcp_health_monitor_record_vital()`, `nimcp_health_check()` |
| Recovery | `nimcp_recovery.h` | `nimcp_recovery_trigger()`, `nimcp_recovery_get_tier()` |
| Sleep-Wake | `nimcp_sleep_wake.h` | `nimcp_sleep_wake_transition()`, `nimcp_sleep_wake_get_state()` |
| Neuromodulators | `nimcp_neuromodulators.h` | `nimcp_neuromod_release()`, `nimcp_neuromod_get_level()` |
| Runtime Adaptation | `nimcp_runtime_adaptation.h` | `nimcp_adaptation_apply()` |
| Fault Attention | `nimcp_fault_attention.h` | `nimcp_fault_attention_prioritize()` |

### 1.3 New Modules to Implement

#### 1.3.1 Unified Arousal State Manager
**File**: `include/core/brain/regions/medulla/nimcp_arousal_state.h`

```c
/**
 * WHAT: Unified arousal state management integrating all neuromodulator systems.
 * WHY:  Biological medulla maintains coherent arousal via brainstem nuclei.
 * HOW:  State machine with hysteresis, integrates existing neuromodulator APIs.
 */

typedef enum {
    NIMCP_AROUSAL_DEEP_SLEEP    = 0,  /* Minimal processing, memory consolidation */
    NIMCP_AROUSAL_LIGHT_SLEEP   = 1,  /* Some responsiveness */
    NIMCP_AROUSAL_DROWSY        = 2,  /* Transition state */
    NIMCP_AROUSAL_RELAXED       = 3,  /* Low vigilance baseline */
    NIMCP_AROUSAL_ALERT         = 4,  /* Normal operation */
    NIMCP_AROUSAL_FOCUSED       = 5,  /* Enhanced attention */
    NIMCP_AROUSAL_STRESSED      = 6,  /* Fight-or-flight activation */
    NIMCP_AROUSAL_EMERGENCY     = 7   /* Maximum mobilization */
} nimcp_arousal_level_t;

typedef struct {
    nimcp_arousal_level_t current_level;
    nimcp_arousal_level_t target_level;
    float                 transition_progress;  /* 0.0 - 1.0 */
    float                 stability;            /* Hysteresis factor */

    /* Mathematical: Leaky integrator for smooth transitions */
    float                 leaky_state;          /* τ·dx/dt = -x + input */
    float                 time_constant_ms;

    /* Integration with existing systems */
    nimcp_neuromod_state_t* neuromod_ref;
    nimcp_sleep_wake_t*     sleep_wake_ref;
} nimcp_arousal_state_t;

/* Core API */
nimcp_status_t nimcp_arousal_init(nimcp_arousal_state_t* state,
                                   nimcp_memory_pool_t* pool);
nimcp_status_t nimcp_arousal_update(nimcp_arousal_state_t* state,
                                     float delta_ms);
nimcp_status_t nimcp_arousal_request_transition(nimcp_arousal_state_t* state,
                                                 nimcp_arousal_level_t target,
                                                 float urgency);  /* 0.0-1.0 */
float nimcp_arousal_get_multiplier(const nimcp_arousal_state_t* state);
```

**Mathematical Enhancement - Arousal Dynamics**:
```
Leaky Integrator Model:
  τ · da/dt = -a + Σ(wᵢ · inputᵢ)

Where:
  a = arousal level (continuous 0-7)
  τ = time constant (adaptive based on stability)
  inputs = {threat_level, task_demand, circadian_phase, fatigue}

Hysteresis via Schmitt Trigger:
  transition_up   requires: input > threshold_high
  transition_down requires: input < threshold_low
  (prevents oscillation at boundaries)
```

#### 1.3.2 Protective Cutoff System
**File**: `include/core/brain/regions/medulla/nimcp_protective_cutoff.h`

```c
/**
 * WHAT: Emergency protective shutdown system with graduated responses.
 * WHY:  Biological medulla protects via vagal tone, circuit breakers.
 * HOW:  Multi-tier protection integrating health_monitor thresholds.
 */

typedef enum {
    NIMCP_PROTECT_NONE       = 0,  /* Normal operation */
    NIMCP_PROTECT_WARN       = 1,  /* Log warning, continue */
    NIMCP_PROTECT_THROTTLE   = 2,  /* Reduce processing rate */
    NIMCP_PROTECT_SHED_LOAD  = 3,  /* Drop low-priority tasks */
    NIMCP_PROTECT_SAFE_MODE  = 4,  /* Minimal functionality only */
    NIMCP_PROTECT_SHUTDOWN   = 5   /* Graceful shutdown */
} nimcp_protection_level_t;

typedef struct {
    /* Vital sign thresholds (integrates with health_monitor) */
    float memory_critical_pct;     /* Default: 95% */
    float cpu_critical_pct;        /* Default: 98% */
    float latency_critical_ms;     /* Default: 100ms */
    float error_rate_critical;     /* Default: 0.1 (10%) */

    /* Response configuration */
    uint32_t throttle_factor;      /* Processing reduction (2 = half speed) */
    uint32_t shed_priority_below;  /* Drop tasks below this priority */

    /* COW integration for state preservation */
    nimcp_cow_handle_t state_snapshot;
} nimcp_protective_config_t;

typedef struct {
    nimcp_protection_level_t current_level;
    nimcp_protective_config_t config;

    /* Circuit breaker pattern */
    uint32_t consecutive_failures;
    uint32_t failure_threshold;
    uint64_t cooldown_until_ns;

    /* Memory pool for emergency allocations */
    nimcp_memory_pool_t* emergency_pool;
} nimcp_protective_cutoff_t;

nimcp_status_t nimcp_protective_init(nimcp_protective_cutoff_t* cutoff,
                                      nimcp_memory_pool_t* pool);
nimcp_protection_level_t nimcp_protective_evaluate(
    nimcp_protective_cutoff_t* cutoff,
    const nimcp_health_vitals_t* vitals);
nimcp_status_t nimcp_protective_execute(nimcp_protective_cutoff_t* cutoff,
                                         nimcp_protection_level_t level);
```

#### 1.3.3 Brainstem-Cortex Coupling
**File**: `include/core/brain/regions/medulla/nimcp_brainstem_coupling.h`

```c
/**
 * WHAT: Bidirectional communication channel between medulla and higher regions.
 * WHY:  Enables bottom-up arousal modulation and top-down regulatory control.
 * HOW:  Signal bus with priority routing and Shannon entropy tracking.
 */

typedef struct {
    /* Bottom-up signals (medulla → cortex) */
    float arousal_signal;           /* Current arousal multiplier */
    float urgency_signal;           /* Emergency priority boost */
    float fatigue_signal;           /* Resource depletion indicator */

    /* Top-down signals (cortex → medulla) */
    float attention_demand;         /* Requested arousal level */
    float predicted_load;           /* Anticipated resource needs */
    bool  inhibit_sleep;            /* Prevent sleep transitions */

    /* Shannon entropy for signal quality */
    float bottom_up_entropy;        /* Bits of information in arousal */
    float top_down_entropy;         /* Bits of information in demands */

    /* Memory pool for signal buffers */
    nimcp_buffer_pool_t* signal_buffer;
} nimcp_brainstem_coupling_t;

nimcp_status_t nimcp_brainstem_coupling_init(
    nimcp_brainstem_coupling_t* coupling,
    nimcp_buffer_pool_t* pool);
nimcp_status_t nimcp_brainstem_send_bottom_up(
    nimcp_brainstem_coupling_t* coupling,
    const nimcp_arousal_state_t* arousal);
nimcp_status_t nimcp_brainstem_receive_top_down(
    nimcp_brainstem_coupling_t* coupling,
    float* attention_demand,
    float* predicted_load);
```

#### 1.3.4 Circadian Modulation
**File**: `include/core/brain/regions/medulla/nimcp_circadian.h`

```c
/**
 * WHAT: Circadian rhythm modulation of system parameters.
 * WHY:  Biological SCN (suprachiasmatic nucleus) optimizes by time of day.
 * HOW:  Sinusoidal modulation with phase adjustment capability.
 */

typedef struct {
    /* Phase parameters */
    float phase_hours;              /* Current circadian phase (0-24) */
    float period_hours;             /* Cycle length (default: 24.0) */
    float phase_shift;              /* Adjustment for schedule (hours) */

    /* Modulation amplitudes */
    float arousal_amplitude;        /* Peak-to-trough arousal swing */
    float learning_amplitude;       /* Memory consolidation timing */
    float attention_amplitude;      /* Focus capability variation */

    /* Integration timestamp */
    uint64_t last_update_ns;
} nimcp_circadian_t;

/**
 * Mathematical Model:
 *   modulation(t) = baseline + amplitude * cos(2π(t - phase_shift)/period)
 *
 * Arousal curve peaks at ~10:00 and ~16:00 (dual-peak model)
 * Learning consolidation peaks during slow-wave sleep (~02:00-04:00)
 */

nimcp_status_t nimcp_circadian_init(nimcp_circadian_t* circadian);
nimcp_status_t nimcp_circadian_update(nimcp_circadian_t* circadian,
                                       uint64_t current_time_ns);
float nimcp_circadian_get_arousal_mod(const nimcp_circadian_t* circadian);
float nimcp_circadian_get_learning_mod(const nimcp_circadian_t* circadian);
```

### 1.4 Medulla Main Module
**File**: `include/core/brain/regions/medulla/nimcp_medulla.h`

```c
/**
 * WHAT: Medulla oblongata brain region - autonomic control center.
 * WHY:  Provides foundational regulation for all higher cognitive functions.
 * HOW:  Orchestrates existing health/recovery/neuromod systems with new
 *       arousal, protection, and circadian modules.
 */

#ifndef NIMCP_MEDULLA_H
#define NIMCP_MEDULLA_H

#include "nimcp_arousal_state.h"
#include "nimcp_protective_cutoff.h"
#include "nimcp_brainstem_coupling.h"
#include "nimcp_circadian.h"
#include "utils/fault_tolerance/nimcp_health_monitor.h"
#include "utils/fault_tolerance/nimcp_recovery.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_buffer_pool.h"

typedef struct {
    /* New medulla-specific modules */
    nimcp_arousal_state_t       arousal;
    nimcp_protective_cutoff_t   protection;
    nimcp_brainstem_coupling_t  coupling;
    nimcp_circadian_t           circadian;

    /* References to existing orchestrated systems */
    nimcp_health_monitor_t*     health_monitor;    /* ORCHESTRATED */
    nimcp_recovery_t*           recovery;          /* ORCHESTRATED */
    nimcp_sleep_wake_t*         sleep_wake;        /* ORCHESTRATED */
    nimcp_neuromod_state_t*     neuromodulators;   /* ORCHESTRATED */

    /* Memory infrastructure */
    nimcp_buffer_pool_t*        buffer_pool;       /* Shared pool */
    nimcp_memory_pool_t         local_pool;        /* Region-specific */

    /* Statistics */
    uint64_t update_count;
    uint64_t protection_triggers;
    float    avg_arousal_level;
} nimcp_medulla_t;

/* Lifecycle */
nimcp_status_t nimcp_medulla_init(
    nimcp_medulla_t* medulla,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_health_monitor_t* health,
    nimcp_recovery_t* recovery,
    nimcp_sleep_wake_t* sleep_wake,
    nimcp_neuromod_state_t* neuromod);

nimcp_status_t nimcp_medulla_destroy(nimcp_medulla_t* medulla);

/* Core update cycle */
nimcp_status_t nimcp_medulla_update(nimcp_medulla_t* medulla, float delta_ms);

/* Query interface */
nimcp_arousal_level_t nimcp_medulla_get_arousal(const nimcp_medulla_t* medulla);
float nimcp_medulla_get_arousal_multiplier(const nimcp_medulla_t* medulla);
nimcp_protection_level_t nimcp_medulla_get_protection_level(
    const nimcp_medulla_t* medulla);

/* Control interface (from higher regions) */
nimcp_status_t nimcp_medulla_request_arousal(
    nimcp_medulla_t* medulla,
    nimcp_arousal_level_t target,
    float urgency);
nimcp_status_t nimcp_medulla_signal_task_demand(
    nimcp_medulla_t* medulla,
    float demand_level);

#endif /* NIMCP_MEDULLA_H */
```

### 1.5 Medulla Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/medulla/test_arousal_state.cpp` | Arousal transitions, hysteresis |
| Unit | `test/unit/core/brain/regions/medulla/test_protective_cutoff.cpp` | Protection levels, circuit breaker |
| Unit | `test/unit/core/brain/regions/medulla/test_circadian.cpp` | Phase calculations, modulation |
| Integration | `test/integration/core/brain/regions/medulla/test_medulla_integration.cpp` | Full system orchestration |
| Regression | `test/regression/core/brain/regions/medulla/test_medulla_regression.cpp` | API stability, performance |

---

## Phase 2: Cerebellum Implementation

### 2.1 Overview

The cerebellum serves as the **motor coordinator and timing system** - managing prediction, error correction, and procedural learning. It provides the precision timing that cognitive operations require.

### 2.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Speech Motor | `nimcp_speech_motor.h` | Motor planning primitives |
| Predictive | `nimcp_predictive.c` | `nimcp_predictive_generate()`, error tracking |
| Sequence Detector | `nimcp_sequence_detector.h` | Pattern recognition |
| Oscillation Detector | `nimcp_oscillation_detector.h` | Timing signals |
| Eligibility Trace | `nimcp_eligibility_trace.h` | Credit assignment |
| STDP | `nimcp_stdp.h` | Synaptic plasticity |

### 2.3 New Modules to Implement

#### 2.3.1 Forward Model System
**File**: `include/core/brain/regions/cerebellum/nimcp_forward_model.h`

```c
/**
 * WHAT: Predictive forward models that anticipate outcomes of actions.
 * WHY:  Biological cerebellum uses climbing fiber error signals for learning.
 * HOW:  Neural network approximator with prediction error backpropagation.
 */

typedef struct {
    /* Model dimensions */
    uint32_t state_dim;             /* Current state vector size */
    uint32_t action_dim;            /* Action/motor command size */
    uint32_t prediction_dim;        /* Predicted outcome size */

    /* Model parameters (using COW for efficient updates) */
    nimcp_cow_handle_t weights;     /* Model weights */
    nimcp_cow_handle_t biases;      /* Model biases */

    /* Prediction state */
    float* current_prediction;
    float* prediction_error;
    float  error_magnitude;

    /* Learning parameters */
    float learning_rate;
    float momentum;
    float error_threshold;          /* Below this = good prediction */

    /* Memory pool */
    nimcp_memory_pool_t* pool;
} nimcp_forward_model_t;

/**
 * Mathematical Model:
 *   predicted_state = f(current_state, action)
 *   prediction_error = actual_state - predicted_state
 *
 * Learning rule (gradient descent):
 *   Δw = -η * ∂E/∂w where E = ||prediction_error||²
 *
 * With eligibility trace integration:
 *   Δw = -η * e(t) * prediction_error
 *   where e(t) = trace from STDP system
 */

nimcp_status_t nimcp_forward_model_init(
    nimcp_forward_model_t* model,
    uint32_t state_dim,
    uint32_t action_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_forward_model_predict(
    nimcp_forward_model_t* model,
    const float* current_state,
    const float* action,
    float* predicted_state);

nimcp_status_t nimcp_forward_model_update(
    nimcp_forward_model_t* model,
    const float* actual_state);

float nimcp_forward_model_get_confidence(const nimcp_forward_model_t* model);
```

#### 2.3.2 Inverse Model System
**File**: `include/core/brain/regions/cerebellum/nimcp_inverse_model.h`

```c
/**
 * WHAT: Inverse models that compute required actions for desired outcomes.
 * WHY:  Enables motor control: "what action achieves this goal?"
 * HOW:  Learned inverse mapping with forward model validation.
 */

typedef struct {
    /* Model dimensions */
    uint32_t current_state_dim;
    uint32_t desired_state_dim;
    uint32_t action_dim;

    /* Model parameters (COW for checkpointing) */
    nimcp_cow_handle_t weights;

    /* Coupled forward model for validation */
    nimcp_forward_model_t* forward_model;

    /* Action generation */
    float* computed_action;
    float  action_confidence;

    /* Learning */
    float learning_rate;
    uint32_t validation_failures;   /* Forward model disagreements */

    nimcp_memory_pool_t* pool;
} nimcp_inverse_model_t;

/**
 * Mathematical Model:
 *   action = g(current_state, desired_state)
 *
 * Validation via forward model:
 *   predicted = forward(current_state, action)
 *   validation_error = ||predicted - desired_state||
 *
 * Learning when validation_error > threshold:
 *   Δw = -η * ∂validation_error/∂w
 */

nimcp_status_t nimcp_inverse_model_init(
    nimcp_inverse_model_t* model,
    uint32_t current_dim,
    uint32_t desired_dim,
    uint32_t action_dim,
    nimcp_forward_model_t* forward_model,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_inverse_model_compute_action(
    nimcp_inverse_model_t* model,
    const float* current_state,
    const float* desired_state,
    float* action);
```

#### 2.3.3 Timing Circuit
**File**: `include/core/brain/regions/cerebellum/nimcp_timing_circuit.h`

```c
/**
 * WHAT: Precision timing system for coordination and synchronization.
 * WHY:  Cerebellum provides ~10ms precision timing for motor coordination.
 * HOW:  Bank of oscillators with phase-locking to external rhythms.
 */

typedef struct {
    float frequency_hz;
    float phase;                    /* 0 to 2π */
    float amplitude;
    float coupling_strength;        /* To external oscillations */
} nimcp_timing_oscillator_t;

typedef struct {
    /* Oscillator bank (multiple frequencies) */
    nimcp_timing_oscillator_t* oscillators;
    uint32_t oscillator_count;

    /* Phase-locked loop for external sync */
    float pll_reference_phase;
    float pll_error_integral;
    float pll_bandwidth_hz;

    /* Interval timing (stopwatch function) */
    uint64_t interval_start_ns;
    uint64_t interval_end_ns;
    float interval_estimate_ms;
    float interval_error_ms;

    /* Integration with oscillation detector */
    nimcp_oscillation_detector_t* external_oscillations;

    nimcp_memory_pool_t* pool;
} nimcp_timing_circuit_t;

/**
 * Mathematical Model:
 *   Phase evolution: dφ/dt = ω + K·sin(φ_ref - φ)
 *
 * Where K = coupling strength, ω = natural frequency
 *
 * Interval timing via state-dependent model:
 *   P(t) = (t/τ)^n · exp(1 - t/τ) · (n/τ)
 *
 * Weber's law: σ_t / t = constant (scalar timing)
 */

nimcp_status_t nimcp_timing_init(
    nimcp_timing_circuit_t* timing,
    uint32_t oscillator_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_timing_update(
    nimcp_timing_circuit_t* timing,
    float delta_ms);

nimcp_status_t nimcp_timing_start_interval(nimcp_timing_circuit_t* timing);
nimcp_status_t nimcp_timing_stop_interval(nimcp_timing_circuit_t* timing);
float nimcp_timing_get_phase(
    const nimcp_timing_circuit_t* timing,
    float target_frequency_hz);
```

#### 2.3.4 Granule Cell Layer
**File**: `include/core/brain/regions/cerebellum/nimcp_granule_cell.h`

```c
/**
 * WHAT: Sparse distributed encoding for pattern separation.
 * WHY:  Biological cerebellum uses granule cells for expansion recoding.
 * HOW:  Random sparse projection with winner-take-all competition.
 */

typedef struct {
    /* Dimensions */
    uint32_t input_dim;             /* Mossy fiber inputs */
    uint32_t granule_dim;           /* Granule cells (>> input_dim) */
    uint32_t active_fraction;       /* Sparsity (typically 2-5%) */

    /* Projection matrix (fixed random, COW for memory efficiency) */
    nimcp_cow_handle_t projection_weights;

    /* Current activation */
    float* activations;
    uint32_t* active_indices;
    uint32_t active_count;

    /* Inhibitory normalization */
    float inhibition_strength;

    nimcp_memory_pool_t* pool;
} nimcp_granule_cell_layer_t;

/**
 * Mathematical Model:
 *   Expansion: y = ReLU(W·x) where W is random sparse
 *
 * Winner-take-all (top k%):
 *   active_i = 1 if y_i in top k% else 0
 *
 * Properties:
 *   - Pattern separation: similar inputs → dissimilar outputs
 *   - Sparsity: ~2-5% active (matches biology)
 *   - Dimensionality expansion: 100x typical
 */

nimcp_status_t nimcp_granule_init(
    nimcp_granule_cell_layer_t* layer,
    uint32_t input_dim,
    uint32_t granule_dim,
    float sparsity,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_granule_encode(
    nimcp_granule_cell_layer_t* layer,
    const float* input,
    float* sparse_output);
```

### 2.4 Cerebellum Main Module
**File**: `include/core/brain/regions/cerebellum/nimcp_cerebellum.h`

```c
/**
 * WHAT: Cerebellum brain region - motor coordination and timing center.
 * WHY:  Provides predictive control, error correction, procedural learning.
 * HOW:  Orchestrates existing prediction/timing systems with new forward/
 *       inverse models and granule cell encoding.
 */

#ifndef NIMCP_CEREBELLUM_H
#define NIMCP_CEREBELLUM_H

#include "nimcp_forward_model.h"
#include "nimcp_inverse_model.h"
#include "nimcp_timing_circuit.h"
#include "nimcp_granule_cell.h"
#include "cognitive/predictive/nimcp_predictive.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "middleware/patterns/nimcp_oscillation_detector.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "utils/memory/nimcp_buffer_pool.h"

/* Cerebellar zone types (functional regions) */
typedef enum {
    NIMCP_CEREBELLUM_VESTIBULOCEREBELLUM = 0,  /* Balance, eye movement */
    NIMCP_CEREBELLUM_SPINOCEREBELLUM     = 1,  /* Body movement, posture */
    NIMCP_CEREBELLUM_CEREBROCEREBELLUM   = 2   /* Cognitive, planning */
} nimcp_cerebellum_zone_t;

typedef struct {
    /* New cerebellum-specific modules */
    nimcp_forward_model_t       forward_models[3];  /* Per zone */
    nimcp_inverse_model_t       inverse_models[3];
    nimcp_timing_circuit_t      timing;
    nimcp_granule_cell_layer_t  granule_layer;

    /* References to orchestrated systems */
    nimcp_predictive_t*         predictive;         /* ORCHESTRATED */
    nimcp_sequence_detector_t*  sequence_detector;  /* ORCHESTRATED */
    nimcp_oscillation_detector_t* oscillations;     /* ORCHESTRATED */
    nimcp_eligibility_trace_t*  eligibility;        /* ORCHESTRATED */

    /* Connection to medulla (arousal modulation) */
    nimcp_medulla_t*            medulla;

    /* Memory infrastructure */
    nimcp_buffer_pool_t*        buffer_pool;
    nimcp_memory_pool_t         local_pool;

    /* Statistics */
    float avg_prediction_error;
    float avg_timing_precision_ms;
    uint64_t procedural_patterns_learned;
} nimcp_cerebellum_t;

/* Lifecycle */
nimcp_status_t nimcp_cerebellum_init(
    nimcp_cerebellum_t* cerebellum,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_predictive_t* predictive,
    nimcp_sequence_detector_t* sequences,
    nimcp_oscillation_detector_t* oscillations,
    nimcp_eligibility_trace_t* eligibility,
    nimcp_medulla_t* medulla);

nimcp_status_t nimcp_cerebellum_destroy(nimcp_cerebellum_t* cerebellum);

/* Core operations */
nimcp_status_t nimcp_cerebellum_predict(
    nimcp_cerebellum_t* cerebellum,
    nimcp_cerebellum_zone_t zone,
    const float* state,
    const float* action,
    float* predicted_outcome);

nimcp_status_t nimcp_cerebellum_compute_action(
    nimcp_cerebellum_t* cerebellum,
    nimcp_cerebellum_zone_t zone,
    const float* current_state,
    const float* desired_state,
    float* action);

nimcp_status_t nimcp_cerebellum_update_from_error(
    nimcp_cerebellum_t* cerebellum,
    nimcp_cerebellum_zone_t zone,
    const float* prediction_error);

/* Timing operations */
float nimcp_cerebellum_get_phase(
    nimcp_cerebellum_t* cerebellum,
    float frequency_hz);

nimcp_status_t nimcp_cerebellum_sync_to_rhythm(
    nimcp_cerebellum_t* cerebellum,
    float frequency_hz,
    float external_phase);

#endif /* NIMCP_CEREBELLUM_H */
```

### 2.5 Cerebellum Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/cerebellum/test_forward_model.cpp` | Prediction accuracy, learning |
| Unit | `test/unit/core/brain/regions/cerebellum/test_inverse_model.cpp` | Action computation, validation |
| Unit | `test/unit/core/brain/regions/cerebellum/test_timing_circuit.cpp` | Phase-locking, interval timing |
| Unit | `test/unit/core/brain/regions/cerebellum/test_granule_cell.cpp` | Sparsity, pattern separation |
| Integration | `test/integration/core/brain/regions/cerebellum/test_cerebellum_integration.cpp` | Full coordination |
| Regression | `test/regression/core/brain/regions/cerebellum/test_cerebellum_regression.cpp` | API, performance |

---

## Phase 3: Prefrontal Cortex Enhancement

### 3.1 Overview

The prefrontal cortex serves as the **executive orchestrator** - managing working memory, decision-making, planning, and cognitive control. This phase ENHANCES existing executive systems rather than replacing them.

### 3.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Executive | `nimcp_executive.c` | Task management, inhibition |
| Working Memory | `nimcp_working_memory.c` | Buffer management, rehearsal |
| Metacognition | `nimcp_metacognition.h` | Self-monitoring, confidence |
| Attention | `nimcp_attention.c` | Focus, selection |
| Decision | `nimcp_decision.h` | Choice evaluation |

### 3.3 New Modules to Implement

#### 3.3.1 Anterior Cingulate Cortex (Conflict Monitor)
**File**: `include/core/brain/regions/pfc/nimcp_acc.h`

```c
/**
 * WHAT: Conflict detection and cognitive control signaling.
 * WHY:  ACC detects response conflicts and triggers control adjustments.
 * HOW:  Energy-based conflict computation with control demand signals.
 */

typedef struct {
    /* Conflict detection */
    float conflict_level;           /* 0.0 - 1.0 */
    float conflict_threshold;       /* Trigger threshold */
    float conflict_history[16];     /* Rolling window */
    uint32_t history_index;

    /* Control demand signal */
    float control_demand;           /* Signal to DLPFC */
    float control_adjustment_rate;

    /* Error-related negativity tracking */
    float error_likelihood;
    float error_history[16];

    /* Integration with metacognition */
    nimcp_metacognition_t* metacognition;

    nimcp_memory_pool_t* pool;
} nimcp_acc_t;

/**
 * Mathematical Model (Botvinick et al., 2001):
 *   Conflict = Σᵢ Σⱼ (aᵢ · aⱼ · wᵢⱼ) for i ≠ j
 *
 * Where:
 *   a = response activations
 *   w = mutual incompatibility weights
 *
 * Control adjustment:
 *   control(t+1) = control(t) + η · (conflict - threshold)
 */

nimcp_status_t nimcp_acc_init(
    nimcp_acc_t* acc,
    nimcp_metacognition_t* metacognition,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_acc_evaluate_conflict(
    nimcp_acc_t* acc,
    const float* response_activations,
    uint32_t response_count);

float nimcp_acc_get_control_demand(const nimcp_acc_t* acc);
```

#### 3.3.2 Dorsolateral PFC (Working Memory + Control)
**File**: `include/core/brain/regions/pfc/nimcp_dlpfc.h`

```c
/**
 * WHAT: Enhanced working memory with control integration.
 * WHY:  DLPFC maintains task goals and exerts top-down control.
 * HOW:  Extends existing working_memory with control demand responses.
 */

typedef struct {
    /* Working memory reference (ORCHESTRATED) */
    nimcp_working_memory_t* working_memory;

    /* Goal maintenance */
    float* current_goal;
    uint32_t goal_dim;
    float goal_strength;            /* Maintenance activation */
    float goal_decay_rate;

    /* Control parameters */
    float control_intensity;        /* From ACC conflict signal */
    float inhibition_strength;      /* Response suppression */

    /* Task set representation */
    nimcp_cow_handle_t task_rules;  /* COW for task switching */
    uint32_t active_task_set;
    float task_inertia;             /* Resistance to switching */

    nimcp_memory_pool_t* pool;
} nimcp_dlpfc_t;

/**
 * Mathematical Model:
 *   Goal maintenance: dg/dt = -λ·g + I_external + control·g
 *
 * Control modulation:
 *   effective_signal = signal · (1 + control_intensity)
 *   inhibition = σ(w·conflict_signal)
 *
 * Task switching cost: RT_switch = RT_base + inertia·Δtask_set
 */

nimcp_status_t nimcp_dlpfc_init(
    nimcp_dlpfc_t* dlpfc,
    nimcp_working_memory_t* wm,
    uint32_t goal_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_dlpfc_set_goal(
    nimcp_dlpfc_t* dlpfc,
    const float* goal);

nimcp_status_t nimcp_dlpfc_maintain(
    nimcp_dlpfc_t* dlpfc,
    float control_demand,
    float delta_ms);

nimcp_status_t nimcp_dlpfc_switch_task(
    nimcp_dlpfc_t* dlpfc,
    uint32_t new_task_set);
```

#### 3.3.3 Ventromedial PFC (Value-Based Decisions)
**File**: `include/core/brain/regions/pfc/nimcp_vmpfc.h`

```c
/**
 * WHAT: Value computation and integration for decision-making.
 * WHY:  VMPFC computes subjective values and integrates with emotions.
 * HOW:  Prospect theory value function with loss aversion.
 */

typedef struct {
    /* Value parameters */
    float alpha;                    /* Diminishing sensitivity (gains) */
    float beta;                     /* Diminishing sensitivity (losses) */
    float lambda;                   /* Loss aversion coefficient */
    float reference_point;          /* Determines gain/loss framing */

    /* Integration weights */
    float emotional_weight;         /* From limbic system */
    float cognitive_weight;         /* From DLPFC */

    /* Decision state */
    float* option_values;
    uint32_t option_count;
    float decision_confidence;

    /* Connection to neuromodulators */
    nimcp_neuromod_state_t* neuromod;

    nimcp_memory_pool_t* pool;
} nimcp_vmpfc_t;

/**
 * Mathematical Model (Kahneman & Tversky):
 *   v(x) = x^α         if x >= 0 (gains)
 *   v(x) = -λ·(-x)^β   if x < 0  (losses)
 *
 * Subjective value:
 *   V = Σᵢ πᵢ · v(xᵢ)
 *
 * Where πᵢ = probability weighting function:
 *   π(p) = p^γ / (p^γ + (1-p)^γ)^(1/γ)
 */

nimcp_status_t nimcp_vmpfc_init(
    nimcp_vmpfc_t* vmpfc,
    nimcp_neuromod_state_t* neuromod,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_vmpfc_compute_value(
    nimcp_vmpfc_t* vmpfc,
    float outcome,
    float probability,
    float* subjective_value);

nimcp_status_t nimcp_vmpfc_evaluate_options(
    nimcp_vmpfc_t* vmpfc,
    const float* outcomes,
    const float* probabilities,
    uint32_t option_count);

uint32_t nimcp_vmpfc_select_best(const nimcp_vmpfc_t* vmpfc);
```

#### 3.3.4 Orbitofrontal Cortex (Reward Integration)
**File**: `include/core/brain/regions/pfc/nimcp_ofc.h`

```c
/**
 * WHAT: Reward and punishment integration with flexible updating.
 * WHY:  OFC tracks expected outcomes and signals prediction errors.
 * HOW:  Temporal difference learning with state-value estimation.
 */

typedef struct {
    /* State value estimates */
    float* state_values;
    uint32_t state_count;

    /* TD learning parameters */
    float learning_rate;
    float discount_factor;
    float eligibility_decay;

    /* Prediction error tracking */
    float last_prediction_error;
    float cumulative_reward;

    /* Reversal learning */
    float reversal_threshold;
    uint32_t consecutive_errors;
    bool reversal_detected;

    /* Memory with COW for value checkpointing */
    nimcp_cow_handle_t value_checkpoint;
    nimcp_memory_pool_t* pool;
} nimcp_ofc_t;

/**
 * Mathematical Model (TD Learning):
 *   δ = r + γ·V(s') - V(s)  (prediction error)
 *   V(s) ← V(s) + α·δ       (value update)
 *
 * With eligibility traces:
 *   e(s) ← γ·λ·e(s) + 1(s = current)
 *   V(s) ← V(s) + α·δ·e(s)
 *
 * Reversal detection:
 *   if consecutive_errors > threshold:
 *       trigger_reversal_learning()
 */

nimcp_status_t nimcp_ofc_init(
    nimcp_ofc_t* ofc,
    uint32_t state_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_ofc_update(
    nimcp_ofc_t* ofc,
    uint32_t prev_state,
    uint32_t current_state,
    float reward);

float nimcp_ofc_get_state_value(
    const nimcp_ofc_t* ofc,
    uint32_t state);

float nimcp_ofc_get_prediction_error(const nimcp_ofc_t* ofc);
```

### 3.4 PFC Main Module
**File**: `include/core/brain/regions/pfc/nimcp_pfc.h`

```c
/**
 * WHAT: Prefrontal cortex brain region - executive control center.
 * WHY:  Provides cognitive control, planning, decision-making, goal pursuit.
 * HOW:  Orchestrates existing executive systems with new ACC, DLPFC, VMPFC, OFC.
 */

#ifndef NIMCP_PFC_H
#define NIMCP_PFC_H

#include "nimcp_acc.h"
#include "nimcp_dlpfc.h"
#include "nimcp_vmpfc.h"
#include "nimcp_ofc.h"
#include "cognitive/executive/nimcp_executive.h"
#include "cognitive/working_memory/nimcp_working_memory.h"
#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/memory/nimcp_buffer_pool.h"

typedef struct {
    /* New PFC-specific modules */
    nimcp_acc_t    acc;             /* Conflict monitoring */
    nimcp_dlpfc_t  dlpfc;           /* Working memory + control */
    nimcp_vmpfc_t  vmpfc;           /* Value computation */
    nimcp_ofc_t    ofc;             /* Reward integration */

    /* References to orchestrated systems */
    nimcp_executive_t*      executive;       /* ORCHESTRATED */
    nimcp_working_memory_t* working_memory;  /* ORCHESTRATED */
    nimcp_metacognition_t*  metacognition;   /* ORCHESTRATED */
    nimcp_attention_t*      attention;       /* ORCHESTRATED */

    /* Connections to other regions */
    nimcp_cerebellum_t*     cerebellum;      /* Motor planning */
    nimcp_medulla_t*        medulla;         /* Arousal state */

    /* Memory infrastructure */
    nimcp_buffer_pool_t*    buffer_pool;
    nimcp_memory_pool_t     local_pool;

    /* Statistics */
    float avg_conflict_level;
    float avg_decision_confidence;
    uint64_t goal_switches;
} nimcp_pfc_t;

/* Lifecycle */
nimcp_status_t nimcp_pfc_init(
    nimcp_pfc_t* pfc,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_executive_t* executive,
    nimcp_working_memory_t* working_memory,
    nimcp_metacognition_t* metacognition,
    nimcp_attention_t* attention,
    nimcp_cerebellum_t* cerebellum,
    nimcp_medulla_t* medulla);

nimcp_status_t nimcp_pfc_destroy(nimcp_pfc_t* pfc);

/* Executive control */
nimcp_status_t nimcp_pfc_set_goal(
    nimcp_pfc_t* pfc,
    const float* goal,
    uint32_t goal_dim);

nimcp_status_t nimcp_pfc_update(
    nimcp_pfc_t* pfc,
    float delta_ms);

/* Decision-making */
nimcp_status_t nimcp_pfc_evaluate_options(
    nimcp_pfc_t* pfc,
    const float* outcomes,
    const float* probabilities,
    uint32_t option_count);

uint32_t nimcp_pfc_decide(nimcp_pfc_t* pfc);

/* Conflict and control */
float nimcp_pfc_get_conflict_level(const nimcp_pfc_t* pfc);
float nimcp_pfc_get_control_intensity(const nimcp_pfc_t* pfc);

#endif /* NIMCP_PFC_H */
```

### 3.5 PFC Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/pfc/test_acc.cpp` | Conflict detection, control signals |
| Unit | `test/unit/core/brain/regions/pfc/test_dlpfc.cpp` | Goal maintenance, task switching |
| Unit | `test/unit/core/brain/regions/pfc/test_vmpfc.cpp` | Value computation, option selection |
| Unit | `test/unit/core/brain/regions/pfc/test_ofc.cpp` | TD learning, reversal detection |
| Integration | `test/integration/core/brain/regions/pfc/test_pfc_integration.cpp` | Full executive control |
| Regression | `test/regression/core/brain/regions/pfc/test_pfc_regression.cpp` | API, performance |

---

---

## Phase 4: Hippocampus Implementation

### 4.1 Overview

The hippocampus serves as the **spatial and episodic memory system** - managing navigation, place recognition, and binding experiences into coherent memories. This is one of the largest new implementations.

### 4.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Engram | `nimcp_engram.c` | Memory formation |
| Systems Consolidation | `nimcp_systems_consolidation.c` | Memory replay |
| Autobiographical | `nimcp_autobiographical_memory.c` | Life events |
| Episodic Recovery | `nimcp_recovery_episodic_memory.c` | Fault tolerance |

### 4.3 New Modules to Implement

#### 4.3.1 Place Cell System
**File**: `include/core/brain/regions/hippocampus/nimcp_place_cells.h`

```c
/**
 * WHAT: Place cells that encode spatial location via Gaussian firing fields.
 * WHY:  Biological hippocampus CA1/CA3 neurons fire at specific locations.
 * HOW:  2D/3D Gaussian receptive fields with competitive activation.
 */

typedef struct {
    float center_x;                 /* Place field center X */
    float center_y;                 /* Place field center Y */
    float center_z;                 /* Place field center Z (optional) */
    float sigma;                    /* Field width (standard deviation) */
    float peak_rate;                /* Maximum firing rate */
    float current_activation;       /* Current firing rate */
} nimcp_place_cell_t;

typedef struct {
    nimcp_place_cell_t* cells;
    uint32_t cell_count;
    uint32_t dimensions;            /* 2D or 3D space */

    /* Spatial bounds */
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;

    /* Current location estimate */
    float estimated_x;
    float estimated_y;
    float estimated_z;
    float location_confidence;

    /* Learning */
    float field_plasticity;         /* Rate of place field formation */
    bool enable_remapping;          /* Allow field relocation */

    nimcp_memory_pool_t* pool;
} nimcp_place_cell_layer_t;

/**
 * Mathematical Model:
 *   firing_rate(x,y) = peak_rate · exp(-||pos - center||² / (2σ²))
 *
 * Population decoding (Bayesian):
 *   P(location | spikes) ∝ P(spikes | location) · P(location)
 *
 * Place field plasticity (BTSP - Behavioral Timescale Plasticity):
 *   Δw = η · (observed_rate - expected_rate) · eligibility
 */

nimcp_status_t nimcp_place_cells_init(
    nimcp_place_cell_layer_t* layer,
    uint32_t cell_count,
    uint32_t dimensions,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_place_cells_update(
    nimcp_place_cell_layer_t* layer,
    float current_x, float current_y, float current_z);

nimcp_status_t nimcp_place_cells_decode_location(
    const nimcp_place_cell_layer_t* layer,
    float* estimated_x, float* estimated_y, float* estimated_z);

nimcp_status_t nimcp_place_cells_learn(
    nimcp_place_cell_layer_t* layer,
    float reward_signal);
```

#### 4.3.2 Grid Cell System
**File**: `include/core/brain/regions/hippocampus/nimcp_grid_cells.h`

```c
/**
 * WHAT: Grid cells with hexagonal firing patterns for path integration.
 * WHY:  Entorhinal cortex grid cells enable metric spatial representation.
 * HOW:  Continuous attractor network with velocity integration.
 */

typedef struct {
    float phase_x;                  /* Grid phase (0-1) */
    float phase_y;
    float orientation;              /* Grid orientation (radians) */
    float spacing;                  /* Distance between peaks */
    float current_activation;
} nimcp_grid_cell_t;

typedef struct {
    nimcp_grid_cell_t* cells;
    uint32_t cell_count;

    /* Grid modules at different scales */
    uint32_t module_count;
    float* module_spacings;         /* Different grid scales */

    /* Velocity integration */
    float velocity_x;
    float velocity_y;
    float integration_gain;

    /* Path integration drift correction */
    float drift_error;
    uint32_t reset_count;

    nimcp_memory_pool_t* pool;
} nimcp_grid_cell_layer_t;

/**
 * Mathematical Model (Continuous Attractor):
 *   Hexagonal pattern: Σᵢ cos(kᵢ · position)
 *   where kᵢ are wave vectors at 60° intervals
 *
 * Path integration:
 *   phase(t+1) = phase(t) + velocity · dt / spacing
 *
 * Multi-scale coding (Fiete et al.):
 *   Combined modules give exponential capacity:
 *   capacity ∝ Π(spacingᵢ / resolution)
 */

nimcp_status_t nimcp_grid_cells_init(
    nimcp_grid_cell_layer_t* layer,
    uint32_t modules,
    const float* spacings,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_grid_cells_integrate_velocity(
    nimcp_grid_cell_layer_t* layer,
    float velocity_x, float velocity_y,
    float delta_ms);

nimcp_status_t nimcp_grid_cells_reset_to_landmark(
    nimcp_grid_cell_layer_t* layer,
    float landmark_x, float landmark_y);

nimcp_status_t nimcp_grid_cells_get_position(
    const nimcp_grid_cell_layer_t* layer,
    float* position_x, float* position_y);
```

#### 4.3.3 Dentate Gyrus Pattern Separation
**File**: `include/core/brain/regions/hippocampus/nimcp_dentate_gyrus.h`

```c
/**
 * WHAT: Pattern separation via sparse expansion coding.
 * WHY:  DG transforms similar inputs to dissimilar outputs (orthogonalization).
 * HOW:  Sparse random projection with competitive inhibition.
 */

typedef struct {
    /* Dimensions */
    uint32_t input_dim;             /* EC input (entorhinal cortex) */
    uint32_t granule_cells;         /* DG granule cells (>> input) */
    float sparsity;                 /* Active fraction (1-5%) */

    /* Connectivity */
    nimcp_cow_handle_t weights;     /* Sparse projection matrix */

    /* Activation */
    float* activations;
    uint32_t* active_indices;
    uint32_t active_count;

    /* Adult neurogenesis simulation */
    bool enable_neurogenesis;
    float neurogenesis_rate;
    uint32_t new_cells_added;

    nimcp_memory_pool_t* pool;
} nimcp_dentate_gyrus_t;

/**
 * Mathematical Model:
 *   Pattern separation: y = WTA(ReLU(W · x))
 *
 * Sparsity via winner-take-all:
 *   active = top k% of activations
 *
 * Orthogonalization metric:
 *   cos(y₁, y₂) << cos(x₁, x₂) for similar x₁, x₂
 *
 * Neurogenesis enables:
 *   - Temporal context encoding
 *   - Interference reduction
 */

nimcp_status_t nimcp_dentate_gyrus_init(
    nimcp_dentate_gyrus_t* dg,
    uint32_t input_dim,
    uint32_t granule_cells,
    float sparsity,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_dentate_gyrus_separate(
    nimcp_dentate_gyrus_t* dg,
    const float* input,
    float* separated_output);

float nimcp_dentate_gyrus_orthogonality(
    const nimcp_dentate_gyrus_t* dg,
    const float* pattern1,
    const float* pattern2);
```

#### 4.3.4 CA3 Pattern Completion
**File**: `include/core/brain/regions/hippocampus/nimcp_ca3.h`

```c
/**
 * WHAT: Autoassociative memory for pattern completion.
 * WHY:  CA3 recurrent connections enable recall from partial cues.
 * HOW:  Hopfield-like attractor network with learned associations.
 */

typedef struct {
    uint32_t neuron_count;

    /* Recurrent weights (symmetric for Hopfield) */
    nimcp_cow_handle_t recurrent_weights;

    /* State */
    float* activations;
    float* previous_activations;

    /* Attractor dynamics */
    uint32_t max_iterations;
    float convergence_threshold;
    float temperature;              /* For stochastic dynamics */

    /* Stored patterns */
    uint32_t pattern_count;
    uint32_t max_patterns;          /* Capacity limit */

    nimcp_memory_pool_t* pool;
} nimcp_ca3_t;

/**
 * Mathematical Model (Hopfield Network):
 *   Energy: E = -½ Σᵢⱼ wᵢⱼ sᵢ sⱼ
 *
 * Update rule:
 *   sᵢ(t+1) = sign(Σⱼ wᵢⱼ sⱼ(t))
 *
 * Storage (Hebb rule):
 *   wᵢⱼ = (1/N) Σₚ ξᵢᵖ ξⱼᵖ
 *
 * Capacity (Amit et al.):
 *   max_patterns ≈ 0.14 · N
 */

nimcp_status_t nimcp_ca3_init(
    nimcp_ca3_t* ca3,
    uint32_t neuron_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_ca3_store_pattern(
    nimcp_ca3_t* ca3,
    const float* pattern);

nimcp_status_t nimcp_ca3_recall(
    nimcp_ca3_t* ca3,
    const float* partial_cue,
    float* completed_pattern);

float nimcp_ca3_pattern_match(
    const nimcp_ca3_t* ca3,
    const float* pattern);
```

#### 4.3.5 Episodic Binding
**File**: `include/core/brain/regions/hippocampus/nimcp_episodic_binding.h`

```c
/**
 * WHAT: Binding of what/where/when into coherent episodic memories.
 * WHY:  Hippocampus binds multimodal information with spatiotemporal context.
 * HOW:  Conjunctive coding with temporal tagging.
 */

typedef struct {
    /* What (content) */
    float* content_vector;
    uint32_t content_dim;

    /* Where (spatial) */
    float location_x;
    float location_y;
    float location_z;

    /* When (temporal) */
    uint64_t timestamp_ns;
    uint32_t sequence_position;

    /* Binding strength */
    float binding_strength;
    float retrieval_count;

    /* Emotional valence (from amygdala) */
    float emotional_salience;
} nimcp_episode_t;

typedef struct {
    nimcp_episode_t* episodes;
    uint32_t episode_count;
    uint32_t max_episodes;

    /* Temporal context */
    float* temporal_context;        /* Slowly drifting context vector */
    float context_drift_rate;

    /* Integration with place/grid cells */
    nimcp_place_cell_layer_t* place_cells;
    nimcp_grid_cell_layer_t* grid_cells;

    /* Consolidation state */
    uint32_t unconsolidated_count;
    bool consolidation_pending;

    nimcp_memory_pool_t* pool;
} nimcp_episodic_memory_t;

/**
 * Mathematical Model:
 *   Episode = Content ⊗ Location ⊗ Time
 *   (tensor product binding)
 *
 * Temporal context (Howard & Kahana TCM):
 *   c(t) = ρ·c(t-1) + β·input(t)
 *
 * Retrieval (similarity-based):
 *   P(recall) ∝ similarity(cue, episode) · strength
 */

nimcp_status_t nimcp_episodic_init(
    nimcp_episodic_memory_t* em,
    uint32_t content_dim,
    uint32_t max_episodes,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_episodic_encode(
    nimcp_episodic_memory_t* em,
    const float* content,
    float emotional_salience);

nimcp_status_t nimcp_episodic_retrieve(
    nimcp_episodic_memory_t* em,
    const float* cue,
    nimcp_episode_t* retrieved,
    uint32_t max_results);

nimcp_status_t nimcp_episodic_consolidate(
    nimcp_episodic_memory_t* em);
```

### 4.4 Hippocampus Main Module
**File**: `include/core/brain/regions/hippocampus/nimcp_hippocampus.h`

```c
/**
 * WHAT: Hippocampus brain region - spatial and episodic memory system.
 * WHY:  Provides navigation, memory formation, and contextual binding.
 * HOW:  Orchestrates place/grid cells, DG, CA3 with existing memory systems.
 */

#ifndef NIMCP_HIPPOCAMPUS_H
#define NIMCP_HIPPOCAMPUS_H

#include "nimcp_place_cells.h"
#include "nimcp_grid_cells.h"
#include "nimcp_dentate_gyrus.h"
#include "nimcp_ca3.h"
#include "nimcp_episodic_binding.h"

typedef struct {
    /* New hippocampus-specific modules */
    nimcp_place_cell_layer_t  place_cells;
    nimcp_grid_cell_layer_t   grid_cells;
    nimcp_dentate_gyrus_t     dentate_gyrus;
    nimcp_ca3_t               ca3;
    nimcp_episodic_memory_t   episodic;

    /* References to orchestrated systems */
    nimcp_engram_t*           engram;              /* ORCHESTRATED */
    nimcp_systems_consolidation_t* consolidation;  /* ORCHESTRATED */

    /* Connections to other regions */
    nimcp_pfc_t*              pfc;                 /* Goal-directed memory */
    nimcp_amygdala_t*         amygdala;            /* Emotional tagging */

    /* Memory infrastructure */
    nimcp_buffer_pool_t*      buffer_pool;
    nimcp_memory_pool_t       local_pool;

    /* Statistics */
    uint64_t locations_visited;
    uint64_t episodes_encoded;
    float avg_pattern_separation;
} nimcp_hippocampus_t;

nimcp_status_t nimcp_hippocampus_init(
    nimcp_hippocampus_t* hpc,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_engram_t* engram,
    nimcp_systems_consolidation_t* consolidation);

nimcp_status_t nimcp_hippocampus_update_location(
    nimcp_hippocampus_t* hpc,
    float x, float y, float z,
    float velocity_x, float velocity_y);

nimcp_status_t nimcp_hippocampus_encode_experience(
    nimcp_hippocampus_t* hpc,
    const float* content,
    uint32_t content_dim,
    float emotional_salience);

nimcp_status_t nimcp_hippocampus_recall(
    nimcp_hippocampus_t* hpc,
    const float* cue,
    float* recalled_content);

nimcp_status_t nimcp_hippocampus_navigate_to(
    nimcp_hippocampus_t* hpc,
    float target_x, float target_y,
    float* direction_x, float* direction_y);

#endif /* NIMCP_HIPPOCAMPUS_H */
```

### 4.5 Hippocampus Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/hippocampus/test_place_cells.cpp` | Spatial encoding, decoding |
| Unit | `test/unit/core/brain/regions/hippocampus/test_grid_cells.cpp` | Path integration, multi-scale |
| Unit | `test/unit/core/brain/regions/hippocampus/test_dentate_gyrus.cpp` | Pattern separation |
| Unit | `test/unit/core/brain/regions/hippocampus/test_ca3.cpp` | Pattern completion, capacity |
| Unit | `test/unit/core/brain/regions/hippocampus/test_episodic_binding.cpp` | WWW binding |
| Integration | `test/integration/core/brain/regions/hippocampus/test_hippocampus_integration.cpp` | Full memory system |
| Regression | `test/regression/core/brain/regions/hippocampus/test_hippocampus_regression.cpp` | API, performance |

---

## Phase 5: Basal Ganglia Implementation

### 5.1 Overview

The basal ganglia serves as the **action selection and habit formation system** - managing Go/NoGo decisions, procedural learning, and reward-based behavior.

### 5.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Brain Learning | `nimcp_brain_learning.c` | Reward learning |
| Executive | `nimcp_executive.c` | Action execution |
| Neuromodulators | `nimcp_neuromodulators.c` | Dopamine signaling |

### 5.3 New Modules to Implement

#### 5.3.1 Direct Pathway (D1/Go)
**File**: `include/core/brain/regions/basal_ganglia/nimcp_direct_pathway.h`

```c
/**
 * WHAT: Direct pathway for action facilitation via D1 receptors.
 * WHY:  Biological striatum D1-MSNs enable "Go" signals for selected actions.
 * HOW:  Dopamine-modulated action values with winner-take-all selection.
 */

typedef struct {
    /* Action representations */
    float* action_values;           /* Q-values for each action */
    uint32_t action_count;

    /* D1 receptor modulation */
    float d1_sensitivity;           /* Dopamine responsiveness */
    float tonic_dopamine;           /* Baseline DA level */
    float phasic_dopamine;          /* Reward-related DA burst */

    /* Selection state */
    uint32_t selected_action;
    float selection_confidence;

    /* Learning */
    float learning_rate;
    float eligibility_decay;
    float* eligibility_traces;

    nimcp_memory_pool_t* pool;
} nimcp_direct_pathway_t;

/**
 * Mathematical Model:
 *   Go signal: G(a) = Q(a) · (1 + k_D1 · [DA])
 *
 * Learning (Actor-Critic):
 *   δ = r + γV(s') - V(s)         (TD error from SNc)
 *   Q(a) ← Q(a) + α · δ · e(a)    (D1 pathway)
 *
 * Selection:
 *   P(a) = softmax(G(a) / τ)
 */

nimcp_status_t nimcp_direct_pathway_init(
    nimcp_direct_pathway_t* pathway,
    uint32_t action_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_direct_pathway_evaluate(
    nimcp_direct_pathway_t* pathway,
    float dopamine_level);

uint32_t nimcp_direct_pathway_select_action(
    nimcp_direct_pathway_t* pathway,
    float temperature);

nimcp_status_t nimcp_direct_pathway_learn(
    nimcp_direct_pathway_t* pathway,
    float td_error);
```

#### 5.3.2 Indirect Pathway (D2/NoGo)
**File**: `include/core/brain/regions/basal_ganglia/nimcp_indirect_pathway.h`

```c
/**
 * WHAT: Indirect pathway for action suppression via D2 receptors.
 * WHY:  D2-MSNs provide "NoGo" signals to inhibit inappropriate actions.
 * HOW:  Dopamine-modulated inhibition values with opposing dynamics.
 */

typedef struct {
    /* Inhibition representations */
    float* inhibition_values;       /* NoGo values for each action */
    uint32_t action_count;

    /* D2 receptor modulation (inverse of D1) */
    float d2_sensitivity;
    float tonic_dopamine;
    float phasic_dopamine;

    /* Suppression state */
    float* suppression_signals;
    float global_inhibition;        /* GPe → STN → GPi */

    /* Learning */
    float learning_rate;
    float* eligibility_traces;

    nimcp_memory_pool_t* pool;
} nimcp_indirect_pathway_t;

/**
 * Mathematical Model:
 *   NoGo signal: N(a) = Q_nogo(a) · (1 - k_D2 · [DA])
 *
 * Learning (opposite sign):
 *   Q_nogo(a) ← Q_nogo(a) - α · δ · e(a)
 *
 * Net selection:
 *   P(a) ∝ exp((G(a) - N(a)) / τ)
 */

nimcp_status_t nimcp_indirect_pathway_init(
    nimcp_indirect_pathway_t* pathway,
    uint32_t action_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_indirect_pathway_evaluate(
    nimcp_indirect_pathway_t* pathway,
    float dopamine_level);

nimcp_status_t nimcp_indirect_pathway_learn(
    nimcp_indirect_pathway_t* pathway,
    float td_error);

float nimcp_indirect_pathway_get_suppression(
    const nimcp_indirect_pathway_t* pathway,
    uint32_t action);
```

#### 5.3.3 Habit Formation
**File**: `include/core/brain/regions/basal_ganglia/nimcp_habit_formation.h`

```c
/**
 * WHAT: Procedural memory and habit formation system.
 * WHY:  Dorsolateral striatum automates stimulus-response mappings.
 * HOW:  Gradual transfer from goal-directed to habitual control.
 */

typedef struct {
    /* Stimulus-response associations */
    nimcp_cow_handle_t sr_weights;  /* S→R mapping */
    uint32_t stimulus_dim;
    uint32_t response_count;

    /* Habit strength */
    float* habit_strengths;         /* Per S-R pair */
    float habit_threshold;          /* When to use habit vs goal-directed */

    /* Training history */
    uint32_t* repetition_counts;
    float overtraining_factor;

    /* Balance with goal-directed */
    float goal_directed_weight;
    float habitual_weight;

    nimcp_memory_pool_t* pool;
} nimcp_habit_system_t;

/**
 * Mathematical Model:
 *   Habit strength: H(s,r) = 1 - exp(-k · repetitions)
 *
 * Response selection:
 *   R = w_goal · π_goal(s) + w_habit · π_habit(s)
 *   where w_habit = H(s,r) and w_goal = 1 - H(s,r)
 *
 * Devaluation insensitivity:
 *   Habitual: response persists despite outcome devaluation
 *   Goal-directed: response sensitive to outcome value
 */

nimcp_status_t nimcp_habit_init(
    nimcp_habit_system_t* habits,
    uint32_t stimulus_dim,
    uint32_t response_count,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_habit_strengthen(
    nimcp_habit_system_t* habits,
    const float* stimulus,
    uint32_t response);

uint32_t nimcp_habit_respond(
    nimcp_habit_system_t* habits,
    const float* stimulus);

float nimcp_habit_get_strength(
    const nimcp_habit_system_t* habits,
    const float* stimulus,
    uint32_t response);

bool nimcp_habit_is_habitual(
    const nimcp_habit_system_t* habits,
    const float* stimulus);
```

### 5.4 Basal Ganglia Main Module
**File**: `include/core/brain/regions/basal_ganglia/nimcp_basal_ganglia.h`

```c
/**
 * WHAT: Basal ganglia brain region - action selection and habit system.
 * WHY:  Provides Go/NoGo decision making and procedural learning.
 * HOW:  Orchestrates D1/D2 pathways with existing reward/executive systems.
 */

#ifndef NIMCP_BASAL_GANGLIA_H
#define NIMCP_BASAL_GANGLIA_H

#include "nimcp_direct_pathway.h"
#include "nimcp_indirect_pathway.h"
#include "nimcp_habit_formation.h"

typedef struct {
    /* New basal ganglia modules */
    nimcp_direct_pathway_t    direct;         /* D1/Go */
    nimcp_indirect_pathway_t  indirect;       /* D2/NoGo */
    nimcp_habit_system_t      habits;

    /* References to orchestrated systems */
    nimcp_neuromod_state_t*   neuromodulators;  /* ORCHESTRATED - dopamine */
    nimcp_executive_t*        executive;        /* ORCHESTRATED */

    /* Connections */
    nimcp_pfc_t*              pfc;              /* Goal-directed control */
    nimcp_cerebellum_t*       cerebellum;       /* Motor coordination */

    /* State */
    uint32_t current_action;
    float action_vigor;                         /* Movement speed/force */

    /* Memory infrastructure */
    nimcp_buffer_pool_t*      buffer_pool;
    nimcp_memory_pool_t       local_pool;

    /* Statistics */
    uint64_t actions_selected;
    uint64_t habits_formed;
    float avg_selection_time_ms;
} nimcp_basal_ganglia_t;

nimcp_status_t nimcp_basal_ganglia_init(
    nimcp_basal_ganglia_t* bg,
    uint32_t action_count,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_neuromod_state_t* neuromod,
    nimcp_executive_t* executive);

nimcp_status_t nimcp_basal_ganglia_select_action(
    nimcp_basal_ganglia_t* bg,
    const float* state,
    uint32_t* selected_action);

nimcp_status_t nimcp_basal_ganglia_learn_from_reward(
    nimcp_basal_ganglia_t* bg,
    float reward,
    float next_value);

bool nimcp_basal_ganglia_is_habitual(
    const nimcp_basal_ganglia_t* bg,
    const float* stimulus);

#endif /* NIMCP_BASAL_GANGLIA_H */
```

### 5.5 Basal Ganglia Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/basal_ganglia/test_direct_pathway.cpp` | Go signals, D1 modulation |
| Unit | `test/unit/core/brain/regions/basal_ganglia/test_indirect_pathway.cpp` | NoGo signals, D2 modulation |
| Unit | `test/unit/core/brain/regions/basal_ganglia/test_habit_formation.cpp` | Habit strength, automaticity |
| Integration | `test/integration/core/brain/regions/basal_ganglia/test_basal_ganglia_integration.cpp` | Full action selection |
| Regression | `test/regression/core/brain/regions/basal_ganglia/test_basal_ganglia_regression.cpp` | API, performance |

---

## Phase 6: Wernicke's Area Implementation

### 6.1 Overview

Wernicke's area serves as the **language comprehension center** - processing speech input, extracting meaning, and interfacing with Broca's area for language production.

### 6.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Speech Cortex | `nimcp_speech_cortex.c` | Phoneme processing |
| Language Bridge | `nimcp_language_production_bridge.h` | Broca's connection |
| NLP | `nimcp_nlp.c` | Semantic processing |
| Semantic Memory | `nimcp_semantic_memory.c` | Concept retrieval |

### 6.3 New Modules to Implement

#### 6.3.1 Lexical Access
**File**: `include/core/brain/regions/wernicke/nimcp_lexical_access.h`

```c
/**
 * WHAT: Word recognition and lexical retrieval system.
 * WHY:  Wernicke's area maps phoneme sequences to lexical entries.
 * HOW:  Spreading activation in lexical network with frequency effects.
 */

typedef struct {
    /* Lexicon */
    uint32_t vocabulary_size;
    nimcp_cow_handle_t word_embeddings;
    nimcp_cow_handle_t phoneme_to_word;     /* Phoneme → word mapping */

    /* Activation state */
    float* word_activations;
    uint32_t* candidate_words;
    uint32_t candidate_count;

    /* Frequency effects */
    float* word_frequencies;                 /* Log frequency */
    float frequency_bias;

    /* Competition */
    float lateral_inhibition;
    float activation_threshold;

    nimcp_memory_pool_t* pool;
} nimcp_lexical_access_t;

/**
 * Mathematical Model:
 *   Activation: a(w,t+1) = a(w,t) + input(w) - inhibition·Σa(w') - decay·a(w,t)
 *
 * Frequency effect:
 *   threshold(w) = base_threshold - k·log(frequency(w))
 *
 * Recognition point:
 *   word recognized when a(w) > threshold AND a(w) > all competitors
 */

nimcp_status_t nimcp_lexical_access_init(
    nimcp_lexical_access_t* lex,
    uint32_t vocabulary_size,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_lexical_access_process_phonemes(
    nimcp_lexical_access_t* lex,
    const float* phoneme_features,
    uint32_t phoneme_count);

nimcp_status_t nimcp_lexical_access_get_candidates(
    const nimcp_lexical_access_t* lex,
    uint32_t* words,
    float* activations,
    uint32_t max_candidates);

uint32_t nimcp_lexical_access_recognize(
    nimcp_lexical_access_t* lex);
```

#### 6.3.2 Semantic Integration
**File**: `include/core/brain/regions/wernicke/nimcp_semantic_integration.h`

```c
/**
 * WHAT: Compositional semantic processing for sentence understanding.
 * WHY:  Language comprehension requires combining word meanings.
 * HOW:  Incremental semantic composition with expectation-based processing.
 */

typedef struct {
    /* Semantic state */
    float* current_meaning;                  /* Accumulated sentence meaning */
    uint32_t meaning_dim;

    /* Composition */
    nimcp_cow_handle_t composition_weights;  /* How to combine meanings */

    /* Prediction */
    float* predicted_next;                   /* Expected next word features */
    float prediction_strength;

    /* Integration difficulty */
    float surprisal;                         /* -log P(word | context) */
    float integration_cost;

    /* Connection to semantic memory */
    nimcp_semantic_memory_t* semantic_memory;

    nimcp_memory_pool_t* pool;
} nimcp_semantic_integration_t;

/**
 * Mathematical Model:
 *   Composition: meaning(s) = f(meaning(s-1), word_i)
 *
 * Surprisal (Hale, 2001):
 *   S(w_i) = -log₂ P(w_i | w_1...w_{i-1})
 *
 * Integration cost (Gibson, 2000):
 *   Cost = f(distance, dependency_type)
 *
 * N400 amplitude ∝ surprisal + semantic_distance
 */

nimcp_status_t nimcp_semantic_integration_init(
    nimcp_semantic_integration_t* sem,
    uint32_t meaning_dim,
    nimcp_semantic_memory_t* semantic_memory,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_semantic_integration_process_word(
    nimcp_semantic_integration_t* sem,
    const float* word_embedding);

nimcp_status_t nimcp_semantic_integration_get_meaning(
    const nimcp_semantic_integration_t* sem,
    float* meaning);

float nimcp_semantic_integration_get_surprisal(
    const nimcp_semantic_integration_t* sem);

nimcp_status_t nimcp_semantic_integration_reset(
    nimcp_semantic_integration_t* sem);
```

#### 6.3.3 Arcuate Fasciculus (Broca-Wernicke Connection)
**File**: `include/core/brain/regions/wernicke/nimcp_arcuate_fasciculus.h`

```c
/**
 * WHAT: Bidirectional fiber tract connecting Wernicke's and Broca's areas.
 * WHY:  Language requires comprehension-production coordination.
 * HOW:  Signal transmission with prediction and feedback.
 */

typedef struct {
    /* Forward pathway (Wernicke → Broca) */
    float* forward_signal;                   /* Semantic → articulatory */
    uint32_t forward_dim;
    float forward_delay_ms;

    /* Backward pathway (Broca → Wernicke) */
    float* backward_signal;                  /* Articulatory → phonological */
    uint32_t backward_dim;
    float backward_delay_ms;

    /* Prediction (efference copy) */
    float* predicted_feedback;
    float prediction_error;

    /* Connection references */
    nimcp_broca_region_t* broca;
    nimcp_wernicke_t* wernicke;

    nimcp_buffer_pool_t* signal_buffer;
} nimcp_arcuate_fasciculus_t;

/**
 * Biological basis:
 *   - Dorsal stream: Sound → articulation mapping
 *   - Ventral stream: Sound → meaning mapping
 *
 * Functions:
 *   - Repetition: Wernicke → Broca for speech repetition
 *   - Inner speech: Broca → Wernicke for internal monitoring
 *   - Prediction: Efference copy for speech monitoring
 */

nimcp_status_t nimcp_arcuate_init(
    nimcp_arcuate_fasciculus_t* arc,
    nimcp_broca_region_t* broca,
    nimcp_buffer_pool_t* pool);

nimcp_status_t nimcp_arcuate_send_to_broca(
    nimcp_arcuate_fasciculus_t* arc,
    const float* semantic_representation);

nimcp_status_t nimcp_arcuate_receive_from_broca(
    nimcp_arcuate_fasciculus_t* arc,
    float* articulatory_feedback);

nimcp_status_t nimcp_arcuate_update(
    nimcp_arcuate_fasciculus_t* arc,
    float delta_ms);
```

### 6.4 Wernicke's Main Module
**File**: `include/core/brain/regions/wernicke/nimcp_wernicke.h`

```c
/**
 * WHAT: Wernicke's area - language comprehension center.
 * WHY:  Completes language circuit with Broca's for full language processing.
 * HOW:  Orchestrates lexical access, semantic integration with existing NLP.
 */

#ifndef NIMCP_WERNICKE_H
#define NIMCP_WERNICKE_H

#include "nimcp_lexical_access.h"
#include "nimcp_semantic_integration.h"
#include "nimcp_arcuate_fasciculus.h"

typedef struct {
    /* New Wernicke modules */
    nimcp_lexical_access_t       lexical;
    nimcp_semantic_integration_t semantic;
    nimcp_arcuate_fasciculus_t   arcuate;

    /* References to orchestrated systems */
    nimcp_speech_cortex_t*       speech_cortex;    /* ORCHESTRATED */
    nimcp_semantic_memory_t*     semantic_memory;  /* ORCHESTRATED */

    /* Connections */
    nimcp_broca_region_t*        broca;            /* Production partner */
    nimcp_hippocampus_t*         hippocampus;      /* Episodic context */

    /* Processing state */
    bool processing_active;
    float comprehension_confidence;

    /* Memory infrastructure */
    nimcp_buffer_pool_t*         buffer_pool;
    nimcp_memory_pool_t          local_pool;

    /* Statistics */
    uint64_t words_processed;
    float avg_lexical_access_ms;
    float avg_surprisal;
} nimcp_wernicke_t;

nimcp_status_t nimcp_wernicke_init(
    nimcp_wernicke_t* wernicke,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_speech_cortex_t* speech_cortex,
    nimcp_semantic_memory_t* semantic_memory,
    nimcp_broca_region_t* broca);

nimcp_status_t nimcp_wernicke_process_speech(
    nimcp_wernicke_t* wernicke,
    const float* audio_features,
    uint32_t feature_count);

nimcp_status_t nimcp_wernicke_get_comprehension(
    const nimcp_wernicke_t* wernicke,
    float* meaning,
    uint32_t meaning_dim);

nimcp_status_t nimcp_wernicke_repeat(
    nimcp_wernicke_t* wernicke);

#endif /* NIMCP_WERNICKE_H */
```

### 6.5 Wernicke's Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/wernicke/test_lexical_access.cpp` | Word recognition, frequency effects |
| Unit | `test/unit/core/brain/regions/wernicke/test_semantic_integration.cpp` | Composition, surprisal |
| Unit | `test/unit/core/brain/regions/wernicke/test_arcuate_fasciculus.cpp` | Bidirectional transmission |
| Integration | `test/integration/core/brain/regions/wernicke/test_wernicke_integration.cpp` | Full comprehension |
| Regression | `test/regression/core/brain/regions/wernicke/test_wernicke_regression.cpp` | API, performance |

---

## Phase 7: Amygdala Enhancement

### 7.1 Overview

The amygdala enhancement adds **fear conditioning, threat detection, and emotional memory** to the existing emotional processing systems.

### 7.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Emotional System | `nimcp_emotional_system.c` | Core emotions |
| Emotional Tagging | `nimcp_emotional_tagging.c` | Valence assignment |
| Shadow Emotions | `nimcp_shadow_emotions.c` | Implicit affect |

### 7.3 New Modules to Implement

#### 7.3.1 Fear Conditioning
**File**: `include/core/brain/regions/amygdala/nimcp_fear_conditioning.h`

```c
/**
 * WHAT: Classical fear conditioning via CS-US associations.
 * WHY:  Biological amygdala learns predictive threat signals.
 * HOW:  Rescorla-Wagner learning with lateral amygdala plasticity.
 */

typedef struct {
    /* CS-US associations */
    nimcp_cow_handle_t associations;        /* CS → fear response weights */
    uint32_t cs_dim;                        /* Conditioned stimulus dimension */

    /* Fear response */
    float fear_level;                       /* Current fear (0-1) */
    float* cs_activations;                  /* CS pattern activations */

    /* Learning parameters */
    float learning_rate;
    float max_association;                  /* Asymptote */

    /* US representation */
    float us_intensity;                     /* Unconditioned stimulus strength */
    bool us_present;

    nimcp_memory_pool_t* pool;
} nimcp_fear_conditioning_t;

/**
 * Mathematical Model (Rescorla-Wagner):
 *   ΔV = α·β·(λ - ΣV)
 *
 * Where:
 *   α = CS salience
 *   β = US intensity
 *   λ = maximum conditioning
 *   ΣV = sum of all CS associations
 *
 * Blocking: New CS doesn't condition if US already predicted
 */

nimcp_status_t nimcp_fear_conditioning_init(
    nimcp_fear_conditioning_t* fc,
    uint32_t cs_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_fear_conditioning_present_cs(
    nimcp_fear_conditioning_t* fc,
    const float* cs_pattern);

nimcp_status_t nimcp_fear_conditioning_present_us(
    nimcp_fear_conditioning_t* fc,
    float us_intensity);

nimcp_status_t nimcp_fear_conditioning_trial(
    nimcp_fear_conditioning_t* fc);

float nimcp_fear_conditioning_get_fear(
    const nimcp_fear_conditioning_t* fc);
```

#### 7.3.2 Threat Detection
**File**: `include/core/brain/regions/amygdala/nimcp_threat_detection.h`

```c
/**
 * WHAT: Fast threat detection via dual pathway processing.
 * WHY:  Amygdala receives fast thalamic input for rapid threat response.
 * HOW:  Parallel fast (crude) and slow (detailed) threat evaluation.
 */

typedef struct {
    /* Fast pathway (thalamus → amygdala) */
    float* fast_features;                   /* Low-level threat features */
    uint32_t fast_dim;
    float fast_threshold;
    float fast_response_ms;                 /* ~12ms in biology */

    /* Slow pathway (cortex → amygdala) */
    float* slow_features;                   /* Detailed threat analysis */
    uint32_t slow_dim;
    float slow_threshold;
    float slow_response_ms;                 /* ~30-40ms */

    /* Threat state */
    float threat_level;
    bool threat_detected;
    uint64_t detection_time_ns;

    /* Feature detectors */
    nimcp_cow_handle_t threat_templates;    /* Learned threat patterns */
    uint32_t template_count;

    nimcp_memory_pool_t* pool;
} nimcp_threat_detection_t;

/**
 * Biological basis (LeDoux):
 *   Low road: Thalamus → Amygdala (fast, coarse)
 *   High road: Thalamus → Cortex → Amygdala (slow, detailed)
 *
 * False positive bias:
 *   Better to react to non-threat than miss real threat
 *   fast_threshold < slow_threshold
 */

nimcp_status_t nimcp_threat_detection_init(
    nimcp_threat_detection_t* td,
    uint32_t fast_dim,
    uint32_t slow_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_threat_detection_fast_evaluate(
    nimcp_threat_detection_t* td,
    const float* sensory_input);

nimcp_status_t nimcp_threat_detection_slow_evaluate(
    nimcp_threat_detection_t* td,
    const float* cortical_input);

bool nimcp_threat_detection_is_threat(
    const nimcp_threat_detection_t* td);

float nimcp_threat_detection_get_level(
    const nimcp_threat_detection_t* td);
```

#### 7.3.3 Extinction Learning
**File**: `include/core/brain/regions/amygdala/nimcp_extinction.h`

```c
/**
 * WHAT: Fear extinction via new inhibitory learning.
 * WHY:  Extinction doesn't erase fear but creates competing safety memory.
 * HOW:  Prefrontal-amygdala inhibition with context-dependence.
 */

typedef struct {
    /* Extinction memory */
    nimcp_cow_handle_t extinction_weights;  /* CS → safety associations */
    uint32_t cs_dim;

    /* Inhibitory strength */
    float* inhibition_levels;
    float extinction_threshold;

    /* Context-dependence */
    float* extinction_context;              /* Context where extinction occurred */
    uint32_t context_dim;
    float context_specificity;

    /* Spontaneous recovery */
    float recovery_rate;
    uint64_t time_since_extinction_ns;

    /* Connection to mPFC */
    bool mpfc_active;                       /* Prefrontal control */
    float mpfc_inhibition;

    nimcp_memory_pool_t* pool;
} nimcp_extinction_t;

/**
 * Mathematical Model:
 *   Fear expression = fear_memory - extinction_memory · context_match
 *
 * Extinction learning:
 *   ΔV_ext = α · (0 - V_fear) when US absent
 *
 * Spontaneous recovery:
 *   V_ext(t) = V_ext(0) · exp(-recovery_rate · t)
 *
 * Renewal effect:
 *   Fear returns in non-extinction context
 */

nimcp_status_t nimcp_extinction_init(
    nimcp_extinction_t* ext,
    uint32_t cs_dim,
    uint32_t context_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_extinction_trial(
    nimcp_extinction_t* ext,
    const float* cs_pattern,
    const float* context,
    bool us_present);

float nimcp_extinction_get_net_fear(
    const nimcp_extinction_t* ext,
    const float* cs_pattern,
    const float* current_context);

nimcp_status_t nimcp_extinction_update_time(
    nimcp_extinction_t* ext,
    uint64_t elapsed_ns);
```

### 7.4 Amygdala Main Module
**File**: `include/core/brain/regions/amygdala/nimcp_amygdala.h`

```c
/**
 * WHAT: Amygdala brain region - emotional learning and threat processing.
 * WHY:  Provides fear conditioning, threat detection, emotional memory.
 * HOW:  Enhances existing emotional systems with conditioning and extinction.
 */

#ifndef NIMCP_AMYGDALA_H
#define NIMCP_AMYGDALA_H

#include "nimcp_fear_conditioning.h"
#include "nimcp_threat_detection.h"
#include "nimcp_extinction.h"

typedef struct {
    /* New amygdala modules */
    nimcp_fear_conditioning_t fear_conditioning;
    nimcp_threat_detection_t  threat_detection;
    nimcp_extinction_t        extinction;

    /* References to orchestrated systems */
    nimcp_emotional_system_t*  emotional_system;   /* ORCHESTRATED */
    nimcp_emotional_tagging_t* emotional_tagging;  /* ORCHESTRATED */

    /* Connections */
    nimcp_hippocampus_t*       hippocampus;        /* Context, episodic */
    nimcp_pfc_t*               pfc;                /* Regulation */
    nimcp_medulla_t*           medulla;            /* Arousal output */

    /* State */
    float current_fear;
    float current_threat;
    bool fight_flight_active;

    /* Memory infrastructure */
    nimcp_buffer_pool_t*       buffer_pool;
    nimcp_memory_pool_t        local_pool;

    /* Statistics */
    uint64_t threat_detections;
    uint64_t fear_conditionings;
    uint64_t extinctions;
} nimcp_amygdala_t;

nimcp_status_t nimcp_amygdala_init(
    nimcp_amygdala_t* amyg,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_emotional_system_t* emotions,
    nimcp_emotional_tagging_t* tagging);

nimcp_status_t nimcp_amygdala_process_stimulus(
    nimcp_amygdala_t* amyg,
    const float* sensory_input,
    const float* cortical_input,
    const float* context);

nimcp_status_t nimcp_amygdala_condition(
    nimcp_amygdala_t* amyg,
    const float* cs,
    float us_intensity);

nimcp_status_t nimcp_amygdala_extinguish(
    nimcp_amygdala_t* amyg,
    const float* cs,
    const float* context);

float nimcp_amygdala_get_fear(const nimcp_amygdala_t* amyg);
float nimcp_amygdala_get_threat(const nimcp_amygdala_t* amyg);

#endif /* NIMCP_AMYGDALA_H */
```

### 7.5 Amygdala Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/amygdala/test_fear_conditioning.cpp` | CS-US learning, blocking |
| Unit | `test/unit/core/brain/regions/amygdala/test_threat_detection.cpp` | Dual pathway, timing |
| Unit | `test/unit/core/brain/regions/amygdala/test_extinction.cpp` | Inhibitory learning, context |
| Integration | `test/integration/core/brain/regions/amygdala/test_amygdala_integration.cpp` | Full emotional processing |
| Regression | `test/regression/core/brain/regions/amygdala/test_amygdala_regression.cpp` | API, performance |

---

## Phase 8: Posterior Cingulate Cortex / Default Mode Network

### 8.1 Overview

The posterior cingulate cortex (PCC) and default mode network (DMN) handle **self-referential processing, mind-wandering, and memory retrieval** during task-free states.

### 8.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Introspection | `nimcp_introspection.c` | Self-monitoring |
| Self Model | `nimcp_self_model.c` | Self-representation |
| Autobiographical | `nimcp_autobiographical_memory.c` | Life memories |

### 8.3 New Modules to Implement

#### 8.3.1 Default Mode Controller
**File**: `include/core/brain/regions/pcc/nimcp_default_mode.h`

```c
/**
 * WHAT: Default mode network activation during task-free periods.
 * WHY:  DMN enables spontaneous cognition, planning, social thinking.
 * HOW:  Anti-correlated with task-positive networks, activates during rest.
 */

typedef struct {
    /* DMN state */
    float dmn_activation;                   /* 0 = task-focused, 1 = DMN active */
    float task_demand;                      /* External task requirements */

    /* DMN components */
    float pcc_activity;                     /* Posterior cingulate */
    float mpfc_activity;                    /* Medial prefrontal */
    float lateral_temporal_activity;
    float hippocampal_activity;

    /* Anti-correlation with task networks */
    float task_positive_activity;
    float anticorrelation_strength;

    /* Mind-wandering */
    bool mind_wandering;
    float wandering_duration_ms;
    uint64_t last_external_input_ns;

    nimcp_memory_pool_t* pool;
} nimcp_default_mode_t;

/**
 * Mathematical Model:
 *   DMN = 1 - task_demand (anti-correlated)
 *
 * Activation dynamics:
 *   d(DMN)/dt = (target - DMN) / τ
 *   where target = 1 - task_positive
 *
 * Mind-wandering onset:
 *   P(wandering) increases with time since last external input
 */

nimcp_status_t nimcp_default_mode_init(
    nimcp_default_mode_t* dmn,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_default_mode_update(
    nimcp_default_mode_t* dmn,
    float task_demand,
    float delta_ms);

nimcp_status_t nimcp_default_mode_external_input(
    nimcp_default_mode_t* dmn);

bool nimcp_default_mode_is_active(
    const nimcp_default_mode_t* dmn);

bool nimcp_default_mode_is_wandering(
    const nimcp_default_mode_t* dmn);
```

#### 8.3.2 Self-Referential Processing
**File**: `include/core/brain/regions/pcc/nimcp_self_reference.h`

```c
/**
 * WHAT: Processing of self-relevant information and self-reflection.
 * WHY:  PCC/mPFC active during self-referential thought and theory of mind.
 * HOW:  Self-model queries with autobiographical memory integration.
 */

typedef struct {
    /* Self-model reference */
    nimcp_self_model_t* self_model;

    /* Self-relevance detection */
    float* self_features;                   /* What defines "self" */
    uint32_t feature_dim;
    float self_relevance_threshold;

    /* Current self-referential state */
    float self_focus;                       /* Attention to self vs other */
    float* current_self_thought;
    bool self_reflection_active;

    /* Autobiographical integration */
    nimcp_autobiographical_memory_t* autobio;
    float memory_retrieval_prob;

    nimcp_memory_pool_t* pool;
} nimcp_self_reference_t;

/**
 * Mathematical Model:
 *   Self-relevance(stimulus) = similarity(stimulus, self_features)
 *
 * Self-reference effect:
 *   Memory enhancement for self-relevant information
 *   P(recall | self-ref) > P(recall | other-ref)
 *
 * Theory of mind:
 *   Other_model = transform(self_model, perspective_shift)
 */

nimcp_status_t nimcp_self_reference_init(
    nimcp_self_reference_t* sr,
    nimcp_self_model_t* self_model,
    nimcp_autobiographical_memory_t* autobio,
    nimcp_memory_pool_t* pool);

float nimcp_self_reference_evaluate(
    nimcp_self_reference_t* sr,
    const float* stimulus);

nimcp_status_t nimcp_self_reference_reflect(
    nimcp_self_reference_t* sr);

nimcp_status_t nimcp_self_reference_retrieve_memory(
    nimcp_self_reference_t* sr,
    const float* cue,
    float* memory);
```

#### 8.3.3 Spontaneous Thought Generator
**File**: `include/core/brain/regions/pcc/nimcp_spontaneous_thought.h`

```c
/**
 * WHAT: Generation of spontaneous, undirected thoughts during DMN activity.
 * WHY:  Mind-wandering involves semi-random memory retrieval and simulation.
 * HOW:  Stochastic sampling from memory with associative chaining.
 */

typedef struct {
    /* Thought state */
    float* current_thought;
    uint32_t thought_dim;
    float thought_coherence;

    /* Generation parameters */
    float randomness;                       /* Exploration vs exploitation */
    float association_strength;             /* Chain strength */
    float goal_attraction;                  /* Pull toward concerns */

    /* Personal concerns (attract thoughts) */
    float* current_concerns;
    uint32_t concern_count;

    /* Thought chain history */
    float** thought_history;
    uint32_t history_length;
    uint32_t history_index;

    nimcp_memory_pool_t* pool;
} nimcp_spontaneous_thought_t;

/**
 * Mathematical Model:
 *   P(next_thought) ∝ similarity(current, next) · concern_relevance(next) · random
 *
 * Associative chaining:
 *   thought(t+1) = sample(P(thought | thought(t), concerns))
 *
 * Current concerns theory (Klinger):
 *   Thoughts biased toward unfinished goals and emotional concerns
 */

nimcp_status_t nimcp_spontaneous_thought_init(
    nimcp_spontaneous_thought_t* st,
    uint32_t thought_dim,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_spontaneous_thought_set_concerns(
    nimcp_spontaneous_thought_t* st,
    const float* concerns,
    uint32_t concern_count);

nimcp_status_t nimcp_spontaneous_thought_generate(
    nimcp_spontaneous_thought_t* st,
    const float* seed);

nimcp_status_t nimcp_spontaneous_thought_get_current(
    const nimcp_spontaneous_thought_t* st,
    float* thought);
```

### 8.4 PCC Main Module
**File**: `include/core/brain/regions/pcc/nimcp_pcc.h`

```c
/**
 * WHAT: Posterior cingulate cortex - default mode network hub.
 * WHY:  Provides self-referential processing and spontaneous cognition.
 * HOW:  Orchestrates DMN, self-reference, and spontaneous thought.
 */

#ifndef NIMCP_PCC_H
#define NIMCP_PCC_H

#include "nimcp_default_mode.h"
#include "nimcp_self_reference.h"
#include "nimcp_spontaneous_thought.h"

typedef struct {
    /* New PCC modules */
    nimcp_default_mode_t       default_mode;
    nimcp_self_reference_t     self_reference;
    nimcp_spontaneous_thought_t spontaneous;

    /* References to orchestrated systems */
    nimcp_introspection_t*     introspection;     /* ORCHESTRATED */
    nimcp_self_model_t*        self_model;        /* ORCHESTRATED */
    nimcp_autobiographical_memory_t* autobio;     /* ORCHESTRATED */

    /* Connections */
    nimcp_hippocampus_t*       hippocampus;       /* Memory retrieval */
    nimcp_pfc_t*               pfc;               /* Task control */

    /* State */
    bool dmn_dominant;
    float introspection_level;

    /* Memory infrastructure */
    nimcp_buffer_pool_t*       buffer_pool;
    nimcp_memory_pool_t        local_pool;

    /* Statistics */
    uint64_t dmn_activations;
    float total_wandering_time_ms;
} nimcp_pcc_t;

nimcp_status_t nimcp_pcc_init(
    nimcp_pcc_t* pcc,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_introspection_t* introspection,
    nimcp_self_model_t* self_model,
    nimcp_autobiographical_memory_t* autobio);

nimcp_status_t nimcp_pcc_update(
    nimcp_pcc_t* pcc,
    float task_demand,
    float delta_ms);

nimcp_status_t nimcp_pcc_get_spontaneous_thought(
    nimcp_pcc_t* pcc,
    float* thought,
    uint32_t thought_dim);

bool nimcp_pcc_is_dmn_active(const nimcp_pcc_t* pcc);

#endif /* NIMCP_PCC_H */
```

### 8.5 PCC Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/pcc/test_default_mode.cpp` | DMN activation, anti-correlation |
| Unit | `test/unit/core/brain/regions/pcc/test_self_reference.cpp` | Self-relevance, reflection |
| Unit | `test/unit/core/brain/regions/pcc/test_spontaneous_thought.cpp` | Generation, chaining |
| Integration | `test/integration/core/brain/regions/pcc/test_pcc_integration.cpp` | Full DMN |
| Regression | `test/regression/core/brain/regions/pcc/test_pcc_regression.cpp` | API, performance |

---

## Phase 9: Insula Enhancement

### 9.1 Overview

The insula enhancement adds **interoception, homeostatic integration, and salience detection** to connect body states with emotional awareness.

### 9.2 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Health Monitor | `nimcp_health_monitor.h` | Body state |
| Introspection | `nimcp_introspection.c` | Self-awareness |
| Emotional System | `nimcp_emotional_system.c` | Affect |

### 9.3 New Modules to Implement

#### 9.3.1 Interoceptive System
**File**: `include/core/brain/regions/insula/nimcp_interoception.h`

```c
/**
 * WHAT: Processing of internal body signals (heartbeat, respiration, etc.).
 * WHY:  Insula integrates visceral signals into conscious awareness.
 * HOW:  Body signal processing with accuracy and awareness tracking.
 */

typedef struct {
    /* Body signals */
    float heart_rate;
    float respiratory_rate;
    float temperature;
    float hunger_level;
    float thirst_level;
    float fatigue_level;
    float pain_level;

    /* Interoceptive accuracy */
    float accuracy;                         /* How well body signals detected */
    float awareness;                        /* Conscious access to signals */

    /* Integration with health monitor */
    nimcp_health_monitor_t* health_monitor;

    /* Prediction */
    float* predicted_body_state;
    float prediction_error;

    nimcp_memory_pool_t* pool;
} nimcp_interoception_t;

/**
 * Mathematical Model:
 *   Interoceptive accuracy = correlation(perceived, actual)
 *
 * Predictive interoception:
 *   prediction_error = actual - expected
 *   Body signals that violate predictions are salient
 *
 * Craig's model:
 *   Insula provides "sentient self" via body representation
 */

nimcp_status_t nimcp_interoception_init(
    nimcp_interoception_t* intero,
    nimcp_health_monitor_t* health,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_interoception_update(
    nimcp_interoception_t* intero);

nimcp_status_t nimcp_interoception_get_body_state(
    const nimcp_interoception_t* intero,
    float* body_state,
    uint32_t state_dim);

float nimcp_interoception_get_accuracy(
    const nimcp_interoception_t* intero);
```

#### 9.3.2 Homeostatic Drive System
**File**: `include/core/brain/regions/insula/nimcp_homeostatic_drives.h`

```c
/**
 * WHAT: Motivational drives arising from homeostatic imbalances.
 * WHY:  Insula translates body needs into motivational states.
 * HOW:  Drive strength proportional to deviation from setpoint.
 */

typedef struct {
    float setpoint;
    float current_value;
    float drive_strength;
    float sensitivity;                      /* How quickly drive increases */
} nimcp_drive_t;

typedef struct {
    /* Primary drives */
    nimcp_drive_t hunger;
    nimcp_drive_t thirst;
    nimcp_drive_t temperature;
    nimcp_drive_t rest;
    nimcp_drive_t social;

    /* Drive competition */
    uint32_t dominant_drive;
    float dominant_strength;

    /* Integration with interoception */
    nimcp_interoception_t* interoception;

    nimcp_memory_pool_t* pool;
} nimcp_homeostatic_drives_t;

/**
 * Mathematical Model:
 *   Drive(d) = k · |setpoint - current|^n
 *
 * Drive reduction:
 *   Behavior selected to minimize strongest drive
 *
 * Allostasis:
 *   Setpoints can shift based on anticipated demands
 */

nimcp_status_t nimcp_drives_init(
    nimcp_homeostatic_drives_t* drives,
    nimcp_interoception_t* intero,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_drives_update(
    nimcp_homeostatic_drives_t* drives);

uint32_t nimcp_drives_get_dominant(
    const nimcp_homeostatic_drives_t* drives);

float nimcp_drives_get_strength(
    const nimcp_homeostatic_drives_t* drives,
    uint32_t drive_id);

nimcp_status_t nimcp_drives_satisfy(
    nimcp_homeostatic_drives_t* drives,
    uint32_t drive_id,
    float amount);
```

#### 9.3.3 Salience Detection
**File**: `include/core/brain/regions/insula/nimcp_salience.h`

```c
/**
 * WHAT: Detection of behaviorally relevant stimuli for attention allocation.
 * WHY:  Anterior insula is hub of salience network.
 * HOW:  Integration of bottom-up salience with top-down relevance.
 */

typedef struct {
    /* Salience computation */
    float* stimulus_salience;
    uint32_t stimulus_count;

    /* Salience factors */
    float novelty_weight;
    float emotional_weight;
    float goal_relevance_weight;
    float interoceptive_weight;

    /* Threshold for attention */
    float attention_threshold;
    uint32_t attended_stimulus;

    /* Integration references */
    nimcp_interoception_t* interoception;
    nimcp_amygdala_t* amygdala;

    nimcp_memory_pool_t* pool;
} nimcp_salience_network_t;

/**
 * Mathematical Model:
 *   Salience(s) = w_n·novelty + w_e·emotion + w_g·goal + w_i·intero
 *
 * Attention allocation:
 *   Attend to max(salience) if > threshold
 *
 * Salience network function:
 *   Switch between DMN and task-positive based on salience
 */

nimcp_status_t nimcp_salience_init(
    nimcp_salience_network_t* sal,
    uint32_t max_stimuli,
    nimcp_interoception_t* intero,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_salience_evaluate(
    nimcp_salience_network_t* sal,
    const float* stimuli,
    uint32_t stimulus_count,
    float novelty,
    float emotional_value,
    float goal_relevance);

uint32_t nimcp_salience_get_attended(
    const nimcp_salience_network_t* sal);

float nimcp_salience_get_level(
    const nimcp_salience_network_t* sal,
    uint32_t stimulus_id);
```

### 9.4 Insula Main Module
**File**: `include/core/brain/regions/insula/nimcp_insula.h`

```c
/**
 * WHAT: Insula brain region - interoception and salience hub.
 * WHY:  Bridges body states, emotions, and attention allocation.
 * HOW:  Orchestrates interoception, drives, and salience with existing systems.
 */

#ifndef NIMCP_INSULA_H
#define NIMCP_INSULA_H

#include "nimcp_interoception.h"
#include "nimcp_homeostatic_drives.h"
#include "nimcp_salience.h"

typedef struct {
    /* New insula modules */
    nimcp_interoception_t      interoception;
    nimcp_homeostatic_drives_t drives;
    nimcp_salience_network_t   salience;

    /* References to orchestrated systems */
    nimcp_health_monitor_t*    health_monitor;    /* ORCHESTRATED */
    nimcp_introspection_t*     introspection;     /* ORCHESTRATED */
    nimcp_emotional_system_t*  emotions;          /* ORCHESTRATED */

    /* Connections */
    nimcp_amygdala_t*          amygdala;          /* Emotional salience */
    nimcp_pfc_t*               pfc;               /* Goal relevance */
    nimcp_pcc_t*               pcc;               /* DMN switching */

    /* State */
    float body_awareness;
    float current_salience;

    /* Memory infrastructure */
    nimcp_buffer_pool_t*       buffer_pool;
    nimcp_memory_pool_t        local_pool;

    /* Statistics */
    uint64_t salience_switches;
    float avg_interoceptive_accuracy;
} nimcp_insula_t;

nimcp_status_t nimcp_insula_init(
    nimcp_insula_t* insula,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_health_monitor_t* health,
    nimcp_introspection_t* introspection,
    nimcp_emotional_system_t* emotions);

nimcp_status_t nimcp_insula_update(
    nimcp_insula_t* insula,
    float delta_ms);

nimcp_status_t nimcp_insula_process_stimulus(
    nimcp_insula_t* insula,
    const float* stimulus,
    float novelty,
    float emotional_value,
    float goal_relevance);

float nimcp_insula_get_body_awareness(const nimcp_insula_t* insula);
uint32_t nimcp_insula_get_dominant_drive(const nimcp_insula_t* insula);

#endif /* NIMCP_INSULA_H */
```

### 9.5 Insula Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/insula/test_interoception.cpp` | Body signal processing |
| Unit | `test/unit/core/brain/regions/insula/test_homeostatic_drives.cpp` | Drive competition |
| Unit | `test/unit/core/brain/regions/insula/test_salience.cpp` | Salience computation |
| Integration | `test/integration/core/brain/regions/insula/test_insula_integration.cpp` | Full interoception |
| Regression | `test/regression/core/brain/regions/insula/test_insula_regression.cpp` | API, performance |

---

## Phase 10: Parietal Lobe Implementation ★ ENHANCED

### 10.1 Overview

The parietal lobe serves as the **spatial reasoning, mathematical cognition, and multimodal integration center**. This implementation is inspired by Einstein's brain findings (enlarged inferior parietal lobule, unusual sulcal patterns) and includes enhanced mathematical and scientific reasoning capabilities.

### 10.2 Anatomical Subdivisions

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PARIETAL LOBE ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │              SUPERIOR PARIETAL LOBULE (SPL)                          │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────┐ │    │
│  │  │ Spatial      │  │ Visuomotor   │  │ Body Schema                │ │    │
│  │  │ Orientation  │  │ Coordination │  │ (Proprioception)           │ │    │
│  │  └──────────────┘  └──────────────┘  └────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│  ┌─────────────────────────────────┴───────────────────────────────────┐    │
│  │              INTRAPARIETAL SULCUS (IPS)                              │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────┐ │    │
│  │  │ Numerical    │  │ Spatial      │  │ Eye Movement               │ │    │
│  │  │ Magnitude    │  │ Attention    │  │ Control                    │ │    │
│  │  │ (Number Line)│  │ (Priority)   │  │ (Saccades)                 │ │    │
│  │  └──────────────┘  └──────────────┘  └────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│  ┌─────────────────────────────────┴───────────────────────────────────┐    │
│  │              INFERIOR PARIETAL LOBULE (IPL) ★ Einstein Enhanced      │    │
│  │  ┌──────────────────────────┐  ┌──────────────────────────────────┐ │    │
│  │  │    SUPRAMARGINAL GYRUS   │  │       ANGULAR GYRUS              │ │    │
│  │  │  ┌────────────────────┐  │  │  ┌────────────────────────────┐  │ │    │
│  │  │  │ Tool Use           │  │  │  │ Mathematical Reasoning ★   │  │ │    │
│  │  │  │ Action Understanding│  │  │  │ Symbolic Manipulation ★    │  │ │    │
│  │  │  │ Phonological Loop  │  │  │  │ Analogical Mapping ★       │  │ │    │
│  │  │  └────────────────────┘  │  │  │ Scientific Inference ★     │  │ │    │
│  │  └──────────────────────────┘  │  │ Semantic Integration       │  │ │    │
│  │                                 │  │ Reading/Writing            │  │ │    │
│  │                                 │  └────────────────────────────┘  │ │    │
│  │                                 └──────────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│  ┌─────────────────────────────────┴───────────────────────────────────┐    │
│  │              SOMATOSENSORY CORTEX (S1/S2)                            │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────┐ │    │
│  │  │ Touch        │  │ Texture      │  │ Pain/Temperature           │ │    │
│  │  │ Processing   │  │ Recognition  │  │ Integration                │ │    │
│  │  └──────────────┘  └──────────────┘  └────────────────────────────┘ │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 10.3 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Multimodal Integration | `nimcp_multimodal_integration.c` | Sensory fusion |
| Attention | `nimcp_attention.c` | Spatial attention |
| Predictive | `nimcp_predictive.c` | Forward models |
| Working Memory | `nimcp_working_memory.c` | Buffer operations |
| Semantic Memory | `nimcp_semantic_memory.c` | Concept retrieval |

### 10.4 New Modules to Implement

#### 10.4.1 Numerical Cognition System (IPS)
**File**: `include/core/brain/regions/parietal/nimcp_numerical_cognition.h`

```c
/**
 * WHAT: Core numerical processing inspired by intraparietal sulcus.
 * WHY:  IPS encodes numerical magnitude on a mental number line.
 * HOW:  Approximate number system (ANS) with Weber-fraction scaling.
 */

typedef struct {
    /* Mental number line representation */
    float* number_line;                     /* Log-scaled magnitude */
    uint32_t resolution;                    /* Precision points */
    float weber_fraction;                   /* Discrimination threshold */

    /* Subitizing (instant small number recognition) */
    uint32_t subitizing_limit;              /* Typically 4 */
    float subitizing_speed_ms;

    /* Arithmetic operations */
    float addition_noise;                   /* Error scaling with magnitude */
    float multiplication_noise;

    /* Symbolic-nonsymbolic mapping */
    nimcp_cow_handle_t symbol_mapping;      /* Numeral → magnitude */
    float mapping_precision;

    nimcp_memory_pool_t* pool;
} nimcp_numerical_cognition_t;

/**
 * Mathematical Models:
 *
 * Weber's Law for number:
 *   discriminability = |n1 - n2| / max(n1, n2)
 *   JND (just noticeable difference) ∝ weber_fraction × magnitude
 *
 * Mental Number Line (Dehaene):
 *   representation(n) = log(n)  (compressed at larger values)
 *
 * Approximate Number System:
 *   perceived(n) = n + N(0, w×n)  where w = weber fraction
 *
 * Subitizing vs Counting:
 *   RT(n) = constant       if n ≤ 4 (subitizing)
 *   RT(n) = a + b×n        if n > 4 (counting)
 */

nimcp_status_t nimcp_numerical_init(
    nimcp_numerical_cognition_t* num,
    uint32_t resolution,
    float weber_fraction,
    nimcp_memory_pool_t* pool);

/* Magnitude operations */
nimcp_status_t nimcp_numerical_encode(
    nimcp_numerical_cognition_t* num,
    double value,
    float* magnitude_representation);

nimcp_status_t nimcp_numerical_compare(
    nimcp_numerical_cognition_t* num,
    double a, double b,
    float* confidence,
    float* reaction_time_ms);

/* Arithmetic */
nimcp_status_t nimcp_numerical_add(
    nimcp_numerical_cognition_t* num,
    double a, double b,
    double* result,
    float* confidence);

nimcp_status_t nimcp_numerical_multiply(
    nimcp_numerical_cognition_t* num,
    double a, double b,
    double* result,
    float* confidence);

/* Subitizing */
uint32_t nimcp_numerical_subitize(
    nimcp_numerical_cognition_t* num,
    const float* visual_input,
    float* confidence);
```

#### 10.4.2 Spatial Reasoning System (SPL/IPS)
**File**: `include/core/brain/regions/parietal/nimcp_spatial_reasoning.h`

```c
/**
 * WHAT: Spatial cognition including mental rotation, navigation, geometry.
 * WHY:  Superior parietal enables 3D spatial reasoning and transformations.
 * HOW:  Mental imagery with transformation operations.
 */

typedef struct {
    /* Spatial representation */
    float* spatial_buffer;                  /* 3D mental workspace */
    uint32_t buffer_resolution;             /* Voxel resolution */
    float buffer_decay_rate;

    /* Transformation state */
    float rotation_angle;
    float rotation_axis[3];
    float translation[3];
    float scale_factor;

    /* Mental rotation parameters (Shepard & Metzler) */
    float rotation_rate_deg_per_sec;        /* ~60°/sec typical */
    float rotation_difficulty;

    /* Coordinate frames */
    float egocentric_origin[3];             /* Self-centered */
    float allocentric_origin[3];            /* World-centered */
    bool use_egocentric;

    nimcp_memory_pool_t* pool;
} nimcp_spatial_reasoning_t;

/**
 * Mathematical Models:
 *
 * Mental Rotation (Shepard & Metzler, 1971):
 *   RT = a + b × angular_disparity
 *   (Linear increase with rotation angle)
 *
 * Spatial Transformations:
 *   Rotation: R(θ) = [[cos θ, -sin θ], [sin θ, cos θ]]
 *   3D: Quaternion representation for smooth interpolation
 *
 * Coordinate Transformation:
 *   allocentric = T × egocentric
 *   where T = transformation matrix
 *
 * Path Integration:
 *   position(t+1) = position(t) + velocity × dt + ½acceleration × dt²
 */

nimcp_status_t nimcp_spatial_init(
    nimcp_spatial_reasoning_t* spatial,
    uint32_t resolution,
    nimcp_memory_pool_t* pool);

/* Mental imagery */
nimcp_status_t nimcp_spatial_imagine(
    nimcp_spatial_reasoning_t* spatial,
    const float* object_description,
    uint32_t description_size);

/* Transformations */
nimcp_status_t nimcp_spatial_rotate(
    nimcp_spatial_reasoning_t* spatial,
    float angle_degrees,
    const float* axis);

nimcp_status_t nimcp_spatial_translate(
    nimcp_spatial_reasoning_t* spatial,
    const float* displacement);

/* Mental rotation task */
nimcp_status_t nimcp_spatial_compare_rotated(
    nimcp_spatial_reasoning_t* spatial,
    const float* object_a,
    const float* object_b,
    bool* same,
    float* reaction_time_ms);

/* Geometric reasoning */
nimcp_status_t nimcp_spatial_compute_distance(
    nimcp_spatial_reasoning_t* spatial,
    const float* point_a,
    const float* point_b,
    float* distance);

nimcp_status_t nimcp_spatial_compute_angle(
    nimcp_spatial_reasoning_t* spatial,
    const float* vertex,
    const float* point_a,
    const float* point_b,
    float* angle_radians);
```

#### 10.4.3 Symbolic Reasoning Engine (Angular Gyrus) ★ Enhanced
**File**: `include/core/brain/regions/parietal/nimcp_symbolic_engine.h`

```c
/**
 * WHAT: Formal symbolic manipulation for mathematics and logic.
 * WHY:  Angular gyrus supports abstract symbol processing (enhanced in Einstein).
 * HOW:  Term rewriting system with unification and proof search.
 */

typedef enum {
    NIMCP_SYMBOL_VARIABLE   = 0,
    NIMCP_SYMBOL_CONSTANT   = 1,
    NIMCP_SYMBOL_FUNCTION   = 2,
    NIMCP_SYMBOL_PREDICATE  = 3,
    NIMCP_SYMBOL_OPERATOR   = 4
} nimcp_symbol_type_t;

typedef struct nimcp_expression {
    nimcp_symbol_type_t type;
    uint32_t symbol_id;
    struct nimcp_expression** children;
    uint32_t child_count;
    float confidence;
} nimcp_expression_t;

typedef struct {
    /* Symbol table */
    nimcp_cow_handle_t symbols;
    uint32_t symbol_count;

    /* Rewrite rules */
    nimcp_cow_handle_t rules;               /* Pattern → replacement */
    uint32_t rule_count;

    /* Working expressions */
    nimcp_expression_t* workspace;
    uint32_t workspace_size;

    /* Proof state */
    nimcp_expression_t** proof_steps;
    uint32_t proof_length;
    bool proof_complete;

    /* Unification */
    nimcp_cow_handle_t substitutions;       /* Variable bindings */

    /* Cognitive limits */
    uint32_t max_depth;                     /* Nesting limit */
    uint32_t max_steps;                     /* Proof step limit */
    float timeout_ms;

    nimcp_memory_pool_t* pool;
} nimcp_symbolic_engine_t;

/**
 * Mathematical Models:
 *
 * Term Rewriting:
 *   Given rule l → r and term t[lσ]
 *   Result: t[rσ] where σ is substitution
 *
 * Unification Algorithm (Robinson):
 *   unify(f(s1...sn), f(t1...tn)) = compose(unify(s1,t1), ..., unify(sn,tn))
 *   unify(x, t) = {x → t} if x ∉ vars(t)
 *
 * Proof Search (Resolution):
 *   From clauses C1 ∨ L and C2 ∨ ¬L
 *   Derive: C1 ∨ C2 (resolvent)
 *
 * Cognitive Complexity:
 *   difficulty ∝ depth × branching_factor × rule_applications
 */

nimcp_status_t nimcp_symbolic_init(
    nimcp_symbolic_engine_t* engine,
    uint32_t max_symbols,
    nimcp_memory_pool_t* pool);

/* Expression building */
nimcp_status_t nimcp_symbolic_create_expr(
    nimcp_symbolic_engine_t* engine,
    nimcp_symbol_type_t type,
    uint32_t symbol_id,
    nimcp_expression_t** children,
    uint32_t child_count,
    nimcp_expression_t** result);

/* Manipulation */
nimcp_status_t nimcp_symbolic_substitute(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* expr,
    uint32_t var_id,
    nimcp_expression_t* replacement,
    nimcp_expression_t** result);

nimcp_status_t nimcp_symbolic_unify(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* expr1,
    nimcp_expression_t* expr2,
    bool* unifiable);

/* Rewriting */
nimcp_status_t nimcp_symbolic_add_rule(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* pattern,
    nimcp_expression_t* replacement);

nimcp_status_t nimcp_symbolic_rewrite(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* expr,
    nimcp_expression_t** result);

/* Proof */
nimcp_status_t nimcp_symbolic_prove(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* goal,
    nimcp_expression_t** axioms,
    uint32_t axiom_count,
    bool* proved);

/* Algebraic operations */
nimcp_status_t nimcp_symbolic_simplify(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* expr,
    nimcp_expression_t** simplified);

nimcp_status_t nimcp_symbolic_differentiate(
    nimcp_symbolic_engine_t* engine,
    nimcp_expression_t* expr,
    uint32_t var_id,
    nimcp_expression_t** derivative);
```

#### 10.4.4 Analogical Reasoning System ★ Enhanced
**File**: `include/core/brain/regions/parietal/nimcp_analogical_reasoning.h`

```c
/**
 * WHAT: Cross-domain analogical mapping for insight and transfer.
 * WHY:  Key to scientific creativity - mapping structure across domains.
 * HOW:  Structure Mapping Theory (Gentner) with progressive alignment.
 */

typedef struct {
    /* Domain elements */
    float* elements;
    uint32_t element_count;

    /* Relations between elements */
    nimcp_cow_handle_t relations;           /* (type, arg1, arg2, ...) */
    uint32_t relation_count;

    /* Higher-order relations */
    nimcp_cow_handle_t higher_order;        /* Relations between relations */
} nimcp_domain_t;

typedef struct {
    uint32_t source_element;
    uint32_t target_element;
    float mapping_strength;
} nimcp_mapping_t;

typedef struct {
    /* Source and target domains */
    nimcp_domain_t* source;
    nimcp_domain_t* target;

    /* Current mapping */
    nimcp_mapping_t* mappings;
    uint32_t mapping_count;

    /* Mapping quality */
    float structural_consistency;           /* 1-to-1 mapping */
    float relational_similarity;            /* Matching relations */
    float systematicity;                    /* Higher-order matches */

    /* Inference candidates */
    nimcp_expression_t** candidate_inferences;
    uint32_t inference_count;

    /* SMT parameters */
    float relation_weight;
    float attribute_weight;                 /* Usually low */
    float higher_order_weight;              /* Usually high */

    nimcp_memory_pool_t* pool;
} nimcp_analogical_engine_t;

/**
 * Mathematical Models (Structure Mapping Theory - Gentner):
 *
 * Structural Consistency:
 *   - 1-to-1 mapping: each element maps to at most one element
 *   - Parallel connectivity: if R(a,b) maps to R'(a',b'), then a→a' and b→b'
 *
 * Systematicity Principle:
 *   Prefer mappings that preserve higher-order relational structure
 *   Score = Σ(relation_matches) + k×Σ(higher_order_matches)
 *   where k > 1 (favor systematicity)
 *
 * Analogical Inference:
 *   If source has P(a) and a→a', infer P(a') in target
 *
 * Progressive Alignment:
 *   Start with local matches, extend to global structure
 *   alignment_score(t) = alignment_score(t-1) + new_matches
 */

nimcp_status_t nimcp_analogical_init(
    nimcp_analogical_engine_t* engine,
    nimcp_memory_pool_t* pool);

/* Domain construction */
nimcp_status_t nimcp_analogical_set_source(
    nimcp_analogical_engine_t* engine,
    nimcp_domain_t* source);

nimcp_status_t nimcp_analogical_set_target(
    nimcp_analogical_engine_t* engine,
    nimcp_domain_t* target);

/* Mapping */
nimcp_status_t nimcp_analogical_compute_mapping(
    nimcp_analogical_engine_t* engine);

nimcp_status_t nimcp_analogical_get_mapping(
    const nimcp_analogical_engine_t* engine,
    uint32_t source_element,
    uint32_t* target_element,
    float* confidence);

/* Inference */
nimcp_status_t nimcp_analogical_generate_inferences(
    nimcp_analogical_engine_t* engine);

nimcp_status_t nimcp_analogical_get_inference(
    const nimcp_analogical_engine_t* engine,
    uint32_t index,
    nimcp_expression_t** inference,
    float* confidence);

/* Quality metrics */
float nimcp_analogical_get_systematicity(
    const nimcp_analogical_engine_t* engine);

float nimcp_analogical_get_soundness(
    const nimcp_analogical_engine_t* engine);
```

#### 10.4.5 Scientific Inference System ★ Enhanced
**File**: `include/core/brain/regions/parietal/nimcp_scientific_inference.h`

```c
/**
 * WHAT: Hypothesis generation, experiment design, theory revision.
 * WHY:  Formalizes scientific method for systematic reasoning.
 * HOW:  Bayesian model comparison with information-theoretic experiment design.
 */

typedef struct {
    nimcp_expression_t* hypothesis;
    float prior_probability;
    float likelihood;
    float posterior_probability;
    float complexity;                       /* Occam factor */
} nimcp_hypothesis_t;

typedef struct {
    float* predicted_outcomes;
    uint32_t outcome_count;
    float information_gain;                 /* Expected KL divergence */
    float cost;
} nimcp_experiment_t;

typedef struct {
    /* Hypothesis space */
    nimcp_hypothesis_t* hypotheses;
    uint32_t hypothesis_count;
    uint32_t max_hypotheses;

    /* Evidence */
    nimcp_cow_handle_t observations;
    uint32_t observation_count;

    /* Experiment planning */
    nimcp_experiment_t* candidate_experiments;
    uint32_t experiment_count;

    /* Theory state */
    nimcp_expression_t* current_theory;
    float theory_confidence;
    uint32_t anomaly_count;

    /* Inference parameters */
    float occam_factor;                     /* Complexity penalty */
    float novelty_bonus;                    /* Reward surprising predictions */
    float confirmation_threshold;

    /* Connection to symbolic engine */
    nimcp_symbolic_engine_t* symbolic;

    nimcp_memory_pool_t* pool;
} nimcp_scientific_inference_t;

/**
 * Mathematical Models:
 *
 * Bayesian Inference:
 *   P(H|E) = P(E|H) × P(H) / P(E)
 *   posterior ∝ likelihood × prior
 *
 * Model Comparison (Bayes Factor):
 *   BF = P(E|H1) / P(E|H2)
 *   With Occam: BF_occam = BF × exp(-k×complexity_diff)
 *
 * Optimal Experiment Design (Information Gain):
 *   IG(experiment) = H(hypothesis) - E[H(hypothesis|outcome)]
 *   = Σ P(outcome) × KL(posterior || prior)
 *
 * Theory Revision (Kuhn/Lakatos):
 *   if anomaly_count > threshold:
 *       trigger_paradigm_shift()
 *   else:
 *       auxiliary_hypothesis_adjustment()
 *
 * Abduction (Inference to Best Explanation):
 *   Best_H = argmax_H [P(H|E) × explanatory_power(H)]
 */

nimcp_status_t nimcp_scientific_init(
    nimcp_scientific_inference_t* sci,
    nimcp_symbolic_engine_t* symbolic,
    nimcp_memory_pool_t* pool);

/* Hypothesis management */
nimcp_status_t nimcp_scientific_add_hypothesis(
    nimcp_scientific_inference_t* sci,
    nimcp_expression_t* hypothesis,
    float prior);

nimcp_status_t nimcp_scientific_generate_hypotheses(
    nimcp_scientific_inference_t* sci,
    nimcp_expression_t** observations,
    uint32_t obs_count);

/* Evidence integration */
nimcp_status_t nimcp_scientific_observe(
    nimcp_scientific_inference_t* sci,
    nimcp_expression_t* observation);

nimcp_status_t nimcp_scientific_update_posteriors(
    nimcp_scientific_inference_t* sci);

/* Experiment design */
nimcp_status_t nimcp_scientific_design_experiment(
    nimcp_scientific_inference_t* sci,
    nimcp_experiment_t** best_experiment);

float nimcp_scientific_compute_information_gain(
    nimcp_scientific_inference_t* sci,
    nimcp_experiment_t* experiment);

/* Theory operations */
nimcp_status_t nimcp_scientific_select_best_theory(
    nimcp_scientific_inference_t* sci,
    nimcp_expression_t** best);

nimcp_status_t nimcp_scientific_revise_theory(
    nimcp_scientific_inference_t* sci,
    nimcp_expression_t* anomaly);

bool nimcp_scientific_needs_paradigm_shift(
    const nimcp_scientific_inference_t* sci);
```

#### 10.4.6 Mental Simulation System ★ Enhanced
**File**: `include/core/brain/regions/parietal/nimcp_mental_simulation.h`

```c
/**
 * WHAT: Thought experiments and counterfactual reasoning.
 * WHY:  Einstein famously used gedankenexperiments (thought experiments).
 * HOW:  Forward model simulation with constraint satisfaction.
 */

typedef struct {
    nimcp_expression_t* condition;
    float probability;
} nimcp_scenario_t;

typedef struct {
    /* Simulation state */
    float* world_state;
    uint32_t state_dim;

    /* Simulation parameters */
    float time_step_ms;
    float max_simulation_time_ms;
    uint32_t max_iterations;

    /* Counterfactual machinery */
    nimcp_scenario_t* counterfactuals;
    uint32_t counterfactual_count;

    /* Constraint system */
    nimcp_expression_t** constraints;       /* Physical laws, logical rules */
    uint32_t constraint_count;

    /* Results */
    float** trajectory;                     /* State over time */
    uint32_t trajectory_length;
    bool simulation_valid;

    /* Connection to forward models (cerebellum) */
    nimcp_forward_model_t* forward_model;

    nimcp_memory_pool_t* pool;
} nimcp_mental_simulation_t;

/**
 * Mathematical Models:
 *
 * Forward Simulation:
 *   state(t+1) = f(state(t), action(t), constraints)
 *
 * Counterfactual Reasoning (Pearl):
 *   P(Y_x | X=x', Y=y) - what would Y be if X were x, given we observed x',y
 *
 * Structural Causal Model:
 *   Y = f(X, U) where U = exogenous noise
 *   Intervention: do(X=x) sets X regardless of causes
 *
 * Thought Experiment Validation:
 *   valid = ∀constraints: satisfied(trajectory)
 *   insight = novel_consequence(trajectory)
 *
 * Constraint Propagation:
 *   Iteratively enforce constraints until fixed point
 */

nimcp_status_t nimcp_simulation_init(
    nimcp_mental_simulation_t* sim,
    uint32_t state_dim,
    nimcp_forward_model_t* forward_model,
    nimcp_memory_pool_t* pool);

/* Setup */
nimcp_status_t nimcp_simulation_set_initial_state(
    nimcp_mental_simulation_t* sim,
    const float* state);

nimcp_status_t nimcp_simulation_add_constraint(
    nimcp_mental_simulation_t* sim,
    nimcp_expression_t* constraint);

/* Counterfactuals */
nimcp_status_t nimcp_simulation_add_counterfactual(
    nimcp_mental_simulation_t* sim,
    nimcp_expression_t* condition,
    float probability);

/* Execution */
nimcp_status_t nimcp_simulation_run(
    nimcp_mental_simulation_t* sim);

nimcp_status_t nimcp_simulation_run_counterfactual(
    nimcp_mental_simulation_t* sim,
    uint32_t counterfactual_index);

/* Results */
nimcp_status_t nimcp_simulation_get_outcome(
    const nimcp_mental_simulation_t* sim,
    float* final_state);

nimcp_status_t nimcp_simulation_get_trajectory(
    const nimcp_mental_simulation_t* sim,
    float** trajectory,
    uint32_t* length);

bool nimcp_simulation_is_valid(
    const nimcp_mental_simulation_t* sim);

/* Insight detection */
nimcp_status_t nimcp_simulation_find_insights(
    nimcp_mental_simulation_t* sim,
    nimcp_expression_t** insights,
    uint32_t* insight_count);
```

#### 10.4.7 Body Schema System (SPL/Somatosensory)
**File**: `include/core/brain/regions/parietal/nimcp_body_schema.h`

```c
/**
 * WHAT: Internal model of body configuration and capabilities.
 * WHY:  Parietal cortex maintains body representation for action planning.
 * HOW:  Kinematic model with proprioceptive integration.
 */

typedef struct {
    float position[3];
    float orientation[4];                   /* Quaternion */
    float joint_angles[32];                 /* Configurable joints */
    float joint_velocities[32];
} nimcp_body_state_t;

typedef struct {
    /* Body model */
    nimcp_body_state_t current_state;
    nimcp_body_state_t predicted_state;

    /* Limb parameters */
    float* limb_lengths;
    float* joint_limits;
    uint32_t joint_count;

    /* Peripersonal space */
    float reach_distance;
    float* reachable_space;                 /* 3D reachability map */

    /* Tool use extension */
    bool tool_incorporated;
    float tool_length;
    float tool_orientation[4];

    /* Integration with interoception */
    nimcp_interoception_t* interoception;

    nimcp_memory_pool_t* pool;
} nimcp_body_schema_t;

/**
 * Mathematical Models:
 *
 * Forward Kinematics:
 *   end_effector_pos = FK(joint_angles)
 *   Using DH parameters or product of exponentials
 *
 * Inverse Kinematics:
 *   joint_angles = IK(desired_pos, current_config)
 *   Jacobian pseudoinverse: Δθ = J†Δx
 *
 * Tool Incorporation (Maravita & Iriki):
 *   effective_reach = body_reach + tool_length
 *   Peripersonal space expands with tool use
 *
 * Body Ownership (Rubber Hand Illusion):
 *   ownership(limb) = f(visual_match, proprioceptive_match, timing)
 */

nimcp_status_t nimcp_body_schema_init(
    nimcp_body_schema_t* body,
    uint32_t joint_count,
    nimcp_interoception_t* intero,
    nimcp_memory_pool_t* pool);

/* State update */
nimcp_status_t nimcp_body_schema_update(
    nimcp_body_schema_t* body,
    const float* proprioceptive_input);

/* Queries */
nimcp_status_t nimcp_body_schema_get_end_effector(
    const nimcp_body_schema_t* body,
    uint32_t effector_id,
    float* position,
    float* orientation);

bool nimcp_body_schema_is_reachable(
    const nimcp_body_schema_t* body,
    const float* target_position);

/* Tool use */
nimcp_status_t nimcp_body_schema_incorporate_tool(
    nimcp_body_schema_t* body,
    float length,
    const float* orientation);

nimcp_status_t nimcp_body_schema_release_tool(
    nimcp_body_schema_t* body);
```

### 10.5 Parietal Lobe Main Module
**File**: `include/core/brain/regions/parietal/nimcp_parietal.h`

```c
/**
 * WHAT: Parietal lobe brain region - spatial, mathematical, scientific reasoning.
 * WHY:  Provides enhanced reasoning capabilities inspired by Einstein's brain.
 * HOW:  Orchestrates numerical, spatial, symbolic, analogical, and scientific modules.
 */

#ifndef NIMCP_PARIETAL_H
#define NIMCP_PARIETAL_H

#include "nimcp_numerical_cognition.h"
#include "nimcp_spatial_reasoning.h"
#include "nimcp_symbolic_engine.h"
#include "nimcp_analogical_reasoning.h"
#include "nimcp_scientific_inference.h"
#include "nimcp_mental_simulation.h"
#include "nimcp_body_schema.h"

typedef struct {
    /* New parietal-specific modules */
    nimcp_numerical_cognition_t  numerical;
    nimcp_spatial_reasoning_t    spatial;
    nimcp_symbolic_engine_t      symbolic;
    nimcp_analogical_engine_t    analogical;
    nimcp_scientific_inference_t scientific;
    nimcp_mental_simulation_t    simulation;
    nimcp_body_schema_t          body_schema;

    /* References to orchestrated systems */
    nimcp_multimodal_integration_t* multimodal;    /* ORCHESTRATED */
    nimcp_attention_t*              attention;      /* ORCHESTRATED */
    nimcp_working_memory_t*         working_memory; /* ORCHESTRATED */
    nimcp_semantic_memory_t*        semantic;       /* ORCHESTRATED */

    /* Connections to other regions */
    nimcp_pfc_t*                pfc;               /* Executive control */
    nimcp_hippocampus_t*        hippocampus;       /* Memory retrieval */
    nimcp_cerebellum_t*         cerebellum;        /* Forward models */
    nimcp_insula_t*             insula;            /* Body awareness */

    /* Einstein enhancements */
    bool enhanced_angular_gyrus;                   /* Extra symbolic capacity */
    float cross_modal_connectivity;                /* Inter-region coupling */

    /* Memory infrastructure */
    nimcp_buffer_pool_t*        buffer_pool;
    nimcp_memory_pool_t         local_pool;

    /* Statistics */
    uint64_t mathematical_operations;
    uint64_t analogies_computed;
    uint64_t thought_experiments;
    float avg_reasoning_depth;
} nimcp_parietal_t;

/* Lifecycle */
nimcp_status_t nimcp_parietal_init(
    nimcp_parietal_t* parietal,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_multimodal_integration_t* multimodal,
    nimcp_attention_t* attention,
    nimcp_working_memory_t* working_memory,
    nimcp_semantic_memory_t* semantic,
    nimcp_cerebellum_t* cerebellum);

nimcp_status_t nimcp_parietal_destroy(nimcp_parietal_t* parietal);

/* High-level reasoning */
nimcp_status_t nimcp_parietal_solve_mathematical(
    nimcp_parietal_t* parietal,
    nimcp_expression_t* problem,
    nimcp_expression_t** solution);

nimcp_status_t nimcp_parietal_find_analogy(
    nimcp_parietal_t* parietal,
    nimcp_domain_t* source,
    nimcp_domain_t* target,
    nimcp_mapping_t** mappings,
    uint32_t* mapping_count);

nimcp_status_t nimcp_parietal_run_thought_experiment(
    nimcp_parietal_t* parietal,
    const float* initial_conditions,
    nimcp_expression_t** constraints,
    uint32_t constraint_count,
    float** outcome);

nimcp_status_t nimcp_parietal_scientific_reason(
    nimcp_parietal_t* parietal,
    nimcp_expression_t** observations,
    uint32_t obs_count,
    nimcp_expression_t** best_hypothesis);

/* Spatial operations */
nimcp_status_t nimcp_parietal_mental_rotate(
    nimcp_parietal_t* parietal,
    const float* object,
    float angle,
    const float* axis,
    float* rotated);

/* Einstein mode (enhanced connectivity) */
nimcp_status_t nimcp_parietal_enable_einstein_mode(
    nimcp_parietal_t* parietal);

#endif /* NIMCP_PARIETAL_H */
```

### 10.6 Parietal Lobe Test Plan

| Test Type | File | Coverage |
|-----------|------|----------|
| Unit | `test/unit/core/brain/regions/parietal/test_numerical_cognition.cpp` | Weber's law, arithmetic |
| Unit | `test/unit/core/brain/regions/parietal/test_spatial_reasoning.cpp` | Mental rotation, geometry |
| Unit | `test/unit/core/brain/regions/parietal/test_symbolic_engine.cpp` | Unification, rewriting, proofs |
| Unit | `test/unit/core/brain/regions/parietal/test_analogical_reasoning.cpp` | Structure mapping, inference |
| Unit | `test/unit/core/brain/regions/parietal/test_scientific_inference.cpp` | Bayesian update, experiment design |
| Unit | `test/unit/core/brain/regions/parietal/test_mental_simulation.cpp` | Counterfactuals, constraints |
| Unit | `test/unit/core/brain/regions/parietal/test_body_schema.cpp` | Kinematics, tool use |
| Integration | `test/integration/core/brain/regions/parietal/test_parietal_integration.cpp` | Full reasoning pipeline |
| Regression | `test/regression/core/brain/regions/parietal/test_parietal_regression.cpp` | API, performance |

---

## Cross-Region Integration (Replaces Old Phase 4)

### Memory Pool Integration Points

All new brain region modules MUST use memory pool infrastructure:

```c
/* Pattern for all new modules */
typedef struct {
    /* Local memory pool for region-specific allocations */
    nimcp_memory_pool_t local_pool;

    /* Shared buffer pool reference for inter-region communication */
    nimcp_buffer_pool_t* shared_pool;

    /* COW handles for state that may be snapshotted */
    nimcp_cow_handle_t state_snapshot;
} nimcp_region_memory_t;

/* Initialization pattern */
nimcp_status_t region_init(region_t* region, nimcp_buffer_pool_t* shared) {
    /* Initialize local pool from shared pool's backing memory */
    CHECK(nimcp_memory_pool_init(&region->local_pool,
                                  REGION_POOL_SIZE,
                                  REGION_BLOCK_SIZE));
    region->shared_pool = shared;
    return NIMCP_SUCCESS;
}
```

### 4.2 Phase 2 Memory Pool Features to Implement

As part of brain region implementation, these Phase 2 memory features should be added:

1. **Synaptic Weight Pools**: Pre-allocated pools for plasticity updates
2. **Activation Buffer Pools**: For forward/inverse model computations
3. **State Snapshot COW**: For protective cutoff state preservation
4. **Inter-Region Message Pools**: For brainstem-cortex coupling

### 4.3 Cross-Region Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                    Integration Architecture                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  PFC ←──────── conflict ────────→ Attention System              │
│   │                                      ↑                       │
│   │←── control_demand ──┐               │                       │
│   │                     │               │                       │
│   ↓                     │               │                       │
│  Cerebellum ←── timing ──┼───────────────┘                       │
│   │                     │                                        │
│   │←── prediction_error ┘                                        │
│   │                                                              │
│   ↓                                                              │
│  Medulla ←── arousal_modulation ──→ All Systems                 │
│                                                                  │
│  Memory Pool + COW ←───── shared infrastructure ────→ All       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## File Structure

```
include/core/brain/regions/
├── medulla/
│   ├── nimcp_medulla.h              # Main module
│   ├── nimcp_arousal_state.h        # Arousal management
│   ├── nimcp_protective_cutoff.h    # Emergency protection
│   ├── nimcp_brainstem_coupling.h   # Inter-region signals
│   └── nimcp_circadian.h            # Time-of-day modulation
├── cerebellum/
│   ├── nimcp_cerebellum.h           # Main module
│   ├── nimcp_forward_model.h        # Predictive models
│   ├── nimcp_inverse_model.h        # Action computation
│   ├── nimcp_timing_circuit.h       # Precision timing
│   └── nimcp_granule_cell.h         # Sparse encoding
├── pfc/
│   ├── nimcp_pfc.h                  # Main module
│   ├── nimcp_acc.h                  # Conflict monitoring
│   ├── nimcp_dlpfc.h                # Working memory + control
│   ├── nimcp_vmpfc.h                # Value computation
│   └── nimcp_ofc.h                  # Reward integration
├── hippocampus/
│   ├── nimcp_hippocampus.h          # Main module
│   ├── nimcp_place_cells.h          # Spatial location encoding
│   ├── nimcp_grid_cells.h           # Path integration
│   ├── nimcp_dentate_gyrus.h        # Pattern separation
│   ├── nimcp_ca3.h                  # Pattern completion
│   └── nimcp_episodic_binding.h     # What/where/when binding
├── basal_ganglia/
│   ├── nimcp_basal_ganglia.h        # Main module
│   ├── nimcp_direct_pathway.h       # D1/Go signals
│   ├── nimcp_indirect_pathway.h     # D2/NoGo signals
│   └── nimcp_habit_formation.h      # Procedural memory
├── wernicke/
│   ├── nimcp_wernicke.h             # Main module
│   ├── nimcp_lexical_access.h       # Word recognition
│   ├── nimcp_semantic_integration.h # Meaning composition
│   └── nimcp_arcuate_fasciculus.h   # Broca connection
├── amygdala/
│   ├── nimcp_amygdala.h             # Main module
│   ├── nimcp_fear_conditioning.h    # CS-US learning
│   ├── nimcp_threat_detection.h     # Fast/slow pathways
│   └── nimcp_extinction.h           # Safety learning
├── pcc/
│   ├── nimcp_pcc.h                  # Main module
│   ├── nimcp_default_mode.h         # DMN controller
│   ├── nimcp_self_reference.h       # Self-relevant processing
│   └── nimcp_spontaneous_thought.h  # Mind-wandering
└── insula/
    ├── nimcp_insula.h               # Main module
    ├── nimcp_interoception.h        # Body signal processing
    ├── nimcp_homeostatic_drives.h   # Motivational drives
    └── nimcp_salience.h             # Attention allocation

src/core/brain/regions/
├── medulla/
│   ├── nimcp_medulla.c
│   ├── nimcp_arousal_state.c
│   ├── nimcp_protective_cutoff.c
│   ├── nimcp_brainstem_coupling.c
│   └── nimcp_circadian.c
├── cerebellum/
│   ├── nimcp_cerebellum.c
│   ├── nimcp_forward_model.c
│   ├── nimcp_inverse_model.c
│   ├── nimcp_timing_circuit.c
│   └── nimcp_granule_cell.c
└── pfc/
    ├── nimcp_pfc.c
    ├── nimcp_acc.c
    ├── nimcp_dlpfc.c
    ├── nimcp_vmpfc.c
    └── nimcp_ofc.c

test/
├── unit/core/brain/regions/
│   ├── medulla/
│   │   └── test_*.cpp (4 files)
│   ├── cerebellum/
│   │   └── test_*.cpp (4 files)
│   └── pfc/
│       └── test_*.cpp (4 files)
├── integration/core/brain/regions/
│   ├── medulla/
│   │   └── test_medulla_integration.cpp
│   ├── cerebellum/
│   │   └── test_cerebellum_integration.cpp
│   └── pfc/
│       └── test_pfc_integration.cpp
└── regression/core/brain/regions/
    ├── medulla/
    │   └── test_medulla_regression.cpp
    ├── cerebellum/
    │   └── test_cerebellum_regression.cpp
    └── pfc/
        └── test_pfc_regression.cpp
```

---

## Phase 11: Software Engineering Cortex ★ ENHANCED

### 11.1 Overview

The Software Engineering Cortex is a **specialized cognitive enhancement** for programming, debugging, and systems thinking. Like Einstein's enhanced parietal lobes enabled superior mathematical reasoning, this module "turbocharges" the brain's ability to reason about code, architectures, and complex software systems.

This implementation draws from cognitive science research on expert programmer cognition (Soloway & Ehrlich's "plans", Pennington's mental models, Détienne's schemas) and enhances existing NIMCP systems with software-specific processing.

### 11.2 Cognitive Architecture for Software Engineering

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    SOFTWARE ENGINEERING CORTEX ARCHITECTURE                      │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────────┐│
│  │                    CODE COMPREHENSION LAYER (Temporal/Parietal)              ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Lexical/Syntax  │  │ Semantic Model  │  │ Program Model               │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Tokenization│ │  │ │ Data Flow   │ │  │ │ Mental Simulation       │ │  ││
│  │  │ │ AST Parsing │ │  │ │ Control Flow│ │  │ │ State Tracking          │ │  ││
│  │  │ │ Pattern     │ │  │ │ Type System │ │  │ │ Execution Tracing       │ │  ││
│  │  │ │ Recognition │ │  │ │ Inference   │ │  │ │ Invariant Detection     │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    MENTAL DEBUGGER (PFC/Parietal) ★ Core Enhancement         ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Symbolic Exec   │  │ State Reasoning │  │ Hypothesis Engine           │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Path        │ │  │ │ Variable    │ │  │ │ Bug Localization        │ │  ││
│  │  │ │ Exploration │ │  │ │ Tracking    │ │  │ │ Root Cause Analysis     │ │  ││
│  │  │ │ Constraint  │ │  │ │ Call Stack  │ │  │ │ Fix Generation          │ │  ││
│  │  │ │ Solving     │ │  │ │ Simulation  │ │  │ │ Regression Prediction   │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    ARCHITECTURE REASONING (PFC) ★ Systems Thinking           ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Decomposition   │  │ Dependency      │  │ Quality Metrics             │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Module      │ │  │ │ Import      │ │  │ │ Coupling Analysis       │ │  ││
│  │  │ │ Boundaries  │ │  │ │ Graph       │ │  │ │ Cohesion Metrics        │ │  ││
│  │  │ │ Interface   │ │  │ │ Circular    │ │  │ │ Complexity Hotspots     │ │  ││
│  │  │ │ Detection   │ │  │ │ Detection   │ │  │ │ Technical Debt          │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    ALGORITHM DESIGNER (Parietal) ★ Optimization              ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Complexity      │  │ Algorithm       │  │ Trade-off Analysis          │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Big-O       │ │  │ │ Pattern     │ │  │ │ Time vs Space           │ │  ││
│  │  │ │ Analysis    │ │  │ │ Matching    │ │  │ │ Accuracy vs Speed       │ │  ││
│  │  │ │ Recurrence  │ │  │ │ Selection   │ │  │ │ Memory vs CPU           │ │  ││
│  │  │ │ Relations   │ │  │ │ Engine      │ │  │ │ Pareto Frontier         │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    CODE GENERATION (Broca's Analog) ★ Production             ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Template Engine │  │ Refactoring     │  │ Idiom Library               │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Design      │ │  │ │ Extract     │ │  │ │ Language Idioms         │ │  ││
│  │  │ │ Pattern     │ │  │ │ Rename      │ │  │ │ Framework Patterns      │ │  ││
│  │  │ │ Instantiate │ │  │ │ Inline      │ │  │ │ Best Practices          │ │  ││
│  │  │ │ Code        │ │  │ │ Move        │ │  │ │ Anti-Pattern Avoidance  │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    API MEMORY (Hippocampus) ★ Knowledge Base                 ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Semantic API    │  │ Usage Examples  │  │ Context Retrieval           │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Function    │ │  │ │ Code Snippet│ │  │ │ Similar Code Search     │ │  ││
│  │  │ │ Signatures  │ │  │ │ Repository  │ │  │ │ Stack Overflow Index    │ │  ││
│  │  │ │ Type Info   │ │  │ │ Best Usage  │ │  │ │ Documentation Links     │ │  ││
│  │  │ │ Deprecation │ │  │ │ Patterns    │ │  │ │ Version Compatibility   │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│  ┌────────────────────────────────────┴────────────────────────────────────────┐│
│  │                    BUG PREDICTION (Amygdala Analog) ★ Threat Detection       ││
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐  ││
│  │  │ Code Smell      │  │ Security Vuln   │  │ Risk Assessment             │  ││
│  │  │ ┌─────────────┐ │  │ ┌─────────────┐ │  │ ┌─────────────────────────┐ │  ││
│  │  │ │ Long Method │ │  │ │ Injection   │ │  │ │ Change Impact           │ │  ││
│  │  │ │ God Class   │ │  │ │ XSS/CSRF    │ │  │ │ Regression Likelihood   │ │  ││
│  │  │ │ Dead Code   │ │  │ │ Buffer      │ │  │ │ Test Coverage Gap       │ │  ││
│  │  │ │ Duplication │ │  │ │ Race Cond.  │ │  │ │ Maintenance Burden      │ │  ││
│  │  │ └─────────────┘ │  │ └─────────────┘ │  │ └─────────────────────────┘ │  ││
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 11.3 Existing Systems to ORCHESTRATE

| System | File | Functions to Call |
|--------|------|-------------------|
| Working Memory | `nimcp_working_memory.c` | Code context buffer |
| Semantic Memory | `nimcp_semantic_memory.c` | API/library knowledge |
| Pattern Recognition | `nimcp_pattern.c` | Design pattern detection |
| Symbolic Engine | `nimcp_symbolic_engine.c` | Type system reasoning |
| Analogical Reasoning | `nimcp_analogical_reasoning.c` | Code analogy |
| Hippocampus | `nimcp_hippocampus.c` | Code example retrieval |
| PFC Executive | `nimcp_pfc.c` | Planning, conflict resolution |
| Broca's Area | `nimcp_broca.c` | Code generation |

### 11.4 New Modules to Implement

#### 11.4.1 Code Comprehension System
**File**: `include/core/brain/regions/software_eng/nimcp_code_comprehension.h`

```c
/**
 * WHAT: Multi-level code understanding from syntax to semantics.
 * WHY:  Expert programmers build mental models at multiple abstraction levels.
 * HOW:  Pennington's dual-model theory: program model + situation model.
 */

/**
 * Cognitive Science Background:
 * - Program Model: Control flow, data flow, operations performed
 * - Situation Model: Real-world meaning, domain concepts
 * - Beacons: Recognizable code patterns that trigger understanding
 * - Plans: Stereotypical code structures (Soloway & Ehrlich)
 */

typedef enum {
    NIMCP_CODE_MODEL_SYNTACTIC,      /* Token/AST level */
    NIMCP_CODE_MODEL_SEMANTIC,       /* Type/data flow level */
    NIMCP_CODE_MODEL_PROGRAM,        /* Control flow/execution */
    NIMCP_CODE_MODEL_SITUATION       /* Domain meaning */
} nimcp_code_model_level_t;

typedef struct {
    /* AST representation */
    void* ast_root;
    uint32_t node_count;

    /* Control flow graph */
    void* cfg;
    uint32_t basic_block_count;
    uint32_t edge_count;

    /* Data flow analysis */
    void* dfg;
    uint32_t def_use_chains;

    /* Call graph */
    void* call_graph;
    uint32_t function_count;

    /* Pattern recognition state */
    nimcp_cow_handle_t recognized_patterns;
    uint32_t pattern_count;
    float pattern_confidence;

    /* Beacon detection (Soloway) */
    nimcp_cow_handle_t beacons;
    uint32_t beacon_count;

    nimcp_memory_pool_t* pool;
} nimcp_code_comprehension_t;

/**
 * Mathematical Models:
 *
 * Information Processing (Card, Moran, Newell):
 *   chunk_size = 7 ± 2 elements (Miller's law)
 *   comprehension_time = sum(chunk_process_time)
 *
 * Cyclomatic Complexity (McCabe):
 *   V(G) = E - N + 2P
 *   Where E = edges, N = nodes, P = connected components
 *
 * Cognitive Complexity (SonarSource):
 *   Increments for: nesting, breaks in flow, recursion
 *   Penalizes deeply nested structures more heavily
 *
 * Halstead Metrics:
 *   Vocabulary: η = η₁ + η₂
 *   Length: N = N₁ + N₂
 *   Volume: V = N × log₂(η)
 *   Difficulty: D = (η₁/2) × (N₂/η₂)
 *   Effort: E = D × V
 *
 * Plan Recognition (Soloway & Ehrlich):
 *   P(plan|code) ∝ P(code|plan) × P(plan)
 *   Bayesian inference over programming plans
 */

nimcp_status_t nimcp_code_comp_init(
    nimcp_code_comprehension_t* comp,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_code_comp_destroy(
    nimcp_code_comprehension_t* comp);

/* Parse and build internal representations */
nimcp_status_t nimcp_code_comp_analyze(
    nimcp_code_comprehension_t* comp,
    const char* source_code,
    const char* language);

/* Build specific models */
nimcp_status_t nimcp_code_comp_build_cfg(
    nimcp_code_comprehension_t* comp);

nimcp_status_t nimcp_code_comp_build_dfg(
    nimcp_code_comprehension_t* comp);

nimcp_status_t nimcp_code_comp_build_call_graph(
    nimcp_code_comprehension_t* comp);

/* Pattern detection */
nimcp_status_t nimcp_code_comp_detect_patterns(
    nimcp_code_comprehension_t* comp,
    nimcp_code_pattern_t** patterns,
    uint32_t* count);

nimcp_status_t nimcp_code_comp_detect_beacons(
    nimcp_code_comprehension_t* comp,
    nimcp_beacon_t** beacons,
    uint32_t* count);

/* Metrics */
nimcp_status_t nimcp_code_comp_cyclomatic_complexity(
    nimcp_code_comprehension_t* comp,
    const char* function_name,
    uint32_t* complexity);

nimcp_status_t nimcp_code_comp_cognitive_complexity(
    nimcp_code_comprehension_t* comp,
    const char* function_name,
    uint32_t* complexity);

nimcp_status_t nimcp_code_comp_halstead_metrics(
    nimcp_code_comprehension_t* comp,
    nimcp_halstead_metrics_t* metrics);

/* Understanding queries */
nimcp_status_t nimcp_code_comp_trace_data_flow(
    nimcp_code_comprehension_t* comp,
    const char* variable,
    nimcp_data_flow_path_t** paths,
    uint32_t* path_count);

nimcp_status_t nimcp_code_comp_find_dependencies(
    nimcp_code_comprehension_t* comp,
    const char* function_name,
    nimcp_dependency_t** deps,
    uint32_t* dep_count);
```

#### 11.4.2 Mental Debugger System
**File**: `include/core/brain/regions/software_eng/nimcp_mental_debugger.h`

```c
/**
 * WHAT: Cognitive debugging through mental execution and state reasoning.
 * WHY:  Expert debuggers simulate execution mentally, form hypotheses.
 * HOW:  Symbolic execution with constraint tracking and hypothesis generation.
 */

/**
 * Cognitive Science Background:
 * - Expert debuggers use "slicing" to focus on relevant code
 * - Bug hypotheses follow spectrum: data → control → algorithm → design
 * - "Debugging is twice as hard as writing code" (Kernighan)
 */

typedef enum {
    NIMCP_BUG_HYPOTHESIS_DATA,       /* Wrong value, type error */
    NIMCP_BUG_HYPOTHESIS_CONTROL,    /* Wrong branch, loop error */
    NIMCP_BUG_HYPOTHESIS_INTERFACE,  /* API misuse, contract violation */
    NIMCP_BUG_HYPOTHESIS_CONCURRENCY,/* Race condition, deadlock */
    NIMCP_BUG_HYPOTHESIS_RESOURCE,   /* Memory leak, handle leak */
    NIMCP_BUG_HYPOTHESIS_ALGORITHM,  /* Logic error, edge case */
    NIMCP_BUG_HYPOTHESIS_DESIGN      /* Architectural flaw */
} nimcp_bug_hypothesis_type_t;

typedef struct {
    nimcp_bug_hypothesis_type_t type;
    char* description;
    char* location;                   /* File:line */
    float confidence;
    char** evidence;                  /* Supporting observations */
    uint32_t evidence_count;
    char** suggested_fixes;
    uint32_t fix_count;
} nimcp_bug_hypothesis_t;

typedef struct {
    /* Symbolic execution state */
    void* symbolic_state;
    uint32_t path_count;
    uint32_t constraint_count;

    /* Variable tracking */
    nimcp_cow_handle_t variable_history;
    uint32_t tracked_variables;

    /* Call stack simulation */
    nimcp_cow_handle_t call_stack;
    uint32_t stack_depth;
    uint32_t max_stack_depth;

    /* Hypothesis management */
    nimcp_bug_hypothesis_t* hypotheses;
    uint32_t hypothesis_count;
    uint32_t hypothesis_capacity;

    /* Breakpoint simulation */
    void* breakpoints;
    uint32_t breakpoint_count;

    /* Backward slice for focusing */
    void* backward_slice;
    uint32_t slice_size;

    nimcp_memory_pool_t* pool;
} nimcp_mental_debugger_t;

/**
 * Mathematical Models:
 *
 * Program Slicing (Weiser):
 *   Slice(P, V, n) = minimal subset affecting V at n
 *   Backward slice: what affects this point?
 *   Forward slice: what does this point affect?
 *
 * Fault Localization (Spectrum-Based):
 *   Suspiciousness(s) = failed(s) / (failed(s) + passed(s))
 *   Tarantula, Ochiai, Jaccard coefficients
 *
 * Delta Debugging:
 *   Minimize failing input: ddmin algorithm
 *   Binary search over change space
 *
 * Symbolic Execution:
 *   State = (PC, PathCondition, SymbolicStore)
 *   Branch: fork state with updated path conditions
 *   Constraint solving: SMT solver for feasibility
 *
 * Bayesian Bug Localization:
 *   P(location|symptoms) ∝ P(symptoms|location) × P(location)
 *   Prior from: code complexity, change history, author
 */

nimcp_status_t nimcp_mental_debug_init(
    nimcp_mental_debugger_t* debugger,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_mental_debug_destroy(
    nimcp_mental_debugger_t* debugger);

/* Load code for debugging */
nimcp_status_t nimcp_mental_debug_load(
    nimcp_mental_debugger_t* debugger,
    nimcp_code_comprehension_t* code);

/* Symbolic execution */
nimcp_status_t nimcp_mental_debug_symbolic_exec(
    nimcp_mental_debugger_t* debugger,
    const char* entry_function,
    uint32_t max_paths);

nimcp_status_t nimcp_mental_debug_step(
    nimcp_mental_debugger_t* debugger);

nimcp_status_t nimcp_mental_debug_step_over(
    nimcp_mental_debugger_t* debugger);

nimcp_status_t nimcp_mental_debug_step_into(
    nimcp_mental_debugger_t* debugger);

/* State inspection */
nimcp_status_t nimcp_mental_debug_get_variable(
    nimcp_mental_debugger_t* debugger,
    const char* variable_name,
    nimcp_symbolic_value_t* value);

nimcp_status_t nimcp_mental_debug_trace_variable(
    nimcp_mental_debugger_t* debugger,
    const char* variable_name,
    nimcp_value_history_t** history);

nimcp_status_t nimcp_mental_debug_get_call_stack(
    nimcp_mental_debugger_t* debugger,
    nimcp_stack_frame_t** frames,
    uint32_t* frame_count);

/* Hypothesis generation */
nimcp_status_t nimcp_mental_debug_generate_hypotheses(
    nimcp_mental_debugger_t* debugger,
    const char* symptom_description,
    nimcp_bug_hypothesis_t** hypotheses,
    uint32_t* count);

nimcp_status_t nimcp_mental_debug_rank_hypotheses(
    nimcp_mental_debugger_t* debugger);

nimcp_status_t nimcp_mental_debug_test_hypothesis(
    nimcp_mental_debugger_t* debugger,
    nimcp_bug_hypothesis_t* hypothesis,
    float* likelihood);

/* Fault localization */
nimcp_status_t nimcp_mental_debug_compute_backward_slice(
    nimcp_mental_debugger_t* debugger,
    const char* variable,
    uint32_t line_number);

nimcp_status_t nimcp_mental_debug_fault_localize(
    nimcp_mental_debugger_t* debugger,
    nimcp_test_result_t* test_results,
    uint32_t test_count,
    nimcp_suspiciousness_t** rankings);

/* Fix suggestion */
nimcp_status_t nimcp_mental_debug_suggest_fixes(
    nimcp_mental_debugger_t* debugger,
    nimcp_bug_hypothesis_t* hypothesis,
    nimcp_code_fix_t** fixes,
    uint32_t* fix_count);
```

#### 11.4.3 Architecture Reasoning System
**File**: `include/core/brain/regions/software_eng/nimcp_architecture_reasoning.h`

```c
/**
 * WHAT: System-level reasoning about software architecture.
 * WHY:  Architecture decisions have long-term impact; need principled analysis.
 * HOW:  Graph-based analysis with coupling/cohesion metrics.
 */

/**
 * Design Principles Encoded:
 * - SOLID: Single responsibility, Open/closed, Liskov, Interface seg, DI
 * - DRY: Don't Repeat Yourself
 * - KISS: Keep It Simple, Stupid
 * - YAGNI: You Aren't Gonna Need It
 * - Law of Demeter: Only talk to immediate friends
 */

typedef enum {
    NIMCP_ARCH_PATTERN_LAYERED,
    NIMCP_ARCH_PATTERN_MICROSERVICES,
    NIMCP_ARCH_PATTERN_EVENT_DRIVEN,
    NIMCP_ARCH_PATTERN_HEXAGONAL,
    NIMCP_ARCH_PATTERN_PIPE_FILTER,
    NIMCP_ARCH_PATTERN_MVC,
    NIMCP_ARCH_PATTERN_CQRS,
    NIMCP_ARCH_PATTERN_MONOLITH
} nimcp_arch_pattern_t;

typedef enum {
    NIMCP_COUPLING_CONTENT,          /* Direct access to internals (worst) */
    NIMCP_COUPLING_COMMON,           /* Shared global data */
    NIMCP_COUPLING_CONTROL,          /* Passing control flags */
    NIMCP_COUPLING_STAMP,            /* Passing composite structures */
    NIMCP_COUPLING_DATA,             /* Passing simple data (best) */
    NIMCP_COUPLING_MESSAGE           /* Async message passing */
} nimcp_coupling_type_t;

typedef struct {
    char* module_a;
    char* module_b;
    nimcp_coupling_type_t type;
    float strength;                   /* 0.0 = loose, 1.0 = tight */
    uint32_t dependency_count;
} nimcp_coupling_info_t;

typedef struct {
    char* module_name;
    float functional_cohesion;        /* 0.0 = coincidental, 1.0 = functional */
    float sequential_cohesion;
    float communicational_cohesion;
    float overall_cohesion;
    char** responsibilities;
    uint32_t responsibility_count;
} nimcp_cohesion_info_t;

typedef struct {
    /* Module graph */
    void* module_graph;
    uint32_t module_count;
    uint32_t dependency_count;

    /* Detected patterns */
    nimcp_arch_pattern_t detected_pattern;
    float pattern_confidence;

    /* Metrics */
    nimcp_coupling_info_t* coupling_matrix;
    nimcp_cohesion_info_t* cohesion_info;
    float instability_index;          /* Ce / (Ca + Ce) */
    float abstractness;               /* Abstract / Total */
    float distance_from_main_seq;     /* |A + I - 1| */

    /* Circular dependencies */
    char*** cycles;
    uint32_t cycle_count;

    /* Layer violations */
    char** layer_violations;
    uint32_t violation_count;

    /* Technical debt indicators */
    float technical_debt_hours;
    float maintainability_index;

    nimcp_memory_pool_t* pool;
} nimcp_architecture_reasoning_t;

/**
 * Mathematical Models:
 *
 * Coupling Metrics:
 *   Afferent Coupling (Ca) = incoming dependencies
 *   Efferent Coupling (Ce) = outgoing dependencies
 *   Instability I = Ce / (Ca + Ce)
 *
 * Cohesion (LCOM - Lack of Cohesion of Methods):
 *   LCOM = |P| - |Q| where P = method pairs sharing no attributes
 *                         Q = method pairs sharing attributes
 *   LCOM4 = number of connected components in method-attribute graph
 *
 * Martin Metrics (Clean Architecture):
 *   Abstractness A = Na / Nc (abstract classes / total classes)
 *   Distance D = |A + I - 1|  (distance from main sequence)
 *
 * Maintainability Index:
 *   MI = 171 - 5.2×ln(V) - 0.23×G - 16.2×ln(LOC) + 50×sin(√(2.4×CM))
 *   Where V=Halstead volume, G=cyclomatic, CM=comment ratio
 *
 * Technical Debt (SQALE):
 *   debt_ratio = remediation_cost / development_cost
 *   Ratings: A(<5%), B(5-10%), C(10-20%), D(20-50%), E(>50%)
 *
 * Graph Analysis:
 *   PageRank for module importance
 *   Tarjan's algorithm for cycle detection
 *   Betweenness centrality for architectural bottlenecks
 */

nimcp_status_t nimcp_arch_init(
    nimcp_architecture_reasoning_t* arch,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_arch_destroy(
    nimcp_architecture_reasoning_t* arch);

/* Analysis */
nimcp_status_t nimcp_arch_analyze_codebase(
    nimcp_architecture_reasoning_t* arch,
    const char* root_directory);

nimcp_status_t nimcp_arch_build_module_graph(
    nimcp_architecture_reasoning_t* arch);

nimcp_status_t nimcp_arch_detect_pattern(
    nimcp_architecture_reasoning_t* arch,
    nimcp_arch_pattern_t* pattern,
    float* confidence);

/* Metrics */
nimcp_status_t nimcp_arch_compute_coupling(
    nimcp_architecture_reasoning_t* arch,
    const char* module_a,
    const char* module_b,
    nimcp_coupling_info_t* info);

nimcp_status_t nimcp_arch_compute_cohesion(
    nimcp_architecture_reasoning_t* arch,
    const char* module_name,
    nimcp_cohesion_info_t* info);

nimcp_status_t nimcp_arch_compute_instability(
    nimcp_architecture_reasoning_t* arch,
    const char* module_name,
    float* instability);

nimcp_status_t nimcp_arch_maintainability_index(
    nimcp_architecture_reasoning_t* arch,
    float* index);

/* Problem detection */
nimcp_status_t nimcp_arch_detect_cycles(
    nimcp_architecture_reasoning_t* arch,
    char**** cycles,
    uint32_t* cycle_count);

nimcp_status_t nimcp_arch_detect_layer_violations(
    nimcp_architecture_reasoning_t* arch,
    nimcp_layer_config_t* layers,
    char*** violations,
    uint32_t* violation_count);

nimcp_status_t nimcp_arch_find_god_classes(
    nimcp_architecture_reasoning_t* arch,
    char*** god_classes,
    uint32_t* count);

nimcp_status_t nimcp_arch_estimate_technical_debt(
    nimcp_architecture_reasoning_t* arch,
    float* debt_hours);

/* Recommendations */
nimcp_status_t nimcp_arch_suggest_decomposition(
    nimcp_architecture_reasoning_t* arch,
    const char* module_name,
    nimcp_decomposition_suggestion_t** suggestions,
    uint32_t* count);

nimcp_status_t nimcp_arch_suggest_refactoring(
    nimcp_architecture_reasoning_t* arch,
    nimcp_refactoring_suggestion_t** suggestions,
    uint32_t* count);
```

#### 11.4.4 Algorithm Designer System
**File**: `include/core/brain/regions/software_eng/nimcp_algorithm_designer.h`

```c
/**
 * WHAT: Algorithm selection, complexity analysis, and optimization.
 * WHY:  Choosing right algorithm is critical for performance.
 * HOW:  Pattern matching against problem characteristics + complexity analysis.
 */

typedef enum {
    NIMCP_COMPLEXITY_O_1,            /* Constant */
    NIMCP_COMPLEXITY_O_LOG_N,        /* Logarithmic */
    NIMCP_COMPLEXITY_O_N,            /* Linear */
    NIMCP_COMPLEXITY_O_N_LOG_N,      /* Linearithmic */
    NIMCP_COMPLEXITY_O_N_SQUARED,    /* Quadratic */
    NIMCP_COMPLEXITY_O_N_CUBED,      /* Cubic */
    NIMCP_COMPLEXITY_O_2_N,          /* Exponential */
    NIMCP_COMPLEXITY_O_N_FACTORIAL   /* Factorial */
} nimcp_complexity_class_t;

typedef struct {
    nimcp_complexity_class_t time_best;
    nimcp_complexity_class_t time_average;
    nimcp_complexity_class_t time_worst;
    nimcp_complexity_class_t space;
    bool in_place;
    bool stable;                      /* For sorting */
    bool online;                      /* Can process streaming */
    bool parallelizable;
    float constant_factor;            /* Hidden constant in O() */
} nimcp_algorithm_characteristics_t;

typedef enum {
    NIMCP_PROBLEM_SORTING,
    NIMCP_PROBLEM_SEARCHING,
    NIMCP_PROBLEM_GRAPH_TRAVERSAL,
    NIMCP_PROBLEM_SHORTEST_PATH,
    NIMCP_PROBLEM_MINIMUM_SPANNING_TREE,
    NIMCP_PROBLEM_DYNAMIC_PROGRAMMING,
    NIMCP_PROBLEM_STRING_MATCHING,
    NIMCP_PROBLEM_NUMERICAL,
    NIMCP_PROBLEM_OPTIMIZATION,
    NIMCP_PROBLEM_COMPRESSION,
    NIMCP_PROBLEM_CRYPTOGRAPHY
} nimcp_problem_class_t;

typedef struct {
    char* name;
    nimcp_problem_class_t problem_class;
    nimcp_algorithm_characteristics_t characteristics;
    char* description;
    char* pseudocode;
    char** applicable_when;           /* Problem characteristics */
    uint32_t applicable_count;
    char** avoid_when;
    uint32_t avoid_count;
} nimcp_algorithm_entry_t;

typedef struct {
    /* Algorithm knowledge base */
    nimcp_algorithm_entry_t* algorithms;
    uint32_t algorithm_count;

    /* Complexity analysis state */
    void* analysis_context;

    /* Recurrence solver */
    void* recurrence_solver;

    /* Trade-off analysis */
    float time_weight;
    float space_weight;
    float simplicity_weight;

    nimcp_memory_pool_t* pool;
} nimcp_algorithm_designer_t;

/**
 * Mathematical Models:
 *
 * Complexity Analysis:
 *   Big-O: upper bound (worst case)
 *   Big-Ω: lower bound (best case)
 *   Big-Θ: tight bound (average case)
 *
 * Master Theorem (Recurrences):
 *   T(n) = aT(n/b) + f(n)
 *   Case 1: f(n) = O(n^(log_b(a)-ε)) → T(n) = Θ(n^log_b(a))
 *   Case 2: f(n) = Θ(n^log_b(a)) → T(n) = Θ(n^log_b(a) × log n)
 *   Case 3: f(n) = Ω(n^(log_b(a)+ε)) → T(n) = Θ(f(n))
 *
 * Amortized Analysis:
 *   Aggregate: Total cost / n operations
 *   Accounting: Assign different costs, maintain credit
 *   Potential: Φ(state) tracks "stored work"
 *
 * Algorithm Selection (Multi-Criteria):
 *   Score = w_t × time_score + w_s × space_score + w_c × complexity_score
 *   Pareto frontier for incomparable trade-offs
 *
 * Problem Reduction:
 *   A ≤_p B means A reduces to B in polynomial time
 *   NP-hardness proofs via reduction chains
 */

nimcp_status_t nimcp_algo_init(
    nimcp_algorithm_designer_t* algo,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_algo_destroy(
    nimcp_algorithm_designer_t* algo);

/* Complexity analysis */
nimcp_status_t nimcp_algo_analyze_complexity(
    nimcp_algorithm_designer_t* algo,
    nimcp_code_comprehension_t* code,
    const char* function_name,
    nimcp_complexity_class_t* time_complexity,
    nimcp_complexity_class_t* space_complexity);

nimcp_status_t nimcp_algo_solve_recurrence(
    nimcp_algorithm_designer_t* algo,
    const char* recurrence_str,
    nimcp_complexity_class_t* solution);

/* Algorithm selection */
nimcp_status_t nimcp_algo_classify_problem(
    nimcp_algorithm_designer_t* algo,
    const char* problem_description,
    nimcp_problem_class_t* classification,
    float* confidence);

nimcp_status_t nimcp_algo_recommend(
    nimcp_algorithm_designer_t* algo,
    nimcp_problem_class_t problem,
    nimcp_problem_constraints_t* constraints,
    nimcp_algorithm_entry_t** recommendations,
    uint32_t* count);

/* Trade-off analysis */
nimcp_status_t nimcp_algo_compare_algorithms(
    nimcp_algorithm_designer_t* algo,
    nimcp_algorithm_entry_t* algo_a,
    nimcp_algorithm_entry_t* algo_b,
    nimcp_comparison_result_t* result);

nimcp_status_t nimcp_algo_pareto_frontier(
    nimcp_algorithm_designer_t* algo,
    nimcp_algorithm_entry_t** candidates,
    uint32_t candidate_count,
    nimcp_algorithm_entry_t** pareto_optimal,
    uint32_t* optimal_count);

/* Optimization suggestions */
nimcp_status_t nimcp_algo_suggest_optimizations(
    nimcp_algorithm_designer_t* algo,
    nimcp_code_comprehension_t* code,
    const char* function_name,
    nimcp_optimization_t** optimizations,
    uint32_t* count);
```

#### 11.4.5 Code Generation System
**File**: `include/core/brain/regions/software_eng/nimcp_code_generation.h`

```c
/**
 * WHAT: Structured code generation and refactoring.
 * WHY:  Correct-by-construction beats fix-after-the-fact.
 * HOW:  Template instantiation + transformation rules.
 */

typedef enum {
    NIMCP_PATTERN_SINGLETON,
    NIMCP_PATTERN_FACTORY,
    NIMCP_PATTERN_BUILDER,
    NIMCP_PATTERN_ADAPTER,
    NIMCP_PATTERN_DECORATOR,
    NIMCP_PATTERN_OBSERVER,
    NIMCP_PATTERN_STRATEGY,
    NIMCP_PATTERN_COMMAND,
    NIMCP_PATTERN_STATE,
    NIMCP_PATTERN_VISITOR,
    NIMCP_PATTERN_ITERATOR,
    NIMCP_PATTERN_COMPOSITE,
    NIMCP_PATTERN_FACADE,
    NIMCP_PATTERN_PROXY,
    NIMCP_PATTERN_TEMPLATE_METHOD,
    NIMCP_PATTERN_CHAIN_OF_RESPONSIBILITY
} nimcp_design_pattern_t;

typedef enum {
    NIMCP_REFACTOR_EXTRACT_METHOD,
    NIMCP_REFACTOR_EXTRACT_CLASS,
    NIMCP_REFACTOR_INLINE_METHOD,
    NIMCP_REFACTOR_RENAME,
    NIMCP_REFACTOR_MOVE_METHOD,
    NIMCP_REFACTOR_PULL_UP,
    NIMCP_REFACTOR_PUSH_DOWN,
    NIMCP_REFACTOR_REPLACE_CONDITIONAL_WITH_POLYMORPHISM,
    NIMCP_REFACTOR_INTRODUCE_PARAMETER_OBJECT,
    NIMCP_REFACTOR_DECOMPOSE_CONDITIONAL,
    NIMCP_REFACTOR_CONSOLIDATE_DUPLICATE,
    NIMCP_REFACTOR_REPLACE_TEMP_WITH_QUERY,
    NIMCP_REFACTOR_INTRODUCE_NULL_OBJECT
} nimcp_refactoring_type_t;

typedef struct {
    nimcp_refactoring_type_t type;
    char* target;                     /* What to refactor */
    char* new_name;                   /* For rename */
    char* destination;                /* For move */
    char** selected_members;          /* For extract */
    uint32_t member_count;
    bool preview_only;
} nimcp_refactoring_request_t;

typedef struct {
    /* Template library */
    void* pattern_templates;
    uint32_t template_count;

    /* Refactoring engine */
    void* refactoring_engine;

    /* Idiom library */
    nimcp_cow_handle_t idioms;
    uint32_t idiom_count;

    /* AST manipulation */
    void* ast_transformer;

    /* Code style */
    nimcp_code_style_t style;

    nimcp_memory_pool_t* pool;
} nimcp_code_generation_t;

/**
 * Mathematical Models:
 *
 * Program Transformation:
 *   Semantics preservation: [[P]] = [[T(P)]]
 *   Refactoring = behavior-preserving transformation
 *
 * Pattern Language (Alexander):
 *   Problem → Forces → Solution → Resulting Context
 *   Patterns form a generative grammar for code
 *
 * Template Instantiation:
 *   Template<T> + bindings → Concrete code
 *   Type inference: Hindley-Milner algorithm
 *
 * Edit Distance for Code:
 *   Tree edit distance on AST
 *   Minimal transformation sequence
 */

nimcp_status_t nimcp_codegen_init(
    nimcp_code_generation_t* gen,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_codegen_destroy(
    nimcp_code_generation_t* gen);

/* Pattern instantiation */
nimcp_status_t nimcp_codegen_instantiate_pattern(
    nimcp_code_generation_t* gen,
    nimcp_design_pattern_t pattern,
    nimcp_pattern_params_t* params,
    char** generated_code);

nimcp_status_t nimcp_codegen_detect_pattern_opportunity(
    nimcp_code_generation_t* gen,
    nimcp_code_comprehension_t* code,
    nimcp_pattern_opportunity_t** opportunities,
    uint32_t* count);

/* Refactoring */
nimcp_status_t nimcp_codegen_refactor(
    nimcp_code_generation_t* gen,
    nimcp_code_comprehension_t* code,
    nimcp_refactoring_request_t* request,
    nimcp_code_diff_t** diff);

nimcp_status_t nimcp_codegen_preview_refactoring(
    nimcp_code_generation_t* gen,
    nimcp_code_comprehension_t* code,
    nimcp_refactoring_request_t* request,
    nimcp_refactoring_preview_t** preview);

/* Idiom application */
nimcp_status_t nimcp_codegen_apply_idiom(
    nimcp_code_generation_t* gen,
    const char* idiom_name,
    nimcp_idiom_params_t* params,
    char** generated_code);

nimcp_status_t nimcp_codegen_detect_idiom_violations(
    nimcp_code_generation_t* gen,
    nimcp_code_comprehension_t* code,
    nimcp_idiom_violation_t** violations,
    uint32_t* count);
```

#### 11.4.6 API Memory System
**File**: `include/core/brain/regions/software_eng/nimcp_api_memory.h`

```c
/**
 * WHAT: Semantic memory for APIs, libraries, and frameworks.
 * WHY:  Modern development requires vast API knowledge.
 * HOW:  Structured knowledge graph + retrieval-augmented generation.
 */

typedef struct {
    char* name;
    char* signature;
    char* return_type;
    char** parameter_names;
    char** parameter_types;
    uint32_t parameter_count;
    char* description;
    char* example_usage;
    char* since_version;
    char* deprecated_since;
    char* replacement;
    float usage_frequency;            /* How often used in codebases */
} nimcp_api_function_t;

typedef struct {
    char* library_name;
    char* version;
    nimcp_api_function_t* functions;
    uint32_t function_count;

    /* Type information */
    void* type_graph;
    uint32_t type_count;

    /* Usage patterns */
    nimcp_cow_handle_t usage_patterns;
    uint32_t pattern_count;

    /* Compatibility info */
    char** compatible_versions;
    uint32_t compatible_count;
    char** breaking_changes;
    uint32_t breaking_count;
} nimcp_library_info_t;

typedef struct {
    /* Knowledge base */
    nimcp_library_info_t* libraries;
    uint32_t library_count;

    /* Semantic index */
    void* semantic_index;             /* For natural language queries */

    /* Usage examples */
    nimcp_cow_handle_t code_examples;
    uint32_t example_count;

    /* Error patterns */
    void* common_errors;
    uint32_t error_pattern_count;

    /* Hippocampus integration */
    nimcp_hippocampus_t* hippocampus;

    nimcp_memory_pool_t* pool;
} nimcp_api_memory_t;

/**
 * Mathematical Models:
 *
 * Semantic Similarity:
 *   cosine_sim(a, b) = (a · b) / (||a|| × ||b||)
 *   Embedding space for API descriptions
 *
 * Retrieval (BM25 + Semantic):
 *   score = α × BM25(query, doc) + (1-α) × semantic_sim(query, doc)
 *
 * API Usage Patterns:
 *   Frequent itemset mining on API call sequences
 *   Association rules: {api_a, api_b} → {api_c}
 *
 * Knowledge Graph:
 *   Entities: Functions, Types, Libraries
 *   Relations: returns, accepts, throws, deprecated_by, similar_to
 */

nimcp_status_t nimcp_api_mem_init(
    nimcp_api_memory_t* mem,
    nimcp_hippocampus_t* hippocampus,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_api_mem_destroy(
    nimcp_api_memory_t* mem);

/* Knowledge loading */
nimcp_status_t nimcp_api_mem_load_library(
    nimcp_api_memory_t* mem,
    const char* library_path);

nimcp_status_t nimcp_api_mem_index_codebase(
    nimcp_api_memory_t* mem,
    const char* codebase_path);

/* Retrieval */
nimcp_status_t nimcp_api_mem_search(
    nimcp_api_memory_t* mem,
    const char* natural_language_query,
    nimcp_api_function_t** results,
    float* relevance_scores,
    uint32_t max_results,
    uint32_t* actual_results);

nimcp_status_t nimcp_api_mem_find_function(
    nimcp_api_memory_t* mem,
    const char* function_name,
    const char* library_name,
    nimcp_api_function_t** result);

nimcp_status_t nimcp_api_mem_find_similar(
    nimcp_api_memory_t* mem,
    nimcp_api_function_t* function,
    nimcp_api_function_t** similar,
    uint32_t max_results,
    uint32_t* actual_results);

/* Usage patterns */
nimcp_status_t nimcp_api_mem_get_usage_example(
    nimcp_api_memory_t* mem,
    const char* function_name,
    char** example_code);

nimcp_status_t nimcp_api_mem_get_common_errors(
    nimcp_api_memory_t* mem,
    const char* function_name,
    nimcp_error_pattern_t** errors,
    uint32_t* count);

/* Compatibility */
nimcp_status_t nimcp_api_mem_check_compatibility(
    nimcp_api_memory_t* mem,
    const char* library_name,
    const char* version_a,
    const char* version_b,
    nimcp_compatibility_report_t** report);
```

#### 11.4.7 Bug Prediction System
**File**: `include/core/brain/regions/software_eng/nimcp_bug_prediction.h`

```c
/**
 * WHAT: Proactive bug and vulnerability detection.
 * WHY:  Prevention beats debugging; security is paramount.
 * HOW:  Pattern matching + statistical risk assessment.
 */

typedef enum {
    NIMCP_SMELL_LONG_METHOD,
    NIMCP_SMELL_GOD_CLASS,
    NIMCP_SMELL_FEATURE_ENVY,
    NIMCP_SMELL_DATA_CLUMPS,
    NIMCP_SMELL_PRIMITIVE_OBSESSION,
    NIMCP_SMELL_SWITCH_STATEMENTS,
    NIMCP_SMELL_PARALLEL_INHERITANCE,
    NIMCP_SMELL_LAZY_CLASS,
    NIMCP_SMELL_SPECULATIVE_GENERALITY,
    NIMCP_SMELL_TEMPORARY_FIELD,
    NIMCP_SMELL_MESSAGE_CHAINS,
    NIMCP_SMELL_MIDDLE_MAN,
    NIMCP_SMELL_INAPPROPRIATE_INTIMACY,
    NIMCP_SMELL_DIVERGENT_CHANGE,
    NIMCP_SMELL_SHOTGUN_SURGERY,
    NIMCP_SMELL_DEAD_CODE,
    NIMCP_SMELL_DUPLICATE_CODE
} nimcp_code_smell_t;

typedef enum {
    NIMCP_VULN_SQL_INJECTION,
    NIMCP_VULN_XSS,
    NIMCP_VULN_CSRF,
    NIMCP_VULN_BUFFER_OVERFLOW,
    NIMCP_VULN_INTEGER_OVERFLOW,
    NIMCP_VULN_FORMAT_STRING,
    NIMCP_VULN_PATH_TRAVERSAL,
    NIMCP_VULN_COMMAND_INJECTION,
    NIMCP_VULN_INSECURE_DESERIALIZATION,
    NIMCP_VULN_HARDCODED_CREDENTIALS,
    NIMCP_VULN_WEAK_CRYPTO,
    NIMCP_VULN_RACE_CONDITION,
    NIMCP_VULN_NULL_DEREFERENCE,
    NIMCP_VULN_USE_AFTER_FREE,
    NIMCP_VULN_DOUBLE_FREE,
    NIMCP_VULN_MEMORY_LEAK,
    NIMCP_VULN_UNVALIDATED_INPUT
} nimcp_vulnerability_t;

typedef struct {
    nimcp_code_smell_t type;
    char* location;
    char* description;
    float severity;                   /* 0.0 - 1.0 */
    char* suggested_refactoring;
} nimcp_smell_detection_t;

typedef struct {
    nimcp_vulnerability_t type;
    char* location;
    char* description;
    float severity;                   /* CVSS-like */
    float exploitability;
    char** cwe_ids;
    uint32_t cwe_count;
    char* remediation;
    char* proof_of_concept;           /* Safe example showing issue */
} nimcp_vulnerability_detection_t;

typedef struct {
    char* file_path;
    float bug_probability;            /* 0.0 - 1.0 */
    float change_proneness;           /* Likelihood of future changes */
    float defect_density;             /* Defects per KLOC */
    uint32_t historical_bugs;
    float complexity_risk;
    float churn_risk;                 /* Based on change frequency */
} nimcp_risk_assessment_t;

typedef struct {
    /* Detection engines */
    void* smell_detector;
    void* vuln_scanner;
    void* risk_model;

    /* Pattern database */
    nimcp_cow_handle_t smell_patterns;
    nimcp_cow_handle_t vuln_patterns;

    /* Historical data */
    void* bug_history;
    void* change_history;

    /* ML model for prediction */
    void* prediction_model;

    nimcp_memory_pool_t* pool;
} nimcp_bug_prediction_t;

/**
 * Mathematical Models:
 *
 * Defect Prediction (Logistic Regression):
 *   P(defect) = 1 / (1 + exp(-(β₀ + β₁×LOC + β₂×complexity + ...)))
 *
 * Code Churn Risk:
 *   churn_risk = α × lines_added + β × lines_deleted + γ × change_frequency
 *
 * CVSS Scoring (Vulnerability Severity):
 *   Base Score = f(Impact, Exploitability)
 *   Impact = f(Confidentiality, Integrity, Availability)
 *
 * Smell Detection Thresholds:
 *   Long method: LOC > 20 OR cyclomatic > 10
 *   God class: methods > 20 AND attributes > 15 AND LCOM > 0.8
 *   Feature envy: external_calls > internal_calls
 *
 * Hotspot Detection:
 *   hotspot_score = complexity × change_frequency × bug_history
 *   Pareto: 20% of files contain 80% of bugs
 */

nimcp_status_t nimcp_bug_pred_init(
    nimcp_bug_prediction_t* pred,
    nimcp_memory_pool_t* pool);

nimcp_status_t nimcp_bug_pred_destroy(
    nimcp_bug_prediction_t* pred);

/* Code smell detection */
nimcp_status_t nimcp_bug_pred_detect_smells(
    nimcp_bug_prediction_t* pred,
    nimcp_code_comprehension_t* code,
    nimcp_smell_detection_t** smells,
    uint32_t* count);

nimcp_status_t nimcp_bug_pred_detect_smell_type(
    nimcp_bug_prediction_t* pred,
    nimcp_code_comprehension_t* code,
    nimcp_code_smell_t type,
    nimcp_smell_detection_t** smells,
    uint32_t* count);

/* Vulnerability scanning */
nimcp_status_t nimcp_bug_pred_scan_vulnerabilities(
    nimcp_bug_prediction_t* pred,
    nimcp_code_comprehension_t* code,
    nimcp_vulnerability_detection_t** vulns,
    uint32_t* count);

nimcp_status_t nimcp_bug_pred_scan_vuln_type(
    nimcp_bug_prediction_t* pred,
    nimcp_code_comprehension_t* code,
    nimcp_vulnerability_t type,
    nimcp_vulnerability_detection_t** vulns,
    uint32_t* count);

/* Risk assessment */
nimcp_status_t nimcp_bug_pred_assess_risk(
    nimcp_bug_prediction_t* pred,
    const char* file_path,
    nimcp_risk_assessment_t* assessment);

nimcp_status_t nimcp_bug_pred_find_hotspots(
    nimcp_bug_prediction_t* pred,
    nimcp_risk_assessment_t** hotspots,
    uint32_t max_results,
    uint32_t* actual_results);

/* Change impact */
nimcp_status_t nimcp_bug_pred_predict_regression_risk(
    nimcp_bug_prediction_t* pred,
    nimcp_code_diff_t* changes,
    float* regression_probability,
    char** affected_areas,
    uint32_t* affected_count);

nimcp_status_t nimcp_bug_pred_suggest_tests(
    nimcp_bug_prediction_t* pred,
    nimcp_code_diff_t* changes,
    char** suggested_tests,
    uint32_t* count);
```

### 11.5 Main Integration Module
**File**: `include/core/brain/regions/software_eng/nimcp_software_eng.h`

```c
/**
 * WHAT: Unified software engineering cognitive enhancement.
 * WHY:  Integrate all SE capabilities under single interface.
 * HOW:  Orchestrates comprehension, debugging, architecture, generation.
 */

#ifndef NIMCP_SOFTWARE_ENG_H
#define NIMCP_SOFTWARE_ENG_H

#include "nimcp_code_comprehension.h"
#include "nimcp_mental_debugger.h"
#include "nimcp_architecture_reasoning.h"
#include "nimcp_algorithm_designer.h"
#include "nimcp_code_generation.h"
#include "nimcp_api_memory.h"
#include "nimcp_bug_prediction.h"

typedef struct {
    /* Core subsystems */
    nimcp_code_comprehension_t     comprehension;
    nimcp_mental_debugger_t        debugger;
    nimcp_architecture_reasoning_t architecture;
    nimcp_algorithm_designer_t     algorithms;
    nimcp_code_generation_t        generation;
    nimcp_api_memory_t             api_memory;
    nimcp_bug_prediction_t         bug_prediction;

    /* Cross-system integration */
    nimcp_pfc_t*                   pfc;          /* Executive control */
    nimcp_hippocampus_t*           hippocampus;  /* Memory retrieval */
    nimcp_broca_t*                 broca;        /* Code output */
    nimcp_parietal_t*              parietal;     /* Symbolic reasoning */

    /* Expert mode enhancements */
    bool expert_mode;                            /* Enhanced pattern recognition */
    float experience_level;                      /* 0.0=novice, 1.0=expert */
    uint32_t deliberate_practice_hours;          /* Ericsson's 10,000 hour rule */

    /* Performance metrics */
    uint64_t bugs_found;
    uint64_t refactorings_applied;
    uint64_t patterns_recognized;
    float avg_comprehension_time_ms;

    nimcp_memory_pool_t* pool;
} nimcp_software_eng_t;

/* Lifecycle */
nimcp_status_t nimcp_software_eng_init(
    nimcp_software_eng_t* se,
    nimcp_buffer_pool_t* shared_pool,
    nimcp_pfc_t* pfc,
    nimcp_hippocampus_t* hippocampus,
    nimcp_broca_t* broca,
    nimcp_parietal_t* parietal);

nimcp_status_t nimcp_software_eng_destroy(
    nimcp_software_eng_t* se);

/* High-level operations */
nimcp_status_t nimcp_software_eng_understand_code(
    nimcp_software_eng_t* se,
    const char* source_code,
    const char* language,
    nimcp_code_understanding_t** understanding);

nimcp_status_t nimcp_software_eng_debug(
    nimcp_software_eng_t* se,
    const char* source_code,
    const char* error_description,
    nimcp_debug_session_t** session);

nimcp_status_t nimcp_software_eng_review_architecture(
    nimcp_software_eng_t* se,
    const char* codebase_path,
    nimcp_architecture_review_t** review);

nimcp_status_t nimcp_software_eng_design_algorithm(
    nimcp_software_eng_t* se,
    const char* problem_description,
    nimcp_constraints_t* constraints,
    nimcp_algorithm_design_t** design);

nimcp_status_t nimcp_software_eng_generate_code(
    nimcp_software_eng_t* se,
    nimcp_code_spec_t* specification,
    char** generated_code);

nimcp_status_t nimcp_software_eng_predict_bugs(
    nimcp_software_eng_t* se,
    const char* source_code,
    nimcp_bug_report_t** report);

/* Expert mode (enhanced pattern recognition) */
nimcp_status_t nimcp_software_eng_enable_expert_mode(
    nimcp_software_eng_t* se);

nimcp_status_t nimcp_software_eng_train(
    nimcp_software_eng_t* se,
    nimcp_training_data_t* data);

#endif /* NIMCP_SOFTWARE_ENG_H */
```

### 11.6 Integration with Brain Systems

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    SOFTWARE ENGINEERING CORTEX INTEGRATION                       │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────────┐│
│  │                         INPUT PATHWAYS                                       ││
│  │                                                                              ││
│  │  Source Code ──→ Lexical Analysis ──→ AST ──→ Code Comprehension            ││
│  │  Error Logs ───→ Pattern Matching ──→ Bug Prediction                        ││
│  │  Requirements ─→ Semantic Parse ────→ Algorithm Designer                    ││
│  │  Codebase ─────→ Graph Analysis ────→ Architecture Reasoning                ││
│  │                                                                              ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│                                       ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────┐│
│  │                    CROSS-BRAIN CONNECTIONS                                   ││
│  │                                                                              ││
│  │  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  ││
│  │  │   PFC       │    │ Hippocampus │    │   Parietal  │    │   Broca's   │  ││
│  │  │             │    │             │    │             │    │             │  ││
│  │  │ • Planning  │←──→│ • API recall│←──→│ • Symbolic  │←──→│ • Code gen  │  ││
│  │  │ • Conflict  │    │ • Examples  │    │ • Algorithm │    │ • Refactor  │  ││
│  │  │ • Executive │    │ • Patterns  │    │ • Type inf. │    │ • Template  │  ││
│  │  │             │    │             │    │             │    │             │  ││
│  │  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘  ││
│  │         │                  │                  │                  │          ││
│  │         └──────────────────┼──────────────────┼──────────────────┘          ││
│  │                            │                  │                              ││
│  │                            ▼                  ▼                              ││
│  │  ┌─────────────────────────────────────────────────────────────────────┐    ││
│  │  │              SOFTWARE ENGINEERING CORTEX (Integration)               │    ││
│  │  │                                                                      │    ││
│  │  │   Comprehension ──→ Debugger ──→ Architecture ──→ Generation        │    ││
│  │  │         │              │              │               │              │    ││
│  │  │         └──────────────┼──────────────┼───────────────┘              │    ││
│  │  │                        │              │                              │    ││
│  │  │                   Algorithm    Bug Prediction                        │    ││
│  │  │                        │              │                              │    ││
│  │  │                        └──────┬───────┘                              │    ││
│  │  │                               │                                      │    ││
│  │  │                          API Memory                                  │    ││
│  │  │                                                                      │    ││
│  │  └─────────────────────────────────────────────────────────────────────┘    ││
│  │                                                                              ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                       │                                          │
│                                       ▼                                          │
│  ┌─────────────────────────────────────────────────────────────────────────────┐│
│  │                         OUTPUT PATHWAYS                                      ││
│  │                                                                              ││
│  │  Code Understanding ──→ Natural Language Explanation                        ││
│  │  Bug Hypotheses ──────→ Ranked Fix Suggestions                              ││
│  │  Algorithm Design ────→ Pseudocode + Complexity Analysis                    ││
│  │  Architecture Review ─→ Quality Metrics + Refactoring Plan                  ││
│  │  Code Generation ─────→ Correct-by-Construction Code                        ││
│  │                                                                              ││
│  └─────────────────────────────────────────────────────────────────────────────┘│
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 11.7 File Structure

```
include/core/brain/regions/software_eng/
├── nimcp_software_eng.h              # Main integration module
├── nimcp_code_comprehension.h        # Multi-level code understanding
├── nimcp_mental_debugger.h           # Symbolic execution & hypothesis
├── nimcp_architecture_reasoning.h    # System-level analysis
├── nimcp_algorithm_designer.h        # Complexity & algorithm selection
├── nimcp_code_generation.h           # Pattern instantiation & refactoring
├── nimcp_api_memory.h                # API/library knowledge base
└── nimcp_bug_prediction.h            # Smell & vulnerability detection

src/core/brain/regions/software_eng/
├── nimcp_software_eng.c
├── nimcp_code_comprehension.c
├── nimcp_mental_debugger.c
├── nimcp_architecture_reasoning.c
├── nimcp_algorithm_designer.c
├── nimcp_code_generation.c
├── nimcp_api_memory.c
└── nimcp_bug_prediction.c
```

### 11.8 Test Plan

```
test/
├── unit/core/brain/regions/software_eng/
│   ├── test_code_comprehension.cpp    # AST, CFG, DFG, patterns
│   ├── test_mental_debugger.cpp       # Symbolic exec, hypotheses
│   ├── test_architecture.cpp          # Coupling, cohesion, cycles
│   ├── test_algorithm_designer.cpp    # Complexity analysis
│   ├── test_code_generation.cpp       # Patterns, refactoring
│   ├── test_api_memory.cpp            # Retrieval, similarity
│   └── test_bug_prediction.cpp        # Smells, vulnerabilities
├── integration/core/brain/regions/software_eng/
│   └── test_software_eng_integration.cpp
└── regression/core/brain/regions/software_eng/
    └── test_software_eng_regression.cpp
```

---

## Mathematical Enhancements Summary

| Region | Component | Mathematical Model |
|--------|-----------|-------------------|
| **Medulla** | Arousal | Leaky integrator with Schmitt trigger hysteresis |
| **Medulla** | Circadian | Dual-peak cosine modulation |
| **Cerebellum** | Forward Model | Gradient descent with eligibility traces |
| **Cerebellum** | Timing | Phase-locked loop, Weber's law |
| **Cerebellum** | Granule Cells | Sparse random projection, winner-take-all |
| **PFC** | ACC | Energy-based conflict (Botvinick model) |
| **PFC** | VMPFC | Prospect theory value function (Kahneman-Tversky) |
| **PFC** | OFC | TD(λ) learning with reversal detection |
| **Hippocampus** | Place Cells | Gaussian firing fields, Bayesian decoding |
| **Hippocampus** | Grid Cells | Hexagonal attractor network (Moser model) |
| **Hippocampus** | Dentate Gyrus | Sparse expansion coding, neurogenesis |
| **Hippocampus** | CA3 | Hopfield network pattern completion |
| **Hippocampus** | Episodic | Tensor product binding, TCM (Howard-Kahana) |
| **Basal Ganglia** | Direct/Indirect | D1/D2 receptor-modulated Go/NoGo |
| **Basal Ganglia** | Habits | Exponential strength consolidation |
| **Wernicke** | Lexical Access | Spreading activation with frequency effects |
| **Wernicke** | Semantic | Surprisal (Hale), integration cost (Gibson) |
| **Amygdala** | Fear | Rescorla-Wagner associative learning |
| **Amygdala** | Threat | Dual-pathway (LeDoux low/high road) |
| **Amygdala** | Extinction | Inhibitory learning with spontaneous recovery |
| **PCC** | Default Mode | Anti-correlated with task-positive networks |
| **PCC** | Spontaneous | Current concerns theory (Klinger) |
| **Insula** | Interoception | Predictive coding (Craig model) |
| **Insula** | Drives | Homeostatic deviation with allostasis |
| **Insula** | Salience | Multi-factor weighted salience computation |
| **Parietal** | Numerical | Weber's Law for number, Mental Number Line |
| **Parietal** | Spatial | Shepard mental rotation model |
| **Parietal** | Symbolic | Term rewriting, unification, proof search |
| **Parietal** | Analogical | Structure Mapping Theory (Gentner) |
| **Parietal** | Scientific | Bayesian model comparison |
| **Parietal** | Simulation | Pearl counterfactuals |
| **Software Eng** | Comprehension | Halstead metrics, cyclomatic complexity, plan recognition |
| **Software Eng** | Debugger | Program slicing (Weiser), symbolic execution, fault localization |
| **Software Eng** | Architecture | Coupling/cohesion metrics, Martin instability, SQALE debt |
| **Software Eng** | Algorithm | Big-O analysis, Master theorem, Pareto frontier |
| **Software Eng** | Generation | Pattern language (Alexander), AST transformation |
| **Software Eng** | API Memory | BM25 + semantic retrieval, knowledge graph |
| **Software Eng** | Bug Prediction | Defect prediction (logistic), CVSS scoring, hotspot detection |

---

## Implementation Order and Dependencies

```
Phase 1: Medulla (Foundation)
├── nimcp_arousal_state.c         # No dependencies
├── nimcp_circadian.c             # No dependencies
├── nimcp_protective_cutoff.c     # Depends on health_monitor
├── nimcp_brainstem_coupling.c    # Depends on arousal
└── nimcp_medulla.c               # Integrates all

Phase 2: Cerebellum (Coordination)
├── nimcp_granule_cell.c          # No dependencies
├── nimcp_timing_circuit.c        # Depends on oscillation_detector
├── nimcp_forward_model.c         # Depends on memory pool
├── nimcp_inverse_model.c         # Depends on forward_model
└── nimcp_cerebellum.c            # Integrates all, connects to medulla

Phase 3: PFC (Executive)
├── nimcp_acc.c                   # Depends on metacognition
├── nimcp_ofc.c                   # No dependencies
├── nimcp_vmpfc.c                 # Depends on neuromodulators
├── nimcp_dlpfc.c                 # Depends on working_memory
└── nimcp_pfc.c                   # Integrates all, connects to cerebellum

Phase 4: Hippocampus (Memory)
├── nimcp_place_cells.c           # No dependencies
├── nimcp_grid_cells.c            # No dependencies
├── nimcp_dentate_gyrus.c         # No dependencies
├── nimcp_ca3.c                   # Depends on dentate_gyrus
├── nimcp_episodic_binding.c      # Depends on place/grid cells
└── nimcp_hippocampus.c           # Integrates all, connects to PFC

Phase 5: Basal Ganglia (Action)
├── nimcp_direct_pathway.c        # Depends on neuromodulators
├── nimcp_indirect_pathway.c      # Depends on neuromodulators
├── nimcp_habit_formation.c       # Depends on direct/indirect
└── nimcp_basal_ganglia.c         # Integrates all, connects to cerebellum

Phase 6: Wernicke's (Language)
├── nimcp_lexical_access.c        # Depends on speech_cortex
├── nimcp_semantic_integration.c  # Depends on semantic_memory
├── nimcp_arcuate_fasciculus.c    # Depends on broca
└── nimcp_wernicke.c              # Integrates all, connects to Broca's

Phase 7: Amygdala (Emotion)
├── nimcp_fear_conditioning.c     # No dependencies
├── nimcp_threat_detection.c      # No dependencies
├── nimcp_extinction.c            # Depends on fear_conditioning
└── nimcp_amygdala.c              # Integrates all, connects to hippocampus

Phase 8: PCC/DMN (Self-Reference)
├── nimcp_default_mode.c          # No dependencies
├── nimcp_self_reference.c        # Depends on self_model
├── nimcp_spontaneous_thought.c   # Depends on default_mode
└── nimcp_pcc.c                   # Integrates all, connects to hippocampus

Phase 9: Insula (Interoception)
├── nimcp_interoception.c         # Depends on health_monitor
├── nimcp_homeostatic_drives.c    # Depends on interoception
├── nimcp_salience.c              # Depends on interoception, amygdala
└── nimcp_insula.c                # Integrates all, connects to amygdala/PCC

Phase 10: Parietal Lobe (Reasoning) ★ ENHANCED
├── nimcp_numerical_cognition.c   # No dependencies
├── nimcp_spatial_reasoning.c     # No dependencies
├── nimcp_symbolic_engine.c       # No dependencies
├── nimcp_analogical_reasoning.c  # Depends on semantic_memory
├── nimcp_scientific_inference.c  # Depends on symbolic_engine
├── nimcp_mental_simulation.c     # Depends on forward_model (cerebellum)
├── nimcp_multimodal_integration.c # Depends on sensory systems
├── nimcp_body_schema.c           # Depends on interoception
└── nimcp_parietal.c              # Integrates all, connects to PFC/hippocampus

Phase 11: Software Engineering Cortex ★ ENHANCED
├── nimcp_code_comprehension.c    # No dependencies (lexer/parser)
├── nimcp_mental_debugger.c       # Depends on code_comprehension
├── nimcp_architecture_reasoning.c # Depends on code_comprehension
├── nimcp_algorithm_designer.c    # Depends on parietal (symbolic_engine)
├── nimcp_code_generation.c       # Depends on broca, code_comprehension
├── nimcp_api_memory.c            # Depends on hippocampus
├── nimcp_bug_prediction.c        # Depends on code_comprehension, architecture
└── nimcp_software_eng.c          # Integrates all, connects to PFC/Parietal/Broca
```

---

## Success Criteria

1. **No Duplication**: All new code orchestrates existing APIs
2. **Memory Efficiency**: All allocations through memory pool
3. **Test Coverage**: >90% for new code
4. **Performance**: No regression in existing benchmarks
5. **Integration**: All regions communicate via defined interfaces
6. **Documentation**: WHAT/WHY/HOW comments on all functions

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| API Changes to Existing Systems | Create adapter layer if needed |
| Memory Pool Contention | Per-region local pools with shared backing |
| Circular Dependencies | Strict layering: Medulla → Cerebellum → PFC |
| Integration Complexity | Incremental integration tests at each phase |

---

## Appendix: Existing API Summary

### Health Monitor (to orchestrate)
```c
nimcp_status_t nimcp_health_monitor_record_vital(
    nimcp_health_monitor_t* monitor, const char* vital, float value);
nimcp_health_status_t nimcp_health_check(nimcp_health_monitor_t* monitor);
```

### Working Memory (to orchestrate)
```c
nimcp_status_t nimcp_wm_store(nimcp_working_memory_t* wm,
                              const void* item, size_t size);
nimcp_status_t nimcp_wm_retrieve(nimcp_working_memory_t* wm,
                                  uint32_t index, void* item);
nimcp_status_t nimcp_wm_rehearse(nimcp_working_memory_t* wm);
```

### Predictive (to orchestrate)
```c
nimcp_status_t nimcp_predictive_generate(nimcp_predictive_t* pred,
                                          const float* context,
                                          float* prediction);
nimcp_status_t nimcp_predictive_update_error(nimcp_predictive_t* pred,
                                              const float* actual);
```

### Memory Pool (to use)
```c
void* nimcp_memory_pool_acquire(nimcp_memory_pool_t* pool);
void nimcp_memory_pool_release(nimcp_memory_pool_t* pool, void* ptr);
nimcp_cow_handle_t nimcp_cow_create(nimcp_cow_manager_t* mgr,
                                     const void* data, size_t size);
void* nimcp_cow_write(nimcp_cow_manager_t* mgr, nimcp_cow_handle_t handle);
```
