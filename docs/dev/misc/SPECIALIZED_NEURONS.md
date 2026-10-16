# NIMCP Specialized Neuron Types Reference

## Overview

NIMCP implements biologically-inspired specialized neurons matched to specific computational tasks in different cortices and cognitive modules. This document catalogs available neuron types and proposes additional types for enhanced biological realism.

---

## Currently Implemented Neuron Types

### Generic Models (0-99)
| Type ID | Name | Description | Use Case |
|---------|------|-------------|----------|
| 0 | `NEURON_EXCITATORY` | Generic excitatory neuron | Backward compatibility, general processing |
| 1 | `NEURON_INHIBITORY` | Generic inhibitory neuron | Backward compatibility, general inhibition |
| 2 | `NEURON_GENERIC_LIF` | Leaky Integrate-and-Fire | Standard computational model |
| 3 | `NEURON_GENERIC_IZHIKEVICH` | Izhikevich spiking model | Reproduces 20+ firing patterns |

**Parameters:**
- **LIF**: `tau_membrane`, `rest_potential`, `threshold`, `reset_potential`, `refractory_period`
- **Izhikevich**: `a` (recovery), `b` (sensitivity), `c` (reset), `d` (recovery reset)

### Visual Cortex (V1) Neurons (100-199)
| Type ID | Name | Biological Basis | Computation |
|---------|------|------------------|-------------|
| 100 | `NEURON_V1_SIMPLE_CELL` | Hubel & Wiesel (1962) | Gabor filter, oriented edge detection |
| 101 | `NEURON_V1_COMPLEX_CELL` | Hubel & Wiesel (1962) | Phase-invariant pooling over simple cells |
| 102 | `NEURON_VISUAL_ORIENTATION` | V1 orientation columns | Orientation selectivity tuning |
| 103 | `NEURON_VISUAL_DIRECTION` | MT/V5 area | Directional motion detection |
| 150 | `NEURON_PYRAMIDAL_L23` | Layer 2/3 pyramidal | Lateral connections, feedback |
| 151 | `NEURON_PYRAMIDAL_L5_THICK` | Layer 5 thick-tufted | Output to subcortical structures |
| 152 | `NEURON_PYRAMIDAL_L6` | Layer 6 pyramidal | Thalamic feedback |

**Parameters:**
- **V1 Simple**: `orientation`, `spatial_frequency`, `phase`, `aspect_ratio`, `sigma`, `on_center`
- **V1 Complex**: `orientation`, `direction_selectivity`, `surround_suppression`, `pooling_size`

### Auditory Cortex (A1) Neurons (200-299)
| Type ID | Name | Biological Basis | Computation |
|---------|------|------------------|-------------|
| 200 | `NEURON_A1_FREQUENCY_TUNED` | Schreiner et al. (2000) | Tonotopic frequency selectivity |
| 201 | `NEURON_A1_COINCIDENCE_DETECTOR` | Jeffress (1948) | Temporal precision for ITD |
| 202 | `NEURON_AUDITORY_ONSET` | A1 onset detectors | Transient response to sound onset |

**Parameters:**
- **Frequency**: `center_frequency`, `q_factor`, `bandwidth`, `integration_window`, `adaptation_rate`
- **Coincidence**: `integration_window`, `temporal_precision`, `threshold`, `decay_rate`

### Motor Neurons (250-299)
| Type ID | Name | Biological Basis | Use Case |
|---------|------|------------------|----------|
| 250 | `NEURON_MOTOR_ALPHA` | Spinal motoneuron | Direct muscle control |
| 251 | `NEURON_MOTOR_PATTERN_GEN` | CPG circuits | Rhythmic motor patterns |

### Cognitive/Metacognitive Neurons (300-399)
| Type ID | Name | Biological Basis | Computation |
|---------|------|------------------|-------------|
| 300 | `NEURON_METACOGNITIVE` | Fleming & Dolan (2012) | Uncertainty estimation, confidence |
| 301 | `NEURON_EXECUTIVE_CONTROL` | Miller & Cohen (2001) | Goal maintenance, top-down modulation |

**Parameters:**
- **Metacognitive**: `confidence_threshold`, `uncertainty_window`, `uncertainty_beta`, `history_size`
- **Executive**: `goal_maintenance`, `modulation_strength`, `decay_rate`, `threshold_boost`, `delay_activity`

---

## Proposed Additional Neuron Types

### Speech Cortex (STG/Wernicke) - Range 400-499

#### Phoneme-Selective Neurons (400-429)
```c
NEURON_STG_PHONEME_DETECTOR = 400  // Categorical phoneme recognition
```
**Biological Basis:** Superior temporal gyrus contains phoneme-selective neurons (Chang et al., 2010)

**Parameters:**
```c
typedef struct {
    phoneme_t preferred_phoneme;    // 44 phonemes (IPA)
    float tuning_width;             // Tuning curve sharpness
    float temporal_window;          // Integration window (20-50ms)
    float voicing_sensitivity;      // Voiced vs. unvoiced sensitivity
    float formant_weights[4];       // F1-F4 weighting
} stg_phoneme_params_t;
```

**Computation:**
- Template matching against phoneme prototypes
- Formant-based feature extraction (F1/F2 for vowels)
- Temporal integration over phoneme duration

#### Prosody Neurons (430-439)
```c
NEURON_STG_PITCH_CONTOUR = 430     // F0 tracking and intonation
NEURON_STG_STRESS_DETECTOR = 431   // Lexical stress patterns
NEURON_STG_RHYTHM = 432            // Speech rhythm/timing
```

**Biological Basis:** Right hemisphere STG for prosody (Ross, 1981)

**Parameters:**
```c
typedef struct {
    float f0_min;                   // Min fundamental frequency (75 Hz)
    float f0_max;                   // Max fundamental frequency (300 Hz)
    float integration_window;       // Pitch averaging window (40-80ms)
    float contour_sensitivity;      // Sensitivity to pitch changes
} stg_pitch_params_t;
```

#### Lexical Access Neurons (440-449)
```c
NEURON_WERNICKE_WORD_DETECTOR = 440  // Word-level recognition
NEURON_WERNICKE_SEMANTIC = 441       // Semantic integration
```

**Biological Basis:** Wernicke's area for word comprehension (Wernicke, 1874)

**Parameters:**
```c
typedef struct {
    uint32_t lexicon_size;          // Max vocabulary size
    float activation_threshold;      // Word recognition threshold
    float phoneme_sequence_weight;   // Weight on phoneme ordering
    float frequency_bias;            // High-frequency word bias
} wernicke_params_t;
```

### Ethics Engine Neurons - Range 500-549

#### Mirror Neurons (500-509)
```c
NEURON_MIRROR = 500                // Action observation/execution matching
NEURON_EMPATHY = 501               // Affective empathy
```

**Biological Basis:** Mirror neurons in premotor/parietal cortex (Rizzolatti & Craighero, 2004)

**Parameters:**
```c
typedef struct {
    float observation_gain;         // Strength of observed action response
    float execution_threshold;      // Threshold for action execution
    float emotional_coupling;       // Coupling to emotional state
    float perspective_taking;       // Ability to take other's viewpoint
} mirror_neuron_params_t;
```

#### Theory of Mind Neurons (510-519)
```c
NEURON_TOM_BELIEF = 510           // Belief state representation
NEURON_TOM_INTENTION = 511        // Intention inference
```

**Biological Basis:** Medial prefrontal cortex, temporoparietal junction (Frith & Frith, 2006)

**Parameters:**
```c
typedef struct {
    uint32_t agent_models;          // Number of agent models to maintain
    float belief_update_rate;       // Rate of belief updating
    float false_belief_capacity;    // Track false beliefs
    float intention_depth;          // Depth of intention reasoning
} tom_params_t;
```

#### Moral Value Neurons (520-529)
```c
NEURON_ETHICS_HARM = 520          // Harm/care foundation
NEURON_ETHICS_FAIRNESS = 521      // Fairness/reciprocity
NEURON_ETHICS_AUTHORITY = 522     // Authority/respect
NEURON_ETHICS_SANCTITY = 523      // Sanctity/purity
```

**Biological Basis:** Moral foundations theory (Haidt, 2007), ventromedial PFC

**Parameters:**
```c
typedef struct {
    float sensitivity;              // Sensitivity to violations
    float cultural_weight;          // Cultural norm weighting
    float threshold;                // Violation detection threshold
    float punishment_strength;      // Inhibition strength on violations
} ethics_foundation_params_t;
```

### Salience/Attention Neurons - Range 550-599

#### Surprise/Novelty Detectors (550-559)
```c
NEURON_SURPRISE = 550             // Prediction error magnitude
NEURON_NOVELTY = 551              // Unfamiliarity detection
NEURON_URGENCY = 552              // Threat/priority detection
```

**Biological Basis:** Locus coeruleus, amygdala, anterior cingulate (Sara & Bouret, 2012)

**Parameters:**
```c
typedef struct {
    float prediction_window;        // Historical prediction window
    float surprise_threshold;       // Threshold for surprise response
    float adaptation_rate;          // Rate of habituation
    float alerting_gain;            // Gain modulation on surprise
    float norepinephrine_scaling;   // NE-like arousal scaling
} surprise_params_t;
```

#### Attention Control (560-569)
```c
NEURON_ATTENTION_TOP_DOWN = 560   // Goal-directed attention
NEURON_ATTENTION_BOTTOM_UP = 561  // Stimulus-driven attention
NEURON_ATTENTION_GATE = 562       // Attentional gating
```

**Biological Basis:** Frontal eye fields, parietal cortex (Corbetta & Shulman, 2002)

**Parameters:**
```c
typedef struct {
    float attentional_gain;         // Gain on attended inputs
    float suppression_strength;     // Suppression of unattended
    float shift_cost;               // Cost of attention shifts
    float spatial_window;           // Spatial attention window
} attention_params_t;
```

### Curiosity/Exploration Neurons - Range 600-649

#### Intrinsic Motivation (600-609)
```c
NEURON_CURIOSITY_EPISTEMIC = 600  // Knowledge-seeking curiosity
NEURON_CURIOSITY_PERCEPTUAL = 601 // Perceptual exploration
```

**Biological Basis:** Dopaminergic reward prediction error (Schultz, 2015)

**Parameters:**
```c
typedef struct {
    float information_gain_weight;  // Weight on information gain
    float uncertainty_bonus;        // Bonus for uncertain states
    float exploration_temperature;  // Exploration vs. exploitation
    float learning_progress_bonus;  // Reward for learning progress
} curiosity_params_t;
```

#### Knowledge Gap Detection (610-619)
```c
NEURON_KNOWLEDGE_GAP = 610        // Detect missing knowledge
NEURON_QUESTION_GENERATOR = 611   // Generate questions
```

**Parameters:**
```c
typedef struct {
    float gap_threshold;            // Threshold for knowledge gap
    uint32_t question_types;        // Types of questions to generate
    float priority_weighting;       // Question priority weighting
    float learning_potential;       // Expected learning value
} knowledge_gap_params_t;
```

### Symbolic Logic Neurons - Range 650-699

#### Logical Inference (650-669)
```c
NEURON_LOGIC_AND = 650            // Conjunction operator
NEURON_LOGIC_OR = 651             // Disjunction operator
NEURON_LOGIC_NOT = 652            // Negation operator
NEURON_LOGIC_IMPLIES = 653        // Implication operator
```

**Biological Basis:** Prefrontal cortex for abstract reasoning (Bunge et al., 2005)

**Parameters:**
```c
typedef struct {
    float threshold;                // Logical threshold (e.g., 0.5 for AND)
    float fuzzy_logic;              // Fuzzy logic parameter [0,1]
    float confidence_propagation;   // How confidence propagates
    float truth_decay;              // Decay of truth values over time
} logic_operator_params_t;
```

#### Variable Binding (670-679)
```c
NEURON_VARIABLE_BINDER = 670      // Variable binding/unification
NEURON_PATTERN_MATCHER = 671      // Pattern matching
```

**Parameters:**
```c
typedef struct {
    uint32_t max_bindings;          // Max simultaneous bindings
    float binding_strength;         // Strength of variable binding
    float unification_threshold;    // Threshold for successful unification
    float context_sensitivity;      // Context-dependent binding
} variable_binding_params_t;
```

### Memory/Consolidation Neurons - Range 700-749

#### Hippocampal-like Neurons (700-719)
```c
NEURON_PLACE_CELL = 700           // Spatial encoding
NEURON_GRID_CELL = 701            // Hexagonal grid representation
NEURON_TIME_CELL = 702            // Temporal encoding
NEURON_CONCEPT_CELL = 703         // Abstract concept encoding
```

**Biological Basis:** Hippocampal formation (O'Keefe & Nadel, 1978; Moser et al., 2008)

**Parameters:**
```c
typedef struct {
    float field_size;               // Receptive field size
    float spatial_frequency;        // Grid spacing (for grid cells)
    float temporal_resolution;      // Temporal precision (for time cells)
    float plasticity_rate;          // Rate of place field formation
} hippocampal_params_t;
```

#### Memory Consolidation (720-729)
```c
NEURON_CONSOLIDATION = 720        // Memory consolidation controller
NEURON_REPLAY = 721               // Experience replay
```

**Parameters:**
```c
typedef struct {
    float consolidation_rate;       // Rate of memory consolidation
    float replay_probability;       // Probability of replay events
    float sleep_gain;               // Consolidation during "sleep"
    float priority_weighting;       // Priority-based consolidation
} consolidation_params_t;
```

---

## Implementation Priority

### High Priority (Immediate Use)
1. **STG_PHONEME_DETECTOR** - Essential for speech processing
2. **WERNICKE_WORD_DETECTOR** - Lexical access
3. **SURPRISE/NOVELTY** - Salience computation
4. **MIRROR_NEURON** - Ethics/empathy

### Medium Priority (Enhanced Functionality)
5. **STG_PITCH_CONTOUR** - Prosody processing
6. **ATTENTION_TOP_DOWN** - Cognitive control
7. **CURIOSITY_EPISTEMIC** - Learning drive
8. **PLACE_CELL** - Spatial/episodic memory

### Low Priority (Future Enhancement)
9. **LOGIC_OPERATORS** - Symbolic reasoning
10. **TOM_BELIEF** - Theory of mind
11. **GRID_CELL** - Spatial navigation

---

## Usage Example

### Creating Specialized Neurons for Speech Cortex

```c
// Allocate phoneme detectors for vowels
stg_phoneme_params_t vowel_detector = {
    .preferred_phoneme = PHONEME_IY,  // /i/ vowel
    .tuning_width = 0.3f,             // Moderate tuning
    .temporal_window = 30.0f,         // 30ms integration
    .voicing_sensitivity = 0.8f,      // High voicing sensitivity
    .formant_weights = {1.0f, 1.0f, 0.5f, 0.3f}  // F1/F2 most important
};

// Create neuron with specialized type
neuron_t* phoneme_neuron = neuron_create_typed(
    NEURON_STG_PHONEME_DETECTOR,
    &vowel_detector,
    sizeof(stg_phoneme_params_t)
);

// Use in speech cortex
speech_cortex_add_neuron(cortex, phoneme_neuron, LAYER_STG);
```

### Creating Mirror Neurons for Ethics Engine

```c
mirror_neuron_params_t mirror_params = {
    .observation_gain = 0.7f,         // Strong observation response
    .execution_threshold = 0.5f,      // Moderate execution threshold
    .emotional_coupling = 0.8f,       // High emotional coupling
    .perspective_taking = 0.6f        // Moderate perspective taking
};

neuron_t* mirror = neuron_create_typed(
    NEURON_MIRROR,
    &mirror_params,
    sizeof(mirror_neuron_params_t)
);

ethics_engine_add_neuron(engine, mirror, EMPATHY_LAYER);
```

---

## Biological Correspondence Table

| Cortex/Module | Brain Region | Specialized Neurons | Key References |
|---------------|--------------|---------------------|----------------|
| Visual Cortex | V1, V2, MT | Simple/Complex cells, Pyramidal | Hubel & Wiesel (1962) |
| Audio Cortex | A1, Belt/Parabelt | Frequency-tuned, Coincidence | Schreiner et al. (2000) |
| Speech Cortex | STG, Wernicke | Phoneme, Prosody, Lexical | Chang et al. (2010) |
| Ethics Engine | vmPFC, TPJ | Mirror, ToM, Moral value | Rizzolatti (2004), Haidt (2007) |
| Salience | LC, Amygdala, ACC | Surprise, Novelty, Urgency | Sara & Bouret (2012) |
| Curiosity | VTA, Hippocampus | Epistemic, Knowledge gap | Schultz (2015) |
| Logic | dlPFC, Parietal | Logical operators, Variable binding | Bunge et al. (2005) |
| Memory | Hippocampus, Entorhinal | Place, Grid, Time, Concept | O'Keefe & Nadel (1978) |

---

## References

1. Hubel, D. H., & Wiesel, T. N. (1962). Receptive fields, binocular interaction and functional architecture in the cat's visual cortex. *Journal of Physiology*, 160(1), 106-154.

2. Izhikevich, E. M. (2003). Simple model of spiking neurons. *IEEE Transactions on Neural Networks*, 14(6), 1569-1572.

3. Schreiner, C. E., Read, H. L., & Sutter, M. L. (2000). Modular organization of frequency integration in primary auditory cortex. *Annual Review of Neuroscience*, 23, 501-529.

4. Chang, E. F., et al. (2010). Categorical speech representation in human superior temporal gyrus. *Nature Neuroscience*, 13(11), 1428-1432.

5. Rizzolatti, G., & Craighero, L. (2004). The mirror-neuron system. *Annual Review of Neuroscience*, 27, 169-192.

6. Haidt, J. (2007). The new synthesis in moral psychology. *Science*, 316(5827), 998-1002.

7. Sara, S. J., & Bouret, S. (2012). Orienting and reorienting: the locus coeruleus mediates cognition through arousal. *Neuron*, 76(1), 130-141.

8. Schultz, W. (2015). Neuronal reward and decision signals: from theories to data. *Physiological Reviews*, 95(3), 853-951.

9. O'Keefe, J., & Nadel, L. (1978). *The Hippocampus as a Cognitive Map*. Oxford University Press.

10. Moser, E. I., Kropff, E., & Moser, M. B. (2008). Place cells, grid cells, and the brain's spatial representation system. *Annual Review of Neuroscience*, 31, 69-89.

---

**Last Updated:** 2025-11-08
**Version:** NIMCP 2.7.0 Phase 8.8
