# Brain Oscillations - Immune System Integration

## Overview

**Date:** 2025-12-11
**Module:** Brain Oscillations (`core/brain_oscillations`) ↔ Brain Immune System (`cognitive/immune`)
**Integration Type:** Bidirectional neuroimmune modulation

This document describes the integration between the NIMCP brain oscillations module and the brain immune system, implementing biologically-realistic bidirectional interactions between neural oscillations and immune responses.

## Biological Basis

### Cytokine-Induced Oscillation Changes

Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) alter EEG patterns during infection and inflammation:

- **Increased Delta Activity:** Slow-wave (1-4 Hz) power increases during sickness behavior
- **Reduced Gamma:** High-frequency (30-100 Hz) oscillations suppressed during cognitive impairment
- **Disrupted Theta-Gamma Coupling:** Memory deficits from inflammation-induced decoupling
- **Loss of Coherence:** Network synchronization impaired during inflammatory states

**References:**
- Opp, M. R. (2005). Cytokines and sleep. *Sleep Medicine Reviews*, 9(5), 355-364.
- Vezzani, A., & Viviani, B. (2015). Neuromodulatory properties of inflammatory cytokines. *Neuropharmacology*, 96, 70-82.

### Abnormal Oscillations Trigger Immune Surveillance

Pathological EEG patterns signal potential CNS dysfunction to the immune system:

- **Excessive Slow Waves:** May indicate infection, metabolic dysfunction, or autoimmune encephalitis
- **Loss of Gamma:** Marker for neurodegenerative processes or severe inflammation
- **Reduced Coherence:** Network dysfunction from synaptic pathology
- **Desynchronization:** Loss of coordinated activity suggests structural damage

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│            BRAIN OSCILLATIONS (FFT-based analysis)           │
│                                                              │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────────────┐  │
│  │  Activity   │──>│  FFT + Power │──>│  Band Powers    │  │
│  │  Recording  │   │  Spectrum    │   │  δ θ α β γ     │  │
│  └─────────────┘   └──────────────┘   └────────┬────────┘  │
│                                                 │           │
└─────────────────────────────────────────────────┼───────────┘
                                                  │
                                    ┌─────────────┴────────────┐
                                    │   BIDIRECTIONAL LINK     │
                                    │   (immune effects +      │
                                    │    abnormality notify)   │
                                    └─────────────┬────────────┘
                                                  │
┌─────────────────────────────────────────────────┼───────────┐
│                                                 ▼           │
│  ┌──────────────────────┐        ┌──────────────────────┐  │
│  │ IMMUNE → OSCILLATIONS│        │ OSCILLATIONS → IMMUNE│  │
│  │  Cytokine Effects    │        │  Abnormality Detect  │  │
│  ├──────────────────────┤        ├──────────────────────┤  │
│  │ • Delta amplify      │        │ • Excessive delta?   │  │
│  │ • Gamma suppress     │        │ • Suppressed gamma?  │  │
│  │ • Beta suppress      │        │ • Low coherence?     │  │
│  │ • Coherence disrupt  │        │ • Low synchrony?     │  │
│  └──────────────────────┘        └──────────────────────┘  │
│                                                             │
│              BRAIN IMMUNE SYSTEM                            │
│     (B/T cells, antibodies, cytokines, inflammation)        │
└─────────────────────────────────────────────────────────────┘
```

## Integration API

### Connection

```c
/**
 * Connect oscillation analyzer to immune system
 * Enables bidirectional immune-oscillation modulation
 */
bool brain_oscillation_connect_immune(
    brain_oscillation_analyzer_t* analyzer,
    brain_immune_system_t* immune_system
);
```

**Usage:**
```c
brain_oscillation_analyzer_t* osc = brain_oscillation_create(brain, 500, 250);
brain_immune_system_t* immune = brain_immune_create(NULL);
brain_oscillation_connect_immune(osc, immune);
```

### Immune → Oscillations: Cytokine-Induced Disruption

```c
/**
 * Compute immune-induced oscillation effects
 * Models cytokine-induced EEG changes
 */
immune_oscillation_effects_t brain_oscillation_compute_immune_effects(
    brain_oscillation_analyzer_t* analyzer,
    uint32_t inflammation_level,        // 0-4 (none to storm)
    float cytokine_concentration        // 0.0-1.0
);

/**
 * Apply immune effects to oscillation analysis
 * Modulates band powers and coherence/synchrony
 */
bool brain_oscillation_apply_immune_effects(
    brain_oscillation_analyzer_t* analyzer,
    const immune_oscillation_effects_t* effects
);
```

**Inflammation Levels:**

| Level | Enum | Delta Amp | Gamma Suppress | Coherence Disrupt |
|-------|------|-----------|----------------|-------------------|
| 0 | `INFLAMMATION_NONE` | 1.0x | 1.0x | 0% |
| 1 | `INFLAMMATION_LOCAL` | 1.0-1.3x | 0.8-1.0x | 0-10% |
| 2 | `INFLAMMATION_REGIONAL` | 1.0-1.8x | 0.6-1.0x | 0-30% |
| 3 | `INFLAMMATION_SYSTEMIC` | 1.0-2.5x | 0.4-1.0x | 0-50% |
| 4 | `INFLAMMATION_STORM` | 1.0-3.0x | 0.3-1.0x | 0-70% |

**Usage Example:**
```c
// Systemic inflammation with high cytokines
immune_oscillation_effects_t effects =
    brain_oscillation_compute_immune_effects(analyzer, 3, 0.9f);

// Apply to oscillation analysis
brain_oscillation_apply_immune_effects(analyzer, &effects);

// Subsequent analyses will show:
// - Amplified delta (2-3x)
// - Suppressed gamma (0.3-0.5x)
// - Reduced coherence
```

### Oscillations → Immune: Abnormality Detection and Notification

```c
/**
 * Detect abnormal oscillation patterns
 * Identifies neural dysfunction markers
 */
bool brain_oscillation_detect_abnormality(
    brain_oscillation_analyzer_t* analyzer,
    oscillation_abnormality_t* abnormality
);

/**
 * Notify immune system of oscillation abnormality
 * Triggers immune surveillance and response
 */
bool brain_oscillation_notify_immune_abnormality(
    brain_oscillation_analyzer_t* analyzer,
    const oscillation_abnormality_t* abnormality
);
```

**Detection Criteria:**

| Criterion | Threshold | Weight | Biological Significance |
|-----------|-----------|--------|------------------------|
| Excessive Delta | > 50% total power | 0.25 | Infection/inflammation marker |
| Suppressed Gamma | < 5% total power | 0.25 | Cognitive impairment |
| Low Coherence | < 0.3 | 0.25 | Network dysfunction |
| Low Synchrony | < 0.2 | 0.25 | Severe desynchronization |

**Abnormality Score:** Sum of weights for detected criteria. Score > 0.5 triggers immune notification.

**Usage Example:**
```c
// Analyze oscillations
oscillation_analysis_t analysis;
brain_oscillation_analyze(analyzer, &analysis);

// Detect abnormality
oscillation_abnormality_t abnormality;
bool is_abnormal = brain_oscillation_detect_abnormality(analyzer, &abnormality);

if (is_abnormal) {
    printf("Abnormality score: %.2f\n", abnormality.abnormality_score);
    printf("Consecutive abnormal: %u\n", abnormality.consecutive_abnormal);

    // Notify immune system
    brain_oscillation_notify_immune_abnormality(analyzer, &abnormality);

    // Immune system creates antigen from oscillation signature
    // Severity scaled by abnormality score
}
```

## Data Structures

### Immune-Induced Effects

```c
typedef struct {
    float delta_amplification;     /**< Delta band amplification (1.0-3.0) */
    float theta_suppression;       /**< Theta band suppression (0.5-1.0) */
    float gamma_suppression;       /**< Gamma band suppression (0.3-1.0) */
    float beta_suppression;        /**< Beta band suppression (0.5-1.0) */
    float coherence_disruption;    /**< Network coherence reduction (0.0-1.0) */
    float synchrony_disruption;    /**< Synchrony reduction (0.0-1.0) */
} immune_oscillation_effects_t;
```

### Oscillation Abnormality

```c
typedef struct {
    bool excessive_delta;          /**< Delta > 50% total power */
    bool suppressed_gamma;         /**< Gamma < 5% total power */
    bool low_coherence;            /**< Coherence < 0.3 */
    bool low_synchrony;            /**< Synchrony < 0.2 */
    float abnormality_score;       /**< Overall abnormality (0-1) */
    uint32_t consecutive_abnormal; /**< Consecutive abnormal readings */
} oscillation_abnormality_t;
```

## Implementation Details

### Modified Files

1. **Header:** `/home/bbrelin/nimcp/include/core/brain_oscillations/nimcp_brain_oscillations.h`
   - Added immune system forward declaration
   - Added `immune_oscillation_effects_t` structure
   - Added `oscillation_abnormality_t` structure
   - Added 5 new API functions

2. **Implementation:** `/home/bbrelin/nimcp/src/core/brain_oscillations/nimcp_brain_oscillations.c`
   - Added immune system header include
   - Extended `brain_oscillation_analyzer_struct` with immune tracking:
     - `immune_system` pointer
     - `active_effects` cache
     - `last_abnormality` cache
     - `consecutive_abnormal_count` counter
   - Implemented 5 integration functions (~300 lines)

3. **Tests:** `/home/bbrelin/nimcp/test/unit/core/brain/test_brain_oscillations_immune_integration.cpp`
   - 26 comprehensive unit tests
   - Covers connection, effects computation, application, detection, notification
   - Includes edge cases and error handling
   - End-to-end bidirectional feedback tests

### Internal Tracking

The oscillation analyzer maintains:
- **Connected immune system pointer** for notification
- **Active immune effects** applied to current analysis
- **Last abnormality detection** for tracking
- **Consecutive abnormal count** for persistence detection

### Automatic Discovery

The test file is automatically discovered by CMake's `discover_category_tests(unit)` function. No manual CMakeLists.txt modification needed.

**Build command:**
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make unit_core_brain_test_brain_oscillations_immune_integration -j4
```

**Run command:**
```bash
./test/unit_core_brain_test_brain_oscillations_immune_integration --gtest_brief=1
```

## Test Coverage

### Unit Tests (26 tests)

#### Connection Tests (3 tests)
- `ConnectImmuneSystem`: Successful connection
- `ConnectNullAnalyzer`: Null analyzer guard
- `ConnectNullImmune`: Null immune guard

#### Immune Effects Computation (5 tests)
- `ComputeEffectsNoInflammation`: Baseline (no disruption)
- `ComputeEffectsLocalInflammation`: Minor disruption
- `ComputeEffectsSystemicInflammation`: Strong disruption
- `ComputeEffectsCytokineStorm`: Severe disruption
- `ComputeEffectsCytokineClamp`: Concentration clamping

#### Effects Application (2 tests)
- `ApplyImmuneEffects`: Verify band power modulation
- `ApplyEffectsNullInputs`: Null input guards

#### Abnormality Detection (5 tests)
- `DetectNormalPattern`: Normal oscillations
- `DetectExcessiveDelta`: Infection marker
- `DetectSuppressedGamma`: Cognitive impairment
- `DetectMultipleAbnormalities`: Combined dysfunction
- `TrackConsecutiveAbnormal`: Persistence tracking
- `DetectAbnormalityNullInputs`: Null input guards

#### Immune Notification (3 tests)
- `NotifyImmuneAbnormality`: Successful antigen creation
- `NotifyImmuneNotConnected`: Connection guard
- `NotifyImmuneNullInputs`: Null input guards

#### End-to-End Integration (3 tests)
- `E2E_ImmuneDisruptsOscillations`: Immune → Oscillations pathway
- `E2E_AbnormalOscillationsTriggersImmune`: Oscillations → Immune pathway
- `E2E_BidirectionalFeedback`: Full feedback loop

### Expected Test Results

All 26 tests should pass, verifying:
- ✓ Proper connection establishment
- ✓ Accurate immune effect computation (all inflammation levels)
- ✓ Correct band power modulation
- ✓ Sensitive abnormality detection
- ✓ Successful immune system notification
- ✓ Bidirectional feedback loop integrity

## Usage Examples

### Example 1: Immune-Induced Oscillation Disruption

```c
// Setup
brain_t brain = brain_create(100, 500, NEURON_MODEL_HODGKIN_HUXLEY);
brain_oscillation_analyzer_t* osc = brain_oscillation_create(brain, 500, 250);
brain_immune_system_t* immune = brain_immune_create(NULL);
brain_immune_start(immune);
brain_oscillation_connect_immune(osc, immune);

// Record baseline oscillations
for (int i = 0; i < 125; i++) {
    brain_step(brain, 0.004f);  // 4ms steps
    brain_oscillation_record_activity(osc);
}

// Analyze baseline
oscillation_analysis_t baseline;
brain_oscillation_analyze(osc, &baseline);
printf("Baseline gamma: %.2f\n", baseline.wave_power.gamma_power);

// Trigger systemic inflammation in immune system
// (e.g., via antigen presentation, cytokine release)
// ...

// Compute immune effects on oscillations
immune_oscillation_effects_t effects =
    brain_oscillation_compute_immune_effects(osc, 3, 0.8f);
brain_oscillation_apply_immune_effects(osc, &effects);

// Re-analyze with immune modulation
brain_wave_power_t disrupted;
brain_oscillation_get_wave_power(osc, &disrupted);
printf("Disrupted gamma: %.2f (%.0f%% of baseline)\n",
       disrupted.gamma_power,
       100.0f * disrupted.gamma_power / baseline.wave_power.gamma_power);
```

### Example 2: Abnormal Oscillations Trigger Immune Response

```c
// Setup (same as Example 1)
brain_oscillation_connect_immune(osc, immune);

// Simulate pathological pattern (excessive delta)
for (int i = 0; i < 125; i++) {
    float t = (float)i / 250.0f;
    // Heavy delta oscillation (2 Hz)
    float pathological = 5.0f * sinf(2.0f * M_PI * 2.0f * t);
    brain_oscillation_record_value(osc, pathological);
}

// Analyze pathological pattern
oscillation_analysis_t analysis;
brain_oscillation_analyze(osc, &analysis);

// Detect abnormality
oscillation_abnormality_t abnormality;
bool is_abnormal = brain_oscillation_detect_abnormality(osc, &abnormality);

if (is_abnormal) {
    printf("Abnormality detected! Score: %.2f\n", abnormality.abnormality_score);
    printf("  Excessive delta: %s\n", abnormality.excessive_delta ? "YES" : "no");
    printf("  Suppressed gamma: %s\n", abnormality.suppressed_gamma ? "YES" : "no");

    // Notify immune system
    brain_oscillation_notify_immune_abnormality(osc, &abnormality);

    // Check immune response
    brain_immune_stats_t stats;
    brain_immune_get_stats(immune, &stats);
    printf("Immune antigens processed: %lu\n", stats.antigens_processed);
}
```

## Design Patterns

- **Observer:** Oscillation analyzer observes immune state changes
- **Strategy:** Different immune effects for different inflammation levels
- **Adapter:** Converts oscillation abnormalities to immune antigens
- **Guard Clauses:** Early returns for null checks and validation

## NIMCP Standards Compliance

✓ All functions < 50 lines
✓ Guard clauses (early returns)
✓ WHAT-WHY-HOW documentation
✓ Thread-safe via existing mutex
✓ nimcp_malloc/nimcp_free memory management
✓ Biological basis documented
✓ Single responsibility per function

## Future Enhancements

1. **Real-time Coherence/Synchrony Modulation:** Currently uses placeholders; integrate with actual FFT-based computation
2. **Cytokine-Specific Effects:** Different effects for IL-1β, IL-6, TNF-α
3. **Time-Course Modeling:** Track inflammation duration effects on oscillations
4. **Cross-Frequency Coupling:** Model cytokine effects on theta-gamma PAC
5. **Callback Integration:** Register callbacks for oscillation state changes to trigger immune auto-responses

## References

### Scientific Literature

1. **Cytokines and Sleep:**
   Opp, M. R. (2005). Cytokines and sleep. *Sleep Medicine Reviews*, 9(5), 355-364.

2. **Neuroinflammation and Oscillations:**
   Vezzani, A., & Viviani, B. (2015). Neuromodulatory properties of inflammatory cytokines. *Neuropharmacology*, 96, 70-82.

3. **EEG in Encephalitis:**
   Bien, C. G., & Elger, C. E. (2007). Autoimmune epilepsy. *Current Opinion in Neurology*, 20(2), 193-199.

4. **Sickness Behavior and Slow Waves:**
   Dantzer, R., et al. (2008). From inflammation to sickness and depression. *Nature Reviews Neuroscience*, 9(1), 46-56.

### NIMCP Documentation

- **Brain Immune System:** `CLAUDE.md` - Brain Immune System section
- **Brain Oscillations:** `include/core/brain_oscillations/nimcp_brain_oscillations.h`
- **Immune API:** `include/cognitive/immune/nimcp_brain_immune.h`

## Summary

This integration implements biologically-realistic bidirectional neuroimmune modulation:

**Immune → Oscillations:**
- Cytokine-induced disruption of EEG patterns
- Progressive effects from local to systemic inflammation
- Models observed clinical phenomena (sickness behavior, cognitive fog)

**Oscillations → Immune:**
- Abnormal pattern detection triggers immune surveillance
- Pattern signature encoded as antigen epitope
- Severity scaling based on abnormality score

**Result:** A closed feedback loop where immune activation disrupts oscillations, and pathological oscillations trigger immune responses - mimicking the complex neuroimmune interactions observed in CNS infections, autoimmune encephalitis, and neurodegenerative diseases.
