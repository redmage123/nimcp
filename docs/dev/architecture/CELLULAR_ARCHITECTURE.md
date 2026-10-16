# NIMCP Cellular Architecture: What Cells Comprise Each Module

**Version 2.7.0 Phase 8.8**
**Author**: NIMCP Development Team
**Date**: 2025-11-08

## Overview

This document describes the cellular composition of each cortex and cognitive module in NIMCP. When you instantiate a module or cortex, you're creating a complex network of specialized neurons, synapses, and glial cells that work together to perform specific computational functions.

---

## Table of Contents

1. [Visual Cortex (V1)](#visual-cortex-v1)
2. [Audio Cortex (A1)](#audio-cortex-a1)
3. [Speech Cortex (STG/Wernicke)](#speech-cortex-stgwernicke)
4. [Cognitive Modules](#cognitive-modules)
5. [Network Architecture](#network-architecture)
6. [Cellular Interactions](#cellular-interactions)

---

## Visual Cortex (V1)

### Primary Function
Edge detection, orientation selectivity, spatial feature extraction

### Cellular Composition

#### Neurons (Biologically-inspired)
- **V1 Simple Cells** (`NEURON_V1_SIMPLE_CELL` = 100)
  - **Function**: Oriented edge detection via Gabor filters
  - **Parameters**:
    - `orientation` (0-180°): Preferred edge angle
    - `spatial_frequency` (cycles/px): Scale of detection
    - `phase` (0-2π): Even vs odd symmetric
    - `aspect_ratio`: Ellipse shape (1.0-2.0)
  - **Biological Basis**: Hubel & Wiesel (1962) - Nobel Prize winning discovery
  - **Count**: Typically 32-64 per orientation (8 orientations × 4-8 spatial frequencies)

- **V1 Complex Cells** (`NEURON_V1_COMPLEX_CELL` = 101)
  - **Function**: Phase-invariant edge detection
  - **Parameters**:
    - `orientation`: Preferred edge angle
    - `receptive_field_size`: Spatial extent
    - `direction_selective`: Motion sensitivity
  - **Biological Basis**: Pool outputs from multiple simple cells
  - **Count**: Typically 16-32 per orientation

- **Pyramidal Neurons Layer 2/3** (`NEURON_PYRAMIDAL_L23` = 106)
  - **Function**: Local recurrent processing, lateral connections
  - **Parameters**:
    - `apical_dendrite_length`: Long-range input integration
    - `basal_dendrite_complexity`: Local connectivity
  - **Count**: 200-500 neurons (dense local network)

- **Orientation-Selective Neurons** (`NEURON_VISUAL_ORIENTATION` = 102)
  - **Function**: Sharply tuned orientation preference
  - **Parameters**:
    - `preferred_orientation`: Peak response angle
    - `tuning_width` (σ): Sharpness (10-30°)
  - **Count**: 64-128 neurons covering 180° orientation space

- **Direction-Selective Neurons** (`NEURON_VISUAL_DIRECTION` = 103)
  - **Function**: Motion detection (leftward/rightward/upward/downward)
  - **Parameters**:
    - `preferred_direction` (0-360°): Motion angle
    - `speed_tuning`: Preferred velocity
  - **Count**: 32-64 neurons (8 directions × 4-8 speeds)

#### Synapses
- **Excitatory Synapses** (`SYNAPSE_TYPE_EXCITATORY`)
  - **Function**: Forward propagation, feedforward connections
  - **Properties**: Fast AMPA-like kinetics (τ = 2-5ms)
  - **Count**: 5,000-20,000 (dense within-layer connectivity)

- **Inhibitory Synapses** (`SYNAPSE_TYPE_INHIBITORY`)
  - **Function**: Surround suppression, normalization
  - **Properties**: GABAergic, slower kinetics (τ = 10-20ms)
  - **Count**: 1,000-5,000 (basket cells providing lateral inhibition)

- **Plastic Synapses** (`SYNAPSE_TYPE_PLASTIC`)
  - **Function**: Learning, feature adaptation
  - **Plasticity**: STDP + Hebbian learning
  - **Count**: 2,000-10,000 (experience-dependent refinement)

#### Glial Cells
- **Protoplasmic Astrocytes** (`ASTROCYTE_TYPE_PROTOPLASMIC`)
  - **Function**: Neurotransmitter reuptake, K+ buffering
  - **Coverage**: 1 astrocyte per 100-400 synapses
  - **Count**: 50-200 cells

- **Fibrous Astrocytes** (`ASTROCYTE_TYPE_FIBROUS`)
  - **Function**: Structural support, blood-brain barrier
  - **Count**: 20-100 cells

### Computational Architecture
```
Input (640×480 pixels)
    ↓
[Convolution Layers]
    ↓
[V1 Simple Cells] → [Gabor Filtering]
    ↓
[V1 Complex Cells] → [Phase Invariance]
    ↓
[Pooling Layer] → [Spatial Downsampling]
    ↓
Output (96 features)
```

### Biological Correspondence
- **Cortical Layer 4C**: Simple cells (LGN input)
- **Cortical Layer 2/3**: Complex cells, orientation columns
- **Horizontal Connections**: Long-range lateral connections for contour integration
- **Feedback from V2**: Top-down modulation (not yet implemented)

---

## Audio Cortex (A1)

### Primary Function
Spectral decomposition, frequency analysis, temporal pattern recognition

### Cellular Composition

#### Neurons
- **Frequency-Tuned Neurons** (`NEURON_A1_FREQUENCY_TUNED` = 200)
  - **Function**: Tonotopic frequency selectivity
  - **Parameters**:
    - `center_frequency` (Hz): Best frequency (20-20,000 Hz)
    - `bandwidth` (octaves): Tuning width (0.1-1.0)
    - `q_factor`: Sharpness of tuning
  - **Biological Basis**: Cochlear-like filtering (von Békésy, 1960)
  - **Count**: 40-128 neurons (logarithmically spaced 20Hz-20kHz)

- **Coincidence Detector Neurons** (`NEURON_A1_COINCIDENCE_DETECTOR` = 201)
  - **Function**: Binaural timing cues (interaural time difference)
  - **Parameters**:
    - `integration_window` (ms): Temporal precision (0.01-1ms)
    - `threshold`: Spike threshold
  - **Biological Basis**: Jeffress model of sound localization
  - **Count**: 32-64 neurons (spatial hearing)

- **Onset Detector Neurons** (`NEURON_AUDITORY_ONSET` = 202)
  - **Function**: Transient detection, attack detection
  - **Parameters**:
    - `adaptation_rate`: Fast adaptation (τ = 5-20ms)
    - `threshold`: Onset sensitivity
  - **Count**: 16-32 neurons (temporal envelope tracking)

- **Pyramidal Neurons Layer 5** (`NEURON_PYRAMIDAL_L5_THICK` = 107)
  - **Function**: Output to subcortical targets, long-range projections
  - **Count**: 100-300 neurons

#### Synapses
- **Excitatory Synapses** (`SYNAPSE_TYPE_EXCITATORY`)
  - **Properties**: Fast glutamatergic, AMPA receptors
  - **Count**: 3,000-15,000

- **Inhibitory Synapses** (`SYNAPSE_TYPE_INHIBITORY`)
  - **Properties**: GABAergic, surround inhibition
  - **Count**: 500-3,000

- **Short-Term Plastic Synapses** (`SYNAPSE_TYPE_FACILITATING`)
  - **Function**: Echo suppression, streaming segregation
  - **Properties**: Facilitation (U = 0.1-0.3)
  - **Count**: 500-2,000

#### Glial Cells
- **Protoplasmic Astrocytes** (`ASTROCYTE_TYPE_PROTOPLASMIC`)
  - **Count**: 30-150 cells

### Computational Architecture
```
Input (Audio samples, 16kHz)
    ↓
[Hamming Window] → [FFT]
    ↓
[Mel Filterbank] → [40 filters]
    ↓
[Frequency-Tuned Neurons] → [Tonotopic map]
    ↓
[MFCC Computation] → [13 coefficients]
    ↓
[Onset Detection] → [Temporal edges]
    ↓
Output (64 features: MFCCs + temporal)
```

### Biological Correspondence
- **Cochlea**: Basilar membrane frequency decomposition
- **A1 Core**: Tonotopic frequency maps
- **A1 Belt**: Spectrotemporal modulation processing
- **Planum Temporale**: Higher-order spectral integration

---

## Speech Cortex (STG/Wernicke)

### Primary Function
Phoneme recognition, formant analysis, lexical access, prosody extraction

### Cellular Composition

#### Proposed Specialized Neurons (Phase 8.8 - Roadmap)

- **Phoneme Detector Neurons** (`NEURON_STG_PHONEME_DETECTOR` = 400) [PROPOSED]
  - **Function**: Categorical phoneme recognition
  - **Parameters**:
    - `preferred_phoneme`: One of 44 IPA phonemes (IY, AA, P, S, etc.)
    - `tuning_width`: Tolerance to speaker variation
    - `temporal_window`: Integration time (20-50ms)
    - `voicing_sensitivity`: Voiced vs. unvoiced discrimination
    - `formant_weights[4]`: F1-F4 weighting
  - **Biological Basis**: Chang et al. (2010) - Phoneme-selective neurons in STG
  - **Count**: 44 neurons (1 per phoneme) × 3-5 instances = 132-220 neurons

- **Pitch Contour Neurons** (`NEURON_STG_PITCH_CONTOUR` = 401) [PROPOSED]
  - **Function**: Prosodic pitch tracking
  - **Parameters**:
    - `preferred_contour`: Rising, falling, flat, etc.
    - `pitch_range` (Hz): Sensitive range (80-400 Hz)
  - **Count**: 16-32 neurons (contour types)

- **Prosody Neurons** (`NEURON_STG_PROSODY` = 402) [PROPOSED]
  - **Function**: Stress, rhythm, intonation detection
  - **Parameters**:
    - `stress_sensitivity`: Emphasis detection
    - `rhythm_window` (ms): Temporal integration
  - **Count**: 16-32 neurons

- **Lexical Access Neurons** (`NEURON_WERNICKE_LEXICAL` = 403) [PROPOSED]
  - **Function**: Word recognition from phoneme sequences
  - **Parameters**:
    - `word_template`: Stored phoneme sequence
    - `tolerance`: Edit distance tolerance
  - **Count**: 100-10,000 neurons (vocabulary size)

#### Current Implementation (Phase 8.8)
The speech cortex is **currently implemented as a signal processing pipeline** rather than a spiking neural network. It uses:

**Processing Stages**:
1. **Formant Extraction**: Linear Predictive Coding (LPC) to find F1-F4
2. **Phoneme Classification**: Template matching in formant space
3. **Prosody Analysis**: Pitch tracking via autocorrelation
4. **Lexical Access**: Phoneme sequence matching against lexicon

**Internal Data Structures** (not neurons):
- FFT buffers (complex floats)
- LPC coefficient arrays
- Mel filterbank (triangular filters)
- Phonological buffer (circular queue, 7±2 items)
- Lexicon hash table (word → phoneme mappings)

**Future Enhancement**: Will be replaced with spiking neural implementation using proposed neuron types above.

#### Synapses (When Neural Implementation Added)
- **Excitatory Synapses** for phoneme → word mapping
- **Plastic Synapses** for vocabulary learning
- **Modulatory Synapses** for attention-based selection

### Computational Architecture (Current)
```
Input (Audio samples from A1)
    ↓
[LPC Analysis] → [Formant Extraction F1-F4]
    ↓
[Vowel Classification] → [Formant space distance]
    ↓
[Consonant Classification] → [Spectral features]
    ↓
[Phoneme Sequence] → [Temporal buffer]
    ↓
[Lexical Access] → [Word recognition]
    ↓
[Prosody Extraction] → [Pitch + stress]
    ↓
Output (64 features: phonemes + prosody)
```

### Biological Correspondence
- **Superior Temporal Gyrus (STG)**: Phoneme recognition
- **Wernicke's Area (BA 22)**: Word comprehension
- **Planum Temporale**: Phonological processing
- **Angular Gyrus (BA 39)**: Grapheme-phoneme mapping (reading)

---

## Cognitive Modules

### Introspection Module

#### Function
Meta-cognitive monitoring, uncertainty estimation, pattern tracking

#### Cellular Composition

**Metacognitive Neurons** (`NEURON_METACOGNITIVE` = 300)
- **Function**: Monitor neural network activity, confidence estimation
- **Parameters**:
  - `monitored_population`: Which neurons to observe
  - `uncertainty_window`: Integration time
- **Count**: 20-100 neurons (observing different brain regions)

**Executive Control Neurons** (`NEURON_EXECUTIVE_CONTROL` = 301)
- **Function**: Control flow, task switching
- **Parameters**:
  - `control_signal`: Inhibition/facilitation
  - `priority`: Task importance
- **Count**: 10-50 neurons

**Internal Data Structures**:
- Pattern registry (hash table of learned patterns)
- Activity history queue (circular buffer, 100 snapshots)
- Network topology cache (adjacency lists)
- Uncertainty ensemble (5 perturbations for variance estimation)

**Synapses**:
- **Observational Synapses**: Read-only connections to monitor activity
- **Control Synapses**: Modulatory connections to regulate processing

### Ethics Module

#### Function
Constraint satisfaction, value alignment, harm prevention

#### Cellular Composition

**Proposed Specialized Neurons** (Roadmap):

- **Mirror Neurons** (`NEURON_ETHICS_MIRROR` = 500) [PROPOSED]
  - **Function**: Simulate consequences, empathy modeling
  - **Parameters**:
    - `action_template`: Observed/executed action
    - `resonance_strength`: Empathy intensity
  - **Biological Basis**: Rizzolatti (2004) - Action observation/execution matching
  - **Count**: 50-200 neurons

- **Theory of Mind Neurons** (`NEURON_ETHICS_TOM` = 501) [PROPOSED]
  - **Function**: Model mental states of others
  - **Parameters**:
    - `belief_state`: Belief attribution
    - `desire_state`: Goal attribution
  - **Count**: 20-100 neurons

- **Moral Value Neurons** (`NEURON_ETHICS_VALUE` = 502) [PROPOSED]
  - **Function**: Encode deontological rules (hard constraints)
  - **Parameters**:
    - `value_type`: Harm, fairness, autonomy, etc.
    - `priority`: Value weight
  - **Count**: 10-50 neurons (one per ethical principle)

**Current Implementation**:
- Rule-based constraint checker (not neural)
- Golden Rule hard-wired (cannot be trained away)
- Statistical evaluator for multi-dimensional ethical inputs

### Salience Module

#### Function
Novelty detection, surprise computation, urgency estimation

#### Cellular Composition

**Proposed Specialized Neurons** (Roadmap):

- **Surprise Neurons** (`NEURON_SALIENCE_SURPRISE` = 550) [PROPOSED]
  - **Function**: Prediction error detection
  - **Parameters**:
    - `prediction_history`: Recent predictions
    - `error_threshold`: Surprise sensitivity
  - **Count**: 20-50 neurons

- **Novelty Neurons** (`NEURON_SALIENCE_NOVELTY` = 551) [PROPOSED]
  - **Function**: New pattern detection
  - **Parameters**:
    - `memory_window`: Comparison timeframe
    - `similarity_threshold`: Novelty cutoff
  - **Count**: 20-50 neurons

- **Urgency Neurons** (`NEURON_SALIENCE_URGENCY` = 552) [PROPOSED]
  - **Function**: Temporal deadline detection
  - **Parameters**:
    - `deadline_sensitivity`: Time pressure response
  - **Count**: 10-30 neurons

- **Attention Control Neurons** (`NEURON_SALIENCE_ATTENTION` = 553) [PROPOSED]
  - **Function**: Modulate processing resources
  - **Parameters**:
    - `gain_factor`: Attention boost multiplier
  - **Count**: 10-30 neurons

**Current Implementation**:
- Statistical novelty detector (Euclidean distance in feature space)
- Temporal salience evaluator (prediction error + urgency)
- History buffer for baseline comparison

### Curiosity Module

#### Function
Knowledge gap detection, exploration drive, learning prioritization

#### Cellular Composition

**Proposed Specialized Neurons** (Roadmap):

- **Epistemic Curiosity Neurons** (`NEURON_CURIOSITY_EPISTEMIC` = 600) [PROPOSED]
  - **Function**: "What if?" exploration
  - **Parameters**:
    - `uncertainty_target`: Preferred uncertainty level
    - `exploration_bonus`: Reward for learning
  - **Count**: 20-50 neurons

- **Knowledge Gap Neurons** (`NEURON_CURIOSITY_GAP` = 601) [PROPOSED]
  - **Function**: Detect missing information
  - **Parameters**:
    - `completeness_threshold`: Knowledge sufficiency
  - **Count**: 10-30 neurons

**Current Implementation**:
- Integrates with salience for novelty-driven exploration
- Statistical uncertainty estimation
- No dedicated neural substrate yet

### Symbolic Logic Engine

#### Function
Logical reasoning, constraint propagation, neuro-symbolic integration

#### Current Implementation (Phase 8.8)

**Architecture Type**: Symbolic (not yet neural)

The symbolic logic engine is currently a **traditional first-order logic system** implemented as data structures and algorithms, NOT as spiking neurons. It operates in parallel with the neural substrate via bridge functions.

**Components**:
- **Knowledge Base**: Facts + Rules (stored symbolically)
- **Inference Engine**: Forward/backward chaining, resolution theorem proving
- **Unification**: Variable substitution (Robinson's algorithm)
- **Working Memory**: Active clauses during inference

**Integration Method**: Bridge functions connect neural and symbolic systems:
```
Neural Network Activity
    ↓ (encode as logical facts)
Symbolic Logic Engine
    ↓ (query KB, perform inference)
Logical Verification / Constraints
    ↓ (modulate neural activity)
Neural Network Outputs
```

**Bridge Functions**:
- `symbolic_logic_compute_novelty()`: Neural patterns → Symbolic novelty scores
- `symbolic_logic_get_salient_facts()`: Neural attention → Symbolic fact retrieval
- `symbolic_logic_explore()`: Symbolic gaps → Neural curiosity
- `symbolic_logic_consolidate_memory()`: Combined neural/symbolic memory

**See detailed documentation**: [docs/NEURO_SYMBOLIC_INTEGRATION.md](NEURO_SYMBOLIC_INTEGRATION.md)

#### Proposed Neural Implementation (Phase 9 Roadmap)

**Vision**: Replace symbolic engine with spiking neural circuits that perform logical operations

**Proposed Specialized Neurons**:

- **Logical Operator Neurons** (`NEURON_LOGIC_AND/OR/NOT` = 650-652) [PROPOSED]
  - **Function**: Implement Boolean operations as neural circuits
  - **Parameters**:
    - `operator_type`: AND, OR, NOT, XOR, IMPLIES, IFF
    - `threshold`: Activation threshold (e.g., AND requires both inputs)
  - **Biological Basis**: Coincidence detector neurons (requires simultaneous inputs)
  - **Count**: 50-100 neurons (logic gates)
  - **Benefits**: Fast (0.1ms vs. seconds for symbolic), differentiable, energy-efficient

- **Variable Binding Neurons** (`NEURON_LOGIC_VARIABLE` = 653) [PROPOSED]
  - **Function**: Bind symbolic variables to neural activation patterns
  - **Parameters**:
    - `symbol_id`: Unique variable identifier
    - `binding_strength`: Confidence [0,1]
  - **Biological Basis**: Pointer neurons (Eliasmith, 2013)
  - **Count**: 100-500 neurons (variable pool)

**Example Neural Logic Circuit**:
```
[Neuron A] ──┐
             ├──→ [AND Gate Neuron] → Output: A ∧ B
[Neuron B] ──┘
             threshold = 2.0
             weights = [1.0, 1.0]
```

**Current Status**:
- Phase 8.8: Traditional symbolic engine (works, but slow for complex reasoning)
- Phase 9: Migrate to neural logic gates for speed and integration

---

## Network Architecture

### Connectivity Patterns

#### Fractal Topology (Phase 8.5+)
When `enable_fractal_topology = true`, networks are generated with:

**Scale-Free Properties**:
- **Degree Distribution**: P(k) ∝ k^γ, where γ = -2.1 (cortical value)
- **Hub Neurons**: 15% of neurons are highly connected (degree > 20)
- **Small-World**: High clustering coefficient + short path length

**Implementation**:
- Preferential attachment algorithm
- Pink noise (1/f) weight initialization
- Biologically realistic connectivity

**Benefits**:
- 70-80% fewer synapses than dense networks
- Efficient information routing via hubs
- Matches cortical connectivity patterns

#### Dense Topology (Legacy)
When `enable_fractal_topology = false`:
- Random connectivity with fixed probability
- More synapses, less efficient
- Used for compatibility

### Integration Architecture (4-Way Multimodal)

```
┌─────────────────────────────────────────────────────┐
│              MULTIMODAL INTEGRATION                 │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────┐ │
│  │ Visual   │  │ Audio    │  │ Speech   │  │Dir.│ │
│  │ (96 dim) │  │ (64 dim) │  │ (64 dim) │  │(32)│ │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └─┬──┘ │
│       │             │              │           │    │
│       └──────────┬──┴──────────────┴───────────┘    │
│                  │                                   │
│           [Attention Layer]                          │
│              ↓   ↓   ↓   ↓                           │
│           α_vis α_aud α_spe α_dir                    │
│              ↓                                       │
│       [Weighted Fusion: Σ α_i × f_i]                │
│              ↓                                       │
│       [Neural Network: 256 inputs]                  │
│              ↓                                       │
│       [Cognitive Modules]                           │
│       Introspection | Ethics | Salience | Curiosity │
└─────────────────────────────────────────────────────┘
```

**Attention Mechanism**:
- Softmax over modality-specific salience scores
- Learned attention weights (plastic synapses)
- Normalized to sum = 1.0

---

## Cellular Interactions

### Neuron-Synapse-Glia Triad

#### Typical Interaction Pattern
```
[Pre-synaptic Neuron]
    ↓ [Action Potential]
[Synapse]
    ↓ [Neurotransmitter Release]
    ↓ [Astrocyte Detection]
[Astrocyte]
    ↓ [Gliotransmitter Release]
    ↓ [Modulatory Feedback]
[Post-synaptic Neuron]
    ↓ [EPSP/IPSP]
```

#### Neuromodulation
- **Dopamine**: Reward prediction error (from VTA)
- **Serotonin**: Patience, mood regulation
- **Acetylcholine**: Attention, learning rate modulation
- **Norepinephrine**: Arousal, vigilance

**Pink Noise Modulation** (Phase 8.5):
- 1/f noise spectrum (biologically realistic)
- Modulates neuromodulator levels across timescales
- Prevents runaway dynamics, improves learning stability

### Learning Mechanisms

#### Spike-Timing-Dependent Plasticity (STDP)
- **Pre before Post**: Potentiation (Δw > 0)
- **Post before Pre**: Depression (Δw < 0)
- **Time Window**: ±20ms typical

#### Eligibility Traces (Phase 8.7)
- **Function**: Credit assignment over longer timescales
- **Mechanism**: Synaptic "tag" decays exponentially (τ = 100-1000ms)
- **Benefit**: Solve temporal credit assignment problem

#### Homeostatic Plasticity
- **Function**: Prevent runaway excitation/inhibition
- **Mechanism**: Target firing rate regulation
- **Timescale**: Hours to days (slow)

---

## Summary: What You Get When Instantiating

### Visual Cortex Creation
```c
brain_t brain = brain_create_custom(&config);
// config.enable_visual_cortex = true;
```

**You Get**:
- 200-500 specialized visual neurons (simple/complex/orientation/direction)
- 5,000-20,000 synapses (excitatory, inhibitory, plastic)
- 50-200 astrocytes for neuromodulation
- Gabor filter kernels for edge detection
- Pooling layers for spatial invariance
- **Output**: 96-dimensional visual feature vector

### Audio Cortex Creation
```c
// config.enable_audio_cortex = true;
```

**You Get**:
- 100-300 frequency-tuned neurons (tonotopic map)
- 32-64 coincidence detectors (spatial hearing)
- 16-32 onset detectors (temporal edges)
- 3,000-15,000 synapses
- 30-150 astrocytes
- FFT/MFCC processing pipeline
- **Output**: 64-dimensional audio feature vector

### Speech Cortex Creation
```c
// config.enable_speech_cortex = true;
```

**You Get** (Currently signal processing, future neural):
- Formant extraction (F1-F4)
- 44 phoneme recognition (IPA inventory)
- Prosody analysis (pitch, stress)
- Lexical access (word recognition)
- Phonological working memory (7±2 items)
- **Output**: 64-dimensional speech feature vector

### Cognitive Modules Creation
```c
// config.enable_introspection = true;
// config.enable_ethics = true;
// config.enable_salience = true;
// config.enable_curiosity = true;
```

**You Get**:
- 20-100 metacognitive neurons (introspection)
- 10-50 executive control neurons
- Pattern tracking registry
- Uncertainty estimation ensemble
- Ethical constraint checker (hard-wired Golden Rule)
- Novelty/surprise/urgency detectors
- Exploration drive system
- **Outputs**: Confidence, salience, novelty scores

---

## Implementation Priorities

### Phase 8.8 (Current) ✅
- Visual cortex: Fully neural
- Audio cortex: Fully neural
- Speech cortex: Signal processing (works, but not neural)
- Cognitive modules: Mixed (some neural, some statistical)

### Phase 9 (Roadmap) 🚧
- **High Priority**: Implement speech cortex with spiking neurons (400-499 range)
- **High Priority**: Add mirror neurons to ethics (500-549)
- **Medium Priority**: Add salience/attention neurons (550-599)
- **Medium Priority**: Add curiosity neurons (600-649)
- **Low Priority**: Add symbolic logic neurons (650-699)
- **Low Priority**: Add memory consolidation neurons (700-749)

---

## References

1. Hubel & Wiesel (1962) "Receptive fields, binocular interaction and functional architecture in the cat's visual cortex"
2. von Békésy (1960) "Experiments in Hearing" - Nobel Prize for cochlear mechanics
3. Chang et al. (2010) "Categorical speech representation in human superior temporal gyrus"
4. Rizzolatti (2004) "The mirror-neuron system" - Mirror neurons discovery
5. Baddeley & Hitch (1974) "Working memory" - Phonological loop model
6. Miller (1956) "The magical number seven, plus or minus two" - Working memory capacity
7. Jeffress (1948) "A place theory of sound localization" - Coincidence detection
8. Fleming & Dolan (2012) "The neural basis of metacognitive ability" - Metacognition
9. Barabási & Albert (1999) "Emergence of scaling in random networks" - Scale-free topology
10. Peterson & Barney (1952) "Control methods for vowel formant measurement" - Formant analysis

---

**🧠 NIMCP 2.7.0 Phase 8.8 - Complete cellular architecture documentation**
