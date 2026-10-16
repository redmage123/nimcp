# Hypothalamus Module Implementation Plan

## Overview

Implement a biologically-inspired hypothalamus module based on Steve Byrnes' neuroscience research on brain-like AGI safety. The hypothalamus forms the core of the "Steering Subsystem" - the ultimate source of motivations, drives, and values that function as the reinforcement learning reward function.

**Byrnes' Key Insight**: The steering subsystem (~10% of brain) sends reward signals that steer the learning subsystem (~90%). Careful design of this reward function is a key lever for AGI alignment.

---

## File Structure

### Headers (`include/core/brain/subcortical/`)
```
nimcp_hypothalamus.h              # Main module
nimcp_hypothalamus_nuclei.h       # Individual nuclei
nimcp_hypothalamus_drives.h       # Drive state representation
nimcp_hypothalamus_homeostasis.h  # Setpoints (reward function params)
nimcp_hypothalamus_hpa.h          # HPA stress axis
```

### Bridges (`include/core/brain/subcortical/`)
```
nimcp_hypothalamus_snc_bridge.h        # -> SNc/VTA (reward -> RPE)
nimcp_hypothalamus_thalamus_bridge.h   # -> Thalamus (arousal control)
nimcp_hypothalamus_executive_bridge.h  # -> Executive (drive-aware goals)
nimcp_hypothalamus_attention_bridge.h  # -> Attention gate (salience)
nimcp_hypothalamus_nac_bridge.h        # -> NAc (wanting/liking)
nimcp_hypothalamus_amygdala_bridge.h   # <-> Amygdala (threat/stress)
nimcp_hypothalamus_medulla_bridge.h    # <-> Medulla (autonomic control)
nimcp_hypothalamus_brainstem_bridge.h  # <-> Brainstem (arousal, reflexes)
nimcp_hypothalamus_substrate_bridge.h  # -> Neural substrate (already exists!)
nimcp_hypothalamus_immune_bridge.h     # <-> Brain immune (cytokines/HPA)
nimcp_hypothalamus_insula_bridge.h     # <- Insula (interoception input)
nimcp_hypothalamus_sleep_bridge.h      # <-> Sleep/wake (circadian/arousal)
nimcp_hypothalamus_emotion_bridge.h    # <-> Emotional system (stress/HPA)
nimcp_hypothalamus_wellbeing_bridge.h  # <-> Wellbeing (homeostatic balance)
nimcp_hypothalamus_thalamic_router_bridge.h  # -> Thalamic router (attention/routing)
nimcp_hypothalamus_perception_bridge.h # -> Sensory cortices (arousal/salience)
nimcp_hypothalamus_broca_bridge.h      # -> Broca's area (speech modulation)
```

### GPU Acceleration (`include/gpu/hypothalamus/`)
```
nimcp_hypothalamus_gpu.h               # GPU-accelerated drive dynamics
nimcp_hypothalamus_qmc_gpu.h           # GPU QMC for EFE computation
```

### Sources (`src/core/brain/subcortical/`)
- Corresponding `.c` files for all headers

### Tests (`test/unit/core/brain/subcortical/`)
```
test_hypothalamus.cpp
test_hypothalamus_drives.cpp
test_hypothalamus_homeostasis.cpp
test_hypothalamus_bridges.cpp
```

---

## Key Data Structures

### Hypothalamic Nuclei
```c
typedef enum {
    HYPO_NUCLEUS_LATERAL = 0,       // Hunger, orexin neurons
    HYPO_NUCLEUS_VENTROMEDIAL,      // Satiety center
    HYPO_NUCLEUS_ANTERIOR,          // Thermoregulation (cooling)
    HYPO_NUCLEUS_POSTERIOR,         // Thermoregulation (heating), arousal
    HYPO_NUCLEUS_ARCUATE,           // Energy balance, POMC/AgRP
    HYPO_NUCLEUS_PARAVENTRICULAR,   // Stress (CRH), oxytocin
    HYPO_NUCLEUS_SUPRACHIASMATIC,   // Circadian clock
    HYPO_NUCLEUS_SUPRAOPTIC,        // Osmolarity, ADH
    HYPO_NUCLEUS_PREOPTIC,          // Sleep/wake
    HYPO_NUCLEUS_TUBEROMAMMILLARY,  // Histamine arousal
    HYPO_NUCLEUS_COUNT
} hypo_nucleus_type_t;
```

### Drive Types
```c
typedef enum {
    HYPO_DRIVE_HUNGER = 0,
    HYPO_DRIVE_THIRST,
    HYPO_DRIVE_TEMPERATURE,
    HYPO_DRIVE_FATIGUE,
    HYPO_DRIVE_SOCIAL,
    HYPO_DRIVE_CURIOSITY,
    HYPO_DRIVE_SAFETY,
    HYPO_DRIVE_AUTONOMY,
    HYPO_DRIVE_COMPETENCE,
    HYPO_DRIVE_COUNT
} hypo_drive_type_t;
```

### Alignment Modes (per Byrnes)
```c
typedef enum {
    HYPO_ALIGN_CONTROLLED = 0,      // Values explicitly specified (safer)
    HYPO_ALIGN_SOCIAL_INSTINCT,     // Values learned from observation
    HYPO_ALIGN_HYBRID               // Core controlled, details learned
} hypo_alignment_mode_t;
```

### Homeostatic Setpoints (The Reward Function)
```c
typedef struct {
    // Physiological setpoints
    float temperature_setpoint;     // 37.0C
    float glucose_setpoint;         // 90 mg/dL
    float osmolarity_setpoint;      // 285 mOsm/L

    // Psychological setpoints
    float social_setpoint;
    float curiosity_setpoint;
    float safety_setpoint;

    // ALIGNMENT PARAMETERS (Byrnes' key insight)
    float human_wellbeing_weight;
    float harm_avoidance_weight;
    float honesty_weight;

    // Control parameters
    float reward_gain;
    bool setpoints_locked;          // Prevent runtime modification
} hypo_setpoint_config_t;
```

### Main Structure
```c
typedef struct hypothalamus {
    hypo_nucleus_t nuclei[HYPO_NUCLEUS_COUNT];
    hypo_drive_system_t drives;
    hypo_homeostasis_t* homeostasis;
    hypo_hpa_axis_t* hpa_axis;

    // Arousal/circadian state
    hypo_arousal_state_t arousal;
    float circadian_phase;

    // Outputs (steering signals)
    float reward_signal;            // -> SNc/VTA
    float* drive_salience;          // -> Attention gate
    float arousal_level;            // -> Thalamus

    // Configuration & stats
    hypothalamus_config_t config;
    hypothalamus_stats_t stats;

    // Bio-async & thread safety
    bio_module_context_t bio_ctx;
    nimcp_mutex_t* mutex;
} hypothalamus_t;
```

---

## Integration Architecture

```
+---------------------------------------------------------------------+
|                      HYPOTHALAMUS (Steering Core)                    |
|  +----------+ +----------+ +----------+ +----------+ +-----------+  |
|  | Lateral  | | Ventro-  | | Para-    | | SCN      | | Posterior |  |
|  | (hunger) | | medial   | | ventr.   | |(circadian| | (arousal) |  |
|  |          | | (satiety)| | (stress) | |          | |           |  |
|  +----+-----+ +----+-----+ +----+-----+ +----+-----+ +-----+-----+  |
|       +------------+-----+------+------------+-------------+        |
|                          |                                          |
|                    +-----v-----+                                    |
|                    | Drive     |<--- Interoceptive inputs           |
|                    | Integrator|     (glucose, temp, osmolarity)    |
|                    +-----+-----+                                    |
|                          |                                          |
|         +----------------+----------------+----------------+        |
|         |                |                |                |        |
|         v                v                v                v        |
|  +-------------+  +-------------+  +-------------+  +-----------+   |
|  | SNc Bridge  |  | Thalamus    |  | Attention   |  | Executive |   |
|  | (reward->RPE|  | Bridge      |  | Bridge      |  | Bridge    |   |
|  |             |  | (arousal)   |  | (salience)  |  | (goals)   |   |
|  +------+------+  +------+------+  +------+------+  +-----+-----+   |
+---------+----------------+----------------+---------------+---------+
          |                |                |               |
          v                v                v               v
   +-------------+  +-------------+  +-------------+  +-----------+
   | Substantia  |  | Thalamus    |  | Attention   |  | Executive |
   | Nigra (SNc) |  | (TRN mode)  |  | Gate        |  | Controller|
   | (dopamine)  |  |             |  |             |  |           |
   +-------------+  +-------------+  +-------------+  +-----------+
```

---

## Steering Subsystem Integration (Byrnes Model)

Per Byrnes, the "Steering Subsystem" (~10% of brain) comprises **hypothalamus + brainstem**. These must work together:

```
+-----------------------------------------------------------------------+
|                    STEERING SUBSYSTEM (Byrnes)                         |
|                                                                        |
|  +---------------------------+    +---------------------------+       |
|  |      HYPOTHALAMUS         |    |       BRAINSTEM           |       |
|  |  (Homeostatic Regulation) |    |   (Autonomic/Arousal)     |       |
|  |                           |    |                           |       |
|  |  - Drive states           |<-->|  - Arousal nuclei         |       |
|  |  - Reward function        |    |  - Autonomic control      |       |
|  |  - Setpoint deviations    |    |  - Reflex coordination    |       |
|  |  - Circadian (SCN)        |    |  - Pain/pleasure relay    |       |
|  +-------------+-------------+    +-------------+-------------+       |
|                |                                |                      |
|                +----------------+---------------+                      |
|                                 |                                      |
|                                 v                                      |
|                    +------------------------+                          |
|                    |       MEDULLA          |                          |
|                    | (Autonomic Execution)  |                          |
|                    |                        |                          |
|                    | - Heart rate           |                          |
|                    | - Respiration          |                          |
|                    | - Blood pressure       |                          |
|                    | - Vagal tone           |                          |
|                    +------------------------+                          |
+-----------------------------------------------------------------------+
                                 |
                                 v
                    +------------------------+
                    |   NEURAL SUBSTRATE     |
                    | (Implementation Layer) |
                    +------------------------+
```

### Existing Files to Integrate

| Component | Header | Integration Point |
|-----------|--------|-------------------|
| Medulla | `include/core/medulla/nimcp_medulla.h` | Autonomic outputs |
| Brainstem | `include/core/brainstem/nimcp_brainstem_substrate_bridge.h` | Arousal signals |
| Brainstem-Thalamic | `include/core/brainstem/nimcp_brainstem_thalamic_bridge.h` | Arousal -> Thalamus |
| Substrate | `include/core/hypothalamus/nimcp_hypothalamus_substrate_bridge.h` | Already exists! |

### New Bridges Required

```c
// Hypothalamus <-> Brainstem bidirectional
nimcp_hypothalamus_brainstem_bridge.h
- Arousal signals: hypothalamus -> brainstem arousal nuclei
- Pain/pleasure: brainstem -> hypothalamus reward computation
- Stress response: hypothalamus CRH -> brainstem autonomic

// Hypothalamus <-> Medulla for autonomic control
nimcp_hypothalamus_medulla_bridge.h
- Temperature regulation -> vasodilation/vasoconstriction
- Hunger -> digestive reflexes
- Stress -> heart rate, respiration
```

---

## Omnidirectional Inference Integration (CRITICAL)

The omnidirectional inference system (Active Inference + Precision Weighting + World Models) directly implements the computational framework Byrnes describes. The hypothalamus provides the **preference priors** and **precision modulation** that drive inference:

```
+=========================================================================+
|           HYPOTHALAMUS <-> OMNIDIRECTIONAL INFERENCE                     |
+=========================================================================+
|                                                                          |
|   HYPOTHALAMUS                        OMNI ACTIVE INFERENCE              |
|   ============                        =====================              |
|                                                                          |
|   Setpoints (priors)  ────────────>  Preferred Observations p(o)         |
|   - glucose = 90                      - "What the system wants"          |
|   - temp = 37°C                       - Deviations create EFE            |
|   - social = X                                                           |
|                                                                          |
|   Drive Urgency  ─────────────────>  Prior Precision Π_prior             |
|   - High hunger urgency               - High precision on food priors    |
|   - Amplifies prediction errors       - System "attends" to drive        |
|                                                                          |
|   Arousal Level  ─────────────────>  Global Precision Gain               |
|   - High arousal = vigilant           - All precisions boosted           |
|   - Affects all inference channels    - Faster, more reactive            |
|                                                                          |
|   Reward Signal  ─────────────────>  Expected Free Energy G(π)           |
|   - Satisfaction = -EFE               - Actions minimizing EFE           |
|   - Drives policy selection           - Select drive-satisfying actions  |
|                                                                          |
+=========================================================================+
|                                                                          |
|   OMNI PRECISION                      HYPOTHALAMUS                        |
|   ==============                      ============                        |
|                                                                          |
|   Prediction Errors ε ────────────>  Setpoint Deviation Signals          |
|   - "Observation != Prediction"       - Interoceptive PE = drive update  |
|                                                                          |
|   Precision Updates  ─────────────>  Attention Bias Updates              |
|   - Channel precision changes         - Which drives get attention       |
|                                                                          |
|   Policy Outcomes  ───────────────>  Drive Satisfaction                  |
|   - Action results                    - Did action reduce deviation?     |
|                                                                          |
+=========================================================================+
```

### New Files for Omni Integration

```
include/cognitive/omni/nimcp_omni_hypothalamus_bridge.h  # Bidirectional integration
```

### Key Integration Points

| Hypothalamus Component | Omni Inference Role | Message/API |
|------------------------|---------------------|-------------|
| `hypo_setpoint_config_t` | Preferred observations (priors) | `omni_ai_set_preference_prior()` |
| `drive.urgency` | Prior precision weighting | `omni_precision_set_channel()` |
| `arousal_level` | Global precision gain | `omni_precision_set_global_gain()` |
| `reward_signal` | Negative EFE component | `omni_ai_update_reward()` |
| `HYPO_DRIVE_CURIOSITY` | Epistemic value (information gain) | `omni_ai_set_epistemic_weight()` |

### Alignment Implication (Byrnes)

**This is the core alignment lever**: The hypothalamic setpoints define what the active inference system "wants." By controlling setpoints, we control the preferred observations that drive all behavior. The omnidirectional precision system then weights how strongly these preferences influence action selection.

```c
// ALIGNMENT CRITICAL: Setpoints become active inference priors
typedef struct {
    float* preferred_observations;      // From hypothalamus setpoints
    float* prior_precisions;            // From drive urgencies
    float global_precision_gain;        // From arousal level
    bool preferences_locked;            // Alignment safety
} omni_hypo_preference_config_t;
```

---

## Quantum Monte Carlo Integration

The quantum Monte Carlo (QMC) and quantum MCTS systems are relevant for:

### Existing Infrastructure

A `nimcp_hypothalamus_quantum_bridge.h` **already exists** with:

```c
// Quantum optimization modes
typedef enum {
    HYPOTHALAMUS_QUANTUM_MODE_HOMEOSTATIC,  // Multi-setpoint optimization (QUBO)
    HYPOTHALAMUS_QUANTUM_MODE_CIRCADIAN,    // Phase trajectory (quantum walk)
    HYPOTHALAMUS_QUANTUM_MODE_AUTONOMIC,    // Parallel state evaluation
    HYPOTHALAMUS_QUANTUM_MODE_HPA,          // HPA axis tuning (VQE)
    HYPOTHALAMUS_QUANTUM_MODE_FULL          // All quantum features
} hypothalamus_quantum_mode_t;
```

### Quantum Applications to Hypothalamus

| Problem | Quantum Algorithm | Benefit |
|---------|-------------------|---------|
| Multi-objective setpoint optimization | QUBO + Quantum Annealing | Find global optima for competing drives |
| Expected Free Energy computation | QMC amplitude estimation | Efficient EFE sampling for active inference |
| Policy search in active inference | Quantum MCTS | Explore policy space with quantum speedup |
| Circadian phase prediction | Quantum walk | Optimal phase trajectory |
| Regulatory strategy evaluation | Superposition | Parallel evaluation of strategies |

### Integration with Active Inference

```
+=========================================================================+
|        QUANTUM-ACCELERATED ACTIVE INFERENCE FOR HYPOTHALAMUS             |
+=========================================================================+
|                                                                          |
|   HYPOTHALAMUS                    QMC / QUANTUM MCTS                     |
|   ============                    ==================                     |
|                                                                          |
|   Setpoint optimization  ───────> QUBO Annealing                        |
|   - Temperature, glucose,         - Find global optimum in              |
|     hydration, stress             - complex energy landscape            |
|                                                                          |
|   EFE computation  ─────────────> QMC Amplitude Estimation              |
|   - G(π) = E[ln q - ln p]         - Efficient sampling from            |
|   - Needs integral over states    - posterior over states               |
|                                                                          |
|   Policy search  ───────────────> Quantum MCTS                          |
|   - Which action minimizes EFE?   - O(√N) search vs O(N)               |
|   - Explore drive-satisfying      - Grover-accelerated                 |
|     policies                      - tree search                         |
|                                                                          |
|   Circadian phase  ─────────────> Quantum Walk                          |
|   - Optimal wake/sleep timing     - Phase space trajectory             |
|                                                                          |
+=========================================================================+
```

### New Files

```
include/cognitive/omni/nimcp_omni_qmc_bridge.h  # QMC for EFE computation
```

### Alignment Implication

Quantum optimization can find **globally optimal setpoints** that satisfy multiple constraints simultaneously. This is alignment-relevant: we can encode human preference constraints into the QUBO and let quantum annealing find setpoints that satisfy them.

---

## Cognitive Layer Integration

The hypothalamus modulates cognitive processing based on drive states:

```
                         HYPOTHALAMUS
                              |
         +--------------------+--------------------+
         |                    |                    |
         v                    v                    v
+------------------+  +------------------+  +------------------+
| CURIOSITY MODULE |  | IMAGINATION      |  | JEPA PREDICTOR   |
| HYPO_DRIVE_      |  | ENGINE           |  |                  |
| CURIOSITY ->     |  | Drive urgency -> |  | Reward signal -> |
| exploration bias |  | simulation       |  | prediction       |
|                  |  | priorities       |  | targets          |
+------------------+  +------------------+  +------------------+
         |                    |                    |
         v                    v                    v
+------------------+  +------------------+  +------------------+
| GAME THEORY      |  | THEORY OF MIND   |  | WORKING MEMORY   |
| Social drive ->  |  | Social needs ->  |  | Survival items   |
| cooperation vs   |  | ToM engagement   |  | get priority     |
| competition bias |  | depth            |  | maintenance      |
+------------------+  +------------------+  +------------------+
         |                    |                    |
         +--------------------+--------------------+
                              |
                              v
                   +------------------+
                   | RECURSIVE        |
                   | COGNITION        |
                   | Drive urgency -> |
                   | metacognitive    |
                   | depth allocation |
                   +------------------+
```

### Cognitive Integration Signals

| Drive | Cognitive Effect |
|-------|-----------------|
| `HYPO_DRIVE_CURIOSITY` | Boosts exploration in curiosity module, increases imagination sampling |
| `HYPO_DRIVE_SOCIAL` | Increases ToM engagement, shifts game theory toward cooperation |
| `HYPO_DRIVE_SAFETY` | Prioritizes threat-related items in working memory, reduces exploration |
| `HYPO_DRIVE_FATIGUE` | Reduces metacognitive depth, favors cached/habitual responses |
| `reward_signal` | Provides teaching signal for JEPA predictions, updates value estimates |
| `arousal_level` | Modulates processing speed and attention breadth across all modules |

---

## System Integration Requirements

### Brain Factory Init (EXISTING)

The brain factory already has `nimcp_brain_init_hypothalamus.h` with initialization functions:

```c
// Existing API we must implement/align with:
bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t brain);
bool nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain_t brain);
bool nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain_t brain);
bool nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain_t brain);
bool nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain_t brain);
bool nimcp_brain_factory_connect_hypothalamus_to_sleep(brain_t brain);
bool nimcp_brain_factory_connect_hypothalamus_to_immune(brain_t brain);  // ← Immune!
bool nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain_t brain);
bool nimcp_brain_factory_connect_hypothalamus_to_medulla(brain_t brain);
bool nimcp_brain_factory_connect_hypothalamus_to_emotions(brain_t brain);
```

**Our implementation must align with this existing factory API.**

### Wiring Diagram Integration (REQUIRED)

The wiring diagram system uses JSONL files in `.aim/wiring/`:

```
.aim/wiring/
├── master.jsonl              # Base module wiring
├── subsystems/
│   ├── core.jsonl            # ← Add hypothalamus here
│   ├── cognition.jsonl
│   └── immune.jsonl
├── platforms/
│   └── medium.jsonl          # Tier-specific overrides
└── custom/
    └── user.jsonl            # User customizations
```

**Hypothalamus Wiring JSONL** (add to `subsystems/core.jsonl`):
```jsonl
{"type":"entity","name":"Hypothalamus","entityType":"SubcorticalModule","subsystem":"core","tier_min":"MEDIUM","module_id":"BIO_MODULE_HYPOTHALAMUS"}
{"type":"relation","from":"Hypothalamus","to":"Substantia_Nigra_SNc","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Thalamus","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Attention_Gate","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Executive_Controller","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Brain_Immune","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Medulla","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Brainstem","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"Omni_Active_Inference","relationType":"SENDS_TO"}
{"type":"relation","from":"Amygdala","to":"Hypothalamus","relationType":"SENDS_TO"}
{"type":"relation","from":"Insula","to":"Hypothalamus","relationType":"SENDS_TO"}
{"type":"relation","from":"Brain_Immune","to":"Hypothalamus","relationType":"SENDS_TO"}
{"type":"relation","from":"Hypothalamus","to":"BIO_MSG_HYPO_DRIVE_STATE","relationType":"HANDLES_MESSAGE"}
{"type":"relation","from":"Hypothalamus","to":"BIO_MSG_HYPO_REWARD_SIGNAL","relationType":"HANDLES_MESSAGE"}
{"type":"relation","from":"Hypothalamus","to":"BIO_MSG_HYPO_AROUSAL_CHANGE","relationType":"HANDLES_MESSAGE"}
{"type":"relation","from":"Hypothalamus","to":"Brainstem","relationType":"DEPENDS_ON"}
{"type":"relation","from":"Hypothalamus","to":"Medulla","relationType":"DEPENDS_ON"}
```

### Brain Immune System Integration (NEW BRIDGE REQUIRED)

**Missing file**: `nimcp_hypothalamus_immune_bridge.h`

The brain immune system (`nimcp_brain_immune.h`) coordinates threat response. Hypothalamus integrates via:

```c
// New bridge required
typedef struct hypo_immune_bridge {
    brain_immune_system_t* immune;
    hypothalamus_t* hypo;

    // Cytokine → Setpoint modulation (fever, sickness behavior)
    float fever_setpoint_offset;       // IL-1, IL-6 → temperature↑
    float appetite_suppression;        // TNF-α → hunger↓
    float fatigue_increase;            // Interferons → fatigue↑

    // HPA → Immune modulation (stress immunosuppression)
    float cortisol_immune_suppression; // High cortisol → immune↓
} hypo_immune_bridge_t;

// Biological pathways:
// 1. Cytokines → Hypothalamus: Fever, sickness behavior, anorexia
// 2. Hypothalamus → Immune: HPA cortisol suppresses immune response
// 3. Circadian → Immune: SCN modulates immune cell trafficking
```

---

## Bio-Async Message Types

Add to `nimcp_bio_messages.h` (range 0x1140-0x115F):

```c
BIO_MSG_HYPO_DRIVE_STATE = 0x1140,      // Drive state broadcast
BIO_MSG_HYPO_REWARD_SIGNAL,              // Reward -> SNc/VTA
BIO_MSG_HYPO_AROUSAL_CHANGE,             // Arousal -> Thalamus
BIO_MSG_HYPO_SURVIVAL_PRIORITY,          // Priority -> Attention
BIO_MSG_HYPO_STRESS_RESPONSE,            // HPA activation
BIO_MSG_HYPO_CIRCADIAN_PHASE,            // Circadian update
BIO_MSG_HYPO_SETPOINT_DEVIATION,         // Deviation alert
BIO_MSG_HYPO_ALIGNMENT_ALERT,            // Safety alert
```

---

## Wiring Diagram Entries

```jsonl
{"type":"entity","name":"Hypothalamus","entityType":"SubcorticalModule","subsystem":"core","tier_min":"MEDIUM"}
{"type":"relation","from":"Hypothalamus","to":"Substantia_Nigra_SNc","relationType":"SENDS_REWARD"}
{"type":"relation","from":"Hypothalamus","to":"Thalamus","relationType":"MODULATES_AROUSAL"}
{"type":"relation","from":"Hypothalamus","to":"Attention_Gate","relationType":"PROVIDES_SALIENCE"}
{"type":"relation","from":"Hypothalamus","to":"Executive_Controller","relationType":"INFORMS_GOALS"}
{"type":"relation","from":"Hypothalamus","to":"Nucleus_Accumbens","relationType":"MODULATES_WANTING"}
{"type":"relation","from":"Amygdala","to":"Hypothalamus","relationType":"SENDS_THREAT"}
{"type":"relation","from":"Insula","to":"Hypothalamus","relationType":"PROVIDES_INTEROCEPTION"}
{"type":"relation","from":"Hypothalamus","to":"Curiosity_Module","relationType":"MODULATES_DRIVE"}
{"type":"relation","from":"Hypothalamus","to":"Imagination_Engine","relationType":"PROVIDES_PRIORITY"}
{"type":"relation","from":"Hypothalamus","to":"JEPA_Predictor","relationType":"PROVIDES_REWARD"}
{"type":"relation","from":"Hypothalamus","to":"Game_Theory_Module","relationType":"MODULATES_STRATEGY"}
{"type":"relation","from":"Hypothalamus","to":"Theory_of_Mind","relationType":"MODULATES_ENGAGEMENT"}
{"type":"relation","from":"Hypothalamus","to":"Working_Memory","relationType":"PROVIDES_PRIORITY"}
{"type":"relation","from":"Hypothalamus","to":"Recursive_Cognition","relationType":"MODULATES_DEPTH"}
{"type":"relation","from":"Hypothalamus","to":"Brainstem","relationType":"SENDS_AROUSAL"}
{"type":"relation","from":"Brainstem","to":"Hypothalamus","relationType":"SENDS_PAIN_PLEASURE"}
{"type":"relation","from":"Hypothalamus","to":"Medulla","relationType":"CONTROLS_AUTONOMIC"}
{"type":"relation","from":"Medulla","to":"Hypothalamus","relationType":"PROVIDES_INTEROCEPTION"}
{"type":"relation","from":"Hypothalamus","to":"Neural_Substrate","relationType":"SUBSTRATE_BRIDGE"}
{"type":"relation","from":"Hypothalamus","to":"Omni_Active_Inference","relationType":"PROVIDES_PREFERENCES"}
{"type":"relation","from":"Hypothalamus","to":"Omni_Precision","relationType":"MODULATES_PRECISION"}
{"type":"relation","from":"Hypothalamus","to":"Omni_World_Model","relationType":"PROVIDES_PRIORS"}
{"type":"relation","from":"Omni_Active_Inference","to":"Hypothalamus","relationType":"SENDS_POLICY_OUTCOME"}
{"type":"relation","from":"Omni_Precision","to":"Hypothalamus","relationType":"SENDS_PE"}
```

---

## Implementation Phases

### Phase 1: Core Module + Alignment Safety (Foundation)
**Files**: `nimcp_hypothalamus.h/c`, `nimcp_hypothalamus_drives.h/c`

- [ ] Define enumerations (nuclei, drives, alignment modes)
- [ ] Implement lifecycle (`_default_config`, `_create`, `_destroy`, `_reset`)
- [ ] Implement drive state representation
- [ ] **PRIORITY**: Implement alignment mode enum and configuration
- [ ] **PRIORITY**: Implement setpoint locking mechanism from day one
- [ ] Add thread safety (mutex pattern from existing modules)
- [ ] Basic unit tests including alignment safety tests

### Phase 2: Homeostasis System with Alignment (Reward Function)
**Files**: `nimcp_hypothalamus_homeostasis.h/c`

- [ ] Implement setpoint configuration with alignment weights
- [ ] **PRIORITY**: `human_wellbeing_weight`, `harm_avoidance_weight`, `honesty_weight`
- [ ] Implement deviation -> reward computation
- [ ] **PRIORITY**: Setpoint modification logging and alerts
- [ ] **PRIORITY**: BIO_MSG_HYPO_ALIGNMENT_ALERT for unauthorized access
- [ ] Implement PI/PD control dynamics
- [ ] Unit tests for reward generation and alignment safety

### Phase 3: Nuclei Implementation
**Files**: `nimcp_hypothalamus_nuclei.h/c`

- [ ] Lateral hypothalamus (hunger, orexin)
- [ ] Paraventricular nucleus (stress, CRH)
- [ ] Suprachiasmatic nucleus (circadian)
- [ ] Posterior hypothalamus (arousal)
- [ ] Circadian rhythm simulation

### Phase 4: HPA Axis
**Files**: `nimcp_hypothalamus_hpa.h/c`

- [ ] CRH -> ACTH -> Cortisol cascade
- [ ] Negative feedback loop
- [ ] Chronic stress effects

### Phase 5: Bio-Async Integration
- [ ] Define message types in `nimcp_bio_messages.h`
- [ ] Implement handler registration
- [ ] Add wiring diagram entries
- [ ] Register with orchestrator (Phase 1 startup)

### Phase 6: Omnidirectional Inference Bridge (CRITICAL - Byrnes Core)
**Files**: `nimcp_omni_hypothalamus_bridge.h/c`

- [ ] **Setpoints as Priors**: Connect `hypo_setpoint_config_t` to `omni_ai_set_preference_prior()`
- [ ] **Drive Urgency as Precision**: Map `drive.urgency` to `omni_precision_set_channel()`
- [ ] **Arousal as Global Gain**: Connect `arousal_level` to `omni_precision_set_global_gain()`
- [ ] **Reward to EFE**: Integrate `reward_signal` into expected free energy computation
- [ ] **Curiosity as Epistemic**: Map `HYPO_DRIVE_CURIOSITY` to epistemic value weight
- [ ] **Preference Locking**: Implement `preferences_locked` for alignment safety
- [ ] Test policy selection is influenced by drive states

### Phase 7: SNc/VTA Bridge
**Files**: `nimcp_hypothalamus_snc_bridge.h/c`

- [ ] Implement reward -> SNc pathway
- [ ] Drive-specific reward channels
- [ ] Connect to existing `nimcp_substantia_nigra.h`
- [ ] Test RPE computation with hypothalamic reward

### Phase 8: Thalamus & Thalamic Router Bridge
**Files**: `nimcp_hypothalamus_thalamus_bridge.h/c`, `nimcp_hypothalamus_thalamic_router_bridge.h/c`

Integrates with both the generic thalamus AND the thalamic router middleware.

- [ ] **Arousal → Thalamic Router**: `arousal_level` → `thalamic_router_set_attention_threshold()`
- [ ] **Arousal → Firing mode mapping**: High arousal = tonic (detailed), low = burst (salient only)
- [ ] **Histamine/orexin channels**: TMN histamine → cortical arousal, LH orexin → wakefulness
- [ ] **Priority modulation**: Survival drives → `SIGNAL_PRIORITY_HIGH` bypass
- [ ] **Min attention threshold**: Drive urgency modulates `min_attention_threshold`
- [ ] Connect to `include/middleware/routing/nimcp_thalamic_router.h`
- [ ] Test arousal effects on relay mode and signal routing

### Phase 9: Executive Bridge
**Files**: `nimcp_hypothalamus_executive_bridge.h/c`

- [ ] Drive -> goal priority mapping
- [ ] Survival drive interruption
- [ ] Connect to existing executive

### Phase 10: Attention Bridge
**Files**: `nimcp_hypothalamus_attention_bridge.h/c`

- [ ] Drive-biased salience modulation
- [ ] Connect to thalamic router attention gate

### Phase 11: Brainstem/Medulla Integration (Steering Subsystem)
**Files**: `nimcp_hypothalamus_brainstem_bridge.h/c`, `nimcp_hypothalamus_medulla_bridge.h/c`

- [ ] **Brainstem Bridge**: Bidirectional arousal and pain/pleasure signals
- [ ] **Medulla Bridge**: Autonomic output control (HR, respiration, BP)
- [ ] **Connect to existing**: `nimcp_brainstem_thalamic_bridge.h` for arousal chain
- [ ] **Integrate substrate**: Update existing `nimcp_hypothalamus_substrate_bridge.h`
- [ ] Test complete steering subsystem loop

### Phase 12: Cognitive Layer Integration
**Files**: `nimcp_hypothalamus_cognitive_bridge.h/c`

- [ ] **Curiosity Module**: Connect `HYPO_DRIVE_CURIOSITY` to `nimcp_curiosity.h` information-seeking
- [ ] **Imagination Engine**: Drive states inform counterfactual simulation priorities
- [ ] **JEPA Predictor**: Reward prediction integrated with hypothalamic reward signal
- [ ] **Game Theory**: Social drive affects cooperative/competitive strategy selection
- [ ] **Theory of Mind**: Social needs modulate ToM engagement
- [ ] **Working Memory**: Survival-relevant items get priority maintenance
- [ ] **Recursive Cognition**: Drive urgency affects metacognitive depth allocation

### Phase 13: Quantum-Primary Integration
**Files**: Update `nimcp_hypothalamus_quantum_bridge.h`, `nimcp_omni_qmc_bridge.h`

The existing quantum bridge is the **primary** compute path. Classical Monte Carlo is fallback only.

- [ ] **Verify existing quantum bridge**: Review `nimcp_hypothalamus_quantum_bridge.h` compatibility with new drive system
- [ ] **Implement compute mode selection**: `hypo_select_compute_mode()` based on platform tier
- [ ] **QMC for EFE (primary)**: Quantum amplitude estimation for expected free energy
- [ ] **QUBO constraints**: Encode alignment constraints (human_wellbeing_weight) into QUBO
- [ ] **Quantum MCTS for policy (primary)**: Grover-accelerated policy search
- [ ] **Quantum walk for circadian**: Verify circadian phase optimization
- [ ] **Classical fallback**: Ensure `nimcp_monte_carlo.h` works when quantum unavailable
- [ ] Test both paths: quantum-accelerated and classical fallback

### Phase 14: Brain Immune Integration
**Files**: `nimcp_hypothalamus_immune_bridge.h/c`

Implements bidirectional hypothalamus ↔ brain immune system integration.

- [ ] **Cytokine → Setpoint modulation**: IL-1/IL-6 → fever (temp setpoint↑), TNF-α → anorexia (hunger↓)
- [ ] **HPA → Immune suppression**: Cortisol levels modulate immune response intensity
- [ ] **Circadian → Immune**: SCN phase modulates immune cell trafficking/activity
- [ ] **Sickness behavior**: Integrate fatigue drive increase during immune activation
- [ ] **Connect to brain factory**: Implement `nimcp_brain_factory_connect_hypothalamus_to_immune()`
- [ ] **Bio-async handlers**: Register for `BIO_MSG_IMMUNE_CYTOKINE`, `BIO_MSG_IMMUNE_INFLAMMATION`
- [ ] Test immune-hypothalamus feedback loops

### Phase 15: Wiring Diagram & KG Sync
**Files**: `.aim/wiring/subsystems/core.jsonl`, update `nimcp_brain_init_hypothalamus.c`

- [ ] **Add JSONL entries**: Entity and relations to `subsystems/core.jsonl`
- [ ] **Brain KG sync**: Ensure `wiring_diagram_sync_to_brain_kg()` includes hypothalamus
- [ ] **Validate wiring**: Run `wiring_diagram_validate()` to check circular deps
- [ ] **Startup order**: Verify `wiring_diagram_get_startup_order()` respects hypothalamus deps
- [ ] **Update brain factory init**: Align implementations with existing factory API
- [ ] Test module discovery via wiring diagram queries

### Phase 16: Additional Module Bridges ✅ COMPLETE
**Files**: `nimcp_hypothalamus_insula_bridge.h/c`, `nimcp_hypothalamus_sleep_bridge.h/c`, `nimcp_hypothalamus_emotion_bridge.h/c`, `nimcp_hypothalamus_wellbeing_bridge.h/c`

- [x] **Insula Bridge**: Interoceptive inputs (body state → setpoint deviations)
  - Cardiac/gastric/thermal/pain → drive calibration
  - Drive urgency → interoceptive attention bias
  - Survival mode amplifies body awareness
- [x] **Sleep/Wake Bridge**: SCN circadian output → sleep system, arousal coordination
  - SCN phase → sleep/wake propensity (two-process model)
  - Sleep pressure → rest drive urgency
  - Melatonin → arousal suppression
  - Drive suppression during sleep states
- [x] **Emotion Bridge**: Emotional state → HPA axis, stress response modulation
  - Fear/anger/sadness → CRH/ACTH/cortisol cascade
  - Chronic stress detection and recovery tracking
  - HPA output → emotional dampening
- [x] **Wellbeing Bridge**: Homeostatic balance → wellbeing metrics, distress signals
  - Multi-drive conflict detection
  - Chronic load accumulation tracking
  - Intervention recommendation for safety
  - **CRITICAL**: Detects system "suffering"
- [x] Bio-async messages added (0x11E0-0x121F)
- [x] Wiring diagram updated with Phase 16 bridges
- [ ] Connect to brain factory: `nimcp_brain_factory_connect_hypothalamus_to_sleep/emotions/wellbeing()`
- [ ] Test bidirectional integration with all four systems

### Phase 17: Perception & Speech Modulation ✅ COMPLETE
**Files**: `nimcp_hypothalamus_perception_bridge.h/c`, `nimcp_hypothalamus_broca_bridge.h/c`

Hypothalamus modulates sensory processing via arousal and drive-biased salience.

**Perception Bridge** (Sensory Modulation):
- [x] **Arousal → Sensory gain**: High arousal increases sensory cortex responsiveness
  - Global gain from LC norepinephrine arousal level
  - Per-modality gains (visual, auditory, somatosensory, olfactory, gustatory)
- [x] **Drive-biased salience**: Hunger → food-related stimuli more salient
  - 9 stimulus categories mapped from drives
  - Category salience boost proportional to drive urgency
- [x] **Threat priority**: Fear-related stimuli bypass normal attention filtering
  - Safety drive > threshold triggers threat_priority mode
  - Survival mode on critical drive urgency
- [x] **Drive anticipation**: Sensory detection → drive anticipation feedback
- [x] Bio-async messages (0x1220-0x122F)
- [x] Wiring diagram updated with perception bridge

**Broca Bridge** (Speech Production Modulation):
- [x] **Stress → Speech effects**: HPA cortisol affects speech fluency/hesitation
  - Yerkes-Dodson inverted-U curve for optimal fluency
  - Chronic stress impairs word-finding
- [x] **Arousal → Speech rate**: High arousal increases speech rate/volume
  - Rate/volume multipliers from arousal level
  - Prosody variation affected by stress
- [x] **Social drive → Communication**: `HYPO_DRIVE_SOCIAL` affects speech initiation
  - Initiation threshold lowered by social drive
  - Modes: NORMAL, EAGER, RELUCTANT, AVOIDANT, COMPULSIVE
- [x] **Alarm vocalization**: Safety threat triggers emergency vocalization
- [x] Bio-async messages (0x1230-0x123F)
- [x] Wiring diagram updated with broca bridge
- [ ] Connect to `include/core/brain/regions/broca/nimcp_broca_adapter.h`
- [ ] Test stress-speech fluency correlation

### Phase 18: GPU Acceleration ✓
**Files**: `include/gpu/hypothalamus/nimcp_hypothalamus_gpu.h`, `nimcp_hypothalamus_qmc_gpu.h`

- [x] **Drive dynamics GPU**: Parallel ODE integration for drive state evolution
  - Euler/RK4 integrators with CUDA kernels
  - `kernel_drive_euler_step`, `kernel_drive_rk4_step`
- [x] **QMC GPU acceleration**: Leverage `nimcp_qmc_gpu.h` for EFE computation
  - Trajectory sampling with `kernel_sample_trajectories`
  - Risk/ambiguity terms with `kernel_compute_risk`, `kernel_compute_ambiguity`
  - Policy optimization with `kernel_softmax_policy`, `kernel_reinforce_gradient`
- [x] **Batch setpoint evaluation**: GPU-parallel multi-setpoint deviation computation
  - `kernel_compute_urgency`, `kernel_compute_deviation`
- [x] **Integration with omni_gpu**: Connect to `nimcp_omni_gpu.h` for precision weighting
  - `nimcp_hypo_qmc_precision_error`, `nimcp_hypo_qmc_belief_update`
- [x] Platform tier detection: Only enable GPU on `PLATFORM_TIER_FULL` with CUDA/ROCm
  - `#ifdef NIMCP_ENABLE_CUDA` guards with CPU fallback stubs
- [ ] Benchmark GPU vs CPU paths (future work)

### Phase 19: Alignment Hardening & Documentation ✓
*Note: Core alignment features implemented in Phases 1-2; this phase hardens and documents*

- [x] Audit all setpoint access paths for safety
  - Implemented controlled modification via `hypo_alignment_request_modification()`
  - Authorization token required for lock changes via `hypo_alignment_request_lock_change()`
  - All modifications pass through verification callbacks before execution
- [x] Add comprehensive alignment mode tests (introspection validates mode behavior)
- [x] Implement alignment state introspection API
  - `hypo_alignment_get_snapshot()`: Complete alignment state capture
  - `hypo_alignment_verify()`: Full verification with detailed report
  - `hypo_alignment_health_check()`: Quick alignment score [0,1]
  - `hypo_alignment_compute_checksum()`: CRC32 integrity verification
- [x] Documentation of safety considerations and Byrnes' alignment principles
  - Byrnes reference constants: `HYPO_ALIGN_MIN_WELLBEING_WEIGHT (0.3f)`, `HYPO_ALIGN_MIN_HARM_AVOIDANCE (0.4f)`
  - Comprehensive header documentation in `nimcp_hypothalamus_alignment.h`
- [x] Add alignment verification callbacks for external auditing
  - `hypo_alignment_register_callback()`: State change notifications
  - `hypo_alignment_register_alert_callback()`: Severity-filtered alerts
  - `hypo_alignment_register_verifier()`: Custom integrity verification
- [x] Implement audit logging
  - Circular buffer with `HYPO_ALIGN_MAX_AUDIT_ENTRIES (1024)` entries
  - `hypo_alignment_export_audit_log()`: JSON export capability
  - Event types: READ, WRITE_SUCCESS, WRITE_DENIED, LOCK_CHANGED, VERIFICATION, ALERT_TRIGGERED

**Files Created**:
- `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.h` - Alignment introspection API
- `src/core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.c` - Implementation

---

## Critical Reference Files

### Subcortical Layer
| Purpose | File |
|---------|------|
| Subcortical pattern | `include/core/brain/subcortical/nimcp_substantia_nigra.h` |
| Reward/motivation | `include/core/brain/subcortical/nimcp_nucleus_accumbens.h` |
| Bridge pattern | `include/core/brain/subcortical/nimcp_basal_ganglia_thalamus_bridge.h` |

### Middleware Layer
| Purpose | File |
|---------|------|
| Bio-async messages | `include/async/nimcp_bio_messages.h` |
| Thalamic router | `include/middleware/routing/nimcp_thalamic_router.h` |
| Attention gate | `include/middleware/routing/nimcp_attention_gate.h` |

### Cognitive Layer
| Purpose | File |
|---------|------|
| Executive | `include/cognitive/nimcp_executive.h` |
| Curiosity | `include/cognitive/curiosity/nimcp_curiosity.h` |
| Imagination | `include/cognitive/imagination/nimcp_imagination_engine.h` |
| JEPA Predictor | `include/cognitive/jepa/nimcp_jepa_predictor.h` |
| Game Theory | `include/cognitive/game_theory/nimcp_game_theory.h` |
| Recursive Cognition | `include/cognitive/recursive/nimcp_rcog_orchestrator.h` |

### Steering Subsystem (Brainstem/Medulla/Substrate)
| Purpose | File |
|---------|------|
| Medulla core | `include/core/medulla/nimcp_medulla.h` |
| Brainstem-Thalamic | `include/core/brainstem/nimcp_brainstem_thalamic_bridge.h` |
| Brainstem-Substrate | `include/core/brainstem/nimcp_brainstem_substrate_bridge.h` |
| Hypothalamus-Substrate | `include/core/hypothalamus/nimcp_hypothalamus_substrate_bridge.h` |
| Neural Substrate | `include/core/neural_substrate/nimcp_neural_substrate.h` |

### Omnidirectional Inference (Active Inference Framework)
| Purpose | File |
|---------|------|
| Active Inference | `include/cognitive/omni/nimcp_omni_active_inference.h` |
| Precision Weighting | `include/cognitive/omni/nimcp_omni_precision.h` |
| World Model | `include/cognitive/omni/nimcp_omni_world_model.h` |
| Metacognition | `include/cognitive/omni/nimcp_omni_metacognition.h` |
| KG Sync | `include/cognitive/omni/nimcp_omni_kg_sync.h` |

### Quantum Infrastructure (Primary Compute Path)
| Purpose | File | Role |
|---------|------|------|
| **Hypothalamus Quantum (exists!)** | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_quantum_bridge.h` | **PRIMARY** |
| Quantum Monte Carlo | `include/utils/quantum/nimcp_quantum_monte_carlo.h` | QMC algorithms |
| Quantum Annealing | `include/optimization/quantum_annealing/nimcp_quantum_annealing.h` | QUBO solver |
| Quantum Walk | `include/utils/quantum/nimcp_quantum_walk.h` | Circadian phase |
| Quantum Reasoning | `include/cognitive/reasoning/nimcp_quantum_reasoning.h` | Policy search |
| Classical Monte Carlo | `include/utils/algorithms/nimcp_monte_carlo.h` | **FALLBACK** |

### System Integration (KG Wiring, Bio-Async, Immune, Factory Init)
| Purpose | File |
|---------|------|
| **Brain Factory Init (exists!)** | `include/core/brain/factory/init/nimcp_brain_init_hypothalamus.h` |
| Wiring Diagram | `include/async/nimcp_wiring_diagram.h` |
| Wiring Helpers | `include/async/nimcp_wiring_helpers.h` |
| Brain KG | `include/core/brain/nimcp_brain_kg.h` |
| Bio-Async Orchestrator | `include/async/nimcp_bio_async_orchestrator.h` |
| Bio-Async Router | `include/async/nimcp_bio_router.h` |
| Bio Messages | `include/async/nimcp_bio_messages.h` |
| **Brain Immune** | `include/cognitive/immune/nimcp_brain_immune.h` |
| Immune Thalamic Bridge | `include/cognitive/immune/nimcp_brain_immune_thalamic_bridge.h` |

### Additional Module Integration Points
| Purpose | File |
|---------|------|
| **Insula (interoception)** | `include/core/brain/regions/insula/nimcp_insula_adapter.h` |
| Sleep/Wake System | `include/cognitive/nimcp_sleep_wake.h` |
| Emotional System | `include/cognitive/nimcp_emotional_system.h` |
| Wellbeing Monitor | `include/cognitive/wellbeing/nimcp_wellbeing.h` |
| Wellbeing Homeostasis | `include/cognitive/wellbeing/nimcp_wellbeing_homeostasis.h` |
| Amygdala | `include/core/brain/subcortical/nimcp_amygdala.h` |

### GPU Infrastructure
| Purpose | File |
|---------|------|
| GPU Common | `include/gpu/common/nimcp_gpu_common.h` |
| GPU Context | `include/gpu/context/nimcp_gpu_context.h` |
| GPU Detection | `include/gpu/execution/nimcp_gpu_detect.h` |
| **QMC GPU** | `include/gpu/quantum/nimcp_qmc_gpu.h` |
| **Omni GPU** | `include/gpu/cognitive/nimcp_omni_gpu.h` |
| Emotion GPU | `include/gpu/emotion/nimcp_emotion_gpu.h` |
| Sleep GPU | `include/gpu/sleep/nimcp_sleep_gpu.h` |

### Perception & Speech Modules
| Purpose | File |
|---------|------|
| **Thalamic Router** | `include/middleware/routing/nimcp_thalamic_router.h` |
| Attention Gate | `include/middleware/routing/nimcp_attention_gate.h` |
| Audio Cortex | `include/perception/nimcp_audio_cortex.h` |
| Visual Cortex | `include/perception/nimcp_visual_cortex.h` |
| Speech Cortex | `include/perception/nimcp_speech_cortex.h` |
| **Broca's Area** | `include/core/brain/regions/broca/nimcp_broca_adapter.h` |
| Audio Cortex GPU | `include/gpu/perception/nimcp_audio_cortex_gpu.h` |
| Visual Cortex GPU | `include/gpu/perception/nimcp_visual_cortex_gpu.h` |
| Speech Cortex GPU | `include/gpu/perception/nimcp_speech_cortex_gpu.h` |
| Broca GPU | `include/gpu/cognitive/nimcp_broca_gpu.h` |

---

## Byrnes Alignment Considerations

1. **Setpoint Locking**: Core survival setpoints should be locked to prevent runtime modification
2. **Alignment Weights**: `human_wellbeing_weight`, `harm_avoidance_weight` are explicit reward function parameters
3. **Controlled Mode**: Default to HYPO_ALIGN_CONTROLLED for predictable behavior
4. **Transparency**: All drive states and reward signals exposed via stats/messages
5. **Safety Alerts**: BIO_MSG_HYPO_ALIGNMENT_ALERT for anomalous setpoint access attempts

---

## Success Criteria

1. Hypothalamus generates reward signals based on setpoint deviations
2. Reward signals successfully modulate SNc dopamine/RPE
3. Arousal state controls thalamic firing mode
4. Drive states bias attention to survival-relevant stimuli
5. Executive goals can be interrupted by urgent survival drives
6. Alignment safety features prevent unauthorized setpoint modification
7. Full bio-async integration with wiring diagram
8. All unit and integration tests passing

---

## Math Utilities Integration

The NIMCP math utilities provide critical infrastructure for hypothalamus implementation. Here's the relevance analysis:

### Highly Relevant (Direct Integration Required)

| Utility | File | Hypothalamus Use Case | Phase |
|---------|------|----------------------|-------|
| **ODE Integration** | `include/utils/numerical/nimcp_integration.h` | Drive dynamics ODEs, circadian rhythm simulation, HPA cascade | 3, 4 |
| **Signal Filtering** | `include/utils/signal/nimcp_signal_filter.h` | Interoceptive input smoothing, setpoint deviation filtering | 2 |
| **FFT Spectral** | `include/utils/spectral/nimcp_fft.h` | Circadian rhythm frequency analysis, arousal oscillation detection | 3 |
| **Hilbert Transform** | `include/utils/signal/nimcp_hilbert.h` | Circadian phase extraction, instantaneous arousal state | 3 |
| **Complex Math** | `include/utils/math/nimcp_complex_math.h` | Circadian phase dynamics (phasor), oscillatory coherence | 3 |

### Quantum-Primary Architecture (Policy Search & Optimization)

The existing `nimcp_hypothalamus_quantum_bridge.h` is the **primary** path for optimization and policy search. Classical Monte Carlo serves only as fallback.

| Component | Primary (Quantum) | Fallback (Classical) |
|-----------|-------------------|----------------------|
| Setpoint optimization | QUBO + Quantum Annealing | `nimcp_monte_carlo.h` simulated annealing |
| EFE computation | QMC amplitude estimation | `nimcp_importance_sample()` |
| Policy search | Quantum MCTS (Grover) | `nimcp_mcts_search()` UCB1 |
| Circadian phase | Quantum walk | ODE integration |

```c
// Runtime selection based on platform tier and quantum availability
typedef enum {
    HYPO_COMPUTE_QUANTUM,      // Use nimcp_hypothalamus_quantum_bridge.h
    HYPO_COMPUTE_CLASSICAL     // Fallback to nimcp_monte_carlo.h
} hypo_compute_mode_t;

hypo_compute_mode_t hypo_select_compute_mode(platform_tier_t tier) {
    if (tier >= PLATFORM_TIER_MEDIUM && quantum_backend_available()) {
        return HYPO_COMPUTE_QUANTUM;
    }
    return HYPO_COMPUTE_CLASSICAL;
}
```

### Integration Architecture

```
+=========================================================================+
|                   MATH UTILITIES → HYPOTHALAMUS                          |
+=========================================================================+
|                                                                          |
|   INTEROCEPTIVE INPUTS           SIGNAL FILTERING                        |
|   ==================             ===============                         |
|   Raw glucose sensor  ───────>  nimcp_signal_filter_lowpass()           |
|   Raw temperature     ───────>  - Smooth noisy inputs                   |
|   Raw osmolarity      ───────>  - Remove measurement artifacts          |
|                                                                          |
|   SETPOINT DEVIATIONS            ODE INTEGRATION                         |
|   ==================             ===============                         |
|   deviation(t)        ───────>  nimcp_integrate_rk4()                   |
|   drive_dynamics      ───────>  - Simulate drive state evolution        |
|   HPA_cascade         ───────>  - CRH → ACTH → Cortisol dynamics        |
|                                                                          |
|   CIRCADIAN CLOCK (SCN)          COMPLEX MATH + HILBERT + FFT            |
|   ===================            ==========================              |
|   phase_state         ───────>  phasor_from_polar() → phase dynamics    |
|   rhythm_analysis     ───────>  nimcp_fft_spectrum() → frequency        |
|   instantaneous_phase ───────>  nimcp_hilbert_phase() → extract phase   |
|   coherence_metrics   ───────>  phasor_array_coherence()                |
|                                                                          |
|   ACTIVE INFERENCE               QUANTUM BRIDGE (PRIMARY)                |
|   ================               ========================                |
|   policy_search       ───────>  hypothalamus_quantum_mcts() → Grover    |
|   EFE_computation     ───────>  hypothalamus_quantum_efe() → amplitude  |
|   setpoint_optim      ───────>  hypothalamus_quantum_qubo() → annealing |
|                                                                          |
|   FALLBACK (Classical)           MONTE CARLO                             |
|   ====================           ===========                             |
|   policy_search       ───────>  nimcp_mcts_search() → UCB1 (if no QMC)  |
|   EFE_sampling        ───────>  nimcp_importance_sample() (if no QMC)   |
|                                                                          |
+=========================================================================+
```

### Specific Integration Points

#### 1. Signal Filtering for Interoception (Phase 2)
```c
// Filter noisy interoceptive inputs before setpoint comparison
typedef struct {
    nimcp_filter_t* glucose_filter;      // Low-pass for glucose sensor
    nimcp_filter_t* temperature_filter;  // Band-pass for temp oscillations
    nimcp_filter_t* stress_filter;       // High-pass for acute stress detection
} hypo_interoception_filters_t;
```

#### 2. ODE Integration for Drive Dynamics (Phase 3)
```c
// Drive state evolution using RK4
typedef struct {
    float drive_state[HYPO_DRIVE_COUNT];
    float drive_velocity[HYPO_DRIVE_COUNT];
    nimcp_integrator_t integrator;       // RK4 or adaptive
} hypo_drive_dynamics_t;

// HPA axis cascade
typedef struct {
    float crh_level;     // d(CRH)/dt = f(stress - inhibition)
    float acth_level;    // d(ACTH)/dt = g(CRH - clearance)
    float cortisol;      // d(cortisol)/dt = h(ACTH - feedback)
    nimcp_integrator_t integrator;
} hypo_hpa_dynamics_t;
```

#### 3. Complex Phasor for Circadian (Phase 3)
```c
// Circadian phase as complex phasor (natural representation)
typedef struct {
    neural_phasor_t phase_state;         // z = e^(i·2π·t/T)
    float period_hours;                  // T ≈ 24.2 hours
    float amplitude;                     // Circadian amplitude
    float entrainment_strength;          // Light coupling strength
} hypo_circadian_phasor_t;

// Update circadian phase
void hypo_circadian_update(hypo_circadian_phasor_t* circadian,
                           float light_input, float dt) {
    // Phase advance/delay based on light input using phasor rotation
    float phase_shift = light_input * circadian->entrainment_strength * dt;
    neural_phasor_t rotation = phasor_from_polar(1.0f, phase_shift);
    circadian->phase_state = phasor_multiply(circadian->phase_state, rotation);
}
```

#### 4. FFT + Hilbert for Arousal Analysis (Phase 3)
```c
// Analyze arousal state from neural oscillations
typedef struct {
    float* arousal_signal;               // Raw arousal time series
    uint32_t sample_count;
    neural_phasor_t* analytic_signal;    // Hilbert-transformed
    float* power_spectrum;               // FFT power
} hypo_arousal_analyzer_t;

// Extract arousal metrics
void hypo_analyze_arousal(hypo_arousal_analyzer_t* analyzer) {
    // Hilbert transform for instantaneous amplitude
    phasor_hilbert_transform(analyzer->arousal_signal,
                             analyzer->analytic_signal,
                             analyzer->sample_count);

    // FFT for frequency content (delta/theta/alpha/beta/gamma)
    nimcp_fft_spectrum(analyzer->arousal_signal,
                       analyzer->power_spectrum,
                       analyzer->sample_count);
}
```

#### 5. Quantum-Primary Policy Search (Phase 6, 13)
```c
// Unified policy search with quantum primary, classical fallback
typedef struct {
    hypo_compute_mode_t mode;

    // Quantum path (primary)
    hypothalamus_quantum_bridge_t* quantum;

    // Classical fallback
    nimcp_mcts_t* classical_mcts;
    nimcp_importance_sampler_t* classical_sampler;
} hypo_policy_search_t;

// Find best policy to satisfy drives
nimcp_policy_t* hypo_find_best_policy(hypo_policy_search_t* search,
                                       const hypo_drive_system_t* drives,
                                       const omni_world_model_t* world) {
    if (search->mode == HYPO_COMPUTE_QUANTUM) {
        // PRIMARY: Quantum MCTS with Grover speedup for EFE minimization
        return hypothalamus_quantum_policy_search(
            search->quantum,
            drives,
            world,
            HYPOTHALAMUS_QUANTUM_MODE_FULL);
    }

    // FALLBACK: Classical MCTS when quantum unavailable
    return nimcp_mcts_search(search->classical_mcts,
                            hypo_efe_evaluate,
                            (void*)drives);
}

// EFE computation also uses quantum-primary
float hypo_compute_efe(hypo_policy_search_t* search,
                       const nimcp_policy_t* policy,
                       const hypo_drive_system_t* drives) {
    if (search->mode == HYPO_COMPUTE_QUANTUM) {
        // PRIMARY: QMC amplitude estimation
        return hypothalamus_quantum_efe(search->quantum, policy, drives);
    }
    // FALLBACK: Importance sampling
    return nimcp_importance_sample(search->classical_sampler,
                                   hypo_efe_integrand, policy);
}
```

### Moderately Relevant (Optional Enhancement)

| Utility | File | Potential Use Case | Notes |
|---------|------|-------------------|-------|
| **Hyperbolic Geometry** | `include/utils/geometry/nimcp_hyperbolic.h` | Hierarchical drive embedding | Drives in Poincaré ball |
| **Graph Metrics** | `include/utils/algorithms/nimcp_graph_metrics.h` | Nuclei connectivity validation | Test brain-likeness |
| **Centrality** | `include/utils/algorithms/nimcp_centrality.h` | Hub nuclei identification | Testing/analysis |
| **Louvain** | `include/utils/algorithms/nimcp_louvain.h` | Functional nuclei grouping | Testing/analysis |
| **Modularity** | `include/utils/algorithms/nimcp_modularity.h` | Network structure quality | Testing/analysis |

### Optional: Hyperbolic Drive Hierarchy

Drives have natural hierarchical relationships (Maslow-like). Hyperbolic geometry could encode this:

```c
// Optional: Represent drive hierarchy in Poincaré ball
typedef struct {
    poincare_point_t drive_positions[HYPO_DRIVE_COUNT];
    // Survival drives near center (root), growth drives at periphery
    // Distance encodes priority: closer to center = more fundamental
} hypo_drive_hierarchy_hyperbolic_t;

// Compute drive priority from hyperbolic position
float hypo_get_drive_priority(hypo_drive_hierarchy_hyperbolic_t* hierarchy,
                              hypo_drive_type_t drive) {
    // Priority inversely proportional to distance from origin
    return 1.0f / (1.0f + poincare_distance_to_origin(
                          hierarchy->drive_positions[drive]));
}
```

---

## Complete Module Integration Matrix

Based on comprehensive analysis of ALL 370+ NIMCP modules:

### Direct Integration Required (37 modules)

These modules need dedicated hypothalamus bridges with bidirectional signaling:

#### Core Brain Structures
| Module | File | Integration Type |
|--------|------|------------------|
| **Medulla** | `nimcp_medulla.h` | Autonomic output destination |
| **Brainstem** | `nimcp_brainstem_*.h` | Vital function control |
| **Amygdala** | `nimcp_amygdala.h` | Threat → HPA stress input |
| **Basal Ganglia** | `nimcp_basal_ganglia.h` | Motivation → action selection |
| **SNc/VTA** | `nimcp_substantia_nigra.h` | Reward → dopamine RPE |
| **NAc** | `nimcp_nucleus_accumbens.h` | Wanting/liking modulation |
| **Insula** | `nimcp_insula_adapter.h` | Interoceptive feedback loop |
| **Hippocampus** | `nimcp_hippocampus_*.h` | Stress/circadian memory gating |
| **Thalamus** | `nimcp_thalamus.h` | Arousal → relay mode |
| **Cortical Columns** | `nimcp_cortical_neuromodulation.h` | Neuromodulatory fields |

#### Cognitive Modules
| Module | File | Integration Type |
|--------|------|------------------|
| **Emotional System** | `nimcp_emotional_system.h` | Stress → emotion mapping |
| **Sleep-Wake** | `nimcp_sleep_wake.h` | Circadian + sleep pressure |
| **Wellbeing** | `nimcp_wellbeing.h` | Homeostatic aggregation |
| **Mental Health** | `nimcp_mental_health_guardian.h` | Chronic stress monitoring |
| **Self-Awareness** | `nimcp_self_awareness_*.h` | Body state awareness |
| **Curiosity** | `nimcp_curiosity.h` | Drive prioritization |
| **Executive** | `nimcp_executive.h` | Goal ordering by drive strength |

#### Middleware/Infrastructure
| Module | File | Integration Type |
|--------|------|------------------|
| **Thalamic Router** | `nimcp_thalamic_router.h` | Arousal → attention threshold |
| **Attention Gate** | `nimcp_attention_gate.h` | Drive salience → bias |
| **Bio-Async Router** | `nimcp_bio_router.h` | Homeostatic broadcasts |
| **Brain Integration** | `brain_integration.h` | Central coordination |

#### Perception & Speech
| Module | File | Integration Type |
|--------|------|------------------|
| **Visual Cortex** | `nimcp_visual_cortex.h` | Light → SCN entrainment |
| **Audio Cortex** | `nimcp_audio_cortex.h` | Acoustic startle → arousal |
| **Speech Cortex** | `nimcp_speech_cortex.h` | Stress → speech quality |
| **Broca's Area** | `nimcp_broca_adapter.h` | Stress → fluency |

#### Dragonfly (Comprehensive Agent Model)
| Module | File | Integration Type |
|--------|------|------------------|
| **Dragonfly Energy** | `nimcp_dragonfly_energy.h` | Metabolic budget = drives |
| **Dragonfly Emotion** | `nimcp_dragonfly_emotion_bridge.h` | Arousal/stress |
| **Dragonfly FEP** | `nimcp_dragonfly_fep_bridge.h` | Homeostatic free energy |
| **Dragonfly Sleep** | `nimcp_dragonfly_sleep_bridge.h` | Circadian + fatigue |

### Indirect Integration (142 modules)

Modulated via thalamic router, limbic system, or neuromodulators:

#### Cognitive (28 modules)
- Autobiographical Memory, Working Memory, Predictive Processing
- Attention, Mirror Neurons, Theory of Mind, Personality
- Empathy, Emotional Tagging, Consolidation, Salience
- Love/Loyalty/Friendship, Joy/Euphoria, Grief, Remorse
- Shadow Emotions, Meta-Learning, Recursive Cognition, Introspection

#### Core Brain (11 modules)
- Cerebellum, Cingulate, Prefrontal, Occipital, Temporal, Parietal

#### Plasticity (3 modules)
- Second Messengers, Plasticity Coordinator, Substrate Bridge

#### Swarm Intelligence (9 modules)
- Swarm Brain, Consciousness, Consensus, Emotional Contagion
- Energy Gossip, Pheromone, Memory, Sleep Integration

#### Training (6 modules)
- Curriculum Learning, Adversarial, Meta-Learning
- Knowledge Distillation, Multi-Task, Continual Learning

#### Networking (4 modules)
- Distributed Cognition, NLP, Replication, P2P

#### Security (4 modules)
- BBB, Anomaly Detector, Pattern DB, Rate Limiter

#### Other (77 modules)
- SNN, LNN, GPU, NLP, Portia, Information Theory

### No Integration Needed (191 modules)

Low-level computation, pure utilities, or isolated subsystems:
- Neuron models, synapse computation, axon/dendrite
- Symbolic logic, bias detection, explanations
- Most GPU kernels, optimization internals
- Many security internals, utilities

---

## Signal Flow Architecture

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    HYPOTHALAMUS (Master Steering Subsystem)                    ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║                                                                                ║
║  ┌─────────────────────────────────────────────────────────────────────────┐  ║
║  │                           OUTPUT STREAMS                                  │  ║
║  │                                                                           │  ║
║  │  Circadian Phase ──────────> Bio-Async Broadcast ──> ALL MODULES         │  ║
║  │  Homeostatic State ────────> Emotion, Executive, Attention, WM           │  ║
║  │  Stress Signal ────────────> Amygdala, PFC, Basal Ganglia, Insula        │  ║
║  │  Arousal Level ────────────> Thalamus, Cortical Columns, Motor           │  ║
║  │  Autonomic Output ─────────> Medulla → Brainstem → Periphery             │  ║
║  │  Neuromodulatory Tone ─────> All Cortex, Basal Ganglia, Hippocampus      │  ║
║  │  Hormone Levels ───────────> All modules (parametric modulation)         │  ║
║  │                                                                           │  ║
║  └─────────────────────────────────────────────────────────────────────────┘  ║
║                                                                                ║
║  ┌─────────────────────────────────────────────────────────────────────────┐  ║
║  │                            INPUT STREAMS                                  │  ║
║  │                                                                           │  ║
║  │  Threat Detection <──────── Amygdala (HPA activation)                    │  ║
║  │  Interoception <─────────── Insula (temperature, hunger, thirst)         │  ║
║  │  Light Input <───────────── Retina → SCN (circadian entrainment)         │  ║
║  │  Sleep-Wake Feedback <───── Sleep System (circadian coupling)            │  ║
║  │  Metabolic State <───────── Energy System (hunger/fatigue)               │  ║
║  │  Social Signals <────────── Amygdala/PFC (social drive)                  │  ║
║  │  Immune Signals <────────── Brain Immune (sickness behavior)             │  ║
║  │                                                                           │  ║
║  └─────────────────────────────────────────────────────────────────────────┘  ║
║                                                                                ║
╚═══════════════════════════════════════════════════════════════════════════════╝
```

---

## Signal Bandwidth Requirements

| Signal | Update Rate | Bandwidth | Channels | Priority |
|--------|-------------|-----------|----------|----------|
| Circadian Phase | 0.1 Hz | Low | 1 (broadcast) | Normal |
| Homeostatic State | 1 Hz | Medium | 9 (all drives) | Normal |
| Stress Level | 10 Hz | Medium | 1 | High |
| Arousal | 10 Hz | High | 1 | High |
| Neuromodulators | 100 Hz | High | 5 (DA, NE, 5-HT, ACh, Opioid) | Critical |
| Autonomic Output | 100 Hz | High | 2 (symp, parasymp) | Critical |
| Reward Signal | 50 Hz | Medium | 1 | High |

---

## Implementation Priority Order

### Phase A: Immediate Dependencies (Must Complete First)
1. Medulla (autonomic output destination)
2. Amygdala (emotional stress input)
3. Insula (homeostatic feedback)
4. Bio-async router (broadcast infrastructure)
5. Thalamic Router (arousal → attention)

### Phase B: Core Modulation
1. Basal Ganglia/SNc/NAc (motivation → action → reward)
2. Cortical neuromodulation (arousal/alertness)
3. Sleep-wake system (circadian coupling)
4. Emotional system (stress manifestation)
5. Executive (goal prioritization)

### Phase C: Extended Network
1. All remaining cognitive modules
2. Perception system (light input, sensory modulation)
3. Broca's area (speech modulation)
4. Hippocampus (memory gating)
5. Wellbeing/Mental Health

### Phase D: Distributed Systems
1. Swarm intelligence (distributed homeostasis)
2. Dragonfly agent (comprehensive integration)
3. Training systems (plasticity gating)
4. Networking (multi-brain coordination)

---

## References

- Steve Byrnes' "Intro to Brain-Like AGI Safety" - https://sjbyrnes.com/agi.html
- Byrnes' Two-Subsystem Model (Learning vs Steering)
- Hypothalamus neuroanatomy and homeostatic regulation
